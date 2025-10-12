#ifndef C_SYNTAX_H
#define C_SYNTAX_H

#include <stddef.h>
#include <stdbool.h>
#include "../common/types.h"
#include "syntax.h"

/* C99 Token types - specific mappings */
typedef enum {
    /* ===== KEYWORDS ===== */
    /* C89/C90 Keywords */
    TOKEN_AUTO = TOKEN_KEYWORD_START,
    TOKEN_BREAK,
    TOKEN_CASE,
    TOKEN_CHAR,
    TOKEN_CONST,
    TOKEN_CONTINUE,
    TOKEN_DEFAULT,
    TOKEN_DO,
    TOKEN_DOUBLE,
    TOKEN_ELSE,
    TOKEN_ENUM,
    TOKEN_EXTERN,
    TOKEN_FLOAT,
    TOKEN_FOR,
    TOKEN_GOTO,
    TOKEN_IF,
    TOKEN_INT,
    TOKEN_LONG,
    TOKEN_REGISTER,
    TOKEN_RETURN,
    TOKEN_SHORT,
    TOKEN_SIGNED,
    TOKEN_SIZEOF,
    TOKEN_STATIC,
    TOKEN_STRUCT,
    TOKEN_SWITCH,
    TOKEN_TYPEDEF,
    TOKEN_UNION,
    TOKEN_UNSIGNED,
    TOKEN_VOID,
    TOKEN_VOLATILE,
    TOKEN_WHILE,
    
    /* C99 Keywords */
    TOKEN_INLINE,
    TOKEN_RESTRICT,
    TOKEN__BOOL,
    TOKEN__COMPLEX,
    TOKEN__IMAGINARY,
    
    /* C11 Keywords */
    TOKEN__ALIGNAS,
    TOKEN__ALIGNOF,
    TOKEN__ATOMIC,
    TOKEN__GENERIC,
    TOKEN__NORETURN,
    TOKEN__STATIC_ASSERT,
    TOKEN__THREAD_LOCAL,
    
    /* C23 Keywords */
    TOKEN__BITINT,
    TOKEN__DECIMAL128,
    TOKEN__DECIMAL32,
    TOKEN__DECIMAL64,
    TOKEN_TYPEOF,
    TOKEN_TYPEOF_UNQUAL,
    TOKEN__BITINT_MAXWIDTH,
    
    /* GCC/Clang extensions */
    TOKEN___TYPEOF__,
    TOKEN___INLINE__,
    TOKEN___CONST__,
    TOKEN___VOLATILE__,
    TOKEN___RESTRICT__,
    TOKEN___ATTRIBUTE__,
    TOKEN___EXTENSION__,
    TOKEN___ASM__,
    TOKEN___SIGNED__,
    TOKEN___UNSIGNED__,
    TOKEN___COMPLEX__,
    TOKEN___IMAG__,
    TOKEN___REAL__,
    TOKEN___LABEL__,
    TOKEN___ALIGNOF__,
    TOKEN___BUILTIN_VA_ARG,
    TOKEN___BUILTIN_OFFSETOF,
    TOKEN___BUILTIN_TYPES_COMPATIBLE_P,
    
    /* ===== OPERATORS ===== */
    TOKEN_PLUS = TOKEN_OPERATOR_START,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDE,
    TOKEN_EXCLAIM,
    TOKEN_QUESTION,
    TOKEN_COLON,
    TOKEN_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    
    /* Compound assignment operators */
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL,
    TOKEN_AMPERSAND_EQUAL,
    TOKEN_PIPE_EQUAL,
    TOKEN_CARET_EQUAL,
    TOKEN_LESS_LESS_EQUAL,
    TOKEN_GREATER_GREATER_EQUAL,
    
    /* Comparison operators */
    TOKEN_EQUAL_EQUAL,
    TOKEN_EXCLAIM_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    
    /* Logical operators */
    TOKEN_AMPERSAND_AMPERSAND,
    TOKEN_PIPE_PIPE,
    
    /* Shift operators */
    TOKEN_LESS_LESS,
    TOKEN_GREATER_GREATER,
    
    /* Increment/decrement */
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    
    /* Member access */
    TOKEN_ARROW,
    TOKEN_DOT,
    
    /* ===== PUNCTUATION ===== */
    TOKEN_LPAREN = TOKEN_PUNCTUATION_START,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_ELLIPSIS,
    TOKEN_HASH,          /* # for preprocessor */
    TOKEN_HASH_HASH,     /* ## for token pasting */
    TOKEN_AT,            /* @ (future use) */
    TOKEN_DOLLAR,        /* $ (future use) */
    TOKEN_BACKTICK,      /* ` (future use) */
} CTokenType;

/* Create C99 syntax definition */
SyntaxDefinition *syntax_c99_create(void);

/* Destroy C99 syntax definition */
void syntax_c99_destroy(SyntaxDefinition *syntax);

#endif /* C_SYNTAX_H */
