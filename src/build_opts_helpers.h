/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

 #ifndef _BUILD_OPT_HELPERS_H_
 #define _BUILD_OPT_HELPERS_H_

 #include "libcc/cc_strings.h"
 #include "libcc/cc_trie_map.h"

// returns pointer offset by `offset` bytes
#define OPT_VIA_OFFSET(ptr, offset) (void*)((char*)(ptr) + (offset))

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
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
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

// Detects a variable in the format $(VARNAME) and returns it as a ccstrview
// Returns a zero-length ccstrview if no variable is found
static ccstrview find_variable(ccstrview sv) {
    ccstrview result = {0};

    if (!sv.cstr || sv.len == 0) {
        return result;
    }
    int start = ccstrstr(sv, CCSTRVIEW_STATIC("$("));
    if (start == -1) {
        return result;
    }
    int end = ccstrstr(ccsv_offset(sv, start), CCSTRVIEW_STATIC(")"));
    if (end == -1) {
        return result;
    }
    return (ccstrview) {
        .cstr = sv.cstr + start,
        .len = end + 1,
    };
}

// the build order of targets can be specified by adding
// a numeric prefix and optional '.' separator to the target
// name, which must be stripped before variable substitution
static ccstrview strip_numeric_prefix(ccstr *str) {
    assert(str != NULL);
    if (str == NULL || str->len == 0 || !str->cstr) {
        return (ccstrview){0};
    }
    char *endptr;
    strtoul(str->cstr, &endptr, 10);
    if (*endptr == '.') ++endptr;
    return ccsv_offset(ccsv(str), (endptr - str->cstr));
}

// given a variable name which should match one of the config options, return its value
static ccstrview get_var_value(const struct option_def* optdefs, struct build_opts *opts, ccstrview varname) {
    if (varname.len == 0 || !varname.cstr) {
        return (ccstrview){0};
    }
    #define NAME_MATCH(varname, name) (ccstrcasecmp(varname, ccsv_raw(name)) == 0)

    // special case for TARGET: it might have a numeric prefix
    // that needs to be stripped
    if (NAME_MATCH(varname, "TARGET")) {
        return strip_numeric_prefix(&opts->target);
    }
    // general case, look for a match in the option definitions
    for (size_t i = 0; optdefs[i].name != NULL; ++i) {
        if (NAME_MATCH(varname, optdefs[i].name)) {
            return ccsv(OPT_VIA_OFFSET(opts, optdefs[i].field_offset));
        }
    }
    /// no matches
    return (ccstrview){0};
}

 #endif // _BUILD_OPT_HELPERS_H_