/*
 * alloc-inject.h
 * allocator fault injection for fuzzing harnesses
 *
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef PKGCONF_FUZZER_ALLOC_INJECT_H
#define PKGCONF_FUZZER_ALLOC_INJECT_H

#include <stdbool.h>

/* arm injection so that the fail_at-th allocation made while armed fails */
void alloc_inject_arm(unsigned long fail_at);

/* stop failing allocations */
void alloc_inject_disarm(void);

/* whether the armed failure point was actually reached since the last arm */
bool alloc_inject_fired(void);

#endif
