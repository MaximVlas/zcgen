#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/error.h"
#include "common/memory.h"
#include "syntax/syntax.h"
#include "syntax/c_syntax.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "ast/ast.h"
#include "codegen/codegen.h"
#include "preprocessor/preprocessor.h"

static void print_usage(const char *program) {
    printf("Usage: %s [options] <input-file>\n", program);
    printf("\nOptions:\n");
    printf("  -o <file>          Write output to <file>\n");
    printf("  -O<level>          Optimization level (0-3, s, z)\n");
    printf("  -g                 Generate debug information\n");
    printf("  -S                 Emit assembly\n");
    printf("  -c                 Compile only, don't link\n");
    printf("  --emit-llvm        Emit LLVM IR\n");
    printf("  --backend=<name>   Use backend (llvm, rust, zig, c)\n");
    printf("  --target=<triple>  Target triple\n");
    printf("  -I<path>           Add include path\n");
    printf("  -D<macro>=<value>  Define macro\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -h, --help         Show this help\n");
    printf("\nBackends:\n");
    printf("  llvm               LLVM backend (default)\n");
    printf("  rust               Rust backend (if available)\n");
    printf("  zig                Zig backend (if available)\n");
    printf("  c                  C transpiler\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Initialize diagnostic system */
    diagnostic_init();
    
    /* Parse command line */
    const char *input_file = NULL;
    const char *output_file = "a.out";
    int opt_level = 0;
    bool debug_info = false;
    bool emit_assembly = false;
    bool emit_llvm = false;
    bool compile_only = false;
    BackendType backend = BACKEND_LLVM;
    const char *target_triple = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strncmp(argv[i], "-O", 2) == 0) {
            char level = argv[i][2];
            if (level >= '0' && level <= '3') {
                opt_level = level - '0';
            } else if (level == 's' || level == 'z') {
                opt_level = 2; /* Size optimization */
            }
        } else if (strcmp(argv[i], "-g") == 0) {
            debug_info = true;
        } else if (strcmp(argv[i], "-S") == 0) {
            emit_assembly = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_only = true;
        } else if (strcmp(argv[i], "--emit-llvm") == 0) {
            emit_llvm = true;
        } else if (strncmp(argv[i], "--backend=", 10) == 0) {
            const char *backend_name = argv[i] + 10;
            if (strcmp(backend_name, "llvm") == 0) {
                backend = BACKEND_LLVM;
            } else if (strcmp(backend_name, "rust") == 0) {
                backend = BACKEND_RUST;
            } else if (strcmp(backend_name, "zig") == 0) {
                backend = BACKEND_ZIG;
            } else if (strcmp(backend_name, "c") == 0) {
                backend = BACKEND_C;
            } else {
                fprintf(stderr, "Unknown backend: %s\n", backend_name);
                return 1;
            }
        } else if (strncmp(argv[i], "--target=", 9) == 0) {
            target_triple = argv[i] + 9;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }
    
    if (!input_file) {
        fprintf(stderr, "Error: no input file\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Read input file */
    FILE *f = fopen(input_file, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open file '%s'\n", input_file);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *source = xmalloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    /* Register source for diagnostics */
    diagnostic_set_source(input_file, source);
    
    /* Create syntax definition */
    SyntaxDefinition *syntax = syntax_c99_create();
    
    /* Preprocess if needed */
    if (syntax->supports_preprocessor) {
        Preprocessor *pp = preprocessor_create(NULL);
        char *preprocessed = preprocessor_process_string(pp, source, input_file);
        if (preprocessed) {
            xfree(source);
            source = preprocessed;
        }
        preprocessor_destroy(pp);
    }
    
    /* Lex */
    printf("Lexing...\n");
    Lexer *lexer = lexer_create(source, input_file, syntax);
    TokenList *tokens = lexer_tokenize(lexer);
    
    if (error_count() > 0) {
        fprintf(stderr, "%d error(s) during lexing\n", error_count());
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(source);
        return 1;
    }
    
    printf("Lexed %zu tokens\n", tokens->count);
    
    /* Parse */
    printf("Parsing...\n");
    Parser *parser = parser_create(tokens, syntax);
    ASTNode *ast = parser_parse(parser);
    
    if (error_count() > 0) {
        fprintf(stderr, "%d error(s) during parsing\n", error_count());
        parser_destroy(parser);
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(source);
        return 1;
    }
    
    printf("Parsed successfully\n");
    
    /* Codegen */
    printf("Generating code...\n");
    CodegenContext *codegen = codegen_init(backend, target_triple);
    if (!codegen) {
        fprintf(stderr, "Error: failed to initialize codegen\n");
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(source);
        return 1;
    }
    
    codegen_set_opt_level(codegen, opt_level);
    codegen_set_debug_info(codegen, debug_info);
    
    if (!codegen_generate(codegen, ast, input_file)) {
        fprintf(stderr, "Error: %s\n", codegen_get_error(codegen));
        codegen_destroy(codegen);
        ast_destroy_node(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);
        syntax_c99_destroy(syntax);
        xfree(source);
        return 1;
    }
    
    /* Emit output */
    bool success = false;
    if (emit_llvm) {
        success = codegen_emit_llvm_ir(codegen, output_file);
    } else if (emit_assembly) {
        success = codegen_emit_assembly(codegen, output_file);
    } else if (compile_only) {
        success = codegen_emit_object(codegen, output_file);
    } else {
        /* Compile and link */
        const char *obj_file = "/tmp/temp.o";
        success = codegen_emit_object(codegen, obj_file);
        if (success) {
            const char *objs[] = {obj_file};
            success = codegen_link(codegen, objs, 1, output_file, false);
        }
    }
    
    if (!success) {
        fprintf(stderr, "Error: %s\n", codegen_get_error(codegen));
    } else {
        printf("Successfully generated: %s\n", output_file);
    }
    
    /* Cleanup */
    codegen_destroy(codegen);
    ast_destroy_node(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    xfree(source);
    
    return success ? 0 : 1;
}
