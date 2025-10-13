int main() {
    void *label_ptr = &&my_label;
    
    goto *label_ptr;
    
    my_label:
        return 0;
}