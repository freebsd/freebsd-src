/*	FreeBSD $Id$ */

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
#if defined(__NetBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/clock.h>
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
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wskbdmap_mfii.h>

#include "opt_pckbd_layout.h"
#include "opt_wsdisplay_compat.h"
#endif

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ukbddebug) printf x
#define DPRINTFN(n,x)	if (ukbddebug>(n)) printf x
int	ukbddebug = 0;
#elif defined(__FreeBSD__)
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UPROTO_BOOT_KEYBOARD 1

#define NKEYCODE 6

#define NUM_LOCK 0x01
#define CAPS_LOCK 0x02
#define SCROLL_LOCK 0x04

struct ukbd_data {
	u_int8_t	modifiers;
#define MOD_CONTROL_L	0x01
#define MOD_CONTROL_R	0x10
#define MOD_SHIFT_L	0x02
#define MOD_SHIFT_R	0x20
#define MOD_ALT_L	0x04
#define MOD_ALT_R	0x40
#define MOD_WIN_L	0x08
#define MOD_WIN_R	0x80
	u_int8_t	reserved;
	u_int8_t	keycode[NKEYCODE];
};

#define PRESS 0
#define RELEASE 0x100

#define NMOD 6
static struct {
	int mask, key;
} ukbd_mods[NMOD] = {
	{ MOD_CONTROL_L, 29 },
	{ MOD_CONTROL_R, 58 },
	{ MOD_SHIFT_L,   42 },
	{ MOD_SHIFT_R,   54 },
	{ MOD_ALT_L,     56 },
	{ MOD_ALT_R,    184 },
};

#define NN 0			/* no translation */
/* Translate USB keycodes to US keyboard AT scancodes. */
static u_int8_t ukbd_trtab[256] = {
	   0,   0,   0,   0,  30,  48,  46,  32, /* 00 - 07 */
	  18,  33,  34,  35,  23,  36,  37,  38, /* 08 - 0F */
	  50,  49,  24,  25,  16,  19,  31,  20, /* 10 - 17 */
	  22,  47,  17,  45,  21,  44,   2,   3, /* 18 - 1F */
	   4,   5,   6,   7,   8,   9,  10,  11, /* 20 - 27 */
	  28,   1,  14,  15,  57,  12,  13,  26, /* 28 - 2F */
	  27,  43,  NN,  39,  40,  41,  51,  52, /* 30 - 37 */
	  53,  58,  59,  60,  61,  62,  63,  64, /* 38 - 3F */
	  65,  66,  67,  68,  87,  88, 170,  70, /* 40 - 47 */
	 127, 210, 199, 201, 211, 207, 209, 205, /* 48 - 4F */
	 203, 208, 200,  69, 181,  55,  74,  78, /* 50 - 57 */
	 156,  79,  80,  81,  75,  76,  77,  71, /* 58 - 5F */
          72,  73,  82,  83,  NN,  NN,  NN,  NN, /* 60 - 67 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 68 - 6F */
          NN,  NN,  NN,  NN,  NN,  NN, 221,  NN, /* 70 - 77 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 78 - 7F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 80 - 87 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 88 - 8F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 90 - 97 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* 98 - 9F */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* A0 - A7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* A8 - AF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* B0 - B7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* B8 - BF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* C0 - C7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* C8 - CF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* D0 - D7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* D8 - DF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* E0 - E7 */
          NN,  NN,  NN, 219,  NN,  NN,  NN, 220, /* E8 - EF */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* F0 - F7 */
          NN,  NN,  NN,  NN,  NN,  NN,  NN,  NN, /* F8 - FF */
};

#define KEY_ERROR 0x01

struct ukbd_softc {
	bdevice		sc_dev;		/* base device */
	usbd_interface_handle sc_iface;	/* interface */
	usbd_pipe_handle sc_intrpipe;	/* interrupt pipe */
	int sc_ep_addr;

	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;

	char sc_enabled;
	char sc_disconnected;		/* device is gone */

	int sc_leds;
#if defined(__NetBSD__)
	struct device *sc_wskbddev;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;
#endif
#endif

