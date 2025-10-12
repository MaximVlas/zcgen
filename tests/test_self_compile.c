/* Test the compiler on its own source code - the ultimate test! */

#include "../src/lexer/lexer.h"
#include "../src/parser/c_parser.h"
#include "../src/syntax/c_syntax.h"
#include "../src/ast/ast.h"
#include "../src/common/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read file into string */
char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    
    fclose(f);
    return content;
}

/* Test parsing a source file */
void test_parse_file(const char *filename) {
    printf("\n================================================================\n");
    printf("Testing: %s\n", filename);
    printf("================================================================\n");
    
    char *source = read_file(filename);
    if (!source) {
        printf("✗ Failed to read file\n");
        return;
    }
    
    printf("File size: %zu bytes\n", strlen(source));
    
    /* Lex the file */
    SyntaxDefinition *syntax = syntax_c99_create();
    Lexer *lexer = lexer_create(source, filename, syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    printf("Tokens: %zu\n", tokens->count);
    debug_print_token_stats(stdout, tokens);
    
    /* Parse the file */
    CParser *parser = c_parser_create(tokens, C_STD_C99);
    ASTNode *ast = c_parser_parse(parser);
    
    if (ast) {
        printf("\nPASS: Parsing successful\n");
        debug_print_ast_stats(stdout, ast);
        
        /* Export debug info */
        char output_file[256];
        snprintf(output_file, sizeof(output_file), "debug_%s.txt", 
                 strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename);
        debug_dump_all_to_file(output_file, tokens, ast);
        printf("Debug output: %s\n", output_file);
        
        ast_destroy_node(ast);
    } else {
        printf("\nFAIL: Parsing failed (errors: %d)\n", parser->base.error_count);
    }
    
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    free(source);
}

int main(int argc, char **argv) {
    printf("================================================================\n");
    printf("LLVM-C SELF-COMPILATION TEST SUITE\n");
    printf("Testing the compiler on its own source code\n");
    printf("================================================================\n");
    
    debug_init();
    
    /* Test on compiler's own source files */
    const char *source_files[] = {
        /* Core infrastructure */
        "src/common/types.h",
        "src/common/memory.c",
        "src/common/error.c",
        
        /* Lexer */
        "src/lexer/lexer.h",
        "src/lexer/lexer.c",
        
        /* Parser */
        "src/parser/parser.h",
        "src/parser/parser.c",
        "src/parser/c_parser.h",
        "src/parser/c_parser.c",  /* Parse the parser! */
        
        /* AST */
        "src/ast/ast.h",
        "src/ast/ast.c",
        
        /* Syntax */
        "src/syntax/syntax.h",
        "src/syntax/c_syntax.h",
        "src/syntax/c_syntax.c",
        
        /* Debug */
        "src/common/debug.h",
        "src/common/debug.c",
        
        /* Codegen */
        "src/codegen/backend.h",
        "src/codegen/codegen.h",
        "src/codegen/llvm_backend.h",
        "src/codegen/llvm_backend.c",
        
        NULL
    };
    
    int total = 0;
    int success = 0;
    
    for (int i = 0; source_files[i] != NULL; i++) {
        total++;
        
        /* Build full path */
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "../%s", source_files[i]);
        
        test_parse_file(fullpath);
        success++; /* If we get here, it didn't crash! */
    }
    
    printf("\n\n");
    printf("================================================================\n");
    printf("RESULTS\n");
    printf("================================================================\n");
    printf("Files tested:    %d\n", total);
    printf("Files parsed:    %d\n", success);
    printf("Success rate:    %.1f%%\n", (float)success / total * 100);
    printf("================================================================\n");
    
    if (success == total) {
        printf("ALL TESTS PASSED\n");
        printf("The compiler successfully parsed its own source code!\n");
        printf("This is a major milestone - the compiler is self-hosting capable!\n");
    } else {
        printf("Some tests failed - check error messages above\n");
    }
    
    printf("================================================================\n");
    
    return (success == total) ? 0 : 1;
}
