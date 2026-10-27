#include <stdio.h>

#include "lexer.h"
#include "symbolTable.h"
#include "symbolTableDef.h"

static ValueType value_type(TokenKind base)
{
    if (base == T_INTEGER)
        return TYPE_INTEGER;
    if (base == T_REAL)
        return TYPE_REAL;
    return TYPE_BOOLEAN;
}

const char *type_name(ValueType type)
{
    static const char *const names[] = {"no type", "integer", "real", "boolean"};
    return names[type];
}

static const char *name_of(const AstNode *variable)
{
    return variable->variable.name.lexeme;
}

bool check_declared(const AstNode *variable, int *errors)
{
    if (variable->variable.symbol != NULL)
        return true;
    printf("Line %d: semantic error: variable '%s' is not declared here\n", variable->line, name_of(variable));
    (*errors)++;
    return false;
}

static bool constant_index(const AstNode *index, long long *value)
{
    bool negative = false;
    for (; index->kind == AST_UNARY; index = index->unary.operand)
        if (index->unary.op == T_MINUS)
            negative = !negative;
    if (index->kind != AST_NUM)
        return false;
    *value = negative ? -index->literal.value.num : index->literal.value.num;
    return true;
}

static ValueType check_variable(const AstNode *variable, int *errors)
{
    const Symbol *symbol = variable->variable.symbol;
    if (!check_declared(variable, errors))
        return TYPE_NONE;
    if (symbol->type.array)
    {
        printf("Line %d: type error: the array '%s' cannot be an operand\n", variable->line, name_of(variable));
        (*errors)++;
        return TYPE_NONE;
    }
    return value_type(symbol->type.base);
}

static ValueType check_unary(AstNode *node, ValueType operand, int *errors)
{
    if (operand == TYPE_BOOLEAN)
    {
        printf("Line %d: type error: '%s' cannot be applied to a boolean\n", node->line, token_text(node->unary.op));
        (*errors)++;
        operand = TYPE_NONE;
    }
    node->unary.type = operand;
    return operand;
}

static ValueType check_binary(AstNode *node, ValueType left, ValueType right, int *errors)
{
    if (left == TYPE_NONE || right == TYPE_NONE)
        return TYPE_NONE;
    bool numeric = left != TYPE_BOOLEAN && right != TYPE_BOOLEAN;
    ValueType type = TYPE_NONE;
    switch (node->binary.op)
    {
    case T_PLUS:
    case T_MINUS:
    case T_MUL:
        if (numeric && left == right)
            type = left;
        break;
    case T_DIV:
        if (numeric)
            type = TYPE_REAL;
        break;
    case T_AND:
    case T_OR:
        if (left == TYPE_BOOLEAN && right == TYPE_BOOLEAN)
            type = TYPE_BOOLEAN;
        break;
    default:
        if (numeric && left == right)
            type = TYPE_BOOLEAN;
        break;
    }
    if (type == TYPE_NONE)
    {
        printf("Line %d: type error: '%s' cannot be applied to %s and %s\n", node->binary.op_line,
               token_text(node->binary.op), type_name(left), type_name(right));
        (*errors)++;
    }
    node->binary.type = type;
    return type;
}

static ValueType check_element(AstNode *node, ValueType index, int *errors)
{
    const AstNode *array = node->element.array;
    const Symbol *symbol = array->variable.symbol;
    if (!check_declared(array, errors))
        return TYPE_NONE;
    if (!symbol->type.array)
    {
        printf("Line %d: type error: '%s' is indexed, but it is not an array\n", array->line, name_of(array));
        (*errors)++;
        return TYPE_NONE;
    }
    if (index == TYPE_NONE)
        return TYPE_NONE;
    if (index != TYPE_INTEGER)
    {
        printf("Line %d: type error: the index of '%s' is %s, not integer\n", array->line, name_of(array),
               type_name(index));
        (*errors)++;
        return TYPE_NONE;
    }
    long long value, low, high;
    if (constant_index(node->element.index, &value) && static_bounds(&symbol->type, &low, &high) &&
        (value < low || value > high))
    {
        printf("Line %d: type error: the index %lld is outside the bounds %lld..%lld of '%s'\n", array->line, value,
               low, high, name_of(array));
        (*errors)++;
    }
    return value_type(symbol->type.base);
}

