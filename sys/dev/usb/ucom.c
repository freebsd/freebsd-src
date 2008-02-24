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
__FBSDID("$FreeBSD: src/sys/dev/usb/ucom.c,v 1.64 2007/06/25 06:40:20 imp Exp $");

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
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/file.h>
#include <sys/selinfo.h>
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
					printf x; \
			} while (0)

#define DPRINTFN(n, x)	do { \
				if (ucomdebug > (n)) \
					printf x; \
			} while (0)
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

static void ucom_cleanup(struct ucom_softc *);
static int ucomparam(struct tty *, struct termios *);
static void ucomstart(struct tty *);
static void ucomstop(struct tty *, int);
static void ucom_shutdown(struct ucom_softc *);
static void ucom_dtr(struct ucom_softc *, int);
static void ucom_rts(struct ucom_softc *, int);
static void ucombreak(struct tty *, int);
static usbd_status ucomstartread(struct ucom_softc *);
static void ucomreadcb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void ucomwritecb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void ucomstopread(struct ucom_softc *);

static t_open_t  ucomopen;
static t_close_t ucomclose;
static t_modem_t ucommodem;
static t_ioctl_t ucomioctl;

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

	sc->sc_tty = tp = ttyalloc();
	tp->t_sc = sc;
	tp->t_oproc = ucomstart;
	tp->t_param = ucomparam;
	tp->t_stop = ucomstop;
	tp->t_break = ucombreak;
	tp->t_open = ucomopen;
	tp->t_close = ucomclose;
	tp->t_modem = ucommodem;
	tp->t_ioctl = ucomioctl;

	DPRINTF(("ucom_attach: tty_attach tp = %p\n", tp));

	ttycreate(tp, TS_CALLOUT, "U%d", unit);
	DPRINTF(("ucom_attach: ttycreate: ttyU%d\n", unit));

	return (0);
}

int
ucom_detach(struct ucom_softc *sc)
{
	int s;

	DPRINTF(("ucom_detach: sc = %p, tp = %p\n", sc, sc->sc_tty));

	sc->sc_dying = 1;
	ttygone(sc->sc_tty);
	if (sc->sc_tty->t_state & TS_ISOPEN)
		ucomclose(sc->sc_tty);

	if (sc->sc_bulkin_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	ttyfree(sc->sc_tty);

	s = splusb();
	splx(s);

	return (0);
}

static void
ucom_shutdown(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucom_shutdown\n"));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		(void)ucommodem(tp, 0, SER_DTR);
		(void)tsleep(sc, TTIPRI, "ucomsd", hz);
	}
}

static int
ucomopen(struct tty *tp, struct cdev *dev)
{
	struct ucom_softc *sc;
	usbd_status err;
	int error;

	sc = tp->t_sc;

	if (sc->sc_dying)
		return (ENXIO);

	DPRINTF(("%s: ucomopen: tp = %p\n", device_get_nameunit(sc->sc_dev), tp));

	sc->sc_poll = 0;
	sc->sc_lsr = sc->sc_msr = sc->sc_mcr = 0;

	(void)ucommodem(tp, SER_DTR | SER_RTS, 0);

	/* Device specific open */
	if (sc->sc_callback->ucom_open != NULL) {
		error = sc->sc_callback->ucom_open(sc->sc_parent,
						   sc->sc_portno);
		if (error) {
			ucom_cleanup(sc);
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
		       device_get_nameunit(sc->sc_dev), sc->sc_bulkin_no,
		       usbd_errstr(err));
		error = EIO;
		goto fail;
	}
	/* Bulk-out pipe */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		printf("%s: open bulk out error (addr %d): %s\n",
		       device_get_nameunit(sc->sc_dev), sc->sc_bulkout_no,
		       usbd_errstr(err));
		error = EIO;
		goto fail;
	}

	/* Allocate a request and an input buffer and start reading. */
	sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_ixfer == NULL) {
		error = ENOMEM;
		goto fail;
	}

	sc->sc_ibuf = usbd_alloc_buffer(sc->sc_ixfer,
					sc->sc_ibufsizepad);
	if (sc->sc_ibuf == NULL) {
		error = ENOMEM;
		goto fail;
	}

	sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_oxfer == NULL) {
		error = ENOMEM;
		goto fail;
	}

	sc->sc_obuf = usbd_alloc_buffer(sc->sc_oxfer,
					sc->sc_obufsize +
					sc->sc_opkthdrlen);
	if (sc->sc_obuf == NULL) {
		error = ENOMEM;
		goto fail;
	}

	sc->sc_state |= UCS_RXSTOP;
	ucomstartread(sc);

	sc->sc_poll = 1;

	return (0);

