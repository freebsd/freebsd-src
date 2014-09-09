/*-
 * Copyright (c) 2014 Boris Samorodov <bsam@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/freescale/imx/imx_iomuxvar.h>
#include "imx6_iomuxreg.h"

#define	IOMUX_WRITE(_sc, _r, _v) \
	bus_write_4((_sc)->sc_res, (_r), (_v))
#define	IOMUX_READ(_sc, _r) \
	bus_read_4((_sc)->sc_res, (_r))
#define	IOMUX_SET(_sc, _r, _m) \
	IOMUX_WRITE((_sc), (_r), IOMUX_READ((_sc), (_r)) | (_m))
#define	IOMUX_CLEAR(_sc, _r, _m) \
	IOMUX_WRITE((_sc), (_r), IOMUX_READ((_sc), (_r)) & ~(_m))

struct imx6_iomux_softc {
	struct resource *sc_res;
	device_t sc_dev;
};

static struct imx6_iomux_softc *iomuxsc = NULL;

static struct resource_spec imx6_iomux_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
imx6_iomux_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,imx6-iomux"))
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX6 IO pins multiplexor");
	return (BUS_PROBE_DEFAULT);

}

static int
imx6_iomux_attach(device_t dev)
{
	struct imx6_iomux_softc * sc;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, imx6_iomux_spec, &sc->sc_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	iomuxsc = sc;

	/*
	 * XXX: place to fetch all info about pinmuxing from loader data
	 * (FDT blob) and apply. Loader (1st one) must care about
	 * device-to-device difference.
	 */

	return (0);
}

static int
imx6_iomux_detach(device_t dev)
{

	/* IOMUX registers are always accessible. */
	return (EBUSY);
}

static void
iomux_set_pad_sub(struct imx6_iomux_softc *sc, uint32_t pin, uint32_t config)
{
	bus_size_t pad_ctl_reg = IOMUX_PIN_TO_PAD_ADDRESS(pin);

	if (pad_ctl_reg != IOMUX_PAD_NONE)
		IOMUX_WRITE(sc, pad_ctl_reg, config);
}

void
iomux_set_pad(unsigned int pin, unsigned int config)
{

	if (iomuxsc == NULL)
		return;
	iomux_set_pad_sub(iomuxsc, pin, config);
}

static void
iomux_set_function_sub(struct imx6_iomux_softc *sc, uint32_t pin, uint32_t fn)
{
	bus_size_t mux_ctl_reg = IOMUX_PIN_TO_MUX_ADDRESS(pin);

	if (mux_ctl_reg != IOMUX_MUX_NONE)
		IOMUX_WRITE(sc, mux_ctl_reg, fn);
}

void
iomux_set_function(unsigned int pin, unsigned int fn)
{

	if (iomuxsc == NULL)
		return;
	iomux_set_function_sub(iomuxsc, pin, fn);
}

static uint32_t
iomux_get_pad_config_sub(struct imx6_iomux_softc *sc, uint32_t pin)
{
	bus_size_t pad_reg = IOMUX_PIN_TO_PAD_ADDRESS(pin);
	uint32_t result;

	result = IOMUX_READ(sc, pad_reg);

	return(result);
}

unsigned int
iomux_get_pad_config(unsigned int pin)
{

	return(iomux_get_pad_config_sub(iomuxsc, pin));
}


uint32_t
imx_iomux_gpr_get(u_int regnum)
{

	KASSERT(iomuxsc != NULL, ("imx_iomux_gpr_get() called before attach"));
	KASSERT(regnum >= 0 && regnum <= 13, 
	    ("imx_iomux_gpr_get bad regnum %u", regnum));
	return (IOMUX_READ(iomuxsc, IOMUXC_GPR0 + regnum));
}

void
imx_iomux_gpr_set(u_int regnum, uint32_t val)
{

	KASSERT(iomuxsc != NULL, ("imx_iomux_gpr_set() called before attach"));
	KASSERT(regnum >= 0 && regnum <= 13, 
	    ("imx_iomux_gpr_set bad regnum %u", regnum));
	IOMUX_WRITE(iomuxsc, IOMUXC_GPR0 + regnum, val);
}

void
imx_iomux_gpr_set_masked(u_int regnum, uint32_t clrbits, uint32_t setbits)
{
	uint32_t val;

	KASSERT(iomuxsc != NULL, 
	    ("imx_iomux_gpr_set_masked called before attach"));
	KASSERT(regnum >= 0 && regnum <= 13, 
	    ("imx_iomux_gpr_set_masked bad regnum %u", regnum));

	val = IOMUX_READ(iomuxsc, IOMUXC_GPR0 + regnum);
	val = (val & ~clrbits) | setbits;
	IOMUX_WRITE(iomuxsc, IOMUXC_GPR0 + regnum, val);
}

static device_method_t imx6_iomux_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         imx6_iomux_probe),
	DEVMETHOD(device_attach,        imx6_iomux_attach),
	DEVMETHOD(device_detach,        imx6_iomux_detach),

	DEVMETHOD_END
};

static driver_t imx6_iomux_driver = {
	"imx6_iomux",
	imx6_iomux_methods,
	sizeof(struct imx6_iomux_softc),
};

static devclass_t imx6_iomux_devclass;

EARLY_DRIVER_MODULE(imx6_iomux, simplebus, imx6_iomux_driver, 
    imx6_iomux_devclass, 0, 0, BUS_PASS_CPU + BUS_PASS_ORDER_LATE);


