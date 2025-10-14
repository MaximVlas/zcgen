#include "lexer.h"
#include "../common/memory.h"
#include "../common/error.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

/* Helper macros */
#define CURRENT(lexer) ((lexer)->source[(lexer)->position])
#define PEEK(lexer, n) ((lexer)->position + (n) < (lexer)->length ? \
                        (lexer)->source[(lexer)->position + (n)] : '\0')
#define AT_END(lexer) ((lexer)->position >= (lexer)->length)
#define ADVANCE(lexer) ((lexer)->position++, (lexer)->column++)

/* Create lexer */
Lexer *lexer_create(const char *source, const char *filename, SyntaxDefinition *syntax) {
    Lexer *lexer = xcalloc(1, sizeof(Lexer));
    lexer->source = source;
    lexer->filename = filename;
    lexer->length = strlen(source);
    lexer->position = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->syntax = syntax;
    lexer->tokens = token_list_create();
    return lexer;
}

void lexer_destroy(Lexer *lexer) {
    if (lexer) {
        token_list_destroy(lexer->tokens);
        xfree(lexer);
    }
}

/* Get current location */
static SourceLocation lexer_location(Lexer *lexer) {
    SourceLocation loc = {
        .filename = lexer->filename,
        .line = lexer->line,
        .column = lexer->column,
        .offset = lexer->position
    };
    return loc;
}

/* Skip whitespace */
static void skip_whitespace(Lexer *lexer) {
    while (!AT_END(lexer) && lexer->syntax->is_whitespace(CURRENT(lexer))) {
        if (CURRENT(lexer) == '\n') {
            lexer->line++;
            lexer->column = 0;
        }
        ADVANCE(lexer);
    }
}

/* Skip single-line comment */
static bool skip_single_line_comment(Lexer *lexer) {
    const char *start = lexer->syntax->comment_style.single_line_start;
    if (!start) return false;
    
    size_t len = strlen(start);
    if (lexer->position + len > lexer->length) return false;
    
    if (strncmp(&lexer->source[lexer->position], start, len) == 0) {
        lexer->position += len;
        lexer->column += len;
        
        while (!AT_END(lexer) && CURRENT(lexer) != '\n') {
            ADVANCE(lexer);
        }
        return true;
    }
    return false;
}

/* Skip multi-line comment */
static bool skip_multi_line_comment(Lexer *lexer) {
    const char *start = lexer->syntax->comment_style.multi_line_start;
    const char *end = lexer->syntax->comment_style.multi_line_end;
    if (!start || !end) return false;
    
    size_t start_len = strlen(start);
    if (lexer->position + start_len > lexer->length) return false;
    
    if (strncmp(&lexer->source[lexer->position], start, start_len) == 0) {
        lexer->position += start_len;
        lexer->column += start_len;
        
        size_t end_len = strlen(end);
        while (!AT_END(lexer)) {
            if (lexer->position + end_len <= lexer->length &&
                strncmp(&lexer->source[lexer->position], end, end_len) == 0) {
                lexer->position += end_len;
                lexer->column += end_len;
                return true;
            }
            if (CURRENT(lexer) == '\n') {
                lexer->line++;
                lexer->column = 0;
            }
            ADVANCE(lexer);
        }
        
        SourceLocation loc = lexer_location(lexer);
        error_report(ERROR_LEXER, loc, "unterminated comment");
        return true;
    }
    return false;
}

/* Skip preprocessor line markers like "# 123 "file.c"" */
static bool skip_preprocessor_line_marker(Lexer *lexer) {
    if (CURRENT(lexer) != '#') return false;
    
    /* Check if we're at the start of a line (or after whitespace) */
    size_t pos = lexer->position;
    if (pos > 0) {
        /* Look back to see if there's only whitespace before # */
        size_t check_pos = pos - 1;
        while (check_pos > 0 && lexer->source[check_pos] != '\n') {
            if (!isspace(lexer->source[check_pos])) {
                return false; /* Not a line marker */
            }
            check_pos--;
        }
    }
    
    /* Look ahead to see if it's # followed by digit (line marker) */
    if (pos + 1 < lexer->length && isdigit(lexer->source[pos + 1])) {
        /* Skip entire line */
        while (!AT_END(lexer) && CURRENT(lexer) != '\n') {
            ADVANCE(lexer);
        }
        if (CURRENT(lexer) == '\n') {
            lexer->line++;
            lexer->column = 0;
            ADVANCE(lexer);
        }
        return true;
    }
    
    return false;
}

