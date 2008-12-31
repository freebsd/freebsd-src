/*	$NetBSD: sa11x0_ostreg.h,v 1.1 2001/07/08 23:37:53 rjs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 *
 * $FreeBSD: src/sys/arm/sa11x0/sa11x0_ostreg.h,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

/*
 * SA-11x0 OS Timer Register 
 */

/* OS Timer Match Register */
#define SAOST_MR0	0x00
#define SAOST_MR1	0x04
#define SAOST_MR2	0x08
#define SAOST_MR3	0x0C

/* OS Timer Count Register */
#define SAOST_CR	0x10

/* OS Timer Status Register */
#define SAOST_SR	0x14
#define SR_CH0		(1<<0)
#define SR_CH1		(1<<1)
#define SR_CH2		(1<<2)
#define SR_CH3		(1<<3)

/* OS Timer Watchdog Match Enable Register */
#define SAOST_WR	0x18

/* OS Timer Interrupt Enable Register */
#define SAOST_IR	0x1C

/*
 * SA-1110 Real Time Clock
 */

/* RTC Alarm Register */
#define SARTC_AR	0x00

/* RTC Counter Register */
#define SARTC_CR	0x04

/* RTC Trim Register */
#define SARTC_TR	0x08

/* RTC Status Register */
#define SARTC_SR	0x0C

/* end of sa11x0_ostreg.h */
