/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * Sony PS4 DualShock 4 driver
 * https://eleccelerator.com/wiki/index.php?title=DualShock_4
 * https://gist.github.com/johndrinkwater/7708901
 * https://www.psdevwiki.com/ps4/DS4-USB
 */

#include "opt_hid.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define	HID_DEBUG_VAR	ps4dshock_debug
#include <dev/hid/hgame.h>
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>
#include <dev/hid/hidmap.h>
#include "usbdevs.h"

#ifdef HID_DEBUG
static int ps4dshock_debug = 1;

static SYSCTL_NODE(_hw_hid, OID_AUTO, ps4dshock, CTLFLAG_RW, 0,
		"Sony PS4 DualShock Gamepad");
SYSCTL_INT(_hw_hid_ps4dshock, OID_AUTO, debug, CTLFLAG_RWTUN,
		&ps4dshock_debug, 0, "Debug level");
#endif

static const uint8_t	ps4dshock_rdesc[] = {
	0x05, 0x01,		/* Usage Page (Generic Desktop Ctrls)	*/
	0x09, 0x05,		/* Usage (Game Pad)			*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x85, 0x01,		/*   Report ID (1)			*/
	0x09, 0x30,		/*   Usage (X)				*/
	0x09, 0x31,		/*   Usage (Y)				*/
	0x09, 0x33,		/*   Usage (Rx)				*/
	0x09, 0x34,		/*   Usage (Ry)				*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x04,		/*   Report Count (4)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x09, 0x39,		/*   Usage (Hat switch)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x07,		/*   Logical Maximum (7)		*/
	0x35, 0x00,		/*   Physical Minimum (0)		*/
	0x46, 0x3B, 0x01,	/*   Physical Maximum (315)		*/
	0x65, 0x14,		/*   Unit (System: English Rotation, Length: Centimeter) */
	0x75, 0x04,		/*   Report Size (4)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x42,		/*   Input (Data,Var,Abs,Null State)	*/
	0x65, 0x00,		/*   Unit (None)			*/
	0x45, 0x00,		/*   Physical Maximum (0)		*/
	0x05, 0x09,		/*   Usage Page (Button)		*/
	0x19, 0x01,		/*   Usage Minimum (0x01)		*/
	0x29, 0x0E,		/*   Usage Maximum (0x0E)		*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x01,		/*   Logical Maximum (1)		*/
	0x75, 0x01,		/*   Report Size (1)			*/
	0x95, 0x0E,		/*   Report Count (14)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x20,		/*   Usage (0x20)			*/
	0x75, 0x06,		/*   Report Size (6)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x3F,		/*   Logical Maximum (63)		*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*   Usage Page (Generic Desktop Ctrls)	*/
	0x09, 0x32,		/*   Usage (Z)				*/
	0x09, 0x35,		/*   Usage (Rz)				*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0xC0,			/* End Collection			*/
	0x05, 0x01,		/* Usage Page (Generic Desktop Ctrls)	*/
	0x09, 0x08,		/* Usage (Multi-axis Controller)	*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x27, 0xFF, 0xFF, 0x00, 0x00,	/*   Logical Maximum (65534)	*/
	0x75, 0x10,		/*   Report Size (16)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x05, 0x06,		/*   Usage Page (Generic Dev Ctrls)	*/
	0x09, 0x20,		/*   Usage (Battery Strength)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*   Usage Page (Generic Desktop Ctrls)	*/
	0x19, 0x33,		/*   Usage Minimum (RX)			*/
	0x29, 0x35,		/*   Usage Maximum (RZ)			*/
	0x16, 0x00, 0x80,	/*   Logical Minimum (-32768)		*/
	0x26, 0xFF, 0x7F,	/*   Logical Maximum (32767)		*/
	0x75, 0x10,		/*   Report Size (16)			*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x19, 0x30,		/*   Usage Minimum (X)			*/
	0x29, 0x32,		/*   Usage Maximum (Z)			*/
	0x16, 0x00, 0x80,	/*   Logical Minimum (-32768)		*/
	0x26, 0xFF, 0x7F,	/*   Logical Maximum (32767)		*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x05,		/*   Report Count (5)			*/
	0x81, 0x03,		/*   Input (Const)			*/
	0xC0,			/* End Collection			*/
	0x05, 0x0C,		/* Usage Page (Consumer)		*/
	0x09, 0x05,		/* Usage (Headphone)			*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x75, 0x05,		/*   Report Size (5)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x03,		/*   Input (Const)			*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x20,		/*   Usage (0x20)			*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x01,		/*   Logical Maximum (1)		*/
	0x75, 0x01,		/*   Report Size (1)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x75, 0x01,		/*   Report Size (1)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x03,		/*   Input (Const)			*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0x81, 0x03,		/*   Input (Const)			*/
	0xC0,			/* End Collection			*/
	0x05, 0x0D,		/* Usage Page (Digitizer)		*/
	0x09, 0x05,		/* Usage (Touch Pad)			*/
	0xA1, 0x01,		/* Collection (Application)		*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x25, 0x03,		/*   Logical Maximum (3)		*/
	0x75, 0x04,		/*   Report Size (4)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x75, 0x04,		/*   Report Size (4)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x03,		/*   Input (Data,Var,Abs)		*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x56,		/*   Usage (0x56)			*/
	0x55, 0x0C,		/*   Unit Exponent (-4)			*/
	0x66, 0x01, 0x10,	/*   Unit (System: SI Linear, Time: Seconds) */
	0x46, 0xCC, 0x06,	/*   Physical Maximum (1740)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*   Unit (None)			*/
	0x45, 0x00,		/*   Physical Maximum (0)		*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x56,		/*   Usage (0x56)			*/
	0x55, 0x0C,		/*   Unit Exponent (-4)			*/
	0x66, 0x01, 0x10,	/*   Unit (System: SI Linear, Time: Seconds) */
	0x46, 0xCC, 0x06,	/*   Physical Maximum (1740)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*   Unit (None)			*/
	0x45, 0x00,		/*   Physical Maximum (0)		*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x56,		/*   Usage (0x56)			*/
	0x55, 0x0C,		/*   Unit Exponent (-4)			*/
	0x66, 0x01, 0x10,	/*   Unit (System: SI Linear, Time: Seconds) */
	0x46, 0xCC, 0x06,	/*   Physical Maximum (1740)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0x81, 0x02,		/*   Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*   Unit (None)			*/
	0x45, 0x00,		/*   Physical Maximum (0)		*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x05, 0x0D,		/*   Usage Page (Digitizer)		*/
	0x09, 0x22,		/*   Usage (Finger)			*/
	0xA1, 0x02,		/*   Collection (Logical)		*/
	0x09, 0x51,		/*     Usage (0x51)			*/
	0x25, 0x7F,		/*     Logical Maximum (127)		*/
	0x75, 0x07,		/*     Report Size (7)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x42,		/*     Usage (Tip Switch)		*/
	0x25, 0x01,		/*     Logical Maximum (1)		*/
	0x75, 0x01,		/*     Report Size (1)			*/
	0x95, 0x01,		/*     Report Count (1)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x05, 0x01,		/*     Usage Page (Generic Desktop Ctrls) */
	0x09, 0x30,		/*     Usage (X)			*/
	0x55, 0x0E,		/*     Unit Exponent (-2)		*/
	0x65, 0x11,		/*     Unit (System: SI Linear, Length: Centimeter) */
	0x35, 0x00,		/*     Physical Minimum (0)		*/
	0x46, 0xB8, 0x01,	/*     Physical Maximum (440)		*/
	0x26, 0x80, 0x07,	/*     Logical Maximum (1920)		*/
	0x75, 0x0C,		/*     Report Size (12)			*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x09, 0x31,		/*     Usage (Y)			*/
	0x46, 0xC0, 0x00,	/*     Physical Maximum (192)		*/
	0x26, 0xAE, 0x03,	/*     Logical Maximum (942)		*/
	0x81, 0x02,		/*     Input (Data,Var,Abs)		*/
	0x65, 0x00,		/*     Unit (None)			*/
	0x45, 0x00,		/*     Physical Maximum (0)		*/
	0xC0,			/*   End Collection			*/
	0x75, 0x08,		/*   Report Size (8)			*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0x81, 0x03,		/*   Input (Const)			*/
	/* Output and feature reports */
	0x85, 0x05,		/*   Report ID (5)			*/
	0x06, 0x00, 0xFF,	/*   Usage Page (Vendor Defined 0xFF00)	*/
	0x09, 0x22,		/*   Usage (0x22)			*/
	0x15, 0x00,		/*   Logical Minimum (0)		*/
	0x26, 0xFF, 0x00,	/*   Logical Maximum (255)		*/
	0x95, 0x1F,		/*   Report Count (31)			*/
	0x91, 0x02,		/*   Output (Data,Var,Abs)		*/
	0x85, 0x04,		/*   Report ID (4)			*/
	0x09, 0x23,		/*   Usage (0x23)			*/
	0x95, 0x24,		/*   Report Count (36)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x02,		/*   Report ID (2)			*/
	0x09, 0x24,		/*   Usage (0x24)			*/
	0x95, 0x24,		/*   Report Count (36)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x08,		/*   Report ID (8)			*/
	0x09, 0x25,		/*   Usage (0x25)			*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x10,		/*   Report ID (16)			*/
	0x09, 0x26,		/*   Usage (0x26)			*/
	0x95, 0x04,		/*   Report Count (4)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x11,		/*   Report ID (17)			*/
	0x09, 0x27,		/*   Usage (0x27)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x12,		/*   Report ID (18)			*/
	0x06, 0x02, 0xFF,	/*   Usage Page (Vendor Defined 0xFF02)	*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x95, 0x0F,		/*   Report Count (15)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x13,		/*   Report ID (19)			*/
	0x09, 0x22,		/*   Usage (0x22)			*/
	0x95, 0x16,		/*   Report Count (22)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x14,		/*   Report ID (20)			*/
	0x06, 0x05, 0xFF,	/*   Usage Page (Vendor Defined 0xFF05)	*/
	0x09, 0x20,		/*   Usage (0x20)			*/
	0x95, 0x10,		/*   Report Count (16)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x15,		/*   Report ID (21)			*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x95, 0x2C,		/*   Report Count (44)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x06, 0x80, 0xFF,	/*   Usage Page (Vendor Defined 0xFF80)	*/
	0x85, 0x80,		/*   Report ID (-128)			*/
	0x09, 0x20,		/*   Usage (0x20)			*/
	0x95, 0x06,		/*   Report Count (6)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x81,		/*   Report ID (-127)			*/
	0x09, 0x21,		/*   Usage (0x21)			*/
	0x95, 0x06,		/*   Report Count (6)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x82,		/*   Report ID (-126)			*/
	0x09, 0x22,		/*   Usage (0x22)			*/
	0x95, 0x05,		/*   Report Count (5)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x83,		/*   Report ID (-125)			*/
	0x09, 0x23,		/*   Usage (0x23)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x84,		/*   Report ID (-124)			*/
	0x09, 0x24,		/*   Usage (0x24)			*/
	0x95, 0x04,		/*   Report Count (4)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x85,		/*   Report ID (-123)			*/
	0x09, 0x25,		/*   Usage (0x25)			*/
	0x95, 0x06,		/*   Report Count (6)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x86,		/*   Report ID (-122)			*/
	0x09, 0x26,		/*   Usage (0x26)			*/
	0x95, 0x06,		/*   Report Count (6)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x87,		/*   Report ID (-121)			*/
	0x09, 0x27,		/*   Usage (0x27)			*/
	0x95, 0x23,		/*   Report Count (35)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x88,		/*   Report ID (-120)			*/
	0x09, 0x28,		/*   Usage (0x28)			*/
	0x95, 0x22,		/*   Report Count (34)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x89,		/*   Report ID (-119)			*/
	0x09, 0x29,		/*   Usage (0x29)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x90,		/*   Report ID (-112)			*/
	0x09, 0x30,		/*   Usage (0x30)			*/
	0x95, 0x05,		/*   Report Count (5)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x91,		/*   Report ID (-111)			*/
	0x09, 0x31,		/*   Usage (0x31)			*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x92,		/*   Report ID (-110)			*/
	0x09, 0x32,		/*   Usage (0x32)			*/
	0x95, 0x03,		/*   Report Count (3)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0x93,		/*   Report ID (-109)			*/
	0x09, 0x33,		/*   Usage (0x33)			*/
	0x95, 0x0C,		/*   Report Count (12)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA0,		/*   Report ID (-96)			*/
	0x09, 0x40,		/*   Usage (0x40)			*/
	0x95, 0x06,		/*   Report Count (6)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA1,		/*   Report ID (-95)			*/
	0x09, 0x41,		/*   Usage (0x41)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA2,		/*   Report ID (-94)			*/
	0x09, 0x42,		/*   Usage (0x42)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA3,		/*   Report ID (-93)			*/
	0x09, 0x43,		/*   Usage (0x43)			*/
	0x95, 0x30,		/*   Report Count (48)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA4,		/*   Report ID (-92)			*/
	0x09, 0x44,		/*   Usage (0x44)			*/
	0x95, 0x0D,		/*   Report Count (13)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA5,		/*   Report ID (-91)			*/
	0x09, 0x45,		/*   Usage (0x45)			*/
	0x95, 0x15,		/*   Report Count (21)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA6,		/*   Report ID (-90)			*/
	0x09, 0x46,		/*   Usage (0x46)			*/
	0x95, 0x15,		/*   Report Count (21)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xF0,		/*   Report ID (-16)			*/
	0x09, 0x47,		/*   Usage (0x47)			*/
	0x95, 0x3F,		/*   Report Count (63)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xF1,		/*   Report ID (-15)			*/
	0x09, 0x48,		/*   Usage (0x48)			*/
	0x95, 0x3F,		/*   Report Count (63)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xF2,		/*   Report ID (-14)			*/
	0x09, 0x49,		/*   Usage (0x49)			*/
	0x95, 0x0F,		/*   Report Count (15)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA7,		/*   Report ID (-89)			*/
	0x09, 0x4A,		/*   Usage (0x4A)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA8,		/*   Report ID (-88)			*/
	0x09, 0x4B,		/*   Usage (0x4B)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xA9,		/*   Report ID (-87)			*/
	0x09, 0x4C,		/*   Usage (0x4C)			*/
	0x95, 0x08,		/*   Report Count (8)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAA,		/*   Report ID (-86)			*/
	0x09, 0x4E,		/*   Usage (0x4E)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAB,		/*   Report ID (-85)			*/
	0x09, 0x4F,		/*   Usage (0x4F)			*/
	0x95, 0x39,		/*   Report Count (57)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAC,		/*   Report ID (-84)			*/
	0x09, 0x50,		/*   Usage (0x50)			*/
	0x95, 0x39,		/*   Report Count (57)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAD,		/*   Report ID (-83)			*/
	0x09, 0x51,		/*   Usage (0x51)			*/
	0x95, 0x0B,		/*   Report Count (11)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAE,		/*   Report ID (-82)			*/
	0x09, 0x52,		/*   Usage (0x52)			*/
	0x95, 0x01,		/*   Report Count (1)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xAF,		/*   Report ID (-81)			*/
	0x09, 0x53,		/*   Usage (0x53)			*/
	0x95, 0x02,		/*   Report Count (2)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0x85, 0xB0,		/*   Report ID (-80)			*/
	0x09, 0x54,		/*   Usage (0x54)			*/
	0x95, 0x3F,		/*   Report Count (63)			*/
	0xB1, 0x02,		/*   Feature (Data,Var,Abs)		*/
	0xC0,			/* End Collection			*/
};

