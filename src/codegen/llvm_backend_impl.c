#include "llvm_backend.h"
#include "../common/memory.h"
#include "../common/error.h"
#include <string.h>
#include <stdio.h>

/* LLVM C API headers */
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Vectorize.h>

/* LLVM backend context */
typedef struct LLVMBackendContext {
    LLVMContextRef llvm_context;
    LLVMModuleRef llvm_module;
    LLVMBuilderRef llvm_builder;
    LLVMTargetMachineRef target_machine;
    LLVMPassManagerRef function_pass_manager;
    LLVMPassManagerRef module_pass_manager;
    
    /* Symbol tables for code generation */
    void *named_values;  /* Hash table: name -> LLVMValueRef */
    void *type_cache;    /* Hash table: type_name -> LLVMTypeRef */
    
    /* Current function being generated */
    LLVMValueRef current_function;
    LLVMBasicBlockRef current_block;
    
    /* Error handling */
    char *last_error;
} LLVMBackendContext;

/* Helper: Set error message */
static void set_error(LLVMBackendContext *ctx, const char *fmt, ...) {
    if (!ctx) return;
    
    if (ctx->last_error) {
        xfree(ctx->last_error);
    }
    
    va_list args;
    va_start(args, fmt);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    ctx->last_error = xstrdup(buffer);
    
    va_end(args);
}

/* ===== LIFECYCLE ===== */

BackendContext *llvm_backend_init(const char *target_triple, const char *cpu,
                                   const char **features, size_t feature_count) {
    /* Initialize LLVM */
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    
    LLVMBackendContext *ctx = xcalloc(1, sizeof(LLVMBackendContext));
    
    /* Create LLVM context */
    ctx->llvm_context = LLVMContextCreate();
    if (!ctx->llvm_context) {
        xfree(ctx);
        return NULL;
    }
    
    /* Create builder */
    ctx->llvm_builder = LLVMCreateBuilderInContext(ctx->llvm_context);
    if (!ctx->llvm_builder) {
        LLVMContextDispose(ctx->llvm_context);
        xfree(ctx);
        return NULL;
    }
    
    /* Setup target machine if triple provided */
    if (target_triple) {
        char *error = NULL;
        LLVMTargetRef target;
        
        if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
            fprintf(stderr, "Error getting target: %s\n", error);
            LLVMDisposeMessage(error);
            /* Continue with default target */
        } else {
            const char *target_cpu = cpu ? cpu : "generic";
            const char *target_features = "";
            
            ctx->target_machine = LLVMCreateTargetMachine(
                target, target_triple, target_cpu, target_features,
                LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault
            );
        }
    }
    
    return (BackendContext *)ctx;
}

void llvm_backend_destroy(BackendContext *ctx_opaque) {
    if (!ctx_opaque) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    if (ctx->function_pass_manager) {
        LLVMDisposePassManager(ctx->function_pass_manager);
    }
    
    if (ctx->module_pass_manager) {
        LLVMDisposePassManager(ctx->module_pass_manager);
    }
    
    if (ctx->target_machine) {
        LLVMDisposeTargetMachine(ctx->target_machine);
    }
    
    if (ctx->llvm_builder) {
        LLVMDisposeBuilder(ctx->llvm_builder);
    }
    
    if (ctx->llvm_module) {
        LLVMDisposeModule(ctx->llvm_module);
    }
    
    if (ctx->llvm_context) {
        LLVMContextDispose(ctx->llvm_context);
    }
    
    if (ctx->last_error) {
        xfree(ctx->last_error);
    }
    
    xfree(ctx);
}

/* ===== MODULE OPERATIONS ===== */

void *llvm_create_module(BackendContext *ctx_opaque, const char *name) {
    if (!ctx_opaque) return NULL;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    ctx->llvm_module = LLVMModuleCreateWithNameInContext(name, ctx->llvm_context);
    
    /* Create pass managers */
    ctx->function_pass_manager = LLVMCreateFunctionPassManagerForModule(ctx->llvm_module);
    ctx->module_pass_manager = LLVMCreatePassManager();
    
    return ctx->llvm_module;
}

void llvm_destroy_module(BackendContext *ctx_opaque, void *module) {
    if (!ctx_opaque || !module) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    if (ctx->llvm_module == module) {
        ctx->llvm_module = NULL;
    }
    
    LLVMDisposeModule((LLVMModuleRef)module);
}

/* ===== TYPE OPERATIONS ===== */

void *llvm_get_void_type(BackendContext *ctx_opaque) {
    if (!ctx_opaque) return NULL;
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    return LLVMVoidTypeInContext(ctx->llvm_context);
}

