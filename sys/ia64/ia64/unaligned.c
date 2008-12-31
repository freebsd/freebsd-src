/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD: src/sys/ia64/ia64/unaligned.c,v 1.13.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <ia64/disasm/disasm.h>

static int ia64_unaligned_print = 0;
SYSCTL_INT(_debug, OID_AUTO, unaligned_print, CTLFLAG_RW,
    &ia64_unaligned_print, 0, "warn about unaligned accesses");

static int ia64_unaligned_test = 0;
SYSCTL_INT(_debug, OID_AUTO, unaligned_test, CTLFLAG_RW,
    &ia64_unaligned_test, 0, "test emulation when PSR.ac is set");

static void *
fpreg_ptr(mcontext_t *mc, int fr)
{
	union _ia64_fpreg *p;

	if (fr <= 1 || fr >= 128)
		return (NULL);
	if (fr >= 32) {
		p = &mc->mc_high_fp.fr32;
		fr -= 32;
	} else if (fr >= 16) {
		p = &mc->mc_preserved_fp.fr16;
		fr -= 16;
	} else if (fr >= 6) {
		p = &mc->mc_scratch_fp.fr6;
		fr -= 6;
	} else {
		p = &mc->mc_preserved_fp.fr2;
		fr -= 2;
	}
	return ((void*)(p + fr));
}

static void *
greg_ptr(mcontext_t *mc, int gr)
{
	uint64_t *p;
	int nslots;

	if (gr <= 0 || gr >= 32 + (mc->mc_special.cfm & 0x7f))
		return (NULL);
	if (gr >= 32) {
	 	nslots = IA64_CFM_SOF(mc->mc_special.cfm) - gr + 32;
		p = (void *)ia64_bsp_adjust(mc->mc_special.bspstore, -nslots);
		gr = 0;
	} else if (gr >= 14) {
		p = &mc->mc_scratch.gr14;
		gr -= 14;
	} else if (gr == 13) {
		p = &mc->mc_special.tp;
		gr = 0;
	} else if (gr == 12) {
		p = &mc->mc_special.sp;
		gr = 0;
	} else if (gr >= 8) {
		p = &mc->mc_scratch.gr8;
		gr -= 8;
	} else if (gr >= 4) {
		p = &mc->mc_preserved.gr4;
		gr -= 4;
	} else if (gr >= 2) {
		p = &mc->mc_scratch.gr2;
		gr -= 2;
	} else {
		p = &mc->mc_special.gp;
		gr = 0;
	}
	return ((void*)(p + gr));
}

static uint64_t
rdreg(uint64_t *addr)
{
	if ((uintptr_t)addr < VM_MAX_ADDRESS)
		return (fuword(addr));
	return (*addr);
}

static void
wrreg(uint64_t *addr, uint64_t val)
{
	if ((uintptr_t)addr < VM_MAX_ADDRESS)
		suword(addr, val);
	else
		*addr = val;
}

