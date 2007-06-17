/*	$NetBSD: uhid.c,v 1.46 2001/11/13 06:24:55 lukem Exp $	*/

/* Also already merged from NetBSD:
 *	$NetBSD: uhid.c,v 1.54 2002/09/23 05:51:21 simonb Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
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
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include "usbdevs.h"
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>

/* Replacement report descriptors for devices shipped with broken ones */
#include <dev/usb/ugraphire_rdesc.h>
#include <dev/usb/uxb360gp_rdesc.h>

/* For hid blacklist quirk */
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uhiddebug) logprintf x
#define DPRINTFN(n,x)	if (uhiddebug>(n)) logprintf x
int	uhiddebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uhid, CTLFLAG_RW, 0, "USB uhid");
SYSCTL_INT(_hw_usb_uhid, OID_AUTO, debug, CTLFLAG_RW,
	   &uhiddebug, 0, "uhid debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhid_softc {
	device_t sc_dev;			/* base device */
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	int sc_isize;
	int sc_osize;
	int sc_fsize;
	u_int8_t sc_iid;
	u_int8_t sc_oid;
	u_int8_t sc_fid;

	u_char *sc_ibuf;
	u_char *sc_obuf;

	void *sc_repdesc;
	int sc_repdesc_size;

	struct clist sc_q;
	struct selinfo sc_rsel;
	struct proc *sc_async;	/* process that wants SIGIO */
	u_char sc_state;	/* driver state */
#define	UHID_OPEN	0x01	/* device is open */
#define	UHID_ASLP	0x02	/* waiting for device data */
#define UHID_NEEDCLEAR	0x04	/* needs clearing endpoint stall */
#define UHID_IMMED	0x08	/* return read data immediately */

	int sc_refcnt;
	u_char sc_dying;

	struct cdev *dev;
};

#define	UHIDUNIT(dev)	(minor(dev))
#define	UHID_CHUNK	128	/* chunk size for read */
#define	UHID_BSIZE	1020	/* buffer size */

d_open_t	uhidopen;
d_close_t	uhidclose;
d_read_t	uhidread;
d_write_t	uhidwrite;
d_ioctl_t	uhidioctl;
d_poll_t	uhidpoll;


static struct cdevsw uhid_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	uhidopen,
	.d_close =	uhidclose,
	.d_read =	uhidread,
	.d_write =	uhidwrite,
	.d_ioctl =	uhidioctl,
	.d_poll =	uhidpoll,
	.d_name =	"uhid",
};

static void uhid_intr(usbd_xfer_handle, usbd_private_handle,
			   usbd_status);

static int uhid_do_read(struct uhid_softc *, struct uio *uio, int);
static int uhid_do_write(struct uhid_softc *, struct uio *uio, int);
static int uhid_do_ioctl(struct uhid_softc *, u_long, caddr_t, int,
			      struct thread *);

USB_DECLARE_DRIVER(uhid);

static int
uhid_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return (UMATCH_NONE);
	if  (id->bInterfaceClass != UICLASS_HID) {
		/* The Xbox 360 gamepad doesn't use the HID class. */
		if (id->bInterfaceClass != UICLASS_VENDOR ||
		    id->bInterfaceSubClass != UISUBCLASS_XBOX360_CONTROLLER ||
		    id->bInterfaceProtocol != UIPROTO_XBOX360_GAMEPAD)
			return (UMATCH_NONE);
	}
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_HID_IGNORE)
		return (UMATCH_NONE);
#if 0
	if (uaa->matchlvl)
		return (uaa->matchlvl);
#endif

	return (UMATCH_IFACECLASS_GENERIC);
}

