/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2022 Christos Margiolis <christos@FreeBSD.org>
 * Copyright (c) 2022 Mark Johnston <markj@FreeBSD.org>
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * Portions of this software were developed by Christos Margiolis
 * <christos@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>

#include <sys/dtrace.h>
#include <cddl/dev/dtrace/dtrace_cddl.h>

#include "kinst.h"

DPCPU_DEFINE_STATIC(struct kinst_cpu_state, kinst_state);

static int
kinst_emulate(struct trapframe *frame, const struct kinst_probe *kp)
{
	kinst_patchval_t instr = kp->kp_savedval;
	uint64_t imm;
	uint8_t cond, reg, bitpos;
	bool res;

	if (((instr >> 24) & 0x1f) == 0b10000) {
		/* adr/adrp */
		reg = instr & 0x1f;
		imm = (instr >> 29) & 0x3;
		imm |= ((instr >> 5) & 0x0007ffff) << 2;
		if (((instr >> 31) & 0x1) == 0) {
			/* adr */
			if (imm & 0x0000000000100000)
				imm |= 0xfffffffffff00000;
			frame->tf_x[reg] = frame->tf_elr + imm;
		} else {
			/* adrp */
			imm <<= 12;
			if (imm & 0x0000000100000000)
				imm |= 0xffffffff00000000;
			frame->tf_x[reg] = (frame->tf_elr & ~0xfff) + imm;
		}
		frame->tf_elr += INSN_SIZE;
	} else if (((instr >> 26) & 0x3f) == 0b000101) {
		/* b */
		imm = instr & 0x03ffffff;
		if (imm & 0x0000000002000000)
			imm |= 0xfffffffffe000000;
		frame->tf_elr += imm << 2;
	} else if (((instr >> 24) & 0xff) == 0b01010100) {
		/* b.cond */
		imm = (instr >> 5) & 0x0007ffff;
		if (imm & 0x0000000000040000)
			imm |= 0xfffffffffffc0000;
		cond = instr & 0xf;
		switch ((cond >> 1) & 0x7) {
		case 0b000:	/* eq/ne */
			res = (frame->tf_spsr & PSR_Z) != 0;
			break;
		case 0b001:	/* cs/cc */
			res = (frame->tf_spsr & PSR_C) != 0;
			break;
		case 0b010:	/* mi/pl */
			res = (frame->tf_spsr & PSR_N) != 0;
			break;
		case 0b011:	/* vs/vc */
			res = (frame->tf_spsr & PSR_V) != 0;
			break;
		case 0b100:	/* hi/ls */
			res = ((frame->tf_spsr & PSR_C) != 0) &&
			    ((frame->tf_spsr & PSR_Z) == 0);
			break;
		case 0b101:	/* ge/lt */
			res = ((frame->tf_spsr & PSR_N) != 0) ==
			    ((frame->tf_spsr & PSR_V) != 0);
			break;
		case 0b110:	/* gt/le */
			res = ((frame->tf_spsr & PSR_Z) == 0) &&
			    (((frame->tf_spsr & PSR_N) != 0) ==
			    ((frame->tf_spsr & PSR_V) != 0));
			break;
		case 0b111:	/* al */
			res = 1;
			break;
		}
		if ((cond & 0x1) && cond != 0b1111)
			res = !res;
		if (res)
			frame->tf_elr += imm << 2;
		else
			frame->tf_elr += INSN_SIZE;
	} else if (((instr >> 26) & 0x3f) == 0b100101) {
		/* bl */
		imm = instr & 0x03ffffff;
		if (imm & 0x0000000002000000)
			imm |= 0xfffffffffe000000;
		frame->tf_lr = frame->tf_elr + INSN_SIZE;
		frame->tf_elr += imm << 2;
	} else if (((instr >> 25) & 0x3f) == 0b011010) {
		/* cbnz/cbz */
		cond = (instr >> 24) & 0x1;
		reg = instr & 0x1f;
		imm = (instr >> 5) & 0x0007ffff;
		if (imm & 0x0000000000040000)
			imm |= 0xfffffffffffc0000;
		if (cond == 1 && frame->tf_x[reg] != 0)
			/* cbnz */
			frame->tf_elr += imm << 2;
		else if (cond == 0 && frame->tf_x[reg] == 0)
			/* cbz */
			frame->tf_elr += imm << 2;
		else
			frame->tf_elr += INSN_SIZE;
	} else if (((instr >> 25) & 0x3f) == 0b011011) {
		/* tbnz/tbz */
		cond = (instr >> 24) & 0x1;
		reg = instr & 0x1f;
		bitpos = (instr >> 19) & 0x1f;
		bitpos |= ((instr >> 31) & 0x1) << 5;
		imm = (instr >> 5) & 0x3fff;
		if (imm & 0x0000000000002000)
			imm |= 0xffffffffffffe000;
		if (cond == 1 && (frame->tf_x[reg] & (1 << bitpos)) != 0)
			/* tbnz */
			frame->tf_elr += imm << 2;
		else if (cond == 0 && (frame->tf_x[reg] & (1 << bitpos)) == 0)
			/* tbz */
			frame->tf_elr += imm << 2;
		else
			frame->tf_elr += INSN_SIZE;
	}

	return (0);
}

