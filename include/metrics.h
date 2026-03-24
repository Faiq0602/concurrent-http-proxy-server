#ifndef METRICS_H
#define METRICS_H

#include <stddef.h>

typedef struct metrics metrics_t;

typedef struct {
    size_t total_requests;
    size_t active_connections;
    size_t cache_hits;
    size_t cache_misses;
    size_t cache_evictions;
    size_t origin_fetch_failures;
    size_t bytes_in;
    size_t bytes_out;
} metrics_snapshot_t;

metrics_t *metrics_create(void);
void metrics_destroy(metrics_t *metrics);
void metrics_increment_total_requests(metrics_t *metrics);
void metrics_increment_active_connections(metrics_t *metrics);
void metrics_decrement_active_connections(metrics_t *metrics);
void metrics_increment_cache_hits(metrics_t *metrics);
void metrics_increment_cache_misses(metrics_t *metrics);
void metrics_increment_cache_evictions(metrics_t *metrics);
void metrics_increment_origin_fetch_failures(metrics_t *metrics);
void metrics_add_bytes_in(metrics_t *metrics, size_t bytes);
void metrics_add_bytes_out(metrics_t *metrics, size_t bytes);
void metrics_snapshot(const metrics_t *metrics, metrics_snapshot_t *snapshot);
void metrics_log_summary(const metrics_t *metrics);

#endif
