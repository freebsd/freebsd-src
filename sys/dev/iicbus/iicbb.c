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
	u_int scl_low_timeout;
};

static int iicbb_attach(device_t);
static void iicbb_child_detached(device_t, device_t);
static int iicbb_detach(device_t);
static int iicbb_print_child(device_t, device_t);
static int iicbb_probe(device_t);

static int iicbb_callback(device_t, int, caddr_t);
static int iicbb_start(device_t, u_char, int);
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
	DEVMETHOD(iicbus_repeated_start, iicbb_start),
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

devclass_t iicbb_devclass;

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

#define	I2C_GETSDA(dev)		(IICBB_GETSDA(device_get_parent(dev)))
#define	I2C_SETSDA(dev, x)	(IICBB_SETSDA(device_get_parent(dev), x))
#define	I2C_GETSCL(dev)		(IICBB_GETSCL(device_get_parent(dev)))
#define	I2C_SETSCL(dev, x)	(IICBB_SETSCL(device_get_parent(dev), x))

#define I2C_SET(sc, dev, ctrl, val) do {		\
	iicbb_setscl(dev, ctrl);			\
	I2C_SETSDA(dev, val);				\
	DELAY(sc->udelay);				\
	} while (0)

static int i2c_debug = 0;
#define I2C_DEBUG(x)	do {					\
				if (i2c_debug) (x);		\
			} while (0)

#define I2C_LOG(format,args...)	do {				\
					printf(format, args);	\
				} while (0)

static void
iicbb_setscl(device_t dev, int val)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	sbintime_t now, end;
	int fast_timeout;

	I2C_SETSCL(dev, val);
	DELAY(sc->udelay);

	/* Pulling low cannot fail. */
	if (!val)
		return;

	/* Use DELAY for up to 1 ms, then switch to pause. */
	end = sbinuptime() + sc->scl_low_timeout * SBT_1US;
	fast_timeout = MIN(sc->scl_low_timeout, 1000);
	while (fast_timeout > 0) {
		if (I2C_GETSCL(dev))
			return;
		I2C_SETSCL(dev, 1);	/* redundant ? */
		DELAY(sc->udelay);
		fast_timeout -= sc->udelay;
	}

	while (!I2C_GETSCL(dev)) {
		now = sbinuptime();
		if (now >= end)
			break;
		pause_sbt("iicbb-scl-low", SBT_1MS, C_PREL(8), 0);
	}

}

static void
iicbb_one(device_t dev, int timeout)
{
	struct iicbb_softc *sc = device_get_softc(dev);

	I2C_SET(sc,dev,0,1);
	I2C_SET(sc,dev,1,1);
	I2C_SET(sc,dev,0,1);
	return;
}

static void
iicbb_zero(device_t dev, int timeout)
{
	struct iicbb_softc *sc = device_get_softc(dev);

	I2C_SET(sc,dev,0,0);
	I2C_SET(sc,dev,1,0);
	I2C_SET(sc,dev,0,0);
	return;
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
iicbb_ack(device_t dev, int timeout)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int noack;
	int k = 0;

	I2C_SET(sc,dev,0,1);
	I2C_SET(sc,dev,1,1);

	/* SCL must be high now. */
	if (!I2C_GETSCL(dev))
		return (IIC_ETIMEOUT);

	do {
		noack = I2C_GETSDA(dev);
		if (!noack)
			break;
		DELAY(1);
		k++;
	} while (k < timeout);

	I2C_SET(sc,dev,0,1);
	I2C_DEBUG(printf("%c ",noack?'-':'+'));

	return (noack ? IIC_ENOACK : 0);
}

static void
iicbb_sendbyte(device_t dev, u_char data, int timeout)
{
	int i;

	for (i=7; i>=0; i--) {
		if (data&(1<<i)) {
			iicbb_one(dev, timeout);
		} else {
			iicbb_zero(dev, timeout);
		}
	}
	I2C_DEBUG(printf("w%02x",(int)data));
	return;
}

static u_char
iicbb_readbyte(device_t dev, int last, int timeout)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int i;
	unsigned char data=0;

	I2C_SET(sc,dev,0,1);
	for (i=7; i>=0; i--) 
	{
		I2C_SET(sc,dev,1,1);
		if (I2C_GETSDA(dev))
			data |= (1<<i);
		I2C_SET(sc,dev,0,1);
	}
	if (last) {
		iicbb_one(dev, timeout);
	} else {
		iicbb_zero(dev, timeout);
	}
	I2C_DEBUG(printf("r%02x%c ",(int)data,last?'-':'+'));
	return (data);
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
iicbb_start(device_t dev, u_char slave, int timeout)
{
	struct iicbb_softc *sc = device_get_softc(dev);
	int error;

	I2C_DEBUG(printf("<"));

	I2C_SET(sc,dev,1,1);

	/* SCL must be high now. */
	if (!I2C_GETSCL(dev))
		return (IIC_ETIMEOUT);

	I2C_SET(sc,dev,1,0);
	I2C_SET(sc,dev,0,0);

	/* send address */
	iicbb_sendbyte(dev, slave, timeout);

	/* check for ack */
	error = iicbb_ack(dev, timeout);
	if (error == 0)
		return (0);

	iicbb_stop(dev);
	return (error);
}

static int
iicbb_stop(device_t dev)
{
	struct iicbb_softc *sc = device_get_softc(dev);

	I2C_SET(sc,dev,0,0);
	I2C_SET(sc,dev,1,0);
	I2C_SET(sc,dev,1,1);
	I2C_DEBUG(printf(">"));
	I2C_DEBUG(printf("\n"));

	/* SCL must be high now. */
	if (!I2C_GETSCL(dev))
		return (IIC_ETIMEOUT);
	return (0);
}

static int
iicbb_write(device_t dev, const char *buf, int len, int *sent, int timeout)
{
	int bytes, error = 0;

	bytes = 0;
	while (len) {
		/* send byte */
		iicbb_sendbyte(dev,(u_char)*buf++, timeout);

		/* check for ack */
		error = iicbb_ack(dev, timeout);
		if (error != 0)
			break;
		bytes++;
		len--;
	}

	*sent = bytes;
	return (error);
}

static int
iicbb_read(device_t dev, char * buf, int len, int *read, int last, int delay)
{
	int bytes;

	bytes = 0;
	while (len) {
		/* XXX should insert delay here */
		*buf++ = (char)iicbb_readbyte(dev, (len == 1) ? last : 0, delay);

		bytes ++;
		len --;
	}

	*read = bytes;
	return (0);
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
	u_int busfreq, period;

	/*
	 * NB: the resulting frequency will be a quarter (even less) of the
	 * configured bus frequency.  This is for historic reasons.  The default
	 * bus frequency is 100 kHz.  And the historic default udelay is 10
	 * microseconds.  The cycle of sending a bit takes four udelay-s plus
	 * SCL is kept low for extra two udelay-s.  The actual I/O toggling also
	 * has an overhead.
	 */
	busfreq = IICBUS_GET_FREQUENCY(sc->iicbus, speed);
	period = 1000000 / busfreq;	/* Hz -> uS */
	sc->udelay = MAX(period, 1);
}

DRIVER_MODULE(iicbus, iicbb, iicbus_driver, iicbus_devclass, 0, 0);

MODULE_DEPEND(iicbb, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(iicbb, IICBB_MODVER);
