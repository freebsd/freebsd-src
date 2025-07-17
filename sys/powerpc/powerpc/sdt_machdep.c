/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/systm.h>
#include <sys/sdt.h>

#include <machine/md_var.h>

/*
 * Return true if we can overwrite a nop at "patchpoint" with a jump to the
 * target address.
 */
bool
sdt_tracepoint_valid(uintptr_t patchpoint, uintptr_t target)
{
	int64_t offset;

	if (patchpoint == target ||
	    (patchpoint & 3) != 0 || (target & 3) != 0)
		return (false);
	offset = target - patchpoint;
	if (offset < -(1 << 26) || offset > (1 << 26))
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
	    ("%s: invalid tracepoint %#lx -> %#lx",
	    __func__, patchpoint, target));

	instr = ((target - patchpoint) & 0x7fffffful) | 0x48000000;
	memcpy((void *)patchpoint, &instr, sizeof(instr));
	__syncicache((void *)patchpoint, sizeof(instr));
}

/*
 * Overwrite the patchpoint with a nop instruction.
 */
void
sdt_tracepoint_restore(uintptr_t patchpoint)
{
	uint32_t instr;

	instr = 0x60000000;
	memcpy((void *)patchpoint, &instr, sizeof(instr));
	__syncicache((void *)patchpoint, sizeof(instr));
}
