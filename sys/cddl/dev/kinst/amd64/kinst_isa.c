/*
 * SPDX-License-Identifier: CDDL 1.0
 *
 * Copyright 2022 Christos Margiolis <christos@FreeBSD.org>
 * Copyright 2022 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/param.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include <sys/dtrace.h>
#include <cddl/dev/dtrace/dtrace_cddl.h>
#include <dis_tables.h>

#include "kinst.h"

#define KINST_PUSHL_RBP		0x55
#define KINST_STI		0xfb
#define KINST_POPF		0x9d

#define KINST_MODRM_MOD(b)	(((b) & 0xc0) >> 6)
#define KINST_MODRM_REG(b)	(((b) & 0x38) >> 3)
#define KINST_MODRM_RM(b)	((b) & 0x07)

#define KINST_SIB_SCALE(s)	(((s) & 0xc0) >> 6)
#define KINST_SIB_INDEX(s)	(((s) & 0x38) >> 3)
#define KINST_SIB_BASE(s)	(((s) & 0x07) >> 0)

#define KINST_REX_W(r)		(((r) & 0x08) >> 3)
#define KINST_REX_R(r)		(((r) & 0x04) >> 2)
#define KINST_REX_X(r)		(((r) & 0x02) >> 1)
#define KINST_REX_B(r)		(((r) & 0x01) >> 0)

#define KINST_F_CALL		0x0001	/* instruction is a "call" */
#define KINST_F_DIRECT_CALL	0x0002	/* instruction is a direct call */
#define KINST_F_RIPREL		0x0004	/* instruction is position-dependent */
#define KINST_F_JMP		0x0008	/* instruction is a %rip-relative jmp */
#define KINST_F_MOD_DIRECT	0x0010	/* operand is not a memory address */

/*
 * Map ModR/M register bits to a trapframe offset.
 */
