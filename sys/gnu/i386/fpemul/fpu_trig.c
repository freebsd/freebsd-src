/*
 *  fpu_trig.c
 *
 * Implementation of the FPU "transcendental" functions.
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
 *    $Id: fpu_trig.c,v 1.5 1995/12/17 21:13:58 phk Exp $
 *
 */


#include <sys/param.h>
#include <sys/proc.h>
#include <machine/cpu.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>
#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/status_w.h>
#include <gnu/i386/fpemul/reg_constant.h>
#include <gnu/i386/fpemul/control_w.h>

static void convert_l2reg(long *arg, FPU_REG * dest);

static int
trig_arg(FPU_REG * X)
{
	FPU_REG tmp, quot;
	int     rv;
	long long q;
	int     old_cw = control_word;

	control_word &= ~CW_RC;
	control_word |= RC_CHOP;

	reg_move(X, &quot);
	reg_div(&quot, &CONST_PI2, &quot, FULL_PRECISION);

	reg_move(&quot, &tmp);
	round_to_int(&tmp);
	if (tmp.sigh & 0x80000000)
		return -1;	/* |Arg| is >= 2^63 */
	tmp.exp = EXP_BIAS + 63;
	q = *(long long *) &(tmp.sigl);
	normalize(&tmp);

	reg_sub(&quot, &tmp, X, FULL_PRECISION);
	rv = q & 7;

	control_word = old_cw;
	return rv;;
}


/* Convert a long to register */
static void
convert_l2reg(long *arg, FPU_REG * dest)
{
	long    num = *arg;

	if (num == 0) {
		reg_move(&CONST_Z, dest);
		return;
	}
	if (num > 0)
		dest->sign = SIGN_POS;
	else {
		num = -num;
		dest->sign = SIGN_NEG;
	}

	dest->sigh = num;
	dest->sigl = 0;
	dest->exp = EXP_BIAS + 31;
	dest->tag = TW_Valid;
	normalize(dest);
}


static void
single_arg_error(void)
{
	switch (FPU_st0_tag) {
		case TW_NaN:
		if (!(FPU_st0_ptr->sigh & 0x40000000)) {	/* Signaling ? */
			EXCEPTION(EX_Invalid);
			/* Convert to a QNaN */
			FPU_st0_ptr->sigh |= 0x40000000;
		}
		break;		/* return with a NaN in st(0) */
	case TW_Empty:
		stack_underflow();	/* Puts a QNaN in st(0) */
		break;
#ifdef PARANOID
	default:
		EXCEPTION(EX_INTERNAL | 0x0112);
#endif				/* PARANOID */
	}
}


/*---------------------------------------------------------------------------*/

static void
f2xm1(void)
{
	switch (FPU_st0_tag) {
		case TW_Valid:
		{
			FPU_REG rv, tmp;

#ifdef DENORM_OPERAND
			if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
				return;
#endif				/* DENORM_OPERAND */

			if (FPU_st0_ptr->sign == SIGN_POS) {
				/* poly_2xm1(x) requires 0 < x < 1. */
				if (poly_2xm1(FPU_st0_ptr, &rv))
					return;	/* error */
				reg_mul(&rv, FPU_st0_ptr, FPU_st0_ptr, FULL_PRECISION);
			} else {
/* **** Should change poly_2xm1() to at least handle numbers near 0 */
				/* poly_2xm1(x) doesn't handle negative
				 * numbers. */
				/* So we compute (poly_2xm1(x+1)-1)/2, for -1
				 * < x < 0 */
				reg_add(FPU_st0_ptr, &CONST_1, &tmp, FULL_PRECISION);
				poly_2xm1(&tmp, &rv);
				reg_mul(&rv, &tmp, &tmp, FULL_PRECISION);
				reg_sub(&tmp, &CONST_1, FPU_st0_ptr, FULL_PRECISION);
				FPU_st0_ptr->exp--;
				if (FPU_st0_ptr->exp <= EXP_UNDER)
					arith_underflow(FPU_st0_ptr);
			}
			return;
		}
	case TW_Zero:
		return;
	case TW_Infinity:
		if (FPU_st0_ptr->sign == SIGN_NEG) {
			/* -infinity gives -1 (p16-10) */
			reg_move(&CONST_1, FPU_st0_ptr);
			FPU_st0_ptr->sign = SIGN_NEG;
		}
		return;
	default:
		single_arg_error();
	}
}

