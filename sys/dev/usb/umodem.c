/*	$NetBSD: umodem.c,v 1.5 1999/01/08 11:58:25 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

/*
 * Comm Class spec: http://www.usb.org/developers/data/devclass/usbcdc10.pdf
 *		    http://www.usb.org/developers/data/devclass/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#endif
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
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/usbdevs.h>

#ifdef UMODEM_DEBUG
#define DPRINTF(x) if(umodemdebug) logprintf x
#define DPRINTFN(n, x) if(umodemdebug > (n)) logprintf x
int	umodemdebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~((unsigned)(f))
#define	ISSET(t, f)	((t) & (f))

#define	UMODEMUNIT_MASK		0x3ffff
#define	UMODEMDIALOUT_MASK	0x80000
#define	UMODEMCALLUNIT_MASK	0x40000

#define	UMODEMUNIT(x)		(minor(x) & UMODEMUNIT_MASK)
#define	UMODEMDIALOUT(x)	(minor(x) & UMODEMDIALOUT_MASK)
#define	UMODEMCALLUNIT(x)	(minor(x) & UMODEMCALLUNIT_MASK)

#define UMODEMIBUFSIZE 64

struct umodem_softc {
	USBBASEDEVICE		sc_dev;		/* base device */

	usbd_device_handle	sc_udev;	/* USB device */

	int			sc_ctl_iface_no;
	usbd_interface_handle	sc_ctl_iface;	/* control interface */
	int			sc_data_iface_no;
	usbd_interface_handle	sc_data_iface;	/* data interface */

	int			sc_bulkin_no;	/* bulk in endpoint address */
	usbd_pipe_handle	sc_bulkin_pipe;	/* bulk in pipe */
	usbd_xfer_handle	sc_ixfer;	/* read request */
	u_char			*sc_ibuf;	/* read buffer */

	int			sc_bulkout_no;	/* bulk out endpoint address */
	usbd_pipe_handle	sc_bulkout_pipe;/* bulk out pipe */
	usbd_xfer_handle	sc_oxfer;	/* read request */

	int			sc_cm_cap;	/* CM capabilities */
	int			sc_acm_cap;	/* ACM capabilities */

	int			sc_cm_over_data;

	struct tty		*sc_tty;	/* our tty */

	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */

	u_char			sc_opening;	/* lock during open */
	u_char			sc_dying;	/* disconnecting */

#if defined(__FreeBSD__)
	dev_t			dev;		/* special device node */
#endif
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(umodem);

#elif defined(__FreeBSD__)
d_open_t  umodemopen;
d_close_t umodemclose;
d_read_t  umodemread;
d_write_t umodemwrite;
d_ioctl_t umodemioctl;

#define UMODEM_CDEV_MAJOR  124

static struct cdevsw umodem_cdevsw = {
	/* open */      umodemopen,
	/* close */     umodemclose,
	/* read */      umodemread,
	/* write */     umodemwrite,
	/* ioctl */     umodemioctl,
	/* poll */      ttypoll,
	/* mmap */      nommap,
	/* strategy */  nostrategy,
	/* name */      "umodem",
	/* maj */       UMODEM_CDEV_MAJOR,
	/* dump */      nodump,
	/* psize */     nopsize,
	/* flags */     D_TTY | D_KQFILTER,
#if __FreeBSD_version < 500014
	/* bmaj */	-1,
#endif
	/* kqfilter */	ttykqfilter,
};
#endif

void *umodem_get_desc
	(usbd_device_handle dev, int type, int subtype);
usbd_status umodem_set_comm_feature
	(struct umodem_softc *sc, int feature, int state);
usbd_status umodem_set_line_coding
	(struct umodem_softc *sc, usb_cdc_line_state_t *state);

