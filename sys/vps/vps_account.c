/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
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

static const char vpsid[] =
    "$Id: vps_account.c 153 2013-06-03 16:18:17Z klaus $";

/*
 * Resource accounting and limiting.
 *
 * This is an attempt to quickly provide a working solution rather
 * than implementing the most perfect scheduling algorithms.
 *
 */

#include "opt_ddb.h"
#include "opt_global.h"

#ifdef VPS

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/ioccom.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/syscallsubr.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/kthread.h>

#include <net/if.h>
#include <netinet/in.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <fs/vpsfs/vpsfs.h>

#include "vps_account.h"
#include "vps_user.h"
#include "vps.h"
#include "vps2.h"

/* kern/vfs_bio.c */
int vps_bio_runningbufspace_high(void);

#ifdef DIAGNOSTIC

#define DBGACC if (vps_debug_acc) printf

int vps_debug_acc = 1;
SYSCTL_INT(_debug, OID_AUTO, vps_account_debug, CTLFLAG_RW,
    &vps_debug_acc, 0, "");

#else

#define DBGACC(x, ...)

#endif /* DIAGNOSTIC */

SYSCTL_NODE(, OID_AUTO, vps, CTLFLAG_RD, NULL, "Virtual Private Systems");
SYSCTL_NODE(_vps, OID_AUTO, acc, CTLFLAG_RD, NULL, "Limits and Accounting");

/* Interval between calls to vps_account_threads() in microseconds. */
/* XXX determine a good default value */
static u_int vps_account_threads_interval = 100*1000;
SYSCTL_UINT(_vps_acc, OID_AUTO, account_threads_interval, CTLFLAG_RW,
    &vps_account_threads_interval, 0,
    "Interval for thread accounting in microseconds");

struct mtx		vps_pfault_mtx;
struct rqhead		vps_paused_threads_head;

static u_int		vps_account_suspensions;
static u_int		vps_account_failedsuspensions;
struct sx		vps_account_threads_sx;
struct mtx		vps_account_pausedqueue_mtx;
struct task		vps_account_threads_task;
static struct proc	*vps_account_kproc_p;
static int		vps_account_exit;

static fixpt_t		vps_account_cpu_idlepct;
static u_long		vps_account_cpu_last;
static long		vps_account_cpu_updated;

static int _vps_account2(struct vps *, int, int, size_t);
static void vps_account_check_threads(void);
static void vps_account_thread_resume(struct thread *td);
void vps_account_threads2(void *, int);
void vps_account_kproc(void *);

int
vps_account_init(void)
{
	int error;

	vps_account_suspensions = vps_account_failedsuspensions = 0;

	mtx_init(&vps_pfault_mtx, "vps accounting page mutex",
	    NULL, MTX_DEF);

	sx_init(&vps_account_threads_sx, "vps accounting threads lock");

	mtx_init(&vps_account_pausedqueue_mtx, "vps accounting paused "
	    "queue lock", NULL, MTX_SPIN);

	TAILQ_INIT(&vps_paused_threads_head);

	error = kproc_create(vps_account_kproc, NULL, &vps_account_kproc_p,
		0, 0, "vps_account");
	KASSERT(error == 0, ("%s: kproc_create() error=%d\n",
	    __func__, error));

	vps_func->vps_account = _vps_account;
	vps_func->vps_account_waitpfault = _vps_account_waitpfault;
	vps_func->vps_account_bio = _vps_account_bio;
	vps_func->vps_account_stats = _vps_account_stats;
	vps_func->vps_account_runnable = _vps_account_runnable;
	vps_func->vps_account_thread_pause = _vps_account_thread_pause;
	vps_func->vps_limit_setitem = _vps_limit_setitem;
	vps_func->vps_limit_getitemall = _vps_limit_getitemall;

	return (0);
}

