#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "symbolTable.h"
#include "symbolTableDef.h"
#include "typechecker.h"

enum
{
    ARRAY_LOW = 8,
    ARRAY_HIGH = 16,
    ARRAY_SIZE = 24
};

static FILE *out;
static int label_count;

static void lay_out_parameters(Definition *d)
{
    long long size = 0;
    for (int i = 0; i < d->input_count; i++)
    {
        d->inputs[i]->frame_offset = 16 + size;
        size += d->inputs[i]->type.array ? ARRAY_SIZE : 8;
    }
    for (int i = 0; i < d->output_count; i++)
    {
        d->outputs[i]->frame_offset = 16 + size;
        size += 8;
    }
    d->parameter_block_size = (size + 15) / 16 * 16;
}

static void load(const Symbol *symbol)
{
    if (symbol->type.base == T_REAL)
        fprintf(out, "\tmovsd xmm0, [rbp%+lld]\n", symbol->frame_offset);
    else
        fprintf(out, "\tmov rax, [rbp%+lld]\n", symbol->frame_offset);
}

static void store(const Symbol *symbol)
{
    if (symbol->type.base == T_REAL)
        fprintf(out, "\tmovsd [rbp%+lld], xmm0\n", symbol->frame_offset);
    else
        fprintf(out, "\tmov [rbp%+lld], rax\n", symbol->frame_offset);
}

static void emit_expression(const AstNode *node);

static void emit_element_address(const AstNode *node)
{
    long long array = node->element.array->variable.symbol->frame_offset;
    emit_expression(node->element.index);
    fprintf(out, "\tcmp rax, [rbp%+lld]\n", array + ARRAY_LOW);
    fprintf(out, "\tjl rt_index_out_of_bound\n");
    fprintf(out, "\tcmp rax, [rbp%+lld]\n", array + ARRAY_HIGH);
    fprintf(out, "\tjg rt_index_out_of_bound\n");
    fprintf(out, "\tsub rax, [rbp%+lld]\n", array + ARRAY_LOW);
    fprintf(out, "\tmov rcx, [rbp%+lld]\n", array);
    fprintf(out, "\tlea rcx, [rcx+rax*8]\n");
}

static void emit_unary(const AstNode *node)
{
    if (node->unary.op == T_PLUS)
        return;
    if (node->unary.type == TYPE_REAL)
    {
        fprintf(out, "\txorpd xmm1, xmm1\n");
        fprintf(out, "\tsubsd xmm1, xmm0\n");
        fprintf(out, "\tmovapd xmm0, xmm1\n");
    }
    else
    {
        fprintf(out, "\tneg rax\n");
    }
}

static void save_left(const AstNode *node)
{
    ValueType left = expression_type(node->binary.left);
    if (node->binary.op == T_DIV && left == TYPE_INTEGER)
        fprintf(out, "\tcvtsi2sd xmm0, rax\n");
    if (node->binary.op == T_DIV || left == TYPE_REAL)
    {
        fprintf(out, "\tsub rsp, 8\n");
        fprintf(out, "\tmovsd [rsp], xmm0\n");
    }
    else
    {
        fprintf(out, "\tpush rax\n");
    }
}

static void emit_division(const AstNode *node)
{
    if (expression_type(node->binary.right) == TYPE_INTEGER)
        fprintf(out, "\tcvtsi2sd xmm0, rax\n");
    fprintf(out, "\txorpd xmm1, xmm1\n");
    fprintf(out, "\tucomisd xmm0, xmm1\n");
    fprintf(out, "\tje rt_division_by_zero\n");
    fprintf(out, "\tmovapd xmm1, xmm0\n");
    fprintf(out, "\tmovsd xmm0, [rsp]\n");
    fprintf(out, "\tadd rsp, 8\n");
    fprintf(out, "\tdivsd xmm0, xmm1\n");
}

static const char *condition(TokenKind op, bool real)
{
    switch (op)
    {
    case T_LT:
        return real ? "b" : "l";
    case T_LE:
        return real ? "be" : "le";
    case T_GT:
        return real ? "a" : "g";
    case T_GE:
        return real ? "ae" : "ge";
    case T_EQ:
        return "e";
    default:
        return "ne";
    }
}

