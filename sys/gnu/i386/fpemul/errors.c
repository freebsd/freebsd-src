/*
 *  errors.c
 *
 *  The error handling functions for wm-FPU-emu
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
 *    $FreeBSD$
 *
 */

/*---------------------------------------------------------------------------+
 | Note:                                                                     |
 |    The file contains code which accesses user memory.                     |
 |    Emulator static data may change when user memory is accessed, due to   |
 |    other processes using the emulator while swapping is in progress.      |
 +---------------------------------------------------------------------------*/





#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/cpu.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>
#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/status_w.h>
#include <gnu/i386/fpemul/control_w.h>
#include <gnu/i386/fpemul/reg_constant.h>
#include <gnu/i386/fpemul/version.h>

/* */
#undef PRINT_MESSAGES
/* */


void
Un_impl(void)
{
	unsigned char byte1, FPU_modrm;

	REENTRANT_CHECK(OFF);
	byte1 = fubyte((unsigned char *) FPU_ORIG_EIP);
	FPU_modrm = fubyte(1 + (unsigned char *) FPU_ORIG_EIP);

	printf("Unimplemented FPU Opcode at eip=%p : %02x ",
	    FPU_ORIG_EIP, byte1);

	if (FPU_modrm >= 0300)
		printf("%02x (%02x+%d)\n", FPU_modrm, FPU_modrm & 0xf8, FPU_modrm & 7);
	else
		printf("/%d\n", (FPU_modrm >> 3) & 7);
	REENTRANT_CHECK(ON);

	EXCEPTION(EX_Invalid);

}




