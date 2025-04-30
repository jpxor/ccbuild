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

struct cc_threadpool {
    size_t nthreads;
    size_t head;
    size_t tail;
    sem_t empty_slots;
    sem_t filled_slots;
    pthread_mutex_t nq_lock;
    pthread_mutex_t dq_lock;
    struct cc_task tasks[CC_THREADPOOL_QUEUE_CAPACITY];
    pthread_t threads[CC_THREADPOOL_MAX_THREADS];
};

// Initialize thread pool in provided memory with specified number of worker threads
int cc_threadpool_init(struct cc_threadpool* pool, size_t num_threads);

// Submit a task to be executed by the thread pool
int cc_threadpool_submit(struct cc_threadpool* pool, void* ctx, cc_task_func func);

// inserts a fence into the task queue and waits for all tasks
// before the fence to complete
void cc_threadpool_fenced_wait(struct cc_threadpool* pool);

// Wait for all tasks to complete and cleanup thread pool resources
void cc_threadpool_stop_and_wait(struct cc_threadpool* pool);

#endif // _CC_THREADPOOL_H

#ifdef CC_THREADPOOL_IMPLEMENTATION

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

static void enqueue_task(struct cc_threadpool* queue, struct cc_task *t) {
    sem_wait(&queue->empty_slots);

    pthread_mutex_lock(&queue->nq_lock);
    queue->tasks[queue->tail] = *t;
    queue->tail = (queue->tail + 1) % CC_THREADPOOL_QUEUE_CAPACITY;
    pthread_mutex_unlock(&queue->nq_lock);

    sem_post(&queue->filled_slots);
}

static void enqueue_fence_tasks(struct cc_threadpool* queue, struct cc_task *t) {
    // all fence tasks must be grouped together, no other tasks interleaved
    // so claim enough space for all fence tasks
    for (size_t i = 0; i < queue->nthreads; i++) {
        sem_wait(&queue->empty_slots);
    }
    // then add all with same lock
    pthread_mutex_lock(&queue->nq_lock);

    for (size_t i = 0; i < queue->nthreads; i++) {
        queue->tasks[queue->tail] = *t;
        queue->tail = (queue->tail + 1) % CC_THREADPOOL_QUEUE_CAPACITY;
    }
    pthread_mutex_unlock(&queue->nq_lock);

    for (size_t i = 0; i < queue->nthreads; i++) {
        sem_post(&queue->filled_slots);
    }
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

struct fence_ctx {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    size_t active_threads;
    sem_t fences_cleared;
};

static void fenced_wait(void* arg) {
    struct fence_ctx* ctx = arg;
    // all threads must block except for the last one,
    // which will signal all pool threads to continue
    pthread_mutex_lock(&ctx->mutex);
    ctx->active_threads--;

    if (ctx->active_threads == 0) {
        pthread_cond_broadcast(&ctx->cond);

    } else {
        while (ctx->active_threads > 0) {
            pthread_cond_wait(&ctx->cond, &ctx->mutex);
        }
    }
    pthread_mutex_unlock(&ctx->mutex);

    // must be done with ctx after clearing fence,
    // the main thread will destroy and free it
    sem_post(&ctx->fences_cleared);
}

void cc_threadpool_fenced_wait(struct cc_threadpool* pool) {
    assert(pool != NULL);

    // each fence added needs its own context
    struct fence_ctx *ctx = calloc(1, sizeof(*ctx));

    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->cond, NULL);

    sem_init(&ctx->fences_cleared, 0, 0);
    ctx->active_threads = pool->nthreads;

    // Submit a fence task for each pool thread, each fence task
    // will block a thread from proceeding until all threads
    // are blocked, then all threads are signaled to continue
    enqueue_fence_tasks(pool, &(struct cc_task){
        .func = fenced_wait,
        .ctx = ctx,
    });

    // wait for all fences to clear
    for (size_t i = 0; i < pool->nthreads; ++i) {
        sem_wait(&ctx->fences_cleared);
    }

    pthread_mutex_destroy(&ctx->mutex);
    sem_destroy(&ctx->fences_cleared);
    pthread_cond_destroy(&ctx->cond);
    free(ctx);
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
