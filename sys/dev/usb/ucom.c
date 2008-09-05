/*	$NetBSD: ucom.c,v 1.40 2001/11/13 06:24:54 lukem Exp $	*/

/*-
 * Copyright (c) 2001-2003, 2005, 2008
 *	Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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

static int ucom_modevent(module_t, int, void *);
static void ucom_cleanup(struct ucom_softc *);
static void ucom_shutdown(struct ucom_softc *);
static void ucom_dtr(struct ucom_softc *, int);
static void ucom_rts(struct ucom_softc *, int);
static void ucombreak(struct ucom_softc *, int);
static usbd_status ucomstartread(struct ucom_softc *);
static void ucomreadcb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void ucomwritecb(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void ucomstopread(struct ucom_softc *);

static tsw_open_t ucomtty_open;
static tsw_close_t ucomtty_close;
static tsw_outwakeup_t ucomtty_outwakeup;
static tsw_ioctl_t ucomtty_ioctl;
static tsw_param_t ucomtty_param;
static tsw_modem_t ucomtty_modem;
static tsw_free_t ucomtty_free;

static struct ttydevsw ucomtty_class = {
	.tsw_flags	= TF_INITLOCK|TF_CALLOUT,
	.tsw_open	= ucomtty_open,
	.tsw_close	= ucomtty_close,
	.tsw_outwakeup	= ucomtty_outwakeup,
	.tsw_ioctl	= ucomtty_ioctl,
	.tsw_param	= ucomtty_param,
	.tsw_modem	= ucomtty_modem,
	.tsw_free	= ucomtty_free,
};

devclass_t ucom_devclass;

static moduledata_t ucom_mod = {
	"ucom",
	ucom_modevent,
	NULL
};

DECLARE_MODULE(ucom, ucom_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_DEPEND(ucom, usb, 1, 1, 1);
MODULE_VERSION(ucom, UCOM_MODVER);

static int
ucom_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		break;
	default:
		return (EOPNOTSUPP);
		break;
	}
	return (0);
}

void
ucom_attach_tty(struct ucom_softc *sc, char* fmt, int unit)
{
	struct tty *tp;

	sc->sc_tty = tp = tty_alloc(&ucomtty_class, sc, &Giant);
	tty_makedev(tp, NULL, fmt, unit);
}

int
ucom_attach(struct ucom_softc *sc)
{

	ucom_attach_tty(sc, "U%d", device_get_unit(sc->sc_dev));

	DPRINTF(("ucom_attach: ttycreate: tp = %p, %s\n",
	    sc->sc_tty, sc->sc_tty->t_dev->si_name));

	return (0);
}

int
ucom_detach(struct ucom_softc *sc)
{
	DPRINTF(("ucom_detach: sc = %p, tp = %p\n", sc, sc->sc_tty));

	tty_lock(sc->sc_tty);
	sc->sc_dying = 1;

	if (sc->sc_bulkin_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	tty_rel_gone(sc->sc_tty);

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
	if (tp->t_termios.c_cflag & HUPCL) {
		(void)ucomtty_modem(tp, 0, SER_DTR);
#if 0
		(void)tsleep(sc, TTIPRI, "ucomsd", hz);
#endif
	}
}

static int
ucomtty_open(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);
	usbd_status err;
	int error;

	if (sc->sc_dying)
		return (ENXIO);

	DPRINTF(("%s: ucomtty_open: tp = %p\n", device_get_nameunit(sc->sc_dev), tp));

	sc->sc_poll = 0;
	sc->sc_lsr = sc->sc_msr = sc->sc_mcr = 0;

	(void)ucomtty_modem(tp, SER_DTR | SER_RTS, 0);

	/* Device specific open */
	if (sc->sc_callback->ucom_open != NULL) {
		error = sc->sc_callback->ucom_open(sc->sc_parent,
						   sc->sc_portno);
		if (error) {
			ucom_cleanup(sc);
			return (error);
		}
	}

	DPRINTF(("ucomtty_open: open pipes in = %d out = %d\n",
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
ucomtty_close(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);

	DPRINTF(("%s: ucomtty_close \n", device_get_nameunit(sc->sc_dev)));

	ucom_cleanup(sc);

	if (sc->sc_callback->ucom_close != NULL)
		sc->sc_callback->ucom_close(sc->sc_parent, sc->sc_portno);
}

static int
ucomtty_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *p)
{
	struct ucom_softc *sc = tty_softc(tp);
	int error;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucomioctl: cmd = 0x%08lx\n", cmd));

	switch (cmd) {
	case TIOCSBRK:
		ucombreak(sc, 1);
		return (0);
	case TIOCCBRK:
		ucombreak(sc, 0);
		return (0);
	}

	error = ENOIOCTL;
	if (sc->sc_callback->ucom_ioctl != NULL)
		error = sc->sc_callback->ucom_ioctl(sc->sc_parent,
						    sc->sc_portno,
						    cmd, data, p);
	return (error);
}