void
emu_printall()
{
	int     i;
	static char *tag_desc[] = {"Valid", "Zero", "ERROR", "ERROR",
	"DeNorm", "Inf", "NaN", "Empty"};
	unsigned char byte1, FPU_modrm;

	REENTRANT_CHECK(OFF);
	byte1 = fubyte((unsigned char *) FPU_ORIG_EIP);
	FPU_modrm = fubyte(1 + (unsigned char *) FPU_ORIG_EIP);

#ifdef DEBUGGING
	if (status_word & SW_Backward)
		printf("SW: backward compatibility\n");
	if (status_word & SW_C3)
		printf("SW: condition bit 3\n");
	if (status_word & SW_C2)
		printf("SW: condition bit 2\n");
	if (status_word & SW_C1)
		printf("SW: condition bit 1\n");
	if (status_word & SW_C0)
		printf("SW: condition bit 0\n");
	if (status_word & SW_Summary)
		printf("SW: exception summary\n");
	if (status_word & SW_Stack_Fault)
		printf("SW: stack fault\n");
	if (status_word & SW_Precision)
		printf("SW: loss of precision\n");
	if (status_word & SW_Underflow)
		printf("SW: underflow\n");
	if (status_word & SW_Overflow)
		printf("SW: overflow\n");
	if (status_word & SW_Zero_Div)
		printf("SW: divide by zero\n");
	if (status_word & SW_Denorm_Op)
		printf("SW: denormalized operand\n");
	if (status_word & SW_Invalid)
		printf("SW: invalid operation\n");
#endif				/* DEBUGGING */

	status_word = status_word & ~SW_Top;
	status_word |= (top & 7) << SW_Top_Shift;

	printf("At %p: %02x ", FPU_ORIG_EIP, byte1);
	if (FPU_modrm >= 0300)
		printf("%02x (%02x+%d)\n", FPU_modrm, FPU_modrm & 0xf8, FPU_modrm & 7);
	else
		printf("/%d, mod=%d rm=%d\n",
		    (FPU_modrm >> 3) & 7, (FPU_modrm >> 6) & 3, FPU_modrm & 7);

	printf(" SW: b=%d st=%d es=%d sf=%d cc=%d%d%d%d ef=%d%d%d%d%d%d\n",
	    status_word & 0x8000 ? 1 : 0,	/* busy */
	    (status_word & 0x3800) >> 11,	/* stack top pointer */
	    status_word & 0x80 ? 1 : 0,	/* Error summary status */
	    status_word & 0x40 ? 1 : 0,	/* Stack flag */
	    status_word & SW_C3 ? 1 : 0, status_word & SW_C2 ? 1 : 0,	/* cc */
	    status_word & SW_C1 ? 1 : 0, status_word & SW_C0 ? 1 : 0,	/* cc */
	    status_word & SW_Precision ? 1 : 0, status_word & SW_Underflow ? 1 : 0,
	    status_word & SW_Overflow ? 1 : 0, status_word & SW_Zero_Div ? 1 : 0,
	    status_word & SW_Denorm_Op ? 1 : 0, status_word & SW_Invalid ? 1 : 0);

	printf(" CW: ic=%d rc=%d%d pc=%d%d iem=%d     ef=%d%d%d%d%d%d\n",
	    control_word & 0x1000 ? 1 : 0,
	    (control_word & 0x800) >> 11, (control_word & 0x400) >> 10,
	    (control_word & 0x200) >> 9, (control_word & 0x100) >> 8,
	    control_word & 0x80 ? 1 : 0,
	    control_word & SW_Precision ? 1 : 0, control_word & SW_Underflow ? 1 : 0,
	    control_word & SW_Overflow ? 1 : 0, control_word & SW_Zero_Div ? 1 : 0,
	    control_word & SW_Denorm_Op ? 1 : 0, control_word & SW_Invalid ? 1 : 0);

	for (i = 0; i < 8; i++) {
		FPU_REG *r = &st(i);
		switch (r->tag) {
		case TW_Empty:
			continue;
			break;
		case TW_Zero:
			printf("st(%d)  %c .0000 0000 0000 0000         ",
			    i, r->sign ? '-' : '+');
			break;
		case TW_Valid:
		case TW_NaN:
		case TW_Denormal:
		case TW_Infinity:
			printf("st(%d)  %c .%04x %04x %04x %04x e%+-6d ", i,
			    r->sign ? '-' : '+',
			    (long) (r->sigh >> 16),
			    (long) (r->sigh & 0xFFFF),
			    (long) (r->sigl >> 16),
			    (long) (r->sigl & 0xFFFF),
			    r->exp - EXP_BIAS + 1);
			break;
		default:
			printf("Whoops! Error in errors.c      ");
			break;
		}
		printf("%s\n", tag_desc[(int) (unsigned) r->tag]);
	}

	printf("[data] %c .%04x %04x %04x %04x e%+-6d ",
	    FPU_loaded_data.sign ? '-' : '+',
	    (long) (FPU_loaded_data.sigh >> 16),
	    (long) (FPU_loaded_data.sigh & 0xFFFF),
	    (long) (FPU_loaded_data.sigl >> 16),
	    (long) (FPU_loaded_data.sigl & 0xFFFF),
	    FPU_loaded_data.exp - EXP_BIAS + 1);
	printf("%s\n", tag_desc[(int) (unsigned) FPU_loaded_data.tag]);
	REENTRANT_CHECK(ON);

}

static struct {
	int     type;
	char   *name;
}       exception_names[] = {
	{
		EX_StackOver, "stack overflow"
	},
	{
		EX_StackUnder, "stack underflow"
	},
	{
		EX_Precision, "loss of precision"
	},
	{
		EX_Underflow, "underflow"
	},
	{
		EX_Overflow, "overflow"
	},
	{
		EX_ZeroDiv, "divide by zero"
	},
	{
		EX_Denormal, "denormalized operand"
	},
	{
		EX_Invalid, "invalid operation"
	},
	{
		EX_INTERNAL, "INTERNAL BUG in " FPU_VERSION
	},
	{
		0, NULL
	}
};
/*
 EX_INTERNAL is always given with a code which indicates where the
 error was detected.

 Internal error types:
       0x14   in e14.c
       0x1nn  in a *.c file:
              0x101  in reg_add_sub.c
              0x102  in reg_mul.c
              0x103  in poly_sin.c
              0x104  in poly_tan.c
              0x105  in reg_mul.c
	      0x106  in reg_mov.c
              0x107  in fpu_trig.c
	      0x108  in reg_compare.c
	      0x109  in reg_compare.c
	      0x110  in reg_add_sub.c
	      0x111  in interface.c
	      0x112  in fpu_trig.c
	      0x113  in reg_add_sub.c
	      0x114  in reg_ld_str.c
	      0x115  in fpu_trig.c
	      0x116  in fpu_trig.c
	      0x117  in fpu_trig.c
	      0x118  in fpu_trig.c
	      0x119  in fpu_trig.c
	      0x120  in poly_atan.c
	      0x121  in reg_compare.c
	      0x122  in reg_compare.c
	      0x123  in reg_compare.c
       0x2nn  in an *.s file:
              0x201  in reg_u_add.S
              0x202  in reg_u_div.S
              0x203  in reg_u_div.S
              0x204  in reg_u_div.S
              0x205  in reg_u_mul.S
              0x206  in reg_u_sub.S
              0x207  in wm_sqrt.S
	      0x208  in reg_div.S
              0x209  in reg_u_sub.S
              0x210  in reg_u_sub.S
              0x211  in reg_u_sub.S
              0x212  in reg_u_sub.S
	      0x213  in wm_sqrt.S
	      0x214  in wm_sqrt.S
	      0x215  in wm_sqrt.S
	      0x216  in reg_round.S
	      0x217  in reg_round.S
	      0x218  in reg_round.S
 */

