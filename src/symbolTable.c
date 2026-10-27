#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "symbolTableDef.h"

static unsigned name_hash(const char *name)
{
    unsigned hash = 5381;
    for (; *name != '\0'; name++)
        hash = 33 * hash + (unsigned char)*name;
    return hash;
}

Module *find_module(const SymbolTables *tables, const char *name)
{
    Module *module = tables->modules[name_hash(name) % MODULE_TABLE_SIZE];
    while (module != NULL && strcmp(module->name, name) != 0)
        module = module->next_in_table;
    return module;
}

static Module *module_entry(SymbolTables *tables, const char *name)
{
    Module *module = find_module(tables, name);
    if (module == NULL)
    {
        unsigned slot = name_hash(name) % MODULE_TABLE_SIZE;
        module = calloc(1, sizeof *module);
        module->name = name;
        module->next_in_table = tables->modules[slot];
        tables->modules[slot] = module;
    }
    return module;
}

Scope *new_scope(SymbolTables *tables, Scope *parent, int first_line, int last_line)
{
    Scope *scope = calloc(1, sizeof *scope);
    scope->parent = parent;
    scope->level = parent != NULL ? parent->level + 1 : 0;
    scope->first_line = first_line;
    scope->last_line = last_line;
    scope->next = tables->scopes;
    tables->scopes = scope;
    return scope;
}

static Symbol *find_in_scope(const Scope *scope, const char *name)
{
    Symbol *symbol = scope->table[name_hash(name) % SCOPE_TABLE_SIZE];
    while (symbol != NULL && strcmp(symbol->name->lexeme, name) != 0)
        symbol = symbol->next_in_scope;
    return symbol;
}

static Symbol *lookup(const Scope *scope, const char *name)
{
    for (; scope != NULL; scope = scope->parent)
    {
        Symbol *symbol = find_in_scope(scope, name);
        if (symbol != NULL)
            return symbol;
    }
    return NULL;
}

void bind(AstNode *n, const Scope *scope)
{
    switch (n->kind)
    {
    case AST_VARIABLE:
        n->variable.symbol = lookup(scope, n->variable.name.lexeme);
        break;
    case AST_BINARY:
        bind(n->binary.left, scope);
        bind(n->binary.right, scope);
        break;
    case AST_UNARY:
        bind(n->unary.operand, scope);
        break;
    case AST_ELEMENT:
        bind(n->element.array, scope);
        bind(n->element.index, scope);
        break;
    default:
        break;
    }
}

static long long bound_value(const Bound *bound)
{
    return bound->negative ? -bound->token->value.num : bound->token->value.num;
}

static Bound make_bound(AstNode *node, const Scope *scope)
{
    Bound bound = {0};
    if (node->kind == AST_UNARY)
    {
        bound.negative = node->unary.op == T_MINUS;
        node = node->unary.operand;
    }
    if (node->kind == AST_NUM)
    {
        bound.token = &node->literal;
    }
    else
    {
        bound.token = &node->variable.name;
        bound.variable = lookup(scope, bound.token->lexeme);
        node->variable.symbol = bound.variable;
    }
    return bound;
}

static Type make_type(AstNode *node, const Scope *scope)
{
    Type type = {0};
    if (node->kind == AST_PRIMITIVE)
    {
        type.base = node->primitive.type;
        return type;
    }
    type.base = node->array_type.element->primitive.type;
    type.array = true;
    type.low = make_bound(node->array_type.low, scope);
    type.high = make_bound(node->array_type.high, scope);
    type.dynamic = type.low.token->kind == T_ID || type.high.token->kind == T_ID;
    return type;
}

bool static_bounds(const Type *type, long long *low, long long *high)
{
    if (!type->array || type->dynamic)
        return false;
    *low = bound_value(&type->low);
    *high = bound_value(&type->high);
    return *low <= *high;
}

static unsigned long long element_count(const Type *type)
{
    long long low, high;
    if (!static_bounds(type, &low, &high))
        return 0;
    return (unsigned long long)high - (unsigned long long)low + 1;
}

