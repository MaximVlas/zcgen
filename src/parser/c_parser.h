#ifndef C_PARSER_H
#define C_PARSER_H

#include <stdbool.h>
#include "parser.h"
#include "../syntax/c_syntax.h"

/* C-specific parser */
typedef struct CParser {
    Parser base;
    CStandard standard;
    
    /* Symbol tables for type checking */
    void *typedef_names;
    void *struct_tags;
    void *union_tags;
    void *enum_tags;
    
    /* Current scope depth */
    int scope_depth;
} CParser;

/* Create C parser */
CParser *c_parser_create(TokenList *tokens, CStandard standard);
void c_parser_destroy(CParser *parser);

/* Parse entry point */
ASTNode *c_parser_parse(CParser *parser);

/* ===== DECLARATIONS ===== */
ASTNode *c_parse_translation_unit(CParser *parser);
ASTNode *c_parse_external_declaration(CParser *parser);
ASTNode *c_parse_function_definition(CParser *parser);
ASTNode *c_parse_declaration(CParser *parser);
ASTNode *c_parse_declaration_specifiers(CParser *parser);
ASTNode *c_parse_declarator(CParser *parser);
ASTNode *c_parse_direct_declarator(CParser *parser);
ASTNode *c_parse_pointer(CParser *parser);
ASTNode *c_parse_parameter_list(CParser *parser);
ASTNode *c_parse_parameter_declaration(CParser *parser);
ASTNode *c_parse_initializer(CParser *parser);
ASTNode *c_parse_initializer_list(CParser *parser);

/* Type specifiers */
ASTNode *c_parse_type_specifier(CParser *parser);
ASTNode *c_parse_struct_or_union_specifier(CParser *parser);
ASTNode *c_parse_struct_declaration_list(CParser *parser);
ASTNode *c_parse_struct_declaration(CParser *parser);
ASTNode *c_parse_enum_specifier(CParser *parser);
ASTNode *c_parse_enumerator_list(CParser *parser);
ASTNode *c_parse_enumerator(CParser *parser);

/* Type qualifiers */
ASTNode *c_parse_type_qualifier(CParser *parser);
ASTNode *c_parse_type_qualifier_list(CParser *parser);

/* Storage class specifiers */
ASTNode *c_parse_storage_class_specifier(CParser *parser);

/* Function specifiers */
ASTNode *c_parse_function_specifier(CParser *parser);

/* ===== STATEMENTS ===== */
ASTNode *c_parse_statement(CParser *parser);
ASTNode *c_parse_labeled_statement(CParser *parser);
ASTNode *c_parse_compound_statement(CParser *parser);
ASTNode *c_parse_expression_statement(CParser *parser);
ASTNode *c_parse_selection_statement(CParser *parser);
ASTNode *c_parse_iteration_statement(CParser *parser);
ASTNode *c_parse_jump_statement(CParser *parser);
ASTNode *c_parse_asm_statement(CParser *parser);

/* ===== EXPRESSIONS ===== */
ASTNode *c_parse_expression(CParser *parser);
ASTNode *c_parse_assignment_expression(CParser *parser);
ASTNode *c_parse_conditional_expression(CParser *parser);
ASTNode *c_parse_logical_or_expression(CParser *parser);
ASTNode *c_parse_logical_and_expression(CParser *parser);
ASTNode *c_parse_inclusive_or_expression(CParser *parser);
ASTNode *c_parse_exclusive_or_expression(CParser *parser);
ASTNode *c_parse_and_expression(CParser *parser);
ASTNode *c_parse_equality_expression(CParser *parser);
ASTNode *c_parse_relational_expression(CParser *parser);
ASTNode *c_parse_shift_expression(CParser *parser);
ASTNode *c_parse_additive_expression(CParser *parser);
ASTNode *c_parse_multiplicative_expression(CParser *parser);
ASTNode *c_parse_cast_expression(CParser *parser);
ASTNode *c_parse_unary_expression(CParser *parser);
ASTNode *c_parse_postfix_expression(CParser *parser);
ASTNode *c_parse_primary_expression(CParser *parser);

/* Expression helpers */
ASTNode *c_parse_argument_expression_list(CParser *parser);
ASTNode *c_parse_constant_expression(CParser *parser);

/* ===== C99/C11/C23 SPECIFIC ===== */
ASTNode *c_parse_generic_selection(CParser *parser);        /* C11 _Generic */
ASTNode *c_parse_static_assert(CParser *parser);            /* C11 _Static_assert */
ASTNode *c_parse_alignas_specifier(CParser *parser);        /* C11 _Alignas */
ASTNode *c_parse_atomic_type_specifier(CParser *parser);    /* C11 _Atomic */

/* ===== GNU EXTENSIONS ===== */
ASTNode *c_parse_attribute(CParser *parser);                /* __attribute__ */
ASTNode *c_parse_asm_operands(CParser *parser);             /* asm operands */
ASTNode *c_parse_typeof(CParser *parser);                   /* __typeof__ */

/* ===== UTILITIES ===== */
bool c_is_type_specifier(CParser *parser);
bool c_is_type_qualifier(CParser *parser);
bool c_is_storage_class_specifier(CParser *parser);
bool c_is_function_specifier(CParser *parser);
bool c_is_declaration_specifier(CParser *parser);
bool c_is_type_name(CParser *parser, const char *name);

/* Scope management */
void c_parser_enter_scope(CParser *parser);
void c_parser_exit_scope(CParser *parser);
void c_parser_add_typedef(CParser *parser, const char *name);

#endif /* C_PARSER_H */
