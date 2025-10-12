#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>
#include "../common/types.h"

/* Backend types - pluggable code generation */
typedef enum {
    BACKEND_LLVM,      /* LLVM IR backend */
    BACKEND_RUST,      /* Rust codegen (via rustc_codegen_ssa if available) */
    BACKEND_ZIG,       /* Zig backend (via libzig if available) */
    BACKEND_C,         /* C transpiler backend */
    BACKEND_CUSTOM     /* Custom backend */
} BackendType;

/* Backend capabilities */
typedef struct {
    bool supports_optimization;
    bool supports_debug_info;
    bool supports_inline_asm;
    bool supports_simd;
    bool supports_atomics;
    bool supports_threads;
    bool supports_exceptions;
    bool supports_cross_compilation;
    const char **supported_targets;
    size_t target_count;
} BackendCapabilities;

/* Backend context - opaque handle */
typedef struct BackendContext BackendContext;

/* Backend interface - function pointers for pluggability */
typedef struct Backend {
    BackendType type;
    const char *name;
    const char *version;
    
    /* Initialization */
    BackendContext *(*init)(const char *target_triple, const char *cpu, 
                           const char **features, size_t feature_count);
    void (*destroy)(BackendContext *ctx);
    
    /* Module operations */
    void *(*create_module)(BackendContext *ctx, const char *name);
    void (*destroy_module)(BackendContext *ctx, void *module);
    
    /* Function operations */
    void *(*create_function)(BackendContext *ctx, void *module, const char *name,
                            void *return_type, void **param_types, size_t param_count);
    void (*set_function_body)(BackendContext *ctx, void *function, ASTNode *body);
    
    /* Type operations */
    void *(*get_void_type)(BackendContext *ctx);
    void *(*get_int_type)(BackendContext *ctx, int bits, bool is_signed);
    void *(*get_float_type)(BackendContext *ctx, int bits);
    void *(*get_pointer_type)(BackendContext *ctx, void *pointee);
    void *(*get_array_type)(BackendContext *ctx, void *element, size_t count);
    void *(*get_struct_type)(BackendContext *ctx, void **fields, size_t field_count);
    void *(*get_function_type)(BackendContext *ctx, void *return_type,
                              void **param_types, size_t param_count);
    
    /* Code generation from AST */
    void *(*codegen_expr)(BackendContext *ctx, ASTNode *expr);
    void (*codegen_stmt)(BackendContext *ctx, ASTNode *stmt);
    void (*codegen_decl)(BackendContext *ctx, ASTNode *decl);
    
    /* Optimization */
    void (*optimize)(BackendContext *ctx, void *module, int opt_level);
    
    /* Output */
    bool (*emit_object)(BackendContext *ctx, void *module, const char *filename);
    bool (*emit_assembly)(BackendContext *ctx, void *module, const char *filename);
    bool (*emit_llvm_ir)(BackendContext *ctx, void *module, const char *filename);
    bool (*emit_bitcode)(BackendContext *ctx, void *module, const char *filename);
    
    /* Linking */
    bool (*link)(BackendContext *ctx, const char **object_files, size_t count,
                const char *output, bool is_shared);
    
    /* Capabilities */
    BackendCapabilities *(*get_capabilities)(BackendContext *ctx);
    
    /* Error handling */
    const char *(*get_last_error)(BackendContext *ctx);
} Backend;

/* Backend registry */
void backend_register(Backend *backend);
Backend *backend_get(BackendType type);
Backend *backend_get_by_name(const char *name);
void backend_list_all(Backend ***backends, size_t *count);

/* Built-in backends */
Backend *backend_llvm_create(void);
Backend *backend_rust_create(void);   /* If rustc available */
Backend *backend_zig_create(void);    /* If libzig available */
Backend *backend_c_create(void);      /* C transpiler */

/* Backend selection */
Backend *backend_select_default(void);
Backend *backend_select_for_target(const char *target_triple);

#endif /* BACKEND_H */
