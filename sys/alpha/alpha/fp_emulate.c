/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <sys/user.h>
#include <machine/inst.h>
#include <machine/fpu.h>
#include <machine/reg.h>
#include <alpha/alpha/ieee_float.h>

#define GETREG(regs, i)		(*(fp_register_t*) &regs->fpr_regs[i])
#define PUTREG(regs, i, v)	(*(fp_register_t*) &regs->fpr_regs[i] = v)

typedef fp_register_t fp_opcode_handler(union alpha_instruction ins,
					int src, int rnd,
					u_int64_t fp_control,
					u_int64_t *status,
					struct fpreg *fpregs);

static fp_register_t fp_add(union alpha_instruction ins,
			    int src, int rnd,
			    u_int64_t control, u_int64_t *status,
			    struct fpreg *fpregs)
{
	return ieee_add(GETREG(fpregs, ins.f_format.fa),
			GETREG(fpregs, ins.f_format.fb),
			src, rnd, control, status);
}

static fp_register_t fp_sub(union alpha_instruction ins,
			    int src, int rnd,
			    u_int64_t control, u_int64_t *status,
			    struct fpreg *fpregs)
{
    return ieee_sub(GETREG(fpregs, ins.f_format.fa),
		    GETREG(fpregs, ins.f_format.fb),
		    src, rnd, control, status);
}

static fp_register_t fp_mul(union alpha_instruction ins,
			    int src, int rnd,
			    u_int64_t control, u_int64_t *status,
			    struct fpreg *fpregs)
{
    return ieee_mul(GETREG(fpregs, ins.f_format.fa),
		    GETREG(fpregs, ins.f_format.fb),
		    src, rnd, control, status);
}

static fp_register_t fp_div(union alpha_instruction ins,
			    int src, int rnd,
			    u_int64_t control, u_int64_t *status,
			    struct fpreg *fpregs)
{
    return ieee_div(GETREG(fpregs, ins.f_format.fa),
		    GETREG(fpregs, ins.f_format.fb),
		    src, rnd, control, status);
}

static fp_register_t fp_cmpun(union alpha_instruction ins,
			      int src, int rnd,
			      u_int64_t control, u_int64_t *status,
			      struct fpreg *fpregs)
{
    return ieee_cmpun(GETREG(fpregs, ins.f_format.fa),
		      GETREG(fpregs, ins.f_format.fb),
		      status);
}

static fp_register_t fp_cmpeq(union alpha_instruction ins,
			      int src, int rnd,
			      u_int64_t control, u_int64_t *status,
			      struct fpreg *fpregs)
{
    return ieee_cmpeq(GETREG(fpregs, ins.f_format.fa),
		      GETREG(fpregs, ins.f_format.fb),
		      status);
}

static fp_register_t fp_cmplt(union alpha_instruction ins,
			      int src, int rnd,
			      u_int64_t control, u_int64_t *status,
			      struct fpreg *fpregs)
{
    return ieee_cmplt(GETREG(fpregs, ins.f_format.fa),
		      GETREG(fpregs, ins.f_format.fb),
		      status);
}

static fp_register_t fp_cmple(union alpha_instruction ins,
			      int src, int rnd,
			      u_int64_t control, u_int64_t *status,
			      struct fpreg *fpregs)
{
    return ieee_cmple(GETREG(fpregs, ins.f_format.fa),
		      GETREG(fpregs, ins.f_format.fb),
		      status);
}

static fp_register_t fp_cvts(union alpha_instruction ins,
			     int src, int rnd,
			     u_int64_t control, u_int64_t *status,
			     struct fpreg *fpregs)
{
	switch (src) {
	case T_FORMAT:
	    return ieee_convert_T_S(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);

	case Q_FORMAT:
	    return ieee_convert_Q_S(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);

	default:
	    *status |= FPCR_INV;
	    return GETREG(fpregs, ins.f_format.fc);
	}
}

