/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver for the TWSI (aka I2C, aka IIC) bus controller found on Marvell
 * SoCs. Supports master operation only, and works in polling mode.
 *
 * Calls to DELAY() are needed per Application Note AN-179 "TWSI Software
 * Guidelines for Discovery(TM), Horizon (TM) and Feroceon(TM) Devices".
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <machine/_inttypes.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include "iicbus_if.h"

#define MV_TWSI_NAME		"twsi"
#define	IICBUS_DEVNAME		"iicbus"

#define TWSI_SLAVE_ADDR		0x00
#define TWSI_EXT_SLAVE_ADDR	0x10
#define TWSI_DATA		0x04

#define TWSI_CONTROL		0x08
#define TWSI_CONTROL_ACK	(1 << 2)
#define TWSI_CONTROL_IFLG	(1 << 3)
#define TWSI_CONTROL_STOP	(1 << 4)
#define TWSI_CONTROL_START	(1 << 5)
#define TWSI_CONTROL_TWSIEN	(1 << 6)
#define TWSI_CONTROL_INTEN	(1 << 7)

#define TWSI_STATUS			0x0c
#define TWSI_STATUS_START		0x08
#define TWSI_STATUS_RPTD_START		0x10
#define TWSI_STATUS_ADDR_W_ACK		0x18
#define TWSI_STATUS_DATA_WR_ACK		0x28
#define TWSI_STATUS_ADDR_R_ACK		0x40
#define TWSI_STATUS_DATA_RD_ACK		0x50
#define TWSI_STATUS_DATA_RD_NOACK	0x58

#define TWSI_BAUD_RATE		0x0c
#define	TWSI_BAUD_RATE_PARAM(M,N)	((((M) << 3) | ((N) & 0x7)) & 0x7f)
#define	TWSI_BAUD_RATE_RAW(C,M,N)	((C)/((10*(M+1))<<(N+1)))
#define	TWSI_BAUD_RATE_SLOW		50000	/* 50kHz */
#define	TWSI_BAUD_RATE_FAST		100000	/* 100kHz */

#define TWSI_SOFT_RESET		0x1c

#define TWSI_DEBUG
#undef TWSI_DEBUG

#ifdef  TWSI_DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__); printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

struct mv_twsi_softc {
	device_t	dev;
	struct resource	*res[1];	/* SYS_RES_MEMORY */
	struct mtx	mutex;
	device_t	iicbus;
};

static struct mv_twsi_baud_rate {
	uint32_t	raw;
	int		param;
	int		m;
	int		n;
} baud_rate[IIC_FASTEST + 1];

static int mv_twsi_probe(device_t);
static int mv_twsi_attach(device_t);
static int mv_twsi_detach(device_t);

static int mv_twsi_reset(device_t dev, u_char speed, u_char addr,
    u_char *oldaddr);
static int mv_twsi_repeated_start(device_t dev, u_char slave, int timeout);
static int mv_twsi_start(device_t dev, u_char slave, int timeout);
static int mv_twsi_stop(device_t dev);
static int mv_twsi_read(device_t dev, char *buf, int len, int *read, int last,
    int delay);
static int mv_twsi_write(device_t dev, const char *buf, int len, int *sent,
    int timeout);

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ -1, 0 }
};

static device_method_t mv_twsi_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mv_twsi_probe),
	DEVMETHOD(device_attach,	mv_twsi_attach),
	DEVMETHOD(device_detach,	mv_twsi_detach),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback, iicbus_null_callback),
	DEVMETHOD(iicbus_repeated_start, mv_twsi_repeated_start),
	DEVMETHOD(iicbus_start,		mv_twsi_start),
	DEVMETHOD(iicbus_stop,		mv_twsi_stop),
	DEVMETHOD(iicbus_write,		mv_twsi_write),
	DEVMETHOD(iicbus_read,		mv_twsi_read),
	DEVMETHOD(iicbus_reset,		mv_twsi_reset),
	DEVMETHOD(iicbus_transfer,	iicbus_transfer_gen),
	{ 0, 0 }
};

static devclass_t mv_twsi_devclass;

static driver_t mv_twsi_driver = {
	MV_TWSI_NAME,
	mv_twsi_methods,
	sizeof(struct mv_twsi_softc),
};