void
exception(int n)
{
	int     i, int_type;

	int_type = 0;		/* Needed only to stop compiler warnings */
	if (n & EX_INTERNAL) {
		int_type = n - EX_INTERNAL;
		n = EX_INTERNAL;
		/* Set lots of exception bits! */
		status_word |= (SW_Exc_Mask | SW_Summary | FPU_BUSY);
	} else {
		/* Extract only the bits which we use to set the status word */
		n &= (SW_Exc_Mask);
		/* Set the corresponding exception bit */
		status_word |= n;
		if (status_word & ~control_word & CW_Exceptions)
			status_word |= SW_Summary;
		if (n & (SW_Stack_Fault | EX_Precision)) {
			if (!(n & SW_C1))
				/* This bit distinguishes over- from underflow
				 * for a stack fault, and roundup from
				 * round-down for precision loss. */
				status_word &= ~SW_C1;
		}
	}

	REENTRANT_CHECK(OFF);
	if ((~control_word & n & CW_Exceptions) || (n == EX_INTERNAL)) {
#ifdef PRINT_MESSAGES
		/* My message from the sponsor */
		printf(FPU_VERSION " " __DATE__ " (C) W. Metzenthen.\n");
#endif				/* PRINT_MESSAGES */

		/* Get a name string for error reporting */
		for (i = 0; exception_names[i].type; i++)
			if ((exception_names[i].type & n) == exception_names[i].type)
				break;

		if (exception_names[i].type) {
#ifdef PRINT_MESSAGES
			printf("FP Exception: %s!\n", exception_names[i].name);
#endif				/* PRINT_MESSAGES */
		} else
			printf("FP emulator: Unknown Exception: 0x%04x!\n", n);

		if (n == EX_INTERNAL) {
			printf("FP emulator: Internal error type 0x%04x\n", int_type);
			emu_printall();
		}
#ifdef PRINT_MESSAGES
		else
			emu_printall();
#endif				/* PRINT_MESSAGES */

		/* The 80486 generates an interrupt on the next non-control
		 * FPU instruction. So we need some means of flagging it. We
		 * use the ES (Error Summary) bit for this, assuming that this
		 * is the way a real FPU does it (until I can check it out),
		 * if not, then some method such as the following kludge might
		 * be needed. */
/*      regs[0].tag |= TW_FPU_Interrupt; */
	}
	REENTRANT_CHECK(ON);

#ifdef __DEBUG__
	math_abort(SIGFPE);
#endif				/* __DEBUG__ */

}


