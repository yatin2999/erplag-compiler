#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#include "parserDef.h"

ParseNode *parse_input_source_code(Lexer *lexer, const Grammar *grammar, int *errors, int *node_count);
void print_parse_tree(const Grammar *grammar, const ParseNode *node, FILE *out);
void free_parse_tree(ParseNode *node);

#endif
