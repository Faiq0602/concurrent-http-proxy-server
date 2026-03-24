#include "metrics.h"

#include <stdlib.h>

#include "logger.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

struct metrics {
    metrics_snapshot_t counters;
#if defined(_WIN32)
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

static void metrics_lock(metrics_t *metrics)
{
#if defined(_WIN32)
    EnterCriticalSection(&metrics->mutex);
#else
    pthread_mutex_lock(&metrics->mutex);
#endif
}

static void metrics_unlock(metrics_t *metrics)
{
#if defined(_WIN32)
    LeaveCriticalSection(&metrics->mutex);
#else
    pthread_mutex_unlock(&metrics->mutex);
#endif
}

metrics_t *metrics_create(void)
{
    metrics_t *metrics = (metrics_t *)calloc(1, sizeof(*metrics));

    if (metrics == NULL) {
        return NULL;
    }

#if defined(_WIN32)
    InitializeCriticalSection(&metrics->mutex);
#else
    pthread_mutex_init(&metrics->mutex, NULL);
#endif
    return metrics;
}

void metrics_destroy(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }

#if defined(_WIN32)
    DeleteCriticalSection(&metrics->mutex);
#else
    pthread_mutex_destroy(&metrics->mutex);
#endif
    free(metrics);
}

void metrics_increment_total_requests(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.total_requests;
    metrics_unlock(metrics);
}

void metrics_increment_active_connections(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.active_connections;
    metrics_unlock(metrics);
}

void metrics_decrement_active_connections(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    if (metrics->counters.active_connections > 0) {
        --metrics->counters.active_connections;
    }
    metrics_unlock(metrics);
}

void metrics_increment_cache_hits(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.cache_hits;
    metrics_unlock(metrics);
}

void metrics_increment_cache_misses(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.cache_misses;
    metrics_unlock(metrics);
}

void metrics_increment_cache_evictions(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.cache_evictions;
    metrics_unlock(metrics);
}

void metrics_increment_origin_fetch_failures(metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    ++metrics->counters.origin_fetch_failures;
    metrics_unlock(metrics);
}

void metrics_add_bytes_in(metrics_t *metrics, size_t bytes)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    metrics->counters.bytes_in += bytes;
    metrics_unlock(metrics);
}

void metrics_add_bytes_out(metrics_t *metrics, size_t bytes)
{
    if (metrics == NULL) {
        return;
    }
    metrics_lock(metrics);
    metrics->counters.bytes_out += bytes;
    metrics_unlock(metrics);
}

void metrics_snapshot(const metrics_t *metrics, metrics_snapshot_t *snapshot)
{
    metrics_t *mutable_metrics = (metrics_t *)metrics;

    if (metrics == NULL || snapshot == NULL) {
        return;
    }

    metrics_lock(mutable_metrics);
    *snapshot = metrics->counters;
    metrics_unlock(mutable_metrics);
}

void metrics_log_summary(const metrics_t *metrics)
{
    metrics_snapshot_t snapshot;

    if (metrics == NULL) {
        return;
    }

    metrics_snapshot(metrics, &snapshot);
    logger_log(LOG_INFO,
        "metrics summary: total_requests=%lu active_connections=%lu cache_hits=%lu cache_misses=%lu cache_evictions=%lu origin_fetch_failures=%lu bytes_in=%lu bytes_out=%lu",
        (unsigned long)snapshot.total_requests,
        (unsigned long)snapshot.active_connections,
        (unsigned long)snapshot.cache_hits,
        (unsigned long)snapshot.cache_misses,
        (unsigned long)snapshot.cache_evictions,
        (unsigned long)snapshot.origin_fetch_failures,
        (unsigned long)snapshot.bytes_in,
        (unsigned long)snapshot.bytes_out);
}
