/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1998, 2001 Nicolas Souchu
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Generic I2C bit-banging code
 *
 * Example:
 *
 *	iicbus
 *	 /  \ 
 *    iicbb pcf
 *     |  \
 *   bti2c lpbb
 *
 * From Linux I2C generic interface
 * (c) 1998 Gerd Knorr <kraxel@cs.tu-berlin.de>
 *
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/fdt/fdt_common.h>
#endif

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/smbus/smbconf.h>

#include "iicbus_if.h"
#include "iicbb_if.h"

/* Based on the SMBus specification. */
#define	DEFAULT_SCL_LOW_TIMEOUT	(25 * 1000)

struct iicbb_softc {
	device_t iicbus;
	u_int udelay;		/* signal toggle delay in usec */
	u_int io_latency;	/* approximate pin toggling latency */
	u_int scl_low_timeout;
};

static int iicbb_attach(device_t);
static void iicbb_child_detached(device_t, device_t);
static int iicbb_detach(device_t);
static int iicbb_print_child(device_t, device_t);
static int iicbb_probe(device_t);

static int iicbb_callback(device_t, int, caddr_t);
static int iicbb_start(device_t, u_char, int);
static int iicbb_repstart(device_t, u_char, int);
static int iicbb_stop(device_t);
static int iicbb_write(device_t, const char *, int, int *, int);
static int iicbb_read(device_t, char *, int, int *, int, int);
static int iicbb_reset(device_t, u_char, u_char, u_char *);
static int iicbb_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs);
static void iicbb_set_speed(struct iicbb_softc *sc, u_char);
#ifdef FDT
static phandle_t iicbb_get_node(device_t, device_t);
#endif

static device_method_t iicbb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		iicbb_probe),
	DEVMETHOD(device_attach,	iicbb_attach),
	DEVMETHOD(device_detach,	iicbb_detach),

	/* bus interface */
	DEVMETHOD(bus_child_detached,	iicbb_child_detached),
	DEVMETHOD(bus_print_child,	iicbb_print_child),

	/* iicbus interface */
	DEVMETHOD(iicbus_callback,	iicbb_callback),
	DEVMETHOD(iicbus_start,		iicbb_start),
	DEVMETHOD(iicbus_repeated_start, iicbb_repstart),
	DEVMETHOD(iicbus_stop,		iicbb_stop),
	DEVMETHOD(iicbus_write,		iicbb_write),
	DEVMETHOD(iicbus_read,		iicbb_read),
	DEVMETHOD(iicbus_reset,		iicbb_reset),
	DEVMETHOD(iicbus_transfer,	iicbb_transfer),

#ifdef FDT
	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	iicbb_get_node),
#endif

	{ 0, 0 }
};

driver_t iicbb_driver = {
	"iicbb",
	iicbb_methods,
	sizeof(struct iicbb_softc),
};

static int
iicbb_probe(device_t dev)
{
	device_set_desc(dev, "I2C bit-banging driver");

	return (0);
}

static int
iicbb_attach(device_t dev)
{
	struct iicbb_softc *sc = (struct iicbb_softc *)device_get_softc(dev);

	sc->iicbus = device_add_child(dev, "iicbus", -1);
	if (!sc->iicbus)
		return (ENXIO);

	sc->scl_low_timeout = DEFAULT_SCL_LOW_TIMEOUT;

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "delay", CTLFLAG_RD, &sc->udelay,
	    0, "Signal change delay controlled by bus frequency, microseconds");

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "scl_low_timeout", CTLFLAG_RWTUN, &sc->scl_low_timeout,
	    0, "SCL low timeout, microseconds");
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "io_latency", CTLFLAG_RWTUN, &sc->io_latency,
	    0, "Estimate of pin toggling latency, microseconds");

	bus_generic_attach(dev);
	return (0);
}

static int
iicbb_detach(device_t dev)
{

	bus_generic_detach(dev);
	device_delete_children(dev);

	return (0);
}

#ifdef FDT
static phandle_t
iicbb_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the I2C bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}
#endif

static void
iicbb_child_detached( device_t dev, device_t child )
{
	struct iicbb_softc *sc = (struct iicbb_softc *)device_get_softc(dev);

	if (child == sc->iicbus)
		sc->iicbus = NULL;
}

static int
iicbb_print_child(device_t bus, device_t dev)
{
	int error;
	int retval = 0;
	u_char oldaddr;

	retval += bus_print_child_header(bus, dev);
	/* retrieve the interface I2C address */
	error = IICBB_RESET(device_get_parent(bus), IIC_FASTEST, 0, &oldaddr);
	if (error == IIC_ENOADDR) {
		retval += printf(" on %s master-only\n",
				 device_get_nameunit(bus));
	} else {
		/* restore the address */
		IICBB_RESET(device_get_parent(bus), IIC_FASTEST, oldaddr, NULL);

		retval += printf(" on %s addr 0x%x\n",
				 device_get_nameunit(bus), oldaddr & 0xff);
	}

	return (retval);
}

#define IICBB_DEBUG
#ifdef IICBB_DEBUG
static int i2c_debug = 0;