static int
kinst_jump_next_instr(struct trapframe *frame, const struct kinst_probe *kp)
{
	frame->tf_elr = (register_t)((const uint8_t *)kp->kp_patchpoint +
	    INSN_SIZE);

	return (0);
}

static void
kinst_trampoline_populate(struct kinst_probe *kp)
{
	static uint32_t bpt = KINST_PATCHVAL;

	kinst_memcpy(kp->kp_tramp, &kp->kp_savedval, INSN_SIZE);
	kinst_memcpy(&kp->kp_tramp[INSN_SIZE], &bpt, INSN_SIZE);

	cpu_icache_sync_range(kp->kp_tramp, KINST_TRAMP_SIZE);
}

/*
 * There are two ways by which an instruction is traced:
 *
 * - By using the trampoline.
 * - By emulating it in software (see kinst_emulate()).
 *
 * The trampoline is used for instructions that can be copied and executed
 * as-is without additional modification. However, instructions that use
 * PC-relative addressing have to be emulated, because ARM64 doesn't allow
 * encoding of large displacements in a single instruction, and since we cannot
 * clobber a register in order to encode the two-instruction sequence needed to
 * create large displacements, we cannot use the trampoline at all.
 * Fortunately, the instructions are simple enough to be emulated in just a few
 * lines of code.
 *
 * The problem discussed above also means that, unlike amd64, we cannot encode
 * a far-jump back from the trampoline to the next instruction. The mechanism
 * employed to achieve this functionality, is to use a breakpoint instead of a
 * jump after the copied instruction. This breakpoint is detected and handled
 * by kinst_invop(), which performs the jump back to the next instruction
 * manually (see kinst_jump_next_instr()).
 */
