/*-
 * Copyright (c) 2004 Bernd Walter <ticso@freebsd.org>
 *
 * $URL: https://devel.bwct.de/svn/projects/ubser/ubser.c $
 * $Date: 2004-02-29 01:53:10 +0100 (Sun, 29 Feb 2004) $
 * $Author: ticso $
 * $Rev: 1127 $
 */

/*-
 * Copyright (c) 2001-2002, Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * BWCT serial adapter driver
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
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

#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#include <dev/usb/ubser.h>

#ifdef USB_DEBUG
static int ubserdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ubser, CTLFLAG_RW, 0, "USB ubser");
SYSCTL_INT(_hw_usb_ubser, OID_AUTO, debug, CTLFLAG_RW,
	   &ubserdebug, 0, "ubser debug level");
#define DPRINTF(x)      do { \
				if (ubserdebug) \
					logprintf x; \
			} while (0)

#define DPRINTFN(n, x)  do { \
				if (ubserdebug > (n)) \
					logprintf x; \
			} while (0)
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define ISSET(t, f)	((t) & (f))
#define SET(t, f)	(t) |= (f)
#define CLR(t, f)	(t) &= ~((unsigned)(f))

struct ubser_softc {
	USBBASEDEVICE		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;	/* data interface */
	int			sc_ifaceno;

	int			sc_refcnt;
	u_char			sc_dying;
	u_char			sc_opening;
	int			sc_state;
	uint8_t			sc_numser;

	int			sc_bulkin_no;	/* bulk in endpoint address */
	usbd_pipe_handle	sc_bulkin_pipe;	/* bulk in pipe */
	usbd_xfer_handle	sc_ixfer;	/* read request */
	u_char			*sc_ibuf;	/* read buffer */
	u_int			sc_ibufsize;	/* read buffer size */
	u_int			sc_ibufsizepad;	/* read buffer size padded */

	int			sc_bulkout_no;	/* bulk out endpoint address */
	usbd_pipe_handle	sc_bulkout_pipe;/* bulk out pipe */
	usbd_xfer_handle	sc_oxfer[8];	/* write request */
	u_char			*sc_obuf[8];	/* write buffer */
	u_int			sc_obufsize;	/* write buffer size */
	u_int			sc_opkthdrlen;	/* header length of
						   output packet */

	struct cdev *dev[8];
};

Static d_open_t  ubser_open;
Static d_close_t ubser_close;
Static d_read_t  ubser_read;
Static d_write_t ubser_write;
Static d_ioctl_t ubser_ioctl;

Static int ubserparam(struct tty *, struct termios *);
Static void ubserstart(struct tty *);
Static void ubserstop(struct tty *, int);
Static usbd_status ubserstartread(struct ubser_softc *);
Static void ubserreadcb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void ubserwritecb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void ubser_cleanup(struct ubser_softc *sc);

static struct cdevsw ubser_cdevsw = {
#if __FreeBSD_version > 502102
	.d_version = 	D_VERSION,
#endif
	.d_open =       ubser_open,
	.d_close =      ubser_close,
	.d_read =       ubser_read,
	.d_write =      ubser_write,
	.d_ioctl =      ubser_ioctl,
#if __FreeBSD_version < 502103
	.d_poll =       ttypoll,
	.d_kqfilter =   ttykqfilter,
#endif
	.d_name =       "ubser",
#if __FreeBSD_version > 502102
	.d_flags =      D_TTY | D_NEEDGIANT,
#else
	.d_flags =      D_TTY,
#endif
#if __FreeBSD_version < 500014
	.d_bmaj =       -1,
#endif
};

USB_DECLARE_DRIVER(ubser);