static void emit_operation(const AstNode *node)
{
    TokenKind op = node->binary.op;
    if (op == T_DIV)
    {
        emit_division(node);
        return;
    }
    bool real = expression_type(node->binary.left) == TYPE_REAL;
    if (real)
    {
        fprintf(out, "\tmovapd xmm1, xmm0\n");
        fprintf(out, "\tmovsd xmm0, [rsp]\n");
        fprintf(out, "\tadd rsp, 8\n");
    }
    else
    {
        fprintf(out, "\tmov rcx, rax\n");
        fprintf(out, "\tpop rax\n");
    }
    if (op == T_PLUS)
        fputs(real ? "\taddsd xmm0, xmm1\n" : "\tadd rax, rcx\n", out);
    else if (op == T_MINUS)
        fputs(real ? "\tsubsd xmm0, xmm1\n" : "\tsub rax, rcx\n", out);
    else if (op == T_MUL)
        fputs(real ? "\tmulsd xmm0, xmm1\n" : "\timul rax, rcx\n", out);
    else if (op == T_AND)
        fprintf(out, "\tand rax, rcx\n");
    else if (op == T_OR)
        fprintf(out, "\tor rax, rcx\n");
    else
    {
        fputs(real ? "\tucomisd xmm0, xmm1\n" : "\tcmp rax, rcx\n", out);
        fprintf(out, "\tset%s al\n", condition(op, real));
        fprintf(out, "\tmovzx eax, al\n");
    }
}

static void emit_expression(const AstNode *node)
{
    switch (node->kind)
    {
    case AST_BINARY:
        emit_expression(node->binary.left);
        save_left(node);
        emit_expression(node->binary.right);
        emit_operation(node);
        break;
    case AST_UNARY:
        emit_expression(node->unary.operand);
        emit_unary(node);
        break;
    case AST_VARIABLE:
        load(node->variable.symbol);
        break;
    case AST_ELEMENT:
        emit_element_address(node);
        if (expression_type(node) == TYPE_REAL)
            fprintf(out, "\tmovsd xmm0, [rcx]\n");
        else
            fprintf(out, "\tmov rax, [rcx]\n");
        break;
    case AST_NUM:
        fprintf(out, "\tmov rax, %lld\n", node->literal.value.num);
        break;
    case AST_RNUM:
    {
        unsigned long long bits;
        memcpy(&bits, &node->literal.value.rnum, sizeof bits);
        fprintf(out, "\tmov rax, 0x%016llx\n", bits);
        fprintf(out, "\tmovq xmm0, rax\n");
        break;
    }
    case AST_BOOL:
        fprintf(out, "\tmov rax, %d\n", node->literal.kind == T_TRUE);
        break;
    default:
        break;
    }
}

static int element_code(TokenKind base)
{
    if (base == T_INTEGER)
        return 0;
    if (base == T_REAL)
        return 1;
    return 2;
}

static void emit_input(const Symbol *symbol)
{
    if (symbol->type.array)
    {
        fprintf(out, "\tlea rdi, [rbp%+lld]\n", symbol->frame_offset);
        fprintf(out, "\tmov rsi, %d\n", element_code(symbol->type.base));
        fprintf(out, "\tcall rt_read_array\n");
        return;
    }
    fprintf(out, "\tcall rt_read_%s\n", token_text(symbol->type.base));
    store(symbol);
}

static void emit_print(const AstNode *operand)
{
    const Symbol *symbol = operand->kind == AST_VARIABLE ? operand->variable.symbol : NULL;
    if (symbol != NULL && symbol->type.array)
    {
        fprintf(out, "\tlea rdi, [rbp%+lld]\n", symbol->frame_offset);
        fprintf(out, "\tmov rsi, %d\n", element_code(symbol->type.base));
        fprintf(out, "\tcall rt_print_array\n");
        return;
    }
    emit_expression(operand);
    ValueType type = expression_type(operand);
    if (type != TYPE_REAL)
        fprintf(out, "\tmov rdi, rax\n");
    fprintf(out, "\tcall rt_print_%s\n", type_name(type));
}

