#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct Token Token;
typedef struct TokenList TokenList;
typedef struct ASTNode ASTNode;
typedef struct SourceLocation SourceLocation;
typedef struct SyntaxDefinition SyntaxDefinition;

/* Source location tracking */
struct SourceLocation {
    const char *filename;
    uint32_t line;
    uint32_t column;
    uint32_t offset;
};

/* Token types - generic, mapped by syntax definitions */
typedef enum {
    /* Special tokens */
    TOKEN_EOF = 0,
    TOKEN_ERROR,
    TOKEN_UNKNOWN,
    
    /* Identifiers and literals */
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    
    /* Keywords - language specific, starts at 100 */
    TOKEN_KEYWORD_START = 100,
    TOKEN_KEYWORD_END = 299,
    
    /* Operators - language specific, starts at 300 */
    TOKEN_OPERATOR_START = 300,
    TOKEN_OPERATOR_END = 499,
    
    /* Punctuation - language specific, starts at 500 */
    TOKEN_PUNCTUATION_START = 500,
    TOKEN_PUNCTUATION_END = 699,
    
    /* Comments and whitespace */
    TOKEN_COMMENT,
    TOKEN_WHITESPACE,
    TOKEN_NEWLINE,
    
    TOKEN_MAX = 1000
} TokenType;

/* Token structure */
struct Token {
    TokenType type;
    char *lexeme;           /* Actual text */
    size_t length;
    SourceLocation location;
    
    /* Token value (for literals) */
    union {
        int64_t int_value;
        double float_value;
        char *string_value;
        char char_value;
    } value;
    
    Token *next;            /* For linked list */
};

/* Token list */
struct TokenList {
    Token *head;
    Token *tail;
    size_t count;
};