static int
uhid_attach(device_t self)
{
	USB_ATTACH_START(uhid, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int size;
	void *desc;
	const void *descptr;
	usbd_status err;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: could not read endpoint descriptor\n",
		       device_get_nameunit(sc->sc_dev));
		sc->sc_dying = 1;
		return ENXIO;
	}

	DPRINTFN(10,("uhid_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		     " bInterval=%d\n",
		     ed->bLength, ed->bDescriptorType,
		     ed->bEndpointAddress & UE_ADDR,
		     UE_GET_DIR(ed->bEndpointAddress)==UE_DIR_IN? "in" : "out",
		     ed->bmAttributes & UE_XFERTYPE,
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n", device_get_nameunit(sc->sc_dev));
		sc->sc_dying = 1;
		return ENXIO;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;

	descptr = NULL;
	if (uaa->vendor == USB_VENDOR_WACOM) {
		/* The report descriptor for the Wacom Graphire is broken. */
		if (uaa->product == USB_PRODUCT_WACOM_GRAPHIRE) {
			size = sizeof uhid_graphire_report_descr;
			descptr = uhid_graphire_report_descr;
		} else if (uaa->product == USB_PRODUCT_WACOM_GRAPHIRE3_4X5) {
			static uByte reportbuf[] = {2, 2, 2};

			/*
			 * The Graphire3 needs 0x0202 to be written to
			 * feature report ID 2 before it'll start
			 * returning digitizer data.
			 */
			usbd_set_report(uaa->iface, UHID_FEATURE_REPORT, 2,
			    &reportbuf, sizeof reportbuf);

			size = sizeof uhid_graphire3_4x5_report_descr;
			descptr = uhid_graphire3_4x5_report_descr;
		}
	} else if (id->bInterfaceClass == UICLASS_VENDOR &&
	    id->bInterfaceSubClass == UISUBCLASS_XBOX360_CONTROLLER &&
	    id->bInterfaceProtocol == UIPROTO_XBOX360_GAMEPAD) {
		static uByte reportbuf[] = {1, 3, 0};

		/* The LEDs on the gamepad are blinking by default, turn off. */
		usbd_set_report(uaa->iface, UHID_OUTPUT_REPORT, 0,
		    &reportbuf, sizeof reportbuf);

		/* The Xbox 360 gamepad has no report descriptor. */
		size = sizeof uhid_xb360gp_report_descr;
		descptr = uhid_xb360gp_report_descr;
	}

	if (descptr) {
		desc = malloc(size, M_USBDEV, M_NOWAIT);
		if (desc == NULL)
			err = USBD_NOMEM;
		else {
			err = USBD_NORMAL_COMPLETION;
			memcpy(desc, descptr, size);
		}
	} else {
		desc = NULL;
		err = usbd_read_report_desc(uaa->iface, &desc, &size,M_USBDEV);
	}

	if (err) {
		printf("%s: no report descriptor\n", device_get_nameunit(sc->sc_dev));
		sc->sc_dying = 1;
		return ENXIO;
	}

	(void)usbd_set_idle(iface, 0, 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input,   &sc->sc_iid);
	sc->sc_osize = hid_report_size(desc, size, hid_output,  &sc->sc_oid);
	sc->sc_fsize = hid_report_size(desc, size, hid_feature, &sc->sc_fid);

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;
	sc->dev = make_dev(&uhid_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "uhid%d", device_get_unit(self));
	return 0;
}

static int
uhid_detach(device_t self)
{
	USB_DETACH_START(uhid, sc);
	int s;

	DPRINTF(("uhid_detach: sc=%p\n", sc));
	sc->sc_dying = 1;
	if (sc->sc_intrpipe != NULL)
		usbd_abort_pipe(sc->sc_intrpipe);

	if (sc->sc_state & UHID_OPEN) {
		s = splusb();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone */
			wakeup(&sc->sc_q);
			/* Wait for processes to go away. */
			usb_detach_wait(sc->sc_dev);
		}
		splx(s);
	}
	destroy_dev(sc->dev);

	if (sc->sc_repdesc)
		free(sc->sc_repdesc, M_USBDEV);

	return (0);
}

void
uhid_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uhid_softc *sc = addr;

#ifdef USB_DEBUG
	if (uhiddebug > 5) {
		u_int32_t cc, i;

		usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
		DPRINTF(("uhid_intr: status=%d cc=%d\n", status, cc));
		DPRINTF(("uhid_intr: data ="));
		for (i = 0; i < cc; i++)
			DPRINTF((" %02x", sc->sc_ibuf[i]));
		DPRINTF(("\n"));
	}
