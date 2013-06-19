/*	$NetBSD: imx51_ccm.c,v 1.1 2012/04/17 09:33:31 bsh Exp $	*/
/*
 * Copyright (c) 2010, 2011, 2012  Genetec Corporation.  All rights reserved.
 * Written by Hashimoto Kenichi for Genetec Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2012, 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.	Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2.	Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

/*
 * Clock Controller Module (CCM)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <arm/freescale/imx/imx51_ccmvar.h>
#include <arm/freescale/imx/imx51_ccmreg.h>
#include <arm/freescale/imx/imx51_dpllreg.h>

#define	IMXCCMDEBUG
#undef	IMXCCMDEBUG

#ifndef	IMX51_OSC_FREQ
#define	IMX51_OSC_FREQ	(24 * 1000 * 1000)	/* 24MHz */
#endif

#ifndef	IMX51_CKIL_FREQ
#define	IMX51_CKIL_FREQ	32768
#endif

struct imxccm_softc {
	device_t	sc_dev;
	struct resource *res[7];
	u_int64_t 	pll_freq[IMX51_N_DPLLS];
};

struct imxccm_softc *ccm_softc = NULL;

static uint64_t imx51_get_pll_freq(u_int);

static int imxccm_match(device_t);
static int imxccm_attach(device_t);

static device_method_t imxccm_methods[] = {
	DEVMETHOD(device_probe, imxccm_match),
	DEVMETHOD(device_attach, imxccm_attach),

	DEVMETHOD_END
};

static driver_t imxccm_driver = {
	"imxccm",
	imxccm_methods,
	sizeof(struct imxccm_softc),
};

static devclass_t imxccm_devclass;

EARLY_DRIVER_MODULE(imxccm, simplebus, imxccm_driver, imxccm_devclass, 0, 0,
    BUS_PASS_CPU);

static struct resource_spec imxccm_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Global registers */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* DPLLIP1 */
	{ SYS_RES_MEMORY,	2,	RF_ACTIVE },	/* DPLLIP2 */
	{ SYS_RES_MEMORY,	3,	RF_ACTIVE },	/* DPLLIP3 */
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },    /* 71 */
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },    /* 72 */
	{ -1, 0 }
};

static int
imxccm_match(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,imx51-ccm"))
		return (ENXIO);

	device_set_desc(dev, "Freescale Clock Control Module");
	return (BUS_PROBE_DEFAULT);
}

