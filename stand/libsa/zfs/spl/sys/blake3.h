/*
 * Copyright 2022, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

/*
 * Gross and ugly hack to cope with upstream's sys/blake3.h not being standalone
 * safe.
 */
#define _KERNEL

#include_next <sys/blake3.h>

#undef _KERNEL
