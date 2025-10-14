/* Test LLVM code generation */

#include "../src/lexer/lexer.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/codegen/codegen.h"
#include "../src/common/debug.h"
#include <stdio.h>
#include <assert.h>

/* Test simple function codegen */
void test_simple_function(void) {
    const char *source =
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n";

    printf("Test: Simple function codegen\n");
    printf("Source:\n%s\n", source);

    /* Parse */
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);

    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);

    assert(ast != NULL);
    printf("✓ Parsed successfully\n");

    /* Generate code */
    CodegenContext *ctx = codegen_init(BACKEND_LLVM, "x86_64-pc-linux-gnu");
    assert(ctx != NULL);
    printf("✓ Codegen context created\n");

    codegen_set_opt_level(ctx, 0);  /* No optimization for testing */

    bool success = codegen_generate(ctx, ast, "test_add");
    if (!success) {
        fprintf(stderr, "Codegen failed: %s\n", codegen_get_error(ctx));
    }
    assert(success);
    printf("✓ Code generated\n");

    /* Emit LLVM IR */
    success = codegen_emit_llvm_ir(ctx, "test_add.ll");
    if (!success) {
        fprintf(stderr, "Failed to emit IR: %s\n", codegen_get_error(ctx));
    }
    assert(success);
    printf("✓ LLVM IR emitted to test_add.ll\n");

    /* Cleanup */
    codegen_destroy(ctx);
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);

    printf("PASS: Simple function codegen\n\n");
}

/* Test expression codegen */
void test_expressions(void) {
    const char *source =
        "int calculate(void) {\n"
        "    return 2 + 3 * 4;\n"
        "}\n";

    printf("Test: Expression codegen\n");
    printf("Source:\n%s\n", source);

    /* Parse */
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);

    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);

    assert(ast != NULL);
    printf("✓ Parsed successfully\n");

    /* Generate code */
    CodegenContext *ctx = codegen_init(BACKEND_LLVM, "x86_64-pc-linux-gnu");
    assert(ctx != NULL);

    codegen_set_opt_level(ctx, 0);

    bool success = codegen_generate(ctx, ast, "test_calc");
    assert(success);
    printf("✓ Code generated\n");

    /* Emit LLVM IR */
    success = codegen_emit_llvm_ir(ctx, "test_calc.ll");
    assert(success);
    printf("✓ LLVM IR emitted to test_calc.ll\n");

    /* Cleanup */
    codegen_destroy(ctx);
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);

    printf("PASS: Expression codegen\n\n");
}

/* Test optimization levels */
void test_optimization(void) {
    const char *source =
        "int factorial(int n) {\n"
        "    if (n <= 1) return 1;\n"
        "    return n * factorial(n - 1);\n"
        "}\n";

    printf("Test: Optimization levels\n");
    printf("Source:\n%s\n", source);

    /* Parse once */
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);

    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);

    assert(ast != NULL);
    printf("Parsed successfully\n");

    /* Test each optimization level */
    for (int opt_level = 0; opt_level <= 3; opt_level++) {
        printf("  Testing O%d...\n", opt_level);

        CodegenContext *ctx = codegen_init(BACKEND_LLVM, "x86_64-pc-linux-gnu");
        assert(ctx != NULL);

        codegen_set_opt_level(ctx, opt_level);

        bool success = codegen_generate(ctx, ast, "test_factorial");
        if (!success) {
            fprintf(stderr, "Codegen failed at O%d: %s\n", opt_level, codegen_get_error(ctx));
        }
        assert(success);

        char filename[64];
        snprintf(filename, sizeof(filename), "test_factorial_O%d.ll", opt_level);
        success = codegen_emit_llvm_ir(ctx, filename);
        assert(success);
        printf("  ✓ O%d: IR emitted to %s\n", opt_level, filename);

        codegen_destroy(ctx);
    }

    /* Cleanup */
    ast_destroy_node(ast);
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);

    printf("PASS: Optimization levels\n\n");
}

int main(void) {
    printf("================================================================\n");
    printf("                LLVM CODEGEN TEST SUITE\n");
    printf("================================================================\n\n");

    test_simple_function();
    test_expressions();
    test_optimization();

    printf("================================================================\n");
    printf("                ALL CODEGEN TESTS PASSED\n");
    printf("================================================================\n");

    return 0;
}
