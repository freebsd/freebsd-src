/*-
 * Copyright (c) 2011-2012 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/reboot.h>

#include <machine/machdep.h>

#include <dev/fdt/fdt_common.h>

#include <powerpc/mpc85xx/mpc85xx.h>

extern void dcache_enable(void);
extern void dcache_inval(void);
extern void icache_enable(void);
extern void icache_inval(void);
extern void l2cache_enable(void);
extern void l2cache_inval(void);

void
booke_init_tlb(vm_paddr_t fdt_immr_pa)
{

	/* Initialize TLB1 handling */
	tlb1_init(fdt_immr_pa);
}

void
booke_enable_l1_cache(void)
{
	uint32_t csr;

	/* Enable D-cache if applicable */
	csr = mfspr(SPR_L1CSR0);
	if ((csr & L1CSR0_DCE) == 0) {
		dcache_inval();
		dcache_enable();
	}

	csr = mfspr(SPR_L1CSR0);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR0_DCE) == 0)
		printf("L1 D-cache %sabled\n",
		    (csr & L1CSR0_DCE) ? "en" : "dis");

	/* Enable L1 I-cache if applicable. */
	csr = mfspr(SPR_L1CSR1);
	if ((csr & L1CSR1_ICE) == 0) {
		icache_inval();
		icache_enable();
	}

	csr = mfspr(SPR_L1CSR1);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR1_ICE) == 0)
		printf("L1 I-cache %sabled\n",
		    (csr & L1CSR1_ICE) ? "en" : "dis");
}

#if 0
void
booke_enable_l2_cache(void)
{
	uint32_t csr;

	/* Enable L2 cache on E500mc */
	if ((((mfpvr() >> 16) & 0xFFFF) == FSL_E500mc) ||
	    (((mfpvr() >> 16) & 0xFFFF) == FSL_E5500)) {
		csr = mfspr(SPR_L2CSR0);
		if ((csr & L2CSR0_L2E) == 0) {
			l2cache_inval();
			l2cache_enable();
		}

		csr = mfspr(SPR_L2CSR0);
		if ((boothowto & RB_VERBOSE) != 0 || (csr & L2CSR0_L2E) == 0)
			printf("L2 cache %sabled\n",
			    (csr & L2CSR0_L2E) ? "en" : "dis");
	}
}

void
booke_enable_l3_cache(void)
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

void
booke_disable_l2_cache(void)
{
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
#endif