void	umodem_get_caps	(usbd_device_handle, int *, int *);
void	umodem_cleanup	(struct umodem_softc *);
int	umodemparam	(struct tty *, struct termios *);
void	umodemstart	(struct tty *);
void	umodemstop	(struct tty *, int);
struct tty * umodemtty	(dev_t dev);
void	umodem_shutdown	(struct umodem_softc *);
void	umodem_modem	(struct umodem_softc *, int);
void	umodem_break	(struct umodem_softc *, int);
usbd_status umodemstartread (struct umodem_softc *);
void	umodemreadcb	(usbd_xfer_handle, usbd_private_handle, 
			     usbd_status status);
void	umodemwritecb	(usbd_xfer_handle, usbd_private_handle, 
			     usbd_status status);

USB_DECLARE_DRIVER(umodem);

USB_MATCH(umodem)
{
	USB_MATCH_START(umodem, uaa);
	usb_interface_descriptor_t *id;
	int cm, acm;
	
	if (!uaa->iface)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == 0 ||
	    id->bInterfaceClass != UICLASS_CDC ||
	    id->bInterfaceSubClass != UISUBCLASS_ABSTRACT_CONTROL_MODEL ||
	    id->bInterfaceProtocol != UIPROTO_CDC_AT)
		return (UMATCH_NONE);
	
	umodem_get_caps(uaa->device, &cm, &acm);
	if (!(cm & USB_CDC_CM_DOES_CM) ||
	    !(cm & USB_CDC_CM_OVER_DATA) ||
	    !(acm & USB_CDC_ACM_HAS_LINE))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

USB_ATTACH(umodem)
{
	USB_ATTACH_START(umodem, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usb_cdc_cm_descriptor_t *cmd;
	char devinfo[1024];
	usbd_status err;
	int data_ifaceno;
	int i;
	struct tty *tp;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	sc->sc_udev = dev;

	sc->sc_ctl_iface = uaa->iface;
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);
	sc->sc_ctl_iface_no = id->bInterfaceNumber;

	umodem_get_caps(dev, &sc->sc_cm_cap, &sc->sc_acm_cap);

	/* Get the data interface no. */
	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (!cmd) {
		DPRINTF(("%s: no CM desc\n", USBDEVNAME(sc->sc_dev)));
		goto bad;
	}
	sc->sc_data_iface_no = data_ifaceno = cmd->bDataInterface;

	printf("%s: data interface %d, has %sCM over data, has %sbreak\n",
	       USBDEVNAME(sc->sc_dev), data_ifaceno,
	       sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	       sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");


	/* Get the data interface too. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i]) {
			id = usbd_get_interface_descriptor(uaa->ifaces[i]);
			if (id->bInterfaceNumber == data_ifaceno) {
				sc->sc_data_iface = uaa->ifaces[i];
				uaa->ifaces[i] = 0;
			}
		}
	}
	if (!sc->sc_data_iface) {
		printf("%s: no data interface\n", USBDEVNAME(sc->sc_dev));
		goto bad;
	}

	/* 
	 * Find the bulk endpoints. 
	 * Iterate over all endpoints in the data interface and take note.
	 */
	sc->sc_bulkin_no = sc->sc_bulkout_no = -1;

	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface, i);
		if (!ed) {
			printf("%s: no endpoint descriptor for %d\n",
				USBDEVNAME(sc->sc_dev), i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        sc->sc_bulkin_no = ed->bEndpointAddress;
                } else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        sc->sc_bulkout_no = ed->bEndpointAddress;
                }
        }

	if (sc->sc_bulkin_no == -1) {
		DPRINTF(("%s: Could not find data bulk in\n",
			USBDEVNAME(sc->sc_dev)));
		goto bad;
	}
	if (sc->sc_bulkout_no == -1) {
		DPRINTF(("%s: Could not find data bulk out\n",
			USBDEVNAME(sc->sc_dev)));
		goto bad;
	}

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_ASSUME_CM_OVER_DATA) {
		sc->sc_cm_over_data = 1;
	} else {
	    if (sc->sc_cm_cap & USB_CDC_CM_OVER_DATA) {
		    err = umodem_set_comm_feature(sc, UCDC_ABSTRACT_STATE,
						UCDC_DATA_MULTIPLEXED);
		    if (err)
				goto bad;
		    sc->sc_cm_over_data = 1;
		}
	}

