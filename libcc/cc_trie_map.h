/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_TRIE_MAP_H
#define _CC_TRIE_MAP_H

#include "cc_allocator.h"
#include "cc_assert_param.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

struct cc_trie {
    // uses this arena for allocating nodes as needed,
    // a new default arena will be created if arena is
    // left NULL on first insert
    struct cc_arena *arena;
    struct cc_trie_node *root;
    bool default_arena;
};

#define CC_TRIE_STR_KEY(str) (const uint8_t*)(str),strlen(str)

int cc_trie_insert(struct cc_trie *trie, const uint8_t *key, size_t keylen, void* val);
int cc_trie_iterate(struct cc_trie *trie, void *ctx, int (*callback)(void *ctx, void *leafdata));
void * cc_trie_search(struct cc_trie *trie, const uint8_t *key, size_t keylen);

int cc_trie_delete(struct cc_trie *trie, const uint8_t *key, size_t keylen);
void cc_trie_clear(struct cc_trie *trie);

#endif // _CC_TRIE_MAP_H

#ifdef CC_TRIE_MAP_IMPLEMENTATION

struct cc_trie_node {
    struct cc_trie_node *nodes[256];
    void *usrdata;
};

int cc_trie_insert(struct cc_trie *trie, const uint8_t *key, size_t keylen, void* val) {
    CC_ASSERT(val != NULL, EINVAL);
    CC_ASSERT(trie != NULL, EINVAL);
    CC_ASSERT(key != NULL && keylen > 0, EINVAL);

    if (trie->arena == NULL) {
        trie->arena = cc_new_arena_calloc_wrapper();
        trie->default_arena = true;
    }
    if (trie->root == NULL) {
        trie->root = cc_alloc(trie->arena, sizeof *trie->root);
        if (trie->root == NULL) {
            return ENOMEM;
        }
    }
    struct cc_trie_node *itr = trie->root;
    while (keylen > 0) {
        if (itr->nodes[key[0]] == NULL) {
            itr->nodes[key[0]] = cc_alloc(trie->arena, sizeof *trie->root);
            if (itr->nodes[key[0]] == NULL) {
                return ENOMEM;
            }
        }
        itr = itr->nodes[key[0]];
        --keylen;
        ++key;
    }
    assert(itr != NULL);
    itr->usrdata = val;
    return 0;
}

void * cc_trie_search(struct cc_trie *trie, const uint8_t *key, size_t keylen) {
    CC_ASSERT(trie != NULL, NULL);
    CC_ASSERT(key != NULL && keylen > 0, NULL);

    if (trie->root == NULL) {
        return NULL;
    }
    struct cc_trie_node *itr = trie->root;
    while (keylen > 0) {
        itr = itr->nodes[key[0]];
        if (itr == NULL) {
            return NULL;
        }
        --keylen;
        ++key;
    }
    assert(itr != NULL);
    return (void*)itr->usrdata;
}

int cc_trie_delete(struct cc_trie *trie, const uint8_t *key, size_t keylen) {
    CC_ASSERT(trie != NULL, EINVAL);
    CC_ASSERT(key != NULL && keylen > 0, EINVAL);

    if (trie->root == NULL) {
        return ENOENT;
    }
    struct cc_trie_node *itr = trie->root;
    while (keylen > 0) {
        itr = itr->nodes[key[0]];
        if (itr == NULL) {
            return ENOENT;
        }
        --keylen;
        ++key;
    }
    assert(itr != NULL);
    if (itr->usrdata == NULL) {
        return ENOENT;
    }
    itr->usrdata = NULL;
    return 0;
}

void cc_trie_clear(struct cc_trie *trie) {
    CC_ASSERT(trie != NULL, /*void*/);
    if (trie->default_arena) {
        cc_free_all(trie->arena);
    }
    trie->root = NULL;
}

static int cc_trie_iterate_recurse(struct cc_trie_node *node, void *ctx, int (*callback)(void *ctx, void *usrdata)) {
    if (node == NULL) return 0;

    for (size_t i = 0; i < 256; ++i) {
        int err = cc_trie_iterate_recurse(node->nodes[i], ctx, callback);
        if (err) return err;
    }
    return node->usrdata ? callback(ctx, node->usrdata) : 0;
}

int cc_trie_iterate(struct cc_trie *trie, void *ctx, int (*callback)(void *ctx, void *usrdata)) {
    CC_ASSERT(trie != NULL, EINVAL);
    CC_ASSERT(callback != NULL, EINVAL);
    return cc_trie_iterate_recurse(trie->root, ctx, callback);
}

#endif // CC_TRIE_MAP_IMPLEMENTATION