USB_MATCH(ubser)
{
	USB_MATCH_START(ubser, uaa);
	usb_string_descriptor_t us;
	usb_interface_descriptor_t *id;
	usb_device_descriptor_t *dd;
	int err, size;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	DPRINTFN(20,("ubser: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

	dd = usbd_get_device_descriptor(uaa->device);
	if (dd == NULL) {
		printf("ubser: failed to get device descriptor\n");
		return (UMATCH_NONE);
	}

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL) {
		printf("ubser: failed to get interface descriptor\n");
		return (UMATCH_NONE);
	}

	err = usbd_get_string_desc(uaa->device, dd->iManufacturer, 0, &us,
	    &size);
	if (err != 0)
		return (UMATCH_NONE);

	/* check if this is a BWCT vendor specific ubser interface */
	if (strcmp((char*)us.bString, "B\0W\0C\0T\0") == 0 &&
	    id->bInterfaceClass == 0xff && id->bInterfaceSubClass == 0x00)
		return (UMATCH_VENDOR_IFACESUBCLASS);

	return (UMATCH_NONE);
}

USB_ATTACH(ubser)
{
	USB_ATTACH_START(ubser, sc, uaa);
	usbd_device_handle udev = uaa->device;
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	usb_device_request_t req;
	char *devinfo;
	struct tty *tp;
	usbd_status err;
	int i;
	int alen;
	uint8_t epcount;

	devinfo = malloc(1024, M_USBDEV, M_WAITOK);
	usbd_devinfo(udev, 0, devinfo);
	USB_ATTACH_SETUP;

	DPRINTFN(10,("\nubser_attach: sc=%p\n", sc));

	sc->sc_udev = udev = uaa->device;
	sc->sc_iface = uaa->iface;

	for (i = 0; i < 8; i++) {
		sc->dev[i] = NULL;
	}

	/* get interface index */
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL) {
		printf("ubser: failed to get interface descriptor\n");
		return (UMATCH_NONE);
	}
	sc->sc_ifaceno = id->bInterfaceNumber;

	/* get number of serials */
	req.bmRequestType = UT_READ_VENDOR_INTERFACE;
	req.bRequest = VENDOR_GET_NUMSER;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 1);
	err = usbd_do_request_flags(udev, &req, &sc->sc_numser,
	    USBD_SHORT_XFER_OK, &alen, USBD_DEFAULT_TIMEOUT);
	if (err) {
		printf("%s: cannot get number of serials\n",
		    USBDEVNAME(sc->sc_dev));
		goto bad;
	} else if (alen != 1) {
		printf("%s: bogus answer on get_numser\n",
		    USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	if (sc->sc_numser > 8)
		sc->sc_numser = 8;
	printf("%s: found %i serials\n", USBDEVNAME(sc->sc_dev), sc->sc_numser);

	sc->sc_ibufsize = 7;
	sc->sc_ibufsizepad = 8;
	sc->sc_obufsize = 7;
	sc->sc_opkthdrlen = 1;

	for (i = 0; i < sc->sc_numser; i++) {
		sc->dev[i] = NULL;
	}

	for (i = 0; i < sc->sc_numser; i++) {
		sc->dev[i] = make_dev(&ubser_cdevsw,
		    USBDEVUNIT(sc->sc_dev) * 8 + i,
		    UID_UUCP, GID_DIALER, 0660,
		    "%s.%d", USBDEVNAME(sc->sc_dev), i);
		if (sc->dev[i] == NULL) {
			printf("%s: make_dev failed\n", USBDEVNAME(sc->sc_dev));
			goto bad;
		}
		sc->dev[i]->si_tty = tp = ttymalloc(NULL);
		if (sc->dev[i]->si_tty == NULL) {
			printf("%s: ttymalloc failed\n", USBDEVNAME(sc->sc_dev));
			goto bad;
		}
		DPRINTF(("ubser_attach: tty_attach tp = %p\n", tp));
		tp->t_oproc = ubserstart;
		tp->t_param = ubserparam;
		tp->t_stop = ubserstop;
	}

	/* find our bulk endpoints */
	epcount = 0;
	usbd_endpoint_count(sc->sc_iface, &epcount);
	sc->sc_bulkin_no = -1;
	sc->sc_bulkout_no = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			USBDEVNAME(sc->sc_dev), i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_bulkout_no = ed->bEndpointAddress;
		}
	}
	if (sc->sc_bulkin_no == -1) {
		printf("%s: could not find bulk in endpoint\n",
		    USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
	if (sc->sc_bulkout_no == -1) {
		printf("%s: could not find bulk out endpoint\n",
		    USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	/* Open the bulk pipes */
	/* Bulk-in pipe */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin_no, 0,
			     &sc->sc_bulkin_pipe);
	if (err) {
		printf("%s: open bulk in error (addr %d): %s\n",
		       USBDEVNAME(sc->sc_dev), sc->sc_bulkin_no,
		       usbd_errstr(err));
		goto fail_0;
	}
	/* Bulk-out pipe */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout_no,
			     USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		printf("%s: open bulk out error (addr %d): %s\n",
		       USBDEVNAME(sc->sc_dev), sc->sc_bulkout_no,
		       usbd_errstr(err));
		goto fail_1;
	}

	/* Allocate a request and an input buffer and start reading. */
	sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_ixfer == NULL) {
		goto fail_2;
	}

	sc->sc_ibuf = usbd_alloc_buffer(sc->sc_ixfer,
					sc->sc_ibufsizepad);
	if (sc->sc_ibuf == NULL) {
		goto fail_3;
	}

	for (i = 0; i < 8; i++) {
		sc->sc_oxfer[i] = NULL;
		sc->sc_obuf[i] = NULL;
	}
	for (i = 0; i < sc->sc_numser; i++) {
		sc->sc_oxfer[i] = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer[i] == NULL) {
			goto fail_4;
		}

		sc->sc_obuf[i] = usbd_alloc_buffer(sc->sc_oxfer[i],
						sc->sc_obufsize +
						sc->sc_opkthdrlen);
		if (sc->sc_obuf[i] == NULL) {
			goto fail_4;
		}
	}

	ubserstartread(sc);

	free(devinfo, M_USBDEV);
	USB_ATTACH_SUCCESS_RETURN;

