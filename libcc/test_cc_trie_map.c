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

void test_insert_delete(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    char usr_test_val2[] = {0, 255, 128};

    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0) == 0);
    assert(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1) == 0);
    assert(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2) == 0);

    assert(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)) == 0);
    assert(cc_trie_delete(&trie, usr_test_val1, strlen(usr_test_val1)) == 0);
    assert(cc_trie_delete(&trie, usr_test_val2, sizeof(usr_test_val2)) == 0);
    
    assert(cc_trie_delete(&trie, "123", 3) == ENOENT);
    assert(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)) == ENOENT);
    assert(cc_trie_delete(&trie, usr_test_val1, strlen(usr_test_val1)) == ENOENT);

    cc_trie_clear(&trie);
}

void test_search(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    char usr_test_val2[] = {0, 255, 128};

    assert(cc_trie_search(&trie, "123", 3) == NULL);

    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0) == 0);
    assert(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1) == 0);
    assert(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2) == 0);

    assert(cc_trie_search(&trie, usr_test_val0, strlen(usr_test_val0)) == usr_test_val0);
    assert(cc_trie_search(&trie, usr_test_val1, strlen(usr_test_val1)) == usr_test_val1);
    assert(cc_trie_search(&trie, usr_test_val2, sizeof(usr_test_val2)) == usr_test_val2);

    assert(cc_trie_search(&trie, "hell", 4) == NULL);
    assert(cc_trie_search(&trie, "hellow", 6) == NULL);

    assert(cc_trie_delete(&trie, usr_test_val0, strlen(usr_test_val0)) == 0);
    assert(cc_trie_search(&trie, usr_test_val0, strlen(usr_test_val0)) == NULL);

    cc_trie_clear(&trie);
}

void test_iterate(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    char usr_test_val2[] = {0, 255, 128};

    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), (void*)2) == 0);
    assert(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), (void*)4) == 0);
    assert(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), (void*)8) == 0);

    struct iterate_ctx ctx = {
        .sum = 0,
        .count = 0,
    };
    assert(cc_trie_iterate(&trie, &ctx, test_iterate_callback) == 0);
    assert(ctx.count == 3);
    assert(ctx.sum == 14);

    cc_trie_clear(&trie);
}

void test_iterate_exit_early(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    char usr_test_val2[] = {0, 255, 128};

    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), (void*)2) == 0);
    assert(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), (void*)42) == 0);
    assert(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), (void*)8) == 0);

    struct iterate_ctx ctx = {
        .sum = 0,
        .count = 0,
    };
    assert(cc_trie_iterate(&trie, &ctx, test_iterate_callback) == -1);
    assert(ctx.count < 3);
    assert(ctx.sum < 14);

    cc_trie_clear(&trie);
}

void test_extern_allocator(void) {
    struct cc_arena *arena = cc_new_arena_bump_allocator(6*4096);
    struct cc_trie trie = {
        .arena = arena,
    };
    char usr_test_val0[] = "helloworld";
    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0) == 0);

    cc_trie_clear(&trie);
    cc_free_all(arena);
}

void test_clear(void) {
    struct cc_trie trie = {0};

    char usr_test_val0[] = "helloworld";
    char usr_test_val1[] = "hello";
    char usr_test_val2[] = {0, 255, 128};

    assert(cc_trie_insert(&trie, usr_test_val0, strlen(usr_test_val0), usr_test_val0) == 0);
    assert(cc_trie_insert(&trie, usr_test_val1, strlen(usr_test_val1), usr_test_val1) == 0);
    assert(cc_trie_insert(&trie, usr_test_val2, sizeof(usr_test_val2), usr_test_val2) == 0);

    cc_trie_clear(&trie);

    assert(trie.arena != NULL);
    assert(trie.root == NULL);
}

int main(void) {
    test_insert_delete();
    test_search();
    test_iterate();
    test_iterate_exit_early();
    test_extern_allocator();
    test_clear();
    printf("[PASSED] test trie map\n");
    return 0;
}
