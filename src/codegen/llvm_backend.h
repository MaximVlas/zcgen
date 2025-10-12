#ifndef LLVM_BACKEND_H
#define LLVM_BACKEND_H

#include "backend.h"

/* LLVM backend implementation */

/* Create LLVM backend */
Backend *llvm_backend_create(void);

/* LLVM-specific context */
typedef struct LLVMBackendContext LLVMBackendContext;

/* LLVM backend functions */
BackendContext *llvm_backend_init(const char *target_triple, const char *cpu,
                                   const char **features, size_t feature_count);
void llvm_backend_destroy(BackendContext *ctx);

/* Module operations */
void *llvm_create_module(BackendContext *ctx, const char *name);
void llvm_destroy_module(BackendContext *ctx, void *module);

/* Function operations */
void *llvm_create_function(BackendContext *ctx, void *module, const char *name,
                           void *return_type, void **param_types, size_t param_count);
void llvm_set_function_body(BackendContext *ctx, void *function, ASTNode *body);

/* Type operations */
void *llvm_get_void_type(BackendContext *ctx);
void *llvm_get_int_type(BackendContext *ctx, int bits, bool is_signed);
void *llvm_get_float_type(BackendContext *ctx, int bits);
void *llvm_get_pointer_type(BackendContext *ctx, void *pointee);
void *llvm_get_array_type(BackendContext *ctx, void *element, size_t count);
void *llvm_get_struct_type(BackendContext *ctx, void **fields, size_t field_count);
void *llvm_get_function_type(BackendContext *ctx, void *return_type,
                             void **param_types, size_t param_count);

/* Code generation */
void *llvm_codegen_expr(BackendContext *ctx, ASTNode *expr);
void llvm_codegen_stmt(BackendContext *ctx, ASTNode *stmt);
void llvm_codegen_decl(BackendContext *ctx, ASTNode *decl);

/* Optimization */
void llvm_optimize(BackendContext *ctx, void *module, int opt_level);

/* Output */
bool llvm_emit_object(BackendContext *ctx, void *module, const char *filename);
bool llvm_emit_assembly(BackendContext *ctx, void *module, const char *filename);
bool llvm_emit_llvm_ir(BackendContext *ctx, void *module, const char *filename);
bool llvm_emit_bitcode(BackendContext *ctx, void *module, const char *filename);

/* Linking */
bool llvm_link(BackendContext *ctx, const char **object_files, size_t count,
              const char *output, bool is_shared);

/* Capabilities */
BackendCapabilities *llvm_get_capabilities(BackendContext *ctx);

/* Error handling */
const char *llvm_get_last_error(BackendContext *ctx);

#endif /* LLVM_BACKEND_H */
