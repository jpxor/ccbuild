/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include "cmd_build.h"
#include "build_opts.h"

#include "libcc/cc_threadpool.h"
#include "libcc/cc_trie_map.h"
#include "libcc/cc_files.h"

#include "vendor/cwalk/cwalk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#ifndef EOK
#define EOK 0
#endif

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

static
int compile_translation_unit_cb(void *ctx, void *data);

static
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
    return EOK;
}


#include <stdio.h> // FILE
static
void foreach_include_directive(void *ctx, const char *srcpath, int (*callback)(void *ctx, const char *header)) {
    FILE *file = fopen(srcpath, "r");
    if (!file) {
        // failure here could simply mean the header is from outside
        // the project (extra includes). We'll skip these for now, assuming
        // they wont change (often)
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file)) {

        if (strncmp(line, "#include", 8) == 0) {
            // get the header file name
            char *start = strchr(line, '<');
            if (!start) {
                start = strchr(line, '"');
            }
            if (start) {
                char *end = strchr(start + 1, (start[0] == '<') ? '>' : '"');
                if (end) {
                    *end = '\0';
                    callback(ctx, start+1);
                }
            }
        }
    }
    fclose(file);
}

static
bool has_entry_point(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open file");
        return false;
    }
    char line[1024];
    bool in_string = false;
    bool in_comment = false;
    bool in_multiline_comment = false;
    while (fgets(line, sizeof(line), file)) {
        for (int i = 0; line[i] != '\0'; i++) {
            if (!in_multiline_comment && !in_comment && line[i] == '\"' && (i == 0 || line[i-1] != '\\')) {
                in_string = !in_string;
            }
            if (!in_string && !in_multiline_comment && line[i] == '/' && line[i+1] == '/') {
                in_comment = true;
                break;
            }
            if (!in_string && !in_multiline_comment && line[i] == '/' && line[i+1] == '*') {
                in_multiline_comment = true;
                i++;
                continue;
            }
            if (!in_string && in_multiline_comment && line[i] == '*' && line[i+1] == '/') {
                in_multiline_comment = false;
                i++;
                continue;
            }
            if (!in_string && !in_comment && !in_multiline_comment && strncmp(line + i, "int main(", 9) == 0) {
                fclose(file);
                return true;
            }
        }
        in_comment = false;
    }
    fclose(file);
    return false;
}

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

static
int link_object_files_cb(void *ctx, char *main_obj) {
    struct build_state *state = ctx;
    int reqsize = str_list_concat(&state->obj_files, ' ', NULL, 0);

    char *objfiles = alloca(reqsize);
    str_list_concat(&state->obj_files, ' ', objfiles, reqsize);

    char all_obj_files[4096] = {0};
    snprintf(all_obj_files, sizeof(all_obj_files), "%s %s", objfiles, main_obj);

    size_t base_name_len;
    const char *base_name_ptr;
    cwk_path_get_basename(main_obj, &base_name_ptr, &base_name_len);

    char *extpos = strchr(base_name_ptr, '.');
    ccstr name = ccstr_rawlen(base_name_ptr, extpos-base_name_ptr);

    const char *path_segments[] = {
        state->target_build_opts->install_root.cstr,
        state->target_build_opts->installdir.cstr,
        name.cstr,
        NULL,
    };

    char binpath[PATH_MAX] = {0};
    cwk_path_join_multiple((const char **)&path_segments, binpath, sizeof binpath);

    size_t dirname_len;
    cwk_path_get_dirname(binpath, &dirname_len);

    char tmpdirpath[PATH_MAX];
    strncpy(tmpdirpath, binpath, dirname_len);
    ccfs_mkdirp(tmpdirpath);

    ccstr_free(&name);

    ccstr command = ccstrdup(state->target_build_opts->link);
    ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(all_obj_files));
    ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

    printf("\nINFO: linking exec '%s'\n", binpath);
    printf("%s\n", command.cstr);
    return system(command.cstr);
}

static
int link_libs(struct build_state *state) {
    struct build_opts *bopts = state->target_build_opts;

    int reqsize = str_list_concat(&state->obj_files, ' ', NULL, 0);

    char *objfiles = alloca(reqsize);
    str_list_concat(&state->obj_files, ' ', objfiles, reqsize);

    if (bopts->libname.len == 0) {
        ccstr_append(&bopts->libname, ccsv(&bopts->target));
    }

    if (strstr(bopts->libname.cstr, "lib") != bopts->libname.cstr) {
        ccstr tmp = CCSTR_STATIC("lib");
        ccstr_append(&tmp, ccsv(&bopts->libname));
        ccstrcpy(&bopts->libname, tmp);
        ccstr_free(&tmp);
    }

    const char *path_segments[] = {
        bopts->install_root.cstr,
        bopts->installdir.cstr,
        bopts->libname.cstr,
        NULL,
    };

    char binpath[PATH_MAX] = {0};
    cwk_path_join_multiple((const char **)&path_segments, binpath, sizeof binpath);

    size_t dirname_len;
    cwk_path_get_dirname(binpath, &dirname_len);

    char tmpdirpath[PATH_MAX];
    strncpy(tmpdirpath, binpath, dirname_len);
    ccfs_mkdirp(tmpdirpath);

    int ret = EOK;

    if (bopts->type & SHARED) {
        ccstr command = ccstrdup(state->target_build_opts->link_shared);
        ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(objfiles));
        ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

        printf("\nINFO: linking shared '%s'\n", binpath);
        printf("%s\n", command.cstr);
        ret = system(command.cstr);
    }

    if (bopts->type & STATIC) {
        ccstr command = ccstrdup(state->target_build_opts->link_static);
        ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(objfiles));
        ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

        printf("\nINFO: linking static '%s'\n", binpath);
        printf("%s\n", command.cstr);
        ret = system(command.cstr);
    }

    return ret;
}