#define	PS4DS_GYRO_RES_PER_DEG_S	1024
#define	PS4DS_ACC_RES_PER_G		8192
#define	PS4DS_MAX_TOUCHPAD_PACKETS	4
#define	PS4DS_FEATURE_REPORT2_SIZE	37
#define	PS4DS_OUTPUT_REPORT5_SIZE	32
#define	PS4DS_OUTPUT_REPORT11_SIZE	78

static hidmap_cb_t	ps4dshock_final_cb;
static hidmap_cb_t	ps4dsacc_data_cb;
static hidmap_cb_t	ps4dsacc_tstamp_cb;
static hidmap_cb_t	ps4dsacc_final_cb;
static hidmap_cb_t	ps4dsmtp_data_cb;
static hidmap_cb_t	ps4dsmtp_npackets_cb;
static hidmap_cb_t	ps4dsmtp_final_cb;

struct ps4ds_out5 {
	uint8_t features;
	uint8_t	reserved1;
	uint8_t	reserved2;
	uint8_t	rumble_right;
	uint8_t	rumble_left;
	uint8_t	led_color_r;
	uint8_t	led_color_g;
	uint8_t	led_color_b;
	uint8_t	led_delay_on;	/* centiseconds */
	uint8_t	led_delay_off;
} __attribute__((packed));

