/*
 * Copyright 2022, Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Hack for aarch64...  Not sure why isspace isn't defined, but it sure doesn't
 * belong here.
 */
#ifndef isspace
static __inline int isspace(int c)
{
    return c == ' ' || (c >= 0x9 && c <= 0xd);
}
#endif

#include "blake3_impl.c"