fail:
	ucom_cleanup(sc);
	return (error);
}

static void
ucomclose(struct tty *tp)
{
	struct ucom_softc *sc;

	sc = tp->t_sc;

	DPRINTF(("%s: ucomclose \n", device_get_nameunit(sc->sc_dev)));

	ucom_cleanup(sc);

	if (sc->sc_callback->ucom_close != NULL)
		sc->sc_callback->ucom_close(sc->sc_parent, sc->sc_portno);
}

static int
ucomioctl(struct tty *tp, u_long cmd, void *data, int flag, struct thread *p)
{
	struct ucom_softc *sc;
	int error;

	sc = tp->t_sc;;
	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucomioctl: cmd = 0x%08lx\n", cmd));

	error = ENOTTY;
	if (sc->sc_callback->ucom_ioctl != NULL)
		error = sc->sc_callback->ucom_ioctl(sc->sc_parent,
						    sc->sc_portno,
						    cmd, data, flag, p);
	return (error);
}

static int
ucommodem(struct tty *tp, int sigon, int sigoff)
{
	struct ucom_softc *sc;
	int	mcr;
	int	msr;
	int	onoff;

	sc = tp->t_sc;

	if (sigon == 0 && sigoff == 0) {
		mcr = sc->sc_mcr;
		if (ISSET(mcr, SER_DTR))
			sigon |= SER_DTR;
		if (ISSET(mcr, SER_RTS))
			sigon |= SER_RTS;

		msr = sc->sc_msr;
		if (ISSET(msr, SER_CTS))
			sigon |= SER_CTS;
		if (ISSET(msr, SER_DCD))
			sigon |= SER_DCD;
		if (ISSET(msr, SER_DSR))
			sigon |= SER_DSR;
		if (ISSET(msr, SER_RI))
			sigon |= SER_RI;
		return (sigon);
	}

	mcr = sc->sc_mcr;
	if (ISSET(sigon, SER_DTR))
		mcr |= SER_DTR;
	if (ISSET(sigoff, SER_DTR))
		mcr &= ~SER_DTR;
	if (ISSET(sigon, SER_RTS))
		mcr |= SER_RTS;
	if (ISSET(sigoff, SER_RTS))
		mcr &= ~SER_RTS;
	sc->sc_mcr = mcr;

	onoff = ISSET(sc->sc_mcr, SER_DTR) ? 1 : 0;
	ucom_dtr(sc, onoff);

	onoff = ISSET(sc->sc_mcr, SER_RTS) ? 1 : 0;
	ucom_rts(sc, onoff);

	return (0);
}

static void
ucombreak(struct tty *tp, int onoff)
{
	struct ucom_softc *sc;

	sc = tp->t_sc;
	DPRINTF(("ucombreak: onoff = %d\n", onoff));
	if (sc->sc_callback->ucom_set == NULL)
		return;
	sc->sc_callback->ucom_set(sc->sc_parent, sc->sc_portno,
				  UCOM_SET_BREAK, onoff);
}

static void
ucom_dtr(struct ucom_softc *sc, int onoff)
{
	DPRINTF(("ucom_dtr: onoff = %d\n", onoff));

	if (sc->sc_callback->ucom_set == NULL)
		return;
	sc->sc_callback->ucom_set(sc->sc_parent, sc->sc_portno,
				  UCOM_SET_DTR, onoff);
}

static void
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
	if (ISSET((sc->sc_msr ^ old_msr), SER_DCD)) {
		if (sc->sc_poll == 0)
			return;
		onoff = ISSET(sc->sc_msr, SER_DCD) ? 1 : 0;
		DPRINTF(("ucom_status_change: DCD changed to %d\n", onoff));
		ttyld_modem(tp, onoff);
	}
}

static int
ucomparam(struct tty *tp, struct termios *t)
{
	struct ucom_softc *sc;
	int error;
	usbd_status uerr;

	sc = tp->t_sc;

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
		(void)ucommodem(tp, SER_RTS, 0);
	}

	ttyldoptim(tp);

	uerr = ucomstartread(sc);
	if (uerr != USBD_NORMAL_COMPLETION)
		return (EIO);

	return (0);
}

