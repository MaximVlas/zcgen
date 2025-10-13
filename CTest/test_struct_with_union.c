struct Token {
    int type;
    char *lexeme;
    union {
        int int_value;
        double double_value;
        char char_value;
    } value;
    struct Token *next;
};

typedef enum {
    AST_BINARY_EXPR,
    AST_ADD_EXPR
} ASTNodeType;