static void emit_bound(const Bound *bound, const char *reg)
{
    if (bound->token->kind == T_NUM)
    {
        fprintf(out, "\tmov %s, %lld\n", reg, bound->negative ? -bound->token->value.num : bound->token->value.num);
        return;
    }
    fprintf(out, "\tmov %s, [rbp%+lld]\n", reg, bound->variable->frame_offset);
    if (bound->negative)
        fprintf(out, "\tneg %s\n", reg);
}

static void emit_declaration(const AstNode *node)
{
    for (const AstNode *name = node->declare.names; name != NULL; name = name->next)
    {
        const Symbol *array = name->variable.symbol;
        if (!array->type.array)
            continue;
        long long descriptor = array->frame_offset;
        emit_bound(&array->type.low, "rsi");
        emit_bound(&array->type.high, "rdx");
        fprintf(out, "\tmov [rbp%+lld], rsi\n", descriptor + ARRAY_LOW);
        fprintf(out, "\tmov [rbp%+lld], rdx\n", descriptor + ARRAY_HIGH);
        if (array->type.dynamic)
        {
            fprintf(out, "\tcmp rsi, rdx\n");
            fprintf(out, "\tjg rt_invalid_array_range\n");
            fprintf(out, "\tmov rdi, rdx\n");
            fprintf(out, "\tsub rdi, rsi\n");
            fprintf(out, "\tadd rdi, 1\n");
            fprintf(out, "\tjc rt_out_of_memory\n");
            fprintf(out, "\tmov rsi, 8\n");
            fprintf(out, "\tcall calloc\n");
            fprintf(out, "\ttest rax, rax\n");
            fprintf(out, "\tjz rt_out_of_memory\n");
        }
        else
        {
            fprintf(out, "\tlea rax, [rbp%+lld]\n", descriptor + ARRAY_SIZE);
        }
        fprintf(out, "\tmov [rbp%+lld], rax\n", descriptor);
    }
}

static void emit_size_check(void)
{
    fprintf(out, "\tmov rax, [rdi+%d]\n", ARRAY_HIGH);
    fprintf(out, "\tsub rax, [rdi+%d]\n", ARRAY_LOW);
    fprintf(out, "\tmov rcx, [rsi+%d]\n", ARRAY_HIGH);
    fprintf(out, "\tsub rcx, [rsi+%d]\n", ARRAY_LOW);
    fprintf(out, "\tcmp rax, rcx\n");
    fprintf(out, "\tjne rt_array_size_mismatch\n");
}

static void emit_assignment(const AstNode *node)
{
    const Symbol *target = node->assign.target->variable.symbol;
    if (!target->type.array)
    {
        emit_expression(node->assign.value);
        store(target);
        return;
    }
    const Symbol *source = node->assign.value->variable.symbol;
    if (target->type.dynamic || source->type.dynamic)
    {
        fprintf(out, "\tlea rdi, [rbp%+lld]\n", target->frame_offset);
        fprintf(out, "\tlea rsi, [rbp%+lld]\n", source->frame_offset);
        emit_size_check();
    }
    fprintf(out, "\tmov rax, [rbp%+lld]\n", source->frame_offset);
    fprintf(out, "\tmov [rbp%+lld], rax\n", target->frame_offset);
}

static void emit_element_assignment(const AstNode *node)
{
    emit_element_address(node->assign.target);
    fprintf(out, "\tpush rcx\n");
    emit_expression(node->assign.value);
    fprintf(out, "\tpop rcx\n");
    if (expression_type(node->assign.target) == TYPE_REAL)
        fprintf(out, "\tmovsd [rcx], xmm0\n");
    else
        fprintf(out, "\tmov [rcx], rax\n");
}

