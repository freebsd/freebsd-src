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

#include <vps/vps.h>
#include <vps/vps2.h>

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

#ifdef VPS
/* XXX calculate real per-vps load avg values */
static int
sysctl_vm_loadavg(SYSCTL_HANDLER_ARGS)
{
	struct loadavg lafake;
#ifdef SCTL_MASK32
	u_int32_t la[4];

	if (req->flags & SCTL_MASK32) {
		if (req->td->td_vps != vps0) {
			memset(&la, 0, sizeof(la));	
		} else {
			la[0] = averunnable.ldavg[0];
			la[1] = averunnable.ldavg[1];
			la[2] = averunnable.ldavg[2];
			la[3] = averunnable.fscale;
		}
		return SYSCTL_OUT(req, la, sizeof(la));
	} else
#endif
		if (req->td->td_vps != vps0) {
			memset(&lafake, 0, sizeof(lafake));
			return SYSCTL_OUT(req, &lafake, sizeof(lafake));
		} else {
			return SYSCTL_OUT(req, &averunnable, sizeof(averunnable));
		}
}
#else
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
#endif /* !VPS */
_SYSCTL_PROC(_vm, VM_LOADAVG, loadavg, CTLTYPE_STRUCT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_vm_loadavg, "S,loadavg",
    "Machine loadaverage history", VPS_PUBLIC);

static int
vmtotal(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	struct vmtotal total;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_map_t map;
	int paging;
	struct thread *td;
	struct vmspace *vm;
#ifdef VPS
	struct vps *vps, *save_vps;
#endif  

	bzero(&total, sizeof(total));
	/*
	 * Mark all objects as inactive.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		if (!VM_OBJECT_TRYWLOCK(object)) {
			/*
			 * Avoid a lock-order reversal.  Consequently,
			 * the reported number of active pages may be
			 * greater than the actual number.
			 */
			continue;
		}
		vm_object_clear_flag(object, OBJ_ACTIVE);
		VM_OBJECT_WUNLOCK(object);
	}
	mtx_unlock(&vm_object_list_mtx);
	/*
	 * Calculate process statistics.
	 */
#ifdef VPS
	save_vps = curthread->td_vps;
	sx_slock(&vps_all_lock);
	LIST_FOREACH(vps, &vps_head, vps_all) {
		curthread->td_vps = vps;
#endif
	sx_slock(&V_allproc_lock);
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
					else if (TD_IS_SLEEPING(td) &&
					    td->td_priority <= PZERO)
						total.t_dw++;
					else
						total.t_sl++;
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
		/*
		 * Note active objects.
		 */
		paging = 0;
		vm = vmspace_acquire_ref(p);
		if (vm == NULL)
			continue;
		map = &vm->vm_map;
		vm_map_lock_read(map);
		for (entry = map->header.next;
		    entry != &map->header; entry = entry->next) {
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) ||
			    (object = entry->object.vm_object) == NULL)
				continue;
			VM_OBJECT_WLOCK(object);
			vm_object_set_flag(object, OBJ_ACTIVE);
			paging |= object->paging_in_progress;
			VM_OBJECT_WUNLOCK(object);
		}
		vm_map_unlock_read(map);
		vmspace_free(vm);
		if (paging)
			total.t_pw++;
	}
	sx_sunlock(&V_allproc_lock);
#ifdef VPS
	}
	sx_sunlock(&vps_all_lock);
	curthread->td_vps = save_vps;
