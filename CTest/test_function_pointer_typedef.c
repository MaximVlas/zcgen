struct Parser;

typedef int* (*ParseRuleFn)(struct Parser *parser);

typedef struct {
    const char *name;
    ParseRuleFn parse_fn;
} GrammarRule;