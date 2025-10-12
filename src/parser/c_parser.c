#include "c_parser.h"
#include "../common/memory.h"
#include "../common/error.h"
#include "../ast/ast.h"
#include <string.h>

/* Simple symbol table entry */
typedef struct SymbolEntry {
    char *name;
    struct SymbolEntry *next;
} SymbolEntry;

/* Simple hash table for symbol tracking */
#define SYMBOL_TABLE_SIZE 256

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SYMBOL_TABLE_SIZE;
}

static void symbol_table_init(void **table) {
    *table = xcalloc(SYMBOL_TABLE_SIZE, sizeof(SymbolEntry*));
}

static void symbol_table_destroy(void *table) {
    if (!table) return;
    
    SymbolEntry **entries = (SymbolEntry**)table;
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        SymbolEntry *entry = entries[i];
        while (entry) {
            SymbolEntry *next = entry->next;
            xfree(entry->name);
            xfree(entry);
            entry = next;
        }
    }
    xfree(table);
}

static void symbol_table_add(void *table, const char *name) {
    if (!table || !name) return;
    
    SymbolEntry **entries = (SymbolEntry**)table;
    unsigned int index = hash_string(name);
    
    /* Check if already exists */
    SymbolEntry *entry = entries[index];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return; /* Already exists */
        }
        entry = entry->next;
    }
    
    /* Add new entry */
    SymbolEntry *new_entry = xcalloc(1, sizeof(SymbolEntry));
    new_entry->name = xstrdup(name);
    new_entry->next = entries[index];
    entries[index] = new_entry;
}

static bool symbol_table_contains(void *table, const char *name) {
    if (!table || !name) return false;
    
    SymbolEntry **entries = (SymbolEntry**)table;
    unsigned int index = hash_string(name);
    
    SymbolEntry *entry = entries[index];
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return true;
        }
        entry = entry->next;
    }
    return false;
}

/* Helper macros for cleaner code - cast CTokenType to TokenType */
#define CURRENT(p) parser_current(&(p)->base)
#define PEEK(p, n) parser_peek(&(p)->base, n)
#define MATCH(p, t) parser_match(&(p)->base, (TokenType)(t))
#define CHECK(p, t) parser_check(&(p)->base, (TokenType)(t))
#define ADVANCE(p) parser_advance(&(p)->base)
#define EXPECT(p, t, msg) parser_expect(&(p)->base, (TokenType)(t), msg)
#define AT_END(p) parser_at_end(&(p)->base)
#define ERROR(p, msg) parser_error(&(p)->base, msg)

/* ===== GCC EXTENSIONS ===== */

/* Parse GCC __attribute__ */
static void c_parse_gcc_attribute(CParser *parser) {
    if (!CHECK(parser, TOKEN___ATTRIBUTE__)) {
        return;
    }
    
    ADVANCE(parser);  /* consume __attribute__ */
    
    if (!MATCH(parser, TOKEN_LPAREN)) {
        return;
    }
    
    if (!MATCH(parser, TOKEN_LPAREN)) {
        return;
    }
    
    /* Parse attribute list - skip everything until matching )) */
    int paren_depth = 2;
    while (!AT_END(parser) && paren_depth > 0) {
        if (CHECK(parser, TOKEN_LPAREN)) {
            paren_depth++;
        } else if (CHECK(parser, TOKEN_RPAREN)) {
            paren_depth--;
        }
        ADVANCE(parser);
    }
}

/* Parse GCC __asm__ */
static void c_parse_gcc_asm(CParser *parser) {
    if (!CHECK(parser, TOKEN___ASM__)) {
        return;
    }
    
    ADVANCE(parser);  /* consume __asm__ */
    
    if (!MATCH(parser, TOKEN_LPAREN)) {
        return;
    }
    
    /* Skip asm content until matching ) */
    int paren_depth = 1;
    while (!AT_END(parser) && paren_depth > 0) {
        if (CHECK(parser, TOKEN_LPAREN)) {
            paren_depth++;
        } else if (CHECK(parser, TOKEN_RPAREN)) {
            paren_depth--;
        }
        ADVANCE(parser);
    }
}

/* Parse all GCC attributes/extensions that might appear */
static void c_parse_gcc_extensions(CParser *parser) {
    while (true) {
        if (CHECK(parser, TOKEN___ATTRIBUTE__)) {
            c_parse_gcc_attribute(parser);
        } else if (CHECK(parser, TOKEN___ASM__)) {
            c_parse_gcc_asm(parser);
        } else if (MATCH(parser, TOKEN___EXTENSION__)) {
            /* __extension__ just suppresses warnings, skip it */
            continue;
        } else {
            break;
        }
    }
}

/* ===== LIFECYCLE ===== */

CParser *c_parser_create(TokenList *tokens, CStandard standard) {
    CParser *parser = xcalloc(1, sizeof(CParser));
    parser->base.tokens = tokens;
    parser->base.position = 0;
    parser->base.current = tokens->head;
    /* Select syntax based on C standard */
    switch (standard) {
        case C_STD_C89:
        case C_STD_C90:
        case C_STD_GNU89:
            parser->base.syntax = syntax_c99_create(); /* Use C99 for now */
            break;
        default:
            parser->base.syntax = syntax_c99_create();
            break;
    }
    parser->base.panic_mode = false;
    parser->base.error_count = 0;
    parser->standard = standard;
    parser->scope_depth = 0;
    /* Initialize symbol tables */
    symbol_table_init(&parser->typedef_names);
    symbol_table_init(&parser->struct_tags);
    symbol_table_init(&parser->union_tags);
    symbol_table_init(&parser->enum_tags);
    return parser;
}

void c_parser_destroy(CParser *parser) {
    /* Free symbol tables */
    symbol_table_destroy(parser->typedef_names);
    symbol_table_destroy(parser->struct_tags);
    symbol_table_destroy(parser->union_tags);
    symbol_table_destroy(parser->enum_tags);
    /* Free syntax definition */
    if (parser->base.syntax) {
        syntax_c99_destroy(parser->base.syntax);
    }
    xfree(parser);
}

ASTNode *c_parser_parse(CParser *parser) {
    return c_parse_translation_unit(parser);
}

/* ===== DECLARATIONS ===== */

ASTNode *c_parse_translation_unit(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *unit = ast_create_translation_unit(loc);
    
    while (!AT_END(parser)) {
        ASTNode *decl = c_parse_external_declaration(parser);
        if (decl) {
            ast_add_child(unit, decl);
        }
        
        if (parser->base.panic_mode) {
            parser_synchronize(&parser->base);
        }
    }
    
    return unit;
}

ASTNode *c_parse_external_declaration(CParser *parser) {
    /* External declaration is either a function definition or declaration */
    
    if (c_is_declaration_specifier(parser)) {
        return c_parse_declaration(parser);
    }
    
    ERROR(parser, "expected declaration");
    return NULL;
}

ASTNode *c_parse_function_definition(CParser *parser) {
    /* Function definition is handled in c_parse_declaration */
    return c_parse_declaration(parser);
}

