/*	$NetBSD: ucom.c,v 1.40 2001/11/13 06:24:54 lukem Exp $	*/

/*-
 * Copyright (c) 2001-2003, 2005 Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/file.h>
#if __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#ifdef USB_DEBUG
static int	ucomdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ucom, CTLFLAG_RW, 0, "USB ucom");
SYSCTL_INT(_hw_usb_ucom, OID_AUTO, debug, CTLFLAG_RW,
	   &ucomdebug, 0, "ucom debug level");
#define DPRINTF(x)	do { \
				if (ucomdebug) \
					logprintf x; \
			} while (0)

#define DPRINTFN(n, x)	do { \
				if (ucomdebug > (n)) \
					logprintf x; \
			} while (0)
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

Static d_open_t  ucomopen;
Static d_close_t ucomclose;
Static d_read_t  ucomread;
Static d_write_t ucomwrite;
Static d_ioctl_t ucomioctl;


static struct cdevsw ucom_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ucomopen,
	.d_close =	ucomclose,
	.d_read =	ucomread,
	.d_write =	ucomwrite,
	.d_ioctl =	ucomioctl,
	.d_name =	"ucom",
	.d_flags =	D_TTY | D_NEEDGIANT,
#if __FreeBSD_version < 500014
	.d_bmaj =	-1,
#endif
};

Static void ucom_cleanup(struct ucom_softc *);
Static int ucomctl(struct ucom_softc *, int, int);
Static int ucomparam(struct tty *, struct termios *);
Static void ucomstart(struct tty *);
Static void ucomstop(struct tty *, int);
Static void ucom_shutdown(struct ucom_softc *);
Static void ucom_dtr(struct ucom_softc *, int);
Static void ucom_rts(struct ucom_softc *, int);
Static void ucom_break(struct ucom_softc *, int);
Static usbd_status ucomstartread(struct ucom_softc *);
Static void ucomreadcb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void ucomwritecb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void ucomstopread(struct ucom_softc *);

devclass_t ucom_devclass;

static moduledata_t ucom_mod = {
	"ucom",
	NULL,
	NULL
};

DECLARE_MODULE(ucom, ucom_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(ucom, usb, 1, 1, 1);
MODULE_VERSION(ucom, UCOM_MODVER);

int
ucom_attach(struct ucom_softc *sc)
{
	struct tty *tp;
	int unit;

	unit = device_get_unit(sc->sc_dev);

	sc->sc_tty = tp = ttymalloc(sc->sc_tty);
	tp->t_oproc = ucomstart;
	tp->t_param = ucomparam;
	tp->t_stop = ucomstop;

	DPRINTF(("ucom_attach: tty_attach tp = %p\n", tp));

	DPRINTF(("ucom_attach: make_dev: ucom%d\n", unit));

	sc->dev = make_dev(&ucom_cdevsw, unit | UCOM_CALLOUT_MASK,
			UID_UUCP, GID_DIALER, 0660,
			"ucom%d", unit);
	sc->dev->si_tty = tp;

	return (0);
}

int
ucom_detach(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	int s;

	DPRINTF(("ucom_detach: sc = %p, tp = %p\n", sc, sc->sc_tty));

	sc->sc_dying = 1;

	if (sc->sc_bulkin_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	if (tp != NULL) {
		if (tp->t_state & TS_ISOPEN) {
			device_printf(sc->sc_dev,
				      "still open, forcing close\n");
			ttyld_close(tp, 0);
			tty_close(tp);
		}
	} else {
		DPRINTF(("ucom_detach: no tty\n"));
		return (0);
	}

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	destroy_dev(sc->dev);

	return (0);
}

Static void
ucom_shutdown(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucom_shutdown\n"));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		(void)ucomctl(sc, TIOCM_DTR, DMBIC);
		(void)tsleep(sc, TTIPRI, "ucomsd", hz);
	}
}

Static int
ucomopen(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = UCOMUNIT(dev);
	struct ucom_softc *sc;
	usbd_status err;
	struct tty *tp;
	int s;
	int error;

	USB_GET_SC_OPEN(ucom, unit, sc);

	if (sc->sc_dying)
		return (ENXIO);

	tp = sc->sc_tty;

	DPRINTF(("%s: ucomopen: tp = %p\n", USBDEVNAME(sc->sc_dev), tp));

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    suser(p))
		return (EBUSY);

	/*
	 * Do the following iff this is a first open.
	 */
	s = spltty();
	while (sc->sc_opening)
		tsleep(&sc->sc_opening, PRIBIO, "ucomop", 0);
	sc->sc_opening = 1;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		struct termios t;

		sc->sc_poll = 0;
		sc->sc_lsr = sc->sc_msr = sc->sc_mcr = 0;

		tp->t_dev = dev;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		/* Make sure ucomparam() will do something. */
		tp->t_ospeed = 0;
		(void)ucomparam(tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		(void)ucomctl(sc, TIOCM_DTR | TIOCM_RTS, DMBIS);

		/* Device specific open */
		if (sc->sc_callback->ucom_open != NULL) {
			error = sc->sc_callback->ucom_open(sc->sc_parent,
							   sc->sc_portno);
			if (error) {
				ucom_cleanup(sc);
				sc->sc_opening = 0;
				wakeup(&sc->sc_opening);
				splx(s);
				return (error);
			}
		}

		DPRINTF(("ucomopen: open pipes in = %d out = %d\n",
			 sc->sc_bulkin_no, sc->sc_bulkout_no));

		/* Open the bulk pipes */
		/* Bulk-in pipe */
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin_no, 0,
				     &sc->sc_bulkin_pipe);
		if (err) {
			printf("%s: open bulk in error (addr %d): %s\n",
			       USBDEVNAME(sc->sc_dev), sc->sc_bulkin_no,
			       usbd_errstr(err));
			error = EIO;
			goto fail_0;
		}
		/* Bulk-out pipe */
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout_no,
				     USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
		if (err) {
			printf("%s: open bulk out error (addr %d): %s\n",
			       USBDEVNAME(sc->sc_dev), sc->sc_bulkout_no,
			       usbd_errstr(err));
			error = EIO;
			goto fail_1;
		}

		/* Allocate a request and an input buffer and start reading. */
		sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_ixfer == NULL) {
			error = ENOMEM;
			goto fail_2;
		}

		sc->sc_ibuf = usbd_alloc_buffer(sc->sc_ixfer,
						sc->sc_ibufsizepad);
		if (sc->sc_ibuf == NULL) {
			error = ENOMEM;
			goto fail_3;
		}

		sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer == NULL) {
			error = ENOMEM;
			goto fail_3;
		}

		sc->sc_obuf = usbd_alloc_buffer(sc->sc_oxfer,
						sc->sc_obufsize +
						sc->sc_opkthdrlen);
		if (sc->sc_obuf == NULL) {
			error = ENOMEM;
			goto fail_4;
		}

		/*
		 * Handle initial DCD.
		 */
		if (ISSET(sc->sc_msr, UMSR_DCD) ||
		    (minor(dev) & UCOM_CALLOUT_MASK))
			ttyld_modem(tp, 1);

		ucomstartread(sc);
	}

	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);

	error = tty_open(dev, tp);
	if (error)
		goto bad;

	error = ttyld_open(tp, dev);
	if (error)
		goto bad;

	ttyldoptim(tp);

	DPRINTF(("%s: ucomopen: success\n", USBDEVNAME(sc->sc_dev)));

	sc->sc_poll = 1;
	sc->sc_refcnt++;

	return (0);

