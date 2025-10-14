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

/* Symbol table entry for variables */
typedef struct SymbolEntry {
    char *name;
    LLVMValueRef value;  /* alloca for locals, global for globals */
    LLVMTypeRef type;
    bool is_global;
    struct SymbolEntry *next;
} SymbolEntry;

#define SYMBOL_TABLE_SIZE 256

/* LLVM backend context */
typedef struct LLVMBackendContext {
    LLVMContextRef llvm_context;
    LLVMModuleRef llvm_module;
    LLVMBuilderRef llvm_builder;
    LLVMTargetMachineRef target_machine;
    
    /* Symbol table for variables */
    SymbolEntry *symbol_table[SYMBOL_TABLE_SIZE];
    
    /* Current function being generated */
    LLVMValueRef current_function;
    LLVMBasicBlockRef current_block;
    
    /* Loop context for break/continue */
    LLVMBasicBlockRef loop_continue_block;
    LLVMBasicBlockRef loop_break_block;
    
    /* Recursion depth tracking */
    int recursion_depth;
    
    /* Error handling */
    char *last_error;
} LLVMBackendContext;

/* Helper: Hash function for symbol table */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SYMBOL_TABLE_SIZE;
}

/* Helper: Add symbol to table */
static void symbol_table_add(LLVMBackendContext *ctx, const char *name, 
                             LLVMValueRef value, LLVMTypeRef type, bool is_global) {
    unsigned int index = hash_string(name);
    
    SymbolEntry *entry = xcalloc(1, sizeof(SymbolEntry));
    entry->name = xstrdup(name);
    entry->value = value;
    entry->type = type;
    entry->is_global = is_global;
    entry->next = ctx->symbol_table[index];
    ctx->symbol_table[index] = entry;
}

/* Helper: Lookup symbol in table */
static SymbolEntry *symbol_table_lookup(LLVMBackendContext *ctx, const char *name) {
    unsigned int index = hash_string(name);
    
    SymbolEntry *entry = ctx->symbol_table[index];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Helper: Clear symbol table */
static void symbol_table_clear(LLVMBackendContext *ctx) {
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        SymbolEntry *entry = ctx->symbol_table[i];
        while (entry) {
            SymbolEntry *next = entry->next;
            xfree(entry->name);
            xfree(entry);
            entry = next;
        }
        ctx->symbol_table[i] = NULL;
    }
}

/* Forward declarations */
static LLVMTypeRef get_llvm_type_from_ast(LLVMBackendContext *ctx, ASTNode *type_node);

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
    
    /* Initialize symbol table */
    memset(ctx->symbol_table, 0, sizeof(ctx->symbol_table));
    
    /* Initialize recursion depth */
    ctx->recursion_depth = 0;
    
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
    
    /* Clean up symbol table */
    symbol_table_clear(ctx);
    
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

static LLVMValueRef codegen_string_literal(LLVMBackendContext *ctx, ASTNode *node) {
    if (!node->data.string_literal.value) return NULL;
    
    /* Create a global string constant */
    LLVMValueRef str = LLVMBuildGlobalStringPtr(ctx->llvm_builder, 
                                                  node->data.string_literal.value, 
                                                  ".str");
    return str;
}

/* Helper: Coerce two values to the same type for binary operations */
static void coerce_binary_operands(LLVMBackendContext *ctx, LLVMValueRef *left, LLVMValueRef *right) {
    if (!left || !right || !*left || !*right) return;
    
    LLVMTypeRef left_type = LLVMTypeOf(*left);
    LLVMTypeRef right_type = LLVMTypeOf(*right);
    
    LLVMTypeKind left_kind = LLVMGetTypeKind(left_type);
    LLVMTypeKind right_kind = LLVMGetTypeKind(right_type);
    
    /* If types are already the same, nothing to do */
    if (left_type == right_type) return;
    
    /* Handle integer type mismatches */
    if (left_kind == LLVMIntegerTypeKind && right_kind == LLVMIntegerTypeKind) {
        unsigned left_width = LLVMGetIntTypeWidth(left_type);
        unsigned right_width = LLVMGetIntTypeWidth(right_type);
        
        /* Extend the smaller one to match the larger */
        if (left_width < right_width) {
            *left = LLVMBuildZExt(ctx->llvm_builder, *left, right_type, "zext");
        } else if (right_width < left_width) {
            *right = LLVMBuildZExt(ctx->llvm_builder, *right, left_type, "zext");
        }
    }
    /* Handle pointer vs integer comparisons - cast int to pointer */
    else if (left_kind == LLVMPointerTypeKind && right_kind == LLVMIntegerTypeKind) {
        *right = LLVMBuildIntToPtr(ctx->llvm_builder, *right, left_type, "inttoptr");
    }
    else if (left_kind == LLVMIntegerTypeKind && right_kind == LLVMPointerTypeKind) {
        *left = LLVMBuildIntToPtr(ctx->llvm_builder, *left, right_type, "inttoptr");
    }
}

