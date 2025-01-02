/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2024 Pierre-Luc Drouin <pldrouin@pldrouin.net>
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
 * Originally based on Chapter 48, Vybrid Reference Manual, Rev. 5, 07/2013
 * Currently based on Chapter 21, LX2160A Reference Manual, Rev. 1, 10/2021
 *
 * The current implementation is based on the original driver by Ruslan Bukin,
 * later modified by Dawid Górecki, and split into FDT and ACPI drivers by Val
 * Packett.
 */

#include <sys/types.h>
#include <sys/mutex.h>
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

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/iicbus/controller/vybrid/vf_i2c.h>

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

#define DIV_REG_UNSET	0xFF

#define	READ1(_sc, _reg) bus_space_read_1(_sc->bst, _sc->bsh, _reg)
#define	WRITE1(_sc, _reg, _val)	bus_space_write_1(_sc->bst,\
		_sc->bsh, _reg, _val)

#ifdef DEBUG
#define vf_i2c_dbg(_sc, fmt, args...) \
	device_printf((_sc)->dev, fmt, ##args)
#ifdef DEBUG2
#undef WRITE1
#define WRITE1(_sc, _reg, _val) ({\
		vf_i2c_dbg(_sc, "WRITE1 REG 0x%02X VAL 0x%02X\n",_reg,_val);\
		bus_space_write_1(_sc->bst, _sc->bsh, _reg, _val);\
		})
#undef READ1
#define READ1(_sc, _reg) ({\
		uint32_t ret=bus_space_read_1(_sc->bst, _sc->bsh, _reg);\
		vf_i2c_dbg(_sc, "READ1 REG 0x%02X RETURNS 0x%02X\n",_reg,ret);\
		ret;\
		})
#endif
#else
#define vf_i2c_dbg(_sc, fmt, args...)
#endif

static int i2c_repeated_start(device_t, u_char, int);
static int i2c_start(device_t, u_char, int);
static int i2c_stop(device_t);
static int i2c_reset(device_t, u_char, u_char, u_char *);
static int i2c_read(device_t, char *, int, int *, int, int);
static int i2c_write(device_t, const char *, int, int *, int);

struct i2c_div_type {
	uint32_t reg_val;
	uint32_t div;
};

static struct resource_spec i2c_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

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
	{ 0x3C, 2304 }, { 0x3D, 2560 }, { 0x3E, 3072 }, { 0x3F, 3840 },
	{ 0x3F, 3840 }, { 0x7B, 4096 }, { 0x7D, 5120 }, { 0x7E, 6144 },
};

int
vf_i2c_attach_common(device_t dev)
{
	struct vf_i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c attach common\n");

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

	mtx_lock(&sc->mutex);

	WRITE1(sc, I2C_IBIC, IBIC_BIIE);

	if (sc->freq == 0) {
		uint8_t div_reg;

		div_reg = READ1(sc, I2C_IBFD);

		if (div_reg != 0x00) {
			sc->freq = UINT32_MAX;
			device_printf(dev, "Using existing bus frequency divider register value (0x%02X).\n", div_reg);
		} else {
			device_printf(dev, "Bus frequency divider value appears unset, defaulting to low I2C bus speed.\n");
		}
	}

	mtx_unlock(&sc->mutex);

	sc->iicbus = device_add_child(dev, "iicbus", DEVICE_UNIT_ANY);

	if (sc->iicbus == NULL) {
		device_printf(dev, "could not add iicbus child");
		mtx_destroy(&sc->mutex);
		bus_release_resources(dev, i2c_spec, sc->res);
		return (ENXIO);
	}

	bus_attach_children(dev);

	return (0);
}

static int
i2c_detach(device_t dev)
{
	struct vf_i2c_softc *sc;
	int error = 0;

	sc = device_get_softc(dev);
	vf_i2c_dbg(sc, "i2c detach\n");

	error = bus_generic_detach(dev);
	if (error != 0) {
		device_printf(dev, "cannot detach child devices.\n");
		return (error);
	}

	mtx_lock(&sc->mutex);

	if (sc->freq == 0) {
		vf_i2c_dbg(sc, "Writing 0x00 to clock divider register\n");
		WRITE1(sc, I2C_IBFD, 0x00);
	}

	bus_release_resources(dev, i2c_spec, sc->res);

	mtx_unlock(&sc->mutex);

	mtx_destroy(&sc->mutex);

	return (0);
}

