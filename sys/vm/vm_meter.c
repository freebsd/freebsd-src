/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD: src/sys/vm/vm_meter.c,v 1.34.2.3 2000/08/03 00:09:43 ps Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <sys/sysctl.h>

struct loadavg averunnable =
	{ {0, 0, 0}, FSCALE };	/* load average, of runnable procs */

struct vmmeter cnt;

static int maxslp = MAXSLP;

/*
 * Constants for averages over 1, 5, and 15 minutes
 * when sampling at 5 second intervals.
 */
static fixpt_t cexp[3] = {
	0.9200444146293232 * FSCALE,	/* exp(-1/12) */
	0.9834714538216174 * FSCALE,	/* exp(-1/60) */
	0.9944598480048967 * FSCALE,	/* exp(-1/180) */
};

/*
 * Compute a tenex style load average of a quantity on
 * 1, 5 and 15 minute intervals.
 */
static void
loadav(struct loadavg *avg)
{
	register int i, nrun;
	register struct proc *p;

	for (nrun = 0, p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		switch (p->p_stat) {
		case SSLEEP:
			if (p->p_priority > PZERO || p->p_slptime != 0)
				continue;
			/* FALLTHROUGH */
		case SRUN:
		case SIDL:
			nrun++;
		}
	}
	for (i = 0; i < 3; i++)
		avg->ldavg[i] = (cexp[i] * avg->ldavg[i] +
		    nrun * FSCALE * (FSCALE - cexp[i])) >> FSHIFT;
}

void
vmmeter()
{

	if (time_second % 5 == 0)
		loadav(&averunnable);
	if (proc0.p_slptime > maxslp / 2)
		wakeup(&proc0);
}

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

SYSCTL_STRUCT(_vm, VM_LOADAVG, loadavg, CTLFLAG_RD, 
    &averunnable, loadavg, "Machine loadaverage history");

static int
vmtotal(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	struct vmtotal total, *totalp;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_map_t map;
	int paging;

	totalp = &total;
	bzero(totalp, sizeof *totalp);
	/*
	 * Mark all objects as inactive.
	 */
	for (object = TAILQ_FIRST(&vm_object_list);
	    object != NULL;
	    object = TAILQ_NEXT(object,object_list))
		vm_object_clear_flag(object, OBJ_ACTIVE);
	/*
	 * Calculate process statistics.
	 */
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_flag & P_SYSTEM)
			continue;
		switch (p->p_stat) {
		case 0:
			continue;

		case SSLEEP:
		case SSTOP:
			if (p->p_flag & P_INMEM) {
				if (p->p_priority <= PZERO)
					totalp->t_dw++;
				else if (p->p_slptime < maxslp)
					totalp->t_sl++;
			} else if (p->p_slptime < maxslp)
				totalp->t_sw++;
			if (p->p_slptime >= maxslp)
				continue;
			break;

		case SRUN:
		case SIDL:
			if (p->p_flag & P_INMEM)
				totalp->t_rq++;
			else
				totalp->t_sw++;
			if (p->p_stat == SIDL)
				continue;
			break;
		}
		/*
		 * Note active objects.
		 */
		paging = 0;
		for (map = &p->p_vmspace->vm_map, entry = map->header.next;
		    entry != &map->header; entry = entry->next) {
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) ||
			    entry->object.vm_object == NULL)
				continue;
			vm_object_set_flag(entry->object.vm_object, OBJ_ACTIVE);
			paging |= entry->object.vm_object->paging_in_progress;
		}
		if (paging)
			totalp->t_pw++;
	}
	/*
	 * Calculate object memory usage statistics.
	 */
	for (object = TAILQ_FIRST(&vm_object_list);
	    object != NULL;
	    object = TAILQ_NEXT(object, object_list)) {
		/*
		 * devices, like /dev/mem, will badly skew our totals
		 */
		if (object->type == OBJT_DEVICE)
			continue;
		totalp->t_vm += object->size;
		totalp->t_rm += object->resident_page_count;
		if (object->flags & OBJ_ACTIVE) {
			totalp->t_avm += object->size;
			totalp->t_arm += object->resident_page_count;
		}
		if (object->shadow_count > 1) {
			/* shared object */
			totalp->t_vmshr += object->size;
			totalp->t_rmshr += object->resident_page_count;
			if (object->flags & OBJ_ACTIVE) {
				totalp->t_avmshr += object->size;
				totalp->t_armshr += object->resident_page_count;
			}
		}
	}
	totalp->t_free = cnt.v_free_count + cnt.v_cache_count;
	return (sysctl_handle_opaque(oidp, totalp, sizeof total, req));
}

SYSCTL_PROC(_vm, VM_METER, vmmeter, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, sizeof(struct vmtotal), vmtotal, "S,vmtotal", 
    "System virtual memory statistics");
