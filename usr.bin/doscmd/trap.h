/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI trap.h,v 2.2 1996/04/08 19:33:09 bostic Exp
 *
 * $FreeBSD$
 */

#define	CLI	0xfa
#define	STI	0xfb
#define	PUSHF	0x9c
#define	POPF	0x9d
#define	INTn	0xcd
#define	TRACETRAP	0xcc
#define	IRET	0xcf
#define	LOCK	0xf0
#define	HLT	0xf4

#define	OPSIZ	0x66
#define	REPNZ	0xf2
#define	REPZ	0xf3

#define	INd	0xe4
#define	INdX	0xe5
#define	OUTd	0xe6
#define	OUTdX	0xe7

#define	IN	0xec
#define	INX	0xed
#define	OUT	0xee
#define	OUTX	0xef

#define	INSB	0x6c
#define	INSW	0x6d
#define	OUTSB	0x6e
#define	OUTSW	0x6f

#define	IOFS	0x64
#define	IOGS	0x65

#define	TWOBYTE	0x0f
#define	 LAR	0x02

#define	AC_P	0x8000			/* Present */
#define	AC_P0	0x0000			/* Priv Level 0 */
#define	AC_P1	0x2000			/* Priv Level 1 */
#define	AC_P2	0x4000			/* Priv Level 2 */
#define	AC_P3	0x6000			/* Priv Level 3 */
#define AC_S	0x1000			/* Memory Segment */
#define	AC_RO	0x0000			/* Read Only */
#define	AC_RW	0x0200			/* Read Write */
#define	AC_RWE	0x0600			/* Read Write Expand Down */
#define	AC_EX	0x0800			/* Execute Only */
#define	AC_EXR	0x0a00			/* Execute Readable */
#define	AC_EXC	0x0c00			/* Execute Only Conforming */
#define	AC_EXRC	0x0e00			/* Execute Readable Conforming */
#define	AC_A	0x0100			/* Accessed */

extern void	fake_int(regcontext_t *REGS, int);
extern void	sigtrap(struct sigframe *sf);
extern void	sigtrace(struct sigframe *sf);
extern void	sigalrm(struct sigframe *sf);
extern void	sigill(struct sigframe *sf);
extern void	sigfpe(struct sigframe *sf);
extern void	sigsegv(struct sigframe *sf);
extern void	breakpoint(struct sigframe *sf);
#ifdef USE_VM86
extern void	sigurg(struct sigframe *sf);
#else
extern void	sigbus(struct sigframe *sf);
#endif