fail_4:
	for (i = 0; i < sc->sc_numser; i++) {
		if (sc->sc_oxfer[i] != NULL) {
			usbd_free_xfer(sc->sc_oxfer[i]);
			sc->sc_oxfer[i] = NULL;
		}
	}
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

bad:
	ubser_cleanup(sc);
	for (i = 0; i < 8; i++) {
		if (sc->dev[i] != NULL) {
			tp = sc->dev[i]->si_tty;
			if (tp != NULL) {
				if (tp->t_state & TS_ISOPEN) {
					ttyld_close(tp, 0);
					tty_close(tp);
				}
			}
			destroy_dev(sc->dev[i]);
		}
	}

	DPRINTF(("ubser_attach: ATTACH ERROR\n"));
	free(devinfo, M_USBDEV);

	USB_ATTACH_ERROR_RETURN;
}

USB_DETACH(ubser)
{
	USB_DETACH_START(ubser, sc);
	int i, s;
	struct tty *tp;

	DPRINTF(("ubser_detach: sc=%p\n", sc));

	sc->sc_dying = 1;

	if (sc->sc_bulkin_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	for (i = 0; i < 8; i++) {
		if (sc->dev[i] != NULL) {
			tp = sc->dev[i]->si_tty;
			if (tp != NULL) {
				if (tp->t_state & TS_ISOPEN) {
					ttyld_close(tp, 0);
					tty_close(tp);
				}
			}
			destroy_dev(sc->dev[i]);
		}
	}

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	return (0);
}

Static int
ubserparam(struct tty *tp, struct termios *t)
{
	struct ubser_softc *sc;

	USB_GET_SC(ubser, dev2unit(tp->t_dev) / 8, sc);

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("ubserparam: sc = %p\n", sc));

	/*
	 * The firmware on our devices can only do 8n1@9600bps
	 * without handshake.
	 * We refuse to accept other configurations.
	 */

	/* enshure 9600bps */
	switch (t->c_ospeed) {
	case 9600:
		break;
	default:
		return (EINVAL);
	}

	/* 2 stop bits not possible */
	if (ISSET(t->c_cflag, CSTOPB))
		return (EINVAL);

	/* XXX parity handling not possible with current firmware */
	if (ISSET(t->c_cflag, PARENB))
		return (EINVAL);

	/* we can only do 8 data bits */
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS8:
		break;
	default:
		return (EINVAL);
	}

	/* we can't do any kind of hardware handshaking */
	if ((t->c_cflag &
	    (CRTS_IFLOW | CDTR_IFLOW |CDSR_OFLOW |CCAR_OFLOW)) != 0)
		return (EINVAL);

	/*
	 * XXX xon/xoff not supported by the firmware!
	 * This is handled within FreeBSD only and may overflow buffers
	 * because of delayed reaction due to device buffering.
	 */

	ttsetwater(tp);

	return (0);
}

