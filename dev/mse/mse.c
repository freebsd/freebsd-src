/*-
 * Copyright (c) 2004 M. Warner Losh
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
 *
 * $FreeBSD$
 */

/*
 * Copyright 1992 by the University of Guelph
 *
 * Permission to use, copy and modify this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation.
 * University of Guelph makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */
/*
 * Driver for the Logitech and ATI Inport Bus mice for use with 386bsd and
 * the X386 port, courtesy of
 * Rick Macklem, rick@snowhite.cis.uoguelph.ca
 * Caveats: The driver currently uses spltty(), but doesn't use any
 * generic tty code. It could use splmse() (that only masks off the
 * bus mouse interrupt, but that would require hacking in i386/isa/icu.s.
 * (This may be worth the effort, since the Logitech generates 30/60
 * interrupts/sec continuously while it is open.)
 * NB: The ATI has NOT been tested yet!
 */

/*
 * Modification history:
 * Sep 6, 1994 -- Lars Fredriksen(fredriks@mcs.com)
 *   improved probe based on input from Logitech.
 *
 * Oct 19, 1992 -- E. Stark (stark@cs.sunysb.edu)
 *   fixes to make it work with Microsoft InPort busmouse
 *
 * Jan, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   added patches for new "select" interface
 *
 * May 4, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   changed position of some spl()'s in mseread
 *
 * October 8, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   limit maximum negative x/y value to -127 to work around XFree problem
 *   that causes spurious button pushes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/mouse.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/mse/msevar.h>

devclass_t	mse_devclass;

static	d_open_t	mseopen;
static	d_close_t	mseclose;
static	d_read_t	mseread;
static  d_ioctl_t	mseioctl;
static	d_poll_t	msepoll;

static struct cdevsw mse_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	mseopen,
	.d_close =	mseclose,
	.d_read =	mseread,
	.d_ioctl =	mseioctl,
	.d_poll =	msepoll,
	.d_name =	"mse",
};

static	void		mseintr(void *);
static	void		mseintr_locked(mse_softc_t *sc);
static	void		msetimeout(void *);

#define	MSE_NBLOCKIO(dev)	(dev2unit(dev) != 0)

#define	MSEPRI	(PZERO + 3)

int
mse_common_attach(device_t dev)
{
	mse_softc_t *sc;
	int unit, flags, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	mtx_init(&sc->sc_lock, "mse", NULL, MTX_DEF);
	callout_init_mtx(&sc->sc_callout, &sc->sc_lock, 0);

	rid = 0;
	sc->sc_intr = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
					     RF_ACTIVE);
	if (sc->sc_intr == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		mtx_destroy(&sc->sc_lock);
		return ENXIO;
	}

	if (bus_setup_intr(dev, sc->sc_intr, INTR_TYPE_TTY | INTR_MPSAFE,
	    NULL, mseintr, sc, &sc->sc_ih)) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->sc_intr);
		mtx_destroy(&sc->sc_lock);
		return ENXIO;
	}
	flags = device_get_flags(dev);
	sc->mode.accelfactor = (flags & MSE_CONFIG_ACCEL) >> 4;

	sc->sc_dev = make_dev(&mse_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "mse%d", unit);
	sc->sc_dev->si_drv1 = sc;
	sc->sc_ndev = make_dev(&mse_cdevsw, 1, UID_ROOT, GID_WHEEL, 0600,
	    "nmse%d", unit);
	sc->sc_ndev->si_drv1 = sc;
	return 0;
}

int
mse_detach(device_t dev)
{
	mse_softc_t *sc;
	int rid;

	sc = device_get_softc(dev);
	MSE_LOCK(sc);
	if (sc->sc_flags & MSESC_OPEN) {
		MSE_UNLOCK(sc);
		return EBUSY;
	}

	/* Sabotage subsequent opens. */
	sc->sc_mousetype = MSE_NONE;
	MSE_UNLOCK(sc);

	destroy_dev(sc->sc_dev);
	destroy_dev(sc->sc_ndev);

	rid = 0;
	bus_teardown_intr(dev, sc->sc_intr, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, rid, sc->sc_intr);
	bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);

	callout_drain(&sc->sc_callout);
	mtx_destroy(&sc->sc_lock);

	return 0;
}