/* Wait for free bus */
static int
wait_for_nibb(struct vf_i2c_softc *sc)
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
wait_for_icf(struct vf_i2c_softc *sc)
{
	int retry;
	uint8_t ibsr;

	vf_i2c_dbg(sc, "i2c wait for transfer complete + interrupt flag\n");

	retry = 1000;
	while (retry --) {
		ibsr = READ1(sc, I2C_IBSR);

		if (ibsr & IBSR_IBIF) {
			WRITE1(sc, I2C_IBSR, IBSR_IBIF);

			if (ibsr & IBSR_IBAL) {
				WRITE1(sc, I2C_IBSR, IBSR_IBAL);
				return (IIC_EBUSBSY);
			}

			if (ibsr & IBSR_TCF)
				return (IIC_NOERR);
		}
		DELAY(10);
	}

	return (IIC_ETIMEOUT);
}
/* Get ACK bit from last write */
static bool
tx_acked(struct vf_i2c_softc *sc)
{
	vf_i2c_dbg(sc, "i2c get ACK bit from last write\n");

	return (READ1(sc, I2C_IBSR) & IBSR_RXAK) ? false : true;

}

static int
i2c_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct vf_i2c_softc *sc;
	int error;
	int reg;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c repeated start\n");

	mtx_lock(&sc->mutex);

	if ((READ1(sc, I2C_IBSR) & IBSR_IBB) == 0) {
		vf_i2c_dbg(sc, "cant i2c repeat start: bus is no longer busy\n");
		mtx_unlock(&sc->mutex);
		return (IIC_EBUSERR);
	}

	reg = READ1(sc, I2C_IBCR);
	reg |= (IBCR_RSTA | IBCR_IBIE);
	WRITE1(sc, I2C_IBCR, reg);

	/* Write target address - LSB is R/W bit */
	WRITE1(sc, I2C_IBDR, slave);

	error = wait_for_icf(sc);

	if (!tx_acked(sc)) {
		mtx_unlock(&sc->mutex);
		vf_i2c_dbg(sc, "cant i2c repeat start: missing ACK after slave address\n");
		return (IIC_ENOACK);
	}

	mtx_unlock(&sc->mutex);

	if (error != 0)
		return (error);

	return (IIC_NOERR);
}

static int
i2c_start(device_t dev, u_char slave, int timeout)
{
	struct vf_i2c_softc *sc;
	int error;
	int reg;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c start\n");

	mtx_lock(&sc->mutex);

	error = wait_for_nibb(sc);

	/* Reset controller if bus is still busy. */
	if (error == IIC_ETIMEOUT) {
		WRITE1(sc, I2C_IBCR, IBCR_MDIS);
		DELAY(1000);
		WRITE1(sc, I2C_IBCR, IBCR_NOACK);
		error = wait_for_nibb(sc);
	}

	if (error != 0) {
		mtx_unlock(&sc->mutex);
		vf_i2c_dbg(sc, "cant i2c start: %i\n", error);
		return (error);
	}

	/* Set start condition */
	reg = (IBCR_MSSL | IBCR_NOACK | IBCR_IBIE | IBCR_TXRX);
	WRITE1(sc, I2C_IBCR, reg);

	WRITE1(sc, I2C_IBSR, IBSR_IBIF);

	/* Write target address - LSB is R/W bit */
	WRITE1(sc, I2C_IBDR, slave);

	error = wait_for_icf(sc);
	if (error != 0) {
		mtx_unlock(&sc->mutex);
		vf_i2c_dbg(sc, "cant i2c start: iif error\n");
		return (error);
	}
	mtx_unlock(&sc->mutex);

	if (!tx_acked(sc)) {
		vf_i2c_dbg(sc, "cant i2c start: missing ACK after slave address\n");
		return (IIC_ENOACK);
	}

	return (IIC_NOERR);
}