static const struct ps4ds_led {
	int	r;
	int	g;
	int	b;
} ps4ds_leds[] = {
	/* The first 4 entries match the PS4, other from Linux driver */
	{ 0x00, 0x00, 0x40 },	/* Blue   */
	{ 0x40, 0x00, 0x00 },	/* Red	  */
	{ 0x00, 0x40, 0x00 },	/* Green  */
	{ 0x20, 0x00, 0x20 },	/* Pink   */
	{ 0x02, 0x01, 0x00 },	/* Orange */
	{ 0x00, 0x01, 0x01 },	/* Teal   */
	{ 0x01, 0x01, 0x01 }	/* White  */
};

enum ps4ds_led_state {
	PS4DS_LED_OFF,
	PS4DS_LED_ON,
	PS4DS_LED_BLINKING,
	PD4DS_LED_CNT,
};

/* Map structure for accelerometer and gyro. */
struct ps4ds_calib_data {
	int32_t usage;
	int32_t code;
	int32_t res;
	int32_t range;
	/* Calibration data for accelerometer and gyro. */
	int16_t bias;
	int32_t sens_numer;
	int32_t sens_denom;
};

enum {
	PS4DS_TSTAMP,
	PS4DS_CID1,
	PS4DS_TIP1,
	PS4DS_X1,
	PS4DS_Y1,
	PS4DS_CID2,
	PS4DS_TIP2,
	PS4DS_X2,
	PS4DS_Y2,
	PS4DS_NTPUSAGES,
};

