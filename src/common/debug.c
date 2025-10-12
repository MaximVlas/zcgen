#include "debug.h"
#include "memory.h"
#include "c_syntax.h"
#include <string.h>

/* Debug options */
static DebugOptions debug_opts = {
    .use_color = true,
    .show_location = true,
    .show_token_values = true,
    .show_ast_addresses = false,
    .show_ast_types = true,
    .indent_size = 2
};

/* Color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

/* ===== INITIALIZATION ===== */

void debug_init(void) {
    /* Already initialized with defaults */
}

void debug_set_options(DebugOptions *opts) {
    if (opts) {
        debug_opts = *opts;
    }
}

DebugOptions *debug_get_options(void) {
    return &debug_opts;
}

/* ===== TOKEN TYPE NAMES ===== */

const char *debug_token_type_name(TokenType type) {
    /* Special tokens */
    if (type == TOKEN_EOF) return "EOF";
    if (type == TOKEN_ERROR) return "ERROR";
    if (type == TOKEN_UNKNOWN) return "UNKNOWN";
    if (type == TOKEN_IDENTIFIER) return "IDENTIFIER";
    if (type == TOKEN_INTEGER_LITERAL) return "INTEGER";
    if (type == TOKEN_FLOAT_LITERAL) return "FLOAT";
    if (type == TOKEN_STRING_LITERAL) return "STRING";
    if (type == TOKEN_CHAR_LITERAL) return "CHAR";
    
    /* Keywords */
    if (type >= TOKEN_KEYWORD_START && type <= TOKEN_KEYWORD_END) {
        switch (type) {
            case TOKEN_AUTO: return "auto";
            case TOKEN_BREAK: return "break";
            case TOKEN_CASE: return "case";
            case TOKEN_CHAR: return "char";
            case TOKEN_CONST: return "const";
            case TOKEN_CONTINUE: return "continue";
            case TOKEN_DEFAULT: return "default";
            case TOKEN_DO: return "do";
            case TOKEN_DOUBLE: return "double";
            case TOKEN_ELSE: return "else";
            case TOKEN_ENUM: return "enum";
            case TOKEN_EXTERN: return "extern";
            case TOKEN_FLOAT: return "float";
            case TOKEN_FOR: return "for";
            case TOKEN_GOTO: return "goto";
            case TOKEN_IF: return "if";
            case TOKEN_INLINE: return "inline";
            case TOKEN_INT: return "int";
            case TOKEN_LONG: return "long";
            case TOKEN_REGISTER: return "register";
            case TOKEN_RESTRICT: return "restrict";
            case TOKEN_RETURN: return "return";
            case TOKEN_SHORT: return "short";
            case TOKEN_SIGNED: return "signed";
            case TOKEN_SIZEOF: return "sizeof";
            case TOKEN_STATIC: return "static";
            case TOKEN_STRUCT: return "struct";
            case TOKEN_SWITCH: return "switch";
            case TOKEN_TYPEDEF: return "typedef";
            case TOKEN_UNION: return "union";
            case TOKEN_UNSIGNED: return "unsigned";
            case TOKEN_VOID: return "void";
            case TOKEN_VOLATILE: return "volatile";
            case TOKEN_WHILE: return "while";
            case TOKEN__BOOL: return "_Bool";
            case TOKEN__COMPLEX: return "_Complex";
            case TOKEN__IMAGINARY: return "_Imaginary";
            default: return "KEYWORD";
        }
    }
    
    /* Operators */
    if (type >= TOKEN_OPERATOR_START && type <= TOKEN_OPERATOR_END) {
        switch (type) {
            case TOKEN_PLUS: return "+";
            case TOKEN_MINUS: return "-";
            case TOKEN_STAR: return "*";
            case TOKEN_SLASH: return "/";
            case TOKEN_PERCENT: return "%";
            case TOKEN_AMPERSAND: return "&";
            case TOKEN_PIPE: return "|";
            case TOKEN_CARET: return "^";
            case TOKEN_TILDE: return "~";
            case TOKEN_EXCLAIM: return "!";
            case TOKEN_EQUAL: return "=";
            case TOKEN_LESS: return "<";
            case TOKEN_GREATER: return ">";
            case TOKEN_PLUS_EQUAL: return "+=";
            case TOKEN_MINUS_EQUAL: return "-=";
            case TOKEN_STAR_EQUAL: return "*=";
            case TOKEN_EQUAL_EQUAL: return "==";
            case TOKEN_EXCLAIM_EQUAL: return "!=";
            case TOKEN_LESS_EQUAL: return "<=";
            case TOKEN_GREATER_EQUAL: return ">=";
            case TOKEN_AMPERSAND_AMPERSAND: return "&&";
            case TOKEN_PIPE_PIPE: return "||";
            case TOKEN_LESS_LESS: return "<<";
            case TOKEN_GREATER_GREATER: return ">>";
            case TOKEN_PLUS_PLUS: return "++";
            case TOKEN_MINUS_MINUS: return "--";
            case TOKEN_ARROW: return "->";
            case TOKEN_DOT: return ".";
            default: return "OPERATOR";
        }
    }
    
    /* Punctuation */
    if (type >= TOKEN_PUNCTUATION_START && type <= TOKEN_PUNCTUATION_END) {
        switch (type) {
            case TOKEN_LPAREN: return "(";
            case TOKEN_RPAREN: return ")";
            case TOKEN_LBRACE: return "{";
            case TOKEN_RBRACE: return "}";
            case TOKEN_LBRACKET: return "[";
            case TOKEN_RBRACKET: return "]";
            case TOKEN_SEMICOLON: return ";";
            case TOKEN_COMMA: return ",";
            case TOKEN_ELLIPSIS: return "...";
            default: return "PUNCTUATION";
        }
    }
    
    return "UNKNOWN";
}

