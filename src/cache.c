#include "cache.h"

#include <stdlib.h>
#include <string.h>

#include "lru.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct cache_entry {
    lru_entry_t lru;
    struct cache_entry *bucket_next;
} cache_entry_t;

struct cache {
    cache_entry_t **buckets;
    size_t bucket_count;
    size_t max_total_size_bytes;
    size_t max_object_size_bytes;
    size_t current_size_bytes;
    lru_list_t lru_list;
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

static unsigned long cache_hash_key(const char *key)
{
    unsigned long hash = 5381;
    int ch;

    while (key != NULL && *key != '\0') {
        ch = (unsigned char)*key++;
        hash = ((hash << 5) + hash) + (unsigned long)ch;
    }

    return hash;
}

static void cache_lock(cache_t *cache)
{
#if defined(_WIN32)
    EnterCriticalSection(&cache->mutex);
#else
    pthread_mutex_lock(&cache->mutex);
#endif
}

static void cache_unlock(cache_t *cache)
{
#if defined(_WIN32)
    LeaveCriticalSection(&cache->mutex);
#else
    pthread_mutex_unlock(&cache->mutex);
#endif
}

static void cache_entry_destroy(cache_entry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    free(entry->lru.key);
    free(entry->lru.data);
    free(entry);
}

static cache_entry_t *cache_find_entry_locked(cache_t *cache, const char *key, size_t *bucket_index)
{
    size_t index;
    cache_entry_t *entry;

    index = (size_t)(cache_hash_key(key) % cache->bucket_count);
    if (bucket_index != NULL) {
        *bucket_index = index;
    }

    entry = cache->buckets[index];
    while (entry != NULL) {
        if (strcmp(entry->lru.key, key) == 0) {
            return entry;
        }

        entry = entry->bucket_next;
    }

    return NULL;
}

static void cache_remove_from_bucket_locked(cache_t *cache, cache_entry_t *entry)
{
    size_t index;
    cache_entry_t *cursor;
    cache_entry_t *previous = NULL;

    index = (size_t)(cache_hash_key(entry->lru.key) % cache->bucket_count);
    cursor = cache->buckets[index];
    while (cursor != NULL) {
        if (cursor == entry) {
            if (previous == NULL) {
                cache->buckets[index] = cursor->bucket_next;
            } else {
                previous->bucket_next = cursor->bucket_next;
            }
            return;
        }

        previous = cursor;
        cursor = cursor->bucket_next;
    }
}

static void cache_evict_if_needed_locked(cache_t *cache, size_t incoming_size)
{
    while (cache->current_size_bytes + incoming_size > cache->max_total_size_bytes &&
           cache->lru_list.tail != NULL) {
        cache_entry_t *evicted = (cache_entry_t *)lru_remove_tail(&cache->lru_list);

        if (evicted == NULL) {
            break;
        }

        cache_remove_from_bucket_locked(cache, evicted);
        if (cache->current_size_bytes >= evicted->lru.size_bytes) {
            cache->current_size_bytes -= evicted->lru.size_bytes;
        } else {
            cache->current_size_bytes = 0;
        }
        cache_entry_destroy(evicted);
    }
}

static int cache_replace_entry_locked(cache_t *cache, cache_entry_t *entry,
    const unsigned char *data, size_t size_bytes)
{
    unsigned char *new_data;
    size_t previous_size;

    previous_size = entry->lru.size_bytes;
    if (size_bytes > previous_size) {
        cache->current_size_bytes -= previous_size;
        lru_detach(&cache->lru_list, &entry->lru);
        cache_evict_if_needed_locked(cache, size_bytes);
        lru_attach_front(&cache->lru_list, &entry->lru);
        if (cache->current_size_bytes + size_bytes > cache->max_total_size_bytes) {
            cache->current_size_bytes += previous_size;
            return -1;
        }
    } else {
        cache->current_size_bytes -= previous_size;
    }

    new_data = (unsigned char *)malloc(size_bytes);
    if (new_data == NULL) {
        cache->current_size_bytes += previous_size;
        return -1;
    }

    (void)memcpy(new_data, data, size_bytes);
    free(entry->lru.data);
    entry->lru.data = new_data;
    entry->lru.size_bytes = size_bytes;
    cache->current_size_bytes += size_bytes;
    lru_move_to_front(&cache->lru_list, &entry->lru);
    return 0;
}

cache_t *cache_create(size_t max_total_size_bytes, size_t max_object_size_bytes)
{
    cache_t *cache;

    if (max_total_size_bytes == 0 || max_object_size_bytes == 0 ||
        max_object_size_bytes > max_total_size_bytes) {
        return NULL;
    }

    cache = (cache_t *)calloc(1, sizeof(*cache));
    if (cache == NULL) {
        return NULL;
    }

    cache->bucket_count = 128;
    cache->buckets = (cache_entry_t **)calloc(cache->bucket_count, sizeof(cache_entry_t *));
    if (cache->buckets == NULL) {
        free(cache);
        return NULL;
    }

    cache->max_total_size_bytes = max_total_size_bytes;
    cache->max_object_size_bytes = max_object_size_bytes;
    lru_init(&cache->lru_list);
#if defined(_WIN32)
    InitializeCriticalSection(&cache->mutex);
#else
    pthread_mutex_init(&cache->mutex, NULL);
#endif
    return cache;
}

void cache_destroy(cache_t *cache)
{
    size_t index;

    if (cache == NULL) {
        return;
    }

    for (index = 0; index < cache->bucket_count; ++index) {
        cache_entry_t *entry = cache->buckets[index];

        while (entry != NULL) {
            cache_entry_t *next = entry->bucket_next;
            cache_entry_destroy(entry);
            entry = next;
        }
    }

#if defined(_WIN32)
    DeleteCriticalSection(&cache->mutex);
#else
    pthread_mutex_destroy(&cache->mutex);
#endif
    free(cache->buckets);
    free(cache);
}

int cache_put(cache_t *cache, const char *key, const unsigned char *data, size_t size_bytes)
{
    cache_entry_t *entry;
    size_t bucket_index;

    if (cache == NULL || key == NULL || data == NULL || size_bytes == 0) {
        return -1;
    }

    if (size_bytes > cache->max_object_size_bytes || size_bytes > cache->max_total_size_bytes) {
        return -1;
    }

    cache_lock(cache);

    entry = cache_find_entry_locked(cache, key, &bucket_index);
    if (entry != NULL) {
        int replace_result = cache_replace_entry_locked(cache, entry, data, size_bytes);

        cache_unlock(cache);
        return replace_result;
    }

    cache_evict_if_needed_locked(cache, size_bytes);
    if (cache->current_size_bytes + size_bytes > cache->max_total_size_bytes) {
        cache_unlock(cache);
        return -1;
    }

    entry = (cache_entry_t *)calloc(1, sizeof(*entry));
    if (entry == NULL) {
        cache_unlock(cache);
        return -1;
    }

    entry->lru.key = (char *)malloc(strlen(key) + 1U);
    entry->lru.data = (unsigned char *)malloc(size_bytes);
    if (entry->lru.key == NULL || entry->lru.data == NULL) {
        cache_entry_destroy(entry);
        cache_unlock(cache);
        return -1;
    }

    (void)strcpy(entry->lru.key, key);
    (void)memcpy(entry->lru.data, data, size_bytes);
    entry->lru.size_bytes = size_bytes;
    entry->bucket_next = cache->buckets[bucket_index];
    cache->buckets[bucket_index] = entry;
    lru_attach_front(&cache->lru_list, &entry->lru);
    cache->current_size_bytes += size_bytes;

    cache_unlock(cache);
    return 0;
}

int cache_get(cache_t *cache, const char *key, cache_value_t *out_value)
{
    cache_entry_t *entry;

    if (cache == NULL || key == NULL || out_value == NULL) {
        return -1;
    }

    out_value->data = NULL;
    out_value->size_bytes = 0;

    cache_lock(cache);
    entry = cache_find_entry_locked(cache, key, NULL);
    if (entry == NULL) {
        cache_unlock(cache);
        return -1;
    }

    out_value->data = (unsigned char *)malloc(entry->lru.size_bytes);
    if (out_value->data == NULL) {
        cache_unlock(cache);
        return -1;
    }

    (void)memcpy(out_value->data, entry->lru.data, entry->lru.size_bytes);
    out_value->size_bytes = entry->lru.size_bytes;
    lru_move_to_front(&cache->lru_list, &entry->lru);
    cache_unlock(cache);
    return 0;
}

void cache_value_free(cache_value_t *value)
{
    if (value == NULL) {
        return;
    }

    free(value->data);
    value->data = NULL;
    value->size_bytes = 0;
}

size_t cache_current_size(const cache_t *cache)
{
    if (cache == NULL) {
        return 0;
    }

    return cache->current_size_bytes;
}

size_t cache_entry_count(const cache_t *cache)
{
    if (cache == NULL) {
        return 0;
    }

    return cache->lru_list.entry_count;
}