ASTNode *c_parse_declaration(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    /* Parse any leading GCC extensions */
    c_parse_gcc_extensions(parser);
    
    /* Check if this is a typedef */
    bool is_typedef = CHECK(parser, TOKEN_TYPEDEF);
    
    /* Parse declaration specifiers (storage class, type, qualifiers) */
    ASTNode *decl_specs = c_parse_declaration_specifiers(parser);
    if (!decl_specs) {
        return NULL;
    }
    
    /* Parse GCC attributes after declaration specifiers */
    c_parse_gcc_extensions(parser);
    
    /* Check for just type declaration (struct/union/enum without declarator) */
    if (MATCH(parser, TOKEN_SEMICOLON)) {
        return decl_specs;
    }
    
    /* Parse declarator */
    ASTNode *declarator = c_parse_declarator(parser);
    
    /* Parse GCC attributes after declarator */
    c_parse_gcc_extensions(parser);
    
    /* If this is a typedef, register the typedef name */
    if (is_typedef && declarator && declarator->type == AST_IDENTIFIER) {
        if (declarator->data.identifier.name) {
            c_parser_add_typedef(parser, declarator->data.identifier.name);
        }
    }
    
    /* Check for function definition vs declaration */
    if (declarator && CHECK(parser, TOKEN_LBRACE)) {
        /* Function definition */
        ASTNode *body = c_parse_compound_statement(parser);
        /* Extract function name from declarator */
        const char *func_name = "function"; /* Simplified - would extract from declarator */
        ASTNode *func = ast_create_function_decl(func_name, decl_specs, NULL, 0, body, loc);
        /* Attach declarator as child to preserve it */
        if (declarator) {
            ast_add_child(func, declarator);
        }
        return func;
    }
    
    /* Variable declaration with optional initializer */
    ASTNode *init = NULL;
    if (MATCH(parser, TOKEN_EQUAL)) {
        init = c_parse_initializer(parser);
    }
    
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after declaration");
    
    /* Extract variable name from declarator */
    const char *var_name = "variable"; /* Simplified - would extract from declarator */
    ASTNode *var = ast_create_var_decl(var_name, decl_specs, init, loc);
    /* Attach declarator as child to preserve it */
    if (declarator) {
        ast_add_child(var, declarator);
    }
    return var;
}

ASTNode *c_parse_declaration_specifiers(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *specs = ast_create_node(AST_TYPE, loc);
    
    /* Parse all declaration specifiers */
    while (c_is_declaration_specifier(parser)) {
        if (c_is_storage_class_specifier(parser)) {
            /* auto, register, static, extern, typedef */
            ADVANCE(parser);
        } else if (c_is_type_specifier(parser)) {
            ASTNode *type = c_parse_type_specifier(parser);
            if (type) {
                ast_add_child(specs, type);
            }
        } else if (c_is_type_qualifier(parser)) {
            /* const, volatile, restrict, _Atomic */
            ADVANCE(parser);
        } else if (c_is_function_specifier(parser)) {
            /* inline, _Noreturn */
            ADVANCE(parser);
        } else {
            break;
        }
    }
    
    return specs;
}

ASTNode *c_parse_declarator(CParser *parser) {
    /* pointer* direct-declarator */
    ASTNode *pointer = c_parse_pointer(parser);
    ASTNode *direct = c_parse_direct_declarator(parser);
    
    if (pointer && direct) {
        ast_add_child(pointer, direct);
        return pointer;
    }
    
    return direct ? direct : pointer;
}

