/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef CMD_COMMON_H
#define CMD_COMMON_H

#include "vendor/cwalk/cwalk.h"
#include "libcc/cc_files.h"
#include "libcc/cc_strings.h"
#include "libcc/cc_trie_map.h"
#include "libcc/cc_threadpool.h"

#include "str_list.h"

#include <limits.h>
#include <stdio.h>

struct cmdopts {
    char* rootdir;
    const char *targets;
    int jlevel;
    bool debug;
    bool release;
};

int cc_clean(struct cmdopts *opts);
int cc_build(struct cmdopts *opts);

struct build_state {
    // common state for all targets
    ccstr buildir;
    ccstr rootdir;
    struct cmdopts cmdopts;
    struct cc_trie optsmap;
    struct cc_trie src_files;
    struct cc_threadpool threadpool;

    // target-specific state
    struct build_opts *target_opts;
    struct str_list main_files;
    struct str_list obj_files;
};

static inline
int set_root_and_build_paths(struct build_state *state) {
    char cwd[PATH_MAX];
    ccfs_cwd(cwd, sizeof cwd);

    // TODO: make ccstr wrapper around cwalk funcs
    ccstr_realloc(&state->rootdir, 4096);
    ccstr_realloc(&state->buildir, 4096);

    size_t reqlen = cwk_path_get_absolute(cwd, state->cmdopts.rootdir, state->rootdir.cstr, state->rootdir.cap);
    if (reqlen >= state->rootdir.cap) {
        printf("error: filepath too long (op: get_absolute)\n");
        return EXIT_FAILURE;
    }
    state->rootdir.len = reqlen;

    reqlen = cwk_path_join(state->rootdir.cstr, "build", state->buildir.cstr, state->buildir.cap);
    if (reqlen >= state->buildir.cap) {
        printf("error: filepath too long (op: join)\n");
        return EXIT_FAILURE;
    }
    state->buildir.len = reqlen;

    // all paths should be relative to project root (?)
    ccfs_chdir(state->rootdir.cstr);

    printf("rootdir='%s'\n", state->rootdir.cstr);
    printf("buildir='%s'\n", state->buildir.cstr);
    return 0;
}

#endif // CMD_COMMON_H