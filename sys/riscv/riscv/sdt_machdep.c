/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/systm.h>
#include <sys/sdt.h>

#include <machine/encoding.h>

/*
 * Return true if we can overwrite a nop at "patchpoint" with a jump to the
 * target address.
 */
bool
sdt_tracepoint_valid(uintptr_t patchpoint, uintptr_t target)
{
	int64_t offset;

	if (patchpoint == target ||
	    (patchpoint & (INSN_C_SIZE - 1)) != 0 ||
	    (target & (INSN_C_SIZE - 1)) != 0)
		return (false);
	offset = target - patchpoint;
	if (offset < -(1 << 19) || offset > (1 << 19))
		return (false);
	return (true);
}

/*
 * Overwrite the copy of _SDT_ASM_PATCH_INSTR at the tracepoint with a jump to
 * the target address.
 */
void
sdt_tracepoint_patch(uintptr_t patchpoint, uintptr_t target)
{
	int32_t imm;
	uint32_t instr;

	KASSERT(sdt_tracepoint_valid(patchpoint, target),
	    ("%s: invalid tracepoint %#lx -> %#lx",
	    __func__, patchpoint, target));

	imm = target - patchpoint;
	imm = (imm & 0x100000) |
	    ((imm & 0x7fe) << 8) |
	    ((imm & 0x800) >> 2) |
	    ((imm & 0xff000) >> 12);
	instr = (imm << 12) | MATCH_JAL;

	memcpy((void *)patchpoint, &instr, sizeof(instr));
	fence_i();
}

/*
 * Overwrite the patchpoint with a nop instruction.
 */
void
sdt_tracepoint_restore(uintptr_t patchpoint)
{
	uint32_t instr;

	instr = 0x13; /* uncompressed nop */

	memcpy((void *)patchpoint, &instr, sizeof(instr));
	fence_i();
}