#if defined(__NetBSD__) || defined(__OpenBSD__)
	sc->sc_tty = tp = ttymalloc();
#elif defined(__FreeBSD__)
	sc->sc_tty = tp = ttymalloc(sc->sc_tty);
#endif
	tp->t_oproc = umodemstart;
	tp->t_param = umodemparam;
	tp->t_stop = umodemstop;
	DPRINTF(("umodem_attach: tty_attach %p\n", tp));
#if defined(__NetBSD__) || defined(__OpenBSD__)
	tty_attach(tp);
#endif

#if defined(__FreeBSD__)
	DPRINTF(("umodem_attach: make_dev: umodem%d\n", device_get_unit(self)));
	sc->dev = make_dev(&umodem_cdevsw, device_get_unit(self),
			UID_UUCP, GID_DIALER, 0660,
			"umodem%d", device_get_unit(self));
	sc->dev->si_tty = tp;
#endif

	sc->sc_dtr = -1;

	USB_ATTACH_SUCCESS_RETURN;

 bad:
	DPRINTF(("umodem_attach: BAD -> DYING\n"));
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}

void
umodem_get_caps(usbd_device_handle dev, int *cm, int *acm)
{
	usb_cdc_cm_descriptor_t *cmd;
	usb_cdc_acm_descriptor_t *cad;

	*cm = *acm = 0;

	cmd = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	if (!cmd) {
		DPRINTF(("umodem_get_desc: no CM desc\n"));
		return;
	}
	*cm = cmd->bmCapabilities;

	cad = umodem_get_desc(dev, UDESC_CS_INTERFACE, UDESCSUB_CDC_ACM);
	if (!cad) {
		DPRINTF(("umodem_get_desc: no ACM desc\n"));
		return;
	}
	*acm = cad->bmCapabilities;
} 

void
umodemstart(struct tty *tp)
{
	struct umodem_softc *sc;
	struct cblock *cbp;
	int s;
	u_char *data;
	int cnt;

	USB_GET_SC(umodem, UMODEMUNIT(tp->t_dev), sc);

	if (sc->sc_dying)
		return;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		DPRINTFN(4,("umodemstart: stopped\n"));
		goto out;
	}

#if defined(__NetBSD__) || defined(__OpenBSD__)
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}
#elif defined(__FreeBSD__)
	if (tp->t_outq.c_cc <= tp->t_olowat) {
		if (ISSET(tp->t_state, TS_SO_OLOWAT)) {
			CLR(tp->t_state, TS_SO_OLOWAT);
			wakeup(TSA_OLOWAT(tp));
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0) {
		        if (ISSET(tp->t_state, TS_BUSY | TS_SO_OCOMPLETE) ==
		            TS_SO_OCOMPLETE && tp->t_outq.c_cc == 0) {
		                CLR(tp->t_state, TS_SO_OCOMPLETE);
		                wakeup(TSA_OCOMPLETE(tp));
		        } 
			goto out;
		}
	}
#endif

	/* Grab the first contiguous region of buffer space. */
	data = tp->t_outq.c_cf;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	cnt = ndqb(&tp->t_outq, 0);
#elif defined(__FreeBSD__)
	cbp = (struct cblock *) ((intptr_t) tp->t_outq.c_cf & ~CROUND);
	cnt = min((char *) (cbp+1) - tp->t_outq.c_cf, tp->t_outq.c_cc);
