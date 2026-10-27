#ifndef GRAMMAR_H
#define GRAMMAR_H

#include "parserDef.h"

bool load_grammar(Grammar *grammar, const char *path);

bool is_nonterminal(int symbol);
int find_nonterminal(const Grammar *grammar, const char *name);
const char *symbol_name(const Grammar *grammar, int symbol);

#endif
