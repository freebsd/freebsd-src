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
#include <sys/reboot.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/machdep.h>
#include <machine/pio.h>
#include <machine/spr.h>

#include <dev/fdt/fdt_common.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

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

	if (mpc85xx_is_qoriq()) {
		ccsr_write4(OCP85XX_LAWBARH(n), bar >> 32);
		ccsr_write4(OCP85XX_LAWBARL(n), bar);
		ccsr_write4(OCP85XX_LAWSR_QORIQ(n), sr);
		ccsr_read4(OCP85XX_LAWSR_QORIQ(n));
	} else {
		ccsr_write4(OCP85XX_LAWBAR(n), bar >> 12);
		ccsr_write4(OCP85XX_LAWSR_85XX(n), sr);
		ccsr_read4(OCP85XX_LAWSR_85XX(n));
	}

	/*
	 * The last write to LAWAR should be followed by a read
	 * of LAWAR before any device try to use any of windows.
	 * What more the read of LAWAR should be followed by isync
	 * instruction.
	 */

	isync();
}

static inline void
law_read(uint32_t n, uint64_t *bar, uint32_t *sr)
{

	if (mpc85xx_is_qoriq()) {
		*bar = (uint64_t)ccsr_read4(OCP85XX_LAWBARH(n)) << 32 |
		    ccsr_read4(OCP85XX_LAWBARL(n));
		*sr = ccsr_read4(OCP85XX_LAWSR_QORIQ(n));
	} else {
		*bar = (uint64_t)ccsr_read4(OCP85XX_LAWBAR(n)) << 12;
		*sr = ccsr_read4(OCP85XX_LAWSR_85XX(n));
	}
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

#define	_LAW_SR(trgt,size)	(0x80000000 | (trgt << 20) | \
				(flsl(size + (size - 1)) - 2))

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

static void
l3cache_inval(void)
{

	/* Flash invalidate the CPC and clear all the locks */
	ccsr_write4(OCP85XX_CPC_CSR0, OCP85XX_CPC_CSR0_FI |
	    OCP85XX_CPC_CSR0_LFC);
	while (ccsr_read4(OCP85XX_CPC_CSR0) & (OCP85XX_CPC_CSR0_FI |
	    OCP85XX_CPC_CSR0_LFC))
		;
}

static void
l3cache_enable(void)
{

	ccsr_write4(OCP85XX_CPC_CSR0, OCP85XX_CPC_CSR0_CE |
	    OCP85XX_CPC_CSR0_PE);
	/* Read back to sync write */
	ccsr_read4(OCP85XX_CPC_CSR0);
}

void
mpc85xx_enable_l3_cache(void)
{
	uint32_t csr, size, ver;

	/* Enable L3 CoreNet Platform Cache (CPC) */
	ver = SVR_VER(mfspr(SPR_SVR));
	if (ver == SVR_P2041 || ver == SVR_P2041E || ver == SVR_P3041 ||
	    ver == SVR_P3041E || ver == SVR_P5020 || ver == SVR_P5020E) {
		csr = ccsr_read4(OCP85XX_CPC_CSR0);
		if ((csr & OCP85XX_CPC_CSR0_CE) == 0) {
			l3cache_inval();
			l3cache_enable();
		}

		csr = ccsr_read4(OCP85XX_CPC_CSR0);
		if ((boothowto & RB_VERBOSE) != 0 ||
		    (csr & OCP85XX_CPC_CSR0_CE) == 0) {
			size = OCP85XX_CPC_CFG0_SZ_K(ccsr_read4(OCP85XX_CPC_CFG0));
			printf("L3 Corenet Platform Cache: %d KB %sabled\n",
			    size, (csr & OCP85XX_CPC_CSR0_CE) == 0 ?
			    "dis" : "en");
		}
	}
}

