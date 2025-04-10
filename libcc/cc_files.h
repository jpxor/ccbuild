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

time_t ccfs_last_modified_time(const char *file_path);

#endif // _CC_FILES_H

#ifdef CC_FILES_IMPLEMENTATION

#include <sys/stat.h>

time_t ccfs_last_modified_time(const char *file_path) {
    struct stat st;
    if (stat(file_path, &st) == -1) {
        return -1;
    }
    return st.st_mtime;
}

#endif // CC_FILES_IMPLEMENTATION