int
vps_account_uninit(void)
{
	struct thread *td, *td2;
	struct vps *vps;

	/* Tell kproc to exit. */
	vps_account_exit = 1;

	tsleep(vps_account_kproc_p, 0, "uninit", 0);

	/* Wakeup vps instances that might sleep for resources. */
	sx_slock(&vps_all_lock);
	LIST_FOREACH(vps, &vps_head, vps_all)
		wakeup(vps);
	sx_sunlock(&vps_all_lock);

	/* Resume paused threads. */
	mtx_lock_spin(&vps_account_pausedqueue_mtx);
	TAILQ_FOREACH_SAFE(td, &vps_paused_threads_head, td_runq, td2) {
		mtx_unlock_spin(&vps_account_pausedqueue_mtx);
		thread_lock(td);
		if (td->td_flags & TDF_VPSLIMIT)
			vps_account_thread_resume(td);
		thread_unlock(td);
		mtx_lock_spin(&vps_account_pausedqueue_mtx);
	}
	mtx_unlock_spin(&vps_account_pausedqueue_mtx);

	vps_func->vps_account = NULL;
	vps_func->vps_account_waitpfault = NULL;
	vps_func->vps_account_bio = NULL;
	vps_func->vps_account_stats = NULL;
	vps_func->vps_account_runnable = NULL;
	vps_func->vps_account_thread_pause = NULL;
	vps_func->vps_limit_setitem = NULL;
	vps_func->vps_limit_getitemall = NULL;

	mtx_destroy(&vps_account_pausedqueue_mtx);
	sx_destroy(&vps_account_threads_sx);
	mtx_destroy(&vps_pfault_mtx);

	return (0);
}

void
vps_account_kproc(void *dummy)
{

	for (;;) {
		vps_account_threads(NULL);
		pause("pause", vps_account_threads_interval /
		    (1000000 / hz));
		if (vps_account_exit != 0)
			kproc_exit(0);
	}
}

int
_vps_account(struct vps *vps, int type, int action, size_t size)
{
	return (_vps_account2(vps, type, action, size));
}

int
_vps_account2(struct vps *vps, int type, int action, size_t size)
{
	struct vps_acc_val *val;
	int error;

	if (vps == NULL)
		return (0);

	switch (type) {
		case VPS_ACC_VIRT:
			val = &vps->vps_acc->virt;
			break;
		case VPS_ACC_PHYS:
			val = &vps->vps_acc->phys;
			break;
		case VPS_ACC_KMEM:
			val = &vps->vps_acc->kmem;
			break;
		case VPS_ACC_KERNEL:
			val = &vps->vps_acc->kernel;
			break;
		case VPS_ACC_BUFFER:
			val = &vps->vps_acc->buffer;
			break;
		case VPS_ACC_THREADS:
			val = &vps->vps_acc->threads;
			break;
		case VPS_ACC_PROCS:
			val = &vps->vps_acc->procs;
			break;
		/* not handled here: VPS_ACC_PCTCPU: */
		default:
			printf("%s: unkown type %d\n", __func__, type);
			return (ENOENT);
			break;
	}

	mtx_lock_spin(&vps->vps_acc->lock);

	error = 0;

	switch (action) {
		case VPS_ACC_ALLOC:
			if (val->hard == 0 && val->soft == 0) {
				/* No limits, skip checks. */
				val->cur += size;
			} else
			if (val->cur + size > val->hard) {
				DBGACC("%s: type=%d hit hard limit cur=%zu "
				    "soft=%zu hard=%zu size=%zu\n",
				    __func__, type, val->cur, val->soft,
				    val->hard, size);
				++val->hits_hard;
				error = ENOMEM;
				/* XXX for some values ENOSPC might be
				   more appropriate. */
			} else
			if (val->cur + size > val->soft) {
				DBGACC("%s: type=%d hit soft limit cur=%zu "
				    "soft=%zu hard=%zu size=%zu\n",
				    __func__, type, val->cur, val->soft,
				    val->hard, size);
				val->cur += size;
				++val->hits_soft;
			} else {
				/* below limits */
				val->cur += size;
			}
			break;
		case VPS_ACC_FREE:
			val->cur -= size;
			break;
		default:
			printf("%s: unkown action %d, type=%d size=%zu\n",
				__func__, type, action, size);
			break;
	}

	if (error == 0)
		val->updated = ticks;

	/* XXX only call if someone's waiting */
	if (type == VPS_ACC_PHYS &&
	    (val->cur < val->hard || (val->hard == 0 && val->soft == 0) ) )
		wakeup(vps);

	mtx_unlock_spin(&vps->vps_acc->lock);

	return (error);
}