fail_4:
	usbd_free_xfer(sc->sc_oxfer);
	sc->sc_oxfer = NULL;
fail_3:
	usbd_free_xfer(sc->sc_ixfer);
	sc->sc_ixfer = NULL;
fail_2:
	usbd_close_pipe(sc->sc_bulkout_pipe);
	sc->sc_bulkout_pipe = NULL;
fail_1:
	usbd_close_pipe(sc->sc_bulkin_pipe);
	sc->sc_bulkin_pipe = NULL;
fail_0:
	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);
	return (error);

bad:
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		ucom_cleanup(sc);
	}

	DPRINTF(("%s: ucomopen: failed\n", USBDEVNAME(sc->sc_dev)));

	return (error);
}

static int
ucomclose(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	struct ucom_softc *sc;
	struct tty *tp;
	int s;

	USB_GET_SC(ucom, UCOMUNIT(dev), sc);

	tp = sc->sc_tty;

	DPRINTF(("%s: ucomclose: unit = %d\n",
		USBDEVNAME(sc->sc_dev), UCOMUNIT(dev)));

	if (!ISSET(tp->t_state, TS_ISOPEN))
		goto quit;

	s = spltty();
	ttyld_close(tp, flag);
	ttyldoptim(tp);
	tty_close(tp);
	splx(s);

	if (sc->sc_dying)
		goto quit;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		ucom_cleanup(sc);
	}

	if (sc->sc_callback->ucom_close != NULL)
		sc->sc_callback->ucom_close(sc->sc_parent, sc->sc_portno);

    quit:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (0);
}

