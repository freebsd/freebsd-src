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
 * HID spec: http://www.usb.org/developers/data/devclass/hid1_1.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#if __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/hid.h>

#include <sys/mouse.h>

#ifdef UMS_DEBUG
#define DPRINTF(x)	if (umsdebug) logprintf x
#define DPRINTFN(n,x)	if (umsdebug>(n)) logprintf x
int	umsdebug = 1;
SYSCTL_INT(_debug_usb, OID_AUTO, ums, CTLFLAG_RW,
	   &umsdebug, 0, "ums debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UMSUNIT(s)	(minor(s)&0x1f)

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)                            

#define QUEUE_BUFSIZE	400	/* MUST be divisible by 5 _and_ 8 */

struct ums_softc {
	device_t sc_dev;		/* base device */
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	u_char *sc_ibuf;
	u_int8_t sc_iid;
	int sc_isize;
	struct hid_location sc_loc_x, sc_loc_y, sc_loc_z;
	struct hid_location *sc_loc_btn;

	usb_callout_t callout_handle;	/* for spurious button ups */

	int sc_enabled;
	int sc_disconnected;	/* device is gone */

	int flags;		/* device configuration */
#define UMS_Z		0x01	/* z direction available */
#define UMS_SPUR_BUT_UP	0x02	/* spurious button up events */
	int nbuttons;
#define MAX_BUTTONS	7	/* chosen because sc_buttons is u_char */

	u_char		qbuf[QUEUE_BUFSIZE];	/* must be divisable by 3&4 */
	u_char		dummy[100];	/* XXX just for safety and for now */
	int		qcount, qhead, qtail;
	mousehw_t	hw;
	mousemode_t	mode;
	mousestatus_t	status;

	int		state;
#	  define	UMS_ASLEEP	0x01	/* readFromDevice is waiting */
#	  define	UMS_SELECT	0x02	/* select is waiting */
	struct selinfo	rsel;		/* process waiting in select */

	dev_t		dev;		/* specfs */
};

#define MOUSE_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)
#define MOUSE_FLAGS (HIO_RELATIVE)

Static void ums_intr(usbd_xfer_handle xfer,
			  usbd_private_handle priv, usbd_status status);

Static void ums_add_to_queue(struct ums_softc *sc,
				int dx, int dy, int dz, int buttons);
Static void ums_add_to_queue_timeout(void *priv);

Static int  ums_enable(void *);
Static void ums_disable(void *);

Static d_open_t  ums_open;
Static d_close_t ums_close;
Static d_read_t  ums_read;
Static d_ioctl_t ums_ioctl;
Static d_poll_t  ums_poll;

#define UMS_CDEV_MAJOR	111

Static struct cdevsw ums_cdevsw = {
	/* open */	ums_open,
	/* close */	ums_close,
	/* read */	ums_read,
	/* write */	nowrite,
	/* ioctl */	ums_ioctl,
	/* poll */	ums_poll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ums",
	/* maj */	UMS_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
#if __FreeBSD_version < 500014
	/* bmaj */	-1
#endif
};

USB_DECLARE_DRIVER(ums);