#endif

	if (cnt == 0) {
		DPRINTF(("umodemstart: cnt==0\n"));
		splx(s);
		return;
	}

	SET(tp->t_state, TS_BUSY);

	DPRINTFN(4,("umodemstart: %d chars\n", cnt));
	/* XXX what can we do on error? */
	usbd_setup_xfer(sc->sc_oxfer, sc->sc_bulkout_pipe, 
			   (usbd_private_handle)sc, data, cnt,
			   0, USBD_NO_TIMEOUT, umodemwritecb);
	(void)usbd_transfer(sc->sc_oxfer);

	ttwwakeup(tp);
out:
	splx(s);
}

void
umodemwritecb(usbd_xfer_handle xfer, usbd_private_handle priv,
	      usbd_status status)
{
	struct umodem_softc *sc = (struct umodem_softc *)priv;
	struct tty *tp = sc->sc_tty;
	u_int32_t cc;
	int s;

	DPRINTFN(5,("umodemwritecb: status=%d\n", status));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodemwritecb: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, 0, 0, &cc, 0);
	DPRINTFN(5,("umodemwritecb: cc=%d\n", cc));

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, cc);
	(*linesw[tp->t_line].l_start)(tp);
	splx(s);
}

int
umodemparam(struct tty *tp, struct termios *t)
{
	struct umodem_softc *sc;
	usb_cdc_line_state_t ls;

	USB_GET_SC(umodem, UMODEMUNIT(tp->t_dev), sc);

	if (sc->sc_dying)
		return (EIO);

	/* Check requested parameters. */
	if (t->c_ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

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

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}
	/* XXX what can we if it fails? */
	(void)umodem_set_line_coding(sc, &ls);

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	(void) (*linesw[tp->t_line].l_modem)(tp, 1 /* XXX carrier */ );

	return (0);
}

int
umodemopen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = UMODEMUNIT(dev);
	struct umodem_softc *sc;
	usbd_status err;
	struct tty *tp;
	int s;
	int error;
 
	USB_GET_SC_OPEN(umodem, unit, sc);

	if (sc->sc_dying)
		return (EIO);

#if defined(__NetBSD__) || defined(__OpenBBSD__)
	if (ISSET(sc->sc_dev.dv_flags, DVF_ACTIVE) == 0)
		return (ENXIO);
#endif

	tp = sc->sc_tty;

	DPRINTF(("%s: umodemopen: tp=%p\n", USBDEVNAME(sc->sc_dev), tp));

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    suser(p))
		return (EBUSY);

	/*
	 * Do the following iff this is a first open.
	 */
	s = spltty();
	while (sc->sc_opening)
		tsleep(&sc->sc_opening, PRIBIO, "umdmop", 0);
	sc->sc_opening = 1;
	
#if defined(__NetBSD__) || defined(__OpenBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#elif defined(__FreeBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		struct termios t;

		tp->t_dev = dev;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		/* Make sure umodemparam() will do something. */
		tp->t_ospeed = 0;
		(void) umodemparam(tp, &t);
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
		umodem_modem(sc, 1);

		DPRINTF(("umodemopen: open pipes\n"));

		/* Open the bulk pipes */
		err = usbd_open_pipe(sc->sc_data_iface, sc->sc_bulkin_no, 0,
				   &sc->sc_bulkin_pipe);
		if (err) {
			DPRINTF(("%s: cannot open bulk out pipe (addr %d)\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkin_no));
			return (EIO);
		}
		err = usbd_open_pipe(sc->sc_data_iface, sc->sc_bulkout_no,
				   USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
		if (err) {
			DPRINTF(("%s: cannot open bulk in pipe (addr %d)\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkout_no));
			usbd_close_pipe(sc->sc_bulkin_pipe);
			return (EIO);
		}
		
		/* Allocate a request and an input buffer and start reading. */
		sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_ixfer == 0) {
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}
		sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer == 0) {
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			usbd_free_xfer(sc->sc_ixfer);
			return (ENOMEM);
		}
		sc->sc_ibuf = malloc(UMODEMIBUFSIZE, M_USBDEV, M_WAITOK);
		umodemstartread(sc);
	}
	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	error = ttyopen(tp, UMODEMDIALOUT(dev), ISSET(flag, O_NONBLOCK));
	if (error)
		goto bad;
#elif defined(__FreeBSD__)
	error = ttyopen(dev, tp);
	if (error)
		goto bad;
#endif

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error)
		goto bad;

	return (0);

bad:
#if defined(__NetBSD__) || defined(__OpenBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#elif defined(__FreeBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		umodem_cleanup(sc);
	}

	return (error);
}