ASTNode *c_parse_direct_declarator(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *declarator = NULL;
    
    if (CHECK(parser, TOKEN_IDENTIFIER)) {
        declarator = ast_create_identifier(CURRENT(parser)->lexeme, loc);
        ADVANCE(parser);
    } else if (MATCH(parser, TOKEN_LPAREN)) {
        /* Parenthesized declarator */
        declarator = c_parse_declarator(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after declarator");
    } else {
        return NULL;
    }
    
    /* Postfix: arrays and functions */
    while (true) {
        if (MATCH(parser, TOKEN_LBRACKET)) {
            /* Array declarator */
            ASTNode *size = NULL;
            if (!CHECK(parser, TOKEN_RBRACKET)) {
                size = c_parse_assignment_expression(parser);
            }
            EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array size");
            
            ASTNode *array = ast_create_array_type(declarator, size, loc);
            declarator = array;
            
        } else if (MATCH(parser, TOKEN_LPAREN)) {
            /* Function declarator */
            ASTNode *params = NULL;
            if (!CHECK(parser, TOKEN_RPAREN)) {
                params = c_parse_parameter_list(parser);
            }
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after parameters");
            
            /* Attach params to declarator to preserve them */
            if (params && declarator) {
                ast_add_child(declarator, params);
            } else if (params) {
                /* Free orphaned params if no declarator */
                ast_destroy_node(params);
            }
        } else {
            break;
        }
    }
    
    return declarator;
}

ASTNode *c_parse_pointer(CParser *parser) {
    if (!MATCH(parser, TOKEN_STAR)) {
        return NULL;
    }
    
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *pointer = ast_create_node(AST_POINTER_TYPE, loc);
    
    /* Type qualifiers (const, volatile, restrict) */
    while (c_is_type_qualifier(parser)) {
        ADVANCE(parser);
    }
    
    /* Nested pointer */
    ASTNode *nested = c_parse_pointer(parser);
    if (nested) {
        ast_add_child(pointer, nested);
    }
    
    return pointer;
}

ASTNode *c_parse_parameter_list(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *list = ast_create_node(AST_PARAM_LIST, loc);
    
    do {
        /* Check for ... (variadic) */
        if (MATCH(parser, TOKEN_ELLIPSIS)) {
            /* Variadic function - marked by ellipsis token */
            break;
        }
        
        ASTNode *param = c_parse_parameter_declaration(parser);
        if (param) {
            ast_add_child(list, param);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    return list;
}

ASTNode *c_parse_parameter_declaration(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    ASTNode *specs = c_parse_declaration_specifiers(parser);
    ASTNode *declarator = c_parse_declarator(parser);
    
    /* Extract parameter name from declarator */
    const char *param_name = "param"; /* Simplified - would extract from declarator */
    ASTNode *param = ast_create_param_decl(param_name, specs, loc);
    
    /* Attach declarator as child to preserve it */
    if (declarator) {
        ast_add_child(param, declarator);
    }
    
    return param;
}

ASTNode *c_parse_initializer(CParser *parser) {
    if (MATCH(parser, TOKEN_LBRACE)) {
        /* Aggregate initializer: { ... } */
        ASTNode *list = c_parse_initializer_list(parser);
        MATCH(parser, TOKEN_COMMA); /* Optional trailing comma */
        EXPECT(parser, TOKEN_RBRACE, "expected '}' after initializer list");
        return list;
    }
    
    /* Single expression initializer */
    return c_parse_assignment_expression(parser);
}

ASTNode *c_parse_initializer_list(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *list = ast_create_node(AST_INIT_LIST_EXPR, loc);
    
    do {
        /* C99 designated initializers: .member = or [index] = */
        if (MATCH(parser, TOKEN_DOT)) {
            EXPECT(parser, TOKEN_IDENTIFIER, "expected member name");
            EXPECT(parser, TOKEN_EQUAL, "expected '=' after designator");
        } else if (MATCH(parser, TOKEN_LBRACKET)) {
            c_parse_constant_expression(parser);
            EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array index");
            EXPECT(parser, TOKEN_EQUAL, "expected '=' after designator");
        }
        
        ASTNode *init = c_parse_initializer(parser);
        if (init) {
            ast_add_child(list, init);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
        
        /* Check for trailing comma before } */
        if (CHECK(parser, TOKEN_RBRACE)) {
            break;
        }
    } while (true);
    
    return list;
}

/* ===== TYPE SPECIFIERS ===== */

ASTNode *c_parse_type_specifier(CParser *parser) {
    Token *token = CURRENT(parser);
    SourceLocation loc = token->location;
    
    switch (token->type) {
        /* Basic types → LLVM: i8, i16, i32, i64, float, double */
        case TOKEN_VOID:
            ADVANCE(parser);
            return ast_create_type("void", loc);
            
        case TOKEN_CHAR:
            ADVANCE(parser);
            return ast_create_type("char", loc);
            
        case TOKEN_SHORT:
            ADVANCE(parser);
            return ast_create_type("short", loc);
            
        case TOKEN_INT:
            ADVANCE(parser);
            return ast_create_type("int", loc);
            
        case TOKEN_LONG:
            ADVANCE(parser);
            return ast_create_type("long", loc);
            
        case TOKEN_FLOAT:
            ADVANCE(parser);
            return ast_create_type("float", loc);
            
        case TOKEN_DOUBLE:
            ADVANCE(parser);
            return ast_create_type("double", loc);
            
        case TOKEN_SIGNED:
            ADVANCE(parser);
            return ast_create_type("signed", loc);
            
        case TOKEN_UNSIGNED:
            ADVANCE(parser);
            return ast_create_type("unsigned", loc);
            
        case TOKEN__BOOL:
            ADVANCE(parser);
            return ast_create_type("_Bool", loc);
            
        case TOKEN__COMPLEX:
            ADVANCE(parser);
            return ast_create_type("_Complex", loc);
            
        case TOKEN__IMAGINARY:
            ADVANCE(parser);
            return ast_create_type("_Imaginary", loc);
            
        /* Aggregate types → LLVM: struct type */
        case TOKEN_STRUCT:
        case TOKEN_UNION:
            return c_parse_struct_or_union_specifier(parser);
            
        /* Enum → LLVM: i32 */
        case TOKEN_ENUM:
            return c_parse_enum_specifier(parser);
            
        /* Typedef name */
        case TOKEN_IDENTIFIER:
            if (c_is_type_name(parser, token->lexeme)) {
                ADVANCE(parser);
                return ast_create_type(token->lexeme, loc);
            }
            break;
            
        /* GNU/C23 extensions */
        case TOKEN___TYPEOF__:
        case TOKEN_TYPEOF:
            return c_parse_typeof(parser);
            
        case TOKEN__ATOMIC:
            if (parser->standard >= C_STD_C11) {
                return c_parse_atomic_type_specifier(parser);
            }
            break;
            
        default:
            break;
    }
    
    return NULL;
}

ASTNode *c_parse_struct_or_union_specifier(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    bool is_union = CHECK(parser, TOKEN_UNION);
    ADVANCE(parser); /* struct or union */
    
    /* Optional tag */
    char *tag = NULL;
    if (CHECK(parser, TOKEN_IDENTIFIER)) {
        tag = xstrdup(CURRENT(parser)->lexeme);
        ADVANCE(parser);
    }
    
    /* Optional body */
    if (MATCH(parser, TOKEN_LBRACE)) {
        ASTNode *body = c_parse_struct_declaration_list(parser);
        EXPECT(parser, TOKEN_RBRACE, "expected '}' after struct body");
        
        ASTNode *node = ast_create_node(is_union ? AST_UNION_DECL : AST_STRUCT_DECL, loc);
        if (tag) {
            node->data.identifier.name = tag;
        }
        if (body) {
            ast_add_child(node, body);
        }
        return node;
    }
    
    if (!tag) {
        ERROR(parser, "expected struct tag or body");
        return NULL;
    }
    
    ASTNode *node = ast_create_node(is_union ? AST_UNION_TYPE : AST_STRUCT_TYPE, loc);
    node->data.identifier.name = tag;
    return node;
}

ASTNode *c_parse_struct_declaration_list(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);
    
    while (!CHECK(parser, TOKEN_RBRACE) && !AT_END(parser)) {
        ASTNode *decl = c_parse_struct_declaration(parser);
        if (decl) {
            ast_add_child(list, decl);
        }
    }
    
    return list;
}

ASTNode *c_parse_struct_declaration(CParser *parser) {
    /* Similar to regular declaration but for struct members */
    ASTNode *specs = c_parse_declaration_specifiers(parser);
    
    /* Parse declarators (can be multiple) */
    do {
        ASTNode *declarator = c_parse_declarator(parser);
        
        /* Attach declarator to specs if both exist */
        if (declarator && specs) {
            ast_add_child(specs, declarator);
        } else if (declarator) {
            /* Free orphaned declarator */
            ast_destroy_node(declarator);
        }
        
        /* Optional bitfield */
        if (MATCH(parser, TOKEN_COLON)) {
            c_parse_constant_expression(parser);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after struct member");
    
    return specs;
}

ASTNode *c_parse_enum_specifier(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* enum */
    
    /* Optional tag */
    char *tag = NULL;
    if (CHECK(parser, TOKEN_IDENTIFIER)) {
        tag = xstrdup(CURRENT(parser)->lexeme);
        ADVANCE(parser);
    }
    
    /* Optional body */
    if (MATCH(parser, TOKEN_LBRACE)) {
        ASTNode *body = c_parse_enumerator_list(parser);
        MATCH(parser, TOKEN_COMMA); /* Optional trailing comma */
        EXPECT(parser, TOKEN_RBRACE, "expected '}' after enum body");
        
        ASTNode *node = ast_create_node(AST_ENUM_DECL, loc);
        if (tag) {
            node->data.identifier.name = tag;
        }
        if (body) {
            ast_add_child(node, body);
        }
        return node;
    }
    
    if (!tag) {
        ERROR(parser, "expected enum tag or body");
        return NULL;
    }
    
    ASTNode *node = ast_create_node(AST_ENUM_TYPE, loc);
    node->data.identifier.name = tag;
    return node;
}

ASTNode *c_parse_enumerator_list(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);
    
    do {
        ASTNode *enumerator = c_parse_enumerator(parser);
        if (enumerator) {
            ast_add_child(list, enumerator);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
        
        /* Check for trailing comma before } */
        if (CHECK(parser, TOKEN_RBRACE)) {
            break;
        }
    } while (true);
    
    return list;
}

ASTNode *c_parse_enumerator(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    if (!CHECK(parser, TOKEN_IDENTIFIER)) {
        ERROR(parser, "expected enumerator name");
        return NULL;
    }
    
    char *name = xstrdup(CURRENT(parser)->lexeme);
    ADVANCE(parser);
    
    ASTNode *value = NULL;
    if (MATCH(parser, TOKEN_EQUAL)) {
        value = c_parse_constant_expression(parser);
    }
    
    ASTNode *node = ast_create_node(AST_ENUM_CONSTANT, loc);
    node->data.identifier.name = name;
    if (value) {
        ast_add_child(node, value);
    }
    
    return node;
}

ASTNode *c_parse_type_qualifier(CParser *parser) {
    /* Type qualifiers don't create nodes, just consumed */
    if (c_is_type_qualifier(parser)) {
        ADVANCE(parser);
    }
    return NULL;
}

ASTNode *c_parse_type_qualifier_list(CParser *parser) {
    while (c_is_type_qualifier(parser)) {
        ADVANCE(parser);
    }
    return NULL;
}

ASTNode *c_parse_storage_class_specifier(CParser *parser) {
    /* Storage class specifiers don't create nodes, just consumed */
    if (c_is_storage_class_specifier(parser)) {
        ADVANCE(parser);
    }
    return NULL;
}

ASTNode *c_parse_function_specifier(CParser *parser) {
    /* Function specifiers don't create nodes, just consumed */
    if (c_is_function_specifier(parser)) {
        ADVANCE(parser);
    }
    return NULL;
}

/* ===== STATEMENTS ===== */

ASTNode *c_parse_statement(CParser *parser) {
    Token *token = CURRENT(parser);
    
    switch (token->type) {
        case TOKEN_IF:
        case TOKEN_SWITCH:
            return c_parse_selection_statement(parser);
            
        case TOKEN_WHILE:
        case TOKEN_DO:
        case TOKEN_FOR:
            return c_parse_iteration_statement(parser);
            
        case TOKEN_GOTO:
        case TOKEN_CONTINUE:
        case TOKEN_BREAK:
        case TOKEN_RETURN:
            return c_parse_jump_statement(parser);
            
        case TOKEN_LBRACE:
            return c_parse_compound_statement(parser);
            
        case TOKEN_CASE:
        case TOKEN_DEFAULT:
            return c_parse_labeled_statement(parser);
            
        case TOKEN___ASM__:
            return c_parse_asm_statement(parser);
            
        case TOKEN_IDENTIFIER:
            /* Could be label or expression */
            if (PEEK(parser, 1) && PEEK(parser, 1)->type == (TokenType)TOKEN_COLON) {
                return c_parse_labeled_statement(parser);
            }
            /* Fall through to expression statement */
            __attribute__((fallthrough));
            
        default:
            return c_parse_expression_statement(parser);
    }
}

ASTNode *c_parse_labeled_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    if (MATCH(parser, TOKEN_CASE)) {
        /* case → LLVM: switch case */
        ASTNode *expr = c_parse_constant_expression(parser);
        EXPECT(parser, TOKEN_COLON, "expected ':' after case value");
        ASTNode *stmt = c_parse_statement(parser);
        
        ASTNode *case_stmt = ast_create_node(AST_CASE_STMT, loc);
        if (expr) ast_add_child(case_stmt, expr);
        if (stmt) ast_add_child(case_stmt, stmt);
        return case_stmt;
        
    } else if (MATCH(parser, TOKEN_DEFAULT)) {
        /* default → LLVM: switch default */
        EXPECT(parser, TOKEN_COLON, "expected ':' after 'default'");
        ASTNode *stmt = c_parse_statement(parser);
        
        ASTNode *default_stmt = ast_create_node(AST_DEFAULT_STMT, loc);
        if (stmt) ast_add_child(default_stmt, stmt);
        return default_stmt;
        
    } else if (CHECK(parser, TOKEN_IDENTIFIER)) {
        /* label: → LLVM: basic block label */
        char *label = xstrdup(CURRENT(parser)->lexeme);
        ADVANCE(parser);
        EXPECT(parser, TOKEN_COLON, "expected ':' after label");
        ASTNode *stmt = c_parse_statement(parser);
        
        ASTNode *label_stmt = ast_create_node(AST_LABEL_STMT, loc);
        label_stmt->data.identifier.name = label;
        if (stmt) ast_add_child(label_stmt, stmt);
        return label_stmt;
    }
    
    return NULL;
}

ASTNode *c_parse_compound_statement(CParser *parser) {
    /* { ... } → LLVM: basic block */
    SourceLocation loc = CURRENT(parser)->location;
    EXPECT(parser, TOKEN_LBRACE, "expected '{'");
    
    c_parser_enter_scope(parser);
    
    ASTNode *compound = ast_create_compound_stmt(loc);
    
    while (!CHECK(parser, TOKEN_RBRACE) && !AT_END(parser)) {
        ASTNode *stmt = NULL;
        
        /* Could be declaration or statement */
        if (c_is_declaration_specifier(parser)) {
            stmt = c_parse_declaration(parser);
        } else {
            stmt = c_parse_statement(parser);
        }
        
        if (stmt) {
            ast_add_child(compound, stmt);
        }
        
        if (parser->base.panic_mode) {
            parser_synchronize(&parser->base);
        }
    }
    
    EXPECT(parser, TOKEN_RBRACE, "expected '}'");
    
    c_parser_exit_scope(parser);
    
    return compound;
}

ASTNode *c_parse_expression_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    /* Empty statement: ; */
    if (MATCH(parser, TOKEN_SEMICOLON)) {
        return ast_create_node(AST_NULL_STMT, loc);
    }
    
    /* Expression followed by semicolon */
    ASTNode *expr = c_parse_expression(parser);
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after expression");
    
    return ast_create_expr_stmt(expr, loc);
}

ASTNode *c_parse_selection_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    if (MATCH(parser, TOKEN_IF)) {
        /* if → LLVM: br instruction with basic blocks */
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'if'");
        ASTNode *condition = c_parse_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");
        
        ASTNode *then_branch = c_parse_statement(parser);
        ASTNode *else_branch = NULL;
        
        if (MATCH(parser, TOKEN_ELSE)) {
            else_branch = c_parse_statement(parser);
        }
        
        return ast_create_if_stmt(condition, then_branch, else_branch, loc);
        
    } else if (MATCH(parser, TOKEN_SWITCH)) {
        /* switch → LLVM: switch instruction */
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'switch'");
        ASTNode *expr = c_parse_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after expression");
        
        ASTNode *body = c_parse_statement(parser);
        
        ASTNode *switch_stmt = ast_create_node(AST_SWITCH_STMT, loc);
        if (expr) ast_add_child(switch_stmt, expr);
        if (body) ast_add_child(switch_stmt, body);
        
        return switch_stmt;
    }
    
    return NULL;
}

ASTNode *c_parse_iteration_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    if (MATCH(parser, TOKEN_WHILE)) {
        /* while → LLVM: loop with basic blocks */
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'while'");
        ASTNode *condition = c_parse_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");
        
        ASTNode *body = c_parse_statement(parser);
        
        return ast_create_while_stmt(condition, body, loc);
        
    } else if (MATCH(parser, TOKEN_DO)) {
        /* do-while → LLVM: loop with basic blocks */
        ASTNode *body = c_parse_statement(parser);
        EXPECT(parser, TOKEN_WHILE, "expected 'while' after do body");
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'while'");
        ASTNode *condition = c_parse_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after condition");
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after do-while");
        
        ASTNode *do_while = ast_create_node(AST_DO_WHILE_STMT, loc);
        if (condition) ast_add_child(do_while, condition);
        if (body) ast_add_child(do_while, body);
        
        return do_while;
        
    } else if (MATCH(parser, TOKEN_FOR)) {
        /* for → LLVM: loop with basic blocks */
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after 'for'");
        
        ASTNode *init = NULL;
        if (!CHECK(parser, TOKEN_SEMICOLON)) {
            if (c_is_declaration_specifier(parser)) {
                init = c_parse_declaration(parser);
            } else {
                init = c_parse_expression(parser);
                EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after for init");
            }
        } else {
            ADVANCE(parser);
        }
        
        ASTNode *condition = NULL;
        if (!CHECK(parser, TOKEN_SEMICOLON)) {
            condition = c_parse_expression(parser);
        }
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after for condition");
        
        ASTNode *increment = NULL;
        if (!CHECK(parser, TOKEN_RPAREN)) {
            increment = c_parse_expression(parser);
        }
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after for clauses");
        
        ASTNode *body = c_parse_statement(parser);
        
        return ast_create_for_stmt(init, condition, increment, body, loc);
    }
    
    return NULL;
}

ASTNode *c_parse_jump_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    
    if (MATCH(parser, TOKEN_GOTO)) {
        /* goto → LLVM: br to labeled block */
        EXPECT(parser, TOKEN_IDENTIFIER, "expected label name after 'goto'");
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after goto");
        return ast_create_node(AST_GOTO_STMT, loc);
        
    } else if (MATCH(parser, TOKEN_CONTINUE)) {
        /* continue → LLVM: br to loop header */
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after 'continue'");
        return ast_create_node(AST_CONTINUE_STMT, loc);
        
    } else if (MATCH(parser, TOKEN_BREAK)) {
        /* break → LLVM: br to exit block */
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after 'break'");
        return ast_create_node(AST_BREAK_STMT, loc);
        
    } else if (MATCH(parser, TOKEN_RETURN)) {
        /* return → LLVM: ret instruction */
        ASTNode *expr = NULL;
        if (!CHECK(parser, TOKEN_SEMICOLON)) {
            expr = c_parse_expression(parser);
        }
        EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after return");
        return ast_create_return_stmt(expr, loc);
    }
    
    return NULL;
}

