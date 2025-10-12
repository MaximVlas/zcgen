#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stddef.h>
#include <stdbool.h>

/* Preprocessor context */
typedef struct Preprocessor Preprocessor;

/* Preprocessor options */
typedef struct {
    bool keep_comments;
    bool keep_whitespace;
    bool expand_macros;
    const char *target_triple;  /* e.g., "x86_64-pc-linux-gnu" */
} PreprocessorOptions;

/* Initialize preprocessor (using Clang's preprocessor) */
Preprocessor *preprocessor_create(PreprocessorOptions *opts);
void preprocessor_destroy(Preprocessor *pp);

/* Add include paths */
void preprocessor_add_include_path(Preprocessor *pp, const char *path);
void preprocessor_add_system_include_path(Preprocessor *pp, const char *path);

/* Define/undefine macros */
void preprocessor_define(Preprocessor *pp, const char *name, const char *value);
void preprocessor_undefine(Preprocessor *pp, const char *name);

/* Preprocess file or string */
char *preprocessor_process_file(Preprocessor *pp, const char *filename);
char *preprocessor_process_string(Preprocessor *pp, const char *source, const char *filename);

/* Error handling */
const char *preprocessor_get_error(Preprocessor *pp);
bool preprocessor_has_error(Preprocessor *pp);

#endif /* PREPROCESSOR_H */