usbd_status
umodemstartread(struct umodem_softc *sc)
{
	usbd_status err;

	DPRINTFN(5,("umodemstartread: start\n"));
	usbd_setup_xfer(sc->sc_ixfer, sc->sc_bulkin_pipe, 
			   (usbd_private_handle)sc, 
			   sc->sc_ibuf,  UMODEMIBUFSIZE, USBD_SHORT_XFER_OK, 
			   USBD_NO_TIMEOUT, umodemreadcb);

	err = usbd_transfer(sc->sc_ixfer);
	if (err != USBD_IN_PROGRESS)
		return (err);

	return (USBD_NORMAL_COMPLETION);
}
 
void
umodemreadcb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct umodem_softc *sc = (struct umodem_softc *)p;
	struct tty *tp = sc->sc_tty;
	int (*rint) (int c, struct tty *tp) = linesw[tp->t_line].l_rint;
	usbd_status err;
	u_int32_t cc;
	u_char *cp;
	int s;

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("umodemreadcb: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, 0, (void **)&cp, &cc, 0);
	DPRINTFN(5,("umodemreadcb: got %d chars, tp=%p\n", cc, tp));
	s = spltty();
	/* Give characters to tty layer. */
	while (cc-- > 0) {
		DPRINTFN(7,("umodemreadcb: char=0x%02x\n", *cp));
		if ((*rint)(*cp++, tp) == -1) {
			/* XXX what should we do? */
			break;
		}
	}
	splx(s);

	err = umodemstartread(sc);
	if (err) {
		printf("%s: read start failed\n", USBDEVNAME(sc->sc_dev));
		/* XXX what should we dow now? */
	}
}

int
umodemclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	struct umodem_softc *sc;
	struct tty *tp;
        int s;

	USB_GET_SC(umodem, UMODEMUNIT(dev), sc);

	tp = sc->sc_tty;

	DPRINTF(("%s: umodemclose sc_tty=%p\n", USBDEVNAME(sc->sc_dev), tp));

	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	DPRINTF(("%s: umodemclose lclose(%p,%d)\n", USBDEVNAME(sc->sc_dev), tp,flag));

        s=spltty();
	DPRINTF(("%s: umodemclose lclose=%p\n", USBDEVNAME(sc->sc_dev), linesw[tp->t_line].l_close));
	(*linesw[tp->t_line].l_close)(tp, flag);

	DPRINTF(("%s: umodemclose ttyclose(%p)\n", USBDEVNAME(sc->sc_dev), tp));
	ttyclose(tp);
        splx(s);

	DPRINTF(("%s: umodemclose sc->sc_dying=%d\n", USBDEVNAME(sc->sc_dev), sc->sc_dying));
	if (sc->sc_dying)
		return (0);

	DPRINTF(("%s: umodemclose tp->t_state=%d\n", USBDEVNAME(sc->sc_dev), tp->t_state));
#if defined(__NetBSD__) || defined(__OpenBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#elif defined(__FreeBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		DPRINTF(("%s: umodemclose umodem_cleanup(%p)\n", USBDEVNAME(sc->sc_dev), sc));
		umodem_cleanup(sc);
	}
	DPRINTF(("%s: umodemclose return\n", USBDEVNAME(sc->sc_dev)));

	return (0);
}
 