int
kinst_invop(uintptr_t addr, struct trapframe *frame, uintptr_t scratch)
{
	solaris_cpu_t *cpu;
	struct kinst_cpu_state *ks;
	const struct kinst_probe *kp;

	ks = DPCPU_PTR(kinst_state);

	/*
	 * Detect if the breakpoint was triggered by the trampoline, and
	 * manually set the PC to the next instruction.
	 */
	if (ks->state == KINST_PROBE_FIRED &&
	    addr == (uintptr_t)(ks->kp->kp_tramp + INSN_SIZE)) {
		/*
		 * Restore interrupts if they were enabled prior to the first
		 * breakpoint.
		 */
		if ((ks->status & PSR_I) == 0)
			frame->tf_spsr &= ~PSR_I;
		ks->state = KINST_PROBE_ARMED;
		return (kinst_jump_next_instr(frame, ks->kp));
	}

	LIST_FOREACH(kp, KINST_GETPROBE(addr), kp_hashnext) {
		if ((uintptr_t)kp->kp_patchpoint == addr)
			break;
	}
	if (kp == NULL)
		return (0);

	cpu = &solaris_cpu[curcpu];
	cpu->cpu_dtrace_caller = addr;
	dtrace_probe(kp->kp_id, 0, 0, 0, 0, 0);
	cpu->cpu_dtrace_caller = 0;

	if (kp->kp_md.emulate)
		return (kinst_emulate(frame, kp));

	ks->state = KINST_PROBE_FIRED;
	ks->kp = kp;

	/*
	 * Cache the current SPSR and clear interrupts for the duration
	 * of the double breakpoint.
	 */
	ks->status = frame->tf_spsr;
	frame->tf_spsr |= PSR_I;
	frame->tf_elr = (register_t)kp->kp_tramp;

	return (0);
}

void
kinst_patch_tracepoint(struct kinst_probe *kp, kinst_patchval_t val)
{
	void *addr;

	if (!arm64_get_writable_addr(kp->kp_patchpoint, &addr))
		panic("%s: Unable to write new instruction", __func__);
	*(kinst_patchval_t *)addr = val;
	cpu_icache_sync_range(kp->kp_patchpoint, INSN_SIZE);
}

static void
kinst_instr_dissect(struct kinst_probe *kp)
{
	struct kinst_probe_md *kpmd;
	kinst_patchval_t instr = kp->kp_savedval;

	kpmd = &kp->kp_md;
	kpmd->emulate = false;

	if (((instr >> 24) & 0x1f) == 0b10000)
		kpmd->emulate = true;	/* adr/adrp */
	else if (((instr >> 26) & 0x3f) == 0b000101)
		kpmd->emulate = true;	/* b */
	else if (((instr >> 24) & 0xff) == 0b01010100)
		kpmd->emulate = true;	/* b.cond */
	else if (((instr >> 26) & 0x3f) == 0b100101)
		kpmd->emulate = true;	/* bl */
	else if (((instr >> 25) & 0x3f) == 0b011010)
		kpmd->emulate = true;	/* cbnz/cbz */
	else if (((instr >> 25) & 0x3f) == 0b011011)
		kpmd->emulate = true;	/* tbnz/tbz */

	if (!kpmd->emulate)
		kinst_trampoline_populate(kp);
}

static bool
kinst_instr_ldx(kinst_patchval_t instr)
{
	if (((instr >> 22) & 0xff) == 0b00100001)
		return (true);

	return (false);
}

static bool
kinst_instr_stx(kinst_patchval_t instr)
{
	if (((instr >> 22) & 0xff) == 0b00100000)
		return (true);

	return (false);
}

