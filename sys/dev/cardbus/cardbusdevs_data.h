/*	$FreeBSD: src/sys/dev/cardbus/cardbusdevs_data.h,v 1.1 1999/11/18 07:22:59 imp Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	FreeBSD: src/sys/dev/cardbus/cardbusdevs,v 1.1 1999/11/18 07:21:50 imp Exp 
 */

/*
 * Copyright (C) 1999  Hayakawa Koichi.
 * All rights reserved.
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
 *      This product includes software developed by the author
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct cardbus_knowndev cardbus_knowndevs[] = {
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575TX,
	    0,
	    "3Com",
	    "3c575 100Base-TX",
	},
	{
	    CARDBUS_VENDOR_3COM, CARDBUS_PRODUCT_3COM_3C575BTX,
	    0,
	    "3Com",
	    "3c575B 100Base-TX",
	},
	{
	    CARDBUS_VENDOR_ADP, CARDBUS_PRODUCT_ADP_1480,
	    0,
	    "Adaptec",
	    "APA-1480",
	},
	{
	    CARDBUS_VENDOR_DEC, CARDBUS_PRODUCT_DEC_21142,
	    0,
	    "Digital Equipment",
	    "DECchip 21142/3",
	},
	{
	    CARDBUS_VENDOR_OPTI, CARDBUS_PRODUCT_OPTI_82C861,
	    0,
	    "Opti",
	    "82C861 USB Host Controller (OHCI)",
	},
	{
	    CARDBUS_VENDOR_DEC, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Digital Equipment",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_3COM, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "3Com",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_ADP2, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Adaptec (2nd PCI Vendor ID)",
	    NULL,
	},
	{
	    CARDBUS_VENDOR_OPTI, 0,
	    CARDBUS_KNOWNDEV_NOPROD,
	    "Opti",
	    NULL,
	},
	{ 0, 0, 0, NULL, NULL, }
};