static LLVMValueRef codegen_binary_expr(LLVMBackendContext *ctx, ASTNode *node) {
    if (node->child_count < 2) return NULL;
    
    LLVMValueRef left = llvm_codegen_expr((BackendContext *)ctx, node->children[0]);
    LLVMValueRef right = llvm_codegen_expr((BackendContext *)ctx, node->children[1]);
    
    if (!left || !right) return NULL;
    
    /* Coerce operands to the same type */
    coerce_binary_operands(ctx, &left, &right);
    
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
    
    /* Validate node is not destroyed */
    if (expr->destroyed) {
        return NULL;
    }
    
    switch (expr->type) {
        case AST_INTEGER_LITERAL:
            return codegen_integer_literal(ctx, expr);
            
        case AST_FLOAT_LITERAL:
            return codegen_float_literal(ctx, expr);
            
        case AST_STRING_LITERAL:
            return codegen_string_literal(ctx, expr);
            
        case AST_IDENTIFIER: {
            /* Look up identifier in symbol table */
            const char *name = expr->data.identifier.name;
            if (!name) return NULL;
            
            /* Check symbol table for variables */
            SymbolEntry *entry = symbol_table_lookup(ctx, name);
            if (entry) {
                /* Validate that local variables belong to the current function */
                if (!entry->is_global && entry->value) {
                    /* Check if this is an instruction (alloca) */
                    if (LLVMIsAInstruction(entry->value)) {
                        /* Get the basic block that owns this instruction */
                        LLVMBasicBlockRef owner_bb = LLVMGetInstructionParent(entry->value);
                        if (owner_bb) {
                            LLVMValueRef owner_func = LLVMGetBasicBlockParent(owner_bb);
                            /* Only load if it belongs to current function or is global */
                            if (owner_func != ctx->current_function) {
                                /* This variable is from another function - skip it */
                                set_error(ctx, "Variable '%s' from another function scope", name);
                                return NULL;
                            }
                        }
                    }
                }
                
                /* Load the value from the alloca/global */
                return LLVMBuildLoad2(ctx->llvm_builder, entry->type, entry->value, name);
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
            
            /* Validate callee node */
            if (callee->destroyed) {
                return NULL;
            }
            
            /* Get the function */
            LLVMValueRef func = llvm_codegen_expr(ctx_opaque, callee);
            if (!func) return NULL;
            
            /* Get function type - validate it's actually a function */
            LLVMTypeRef func_type = LLVMGlobalGetValueType(func);
            if (!func_type) {
                set_error(ctx, "Invalid function type");
                return NULL;
            }
            
            /* Verify it's a function type */
            if (LLVMGetTypeKind(func_type) != LLVMFunctionTypeKind) {
                set_error(ctx, "Called value is not a function");
                return NULL;
            }
            
            /* Generate arguments */
            size_t arg_count = expr->data.call_expr.arg_count;
            LLVMValueRef *args = NULL;
            
            /* Get parameter types from function type */
            unsigned param_count = LLVMCountParamTypes(func_type);
            LLVMTypeRef *param_types = NULL;
            if (param_count > 0) {
                param_types = xmalloc(sizeof(LLVMTypeRef) * param_count);
                LLVMGetParamTypes(func_type, param_types);
            }
            
            if (arg_count > 0) {
                args = xmalloc(sizeof(LLVMValueRef) * arg_count);
                for (size_t i = 0; i < arg_count; i++) {
                    /* Validate argument node */
                    if (!expr->data.call_expr.args[i] || expr->data.call_expr.args[i]->destroyed) {
                        xfree(args);
                        if (param_types) xfree(param_types);
                        return NULL;
                    }
                    
                    args[i] = llvm_codegen_expr(ctx_opaque, expr->data.call_expr.args[i]);
                    if (!args[i]) {
                        xfree(args);
                        if (param_types) xfree(param_types);
                        return NULL;
                    }
                    
                    /* Coerce argument to match parameter type if available */
                    if (param_types && i < param_count) {
                        LLVMTypeRef expected_type = param_types[i];
                        LLVMTypeRef actual_type = LLVMTypeOf(args[i]);
                        
                        if (expected_type != actual_type) {
                            LLVMTypeKind expected_kind = LLVMGetTypeKind(expected_type);
                            LLVMTypeKind actual_kind = LLVMGetTypeKind(actual_type);
                            
                            /* Pointer to integer */
                            if (expected_kind == LLVMIntegerTypeKind && actual_kind == LLVMPointerTypeKind) {
                                args[i] = LLVMBuildPtrToInt(ctx->llvm_builder, args[i], expected_type, "ptrtoint");
                            }
                            /* Integer to pointer */
                            else if (expected_kind == LLVMPointerTypeKind && actual_kind == LLVMIntegerTypeKind) {
                                args[i] = LLVMBuildIntToPtr(ctx->llvm_builder, args[i], expected_type, "inttoptr");
                            }
                            /* Integer width mismatch */
                            else if (expected_kind == LLVMIntegerTypeKind && actual_kind == LLVMIntegerTypeKind) {
                                unsigned expected_width = LLVMGetIntTypeWidth(expected_type);
                                unsigned actual_width = LLVMGetIntTypeWidth(actual_type);
                                if (expected_width > actual_width) {
                                    args[i] = LLVMBuildZExt(ctx->llvm_builder, args[i], expected_type, "zext");
                                } else if (expected_width < actual_width) {
                                    args[i] = LLVMBuildTrunc(ctx->llvm_builder, args[i], expected_type, "trunc");
                                }
                            }
                        }
                    }
                }
            }
            
            if (param_types) xfree(param_types);
            
            /* Build call - use empty string instead of "calltmp" to avoid name corruption */
            LLVMValueRef call = LLVMBuildCall2(
                ctx->llvm_builder,
                func_type,
                func,
                args,
                arg_count,
                ""
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
            
        case AST_ASSIGN_EXPR: {
            /* Assignment: lhs = rhs */
            if (expr->child_count < 2) return NULL;
            
            ASTNode *lhs = expr->children[0];
            ASTNode *rhs = expr->children[1];
            
            /* Get the rvalue */
            LLVMValueRef rvalue = llvm_codegen_expr(ctx_opaque, rhs);
            if (!rvalue) return NULL;
            
            /* Get the lvalue (address) */
            if (lhs->type == AST_IDENTIFIER) {
                const char *name = lhs->data.identifier.name;
                if (!name) return NULL;
                
                SymbolEntry *entry = symbol_table_lookup(ctx, name);
                if (!entry) {
                    set_error(ctx, "Undefined variable: %s", name);
                    return NULL;
                }
                
                /* Store the value */
                LLVMBuildStore(ctx->llvm_builder, rvalue, entry->value);
                return rvalue;  /* Assignment returns the assigned value */
            }
            
            set_error(ctx, "Invalid lvalue in assignment");
            return NULL;
        }
        
        /* Compound assignments */
        case AST_ADD_ASSIGN_EXPR:
        case AST_SUB_ASSIGN_EXPR:
        case AST_MUL_ASSIGN_EXPR:
        case AST_DIV_ASSIGN_EXPR:
        case AST_MOD_ASSIGN_EXPR:
        case AST_AND_ASSIGN_EXPR:
        case AST_OR_ASSIGN_EXPR:
        case AST_XOR_ASSIGN_EXPR:
        case AST_SHL_ASSIGN_EXPR:
        case AST_SHR_ASSIGN_EXPR: {
            /* Compound assignment: lhs op= rhs */
            if (expr->child_count < 2) return NULL;
            
            ASTNode *lhs = expr->children[0];
            ASTNode *rhs = expr->children[1];
            
            if (lhs->type != AST_IDENTIFIER) {
                set_error(ctx, "Invalid lvalue in compound assignment");
                return NULL;
            }
            
            const char *name = lhs->data.identifier.name;
            if (!name) return NULL;
            
            SymbolEntry *entry = symbol_table_lookup(ctx, name);
            if (!entry) {
                set_error(ctx, "Undefined variable: %s", name);
                return NULL;
            }
            
            /* Load current value */
            LLVMValueRef current = LLVMBuildLoad2(ctx->llvm_builder, entry->type, entry->value, name);
            LLVMValueRef rvalue = llvm_codegen_expr(ctx_opaque, rhs);
            if (!rvalue) return NULL;
            
            /* Coerce operands to the same type */
            coerce_binary_operands(ctx, &current, &rvalue);
            
            /* Perform operation */
            LLVMValueRef result = NULL;
            switch (expr->type) {
                case AST_ADD_ASSIGN_EXPR: result = LLVMBuildAdd(ctx->llvm_builder, current, rvalue, "addassign"); break;
                case AST_SUB_ASSIGN_EXPR: result = LLVMBuildSub(ctx->llvm_builder, current, rvalue, "subassign"); break;
                case AST_MUL_ASSIGN_EXPR: result = LLVMBuildMul(ctx->llvm_builder, current, rvalue, "mulassign"); break;
                case AST_DIV_ASSIGN_EXPR: result = LLVMBuildSDiv(ctx->llvm_builder, current, rvalue, "divassign"); break;
                case AST_MOD_ASSIGN_EXPR: result = LLVMBuildSRem(ctx->llvm_builder, current, rvalue, "modassign"); break;
                case AST_AND_ASSIGN_EXPR: result = LLVMBuildAnd(ctx->llvm_builder, current, rvalue, "andassign"); break;
                case AST_OR_ASSIGN_EXPR: result = LLVMBuildOr(ctx->llvm_builder, current, rvalue, "orassign"); break;
                case AST_XOR_ASSIGN_EXPR: result = LLVMBuildXor(ctx->llvm_builder, current, rvalue, "xorassign"); break;
                case AST_SHL_ASSIGN_EXPR: result = LLVMBuildShl(ctx->llvm_builder, current, rvalue, "shlassign"); break;
                case AST_SHR_ASSIGN_EXPR: result = LLVMBuildAShr(ctx->llvm_builder, current, rvalue, "shrassign"); break;
                default: return NULL;
            }
            
            if (result) {
                LLVMBuildStore(ctx->llvm_builder, result, entry->value);
            }
            return result;
        }
        
        /* Unary operators */
        case AST_UNARY_MINUS_EXPR:
        case AST_UNARY_PLUS_EXPR:
        case AST_NOT_EXPR:
        case AST_BIT_NOT_EXPR: {
            if (expr->child_count < 1) return NULL;
            LLVMValueRef operand = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            if (!operand) return NULL;
            
            switch (expr->type) {
                case AST_UNARY_MINUS_EXPR:
                    return LLVMBuildNeg(ctx->llvm_builder, operand, "negtmp");
                case AST_UNARY_PLUS_EXPR:
                    return operand;  /* Unary + is a no-op */
                case AST_NOT_EXPR: {
                    LLVMValueRef zero = LLVMConstInt(LLVMTypeOf(operand), 0, 0);
                    return LLVMBuildICmp(ctx->llvm_builder, LLVMIntEQ, operand, zero, "nottmp");
                }
                case AST_BIT_NOT_EXPR:
                    return LLVMBuildNot(ctx->llvm_builder, operand, "bitnottmp");
                default:
                    return NULL;
            }
        }
        
        /* Increment/Decrement */
        case AST_PRE_INC_EXPR:
        case AST_PRE_DEC_EXPR:
        case AST_POST_INC_EXPR:
        case AST_POST_DEC_EXPR: {
            if (expr->child_count < 1) return NULL;
            ASTNode *operand_node = expr->children[0];
            
            if (operand_node->type != AST_IDENTIFIER) {
                set_error(ctx, "Invalid operand for increment/decrement");
                return NULL;
            }
            
            const char *name = operand_node->data.identifier.name;
            if (!name) return NULL;
            
            SymbolEntry *entry = symbol_table_lookup(ctx, name);
            if (!entry) {
                set_error(ctx, "Undefined variable: %s", name);
                return NULL;
            }
            
            /* Load current value */
            LLVMValueRef current = LLVMBuildLoad2(ctx->llvm_builder, entry->type, entry->value, name);
            LLVMValueRef one = LLVMConstInt(entry->type, 1, 0);
            
            /* Perform operation */
            LLVMValueRef new_val = (expr->type == AST_PRE_INC_EXPR || expr->type == AST_POST_INC_EXPR)
                ? LLVMBuildAdd(ctx->llvm_builder, current, one, "inctmp")
                : LLVMBuildSub(ctx->llvm_builder, current, one, "dectmp");
            
            /* Store new value */
            LLVMBuildStore(ctx->llvm_builder, new_val, entry->value);
            
            /* Return appropriate value */
            return (expr->type == AST_PRE_INC_EXPR || expr->type == AST_PRE_DEC_EXPR)
                ? new_val : current;
        }
        
        /* Address-of and dereference */
        case AST_ADDR_OF_EXPR: {
            if (expr->child_count < 1) return NULL;
            ASTNode *operand = expr->children[0];
            
            if (operand->type == AST_IDENTIFIER) {
                const char *name = operand->data.identifier.name;
                if (!name) return NULL;
                
                SymbolEntry *entry = symbol_table_lookup(ctx, name);
                if (!entry) {
                    set_error(ctx, "Undefined variable: %s", name);
                    return NULL;
                }
                
                /* Return the address (the alloca/global itself) */
                return entry->value;
            }
            
            set_error(ctx, "Invalid operand for address-of");
            return NULL;
        }
        
        case AST_DEREF_EXPR: {
            if (expr->child_count < 1) return NULL;
            LLVMValueRef ptr = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            if (!ptr) return NULL;
            
            /* Load from pointer */
            LLVMTypeRef ptr_type = LLVMTypeOf(ptr);
            if (LLVMGetTypeKind(ptr_type) == LLVMPointerTypeKind) {
                LLVMTypeRef elem_type = LLVMInt32TypeInContext(ctx->llvm_context);  /* TODO: proper type */
                return LLVMBuildLoad2(ctx->llvm_builder, elem_type, ptr, "dereftmp");
            }
            
            set_error(ctx, "Dereference of non-pointer");
            return NULL;
        }
        
        /* Ternary operator */
        case AST_CONDITIONAL_EXPR: {
            if (expr->child_count < 3) return NULL;
            
            LLVMValueRef cond = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            if (!cond) return NULL;
            
            /* Convert condition to i1 */
            LLVMTypeRef cond_type = LLVMTypeOf(cond);
            if (LLVMGetTypeKind(cond_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(cond_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(cond_type, 0, 0);
                cond = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond, zero, "terncond");
            }
            
            /* Create blocks */
            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "tern.then");
            LLVMBasicBlockRef else_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "tern.else");
            LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "tern.end");
            
            LLVMBuildCondBr(ctx->llvm_builder, cond, then_bb, else_bb);
            
            /* Then branch */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, then_bb);
            LLVMValueRef then_val = llvm_codegen_expr(ctx_opaque, expr->children[1]);
            LLVMBasicBlockRef then_end_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            LLVMBuildBr(ctx->llvm_builder, merge_bb);
            
            /* Else branch */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, else_bb);
            LLVMValueRef else_val = llvm_codegen_expr(ctx_opaque, expr->children[2]);
            LLVMBasicBlockRef else_end_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            LLVMBuildBr(ctx->llvm_builder, merge_bb);
            
            /* Merge with phi */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, merge_bb);
            if (then_val && else_val) {
                LLVMValueRef phi = LLVMBuildPhi(ctx->llvm_builder, LLVMTypeOf(then_val), "ternphi");
                LLVMValueRef values[] = {then_val, else_val};
                LLVMBasicBlockRef blocks[] = {then_end_bb, else_end_bb};
                LLVMAddIncoming(phi, values, blocks, 2);
                return phi;
            }
            
            /* If one branch failed, return a default value to avoid leaving block without terminator */
            if (then_val) return then_val;
            if (else_val) return else_val;
            return LLVMConstInt(LLVMInt32TypeInContext(ctx->llvm_context), 0, 0);
        }
        
        /* Comma operator */
        case AST_COMMA_EXPR: {
            LLVMValueRef result = NULL;
            for (size_t i = 0; i < expr->child_count; i++) {
                result = llvm_codegen_expr(ctx_opaque, expr->children[i]);
            }
            return result;  /* Return last value */
        }
        
        /* Sizeof */
        case AST_SIZEOF_EXPR: {
            /* For now, return a constant based on type */
            /* TODO: Proper sizeof implementation */
            return LLVMConstInt(LLVMInt64TypeInContext(ctx->llvm_context), 4, 0);
        }
        
        /* Logical operators with short-circuit evaluation */
        case AST_LOGICAL_AND_EXPR: {
            if (expr->child_count < 2) return NULL;
            
            /* Create blocks */
            LLVMBasicBlockRef rhs_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "land.rhs");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "land.end");
            
            /* Evaluate LHS */
            LLVMValueRef lhs = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            if (!lhs) return NULL;
            
            LLVMBasicBlockRef lhs_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            
            /* Convert to bool */
            LLVMTypeRef lhs_type = LLVMTypeOf(lhs);
            if (LLVMGetTypeKind(lhs_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(lhs_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(lhs_type, 0, 0);
                lhs = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, lhs, zero, "landcond");
            }
            
            /* Short-circuit: if LHS is false, skip RHS */
            LLVMBuildCondBr(ctx->llvm_builder, lhs, rhs_bb, end_bb);
            
            /* Evaluate RHS */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, rhs_bb);
            LLVMValueRef rhs = llvm_codegen_expr(ctx_opaque, expr->children[1]);
            if (!rhs) {
                /* If RHS fails, use false and add terminator */
                rhs = LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_context), 0, 0);
            }
            
            LLVMBasicBlockRef rhs_end_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            
            /* Convert to bool */
            LLVMTypeRef rhs_type = LLVMTypeOf(rhs);
            if (LLVMGetTypeKind(rhs_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(rhs_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(rhs_type, 0, 0);
                rhs = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, rhs, zero, "landval");
            }
            
            LLVMBuildBr(ctx->llvm_builder, end_bb);
            
            /* Merge with phi */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_bb);
            LLVMValueRef phi = LLVMBuildPhi(ctx->llvm_builder, LLVMInt1TypeInContext(ctx->llvm_context), "landphi");
            LLVMValueRef false_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_context), 0, 0);
            LLVMValueRef values[] = {false_val, rhs};
            LLVMBasicBlockRef blocks[] = {lhs_bb, rhs_end_bb};
            LLVMAddIncoming(phi, values, blocks, 2);
            
            return phi;
        }
        
        case AST_LOGICAL_OR_EXPR: {
            if (expr->child_count < 2) return NULL;
            
            /* Create blocks */
            LLVMBasicBlockRef rhs_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "lor.rhs");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "lor.end");
            
            /* Evaluate LHS */
            LLVMValueRef lhs = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            if (!lhs) return NULL;
            
            LLVMBasicBlockRef lhs_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            
            /* Convert to bool */
            LLVMTypeRef lhs_type = LLVMTypeOf(lhs);
            if (LLVMGetTypeKind(lhs_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(lhs_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(lhs_type, 0, 0);
                lhs = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, lhs, zero, "lorcond");
            }
            
            /* Short-circuit: if LHS is true, skip RHS */
            LLVMBuildCondBr(ctx->llvm_builder, lhs, end_bb, rhs_bb);
            
            /* Evaluate RHS */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, rhs_bb);
            LLVMValueRef rhs = llvm_codegen_expr(ctx_opaque, expr->children[1]);
            if (!rhs) {
                /* If RHS fails, use true and add terminator */
                rhs = LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_context), 1, 0);
            }
            
            LLVMBasicBlockRef rhs_end_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            
            /* Convert to bool */
            LLVMTypeRef rhs_type = LLVMTypeOf(rhs);
            if (LLVMGetTypeKind(rhs_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(rhs_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(rhs_type, 0, 0);
                rhs = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, rhs, zero, "lorval");
            }
            
            LLVMBuildBr(ctx->llvm_builder, end_bb);
            
            /* Merge with phi */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_bb);
            LLVMValueRef phi = LLVMBuildPhi(ctx->llvm_builder, LLVMInt1TypeInContext(ctx->llvm_context), "lorphi");
            LLVMValueRef true_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_context), 1, 0);
            LLVMValueRef values[] = {true_val, rhs};
            LLVMBasicBlockRef blocks[] = {lhs_bb, rhs_end_bb};
            LLVMAddIncoming(phi, values, blocks, 2);
            
            return phi;
        }
        
        /* Array subscript */
        case AST_ARRAY_SUBSCRIPT_EXPR: {
            if (expr->child_count < 2) return NULL;
            
            LLVMValueRef array = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            LLVMValueRef index = llvm_codegen_expr(ctx_opaque, expr->children[1]);
            
            if (!array || !index) return NULL;
            
            /* GEP to get element pointer */
            LLVMValueRef indices[] = {index};
            LLVMValueRef ptr = LLVMBuildGEP2(ctx->llvm_builder, 
                                             LLVMInt32TypeInContext(ctx->llvm_context),
                                             array, indices, 1, "arrayidx");
            
            /* Load the value */
            return LLVMBuildLoad2(ctx->llvm_builder, 
                                 LLVMInt32TypeInContext(ctx->llvm_context),
                                 ptr, "arrayval");
        }
        
        /* Cast expressions */
        case AST_CAST_EXPR:
        case AST_IMPLICIT_CAST_EXPR: {
            if (expr->child_count < 1) return NULL;
            
            /* For now, just return the value without casting */
            /* TODO: Proper type casting based on target type */
            return llvm_codegen_expr(ctx_opaque, expr->children[expr->child_count - 1]);
        }
        
        /* Member access */
        case AST_MEMBER_EXPR:
        case AST_ARROW_EXPR: {
            /* Struct/union member access: obj.member or ptr->member */
            if (!expr->data.identifier.name) return NULL;
            
            const char *member_name = expr->data.identifier.name;
            
            /* Get the base object/pointer */
            LLVMValueRef base = NULL;
            if (expr->child_count > 0 && expr->children[0]) {
                base = llvm_codegen_expr(ctx_opaque, expr->children[0]);
            }
            
            if (!base) {
                set_error(ctx, "Invalid member access base");
                return NULL;
            }
            
            /* For now, treat all member accesses as simple pointer offsets */
            /* This is a simplified implementation - proper struct layout needed */
            /* Return the base for now to avoid NULL returns */
            (void)member_name;  /* Suppress unused warning */
            return base;
        }
        
        /* Literals */
        case AST_CHAR_LITERAL:
            /* Char literals use int_literal.value field */
            return LLVMConstInt(LLVMInt8TypeInContext(ctx->llvm_context), 
                               (unsigned char)expr->data.int_literal.value, 0);
            
        case AST_BOOL_LITERAL:
        case AST_NULL_LITERAL:
            return LLVMConstInt(LLVMInt1TypeInContext(ctx->llvm_context), 0, 0);
            
        default:
            set_error(ctx, "Unsupported expression type: %d", expr->type);
            return NULL;
    }
}