static int
i2c_stop(device_t dev)
{
	struct vf_i2c_softc *sc;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c stop\n");

	mtx_lock(&sc->mutex);

	if ((READ1(sc, I2C_IBCR) & IBCR_MSSL) != 0)
		WRITE1(sc, I2C_IBCR, IBCR_NOACK | IBCR_IBIE);

	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static uint8_t
i2c_get_div_val(device_t dev)
{
	struct vf_i2c_softc *sc;
	uint8_t div_reg = DIV_REG_UNSET;

	sc = device_get_softc(dev);

	if (sc->freq == UINT32_MAX)
		return div_reg;
#ifndef FDT
	div_reg = vf610_div_table[nitems(vf610_div_table) - 1].reg_val;
#else
	if (sc->hwtype == HW_MVF600)
		div_reg = MVF600_DIV_REG;
	else if (sc->freq == 0)
		div_reg = vf610_div_table[nitems(vf610_div_table) - 1].reg_val;
	else {
		uint64_t clk_freq;
		int error, i;

		error = clk_get_freq(sc->clock, &clk_freq);
		if (error != 0) {
			device_printf(dev, "Could not get parent clock frequency. "
					"Using default divider.\n");
			div_reg = vf610_div_table[nitems(vf610_div_table) - 1].reg_val;
		} else {

			for (i = 0; i < nitems(vf610_div_table) - 1; i++)
				if ((clk_freq / vf610_div_table[i].div) <= sc->freq)
					break;
			div_reg = vf610_div_table[i].reg_val;
		}
	}
#endif
	vf_i2c_dbg(sc, "Writing 0x%02X to clock divider register\n", div_reg);
	return div_reg;
}

static int
i2c_reset(device_t dev, u_char speed, u_char addr, u_char *oldadr)
{
	struct vf_i2c_softc *sc;
	uint8_t div_reg;

	sc = device_get_softc(dev);
	div_reg = i2c_get_div_val(dev);
	vf_i2c_dbg(sc, "i2c reset\n");

	mtx_lock(&sc->mutex);
	WRITE1(sc, I2C_IBCR, IBCR_MDIS);

	if(div_reg != DIV_REG_UNSET)
		WRITE1(sc, I2C_IBFD, div_reg);

	WRITE1(sc, I2C_IBCR, 0x0); /* Enable i2c */

	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct vf_i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c read\n");

	*read = 0;

	mtx_lock(&sc->mutex);

	if (len) {
		if (len == 1)
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL | IBCR_NOACK);
		else
			WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL);

		/* dummy read */
		READ1(sc, I2C_IBDR);

		while (*read < len) {
			error = wait_for_icf(sc);
			if (error != 0) {
				mtx_unlock(&sc->mutex);
				return (error);
			}

			if (last) {
				if (*read == len - 2) {
					/* NO ACK on last byte */
					WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_MSSL | IBCR_NOACK);

				} else if (*read == len - 1) {
					/* Transfer done, remove master bit */
					WRITE1(sc, I2C_IBCR, IBCR_IBIE | IBCR_NOACK);
				}
			}

			*buf++ = READ1(sc, I2C_IBDR);
			(*read)++;
		}
	}
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
i2c_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct vf_i2c_softc *sc;
	int error;

	sc = device_get_softc(dev);

	vf_i2c_dbg(sc, "i2c write\n");

	*sent = 0;

	mtx_lock(&sc->mutex);
	while (*sent < len) {
		WRITE1(sc, I2C_IBDR, *buf++);

		error = wait_for_icf(sc);
		if (error != 0) {
			mtx_unlock(&sc->mutex);
			return (error);
		}

		if (!tx_acked(sc) && (*sent  = (len - 2)) ){
			mtx_unlock(&sc->mutex);
			vf_i2c_dbg(sc, "no ACK on %d write\n", *sent);
			return (IIC_ENOACK);
		}

		(*sent)++;
	}
	mtx_unlock(&sc->mutex);
	return (IIC_NOERR);
}

static device_method_t i2c_methods[] = {
	/* Device interface */
	DEVMETHOD(device_detach,		i2c_detach),

	/* Device interface */
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,		bus_generic_adjust_resource),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,		iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start,	i2c_repeated_start),
	DEVMETHOD(iicbus_start,			i2c_start),
	DEVMETHOD(iicbus_stop,			i2c_stop),
	DEVMETHOD(iicbus_reset,			i2c_reset),
	DEVMETHOD(iicbus_read,			i2c_read),
	DEVMETHOD(iicbus_write,			i2c_write),
	DEVMETHOD(iicbus_transfer,		iicbus_transfer_gen),
	DEVMETHOD_END
};

driver_t vf_i2c_driver = {
	"i2c",
	i2c_methods,
	sizeof(struct vf_i2c_softc),
};