USB_MATCH(ums)
{
	USB_MATCH_START(ums, uaa);
	usb_interface_descriptor_t *id;
	int size, ret;
	void *desc;
	usbd_status err;
	
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id || id->bInterfaceClass != UICLASS_HID)
		return (UMATCH_NONE);

	err = usbd_read_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (err)
		return (UMATCH_NONE);

	if (hid_is_collection(desc, size, 
			      HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		ret = UMATCH_IFACECLASS;
	else
		ret = UMATCH_NONE;

	free(desc, M_TEMP);
	return (ret);
}

USB_ATTACH(ums)
{
	USB_ATTACH_START(ums, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int size;
	void *desc;
	usbd_status err;
	char devinfo[1024];
	u_int32_t flags;
	int i;
	struct hid_location loc_btn;
	
	sc->sc_disconnected = 1;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (!ed) {
		printf("%s: could not read endpoint descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	DPRINTFN(10,("ums_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		     " bInterval=%d\n",
		     ed->bLength, ed->bDescriptorType, 
		     UE_GET_ADDR(ed->bEndpointAddress),
		     UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN ? "in":"out",
		     UE_GET_XFERTYPE(ed->bmAttributes),
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN ||
	    UE_GET_XFERTYPE(ed->bmAttributes) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_read_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (err)
		USB_ATTACH_ERROR_RETURN;

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		       hid_input, &sc->sc_loc_x, &flags)) {
		printf("%s: mouse has no X report\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("%s: X report 0x%04x not supported\n",
		       USBDEVNAME(sc->sc_dev), flags);
		USB_ATTACH_ERROR_RETURN;
	}

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		       hid_input, &sc->sc_loc_y, &flags)) {
		printf("%s: mouse has no Y report\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("%s: Y report 0x%04x not supported\n",
		       USBDEVNAME(sc->sc_dev), flags);
		USB_ATTACH_ERROR_RETURN;
	}

	/* try to guess the Z activator: first check Z, then WHEEL */
	if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
		       hid_input, &sc->sc_loc_z, &flags) ||
	    hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
		       hid_input, &sc->sc_loc_z, &flags)) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			sc->sc_loc_z.size = 0;	/* Bad Z coord, ignore it */
		} else {
			sc->flags |= UMS_Z;
		}
	}

	/* figure out the number of buttons */
	for (i = 1; i <= MAX_BUTTONS; i++)
		if (!hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
				hid_input, &loc_btn, 0))
			break;
	sc->nbuttons = i - 1;
	sc->sc_loc_btn = malloc(sizeof(struct hid_location)*sc->nbuttons, 
				M_USBDEV, M_NOWAIT);
	if (!sc->sc_loc_btn) {
		printf("%s: no memory\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	printf("%s: %d buttons%s\n", USBDEVNAME(sc->sc_dev),
	       sc->nbuttons, sc->flags & UMS_Z? " and Z dir." : "");

	for (i = 1; i <= sc->nbuttons; i++)
		hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
				hid_input, &sc->sc_loc_btn[i-1], 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input, &sc->sc_iid);
	sc->sc_ibuf = malloc(sc->sc_isize, M_USB, M_NOWAIT);
	if (!sc->sc_ibuf) {
		printf("%s: no memory\n", USBDEVNAME(sc->sc_dev));
		free(sc->sc_loc_btn, M_USB);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;
	free(desc, M_TEMP);

#ifdef UMS_DEBUG
	DPRINTF(("ums_attach: sc=%p\n", sc));
	DPRINTF(("ums_attach: X\t%d/%d\n", 
		 sc->sc_loc_x.pos, sc->sc_loc_x.size));
	DPRINTF(("ums_attach: Y\t%d/%d\n", 
		 sc->sc_loc_y.pos, sc->sc_loc_y.size));
	if (sc->flags & UMS_Z)
		DPRINTF(("ums_attach: Z\t%d/%d\n", 
			 sc->sc_loc_z.pos, sc->sc_loc_z.size));
	for (i = 1; i <= sc->nbuttons; i++) {
		DPRINTF(("ums_attach: B%d\t%d/%d\n",
			 i, sc->sc_loc_btn[i-1].pos,sc->sc_loc_btn[i-1].size));
	}
	DPRINTF(("ums_attach: size=%d, id=%d\n", sc->sc_isize, sc->sc_iid));
#endif

	if (sc->nbuttons > MOUSE_MSC_MAXBUTTON)
		sc->hw.buttons = MOUSE_MSC_MAXBUTTON;
	else
		sc->hw.buttons = sc->nbuttons;
	sc->hw.iftype = MOUSE_IF_USB;
	sc->hw.type = MOUSE_MOUSE;
	sc->hw.model = MOUSE_MODEL_GENERIC;
	sc->hw.hwid = 0;
	sc->mode.protocol = MOUSE_PROTO_MSC;
	sc->mode.rate = -1;
	sc->mode.resolution = MOUSE_RES_UNKNOWN;
	sc->mode.accelfactor = 0;
	sc->mode.level = 0;
	sc->mode.packetsize = MOUSE_MSC_PACKETSIZE;
	sc->mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
	sc->mode.syncmask[1] = MOUSE_MSC_SYNC;

	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;

#ifndef __FreeBSD__
	sc->rsel.si_flags = 0;
	sc->rsel.si_pid = 0;
#endif

	sc->dev = make_dev(&ums_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "ums%d", device_get_unit(self));

	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_SPUR_BUT_UP) {
		DPRINTF(("%s: Spurious button up events\n",
			USBDEVNAME(sc->sc_dev)));
		sc->flags |= UMS_SPUR_BUT_UP;
	}

	USB_ATTACH_SUCCESS_RETURN;
}


