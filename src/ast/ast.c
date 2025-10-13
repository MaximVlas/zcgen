#include "ast.h"
#include "../common/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Create base AST node */
ASTNode *ast_create_node(ASTNodeType type, SourceLocation loc) {
    ASTNode *node = xcalloc(1, sizeof(ASTNode));
    node->type = type;
    node->location = loc;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->destroyed = false;
    return node;
}

/* Destroy AST node recursively */
void ast_destroy_node(ASTNode *node) {
    if (!node || node->destroyed) return;
    
    /* Mark as destroyed to prevent double-free */
    node->destroyed = true;
    
    /* Destroy children */
    for (size_t i = 0; i < node->child_count; i++) {
        ast_destroy_node(node->children[i]);
    }
    xfree(node->children);
    
    /* Free node-specific data */
    switch (node->type) {
        case AST_VAR_DECL:
        case AST_GLOBAL_VAR_DECL:
        case AST_LOCAL_VAR_DECL:
        case AST_STATIC_VAR_DECL:
        case AST_EXTERN_VAR_DECL:
        case AST_PARAM_DECL:
            xfree(node->data.var_decl.name);
            break;
        case AST_FUNCTION_DECL:
            xfree(node->data.func_decl.name);
            xfree(node->data.func_decl.params);
            break;
        case AST_BINARY_EXPR:
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
        case AST_LOGICAL_AND_EXPR:
        case AST_LOGICAL_OR_EXPR:
        case AST_EQ_EXPR:
        case AST_NE_EXPR:
        case AST_LT_EXPR:
        case AST_LE_EXPR:
        case AST_GT_EXPR:
        case AST_GE_EXPR:
        case AST_ASSIGN_EXPR:
        case AST_ADD_ASSIGN_EXPR:
        case AST_SUB_ASSIGN_EXPR:
        case AST_MUL_ASSIGN_EXPR:
        case AST_DIV_ASSIGN_EXPR:
        case AST_MOD_ASSIGN_EXPR:
        case AST_AND_ASSIGN_EXPR:
        case AST_OR_ASSIGN_EXPR:
        case AST_XOR_ASSIGN_EXPR:
        case AST_SHL_ASSIGN_EXPR:
        case AST_SHR_ASSIGN_EXPR:
            xfree(node->data.binary_expr.op);
            break;
        case AST_UNARY_EXPR:
        case AST_UNARY_PLUS_EXPR:
        case AST_UNARY_MINUS_EXPR:
        case AST_NOT_EXPR:
        case AST_BIT_NOT_EXPR:
        case AST_DEREF_EXPR:
        case AST_ADDR_OF_EXPR:
        case AST_PRE_INC_EXPR:
        case AST_PRE_DEC_EXPR:
        case AST_POST_INC_EXPR:
        case AST_POST_DEC_EXPR:
            xfree(node->data.unary_expr.op);
            break;
        case AST_IDENTIFIER:
        case AST_MEMBER_EXPR:
        case AST_ARROW_EXPR:
        case AST_STRUCT_DECL:
        case AST_UNION_DECL:
        case AST_STRUCT_TYPE:
        case AST_UNION_TYPE:
        case AST_ENUM_DECL:
        case AST_ENUM_TYPE:
            xfree(node->data.identifier.name);
            break;
        case AST_STRING_LITERAL:
            xfree(node->data.string_literal.value);
            break;
        case AST_TYPE:
        case AST_BUILTIN_TYPE:
            xfree(node->data.type.name);
            break;
        case AST_CALL_EXPR:
            xfree(node->data.call_expr.args);
            break;
        default:
            break;
    }
    
    xfree(node);
}

/* Add child to node */
void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        parent->children = xrealloc(parent->children,
                                   parent->child_capacity * sizeof(ASTNode *));
    }
    
    parent->children[parent->child_count++] = child;
}

/* Translation unit */
ASTNode *ast_create_translation_unit(SourceLocation loc) {
    return ast_create_node(AST_TRANSLATION_UNIT, loc);
}

/* Function declaration */
ASTNode *ast_create_function_decl(const char *name, ASTNode *return_type,
                                   ASTNode **params, size_t param_count,
                                   ASTNode *body, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_FUNCTION_DECL, loc);
    node->data.func_decl.name = xstrdup(name);
    node->data.func_decl.return_type = return_type;
    node->data.func_decl.params = xcalloc(param_count, sizeof(ASTNode *));
    memcpy(node->data.func_decl.params, params, param_count * sizeof(ASTNode *));
    node->data.func_decl.param_count = param_count;
    node->data.func_decl.body = body;
    
    if (return_type) ast_add_child(node, return_type);
    for (size_t i = 0; i < param_count; i++) {
        if (params[i]) ast_add_child(node, params[i]);
    }
    if (body) ast_add_child(node, body);
    
    return node;
}

/* Variable declaration */
ASTNode *ast_create_var_decl(const char *name, ASTNode *type, ASTNode *init,
                              SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_VAR_DECL, loc);
    node->data.var_decl.name = xstrdup(name);
    node->data.var_decl.type = type;
    node->data.var_decl.init = init;
    
    if (type) ast_add_child(node, type);
    if (init) ast_add_child(node, init);
    
    return node;
}

