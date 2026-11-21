/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Piotr Kubaj
 */

#include <machine/cpu.h>
#include <machine/ifunc.h>

#include <setjmp.h>

int	__sigsetjmp(sigjmp_buf, int);
int	__sigsetjmp_altivec(sigjmp_buf, int);

DEFINE_UIFUNC(, int, sigsetjmp, (sigjmp_buf, int))
{
	if (cpu_features & PPC_FEATURE_HAS_ALTIVEC)
		return (__sigsetjmp_altivec);
	return (__sigsetjmp);
}