void *llvm_get_int_type(BackendContext *ctx_opaque, int bits, bool is_signed) {
    if (!ctx_opaque) return NULL;
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    (void)is_signed; /* Signedness is handled by operations, not type */
    return LLVMIntTypeInContext(ctx->llvm_context, bits);
}

void *llvm_get_float_type(BackendContext *ctx_opaque, int bits) {
    if (!ctx_opaque) return NULL;
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    if (bits == 32) {
        return LLVMFloatTypeInContext(ctx->llvm_context);
    } else if (bits == 64) {
        return LLVMDoubleTypeInContext(ctx->llvm_context);
    } else if (bits == 80 || bits == 96) {
        return LLVMX86FP80TypeInContext(ctx->llvm_context);
    } else if (bits == 128) {
        return LLVMFP128TypeInContext(ctx->llvm_context);
    }
    
    return LLVMDoubleTypeInContext(ctx->llvm_context);
}

void *llvm_get_pointer_type(BackendContext *ctx_opaque, void *pointee) {
    if (!ctx_opaque) return NULL;
    
    /* In LLVM 15+, all pointers are opaque */
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    return LLVMPointerType((LLVMTypeRef)pointee, 0);
}

void *llvm_get_array_type(BackendContext *ctx_opaque, void *element, size_t count) {
    if (!ctx_opaque || !element) return NULL;
    return LLVMArrayType((LLVMTypeRef)element, count);
}

void *llvm_get_struct_type(BackendContext *ctx_opaque, void **fields, size_t field_count) {
    if (!ctx_opaque) return NULL;
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    return LLVMStructTypeInContext(ctx->llvm_context, (LLVMTypeRef *)fields, field_count, 0);
}

void *llvm_get_function_type(BackendContext *ctx_opaque, void *return_type,
                             void **param_types, size_t param_count) {
    if (!ctx_opaque || !return_type) return NULL;
    return LLVMFunctionType((LLVMTypeRef)return_type, (LLVMTypeRef *)param_types, param_count, 0);
}

/* ===== FUNCTION OPERATIONS ===== */

void *llvm_create_function(BackendContext *ctx_opaque, void *module, const char *name,
                           void *return_type, void **param_types, size_t param_count) {
    if (!ctx_opaque || !module || !name) return NULL;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    /* Create function type */
    LLVMTypeRef func_type = LLVMFunctionType(
        (LLVMTypeRef)return_type,
        (LLVMTypeRef *)param_types,
        param_count,
        0  /* not variadic */
    );
    
    /* Add function to module */
    LLVMValueRef function = LLVMAddFunction((LLVMModuleRef)module, name, func_type);
    
    /* Create entry basic block */
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->llvm_context, function, "entry");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, entry);
    
    ctx->current_function = function;
    ctx->current_block = entry;
    
    return function;
}

void llvm_set_function_body(BackendContext *ctx_opaque, void *function, ASTNode *body) {
    if (!ctx_opaque || !function || !body) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    ctx->current_function = (LLVMValueRef)function;
    
    /* Generate code for function body */
    llvm_codegen_stmt(ctx_opaque, body);
}

/* ===== CODE GENERATION HELPERS ===== */

static LLVMValueRef codegen_integer_literal(LLVMBackendContext *ctx, ASTNode *node) {
    LLVMTypeRef int_type = LLVMInt32TypeInContext(ctx->llvm_context);
    return LLVMConstInt(int_type, node->data.int_literal.value, 0);
}

static LLVMValueRef codegen_float_literal(LLVMBackendContext *ctx, ASTNode *node) {
    LLVMTypeRef float_type = LLVMDoubleTypeInContext(ctx->llvm_context);
    return LLVMConstReal(float_type, node->data.float_literal.value);
}

static LLVMValueRef codegen_binary_expr(LLVMBackendContext *ctx, ASTNode *node) {
    if (node->child_count < 2) return NULL;
    
    LLVMValueRef left = llvm_codegen_expr((BackendContext *)ctx, node->children[0]);
    LLVMValueRef right = llvm_codegen_expr((BackendContext *)ctx, node->children[1]);
    
    if (!left || !right) return NULL;
    
    switch (node->type) {
        case AST_ADD_EXPR:
            return LLVMBuildAdd(ctx->llvm_builder, left, right, "addtmp");
        case AST_SUB_EXPR:
            return LLVMBuildSub(ctx->llvm_builder, left, right, "subtmp");
        case AST_MUL_EXPR:
            return LLVMBuildMul(ctx->llvm_builder, left, right, "multmp");
        case AST_DIV_EXPR:
            return LLVMBuildSDiv(ctx->llvm_builder, left, right, "divtmp");
        case AST_MOD_EXPR:
            return LLVMBuildSRem(ctx->llvm_builder, left, right, "modtmp");
        case AST_AND_EXPR:
            return LLVMBuildAnd(ctx->llvm_builder, left, right, "andtmp");
        case AST_OR_EXPR:
            return LLVMBuildOr(ctx->llvm_builder, left, right, "ortmp");
        case AST_XOR_EXPR:
            return LLVMBuildXor(ctx->llvm_builder, left, right, "xortmp");
        case AST_SHL_EXPR:
            return LLVMBuildShl(ctx->llvm_builder, left, right, "shltmp");
        case AST_SHR_EXPR:
            return LLVMBuildAShr(ctx->llvm_builder, left, right, "shrtmp");
        case AST_EQ_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, left, right, "eqtmp");
        case AST_NE_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, left, right, "netmp");
        case AST_LT_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSLT, left, right, "lttmp");
        case AST_LE_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSLE, left, right, "letmp");
        case AST_GT_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSGT, left, right, "gttmp");
        case AST_GE_EXPR:
            return LLVMBuildICmp(ctx->llvm_builder, LLVMIntSGE, left, right, "getmp");
        default:
            return NULL;
    }
}

