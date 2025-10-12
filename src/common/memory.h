#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdbool.h>

/* Memory allocation wrappers with error checking and safety features */
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
void xfree(void *ptr);

/* Memory debugging and statistics */
void memory_init(void);
void memory_shutdown(void);
void memory_print_stats(void);
void memory_check_leaks(void);
bool memory_enable_guards(bool enable);
bool memory_enable_tracking(bool enable);

/* Memory statistics structure */
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    size_t allocation_count;
    size_t free_count;
    size_t realloc_count;
} MemoryStats;

const MemoryStats *memory_get_stats(void);

#endif /* MEMORY_H */
