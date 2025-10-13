/* Math operations test */
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int multiply(int x, int y) {
    return x * y;
}

int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(void) {
    int a = 10;
    int b = 20;
    
    printf("Addition: %d + %d = %d\n", a, b, add(a, b));
    printf("Multiplication: %d * %d = %d\n", a, b, multiply(a, b));
    printf("Factorial of 5: %d\n", factorial(5));
    printf("Fibonacci of 10: %d\n", fibonacci(10));
    
    return 0;
}
