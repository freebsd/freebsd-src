/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_meter.c	8.4 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/vmmeter.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <sys/sysctl.h>

struct vmmeter cnt;

SYSCTL_UINT(_vm, VM_V_FREE_MIN, v_free_min,
	CTLFLAG_RW, &cnt.v_free_min, 0, "Minimum low-free-pages threshold");
SYSCTL_UINT(_vm, VM_V_FREE_TARGET, v_free_target,
	CTLFLAG_RW, &cnt.v_free_target, 0, "Desired free pages");
SYSCTL_UINT(_vm, VM_V_FREE_RESERVED, v_free_reserved,
	CTLFLAG_RW, &cnt.v_free_reserved, 0, "Pages reserved for deadlock");
SYSCTL_UINT(_vm, VM_V_INACTIVE_TARGET, v_inactive_target,
	CTLFLAG_RW, &cnt.v_inactive_target, 0, "Pages desired inactive");
SYSCTL_UINT(_vm, VM_V_CACHE_MIN, v_cache_min,
	CTLFLAG_RW, &cnt.v_cache_min, 0, "Min pages on cache queue");
SYSCTL_UINT(_vm, VM_V_CACHE_MAX, v_cache_max,
	CTLFLAG_RW, &cnt.v_cache_max, 0, "Max pages on cache queue");
SYSCTL_UINT(_vm, VM_V_PAGEOUT_FREE_MIN, v_pageout_free_min,
	CTLFLAG_RW, &cnt.v_pageout_free_min, 0, "Min pages reserved for kernel");
SYSCTL_UINT(_vm, OID_AUTO, v_free_severe,
	CTLFLAG_RW, &cnt.v_free_severe, 0, "Severe page depletion point");

static int
sysctl_vm_loadavg(SYSCTL_HANDLER_ARGS)
{
	
#ifdef SCTL_MASK32
	u_int32_t la[4];

	if (req->flags & SCTL_MASK32) {
		la[0] = averunnable.ldavg[0];
		la[1] = averunnable.ldavg[1];
		la[2] = averunnable.ldavg[2];
		la[3] = averunnable.fscale;
		return SYSCTL_OUT(req, la, sizeof(la));
	} else
#endif
		return SYSCTL_OUT(req, &averunnable, sizeof(averunnable));
}
SYSCTL_PROC(_vm, VM_LOADAVG, loadavg, CTLTYPE_STRUCT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_vm_loadavg, "S,loadavg",
    "Machine loadaverage history");

/*
 * This function aims to determine if the object is mapped,
 * specifically, if it is referenced by a vm_map_entry.  Because
 * objects occasionally acquire transient references that do not
 * represent a mapping, the method used here is inexact.  However, it
 * has very low overhead and is good enough for the advisory
 * vm.vmtotal sysctl.
 */
static bool
is_object_active(vm_object_t obj)
{

	return (obj->ref_count > obj->shadow_count);
}

static int
vmtotal(SYSCTL_HANDLER_ARGS)
{
	struct vmtotal total;
	vm_object_t object;
	struct proc *p;
	struct thread *td;

	bzero(&total, sizeof(total));

	/*
	 * Calculate process statistics.
	 */
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_flag & P_SYSTEM)
			continue;
		PROC_LOCK(p);
		switch (p->p_state) {
		case PRS_NEW:
			PROC_UNLOCK(p);
			continue;
			break;
		default:
			FOREACH_THREAD_IN_PROC(p, td) {
				thread_lock(td);
				switch (td->td_state) {
				case TDS_INHIBITED:
					if (TD_IS_SWAPPED(td))
						total.t_sw++;
					else if (TD_IS_SLEEPING(td)) {
						if (td->td_priority <= PZERO)
							total.t_dw++;
						else
							total.t_sl++;
						if (td->td_wchan ==
						    &cnt.v_free_count)
							total.t_pw++;
					}
					break;

				case TDS_CAN_RUN:
					total.t_sw++;
					break;
				case TDS_RUNQ:
				case TDS_RUNNING:
					total.t_rq++;
					thread_unlock(td);
					continue;
				default:
					break;
				}
				thread_unlock(td);
			}
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);
	/*
	 * Calculate object memory usage statistics.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		/*
		 * Perform unsynchronized reads on the object.  In
		 * this case, the lack of synchronization should not
		 * impair the accuracy of the reported statistics.
		 */
		if ((object->flags & OBJ_FICTITIOUS) != 0) {
			/*
			 * Devices, like /dev/mem, will badly skew our totals.
			 */
			continue;
		}
		if (object->ref_count == 0) {
			/*
			 * Also skip unreferenced objects, including
			 * vnodes representing mounted file systems.
			 */
			continue;
		}
		if (object->ref_count == 1 &&
		    (object->flags & OBJ_NOSPLIT) != 0) {
			/*
			 * Also skip otherwise unreferenced swap
			 * objects backing tmpfs vnodes, and POSIX or
			 * SysV shared memory.
			 */
			continue;
		}
		total.t_vm += object->size;
		total.t_rm += object->resident_page_count;
		if (is_object_active(object)) {
			total.t_avm += object->size;
			total.t_arm += object->resident_page_count;
		}
		if (object->shadow_count > 1) {
			/* shared object */
			total.t_vmshr += object->size;
			total.t_rmshr += object->resident_page_count;
			if (is_object_active(object)) {
				total.t_avmshr += object->size;
				total.t_armshr += object->resident_page_count;
			}
		}
	}
	mtx_unlock(&vm_object_list_mtx);
	total.t_free = cnt.v_free_count + cnt.v_cache_count;
	return (sysctl_handle_opaque(oidp, &total, sizeof(total), req));
}