struct ps4dshock_softc {
	struct hidmap		hm;

	bool			is_bluetooth;

	struct sx		lock;
	enum ps4ds_led_state	led_state;
	struct ps4ds_led	led_color;
	int			led_delay_on;	/* msecs */
	int			led_delay_off;

	int			rumble_right;
	int			rumble_left;
};

struct ps4dsacc_softc {
	struct hidmap		hm;

	uint16_t		hw_tstamp;
	int32_t			ev_tstamp;

	struct ps4ds_calib_data	calib_data[6];
};

struct ps4dsmtp_softc {
	struct hidmap		hm;

	struct hid_location	btn_loc;
	u_int		npackets;
	int32_t		*data_ptr;
	int32_t		data[PS4DS_MAX_TOUCHPAD_PACKETS * PS4DS_NTPUSAGES];

	bool		do_tstamps;
	uint8_t		hw_tstamp;
	int32_t		ev_tstamp;
	bool		touch;
};

#define PD4DSHOCK_OFFSET(field) offsetof(struct ps4dshock_softc, field)
enum {
	PD4DSHOCK_SYSCTL_LED_STATE =	PD4DSHOCK_OFFSET(led_state),
	PD4DSHOCK_SYSCTL_LED_COLOR_R =	PD4DSHOCK_OFFSET(led_color.r),
	PD4DSHOCK_SYSCTL_LED_COLOR_G =	PD4DSHOCK_OFFSET(led_color.g),
	PD4DSHOCK_SYSCTL_LED_COLOR_B =	PD4DSHOCK_OFFSET(led_color.b),
	PD4DSHOCK_SYSCTL_LED_DELAY_ON =	PD4DSHOCK_OFFSET(led_delay_on),
	PD4DSHOCK_SYSCTL_LED_DELAY_OFF=	PD4DSHOCK_OFFSET(led_delay_off),
#define	PD4DSHOCK_SYSCTL_LAST		PD4DSHOCK_SYSCTL_LED_DELAY_OFF
};

#define PS4DS_MAP_BTN(number, code)		\
	{ HIDMAP_KEY(HUP_BUTTON, number, code) }
#define PS4DS_MAP_ABS(usage, code)		\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code) }
#define PS4DS_MAP_FLT(usage, code)		\
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_##usage, code), .flat = 15 }
#define PS4DS_MAP_VSW(usage, code)	\
	{ HIDMAP_SW(HUP_MICROSOFT, usage, code) }
#define PS4DS_MAP_GCB(usage, callback)	\
	{ HIDMAP_ANY_CB(HUP_GENERIC_DESKTOP, HUG_##usage, callback) }
#define PS4DS_MAP_VCB(usage, callback)	\
	{ HIDMAP_ANY_CB(HUP_MICROSOFT, usage, callback) }
#define PS4DS_FINALCB(cb)			\
	{ HIDMAP_FINAL_CB(&cb) }

static const struct hidmap_item ps4dshock_map[] = {
	PS4DS_MAP_FLT(X,		ABS_X),
	PS4DS_MAP_FLT(Y,		ABS_Y),
	PS4DS_MAP_ABS(Z,		ABS_Z),
	PS4DS_MAP_FLT(RX,		ABS_RX),
	PS4DS_MAP_FLT(RY,		ABS_RY),
	PS4DS_MAP_ABS(RZ,		ABS_RZ),
	PS4DS_MAP_BTN(1,		BTN_WEST),
	PS4DS_MAP_BTN(2,		BTN_SOUTH),
	PS4DS_MAP_BTN(3,		BTN_EAST),
	PS4DS_MAP_BTN(4,		BTN_NORTH),
	PS4DS_MAP_BTN(5,		BTN_TL),
	PS4DS_MAP_BTN(6,		BTN_TR),
	PS4DS_MAP_BTN(7,		BTN_TL2),
	PS4DS_MAP_BTN(8,		BTN_TR2),
	PS4DS_MAP_BTN(9,		BTN_SELECT),
	PS4DS_MAP_BTN(10,		BTN_START),
	PS4DS_MAP_BTN(11,		BTN_THUMBL),
	PS4DS_MAP_BTN(12,		BTN_THUMBR),
	PS4DS_MAP_BTN(13,		BTN_MODE),
	/* Click button is handled by touchpad driver */
	/* PS4DS_MAP_BTN(14,	BTN_LEFT), */
	PS4DS_MAP_GCB(HAT_SWITCH,	hgame_hat_switch_cb),
	PS4DS_FINALCB(			ps4dshock_final_cb),
};
static const struct hidmap_item ps4dsacc_map[] = {
	PS4DS_MAP_GCB(X,		ps4dsacc_data_cb),
	PS4DS_MAP_GCB(Y,		ps4dsacc_data_cb),
	PS4DS_MAP_GCB(Z,		ps4dsacc_data_cb),
	PS4DS_MAP_GCB(RX,		ps4dsacc_data_cb),
	PS4DS_MAP_GCB(RY,		ps4dsacc_data_cb),
	PS4DS_MAP_GCB(RZ,		ps4dsacc_data_cb),
	PS4DS_MAP_VCB(0x0021,		ps4dsacc_tstamp_cb),
	PS4DS_FINALCB(			ps4dsacc_final_cb),
};
static const struct hidmap_item ps4dshead_map[] = {
	PS4DS_MAP_VSW(0x0020,		SW_MICROPHONE_INSERT),
	PS4DS_MAP_VSW(0x0021,		SW_HEADPHONE_INSERT),
};
static const struct hidmap_item ps4dsmtp_map[] = {
	{ HIDMAP_ABS_CB(HUP_MICROSOFT, 0x0021, 		ps4dsmtp_npackets_cb)},
	{ HIDMAP_ABS_CB(HUP_DIGITIZERS, HUD_SCAN_TIME,	ps4dsmtp_data_cb) },
	{ HIDMAP_ABS_CB(HUP_DIGITIZERS, HUD_CONTACTID,	ps4dsmtp_data_cb) },
	{ HIDMAP_ABS_CB(HUP_DIGITIZERS, HUD_TIP_SWITCH,	ps4dsmtp_data_cb) },
	{ HIDMAP_ABS_CB(HUP_GENERIC_DESKTOP, HUG_X,	ps4dsmtp_data_cb) },
	{ HIDMAP_ABS_CB(HUP_GENERIC_DESKTOP, HUG_Y,	ps4dsmtp_data_cb) },
	{ HIDMAP_FINAL_CB(				ps4dsmtp_final_cb) },
};

static const struct hid_device_id ps4dshock_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x9cc),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x5c4),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0xba0),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
};
static const struct hid_device_id ps4dsacc_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x9cc),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_MULTIAXIS_CNTROLLER) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x5c4),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_MULTIAXIS_CNTROLLER) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0xba0),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_MULTIAXIS_CNTROLLER) },
};
static const struct hid_device_id ps4dshead_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x9cc),
	  HID_TLC(HUP_CONSUMER, HUC_HEADPHONE) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x5c4),
	  HID_TLC(HUP_CONSUMER, HUC_HEADPHONE) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0xba0),
	  HID_TLC(HUP_CONSUMER, HUC_HEADPHONE) },
};
static const struct hid_device_id ps4dsmtp_devs[] = {
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x9cc),
	  HID_TLC(HUP_DIGITIZERS, HUD_TOUCHPAD) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0x5c4),
	  HID_TLC(HUP_DIGITIZERS, HUD_TOUCHPAD) },
	{ HID_BVP(BUS_USB, USB_VENDOR_SONY, 0xba0),
	  HID_TLC(HUP_DIGITIZERS, HUD_TOUCHPAD) },
};

