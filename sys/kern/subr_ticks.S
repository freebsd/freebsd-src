/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
 */

/*
 * Define the "ticks" and "ticksl" variables.  The former is overlaid onto the
 * low bits of the latter.  Also define an alias "jiffies" of "ticksl",
 * used by the LinuxKPI.
 */

#if defined(__aarch64__)
#include <sys/elf_common.h>
#include <machine/asm.h>

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define	TICKS_OFFSET	0
#else
#define	TICKS_OFFSET	(__SIZEOF_LONG__ - __SIZEOF_INT__)
#endif

	.bss

	.global ticksl
	.type ticksl, %object
	.balign __SIZEOF_LONG__
ticksl:	.zero __SIZEOF_LONG__
	.size ticksl, __SIZEOF_LONG__

	.global ticks
	.type ticks, %object
ticks	=ticksl + TICKS_OFFSET
	.size ticks, __SIZEOF_INT__

	.global jiffies
	.type jiffies, %object
jiffies	= ticksl
	.size jiffies, __SIZEOF_LONG__
