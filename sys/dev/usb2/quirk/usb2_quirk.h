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

#ifndef _USB2_QUIRK_H_
#define	_USB2_QUIRK_H_

/* NOTE: UQ_NONE is not a valid quirk */

#define	USB_QUIRK(m,n)					\
  m(n, UQ_NONE)						\
  /* left and right sound channels are swapped */	\
  m(n, UQ_AUDIO_SWAP_LR)				\
  /* input is async despite claim of adaptive */	\
  m(n, UQ_AU_INP_ASYNC)					\
  /* don't adjust for fractional samples */		\
  m(n, UQ_AU_NO_FRAC)					\
  /* audio device has broken extension unit */		\
  m(n, UQ_AU_NO_XU)					\
  /* bad audio spec version number */			\
  m(n, UQ_BAD_ADC)					\
  /* device claims audio class, but isn't */		\
  m(n, UQ_BAD_AUDIO)					\
  /* printer has broken bidir mode */			\
  m(n, UQ_BROKEN_BIDIR)					\
  /* device is bus powered, despite claim */		\
  m(n, UQ_BUS_POWERED)					\
  /* device should be ignored by hid class */		\
  m(n, UQ_HID_IGNORE)					\
  /* device should be ignored by kbd class */		\
  m(n, UQ_KBD_IGNORE)					\
  /* doesn't identify properly */			\
  m(n, UQ_MS_BAD_CLASS)					\
  /* mouse sends an unknown leading byte */		\
  m(n, UQ_MS_LEADING_BYTE)				\
  /* mouse has Z-axis reversed */			\
  m(n, UQ_MS_REVZ)					\
  /* string descriptors are broken */			\
  m(n, UQ_NO_STRINGS)					\
  /* device needs clear endpoint stall */		\
  m(n, UQ_OPEN_CLEARSTALL)				\
  /* hub lies about power status */			\
  m(n, UQ_POWER_CLAIM)					\
  /* spurious mouse button up events */			\
  m(n, UQ_SPUR_BUT_UP)					\
  /* has some Unicode strings swapped */		\
  m(n, UQ_SWAP_UNICODE)					\
  /* select configuration index 1 by default */		\
  m(n, UQ_CFG_INDEX_1)					\
  /* select configuration index 2 by default */		\
  m(n, UQ_CFG_INDEX_2)					\
  /* select configuration index 3 by default */		\
  m(n, UQ_CFG_INDEX_3)					\
  /* select configuration index 4 by default */		\
  m(n, UQ_CFG_INDEX_4)					\
  /* select configuration index 0 by default */		\
  m(n, UQ_CFG_INDEX_0)

USB_MAKE_ENUM(USB_QUIRK);

#endif					/* _USB2_QUIRK_H_ */