/*
 * Exclusive open the mouse, initialize it and enable interrupts.
 */
static	int
mseopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	mse_softc_t *sc = dev->si_drv1;

	MSE_LOCK(sc);
	if (sc->sc_mousetype == MSE_NONE) {
		MSE_UNLOCK(sc);
		return (ENXIO);
	}
	if (sc->sc_flags & MSESC_OPEN) {
		MSE_UNLOCK(sc);
		return (EBUSY);
	}
	sc->sc_flags |= MSESC_OPEN;
	sc->sc_obuttons = sc->sc_buttons = MOUSE_MSC_BUTTONS;
	sc->sc_deltax = sc->sc_deltay = 0;
	sc->sc_bytesread = sc->mode.packetsize = MOUSE_MSC_PACKETSIZE;
	sc->sc_watchdog = FALSE;
	callout_reset(&sc->sc_callout, hz * 2, msetimeout, dev);
	sc->mode.level = 0;
	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;

	/*
	 * Initialize mouse interface and enable interrupts.
	 */
	(*sc->sc_enablemouse)(sc->sc_port);
	MSE_UNLOCK(sc);
	return (0);
}

/*
 * mseclose: just turn off mouse innterrupts.
 */
static	int
mseclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	mse_softc_t *sc = dev->si_drv1;

	MSE_LOCK(sc);
	callout_stop(&sc->sc_callout);
	(*sc->sc_disablemouse)(sc->sc_port);
	sc->sc_flags &= ~MSESC_OPEN;
	MSE_UNLOCK(sc);
	return(0);
}

/*
 * mseread: return mouse info using the MSC serial protocol, but without
 * using bytes 4 and 5.
 * (Yes this is cheesy, but it makes the X386 server happy, so...)
 */
static	int
mseread(struct cdev *dev, struct uio *uio, int ioflag)
{
	mse_softc_t *sc = dev->si_drv1;
	int xfer, error;

	/*
	 * If there are no protocol bytes to be read, set up a new protocol
	 * packet.
	 */
	MSE_LOCK(sc);
	while (sc->sc_flags & MSESC_READING) {
		if (MSE_NBLOCKIO(dev)) {
			MSE_UNLOCK(sc);
			return (0);
		}
		sc->sc_flags |= MSESC_WANT;
		error = mtx_sleep(sc, &sc->sc_lock, MSEPRI | PCATCH, "mseread",
		    0);
		if (error) {
			MSE_UNLOCK(sc);
			return (error);
		}
	}
	sc->sc_flags |= MSESC_READING;
	xfer = 0;
	if (sc->sc_bytesread >= sc->mode.packetsize) {
		while (sc->sc_deltax == 0 && sc->sc_deltay == 0 &&
		       (sc->sc_obuttons ^ sc->sc_buttons) == 0) {
			if (MSE_NBLOCKIO(dev))
				goto out;
			sc->sc_flags |= MSESC_WANT;
			error = mtx_sleep(sc, &sc->sc_lock, MSEPRI | PCATCH,
				"mseread", 0);
			if (error)
				goto out;
		}

		/*
		 * Generate protocol bytes.
		 * For some reason X386 expects 5 bytes but never uses
		 * the fourth or fifth?
		 */
		sc->sc_bytes[0] = sc->mode.syncmask[1] 
		    | (sc->sc_buttons & ~sc->mode.syncmask[0]);
		if (sc->sc_deltax > 127)
			sc->sc_deltax = 127;
		if (sc->sc_deltax < -127)
			sc->sc_deltax = -127;
		sc->sc_deltay = -sc->sc_deltay;	/* Otherwise mousey goes wrong way */
		if (sc->sc_deltay > 127)
			sc->sc_deltay = 127;
		if (sc->sc_deltay < -127)
			sc->sc_deltay = -127;
		sc->sc_bytes[1] = sc->sc_deltax;
		sc->sc_bytes[2] = sc->sc_deltay;
		sc->sc_bytes[3] = sc->sc_bytes[4] = 0;
		sc->sc_bytes[5] = sc->sc_bytes[6] = 0;
		sc->sc_bytes[7] = MOUSE_SYS_EXTBUTTONS;
		sc->sc_obuttons = sc->sc_buttons;
		sc->sc_deltax = sc->sc_deltay = 0;
		sc->sc_bytesread = 0;
	}
	xfer = min(uio->uio_resid, sc->mode.packetsize - sc->sc_bytesread);
	MSE_UNLOCK(sc);
	error = uiomove(&sc->sc_bytes[sc->sc_bytesread], xfer, uio);
	MSE_LOCK(sc);
out:
	sc->sc_flags &= ~MSESC_READING;
	if (error == 0)
		sc->sc_bytesread += xfer;
	if (sc->sc_flags & MSESC_WANT) {
		sc->sc_flags &= ~MSESC_WANT;
		MSE_UNLOCK(sc);
		wakeup(sc);
	} else
		MSE_UNLOCK(sc);
	return (error);
}

