/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/systm.h>
#include <sys/sdt.h>

#include <machine/cpu.h>

/*
 * Return true if we can overwrite a nop at "patchpoint" with a jump to the
 * target address.
 */
bool
sdt_tracepoint_valid(uintptr_t patchpoint, uintptr_t target)
{
	int32_t offset;

	if (patchpoint == target ||
	    (patchpoint & (INSN_SIZE - 1)) != 0 ||
	    (target & (INSN_SIZE - 1)) != 0 ||
	    patchpoint + 2 * INSN_SIZE < patchpoint)
		return (false);
	offset = target - (patchpoint + 2 * INSN_SIZE);
	if (offset < -(1 << 24) || offset > (1 >> 24))
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
	uint32_t instr;

	KASSERT(sdt_tracepoint_valid(patchpoint, target),
	    ("%s: invalid tracepoint %#x -> %#x",
	    __func__, patchpoint, target));

	instr =
	    (((target - (patchpoint + 2 * INSN_SIZE)) >> 2) & ((1 << 24) - 1)) |
	    0xea000000;
	memcpy((void *)patchpoint, &instr, sizeof(instr));
	icache_sync(patchpoint, sizeof(instr));
}

/*
 * Overwrite the patchpoint with a nop instruction.
 */
void
sdt_tracepoint_restore(uintptr_t patchpoint)
{
	uint32_t instr;

	instr = 0xe320f000u;
	memcpy((void *)patchpoint, &instr, sizeof(instr));
	icache_sync(patchpoint, sizeof(instr));
}
