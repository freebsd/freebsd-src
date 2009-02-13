/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/kmem.h>
#include <sys/strsubr.h>
#include <sys/fasttrap.h>
#include <sys/fasttrap_impl.h>
#include <sys/fasttrap_isa.h>
#include <sys/dtrace.h>
#include <sys/dtrace_impl.h>
#include <sys/sysmacros.h>
#include <sys/proc.h>
#include <sys/priv.h>
#include <sys/policy.h>
#include <util/qsort.h>

/*
 * User-Land Trap-Based Tracing
 * ----------------------------
 *
 * The fasttrap provider allows DTrace consumers to instrument any user-level
 * instruction to gather data; this includes probes with semantic
 * signifigance like entry and return as well as simple offsets into the
 * function. While the specific techniques used are very ISA specific, the
 * methodology is generalizable to any architecture.
 *
 *
 * The General Methodology
 * -----------------------
 *
 * With the primary goal of tracing every user-land instruction and the
 * limitation that we can't trust user space so don't want to rely on much
 * information there, we begin by replacing the instructions we want to trace
 * with trap instructions. Each instruction we overwrite is saved into a hash
 * table keyed by process ID and pc address. When we enter the kernel due to
 * this trap instruction, we need the effects of the replaced instruction to
 * appear to have occurred before we proceed with the user thread's
 * execution.
 *
 * Each user level thread is represented by a ulwp_t structure which is
 * always easily accessible through a register. The most basic way to produce
 * the effects of the instruction we replaced is to copy that instruction out
 * to a bit of scratch space reserved in the user thread's ulwp_t structure
 * (a sort of kernel-private thread local storage), set the PC to that
 * scratch space and single step. When we reenter the kernel after single
 * stepping the instruction we must then adjust the PC to point to what would
 * normally be the next instruction. Of course, special care must be taken
 * for branches and jumps, but these represent such a small fraction of any
 * instruction set that writing the code to emulate these in the kernel is
 * not too difficult.
 *
 * Return probes may require several tracepoints to trace every return site,
 * and, conversely, each tracepoint may activate several probes (the entry
 * and offset 0 probes, for example). To solve this muliplexing problem,
 * tracepoints contain lists of probes to activate and probes contain lists
 * of tracepoints to enable. If a probe is activated, it adds its ID to
 * existing tracepoints or creates new ones as necessary.
 *
 * Most probes are activated _before_ the instruction is executed, but return
 * probes are activated _after_ the effects of the last instruction of the
 * function are visible. Return probes must be fired _after_ we have
 * single-stepped the instruction whereas all other probes are fired
 * beforehand.
 *
 *
 * Lock Ordering
 * -------------
 *
 * The lock ordering below -- both internally and with respect to the DTrace
 * framework -- is a little tricky and bears some explanation. Each provider
 * has a lock (ftp_mtx) that protects its members including reference counts
 * for enabled probes (ftp_rcount), consumers actively creating probes
 * (ftp_ccount) and USDT consumers (ftp_mcount); all three prevent a provider
 * from being freed. A provider is looked up by taking the bucket lock for the
 * provider hash table, and is returned with its lock held. The provider lock
 * may be taken in functions invoked by the DTrace framework, but may not be
 * held while calling functions in the DTrace framework.
 *
 * To ensure consistency over multiple calls to the DTrace framework, the
 * creation lock (ftp_cmtx) should be held. Naturally, the creation lock may
 * not be taken when holding the provider lock as that would create a cyclic
 * lock ordering. In situations where one would naturally take the provider
 * lock and then the creation lock, we instead up a reference count to prevent
 * the provider from disappearing, drop the provider lock, and acquire the
 * creation lock.
 *
 * Briefly:
 * 	bucket lock before provider lock
 *	DTrace before provider lock
 *	creation lock before DTrace
 *	never hold the provider lock and creation lock simultaneously
 */

static dev_info_t *fasttrap_devi;
static dtrace_meta_provider_id_t fasttrap_meta_id;

static timeout_id_t fasttrap_timeout;
static kmutex_t fasttrap_cleanup_mtx;
static uint_t fasttrap_cleanup_work;

/*
 * Generation count on modifications to the global tracepoint lookup table.
 */
static volatile uint64_t fasttrap_mod_gen;

/*
 * When the fasttrap provider is loaded, fasttrap_max is set to either
 * FASTTRAP_MAX_DEFAULT or the value for fasttrap-max-probes in the
 * fasttrap.conf file. Each time a probe is created, fasttrap_total is
 * incremented by the number of tracepoints that may be associated with that
 * probe; fasttrap_total is capped at fasttrap_max.
 */
#define	FASTTRAP_MAX_DEFAULT		250000
static uint32_t fasttrap_max;
static uint32_t fasttrap_total;


#define	FASTTRAP_TPOINTS_DEFAULT_SIZE	0x4000
#define	FASTTRAP_PROVIDERS_DEFAULT_SIZE	0x100
#define	FASTTRAP_PROCS_DEFAULT_SIZE	0x100

#define	FASTTRAP_PID_NAME		"pid"

fasttrap_hash_t			fasttrap_tpoints;
static fasttrap_hash_t		fasttrap_provs;
static fasttrap_hash_t		fasttrap_procs;

static uint64_t			fasttrap_pid_count;	/* pid ref count */
static kmutex_t			fasttrap_count_mtx;	/* lock on ref count */

#define	FASTTRAP_ENABLE_FAIL	1
#define	FASTTRAP_ENABLE_PARTIAL	2

static int fasttrap_tracepoint_enable(proc_t *, fasttrap_probe_t *, uint_t);
static void fasttrap_tracepoint_disable(proc_t *, fasttrap_probe_t *, uint_t);

static fasttrap_provider_t *fasttrap_provider_lookup(pid_t, const char *,
    const dtrace_pattr_t *);
static void fasttrap_provider_retire(pid_t, const char *, int);
static void fasttrap_provider_free(fasttrap_provider_t *);

static fasttrap_proc_t *fasttrap_proc_lookup(pid_t);
static void fasttrap_proc_release(fasttrap_proc_t *);

#define	FASTTRAP_PROVS_INDEX(pid, name) \
	((fasttrap_hash_str(name) + (pid)) & fasttrap_provs.fth_mask)

#define	FASTTRAP_PROCS_INDEX(pid) ((pid) & fasttrap_procs.fth_mask)

static int
fasttrap_highbit(ulong_t i)
{
	int h = 1;

	if (i == 0)
		return (0);
#ifdef _LP64
	if (i & 0xffffffff00000000ul) {
		h += 32; i >>= 32;
	}
#endif
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
}

static uint_t
fasttrap_hash_str(const char *p)
{
	unsigned int g;
	uint_t hval = 0;

	while (*p) {
		hval = (hval << 4) + *p++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;
		hval &= ~g;
	}
	return (hval);
}

void
fasttrap_sigtrap(proc_t *p, kthread_t *t, uintptr_t pc)
{
	sigqueue_t *sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);

	sqp->sq_info.si_signo = SIGTRAP;
	sqp->sq_info.si_code = TRAP_DTRACE;
	sqp->sq_info.si_addr = (caddr_t)pc;

	mutex_enter(&p->p_lock);
	sigaddqa(p, t, sqp);
	mutex_exit(&p->p_lock);

	if (t != NULL)
		aston(t);
}

/*
 * This function ensures that no threads are actively using the memory
 * associated with probes that were formerly live.
 */
static void
fasttrap_mod_barrier(uint64_t gen)
{
	int i;

	if (gen < fasttrap_mod_gen)
		return;

	fasttrap_mod_gen++;

	for (i = 0; i < NCPU; i++) {
		mutex_enter(&cpu_core[i].cpuc_pid_lock);
		mutex_exit(&cpu_core[i].cpuc_pid_lock);
	}
}

/*
 * This is the timeout's callback for cleaning up the providers and their
 * probes.
 */
