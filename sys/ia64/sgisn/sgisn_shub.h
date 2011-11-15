/*-
 * Copyright (c) 2011 Marcel Moolenaar
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
 *
 * $FreeBSD$
 */

#ifndef _IA64_SGISN_SHUB_H_
#define	_IA64_SGISN_SHUB_H_

#define	SHUB_MMR_SIZE		(1 << 32)

#define	SHUB_MMR_IPI		0x10000380
#define	SHUB_MMR_RTC1_ICFG	0x10001480
#define	SHUB_MMR_RTC1_IENA	0x10001500
#define	SHUB_MMR_RTC2_ICFG	0x10001580
#define	SHUB_MMR_RTC2_IENA	0x10001600
#define	SHUB_MMR_RTC3_ICFG	0x10001680
#define	SHUB_MMR_RTC3_IENA	0x10001700
#define	SHUB_MMR_EVENT		0x10010000
#define	SHUB_MMR_EVENT_WR	0x10010008
#define	SHUB_MMR_IPI_ACC	0x10060480
#define	SHUB_MMR_ID		0x10060580
#define	SHUB_MMR_PTC_CFG0	0x101a0000
#define	SHUB_MMR_PTC_CFG1	0x101a0080
#define	SHUB_MMR_RTC		0x101c0000
#define	SHUB_MMR_PIO_WSTAT0	0x20070200
#define	SHUB_MMR_PIO_WSTAT1	0x20070280
#define	SHUB_MMR_PTC		0x70000000

#endif /* _IA64_SGISN_SHUB_H_ */