static long long width_of(const Type *type, bool parameter)
{
    long long element = 1;
    if (type->base == T_INTEGER)
        element = 2;
    else if (type->base == T_REAL)
        element = 4;
    if (!type->array)
        return element;
    if (parameter)
        return 1 + 2 * 2;
    unsigned long long count = element_count(type);
    if (count > MAX_STATIC_ELEMENTS)
        return 1;
    return 1 + (long long)count * element;
}

static long long frame_bytes(const Type *type)
{
    if (!type->array)
        return 8;
    unsigned long long count = element_count(type);
    if (count > MAX_STATIC_ELEMENTS)
        return FRAME_LIMIT + 1LL;
    return 24 + 8 * (long long)count;
}

static void allocate(Definition *d, Symbol *symbol, int *errors)
{
    bool fitted = d->frame_size <= FRAME_LIMIT;
    d->frame_size += frame_bytes(&symbol->type);
    if (d->frame_size > FRAME_LIMIT)
    {
        d->frame_size = FRAME_LIMIT + 1LL;
        if (fitted)
        {
            printf("Line %d: semantic error: '%s' does not fit in the activation record of module '%s' (at most %d "
                   "bytes)\n",
                   symbol->name->line, symbol->name->lexeme, d->name, FRAME_LIMIT);
            (*errors)++;
        }
    }
    symbol->frame_offset = -d->frame_size;
}

static Symbol *add_entry(Definition *d, Scope *scope, const Token *name, SymbolKind kind, const Type *type)
{
    Symbol *symbol = calloc(1, sizeof *symbol);
    symbol->name = name;
    symbol->kind = kind;
    symbol->type = *type;
    symbol->scope = scope;
    symbol->width = width_of(type, kind == INPUT_PARAMETER || kind == OUTPUT_PARAMETER);
    symbol->offset = d->size;
    d->size += symbol->width;

    unsigned slot = name_hash(name->lexeme) % SCOPE_TABLE_SIZE;
    symbol->next_in_scope = scope->table[slot];
    scope->table[slot] = symbol;

    if (d->first_symbol == NULL)
        d->first_symbol = symbol;
    else
        d->last_symbol->next_in_module = symbol;
    d->last_symbol = symbol;
    return symbol;
}

static void check_static_range(const Type *type, int line, int *errors)
{
    if (type->dynamic || bound_value(&type->low) <= bound_value(&type->high))
        return;
    printf("Line %d: semantic error: the lower bound %s%s of the array exceeds its upper bound %s%s\n", line,
           type->low.negative ? "-" : "", type->low.token->lexeme, type->high.negative ? "-" : "",
           type->high.token->lexeme);
    (*errors)++;
}

static void check_declared_bound(const Bound *bound, int line, int *errors)
{
    if (bound->token->kind != T_ID)
        return;
    if (bound->variable == NULL)
        printf("Line %d: semantic error: the array bound '%s' is not declared\n", line, bound->token->lexeme);
    else if (bound->variable->type.array || bound->variable->type.base != T_INTEGER)
        printf("Line %d: semantic error: the array bound '%s' is not an integer variable\n", line,
               bound->token->lexeme);
    else
        return;
    (*errors)++;
}

static void check_parameter_bound(const Bound *bound, const Token *name, int *errors)
{
    if (bound->token->kind != T_ID)
        return;
    printf("Line %d: semantic error: the bound '%s' of the array parameter '%s' is not an integer number\n",
           name->line, bound->token->lexeme, name->lexeme);
    (*errors)++;
}