ASTNode *c_parse_asm_statement(CParser *parser) {
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* __asm__ or asm */
    
    bool is_volatile = false;
    if (MATCH(parser, TOKEN_VOLATILE) || MATCH(parser, TOKEN___VOLATILE__)) {
        is_volatile = true;
    }
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after asm");
    
    /* Assembly string */
    if (!CHECK(parser, TOKEN_STRING_LITERAL)) {
        ERROR(parser, "expected assembly string");
        return NULL;
    }
    
    char *asm_string = xstrdup(CURRENT(parser)->lexeme);
    ADVANCE(parser);
    
    /* Optional output/input operands and clobbers */
    if (MATCH(parser, TOKEN_COLON)) {
        /* Output operands */
        if (!CHECK(parser, TOKEN_COLON) && !CHECK(parser, TOKEN_RPAREN)) {
            /* Parse asm output operands */
            c_parse_asm_operands(parser);
        }
        
        if (MATCH(parser, TOKEN_COLON)) {
            /* Input operands */
            if (!CHECK(parser, TOKEN_COLON) && !CHECK(parser, TOKEN_RPAREN)) {
                /* Parse asm input operands */
                c_parse_asm_operands(parser);
            }
            
            if (MATCH(parser, TOKEN_COLON)) {
                /* Clobbers - string literals */
                do {
                    if (CHECK(parser, TOKEN_STRING_LITERAL)) {
                        ADVANCE(parser);
                    }
                    if (!MATCH(parser, TOKEN_COMMA)) break;
                } while (true);
            }
        }
    }
    
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after asm");
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after asm statement");
    
    ASTNode *asm_stmt = ast_create_node(AST_ASM_STMT, loc);
    asm_stmt->data.asm_stmt.asm_string = asm_string;
    asm_stmt->data.asm_stmt.is_volatile = is_volatile;
    
    return asm_stmt;
}

