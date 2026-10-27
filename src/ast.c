#include <stdio.h>
#include <stdlib.h>

#include "astDef.h"
#include "lexer.h"
#include "parser.h"

static int node_count;

static AstNode *new_node(AstKind kind, int line)
{
    AstNode *node = calloc(1, sizeof *node);
    node->kind = kind;
    node->line = line;
    node_count++;
    return node;
}

static ParseNode *detach(ParseNode *n, int i)
{
    ParseNode *child = n->children[i];
    n->children[i] = NULL;
    return child;
}

static Token take_token(ParseNode *leaf)
{
    Token token = leaf->token;
    leaf->token.lexeme = NULL;
    return token;
}

static AstNode *leaf(ParseNode *n, int i)
{
    const Token *token = &n->children[i]->token;
    AstKind kind = AST_BOOL;
    if (token->kind == T_NUM)
        kind = AST_NUM;
    else if (token->kind == T_RNUM)
        kind = AST_RNUM;
    AstNode *node = new_node(kind, token->line);
    node->literal = take_token(n->children[i]);
    return node;
}

static AstNode *variable(ParseNode *n, int i)
{
    AstNode *node = new_node(AST_VARIABLE, n->children[i]->token.line);
    node->variable.name = take_token(n->children[i]);
    return node;
}

static AstNode *element(ParseNode *n, int i, AstNode *index)
{
    AstNode *array = variable(n, i);
    if (index == NULL)
        return array;
    AstNode *node = new_node(AST_ELEMENT, array->line);
    node->element.array = array;
    node->element.index = index;
    return node;
}

static AstNode *parameter(ParseNode *n, int i, AstNode *type)
{
    AstNode *node = new_node(AST_PARAMETER, n->children[i]->token.line);
    node->parameter.name = take_token(n->children[i]);
    node->parameter.type = type;
    return node;
}

static AstNode *signed_node(const ParseNode *sign, AstNode *operand)
{
    if (sign->child_count == 0)
        return operand;
    const Token *op = &sign->children[0]->token;
    AstNode *node = new_node(AST_UNARY, op->line);
    node->unary.op = op->kind;
    node->unary.operand = operand;
    return node;
}

static AstNode *join(AstNode *first, AstNode *second)
{
    if (first == NULL)
        return second;
    AstNode *last = first;
    while (last->next != NULL)
        last = last->next;
    last->next = second;
    return first;
}

static AstNode *build(ParseNode *n);

static AstNode *build_list(ParseNode *n)
{
    AstNode *first = NULL, *last = NULL;
    while (n->child_count > 0)
    {
        ParseNode *rest = detach(n, n->child_count - 1);
        AstNode *item = build(n);
        if (first == NULL)
            first = item;
        else
            last->next = item;
        last = item;
        n = rest;
    }
    free_parse_tree(n);
    return first;
}

static AstNode *fold_left(AstNode *left, ParseNode *tail)
{
    while (tail != NULL && tail->child_count > 0)
    {
        const Token *op = &tail->children[0]->children[0]->token;
        AstNode *node = new_node(AST_BINARY, left->line);
        node->binary.op = op->kind;
        node->binary.op_line = op->line;
        node->binary.left = left;
        node->binary.right = build(detach(tail, 1));
        left = node;
        ParseNode *rest = tail->child_count == 3 ? detach(tail, 2) : NULL;
        free_parse_tree(tail);
        tail = rest;
    }
    free_parse_tree(tail);
    return left;
}

static void set_body(AstNode *module, ParseNode *module_def)
{
    module->module.body = build_list(detach(module_def, 1));
    module->module.end_line = module_def->children[2]->token.line;
}