/* Skip comments and whitespace */
static void skip_trivia(Lexer *lexer) {
    while (!AT_END(lexer)) {
        if (lexer->syntax->is_whitespace(CURRENT(lexer))) {
            skip_whitespace(lexer);
        } else if (skip_preprocessor_line_marker(lexer)) {
            continue;
        } else if (skip_single_line_comment(lexer)) {
            continue;
        } else if (skip_multi_line_comment(lexer)) {
            continue;
        } else {
            break;
        }
    }
}

/* Lex identifier or keyword */
static Token *lex_identifier(Lexer *lexer) {
    SourceLocation loc = lexer_location(lexer);
    size_t start = lexer->position;
    
    while (!AT_END(lexer) && lexer->syntax->is_identifier_continue(CURRENT(lexer))) {
        ADVANCE(lexer);
    }
    
    size_t length = lexer->position - start;
    char *lexeme = xstrndup(&lexer->source[start], length);
    
    /* Check if it's a keyword */
    for (size_t i = 0; i < lexer->syntax->keyword_count; i++) {
        if (strcmp(lexeme, lexer->syntax->keywords[i].name) == 0) {
            Token *token = token_create(lexer->syntax->keywords[i].token_type,
                                       lexeme, length, loc);
            xfree(lexeme);
            return token;
        }
    }
    
    /* It's an identifier */
    Token *token = token_create(TOKEN_IDENTIFIER, lexeme, length, loc);
    xfree(lexeme);
    return token;
}

/* Lex number literal */
static Token *lex_number(Lexer *lexer) {
    SourceLocation loc = lexer_location(lexer);
    size_t start = lexer->position;
    bool is_float = false;
    
    /* Check for hex, octal, binary prefix */
    if (CURRENT(lexer) == '0') {
        ADVANCE(lexer);
        
        if (!AT_END(lexer)) {
            char next = CURRENT(lexer);
            if ((next == 'x' || next == 'X') && lexer->syntax->supports_hex) {
                ADVANCE(lexer);
                while (!AT_END(lexer) && isxdigit(CURRENT(lexer))) {
                    ADVANCE(lexer);
                }
            } else if ((next == 'b' || next == 'B') && lexer->syntax->supports_binary) {
                ADVANCE(lexer);
                while (!AT_END(lexer) && (CURRENT(lexer) == '0' || CURRENT(lexer) == '1')) {
                    ADVANCE(lexer);
                }
            } else if (isdigit(next) && lexer->syntax->supports_octal) {
                while (!AT_END(lexer) && CURRENT(lexer) >= '0' && CURRENT(lexer) <= '7') {
                    ADVANCE(lexer);
                }
            }
        }
    } else {
        /* Decimal number */
        while (!AT_END(lexer) && lexer->syntax->is_digit(CURRENT(lexer))) {
            ADVANCE(lexer);
        }
    }
    
    /* Check for decimal point */
    if (!AT_END(lexer) && CURRENT(lexer) == '.' && lexer->syntax->supports_float) {
        is_float = true;
        ADVANCE(lexer);
        while (!AT_END(lexer) && lexer->syntax->is_digit(CURRENT(lexer))) {
            ADVANCE(lexer);
        }
    }
    
    /* Check for exponent */
    if (!AT_END(lexer) && (CURRENT(lexer) == 'e' || CURRENT(lexer) == 'E') &&
        lexer->syntax->supports_scientific) {
        is_float = true;
        ADVANCE(lexer);
        if (!AT_END(lexer) && (CURRENT(lexer) == '+' || CURRENT(lexer) == '-')) {
            ADVANCE(lexer);
        }
        while (!AT_END(lexer) && lexer->syntax->is_digit(CURRENT(lexer))) {
            ADVANCE(lexer);
        }
    }
    
    /* Check for suffix (f, l, u, etc.) */
    while (!AT_END(lexer) && (CURRENT(lexer) == 'f' || CURRENT(lexer) == 'F' ||
                              CURRENT(lexer) == 'l' || CURRENT(lexer) == 'L' ||
                              CURRENT(lexer) == 'u' || CURRENT(lexer) == 'U')) {
        ADVANCE(lexer);
    }
    
    size_t length = lexer->position - start;
    char *lexeme = xstrndup(&lexer->source[start], length);
    
    Token *token = token_create(is_float ? TOKEN_FLOAT_LITERAL : TOKEN_INTEGER_LITERAL,
                               lexeme, length, loc);
    
    /* Parse value */
    if (is_float) {
        token->value.float_value = strtod(lexeme, NULL);
    } else {
        token->value.int_value = strtoll(lexeme, NULL, 0);
    }
    
    xfree(lexeme);
    return token;
}