/* AST Node types - comprehensive and LLVM IR friendly */
typedef enum {
    /* ===== TOP LEVEL ===== */
    AST_TRANSLATION_UNIT,
    AST_MODULE,
    
    /* ===== DECLARATIONS ===== */
    /* Function declarations */
    AST_FUNCTION_DECL,
    AST_FUNCTION_PROTO,
    AST_PARAM_DECL,
    AST_PARAM_LIST,
    
    /* Variable declarations */
    AST_VAR_DECL,
    AST_GLOBAL_VAR_DECL,
    AST_LOCAL_VAR_DECL,
    AST_STATIC_VAR_DECL,
    AST_EXTERN_VAR_DECL,
    
    /* Type declarations */
    AST_TYPEDEF_DECL,
    AST_STRUCT_DECL,
    AST_UNION_DECL,
    AST_ENUM_DECL,
    AST_ENUM_CONSTANT,
    
    /* Field declarations */
    AST_FIELD_DECL,
    AST_BITFIELD_DECL,
    
    /* ===== STATEMENTS ===== */
    AST_COMPOUND_STMT,
    AST_EXPR_STMT,
    AST_DECL_STMT,
    AST_NULL_STMT,
    
    /* Control flow */
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_DO_WHILE_STMT,
    AST_FOR_STMT,
    AST_SWITCH_STMT,
    AST_CASE_STMT,
    AST_DEFAULT_STMT,
    
    /* Jumps */
    AST_RETURN_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_GOTO_STMT,
    AST_LABEL_STMT,
    
    /* Inline assembly */
    AST_ASM_STMT,
    AST_ASM_OPERAND,
    
    /* ===== EXPRESSIONS ===== */
    /* Binary operators */
    AST_BINARY_EXPR,
    AST_ADD_EXPR,
    AST_SUB_EXPR,
    AST_MUL_EXPR,
    AST_DIV_EXPR,
    AST_MOD_EXPR,
    AST_AND_EXPR,           /* Bitwise AND */
    AST_OR_EXPR,            /* Bitwise OR */
    AST_XOR_EXPR,           /* Bitwise XOR */
    AST_SHL_EXPR,           /* Left shift */
    AST_SHR_EXPR,           /* Right shift */
    AST_LOGICAL_AND_EXPR,   /* Logical AND */
    AST_LOGICAL_OR_EXPR,    /* Logical OR */
    
    /* Comparison operators */
    AST_EQ_EXPR,            /* == */
    AST_NE_EXPR,            /* != */
    AST_LT_EXPR,            /* < */
    AST_LE_EXPR,            /* <= */
    AST_GT_EXPR,            /* > */
    AST_GE_EXPR,            /* >= */
    
    /* Assignment operators */
    AST_ASSIGN_EXPR,
    AST_ADD_ASSIGN_EXPR,
    AST_SUB_ASSIGN_EXPR,
    AST_MUL_ASSIGN_EXPR,
    AST_DIV_ASSIGN_EXPR,
    AST_MOD_ASSIGN_EXPR,
    AST_AND_ASSIGN_EXPR,
    AST_OR_ASSIGN_EXPR,
    AST_XOR_ASSIGN_EXPR,
    AST_SHL_ASSIGN_EXPR,
    AST_SHR_ASSIGN_EXPR,
    
    /* Unary operators */
    AST_UNARY_EXPR,
    AST_UNARY_PLUS_EXPR,
    AST_UNARY_MINUS_EXPR,
    AST_NOT_EXPR,           /* Logical NOT */
    AST_BIT_NOT_EXPR,       /* Bitwise NOT */
    AST_DEREF_EXPR,         /* * dereference */
    AST_ADDR_OF_EXPR,       /* & address-of */
    AST_PRE_INC_EXPR,       /* ++x */
    AST_PRE_DEC_EXPR,       /* --x */
    AST_POST_INC_EXPR,      /* x++ */
    AST_POST_DEC_EXPR,      /* x-- */
    
    /* Other expressions */
    AST_CALL_EXPR,
    AST_CAST_EXPR,
    AST_IMPLICIT_CAST_EXPR,
    AST_MEMBER_EXPR,
    AST_ARROW_EXPR,
    AST_ARRAY_SUBSCRIPT_EXPR,
    AST_CONDITIONAL_EXPR,   /* ternary ? : */
    AST_COMMA_EXPR,
    AST_SIZEOF_EXPR,
    AST_ALIGNOF_EXPR,
    AST_OFFSETOF_EXPR,
    AST_VA_ARG_EXPR,
    AST_COMPOUND_LITERAL_EXPR,
    AST_INIT_LIST_EXPR,
    AST_DESIGNATED_INIT_EXPR,
    AST_GENERIC_EXPR,           /* C11 _Generic */
    AST_STATIC_ASSERT,          /* C11 _Static_assert */
    
    /* ===== LITERALS ===== */
    AST_INTEGER_LITERAL,
    AST_FLOAT_LITERAL,
    AST_DOUBLE_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_BOOL_LITERAL,
    AST_NULL_LITERAL,
    AST_IDENTIFIER,
    
    /* ===== TYPES ===== */
    /* Basic types */
    AST_TYPE,
    AST_BUILTIN_TYPE,
    AST_VOID_TYPE,
    AST_BOOL_TYPE,
    AST_CHAR_TYPE,
    AST_SHORT_TYPE,
    AST_INT_TYPE,
    AST_LONG_TYPE,
    AST_LONG_LONG_TYPE,
    AST_FLOAT_TYPE,
    AST_DOUBLE_TYPE,
    AST_LONG_DOUBLE_TYPE,
    
    /* Derived types */
    AST_POINTER_TYPE,
    AST_ARRAY_TYPE,
    AST_FUNCTION_TYPE,
    AST_STRUCT_TYPE,
    AST_UNION_TYPE,
    AST_ENUM_TYPE,
    AST_TYPEDEF_TYPE,
    
    /* Type qualifiers */
    AST_CONST_TYPE,
    AST_VOLATILE_TYPE,
    AST_RESTRICT_TYPE,
    AST_ATOMIC_TYPE,
    
    /* ===== LLVM IR SPECIFIC ===== */
    /* These map directly to LLVM IR concepts */
    AST_ALLOCA,             /* Stack allocation */
    AST_LOAD,               /* Memory load */
    AST_STORE,              /* Memory store */
    AST_GEP,                /* GetElementPtr */
    AST_PHI,                /* PHI node */
    AST_SELECT,             /* Select instruction */
    AST_ICMP,               /* Integer comparison */
    AST_FCMP,               /* Float comparison */
    AST_ZEXT,               /* Zero extend */
    AST_SEXT,               /* Sign extend */
    AST_TRUNC,              /* Truncate */
    AST_FPEXT,              /* Float extend */
    AST_FPTRUNC,            /* Float truncate */
    AST_FPTOUI,             /* Float to unsigned int */
    AST_FPTOSI,             /* Float to signed int */
    AST_UITOFP,             /* Unsigned int to float */
    AST_SITOFP,             /* Signed int to float */
    AST_PTRTOINT,           /* Pointer to int */
    AST_INTTOPTR,           /* Int to pointer */
    AST_BITCAST,            /* Bitcast */
    
    /* Basic blocks and control flow */
    AST_BASIC_BLOCK,
    AST_BR,                 /* Branch */
    AST_COND_BR,            /* Conditional branch */
    AST_UNREACHABLE,
    
    /* Aggregate operations */
    AST_EXTRACT_VALUE,
    AST_INSERT_VALUE,
    AST_EXTRACT_ELEMENT,
    AST_INSERT_ELEMENT,
    
    /* Memory operations */
    AST_MEMCPY,
    AST_MEMMOVE,
    AST_MEMSET,
    
    /* Intrinsics */
    AST_INTRINSIC_CALL,
    
    /* Attributes */
    AST_ATTRIBUTE,
    AST_ATTRIBUTE_LIST,
    
    AST_MAX
} ASTNodeType;

