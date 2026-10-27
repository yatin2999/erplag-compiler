#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexerDef.h"

enum
{
    MAX_IDENTIFIER_LENGTH = 20
};

typedef struct
{
    const char *name;
    const char *text;
} TokenInfo;

static const TokenInfo token_info[] = {
    [T_INTEGER] = {"INTEGER", "integer"},
    [T_REAL] = {"REAL", "real"},
    [T_BOOLEAN] = {"BOOLEAN", "boolean"},
    [T_OF] = {"OF", "of"},
    [T_ARRAY] = {"ARRAY", "array"},
    [T_START] = {"START", "start"},
    [T_END] = {"END", "end"},
    [T_DECLARE] = {"DECLARE", "declare"},
    [T_MODULE] = {"MODULE", "module"},
    [T_DRIVER] = {"DRIVER", "driver"},
    [T_PROGRAM] = {"PROGRAM", "program"},
    [T_GET_VALUE] = {"GET_VALUE", "get_value"},
    [T_PRINT] = {"PRINT", "print"},
    [T_USE] = {"USE", "use"},
    [T_WITH] = {"WITH", "with"},
    [T_PARAMETERS] = {"PARAMETERS", "parameters"},
    [T_TAKES] = {"TAKES", "takes"},
    [T_INPUT] = {"INPUT", "input"},
    [T_RETURNS] = {"RETURNS", "returns"},
    [T_FOR] = {"FOR", "for"},
    [T_IN] = {"IN", "in"},
    [T_SWITCH] = {"SWITCH", "switch"},
    [T_CASE] = {"CASE", "case"},
    [T_BREAK] = {"BREAK", "break"},
    [T_DEFAULT] = {"DEFAULT", "default"},
    [T_WHILE] = {"WHILE", "while"},
    [T_TRUE] = {"TRUE", "true"},
    [T_FALSE] = {"FALSE", "false"},
    [T_AND] = {"AND", "AND"},
    [T_OR] = {"OR", "OR"},
    [T_ID] = {"ID", NULL},
    [T_NUM] = {"NUM", NULL},
    [T_RNUM] = {"RNUM", NULL},
    [T_PLUS] = {"PLUS", "+"},
    [T_MINUS] = {"MINUS", "-"},
    [T_MUL] = {"MUL", "*"},
    [T_DIV] = {"DIV", "/"},
    [T_LT] = {"LT", "<"},
    [T_LE] = {"LE", "<="},
    [T_GT] = {"GT", ">"},
    [T_GE] = {"GE", ">="},
    [T_EQ] = {"EQ", "=="},
    [T_NE] = {"NE", "!="},
    [T_DEF] = {"DEF", "<<"},
    [T_ENDDEF] = {"ENDDEF", ">>"},
    [T_DRIVERDEF] = {"DRIVERDEF", "<<<"},
    [T_DRIVERENDDEF] = {"DRIVERENDDEF", ">>>"},
    [T_COLON] = {"COLON", ":"},
    [T_RANGEOP] = {"RANGEOP", ".."},
    [T_SEMICOL] = {"SEMICOL", ";"},
    [T_COMMA] = {"COMMA", ","},
    [T_ASSIGNOP] = {"ASSIGNOP", ":="},
    [T_SQBO] = {"SQBO", "["},
    [T_SQBC] = {"SQBC", "]"},
    [T_BO] = {"BO", "("},
    [T_BC] = {"BC", ")"},
    [T_EOF] = {"EOF", NULL},
    [T_LEXERROR] = {"LEXICAL_ERROR", NULL},
};

const char *token_name(TokenKind kind)
{
    return token_info[kind].name;
}

const char *token_text(TokenKind kind)
{
    return token_info[kind].text;
}

static unsigned keyword_hash(const char *word)
{
    unsigned hash = 0;
    for (; *word != '\0'; word++)
        hash = 31 * hash + (unsigned char)*word;
    return hash % KEYWORD_TABLE_SIZE;
}

static void build_keyword_table(Lexer *lx)
{
    for (int i = 0; i < KEYWORD_TABLE_SIZE; i++)
        lx->keywords[i].word = NULL;
    for (int kind = T_INTEGER; kind <= T_OR; kind++)
    {
        const char *word = token_text(kind);
        unsigned slot = keyword_hash(word);
        while (lx->keywords[slot].word != NULL)
            slot = (slot + 1) % KEYWORD_TABLE_SIZE;
        lx->keywords[slot].word = word;
        lx->keywords[slot].kind = kind;
    }
}

static TokenKind word_kind(const Lexer *lx, const char *word)
{
    unsigned slot = keyword_hash(word);
    while (lx->keywords[slot].word != NULL)
    {
        if (strcmp(lx->keywords[slot].word, word) == 0)
            return lx->keywords[slot].kind;
        slot = (slot + 1) % KEYWORD_TABLE_SIZE;
    }
    return T_ID;
}

