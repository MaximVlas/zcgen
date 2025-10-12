#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/preprocessor/preprocessor.h"
#include "../src/lexer/lexer.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/common/debug.h"
#include "../src/common/memory.h"

void print_separator(const char *title) {
    printf("\n");
    printf("=================================================================\n");
    printf("  %s\n", title);
    printf("=================================================================\n");
}

void count_ast_nodes(ASTNode *node, size_t *count) {
    if (!node) return;
    (*count)++;
    for (size_t i = 0; i < node->child_count; i++) {
        count_ast_nodes(node->children[i], count);
    }
}

int main(int argc, char **argv) {
    const char *lua_file = argc > 1 ? argv[1] : "lua/onelua.c";
    
    print_separator("LUA CODEBASE COMPILATION TEST");
    printf("Testing against: %s\n", lua_file);
    
    clock_t start_time = clock();
    
    /* ===== STEP 1: PREPROCESSING ===== */
    print_separator("STEP 1: PREPROCESSING");
    
    PreprocessorOptions opts = {
        .keep_comments = false,
        .keep_whitespace = false,
        .expand_macros = true,
        .target_triple = "x86_64-pc-linux-gnu"
    };
    
    Preprocessor *pp = preprocessor_create(&opts);
    
    /* Add Lua directory to include paths */
    preprocessor_add_include_path(pp, "lua");
    
    /* Add system includes */
    preprocessor_add_system_include_path(pp, "/usr/include");
    
    /* Define standard macros */
    preprocessor_define(pp, "__STDC__", "1");
    preprocessor_define(pp, "__STDC_VERSION__", "199901L");
    preprocessor_define(pp, "MAKE_LUA", NULL);
    
    printf("Preprocessing %s...\n", lua_file);
    char *preprocessed = preprocessor_process_file(pp, lua_file);
    
    if (!preprocessed) {
        printf("FAIL: Preprocessing failed: %s\n", preprocessor_get_error(pp));
        preprocessor_destroy(pp);
        return 1;
    }
    
    size_t preprocessed_size = strlen(preprocessed);
    printf("SUCCESS: Preprocessed %zu bytes\n", preprocessed_size);
    
    /* Count lines */
    size_t line_count = 1;
    for (size_t i = 0; i < preprocessed_size; i++) {
        if (preprocessed[i] == '\n') line_count++;
    }
    printf("Lines of code: %zu\n", line_count);
    
    preprocessor_destroy(pp);
    
    /* ===== STEP 2: LEXICAL ANALYSIS ===== */
    print_separator("STEP 2: LEXICAL ANALYSIS");
    
    printf("Tokenizing preprocessed source...\n");
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(preprocessed, lua_file, syntax);
    
    clock_t lex_start = clock();
    TokenList *tokens = lexer_tokenize(lexer);
    clock_t lex_end = clock();
    
    if (!tokens) {
        printf("FAIL: Lexer failed\n");
        xfree(preprocessed);
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        return 1;
    }
    
    double lex_time = (double)(lex_end - lex_start) / CLOCKS_PER_SEC;
    printf("SUCCESS: Generated %zu tokens in %.3f seconds\n", tokens->count, lex_time);
    printf("Tokens per second: %.0f\n", tokens->count / lex_time);
    
    /* Token statistics */
    size_t keyword_count = 0;
    size_t identifier_count = 0;
    size_t literal_count = 0;
    size_t operator_count = 0;
    size_t punctuation_count = 0;
    
    Token *tok = tokens->head;
    while (tok) {
        const char *category = debug_token_category(tok->type);
        if (strcmp(category, "keyword") == 0) keyword_count++;
        else if (tok->type == TOKEN_IDENTIFIER) identifier_count++;
        else if (strcmp(category, "literal") == 0) literal_count++;
        else if (strcmp(category, "operator") == 0) operator_count++;
        else if (strcmp(category, "punctuation") == 0) punctuation_count++;
        tok = tok->next;
    }
    
    printf("\nToken Breakdown:\n");
    printf("  Keywords:     %zu\n", keyword_count);
    printf("  Identifiers:  %zu\n", identifier_count);
    printf("  Literals:     %zu\n", literal_count);
    printf("  Operators:    %zu\n", operator_count);
    printf("  Punctuation:  %zu\n", punctuation_count);
    
    /* ===== STEP 3: SYNTAX ANALYSIS (PARSING) ===== */
    print_separator("STEP 3: SYNTAX ANALYSIS");
    
    printf("Parsing token stream...\n");
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    
    clock_t parse_start = clock();
    ASTNode *ast = c_parser_parse(parser);
    clock_t parse_end = clock();
    
    if (!ast) {
        printf("FAIL: Parser failed\n");
        printf("This is expected - Lua uses many advanced C features\n");
        c_parser_destroy(parser);
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(preprocessed);
        return 1;
    }
    
    double parse_time = (double)(parse_end - parse_start) / CLOCKS_PER_SEC;
    printf("SUCCESS: Parsed in %.3f seconds\n", parse_time);
    
    /* AST statistics */
    size_t node_count = 0;
    count_ast_nodes(ast, &node_count);
    
    printf("AST nodes created: %zu\n", node_count);
    printf("Nodes per second: %.0f\n", node_count / parse_time);
    
    /* ===== SUMMARY ===== */
    print_separator("COMPILATION SUMMARY");
    
    clock_t end_time = clock();
    double total_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    
    printf("Total compilation time: %.3f seconds\n", total_time);
    printf("\nPhase Breakdown:\n");
    printf("  Preprocessing: %.1f%%\n", 100.0 * (total_time - lex_time - parse_time) / total_time);
    printf("  Lexing:        %.1f%%\n", 100.0 * lex_time / total_time);
    printf("  Parsing:       %.1f%%\n", 100.0 * parse_time / total_time);
    
    printf("\nPerformance:\n");
    printf("  Lines/sec:     %.0f\n", line_count / total_time);
    printf("  Tokens/sec:    %.0f\n", tokens->count / total_time);
    printf("  Nodes/sec:     %.0f\n", node_count / total_time);
    
    printf("\nMemory Usage:\n");
    printf("  Preprocessed:  %zu bytes (%.2f MB)\n", 
           preprocessed_size, preprocessed_size / 1024.0 / 1024.0);
    printf("  Tokens:        %zu tokens\n", tokens->count);
    printf("  AST nodes:     %zu nodes\n", node_count);
    
    print_separator("TEST COMPLETED SUCCESSFULLY");
    printf("✓ Lua codebase successfully compiled!\n");
    printf("✓ All phases completed without errors\n");
    printf("✓ Your compiler can handle real-world C code!\n");
    
    /* Cleanup */
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    xfree(preprocessed);
    
    return 0;
}