/* ===== EXPRESSIONS (Precedence Climbing) ===== */

ASTNode *c_parse_expression(CParser *parser) {
    /* Comma operator: evaluate left, discard, evaluate right, return right */
    ASTNode *left = c_parse_assignment_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_COMMA)) {
        /* , → LLVM: evaluate both, return second */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_assignment_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_COMMA_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_assignment_expression(CParser *parser) {
    /* Precedence 1: =, +=, -=, etc. (right-associative) */
    ASTNode *left = c_parse_conditional_expression(parser);
    if (!left) return NULL;
    
    SourceLocation loc = CURRENT(parser)->location;
    ASTNodeType node_type;
    
    if (MATCH(parser, TOKEN_EQUAL)) {
        /* = → LLVM: store */
        node_type = AST_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_PLUS_EQUAL)) {
        /* += → LLVM: load, add, store */
        node_type = AST_ADD_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_MINUS_EQUAL)) {
        /* -= → LLVM: load, sub, store */
        node_type = AST_SUB_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_STAR_EQUAL)) {
        /* *= → LLVM: load, mul, store */
        node_type = AST_MUL_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_SLASH_EQUAL)) {
        /* /= → LLVM: load, div, store */
        node_type = AST_DIV_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_PERCENT_EQUAL)) {
        /* %= → LLVM: load, rem, store */
        node_type = AST_MOD_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_AMPERSAND_EQUAL)) {
        /* &= → LLVM: load, and, store */
        node_type = AST_AND_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_PIPE_EQUAL)) {
        /* |= → LLVM: load, or, store */
        node_type = AST_OR_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_CARET_EQUAL)) {
        /* ^= → LLVM: load, xor, store */
        node_type = AST_XOR_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_LESS_LESS_EQUAL)) {
        /* <<= → LLVM: load, shl, store */
        node_type = AST_SHL_ASSIGN_EXPR;
    } else if (MATCH(parser, TOKEN_GREATER_GREATER_EQUAL)) {
        /* >>= → LLVM: load, shr, store */
        node_type = AST_SHR_ASSIGN_EXPR;
    } else {
        /* Not an assignment */
        return left;
    }
    
    /* Right-associative: parse another assignment expression */
    ASTNode *right = c_parse_assignment_expression(parser);
    if (!right) return left;
    
    ASTNode *node = ast_create_node(node_type, loc);
    ast_add_child(node, left);
    ast_add_child(node, right);
    return node;
}

ASTNode *c_parse_conditional_expression(CParser *parser) {
    /* Precedence 2: ?: (right-associative) */
    ASTNode *condition = c_parse_logical_or_expression(parser);
    if (!condition) return NULL;
    
    if (MATCH(parser, TOKEN_QUESTION)) {
        /* ?: → LLVM: select or phi nodes */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *then_expr = c_parse_expression(parser);
        EXPECT(parser, TOKEN_COLON, "expected ':' in conditional expression");
        ASTNode *else_expr = c_parse_conditional_expression(parser);
        
        ASTNode *node = ast_create_node(AST_CONDITIONAL_EXPR, loc);
        ast_add_child(node, condition);
        ast_add_child(node, then_expr);
        ast_add_child(node, else_expr);
        return node;
    }
    
    return condition;
}

