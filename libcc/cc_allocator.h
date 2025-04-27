/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_ALLOCATOR_H
#define _CC_ALLOCATOR_H

#include <stdint.h>
#include <stddef.h>

// arena that wraps calloc/free
struct cc_arena* cc_new_arena_calloc_wrapper(void);
void cc_destroy_arena_calloc_wrapper(struct cc_arena* a);

// arena that uses mmap to reserve address range and maps
// memmory into the process in 64KB blocks (default)
struct cc_arena* cc_new_arena_bump_allocator(size_t reserve_size);
void cc_destroy_arena_bump_allocator(struct cc_arena* a);

// simple check for out of bounds writes, print or abort on detection
void cc_arena_debug_outofbounds_check(void *rawptr, int do_abort);

// 64KB blocks
#ifndef BUMP_BLOCK_SIZE
#define BUMP_BLOCK_SIZE (64 * 1024)
#endif

struct cc_alloc_debug_info {
#ifndef NDEBUG
    const char* file;
    const char* function;
    int line;
    size_t size;
#endif
};

struct cc_arena {
    void* (*alloc)(struct cc_arena *arena, size_t size, struct cc_alloc_debug_info debug);
    void (*free_all)(struct cc_arena *arena);
};

static inline
void cc_free_all(struct cc_arena *a) {
    a->free_all(a);
}

#ifdef NDEBUG

static inline
void* cc_alloc(struct cc_arena *a,  size_t size) {
    return a->alloc(a, size, (struct cc_alloc_debug_info){});
}

#else // DEBUG
#define CC_ARENA_GUARDSIZE 16
#define CC_ARENA_GUARD_PATTERN "0123456789ABCDEF"

#define cc_alloc(arena, _size) \
    (arena)->alloc(arena, _size, (struct cc_alloc_debug_info) { \
        .file = __FILE__,     \
        .function = __func__, \
        .line = __LINE__,     \
        .size = _size,        \
    })
#endif

#endif // _CC_ALLOCATOR_H

#ifdef CC_ALLOCATOR_IMPLEMENTATION

#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#ifndef CC_LOGF
// set default logging
#include <stdio.h>
#define CC_LOGF printf
#endif

#ifndef NDEBUG
#include <string.h>
#include <stdbool.h>
#endif

#define MAGIC 0xCCA110C2025

// offset bytes from void ptr
#define PTR_OFFSET(ptr, offset) ((void*)((intptr_t)(ptr) + (offset)))

struct allocation {
    struct allocation *next;
#ifndef NDEBUG
    uint64_t magic;
    struct cc_alloc_debug_info debug;
    char guard[CC_ARENA_GUARDSIZE];
#endif
    char block[];
};

struct generic_arena {
    struct cc_arena arena;
    struct allocation *node;
    pthread_mutex_t mutex;
    size_t total_allocated_bytes;
    size_t count;
};

#ifndef NDEBUG
static inline
bool guard_check(struct allocation *ptr) {
    bool detected = false;
    if (ptr->magic != MAGIC) {
        CC_LOGF("ERROR: corrupt allocation or address was not allocated by cc_alloc\n");
        return true;
    }
    if (strncmp(ptr->guard, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE) != 0) {
        CC_LOGF("ERROR: detected out of bounds write ahead of arena memory allocation\n");
        detected = true;
    }
    if (strncmp(ptr->block+ptr->debug.size, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE) != 0) {
        CC_LOGF("ERROR: detected out of bounds write past arena memory allocation\n");
        detected = true;
    }
    if (detected) {
        CC_LOGF("     : was allocated here: %s:%d (%s)\n", ptr->debug.file, ptr->debug.line, ptr->debug.function);
        CC_LOGF("     : ptr 0x%p, size %zu bytes\n", ptr->block, ptr->debug.size);
    }
    return detected;
}
#endif

void cc_arena_debug_outofbounds_check(void *rawptr, int do_abort) {
    (void)rawptr;
    (void)do_abort;
#ifndef NDEBUG
    struct allocation *ptr = PTR_OFFSET(rawptr, -offsetof(struct allocation, block));
    if (guard_check(ptr) && do_abort) {
        abort();
    }
#endif
}

////////////// CALLOC WRAPPER //////////////////

static inline
void* calloc_wrapper_alloc(struct cc_arena *a, size_t size, struct cc_alloc_debug_info debug) {
    struct generic_arena *arena = (void*)a;
    (void)debug;

    size_t guardsize = 0;
    #ifndef NDEBUG
    guardsize += CC_ARENA_GUARDSIZE;
    #endif

    if (size > SIZE_MAX - sizeof(struct allocation) - guardsize) {
        errno = ENOMEM;
        return NULL;
    }
    size_t alloc_size = size + sizeof(struct allocation) + guardsize;
    struct allocation *next = calloc(1, alloc_size);
    if (next == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&arena->mutex);
    next->next = arena->node;
    arena->node = next;
    arena->count++;
    arena->total_allocated_bytes += size;

    #ifndef NDEBUG
    next->magic = MAGIC;
    next->debug = debug;
    memcpy(next->guard, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE);
    memcpy(next->block + size, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE);
    #endif

    pthread_mutex_unlock(&arena->mutex);
    return next->block;
}

static inline
void calloc_wrapper_free_all(struct cc_arena *a) {
    struct generic_arena *arena = (void*)a;

    pthread_mutex_lock(&arena->mutex);
    while (arena->node != NULL) {
        struct allocation *ptr = arena->node;
        arena->node = arena->node->next;
        
        #ifndef NDEBUG
        guard_check(ptr);
        #endif

        free(ptr);
    }
    arena->count = 0;
    arena->total_allocated_bytes = 0;
    pthread_mutex_unlock(&arena->mutex);
}