const char *debug_token_category(TokenType type) {
    if (type == TOKEN_EOF) return "special";
    if (type == TOKEN_ERROR) return "error";
    if (type >= TOKEN_IDENTIFIER && type <= TOKEN_CHAR_LITERAL) return "literal";
    if (type >= TOKEN_KEYWORD_START && type <= TOKEN_KEYWORD_END) return "keyword";
    if (type >= TOKEN_OPERATOR_START && type <= TOKEN_OPERATOR_END) return "operator";
    if (type >= TOKEN_PUNCTUATION_START && type <= TOKEN_PUNCTUATION_END) return "punctuation";
    return "unknown";
}

/* ===== AST NODE TYPE NAMES ===== */

const char *debug_ast_node_type_name(ASTNodeType type) {
    switch (type) {
        /* Top level */
        case AST_TRANSLATION_UNIT: return "TranslationUnit";
        case AST_MODULE: return "Module";
        
        /* Declarations */
        case AST_FUNCTION_DECL: return "FunctionDecl";
        case AST_VAR_DECL: return "VarDecl";
        case AST_PARAM_DECL: return "ParamDecl";
        case AST_TYPEDEF_DECL: return "TypedefDecl";
        case AST_STRUCT_DECL: return "StructDecl";
        case AST_UNION_DECL: return "UnionDecl";
        case AST_ENUM_DECL: return "EnumDecl";
        
        /* Statements */
        case AST_COMPOUND_STMT: return "CompoundStmt";
        case AST_EXPR_STMT: return "ExprStmt";
        case AST_IF_STMT: return "IfStmt";
        case AST_WHILE_STMT: return "WhileStmt";
        case AST_FOR_STMT: return "ForStmt";
        case AST_RETURN_STMT: return "ReturnStmt";
        case AST_BREAK_STMT: return "BreakStmt";
        case AST_CONTINUE_STMT: return "ContinueStmt";
        
        /* Expressions */
        case AST_BINARY_EXPR: return "BinaryExpr";
        case AST_ADD_EXPR: return "AddExpr";
        case AST_SUB_EXPR: return "SubExpr";
        case AST_MUL_EXPR: return "MulExpr";
        case AST_DIV_EXPR: return "DivExpr";
        case AST_MOD_EXPR: return "ModExpr";
        case AST_UNARY_EXPR: return "UnaryExpr";
        case AST_CALL_EXPR: return "CallExpr";
        case AST_MEMBER_EXPR: return "MemberExpr";
        case AST_ARRAY_SUBSCRIPT_EXPR: return "ArraySubscript";
        
        /* Literals */
        case AST_INTEGER_LITERAL: return "IntegerLiteral";
        case AST_FLOAT_LITERAL: return "FloatLiteral";
        case AST_STRING_LITERAL: return "StringLiteral";
        case AST_CHAR_LITERAL: return "CharLiteral";
        case AST_IDENTIFIER: return "Identifier";
        
        /* Types */
        case AST_TYPE: return "Type";
        case AST_POINTER_TYPE: return "PointerType";
        case AST_ARRAY_TYPE: return "ArrayType";
        
        /* Inline assembly */
        case AST_ASM_STMT: return "AsmStmt";
        
        default: return "Unknown";
    }
}

