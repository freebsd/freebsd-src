/*
 *  fpu_entry.c
 *
 * The entry function for wm-FPU-emu
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
 *  $Id: fpu_entry.c,v 1.12 1997/06/14 15:11:03 bde Exp $
 *
 */

/*---------------------------------------------------------------------------+
 | Note:                                                                     |
 |    The file contains code which accesses user memory.                     |
 |    Emulator static data may change when user memory is accessed, due to   |
 |    other processes using the emulator while swapping is in progress.      |
 +---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------+
 | math_emulate() is the sole entry point for wm-FPU-emu                     |
 +---------------------------------------------------------------------------*/


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>
#include <gnu/i386/fpemul/exception.h>
#include <gnu/i386/fpemul/status_w.h>


#define __BAD__ Un_impl		/* Not implemented */

#define FPU_LOOKAHEAD   1       /* For performance boost */

#if FPU_LOOKAHEAD != 0          /* I think thet we have to limit the */
#define LOOKAHEAD_LIMIT	7	/* Max number of lookahead instructions*/
#endif                          /* Or else a prog consisting of a million */
                                /* fnops will spend all its time in kernel*/

#ifndef NO_UNDOC_CODE		/* Un-documented FPU op-codes supported by
				 * default. */

/* WARNING: These codes are not documented by Intel in their 80486 manual
   and may not work on FPU clones or later Intel FPUs. */

/* Changes to support the un-doc codes provided by Linus Torvalds. */

#define _d9_d8_ fstp_i		/* unofficial code (19) */
#define _dc_d0_ fcom_st		/* unofficial code (14) */
#define _dc_d8_ fcompst		/* unofficial code (1c) */
#define _dd_c8_ fxch_i		/* unofficial code (0d) */
#define _de_d0_ fcompst		/* unofficial code (16) */
#define _df_c0_ ffreep		/* unofficial code (07) ffree + pop */
#define _df_c8_ fxch_i		/* unofficial code (0f) */
#define _df_d0_ fstp_i		/* unofficial code (17) */
#define _df_d8_ fstp_i		/* unofficial code (1f) */

static FUNC st_instr_table[64] = {
	fadd__, fld_i_, __BAD__, __BAD__, fadd_i, ffree_, faddp_, _df_c0_,
	fmul__, fxch_i, __BAD__, __BAD__, fmul_i, _dd_c8_, fmulp_, _df_c8_,
	fcom_st, fp_nop, __BAD__, __BAD__, _dc_d0_, fst_i_, _de_d0_, _df_d0_,
	fcompst, _d9_d8_, __BAD__, __BAD__, _dc_d8_, fstp_i, fcompp, _df_d8_,
	fsub__, fp_etc, __BAD__, finit_, fsubri, fucom_, fsubrp, fstsw_,
	fsubr_, fconst, fucompp, __BAD__, fsub_i, fucomp, fsubp_, __BAD__,
	fdiv__, trig_a, __BAD__, __BAD__, fdivri, __BAD__, fdivrp, __BAD__,
	fdivr_, trig_b, __BAD__, __BAD__, fdiv_i, __BAD__, fdivp_, __BAD__,
};
#else				/* Support only documented FPU op-codes */

static FUNC st_instr_table[64] = {
	fadd__, fld_i_, __BAD__, __BAD__, fadd_i, ffree_, faddp_, __BAD__,
	fmul__, fxch_i, __BAD__, __BAD__, fmul_i, __BAD__, fmulp_, __BAD__,
	fcom_st, fp_nop, __BAD__, __BAD__, __BAD__, fst_i_, __BAD__, __BAD__,
	fcompst, __BAD__, __BAD__, __BAD__, __BAD__, fstp_i, fcompp, __BAD__,
	fsub__, fp_etc, __BAD__, finit_, fsubri, fucom_, fsubrp, fstsw_,
	fsubr_, fconst, fucompp, __BAD__, fsub_i, fucomp, fsubp_, __BAD__,
	fdiv__, trig_a, __BAD__, __BAD__, fdivri, __BAD__, fdivrp, __BAD__,
	fdivr_, trig_b, __BAD__, __BAD__, fdiv_i, __BAD__, fdivp_, __BAD__,
};
#endif				/* NO_UNDOC_CODE */


#define _NONE_ 0		/* Take no special action */
#define _REG0_ 1		/* Need to check for not empty st(0) */
#define _REGI_ 2		/* Need to check for not empty st(0) and
				 * st(rm) */
#define _REGi_ 0		/* Uses st(rm) */
#define _PUSH_ 3		/* Need to check for space to push onto stack */
#define _null_ 4		/* Function illegal or not implemented */
#define _REGIi 5		/* Uses st(0) and st(rm), result to st(rm) */
#define _REGIp 6		/* Uses st(0) and st(rm), result to st(rm)
				 * then pop */
