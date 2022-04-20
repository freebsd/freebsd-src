/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) Andriy Gapon
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

/*
 * Driver for MAX44009 Ambient Light Sensor with ADC.
 */
#define	REG_LUX_HIGH			0x03
#define	REG_LUX_LOW			0x04

struct max44009_softc {
	device_t		sc_dev;
	uint8_t			sc_addr;
};

#ifdef FDT
static const struct ofw_compat_data compat_data[] = {
        { "maxim,max44009",		true },
	{ NULL,				false },
};
#endif

static int
max44009_get_reading(device_t dev, u_int *reading)
{
	struct iic_msg msgs[4];
	struct max44009_softc *sc;
	u_int val;
	uint8_t reghi, reglo, valhi, vallo;
	int error;

	sc = device_get_softc(dev);

	reghi = REG_LUX_HIGH;
	reglo = REG_LUX_LOW;
	msgs[0].slave = sc->sc_addr;
	msgs[0].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[0].len = 1;
	msgs[0].buf = &reghi;
	msgs[1].slave = sc->sc_addr;
	msgs[1].flags = IIC_M_RD | IIC_M_NOSTOP;
	msgs[1].len = 1;
	msgs[1].buf = &valhi;
	msgs[2].slave = sc->sc_addr;
	msgs[2].flags = IIC_M_WR | IIC_M_NOSTOP;
	msgs[2].len = 1;
	msgs[2].buf = &reglo;
	msgs[3].slave = sc->sc_addr;
	msgs[3].flags = IIC_M_RD;
	msgs[3].len = 1;
	msgs[3].buf = &vallo;

	error = iicbus_transfer_excl(dev, msgs, nitems(msgs), IIC_INTRWAIT);
	if (error != 0)
		return (error);

	val = ((valhi & 0x0f) << 4) | (vallo & 0x0f);
	val <<= (valhi & 0xf0) >> 4;
	val = val * 72 / 100;
	*reading = val;
	return (0);
}

static int
max44009_lux_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	u_int reading;
	int error, val;

	if (req->oldptr != NULL) {
		dev = arg1;
		error = max44009_get_reading(dev, &reading);
		if (error != 0)
			return (EIO);
		val = reading;
	}
	error = sysctl_handle_int(oidp, &val, 0, req);
	return (error);
}

static int
max44009_probe(device_t dev)
{
	int rc;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		rc = BUS_PROBE_GENERIC;
	else
#endif
		rc = BUS_PROBE_NOWILDCARD;
	device_set_desc(dev, "MAX44009 light intensity sensor");
	return (rc);
}

static int
max44009_attach(device_t dev)
{
	struct max44009_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "illuminance",
	    CTLTYPE_INT | CTLFLAG_RD, dev, 0,
	    max44009_lux_sysctl, "I", "Light intensity, lux");
	return (0);
}

static int
max44009_detach(device_t dev)
{
	return (0);
}

static device_method_t  max44009_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		max44009_probe),
	DEVMETHOD(device_attach,	max44009_attach),
	DEVMETHOD(device_detach,	max44009_detach),

	DEVMETHOD_END
};

static driver_t max44009_driver = {
	"max44009",
	max44009_methods,
	sizeof(struct max44009_softc)
};

DRIVER_MODULE(max44009, iicbus, max44009_driver, 0, 0);
MODULE_DEPEND(max44009, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(max44009, 1);
IICBUS_FDT_PNP_INFO(compat_data);
