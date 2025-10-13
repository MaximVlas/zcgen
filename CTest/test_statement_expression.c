int main() {
    int result = ({
        int a = 5;
        int b = 10;
        a + b; // last expression is the result
    });
    return result;
}