static int
kinst_regoff(int reg)
{
#define	_MATCH_REG(i, reg)			\
	case i:					\
		return (offsetof(struct trapframe, tf_ ## reg) / \
		    sizeof(register_t))
	switch (reg) {
	_MATCH_REG( 0, rax);
	_MATCH_REG( 1, rcx);
	_MATCH_REG( 2, rdx);
	_MATCH_REG( 3, rbx);
	_MATCH_REG( 4, rsp); /* SIB when mod != 3 */
	_MATCH_REG( 5, rbp);
	_MATCH_REG( 6, rsi);
	_MATCH_REG( 7, rdi);
	_MATCH_REG( 8, r8); /* REX.R is set */
	_MATCH_REG( 9, r9);
	_MATCH_REG(10, r10);
	_MATCH_REG(11, r11);
	_MATCH_REG(12, r12);
	_MATCH_REG(13, r13);
	_MATCH_REG(14, r14);
	_MATCH_REG(15, r15);
	}
#undef _MATCH_REG
	panic("%s: unhandled register index %d", __func__, reg);
}

/*
 * Obtain the specified register's value.
 */
static uint64_t
kinst_regval(struct trapframe *frame, int reg)
{
	if (reg == -1)
		return (0);
	return (((register_t *)frame)[kinst_regoff(reg)]);
}

static uint32_t
kinst_riprel_disp(struct kinst_probe *kp, void *dst)
{
	return ((uint32_t)((intptr_t)kp->kp_patchpoint + kp->kp_md.disp -
	    (intptr_t)dst));
}

static void
kinst_trampoline_populate(struct kinst_probe *kp, uint8_t *tramp)
{
	uint8_t *instr;
	uint32_t disp;
	int ilen;

	ilen = kp->kp_md.tinstlen;

	memcpy(tramp, kp->kp_md.template, ilen);
	if ((kp->kp_md.flags & KINST_F_RIPREL) != 0) {
		disp = kinst_riprel_disp(kp, tramp);
		memcpy(&tramp[kp->kp_md.dispoff], &disp, sizeof(uint32_t));
	}

	/*
	 * The following position-independent jmp takes us back to the
	 * original code.  It is encoded as "jmp *0(%rip)" (six bytes),
	 * followed by the absolute address of the instruction following
	 * the one that was traced (eight bytes).
	 */
	tramp[ilen + 0] = 0xff;
	tramp[ilen + 1] = 0x25;
	tramp[ilen + 2] = 0x00;
	tramp[ilen + 3] = 0x00;
	tramp[ilen + 4] = 0x00;
	tramp[ilen + 5] = 0x00;
	instr = kp->kp_patchpoint + kp->kp_md.instlen;
	memcpy(&tramp[ilen + 6], &instr, sizeof(uintptr_t));
}

int
kinst_invop(uintptr_t addr, struct trapframe *frame, uintptr_t scratch)
{
	solaris_cpu_t *cpu;
	uintptr_t *stack, retaddr;
	struct kinst_probe *kp;
	struct kinst_probe_md *kpmd;
	uint8_t *tramp;

	stack = (uintptr_t *)frame->tf_rsp;
	cpu = &solaris_cpu[curcpu];

	LIST_FOREACH(kp, KINST_GETPROBE(addr), kp_hashnext) {
		if ((uintptr_t)kp->kp_patchpoint == addr)
			break;
	}
	if (kp == NULL)
		return (0);

	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	cpu->cpu_dtrace_caller = stack[0];
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT | CPU_DTRACE_BADADDR);
	dtrace_probe(kp->kp_id, 0, 0, 0, 0, 0);
	cpu->cpu_dtrace_caller = 0;

	kpmd = &kp->kp_md;
	if ((kpmd->flags & KINST_F_CALL) != 0) {
		/*
		 * dtrace_invop_start() reserves space on the stack to
		 * store the return address of the call instruction.
		 */
		retaddr = (uintptr_t)(kp->kp_patchpoint + kpmd->instlen);
		*(uintptr_t *)scratch = retaddr;

		if ((kpmd->flags & KINST_F_DIRECT_CALL) != 0) {
			frame->tf_rip = (uintptr_t)(kp->kp_patchpoint +
			    kpmd->disp + kpmd->instlen);
		} else {
			register_t rval;

			if (kpmd->reg1 == -1 && kpmd->reg2 == -1) {
				/* rip-relative */
				rval = frame->tf_rip - 1 + kpmd->instlen;
			} else {
				/* indirect */
				rval = kinst_regval(frame, kpmd->reg1) +
				    (kinst_regval(frame, kpmd->reg2) <<
				    kpmd->scale);
			}

			if ((kpmd->flags & KINST_F_MOD_DIRECT) != 0) {
				frame->tf_rip = rval + kpmd->disp;
			} else {
				frame->tf_rip =
				    *(uintptr_t *)(rval + kpmd->disp);
			}
		}
		return (DTRACE_INVOP_CALL);
	} else {
		tramp = curthread->t_kinst;
		if (tramp == NULL) {
			/*
			 * A trampoline allocation failed, so this probe is
			 * effectively disabled.  Restore the original
			 * instruction.
			 *
			 * We can't safely print anything here, but the
			 * trampoline allocator should have left a breadcrumb in
			 * the dmesg.
			 */
			kinst_patch_tracepoint(kp, kp->kp_savedval);
			frame->tf_rip = (register_t)kp->kp_patchpoint;
		} else {
			kinst_trampoline_populate(kp, tramp);
			frame->tf_rip = (register_t)tramp;
		}
		return (DTRACE_INVOP_NOP);
	}
}

void
kinst_patch_tracepoint(struct kinst_probe *kp, kinst_patchval_t val)
{
	register_t reg;
	int oldwp;

	reg = intr_disable();
	oldwp = disable_wp();
	*kp->kp_patchpoint = val;
	restore_wp(oldwp);
	intr_restore(reg);
}

static void
kinst_set_disp8(struct kinst_probe *kp, uint8_t byte)
{
	kp->kp_md.disp = (int64_t)(int8_t)byte;
}

static void
kinst_set_disp32(struct kinst_probe *kp, uint8_t *bytes)
{
	int32_t disp32;

	memcpy(&disp32, bytes, sizeof(disp32));
	kp->kp_md.disp = (int64_t)disp32;
}

static int
kinst_dis_get_byte(void *p)
{
	int ret;
	uint8_t **instr = p;

	ret = **instr;
	(*instr)++;

	return (ret);
}

/*
 * Set up all of the state needed to faithfully execute a probed instruction.
 *
 * In the simple case, we copy the instruction unmodified to a per-thread
 * trampoline, wherein it is followed by a jump back to the original code.
 * - Instructions can have %rip as an operand:
 *   - with %rip-relative addressing encoded in ModR/M, or
 *   - implicitly as a part of the instruction definition (jmp, call).
 * - Call instructions (which may be %rip-relative) need to push the correct
 *   return address onto the stack.
 *
 * Call instructions are simple enough to be emulated in software, so we simply
 * do not use the trampoline mechanism in that case.  kinst_invop() will compute
 * the branch target using the address info computed here (register operands and
 * displacement).
 *
 * %rip-relative operands encoded using the ModR/M byte always use a 32-bit
 * displacement; when populating the trampoline the displacement is adjusted to
 * be relative to the trampoline address.  Trampolines are always allocated
 * above KERNBASE for this reason.
 *
 * For other %rip-relative operands (just jumps) we take the same approach.
 * Instructions which specify an 8-bit displacement must be rewritten to use a
 * 32-bit displacement.
 */