SYSCTL_DECL(_hw_i2c);
SYSCTL_INT(_hw_i2c, OID_AUTO, iicbb_debug, CTLFLAG_RWTUN,
    &i2c_debug, 0, "Enable i2c bit-banging driver debug");

#define I2C_DEBUG(x)	do {		\
		if (i2c_debug) (x);	\
	} while (0)
#else
#define I2C_DEBUG(x)
#endif

#define	I2C_GETSDA(dev)		(IICBB_GETSDA(device_get_parent(dev)))
#define	I2C_SETSDA(dev, x)	(IICBB_SETSDA(device_get_parent(dev), x))
#define	I2C_GETSCL(dev)		(IICBB_GETSCL(device_get_parent(dev)))
#define	I2C_SETSCL(dev, x)	(IICBB_SETSCL(device_get_parent(dev), x))

static int
iicbb_waitforscl(device_t dev)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	sbintime_t fast_timeout;
	sbintime_t now, timeout;

	/* Spin for up to 1 ms, then switch to pause. */
	now = sbinuptime();
	fast_timeout = now + SBT_1MS;
	timeout = now + sc->scl_low_timeout * SBT_1US;
	do {
		if (I2C_GETSCL(dev))
			return (0);
		now = sbinuptime();
	} while (now < fast_timeout);
	do {
		I2C_DEBUG(printf("."));
		pause_sbt("iicbb-scl-low", SBT_1MS, C_PREL(8), 0);
		if (I2C_GETSCL(dev))
			return (0);
		now = sbinuptime();
	} while (now < timeout);

	I2C_DEBUG(printf("*"));
	return (IIC_ETIMEOUT);
}

/* Start the high phase of the clock. */
static int
iicbb_clockin(device_t dev, int sda)
{

	/*
	 * Precondition: SCL is low.
	 * Action:
	 * - set SDA to the value;
	 * - release SCL and wait until it's high.
	 * The caller is responsible for keeping SCL high for udelay.
	 *
	 * There should be a data set-up time, 250 ns minimum, between setting
	 * SDA and raising SCL.  It's expected that the I/O access latency will
	 * naturally provide that delay.
	 */
	I2C_SETSDA(dev, sda);
	I2C_SETSCL(dev, 1);
	return (iicbb_waitforscl(dev));
}

/*
 * End the high phase of the clock and wait out the low phase
 * as nothing interesting happens during it anyway.
 */
static void
iicbb_clockout(device_t dev)
{
	struct iicbb_softc *sc = device_get_softc(dev);

	/*
	 * Precondition: SCL is high.
	 * Action:
	 * - pull SCL low and hold for udelay.
	 */
	I2C_SETSCL(dev, 0);
	DELAY(sc->udelay);
}

static int
iicbb_sendbit(device_t dev, int bit)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int err;

	err = iicbb_clockin(dev, bit);
	if (err != 0)
		return (err);
	DELAY(sc->udelay);
	iicbb_clockout(dev);
	return (0);
}

/*
 * Waiting for ACKNOWLEDGE.
 *
 * When a chip is being addressed or has received data it will issue an
 * ACKNOWLEDGE pulse. Therefore the MASTER must release the DATA line
 * (set it to high level) and then release the CLOCK line.
 * Now it must wait for the SLAVE to pull the DATA line low.
 * Actually on the bus this looks like a START condition so nothing happens
 * because of the fact that the IC's that have not been addressed are doing
 * nothing.
 *
 * When the SLAVE has pulled this line low the MASTER will take the CLOCK
 * line low and then the SLAVE will release the SDA (data) line.
 */
static int
iicbb_getack(device_t dev)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int noack, err;
	int t;

	/* Release SDA so that the slave can drive it. */
	err = iicbb_clockin(dev, 1);
	if (err != 0) {
		I2C_DEBUG(printf("! "));
		return (err);
	}

	/* Sample SDA until ACK (low) or udelay runs out. */
	for (t = 0; t < sc->udelay; t++) {
		noack = I2C_GETSDA(dev);
		if (!noack)
			break;
		DELAY(1);
	}

	DELAY(sc->udelay - t);
	iicbb_clockout(dev);

	I2C_DEBUG(printf("%c ", noack ? '-' : '+'));
	return (noack ? IIC_ENOACK : 0);
}

static int
iicbb_sendbyte(device_t dev, uint8_t data)
{
	int err, i;

	for (i = 7; i >= 0; i--) {
		err = iicbb_sendbit(dev, (data & (1 << i)) != 0);
		if (err != 0) {
			I2C_DEBUG(printf("w!"));
			return (err);
		}
	}
	I2C_DEBUG(printf("w%02x", data));
	return (0);
}

