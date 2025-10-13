// Test GCC intrinsics
__uint16_t __bswap_16(__uint16_t __bsx) {
    return __bsx;
}

static inline __uint32_t __bswap_32(__uint32_t __bsx) {
    return __bsx;
}

// Test inline assembly
void test_asm() {
    int x = 42;
    asm volatile ("nop" : : "r" (x) : "memory");
    __asm__ ("movl %0, %%eax" : : "r" (x));
}

// Test 128-bit types
__int128 big_int = 0;
__uint128_t big_uint = 0;

int main() {
    return 0;
}