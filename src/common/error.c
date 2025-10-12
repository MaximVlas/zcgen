#include "error.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Global state */
static int error_cnt = 0;
static int warning_cnt = 0;
static DiagnosticOptions diag_opts;

/* Source file cache for showing snippets */
#define MAX_SOURCE_FILES 256
typedef struct {
    char *filename;
    char *source;
} SourceFile;

static SourceFile source_files[MAX_SOURCE_FILES];
static int source_file_count = 0;

/* Initialize diagnostic system */
void diagnostic_init(void) {
    /* Default options - LLVM-style */
    diag_opts.use_color = isatty(STDERR_FILENO);
    diag_opts.show_source_location = true;
    diag_opts.show_source_snippet = true;
    diag_opts.show_caret = true;
    diag_opts.show_column = true;
    diag_opts.show_line_numbers = true;
    diag_opts.context_lines = 0;
    diag_opts.show_option_name = true;
    diag_opts.show_fix_hints = true;
}

void diagnostic_set_options(DiagnosticOptions *opts) {
    if (opts) {
        diag_opts = *opts;
    }
}

DiagnosticOptions *diagnostic_get_options(void) {
    return &diag_opts;
}

/* Source management */
void diagnostic_set_source(const char *filename, const char *source) {
    if (source_file_count >= MAX_SOURCE_FILES) return;
    
    /* Check if already exists */
    for (int i = 0; i < source_file_count; i++) {
        if (strcmp(source_files[i].filename, filename) == 0) {
            xfree(source_files[i].source);
            source_files[i].source = xstrdup(source);
            return;
        }
    }
    
    /* Add new */
    source_files[source_file_count].filename = xstrdup(filename);
    source_files[source_file_count].source = xstrdup(source);
    source_file_count++;
}

void diagnostic_clear_source(const char *filename) {
    for (int i = 0; i < source_file_count; i++) {
        if (strcmp(source_files[i].filename, filename) == 0) {
            xfree(source_files[i].filename);
            xfree(source_files[i].source);
            /* Shift remaining */
            for (int j = i; j < source_file_count - 1; j++) {
                source_files[j] = source_files[j + 1];
            }
            source_file_count--;
            return;
        }
    }
}

static const char *get_source_for_file(const char *filename) {
    for (int i = 0; i < source_file_count; i++) {
        if (strcmp(source_files[i].filename, filename) == 0) {
            return source_files[i].source;
        }
    }
    return NULL;
}

/* Color helpers */
static const char *color_for_level(DiagnosticLevel level) {
    if (!diag_opts.use_color) return "";
    
    switch (level) {
        case DIAG_ERROR:
        case DIAG_FATAL:
            return COLOR_BOLD COLOR_RED;
        case DIAG_WARNING:
            return COLOR_BOLD COLOR_MAGENTA;
        case DIAG_NOTE:
            return COLOR_BOLD COLOR_CYAN;
        case DIAG_REMARK:
            return COLOR_BOLD COLOR_BLUE;
        default:
            return "";
    }
}

static const char *level_string(DiagnosticLevel level) {
    switch (level) {
        case DIAG_ERROR: return "error";
        case DIAG_WARNING: return "warning";
        case DIAG_NOTE: return "note";
        case DIAG_REMARK: return "remark";
        case DIAG_FATAL: return "fatal error";
        default: return "diagnostic";
    }
}

/* Get line from source */
static const char *get_line_from_source(const char *source, uint32_t line_num, size_t *len) {
    if (!source) return NULL;
    
    uint32_t current_line = 1;
    const char *line_start = source;
    const char *p = source;
    
    while (*p && current_line < line_num) {
        if (*p == '\n') {
            current_line++;
            line_start = p + 1;
        }
        p++;
    }
    
    if (current_line != line_num) return NULL;
    
    /* Find line end */
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n') {
        line_end++;
    }
    
    *len = line_end - line_start;
    return line_start;
}

/* Print source snippet with caret */
static void print_source_snippet(SourceLocation loc) {
    if (!diag_opts.show_source_snippet) return;
    
    const char *source = get_source_for_file(loc.filename);
    if (!source) return;
    
    size_t line_len;
    const char *line = get_line_from_source(source, loc.line, &line_len);
    if (!line) return;
    
    /* Print line number if enabled */
    if (diag_opts.show_line_numbers) {
        if (diag_opts.use_color) {
            fprintf(stderr, "%s%5u | %s", COLOR_BOLD, loc.line, COLOR_RESET);
        } else {
            fprintf(stderr, "%5u | ", loc.line);
        }
    }
    
    /* Print source line */
    fprintf(stderr, "%.*s\n", (int)line_len, line);
    
    /* Print caret if enabled */
    if (diag_opts.show_caret && loc.column > 0) {
        if (diag_opts.show_line_numbers) {
            fprintf(stderr, "      | ");
        }
        
        /* Print spaces up to column */
        for (uint32_t i = 1; i < loc.column; i++) {
            fprintf(stderr, " ");
        }
        
        /* Print caret */
        if (diag_opts.use_color) {
            fprintf(stderr, "%s^%s\n", COLOR_BOLD COLOR_GREEN, COLOR_RESET);
        } else {
            fprintf(stderr, "^\n");
        }
    }
}