#endif
	/*
	 * Calculate object memory usage statistics.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		/*
		 * Perform unsynchronized reads on the object to avoid
		 * a lock-order reversal.  In this case, the lack of
		 * synchronization should not impair the accuracy of
		 * the reported statistics. 
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
		total.t_vm += object->size;
		total.t_rm += object->resident_page_count;
		if (object->flags & OBJ_ACTIVE) {
			total.t_avm += object->size;
			total.t_arm += object->resident_page_count;
		}
		if (object->shadow_count > 1) {
			/* shared object */
			total.t_vmshr += object->size;
			total.t_rmshr += object->resident_page_count;
			if (object->flags & OBJ_ACTIVE) {
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
#ifdef VPS
	u_int fakeval;
#endif

	CPU_FOREACH(i) {
		struct pcpu *pcpu = pcpu_find(i);
		count += *(int *)((char *)&pcpu->pc_cnt + offset);
	}
#ifdef VPS
	if (req->td->td_vps != vps0) {
		/* XXX calc real per-vps values */
		fakeval = 0;
		return (SYSCTL_OUT(req, &fakeval, sizeof(int)));
	}
#endif
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

#define		VM_STATS(parent, var, descr, vps) \
	_SYSCTL_PROC(parent, OID_AUTO, var, \
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE, &cnt.var, 0, vcnt, \
	    "IU", descr, vps)
#define		VM_STATS_VM(var, descr, vps)         VM_STATS(_vm_stats_vm, var, descr, vps)
#define		VM_STATS_SYS(var, descr, vps)        VM_STATS(_vm_stats_sys, var, descr, vps)

VM_STATS_SYS(v_swtch, "Context switches", VPS_0);
VM_STATS_SYS(v_trap, "Traps", VPS_0);
VM_STATS_SYS(v_syscall, "System calls", VPS_0);
VM_STATS_SYS(v_intr, "Device interrupts", VPS_0);
VM_STATS_SYS(v_soft, "Software interrupts", VPS_0);
VM_STATS_VM(v_vm_faults, "Address memory faults", VPS_0);
VM_STATS_VM(v_io_faults, "Page faults requiring I/O", VPS_0);
VM_STATS_VM(v_cow_faults, "Copy-on-write faults", VPS_0);
VM_STATS_VM(v_cow_optim, "Optimized COW faults", VPS_0);
VM_STATS_VM(v_zfod, "Pages zero-filled on demand", VPS_0);
VM_STATS_VM(v_ozfod, "Optimized zero fill pages", VPS_0);
VM_STATS_VM(v_swapin, "Swap pager pageins", VPS_0);
VM_STATS_VM(v_swapout, "Swap pager pageouts", VPS_0);
VM_STATS_VM(v_swappgsin, "Swap pages swapped in", VPS_PUBLIC);
VM_STATS_VM(v_swappgsout, "Swap pages swapped out", VPS_PUBLIC);
VM_STATS_VM(v_vnodein, "Vnode pager pageins", VPS_0);
VM_STATS_VM(v_vnodeout, "Vnode pager pageouts", VPS_0);
VM_STATS_VM(v_vnodepgsin, "Vnode pages paged in", VPS_0);
VM_STATS_VM(v_vnodepgsout, "Vnode pages paged out", VPS_0);
VM_STATS_VM(v_intrans, "In transit page faults", VPS_0);
VM_STATS_VM(v_reactivated, "Pages reactivated from free list", VPS_0);
VM_STATS_VM(v_pdwakeups, "Pagedaemon wakeups", VPS_0);
VM_STATS_VM(v_pdpages, "Pages analyzed by pagedaemon", VPS_0);
VM_STATS_VM(v_tcached, "Total pages cached", VPS_0);
VM_STATS_VM(v_dfree, "Pages freed by pagedaemon", VPS_0);
VM_STATS_VM(v_pfree, "Pages freed by exiting processes", VPS_0);
VM_STATS_VM(v_tfree, "Total pages freed", VPS_0);
VM_STATS_VM(v_page_size, "Page size in bytes", VPS_0);
VM_STATS_VM(v_page_count, "Total number of pages in system", VPS_0);
VM_STATS_VM(v_free_reserved, "Pages reserved for deadlock", VPS_0);
VM_STATS_VM(v_free_target, "Pages desired free", VPS_0);
VM_STATS_VM(v_free_min, "Minimum low-free-pages threshold", VPS_0);
VM_STATS_VM(v_free_count, "Free pages", VPS_PUBLIC);
VM_STATS_VM(v_wire_count, "Wired pages", VPS_PUBLIC);
VM_STATS_VM(v_active_count, "Active pages", VPS_PUBLIC);
VM_STATS_VM(v_inactive_target, "Desired inactive pages", VPS_0);
VM_STATS_VM(v_inactive_count, "Inactive pages", VPS_PUBLIC);
VM_STATS_VM(v_cache_count, "Pages on cache queue", VPS_PUBLIC);
VM_STATS_VM(v_cache_min, "Min pages on cache queue", VPS_0);
VM_STATS_VM(v_cache_max, "Max pages on cached queue", VPS_0);
VM_STATS_VM(v_pageout_free_min, "Min pages reserved for kernel", VPS_0);
VM_STATS_VM(v_interrupt_free_min, "Reserved pages for interrupt code", VPS_0);
VM_STATS_VM(v_forks, "Number of fork() calls", VPS_0);
VM_STATS_VM(v_vforks, "Number of vfork() calls", VPS_0);
VM_STATS_VM(v_rforks, "Number of rfork() calls", VPS_0);
VM_STATS_VM(v_kthreads, "Number of fork() calls by kernel", VPS_0);
VM_STATS_VM(v_forkpages, "VM pages affected by fork()", VPS_0);
VM_STATS_VM(v_vforkpages, "VM pages affected by vfork()", VPS_0);
VM_STATS_VM(v_rforkpages, "VM pages affected by rfork()", VPS_0);
VM_STATS_VM(v_kthreadpages, "VM pages affected by fork() by kernel", VPS_0);

SYSCTL_INT(_vm_stats_misc, OID_AUTO, zero_page_count, CTLFLAG_RD,
	&vm_page_zero_count, 0, "Number of zero-ed free pages");