static fp_register_t fp_cvtt(union alpha_instruction ins,
			     int src, int rnd,
			     u_int64_t control, u_int64_t *status,
			     struct fpreg *fpregs)
{
	switch (src) {
	case S_FORMAT:
	    return ieee_convert_S_T(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);
	    break;

	case Q_FORMAT:
	    return ieee_convert_Q_T(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);
	    break;

	default:
	    *status |= FPCR_INV;
	    return GETREG(fpregs, ins.f_format.fc);
	}
}

static fp_register_t fp_cvtq(union alpha_instruction ins,
			     int src, int rnd,
			     u_int64_t control, u_int64_t *status,
			     struct fpreg *fpregs)
{
	switch (src) {
	case S_FORMAT:
	    return ieee_convert_S_Q(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);
	    break;

	case T_FORMAT:
	    return ieee_convert_T_Q(GETREG(fpregs, ins.f_format.fb),
				    rnd, control, status);
	    break;
	    
	default:
	    *status |= FPCR_INV;
	    return GETREG(fpregs, ins.f_format.fc);
	}
}

static fp_register_t fp_reserved(union alpha_instruction ins,
				 int src, int rnd,
				 u_int64_t control, u_int64_t *status,
				 struct fpreg *fpregs)
{
    *status |= FPCR_INV;
    return GETREG(fpregs, ins.f_format.fc);
}

static fp_register_t fp_cvtql(union alpha_instruction ins,
				 int src, int rnd,
				 u_int64_t control, u_int64_t *status,
				 struct fpreg *fpregs)

{
    fp_register_t fb = GETREG(fpregs, ins.f_format.fb);
    fp_register_t ret;
    *status |= FPCR_INV;
    ret.q = ((fb.q & 0xc0000000) << 32 | (fb.q & 0x3fffffff) << 29);
    return ret;
}

static int fp_emulate(union alpha_instruction ins, struct proc *p)
{
	u_int64_t control = p->p_addr->u_pcb.pcb_fp_control;
	struct fpreg *fpregs = &p->p_addr->u_pcb.pcb_fp;
	static fp_opcode_handler *ops[16] = {
		fp_add,		/* 0 */
		fp_sub,		/* 1 */
		fp_mul,		/* 2 */
		fp_div,		/* 3 */
		fp_cmpun,	/* 4 */
		fp_cmpeq,	/* 5 */
		fp_cmplt,	/* 6 */
		fp_cmple,	/* 7 */
		fp_reserved,	/* 8 */
		fp_reserved,	/* 9 */
		fp_reserved,	/* 10 */
		fp_reserved,	/* 11 */
		fp_cvts,	/* 12 */
		fp_reserved,	/* 13 */
		fp_cvtt,	/* 14 */
		fp_cvtq,	/* 15 */
	};
	int src, rnd;
	fp_register_t result;
	u_int64_t status;

	/*
	 * Only attempt to emulate ieee instructions & integer overflow
	 */
	if ((ins.common.opcode != op_flti) &&
	    (ins.f_format.function != fltl_cvtqlsv)){
		printf("fp_emulate: unhandled opcode = 0x%x, fun = 0x%x\n",ins.common.opcode,ins.f_format.function);
		return 0;
	}

	/*
	 * Dump the float registers into the pcb so we can get at
	 * them. We are potentially going to modify the fp state, so
	 * cancel fpcurproc too.
	 */
	alpha_fpstate_save(p, 1);

	/*
	 * Decode and execute the instruction.
	 */
	src = (ins.f_format.function >> 4) & 3;
	rnd = (ins.f_format.function >> 6) & 3;
	if (rnd == 3)
		rnd = (fpregs->fpr_cr >> FPCR_DYN_SHIFT) & 3;
	status = 0;
	if (ins.common.opcode == op_fltl
	   && ins.f_format.function == fltl_cvtqlsv)
		result = fp_cvtql(ins, src, rnd, control, &status,
				  fpregs);
	else
		result = ops[ins.f_format.function & 0xf](ins, src, rnd,
							  control, &status,
							  fpregs);
	
	/*
	 * Handle exceptions.
	 */
	if (status) {
		u_int64_t fpcr;

		/* Record the exception in the software control word. */
		control |= (status >> IEEE_STATUS_TO_FPCR_SHIFT);
		p->p_addr->u_pcb.pcb_fp_control = control;

		/* Regenerate the control register */
		fpcr = fpregs->fpr_cr & FPCR_DYN_MASK;
		fpcr |= ((control & IEEE_STATUS_MASK)
			 << IEEE_STATUS_TO_FPCR_SHIFT);
		if (!(control & IEEE_TRAP_ENABLE_INV))
			fpcr |= FPCR_INVD;
		if (!(control & IEEE_TRAP_ENABLE_DZE))
			fpcr |= FPCR_DZED;
		if (!(control & IEEE_TRAP_ENABLE_OVF))
			fpcr |= FPCR_OVFD;
		if (!(control & IEEE_TRAP_ENABLE_UNF))
			fpcr |= FPCR_UNFD;
		if (!(control & IEEE_TRAP_ENABLE_INE))
			fpcr |= FPCR_INED;
		if (control & IEEE_STATUS_MASK)
			fpcr |= FPCR_SUM;
		fpregs->fpr_cr = fpcr;

		/* Check the trap enable */
		if ((control >> IEEE_STATUS_TO_EXCSUM_SHIFT)
		    & (control & IEEE_TRAP_ENABLE_MASK))
			return 0;
	}

	PUTREG(fpregs, ins.f_format.fc, result);
	return 1;
}