/* ===== LOCATION PRINTING ===== */

void debug_print_location(FILE *out, SourceLocation loc) {
    if (debug_opts.use_color) {
        fprintf(out, "%s%s:%u:%u%s", COLOR_DIM, 
                loc.filename ? loc.filename : "<unknown>",
                loc.line, loc.column, COLOR_RESET);
    } else {
        fprintf(out, "%s:%u:%u", 
                loc.filename ? loc.filename : "<unknown>",
                loc.line, loc.column);
    }
}

/* ===== TOKEN PRINTING ===== */

void debug_print_token(FILE *out, Token *token) {
    if (!token) return;
    
    const char *color = "";
    const char *reset = "";
    
    if (debug_opts.use_color) {
        const char *category = debug_token_category(token->type);
        if (strcmp(category, "keyword") == 0) color = COLOR_BLUE;
        else if (strcmp(category, "operator") == 0) color = COLOR_YELLOW;
        else if (strcmp(category, "literal") == 0) color = COLOR_GREEN;
        else if (strcmp(category, "punctuation") == 0) color = COLOR_CYAN;
        reset = COLOR_RESET;
    }
    
    fprintf(out, "%s%-20s%s", color, debug_token_type_name(token->type), reset);
    
    if (token->lexeme && strlen(token->lexeme) > 0) {
        fprintf(out, " '%s'", token->lexeme);
    }
    
    if (debug_opts.show_token_values) {
        switch (token->type) {
            case TOKEN_INTEGER_LITERAL:
                fprintf(out, " = %ld", token->value.int_value);
                break;
            case TOKEN_FLOAT_LITERAL:
                fprintf(out, " = %f", token->value.float_value);
                break;
            case TOKEN_CHAR_LITERAL:
                fprintf(out, " = '%c'", token->value.char_value);
                break;
            default:
                break;
        }
    }
    
    if (debug_opts.show_location) {
        fprintf(out, " @ ");
        debug_print_location(out, token->location);
    }
    
    fprintf(out, "\n");
}

void debug_print_token_list(FILE *out, TokenList *tokens) {
    if (!tokens) return;
    
    fprintf(out, "=== TOKEN LIST (%zu tokens) ===\n\n", tokens->count);
    
    Token *token = tokens->head;
    size_t index = 0;
    
    while (token) {
        fprintf(out, "[%4zu] ", index++);
        debug_print_token(out, token);
        token = token->next;
    }
    
    fprintf(out, "\n");
}

void debug_print_token_list_compact(FILE *out, TokenList *tokens) {
    if (!tokens) return;
    
    Token *token = tokens->head;
    while (token) {
        if (token->lexeme && strlen(token->lexeme) > 0) {
            fprintf(out, "%s ", token->lexeme);
        } else {
            fprintf(out, "%s ", debug_token_type_name(token->type));
        }
        token = token->next;
    }
    fprintf(out, "\n");
}

void debug_print_token_stats(FILE *out, TokenList *tokens) {
    if (!tokens) return;
    
    size_t keyword_count = 0;
    size_t operator_count = 0;
    size_t literal_count = 0;
    size_t punctuation_count = 0;
    size_t identifier_count = 0;
    
    Token *token = tokens->head;
    while (token) {
        const char *category = debug_token_category(token->type);
        if (strcmp(category, "keyword") == 0) keyword_count++;
        else if (strcmp(category, "operator") == 0) operator_count++;
        else if (strcmp(category, "literal") == 0) literal_count++;
        else if (strcmp(category, "punctuation") == 0) punctuation_count++;
        else if (token->type == TOKEN_IDENTIFIER) identifier_count++;
        token = token->next;
    }
    
    fprintf(out, "=== TOKEN STATISTICS ===\n");
    fprintf(out, "Total tokens:    %zu\n", tokens->count);
    fprintf(out, "Keywords:        %zu\n", keyword_count);
    fprintf(out, "Operators:       %zu\n", operator_count);
    fprintf(out, "Identifiers:     %zu\n", identifier_count);
    fprintf(out, "Literals:        %zu\n", literal_count);
    fprintf(out, "Punctuation:     %zu\n", punctuation_count);
    fprintf(out, "\n");
}

