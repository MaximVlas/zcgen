#ifndef ERROR_H
#define ERROR_H

#include "types.h"
#include <stdarg.h>

typedef enum {
    ERROR_LEXER,
    ERROR_PARSER,
    ERROR_SEMANTIC,
    ERROR_CODEGEN,
    ERROR_INTERNAL
} ErrorType;

typedef enum {
    DIAG_ERROR,
    DIAG_WARNING,
    DIAG_NOTE,
    DIAG_REMARK,
    DIAG_FATAL
} DiagnosticLevel;

/* Diagnostic options (LLVM-style) */
typedef struct {
    bool use_color;              /* Use ANSI color codes */
    bool show_source_location;   /* Show file:line:column */
    bool show_source_snippet;    /* Show source code snippet */
    bool show_caret;             /* Show ^ caret under error */
    bool show_column;            /* Show column numbers */
    bool show_line_numbers;      /* Show line numbers in snippets */
    int context_lines;           /* Lines of context to show */
    bool show_option_name;       /* Show -W flag name for warnings */
    bool show_fix_hints;         /* Show "did you mean?" suggestions */
} DiagnosticOptions;

/* Initialize diagnostic system */
void diagnostic_init(void);
void diagnostic_set_options(DiagnosticOptions *opts);
DiagnosticOptions *diagnostic_get_options(void);

/* LLVM-style error reporting with source snippets */
void error_report(ErrorType type, SourceLocation loc, const char *fmt, ...);
void error_report_range(ErrorType type, SourceLocation start, SourceLocation end, 
                       const char *fmt, ...);
void error_warning(SourceLocation loc, const char *fmt, ...);
void error_note(SourceLocation loc, const char *fmt, ...);
void error_remark(SourceLocation loc, const char *fmt, ...);

/* Advanced diagnostics */
void diagnostic_emit(DiagnosticLevel level, SourceLocation loc, const char *fmt, ...);
void diagnostic_emit_range(DiagnosticLevel level, SourceLocation start, 
                          SourceLocation end, const char *fmt, ...);

/* Add fix-it hints */
void diagnostic_add_fixit(SourceLocation loc, const char *replacement);
void diagnostic_add_note(SourceLocation loc, const char *fmt, ...);

/* Source context management */
void diagnostic_set_source(const char *filename, const char *source);
void diagnostic_clear_source(const char *filename);

/* Error statistics */
int error_count(void);
int warning_count(void);
void error_reset(void);

/* Fatal error */
void error_fatal(const char *fmt, ...);

/* Color codes (ANSI) */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

#endif /* ERROR_H */