void llvm_codegen_stmt(BackendContext *ctx_opaque, ASTNode *stmt) {
    if (!ctx_opaque || !stmt) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    /* Prevent infinite recursion */
    if (ctx->recursion_depth > 500) {
        return;
    }
    ctx->recursion_depth++;
    
    /* Safety check: if we don't have a current function and this is not a global scope statement, skip */
    if (!ctx->current_function && 
        (stmt->type == AST_VAR_DECL || stmt->type == AST_LOCAL_VAR_DECL)) {
        ctx->recursion_depth--;
        return;
    }
    
    /* Skip if current block already has a terminator (avoid "terminator in middle" errors) */
    LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->llvm_builder);
    if (current_bb && LLVMGetBasicBlockTerminator(current_bb)) {
        ctx->recursion_depth--;
        return;
    }
    
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
                if (ret_val && ctx->current_function) {
                    /* Get the function's return type */
                    LLVMTypeRef func_type = LLVMGlobalGetValueType(ctx->current_function);
                    if (func_type && LLVMGetTypeKind(func_type) == LLVMFunctionTypeKind) {
                        LLVMTypeRef expected_ret_type = LLVMGetReturnType(func_type);
                        LLVMTypeRef actual_ret_type = LLVMTypeOf(ret_val);
                        
                        /* Coerce return value to match expected type */
                        if (expected_ret_type != actual_ret_type) {
                            LLVMTypeKind expected_kind = LLVMGetTypeKind(expected_ret_type);
                            LLVMTypeKind actual_kind = LLVMGetTypeKind(actual_ret_type);
                            
                            /* Pointer to integer */
                            if (expected_kind == LLVMIntegerTypeKind && actual_kind == LLVMPointerTypeKind) {
                                ret_val = LLVMBuildPtrToInt(ctx->llvm_builder, ret_val, expected_ret_type, "ptrtoint");
                            }
                            /* Integer to pointer */
                            else if (expected_kind == LLVMPointerTypeKind && actual_kind == LLVMIntegerTypeKind) {
                                ret_val = LLVMBuildIntToPtr(ctx->llvm_builder, ret_val, expected_ret_type, "inttoptr");
                            }
                            /* Integer width mismatch */
                            else if (expected_kind == LLVMIntegerTypeKind && actual_kind == LLVMIntegerTypeKind) {
                                unsigned expected_width = LLVMGetIntTypeWidth(expected_ret_type);
                                unsigned actual_width = LLVMGetIntTypeWidth(actual_ret_type);
                                if (expected_width > actual_width) {
                                    ret_val = LLVMBuildZExt(ctx->llvm_builder, ret_val, expected_ret_type, "zext");
                                } else if (expected_width < actual_width) {
                                    ret_val = LLVMBuildTrunc(ctx->llvm_builder, ret_val, expected_ret_type, "trunc");
                                }
                            }
                        }
                    }
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
            
        case AST_DECL_STMT:
            /* Declaration statement - process as declarations */
            for (size_t i = 0; i < stmt->child_count; i++) {
                if (stmt->children[i]) {
                    llvm_codegen_decl(ctx_opaque, stmt->children[i]);
                }
            }
            break;
            
        case AST_VAR_DECL:
        case AST_LOCAL_VAR_DECL: {
            /* Local variable declaration */
            /* Skip if not in a function context */
            if (!ctx->current_function) {
                break;
            }
            
            /* CRITICAL: Check if node is destroyed before accessing data */
            if (stmt->destroyed) {
                break;
            }
            
            const char *var_name = stmt->data.var_decl.name;
            if (!var_name) break;
            
            ASTNode *var_type = stmt->data.var_decl.type;
            ASTNode *init_expr = stmt->data.var_decl.init;
            
            /* Validate type node if present */
            if (var_type && var_type->destroyed) {
                var_type = NULL;
            }
            
            /* Validate init node if present */
            if (init_expr && init_expr->destroyed) {
                init_expr = NULL;
            }
            
            /* Get LLVM type - check children for declarator info */
            LLVMTypeRef llvm_type = NULL;
            
            /* First try to get type from children (declarator might have pointer/array info) */
            if (stmt->children) {
                for (size_t i = 0; i < stmt->child_count && !llvm_type; i++) {
                    if (stmt->children[i] && !stmt->children[i]->destroyed) {
                        ASTNode *child = stmt->children[i];
                        if (child->type == AST_POINTER_TYPE || child->type == AST_ARRAY_TYPE) {
                            llvm_type = get_llvm_type_from_ast(ctx, child);
                        }
                    }
                }
            }
            
            /* If not found, try the type field */
            if (!llvm_type && var_type) {
                llvm_type = get_llvm_type_from_ast(ctx, var_type);
            }
            
            /* Default to i32 if still no type */
            if (!llvm_type && ctx->llvm_context) {
                llvm_type = LLVMInt32TypeInContext(ctx->llvm_context);
            }
            
            /* CRITICAL: Validate all required components before creating alloca */
            if (!llvm_type || !ctx->llvm_builder || !ctx->current_function || !var_name) {
                break;  /* Skip this variable - missing required components */
            }
            
            /* Get current insertion block */
            LLVMBasicBlockRef current_bb = LLVMGetInsertBlock(ctx->llvm_builder);
            if (!current_bb) {
                break;  /* No valid insertion point */
            }
            
            /* Verify the basic block belongs to our function */
            LLVMValueRef bb_parent = LLVMGetBasicBlockParent(current_bb);
            if (!bb_parent || bb_parent != ctx->current_function) {
                break;  /* Basic block doesn't belong to current function */
            }
            
            /* Create alloca - skip on any error */
            LLVMValueRef alloca = NULL;
            alloca = LLVMBuildAlloca(ctx->llvm_builder, llvm_type, var_name);
            
            if (!alloca) {
                break;
            }
            
            /* Add to symbol table */
            symbol_table_add(ctx, var_name, alloca, llvm_type, false);
            
            /* Handle initializer if present */
            if (init_expr) {
                LLVMValueRef init_val = llvm_codegen_expr(ctx_opaque, init_expr);
                if (init_val) {
                    LLVMBuildStore(ctx->llvm_builder, init_val, alloca);
                }
            }
            break;
        }
            
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
            ctx->current_block = merge_bb;
            break;
        }
            
        case AST_WHILE_STMT: {
            /* while (condition) body */
            ASTNode *condition = stmt->data.while_stmt.condition;
            ASTNode *body = stmt->data.while_stmt.body;
            
            if (!condition || !body) {
                set_error(ctx, "Invalid while statement");
                break;
            }
            
            /* Create basic blocks */
            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "while.cond");
            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "while.body");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "while.end");
            
            /* Branch to condition */
            LLVMBuildBr(ctx->llvm_builder, cond_bb);
            
            /* Generate condition */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, cond_bb);
            LLVMValueRef cond_val = llvm_codegen_expr(ctx_opaque, condition);
            if (!cond_val) break;
            
            /* Convert to i1 if needed */
            LLVMTypeRef cond_type = LLVMTypeOf(cond_val);
            if (LLVMGetTypeKind(cond_type) != LLVMIntegerTypeKind || 
                LLVMGetIntTypeWidth(cond_type) != 1) {
                LLVMValueRef zero = LLVMConstInt(cond_type, 0, 0);
                cond_val = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond_val, zero, "loopcond");
            }
            
            LLVMBuildCondBr(ctx->llvm_builder, cond_val, loop_bb, end_bb);
            
            /* Save old loop context and set new one */
            LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
            LLVMBasicBlockRef old_break = ctx->loop_break_block;
            ctx->loop_continue_block = cond_bb;  /* continue goes to condition */
            ctx->loop_break_block = end_bb;      /* break goes to end */
            
            /* Generate loop body */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_bb);
            llvm_codegen_stmt(ctx_opaque, body);
            
            /* Restore old loop context */
            ctx->loop_continue_block = old_continue;
            ctx->loop_break_block = old_break;
            
            /* Branch back to condition if no terminator */
            if (!LLVMGetBasicBlockTerminator(loop_bb)) {
                LLVMBuildBr(ctx->llvm_builder, cond_bb);
            }
            
            /* Continue after loop */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_bb);
            ctx->current_block = end_bb;
            break;
        }
            
        case AST_FOR_STMT: {
            /* for (init; condition; increment) body */
            ASTNode *init = stmt->data.for_stmt.init;
            ASTNode *condition = stmt->data.for_stmt.condition;
            ASTNode *increment = stmt->data.for_stmt.increment;
            ASTNode *body = stmt->data.for_stmt.body;
            
            /* Generate init */
            if (init) {
                if (init->type == AST_DECL_STMT || init->type == AST_VAR_DECL) {
                    llvm_codegen_stmt(ctx_opaque, init);
                } else {
                    llvm_codegen_expr(ctx_opaque, init);
                }
            }
            
            /* Create basic blocks */
            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "for.cond");
            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "for.body");
            LLVMBasicBlockRef inc_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "for.inc");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "for.end");
            
            /* Branch to condition */
            LLVMBuildBr(ctx->llvm_builder, cond_bb);
            
            /* Generate condition */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, cond_bb);
            if (condition) {
                LLVMValueRef cond_val = llvm_codegen_expr(ctx_opaque, condition);
                if (cond_val) {
                    LLVMTypeRef cond_type = LLVMTypeOf(cond_val);
                    if (LLVMGetTypeKind(cond_type) != LLVMIntegerTypeKind || 
                        LLVMGetIntTypeWidth(cond_type) != 1) {
                        LLVMValueRef zero = LLVMConstInt(cond_type, 0, 0);
                        cond_val = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond_val, zero, "forcond");
                    }
                    LLVMBuildCondBr(ctx->llvm_builder, cond_val, loop_bb, end_bb);
                } else {
                    LLVMBuildBr(ctx->llvm_builder, loop_bb);
                }
            } else {
                /* No condition = infinite loop */
                LLVMBuildBr(ctx->llvm_builder, loop_bb);
            }
            
            /* Save old loop context and set new one */
            LLVMBasicBlockRef old_continue = ctx->loop_continue_block;
            LLVMBasicBlockRef old_break = ctx->loop_break_block;
            ctx->loop_continue_block = inc_bb;   /* continue goes to increment */
            ctx->loop_break_block = end_bb;      /* break goes to end */
            
            /* Generate loop body */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_bb);
            if (body) {
                llvm_codegen_stmt(ctx_opaque, body);
            }
            
            /* Restore old loop context */
            ctx->loop_continue_block = old_continue;
            ctx->loop_break_block = old_break;
            
            /* Branch to increment if no terminator */
            if (!LLVMGetBasicBlockTerminator(loop_bb)) {
                LLVMBuildBr(ctx->llvm_builder, inc_bb);
            }
            
            /* Generate increment */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, inc_bb);
            if (increment) {
                llvm_codegen_expr(ctx_opaque, increment);
            }
            LLVMBuildBr(ctx->llvm_builder, cond_bb);
            
            /* Continue after loop */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_bb);
            ctx->current_block = end_bb;
            break;
        }
            
        case AST_DO_WHILE_STMT: {
            /* do body while (condition) */
            ASTNode *body = stmt->data.while_stmt.body;
            ASTNode *condition = stmt->data.while_stmt.condition;
            
            if (!condition || !body) {
                set_error(ctx, "Invalid do-while statement");
                break;
            }
            
            /* Create basic blocks */
            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "do.body");
            LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "do.cond");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->llvm_context, ctx->current_function, "do.end");
            
            /* Branch to loop body */
            LLVMBuildBr(ctx->llvm_builder, loop_bb);
            
            /* Generate loop body */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, loop_bb);
            llvm_codegen_stmt(ctx_opaque, body);
            
            /* Branch to condition if no terminator */
            if (!LLVMGetBasicBlockTerminator(loop_bb)) {
                LLVMBuildBr(ctx->llvm_builder, cond_bb);
            }
            
            /* Generate condition */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, cond_bb);
            LLVMValueRef cond_val = llvm_codegen_expr(ctx_opaque, condition);
            if (cond_val) {
                LLVMTypeRef cond_type = LLVMTypeOf(cond_val);
                if (LLVMGetTypeKind(cond_type) != LLVMIntegerTypeKind || 
                    LLVMGetIntTypeWidth(cond_type) != 1) {
                    LLVMValueRef zero = LLVMConstInt(cond_type, 0, 0);
                    cond_val = LLVMBuildICmp(ctx->llvm_builder, LLVMIntNE, cond_val, zero, "docond");
                }
                LLVMBuildCondBr(ctx->llvm_builder, cond_val, loop_bb, end_bb);
            }
            
            /* Continue after loop */
            LLVMPositionBuilderAtEnd(ctx->llvm_builder, end_bb);
            ctx->current_block = end_bb;
            break;
        }
        
        /* Break and continue */
        case AST_BREAK_STMT:
            if (ctx->loop_break_block) {
                LLVMBuildBr(ctx->llvm_builder, ctx->loop_break_block);
            } else {
                set_error(ctx, "break statement outside of loop");
                LLVMBuildUnreachable(ctx->llvm_builder);
            }
            break;
            
        case AST_CONTINUE_STMT:
            if (ctx->loop_continue_block) {
                LLVMBuildBr(ctx->llvm_builder, ctx->loop_continue_block);
            } else {
                set_error(ctx, "continue statement outside of loop");
                LLVMBuildUnreachable(ctx->llvm_builder);
            }
            break;
            
        /* Goto and labels - skip for now */
        case AST_GOTO_STMT:
        case AST_LABEL_STMT:
            /* TODO: Implement goto/label support */
            break;
            
        /* Switch statement - simplified version */
        case AST_SWITCH_STMT: {
            /* For now, treat as a series of if-else */
            /* TODO: Proper switch implementation with jump table */
            if (stmt->child_count > 0) {
                llvm_codegen_expr(ctx_opaque, stmt->children[0]);  /* Evaluate switch expression */
                if (stmt->child_count > 1) {
                    llvm_codegen_stmt(ctx_opaque, stmt->children[1]);  /* Execute body */
                }
            }
            break;
        }
        
        case AST_CASE_STMT:
        case AST_DEFAULT_STMT:
            /* Process children */
            for (size_t i = 0; i < stmt->child_count; i++) {
                if (stmt->children[i]) {
                    llvm_codegen_stmt(ctx_opaque, stmt->children[i]);
                }
            }
            break;
            
        default:
            /* Silently skip unsupported statement types to avoid crashes */
            break;
    }
    
    ctx->recursion_depth--;
}

