/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
 */

/*
 * Define the "ticks" and "ticksl" variables.  The former is overlaid onto the
 * low bits of the latter.
 */

#if defined(__aarch64__)
#include <sys/elf_common.h>
#include <machine/asm.h>

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
#endif

#ifdef _ILP32
#define	SIZEOF_TICKSL	4
#define	TICKSL_INIT	.long 0
#else
#define	SIZEOF_TICKSL	8
#define	TICKSL_INIT	.quad 0
#endif

#if defined(_ILP32) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define	TICKS_OFFSET	0
#else
#define	TICKS_OFFSET	4
#endif

	.data

	.global ticksl
	.type ticksl, %object
	.align SIZEOF_TICKSL
ticksl:	TICKSL_INIT
	.size ticksl, SIZEOF_TICKSL

	.global ticks
	.type ticks, %object
ticks	=ticksl + TICKS_OFFSET
	.size ticks, 4