static int
ucomtty_modem(struct tty *tp, int sigon, int sigoff)
{
	struct ucom_softc *sc = tty_softc(tp);
	int	mcr;
	int	msr;
	int	onoff;

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
ucombreak(struct ucom_softc *sc, int onoff)
{
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
		ttydisc_modem(tp, onoff);
	}
}

static int
ucomtty_param(struct tty *tp, struct termios *t)
{
	struct ucom_softc *sc = tty_softc(tp);
	int error;
	usbd_status uerr;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ucomtty_param: sc = %p\n", sc));

	/* Check requested parameters. */
	if (t->c_ospeed < 0) {
		DPRINTF(("ucomtty_param: negative ospeed\n"));
		return (EINVAL);
	}
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed) {
		DPRINTF(("ucomtty_param: mismatch ispeed and ospeed\n"));
		return (EINVAL);
	}
	t->c_ispeed = t->c_ospeed;

	if (sc->sc_callback->ucom_param == NULL)
		return (0);

	ucomstopread(sc);

	error = sc->sc_callback->ucom_param(sc->sc_parent, sc->sc_portno, t);
	if (error) {
		DPRINTF(("ucomtty_param: callback: error = %d\n", error));
		return (error);
	}

#if 0
	ttsetwater(tp);
#endif

	if (t->c_cflag & CRTS_IFLOW) {
		sc->sc_state |= UCS_RTS_IFLOW;
	} else if (sc->sc_state & UCS_RTS_IFLOW) {
		sc->sc_state &= ~UCS_RTS_IFLOW;
		(void)ucomtty_modem(tp, SER_RTS, 0);
	}

#if 0
	ttyldoptim(tp);
#endif

	uerr = ucomstartread(sc);
	if (uerr != USBD_NORMAL_COMPLETION)
		return (EIO);

	return (0);
}

static void
ucomtty_free(void *sc)
{
	/*
	 * Our softc gets deallocated earlier on.
	 * XXX: we should make sure the TTY device name doesn't get
	 * recycled before we end up here!
	 */
}

static void
ucomtty_outwakeup(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);
	usbd_status err;
	size_t cnt;

	DPRINTF(("ucomtty_outwakeup: sc = %p\n", sc));

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

	/* XXX: hardware flow control. We should use inwakeup here. */
#if 0
	if (tp->t_state & TS_TBLOCK) {
		if (ISSET(sc->sc_mcr, SER_RTS) &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomtty_outwakeup: clear RTS\n"));
			(void)ucomtty_modem(tp, 0, SER_RTS);
		}
	} else {
		if (!ISSET(sc->sc_mcr, SER_RTS) &&
		    tp->t_rawq.c_cc <= tp->t_ilowat &&
		    ISSET(sc->sc_state, UCS_RTS_IFLOW)) {
			DPRINTF(("ucomtty_outwakeup: set RTS\n"));
			(void)ucomtty_modem(tp, SER_RTS, 0);
		}
	}
