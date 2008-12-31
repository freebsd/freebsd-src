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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/usb/ums.c,v 1.96.2.5.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"
#include <dev/usb/usb_quirks.h>
#include <dev/usb/hid.h>

#include <sys/mouse.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (umsdebug) printf x
#define DPRINTFN(n,x)	if (umsdebug>(n)) printf x
int	umsdebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ums, CTLFLAG_RW, 0, "USB ums");
SYSCTL_INT(_hw_usb_ums, OID_AUTO, debug, CTLFLAG_RW,
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
	struct hid_location sc_loc_x, sc_loc_y, sc_loc_z, sc_loc_t, sc_loc_w;
	struct hid_location *sc_loc_btn;

	struct callout callout_handle;	/* for spurious button ups */

	int sc_enabled;
	int sc_disconnected;	/* device is gone */

	int flags;		/* device configuration */
#define UMS_Z		0x01	/* z direction available */
#define UMS_SPUR_BUT_UP	0x02	/* spurious button up events */
#define UMS_T		0x04	/* aa direction available (tilt) */
#define UMS_REVZ	0x08	/* Z-axis is reversed */
	int nbuttons;
#define MAX_BUTTONS	31	/* chosen because sc_buttons is int */

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

	struct cdev *dev;		/* specfs */
};

#define MOUSE_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)
#define MOUSE_FLAGS (HIO_RELATIVE)

static void ums_intr(usbd_xfer_handle xfer,
			  usbd_private_handle priv, usbd_status status);

static void ums_add_to_queue(struct ums_softc *sc,
				int dx, int dy, int dz, int dt, int buttons);
static void ums_add_to_queue_timeout(void *priv);

static int  ums_enable(void *);
static void ums_disable(void *);

static d_open_t  ums_open;
static d_close_t ums_close;
static d_read_t  ums_read;
static d_ioctl_t ums_ioctl;
static d_poll_t  ums_poll;


static struct cdevsw ums_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ums_open,
	.d_close =	ums_close,
	.d_read =	ums_read,
	.d_ioctl =	ums_ioctl,
	.d_poll =	ums_poll,
	.d_name =	"ums",
};

static device_probe_t ums_match;
static device_attach_t ums_attach;
static device_detach_t ums_detach;

static device_method_t ums_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ums_match),
	DEVMETHOD(device_attach,	ums_attach),
	DEVMETHOD(device_detach,	ums_detach),

	{ 0, 0 }
};

static driver_t ums_driver = {
	"ums",
	ums_methods,
	sizeof(struct ums_softc)
};

static devclass_t ums_devclass;

static int
ums_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
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
	else if (id->bInterfaceClass == UICLASS_HID &&
	    id->bInterfaceSubClass == UISUBCLASS_BOOT &&
	    id->bInterfaceProtocol == UIPROTO_MOUSE)
		ret = UMATCH_IFACECLASS;
	else
		ret = UMATCH_NONE;

	free(desc, M_TEMP);
	return (ret);
}

