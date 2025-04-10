/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

 #ifndef _BUILD_OPT_DEFS_H_
 #define _BUILD_OPT_DEFS_H_

 #include <stdbool.h>
 #include <stdio.h>
 #include <assert.h>
 #include <errno.h>

// provides a "definition" of each build options which allows
// to declaritively define how to parse and handle each option
// and provides the infrastructure for looping over each opt
struct option_def{
    const char *name;
    bool supports_append;
    bool ccstr_init_cp;
    int field_offset;
    void (*opt_handler)(const struct option_def *def, void *optptr, const char *key, const char *value);
};

 static void general_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);
 static void so_version_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);
 static void type_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);

 #define YES_APPEND   true
 #define YES_CCSTRCPY true
 #define NO_APPEND    false
 #define NO_CCSTRCPY  false
 #define BOPT_OFFSET(name) offsetof(struct build_opts, name)
 
static const struct option_def build_option_defs[] = {
    {"BUILD_ROOT",   NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(build_root), general_opt_handler},
    {"INSTALL_ROOT", NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(install_root), general_opt_handler},
    {"CC",           NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(cc), general_opt_handler},
    {"LIBNAME",      NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(libname), general_opt_handler},
    {"COMPILE",      NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(compile), general_opt_handler},
    {"LINK",         NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(link), general_opt_handler},
    {"LINK_SHARED",  NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(link_shared), general_opt_handler},
    {"LINK_STATIC",  NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(link_static), general_opt_handler},
    {"INSTALL_DIR",  NO_APPEND, YES_CCSTRCPY, BOPT_OFFSET(installdir), general_opt_handler},
    {"SRC_PATHS",   YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(srcpaths), general_opt_handler},
    {"INC_PATHS",   YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(incpaths), general_opt_handler},
    {"LIB_PATHS",   YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(libpaths), general_opt_handler},
    {"CCFLAGS",     YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(ccflags), general_opt_handler},
    {"LDFLAGS",     YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(ldflags), general_opt_handler},
    {"LIBS",        YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(libs), general_opt_handler},
    {"RELEASE",     YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(release), general_opt_handler},
    {"DEBUG",       YES_APPEND, YES_CCSTRCPY, BOPT_OFFSET(debug), general_opt_handler},
    {"TARGET",       NO_APPEND,  NO_CCSTRCPY, BOPT_OFFSET(target), general_opt_handler},
    {"TYPE",         NO_APPEND,  NO_CCSTRCPY, BOPT_OFFSET(type), type_opt_handler},
    {"SO_VERSION",   NO_APPEND,  NO_CCSTRCPY, BOPT_OFFSET(so_version), so_version_opt_handler},
    {NULL, 0, 0, 0, NULL}
};

static inline
bool append_opt(const char *optname) {
    return optname[strlen(optname)-1] == '+';
}

// handles general case for parsed options
// if append is true, append value to existing opt
// if append is false, set opt to value
static void general_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value) {
    if (append_opt(key)) {
        if (def->supports_append) {
            ccstrcat((ccstr*)optptr, " ", value);
        } else {
            printf("config error: append to %s not supported.\n", def->name);
            exit(1);
        }
    } else {
        ccstrcpy_raw((ccstr*)optptr, value);
    }
};

// so_version needs to be parsed as an int
static void so_version_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value) {
    assert(def->field_offset == offsetof(struct build_opts, so_version));

    if (append_opt(key)) {
        printf("config error: append to %s not supported.\n", def->name);
        exit(1);
    }
    char *endptr;
    int so_version = strtoul(value, &endptr, 10);
    if (errno == ERANGE) {
        printf("config error: SO_VERSION overflow or underflow.\n");
        exit(1);
    }
    if (*endptr != 0 || endptr == value) {
        printf("config error: SO_VERSION not a valid number: %s\n", value);
        exit(1);
    }
    int *opt = optptr;
    *opt = so_version;
}

// type needs to be parsed as a bitfield
// with multiple flags possiblly set
static void type_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value) {
    assert(def->field_offset == offsetof(struct build_opts, type));
    (void)def;

    unsigned type = 0;
    if (strstr(value, "bin")) {
        type |= BIN;
    }
    if (strstr(value, "shared")) {
        type |= SHARED;
    }
    if (strstr(value, "static")) {
        type |= STATIC;
    }
    if (strstr(value, "lib")) {
        type |= SHARED|STATIC;
    }
    enum target_type *opt = optptr;
    if (append_opt(key)) {
        *opt |= type;
    } else {
        *opt = type;
    }
    if (*opt == 0) {
        printf("config error: invalid TYPE: %s\n", value);
        printf("            : options: bin, shared, static\n");
        exit(1);
    }
}

 #endif // _BUILD_OPT_DEFS_H_