/* Lex string literal */
static Token *lex_string(Lexer *lexer) {
    SourceLocation loc = lexer_location(lexer);
    size_t start = lexer->position;
    
    ADVANCE(lexer); /* Skip opening quote */
    
    while (!AT_END(lexer) && CURRENT(lexer) != lexer->syntax->string_delimiter) {
        if (CURRENT(lexer) == lexer->syntax->escape_char) {
            ADVANCE(lexer);
            if (!AT_END(lexer)) {
                ADVANCE(lexer);
            }
        } else {
            if (CURRENT(lexer) == '\n') {
                lexer->line++;
                lexer->column = 0;
            }
            ADVANCE(lexer);
        }
    }
    
    if (AT_END(lexer)) {
        error_report(ERROR_LEXER, loc, "unterminated string literal");
        return token_create(TOKEN_ERROR, "", 0, loc);
    }
    
    ADVANCE(lexer); /* Skip closing quote */
    
    /* Extract string content without quotes */
    size_t content_start = start + 1;  /* Skip opening quote */
    size_t content_length = (lexer->position - 1) - content_start;  /* Exclude closing quote */
    
    /* Process escape sequences */
    char *raw_content = xstrndup(&lexer->source[content_start], content_length);
    char *processed_content = xmalloc(content_length + 1);
    size_t j = 0;
    
    for (size_t i = 0; i < content_length; i++) {
        if (raw_content[i] == '\\' && i + 1 < content_length) {
            i++; /* Skip backslash */
            switch (raw_content[i]) {
                case 'n': processed_content[j++] = '\n'; break;
                case 't': processed_content[j++] = '\t'; break;
                case 'r': processed_content[j++] = '\r'; break;
                case '\\': processed_content[j++] = '\\'; break;
                case '"': processed_content[j++] = '"'; break;
                case '0': processed_content[j++] = '\0'; break;
                default: 
                    /* Unknown escape - keep as is */
                    processed_content[j++] = '\\';
                    processed_content[j++] = raw_content[i];
                    break;
            }
        } else {
            processed_content[j++] = raw_content[i];
        }
    }
    processed_content[j] = '\0';
    xfree(raw_content);
    
    /* Create lexeme with quotes for display */
    size_t lexeme_length = lexer->position - start;
    char *lexeme = xstrndup(&lexer->source[start], lexeme_length);
    
    Token *token = token_create(TOKEN_STRING_LITERAL, lexeme, lexeme_length, loc);
    
    /* Store the processed string content in the token */
    if (token->value.string_value) {
        xfree(token->value.string_value);
    }
    token->value.string_value = processed_content;
    
    xfree(lexeme);
    return token;
}

/* Lex char literal */
static Token *lex_char(Lexer *lexer) {
    SourceLocation loc = lexer_location(lexer);
    size_t start = lexer->position;
    
    ADVANCE(lexer); /* Skip opening quote */
    
    if (AT_END(lexer)) {
        error_report(ERROR_LEXER, loc, "unterminated character literal");
        return token_create(TOKEN_ERROR, "", 0, loc);
    }
    
    char value = CURRENT(lexer);
    if (CURRENT(lexer) == lexer->syntax->escape_char) {
        ADVANCE(lexer);
        if (!AT_END(lexer)) {
            value = CURRENT(lexer);
            /* Handle escape sequences */
            switch (value) {
                case 'n': value = '\n'; break;
                case 't': value = '\t'; break;
                case 'r': value = '\r'; break;
                case '0': value = '\0'; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
            }
            ADVANCE(lexer);
        }
    } else {
        ADVANCE(lexer);
    }
    
    if (AT_END(lexer) || CURRENT(lexer) != lexer->syntax->char_delimiter) {
        error_report(ERROR_LEXER, loc, "unterminated character literal");
        return token_create(TOKEN_ERROR, "", 0, loc);
    }
    
    ADVANCE(lexer); /* Skip closing quote */
    
    size_t length = lexer->position - start;
    char *lexeme = xstrndup(&lexer->source[start], length);
    
    Token *token = token_create(TOKEN_CHAR_LITERAL, lexeme, length, loc);
    token->value.char_value = value;
    xfree(lexeme);
    return token;
}

