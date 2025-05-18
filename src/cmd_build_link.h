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
    ccstr name = CCSTR_VIEW(base_name_ptr, extpos-base_name_ptr);

    if (state->target_opts->installdir.len == 0) {
        ccstr_append(&state->target_opts->installdir, CCSTR_LITERAL("/"));
    }

    const char *path_segments[] = {
        state->target_opts->install_root.cptr,
        state->target_opts->installdir.cptr,
        name.cptr,
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

    ccstr ccstr_all_obj_files = CCSTR_VIEW(all_obj_files, strlen(all_obj_files));
    ccstr ccstr_binpath = CCSTR_VIEW(binpath, strlen(binpath));

    int reqlen = state->target_opts->link.len + ccstr_all_obj_files.len + ccstr_binpath.len;
    char command_buf[reqlen+1];

    ccstr command = CCSTR(command_buf, 0, sizeof(command_buf));
    ccstrcpy(&command, state->target_opts->link);

    ccstr_replace(&command, CCSTR_LITERAL("[OBJS]"), ccstr_all_obj_files);
    ccstr_replace(&command, CCSTR_LITERAL("[BINPATH]"), ccstr_binpath);

    printf("\nINFO: linking exec '%s'\n", binpath);
    return execute_command(command);
}

static
int link_libs(struct build_state *state) {
    struct build_opts *bopts = state->target_opts;

    int reqsize = str_list_concat(&state->obj_files, ' ', NULL, 0);

    char objfiles[reqsize];
    str_list_concat(&state->obj_files, ' ', objfiles, reqsize);

    if (bopts->libname.len == 0) {
        ccstr_append(&bopts->libname, bopts->target);
    }

    if (strstr(bopts->libname.cptr, "lib") != bopts->libname.cptr) {
        ccstr tmp = CCSTR_LITERAL("lib");
        ccstr_append(&tmp, bopts->libname);
        ccstrcpy(&bopts->libname, tmp);
    }

    if (bopts->installdir.len == 0) {
        ccstr_append(&bopts->installdir, CCSTR_LITERAL("/"));
    }

    const char *path_segments[] = {
        bopts->install_root.cptr,
        bopts->installdir.cptr,
        bopts->libname.cptr,
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

    ccstr ccstr_objfiles = CCSTR_VIEW(objfiles, strlen(objfiles));
    ccstr ccstr_binpath = CCSTR_VIEW(binpath, strlen(binpath));

    int reqlen = state->target_opts->link_shared.len;
    if (reqlen < state->target_opts->link_static.len) {
        reqlen = state->target_opts->link_static.len;
    }
    reqlen += ccstr_objfiles.len + ccstr_binpath.len;
    char command_buf[reqlen+1];

    if (bopts->type & SHARED) {
        
        ccstr command = CCSTR(command_buf, 0, sizeof(command_buf));
        ccstrcpy(&command, state->target_opts->link_shared);

        ccstr_replace(&command, CCSTR_LITERAL("[OBJS]"), ccstr_objfiles);
        ccstr_replace(&command, CCSTR_LITERAL("[BINPATH]"), ccstr_binpath);

        printf("\nINFO: linking shared '%s'\n", binpath);
        ret = execute_command(command);
    }

    if (bopts->type & STATIC) {

        ccstr command = CCSTR(command_buf, 0, sizeof(command_buf));
        ccstrcpy(&command, state->target_opts->link_static);

        ccstr_replace(&command, CCSTR_LITERAL("[OBJS]"), CCSTR_LITERAL(objfiles));
        ccstr_replace(&command, CCSTR_LITERAL("[BINPATH]"), CCSTR_LITERAL(binpath));

        printf("\nINFO: linking static '%s'\n", binpath);
        ret = execute_command(command);
    }

    return ret;
}

#endif // CMD_BUILD_LINK_H