static int
ucomread(struct cdev *dev, struct uio *uio, int flag)
{
	struct ucom_softc *sc;
	struct tty *tp;
	int error;

	USB_GET_SC(ucom, UCOMUNIT(dev), sc);
	tp = sc->sc_tty;

	DPRINTF(("ucomread: tp = %p, flag = 0x%x\n", tp, flag));

	if (sc->sc_dying)
		return (EIO);

	error = ttyld_read(tp, uio, flag);

	DPRINTF(("ucomread: error = %d\n", error));

	return (error);
}

static int
ucomwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct ucom_softc *sc;
	struct tty *tp;
	int error;

	USB_GET_SC(ucom, UCOMUNIT(dev), sc);
	tp = sc->sc_tty;

	DPRINTF(("ucomwrite: tp = %p, flag = 0x%x\n", tp, flag));

	if (sc->sc_dying)
		return (EIO);

	error = ttyld_write(tp, uio, flag);

	DPRINTF(("ucomwrite: error = %d\n", error));

	return (error);
}

static int
ucomioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	struct ucom_softc *sc;
	struct tty *tp;
	int error;
	int s;
	int d;
#if defined(COMPAT_43)
	u_long oldcmd;
	struct termios term;
#endif

	USB_GET_SC(ucom, UCOMUNIT(dev), sc);
	tp = sc->sc_tty;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucomioctl: cmd = 0x%08lx\n", cmd));

#if defined(COMPAT_43)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t)&term;
#endif

	error = ttyioctl(dev, cmd, data, flag, p);
	ttyldoptim(tp);
	if (error != ENOTTY) {
		DPRINTF(("ucomioctl: l_ioctl: error = %d\n", error));
		return (error);
	}

	s = spltty();

	if (sc->sc_callback->ucom_ioctl != NULL) {
		error = sc->sc_callback->ucom_ioctl(sc->sc_parent,
						    sc->sc_portno,
						    cmd, data, flag, p);
		if (error >= 0)
			return (error);
	}

	error = 0;

	DPRINTF(("ucomioctl: our cmd = 0x%08lx\n", cmd));

	switch (cmd) {
	case TIOCSBRK:
		DPRINTF(("ucomioctl: TIOCSBRK\n"));
		ucom_break(sc, 1);
		break;
	case TIOCCBRK:
		DPRINTF(("ucomioctl: TIOCCBRK\n"));
		ucom_break(sc, 0);
		break;

	case TIOCSDTR:
		DPRINTF(("ucomioctl: TIOCSDTR\n"));
		(void)ucomctl(sc, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		DPRINTF(("ucomioctl: TIOCCDTR\n"));
		(void)ucomctl(sc, TIOCM_DTR, DMBIC);
		break;

	case TIOCMSET:
		d = *(int *)data;
		DPRINTF(("ucomioctl: TIOCMSET, 0x%x\n", d));
		(void)ucomctl(sc, d, DMSET);
		break;
	case TIOCMBIS:
		d = *(int *)data;
		DPRINTF(("ucomioctl: TIOCMBIS, 0x%x\n", d));
		(void)ucomctl(sc, d, DMBIS);
		break;
	case TIOCMBIC:
		d = *(int *)data;
		DPRINTF(("ucomioctl: TIOCMBIC, 0x%x\n", d));
		(void)ucomctl(sc, d, DMBIC);
		break;
	case TIOCMGET:
		d = ucomctl(sc, 0, DMGET);
		DPRINTF(("ucomioctl: TIOCMGET, 0x%x\n", d));
		*(int *)data = d;
		break;

	default:
		DPRINTF(("ucomioctl: error: our cmd = 0x%08lx\n", cmd));
		error = ENOTTY;
		break;
	}

	splx(s);

	return (error);
}

