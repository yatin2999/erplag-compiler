#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "symbolTableDef.h"

void collect_modules(SymbolTables *tables, AstNode *program);
Scope *new_scope(SymbolTables *tables, Scope *parent, int first_line, int last_line);
void check_parameters(const Definition *definition, int *errors);
void declare_variables(Definition *definition, AstNode *declaration, Scope *scope, int *errors);
void declare_loop_variable(Definition *definition, AstNode *variable, Scope *scope, int *errors);
void bind(AstNode *expression, const Scope *scope);
void free_symbol_tables(SymbolTables *tables);

Module *find_module(const SymbolTables *tables, const char *name);
bool static_bounds(const Type *type, long long *low, long long *high);

void print_symbol_table(const SymbolTables *tables);
void print_activation_records(const SymbolTables *tables);
void print_array_variables(const SymbolTables *tables);

#endif