static ValueType check_expression(AstNode *node, int *errors)
{
    switch (node->kind)
    {
    case AST_BINARY:
    {
        ValueType left = check_expression(node->binary.left, errors);
        ValueType right = check_expression(node->binary.right, errors);
        return check_binary(node, left, right, errors);
    }
    case AST_UNARY:
        return check_unary(node, check_expression(node->unary.operand, errors), errors);
    case AST_ELEMENT:
        return check_element(node, check_expression(node->element.index, errors), errors);
    case AST_VARIABLE:
        return check_variable(node, errors);
    case AST_NUM:
        return TYPE_INTEGER;
    case AST_RNUM:
        return TYPE_REAL;
    default:
        return TYPE_BOOLEAN;
    }
}

static bool equivalent(const Type *x, const Type *y)
{
    long long x_low, x_high, y_low, y_high;
    if (x->base != y->base)
        return false;
    if (!static_bounds(x, &x_low, &x_high) || !static_bounds(y, &y_low, &y_high))
        return true;
    return (unsigned long long)x_high - (unsigned long long)x_low ==
           (unsigned long long)y_high - (unsigned long long)y_low;
}

void check_assignment(AstNode *node, int *errors)
{
    AstNode *target = node->assign.target, *value = node->assign.value;
    const Symbol *symbol = target->variable.symbol;
    check_declared(target, errors);
    const Symbol *source = value->kind == AST_VARIABLE ? value->variable.symbol : NULL;
    if (source != NULL && source->type.array)
    {
        if (symbol != NULL && !symbol->type.array)
            printf("Line %d: type error: the array '%s' cannot be an operand\n", value->line, name_of(value));
        else if (symbol != NULL && !equivalent(&symbol->type, &source->type))
            printf("Line %d: type error: the arrays '%s' and '%s' are not structurally equivalent\n", node->line,
                   name_of(target), name_of(value));
        else
            return;
        (*errors)++;
        return;
    }
    ValueType type = check_expression(value, errors);
    if (symbol == NULL || type == TYPE_NONE)
        return;
    if (symbol->type.array)
        printf("Line %d: type error: the array '%s' cannot be assigned a value of type %s\n", node->line,
               name_of(target), type_name(type));
    else if (type != value_type(symbol->type.base))
        printf("Line %d: type error: '%s' is %s, but the assigned value is %s\n", node->line, name_of(target),
               type_name(value_type(symbol->type.base)), type_name(type));
    else
        return;
    (*errors)++;
}

void check_element_assignment(AstNode *node, int *errors)
{
    AstNode *element = node->assign.target;
    ValueType target = check_expression(element, errors);
    ValueType value = check_expression(node->assign.value, errors);
    if (target != TYPE_NONE && value != TYPE_NONE && value != target)
    {
        printf("Line %d: type error: the elements of '%s' are %s, but the assigned value is %s\n", node->line,
               name_of(element->element.array), type_name(target), type_name(value));
        (*errors)++;
    }
}

void check_print(AstNode *operand, int *errors)
{
    if (operand->kind == AST_VARIABLE)
        check_declared(operand, errors);
    else
        check_expression(operand, errors);
}