static int
ps4dshock_final_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING)
		evdev_support_prop(evdev, INPUT_PROP_DIRECT);

	/* Do not execute callback at interrupt handler and detach */
	return (ENOSYS);
}

static int
ps4dsacc_data_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	struct ps4dsacc_softc *sc = HIDMAP_CB_GET_SOFTC();
	struct ps4ds_calib_data *calib;
	u_int i;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		for (i = 0; i < nitems(sc->calib_data); i++) {
			if (sc->calib_data[i].usage == ctx.hi->usage) {
				evdev_support_abs(evdev,
				     sc->calib_data[i].code,
				    -sc->calib_data[i].range,
				     sc->calib_data[i].range, 16, 0,
				     sc->calib_data[i].res);
				HIDMAP_CB_UDATA = &sc->calib_data[i];
				break;
			}
		}
		break;

	case HIDMAP_CB_IS_RUNNING:
		calib = HIDMAP_CB_UDATA;
		evdev_push_abs(evdev, calib->code,
		    ((int64_t)ctx.data - calib->bias) * calib->sens_numer /
		    calib->sens_denom);
		break;

	default:
		break;
	}

	return (0);
}

static int
ps4dsacc_tstamp_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	struct ps4dsacc_softc *sc = HIDMAP_CB_GET_SOFTC();
	uint16_t tstamp;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		evdev_support_event(evdev, EV_MSC);
		evdev_support_msc(evdev, MSC_TIMESTAMP);
		break;

	case HIDMAP_CB_IS_RUNNING:
		/* Convert timestamp (in 5.33us unit) to timestamp_us */
		tstamp = (uint16_t)ctx.data;
		sc->ev_tstamp += (uint16_t)(tstamp - sc->hw_tstamp) * 16 / 3;
		sc->hw_tstamp = tstamp;
		evdev_push_msc(evdev, MSC_TIMESTAMP, sc->ev_tstamp);
		break;

	default:
		break;
	}

	return (0);
}

static int
ps4dsacc_final_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING) {
		evdev_support_event(evdev, EV_ABS);
		evdev_support_prop(evdev, INPUT_PROP_ACCELEROMETER);
	}
        /* Do not execute callback at interrupt handler and detach */
        return (ENOSYS);
}

static int
ps4dsmtp_npackets_cb(HIDMAP_CB_ARGS)
{
	struct ps4dsmtp_softc *sc = HIDMAP_CB_GET_SOFTC();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_RUNNING) {
		sc->npackets = MIN(PS4DS_MAX_TOUCHPAD_PACKETS,(u_int)ctx.data);
		/* Reset pointer here as it is first usage in touchpad TLC */
		sc->data_ptr = sc->data;
	}

	return (0);
}

static int
ps4dsmtp_data_cb(HIDMAP_CB_ARGS)
{
	struct ps4dsmtp_softc *sc = HIDMAP_CB_GET_SOFTC();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_RUNNING) {
		*sc->data_ptr = ctx.data;
		++sc->data_ptr;
	}

	return (0);
}

static void
ps4dsmtp_push_packet(struct ps4dsmtp_softc *sc, struct evdev_dev *evdev,
    int32_t *data)
{
	uint8_t hw_tstamp, delta;
	bool touch;

	evdev_push_abs(evdev, ABS_MT_SLOT, 0);
	if (data[PS4DS_TIP1] == 0) {
		evdev_push_abs(evdev, ABS_MT_TRACKING_ID, data[PS4DS_CID1]);
		evdev_push_abs(evdev, ABS_MT_POSITION_X, data[PS4DS_X1]);
		evdev_push_abs(evdev, ABS_MT_POSITION_Y, data[PS4DS_Y1]);
	} else
		evdev_push_abs(evdev, ABS_MT_TRACKING_ID, -1);
	evdev_push_abs(evdev, ABS_MT_SLOT, 1);
	if (data[PS4DS_TIP2] == 0) {
		evdev_push_abs(evdev, ABS_MT_TRACKING_ID, data[PS4DS_CID2]);
		evdev_push_abs(evdev, ABS_MT_POSITION_X, data[PS4DS_X2]);
		evdev_push_abs(evdev, ABS_MT_POSITION_Y, data[PS4DS_Y2]);
	} else
		evdev_push_abs(evdev, ABS_MT_TRACKING_ID, -1);

	if (sc->do_tstamps) {
		/*
		 * Export hardware timestamps in libinput-friendly way.
		 * Make timestamp counter 32-bit, scale up hardware
		 * timestamps to be on per 1usec basis and reset
		 * counter at the start of each touch.
		 */
		hw_tstamp = (uint8_t)data[PS4DS_TSTAMP];
		delta = hw_tstamp - sc->hw_tstamp;
		sc->hw_tstamp = hw_tstamp;
		touch = data[PS4DS_TIP1] == 0 || data[PS4DS_TIP2] == 0;
		/* Hardware timestamp counter ticks in 682 usec interval. */
		if ((touch || sc->touch) && delta != 0) {
			if (sc->touch)
				sc->ev_tstamp += delta * 682;
			evdev_push_msc(evdev, MSC_TIMESTAMP, sc->ev_tstamp);
		}
		if (!touch)
			sc->ev_tstamp = 0;
		sc->touch = touch;
	}
}

