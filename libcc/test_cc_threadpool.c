/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#define CC_THREADPOOL_MAX_THREADS 8
#define CC_THREADPOOL_IMPLEMENTATION
#include "cc_threadpool.h"

#include "cc_test.h"

#include <stdatomic.h>

static void test_task_inc(void* ctx) {
    int* val = ctx;
    atomic_fetch_add(val, 1);
}

int test_init(void) {
    struct cc_threadpool pool;

    // cannot exceed max thread count
    CHKEQ_INT(cc_threadpool_init(&pool, 420), EINVAL);

    // happy path
    CHKEQ_INT(cc_threadpool_init(&pool, 8), 0);
    CHKEQ_INT(pool.nthreads, 8);
    CHKEQ_INT(pool.head, 0);
    CHKEQ_INT(pool.tail, 0);

    cc_threadpool_stop_and_wait(&pool);
    return 0;
}

int test_submit(void) {
    struct cc_threadpool pool;
    cc_threadpool_init(&pool, 4);

    // cannot submit null task
    CHKEQ_INT(cc_threadpool_submit(&pool, NULL, NULL), EINVAL);

    // submit 4 tasks
    _Atomic int val = 0;
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    cc_threadpool_stop_and_wait(&pool);
    CHKEQ_INT(val, 4);

    return 0;
}

int test_fence(void) {
    struct cc_threadpool pool;
    cc_threadpool_init(&pool, 4);

    // empty queue
    cc_threadpool_fenced_wait(&pool);

    // submit 6 tasks (more than nthreads)
    _Atomic int val = 0;
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    cc_threadpool_fenced_wait(&pool);
    CHKEQ_INT(val, 6);

    // submit 1 more (less than nthreads)
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    cc_threadpool_fenced_wait(&pool);
    CHKEQ_INT(val, 7);

    // submit 4 more (same as nthreads)
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    CHKEQ_INT(cc_threadpool_submit(&pool, &val, test_task_inc), 0);
    cc_threadpool_fenced_wait(&pool);
    CHKEQ_INT(val, 11);

    // empty + called multiple times in a row
    cc_threadpool_fenced_wait(&pool);
    cc_threadpool_fenced_wait(&pool);

    cc_threadpool_stop_and_wait(&pool);
    return 0;
}

int main(void) {
    int err = 0;
    
    err |= test_init();
    err |= test_submit();
    err |= test_fence();

    printf("[%s] test cc_threadpool\n", err? "FAILED": "PASSED");
    return 0;
}
