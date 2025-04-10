/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include "build_opts.h"
#include "build_opts_def.h"

#include "libcc/cc_files.h"
#include "libcc/cc_strings.h"

#include "vendor/inih/ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PTR_OFFSET(ptr, offset) ((char*)(ptr) + (offset))

// will find compiler if its on the PATH
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

// global build options allocator
static struct cc_arena *g_opts_allocator = NULL;

// global default build options
static struct build_opts g_default_bopts = {
    .type = BIN,
    .so_version = 0,
    .lastmodified = 0,
    .target = CCSTR_STATIC(""),
    .cc = CCSTR_STATIC(""),
    .libname = CCSTR_STATIC("$(TARGET)"),
    .build_root = CCSTR_STATIC("./build/$(TARGET)/"),
    .install_root = CCSTR_STATIC("./install/$(TARGET)/"),
    .installdir = CCSTR_STATIC(""),
    .srcpaths = CCSTR_STATIC(". ./src"),
    .incpaths = CCSTR_STATIC(". ./includes"),
    .libpaths = CCSTR_STATIC("$(INSTALL_ROOT)/$(TARGET)"),
    .libs = CCSTR_STATIC(""),
    .ccflags = CCSTR_STATIC("-Wall -Wextra"),
    .ldflags = CCSTR_STATIC(""),
    .release = CCSTR_STATIC("-O2 -DNDEBUG -Werror"),
    .debug = CCSTR_STATIC("-g -O0 -D_FORTIFY_SOURCE=2"),
    .compile = CCSTR_STATIC("$(CC) $(CCFLAGS) [DEBUG_OR_RELEASE] -I[INCPATHS] -o [OBJPATH] -c [SRCPATH]"),
    .link = CCSTR_STATIC("$(CC) $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH]"),
    .link_static = CCSTR_STATIC("ar rcs [BINPATH].a [OBJS]"),
    .link_shared = CCSTR_STATIC("$(CC) -shared -fPIC $(LDFLAGS) [OBJS] -L[LIBPATHS] $(LIBS) -o [BINPATH].so"),
};

// init new target opts by copying global default opts
void init_opts(struct build_opts *opts, const char *name) {
    ccstrcpy_raw(&opts->target, name);

    ccstrcpy(&opts->cc, g_default_bopts.cc);
    ccstrcpy(&opts->cc, g_default_bopts.cc);
    ccstrcpy(&opts->build_root, g_default_bopts.build_root);
    ccstrcpy(&opts->install_root, g_default_bopts.install_root);
    ccstrcpy(&opts->srcpaths, g_default_bopts.srcpaths);
    ccstrcpy(&opts->incpaths, g_default_bopts.incpaths);
    ccstrcpy(&opts->libpaths, g_default_bopts.libpaths);
    ccstrcpy(&opts->installdir, g_default_bopts.installdir);
    ccstrcpy(&opts->ccflags, g_default_bopts.ccflags);
    ccstrcpy(&opts->ldflags, g_default_bopts.ldflags);
    ccstrcpy(&opts->libs, g_default_bopts.libs);
    ccstrcpy(&opts->release, g_default_bopts.release);
    ccstrcpy(&opts->debug, g_default_bopts.debug);
    ccstrcpy(&opts->compile, g_default_bopts.compile);
    ccstrcpy(&opts->link, g_default_bopts.link);
    ccstrcpy(&opts->link_shared, g_default_bopts.link_shared);
    ccstrcpy(&opts->link_static, g_default_bopts.link_static);
    ccstrcpy(&opts->libname, g_default_bopts.libname);

    opts->type = g_default_bopts.type;
    opts->so_version = g_default_bopts.so_version;
    opts->lastmodified = g_default_bopts.lastmodified;
}

static inline
bool append_opt(const char *optname) {
    return optname[strlen(optname)-1] == '+';
}

static inline
bool match_opt(const char *opt, const char *key) {
    size_t optlen = strlen(opt);
    return strncasecmp(opt, key, optlen) == 0
        // key is at least as long as opt
        && (key[optlen] == 0 || key[optlen] == ' ' || key[optlen] == '+');
}

