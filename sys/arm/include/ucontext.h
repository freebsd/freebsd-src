/*
 * Copyright (c) 2001 David O'Brien.
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * signal.h
 *
 * Architecture dependant signal types and structures
 *
 * Created      : 30/09/94
 *
 *	$NetBSD: signal.h,v 1.8 1998/09/14 02:48:33 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

typedef struct __mcontext {
	/*
	 * The first 20 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	unsigned int	mc_onstack;		/* XXX - sigcontext compat. */
	unsigned int	mc_spsr;
	unsigned int	mc_r0;
	unsigned int	mc_r1;
	unsigned int	mc_r2;
	unsigned int	mc_r3;
	unsigned int	mc_r4;
	unsigned int	mc_r5;
	unsigned int	mc_r6;
	unsigned int	mc_r7;
	unsigned int	mc_r8;
	unsigned int	mc_r9;
	unsigned int	mc_r10;
	unsigned int	mc_r11;
	unsigned int	mc_r12;
	unsigned int	mc_usr_sp;
	unsigned int	mc_usr_lr;
	unsigned int	mc_svc_lr;
	unsigned int	mc_pc;

	unsigned int	__spare__[1];	/* XXX fix the size later */
} mcontext_t;

#endif /* !_MACHINE_UCONTEXT_H_ */