static void
fptan(void)
{
	FPU_REG *st_new_ptr;
	int     q;
	char    arg_sign = FPU_st0_ptr->sign;

	if (STACK_OVERFLOW) {
		stack_overflow();
		return;
	}
	switch (FPU_st0_tag) {
	case TW_Valid:

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		FPU_st0_ptr->sign = SIGN_POS;
		if ((q = trig_arg(FPU_st0_ptr)) != -1) {
			if (q & 1)
				reg_sub(&CONST_1, FPU_st0_ptr, FPU_st0_ptr, FULL_PRECISION);

			poly_tan(FPU_st0_ptr, FPU_st0_ptr);

			FPU_st0_ptr->sign = (q & 1) ^ arg_sign;

			if (FPU_st0_ptr->exp <= EXP_UNDER)
				arith_underflow(FPU_st0_ptr);

			push();
			reg_move(&CONST_1, FPU_st0_ptr);
			setcc(0);
		} else {
			/* Operand is out of range */
			setcc(SW_C2);
			FPU_st0_ptr->sign = arg_sign;	/* restore st(0) */
			return;
		}
		break;
	case TW_Infinity:
		/* Operand is out of range */
		setcc(SW_C2);
		FPU_st0_ptr->sign = arg_sign;	/* restore st(0) */
		return;
	case TW_Zero:
		push();
		reg_move(&CONST_1, FPU_st0_ptr);
		setcc(0);
		break;
	default:
		single_arg_error();
		break;
	}
}


static void
fxtract(void)
{
	FPU_REG *st_new_ptr;
	register FPU_REG *st1_ptr = FPU_st0_ptr;	/* anticipate */

	if (STACK_OVERFLOW) {
		stack_overflow();
		return;
	}
	if (!(FPU_st0_tag ^ TW_Valid)) {
		long    e;

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		push();
		reg_move(st1_ptr, FPU_st0_ptr);
		FPU_st0_ptr->exp = EXP_BIAS;
		e = st1_ptr->exp - EXP_BIAS;
		convert_l2reg(&e, st1_ptr);
		return;
	} else
		if (FPU_st0_tag == TW_Zero) {
			char    sign = FPU_st0_ptr->sign;
			divide_by_zero(SIGN_NEG, FPU_st0_ptr);
			push();
			reg_move(&CONST_Z, FPU_st0_ptr);
			FPU_st0_ptr->sign = sign;
			return;
		} else
			if (FPU_st0_tag == TW_Infinity) {
				char    sign = FPU_st0_ptr->sign;
				FPU_st0_ptr->sign = SIGN_POS;
				push();
				reg_move(&CONST_INF, FPU_st0_ptr);
				FPU_st0_ptr->sign = sign;
				return;
			} else
				if (FPU_st0_tag == TW_NaN) {
					if (!(FPU_st0_ptr->sigh & 0x40000000)) {	/* Signaling ? */
						EXCEPTION(EX_Invalid);
						/* Convert to a QNaN */
						FPU_st0_ptr->sigh |= 0x40000000;
					}
					push();
					reg_move(st1_ptr, FPU_st0_ptr);
					return;
				} else
					if (FPU_st0_tag == TW_Empty) {
						/* Is this the correct
						 * behaviour? */
						if (control_word & EX_Invalid) {
							stack_underflow();
							push();
							stack_underflow();
						} else
							EXCEPTION(EX_StackUnder);
					}
#ifdef PARANOID
					else
						EXCEPTION(EX_INTERNAL | 0x119);
#endif				/* PARANOID */
}


static void
fdecstp(void)
{
	top--;			/* FPU_st0_ptr will be fixed in math_emulate()
				 * before the next instr */
}

static void
fincstp(void)
{
	top++;			/* FPU_st0_ptr will be fixed in math_emulate()
				 * before the next instr */
}


static void
fsqrt_(void)
{
	if (!(FPU_st0_tag ^ TW_Valid)) {
		int     expon;

		if (FPU_st0_ptr->sign == SIGN_NEG) {
			arith_invalid(FPU_st0_ptr);	/* sqrt(negative) is
							 * invalid */
			return;
		}
#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		expon = FPU_st0_ptr->exp - EXP_BIAS;
		FPU_st0_ptr->exp = EXP_BIAS + (expon & 1);	/* make st(0) in  [1.0
								 * .. 4.0) */

		wm_sqrt(FPU_st0_ptr, control_word);	/* Do the computation */

		FPU_st0_ptr->exp += expon >> 1;
		FPU_st0_ptr->sign = SIGN_POS;
	} else
		if (FPU_st0_tag == TW_Zero)
			return;
		else
			if (FPU_st0_tag == TW_Infinity) {
				if (FPU_st0_ptr->sign == SIGN_NEG)
					arith_invalid(FPU_st0_ptr);	/* sqrt(-Infinity) is
									 * invalid */
				return;
			} else {
				single_arg_error();
				return;
			}

}


