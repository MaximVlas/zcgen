#define _POSIX_C_SOURCE 200809L
#include "memory.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ========== Configuration ========== */

/* Guard bytes for overflow/underflow detection */
#define GUARD_SIZE 16
#define GUARD_PATTERN_FRONT 0xDEADBEEF
#define GUARD_PATTERN_BACK  0xBEEFDEAD
#define FREED_PATTERN       0xFEEDFACE

/* Maximum number of tracked allocations */
#define MAX_TRACKED_ALLOCS 100000

/* ========== Internal Structures ========== */

typedef struct AllocationHeader {
    uint32_t front_guard;
    size_t size;
    size_t line;
    const char *file;
    struct AllocationHeader *next;
    struct AllocationHeader *prev;
    bool is_freed;
    uint32_t magic;
} AllocationHeader;

typedef struct {
    uint32_t back_guard;
} AllocationFooter;

#define HEADER_MAGIC 0xABCDEF12

/* ========== Global State ========== */

static struct {
    MemoryStats stats;
    AllocationHeader *alloc_list_head;
    AllocationHeader *alloc_list_tail;
    bool guards_enabled;
    bool tracking_enabled;
    bool initialized;
} g_memory = {
    .stats = {0},
    .alloc_list_head = NULL,
    .alloc_list_tail = NULL,
    .guards_enabled = true,
    .tracking_enabled = true,
    .initialized = false
};

/* ========== Internal Functions ========== */

static void memory_ensure_init(void) {
    if (!g_memory.initialized) {
        memory_init();
    }
}

static void check_guards(AllocationHeader *header, const char *operation) {
    if (!g_memory.guards_enabled) return;
    
    /* Check header magic */
    if (header->magic != HEADER_MAGIC) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "                   MEMORY CORRUPTION DETECTED\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "Operation: %s\n", operation);
        fprintf(stderr, "Error: Invalid allocation header (corrupted or invalid pointer)\n");
        fprintf(stderr, "Address: %p\n", (void*)header);
        fprintf(stderr, "Expected magic: 0x%08X\n", HEADER_MAGIC);
        fprintf(stderr, "Found magic: 0x%08X\n", header->magic);
        fprintf(stderr, "=================================================================\n");
        error_fatal("memory corruption detected");
    }
    
    /* Check front guard */
    if (header->front_guard != GUARD_PATTERN_FRONT) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "                   BUFFER UNDERFLOW DETECTED\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "Operation: %s\n", operation);
        fprintf(stderr, "Allocation size: %zu bytes\n", header->size);
        fprintf(stderr, "Address: %p\n", (void*)(header + 1));
        if (header->file) {
            fprintf(stderr, "Allocated at: %s:%zu\n", header->file, header->line);
        }
        fprintf(stderr, "Expected front guard: 0x%08X\n", GUARD_PATTERN_FRONT);
        fprintf(stderr, "Found: 0x%08X\n", header->front_guard);
        fprintf(stderr, "=================================================================\n");
        error_fatal("buffer underflow detected");
    }
    
    /* Check back guard */
    AllocationFooter *footer = (AllocationFooter*)((char*)(header + 1) + header->size);
    if (footer->back_guard != GUARD_PATTERN_BACK) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "                   BUFFER OVERFLOW DETECTED\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "Operation: %s\n", operation);
        fprintf(stderr, "Allocation size: %zu bytes\n", header->size);
        fprintf(stderr, "Address: %p\n", (void*)(header + 1));
        if (header->file) {
            fprintf(stderr, "Allocated at: %s:%zu\n", header->file, header->line);
        }
        fprintf(stderr, "Expected back guard: 0x%08X\n", GUARD_PATTERN_BACK);
        fprintf(stderr, "Found: 0x%08X\n", footer->back_guard);
        fprintf(stderr, "Overflow detected at offset: %zu\n", header->size);
        fprintf(stderr, "=================================================================\n");
        error_fatal("buffer overflow detected");
    }
    
    /* Check if already freed */
    if (header->is_freed) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "                   DOUBLE FREE DETECTED\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "Operation: %s\n", operation);
        fprintf(stderr, "Address: %p\n", (void*)(header + 1));
        if (header->file) {
            fprintf(stderr, "Originally allocated at: %s:%zu\n", header->file, header->line);
        }
        fprintf(stderr, "This memory was already freed!\n");
        fprintf(stderr, "=================================================================\n");
        error_fatal("double free detected");
    }
}

static void track_allocation(AllocationHeader *header) {
    if (!g_memory.tracking_enabled) return;
    
    /* Add to linked list */
    header->next = NULL;
    header->prev = g_memory.alloc_list_tail;
    
    if (g_memory.alloc_list_tail) {
        g_memory.alloc_list_tail->next = header;
    } else {
        g_memory.alloc_list_head = header;
    }
    g_memory.alloc_list_tail = header;
}

static void untrack_allocation(AllocationHeader *header) {
    if (!g_memory.tracking_enabled) return;
    
    /* Remove from linked list */
    if (header->prev) {
        header->prev->next = header->next;
    } else {
        g_memory.alloc_list_head = header->next;
    }
    
    if (header->next) {
        header->next->prev = header->prev;
    } else {
        g_memory.alloc_list_tail = header->prev;
    }
}