static void
ucomstart(struct tty *tp)
{
	struct ucom_softc *sc;
	struct cblock *cbp;
	usbd_status err;
	int s;
	u_char *data;
	int cnt;

	sc = tp->t_sc;
	DPRINTF(("ucomstart: sc = %p\n", sc));

	if (sc->sc_dying)
		return;

	/*
	 * If there's no sc_oxfer, then ucomclose has removed it.  The buffer
	 * has just been flushed in the ttyflush() in ttyclose().  ttyflush()
	 * then calls tt_stop().  ucomstop calls ucomstart, so the right thing
	 * to do here is just abort if sc_oxfer is NULL, as everything else
	 * is cleaned up elsewhere.
	 */
	if (sc->sc_oxfer == NULL)
		return;

	s = spltty();

	if (tp->t_state & TS_TBLOCK) {
		if (ISSET(sc->sc_mcr, SER_RTS) &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomstart: clear RTS\n"));
			(void)ucommodem(tp, 0, SER_RTS);
		}
	} else {
		if (!ISSET(sc->sc_mcr, SER_RTS) &&
		    tp->t_rawq.c_cc <= tp->t_ilowat &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomstart: set RTS\n"));
			(void)ucommodem(tp, SER_RTS, 0);
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

static void
ucomstop(struct tty *tp, int flag)
{
	struct ucom_softc *sc;
	int s;

	sc = tp->t_sc;

	DPRINTF(("ucomstop: %d\n", flag));

	if ((flag & FREAD) && (sc->sc_state & UCS_RXSTOP) == 0) {
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

static void
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
		       device_get_nameunit(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		goto error;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
	DPRINTF(("ucomwritecb: cc = %d\n", cc));
	if (cc <= sc->sc_opkthdrlen) {
		printf("%s: sent size too small, cc = %d\n",
		       device_get_nameunit(sc->sc_dev), cc);
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

static usbd_status
ucomstartread(struct ucom_softc *sc)
{
	usbd_status err;

	DPRINTF(("ucomstartread: start\n"));

	if (sc->sc_bulkin_pipe == NULL || (sc->sc_state & UCS_RXSTOP) == 0)
		return (USBD_NORMAL_COMPLETION);
	sc->sc_state &= ~UCS_RXSTOP;

	usbd_setup_xfer(sc->sc_ixfer, sc->sc_bulkin_pipe,
			(usbd_private_handle)sc,
			sc->sc_ibuf, sc->sc_ibufsize,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, ucomreadcb);

	err = usbd_transfer(sc->sc_ixfer);
	if (err && err != USBD_IN_PROGRESS) {
		sc->sc_state |= UCS_RXSTOP;
		DPRINTF(("ucomstartread: err = %s\n", usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

static void
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
			       device_get_nameunit(sc->sc_dev), usbd_errstr(status));
		sc->sc_state |= UCS_RXSTOP;
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}
	sc->sc_state |= UCS_RXSTOP;

	usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);
	DPRINTF(("ucomreadcb: got %d chars, tp = %p\n", cc, tp));
	if (cc == 0)
		goto resubmit;

	if (sc->sc_callback->ucom_read != NULL)
		sc->sc_callback->ucom_read(sc->sc_parent, sc->sc_portno,
					   &cp, &cc);

	if (cc > sc->sc_ibufsize) {
		printf("%s: invalid receive data size, %d chars\n",
		       device_get_nameunit(sc->sc_dev), cc);
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
			printf("%s: lost %d chars\n", device_get_nameunit(sc->sc_dev),
			       lostcc);
	} else {
		/* Give characters to tty layer. */
		while (cc > 0) {
			DPRINTFN(7, ("ucomreadcb: char = 0x%02x\n", *cp));
			if (ttyld_rint(tp, *cp) == -1) {
				/* XXX what should we do? */
				printf("%s: lost %d chars\n",
				       device_get_nameunit(sc->sc_dev), cc);
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
		printf("%s: read start failed\n", device_get_nameunit(sc->sc_dev));
		/* XXX what should we dow now? */
	}

	if ((sc->sc_state & UCS_RTS_IFLOW) && !ISSET(sc->sc_mcr, SER_RTS)
	    && !(tp->t_state & TS_TBLOCK))
		ucommodem(tp, SER_RTS, 0);
}

static void
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

static void
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
