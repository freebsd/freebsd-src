/*	$NetBSD: usb_quirks.c,v 1.1 1998/07/12 19:52:00 augustss Exp $	*/

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
#if defined(__NetBSD__)
#include <sys/device.h>
#endif
#include <sys/select.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

struct usbd_quirk_entry {
	u_int16_t idVendor;
	u_int16_t idProduct;
	u_int16_t bcdDevice;
	struct usbd_quirks quirks;
} quirks[] = {
 { USB_VENDOR_GENIUS, USB_PRODUCT_GENIUS_NICHE,     0x100, { UQ_NO_SET_PROTO}},
 { USB_VENDOR_INSIDEOUT,USB_PRODUCT_INSIDEOUT_EDGEPORT4, 
   						    0x094, { UQ_SWAP_UNICODE}},
 { USB_VENDOR_UNIXTAR, USB_PRODUCT_UNIXTAR_UTUSB41, 0x100, { UQ_HUB_POWER }},
 { USB_VENDOR_BTC, USB_PRODUCT_BTC_BTC7932,        0x100, { UQ_NO_STRINGS }},
 { 0, 0, 0, { 0 } }
};

struct usbd_quirks usbd_no_quirk = { 0 };

struct usbd_quirks *
usbd_find_quirk(d)
	usb_device_descriptor_t *d;
{
	struct usbd_quirk_entry *t;

	for (t = quirks; t->idVendor != 0; t++) {
		if (t->idVendor  == UGETW(d->idVendor) &&
		    t->idProduct == UGETW(d->idProduct) &&
		    t->bcdDevice == UGETW(d->bcdDevice))
			break;
	}
#ifdef USB_DEBUG
	{ extern int usbdebug;
	if (usbdebug && t->quirks.uq_flags)
		printf("quirk %d/%d/%x: %d\n", 
		       UGETW(d->idVendor), UGETW(d->idProduct),
		       UGETW(d->bcdDevice), t->quirks.uq_flags);
	}
#endif
	return (&t->quirks);
}
