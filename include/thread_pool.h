#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "socket_utils.h"

typedef struct {
    socket_handle_t client_socket;
    char client_host[NI_MAXHOST];
    char client_service[NI_MAXSERV];
} client_job_t;

typedef struct thread_pool thread_pool_t;

thread_pool_t *thread_pool_create(int worker_count, int queue_capacity,
    void (*job_handler)(const client_job_t *job, void *context), void *context);
int thread_pool_submit(thread_pool_t *pool, const client_job_t *job);
void thread_pool_shutdown(thread_pool_t *pool);
void thread_pool_destroy(thread_pool_t *pool);

#endif
