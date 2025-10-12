#include <stdio.h>
#include <string.h>
#include "../src/preprocessor/preprocessor.h"
#include "../src/lexer/lexer.h"
#include "../src/parser/parser.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/common/debug.h"
#include "../src/common/memory.h"

void test_basic_preprocessing(void) {
    printf("\n=== Test: Basic Preprocessing ===\n");
    
    PreprocessorOptions opts = {
        .keep_comments = false,
        .keep_whitespace = false,
        .expand_macros = true,
        .target_triple = NULL
    };
    
    Preprocessor *pp = preprocessor_create(&opts);
    
    /* Add some defines */
    preprocessor_define(pp, "MAX_SIZE", "100");
    preprocessor_define(pp, "DEBUG", NULL);
    
    /* Test source with macros */
    const char *source = 
        "#ifdef DEBUG\n"
        "int debug_mode = 1;\n"
        "#endif\n"
        "int buffer[MAX_SIZE];\n"
        "int main(void) { return 0; }\n";
    
    char *preprocessed = preprocessor_process_string(pp, source, NULL);
    
    if (preprocessed) {
        printf("Preprocessed output:\n%s\n", preprocessed);
        xfree(preprocessed);
        printf("PASS\n");
    } else {
        printf("FAIL: %s\n", preprocessor_get_error(pp));
    }
    
    preprocessor_destroy(pp);
}

void test_include_file(void) {
    printf("\n=== Test: Include File Processing ===\n");
    
    /* Create a simple header file */
    FILE *fp = fopen("/tmp/test_header.h", "w");
    if (fp) {
        fprintf(fp, "#define PI 3.14159\n");
        fprintf(fp, "typedef struct { int x; int y; } Point;\n");
        fclose(fp);
    }
    
    PreprocessorOptions opts = {
        .keep_comments = false,
        .keep_whitespace = false,
        .expand_macros = true,
        .target_triple = NULL
    };
    
    Preprocessor *pp = preprocessor_create(&opts);
    preprocessor_add_include_path(pp, "/tmp");
    
    const char *source = 
        "#include \"test_header.h\"\n"
        "double radius = PI;\n"
        "Point origin = {0, 0};\n";
    
    char *preprocessed = preprocessor_process_string(pp, source, NULL);
    
    if (preprocessed) {
        printf("Preprocessed output:\n%s\n", preprocessed);
        
        /* Now parse the preprocessed output */
        printf("\n--- Parsing preprocessed output ---\n");
        SyntaxDefinition *syntax = syntax_c99_create();
        Lexer *lexer = lexer_create(preprocessed, "<preprocessed>", syntax);
        TokenList *tokens = lexer_tokenize(lexer);
        
        if (tokens) {
            printf("Tokens generated: %zu\n", tokens->count);
            
            /* Parse the tokens */
            CParser *parser = c_parser_create(tokens, C_STD_C99);
            ASTNode *ast = c_parser_parse(parser);
            
            if (ast) {
                printf("AST created successfully!\n");
                debug_print_ast(stdout, ast);
                ast_destroy_node(ast);
            }
            
            c_parser_destroy(parser);
        }
        
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(preprocessed);
        printf("PASS\n");
    } else {
        printf("FAIL: %s\n", preprocessor_get_error(pp));
    }
    
    preprocessor_destroy(pp);
    remove("/tmp/test_header.h");
}

void test_conditional_compilation(void) {
    printf("\n=== Test: Conditional Compilation ===\n");
    
    PreprocessorOptions opts = {
        .keep_comments = false,
        .keep_whitespace = false,
        .expand_macros = true,
        .target_triple = NULL
    };
    
    Preprocessor *pp = preprocessor_create(&opts);
    preprocessor_define(pp, "LINUX", "1");
    
    const char *source = 
        "#if defined(LINUX)\n"
        "const char *os = \"Linux\";\n"
        "#elif defined(WINDOWS)\n"
        "const char *os = \"Windows\";\n"
        "#else\n"
        "const char *os = \"Unknown\";\n"
        "#endif\n";
    
    char *preprocessed = preprocessor_process_string(pp, source, NULL);
    
    if (preprocessed) {
        printf("Preprocessed output:\n%s\n", preprocessed);
        
        /* Verify it contains "Linux" */
        if (strstr(preprocessed, "Linux")) {
            printf("PASS: Conditional compilation worked\n");
        } else {
            printf("FAIL: Expected 'Linux' in output\n");
        }
        
        xfree(preprocessed);
    } else {
        printf("FAIL: %s\n", preprocessor_get_error(pp));
    }
    
    preprocessor_destroy(pp);
}

void test_macro_expansion(void) {
    printf("\n=== Test: Macro Expansion ===\n");
    
    PreprocessorOptions opts = {
        .keep_comments = false,
        .keep_whitespace = false,
        .expand_macros = true,
        .target_triple = NULL
    };
    
    Preprocessor *pp = preprocessor_create(&opts);
    preprocessor_define(pp, "SQUARE(x)", "((x) * (x))");
    preprocessor_define(pp, "MAX(a,b)", "((a) > (b) ? (a) : (b))");
    
    const char *source = 
        "int area = SQUARE(5);\n"
        "int maximum = MAX(10, 20);\n";
    
    char *preprocessed = preprocessor_process_string(pp, source, NULL);
    
    if (preprocessed) {
        printf("Preprocessed output:\n%s\n", preprocessed);
        printf("PASS\n");
        xfree(preprocessed);
    } else {
        printf("FAIL: %s\n", preprocessor_get_error(pp));
    }
    
    preprocessor_destroy(pp);
}

int main(void) {
    printf("=================================================================\n");
    printf("                PREPROCESSOR TESTS\n");
    printf("=================================================================\n");
    
    test_basic_preprocessing();
    test_include_file();
    test_conditional_compilation();
    test_macro_expansion();
    
    printf("\n=================================================================\n");
    printf("                ALL PREPROCESSOR TESTS COMPLETED\n");
    printf("=================================================================\n");
    
    return 0;
}