DRIVER_MODULE(twsi, simplebus, mv_twsi_driver, mv_twsi_devclass, 0, 0);
DRIVER_MODULE(iicbus, twsi, iicbus_driver, iicbus_devclass, 0, 0);
MODULE_DEPEND(twsi, iicbus, 1, 1, 1);

static __inline uint32_t
TWSI_READ(struct mv_twsi_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->res[0], off));
}

static __inline void
TWSI_WRITE(struct mv_twsi_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->res[0], off, val);
}

static __inline void
twsi_control_clear(struct mv_twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, TWSI_CONTROL);
	val &= ~mask;
	TWSI_WRITE(sc, TWSI_CONTROL, val);
}

static __inline void
twsi_control_set(struct mv_twsi_softc *sc, uint32_t mask)
{
	uint32_t val;

	val = TWSI_READ(sc, TWSI_CONTROL);
	val |= mask;
	TWSI_WRITE(sc, TWSI_CONTROL, val);
}

static __inline void
twsi_clear_iflg(struct mv_twsi_softc *sc)
{

	DELAY(1000);
	twsi_control_clear(sc, TWSI_CONTROL_IFLG);
	DELAY(1000);
}


/*
 * timeout given in us
 * returns
 *   0 on sucessfull mask change
 *   non-zero on timeout
 */
static int
twsi_poll_ctrl(struct mv_twsi_softc *sc, int timeout, uint32_t mask)
{

	timeout /= 10;
	while (!(TWSI_READ(sc, TWSI_CONTROL) & mask)) {
		DELAY(10);
		if (--timeout < 0)
			return (timeout);
	}
	return (0);
}


/*
 * 'timeout' is given in us. Note also that timeout handling is not exact --
 * twsi_locked_start() total wait can be more than 2 x timeout
 * (twsi_poll_ctrl() is called twice). 'mask' can be either TWSI_STATUS_START
 * or TWSI_STATUS_RPTD_START
 */
static int
twsi_locked_start(device_t dev, struct mv_twsi_softc *sc, int32_t mask,
    u_char slave, int timeout)
{
	int read_access, iflg_set = 0;
	uint32_t status;

	mtx_assert(&sc->mutex, MA_OWNED);

	if (mask == TWSI_STATUS_RPTD_START)
		/* read IFLG to know if it should be cleared later; from NBSD */
		iflg_set = TWSI_READ(sc, TWSI_CONTROL) & TWSI_CONTROL_IFLG;

	twsi_control_set(sc, TWSI_CONTROL_START);

	if (mask == TWSI_STATUS_RPTD_START && iflg_set) {
		debugf("IFLG set, clearing\n");
		twsi_clear_iflg(sc);
	}

	/*
	 * Without this delay we timeout checking IFLG if the timeout is 0.
	 * NBSD driver always waits here too.
	 */
	DELAY(1000);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf("timeout sending %sSTART condition\n",
		    mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ETIMEOUT);
	}

	status = TWSI_READ(sc, TWSI_STATUS);
	if (status != mask) {
		debugf("wrong status (%02x) after sending %sSTART condition\n",
		    status, mask == TWSI_STATUS_START ? "" : "repeated ");
		return (IIC_ESTATUS);
	}

	TWSI_WRITE(sc, TWSI_DATA, slave);
	DELAY(1000);
	twsi_clear_iflg(sc);

	if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
		debugf("timeout sending slave address\n");
		return (IIC_ETIMEOUT);
	}
	
	read_access = (slave & 0x1) ? 1 : 0;
	status = TWSI_READ(sc, TWSI_STATUS);
	if (status != (read_access ?
	    TWSI_STATUS_ADDR_R_ACK : TWSI_STATUS_ADDR_W_ACK)) {
		debugf("no ACK (status: %02x) after sending slave address\n",
		    status);
		return (IIC_ENOACK);
	}

	return (IIC_NOERR);
}

static int
mv_twsi_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "mrvl,twsi"))
		return (ENXIO);

	device_set_desc(dev, "Marvell Integrated I2C Bus Controller");
	return (BUS_PROBE_DEFAULT);
}