static AstNode *build(ParseNode *n)
{
    ParseNode **c = n->children;
    AstNode *node = NULL;
    switch (n->production)
    {
    case 1:
    {
        node = new_node(AST_PROGRAM, 0);
        node->program.declarations = build_list(detach(n, 0));
        AstNode *before = build_list(detach(n, 1));
        AstNode *driver = build(detach(n, 2));
        driver->next = build_list(detach(n, 3));
        node->program.modules = join(before, driver);
        node->line = node->program.declarations != NULL ? node->program.declarations->line
                                                        : node->program.modules->line;
        break;
    }

    case 2:
    case 5:
    case 32:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 44:
    case 49:
    case 50:
    case 62:
    case 65:
    case 85:
        node = build(detach(n, 0));
        break;

    case 4:
        node = new_node(AST_MODULE_DECL, c[0]->token.line);
        node->module_decl.name = take_token(c[2]);
        break;

    case 7:
        node = new_node(AST_MODULE, c[0]->token.line);
        set_body(node, c[4]);
        break;

    case 8:
        node = new_node(AST_MODULE, c[0]->token.line);
        node->module.name = take_token(c[2]);
        node->module.inputs = build_list(detach(n, 7));
        node->module.outputs = build(detach(n, 10));
        set_body(node, c[11]);
        break;

    case 9:
        node = build_list(detach(n, 2));
        break;

    case 11:
    case 14:
        node = parameter(n, 0, build(detach(n, 2)));
        break;

    case 12:
    case 15:
        node = parameter(n, 1, build(detach(n, 3)));
        break;

    case 17:
    case 18:
    case 19:
    case 22:
    case 23:
    case 24:
        node = new_node(AST_PRIMITIVE, c[0]->token.line);
        node->primitive.type = c[0]->token.kind;
        break;

    case 20:
        node = new_node(AST_ARRAY_TYPE, c[0]->token.line);
        node->array_type.low = build(detach(c[2], 0));
        node->array_type.high = build(detach(c[2], 2));
        node->array_type.element = build(detach(n, 5));
        break;

    case 25:
    case 66:
    case 87:
    case 103:
        node = signed_node(c[0], build(detach(n, 1)));
        break;

    case 26:
    case 42:
    case 43:
    case 47:
    case 48:
    case 67:
    case 68:
    case 83:
    case 84:
    case 89:
    case 90:
    case 101:
    case 105:
    case 126:
    case 127:
    case 128:
        node = leaf(n, 0);
        break;

    case 27:
    case 100:
    case 104:
        node = variable(n, 0);
        break;

    case 39:
        node = new_node(AST_GET_VALUE, c[0]->token.line);
        node->get_value.target = variable(n, 2);
        break;

    case 40:
        node = new_node(AST_PRINT, c[0]->token.line);
        node->print.operand = build(detach(n, 2));
        break;

    case 41:
    case 69:
    case 86:
    case 91:
        node = element(n, 0, build(detach(n, 1)));
        break;

    case 45:
    case 82:
    case 88:
    case 92:
    case 102:
    case 106:
        node = build(detach(n, 1));
        break;

    case 51:
    {
        ParseNode *lvalue = c[1]->children[0];
        if (lvalue->production == 54)
        {
            node = new_node(AST_ASSIGN, c[0]->token.line);
            node->assign.target = variable(n, 0);
            node->assign.value = build(detach(lvalue, 1));
        }
        else
        {
            node = new_node(AST_ASSIGN_ELEMENT, c[0]->token.line);
            node->assign.target = element(n, 0, build(detach(lvalue, 1)));
            node->assign.value = build(detach(lvalue, 4));
        }
        break;
    }

    case 56:
    {
        int line = c[0]->child_count > 0 ? c[0]->children[0]->token.line : c[1]->token.line;
        node = new_node(AST_CALL, line);
        node->call.results = build(detach(n, 0));
        node->call.module = take_token(c[3]);
        node->call.arguments = build_list(detach(n, 6));
        break;
    }

    case 57:
        node = build_list(detach(n, 1));
        break;

    case 59:
        node = variable(n, 0);
        break;

    case 60:
        node = variable(n, 1);
        break;

    case 63:
        node = build(detach(n, 1));
        break;

    case 70:
    case 73:
    case 76:
    case 79:
    case 94:
    case 97:
    {
        AstNode *left = build(detach(n, 0));
        node = fold_left(left, detach(n, 1));
        break;
    }

    case 121:
        node = new_node(AST_DECLARE, c[0]->token.line);
        node->declare.names = build_list(detach(n, 1));
        node->declare.type = build(detach(n, 3));
        break;

    case 122:
        node = new_node(AST_SWITCH, c[0]->token.line);
        node->switch_stmt.selector = variable(n, 2);
        node->switch_stmt.cases = build_list(detach(n, 5));
        node->switch_stmt.default_case = build(detach(n, 6));
        node->switch_stmt.end_line = c[7]->token.line;
        break;

    case 123:
    case 124:
        node = new_node(AST_CASE, c[0]->token.line);
        node->case_stmt.value = build(detach(n, 1));
        node->case_stmt.body = build_list(detach(n, 3));
        node->case_stmt.break_line = c[4]->token.line;
        break;

    case 129:
        node = new_node(AST_DEFAULT, c[0]->token.line);
        node->case_stmt.body = build_list(detach(n, 2));
        node->case_stmt.break_line = c[3]->token.line;
        break;

    case 131:
        node = new_node(AST_FOR, c[0]->token.line);
        node->for_stmt.variable = variable(n, 2);
        node->for_stmt.low = build(detach(c[4], 0));
        node->for_stmt.high = build(detach(c[4], 2));
        node->for_stmt.body = build_list(detach(n, 7));
        node->for_stmt.end_line = c[8]->token.line;
        break;

    case 132:
        node = new_node(AST_WHILE, c[0]->token.line);
        node->while_stmt.condition = build(detach(n, 2));
        node->while_stmt.body = build_list(detach(n, 5));
        node->while_stmt.end_line = c[6]->token.line;
        break;

    case 134:
        node = signed_node(c[0], leaf(n, 1));
        break;

    default:
        break;
    }
    free_parse_tree(n);
    return node;
}