ASTNode *c_parse_logical_or_expression(CParser *parser) {
    /* Precedence 3: || (left-associative, short-circuit) */
    ASTNode *left = c_parse_logical_and_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_PIPE_PIPE)) {
        /* || → LLVM: short-circuit with basic blocks */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_logical_and_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_LOGICAL_OR_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_logical_and_expression(CParser *parser) {
    /* Precedence 4: && (left-associative, short-circuit) */
    ASTNode *left = c_parse_inclusive_or_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_AMPERSAND_AMPERSAND)) {
        /* && → LLVM: short-circuit with basic blocks */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_inclusive_or_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_LOGICAL_AND_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_inclusive_or_expression(CParser *parser) {
    /* Precedence 5: | (left-associative) */
    ASTNode *left = c_parse_exclusive_or_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_PIPE)) {
        /* | → LLVM: or */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_exclusive_or_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_OR_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_exclusive_or_expression(CParser *parser) {
    /* Precedence 6: ^ (left-associative) */
    ASTNode *left = c_parse_and_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_CARET)) {
        /* ^ → LLVM: xor */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_and_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_XOR_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_and_expression(CParser *parser) {
    /* Precedence 7: & (left-associative) */
    ASTNode *left = c_parse_equality_expression(parser);
    if (!left) return NULL;
    
    while (MATCH(parser, TOKEN_AMPERSAND)) {
        /* & → LLVM: and */
        SourceLocation loc = CURRENT(parser)->location;
        ASTNode *right = c_parse_equality_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(AST_AND_EXPR, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_equality_expression(CParser *parser) {
    /* Precedence 8: ==, != (left-associative) */
    ASTNode *left = c_parse_relational_expression(parser);
    if (!left) return NULL;
    
    while (true) {
        SourceLocation loc = CURRENT(parser)->location;
        ASTNodeType node_type;
        
        if (MATCH(parser, TOKEN_EQUAL_EQUAL)) {
            /* == → LLVM: icmp eq or fcmp oeq */
            node_type = AST_EQ_EXPR;
        } else if (MATCH(parser, TOKEN_EXCLAIM_EQUAL)) {
            /* != → LLVM: icmp ne or fcmp one */
            node_type = AST_NE_EXPR;
        } else {
            break;
        }
        
        ASTNode *right = c_parse_relational_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(node_type, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_relational_expression(CParser *parser) {
    /* Precedence 9: <, >, <=, >= (left-associative) */
    ASTNode *left = c_parse_shift_expression(parser);
    if (!left) return NULL;
    
    while (true) {
        SourceLocation loc = CURRENT(parser)->location;
        ASTNodeType node_type;
        
        if (MATCH(parser, TOKEN_LESS)) {
            /* < → LLVM: icmp slt/ult or fcmp olt */
            node_type = AST_LT_EXPR;
        } else if (MATCH(parser, TOKEN_GREATER)) {
            /* > → LLVM: icmp sgt/ugt or fcmp ogt */
            node_type = AST_GT_EXPR;
        } else if (MATCH(parser, TOKEN_LESS_EQUAL)) {
            /* <= → LLVM: icmp sle/ule or fcmp ole */
            node_type = AST_LE_EXPR;
        } else if (MATCH(parser, TOKEN_GREATER_EQUAL)) {
            /* >= → LLVM: icmp sge/uge or fcmp oge */
            node_type = AST_GE_EXPR;
        } else {
            break;
        }
        
        ASTNode *right = c_parse_shift_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(node_type, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_shift_expression(CParser *parser) {
    /* Precedence 10: <<, >> (left-associative) */
    ASTNode *left = c_parse_additive_expression(parser);
    if (!left) return NULL;
    
    while (true) {
        SourceLocation loc = CURRENT(parser)->location;
        ASTNodeType node_type;
        
        if (MATCH(parser, TOKEN_LESS_LESS)) {
            /* << → LLVM: shl */
            node_type = AST_SHL_EXPR;
        } else if (MATCH(parser, TOKEN_GREATER_GREATER)) {
            /* >> → LLVM: lshr (unsigned) or ashr (signed) */
            node_type = AST_SHR_EXPR;
        } else {
            break;
        }
        
        ASTNode *right = c_parse_additive_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(node_type, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_additive_expression(CParser *parser) {
    /* Precedence 11: +, - (left-associative) */
    ASTNode *left = c_parse_multiplicative_expression(parser);
    if (!left) return NULL;
    
    while (true) {
        SourceLocation loc = CURRENT(parser)->location;
        ASTNodeType node_type;
        
        if (MATCH(parser, TOKEN_PLUS)) {
            /* + → LLVM: add (int) or fadd (float) or GEP (pointer) */
            node_type = AST_ADD_EXPR;
        } else if (MATCH(parser, TOKEN_MINUS)) {
            /* - → LLVM: sub (int) or fsub (float) or GEP (pointer) */
            node_type = AST_SUB_EXPR;
        } else {
            break;
        }
        
        ASTNode *right = c_parse_multiplicative_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(node_type, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_multiplicative_expression(CParser *parser) {
    /* Precedence 12: *, /, % (left-associative) */
    ASTNode *left = c_parse_cast_expression(parser);
    if (!left) return NULL;
    
    while (true) {
        SourceLocation loc = CURRENT(parser)->location;
        ASTNodeType node_type;
        
        if (MATCH(parser, TOKEN_STAR)) {
            /* * → LLVM: mul (int) or fmul (float) */
            node_type = AST_MUL_EXPR;
        } else if (MATCH(parser, TOKEN_SLASH)) {
            /* / → LLVM: sdiv/udiv (int) or fdiv (float) */
            node_type = AST_DIV_EXPR;
        } else if (MATCH(parser, TOKEN_PERCENT)) {
            /* % → LLVM: srem/urem (int only) */
            node_type = AST_MOD_EXPR;
        } else {
            break;
        }
        
        ASTNode *right = c_parse_cast_expression(parser);
        if (!right) return left;
        
        ASTNode *node = ast_create_node(node_type, loc);
        ast_add_child(node, left);
        ast_add_child(node, right);
        left = node;
    }
    
    return left;
}

ASTNode *c_parse_cast_expression(CParser *parser) {
    /* Check for cast: (type)expr → LLVM: bitcast/trunc/zext/sext/fptrunc/fpext */
    if (CHECK(parser, TOKEN_LPAREN)) {
        /* Could be cast or parenthesized expression - need lookahead */
        /* For now, simple heuristic: if next token looks like a type, it's a cast */
        Token *next = PEEK(parser, 1);
        if (next && (next->type >= TOKEN_KEYWORD_START && next->type <= TOKEN_KEYWORD_END)) {
            /* Likely a cast */
            SourceLocation loc = CURRENT(parser)->location;
            ADVANCE(parser); /* ( */
            
            /* Parse type name - simplified for now */
            ASTNode *type = c_parse_type_specifier(parser);
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after type name");
            
            ASTNode *expr = c_parse_cast_expression(parser);
            return ast_create_cast_expr(type, expr, loc);
        }
    }
    
    /* Not a cast, parse unary */
    return c_parse_unary_expression(parser);
}

ASTNode *c_parse_unary_expression(CParser *parser) {
    Token *token = CURRENT(parser);
    SourceLocation loc = token->location;
    
    /* Pre-increment: ++x → LLVM: load, add 1, store, return new value */
    if (MATCH(parser, TOKEN_PLUS_PLUS)) {
        ASTNode *operand = c_parse_unary_expression(parser);
        ASTNode *node = ast_create_node(AST_PRE_INC_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Pre-decrement: --x → LLVM: load, sub 1, store, return new value */
    if (MATCH(parser, TOKEN_MINUS_MINUS)) {
        ASTNode *operand = c_parse_unary_expression(parser);
        ASTNode *node = ast_create_node(AST_PRE_DEC_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Address-of: &x → LLVM: alloca address (no load) */
    if (MATCH(parser, TOKEN_AMPERSAND)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_ADDR_OF_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Dereference: *p → LLVM: load */
    if (MATCH(parser, TOKEN_STAR)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_DEREF_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Unary plus: +x → LLVM: nop (identity) */
    if (MATCH(parser, TOKEN_PLUS)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_UNARY_PLUS_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Unary minus: -x → LLVM: sub 0, x */
    if (MATCH(parser, TOKEN_MINUS)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_UNARY_MINUS_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Bitwise NOT: ~x → LLVM: xor -1, x */
    if (MATCH(parser, TOKEN_TILDE)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_BIT_NOT_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Logical NOT: !x → LLVM: icmp eq 0, x */
    if (MATCH(parser, TOKEN_EXCLAIM)) {
        ASTNode *operand = c_parse_cast_expression(parser);
        ASTNode *node = ast_create_node(AST_NOT_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* sizeof: sizeof(type) or sizeof expr → LLVM: constant (compile-time) */
    if (MATCH(parser, TOKEN_SIZEOF)) {
        ASTNode *operand;
        if (MATCH(parser, TOKEN_LPAREN)) {
            /* Could be type or expression */
            operand = c_parse_unary_expression(parser);
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after sizeof");
        } else {
            operand = c_parse_unary_expression(parser);
        }
        return ast_create_sizeof_expr(operand, loc);
    }
    
    /* _Alignof: _Alignof(type) → LLVM: constant (compile-time) */
    if (MATCH(parser, TOKEN__ALIGNOF) || MATCH(parser, TOKEN___ALIGNOF__)) {
        EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Alignof");
        ASTNode *operand = c_parse_unary_expression(parser);
        EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Alignof");
        ASTNode *node = ast_create_node(AST_ALIGNOF_EXPR, loc);
        ast_add_child(node, operand);
        return node;
    }
    
    /* Not a unary operator, parse postfix */
    return c_parse_postfix_expression(parser);
}

ASTNode *c_parse_postfix_expression(CParser *parser) {
    ASTNode *expr = c_parse_primary_expression(parser);
    if (!expr) return NULL;
    
    while (true) {
        Token *token = CURRENT(parser);
        SourceLocation loc = token->location;
        
        if (MATCH(parser, TOKEN_LBRACKET)) {
            /* Array subscript: a[i] → LLVM: GEP + load */
            ASTNode *index = c_parse_expression(parser);
            EXPECT(parser, TOKEN_RBRACKET, "expected ']' after array index");
            expr = ast_create_array_subscript(expr, index, loc);
            
        } else if (MATCH(parser, TOKEN_LPAREN)) {
            /* Function call: f(args) → LLVM: call instruction */
            ASTNode **args = NULL;
            size_t arg_count = 0;
            
            if (!CHECK(parser, TOKEN_RPAREN)) {
                /* Parse arguments */
                size_t capacity = 4;
                args = xcalloc(capacity, sizeof(ASTNode*));
                
                do {
                    if (arg_count >= capacity) {
                        capacity *= 2;
                        args = xrealloc(args, capacity * sizeof(ASTNode*));
                    }
                    args[arg_count++] = c_parse_assignment_expression(parser);
                } while (MATCH(parser, TOKEN_COMMA));
            }
            
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after arguments");
            expr = ast_create_call_expr(expr, args, arg_count, loc);
            xfree(args);
            
        } else if (MATCH(parser, TOKEN_DOT)) {
            /* Member access: s.member → LLVM: GEP */
            if (!CHECK(parser, TOKEN_IDENTIFIER)) {
                ERROR(parser, "expected member name after '.'");
                return expr;
            }
            char *member = xstrdup(CURRENT(parser)->lexeme);
            ADVANCE(parser);
            expr = ast_create_member_expr(expr, member, false, loc);
            xfree(member);
            
        } else if (MATCH(parser, TOKEN_ARROW)) {
            /* Arrow: p->member → LLVM: load + GEP */
            if (!CHECK(parser, TOKEN_IDENTIFIER)) {
                ERROR(parser, "expected member name after '->'");
                return expr;
            }
            char *member = xstrdup(CURRENT(parser)->lexeme);
            ADVANCE(parser);
            expr = ast_create_member_expr(expr, member, true, loc);
            xfree(member);
            
        } else if (MATCH(parser, TOKEN_PLUS_PLUS)) {
            /* Post-increment: x++ → LLVM: load, add 1, store, return old value */
            ASTNode *node = ast_create_node(AST_POST_INC_EXPR, loc);
            ast_add_child(node, expr);
            expr = node;
            
        } else if (MATCH(parser, TOKEN_MINUS_MINUS)) {
            /* Post-decrement: x-- → LLVM: load, sub 1, store, return old value */
            ASTNode *node = ast_create_node(AST_POST_DEC_EXPR, loc);
            ast_add_child(node, expr);
            expr = node;
            
        } else {
            break;
        }
    }
    
    return expr;
}

ASTNode *c_parse_primary_expression(CParser *parser) {
    Token *token = CURRENT(parser);
    SourceLocation loc = token->location;
    
    switch (token->type) {
        /* Identifier - maps to LLVM load instruction */
        case TOKEN_IDENTIFIER: {
            const char *name = token->lexeme;
            ADVANCE(parser);
            return ast_create_identifier(name, loc);
        }
        
        /* Integer literal - maps to LLVM constant int */
        case TOKEN_INTEGER_LITERAL: {
            int64_t value = token->value.int_value;
            ADVANCE(parser);
            return ast_create_integer_literal(value, loc);
        }
        
        /* Float literal - maps to LLVM constant float/double */
        case TOKEN_FLOAT_LITERAL: {
            double value = token->value.float_value;
            ADVANCE(parser);
            return ast_create_float_literal(value, loc);
        }
        
        /* String literal - maps to LLVM global constant */
        case TOKEN_STRING_LITERAL: {
            const char *value = token->lexeme;
            ADVANCE(parser);
            return ast_create_string_literal(value, loc);
        }
        
        /* Char literal - maps to LLVM constant i8 */
        case TOKEN_CHAR_LITERAL: {
            char value = token->value.char_value;
            ADVANCE(parser);
            return ast_create_char_literal(value, loc);
        }
        
        /* Parenthesized expression or cast */
        case TOKEN_LPAREN: {
            ADVANCE(parser);
            
            /* Try to disambiguate cast vs parenthesized expression */
            /* If next token looks like a type name, might be a cast */
            /* For now, always parse as expression - casts handled in unary */
            ASTNode *expr = c_parse_expression(parser);
            if (!expr) {
                /* If expression parsing failed, might be at end or error */
                if (CHECK(parser, TOKEN_RPAREN)) {
                    ADVANCE(parser);
                }
                return NULL;
            }
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after expression");
            return expr;
        }
        
        /* C11 _Generic selection */
        case TOKEN__GENERIC:
            if (parser->standard >= C_STD_C11) {
                return c_parse_generic_selection(parser);
            }
            break;
        
        default:
            break;
    }
    
    ERROR(parser, "expected primary expression");
    return NULL;
}

ASTNode *c_parse_argument_expression_list(CParser *parser) {
    /* Parse comma-separated argument list */
    SourceLocation loc = CURRENT(parser)->location;
    ASTNode *list = ast_create_node(AST_COMPOUND_STMT, loc);
    
    do {
        ASTNode *arg = c_parse_assignment_expression(parser);
        if (arg) {
            ast_add_child(list, arg);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    return list;
}

ASTNode *c_parse_constant_expression(CParser *parser) {
    /* Constant expressions are just conditional expressions that must be compile-time evaluable */
    return c_parse_conditional_expression(parser);
}

/* ===== C99/C11/C23 SPECIFIC ===== */

ASTNode *c_parse_generic_selection(CParser *parser) {
    /* C11 _Generic: compile-time type selection */
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* _Generic */
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Generic");
    
    /* Controlling expression */
    ASTNode *expr = c_parse_assignment_expression(parser);
    EXPECT(parser, TOKEN_COMMA, "expected ',' after expression");
    
    ASTNode *node = ast_create_node(AST_GENERIC_EXPR, loc);
    if (expr) ast_add_child(node, expr);
    
    /* Generic associations */
    do {
        if (MATCH(parser, TOKEN_DEFAULT)) {
            EXPECT(parser, TOKEN_COLON, "expected ':' after default");
            ASTNode *value = c_parse_assignment_expression(parser);
            if (value) ast_add_child(node, value);
        } else {
            /* Type name */
            c_parse_type_specifier(parser);
            EXPECT(parser, TOKEN_COLON, "expected ':' after type");
            ASTNode *value = c_parse_assignment_expression(parser);
            if (value) ast_add_child(node, value);
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Generic");
    
    return node;
}

ASTNode *c_parse_static_assert(CParser *parser) {
    /* C11 _Static_assert: compile-time assertion */
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* _Static_assert */
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Static_assert");
    ASTNode *expr = c_parse_constant_expression(parser);
    EXPECT(parser, TOKEN_COMMA, "expected ',' after expression");
    
    /* Message string */
    if (!CHECK(parser, TOKEN_STRING_LITERAL)) {
        ERROR(parser, "expected string literal");
    } else {
        ADVANCE(parser);
    }
    
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Static_assert");
    EXPECT(parser, TOKEN_SEMICOLON, "expected ';' after _Static_assert");
    
    ASTNode *node = ast_create_node(AST_STATIC_ASSERT, loc);
    if (expr) ast_add_child(node, expr);
    
    return node;
}

ASTNode *c_parse_alignas_specifier(CParser *parser) {
    /* C11 _Alignas */
    ADVANCE(parser); /* _Alignas */
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Alignas");
    
    /* Can be type or constant expression */
    c_parse_conditional_expression(parser);
    
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after _Alignas");
    
    return NULL; /* Consumed, no node created */
}

ASTNode *c_parse_atomic_type_specifier(CParser *parser) {
    /* C11 _Atomic(type) */
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* _Atomic */
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after _Atomic");
    ASTNode *type = c_parse_type_specifier(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after type");
    
    return type;
}

/* ===== GNU EXTENSIONS ===== */

ASTNode *c_parse_attribute(CParser *parser) {
    /* GNU __attribute__ */
    ADVANCE(parser); /* __attribute__ */
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after __attribute__");
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after __attribute__(");
    
    /* Parse attribute list */
    do {
        if (CHECK(parser, TOKEN_IDENTIFIER)) {
            ADVANCE(parser);
            
            /* Optional parameters */
            if (MATCH(parser, TOKEN_LPAREN)) {
                /* Skip parameters for now */
                int depth = 1;
                while (depth > 0 && !AT_END(parser)) {
                    if (CHECK(parser, TOKEN_LPAREN)) depth++;
                    if (CHECK(parser, TOKEN_RPAREN)) depth--;
                    ADVANCE(parser);
                }
            }
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after attributes");
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after __attribute__((...)))");
    
    return NULL; /* Consumed, no node created */
}

ASTNode *c_parse_asm_operands(CParser *parser) {
    /* Parse asm operand list */
    /* Format: "constraint" (expression) */
    do {
        if (CHECK(parser, TOKEN_STRING_LITERAL)) {
            ADVANCE(parser); /* constraint */
            EXPECT(parser, TOKEN_LPAREN, "expected '(' after constraint");
            c_parse_assignment_expression(parser);
            EXPECT(parser, TOKEN_RPAREN, "expected ')' after operand");
        }
        
        if (!MATCH(parser, TOKEN_COMMA)) {
            break;
        }
    } while (true);
    
    return NULL;
}

ASTNode *c_parse_typeof(CParser *parser) {
    /* GNU/C23 typeof */
    SourceLocation loc = CURRENT(parser)->location;
    ADVANCE(parser); /* typeof or __typeof__ */
    
    EXPECT(parser, TOKEN_LPAREN, "expected '(' after typeof");
    ASTNode *expr = c_parse_conditional_expression(parser);
    EXPECT(parser, TOKEN_RPAREN, "expected ')' after typeof");
    
    /* Return the type of the expression */
    return expr;
}

/* ===== UTILITIES ===== */

bool c_is_type_specifier(CParser *parser) {
    Token *token = CURRENT(parser);
    switch (token->type) {
        case TOKEN_VOID:
        case TOKEN_CHAR:
        case TOKEN_SHORT:
        case TOKEN_INT:
        case TOKEN_LONG:
        case TOKEN_FLOAT:
        case TOKEN_DOUBLE:
        case TOKEN_SIGNED:
        case TOKEN_UNSIGNED:
        case TOKEN__BOOL:
        case TOKEN__COMPLEX:
        case TOKEN__IMAGINARY:
        case TOKEN_STRUCT:
        case TOKEN_UNION:
        case TOKEN_ENUM:
        case TOKEN___TYPEOF__:
        case TOKEN_TYPEOF:
        case TOKEN__ATOMIC:
            return true;
        case TOKEN_IDENTIFIER:
            /* Check if it's a typedef name */
            return c_is_type_name(parser, token->lexeme);
        default:
            return false;
    }
}

bool c_is_type_qualifier(CParser *parser) {
    Token *token = CURRENT(parser);
    switch (token->type) {
        case TOKEN_CONST:
        case TOKEN_VOLATILE:
        case TOKEN_RESTRICT:
        case TOKEN__ATOMIC:
        case TOKEN___CONST__:
        case TOKEN___VOLATILE__:
        case TOKEN___RESTRICT__:
            return true;
        default:
            return false;
    }
}

bool c_is_storage_class_specifier(CParser *parser) {
    Token *token = CURRENT(parser);
    switch (token->type) {
        case TOKEN_AUTO:
        case TOKEN_REGISTER:
        case TOKEN_STATIC:
        case TOKEN_EXTERN:
        case TOKEN_TYPEDEF:
        case TOKEN__THREAD_LOCAL:
            return true;
        default:
            return false;
    }
}

bool c_is_function_specifier(CParser *parser) {
    Token *token = CURRENT(parser);
    switch (token->type) {
        case TOKEN_INLINE:
        case TOKEN__NORETURN:
        case TOKEN___INLINE__:
            return true;
        default:
            return false;
    }
}

bool c_is_declaration_specifier(CParser *parser) {
    return c_is_storage_class_specifier(parser) ||
           c_is_type_specifier(parser) ||
           c_is_type_qualifier(parser) ||
           c_is_function_specifier(parser);
}

bool c_is_type_name(CParser *parser, const char *name) {
    /* Check if identifier is a typedef name */
    return symbol_table_contains(parser->typedef_names, name);
}

/* ===== SCOPE MANAGEMENT ===== */

void c_parser_enter_scope(CParser *parser) {
    parser->scope_depth++;
    /* Push new scope - symbol table implementation deferred */
    /* Scope tracking works, symbol resolution needs full symbol table */
}

void c_parser_exit_scope(CParser *parser) {
    parser->scope_depth--;
    /* Pop scope - symbol table implementation deferred */
}

void c_parser_add_typedef(CParser *parser, const char *name) {
    /* Add typedef name to symbol table */
    symbol_table_add(parser->typedef_names, name);
}