/* ===== CODE GENERATION ===== */

void *llvm_codegen_expr(BackendContext *ctx_opaque, ASTNode *expr) {
    if (!ctx_opaque || !expr) return NULL;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    switch (expr->type) {
        case AST_INTEGER_LITERAL:
            return codegen_integer_literal(ctx, expr);
            
        case AST_FLOAT_LITERAL:
            return codegen_float_literal(ctx, expr);
            
        case AST_ADD_EXPR:
        case AST_SUB_EXPR:
        case AST_MUL_EXPR:
        case AST_DIV_EXPR:
        case AST_MOD_EXPR:
        case AST_AND_EXPR:
        case AST_OR_EXPR:
        case AST_XOR_EXPR:
        case AST_SHL_EXPR:
        case AST_SHR_EXPR:
        case AST_EQ_EXPR:
        case AST_NE_EXPR:
        case AST_LT_EXPR:
        case AST_LE_EXPR:
        case AST_GT_EXPR:
        case AST_GE_EXPR:
            return codegen_binary_expr(ctx, expr);
            
        default:
            set_error(ctx, "Unsupported expression type: %d", expr->type);
            return NULL;
    }
}

void llvm_codegen_stmt(BackendContext *ctx_opaque, ASTNode *stmt) {
    if (!ctx_opaque || !stmt) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    switch (stmt->type) {
        case AST_COMPOUND_STMT:
            /* Generate code for each statement in the compound */
            for (size_t i = 0; i < stmt->child_count; i++) {
                llvm_codegen_stmt(ctx_opaque, stmt->children[i]);
            }
            break;
            
        case AST_RETURN_STMT:
            if (stmt->child_count > 0) {
                LLVMValueRef ret_val = llvm_codegen_expr(ctx_opaque, stmt->children[0]);
                if (ret_val) {
                    LLVMBuildRet(ctx->llvm_builder, ret_val);
                }
            } else {
                LLVMBuildRetVoid(ctx->llvm_builder);
            }
            break;
            
        case AST_EXPR_STMT:
            if (stmt->child_count > 0) {
                llvm_codegen_expr(ctx_opaque, stmt->children[0]);
            }
            break;
            
        default:
            set_error(ctx, "Unsupported statement type: %d", stmt->type);
            break;
    }
}

void llvm_codegen_decl(BackendContext *ctx_opaque, ASTNode *decl) {
    if (!ctx_opaque || !decl) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    switch (decl->type) {
        case AST_TRANSLATION_UNIT:
            /* Generate code for each declaration */
            for (size_t i = 0; i < decl->child_count; i++) {
                llvm_codegen_decl(ctx_opaque, decl->children[i]);
            }
            break;
            
        case AST_FUNCTION_DECL:
            /* TODO: Implement function declaration codegen */
            break;
            
        default:
            set_error(ctx, "Unsupported declaration type: %d", decl->type);
            break;
    }
}

/* ===== OPTIMIZATION ===== */

void llvm_optimize(BackendContext *ctx_opaque, void *module, int opt_level) {
    if (!ctx_opaque || !module) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    LLVMModuleRef mod = (LLVMModuleRef)module;
    
    if (opt_level == 0) return;  /* No optimization */
    
    /* Add optimization passes based on level */
    LLVMPassManagerRef pm = ctx->module_pass_manager;
    
    /* Always add verification pass */
    LLVMAddVerifierPass(pm);
    
    if (opt_level >= 1) {
        /* Basic optimizations */
        LLVMAddInstructionCombiningPass(pm);
        LLVMAddReassociatePass(pm);
        LLVMAddGVNPass(pm);
        LLVMAddCFGSimplificationPass(pm);
    }
    
    if (opt_level >= 2) {
        /* More aggressive optimizations */
        LLVMAddFunctionInliningPass(pm);
        LLVMAddGlobalOptimizerPass(pm);
        LLVMAddIPSCCPPass(pm);
        LLVMAddDeadArgEliminationPass(pm);
        LLVMAddAggressiveDCEPass(pm);
    }
    
    if (opt_level >= 3) {
        /* Maximum optimizations */
        LLVMAddLoopVectorizePass(pm);
        LLVMAddSLPVectorizePass(pm);
    }
    
    /* Run the passes */
    LLVMRunPassManager(pm, mod);
}

