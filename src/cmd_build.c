/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include "cmd.h"
#include "cmd_build_helpers.h"
#include "cmd_build_compile.h"
#include "cmd_build_link.h"
#include "build_opts.h"

#include <stdio.h>

// Ensures each path in a space-separated path list has the specified prefix.
// exmaple: include paths with -I prefix, lib paths with -L prefix
static ccstr tidy_pathlist(ccstr inclist, ccstrview prefix) {
    ccstrview sv = ccsv(&inclist);
    ccstrview path;

    int numPaths = 1 + ccsv_charcount(sv, ' ');
    int numPrefix = ccsv_strcount(sv, prefix);

    // shortcut
    if ((sv.len == 0) || (numPrefix == numPaths)) {
        return inclist;
    }

    // add include flag to each path if not present
    ccstr newincpaths = ccstr_empty(inclist.len + 2*(numPaths-numPrefix));

    ccstr space_and_prefix = CCSTR_STATIC(" ");
    ccstr_append(&space_and_prefix, prefix);

    while (sv.len > 0) {
        path = ccsv_tokenize(&sv, ' ');
        if (ccstrncmp(path, prefix, prefix.len) == 0) {
            ccstr_append_join(&newincpaths, ccsv_raw(" "), &path, 1);
        } else {
            ccstr_append_join(&newincpaths, ccsv(&space_and_prefix), &path, 1);
        }
    }
    free(inclist.cstr);
    return newincpaths;
}

// fill in the per-target level predefined compile command template placeholders
static void resolve_compile_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    if (cmdopts->release) {
        ccstr_replace(cmd, ccsv_raw("[DEBUG_OR_RELEASE]"), ccsv(&opts->release));
    } else {
        ccstr_replace(cmd, ccsv_raw("[DEBUG_OR_RELEASE]"), ccsv(&opts->debug));
    }
    ccstr_replace(cmd, ccsv_raw("-I[INCPATHS]"), ccsv(&opts->incpaths));
}

// fill in the per-target level predefined link command template placeholders
static void resolve_link_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    (void)cmdopts;
    ccstr_replace(cmd, ccsv_raw("-L[LIBPATHS]"), ccsv(&opts->libpaths));
}

// callback, executed on each build target to initiate a build
static int build_target_cb(void *ctx, void *data) {
    struct build_state *state = ctx;
    struct build_opts *opts = data;

    // a simple string search means a selected target can match  multiple targets if
    // its shows up as a substring... this was not intentional but maybe a feature
    // worth keeping?
    if (state->cmdopts.targets != NULL) {
        bool target_matches = (ccstrstr(ccsv(&opts->target), ccsv_raw(state->cmdopts.targets)) == 0);
        if (!target_matches) {
            return 0;
        }
    }
    
    // clean up/reset per target
    str_list_clear(&state->main_files);
    str_list_clear(&state->obj_files);

    state->target_build_opts = opts;
    printf("\nINFO: building target '%s'\n", opts->target.cstr);

    // resolve path lists
    opts->incpaths = tidy_pathlist(opts->incpaths, ccsv_raw("-I"));
    opts->libpaths = tidy_pathlist(opts->libpaths, ccsv_raw("-L"));

    // resolve commands
    resolve_compile_cmd(&opts->compile, &state->cmdopts, opts);
    resolve_link_cmd(&opts->link, &state->cmdopts, opts);

    cc_threadpool_init(&state->threadpool, state->cmdopts.jlevel);

    int err = foreach_src_file(state, opts->srcpaths, dispatch_compilation_cb);
    if (err) return err;

    cc_threadpool_stop_and_wait(&state->threadpool);

    if (opts->type & BIN) {
        foreach_main_file(state, link_object_files_cb);
    }

    if (opts->type & (SHARED|STATIC) || state->main_files.count == 0) {
        link_libs(state);
    }

    printf("\n");
    return 0;
}

int cc_build(struct cmdopts *cmdopts) {
    struct build_state state = {
        .cmdopts = *cmdopts,
    };
    int err = set_root_and_build_paths(&state);
    if (err) return EXIT_FAILURE;

    state.optsmap = parse_build_opts(state.rootdir);
    foreach_target(&state, build_target_cb);
    return EXIT_SUCCESS;
}