/*
 * mseioctl: process ioctl commands.
 */
static int
mseioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
	mse_softc_t *sc = dev->si_drv1;
	mousestatus_t status;
	int err = 0;

	switch (cmd) {

	case MOUSE_GETHWINFO:
		MSE_LOCK(sc);
		*(mousehw_t *)addr = sc->hw;
		if (sc->mode.level == 0)
			((mousehw_t *)addr)->model = MOUSE_MODEL_GENERIC;
		MSE_UNLOCK(sc);
		break;

	case MOUSE_GETMODE:
		MSE_LOCK(sc);
		*(mousemode_t *)addr = sc->mode;
		switch (sc->mode.level) {
		case 0:
			break;
		case 1:
			((mousemode_t *)addr)->protocol = MOUSE_PROTO_SYSMOUSE;
	    		((mousemode_t *)addr)->syncmask[0] = MOUSE_SYS_SYNCMASK;
	    		((mousemode_t *)addr)->syncmask[1] = MOUSE_SYS_SYNC;
			break;
		}
		MSE_UNLOCK(sc);
		break;

	case MOUSE_SETMODE:
		switch (((mousemode_t *)addr)->level) {
		case 0:
		case 1:
			break;
		default:
			return (EINVAL);
		}
		MSE_LOCK(sc);
		if (((mousemode_t *)addr)->accelfactor < -1) {
			MSE_UNLOCK(sc);
			return (EINVAL);
		} else if (((mousemode_t *)addr)->accelfactor >= 0)
			sc->mode.accelfactor = 
			    ((mousemode_t *)addr)->accelfactor;
		sc->mode.level = ((mousemode_t *)addr)->level;
		switch (sc->mode.level) {
		case 0:
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_MSC_PACKETSIZE;
			break;
		case 1:
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_SYS_PACKETSIZE;
			break;
		}
		MSE_UNLOCK(sc);
		break;

	case MOUSE_GETLEVEL:
		MSE_LOCK(sc);
		*(int *)addr = sc->mode.level;
		MSE_UNLOCK(sc);
		break;

	case MOUSE_SETLEVEL:
		switch (*(int *)addr) {
		case 0:
			MSE_LOCK(sc);
			sc->mode.level = *(int *)addr;
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_MSC_PACKETSIZE;
			MSE_UNLOCK(sc);
			break;
		case 1:
			MSE_LOCK(sc);
			sc->mode.level = *(int *)addr;
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_SYS_PACKETSIZE;
			MSE_UNLOCK(sc);
			break;
		default:
			return (EINVAL);
		}
		break;

	case MOUSE_GETSTATUS:
		MSE_LOCK(sc);
		status = sc->status;
		sc->status.flags = 0;
		sc->status.obutton = sc->status.button;
		sc->status.button = 0;
		sc->status.dx = 0;
		sc->status.dy = 0;
		sc->status.dz = 0;
		MSE_UNLOCK(sc);
		*(mousestatus_t *)addr = status;
		break;

	case MOUSE_READSTATE:
	case MOUSE_READDATA:
		return (ENODEV);

#if (defined(MOUSE_GETVARS))
	case MOUSE_GETVARS:
	case MOUSE_SETVARS:
		return (ENODEV);
#endif

	default:
		return (ENOTTY);
	}
	return (err);
}