/*
 * Attempt to complete a floating point instruction which trapped by
 * emulating it in software.  Return non-zero if the completion was
 * successful, otherwise zero.
 */
int fp_software_completion(u_int64_t regmask, struct proc *p)
{
	struct trapframe *frame = p->p_md.md_tf;
	u_int64_t pc = frame->tf_regs[FRAME_PC];
	int error;

	/*
	 * First we must search back through the trap shadow to find which
	 * instruction was responsible for generating the trap.
	 */
	pc -= 4;
	while (regmask) {
		union alpha_instruction ins;

		/*
		 * Read the instruction and figure out the destination
		 * register and opcode.
		 */
		error = copyin((caddr_t) pc, &ins, sizeof(ins));
		if (error)
			return 0;
		
		switch (ins.common.opcode) {
		case op_call_pal:
		case op_jsr:
		case op_br ... op_bgt:
			/*
			 * Condition 6: the trap shadow may not
			 * include any branch instructions.   Also,
			 * the trap shadow is bounded by EXCB, TRAPB
			 * and CALL_PAL.
			 */
			return 0;

		case op_misc:
			switch (ins.memory_format.function) {
			case misc_trapb:
			case misc_excb:
				return 0;
			}
			break;

		case op_inta:
		case op_intl:
		case op_ints:
			/*
			 * The first 32 bits of the register mask
			 * represents integer registers which were
			 * modified in the trap shadow.
			 */
			regmask &= ~(1LL << ins.o_format.rc);
			break;

		case op_fltv:
		case op_flti:
		case op_fltl:
			/*
			 * The second 32 bits of the register mask
			 * represents float registers which were
			 * modified in the trap shadow.
			 */
			regmask &= ~(1LL << (ins.f_format.fc + 32));
			break;
		}

		if (regmask == 0) {
			/*
			 * We have traced back through all the
			 * instructions in the trap shadow, so this
			 * must be the one which generated the trap.
			 */
			if (fp_emulate(ins, p)) {
				/*
				 * Restore pc to the first instruction
				 * in the trap shadow.
				 */
				frame->tf_regs[FRAME_PC] = pc + 4;
				return 1;
			} else
				return 0;
		}
		pc -= 4;
	}
	return 0;
}
