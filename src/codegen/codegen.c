#include "codegen.h"
#include "../common/memory.h"
#include "../common/error.h"

/* Stub implementation - to be completed */

CodegenContext *codegen_init(BackendType backend_type, const char *target_triple) {
    CodegenContext *ctx = xcalloc(1, sizeof(CodegenContext));
    
    /* Get backend */
    ctx->backend = backend_get(backend_type);
    if (!ctx->backend) {
        error_fatal("backend not available: %d", backend_type);
    }
    
    /* Initialize backend */
    ctx->backend_ctx = ctx->backend->init(target_triple, NULL, NULL, 0);
    if (!ctx->backend_ctx) {
        xfree(ctx);
        return NULL;
    }
    
    ctx->opt_level = 0;
    ctx->debug_info = false;
    ctx->pic = false;
    ctx->target_triple = target_triple;
    
    return ctx;
}

void codegen_destroy(CodegenContext *ctx) {
    if (!ctx) return;
    
    if (ctx->backend && ctx->backend_ctx) {
        ctx->backend->destroy(ctx->backend_ctx);
    }
    
    xfree(ctx);
}

void codegen_set_opt_level(CodegenContext *ctx, int level) {
    if (ctx) {
        ctx->opt_level = level;
    }
}

void codegen_set_debug_info(CodegenContext *ctx, bool enable) {
    if (ctx) {
        ctx->debug_info = enable;
    }
}

void codegen_set_pic(CodegenContext *ctx, bool enable) {
    if (ctx) {
        ctx->pic = enable;
    }
}

bool codegen_generate(CodegenContext *ctx, ASTNode *ast, const char *module_name) {
    if (!ctx || !ast || !ctx->backend) return false;
    
    /* Create module */
    ctx->current_module = ctx->backend->create_module(ctx->backend_ctx, module_name);
    if (!ctx->current_module) return false;
    
    /* Generate code for translation unit */
    ctx->backend->codegen_decl(ctx->backend_ctx, ast);
    
    /* Optimize if requested */
    if (ctx->opt_level > 0) {
        ctx->backend->optimize(ctx->backend_ctx, ctx->current_module, ctx->opt_level);
    }
    
    return true;
}

bool codegen_emit_object(CodegenContext *ctx, const char *filename) {
    if (!ctx || !ctx->backend || !ctx->current_module) return false;
    return ctx->backend->emit_object(ctx->backend_ctx, ctx->current_module, filename);
}

bool codegen_emit_assembly(CodegenContext *ctx, const char *filename) {
    if (!ctx || !ctx->backend || !ctx->current_module) return false;
    return ctx->backend->emit_assembly(ctx->backend_ctx, ctx->current_module, filename);
}

bool codegen_emit_llvm_ir(CodegenContext *ctx, const char *filename) {
    if (!ctx || !ctx->backend || !ctx->current_module) return false;
    if (!ctx->backend->emit_llvm_ir) return false;
    return ctx->backend->emit_llvm_ir(ctx->backend_ctx, ctx->current_module, filename);
}

bool codegen_emit_bitcode(CodegenContext *ctx, const char *filename) {
    if (!ctx || !ctx->backend || !ctx->current_module) return false;
    if (!ctx->backend->emit_bitcode) return false;
    return ctx->backend->emit_bitcode(ctx->backend_ctx, ctx->current_module, filename);
}

bool codegen_link(CodegenContext *ctx, const char **object_files, size_t count,
                 const char *output, bool is_shared) {
    if (!ctx || !ctx->backend) return false;
    return ctx->backend->link(ctx->backend_ctx, object_files, count, output, is_shared);
}

const char *codegen_get_error(CodegenContext *ctx) {
    if (!ctx || !ctx->backend || !ctx->backend_ctx) return "invalid context";
    return ctx->backend->get_last_error(ctx->backend_ctx);
}