/* Parameter declaration */
ASTNode *ast_create_param_decl(const char *name, ASTNode *type, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_PARAM_DECL, loc);
    node->data.var_decl.name = xstrdup(name);
    node->data.var_decl.type = type;
    node->data.var_decl.init = NULL;
    
    if (type) ast_add_child(node, type);
    
    return node;
}

/* Compound statement */
ASTNode *ast_create_compound_stmt(SourceLocation loc) {
    return ast_create_node(AST_COMPOUND_STMT, loc);
}

/* If statement */
ASTNode *ast_create_if_stmt(ASTNode *condition, ASTNode *then_branch,
                             ASTNode *else_branch, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_IF_STMT, loc);
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    
    if (condition) ast_add_child(node, condition);
    if (then_branch) ast_add_child(node, then_branch);
    if (else_branch) ast_add_child(node, else_branch);
    
    return node;
}

/* While statement */
ASTNode *ast_create_while_stmt(ASTNode *condition, ASTNode *body, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_WHILE_STMT, loc);
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    
    if (condition) ast_add_child(node, condition);
    if (body) ast_add_child(node, body);
    
    return node;
}

/* For statement */
ASTNode *ast_create_for_stmt(ASTNode *init, ASTNode *condition,
                              ASTNode *increment, ASTNode *body, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_FOR_STMT, loc);
    node->data.for_stmt.init = init;
    node->data.for_stmt.condition = condition;
    node->data.for_stmt.increment = increment;
    node->data.for_stmt.body = body;
    
    if (init) ast_add_child(node, init);
    if (condition) ast_add_child(node, condition);
    if (increment) ast_add_child(node, increment);
    if (body) ast_add_child(node, body);
    
    return node;
}

/* Return statement */
ASTNode *ast_create_return_stmt(ASTNode *expr, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_RETURN_STMT, loc);
    if (expr) ast_add_child(node, expr);
    return node;
}

/* Expression statement */
ASTNode *ast_create_expr_stmt(ASTNode *expr, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_EXPR_STMT, loc);
    if (expr) ast_add_child(node, expr);
    return node;
}

/* Binary expression */
ASTNode *ast_create_binary_expr(const char *op, ASTNode *left, ASTNode *right,
                                 SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_BINARY_EXPR, loc);
    node->data.binary_expr.op = xstrdup(op);
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    
    if (left) ast_add_child(node, left);
    if (right) ast_add_child(node, right);
    
    return node;    
}

/* Unary expression */
ASTNode *ast_create_unary_expr(const char *op, ASTNode *operand, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_UNARY_EXPR, loc);
    node->data.unary_expr.op = xstrdup(op);
    node->data.unary_expr.operand = operand;
    
    if (operand) ast_add_child(node, operand);
    
    return node;
}

/* Call expression */
ASTNode *ast_create_call_expr(ASTNode *callee, ASTNode **args, size_t arg_count,
                               SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_CALL_EXPR, loc);
    node->data.call_expr.callee = callee;
    node->data.call_expr.args = xcalloc(arg_count, sizeof(ASTNode *));
    memcpy(node->data.call_expr.args, args, arg_count * sizeof(ASTNode *));
    node->data.call_expr.arg_count = arg_count;
    
    if (callee) ast_add_child(node, callee);
    for (size_t i = 0; i < arg_count; i++) {
        if (args[i]) ast_add_child(node, args[i]);
    }
    
    return node;
}

/* Member expression */
ASTNode *ast_create_member_expr(ASTNode *object, const char *member, bool is_arrow,
                                SourceLocation loc) {
    ASTNode *node = ast_create_node(is_arrow ? AST_ARROW_EXPR : AST_MEMBER_EXPR, loc);
    if (object) ast_add_child(node, object);
    /* Store member name in identifier data */
    node->data.identifier.name = xstrdup(member);
    return node;
}

/* Array subscript */
ASTNode *ast_create_array_subscript(ASTNode *array, ASTNode *index, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_ARRAY_SUBSCRIPT_EXPR, loc);
    if (array) ast_add_child(node, array);
    if (index) ast_add_child(node, index);
    return node;
}

/* Cast expression */
ASTNode *ast_create_cast_expr(ASTNode *type, ASTNode *expr, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_CAST_EXPR, loc);
    if (type) ast_add_child(node, type);
    if (expr) ast_add_child(node, expr);
    return node;
}

/* Sizeof expression */
ASTNode *ast_create_sizeof_expr(ASTNode *operand, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_SIZEOF_EXPR, loc);
    if (operand) ast_add_child(node, operand);
    return node;
}

/* Integer literal */
ASTNode *ast_create_integer_literal(int64_t value, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_INTEGER_LITERAL, loc);
    node->data.int_literal.value = value;
    return node;
}

/* Float literal */
ASTNode *ast_create_float_literal(double value, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_FLOAT_LITERAL, loc);
    node->data.float_literal.value = value;
    return node;
}