int
kinst_make_probe(linker_file_t lf, int symindx, linker_symval_t *symval,
    void *opaque)
{
	struct kinst_probe *kp;
	dtrace_kinst_probedesc_t *pd;
	const char *func;
	kinst_patchval_t *instr, *limit, *tmp;
	int n, off;
	bool ldxstx_block, found;

	pd = opaque;
	func = symval->name;

	if (kinst_excluded(func))
		return (0);
	if (strcmp(func, pd->kpd_func) != 0)
		return (0);

	instr = (kinst_patchval_t *)(symval->value);
	limit = (kinst_patchval_t *)(symval->value + symval->size);
	if (instr >= limit)
		return (0);

	tmp = instr;

	/*
	 * Ignore any bti instruction at the start of the function
	 * we need to keep it there for any indirect branches calling
	 * the function on Armv8.5+
	 */
	if ((*tmp & BTI_MASK) == BTI_INSTR)
		tmp++;

	/* Look for stp (pre-indexed) operation */
	found = false;

	/*
	 * If the first instruction is a nop it's a specially marked
	 * asm function. We only support a nop first as it's not a normal
	 * part of the function prologue.
	 */
	if (*tmp == NOP_INSTR)
		found = true;
	for (; !found && tmp < limit; tmp++) {
		/*
		 * Functions start with "stp xt1, xt2, [xn, <const>]!" or
		 * "sub sp, sp, <const>".
		 *
		 * Sometimes the compiler will have a sub instruction that is
		 * not of the above type so don't stop if we see one.
		 */
		if ((*tmp & LDP_STP_MASK) == STP_64) {
			/*
			 * Assume any other store of this type means we are
			 * past the function prolog.
			 */
			if (((*tmp >> ADDR_SHIFT) & ADDR_MASK) == 31)
				found = true;
		} else if ((*tmp & SUB_MASK) == SUB_INSTR &&
		    ((*tmp >> SUB_RD_SHIFT) & SUB_R_MASK) == 31 &&
		    ((*tmp >> SUB_RN_SHIFT) & SUB_R_MASK) == 31)
			found = true;
	}

	if (!found)
		return (0);

	ldxstx_block = false;
	for (n = 0; instr < limit; instr++) {
		off = (int)((uint8_t *)instr - (uint8_t *)symval->value);

		/*
		 * Skip LDX/STX blocks that contain atomic operations. If a
		 * breakpoint is placed in a LDX/STX block, we violate the
		 * operation and the loop might fail.
		 */
		if (kinst_instr_ldx(*instr))
			ldxstx_block = true;
		else if (kinst_instr_stx(*instr)) {
			ldxstx_block = false;
			continue;
		}
		if (ldxstx_block)
			continue;

		/*
		 * XXX: Skip ADR and ADRP instructions. The arm64 exception
		 * handler has a micro-optimization where it doesn't restore
		 * callee-saved registers when returning from exceptions in
		 * EL1. This results in a panic when the kinst emulation code
		 * modifies one of those registers.
		 */
		if (((*instr >> 24) & 0x1f) == 0b10000)
			continue;

		if (pd->kpd_off != -1 && off != pd->kpd_off)
			continue;

		/*
		 * Prevent separate dtrace(1) instances from creating copies of
		 * the same probe.
		 */
		LIST_FOREACH(kp, KINST_GETPROBE(instr), kp_hashnext) {
			if (strcmp(kp->kp_func, func) == 0 &&
			    strtol(kp->kp_name, NULL, 10) == off)
				return (0);
		}
		if (++n > KINST_PROBETAB_MAX) {
			KINST_LOG("probe list full: %d entries", n);
			return (ENOMEM);
		}
		kp = malloc(sizeof(struct kinst_probe), M_KINST,
		    M_WAITOK | M_ZERO);
		kp->kp_func = func;
		snprintf(kp->kp_name, sizeof(kp->kp_name), "%d", off);
		kp->kp_patchpoint = instr;
		kp->kp_savedval = *instr;
		kp->kp_patchval = KINST_PATCHVAL;
		if ((kp->kp_tramp = kinst_trampoline_alloc(M_WAITOK)) == NULL) {
			KINST_LOG("cannot allocate trampoline for %p", instr);
			return (ENOMEM);
		}

		kinst_instr_dissect(kp);
		kinst_probe_create(kp, lf);
	}
	if (ldxstx_block)
		KINST_LOG("warning: unterminated LDX/STX block");

	return (0);
}

int
kinst_md_init(void)
{
	struct kinst_cpu_state *ks;
	int cpu;

	CPU_FOREACH(cpu) {
		ks = DPCPU_PTR(kinst_state);
		ks->state = KINST_PROBE_ARMED;
	}

	return (0);
}

void
kinst_md_deinit(void)
{
}

/*
 * Exclude machine-dependent functions that are not safe-to-trace.
 */
bool
kinst_md_excluded(const char *name)
{
	if (strcmp(name, "handle_el1h_sync") == 0 ||
	    strcmp(name, "do_el1h_sync") == 0)
                return (true);

	return (false);
}
