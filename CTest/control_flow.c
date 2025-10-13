/* Control flow test */
#include <stdio.h>

int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}

int min(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

int abs_value(int x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

int sign(int x) {
    if (x > 0) {
        return 1;
    } else if (x < 0) {
        return -1;
    } else {
        return 0;
    }
}

int main(void) {
    printf("Max of 10 and 20: %d\n", max(10, 20));
    printf("Min of 10 and 20: %d\n", min(10, 20));
    printf("Absolute value of -15: %d\n", abs_value(-15));
    printf("Sign of 42: %d\n", sign(42));
    printf("Sign of -42: %d\n", sign(-42));
    printf("Sign of 0: %d\n", sign(0));
    
    return 0;
}