static int
ps4dsmtp_final_cb(HIDMAP_CB_ARGS)
{
	struct ps4dsmtp_softc *sc = HIDMAP_CB_GET_SOFTC();
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	int32_t *data;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		if (hid_test_quirk(hid_get_device_info(sc->hm.dev),
		    HQ_MT_TIMESTAMP))
			sc->do_tstamps = true;
		/*
		 * Dualshock 4 touchpad TLC contained in fixed report
		 * descriptor is almost compatible with MS precission touchpad
		 * specs and hmt(4) driver. But... for some reasons "Click"
		 * button location was grouped with other GamePad buttons by
		 * touchpad designers so it belongs to GamePad TLC. Fix it with
		 * direct reading of "Click" button value from interrupt frame.
		 */
		sc->btn_loc = (struct hid_location) { 1, 0, 49 };
		evdev_support_event(evdev, EV_SYN);
		evdev_support_event(evdev, EV_KEY);
		evdev_support_event(evdev, EV_ABS);
		if (sc->do_tstamps) {
			evdev_support_event(evdev, EV_MSC);
			evdev_support_msc(evdev, MSC_TIMESTAMP);
		}
		evdev_support_key(evdev, BTN_LEFT);
		evdev_support_abs(evdev, ABS_MT_SLOT, 0, 1, 0, 0, 0);
		evdev_support_abs(evdev, ABS_MT_TRACKING_ID, -1, 127, 0, 0, 0);
		evdev_support_abs(evdev, ABS_MT_POSITION_X, 0, 1920, 0, 0, 30);
		evdev_support_abs(evdev, ABS_MT_POSITION_Y, 0, 942, 0, 0, 49);
		evdev_support_prop(evdev, INPUT_PROP_POINTER);
		evdev_support_prop(evdev, INPUT_PROP_BUTTONPAD);
		evdev_set_flag(evdev, EVDEV_FLAG_MT_STCOMPAT);
		break;

	case HIDMAP_CB_IS_RUNNING:
		/* Only packets with ReportID=1 are accepted */
		if (HIDMAP_CB_GET_RID() != 1)
			return (ENOTSUP);
		evdev_push_key(evdev, BTN_LEFT,
		    HIDMAP_CB_GET_UDATA(&sc->btn_loc));
		for (data = sc->data;
		     data < sc->data + PS4DS_NTPUSAGES * sc->npackets;
		     data += PS4DS_NTPUSAGES) {
			ps4dsmtp_push_packet(sc, evdev, data);
			evdev_sync(evdev);
		}
		break;

	default:
		break;
	}

	/* Do execute callback at interrupt handler and detach */
	return (0);
}

static int
ps4dshock_write(struct ps4dshock_softc *sc)
{
	hid_size_t osize = sc->is_bluetooth ?
	    PS4DS_OUTPUT_REPORT11_SIZE : PS4DS_OUTPUT_REPORT5_SIZE;
	uint8_t buf[osize];
	int offset;
	bool led_on, led_blinks;

	memset(buf, 0, osize);
	buf[0] = sc->is_bluetooth ? 0x11 : 0x05;
	offset = sc->is_bluetooth ? 3 : 1;
	led_on = sc->led_state != PS4DS_LED_OFF;
	led_blinks = sc->led_state == PS4DS_LED_BLINKING;
	*(struct ps4ds_out5 *)(buf + offset) = (struct ps4ds_out5) {
		.features = 0x07, /* blink + LEDs + motor */
		.rumble_right = sc->rumble_right,
		.rumble_left = sc->rumble_left,
		.led_color_r = led_on ? sc->led_color.r : 0,
		.led_color_g = led_on ? sc->led_color.g : 0,
		.led_color_b = led_on ? sc->led_color.b : 0,
		/* convert milliseconds to centiseconds */
		.led_delay_on = led_blinks ? sc->led_delay_on / 10 : 0,
		.led_delay_off = led_blinks ? sc->led_delay_off / 10 : 0,
	};

	return (hid_write(sc->hm.dev, buf, osize));
}

/* Synaptics Touchpad */
static int
ps4dshock_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct ps4dshock_softc *sc;
	int error, arg;

	if (oidp->oid_arg1 == NULL || oidp->oid_arg2 < 0 ||
	    oidp->oid_arg2 > PD4DSHOCK_SYSCTL_LAST)
		return (EINVAL);

	sc = oidp->oid_arg1;
	sx_xlock(&sc->lock);

	/* Read the current value. */
	arg = *(int *)((char *)sc + oidp->oid_arg2);
	error = sysctl_handle_int(oidp, &arg, 0, req);

	/* Sanity check. */
	if (error || !req->newptr)
		goto unlock;

	/*
	 * Check that the new value is in the concerned node's range
	 * of values.
	 */
	switch (oidp->oid_arg2) {
	case PD4DSHOCK_SYSCTL_LED_STATE:
		if (arg < 0 || arg >= PD4DS_LED_CNT)
			error = EINVAL;
		break;
	case PD4DSHOCK_SYSCTL_LED_COLOR_R:
	case PD4DSHOCK_SYSCTL_LED_COLOR_G:
	case PD4DSHOCK_SYSCTL_LED_COLOR_B:
		if (arg < 0 || arg > UINT8_MAX)
			error = EINVAL;
		break;
	case PD4DSHOCK_SYSCTL_LED_DELAY_ON:
	case PD4DSHOCK_SYSCTL_LED_DELAY_OFF:
		if (arg < 0 || arg > UINT8_MAX * 10)
			error = EINVAL;
		break;
	default:
		error = EINVAL;
	}

	/* Update. */
	if (error == 0) {
		*(int *)((char *)sc + oidp->oid_arg2) = arg;
		ps4dshock_write(sc);
	}