Static int
ucomctl(struct ucom_softc *sc, int bits, int how)
{
	int	mcr;
	int	msr;
	int	onoff;

	DPRINTF(("ucomctl: bits = 0x%x, how = %d\n", bits, how));

	if (how == DMGET) {
		SET(bits, TIOCM_LE);		/* always set TIOCM_LE bit */
		DPRINTF(("ucomctl: DMGET: LE"));

		mcr = sc->sc_mcr;
		if (ISSET(mcr, UMCR_DTR)) {
			SET(bits, TIOCM_DTR);
			DPRINTF((" DTR"));
		}
		if (ISSET(mcr, UMCR_RTS)) {
			SET(bits, TIOCM_RTS);
			DPRINTF((" RTS"));
		}

		msr = sc->sc_msr;
		if (ISSET(msr, UMSR_CTS)) {
			SET(bits, TIOCM_CTS);
			DPRINTF((" CTS"));
		}
		if (ISSET(msr, UMSR_DCD)) {
			SET(bits, TIOCM_CD);
			DPRINTF((" CD"));
		}
		if (ISSET(msr, UMSR_DSR)) {
			SET(bits, TIOCM_DSR);
			DPRINTF((" DSR"));
		}
		if (ISSET(msr, UMSR_RI)) {
			SET(bits, TIOCM_RI);
			DPRINTF((" RI"));
		}

		DPRINTF(("\n"));

		return (bits);
	}

	mcr = 0;
	if (ISSET(bits, TIOCM_DTR))
		SET(mcr, UMCR_DTR);
	if (ISSET(bits, TIOCM_RTS))
		SET(mcr, UMCR_RTS);

	switch (how) {
	case DMSET:
		sc->sc_mcr = mcr;
		break;
	case DMBIS:
		sc->sc_mcr |= mcr;
		break;
	case DMBIC:
		sc->sc_mcr &= ~mcr;
		break;
	}

	onoff = ISSET(sc->sc_mcr, UMCR_DTR) ? 1 : 0;
	ucom_dtr(sc, onoff);

	onoff = ISSET(sc->sc_mcr, UMCR_RTS) ? 1 : 0;
	ucom_rts(sc, onoff);

	return (0);
}

Static void
ucom_break(struct ucom_softc *sc, int onoff)
{
	DPRINTF(("ucom_break: onoff = %d\n", onoff));

	if (sc->sc_callback->ucom_set == NULL)
		return;
	sc->sc_callback->ucom_set(sc->sc_parent, sc->sc_portno,
				  UCOM_SET_BREAK, onoff);
}

Static void
ucom_dtr(struct ucom_softc *sc, int onoff)
{
	DPRINTF(("ucom_dtr: onoff = %d\n", onoff));

	if (sc->sc_callback->ucom_set == NULL)
		return;
	sc->sc_callback->ucom_set(sc->sc_parent, sc->sc_portno,
				  UCOM_SET_DTR, onoff);
}

Static void
ucom_rts(struct ucom_softc *sc, int onoff)
{
	DPRINTF(("ucom_rts: onoff = %d\n", onoff));

	if (sc->sc_callback->ucom_set == NULL)
		return;
	sc->sc_callback->ucom_set(sc->sc_parent, sc->sc_portno,
				  UCOM_SET_RTS, onoff);
}