#define	ABSSUB(a,b)	(((a) > (b)) ? (a) - (b) : (b) - (a))
static void
mv_twsi_cal_baud_rate(const uint32_t target, struct mv_twsi_baud_rate *rate)
{
	uint32_t clk, cur, diff, diff0;
	int m, n, m0, n0;

	/* Calculate baud rate. */
	m0 = n0 = 4;	/* Default values on reset */
	diff0 = 0xffffffff;
	clk = get_tclk();

	for (n = 0; n < 8; n++) {
		for (m = 0; m < 16; m++) {
			cur = TWSI_BAUD_RATE_RAW(clk,m,n);
			diff = ABSSUB(target, cur);
			if (diff < diff0) {
				m0 = m;
				n0 = n;
				diff0 = diff;
			}
		}
	}
	rate->raw = TWSI_BAUD_RATE_RAW(clk, m0, n0);
	rate->param = TWSI_BAUD_RATE_PARAM(m0, n0);
	rate->m = m0;
	rate->n = n0;
}

static int
mv_twsi_attach(device_t dev)
{
	struct mv_twsi_softc *sc;
	phandle_t child, iicbusnode;
	device_t childdev;
	struct iicbus_ivar *devi;
	char dname[32];	/* 32 is taken from struct u_device */
	uint32_t paddr;
	int len, error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	bzero(baud_rate, sizeof(baud_rate));

	mtx_init(&sc->mutex, device_get_nameunit(dev), MV_TWSI_NAME, MTX_DEF);

	/* Allocate IO resources */
	if (bus_alloc_resources(dev, res_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		mv_twsi_detach(dev);
		return (ENXIO);
	}

	mv_twsi_cal_baud_rate(TWSI_BAUD_RATE_SLOW, &baud_rate[IIC_SLOW]);
	mv_twsi_cal_baud_rate(TWSI_BAUD_RATE_FAST, &baud_rate[IIC_FAST]);
	if (bootverbose)
		device_printf(dev, "calculated baud rates are:\n"
		    " %" PRIu32 " kHz (M=%d, N=%d) for slow,\n"
		    " %" PRIu32 " kHz (M=%d, N=%d) for fast.\n",
		    baud_rate[IIC_SLOW].raw / 1000,
		    baud_rate[IIC_SLOW].m,
		    baud_rate[IIC_SLOW].n,
		    baud_rate[IIC_FAST].raw / 1000,
		    baud_rate[IIC_FAST].m,
		    baud_rate[IIC_FAST].n);

	sc->iicbus = device_add_child(dev, IICBUS_DEVNAME, -1);
	if (sc->iicbus == NULL) {
		device_printf(dev, "could not add iicbus child\n");
		mv_twsi_detach(dev);
		return (ENXIO);
	}
	/* Attach iicbus. */
	bus_generic_attach(dev);

	iicbusnode = 0;
	/* Find iicbus as the child devices in the device tree. */
	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child)) {
		len = OF_getproplen(child, "model");
		if (len <= 0 || len > sizeof(dname) - 1)
			continue;
		error = OF_getprop(child, "model", &dname, len);
		dname[len + 1] = '\0';
		if (error == -1)
			continue;
		len = strlen(dname);
		if (len == strlen(IICBUS_DEVNAME) &&
		    strncasecmp(dname, IICBUS_DEVNAME, len) == 0) {
			iicbusnode = child;
			break; 
		}
	}
	if (iicbusnode == 0)
		goto attach_end;

	/* Attach child devices onto iicbus. */
	for (child = OF_child(iicbusnode); child != 0; child = OF_peer(child)) {
		/* Get slave address. */
		error = OF_getprop(child, "i2c-address", &paddr, sizeof(paddr));
		if (error == -1)
			error = OF_getprop(child, "reg", &paddr, sizeof(paddr));
		if (error == -1)
			continue;

		/* Get device driver name. */
		len = OF_getproplen(child, "model");
		if (len <= 0 || len > sizeof(dname) - 1)
			continue;
		OF_getprop(child, "model", &dname, len);
		dname[len + 1] = '\0';

		if (bootverbose)
			device_printf(dev, "adding a device %s at %d.\n",
			    dname, fdt32_to_cpu(paddr));
		childdev = BUS_ADD_CHILD(sc->iicbus, 0, dname, -1);
		devi = IICBUS_IVAR(childdev);
		devi->addr = fdt32_to_cpu(paddr);
	}

attach_end:
	bus_generic_attach(sc->iicbus);

	return (0);
}

static int
mv_twsi_detach(device_t dev)
{
	struct mv_twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if ((rv = bus_generic_detach(dev)) != 0)
		return (rv);

	if (sc->iicbus != NULL)
		if ((rv = device_delete_child(dev, sc->iicbus)) != 0)
			return (rv);

	bus_release_resources(dev, res_spec, sc->res);

	mtx_destroy(&sc->mutex);
	return (0);
}

