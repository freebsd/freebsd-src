/*	$NetBSD: usb_quirks.c,v 1.26 2000/04/27 15:26:50 augustss Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
 
#include <dev/usb/usb.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
extern int usbdebug;
#endif

#define ANY 0xffff

Static struct usbd_quirk_entry {
	u_int16_t idVendor;
	u_int16_t idProduct;
	u_int16_t bcdDevice;
	struct usbd_quirks quirks;
} usb_quirks[] = {
 { USB_VENDOR_KYE, USB_PRODUCT_KYE_NICHE,	    0x100, { UQ_NO_SET_PROTO}},
 { USB_VENDOR_INSIDEOUT,USB_PRODUCT_INSIDEOUT_EDGEPORT4, 
   						    0x094, { UQ_SWAP_UNICODE}},
 { USB_VENDOR_BTC, USB_PRODUCT_BTC_BTC7932,	    0x100, { UQ_NO_STRINGS }},
 { USB_VENDOR_ADS, USB_PRODUCT_ADS_UBS10BT,	    0x002, { UQ_NO_STRINGS }},
 { USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_SERIAL1, 0x101, { UQ_NO_STRINGS }},
 { USB_VENDOR_WACOM, USB_PRODUCT_WACOM_CT0405U,     0x101, { UQ_NO_STRINGS }},
 { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502,	    0x0a2, { UQ_BAD_ADC }},
 { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502,	    0x0a2, { UQ_AU_NO_XU }},
 { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ADA70,	    0x103, { UQ_BAD_ADC }},
 { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ASC495,      0x000, { UQ_BAD_AUDIO }},
 { USB_VENDOR_QTRONIX, USB_PRODUCT_QTRONIX_980N,    0x110, { UQ_SPUR_BUT_UP }},
 { USB_VENDOR_ALCOR2, USB_PRODUCT_ALCOR2_KBD_HUB,   0x001, { UQ_SPUR_BUT_UP }},
 { USB_VENDOR_MCT, USB_PRODUCT_MCT_HUB0100,         0x102, { UQ_BUS_POWERED }},
 { USB_VENDOR_MCT, USB_PRODUCT_MCT_USB232,          0x102, { UQ_BUS_POWERED }},
 { USB_VENDOR_METRICOM, USB_PRODUCT_METRICOM_RICOCHET_GS,
 	0x100, { UQ_ASSUME_CM_OVER_DATA | UQ_NO_STRINGS }},
 { USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_320U,
 						    0x000, { UQ_NO_STRINGS }},
 /* XXX These should have a revision number, but I don't know what they are. */
 { USB_VENDOR_HP, USB_PRODUCT_HP_895C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_880C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_815C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_810C,		    ANY,   { UQ_BROKEN_BIDIR }},
 { USB_VENDOR_HP, USB_PRODUCT_HP_830C,		    ANY,   { UQ_BROKEN_BIDIR }},
 /* YAMAHA router's ucdDevice is the version of farmware and often changes. */
 { USB_VENDOR_YAMAHA, USB_PRODUCT_YAMAHA_RTA54I,
	ANY, { UQ_ASSUME_CM_OVER_DATA }},
 { USB_VENDOR_YAMAHA, USB_PRODUCT_YAMAHA_RTA55I,
	ANY, { UQ_ASSUME_CM_OVER_DATA }},
 { USB_VENDOR_YAMAHA, USB_PRODUCT_YAMAHA_RTW65B,
	ANY, { UQ_ASSUME_CM_OVER_DATA }},
 { USB_VENDOR_YAMAHA, USB_PRODUCT_YAMAHA_RTW65I,
	ANY, { UQ_ASSUME_CM_OVER_DATA }},

 { 0, 0, 0, { 0 } }
};

struct usbd_quirks usbd_no_quirk = { 0 };

struct usbd_quirks *
usbd_find_quirk(usb_device_descriptor_t *d)
{
	struct usbd_quirk_entry *t;

	for (t = usb_quirks; t->idVendor != 0; t++) {
		if (t->idVendor  == UGETW(d->idVendor) &&
		    t->idProduct == UGETW(d->idProduct) &&
		    (t->bcdDevice == ANY || t->bcdDevice == UGETW(d->bcdDevice)))
			break;
	}
#ifdef USB_DEBUG
	if (usbdebug && t->quirks.uq_flags)
		logprintf("usbd_find_quirk 0x%04x/0x%04x/%x: %d\n", 
			  UGETW(d->idVendor), UGETW(d->idProduct),
			  UGETW(d->bcdDevice), t->quirks.uq_flags);
#endif
	return (&t->quirks);
}
