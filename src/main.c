#include "ast/ast.h"
#include "codegen/codegen.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/memory.h"
#include "lexer/lexer.h"
#include "parser/c_parser.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "syntax/c_syntax.h"
#include "syntax/syntax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Debug flags */
typedef struct {
    bool lexer;
    bool parser;
    bool ast;
    bool codegen;
    bool tokens;
    bool stats;
    bool verbose;
    bool all;
    char *output_file;
} DebugFlags;

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
  printf("\nDebug Options:\n");
  printf("  --debug-lexer      Show lexer debug output (tokens)\n");
  printf("  --debug-parser     Show parser debug output (parsing steps)\n");
  printf("  --debug-ast        Show AST debug output (tree structure)\n");
  printf("  --debug-codegen    Show codegen debug output (LLVM generation)\n");
  printf("  --debug-all        Enable all debug output\n");
  printf("  --debug-tokens     Dump tokens to stdout\n");
  printf("  --debug-stats      Show compilation statistics\n");
  printf("  --debug-verbose    Extra verbose debug output\n");
  printf("  --debug-file <f>   Write debug output to file instead of stdout\n");
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
  
  /* Initialize debug flags */
  DebugFlags debug_flags = {0};
  debug_flags.output_file = NULL;

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
    } else if (strcmp(argv[i], "--debug-lexer") == 0) {
      debug_flags.lexer = true;
    } else if (strcmp(argv[i], "--debug-parser") == 0) {
      debug_flags.parser = true;
    } else if (strcmp(argv[i], "--debug-ast") == 0) {
      debug_flags.ast = true;
    } else if (strcmp(argv[i], "--debug-codegen") == 0) {
      debug_flags.codegen = true;
    } else if (strcmp(argv[i], "--debug-tokens") == 0) {
      debug_flags.tokens = true;
    } else if (strcmp(argv[i], "--debug-stats") == 0) {
      debug_flags.stats = true;
    } else if (strcmp(argv[i], "--debug-verbose") == 0) {
      debug_flags.verbose = true;
    } else if (strcmp(argv[i], "--debug-all") == 0) {
      debug_flags.all = true;
      debug_flags.lexer = true;
      debug_flags.parser = true;
      debug_flags.ast = true;
      debug_flags.codegen = true;
      debug_flags.tokens = true;
      debug_flags.stats = true;
      debug_flags.verbose = true;
    } else if (strcmp(argv[i], "--debug-file") == 0 && i + 1 < argc) {
      debug_flags.output_file = argv[++i];
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
  if (!debug_flags.verbose) printf("Lexing...\n");
  Lexer *lexer = lexer_create(source, input_file, syntax);
  TokenList *tokens = lexer_tokenize(lexer);

  if (error_count() > 0) {
    fprintf(stderr, "%d error(s) during lexing\n", error_count());
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    xfree(source);
    return 1;
  }

  if (!debug_flags.verbose) printf("Lexed %zu tokens\n", tokens->count);
  
  /* Debug output for lexer */
  FILE *debug_out = debug_flags.output_file ? fopen(debug_flags.output_file, "w") : stdout;
  if (!debug_out) debug_out = stdout;
  
  if (debug_flags.lexer || debug_flags.all) {
    fprintf(debug_out, "\n=== LEXER DEBUG OUTPUT ===\n");
    debug_print_token_list(debug_out, tokens);
  }
  
  if (debug_flags.tokens || debug_flags.all) {
    fprintf(debug_out, "\n=== TOKEN DUMP ===\n");
    debug_print_token_list_compact(debug_out, tokens);
  }
  
  if (debug_flags.stats || debug_flags.all) {
    fprintf(debug_out, "\n=== LEXER STATISTICS ===\n");
    debug_print_token_stats(debug_out, tokens);
  }

  /* Parse */
  if (!debug_flags.verbose) printf("Parsing...\n");
  
  /* Enable parser debug output if requested */
  if (debug_flags.parser || debug_flags.verbose || debug_flags.all) {
    debug_set_parser_verbose(true);
  }
  
  CParser *parser = c_parser_create(tokens, C_STD_C99);
  ASTNode *ast = c_parser_parse(parser);

  if (error_count() > 0) {
    fprintf(stderr, "%d error(s) during parsing\n", error_count());

    /* Always dump debug info on error, but respect output file setting */
    const char *error_debug_file = debug_flags.output_file ? debug_flags.output_file : "debug_parse_error.txt";
    if (!debug_flags.output_file) {
      char debug_file[256];
      snprintf(debug_file, sizeof(debug_file), "debug_parse_error_%s.txt",
               strrchr(input_file, '/') ? strrchr(input_file, '/') + 1 : input_file);
      debug_dump_all_to_file(debug_file, tokens, ast);
      fprintf(stderr, "Debug info dumped to: %s\n", debug_file);
    } else {
      debug_dump_all_to_file(error_debug_file, tokens, ast);
      fprintf(stderr, "Debug info dumped to: %s\n", error_debug_file);
    }

    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    xfree(source);
    return 1;
  }

  if (!debug_flags.verbose) printf("Parsed successfully\n");
  
  /* Debug output for AST */
  if (debug_flags.ast || debug_flags.all) {
    fprintf(debug_out, "\n=== AST DEBUG OUTPUT ===\n");
    debug_print_ast_detailed(debug_out, ast);
  }
  
  if (debug_flags.stats || debug_flags.all) {
    fprintf(debug_out, "\n=== AST STATISTICS ===\n");
    debug_print_ast_stats(debug_out, ast);
  }

  /* Codegen */
  if (!debug_flags.verbose) printf("Generating code...\n");
  
  if (debug_flags.codegen || debug_flags.all) {
    fprintf(debug_out, "\n=== CODEGEN DEBUG OUTPUT ===\n");
    fprintf(debug_out, "Backend: %d\n", backend);
    fprintf(debug_out, "Target: %s\n", target_triple ? target_triple : "default");
    fprintf(debug_out, "Optimization level: %d\n", opt_level);
    fprintf(debug_out, "Debug info: %s\n", debug_info ? "enabled" : "disabled");
  }
  
  CodegenContext *codegen = codegen_init(backend, target_triple);
  if (!codegen) {
    fprintf(stderr, "Error: failed to initialize codegen\n");
    ast_destroy_node(ast);
    c_parser_destroy(parser);
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
    c_parser_destroy(parser);
    lexer_destroy(lexer);
    syntax_c99_destroy(syntax);
    xfree(source);
    return 1;
  }
  
  if (debug_flags.codegen || debug_flags.all) {
    fprintf(debug_out, "Code generation completed successfully\n");
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

  /* Close debug output file if we opened one */
  if (debug_flags.output_file && debug_out != stdout) {
    fclose(debug_out);
    if (debug_flags.verbose) {
      printf("Debug output written to: %s\n", debug_flags.output_file);
    }
  }

  /* Cleanup */
  codegen_destroy(codegen);
  ast_destroy_node(ast);
  c_parser_destroy(parser);
  lexer_destroy(lexer);
  syntax_c99_destroy(syntax);
  xfree(source);

  return success ? 0 : 1;
}
