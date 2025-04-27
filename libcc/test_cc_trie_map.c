/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) 2025 Josh Simonot
 */

#define CC_TRIE_MAP_IMPLEMENTATION
#include "cc_trie_map.h"

#define CC_ALLOCATOR_IMPLEMENTATION
#include "cc_allocator.h"

#include <stdio.h>
#include "libcc/cc_test.h"

struct iterate_ctx {
    int sum;
    int count;
};

int test_iterate_callback(void *ctx, void *usrdata) {
    intptr_t usr_val = (intptr_t)usrdata;
    if (usr_val == 42) {
        return -1; // any error tp exit the iteration
    }
    struct iterate_ctx *ictx = ctx;
    ictx->sum += usr_val;
    ictx->count++;
    return 0;
}

int test_insert_delete(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    uint8_t usr_test_val2[] = {0, 255, 128};

    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2), 0);

    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)), 0);
    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val1, strlen(usr_test_val1)), 0);
    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val2, sizeof(usr_test_val2)), 0);
    
    CHKEQ_INT(cc_trie_delete(&trie, "123", 3), ENOENT);
    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)), ENOENT);
    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val1, strlen(usr_test_val1)), ENOENT);

    cc_trie_clear(&trie);
    return 0;
}

int test_search(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    uint8_t usr_test_val2[] = {0, 255, 128};

    CHKEQ_PTR(cc_trie_search(&trie, "123", 3), NULL);

    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2), 0);

    CHKEQ_PTR(cc_trie_search(&trie, usr_test_val0, strlen(usr_test_val0)), usr_test_val0);
    CHKEQ_PTR(cc_trie_search(&trie, usr_test_val1, strlen(usr_test_val1)), usr_test_val1);
    CHKEQ_PTR(cc_trie_search(&trie, usr_test_val2, sizeof(usr_test_val2)), usr_test_val2);

    CHKEQ_PTR(cc_trie_search(&trie, "hell", 4), NULL);
    CHKEQ_PTR(cc_trie_search(&trie, "hellow", 6), NULL);

    CHKEQ_INT(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)), 0);
    CHKEQ_PTR(cc_trie_search(&trie, usr_test_val0, strlen(usr_test_val0)), NULL);

    cc_trie_clear(&trie);
    return 0;
}

int test_iterate(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    uint8_t usr_test_val2[] = {0, 255, 128};

    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), (void*)2), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), (void*)4), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), (void*)8), 0);

    struct iterate_ctx ctx = {
        .sum = 0,
        .count = 0,
    };
    CHKEQ_INT(cc_trie_iterate(&trie, &ctx, test_iterate_callback), 0);
    CHKEQ_INT(ctx.count, 3);
    CHKEQ_INT(ctx.sum, 14);

    cc_trie_clear(&trie);
    return 0;
}

int test_iterate_exit_early(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    uint8_t usr_test_val2[] = {0, 255, 128};

    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), (void*)2), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), (void*)42), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), (void*)8), 0);

    struct iterate_ctx ctx = {
        .sum = 0,
        .count = 0,
    };
    CHKEQ_INT(cc_trie_iterate(&trie, &ctx, test_iterate_callback), -1);
    assert(ctx.count < 3);
    assert(ctx.sum < 14);

    cc_trie_clear(&trie);
    return 0;
}

int test_extern_allocator(void) {
    struct cc_arena *arena = cc_new_arena_bump_allocator(6*4096);
    struct cc_trie trie = {
        .arena = arena,
    };
    char usr_test_val0[] = "helloworld";
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0), 0);

    cc_trie_clear(&trie);
    cc_free_all(arena);
    return 0;
}

int test_clear(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    uint8_t usr_test_val2[] = {0, 255, 128};

    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1), 0);
    CHKEQ_INT(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2), 0);

    cc_trie_clear(&trie);

    assert(trie.arena != NULL);
    CHKEQ_PTR(trie.root, NULL);
    return 0;
}

int main(void) {
    int err = 0;

    err |= test_insert_delete();
    err |= test_search();
    err |= test_iterate();
    err |= test_iterate_exit_early();
    err |= test_extern_allocator();
    err |= test_clear();

    printf("[%s] test cc_trie_map\n", err? "FAILED": "PASSED");
    return 0;
}
