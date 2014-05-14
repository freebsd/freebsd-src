/*-
 * Copyright (C) 2008 Semihalf, Rafal Jaworowski
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/pio.h>
#include <machine/spr.h>

#include <dev/fdt/fdt_common.h>

#include <powerpc/mpc85xx/mpc85xx.h>

/*
 * MPC85xx system specific routines
 */

uint32_t
ccsr_read4(uintptr_t addr)
{
	volatile uint32_t *ptr = (void *)addr;

	return (*ptr);
}

void
ccsr_write4(uintptr_t addr, uint32_t val)
{
	volatile uint32_t *ptr = (void *)addr;

	*ptr = val;
	powerpc_iomb();
}

int
law_getmax(void)
{
	uint32_t ver;

	ver = SVR_VER(mfspr(SPR_SVR));
	if (ver == SVR_MPC8555E || ver == SVR_MPC8555)
		return (8);
	if (ver == SVR_MPC8548E || ver == SVR_MPC8548 ||
	    ver == SVR_MPC8533E || ver == SVR_MPC8533)
		return (10);

	return (12);
}

#define	_LAW_SR(trgt,size)	(0x80000000 | (trgt << 20) | (ffsl(size) - 2))
#define	_LAW_BAR(addr)		(addr >> 12)

int
law_enable(int trgt, u_long addr, u_long size)
{
	uint32_t bar, sr;
	int i, law_max;

	if (size == 0)
		return (0);

	law_max = law_getmax();
	bar = _LAW_BAR(addr);
	sr = _LAW_SR(trgt, size);

	/* Bail if already programmed. */
	for (i = 0; i < law_max; i++)
		if (sr == ccsr_read4(OCP85XX_LAWSR(i)) &&
		    bar == ccsr_read4(OCP85XX_LAWBAR(i)))
			return (0);

	/* Find an unused access window. */
	for (i = 0; i < law_max; i++)
		if ((ccsr_read4(OCP85XX_LAWSR(i)) & 0x80000000) == 0)
			break;

	if (i == law_max)
		return (ENOSPC);

	ccsr_write4(OCP85XX_LAWBAR(i), bar);
	ccsr_write4(OCP85XX_LAWSR(i), sr);
	return (0);
}

int
law_disable(int trgt, u_long addr, u_long size)
{
	uint32_t bar, sr;
	int i, law_max;

	law_max = law_getmax();
	bar = _LAW_BAR(addr);
	sr = _LAW_SR(trgt, size);

	/* Find and disable requested LAW. */
	for (i = 0; i < law_max; i++)
		if (sr == ccsr_read4(OCP85XX_LAWSR(i)) &&
		    bar == ccsr_read4(OCP85XX_LAWBAR(i))) {
			ccsr_write4(OCP85XX_LAWBAR(i), 0);
			ccsr_write4(OCP85XX_LAWSR(i), 0);
			return (0);
		}

	return (ENOENT);
}

int
law_pci_target(struct resource *res, int *trgt_mem, int *trgt_io)
{
	u_long start;
	uint32_t ver;
	int trgt, rv;

	ver = SVR_VER(mfspr(SPR_SVR));

	start = rman_get_start(res) & 0xf000;

	rv = 0;
	trgt = -1;
	switch (start) {
	case 0x8000:
		trgt = 0;
		break;
	case 0x9000:
		trgt = 1;
		break;
	case 0xa000:
		if (ver == SVR_MPC8548E || ver == SVR_MPC8548)
			trgt = 3;
		else
			trgt = 2;
		break;
	case 0xb000:
		if (ver == SVR_MPC8548E || ver == SVR_MPC8548)
			rv = EINVAL;
		else
			trgt = 3;
		break;
	default:
		rv = ENXIO;
	}
	if (rv == 0) {
		*trgt_mem = trgt;
		*trgt_io = trgt;
	}
	return (rv);
}

