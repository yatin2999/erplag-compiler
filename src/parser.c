#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "lexer.h"
#include "parserDef.h"

static void next_token(Parser *p)
{
    p->token = get_next_token(p->lexer);
    while (p->token.kind == T_LEXERROR)
    {
        printf("Line %d: lexical error: '%s' %s\n", p->token.line, p->token.lexeme, p->token.message);
        p->errors++;
        p->token = get_next_token(p->lexer);
    }
}

static void push(Parser *p, int symbol, ParseNode *node)
{
    StackItem *item = malloc(sizeof *item);
    item->symbol = symbol;
    item->node = node;
    item->below = p->top;
    p->top = item;
}

static void pop(Parser *p)
{
    StackItem *item = p->top;
    p->top = item->below;
    free(item);
}

static ParseNode *new_node(Parser *p, int symbol)
{
    ParseNode *node = calloc(1, sizeof *node);
    node->symbol = symbol;
    p->node_count++;
    return node;
}

static void expand(Parser *p, ParseNode *node, int production)
{
    const Production *rule = &p->grammar->productions[production];
    pop(p);
    node->production = production;
    node->child_count = rule->length;
    if (rule->length > 0)
        node->children = malloc(rule->length * sizeof *node->children);
    for (int i = 0; i < rule->length; i++)
        node->children[i] = new_node(p, rule->rhs[i]);
    for (int i = rule->length - 1; i >= 0; i--)
        push(p, rule->rhs[i], node->children[i]);
}

static void match(Parser *p)
{
    Token *token = &p->top->node->token;
    *token = p->token;
    token->lexeme = malloc(strlen(p->token.lexeme) + 1);
    strcpy(token->lexeme, p->token.lexeme);
    pop(p);
    p->recovering = false;
    next_token(p);
}

static void syntax_error(Parser *p, int top)
{
    if (p->recovering)
        return;
    p->recovering = true;
    p->errors++;
    printf("Line %d: syntax error: ", p->token.line);
    if (p->token.kind == T_EOF)
        printf("unexpected end of input");
    else
        printf("unexpected '%s'", p->token.lexeme);
    if (is_nonterminal(top))
        printf(" while parsing %s\n", symbol_name(p->grammar, top));
    else if (token_text(top) != NULL)
        printf(", expected '%s'\n", token_text(top));
    else
        printf(", expected %s\n", token_name(top));
}

static void recover_terminal(Parser *p, int expected)
{
    syntax_error(p, expected);
    pop(p);
}

static bool in_statement_list(const Parser *p)
{
    for (const StackItem *item = p->top; item != NULL; item = item->below)
        if (item->symbol == p->statements)
            return true;
    return false;
}

static void recover_nonterminal(Parser *p, int nonterminal)
{
    uint64_t first = p->grammar->first[nonterminal - NT_BASE];
    uint64_t follow = p->grammar->follow[nonterminal - NT_BASE];
    syntax_error(p, nonterminal);
    for (;;)
    {
        TokenKind kind = p->token.kind;
        if (kind == T_EOF || ((first | follow) >> kind & 1))
            break;
        if ((kind == T_SEMICOL || kind == T_END) && in_statement_list(p))
            break;
        next_token(p);
    }

    TokenKind kind = p->token.kind;
    if (first >> kind & 1)
    {
        expand(p, p->top->node, p->grammar->table[nonterminal - NT_BASE][kind]);
    }
    else if (kind == T_EOF || (follow >> kind & 1))
    {
        pop(p);
    }
    else
    {
        while (p->top->symbol != p->statements)
            pop(p);
        if (kind == T_SEMICOL)
            next_token(p);
    }
}

ParseNode *parse_input_source_code(Lexer *lexer, const Grammar *grammar, int *errors, int *node_count)
{
    Parser p = {.grammar = grammar, .lexer = lexer};
    p.statements = find_nonterminal(grammar, "<statements>");
    ParseNode *root = new_node(&p, NT_BASE);
    push(&p, T_EOF, NULL);
    push(&p, NT_BASE, root);
    next_token(&p);

    while (p.top->symbol != T_EOF)
    {
        int symbol = p.top->symbol;
        TokenKind kind = p.token.kind;
        if (!is_nonterminal(symbol))
        {
            if (symbol == (int)kind)
                match(&p);
            else
                recover_terminal(&p, symbol);
        }
        else if (grammar->table[symbol - NT_BASE][kind] != 0)
        {
            expand(&p, p.top->node, grammar->table[symbol - NT_BASE][kind]);
        }
        else
        {
            recover_nonterminal(&p, symbol);
        }
    }
    pop(&p);

    if (p.token.kind != T_EOF)
        syntax_error(&p, T_EOF);
    while (p.token.kind != T_EOF)
        next_token(&p);

    *errors = p.errors;
    *node_count = p.node_count;
    return root;
}

static void print_row(const Grammar *g, const ParseNode *node, const char *parent, FILE *out)
{
    if (is_nonterminal(node->symbol))
    {
        fprintf(out, "%-20s %-6s %-13s %-20s %-21s %-4s %s\n", "----", "----", "----", "----", parent,
                node->child_count == 0 ? "yes" : "no", symbol_name(g, node->symbol));
        return;
    }
    const Token *t = &node->token;
    fprintf(out, "%-20s %-6d %-13s ", t->lexeme, t->line, token_name(t->kind));
    if (t->kind == T_NUM)
        fprintf(out, "%-20lld ", t->value.num);
    else if (t->kind == T_RNUM)
        fprintf(out, "%-20.4f ", t->value.rnum);
    else
        fprintf(out, "%-20s ", "----");
    fprintf(out, "%-21s %-4s %s\n", parent, "yes", "----");
}

static void print_subtree(const Grammar *g, const ParseNode *node, const char *parent, FILE *out)
{
    while (node != NULL)
    {
        if (node->child_count == 0)
        {
            print_row(g, node, parent, out);
            return;
        }
        const char *name = symbol_name(g, node->symbol);
        print_subtree(g, node->children[0], name, out);
        print_row(g, node, parent, out);
        for (int i = 1; i < node->child_count - 1; i++)
            print_subtree(g, node->children[i], name, out);
        parent = name;
        node = node->child_count > 1 ? node->children[node->child_count - 1] : NULL;
    }
}

void print_parse_tree(const Grammar *g, const ParseNode *root, FILE *out)
{
    print_subtree(g, root, "ROOT", out);
}

void free_parse_tree(ParseNode *node)
{
    while (node != NULL)
    {
        ParseNode *last = NULL;
        for (int i = 0; i < node->child_count; i++)
        {
            if (i == node->child_count - 1)
                last = node->children[i];
            else
                free_parse_tree(node->children[i]);
        }
        free(node->children);
        free(node->token.lexeme);
        free(node);
        node = last;
    }
}
