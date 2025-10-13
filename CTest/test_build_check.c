// Simple test to check if our GCC intrinsics work
#include <stdio.h>

__uint16_t test_func(__uint16_t x) {
    return x + 1;
}

int main() {
    printf("Testing GCC intrinsics: %u\n", test_func(42));
    return 0;
}