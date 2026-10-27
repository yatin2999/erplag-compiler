#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "symbolTableDef.h"

bool check_declared(const AstNode *variable, int *errors);
void check_assignment(AstNode *statement, int *errors);
void check_element_assignment(AstNode *statement, int *errors);
void check_print(AstNode *operand, int *errors);
void check_arguments(AstNode *call, const Definition *callee, int *errors);
void check_result_types(const AstNode *call, const Definition *callee, int *errors);
ValueType check_switch_selector(const AstNode *node, int *errors);
bool check_case_value(const AstNode *node, ValueType selector, int *errors);
void check_while_condition(AstNode *node, int *errors);

ValueType expression_type(const AstNode *expression);
const char *type_name(ValueType type);

#endif
