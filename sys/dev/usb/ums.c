/*	$NetBSD: ums.c,v 1.22 1999/01/12 22:06:48 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
#if defined(__NetBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#endif
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
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/hid.h>

#if defined(__NetBSD__)
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#elif defined(__FreeBSD__)
#include <machine/mouse.h>
#endif

#ifdef UMS_DEBUG
#define DPRINTF(x)	if (umsdebug) logprintf x
#define DPRINTFN(n,x)	if (umsdebug>(n)) logprintf x
int	umsdebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UMSUNIT(s)	(minor(s)&0x1f)

#define QUEUE_BUFSIZE	400	/* MUST be divisible by 5 _and_ 8 */

struct ums_softc {
	bdevice sc_dev;			/* base device */
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	u_char *sc_ibuf;
	u_int8_t sc_iid;
	int sc_isize;
	struct hid_location sc_loc_x, sc_loc_y, sc_loc_z;
	struct hid_location *sc_loc_btn;

	int sc_enabled;
	int sc_disconnected;	/* device is gone */

	int flags;		/* device configuration */
#define UMS_Z		0x01	/* z direction available */
	int nbuttons;
#define MAX_BUTTONS	7	/* chosen because sc_buttons is u_char */

#if defined(__NetBSD__)
	u_char sc_buttons;	/* mouse button status */
	struct device *sc_wsmousedev;
#elif defined(__FreeBSD__)
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
#endif
};

#define MOUSE_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)
#define MOUSE_FLAGS (HIO_RELATIVE)

void ums_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));
void ums_disco __P((void *));

static int	ums_enable __P((void *));
static void	ums_disable __P((void *));

#if defined(__NetBSD__)
static int	ums_ioctl __P((void *, u_long, caddr_t, int, struct proc *));

const struct wsmouse_accessops ums_accessops = {
	ums_enable,
	ums_ioctl,
	ums_disable,
};

#elif defined(__FreeBSD__)
static d_open_t ums_open;
static d_close_t ums_close;
static d_read_t ums_read;
static d_ioctl_t ums_ioctl;
static d_poll_t ums_poll;

#define UMS_CDEV_MAJOR	111

static struct cdevsw ums_cdevsw = {
	ums_open,	ums_close,	ums_read,	nowrite,
	ums_ioctl,	nostop,		nullreset,	nodevtotty,
	ums_poll,	nommap,		nostrat,
	"ums",		NULL,		-1
};
#endif

USB_DECLARE_DRIVER(ums);

USB_MATCH(ums)
{
	USB_MATCH_START(ums, uaa);
	usb_interface_descriptor_t *id;
	int size, ret;
	void *desc;
	usbd_status r;
	
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id || id->bInterfaceClass != UCLASS_HID)
		return (UMATCH_NONE);

	r = usbd_alloc_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (r != USBD_NORMAL_COMPLETION)
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
#if defined(__NetBSD__)
	struct wsmousedev_attach_args a;
#endif
	int size;
	void *desc;
	usbd_status r;
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
		     ed->bEndpointAddress & UE_ADDR,
		     ed->bEndpointAddress & UE_IN ? "in" : "out",
		     ed->bmAttributes & UE_XFERTYPE,
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if ((ed->bEndpointAddress & UE_IN) != UE_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	r = usbd_alloc_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (r != USBD_NORMAL_COMPLETION)
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
		 sc->sc_loc_x.pos, sc->sc_loc_x.size));
	if (sc->flags & UMS_Z)
		DPRINTF(("ums_attach: Z\t%d/%d\n", 
			 sc->sc_loc_z.pos, sc->sc_loc_z.size));
	for (i = 1; i <= sc->nbuttons; i++) {
		DPRINTF(("ums_attach: B%d\t%d/%d\n",
			 i, sc->sc_loc_btn[i-1].pos,sc->sc_loc_btn[i-1].size));
	}
	DPRINTF(("ums_attach: size=%d, id=%d\n", sc->sc_isize, sc->sc_iid));
#endif

#if defined(__NetBSD__)
	a.accessops = &ums_accessops;
	a.accesscookie = sc;

	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
#elif defined(__FreeBSD__)
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

	sc->rsel.si_flags = 0;
	sc->rsel.si_pid = 0;
#endif

	USB_ATTACH_SUCCESS_RETURN;
}


#if defined(__FreeBSD__)
static int
ums_detach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);

	if (sc->sc_enabled) {
		usbd_abort_pipe(sc->sc_intrpipe);
		usbd_close_pipe(sc->sc_intrpipe);
	}
	sc->sc_disconnected = 1;

	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	device_set_desc(self, NULL);
	free(sc->sc_loc_btn, M_USB);
	free(sc->sc_ibuf, M_USB);

	/* someone waiting for data */
	if (sc->state & UMS_ASLEEP) {
		sc->state &= ~UMS_ASLEEP;
		wakeup(sc);
	}
	if (sc->state & UMS_SELECT) {
		sc->state &= ~UMS_SELECT;
		selwakeup(&sc->rsel);
	}

	return 0;
}
#endif

