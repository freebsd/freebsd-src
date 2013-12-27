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
 * Vybrid Family Input/Output Multiplexer Controller (IOMUXC)
 * Chapter 5, Vybrid Reference Manual, Rev. 5, 07/2013
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

#include <arm/freescale/vybrid/vf_iomuxc.h>
#include <arm/freescale/vybrid/vf_common.h>

#define	IBE		(1 << 0) /* Input Buffer Enable Field */
#define	OBE		(1 << 1) /* Output Buffer Enable Field. */
#define	PUE		(1 << 2) /* Pull / Keep Select Field. */
#define	PKE		(1 << 3) /* Pull / Keep Enable Field. */
#define	PUS_MASK	(3 << 4) /* Pull Up / Down Config Field. */
#define	DSE_MASK	(7 << 6) /* Drive Strength Field. */
#define	HYS		(1 << 9) /* Hysteresis Enable Field */

#define	MUX_MODE_MASK		7
#define	MUX_MODE_SHIFT		20
#define	MUX_MODE_GPIO		0
#define	MUX_MODE_RMII		1
#define	MUX_MODE_RMII_CLKIN	2
#define	MUX_MODE_VBUS_EN_OTG	2

#define	PUS_22_KOHM_PULL_UP	(3 << 4)
#define	DSE_25_OHM		(6 << 6)

#define	NET0_PAD_START	45
#define	NET1_PAD_START	54
#define	NET_PAD_N	9

struct iomuxc_softc {
	struct resource		*tmr_res[1];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	device_t		dev;
};

static struct resource_spec iomuxc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
iomuxc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,mvf600-iomuxc"))
		return (ENXIO);

	device_set_desc(dev, "Vybrid Family IOMUXC Unit");
	return (BUS_PROBE_DEFAULT);
}

static int
configure_pad(struct iomuxc_softc *sc, int pad, int mux_mode)
{
	int reg;

	reg = READ4(sc, pad);
	reg &= ~(MUX_MODE_MASK << MUX_MODE_SHIFT);
	reg |= (mux_mode << MUX_MODE_SHIFT);
	WRITE4(sc, pad, reg);

	return (0);
}

static int
iomuxc_attach(device_t dev)
{
	struct iomuxc_softc *sc;
	int reg;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, iomuxc_spec, sc->tmr_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->tmr_res[0]);
	sc->bsh = rman_get_bushandle(sc->tmr_res[0]);

	/* USB */
	configure_pad(sc, IOMUXC_PTA17, MUX_MODE_VBUS_EN_OTG);
	reg = (PKE | PUE | PUS_22_KOHM_PULL_UP | DSE_25_OHM | OBE);
	WRITE4(sc, IOMUXC_PTA7, reg);

	/* NET */
	configure_pad(sc, IOMUXC_PTA6, MUX_MODE_RMII_CLKIN);

	/* NET0 */
	for (i = NET0_PAD_START; i <= (NET0_PAD_START + NET_PAD_N); i++) {
		configure_pad(sc, IOMUXC(i), MUX_MODE_RMII);
	}

	/* NET1 */
	for (i = NET1_PAD_START; i <= (NET1_PAD_START + NET_PAD_N); i++) {
		configure_pad(sc, IOMUXC(i), MUX_MODE_RMII);
	}

	return (0);
}

static device_method_t iomuxc_methods[] = {
	DEVMETHOD(device_probe,		iomuxc_probe),
	DEVMETHOD(device_attach,	iomuxc_attach),
	{ 0, 0 }
};

static driver_t iomuxc_driver = {
	"iomuxc",
	iomuxc_methods,
	sizeof(struct iomuxc_softc),
};

static devclass_t iomuxc_devclass;

DRIVER_MODULE(iomuxc, simplebus, iomuxc_driver, iomuxc_devclass, 0, 0);
