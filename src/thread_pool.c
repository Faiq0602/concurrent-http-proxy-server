#include "thread_pool.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

struct thread_pool {
    client_job_t *jobs;
    int queue_capacity;
    int queue_size;
    int queue_head;
    int queue_tail;
    int worker_count;
    int shutting_down;
    int threads_started;
    int sync_initialized;
    void (*job_handler)(const client_job_t *job, void *context);
    void *context;
#if defined(_WIN32)
    HANDLE *threads;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE queue_not_empty;
    CONDITION_VARIABLE queue_not_full;
#else
    pthread_t *threads;
    pthread_mutex_t mutex;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
#endif
};

static void client_job_reset(client_job_t *job)
{
    if (job != NULL) {
        (void)memset(job, 0, sizeof(*job));
        job->client_socket = SOCKET_HANDLE_INVALID;
    }
}

#if defined(_WIN32)
static unsigned __stdcall thread_pool_worker(void *arg)
#else
static void *thread_pool_worker(void *arg)
#endif
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    for (;;) {
        client_job_t job;

        client_job_reset(&job);

#if defined(_WIN32)
        EnterCriticalSection(&pool->mutex);
        while (pool->queue_size == 0 && !pool->shutting_down) {
            SleepConditionVariableCS(&pool->queue_not_empty, &pool->mutex, INFINITE);
        }

        if (pool->queue_size == 0 && pool->shutting_down) {
            LeaveCriticalSection(&pool->mutex);
            break;
        }

        job = pool->jobs[pool->queue_head];
        client_job_reset(&pool->jobs[pool->queue_head]);
        pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
        --pool->queue_size;
        WakeConditionVariable(&pool->queue_not_full);
        LeaveCriticalSection(&pool->mutex);
#else
        pthread_mutex_lock(&pool->mutex);
        while (pool->queue_size == 0 && !pool->shutting_down) {
            pthread_cond_wait(&pool->queue_not_empty, &pool->mutex);
        }

        if (pool->queue_size == 0 && pool->shutting_down) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        job = pool->jobs[pool->queue_head];
        client_job_reset(&pool->jobs[pool->queue_head]);
        pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
        --pool->queue_size;
        pthread_cond_signal(&pool->queue_not_full);
        pthread_mutex_unlock(&pool->mutex);
#endif

        pool->job_handler(&job, pool->context);
    }

#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

thread_pool_t *thread_pool_create(int worker_count, int queue_capacity,
    void (*job_handler)(const client_job_t *job, void *context), void *context)
{
    thread_pool_t *pool;
    int index;

    if (worker_count <= 0 || queue_capacity <= 0 || job_handler == NULL) {
        return NULL;
    }

    pool = (thread_pool_t *)calloc(1, sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }

    (void)memset(pool, 0, sizeof(*pool));
    pool->queue_capacity = queue_capacity;
    pool->worker_count = worker_count;
    pool->job_handler = job_handler;
    pool->context = context;

    pool->jobs = (client_job_t *)calloc((size_t)queue_capacity, sizeof(client_job_t));
    if (pool->jobs == NULL) {
        thread_pool_destroy(pool);
        return NULL;
    }

    for (index = 0; index < queue_capacity; ++index) {
        pool->jobs[index].client_socket = SOCKET_HANDLE_INVALID;
    }

#if defined(_WIN32)
    pool->threads = (HANDLE *)calloc((size_t)worker_count, sizeof(HANDLE));
    if (pool->threads == NULL) {
        free(pool->jobs);
        free(pool);
        return NULL;
    }

    InitializeCriticalSection(&pool->mutex);
    InitializeConditionVariable(&pool->queue_not_empty);
    InitializeConditionVariable(&pool->queue_not_full);
    pool->sync_initialized = 1;

    for (index = 0; index < worker_count; ++index) {
        uintptr_t thread_handle;

        thread_handle = _beginthreadex(NULL, 0, thread_pool_worker, pool, 0, NULL);
        if (thread_handle == 0) {
            thread_pool_destroy(pool);
            return NULL;
        }

        pool->threads[index] = (HANDLE)thread_handle;
        ++pool->threads_started;
    }
#else
    pool->threads = (pthread_t *)calloc((size_t)worker_count, sizeof(pthread_t));
    if (pool->threads == NULL) {
        free(pool->jobs);
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL);
    pthread_cond_init(&pool->queue_not_full, NULL);
    pool->sync_initialized = 1;

    for (index = 0; index < worker_count; ++index) {
        if (pthread_create(&pool->threads[index], NULL, thread_pool_worker, pool) != 0) {
            thread_pool_destroy(pool);
            return NULL;
        }

        ++pool->threads_started;
    }
#endif

    return pool;
}