static int
kinst_instr_dissect(struct kinst_probe *kp, uint8_t **instr)
{
	struct kinst_probe_md *kpmd;
	dis86_t d86;
	uint8_t *bytes, modrm, rex;
	int dispoff, i, ilen, opcidx;

	kpmd = &kp->kp_md;

	d86.d86_data = instr;
	d86.d86_get_byte = kinst_dis_get_byte;
	d86.d86_check_func = NULL;
	if (dtrace_disx86(&d86, SIZE64) != 0) {
		KINST_LOG("failed to disassemble instruction at: %p", *instr);
		return (EINVAL);
	}
	bytes = d86.d86_bytes;
	kpmd->instlen = kpmd->tinstlen = d86.d86_len;

	/*
	 * Skip over prefixes, save REX.
	 */
	rex = 0;
	for (i = 0; i < kpmd->instlen; i++) {
		switch (bytes[i]) {
		case 0xf0 ... 0xf3:
			/* group 1 */
			continue;
		case 0x26:
		case 0x2e:
		case 0x36:
		case 0x3e:
		case 0x64:
		case 0x65:
			/* group 2 */
			continue;
		case 0x66:
			/* group 3 */
			continue;
		case 0x67:
			/* group 4 */
			continue;
		case 0x40 ... 0x4f:
			/* REX */
			rex = bytes[i];
			continue;
		}
		break;
	}
	KASSERT(i < kpmd->instlen,
	    ("%s: failed to disassemble instruction at %p", __func__, bytes));
	opcidx = i;

	/*
	 * Identify instructions of interest by opcode: calls and jumps.
	 * Extract displacements.
	 */
	dispoff = -1;
	switch (bytes[opcidx]) {
	case 0x0f:
		switch (bytes[opcidx + 1]) {
		case 0x80 ... 0x8f:
			/* conditional jmp near */
			kpmd->flags |= KINST_F_JMP | KINST_F_RIPREL;
			dispoff = opcidx + 2;
			kinst_set_disp32(kp, &bytes[dispoff]);
			break;
		}
		break;
	case 0xe3:
		/*
		 * There is no straightforward way to translate this instruction
		 * to use a 32-bit displacement.  Fortunately, it is rarely
		 * used.
		 */
		return (EINVAL);
	case 0x70 ... 0x7f:
		/* conditional jmp short */
		kpmd->flags |= KINST_F_JMP | KINST_F_RIPREL;
		dispoff = opcidx + 1;
		kinst_set_disp8(kp, bytes[dispoff]);
		break;
	case 0xe9:
		/* unconditional jmp near */
		kpmd->flags |= KINST_F_JMP | KINST_F_RIPREL;
		dispoff = opcidx + 1;
		kinst_set_disp32(kp, &bytes[dispoff]);
		break;
	case 0xeb:
		/* unconditional jmp short */
		kpmd->flags |= KINST_F_JMP | KINST_F_RIPREL;
		dispoff = opcidx + 1;
		kinst_set_disp8(kp, bytes[dispoff]);
		break;
	case 0xe8:
	case 0x9a:
		/* direct call */
		kpmd->flags |= KINST_F_CALL | KINST_F_DIRECT_CALL;
		dispoff = opcidx + 1;
		kinst_set_disp32(kp, &bytes[dispoff]);
		break;
	case 0xff:
		KASSERT(d86.d86_got_modrm,
		    ("no ModR/M byte for instr at %p", *instr - kpmd->instlen));
		switch (KINST_MODRM_REG(bytes[d86.d86_rmindex])) {
		case 0x02:
		case 0x03:
			/* indirect call */
			kpmd->flags |= KINST_F_CALL;
			break;
		case 0x04:
		case 0x05:
			/* indirect jump */
			kpmd->flags |= KINST_F_JMP;
			break;
		}
	}

	/*
	 * If there's a ModR/M byte, we need to check it to see if the operand
	 * is %rip-relative, and rewrite the displacement if so.  If not, we
	 * might still have to extract operand info if this is a call
	 * instruction.
	 */
	if (d86.d86_got_modrm) {
		uint8_t mod, rm, sib;

		kpmd->reg1 = kpmd->reg2 = -1;

		modrm = bytes[d86.d86_rmindex];
		mod = KINST_MODRM_MOD(modrm);
		rm = KINST_MODRM_RM(modrm);
		if (mod == 0 && rm == 5) {
			kpmd->flags |= KINST_F_RIPREL;
			dispoff = d86.d86_rmindex + 1;
			kinst_set_disp32(kp, &bytes[dispoff]);
		} else if ((kpmd->flags & KINST_F_CALL) != 0) {
			bool havesib;

			havesib = (mod != 3 && rm == 4);
			dispoff = d86.d86_rmindex + (havesib ? 2 : 1);
			if (mod == 1)
				kinst_set_disp8(kp, bytes[dispoff]);
			else if (mod == 2)
				kinst_set_disp32(kp, &bytes[dispoff]);
			else if (mod == 3)
				kpmd->flags |= KINST_F_MOD_DIRECT;

			if (havesib) {
				sib = bytes[d86.d86_rmindex + 1];
				if (KINST_SIB_BASE(sib) != 5) {
					kpmd->reg1 = KINST_SIB_BASE(sib) |
					    (KINST_REX_B(rex) << 3);
				}
				kpmd->scale = KINST_SIB_SCALE(sib);
				kpmd->reg2 = KINST_SIB_INDEX(sib) |
				    (KINST_REX_X(rex) << 3);
			} else {
				kpmd->reg1 = rm | (KINST_REX_B(rex) << 3);
			}
		}
	}

	/*
	 * Calls are emulated in software; once operands are decoded we have
	 * nothing else to do.
	 */
	if ((kpmd->flags & KINST_F_CALL) != 0)
		return (0);

	/*
	 * Allocate and populate an instruction trampoline template.
	 *
	 * Position-independent instructions can simply be copied, but
	 * position-dependent instructions require some surgery: jump
	 * instructions with an 8-bit displacement need to be converted to use a
	 * 32-bit displacement, and the adjusted displacement needs to be
	 * computed.
	 */
	ilen = kpmd->instlen;
	if ((kpmd->flags & KINST_F_RIPREL) != 0) {
		if ((kpmd->flags & KINST_F_JMP) == 0 ||
		    bytes[opcidx] == 0x0f ||
		    bytes[opcidx] == 0xe9 ||
		    bytes[opcidx] == 0xff) {
			memcpy(kpmd->template, bytes, dispoff);
			memcpy(&kpmd->template[dispoff + 4],
			    &bytes[dispoff + 4], ilen - (dispoff + 4));
			kpmd->dispoff = dispoff;
		} else if (bytes[opcidx] == 0xeb) {
			memcpy(kpmd->template, bytes, opcidx);
			kpmd->template[opcidx] = 0xe9;
			kpmd->dispoff = opcidx + 1;

			/* Instruction length changes from 2 to 5. */
			kpmd->tinstlen = 5;
			kpmd->disp -= 3;
		} else if (bytes[opcidx] >= 0x70 && bytes[opcidx] <= 0x7f)  {
			memcpy(kpmd->template, bytes, opcidx);
			kpmd->template[opcidx] = 0x0f;
			kpmd->template[opcidx + 1] = bytes[opcidx] + 0x10;
			kpmd->dispoff = opcidx + 2;

			/* Instruction length changes from 2 to 6. */
			kpmd->tinstlen = 6;
			kpmd->disp -= 4;
		} else {
			panic("unhandled opcode %#x", bytes[opcidx]);
		}
	} else {
		memcpy(kpmd->template, bytes, ilen);
	}

	return (0);
}

