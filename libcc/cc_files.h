/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_FILES_H
#define _CC_FILES_H

#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

time_t ccfs_last_modified_time(const char *filepath);
bool ccfs_is_regular_file(const char *path);
bool ccfs_is_directory(const char *path);
int ccfs_cwd(char *outp, size_t bufsize);
int ccfs_mkdirp(const char *filename);
int ccfs_chdir(const char *path);
int ccfs_rmdir_recursive(const char *path);

int ccfs_iterate_files(const char *directory, void *ctx, int (*callback)(void *ctx, const char *filepath));

#endif // _CC_FILES_H

#ifdef CC_FILES_IMPLEMENTATION

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

// set default logging
#ifndef CC_LOGF
#include <stdio.h>
#define CC_LOGF printf
#endif

#ifdef _WIN32
#include <windows.h>
#endif

time_t ccfs_last_modified_time(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        return -1;
    }
    return st.st_mtime;
}

bool ccfs_is_directory(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return false; // Error occurred
    }
    return S_ISDIR(path_stat.st_mode);
}

bool ccfs_is_regular_file(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return false; // Error accessing the file stats
    }
    return S_ISREG(path_stat.st_mode);
}

static int iterate_dirent(DIR *dir, const char *dirname, void *ctx, int (*callback)(void *ctx, const char *filepath)) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char filepath[PATH_MAX];
        int reqsize = snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);

        if (reqsize >= (int)sizeof(filepath)) {
            CC_LOGF("Error: Filepath too long\n");
            CC_LOGF("%s/%s\n", dirname, entry->d_name);
            return -1;
        }

        if (ccfs_is_regular_file(filepath)) {
            if (callback(ctx, filepath) == -1) {
                return -1;
            }

        } else if (ccfs_is_directory(filepath)) {
            if (ccfs_iterate_files(filepath, ctx, callback) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int ccfs_iterate_files(const char *directory, void *ctx, int (*callback)(void *ctx, const char *filepath)) {
    DIR *dir = opendir(directory);
    if (!dir) {
        CC_LOGF("Error: Unable to open directory %s, %s\n", directory, strerror(errno));
        return -1;
    }
    int ret = iterate_dirent(dir, directory, ctx, callback);
    closedir(dir);
    return ret;
}

int ccfs_cwd(char *outp, size_t bufsize) {
    if (outp == NULL) {
        return EINVAL;
    }
    char cwd[PATH_MAX];
    
    #if defined(_WIN32) || defined(_WIN64)
    if (GetCurrentDirectoryA(PATH_MAX, cwd) == 0) {
        return GetLastError();
    }
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return errno;
    }
    #else
    #error "Platform not supported"
    #endif

    int reqlen = snprintf(outp, bufsize, "%s", cwd);
    if (reqlen >= (int)bufsize) {
        return EINVAL;
    }
    return 0;
}

int ccfs_chdir(const char *path) {
    #if defined(_WIN32) || defined(_WIN64)
    if (SetCurrentDirectoryA(path) == 0) {
        return GetLastError();
    }
    return 0;
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
    return chdir(path);
    #else
    #error "Platform not supported"
    #endif
}

bool create_directory(const char *path) {
    #ifdef _WIN32
        return CreateDirectory(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
        return mkdir(path, 0755) == 0 || errno == EEXIST;
    #else
        #error "platform not supported."
    #endif
}

int delete_empty_directory(const char *path) {
    #ifdef _WIN32
        if (RemoveDirectory(path)) {
            return 0;
        } else {
            CC_LOGF("failed to delete directory: %s\n", path);
            return -1;
        }
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
        if (rmdir(path) == 0) {
            return 0;
        } else {
            CC_LOGF("failed to delete directory: %s\n", path);
            return -1;
        }
    #else
        #error "platform not supported."
    #endif
}

int delete_file(const char *path) {
    #ifdef _WIN32
        if (DeleteFile(path)) {
            return 0;
        } else {
            CC_LOGF("failed to delete file: %s\n", path);
            return -1;
        }
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
        if (unlink(path) == 0) {
            return 0;
        } else {
            perror("Failed to delete file");
            return -1;
        }
    #else
        #error "platform not supported."
    #endif
}

int ccfs_mkdirp(const char *filename) {
    if (filename==NULL) {
        return EINVAL;
    }
    char dirpath[PATH_MAX];
    snprintf(dirpath, sizeof dirpath, "%s", filename);

    for (char *p = dirpath + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            create_directory(dirpath);
            *p = '/';
        }
    }
    if (!create_directory(dirpath)) {
        return -1;
    }
    return 0;
}

int ccfs_rmdir_recursive(const char *directory) {
    DIR *dir = opendir(directory);
    if (!dir) {
        CC_LOGF("Error: Unable to open directory %s, %s\n", directory, strerror(errno));
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char filepath[PATH_MAX];
        int reqsize = snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);

        if (reqsize >= (int)sizeof(filepath)) {
            CC_LOGF("Error: Filepath too long\n");
            CC_LOGF("%s/%s\n", directory, entry->d_name);
            closedir(dir);
            return -1;
        }

        if (ccfs_is_regular_file(filepath)) {
            delete_file(filepath);

        } else if (ccfs_is_directory(filepath)) {
            ccfs_rmdir_recursive(filepath);
            delete_empty_directory(filepath);
        }
    }
    closedir(dir);
    return 0;
}

#endif // CC_FILES_IMPLEMENTATION