void
ums_disco(p)
	void *p;
{
	struct ums_softc *sc = p;

	DPRINTF(("ums_disco: sc=%p\n", sc));
	usbd_abort_pipe(sc->sc_intrpipe);
	sc->sc_disconnected = 1;
}

void
ums_intr(reqh, addr, status)
	usbd_request_handle reqh;
	usbd_private_handle addr;
	usbd_status status;
{
	struct ums_softc *sc = addr;
	u_char *ibuf;
	int dx, dy, dz;
	u_char buttons = 0;
	int i;

#if defined(__NetBSD__)
#define UMS_BUT(i) ((i) == 1 || (i) == 2 ? 3 - (i) : i)
#elif defined(__FreeBSD__)
#define UMS_BUT(i) ((i) < 3 ? (((i) + 2) % 3) : (i))
#endif

	DPRINTFN(5, ("ums_intr: sc=%p status=%d\n", sc, status));
	DPRINTFN(5, ("ums_intr: data = %02x %02x %02x\n",
		     sc->sc_ibuf[0], sc->sc_ibuf[1], sc->sc_ibuf[2]));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ums_intr: status=%d\n", status));
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

#if defined(__NetBSD__)
	if (dx || dy || buttons != sc->sc_buttons) {
		DPRINTFN(10, ("ums_intr: x:%d y:%d z:%d buttons:0x%x\n",
			dx, dy, dz, buttons));
		sc->sc_buttons = buttons;
		if (sc->sc_wsmousedev)
			wsmouse_input(sc->sc_wsmousedev, buttons, dx, dy, dz);
#elif defined(__FreeBSD__)
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
#ifdef UMS_DEBUG
		if (sc->qhead > sizeof(sc->qbuf))
			DPRINTF(("Buffer overrun! %d %d\n", 
				 sc->qhead, sizeof(sc->qbuf)));
#endif
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
#endif
	}
}


static int
ums_enable(v)
	void *v;
{
	struct ums_softc *sc = v;

	usbd_status r;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
#if defined(__NetBSD__)
	sc->sc_buttons = 0;
#elif defined(__FreeBSD__)
	sc->qcount = 0;
	sc->qhead = sc->qtail = 0;
	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;
#endif

	/* Set up interrupt pipe. */
	r = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr, 
				USBD_SHORT_XFER_OK, &sc->sc_intrpipe, sc, 
				sc->sc_ibuf, sc->sc_isize, ums_intr);
	if (r != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ums_enable: usbd_open_pipe_intr failed, error=%d\n",
			 r));
		sc->sc_enabled = 0;
		return (EIO);
	}
	usbd_set_disco(sc->sc_intrpipe, ums_disco, sc);
	return (0);
}

static void
ums_disable(v)
	void *v;
{
	struct ums_softc *sc = v;

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);

	sc->sc_enabled = 0;

#if defined(__FreeBSD__)
	if (sc->qcount != 0)
		DPRINTF(("Discarded %d bytes in queue\n", sc->qcount));
#endif
}

#if defined(__NetBSD__)
static int
ums_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;

{
	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_USB;
		return (0);
	}

	return (-1);
}

#elif defined(__FreeBSD__)
static int
ums_open(dev_t dev, int flag, int fmt, struct proc *p)
{
	USB_GET_SC_OPEN(ums, UMSUNIT(dev), sc);

	return ums_enable(sc);
}

static int
ums_close(dev_t dev, int flag, int fmt, struct proc *p)
{
	USB_GET_SC(ums, UMSUNIT(dev), sc);

	if (sc->sc_enabled)
		ums_disable(sc);

	return 0;
}

static int
ums_read(dev_t dev, struct uio *uio, int flag)
{
	USB_GET_SC(ums, UMSUNIT(dev), sc);
	int s;
	char buf[sizeof(sc->qbuf)];
	int l = 0;
	int error;

	s = splusb();
	while (sc->qcount == 0 )  {
		/* NWH XXX non blocking I/O ??
		if (non blocking I/O ) {
			splx(s);
			return EWOULDBLOCK;
		} else {
		*/
		sc->state |= UMS_ASLEEP;
		error = tsleep(sc, PZERO | PCATCH, "umsrea", 0);
		if (error) {
			splx(s);
			return error;
		} else if (!sc->sc_enabled) {
			splx(s);
			return EINTR;
		}
	}

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

static int
ums_poll(dev_t dev, int events, struct proc *p)
{
	USB_GET_SC(ums, UMSUNIT(dev), sc);
	int revents = 0;
	int s;

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
ums_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	USB_GET_SC(ums, UMSUNIT(dev), sc);
	int error = 0;
	int s;
	mousemode_t mode;

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
#endif

#if defined(__FreeBSD__)
CDEV_DRIVER_MODULE(ums, uhub, ums_driver, ums_devclass,
			UMS_CDEV_MAJOR, ums_cdevsw, usbd_driver_load, 0);
#endif