/* ===== AST PRINTING ===== */

static void print_ast_indent(FILE *out, int depth) {
    for (int i = 0; i < depth * debug_opts.indent_size; i++) {
        fprintf(out, " ");
    }
}

void debug_print_ast(FILE *out, ASTNode *node) {
    debug_print_ast_detailed(out, node);
}

static void print_ast_recursive(FILE *out, ASTNode *node, int depth) {
    if (!node) return;
    
    print_ast_indent(out, depth);
    
    const char *color = debug_opts.use_color ? COLOR_CYAN : "";
    const char *reset = debug_opts.use_color ? COLOR_RESET : "";
    
    fprintf(out, "%s%s%s", color, debug_ast_node_type_name(node->type), reset);
    
    /* Print node-specific info */
    switch (node->type) {
        case AST_FUNCTION_DECL:
            if (node->data.func_decl.name) {
                fprintf(out, " '%s'", node->data.func_decl.name);
            }
            break;
        case AST_VAR_DECL:
        case AST_PARAM_DECL:
            if (node->data.var_decl.name) {
                fprintf(out, " '%s'", node->data.var_decl.name);
            }
            break;
        case AST_IDENTIFIER:
            if (node->data.identifier.name) {
                fprintf(out, " '%s'", node->data.identifier.name);
            }
            break;
        case AST_INTEGER_LITERAL:
            fprintf(out, " %ld", node->data.int_literal.value);
            break;
        case AST_FLOAT_LITERAL:
            fprintf(out, " %f", node->data.float_literal.value);
            break;
        case AST_STRING_LITERAL:
            if (node->data.string_literal.value) {
                fprintf(out, " \"%s\"", node->data.string_literal.value);
            }
            break;
        case AST_BINARY_EXPR:
            if (node->data.binary_expr.op) {
                fprintf(out, " '%s'", node->data.binary_expr.op);
            }
            break;
        default:
            break;
    }
    
    if (debug_opts.show_location) {
        fprintf(out, " @ ");
        debug_print_location(out, node->location);
    }
    
    if (debug_opts.show_ast_addresses) {
        fprintf(out, " [%p]", (void*)node);
    }
    
    fprintf(out, "\n");
    
    /* Print children */
    for (size_t i = 0; i < node->child_count; i++) {
        print_ast_recursive(out, node->children[i], depth + 1);
    }
}

void debug_print_ast_detailed(FILE *out, ASTNode *node) {
    fprintf(out, "=== AST TREE ===\n\n");
    print_ast_recursive(out, node, 0);
    fprintf(out, "\n");
}

static size_t count_ast_nodes(ASTNode *node) {
    if (!node) return 0;
    
    size_t count = 1;
    for (size_t i = 0; i < node->child_count; i++) {
        count += count_ast_nodes(node->children[i]);
    }
    return count;
}

static size_t get_ast_depth(ASTNode *node) {
    if (!node) return 0;
    
    size_t max_depth = 0;
    for (size_t i = 0; i < node->child_count; i++) {
        size_t child_depth = get_ast_depth(node->children[i]);
        if (child_depth > max_depth) {
            max_depth = child_depth;
        }
    }
    return max_depth + 1;
}

void debug_print_ast_stats(FILE *out, ASTNode *node) {
    fprintf(out, "=== AST STATISTICS ===\n");
    fprintf(out, "Total nodes:     %zu\n", count_ast_nodes(node));
    fprintf(out, "Tree depth:      %zu\n", get_ast_depth(node));
    fprintf(out, "\n");
}

/* ===== FILE OUTPUT ===== */

void debug_dump_tokens_to_file(const char *filename, TokenList *tokens) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    debug_print_token_list(f, tokens);
    debug_print_token_stats(f, tokens);
    
    fclose(f);
}

void debug_dump_ast_to_file(const char *filename, ASTNode *ast) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    debug_print_ast_detailed(f, ast);
    debug_print_ast_stats(f, ast);
    
    fclose(f);
}

