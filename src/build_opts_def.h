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
 #include <stddef.h>
 #include <stdio.h>
 #include <assert.h>
 #include <errno.h>

 enum optdef_flag {
    OPTDEF_NO_FLAGS = 0,
    OPTDEF_APPEND = 0b001,
    OPTDEF_CCSTRCPY = 0b010,
    OPTDEF_VAR_EXPAND = 0b100,
 };

// provides a "definition" of each build options which allows
// to declaritively define how to parse and handle each option
// and provides the infrastructure for looping over each opt
struct option_def{
    const char *name;
    void (*opt_handler)(const struct option_def *def, void *optptr, const char *key, const char *value);
    int field_offset;
    int flags;
};

 static void ccstr_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);
 static void so_version_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);
 static void type_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value);

 #define BOPT_OFFSET(name) offsetof(struct build_opts, name)

static const struct option_def build_option_defs[] = {
    {"BUILD_ROOT",   ccstr_opt_handler,      BOPT_OFFSET(build_root),   OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"INSTALL_ROOT", ccstr_opt_handler,      BOPT_OFFSET(install_root), OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"CC",           ccstr_opt_handler,      BOPT_OFFSET(cc),           OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"LIBNAME",      ccstr_opt_handler,      BOPT_OFFSET(libname),      OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"COMPILE",      ccstr_opt_handler,      BOPT_OFFSET(compile),      OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"LINK",         ccstr_opt_handler,      BOPT_OFFSET(link),         OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"LINK_SHARED",  ccstr_opt_handler,      BOPT_OFFSET(link_shared),  OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"LINK_STATIC",  ccstr_opt_handler,      BOPT_OFFSET(link_static),  OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"INSTALLDIR",   ccstr_opt_handler,      BOPT_OFFSET(installdir),   OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND},
    {"SRCPATHS",     ccstr_opt_handler,      BOPT_OFFSET(srcpaths),     OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"INCPATHS",     ccstr_opt_handler,      BOPT_OFFSET(incpaths),     OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"LIBPATHS",     ccstr_opt_handler,      BOPT_OFFSET(libpaths),     OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"CCFLAGS",      ccstr_opt_handler,      BOPT_OFFSET(ccflags),      OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"LDFLAGS",      ccstr_opt_handler,      BOPT_OFFSET(ldflags),      OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"LIBS",         ccstr_opt_handler,      BOPT_OFFSET(libs),         OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"RELEASE",      ccstr_opt_handler,      BOPT_OFFSET(release),      OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"DEBUG",        ccstr_opt_handler,      BOPT_OFFSET(debug),        OPTDEF_CCSTRCPY | OPTDEF_VAR_EXPAND | OPTDEF_APPEND},
    {"TARGET",       ccstr_opt_handler,      BOPT_OFFSET(target),       OPTDEF_NO_FLAGS},
    {"TYPE",         type_opt_handler,       BOPT_OFFSET(type),         OPTDEF_NO_FLAGS},
    {"SO_VERSION",   so_version_opt_handler, BOPT_OFFSET(so_version),   OPTDEF_NO_FLAGS},
    {NULL, NULL, 0, 0},
};

static inline
bool append_opt(const char *optname) {
    return optname[strlen(optname)-1] == '+';
}

// handles general case for parsed options
// if append is true, append value to existing opt
// if append is false, set opt to value
static void ccstr_opt_handler(const struct option_def *def, void *optptr, const char *key, const char *value) {
    ccstr sv = CCSTR_VIEW(value, strlen(value));
    if (append_opt(key)) {
        if (def->flags & OPTDEF_APPEND) {
            ccstr_append_join((ccstr*)optptr, CCSTR_LITERAL(" "), &sv, 1);
        } else {
            printf("config error: append to %s not supported.\n", def->name);
            exit(1);
        }
    } else {
        ccstrcpy((ccstr*)optptr, sv);
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
// where multiple flags can be set
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