static void *allocate_with_guards(size_t size) {
    memory_ensure_init();
    
    size_t total_size = sizeof(AllocationHeader) + size + sizeof(AllocationFooter);
    AllocationHeader *header = (AllocationHeader*)malloc(total_size);
    
    if (!header) {
        return NULL;
    }
    
    /* Initialize header */
    header->magic = HEADER_MAGIC;
    header->front_guard = GUARD_PATTERN_FRONT;
    header->size = size;
    header->file = NULL;
    header->line = 0;
    header->is_freed = false;
    header->next = NULL;
    header->prev = NULL;
    
    /* Initialize footer */
    AllocationFooter *footer = (AllocationFooter*)((char*)(header + 1) + size);
    footer->back_guard = GUARD_PATTERN_BACK;
    
    /* Track allocation */
    track_allocation(header);
    
    /* Update statistics */
    g_memory.stats.total_allocated += size;
    g_memory.stats.current_usage += size;
    g_memory.stats.allocation_count++;
    
    if (g_memory.stats.current_usage > g_memory.stats.peak_usage) {
        g_memory.stats.peak_usage = g_memory.stats.current_usage;
    }
    
    return (void*)(header + 1);
}

static void free_with_guards(void *ptr) {
    if (!ptr) return;
    
    memory_ensure_init();
    
    AllocationHeader *header = ((AllocationHeader*)ptr) - 1;
    
    /* Check for corruption */
    check_guards(header, "free");
    
    /* Mark as freed */
    header->is_freed = true;
    
    /* Fill with freed pattern to detect use-after-free */
    memset(ptr, 0xFE, header->size);
    
    /* Update statistics */
    g_memory.stats.total_freed += header->size;
    g_memory.stats.current_usage -= header->size;
    g_memory.stats.free_count++;
    
    /* Untrack */
    untrack_allocation(header);
    
    /* Free the memory */
    free(header);
}

/* ========== Public API ========== */

void memory_init(void) {
    if (g_memory.initialized) return;
    
    memset(&g_memory.stats, 0, sizeof(MemoryStats));
    g_memory.alloc_list_head = NULL;
    g_memory.alloc_list_tail = NULL;
    g_memory.guards_enabled = true;
    g_memory.tracking_enabled = true;
    g_memory.initialized = true;
}

void memory_shutdown(void) {
    if (!g_memory.initialized) return;
    
    memory_check_leaks();
    g_memory.initialized = false;
}

void *xmalloc(size_t size) {
    void *ptr = allocate_with_guards(size);
    if (!ptr && size != 0) {
        error_fatal("out of memory");
    }
    return ptr;
}

void *xcalloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = allocate_with_guards(total);
    if (!ptr && total != 0) {
        error_fatal("out of memory");
    }
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return xmalloc(size);
    }
    
    if (size == 0) {
        xfree(ptr);
        return NULL;
    }
    
    memory_ensure_init();
    
    AllocationHeader *old_header = ((AllocationHeader*)ptr) - 1;
    check_guards(old_header, "realloc");
    
    /* Allocate new block */
    void *new_ptr = allocate_with_guards(size);
    if (!new_ptr) {
        error_fatal("out of memory");
    }
    
    /* Copy old data */
    size_t copy_size = old_header->size < size ? old_header->size : size;
    memcpy(new_ptr, ptr, copy_size);
    
    /* Free old block */
    free_with_guards(ptr);
    
    g_memory.stats.realloc_count++;
    
    return new_ptr;
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *dup = (char*)xmalloc(len);
    memcpy(dup, s, len);
    return dup;
}

char *xstrndup(const char *s, size_t n) {
    if (!s) return NULL;
    
    size_t len = strnlen(s, n);
    char *dup = (char*)xmalloc(len + 1);
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

void xfree(void *ptr) {
    free_with_guards(ptr);
}

void memory_print_stats(void) {
    fprintf(stderr, "\n");
    fprintf(stderr, "=================================================================\n");
    fprintf(stderr, "                    MEMORY STATISTICS\n");
    fprintf(stderr, "=================================================================\n");
    fprintf(stderr, "Total allocated:     %zu bytes\n", g_memory.stats.total_allocated);
    fprintf(stderr, "Total freed:         %zu bytes\n", g_memory.stats.total_freed);
    fprintf(stderr, "Current usage:       %zu bytes\n", g_memory.stats.current_usage);
    fprintf(stderr, "Peak usage:          %zu bytes (%.2f MB)\n", 
            g_memory.stats.peak_usage, g_memory.stats.peak_usage / 1024.0 / 1024.0);
    fprintf(stderr, "Allocations:         %zu\n", g_memory.stats.allocation_count);
    fprintf(stderr, "Frees:               %zu\n", g_memory.stats.free_count);
    fprintf(stderr, "Reallocs:            %zu\n", g_memory.stats.realloc_count);
    fprintf(stderr, "=================================================================\n");
}

void memory_check_leaks(void) {
    if (!g_memory.tracking_enabled) return;
    
    size_t leak_count = 0;
    size_t leak_bytes = 0;
    
    AllocationHeader *current = g_memory.alloc_list_head;
    while (current) {
        if (!current->is_freed) {
            leak_count++;
            leak_bytes += current->size;
        }
        current = current->next;
    }
    
    if (leak_count > 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "                    MEMORY LEAKS DETECTED\n");
        fprintf(stderr, "=================================================================\n");
        fprintf(stderr, "Leaked allocations: %zu\n", leak_count);
        fprintf(stderr, "Leaked bytes:       %zu\n", leak_bytes);
        fprintf(stderr, "=================================================================\n");
    }
}

bool memory_enable_guards(bool enable) {
    bool old = g_memory.guards_enabled;
    g_memory.guards_enabled = enable;
    return old;
}

bool memory_enable_tracking(bool enable) {
    bool old = g_memory.tracking_enabled;
    g_memory.tracking_enabled = enable;
    return old;
}

const MemoryStats *memory_get_stats(void) {
    return &g_memory.stats;
}