unlock:
	sx_unlock(&sc->lock);

	return (error);
}

static void
ps4dshock_identify(driver_t *driver, device_t parent)
{

	/* Overload PS4 DualShock gamepad rudimentary report descriptor */
	if (HIDBUS_LOOKUP_ID(parent, ps4dshock_devs) != NULL)
		hid_set_report_descr(parent, ps4dshock_rdesc,
		    sizeof(ps4dshock_rdesc));
}

static int
ps4dshock_probe(device_t dev)
{
	struct ps4dshock_softc *sc = device_get_softc(dev);

	hidmap_set_debug_var(&sc->hm, &HID_DEBUG_VAR);
	return (
	    HIDMAP_PROBE(&sc->hm, dev, ps4dshock_devs, ps4dshock_map, NULL)
	);
}

static int
ps4dsacc_probe(device_t dev)
{
	struct ps4dsacc_softc *sc = device_get_softc(dev);

	hidmap_set_debug_var(&sc->hm, &HID_DEBUG_VAR);
	return (
	    HIDMAP_PROBE(&sc->hm, dev, ps4dsacc_devs, ps4dsacc_map, "Sensors")
	);
}

static int
ps4dshead_probe(device_t dev)
{
	struct hidmap *hm = device_get_softc(dev);

	hidmap_set_debug_var(hm, &HID_DEBUG_VAR);
	return (
	    HIDMAP_PROBE(hm, dev, ps4dshead_devs, ps4dshead_map, "Headset")
	);
}

static int
ps4dsmtp_probe(device_t dev)
{
	struct ps4dshock_softc *sc = device_get_softc(dev);

	hidmap_set_debug_var(&sc->hm, &HID_DEBUG_VAR);
	return (
	    HIDMAP_PROBE(&sc->hm, dev, ps4dsmtp_devs, ps4dsmtp_map, "Touchpad")
	);
}

static int
ps4dshock_attach(device_t dev)
{
	struct ps4dshock_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);

	sc->led_state = PS4DS_LED_ON;
	sc->led_color = ps4ds_leds[device_get_unit(dev) % nitems(ps4ds_leds)];
	sc->led_delay_on = 500;	/* 1 Hz */
	sc->led_delay_off = 500;
	ps4dshock_write(sc);

	sx_init(&sc->lock, "ps4dshock");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_state", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_STATE, ps4dshock_sysctl, "I",
	    "LED state: 0 - off, 1 - on, 2 - blinking.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_r", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_COLOR_R, ps4dshock_sysctl, "I",
	    "LED color. Red component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_g", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_COLOR_G, ps4dshock_sysctl, "I",
	    "LED color. Green component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_color_b", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_COLOR_B, ps4dshock_sysctl, "I",
	    "LED color. Blue component.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_delay_on", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_DELAY_ON, ps4dshock_sysctl, "I",
	    "LED blink. On delay, msecs.");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "led_delay_off", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY, sc,
	    PD4DSHOCK_SYSCTL_LED_DELAY_OFF, ps4dshock_sysctl, "I",
	    "LED blink. Off delay, msecs.");

	return (hidmap_attach(&sc->hm));
}

static int
ps4dsacc_attach(device_t dev)
{
	struct ps4dsacc_softc *sc = device_get_softc(dev);
	uint8_t buf[PS4DS_FEATURE_REPORT2_SIZE];
	int error, speed_2x, range_2g;

	/* Read accelerometers and gyroscopes calibration data */
	error = hid_get_report(dev, buf, sizeof(buf), NULL,
	    HID_FEATURE_REPORT, 0x02);
	if (error)
		DPRINTF("get feature report failed, error=%d "
		    "(ignored)\n", error);

	DPRINTFN(5, "calibration data: %*D\n", (int)sizeof(buf), buf, " ");

	/*
	 * Set gyroscope calibration and normalization parameters.
	 * Data values will be normalized to 1/ PS4DS_GYRO_RES_PER_DEG_S
	 * degree/s.
	 */
#define HGETW(w) ((int16_t)((w)[0] | (((uint16_t)((w)[1])) << 8)))
	speed_2x = HGETW(&buf[19]) + HGETW(&buf[21]);
	sc->calib_data[0].usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_RX);
	sc->calib_data[0].code = ABS_RX;
	sc->calib_data[0].range = PS4DS_GYRO_RES_PER_DEG_S * 2048;
	sc->calib_data[0].res = PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[0].bias = HGETW(&buf[1]);
	sc->calib_data[0].sens_numer = speed_2x * PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[0].sens_denom = HGETW(&buf[7]) - HGETW(&buf[9]);
	/* BT case */
	/* sc->calib_data[0].sens_denom = HGETW(&buf[7]) - HGETW(&buf[13]); */

	sc->calib_data[1].usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_RY);
	sc->calib_data[1].code = ABS_RY;
	sc->calib_data[1].range = PS4DS_GYRO_RES_PER_DEG_S * 2048;
	sc->calib_data[1].res = PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[1].bias = HGETW(&buf[3]);
	sc->calib_data[1].sens_numer = speed_2x * PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[1].sens_denom = HGETW(&buf[11]) - HGETW(&buf[13]);
	/* BT case */
	/* sc->calib_data[1].sens_denom = HGETW(&buf[9]) - HGETW(&buf[15]); */

	sc->calib_data[2].usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_RZ);
	sc->calib_data[2].code = ABS_RZ;
	sc->calib_data[2].range = PS4DS_GYRO_RES_PER_DEG_S * 2048;
	sc->calib_data[2].res = PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[2].bias = HGETW(&buf[5]);
	sc->calib_data[2].sens_numer = speed_2x * PS4DS_GYRO_RES_PER_DEG_S;
	sc->calib_data[2].sens_denom = HGETW(&buf[15]) - HGETW(&buf[17]);
	/* BT case */
	/* sc->calib_data[2].sens_denom = HGETW(&buf[11]) - HGETW(&buf[17]); */

	/*
	 * Set accelerometer calibration and normalization parameters.
	 * Data values will be normalized to 1 / PS4DS_ACC_RES_PER_G G.
	 */
	range_2g = HGETW(&buf[23]) - HGETW(&buf[25]);
	sc->calib_data[3].usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X);
	sc->calib_data[3].code = ABS_X;
	sc->calib_data[3].range = PS4DS_ACC_RES_PER_G * 4;
	sc->calib_data[3].res = PS4DS_ACC_RES_PER_G;
	sc->calib_data[3].bias = HGETW(&buf[23]) - range_2g / 2;
	sc->calib_data[3].sens_numer = 2 * PS4DS_ACC_RES_PER_G;
	sc->calib_data[3].sens_denom = range_2g;

	range_2g = HGETW(&buf[27]) - HGETW(&buf[29]);
	sc->calib_data[4].usage =  HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y);
	sc->calib_data[4].code = ABS_Y;
	sc->calib_data[4].range = PS4DS_ACC_RES_PER_G * 4;
	sc->calib_data[4].res = PS4DS_ACC_RES_PER_G;
	sc->calib_data[4].bias = HGETW(&buf[27]) - range_2g / 2;
	sc->calib_data[4].sens_numer = 2 * PS4DS_ACC_RES_PER_G;
	sc->calib_data[4].sens_denom = range_2g;

	range_2g = HGETW(&buf[31]) - HGETW(&buf[33]);
	sc->calib_data[5].usage = HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Z);
	sc->calib_data[5].code = ABS_Z;
	sc->calib_data[5].range = PS4DS_ACC_RES_PER_G * 4;
	sc->calib_data[5].res = PS4DS_ACC_RES_PER_G;
	sc->calib_data[5].bias = HGETW(&buf[31]) - range_2g / 2;
	sc->calib_data[5].sens_numer = 2 * PS4DS_ACC_RES_PER_G;
	sc->calib_data[5].sens_denom = range_2g;

	return (hidmap_attach(&sc->hm));
}

