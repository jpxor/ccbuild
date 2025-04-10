/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

 #ifndef _BUILD_OPT_HELPERS_H_
 #define _BUILD_OPT_HELPERS_H_

 #include "libcc/cc_trie_map.h"

// returns pointer offset by `offset` bytes
 #define OPT_VIA_OFFSET(ptr, offset) ((char*)(ptr) + (offset))

// iterate over all targets in the trie_map
static void foreach_target(struct cc_trie *targets, int (*callback)(void *ctx, void *data)) {
    cc_trie_iterate(targets, targets, callback);
}

// attempt to run a compiler to determine if its
// available
static int is_compiler_available(const char *compiler) {
    char command[128];

    #if defined(_WIN32) || defined(_WIN64)
    snprintf(command, sizeof(command), "%s --version >nul 2>nul", compiler);
    #elif defined(_POSIX_VERSION)
    snprintf(command, sizeof(command), "%s --version > /dev/null 2>&1", compiler);
    #else
    #error Platform not supported.
    #endif

    return (system(command) == 0);
}

// given a pipe (|) separated list of compiler names,
// test each one until a working compiler is found
static char* find_compiler(const char *compiler_list) {
    static char compiler[128];
    char temp[256];

    strncpy(temp, compiler_list, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = 0;

    char *token = strtok(temp, "|");
    while (token != NULL) {
        if (is_compiler_available(token)) {
            // skip leading whitespace
            while (*token && (*token == ' ' || *token == '\t')) token++;

            // find length to trim trailing whitespace
            size_t len = strlen(token);
            while (len > 0 && (token[len-1] == ' ' || token[len-1] == '\t')) len--;

            strncpy(compiler, token, len);
            compiler[len] = 0;
            return compiler;
        }
        token = strtok(NULL, "|");
    }
    printf("error: no compiler found on 'path'. Check that a compiler is installed and available.\n");
    printf("       If a compiler is installed, ensure its on the path or specify an absolute path\n");
    printf("       with the option: CC\n");
    exit(1);
}

 #endif // _BUILD_OPT_HELPERS_H_