Static void
ubserstart(struct tty *tp)
{
	struct ubser_softc *sc;
	struct cblock *cbp;
	usbd_status err;
	int s;
	u_char *data;
	int cnt;
	uint8_t serial;

	USB_GET_SC(ubser, dev2unit(tp->t_dev) / 8, sc);
	serial = dev2unit(tp->t_dev) & 0x07;
	DPRINTF(("ubserstart: sc = %p, tp = %p\n", sc, tp));

	if (sc->sc_dying)
		return;

	s = spltty();

	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		DPRINTF(("ubserstart: stopped\n"));
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
		DPRINTF(("ubserstart: cnt == 0\n"));
		goto out;
	}

	SET(tp->t_state, TS_BUSY);

	if (cnt + sc->sc_opkthdrlen > sc->sc_obufsize) {
		DPRINTF(("ubserstart: big buffer %d chars\n", cnt));
		cnt = sc->sc_obufsize;
	}
	sc->sc_obuf[serial][0] = serial;
	memcpy(sc->sc_obuf[serial] + sc->sc_opkthdrlen, data, cnt);


	DPRINTF(("ubserstart: %d chars\n", cnt));
	usbd_setup_xfer(sc->sc_oxfer[serial], sc->sc_bulkout_pipe,
			(usbd_private_handle)tp, sc->sc_obuf[serial],
			cnt + sc->sc_opkthdrlen,
			USBD_NO_COPY, USBD_NO_TIMEOUT, ubserwritecb);
	/* What can we do on error? */
	err = usbd_transfer(sc->sc_oxfer[serial]);
	if (err != USBD_IN_PROGRESS)
		printf("ubserstart: err=%s\n", usbd_errstr(err));

	ttwwakeup(tp);

    out:
	splx(s);
}

Static void
ubserstop(struct tty *tp, int flag)
{
	struct ubser_softc *sc;
	int s;

	USB_GET_SC(ubser, dev2unit(tp->t_dev) / 8, sc);

	DPRINTF(("ubserstop: %d\n", flag));

	if (flag & FWRITE) {
		DPRINTF(("ubserstop: write\n"));
		s = spltty();
		if (ISSET(tp->t_state, TS_BUSY)) {
			/* XXX do what? */
			if (!ISSET(tp->t_state, TS_TTSTOP))
				SET(tp->t_state, TS_FLUSH);
		}
		splx(s);
	}

	DPRINTF(("ubserstop: done\n"));
}