void
umodem_cleanup(struct umodem_softc *sc)
{
	umodem_shutdown(sc);

	DPRINTFN(5, ("%s: umodem_cleanup: closing pipes\n",
		USBDEVNAME(sc->sc_dev)));

	usbd_abort_pipe(sc->sc_bulkin_pipe);
	usbd_close_pipe(sc->sc_bulkin_pipe);
	usbd_abort_pipe(sc->sc_bulkout_pipe);
	usbd_close_pipe(sc->sc_bulkout_pipe);
	usbd_free_xfer(sc->sc_ixfer);
	usbd_free_xfer(sc->sc_oxfer);
	free(sc->sc_ibuf, M_USBDEV);
}

int
umodemread(dev_t dev, uio *uio, int flag)
{
	struct umodem_softc *sc;
	struct tty *tp;

	USB_GET_SC(umodem, UMODEMUNIT(dev), sc);

	tp = sc->sc_tty;
	
	if (sc->sc_dying)
		return (EIO);
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
umodemwrite(dev_t dev, struct uio *uio, int flag)
{
	struct umodem_softc *sc;
	struct tty *tp;

	USB_GET_SC(umodem, UMODEMUNIT(dev), sc);

 	tp = sc->sc_tty;

	if (sc->sc_dying)
		return (EIO);
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void
umodemstop(struct tty *tp, int flag)
{
	struct umodem_softc *sc;
	int s;

	USB_GET_SC(umodem, UMODEMUNIT(tp->t_dev), sc);

	DPRINTF(("umodemstop: %d\n", flag));
	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		DPRINTF(("umodemstop: XXX\n"));
		/* XXX do what? */
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
}

struct tty *
umodemtty(dev_t dev)
{
	struct umodem_softc *sc;
	struct tty *tp;

	USB_GET_SC(umodem, UMODEMUNIT(dev), sc);

	tp = sc->sc_tty;

	return (tp);
}

int
umodemioctl(dev_t dev, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	struct umodem_softc *sc;
	struct tty *tp;
	int error;
	int s;
	int bits;

	USB_GET_SC(umodem, UMODEMUNIT(dev), sc);

	tp = sc->sc_tty;

	if (sc->sc_dying)
		return (EIO);
 
	DPRINTF(("umodemioctl: cmd=0x%08lx\n", cmd));

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	error = ttioctl(tp, cmd, data, flag, p);
#elif defined(__FreeBSD__)
	error = ttioctl(tp, cmd, data, flag);
#endif
	if (error >= 0)
		return (error);

	DPRINTF(("umodemioctl: our cmd=0x%08lx\n", cmd));
	s = spltty();

	switch (cmd) {
	case TIOCSBRK:
		umodem_break(sc, 1);
		break;

	case TIOCCBRK:
		umodem_break(sc, 0);
		break;

	case TIOCSDTR:
		umodem_modem(sc, 1);
		break;

	case TIOCCDTR:
		umodem_modem(sc, 0);
		break;

	case TIOCMGET:
		bits = TIOCM_LE;
		if(sc->sc_dtr)
			bits |= TIOCM_DTR;
		*(int *)data = bits;
		break;

	case TIOCMSET:
		break;

	case USB_GET_CM_OVER_DATA:
		*(int *)data = sc->sc_cm_over_data;
		break;

	case USB_SET_CM_OVER_DATA:
		if (*(int *)data != sc->sc_cm_over_data) {
			/* XXX change it */
		}
		break;

	default:
		DPRINTF(("umodemioctl: unknown\n"));
		error = ENOTTY;
		splx(s);
		return (error);
	}

	splx(s);
	return(0);
}

void
umodem_shutdown(struct umodem_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("%s: umodem_shutdown\n", USBDEVNAME(sc->sc_dev)));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		umodem_modem(sc, 0);
#if defined(__NetBSD__) || defined(__OpenBSD__)
		(void) tsleep(sc, TTIPRI, ttclos, hz);
#elif defined(__FreeBSD__)
		(void) tsleep(sc, TTIPRI, "umdmsd", hz);
#endif
	}
}

