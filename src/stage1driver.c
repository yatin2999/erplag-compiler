#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "grammar.h"
#include "lexer.h"
#include "parser.h"

#define GRAMMAR_FILE "grammar.txt"

static const char *source_path;
static const char *tree_path;
static int buffer_size;
static Grammar grammar;

static void load_grammar_file(void)
{
    if (!load_grammar(&grammar, GRAMMAR_FILE))
        exit(1);
}

static ParseNode *parse_source(int *errors)
{
    Lexer lexer;
    if (!open_lexer(&lexer, source_path, buffer_size))
    {
        printf("Error: cannot read the source file '%s'\n", source_path);
        exit(1);
    }
    int node_count;
    ParseNode *tree = parse_input_source_code(&lexer, &grammar, errors, &node_count);
    close_lexer(&lexer);
    return tree;
}

static void parse_and_print_tree(void)
{
    load_grammar_file();
    int errors;
    ParseNode *tree = parse_source(&errors);
    if (errors == 0)
    {
        printf("Input source code is syntactically correct...........\n");
        FILE *file = fopen(tree_path, "w");
        if (file == NULL)
        {
            printf("Error: cannot write the parse tree file '%s'\n", tree_path);
            exit(1);
        }
        print_parse_tree(&grammar, tree, file);
        fclose(file);
        printf("Parse tree written to %s\n", tree_path);
    }
    free_parse_tree(tree);
}

static void measure_time(void)
{
    load_grammar_file();
    int errors;
    clock_t start_time = clock();
    ParseNode *tree = parse_source(&errors);
    clock_t end_time = clock();
    double total_CPU_time = (double)(end_time - start_time);
    printf("total_CPU_time = %.0f\n", total_CPU_time);
    printf("total_CPU_time_in_seconds = %f\n", total_CPU_time / CLOCKS_PER_SEC);
    free_parse_tree(tree);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: ./stage1exe <testcase file> <parse tree output file> <size of buffer>\n");
        return 1;
    }
    source_path = argv[1];
    tree_path = argv[2];
    buffer_size = atoi(argv[3]);
    if (buffer_size < 2)
    {
        printf("The size of buffer must be at least 2\n");
        return 1;
    }

    printf("Implementation status:\n");
    printf("(a) FIRST and FOLLOW sets automated\n");
    printf("(c) Both lexical and syntax analysis modules implemented\n");
    printf("Parse tree constructed; all lexical and syntax errors are reported, with error recovery\n");

    int option;
    for (;;)
    {
        printf("\n0. Exit\n");
        printf("1. Remove comments\n");
        printf("2. Print the token list\n");
        printf("3. Parse and write the parse tree to %s\n", tree_path);
        printf("4. Total CPU time of the lexer and parser\n");
        printf("Enter an option (0-4): ");
        fflush(stdout);
        if (scanf("%d", &option) != 1 || option == 0)
            return 0;
        switch (option)
        {
        case 1:
            if (!remove_comments(source_path))
            {
                printf("Error: cannot read the source file '%s'\n", source_path);
                return 1;
            }
            break;
        case 2:
            if (!print_token_list(source_path, buffer_size))
            {
                printf("Error: cannot read the source file '%s'\n", source_path);
                return 1;
            }
            break;
        case 3:
            parse_and_print_tree();
            break;
        case 4:
            measure_time();
            break;
        default:
            printf("Invalid option: enter a number from 0 to 4\n");
            break;
        }
    }
}
