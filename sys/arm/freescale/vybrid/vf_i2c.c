/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
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
 * Vybrid Family Inter-Integrated Circuit (I2C)
 * Chapter 48, Vybrid Reference Manual, Rev. 5, 07/2013
 */

/*
 * This driver is based on the I2C driver for i.MX
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

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#ifdef EXT_RESOURCES
#include <dev/extres/clk/clk.h>
#endif

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <arm/freescale/vybrid/vf_common.h>

#define	I2C_IBAD	0x0	/* I2C Bus Address Register */
#define	I2C_IBFD	0x1	/* I2C Bus Frequency Divider Register */
#define	I2C_IBCR	0x2	/* I2C Bus Control Register */
#define	 IBCR_MDIS		(1 << 7) /* Module disable. */
#define	 IBCR_IBIE		(1 << 6) /* I-Bus Interrupt Enable. */
#define	 IBCR_MSSL		(1 << 5) /* Master/Slave mode select. */
#define	 IBCR_TXRX		(1 << 4) /* Transmit/Receive mode select. */
#define	 IBCR_NOACK		(1 << 3) /* Data Acknowledge disable. */
#define	 IBCR_RSTA		(1 << 2) /* Repeat Start. */
#define	 IBCR_DMAEN		(1 << 1) /* DMA Enable. */
#define	I2C_IBSR	0x3	/* I2C Bus Status Register */
#define	 IBSR_TCF		(1 << 7) /* Transfer complete. */
#define	 IBSR_IAAS		(1 << 6) /* Addressed as a slave. */
#define	 IBSR_IBB		(1 << 5) /* Bus busy. */
#define	 IBSR_IBAL		(1 << 4) /* Arbitration Lost. */
#define	 IBSR_SRW		(1 << 2) /* Slave Read/Write. */
#define	 IBSR_IBIF		(1 << 1) /* I-Bus Interrupt Flag. */
#define	 IBSR_RXAK		(1 << 0) /* Received Acknowledge. */
#define	I2C_IBDR	0x4	/* I2C Bus Data I/O Register */
#define	I2C_IBIC	0x5	/* I2C Bus Interrupt Config Register */
#define	 IBIC_BIIE		(1 << 7) /* Bus Idle Interrupt Enable bit. */
#define	I2C_IBDBG	0x6	/* I2C Bus Debug Register */

#ifdef DEBUG
#define vf_i2c_dbg(_sc, fmt, args...) \
	device_printf((_sc)->dev, fmt, ##args)
#else
#define vf_i2c_dbg(_sc, fmt, args...)
#endif

#define	HW_UNKNOWN	0x00
#define	HW_MVF600	0x01
#define	HW_VF610	0x02

static int i2c_repeated_start(device_t, u_char, int);
static int i2c_start(device_t, u_char, int);
static int i2c_stop(device_t);
static int i2c_reset(device_t, u_char, u_char, u_char *);
static int i2c_read(device_t, char *, int, int *, int, int);
static int i2c_write(device_t, const char *, int, int *, int);
static phandle_t i2c_get_node(device_t, device_t);

struct i2c_div_type {
	uint32_t reg_val;
	uint32_t div;
};

struct i2c_softc {
	struct resource		*res[2];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
#ifdef EXT_RESOURCES
	clk_t			clock;
	uint32_t		freq;
#endif
	device_t		dev;
	device_t		iicbus;
	struct mtx		mutex;
	uintptr_t		hwtype;
};

static struct resource_spec i2c_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#ifdef EXT_RESOURCES
static struct i2c_div_type vf610_div_table[] = {
	{ 0x00, 20 }, { 0x01, 22 }, { 0x02, 24 }, { 0x03, 26 },
	{ 0x04, 28 }, { 0x05, 30 }, { 0x09, 32 }, { 0x06, 34 },
	{ 0x0A, 36 }, { 0x0B, 40 }, { 0x0C, 44 }, { 0x0D, 48 },
	{ 0x0E, 56 }, { 0x12, 64 }, { 0x13, 72 }, { 0x14, 80 },
	{ 0x15, 88 }, { 0x19, 96 }, { 0x16, 104 }, { 0x1A, 112 },
	{ 0x17, 128 }, { 0x1D, 160 }, { 0x1E, 192 }, { 0x22, 224 },
	{ 0x1F, 240 }, { 0x23, 256 }, { 0x24, 288 }, { 0x25, 320 },
	{ 0x26, 384 }, { 0x2A, 448 }, { 0x27, 480 }, { 0x2B, 512 },
	{ 0x2C, 576 }, { 0x2D, 640 }, { 0x2E, 768 }, { 0x32, 896 },
	{ 0x2F, 960 }, { 0x33, 1024 }, { 0x34, 1152 }, { 0x35, 1280 },
	{ 0x36, 1536 }, { 0x3A, 1792 }, { 0x37, 1920 }, { 0x3B, 2048 },
	{ 0x3C, 2304 }, { 0x3D, 2560 }, { 0x3E, 3072 }, { 0x3F, 3840 }
};
#endif

static const struct ofw_compat_data i2c_compat_data[] = {
	{"fsl,mvf600-i2c",	HW_MVF600},
	{"fsl,vf610-i2c",	HW_VF610},
	{NULL,			HW_UNKNOWN}
};

static int
i2c_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, i2c_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Vybrid Family Inter-Integrated Circuit (I2C)");
	return (BUS_PROBE_DEFAULT);
}