static int
fixup(struct asm_inst *i, mcontext_t *mc, uint64_t va)
{
	union {
		double d;
		long double e;
		uint64_t i;
		float s;
	} buf;
	void *reg;
	uint64_t postinc;

	switch (i->i_op) {
	case ASM_OP_LD2:
		copyin((void*)va, (void*)&buf.i, 2);
		reg = greg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		wrreg(reg, buf.i & 0xffffU);
		break;
	case ASM_OP_LD4:
		copyin((void*)va, (void*)&buf.i, 4);
		reg = greg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		wrreg(reg, buf.i & 0xffffffffU);
		break;
	case ASM_OP_LD8:
		copyin((void*)va, (void*)&buf.i, 8);
		reg = greg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		wrreg(reg, buf.i);
		break;
	case ASM_OP_LDFD:
		copyin((void*)va, (void*)&buf.d, sizeof(buf.d));
		reg = fpreg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldfd f6=%1;; stf.spill %0=f6" : "=m"(*(double *)reg) :
		    "m"(buf.d) : "f6");
		break;
	case ASM_OP_LDFE:
		copyin((void*)va, (void*)&buf.e, sizeof(buf.e));
		reg = fpreg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldfe f6=%1;; stf.spill %0=f6" :
		    "=m"(*(long double *)reg) : "m"(buf.e) : "f6");
		break;
	case ASM_OP_LDFS:
		copyin((void*)va, (void*)&buf.s, sizeof(buf.s));
		reg = fpreg_ptr(mc, (int)i->i_oper[1].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldfs f6=%1;; stf.spill %0=f6" : "=m"(*(float *)reg) :
		    "m"(buf.s) : "f6");
		break;
	case ASM_OP_ST2:
		reg = greg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		buf.i = rdreg(reg);
		copyout((void*)&buf.i, (void*)va, 2);
		break;
	case ASM_OP_ST4:
		reg = greg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		buf.i = rdreg(reg);
		copyout((void*)&buf.i, (void*)va, 4);
		break;
	case ASM_OP_ST8:
		reg = greg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		buf.i = rdreg(reg);
		copyout((void*)&buf.i, (void*)va, 8);
		break;
	case ASM_OP_STFD:
		reg = fpreg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldf.fill f6=%1;; stfd %0=f6" : "=m"(buf.d) :
		    "m"(*(double *)reg) : "f6");
		copyout((void*)&buf.d, (void*)va, sizeof(buf.d));
		break;
	case ASM_OP_STFE:
		reg = fpreg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldf.fill f6=%1;; stfe %0=f6" : "=m"(buf.e) :
		    "m"(*(long double *)reg) : "f6");
		copyout((void*)&buf.e, (void*)va, sizeof(buf.e));
		break;
	case ASM_OP_STFS:
		reg = fpreg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		__asm("ldf.fill f6=%1;; stfs %0=f6" : "=m"(buf.s) :
		    "m"(*(float *)reg) : "f6");
		copyout((void*)&buf.s, (void*)va, sizeof(buf.s));
		break;
	default:
		return (ENOENT);
	}

	/* Handle post-increment. */
	if (i->i_oper[3].o_type == ASM_OPER_GREG) {
		reg = greg_ptr(mc, (int)i->i_oper[3].o_value);
		if (reg == NULL)
			return (EINVAL);
		postinc = rdreg(reg);
	} else
		postinc = (i->i_oper[3].o_type == ASM_OPER_IMM)
		    ? i->i_oper[3].o_value : 0;
	if (postinc != 0) {
		if (i->i_oper[1].o_type == ASM_OPER_MEM)
			reg = greg_ptr(mc, (int)i->i_oper[1].o_value);
		else
			reg = greg_ptr(mc, (int)i->i_oper[2].o_value);
		if (reg == NULL)
			return (EINVAL);
		postinc += rdreg(reg);
		wrreg(reg, postinc);
	}
	return (0);
}

int
unaligned_fixup(struct trapframe *tf, struct thread *td)
{
	mcontext_t context;
	struct asm_bundle bundle;
	int error, slot;

	slot = ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_0) ? 0 :
	    ((tf->tf_special.psr & IA64_PSR_RI) == IA64_PSR_RI_1) ? 1 : 2;

	if (ia64_unaligned_print) {
		uprintf("pid %d (%s): unaligned access: va=0x%lx, pc=0x%lx\n",
		    td->td_proc->p_pid, td->td_proc->p_comm,
		    tf->tf_special.ifa, tf->tf_special.iip + slot);
	}

	/*
	 * If PSR.ac is set, the process wants to be signalled about mis-
	 * aligned loads and stores. Send it a SIGBUS. In order for us to
	 * test the emulation of misaligned loads and stores, we have a
	 * sysctl that tells us that we must emulate the load or store,
	 * instead of sending the signal. We need the sysctl because if
	 * PSR.ac is not set, the CPU may (and likely will) deal with the
	 * misaligned load or store itself. As such, we won't get the
	 * exception.
	 */
	if ((tf->tf_special.psr & IA64_PSR_AC) && !ia64_unaligned_test)
		return (SIGBUS);

	if (!asm_decode(tf->tf_special.iip, &bundle))
		return (SIGILL);

	get_mcontext(td, &context, 0);

	error = fixup(bundle.b_inst + slot, &context, tf->tf_special.ifa);
	if (error == ENOENT) {
		printf("unhandled misaligned memory access:\n\t");
		asm_print_inst(&bundle, slot, tf->tf_special.iip);
		return (SIGILL);
	} else if (error != 0)
		return (SIGBUS);

	set_mcontext(td, &context);

	/* Advance to the next instruction. */
	if (slot == 2) {
		tf->tf_special.psr &= ~IA64_PSR_RI;
		tf->tf_special.iip += 16;
	} else
		tf->tf_special.psr += IA64_PSR_RI_1;

	return (0);
}
