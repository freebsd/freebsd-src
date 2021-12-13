/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/cdefs.h>

/*
 * A hack to allow libdwarf.a to be statically linked without zlib.  This is
 * unfortunately required for FreeBSD <= 13.0 to bootstrap build tools
 * such as nm(1), as they use metadata from the source tree to generate the
 * dependency list but then link with the build host's libraries.
 */

extern int uncompress(void *, unsigned long *, const void *,
    unsigned long);

int __weak_symbol
uncompress(void *dst __unused, unsigned long *dstsz __unused,
    const void *src __unused, unsigned long srcsz __unused)
{
	return (-6); /* Z_VERSION_ERROR */
}
