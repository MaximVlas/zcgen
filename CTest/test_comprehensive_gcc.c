// Comprehensive GCC extensions test

// GCC intrinsic types
__uint8_t u8_var = 255;
__uint16_t u16_var = 65535;
__uint32_t u32_var = 4294967295U;
__uint64_t u64_var = 18446744073709551615ULL;
__int8_t i8_var = -128;
__int16_t i16_var = -32768;
__int32_t i32_var = -2147483648;
__int64_t i64_var = -9223372036854775808LL;
__int128 i128_var = 0;
__uint128_t u128_var = 0;

// GCC intrinsic functions
static inline __uint16_t __bswap_16(__uint16_t __bsx) {
    return ((__bsx >> 8) & 0xff) | ((__bsx & 0xff) << 8);
}

static inline __uint32_t __bswap_32(__uint32_t __bsx) {
    return __builtin_bswap32(__bsx);
}

// Inline assembly
void test_inline_asm() {
    int input = 42;
    int output;
    
    // Basic inline assembly
    asm ("movl %1, %0" : "=r" (output) : "r" (input));
    
    // Volatile inline assembly
    asm volatile ("nop" : : : "memory");
    
    // GCC style
    __asm__ ("nop");
    __asm__ volatile ("pause" : : : "memory");
}

// Function using GCC types
__uint64_t process_data(__uint32_t input) {
    __uint64_t result = input;
    result = __bswap_32(result);
    return result;
}

int main() {
    test_inline_asm();
    return process_data(0x12345678);
}