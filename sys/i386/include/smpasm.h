/*
 * Copyright (c) 1996, by Peter Wemm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$Id: smpasm.h,v 1.8 1997/04/25 03:11:40 fsmp Exp $
 */
#ifndef __MACHINE_SMPASM_H__
#define __MACHINE_SMPASM_H__ 1


#if defined(SMP) || defined(APIC_IO)

#include <machine/apic.h>

/* Macro to retrieve the current CPU id from hardware */
#define GETCPUID(reg)				\
	movl _apic_base, reg;			\
	movl APIC_ID(reg), reg; 		\
	andl $APIC_ID_MASK, reg;		\
	shrl $24, reg;				\
	movl _apic_id_to_logical(,reg,4), reg

#define SETCURPROC(val, reg)	GETCPUID(reg); movl val, _SMPcurproc(,reg,4)
#define GETCURPROC(reg)		GETCPUID(reg); movl _SMPcurproc(,reg,4), reg
#define SETCURPCB(val, reg)	GETCPUID(reg); movl val, _SMPcurpcb(,reg,4)
#define GETCURPCB(reg)		GETCPUID(reg); movl _SMPcurpcb(,reg,4), reg
#define SETNPXPROC(val, reg)	GETCPUID(reg); movl val, _SMPnpxproc(,reg,4)
#define GETNPXPROC(reg)		GETCPUID(reg); movl _SMPnpxproc(,reg,4), reg

#else /* !SMP && !APIC_IO */

#define SETCURPROC(val, reg)	movl val, _curproc
#define GETCURPROC(reg)		movl _curproc, reg
#define SETCURPCB(val, reg)	movl val, _curpcb
#define GETCURPCB(reg)		movl _curpcb, reg
#define SETNPXPROC(val, reg)	movl val, _npxproc
#define GETNPXPROC(reg)		movl _npxproc, reg

#endif /* SMP || APIC_IO */

#endif /* __MACHINE_SMPASM_H__ */
