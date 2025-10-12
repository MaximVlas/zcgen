#include "preprocessor.h"
#include "../common/memory.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <clang-c/Index.h>

/* Preprocessor implementation using Clang's C API */

struct Preprocessor {
    CXIndex index;
    char **include_paths;
    size_t include_path_count;
    char **system_include_paths;
    size_t system_include_path_count;
    char **defines;
    size_t define_count;
    char **undefines;
    size_t undefine_count;
    char *last_error;
    PreprocessorOptions options;
};

Preprocessor *preprocessor_create(PreprocessorOptions *opts) {
    Preprocessor *pp = xcalloc(1, sizeof(Preprocessor));
    
    /* Create Clang index */
    pp->index = clang_createIndex(0, 0);
    
    /* Set default options */
    if (opts) {
        pp->options = *opts;
    } else {
        pp->options.keep_comments = false;
        pp->options.keep_whitespace = false;
        pp->options.expand_macros = true;
        pp->options.target_triple = NULL;
    }
    
    return pp;
}

void preprocessor_destroy(Preprocessor *pp) {
    if (!pp) return;
    
    /* Free include paths */
    for (size_t i = 0; i < pp->include_path_count; i++) {
        xfree(pp->include_paths[i]);
    }
    xfree(pp->include_paths);
    
    /* Free system include paths */
    for (size_t i = 0; i < pp->system_include_path_count; i++) {
        xfree(pp->system_include_paths[i]);
    }
    xfree(pp->system_include_paths);
    
    /* Free defines */
    for (size_t i = 0; i < pp->define_count; i++) {
        xfree(pp->defines[i]);
    }
    xfree(pp->defines);
    
    /* Free undefines */
    for (size_t i = 0; i < pp->undefine_count; i++) {
        xfree(pp->undefines[i]);
    }
    xfree(pp->undefines);
    
    xfree(pp->last_error);
    
    /* Dispose Clang index */
    if (pp->index) {
        clang_disposeIndex(pp->index);
    }
    
    xfree(pp);
}

void preprocessor_add_include_path(Preprocessor *pp, const char *path) {
    if (!pp || !path) return;
    
    pp->include_paths = xrealloc(pp->include_paths,
                                 (pp->include_path_count + 1) * sizeof(char *));
    pp->include_paths[pp->include_path_count++] = xstrdup(path);
}

void preprocessor_add_system_include_path(Preprocessor *pp, const char *path) {
    if (!pp || !path) return;
    
    pp->system_include_paths = xrealloc(pp->system_include_paths,
                                        (pp->system_include_path_count + 1) * sizeof(char *));
    pp->system_include_paths[pp->system_include_path_count++] = xstrdup(path);
}

void preprocessor_define(Preprocessor *pp, const char *name, const char *value) {
    if (!pp || !name) return;
    
    /* Format: -DNAME=VALUE or -DNAME */
    /* Store as two separate strings to avoid shell escaping issues */
    char *define;
    if (value && strlen(value) > 0) {
        /* Allocate space for -D prefix + name + = + value */
        size_t len = strlen(name) + strlen(value) + 4;
        define = xmalloc(len);
        snprintf(define, len, "-D%s=%s", name, value);
    } else {
        size_t len = strlen(name) + 3;
        define = xmalloc(len);
        snprintf(define, len, "-D%s", name);
    }
    
    pp->defines = xrealloc(pp->defines, (pp->define_count + 1) * sizeof(char *));
    pp->defines[pp->define_count++] = define;
}

void preprocessor_undefine(Preprocessor *pp, const char *name) {
    if (!pp || !name) return;
    
    /* Format: -UNAME */
    size_t len = strlen(name) + 3; /* -U + null */
    char *undefine = xmalloc(len);
    snprintf(undefine, len, "-U%s", name);
    
    pp->undefines = xrealloc(pp->undefines, (pp->undefine_count + 1) * sizeof(char *));
    pp->undefines[pp->undefine_count++] = undefine;
}

/* These functions are kept for potential future use with libclang API */
#if 0
static char **build_clang_args(Preprocessor *pp, size_t *arg_count) {
    size_t capacity = 32;
    size_t count = 0;
    char **args = xmalloc(capacity * sizeof(char *));
    
    /* Add standard flags */
    args[count++] = xstrdup("-E");  /* Preprocess only */
    args[count++] = xstrdup("-C");  /* Keep comments if requested */
    
    if (!pp->options.expand_macros) {
        args[count++] = xstrdup("-dM");  /* Dump macros */
    }
    
    /* Add target triple if specified */
    if (pp->options.target_triple) {
        args[count++] = xstrdup("-target");
        args[count++] = xstrdup(pp->options.target_triple);
    }
    
    /* Add include paths */
    for (size_t i = 0; i < pp->include_path_count; i++) {
        if (count + 2 >= capacity) {
            capacity *= 2;
            args = xrealloc(args, capacity * sizeof(char *));
        }
        args[count++] = xstrdup("-I");
        args[count++] = xstrdup(pp->include_paths[i]);
    }
    
    /* Add system include paths */
    for (size_t i = 0; i < pp->system_include_path_count; i++) {
        if (count + 2 >= capacity) {
            capacity *= 2;
            args = xrealloc(args, capacity * sizeof(char *));
        }
        args[count++] = xstrdup("-isystem");
        args[count++] = xstrdup(pp->system_include_paths[i]);
    }
    
    /* Add defines */
    for (size_t i = 0; i < pp->define_count; i++) {
        if (count + 1 >= capacity) {
            capacity *= 2;
            args = xrealloc(args, capacity * sizeof(char *));
        }
        args[count++] = xstrdup(pp->defines[i]);
    }
    
    /* Add undefines */
    for (size_t i = 0; i < pp->undefine_count; i++) {
        if (count + 1 >= capacity) {
            capacity *= 2;
            args = xrealloc(args, capacity * sizeof(char *));
        }
        args[count++] = xstrdup(pp->undefines[i]);
    }
    
    *arg_count = count;
    return args;
}