static bool argument_matches(AstNode *argument, const Symbol *formal, int *errors)
{
    AstNode *value = argument->kind == AST_UNARY ? argument->unary.operand : argument;
    ValueType type = TYPE_NONE;
    const Symbol *array = NULL;
    if (value->kind != AST_VARIABLE)
        type = check_expression(value, errors);
    else if (!check_declared(value, errors))
        return true;
    else if (value->variable.symbol->type.array)
        array = value->variable.symbol;
    else
        type = value_type(value->variable.symbol->type.base);

    if (argument->kind == AST_UNARY)
    {
        if (array != NULL || type == TYPE_BOOLEAN)
        {
            printf("Line %d: type error: a sign cannot be applied to %s\n", argument->line,
                   array != NULL ? "an array" : "a boolean");
            (*errors)++;
            return true;
        }
        argument->unary.type = type;
    }
    if (formal == NULL || (type == TYPE_NONE && array == NULL))
        return true;
    if (formal->type.array)
        return array != NULL && equivalent(&formal->type, &array->type);
    return array == NULL && type == value_type(formal->type.base);
}

void check_arguments(AstNode *call, const Definition *callee, int *errors)
{
    int i = 0;
    for (AstNode *argument = call->call.arguments; argument != NULL; argument = argument->next, i++)
    {
        const Symbol *formal = callee != NULL ? callee->inputs[i] : NULL;
        if (!argument_matches(argument, formal, errors))
        {
            printf("Line %d: type error: actual parameter %d does not match the input parameter '%s' of module "
                   "'%s'\n",
                   call->line, i + 1, formal->name->lexeme, callee->name);
            (*errors)++;
        }
    }
}

void check_result_types(const AstNode *call, const Definition *callee, int *errors)
{
    int i = 0;
    for (const AstNode *result = call->call.results; result != NULL; result = result->next, i++)
    {
        const Symbol *symbol = result->variable.symbol;
        const Symbol *output = callee->outputs[i];
        if (symbol != NULL && (symbol->type.array || symbol->type.base != output->type.base))
        {
            printf("Line %d: type error: result variable '%s' does not match the output parameter '%s' of module "
                   "'%s'\n",
                   call->line, name_of(result), output->name->lexeme, callee->name);
            (*errors)++;
        }
    }
}

ValueType check_switch_selector(const AstNode *node, int *errors)
{
    const AstNode *selector = node->switch_stmt.selector;
    const Symbol *symbol = selector->variable.symbol;
    if (!check_declared(selector, errors))
        return TYPE_NONE;
    if (!symbol->type.array && symbol->type.base != T_REAL)
        return value_type(symbol->type.base);
    printf("Line %d: type error: the switch selector '%s' is %s; it must be integer or boolean\n", node->line,
           name_of(selector), symbol->type.array ? "an array" : "real");
    (*errors)++;
    return TYPE_NONE;
}

bool check_case_value(const AstNode *node, ValueType selector, int *errors)
{
    const Token *value = &node->case_stmt.value->literal;
    if (selector == TYPE_INTEGER && value->kind != T_NUM)
        printf("Line %d: type error: the case value %s is not an integer, as the selector is\n", node->line,
               value->lexeme);
    else if (selector == TYPE_BOOLEAN && value->kind == T_NUM)
        printf("Line %d: type error: the case value %s is not a boolean, as the selector is\n", node->line,
               value->lexeme);
    else
        return true;
    (*errors)++;
    return false;
}

void check_while_condition(AstNode *node, int *errors)
{
    ValueType type = check_expression(node->while_stmt.condition, errors);
    if (type != TYPE_NONE && type != TYPE_BOOLEAN)
    {
        printf("Line %d: type error: the condition of a while loop must be boolean, not %s\n", node->line,
               type_name(type));
        (*errors)++;
    }
}

ValueType expression_type(const AstNode *expression)
{
    switch (expression->kind)
    {
    case AST_BINARY:
        return expression->binary.type;
    case AST_UNARY:
        return expression->unary.type;
    case AST_VARIABLE:
        return value_type(expression->variable.symbol->type.base);
    case AST_ELEMENT:
        return value_type(expression->element.array->variable.symbol->type.base);
    case AST_NUM:
        return TYPE_INTEGER;
    case AST_RNUM:
        return TYPE_REAL;
    default:
        return TYPE_BOOLEAN;
    }
}