/*
 * Called by vm_fault() if vps instance is currently not allowed to allocate
 * more pages (physical memory).
 *
 * This will always lead to situation where the entire vps instance
 * locks up, until either
 * - the limit is raised
 * - the limit is deactivated (zero)
 *   (XXX have to do wakeup after setting new limit)
 * - processes unmap (?) memory or exit.
 *
 * Or ...
 */

#if 0

void
_vps_account_waitpfault(struct vps *vps)
{
	mtx_lock(&vps_pfault_mtx);

	msleep(vps, &vps_pfault_mtx, PDROP | PUSER, "pfault", 0);
}

#else

/*
 * Killing the biggest process in vps instance.
 */

/* Copied and adapted vm_pageout_oom(). */
int
_vps_account_waitpfault(struct vps *vps)
{
	struct proc *p, *bigproc;
	vm_offset_t size, bigsize;
	struct thread *td;
	struct vmspace *vm;
	int breakout;

	bigproc = NULL;
	bigsize = 0;

	sx_slock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {

		if (PROC_TRYLOCK(p) == 0)
			continue;

		if (p->p_state != PRS_NORMAL ||
		    (p->p_flag & (P_INEXEC | P_PROTECTED | P_SYSTEM)) ||
		    P_KILLED(p)) {
			PROC_UNLOCK(p);
			continue;
		}

		breakout = 0;
		FOREACH_THREAD_IN_PROC(p, td) {
			thread_lock(td);
			if (!TD_ON_RUNQ(td) &&
			    !TD_IS_RUNNING(td) &&
			    !TD_IS_SLEEPING(td) &&
			    !TD_IS_SUSPENDED(td)) {
				thread_unlock(td);
				breakout = 1;
				break;
			}
			thread_unlock(td);
		}
		if (breakout) {
			PROC_UNLOCK(p);
			continue;
		}

		vm = vmspace_acquire_ref(p);
		if (vm == NULL) {
			PROC_UNLOCK(p);
			continue;
		}
		if (!vm_map_trylock_read(&vm->vm_map)) {
			vmspace_free(vm);
			PROC_UNLOCK(p);
			continue;
		}
		size = vmspace_swap_count(vm);
		vm_map_unlock_read(&vm->vm_map);
		size += vmspace_resident_count(vm);
		vmspace_free(vm);

		if (size > bigsize) {
			if (bigproc != NULL)
				PROC_UNLOCK(bigproc);
			bigproc = p;
			bigsize = size;
		} else
			PROC_UNLOCK(p);
	}
	sx_sunlock(&VPS_VPS(vps, allproc_lock));

	if (bigproc != NULL) {
		killproc(bigproc, "enforcing vps mem limit");
		sched_nice(bigproc, PRIO_MIN);
		PROC_UNLOCK(bigproc);
	}

	/* vm_fault() will abort we killed curproc, otherwise retry. */
	if (bigproc == curproc)
		return (1);
	else
		return (0);
}

#endif /* 0 */

static
void
vps_account_bio_update(struct vps *vps)
{
	struct vps_acc_val *val;
	size_t newval;

	val = &vps->vps_acc->blockio;

	if (((ticks - val->updated) / hz) == 0)
		return;

	/*
	 * This is supposed to represent a sort of ''sliding window''
	 * calculation.
	 */
	newval = (val->cnt_cur / ((ticks - val->updated) / hz));
	val->cur = (newval + val->cur * 2) / 3;
	val->cnt_cur = 0;
	val->updated = ticks;

	/*
	DBGACC("%s: vps=%p [%s] blockio rate=%zu/sec val->cnt_cur=%zu "
	    "val->updated=%d\n", __func__, vps, vps->vps_name, val->cur,
	    val->cnt_cur, val->updated);
	*/
}

/*
 * XXX Attaching to vfs_aio !
 */
