/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include "build_opts.h"
#include "build_opts_def.h"
#include "build_opts_helpers.h"

#include "libcc/cc_files.h"
#include "libcc/cc_strings.h"
#include "libcc/cc_trie_map.h"

#include "vendor/inih/ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// global build options allocator,
// needs to be initialized before use
static struct cc_arena *g_opts_allocator = NULL;

// global default build options
static struct build_opts g_default_bopts = {
    .type = BIN,
    .so_version = 0,
    .lastmodified = 0,
    .target = CCSTR_STATIC(""),
    .cc = CCSTR_STATIC(""),
    .libname = CCSTR_STATIC("$(TARGET)"),
    .build_root = CCSTR_STATIC("./build/$(TARGET)/"),
    .install_root = CCSTR_STATIC("./install/$(TARGET)/"),
    .installdir = CCSTR_STATIC(""),
    .srcpaths = CCSTR_STATIC(". ./src"),
    .incpaths = CCSTR_STATIC(". ./includes"),
    .libpaths = CCSTR_STATIC("$(INSTALL_ROOT)/$(TARGET)"),
    .libs = CCSTR_STATIC(""),
    .ccflags = CCSTR_STATIC("-Wall -Wextra"),
    .ldflags = CCSTR_STATIC(""),
    .release = CCSTR_STATIC("-O2 -DNDEBUG -Werror"),
    .debug = CCSTR_STATIC("-g -O0 -D_FORTIFY_SOURCE=2"),
    .compile = CCSTR_STATIC("$(CC) $(CCFLAGS) [DEBUG_OR_RELEASE] -I[INCPATHS] -o [OBJPATH] -c [SRCPATH]"),
    .link = CCSTR_STATIC("$(CC) $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH]"),
    .link_static = CCSTR_STATIC("ar rcs [BINPATH].a [OBJS]"),
    .link_shared = CCSTR_STATIC("$(CC) -shared -fPIC $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH].so"),
};

// init new target opts by copying global default opts
void init_opts(struct build_opts *opts, const char *name) {
    ccstrcpy_raw(&opts->target, name);

    opts->type = g_default_bopts.type;
    opts->so_version = g_default_bopts.so_version;
    opts->lastmodified = g_default_bopts.lastmodified;

    for (int i = 0; build_option_defs[i].name != NULL; i++) {
        struct option_def def = build_option_defs[i];
        if (def.flags & OPTDEF_CCSTRCPY) {
            ccstr *dest = (ccstr*)OPT_VIA_OFFSET(opts, def.field_offset);
            ccstr *src = (ccstr*)OPT_VIA_OFFSET(&g_default_bopts, def.field_offset);
            ccstrcpy(dest, *src);
        }
    }
}

// compare parsed key from ini file to known option string,
// key may be upper or lower case and may have a + symbol
// to indicate intent to append to the current value
static bool match_opt(const char *opt, const char *key) {
    size_t optlen = strlen(opt);
    return strncasecmp(opt, key, optlen) == 0
        // key is at least as long as opt
        && (key[optlen] == 0 || key[optlen] == ' ' || key[optlen] == '+');
}

// if default cc is not set in the config, assume default
// is one of gcc|clang|cl
//
// if default cc in the config provides a list of compilers
// then resolve which compiler to use
static void resolve_default_cc(void) {
    if (g_default_bopts.cc.len == 0) {
        ccstrcpy_raw(&g_default_bopts.cc, find_compiler("gcc|clang|cl"));
    } else if (ccstrchr(ccsv(&g_default_bopts.cc), '|')) {
        ccstrcpy_raw(&g_default_bopts.cc, find_compiler(g_default_bopts.cc.cstr));
    }
}