void
umodem_modem(struct umodem_softc *sc, int onoff)
{
	usb_device_request_t req;

	DPRINTF(("%s: umodem_modem: onoff=%d\n", USBDEVNAME(sc->sc_dev),onoff));

	if (sc->sc_dtr == onoff)
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, onoff ? UCDC_LINE_DTR : 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);

	sc->sc_dtr = onoff;
}

void
umodem_break(struct umodem_softc *sc, int onoff)
{
	usb_device_request_t req;

	DPRINTF(("%s: umodem_break: onoff=%d\n", USBDEVNAME(sc->sc_dev),onoff));

	if (!(sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);
}

void *
umodem_get_desc(usbd_device_handle dev, int type, int subtype)
{
	usb_descriptor_t *desc;
	usb_config_descriptor_t *cd = usbd_get_config_descriptor(dev);
        uByte *p = (uByte *)cd;
        uByte *end = p + UGETW(cd->wTotalLength);

	while (p < end) {
		desc = (usb_descriptor_t *)p;
		if (desc->bDescriptorType == type &&
		    desc->bDescriptorSubtype == subtype)
			return (desc);
		p += desc->bLength;
	}

	return (0);
}

usbd_status
umodem_set_comm_feature(struct umodem_softc *sc, int feature, int state)
{
	usb_device_request_t req;
	usbd_status err;
	usb_cdc_abstract_state_t ast;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_COMM_FEATURE;
	USETW(req.wValue, feature);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_ABSTRACT_STATE_LENGTH);
	USETW(ast.wState, state);

	err = usbd_do_request(sc->sc_udev, &req, &ast);
	if (err) {
		DPRINTF(("%s: umodem_set_comm_feat: feature=%d failed, err=%d\n",
			 USBDEVNAME(sc->sc_dev), feature, err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
umodem_set_line_coding(struct umodem_softc *sc, usb_cdc_line_state_t state)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("%s: umodem_set_line_cod: rate=%d fmt=%d parity=%d bits=%d\n",
		 USBDEVNAME(sc->sc_dev), UGETDW(state->dwDTERate),
		 state->bCharFormat, state->bParityType, state->bDataBits));

	if (bcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("%s: umodem_set_line_coding: already set\n",
			USBDEVNAME(sc->sc_dev)));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(sc->sc_udev, &req, state);
	if (err) {
		DPRINTF(("%s: umodem_set_line_coding: failed, err=%d\n",
			USBDEVNAME(sc->sc_dev), err));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
umodem_activate(device_ptr_t self, enum devact act)
{
	struct umodem_softc *sc = (struct umodem_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
#endif

USB_DETACH(umodem)
{
	USB_DETACH_START(umodem, sc);
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int maj, mn;

	DPRINTF(("umodem_detach: sc=%p flags=%d tp=%p\n", 
		 sc, flags, sc->sc_tty));
#elif defined(__FreeBSD__)
	DPRINTF(("umodem_detach: sc=%p flags=%d, tp=%p\n", 
		 sc, 0,  sc->sc_tty));
#endif

	sc->sc_dying = 1;

#ifdef DIAGNOSTIC
	if (sc->sc_tty == 0) {
		DPRINTF(("umodem_detach: no tty\n"));
		return (0);
	}
#endif

	/* use refernce count? XXX */

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == umodemopen)
			break;

	/* Nuke the vnodes for any open instances. */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);
	vdevgone(maj, mn, mn | UMODEMDIALOUT_MASK, VCHR);
	vdevgone(maj, mn, mn | UMODEMCALLUNIT_MASK, VCHR);
#elif defined(__FreeBSD__)
	/* XXX not yet implemented */
#endif

#if defined(__FreeBSD__)
	destroy_dev(sc->dev);
#endif

	/* Detach and free the tty. */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	tty_detach(sc->sc_tty);
	ttyfree(sc->sc_tty);
	sc->sc_tty = 0;
#endif

	return (0);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(umodem, uhub, umodem_driver, umodem_devclass, usbd_driver_load,0);
#endif
