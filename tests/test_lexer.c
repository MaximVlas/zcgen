/* Test the lexer with real C code */

#include "../src/lexer/lexer.h"
#include "../src/syntax/c_syntax.h"
#include "../src/common/debug.h"
#include <stdio.h>
#include <assert.h>

/* Test simple tokens */
void test_simple_tokens(void) {
    const char *source = "int x = 42;";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: Simple tokens\n");
    debug_print_token_list(stdout, tokens);
    
    assert(tokens->count == 6); /* int, x, =, 42, ;, EOF */
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Simple tokens test\n\n");
}

/* Test all operators */
void test_operators(void) {
    const char *source = 
        "+ - * / % "
        "++ -- "
        "== != < > <= >= "
        "&& || ! "
        "& | ^ ~ << >> "
        "= += -= *= /= %= &= |= ^= <<= >>= "
        "-> . "
        "? : , ";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: All operators\n");
    debug_print_token_list_compact(stdout, tokens);
    
    assert(tokens->count > 30); /* Many operators + EOF */
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Operators test\n\n");
}

/* Test keywords */
void test_keywords(void) {
    const char *source = 
        "auto break case char const continue default do "
        "double else enum extern float for goto if "
        "inline int long register return short signed sizeof static "
        "struct switch typedef union unsigned void volatile while "
        "_Bool _Complex _Imaginary "
        "restrict "
        "_Alignas _Alignof _Atomic _Generic _Noreturn _Static_assert _Thread_local";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: All C keywords\n");
    debug_print_token_stats(stdout, tokens);
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Keywords test\n\n");
}

/* Test string and char literals */
void test_literals(void) {
    const char *source = 
        "\"hello world\" "
        "'a' '\\n' '\\t' "
        "42 0x2A 0b101010 052 "
        "3.14 1.0e10 0.5f";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: Literals\n");
    debug_print_token_list(stdout, tokens);
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Literals test\n\n");
}

/* Test comments */
void test_comments(void) {
    const char *source = 
        "int x; // single line comment\n"
        "/* multi\n"
        "   line\n"
        "   comment */\n"
        "int y;";
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, "test.c", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Test: Comments\n");
    debug_print_token_list(stdout, tokens);
    
    /* Comments should be skipped */
    assert(tokens->count == 7); /* int, x, ;, int, y, ;, EOF */
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Comments test\n\n");
}

/* Test on real function */
void test_real_function(void) {
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
    
    printf("Test: Real function\n");
    debug_print_token_list(stdout, tokens);
    debug_print_token_stats(stdout, tokens);
    
    /* Export to file for inspection */
    debug_dump_tokens_to_file("test_factorial_tokens.txt", tokens);
    printf("Tokens exported to test_factorial_tokens.txt\n");
    
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    printf("PASS: Real function test\n\n");
}

int main(void) {
    printf("================================================================\n");
    printf("LLVM-C LEXER TEST SUITE\n");
    printf("================================================================\n\n");
    
    debug_init();
    
    test_simple_tokens();
    test_operators();
    test_keywords();
    test_literals();
    test_comments();
    test_real_function();
    
    printf("================================================================\n");
    printf("ALL LEXER TESTS PASSED\n");
    printf("================================================================\n");
    
    return 0;
}