/*
 * Only slave mode supported, disregard [old]addr
 */
static int
mv_twsi_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct mv_twsi_softc *sc;
	uint32_t param;

	sc = device_get_softc(dev);

	switch (speed) {
	case IIC_SLOW:
	case IIC_FAST:
		param = baud_rate[speed].param;
		break;
	case IIC_FASTEST:
	case IIC_UNKNOWN:
	default:
		param = baud_rate[IIC_FAST].param;
		break;
	}

	mtx_lock(&sc->mutex);
	TWSI_WRITE(sc, TWSI_SOFT_RESET, 0x0);
	DELAY(2000);
	TWSI_WRITE(sc, TWSI_BAUD_RATE, param);
	TWSI_WRITE(sc, TWSI_CONTROL, TWSI_CONTROL_TWSIEN | TWSI_CONTROL_ACK);
	DELAY(1000);
	mtx_unlock(&sc->mutex);

	return (0);
}

/*
 * timeout is given in us
 */
static int
mv_twsi_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct mv_twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	rv = twsi_locked_start(dev, sc, TWSI_STATUS_RPTD_START, slave,
	    timeout);
	mtx_unlock(&sc->mutex);

	if (rv) {
		mv_twsi_stop(dev);
		return (rv);
	} else
		return (IIC_NOERR);
}

/*
 * timeout is given in us
 */
static int
mv_twsi_start(device_t dev, u_char slave, int timeout)
{
	struct mv_twsi_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	rv = twsi_locked_start(dev, sc, TWSI_STATUS_START, slave, timeout);
	mtx_unlock(&sc->mutex);

	if (rv) {
		mv_twsi_stop(dev);
		return (rv);
	} else
		return (IIC_NOERR);
}

static int
mv_twsi_stop(device_t dev)
{
	struct mv_twsi_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	twsi_control_set(sc, TWSI_CONTROL_STOP);
	DELAY(1000);
	twsi_clear_iflg(sc);
	mtx_unlock(&sc->mutex);

	return (IIC_NOERR);
}

static int
mv_twsi_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	struct mv_twsi_softc *sc;
	uint32_t status;
	int last_byte, rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	*read = 0;
	while (*read < len) {
		/*
		 * Check if we are reading last byte of the last buffer,
		 * do not send ACK then, per I2C specs
		 */
		last_byte = ((*read == len - 1) && last) ? 1 : 0;
		if (last_byte)
			twsi_control_clear(sc, TWSI_CONTROL_ACK);
		else
			twsi_control_set(sc, TWSI_CONTROL_ACK);

		DELAY (1000);
		twsi_clear_iflg(sc);

		if (twsi_poll_ctrl(sc, delay, TWSI_CONTROL_IFLG)) {
			debugf("timeout reading data\n");
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, TWSI_STATUS);
		if (status != (last_byte ?
		    TWSI_STATUS_DATA_RD_NOACK : TWSI_STATUS_DATA_RD_ACK)) {
			debugf("wrong status (%02x) while reading\n", status);
			rv = IIC_ESTATUS;
			goto out;
		}

		*buf++ = TWSI_READ(sc, TWSI_DATA);
		(*read)++;
	}
	rv = IIC_NOERR;
out:
	mtx_unlock(&sc->mutex);
	return (rv);
}

static int
mv_twsi_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	struct mv_twsi_softc *sc;
	uint32_t status;
	int rv;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mutex);
	*sent = 0;
	while (*sent < len) {
		TWSI_WRITE(sc, TWSI_DATA, *buf++);

		twsi_clear_iflg(sc);
		if (twsi_poll_ctrl(sc, timeout, TWSI_CONTROL_IFLG)) {
			debugf("timeout writing data\n");
			rv = IIC_ETIMEOUT;
			goto out;
		}

		status = TWSI_READ(sc, TWSI_STATUS);
		if (status != TWSI_STATUS_DATA_WR_ACK) {
			debugf("wrong status (%02x) while writing\n", status);
			rv = IIC_ESTATUS;
			goto out;
		}
		(*sent)++;
	}
	rv = IIC_NOERR;
out:
	mtx_unlock(&sc->mutex);
	return (rv);
}