	int sc_polling;
	int sc_pollchar;
};

#define	UKBDUNIT(dev)	(minor(dev))
#define	UKBD_CHUNK	128	/* chunk size for read */
#define	UKBD_BSIZE	1020	/* buffer size */

#if defined(__NetBSD__)
int	ukbd_match __P((struct device *, struct cfdata *, void *));
void	ukbd_attach __P((struct device *, struct device *, void *));
#elif defined(__FreeBSD__)
static device_probe_t ukbd_match;
static device_attach_t ukbd_attach;
static device_detach_t ukbd_detach;
#endif

void	ukbd_cngetc __P((void *, u_int *, int *));
void	ukbd_cnpollc __P((void *, int));

#if defined(__NetBSD__)
const struct wskbd_consops ukbd_consops = {
	ukbd_cngetc,
	ukbd_cnpollc,
};
#endif

void	ukbd_intr __P((usbd_request_handle, usbd_private_handle, usbd_status));
void	ukbd_disco __P((void *));

int	ukbd_enable __P((void *, int));
void	ukbd_set_leds __P((void *, int));
#if defined(__NetBSD__)
int	ukbd_ioctl __P((void *, u_long, caddr_t, int, struct proc *));

const struct wskbd_accessops ukbd_accessops = {
	ukbd_enable,
	ukbd_set_leds,
	ukbd_ioctl,
};

const struct wskbd_mapdata ukbd_keymapdata = {
	pckbd_keydesctab,
#ifdef PCKBD_LAYOUT
	PCKBD_LAYOUT,
#else
	KB_US,
#endif
};
#endif

#if defined(__NetBSD__)
extern struct cfdriver ukbd_cd;

struct cfattach ukbd_ca = {
	sizeof(struct ukbd_softc), ukbd_match, ukbd_attach
};
#elif defined(__FreeBSD__)
static devclass_t ukbd_devclass;

static device_method_t ukbd_methods[] = {
	DEVMETHOD(device_probe, ukbd_match),
	DEVMETHOD(device_attach, ukbd_attach),
	DEVMETHOD(device_detach, ukbd_detach),
	{0,0}
};

static driver_t ukbd_driver = {
	"ukbd",
	ukbd_methods,
	DRIVER_TYPE_MISC,
	sizeof(struct ukbd_softc)
};
#endif

#if defined(__NetBSD__)
int
ukbd_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct usb_attach_arg *uaa = (struct usb_attach_arg *)aux;
#elif defined(__FreeBSD__)
static int
ukbd_match(device_t device)
{
	struct usb_attach_arg *uaa = device_get_ivars(device);
#endif
	usb_interface_descriptor_t *id;
	
	/* Check that this is a keyboard that speaks the boot protocol. */
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id->bInterfaceClass != UCLASS_HID || 
	    id->bInterfaceSubClass != USUBCLASS_BOOT ||
	    id->bInterfaceProtocol != UPROTO_BOOT_KEYBOARD)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

#if defined(__NetBSD__)
void
ukbd_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ukbd_softc *sc = (struct ukbd_softc *)self;
	struct usb_attach_arg *uaa = aux;
#elif defined(__FreeBSD__)
static int
ukbd_attach(device_t self)
{
	struct ukbd_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
#endif
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status r;
	char devinfo[1024];
#if defined(__NetBSD__)
	struct wskbddev_attach_args a;
#else
	int i;
#endif
	
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

	DPRINTFN(10,("ukbd_attach: \
bLength=%d bDescriptorType=%d bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d bInterval=%d\n",
	       ed->bLength, ed->bDescriptorType, ed->bEndpointAddress & UE_ADDR,
	       ed->bEndpointAddress & UE_IN ? "in" : "out",
	       ed->bmAttributes & UE_XFERTYPE,
	       UGETW(ed->wMaxPacketSize), ed->bInterval));

	if ((ed->bEndpointAddress & UE_IN) != UE_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		DEVICE_ERROR(sc->sc_dev, ("unexpected endpoint\n"));
		ATTACH_ERROR_RETURN;
	}

	if ((usbd_get_quirks(uaa->device)->uq_flags & UQ_NO_SET_PROTO) == 0) {
		r = usbd_set_protocol(iface, 0);
		DPRINTFN(5, ("ukbd_attach: protocol set\n"));
		if (r != USBD_NORMAL_COMPLETION) {
			DEVICE_ERROR(sc->sc_dev, ("set protocol failed\n"));
			ATTACH_ERROR_RETURN;
		}
	}
	/* Ignore if SETIDLE fails since it is not crucial. */
	usbd_set_idle(iface, 0, 0);

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;

#if defined(__NetBSD__)
	a.console = 0;	/* XXX */

	a.keymap = &ukbd_keymapdata;

	a.accessops = &ukbd_accessops;
	a.accesscookie = sc;

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);