int thread_pool_submit(thread_pool_t *pool, const client_job_t *job)
{
    if (pool == NULL || job == NULL || job->client_socket == SOCKET_HANDLE_INVALID) {
        return -1;
    }

#if defined(_WIN32)
    EnterCriticalSection(&pool->mutex);
    while (pool->queue_size == pool->queue_capacity && !pool->shutting_down) {
        SleepConditionVariableCS(&pool->queue_not_full, &pool->mutex, INFINITE);
    }

    if (pool->shutting_down) {
        LeaveCriticalSection(&pool->mutex);
        return -1;
    }

    pool->jobs[pool->queue_tail] = *job;
    pool->queue_tail = (pool->queue_tail + 1) % pool->queue_capacity;
    ++pool->queue_size;
    WakeConditionVariable(&pool->queue_not_empty);
    LeaveCriticalSection(&pool->mutex);
#else
    pthread_mutex_lock(&pool->mutex);
    while (pool->queue_size == pool->queue_capacity && !pool->shutting_down) {
        pthread_cond_wait(&pool->queue_not_full, &pool->mutex);
    }

    if (pool->shutting_down) {
        pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    pool->jobs[pool->queue_tail] = *job;
    pool->queue_tail = (pool->queue_tail + 1) % pool->queue_capacity;
    ++pool->queue_size;
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->mutex);
#endif

    return 0;
}

void thread_pool_shutdown(thread_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    if (!pool->sync_initialized) {
        return;
    }

#if defined(_WIN32)
    EnterCriticalSection(&pool->mutex);
    pool->shutting_down = 1;
    WakeAllConditionVariable(&pool->queue_not_empty);
    WakeAllConditionVariable(&pool->queue_not_full);
    LeaveCriticalSection(&pool->mutex);
#else
    pthread_mutex_lock(&pool->mutex);
    pool->shutting_down = 1;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_cond_broadcast(&pool->queue_not_full);
    pthread_mutex_unlock(&pool->mutex);
#endif
}

void thread_pool_destroy(thread_pool_t *pool)
{
    int index;

    if (pool == NULL) {
        return;
    }

    if (pool->sync_initialized) {
        thread_pool_shutdown(pool);
    }

#if defined(_WIN32)
    if (pool->threads != NULL) {
        for (index = 0; index < pool->threads_started; ++index) {
            if (pool->threads[index] != NULL) {
                (void)WaitForSingleObject(pool->threads[index], INFINITE);
                (void)CloseHandle(pool->threads[index]);
            }
        }
    }

    if (pool->sync_initialized) {
        DeleteCriticalSection(&pool->mutex);
    }
#else
    if (pool->threads != NULL) {
        for (index = 0; index < pool->threads_started; ++index) {
            (void)pthread_join(pool->threads[index], NULL);
        }
    }

    if (pool->sync_initialized) {
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->queue_not_empty);
        pthread_cond_destroy(&pool->queue_not_full);
    }
#endif

    if (pool->jobs != NULL) {
        for (index = 0; index < pool->queue_capacity; ++index) {
            if (pool->jobs[index].client_socket != SOCKET_HANDLE_INVALID) {
                socket_utils_close(pool->jobs[index].client_socket);
            }
        }
    }

    free(pool->jobs);
    free(pool->threads);
    free(pool);
}
