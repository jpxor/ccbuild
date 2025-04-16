/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef BUILD_H
#define BUILD_H

#include "libcc/cc_threadpool.h"
#include "libcc/cc_trie_map.h"
#include "str_list.h"
#include "build_opts.h"

#include <limits.h>

struct cmdopts {
    char* rootdir;
    const char *targets;
    int jlevel;
    bool debug;
    bool release;
};

struct build_state {
    struct cc_threadpool threadpool;
    struct cmdopts cmdopts;
    struct cc_trie optsmap;
    struct build_opts *target_build_opts;
    struct cc_trie src_files;
    struct str_list main_files;
    struct str_list obj_files;
    ccstr buildir;
    ccstr rootdir;
};

int cc_build(struct cmdopts *opts);
int cc_clean(struct cmdopts *opts);

#endif // BUILD_H
