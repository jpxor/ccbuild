/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef CMD_BUILD_HELPERS_H
#define CMD_BUILD_HELPERS_H

#include "cmd.h"
#include "str_list.h"

#include "libcc/cc_strings.h"
#include "libcc/cc_files.h"

#include <stdio.h>

// iterate over all targets in the trie_map
static void foreach_target(struct build_state *state, int (*callback)(void *ctx, void *data)) {
    cc_trie_iterate(&state->optsmap, state, callback);
}

// iterate over the list of files with entry-points
static void foreach_main_file(struct build_state *state, int (*callback)(void *ctx, char *str)) {
    str_list_iterate(&state->main_files, state, callback);
}

// iterate over all files found in the SRCPATHS directory paths list
static int foreach_src_file(struct build_state *state, ccstr srcpaths, int (*callback)(void *ctx, const char *data)) {
    ccstr sv = srcpaths;
    ccstr path;

    while (sv.len > 0) {
        path = ccstr_tokenize(&sv, ' ');

        char pathstr[PATH_MAX] = {0};
        memcpy(pathstr, path.cptr, path.len);
        if (ccfs_iterate_files(pathstr, state, callback) == -1) {
            return -1;
        }
    }
    return 0;
}

// iterate over all files found included in a source file
static void foreach_include_directive(void *ctx, const char *srcpath, int (*callback)(void *ctx, const char *header)) {
    FILE *file = fopen(srcpath, "r");
    if (!file) {
        // failure here could simply mean the header is from outside
        // the project (systems includes). We'll skip these, assuming
        // they wont change (often)
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file)) {

        if (strncmp(line, "#include", 8) == 0) {
            // get the included file name
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

// detect if a source file has an entry point (main function)
static bool has_entry_point(const char *filename) {
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

static int execute_command(ccstr command) {
    printf("%s\n", command.cptr);
    return system(command.cptr);
}

#endif // CMD_BUILD_HELPERS_H