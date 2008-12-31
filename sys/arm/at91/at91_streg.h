/*-
 * Copyright (c) 2005 M. Warner Losh.  All rights reserved.
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

/* $FreeBSD: src/sys/arm/at91/at91_streg.h,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef ARM_AT91_AT91STREG_H
#define ARM_AT91_AT91STREG_H

#define ST_CR		0x00 /* Control register */
#define ST_PIMR		0x04 /* Period interval mode register */
#define ST_WDMR		0x08 /* Watchdog mode register */
#define ST_RTMR		0x0c /* Real-time mode register */
#define ST_SR		0x10 /* Status register */
#define ST_IER		0x14 /* Interrupt enable register */
#define ST_IDR		0x18 /* Interrupt disable register */
#define ST_IMR		0x1c /* Interrupt mask register */
#define ST_RTAR		0x20 /* Real-time alarm register */
#define	ST_CRTR		0x24 /* Current real-time register */

/* ST_CR */
#define ST_CR_WDRST	(1U << 0) /* WDRST: Watchdog Timer Restart */

/* ST_WDMR */
#define ST_WDMR_EXTEN	(1U << 17) /* EXTEN: External Signal Assert Enable */
#define ST_WDMR_RSTEN	(1U << 16) /* RSTEN: Reset Enable */

/* ST_SR, ST_IER, ST_IDR, ST_IMR */
#define ST_SR_PITS	(1U << 0) /* PITS: Period Interval Timer Status */
#define ST_SR_WDOVF	(1U << 1) /* WDOVF: Watchdog Overflow */
#define ST_SR_RTTINC	(1U << 2) /* RTTINC: Real-time Timer Increment */
#define ST_SR_ALMS	(1U << 3) /* ALMS: Alarm Status */

/* ST_CRTR */
#define ST_CRTR_MASK	0xfffff /* 20-bit counter */

#endif /* ARM_AT91_AT91STREG_H */
