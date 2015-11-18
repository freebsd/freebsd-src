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

#include "opt_platform.h"
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
	int law_max;

	ver = SVR_VER(mfspr(SPR_SVR));
	switch (ver) {
	case SVR_MPC8555:
	case SVR_MPC8555E:
		law_max = 8;
		break;
	case SVR_MPC8533:
	case SVR_MPC8533E:
	case SVR_MPC8548:
	case SVR_MPC8548E:
		law_max = 10;
		break;
	case SVR_P5020:
	case SVR_P5020E:
		law_max = 32;
		break;
	default:
		law_max = 8;
	}

	return (law_max);
}

static inline void
law_write(uint32_t n, uint64_t bar, uint32_t sr)
{
#if defined(QORIQ_DPAA)
	ccsr_write4(OCP85XX_LAWBARH(n), bar >> 32);
	ccsr_write4(OCP85XX_LAWBARL(n), bar);
#else
	ccsr_write4(OCP85XX_LAWBAR(n), bar >> 12);
#endif
	ccsr_write4(OCP85XX_LAWSR(n), sr);

	/*
	 * The last write to LAWAR should be followed by a read
	 * of LAWAR before any device try to use any of windows.
	 * What more the read of LAWAR should be followed by isync
	 * instruction.
	 */

	ccsr_read4(OCP85XX_LAWSR(n));
	isync();
}

static inline void
law_read(uint32_t n, uint64_t *bar, uint32_t *sr)
{
#if defined(QORIQ_DPAA)
	*bar = (uint64_t)ccsr_read4(OCP85XX_LAWBARH(n)) << 32 |
	    ccsr_read4(OCP85XX_LAWBARL(n));
#else
	*bar = (uint64_t)ccsr_read4(OCP85XX_LAWBAR(n)) << 12;
#endif
	*sr = ccsr_read4(OCP85XX_LAWSR(n));
}

static int
law_find_free(void)
{
	uint32_t i,sr;
	uint64_t bar;
	int law_max;

	law_max = law_getmax();
	/* Find free LAW */
	for (i = 0; i < law_max; i++) {
		law_read(i, &bar, &sr);
		if ((sr & 0x80000000) == 0)
			break;
	}

	return (i);
}

#define	_LAW_SR(trgt,size)	(0x80000000 | (trgt << 20) | (ffsl(size) - 2))

int
law_enable(int trgt, uint64_t bar, uint32_t size)
{
	uint64_t bar_tmp;
	uint32_t sr, sr_tmp;
	int i, law_max;

	if (size == 0)
		return (0);

	law_max = law_getmax();
	sr = _LAW_SR(trgt, size);

	/* Bail if already programmed. */
	for (i = 0; i < law_max; i++) {
		law_read(i, &bar_tmp, &sr_tmp);
		if (sr == sr_tmp && bar == bar_tmp)
			return (0);
	}

	/* Find an unused access window. */
	i = law_find_free();

	if (i == law_max)
		return (ENOSPC);

	law_write(i, bar, sr);
	return (0);
}

int
law_disable(int trgt, uint64_t bar, uint32_t size)
{
	uint64_t bar_tmp;
	uint32_t sr, sr_tmp;
	int i, law_max;

	law_max = law_getmax();
	sr = _LAW_SR(trgt, size);

	/* Find and disable requested LAW. */
	for (i = 0; i < law_max; i++) {
		law_read(i, &bar_tmp, &sr_tmp);
		if (sr == sr_tmp && bar == bar_tmp) {
			law_write(i, 0, 0);
			return (0);
		}
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
	case 0x0000:
	case 0x8000:
		trgt = 0;
		break;
	case 0x1000:
	case 0x9000:
		trgt = 1;
		break;
	case 0x2000:
	case 0xa000:
		if (ver == SVR_MPC8548E || ver == SVR_MPC8548)
			trgt = 3;
		else
			trgt = 2;
		break;
	case 0x3000:
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

