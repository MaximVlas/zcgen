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
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Error.h>
#include <stdarg.h>
#include <stdlib.h>

/* LLVM backend context */
typedef struct LLVMBackendContext {
    LLVMContextRef llvm_context;
    LLVMModuleRef llvm_module;
    LLVMBuilderRef llvm_builder;
    LLVMTargetMachineRef target_machine;
    
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
    
    /* Setup target machine */
    char *error = NULL;
    LLVMTargetRef target = NULL;
    char *allocated_triple = NULL;
    const char *actual_triple = target_triple;
    
    /* If no target triple provided, use native target */
    if (!actual_triple) {
        allocated_triple = LLVMGetDefaultTargetTriple();
        actual_triple = allocated_triple;
    }
    
    if (actual_triple && LLVMGetTargetFromTriple(actual_triple, &target, &error)) {
        fprintf(stderr, "Error getting target for '%s': %s\n", actual_triple, error);
        LLVMDisposeMessage(error);
        target = NULL;
        
        /* Try to get native target as fallback if we weren't already using it */
        if (!allocated_triple) {
            if (allocated_triple) {
                LLVMDisposeMessage(allocated_triple);
            }
            allocated_triple = LLVMGetDefaultTargetTriple();
            if (allocated_triple && LLVMGetTargetFromTriple(allocated_triple, &target, &error)) {
                fprintf(stderr, "Error getting native target: %s\n", error);
                LLVMDisposeMessage(error);
                target = NULL;
            }
        }
    }
    
    if (target) {
        const char *target_cpu = cpu ? cpu : "generic";
        const char *target_features = "";
        
        ctx->target_machine = LLVMCreateTargetMachine(
            target, actual_triple, target_cpu, target_features,
            LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault
        );
        
        if (!ctx->target_machine) {
            fprintf(stderr, "Failed to create target machine for '%s'\n", actual_triple);
        }
    }
    
    /* Clean up allocated triple */
    if (allocated_triple) {
        LLVMDisposeMessage(allocated_triple);
    }
    
    return (BackendContext *)ctx;
}

