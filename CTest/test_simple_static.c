static __uint16_t test_var = 42;

static inline __uint16_t test_func(__uint16_t x) {
    return x + 1;
}

int main() {
    return test_func(test_var);
}