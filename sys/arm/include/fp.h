/*	$NetBSD: fp.h,v 1.1 2001/01/10 19:02:06 bjh21 Exp $	*/

/*-
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
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
 * fp.h
 *
 * FP info
 *
 * Created      : 10/10/95
 *
 * $FreeBSD: src/sys/arm/include/fp.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_FP_H
#define _MACHINE_FP_H

/*
 * An extended precision floating point number
 */

typedef struct fp_extended_precision {
	u_int32_t fp_exponent;
	u_int32_t fp_mantissa_hi;
	u_int32_t fp_mantissa_lo;
} fp_extended_precision_t;

typedef struct fp_extended_precision fp_reg_t;

/*
 * Information about the FPE-SP state that is stored in the pcb
 *
 * This needs to move and be hidden from userland.
 */

struct fpe_sp_state {
	unsigned int fp_flags;
	unsigned int fp_sr;
	unsigned int fp_cr;
	fp_reg_t fp_registers[16];
};

/*
 * Type for a saved FP context, if we want to translate the context to a
 * user-readable form
 */
 
typedef struct {
	u_int32_t fpsr;
	fp_extended_precision_t regs[8];
} fp_state_t;

#endif /* _MACHINE_FP_H_ */

/* End of fp.h */