int
_vps_account_bio(struct thread *td)
{
	struct vps_acc_val *val;
	struct vps *vps;
	int wait;

	vps = td->td_vps;

	if (vps == NULL || vps->vps_acc == NULL)
		return (0);

	mtx_lock_spin(&vps->vps_acc->lock);

 retry:
	val = &vps->vps_acc->blockio;

	if (val->soft == 0 && val->hard == 0) {
		wait = 0;
	} else if (val->cur > val->hard) {
		wait = 1;
	} else if (val->cur > val->soft) {
		/* Allow in case global I/O load (XXX or per device??)
		   is moderate. */
		if (vps_bio_runningbufspace_high())
			wait = 1;
		else
			wait = 0;
	} else {
		wait = 0;
	}

	if (wait != 0) {
		/*
		DBGACC("%s: td=%p [%d] waiting\n",
		    __func__, td, td->td_tid);
		*/
		mtx_unlock_spin(&vps->vps_acc->lock);
		pause("vpsbio", hz);
		mtx_lock_spin(&vps->vps_acc->lock);
		vps_account_bio_update(vps);
		goto retry;
	}

	++val->cnt_cur;

	if ((ticks - val->updated) > hz)
		vps_account_bio_update(vps);

	mtx_unlock_spin(&vps->vps_acc->lock);

	return (0);
}

/*
 * Calculate cpu utilization.
 *
 * sched_pctcpu() * 100 / FSCALE == utilization in percent
 *
 * XXX: It isn't really doing what I expected.
 *      The sum of pctcpu for all threads of a VPS is kept quite
 *      close to the limit, but the real cpu utilization (monitored
 *      in the vmware esx gui) increases with the number of active
 *      threads ...
 *
 */
void
vps_account_threads(void *dummy)
{
	struct vps *vps;
	struct thread *td;
	struct proc *p;
	fixpt_t pctcpu;
	fixpt_t cpuvpstot;
	fixpt_t biovpstot;
	long cpu_time[CPUSTATES];
	long ticks_delta;
	int threads;

	/*
	 * Never run more than once at the same time.
	 */
	if (sx_try_xlock(&vps_account_threads_sx) == 0)
		return;

	/*
	 * If we modify curthread->td_vps here,
	 * we might pause ourselves ...
	 */

	threads = 0;
	cpuvpstot = 0;
	biovpstot = 0;

	sx_slock(&vps_all_lock);
	LIST_FOREACH(vps, &vps_head, vps_all) {
		threads = 0;
		cpuvpstot = 0;

		//sx_slock(&vps->_proctree_lock);
		sx_slock(&VPS_VPS(vps, allproc_lock));
		LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
			if (p->p_state != PRS_NORMAL)
				continue;
			/*
			if (p->p_flag & P_SYSTEM)
				continue;
			*/
			PROC_LOCK(p);
			TAILQ_FOREACH(td, &p->p_threads, td_plist) {

				thread_lock(td);
				++threads;
				pctcpu = sched_pctcpu(td);
				/* XXX Why not account P_SYSTEM procs ? */
				if ((p->p_flag & P_SYSTEM) == 0)
					cpuvpstot += pctcpu;
				thread_unlock(td);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&VPS_VPS(vps, allproc_lock));
		//sx_sunlock(&vps->_proctree_lock);

		mtx_lock_spin(&vps->vps_acc->lock);

		vps->vps_acc->pctcpu.cur = cpuvpstot;
		vps->vps_acc->pctcpu.updated = ticks;

		vps->vps_acc->nthreads.cur = threads;
		vps->vps_acc->nthreads.updated = ticks;

		/*
		 * Make sure blockio stats get updated at least
		 * every 4 seconds.
		 */
		if (vps->vps_acc->blockio.cur > 0 &&
		    ((ticks - vps->vps_acc->blockio.updated) > (hz * 4)))
			vps_account_bio_update(vps);

		mtx_unlock_spin(&vps->vps_acc->lock);

		/*
		DBGACC("%s: vps=%p [%s] threads=%d cpuvpstot=%u\n",
			__func__, vps, vps->vps_name, threads, cpuvpstot);
		*/
	}
	sx_sunlock(&vps_all_lock);

	/* Calculate global cpu idle time. */
	/* XXX handle counter wrap */
	read_cpu_time(cpu_time);
	ticks_delta = ticks - vps_account_cpu_updated;
	if (ticks_delta > 0) {
		vps_account_cpu_idlepct = ((cpu_time[4] -
		    vps_account_cpu_last) * FSCALE) / ticks_delta;
	} else {
		printf("%s: WARNING: ticks=%d "
		    "vps_account_cpu_updated=%ld\n",
		    __func__, ticks, vps_account_cpu_updated);
	}
	vps_account_cpu_last = cpu_time[4];
	vps_account_cpu_updated = ticks;

	vps_account_check_threads();

	sx_unlock(&vps_account_threads_sx);
}

