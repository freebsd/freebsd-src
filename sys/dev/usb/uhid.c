/*	$NetBSD: uhid.c,v 1.38 2000/04/27 15:26:48 augustss Exp $	*/
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
 * HID spec: http://www.usb.org/developers/data/usbhid10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#endif
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>

#ifdef UHID_DEBUG
#define DPRINTF(x)	if (uhiddebug) logprintf x
#define DPRINTFN(n,x)	if (uhiddebug>(n)) logprintf x
int	uhiddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhid_softc {
	USBBASEDEVICE sc_dev;			/* base device */
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

#if defined(__FreeBSD__)
	dev_t dev;
#endif
};

#define	UHIDUNIT(dev)	(minor(dev))
#define	UHID_CHUNK	128	/* chunk size for read */
#define	UHID_BSIZE	1020	/* buffer size */

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(uhid);
#elif defined(__FreeBSD__)
d_open_t	uhidopen;
d_close_t	uhidclose;
d_read_t	uhidread;
d_write_t	uhidwrite;
d_ioctl_t	uhidioctl;
d_poll_t	uhidpoll;

#define		UHID_CDEV_MAJOR 122

Static struct cdevsw uhid_cdevsw = {
	/* open */	uhidopen,
	/* close */	uhidclose,
	/* read */	uhidread,
	/* write */	uhidwrite,
	/* ioctl */	uhidioctl,
	/* poll */	uhidpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"uhid",
	/* maj */	UHID_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};
#endif

Static void uhid_intr(usbd_xfer_handle, usbd_private_handle,
			   usbd_status);

Static int uhid_do_read(struct uhid_softc *, struct uio *uio, int);
Static int uhid_do_write(struct uhid_softc *, struct uio *uio, int);
Static int uhid_do_ioctl(struct uhid_softc *, u_long, caddr_t, int,
			      struct proc *);

USB_DECLARE_DRIVER(uhid);

USB_MATCH(uhid)
{
	USB_MATCH_START(uhid, uaa);
	usb_interface_descriptor_t *id;
	
	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL || id->bInterfaceClass != UCLASS_HID)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_GENERIC);
}

USB_ATTACH(uhid)
{
	USB_ATTACH_START(uhid, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int size;
	void *desc;
	usbd_status err;
	char devinfo[1024];
	
	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);

	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: could not read endpoint descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
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
		printf("%s: unexpected endpoint\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;

	desc = 0;
	err = usbd_alloc_report_desc(uaa->iface, &desc, &size, M_USBDEV);
	if (err) {
		printf("%s: no report descriptor\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		if (desc != NULL)
			free(desc, M_USBDEV);
		USB_ATTACH_ERROR_RETURN;
	}
	
	(void)usbd_set_idle(iface, 0, 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input,   &sc->sc_iid);
	sc->sc_osize = hid_report_size(desc, size, hid_output,  &sc->sc_oid);
	sc->sc_fsize = hid_report_size(desc, size, hid_feature, &sc->sc_fid);

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

#if defined(__FreeBSD__)
	sc->dev = make_dev(&uhid_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "uhid%d", device_get_unit(self));
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uhid_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct uhid_softc *sc = (struct uhid_softc *)self;

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

USB_DETACH(uhid)
{
	USB_DETACH_START(uhid, sc);
	int s;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int maj, mn;
#elif defined(__FreeBSD__)
	struct vnode *vp;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	DPRINTF(("uhid_detach: sc=%p flags=%d\n", sc, flags));
#else
	DPRINTF(("uhid_detach: sc=%p\n", sc));
#endif

	sc->sc_dying = 1;
	if (sc->sc_intrpipe != NULL)
		usbd_abort_pipe(sc->sc_intrpipe);

	if (sc->sc_state & UHID_OPEN) {
		s = splusb();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone */
			wakeup(&sc->sc_q);
			/* Wait for processes to go away. */
			usb_detach_wait(USBDEV(sc->sc_dev));
		}
		splx(s);
	}

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == uhidopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);
#elif defined(__FreeBSD__)
	vp = SLIST_FIRST(&sc->dev->si_hlist);
	if (vp)
		VOP_REVOKE(vp, REVOKEALL);

	destroy_dev(sc->dev);
#endif

	free(sc->sc_repdesc, M_USBDEV);

	return (0);
}

void
uhid_intr(xfer, addr, status)
	usbd_xfer_handle xfer;
	usbd_private_handle addr;
	usbd_status status;
{
	struct uhid_softc *sc = addr;

#ifdef UHID_DEBUG
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
		sc->sc_state |= UHID_NEEDCLEAR;
		return;
	}

	(void) b_to_q(sc->sc_ibuf, sc->sc_isize, &sc->sc_q);
		
	if (sc->sc_state & UHID_ASLP) {
		sc->sc_state &= ~UHID_ASLP;
		DPRINTFN(5, ("uhid_intr: waking %p\n", sc));
		wakeup(&sc->sc_q);
	}
	selwakeup(&sc->sc_rsel);
	if (sc->sc_async != NULL) {
		DPRINTFN(3, ("uhid_intr: sending SIGIO %p\n", sc->sc_async));
		psignal(sc->sc_async, SIGIO);
	}
}

int
uhidopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
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

	if (clalloc(&sc->sc_q, UHID_BSIZE, 0) == -1) {
		sc->sc_state &= ~UHID_OPEN;
		return (ENOMEM);
	}

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
		sc->sc_state &= ~UHID_OPEN;
		return (EIO);
	}

	sc->sc_state &= ~UHID_IMMED;

	sc->sc_async = 0;

	return (0);
}

int
uhidclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct uhid_softc *sc;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	DPRINTF(("uhidclose: sc=%p\n", sc));

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);
	sc->sc_intrpipe = 0;

	ndflush(&sc->sc_q, sc->sc_q.c_cc);
	clfree(&sc->sc_q);

	free(sc->sc_ibuf, M_USBDEV);
	free(sc->sc_obuf, M_USBDEV);

	sc->sc_state &= ~UHID_OPEN;

	sc->sc_async = 0;

	return (0);
}

int
uhid_do_read(sc, uio, flag)
	struct uhid_softc *sc;
	struct uio *uio;
	int flag;
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
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_state |= UHID_ASLP;
		DPRINTFN(5, ("uhidread: sleep on %p\n", sc));
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
uhidread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_read(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

int
uhid_do_write(sc, uio, flag)
	struct uhid_softc *sc;
	struct uio *uio;
	int flag;
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
uhidwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_write(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

int
uhid_do_ioctl(sc, cmd, addr, flag, p)
	struct uhid_softc *sc;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
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
			sc->sc_async = p;
			DPRINTF(("uhid_do_ioctl: FIOASYNC %p\n", p));
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
		size = min(sc->sc_repdesc_size, sizeof rd->data);
		rd->size = size;
		memcpy(rd->data, sc->sc_repdesc, size);
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
		switch (re->report) {
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
		err = usbd_get_report(sc->sc_iface, re->report, id, re->data,
			  size);
		if (err)
			return (EIO);
		break;

	case USB_SET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->report) {
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
		err = usbd_set_report(sc->sc_iface, re->report, id, re->data,
			  size);
		if (err)
			return (EIO);
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
uhidioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct uhid_softc *sc;
	int error;

	USB_GET_SC(uhid, UHIDUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uhid_do_ioctl(sc, cmd, addr, flag, p);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

int
uhidpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
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

#if defined(__FreeBSD__)
DRIVER_MODULE(uhid, uhub, uhid_driver, uhid_devclass, usbd_driver_load, 0);
#endif