void llvm_backend_destroy(BackendContext *ctx_opaque) {
    if (!ctx_opaque) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
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
            
        case AST_IDENTIFIER: {
            /* Look up identifier - for now, check function parameters and functions */
            const char *name = expr->data.identifier.name;
            if (!name) return NULL;
            
            /* Check if it's a function parameter */
            if (ctx->current_function) {
                unsigned param_count = LLVMCountParams(ctx->current_function);
                for (unsigned i = 0; i < param_count; i++) {
                    LLVMValueRef param = LLVMGetParam(ctx->current_function, i);
                    size_t name_len;
                    const char *param_name = LLVMGetValueName2(param, &name_len);
                    if (param_name && strcmp(param_name, name) == 0) {
                        return param;
                    }
                }
            }
            
            /* Check if it's a function in the module */
            LLVMValueRef func = LLVMGetNamedFunction(ctx->llvm_module, name);
            if (func) {
                return func;
            }
            
            set_error(ctx, "Undefined identifier: %s", name);
            return NULL;
        }
        
        case AST_CALL_EXPR: {
            /* Function call: callee(args...) */
            ASTNode *callee = expr->data.call_expr.callee;
            if (!callee) return NULL;
            
            /* Get the function */
            LLVMValueRef func = llvm_codegen_expr(ctx_opaque, callee);
            if (!func) return NULL;
            
            /* Get function type */
            LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
            
            /* Generate arguments */
            size_t arg_count = expr->data.call_expr.arg_count;
            LLVMValueRef *args = NULL;
            
            if (arg_count > 0) {
                args = xmalloc(sizeof(LLVMValueRef) * arg_count);
                for (size_t i = 0; i < arg_count; i++) {
                    args[i] = llvm_codegen_expr(ctx_opaque, expr->data.call_expr.args[i]);
                    if (!args[i]) {
                        xfree(args);
                        return NULL;
                    }
                }
            }
            
            /* Build call */
            LLVMValueRef call = LLVMBuildCall2(
                ctx->llvm_builder,
                func_type,
                func,
                args,
                arg_count,
                "calltmp"
            );
            
            if (args) xfree(args);
            return call;
        }
            
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
            
        case AST_IF_STMT: {
            /* if (condition) then_branch [else else_branch] */
            ASTNode *condition = stmt->data.if_stmt.condition;
            ASTNode *then_branch = stmt->data.if_stmt.then_branch;
            ASTNode *else_branch = stmt->data.if_stmt.else_branch;
            
            if (!condition || !then_branch) {
                set_error(ctx, "Invalid if statement");
                break;
            }
            
            /* Generate condition */
            LLVMValueRef cond_val = llvm_codegen_expr(ctx_opaque, condition);
            if (!cond_val) break;
            
            /* Convert condition to i1 (boolean) */
            LLVMTypeRef cond_type = LLVMTypeOf(cond_val);
            if (LLVMGetTypeKind(cond_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(cond_type) != 1) {
                /* Compare with zero to get boolean */
                LLVMValueRef zero = LLVMConstInt(cond_type, 0, 0);
                cond_val = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond_val, zero, "ifcond");
            }
            
            /* Create basic blocks */
            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "then");
            LLVMBasicBlockRef else_bb = else_branch ? LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "else") : NULL;
            LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "ifcont");
            
            /* Branch based on condition */
            if (else_bb) {
                LLVMBuildCondBr(ctx->llvm_builder, cond_val, then_bb, else_bb);
            } else {
                LLVMBuildCondBr(ctx->llvm_builder, cond_val, then_bb, merge_bb);
            }
            
            /* Generate then branch */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, then_bb);
            llvm_codegen_stmt(ctx_opaque, then_branch);
            
            /* Add branch to merge if then block doesn't already have terminator */
            if (!LLVMGetBasicBlockTerminator(then_bb)) {
                LLVMBuildBr(ctx->llvm_builder, merge_bb);
            }
            
            /* Generate else branch if present */
            if (else_branch && else_bb) {
                LLVMPositionBuilderAtEnd(ctx->llvm_builder, else_bb);
                llvm_codegen_stmt(ctx_opaque, else_branch);
                
                /* Add branch to merge if else block doesn't already have terminator */
                if (!LLVMGetBasicBlockTerminator(else_bb)) {
                    LLVMBuildBr(ctx->llvm_builder, merge_bb);
                }
            }
            
            /* Continue in merge block */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_bb);
            break;
        }
            
        default:
            set_error(ctx, "Unsupported statement type: %d", stmt->type);
            break;
    }
}

/* Helper: Get LLVM type from AST type node */
static LLVMTypeRef get_llvm_type_from_ast(LLVMBackendContext *ctx, ASTNode *type_node) {
    if (!type_node) {
        /* Default to i32 if no type specified */
        return LLVMInt32TypeInContext(ctx->llvm_context);
    }
    
    /* For now, simple type mapping based on type name */
    if (type_node->type == AST_TYPE && type_node->data.type.name) {
        const char *name = type_node->data.type.name;
        
        if (strcmp(name, "void") == 0) {
            return LLVMVoidTypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "int") == 0) {
            return LLVMInt32TypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "char") == 0) {
            return LLVMInt8TypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "short") == 0) {
            return LLVMInt16TypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "long") == 0) {
            return LLVMInt64TypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "float") == 0) {
            return LLVMFloatTypeInContext(ctx->llvm_context);
        } else if (strcmp(name, "double") == 0) {
            return LLVMDoubleTypeInContext(ctx->llvm_context);
        }
    } else if (type_node->type == AST_POINTER_TYPE) {
        /* Pointer type */
        LLVMTypeRef pointee = LLVMInt8TypeInContext(ctx->llvm_context); /* default to void* */
        if (type_node->child_count > 0 && type_node->children && type_node->children[0]) {
            pointee = get_llvm_type_from_ast(ctx, type_node->children[0]);
        }
        return LLVMPointerType(pointee, 0);
    }
    
    /* Default to i32 */
    return LLVMInt32TypeInContext(ctx->llvm_context);
}