Static int
ums_detach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);
	struct vnode *vp;

	if (sc->sc_enabled)
		ums_disable(sc);

	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));

	free(sc->sc_loc_btn, M_USB);
	free(sc->sc_ibuf, M_USB);

	vp = SLIST_FIRST(&sc->dev->si_hlist);
	if (vp)
		VOP_REVOKE(vp, REVOKEALL);

	/* someone waiting for data */
	/*
	 * XXX If we wakeup the process here, the device will be gone by
	 * the time the process gets a chance to notice. *_close and friends
	 * should be fixed to handle this case.
	 * Or we should do a delayed detach for this.
	 * Does this delay now force tsleep to exit with an error?
	 */
	if (sc->state & UMS_ASLEEP) {
		sc->state &= ~UMS_ASLEEP;
		wakeup(sc);
	}
	if (sc->state & UMS_SELECT) {
		sc->state &= ~UMS_SELECT;
		selwakeup(&sc->rsel);
	}

	destroy_dev(sc->dev);

	return 0;
}

void
ums_intr(xfer, addr, status)
	usbd_xfer_handle xfer;
	usbd_private_handle addr;
	usbd_status status;
{
	struct ums_softc *sc = addr;
	u_char *ibuf;
	int dx, dy, dz;
	u_char buttons = 0;
	int i;

#define UMS_BUT(i) ((i) < 3 ? (((i) + 2) % 3) : (i))

	DPRINTFN(5, ("ums_intr: sc=%p status=%d\n", sc, status));
	DPRINTFN(5, ("ums_intr: data = %02x %02x %02x\n",
		     sc->sc_ibuf[0], sc->sc_ibuf[1], sc->sc_ibuf[2]));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ums_intr: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sc->sc_intrpipe);
		return;
	}

	ibuf = sc->sc_ibuf;
	if (sc->sc_iid) {
		if (*ibuf++ != sc->sc_iid)
			return;
	}

	dx =  hid_get_data(ibuf, &sc->sc_loc_x);
	dy = -hid_get_data(ibuf, &sc->sc_loc_y);
	dz = -hid_get_data(ibuf, &sc->sc_loc_z);
	for (i = 0; i < sc->nbuttons; i++)
		if (hid_get_data(ibuf, &sc->sc_loc_btn[i]))
			buttons |= (1 << UMS_BUT(i));

	if (dx || dy || dz || (sc->flags & UMS_Z)
	    || buttons != sc->status.button) {
		DPRINTFN(5, ("ums_intr: x:%d y:%d z:%d buttons:0x%x\n",
			dx, dy, dz, buttons));

		sc->status.button = buttons;
		sc->status.dx += dx;
		sc->status.dy += dy;
		sc->status.dz += dz;

		/* Discard data in case of full buffer */
		if (sc->qcount == sizeof(sc->qbuf)) {
			DPRINTF(("Buffer full, discarded packet"));
			return;
		}

		/*
		 * The Qtronix keyboard has a built in PS/2 port for a mouse.
		 * The firmware once in a while posts a spurious button up
		 * event. This event we ignore by doing a timeout for 50 msecs.
		 * If we receive dx=dy=dz=buttons=0 before we add the event to
		 * the queue.
		 * In any other case we delete the timeout event.
		 */
		if (sc->flags & UMS_SPUR_BUT_UP &&
		    dx == 0 && dy == 0 && dz == 0 && buttons == 0) {
			usb_callout(sc->callout_handle, MS_TO_TICKS(50 /*msecs*/),
				    ums_add_to_queue_timeout, (void *) sc);
		} else {
			usb_uncallout(sc->callout_handle,
				      ums_add_to_queue_timeout, (void *) sc);
			ums_add_to_queue(sc, dx, dy, dz, buttons);
		}
	}
}

Static void
ums_add_to_queue_timeout(void *priv)
{
	struct ums_softc *sc = priv;
	int s;

	s = splusb();
	ums_add_to_queue(sc, 0, 0, 0, 0);
	splx(s);
}

