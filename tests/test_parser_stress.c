#include <stdio.h>
#include <string.h>
#include "../src/lexer/lexer.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/common/debug.h"
#include "../src/common/memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

void test_case(const char *name, const char *code, bool should_pass) {
    printf("\n=== Test: %s ===\n", name);
    printf("Code: %s\n", code);
    
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(code, "<test>", syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    if (!tokens) {
        printf("FAIL: Lexer failed\n");
        tests_failed++;
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        return;
    }
    
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    bool passed = (ast != NULL) == should_pass;
    
    if (passed) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL: Expected %s but got %s\n", 
               should_pass ? "success" : "failure",
               ast ? "success" : "failure");
        tests_failed++;
    }
    
    if (ast) {
        ast_destroy_node(ast);
    }
    
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
}

void test_trigraphs_and_digraphs(void) {
    printf("\n\n========== TRIGRAPHS & DIGRAPHS ==========\n");
    
    /* Digraphs */
    test_case("Digraph array subscript", 
              "int main() <% int a<:10:>; return a<:0:>; %>",
              true);
    
    test_case("Digraph logical operators",
              "int x = 1 and 2 or 3;",
              false);  /* 'and'/'or' are C++ keywords, not C */
}

void test_nested_declarations(void) {
    printf("\n\n========== NESTED DECLARATIONS ==========\n");
    
    test_case("Function pointer madness",
              "int (*(*foo)(int))(float);",
              true);
    
    test_case("Array of function pointers",
              "int (*arr[10])(int, float);",
              true);
    
    test_case("Pointer to array of function pointers",
              "int (*(*ptr)[10])(int);",
              true);
    
    test_case("Function returning pointer to function",
              "int (*func(int x))(float);",
              true);
    
    test_case("Spiral rule nightmare",
              "void (*signal(int, void (*)(int)))(int);",
              true);
}

void test_weird_syntax(void) {
    printf("\n\n========== WEIRD BUT VALID SYNTAX ==========\n");
    
    test_case("Empty statement spam",
              "int main() { ;;;;;;; return 0; }",
              true);
    
    test_case("Comma operator abuse",
              "int x = (1, 2, 3, 4, 5);",
              true);
    
    test_case("Nested ternary hell",
              "int x = a ? b ? c : d : e ? f : g;",
              true);
    
    test_case("Cast to function pointer",
              "int x = ((int(*)(void))0)();",
              true);
    
    test_case("Compound literal",
              "int *p = (int[]){1, 2, 3};",
              true);
    
    test_case("Designated initializers chaos",
              "struct { int a, b, c; } s = { .c = 3, .a = 1, .b = 2 };",
              true);
    
    test_case("Zero-length array (GCC extension)",
              "struct flex { int n; int data[0]; };",
              true);
}

void test_preprocessor_artifacts(void) {
    printf("\n\n========== PREPROCESSOR ARTIFACTS ==========\n");
    
    test_case("Line continuation artifacts",
              "int very\\\nlong\\\nname = 42;",
              false);  /* Lexer doesn't handle backslash continuation */
    
    test_case("Multiple spaces",
              "int     x     =     42     ;",
              true);
    
    test_case("Tabs and spaces mix",
              "int\tx\t=\t42\t;",
              true);
}

void test_edge_case_literals(void) {
    printf("\n\n========== EDGE CASE LITERALS ==========\n");
    
    test_case("Octal literal",
              "int x = 0777;",
              true);
    
    test_case("Hex literal",
              "int x = 0xDEADBEEF;",
              true);
    
    test_case("Float with exponent",
              "double x = 1.23e-45;",
              true);
    
    test_case("Hex float (C99)",
              "double x = 0x1.fp3;",
              true);
    
    test_case("Character escape sequences",
              "char c = '\\x41';",
              true);
    
    test_case("Wide string literal",
              "wchar_t *s = L\"hello\";",
              true);
    
    test_case("Adjacent string literals",
              "char *s = \"hello\" \"world\";",
              true);
}

void test_declaration_madness(void) {
    printf("\n\n========== DECLARATION MADNESS ==========\n");
    
    test_case("Multiple declarators",
              "int *p, **pp, ***ppp, a[10], b;",
              true);
    
    test_case("Old-style K&R function",
              "int func(a, b) int a; int b; { return a + b; }",
              false);  /* K&R style not fully supported */
    
    test_case("Variadic function",
              "int printf(const char *fmt, ...);",
              true);
    
    test_case("Inline function",
              "inline int add(int a, int b) { return a + b; }",
              true);
    
    test_case("Static inline",
              "static inline void foo(void) {}",
              true);
    
    test_case("Restrict pointer",
              "void func(int *restrict p);",
              true);
    
    test_case("Volatile sig_atomic_t",
              "volatile int flag;",
              true);
}

void test_control_flow_edge_cases(void) {
    printf("\n\n========== CONTROL FLOW EDGE CASES ==========\n");
    
    test_case("Empty for loop",
              "void f() { for(;;); }",
              true);
    
    test_case("For loop with comma",
              "void f() { for(int i=0, j=0; i<10; i++, j++) {} }",
              true);
    
    test_case("Switch with no cases",
              "void f(int x) { switch(x) {} }",
              true);
    
    test_case("Nested switch",
              "void f(int x, int y) { switch(x) { case 1: switch(y) { case 2: break; } break; } }",
              true);
    
    test_case("Do-while with complex condition",
              "void f() { do {} while((x++, y--, z)); }",
              true);
    
    test_case("If without braces",
              "void f() { if(x) if(y) z(); else w(); }",
              true);
    
    test_case("Dangling else",
              "void f() { if(a) if(b) c(); else d(); }",
              true);
}