static Symbol **declare_list(Definition *d, AstNode *parameters, SymbolKind kind, int *count)
{
    int length = 0;
    for (AstNode *p = parameters; p != NULL; p = p->next)
        length++;
    Symbol **symbols = malloc(length * sizeof *symbols);
    *count = 0;
    for (AstNode *p = parameters; p != NULL; p = p->next)
    {
        if (find_in_scope(d->parameters, p->parameter.name.lexeme) != NULL)
            continue;
        Type type = make_type(p->parameter.type, d->parameters);
        p->parameter.symbol = add_entry(d, d->parameters, &p->parameter.name, kind, &type);
        symbols[*count] = p->parameter.symbol;
        (*count)++;
    }
    return symbols;
}

static void declare_parameters(SymbolTables *tables, Definition *d)
{
    AstNode *module = d->node;
    d->parameters = new_scope(tables, NULL, module->line, module->module.end_line);
    d->inputs = declare_list(d, module->module.inputs, INPUT_PARAMETER, &d->input_count);
    d->outputs = declare_list(d, module->module.outputs, OUTPUT_PARAMETER, &d->output_count);
}

static void check_parameter_list(const Definition *d, const AstNode *parameters, int *errors)
{
    for (const AstNode *p = parameters; p != NULL; p = p->next)
    {
        const Token *name = &p->parameter.name;
        const Symbol *symbol = p->parameter.symbol;
        if (symbol == NULL)
        {
            printf("Line %d: semantic error: parameter '%s' is already declared, at line %d\n", name->line,
                   name->lexeme, find_in_scope(d->parameters, name->lexeme)->name->line);
            (*errors)++;
        }
        else if (symbol->type.array)
        {
            check_parameter_bound(&symbol->type.low, name, errors);
            check_parameter_bound(&symbol->type.high, name, errors);
            check_static_range(&symbol->type, name->line, errors);
        }
    }
}

void check_parameters(const Definition *d, int *errors)
{
    check_parameter_list(d, d->node->module.inputs, errors);
    check_parameter_list(d, d->node->module.outputs, errors);
}

void declare_variables(Definition *d, AstNode *declaration, Scope *scope, int *errors)
{
    Type type = make_type(declaration->declare.type, scope);
    bool accepted = false;
    for (AstNode *name = declaration->declare.names; name != NULL; name = name->next)
    {
        const Token *token = &name->variable.name;
        const Symbol *parameter = find_in_scope(d->parameters, token->lexeme);
        const Symbol *earlier = find_in_scope(scope, token->lexeme);
        bool output = parameter != NULL && parameter->kind == OUTPUT_PARAMETER;
        if (!output && earlier == NULL)
        {
            name->variable.symbol = add_entry(d, scope, token, LOCAL_VARIABLE, &type);
            allocate(d, name->variable.symbol, errors);
            accepted = true;
            continue;
        }
        if (output)
            printf("Line %d: semantic error: '%s' is an output parameter, which cannot be redeclared\n", token->line,
                   token->lexeme);
        else if (earlier->kind == LOOP_VARIABLE)
            printf("Line %d: semantic error: '%s' is the variable of this for loop and cannot be declared in its body\n",
                   token->line, token->lexeme);
        else
            printf("Line %d: semantic error: '%s' is already declared in this scope, at line %d\n", token->line,
                   token->lexeme, earlier->name->line);
        (*errors)++;
    }
    if (type.array && accepted)
    {
        check_declared_bound(&type.low, declaration->line, errors);
        check_declared_bound(&type.high, declaration->line, errors);
        check_static_range(&type, declaration->line, errors);
    }
}

void declare_loop_variable(Definition *d, AstNode *variable, Scope *scope, int *errors)
{
    Type integer = {.base = T_INTEGER};
    variable->variable.symbol = add_entry(d, scope, &variable->variable.name, LOOP_VARIABLE, &integer);
    allocate(d, variable->variable.symbol, errors);
}

