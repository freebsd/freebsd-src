/*
 *  fpu_emu.h
 *
 *
 * Copyright (C) 1992,1993,1994
 *                       W. Metzenthen, 22 Parker St, Ormond, Vic 3163,
 *                       Australia.  E-mail   billm@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD, FreeBSD and NetBSD operating systems. Any other
 * use is not permitted under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * The purpose of this copyright, based upon the Berkeley copyright, is to
 * ensure that the covered software remains freely available to everyone.
 *
 * The software (with necessary differences) is also available, but under
 * the terms of the GNU copyleft, for the Linux operating system and for
 * the djgpp ms-dos extender.
 *
 * W. Metzenthen   June 1994.
 *
 *
 *     $Id: fpu_emu.h,v 1.3 1994/06/10 07:44:19 rich Exp $
 *
 */


#ifndef _FPU_EMU_H_
#define _FPU_EMU_H_

/*
 * Define DENORM_OPERAND to make the emulator detect denormals
 * and use the denormal flag of the status word. Note: this only
 * affects the flag and corresponding interrupt, the emulator
 * will always generate denormals and operate upon them as required.
 */
#define DENORM_OPERAND

/*
 * Define PECULIAR_486 to get a closer approximation to 80486 behaviour,
 * rather than behaviour which appears to be cleaner.
 * This is a matter of opinion: for all I know, the 80486 may simply
 * be complying with the IEEE spec. Maybe one day I'll get to see the
 * spec...
 */
#define PECULIAR_486

#ifdef LOCORE
#include <gnu/i386/fpemul/fpu_asm.h>
#define	Const(x)	$/**/x
#else
#define	Const(x)	x
#endif

#define EXP_BIAS	Const(0)
#define EXP_OVER	Const(0x4000)	/* smallest invalid large exponent */
#define	EXP_UNDER	Const(-0x3fff)	/* largest invalid small exponent */

#define SIGN_POS	Const(0)
#define SIGN_NEG	Const(1)

/* Keep the order TW_Valid, TW_Zero, TW_Denormal */
#define TW_Valid	Const(0)/* valid */
#define TW_Zero		Const(1)/* zero */
/* The following fold to 2 (Special) in the Tag Word */
#define TW_Denormal     Const(4)/* De-normal */
#define TW_Infinity	Const(5)/* + or - infinity */
#define	TW_NaN		Const(6)/* Not a Number */

#define TW_Empty	Const(7)/* empty */

 /* #define TW_FPU_Interrupt Const(0x80) *//* Signals an interrupt */


#ifndef LOCORE

#include <sys/types.h>
#include <gnu/i386/fpemul/math_emu.h>

#ifdef PARANOID
extern char emulating;
#define REENTRANT_CHECK(state) emulating = (state)
#define ON 1
#define OFF 0
#else
#define REENTRANT_CHECK(state)
#endif				/* PARANOID */

typedef void (*FUNC) (void);
typedef struct fpu_reg FPU_REG;

#define	st(x)	( regs[((top+x) &7 )] )

#define	STACK_OVERFLOW	(st_new_ptr = &st(-1), st_new_ptr->tag != TW_Empty)
#define	NOT_EMPTY(i)	(st(i).tag != TW_Empty)
#define	NOT_EMPTY_0	(FPU_st0_tag ^ TW_Empty)

extern unsigned char FPU_rm;

extern char FPU_st0_tag;
extern FPU_REG *FPU_st0_ptr;

extern void *FPU_data_address;

extern FPU_REG FPU_loaded_data;

#define pop()	{ FPU_st0_ptr->tag = TW_Empty; top++; }

/* push() does not affect the tags */
#define push()	{ top--; FPU_st0_ptr = st_new_ptr; }


#define reg_move(x, y) { \
		 *(short *)&((y)->sign) = *(short *)&((x)->sign); \
		 *(long *)&((y)->exp) = *(long *)&((x)->exp); \
		 *(long long *)&((y)->sigl) = *(long long *)&((x)->sigl); }


/*----- Prototypes for functions written in assembler -----*/
/* extern void reg_move(FPU_REG *a, FPU_REG *b); */

extern void mul64(long long *a, long long *b, long long *result);
extern void poly_div2(long long *x);
extern void poly_div4(long long *x);
extern void poly_div16(long long *x);
extern void
polynomial(unsigned accum[], unsigned x[],
    unsigned short terms[][4], int n);
	extern void normalize(FPU_REG * x);
	extern void normalize_nuo(FPU_REG * x);
	extern void reg_div(FPU_REG * arg1, FPU_REG * arg2, FPU_REG * answ,
            unsigned int control_w);
	extern void reg_u_sub(FPU_REG * arg1, FPU_REG * arg2, FPU_REG * answ,
            unsigned int control_w);
	extern void reg_u_mul(FPU_REG * arg1, FPU_REG * arg2, FPU_REG * answ,
            unsigned int control_w);
	extern void reg_u_div(FPU_REG * arg1, FPU_REG * arg2, FPU_REG * answ,
            unsigned int control_w);
	extern void reg_u_add(FPU_REG * arg1, FPU_REG * arg2, FPU_REG * answ,
            unsigned int control_w);
	extern void wm_sqrt(FPU_REG * n, unsigned int control_w);
	extern unsigned shrx(void *l, unsigned x);
	extern unsigned shrxs(void *v, unsigned x);
	extern unsigned long div_small(unsigned long long *x, unsigned long y);
	extern void round_reg(FPU_REG * arg, unsigned int extent,
            unsigned int control_w);

#ifndef MAKING_PROTO
#include <gnu/i386/fpemul/fpu_proto.h>
#endif

#endif				/* LOCORE */

#endif				/* _FPU_EMU_H_ */
