#include <stdio.h>

#include "lexer.h"
#include "symbolTable.h"
#include "symbolTableDef.h"
#include "typechecker.h"

typedef struct
{
    SymbolTables *tables;
    Definition *definition;
    int assignments;
    int errors;
} Analyzer;

static bool reaches(SymbolTables *tables, Definition *from, const Definition *to);

static bool calls_reach(SymbolTables *tables, AstNode *statements, const Definition *to)
{
    for (AstNode *s = statements; s != NULL; s = s->next)
    {
        switch (s->kind)
        {
        case AST_CALL:
        {
            const Module *module = find_module(tables, s->call.module.lexeme);
            if (module != NULL && module->definition != NULL && reaches(tables, module->definition, to))
                return true;
            break;
        }
        case AST_SWITCH:
            for (AstNode *c = s->switch_stmt.cases; c != NULL; c = c->next)
                if (calls_reach(tables, c->case_stmt.body, to))
                    return true;
            if (s->switch_stmt.default_case != NULL &&
                calls_reach(tables, s->switch_stmt.default_case->case_stmt.body, to))
                return true;
            break;
        case AST_FOR:
            if (calls_reach(tables, s->for_stmt.body, to))
                return true;
            break;
        case AST_WHILE:
            if (calls_reach(tables, s->while_stmt.body, to))
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

static bool reaches(SymbolTables *tables, Definition *from, const Definition *to)
{
    if (from == to)
        return true;
    if (from->visited)
        return false;
    from->visited = true;
    return calls_reach(tables, from->node->module.body, to);
}

static bool is_recursive(SymbolTables *tables, Definition *callee, const Definition *caller)
{
    for (int i = 0; i < tables->definition_count; i++)
        tables->definitions[i].visited = false;
    return reaches(tables, callee, caller);
}

static void assigned(Analyzer *a, const AstNode *statement, Symbol *symbol)
{
    if (symbol == NULL)
        return;
    a->assignments++;
    symbol->assignment = a->assignments;
    if (symbol->kind == LOOP_VARIABLE)
    {
        printf("Line %d: semantic error: the loop variable '%s' cannot be assigned a value\n", statement->line,
               symbol->name->lexeme);
        a->errors++;
    }
}

static void check_call(Analyzer *a, AstNode *node, const Scope *scope)
{
    const char *name = node->call.module.lexeme;
    Module *module = find_module(a->tables, name);
    Definition *callee = module != NULL ? module->definition : NULL;
    bool declared = module != NULL && module->declaration != NULL;
    bool before_definition = callee != NULL && callee > a->definition;
    node->call.callee = callee;
    if (before_definition)
        module->needed = true;
    if (callee == NULL && !declared)
    {
        printf("Line %d: semantic error: module '%s' is neither defined nor declared\n", node->line, name);
        a->errors++;
    }
    else if (before_definition && !declared)
    {
        printf("Line %d: semantic error: module '%s' is called before its definition at line %d and is not declared\n",
               node->line, name, callee->node->line);
        a->errors++;
    }
    if (callee != NULL && is_recursive(a->tables, callee, a->definition))
    {
        if (callee == a->definition)
            printf("Line %d: semantic error: module '%s' calls itself\n", node->line, name);
        else
            printf("Line %d: semantic error: this call to module '%s' is part of a cycle of calls\n", node->line, name);
        a->errors++;
    }

    int result_count = 0;
    for (AstNode *result = node->call.results; result != NULL; result = result->next)
    {
        bind(result, scope);
        check_declared(result, &a->errors);
        assigned(a, node, result->variable.symbol);
        result_count++;
    }
    int argument_count = 0;
    for (AstNode *argument = node->call.arguments; argument != NULL; argument = argument->next)
    {
        bind(argument, scope);
        argument_count++;
    }
    bool counts_match = callee != NULL && argument_count == callee->input_count;
    check_arguments(node, counts_match ? callee : NULL, &a->errors);
    if (callee == NULL)
        return;
    if (!counts_match)
    {
        printf("Line %d: semantic error: module '%s': expected input count %d, received %d\n", node->line, name,
               callee->input_count, argument_count);
        a->errors++;
    }
    if (callee->output_count == 0 && result_count > 0)
        printf("Line %d: semantic error: module '%s' returns no values, so the call cannot have a result list\n",
               node->line, name);
    else if (callee->output_count > 0 && result_count == 0)
        printf("Line %d: semantic error: module '%s' has output parameters, so the call needs a result list\n",
               node->line, name);
    else if (result_count != callee->output_count)
        printf("Line %d: semantic error: module '%s': expected result count %d, received %d\n", node->line, name,
               callee->output_count, result_count);
    else
    {
        check_result_types(node, callee, &a->errors);
        return;
    }
    a->errors++;
}

static void check_statements(Analyzer *a, AstNode *statements, Scope *scope);

static void check_duplicate_case(Analyzer *a, const AstNode *node, const AstNode *cases, ValueType selector,
                                 bool *seen_true, bool *seen_false)
{
    const Token *value = &node->case_stmt.value->literal;
    if (selector == TYPE_INTEGER)
    {
        for (const AstNode *earlier = cases; earlier != node; earlier = earlier->next)
        {
            const Token *other = &earlier->case_stmt.value->literal;
            if (other->kind == T_NUM && other->value.num == value->value.num)
            {
                printf("Line %d: semantic error: the case value %lld appears twice\n", node->line, value->value.num);
                a->errors++;
                return;
            }
        }
        return;
    }
    bool *seen = value->kind == T_TRUE ? seen_true : seen_false;
    if (*seen)
    {
        printf("Line %d: semantic error: the case value %s appears twice\n", node->line, value->lexeme);
        a->errors++;
    }
    *seen = true;
}

static void check_switch(Analyzer *a, AstNode *node, Scope *scope)
{
    bind(node->switch_stmt.selector, scope);
    ValueType selector = check_switch_selector(node, &a->errors);
    if (selector == TYPE_NONE)
        return;

    bool seen_true = false, seen_false = false;
    for (AstNode *c = node->switch_stmt.cases; c != NULL; c = c->next)
    {
        if (check_case_value(c, selector, &a->errors))
            check_duplicate_case(a, c, node->switch_stmt.cases, selector, &seen_true, &seen_false);
        check_statements(a, c->case_stmt.body, new_scope(a->tables, scope, c->line, c->case_stmt.break_line));
    }
    AstNode *other = node->switch_stmt.default_case;
    if (other != NULL)
    {
        if (selector == TYPE_BOOLEAN)
        {
            printf("Line %d: semantic error: a switch on a boolean value cannot have a default\n", other->line);
            a->errors++;
        }
        check_statements(a, other->case_stmt.body,
                         new_scope(a->tables, scope, other->line, other->case_stmt.break_line));
    }
    int end = node->switch_stmt.end_line;
    if (selector == TYPE_INTEGER && other == NULL)
    {
        printf("Line %d: semantic error: the switch at lines %d-%d is on an integer value and needs a default\n", end,
               node->line, end);
        a->errors++;
    }
    else if (selector == TYPE_BOOLEAN && !(seen_true && seen_false))
    {
        printf("Line %d: semantic error: the switch at lines %d-%d is on a boolean value and needs both a true and a "
               "false case\n",
               end, node->line, end);
        a->errors++;
    }
}

static bool all_declared(const AstNode *node)
{
    switch (node->kind)
    {
    case AST_VARIABLE:
        return node->variable.symbol != NULL;
    case AST_BINARY:
        return all_declared(node->binary.left) && all_declared(node->binary.right);
    case AST_UNARY:
        return all_declared(node->unary.operand);
    case AST_ELEMENT:
        return all_declared(node->element.array) && all_declared(node->element.index);
    default:
        return true;
    }
}

static bool assigned_since(const AstNode *node, int assignment)
{
    switch (node->kind)
    {
    case AST_VARIABLE:
        return node->variable.symbol->assignment > assignment;
    case AST_BINARY:
        return assigned_since(node->binary.left, assignment) || assigned_since(node->binary.right, assignment);
    case AST_UNARY:
        return assigned_since(node->unary.operand, assignment);
    case AST_ELEMENT:
        return assigned_since(node->element.array, assignment) || assigned_since(node->element.index, assignment);
    default:
        return false;
    }
}

static void check_while(Analyzer *a, AstNode *node, Scope *scope)
{
    AstNode *condition = node->while_stmt.condition;
    bind(condition, scope);
    check_while_condition(node, &a->errors);
    int before = a->assignments;
    int end = node->while_stmt.end_line;
    check_statements(a, node->while_stmt.body, new_scope(a->tables, scope, node->line, end));
    if (all_declared(condition) && !assigned_since(condition, before))
    {
        printf("Line %d: semantic error: no variable of the condition of the while loop at lines %d-%d is assigned in "
               "its body\n",
               end, node->line, end);
        a->errors++;
    }
}

static void check_statements(Analyzer *a, AstNode *statements, Scope *scope)
{
    for (AstNode *s = statements; s != NULL; s = s->next)
    {
        switch (s->kind)
        {
        case AST_DECLARE:
            declare_variables(a->definition, s, scope, &a->errors);
            break;
        case AST_ASSIGN:
            bind(s->assign.target, scope);
            bind(s->assign.value, scope);
            assigned(a, s, s->assign.target->variable.symbol);
            check_assignment(s, &a->errors);
            break;
        case AST_ASSIGN_ELEMENT:
            bind(s->assign.target, scope);
            bind(s->assign.value, scope);
            assigned(a, s, s->assign.target->element.array->variable.symbol);
            check_element_assignment(s, &a->errors);
            break;
        case AST_GET_VALUE:
            bind(s->get_value.target, scope);
            check_declared(s->get_value.target, &a->errors);
            assigned(a, s, s->get_value.target->variable.symbol);
            break;
        case AST_PRINT:
            bind(s->print.operand, scope);
            check_print(s->print.operand, &a->errors);
            break;
        case AST_CALL:
            check_call(a, s, scope);
            break;
        case AST_SWITCH:
            check_switch(a, s, scope);
            break;
        case AST_FOR:
        {
            Scope *loop = new_scope(a->tables, scope, s->line, s->for_stmt.end_line);
            declare_loop_variable(a->definition, s->for_stmt.variable, loop, &a->errors);
            check_statements(a, s->for_stmt.body, loop);
            break;
        }
        case AST_WHILE:
            check_while(a, s, scope);
            break;
        default:
            break;
        }
    }
}

static void analyse_definition(Analyzer *a, Definition *d)
{
    const AstNode *module = d->node;
    a->definition = d;
    if (d->repeated)
    {
        printf("Line %d: semantic error: module '%s' is already defined, at line %d\n", module->line, d->name,
               d->module->definition->node->line);
        a->errors++;
    }
    else if (d->module != NULL && d->module->declaration != NULL && !d->module->needed)
    {
        printf("Line %d: semantic error: the declaration of module '%s' at line %d is redundant: no call before this "
               "definition needs it\n",
               module->line, d->name, d->module->declaration->line);
        a->errors++;
    }
    check_parameters(d, &a->errors);
    check_statements(a, module->module.body, new_scope(a->tables, d->parameters, module->line, module->module.end_line));
    for (int i = 0; i < d->output_count; i++)
    {
        if (d->outputs[i]->assignment == 0)
        {
            printf("Line %d: semantic error: output parameter '%s' is never assigned a value in module '%s' at lines "
                   "%d-%d\n",
                   module->module.end_line, d->outputs[i]->name->lexeme, d->name, module->line,
                   module->module.end_line);
            a->errors++;
        }
    }
}

int analyse_program(SymbolTables *tables, AstNode *program)
{
    Analyzer a = {.tables = tables};
    collect_modules(tables, program);
    for (const AstNode *declaration = program->program.declarations; declaration != NULL; declaration = declaration->next)
    {
        const Module *module = find_module(tables, declaration->module_decl.name.lexeme);
        if (module->declaration != declaration)
            printf("Line %d: semantic error: module '%s' is already declared, at line %d\n", declaration->line,
                   module->name, module->declaration->line);
        else if (module->definition == NULL)
            printf("Line %d: semantic error: module '%s' is declared but never defined\n", declaration->line,
                   module->name);
        else
            continue;
        a.errors++;
    }

    for (int i = 0; i < tables->definition_count; i++)
        analyse_definition(&a, &tables->definitions[i]);
    return a.errors;
}