/* Main diagnostic emission */
void diagnostic_emit(DiagnosticLevel level, SourceLocation loc, const char *fmt, ...) {
    const char *color = color_for_level(level);
    const char *reset = diag_opts.use_color ? COLOR_RESET : "";
    
    /* Print location */
    if (diag_opts.show_source_location) {
        fprintf(stderr, "%s%s:%u:%u: %s",
                diag_opts.use_color ? COLOR_BOLD : "",
                loc.filename ? loc.filename : "<unknown>",
                loc.line,
                diag_opts.show_column ? loc.column : 0,
                reset);
        fprintf(stderr, " ");
    }
    
    /* Print level */
    fprintf(stderr, "%s%s:%s ", color, level_string(level), reset);
    
    /* Print message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    /* Print source snippet */
    print_source_snippet(loc);
    
    /* Update counters */
    if (level == DIAG_ERROR || level == DIAG_FATAL) {
        error_cnt++;
    } else if (level == DIAG_WARNING) {
        warning_cnt++;
    }
    
    if (level == DIAG_FATAL) {
        exit(EXIT_FAILURE);
    }
}

void diagnostic_emit_range(DiagnosticLevel level, SourceLocation start, 
                          SourceLocation end, const char *fmt, ...) {
    const char *color = color_for_level(level);
    const char *reset = diag_opts.use_color ? COLOR_RESET : "";
    
    /* Print location range */
    if (diag_opts.show_source_location) {
        fprintf(stderr, "%s%s:%u:%u-%u:%u: %s",
                diag_opts.use_color ? COLOR_BOLD : "",
                start.filename ? start.filename : "<unknown>",
                start.line, start.column,
                end.line, end.column,
                reset);
        fprintf(stderr, " ");
    }
    
    /* Print level */
    fprintf(stderr, "%s%s:%s ", color, level_string(level), reset);
    
    /* Print message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    /* Print source snippet for start location */
    print_source_snippet(start);
    
    /* Update counters */
    if (level == DIAG_ERROR || level == DIAG_FATAL) {
        error_cnt++;
    } else if (level == DIAG_WARNING) {
        warning_cnt++;
    }
}

/* Convenience wrappers */
static const char *error_type_str(ErrorType type) {
    switch (type) {
        case ERROR_LEXER: return "lexer error";
        case ERROR_PARSER: return "parser error";
        case ERROR_SEMANTIC: return "semantic error";
        case ERROR_CODEGEN: return "codegen error";
        case ERROR_INTERNAL: return "internal error";
        default: return "error";
    }
}

void error_report(ErrorType type, SourceLocation loc, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit(DIAG_ERROR, loc, "%s", buffer);
}

void error_report_range(ErrorType type, SourceLocation start, SourceLocation end, 
                       const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit_range(DIAG_ERROR, start, end, "%s", buffer);
}

void error_warning(SourceLocation loc, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit(DIAG_WARNING, loc, "%s", buffer);
}

void error_note(SourceLocation loc, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit(DIAG_NOTE, loc, "%s", buffer);
}

void error_remark(SourceLocation loc, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit(DIAG_REMARK, loc, "%s", buffer);
}

void diagnostic_add_fixit(SourceLocation loc, const char *replacement) {
    if (!diag_opts.show_fix_hints) return;
    
    const char *color = diag_opts.use_color ? COLOR_BOLD COLOR_GREEN : "";
    const char *reset = diag_opts.use_color ? COLOR_RESET : "";
    
    fprintf(stderr, "%sfix-it hint:%s replace with '%s'\n", color, reset, replacement);
}

void diagnostic_add_note(SourceLocation loc, const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    diagnostic_emit(DIAG_NOTE, loc, "%s", buffer);
}

int error_count(void) {
    return error_cnt;
}

int warning_count(void) {
    return warning_cnt;
}

void error_reset(void) {
    error_cnt = 0;
    warning_cnt = 0;
}

void error_fatal(const char *fmt, ...) {
    const char *color = diag_opts.use_color ? COLOR_BOLD COLOR_RED : "";
    const char *reset = diag_opts.use_color ? COLOR_RESET : "";
    
    fprintf(stderr, "%sfatal error:%s ", color, reset);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