/*ARGSUSED*/
static void
fasttrap_pid_cleanup_cb(void *data)
{
	fasttrap_provider_t **fpp, *fp;
	fasttrap_bucket_t *bucket;
	dtrace_provider_id_t provid;
	int i, later;

	static volatile int in = 0;
	ASSERT(in == 0);
	in = 1;

	mutex_enter(&fasttrap_cleanup_mtx);
	while (fasttrap_cleanup_work) {
		fasttrap_cleanup_work = 0;
		mutex_exit(&fasttrap_cleanup_mtx);

		later = 0;

		/*
		 * Iterate over all the providers trying to remove the marked
		 * ones. If a provider is marked but not retired, we just
		 * have to take a crack at removing it -- it's no big deal if
		 * we can't.
		 */
		for (i = 0; i < fasttrap_provs.fth_nent; i++) {
			bucket = &fasttrap_provs.fth_table[i];
			mutex_enter(&bucket->ftb_mtx);
			fpp = (fasttrap_provider_t **)&bucket->ftb_data;

			while ((fp = *fpp) != NULL) {
				if (!fp->ftp_marked) {
					fpp = &fp->ftp_next;
					continue;
				}

				mutex_enter(&fp->ftp_mtx);

				/*
				 * If this provider has consumers actively
				 * creating probes (ftp_ccount) or is a USDT
				 * provider (ftp_mcount), we can't unregister
				 * or even condense.
				 */
				if (fp->ftp_ccount != 0 ||
				    fp->ftp_mcount != 0) {
					mutex_exit(&fp->ftp_mtx);
					fp->ftp_marked = 0;
					continue;
				}

				if (!fp->ftp_retired || fp->ftp_rcount != 0)
					fp->ftp_marked = 0;

				mutex_exit(&fp->ftp_mtx);

				/*
				 * If we successfully unregister this
				 * provider we can remove it from the hash
				 * chain and free the memory. If our attempt
				 * to unregister fails and this is a retired
				 * provider, increment our flag to try again
				 * pretty soon. If we've consumed more than
				 * half of our total permitted number of
				 * probes call dtrace_condense() to try to
				 * clean out the unenabled probes.
				 */
				provid = fp->ftp_provid;
				if (dtrace_unregister(provid) != 0) {
					if (fasttrap_total > fasttrap_max / 2)
						(void) dtrace_condense(provid);
					later += fp->ftp_marked;
					fpp = &fp->ftp_next;
				} else {
					*fpp = fp->ftp_next;
					fasttrap_provider_free(fp);
				}
			}
			mutex_exit(&bucket->ftb_mtx);
		}

		mutex_enter(&fasttrap_cleanup_mtx);
	}

	ASSERT(fasttrap_timeout != 0);

	/*
	 * If we were unable to remove a retired provider, try again after
	 * a second. This situation can occur in certain circumstances where
	 * providers cannot be unregistered even though they have no probes
	 * enabled because of an execution of dtrace -l or something similar.
	 * If the timeout has been disabled (set to 1 because we're trying
	 * to detach), we set fasttrap_cleanup_work to ensure that we'll
	 * get a chance to do that work if and when the timeout is reenabled
	 * (if detach fails).
	 */
	if (later > 0 && fasttrap_timeout != (timeout_id_t)1)
		fasttrap_timeout = timeout(&fasttrap_pid_cleanup_cb, NULL, hz);
	else if (later > 0)
		fasttrap_cleanup_work = 1;
	else
		fasttrap_timeout = 0;

	mutex_exit(&fasttrap_cleanup_mtx);
	in = 0;
}

/*
 * Activates the asynchronous cleanup mechanism.
 */
static void
fasttrap_pid_cleanup(void)
{
	mutex_enter(&fasttrap_cleanup_mtx);
	fasttrap_cleanup_work = 1;
	if (fasttrap_timeout == 0)
		fasttrap_timeout = timeout(&fasttrap_pid_cleanup_cb, NULL, 1);
	mutex_exit(&fasttrap_cleanup_mtx);
}

/*
 * This is called from cfork() via dtrace_fasttrap_fork(). The child
 * process's address space is (roughly) a copy of the parent process's so
 * we have to remove all the instrumentation we had previously enabled in the
 * parent.
 */
static void
fasttrap_fork(proc_t *p, proc_t *cp)
{
	pid_t ppid = p->p_pid;
	int i;

	ASSERT(curproc == p);
	ASSERT(p->p_proc_flag & P_PR_LOCK);
	ASSERT(p->p_dtrace_count > 0);
	ASSERT(cp->p_dtrace_count == 0);

	/*
	 * This would be simpler and faster if we maintained per-process
	 * hash tables of enabled tracepoints. It could, however, potentially
	 * slow down execution of a tracepoint since we'd need to go
	 * through two levels of indirection. In the future, we should
	 * consider either maintaining per-process ancillary lists of
	 * enabled tracepoints or hanging a pointer to a per-process hash
	 * table of enabled tracepoints off the proc structure.
	 */

	/*
	 * We don't have to worry about the child process disappearing
	 * because we're in fork().
	 */
	mutex_enter(&cp->p_lock);
	sprlock_proc(cp);
	mutex_exit(&cp->p_lock);

	/*
	 * Iterate over every tracepoint looking for ones that belong to the
	 * parent process, and remove each from the child process.
	 */
	for (i = 0; i < fasttrap_tpoints.fth_nent; i++) {
		fasttrap_tracepoint_t *tp;
		fasttrap_bucket_t *bucket = &fasttrap_tpoints.fth_table[i];

		mutex_enter(&bucket->ftb_mtx);
		for (tp = bucket->ftb_data; tp != NULL; tp = tp->ftt_next) {
			if (tp->ftt_pid == ppid &&
			    tp->ftt_proc->ftpc_acount != 0) {
				int ret = fasttrap_tracepoint_remove(cp, tp);
				ASSERT(ret == 0);

				/*
				 * The count of active providers can only be
				 * decremented (i.e. to zero) during exec,
				 * exit, and removal of a meta provider so it
				 * should be impossible to drop the count
				 * mid-fork.
				 */
				ASSERT(tp->ftt_proc->ftpc_acount != 0);
			}
		}
		mutex_exit(&bucket->ftb_mtx);
	}

	mutex_enter(&cp->p_lock);
	sprunlock(cp);
}

/*
 * This is called from proc_exit() or from exec_common() if p_dtrace_probes
 * is set on the proc structure to indicate that there is a pid provider
 * associated with this process.
 */
static void
fasttrap_exec_exit(proc_t *p)
{
	ASSERT(p == curproc);
	ASSERT(MUTEX_HELD(&p->p_lock));

	mutex_exit(&p->p_lock);

	/*
	 * We clean up the pid provider for this process here; user-land
	 * static probes are handled by the meta-provider remove entry point.
	 */
	fasttrap_provider_retire(p->p_pid, FASTTRAP_PID_NAME, 0);

	mutex_enter(&p->p_lock);
}


/*ARGSUSED*/
static void
fasttrap_pid_provide(void *arg, const dtrace_probedesc_t *desc)
{
	/*
	 * There are no "default" pid probes.
	 */
}

static int
fasttrap_tracepoint_enable(proc_t *p, fasttrap_probe_t *probe, uint_t index)
{
	fasttrap_tracepoint_t *tp, *new_tp = NULL;
	fasttrap_bucket_t *bucket;
	fasttrap_id_t *id;
	pid_t pid;
	uintptr_t pc;

	ASSERT(index < probe->ftp_ntps);

	pid = probe->ftp_pid;
	pc = probe->ftp_tps[index].fit_tp->ftt_pc;
	id = &probe->ftp_tps[index].fit_id;

	ASSERT(probe->ftp_tps[index].fit_tp->ftt_pid == pid);

	ASSERT(!(p->p_flag & SVFORK));

	/*
	 * Before we make any modifications, make sure we've imposed a barrier
	 * on the generation in which this probe was last modified.
	 */
	fasttrap_mod_barrier(probe->ftp_gen);

	bucket = &fasttrap_tpoints.fth_table[FASTTRAP_TPOINTS_INDEX(pid, pc)];

	/*
	 * If the tracepoint has already been enabled, just add our id to the
	 * list of interested probes. This may be our second time through
	 * this path in which case we'll have constructed the tracepoint we'd
	 * like to install. If we can't find a match, and have an allocated
	 * tracepoint ready to go, enable that one now.
	 *
	 * A tracepoint whose process is defunct is also considered defunct.
	 */
again:
	mutex_enter(&bucket->ftb_mtx);
	for (tp = bucket->ftb_data; tp != NULL; tp = tp->ftt_next) {
		/*
		 * Note that it's safe to access the active count on the
		 * associated proc structure because we know that at least one
		 * provider (this one) will still be around throughout this
		 * operation.
		 */
		if (tp->ftt_pid != pid || tp->ftt_pc != pc ||
		    tp->ftt_proc->ftpc_acount == 0)
			continue;

		/*
		 * Now that we've found a matching tracepoint, it would be
		 * a decent idea to confirm that the tracepoint is still
		 * enabled and the trap instruction hasn't been overwritten.
		 * Since this is a little hairy, we'll punt for now.
		 */

		/*
		 * This can't be the first interested probe. We don't have
		 * to worry about another thread being in the midst of
		 * deleting this tracepoint (which would be the only valid
		 * reason for a tracepoint to have no interested probes)
		 * since we're holding P_PR_LOCK for this process.
		 */
		ASSERT(tp->ftt_ids != NULL || tp->ftt_retids != NULL);

		switch (id->fti_ptype) {
		case DTFTP_ENTRY:
		case DTFTP_OFFSETS:
		case DTFTP_IS_ENABLED:
			id->fti_next = tp->ftt_ids;
			membar_producer();
			tp->ftt_ids = id;
			membar_producer();
			break;

		case DTFTP_RETURN:
		case DTFTP_POST_OFFSETS:
			id->fti_next = tp->ftt_retids;
			membar_producer();
			tp->ftt_retids = id;
			membar_producer();
			break;

		default:
			ASSERT(0);
		}

		mutex_exit(&bucket->ftb_mtx);

		if (new_tp != NULL) {
			new_tp->ftt_ids = NULL;
			new_tp->ftt_retids = NULL;
		}

		return (0);
	}

	/*
	 * If we have a good tracepoint ready to go, install it now while
	 * we have the lock held and no one can screw with us.
	 */
	if (new_tp != NULL) {
		int rc = 0;

		new_tp->ftt_next = bucket->ftb_data;
		membar_producer();
		bucket->ftb_data = new_tp;
		membar_producer();
		mutex_exit(&bucket->ftb_mtx);

		/*
		 * Activate the tracepoint in the ISA-specific manner.
		 * If this fails, we need to report the failure, but
		 * indicate that this tracepoint must still be disabled
		 * by calling fasttrap_tracepoint_disable().
		 */
		if (fasttrap_tracepoint_install(p, new_tp) != 0)
			rc = FASTTRAP_ENABLE_PARTIAL;

		/*
		 * Increment the count of the number of tracepoints active in
		 * the victim process.
		 */
		ASSERT(p->p_proc_flag & P_PR_LOCK);
		p->p_dtrace_count++;

		return (rc);
	}

	mutex_exit(&bucket->ftb_mtx);

	/*
	 * Initialize the tracepoint that's been preallocated with the probe.
	 */
	new_tp = probe->ftp_tps[index].fit_tp;

	ASSERT(new_tp->ftt_pid == pid);
	ASSERT(new_tp->ftt_pc == pc);
	ASSERT(new_tp->ftt_proc == probe->ftp_prov->ftp_proc);
	ASSERT(new_tp->ftt_ids == NULL);
	ASSERT(new_tp->ftt_retids == NULL);

	switch (id->fti_ptype) {
	case DTFTP_ENTRY:
	case DTFTP_OFFSETS:
	case DTFTP_IS_ENABLED:
		id->fti_next = NULL;
		new_tp->ftt_ids = id;
		break;

	case DTFTP_RETURN:
	case DTFTP_POST_OFFSETS:
		id->fti_next = NULL;
		new_tp->ftt_retids = id;
		break;

	default:
		ASSERT(0);
	}

	/*
	 * If the ISA-dependent initialization goes to plan, go back to the
	 * beginning and try to install this freshly made tracepoint.
	 */
	if (fasttrap_tracepoint_init(p, new_tp, pc, id->fti_ptype) == 0)
		goto again;

	new_tp->ftt_ids = NULL;
	new_tp->ftt_retids = NULL;

	return (FASTTRAP_ENABLE_FAIL);
}

