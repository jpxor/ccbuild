/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef CMD_BUILD_LINK_H
#define CMD_BUILD_LINK_H

#include "cmd.h"
#include "cmd_build_helpers.h"
#include "build_opts.h"

static
int link_object_files_cb(void *ctx, char *main_obj) {
    struct build_state *state = ctx;
    int reqsize = str_list_concat(&state->obj_files, ' ', NULL, 0);

    char objfiles[reqsize];
    str_list_concat(&state->obj_files, ' ', objfiles, reqsize);

    char all_obj_files[4096] = {0};
    snprintf(all_obj_files, sizeof(all_obj_files), "%s %s", objfiles, main_obj);

    size_t base_name_len;
    const char *base_name_ptr;
    cwk_path_get_basename(main_obj, &base_name_ptr, &base_name_len);

    char *extpos = strchr(base_name_ptr, '.');
    ccstr name = ccstr_rawlen(base_name_ptr, extpos-base_name_ptr);

    if (state->target_opts->installdir.len == 0) {
        ccstr_append(&state->target_opts->installdir, CCSTRVIEW_STATIC("/"));
    }

    const char *path_segments[] = {
        state->target_opts->install_root.cstr,
        state->target_opts->installdir.cstr,
        name.cstr,
        NULL,
    };

    char binpath[PATH_MAX] = {0};
    cwk_path_join_multiple((const char **)&path_segments, binpath, sizeof binpath);

    size_t dirname_len;
    cwk_path_get_dirname(binpath, &dirname_len);

    char tmpdirpath[PATH_MAX];
    strncpy(tmpdirpath, binpath, dirname_len-1);
    tmpdirpath[dirname_len-1] = 0;
    ccfs_mkdirp(tmpdirpath);

    ccstr_free(&name);

    ccstr command = ccstrdup(state->target_opts->link);
    ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(all_obj_files));
    ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

    printf("\nINFO: linking exec '%s'\n", binpath);
    int ret = execute_command(command);

    ccstr_free(&command);
    return ret;
}

static
int link_libs(struct build_state *state) {
    struct build_opts *bopts = state->target_opts;

    int reqsize = str_list_concat(&state->obj_files, ' ', NULL, 0);

    char objfiles[reqsize];
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

    if (bopts->installdir.len == 0) {
        ccstr_append(&bopts->installdir, CCSTRVIEW_STATIC("/"));
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
    strncpy(tmpdirpath, binpath, dirname_len-1);
    tmpdirpath[dirname_len-1] = 0;
    ccfs_mkdirp(tmpdirpath);

    int ret = 0;

    if (bopts->type & SHARED) {
        ccstr command = ccstrdup(state->target_opts->link_shared);
        ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(objfiles));
        ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

        printf("\nINFO: linking shared '%s'\n", binpath);
        ret = execute_command(command);
        ccstr_free(&command);
    }

    if (bopts->type & STATIC) {
        ccstr command = ccstrdup(state->target_opts->link_static);
        ccstr_replace(&command, ccsv_raw("[OBJS]"), ccsv_raw(objfiles));
        ccstr_replace(&command, ccsv_raw("[BINPATH]"), ccsv_raw(binpath));

        printf("\nINFO: linking static '%s'\n", binpath);
        ret = execute_command(command);
        ccstr_free(&command);
    }

    return ret;
}

#endif // CMD_BUILD_LINK_H