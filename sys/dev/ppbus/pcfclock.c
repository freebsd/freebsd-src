/*-
 * Copyright (c) 2000 Sascha Schumann. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SASCHA SCHUMANN ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/ppbus/pcfclock.c,v 1.20 2005/01/06 01:43:06 imp Exp $");

#include "opt_pcfclock.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_msq.h>
#include <dev/ppbus/ppbio.h>

#include "ppbus_if.h"

#define PCFCLOCK_NAME "pcfclock"

struct pcfclock_data {
	int	count;
};

#define DEVTOSOFTC(dev) \
	((struct pcfclock_data *)device_get_softc(dev))
#define UNITOSOFTC(unit) \
	((struct pcfclock_data *)devclass_get_softc(pcfclock_devclass, (unit)))
#define UNITODEVICE(unit) \
	(devclass_get_device(pcfclock_devclass, (unit)))

static devclass_t pcfclock_devclass;

static	d_open_t		pcfclock_open;
static	d_close_t		pcfclock_close;
static	d_read_t		pcfclock_read;

static struct cdevsw pcfclock_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	pcfclock_open,
	.d_close =	pcfclock_close,
	.d_read =	pcfclock_read,
	.d_name =	PCFCLOCK_NAME,
};

#ifndef PCFCLOCK_MAX_RETRIES
#define PCFCLOCK_MAX_RETRIES 10
#endif

#define AFC_HI 0
#define AFC_LO AUTOFEED

/* AUTO FEED is used as clock */
#define AUTOFEED_CLOCK(val) \
	ctr = (ctr & ~(AUTOFEED)) ^ (val); ppb_wctr(ppbus, ctr)

/* SLCT is used as clock */
#define CLOCK_OK \
	((ppb_rstr(ppbus) & SELECT) == (i & 1 ? SELECT : 0))

/* PE is used as data */
#define BIT_SET (ppb_rstr(ppbus)&PERROR)

/* the first byte sent as reply must be 00001001b */
#define PCFCLOCK_CORRECT_SYNC(buf) (buf[0] == 9)

#define NR(buf, off) (buf[off+1]*10+buf[off])

/* check for correct input values */
#define PCFCLOCK_CORRECT_FORMAT(buf) (\
	NR(buf, 14) <= 99 && \
	NR(buf, 12) <= 12 && \
	NR(buf, 10) <= 31 && \
	NR(buf,  6) <= 23 && \
	NR(buf,  4) <= 59 && \
	NR(buf,  2) <= 59)

#define PCFCLOCK_BATTERY_STATUS_LOW(buf) (buf[8] & 4)
	 
#define PCFCLOCK_CMD_TIME 0		/* send current time */
#define PCFCLOCK_CMD_COPY 7 	/* copy received signal to PC */

static void
pcfclock_identify(driver_t *driver, device_t parent)
{

	device_t dev;

	dev = device_find_child(parent, PCFCLOCK_NAME, 0);
	if (!dev)
		BUS_ADD_CHILD(parent, 0, PCFCLOCK_NAME, -1);
}

static int
pcfclock_probe(device_t dev)
{
	struct pcfclock_data *sc;

	device_set_desc(dev, "PCF-1.0");

	sc = DEVTOSOFTC(dev);
	
	return (0);
}

static int
pcfclock_attach(device_t dev)
{
	int unit;
	
	unit = device_get_unit(dev);

	make_dev(&pcfclock_cdevsw, unit,
			UID_ROOT, GID_WHEEL, 0400, PCFCLOCK_NAME "%d", unit);

	return (0);
}

static int 
pcfclock_open(struct cdev *dev, int flag, int fms, struct thread *td)
{
	u_int unit = minor(dev);
	struct pcfclock_data *sc = UNITOSOFTC(unit);
	device_t pcfclockdev = UNITODEVICE(unit);
	device_t ppbus = device_get_parent(pcfclockdev);
	int res;
	
	if (!sc)
		return (ENXIO);

	if ((res = ppb_request_bus(ppbus, pcfclockdev,
		(flag & O_NONBLOCK) ? PPB_DONTWAIT : PPB_WAIT)))
		return (res);

	sc->count++;
	
	return (0);
}

static int
pcfclock_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	u_int unit = minor(dev);
	struct pcfclock_data *sc = UNITOSOFTC(unit);
	device_t pcfclockdev = UNITODEVICE(unit);
	device_t ppbus = device_get_parent(pcfclockdev);

	sc->count--;
	if (sc->count == 0)
		ppb_release_bus(ppbus, pcfclockdev);

	return (0);
}

