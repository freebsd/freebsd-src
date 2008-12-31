/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: trap.h,v 1.7 2002/02/22 13:51:40 kleink Exp $
 * $FreeBSD: src/sys/powerpc/include/trap.h,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $ 
 */

#ifndef	_POWERPC_TRAP_H_
#define	_POWERPC_TRAP_H_

#define	EXC_RSVD	0x0000		/* Reserved */
#define	EXC_RST		0x0100		/* Reset; all but IBM4xx */
#define	EXC_MCHK	0x0200		/* Machine Check */
#define	EXC_DSI		0x0300		/* Data Storage Interrupt */
#define	EXC_ISI		0x0400		/* Instruction Storage Interrupt */
#define	EXC_EXI		0x0500		/* External Interrupt */
#define	EXC_ALI		0x0600		/* Alignment Interrupt */
#define	EXC_PGM		0x0700		/* Program Interrupt */
#define	EXC_FPU		0x0800		/* Floating-point Unavailable */
#define	EXC_DECR	0x0900		/* Decrementer Interrupt */
#define	EXC_SC		0x0c00		/* System Call */
#define	EXC_TRC		0x0d00		/* Trace */
#define	EXC_FPA		0x0e00		/* Floating-point Assist */

/* The following is only available on the 601: */
#define	EXC_RUNMODETRC	0x2000		/* Run Mode/Trace Exception */

/* The following are only available on 7400(G4): */
#define	EXC_VEC		0x0f20		/* AltiVec Unavailable */
#define	EXC_VECAST	0x1600		/* AltiVec Assist */

/* The following are only available on 604/750/7400: */
#define	EXC_PERF	0x0f00		/* Performance Monitoring */
#define	EXC_BPT		0x1300		/* Instruction Breakpoint */
#define	EXC_SMI		0x1400		/* System Managment Interrupt */

/* The following are only available on 750/7400: */
#define	EXC_THRM	0x1700		/* Thermal Management Interrupt */

/* And these are only on the 603: */
#define	EXC_IMISS	0x1000		/* Instruction translation miss */
#define	EXC_DLMISS	0x1100		/* Data load translation miss */
#define	EXC_DSMISS	0x1200		/* Data store translation miss */

/* The following are only available on 405 (and 403?) */
#define	EXC_CII		0x0100		/* Critical Input Interrupt */
#define	EXC_PIT		0x1000		/* Programmable Interval Timer */
#define	EXC_FIT		0x1010		/* Fixed Interval Timer */
#define	EXC_WDOG	0x1020		/* Watchdog Timer */
#define	EXC_DTMISS	0x1100		/* Data TLB Miss */
#define	EXC_ITMISS	0x1200		/* Instruction TLB Miss */
#define	EXC_DEBUG	0x2000		/* Debug trap */

#define	EXC_LAST	0x2f00		/* Last possible exception vector */

#define	EXC_AST		0x3000		/* Fake AST vector */

/* Trap was in user mode */
#define	EXC_USER	0x10000


/*
 * EXC_ALI sets bits in the DSISR and DAR to provide enough
 * information to recover from the unaligned access without needing to
 * parse the offending instruction. This includes certain bits of the
 * opcode, and information about what registers are used. The opcode
 * indicator values below come from Appendix F of Book III of "The
 * PowerPC Architecture".
 */

#define EXC_ALI_OPCODE_INDICATOR(dsisr) ((dsisr >> 10) & 0x7f)
#define EXC_ALI_LFD	0x09
#define EXC_ALI_STFD	0x0b

/* Macros to extract register information */
#define EXC_ALI_RST(dsisr) ((dsisr >> 5) & 0x1f)   /* source or target */
#define EXC_ALI_RA(dsisr) (dsisr & 0x1f)

#ifndef LOCORE

struct trapframe;
void    trap(struct trapframe *);

#endif

#endif	/* _POWERPC_TRAP_H_ */
