#ifndef AST_H
#define AST_H

#include "astDef.h"
#include "parserDef.h"

AstNode *build_ast(ParseNode *root, int *node_count);
void print_ast(const AstNode *root);
void free_ast(AstNode *node);

#endif
