/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef _BUILD_OPTS_H_
#define _BUILD_OPTS_H_

#include "libcc/cc_strings.h"
#include "libcc/cc_trie_map.h"

enum target_type {
    BIN =    0b001,
    SHARED = 0b010,
    STATIC = 0b100,
};

struct build_opts {
    ccstr target;
    ccstr cc;
    ccstr build_root;
    ccstr install_root;
    ccstr installdir;
    ccstr srcpaths;
    ccstr incpaths;
    ccstr libpaths;
    ccstr ccflags;
    ccstr ldflags;
    ccstr libs;
    ccstr compile;
    ccstr link;
    ccstr link_static;
    ccstr link_shared;
    ccstr release;
    ccstr debug;
    ccstr libname;
    time_t lastmodified;
    int so_version;
    enum target_type type;
};

struct cc_trie parse_build_opts(ccstr rootdir);
void print_config(const struct build_opts *bopts);

#endif // _BUILD_OPTS_H_