/* Helper: Generate function declaration */
static void codegen_function_decl(LLVMBackendContext *ctx, ASTNode *func_decl) {
    if (!ctx || !func_decl || func_decl->type != AST_FUNCTION_DECL) return;
    
    /* Ensure we have a module to work with */
    if (!ctx->llvm_module) {
        set_error(ctx, "No module available for function generation");
        return;
    }
    
    /* Debug output */
    fprintf(stderr, "CODEGEN: Processing function declaration with %zu children\n", func_decl->child_count);
    
    /* 
     * Parser AST structure for functions:
     * FunctionDecl
     *   ├─ Type (return type)
     *   ├─ CompoundStmt (body)
     *   └─ Identifier (function name)
     *       └─ Unknown (parameter list)
     *           ├─ ParamDecl
     *           │   ├─ Type
     *           │   └─ Identifier (parameter name)
     *           └─ ...
     */
    
    /* Find function name */
    const char *func_name = "function";  /* default */
    ASTNode *param_list = NULL;
    
    for (size_t i = 0; i < func_decl->child_count; i++) {
        if (func_decl->children[i] && func_decl->children[i]->type == AST_IDENTIFIER) {
            if (func_decl->children[i]->data.identifier.name) {
                func_name = func_decl->children[i]->data.identifier.name;
            }
            /* Parameter list is a child of the identifier node */
            if (func_decl->children[i]->child_count > 0 && func_decl->children[i]->children[0]) {
                param_list = func_decl->children[i]->children[0];
            }
            break;
        }
    }
    
    /* Find return type */
    LLVMTypeRef return_type = LLVMInt32TypeInContext(ctx->llvm_context);
    for (size_t i = 0; i < func_decl->child_count; i++) {
        if (func_decl->children[i] && func_decl->children[i]->type == AST_TYPE) {
            return_type = get_llvm_type_from_ast(ctx, func_decl->children[i]);
            break;
        }
    }
    
    /* Collect parameters */
    LLVMTypeRef *param_types = NULL;
    const char **param_names = NULL;
    size_t param_count = 0;
    
    if (param_list && param_list->child_count > 0) {
        param_count = param_list->child_count;
        param_types = xmalloc(sizeof(LLVMTypeRef) * param_count);
        param_names = xmalloc(sizeof(char *) * param_count);
        
        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = param_list->children[i];
            param_names[i] = NULL;
            param_types[i] = LLVMInt32TypeInContext(ctx->llvm_context);  /* default */
            
            if (param && param->type == AST_PARAM_DECL) {
                /* Find type and name in param children */
                for (size_t j = 0; j < param->child_count; j++) {
                    if (param->children[j] && param->children[j]->type == AST_TYPE) {
                        param_types[i] = get_llvm_type_from_ast(ctx, param->children[j]);
                    } else if (param->children[j] && param->children[j]->type == AST_IDENTIFIER) {
                        if (param->children[j]->data.identifier.name) {
                            param_names[i] = param->children[j]->data.identifier.name;
                        }
                    }
                }
            }
        }
    }
    
    /* Create function type */
    LLVMTypeRef func_type = LLVMFunctionType(return_type, param_types, param_count, 0);
    
    /* Add function to module */
    LLVMValueRef function = LLVMAddFunction(ctx->llvm_module, func_name, func_type);
    
    /* Set parameter names and create allocas for them */
    for (size_t i = 0; i < param_count; i++) {
        LLVMValueRef llvm_param = LLVMGetParam(function, i);
        if (param_names && param_names[i]) {
            LLVMSetValueName2(llvm_param, param_names[i], strlen(param_names[i]));
        }
    }
    
    /* Create entry basic block */
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->llvm_context, function, "entry");
    LLVMPositionBuilderAtEnd(ctx->llvm_builder, entry);
    
    ctx->current_function = function;
    ctx->current_block = entry;
    
    /* Find and generate function body */
    ASTNode *body = NULL;
    for (size_t i = 0; i < func_decl->child_count; i++) {
        if (func_decl->children[i] && func_decl->children[i]->type == AST_COMPOUND_STMT) {
            body = func_decl->children[i];
            break;
        }
    }
    
    if (body) {
        llvm_codegen_stmt((BackendContext *)ctx, body);
        
        /* Add return void if function doesn't return a value and last instruction isn't a return */
        if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            if (current_bb) {
                LLVMValueRef last_inst = LLVMGetLastInstruction(current_bb);
                if (!last_inst || LLVMGetInstructionOpcode(last_inst) != LLVMRet) {
                    LLVMBuildRetVoid(ctx->llvm_builder);
                }
            }
        }
    }
    
    /* Cleanup */
    if (param_types) xfree(param_types);
    if (param_names) xfree(param_names);
}

