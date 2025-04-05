/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 * 
 * Copyright (c) 2025 Josh Simonot
 */

// set logging function matching printf declaration,
// or define as nothing to disable stdio and prints
#include <stdio.h>
#define CC_LOGF printf

#define CC_ALLOCATOR_IMPLEMENTATION
#include "cc_allocator.h"
