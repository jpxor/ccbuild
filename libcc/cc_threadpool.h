/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef _CC_THREADPOOL_H
#define _CC_THREADPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#ifndef CC_THREADPOOL_QUEUE_CAPACITY
#define CC_THREADPOOL_QUEUE_CAPACITY 128
#endif

#ifndef CC_THREADPOOL_MAX_THREADS
#define CC_THREADPOOL_MAX_THREADS 64
#endif

typedef void (*cc_task_func)(void*);

struct cc_task {
    cc_task_func func;
    void* ctx;
};

struct fenced_ctx {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    uint32_t idle_thread_count;
    uint32_t id;
};

struct cc_threadpool {
    size_t nthreads;
    size_t head;
    size_t tail;
    sem_t empty_slots;
    sem_t filled_slots;
    pthread_mutex_t nq_lock;
    pthread_mutex_t dq_lock;
    struct fenced_ctx fence_ctx;
    struct cc_task tasks[CC_THREADPOOL_QUEUE_CAPACITY];
    pthread_t threads[CC_THREADPOOL_MAX_THREADS];
};

// Initialize thread pool in provided memory with specified number of worker threads
int cc_threadpool_init(struct cc_threadpool* pool, size_t num_threads);

// Submit a task to be executed by the thread pool
int cc_threadpool_submit(struct cc_threadpool* pool, void* ctx, cc_task_func func);

// inserts a fence into the task queue and waits for all tasks
// before the fence to complete. Only supports a single fence
// at a time (not thread-safe!)
void cc_threadpool_fenced_wait(struct cc_threadpool* pool);

// Wait for all tasks to complete and cleanup thread pool resources
void cc_threadpool_stop_and_wait(struct cc_threadpool* pool);

#endif // _CC_THREADPOOL_H

#ifdef CC_THREADPOOL_IMPLEMENTATION

#include <assert.h>
#include <stdio.h>
#include <errno.h>

static void enqueue_task(struct cc_threadpool* queue, struct cc_task *t) {
    sem_wait(&queue->empty_slots);

    pthread_mutex_lock(&queue->nq_lock);
    queue->tasks[queue->tail] = *t;
    queue->tail = (queue->tail + 1) % CC_THREADPOOL_QUEUE_CAPACITY;
    pthread_mutex_unlock(&queue->nq_lock);

    sem_post(&queue->filled_slots);
}

static struct cc_task dequeue_task(struct cc_threadpool* queue) {
    sem_wait(&queue->filled_slots);

    pthread_mutex_lock(&queue->dq_lock);
    struct cc_task t = queue->tasks[queue->head];
    queue->head = (queue->head + 1) % CC_THREADPOOL_QUEUE_CAPACITY;
    pthread_mutex_unlock(&queue->dq_lock);

    sem_post(&queue->empty_slots);
    return t;
}

static void* worker_thread(void* arg) {
    struct cc_threadpool* pool = (struct cc_threadpool*)arg;
    for (;;) {
        struct cc_task t = dequeue_task(pool);
        if (t.func == NULL) {
            // exit signal
            break;
        }
        t.func(t.ctx);
    }
    return NULL;
}

int cc_threadpool_init(struct cc_threadpool* pool, size_t nthreads) {
    assert(pool != NULL);

    if (nthreads > CC_THREADPOOL_MAX_THREADS) {
        printf("error: reached max threads: CC_THREADPOOL_MAX_THREADS(%d) > nthreads(%zu)\n", CC_THREADPOOL_MAX_THREADS, nthreads);
        printf("       can rebuild with custom define CC_THREADPOOL_MAX_THREADS\n");
        printf("       to increase limit\n");
        return EINVAL;
    }

    memset(pool, 0, sizeof*pool);

    sem_init(&pool->empty_slots, 0, CC_THREADPOOL_QUEUE_CAPACITY);
    sem_init(&pool->filled_slots, 0, 0);

    pthread_mutex_init(&pool->nq_lock, NULL);
    pthread_mutex_init(&pool->dq_lock, NULL);
    pthread_mutex_init(&pool->fence_ctx.mutex, NULL);
    pthread_cond_init(&pool->fence_ctx.cond, NULL);

    pool->head = 0;
    pool->tail = 0;
    pool->nthreads = nthreads;

    for (size_t i = 0; i < nthreads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, pool);
    }
    return 0;
}

int cc_threadpool_submit(struct cc_threadpool* pool, void* ctx, cc_task_func func) {
    assert(pool != NULL);
    assert(pool->nthreads > 0);

    if (func == NULL) {
        return EINVAL;
    }
    enqueue_task(pool, &(struct cc_task){
        .func = func,
        .ctx = ctx,
    });
    return 0;
}

static void fenced_wait(void* arg) {
    struct cc_threadpool* pool = arg;
    struct fenced_ctx* ctx = &pool->fence_ctx;
    // all threads must block except for the last one,
    // which will signal all threads to continue
    pthread_mutex_lock(&ctx->mutex);
    size_t fence_id = ctx->id;
    ctx->idle_thread_count++;
    if (ctx->idle_thread_count == pool->nthreads) {
        pthread_cond_broadcast(&ctx->cond);
        ctx->id++;
    } else {
        while (fence_id == ctx->id) {
            pthread_cond_wait(&ctx->cond, &ctx->mutex);
        }
    }
    pthread_mutex_unlock(&ctx->mutex);
}

void cc_threadpool_fenced_wait(struct cc_threadpool* pool) {
    assert(pool != NULL);

    pthread_mutex_lock(&pool->fence_ctx.mutex);
    pool->fence_ctx.idle_thread_count = 0;

    // fence id is used in case a new fence is submitted
    // before the previous threadpool threads have been signaled.
    // they wont block on an old fence id
    size_t fence_id = pool->fence_ctx.id;

    // Submit fence tasks for each worker thread, each fence
    // will block a thread from proceeding until all threads
    // are blocked on the fence, then all threads are signaled
    // to continue
    for (size_t i = 0; i < pool->nthreads; i++) {
        enqueue_task(pool, &(struct cc_task){
            .func = fenced_wait,
            .ctx = pool,
        });
    }

    // wait for signal
    while (fence_id == pool->fence_ctx.id) {
        pthread_cond_wait(&pool->fence_ctx.cond, &pool->fence_ctx.mutex);
    }
    pthread_mutex_unlock(&pool->fence_ctx.mutex);
}

void cc_threadpool_stop_and_wait(struct cc_threadpool* pool) {
    assert(pool != NULL);

    // Submit "stop" tasks for each worker thread
    for (size_t i = 0; i < pool->nthreads; i++) {
        enqueue_task(pool, &(struct cc_task){
            .func = NULL,
            .ctx = NULL,
        });
    }

    // wait
    for (size_t i = 0; i < pool->nthreads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // clean up
    sem_destroy(&pool->empty_slots);
    sem_destroy(&pool->filled_slots);
    pthread_mutex_destroy(&pool->nq_lock);
    pthread_mutex_destroy(&pool->dq_lock);
    pool->nthreads = 0;
}

#endif // CC_THREADPOOL_IMPLEMENTATION
