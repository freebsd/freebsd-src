/* $Id: arch.c,v 1.17 2021/05/13 13:33:11 schwarze Exp $ */
/*
 * Copyright (c) 2017, 2019 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <string.h>

#include "roff.h"

int
arch_valid(const char *arch, enum mandoc_os os)
{
	const char *openbsd_arch[] = {
		"alpha", "amd64", "arm64", "armv7", "hppa", "i386",
		"landisk", "loongson", "luna88k", "macppc", "mips64",
		"octeon", "powerpc64", "riscv64", "sparc64", NULL
	};
	const char *netbsd_arch[] = {
		"acorn26", "acorn32", "algor", "alpha", "amiga",
		"arc", "atari",
		"bebox", "cats", "cesfic", "cobalt", "dreamcast",
		"emips", "evbarm", "evbmips", "evbppc", "evbsh3", "evbsh5",
		"hp300", "hpcarm", "hpcmips", "hpcsh", "hppa",
		"i386", "ibmnws", "luna68k",
		"mac68k", "macppc", "mipsco", "mmeye", "mvme68k", "mvmeppc",
		"netwinder", "news68k", "newsmips", "next68k",
		"pc532", "playstation2", "pmax", "pmppc", "prep",
		"sandpoint", "sbmips", "sgimips", "shark",
		"sparc", "sparc64", "sun2", "sun3",
		"vax", "walnut", "x68k", "x86", "x86_64", "xen", NULL
	};
	const char **arches[] = { NULL, netbsd_arch, openbsd_arch };
	const char **arch_p;

	if ((arch_p = arches[os]) == NULL)
		return 1;
	for (; *arch_p != NULL; arch_p++)
		if (strcmp(*arch_p, arch) == 0)
			return 1;
	return 0;
}