#elif defined(__FreeBSD__)
	/* it's alive! IT'S ALIVE! */
	ukbd_set_leds(sc, NUM_LOCK);
	DELAY(15000);
	ukbd_set_leds(sc, CAPS_LOCK);
	DELAY(20000);
	ukbd_set_leds(sc, SCROLL_LOCK);
	DELAY(30000);
	ukbd_set_leds(sc, CAPS_LOCK);
	DELAY(50000);
	ukbd_set_leds(sc, NUM_LOCK);

	ukbd_enable(sc, 1);
#endif

	ATTACH_SUCCESS_RETURN;
}


int
ukbd_detach(device_t self)
{
	struct ukbd_softc *sc = device_get_softc(self);
	char *devinfo = (char *) device_get_desc(self);

	if (sc->sc_enabled)
		return ENXIO;

	if (devinfo) {
		device_set_desc(self, NULL);
		free(devinfo, M_USB);
	}

	return 0;
}


void
ukbd_disco(p)
	void *p;
{
	struct ukbd_softc *sc = p;

	DPRINTF(("ukbd_disco: sc=%p\n", sc));
	usbd_abort_pipe(sc->sc_intrpipe);
	sc->sc_disconnected = 1;
}

int
ukbd_enable(v, on)
	void *v;
	int on;
{
	struct ukbd_softc *sc = v;
	usbd_status r;

	if (on) {
		/* Set up interrupt pipe. */
		if (sc->sc_enabled)
			return EBUSY;
		
		sc->sc_enabled = 1;
		r = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr, 
					USBD_SHORT_XFER_OK,
					&sc->sc_intrpipe, sc, &sc->sc_ndata, 
					sizeof(sc->sc_ndata), ukbd_intr);
		if (r != USBD_NORMAL_COMPLETION)
			return (EIO);
		usbd_set_disco(sc->sc_intrpipe, ukbd_disco, sc);
	} else {
		/* Disable interrupts. */
		usbd_abort_pipe(sc->sc_intrpipe);
		usbd_close_pipe(sc->sc_intrpipe);

		sc->sc_enabled = 0;
	}

	return (0);
}