#define _REGIc 0		/* Compare st(0) and st(rm) */
#define _REGIn 0		/* Uses st(0) and st(rm), but handle checks
				 * later */

#ifndef NO_UNDOC_CODE

/* Un-documented FPU op-codes supported by default. (see above) */

static unsigned char type_table[64] = {
	_REGI_, _NONE_, _null_, _null_, _REGIi, _REGi_, _REGIp, _REGi_,
	_REGI_, _REGIn, _null_, _null_, _REGIi, _REGI_, _REGIp, _REGI_,
	_REGIc, _NONE_, _null_, _null_, _REGIc, _REG0_, _REGIc, _REG0_,
	_REGIc, _REG0_, _null_, _null_, _REGIc, _REG0_, _REGIc, _REG0_,
	_REGI_, _NONE_, _null_, _NONE_, _REGIi, _REGIc, _REGIp, _NONE_,
	_REGI_, _NONE_, _REGIc, _null_, _REGIi, _REGIc, _REGIp, _null_,
	_REGI_, _NONE_, _null_, _null_, _REGIi, _null_, _REGIp, _null_,
	_REGI_, _NONE_, _null_, _null_, _REGIi, _null_, _REGIp, _null_
};
#else				/* Support only documented FPU op-codes */

static unsigned char type_table[64] = {
	_REGI_, _NONE_, _null_, _null_, _REGIi, _REGi_, _REGIp, _null_,
	_REGI_, _REGIn, _null_, _null_, _REGIi, _null_, _REGIp, _null_,
	_REGIc, _NONE_, _null_, _null_, _null_, _REG0_, _null_, _null_,
	_REGIc, _null_, _null_, _null_, _null_, _REG0_, _REGIc, _null_,
	_REGI_, _NONE_, _null_, _NONE_, _REGIi, _REGIc, _REGIp, _NONE_,
	_REGI_, _NONE_, _REGIc, _null_, _REGIi, _REGIc, _REGIp, _null_,
	_REGI_, _NONE_, _null_, _null_, _REGIi, _null_, _REGIp, _null_,
	_REGI_, _NONE_, _null_, _null_, _REGIi, _null_, _REGIp, _null_
};
#endif				/* NO_UNDOC_CODE */

/* Be careful when using any of these global variables...
   they might change if swapping is triggered */
unsigned char FPU_rm;
char    FPU_st0_tag;
FPU_REG *FPU_st0_ptr;

#ifdef PARANOID
char    emulating = 0;
#endif				/* PARANOID */

#define bswapw(x) __asm__("xchgb %%al,%%ah":"=a" (x):"0" ((short)x))
#define math_abort(signo) \
    FPU_EIP = FPU_ORIG_EIP;REENTRANT_CHECK(OFF);return(signo);