static void
fasttrap_tracepoint_disable(proc_t *p, fasttrap_probe_t *probe, uint_t index)
{
	fasttrap_bucket_t *bucket;
	fasttrap_provider_t *provider = probe->ftp_prov;
	fasttrap_tracepoint_t **pp, *tp;
	fasttrap_id_t *id, **idp;
	pid_t pid;
	uintptr_t pc;

	ASSERT(index < probe->ftp_ntps);

	pid = probe->ftp_pid;
	pc = probe->ftp_tps[index].fit_tp->ftt_pc;
	id = &probe->ftp_tps[index].fit_id;

	ASSERT(probe->ftp_tps[index].fit_tp->ftt_pid == pid);

	/*
	 * Find the tracepoint and make sure that our id is one of the
	 * ones registered with it.
	 */
	bucket = &fasttrap_tpoints.fth_table[FASTTRAP_TPOINTS_INDEX(pid, pc)];
	mutex_enter(&bucket->ftb_mtx);
	for (tp = bucket->ftb_data; tp != NULL; tp = tp->ftt_next) {
		if (tp->ftt_pid == pid && tp->ftt_pc == pc &&
		    tp->ftt_proc == provider->ftp_proc)
			break;
	}

	/*
	 * If we somehow lost this tracepoint, we're in a world of hurt.
	 */
	ASSERT(tp != NULL);

	switch (id->fti_ptype) {
	case DTFTP_ENTRY:
	case DTFTP_OFFSETS:
	case DTFTP_IS_ENABLED:
		ASSERT(tp->ftt_ids != NULL);
		idp = &tp->ftt_ids;
		break;

	case DTFTP_RETURN:
	case DTFTP_POST_OFFSETS:
		ASSERT(tp->ftt_retids != NULL);
		idp = &tp->ftt_retids;
		break;

	default:
		ASSERT(0);
	}

	while ((*idp)->fti_probe != probe) {
		idp = &(*idp)->fti_next;
		ASSERT(*idp != NULL);
	}

	id = *idp;
	*idp = id->fti_next;
	membar_producer();

	ASSERT(id->fti_probe == probe);

	/*
	 * If there are other registered enablings of this tracepoint, we're
	 * all done, but if this was the last probe assocated with this
	 * this tracepoint, we need to remove and free it.
	 */
	if (tp->ftt_ids != NULL || tp->ftt_retids != NULL) {

		/*
		 * If the current probe's tracepoint is in use, swap it
		 * for an unused tracepoint.
		 */
		if (tp == probe->ftp_tps[index].fit_tp) {
			fasttrap_probe_t *tmp_probe;
			fasttrap_tracepoint_t **tmp_tp;
			uint_t tmp_index;

			if (tp->ftt_ids != NULL) {
				tmp_probe = tp->ftt_ids->fti_probe;
				/* LINTED - alignment */
				tmp_index = FASTTRAP_ID_INDEX(tp->ftt_ids);
				tmp_tp = &tmp_probe->ftp_tps[tmp_index].fit_tp;
			} else {
				tmp_probe = tp->ftt_retids->fti_probe;
				/* LINTED - alignment */
				tmp_index = FASTTRAP_ID_INDEX(tp->ftt_retids);
				tmp_tp = &tmp_probe->ftp_tps[tmp_index].fit_tp;
			}

			ASSERT(*tmp_tp != NULL);
			ASSERT(*tmp_tp != probe->ftp_tps[index].fit_tp);
			ASSERT((*tmp_tp)->ftt_ids == NULL);
			ASSERT((*tmp_tp)->ftt_retids == NULL);

			probe->ftp_tps[index].fit_tp = *tmp_tp;
			*tmp_tp = tp;
		}

		mutex_exit(&bucket->ftb_mtx);

		/*
		 * Tag the modified probe with the generation in which it was
		 * changed.
		 */
		probe->ftp_gen = fasttrap_mod_gen;
		return;
	}

	mutex_exit(&bucket->ftb_mtx);

	/*
	 * We can't safely remove the tracepoint from the set of active
	 * tracepoints until we've actually removed the fasttrap instruction
	 * from the process's text. We can, however, operate on this
	 * tracepoint secure in the knowledge that no other thread is going to
	 * be looking at it since we hold P_PR_LOCK on the process if it's
	 * live or we hold the provider lock on the process if it's dead and
	 * gone.
	 */

	/*
	 * We only need to remove the actual instruction if we're looking
	 * at an existing process
	 */
	if (p != NULL) {
		/*
		 * If we fail to restore the instruction we need to kill
		 * this process since it's in a completely unrecoverable
		 * state.
		 */
		if (fasttrap_tracepoint_remove(p, tp) != 0)
			fasttrap_sigtrap(p, NULL, pc);

		/*
		 * Decrement the count of the number of tracepoints active
		 * in the victim process.
		 */
		ASSERT(p->p_proc_flag & P_PR_LOCK);
		p->p_dtrace_count--;
	}

	/*
	 * Remove the probe from the hash table of active tracepoints.
	 */
	mutex_enter(&bucket->ftb_mtx);
	pp = (fasttrap_tracepoint_t **)&bucket->ftb_data;
	ASSERT(*pp != NULL);
	while (*pp != tp) {
		pp = &(*pp)->ftt_next;
		ASSERT(*pp != NULL);
	}

	*pp = tp->ftt_next;
	membar_producer();

	mutex_exit(&bucket->ftb_mtx);

	/*
	 * Tag the modified probe with the generation in which it was changed.
	 */
	probe->ftp_gen = fasttrap_mod_gen;
}

static void
fasttrap_enable_callbacks(void)
{
	/*
	 * We don't have to play the rw lock game here because we're
	 * providing something rather than taking something away --
	 * we can be sure that no threads have tried to follow this
	 * function pointer yet.
	 */
	mutex_enter(&fasttrap_count_mtx);
	if (fasttrap_pid_count == 0) {
		ASSERT(dtrace_pid_probe_ptr == NULL);
		ASSERT(dtrace_return_probe_ptr == NULL);
		dtrace_pid_probe_ptr = &fasttrap_pid_probe;
		dtrace_return_probe_ptr = &fasttrap_return_probe;
	}
	ASSERT(dtrace_pid_probe_ptr == &fasttrap_pid_probe);
	ASSERT(dtrace_return_probe_ptr == &fasttrap_return_probe);
	fasttrap_pid_count++;
	mutex_exit(&fasttrap_count_mtx);
}

static void
fasttrap_disable_callbacks(void)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	mutex_enter(&fasttrap_count_mtx);
	ASSERT(fasttrap_pid_count > 0);
	fasttrap_pid_count--;
	if (fasttrap_pid_count == 0) {
		cpu_t *cur, *cpu = CPU;

		for (cur = cpu->cpu_next_onln; cur != cpu;
		    cur = cur->cpu_next_onln) {
			rw_enter(&cur->cpu_ft_lock, RW_WRITER);
		}

		dtrace_pid_probe_ptr = NULL;
		dtrace_return_probe_ptr = NULL;

		for (cur = cpu->cpu_next_onln; cur != cpu;
		    cur = cur->cpu_next_onln) {
			rw_exit(&cur->cpu_ft_lock);
		}
	}
	mutex_exit(&fasttrap_count_mtx);
}

