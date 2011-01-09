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
	CTLFLAG_RW, &cnt.v_free_min, 0, "");
SYSCTL_UINT(_vm, VM_V_FREE_TARGET, v_free_target,
	CTLFLAG_RW, &cnt.v_free_target, 0, "");
SYSCTL_UINT(_vm, VM_V_FREE_RESERVED, v_free_reserved,
	CTLFLAG_RW, &cnt.v_free_reserved, 0, "");
SYSCTL_UINT(_vm, VM_V_INACTIVE_TARGET, v_inactive_target,
	CTLFLAG_RW, &cnt.v_inactive_target, 0, "");
SYSCTL_UINT(_vm, VM_V_CACHE_MIN, v_cache_min,
	CTLFLAG_RW, &cnt.v_cache_min, 0, "");
SYSCTL_UINT(_vm, VM_V_CACHE_MAX, v_cache_max,
	CTLFLAG_RW, &cnt.v_cache_max, 0, "");
SYSCTL_UINT(_vm, VM_V_PAGEOUT_FREE_MIN, v_pageout_free_min,
	CTLFLAG_RW, &cnt.v_pageout_free_min, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, v_free_severe,
	CTLFLAG_RW, &cnt.v_free_severe, 0, "");

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

	bzero(&total, sizeof(total));
	/*
	 * Mark all objects as inactive.
	 */
	mtx_lock(&vm_object_list_mtx);
	TAILQ_FOREACH(object, &vm_object_list, object_list) {
		if (!VM_OBJECT_TRYLOCK(object)) {
			/*
			 * Avoid a lock-order reversal.  Consequently,
			 * the reported number of active pages may be
			 * greater than the actual number.
			 */
			continue;
		}
		vm_object_clear_flag(object, OBJ_ACTIVE);
		VM_OBJECT_UNLOCK(object);
	}
	mtx_unlock(&vm_object_list_mtx);
	/*
	 * Calculate process statistics.
	 */
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_flag & P_SYSTEM)
			continue;
		PROC_LOCK(p);
		PROC_SLOCK(p);
		switch (p->p_state) {
		case PRS_NEW:
			PROC_SUNLOCK(p);
			PROC_UNLOCK(p);
			continue;
			break;
		default:
			PROC_SUNLOCK(p);
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
			VM_OBJECT_LOCK(object);
			vm_object_set_flag(object, OBJ_ACTIVE);
			paging |= object->paging_in_progress;
			VM_OBJECT_UNLOCK(object);
		}
		vm_map_unlock_read(map);
		vmspace_free(vm);
		if (paging)
			total.t_pw++;
	}
	sx_sunlock(&allproc_lock);
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
		if (object->type == OBJT_DEVICE || object->type == OBJT_SG) {
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
#ifdef SMP
	int i;

	for (i = 0; i < mp_ncpus; ++i) {
		struct pcpu *pcpu = pcpu_find(i);
		count += *(int *)((char *)&pcpu->pc_cnt + offset);
	}
#else
	count += *(int *)((char *)PCPU_PTR(cnt) + offset);
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

SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_swtch, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_swtch, 0, vcnt, "IU", "Context switches");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_trap, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_trap, 0, vcnt, "IU", "Traps");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_syscall, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_syscall, 0, vcnt, "IU", "Syscalls");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_intr, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_intr, 0, vcnt, "IU", "Hardware interrupts");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_soft, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_soft, 0, vcnt, "IU", "Software interrupts");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vm_faults, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vm_faults, 0, vcnt, "IU", "VM faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cow_faults, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_cow_faults, 0, vcnt, "IU", "COW faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cow_optim, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_cow_optim, 0, vcnt, "IU", "Optimized COW faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_zfod, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_zfod, 0, vcnt, "IU", "Zero fill");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_ozfod, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_ozfod, 0, vcnt, "IU", "Optimized zero fill");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swapin, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_swapin, 0, vcnt, "IU", "Swapin operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swapout, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_swapout, 0, vcnt, "IU", "Swapout operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swappgsin, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_swappgsin, 0, vcnt, "IU", "Swapin pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swappgsout, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_swappgsout, 0, vcnt, "IU", "Swapout pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodein, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vnodein, 0, vcnt, "IU", "Vnodein operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodeout, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vnodeout, 0, vcnt, "IU", "Vnodeout operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodepgsin, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vnodepgsin, 0, vcnt, "IU", "Vnodein pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodepgsout, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vnodepgsout, 0, vcnt, "IU", "Vnodeout pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_intrans, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_intrans, 0, vcnt, "IU", "In transit page blocking");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_reactivated, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_reactivated, 0, vcnt, "IU", "Reactivated pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pdwakeups, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_pdwakeups, 0, vcnt, "IU", "Pagedaemon wakeups");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pdpages, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_pdpages, 0, vcnt, "IU", "Pagedaemon page scans");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_tcached, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_tcached, 0, vcnt, "IU", "Total pages cached");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_dfree, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_dfree, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pfree, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_pfree, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_tfree, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_tfree, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_page_size, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_page_size, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_page_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_page_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_free_reserved, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_free_reserved, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_free_target, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_free_target, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_free_min, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_free_min, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_free_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_free_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_wire_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_wire_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_active_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_active_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_inactive_target, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_inactive_target, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_inactive_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_inactive_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cache_count, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_cache_count, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cache_min, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_cache_min, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cache_max, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_cache_max, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pageout_free_min, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_pageout_free_min, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_interrupt_free_min, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_interrupt_free_min, 0, vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_forks, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_forks, 0, vcnt, "IU", "Number of fork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vforks, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vforks, 0, vcnt, "IU", "Number of vfork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_rforks, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_rforks, 0, vcnt, "IU", "Number of rfork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_kthreads, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_kthreads, 0, vcnt, "IU", "Number of fork() calls by kernel");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_forkpages, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_forkpages, 0, vcnt, "IU", "VM pages affected by fork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vforkpages, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_vforkpages, 0, vcnt, "IU", "VM pages affected by vfork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_rforkpages, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_rforkpages, 0, vcnt, "IU", "VM pages affected by rfork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_kthreadpages, CTLTYPE_UINT|CTLFLAG_RD|CTLFLAG_MPSAFE,
	&cnt.v_kthreadpages, 0, vcnt, "IU", "VM pages affected by fork() by kernel");

SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	zero_page_count, CTLFLAG_RD, &vm_page_zero_count, 0, "");
