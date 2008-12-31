/*-
 * Copyright (c) 2000
 * Intel Corporation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 
 *    This product includes software developed by Intel Corporation and
 *    its contributors.
 * 
 * 4. Neither the name of Intel Corporation or its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL INTEL CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/ia64/include/setjmp.h,v 1.12.28.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_SETJMP_H_
#define	_MACHINE_SETJMP_H_

#include <sys/cdefs.h>

#if __BSD_VISIBLE
#define	JMPBUF_ADDR_OF(buf, item)	((unsigned long)((char *)buf + item))

#define	J_UNAT		0
#define	J_NATS		0x8
#define	J_PFS		0x10
#define	J_BSP		0x18
#define	J_RNAT		0x20
#define	J_PREDS		0x28
#define	J_LC		0x30
#define	J_R4		0x38
#define	J_R5		0x40
#define	J_R6		0x48
#define	J_R7		0x50
#define	J_SP		0x58
#define	J_F2		0x60
#define	J_F3		0x70
#define	J_F4		0x80
#define	J_F5		0x90
#define	J_F16		0xa0
#define	J_F17		0xb0
#define	J_F18		0xc0
#define	J_F19		0xd0
#define	J_F20		0xe0
#define	J_F21		0xf0
#define	J_F22		0x100
#define	J_F23		0x110
#define	J_F24		0x120
#define	J_F25		0x130
#define	J_F26		0x140
#define	J_F27		0x150
#define	J_F28		0x160
#define	J_F29		0x170
#define	J_F30		0x180
#define	J_F31		0x190
#define	J_FPSR		0x1a0
#define	J_B0		0x1a8
#define	J_B1		0x1b0
#define	J_B2		0x1b8
#define	J_B3		0x1c0
#define	J_B4		0x1c8
#define	J_B5		0x1d0
#define	J_SIGMASK	0x1d8
#define	J_SIGSET	0x1e0
#endif /* __BSD_VISIBLE */

#define	_JBLEN		0x20			/* Size in long doubles */

/*
 * XXX this check is wrong, since LOCORE is in the application namespace and
 * applications shouldn't be able to affect the implementation.  One workaround
 * would be to only check LOCORE if _KERNEL is defined, but unfortunately
 * LOCORE is used outside of the kernel.  The best solution would be to rename
 * LOCORE to _LOCORE, so that it can be used in userland to safely affect the
 * implementation.
 */
#ifndef LOCORE

/*
 * jmp_buf and sigjmp_buf are encapsulated in different structs to force
 * compile-time diagnostics for mismatches.  The structs are the same
 * internally to avoid some run-time errors for mismatches.
 */
#if __BSD_VISIBLE || __POSIX_VISIBLE || __XSI_VISIBLE
struct _sigjmp_buf {
	long double buf[_JBLEN];
};
typedef struct _sigjmp_buf sigjmp_buf[1];
#endif

struct _jmp_buf {
	long double buf[_JBLEN];
};
typedef struct _jmp_buf	jmp_buf[1];

#ifdef _KERNEL
#ifdef CTASSERT
CTASSERT(sizeof(struct _jmp_buf) == 512);
#endif
#endif

#endif /* !LOCORE */

#endif /* !_MACHINE_SETJMP_H_ */
