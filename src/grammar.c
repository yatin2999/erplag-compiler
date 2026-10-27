#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parserDef.h"

bool is_nonterminal(int symbol)
{
    return symbol >= NT_BASE;
}

int find_nonterminal(const Grammar *g, const char *name)
{
    for (int i = 0; i < g->nonterminal_count; i++)
        if (strcmp(g->nonterminal_names[i], name) == 0)
            return NT_BASE + i;
    return -1;
}

const char *symbol_name(const Grammar *g, int symbol)
{
    if (is_nonterminal(symbol))
        return g->nonterminal_names[symbol - NT_BASE];
    return symbol == T_EOF ? "$" : token_name(symbol);
}

static int nonterminal_number(Grammar *g, const char *name)
{
    int symbol = find_nonterminal(g, name);
    if (symbol >= 0)
        return symbol;
    if (name[0] != '<' || strlen(name) >= MAX_NAME_LENGTH || g->nonterminal_count == MAX_NONTERMINALS)
        return -1;
    strcpy(g->nonterminal_names[g->nonterminal_count], name);
    g->nonterminal_count++;
    return NT_BASE + g->nonterminal_count - 1;
}

static int terminal_number(const char *name)
{
    for (int kind = 0; kind < T_EOF; kind++)
        if (strcmp(token_name(kind), name) == 0)
            return kind;
    return -1;
}

static Production *new_production(Grammar *g, int lhs)
{
    if (g->production_count == MAX_PRODUCTIONS)
        return NULL;
    g->production_count++;
    Production *rule = &g->productions[g->production_count];
    rule->lhs = lhs;
    return rule;
}

static bool read_productions(Grammar *g, FILE *file)
{
    char line[1024];
    for (int number = 1; fgets(line, sizeof line, file) != NULL; number++)
    {
        char *word = strtok(line, " \t\r\n");
        if (word == NULL)
            continue;
        int lhs = nonterminal_number(g, word);
        word = strtok(NULL, " \t\r\n");
        Production *rule = lhs >= 0 && word != NULL && strcmp(word, "->") == 0 ? new_production(g, lhs) : NULL;
        bool good = rule != NULL;
        while (good && (word = strtok(NULL, " \t\r\n")) != NULL)
        {
            if (strcmp(word, "|") == 0)
            {
                rule = new_production(g, lhs);
                good = rule != NULL;
            }
            else if (strcmp(word, "EPSILON") != 0)
            {
                int symbol = word[0] == '<' ? nonterminal_number(g, word) : terminal_number(word);
                good = symbol >= 0 && rule->length < MAX_RHS_LENGTH;
                if (good)
                {
                    rule->rhs[rule->length] = symbol;
                    rule->length++;
                }
            }
        }
        if (!good)
        {
            printf("Error: line %d of the grammar is not of the form <A> -> ..., has an unknown symbol, or exceeds "
                   "the size of the grammar tables\n",
                   number);
            return false;
        }
    }
    return true;
}

static bool first_of_sequence(const Grammar *g, const int *symbols, int length, uint64_t *first)
{
    for (int i = 0; i < length; i++)
    {
        int symbol = symbols[i];
        if (!is_nonterminal(symbol))
        {
            *first |= (uint64_t)1 << symbol;
            return false;
        }
        *first |= g->first[symbol - NT_BASE];
        if (!g->nullable[symbol - NT_BASE])
            return false;
    }
    return true;
}

static void compute_nullable(Grammar *g)
{
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int p = 1; p <= g->production_count; p++)
        {
            const Production *rule = &g->productions[p];
            if (g->nullable[rule->lhs - NT_BASE])
                continue;
            bool all_nullable = true;
            for (int i = 0; i < rule->length; i++)
                if (!is_nonterminal(rule->rhs[i]) || !g->nullable[rule->rhs[i] - NT_BASE])
                    all_nullable = false;
            if (all_nullable)
            {
                g->nullable[rule->lhs - NT_BASE] = true;
                changed = true;
            }
        }
    }
}

static void compute_first(Grammar *g)
{
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int p = 1; p <= g->production_count; p++)
        {
            const Production *rule = &g->productions[p];
            uint64_t *first = &g->first[rule->lhs - NT_BASE];
            uint64_t before = *first;
            first_of_sequence(g, rule->rhs, rule->length, first);
            if (*first != before)
                changed = true;
        }
    }
}

static void compute_follow(Grammar *g)
{
    g->follow[0] = (uint64_t)1 << T_EOF;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int p = 1; p <= g->production_count; p++)
        {
            const Production *rule = &g->productions[p];
            for (int i = 0; i < rule->length; i++)
            {
                int symbol = rule->rhs[i];
                if (!is_nonterminal(symbol))
                    continue;
                uint64_t *follow = &g->follow[symbol - NT_BASE];
                uint64_t before = *follow;
                if (first_of_sequence(g, rule->rhs + i + 1, rule->length - i - 1, follow))
                    *follow |= g->follow[rule->lhs - NT_BASE];
                if (*follow != before)
                    changed = true;
            }
        }
    }
}

static void compute_first_and_follow_sets(Grammar *g)
{
    compute_nullable(g);
    compute_first(g);
    compute_follow(g);
}

static void print_production(const Grammar *g, int p)
{
    const Production *rule = &g->productions[p];
    printf("%s ->", symbol_name(g, rule->lhs));
    if (rule->length == 0)
        printf(" EPSILON");
    for (int i = 0; i < rule->length; i++)
        printf(" %s", symbol_name(g, rule->rhs[i]));
}

static bool create_parse_table(Grammar *g)
{
    bool conflict = false;
    for (int p = 1; p <= g->production_count; p++)
    {
        const Production *rule = &g->productions[p];
        uint64_t predict = 0;
        if (first_of_sequence(g, rule->rhs, rule->length, &predict))
            predict |= g->follow[rule->lhs - NT_BASE];
        for (int t = 0; t < TERMINAL_COUNT; t++)
        {
            if ((predict >> t & 1) == 0)
                continue;
            int *entry = &g->table[rule->lhs - NT_BASE][t];
            if (*entry == 0)
            {
                *entry = p;
            }
            else if (*entry != p)
            {
                conflict = true;
                printf("LL(1) conflict in M[%s, %s]: ", symbol_name(g, rule->lhs), symbol_name(g, t));
                print_production(g, *entry);
                printf(" and ");
                print_production(g, p);
                printf("\n");
            }
        }
    }
    return !conflict;
}

bool load_grammar(Grammar *g, const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("Error: cannot read the grammar file '%s' in the current directory\n", path);
        return false;
    }
    memset(g, 0, sizeof *g);
    bool loaded = read_productions(g, file);
    fclose(file);
    if (!loaded)
        return false;
    compute_first_and_follow_sets(g);
    if (!create_parse_table(g))
    {
        printf("Error: %s is not an LL(1) grammar\n", path);
        return false;
    }
    return true;
}