static int
i2c_attach(device_t dev)
{
	struct i2c_softc *sc;
#ifdef EXT_RESOURCES
	phandle_t node;
#endif
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->hwtype = ofw_bus_search_compatible(dev, i2c_compat_data)->ocd_data;
#ifdef EXT_RESOURCES
	node = ofw_bus_get_node(dev);

	error = clk_get_by_ofw_index(dev, node, 0, &sc->clock);
	if (error != 0) {
		sc->freq = 0;
		device_printf(dev, "Parent clock not found.\n");
	} else {
		if (OF_hasprop(node, "clock-frequency"))
			OF_getencprop(node, "clock-frequency", &sc->freq,
			    sizeof(sc->freq));
		else
			sc->freq = 100000;
	}
#endif

	mtx_init(&sc->mutex, device_get_nameunit(dev), "I2C", MTX_DEF);

	error = bus_alloc_resources(dev, i2c_spec, sc->res);
	if (error != 0) {
		mtx_destroy(&sc->mutex);
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	WRITE1(sc, I2C_IBIC, IBIC_BIIE);

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "could not add iicbus child");
		mtx_destroy(&sc->mutex);
		bus_release_resources(dev, i2c_spec, sc->res);
		return (ENXIO);
	}

	bus_generic_attach(dev);

	return (0);
}

static int
i2c_detach(device_t dev)
{
	struct i2c_softc *sc;
	int error = 0;

	sc = device_get_softc(dev);

	error = bus_generic_detach(dev);
	if (error != 0) {
		device_printf(dev, "cannot detach child devices.\n");
		return (error);
	}

	error = device_delete_child(dev, sc->iicbus);
	if (error != 0) {
		device_printf(dev, "could not delete iicbus child.\n");
		return (error);
	}

	bus_release_resources(dev, i2c_spec, sc->res);

	mtx_destroy(&sc->mutex);

	return (0);
}

