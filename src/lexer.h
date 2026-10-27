#ifndef LEXER_H
#define LEXER_H

#include "lexerDef.h"

const char *token_name(TokenKind kind);
const char *token_text(TokenKind kind);

bool open_lexer(Lexer *lexer, const char *path, int buffer_size);
Token get_next_token(Lexer *lexer);
void close_lexer(Lexer *lexer);

bool remove_comments(const char *path);
bool print_token_list(const char *path, int buffer_size);

#endif