void
ucom_status_change(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	u_char old_msr;
	int onoff;

	if (sc->sc_callback->ucom_get_status == NULL) {
		sc->sc_lsr = 0;
		sc->sc_msr = 0;
		return;
	}

	old_msr = sc->sc_msr;
	sc->sc_callback->ucom_get_status(sc->sc_parent, sc->sc_portno,
					 &sc->sc_lsr, &sc->sc_msr);
	if (ISSET((sc->sc_msr ^ old_msr), UMSR_DCD)) {
		if (sc->sc_poll == 0)
			return;
		onoff = ISSET(sc->sc_msr, UMSR_DCD) ? 1 : 0;
		DPRINTF(("ucom_status_change: DCD changed to %d\n", onoff));
		ttyld_modem(tp, onoff);
	}
}

Static int
ucomparam(struct tty *tp, struct termios *t)
{
	struct ucom_softc *sc;
	int error;
	usbd_status uerr;

	USB_GET_SC(ucom, UCOMUNIT(tp->t_dev), sc);

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucomparam: sc = %p\n", sc));

	/* Check requested parameters. */
	if (t->c_ospeed < 0) {
		DPRINTF(("ucomparam: negative ospeed\n"));
		return (EINVAL);
	}
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed) {
		DPRINTF(("ucomparam: mismatch ispeed and ospeed\n"));
		return (EINVAL);
	}

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	if (sc->sc_callback->ucom_param == NULL)
		return (0);

	ucomstopread(sc);

	error = sc->sc_callback->ucom_param(sc->sc_parent, sc->sc_portno, t);
	if (error) {
		DPRINTF(("ucomparam: callback: error = %d\n", error));
		return (error);
	}

	ttsetwater(tp);

	if (t->c_cflag & CRTS_IFLOW) {
		sc->sc_state |= UCS_RTS_IFLOW;
	} else if (sc->sc_state & UCS_RTS_IFLOW) {
		sc->sc_state &= ~UCS_RTS_IFLOW;
		(void)ucomctl(sc, UMCR_RTS, DMBIS);
	}

	ttyldoptim(tp);

	uerr = ucomstartread(sc);
	if (uerr != USBD_NORMAL_COMPLETION)
		return (EIO);

	return (0);
}

Static void
ucomstart(struct tty *tp)
{
	struct ucom_softc *sc;
	struct cblock *cbp;
	usbd_status err;
	int s;
	u_char *data;
	int cnt;

	USB_GET_SC(ucom, UCOMUNIT(tp->t_dev), sc);
	DPRINTF(("ucomstart: sc = %p\n", sc));

	if (sc->sc_dying)
		return;

	s = spltty();

	if (tp->t_state & TS_TBLOCK) {
		if (ISSET(sc->sc_mcr, UMCR_RTS) &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomstart: clear RTS\n"));
			(void)ucomctl(sc, UMCR_RTS, DMBIC);
		}
	} else {
		if (!ISSET(sc->sc_mcr, UMCR_RTS) &&
		    tp->t_rawq.c_cc <= tp->t_ilowat &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomstart: set RTS\n"));
			(void)ucomctl(sc, UMCR_RTS, DMBIS);
		}
	}

	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		DPRINTF(("ucomstart: stopped\n"));
		goto out;
	}

	if (tp->t_outq.c_cc <= tp->t_olowat) {
		if (ISSET(tp->t_state, TS_SO_OLOWAT)) {
			CLR(tp->t_state, TS_SO_OLOWAT);
			wakeup(TSA_OLOWAT(tp));
		}
		selwakeuppri(&tp->t_wsel, TTIPRI);
		if (tp->t_outq.c_cc == 0) {
			if (ISSET(tp->t_state, TS_BUSY | TS_SO_OCOMPLETE) ==
			    TS_SO_OCOMPLETE && tp->t_outq.c_cc == 0) {
				CLR(tp->t_state, TS_SO_OCOMPLETE);
				wakeup(TSA_OCOMPLETE(tp));
			}
			goto out;
		}
	}

	/* Grab the first contiguous region of buffer space. */
	data = tp->t_outq.c_cf;
	cbp = (struct cblock *) ((intptr_t) tp->t_outq.c_cf & ~CROUND);
	cnt = min((char *) (cbp+1) - tp->t_outq.c_cf, tp->t_outq.c_cc);

	if (cnt == 0) {
		DPRINTF(("ucomstart: cnt == 0\n"));
		goto out;
	}

	SET(tp->t_state, TS_BUSY);

	if (cnt > sc->sc_obufsize) {
		DPRINTF(("ucomstart: big buffer %d chars\n", cnt));
		cnt = sc->sc_obufsize;
	}
	if (sc->sc_callback->ucom_write != NULL)
		sc->sc_callback->ucom_write(sc->sc_parent, sc->sc_portno,
					    sc->sc_obuf, data, &cnt);
	else
		memcpy(sc->sc_obuf, data, cnt);

	DPRINTF(("ucomstart: %d chars\n", cnt));
	usbd_setup_xfer(sc->sc_oxfer, sc->sc_bulkout_pipe,
			(usbd_private_handle)sc, sc->sc_obuf, cnt,
			USBD_NO_COPY, USBD_NO_TIMEOUT, ucomwritecb);
	/* What can we do on error? */
	err = usbd_transfer(sc->sc_oxfer);
	if (err != USBD_IN_PROGRESS)
		printf("ucomstart: err=%s\n", usbd_errstr(err));

	ttwwakeup(tp);

    out:
	splx(s);
}