static int
iicbb_readbyte(device_t dev, bool last, uint8_t *data)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int i, err;

	/*
	 * Release SDA so that the slave can drive it.
	 * We do not use iicbb_clockin() here because we need to release SDA
	 * only once and then we just pulse the SCL.
	 */
	*data = 0;
	I2C_SETSDA(dev, 1);
	for (i = 7; i >= 0; i--) {
		I2C_SETSCL(dev, 1);
		err = iicbb_waitforscl(dev);
		if (err != 0) {
			I2C_DEBUG(printf("r! "));
			return (err);
		}
		DELAY((sc->udelay + 1) / 2);
		if (I2C_GETSDA(dev))
			*data |= 1 << i;
		DELAY((sc->udelay + 1) / 2);
		iicbb_clockout(dev);
	}

	/*
	 * Send master->slave ACK (low) for more data,
	 * NoACK (high) otherwise.
	 */
	iicbb_sendbit(dev, last);
	I2C_DEBUG(printf("r%02x%c ", *data, last ? '-' : '+'));
	return (0);
}

static int
iicbb_callback(device_t dev, int index, caddr_t data)
{
	return (IICBB_CALLBACK(device_get_parent(dev), index, data));
}

static int
iicbb_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	iicbb_set_speed(device_get_softc(dev), speed);
	return (IICBB_RESET(device_get_parent(dev), speed, addr, oldaddr));
}

static int
iicbb_start_impl(device_t dev, u_char slave, bool repstart)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int error;

	if (!repstart) {
		I2C_DEBUG(printf("<<"));

		/* SCL must be high on the idle bus. */
		if (iicbb_waitforscl(dev) != 0) {
			I2C_DEBUG(printf("C!\n"));
			return (IIC_EBUSERR);
		}
	} else {
		I2C_DEBUG(printf("<"));
		error = iicbb_clockin(dev, 1);
		if (error != 0)
			return (error);

		/* SDA will go low in the middle of the SCL high phase. */
		DELAY((sc->udelay + 1) / 2);
	}

	/*
	 * SDA must be high after the earlier stop condition or the end
	 * of Ack/NoAck pulse.
	 */
	if (!I2C_GETSDA(dev)) {
		I2C_DEBUG(printf("D!\n"));
		return (IIC_EBUSERR);
	}

	/* Start: SDA high->low. */
	I2C_SETSDA(dev, 0);

	/* Wait the second half of the SCL high phase. */
	DELAY((sc->udelay + 1) / 2);

	/* Pull SCL low to keep the bus reserved. */
	iicbb_clockout(dev);

	/* send address */
	error = iicbb_sendbyte(dev, slave);

	/* check for ack */
	if (error == 0)
		error = iicbb_getack(dev);
	if (error != 0)
		(void)iicbb_stop(dev);
	return (error);
}

/* NB: the timeout is ignored. */
static int
iicbb_start(device_t dev, u_char slave, int timeout)
{
	return (iicbb_start_impl(dev, slave, false));
}

/* NB: the timeout is ignored. */
static int
iicbb_repstart(device_t dev, u_char slave, int timeout)
{
	return (iicbb_start_impl(dev, slave, true));
}

static int
iicbb_stop(device_t dev)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int err = 0;

	/*
	 * Stop: SDA goes from low to high in the middle of the SCL high phase.
	 */
	err = iicbb_clockin(dev, 0);
	if (err != 0)
		return (err);
	DELAY((sc->udelay + 1) / 2);
	I2C_SETSDA(dev, 1);
	DELAY((sc->udelay + 1) / 2);

	I2C_DEBUG(printf("%s>>", err != 0 ? "!" : ""));
	I2C_DEBUG(printf("\n"));
	return (err);
}

/* NB: the timeout is ignored. */
static int
iicbb_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	int bytes, error = 0;

	bytes = 0;
	while (len > 0) {
		/* send byte */
		iicbb_sendbyte(dev, (uint8_t)*buf++);

		/* check for ack */
		error = iicbb_getack(dev);
		if (error != 0)
			break;
		bytes++;
		len--;
	}

	*sent = bytes;
	return (error);
}

/* NB: whatever delay is, it's ignored. */
static int
iicbb_read(device_t dev, char *buf, int len, int *read, int last, int delay)
{
	int bytes = 0;
	int err = 0;

	while (len > 0) {
		err = iicbb_readbyte(dev, (len == 1) ? last : 0,
		    (uint8_t *)buf);
		if (err != 0)
			break;
		buf++;
		bytes++;
		len--;
	}

	*read = bytes;
	return (err);
}

static int
iicbb_transfer(device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	int error;

	error = IICBB_PRE_XFER(device_get_parent(dev));
	if (error)
		return (error);

	error = iicbus_transfer_gen(dev, msgs, nmsgs);

	IICBB_POST_XFER(device_get_parent(dev));
	return (error);
}

static void
iicbb_set_speed(struct iicbb_softc *sc, u_char speed)
{
	u_int busfreq;
	int period;

	/*
	 * udelay is half a period, the clock is held high or low for this long.
	 */
	busfreq = IICBUS_GET_FREQUENCY(sc->iicbus, speed);
	period = 1000000 / 2 / busfreq;	/* Hz -> uS */
	period -= sc->io_latency;
	sc->udelay = MAX(period, 1);
}

DRIVER_MODULE(iicbus, iicbb, iicbus_driver, 0, 0);

MODULE_DEPEND(iicbb, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(iicbb, IICBB_MODVER);
