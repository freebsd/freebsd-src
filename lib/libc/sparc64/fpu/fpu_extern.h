/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: NetBSD: fpu_extern.h,v 1.4 2000/08/03 18:32:08 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_FPU_FPU_EXTERN_H_
#define _SPARC64_FPU_FPU_EXTERN_H_

struct proc;
struct fpstate;
struct utrapframe;
union instr;
struct fpemu;
struct fpn;

/* fpu.c */
void __fpu_exception __P((struct utrapframe *tf));
void __fpu_panic __P((char *msg));

/* fpu_add.c */
struct fpn *__fpu_add __P((struct fpemu *));

/* fpu_compare.c */
void __fpu_compare __P((struct fpemu *, int));

/* fpu_div.c */
struct fpn *__fpu_div __P((struct fpemu *));

/* fpu_explode.c */
int __fpu_itof __P((struct fpn *, u_int));
int __fpu_xtof __P((struct fpn *, u_int64_t));
int __fpu_stof __P((struct fpn *, u_int));
int __fpu_dtof __P((struct fpn *, u_int, u_int ));
int __fpu_qtof __P((struct fpn *, u_int, u_int , u_int , u_int ));
void __fpu_explode __P((struct fpemu *, struct fpn *, int, int ));

/* fpu_implode.c */
u_int __fpu_ftoi __P((struct fpemu *, struct fpn *));
u_int __fpu_ftox __P((struct fpemu *, struct fpn *, u_int *));
u_int __fpu_ftos __P((struct fpemu *, struct fpn *));
u_int __fpu_ftod __P((struct fpemu *, struct fpn *, u_int *));
u_int __fpu_ftoq __P((struct fpemu *, struct fpn *, u_int *));
void __fpu_implode __P((struct fpemu *, struct fpn *, int, u_int *));

/* fpu_mul.c */
struct fpn *__fpu_mul __P((struct fpemu *));

/* fpu_sqrt.c */
struct fpn *__fpu_sqrt __P((struct fpemu *));

/* fpu_subr.c */
/*
 * Shift a number right some number of bits, taking care of round/sticky.
 * Note that the result is probably not a well-formed number (it will lack
 * the normal 1-bit mant[0]&FP_1).
 */
int __fpu_shr __P((register struct fpn *, register int));
void __fpu_norm __P((register struct fpn *));
/* Build a new Quiet NaN (sign=0, frac=all 1's). */
struct fpn *__fpu_newnan __P((register struct fpemu *));

#endif /* !_SPARC64_FPU_FPU_EXTERN_H_ */
