/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include "amd64_archlevel.h"

	.text
ARCHENTRY(fmaf, avx):
	vfmadd132ss	%xmm1, %xmm2, %xmm0
	ret
ARCHEND(fmaf, avx):

ARCHFUNCS(fmaf)
	ARCHFUNC(fmaf, sw)
	NOARCHFUNC
	NOARCHFUNC
	ARCHFUNC(fmaf, avx)
ENDARCHFUNCS(fmaf)
