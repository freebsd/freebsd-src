/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

/*
 * Screening of all content of this file except HID_QUIRK_LIST is a kind of
 * hack that allows multiple HID_QUIRK_LIST inclusion with different HQ()
 * wrappers. That save us splitting hidquirk.h on two header files.
 */
#ifndef HQ
#ifndef _HID_QUIRK_H_
#define	_HID_QUIRK_H_
#endif

/*
 * Keep in sync with share/man/man4/hidquirk.4
 */
#define	HID_QUIRK_LIST(...)						\
	HQ(NONE),		/* not a valid quirk */			\
									\
	HQ(MATCH_VENDOR_ONLY),	/* match quirk on vendor only */	\
									\
	/* Autoquirks */						\
	HQ(HAS_KBD_BOOTPROTO),	/* device supports keyboard boot protocol */ \
	HQ(HAS_MS_BOOTPROTO),	/* device supports mouse boot protocol */ \
	HQ(IS_XBOX360GP), 	/* device is XBox 360 GamePad */	\
	HQ(NOWRITE),		/* device does not support writes */	\
	HQ(IICHID_SAMPLING),	/* IIC backend runs in sampling mode */	\
	HQ(NO_READAHEAD),	/* Disable interrupt after one report */\
									\
	/* Various quirks */						\
	HQ(HID_IGNORE),		/* device should be ignored by hid class */ \
	HQ(KBD_BOOTPROTO),	/* device should set the boot protocol */ \
	HQ(MS_BOOTPROTO),	/* device should set the boot protocol */ \
	HQ(MS_BAD_CLASS),	/* doesn't identify properly */		\
	HQ(MS_LEADING_BYTE),	/* mouse sends an unknown leading byte */ \
	HQ(MS_REVZ),		/* mouse has Z-axis reversed */		\
	HQ(MS_VENDOR_BTN),	/* mouse has buttons in vendor usage page */ \
	HQ(SPUR_BUT_UP),	/* spurious mouse button up events */	\
	HQ(MT_TIMESTAMP)	/* Multitouch device exports HW timestamps */

#ifndef	HQ
#define	HQ(x)	HQ_##x
enum {
	HID_QUIRK_LIST(),
	HID_QUIRK_MAX
};
#undef HQ

#endif					/* _HID_QUIRK_H_ */
#endif					/* HQ */
