/*	$OpenBSD: frame.h,v 1.3 1998/09/15 10:50:12 pefo Exp $ */

/*-
 * Copyright (c) 2011-2016 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	JNPR: frame.h,v 1.6.2.1 2007/09/10 08:14:57 girish
 * $FreeBSD$
 *
 */
#ifndef _MACHINE_FRAME_H_
#define	_MACHINE_FRAME_H_


#ifdef CPU_CHERI
#include <machine/cheri.h>
#endif

/* Note: This must also match regnum.h and regdef.h */

struct trapframe {
	register_t	zero;
	register_t	ast;
	register_t	v0;
	register_t	v1;
	register_t	a0;
	register_t	a1;
	register_t	a2;
	register_t	a3;
#if defined(__mips_n32) || defined(__mips_n64)
	register_t	a4;
	register_t	a5;
	register_t	a6;
	register_t	a7;
	register_t	t0;
	register_t	t1;
	register_t	t2;
	register_t	t3;
#else
	register_t	t0;
	register_t	t1;
	register_t	t2;
	register_t	t3;
	register_t	t4;
	register_t	t5;
	register_t	t6;
	register_t	t7;
#endif
	register_t	s0;
	register_t	s1;
	register_t	s2;
	register_t	s3;
	register_t	s4;
	register_t	s5;
	register_t	s6;
	register_t	s7;
	register_t	t8;
	register_t	t9;
	register_t	k0;
	register_t	k1;
	register_t	gp;
	register_t	sp;
	register_t	s8;
	register_t	ra;
	register_t	sr;
	register_t	mullo;
	register_t	mulhi;
	register_t	badvaddr;
	register_t	cause;
	register_t	pc;
	/*
	 * FREEBSD_DEVELOPERS_FIXME:
	 * Include any other registers which are CPU-Specific and
	 * need to be part of the frame here.
	 * 
	 * Also, be sure this matches what is defined in regnum.h
	 */
	register_t	ic;	/* RM7k and RM9k specific */
	register_t	dummy;	/* Alignment for 32-bit case */

	/*
	 * CHERI registers are saved for both user and kernel processes -- but
	 * are defined only on supporting CPUs.  This need not be binary
	 * compatible with struct cheri_frame, but should contain the same
	 * fields.
	 *
	 * NB: 40x 64-bit registers above, which is (conveniently) aligned to
	 * 16 (or 32) bytes with respect to the capabilities that follow.  See
	 * also compile-time assertions in cheri.c.
	 */
#ifdef CPU_CHERI
	struct chericap		ddc;
	struct chericap		c1;
	struct chericap		c2;
	struct chericap		c3;
	struct chericap		c4;
	struct chericap		c5;
	struct chericap		c6;
	struct chericap		c7;
	struct chericap		c8;
	struct chericap		c9;
	struct chericap		c10;
	struct chericap		c11;
	struct chericap		c12;
	struct chericap		c13;
	struct chericap		c14;
	struct chericap		c15;
	struct chericap		c16;
	struct chericap		c17;
	struct chericap		c18;
	struct chericap		c19;
	struct chericap		c20;
	struct chericap		c21;
	struct chericap		c22;
	struct chericap		c23;
	struct chericap		c24;
	struct chericap		c25;
	struct chericap		c26;	/* Sometimes known as idc. */
	struct chericap		epcc;
#endif

	/*
	 * Floating-point registers are preserved only for user processes.
	 */
	f_register_t	f0;
	f_register_t	f1;
	f_register_t	f2;
	f_register_t	f3;
	f_register_t	f4;
	f_register_t	f5;
	f_register_t	f6;
	f_register_t	f7;
	f_register_t	f8;
	f_register_t	f9;
	f_register_t	f10;
	f_register_t	f11;
	f_register_t	f12;
	f_register_t	f13;
	f_register_t	f14;
	f_register_t	f15;
	f_register_t	f16;
	f_register_t	f17;
	f_register_t	f18;
	f_register_t	f19;
	f_register_t	f20;
	f_register_t	f21;
	f_register_t	f22;
	f_register_t	f23;
	f_register_t	f24;
	f_register_t	f25;
	f_register_t	f26;
	f_register_t	f27;
	f_register_t	f28;
	f_register_t	f29;
	f_register_t	f30;
	f_register_t	f31;
	register_t	fsr;
        register_t   fdummy;
};

#endif	/* !_MACHINE_FRAME_H_ */