struct cc_arena* cc_new_arena_calloc_wrapper(void) {
    struct generic_arena *a = calloc(1, sizeof *a);
    if (!a) return NULL;

    a->arena = (struct cc_arena){
        .alloc = calloc_wrapper_alloc,
        .free_all = calloc_wrapper_free_all,
    };
    pthread_mutex_init(&a->mutex, NULL);
    return (struct cc_arena*) a;
}

void cc_destroy_arena_calloc_wrapper(struct cc_arena* a) {
    struct generic_arena *arena = (void*)a;
    cc_free_all(a);
    pthread_mutex_destroy(&arena->mutex);
    free(arena);
}


///////////////  MMAP BUMP ALLOCATOR ////////////////////

struct bump_arena {
    struct generic_arena wrapped_arena;
    void* base;
    size_t reserved_size;
    size_t committed_size;
    size_t used_size;
};


#ifdef _WIN32
#include <windows.h>
#define MAP_FAILED NULL
static void* platform_mmap(size_t size) {
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

static void* platform_commit(void* addr, size_t size) {
    return VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
}

static void platform_decommit(void* addr, size_t size) {
    VirtualFree(addr, size, MEM_DECOMMIT);
}

static void platform_munmap(void* addr, size_t size) {
    (void)size;
    VirtualFree(addr, 0, MEM_RELEASE);
}
#else
#include <sys/mman.h>
static void* platform_mmap(size_t size) {
    return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void* platform_commit(void* addr, size_t size) {
    mprotect(addr, size, PROT_READ | PROT_WRITE);
    return addr;
}

static void platform_decommit(void* addr, size_t size) {
    mprotect(addr, size, PROT_NONE);
    madvise(addr, size, MADV_DONTNEED);
}

static void platform_munmap(void* addr, size_t size) {
    munmap(addr, size);
}
#endif

static inline
void* bump_alloc(struct cc_arena *a, size_t size, struct cc_alloc_debug_info debug) {
    struct bump_arena *arena = (void*)a;
    (void)debug;
    
    size_t alloc_size = size + sizeof(struct allocation);
    #ifndef NDEBUG
    alloc_size += CC_ARENA_GUARDSIZE;
    #endif

    // 16-byte alignment
    size_t aligned_size = (alloc_size + 15) & ~15;

    pthread_mutex_lock(&arena->wrapped_arena.mutex);
    
    if (arena->used_size + aligned_size > arena->reserved_size) {
        pthread_mutex_unlock(&arena->wrapped_arena.mutex);
        return NULL;
    }

    // Check if we need to commit more memory
    size_t required_size = arena->used_size + aligned_size;
    if (required_size > arena->committed_size) {

        void* commit_addr = PTR_OFFSET(arena->base, arena->committed_size);
        size_t commit_size = ((required_size - arena->committed_size + BUMP_BLOCK_SIZE - 1) 
                             & ~(BUMP_BLOCK_SIZE - 1));
        
        if (arena->committed_size + commit_size > arena->reserved_size) {
            commit_size = arena->reserved_size - arena->committed_size;
        }
        if (platform_commit(commit_addr, commit_size) == MAP_FAILED) {
            pthread_mutex_unlock(&arena->wrapped_arena.mutex);
            return NULL;
        }
        arena->committed_size += commit_size;
    }

    // bump
    struct allocation *next = PTR_OFFSET(arena->base, arena->used_size);
    arena->used_size += aligned_size;

    next->next = arena->wrapped_arena.node;
    arena->wrapped_arena.node = next;
    arena->wrapped_arena.count++;
    arena->wrapped_arena.total_allocated_bytes += size;

    #ifndef NDEBUG
    next->magic = MAGIC;
    next->debug = debug;
    memcpy(next->guard, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE);
    memcpy(next->block + size, CC_ARENA_GUARD_PATTERN, CC_ARENA_GUARDSIZE);
    #endif

    pthread_mutex_unlock(&arena->wrapped_arena.mutex);
    return next->block;
}

static inline
void bump_free_all(struct cc_arena *a) {
    struct bump_arena *arena = (void*)a;
    struct generic_arena *ga = &arena->wrapped_arena;

    pthread_mutex_lock(&ga->mutex);
    
    #ifndef NDEBUG
    struct allocation *itr = ga->node;
    while (itr != NULL) {
        guard_check(itr);
        itr = itr->next;
    }
    #endif
    ga->node = NULL;
    
    platform_decommit(arena->base, arena->committed_size);

    arena->used_size = 0;
    arena->committed_size = 0;
    arena->wrapped_arena.count = 0;
    arena->wrapped_arena.total_allocated_bytes = 0;
    
    pthread_mutex_unlock(&ga->mutex);
}

struct cc_arena* cc_new_arena_bump_allocator(size_t reserve_size) {
    struct bump_arena *a = calloc(1, sizeof *a);
    if (!a) return NULL;

    a->base = platform_mmap(reserve_size);
    if (a->base == MAP_FAILED) {
        free(a);
        return NULL;
    }
    a->wrapped_arena.arena = (struct cc_arena){
        .alloc = bump_alloc,
        .free_all = bump_free_all,
    };
    a->reserved_size = reserve_size;
    a->committed_size = 0;
    a->used_size = 0;
    pthread_mutex_init(&a->wrapped_arena.mutex, NULL);
    return (struct cc_arena*)a;
}

void cc_destroy_arena_bump_allocator(struct cc_arena* a) {
    struct bump_arena *arena = (void*)a;
    cc_free_all(a);
    pthread_mutex_destroy(&arena->wrapped_arena.mutex);
    platform_munmap(arena->base, arena->reserved_size);
    free(arena);
}

#endif // implementation
