/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#ifndef _USB_QUIRK_H_
#define	_USB_QUIRK_H_

/* NOTE: UQ_NONE is not a valid quirk */
enum {	/* keep in sync with usb_quirk_str table */
	UQ_NONE,
	UQ_AUDIO_SWAP_LR,	/* left and right sound channels are swapped */
	UQ_AU_INP_ASYNC,	/* input is async despite claim of adaptive */
	UQ_AU_NO_FRAC,		/* don't adjust for fractional samples */
	UQ_AU_NO_XU,		/* audio device has broken extension unit */
	UQ_BAD_ADC,		/* bad audio spec version number */
	UQ_BAD_AUDIO,		/* device claims audio class, but isn't */
	UQ_BROKEN_BIDIR,	/* printer has broken bidir mode */
	UQ_BUS_POWERED,		/* device is bus powered, despite claim */
	UQ_HID_IGNORE,		/* device should be ignored by hid class */
	UQ_KBD_IGNORE,		/* device should be ignored by kbd class */
	UQ_MS_BAD_CLASS,	/* doesn't identify properly */
	UQ_MS_LEADING_BYTE,	/* mouse sends an unknown leading byte */
	UQ_MS_REVZ,		/* mouse has Z-axis reversed */
	UQ_NO_STRINGS,		/* string descriptors are broken */
	UQ_OPEN_CLEARSTALL,	/* device needs clear endpoint stall */
	UQ_POWER_CLAIM,		/* hub lies about power status */
	UQ_SPUR_BUT_UP,		/* spurious mouse button up events */
	UQ_SWAP_UNICODE,	/* has some Unicode strings swapped */
	UQ_CFG_INDEX_1,		/* select configuration index 1 by default */
	UQ_CFG_INDEX_2,		/* select configuration index 2 by default */
	UQ_CFG_INDEX_3,		/* select configuration index 3 by default */
	UQ_CFG_INDEX_4,		/* select configuration index 4 by default */
	UQ_CFG_INDEX_0,		/* select configuration index 0 by default */
	UQ_ASSUME_CM_OVER_DATA,	/* modem device breaks on cm over data */
	USB_QUIRK_MAX
};

uint8_t	usb_test_quirk(const struct usb_attach_arg *uaa, uint16_t quirk);

#endif					/* _USB_QUIRK_H_ */