#endif

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uhid_intr: status=%d\n", status));
		if (status == USBD_STALLED)
		    sc->sc_state |= UHID_NEEDCLEAR;
		return;
	}

	(void) b_to_q(sc->sc_ibuf, sc->sc_isize, &sc->sc_q);

	if (sc->sc_state & UHID_ASLP) {
		sc->sc_state &= ~UHID_ASLP;
		DPRINTFN(5, ("uhid_intr: waking %p\n", &sc->sc_q));
		wakeup(&sc->sc_q);
	}
	selwakeuppri(&sc->sc_rsel, PZERO);
	if (sc->sc_async != NULL) {
		DPRINTFN(3, ("uhid_intr: sending SIGIO %p\n", sc->sc_async));
		PROC_LOCK(sc->sc_async);
		psignal(sc->sc_async, SIGIO);
		PROC_UNLOCK(sc->sc_async);
	}
}

int
uhidopen(struct cdev *dev, int flag, int mode, struct thread *p)
{
	struct uhid_softc *sc;
	usbd_status err;

	USB_GET_SC_OPEN(uhid, UHIDUNIT(dev), sc);

	DPRINTF(("uhidopen: sc=%p\n", sc));

	if (sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state & UHID_OPEN)
		return (EBUSY);
	sc->sc_state |= UHID_OPEN;

	clist_alloc_cblocks(&sc->sc_q, UHID_BSIZE, UHID_BSIZE);
	sc->sc_ibuf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
	sc->sc_obuf = malloc(sc->sc_osize, M_USBDEV, M_WAITOK);

	/* Set up interrupt pipe. */
	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr,
		  USBD_SHORT_XFER_OK, &sc->sc_intrpipe, sc, sc->sc_ibuf,
		  sc->sc_isize, uhid_intr, USBD_DEFAULT_INTERVAL);
	if (err) {
		DPRINTF(("uhidopen: usbd_open_pipe_intr failed, "
			 "error=%d\n",err));
		free(sc->sc_ibuf, M_USBDEV);
		free(sc->sc_obuf, M_USBDEV);
		sc->sc_ibuf = sc->sc_obuf = NULL;

		sc->sc_state &= ~UHID_OPEN;
		return (EIO);
	}

	sc->sc_state &= ~UHID_IMMED;

	sc->sc_async = 0;

	return (0);
}

int
uhidclose(struct cdev *dev, int flag, int mode, struct thread *p)
{
	struct uhid_softc *sc;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	DPRINTF(("uhidclose: sc=%p\n", sc));

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);
	sc->sc_intrpipe = 0;

	ndflush(&sc->sc_q, sc->sc_q.c_cc);
	clist_free_cblocks(&sc->sc_q);

	free(sc->sc_ibuf, M_USBDEV);
	free(sc->sc_obuf, M_USBDEV);
	sc->sc_ibuf = sc->sc_obuf = NULL;

	sc->sc_state &= ~UHID_OPEN;

	sc->sc_async = 0;

	return (0);
}

int
uhid_do_read(struct uhid_softc *sc, struct uio *uio, int flag)
{
	int s;
	int error = 0;
	size_t length;
	u_char buffer[UHID_CHUNK];
	usbd_status err;

	DPRINTFN(1, ("uhidread\n"));
	if (sc->sc_state & UHID_IMMED) {
		DPRINTFN(1, ("uhidread immed\n"));

		err = usbd_get_report(sc->sc_iface, UHID_INPUT_REPORT,
			  sc->sc_iid, buffer, sc->sc_isize);
		if (err)
			return (EIO);
		return (uiomove(buffer, sc->sc_isize, uio));
	}

	s = splusb();
	while (sc->sc_q.c_cc == 0) {
		if (flag & O_NONBLOCK) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_state |= UHID_ASLP;
		DPRINTFN(5, ("uhidread: sleep on %p\n", &sc->sc_q));
		error = tsleep(&sc->sc_q, PZERO | PCATCH, "uhidrea", 0);
		DPRINTFN(5, ("uhidread: woke, error=%d\n", error));
		if (sc->sc_dying)
			error = EIO;
		if (error) {
			sc->sc_state &= ~UHID_ASLP;
			break;
		}
		if (sc->sc_state & UHID_NEEDCLEAR) {
			DPRINTFN(-1,("uhidread: clearing stall\n"));
			sc->sc_state &= ~UHID_NEEDCLEAR;
			usbd_clear_endpoint_stall(sc->sc_intrpipe);
		}
	}
	splx(s);

	/* Transfer as many chunks as possible. */
	while (sc->sc_q.c_cc > 0 && uio->uio_resid > 0 && !error) {
		length = min(sc->sc_q.c_cc, uio->uio_resid);
		if (length > sizeof(buffer))
			length = sizeof(buffer);

		/* Remove a small chunk from the input queue. */
		(void) q_to_b(&sc->sc_q, buffer, length);
		DPRINTFN(5, ("uhidread: got %lu chars\n", (u_long)length));

		/* Copy the data to the user process. */
		if ((error = uiomove(buffer, length, uio)) != 0)
			break;
	}

	return (error);
}

