/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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

/*
 * TODO: sprom
 * TODO: implement dma translation bits (if needed for system bus)
 */

#ifndef _SIBA_SIBAREG_H_
#define _SIBA_SIBAREG_H_

#define SIBA_CORE_LEN		0x00001000	/* Size of cfg per core */
#define SIBA_CFG_END		0x00010000	/* Upper bound of cfg space */
#define SIBA_MAX_CORES		(SIBA_CFG_END/SIBA_CORE_LEN)	/* #max cores */

/* offset of high ID register */
#define SIBA_CORE_IDLO		0x00000ff8
#define SIBA_CORE_IDHI		0x00000ffc

/*
 * Offsets of ChipCommon core registers.
 * XXX: move to siba_cc
 */
#define SIBA_CC_UART0	0x00000300	/* offset of UART0 */
#define SIBA_CC_UART1	0x00000400	/* offset of UART1 */

#define SIBA_CC_CCID 0x0000
#define  SIBA_CC_IDMASK 0x0000FFFF
#define  SIBA_CC_REVMASK 0x000F0000
#define  SIBA_CC_REVSHIFT 16
#define  SIBA_CC_PACKMASK 0x00F00000
#define  SIBA_CC_PACKSHIFT 20
#define  SIBA_CC_NRCORESMASK 0x0F000000
#define  SIBA_CC_NRCORESSHIFT 24

#define  SIBA_IDHIGH_RCLO	0x0000000F /* Revision Code (low part) */
#define  SIBA_IDHIGH_CC		0x00008FF0 /* Core Code */
#define  SIBA_IDHIGH_CC_SHIFT	4
#define  SIBA_IDHIGH_RCHI	0x00007000 /* Revision Code (high part) */
#define  SIBA_IDHIGH_RCHI_SHIFT	8
#define  SIBA_IDHIGH_VC		0xFFFF0000 /* Vendor Code */
#define  SIBA_IDHIGH_VC_SHIFT	16

#define SIBA_CCID_BCM4710	0x4710
#define SIBA_CCID_BCM4704	0x4704
#define SIBA_CCID_SENTRY5	0x5365

#endif /* _SIBA_SIBAREG_H_ */
