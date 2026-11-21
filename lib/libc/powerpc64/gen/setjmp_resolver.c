/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Piotr Kubaj
 */

#include <machine/cpu.h>
#include <machine/ifunc.h>

#include <setjmp.h>

int	__setjmp(jmp_buf);
int	__setjmp_altivec(jmp_buf);

DEFINE_UIFUNC(, int, setjmp, (jmp_buf))
{
	if (cpu_features & PPC_FEATURE_HAS_ALTIVEC)
		return (__setjmp_altivec);
	return (__setjmp);
}