AstNode *build_ast(ParseNode *root, int *count)
{
    node_count = 0;
    AstNode *program = build(root);
    *count = node_count;
    return program;
}

static const char *const construct_names[] = {
    [AST_PROGRAM] = "Program",
    [AST_MODULE_DECL] = "ModuleDeclaration",
    [AST_MODULE] = "Module",
    [AST_PARAMETER] = "Parameter",
    [AST_PRIMITIVE] = "Type",
    [AST_ARRAY_TYPE] = "ArrayType",
    [AST_DECLARE] = "Declare",
    [AST_ASSIGN] = "Assign",
    [AST_ASSIGN_ELEMENT] = "AssignElement",
    [AST_GET_VALUE] = "GetValue",
    [AST_PRINT] = "Print",
    [AST_CALL] = "ModuleCall",
    [AST_SWITCH] = "Switch",
    [AST_CASE] = "Case",
    [AST_DEFAULT] = "Default",
    [AST_FOR] = "For",
    [AST_WHILE] = "While",
    [AST_BINARY] = "Binary",
    [AST_UNARY] = "Unary",
    [AST_ELEMENT] = "Element",
    [AST_VARIABLE] = "Variable",
    [AST_NUM] = "Number",
    [AST_RNUM] = "Real",
    [AST_BOOL] = "Boolean",
};

static void print_node(const AstNode *n, const char *role, int depth);

static void print_list(const AstNode *list, const char *role, int depth)
{
    for (; list != NULL; list = list->next)
        print_node(list, role, depth);
}

static void print_node(const AstNode *n, const char *role, int depth)
{
    printf("%*s", 2 * depth, "");
    if (role != NULL)
        printf("%s: ", role);
    printf("%s", n->kind == AST_MODULE && n->module.name.lexeme == NULL ? "Driver" : construct_names[n->kind]);

    const char *detail = NULL;
    int end_line = 0;
    switch (n->kind)
    {
    case AST_MODULE_DECL:
        detail = n->module_decl.name.lexeme;
        break;
    case AST_MODULE:
        detail = n->module.name.lexeme;
        end_line = n->module.end_line;
        break;
    case AST_PARAMETER:
        detail = n->parameter.name.lexeme;
        break;
    case AST_PRIMITIVE:
        detail = token_text(n->primitive.type);
        break;
    case AST_CALL:
        detail = n->call.module.lexeme;
        break;
    case AST_SWITCH:
        end_line = n->switch_stmt.end_line;
        break;
    case AST_CASE:
    case AST_DEFAULT:
        end_line = n->case_stmt.break_line;
        break;
    case AST_FOR:
        end_line = n->for_stmt.end_line;
        break;
    case AST_WHILE:
        end_line = n->while_stmt.end_line;
        break;
    case AST_BINARY:
        detail = token_text(n->binary.op);
        break;
    case AST_UNARY:
        detail = token_text(n->unary.op);
        break;
    case AST_VARIABLE:
        detail = n->variable.name.lexeme;
        break;
    case AST_NUM:
    case AST_RNUM:
    case AST_BOOL:
        detail = n->literal.lexeme;
        break;
    default:
        break;
    }
    if (detail != NULL && detail[0] != '\0')
        printf(" %s", detail);
    if (end_line > 0)
        printf(" (lines %d-%d)\n", n->line, end_line);
    else if (n->kind == AST_BINARY && n->binary.op_line != n->line)
        printf(" (line %d, operator on line %d)\n", n->line, n->binary.op_line);
    else
        printf(" (line %d)\n", n->line);

    depth++;
    switch (n->kind)
    {
    case AST_PROGRAM:
        print_list(n->program.declarations, "declaration", depth);
        print_list(n->program.modules, "module", depth);
        break;
    case AST_MODULE:
        print_list(n->module.inputs, "input", depth);
        print_list(n->module.outputs, "output", depth);
        print_list(n->module.body, "statement", depth);
        break;
    case AST_PARAMETER:
        print_node(n->parameter.type, "type", depth);
        break;
    case AST_ARRAY_TYPE:
        print_node(n->array_type.low, "low", depth);
        print_node(n->array_type.high, "high", depth);
        print_node(n->array_type.element, "element", depth);
        break;
    case AST_DECLARE:
        print_list(n->declare.names, "name", depth);
        print_node(n->declare.type, "type", depth);
        break;
    case AST_ASSIGN:
    case AST_ASSIGN_ELEMENT:
        print_node(n->assign.target, "target", depth);
        print_node(n->assign.value, "value", depth);
        break;
    case AST_GET_VALUE:
        print_node(n->get_value.target, "target", depth);
        break;
    case AST_PRINT:
        print_node(n->print.operand, "operand", depth);
        break;
    case AST_CALL:
        print_list(n->call.results, "result", depth);
        print_list(n->call.arguments, "argument", depth);
        break;
    case AST_SWITCH:
        print_node(n->switch_stmt.selector, "selector", depth);
        print_list(n->switch_stmt.cases, "case", depth);
        if (n->switch_stmt.default_case != NULL)
            print_node(n->switch_stmt.default_case, "default", depth);
        break;
    case AST_CASE:
        print_node(n->case_stmt.value, "value", depth);
        print_list(n->case_stmt.body, "statement", depth);
        break;
    case AST_DEFAULT:
        print_list(n->case_stmt.body, "statement", depth);
        break;
    case AST_FOR:
        print_node(n->for_stmt.variable, "variable", depth);
        print_node(n->for_stmt.low, "low", depth);
        print_node(n->for_stmt.high, "high", depth);
        print_list(n->for_stmt.body, "statement", depth);
        break;
    case AST_WHILE:
        print_node(n->while_stmt.condition, "condition", depth);
        print_list(n->while_stmt.body, "statement", depth);
        break;
    case AST_BINARY:
        print_node(n->binary.left, "left", depth);
        print_node(n->binary.right, "right", depth);
        break;
    case AST_UNARY:
        print_node(n->unary.operand, "operand", depth);
        break;
    case AST_ELEMENT:
        print_node(n->element.array, "array", depth);
        print_node(n->element.index, "index", depth);
        break;
    default:
        break;
    }
}

