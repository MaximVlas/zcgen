#include "c_syntax.h"
#include "../common/memory.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Forward declarations for grammar rules */
struct Parser;
static ASTNode *parse_translation_unit(struct Parser *parser);
static ASTNode *parse_external_declaration(struct Parser *parser);
static ASTNode *parse_function_definition(struct Parser *parser);
static ASTNode *parse_declaration(struct Parser *parser);
static ASTNode *parse_statement(struct Parser *parser);
static ASTNode *parse_expression(struct Parser *parser);

/* C99+ Keywords - Complete list */
static const KeywordDef c_keywords[] = {
    /* C89/C90 */
    {"auto", TOKEN_AUTO},
    {"break", TOKEN_BREAK},
    {"case", TOKEN_CASE},
    {"char", TOKEN_CHAR},
    {"const", TOKEN_CONST},
    {"continue", TOKEN_CONTINUE},
    {"default", TOKEN_DEFAULT},
    {"do", TOKEN_DO},
    {"double", TOKEN_DOUBLE},
    {"else", TOKEN_ELSE},
    {"enum", TOKEN_ENUM},
    {"extern", TOKEN_EXTERN},
    {"float", TOKEN_FLOAT},
    {"for", TOKEN_FOR},
    {"goto", TOKEN_GOTO},
    {"if", TOKEN_IF},
    {"int", TOKEN_INT},
    {"long", TOKEN_LONG},
    {"register", TOKEN_REGISTER},
    {"return", TOKEN_RETURN},
    {"short", TOKEN_SHORT},
    {"signed", TOKEN_SIGNED},
    {"sizeof", TOKEN_SIZEOF},
    {"static", TOKEN_STATIC},
    {"struct", TOKEN_STRUCT},
    {"switch", TOKEN_SWITCH},
    {"typedef", TOKEN_TYPEDEF},
    {"union", TOKEN_UNION},
    {"unsigned", TOKEN_UNSIGNED},
    {"void", TOKEN_VOID},
    {"volatile", TOKEN_VOLATILE},
    {"while", TOKEN_WHILE},
    
    /* C99 */
    {"inline", TOKEN_INLINE},
    {"restrict", TOKEN_RESTRICT},
    {"_Bool", TOKEN__BOOL},
    {"_Complex", TOKEN__COMPLEX},
    {"_Imaginary", TOKEN__IMAGINARY},
    
    /* C11 */
    {"_Alignas", TOKEN__ALIGNAS},
    {"_Alignof", TOKEN__ALIGNOF},
    {"_Atomic", TOKEN__ATOMIC},
    {"_Generic", TOKEN__GENERIC},
    {"_Noreturn", TOKEN__NORETURN},
    {"_Static_assert", TOKEN__STATIC_ASSERT},
    {"_Thread_local", TOKEN__THREAD_LOCAL},
    
    /* C23 */
    {"_BitInt", TOKEN__BITINT},
    {"_Decimal128", TOKEN__DECIMAL128},
    {"_Decimal32", TOKEN__DECIMAL32},
    {"_Decimal64", TOKEN__DECIMAL64},
    {"typeof", TOKEN_TYPEOF},
    {"typeof_unqual", TOKEN_TYPEOF_UNQUAL},
    
    /* GNU Extensions */
    {"__attribute__", TOKEN___ATTRIBUTE__},
    {"__extension__", TOKEN___EXTENSION__},
    {"__asm__", TOKEN___ASM__},
    {"__typeof__", TOKEN___TYPEOF__},
    {"__inline__", TOKEN___INLINE__},
    {"__restrict__", TOKEN___RESTRICT__},
    {"__volatile__", TOKEN___VOLATILE__},
    {"__const__", TOKEN___CONST__},
    {"__signed__", TOKEN___SIGNED__},
    {"__unsigned__", TOKEN___UNSIGNED__},
    {"__complex__", TOKEN___COMPLEX__},
    {"__imag__", TOKEN___IMAG__},
    {"__real__", TOKEN___REAL__},
    {"__label__", TOKEN___LABEL__},
    {"__alignof__", TOKEN___ALIGNOF__},
    {"__builtin_va_arg", TOKEN___BUILTIN_VA_ARG},
    {"__builtin_offsetof", TOKEN___BUILTIN_OFFSETOF},
    {"__builtin_types_compatible_p", TOKEN___BUILTIN_TYPES_COMPATIBLE_P},
};

