#include "metrics.h"

#include <stdio.h>

static int assert_true(int condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "test failure: %s\n", message);
        return 1;
    }

    return 0;
}

int main(void)
{
    metrics_t *metrics = metrics_create();
    metrics_snapshot_t snapshot;

    if (metrics == NULL) {
        (void)fprintf(stderr, "test failure: metrics_create returned NULL\n");
        return 1;
    }

    metrics_increment_total_requests(metrics);
    metrics_increment_total_requests(metrics);
    metrics_increment_active_connections(metrics);
    metrics_increment_active_connections(metrics);
    metrics_decrement_active_connections(metrics);
    metrics_increment_cache_hits(metrics);
    metrics_increment_cache_misses(metrics);
    metrics_increment_cache_evictions(metrics);
    metrics_increment_origin_fetch_failures(metrics);
    metrics_add_bytes_in(metrics, 128);
    metrics_add_bytes_out(metrics, 512);

    metrics_snapshot(metrics, &snapshot);

    if (assert_true(snapshot.total_requests == 2, "total_requests should accumulate") != 0 ||
        assert_true(snapshot.active_connections == 1, "active_connections should decrement") != 0 ||
        assert_true(snapshot.cache_hits == 1, "cache_hits should accumulate") != 0 ||
        assert_true(snapshot.cache_misses == 1, "cache_misses should accumulate") != 0 ||
        assert_true(snapshot.cache_evictions == 1, "cache_evictions should accumulate") != 0 ||
        assert_true(snapshot.origin_fetch_failures == 1, "origin_fetch_failures should accumulate") != 0 ||
        assert_true(snapshot.bytes_in == 128, "bytes_in should accumulate") != 0 ||
        assert_true(snapshot.bytes_out == 512, "bytes_out should accumulate") != 0) {
        metrics_destroy(metrics);
        return 1;
    }

    metrics_destroy(metrics);
    (void)printf("test_metrics: all tests passed\n");
    return 0;
}