static void
frndint_(void)
{
	if (!(FPU_st0_tag ^ TW_Valid)) {
		if (FPU_st0_ptr->exp > EXP_BIAS + 63)
			return;

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		round_to_int(FPU_st0_ptr);	/* Fortunately, this can't
						 * overflow to 2^64 */
		FPU_st0_ptr->exp = EXP_BIAS + 63;
		normalize(FPU_st0_ptr);
		return;
	} else
		if ((FPU_st0_tag == TW_Zero) || (FPU_st0_tag == TW_Infinity))
			return;
		else
			single_arg_error();
}


static void
fsin(void)
{
	char    arg_sign = FPU_st0_ptr->sign;

	if (FPU_st0_tag == TW_Valid) {
		int     q;
		FPU_st0_ptr->sign = SIGN_POS;

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		if ((q = trig_arg(FPU_st0_ptr)) != -1) {
			FPU_REG rv;

			if (q & 1)
				reg_sub(&CONST_1, FPU_st0_ptr, FPU_st0_ptr, FULL_PRECISION);

			poly_sine(FPU_st0_ptr, &rv);

			setcc(0);
			if (q & 2)
				rv.sign ^= SIGN_POS ^ SIGN_NEG;
			rv.sign ^= arg_sign;
			reg_move(&rv, FPU_st0_ptr);

			if (FPU_st0_ptr->exp <= EXP_UNDER)
				arith_underflow(FPU_st0_ptr);

			set_precision_flag_up();	/* We do not really know
							 * if up or down */

			return;
		} else {
			/* Operand is out of range */
			setcc(SW_C2);
			FPU_st0_ptr->sign = arg_sign;	/* restore st(0) */
			return;
		}
	} else
		if (FPU_st0_tag == TW_Zero) {
			setcc(0);
			return;
		} else
			if (FPU_st0_tag == TW_Infinity) {
				/* Operand is out of range */
				setcc(SW_C2);
				FPU_st0_ptr->sign = arg_sign;	/* restore st(0) */
				return;
			} else
				single_arg_error();
}


static int
f_cos(FPU_REG * arg)
{
	char    arg_sign = arg->sign;

	if (arg->tag == TW_Valid) {
		int     q;

#ifdef DENORM_OPERAND
		if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
			return 1;
#endif				/* DENORM_OPERAND */

		arg->sign = SIGN_POS;
		if ((q = trig_arg(arg)) != -1) {
			FPU_REG rv;

			if (!(q & 1))
				reg_sub(&CONST_1, arg, arg, FULL_PRECISION);

			poly_sine(arg, &rv);

			setcc(0);
			if ((q + 1) & 2)
				rv.sign ^= SIGN_POS ^ SIGN_NEG;
			reg_move(&rv, arg);

			set_precision_flag_up();	/* We do not really know
							 * if up or down */

			return 0;
		} else {
			/* Operand is out of range */
			setcc(SW_C2);
			arg->sign = arg_sign;	/* restore st(0) */
			return 1;
		}
	} else
		if (arg->tag == TW_Zero) {
			reg_move(&CONST_1, arg);
			setcc(0);
			return 0;
		} else
			if (FPU_st0_tag == TW_Infinity) {
				/* Operand is out of range */
				setcc(SW_C2);
				arg->sign = arg_sign;	/* restore st(0) */
				return 1;
			} else {
				single_arg_error();	/* requires arg ==
							 * &st(0) */
				return 1;
			}
}


static void
fcos(void)
{
	f_cos(FPU_st0_ptr);
}


static void
fsincos(void)
{
	FPU_REG *st_new_ptr;
	FPU_REG arg;

	if (STACK_OVERFLOW) {
		stack_overflow();
		return;
	}
	reg_move(FPU_st0_ptr, &arg);
	if (!f_cos(&arg)) {
		fsin();
		push();
		reg_move(&arg, FPU_st0_ptr);
	}
}


/*---------------------------------------------------------------------------*/
/* The following all require two arguments: st(0) and st(1) */