static int
imxccm_attach(device_t dev)
{
	struct imxccm_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	if (bus_alloc_resources(dev, imxccm_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	ccm_softc = sc;

	imx51_get_pll_freq(1);
	imx51_get_pll_freq(2);
	imx51_get_pll_freq(3);

	device_printf(dev, "PLL1=%lluMHz, PLL2=%lluMHz, PLL3=%lluMHz\n",
	    sc->pll_freq[0] / 1000000,
	    sc->pll_freq[1] / 1000000,
	    sc->pll_freq[2] / 1000000);
	device_printf(dev, "CPU clock=%d, UART clock=%d\n",
	    imx51_get_clock(IMX51CLK_ARM_ROOT),
	    imx51_get_clock(IMX51CLK_UART_CLK_ROOT));
	device_printf(dev,
	    "mainbus clock=%d, ahb clock=%d ipg clock=%d perclk=%d\n",
	    imx51_get_clock(IMX51CLK_MAIN_BUS_CLK),
	    imx51_get_clock(IMX51CLK_AHB_CLK_ROOT),
	    imx51_get_clock(IMX51CLK_IPG_CLK_ROOT),
	    imx51_get_clock(IMX51CLK_PERCLK_ROOT));


	return (0);
}

u_int
imx51_get_clock(enum imx51_clock clk)
{
	u_int freq;
	u_int sel;
	uint32_t cacrr;	/* ARM clock root register */
	uint32_t ccsr;
	uint32_t cscdr1;
	uint32_t cscmr1;
	uint32_t cbcdr;
	uint32_t cbcmr;
	uint32_t cdcr;

	if (ccm_softc == NULL)
		return (0);

	switch (clk) {
	case IMX51CLK_PLL1:
	case IMX51CLK_PLL2:
	case IMX51CLK_PLL3:
		return ccm_softc->pll_freq[clk-IMX51CLK_PLL1];
	case IMX51CLK_PLL1SW:
		ccsr = bus_read_4(ccm_softc->res[0], CCMC_CCSR);
		if ((ccsr & CCSR_PLL1_SW_CLK_SEL) == 0)
			return ccm_softc->pll_freq[1-1];
		/* step clock */
		/* FALLTHROUGH */
	case IMX51CLK_PLL1STEP:
		ccsr = bus_read_4(ccm_softc->res[0], CCMC_CCSR);
		switch ((ccsr & CCSR_STEP_SEL_MASK) >> CCSR_STEP_SEL_SHIFT) {
		case 0:
			return imx51_get_clock(IMX51CLK_LP_APM);
		case 1:
			return 0; /* XXX PLL bypass clock */
		case 2:
			return ccm_softc->pll_freq[2-1] /
			    (1 + ((ccsr & CCSR_PLL2_DIV_PODF_MASK) >>
				CCSR_PLL2_DIV_PODF_SHIFT));
		case 3:
			return ccm_softc->pll_freq[3-1] /
			    (1 + ((ccsr & CCSR_PLL3_DIV_PODF_MASK) >>
				CCSR_PLL3_DIV_PODF_SHIFT));
		}
		/*NOTREACHED*/
	case IMX51CLK_PLL2SW:
		ccsr = bus_read_4(ccm_softc->res[0], CCMC_CCSR);
		if ((ccsr & CCSR_PLL2_SW_CLK_SEL) == 0)
			return imx51_get_clock(IMX51CLK_PLL2);
		return 0; /* XXX PLL2 bypass clk */
	case IMX51CLK_PLL3SW:
		ccsr = bus_read_4(ccm_softc->res[0], CCMC_CCSR);
		if ((ccsr & CCSR_PLL3_SW_CLK_SEL) == 0)
			return imx51_get_clock(IMX51CLK_PLL3);
		return 0; /* XXX PLL3 bypass clk */

	case IMX51CLK_LP_APM:
		ccsr = bus_read_4(ccm_softc->res[0], CCMC_CCSR);
		return (ccsr & CCSR_LP_APM) ?
			    imx51_get_clock(IMX51CLK_FPM) : IMX51_OSC_FREQ;

	case IMX51CLK_ARM_ROOT:
		freq = imx51_get_clock(IMX51CLK_PLL1SW);
		cacrr = bus_read_4(ccm_softc->res[0], CCMC_CACRR);
		return freq / (cacrr + 1);

		/* ... */
	case IMX51CLK_MAIN_BUS_CLK_SRC:
		cbcdr = bus_read_4(ccm_softc->res[0], CCMC_CBCDR);
		if ((cbcdr & CBCDR_PERIPH_CLK_SEL) == 0)
			freq = imx51_get_clock(IMX51CLK_PLL2SW);
		else {
			freq = 0;
			cbcmr = bus_read_4(ccm_softc->res[0],  CCMC_CBCMR);
			switch ((cbcmr & CBCMR_PERIPH_APM_SEL_MASK) >>
				CBCMR_PERIPH_APM_SEL_SHIFT) {
			case 0:
				freq = imx51_get_clock(IMX51CLK_PLL1SW);
				break;
			case 1:
				freq = imx51_get_clock(IMX51CLK_PLL3SW);
				break;
			case 2:
				freq = imx51_get_clock(IMX51CLK_LP_APM);
				break;
			case 3:
				/* XXX: error */
				break;
			}
		}
		return freq;
	case IMX51CLK_MAIN_BUS_CLK:
		freq = imx51_get_clock(IMX51CLK_MAIN_BUS_CLK_SRC);
		cdcr = bus_read_4(ccm_softc->res[0], CCMC_CDCR);
		return freq / (cdcr & CDCR_PERIPH_CLK_DVFS_PODF_MASK) >>
			CDCR_PERIPH_CLK_DVFS_PODF_SHIFT;
	case IMX51CLK_AHB_CLK_ROOT:
		freq = imx51_get_clock(IMX51CLK_MAIN_BUS_CLK);
		cbcdr = bus_read_4(ccm_softc->res[0], CCMC_CBCDR);
		return freq / (1 + ((cbcdr & CBCDR_AHB_PODF_MASK) >>
				    CBCDR_AHB_PODF_SHIFT));
	case IMX51CLK_IPG_CLK_ROOT:
		freq = imx51_get_clock(IMX51CLK_AHB_CLK_ROOT);
		cbcdr = bus_read_4(ccm_softc->res[0], CCMC_CBCDR);
		return freq / (1 + ((cbcdr & CBCDR_IPG_PODF_MASK) >>
				    CBCDR_IPG_PODF_SHIFT));

	case IMX51CLK_PERCLK_ROOT:
		cbcmr = bus_read_4(ccm_softc->res[0], CCMC_CBCMR);
		if (cbcmr & CBCMR_PERCLK_IPG_SEL)
			return imx51_get_clock(IMX51CLK_IPG_CLK_ROOT);
		if (cbcmr & CBCMR_PERCLK_LP_APM_SEL)
			freq = imx51_get_clock(IMX51CLK_LP_APM);
		else
			freq = imx51_get_clock(IMX51CLK_MAIN_BUS_CLK_SRC);
		cbcdr = bus_read_4(ccm_softc->res[0], CCMC_CBCDR);

#ifdef IMXCCMDEBUG
		printf("cbcmr=%x cbcdr=%x\n", cbcmr, cbcdr);
#endif

		freq /= 1 + ((cbcdr & CBCDR_PERCLK_PRED1_MASK) >>
			CBCDR_PERCLK_PRED1_SHIFT);
		freq /= 1 + ((cbcdr & CBCDR_PERCLK_PRED2_MASK) >>
			CBCDR_PERCLK_PRED2_SHIFT);
		freq /= 1 + ((cbcdr & CBCDR_PERCLK_PODF_MASK) >>
			CBCDR_PERCLK_PODF_SHIFT);
		return freq;
	case IMX51CLK_UART_CLK_ROOT:
		cscdr1 = bus_read_4(ccm_softc->res[0], CCMC_CSCDR1);
		cscmr1 = bus_read_4(ccm_softc->res[0], CCMC_CSCMR1);

#ifdef IMXCCMDEBUG
		printf("cscdr1=%x cscmr1=%x\n", cscdr1, cscmr1);
#endif

		sel = (cscmr1 & CSCMR1_UART_CLK_SEL_MASK) >>
		    CSCMR1_UART_CLK_SEL_SHIFT;

		freq = 0; /* shut up GCC */
		switch (sel) {
		case 0:
		case 1:
		case 2:
			freq = imx51_get_clock(IMX51CLK_PLL1SW + sel);
			break;
		case 3:
			freq = imx51_get_clock(IMX51CLK_LP_APM);
			break;
		}

		return freq / (1 + ((cscdr1 & CSCDR1_UART_CLK_PRED_MASK) >>
			CSCDR1_UART_CLK_PRED_SHIFT)) /
		    (1 + ((cscdr1 & CSCDR1_UART_CLK_PODF_MASK) >>
			CSCDR1_UART_CLK_PODF_SHIFT));
	case IMX51CLK_IPU_HSP_CLK_ROOT:
		freq = 0;
		cbcmr = bus_read_4(ccm_softc->res[0],  CCMC_CBCMR);
		switch ((cbcmr & CBCMR_IPU_HSP_CLK_SEL_MASK) >>
				CBCMR_IPU_HSP_CLK_SEL_SHIFT) {
			case 0:
				freq = imx51_get_clock(IMX51CLK_ARM_AXI_A_CLK);
				break;
			case 1:
				freq = imx51_get_clock(IMX51CLK_ARM_AXI_B_CLK);
				break;
			case 2:
				freq = imx51_get_clock(
					IMX51CLK_EMI_SLOW_CLK_ROOT);
				break;
			case 3:
				freq = imx51_get_clock(IMX51CLK_AHB_CLK_ROOT);
				break;
			}
		return freq;
	default:
		device_printf(ccm_softc->sc_dev,
		    "clock %d: not supported yet\n", clk);
		return 0;
	}
}


static uint64_t
imx51_get_pll_freq(u_int pll_no)
{
	uint32_t dp_ctrl;
	uint32_t dp_op;
	uint32_t dp_mfd;
	uint32_t dp_mfn;
	uint32_t mfi;
	int32_t mfn;
	uint32_t mfd;
	uint32_t pdf;
	uint32_t ccr;
	uint64_t freq = 0;
	u_int ref = 0;

	KASSERT(1 <= pll_no && pll_no <= IMX51_N_DPLLS, ("Wrong PLL id"));

	dp_ctrl = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_CTL);

	if (dp_ctrl & DP_CTL_HFSM) {
		dp_op = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_HFS_OP);
		dp_mfd = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_HFS_MFD);
		dp_mfn = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_HFS_MFN);
	} else {
		dp_op = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_OP);
		dp_mfd = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_MFD);
		dp_mfn = bus_read_4(ccm_softc->res[pll_no], DPLL_DP_MFN);
	}

	pdf = dp_op & DP_OP_PDF_MASK;
	mfi = max(5, (dp_op & DP_OP_MFI_MASK) >> DP_OP_MFI_SHIFT);
	mfd = dp_mfd;
	if (dp_mfn & 0x04000000)
		/* 27bit signed value */
		mfn = (uint32_t)(0xf8000000 | dp_mfn);
	else
		mfn = dp_mfn;

	switch (dp_ctrl &  DP_CTL_REF_CLK_SEL_MASK) {
	case DP_CTL_REF_CLK_SEL_COSC:
		/* Internal Oscillator */
		/* TODO: get from FDT "fsl,imx-osc" */
		ref = 24000000; /* IMX51_OSC_FREQ */
		break;
	case DP_CTL_REF_CLK_SEL_FPM:
		ccr = bus_read_4(ccm_softc->res[0], CCMC_CCR);
		if (ccr & CCR_FPM_MULT)
		/* TODO: get from FDT "fsl,imx-ckil" */
			ref = 32768 * 1024;
		else
		/* TODO: get from FDT "fsl,imx-ckil" */
			ref = 32768 * 512;
		break;
	default:
		ref = 0;
	}

	if (dp_ctrl & DP_CTL_REF_CLK_DIV)
		ref /= 2;

	ref *= 4;
	freq = (int64_t)ref * mfi + (int64_t)ref * mfn / (mfd + 1);
	freq /= pdf + 1;

	if (!(dp_ctrl & DP_CTL_DPDCK0_2_EN))
		freq /= 2;

