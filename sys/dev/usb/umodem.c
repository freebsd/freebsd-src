/*	$NetBSD: umodem.c,v 1.5 1999/01/08 11:58:25 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/umodem.c,v 1.17.2.3 2000/07/02 11:43:59 n_hibma Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__)
#include <sys/ioctl.h>
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#ifdef UMODEM_DEBUG
#define DPRINTF(x)	if (umodemdebug) logprintf x
#define DPRINTFN(n,x)	if (umodemdebug>(n)) logprintf x
int	umodemdebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct umodem_softc {
	USBBASEDEVICE sc_dev;		/* base device */
	usbd_interface_handle sc_ctl;	/* control interface */
	usbd_interface_handle sc_data;	/* data interface */
	uByte	cmCaps;
	uByte	acmCaps;
};

void umodem_intr __P((usbd_xfer_handle, usbd_private_handle, usbd_status));
void umodem_disco __P((void *));

USB_DECLARE_DRIVER(umodem);

USB_MATCH(umodem)
{
	USB_MATCH_START(umodem, uaa);

	usb_interface_descriptor_t *id;
	
	if (!uaa->iface)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (!id ||
	    id->bInterfaceClass != UCLASS_CDC ||
	    id->bInterfaceSubClass != USUBCLASS_ABSTRACT_CONTROL_MODEL ||
	    id->bInterfaceProtocol != UPROTO_CDC_AT)
		return (UMATCH_NONE);
	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

USB_ATTACH(umodem)
{
	USB_ATTACH_START(umodem, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	char devinfo[1024];
	
	sc->sc_ctl = iface;
	id = usbd_get_interface_descriptor(iface);
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfo, id->bInterfaceClass, id->bInterfaceSubClass);

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__FreeBSD__)
Static int
umodem_detach(device_t self)
{
	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));

	return 0;
}
#endif

#if defined(__FreeBSD__)
DRIVER_MODULE(umodem, uhub, umodem_driver, umodem_devclass, usbd_driver_load,0);
#endif

