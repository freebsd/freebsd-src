/*-
 * Copyright (c) 2017 Andrew Turner
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/frame.h>
#include <machine/undefined.h>

MALLOC_DEFINE(M_UNDEF, "undefhandler", "Undefined instruction handler data");

struct undef_handler {
	LIST_ENTRY(undef_handler) uh_link;
	undef_handler_t		uh_handler;
};

/*
 * Create two undefined instruction handler lists, one for userspace, one for
 * the kernel. This allows us to handle instructions that will trap
 */
LIST_HEAD(, undef_handler) undef_handlers[2];

#define	MRS_MASK			0xfff00000
#define	MRS_VALUE			0xd5300000
#define	MRS_SPECIAL(insn)		((insn) & 0x000fffe0)
#define	MRS_REGISTER(insn)		((insn) & 0x0000001f)
#define	 MRS_Op0_SHIFT			19
#define	 MRS_Op0_MASK			0x00080000
#define	 MRS_Op1_SHIFT			16
#define	 MRS_Op1_MASK			0x00070000
#define	 MRS_CRn_SHIFT			12
#define	 MRS_CRn_MASK			0x0000f000
#define	 MRS_CRm_SHIFT			8
#define	 MRS_CRm_MASK			0x00000f00
#define	 MRS_Op2_SHIFT			5
#define	 MRS_Op2_MASK			0x000000e0
#define	 MRS_Rt_SHIFT			0
#define	 MRS_Rt_MASK			0x0000001f

static inline int
mrs_Op0(uint32_t insn)
{

	/* op0 is encoded without the top bit in a mrs instruction */
	return (2 | ((insn & MRS_Op0_MASK) >> MRS_Op0_SHIFT));
}

#define	MRS_GET(op)						\
static inline int						\
mrs_##op(uint32_t insn)						\
{								\
								\
	return ((insn & MRS_##op##_MASK) >> MRS_##op##_SHIFT);	\
}
MRS_GET(Op1)
MRS_GET(CRn)
MRS_GET(CRm)
MRS_GET(Op2)

struct mrs_safe_value {
	u_int		CRm;
	u_int		Op2;
	uint64_t	value;
};

static struct mrs_safe_value safe_values[] = {
	{	/* id_aa64pfr0_el1 */
		.CRm = 4,
		.Op2 = 0,
		.value = ID_AA64PFR0_ADV_SIMD_NONE | ID_AA64PFR0_FP_NONE |
		    ID_AA64PFR0_EL1_64 | ID_AA64PFR0_EL0_64,
	},
	{	/* id_aa64dfr0_el1 */
		.CRm = 5,
		.Op2 = 0,
		.value = ID_AA64DFR0_DEBUG_VER_8,
	},
};

static int
user_mrs_handler(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	uint64_t value;
	int CRm, Op2, i, reg;

	if ((insn & MRS_MASK) != MRS_VALUE)
		return (0);

	/*
	 * We only emulate Op0 == 3, Op1 == 0, CRn == 0, CRm == {0, 4-7}.
	 * These are in the EL1 CPU identification space.
	 * CRm == 0 holds MIDR_EL1, MPIDR_EL1, and REVID_EL1.
	 * CRm == {4-7} holds the ID_AA64 registers.
	 *
	 * For full details see the ARMv8 ARM (ARM DDI 0487C.a)
	 * Table D9-2 System instruction encodings for non-Debug System
	 * register accesses.
	 */
	if (mrs_Op0(insn) != 3 || mrs_Op1(insn) != 0 || mrs_CRn(insn) != 0)
		return (0);

	CRm = mrs_CRm(insn);
	if (CRm > 7 || (CRm < 4 && CRm != 0))
		return (0);

	Op2 = mrs_Op2(insn);
	value = 0;

	for (i = 0; i < nitems(safe_values); i++) {
		if (safe_values[i].CRm == CRm && safe_values[i].Op2 == Op2) {
			value = safe_values[i].value;
			break;
		}
	}

	if (CRm == 0) {
		switch (Op2) {
		case 0:
			value = READ_SPECIALREG(midr_el1);
			break;
		case 5:
			value = READ_SPECIALREG(mpidr_el1);
			break;
		case 6:
			value = READ_SPECIALREG(revidr_el1);
			break;
		default:
			return (0);
		}
	}

	/*
	 * We will handle this instruction, move to the next so we
	 * don't trap here again.
	 */
	frame->tf_elr += INSN_SIZE;

	reg = MRS_REGISTER(insn);
	/* If reg is 31 then write to xzr, i.e. do nothing */
	if (reg == 31)
		return (1);

	if (reg < nitems(frame->tf_x))
		frame->tf_x[reg] = value;
	else if (reg == 30)
		frame->tf_lr = value;

	return (1);
}

/*
 * Work around a bug in QEMU prior to 2.5.1 where reading unknown ID
 * registers would raise an exception when they should return 0.
 */
static int
id_aa64mmfr2_handler(vm_offset_t va, uint32_t insn, struct trapframe *frame,
    uint32_t esr)
{
	int reg;

#define	 MRS_ID_AA64MMFR2_EL0_MASK	(MRS_MASK | 0x000fffe0)
#define	 MRS_ID_AA64MMFR2_EL0_VALUE	(MRS_VALUE | 0x00080740)

	/* mrs xn, id_aa64mfr2_el1 */
	if ((insn & MRS_ID_AA64MMFR2_EL0_MASK) == MRS_ID_AA64MMFR2_EL0_VALUE) {
		reg = MRS_REGISTER(insn);

		frame->tf_elr += INSN_SIZE;
		if (reg < nitems(frame->tf_x)) {
			frame->tf_x[reg] = 0;
		} else if (reg == 30) {
			frame->tf_lr = 0;
		}
		/* If reg is 32 then write to xzr, i.e. do nothing */

		return (1);
	}
	return (0);
}

void
undef_init(void)
{

	LIST_INIT(&undef_handlers[0]);
	LIST_INIT(&undef_handlers[1]);

	install_undef_handler(true, user_mrs_handler);
	install_undef_handler(false, id_aa64mmfr2_handler);
}

void *
install_undef_handler(bool user, undef_handler_t func)
{
	struct undef_handler *uh;

	uh = malloc(sizeof(*uh), M_UNDEF, M_WAITOK);
	uh->uh_handler = func;
	LIST_INSERT_HEAD(&undef_handlers[user ? 0 : 1], uh, uh_link);

	return (uh);
}

void
remove_undef_handler(void *handle)
{
	struct undef_handler *uh;

	uh = handle;
	LIST_REMOVE(uh, uh_link);
	free(handle, M_UNDEF);
}

int
undef_insn(u_int el, struct trapframe *frame)
{
	struct undef_handler *uh;
	uint32_t insn;
	int ret;

	KASSERT(el < 2, ("Invalid exception level %u", el));

	if (el == 0) {
		ret = fueword32((uint32_t *)frame->tf_elr, &insn);
		if (ret != 0)
			panic("Unable to read userspace faulting instruction");
	} else {
		insn = *(uint32_t *)frame->tf_elr;
	}

	LIST_FOREACH(uh, &undef_handlers[el], uh_link) {
		ret = uh->uh_handler(frame->tf_elr, insn, frame, frame->tf_esr);
		if (ret)
			return (1);
	}

	return (0);
}
