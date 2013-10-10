/*-
 * Copyright (c) 2009 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/smp.h>

static struct mtx ia64_highfp_mtx;

static void
ia64_highfp_init(void *_)
{
	mtx_init(&ia64_highfp_mtx, "High FP lock", NULL, MTX_SPIN);
}
SYSINIT(ia64_highfp_init, SI_SUB_LOCK, SI_ORDER_ANY, ia64_highfp_init, NULL);

#ifdef SMP
static int
ia64_highfp_ipi(struct pcpu *cpu)
{
	int error;

	ipi_send(cpu, ia64_ipi_highfp);
	error = msleep_spin(&cpu->pc_fpcurthread, &ia64_highfp_mtx,
	    "High FP", 0);
	return (error);
}
#endif

int
ia64_highfp_drop(struct thread *td)
{
	struct pcb *pcb;
	struct pcpu *cpu;

	pcb = td->td_pcb;

	mtx_lock_spin(&ia64_highfp_mtx);
	cpu = pcb->pcb_fpcpu;
	if (cpu != NULL) {
		KASSERT(cpu->pc_fpcurthread == td,
		    ("cpu->pc_fpcurthread != td"));
		td->td_frame->tf_special.psr |= IA64_PSR_DFH;
		pcb->pcb_fpcpu = NULL;
		cpu->pc_fpcurthread = NULL;
	}
	mtx_unlock_spin(&ia64_highfp_mtx);

	return ((cpu != NULL) ? 1 : 0);
}

int
ia64_highfp_enable(struct thread *td, struct trapframe *tf)
{
	struct pcb *pcb;
	struct pcpu *cpu;
	struct thread *td1;

	pcb = td->td_pcb;

	mtx_lock_spin(&ia64_highfp_mtx);
	cpu = pcb->pcb_fpcpu;
#ifdef SMP
	if (cpu != NULL && cpu != pcpup) {
		KASSERT(cpu->pc_fpcurthread == td,
		    ("cpu->pc_fpcurthread != td"));
		ia64_highfp_ipi(cpu);
	}
#endif
	td1 = PCPU_GET(fpcurthread);
	if (td1 != NULL && td1 != td) {
		KASSERT(td1->td_pcb->pcb_fpcpu == pcpup,
		    ("td1->td_pcb->pcb_fpcpu != pcpup"));
		save_high_fp(&td1->td_pcb->pcb_high_fp);
		td1->td_frame->tf_special.psr |= IA64_PSR_DFH;
		td1->td_pcb->pcb_fpcpu = NULL;
		PCPU_SET(fpcurthread, NULL);
		td1 = NULL;
	}
	if (td1 == NULL) {
		KASSERT(pcb->pcb_fpcpu == NULL, ("pcb->pcb_fpcpu != NULL"));
		KASSERT(PCPU_GET(fpcurthread) == NULL,
		    ("PCPU_GET(fpcurthread) != NULL"));
		restore_high_fp(&pcb->pcb_high_fp);
		PCPU_SET(fpcurthread, td);
		pcb->pcb_fpcpu = pcpup;
		tf->tf_special.psr &= ~IA64_PSR_MFH;
	}
	tf->tf_special.psr &= ~IA64_PSR_DFH;
	mtx_unlock_spin(&ia64_highfp_mtx);

	return ((td1 != NULL) ? 1 : 0);
}

int
ia64_highfp_save(struct thread *td)
{
	struct pcb *pcb;
	struct pcpu *cpu;

	pcb = td->td_pcb;

	mtx_lock_spin(&ia64_highfp_mtx);
	cpu = pcb->pcb_fpcpu;
#ifdef SMP
	if (cpu != NULL && cpu != pcpup) {
		KASSERT(cpu->pc_fpcurthread == td,
		    ("cpu->pc_fpcurthread != td"));
		ia64_highfp_ipi(cpu);
	} else
#endif
	if (cpu != NULL) {
		KASSERT(cpu->pc_fpcurthread == td,
		    ("cpu->pc_fpcurthread != td"));
		save_high_fp(&pcb->pcb_high_fp);
		td->td_frame->tf_special.psr |= IA64_PSR_DFH;
		pcb->pcb_fpcpu = NULL;
		cpu->pc_fpcurthread = NULL;
	}
	mtx_unlock_spin(&ia64_highfp_mtx);

	return ((cpu != NULL) ? 1 : 0);
}

#ifdef SMP
int
ia64_highfp_save_ipi(void)
{
	struct thread *td;

	mtx_lock_spin(&ia64_highfp_mtx);
	td = PCPU_GET(fpcurthread);
	if (td != NULL) {
		KASSERT(td->td_pcb->pcb_fpcpu == pcpup,
		    ("td->td_pcb->pcb_fpcpu != pcpup"));
		save_high_fp(&td->td_pcb->pcb_high_fp);
		td->td_frame->tf_special.psr |= IA64_PSR_DFH;
		td->td_pcb->pcb_fpcpu = NULL;
		PCPU_SET(fpcurthread, NULL);
	}
	mtx_unlock_spin(&ia64_highfp_mtx);
	wakeup(&PCPU_GET(fpcurthread));

	return ((td != NULL) ? 1 : 0);
}
#endif