/* Lex operator or punctuation */
static Token *lex_operator_or_punct(Lexer *lexer) {
    SourceLocation loc = lexer_location(lexer);
    
    /* Try punctuation first (longest match) */
    for (size_t i = 0; i < lexer->syntax->punctuation_count; i++) {
        const char *symbol = lexer->syntax->punctuation[i].symbol;
        size_t len = strlen(symbol);
        
        if (lexer->position + len <= lexer->length &&
            strncmp(&lexer->source[lexer->position], symbol, len) == 0) {
            char *lexeme = xstrndup(symbol, len);
            Token *token = token_create(lexer->syntax->punctuation[i].token_type,
                                       lexeme, len, loc);
            lexer->position += len;
            lexer->column += len;
            xfree(lexeme);
            return token;
        }
    }
    
    /* Try operators */
    for (size_t i = 0; i < lexer->syntax->operator_count; i++) {
        const char *symbol = lexer->syntax->operators[i].symbol;
        size_t len = strlen(symbol);
        
        if (lexer->position + len <= lexer->length &&
            strncmp(&lexer->source[lexer->position], symbol, len) == 0) {
            char *lexeme = xstrndup(symbol, len);
            Token *token = token_create(lexer->syntax->operators[i].token_type,
                                       lexeme, len, loc);
            lexer->position += len;
            lexer->column += len;
            xfree(lexeme);
            return token;
        }
    }
    
    /* Unknown character */
    char unknown[2] = {CURRENT(lexer), '\0'};
    error_report(ERROR_LEXER, loc, "unexpected character '%c'", CURRENT(lexer));
    ADVANCE(lexer);
    return token_create(TOKEN_ERROR, unknown, 1, loc);
}

/* Tokenize entire source */
TokenList *lexer_tokenize(Lexer *lexer) {
    while (!AT_END(lexer)) {
        skip_trivia(lexer);
        
        if (AT_END(lexer)) break;
        
        Token *token = NULL;
        char current = CURRENT(lexer);
        
        if (lexer->syntax->is_identifier_start(current)) {
            token = lex_identifier(lexer);
        } else if (lexer->syntax->is_digit(current)) {
            token = lex_number(lexer);
        } else if (current == lexer->syntax->string_delimiter) {
            token = lex_string(lexer);
        } else if (current == lexer->syntax->char_delimiter) {
            token = lex_char(lexer);
        } else {
            token = lex_operator_or_punct(lexer);
        }
        
        if (token) {
            token_list_append(lexer->tokens, token);
        }
    }
    
    /* Add EOF token */
    SourceLocation loc = lexer_location(lexer);
    Token *eof = token_create(TOKEN_EOF, "", 0, loc);
    token_list_append(lexer->tokens, eof);
    
    return lexer->tokens;
}

/* Token list operations */
TokenList *token_list_create(void) {
    return xcalloc(1, sizeof(TokenList));
}

void token_list_destroy(TokenList *list) {
    if (!list) return;
    
    Token *current = list->head;
    while (current) {
        Token *next = current->next;
        token_destroy(current);
        current = next;
    }
    xfree(list);
}

void token_list_append(TokenList *list, Token *token) {
    if (!list->head) {
        list->head = token;
        list->tail = token;
    } else {
        list->tail->next = token;
        list->tail = token;
    }
    list->count++;
}

Token *token_list_get(TokenList *list, size_t index) {
    Token *current = list->head;
    for (size_t i = 0; i < index && current; i++) {
        current = current->next;
    }
    return current;
}

/* Token operations */
Token *token_create(TokenType type, const char *lexeme, size_t length, SourceLocation loc) {
    Token *token = xcalloc(1, sizeof(Token));
    token->type = type;
    token->lexeme = xstrndup(lexeme, length);
    token->length = length;
    token->location = loc;
    token->next = NULL;
    return token;
}

void token_destroy(Token *token) {
    if (token) {
        xfree(token->lexeme);
        if (token->type == TOKEN_STRING_LITERAL && token->value.string_value) {
            xfree(token->value.string_value);
        }
        xfree(token);
    }
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_INTEGER_LITERAL: return "INTEGER";
        case TOKEN_FLOAT_LITERAL: return "FLOAT";
        case TOKEN_STRING_LITERAL: return "STRING";
        case TOKEN_CHAR_LITERAL: return "CHAR";
        default: return "UNKNOWN";
    }
}
