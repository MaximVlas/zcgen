#ifndef SYNTAX_H
#define SYNTAX_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "../common/types.h"

/* Keyword definition */
typedef struct {
    const char *name;
    TokenType token_type;
} KeywordDef;

/* Operator definition */
typedef struct {
    const char *symbol;
    TokenType token_type;
    int precedence;
    enum {
        ASSOC_LEFT,
        ASSOC_RIGHT,
        ASSOC_NONE
    } associativity;
} OperatorDef;

/* Punctuation definition */
typedef struct {
    const char *symbol;
    TokenType token_type;
} PunctuationDef;

/* Grammar rule callback types - Forward declaration */
struct Parser;
typedef ASTNode* (*ParseRuleFn)(struct Parser *parser);

/* Grammar rule definition */
typedef struct {
    const char *name;
    ParseRuleFn parse_fn;
} GrammarRule;

/* Comment style definition */
typedef struct {
    const char *single_line_start;  /* e.g., "//" for C */
    const char *multi_line_start;   /* e.g., slash-star for C */
    const char *multi_line_end;     /* e.g., star-slash for C */
} CommentStyle;

/* C Standard versions */
typedef enum {
    C_STD_C89,
    C_STD_C90,      /* Same as C89 */
    C_STD_C99,
    C_STD_C11,
    C_STD_C17,      /* Bug fix release of C11 */
    C_STD_C23,
    C_STD_GNU89,    /* GNU extensions */
    C_STD_GNU99,
    C_STD_GNU11,
    C_STD_GNU17,
    C_STD_GNU23
} CStandard;

/* Syntax definition - the heart of the pluggable system */
struct SyntaxDefinition {
    const char *language_name;
    const char *version;
    CStandard c_standard;       /* For C language */
    
    /* Lexical elements */
    const KeywordDef *keywords;
    size_t keyword_count;
    
    const OperatorDef *operators;
    size_t operator_count;
    
    const PunctuationDef *punctuation;
    size_t punctuation_count;
    
    CommentStyle comment_style;
    
    /* Character classification */
    bool (*is_identifier_start)(char c);
    bool (*is_identifier_continue)(char c);
    bool (*is_digit)(char c);
    bool (*is_whitespace)(char c);
    
    /* String/char literal handling */
    char string_delimiter;      /* e.g., '"' for C */
    char char_delimiter;        /* e.g., '\'' for C */
    char escape_char;           /* e.g., '\\' for C */
    
    /* Number literal handling */
    bool supports_hex;          /* 0x prefix */
    bool supports_octal;        /* 0 prefix */
    bool supports_binary;       /* 0b prefix */
    bool supports_float;
    bool supports_scientific;   /* e.g., 1.5e10 */
    
    /* Grammar rules - top-down parsing */
    const GrammarRule *grammar_rules;
    size_t grammar_rule_count;
    
    /* Entry point for parsing */
    const char *start_rule;     /* e.g., "translation_unit" for C */
    
    /* Type system callbacks (for semantic analysis) */
    bool (*is_type_name)(const char *name);
    
    /* Language-specific features */
    bool case_sensitive;
    bool requires_semicolons;
    bool supports_preprocessor;
};

/* Get syntax definition for a language */
SyntaxDefinition *syntax_get_definition(const char *language);

/* C99 syntax definition */
SyntaxDefinition *syntax_c99_create(void);

#endif /* SYNTAX_H */