static int
ps4dshead_attach(device_t dev)
{
	return (hidmap_attach(device_get_softc(dev)));
}

static int
ps4dsmtp_attach(device_t dev)
{
	struct ps4dsmtp_softc *sc = device_get_softc(dev);

	return (hidmap_attach(&sc->hm));
}

static int
ps4dshock_detach(device_t dev)
{
	struct ps4dshock_softc *sc = device_get_softc(dev);

	hidmap_detach(&sc->hm);
	sc->led_state = PS4DS_LED_OFF;
	ps4dshock_write(sc);
	sx_destroy(&sc->lock);

	return (0);
}

static int
ps4dsacc_detach(device_t dev)
{
	struct ps4dsacc_softc *sc = device_get_softc(dev);

	return (hidmap_detach(&sc->hm));
}

static int
ps4dshead_detach(device_t dev)
{
	return (hidmap_detach(device_get_softc(dev)));
}

static int
ps4dsmtp_detach(device_t dev)
{
	struct ps4dsmtp_softc *sc = device_get_softc(dev);

	return (hidmap_detach(&sc->hm));
}

static device_method_t ps4dshock_methods[] = {
	DEVMETHOD(device_identify,	ps4dshock_identify),
	DEVMETHOD(device_probe,		ps4dshock_probe),
	DEVMETHOD(device_attach,	ps4dshock_attach),
	DEVMETHOD(device_detach,	ps4dshock_detach),

	DEVMETHOD_END
};
static device_method_t ps4dsacc_methods[] = {
	DEVMETHOD(device_probe,		ps4dsacc_probe),
	DEVMETHOD(device_attach,	ps4dsacc_attach),
	DEVMETHOD(device_detach,	ps4dsacc_detach),

	DEVMETHOD_END
};
static device_method_t ps4dshead_methods[] = {
	DEVMETHOD(device_probe,		ps4dshead_probe),
	DEVMETHOD(device_attach,	ps4dshead_attach),
	DEVMETHOD(device_detach,	ps4dshead_detach),

	DEVMETHOD_END
};
static device_method_t ps4dsmtp_methods[] = {
	DEVMETHOD(device_probe,		ps4dsmtp_probe),
	DEVMETHOD(device_attach,	ps4dsmtp_attach),
	DEVMETHOD(device_detach,	ps4dsmtp_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ps4dsacc, ps4dsacc_driver, ps4dsacc_methods,
    sizeof(struct ps4dsacc_softc));
DRIVER_MODULE(ps4dsacc, hidbus, ps4dsacc_driver, NULL, NULL);
DEFINE_CLASS_0(ps4dshead, ps4dshead_driver, ps4dshead_methods,
    sizeof(struct hidmap));
DRIVER_MODULE(ps4dshead, hidbus, ps4dshead_driver, NULL, NULL);
DEFINE_CLASS_0(ps4dsmtp, ps4dsmtp_driver, ps4dsmtp_methods,
    sizeof(struct ps4dsmtp_softc));
DRIVER_MODULE(ps4dsmtp, hidbus, ps4dsmtp_driver, NULL, NULL);
DEFINE_CLASS_0(ps4dshock, ps4dshock_driver, ps4dshock_methods,
    sizeof(struct ps4dshock_softc));
DRIVER_MODULE(ps4dshock, hidbus, ps4dshock_driver, NULL, NULL);

MODULE_DEPEND(ps4dshock, hid, 1, 1, 1);
MODULE_DEPEND(ps4dshock, hidbus, 1, 1, 1);
MODULE_DEPEND(ps4dshock, hidmap, 1, 1, 1);
MODULE_DEPEND(ps4dshock, hgame, 1, 1, 1);
MODULE_DEPEND(ps4dshock, evdev, 1, 1, 1);
MODULE_VERSION(ps4dshock, 1);
HID_PNP_INFO(ps4dshock_devs);