/*ARGSUSED*/
static void
fasttrap_pid_enable(void *arg, dtrace_id_t id, void *parg)
{
	fasttrap_probe_t *probe = parg;
	proc_t *p;
	int i, rc;

	ASSERT(probe != NULL);
	ASSERT(!probe->ftp_enabled);
	ASSERT(id == probe->ftp_id);
	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Increment the count of enabled probes on this probe's provider;
	 * the provider can't go away while the probe still exists. We
	 * must increment this even if we aren't able to properly enable
	 * this probe.
	 */
	mutex_enter(&probe->ftp_prov->ftp_mtx);
	probe->ftp_prov->ftp_rcount++;
	mutex_exit(&probe->ftp_prov->ftp_mtx);

	/*
	 * If this probe's provider is retired (meaning it was valid in a
	 * previously exec'ed incarnation of this address space), bail out. The
	 * provider can't go away while we're in this code path.
	 */
	if (probe->ftp_prov->ftp_retired)
		return;

	/*
	 * If we can't find the process, it may be that we're in the context of
	 * a fork in which the traced process is being born and we're copying
	 * USDT probes. Otherwise, the process is gone so bail.
	 */
	if ((p = sprlock(probe->ftp_pid)) == NULL) {
		if ((curproc->p_flag & SFORKING) == 0)
			return;

		mutex_enter(&pidlock);
		p = prfind(probe->ftp_pid);

		/*
		 * Confirm that curproc is indeed forking the process in which
		 * we're trying to enable probes.
		 */
		ASSERT(p != NULL);
		ASSERT(p->p_parent == curproc);
		ASSERT(p->p_stat == SIDL);

		mutex_enter(&p->p_lock);
		mutex_exit(&pidlock);

		sprlock_proc(p);
	}

	ASSERT(!(p->p_flag & SVFORK));
	mutex_exit(&p->p_lock);

	/*
	 * We have to enable the trap entry point before any user threads have
	 * the chance to execute the trap instruction we're about to place
	 * in their process's text.
	 */
	fasttrap_enable_callbacks();

	/*
	 * Enable all the tracepoints and add this probe's id to each
	 * tracepoint's list of active probes.
	 */
	for (i = 0; i < probe->ftp_ntps; i++) {
		if ((rc = fasttrap_tracepoint_enable(p, probe, i)) != 0) {
			/*
			 * If enabling the tracepoint failed completely,
			 * we don't have to disable it; if the failure
			 * was only partial we must disable it.
			 */
			if (rc == FASTTRAP_ENABLE_FAIL)
				i--;
			else
				ASSERT(rc == FASTTRAP_ENABLE_PARTIAL);

			/*
			 * Back up and pull out all the tracepoints we've
			 * created so far for this probe.
			 */
			while (i >= 0) {
				fasttrap_tracepoint_disable(p, probe, i);
				i--;
			}

			mutex_enter(&p->p_lock);
			sprunlock(p);

			/*
			 * Since we're not actually enabling this probe,
			 * drop our reference on the trap table entry.
			 */
			fasttrap_disable_callbacks();
			return;
		}
	}

	mutex_enter(&p->p_lock);
	sprunlock(p);

	probe->ftp_enabled = 1;
}

/*ARGSUSED*/
static void
fasttrap_pid_disable(void *arg, dtrace_id_t id, void *parg)
{
	fasttrap_probe_t *probe = parg;
	fasttrap_provider_t *provider = probe->ftp_prov;
	proc_t *p;
	int i, whack = 0;

	ASSERT(id == probe->ftp_id);

	/*
	 * We won't be able to acquire a /proc-esque lock on the process
	 * iff the process is dead and gone. In this case, we rely on the
	 * provider lock as a point of mutual exclusion to prevent other
	 * DTrace consumers from disabling this probe.
	 */
	if ((p = sprlock(probe->ftp_pid)) != NULL) {
		ASSERT(!(p->p_flag & SVFORK));
		mutex_exit(&p->p_lock);
	}

	mutex_enter(&provider->ftp_mtx);

	/*
	 * Disable all the associated tracepoints (for fully enabled probes).
	 */
	if (probe->ftp_enabled) {
		for (i = 0; i < probe->ftp_ntps; i++) {
			fasttrap_tracepoint_disable(p, probe, i);
		}
	}

	ASSERT(provider->ftp_rcount > 0);
	provider->ftp_rcount--;

	if (p != NULL) {
		/*
		 * Even though we may not be able to remove it entirely, we
		 * mark this retired provider to get a chance to remove some
		 * of the associated probes.
		 */
		if (provider->ftp_retired && !provider->ftp_marked)
			whack = provider->ftp_marked = 1;
		mutex_exit(&provider->ftp_mtx);

		mutex_enter(&p->p_lock);
		sprunlock(p);
	} else {
		/*
		 * If the process is dead, we're just waiting for the
		 * last probe to be disabled to be able to free it.
		 */
		if (provider->ftp_rcount == 0 && !provider->ftp_marked)
			whack = provider->ftp_marked = 1;
		mutex_exit(&provider->ftp_mtx);
	}

	if (whack)
		fasttrap_pid_cleanup();

	if (!probe->ftp_enabled)
		return;

	probe->ftp_enabled = 0;

	ASSERT(MUTEX_HELD(&cpu_lock));
	fasttrap_disable_callbacks();
}

/*ARGSUSED*/
static void
fasttrap_pid_getargdesc(void *arg, dtrace_id_t id, void *parg,
    dtrace_argdesc_t *desc)
{
	fasttrap_probe_t *probe = parg;
	char *str;
	int i, ndx;

	desc->dtargd_native[0] = '\0';
	desc->dtargd_xlate[0] = '\0';

	if (probe->ftp_prov->ftp_retired != 0 ||
	    desc->dtargd_ndx >= probe->ftp_nargs) {
		desc->dtargd_ndx = DTRACE_ARGNONE;
		return;
	}

	ndx = (probe->ftp_argmap != NULL) ?
	    probe->ftp_argmap[desc->dtargd_ndx] : desc->dtargd_ndx;

	str = probe->ftp_ntypes;
	for (i = 0; i < ndx; i++) {
		str += strlen(str) + 1;
	}

	ASSERT(strlen(str + 1) < sizeof (desc->dtargd_native));
	(void) strcpy(desc->dtargd_native, str);

	if (probe->ftp_xtypes == NULL)
		return;

	str = probe->ftp_xtypes;
	for (i = 0; i < desc->dtargd_ndx; i++) {
		str += strlen(str) + 1;
	}

	ASSERT(strlen(str + 1) < sizeof (desc->dtargd_xlate));
	(void) strcpy(desc->dtargd_xlate, str);
}

/*ARGSUSED*/
static void
fasttrap_pid_destroy(void *arg, dtrace_id_t id, void *parg)
{
	fasttrap_probe_t *probe = parg;
	int i;
	size_t size;

	ASSERT(probe != NULL);
	ASSERT(!probe->ftp_enabled);
	ASSERT(fasttrap_total >= probe->ftp_ntps);

	atomic_add_32(&fasttrap_total, -probe->ftp_ntps);
	size = offsetof(fasttrap_probe_t, ftp_tps[probe->ftp_ntps]);

	if (probe->ftp_gen + 1 >= fasttrap_mod_gen)
		fasttrap_mod_barrier(probe->ftp_gen);

	for (i = 0; i < probe->ftp_ntps; i++) {
		kmem_free(probe->ftp_tps[i].fit_tp,
		    sizeof (fasttrap_tracepoint_t));
	}

	kmem_free(probe, size);
}


static const dtrace_pattr_t pid_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
};

static dtrace_pops_t pid_pops = {
	fasttrap_pid_provide,
	NULL,
	fasttrap_pid_enable,
	fasttrap_pid_disable,
	NULL,
	NULL,
	fasttrap_pid_getargdesc,
	fasttrap_pid_getarg,
	NULL,
	fasttrap_pid_destroy
};

static dtrace_pops_t usdt_pops = {
	fasttrap_pid_provide,
	NULL,
	fasttrap_pid_enable,
	fasttrap_pid_disable,
	NULL,
	NULL,
	fasttrap_pid_getargdesc,
	fasttrap_usdt_getarg,
	NULL,
	fasttrap_pid_destroy
};

static fasttrap_proc_t *
fasttrap_proc_lookup(pid_t pid)
{
	fasttrap_bucket_t *bucket;
	fasttrap_proc_t *fprc, *new_fprc;

	bucket = &fasttrap_procs.fth_table[FASTTRAP_PROCS_INDEX(pid)];
	mutex_enter(&bucket->ftb_mtx);

	for (fprc = bucket->ftb_data; fprc != NULL; fprc = fprc->ftpc_next) {
		if (fprc->ftpc_pid == pid && fprc->ftpc_acount != 0) {
			mutex_enter(&fprc->ftpc_mtx);
			mutex_exit(&bucket->ftb_mtx);
			fprc->ftpc_rcount++;
			atomic_add_64(&fprc->ftpc_acount, 1);
			ASSERT(fprc->ftpc_acount <= fprc->ftpc_rcount);
			mutex_exit(&fprc->ftpc_mtx);

			return (fprc);
		}
	}

	/*
	 * Drop the bucket lock so we don't try to perform a sleeping
	 * allocation under it.
	 */
	mutex_exit(&bucket->ftb_mtx);

	new_fprc = kmem_zalloc(sizeof (fasttrap_proc_t), KM_SLEEP);
	new_fprc->ftpc_pid = pid;
	new_fprc->ftpc_rcount = 1;
	new_fprc->ftpc_acount = 1;

	mutex_enter(&bucket->ftb_mtx);

	/*
	 * Take another lap through the list to make sure a proc hasn't
	 * been created for this pid while we weren't under the bucket lock.
	 */
	for (fprc = bucket->ftb_data; fprc != NULL; fprc = fprc->ftpc_next) {
		if (fprc->ftpc_pid == pid && fprc->ftpc_acount != 0) {
			mutex_enter(&fprc->ftpc_mtx);
			mutex_exit(&bucket->ftb_mtx);
			fprc->ftpc_rcount++;
			atomic_add_64(&fprc->ftpc_acount, 1);
			ASSERT(fprc->ftpc_acount <= fprc->ftpc_rcount);
			mutex_exit(&fprc->ftpc_mtx);

			kmem_free(new_fprc, sizeof (fasttrap_proc_t));

			return (fprc);
		}
	}

	new_fprc->ftpc_next = bucket->ftb_data;
	bucket->ftb_data = new_fprc;

	mutex_exit(&bucket->ftb_mtx);

	return (new_fprc);
}