int
mpc85xx_is_qoriq(void)
{
	uint16_t pvr = mfpvr() >> 16;

	/* QorIQ register set is only in e500mc and derivative core based SoCs. */
	if (pvr == FSL_E500mc || pvr == FSL_E5500 || pvr == FSL_E6500)
		return (1);

	return (0);
}

static void
mpc85xx_dataloss_erratum_spr976(void)
{
	uint32_t svr = SVR_VER(mfspr(SPR_SVR));

	/* Ignore whether it's the E variant */
	svr &= ~0x8;

	if (svr != SVR_P3041 && svr != SVR_P4040 &&
	    svr != SVR_P4080 && svr != SVR_P5020)
		return;

	mb();
	isync();
	mtspr(976, (mfspr(976) & ~0x1f8) | 0x48);
	isync();
}

static vm_offset_t
mpc85xx_map_dcsr(void)
{
	phandle_t node;
	u_long b, s;
	int err;

	/*
	 * Try to access the dcsr node directly i.e. through /aliases/.
	 */
	if ((node = OF_finddevice("dcsr")) != -1)
		if (fdt_is_compatible_strict(node, "fsl,dcsr"))
			goto moveon;
	/*
	 * Find the node the long way.
	 */
	if ((node = OF_finddevice("/")) == -1)
		return (0);

	if ((node = ofw_bus_find_compatible(node, "fsl,dcsr")) == 0)
		return (0);

moveon:
	err = fdt_get_range(node, 0, &b, &s);

	if (err != 0)
		return (0);

	law_enable(OCP85XX_TGTIF_DCSR, b, 0x400000);
	return pmap_early_io_map(b, 0x400000);
}



void
mpc85xx_fix_errata(vm_offset_t va_ccsr)
{
	uint32_t svr = SVR_VER(mfspr(SPR_SVR));
	vm_offset_t va_dcsr;

	/* Ignore whether it's the E variant */
	svr &= ~0x8;

	if (svr != SVR_P3041 && svr != SVR_P4040 &&
	    svr != SVR_P4080 && svr != SVR_P5020)
		return;

	if (mfmsr() & PSL_EE)
		return;

	/*
	 * dcsr region need to be mapped thus patch can refer to.
	 * Align dcsr right after ccsbar.
	 */
	va_dcsr = mpc85xx_map_dcsr();
	if (va_dcsr == 0)
		goto err;

	/*
	 * As A004510 errata specify, special purpose register 976
	 * SPR976[56:60] = 6'b001001 must be set. e500mc core reference manual
	 * does not document SPR976 register.
	 */
	mpc85xx_dataloss_erratum_spr976();

	/*
	 * Specific settings in the CCF and core platform cache (CPC)
	 * are required to reconfigure the CoreNet coherency fabric.
	 * The register settings that should be updated are described
	 * in errata and relay on base address, offset and updated value.
	 * Special conditions must be used to update these registers correctly.
	 */
	dataloss_erratum_access(va_dcsr + 0xb0e08, 0xe0201800);
	dataloss_erratum_access(va_dcsr + 0xb0e18, 0xe0201800);
	dataloss_erratum_access(va_dcsr + 0xb0e38, 0xe0400000);
	dataloss_erratum_access(va_dcsr + 0xb0008, 0x00900000);
	dataloss_erratum_access(va_dcsr + 0xb0e40, 0xe00a0000);

	switch (svr) {
	case SVR_P5020:
		dataloss_erratum_access(va_ccsr + 0x18600, 0xc0000000);
		break;
	case SVR_P4040:
	case SVR_P4080:
		dataloss_erratum_access(va_ccsr + 0x18600, 0xff000000);
		break;
	case SVR_P3041:
		dataloss_erratum_access(va_ccsr + 0x18600, 0xf0000000);
	}
	dataloss_erratum_access(va_ccsr + 0x10f00, 0x415e5000);
	dataloss_erratum_access(va_ccsr + 0x11f00, 0x415e5000);

err:
	return;
}
