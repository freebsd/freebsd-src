/*	$NetBSD: smc90cx6reg.h,v 1.7 1999/02/16 23:34:13 is Exp $ */
/*	$FreeBSD$ */

/*-
 * Copyright (c) 1994, 1995, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ignatios Souvatzis.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 */

/*
 * chip offsets and bits for the SMC Arcnet chipset.
 */

#ifndef _SMC90CXVAR_H_
#define _SMC90CXVAR_H_

#define CM_IO_PORTS 16

/* register offsets */

#define CMSTAT	0
#define	CMCMD	1
#define CMRESET 8

/* memory offsets */
#define CMCHECKBYTE 0
#define CMMACOFF 1

#define CM_TXDIS	0x01
#define CM_RXDIS	0x02
#define CM_TX(x)	(0x03 | ((x)<<3))
#define CM_RX(x)	(0x04 | ((x)<<3))
#define CM_RXBC(x)	(0x84 | ((x)<<3))

#define CM_CONF(x)	(0x05 | (x))
#define CLR_POR		0x08
#define CLR_RECONFIG	0x10

#define CM_CLR(x)	(0x06 | (x))
#define CONF_LONG	0x08
#define CONF_SHORT	0x00

/*
 * These are not in the COM90C65 docs. Derived from the arcnet.asm
 * packet driver by Philippe Prindeville and Russel Nelson.
 */

#define CM_LDTST(x)	(0x07 | (x))
#define TEST_ON		0x08
#define TEST_OFF	0x00

#define CM_TA		1	/* int mask also */
#define CM_TMA		2
#define CM_RECON	4	/* int mask also */
#define CM_TEST	8		/* not in the COM90C65 docs (see above) */
#define CM_POR		0x10	/* non maskable interrupt */
#define CM_ET1		0x20	/* timeout value bits, normally 1 */
#define CM_ET2		0x40	/* timeout value bits, normally 1 */
#define CM_RI		0x80	/* int mask also */

#endif