static void
fasttrap_proc_release(fasttrap_proc_t *proc)
{
	fasttrap_bucket_t *bucket;
	fasttrap_proc_t *fprc, **fprcp;
	pid_t pid = proc->ftpc_pid;

	mutex_enter(&proc->ftpc_mtx);

	ASSERT(proc->ftpc_rcount != 0);
	ASSERT(proc->ftpc_acount <= proc->ftpc_rcount);

	if (--proc->ftpc_rcount != 0) {
		mutex_exit(&proc->ftpc_mtx);
		return;
	}

	mutex_exit(&proc->ftpc_mtx);

	/*
	 * There should definitely be no live providers associated with this
	 * process at this point.
	 */
	ASSERT(proc->ftpc_acount == 0);

	bucket = &fasttrap_procs.fth_table[FASTTRAP_PROCS_INDEX(pid)];
	mutex_enter(&bucket->ftb_mtx);

	fprcp = (fasttrap_proc_t **)&bucket->ftb_data;
	while ((fprc = *fprcp) != NULL) {
		if (fprc == proc)
			break;

		fprcp = &fprc->ftpc_next;
	}

	/*
	 * Something strange has happened if we can't find the proc.
	 */
	ASSERT(fprc != NULL);

	*fprcp = fprc->ftpc_next;

	mutex_exit(&bucket->ftb_mtx);

	kmem_free(fprc, sizeof (fasttrap_proc_t));
}

/*
 * Lookup a fasttrap-managed provider based on its name and associated pid.
 * If the pattr argument is non-NULL, this function instantiates the provider
 * if it doesn't exist otherwise it returns NULL. The provider is returned
 * with its lock held.
 */
static fasttrap_provider_t *
fasttrap_provider_lookup(pid_t pid, const char *name,
    const dtrace_pattr_t *pattr)
{
	fasttrap_provider_t *fp, *new_fp = NULL;
	fasttrap_bucket_t *bucket;
	char provname[DTRACE_PROVNAMELEN];
	proc_t *p;
	cred_t *cred;

	ASSERT(strlen(name) < sizeof (fp->ftp_name));
	ASSERT(pattr != NULL);

	bucket = &fasttrap_provs.fth_table[FASTTRAP_PROVS_INDEX(pid, name)];
	mutex_enter(&bucket->ftb_mtx);

	/*
	 * Take a lap through the list and return the match if we find it.
	 */
	for (fp = bucket->ftb_data; fp != NULL; fp = fp->ftp_next) {
		if (fp->ftp_pid == pid && strcmp(fp->ftp_name, name) == 0 &&
		    !fp->ftp_retired) {
			mutex_enter(&fp->ftp_mtx);
			mutex_exit(&bucket->ftb_mtx);
			return (fp);
		}
	}

	/*
	 * Drop the bucket lock so we don't try to perform a sleeping
	 * allocation under it.
	 */
	mutex_exit(&bucket->ftb_mtx);

	/*
	 * Make sure the process exists, isn't a child created as the result
	 * of a vfork(2), and isn't a zombie (but may be in fork).
	 */
	mutex_enter(&pidlock);
	if ((p = prfind(pid)) == NULL) {
		mutex_exit(&pidlock);
		return (NULL);
	}
	mutex_enter(&p->p_lock);
	mutex_exit(&pidlock);
	if (p->p_flag & (SVFORK | SEXITING)) {
		mutex_exit(&p->p_lock);
		return (NULL);
	}

	/*
	 * Increment p_dtrace_probes so that the process knows to inform us
	 * when it exits or execs. fasttrap_provider_free() decrements this
	 * when we're done with this provider.
	 */
	p->p_dtrace_probes++;

	/*
	 * Grab the credentials for this process so we have
	 * something to pass to dtrace_register().
	 */
	mutex_enter(&p->p_crlock);
	crhold(p->p_cred);
	cred = p->p_cred;
	mutex_exit(&p->p_crlock);
	mutex_exit(&p->p_lock);

	new_fp = kmem_zalloc(sizeof (fasttrap_provider_t), KM_SLEEP);
	new_fp->ftp_pid = pid;
	new_fp->ftp_proc = fasttrap_proc_lookup(pid);

	ASSERT(new_fp->ftp_proc != NULL);

	mutex_enter(&bucket->ftb_mtx);

	/*
	 * Take another lap through the list to make sure a provider hasn't
	 * been created for this pid while we weren't under the bucket lock.
	 */
	for (fp = bucket->ftb_data; fp != NULL; fp = fp->ftp_next) {
		if (fp->ftp_pid == pid && strcmp(fp->ftp_name, name) == 0 &&
		    !fp->ftp_retired) {
			mutex_enter(&fp->ftp_mtx);
			mutex_exit(&bucket->ftb_mtx);
			fasttrap_provider_free(new_fp);
			crfree(cred);
			return (fp);
		}
	}

	(void) strcpy(new_fp->ftp_name, name);

	/*
	 * Fail and return NULL if either the provider name is too long
	 * or we fail to register this new provider with the DTrace
	 * framework. Note that this is the only place we ever construct
	 * the full provider name -- we keep it in pieces in the provider
	 * structure.
	 */
	if (snprintf(provname, sizeof (provname), "%s%u", name, (uint_t)pid) >=
	    sizeof (provname) ||
	    dtrace_register(provname, pattr,
	    DTRACE_PRIV_PROC | DTRACE_PRIV_OWNER | DTRACE_PRIV_ZONEOWNER, cred,
	    pattr == &pid_attr ? &pid_pops : &usdt_pops, new_fp,
	    &new_fp->ftp_provid) != 0) {
		mutex_exit(&bucket->ftb_mtx);
		fasttrap_provider_free(new_fp);
		crfree(cred);
		return (NULL);
	}

	new_fp->ftp_next = bucket->ftb_data;
	bucket->ftb_data = new_fp;

	mutex_enter(&new_fp->ftp_mtx);
	mutex_exit(&bucket->ftb_mtx);

	crfree(cred);
	return (new_fp);
}

static void
fasttrap_provider_free(fasttrap_provider_t *provider)
{
	pid_t pid = provider->ftp_pid;
	proc_t *p;

	/*
	 * There need to be no associated enabled probes, no consumers
	 * creating probes, and no meta providers referencing this provider.
	 */
	ASSERT(provider->ftp_rcount == 0);
	ASSERT(provider->ftp_ccount == 0);
	ASSERT(provider->ftp_mcount == 0);

	/*
	 * If this provider hasn't been retired, we need to explicitly drop the
	 * count of active providers on the associated process structure.
	 */
	if (!provider->ftp_retired) {
		atomic_add_64(&provider->ftp_proc->ftpc_acount, -1);
		ASSERT(provider->ftp_proc->ftpc_acount <
		    provider->ftp_proc->ftpc_rcount);
	}

	fasttrap_proc_release(provider->ftp_proc);

	kmem_free(provider, sizeof (fasttrap_provider_t));

	/*
	 * Decrement p_dtrace_probes on the process whose provider we're
	 * freeing. We don't have to worry about clobbering somone else's
	 * modifications to it because we have locked the bucket that
	 * corresponds to this process's hash chain in the provider hash
	 * table. Don't sweat it if we can't find the process.
	 */
	mutex_enter(&pidlock);
	if ((p = prfind(pid)) == NULL) {
		mutex_exit(&pidlock);
		return;
	}

	mutex_enter(&p->p_lock);
	mutex_exit(&pidlock);

	p->p_dtrace_probes--;
	mutex_exit(&p->p_lock);
}

static void
fasttrap_provider_retire(pid_t pid, const char *name, int mprov)
{
	fasttrap_provider_t *fp;
	fasttrap_bucket_t *bucket;
	dtrace_provider_id_t provid;

	ASSERT(strlen(name) < sizeof (fp->ftp_name));

	bucket = &fasttrap_provs.fth_table[FASTTRAP_PROVS_INDEX(pid, name)];
	mutex_enter(&bucket->ftb_mtx);

	for (fp = bucket->ftb_data; fp != NULL; fp = fp->ftp_next) {
		if (fp->ftp_pid == pid && strcmp(fp->ftp_name, name) == 0 &&
		    !fp->ftp_retired)
			break;
	}

	if (fp == NULL) {
		mutex_exit(&bucket->ftb_mtx);
		return;
	}

	mutex_enter(&fp->ftp_mtx);
	ASSERT(!mprov || fp->ftp_mcount > 0);
	if (mprov && --fp->ftp_mcount != 0)  {
		mutex_exit(&fp->ftp_mtx);
		mutex_exit(&bucket->ftb_mtx);
		return;
	}

	/*
	 * Mark the provider to be removed in our post-processing step, mark it
	 * retired, and drop the active count on its proc. Marking it indicates
	 * that we should try to remove it; setting the retired flag indicates
	 * that we're done with this provider; dropping the active the proc
	 * releases our hold, and when this reaches zero (as it will during
	 * exit or exec) the proc and associated providers become defunct.
	 *
	 * We obviously need to take the bucket lock before the provider lock
	 * to perform the lookup, but we need to drop the provider lock
	 * before calling into the DTrace framework since we acquire the
	 * provider lock in callbacks invoked from the DTrace framework. The
	 * bucket lock therefore protects the integrity of the provider hash
	 * table.
	 */
	atomic_add_64(&fp->ftp_proc->ftpc_acount, -1);
	ASSERT(fp->ftp_proc->ftpc_acount < fp->ftp_proc->ftpc_rcount);

	fp->ftp_retired = 1;
	fp->ftp_marked = 1;
	provid = fp->ftp_provid;
	mutex_exit(&fp->ftp_mtx);

	/*
	 * We don't have to worry about invalidating the same provider twice
	 * since fasttrap_provider_lookup() will ignore provider that have
	 * been marked as retired.
	 */
	dtrace_invalidate(provid);

	mutex_exit(&bucket->ftb_mtx);

	fasttrap_pid_cleanup();
}

