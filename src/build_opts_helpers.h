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
static int is_compiler_available(const ccstr compiler) {
    char command[128];
    char compiler_cstr[128];

    int len = ccstr_tostr(compiler_cstr, sizeof(compiler_cstr), compiler);
    if (len >= sizeof(compiler_cstr)) {
        printf("compiler name truncated: '%s'\n", compiler_cstr);
        abort();
    }
    assert(compiler_cstr[len] == 0);

    #if defined(_WIN32) || defined(_WIN64)
    snprintf(command, sizeof(command), "%s --version", compiler_cstr);
    #elif defined(_POSIX_VERSION) || defined(__linux__) || defined(__unix__)
    snprintf(command, sizeof(command), "%s --version > /dev/null 2>&1", compiler_cstr);
    #else
    #error Platform not supported.
    #endif

    return (system(command) == 0);
}

// given a pipe (|) separated list of compiler names,
// test each one until a working compiler is found
static ccstr find_compiler(const ccstr compiler_list) {
    static char compiler[128];

    ccstr listview = ccstr_view(compiler_list);
    ccstr token;

    while (token = ccstr_tokenize(&listview, '|'), token.len > 0) {
        ccstr_strip_whitespace(&token);

        if (is_compiler_available(token)) {
            return token;
        }
    }
    printf("error: no compiler found on 'path'. Check that a compiler is installed and available.\n");
    printf("       If a compiler is installed, ensure its on the path or specify an absolute path\n");
    printf("       with the option: CC\n");
    exit(1);
}

// Detects a variable in the format $(VARNAME) and returns a copy of it
// Returns a zero-length ccstr if no variable is found
static ccstr find_variable(ccstr sv) {
    if (!sv.cptr || sv.len == 0) {
        return CCSTR_EMPTY();
    }
    int start = ccstrstr(sv, CCSTR_LITERAL("$("));
    if (start == -1) {
        return CCSTR_EMPTY();
    }
    int end = ccstrstr(ccstr_offset(sv, start), CCSTR_LITERAL(")"));
    if (end == -1) {
        return CCSTR_EMPTY();
    }
    return CCSTR_VIEW(sv.cptr + start, end + 1);
}

// the build order of targets can be specified by adding
// a numeric prefix and optional '.' separator to the target
// name, which must be stripped before variable substitution
static ccstr strip_numeric_prefix(ccstr *str) {
    assert(str != NULL);
    if (str == NULL || str->len == 0 || !str->cptr) {
        return (ccstr){0};
    }
    char *endptr;
    strtoul(str->cptr, &endptr, 10);
    if (*endptr == '.') ++endptr;
    return ccstr_offset(*str, (endptr - str->cptr));
}

// given a variable name which should match one of the config options, return its value
static ccstr get_var_value(const struct option_def* optdefs, struct build_opts *opts, ccstr varname) {
    if (varname.len == 0 || !varname.cptr) {
        return (ccstr){0};
    }
    #define NAME_MATCH(varname, name) (ccstrcasecmp(varname, CCSTR_LITERAL(name)) == 0)

    // special case for TARGET: it might have a numeric prefix
    // that needs to be stripped
    if (NAME_MATCH(varname, "TARGET")) {
        return strip_numeric_prefix(&opts->target);
    }
    // general case, look for a match in the option definitions
    for (size_t i = 0; optdefs[i].name != NULL; ++i) {
        if (NAME_MATCH(varname, optdefs[i].name)) {
            // TODO: fix this ugly thing:
            return *(ccstr*)OPT_VIA_OFFSET(opts, optdefs[i].field_offset);
        }
    }
    /// no matches
    return (ccstr){0};
}

 #endif // _BUILD_OPT_HELPERS_H_