bool open_lexer(Lexer *lx, const char *path, int buffer_size)
{
    lx->file = fopen(path, "rb");
    if (lx->file == NULL)
        return false;
    lx->buffer_size = buffer_size;
    lx->buffer = malloc(2 * (size_t)buffer_size);
    lx->lexeme_capacity = 64;
    lx->lexeme = malloc(lx->lexeme_capacity);
    lx->lexeme_length = 0;
    lx->loaded = -1;
    lx->forward = 0;
    lx->line = 1;
    lx->last_token_line = 1;
    build_keyword_table(lx);
    return true;
}

void close_lexer(Lexer *lx)
{
    fclose(lx->file);
    free(lx->buffer);
    free(lx->lexeme);
}

static void get_stream(Lexer *lx, int half)
{
    lx->count[half] = (int)fread(lx->buffer + half * lx->buffer_size, 1, lx->buffer_size, lx->file);
    lx->loaded = half;
}

static void append_char(Lexer *lx, char c)
{
    if (lx->lexeme_length + 1 == lx->lexeme_capacity)
    {
        lx->lexeme_capacity *= 2;
        lx->lexeme = realloc(lx->lexeme, lx->lexeme_capacity);
    }
    lx->lexeme[lx->lexeme_length] = c;
    lx->lexeme_length++;
}

static int read_char(Lexer *lx)
{
    int half = lx->forward / lx->buffer_size;
    int position = lx->forward % lx->buffer_size;
    if (position == 0 && half != lx->loaded)
        get_stream(lx, half);
    lx->forward = (lx->forward + 1) % (2 * lx->buffer_size);
    if (position >= lx->count[half])
        return EOF;
    return (unsigned char)lx->buffer[half * lx->buffer_size + position];
}

static int next_char(Lexer *lx)
{
    int c = read_char(lx);
    if (c != EOF)
        append_char(lx, (char)c);
    return c;
}

static void retract(Lexer *lx, int c)
{
    lx->forward = (lx->forward + 2 * lx->buffer_size - 1) % (2 * lx->buffer_size);
    if (c != EOF)
        lx->lexeme_length--;
}

static Token make_token(Lexer *lx, TokenKind kind, int line)
{
    Token token = {0};
    token.kind = kind;
    token.line = line;
    lx->lexeme[lx->lexeme_length] = '\0';
    token.lexeme = lx->lexeme;
    if (kind != T_LEXERROR)
        lx->last_token_line = line;
    return token;
}

static Token error_token(Lexer *lx, int line, const char *message)
{
    Token token = make_token(lx, T_LEXERROR, line);
    token.message = message;
    return token;
}

static bool is_letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static bool skip_comment(Lexer *lx)
{
    int c = read_char(lx);
    while (c != EOF)
    {
        if (c == '\n')
            lx->line++;
        if (c == '*')
        {
            c = read_char(lx);
            if (c == '*')
                return true;
        }
        else
        {
            c = read_char(lx);
        }
    }
    return false;
}

static Token scan_word(Lexer *lx)
{
    int c = next_char(lx);
    while (is_letter(c) || is_digit(c))
        c = next_char(lx);
    retract(lx, c);
    if (lx->lexeme_length > MAX_IDENTIFIER_LENGTH)
        return error_token(lx, lx->line, "is an identifier longer than 20 characters");
    Token token = make_token(lx, T_ID, lx->line);
    token.kind = word_kind(lx, token.lexeme);
    return token;
}

static Token integer_token(Lexer *lx)
{
    Token token = make_token(lx, T_NUM, lx->line);
    errno = 0;
    token.value.num = strtoll(token.lexeme, NULL, 10);
    if (errno == ERANGE)
        return error_token(lx, lx->line, "is too large for an integer (at most 9223372036854775807)");
    return token;
}

static Token scan_number(Lexer *lx)
{
    int c = next_char(lx);
    while (is_digit(c))
        c = next_char(lx);
    if (c != '.')
    {
        retract(lx, c);
        return integer_token(lx);
    }
    c = next_char(lx);
    if (c == '.')
    {
        retract(lx, c);
        retract(lx, '.');
        return integer_token(lx);
    }
    if (!is_digit(c))
    {
        retract(lx, c);
        return error_token(lx, lx->line, "is a malformed number: no digit after the decimal point");
    }
    while (is_digit(c))
        c = next_char(lx);
    if (c == 'e' || c == 'E')
    {
        c = next_char(lx);
        if (c == '+' || c == '-')
            c = next_char(lx);
        if (!is_digit(c))
        {
            retract(lx, c);
            return error_token(lx, lx->line, "is a malformed number: no digit in the exponent");
        }
        while (is_digit(c))
            c = next_char(lx);
    }
    retract(lx, c);
    Token token = make_token(lx, T_RNUM, lx->line);
    token.value.rnum = strtod(token.lexeme, NULL);
    if (isinf(token.value.rnum))
        return error_token(lx, lx->line, "is too large for a real number (at most about 1.8E+308)");
    return token;
}