static void emit_call(const AstNode *node)
{
    const Definition *callee = node->call.callee;
    fprintf(out, "\tsub rsp, %lld\n", callee->parameter_block_size);
    const AstNode *argument = node->call.arguments;
    for (int i = 0; i < callee->input_count; i++, argument = argument->next)
    {
        const Symbol *formal = callee->inputs[i];
        long long slot = formal->frame_offset - 16;
        if (!formal->type.array)
        {
            emit_expression(argument);
            if (expression_type(argument) == TYPE_REAL)
                fprintf(out, "\tmovsd [rsp+%lld], xmm0\n", slot);
            else
                fprintf(out, "\tmov [rsp+%lld], rax\n", slot);
            continue;
        }
        const Symbol *actual = argument->variable.symbol;
        long long low, high;
        static_bounds(&formal->type, &low, &high);
        fprintf(out, "\tmov rax, %lld\n", low);
        fprintf(out, "\tmov [rsp+%lld], rax\n", slot + ARRAY_LOW);
        fprintf(out, "\tmov rax, %lld\n", high);
        fprintf(out, "\tmov [rsp+%lld], rax\n", slot + ARRAY_HIGH);
        if (actual->type.dynamic)
        {
            fprintf(out, "\tlea rdi, [rsp+%lld]\n", slot);
            fprintf(out, "\tlea rsi, [rbp%+lld]\n", actual->frame_offset);
            emit_size_check();
        }
        fprintf(out, "\tmov rax, [rbp%+lld]\n", actual->frame_offset);
        fprintf(out, "\tmov [rsp+%lld], rax\n", slot);
    }
    fprintf(out, "\tcall module_%s\n", callee->name);
    const AstNode *result = node->call.results;
    for (int i = 0; i < callee->output_count; i++, result = result->next)
    {
        fprintf(out, "\tmov rax, [rsp+%lld]\n", callee->outputs[i]->frame_offset - 16);
        fprintf(out, "\tmov [rbp%+lld], rax\n", result->variable.symbol->frame_offset);
    }
    fprintf(out, "\tadd rsp, %lld\n", callee->parameter_block_size);
}

static void emit_statements(const AstNode *statements);

static void emit_switch(const AstNode *node)
{
    int label = label_count++;
    const AstNode *other = node->switch_stmt.default_case;
    load(node->switch_stmt.selector->variable.symbol);
    int number = 0;
    for (const AstNode *c = node->switch_stmt.cases; c != NULL; c = c->next, number++)
    {
        const Token *value = &c->case_stmt.value->literal;
        long long constant = value->kind == T_TRUE ? 1 : 0;
        if (value->kind == T_NUM)
            constant = value->value.num;
        fprintf(out, "\tmov rcx, %lld\n", constant);
        fprintf(out, "\tcmp rax, rcx\n");
        fprintf(out, "\tje .L%d_case%d\n", label, number);
    }
    fprintf(out, "\tjmp .L%d_%s\n", label, other != NULL ? "default" : "end");
    number = 0;
    for (const AstNode *c = node->switch_stmt.cases; c != NULL; c = c->next, number++)
    {
        fprintf(out, ".L%d_case%d:\n", label, number);
        emit_statements(c->case_stmt.body);
        fprintf(out, "\tjmp .L%d_end\n", label);
    }
    if (other != NULL)
    {
        fprintf(out, ".L%d_default:\n", label);
        emit_statements(other->case_stmt.body);
    }
    fprintf(out, ".L%d_end:\n", label);
}

static long long loop_bound(const AstNode *node)
{
    if (node->kind == AST_UNARY && node->unary.op == T_MINUS)
        return -node->unary.operand->literal.value.num;
    if (node->kind == AST_UNARY)
        return node->unary.operand->literal.value.num;
    return node->literal.value.num;
}