static
void resolve_default_cc(void) {
    if (g_default_bopts.cc.len == 0) {
        ccstrcpy_raw(&g_default_bopts.cc, find_compiler("gcc|clang|cl"));

    } else if (ccstrchr(ccsv(&g_default_bopts.cc), '|')) {
        ccstrcpy_raw(&g_default_bopts.cc, find_compiler(g_default_bopts.cc.cstr));
    }
}

int parse_opts_cb(void* ctx, const char* target, const char* key, const char* value) {
    struct cc_trie *target_opts_map = ctx;
    struct build_opts *target_opts = NULL;

    // parser detected new [section], which means new target
    if (key == NULL && value == NULL) {

        // resolve the default CC option before creating target opts
        // based on the default
        resolve_default_cc();

        struct build_opts *target_opts = cc_alloc(target_opts_map->arena, sizeof *target_opts);
        init_opts(target_opts, target);

        cc_trie_insert(target_opts_map, CC_TRIE_STR_KEY(target), target_opts);
        return 0;
    }

    if (strlen(target) == 0) {
        target = "default";
        target_opts = &g_default_bopts;
    } else {
        target_opts = cc_trie_search(target_opts_map, CC_TRIE_STR_KEY(target));
        assert(target_opts != NULL);
    }

    // handles the parsed string, setting or appending the value
    // into the correct target_opts build option field
    for (int i = 0; build_option_defs[i].name != NULL; i++) {
        const struct option_def *def = &build_option_defs[i];
        if (match_opt(def->name, key)) {
            void *opt = (void*)PTR_OFFSET(target_opts, def->field_offset);
            def->opt_handler(def, opt, key, value);
            return 0;
        }
    }
    printf("config error: unknown option: '%s'.\n", key);
    exit(1);
}

