/*	$NetBSD: ums.c,v 1.8 1998/08/01 20:11:39 augustss Exp $	*/
/*	FreeBSD $Id: ums.c,v 1.3 1998/12/14 09:32:24 n_hibma Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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

#include <dev/usb/usb_port.h>

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
#include <dev/usb/usbdivar.h>
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

#ifdef USB_DEBUG
#define DPRINTF(x)	if (umsdebug) printf x
#define DPRINTFN(n,x)	if (umsdebug>(n)) printf x
int	umsdebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UMSUNIT(s)	(minor(s)&0x1f)

#define PS2LBUTMASK	x01
#define PS2RBUTMASK	x02
#define PS2MBUTMASK	x04
#define PS2BUTMASK	0x0f

#define QUEUE_BUFSIZE	240	/* MUST be dividable by 3 _and_ 4 */

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
#	define UMS_Z		0x01	/* z direction available */
	int nbuttons;

#if defined(__NetBSD__)
	u_char sc_buttons;	/* mouse button status */
	struct device *sc_wsmousedev;
#elif defined(__FreeBSD__)
	u_char		qbuf[QUEUE_BUFSIZE];
	u_char		dummy[100];		/* just for safety and for now */
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

#define MOUSE_FLAGS_MASK	(HIO_CONST|HIO_RELATIVE)
#define MOUSE_FLAGS		(HIO_RELATIVE)

#if defined(__NetBSD__)
int ums_match __P((struct device *, struct cfdata *, void *));
void ums_attach __P((struct device *, struct device *, void *));
#elif defined(__FreeBSD__)
static device_probe_t ums_match;
static device_attach_t ums_attach;
static device_detach_t ums_detach;
#endif

void ums_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));
void ums_disco __P((void *));

static int	ums_enable __P((void *));
static void	ums_disable __P((void *));

#if defined(__NetBSD__)
static int	ums_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
#elif defined(__FreeBSD__)
static d_open_t ums_open;
static d_close_t ums_close;
static d_read_t ums_read;
static d_ioctl_t ums_ioctl;
static d_poll_t ums_poll;

#define UMS_CDEV_MAJOR	111

static struct  cdevsw ums_cdevsw = {
	ums_open,	ums_close,	ums_read,	nowrite,
	ums_ioctl,	nostop,		nullreset,	nodevtotty,
	ums_poll,	nommap,
	NULL,		"ums_",		NULL,		-1
};
#endif

#if defined(__NetBSD__)
const struct wsmouse_accessops ums_accessops = {
	ums_enable,
	ums_ioctl,
	ums_disable,
};
#endif

#if defined(__NetBSD__)
extern struct cfdriver ums_cd;

struct cfattach ums_ca = {
	sizeof(struct ums_softc), ums_match, ums_attach
};
#elif defined(__FreeBSD__)
static devclass_t ums_devclass;

static device_method_t ums_methods[] = {
        DEVMETHOD(device_probe, ums_match),
        DEVMETHOD(device_attach, ums_attach),
        DEVMETHOD(device_detach, ums_detach),
        {0,0}
};

static driver_t ums_driver = {
        "ums", 
        ums_methods,   
        DRIVER_TYPE_MISC,       
        sizeof(struct ums_softc)
};
#endif


#if defined(__NetBSD__)
int
ums_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
ums_match(device_t device)
{
        struct usb_attach_arg *uaa = device_get_ivars(device);
#endif
	usb_interface_descriptor_t *id;
	int size, ret;
	void *desc;
	usbd_status r;
	
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id->bInterfaceClass != UCLASS_HID)
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

#if defined(__NetBSD__)
void
ums_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ums_softc *sc = (struct ums_softc *)self;
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
ums_attach(device_t self)
{
        struct ums_softc *sc = device_get_softc(self);
        struct usb_attach_arg *uaa = device_get_ivars(self);
#endif
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
#if defined(__NetBSD__)
	struct wsmousedev_attach_args a;
#endif
	char devinfo[1024];
	int size;
	void *desc;
	usbd_status r;
	u_int32_t flags;
	struct hid_location loc_btn;
	int i;
	
	sc->sc_disconnected = 1;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
#if defined(__FreeBSD__)
	usb_device_set_desc(self, devinfo);
	printf("%s%d", device_get_name(self), device_get_unit(self));
#endif
	printf(": %s (interface class %d/%d)\n", devinfo,
	       id->bInterfaceClass, id->bInterfaceSubClass);
	sc->sc_dev = self;
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (!ed) {
		DEVICE_ERROR(sc->sc_dev, ("could not read endpoint descriptor\n"));
		ATTACH_ERROR_RETURN;
	}

	DPRINTFN(10,("ums_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d "
		     "bInterval=%d\n",
	       ed->bLength, ed->bDescriptorType, ed->bEndpointAddress & UE_ADDR,
	       ed->bEndpointAddress & UE_IN ? "in" : "out",
	       ed->bmAttributes & UE_XFERTYPE,
	       UGETW(ed->wMaxPacketSize), ed->bInterval));

	if ((ed->bEndpointAddress & UE_IN) != UE_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		DEVICE_ERROR(sc->sc_dev, ("unexpected endpoint\n"));
		ATTACH_ERROR_RETURN;
	}

	r = usbd_alloc_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (r != USBD_NORMAL_COMPLETION)
		ATTACH_ERROR_RETURN;

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		       hid_input, &sc->sc_loc_x, &flags)) {
		DEVICE_ERROR(sc->sc_dev, ("mouse has no X report\n"));
		ATTACH_ERROR_RETURN;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS)
		DEVICE_ERROR(sc->sc_dev, ("X report 0x%04x not supported\n",
						flags));

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		       hid_input, &sc->sc_loc_y, &flags)) {
		DEVICE_ERROR(sc->sc_dev, ("mouse has no Y report\n"));
		ATTACH_ERROR_RETURN;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS)
		DEVICE_ERROR(sc->sc_dev, ("Y report 0x%04x not supported\n",
						flags));