static void emit_for(const AstNode *node)
{
    int label = label_count++;
    const Symbol *variable = node->for_stmt.variable->variable.symbol;
    long long first = loop_bound(node->for_stmt.low), last = loop_bound(node->for_stmt.high);
    fprintf(out, "\tmov rax, %lld\n", first);
    store(variable);
    fprintf(out, ".L%d_for:\n", label);
    emit_statements(node->for_stmt.body);
    load(variable);
    fprintf(out, "\tmov rcx, %lld\n", last);
    fprintf(out, "\tcmp rax, rcx\n");
    fprintf(out, "\tje .L%d_end\n", label);
    fprintf(out, "\t%s rax, 1\n", first <= last ? "add" : "sub");
    store(variable);
    fprintf(out, "\tjmp .L%d_for\n", label);
    fprintf(out, ".L%d_end:\n", label);
}

static void emit_while(const AstNode *node)
{
    int label = label_count++;
    fprintf(out, ".L%d_while:\n", label);
    emit_expression(node->while_stmt.condition);
    fprintf(out, "\tcmp rax, 0\n");
    fprintf(out, "\tje .L%d_end\n", label);
    emit_statements(node->while_stmt.body);
    fprintf(out, "\tjmp .L%d_while\n", label);
    fprintf(out, ".L%d_end:\n", label);
}

static void emit_statements(const AstNode *statements)
{
    for (const AstNode *s = statements; s != NULL; s = s->next)
    {
        switch (s->kind)
        {
        case AST_DECLARE:
            emit_declaration(s);
            break;
        case AST_ASSIGN:
            emit_assignment(s);
            break;
        case AST_ASSIGN_ELEMENT:
            emit_element_assignment(s);
            break;
        case AST_GET_VALUE:
            emit_input(s->get_value.target->variable.symbol);
            break;
        case AST_PRINT:
            emit_print(s->print.operand);
            break;
        case AST_CALL:
            emit_call(s);
            break;
        case AST_SWITCH:
            emit_switch(s);
            break;
        case AST_FOR:
            emit_for(s);
            break;
        case AST_WHILE:
            emit_while(s);
            break;
        default:
            break;
        }
    }
}

static void emit_definition(const Definition *d)
{
    if (d->module == NULL)
        fprintf(out, "main:\n");
    else
        fprintf(out, "module_%s:\n", d->name);
    fprintf(out, "\tpush rbp\n");
    fprintf(out, "\tmov rbp, rsp\n");
    long long frame = (d->frame_size + 15) / 16 * 16;
    if (frame > 0)
        fprintf(out, "\tsub rsp, %lld\n", frame);
    emit_statements(d->node->module.body);
    if (d->module == NULL)
        fprintf(out, "\tmov rax, 0\n");
    fprintf(out, "\tleave\n");
    fprintf(out, "\tret\n\n");
}

static void emit_input_routines(void)
{
    fprintf(out, "rt_read_integer:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tsub rsp, 16\n"
                 "\tlea rdi, [prompt_integer]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov qword [rbp-8], 0\n"
                 "\tlea rdi, [scan_integer]\n"
                 "\tlea rsi, [rbp-8]\n"
                 "\tmov rax, 0\n"
                 "\tcall scanf\n"
                 "\tmov rax, [rbp-8]\n"
                 "\tleave\n"
                 "\tret\n\n");
    fprintf(out, "rt_read_real:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tsub rsp, 16\n"
                 "\tlea rdi, [prompt_real]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov qword [rbp-8], 0\n"
                 "\tlea rdi, [scan_real]\n"
                 "\tlea rsi, [rbp-8]\n"
                 "\tmov rax, 0\n"
                 "\tcall scanf\n"
                 "\tmovsd xmm0, [rbp-8]\n"
                 "\tleave\n"
                 "\tret\n\n");
    fprintf(out, "rt_read_boolean:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tsub rsp, 16\n"
                 "\tlea rdi, [prompt_boolean]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov qword [rbp-8], 0\n"
                 "\tlea rdi, [scan_integer]\n"
                 "\tlea rsi, [rbp-8]\n"
                 "\tmov rax, 0\n"
                 "\tcall scanf\n"
                 "\tcmp qword [rbp-8], 0\n"
                 "\tsetne al\n"
                 "\tmovzx eax, al\n"
                 "\tleave\n"
                 "\tret\n\n");
}