Static void
ums_add_to_queue(struct ums_softc *sc, int dx, int dy, int dz, int buttons)
{
	/* Discard data in case of full buffer */
	if (sc->qhead+sc->mode.packetsize > sizeof(sc->qbuf)) {
		DPRINTF(("Buffer full, discarded packet"));
		return;
	}

	if (dx >  254)		dx =  254;
	if (dx < -256)		dx = -256;
	if (dy >  254)		dy =  254;
	if (dy < -256)		dy = -256;
	if (dz >  126)		dz =  126;
	if (dz < -128)		dz = -128;

	sc->qbuf[sc->qhead] = sc->mode.syncmask[1];
	sc->qbuf[sc->qhead] |= ~buttons & MOUSE_MSC_BUTTONS;
	sc->qbuf[sc->qhead+1] = dx >> 1;
	sc->qbuf[sc->qhead+2] = dy >> 1;
	sc->qbuf[sc->qhead+3] = dx - (dx >> 1);
	sc->qbuf[sc->qhead+4] = dy - (dy >> 1);

	if (sc->mode.level == 1) {
		sc->qbuf[sc->qhead+5] = dz >> 1;
		sc->qbuf[sc->qhead+6] = dz - (dz >> 1);
		sc->qbuf[sc->qhead+7] = ((~buttons >> 3)
					 & MOUSE_SYS_EXTBUTTONS);
	}

	sc->qhead += sc->mode.packetsize;
	sc->qcount += sc->mode.packetsize;
	/* wrap round at end of buffer */
	if (sc->qhead >= sizeof(sc->qbuf))
		sc->qhead = 0;

	/* someone waiting for data */
	if (sc->state & UMS_ASLEEP) {
		sc->state &= ~UMS_ASLEEP;
		wakeup(sc);
	}
	if (sc->state & UMS_SELECT) {
		sc->state &= ~UMS_SELECT;
		selwakeup(&sc->rsel);
	}
}
Static int
ums_enable(v)
	void *v;
{
	struct ums_softc *sc = v;

	usbd_status err;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	sc->qcount = 0;
	sc->qhead = sc->qtail = 0;
	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;

	callout_handle_init((struct callout_handle *)&sc->callout_handle);

	/* Set up interrupt pipe. */
	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr, 
				USBD_SHORT_XFER_OK, &sc->sc_intrpipe, sc, 
				sc->sc_ibuf, sc->sc_isize, ums_intr,
				USBD_DEFAULT_INTERVAL);
	if (err) {
		DPRINTF(("ums_enable: usbd_open_pipe_intr failed, error=%d\n",
			 err));
		sc->sc_enabled = 0;
		return (EIO);
	}
	return (0);
}

Static void
ums_disable(priv)
	void *priv;
{
	struct ums_softc *sc = priv;

	usb_uncallout(sc->callout_handle, ums_add_to_queue_timeout, sc);

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);

	sc->sc_enabled = 0;

	if (sc->qcount != 0)
		DPRINTF(("Discarded %d bytes in queue\n", sc->qcount));
}

Static int
ums_open(dev_t dev, int flag, int fmt, usb_proc_ptr p)
{
	struct ums_softc *sc;

	USB_GET_SC_OPEN(ums, UMSUNIT(dev), sc);

	return ums_enable(sc);
}

Static int
ums_close(dev_t dev, int flag, int fmt, usb_proc_ptr p)
{
	struct ums_softc *sc;

	USB_GET_SC(ums, UMSUNIT(dev), sc);

	if (!sc)
		return 0;

	if (sc->sc_enabled)
		ums_disable(sc);

	return 0;
}

Static int
ums_read(dev_t dev, struct uio *uio, int flag)
{
	struct ums_softc *sc;
	int s;
	char buf[sizeof(sc->qbuf)];
	int l = 0;
	int error;

	USB_GET_SC(ums, UMSUNIT(dev), sc);

	s = splusb();
	if (!sc) {
		splx(s);
		return EIO;
	}

	while (sc->qcount == 0 )  {
		if (flag & IO_NDELAY) {		/* non-blocking I/O */
			splx(s);
			return EWOULDBLOCK;
		}
		
		sc->state |= UMS_ASLEEP;	/* blocking I/O */
		error = tsleep(sc, PZERO | PCATCH, "umsrea", 0);
		if (error) {
			splx(s);
			return error;
		} else if (!sc->sc_enabled) {
			splx(s);
			return EINTR;
		}
		/* check whether the device is still there */

		sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
		if (!sc) {
			splx(s);
			return EIO;
		}
	}

	/*
	 * XXX we could optimise the use of splx/splusb somewhat. The writer
	 * process only extends qcount and qtail. We could copy them and use the copies
	 * to do the copying out of the queue.
	 */

	while ((sc->qcount > 0) && (uio->uio_resid > 0)) {
		l = (sc->qcount < uio->uio_resid? sc->qcount:uio->uio_resid);
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (l > sizeof(sc->qbuf) - sc->qtail)		/* transfer till end of buf */
			l = sizeof(sc->qbuf) - sc->qtail;

		splx(s);
		uiomove(&sc->qbuf[sc->qtail], l, uio);
		s = splusb();

		if ( sc->qcount - l < 0 ) {
			DPRINTF(("qcount below 0, count=%d l=%d\n", sc->qcount, l));
			sc->qcount = l;
		}
		sc->qcount -= l;	/* remove the bytes from the buffer */
		sc->qtail = (sc->qtail + l) % sizeof(sc->qbuf);
	}
	splx(s);

	return 0;
}