static int
ums_attach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int size;
	void *desc;
	usbd_status err;
	u_int32_t flags;
	int i, wheel;
	struct hid_location loc_btn;

	sc->sc_disconnected = 1;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);
	sc->sc_dev = self;
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (!ed) {
		printf("%s: could not read endpoint descriptor\n",
		       device_get_nameunit(sc->sc_dev));
		return ENXIO;
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
		       device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	err = usbd_read_report_desc(uaa->iface, &desc, &size, M_TEMP);
	if (err)
		return ENXIO;

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
		       hid_input, &sc->sc_loc_x, &flags)) {
		printf("%s: mouse has no X report\n", device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("%s: X report 0x%04x not supported\n",
		       device_get_nameunit(sc->sc_dev), flags);
		return ENXIO;
	}

	if (!hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
		       hid_input, &sc->sc_loc_y, &flags)) {
		printf("%s: mouse has no Y report\n", device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}
	if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
		printf("%s: Y report 0x%04x not supported\n",
		       device_get_nameunit(sc->sc_dev), flags);
		return ENXIO;
	}

	/* Try the wheel first as the Z activator since it's tradition. */
	wheel = hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP,
						  HUG_WHEEL),
			    hid_input, &sc->sc_loc_z, &flags) ||
		hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP,
						  HUG_TWHEEL),
			    hid_input, &sc->sc_loc_z, &flags);

	if (wheel) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			printf("\n%s: Wheel report 0x%04x not supported\n",
			       device_get_nameunit(sc->sc_dev), flags);
			sc->sc_loc_z.size = 0;	/* Bad Z coord, ignore it */
		} else {
			sc->flags |= UMS_Z;
			if (usbd_get_quirks(uaa->device)->uq_flags &
			    UQ_MS_REVZ) {
				/* Some wheels need the Z axis reversed. */
				sc->flags |= UMS_REVZ;
			}

		}
		/*
		 * We might have both a wheel and Z direction, if so put
		 * put the Z on the W coordinate.
		 */
		if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP,
						      HUG_Z),
				hid_input, &sc->sc_loc_w, &flags)) {
			if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
				printf("\n%s: Z report 0x%04x not supported\n",
				       device_get_nameunit(sc->sc_dev), flags);
				sc->sc_loc_w.size = 0;	/* Bad Z, ignore */
			}
		}
	} else if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP,
						     HUG_Z),
			       hid_input, &sc->sc_loc_z, &flags)) {
		if ((flags & MOUSE_FLAGS_MASK) != MOUSE_FLAGS) {
			printf("\n%s: Z report 0x%04x not supported\n",
			       device_get_nameunit(sc->sc_dev), flags);
			sc->sc_loc_z.size = 0;	/* Bad Z coord, ignore it */
		} else {
			sc->flags |= UMS_Z;
		}
	}

	/*
	 * The Microsoft Wireless Intellimouse 2.0 reports it's wheel
	 * using 0x0048 (i've called it HUG_TWHEEL) and seems to expect
	 * you to know that the byte after the wheel is the tilt axis.
	 * There are no other HID axis descriptors other than X,Y and 
	 * TWHEEL
	 */
	if (hid_locate(desc, size, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_TWHEEL),
			hid_input, &sc->sc_loc_t, &flags)) {
			sc->sc_loc_t.pos = sc->sc_loc_t.pos + 8;
			sc->flags |= UMS_T;
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
		printf("%s: no memory\n", device_get_nameunit(sc->sc_dev));
		return ENXIO;
	}

	printf("%s: %d buttons%s%s.\n", device_get_nameunit(sc->sc_dev),
	       sc->nbuttons, sc->flags & UMS_Z? " and Z dir" : "", 
	       sc->flags & UMS_T?" and a TILT dir": "");

	for (i = 1; i <= sc->nbuttons; i++)
		hid_locate(desc, size, HID_USAGE2(HUP_BUTTON, i),
				hid_input, &sc->sc_loc_btn[i-1], 0);

	sc->sc_isize = hid_report_size(desc, size, hid_input, &sc->sc_iid);

	/*
	 * The Microsoft Wireless Notebook Optical Mouse seems to be in worse
	 * shape than the Wireless Intellimouse 2.0, as its X, Y, wheel, and
	 * all of its other button positions are all off. It also reports that
	 * it has two addional buttons and a tilt wheel.
	 */
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_MS_BAD_CLASS) {
		sc->flags = UMS_Z;
		sc->flags |= UMS_SPUR_BUT_UP;
		sc->nbuttons = 3;
		sc->sc_isize = 5;
		sc->sc_iid = 0;
		/* 1st byte of descriptor report contains garbage */
		sc->sc_loc_x.pos = 16;
		sc->sc_loc_y.pos = 24;
		sc->sc_loc_z.pos = 32;
		sc->sc_loc_btn[0].pos = 8;
		sc->sc_loc_btn[1].pos = 9;
		sc->sc_loc_btn[2].pos = 10;
	}

	/*
	 * The Microsoft Wireless Notebook Optical Mouse 3000 Model 1049 has
	 * five Report IDs: 19 23 24 17 18 (in the order they appear in report
	 * descriptor), it seems that report id 17 contains the necessary
	 * mouse information(3-buttons,X,Y,wheel) so we specify it manually.
	 */
	if (uaa->vendor == USB_VENDOR_MICROSOFT &&
	    uaa->product == USB_PRODUCT_MICROSOFT_WLNOTEBOOK3) {
		sc->flags = UMS_Z;
		sc->nbuttons = 3;
		sc->sc_isize = 5;
		sc->sc_iid = 17;
		sc->sc_loc_x.pos = 8;
		sc->sc_loc_y.pos = 16;
		sc->sc_loc_z.pos = 24;
		sc->sc_loc_btn[0].pos = 0;
		sc->sc_loc_btn[1].pos = 1;
		sc->sc_loc_btn[2].pos = 2;
	}

	sc->sc_ibuf = malloc(sc->sc_isize, M_USB, M_NOWAIT);
	if (!sc->sc_ibuf) {
		printf("%s: no memory\n", device_get_nameunit(sc->sc_dev));
		free(sc->sc_loc_btn, M_USB);
		return ENXIO;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;
	free(desc, M_TEMP);

#ifdef USB_DEBUG
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

	sc->dev = make_dev(&ums_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "ums%d", device_get_unit(self));

	callout_init(&sc->callout_handle, 0);
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_SPUR_BUT_UP) {
		DPRINTF(("%s: Spurious button up events\n",
			device_get_nameunit(sc->sc_dev)));
		sc->flags |= UMS_SPUR_BUT_UP;
	}

	return 0;
}


