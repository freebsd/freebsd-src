/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

#define IA64_MC_FLAG_ONSTACK		0
#define IA64_MC_FLAG_IN_SYSCALL		1
#define IA64_MC_FLAG_FPH_VALID		2

typedef struct __mcontext {
	/*
	 * These fields must match the definition
	 * of struct sigcontext. That way we can support
	 * struct sigcontext and ucontext_t at the same
	 * time.
	 */
	long	mc_onstack;		/* XXX - sigcontext compat. */
	unsigned long	mc_flags;
	unsigned long	mc_nat;
	unsigned long	mc_sp;
	unsigned long	mc_ip;
	unsigned long	mc_cfm;
	unsigned long	mc_um;
	unsigned long	mc_ar_rsc;
	unsigned long	mc_ar_bsp;
	unsigned long	mc_ar_rnat;
	unsigned long	mc_ar_ccv;
	unsigned long	mc_ar_unat;
	unsigned long	mc_ar_fpsr;
	unsigned long	mc_ar_pfs;
	unsigned long	mc_pr;
	unsigned long	mc_br[8];
	unsigned long	mc_gr[32];
	struct ia64_fpreg mc_fr[128];
} mcontext_t;

#endif /* !_MACHINE_UCONTEXT_H_ */
