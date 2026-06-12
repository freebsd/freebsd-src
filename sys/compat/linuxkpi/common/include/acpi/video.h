/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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

#ifndef _LINUXKPI_ACPI_VIDEO_H_
#define _LINUXKPI_ACPI_VIDEO_H_

#include <sys/types.h>
#include <sys/errno.h>

#define	ACPI_VIDEO_CLASS	"video"

#define	ACPI_VIDEO_DISPLAY_CRT	1
#define	ACPI_VIDEO_DISPLAY_TV	2
#define	ACPI_VIDEO_DISPLAY_DVI	3
#define	ACPI_VIDEO_DISPLAY_LCD	4

#define	ACPI_VIDEO_DISPLAY_LEGACY_MONITOR	0x0100
#define	ACPI_VIDEO_DISPLAY_LEGACY_PANEL		0x0110
#define	ACPI_VIDEO_DISPLAY_LEGACY_TV		0x0200

#define	ACPI_VIDEO_NOTIFY_SWITCH		0x80
#define	ACPI_VIDEO_NOTIFY_PROBE			0x81
#define	ACPI_VIDEO_NOTIFY_CYCLE			0x82
#define	ACPI_VIDEO_NOTIFY_NEXT_OUTPUT		0x83
#define	ACPI_VIDEO_NOTIFY_PREV_OUTPUT		0x84
#define	ACPI_VIDEO_NOTIFY_CYCLE_BRIGHTNESS	0x85
#define	ACPI_VIDEO_NOTIFY_INC_BRIGHTNESS	0x86
#define	ACPI_VIDEO_NOTIFY_DEC_BRIGHTNESS	0x87
#define	ACPI_VIDEO_NOTIFY_ZERO_BRIGHTNESS	0x88
#define	ACPI_VIDEO_NOTIFY_DISPLAY_OFF		0x89

static inline int
acpi_video_register(void)
{

	return (-ENODEV);
}

static inline void
acpi_video_unregister(void)
{
}

static inline void
acpi_video_register_backlight(void)
{
}

static inline bool
acpi_video_backlight_use_native(void)
{
	return (true);
}

#endif	/* _LINUXKPI_ACPI_VIDEO_H_ */