void collect_modules(SymbolTables *tables, AstNode *program)
{
    memset(tables, 0, sizeof *tables);
    for (const AstNode *declaration = program->program.declarations; declaration != NULL; declaration = declaration->next)
    {
        Module *module = module_entry(tables, declaration->module_decl.name.lexeme);
        if (module->declaration == NULL)
            module->declaration = declaration;
    }

    for (AstNode *node = program->program.modules; node != NULL; node = node->next)
        tables->definition_count++;
    tables->definitions = calloc(tables->definition_count, sizeof *tables->definitions);

    Definition *d = tables->definitions;
    for (AstNode *node = program->program.modules; node != NULL; node = node->next, d++)
    {
        d->node = node;
        if (node->module.name.lexeme == NULL)
        {
            d->name = "driver";
            continue;
        }
        d->name = node->module.name.lexeme;
        d->module = module_entry(tables, d->name);
        if (d->module->definition == NULL)
            d->module->definition = d;
        else
            d->repeated = true;
    }
    for (int i = 0; i < tables->definition_count; i++)
        declare_parameters(tables, &tables->definitions[i]);
}

void free_symbol_tables(SymbolTables *tables)
{
    for (int i = 0; i < tables->definition_count; i++)
    {
        Definition *d = &tables->definitions[i];
        Symbol *symbol = d->first_symbol;
        while (symbol != NULL)
        {
            Symbol *next = symbol->next_in_module;
            free(symbol);
            symbol = next;
        }
        free(d->inputs);
        free(d->outputs);
    }
    free(tables->definitions);
    while (tables->scopes != NULL)
    {
        Scope *next = tables->scopes->next;
        free(tables->scopes);
        tables->scopes = next;
    }
    for (int i = 0; i < MODULE_TABLE_SIZE; i++)
    {
        Module *module = tables->modules[i];
        while (module != NULL)
        {
            Module *next = module->next_in_table;
            free(module);
            module = next;
        }
    }
    memset(tables, 0, sizeof *tables);
}

static void print_range(const Type *type)
{
    int length = 2;
    if (type->array)
        length = printf("[%s%s, %s%s]", type->low.negative ? "-" : "", type->low.token->lexeme,
                        type->high.negative ? "-" : "", type->high.token->lexeme);
    else
        printf("**");
    printf("%*s", length < 47 ? 48 - length : 1, "");
}

void print_symbol_table(const SymbolTables *tables)
{
    for (int i = 0; i < tables->definition_count; i++)
    {
        const Definition *d = &tables->definitions[i];
        if (d->repeated)
            continue;
        for (const Symbol *symbol = d->first_symbol; symbol != NULL; symbol = symbol->next_in_module)
        {
            const Type *type = &symbol->type;
            char lines[32];
            snprintf(lines, sizeof lines, "[%d-%d]", symbol->scope->first_line, symbol->scope->last_line);
            const char *kind = "**";
            if (type->array)
                kind = type->dynamic ? "dynamic" : "static";
            printf("%-20s %-20s %-14s %-8s %-4s %-8s ", symbol->name->lexeme, d->name, lines, token_text(type->base),
                   type->array ? "yes" : "no", kind);
            print_range(type);
            printf("%-7lld %-7lld %d\n", symbol->width, symbol->offset, symbol->scope->level);
        }
    }
}

void print_activation_records(const SymbolTables *tables)
{
    for (int i = 0; i < tables->definition_count; i++)
        if (!tables->definitions[i].repeated)
            printf("%-20s %lld\n", tables->definitions[i].name, tables->definitions[i].size);
}

void print_array_variables(const SymbolTables *tables)
{
    for (int i = 0; i < tables->definition_count; i++)
    {
        const Definition *d = &tables->definitions[i];
        if (d->repeated)
            continue;
        for (const Symbol *symbol = d->first_symbol; symbol != NULL; symbol = symbol->next_in_module)
        {
            if (!symbol->type.array)
                continue;
            char lines[32];
            snprintf(lines, sizeof lines, "%d-%d", symbol->scope->first_line, symbol->scope->last_line);
            printf("%-20s %-13s %-20s %-14s ", d->name, lines, symbol->name->lexeme,
                   symbol->type.dynamic ? "dynamic array" : "static array");
            print_range(&symbol->type);
            printf("%s\n", token_text(symbol->type.base));
        }
    }
}
