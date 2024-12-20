/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/systm.h>
#include <sys/sdt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

/*
 * Return true if we can overwrite a nop at "patchpoint" with a jump to the
 * target address.
 */
bool
sdt_tracepoint_valid(uintptr_t patchpoint, uintptr_t target)
{
	void *addr;
	int64_t offset;

	if (!arm64_get_writable_addr((void *)patchpoint, &addr))
		return (false);

	if (patchpoint == target ||
	    (patchpoint & (INSN_SIZE - 1)) != 0 ||
	    (target & (INSN_SIZE - 1)) != 0)
		return (false);
	offset = target - patchpoint;
	if (offset < -(1 << 26) || offset > (1 << 26))
		return (false);
	return (true);
}

/*
 * Overwrite the copy of _SDT_ASM_PATCH_INSTR at the tracepoint with a jump to the
 * target address.
 */
void
sdt_tracepoint_patch(uintptr_t patchpoint, uintptr_t target)
{
	void *addr;
	uint32_t instr;

	KASSERT(sdt_tracepoint_valid(patchpoint, target),
	    ("%s: invalid tracepoint %#lx -> %#lx",
	    __func__, patchpoint, target));

	if (!arm64_get_writable_addr((void *)patchpoint, &addr))
		panic("%s: Unable to write new instruction", __func__);

	instr = (((target - patchpoint) >> 2) & 0x3fffffful) | 0x14000000;
	memcpy(addr, &instr, sizeof(instr));
	cpu_icache_sync_range((void *)patchpoint, INSN_SIZE);
}

/*
 * Overwrite the patchpoint with a nop instruction.
 */
void
sdt_tracepoint_restore(uintptr_t patchpoint)
{
	void *addr;
	uint32_t instr;

	if (!arm64_get_writable_addr((void *)patchpoint, &addr))
		panic("%s: Unable to write new instruction", __func__);

	instr = 0xd503201f;
	memcpy(addr, &instr, sizeof(instr));
	cpu_icache_sync_range((void *)patchpoint, INSN_SIZE);
}
