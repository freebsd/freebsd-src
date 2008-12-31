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
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/joy/joy.c,v 1.54.6.1 2008/11/25 02:59:29 kensmith Exp $");

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
#include <dev/joy/joyvar.h>

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

#define JOY_SOFTC(unit) (struct joy_softc *) \
        devclass_get_softc(joy_devclass,(unit))

static	d_open_t	joyopen;
static	d_close_t	joyclose;
static	d_read_t	joyread;
static	d_ioctl_t	joyioctl;

static struct cdevsw joy_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	joyopen,
	.d_close =	joyclose,
	.d_read =	joyread,
	.d_ioctl =	joyioctl,
	.d_name =	"joy",
};

devclass_t joy_devclass;

int
joy_probe(device_t dev)
{
#ifdef WANT_JOYSTICK_CONNECTED
#ifdef notyet
	outb(dev->id_iobase, 0xff);
	DELAY(10000); /*  10 ms delay */
	return (inb(dev->id_iobase) & 0x0f) != 0x0f;
#else
	return (0);
#endif
#else
	return (0);
#endif
}

int
joy_attach(device_t dev)
{
	int	unit = device_get_unit(dev);
	struct joy_softc *joy = device_get_softc(dev);

	joy->rid = 0;
	joy->res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &joy->rid,
	    RF_ACTIVE|RF_SHAREABLE);
	if (joy->res == NULL)
		return ENXIO;
	joy->bt = rman_get_bustag(joy->res);
	joy->port = rman_get_bushandle(joy->res);
	joy->timeout[0] = joy->timeout[1] = 0;
	joy->d = make_dev(&joy_cdevsw, unit, 0, 0, 0600, "joy%d", unit);
	return (0);
}

int
joy_detach(device_t dev)
{
	struct joy_softc *joy = device_get_softc(dev);

	if (joy->res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, joy->rid, joy->res);
	if (joy->d)
		destroy_dev(joy->d);
	return (0);
}


static int
joyopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	int i = joypart (dev);
	struct joy_softc *joy = JOY_SOFTC(UNIT(dev));

	if (joy->timeout[i])
		return (EBUSY);
	joy->x_off[i] = joy->y_off[i] = 0;
	joy->timeout[i] = JOY_TIMEOUT;
	return (0);
}

static int
joyclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	int i = joypart (dev);
	struct joy_softc *joy = JOY_SOFTC(UNIT(dev));

	joy->timeout[i] = 0;
	return (0);
}

static int
joyread(struct cdev *dev, struct uio *uio, int flag)
{
	struct joy_softc *joy = JOY_SOFTC(UNIT(dev));
	bus_space_handle_t port = joy->port;
	bus_space_tag_t bt = joy->bt;
	struct timespec t, start, end;
	int state = 0;
	struct timespec x, y;
	struct joystick c;
#ifndef __i386__
	int s;

	s = splhigh();
#else
	disable_intr ();
#endif
	nanotime(&t);
	end.tv_sec = 0;
	end.tv_nsec = joy->timeout[joypart(dev)] * 1000;
	timespecadd(&end, &t);
	for (; timespeccmp(&t, &end, <) && (bus_space_read_1(bt, port, 0) & 0x0f); nanotime(&t))
		;	/* nothing */
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
#ifndef __i386__
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
	return (uiomove((caddr_t)&c, sizeof(struct joystick), uio));
}

static int
joyioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, struct thread *td)
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
		return (ENOTTY);
	}
	return (0);
}