int
kinst_make_probe(linker_file_t lf, int symindx, linker_symval_t *symval,
    void *opaque)
{
	struct kinst_probe *kp;
	dtrace_kinst_probedesc_t *pd;
	const char *func;
	int error, n, off;
	uint8_t *instr, *limit;

	pd = opaque;
	func = symval->name;
	if (strcmp(func, pd->kpd_func) != 0 || strcmp(func, "trap_check") == 0)
		return (0);

	instr = (uint8_t *)symval->value;
	limit = (uint8_t *)symval->value + symval->size;
	if (instr >= limit)
		return (0);

	/*
	 * Ignore functions not beginning with the usual function prologue.
	 * These might correspond to assembly routines with which we should not
	 * meddle.
	 */
	if (*instr != KINST_PUSHL_RBP)
		return (0);

	n = 0;
	while (instr < limit) {
		off = (int)(instr - (uint8_t *)symval->value);
		if (pd->kpd_off != -1 && off != pd->kpd_off) {
			instr += dtrace_instr_size(instr);
			continue;
		}

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
		kp->kp_savedval = *instr;
		kp->kp_patchval = KINST_PATCHVAL;
		kp->kp_patchpoint = instr;

		error = kinst_instr_dissect(kp, &instr);
		if (error != 0)
			return (error);

		kinst_probe_create(kp, lf);
	}

	return (0);
}