void
_vps_account_thread_pause(struct thread *td)
{

	/*
	 * We can't call sched_rem() because thread is already
	 * removed from run queue.
	 */

	thread_lock(td);
	TD_SET_CAN_RUN(td);
	sched_rem_norunq(td);
	thread_unlock(td);

	mtx_lock_spin(&vps_account_pausedqueue_mtx);
	/* Thread is already off the run queues. */
	td->td_flags |= TDF_VPSLIMIT;	/* XXX _Should_ be protected
					   by thread_lock */
	TAILQ_INSERT_TAIL(&vps_paused_threads_head, td, td_runq);
	mtx_unlock_spin(&vps_account_pausedqueue_mtx);
	/*DBGACC("%s: paused thread=%p/%d\n", __func__, td, td->td_tid);*/
}

static void
vps_account_thread_resume(struct thread *td)
{

	THREAD_LOCK_ASSERT(td, MA_OWNED);

	TAILQ_REMOVE(&vps_paused_threads_head, td, td_runq);
	td->td_flags &= ~TDF_VPSLIMIT;
	mtx_unlock_spin(&vps_account_pausedqueue_mtx);

	sched_add(td, SRQ_BORING);
	/*DBGACC("%s: resumed thread=%p/%d\n", __func__, td, td->td_tid);*/

}

static void
vps_account_check_threads(void)
{
	struct thread *td, *td2;

	mtx_lock_spin(&vps_account_pausedqueue_mtx);
	TAILQ_FOREACH_SAFE(td, &vps_paused_threads_head, td_runq, td2) {
		mtx_unlock_spin(&vps_account_pausedqueue_mtx);
		thread_lock(td);
		if (_vps_account_runnable(td)) {
			/* If TDF_VPSLIMIT is missing thread is
			   already removed. */
			mtx_lock_spin(&vps_account_pausedqueue_mtx);
			if (td->td_flags & TDF_VPSLIMIT)
				vps_account_thread_resume(td);
			else
				mtx_unlock_spin(
				    &vps_account_pausedqueue_mtx);
		}
		thread_unlock(td);
		mtx_lock_spin(&vps_account_pausedqueue_mtx);
	}
	mtx_unlock_spin(&vps_account_pausedqueue_mtx);
}

int
_vps_account_runnable(struct thread *td)
{
	struct vps_acc *vacc;
	struct vps *vps;
	int rc;

	rc = 1;

	if (td->td_vps_acc == NULL)
		return (1);

	vacc = td->td_vps_acc;
	vps = vacc->vps;

	KASSERT(vps->vps_status != VPS_ST_DEAD,
		("%s: vps=%p [%s] state=VPS_ST_DEAD, td=%p\n",
		__func__, vps, vps->vps_name, td));

	mtx_lock_spin(&vacc->lock);
	if (vacc->pctcpu.soft == 0 && vacc->pctcpu.hard == 0) {
		rc = 1;
		goto out;
	} else
	/* Check if this thread is not one of the big cpu-consumers. */
	if (vacc->nthreads.cur > 0 &&
	   sched_pctcpu(td) < (vacc->pctcpu.cur / vacc->nthreads.cur)) {
		rc = 1;
		goto out;
	} else
	if (vacc->pctcpu.cur > vacc->pctcpu.hard) {
		/* Don't run it. */
		rc = 0;
		goto out;
	} else
	if (vacc->pctcpu.cur > vacc->pctcpu.soft) {
		/* If there's idle cpu time let it run. */
		if (vps_account_cpu_idlepct > (FSCALE / 8))
			rc = 1;
		else
			rc = 0;
		goto out;
	}

 out:
	/*
	 * In case the thread has locks run it anyway.
	 * Otherwise we introduce performance loss and blow up the system.
	 *
	 * If td was preempted while being in kernel mode,
	 * TDF_PREEMPTED is set.
	 * In this case we can't and don't want to keep td from running.
	 */
	if (rc == 0 && ((td->td_flags & TDF_PREEMPTED) || TD_ON_LOCK(td))) {
		/*
		DBGACC("%s: td=%p [%d][%s] is not to be run but was "
		    "preempted or is on lock!\n",
		    __func__, td, td->td_tid, td->td_proc->p_comm);
		*/
		rc = 1;
		++vps_account_failedsuspensions;
	}

	if (rc == 0)
		++vps_account_suspensions;

	mtx_unlock_spin(&vacc->lock);

	return (rc);
}