static void
pcfclock_write_cmd(struct cdev *dev, unsigned char command)
{
	u_int unit = minor(dev);
	device_t ppidev = UNITODEVICE(unit);
        device_t ppbus = device_get_parent(ppidev);
	unsigned char ctr = 14;
	char i;
	
	for (i = 0; i <= 7; i++) {
		ppb_wdtr(ppbus, i);
		AUTOFEED_CLOCK(i & 1 ? AFC_HI : AFC_LO);
		DELAY(3000);
	}
	ppb_wdtr(ppbus, command);
	AUTOFEED_CLOCK(AFC_LO);
	DELAY(3000);
	AUTOFEED_CLOCK(AFC_HI);
}

static void
pcfclock_display_data(struct cdev *dev, char buf[18]) 
{
	u_int unit = minor(dev);
#ifdef PCFCLOCK_VERBOSE
	int year;

	year = NR(buf, 14);
	if (year < 70)
		year += 100;

	printf(PCFCLOCK_NAME "%d: %02d.%02d.%4d %02d:%02d:%02d, "
			"battery status: %s\n",
			unit,
			NR(buf, 10), NR(buf, 12), 1900 + year,
			NR(buf, 6), NR(buf, 4), NR(buf, 2),
			PCFCLOCK_BATTERY_STATUS_LOW(buf) ? "LOW" : "ok");
#else
	if (PCFCLOCK_BATTERY_STATUS_LOW(buf))
		printf(PCFCLOCK_NAME "%d: BATTERY STATUS LOW ON\n",
				unit);
#endif
}

static int 
pcfclock_read_data(struct cdev *dev, char *buf, ssize_t bits)
{
	u_int unit = minor(dev);
	device_t ppidev = UNITODEVICE(unit);
        device_t ppbus = device_get_parent(ppidev);
	int i;
	char waitfor;
	int offset;

	/* one byte per four bits */
	bzero(buf, ((bits + 3) >> 2) + 1);
	
	waitfor = 100;
	for (i = 0; i <= bits; i++) {
		/* wait for clock, maximum (waitfor*100) usec */
		while(!CLOCK_OK && --waitfor > 0)
			DELAY(100);

		/* timed out? */
		if (!waitfor) 
			return (EIO);
		
		waitfor = 100; /* reload */
		
		/* give it some time */
		DELAY(500);

		/* calculate offset into buffer */
		offset = i >> 2;
		buf[offset] <<= 1;

		if (BIT_SET)
			buf[offset] |= 1;
	}

	return (0);
}

static int 
pcfclock_read_dev(struct cdev *dev, char *buf, int maxretries) 
{
	u_int unit = minor(dev);
	device_t ppidev = UNITODEVICE(unit);
        device_t ppbus = device_get_parent(ppidev);
	int error = 0;

	ppb_set_mode(ppbus, PPB_COMPATIBLE);

	while (--maxretries > 0) {
		pcfclock_write_cmd(dev, PCFCLOCK_CMD_TIME);
		if (pcfclock_read_data(dev, buf, 68))
			continue;
			
		if (!PCFCLOCK_CORRECT_SYNC(buf))
			continue;

		if (!PCFCLOCK_CORRECT_FORMAT(buf))
			continue;

		break;
	}

	if (!maxretries)
		error = EIO;
	
	return (error);
}

static int
pcfclock_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	u_int unit = minor(dev);
	char buf[18];
	int error = 0;

	if (uio->uio_resid < 18)
		return (ERANGE);

	error = pcfclock_read_dev(dev, buf, PCFCLOCK_MAX_RETRIES);
	
	if (error) {
		printf(PCFCLOCK_NAME "%d: no PCF found\n", unit);
	} else {
		pcfclock_display_data(dev, buf);
		
		uiomove(buf, 18, uio);
	}
	
	return (error);
}

static device_method_t pcfclock_methods[] = {
	/* device interface */
	DEVMETHOD(device_identify,	pcfclock_identify),
	DEVMETHOD(device_probe,		pcfclock_probe),
	DEVMETHOD(device_attach,	pcfclock_attach),

	{ 0, 0 }
};

static driver_t pcfclock_driver = {
	PCFCLOCK_NAME,
	pcfclock_methods,
	sizeof(struct pcfclock_data),
};

DRIVER_MODULE(pcfclock, ppbus, pcfclock_driver, pcfclock_devclass, 0, 0);
