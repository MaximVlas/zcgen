#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../common/types.h"

/* AST Node creation */
ASTNode *ast_create_node(ASTNodeType type, SourceLocation loc);
void ast_destroy_node(ASTNode *node);

/* Add child to node */
void ast_add_child(ASTNode *parent, ASTNode *child);

/* Specific node creators */
ASTNode *ast_create_translation_unit(SourceLocation loc);
ASTNode *ast_create_function_decl(const char *name, ASTNode *return_type,
                                   ASTNode **params, size_t param_count,
                                   ASTNode *body, SourceLocation loc);
ASTNode *ast_create_var_decl(const char *name, ASTNode *type, ASTNode *init,
                              SourceLocation loc);
ASTNode *ast_create_param_decl(const char *name, ASTNode *type, SourceLocation loc);

/* Statement creators */
ASTNode *ast_create_compound_stmt(SourceLocation loc);
ASTNode *ast_create_if_stmt(ASTNode *condition, ASTNode *then_branch,
                             ASTNode *else_branch, SourceLocation loc);
ASTNode *ast_create_while_stmt(ASTNode *condition, ASTNode *body, SourceLocation loc);
ASTNode *ast_create_for_stmt(ASTNode *init, ASTNode *condition,
                              ASTNode *increment, ASTNode *body, SourceLocation loc);
ASTNode *ast_create_return_stmt(ASTNode *expr, SourceLocation loc);
ASTNode *ast_create_expr_stmt(ASTNode *expr, SourceLocation loc);

/* Expression creators */
ASTNode *ast_create_binary_expr(const char *op, ASTNode *left, ASTNode *right,
                                 SourceLocation loc);
ASTNode *ast_create_unary_expr(const char *op, ASTNode *operand, SourceLocation loc);
ASTNode *ast_create_call_expr(ASTNode *callee, ASTNode **args, size_t arg_count,
                               SourceLocation loc);
ASTNode *ast_create_member_expr(ASTNode *object, const char *member, bool is_arrow,
                                SourceLocation loc);
ASTNode *ast_create_array_subscript(ASTNode *array, ASTNode *index, SourceLocation loc);
ASTNode *ast_create_cast_expr(ASTNode *type, ASTNode *expr, SourceLocation loc);
ASTNode *ast_create_sizeof_expr(ASTNode *operand, SourceLocation loc);

/* Literal creators */
ASTNode *ast_create_integer_literal(int64_t value, SourceLocation loc);
ASTNode *ast_create_float_literal(double value, SourceLocation loc);
ASTNode *ast_create_string_literal(const char *value, SourceLocation loc);
ASTNode *ast_create_char_literal(char value, SourceLocation loc);
ASTNode *ast_create_identifier(const char *name, SourceLocation loc);

/* Type creators */
ASTNode *ast_create_type(const char *name, SourceLocation loc);
ASTNode *ast_create_pointer_type(ASTNode *pointee, SourceLocation loc);
ASTNode *ast_create_array_type(ASTNode *element_type, ASTNode *size, SourceLocation loc);

/* AST traversal */
typedef void (*ASTVisitor)(ASTNode *node, void *data);
void ast_traverse(ASTNode *node, ASTVisitor visitor, void *data);

/* AST printing (for debugging) */
void ast_print(ASTNode *node, int indent);
void ast_dump(ASTNode *node);

#endif /* AST_H */
