/*	$NetBSD: usbdevs_data.h,v 1.6 1998/10/05 02:31:14 mark Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: usbdevs,v 1.5 1998/10/05 02:30:17 mark Exp 
 */

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

struct usb_knowndev usb_knowndevs[] = {
	{
	    USB_VENDOR_NEC, USB_PRODUCT_NEC_HUB,
	    0,
	    "NEC",
	    "hub",
	},
	{
	    USB_VENDOR_KODAK, USB_PRODUCT_KODAK_DC260,
	    0,
	    "Eastman Kodak",
	    "Digital Science DC260",
	},
	{
	    USB_VENDOR_NANAO, USB_PRODUCT_NANAO_HUB,
	    0,
	    "Nanao",
	    "hub",
	},
	{
	    USB_VENDOR_NANAO, USB_PRODUCT_NANAO_MONITOR,
	    0,
	    "Nanao",
	    "monitor",
	},
	{
	    USB_VENDOR_UNIXTAR, USB_PRODUCT_UNIXTAR_UTUSB41,
	    0,
	    "Unixtar",
	    "UT-USB41",
	},
	{
	    USB_VENDOR_GENIUS, USB_PRODUCT_GENIUS_NICHE,
	    0,
	    "Genius",
	    "Niche mouse",
	},
	{
	    USB_VENDOR_GENIUS, USB_PRODUCT_GENIUS_FLIGHT2000,
	    0,
	    "Genius",
	    "Flight 2000 joystick",
	},
	{
	    USB_VENDOR_CHERRY, USB_PRODUCT_CHERRY_MY3000KBD,
	    0,
	    "Cherry",
	    "My3000 keyboard",
	},
	{
	    USB_VENDOR_CHERRY, USB_PRODUCT_CHERRY_MY3000HUB,
	    0,
	    "Cherry",
	    "My3000 hub",
	},
	{
	    USB_VENDOR_PHILIPS, USB_PRODUCT_PHILIPS_DSS,
	    0,
	    "Philips",
	    "DSS 350 Digital Speaker System",
	},
	{
	    USB_VENDOR_PHILIPS, USB_PRODUCT_PHILIPS_HUB,
	    0,
	    "Philips",
	    "hub",
	},
	{
	    USB_VENDOR_CONNECTIX, USB_PRODUCT_CONNECTIX_QUICKCAM,
	    0,
	    "Connectix",
	    "QuickCam",
	},
	{
	    USB_VENDOR_CYPRESS, USB_PRODUCT_CYPRESS_MOUSE,
	    0,
	    "Cypress Semicondutor",
	    "mouse",
	},
	{
	    USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_F5U002,
	    0,
	    "Belkin",
	    "Parallel printer adapter",
	},
	{
	    USB_VENDOR_EIZO, USB_PRODUCT_EIZO_HUB,
	    0,
	    "EIZO",
	    "hub",
	},
	{
	    USB_VENDOR_EIZO, USB_PRODUCT_EIZO_MONITOR,
	    0,
	    "EIZO",
	    "monitor",
	},
	{
	    USB_VENDOR_EIZONANAO, USB_PRODUCT_EIZONANAO_HUB,
	    0,
	    "EIZO Nanao",
	    "hub",
	},
	{
	    USB_VENDOR_EIZONANAO, USB_PRODUCT_EIZONANAO_MONITOR,
	    0,
	    "EIZO Nanao",
	    "monitor",
	},
	{
	    USB_VENDOR_CHIC, USB_PRODUCT_CHIC_MOUSE1,
	    0,
	    "Chic Technology",
	    "mouse",
	},
	{
	    USB_VENDOR_PLX, USB_PRODUCT_PLX_TESTBOARD,
	    0,
	    "PLX",
	    "test board",
	},
	{
	    USB_VENDOR_INSIDEOUT, USB_PRODUCT_INSIDEOUT_EDGEPORT4,
	    0,
	    "Inside Out Networks",
	    "EdgePort/4",
	},
	{
	    USB_VENDOR_INTEL, USB_PRODUCT_INTEL_TESTBOARD,
	    0,
	    "Intel",
	    "82930 test board",
	},
	{
	    USB_VENDOR_NEC, 0,
	    USB_KNOWNDEV_NOPROD,
	    "NEC",
	    NULL,
	},
	{
	    USB_VENDOR_KODAK, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Eastman Kodak",
	    NULL,
	},
	{
	    USB_VENDOR_NANAO, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Nanao",
	    NULL,
	},
	{
	    USB_VENDOR_UNIXTAR, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Unixtar",
	    NULL,
	},
	{
	    USB_VENDOR_GENIUS, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Genius",
	    NULL,
	},
	{
	    USB_VENDOR_CHERRY, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Cherry",
	    NULL,
	},
	{
	    USB_VENDOR_PHILIPS, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Philips",
	    NULL,
	},
	{
	    USB_VENDOR_CONNECTIX, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Connectix",
	    NULL,
	},
	{
	    USB_VENDOR_CYPRESS, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Cypress Semicondutor",
	    NULL,
	},
	{
	    USB_VENDOR_EIZO, 0,
	    USB_KNOWNDEV_NOPROD,
	    "EIZO",
	    NULL,
	},
	{
	    USB_VENDOR_BELKIN, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Belkin",
	    NULL,
	},
	{
	    USB_VENDOR_EIZONANAO, 0,
	    USB_KNOWNDEV_NOPROD,
	    "EIZO Nanao",
	    NULL,
	},
	{
	    USB_VENDOR_CHIC, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Chic Technology",
	    NULL,
	},
	{
	    USB_VENDOR_PLX, 0,
	    USB_KNOWNDEV_NOPROD,
	    "PLX",
	    NULL,
	},
	{
	    USB_VENDOR_INSIDEOUT, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Inside Out Networks",
	    NULL,
	},
	{
	    USB_VENDOR_INTEL, 0,
	    USB_KNOWNDEV_NOPROD,
	    "Intel",
	    NULL,
	},
	{ 0, 0, 0, NULL, NULL, }
};