Static void
ubserwritecb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct tty *tp;
	struct ubser_softc *sc;
	u_int32_t cc;
	int s;

	tp = (struct tty *)p;
	USB_GET_SC(ubser, dev2unit(tp->t_dev) / 8, sc);

	DPRINTF(("ubserwritecb: status = %d\n", status));

	if (status == USBD_CANCELLED || sc->sc_dying)
		goto error;

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: ubserwritecb: %s\n",
		       USBDEVNAME(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		goto error;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
	DPRINTF(("ubserwritecb: cc = %d\n", cc));
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
ubserstartread(struct ubser_softc *sc)
{
	usbd_status err;

	DPRINTF(("ubserstartread: start\n"));

	if (sc->sc_bulkin_pipe == NULL)
		return (USBD_NORMAL_COMPLETION);

	usbd_setup_xfer(sc->sc_ixfer, sc->sc_bulkin_pipe,
			(usbd_private_handle)sc,
			sc->sc_ibuf, sc->sc_ibufsizepad,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, ubserreadcb);

	err = usbd_transfer(sc->sc_ixfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("ubserstartread: err = %s\n", usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
ubserreadcb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct ubser_softc *sc = (struct ubser_softc *)p;
	struct tty *tp;
	usbd_status err;
	u_int32_t cc;
	u_char *cp;
	int lostcc;
	int s;

	DPRINTF(("ubserreadcb: status = %d\n", status));

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: ubserreadcb: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);

	DPRINTF(("ubserreadcb: got %d bytes from device\n", cc));
	if (cc == 0)
		goto resubmit;

	if (cc > sc->sc_ibufsizepad) {
		printf("%s: invalid receive data size, %d chars\n",
		       USBDEVNAME(sc->sc_dev), cc);
		goto resubmit;
	}

	/* parse header */
	if (cc < 1)
		goto resubmit;
	DPRINTF(("ubserreadcb: got %d chars for serial %d\n", cc - 1, *cp));
	tp = sc->dev[*cp]->si_tty;
	cp++;
	cc--;

	if (cc < 1)
		goto resubmit;

	if (!(tp->t_state & TS_ISOPEN))	/* drop data for unused serials */
		goto resubmit;

	s = spltty();
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		if (tp->t_rawq.c_cc + cc > tp->t_ihiwat
		    && (tp->t_iflag & IXOFF)
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
			ubserstart(tp);
		}
		if (lostcc > 0)
			printf("%s: lost %d chars\n", USBDEVNAME(sc->sc_dev),
			       lostcc);
	} else {
		/* Give characters to tty layer. */
		while (cc > 0) {
			DPRINTFN(7, ("ubserreadcb: char = 0x%02x\n", *cp));
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
	err = ubserstartread(sc);
	if (err) {
		printf("%s: read start failed\n", USBDEVNAME(sc->sc_dev));
		/* XXX what should we do now? */
	}

}

Static void
ubser_cleanup(struct ubser_softc *sc)
{
	int i;

	DPRINTF(("ubser_cleanup: closing pipes\n"));

	if (sc->sc_bulkin_pipe != NULL) {
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
	for (i = 0; i < sc->sc_numser; i++) {
		if (sc->sc_oxfer[i] != NULL) {
			usbd_free_xfer(sc->sc_oxfer[i]);
			sc->sc_oxfer[i] = NULL;
		}
	}
}

static int
ubser_open(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	struct ubser_softc *sc;
	struct tty *tp;
	int s;
	int error;

	USB_GET_SC(ubser, dev2unit(dev) / 8, sc);

	if (sc->sc_dying)
		return (ENXIO);

	tp = sc->dev[dev2unit(dev) & 0x07]->si_tty;

	DPRINTF(("%s: ubser_open: tp = %p\n", USBDEVNAME(sc->sc_dev), tp));

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    suser(p))
		return (EBUSY);

	/*
	 * Do the following if this is a first open.
	 */
	s = spltty();
	while (sc->sc_opening)
		tsleep(&sc->sc_opening, PRIBIO, "ubser_op", 0);
	sc->sc_opening = 1;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		tp->t_dev = dev;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/*
		 * Handle initial DCD.
		 */
		ttyld_modem(tp, 1);
	}

	sc->sc_refcnt++;	/* XXX: wrong refcnt on error later on */
	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);

	error = tty_open(dev, tp);
	if (error)
		goto bad;

	error = ttyld_open(tp, dev);
	if (error)
		goto bad;

	DPRINTF(("%s: ubser_open: success\n", USBDEVNAME(sc->sc_dev)));

	return (0);

	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);
	return (error);

bad:
	DPRINTF(("%s: ubser_open: failed\n", USBDEVNAME(sc->sc_dev)));
	return (error);
}

