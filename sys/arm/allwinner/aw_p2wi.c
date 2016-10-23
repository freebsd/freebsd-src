/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Allwinner P2WI (Push-Pull Two Wire Interface)
 * P2WI is a iic-like interface used on sun6i hardware
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <arm/allwinner/aw_p2wi.h>

#include "iicbus_if.h"

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun6i-a31-p2wi",		1 },
	{ NULL,					0 }
};

static struct resource_spec p2wi_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct p2wi_softc {
	struct resource	*res;
	struct mtx	mtx;
	clk_t		clk;
	hwreset_t	rst;
	device_t	iicbus;
};

#define	P2WI_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	P2WI_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	P2WI_READ(sc, reg)		bus_read_4((sc)->res, (reg))
#define	P2WI_WRITE(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

#define	P2WI_RETRY	1000

static phandle_t
p2wi_get_node(device_t bus, device_t dev)
{
	return (ofw_bus_get_node(bus));
}

static int
p2wi_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct p2wi_softc *sc;
	int retry;

	sc = device_get_softc(dev);

	P2WI_LOCK(sc);

	P2WI_WRITE(sc, P2WI_CTRL, P2WI_CTRL_SOFT_RESET);
	for (retry = P2WI_RETRY; retry > 0; retry--)
		if ((P2WI_READ(sc, P2WI_CTRL) & P2WI_CTRL_SOFT_RESET) == 0)
			break;

	P2WI_UNLOCK(sc);

	if (retry == 0) {
		device_printf(dev, "soft reset timeout\n");
		return (ETIMEDOUT);
	}

	return (IIC_ENOADDR);
}

static int
p2wi_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct p2wi_softc *sc;
	int retry, error;
	uint8_t data_len;

	sc = device_get_softc(dev);

	/*
	 * Since P2WI is only used for AXP22x PMIC we only support
	 * two messages of one byte length
	 */
	if (nmsgs != 2 || (msgs[0].flags & IIC_M_RD) == IIC_M_RD ||
	    msgs[0].len != 1 || msgs[1].len != 1)
		return (EINVAL);

	P2WI_LOCK(sc);

	/* Write address */
	P2WI_WRITE(sc, P2WI_DADDR0, msgs[0].buf[0]);

	/* Write Data length/Direction */
	data_len = P2WI_DLEN_LEN(msgs[1].len);
	if ((msgs[1].flags & IIC_M_RD) == 0)
		P2WI_WRITE(sc, P2WI_DATA0, msgs[1].buf[0]);
	else
		data_len |= P2WI_DLEN_READ;
	P2WI_WRITE(sc, P2WI_DLEN, data_len);

	/* Start transfer */
	P2WI_WRITE(sc, P2WI_CTRL, P2WI_CTRL_START_TRANS);

	/* Wait for transfer to complete */
	for (retry = P2WI_RETRY; retry > 0; retry--)
		if ((P2WI_READ(sc, P2WI_CTRL) & P2WI_CTRL_START_TRANS) == 0)
			break;

	if (retry == 0) {
		error = ETIMEDOUT;
		goto done;
	}

	/* Read data if needed */
	if ((msgs[1].flags & IIC_M_RD) == IIC_M_RD) {
		msgs[1].buf[0] = P2WI_READ(sc, P2WI_DATA0) & 0xff;
		msgs[1].len = 1;
	}

	error = 0;

done:
	P2WI_UNLOCK(sc);

	return (error);
}

static int
p2wi_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner P2WI");
	return (BUS_PROBE_DEFAULT);
}

static int
p2wi_attach(device_t dev)
{
	struct p2wi_softc *sc;
	int error;

	sc = device_get_softc(dev);
	mtx_init(&sc->mtx, device_get_nameunit(dev), "p2wi", MTX_DEF);

	if (clk_get_by_ofw_index(dev, 0, 0, &sc->clk) == 0) {
		error = clk_enable(sc->clk);
		if (error != 0) {
			device_printf(dev, "cannot enable clock\n");
			goto fail;
		}
	}
	if (hwreset_get_by_ofw_idx(dev, 0, 0, &sc->rst) == 0) {
		error = hwreset_deassert(sc->rst);
		if (error != 0) {
			device_printf(dev, "cannot de-assert reset\n");
			goto fail;
		}
	}

	if (bus_alloc_resources(dev, p2wi_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		error = ENXIO;
		goto fail;
	}

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "cannot add iicbus child device\n");
		error = ENXIO;
		goto fail;
	}

	/* Disable interrupts */
	P2WI_WRITE(sc, P2WI_INTE, 0x0);

	bus_generic_attach(dev);

	return (0);

fail:
	bus_release_resources(dev, p2wi_spec, &sc->res);
	if (sc->rst != NULL)
		hwreset_release(sc->rst);
	if (sc->clk != NULL)
		clk_release(sc->clk);
	mtx_destroy(&sc->mtx);
	return (error);
}

static device_method_t p2wi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		p2wi_probe),
	DEVMETHOD(device_attach,	p2wi_attach),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	/* OFW methods */
	DEVMETHOD(ofw_bus_get_node,	p2wi_get_node),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	iicbus_null_callback),
	DEVMETHOD(iicbus_reset,		p2wi_reset),
	DEVMETHOD(iicbus_transfer,	p2wi_transfer),

	DEVMETHOD_END
};

static driver_t p2wi_driver = {
	"iichb",
	p2wi_methods,
	sizeof(struct p2wi_softc),
};

static devclass_t p2wi_devclass;

EARLY_DRIVER_MODULE(iicbus, p2wi, iicbus_driver, iicbus_devclass, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(p2wi, simplebus, p2wi_driver, p2wi_devclass, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(p2wi, 1);
