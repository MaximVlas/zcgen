#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>
#include "../common/types.h"
#include "backend.h"

/* Codegen context */
typedef struct CodegenContext {
    Backend *backend;
    BackendContext *backend_ctx;
    void *current_module;
    void *current_function;
    
    /* Symbol tables */
    void *global_symbols;
    void *local_symbols;
    
    /* Type cache */
    void *type_cache;
    
    /* Options */
    int opt_level;              /* 0-3 */
    bool debug_info;
    bool pic;                   /* Position independent code */
    const char *target_triple;
    const char *target_cpu;
    const char **target_features;
    size_t target_feature_count;
} CodegenContext;

/* Initialize codegen */
CodegenContext *codegen_init(BackendType backend_type, const char *target_triple);
void codegen_destroy(CodegenContext *ctx);

/* Set options */
void codegen_set_opt_level(CodegenContext *ctx, int level);
void codegen_set_debug_info(CodegenContext *ctx, bool enable);
void codegen_set_pic(CodegenContext *ctx, bool enable);

/* Generate code from AST */
bool codegen_generate(CodegenContext *ctx, ASTNode *ast, const char *module_name);

/* Emit output */
bool codegen_emit_object(CodegenContext *ctx, const char *filename);
bool codegen_emit_assembly(CodegenContext *ctx, const char *filename);
bool codegen_emit_llvm_ir(CodegenContext *ctx, const char *filename);
bool codegen_emit_bitcode(CodegenContext *ctx, const char *filename);

/* Link */
bool codegen_link(CodegenContext *ctx, const char **object_files, size_t count,
                 const char *output, bool is_shared);

/* Error handling */
const char *codegen_get_error(CodegenContext *ctx);

#endif /* CODEGEN_H */
