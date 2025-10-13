/* Operator test */
#include <stdio.h>

int test_arithmetic(int a, int b) {
    int sum = a + b;
    int diff = a - b;
    int prod = a * b;
    int quot = a / b;
    int rem = a % b;
    
    printf("Arithmetic: %d + %d = %d\n", a, b, sum);
    printf("Arithmetic: %d - %d = %d\n", a, b, diff);
    printf("Arithmetic: %d * %d = %d\n", a, b, prod);
    printf("Arithmetic: %d / %d = %d\n", a, b, quot);
    printf("Arithmetic: %d %% %d = %d\n", a, b, rem);
    
    return sum;
}

int test_comparison(int a, int b) {
    int eq = a == b;
    int ne = a != b;
    int lt = a < b;
    int le = a <= b;
    int gt = a > b;
    int ge = a >= b;
    
    printf("Comparison: %d == %d: %d\n", a, b, eq);
    printf("Comparison: %d != %d: %d\n", a, b, ne);
    printf("Comparison: %d < %d: %d\n", a, b, lt);
    printf("Comparison: %d <= %d: %d\n", a, b, le);
    printf("Comparison: %d > %d: %d\n", a, b, gt);
    printf("Comparison: %d >= %d: %d\n", a, b, ge);
    
    return eq;
}

int test_bitwise(int a, int b) {
    int and_result = a & b;
    int or_result = a | b;
    int xor_result = a ^ b;
    int shl_result = a << 2;
    int shr_result = a >> 2;
    
    printf("Bitwise: %d & %d = %d\n", a, b, and_result);
    printf("Bitwise: %d | %d = %d\n", a, b, or_result);
    printf("Bitwise: %d ^ %d = %d\n", a, b, xor_result);
    printf("Bitwise: %d << 2 = %d\n", a, shl_result);
    printf("Bitwise: %d >> 2 = %d\n", a, shr_result);
    
    return and_result;
}

int main(void) {
    printf("=== Arithmetic Operators ===\n");
    test_arithmetic(100, 7);
    
    printf("\n=== Comparison Operators ===\n");
    test_comparison(10, 20);
    test_comparison(20, 10);
    test_comparison(15, 15);
    
    printf("\n=== Bitwise Operators ===\n");
    test_bitwise(12, 10);
    
    return 0;
}
