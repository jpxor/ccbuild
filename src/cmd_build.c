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
static ccstr* tidy_pathlist(ccstr *pathlist, ccstr prefix) {
    ccstr sv = *pathlist;
    ccstr path;

    int numPaths = 1 + ccsv_charcount(sv, ' ');
    int numPrefix = ccsv_strcount(sv, prefix);

    if ((sv.len == 0) || (numPrefix == numPaths)) {
        return pathlist;
    }

    char tmp0_store[prefix.len + 2 /*space & null*/];
    ccstr space_and_prefix = CCSTR(tmp0_store, 0, sizeof tmp0_store);

    ccstr_append(&space_and_prefix, CCSTR_LITERAL(" "));
    ccstr_append(&space_and_prefix, prefix);

    char tmp1_store[pathlist->len + 3*(numPaths-numPrefix) + 1];
    ccstr newpathlist = CCSTR(tmp1_store, 0, sizeof tmp1_store);

    while (sv.len > 0) {
        path = ccstr_tokenize(&sv, ' ');
        if (ccstrncmp(path, prefix, prefix.len) == 0) {
            ccstr_append_join(&newpathlist, CCSTR_LITERAL(" "), &path, 1);
        } else {
            ccstr_append_join(&newpathlist, space_and_prefix, &path, 1);
        }
    }
    ccstrcpy(pathlist, newpathlist);
    return pathlist;
}

// fill in the per-target predefined template placeholders for the compile command
static void resolve_compile_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    ccstr debug_or_release = (cmdopts->release)? opts->release : opts->debug;
    ccstr_replace(cmd, CCSTR_LITERAL("[DEBUG_OR_RELEASE]"), debug_or_release);
    ccstr_replace(cmd, CCSTR_LITERAL("-I[INCPATHS]"), opts->incpaths);
}

// fill in the per-target predefined template placeholders for the link command
static void resolve_link_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    (void)cmdopts;
    ccstr_replace(cmd, CCSTR_LITERAL("-L[LIBPATHS]"), opts->libpaths);
}

// callback, executed on each source file to initiate a compilation
static int dispatch_compilation_cb(void *ctx, const char *srcpath) {
    struct build_state *state = ctx;

    // TODO: get memory from pool allocator (make per-target arena allocator)
    struct compilation_task_ctx *taskctx = calloc(1, sizeof*taskctx);

    taskctx->state = state;
    ccstrcpy(&taskctx->srcpath, CCSTR_VIEW(srcpath, strlen(srcpath)));
    return cc_threadpool_submit(&state->threadpool, taskctx, compile_translation_unit_cb);
}

// callback, executed on each build target to initiate a build
static int build_target_cb(void *ctx, void *data) {
    struct build_state *state = ctx;
    struct build_opts *opts = data;

    // a simple string search means a selected target can match  multiple targets if
    // it shows up as a substring... this was not intentional but maybe a feature
    // worth keeping?
    if (state->cmdopts.targets != NULL) {
        bool target_matches = (ccstrstr(opts->target, CCSTR_LITERAL(state->cmdopts.targets)) == 0);
        if (!target_matches) {
            return 0;
        }
    }
    
    // setup per target variables
    state->target_opts = opts;
    str_list_clear(&state->main_files);
    str_list_clear(&state->obj_files);

    // ensures all paths have the correct prefixes
    tidy_pathlist(&opts->incpaths, CCSTR_LITERAL("-I"));
    tidy_pathlist(&opts->libpaths, CCSTR_LITERAL("-L"));

    // resolve command template per-target placeholders
    resolve_compile_cmd(&opts->compile, &state->cmdopts, opts);
    resolve_link_cmd(&opts->link, &state->cmdopts, opts);

    printf("\nINFO: building target '%s'\n", opts->target.cptr);
    
    // queues up all source files for compilation in threadpool
    foreach_src_file(state, opts->srcpaths, dispatch_compilation_cb);
    cc_threadpool_fenced_wait(&state->threadpool);

    // TODO: move linking to threadpool?
    if (opts->type & BIN) {
        foreach_main_file(state, link_object_files_cb);
    }

    // automatically link libs if there are no main files
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

    cc_threadpool_init(&state.threadpool, state.cmdopts.jlevel);
    foreach_target(&state, build_target_cb);

    cc_threadpool_stop_and_wait(&state.threadpool);
    return EXIT_SUCCESS;
}