/* C99 Operators - MUST be ordered longest-first for correct lexing */
static const OperatorDef c_operators[] = {
    /* Three-character operators first */
    {"<<=", TOKEN_LESS_LESS_EQUAL, 1, ASSOC_RIGHT},
    {">>=", TOKEN_GREATER_GREATER_EQUAL, 1, ASSOC_RIGHT},
    
    /* Two-character operators */
    {"==", TOKEN_EQUAL_EQUAL, 8, ASSOC_LEFT},
    {"!=", TOKEN_EXCLAIM_EQUAL, 8, ASSOC_LEFT},
    {"<=", TOKEN_LESS_EQUAL, 9, ASSOC_LEFT},
    {">=", TOKEN_GREATER_EQUAL, 9, ASSOC_LEFT},
    {"<<", TOKEN_LESS_LESS, 10, ASSOC_LEFT},
    {">>", TOKEN_GREATER_GREATER, 10, ASSOC_LEFT},
    {"&&", TOKEN_AMPERSAND_AMPERSAND, 4, ASSOC_LEFT},
    {"||", TOKEN_PIPE_PIPE, 3, ASSOC_LEFT},
    {"++", TOKEN_PLUS_PLUS, 14, ASSOC_LEFT},
    {"--", TOKEN_MINUS_MINUS, 14, ASSOC_LEFT},
    {"->", TOKEN_ARROW, 14, ASSOC_LEFT},
    {"+=", TOKEN_PLUS_EQUAL, 1, ASSOC_RIGHT},
    {"-=", TOKEN_MINUS_EQUAL, 1, ASSOC_RIGHT},
    {"*=", TOKEN_STAR_EQUAL, 1, ASSOC_RIGHT},
    {"/=", TOKEN_SLASH_EQUAL, 1, ASSOC_RIGHT},
    {"%=", TOKEN_PERCENT_EQUAL, 1, ASSOC_RIGHT},
    {"&=", TOKEN_AMPERSAND_EQUAL, 1, ASSOC_RIGHT},
    {"|=", TOKEN_PIPE_EQUAL, 1, ASSOC_RIGHT},
    {"^=", TOKEN_CARET_EQUAL, 1, ASSOC_RIGHT},
    
    /* Single-character operators */
    {"=", TOKEN_EQUAL, 1, ASSOC_RIGHT},
    {"+", TOKEN_PLUS, 11, ASSOC_LEFT},
    {"-", TOKEN_MINUS, 11, ASSOC_LEFT},
    {"*", TOKEN_STAR, 12, ASSOC_LEFT},
    {"/", TOKEN_SLASH, 12, ASSOC_LEFT},
    {"%", TOKEN_PERCENT, 12, ASSOC_LEFT},
    {"&", TOKEN_AMPERSAND, 7, ASSOC_LEFT},
    {"|", TOKEN_PIPE, 5, ASSOC_LEFT},
    {"^", TOKEN_CARET, 6, ASSOC_LEFT},
    {"<", TOKEN_LESS, 9, ASSOC_LEFT},
    {">", TOKEN_GREATER, 9, ASSOC_LEFT},
    {"!", TOKEN_EXCLAIM, 13, ASSOC_RIGHT},
    {"~", TOKEN_TILDE, 13, ASSOC_RIGHT},
    {"?", TOKEN_QUESTION, 2, ASSOC_RIGHT},
    {":", TOKEN_COLON, 2, ASSOC_RIGHT},
    {".", TOKEN_DOT, 14, ASSOC_LEFT},
};

/* C99 Punctuation */
static const PunctuationDef c_punctuation[] = {
    /* Must check longer sequences first for correct tokenization */
    {"...", TOKEN_ELLIPSIS},
    {"##", TOKEN_HASH_HASH},
    {"(", TOKEN_LPAREN},
    {")", TOKEN_RPAREN},
    {"{", TOKEN_LBRACE},
    {"}", TOKEN_RBRACE},
    {"[", TOKEN_LBRACKET},
    {"]", TOKEN_RBRACKET},
    {";", TOKEN_SEMICOLON},
    {",", TOKEN_COMMA},
    {"#", TOKEN_HASH},
    {"@", TOKEN_AT},
    {"$", TOKEN_DOLLAR},
    {"`", TOKEN_BACKTICK},
};