SYSCTL_NODE(_vm, OID_AUTO, stats, CTLFLAG_RW, 0, "VM meter stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, sys, CTLFLAG_RW, 0, "VM meter sys stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, vm, CTLFLAG_RW, 0, "VM meter vm stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, misc, CTLFLAG_RW, 0, "VM meter misc stats");
SYSCTL_UINT(_vm_stats_sys, OID_AUTO,
	v_swtch, CTLFLAG_RD, &cnt.v_swtch, 0, "Context switches");
SYSCTL_UINT(_vm_stats_sys, OID_AUTO,
	v_trap, CTLFLAG_RD, &cnt.v_trap, 0, "Traps");
SYSCTL_UINT(_vm_stats_sys, OID_AUTO,
	v_syscall, CTLFLAG_RD, &cnt.v_syscall, 0, "Syscalls");
SYSCTL_UINT(_vm_stats_sys, OID_AUTO, v_intr, CTLFLAG_RD,
    &cnt.v_intr, 0, "Hardware interrupts");
SYSCTL_UINT(_vm_stats_sys, OID_AUTO, v_soft, CTLFLAG_RD, 
    &cnt.v_soft, 0, "Software interrupts");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_vm_faults, CTLFLAG_RD, &cnt.v_vm_faults, 0, "VM faults");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cow_faults, CTLFLAG_RD, &cnt.v_cow_faults, 0, "COW faults");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cow_optim, CTLFLAG_RD, &cnt.v_cow_optim, 0, "Optimized COW faults");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_zfod, CTLFLAG_RD, &cnt.v_zfod, 0, "Zero fill");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_ozfod, CTLFLAG_RD, &cnt.v_ozfod, 0, "Optimized zero fill");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_swapin, CTLFLAG_RD, &cnt.v_swapin, 0, "Swapin operations");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_swapout, CTLFLAG_RD, &cnt.v_swapout, 0, "Swapout operations");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_swappgsin, CTLFLAG_RD, &cnt.v_swappgsin, 0, "Swapin pages");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_swappgsout, CTLFLAG_RD, &cnt.v_swappgsout, 0, "Swapout pages");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_vnodein, CTLFLAG_RD, &cnt.v_vnodein, 0, "Vnodein operations");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_vnodeout, CTLFLAG_RD, &cnt.v_vnodeout, 0, "Vnodeout operations");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_vnodepgsin, CTLFLAG_RD, &cnt.v_vnodepgsin, 0, "Vnodein pages");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_vnodepgsout, CTLFLAG_RD, &cnt.v_vnodepgsout, 0, "Vnodeout pages");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_intrans, CTLFLAG_RD, &cnt.v_intrans, 0, "In transit page blocking");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_reactivated, CTLFLAG_RD, &cnt.v_reactivated, 0, "Reactivated pages");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_pdwakeups, CTLFLAG_RD, &cnt.v_pdwakeups, 0, "Pagedaemon wakeups");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_pdpages, CTLFLAG_RD, &cnt.v_pdpages, 0, "Pagedaemon page scans");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_dfree, CTLFLAG_RD, &cnt.v_dfree, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_pfree, CTLFLAG_RD, &cnt.v_pfree, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_tfree, CTLFLAG_RD, &cnt.v_tfree, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_page_size, CTLFLAG_RD, &cnt.v_page_size, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_page_count, CTLFLAG_RD, &cnt.v_page_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_reserved, CTLFLAG_RD, &cnt.v_free_reserved, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_target, CTLFLAG_RD, &cnt.v_free_target, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_min, CTLFLAG_RD, &cnt.v_free_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_count, CTLFLAG_RD, &cnt.v_free_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_wire_count, CTLFLAG_RD, &cnt.v_wire_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_active_count, CTLFLAG_RD, &cnt.v_active_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_inactive_target, CTLFLAG_RD, &cnt.v_inactive_target, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_inactive_count, CTLFLAG_RD, &cnt.v_inactive_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_count, CTLFLAG_RD, &cnt.v_cache_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_min, CTLFLAG_RD, &cnt.v_cache_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_max, CTLFLAG_RD, &cnt.v_cache_max, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_pageout_free_min, CTLFLAG_RD, &cnt.v_pageout_free_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_interrupt_free_min, CTLFLAG_RD, &cnt.v_interrupt_free_min, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	zero_page_count, CTLFLAG_RD, &vm_page_zero_count, 0, "");
#if 0
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	page_mask, CTLFLAG_RD, &page_mask, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	page_shift, CTLFLAG_RD, &page_shift, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	first_page, CTLFLAG_RD, &first_page, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	last_page, CTLFLAG_RD, &last_page, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	vm_page_bucket_count, CTLFLAG_RD, &vm_page_bucket_count, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	vm_page_hash_mask, CTLFLAG_RD, &vm_page_hash_mask, 0, "");
#endif