/* ===== OUTPUT ===== */

bool llvm_emit_object(BackendContext *ctx_opaque, void *module, const char *filename) {
    if (!ctx_opaque || !module || !filename) return false;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    if (!ctx->target_machine) {
        set_error(ctx, "No target machine configured");
        return false;
    }
    
    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(ctx->target_machine, (LLVMModuleRef)module,
                                    (char *)filename, LLVMObjectFile, &error)) {
        set_error(ctx, "Failed to emit object file: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    return true;
}

bool llvm_emit_assembly(BackendContext *ctx_opaque, void *module, const char *filename) {
    if (!ctx_opaque || !module || !filename) return false;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    if (!ctx->target_machine) {
        set_error(ctx, "No target machine configured");
        return false;
    }
    
    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(ctx->target_machine, (LLVMModuleRef)module,
                                    (char *)filename, LLVMAssemblyFile, &error)) {
        set_error(ctx, "Failed to emit assembly file: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    return true;
}

bool llvm_emit_llvm_ir(BackendContext *ctx_opaque, void *module, const char *filename) {
    if (!ctx_opaque || !module || !filename) return false;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    char *error = NULL;
    if (LLVMPrintModuleToFile((LLVMModuleRef)module, filename, &error)) {
        set_error(ctx, "Failed to emit LLVM IR: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    return true;
}

bool llvm_emit_bitcode(BackendContext *ctx_opaque, void *module, const char *filename) {
    if (!ctx_opaque || !module || !filename) return false;
    
    if (LLVMWriteBitcodeToFile((LLVMModuleRef)module, filename)) {
        return false;
    }
    
    return true;
}

/* ===== LINKING ===== */

bool llvm_link(BackendContext *ctx_opaque, const char **object_files, size_t count,
              const char *output, bool is_shared) {
    if (!ctx_opaque || !object_files || !output) return false;
    
    /* Use system linker (ld, lld, or clang) */
    char command[4096] = "clang";
    
    for (size_t i = 0; i < count; i++) {
        strcat(command, " ");
        strcat(command, object_files[i]);
    }
    
    strcat(command, " -o ");
    strcat(command, output);
    
    if (is_shared) {
        strcat(command, " -shared");
    }
    
    int result = system(command);
    return result == 0;
}

/* ===== ERROR HANDLING ===== */

const char *llvm_get_last_error(BackendContext *ctx_opaque) {
    if (!ctx_opaque) return "invalid context";
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    return ctx->last_error ? ctx->last_error : "no error";
}

/* ===== BACKEND REGISTRATION ===== */

Backend *backend_llvm_create(void) {
    Backend *backend = xcalloc(1, sizeof(Backend));
    
    backend->type = BACKEND_LLVM;
    backend->name = "LLVM";
    backend->version = "1.0.0";
    
    /* Lifecycle */
    backend->init = llvm_backend_init;
    backend->destroy = llvm_backend_destroy;
    
    /* Module operations */
    backend->create_module = llvm_create_module;
    backend->destroy_module = llvm_destroy_module;
    
    /* Function operations */
    backend->create_function = llvm_create_function;
    backend->set_function_body = llvm_set_function_body;
    
    /* Type operations */
    backend->get_void_type = llvm_get_void_type;
    backend->get_int_type = llvm_get_int_type;
    backend->get_float_type = llvm_get_float_type;
    backend->get_pointer_type = llvm_get_pointer_type;
    backend->get_array_type = llvm_get_array_type;
    backend->get_struct_type = llvm_get_struct_type;
    backend->get_function_type = llvm_get_function_type;
    
    /* Code generation */
    backend->codegen_expr = llvm_codegen_expr;
    backend->codegen_stmt = llvm_codegen_stmt;
    backend->codegen_decl = llvm_codegen_decl;
    
    /* Optimization */
    backend->optimize = llvm_optimize;
    
    /* Output */
    backend->emit_object = llvm_emit_object;
    backend->emit_assembly = llvm_emit_assembly;
    backend->emit_llvm_ir = llvm_emit_llvm_ir;
    backend->emit_bitcode = llvm_emit_bitcode;
    
    /* Linking */
    backend->link = llvm_link;
    
    /* Error handling */
    backend->get_last_error = llvm_get_last_error;
    
    return backend;
}
