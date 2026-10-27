#ifndef SYMBOL_TABLE_DEF_H
#define SYMBOL_TABLE_DEF_H

#include <stdbool.h>

#include "astDef.h"

enum
{
    SCOPE_TABLE_SIZE = 31,
    MODULE_TABLE_SIZE = 101,
    FRAME_LIMIT = 0x7FFFFFF0,
    MAX_STATIC_ELEMENTS = (FRAME_LIMIT - 24) / 8
};

typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Definition Definition;
typedef struct Module Module;

typedef struct
{
    bool negative;
    const Token *token;
    Symbol *variable;
} Bound;

typedef struct
{
    TokenKind base;
    bool array;
    bool dynamic;
    Bound low, high;
} Type;

typedef enum
{
    LOCAL_VARIABLE,
    INPUT_PARAMETER,
    OUTPUT_PARAMETER,
    LOOP_VARIABLE
} SymbolKind;

struct Symbol
{
    const Token *name;
    SymbolKind kind;
    Type type;
    Scope *scope;
    long long width, offset;
    int assignment;
    long long frame_offset;
    Symbol *next_in_scope;
    Symbol *next_in_module;
};

struct Scope
{
    Scope *parent;
    int level;
    int first_line, last_line;
    Symbol *table[SCOPE_TABLE_SIZE];
    Scope *next;
};

struct Definition
{
    AstNode *node;
    const char *name;
    Module *module;
    bool repeated;
    bool visited;
    Scope *parameters;
    Symbol **inputs;
    int input_count;
    Symbol **outputs;
    int output_count;
    Symbol *first_symbol, *last_symbol;
    long long size;
    long long frame_size;
    long long parameter_block_size;
};

struct Module
{
    const char *name;
    const AstNode *declaration;
    Definition *definition;
    bool needed;
    Module *next_in_table;
};

typedef struct
{
    Definition *definitions;
    int definition_count;
    Module *modules[MODULE_TABLE_SIZE];
    Scope *scopes;
} SymbolTables;

#endif