Static void
ucomstop(struct tty *tp, int flag)
{
	struct ucom_softc *sc;
	int s;

	USB_GET_SC(ucom, UCOMUNIT(tp->t_dev), sc);

	DPRINTF(("ucomstop: %d\n", flag));

	if (flag & FREAD) {
		DPRINTF(("ucomstop: read\n"));
		ucomstopread(sc);
		ucomstartread(sc);
	}

	if (flag & FWRITE) {
		DPRINTF(("ucomstop: write\n"));
		s = spltty();
		if (ISSET(tp->t_state, TS_BUSY)) {
			/* XXX do what? */
			if (!ISSET(tp->t_state, TS_TTSTOP))
				SET(tp->t_state, TS_FLUSH);
		}
		splx(s);
	}

	ucomstart(tp);

	DPRINTF(("ucomstop: done\n"));
}

Static void
ucomwritecb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct ucom_softc *sc = (struct ucom_softc *)p;
	struct tty *tp = sc->sc_tty;
	u_int32_t cc;
	int s;

	DPRINTF(("ucomwritecb: status = %d\n", status));

	if (status == USBD_CANCELLED || sc->sc_dying)
		goto error;

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: ucomwritecb: %s\n",
		       USBDEVNAME(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		goto error;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
	DPRINTF(("ucomwritecb: cc = %d\n", cc));
	if (cc <= sc->sc_opkthdrlen) {
		printf("%s: sent size too small, cc = %d\n",
		       USBDEVNAME(sc->sc_dev), cc);
		goto error;
	}

	/* convert from USB bytes to tty bytes */
	cc -= sc->sc_opkthdrlen;

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, cc);
	ttyld_start(tp);
	splx(s);

	return;

  error:
	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	splx(s);
	return;
}

