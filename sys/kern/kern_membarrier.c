/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/membarrier.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>

#include <vm/vm_param.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#define MEMBARRIER_SUPPORTED_CMDS	(			\
    MEMBARRIER_CMD_GLOBAL |					\
    MEMBARRIER_CMD_GLOBAL_EXPEDITED |				\
    MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED |			\
    MEMBARRIER_CMD_PRIVATE_EXPEDITED |				\
    MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED |			\
    MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE |		\
    MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE)

static void
membarrier_action_seqcst(void *arg __unused)
{
	atomic_thread_fence_seq_cst();
}

static void
membarrier_action_seqcst_sync_core(void *arg __unused)
{
	atomic_thread_fence_seq_cst();
	cpu_sync_core();
}

static void
do_membarrier_ipi(cpuset_t *csp, void (*func)(void *))
{
	atomic_thread_fence_seq_cst();
	smp_rendezvous_cpus(*csp, smp_no_rendezvous_barrier, func,
	    smp_no_rendezvous_barrier, NULL);
	atomic_thread_fence_seq_cst();
}

static void
check_cpu_switched(int c, cpuset_t *csp, uint64_t *swt, bool init)
{
	struct pcpu *pc;
	uint64_t sw;

	if (CPU_ISSET(c, csp))
		return;

	pc = cpuid_to_pcpu[c];
	if (pc->pc_curthread == pc->pc_idlethread) {
		CPU_SET(c, csp);
		return;
	}

	/*
	 * Sync with context switch to ensure that override of
	 * pc_curthread with non-idle thread pointer is visible before
	 * reading of pc_switchtime.
	 */
	atomic_thread_fence_acq();

	sw = pc->pc_switchtime;
	if (init)
		swt[c] = sw;
	else if (sw != swt[c])
		CPU_SET(c, csp);
}

/*
 *
 * XXXKIB: We execute the requested action (seq_cst and possibly
 * sync_core) on current CPU as well.  There is no guarantee that
 * current thread executes anything with the full fence semantics
 * during syscall execution.  Similarly, cpu_core_sync() semantics
 * might be not provided by the syscall return.  E.g. on amd64 we
 * typically return without IRET.
 */
int
kern_membarrier(struct thread *td, int cmd, unsigned flags, int cpu_id)
{
	struct proc *p, *p1;
	struct thread *td1;
	cpuset_t cs;
	uint64_t *swt;
	int c, error;
	bool first;

	if (flags != 0 || (cmd & ~MEMBARRIER_SUPPORTED_CMDS) != 0)
		return (EINVAL);

	if (cmd == MEMBARRIER_CMD_QUERY) {
		td->td_retval[0] = MEMBARRIER_SUPPORTED_CMDS;
		return (0);
	}

	p = td->td_proc;
	error = 0;

	switch (cmd) {
	case MEMBARRIER_CMD_GLOBAL:
		swt = malloc((mp_maxid + 1) * sizeof(*swt), M_TEMP, M_WAITOK);
		CPU_ZERO(&cs);
		sched_pin();
		CPU_SET(PCPU_GET(cpuid), &cs);
		for (first = true; error == 0; first = false) {
			CPU_FOREACH(c)
				check_cpu_switched(c, &cs, swt, first);
			if (CPU_CMP(&cs, &all_cpus) == 0)
				break;
			error = pause_sig("mmbr", 1);
			if (error == EWOULDBLOCK)
				error = 0;
		}
		sched_unpin();
		free(swt, M_TEMP);
		atomic_thread_fence_seq_cst();
		break;

	case MEMBARRIER_CMD_GLOBAL_EXPEDITED:
		if ((td->td_proc->p_flag2 & P2_MEMBAR_GLOBE) == 0) {
			error = EPERM;
		} else {
			CPU_ZERO(&cs);
			CPU_FOREACH(c) {
				td1 = cpuid_to_pcpu[c]->pc_curthread;
				p1 = td1->td_proc;
				if (p1 != NULL &&
				    (p1->p_flag2 & P2_MEMBAR_GLOBE) != 0)
					CPU_SET(c, &cs);
			}
			do_membarrier_ipi(&cs, membarrier_action_seqcst);
		}
		break;

	case MEMBARRIER_CMD_REGISTER_GLOBAL_EXPEDITED:
		if ((p->p_flag2 & P2_MEMBAR_GLOBE) == 0) {
			PROC_LOCK(p);
			p->p_flag2 |= P2_MEMBAR_GLOBE;
			PROC_UNLOCK(p);
		}
		break;

	case MEMBARRIER_CMD_PRIVATE_EXPEDITED:
		if ((td->td_proc->p_flag2 & P2_MEMBAR_PRIVE) == 0) {
			error = EPERM;
		} else {
			pmap_active_cpus(vmspace_pmap(p->p_vmspace), &cs);
			do_membarrier_ipi(&cs, membarrier_action_seqcst);
		}
		break;

	case MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED:
		if ((p->p_flag2 & P2_MEMBAR_PRIVE) == 0) {
			PROC_LOCK(p);
			p->p_flag2 |= P2_MEMBAR_PRIVE;
			PROC_UNLOCK(p);
		}
		break;

	case MEMBARRIER_CMD_PRIVATE_EXPEDITED_SYNC_CORE:
		if ((td->td_proc->p_flag2 & P2_MEMBAR_PRIVE_SYNCORE) == 0) {
			error = EPERM;
		} else {
			/*
			 * Calculating the IPI multicast mask from
			 * pmap active mask means that we do not call
			 * cpu_sync_core() on CPUs that were missed
			 * from pmap active mask but could be switched
			 * from or to meantime.  This is fine at least
			 * on amd64 because threads always use slow
			 * (IRETQ) path to return from syscall after
			 * context switch.
			 */
			pmap_active_cpus(vmspace_pmap(p->p_vmspace), &cs);

			do_membarrier_ipi(&cs,
			    membarrier_action_seqcst_sync_core);
		}
		break;

	case MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED_SYNC_CORE:
		if ((p->p_flag2 & P2_MEMBAR_PRIVE_SYNCORE) == 0) {
			PROC_LOCK(p);
			p->p_flag2 |= P2_MEMBAR_PRIVE_SYNCORE;
			PROC_UNLOCK(p);
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

int
sys_membarrier(struct thread *td, struct membarrier_args *uap)
{
	return (kern_membarrier(td, uap->cmd, uap->flags, uap->cpu_id));
}