void llvm_codegen_decl(BackendContext *ctx_opaque, ASTNode *decl) {
    if (!ctx_opaque || !decl) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    /* Ensure we have a module for code generation */
    if (!ctx->llvm_module) {
        set_error(ctx, "No module available for code generation");
        return;
    }
    
    /* Debug output */
    fprintf(stderr, "CODEGEN: Processing declaration type %d\n", decl->type);
    
    switch (decl->type) {
        case AST_TRANSLATION_UNIT:
            /* Generate code for each declaration */
            for (size_t i = 0; i < decl->child_count; i++) {
                if (decl->children[i]) {
                    llvm_codegen_decl(ctx_opaque, decl->children[i]);
                }
            }
            break;
            
        case AST_FUNCTION_DECL:
            codegen_function_decl(ctx, decl);
            break;
            
        case AST_VAR_DECL:
            /* TODO: Handle variable declarations */
            break;
            
        case AST_DECL_STMT:
            /* Declaration statement - process children */
            for (size_t i = 0; i < decl->child_count; i++) {
                if (decl->children[i]) {
                    llvm_codegen_decl(ctx_opaque, decl->children[i]);
                }
            }
            break;
            
        case AST_NULL_STMT:
            /* Empty statement - do nothing */
            break;
            
        case AST_TYPE:
        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
        case AST_ENUM_DECL:
            /* Type declarations - TODO: handle properly */
            break;
            
        default:
            /* Don't error on unknown types, just skip them for now */
            /* This prevents crashes on complex AST structures */
            break;
    }
}

/* ===== OPTIMIZATION ===== */

void llvm_optimize(BackendContext *ctx_opaque, void *module, int opt_level) {
    if (!ctx_opaque || !module) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    LLVMModuleRef mod = (LLVMModuleRef)module;
    
    if (opt_level == 0) return;  /* No optimization */
    
    /* Use new PassBuilder API (LLVM 14+) */
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    
    /* Configure optimization options */
    LLVMPassBuilderOptionsSetVerifyEach(options, 1);
    
    if (opt_level >= 2) {
        LLVMPassBuilderOptionsSetLoopInterleaving(options, 1);
        LLVMPassBuilderOptionsSetLoopVectorization(options, 1);
        LLVMPassBuilderOptionsSetSLPVectorization(options, 1);
    }
    
    if (opt_level >= 3) {
        LLVMPassBuilderOptionsSetLoopUnrolling(options, 1);
    }
    
    /* Build pass pipeline string based on optimization level */
    const char *passes = NULL;
    switch (opt_level) {
        case 1:
            passes = "default<O1>";
            break;
        case 2:
            passes = "default<O2>";
            break;
        case 3:
            passes = "default<O3>";
            break;
        default:
            passes = "default<O0>";
            break;
    }
    
    /* Run the optimization passes */
    LLVMErrorRef error = LLVMRunPasses(mod, passes, ctx->target_machine, options);
    
    if (error) {
        char *error_msg = LLVMGetErrorMessage(error);
        set_error(ctx, "Optimization failed: %s", error_msg);
        LLVMDisposeErrorMessage(error_msg);
    }
    
    LLVMDisposePassBuilderOptions(options);
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
