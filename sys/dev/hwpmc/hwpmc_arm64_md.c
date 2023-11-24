/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory with support from ARM Ltd.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pmc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pmc_mdep.h>
#include <machine/stack.h>
#include <machine/vmparam.h>

struct pmc_mdep *
pmc_md_initialize(void)
{

	return (pmc_arm64_initialize());
}

void
pmc_md_finalize(struct pmc_mdep *md)
{

	pmc_arm64_finalize(md);
}

int
pmc_save_kernel_callchain(uintptr_t *cc, int maxsamples, struct trapframe *tf)
{
	struct unwind_state frame;
	int count;

	KASSERT(TRAPF_USERMODE(tf) == 0,("[arm64,%d] not a kernel backtrace",
	    __LINE__));

	frame.pc = PMC_TRAPFRAME_TO_PC(tf);
	*cc++ = frame.pc;

	if (maxsamples <= 1)
		return (1);

	frame.fp = PMC_TRAPFRAME_TO_FP(tf);
	if (!PMC_IN_KERNEL(frame.pc) || !PMC_IN_KERNEL_STACK(frame.fp))
		return (1);

	for (count = 1; count < maxsamples; count++) {
		if (!unwind_frame(curthread, &frame))
			break;
		if (!PMC_IN_KERNEL(frame.pc))
			break;
		*cc++ = frame.pc;
	}

	return (count);
}

int
pmc_save_user_callchain(uintptr_t *cc, int maxsamples,
    struct trapframe *tf)
{
	uintptr_t pc, r, oldfp, fp;
	int count;

	KASSERT(TRAPF_USERMODE(tf), ("[arm64,%d] Not a user trap frame tf=%p",
	    __LINE__, (void *) tf));

	pc = PMC_TRAPFRAME_TO_PC(tf);
	*cc++ = pc;

	if (maxsamples <= 1)
		return (1);

	oldfp = fp = PMC_TRAPFRAME_TO_FP(tf);

	if (!PMC_IN_USERSPACE(pc) ||
	    !PMC_IN_USERSPACE(fp))
		return (1);

	for (count = 1; count < maxsamples; count++) {
		/* Use saved lr as pc. */
		r = fp + sizeof(uintptr_t);
		if (copyin((void *)r, &pc, sizeof(pc)) != 0)
			break;
		if (!PMC_IN_USERSPACE(pc))
			break;

		*cc++ = pc;

		/* Switch to next frame up */
		oldfp = fp;
		r = fp;
		if (copyin((void *)r, &fp, sizeof(fp)) != 0)
			break;
		if (fp < oldfp || !PMC_IN_USERSPACE(fp))
			break;
	}

	return (count);
}