void
_vps_account_stats(struct vps *vps)
{
	struct vps_acc *va;

#ifdef DIAGNOSTIC
	if (vps_debug_acc == 0)
		return;
#endif

	va = vps->vps_acc;

	mtx_lock_spin(&vps->vps_acc->lock);

	printf("%s: vps=%p\n", __func__, vps);
	printf("%s: virt=%zu\n", __func__, va->virt.cur);
	printf("%s: phys=%zu\n", __func__, va->phys.cur);
	printf("%s: pctcpu=%zu\n", __func__, va->pctcpu.cur);
	printf("%s: blockio=%zu\n", __func__, va->blockio.cur);
	printf("%s: threads=%zu\n", __func__, va->threads.cur);
	printf("%s: procs=%zu\n", __func__, va->procs.cur);
	printf("%s: ---------------\n", __func__);

	mtx_unlock_spin(&vps->vps_acc->lock);
}

int
_vps_limit_setitem(struct vps *vpsp, struct vps *vps,
    struct vps_arg_item *item)
{
	struct vps_acc_val *val;
	int error;

	if (item->type != VPS_ARG_ITEM_LIMIT)
		return (EINVAL);

	if (vps->vps_acc == NULL)
		return (EINVAL);

	error = 0;
	mtx_lock_spin(&vps->vps_acc->lock);

	switch (item->u.limit.resource) {
	case VPS_ACC_VIRT:
		val = &vps->vps_acc->virt;
		break;
	case VPS_ACC_PHYS:
		val = &vps->vps_acc->phys;
		break;
	case VPS_ACC_KMEM:
		val = &vps->vps_acc->kmem;
		break;
	case VPS_ACC_KERNEL:
		val = &vps->vps_acc->kernel;
		break;
	case VPS_ACC_BUFFER:
		val = &vps->vps_acc->buffer;
		break;
	case VPS_ACC_PCTCPU:
		val = &vps->vps_acc->pctcpu;
		break;
	case VPS_ACC_BLOCKIO:
		val = &vps->vps_acc->blockio;
		break;
	case VPS_ACC_THREADS:
		val = &vps->vps_acc->threads;
		break;
	case VPS_ACC_PROCS:
		val = &vps->vps_acc->procs;
		break;
	default:
		error = EINVAL;
		goto out;
		break;
	}

	/*
	 * XXX
	 * For hierarchical resource limits check
	 * new limits against parent vps etc.
	 */

	val->soft = item->u.limit.soft;
	val->hard = item->u.limit.hard;

 out:
	mtx_unlock_spin(&vps->vps_acc->lock);

	return (error);
}

#define FILL(x, y, res) 					\
	do {							\
	(x)->type = VPS_ARG_ITEM_LIMIT;				\
	(x)->u.limit.resource = res;				\
	(x)->u.limit.cur = (y)->cur;				\
	(x)->u.limit.soft = (y)->soft;				\
	(x)->u.limit.hard = (y)->hard;				\
	(x)->u.limit.hits_soft = (y)->hits_soft;		\
	(x)->u.limit.hits_hard = (y)->hits_hard;		\
	} while (0);

#define ACC_ITEM_CNT 6

