#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>

typedef struct {
    unsigned char *data;
    size_t size_bytes;
} cache_value_t;

typedef struct {
    int evicted;
} cache_put_result_t;

typedef struct cache cache_t;

cache_t *cache_create(size_t max_total_size_bytes, size_t max_object_size_bytes);
void cache_destroy(cache_t *cache);
int cache_put(cache_t *cache, const char *key, const unsigned char *data, size_t size_bytes,
    cache_put_result_t *result);
int cache_get(cache_t *cache, const char *key, cache_value_t *out_value);
void cache_value_free(cache_value_t *value);
size_t cache_current_size(const cache_t *cache);
size_t cache_entry_count(const cache_t *cache);

#endif