static int
fasttrap_uint32_cmp(const void *ap, const void *bp)
{
	return (*(const uint32_t *)ap - *(const uint32_t *)bp);
}

static int
fasttrap_uint64_cmp(const void *ap, const void *bp)
{
	return (*(const uint64_t *)ap - *(const uint64_t *)bp);
}

static int
fasttrap_add_probe(fasttrap_probe_spec_t *pdata)
{
	fasttrap_provider_t *provider;
	fasttrap_probe_t *pp;
	fasttrap_tracepoint_t *tp;
	char *name;
	int i, aframes, whack;

	/*
	 * There needs to be at least one desired trace point.
	 */
	if (pdata->ftps_noffs == 0)
		return (EINVAL);

	switch (pdata->ftps_type) {
	case DTFTP_ENTRY:
		name = "entry";
		aframes = FASTTRAP_ENTRY_AFRAMES;
		break;
	case DTFTP_RETURN:
		name = "return";
		aframes = FASTTRAP_RETURN_AFRAMES;
		break;
	case DTFTP_OFFSETS:
		name = NULL;
		break;
	default:
		return (EINVAL);
	}

	if ((provider = fasttrap_provider_lookup(pdata->ftps_pid,
	    FASTTRAP_PID_NAME, &pid_attr)) == NULL)
		return (ESRCH);

	/*
	 * Increment this reference count to indicate that a consumer is
	 * actively adding a new probe associated with this provider. This
	 * prevents the provider from being deleted -- we'll need to check
	 * for pending deletions when we drop this reference count.
	 */
	provider->ftp_ccount++;
	mutex_exit(&provider->ftp_mtx);

	/*
	 * Grab the creation lock to ensure consistency between calls to
	 * dtrace_probe_lookup() and dtrace_probe_create() in the face of
	 * other threads creating probes. We must drop the provider lock
	 * before taking this lock to avoid a three-way deadlock with the
	 * DTrace framework.
	 */
	mutex_enter(&provider->ftp_cmtx);

	if (name == NULL) {
		for (i = 0; i < pdata->ftps_noffs; i++) {
			char name_str[17];

			(void) sprintf(name_str, "%llx",
			    (unsigned long long)pdata->ftps_offs[i]);

			if (dtrace_probe_lookup(provider->ftp_provid,
			    pdata->ftps_mod, pdata->ftps_func, name_str) != 0)
				continue;

			atomic_add_32(&fasttrap_total, 1);

			if (fasttrap_total > fasttrap_max) {
				atomic_add_32(&fasttrap_total, -1);
				goto no_mem;
			}

			pp = kmem_zalloc(sizeof (fasttrap_probe_t), KM_SLEEP);

			pp->ftp_prov = provider;
			pp->ftp_faddr = pdata->ftps_pc;
			pp->ftp_fsize = pdata->ftps_size;
			pp->ftp_pid = pdata->ftps_pid;
			pp->ftp_ntps = 1;

			tp = kmem_zalloc(sizeof (fasttrap_tracepoint_t),
			    KM_SLEEP);

			tp->ftt_proc = provider->ftp_proc;
			tp->ftt_pc = pdata->ftps_offs[i] + pdata->ftps_pc;
			tp->ftt_pid = pdata->ftps_pid;

			pp->ftp_tps[0].fit_tp = tp;
			pp->ftp_tps[0].fit_id.fti_probe = pp;
			pp->ftp_tps[0].fit_id.fti_ptype = pdata->ftps_type;

			pp->ftp_id = dtrace_probe_create(provider->ftp_provid,
			    pdata->ftps_mod, pdata->ftps_func, name_str,
			    FASTTRAP_OFFSET_AFRAMES, pp);
		}

	} else if (dtrace_probe_lookup(provider->ftp_provid, pdata->ftps_mod,
	    pdata->ftps_func, name) == 0) {
		atomic_add_32(&fasttrap_total, pdata->ftps_noffs);

		if (fasttrap_total > fasttrap_max) {
			atomic_add_32(&fasttrap_total, -pdata->ftps_noffs);
			goto no_mem;
		}

		/*
		 * Make sure all tracepoint program counter values are unique.
		 * We later assume that each probe has exactly one tracepoint
		 * for a given pc.
		 */
		qsort(pdata->ftps_offs, pdata->ftps_noffs,
		    sizeof (uint64_t), fasttrap_uint64_cmp);
		for (i = 1; i < pdata->ftps_noffs; i++) {
			if (pdata->ftps_offs[i] > pdata->ftps_offs[i - 1])
				continue;

			atomic_add_32(&fasttrap_total, -pdata->ftps_noffs);
			goto no_mem;
		}

		ASSERT(pdata->ftps_noffs > 0);
		pp = kmem_zalloc(offsetof(fasttrap_probe_t,
		    ftp_tps[pdata->ftps_noffs]), KM_SLEEP);

		pp->ftp_prov = provider;
		pp->ftp_faddr = pdata->ftps_pc;
		pp->ftp_fsize = pdata->ftps_size;
		pp->ftp_pid = pdata->ftps_pid;
		pp->ftp_ntps = pdata->ftps_noffs;

		for (i = 0; i < pdata->ftps_noffs; i++) {
			tp = kmem_zalloc(sizeof (fasttrap_tracepoint_t),
			    KM_SLEEP);

			tp->ftt_proc = provider->ftp_proc;
			tp->ftt_pc = pdata->ftps_offs[i] + pdata->ftps_pc;
			tp->ftt_pid = pdata->ftps_pid;

			pp->ftp_tps[i].fit_tp = tp;
			pp->ftp_tps[i].fit_id.fti_probe = pp;
			pp->ftp_tps[i].fit_id.fti_ptype = pdata->ftps_type;
		}

		pp->ftp_id = dtrace_probe_create(provider->ftp_provid,
		    pdata->ftps_mod, pdata->ftps_func, name, aframes, pp);
	}

	mutex_exit(&provider->ftp_cmtx);

	/*
	 * We know that the provider is still valid since we incremented the
	 * creation reference count. If someone tried to clean up this provider
	 * while we were using it (e.g. because the process called exec(2) or
	 * exit(2)), take note of that and try to clean it up now.
	 */
	mutex_enter(&provider->ftp_mtx);
	provider->ftp_ccount--;
	whack = provider->ftp_retired;
	mutex_exit(&provider->ftp_mtx);

	if (whack)
		fasttrap_pid_cleanup();

	return (0);

no_mem:
	/*
	 * If we've exhausted the allowable resources, we'll try to remove
	 * this provider to free some up. This is to cover the case where
	 * the user has accidentally created many more probes than was
	 * intended (e.g. pid123:::).
	 */
	mutex_exit(&provider->ftp_cmtx);
	mutex_enter(&provider->ftp_mtx);
	provider->ftp_ccount--;
	provider->ftp_marked = 1;
	mutex_exit(&provider->ftp_mtx);

	fasttrap_pid_cleanup();

	return (ENOMEM);
}

/*ARGSUSED*/
static void *
fasttrap_meta_provide(void *arg, dtrace_helper_provdesc_t *dhpv, pid_t pid)
{
	fasttrap_provider_t *provider;

	/*
	 * A 32-bit unsigned integer (like a pid for example) can be
	 * expressed in 10 or fewer decimal digits. Make sure that we'll
	 * have enough space for the provider name.
	 */
	if (strlen(dhpv->dthpv_provname) + 10 >=
	    sizeof (provider->ftp_name)) {
		cmn_err(CE_WARN, "failed to instantiate provider %s: "
		    "name too long to accomodate pid", dhpv->dthpv_provname);
		return (NULL);
	}

	/*
	 * Don't let folks spoof the true pid provider.
	 */
	if (strcmp(dhpv->dthpv_provname, FASTTRAP_PID_NAME) == 0) {
		cmn_err(CE_WARN, "failed to instantiate provider %s: "
		    "%s is an invalid name", dhpv->dthpv_provname,
		    FASTTRAP_PID_NAME);
		return (NULL);
	}

	/*
	 * The highest stability class that fasttrap supports is ISA; cap
	 * the stability of the new provider accordingly.
	 */
	if (dhpv->dthpv_pattr.dtpa_provider.dtat_class > DTRACE_CLASS_ISA)
		dhpv->dthpv_pattr.dtpa_provider.dtat_class = DTRACE_CLASS_ISA;
	if (dhpv->dthpv_pattr.dtpa_mod.dtat_class > DTRACE_CLASS_ISA)
		dhpv->dthpv_pattr.dtpa_mod.dtat_class = DTRACE_CLASS_ISA;
	if (dhpv->dthpv_pattr.dtpa_func.dtat_class > DTRACE_CLASS_ISA)
		dhpv->dthpv_pattr.dtpa_func.dtat_class = DTRACE_CLASS_ISA;
	if (dhpv->dthpv_pattr.dtpa_name.dtat_class > DTRACE_CLASS_ISA)
		dhpv->dthpv_pattr.dtpa_name.dtat_class = DTRACE_CLASS_ISA;
	if (dhpv->dthpv_pattr.dtpa_args.dtat_class > DTRACE_CLASS_ISA)
		dhpv->dthpv_pattr.dtpa_args.dtat_class = DTRACE_CLASS_ISA;

	if ((provider = fasttrap_provider_lookup(pid, dhpv->dthpv_provname,
	    &dhpv->dthpv_pattr)) == NULL) {
		cmn_err(CE_WARN, "failed to instantiate provider %s for "
		    "process %u",  dhpv->dthpv_provname, (uint_t)pid);
		return (NULL);
	}

	/*
	 * Up the meta provider count so this provider isn't removed until
	 * the meta provider has been told to remove it.
	 */
	provider->ftp_mcount++;

	mutex_exit(&provider->ftp_mtx);

	return (provider);
}

