/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)rtc.h	7.1 (Berkeley) 5/12/91
 *	$Id: rtc.h,v 1.3 1993/11/07 17:44:34 wollman Exp $
 */

#ifndef _I386_ISA_RTC_H_
#define _I386_ISA_RTC_H_ 1

/*
 * RTC Register locations
 */

#define RTC_SEC		0x00	/* seconds */
#define RTC_SECALRM	0x01	/* seconds alarm */
#define RTC_MIN		0x02	/* minutes */
#define RTC_MINALRM	0x03	/* minutes alarm */
#define RTC_HRS		0x04	/* hours */
#define RTC_HRSALRM	0x05	/* hours alarm */
#define RTC_WDAY	0x06	/* week day */
#define RTC_DAY		0x07	/* day of month */
#define RTC_MONTH	0x08	/* month of year */
#define RTC_YEAR	0x09	/* month of year */
#define RTC_STATUSA	0x0a	/* status register A */
#define  RTCSA_TUP	 0x80	/* time update, don't look now */

#define RTC_STATUSB	0x0b	/* status register B */

#define RTC_INTR	0x0c	/* status register C (R) interrupt source */
#define  RTCIR_UPDATE	 0x10	/* update intr */
#define  RTCIR_ALARM	 0x20	/* alarm intr */
#define  RTCIR_PERIOD	 0x40	/* periodic intr */
#define  RTCIR_INT	 0x80	/* interrupt output signal */

#define RTC_STATUSD	0x0d	/* status register D (R) Lost Power */
#define  RTCSD_PWR	 0x80	/* clock lost power */

#define RTC_DIAG	0x0e	/* status register E - bios diagnostic */
#define RTCDG_BITS	"\020\010clock_battery\007ROM_cksum\006config_unit\005memory_size\004fixed_disk\003invalid_time"

#define RTC_RESET	0x0f	/* status register F - reset code byte */
#define	 RTCRS_RST	 0x00		/* normal reset */
#define	 RTCRS_LOAD	 0x04		/* load system */

#define RTC_FDISKETTE	0x10	/* diskette drive type in upper/lower nibble */
#define	 RTCFDT_NONE	 0		/* none present */
#define	 RTCFDT_360K	 0x10		/* 360K */
#define	 RTCFDT_12M	 0x20		/* 1.2M */
#define  RTCFDT_720K     0x30           /* 720K */
#define	 RTCFDT_144M	 0x40		/* 1.44M */

#define RTC_BASELO	0x15	/* low byte of basemem size */
#define RTC_BASEHI	0x16	/* high byte of basemem size */
#define RTC_EXTLO	0x17	/* low byte of extended mem size */
#define RTC_EXTHI	0x18	/* low byte of extended mem size */

#define RTC_CENTURY	0x32	/* current century - please increment in Dec99*/
#endif /* _I386_ISA_RTC_H_ */
