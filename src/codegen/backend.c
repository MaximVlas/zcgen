#include "backend.h"
#include "../common/memory.h"
#include <string.h>

/* Backend registry */
static Backend *registered_backends[16];
static size_t backend_count = 0;

/* ===== REGISTRY ===== */

void backend_register(Backend *backend) {
    if (!backend || backend_count >= 16) return;
    registered_backends[backend_count++] = backend;
}

Backend *backend_get(BackendType type) {
    /* For now, directly create LLVM backend if requested */
    if (type == BACKEND_LLVM) {
        return backend_llvm_create();
    }
    
    /* Check registered backends */
    for (size_t i = 0; i < backend_count; i++) {
        if (registered_backends[i]->type == type) {
            return registered_backends[i];
        }
    }
    
    return NULL;
}

Backend *backend_get_by_name(const char *name) {
    if (!name) return NULL;
    
    /* Check registered backends */
    for (size_t i = 0; i < backend_count; i++) {
        if (strcmp(registered_backends[i]->name, name) == 0) {
            return registered_backends[i];
        }
    }
    
    return NULL;
}

void backend_list_all(Backend ***backends, size_t *count) {
    if (backends) *backends = registered_backends;
    if (count) *count = backend_count;
}

/* ===== SELECTION ===== */

Backend *backend_select_default(void) {
    /* Default to LLVM backend */
    return backend_llvm_create();
}

Backend *backend_select_for_target(const char *target_triple) {
    /* For now, always use LLVM */
    (void)target_triple;
    return backend_llvm_create();
}

/* ===== BUILT-IN BACKENDS ===== */

/* LLVM backend is implemented in llvm_backend_impl.c */
extern Backend *backend_llvm_create(void);

Backend *backend_rust_create(void) {
    /* TODO: Create Rust backend (if available) */
    return NULL;
}

Backend *backend_zig_create(void) {
    /* TODO: Create Zig backend (if available) */
    return NULL;
}

Backend *backend_c_create(void) {
    /* TODO: Create C transpiler backend */
    return NULL;
}