#ifndef USBVERBOSE
	if (bootverbose)
#endif
	{
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
				       hid_input, &sc->sc_loc_z, &flags))
			DEVICE_MSG(sc->sc_dev, ("Device has Z axis\n"));
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_SLIDER),
				       hid_input, &sc->sc_loc_z, &flags))
			DEVICE_MSG(sc->sc_dev, ("Device has Slider\n"));
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_DIAL),
				       hid_input, &sc->sc_loc_z, &flags))
			DEVICE_MSG(sc->sc_dev, ("Device has Dial\n"));
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
				       hid_input, &sc->sc_loc_z, &flags))
			DEVICE_MSG(sc->sc_dev, ("Device has Wheel\n"));
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_HAT_SWITCH),
				       hid_input, &sc->sc_loc_z, &flags))
			DEVICE_MSG(sc->sc_dev, ("Device has Hat Switch\n"));
	}

	/* try to guess the Z activator: first check Z, then WHEEL */
	if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z),
		       hid_input, &sc->sc_loc_z, &flags) ||
	    hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_WHEEL),
		       hid_input, &sc->sc_loc_z, &flags)) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			sc->sc_loc_z.size = 0;	/* Bad Z coord, ignore it */
#if !defined(__FreeBSD__)	/* FIXME */
		/* IntelliMouse protocol is not properly implemented yet.
		 * Probably the wisest to use the sysmouse protocol
		 */
		} else {
			sc->flags |= UMS_Z;
#endif
		}
	}

	/* figure out the number of buttons, 7 is an arbitrary limit */
	for (i = 1; i <= 7; i++)
		if (!hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
				hid_input, &loc_btn, 0))
			break;
	sc->nbuttons = i - 1;
	sc->sc_loc_btn = malloc(sizeof(struct hid_location)*sc->nbuttons, M_USBDEV, M_NOWAIT);
	if (!sc->sc_loc_btn)
		ATTACH_ERROR_RETURN;

#ifndef USBVERBOSE
	if (bootverbose)
#endif
		DEVICE_MSG(sc->sc_dev, ("%d buttons%s\n",
			sc->nbuttons, (sc->flags & UMS_Z? " and Z dir.":"")));

	for (i = 1; i <= sc->nbuttons; i++)
		hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
				hid_input, &sc->sc_loc_btn[i-1], 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input, &sc->sc_iid);
	sc->sc_ibuf = malloc(sc->sc_isize, M_USB, M_NOWAIT);
	if (!sc->sc_ibuf) {
		free(sc->sc_loc_btn, M_USB);
		ATTACH_ERROR_RETURN;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;
	free(desc, M_TEMP);

#ifdef USB_DEBUG
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
	sc->hw.buttons = 2;		/* XXX hw&mode values are bogus */
	sc->hw.iftype = MOUSE_IF_PS2;
	sc->hw.type = MOUSE_MOUSE;
	if (sc->flags & UMS_Z)
		sc->hw.model = MOUSE_MODEL_INTELLI;
	else
		sc->hw.model = MOUSE_MODEL_GENERIC;
	sc->hw.hwid = 0;
	sc->mode.protocol = MOUSE_PROTO_PS2;
	sc->mode.rate = -1;
	sc->mode.resolution = MOUSE_RES_DEFAULT;
	sc->mode.accelfactor = 1;
	sc->mode.level = 0;
	if (sc->flags & UMS_Z) {
		sc->mode.packetsize = MOUSE_INTELLI_PACKETSIZE;
		sc->mode.syncmask[0] = 0xc8;
	} else {
		sc->mode.packetsize = MOUSE_PS2_PACKETSIZE;
		sc->mode.syncmask[0] = 0xc0;
	}
	sc->mode.syncmask[1] = 0;

	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;

	sc->rsel.si_flags = 0;
	sc->rsel.si_pid = 0;
#endif

	ATTACH_SUCCESS_RETURN;
}