static int
ums_detach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);

	if (sc->sc_enabled)
		ums_disable(sc);

	DPRINTF(("%s: disconnected\n", device_get_nameunit(self)));

	free(sc->sc_loc_btn, M_USB);
	free(sc->sc_ibuf, M_USB);

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
		selwakeuppri(&sc->rsel, PZERO);
	}

	destroy_dev(sc->dev);

	return 0;
}

void
ums_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct ums_softc *sc = addr;
	u_char *ibuf;
	int dx, dy, dz, dw, dt;
	int buttons = 0;
	int i;

#define UMS_BUT(i) ((i) < 3 ? (((i) + 2) % 3) : (i))

	DPRINTFN(5, ("ums_intr: sc=%p status=%d\n", sc, status));
	DPRINTFN(5, ("ums_intr: data ="));
	for (i = 0; i < sc->sc_isize; i++)
		DPRINTFN(5, (" %02x", sc->sc_ibuf[i]));
	DPRINTFN(5, ("\n"));

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ums_intr: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sc->sc_intrpipe);
		if(status != USBD_IOERROR)
			return;
	}

	ibuf = sc->sc_ibuf;
	/*
	 * The M$ Wireless Intellimouse 2.0 sends 1 extra leading byte of
	 * data compared to most USB mice. This byte frequently switches
	 * from 0x01 (usual state) to 0x02. I assume it is to allow
	 * extra, non-standard, reporting (say battery-life). However
	 * at the same time it generates a left-click message on the button
	 * byte which causes spurious left-click's where there shouldn't be.
	 * This should sort that.
	 * Currently it's the only user of UMS_T so use it as an identifier.
	 * We probably should switch to some more official quirk.
	 *
	 * UPDATE: This problem affects the M$ Wireless Notebook Optical Mouse,
	 * too. However, the leading byte for this mouse is normally 0x11,
	 * and the phantom mouse click occurs when its 0x14.
	 */
	if (sc->flags & UMS_T) {
		if (sc->sc_iid) {
			if (*ibuf++ == 0x02)
				return;
		}
	} else if (sc->flags & UMS_SPUR_BUT_UP) {
		DPRINTFN(5, ("ums_intr: #### ibuf[0] =3D %d ####\n", *ibuf));
		if (*ibuf == 0x14 || *ibuf == 0x15)
			return;
	} else {
		if (sc->sc_iid) {
			if (*ibuf++ != sc->sc_iid)
				return;
		}
	}

	dx =  hid_get_data(ibuf, &sc->sc_loc_x);
	dy = -hid_get_data(ibuf, &sc->sc_loc_y);
	dz = -hid_get_data(ibuf, &sc->sc_loc_z);
	dw =  hid_get_data(ibuf, &sc->sc_loc_w);
	if (sc->flags & UMS_REVZ)
		dz = -dz;
	if (sc->flags & UMS_T)
		dt = -hid_get_data(ibuf, &sc->sc_loc_t);
	else
		dt = 0;
	for (i = 0; i < sc->nbuttons; i++)
		if (hid_get_data(ibuf, &sc->sc_loc_btn[i]))
			buttons |= (1 << UMS_BUT(i));

	if (dx || dy || dz || dt || dw || (sc->flags & UMS_Z)
	    || buttons != sc->status.button) {
		DPRINTFN(5, ("ums_intr: x:%d y:%d z:%d w:%d t:%d buttons:0x%x\n",
			dx, dy, dz, dw, dt, buttons));

		sc->status.button = buttons;
		sc->status.dx += dx;
		sc->status.dy += dy;
		sc->status.dz += dz;
		/* sc->status.dt += dt; */ /* no way to export this yet */
		/* sc->status.dw += dw; */ /* idem */
		
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
		    dx == 0 && dy == 0 && dz == 0 && dt == 0 && buttons == 0) {
			callout_reset(&sc->callout_handle, MS_TO_TICKS(50),
			    ums_add_to_queue_timeout, (void *) sc);
		} else {
			callout_stop(&sc->callout_handle);
			ums_add_to_queue(sc, dx, dy, dz, dt, buttons);
		}
	}
}

