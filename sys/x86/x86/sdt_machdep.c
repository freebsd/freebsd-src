/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
 */

#include <sys/systm.h>
#include <sys/sdt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/vmparam.h>

#define	SDT_PATCH_SIZE		5

/*
 * Return true if we can overwrite a nop at "patchpoint" with a jump to the
 * target address.
 */
bool
sdt_tracepoint_valid(uintptr_t patchpoint, uintptr_t target)
{
#ifdef __amd64__
	if (patchpoint < KERNSTART || target < KERNSTART)
		return (false);
#endif
	if (patchpoint == target ||
	    patchpoint + SDT_PATCH_SIZE < patchpoint)
		return (false);
#ifdef __amd64__
	int64_t offset = target - (patchpoint + SDT_PATCH_SIZE);
	if (offset < -(1l << 31) || offset > (1l << 31))
		return (false);
#endif
	return (true);
}

/*
 * Overwrite the copy of _SDT_ASM_PATCH_INSTR at the tracepoint with a jump to
 * the target address.
 */
void
sdt_tracepoint_patch(uintptr_t patchpoint, uintptr_t target)
{
	uint8_t instr[SDT_PATCH_SIZE];
	int32_t disp;
	bool old_wp;

	KASSERT(sdt_tracepoint_valid(patchpoint, target),
	    ("%s: invalid tracepoint %#jx -> %#jx",
	    __func__, (uintmax_t)patchpoint, (uintmax_t)target));

	instr[0] = 0xe9;
	disp = target - (patchpoint + SDT_PATCH_SIZE);
	memcpy(&instr[1], &disp, sizeof(disp));

	old_wp = disable_wp();
	memcpy((void *)patchpoint, instr, sizeof(instr));
	restore_wp(old_wp);
}

/*
 * Overwrite the patchpoint with a nop instruction.
 */
void
sdt_tracepoint_restore(uintptr_t patchpoint)
{
	uint8_t instr[SDT_PATCH_SIZE];
	bool old_wp;

#ifdef __amd64__
	KASSERT(patchpoint >= KERNSTART,
	    ("%s: invalid patchpoint %#lx", __func__, patchpoint));
#endif

	for (int i = 0; i < SDT_PATCH_SIZE; i++)
		instr[i] = 0x90;

	old_wp = disable_wp();
	memcpy((void *)patchpoint, instr, sizeof(instr));
	restore_wp(old_wp);
}