int
uhidread(struct cdev *dev, struct uio *uio, int flag)
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_read(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	return (error);
}

int
uhid_do_write(struct uhid_softc *sc, struct uio *uio, int flag)
{
	int error;
	int size;
	usbd_status err;

	DPRINTFN(1, ("uhidwrite\n"));

	if (sc->sc_dying)
		return (EIO);

	size = sc->sc_osize;
	error = 0;
	if (uio->uio_resid != size)
		return (EINVAL);
	error = uiomove(sc->sc_obuf, size, uio);
	if (!error) {
		if (sc->sc_oid)
			err = usbd_set_report(sc->sc_iface, UHID_OUTPUT_REPORT,
				  sc->sc_obuf[0], sc->sc_obuf+1, size-1);
		else
			err = usbd_set_report(sc->sc_iface, UHID_OUTPUT_REPORT,
				  0, sc->sc_obuf, size);
		if (err)
			error = EIO;
	}

	return (error);
}

int
uhidwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_write(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	return (error);
}

int
uhid_do_ioctl(struct uhid_softc *sc, u_long cmd, caddr_t addr, int flag,
	      struct thread *p)
{
	struct usb_ctl_report_desc *rd;
	struct usb_ctl_report *re;
	int size, id;
	usbd_status err;

	DPRINTFN(2, ("uhidioctl: cmd=%lx\n", cmd));

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->sc_async != NULL)
				return (EBUSY);
			sc->sc_async = p->td_proc;
			DPRINTF(("uhid_do_ioctl: FIOASYNC %p\n", sc->sc_async));
		} else
			sc->sc_async = NULL;
		break;

	/* XXX this is not the most general solution. */
	case TIOCSPGRP:
		if (sc->sc_async == NULL)
			return (EINVAL);
		if (*(int *)addr != sc->sc_async->p_pgid)
			return (EPERM);
		break;

	case USB_GET_REPORT_DESC:
		rd = (struct usb_ctl_report_desc *)addr;
		size = min(sc->sc_repdesc_size, sizeof rd->ucrd_data);
		rd->ucrd_size = size;
		memcpy(rd->ucrd_data, sc->sc_repdesc, size);
		break;

	case USB_SET_IMMED:
		if (*(int *)addr) {
			/* XXX should read into ibuf, but does it matter? */
			err = usbd_get_report(sc->sc_iface, UHID_INPUT_REPORT,
				  sc->sc_iid, sc->sc_ibuf, sc->sc_isize);
			if (err)
				return (EOPNOTSUPP);

			sc->sc_state |=  UHID_IMMED;
		} else
			sc->sc_state &= ~UHID_IMMED;
		break;

	case USB_GET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		err = usbd_get_report(sc->sc_iface, re->ucr_report, id, re->ucr_data,
			  size);
		if (err)
			return (EIO);
		break;

	case USB_SET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			id = sc->sc_iid;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			id = sc->sc_oid;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			id = sc->sc_fid;
			break;
		default:
			return (EINVAL);
		}
		err = usbd_set_report(sc->sc_iface, re->ucr_report, id, re->ucr_data,
			  size);
		if (err)
			return (EIO);
		break;

	case USB_GET_REPORT_ID:
		*(int *)addr = 0;	/* XXX: we only support reportid 0? */
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
uhidioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *p)
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_ioctl(sc, cmd, addr, flag, p);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(sc->sc_dev);
	return (error);
}

int
uhidpoll(struct cdev *dev, int events, struct thread *p)
{
	struct uhid_softc *sc;
	int revents = 0;
	int s;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	if (sc->sc_dying)
		return (EIO);

	s = splusb();
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_q.c_cc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &sc->sc_rsel);
	}

	splx(s);
	return (revents);
}

DRIVER_MODULE(uhid, uhub, uhid_driver, uhid_devclass, usbd_driver_load, 0);
