/*-
 * Copyright (c) 2010, George V. Neville-Neil <gnn@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/md_var.h>
#include <machine/mips_opcode.h>
#include <machine/vmparam.h>

#if defined(__mips_n64)
#	define	MIPS_IS_VALID_KERNELADDR(reg)	((((reg) & 3) == 0) && \
					((vm_offset_t)(reg) >= MIPS_XKPHYS_START))
#else
#	define	MIPS_IS_VALID_KERNELADDR(reg)	((((reg) & 3) == 0) && \
					((vm_offset_t)(reg) >= MIPS_KSEG0_START))
#endif

/*
 * We need some reasonable default to prevent backtrace code
 * from wandering too far
 */
#define	MAX_FUNCTION_SIZE 0x10000
#define	MAX_PROLOGUE_SIZE 0x100

static int
pmc_next_frame(register_t *pc, register_t *sp)
{
	InstFmt i;
	uintptr_t va;
	uint32_t instr, mask;
	int more, stksize;
	register_t ra = 0;

	/* Jump here after a nonstandard (interrupt handler) frame */
	stksize = 0;

	/* check for bad SP: could foul up next frame */
	if (!MIPS_IS_VALID_KERNELADDR(*sp)) {
		goto error;
	}

	/* check for bad PC */
	if (!MIPS_IS_VALID_KERNELADDR(*pc)) {
		goto error;
	}

	/*
	 * Find the beginning of the current subroutine by scanning
	 * backwards from the current PC for the end of the previous
	 * subroutine.
	 */
	va = *pc - sizeof(int);
	while (1) {
		instr = *((uint32_t *)va);

		/* [d]addiu sp,sp,-X */
		if (((instr & 0xffff8000) == 0x27bd8000)
		    || ((instr & 0xffff8000) == 0x67bd8000))
			break;

		/* jr	ra */
		if (instr == 0x03e00008) {
			/* skip over branch-delay slot instruction */
			va += 2 * sizeof(int);
			break;
		}

		va -= sizeof(int);
	}

	/* skip over nulls which might separate .o files */
	while ((instr = *((uint32_t *)va)) == 0)
		va += sizeof(int);

	/* scan forwards to find stack size and any saved registers */
	stksize = 0;
	more = 3;
	mask = 0;
	for (; more; va += sizeof(int),
	    more = (more == 3) ? 3 : more - 1) {
		/* stop if hit our current position */
		if (va >= *pc)
			break;
		instr = *((uint32_t *)va);
		i.word = instr;
		switch (i.JType.op) {
		case OP_SPECIAL:
			switch (i.RType.func) {
			case OP_JR:
			case OP_JALR:
				more = 2;	/* stop after next instruction */
				break;

			case OP_SYSCALL:
			case OP_BREAK:
				more = 1;	/* stop now */
			};
			break;

		case OP_BCOND:
		case OP_J:
		case OP_JAL:
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
			more = 2;	/* stop after next instruction */
			break;

		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			switch (i.RType.rs) {
			case OP_BCx:
			case OP_BCy:
				more = 2;	/* stop after next instruction */
			};
			break;

		case OP_SW:
		case OP_SD:
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			if (i.IType.rt == 31)
				ra = *((register_t *)(*sp + (short)i.IType.imm));
			break;

		case OP_ADDI:
		case OP_ADDIU:
		case OP_DADDI:
		case OP_DADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != 29 || i.IType.rt != 29)
				break;
			stksize = -((short)i.IType.imm);
		}
	}

	if (!MIPS_IS_VALID_KERNELADDR(ra))
		return (-1);

	*pc = ra;
	*sp += stksize;

	return (0);

error:
	return (-1);
}

