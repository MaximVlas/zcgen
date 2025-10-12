#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../common/types.h"
#include "../syntax/syntax.h"

/* Lexer state */
typedef struct Lexer {
    const char *source;
    const char *filename;
    size_t length;
    size_t position;
    
    /* Current location */
    uint32_t line;
    uint32_t column;
    
    /* Syntax definition */
    SyntaxDefinition *syntax;
    
    /* Token list */
    TokenList *tokens;
} Lexer;

/* Create and destroy lexer */
Lexer *lexer_create(const char *source, const char *filename, SyntaxDefinition *syntax);
void lexer_destroy(Lexer *lexer);

/* Tokenize entire source */
TokenList *lexer_tokenize(Lexer *lexer);

/* Token list operations */
TokenList *token_list_create(void);
void token_list_destroy(TokenList *list);
void token_list_append(TokenList *list, Token *token);
Token *token_list_get(TokenList *list, size_t index);

/* Token operations */
Token *token_create(TokenType type, const char *lexeme, size_t length, SourceLocation loc);
void token_destroy(Token *token);
const char *token_type_name(TokenType type);

#endif /* LEXER_H */
