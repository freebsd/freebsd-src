/* $FreeBSD: src/sys/alpha/tlsb/dwlpxvar.h,v 1.1.2.1 2000/03/27 18:32:39 mjacob Exp $ */
/*
 * Copyright (c) 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#define	DWLPX_NONE	0
#define	DWLPX_SG32K	1
#define	DWLPX_SG64K	2
#define	DWLPX_SG128K	3

/*
 * Each DWLPX supports up to 15 devices, 12 of which are PCI slots.
 *
 * Since the STD I/O modules in slots 12-14 are really a PCI-EISA
 * bridge, we'll punt on those for the moment.
 */
#define	DWLPX_MAXDEV	12

/*
 * There are 5 possible slots that can have I/O boards, and for each
 * one there are 4 possible hoses. To cover them all, we'd have to
 * reserve 5 bits of selector out our current 32 bit cookie we use
 * for primary PCI address spaces. It turns out that we *just* have
 * enough bits for this (see drawing in dwlpxreg.h)
 */

#define	DWLPX_NIONODE	5
#define	DWLPX_NHOSE	4

/*
 * Interrupt Cookie for DWLPX vectors.
 *
 * Bits 0..3	PCI Slot (0..11)
 * Bits 4..7	I/O Hose (0..3)
 * Bits 8..11	I/O Node (0..4)
 * Bit	15	Constant 1
 */
#define	DWLPX_VEC_MARK	(1<<15)
#define	DWLPX_MVEC(ionode, hose, pcislot)	\
	(DWLPX_VEC_MARK | (ionode << 8) | (hose << 4) | (pcislot))

#define	DWLPX_MVEC_IONODE(cookie)	\
	((((u_int64_t)(cookie)) >> 8) & 0xf)
#define	DWLPX_MVEC_HOSE(cookie)	\
	((((u_int64_t)(cookie)) >> 4) & 0xf)
#define	DWLPX_MVEC_PCISLOT(cookie)	\
	(((u_int64_t)(cookie)) & 0xf)

/*
 * DWLPX Error Interrupt
 */
#define	DWLPX_VEC_EMARK	(1<<14)
#define	DWLPX_ERRVEC(ionode, hose)	\
	(DWLPX_VEC_EMARK | (ionode << 8) | (hose << 4))

/*
 * Default values to put into DWLPX IMASK register(s)
 */
#define	DWLPX_IMASK_DFLT	\
	(1 << 24) |	/* IPL 17 for error interrupts */ \
	(1 << 17) |	/* IPL 14 for device interrupts */ \
	(1 << 16)	/* Enable Error Interrupts */

#define	DWLPX_BASE(node, hose)					\
	    ((((unsigned long)(node - 4))	<< 36) |	\
	     (((unsigned long)(hose))		<< 34) |	\
	     (1LL				<< 39))
