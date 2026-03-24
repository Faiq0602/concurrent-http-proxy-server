#include "cache.h"

#include <stdio.h>
#include <string.h>

static int assert_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "test failure: %s\n", message);
        return 1;
    }

    return 0;
}

static int test_put_get(void)
{
    static const unsigned char payload[] = "alpha";
    cache_t *cache = cache_create(64, 32);
    cache_value_t value;

    if (cache == NULL) {
        return 1;
    }

    if (assert_true(cache_put(cache, "a", payload, sizeof(payload)) == 0,
            "cache_put should succeed") != 0 ||
        assert_true(cache_get(cache, "a", &value) == 0,
            "cache_get should succeed") != 0 ||
        assert_true(value.size_bytes == sizeof(payload),
            "cached size should match") != 0 ||
        assert_true(memcmp(value.data, payload, sizeof(payload)) == 0,
            "cached bytes should match") != 0) {
        cache_destroy(cache);
        return 1;
    }

    cache_value_free(&value);
    cache_destroy(cache);
    return 0;
}

static int test_lru_eviction(void)
{
    static const unsigned char payload_a[] = "aaaa";
    static const unsigned char payload_b[] = "bbbb";
    static const unsigned char payload_c[] = "cccc";
    cache_t *cache = cache_create(10, 8);
    cache_value_t value;

    if (cache == NULL) {
        return 1;
    }

    if (cache_put(cache, "a", payload_a, 4) != 0 ||
        cache_put(cache, "b", payload_b, 4) != 0 ||
        cache_put(cache, "c", payload_c, 4) != 0) {
        cache_destroy(cache);
        return 1;
    }

    if (assert_true(cache_get(cache, "a", &value) != 0,
            "oldest entry should be evicted") != 0 ||
        assert_true(cache_get(cache, "b", &value) == 0,
            "newer entry should remain") != 0 ||
        assert_true(cache_get(cache, "c", &value) == 0,
            "newest entry should remain") != 0) {
        cache_destroy(cache);
        return 1;
    }

    cache_value_free(&value);
    cache_destroy(cache);
    return 0;
}

static int test_object_too_large(void)
{
    static const unsigned char payload[] = "123456789";
    cache_t *cache = cache_create(16, 4);

    if (cache == NULL) {
        return 1;
    }

    if (assert_true(cache_put(cache, "oversized", payload, 9) != 0,
            "oversized object should be rejected") != 0) {
        cache_destroy(cache);
        return 1;
    }

    cache_destroy(cache);
    return 0;
}

static int test_overwrite_existing_entry(void)
{
    static const unsigned char first[] = "abc";
    static const unsigned char second[] = "updated";
    cache_t *cache = cache_create(32, 16);
    cache_value_t value;

    if (cache == NULL) {
        return 1;
    }

    if (cache_put(cache, "same", first, sizeof(first)) != 0 ||
        cache_put(cache, "same", second, sizeof(second)) != 0 ||
        assert_true(cache_entry_count(cache) == 1, "overwrite should not duplicate entry") != 0 ||
        assert_true(cache_get(cache, "same", &value) == 0, "overwritten entry should be readable") != 0 ||
        assert_true(value.size_bytes == sizeof(second), "overwritten size should match latest bytes") != 0 ||
        assert_true(memcmp(value.data, second, sizeof(second)) == 0,
            "overwritten payload should match latest value") != 0) {
        cache_destroy(cache);
        return 1;
    }

    cache_value_free(&value);
    cache_destroy(cache);
    return 0;
}

int main(void)
{
    if (test_put_get() != 0 ||
        test_lru_eviction() != 0 ||
        test_object_too_large() != 0 ||
        test_overwrite_existing_entry() != 0) {
        return 1;
    }

    (void)printf("test_cache: all tests passed\n");
    return 0;
}