/* remainder of st(0) / st(1) */
/* Assumes that st(0) and st(1) are both TW_Valid */
static void
fprem_kernel(int round)
{
	FPU_REG *st1_ptr = &st(1);
	char    st1_tag = st1_ptr->tag;

	if (!((FPU_st0_tag ^ TW_Valid) | (st1_tag ^ TW_Valid))) {
		FPU_REG tmp;
		int     old_cw = control_word;
		int     expdif = FPU_st0_ptr->exp - (st1_ptr)->exp;

#ifdef DENORM_OPERAND
		if (((FPU_st0_ptr->exp <= EXP_UNDER) ||
			(st1_ptr->exp <= EXP_UNDER)) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		control_word &= ~CW_RC;
		control_word |= round;

		if (expdif < 64) {
			/* This should be the most common case */
			long long q;
			int     c = 0;

			reg_div(FPU_st0_ptr, st1_ptr, &tmp, FULL_PRECISION);

			round_to_int(&tmp);	/* Fortunately, this can't
						 * overflow to 2^64 */
			tmp.exp = EXP_BIAS + 63;
			q = *(long long *) &(tmp.sigl);
			normalize(&tmp);

			reg_mul(st1_ptr, &tmp, &tmp, FULL_PRECISION);
			reg_sub(FPU_st0_ptr, &tmp, FPU_st0_ptr, FULL_PRECISION);

			if (q & 4)
				c |= SW_C3;
			if (q & 2)
				c |= SW_C1;
			if (q & 1)
				c |= SW_C0;

			setcc(c);
		} else {
			/* There is a large exponent difference ( >= 64 ) */
			int     N_exp;

			reg_div(FPU_st0_ptr, st1_ptr, &tmp, FULL_PRECISION);
			/* N is 'a number between 32 and 63' (p26-113) */
			N_exp = (tmp.exp & 31) + 32;
			tmp.exp = EXP_BIAS + N_exp;

			round_to_int(&tmp);	/* Fortunately, this can't
						 * overflow to 2^64 */
			tmp.exp = EXP_BIAS + 63;
			normalize(&tmp);

			tmp.exp = EXP_BIAS + expdif - N_exp;

			reg_mul(st1_ptr, &tmp, &tmp, FULL_PRECISION);
			reg_sub(FPU_st0_ptr, &tmp, FPU_st0_ptr, FULL_PRECISION);

			setcc(SW_C2);
		}
		control_word = old_cw;

		if (FPU_st0_ptr->exp <= EXP_UNDER)
			arith_underflow(FPU_st0_ptr);
		return;
	} else
		if ((FPU_st0_tag == TW_Empty) | (st1_tag == TW_Empty)) {
			stack_underflow();
			return;
		} else
			if (FPU_st0_tag == TW_Zero) {
				if (st1_tag == TW_Valid) {

#ifdef DENORM_OPERAND
					if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
						return;
#endif				/* DENORM_OPERAND */

					setcc(0);
					return;
				} else
					if (st1_tag == TW_Zero) {
						arith_invalid(FPU_st0_ptr);
						return;
					}
				/* fprem(?,0) always invalid */
					else
						if (st1_tag == TW_Infinity) {
							setcc(0);
							return;
						}
			} else
				if (FPU_st0_tag == TW_Valid) {
					if (st1_tag == TW_Zero) {
						arith_invalid(FPU_st0_ptr);	/* fprem(Valid,Zero) is
										 * invalid */
						return;
					} else
						if (st1_tag != TW_NaN) {
#ifdef DENORM_OPERAND
							if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */

							if (st1_tag == TW_Infinity) {
								/* fprem(Valid,
								 * Infinity)
								 * is o.k. */
								setcc(0);
								return;
							}
						}
				} else
					if (FPU_st0_tag == TW_Infinity) {
						if (st1_tag != TW_NaN) {
							arith_invalid(FPU_st0_ptr);	/* fprem(Infinity,?) is
											 * invalid */
							return;
						}
					}
	/* One of the registers must contain a NaN is we got here. */

#ifdef PARANOID
	if ((FPU_st0_tag != TW_NaN) && (st1_tag != TW_NaN))
		EXCEPTION(EX_INTERNAL | 0x118);
#endif				/* PARANOID */

	real_2op_NaN(FPU_st0_ptr, st1_ptr, FPU_st0_ptr);

}


/* ST(1) <- ST(1) * log ST;  pop ST */
static void
fyl2x(void)
{
	FPU_REG *st1_ptr = &st(1);
	char    st1_tag = st1_ptr->tag;

	if (!((FPU_st0_tag ^ TW_Valid) | (st1_tag ^ TW_Valid))) {
		if (FPU_st0_ptr->sign == SIGN_POS) {
			int     saved_control, saved_status;

#ifdef DENORM_OPERAND
			if (((FPU_st0_ptr->exp <= EXP_UNDER) ||
				(st1_ptr->exp <= EXP_UNDER)) && (denormal_operand()))
				return;
#endif				/* DENORM_OPERAND */

			/* We use the general purpose arithmetic, so we need
			 * to save these. */
			saved_status = status_word;
			saved_control = control_word;
			control_word = FULL_PRECISION;

			poly_l2(FPU_st0_ptr, FPU_st0_ptr);

			/* Enough of the basic arithmetic is done now */
			control_word = saved_control;
			status_word = saved_status;

			/* Let the multiply set the flags */
			reg_mul(FPU_st0_ptr, st1_ptr, st1_ptr, FULL_PRECISION);

			pop();
			FPU_st0_ptr = &st(0);
		} else {
			/* negative	 */
			pop();
			FPU_st0_ptr = &st(0);
			arith_invalid(FPU_st0_ptr);	/* st(0) cannot be
							 * negative */
			return;
		}
	} else
		if ((FPU_st0_tag == TW_Empty) || (st1_tag == TW_Empty)) {
			stack_underflow_pop(1);
			return;
		} else
			if ((FPU_st0_tag == TW_NaN) || (st1_tag == TW_NaN)) {
				real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
				pop();
				return;
			} else
				if ((FPU_st0_tag <= TW_Zero) && (st1_tag <= TW_Zero)) {
					/* one of the args is zero, the other
					 * valid, or both zero */
					if (FPU_st0_tag == TW_Zero) {
						pop();
						FPU_st0_ptr = &st(0);
						if (FPU_st0_ptr->tag == TW_Zero)
							arith_invalid(FPU_st0_ptr);	/* Both args zero is
											 * invalid */
#ifdef PECULIAR_486
						/* This case is not
						 * specifically covered in the
						 * manual, but divide-by-zero
						 * would seem to be the best
						 * response. However, a real
						 * 80486 does it this way... */
						else
							if (FPU_st0_ptr->tag == TW_Infinity) {
								reg_move(&CONST_INF, FPU_st0_ptr);
								return;
							}
#endif				/* PECULIAR_486 */
							else
								divide_by_zero(st1_ptr->sign ^ SIGN_NEG ^ SIGN_POS, FPU_st0_ptr);
						return;
					} else {
						/* st(1) contains zero, st(0)
						 * valid <> 0 */
						/* Zero is the valid answer */
						char    sign = st1_ptr->sign;

#ifdef DENORM_OPERAND
						if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */
						if (FPU_st0_ptr->sign == SIGN_NEG) {
							pop();
							FPU_st0_ptr = &st(0);
							arith_invalid(FPU_st0_ptr);	/* log(negative) */
							return;
						}
						if (FPU_st0_ptr->exp < EXP_BIAS)
							sign ^= SIGN_NEG ^ SIGN_POS;
						pop();
						FPU_st0_ptr = &st(0);
						reg_move(&CONST_Z, FPU_st0_ptr);
						FPU_st0_ptr->sign = sign;
						return;
					}
				}
	/* One or both arg must be an infinity */
				else
					if (FPU_st0_tag == TW_Infinity) {
						if ((FPU_st0_ptr->sign == SIGN_NEG) || (st1_tag == TW_Zero)) {
							pop();
							FPU_st0_ptr = &st(0);
							arith_invalid(FPU_st0_ptr);	/* log(-infinity) or
											 * 0*log(infinity) */
							return;
						} else {
							char    sign = st1_ptr->sign;

#ifdef DENORM_OPERAND
							if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */

							pop();
							FPU_st0_ptr = &st(0);
							reg_move(&CONST_INF, FPU_st0_ptr);
							FPU_st0_ptr->sign = sign;
							return;
						}
					}
	/* st(1) must be infinity here */
					else
						if ((FPU_st0_tag == TW_Valid) && (FPU_st0_ptr->sign == SIGN_POS)) {
							if (FPU_st0_ptr->exp >= EXP_BIAS) {
								if ((FPU_st0_ptr->exp == EXP_BIAS) &&
								    (FPU_st0_ptr->sigh == 0x80000000) &&
								    (FPU_st0_ptr->sigl == 0)) {
									/* st(0
									 * )
									 * hold
									 * s
									 * 1.0 */
									pop();
									FPU_st0_ptr = &st(0);
									arith_invalid(FPU_st0_ptr);	/* infinity*log(1) */
									return;
								}
								/* st(0) is
								 * positive
								 * and > 1.0 */
								pop();
							} else {
								/* st(0) is
								 * positive
								 * and < 1.0 */

#ifdef DENORM_OPERAND
								if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
									return;
#endif				/* DENORM_OPERAND */

								st1_ptr->sign ^= SIGN_NEG;
								pop();
							}
							return;
						} else {
							/* st(0) must be zero
							 * or negative */
							if (FPU_st0_ptr->tag == TW_Zero) {
								pop();
								FPU_st0_ptr = st1_ptr;
								st1_ptr->sign ^= SIGN_NEG ^ SIGN_POS;
								/* This should
								 * be invalid,
								 * but a real
								 * 80486 is
								 * happy with
								 * it. */
#ifndef PECULIAR_486
								divide_by_zero(st1_ptr->sign, FPU_st0_ptr);
#endif				/* PECULIAR_486 */
							} else {
								pop();
								FPU_st0_ptr = st1_ptr;
								arith_invalid(FPU_st0_ptr);	/* log(negative) */
							}
							return;
						}
}


static void
fpatan(void)
{
	FPU_REG *st1_ptr = &st(1);
	char    st1_tag = st1_ptr->tag;

	if (!((FPU_st0_tag ^ TW_Valid) | (st1_tag ^ TW_Valid))) {
		int     saved_control, saved_status;
		FPU_REG sum;
		int     quadrant = st1_ptr->sign | ((FPU_st0_ptr->sign) << 1);

#ifdef DENORM_OPERAND
		if (((FPU_st0_ptr->exp <= EXP_UNDER) ||
			(st1_ptr->exp <= EXP_UNDER)) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		/* We use the general purpose arithmetic so we need to save
		 * these. */
		saved_status = status_word;
		saved_control = control_word;
		control_word = FULL_PRECISION;

		st1_ptr->sign = FPU_st0_ptr->sign = SIGN_POS;
		if (compare(st1_ptr) == COMP_A_lt_B) {
			quadrant |= 4;
			reg_div(FPU_st0_ptr, st1_ptr, &sum, FULL_PRECISION);
		} else
			reg_div(st1_ptr, FPU_st0_ptr, &sum, FULL_PRECISION);

		poly_atan(&sum);

		if (quadrant & 4) {
			reg_sub(&CONST_PI2, &sum, &sum, FULL_PRECISION);
		}
		if (quadrant & 2) {
			reg_sub(&CONST_PI, &sum, &sum, FULL_PRECISION);
		}
		if (quadrant & 1)
			sum.sign ^= SIGN_POS ^ SIGN_NEG;

		/* All of the basic arithmetic is done now */
		control_word = saved_control;
		status_word = saved_status;

		reg_move(&sum, st1_ptr);
	} else
		if ((FPU_st0_tag == TW_Empty) || (st1_tag == TW_Empty)) {
			stack_underflow_pop(1);
			return;
		} else
			if ((FPU_st0_tag == TW_NaN) || (st1_tag == TW_NaN)) {
				real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
				pop();
				return;
			} else
				if ((FPU_st0_tag == TW_Infinity) || (st1_tag == TW_Infinity)) {
					char    sign = st1_ptr->sign;
					if (FPU_st0_tag == TW_Infinity) {
						if (st1_tag == TW_Infinity) {
							if (FPU_st0_ptr->sign == SIGN_POS) {
								reg_move(&CONST_PI4, st1_ptr);
							} else
								reg_add(&CONST_PI4, &CONST_PI2, st1_ptr, FULL_PRECISION);
						} else {

#ifdef DENORM_OPERAND
							if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */

							if (FPU_st0_ptr->sign == SIGN_POS) {
								reg_move(&CONST_Z, st1_ptr);
								pop();
								return;
							} else
								reg_move(&CONST_PI, st1_ptr);
						}
					} else {
						/* st(1) is infinity, st(0)
						 * not infinity */
#ifdef DENORM_OPERAND
						if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */

						reg_move(&CONST_PI2, st1_ptr);
					}
					st1_ptr->sign = sign;
				} else
					if (st1_tag == TW_Zero) {
						/* st(0) must be valid or zero */
						char    sign = st1_ptr->sign;

#ifdef DENORM_OPERAND
						if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */

						if (FPU_st0_ptr->sign == SIGN_POS) {
							reg_move(&CONST_Z, st1_ptr);
							pop();
							return;
						} else
							reg_move(&CONST_PI, st1_ptr);
						st1_ptr->sign = sign;
					} else
						if (FPU_st0_tag == TW_Zero) {
							/* st(1) must be
							 * TW_Valid here */
							char    sign = st1_ptr->sign;

#ifdef DENORM_OPERAND
							if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */

							reg_move(&CONST_PI2, st1_ptr);
							st1_ptr->sign = sign;
						}
#ifdef PARANOID
						else
							EXCEPTION(EX_INTERNAL | 0x220);
#endif				/* PARANOID */

	pop();
	set_precision_flag_up();/* We do not really know if up or down */
}


static void
fprem(void)
{
	fprem_kernel(RC_CHOP);
}


static void
fprem1(void)
{
	fprem_kernel(RC_RND);
}


static void
fyl2xp1(void)
{
	FPU_REG *st1_ptr = &st(1);
	char    st1_tag = st1_ptr->tag;

	if (!((FPU_st0_tag ^ TW_Valid) | (st1_tag ^ TW_Valid))) {
		int     saved_control, saved_status;

#ifdef DENORM_OPERAND
		if (((FPU_st0_ptr->exp <= EXP_UNDER) ||
			(st1_ptr->exp <= EXP_UNDER)) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		/* We use the general purpose arithmetic so we need to save
		 * these. */
		saved_status = status_word;
		saved_control = control_word;
		control_word = FULL_PRECISION;

		if (poly_l2p1(FPU_st0_ptr, FPU_st0_ptr)) {
			arith_invalid(st1_ptr);	/* poly_l2p1() returned
						 * invalid */
			pop();
			return;
		}
		/* Enough of the basic arithmetic is done now */
		control_word = saved_control;
		status_word = saved_status;

		/* Let the multiply set the flags */
		reg_mul(FPU_st0_ptr, st1_ptr, st1_ptr, FULL_PRECISION);

		pop();
	} else
		if ((FPU_st0_tag == TW_Empty) | (st1_tag == TW_Empty)) {
			stack_underflow_pop(1);
			return;
		} else
			if (FPU_st0_tag == TW_Zero) {
				if (st1_tag <= TW_Zero) {

#ifdef DENORM_OPERAND
					if ((st1_tag == TW_Valid) && (st1_ptr->exp <= EXP_UNDER) &&
					    (denormal_operand()))
						return;
#endif				/* DENORM_OPERAND */

					st1_ptr->sign ^= FPU_st0_ptr->sign;
					reg_move(FPU_st0_ptr, st1_ptr);
				} else
					if (st1_tag == TW_Infinity) {
						arith_invalid(st1_ptr);	/* Infinity*log(1) */
						pop();
						return;
					} else
						if (st1_tag == TW_NaN) {
							real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
							pop();
							return;
						}
#ifdef PARANOID
						else {
							EXCEPTION(EX_INTERNAL | 0x116);
							return;
						}
#endif				/* PARANOID */
				pop();
				return;
			} else
				if (FPU_st0_tag == TW_Valid) {
					if (st1_tag == TW_Zero) {
						if (FPU_st0_ptr->sign == SIGN_NEG) {
							if (FPU_st0_ptr->exp >= EXP_BIAS) {
								/* st(0) holds
								 * <= -1.0 */
								arith_invalid(st1_ptr);	/* infinity*log(1) */
								pop();
								return;
							}
#ifdef DENORM_OPERAND
							if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */
							st1_ptr->sign ^= SIGN_POS ^ SIGN_NEG;
							pop();
							return;
						}
#ifdef DENORM_OPERAND
						if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */
						pop();
						return;
					}
					if (st1_tag == TW_Infinity) {
						if (FPU_st0_ptr->sign == SIGN_NEG) {
							if ((FPU_st0_ptr->exp >= EXP_BIAS) &&
							    !((FPU_st0_ptr->sigh == 0x80000000) &&
								(FPU_st0_ptr->sigl == 0))) {
								/* st(0) holds
								 * < -1.0 */
								arith_invalid(st1_ptr);
								pop();
								return;
							}
#ifdef DENORM_OPERAND
							if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */
							st1_ptr->sign ^= SIGN_POS ^ SIGN_NEG;
							pop();
							return;
						}
#ifdef DENORM_OPERAND
						if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */
						pop();
						return;
					}
					if (st1_tag == TW_NaN) {
						real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
						pop();
						return;
					}
				} else
					if (FPU_st0_tag == TW_NaN) {
						real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
						pop();
						return;
					} else
						if (FPU_st0_tag == TW_Infinity) {
							if (st1_tag == TW_NaN) {
								real_2op_NaN(FPU_st0_ptr, st1_ptr, st1_ptr);
								pop();
								return;
							} else
								if ((FPU_st0_ptr->sign == SIGN_NEG) ||
								    (st1_tag == TW_Zero)) {
									arith_invalid(st1_ptr);	/* log(infinity) */
									pop();
									return;
								}
							/* st(1) must be valid
							 * here. */

#ifdef DENORM_OPERAND
							if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
								return;
#endif				/* DENORM_OPERAND */

							/* The Manual says
							 * that log(Infinity)
							 * is invalid, but a
							 * real 80486 sensibly
							 * says that it is
							 * o.k. */
							{
								char    sign = st1_ptr->sign;
								reg_move(&CONST_INF, st1_ptr);
								st1_ptr->sign = sign;
							}
							pop();
							return;
						}
#ifdef PARANOID
						else {
							EXCEPTION(EX_INTERNAL | 0x117);
						}
#endif				/* PARANOID */
}


static void
emu_fscale(void)
{
	FPU_REG *st1_ptr = &st(1);
	char    st1_tag = st1_ptr->tag;
	int     old_cw = control_word;

	if (!((FPU_st0_tag ^ TW_Valid) | (st1_tag ^ TW_Valid))) {
		long    scale;
		FPU_REG tmp;

#ifdef DENORM_OPERAND
		if (((FPU_st0_ptr->exp <= EXP_UNDER) ||
			(st1_ptr->exp <= EXP_UNDER)) && (denormal_operand()))
			return;
#endif				/* DENORM_OPERAND */

		if (st1_ptr->exp > EXP_BIAS + 30) {
			/* 2^31 is far too large, would require 2^(2^30) or
			 * 2^(-2^30) */
			char    sign;

			if (st1_ptr->sign == SIGN_POS) {
				EXCEPTION(EX_Overflow);
				sign = FPU_st0_ptr->sign;
				reg_move(&CONST_INF, FPU_st0_ptr);
				FPU_st0_ptr->sign = sign;
			} else {
				EXCEPTION(EX_Underflow);
				sign = FPU_st0_ptr->sign;
				reg_move(&CONST_Z, FPU_st0_ptr);
				FPU_st0_ptr->sign = sign;
			}
			return;
		}
		control_word &= ~CW_RC;
		control_word |= RC_CHOP;
		reg_move(st1_ptr, &tmp);
		round_to_int(&tmp);	/* This can never overflow here */
		control_word = old_cw;
		scale = st1_ptr->sign ? -tmp.sigl : tmp.sigl;
		scale += FPU_st0_ptr->exp;
		FPU_st0_ptr->exp = scale;

		/* Use round_reg() to properly detect under/overflow etc */
		round_reg(FPU_st0_ptr, 0, control_word);

		return;
	} else
		if (FPU_st0_tag == TW_Valid) {
			if (st1_tag == TW_Zero) {

#ifdef DENORM_OPERAND
				if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
					return;
#endif				/* DENORM_OPERAND */

				return;
			}
			if (st1_tag == TW_Infinity) {
				char    sign = st1_ptr->sign;

#ifdef DENORM_OPERAND
				if ((FPU_st0_ptr->exp <= EXP_UNDER) && (denormal_operand()))
					return;
#endif				/* DENORM_OPERAND */

				if (sign == SIGN_POS) {
					reg_move(&CONST_INF, FPU_st0_ptr);
				} else
					reg_move(&CONST_Z, FPU_st0_ptr);
				FPU_st0_ptr->sign = sign;
				return;
			}
			if (st1_tag == TW_NaN) {
				real_2op_NaN(FPU_st0_ptr, st1_ptr, FPU_st0_ptr);
				return;
			}
		} else
			if (FPU_st0_tag == TW_Zero) {
				if (st1_tag == TW_Valid) {

#ifdef DENORM_OPERAND
					if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
						return;
#endif				/* DENORM_OPERAND */

					return;
				} else
					if (st1_tag == TW_Zero) {
						return;
					} else
						if (st1_tag == TW_Infinity) {
							if (st1_ptr->sign == SIGN_NEG)
								return;
							else {
								arith_invalid(FPU_st0_ptr);	/* Zero scaled by
												 * +Infinity */
								return;
							}
						} else
							if (st1_tag == TW_NaN) {
								real_2op_NaN(FPU_st0_ptr, st1_ptr, FPU_st0_ptr);
								return;
							}
			} else
				if (FPU_st0_tag == TW_Infinity) {
					if (st1_tag == TW_Valid) {

#ifdef DENORM_OPERAND
						if ((st1_ptr->exp <= EXP_UNDER) && (denormal_operand()))
							return;
#endif				/* DENORM_OPERAND */

						return;
					}
					if (((st1_tag == TW_Infinity) && (st1_ptr->sign == SIGN_POS))
					    || (st1_tag == TW_Zero))
						return;
					else
						if (st1_tag == TW_Infinity) {
							arith_invalid(FPU_st0_ptr);	/* Infinity scaled by
											 * -Infinity */
							return;
						} else
							if (st1_tag == TW_NaN) {
								real_2op_NaN(FPU_st0_ptr, st1_ptr, FPU_st0_ptr);
								return;
							}
				} else
					if (FPU_st0_tag == TW_NaN) {
						if (st1_tag != TW_Empty) {
							real_2op_NaN(FPU_st0_ptr, st1_ptr, FPU_st0_ptr);
							return;
						}
					}
#ifdef PARANOID
	if (!((FPU_st0_tag == TW_Empty) || (st1_tag == TW_Empty))) {
		EXCEPTION(EX_INTERNAL | 0x115);
		return;
	}
#endif

	/* At least one of st(0), st(1) must be empty */
	stack_underflow();

}


/*---------------------------------------------------------------------------*/

static FUNC trig_table_a[] = {
	f2xm1, fyl2x, fptan, fpatan, fxtract, fprem1, fdecstp, fincstp
};

void
trig_a(void)
{
	(trig_table_a[FPU_rm]) ();
}


static FUNC trig_table_b[] =
{
	fprem, fyl2xp1, fsqrt_, fsincos, frndint_, emu_fscale, fsin, fcos
};

void
trig_b(void)
{
	(trig_table_b[FPU_rm]) ();
}
