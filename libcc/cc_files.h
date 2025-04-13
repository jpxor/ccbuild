/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_FILES_H
#define _CC_FILES_H

#include "time.h"

time_t ccfs_last_modified_time(const char *filepath);
int ccfs_iterate_files(const char *directory, void *ctx, int (*callback)(void *ctx, const char *filepath));

#endif // _CC_FILES_H

#ifdef CC_FILES_IMPLEMENTATION

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

time_t ccfs_last_modified_time(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        return -1;
    }
    return st.st_mtime;
}

int ccfs_iterate_files(const char *directory, void *ctx, int (*callback)(void *ctx, const char *filepath)) {
    DIR *dir = opendir(directory);
    if (!dir) {
        if (errno != ENOENT) {
            printf("Error: Unable to open directory %s\n", directory);
        }
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char filepath[PATH_MAX];
        int reqsize = snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);

        if (reqsize >= sizeof(filepath)) {
            printf("Error: Filepath too long\n");
            printf("%s/%s\n", directory, entry->d_name);
            closedir(dir);
            return -1;
        }

        if (is_regular_file(filepath)) {
            if (callback(ctx, filepath) == -1) {
                closedir(dir);
                return -1;
            }

        } else if (is_directory(filepath)) {
            if (iterate_files(filepath, ctx, callback) == -1) {
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    return 0;
}

#endif // CC_FILES_IMPLEMENTATION