/* Real operation attempted on two operands, one a NaN */
void
real_2op_NaN(FPU_REG * a, FPU_REG * b, FPU_REG * dest)
{
	FPU_REG *x;
	int     signalling;

	x = a;
	if (a->tag == TW_NaN) {
		if (b->tag == TW_NaN) {
			signalling = !(a->sigh & b->sigh & 0x40000000);
			/* find the "larger" */
			if (*(long long *) &(a->sigl) < *(long long *) &(b->sigl))
				x = b;
		} else {
			/* return the quiet version of the NaN in a */
			signalling = !(a->sigh & 0x40000000);
		}
	} else
#ifdef PARANOID
		if (b->tag == TW_NaN)
#endif				/* PARANOID */
		{
			signalling = !(b->sigh & 0x40000000);
			x = b;
		}
#ifdef PARANOID
		else {
			signalling = 0;
			EXCEPTION(EX_INTERNAL | 0x113);
			x = &CONST_QNaN;
		}
#endif				/* PARANOID */

	if (!signalling) {
		if (!(x->sigh & 0x80000000))	/* pseudo-NaN ? */
			x = &CONST_QNaN;
		reg_move(x, dest);
		return;
	}
	if (control_word & CW_Invalid) {
		/* The masked response */
		if (!(x->sigh & 0x80000000))	/* pseudo-NaN ? */
			x = &CONST_QNaN;
		reg_move(x, dest);
		/* ensure a Quiet NaN */
		dest->sigh |= 0x40000000;
	}
	EXCEPTION(EX_Invalid);

	return;
}
/* Invalid arith operation on Valid registers */
void
arith_invalid(FPU_REG * dest)
{

	if (control_word & CW_Invalid) {
		/* The masked response */
		reg_move(&CONST_QNaN, dest);
	}
	EXCEPTION(EX_Invalid);

	return;

}


/* Divide a finite number by zero */
void
divide_by_zero(int sign, FPU_REG * dest)
{

	if (control_word & CW_ZeroDiv) {
		/* The masked response */
		reg_move(&CONST_INF, dest);
		dest->sign = (unsigned char) sign;
	}
	EXCEPTION(EX_ZeroDiv);

	return;

}


/* This may be called often, so keep it lean */
void
set_precision_flag_up(void)
{
	if (control_word & CW_Precision)
		status_word |= (SW_Precision | SW_C1);	/* The masked response */
	else
		exception(EX_Precision | SW_C1);

}


/* This may be called often, so keep it lean */
void
set_precision_flag_down(void)
{
	if (control_word & CW_Precision) {	/* The masked response */
		status_word &= ~SW_C1;
		status_word |= SW_Precision;
	} else
		exception(EX_Precision);
}


int
denormal_operand(void)
{
	if (control_word & CW_Denormal) {	/* The masked response */
		status_word |= SW_Denorm_Op;
		return 0;
	} else {
		exception(EX_Denormal);
		return 1;
	}
}


void
arith_overflow(FPU_REG * dest)
{

	if (control_word & CW_Overflow) {
		char    sign;
		/* The masked response */
/* **** The response here depends upon the rounding mode */
		sign = dest->sign;
		reg_move(&CONST_INF, dest);
		dest->sign = sign;
	} else {
		/* Subtract the magic number from the exponent */
		dest->exp -= (3 * (1 << 13));
	}

	/* By definition, precision is lost. It appears that the roundup bit
	 * (C1) is also set by convention. */
	EXCEPTION(EX_Overflow | EX_Precision | SW_C1);

	return;

}


void
arith_underflow(FPU_REG * dest)
{

	if (control_word & CW_Underflow) {
		/* The masked response */
		if (dest->exp <= EXP_UNDER - 63)
			reg_move(&CONST_Z, dest);
	} else {
		/* Add the magic number to the exponent */
		dest->exp += (3 * (1 << 13));
	}

	EXCEPTION(EX_Underflow);

	return;
}


void
stack_overflow(void)
{

	if (control_word & CW_Invalid) {
		/* The masked response */
		top--;
		reg_move(&CONST_QNaN, FPU_st0_ptr = &st(0));
	}
	EXCEPTION(EX_StackOver);

	return;

}


void
stack_underflow(void)
{

	if (control_word & CW_Invalid) {
		/* The masked response */
		reg_move(&CONST_QNaN, FPU_st0_ptr);
	}
	EXCEPTION(EX_StackUnder);

	return;

}


void
stack_underflow_i(int i)
{

	if (control_word & CW_Invalid) {
		/* The masked response */
		reg_move(&CONST_QNaN, &(st(i)));
	}
	EXCEPTION(EX_StackUnder);

	return;

}


void
stack_underflow_pop(int i)
{

	if (control_word & CW_Invalid) {
		/* The masked response */
		reg_move(&CONST_QNaN, &(st(i)));
		pop();
	}
	EXCEPTION(EX_StackUnder);

	return;

}
