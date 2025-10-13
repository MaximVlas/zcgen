typedef struct {
    void *data;
    int (*parse_fn)(struct Parser *parser);
} GrammarRule;

typedef struct {
    const char *name;
    int (*parse_fn)(void);
} AnotherStruct;