/* Character classification for C */
static bool c_is_identifier_start(char c) {
    return isalpha(c) || c == '_';
}

static bool c_is_identifier_continue(char c) {
    return isalnum(c) || c == '_';
}

static bool c_is_digit(char c) {
    return isdigit(c);
}

static bool c_is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* Type name checking (simplified - would need symbol table in real impl) */
static bool c_is_type_name(const char *name) {
    /* Basic types */
    static const char *type_names[] = {
        "void", "char", "short", "int", "long", "float", "double",
        "signed", "unsigned", "_Bool", "_Complex", "_Imaginary",
        "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
        NULL
    };
    
    for (int i = 0; type_names[i]; i++) {
        if (strcmp(name, type_names[i]) == 0) {
            return true;
        }
    }
    
    /* Would check typedef'd names from symbol table here */
    return false;
}

/* Grammar rules */
static const GrammarRule c_grammar_rules[] = {
    {"translation_unit", parse_translation_unit},
    {"external_declaration", parse_external_declaration},
    {"function_definition", parse_function_definition},
    {"declaration", parse_declaration},
    {"statement", parse_statement},
    {"expression", parse_expression},
};

/* Create C99 syntax definition */
SyntaxDefinition *syntax_c99_create(void) {
    SyntaxDefinition *syntax = xcalloc(1, sizeof(SyntaxDefinition));
    
    syntax->language_name = "C";
    syntax->version = "C99";
    
    /* Lexical elements */
    syntax->keywords = c_keywords;
    syntax->keyword_count = sizeof(c_keywords) / sizeof(c_keywords[0]);
    
    syntax->operators = c_operators;
    syntax->operator_count = sizeof(c_operators) / sizeof(c_operators[0]);
    
    syntax->punctuation = c_punctuation;
    syntax->punctuation_count = sizeof(c_punctuation) / sizeof(c_punctuation[0]);
    
    /* Comment style */
    syntax->comment_style.single_line_start = "//";
    syntax->comment_style.multi_line_start = "/*";
    syntax->comment_style.multi_line_end = "*/";
    
    /* Character classification */
    syntax->is_identifier_start = c_is_identifier_start;
    syntax->is_identifier_continue = c_is_identifier_continue;
    syntax->is_digit = c_is_digit;
    syntax->is_whitespace = c_is_whitespace;
    
    /* String/char literals */
    syntax->string_delimiter = '"';
    syntax->char_delimiter = '\'';
    syntax->escape_char = '\\';
    
    /* Number literals */
    syntax->supports_hex = true;
    syntax->supports_octal = true;
    syntax->supports_binary = false;  /* C23 feature */
    syntax->supports_float = true;
    syntax->supports_scientific = true;
    
    /* Grammar */
    syntax->grammar_rules = c_grammar_rules;
    syntax->grammar_rule_count = sizeof(c_grammar_rules) / sizeof(c_grammar_rules[0]);
    syntax->start_rule = "translation_unit";
    
    /* Type system */
    syntax->is_type_name = c_is_type_name;
    
    /* Language features */
    syntax->case_sensitive = true;
    syntax->requires_semicolons = true;
    syntax->supports_preprocessor = true;
    
    return syntax;
}

/* Destroy C99 syntax definition */
void syntax_c99_destroy(SyntaxDefinition *syntax) {
    if (syntax) {
        xfree(syntax);
    }
}

/* Placeholder grammar rule implementations - will be filled in parser.c */
static ASTNode *parse_translation_unit(struct Parser *parser) {
    /* Implemented in parser.c */
    (void)parser;
    return NULL;
}

static ASTNode *parse_external_declaration(struct Parser *parser) {
    (void)parser;
    return NULL;
}

static ASTNode *parse_function_definition(struct Parser *parser) {
    (void)parser;
    return NULL;
}

static ASTNode *parse_declaration(struct Parser *parser) {
    (void)parser;
    return NULL;
}

static ASTNode *parse_statement(struct Parser *parser) {
    (void)parser;
    return NULL;
}

static ASTNode *parse_expression(struct Parser *parser) {
    (void)parser;
    return NULL;
}
