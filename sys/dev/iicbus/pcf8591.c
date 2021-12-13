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
 * Driver for PCF8591 I2C 8-bit ADC and DAC.
 */
#define	CTRL_CH_SELECT_MASK			0x03
#define	CTRL_AUTOINC_EN				0x04
#define	CTRL_CH_CONFIG_MASK			0x30
#define		CTRL_CH_CONFIG_4_SINGLE		0x00
#define		CTRL_CH_CONFIG_3_DIFF		0x10
#define		CTRL_CH_CONFIG_2_SINGLE_1_DIFF	0x20
#define		CTRL_CH_CONFIG_2_DIFF		0x30
#define	CTRL_OUTPUT_EN				0x40

struct pcf8591_softc {
	device_t		sc_dev;
	int			sc_ch_count;
	uint8_t			sc_addr;
	uint8_t			sc_cfg;
	uint8_t			sc_output;
};

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "nxp,pcf8591",	true },
	{ NULL,			false }
};
#endif

static int
pcf8591_set_config(device_t dev)
{

	struct iic_msg msg;
	uint8_t data[2];
	struct pcf8591_softc *sc;
	int error;

	sc = device_get_softc(dev);
	data[0] = sc->sc_cfg;
	data[1] = sc->sc_output;
	msg.slave = sc->sc_addr;
	msg.flags = IIC_M_WR;
	msg.len = nitems(data);
	msg.buf = data;

	error = iicbus_transfer_excl(dev, &msg, 1, IIC_INTRWAIT);
	return (error);
}

static int
pcf8591_get_reading(device_t dev, uint8_t *reading)
{
	struct iic_msg msg;
	struct pcf8591_softc *sc;
	int error;

	sc = device_get_softc(dev);

	msg.slave = sc->sc_addr;
	msg.flags = IIC_M_RD;
	msg.len = 1;
	msg.buf = reading;

	error = iicbus_transfer_excl(dev, &msg, 1, IIC_INTRWAIT);
	return (error);
}

static int
pcf8591_select_channel(device_t dev, int channel)
{
	struct pcf8591_softc *sc;
	int error;
	uint8_t unused;

	sc = device_get_softc(dev);
	if (channel >= sc->sc_ch_count)
		return (EINVAL);
	sc->sc_cfg &= ~CTRL_CH_SELECT_MASK;
	sc->sc_cfg += channel;
	error = pcf8591_set_config(dev);
	if (error != 0)
		return (error);

	/*
	 * The next read is still for the old channel,
	 * so do it and discard.
	 */
	error = pcf8591_get_reading(dev, &unused);
	return (error);
}

static int
pcf8591_channel_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int error, channel, val;
	uint8_t reading;

	dev = arg1;
	channel = arg2;

	if (req->oldptr != NULL) {
		error = pcf8591_select_channel(dev, channel);
		if (error != 0)
			return (EIO);
		error = pcf8591_get_reading(dev, &reading);
		if (error != 0)
			return (EIO);
		val = reading;
	}
	error = sysctl_handle_int(oidp, &val, 0, req);
	return (error);
}

static void
pcf8591_start(void *arg)
{
	device_t dev;
	struct pcf8591_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	struct sysctl_oid *inputs_node;
	struct sysctl_oid_list *inputs;

	sc = arg;
	dev = sc->sc_dev;

	/*
	 * Set initial -- and, for the time being, fixed -- configuration.
	 * Channel auto-incrementi is disabled, although it could be more
	 * performant and precise for bulk channel queries.
	 * The inputs are configured as four single channels.
	 * The output is disabled.
	 */
	sc->sc_cfg = 0;
	sc->sc_output = 0;
	sc->sc_ch_count = 4;
	(void)pcf8591_set_config(dev);

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	inputs_node = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO, "inputs",
	    CTLTYPE_NODE, NULL, "Input channels");
	inputs = SYSCTL_CHILDREN(inputs_node);
	for (int i = 0; i < sc->sc_ch_count; i++) {
		char buf[4];

		snprintf(buf, sizeof(buf), "%d", i);
		SYSCTL_ADD_PROC(ctx, inputs, OID_AUTO, buf,
		    CTLTYPE_INT | CTLFLAG_RD, dev, i,
		    pcf8591_channel_sysctl, "I", "Input level from 0 to 255 "
		    "(relative to Vref)");
	}
}

static int
pcf8591_probe(device_t dev)
{
#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
#endif
	device_set_desc(dev, "PCF8591 8-bit ADC / DAC");
#ifdef FDT
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (BUS_PROBE_GENERIC);
#endif
	return (BUS_PROBE_NOWILDCARD);
}

static int
pcf8591_attach(device_t dev)
{
	struct pcf8591_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_addr = iicbus_get_addr(dev);

	/*
	 * We have to wait until interrupts are enabled.  Usually I2C read
	 * and write only works when the interrupts are available.
	 */
	config_intrhook_oneshot(pcf8591_start, sc);
	return (0);
}

static int
pcf8591_detach(device_t dev)
{
	return (0);
}

static device_method_t  pcf8591_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcf8591_probe),
	DEVMETHOD(device_attach,	pcf8591_attach),
	DEVMETHOD(device_detach,	pcf8591_detach),

	DEVMETHOD_END
};

static driver_t pcf8591_driver = {
	"pcf8591",
	pcf8591_methods,
	sizeof(struct pcf8591_softc)
};

static devclass_t pcf8591_devclass;

DRIVER_MODULE(pcf8591, iicbus, pcf8591_driver, pcf8591_devclass, 0, 0);
MODULE_DEPEND(pcf8591, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(pcf8591, 1);
#ifdef FDT
IICBUS_FDT_PNP_INFO(compat_data);
#endif
