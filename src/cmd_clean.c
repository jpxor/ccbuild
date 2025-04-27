/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#include "cmd_clean.h"
#include "cmd_build.h"
#include "libcc/cc_files.h"

int cc_clean(struct cmdopts *cmdopts) {
    struct build_state state = {
        .cmdopts = *cmdopts,
    };
    int err = set_root_and_build_paths(&state);
    if (err) return EXIT_FAILURE;

    ccfs_rmdir_recursive(state.buildir.cstr);
    return EXIT_SUCCESS;
}
