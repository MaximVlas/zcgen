int main() {
    static void *dispatch_table[] = {
        &&L_OP_MOVE,
        &&L_OP_LOADI,
        &&L_OP_LOADF
    };
    
    L_OP_MOVE:
        return 1;
    L_OP_LOADI:
        return 2;
    L_OP_LOADF:
        return 3;
}