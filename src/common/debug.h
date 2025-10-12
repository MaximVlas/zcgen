#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"
#include <stdio.h>

/* Debug output options */
typedef struct {
    bool use_color;
    bool show_location;
    bool show_token_values;
    bool show_ast_addresses;
    bool show_ast_types;
    int indent_size;
} DebugOptions;

/* Initialize debug system */
void debug_init(void);
void debug_set_options(DebugOptions *opts);
DebugOptions *debug_get_options(void);

/* ===== TOKEN DEBUGGING ===== */

/* Print single token with full details */
void debug_print_token(FILE *out, Token *token);

/* Print token list in various formats */
void debug_print_token_list(FILE *out, TokenList *tokens);
void debug_print_token_list_compact(FILE *out, TokenList *tokens);
void debug_print_token_list_detailed(FILE *out, TokenList *tokens);

/* Print token statistics */
void debug_print_token_stats(FILE *out, TokenList *tokens);

/* Export tokens to JSON */
void debug_export_tokens_json(FILE *out, TokenList *tokens);

/* Export tokens to XML */
void debug_export_tokens_xml(FILE *out, TokenList *tokens);

/* ===== AST DEBUGGING ===== */

/* Print AST tree structure */
void debug_print_ast(FILE *out, ASTNode *node);
void debug_print_ast_compact(FILE *out, ASTNode *node);
void debug_print_ast_detailed(FILE *out, ASTNode *node);

/* Print AST with line drawing characters */
void debug_print_ast_tree(FILE *out, ASTNode *node);

/* Print AST statistics */
void debug_print_ast_stats(FILE *out, ASTNode *node);

/* Export AST to JSON */
void debug_export_ast_json(FILE *out, ASTNode *node);

/* Export AST to XML */
void debug_export_ast_xml(FILE *out, ASTNode *node);

/* Export AST to Graphviz DOT format */
void debug_export_ast_dot(FILE *out, ASTNode *node);

/* ===== SYMBOL TABLE DEBUGGING ===== */

/* Print symbol table (when implemented) */
void debug_print_symbol_table(FILE *out, void *symbol_table);

/* ===== TYPE DEBUGGING ===== */

/* Print type information */
void debug_print_type(FILE *out, ASTNode *type);

/* ===== UTILITY FUNCTIONS ===== */

/* Get human-readable token type name */
const char *debug_token_type_name(TokenType type);

/* Get human-readable AST node type name */
const char *debug_ast_node_type_name(ASTNodeType type);

/* Get token type category */
const char *debug_token_category(TokenType type);

/* Print source location */
void debug_print_location(FILE *out, SourceLocation loc);

/* ===== PARSER ERROR DEBUGGING ===== */

/* Enable/disable verbose parser error output */
void debug_set_parser_verbose(bool verbose);

/* Print parser state at error */
void debug_print_parser_error(FILE *out, Token *current, const char *message);

/* Print parser context (surrounding tokens) */
void debug_print_parser_context(FILE *out, Token *current, int context_size);

/* Print expected vs actual token */
void debug_print_token_mismatch(FILE *out, Token *actual, TokenType expected, const char *message);

/* ===== FILE OUTPUT ===== */

/* Dump tokens to file */
void debug_dump_tokens_to_file(const char *filename, TokenList *tokens);

/* Dump AST to file */
void debug_dump_ast_to_file(const char *filename, ASTNode *ast);

/* Dump both tokens and AST to file */
void debug_dump_all_to_file(const char *filename, TokenList *tokens, ASTNode *ast);

#endif /* DEBUG_H */