/* AST Node structure */
struct ASTNode {
    ASTNodeType type;
    SourceLocation location;
    
    /* Children nodes */
    ASTNode **children;
    size_t child_count;
    size_t child_capacity;
    
    /* Destruction flag to prevent double-free */
    bool destroyed;
    
    /* Node-specific data */
    union {
        struct {
            char *name;
            ASTNode *type;
            ASTNode *init;
        } var_decl;
        
        struct {
            char *name;
            ASTNode *return_type;
            ASTNode **params;
            size_t param_count;
            ASTNode *body;
        } func_decl;
        
        struct {
            ASTNode *condition;
            ASTNode *then_branch;
            ASTNode *else_branch;
        } if_stmt;
        
        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_stmt;
        
        struct {
            ASTNode *init;
            ASTNode *condition;
            ASTNode *increment;
            ASTNode *body;
        } for_stmt;
        
        struct {
            char *op;
            ASTNode *left;
            ASTNode *right;
        } binary_expr;
        
        struct {
            char *op;
            ASTNode *operand;
        } unary_expr;
        
        struct {
            ASTNode *callee;
            ASTNode **args;
            size_t arg_count;
        } call_expr;
        
        struct {
            char *name;
        } identifier;
        
        struct {
            char *asm_string;       /* Assembly code */
            ASTNode **outputs;      /* Output operands */
            size_t output_count;
            ASTNode **inputs;       /* Input operands */
            size_t input_count;
            char **clobbers;        /* Clobbered registers */
            size_t clobber_count;
            bool is_volatile;
            bool is_goto;
        } asm_stmt;
        
        struct {
            int64_t value;
        } int_literal;
        
        struct {
            double value;
        } float_literal;
        
        struct {
            char *value;
        } string_literal;
        
        struct {
            char *name;
            int size;
            bool is_signed;
            bool is_const;
            bool is_volatile;
        } type;
    } data;
};

#endif /* TYPES_H */