/*
 * vcnt() -	accumulate statistics from all cpus and the global cnt
 *		structure.
 *
 *	The vmmeter structure is now per-cpu as well as global.  Those
 *	statistics which can be kept on a per-cpu basis (to avoid cache
 *	stalls between cpus) can be moved to the per-cpu vmmeter.  Remaining
 *	statistics, such as v_free_reserved, are left in the global
 *	structure.
 *
 * (sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
 */
static int
vcnt(SYSCTL_HANDLER_ARGS)
{
	int count = *(int *)arg1;
	int offset = (char *)arg1 - (char *)&cnt;
	int i;

	CPU_FOREACH(i) {
		struct pcpu *pcpu = pcpu_find(i);
		count += *(int *)((char *)&pcpu->pc_cnt + offset);
	}
	return (SYSCTL_OUT(req, &count, sizeof(int)));
}

SYSCTL_PROC(_vm, VM_TOTAL, vmtotal, CTLTYPE_OPAQUE|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0, sizeof(struct vmtotal), vmtotal, "S,vmtotal", 
    "System virtual memory statistics");
SYSCTL_NODE(_vm, OID_AUTO, stats, CTLFLAG_RW, 0, "VM meter stats");
static SYSCTL_NODE(_vm_stats, OID_AUTO, sys, CTLFLAG_RW, 0,
	"VM meter sys stats");
static SYSCTL_NODE(_vm_stats, OID_AUTO, vm, CTLFLAG_RW, 0,
	"VM meter vm stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, misc, CTLFLAG_RW, 0, "VM meter misc stats");

#define	VM_STATS(parent, var, descr) \
	SYSCTL_PROC(parent, OID_AUTO, var, \
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE, &cnt.var, 0, vcnt, \
	    "IU", descr)
#define	VM_STATS_VM(var, descr)		VM_STATS(_vm_stats_vm, var, descr)
#define	VM_STATS_SYS(var, descr)	VM_STATS(_vm_stats_sys, var, descr)

VM_STATS_SYS(v_swtch, "Context switches");
VM_STATS_SYS(v_trap, "Traps");
VM_STATS_SYS(v_syscall, "System calls");
VM_STATS_SYS(v_intr, "Device interrupts");
VM_STATS_SYS(v_soft, "Software interrupts");
VM_STATS_VM(v_vm_faults, "Address memory faults");
VM_STATS_VM(v_io_faults, "Page faults requiring I/O");
VM_STATS_VM(v_cow_faults, "Copy-on-write faults");
VM_STATS_VM(v_cow_optim, "Optimized COW faults");
VM_STATS_VM(v_zfod, "Pages zero-filled on demand");
VM_STATS_VM(v_ozfod, "Optimized zero fill pages");
VM_STATS_VM(v_swapin, "Swap pager pageins");
VM_STATS_VM(v_swapout, "Swap pager pageouts");
VM_STATS_VM(v_swappgsin, "Swap pages swapped in");
VM_STATS_VM(v_swappgsout, "Swap pages swapped out");
VM_STATS_VM(v_vnodein, "Vnode pager pageins");
VM_STATS_VM(v_vnodeout, "Vnode pager pageouts");
VM_STATS_VM(v_vnodepgsin, "Vnode pages paged in");
VM_STATS_VM(v_vnodepgsout, "Vnode pages paged out");
VM_STATS_VM(v_intrans, "In transit page faults");
VM_STATS_VM(v_reactivated, "Pages reactivated from free list");
VM_STATS_VM(v_pdwakeups, "Pagedaemon wakeups");
VM_STATS_VM(v_pdpages, "Pages analyzed by pagedaemon");
VM_STATS_VM(v_tcached, "Total pages cached");
VM_STATS_VM(v_dfree, "Pages freed by pagedaemon");
VM_STATS_VM(v_pfree, "Pages freed by exiting processes");
VM_STATS_VM(v_tfree, "Total pages freed");
VM_STATS_VM(v_page_size, "Page size in bytes");
VM_STATS_VM(v_page_count, "Total number of pages in system");
VM_STATS_VM(v_free_reserved, "Pages reserved for deadlock");
VM_STATS_VM(v_free_target, "Pages desired free");
VM_STATS_VM(v_free_min, "Minimum low-free-pages threshold");
VM_STATS_VM(v_free_count, "Free pages");
VM_STATS_VM(v_wire_count, "Wired pages");
VM_STATS_VM(v_active_count, "Active pages");
VM_STATS_VM(v_inactive_target, "Desired inactive pages");
VM_STATS_VM(v_inactive_count, "Inactive pages");
VM_STATS_VM(v_cache_count, "Pages on cache queue");
VM_STATS_VM(v_cache_min, "Min pages on cache queue");
VM_STATS_VM(v_cache_max, "Max pages on cached queue");
VM_STATS_VM(v_pageout_free_min, "Min pages reserved for kernel");
VM_STATS_VM(v_interrupt_free_min, "Reserved pages for interrupt code");
VM_STATS_VM(v_forks, "Number of fork() calls");
VM_STATS_VM(v_vforks, "Number of vfork() calls");
VM_STATS_VM(v_rforks, "Number of rfork() calls");
VM_STATS_VM(v_kthreads, "Number of fork() calls by kernel");
VM_STATS_VM(v_forkpages, "VM pages affected by fork()");
VM_STATS_VM(v_vforkpages, "VM pages affected by vfork()");
VM_STATS_VM(v_rforkpages, "VM pages affected by rfork()");
VM_STATS_VM(v_kthreadpages, "VM pages affected by fork() by kernel");

SYSCTL_INT(_vm_stats_misc, OID_AUTO, zero_page_count, CTLFLAG_RD,
	&vm_page_zero_count, 0, "Number of zero-ed free pages");
