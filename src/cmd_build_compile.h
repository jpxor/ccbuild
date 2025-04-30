/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef CMD_BUILD_COMPILE_H
#define CMD_BUILD_COMPILE_H

#include "cmd.h"
#include "cmd_build_helpers.h"
#include "build_opts.h"

struct srcinfo {
    char *path;
    time_t lastmodified;
    bool translation_unit;
    bool main_file;
};

// Foreach Include Directive ctx
struct fid_ctx {
    struct build_state *state;
    time_t *lastmodified;
};

struct compilation_task_ctx {
    struct build_state *state;
    ccstr srcpath;
};

static
int update_lastmodified_cb(void *ctx, const char *header) {
    struct fid_ctx *fidctx = ctx;
    time_t lastmodified;

    // TODO: check all include directories to find header
    // for now its just checking relative to project root...?
    struct srcinfo *sinfo = cc_trie_search(&fidctx->state->src_files, CC_TRIE_STR_KEY(header));
    if (sinfo == NULL) {
        lastmodified = ccfs_last_modified_time(header);
    } else {
        lastmodified = sinfo->lastmodified;
    }
    if (lastmodified == -1) {
        // printf("header not found: '%s'\n", header);
        return 0;
    }
    if (lastmodified > *fidctx->lastmodified) {
        *fidctx->lastmodified = lastmodified;
    }
    // recurse to get all includes...
    foreach_include_directive(ctx, header, update_lastmodified_cb);
    return 0;
}

static int compile_source(struct build_state *state, struct srcinfo *src) {
    assert(state != NULL);
    assert(src != NULL);

    if (!src->translation_unit) {
        return 0;
    }

    // obj files are created in the build directory following
    // the same hierarchy & name as the source files
    char objpath[PATH_MAX] = {0};
    size_t reqsize;

    reqsize = cwk_path_join(state->target_opts->build_root.cstr, src->path, objpath, sizeof objpath);
    if (reqsize >= sizeof objpath) {
        printf("%s: cwk_path_join failed\n", __func__);
        abort();
    }

    reqsize = cwk_path_change_extension(objpath, ".o", objpath, sizeof objpath);
    if (reqsize >= sizeof objpath) {
        printf("%s: cwk_path_change_extension failed\n", __func__);
        abort();
    }

    if (src->main_file) {
        str_list_new_node(&state->main_files, objpath);
    } else {
        str_list_new_node(&state->obj_files, objpath);
    }

    time_t objlastmodified = ccfs_last_modified_time(objpath);
    if (objlastmodified > src->lastmodified && objlastmodified > state->target_opts->lastmodified) {
        // TOOD: look into using file hashes to identify changes?
        // not sure if this would be faster/slower than just a quick
        // rebuild...
        return 0;
    }
    if (objlastmodified == -1) {
        // obj does not exist, create full path in case dir structure
        // also does not exist
        size_t dirname_size;
        cwk_path_get_dirname(objpath, &dirname_size);

        char tmpdirpath[PATH_MAX];
        strncpy(tmpdirpath, objpath, dirname_size-1);
        tmpdirpath[dirname_size-1] = 0;
        ccfs_mkdirp(tmpdirpath);
    }

    ccstr command = ccstrdup(state->target_opts->compile);
    ccstr_replace(&command, CCSTRVIEW_STATIC("[OBJPATH]"), ccsv_raw(objpath));
    ccstr_replace(&command, CCSTRVIEW_STATIC("[SRCPATH]"), ccsv_raw(src->path));
    int ret = execute_command(command);

    ccstr_free(&command);
    return ret;
}

static void compile_translation_unit_cb(void*ctx) {
    struct compilation_task_ctx taskctx = *(struct compilation_task_ctx*)ctx;
    struct build_state *state = taskctx.state;
    const char *filepath = taskctx.srcpath.cstr;

    // filter by file type
    const char *ext = NULL;
    size_t extlen = 0;

    if (!cwk_path_get_extension(filepath, &ext, &extlen)){
        return; // no extension, skip
    }

    bool cext = strcmp(ext, ".c") == 0 || strcmp(ext, ".C") == 0
                || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0;

    if (!cext) {
        return; // not source file, skip
    }

    // get path relative to project root
    char relpath[PATH_MAX];
    if (!cwk_path_is_relative(filepath)) {
        size_t reqsize = cwk_path_get_relative(state->rootdir.cstr, filepath, relpath, sizeof relpath);
        if (reqsize >= sizeof relpath) {
            printf("%s: cwk_path_get_relative failed\n", __func__);
            abort();
        }
    } else {
        strcpy(relpath, filepath);
    }

    struct srcinfo src_info = {
        .translation_unit = cext,
        .lastmodified = ccfs_last_modified_time(relpath),
        .main_file = has_entry_point(relpath),
        .path = relpath,
    };

    // get lastmodified time from all included headers as well
    struct fid_ctx fidctx = {
        .state = state,
        .lastmodified = &src_info.lastmodified,
    };
    foreach_include_directive(&fidctx, relpath, update_lastmodified_cb);
    compile_source(state, &src_info);
}

static int dispatch_compilation_cb(void *ctx, const char *srcpath) {
    struct build_state *state = ctx;

    // TODO: get memory from pool allocator (make per-target arena allocator)
    struct compilation_task_ctx *taskctx = calloc(1, sizeof*taskctx);
    taskctx->state = state;
    ccstrcpy_raw(&taskctx->srcpath, srcpath);
    cc_threadpool_submit(&state->threadpool, taskctx, compile_translation_unit_cb);
    return 0;
}

#endif // CMD_BUILD_COMPILE_H