/*
 * msepoll: check for mouse input to be processed.
 */
static	int
msepoll(struct cdev *dev, int events, struct thread *td)
{
	mse_softc_t *sc = dev->si_drv1;
	int revents = 0;

	MSE_LOCK(sc);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_bytesread != sc->mode.packetsize ||
		    sc->sc_deltax != 0 || sc->sc_deltay != 0 ||
		    (sc->sc_obuttons ^ sc->sc_buttons) != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &sc->sc_selp);
	}
	MSE_UNLOCK(sc);
	return (revents);
}

/*
 * msetimeout: watchdog timer routine.
 */
static void
msetimeout(void *arg)
{
	struct cdev *dev;
	mse_softc_t *sc;

	dev = (struct cdev *)arg;
	sc = dev->si_drv1;
	MSE_ASSERT_LOCKED(sc);
	if (sc->sc_watchdog) {
		if (bootverbose)
			printf("%s: lost interrupt?\n", devtoname(dev));
		mseintr_locked(sc);
	}
	sc->sc_watchdog = TRUE;
	callout_schedule(&sc->sc_callout, hz);
}

/*
 * mseintr: update mouse status. sc_deltax and sc_deltay are accumulative.
 */
static void
mseintr(void *arg)
{
	mse_softc_t *sc = arg;

	MSE_LOCK(sc);
	mseintr_locked(sc);
	MSE_UNLOCK(sc);
}

static void
mseintr_locked(mse_softc_t *sc)
{
	/*
	 * the table to turn MouseSystem button bits (MOUSE_MSC_BUTTON?UP)
	 * into `mousestatus' button bits (MOUSE_BUTTON?DOWN).
	 */
	static int butmap[8] = {
		0, 
		MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON2DOWN, 
		MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON1DOWN, 
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
        	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
	};
	int dx, dy, but;
	int sign;

#ifdef DEBUG
	static int mse_intrcnt = 0;
	if((mse_intrcnt++ % 10000) == 0)
		printf("mseintr\n");
#endif /* DEBUG */
	if ((sc->sc_flags & MSESC_OPEN) == 0)
		return;

	(*sc->sc_getmouse)(sc->sc_port, &dx, &dy, &but);
	if (sc->mode.accelfactor > 0) {
		sign = (dx < 0);
		dx = dx * dx / sc->mode.accelfactor;
		if (dx == 0)
			dx = 1;
		if (sign)
			dx = -dx;
		sign = (dy < 0);
		dy = dy * dy / sc->mode.accelfactor;
		if (dy == 0)
			dy = 1;
		if (sign)
			dy = -dy;
	}
	sc->sc_deltax += dx;
	sc->sc_deltay += dy;
	sc->sc_buttons = but;

	but = butmap[~but & MOUSE_MSC_BUTTONS];
	sc->status.dx += dx;
	sc->status.dy += dy;
	sc->status.flags |= ((dx || dy) ? MOUSE_POSCHANGED : 0)
	    | (sc->status.button ^ but);
	sc->status.button = but;

	sc->sc_watchdog = FALSE;

	/*
	 * If mouse state has changed, wake up anyone wanting to know.
	 */
	if (sc->sc_deltax != 0 || sc->sc_deltay != 0 ||
	    (sc->sc_obuttons ^ sc->sc_buttons) != 0) {
		if (sc->sc_flags & MSESC_WANT) {
			sc->sc_flags &= ~MSESC_WANT;
			wakeup(sc);
		}
		selwakeuppri(&sc->sc_selp, MSEPRI);
	}
}
