/* Test without standard library - pure computation */

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

int power(int base, int exp) {
    if (exp == 0) {
        return 1;
    }
    return base * power(base, exp - 1);
}

int main(void) {
    int x = add(10, 20);
    int y = multiply(5, 6);
    int z = power(2, 10);
    
    return x + y + z;  /* Should return 30 + 30 + 1024 = 1084 */
}
