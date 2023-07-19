/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>

#include <sys/dtrace.h>
#include <cddl/dev/dtrace/dtrace_cddl.h>

#include "kinst.h"

DPCPU_DEFINE_STATIC(struct kinst_cpu_state, kinst_state);

#define _MATCH_REG(reg)	\
	(offsetof(struct trapframe, tf_ ## reg) / sizeof(register_t))

static int
kinst_regoff(struct trapframe *frame, int n)
{
	switch (n) {
	case 0:
		/* There is no zero register in the trapframe structure. */
		return (-1);
	case 1:
		return (_MATCH_REG(ra));
	case 2:
		return (_MATCH_REG(sp));
	case 3:
		return (_MATCH_REG(gp));
	case 4:
		return (_MATCH_REG(tp));
	case 5 ... 7:
		return (_MATCH_REG(t[n - 5]));
	case 8 ... 9:
		return (_MATCH_REG(s[n - 8]));
	case 10 ... 17:
		return (_MATCH_REG(a[n - 10]));
	case 18 ... 27:
		return (_MATCH_REG(s[n - 18 + 2]));
	case 28 ... 31:
		return (_MATCH_REG(t[n - 28 + 3]));
	default:
		panic("%s: unhandled register index %d", __func__, n);
	}
}

static int
kinst_c_regoff(struct trapframe *frame, int n)
{
	switch (n) {
	case 0 ... 1:
		return (_MATCH_REG(s[n]));
	case 2 ... 7:
		return (_MATCH_REG(a[n - 2]));
	default:
		panic("%s: unhandled register index %d", __func__, n);
	}
}

#undef _MATCH_REG

static int
kinst_emulate(struct trapframe *frame, const struct kinst_probe *kp)
{
	kinst_patchval_t instr = kp->kp_savedval;
	register_t prevpc;
	uint64_t imm;
	uint16_t off;
	uint8_t funct;

	if (kp->kp_md.instlen == INSN_SIZE) {
#define rs1_index	((instr & RS1_MASK) >> RS1_SHIFT)
#define rs2_index	((instr & RS2_MASK) >> RS2_SHIFT)
#define rd_index	((instr & RD_MASK) >> RD_SHIFT)
#define rs1		((register_t *)frame)[kinst_regoff(frame, rs1_index)]
#define rs2		((register_t *)frame)[kinst_regoff(frame, rs2_index)]
#define rd		((register_t *)frame)[kinst_regoff(frame, rd_index)]
#define rs1_lval	(rs1_index != 0 ? rs1 : 0)
#define rs2_lval	(rs2_index != 0 ? rs2 : 0)
		switch (instr & 0x7f) {
		case 0b1101111: /* jal */
			imm = 0;
			imm |= ((instr >> 21) & 0x03ff) << 1;
			imm |= ((instr >> 20) & 0x0001) << 11;
			imm |= ((instr >> 12) & 0x00ff) << 12;
			imm |= ((instr >> 31) & 0x0001) << 20;
			if (imm & 0x0000000000100000)
				imm |= 0xfffffffffff00000;
			if (rd_index != 0)
				rd = frame->tf_sepc + INSN_SIZE;
			frame->tf_sepc += imm;
			break;
		case 0b1100111:	/* jalr */
			prevpc = frame->tf_sepc;
			imm = (instr & IMM_MASK) >> IMM_SHIFT;
			if (imm & 0x0000000000000800)
				imm |= 0xfffffffffffff000;
			frame->tf_sepc = (rs1_lval + imm) & ~1;
			if (rd_index != 0)
				rd = prevpc + INSN_SIZE;
			break;
		case 0b1100011:	/* branch */
			imm = 0;
			imm |= ((instr >> 8) & 0x000f) << 1;
			imm |= ((instr >> 25) & 0x003f) << 5;
			imm |= ((instr >> 7) & 0x0001) << 11;
			imm |= ((instr >> 31) & 0x0001) << 12;
			if (imm & 0x0000000000001000)
				imm |= 0xfffffffffffff000;
			funct = (instr >> 12) & 0x07;
			switch (funct) {
			case 0b000:	/* beq */
				if (rs1_lval == rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			case 0b001:	/* bne */
				if (rs1_lval != rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			case 0b100:	/* blt */
				if ((int64_t)rs1_lval < (int64_t)rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			case 0b110:	/* bltu */
				if ((uint64_t)rs1_lval < (uint64_t)rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			case 0b101:	/* bge */
				if ((int64_t)rs1_lval >= (int64_t)rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			case 0b111:	/* bgeu */
				if ((uint64_t)rs1_lval >= (uint64_t)rs2_lval)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_SIZE;
				break;
			}
			break;
		case 0b0010111:	/* auipc */
			imm = instr & 0xfffff000;
			rd = frame->tf_sepc +
			    (imm & 0x0000000080000000 ?
			    imm | 0xffffffff80000000 : imm);
			frame->tf_sepc += INSN_SIZE;
			break;
		}
#undef rs1_lval
#undef rs2_lval
#undef rs1
#undef rs2
#undef rd
#undef rs1_index
#undef rs2_index
#undef rd_index
	} else {
		switch (instr & 0x03) {
#define rs1	\
	((register_t *)frame)[kinst_c_regoff(frame, (instr >> 7) & 0x07)]
		case 0b01:
			funct = (instr >> 13) & 0x07;
			switch (funct) {
			case 0b101:	/* c.j */
				off = (instr >> 2) & 0x07ff;
				imm = 0;
				imm |= ((off >> 1) & 0x07) << 1;
				imm |= ((off >> 9) & 0x01) << 4;
				imm |= ((off >> 0) & 0x01) << 5;
				imm |= ((off >> 5) & 0x01) << 6;
				imm |= ((off >> 4) & 0x01) << 7;
				imm |= ((off >> 7) & 0x03) << 8;
				imm |= ((off >> 6) & 0x01) << 10;
				imm |= ((off >> 10) & 0x01) << 11;
				if (imm & 0x0000000000000800)
					imm |= 0xfffffffffffff000;
				frame->tf_sepc += imm;
				break;
			case 0b110:	/* c.beqz */
			case 0b111:	/* c.bnez */
				imm = 0;
				imm |= ((instr >> 3) & 0x03) << 1;
				imm |= ((instr >> 10) & 0x03) << 3;
				imm |= ((instr >> 2) & 0x01) << 5;
				imm |= ((instr >> 5) & 0x03) << 6;
				imm |= ((instr >> 12) & 0x01) << 8;
				if (imm & 0x0000000000000100)
					imm |= 0xffffffffffffff00;
				if (funct == 0b110 && rs1 == 0)
					frame->tf_sepc += imm;
				else if (funct == 0b111 && rs1 != 0)
					frame->tf_sepc += imm;
				else
					frame->tf_sepc += INSN_C_SIZE;
				break;
			}
			break;
#undef rs1
#define rs1_index	((instr & RD_MASK) >> RD_SHIFT)
#define rs1		((register_t *)frame)[kinst_regoff(frame, rs1_index)]
		case 0b10:
			funct = (instr >> 13) & 0x07;
			if (funct == 0b100 && rs1_index != 0) {
				/* c.jr/c.jalr */
				prevpc = frame->tf_sepc;
				frame->tf_sepc = rs1;
				if (((instr >> 12) & 0x01) != 0)
					frame->tf_ra = prevpc + INSN_C_SIZE;
			}
			break;
#undef rs1
#undef rs1_index
		}
	}

	return (MATCH_C_NOP);
}

static int
kinst_jump_next_instr(struct trapframe *frame, const struct kinst_probe *kp)
{
	frame->tf_sepc = (register_t)((const uint8_t *)kp->kp_patchpoint +
	    kp->kp_md.instlen);

	return (MATCH_C_NOP);
}

static void
kinst_trampoline_populate(struct kinst_probe *kp)
{
	static uint16_t nop = MATCH_C_NOP;
	static uint32_t ebreak = MATCH_EBREAK;
	int ilen;

	ilen = kp->kp_md.instlen;
	kinst_memcpy(kp->kp_tramp, &kp->kp_savedval, ilen);

	/*
	 * Since we cannot encode large displacements in a single instruction
	 * in order to encode a far-jump back to the next instruction, and we
	 * also cannot clobber a register inside the trampoline, we execute a
	 * breakpoint after the copied instruction. kinst_invop() is
	 * responsible for detecting this special case and performing the
	 * "jump" manually.
	 *
	 * Add a NOP after a compressed instruction for padding.
	 */
	if (ilen == INSN_C_SIZE)
		kinst_memcpy(&kp->kp_tramp[ilen], &nop, INSN_C_SIZE);

	kinst_memcpy(&kp->kp_tramp[INSN_SIZE], &ebreak, INSN_SIZE);

	fence_i();
}

/*
 * There are two ways by which an instruction is traced:
 *
 * - By using the trampoline.
 * - By emulating it in software (see kinst_emulate()).
 *
 * The trampoline is used for instructions that can be copied and executed
 * as-is without additional modification. However, instructions that use
 * PC-relative addressing have to be emulated, because RISC-V doesn't allow
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
		if ((ks->status & SSTATUS_SPIE) != 0)
			frame->tf_sstatus |= SSTATUS_SPIE;
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
	 * Cache the current SSTATUS and clear interrupts for the
	 * duration of the double breakpoint.
	 */
	ks->status = frame->tf_sstatus;
	frame->tf_sstatus &= ~SSTATUS_SPIE;
	frame->tf_sepc = (register_t)kp->kp_tramp;

	return (MATCH_C_NOP);
}

void
kinst_patch_tracepoint(struct kinst_probe *kp, kinst_patchval_t val)
{
	switch (kp->kp_patchval) {
	case KINST_C_PATCHVAL:
		*(uint16_t *)kp->kp_patchpoint = (uint16_t)val;
		fence_i();
		break;
	case KINST_PATCHVAL:
		*kp->kp_patchpoint = val;
		fence_i();
		break;
	}
}

static void
kinst_instr_dissect(struct kinst_probe *kp, int instrsize)
{
	struct kinst_probe_md *kpmd;
	kinst_patchval_t instr = kp->kp_savedval;
	uint8_t funct;

	kpmd = &kp->kp_md;
	kpmd->instlen = instrsize;
	kpmd->emulate = false;

	/*
	 * The following instructions use PC-relative addressing and need to be
	 * emulated in software.
	 */
	if (kpmd->instlen == INSN_SIZE) {
		switch (instr & 0x7f) {
		case 0b1101111: /* jal */
		case 0b1100111:	/* jalr */
		case 0b1100011:	/* branch */
		case 0b0010111:	/* auipc */
			kpmd->emulate = true;
			break;
		}
	} else {
		switch (instr & 0x03) {
		case 0b01:
			funct = (instr >> 13) & 0x07;
			switch (funct) {
			case 0b101:	/* c.j */
			case 0b110:	/* c.beqz */
			case 0b111:	/* c.bnez */
				kpmd->emulate = true;
				break;
			}
			break;
		case 0b10:
			funct = (instr >> 13) & 0x07;
			if (funct == 0b100 &&
			    ((instr >> 7) & 0x1f) != 0 &&
			    ((instr >> 2) & 0x1f) == 0)
				kpmd->emulate = true;	/* c.jr/c.jalr */
			break;
		}
	}

	if (!kpmd->emulate)
		kinst_trampoline_populate(kp);
}

static bool
kinst_instr_system(kinst_patchval_t instr)
{
	if (dtrace_match_opcode(instr, MATCH_C_EBREAK, MASK_C_EBREAK) ||
	    (instr & 0x7f) == 0b1110011)
		return (true);

	return (false);
}

static bool
kinst_instr_lr(kinst_patchval_t instr)
{
	if (dtrace_match_opcode(instr, MATCH_LR_W, MASK_LR_W) ||
	    dtrace_match_opcode(instr, MATCH_LR_D, MASK_LR_D))
		return (true);

	return (false);
}

static bool
kinst_instr_sc(kinst_patchval_t instr)
{
	if (dtrace_match_opcode(instr, MATCH_SC_W, MASK_SC_W) ||
	    dtrace_match_opcode(instr, MATCH_SC_D, MASK_SC_D))
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
	kinst_patchval_t *insn, v;
	uint8_t *instr, *limit;
	int instrsize, n, off;
	bool lrsc_block, store_found, ret_found;

	pd = opaque;
	func = symval->name;

	if (kinst_excluded(func))
		return (0);
	if (strcmp(func, pd->kpd_func) != 0)
		return (0);

	instr = (uint8_t *)(symval->value);
	limit = (uint8_t *)(symval->value + symval->size);
	if (instr >= limit)
		return (0);

	/* Check for the usual function prologue. */
	for (insn = (kinst_patchval_t *)instr;
	    insn < (kinst_patchval_t *)limit; insn++) {
		if (dtrace_instr_sdsp(&insn) || dtrace_instr_c_sdsp(&insn))
			store_found = true;
		else if (dtrace_instr_ret(&insn) || dtrace_instr_c_ret(&insn))
			ret_found = true;
		if (store_found && ret_found)
			break;
	}
	if (!store_found || !ret_found)
		return (0);

	n = 0;
	lrsc_block = false;
	while (instr < limit) {
		instrsize = dtrace_instr_size(instr);
		off = (int)(instr - (uint8_t *)symval->value);

		/*
		 * Avoid undefined behavior (i.e simply casting `*instr` to
		 * `kinst_patchval_t`) in case the pointer is unaligned.
		 * memcpy() can safely operate on unaligned pointers.
		 */
		memcpy(&v, instr, sizeof(kinst_patchval_t));

		/* Skip SYSTEM instructions. */
		if (kinst_instr_system(v))
			goto cont;

		/*
		 * Skip LR/SC blocks used to build atomic operations. If a
		 * breakpoint is placed in a LR/SC block, the loop becomes
		 * unconstrained. In this case we violate the operation and the
		 * loop might fail on some implementations (see section 8.3 of
		 * the RISC-V unprivileged spec).
		 */
		if (kinst_instr_lr(v))
			lrsc_block = true;
		else if (kinst_instr_sc(v)) {
			lrsc_block = false;
			goto cont;
		}
		if (lrsc_block)
			goto cont;

		if (pd->kpd_off != -1 && off != pd->kpd_off)
			goto cont;

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
		kp->kp_patchpoint = (kinst_patchval_t *)instr;
		kp->kp_savedval = v;
		if (instrsize == INSN_SIZE)
			kp->kp_patchval = KINST_PATCHVAL;
		else
			kp->kp_patchval = KINST_C_PATCHVAL;
		if ((kp->kp_tramp = kinst_trampoline_alloc(M_WAITOK)) == NULL) {
			KINST_LOG("cannot allocate trampoline for %p", instr);
			return (ENOMEM);
		}

		kinst_instr_dissect(kp, instrsize);
		kinst_probe_create(kp, lf);
cont:
		instr += instrsize;
	}
	if (lrsc_block)
		KINST_LOG("warning: unterminated LR/SC block");

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
	if (strcmp(name, "cpu_exception_handler") == 0 ||
            strcmp(name, "cpu_exception_handler_supervisor") == 0 ||
            strcmp(name, "cpu_exception_handler_user") == 0 ||
            strcmp(name, "do_trap_supervisor") == 0 ||
            strcmp(name, "do_trap_user") == 0)
                return (true);

	return (false);
}