struct compilation_task_ctx {
    struct build_state *state;
    ccstr srcpath;
};

static
void compile_translation_unit_cbv2(void*ctx) {
    struct compilation_task_ctx taskctx = *(struct compilation_task_ctx*)ctx;
    struct build_state *state = taskctx.state;
    const char *filepath = taskctx.srcpath.cstr;

    // filter by file type
    const char *ext = NULL;
    size_t extlen = 0;

    if (!cwk_path_get_extension(filepath, &ext, &extlen)){
        return; // no extension, skip
    }

    bool cext = strcmp(ext, ".c") == 0
                || strcmp(ext, ".cpp") == 0
                || strcmp(ext, ".cc") == 0;

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

    struct srcinfo info = {
        .translation_unit = cext,
        .lastmodified = ccfs_last_modified_time(relpath),
        .main_file = has_entry_point(relpath),
        .path = relpath,
    };

    // get lastmodified time from all included headers as well
    struct fid_ctx fidctx = {
        .state = state,
        .lastmodified = &info.lastmodified,
    };
    foreach_include_directive(&fidctx, relpath, update_lastmodified_cb);

    compile_translation_unit_cb(state, &info);
}

static
int dispatch_compilation_cb(void *ctx, const char *srcpath) {
    struct build_state *state = ctx;

    // TODO: get memory from pool allocator (built into threadpool?)
    struct compilation_task_ctx *taskctx = calloc(1, sizeof*taskctx);
    taskctx->state = state;
    ccstrcpy_raw(&taskctx->srcpath, srcpath);
    cc_threadpool_submit(&state->threadpool, taskctx, compile_translation_unit_cbv2);
    return EOK;
}

static
int foreach_src_file(struct build_state *state, ccstr srcpaths, int (*callback)(void *ctx, const char *data)) {
    ccstrview sv = ccsv(&srcpaths);
    ccstrview path;

    while (sv.len > 0) {
        path = ccsv_tokenize(&sv, ' ');

        char pathstr[PATH_MAX];
        memcpy(pathstr, path.cstr, path.len);
        if (ccfs_iterate_files(pathstr, state, callback) == -1) {
            return -1;
        }
    }
    return 0;
}

static
void foreach_main_file(struct build_state *state, int (*callback)(void *ctx, char *str)) {
    str_list_iterate(&state->main_files, state, callback);
}

static
void foreach_target(struct build_state *state, int (*callback)(void *ctx, void *data)) {
    cc_trie_iterate(&state->optsmap, state, callback);
}

static
int compile_translation_unit_cb(void *ctx, void *data) {
    struct build_state *state = ctx;
    struct srcinfo *src = data;

    if (!src->translation_unit) {
        return 0;
    }

    // obj files are created in the build directory following
    // the same hierarchy & name as the source files
    char objpath[PATH_MAX] = {0};
    size_t reqsize;

    reqsize = cwk_path_join(state->target_build_opts->build_root.cstr, src->path, objpath, sizeof objpath);
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
    if (objlastmodified > src->lastmodified && objlastmodified > state->target_build_opts->lastmodified) {
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
        strncpy(tmpdirpath, objpath, dirname_size);
        ccfs_mkdirp(tmpdirpath);

    }

    // needs free?
    ccstr command = ccstrdup(state->target_build_opts->compile);
    ccstr_replace(&command, CCSTRVIEW_STATIC("[OBJPATH]"), ccsv_raw(objpath));
    ccstr_replace(&command, CCSTRVIEW_STATIC("[SRCPATH]"), ccsv_raw(src->path));

    printf("%s\n", command.cstr);
    return system(command.cstr);
}

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

static void resolve_compile_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    if (cmdopts->release) {
        ccstr_replace(cmd, ccsv_raw("[DEBUG_OR_RELEASE]"), ccsv(&opts->release));
    } else {
        ccstr_replace(cmd, ccsv_raw("[DEBUG_OR_RELEASE]"), ccsv(&opts->debug));
    }
    ccstr_replace(cmd, ccsv_raw("-I[INCPATHS]"), ccsv(&opts->incpaths));
}

static void resolve_link_cmd(ccstr *cmd, struct cmdopts *cmdopts, struct build_opts *opts) {
    (void)cmdopts;
    ccstr_replace(cmd, ccsv_raw("-L[LIBPATHS]"), ccsv(&opts->libpaths));
}

static
int build_target_cb(void *ctx, void *data) {
    struct build_state *state = ctx;
    struct build_opts *opts = data;

    bool skip = (state->cmdopts.targets != NULL)
             && (ccstrstr(ccsv_raw(state->cmdopts.targets), ccsv(&opts->target)) == -1);

    if (skip) {
        return 0;
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


// CLEAN ////////////////////////////////////////////////////////////

// int delete_object_files_cb(void *ctx, const char *filepath) {
//     const char *ext = NULL;
//     size_t extlen = 0;
//     (void)ctx;

//     if (cwk_path_get_extension(filepath, &ext, &extlen)
//     && strcmp(ext, ".o") == 0) {
//         printf("rm %s\n", filepath);
//         futils_rm(filepath);
//     }
//     return 0;
// }

// int delete_object_files(const char *dir) {
//     iterate_files(dir, NULL, delete_object_files_cb);
//     return EXIT_SUCCESS;
// }

int cc_clean(struct cmdopts *cmdopts) {
    (void)cmdopts;
    // struct build_state state = {
    //     .cmdopts = *cmdopts,
    // };
    // int err = set_root_and_build_paths(&state);
    // if (err) return EXIT_FAILURE;

    // return delete_object_files(state.buildir);
    return -1;
}