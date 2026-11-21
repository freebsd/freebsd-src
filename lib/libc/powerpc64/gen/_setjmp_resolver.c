/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Piotr Kubaj
 */

#include <machine/cpu.h>
#include <machine/ifunc.h>

#include <setjmp.h>

int	___setjmp(jmp_buf);
int	___setjmp_altivec(jmp_buf);

DEFINE_UIFUNC(, int, _setjmp, (jmp_buf))
{
	if (cpu_features & PPC_FEATURE_HAS_ALTIVEC)
		return (___setjmp_altivec);
	return (___setjmp);
}