/* String literal */
ASTNode *ast_create_string_literal(const char *value, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_STRING_LITERAL, loc);
    node->data.string_literal.value = xstrdup(value);
    return node;
}

/* Char literal */
ASTNode *ast_create_char_literal(char value, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_CHAR_LITERAL, loc);
    node->data.int_literal.value = value;
    return node;
}

/* Identifier */
ASTNode *ast_create_identifier(const char *name, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_IDENTIFIER, loc);
    node->data.identifier.name = xstrdup(name);
    return node;
}

/* Type */
ASTNode *ast_create_type(const char *name, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_TYPE, loc);
    node->data.type.name = xstrdup(name);
    node->data.type.size = 0;
    node->data.type.is_signed = true;
    node->data.type.is_const = false;
    node->data.type.is_volatile = false;
    return node;
}

/* Pointer type */
ASTNode *ast_create_pointer_type(ASTNode *pointee, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_POINTER_TYPE, loc);
    if (pointee) ast_add_child(node, pointee);
    return node;
}

/* Array type */
ASTNode *ast_create_array_type(ASTNode *element_type, ASTNode *size, SourceLocation loc) {
    ASTNode *node = ast_create_node(AST_ARRAY_TYPE, loc);
    if (element_type) ast_add_child(node, element_type);
    if (size) ast_add_child(node, size);
    return node;
}

/* AST traversal */
void ast_traverse(ASTNode *node, ASTVisitor visitor, void *data) {
    if (!node || !visitor) return;
    
    visitor(node, data);
    
    for (size_t i = 0; i < node->child_count; i++) {
        ast_traverse(node->children[i], visitor, data);
    }
}

/* AST printing helper */
static const char *ast_node_type_name(ASTNodeType type) {
    switch (type) {
        case AST_TRANSLATION_UNIT: return "TranslationUnit";
        case AST_FUNCTION_DECL: return "FunctionDecl";
        case AST_VAR_DECL: return "VarDecl";
        case AST_PARAM_DECL: return "ParamDecl";
        case AST_COMPOUND_STMT: return "CompoundStmt";
        case AST_IF_STMT: return "IfStmt";
        case AST_WHILE_STMT: return "WhileStmt";
        case AST_FOR_STMT: return "ForStmt";
        case AST_RETURN_STMT: return "ReturnStmt";
        case AST_EXPR_STMT: return "ExprStmt";
        case AST_BINARY_EXPR: return "BinaryExpr";
        case AST_ADD_EXPR: return "AddExpr";
        case AST_SUB_EXPR: return "SubExpr";
        case AST_MUL_EXPR: return "MulExpr";
        case AST_DIV_EXPR: return "DivExpr";
        case AST_UNARY_EXPR: return "UnaryExpr";
        case AST_CALL_EXPR: return "CallExpr";
        case AST_CAST_EXPR: return "CastExpr";
        case AST_MEMBER_EXPR: return "MemberExpr";
        case AST_ARRAY_SUBSCRIPT_EXPR: return "ArraySubscript";
        case AST_INTEGER_LITERAL: return "IntegerLiteral";
        case AST_FLOAT_LITERAL: return "FloatLiteral";
        case AST_STRING_LITERAL: return "StringLiteral";
        case AST_CHAR_LITERAL: return "CharLiteral";
        case AST_IDENTIFIER: return "Identifier";
        case AST_TYPE: return "Type";
        case AST_POINTER_TYPE: return "PointerType";
        case AST_ARRAY_TYPE: return "ArrayType";
        default: return "Unknown";
    }
}

/* Print AST with indentation */
void ast_print(ASTNode *node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    printf("%s", ast_node_type_name(node->type));
    
    /* Print node-specific info */
    switch (node->type) {
        case AST_FUNCTION_DECL:
            printf(" '%s'", node->data.func_decl.name);
            break;
        case AST_VAR_DECL:
        case AST_PARAM_DECL:
            printf(" '%s'", node->data.var_decl.name);
            break;
        case AST_IDENTIFIER:
            printf(" '%s'", node->data.identifier.name);
            break;
        case AST_INTEGER_LITERAL:
            printf(" %ld", node->data.int_literal.value);
            break;
        case AST_FLOAT_LITERAL:
            printf(" %f", node->data.float_literal.value);
            break;
        case AST_STRING_LITERAL:
            printf(" \"%s\"", node->data.string_literal.value);
            break;
        case AST_BINARY_EXPR:
            printf(" '%s'", node->data.binary_expr.op);
            break;
        case AST_UNARY_EXPR:
            printf(" '%s'", node->data.unary_expr.op);
            break;
        case AST_TYPE:
            printf(" '%s'", node->data.type.name);
            break;
        default:
            break;
    }
    
    printf("\n");
    
    /* Print children */
    for (size_t i = 0; i < node->child_count; i++) {
        ast_print(node->children[i], indent + 1);
    }
}

/* Dump AST (convenience function) */
void ast_dump(ASTNode *node) {
    ast_print(node, 0);
}