#ifdef IMXCCMDEBUG
	printf("ref: %dKHz ", ref);
	printf("dp_ctl: %08x ", dp_ctrl);
	printf("pdf: %3d ", pdf);
	printf("mfi: %3d ", mfi);
	printf("mfd: %3d ", mfd);
	printf("mfn: %3d ", mfn);
	printf("pll: %d\n", (uint32_t)freq);
#endif

	ccm_softc->pll_freq[pll_no-1] = freq;

	return (freq);
}

void
imx51_clk_gating(int clk_src, int mode)
{
	int field, group;
	uint32_t reg;

	group = CCMR_CCGR_MODULE(clk_src);
	field = clk_src % CCMR_CCGR_NSOURCE;
	reg = bus_read_4(ccm_softc->res[0], CCMC_CCGR(group));
	reg &= ~(0x03 << field * 2);
	reg |= (mode << field * 2);
	bus_write_4(ccm_softc->res[0], CCMC_CCGR(group), reg);
}

int
imx51_get_clk_gating(int clk_src)
{
	uint32_t reg;

	reg = bus_read_4(ccm_softc->res[0],
	    CCMC_CCGR(CCMR_CCGR_MODULE(clk_src)));
	return ((reg >> (clk_src % CCMR_CCGR_NSOURCE) * 2) & 0x03);
}