/*ARGSUSED*/
static void
fasttrap_meta_create_probe(void *arg, void *parg,
    dtrace_helper_probedesc_t *dhpb)
{
	fasttrap_provider_t *provider = parg;
	fasttrap_probe_t *pp;
	fasttrap_tracepoint_t *tp;
	int i, j;
	uint32_t ntps;

	/*
	 * Since the meta provider count is non-zero we don't have to worry
	 * about this provider disappearing.
	 */
	ASSERT(provider->ftp_mcount > 0);

	/*
	 * The offsets must be unique.
	 */
	qsort(dhpb->dthpb_offs, dhpb->dthpb_noffs, sizeof (uint32_t),
	    fasttrap_uint32_cmp);
	for (i = 1; i < dhpb->dthpb_noffs; i++) {
		if (dhpb->dthpb_base + dhpb->dthpb_offs[i] <=
		    dhpb->dthpb_base + dhpb->dthpb_offs[i - 1])
			return;
	}

	qsort(dhpb->dthpb_enoffs, dhpb->dthpb_nenoffs, sizeof (uint32_t),
	    fasttrap_uint32_cmp);
	for (i = 1; i < dhpb->dthpb_nenoffs; i++) {
		if (dhpb->dthpb_base + dhpb->dthpb_enoffs[i] <=
		    dhpb->dthpb_base + dhpb->dthpb_enoffs[i - 1])
			return;
	}

	/*
	 * Grab the creation lock to ensure consistency between calls to
	 * dtrace_probe_lookup() and dtrace_probe_create() in the face of
	 * other threads creating probes.
	 */
	mutex_enter(&provider->ftp_cmtx);

	if (dtrace_probe_lookup(provider->ftp_provid, dhpb->dthpb_mod,
	    dhpb->dthpb_func, dhpb->dthpb_name) != 0) {
		mutex_exit(&provider->ftp_cmtx);
		return;
	}

	ntps = dhpb->dthpb_noffs + dhpb->dthpb_nenoffs;
	ASSERT(ntps > 0);

	atomic_add_32(&fasttrap_total, ntps);

	if (fasttrap_total > fasttrap_max) {
		atomic_add_32(&fasttrap_total, -ntps);
		mutex_exit(&provider->ftp_cmtx);
		return;
	}

	pp = kmem_zalloc(offsetof(fasttrap_probe_t, ftp_tps[ntps]), KM_SLEEP);

	pp->ftp_prov = provider;
	pp->ftp_pid = provider->ftp_pid;
	pp->ftp_ntps = ntps;
	pp->ftp_nargs = dhpb->dthpb_xargc;
	pp->ftp_xtypes = dhpb->dthpb_xtypes;
	pp->ftp_ntypes = dhpb->dthpb_ntypes;

	/*
	 * First create a tracepoint for each actual point of interest.
	 */
	for (i = 0; i < dhpb->dthpb_noffs; i++) {
		tp = kmem_zalloc(sizeof (fasttrap_tracepoint_t), KM_SLEEP);

		tp->ftt_proc = provider->ftp_proc;
		tp->ftt_pc = dhpb->dthpb_base + dhpb->dthpb_offs[i];
		tp->ftt_pid = provider->ftp_pid;

		pp->ftp_tps[i].fit_tp = tp;
		pp->ftp_tps[i].fit_id.fti_probe = pp;
#ifdef __sparc
		pp->ftp_tps[i].fit_id.fti_ptype = DTFTP_POST_OFFSETS;
#else
		pp->ftp_tps[i].fit_id.fti_ptype = DTFTP_OFFSETS;
#endif
	}

	/*
	 * Then create a tracepoint for each is-enabled point.
	 */
	for (j = 0; i < ntps; i++, j++) {
		tp = kmem_zalloc(sizeof (fasttrap_tracepoint_t), KM_SLEEP);

		tp->ftt_proc = provider->ftp_proc;
		tp->ftt_pc = dhpb->dthpb_base + dhpb->dthpb_enoffs[j];
		tp->ftt_pid = provider->ftp_pid;

		pp->ftp_tps[i].fit_tp = tp;
		pp->ftp_tps[i].fit_id.fti_probe = pp;
		pp->ftp_tps[i].fit_id.fti_ptype = DTFTP_IS_ENABLED;
	}

	/*
	 * If the arguments are shuffled around we set the argument remapping
	 * table. Later, when the probe fires, we only remap the arguments
	 * if the table is non-NULL.
	 */
	for (i = 0; i < dhpb->dthpb_xargc; i++) {
		if (dhpb->dthpb_args[i] != i) {
			pp->ftp_argmap = dhpb->dthpb_args;
			break;
		}
	}

	/*
	 * The probe is fully constructed -- register it with DTrace.
	 */
	pp->ftp_id = dtrace_probe_create(provider->ftp_provid, dhpb->dthpb_mod,
	    dhpb->dthpb_func, dhpb->dthpb_name, FASTTRAP_OFFSET_AFRAMES, pp);

	mutex_exit(&provider->ftp_cmtx);
}

/*ARGSUSED*/
static void
fasttrap_meta_remove(void *arg, dtrace_helper_provdesc_t *dhpv, pid_t pid)
{
	/*
	 * Clean up the USDT provider. There may be active consumers of the
	 * provider busy adding probes, no damage will actually befall the
	 * provider until that count has dropped to zero. This just puts
	 * the provider on death row.
	 */
	fasttrap_provider_retire(pid, dhpv->dthpv_provname, 1);
}

static dtrace_mops_t fasttrap_mops = {
	fasttrap_meta_create_probe,
	fasttrap_meta_provide,
	fasttrap_meta_remove
};

/*ARGSUSED*/
static int
fasttrap_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return (0);
}

/*ARGSUSED*/
static int
fasttrap_ioctl(dev_t dev, int cmd, intptr_t arg, int md, cred_t *cr, int *rv)
{
	if (!dtrace_attached())
		return (EAGAIN);

	if (cmd == FASTTRAPIOC_MAKEPROBE) {
		fasttrap_probe_spec_t *uprobe = (void *)arg;
		fasttrap_probe_spec_t *probe;
		uint64_t noffs;
		size_t size;
		int ret;
		char *c;

		if (copyin(&uprobe->ftps_noffs, &noffs,
		    sizeof (uprobe->ftps_noffs)))
			return (EFAULT);

		/*
		 * Probes must have at least one tracepoint.
		 */
		if (noffs == 0)
			return (EINVAL);

		size = sizeof (fasttrap_probe_spec_t) +
		    sizeof (probe->ftps_offs[0]) * (noffs - 1);

		if (size > 1024 * 1024)
			return (ENOMEM);

		probe = kmem_alloc(size, KM_SLEEP);

		if (copyin(uprobe, probe, size) != 0) {
			kmem_free(probe, size);
			return (EFAULT);
		}

		/*
		 * Verify that the function and module strings contain no
		 * funny characters.
		 */
		for (c = &probe->ftps_func[0]; *c != '\0'; c++) {
			if (*c < 0x20 || 0x7f <= *c) {
				ret = EINVAL;
				goto err;
			}
		}

		for (c = &probe->ftps_mod[0]; *c != '\0'; c++) {
			if (*c < 0x20 || 0x7f <= *c) {
				ret = EINVAL;
				goto err;
			}
		}

		if (!PRIV_POLICY_CHOICE(cr, PRIV_ALL, B_FALSE)) {
			proc_t *p;
			pid_t pid = probe->ftps_pid;

			mutex_enter(&pidlock);
			/*
			 * Report an error if the process doesn't exist
			 * or is actively being birthed.
			 */
			if ((p = prfind(pid)) == NULL || p->p_stat == SIDL) {
				mutex_exit(&pidlock);
				return (ESRCH);
			}
			mutex_enter(&p->p_lock);
			mutex_exit(&pidlock);

			if ((ret = priv_proc_cred_perm(cr, p, NULL,
			    VREAD | VWRITE)) != 0) {
				mutex_exit(&p->p_lock);
				return (ret);
			}

			mutex_exit(&p->p_lock);
		}

		ret = fasttrap_add_probe(probe);
err:
		kmem_free(probe, size);

		return (ret);

	} else if (cmd == FASTTRAPIOC_GETINSTR) {
		fasttrap_instr_query_t instr;
		fasttrap_tracepoint_t *tp;
		uint_t index;
		int ret;

		if (copyin((void *)arg, &instr, sizeof (instr)) != 0)
			return (EFAULT);

		if (!PRIV_POLICY_CHOICE(cr, PRIV_ALL, B_FALSE)) {
			proc_t *p;
			pid_t pid = instr.ftiq_pid;

			mutex_enter(&pidlock);
			/*
			 * Report an error if the process doesn't exist
			 * or is actively being birthed.
			 */
			if ((p = prfind(pid)) == NULL || p->p_stat == SIDL) {
				mutex_exit(&pidlock);
				return (ESRCH);
			}
			mutex_enter(&p->p_lock);
			mutex_exit(&pidlock);

			if ((ret = priv_proc_cred_perm(cr, p, NULL,
			    VREAD)) != 0) {
				mutex_exit(&p->p_lock);
				return (ret);
			}

			mutex_exit(&p->p_lock);
		}

		index = FASTTRAP_TPOINTS_INDEX(instr.ftiq_pid, instr.ftiq_pc);

		mutex_enter(&fasttrap_tpoints.fth_table[index].ftb_mtx);
		tp = fasttrap_tpoints.fth_table[index].ftb_data;
		while (tp != NULL) {
			if (instr.ftiq_pid == tp->ftt_pid &&
			    instr.ftiq_pc == tp->ftt_pc &&
			    tp->ftt_proc->ftpc_acount != 0)
				break;

			tp = tp->ftt_next;
		}

		if (tp == NULL) {
			mutex_exit(&fasttrap_tpoints.fth_table[index].ftb_mtx);
			return (ENOENT);
		}

		bcopy(&tp->ftt_instr, &instr.ftiq_instr,
		    sizeof (instr.ftiq_instr));
		mutex_exit(&fasttrap_tpoints.fth_table[index].ftb_mtx);

		if (copyout(&instr, (void *)arg, sizeof (instr)) != 0)
			return (EFAULT);

		return (0);
	}

	return (EINVAL);
}

