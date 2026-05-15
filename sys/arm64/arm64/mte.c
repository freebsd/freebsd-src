/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024-2026 Arm Ltd
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
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/proc.h>

#include <machine/cpu_feat.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/sysarch.h>
#include <vm/vm.h>
#include <vm/vm_page.h>

/* Version of MTE implemented. 0 == unimplemented */
static u_int __read_mostly mte_version = 0;

/*
 * FEAT_MTE (mte_version == 1) has userspace instructions, but no tag
 * checking. May of the registers/fields need FEAT_MTE2 to be implemented
 * before we can access them.
 */
#define	MTE_HAS_TAG_CHECK	(mte_version >= 2)

struct thread *mte_switch(struct thread *);

static void
mte_update_sctlr(struct thread *td, uint64_t sctlr)
{
	MPASS((sctlr & ~(SCTLR_ATA0 | SCTLR_TCF0_MASK)) == 0);
	td->td_md.md_sctlr &= ~(SCTLR_ATA0 | SCTLR_TCF0_MASK);
	td->td_md.md_sctlr |= sctlr;
}

void
mte_fork(struct thread *new_td, struct thread *orig_td)
{
	if (!MTE_HAS_TAG_CHECK)
		return;

	mte_update_sctlr(new_td,
	    orig_td->td_md.md_sctlr & SCTLR_TCF0_MASK);
	new_td->td_md.md_gcr = orig_td->td_md.md_gcr;
}

void
mte_exec(struct thread *td)
{
	if (!MTE_HAS_TAG_CHECK)
		return;

	mte_update_sctlr(td, SCTLR_TCF0_NONE);
	td->td_md.md_gcr = GCR_RRND;
}

void
mte_copy_thread(struct thread *new_td, struct thread *orig_td)
{
	if (!MTE_HAS_TAG_CHECK)
		return;

	mte_update_sctlr(new_td,
	    orig_td->td_md.md_sctlr & SCTLR_TCF0_MASK);
	new_td->td_md.md_gcr = orig_td->td_md.md_gcr;
}

/* Only for kernel threads */
void
mte_thread_alloc(struct thread *td)
{
}

/* Only for a kernel thread */
void
mte_thread0(struct thread *td)
{
}


struct thread *
mte_switch(struct thread *td)
{
	if (MTE_HAS_TAG_CHECK) {
		WRITE_SPECIALREG(GCR_EL1_REG, td->td_md.md_gcr);
	}
	return (td);
}
