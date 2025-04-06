/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

// disable logging for tests
#define CC_LOGF(...)

#define CC_ALLOCATOR_IMPLEMENTATION
#include "cc_allocator.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_basic_alloc(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();
    struct generic_arena* base_arena = (void*)arena;

    void* ptr0 = cc_alloc(arena, 0);
    void* ptr1 = cc_alloc(arena, 16);
    void* ptr2 = cc_alloc(arena, 1024);

    assert(ptr0 != NULL);
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);

    char test[16] = {
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42
    };

    memcpy(ptr1, &test, 16);
    memset(ptr2, 0x00, 1024);
    assert(memcmp(ptr1, &test, 16) == 0);

    struct allocation *a0 = (struct allocation *)(ptr0 - sizeof(struct allocation));
    struct allocation *a1 = (struct allocation *)(ptr1 - sizeof(struct allocation));
    struct allocation *a2 = (struct allocation *)(ptr2 - sizeof(struct allocation));

    assert(base_arena->node == a2 || base_arena->node == a0);
    assert(base_arena->total_allocated_bytes == 16+1024);
    assert(base_arena->count == 3);

    assert(a0->debug.size == 0);
    assert(a1->debug.size == 16);
    assert(a2->debug.size == 1024);

    assert(a0->magic == MAGIC);
    assert(a1->magic == MAGIC);
    assert(a2->magic == MAGIC);

    cc_free_all(arena);

    assert(base_arena->count == 0);
    assert(base_arena->total_allocated_bytes == 0);
    assert(base_arena->node == NULL);

    cc_destroy_arena_bump_allocator(arena);
    arena = NULL; // destroy frees the arena
}

static void test_alloc_alignment(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();

    void* ptr1 = cc_alloc(arena, 1);
    void* ptr2 = cc_alloc(arena, 30);

    assert(((uintptr_t)ptr1 & 15) == 0);
    assert(((uintptr_t)ptr2 & 15) == 0);

    cc_destroy_arena_bump_allocator(arena);
}

static void test_guard_check(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();
    
    void* ptr0 = cc_alloc(arena, 128);
    void* ptr1 = cc_alloc(arena, 128);
    void* ptr2 = cc_alloc(arena, 128);

    assert(ptr0 != NULL);
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    
    struct allocation *a0 = (struct allocation *)(ptr0 - sizeof(struct allocation));
    struct allocation *a1 = (struct allocation *)(ptr1 - sizeof(struct allocation));
    struct allocation *a2 = (struct allocation *)(ptr2 - sizeof(struct allocation));

    assert(guard_check(a0) == false);
    assert(guard_check(a1) == false);
    assert(guard_check(a2) == false);
    
    // corrupt the magic number
    a0->magic = 0xDEADBEEF;
    assert(guard_check(a0) == true);
    
    // check for out of bounds writes
    memset(ptr1+1, 0x01, 128);
    assert(guard_check(a1) == true);

    memset(ptr2-1, 0x01, 128);
    assert(guard_check(a2) == true);
    
    cc_free_all(arena);
    cc_destroy_arena_bump_allocator(arena);
}

static void test_bump_out_of_memory(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();

    void* ptr1 = cc_alloc(arena, 512);
    assert(ptr1 != NULL);

    void* ptr2 = cc_alloc(arena, 2048000);
    assert(ptr2 == NULL);

    cc_destroy_arena_bump_allocator(arena);
}

static void test_bump_reuse(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();

    void* ptr1 = cc_alloc(arena, 1000);
    assert(ptr1 != NULL);

    char test[16] = {
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42,
        0x42, 0x42, 0x42, 0x42
    };
    memcpy(ptr1, &test, 16);

    cc_free_all(arena);

    void* ptr2 = cc_alloc(arena, 1000);
    assert(ptr2 != NULL);

    // same pointer, but can't access old data
    char zeros[16] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    assert(ptr2 == ptr1);
    assert(memcmp(ptr2, &zeros, 16) == 0);

    cc_destroy_arena_bump_allocator(arena);
}

static void test_bump_multiple_allocs(struct cc_arena*(*make_arena)(void)) {
    struct cc_arena* arena = make_arena();
    void* ptrs[1000];

    for (int i = 0; i < 1000; i++) {
        ptrs[i] = cc_alloc(arena, 128);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], i & 0xFF, 128);
    }
    cc_free_all(arena);
    cc_destroy_arena_bump_allocator(arena);
}

static void test_calloc_allocator(void) {
    test_basic_alloc(cc_new_arena_calloc_wrapper);
    test_alloc_alignment(cc_new_arena_calloc_wrapper);
    test_guard_check(cc_new_arena_calloc_wrapper);
    printf("[PASSED] test calloc_wrapper allocator\n");
}

static size_t bump_allocator_reserve_size = 0;
static struct cc_arena* bump_allocator_factory() {
    return cc_new_arena_bump_allocator(bump_allocator_reserve_size);
}

static void test_bump_allocator(void) {
    bump_allocator_reserve_size = 2048u;
    test_basic_alloc(bump_allocator_factory);
    test_alloc_alignment(bump_allocator_factory);
    test_guard_check(bump_allocator_factory);
    test_bump_out_of_memory(bump_allocator_factory);
    test_bump_reuse(bump_allocator_factory);

    bump_allocator_reserve_size = 2048000u;
    test_bump_multiple_allocs(bump_allocator_factory);

    printf("[PASSED] test bump allocator\n");
}

int main(void) {
    test_calloc_allocator();
    test_bump_allocator();
    return 0;
}
