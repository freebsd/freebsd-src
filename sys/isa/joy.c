/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/isa/joy.c,v 1.38 2000/01/18 08:38:35 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/joystick.h>

#include <isa/isavar.h>
#include "isa_if.h"

/* The game port can manage 4 buttons and 4 variable resistors (usually 2
 * joysticks, each with 2 buttons and 2 pots.) via the port at address 0x201.
 * Getting the state of the buttons is done by reading the game port:
 * buttons 1-4 correspond to bits 4-7 and resistors 1-4 (X1, Y1, X2, Y2)
 * to bits 0-3.
 * if button 1 (resp 2, 3, 4) is pressed, the bit 4 (resp 5, 6, 7) is set to 0
 * to get the value of a resistor, write the value 0xff at port and
 * wait until the corresponding bit returns to 0.
 */

#define joypart(d) (minor(d)&1)
#define UNIT(d) ((minor(d)>>1)&3)
#ifndef JOY_TIMEOUT
#define JOY_TIMEOUT   2000 /* 2 milliseconds */
#endif

struct joy_softc {
    bus_space_tag_t  bt;
    bus_space_handle_t port;
    int x_off[2], y_off[2];
    int timeout[2];
};

#define JOY_SOFTC(unit) (struct joy_softc *) \
        devclass_get_softc(joy_devclass,(unit))

static int joy_probe (device_t);
static int joy_attach (device_t);

#define CDEV_MAJOR 51
static	d_open_t	joyopen;
static	d_close_t	joyclose;
static	d_read_t	joyread;
static	d_ioctl_t	joyioctl;

static struct cdevsw joy_cdevsw = {
	/* open */	joyopen,
	/* close */	joyclose,
	/* read */	joyread,
	/* write */	nowrite,
	/* ioctl */	joyioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"joy",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

devclass_t joy_devclass;

static struct isa_pnp_id joy_ids[] = {
    {0x0100630e, "CSC0001 PnP Joystick"},	/* CSC0001 */
    {0x0101630e, "CSC0101 PnP Joystick"},	/* CSC0101 */
    {0x01100002, "ALS0110 PnP Joystick"},	/* @P@1001 */
    {0x01100002, "ALS0120 PnP Joystick"},	/* @P@2001 */
    {0x01007316, "ESS0001 PnP Joystick"},	/* ESS0001 */
    {0x2fb0d041, "Generic PnP Joystick"},	/* PNPb02f */
    {0x2200a865, "YMH0022 PnP Joystick"},	/* YMH0022 */
    {0x82719304, NULL},    			/* ADS7182 */
    {0}
};

static int
joy_probe (device_t dev)
{
    if (ISA_PNP_PROBE(device_get_parent(dev), dev, joy_ids) == ENXIO)
        return ENXIO;
#ifdef WANT_JOYSTICK_CONNECTED
#ifdef notyet
    outb (dev->id_iobase, 0xff);
    DELAY (10000); /*  10 ms delay */
    return (inb (dev->id_iobase) & 0x0f) != 0x0f;
#endif
#else
    return 0;
#endif
}

static int
joy_attach (device_t dev)
{
    int	unit = device_get_unit(dev);
    int rid = 0;
    struct resource *res;
    struct joy_softc *joy = device_get_softc(dev);

    res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
    if (res == NULL)
        return ENXIO;
    joy->bt = rman_get_bustag(res);
    joy->port = rman_get_bushandle(res);
    joy->timeout[0] = joy->timeout[1] = 0;
    make_dev(&joy_cdevsw, 0, 0, 0, 0600, "joy%d", unit);
    return 0;
}

static device_method_t joy_methods[] = {
    DEVMETHOD(device_probe,	joy_probe),
    DEVMETHOD(device_attach,	joy_attach),
    { 0, 0 }
};

static driver_t joy_isa_driver = {
    "joy",
    joy_methods,
    sizeof (struct joy_softc)
};

DRIVER_MODULE(joy, isa, joy_isa_driver, joy_devclass, 0, 0);

static int
joyopen(dev_t dev, int flags, int fmt, struct proc *p)
{
    int i = joypart (dev);
    struct joy_softc *joy = JOY_SOFTC(UNIT(dev));

    if (joy->timeout[i])
	return EBUSY;
    joy->x_off[i] = joy->y_off[i] = 0;
    joy->timeout[i] = JOY_TIMEOUT;
    return 0;
}

static int
joyclose(dev_t dev, int flags, int fmt, struct proc *p)
{
    int i = joypart (dev);
    struct joy_softc *joy = JOY_SOFTC(UNIT(dev));

    joy->timeout[i] = 0;
    return 0;
}

static int
joyread(dev_t dev, struct uio *uio, int flag)
{
    struct joy_softc *joy = JOY_SOFTC(UNIT(dev));
    bus_space_handle_t port = joy->port;
    bus_space_tag_t bt = joy->bt;
    struct timespec t, start, end;
    int state = 0;
    struct timespec x, y;
    struct joystick c;
#ifndef i386
    int s;

    s = splhigh();
#else
    disable_intr ();
#endif
    bus_space_write_1 (bt, port, 0, 0xff);
    nanotime(&start);
    end.tv_sec = 0;
    end.tv_nsec = joy->timeout[joypart(dev)] * 1000;
    timespecadd(&end, &start);
    t = start;
    timespecclear(&x);
    timespecclear(&y);
    while (timespeccmp(&t, &end, <)) {
	state = bus_space_read_1 (bt, port, 0);
	if (joypart(dev) == 1)
	    state >>= 2;
	nanotime(&t);
	if (!timespecisset(&x) && !(state & 0x01))
	    x = t;
	if (!timespecisset(&y) && !(state & 0x02))
	    y = t;
	if (timespecisset(&x) && timespecisset(&y))
	    break;
    }
#ifndef i386
    splx(s);
#else
    enable_intr ();
#endif
    if (timespecisset(&x)) {
	timespecsub(&x, &start);
	c.x = joy->x_off[joypart(dev)] + x.tv_nsec / 1000;
    } else
	c.x = 0x80000000;
    if (timespecisset(&y)) {
	timespecsub(&y, &start);
	c.y = joy->y_off[joypart(dev)] + y.tv_nsec / 1000;
    } else
	c.y = 0x80000000;
    state >>= 4;
    c.b1 = ~state & 1;
    c.b2 = ~(state >> 1) & 1;
    return uiomove ((caddr_t)&c, sizeof(struct joystick), uio);
}

static int
joyioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    struct joy_softc *joy = JOY_SOFTC(UNIT(dev));
    int i = joypart (dev);
    int x;

    switch (cmd) {
    case JOY_SETTIMEOUT:
	x = *(int *) data;
	if (x < 1 || x > 10000) /* 10ms maximum! */
	    return EINVAL;
	joy->timeout[i] = x;
	break;
    case JOY_GETTIMEOUT:
	*(int *) data = joy->timeout[i];
	break;
    case JOY_SET_X_OFFSET:
	joy->x_off[i] = *(int *) data;
	break;
    case JOY_SET_Y_OFFSET:
	joy->y_off[i] = *(int *) data;
	break;
    case JOY_GET_X_OFFSET:
	*(int *) data = joy->x_off[i];
	break;
    case JOY_GET_Y_OFFSET:
	*(int *) data = joy->y_off[i];
	break;
    default:
	return ENXIO;
    }
    return 0;
}