static int
math_emulate(struct trapframe * tframe)
{

	unsigned char FPU_modrm;
	unsigned short code;
#ifdef LOOKAHEAD_LIMIT
	int lookahead_limit = LOOKAHEAD_LIMIT;
#endif
#ifdef PARANOID
	if (emulating) {
		printf("ERROR: wm-FPU-emu is not RE-ENTRANT!\n");
	}
	REENTRANT_CHECK(ON);
#endif				/* PARANOID */

	if ((((struct pcb *) curproc->p_addr)->pcb_flags & FP_SOFTFP) == 0) {
		finit();
		control_word = __INITIAL_NPXCW__;
		((struct pcb *) curproc->p_addr)->pcb_flags |= FP_SOFTFP;
	}
	FPU_info = tframe;
	FPU_ORIG_EIP = FPU_EIP;	/* --pink-- */

	if (FPU_CS != 0x001f) {
		printf("math_emulate: %x : %x\n", FPU_CS, FPU_EIP);
		panic("FPU emulation in kernel");
	}
#ifdef notyet
	/* We cannot handle emulation in v86-mode */
	if (FPU_EFLAGS & 0x00020000) {
		FPU_ORIG_EIP = FPU_EIP;
		math_abort(FPU_info, SIGILL);
	}
#endif

	FPU_lookahead = FPU_LOOKAHEAD;
	if (curproc->p_flag & P_TRACED)
		FPU_lookahead = 0;

do_another_FPU_instruction:

	REENTRANT_CHECK(OFF);
	code = fuword((u_int *) FPU_EIP);
	REENTRANT_CHECK(ON);
	if ((code & 0xff) == 0x9b) {	/* fwait */
		if (status_word & SW_Summary)
			goto do_the_FPU_interrupt;
		else {
			FPU_EIP++;
			goto FPU_instruction_done;
		}
	}
	if (status_word & SW_Summary) {
		/* Ignore the error for now if the current instruction is a
		 * no-wait control instruction */
		/* The 80486 manual contradicts itself on this topic, so I use
		 * the following list of such instructions until I can check
		 * on a real 80486: fninit, fnstenv, fnsave, fnstsw, fnstenv,
		 * fnclex. */
		if (!((((code & 0xf803) == 0xe003) ||	/* fnclex, fninit,
							 * fnstsw */
			    (((code & 0x3003) == 0x3001) &&	/* fnsave, fnstcw,
								 * fnstenv, fnstsw */
				((code & 0xc000) != 0xc000))))) {
			/* This is a guess about what a real FPU might do to
			 * this bit: */
/*	  status_word &= ~SW_Summary; ****/

			/* We need to simulate the action of the kernel to FPU
			 * interrupts here. Currently, the "real FPU" part of
			 * the kernel (0.99.10) clears the exception flags,
			 * sets the registers to empty, and passes information
			 * back to the interrupted process via the cs selector
			 * and operand selector, so we do the same. */
	do_the_FPU_interrupt:
			cs_selector &= 0xffff0000;
			cs_selector |= (status_word & ~SW_Top) | ((top & 7) << SW_Top_Shift);
			operand_selector = tag_word();
			status_word = 0;
			top = 0;
			{
				int     r;
				for (r = 0; r < 8; r++) {
					regs[r].tag = TW_Empty;
				}
			}
			REENTRANT_CHECK(OFF);
			math_abort(SIGFPE);
		}
	}
	FPU_entry_eip = FPU_ORIG_EIP = FPU_EIP;

	if ((code & 0xff) == 0x66) {	/* size prefix */
		FPU_EIP++;
		REENTRANT_CHECK(OFF);
		code = fuword((u_int *) FPU_EIP);
		REENTRANT_CHECK(ON);
	}
	FPU_EIP += 2;

	FPU_modrm = code >> 8;
	FPU_rm = FPU_modrm & 7;

	if (FPU_modrm < 0300) {
		/* All of these instructions use the mod/rm byte to get a data
		 * address */
		get_address(FPU_modrm);
		if (!(code & 1)) {
			unsigned short status1 = status_word;
			FPU_st0_ptr = &st(0);
			FPU_st0_tag = FPU_st0_ptr->tag;

			/* Stack underflow has priority */
			if (NOT_EMPTY_0) {
				switch ((code >> 1) & 3) {
				case 0:
					reg_load_single();
					break;
				case 1:
					reg_load_int32();
					break;
				case 2:
					reg_load_double();
					break;
				case 3:
					reg_load_int16();
					break;
				}

				/* No more access to user memory, it is safe
				 * to use static data now */
				FPU_st0_ptr = &st(0);
				FPU_st0_tag = FPU_st0_ptr->tag;

				/* NaN operands have the next priority. */
				/* We have to delay looking at st(0) until
				 * after loading the data, because that data
				 * might contain an SNaN */
				if ((FPU_st0_tag == TW_NaN) ||
				    (FPU_loaded_data.tag == TW_NaN)) {
					/* Restore the status word; we might
					 * have loaded a denormal. */
					status_word = status1;
					if ((FPU_modrm & 0x30) == 0x10) {
						/* fcom or fcomp */
						EXCEPTION(EX_Invalid);
						setcc(SW_C3 | SW_C2 | SW_C0);
						if (FPU_modrm & 0x08)
							pop();	/* fcomp, so we pop. */
					} else
						real_2op_NaN(FPU_st0_ptr, &FPU_loaded_data, FPU_st0_ptr);
					goto reg_mem_instr_done;
				}
				switch ((FPU_modrm >> 3) & 7) {
				case 0:	/* fadd */
					reg_add(FPU_st0_ptr, &FPU_loaded_data, FPU_st0_ptr, control_word);
					break;
				case 1:	/* fmul */
					reg_mul(FPU_st0_ptr, &FPU_loaded_data, FPU_st0_ptr, control_word);
					break;
				case 2:	/* fcom */
					compare_st_data();
					break;
				case 3:	/* fcomp */
					compare_st_data();
					pop();
					break;
				case 4:	/* fsub */
					reg_sub(FPU_st0_ptr, &FPU_loaded_data, FPU_st0_ptr, control_word);
					break;
				case 5:	/* fsubr */
					reg_sub(&FPU_loaded_data, FPU_st0_ptr, FPU_st0_ptr, control_word);
					break;
				case 6:	/* fdiv */
					reg_div(FPU_st0_ptr, &FPU_loaded_data, FPU_st0_ptr, control_word);
					break;
				case 7:	/* fdivr */
					if (FPU_st0_tag == TW_Zero)
						status_word = status1;	/* Undo any denorm tag,
									 * zero-divide has
									 * priority. */
					reg_div(&FPU_loaded_data, FPU_st0_ptr, FPU_st0_ptr, control_word);
					break;
				}
			} else {
				if ((FPU_modrm & 0x30) == 0x10) {
					/* The instruction is fcom or fcomp */
					EXCEPTION(EX_StackUnder);
					setcc(SW_C3 | SW_C2 | SW_C0);
					if (FPU_modrm & 0x08)
						pop();	/* fcomp, Empty or not,
							 * we pop. */
				} else
					stack_underflow();
			}
		} else {
			load_store_instr(((FPU_modrm & 0x38) | (code & 6)) >> 1);
		}

reg_mem_instr_done:

		data_operand_offset = (unsigned long) FPU_data_address;
	} else {
		/* None of these instructions access user memory */
		unsigned char instr_index = (FPU_modrm & 0x38) | (code & 7);

		FPU_st0_ptr = &st(0);
		FPU_st0_tag = FPU_st0_ptr->tag;
		switch (type_table[(int) instr_index]) {
		case _NONE_:	/* also _REGIc: _REGIn */
			break;
		case _REG0_:
			if (!NOT_EMPTY_0) {
				stack_underflow();
				goto FPU_instruction_done;
			}
			break;
		case _REGIi:
			if (!NOT_EMPTY_0 || !NOT_EMPTY(FPU_rm)) {
				stack_underflow_i(FPU_rm);
				goto FPU_instruction_done;
			}
			break;
		case _REGIp:
			if (!NOT_EMPTY_0 || !NOT_EMPTY(FPU_rm)) {
				stack_underflow_i(FPU_rm);
				pop();
				goto FPU_instruction_done;
			}
			break;
		case _REGI_:
			if (!NOT_EMPTY_0 || !NOT_EMPTY(FPU_rm)) {
				stack_underflow();
				goto FPU_instruction_done;
			}
			break;
		case _PUSH_:	/* Only used by the fld st(i) instruction */
			break;
		case _null_:
			Un_impl();
			goto FPU_instruction_done;
		default:
			EXCEPTION(EX_INTERNAL | 0x111);
			goto FPU_instruction_done;
		}
		(*st_instr_table[(int) instr_index]) ();
	}

FPU_instruction_done:

	ip_offset = FPU_entry_eip;
	bswapw(code);
	*(1 + (unsigned short *) &cs_selector) = code & 0x7ff;

#ifdef DEBUG
	REENTRANT_CHECK(OFF);
	emu_printall();
	REENTRANT_CHECK(ON);
#endif				/* DEBUG */
#ifdef LOOKAHEAD_LIMIT
if (--lookahead_limit)
#endif
	if (FPU_lookahead) {
		unsigned char next;

		/* (This test should generate no machine code) */
		while (1) {
			REENTRANT_CHECK(OFF);
			next = fubyte((u_char *) FPU_EIP);
			REENTRANT_CHECK(ON);
			if (((next & 0xf8) == 0xd8) || (next == 0x9b)) {	/* fwait */
				goto do_another_FPU_instruction;
			} else
				if (next == 0x66) {	/* size prefix */
					REENTRANT_CHECK(OFF);
					next = fubyte((u_char *) (FPU_EIP + 1));
					REENTRANT_CHECK(ON);
					if ((next & 0xf8) == 0xd8) {
						FPU_EIP++;
						goto do_another_FPU_instruction;
					}
				}
			break;
		}
	}
	REENTRANT_CHECK(OFF);
	return (0);		/* --pink-- */
}

#ifdef LKM
MOD_MISC(gnufpu);
static int
gnufpu_load(struct lkm_table *lkmtp, int cmd)
{
	if (pmath_emulate) {
		printf("Math emulator already present\n");
		return EBUSY;
	}
	pmath_emulate = math_emulate;
	return 0;
}

static int
gnufpu_unload(struct lkm_table *lkmtp, int cmd)
{
	if (pmath_emulate != math_emulate) {
		printf("Cannot unload another math emulator\n");
		return EACCES;
	}
	pmath_emulate = 0;
	return 0;
}

int
gnufpu_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	MOD_DISPATCH(gnufpu, lkmtp, cmd, ver, gnufpu_load, gnufpu_unload,
	    lkm_nullcmd);
}
#else /* !LKM */

static void
gnufpu_init(void *unused)
{
	if (pmath_emulate)
		printf("Another Math emulator already present\n");
	else
		pmath_emulate = math_emulate;
}

SYSINIT(gnufpu, SI_SUB_CPU, SI_ORDER_ANY, gnufpu_init, NULL);

#endif /* LKM */
