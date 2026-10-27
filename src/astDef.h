#ifndef AST_DEF_H
#define AST_DEF_H

#include "lexerDef.h"

struct Symbol;
struct Definition;

typedef enum
{
    TYPE_NONE,
    TYPE_INTEGER,
    TYPE_REAL,
    TYPE_BOOLEAN
} ValueType;

typedef enum
{
    AST_PROGRAM,
    AST_MODULE_DECL,
    AST_MODULE,
    AST_PARAMETER,
    AST_PRIMITIVE,
    AST_ARRAY_TYPE,
    AST_DECLARE,
    AST_ASSIGN,
    AST_ASSIGN_ELEMENT,
    AST_GET_VALUE,
    AST_PRINT,
    AST_CALL,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_FOR,
    AST_WHILE,
    AST_BINARY,
    AST_UNARY,
    AST_ELEMENT,
    AST_VARIABLE,
    AST_NUM,
    AST_RNUM,
    AST_BOOL
} AstKind;

typedef struct AstNode AstNode;
struct AstNode
{
    AstKind kind;
    int line;
    AstNode *next;
    union
    {
        struct
        {
            AstNode *declarations, *modules;
        } program;
        struct
        {
            Token name;
        } module_decl;
        struct
        {
            Token name;
            AstNode *inputs, *outputs, *body;
            int end_line;
        } module;
        struct
        {
            Token name;
            AstNode *type;
            struct Symbol *symbol;
        } parameter;
        struct
        {
            TokenKind type;
        } primitive;
        struct
        {
            AstNode *low, *high, *element;
        } array_type;
        struct
        {
            AstNode *names, *type;
        } declare;
        struct
        {
            AstNode *target, *value;
        } assign;
        struct
        {
            AstNode *target;
        } get_value;
        struct
        {
            AstNode *operand;
        } print;
        struct
        {
            AstNode *results;
            Token module;
            AstNode *arguments;
            struct Definition *callee;
        } call;
        struct
        {
            AstNode *selector, *cases, *default_case;
            int end_line;
        } switch_stmt;
        struct
        {
            AstNode *value, *body;
            int break_line;
        } case_stmt;
        struct
        {
            AstNode *variable, *low, *high, *body;
            int end_line;
        } for_stmt;
        struct
        {
            AstNode *condition, *body;
            int end_line;
        } while_stmt;
        struct
        {
            TokenKind op;
            int op_line;
            AstNode *left, *right;
            ValueType type;
        } binary;
        struct
        {
            TokenKind op;
            ValueType type;
            AstNode *operand;
        } unary;
        struct
        {
            AstNode *array, *index;
        } element;
        struct
        {
            Token name;
            struct Symbol *symbol;
        } variable;
        Token literal;
    };
};

#endif
