/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_ASSERT_PARAM_H
#define _CC_ASSERT_PARAM_H

#include <assert.h>

#define CC_ASSERT(cond, errno) do { \
    assert(cond);                   \
    if (!(cond)) return errno;      \
} while(0)

#endif // _CC_ASSERT_PARAM_H