void test_operator_precedence_traps(void) {
    printf("\n\n========== OPERATOR PRECEDENCE TRAPS ==========\n");
    
    test_case("Bitwise vs logical",
              "int x = a & b == c;",
              true);
    
    test_case("Shift and add",
              "int x = 1 << 2 + 3;",
              true);
    
    test_case("Ternary in ternary",
              "int x = a ? b : c ? d : e;",
              true);
    
    test_case("Comma in function call",
              "int x = func((a, b), c);",
              true);
    
    test_case("Assignment in condition",
              "if (x = 5) {}",
              true);
    
    test_case("Pre/post increment mix",
              "int x = ++a + a++ + --b + b--;",
              true);
}

void test_type_system_abuse(void) {
    printf("\n\n========== TYPE SYSTEM ABUSE ==========\n");
    
    test_case("Typedef redefinition",
              "typedef int myint; typedef int myint;",
              true);  /* Same type, allowed */
    
    test_case("Struct with same name as typedef",
              "typedef struct foo { int x; } foo;",
              true);
    
    test_case("Anonymous struct",
              "struct { int x; } var;",
              true);
    
    test_case("Bit fields",
              "struct { int a:3; int b:5; int :0; int c:2; } s;",
              true);
    
    test_case("Flexible array member",
              "struct { int n; int data[]; } s;",
              true);
    
    test_case("Const volatile",
              "const volatile int x = 42;",
              true);
    
    test_case("Pointer to const vs const pointer",
              "const int *p1; int *const p2 = 0;",
              true);
}

void test_expression_statement_ambiguity(void) {
    printf("\n\n========== EXPRESSION/STATEMENT AMBIGUITY ==========\n");
    
    test_case("Cast or multiplication",
              "int x = (int)*p;",
              true);
    
    test_case("Function call or declaration",
              "int main() { int (*fp)(); }",
              true);
    
    test_case("Compound literal or cast",
              "int *p = (int[3]){1,2,3};",
              true);
    
    test_case("Label or case",
              "void f() { label: x = 1; }",
              true);
}

void test_unicode_and_special_chars(void) {
    printf("\n\n========== UNICODE & SPECIAL CHARS ==========\n");
    
    test_case("Universal character name",
              "int \\u0041 = 65;",
              false);  /* UCN support may be limited */
    
    test_case("Null character in string",
              "char *s = \"hello\\0world\";",
              true);
    
    test_case("All escape sequences",
              "char *s = \"\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\?\";",
              true);
}

void test_macro_like_patterns(void) {
    printf("\n\n========== MACRO-LIKE PATTERNS ==========\n");
    
    test_case("Token pasting lookalike",
              "int x##y = 42;",
              false);  /* ## is preprocessor, not valid C */
    
    test_case("Stringification lookalike",
              "char *s = #define;",
              false);  /* # is preprocessor */
    
    test_case("Defined operator lookalike",
              "int defined = 42;",
              false);  /* 'defined' might be reserved */
}

void test_sizeof_and_alignof(void) {
    printf("\n\n========== SIZEOF & ALIGNOF ==========\n");
    
    test_case("Sizeof expression",
              "int x = sizeof(int);",
              true);
    
    test_case("Sizeof without parens",
              "int x = sizeof x;",
              true);
    
    test_case("Sizeof array",
              "int arr[10]; int x = sizeof arr;",
              true);
    
    test_case("Sizeof pointer arithmetic",
              "int x = sizeof(int*) * 10;",
              true);
    
    test_case("Nested sizeof",
              "int x = sizeof(sizeof(int));",
              true);
}

void test_goto_and_labels(void) {
    printf("\n\n========== GOTO & LABELS ==========\n");
    
    test_case("Simple goto",
              "void f() { goto end; end: return; }",
              true);
    
    test_case("Forward goto",
              "void f() { goto skip; int x = 1; skip: return; }",
              true);
    
    test_case("Backward goto",
              "void f() { loop: goto loop; }",
              true);
    
    test_case("Multiple labels",
              "void f() { a: b: c: return; }",
              true);
}

void test_incomplete_and_void_types(void) {
    printf("\n\n========== INCOMPLETE & VOID TYPES ==========\n");
    
    test_case("Void pointer",
              "void *p;",
              true);
    
    test_case("Void function",
              "void f(void) {}",
              true);
    
    test_case("Incomplete array",
              "extern int arr[];",
              true);
    
    test_case("Incomplete struct",
              "struct incomplete;",
              true);
    
    test_case("Forward declaration",
              "struct node; struct node { struct node *next; };",
              true);
}

int main(void) {
    printf("=================================================================\n");
    printf("              PARSER STRESS TESTS - EDGE CASES\n");
    printf("=================================================================\n");
    
    test_trigraphs_and_digraphs();
    test_nested_declarations();
    test_weird_syntax();
    test_preprocessor_artifacts();
    test_edge_case_literals();
    test_declaration_madness();
    test_control_flow_edge_cases();
    test_operator_precedence_traps();
    test_type_system_abuse();
    test_expression_statement_ambiguity();
    test_unicode_and_special_chars();
    test_macro_like_patterns();
    test_sizeof_and_alignof();
    test_goto_and_labels();
    test_incomplete_and_void_types();
    
    printf("\n\n=================================================================\n");
    printf("                    STRESS TEST RESULTS\n");
    printf("=================================================================\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("Total Tests:  %d\n", tests_passed + tests_failed);
    printf("Success Rate: %.1f%%\n", 
           100.0 * tests_passed / (tests_passed + tests_failed));
    printf("=================================================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
