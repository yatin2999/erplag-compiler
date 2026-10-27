#ifndef PARSER_DEF_H
#define PARSER_DEF_H

#include <stdbool.h>
#include <stdint.h>

#include "lexerDef.h"

enum
{
    TERMINAL_COUNT = T_EOF + 1,
    NT_BASE = T_EOF + 1,
    MAX_NONTERMINALS = 100,
    MAX_NAME_LENGTH = 40,
    MAX_PRODUCTIONS = 200,
    MAX_RHS_LENGTH = 16
};

typedef struct
{
    int lhs;
    int rhs[MAX_RHS_LENGTH];
    int length;
} Production;

typedef struct
{
    int nonterminal_count;
    char nonterminal_names[MAX_NONTERMINALS][MAX_NAME_LENGTH];
    int production_count;
    Production productions[MAX_PRODUCTIONS + 1];
    bool nullable[MAX_NONTERMINALS];
    uint64_t first[MAX_NONTERMINALS];
    uint64_t follow[MAX_NONTERMINALS];
    int table[MAX_NONTERMINALS][TERMINAL_COUNT];
} Grammar;

_Static_assert(TERMINAL_COUNT <= 64, "FIRST/FOLLOW bitmasks require at most 64 terminals");

typedef struct ParseNode ParseNode;
struct ParseNode
{
    int symbol;
    int production;
    Token token;
    ParseNode **children;
    int child_count;
};

typedef struct StackItem StackItem;
struct StackItem
{
    int symbol;
    ParseNode *node;
    StackItem *below;
};

typedef struct
{
    const Grammar *grammar;
    Lexer *lexer;
    Token token;
    StackItem *top;
    int statements;
    bool recovering;
    int errors;
    int node_count;
} Parser;

#endif
