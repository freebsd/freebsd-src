/*      $NetBSD: ukbd.c,v 1.22 1999/01/09 12:10:36 drochner Exp $        */
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
 * Information about USB keyboard can be found in the USB HID spec.
 */

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

#elif defined(__FreeBSD__)
#include <machine/clock.h>
#define delay(d)         DELAY(d)
#endif

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ukbddebug) printf x
#define DPRINTFN(n,x)	if (ukbddebug>(n)) printf x
int	ukbddebug = 1;
#else
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
/* 
 * Translate USB keycodes to US keyboard AT scancodes.
 * Scancodes >= 128 represent EXTENDED keycodes.
 */
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

#define MAXKEYS (NMOD+2*NKEYCODE)

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
#define REP_DELAY1 400
#define REP_DELAYN 100
	int sc_rawkbd;
	int sc_nrep;
	char sc_rep[MAXKEYS];
#endif

	int sc_polling;
	int sc_pollchar;
#endif
};

#define	UKBDUNIT(dev)	(minor(dev))
#define	UKBD_CHUNK	128	/* chunk size for read */
#define	UKBD_BSIZE	1020	/* buffer size */

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
int	ukbd_cnattach __P((void *v));
void	ukbd_rawrepeat __P((void *v));

const struct wskbd_accessops ukbd_accessops = {
	ukbd_enable,
	ukbd_set_leds,
	ukbd_ioctl,
#if 0
	ukbd_cnattach,
#endif
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

USB_DECLARE_DRIVER(ukbd);

USB_MATCH(ukbd)
{
	USB_MATCH_START(ukbd, uaa);
	usb_interface_descriptor_t *id;
	
	/* Check that this is a keyboard that speaks the boot protocol. */
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id ||
	    id->bInterfaceClass != UCLASS_HID || 
	    id->bInterfaceSubClass != USUBCLASS_BOOT ||
	    id->bInterfaceProtocol != UPROTO_BOOT_KEYBOARD)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

USB_ATTACH(ukbd)
{
	USB_ATTACH_START(ukbd, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status r;
	char devinfo[1024];
#if defined(__NetBSD__)
	struct wskbddev_attach_args a;
#endif
	
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

	DPRINTFN(10,("ukbd_attach: bLength=%d bDescriptorType=%d "
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

	if ((usbd_get_quirks(uaa->device)->uq_flags & UQ_NO_SET_PROTO) == 0) {
		r = usbd_set_protocol(iface, 0);
		DPRINTFN(5, ("ukbd_attach: protocol set\n"));
		if (r != USBD_NORMAL_COMPLETION) {
			printf("%s: set protocol failed\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
	}

	/* Ignore if SETIDLE fails since it is not crucial. */
	usbd_set_idle(iface, 0, 0);

	sc->sc_ep_addr = ed->bEndpointAddress;
	sc->sc_disconnected = 0;

#if defined(__NetBSD__)
	a.console = 0;

	a.keymap = &ukbd_keymapdata;

	a.accessops = &ukbd_accessops;
	a.accesscookie = sc;

	/* Flash the leds; no real purpose, just shows we're alive. */
	ukbd_set_leds(sc, WSKBD_LED_SCROLL | WSKBD_LED_NUM | WSKBD_LED_CAPS);
	usbd_delay_ms(uaa->device, 300);
	ukbd_set_leds(sc, 0);

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);

#elif defined(__FreeBSD__)
	/* XXX why waste CPU in delay() ? */
	/* It's alive!  IT'S ALIVE!  Do a little song and dance. */
	ukbd_set_leds(sc, NUM_LOCK);
	delay(15000);
	ukbd_set_leds(sc, CAPS_LOCK);
	delay(20000);
	ukbd_set_leds(sc, SCROLL_LOCK);
	delay(30000);
	ukbd_set_leds(sc, CAPS_LOCK);
	delay(50000);
	ukbd_set_leds(sc, NUM_LOCK);

	ukbd_enable(sc, 1);
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__FreeBSD__)
int
ukbd_detach(device_t self)
{
	struct ukbd_softc *sc = device_get_softc(self);
	char *devinfo = (char *) device_get_desc(self);

	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	if (sc->sc_enabled)
		return (ENXIO);

	if (devinfo) {
		device_set_desc(self, NULL);
		free(devinfo, M_USB);
	}

	return (0);
}
#endif

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
			return (EBUSY);
		
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
	int ibuf[MAXKEYS];	/* chars events */
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

	if (nkeys == 0)
		return;

#if defined(__NetBSD__)
	if (sc->sc_polling) {
		DPRINTFN(1,("ukbd_intr: pollchar = 0x%02x\n", ibuf[0]));
		if (nkeys > 0)
			sc->sc_pollchar = ibuf[0]; /* XXX lost keys? */
		return;
	}
#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd) {
		char cbuf[MAXKEYS * 2];
		int npress;

		for (npress = i = j = 0; i < nkeys; i++, j++) {
			c = ibuf[i];
			if (c & 0x80)
				cbuf[j++] = 0xe0;
			cbuf[j] = c & 0x7f;
			if (c & RELEASE)
				cbuf[j] |= 0x80;
			else {
				/* remember keys for autorepeat */
				if (c & 0x80)
					sc->sc_rep[npress++] = 0xe0;
				sc->sc_rep[npress++] = c & 0x7f;
			}
		}
		wskbd_rawinput(sc->sc_wskbddev, cbuf, j);
		untimeout(ukbd_rawrepeat, sc);
		if (npress != 0) {
			sc->sc_nrep = npress;
			timeout(ukbd_rawrepeat, sc, hz * REP_DELAY1 / 1000);
		}
		return;
	}
#endif

	for (i = 0; i < nkeys; i++) {
		c = ibuf[i];
		wskbd_input(sc->sc_wskbddev, 
		    c & RELEASE ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN,
		    c & 0xff);
	}
#elif defined(__FreeBSD__)
	/* XXX shouldn't the keys be used? */
	for (i = 0; i < nkeys; i++) {
		c = ibuf[i];
		printf("%c (%d) %s ", 
		       ((c&0xff) < 32 || (c&0xff) > 126? '.':(c&0xff)), c,
		       (c&RELEASE? "released":"pressed"));
		if (ud->modifiers)
			printf("mod = 0x%04x ", ud->modifiers);
                for (i = 0; i < NKEYCODE; i++)
			if (ud->keycode[i])
				printf("%d ", ud->keycode[i]);
		printf("\n");
	}
#endif
}

void
ukbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct ukbd_softc *sc = v;
	u_int8_t res;

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
#elif defined(__FreeBSD__)
	res = leds;
#endif
	usbd_set_report_async(sc->sc_iface, UHID_OUTPUT_REPORT, 0, &res, 1);
}

#if defined(__NetBSD__)

#ifdef WSDISPLAY_COMPAT_RAWKBD
void
ukbd_rawrepeat(v)
	void *v;
{
	struct ukbd_softc *sc = v;

	wskbd_rawinput(sc->sc_wskbddev, sc->sc_rep, sc->sc_nrep);
	timeout(ukbd_rawrepeat, sc, hz * REP_DELAYN / 1000);
}
#endif

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
		*(int *)data = WSKBD_TYPE_USB;
		return (0);
	case WSKBDIO_SETLEDS:
		ukbd_set_leds(v, *(int *)data);
		return (0);
	case WSKBDIO_GETLEDS:
		*(int *)data = sc->sc_leds;
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		DPRINTF(("ukbd_ioctl: set raw = %d\n", *(int *)data));
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		untimeout(ukbd_rawrepeat, sc);
		return (0);
#endif
	}
	return (-1);
}

/* Console interface. */
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

	DPRINTFN(2,("ukbd_cnpollc: sc=%p on=%d\n", v, on));

	usbd_set_polling(sc->sc_iface, on);
}

int
ukbd_cnattach(v)
	void *v;
{
	struct ukbd_softc *sc = v;

	DPRINTF(("ukbd_cnattach: sc=%p\n", sc));
	wskbd_cnattach(&ukbd_consops, sc, &ukbd_keymapdata);
	return (0);
}

#endif /* NetBSD */

#if defined(__FreeBSD__)
DRIVER_MODULE(ukbd, uhub, ukbd_driver, ukbd_devclass, usbd_driver_load, 0);
#endif