static void emit_output_routines(void)
{
    fprintf(out, "rt_print_integer:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tmov rsi, rdi\n"
                 "\tlea rdi, [print_integer]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tleave\n"
                 "\tret\n\n");
    fprintf(out, "rt_print_real:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tlea rdi, [print_real]\n"
                 "\tmov rax, 1\n"
                 "\tcall printf\n"
                 "\tleave\n"
                 "\tret\n\n");
    fprintf(out, "rt_print_boolean:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tlea rax, [print_true]\n"
                 "\tcmp rdi, 0\n"
                 "\tjne .print\n"
                 "\tlea rax, [print_false]\n"
                 ".print:\n"
                 "\tmov rdi, rax\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tleave\n"
                 "\tret\n\n");
}

static void emit_array_input_output_routines(void)
{
    fprintf(out, "rt_read_array:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tpush rbx\n"
                 "\tpush r12\n"
                 "\tpush r13\n"
                 "\tpush r14\n"
                 "\tmov rbx, rdi\n"
                 "\tmov r12, rsi\n"
                 "\tlea rdi, [prompt_array]\n"
                 "\tmov rsi, [rbx+16]\n"
                 "\tsub rsi, [rbx+8]\n"
                 "\tadd rsi, 1\n"
                 "\tlea rax, [type_names]\n"
                 "\tmov rdx, [rax+r12*8]\n"
                 "\tmov rcx, [rbx+8]\n"
                 "\tmov r8, [rbx+16]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov r13, [rbx]\n"
                 "\tmov r14, [rbx+16]\n"
                 "\tsub r14, [rbx+8]\n"
                 "\tadd r14, 1\n"
                 ".next:\n"
                 "\tmov qword [r13], 0\n"
                 "\tlea rax, [scan_formats]\n"
                 "\tmov rdi, [rax+r12*8]\n"
                 "\tmov rsi, r13\n"
                 "\tmov rax, 0\n"
                 "\tcall scanf\n"
                 "\tcmp r12, 2\n"
                 "\tjne .stored\n"
                 "\tcmp qword [r13], 0\n"
                 "\tsetne al\n"
                 "\tmovzx eax, al\n"
                 "\tmov [r13], rax\n"
                 ".stored:\n"
                 "\tadd r13, 8\n"
                 "\tsub r14, 1\n"
                 "\tjnz .next\n"
                 "\tpop r14\n"
                 "\tpop r13\n"
                 "\tpop r12\n"
                 "\tpop rbx\n"
                 "\tpop rbp\n"
                 "\tret\n\n");
    fprintf(out, "rt_print_array:\n"
                 "\tpush rbp\n"
                 "\tmov rbp, rsp\n"
                 "\tpush rbx\n"
                 "\tpush r12\n"
                 "\tpush r13\n"
                 "\tpush r14\n"
                 "\tmov rbx, rdi\n"
                 "\tmov r12, rsi\n"
                 "\tlea rdi, [print_array]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov r13, [rbx]\n"
                 "\tmov r14, [rbx+16]\n"
                 "\tsub r14, [rbx+8]\n"
                 "\tadd r14, 1\n"
                 ".next:\n"
                 "\tcmp r12, 1\n"
                 "\tje .real\n"
                 "\tcmp r12, 2\n"
                 "\tje .boolean\n"
                 "\tlea rdi, [print_integer_element]\n"
                 "\tmov rsi, [r13]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tjmp .printed\n"
                 ".real:\n"
                 "\tlea rdi, [print_real_element]\n"
                 "\tmovsd xmm0, [r13]\n"
                 "\tmov rax, 1\n"
                 "\tcall printf\n"
                 "\tjmp .printed\n"
                 ".boolean:\n"
                 "\tlea rdi, [print_true_element]\n"
                 "\tcmp qword [r13], 0\n"
                 "\tjne .print_boolean\n"
                 "\tlea rdi, [print_false_element]\n"
                 ".print_boolean:\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 ".printed:\n"
                 "\tadd r13, 8\n"
                 "\tsub r14, 1\n"
                 "\tjnz .next\n"
                 "\tlea rdi, [newline]\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tpop r14\n"
                 "\tpop r13\n"
                 "\tpop r12\n"
                 "\tpop rbx\n"
                 "\tpop rbp\n"
                 "\tret\n\n");
}