#endif

	if (sc->sc_state & UCS_TXBUSY)
		return;

	sc->sc_state |= UCS_TXBUSY;
	if (sc->sc_callback->ucom_write != NULL)
		cnt = sc->sc_callback->ucom_write(sc->sc_parent,
		    sc->sc_portno, tp, sc->sc_obuf, sc->sc_obufsize);
	else
		cnt = ttydisc_getc(tp, sc->sc_obuf, sc->sc_obufsize);

	if (cnt == 0) {
		DPRINTF(("ucomtty_outwakeup: cnt == 0\n"));
		sc->sc_state &= ~UCS_TXBUSY;
		return;
	}
	sc->sc_obufactive = cnt;

	DPRINTF(("ucomtty_outwakeup: %zu chars\n", cnt));
	usbd_setup_xfer(sc->sc_oxfer, sc->sc_bulkout_pipe,
			(usbd_private_handle)sc, sc->sc_obuf, cnt,
			USBD_NO_COPY, USBD_NO_TIMEOUT, ucomwritecb);
	/* What can we do on error? */
	err = usbd_transfer(sc->sc_oxfer);
	if (err != USBD_IN_PROGRESS) {
		printf("ucomtty_outwakeup: err=%s\n", usbd_errstr(err));
		sc->sc_state &= ~UCS_TXBUSY;
	}
}

#if 0
static void
ucomstop(struct tty *tp, int flag)
{
	struct ucom_softc *sc = tty_softc(tp);
	int s;

	DPRINTF(("ucomstop: %d\n", flag));

	if ((flag & FREAD) && (sc->sc_state & UCS_RXSTOP) == 0) {
		DPRINTF(("ucomstop: read\n"));
		ucomstopread(sc);
		ucomstartread(sc);
	}

	if (flag & FWRITE) {
		DPRINTF(("ucomstop: write\n"));
		if (ISSET(tp->t_state, TS_BUSY)) {
			/* XXX do what? */
			if (!ISSET(tp->t_state, TS_TTSTOP))
				SET(tp->t_state, TS_FLUSH);
		}
	}

	ucomtty_outwakeup(tp);

	DPRINTF(("ucomstop: done\n"));
}
#endif

static void
ucomwritecb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct ucom_softc *sc = (struct ucom_softc *)p;
	struct tty *tp = sc->sc_tty;
	u_int32_t cc;

	DPRINTF(("ucomwritecb: status = %d\n", status));

	if (status == USBD_CANCELLED || sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: ucomwritecb: %s\n",
		       device_get_nameunit(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
	DPRINTF(("ucomwritecb: cc = %d\n", cc));
	if (cc <= sc->sc_opkthdrlen) {
		printf("%s: sent size too small, cc = %d\n",
		       device_get_nameunit(sc->sc_dev), cc);
		return;
	}

	/* convert from USB bytes to tty bytes */
	cc -= sc->sc_opkthdrlen;
	if (cc != sc->sc_obufactive)
		panic("Partial write of %d of %d bytes, not supported\n",
		    cc, sc->sc_obufactive);

	sc->sc_state &= ~UCS_TXBUSY;
#if 0
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, cc);
#endif
	ucomtty_outwakeup(tp);
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
	
	/* Give characters to tty layer. */
	while (cc > 0) {
		DPRINTFN(7, ("ucomreadcb: char = 0x%02x\n", *cp));
		if (ttydisc_rint(tp, *cp, 0) == -1) {
			/* XXX what should we do? */
			printf("%s: lost %d chars\n",
			       device_get_nameunit(sc->sc_dev), cc);
			break;
		}
		cc--;
		cp++;
	}
	ttydisc_rint_done(tp);

    resubmit:
	err = ucomstartread(sc);
	if (err) {
		printf("%s: read start failed\n", device_get_nameunit(sc->sc_dev));
		/* XXX what should we dow now? */
	}

#if 0
	if ((sc->sc_state & UCS_RTS_IFLOW) && !ISSET(sc->sc_mcr, SER_RTS)
	    && !(tp->t_state & TS_TBLOCK))
		ucomtty_modem(tp, SER_RTS, 0);
#endif
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
