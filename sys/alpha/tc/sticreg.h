/* $FreeBSD: src/sys/alpha/tc/sticreg.h,v 1.2 1999/08/28 00:39:10 peter Exp $ */
/*	$NetBSD: sticreg.h,v 1.1 1997/11/08 07:27:50 jonathan Exp $	*/

/*
 * Copyright (c) 1997 Jonathan Stone
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/* 
 * Register definitions for the pixelstamp and stamp interface  chip (STIC)
 * used in PMAG-C 2-d and PMAG-D 3-d accelerated TurboChannel framebuffers.
 */

#ifndef	_TC_STICREG_H_
#define	_TC_STICREG_H_

struct stic_regs {
	volatile int32_t	stic__pad0, __pad1;
	volatile int32_t        hsync;
	volatile int32_t        hsync2;
	volatile int32_t        hblank;
	volatile int32_t        vsync;
	volatile int32_t        vblank;
	volatile int32_t        vtest;
	volatile int32_t        ipdvint;
	volatile int32_t	stic__pad2;
	volatile int32_t        sticsr;
	volatile int32_t        busdat;
	volatile int32_t        busadr;
	volatile int32_t        stic__pad3;
	volatile int32_t        buscsr;
	volatile int32_t        modcl;
};

#define STICADDR(x) ((volatile struct stic_regs*) (x))


/*
 * Bit definitions for stic_regs.stic_csr.
 * these appear to exactly what the PROM tests use.
 */
#define STIC_CSR_TSTFNC		0x00000003
# define STIC_CSR_TSTFNC_NORMAL	0
# define STIC_CSR_TSTFNC_PARITY	1
# define STIC_CSR_TSTFNC_CNTPIX	2
# define STIC_CSR_TSTFNC_TSTDAC	3
#define STIC_CSR_CHECKPAR	0x00000004
#define STIC_CSR_STARTVT	0x00000010
#define STIC_CSR_START		0x00000020
#define STIC_CSR_RESET		0x00000040
#define STIC_CSR_STARTST	0x00000080

/*
 * Bit definitions for stic_regs.int.
 * Three four-bit wide fields, for error (E), vertical-blank (V), and 
 * packetbuf-done (P) intererupts, respectively. 
 * The low-order three bits of each field are enable, requested,
 * and acknowledge bits. The top bit of each field is unused.
 */
#define STIC_INT_E_EN		0x00000001
#define STIC_INT_E		0x00000002
#define STIC_INT_E_WE		0x00000004

#define STIC_INT_V_EN		0x00000100
#define STIC_INT_V		0x00000200
#define STIC_INT_V_WE		0x00000400

#define STIC_INT_P_EN		0x00010000
#define STIC_INT_P		0x00020000
#define STIC_INT_P_WE		0x00040000

#define STIC_INT_WE	(STIC_INT_E_WE|STIC_INT_V_WE|STIC_INT_PE_WE)
#define STIC_INT_CLR	(STIC_INT_E_EN|STIC_INT_V_EN|STIC_INT_P_EN)

#endif	/* _TC_STICREG_H_ */