static int
pmc_next_uframe(register_t *pc, register_t *sp, register_t *ra)
{
	int offset, registers_on_stack;
	uint32_t opcode, mask;
	register_t function_start;
	int stksize;
	InstFmt i;

	registers_on_stack = 0;
	mask = 0;
	function_start = 0;
	offset = 0;
	stksize = 0;

	while (offset < MAX_FUNCTION_SIZE) {
		opcode = fuword32((void *)(*pc - offset));

		/* [d]addiu sp, sp, -X*/
		if (((opcode & 0xffff8000) == 0x27bd8000)
		    || ((opcode & 0xffff8000) == 0x67bd8000)) {
			function_start = *pc - offset;
			registers_on_stack = 1;
			break;
		}

		/* lui gp, X */
		if ((opcode & 0xffff8000) == 0x3c1c0000) {
			/*
			 * Function might start with this instruction
			 * Keep an eye on "jr ra" and sp correction
			 * with positive value further on
			 */
			function_start = *pc - offset;
		}

		if (function_start) {
			/*
			 * Stop looking further. Possible end of
			 * function instruction: it means there is no
			 * stack modifications, sp is unchanged
			 */

			/* [d]addiu sp,sp,X */
			if (((opcode & 0xffff8000) == 0x27bd0000)
			    || ((opcode & 0xffff8000) == 0x67bd0000))
				break;

			if (opcode == 0x03e00008)
				break;
		}

		offset += sizeof(int);
	}

	if (!function_start)
		return (-1);

	if (registers_on_stack) {
		offset = 0;
		while ((offset < MAX_PROLOGUE_SIZE)
		    && ((function_start + offset) < *pc)) {
			i.word = fuword32((void *)(function_start + offset));
			switch (i.JType.op) {
			case OP_SW:
				/* look for saved registers on the stack */
				if (i.IType.rs != 29)
					break;
				/* only restore the first one */
				if (mask & (1 << i.IType.rt))
					break;
				mask |= (1 << i.IType.rt);
				if (i.IType.rt == 31)
					*ra = fuword32((void *)(*sp + (short)i.IType.imm));
				break;

#if defined(__mips_n64)
			case OP_SD:
				/* look for saved registers on the stack */
				if (i.IType.rs != 29)
					break;
				/* only restore the first one */
				if (mask & (1 << i.IType.rt))
					break;
				mask |= (1 << i.IType.rt);
				/* ra */
				if (i.IType.rt == 31)
					*ra = fuword64((void *)(*sp + (short)i.IType.imm));
			break;
#endif

			case OP_ADDI:
			case OP_ADDIU:
			case OP_DADDI:
			case OP_DADDIU:
				/* look for stack pointer adjustment */
				if (i.IType.rs != 29 || i.IType.rt != 29)
					break;
				stksize = -((short)i.IType.imm);
			}

			offset += sizeof(int);
		}
	}

	/*
	 * We reached the end of backtrace
	 */
	if (*pc == *ra)
		return (-1);

	*pc = *ra;
	*sp += stksize;

	return (0);
}

struct pmc_mdep *
pmc_md_initialize()
{
  /*	if (cpu_class == CPU_CLASS_MIPS24K)*/
		return pmc_mips24k_initialize();
		/*	else
			return NULL;*/
}

void
pmc_md_finalize(struct pmc_mdep *md)
{
  /*	if (cpu_class == CPU_CLASS_MIPS24K) */
		pmc_mips24k_finalize(md);
		/*	else
		KASSERT(0, ("[mips,%d] Unknown CPU Class 0x%x", __LINE__,
		cpu_class));*/
}

int
pmc_save_kernel_callchain(uintptr_t *cc, int nframes,
    struct trapframe *tf)
{
	register_t pc, ra, sp;
	int frames = 0;

	pc = (uint64_t)tf->pc;
	sp = (uint64_t)tf->sp;
	ra = (uint64_t)tf->ra;

	/*
	 * Unwind, and unwind, and unwind
	 */
	while (1) {
		cc[frames++] = pc;
		if (frames >= nframes)
			break;

		if (pmc_next_frame(&pc, &sp) < 0)
			break;
	}

	return (frames);
}

int
pmc_save_user_callchain(uintptr_t *cc, int nframes,
    struct trapframe *tf)
{
	register_t pc, ra, sp;
	int frames = 0;

	pc = (uint64_t)tf->pc;
	sp = (uint64_t)tf->sp;
	ra = (uint64_t)tf->ra;

	/*
	 * Unwind, and unwind, and unwind
	 */
	while (1) {
		cc[frames++] = pc;
		if (frames >= nframes)
			break;

		if (pmc_next_uframe(&pc, &sp, &ra) < 0)
			break;
	}

	return (frames);
}
