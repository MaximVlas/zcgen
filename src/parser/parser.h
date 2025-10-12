#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include <stdbool.h>
#include "../common/types.h"
#include "../syntax/syntax.h"
#include "../lexer/lexer.h"

/* Parser state */
typedef struct Parser {
    TokenList *tokens;
    size_t position;
    Token *current;
    
    SyntaxDefinition *syntax;
    
    /* Error recovery */
    bool panic_mode;
    int error_count;
} Parser;

/* Create and destroy parser */
Parser *parser_create(TokenList *tokens, SyntaxDefinition *syntax);
void parser_destroy(Parser *parser);

/* Parse entire program */
ASTNode *parser_parse(Parser *parser);

/* Parser utilities */
Token *parser_current(Parser *parser);
Token *parser_peek(Parser *parser, int offset);
bool parser_match(Parser *parser, TokenType type);
bool parser_check(Parser *parser, TokenType type);
Token *parser_advance(Parser *parser);
Token *parser_expect(Parser *parser, TokenType type, const char *message);
bool parser_at_end(Parser *parser);

/* Error handling */
void parser_error(Parser *parser, const char *message);
void parser_error_at_current(Parser *parser, const char *message);
void parser_synchronize(Parser *parser);

#endif /* PARSER_H */
