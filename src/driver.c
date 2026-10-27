#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ast.h"
#include "codegen.h"
#include "grammar.h"
#include "lexer.h"
#include "parser.h"
#include "semanticAnalyzer.h"
#include "symbolTable.h"

#define GRAMMAR_FILE "grammar.txt"
#define BUFFER_SIZE 1024

typedef struct
{
    const char *source_path;
    const char *assembly_path;
    Grammar grammar;
    int parse_tree_nodes;
    int ast_nodes;
    AstNode *program;
    SymbolTables tables;
} Compiler;

static void load_grammar_file(Compiler *c)
{
    if (!load_grammar(&c->grammar, GRAMMAR_FILE))
        exit(1);
}

static ParseNode *parse_source(Compiler *c, int *errors)
{
    Lexer lexer;
    if (!open_lexer(&lexer, c->source_path, BUFFER_SIZE))
    {
        printf("Error: cannot read the source file '%s'\n", c->source_path);
        exit(1);
    }
    ParseNode *tree = parse_input_source_code(&lexer, &c->grammar, errors, &c->parse_tree_nodes);
    close_lexer(&lexer);
    return tree;
}

static bool build_program(Compiler *c)
{
    int errors;
    ParseNode *tree = parse_source(c, &errors);
    if (errors > 0)
    {
        free_parse_tree(tree);
        return false;
    }
    c->program = build_ast(tree, &c->ast_nodes);
    return true;
}

static void show_tokens(Compiler *c)
{
    if (!print_token_list(c->source_path, BUFFER_SIZE))
    {
        printf("Error: cannot read the source file '%s'\n", c->source_path);
        exit(1);
    }
}

static void show_parse_tree(Compiler *c)
{
    load_grammar_file(c);
    int errors;
    ParseNode *tree = parse_source(c, &errors);
    if (errors == 0)
    {
        printf("Input source code is syntactically correct...........\n");
        print_parse_tree(&c->grammar, tree, stdout);
    }
    free_parse_tree(tree);
}

static void show_ast(Compiler *c)
{
    load_grammar_file(c);
    if (!build_program(c))
        return;
    print_ast(c->program);
    free_ast(c->program);
}

static void show_memory(Compiler *c)
{
    load_grammar_file(c);
    if (!build_program(c))
        return;
    size_t parse_tree_bytes = c->parse_tree_nodes * sizeof(ParseNode) + (c->parse_tree_nodes - 1) * sizeof(ParseNode *);
    size_t ast_bytes = c->ast_nodes * sizeof(AstNode);
    double percentage = ((double)parse_tree_bytes - (double)ast_bytes) / (double)parse_tree_bytes * 100.0;
    printf("Parse tree Number of nodes = %d, Allocated Memory = %zu Bytes\n", c->parse_tree_nodes, parse_tree_bytes);
    printf("AST Number of nodes = %d, Allocated Memory = %zu Bytes\n", c->ast_nodes, ast_bytes);
    printf("Compression percentage = %.2f%%\n", percentage);
    free_ast(c->program);
}

static void show_tables(Compiler *c, int option)
{
    load_grammar_file(c);
    if (!build_program(c))
        return;
    analyse_program(&c->tables, c->program);
    if (option == 5)
        print_symbol_table(&c->tables);
    else if (option == 6)
        print_activation_records(&c->tables);
    else
        print_array_variables(&c->tables);
    free_symbol_tables(&c->tables);
    free_ast(c->program);
}

static void compile(Compiler *c)
{
    load_grammar_file(c);
    clock_t start_time = clock();
    bool built = build_program(c);
    bool correct = built && analyse_program(&c->tables, c->program) == 0;
    clock_t end_time = clock();
    if (correct)
        printf("Code compiles successfully..........\n");
    double total_CPU_time = (double)(end_time - start_time);
    printf("total_CPU_time = %.0f\n", total_CPU_time);
    printf("total_CPU_time_in_seconds = %f\n", total_CPU_time / CLOCKS_PER_SEC);
    if (built)
    {
        free_symbol_tables(&c->tables);
        free_ast(c->program);
    }
}

static void generate(Compiler *c)
{
    load_grammar_file(c);
    bool built = build_program(c);
    bool correct = built && analyse_program(&c->tables, c->program) == 0;
    if (correct)
    {
        FILE *file = fopen(c->assembly_path, "w");
        if (file == NULL)
        {
            printf("Error: cannot write the assembly file '%s'\n", c->assembly_path);
            exit(1);
        }
        generate_code(&c->tables, file);
        fclose(file);
        printf("Assembly code written to %s\n", c->assembly_path);
    }
    if (built)
    {
        free_symbol_tables(&c->tables);
        free_ast(c->program);
    }
}

static void run_option(int option, const char *source_path, const char *assembly_path)
{
    Compiler c = {.source_path = source_path, .assembly_path = assembly_path};
    switch (option)
    {
    case 1:
        show_tokens(&c);
        break;
    case 2:
        show_parse_tree(&c);
        break;
    case 3:
        show_ast(&c);
        break;
    case 4:
        show_memory(&c);
        break;
    case 5:
    case 6:
    case 7:
        show_tables(&c, option);
        break;
    case 8:
        compile(&c);
        break;
    case 9:
        generate(&c);
        break;
    }
}

static void print_menu(void)
{
    printf("\n0. Exit\n");
    printf("1. Lexer: print the token list\n");
    printf("2. Parser: verify the syntax and print the parse tree\n");
    printf("3. AST: print the abstract syntax tree\n");
    printf("4. Memory: parse tree and AST sizes and the compression percentage\n");
    printf("5. Symbol table\n");
    printf("6. Activation record sizes\n");
    printf("7. Static and dynamic arrays\n");
    printf("8. Errors and total compiling time\n");
    printf("9. Code generation\n");
    printf("Enter an option (0-9): ");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: ./compiler <testcase file> <assembly file>\n");
        return 1;
    }
    printf("LEVEL 4: Symbol table / AST / Type checking / Semantic rules modules work; static and dynamic arrays "
           "are handled in type checking and code generation\n");

    int option;
    for (;;)
    {
        print_menu();
        if (scanf("%d", &option) != 1 || option == 0)
            return 0;
        if (option < 1 || option > 9)
            printf("Invalid option: enter a number from 0 to 9\n");
        else
            run_option(option, argv[1], argv[2]);
    }
}