static void
ums_add_to_queue_timeout(void *priv)
{
	struct ums_softc *sc = priv;
	int s;

	s = splusb();
	ums_add_to_queue(sc, 0, 0, 0, 0, 0);
	splx(s);
}

static void
ums_add_to_queue(struct ums_softc *sc, int dx, int dy, int dz, int dt, int buttons)
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
	if (dt >  126)		dt =  126;
        if (dt < -128)		dt = -128;

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
		selwakeuppri(&sc->rsel, PZERO);
	}
}
static int
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
	sc->status.dx = sc->status.dy = sc->status.dz /* = sc->status.dt */ = 0;

	callout_handle_init((struct callout_handle *)&sc->callout_handle);

	/*
	 * Force the report (non-boot) protocol.
	 *
	 * Mice without boot protocol support may choose not to implement
	 * Set_Protocol at all; do not check for error.
	 */
	usbd_set_protocol(sc->sc_iface, 1);

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

static void
ums_disable(priv)
	void *priv;
{
	struct ums_softc *sc = priv;

	callout_stop(&sc->callout_handle);

	/* Disable interrupts. */
	usbd_abort_pipe(sc->sc_intrpipe);
	usbd_close_pipe(sc->sc_intrpipe);

	sc->sc_enabled = 0;

	if (sc->qcount != 0)
		DPRINTF(("Discarded %d bytes in queue\n", sc->qcount));
}

static int
ums_open(struct cdev *dev, int flag, int fmt, struct thread *p)
{
	struct ums_softc *sc;

	sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	return ums_enable(sc);
}

static int
ums_close(struct cdev *dev, int flag, int fmt, struct thread *p)
{
	struct ums_softc *sc;

	sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	if (!sc)
		return 0;

	if (sc->sc_enabled)
		ums_disable(sc);

	return 0;
}

static int
ums_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct ums_softc *sc;
	int s;
	char buf[sizeof(sc->qbuf)];
	int l = 0;
	int error;

	sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
	s = splusb();
	if (!sc) {
		splx(s);
		return EIO;
	}

	while (sc->qcount == 0 )  {
		if (flag & O_NONBLOCK) {		/* non-blocking I/O */
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

static int
ums_poll(struct cdev *dev, int events, struct thread *p)
{
	struct ums_softc *sc;
	int revents = 0;
	int s;

	sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
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
ums_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *p)
{
	struct ums_softc *sc;
	int error = 0;
	int s;
	mousemode_t mode;

	sc = devclass_get_softc(ums_devclass, UMSUNIT(dev));
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
		sc->status.dx = sc->status.dy
		    = sc->status.dz = /* sc->status.dt = */ 0;
		splx(s);

		if (status->dx || status->dy || status->dz /* || status->dt */)
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

MODULE_DEPEND(ums, usb, 1, 1, 1);
DRIVER_MODULE(ums, uhub, ums_driver, ums_devclass, usbd_driver_load, 0);
