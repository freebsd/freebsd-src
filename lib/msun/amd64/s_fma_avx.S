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
ARCHENTRY(fma, avx)
	vfmadd132sd	%xmm1, %xmm2, %xmm0
	ret
ARCHEND(fma, avx)

ARCHFUNCS(fma)
	ARCHFUNC(fma, sw)
	NOARCHFUNC
	NOARCHFUNC
	ARCHFUNC(fma, avx)
ENDARCHFUNCS(fma)