/* Wait for transfer interrupt flag */
static int
wait_for_iif(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {
		if (READ1(sc, I2C_IBSR) & IBSR_IBIF) {
			WRITE1(sc, I2C_IBSR, IBSR_IBIF);
			return (IIC_NOERR);
		}
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

/* Wait for free bus */
static int
wait_for_nibb(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {
		if ((READ1(sc, I2C_IBSR) & IBSR_IBB) == 0)
			return (IIC_NOERR);
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

/* Wait for transfer complete+interrupt flag */
static int
wait_for_icf(struct i2c_softc *sc)
{
	int retry;

	retry = 1000;
	while (retry --) {
		if (READ1(sc, I2C_IBSR) & IBSR_TCF) {
			if (READ1(sc, I2C_IBSR) & IBSR_IBIF) {
				WRITE1(sc, I2C_IBSR, IBSR_IBIF);
				return (IIC_NOERR);
			}
		}
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}

static int
i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;
	int reg;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c repeated start\n");

	mtx_lock(&sc->mutex);

	WRITE1(sc, I2C_IBAD, slave);

	if ((READ1(sc, I2C_IBSR) & IBSR_IBB) == 0) {
		mtx_unlock(&sc->mutex);
		return (IIC_EBUSERR);
	}

	/* Set repeated start condition */
	DELAY(10);

	reg = READ1(sc, I2C_IBCR);
	reg |= (IBCR_RSTA | IBCR_IBIE);
	WRITE1(sc, I2C_IBCR, reg);

	DELAY(10);

	/* Write target address - LSB is R/W bit */
	WRITE1(sc, I2C_IBDR, slave);

	error = wait_for_iif(sc);

	mtx_unlock(&sc->mutex);

	if (error != 0)
		return (error);

	return (IIC_NOERR);
}

static int
i2c_start(device_t dev, u_char slave, int timeout)
{
	struct i2c_softc *sc;
	int error;
	int reg;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c start\n");

	mtx_lock(&sc->mutex);

	WRITE1(sc, I2C_IBAD, slave);

	if (READ1(sc, I2C_IBSR) & IBSR_IBB) {
		mtx_unlock(&sc->mutex);
		vf_i2c_dbg(sc, "cant i2c start: IIC_EBUSBSY\n");
		return (IIC_EBUSERR);
	}

	/* Set start condition */
	reg = (IBCR_MSSL | IBCR_NOACK | IBCR_IBIE);
	WRITE1(sc, I2C_IBCR, reg);

	DELAY(100);

	reg |= (IBCR_TXRX);
	WRITE1(sc, I2C_IBCR, reg);

	/* Write target address - LSB is R/W bit */
	WRITE1(sc, I2C_IBDR, slave);

	error = wait_for_iif(sc);

	mtx_unlock(&sc->mutex);
	if (error != 0) {
		vf_i2c_dbg(sc, "cant i2c start: iif error\n");
		return (error);
	}

	return (IIC_NOERR);
}

static int
i2c_stop(device_t dev)
{
	struct i2c_softc *sc;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c stop\n");

	mtx_lock(&sc->mutex);

	WRITE1(sc, I2C_IBCR, IBCR_NOACK | IBCR_IBIE);

	DELAY(100);

	/* Reset controller if bus still busy after STOP */
	if (wait_for_nibb(sc) == IIC_ETIMEOUT) {
		WRITE1(sc, I2C_IBCR, IBCR_MDIS);
		DELAY(1000);
		WRITE1(sc, I2C_IBCR, IBCR_NOACK);
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static uint32_t
i2c_get_div_val(device_t dev)
{
	struct i2c_softc *sc;
#ifdef EXT_RESOURCES
	uint64_t clk_freq;
	int error, i;

	sc = device_get_softc(dev);

	if (sc->hwtype == HW_MVF600)
		return 20;

	if (sc->freq == 0)
		return vf610_div_table[nitems(vf610_div_table) - 1].reg_val;

	error = clk_get_freq(sc->clock, &clk_freq);
	if (error != 0) {
		device_printf(dev, "Could not get parent clock frequency. "
		    "Using default divider.\n");
		return vf610_div_table[nitems(vf610_div_table) - 1].reg_val;
	}

	for (i = 0; i < nitems(vf610_div_table) - 1; i++)
		if ((clk_freq / vf610_div_table[i].div) <= sc->freq)
			break;

	return vf610_div_table[i].reg_val;
#else
	sc = device_get_softc(dev);

	if (sc->hwtype == HW_VF610)
		return 0x3F;
	else
		return 20;
#endif
}

static int
i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldadr)
{
	struct i2c_softc *sc;
	uint32_t div;

	sc = device_get_softc(dev);
	div = i2c_get_div_val(dev);
	vf_i2c_dbg(sc, "Div val: %02x\n", div);

	vf_i2c_dbg(sc, "i2c reset\n");

	switch (speed) {
	case IIC_FAST:
	case IIC_SLOW:
	case IIC_UNKNOWN:
	case IIC_FASTEST:
	default:
		break;
	}

	mtx_lock(&sc->mutex);
	WRITE1(sc, I2C_IBCR, IBCR_MDIS);

	DELAY(1000);

	WRITE1(sc, I2C_IBFD, div);
	WRITE1(sc, I2C_IBCR, 0x0); /* Enable i2c */

	DELAY(1000);

	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c read\n");

	*read = 0;

	mtx_lock(&sc->mutex);

	if (len) {
		if (len == 1)
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL |	\
			    IBCR_NOACK);
		else
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL);

		/* dummy read */
		READ1(sc, I2C_IBDR);
		DELAY(1000);
	}

	while (*read < len) {
		error = wait_for_icf(sc);
		if (error != 0) {
			mtx_unlock(&sc->mutex);
			return (error);
		}

		if ((*read == len - 2) && last) {
			/* NO ACK on last byte */
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL |	\
			    IBCR_NOACK);
		}

		if ((*read == len - 1) && last) {
			/* Transfer done, remove master bit */
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_NOACK);
		}

		*buf++ = READ1(sc, I2C_IBDR);
		(*read)++;
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c write\n");

	*sent = 0;

	mtx_lock(&sc->mutex);
	while (*sent < len) {
		WRITE1(sc, I2C_IBDR, *buf++);

		error = wait_for_iif(sc);
		if (error != 0) {
			mtx_unlock(&sc->mutex);
			return (error);
		}

		(*sent)++;
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static phandle_t
i2c_get_node(device_t bus, device_t dev)
{

	return ofw_bus_get_node(bus);
}

static device_method_t i2c_methods[] = {
	DEVMETHOD(device_probe,			i2c_probe),
	DEVMETHOD(device_attach,		i2c_attach),
	DEVMETHOD(device_detach,		i2c_detach),

	DEVMETHOD(ofw_bus_get_node,		i2c_get_node),

	DEVMETHOD(iicbus_callback,		iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start,	i2c_repeated_start),
	DEVMETHOD(iicbus_start,			i2c_start),
	DEVMETHOD(iicbus_stop,			i2c_stop),
	DEVMETHOD(iicbus_reset,			i2c_reset),
	DEVMETHOD(iicbus_read,			i2c_read),
	DEVMETHOD(iicbus_write,			i2c_write),
	DEVMETHOD(iicbus_transfer,		iicbus_transfer_gen),
	{ 0, 0 }
};

static driver_t i2c_driver = {
	"i2c",
	i2c_methods,
	sizeof(struct i2c_softc),
};

static devclass_t i2c_devclass;

DRIVER_MODULE(i2c, simplebus, i2c_driver, i2c_devclass, 0, 0);
DRIVER_MODULE(iicbus, i2c, iicbus_driver, iicbus_devclass, 0, 0);
DRIVER_MODULE(ofw_iicbus, i2c, ofw_iicbus_driver, ofw_iicbus_devclass, 0, 0);