int
_vps_limit_getitemall(struct vps *vpsp, struct vps *vps,
    caddr_t kdata, size_t *kdatalen)
{
	struct vps_arg_item *item;

	if (vps->vps_acc == NULL) {
		*kdatalen = 0;
		return (0);
	}

	if ((sizeof(*item) * ACC_ITEM_CNT) > *kdatalen)
		return (ENOSPC);

	item = (struct vps_arg_item *)kdata;
	memset(item, 0, sizeof (*item) * ACC_ITEM_CNT);

	mtx_lock_spin(&vps->vps_acc->lock);

	FILL(item, &vps->vps_acc->virt, VPS_ACC_VIRT);
	item++;
	FILL(item, &vps->vps_acc->phys, VPS_ACC_PHYS);
	item++;
	FILL(item, &vps->vps_acc->pctcpu, VPS_ACC_PCTCPU);
	item++;
	FILL(item, &vps->vps_acc->blockio, VPS_ACC_BLOCKIO);
	item++;
	FILL(item, &vps->vps_acc->threads, VPS_ACC_THREADS);
	item++;
	FILL(item, &vps->vps_acc->procs, VPS_ACC_PROCS);
	item++;

	mtx_unlock_spin(&vps->vps_acc->lock);

	*kdatalen = sizeof (*item) * ACC_ITEM_CNT;

	return (0);
}

#undef FILL

void
vps_account_print_pctcpu(struct vps *vps)
{
	struct proc *p;
	struct thread *td;
	int threads;
	fixpt_t vpstot, pctcpu;

	threads = 0;
	vpstot = 0;

	//sx_slock(&vps->_proctree_lock);
	sx_slock(&VPS_VPS(vps, allproc_lock));
	LIST_FOREACH(p, &VPS_VPS(vps, allproc), p_list) {
		if (p->p_flag & P_SYSTEM)
			continue;
		PROC_LOCK(p);
		TAILQ_FOREACH(td, &p->p_threads, td_plist) {
			thread_lock(td);
			++threads;
			pctcpu = sched_pctcpu(td);
			thread_unlock(td);
			vpstot += pctcpu;
			printf("%s: td=%p tid=%d pctcpu=%u\n",
				__func__, td, td->td_tid, pctcpu);
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&VPS_VPS(vps, allproc_lock));
	//sx_sunlock(&vps->_proctree_lock);

	printf("%s: vps=%p [%s] threads=%d vpstot=%u\n",
		__func__, vps, vps->vps_name, threads, vpstot);
}

int
vps_account_vpsfs_calc_mount(struct vps *vps, struct mount *mp,
    caddr_t kdata, size_t *kdatalen)
{

	return (EOPNOTSUPP);
}

int
vps_account_vpsfs_calc_path(struct vps *vps, const char *path,
    caddr_t kdata, size_t *kdatalen)
{
	struct vpsfs_limits *usage;
	struct vps_arg_item *item;
	int error;

	if (vps_func->vpsfs_calcusage_path == NULL)
		return (EOPNOTSUPP);

	usage = malloc(sizeof(*usage), M_TEMP, M_WAITOK | M_ZERO);

	if ((error = vps_func->vpsfs_calcusage_path(path, usage))) {
		free(usage, M_TEMP);
		return (error);
	}

	item = (struct vps_arg_item *)kdata;
        item->type = VPS_ARG_ITEM_LIMIT;
        item->u.limit.resource = VPS_ACC_FSSPACE;
        item->u.limit.cur = usage->space_used;
        item++;
        item->type = VPS_ARG_ITEM_LIMIT;
        item->u.limit.resource = VPS_ACC_FSFILES;
        item->u.limit.cur = usage->nodes_used;
        item++;
        *kdatalen = ((caddr_t)item) - kdata;

	free(usage, M_TEMP);
	return (0);
}

static int
vps_account_modevent(module_t mod, int type, void *data)
{
	int error;

	switch (type) {
	case MOD_LOAD:
	    error = vps_account_init();
	    break;
	case MOD_UNLOAD:
	    error = vps_account_uninit();
	    break;
	default:
	    error = EOPNOTSUPP;
	    break;
	}
	return (error);
}

static moduledata_t vps_account_mod = {
	"vps_account",
	vps_account_modevent,
	0
};

DECLARE_MODULE(vps_account, vps_account_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

#endif /* VPS */

/* EOF */