static struct cb_ops fasttrap_cb_ops = {
	fasttrap_open,		/* open */
	nodev,			/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	fasttrap_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

/*ARGSUSED*/
static int
fasttrap_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *)fasttrap_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

static int
fasttrap_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	ulong_t nent;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	if (ddi_create_minor_node(devi, "fasttrap", S_IFCHR, 0,
	    DDI_PSEUDO, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	fasttrap_devi = devi;

	/*
	 * Install our hooks into fork(2), exec(2), and exit(2).
	 */
	dtrace_fasttrap_fork_ptr = &fasttrap_fork;
	dtrace_fasttrap_exit_ptr = &fasttrap_exec_exit;
	dtrace_fasttrap_exec_ptr = &fasttrap_exec_exit;

	fasttrap_max = ddi_getprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
	    "fasttrap-max-probes", FASTTRAP_MAX_DEFAULT);
	fasttrap_total = 0;

	/*
	 * Conjure up the tracepoints hashtable...
	 */
	nent = ddi_getprop(DDI_DEV_T_ANY, devi, DDI_PROP_DONTPASS,
	    "fasttrap-hash-size", FASTTRAP_TPOINTS_DEFAULT_SIZE);

	if (nent == 0 || nent > 0x1000000)
		nent = FASTTRAP_TPOINTS_DEFAULT_SIZE;

	if ((nent & (nent - 1)) == 0)
		fasttrap_tpoints.fth_nent = nent;
	else
		fasttrap_tpoints.fth_nent = 1 << fasttrap_highbit(nent);
	ASSERT(fasttrap_tpoints.fth_nent > 0);
	fasttrap_tpoints.fth_mask = fasttrap_tpoints.fth_nent - 1;
	fasttrap_tpoints.fth_table = kmem_zalloc(fasttrap_tpoints.fth_nent *
	    sizeof (fasttrap_bucket_t), KM_SLEEP);

	/*
	 * ... and the providers hash table...
	 */
	nent = FASTTRAP_PROVIDERS_DEFAULT_SIZE;
	if ((nent & (nent - 1)) == 0)
		fasttrap_provs.fth_nent = nent;
	else
		fasttrap_provs.fth_nent = 1 << fasttrap_highbit(nent);
	ASSERT(fasttrap_provs.fth_nent > 0);
	fasttrap_provs.fth_mask = fasttrap_provs.fth_nent - 1;
	fasttrap_provs.fth_table = kmem_zalloc(fasttrap_provs.fth_nent *
	    sizeof (fasttrap_bucket_t), KM_SLEEP);

	/*
	 * ... and the procs hash table.
	 */
	nent = FASTTRAP_PROCS_DEFAULT_SIZE;
	if ((nent & (nent - 1)) == 0)
		fasttrap_procs.fth_nent = nent;
	else
		fasttrap_procs.fth_nent = 1 << fasttrap_highbit(nent);
	ASSERT(fasttrap_procs.fth_nent > 0);
	fasttrap_procs.fth_mask = fasttrap_procs.fth_nent - 1;
	fasttrap_procs.fth_table = kmem_zalloc(fasttrap_procs.fth_nent *
	    sizeof (fasttrap_bucket_t), KM_SLEEP);

	(void) dtrace_meta_register("fasttrap", &fasttrap_mops, NULL,
	    &fasttrap_meta_id);

	return (DDI_SUCCESS);
}

static int
fasttrap_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int i, fail = 0;
	timeout_id_t tmp;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	/*
	 * Unregister the meta-provider to make sure no new fasttrap-
	 * managed providers come along while we're trying to close up
	 * shop. If we fail to detach, we'll need to re-register as a
	 * meta-provider. We can fail to unregister as a meta-provider
	 * if providers we manage still exist.
	 */
	if (fasttrap_meta_id != DTRACE_METAPROVNONE &&
	    dtrace_meta_unregister(fasttrap_meta_id) != 0)
		return (DDI_FAILURE);

	/*
	 * Prevent any new timeouts from running by setting fasttrap_timeout
	 * to a non-zero value, and wait for the current timeout to complete.
	 */
	mutex_enter(&fasttrap_cleanup_mtx);
	fasttrap_cleanup_work = 0;

	while (fasttrap_timeout != (timeout_id_t)1) {
		tmp = fasttrap_timeout;
		fasttrap_timeout = (timeout_id_t)1;

		if (tmp != 0) {
			mutex_exit(&fasttrap_cleanup_mtx);
			(void) untimeout(tmp);
			mutex_enter(&fasttrap_cleanup_mtx);
		}
	}

	fasttrap_cleanup_work = 0;
	mutex_exit(&fasttrap_cleanup_mtx);

	/*
	 * Iterate over all of our providers. If there's still a process
	 * that corresponds to that pid, fail to detach.
	 */
	for (i = 0; i < fasttrap_provs.fth_nent; i++) {
		fasttrap_provider_t **fpp, *fp;
		fasttrap_bucket_t *bucket = &fasttrap_provs.fth_table[i];

		mutex_enter(&bucket->ftb_mtx);
		fpp = (fasttrap_provider_t **)&bucket->ftb_data;
		while ((fp = *fpp) != NULL) {
			/*
			 * Acquire and release the lock as a simple way of
			 * waiting for any other consumer to finish with
			 * this provider. A thread must first acquire the
			 * bucket lock so there's no chance of another thread
			 * blocking on the provider's lock.
			 */
			mutex_enter(&fp->ftp_mtx);
			mutex_exit(&fp->ftp_mtx);

			if (dtrace_unregister(fp->ftp_provid) != 0) {
				fail = 1;
				fpp = &fp->ftp_next;
			} else {
				*fpp = fp->ftp_next;
				fasttrap_provider_free(fp);
			}
		}

		mutex_exit(&bucket->ftb_mtx);
	}

	if (fail) {
		uint_t work;
		/*
		 * If we're failing to detach, we need to unblock timeouts
		 * and start a new timeout if any work has accumulated while
		 * we've been unsuccessfully trying to detach.
		 */
		mutex_enter(&fasttrap_cleanup_mtx);
		fasttrap_timeout = 0;
		work = fasttrap_cleanup_work;
		mutex_exit(&fasttrap_cleanup_mtx);

		if (work)
			fasttrap_pid_cleanup();

		(void) dtrace_meta_register("fasttrap", &fasttrap_mops, NULL,
		    &fasttrap_meta_id);

		return (DDI_FAILURE);
	}

#ifdef DEBUG
	mutex_enter(&fasttrap_count_mtx);
	ASSERT(fasttrap_pid_count == 0);
	mutex_exit(&fasttrap_count_mtx);
#endif

	kmem_free(fasttrap_tpoints.fth_table,
	    fasttrap_tpoints.fth_nent * sizeof (fasttrap_bucket_t));
	fasttrap_tpoints.fth_nent = 0;

	kmem_free(fasttrap_provs.fth_table,
	    fasttrap_provs.fth_nent * sizeof (fasttrap_bucket_t));
	fasttrap_provs.fth_nent = 0;

	kmem_free(fasttrap_procs.fth_table,
	    fasttrap_procs.fth_nent * sizeof (fasttrap_bucket_t));
	fasttrap_procs.fth_nent = 0;

	/*
	 * We know there are no tracepoints in any process anywhere in
	 * the system so there is no process which has its p_dtrace_count
	 * greater than zero, therefore we know that no thread can actively
	 * be executing code in fasttrap_fork(). Similarly for p_dtrace_probes
	 * and fasttrap_exec() and fasttrap_exit().
	 */
	ASSERT(dtrace_fasttrap_fork_ptr == &fasttrap_fork);
	dtrace_fasttrap_fork_ptr = NULL;

	ASSERT(dtrace_fasttrap_exec_ptr == &fasttrap_exec_exit);
	dtrace_fasttrap_exec_ptr = NULL;

	ASSERT(dtrace_fasttrap_exit_ptr == &fasttrap_exec_exit);
	dtrace_fasttrap_exit_ptr = NULL;

	ddi_remove_minor_node(devi, NULL);

	return (DDI_SUCCESS);
}

static struct dev_ops fasttrap_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt */
	fasttrap_info,		/* get_dev_info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	fasttrap_attach,	/* attach */
	fasttrap_detach,	/* detach */
	nodev,			/* reset */
	&fasttrap_cb_ops,	/* driver operations */
	NULL,			/* bus operations */
	nodev			/* dev power */
};

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops,		/* module type (this is a pseudo driver) */
	"Fasttrap Tracing",	/* name of module */
	&fasttrap_ops,		/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}