static
int resolve_variables_cb(void *ctx, void *data) {
    struct build_opts *opts = data;
    (void)ctx;

    // strip numeric prefix from target
    char *resolved_target;
    strtoul(opts->target.cstr, &resolved_target, 10);
    if (resolved_target[0] == '.') {
        ++resolved_target;
    }
    ccstrview str_target = ccsv_raw(resolved_target);

    // resolve ROOT's first
    ccstr_replace_raw(&opts->build_root, "$(TARGET)", resolved_target);
    ccstr_replace_raw(&opts->install_root,"$(TARGET)", resolved_target);
    ccstr_replace_raw(&opts->libname,"$(TARGET)", resolved_target);

    ccstrview str_build_root = ccsv(&opts->build_root);
    ccstrview str_install_root = ccsv(&opts->install_root);

    // then PATHS
    ccstr_replace_raw(&opts->srcpaths, "$(TARGET)", resolved_target);
    ccstr_replace(&opts->srcpaths, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
    ccstr_replace(&opts->srcpaths, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

    ccstr_replace_raw(&opts->incpaths, "$(TARGET)", resolved_target);
    ccstr_replace(&opts->incpaths, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
    ccstr_replace(&opts->incpaths, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

    ccstr_replace_raw(&opts->libpaths, "$(TARGET)", resolved_target);
    ccstr_replace(&opts->libpaths, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
    ccstr_replace(&opts->libpaths, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

    // then compile/link commands
    ccstr_replace(&opts->compile, ccsv_raw("$(CC)"), ccsv(&opts->cc));
    ccstr_replace(&opts->compile, ccsv_raw("$(CCFLAGS)"), ccsv(&opts->ccflags));
    ccstr_replace(&opts->compile, ccsv_raw("$(TARGET)"), str_target);
    ccstr_replace(&opts->compile, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
    ccstr_replace(&opts->compile, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

    ccstr_replace(&opts->link, ccsv_raw("$(CC)"), ccsv(&opts->cc));
    ccstr_replace(&opts->link, ccsv_raw("$(LDFLAGS)"), ccsv(&opts->ldflags));
    ccstr_replace(&opts->link, ccsv_raw("$(TARGET)"), str_target);
    ccstr_replace(&opts->link, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
    ccstr_replace(&opts->link, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

    ccstr_replace(&opts->link, ccsv_raw("$(INSTALLDIR)"), ccsv(&opts->installdir));
    ccstr_replace(&opts->link, ccsv_raw("$(LIBPATHS)"), ccsv(&opts->libpaths));
    ccstr_replace(&opts->link, ccsv_raw("$(LIBS)"), ccsv(&opts->libs));

    if (opts->type & SHARED) {
        ccstr_replace(&opts->link_shared, ccsv_raw("$(CC)"), ccsv(&opts->cc));
        ccstr_replace(&opts->link_shared, ccsv_raw("$(LDFLAGS)"), ccsv(&opts->ldflags));
        ccstr_replace(&opts->link_shared, ccsv_raw("$(TARGET)"), str_target);
        ccstr_replace(&opts->link_shared, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
        ccstr_replace(&opts->link_shared, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

        ccstr_replace(&opts->link_shared, ccsv_raw("$(INSTALLDIR)"), ccsv(&opts->installdir));
        ccstr_replace(&opts->link_shared, ccsv_raw("$(LIBPATHS)"), ccsv(&opts->libpaths));
        ccstr_replace(&opts->link_shared, ccsv_raw("$(LIBS)"), ccsv(&opts->libs));
    }

    if (opts->type & STATIC) {
        ccstr_replace(&opts->link_static, ccsv_raw("$(CC)"), ccsv(&opts->cc));
        ccstr_replace(&opts->link_static, ccsv_raw("$(LDFLAGS)"), ccsv(&opts->ldflags));
        ccstr_replace(&opts->link_static, ccsv_raw("$(TARGET)"), str_target);
        ccstr_replace(&opts->link_static, ccsv_raw("$(BUILD_ROOT)"), str_build_root);
        ccstr_replace(&opts->link_static, ccsv_raw("$(INSTALL_ROOT)"), str_install_root);

        ccstr_replace(&opts->link_static, ccsv_raw("$(INSTALLDIR)"), ccsv(&opts->installdir));
        ccstr_replace(&opts->link_static, ccsv_raw("$(LIBPATHS)"), ccsv(&opts->libpaths));
        ccstr_replace(&opts->link_static, ccsv_raw("$(LIBS)"), ccsv(&opts->libs));
    }
    return 0;
}

static
void foreach_target(struct cc_trie *targets, int (*callback)(void *ctx, void *data)) {
    cc_trie_iterate(targets, targets, callback);
}

// TODO: what happens if no config file?
struct cc_trie parse_bopts(const char *filename) {

    // init globals
    if (!g_opts_allocator) {
        g_opts_allocator = cc_new_arena_calloc_wrapper();
        g_default_bopts.lastmodified = ccfs_last_modified_time(filename);
    }

    struct cc_trie target_opts_map = {
        .arena = g_opts_allocator,
    };
    ini_parse(filename, parse_opts_cb, &target_opts_map);

    foreach_target(&target_opts_map, resolve_variables_cb);
    return target_opts_map;
}

// static
// int print_opts_cb(void *ctx, void *data) {
//     struct build_opts *opts = data;
//     (void)ctx;

//     printf("[%s]\n", opts->target.base);
//     printf("type: flag(%d)\n", opts->type);
//     if (opts->type & (SHARED|STATIC)) {
//         printf("so_version: %u\n", opts->so_version);
//     }
// #define printopt(optname) printf(#optname" = '%s'\n", opts->optname.base)
//     printopt(compile);
//     printopt(link);
//     printopt(build_root);
//     printopt(install_root);
//     printopt(installdir);
//     printopt(srcpaths);
//     printopt(incpaths);
//     printopt(libpaths);
//     printopt(ccflags);
//     printopt(ldflags);
//     printopt(release);
//     printopt(debug);
//     printopt(libs);
// #undef printopt
//     printf("\n");
//     return 0;
// }

// void print_configs(struct parsed_opts parsed_opts) {
//     foreach_target(&parsed_opts, print_opts_cb);
// }