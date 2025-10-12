/* Test the parser with real C code */

#include "../src/lexer/lexer.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/common/debug.h"
#include <stdio.h>
#include <assert.h>

/* Test simple expression */
void test_simple_expression(void) {
    /* In C, expressions must be inside functions, not at file level */
    const char *source = 
        "void test(void) {\n"
        "    2 + 3 * 4;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Simple expression (2 + 3 * 4)\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Simple expression test\n\n");
}

/* Test function declaration */
void test_function_declaration(void) {
    const char *source = 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Function declaration\n");
    debug_print_ast_detailed(stdout, ast);
    debug_print_ast_stats(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Function declaration test\n\n");
}

/* Test control flow */
void test_control_flow(void) {
    const char *source = 
        "int factorial(int n) {\n"
        "    if (n <= 1) {\n"
        "        return 1;\n"
        "    }\n"
        "    return n * factorial(n - 1);\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Control flow (factorial)\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    /* Export for inspection */
    debug_dump_ast_to_file("test_factorial_ast.txt", ast);
    printf("AST exported to test_factorial_ast.txt\n");
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Control flow test\n\n");
}

/* Test struct declaration */
void test_struct_declaration(void) {
    const char *source = 
        "struct Point {\n"
        "    int x;\n"
        "    int y;\n"
        "};\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Struct declaration\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Struct declaration test\n\n");
}

/* Test complex expressions */
void test_complex_expressions(void) {
    const char *source = 
        "int test(void) {\n"
        "    int x = (a + b) * (c - d);\n"
        "    int y = arr[i] + ptr->member;\n"
        "    int z = func(1, 2, 3);\n"
        "    return x ? y : z;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Complex expressions\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Complex expressions test\n\n");
}

/* Test all statement types */
void test_all_statements(void) {
    const char *source = 
        "void test_statements(void) {\n"
        "    int x = 0;\n"
        "    \n"
        "    if (x > 0) { x++; }\n"
        "    \n"
        "    while (x < 10) { x++; }\n"
        "    \n"
        "    for (int i = 0; i < 10; i++) { x += i; }\n"
        "    \n"
        "    do { x--; } while (x > 0);\n"
        "    \n"
        "    switch (x) {\n"
        "        case 0: break;\n"
        "        case 1: return;\n"
        "        default: continue;\n"
        "    }\n"
        "    \n"
        "    goto label;\n"
        "    label: return;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: All statement types\n");
    debug_print_ast_detailed(stdout, ast);
    debug_print_ast_stats(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: All statements test\n\n");
}

/* Test on parser's own source code! */
void test_self_parsing(void) {
    /* Read a small snippet from c_parser.c */
    const char *source = 
        "static unsigned int hash_string(const char *str) {\n"
        "    unsigned int hash = 5381;\n"
        "    int c;\n"
        "    while ((c = *str++)) {\n"
        "        hash = ((hash << 5) + hash) + c;\n"
        "    }\n"
        "    return hash % SYMBOL_TABLE_SIZE;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "c_parser.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: Self-parsing (hash_string from c_parser.c)\n");
    printf("Tokens:\n");
    debug_print_token_list_compact(stdout, tokens);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("\nAST:\n");
    debug_print_ast(stdout, ast);
    
    /* Export full debug info */
    debug_dump_all_to_file("test_self_parse.txt", tokens, ast);
    printf("Full debug output exported to test_self_parse.txt\n");
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Self-parsing test\n\n");
}

/* Test multiple function declarations */
void test_multiple_functions(void) {
    const char *source = 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n"
        "\n"
        "int subtract(int x, int y) {\n"
        "    return x - y;\n"
        "}\n"
        "\n"
        "int multiply(int m, int n) {\n"
        "    return m * n;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Multiple functions\n");
    debug_print_ast(stdout, ast);
    debug_print_ast_stats(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Multiple functions test\n\n");
}

/* Test pointer declarations */
void test_pointers(void) {
    const char *source = 
        "int *ptr;\n"
        "char **pptr;\n"
        "void *generic;\n"
        "int *array[10];\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Pointer declarations\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Pointers test\n\n");
}

/* Test nested control flow */
void test_nested_control_flow(void) {
    const char *source = 
        "int search(int arr[], int size, int target) {\n"
        "    for (int i = 0; i < size; i++) {\n"
        "        if (arr[i] == target) {\n"
        "            return i;\n"
        "        }\n"
        "    }\n"
        "    return -1;\n"
        "}\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Nested control flow\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Nested control flow test\n\n");
}

/* Test global variables */
void test_global_variables(void) {
    const char *source = 
        "int global_counter = 0;\n"
        "const char *message = \"Hello\";\n"
        "static int internal = 42;\n"
        "extern int external;\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Global variables\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Global variables test\n\n");
}

/* Test typedef and custom types */
void test_typedefs(void) {
    const char *source = 
        "typedef int Integer;\n"
        "typedef struct Node Node;\n"
        "Integer value = 10;\n";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    printf("Test: Typedefs\n");
    debug_print_ast(stdout, ast);
    
    assert(ast != NULL);
    
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Typedefs test\n\n");
}

int main(void) {
    printf("================================================================\n");
    printf("LLVM-C PARSER TEST SUITE\n");
    printf("================================================================\n\n");
    
    debug_init();
    
    test_simple_expression();
    test_function_declaration();
    test_control_flow();
    test_struct_declaration();
    test_complex_expressions();
    test_all_statements();
    test_self_parsing();
    
    /* New expanded tests */
    test_multiple_functions();
    test_pointers();
    test_nested_control_flow();
    test_global_variables();
    test_typedefs();
    
    printf("\n================================================================\n");
    printf("ALL PARSER TESTS PASSED\n");
    printf("================================================================\n");
    
    return 0;
}