void
ukbd_intr(reqh, addr, status)
	usbd_request_handle reqh;
	usbd_private_handle addr;
	usbd_status status;
{
	struct ukbd_softc *sc = addr;
	struct ukbd_data *ud = &sc->sc_ndata;
	int mod, omod;
	int ibuf[NMOD+2*NKEYCODE];	/* chars events */
	int nkeys, i, j;
	int key, c;
#define ADDKEY(c) ibuf[nkeys++] = (c)

	DPRINTFN(5, ("ukbd_intr: status=%d\n", status));
	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ukbd_intr: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_intrpipe);
		return;
	}

	DPRINTFN(5, ("          mod=0x%02x key0=0x%02x key1=0x%02x\n",
		     ud->modifiers, ud->keycode[0], ud->keycode[1]));

	if (ud->keycode[0] == KEY_ERROR)
		return;		/* ignore  */
	nkeys = 0;
	mod = ud->modifiers;
	omod = sc->sc_odata.modifiers;
	if (mod != omod)
		for (i = 0; i < NMOD; i++)
			if (( mod & ukbd_mods[i].mask) != 
			    (omod & ukbd_mods[i].mask))
				ADDKEY(ukbd_mods[i].key | 
				       (mod & ukbd_mods[i].mask 
					  ? PRESS : RELEASE));
	if (memcmp(ud->keycode, sc->sc_odata.keycode, NKEYCODE) != 0) {
		/* Check for released keys. */
		for (i = 0; i < NKEYCODE; i++) {
			key = sc->sc_odata.keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < NKEYCODE; j++)
				if (key == ud->keycode[j])
					goto rfound;
			c = ukbd_trtab[key];
			if (c)
				ADDKEY(c | RELEASE);
		rfound:
			;
		}
		
		/* Check for pressed keys. */
		for (i = 0; i < NKEYCODE; i++) {
			key = ud->keycode[i];
			if (key == 0)
				continue;
			for (j = 0; j < NKEYCODE; j++)
				if (key == sc->sc_odata.keycode[j])
					goto pfound;
			c = ukbd_trtab[key];
			DPRINTFN(2,("ukbd_intr: press key=0x%02x -> 0x%02x\n",
				    key, c));
			if (c)
				ADDKEY(c | PRESS);
		pfound:
			;
		}
	}
	sc->sc_odata = *ud;

	if (sc->sc_polling) {
		if (nkeys > 0)
			sc->sc_pollchar = ibuf[0]; /* XXX lost keys? */
		return;
	}
	for (i = 0; i < nkeys; i++) {
		c = ibuf[i];
#if defined(__NetBSD__)
		wskbd_input(sc->sc_wskbddev, 
		    c & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN,
		    c & 0xff);
#elif defined(__FreeBSD__)
		printf("%c (%d) %s\n", ((c&0xff) < 32 || (c&0xff) > 126? '.':(c&0xff)), c,
			(c&RELEASE? "released":"pressed"));
		if (ud->modifiers)
			printf("0x%04x\n", ud->modifiers);
                for (i = 0; i < NKEYCODE; i++)
			if (ud->keycode[i])
				printf("%d ", ud->keycode[i]);
		printf("\n");
#endif
	}
}

void
ukbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct ukbd_softc *sc = v;
	u_int8_t res = leds;

	DPRINTF(("ukbd_set_leds: sc=%p leds=%d\n", sc, leds));

	sc->sc_leds = leds;
#if defined(__NetBSD__)
	res = 0;
	if (leds & WSKBD_LED_SCROLL)
		res |= SCROLL_LOCK;
	if (leds & WSKBD_LED_NUM)
		res |= NUM_LOCK;
	if (leds & WSKBD_LED_CAPS)
		res |= CAPS_LOCK;
#endif
	usbd_set_report_async(sc->sc_iface, UHID_OUTPUT_REPORT, 0, &res, 1);
}

#if defined(__NetBSD__)
int
ukbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct ukbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_PC_XT;
		return 0;
	case WSKBDIO_SETLEDS:
		ukbd_set_leds(v, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		return (0);
#endif
	}
	return -1;
}

/* Console interface. */
/* XXX does not work. */
void
ukbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	struct ukbd_softc *sc = v;
	usbd_lock_token s;
	int c;

	DPRINTFN(1,("ukbd_cngetc: enter\n"));
	s = usbd_lock();
	sc->sc_polling = 1;
	sc->sc_pollchar = -1;
	while(sc->sc_pollchar == -1)
		usbd_dopoll(sc->sc_iface);
	sc->sc_polling = 0;
	c = sc->sc_pollchar;
	*type = c & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*data = c & 0xff;
	usbd_unlock(s);
	DPRINTFN(1,("ukbd_cngetc: return 0x%02x\n", c));
}

void
ukbd_cnpollc(v, on)
	void *v;
        int on;
{
	struct ukbd_softc *sc = v;

	DPRINTFN(1,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	usbd_set_polling(sc->sc_iface, on);
}
#endif

#if defined(__FreeBSD__)
DRIVER_MODULE(ukbd, usb, ukbd_driver, ukbd_devclass, usb_driver_load, 0);
#endif