void debug_dump_all_to_file(const char *filename, TokenList *tokens, ASTNode *ast) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "=============================================================\n");
    fprintf(f, "           LLVM-C COMPILER DEBUG OUTPUT                     \n");
    fprintf(f, "=============================================================\n\n");
    
    debug_print_token_list(f, tokens);
    debug_print_token_stats(f, tokens);
    
    fprintf(f, "\n");
    
    debug_print_ast_detailed(f, ast);
    debug_print_ast_stats(f, ast);
    
    fclose(f);
}

/* ===== PARSER ERROR DEBUGGING ===== */

static bool parser_verbose = true;

void debug_set_parser_verbose(bool verbose) {
    parser_verbose = verbose;
}

void debug_print_parser_error(FILE *out, Token *current, const char *message) {
    if (!parser_verbose) {
        fprintf(out, "error: %s\n", message);
        return;
    }
    
    fprintf(out, "\n");
    fprintf(out, "================================================================\n");
    fprintf(out, "                    PARSER ERROR                                \n");
    fprintf(out, "================================================================\n\n");
    
    if (current) {
        fprintf(out, "Location: %s:%d:%d\n", 
                current->location.filename,
                current->location.line,
                current->location.column);
        fprintf(out, "Error: %s\n", message);
        fprintf(out, "Current token: %s '%s'\n", 
                debug_token_type_name(current->type),
                current->lexeme ? current->lexeme : "");
        fprintf(out, "\n");
    } else {
        fprintf(out, "Error: %s\n", message);
        fprintf(out, "Current token: <EOF>\n\n");
    }
}

void debug_print_parser_context(FILE *out, Token *current, int context_size) {
    if (!current) {
        fprintf(out, "No context available (at EOF)\n");
        return;
    }
    
    fprintf(out, "Parser Context (next %d tokens):\n", context_size * 2);
    fprintf(out, "-----------------------------------------------------------------\n");
    
    // Print current token highlighted
    fprintf(out, "  >> CURRENT: %-20s '%s' @ %s:%d:%d\n",
            debug_token_type_name(current->type),
            current->lexeme ? current->lexeme : "",
            current->location.filename,
            current->location.line,
            current->location.column);
    
    // Print following tokens
    Token *tok = current->next;
    int pos = 1;
    while (tok && pos <= context_size * 2) {
        fprintf(out, "    [+%2d]  %-20s '%s' @ %s:%d:%d\n",
                pos,
                debug_token_type_name(tok->type),
                tok->lexeme ? tok->lexeme : "",
                tok->location.filename,
                tok->location.line,
                tok->location.column);
        tok = tok->next;
        pos++;
    }
    fprintf(out, "-----------------------------------------------------------------\n\n");
}

void debug_print_token_mismatch(FILE *out, Token *actual, TokenType expected, const char *message) {
    fprintf(out, "\n");
    fprintf(out, "================================================================\n");
    fprintf(out, "                  TOKEN MISMATCH ERROR                          \n");
    fprintf(out, "================================================================\n\n");
    
    if (actual) {
        fprintf(out, "Location: %s:%d:%d\n",
                actual->location.filename,
                actual->location.line,
                actual->location.column);
    }
    
    fprintf(out, "Error: %s\n\n", message);
    fprintf(out, "Expected: %s\n", debug_token_type_name(expected));
    
    if (actual) {
        fprintf(out, "Got:      %s '%s'\n\n",
                debug_token_type_name(actual->type),
                actual->lexeme ? actual->lexeme : "");
        
        // Print context
        debug_print_parser_context(out, actual, 3);
    } else {
        fprintf(out, "Got:      <EOF>\n\n");
    }
}

/* ===== JSON EXPORT (STUB) ===== */

void debug_export_tokens_json(FILE *out, TokenList *tokens) {
    /* TODO: Implement JSON export */
    (void)out;
    (void)tokens;
}

void debug_export_ast_json(FILE *out, ASTNode *node) {
    /* TODO: Implement JSON export */
    (void)out;
    (void)node;
}

/* ===== XML EXPORT (STUB) ===== */

void debug_export_tokens_xml(FILE *out, TokenList *tokens) {
    /* TODO: Implement XML export */
    (void)out;
    (void)tokens;
}

void debug_export_ast_xml(FILE *out, ASTNode *node) {
    /* TODO: Implement XML export */
    (void)out;
    (void)node;
}

/* ===== GRAPHVIZ DOT EXPORT (STUB) ===== */

void debug_export_ast_dot(FILE *out, ASTNode *node) {
    /* TODO: Implement Graphviz DOT export */
    (void)out;
    (void)node;
}
