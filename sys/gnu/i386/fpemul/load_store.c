/*
 *  load_store.c
 *
 * This file contains most of the code to interpret the FPU instructions
 * which load and store from user memory.
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
 * $FreeBSD: src/sys/gnu/i386/fpemul/load_store.c,v 1.13 1999/08/28 00:42:52 peter Exp $
 *
 */

/*---------------------------------------------------------------------------+
 | Note:                                                                     |
 |    The file contains code which accesses user memory.                     |
 |    Emulator static data may change when user memory is accessed, due to   |
 |    other processes using the emulator while swapping is in progress.      |
 +---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <machine/pcb.h>

#include <gnu/i386/fpemul/fpu_emu.h>
#include <gnu/i386/fpemul/fpu_system.h>
#include <gnu/i386/fpemul/status_w.h>


#define _NONE_ 0		/* FPU_st0_ptr etc not needed */
#define _REG0_ 1		/* Will be storing st(0) */
#define _PUSH_ 3		/* Need to check for space to push onto stack */
#define _null_ 4		/* Function illegal or not implemented */

#define pop_0()	{ pop_ptr->tag = TW_Empty; top++; }


static unsigned char type_table[32] = {
	_PUSH_, _PUSH_, _PUSH_, _PUSH_,
	_null_, _null_, _null_, _null_,
	_REG0_, _REG0_, _REG0_, _REG0_,
	_REG0_, _REG0_, _REG0_, _REG0_,
	_NONE_, _null_, _NONE_, _PUSH_,
	_NONE_, _PUSH_, _null_, _PUSH_,
	_NONE_, _null_, _NONE_, _REG0_,
	_NONE_, _REG0_, _NONE_, _REG0_
};

void
load_store_instr(char type)
{
	FPU_REG *pop_ptr;	/* We need a version of FPU_st0_ptr which
				 * won't change. */

	pop_ptr = NULL;		/* Initialized just to stop compiler warnings. */


	switch (type_table[(int) (unsigned) type]) {
	case _NONE_:
		break;
	case _REG0_:
		pop_ptr = &st(0);	/* Some of these instructions pop
					 * after storing */

		FPU_st0_ptr = pop_ptr;	/* Set the global variables. */
		FPU_st0_tag = FPU_st0_ptr->tag;
		break;
	case _PUSH_:
		{
			pop_ptr = &st(-1);
			if (pop_ptr->tag != TW_Empty) {
				stack_overflow();
				return;
			}
			top--;
		}
		break;
	case _null_:
		Un_impl();
		return;
#ifdef PARANOID
	default:
		return EXCEPTION(EX_INTERNAL);
#endif				/* PARANOID */
	}

	switch (type) {
	case 000:		/* fld m32real */
		reg_load_single();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 001:		/* fild m32int */
		reg_load_int32();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 002:		/* fld m64real */
		reg_load_double();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 003:		/* fild m16int */
		reg_load_int16();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 010:		/* fst m32real */
		reg_store_single();
		break;
	case 011:		/* fist m32int */
		reg_store_int32();
		break;
	case 012:		/* fst m64real */
		reg_store_double();
		break;
	case 013:		/* fist m16int */
		reg_store_int16();
		break;
	case 014:		/* fstp m32real */
		if (reg_store_single())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 015:		/* fistp m32int */
		if (reg_store_int32())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 016:		/* fstp m64real */
		if (reg_store_double())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 017:		/* fistp m16int */
		if (reg_store_int16())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 020:		/* fldenv  m14/28byte */
		fldenv();
		break;
	case 022:		/* frstor m94/108byte */
		frstor();
		break;
	case 023:		/* fbld m80dec */
		reg_load_bcd();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 024:		/* fldcw */
		REENTRANT_CHECK(OFF);
		control_word = fusword((unsigned short *) FPU_data_address);
		REENTRANT_CHECK(ON);
#ifdef NO_UNDERFLOW_TRAP
		if (!(control_word & EX_Underflow)) {
			control_word |= EX_Underflow;
		}
#endif
		/* We want no net effect: */
		FPU_data_address = (void *) (intptr_t) data_operand_offset;
		FPU_entry_eip = ip_offset;	/* We want no net effect */
		break;
	case 025:		/* fld m80real */
		reg_load_extended();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 027:		/* fild m64int */
		reg_load_int64();
		setcc(0);	/* Clear the SW_C1 bit, "other bits undefined" */
		reg_move(&FPU_loaded_data, pop_ptr);
		break;
	case 030:		/* fstenv  m14/28byte */
		fstenv();
		/* We want no net effect: */
		FPU_data_address = (void *) (intptr_t) data_operand_offset;
		FPU_entry_eip = ip_offset;	/* We want no net effect */
		break;
	case 032:		/* fsave */
		fsave();
		/* We want no net effect: */
		FPU_data_address = (void *) (intptr_t) data_operand_offset;
		FPU_entry_eip = ip_offset;	/* We want no net effect */
		break;
	case 033:		/* fbstp m80dec */
		if (reg_store_bcd())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 034:		/* fstcw m16int */
		REENTRANT_CHECK(OFF);
/*		    verify_area(VERIFY_WRITE, FPU_data_address, 2);*/
		susword( (short *) FPU_data_address,control_word);
		REENTRANT_CHECK(ON);
		/* We want no net effect: */
		FPU_data_address = (void *) (intptr_t ) data_operand_offset;
		FPU_entry_eip = ip_offset;	/* We want no net effect */
		break;
	case 035:		/* fstp m80real */
		if (reg_store_extended())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	case 036:		/* fstsw m2byte */
		status_word &= ~SW_Top;
		status_word |= (top & 7) << SW_Top_Shift;
		REENTRANT_CHECK(OFF);
/*		    verify_area(VERIFY_WRITE, FPU_data_address, 2);*/
		susword( (short *) FPU_data_address,status_word);
		REENTRANT_CHECK(ON);
		/* We want no net effect: */
		FPU_data_address = (void *) (intptr_t) data_operand_offset;
		FPU_entry_eip = ip_offset;	/* We want no net effect */
		break;
	case 037:		/* fistp m64int */
		if (reg_store_int64())
			pop_0();/* pop only if the number was actually stored
				 * (see the 80486 manual p16-28) */
		break;
	}
}