static void free_clang_args(char **args, size_t count) {
    for (size_t i = 0; i < count; i++) {
        xfree(args[i]);
    }
    xfree(args);
}
#endif

char *preprocessor_process_file(Preprocessor *pp, const char *filename) {
    if (!pp || !filename) {
        if (pp) {
            xfree(pp->last_error);
            pp->last_error = xstrdup("Invalid filename");
        }
        return NULL;
    }
    
    /* Build command using execvp-style array to avoid shell escaping issues */
    size_t argc = 0;
    char **argv = xmalloc(256 * sizeof(char *));
    
    argv[argc++] = xstrdup("clang");
    argv[argc++] = xstrdup("-E");
    argv[argc++] = xstrdup("-P");  /* Don't generate line markers */
    
    /* Add include paths */
    for (size_t i = 0; i < pp->include_path_count; i++) {
        char *arg = xmalloc(strlen(pp->include_paths[i]) + 3);
        snprintf(arg, strlen(pp->include_paths[i]) + 3, "-I%s", pp->include_paths[i]);
        argv[argc++] = arg;
    }
    
    /* Add system include paths */
    for (size_t i = 0; i < pp->system_include_path_count; i++) {
        argv[argc++] = xstrdup("-isystem");
        argv[argc++] = xstrdup(pp->system_include_paths[i]);
    }
    
    /* Add defines */
    for (size_t i = 0; i < pp->define_count; i++) {
        argv[argc++] = xstrdup(pp->defines[i]);
    }
    
    /* Add undefines */
    for (size_t i = 0; i < pp->undefine_count; i++) {
        argv[argc++] = xstrdup(pp->undefines[i]);
    }
    
    argv[argc++] = xstrdup(filename);
    argv[argc] = NULL;
    
    /* Build command string for popen (properly escaped) */
    size_t cmd_len = 0;
    for (size_t i = 0; i < argc; i++) {
        cmd_len += strlen(argv[i]) + 3;  /* quotes + space */
    }
    char *cmd = xmalloc(cmd_len + 1);
    cmd[0] = '\0';
    
    for (size_t i = 0; i < argc; i++) {
        if (i > 0) strcat(cmd, " ");
        
        /* Check if argument needs quoting - quote all defines to be safe */
        bool needs_quote = strchr(argv[i], ' ') != NULL || 
                          strchr(argv[i], '(') != NULL ||
                          strchr(argv[i], ')') != NULL ||
                          strchr(argv[i], '>') != NULL ||
                          strchr(argv[i], '<') != NULL ||
                          strchr(argv[i], '=') != NULL ||
                          (argv[i][0] == '-' && argv[i][1] == 'D');  /* Always quote defines */
        
        if (needs_quote) {
            strcat(cmd, "'");
            /* Escape single quotes in the argument */
            for (const char *p = argv[i]; *p; p++) {
                if (*p == '\'') {
                    strcat(cmd, "'\\''");
                } else {
                    size_t len = strlen(cmd);
                    cmd[len] = *p;
                    cmd[len + 1] = '\0';
                }
            }
            strcat(cmd, "'");
        } else {
            strcat(cmd, argv[i]);
        }
    }
    
    /* Free argv */
    for (size_t i = 0; i < argc; i++) {
        xfree(argv[i]);
    }
    xfree(argv);
    
    FILE *fp = popen(cmd, "r");
    xfree(cmd);
    
    if (!fp) {
        xfree(pp->last_error);
        pp->last_error = xstrdup("Failed to run preprocessor");
        return NULL;
    }
    
    /* Read output */
    size_t capacity = 4096;
    size_t size = 0;
    char *output = xmalloc(capacity);
    
    while (!feof(fp)) {
        if (size + 1024 >= capacity) {
            capacity *= 2;
            output = xrealloc(output, capacity);
        }
        size_t read = fread(output + size, 1, 1024, fp);
        size += read;
    }
    
    output[size] = '\0';
    int status = pclose(fp);
    
    if (status != 0) {
        xfree(pp->last_error);
        pp->last_error = xstrdup("Preprocessor failed");
        xfree(output);
        return NULL;
    }
    
    return output;
}

char *preprocessor_process_string(Preprocessor *pp, const char *source, const char *filename) {
    if (!pp || !source) {
        if (pp) {
            xfree(pp->last_error);
            pp->last_error = xstrdup("Invalid source");
        }
        return NULL;
    }
    
    /* Write source to temporary file */
    const char *tmpfile = filename ? filename : "/tmp/llvm-c-preprocess.c";
    FILE *fp = fopen(tmpfile, "w");
    if (!fp) {
        xfree(pp->last_error);
        pp->last_error = xstrdup("Failed to create temporary file");
        return NULL;
    }
    
    fputs(source, fp);
    fclose(fp);
    
    /* Process the temporary file */
    char *result = preprocessor_process_file(pp, tmpfile);
    
    /* Clean up if we created a temp file */
    if (!filename) {
        remove(tmpfile);
    }
    
    return result;
}

const char *preprocessor_get_error(Preprocessor *pp) {
    return pp ? pp->last_error : "invalid preprocessor";
}

bool preprocessor_has_error(Preprocessor *pp) {
    return pp && pp->last_error != NULL;
}