static Token scan_symbol(Lexer *lx, int c)
{
    int next;
    switch (c)
    {
    case '+':
        return make_token(lx, T_PLUS, lx->line);
    case '-':
        return make_token(lx, T_MINUS, lx->line);
    case '/':
        return make_token(lx, T_DIV, lx->line);
    case ';':
        return make_token(lx, T_SEMICOL, lx->line);
    case ',':
        return make_token(lx, T_COMMA, lx->line);
    case '[':
        return make_token(lx, T_SQBO, lx->line);
    case ']':
        return make_token(lx, T_SQBC, lx->line);
    case '(':
        return make_token(lx, T_BO, lx->line);
    case ')':
        return make_token(lx, T_BC, lx->line);
    case '<':
        next = next_char(lx);
        if (next == '=')
            return make_token(lx, T_LE, lx->line);
        if (next != '<')
        {
            retract(lx, next);
            return make_token(lx, T_LT, lx->line);
        }
        next = next_char(lx);
        if (next == '<')
            return make_token(lx, T_DRIVERDEF, lx->line);
        retract(lx, next);
        return make_token(lx, T_DEF, lx->line);
    case '>':
        next = next_char(lx);
        if (next == '=')
            return make_token(lx, T_GE, lx->line);
        if (next != '>')
        {
            retract(lx, next);
            return make_token(lx, T_GT, lx->line);
        }
        next = next_char(lx);
        if (next == '>')
            return make_token(lx, T_DRIVERENDDEF, lx->line);
        retract(lx, next);
        return make_token(lx, T_ENDDEF, lx->line);
    case ':':
        next = next_char(lx);
        if (next == '=')
            return make_token(lx, T_ASSIGNOP, lx->line);
        retract(lx, next);
        return make_token(lx, T_COLON, lx->line);
    case '=':
        next = next_char(lx);
        if (next == '=')
            return make_token(lx, T_EQ, lx->line);
        retract(lx, next);
        return error_token(lx, lx->line, "is not a token (assignment is ':=', comparison is '==')");
    case '!':
        next = next_char(lx);
        if (next == '=')
            return make_token(lx, T_NE, lx->line);
        retract(lx, next);
        return error_token(lx, lx->line, "is not a token (inequality is '!=')");
    case '.':
        next = next_char(lx);
        if (next == '.')
            return make_token(lx, T_RANGEOP, lx->line);
        retract(lx, next);
        return error_token(lx, lx->line, "is not a token (the range operator is '..')");
    default:
        return error_token(lx, lx->line, "cannot begin a token");
    }
}

Token get_next_token(Lexer *lx)
{
    for (;;)
    {
        lx->lexeme_length = 0;
        int c = next_char(lx);
        if (c == EOF)
        {
            retract(lx, c);
            return make_token(lx, T_EOF, lx->last_token_line);
        }
        if (c == '\n')
        {
            lx->line++;
        }
        else if (c == ' ' || c == '\t' || c == '\r')
        {
        }
        else if (is_letter(c))
        {
            retract(lx, c);
            return scan_word(lx);
        }
        else if (is_digit(c))
        {
            retract(lx, c);
            return scan_number(lx);
        }
        else if (c == '*')
        {
            int next = next_char(lx);
            if (next != '*')
            {
                retract(lx, next);
                return make_token(lx, T_MUL, lx->line);
            }
            int line = lx->line;
            if (!skip_comment(lx))
                return error_token(lx, line, "opens a comment that is never closed");
        }
        else
        {
            return scan_symbol(lx, c);
        }
    }
}

bool remove_comments(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    bool comment = false;
    int c;
    while ((c = fgetc(file)) != EOF)
    {
        if (c == '*')
        {
            int next = fgetc(file);
            if (next == '*')
            {
                comment = !comment;
                continue;
            }
            if (!comment)
                putchar('*');
            if (next == EOF)
                break;
            c = next;
        }
        if (!comment || c == '\n')
            putchar(c);
    }
    fclose(file);
    return true;
}

bool print_token_list(const char *path, int buffer_size)
{
    Lexer lexer;
    if (!open_lexer(&lexer, path, buffer_size))
        return false;
    Token token = get_next_token(&lexer);
    while (token.kind != T_EOF)
    {
        if (token.kind == T_LEXERROR)
            printf("%d %s %s %s\n", token.line, token.lexeme, token_name(token.kind), token.message);
        else
            printf("%d %s %s\n", token.line, token.lexeme, token_name(token.kind));
        token = get_next_token(&lexer);
    }
    close_lexer(&lexer);
    return true;
}
