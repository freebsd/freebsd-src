/*-
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Vybrid Family Clock Controller Module (CCM)
 * Chapter 10, Vybrid Reference Manual, Rev. 5, 07/2013
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
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <arm/freescale/vybrid/vf_common.h>

#define	CCM_CCR		0x00	/* Control Register */
#define	CCM_CSR		0x04	/* Status Register */
#define	CCM_CCSR	0x08	/* Clock Switcher Register */
#define	CCM_CACRR	0x0C	/* ARM Clock Root Register */
#define	CCM_CSCMR1	0x10	/* Serial Clock Multiplexer Register 1 */
#define	CCM_CSCDR1	0x14	/* Serial Clock Divider Register 1 */
#define	CCM_CSCDR2	0x18	/* Serial Clock Divider Register 2 */
#define	CCM_CSCDR3	0x1C	/* Serial Clock Divider Register 3 */
#define	CCM_CSCMR2	0x20	/* Serial Clock Multiplexer Register 2 */
#define	CCM_CTOR	0x28	/* Testing Observability Register */
#define	CCM_CLPCR	0x2C	/* Low Power Control Register */
#define	CCM_CISR	0x30	/* Interrupt Status Register */
#define	CCM_CIMR	0x34	/* Interrupt Mask Register */
#define	CCM_CCOSR	0x38	/* Clock Output Source Register */
#define	CCM_CGPR	0x3C	/* General Purpose Register */

#define	CCM_CCGRN	12
#define	CCM_CCGR(n)	(0x40 + (n * 0x04))	/* Clock Gating Register */
#define	CCM_CMEOR(n)	(0x70 + (n * 0x70))	/* Module Enable Override Reg */
#define	CCM_CCPGR(n)	(0x90 + (n * 0x04))	/* Platform Clock Gating Reg */

#define	CCM_CPPDSR	0x88	/* PLL PFD Disable Status Register */
#define	CCM_CCOWR	0x8C	/* CORE Wakeup Register */

#define	PLL3_PFD4_EN	(1U << 31)
#define	PLL3_PFD3_EN	(1 << 30)
#define	PLL3_PFD2_EN	(1 << 29)
#define	PLL3_PFD1_EN	(1 << 28)
#define	PLL2_PFD4_EN	(1 << 15)
#define	PLL2_PFD3_EN	(1 << 14)
#define	PLL2_PFD2_EN	(1 << 13)
#define	PLL2_PFD1_EN	(1 << 12)
#define	PLL1_PFD4_EN	(1 << 11)
#define	PLL1_PFD3_EN	(1 << 10)
#define	PLL1_PFD2_EN	(1 << 9)
#define	PLL1_PFD1_EN	(1 << 8)

/* CCM_CCR */
#define	FIRC_EN		(1 << 16)
#define	FXOSC_EN	(1 << 12)
#define	FXOSC_RDY	(1 << 5)

/* CCM_CSCDR1 */
#define	ENET_TS_EN	(1 << 23)
#define	RMII_CLK_EN	(1 << 24)

struct ccm_softc {
	struct resource		*res[1];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	device_t		dev;
};

static struct resource_spec ccm_spec[] = {
	{ SYS_RES_MEMORY,       0,      RF_ACTIVE },
	{ -1, 0 }
};

static int
ccm_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,mvf600-ccm"))
		return (ENXIO);

	device_set_desc(dev, "Vybrid Family CCM Unit");
	return (BUS_PROBE_DEFAULT);
}

static int
ccm_attach(device_t dev)
{
	struct ccm_softc *sc;
	int reg;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, ccm_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	/* Enable oscillator */
	reg = READ4(sc, CCM_CCR);
	reg |= (FIRC_EN | FXOSC_EN);
	WRITE4(sc, CCM_CCR, reg);

	/* Wait 10 times */
	for (i = 0; i < 10; i++) {
		if (READ4(sc, CCM_CSR) & FXOSC_RDY) {
			device_printf(sc->dev, "On board oscillator is ready.\n");
			break;
		}

		cpufunc_nullop();
	}

	/* Clock is on during all modes, except stop mode. */
	for (i = 0; i < CCM_CCGRN; i++) {
		WRITE4(sc, CCM_CCGR(i), 0xffffffff);
	}

	/* Enable ENET clocks */
	reg = READ4(sc, CCM_CSCDR1);
	reg |= (ENET_TS_EN | RMII_CLK_EN);
	WRITE4(sc, CCM_CSCDR1, reg);

	return (0);
}

static device_method_t ccm_methods[] = {
	DEVMETHOD(device_probe,		ccm_probe),
	DEVMETHOD(device_attach,	ccm_attach),
	{ 0, 0 }
};

static driver_t ccm_driver = {
	"ccm",
	ccm_methods,
	sizeof(struct ccm_softc),
};

static devclass_t ccm_devclass;

DRIVER_MODULE(ccm, simplebus, ccm_driver, ccm_devclass, 0, 0);
