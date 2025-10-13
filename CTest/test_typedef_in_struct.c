typedef int* (*ParseRuleFn)(void);

struct Test {
    ParseRuleFn parse_fn;
};