#if defined(__FreeBSD__)
static int
ums_detach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);
	char *devinfo = (char *) device_get_desc(self);

	if (devinfo) {
		device_set_desc(self, NULL);
		free(devinfo, M_USB);
	}
	free(sc->sc_loc_btn, M_USB);
	free(sc->sc_ibuf, M_USB);

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
	dz =  hid_get_data(ibuf, &sc->sc_loc_z);
	/* NWH Why are you modifying the button assignments here?
	 * That's the purpose of a high level mouse driver
	 */
	for (i = 1; i <= sc->nbuttons; i++)
		if (hid_get_data(ibuf, &sc->sc_loc_btn[i-1]))
			buttons |= (1 << (i-1));

#if defined(__NetBSD__)
	if (dx || dy || buttons != sc->sc_buttons) {
		DPRINTFN(10, ("ums_intr: x:%d y:%d z:%d buttons:0x%x\n",
			dx, dy, dz, buttons));
		sc->sc_buttons = buttons;
		if (sc->sc_wsmousedev)
			wsmouse_input(sc->sc_wsmousedev, buttons, dx, dy, dz);
#elif defined(__FreeBSD__)
	if (dx || dy || buttons != sc->status.button) {
		DPRINTFN(10, ("ums_intr: x:%d y:%d z:%d buttons:0x%x\n",
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

		sc->qbuf[sc->qhead] = MOUSE_PS2_SYNC;
		if (dx < 0)
			sc->qbuf[sc->qhead] |= MOUSE_PS2_XNEG;
		if (dx > 255 || dx < -255)
			sc->qbuf[sc->qhead] |= MOUSE_PS2_XOVERFLOW;
		if (dy < 0)
			sc->qbuf[sc->qhead] |= MOUSE_PS2_YNEG;
		if (dy > 255 || dy < -255)
			sc->qbuf[sc->qhead] |= MOUSE_PS2_YOVERFLOW;
		sc->qbuf[sc->qhead++] |= buttons;
		sc->qbuf[sc->qhead++] = dx;
		sc->qbuf[sc->qhead++] = dy;
		sc->qcount += 3;
		if (sc->flags & UMS_Z) {
			sc->qbuf[sc->qhead++] = dz;
			sc->qcount++;
		}
#ifdef USB_DEBUG
		if (sc->qhead > sizeof(sc->qbuf))
			DPRINTF(("Buffer overrun! %d %d\n", sc->qhead, sizeof(sc->qbuf)));
#endif
		if (sc->qhead >= sizeof(sc->qbuf))	/* wrap round at end of buffer */
			sc->qhead = 0;

		if (sc->state & UMS_ASLEEP)		/* someone waiting for data */
			wakeup(sc);
		selwakeup(&sc->rsel);			/* wake up any pending selects */
		sc->state &= ~UMS_SELECT;
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
#ifdef USB_DEBUG
	if (sizeof(sc->qbuf) % 4 || sizeof(sc->qbuf) % 3) {
		DPRINTF(("Buffer size not dividable by 3 or 4\n"));
		return ENXIO;
	}
#endif
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

#if defined(USBVERBOSE) && defined(__FreeBSD__)
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
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		return (0);
	}

	return (-1); /* NWH XXX Should we not return something ? */
}

#elif defined(__FreeBSD__)
static int
ums_open(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct ums_softc *sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));

	if (!sc) {
		DPRINTF(("sc not found at open"));
		return EINVAL;
	}

	return ums_enable(sc);
}

static int
ums_close(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct ums_softc *sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));

	if (!sc) {
		DPRINTF(("sc not found at close"));
		return EINVAL;
	}

	if (sc->sc_enabled)
		ums_disable(sc);
	return 0;
}

static int
ums_read(dev_t dev, struct uio *uio, int flag)
{
	struct ums_softc *sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	int s;
	char buf[sizeof(sc->qbuf)];
	int l = 0;
	int error;

	if (!sc || !sc->sc_enabled) {
		DPRINTF(("sc not found at read"));
		return EINVAL;
	}

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
		sc->state &= ~UMS_ASLEEP;
		if (error) {
			splx(s);
			return error;
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
	struct ums_softc *sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	int revents = 0;
	int s;

	if (!sc) {
		DPRINTF(("sc not found at poll"));
		return 0;	/* just to make sure */
	}

	s = splusb();
	if (events & (POLLIN | POLLRDNORM))
		if (sc->qcount) {
			revents = events & (POLLIN | POLLRDNORM);
		} else {
			sc->state |= UMS_SELECT;
			selrecord(p, &sc->rsel);
		}
	splx(s);

	return revents;
}
	
int
ums_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct ums_softc *sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	int error = 0;
	int s;

	if (!sc) {
		DPRINTF(("sc not found at ioctl"));
		return ENOENT;
	}

	switch(cmd) {
	case MOUSE_GETHWINFO:
		*(mousehw_t *)addr = sc->hw;
		break;
	case MOUSE_GETMODE:
		*(mousemode_t *)addr = sc->mode;
		break;
	case MOUSE_GETLEVEL:
		*(int *)addr = sc->mode.level;
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
CDEV_DRIVER_MODULE(ums, usb, ums_driver, ums_devclass,
			UMS_CDEV_MAJOR, ums_cdevsw, usb_driver_load, 0);
#endif