static int
ubser_close(struct cdev *dev, int flag, int mode, usb_proc_ptr p)
{
	struct ubser_softc *sc;
	struct tty *tp;

	USB_GET_SC(ubser, dev2unit(dev) / 8, sc);
	tp = sc->dev[dev2unit(dev) & 0x07]->si_tty;
	DPRINTF(("%s: ubserclose\n",
	    USBDEVNAME(sc->sc_dev)));

	if (!ISSET(tp->t_state, TS_ISOPEN))
		goto quit;

	if (sc->sc_dying)
		goto quit;

quit:
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (0);
}

static int
ubser_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	usb_device_request_t req;
	struct ubser_softc *sc;
	struct tty *tp;
	int error;
	int s;
	int alen;

	USB_GET_SC(ubser, dev2unit(dev) / 8, sc);
	tp = sc->dev[dev2unit(dev) & 0x07]->si_tty;

	DPRINTF(("ubser_ioctl: cmd = 0x%08lx\n", cmd));

	if (sc->sc_dying)
		return (EIO);

	error = ttyioctl(dev, cmd, data, flag, p);
        if (error != ENOTTY) {
		DPRINTF(("ubser_ioctl: l_ioctl: error = %d\n", error));
		return (error);
	}

	error = 0;

	s = spltty();
	switch (cmd) {
	case TIOCSBRK:	/* clearing break condition is done in firmware */
		DPRINTF(("ubser_ioctl: TIOCSBRK\n"));
		req.bmRequestType = UT_READ_VENDOR_INTERFACE;
		req.bRequest = VENDOR_SET_BREAK;
		USETW(req.wValue, dev2unit(dev) & 0x07);
		USETW(req.wIndex, sc->sc_ifaceno);
		USETW(req.wLength, 0);
		error = usbd_do_request_flags(sc->sc_udev, &req, &sc->sc_numser,
		    USBD_SHORT_XFER_OK, &alen, USBD_DEFAULT_TIMEOUT);
		break;
	/* XXX: something else to handle? */
	}
	splx(s);

	return (error);
}

static int
ubser_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct ubser_softc *sc;
	struct tty *tp;
	int error;

	USB_GET_SC(ubser, dev2unit(dev) / 8, sc);
	tp = sc->dev[dev2unit(dev) & 0x07]->si_tty;

	DPRINTF(("ubser_read: tp = %p, flag = 0x%x\n", tp, flag));

	if (sc->sc_dying)
		return (EIO);

	error = ttyld_read(tp, uio, flag);

	DPRINTF(("ubser_read: error = %d\n", error));

	return (error);
}

static int
ubser_write(struct cdev *dev, struct uio *uio, int flag)
{
	struct ubser_softc *sc;
	struct tty *tp;
	int error;

	USB_GET_SC(ubser, dev2unit(dev) / 8, sc);
	tp = sc->dev[dev2unit(dev) & 0x07]->si_tty;

	DPRINTF(("ubser_write: tp = %p, flag = 0x%x\n", tp, flag));

	if (sc->sc_dying)
		return (EIO);

	error = ttyld_write(tp, uio, flag);

	DPRINTF(("ubser_write: error = %d\n", error));

	return (error);
}

DRIVER_MODULE(ubser, uhub, ubser_driver, ubser_devclass, usbd_driver_load, 0);