/* Helper: Get LLVM type from AST type node */
static LLVMTypeRef get_llvm_type_from_ast(LLVMBackendContext *ctx, ASTNode *type_node) {
    if (!ctx || !ctx->llvm_context) {
        return NULL;
    }
    
    if (!type_node) {
        /* Default to i32 if no type specified */
        return LLVMInt32TypeInContext(ctx->llvm_context);
    }
    
    /* Safety: Check if node is destroyed or invalid */
    if (type_node->destroyed) {
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
        /* Pointer type - LLVM 20+ uses opaque pointers */
        /* In LLVM 20+, all pointers are opaque (ptr) regardless of pointee type */
        return LLVMPointerTypeInContext(ctx->llvm_context, 0);
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
    
    /* Safety: Skip if recursion is too deep */
    if (ctx->recursion_depth > 400) {
        return;
    }
    
    /* Find function name */
    const char *func_name = "function";  /* default */
    ASTNode *param_list = NULL;
    
    /* Check if function name is stored in func_decl data */
    if (func_decl->data.func_decl.name) {
        func_name = func_decl->data.func_decl.name;
    }
    
    /* Look for identifier and function type in children */
    for (size_t i = 0; i < func_decl->child_count; i++) {
        if (func_decl->children[i]) {
            ASTNode *child = func_decl->children[i];
            
            if (child->type == AST_IDENTIFIER) {
                if (child->data.identifier.name) {
                    func_name = child->data.identifier.name;
                }
                /* Parameter list is a child of the identifier node */
                if (child->child_count > 0 && child->children[0]) {
                    param_list = child->children[0];
                }
            } else if (child->type == AST_FUNCTION_TYPE) {
                /* Function type node - parameters are its children */
                for (size_t j = 0; j < child->child_count; j++) {
                    if (child->children[j] && child->children[j]->type == AST_PARAM_LIST) {
                        param_list = child->children[j];
                        break;
                    }
                }
                /* If no PARAM_LIST, check first child */
                if (!param_list && child->child_count > 0 && child->children[0]) {
                    if (child->children[0]->type == AST_PARAM_LIST) {
                        param_list = child->children[0];
                    }
                }
            }
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
    bool is_variadic = false;
    
    if (param_list) {
        /* Check if variadic (stored in int_literal.value) */
        is_variadic = (param_list->data.int_literal.value != 0);
        
        if (param_list->child_count > 0) {
            param_count = param_list->child_count;
            param_types = xmalloc(sizeof(LLVMTypeRef) * param_count);
            param_names = xmalloc(sizeof(char *) * param_count);
            
            for (size_t i = 0; i < param_count; i++) {
                ASTNode *param = param_list->children[i];
                param_names[i] = NULL;
                param_types[i] = LLVMInt32TypeInContext(ctx->llvm_context);  /* default */
                
                if (param && param->type == AST_PARAM_DECL) {
                    /* Get type from param data if available */
                    if (param->data.var_decl.type) {
                        param_types[i] = get_llvm_type_from_ast(ctx, param->data.var_decl.type);
                    }
                    
                    /* Get name from param data if available */
                    if (param->data.var_decl.name) {
                        param_names[i] = param->data.var_decl.name;
                    }
                    
                    /* Also check children for type and name */
                    for (size_t j = 0; j < param->child_count; j++) {
                        if (param->children[j]) {
                            if (param->children[j]->type == AST_TYPE || param->children[j]->type == AST_POINTER_TYPE) {
                                param_types[i] = get_llvm_type_from_ast(ctx, param->children[j]);
                            } else if (param->children[j]->type == AST_IDENTIFIER) {
                                if (param->children[j]->data.identifier.name) {
                                    param_names[i] = param->children[j]->data.identifier.name;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    /* Create function type with variadic flag */
    LLVMTypeRef func_type = LLVMFunctionType(return_type, param_types, param_count, is_variadic ? 1 : 0);
    
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
    
    /* Only generate body if this is a definition (not just a declaration) */
    if (body) {
        llvm_codegen_stmt((BackendContext *)ctx, body);
        
        /* Ensure ALL basic blocks in the function have terminators */
        LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function);
        while (bb) {
            if (!LLVMGetBasicBlockTerminator(bb)) {
                /* Position builder at end of this block */
                LLVMPositionBuilderAtEnd(ctx->llvm_builder, bb);
                
                /* Add appropriate return based on return type */
                if (LLVMGetTypeKind(return_type) == LLVMVoidTypeKind) {
                    LLVMBuildRetVoid(ctx->llvm_builder);
                } else {
                    /* Return a default value (0 or null) for non-void functions */
                    LLVMValueRef default_val = LLVMConstNull(return_type);
                    LLVMBuildRet(ctx->llvm_builder, default_val);
                }
            }
            bb = LLVMGetNextBasicBlock(bb);
        }
    } else {
        /* This is just a declaration - remove the entry block we created */
        LLVMBasicBlockRef entry_block = LLVMGetFirstBasicBlock(function);
        if (entry_block) {
            LLVMDeleteBasicBlock(entry_block);
        }
    }
    
    /* Cleanup */
    if (param_types) xfree(param_types);
    if (param_names) xfree(param_names);
}

void llvm_codegen_decl(BackendContext *ctx_opaque, ASTNode *decl) {
    if (!ctx_opaque || !decl) return;
    
    LLVMBackendContext *ctx = (LLVMBackendContext *)ctx_opaque;
    
    /* Prevent infinite recursion in declarations too */
    if (ctx->recursion_depth > 100) {
        return;  /* Reduced from 500 to 100 to catch issues faster */
    }
    ctx->recursion_depth++;
    
    /* Ensure we have a module for code generation */
    if (!ctx->llvm_module) {
        set_error(ctx, "No module available for code generation");
        ctx->recursion_depth--;
        return;
    }
    
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
            
        case AST_FUNCTION_PROTO:
            /* Function prototype - just declare it */
            codegen_function_decl(ctx, decl);
            break;
            
        case AST_VAR_DECL:
        case AST_LOCAL_VAR_DECL:
        case AST_GLOBAL_VAR_DECL:
        case AST_STATIC_VAR_DECL:
        case AST_EXTERN_VAR_DECL:
            /* Handle as statement if we're in a function */
            if (ctx->current_function) {
                llvm_codegen_stmt(ctx_opaque, decl);
            } else {
                /* Global variable */
                const char *var_name = decl->data.var_decl.name;
                ASTNode *var_type = decl->data.var_decl.type;
                ASTNode *init_expr = decl->data.var_decl.init;
                
                if (!var_name) break;
                
                /* Get LLVM type */
                LLVMTypeRef llvm_type = get_llvm_type_from_ast(ctx, var_type);
                
                /* Create global variable */
                LLVMValueRef global = LLVMAddGlobal(ctx->llvm_module, llvm_type, var_name);
                
                /* Set initializer */
                if (init_expr && init_expr->type == AST_INTEGER_LITERAL) {
                    LLVMSetInitializer(global, LLVMConstInt(llvm_type, init_expr->data.int_literal.value, 0));
                } else {
                    /* Default initialize to zero */
                    LLVMSetInitializer(global, LLVMConstNull(llvm_type));
                }
                
                /* Add to symbol table */
                symbol_table_add(ctx, var_name, global, llvm_type, true);
            }
            break;
            
        case AST_DECL_STMT:
            /* Declaration statement - only process if in a function */
            if (ctx->current_function) {
                for (size_t i = 0; i < decl->child_count; i++) {
                    if (decl->children[i]) {
                        llvm_codegen_decl(ctx_opaque, decl->children[i]);
                    }
                }
            }
            /* else: Global declarations - skip for now */
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
    
    ctx->recursion_depth--;
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
    
    /* Verify the module before emitting */
    char *error = NULL;
    if (LLVMVerifyModule((LLVMModuleRef)module, LLVMPrintMessageAction, &error)) {
        set_error(ctx, "Module verification failed: %s", error ? error : "unknown error");
        if (error) LLVMDisposeMessage(error);
        return false;
    }
    /* Dispose error message even on success (LLVM may allocate it) */
    if (error) {
        LLVMDisposeMessage(error);
    }
    
    /* Emit object file */
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
    char command[4096] = "clang -no-pie";
    
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