// callback for ini parser, called for each key/value pair
// in the ini file and called when entering new [section]
int parse_opts_cb(void* ctx, const char* target, const char* key, const char* value) {
    struct cc_trie *target_opts_map = ctx;
    struct build_opts *target_opts = NULL;

    // parser detected new [section], which means new target
    if (key == NULL && value == NULL) {

        // resolve the default CC option before creating target opts
        // based on the default
        resolve_default_cc();

        struct build_opts *target_opts = cc_alloc(target_opts_map->arena, sizeof *target_opts);
        init_opts(target_opts, target);

        cc_trie_insert(target_opts_map, CC_TRIE_STR_KEY(target), target_opts);
        return 0;
    }

    // detect default opts vs specific target opts
    if (strlen(target) == 0) {
        target = "default";
        target_opts = &g_default_bopts;
    } else {
        target_opts = cc_trie_search(target_opts_map, CC_TRIE_STR_KEY(target));
        assert(target_opts != NULL);
    }

    // handles the parsed string, setting or appending the value
    // into the correct target_opts build option field
    for (int i = 0; build_option_defs[i].name != NULL; i++) {
        const struct option_def *def = &build_option_defs[i];
        if (match_opt(def->name, key)) {
            void *opt = (void*)OPT_VIA_OFFSET(target_opts, def->field_offset);
            def->opt_handler(def, opt, key, value);
            return 0;
        }
    }
    printf("config error: unknown option: '%s'.\n", key);
    exit(1);
}

static int resolve_variables_cb(void *ctx, void *data) {
    struct build_opts *opts = data;
    (void)ctx;

    // perform 3 passes to resolve variables
    // which allows for short chains of nested/dependent variables
    for (int pass = 0; pass < 3; ++pass) {

        // resolve each opt variable in order defined by build_option_defs
        for (int i = 0; build_option_defs[i].name != NULL; i++) {
            struct option_def def = build_option_defs[i];

            // opt must be a ccstr that allows variable expansion
            if (!(def.flags & OPTDEF_VAR_EXPAND)) {
                continue;
            }
            ccstr *optval = (ccstr*)OPT_VIA_OFFSET(opts, def.field_offset);

            size_t max_loops = 10;
            for (size_t j = 0;; ++j) {
                ccstrview var = find_variable(ccsv(optval));
                if (var.len == 0) {
                    break;
                }
                if (j >= max_loops) {
                    printf("config error: failed to resolve variable '%.*s'\n", var.len, var.cstr);
                    exit(1);
                }
                ccstrview varname = ccsv_slice(var, 2, var.len - 3);
                ccstrview value = get_var_value(build_option_defs, opts, varname);
                ccstr_replace(optval, var, value);
            }
        }
    }
    return 0;
}

// TODO: what happens if no config file?
struct cc_trie parse_build_opts(const char *filename) {
    // init globals
    if (!g_opts_allocator) {
        g_opts_allocator = cc_new_arena_calloc_wrapper();
        g_default_bopts.lastmodified = ccfs_last_modified_time(filename);
    }
    struct cc_trie target_opts_map = {
        .arena = g_opts_allocator,
    };

    ini_parse(filename, parse_opts_cb, &target_opts_map);
    foreach_target(&target_opts_map, resolve_variables_cb);
    return target_opts_map;
}

// static
// int print_opts_cb(void *ctx, void *data) {
//     struct build_opts *opts = data;
//     (void)ctx;

//     printf("[%s]\n", opts->target.base);
//     printf("type: flag(%d)\n", opts->type);
//     if (opts->type & (SHARED|STATIC)) {
//         printf("so_version: %u\n", opts->so_version);
//     }
// #define printopt(optname) printf(#optname" = '%s'\n", opts->optname.base)
//     printopt(compile);
//     printopt(link);
//     printopt(build_root);
//     printopt(install_root);
//     printopt(installdir);
//     printopt(srcpaths);
//     printopt(incpaths);
//     printopt(libpaths);
//     printopt(ccflags);
//     printopt(ldflags);
//     printopt(release);
//     printopt(debug);
//     printopt(libs);
// #undef printopt
//     printf("\n");
//     return 0;
// }

// void print_configs(struct parsed_opts parsed_opts) {
//     foreach_target(&parsed_opts, print_opts_cb);
// }