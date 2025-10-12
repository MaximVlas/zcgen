#include "parser.h"
#include "../common/error.h"
#include "../common/memory.h"
#include "../common/debug.h"
#include "../ast/ast.h"
#include <string.h>
#include <stdio.h>

/* Stub implementation - to be completed with full C99 parser */

Parser *parser_create(TokenList *tokens, SyntaxDefinition *syntax) {
    Parser *parser = xcalloc(1, sizeof(Parser));
    parser->tokens = tokens;
    parser->position = 0;
    parser->current = tokens->head;
    parser->syntax = syntax;
    parser->panic_mode = false;
    parser->error_count = 0;
    return parser;
}

void parser_destroy(Parser *parser) {
    xfree(parser);
}

Token *parser_current(Parser *parser) {
    return parser->current;
}

Token *parser_peek(Parser *parser, int offset) {
    Token *token = parser->current;
    for (int i = 0; i < offset && token; i++) {
        token = token->next;
    }
    return token;
}

bool parser_match(Parser *parser, TokenType type) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

bool parser_check(Parser *parser, TokenType type) {
    if (parser_at_end(parser)) return false;
    return parser->current->type == type;
}

Token *parser_advance(Parser *parser) {
    if (!parser_at_end(parser)) {
        Token *prev = parser->current;
        parser->current = parser->current->next;
        parser->position++;
        return prev;
    }
    return parser->current;
}

Token *parser_expect(Parser *parser, TokenType type, const char *message) {
    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }
    
    parser_error_at_current(parser, message);
    return NULL;
}

bool parser_at_end(Parser *parser) {
    return parser->current == NULL || parser->current->type == TOKEN_EOF;
}

void parser_error(Parser *parser, const char *message) {
    if (parser->panic_mode) return;
    
    Token *token = parser->current;
    if (token) {
        error_report(ERROR_PARSER, token->location, "%s", message);
        debug_print_parser_error(stderr, token, message);
        debug_print_parser_context(stderr, token, 5);
    } else {
        fprintf(stderr, "error: %s (at EOF)\n", message);
    }
    
    parser->error_count++;
    parser->panic_mode = true;
}

void parser_error_at_current(Parser *parser, const char *message) {
    parser_error(parser, message);
}

void parser_synchronize(Parser *parser) {
    parser->panic_mode = false;
    
    /* Skip tokens until we find a synchronization point */
    while (!parser_at_end(parser)) {
        parser_advance(parser);
        /* TODO: Add proper synchronization logic */
        if (parser->current && parser->current->type == 0) {
            return;
        }
    }
}

ASTNode *parser_parse(Parser *parser) {
    /* Create translation unit */
    SourceLocation loc = {
        .filename = "<input>",
        .line = 1,
        .column = 1,
        .offset = 0
    };
    
    ASTNode *unit = ast_create_translation_unit(loc);
    
    /* Parse declarations until EOF */
    while (!parser_at_end(parser)) {
        /* TODO: Parse actual declarations */
        /* For now, just advance to avoid infinite loop */
        parser_advance(parser);
    }
    
    return unit;
}