static void emit_error_routines(void)
{
    fprintf(out, "rt_index_out_of_bound:\n"
                 "\tlea rdi, [error_index_out_of_bound]\n"
                 "\tjmp rt_fail\n\n"
                 "rt_array_size_mismatch:\n"
                 "\tlea rdi, [error_array_size_mismatch]\n"
                 "\tjmp rt_fail\n\n"
                 "rt_invalid_array_range:\n"
                 "\tlea rdi, [error_invalid_array_range]\n"
                 "\tjmp rt_fail\n\n"
                 "rt_out_of_memory:\n"
                 "\tlea rdi, [error_out_of_memory]\n"
                 "\tjmp rt_fail\n\n"
                 "rt_division_by_zero:\n"
                 "\tlea rdi, [error_division_by_zero]\n"
                 "\tjmp rt_fail\n\n"
                 "rt_fail:\n"
                 "\tand rsp, -16\n"
                 "\tmov rax, 0\n"
                 "\tcall printf\n"
                 "\tmov rdi, 1\n"
                 "\tcall exit\n\n");
}

static void emit_data(void)
{
    fprintf(out, "section .rodata\n"
                 "prompt_integer: db `Input: Enter an integer value\\n`, 0\n"
                 "prompt_real: db `Input: Enter a real value\\n`, 0\n"
                 "prompt_boolean: db `Input: Enter a boolean value\\n`, 0\n"
                 "prompt_array: db `Input: Enter %%lld array elements of %%s type for range %%lld to %%lld\\n`, 0\n"
                 "scan_integer: db `%%lld`, 0\n"
                 "scan_real: db `%%lf`, 0\n"
                 "print_integer: db `Output: %%lld\\n`, 0\n"
                 "print_real: db `Output: %%.4f\\n`, 0\n"
                 "print_true: db `Output: true\\n`, 0\n"
                 "print_false: db `Output: false\\n`, 0\n"
                 "print_array: db `Output:`, 0\n"
                 "print_integer_element: db ` %%lld`, 0\n"
                 "print_real_element: db ` %%.4f`, 0\n"
                 "print_true_element: db ` true`, 0\n"
                 "print_false_element: db ` false`, 0\n"
                 "newline: db `\\n`, 0\n"
                 "name_integer: db `integer`, 0\n"
                 "name_real: db `real`, 0\n"
                 "name_boolean: db `boolean`, 0\n"
                 "error_index_out_of_bound: db `RUN TIME ERROR: Array index out of bound\\n`, 0\n"
                 "error_array_size_mismatch: db `RUN TIME ERROR: Array size mismatch\\n`, 0\n"
                 "error_invalid_array_range: db `RUN TIME ERROR: Invalid array range\\n`, 0\n"
                 "error_out_of_memory: db `RUN TIME ERROR: Out of memory\\n`, 0\n"
                 "error_division_by_zero: db `RUN TIME ERROR: Division by zero\\n`, 0\n"
                 "type_names: dq name_integer, name_real, name_boolean\n"
                 "scan_formats: dq scan_integer, scan_real, scan_integer\n");
}

void generate_code(SymbolTables *tables, FILE *file)
{
    out = file;
    label_count = 0;
    for (int i = 0; i < tables->definition_count; i++)
        lay_out_parameters(&tables->definitions[i]);
    fprintf(out, "default rel\n");
    fprintf(out, "global main\n");
    fprintf(out, "extern printf, scanf, calloc, exit\n\n");
    fprintf(out, "section .text\n");
    for (int i = 0; i < tables->definition_count; i++)
        emit_definition(&tables->definitions[i]);
    emit_input_routines();
    emit_output_routines();
    emit_array_input_output_routines();
    emit_error_routines();
    emit_data();
}