Static usbd_status
ucomstartread(struct ucom_softc *sc)
{
	usbd_status err;

	DPRINTF(("ucomstartread: start\n"));

	sc->sc_state &= ~UCS_RXSTOP;

	if (sc->sc_bulkin_pipe == NULL)
		return (USBD_NORMAL_COMPLETION);

	usbd_setup_xfer(sc->sc_ixfer, sc->sc_bulkin_pipe,
			(usbd_private_handle)sc,
			sc->sc_ibuf, sc->sc_ibufsize,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, ucomreadcb);

	err = usbd_transfer(sc->sc_ixfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("ucomstartread: err = %s\n", usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
ucomreadcb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct ucom_softc *sc = (struct ucom_softc *)p;
	struct tty *tp = sc->sc_tty;
	usbd_status err;
	u_int32_t cc;
	u_char *cp;
	int lostcc;
	int s;

	DPRINTF(("ucomreadcb: status = %d\n", status));

	if (status != USBD_NORMAL_COMPLETION) {
		if (!(sc->sc_state & UCS_RXSTOP))
			printf("%s: ucomreadcb: %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);
	DPRINTF(("ucomreadcb: got %d chars, tp = %p\n", cc, tp));
	if (cc == 0)
		goto resubmit;

	if (sc->sc_callback->ucom_read != NULL)
		sc->sc_callback->ucom_read(sc->sc_parent, sc->sc_portno,
					   &cp, &cc);

	if (cc > sc->sc_ibufsize) {
		printf("%s: invalid receive data size, %d chars\n",
		       USBDEVNAME(sc->sc_dev), cc);
		goto resubmit;
	}
	if (cc < 1)
		goto resubmit;

	s = spltty();
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		if (tp->t_rawq.c_cc + cc > tp->t_ihiwat
		    && (sc->sc_state & UCS_RTS_IFLOW
			|| tp->t_iflag & IXOFF)
		    && !(tp->t_state & TS_TBLOCK))
			ttyblock(tp);
		lostcc = b_to_q((char *)cp, cc, &tp->t_rawq);
		tp->t_rawcc += cc;
		ttwakeup(tp);
		if (tp->t_state & TS_TTSTOP
		    && (tp->t_iflag & IXANY
			|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
			tp->t_state &= ~TS_TTSTOP;
			tp->t_lflag &= ~FLUSHO;
			ucomstart(tp);
		}
		if (lostcc > 0)
			printf("%s: lost %d chars\n", USBDEVNAME(sc->sc_dev),
			       lostcc);
	} else {
		/* Give characters to tty layer. */
		while (cc > 0) {
			DPRINTFN(7, ("ucomreadcb: char = 0x%02x\n", *cp));
			if (ttyld_rint(tp, *cp) == -1) {
				/* XXX what should we do? */
				printf("%s: lost %d chars\n",
				       USBDEVNAME(sc->sc_dev), cc);
				break;
			}
			cc--;
			cp++;
		}
	}
	splx(s);

    resubmit:
	err = ucomstartread(sc);
	if (err) {
		printf("%s: read start failed\n", USBDEVNAME(sc->sc_dev));
		/* XXX what should we dow now? */
	}

	if ((sc->sc_state & UCS_RTS_IFLOW) && !ISSET(sc->sc_mcr, UMCR_RTS)
	    && !(tp->t_state & TS_TBLOCK))
		ucomctl(sc, UMCR_RTS, DMBIS);
}

Static void
ucom_cleanup(struct ucom_softc *sc)
{
	DPRINTF(("ucom_cleanup: closing pipes\n"));

	ucom_shutdown(sc);
	if (sc->sc_bulkin_pipe != NULL) {
		sc->sc_state |= UCS_RXSTOP;
		usbd_abort_pipe(sc->sc_bulkin_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
	}
	if (sc->sc_bulkout_pipe != NULL) {
		usbd_abort_pipe(sc->sc_bulkout_pipe);
		usbd_close_pipe(sc->sc_bulkout_pipe);
		sc->sc_bulkout_pipe = NULL;
	}
	if (sc->sc_ixfer != NULL) {
		usbd_free_xfer(sc->sc_ixfer);
		sc->sc_ixfer = NULL;
	}
	if (sc->sc_oxfer != NULL) {
		usbd_free_xfer(sc->sc_oxfer);
		sc->sc_oxfer = NULL;
	}
}

Static void
ucomstopread(struct ucom_softc *sc)
{
	usbd_status err;

	DPRINTF(("ucomstopread: enter\n"));

	if (!(sc->sc_state & UCS_RXSTOP)) {
		sc->sc_state |= UCS_RXSTOP;
		if (sc->sc_bulkin_pipe == NULL) {
			DPRINTF(("ucomstopread: bulkin pipe NULL\n"));
			return;
		}
		err = usbd_abort_pipe(sc->sc_bulkin_pipe);
		if (err) {
			DPRINTF(("ucomstopread: err = %s\n",
				 usbd_errstr(err)));
		}
	}

	DPRINTF(("ucomstopread: leave\n"));
}