Static int
ums_poll(dev_t dev, int events, usb_proc_ptr p)
{
	struct ums_softc *sc;
	int revents = 0;
	int s;

	USB_GET_SC(ums, UMSUNIT(dev), sc);

	if (!sc)
		return 0;

	s = splusb();
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->qcount) {
			revents = events & (POLLIN | POLLRDNORM);
		} else {
			sc->state |= UMS_SELECT;
			selrecord(p, &sc->rsel);
		}
	}
	splx(s);

	return revents;
}
	
int
ums_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, usb_proc_ptr p)
{
	struct ums_softc *sc;
	int error = 0;
	int s;
	mousemode_t mode;

	USB_GET_SC(ums, UMSUNIT(dev), sc);

	if (!sc)
		return EIO;

	switch(cmd) {
	case MOUSE_GETHWINFO:
		*(mousehw_t *)addr = sc->hw;
		break;
	case MOUSE_GETMODE:
		*(mousemode_t *)addr = sc->mode;
		break;
	case MOUSE_SETMODE:
		mode = *(mousemode_t *)addr;

		if (mode.level == -1)
			/* don't change the current setting */
			;
		else if ((mode.level < 0) || (mode.level > 1))
			return (EINVAL);

		s = splusb();
		sc->mode.level = mode.level;

		if (sc->mode.level == 0) {
			if (sc->nbuttons > MOUSE_MSC_MAXBUTTON)
				sc->hw.buttons = MOUSE_MSC_MAXBUTTON;
			else
				sc->hw.buttons = sc->nbuttons;
			sc->mode.protocol = MOUSE_PROTO_MSC;
			sc->mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->mode.level == 1) {
			if (sc->nbuttons > MOUSE_SYS_MAXBUTTON)
				sc->hw.buttons = MOUSE_SYS_MAXBUTTON;
			else
				sc->hw.buttons = sc->nbuttons;
			sc->mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->mode.syncmask[1] = MOUSE_SYS_SYNC;
		}

		bzero(sc->qbuf, sizeof(sc->qbuf));
		sc->qhead = sc->qtail = sc->qcount = 0;
		splx(s);

		break;
	case MOUSE_GETLEVEL:
		*(int *)addr = sc->mode.level;
		break;
	case MOUSE_SETLEVEL:
		if (*(int *)addr < 0 || *(int *)addr > 1)
			return (EINVAL);

		s = splusb();
		sc->mode.level = *(int *)addr;

		if (sc->mode.level == 0) {
			if (sc->nbuttons > MOUSE_MSC_MAXBUTTON)
				sc->hw.buttons = MOUSE_MSC_MAXBUTTON;
			else
				sc->hw.buttons = sc->nbuttons;
			sc->mode.protocol = MOUSE_PROTO_MSC;
			sc->mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->mode.level == 1) {
			if (sc->nbuttons > MOUSE_SYS_MAXBUTTON)
				sc->hw.buttons = MOUSE_SYS_MAXBUTTON;
			else
				sc->hw.buttons = sc->nbuttons;
			sc->mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->mode.syncmask[1] = MOUSE_SYS_SYNC;
		}

		bzero(sc->qbuf, sizeof(sc->qbuf));
		sc->qhead = sc->qtail = sc->qcount = 0;
		splx(s);

		break;
	case MOUSE_GETSTATUS: {
		mousestatus_t *status = (mousestatus_t *) addr;

		s = splusb();
		*status = sc->status;
		sc->status.obutton = sc->status.button;
		sc->status.button = 0;
		sc->status.dx = sc->status.dy = sc->status.dz = 0;
		splx(s);

		if (status->dx || status->dy || status->dz)
			status->flags |= MOUSE_POSCHANGED;
		if (status->button != status->obutton)
			status->flags |= MOUSE_BUTTONSCHANGED;
		break;
		}
	default:
		error = ENOTTY;
	}

	return error;
}

DRIVER_MODULE(ums, uhub, ums_driver, ums_devclass, usbd_driver_load, 0);
