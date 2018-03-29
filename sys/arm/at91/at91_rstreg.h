/*-
 * Copyright (c) 2009 Greg Ansley.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_RSTREG_H
#define ARM_AT91_AT91_RSTREG_H

#define	RST_CR		0x0	/* Control Register */
#define	RST_SR		0x4	/* Status Register */
#define	RST_MR		0x8	/* Mode Register */

/* RST_CR */
#define	RST_CR_PROCRST		(1<<0)
#define	RST_CR_PERRST		(1<<2)
#define	RST_CR_EXTRST		(1<<3)
#define	RST_CR_KEY		(0xa5<<24)

/* RST_SR */
#define	RST_SR_SRCMP		(1<<17)	/* Software Reset in progress */	
#define	RST_SR_NRSTL		(1<<16)	/* NRST pin level at MCK */	
#define	RST_SR_URSTS		(1<<0)	/* NRST pin has been active */	

#define	RST_SR_RST_POW		(0<<8)	/* General (Power On) reset */	
#define	RST_SR_RST_WAKE		(1<<8)	/* Wake-up reset */
#define	RST_SR_RST_WDT		(2<<8)	/* Watchdog reset */
#define	RST_SR_RST_SOFT		(3<<8)	/* Software  reset */
#define	RST_SR_RST_USR		(4<<8)	/* User (External) reset */
#define	RST_SR_RST_MASK		(7<<8)	/* User (External) reset */

/* RST_MR */
#define	RST_MR_URSTEN		(1<<0)	/* User reset enable */	
#define	RST_MR_URSIEN		(1<<4)	/* User interrupt enable */	
#define	RST_MR_ERSTL(x)		((x)<<8) /* External reset length */	
#define	RST_MR_KEY		(0xa5<<24)

#ifndef __ASSEMBLER__
void at91_rst_cpu_reset(void);
#endif

#endif /* ARM_AT91_AT91_RSTREG_H */