void print_ast(const AstNode *root)
{
    printf("AST in preorder; each level indented by two spaces; \"role:\" names a node's place in its parent\n");
    print_node(root, NULL, 0);
}

void free_ast(AstNode *node)
{
    while (node != NULL)
    {
        AstNode *next = node->next;
        switch (node->kind)
        {
        case AST_PROGRAM:
            free_ast(node->program.declarations);
            free_ast(node->program.modules);
            break;
        case AST_MODULE_DECL:
            free(node->module_decl.name.lexeme);
            break;
        case AST_MODULE:
            free(node->module.name.lexeme);
            free_ast(node->module.inputs);
            free_ast(node->module.outputs);
            free_ast(node->module.body);
            break;
        case AST_PARAMETER:
            free(node->parameter.name.lexeme);
            free_ast(node->parameter.type);
            break;
        case AST_ARRAY_TYPE:
            free_ast(node->array_type.low);
            free_ast(node->array_type.high);
            free_ast(node->array_type.element);
            break;
        case AST_DECLARE:
            free_ast(node->declare.names);
            free_ast(node->declare.type);
            break;
        case AST_ASSIGN:
        case AST_ASSIGN_ELEMENT:
            free_ast(node->assign.target);
            free_ast(node->assign.value);
            break;
        case AST_GET_VALUE:
            free_ast(node->get_value.target);
            break;
        case AST_PRINT:
            free_ast(node->print.operand);
            break;
        case AST_CALL:
            free(node->call.module.lexeme);
            free_ast(node->call.results);
            free_ast(node->call.arguments);
            break;
        case AST_SWITCH:
            free_ast(node->switch_stmt.selector);
            free_ast(node->switch_stmt.cases);
            free_ast(node->switch_stmt.default_case);
            break;
        case AST_CASE:
        case AST_DEFAULT:
            free_ast(node->case_stmt.value);
            free_ast(node->case_stmt.body);
            break;
        case AST_FOR:
            free_ast(node->for_stmt.variable);
            free_ast(node->for_stmt.low);
            free_ast(node->for_stmt.high);
            free_ast(node->for_stmt.body);
            break;
        case AST_WHILE:
            free_ast(node->while_stmt.condition);
            free_ast(node->while_stmt.body);
            break;
        case AST_BINARY:
            free_ast(node->binary.left);
            free_ast(node->binary.right);
            break;
        case AST_UNARY:
            free_ast(node->unary.operand);
            break;
        case AST_ELEMENT:
            free_ast(node->element.array);
            free_ast(node->element.index);
            break;
        case AST_VARIABLE:
            free(node->variable.name.lexeme);
            break;
        case AST_NUM:
        case AST_RNUM:
        case AST_BOOL:
            free(node->literal.lexeme);
            break;
        default:
            break;
        }
        free(node);
        node = next;
    }
}
