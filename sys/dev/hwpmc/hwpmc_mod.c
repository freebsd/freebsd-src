/*-
 * Copyright (c) 2003-2005 Joseph Koshy
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/systm.h>

#include <machine/md_var.h>

/*
 * Types
 */

enum pmc_flags {
	PMC_FLAG_NONE	  = 0x00, /* do nothing */
	PMC_FLAG_REMOVE   = 0x01, /* atomically remove entry from hash */
	PMC_FLAG_ALLOCATE = 0x02, /* add entry to hash if not found */
};

/*
 * The offset in sysent where the syscall is allocated.
 */

static int pmc_syscall_num = NO_SYSCALL;
struct pmc_cpu		**pmc_pcpu;	 /* per-cpu state */
pmc_value_t		*pmc_pcpu_saved; /* saved PMC values: CSW handling */

#define	PMC_PCPU_SAVED(C,R)	pmc_pcpu_saved[(R) + md->pmd_npmc*(C)]

struct mtx_pool		*pmc_mtxpool;
static int		*pmc_pmcdisp;	 /* PMC row dispositions */

#define	PMC_ROW_DISP_IS_FREE(R)		(pmc_pmcdisp[(R)] == 0)
#define	PMC_ROW_DISP_IS_THREAD(R)	(pmc_pmcdisp[(R)] > 0)
#define	PMC_ROW_DISP_IS_STANDALONE(R)	(pmc_pmcdisp[(R)] < 0)

#define	PMC_MARK_ROW_FREE(R) do {					  \
	pmc_pmcdisp[(R)] = 0;						  \
} while (0)

#define	PMC_MARK_ROW_STANDALONE(R) do {					  \
	KASSERT(pmc_pmcdisp[(R)] <= 0, ("[pmc,%d] row disposition error", \
		    __LINE__));						  \
	atomic_add_int(&pmc_pmcdisp[(R)], -1);				  \
	KASSERT(pmc_pmcdisp[(R)] >= (-mp_ncpus), ("[pmc,%d] row "	  \
		"disposition error", __LINE__));			  \
} while (0)

#define	PMC_UNMARK_ROW_STANDALONE(R) do { 				  \
	atomic_add_int(&pmc_pmcdisp[(R)], 1);				  \
	KASSERT(pmc_pmcdisp[(R)] <= 0, ("[pmc,%d] row disposition error", \
		    __LINE__));						  \
} while (0)

#define	PMC_MARK_ROW_THREAD(R) do {					  \
	KASSERT(pmc_pmcdisp[(R)] >= 0, ("[pmc,%d] row disposition error", \
		    __LINE__));						  \
	atomic_add_int(&pmc_pmcdisp[(R)], 1);				  \
} while (0)

#define	PMC_UNMARK_ROW_THREAD(R) do {					  \
	atomic_add_int(&pmc_pmcdisp[(R)], -1);				  \
	KASSERT(pmc_pmcdisp[(R)] >= 0, ("[pmc,%d] row disposition error", \
		    __LINE__));						  \
} while (0)


/* various event handlers */
static eventhandler_tag	pmc_exit_tag, pmc_fork_tag;

/* Module statistics */
struct pmc_op_getdriverstats pmc_stats;

/* Machine/processor dependent operations */
struct pmc_mdep  *md;

/*
 * Hash tables mapping owner processes and target threads to PMCs.
 */

struct mtx pmc_processhash_mtx;		/* spin mutex */
static u_long pmc_processhashmask;
static LIST_HEAD(pmc_processhash, pmc_process)	*pmc_processhash;

/*
 * Hash table of PMC owner descriptors.  This table is protected by
 * the shared PMC "sx" lock.
 */

static u_long pmc_ownerhashmask;
static LIST_HEAD(pmc_ownerhash, pmc_owner)	*pmc_ownerhash;

/*
 * Prototypes
 */

#if	DEBUG
static int	pmc_debugflags_sysctl_handler(SYSCTL_HANDLER_ARGS);
static int	pmc_debugflags_parse(char *newstr, char *fence);
#endif

static int	load(struct module *module, int cmd, void *arg);
static int	pmc_syscall_handler(struct thread *td, void *syscall_args);
static int	pmc_configure_log(struct pmc_owner *po, int logfd);
static void	pmc_log_process_exit(struct pmc *pm, struct pmc_process *pp);
static struct pmc *pmc_allocate_pmc_descriptor(void);
static struct pmc *pmc_find_pmc_descriptor_in_process(struct pmc_owner *po,
    pmc_id_t pmc);
static void	pmc_release_pmc_descriptor(struct pmc *pmc);
static int	pmc_can_allocate_rowindex(struct proc *p, unsigned int ri);
static struct pmc_process *pmc_find_process_descriptor(struct proc *p,
    uint32_t mode);
static void	pmc_remove_process_descriptor(struct pmc_process *pp);
static struct pmc_owner *pmc_find_owner_descriptor(struct proc *p);
static int	pmc_find_pmc(pmc_id_t pmcid, struct pmc **pm);
static void	pmc_remove_owner(struct pmc_owner *po);
static void	pmc_maybe_remove_owner(struct pmc_owner *po);
static void	pmc_unlink_target_process(struct pmc *pmc,
    struct pmc_process *pp);
static void	pmc_link_target_process(struct pmc *pm,
    struct pmc_process *pp);
static void	pmc_unlink_owner(struct pmc *pmc);
static void	pmc_cleanup(void);
static void	pmc_save_cpu_binding(struct pmc_binding *pb);
static void	pmc_restore_cpu_binding(struct pmc_binding *pb);
static void	pmc_select_cpu(int cpu);
static void	pmc_process_exit(void *arg, struct proc *p);
static void	pmc_process_fork(void *arg, struct proc *p1,
    struct proc *p2, int n);
static int	pmc_attach_one_process(struct proc *p, struct pmc *pm);
static int	pmc_attach_process(struct proc *p, struct pmc *pm);
static int	pmc_detach_one_process(struct proc *p, struct pmc *pm,
    int flags);
static int	pmc_detach_process(struct proc *p, struct pmc *pm);
static int	pmc_start(struct pmc *pm);
static int	pmc_stop(struct pmc *pm);
static int	pmc_can_attach(struct pmc *pm, struct proc *p);

/*
 * Kernel tunables and sysctl(8) interface.
 */

#define PMC_SYSCTL_NAME_PREFIX "kern." PMC_MODULE_NAME "."

SYSCTL_NODE(_kern, OID_AUTO, hwpmc, CTLFLAG_RW, 0, "HWPMC parameters");

#if	DEBUG
unsigned int pmc_debugflags = PMC_DEBUG_DEFAULT_FLAGS;
char	pmc_debugstr[PMC_DEBUG_STRSIZE];
TUNABLE_STR(PMC_SYSCTL_NAME_PREFIX "debugflags", pmc_debugstr,
    sizeof(pmc_debugstr));
SYSCTL_PROC(_kern_hwpmc, OID_AUTO, debugflags,
    CTLTYPE_STRING|CTLFLAG_RW|CTLFLAG_TUN,
    0, 0, pmc_debugflags_sysctl_handler, "A", "debug flags");
#endif

/*
 * kern.pmc.hashrows -- determines the number of rows in the
 * of the hash table used to look up threads
 */

static int pmc_hashsize = PMC_HASH_SIZE;
TUNABLE_INT(PMC_SYSCTL_NAME_PREFIX "hashsize", &pmc_hashsize);
SYSCTL_INT(_kern_hwpmc, OID_AUTO, hashsize, CTLFLAG_TUN|CTLFLAG_RD,
    &pmc_hashsize, 0, "rows in hash tables");

/*
 * kern.pmc.pcpusize -- the size of each per-cpu
 * area for collection PC samples.
 */

static int pmc_pcpu_buffer_size = PMC_PCPU_BUFFER_SIZE;
TUNABLE_INT(PMC_SYSCTL_NAME_PREFIX "pcpubuffersize", &pmc_pcpu_buffer_size);
SYSCTL_INT(_kern_hwpmc, OID_AUTO, pcpubuffersize, CTLFLAG_TUN|CTLFLAG_RD,
    &pmc_pcpu_buffer_size, 0, "size of per-cpu buffer in 4K pages");

/*
 * kern.pmc.mtxpoolsize -- number of mutexes in the mutex pool.
 */

static int pmc_mtxpool_size = PMC_MTXPOOL_SIZE;
TUNABLE_INT(PMC_SYSCTL_NAME_PREFIX "mtxpoolsize", &pmc_mtxpool_size);
SYSCTL_INT(_kern_hwpmc, OID_AUTO, mtxpoolsize, CTLFLAG_TUN|CTLFLAG_RD,
    &pmc_mtxpool_size, 0, "size of spin mutex pool");



/*
 * security.bsd.unprivileged_syspmcs -- allow non-root processes to
 * allocate system-wide PMCs.
 *
 * Allowing unprivileged processes to allocate system PMCs is convenient
 * if system-wide measurements need to be taken concurrently with other
 * per-process measurements.  This feature is turned off by default.
 */

SYSCTL_DECL(_security_bsd);

static int pmc_unprivileged_syspmcs = 0;
TUNABLE_INT("security.bsd.unprivileged_syspmcs", &pmc_unprivileged_syspmcs);
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_syspmcs, CTLFLAG_RW,
    &pmc_unprivileged_syspmcs, 0,
    "allow unprivileged process to allocate system PMCs");

#if	PMC_HASH_USE_CRC32

#define	PMC_HASH_PTR(P,M)	(crc32(&(P), sizeof((P))) & (M))

#else 	/* integer multiplication */

#if	LONG_BIT == 64
#define	_PMC_HM		11400714819323198486u
#elif	LONG_BIT == 32
#define	_PMC_HM		2654435769u
#else
#error 	Must know the size of 'long' to compile
#endif

/*
 * Hash function.  Discard the lower 2 bits of the pointer since
 * these are always zero for our uses.  The hash multiplier is
 * round((2^LONG_BIT) * ((sqrt(5)-1)/2)).
 */

#define	PMC_HASH_PTR(P,M)	((((unsigned long) (P) >> 2) * _PMC_HM) & (M))

#endif

/*
 * Syscall structures
 */

/* The `sysent' for the new syscall */
static struct sysent pmc_sysent = {
	2,			/* sy_narg */
	pmc_syscall_handler	/* sy_call */
};

static struct syscall_module_data pmc_syscall_mod = {
	load,
	NULL,
	&pmc_syscall_num,
	&pmc_sysent,
	{ 0, NULL }
};

static moduledata_t pmc_mod = {
	PMC_MODULE_NAME,
	syscall_module_handler,
	&pmc_syscall_mod
};

DECLARE_MODULE(pmc, pmc_mod, SI_SUB_SMP, SI_ORDER_ANY);
MODULE_VERSION(pmc, PMC_VERSION);

#if	DEBUG
static int
pmc_debugflags_parse(char *newstr, char *fence)
{
	char c, *p, *q;
	unsigned int tmpflags;
	int level;
	char tmpbuf[4];		/* 3 character keyword + '\0' */

	tmpflags = 0;
	level = 0xF;	/* max verbosity */

	p = newstr;

	for (; p < fence && (c = *p);) {

		/* skip separators */
		if (c == ' ' || c == '\t' || c == ',') {
			p++; continue;
		}

		(void) strlcpy(tmpbuf, p, sizeof(tmpbuf));

#define	CMP_SET_FLAG_MAJ(S,F)					\
		else if (strncmp(tmpbuf, S, 3) == 0)		\
			tmpflags |= __PMCDFMAJ(F)

#define	CMP_SET_FLAG_MIN(S,F)					\
		else if (strncmp(tmpbuf, S, 3) == 0)		\
			tmpflags |= __PMCDFMIN(F)

		if (fence - p > 6 && strncmp(p, "level=", 6) == 0) {
			p += 6;	/* skip over keyword */
			level = strtoul(p, &q, 16);
		}
		CMP_SET_FLAG_MAJ("mod", MOD);
		CMP_SET_FLAG_MAJ("pmc", PMC);
		CMP_SET_FLAG_MAJ("ctx", CTX);
		CMP_SET_FLAG_MAJ("own", OWN);
		CMP_SET_FLAG_MAJ("prc", PRC);
		CMP_SET_FLAG_MAJ("mdp", MDP);
		CMP_SET_FLAG_MAJ("cpu", CPU);

		CMP_SET_FLAG_MIN("all", ALL);
		CMP_SET_FLAG_MIN("rel", REL);
		CMP_SET_FLAG_MIN("ops", OPS);
		CMP_SET_FLAG_MIN("ini", INI);
		CMP_SET_FLAG_MIN("fnd", FND);
		CMP_SET_FLAG_MIN("pmh", PMH);
		CMP_SET_FLAG_MIN("pms", PMS);
		CMP_SET_FLAG_MIN("orm", ORM);
		CMP_SET_FLAG_MIN("omr", OMR);
		CMP_SET_FLAG_MIN("tlk", TLK);
		CMP_SET_FLAG_MIN("tul", TUL);
		CMP_SET_FLAG_MIN("ext", EXT);
		CMP_SET_FLAG_MIN("exc", EXC);
		CMP_SET_FLAG_MIN("frk", FRK);
		CMP_SET_FLAG_MIN("att", ATT);
		CMP_SET_FLAG_MIN("swi", SWI);
		CMP_SET_FLAG_MIN("swo", SWO);
		CMP_SET_FLAG_MIN("reg", REG);
		CMP_SET_FLAG_MIN("alr", ALR);
		CMP_SET_FLAG_MIN("rea", REA);
		CMP_SET_FLAG_MIN("wri", WRI);
		CMP_SET_FLAG_MIN("cfg", CFG);
		CMP_SET_FLAG_MIN("sta", STA);
		CMP_SET_FLAG_MIN("sto", STO);
		CMP_SET_FLAG_MIN("bnd", BND);
		CMP_SET_FLAG_MIN("sel", SEL);
		else	/* unrecognized keyword */
			return EINVAL;

		p += 4;	/* skip keyword and separator */
	}

	pmc_debugflags = (tmpflags|level);

	return 0;
}

static int
pmc_debugflags_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	char *fence, *newstr;
	int error;
	unsigned int n;

	(void) arg1; (void) arg2; /* unused parameters */

	n = sizeof(pmc_debugstr);
	MALLOC(newstr, char *, n, M_PMC, M_ZERO|M_WAITOK);
	(void) strlcpy(newstr, pmc_debugstr, sizeof(pmc_debugstr));

	error = sysctl_handle_string(oidp, newstr, n, req);

	/* if there is a new string, parse and copy it */
	if (error == 0 && req->newptr != NULL) {
		fence = newstr + (n < req->newlen ? n : req->newlen);
		if ((error = pmc_debugflags_parse(newstr, fence)) == 0)
			(void) strlcpy(pmc_debugstr, newstr,
			    sizeof(pmc_debugstr));
	}

	FREE(newstr, M_PMC);

	return error;
}
#endif

/*
 * Concurrency Control
 *
 * The driver manages the following data structures:
 *
 *   - target process descriptors, one per target process
 *   - owner process descriptors (and attached lists), one per owner process
 *   - lookup hash tables for owner and target processes
 *   - PMC descriptors (and attached lists)
 *   - per-cpu hardware state
 *   - the 'hook' variable through which the kernel calls into
 *     this module
 *   - the machine hardware state (managed by the MD layer)
 *
 * These data structures are accessed from:
 *
 * - thread context-switch code
 * - interrupt handlers (possibly on multiple cpus)
 * - kernel threads on multiple cpus running on behalf of user
 *   processes doing system calls
 * - this driver's private kernel threads
 *
 * = Locks and Locking strategy =
 *
 * The driver uses four locking strategies for its operation:
 *
 * - There is a 'global' SX lock "pmc_sx" that is used to protect
 *   the its 'meta-data'.
 *
 *   Calls into the module (via syscall() or by the kernel) start with
 *   this lock being held in exclusive mode.  Depending on the requested
 *   operation, the lock may be downgraded to 'shared' mode to allow
 *   more concurrent readers into the module.
 *
 *   This SX lock is held in exclusive mode for any operations that
 *   modify the linkages between the driver's internal data structures.
 *
 *   The 'pmc_hook' function pointer is also protected by this lock.
 *   It is only examined with the sx lock held in exclusive mode.  The
 *   kernel module is allowed to be unloaded only with the sx lock
 *   held in exclusive mode.  In normal syscall handling, after
 *   acquiring the pmc_sx lock we first check that 'pmc_hook' is
 *   non-null before proceeding.  This prevents races between the
 *   thread unloading the module and other threads seeking to use the
 *   module.
 *
 * - Lookups of target process structures and owner process structures
 *   cannot use the global "pmc_sx" SX lock because these lookups need
 *   to happen during context switches and in other critical sections
 *   where sleeping is not allowed.  We protect these lookup tables
 *   with their own private spin-mutexes, "pmc_processhash_mtx" and
 *   "pmc_ownerhash_mtx".  These are 'leaf' mutexes, in that no other
 *   lock is acquired with these locks held.
 *
 * - Interrupt handlers work in a lock free manner.  At interrupt
 *   time, handlers look at the PMC pointer (phw->phw_pmc) configured
 *   when the PMC was started.  If this pointer is NULL, the interrupt
 *   is ignored after updating driver statistics.  We ensure that this
 *   pointer is set (using an atomic operation if necessary) before the
 *   PMC hardware is started.  Conversely, this pointer is unset atomically
 *   only after the PMC hardware is stopped.
 *
 *   We ensure that everything needed for the operation of an
 *   interrupt handler is available without it needing to acquire any
 *   locks.  We also ensure that a PMC's software state is destroyed only
 *   after the PMC is taken off hardware (on all CPUs).
 *
 * - Context-switch handling with process-private PMCs needs more
 *   care.
 *
 *   A given process may be the target of multiple PMCs.  For example,
 *   PMCATTACH and PMCDETACH may be requested by a process on one CPU
 *   while the target process is running on another.  A PMC could also
 *   be getting released because its owner is exiting.  We tackle
 *   these situations in the following manner:
 *
 *   - each target process structure 'pmc_process' has an array
 *     of 'struct pmc *' pointers, one for each hardware PMC.
 *
 *   - At context switch IN time, each "target" PMC in RUNNING state
 *     gets started on hardware and a pointer to each PMC is copied into
 *     the per-cpu phw array.  The 'runcount' for the PMC is
 *     incremented.
 *
 *   - At context switch OUT time, all process-virtual PMCs are stopped
 *     on hardware.  The saved value is added to the PMCs value field
 *     only if the PMC is in a non-deleted state (the PMCs state could
 *     have changed during the current time slice).
 *
 *     Note that since in-between a switch IN on a processor and a switch
 *     OUT, the PMC could have been released on another CPU.  Therefore
 *     context switch OUT always looks at the hardware state to turn
 *     OFF PMCs and will update a PMC's saved value only if reachable
 *     from the target process record.
 *
 *   - OP PMCRELEASE could be called on a PMC at any time (the PMC could
 *     be attached to many processes at the time of the call and could
 *     be active on multiple CPUs).
 *
 *     We prevent further scheduling of the PMC by marking it as in
 *     state 'DELETED'.  If the runcount of the PMC is non-zero then
 *     this PMC is currently running on a CPU somewhere.  The thread
 *     doing the PMCRELEASE operation waits by repeatedly doing an
 *     tsleep() till the runcount comes to zero.
 *
 */

/*
 * save the cpu binding of the current kthread
 */

static void
pmc_save_cpu_binding(struct pmc_binding *pb)
{
	PMCDBG(CPU,BND,2, "%s", "save-cpu");
	mtx_lock_spin(&sched_lock);
	pb->pb_bound = sched_is_bound(curthread);
	pb->pb_cpu   = curthread->td_oncpu;
	mtx_unlock_spin(&sched_lock);
	PMCDBG(CPU,BND,2, "save-cpu cpu=%d", pb->pb_cpu);
}

/*
 * restore the cpu binding of the current thread
 */

static void
pmc_restore_cpu_binding(struct pmc_binding *pb)
{
	PMCDBG(CPU,BND,2, "restore-cpu curcpu=%d restore=%d",
	    curthread->td_oncpu, pb->pb_cpu);
	mtx_lock_spin(&sched_lock);
	if (pb->pb_bound)
		sched_bind(curthread, pb->pb_cpu);
	else
		sched_unbind(curthread);
	mtx_unlock_spin(&sched_lock);
	PMCDBG(CPU,BND,2, "%s", "restore-cpu done");
}

/*
 * move execution over the specified cpu and bind it there.
 */

static void
pmc_select_cpu(int cpu)
{
	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[pmc,%d] bad cpu number %d", __LINE__, cpu));

	/* never move to a disabled CPU */
	KASSERT(pmc_cpu_is_disabled(cpu) == 0, ("[pmc,%d] selecting "
	    "disabled CPU %d", __LINE__, cpu));

	PMCDBG(CPU,SEL,2, "select-cpu cpu=%d", cpu);
	mtx_lock_spin(&sched_lock);
	sched_bind(curthread, cpu);
	mtx_unlock_spin(&sched_lock);

	KASSERT(curthread->td_oncpu == cpu,
	    ("[pmc,%d] CPU not bound [cpu=%d, curr=%d]", __LINE__,
		cpu, curthread->td_oncpu));

	PMCDBG(CPU,SEL,2, "select-cpu cpu=%d ok", cpu);
}

/*
 * Update the per-pmc histogram
 */

void
pmc_update_histogram(struct pmc_hw *phw, uintptr_t pc)
{
	(void) phw;
	(void) pc;
}

/*
 * Send a signal to a process.  This is meant to be invoked from an
 * interrupt handler.
 */

void
pmc_send_signal(struct pmc *pmc)
{
	(void) pmc;	/* shutup gcc */

#if	0
	struct proc   *proc;
	struct thread *td;

	KASSERT(pmc->pm_owner != NULL,
	    ("[pmc,%d] No owner for PMC", __LINE__));

	KASSERT((pmc->pm_owner->po_flags & PMC_FLAG_IS_OWNER) &&
	    (pmc->pm_owner->po_flags & PMC_FLAG_HAS_TS_PMC),
	    ("[pmc,%d] interrupting PMC owner has wrong flags 0x%x",
		__LINE__, pmc->pm_owner->po_flags));

	proc = pmc->pm_owner->po_owner;

	KASSERT(curthread->td_proc == proc,
	    ("[pmc,%d] interruping the wrong thread (owner %p, "
		"cur %p)", __LINE__, (void *) proc, curthread->td_proc));

	mtx_lock_spin(&sched_lock);
	td = TAILQ_FIRST(&proc->p_threads);
	mtx_unlock_spin(&sched_lock);
	/* XXX RACE HERE: can 'td' disappear now? */
	trapsignal(td, SIGPROF, 0);
	/* XXX rework this to use the regular 'psignal' interface from a
	   helper thread */
#endif

}

/*
 * remove an process owning PMCs
 */

void
pmc_remove_owner(struct pmc_owner *po)
{
	struct pmc_list *pl, *tmp;

	sx_assert(&pmc_sx, SX_XLOCKED);

	PMCDBG(OWN,ORM,1, "remove-owner po=%p", po);

	/* Remove descriptor from the owner hash table */
	LIST_REMOVE(po, po_next);

	/* pass 1: release all owned PMC descriptors */
	LIST_FOREACH_SAFE(pl, &po->po_pmcs, pl_next, tmp) {

		PMCDBG(OWN,ORM,2, "pl=%p pmc=%p", pl, pl->pl_pmc);

		/* remove the associated PMC descriptor, if present */
		if (pl->pl_pmc)
			pmc_release_pmc_descriptor(pl->pl_pmc);

		/* remove the linked list entry */
		LIST_REMOVE(pl, pl_next);
		FREE(pl, M_PMC);
	}

	/* pass 2: delete the pmc_list chain */
	LIST_FOREACH_SAFE(pl, &po->po_pmcs, pl_next, tmp) {
		KASSERT(pl->pl_pmc == NULL,
		    ("[pmc,%d] non-null pmc pointer", __LINE__));
		LIST_REMOVE(pl, pl_next);
		FREE(pl, M_PMC);
	}

	KASSERT(LIST_EMPTY(&po->po_pmcs),
		("[pmc,%d] PMC list not empty", __LINE__));


	/*
	 * If this process owns a log file used for system wide logging,
	 * remove the log file.
	 *
	 * XXX rework needed.
	 */

	if (po->po_flags & PMC_FLAG_OWNS_LOGFILE)
		pmc_configure_log(po, -1);

}

/*
 * remove an owner process record if all conditions are met.
 */

static void
pmc_maybe_remove_owner(struct pmc_owner *po)
{

	PMCDBG(OWN,OMR,1, "maybe-remove-owner po=%p", po);

	/*
	 * Remove owner record if
	 * - this process does not own any PMCs
	 * - this process has not allocated a system-wide sampling buffer
	 */

	if (LIST_EMPTY(&po->po_pmcs) &&
	    ((po->po_flags & PMC_FLAG_OWNS_LOGFILE) == 0)) {
		pmc_remove_owner(po);
		FREE(po, M_PMC);
	}
}

/*
 * Add an association between a target process and a PMC.
 */

static void
pmc_link_target_process(struct pmc *pm, struct pmc_process *pp)
{
	int ri;
	struct pmc_target *pt;

	sx_assert(&pmc_sx, SX_XLOCKED);

	KASSERT(pm != NULL && pp != NULL,
	    ("[pmc,%d] Null pm %p or pp %p", __LINE__, pm, pp));

	KASSERT(pp->pp_refcnt >= 0 && pp->pp_refcnt < ((int) md->pmd_npmc - 1),
	    ("[pmc,%d] Illegal reference count %d for process record %p",
		__LINE__, pp->pp_refcnt, (void *) pp));

	ri = pm->pm_rowindex;

	PMCDBG(PRC,TLK,1, "link-target pmc=%p ri=%d pmc-process=%p",
	    pm, ri, pp);

#if	DEBUG
	LIST_FOREACH(pt, &pm->pm_targets, pt_next)
	    if (pt->pt_process == pp)
		    KASSERT(0, ("[pmc,%d] pp %p already in pmc %p targets",
				__LINE__, pp, pm));
#endif

	MALLOC(pt, struct pmc_target *, sizeof(struct pmc_target),
	    M_PMC, M_ZERO|M_WAITOK);

	pt->pt_process = pp;

	LIST_INSERT_HEAD(&pm->pm_targets, pt, pt_next);

	atomic_store_rel_ptr(&pp->pp_pmcs[ri].pp_pmc, pm);

	pp->pp_refcnt++;

}

/*
 * Removes the association between a target process and a PMC.
 */

static void
pmc_unlink_target_process(struct pmc *pm, struct pmc_process *pp)
{
	int ri;
	struct pmc_target *ptgt;

	sx_assert(&pmc_sx, SX_XLOCKED);

	KASSERT(pm != NULL && pp != NULL,
	    ("[pmc,%d] Null pm %p or pp %p", __LINE__, pm, pp));

	KASSERT(pp->pp_refcnt >= 1 && pp->pp_refcnt < (int) md->pmd_npmc,
	    ("[pmc,%d] Illegal ref count %d on process record %p",
		__LINE__, pp->pp_refcnt, (void *) pp));

	ri = pm->pm_rowindex;

	PMCDBG(PRC,TUL,1, "unlink-target pmc=%p ri=%d pmc-process=%p",
	    pm, ri, pp);

	KASSERT(pp->pp_pmcs[ri].pp_pmc == pm,
	    ("[pmc,%d] PMC ri %d mismatch pmc %p pp->[ri] %p", __LINE__,
		ri, pm, pp->pp_pmcs[ri].pp_pmc));

	pp->pp_pmcs[ri].pp_pmc = NULL;
	pp->pp_pmcs[ri].pp_pmcval = (pmc_value_t) 0;

	pp->pp_refcnt--;

	/* Remove the target process from the PMC structure */
	LIST_FOREACH(ptgt, &pm->pm_targets, pt_next)
		if (ptgt->pt_process == pp)
			break;

	KASSERT(ptgt != NULL, ("[pmc,%d] process %p (pp: %p) not found "
		    "in pmc %p", __LINE__, pp->pp_proc, pp, pm));

	PMCDBG(PRC,TUL,4, "unlink ptgt=%p", ptgt);

	LIST_REMOVE(ptgt, pt_next);
	FREE(ptgt, M_PMC);
}

/*
 * Remove PMC descriptor 'pmc' from the owner descriptor.
 */

void
pmc_unlink_owner(struct pmc *pm)
{
	struct pmc_list	*pl, *tmp;
	struct pmc_owner *po;

#if	DEBUG
	KASSERT(LIST_EMPTY(&pm->pm_targets),
	    ("[pmc,%d] unlinking PMC with targets", __LINE__));
#endif

	po = pm->pm_owner;

	KASSERT(po != NULL, ("[pmc,%d] No owner for PMC", __LINE__));

	LIST_FOREACH_SAFE(pl, &po->po_pmcs, pl_next, tmp) {
		if (pl->pl_pmc == pm) {
			pl->pl_pmc    = NULL;
			pm->pm_owner = NULL;
			return;
		}
	}

	KASSERT(0, ("[pmc,%d] couldn't find pmc in owner list", __LINE__));
}

/*
 * Check if PMC 'pm' may be attached to target process 't'.
 */

static int
pmc_can_attach(struct pmc *pm, struct proc *t)
{
	struct proc *o;		/* pmc owner */
	struct ucred *oc, *tc;	/* owner, target credentials */
	int decline_attach, i;

	/*
	 * A PMC's owner can always attach that PMC to itself.
	 */

	if ((o = pm->pm_owner->po_owner) == t)
		return 0;

	PROC_LOCK(o);
	oc = o->p_ucred;
	crhold(oc);
	PROC_UNLOCK(o);

	PROC_LOCK(t);
	tc = t->p_ucred;
	crhold(tc);
	PROC_UNLOCK(t);

	/*
	 * The effective uid of the PMC owner should match at least one
	 * of the {effective,real,saved} uids of the target process.
	 */

	decline_attach = oc->cr_uid != tc->cr_uid &&
	    oc->cr_uid != tc->cr_svuid &&
	    oc->cr_uid != tc->cr_ruid;

	/*
	 * Every one of the target's group ids, must be in the owner's
	 * group list.
	 */
	for (i = 0; !decline_attach && i < tc->cr_ngroups; i++)
		decline_attach = !groupmember(tc->cr_groups[i], oc);

	/* check the read and saved gids too */
	if (decline_attach == 0)
		decline_attach = !groupmember(tc->cr_rgid, oc) ||
		    !groupmember(tc->cr_svgid, oc);

	crfree(tc);
	crfree(oc);

	return !decline_attach;
}

/*
 * Attach a process to a PMC.
 */

static int
pmc_attach_one_process(struct proc *p, struct pmc *pm)
{
	int ri;
	struct pmc_process	*pp;

	sx_assert(&pmc_sx, SX_XLOCKED);

	PMCDBG(PRC,ATT,2, "attach-one pm=%p ri=%d proc=%p (%d, %s)", pm,
	    pm->pm_rowindex, p, p->p_pid, p->p_comm);

	/*
	 * Locate the process descriptor corresponding to process 'p',
	 * allocating space as needed.
	 *
	 * Verify that rowindex 'pm_rowindex' is free in the process
	 * descriptor.
	 *
	 * If not, allocate space for a descriptor and link the
	 * process descriptor and PMC.
	 */

	ri = pm->pm_rowindex;

	if ((pp = pmc_find_process_descriptor(p, PMC_FLAG_ALLOCATE)) == NULL)
		return ENOMEM;

	if (pp->pp_pmcs[ri].pp_pmc == pm) /* already present at slot [ri] */
		return EEXIST;

	if (pp->pp_pmcs[ri].pp_pmc != NULL)
		return EBUSY;

	pmc_link_target_process(pm, pp);

	/* mark process as using HWPMCs */
	PROC_LOCK(p);
	p->p_flag |= P_HWPMC;
	PROC_UNLOCK(p);

	return 0;
}

/*
 * Attach a process and optionally its children
 */

static int
pmc_attach_process(struct proc *p, struct pmc *pm)
{
	int error;
	struct proc *top;

	sx_assert(&pmc_sx, SX_XLOCKED);

	PMCDBG(PRC,ATT,1, "attach pm=%p ri=%d proc=%p (%d, %s)", pm,
	    pm->pm_rowindex, p, p->p_pid, p->p_comm);

	if ((pm->pm_flags & PMC_F_DESCENDANTS) == 0)
		return pmc_attach_one_process(p, pm);

	/*
	 * Traverse all child processes, attaching them to
	 * this PMC.
	 */

	sx_slock(&proctree_lock);

	top = p;

	for (;;) {
		if ((error = pmc_attach_one_process(p, pm)) != 0)
			break;
		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				goto done;
			if (LIST_NEXT(p, p_sibling)) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}

	if (error)
		(void) pmc_detach_process(top, pm);

 done:
	sx_sunlock(&proctree_lock);
	return error;
}

/*
 * Detach a process from a PMC.  If there are no other PMCs tracking
 * this process, remove the process structure from its hash table.  If
 * 'flags' contains PMC_FLAG_REMOVE, then free the process structure.
 */

static int
pmc_detach_one_process(struct proc *p, struct pmc *pm, int flags)
{
	int ri;
	struct pmc_process *pp;

	sx_assert(&pmc_sx, SX_XLOCKED);

	KASSERT(pm != NULL,
	    ("[pmc,%d] null pm pointer", __LINE__));

	PMCDBG(PRC,ATT,2, "detach-one pm=%p ri=%d proc=%p (%d, %s) flags=0x%x",
	    pm, pm->pm_rowindex, p, p->p_pid, p->p_comm, flags);

	ri = pm->pm_rowindex;

	if ((pp = pmc_find_process_descriptor(p, 0)) == NULL)
		return ESRCH;

	if (pp->pp_pmcs[ri].pp_pmc != pm)
		return EINVAL;

	pmc_unlink_target_process(pm, pp);

	/*
	 * If there are no PMCs targetting this process, we remove its
	 * descriptor from the target hash table and unset the P_HWPMC
	 * flag in the struct proc.
	 */

	KASSERT(pp->pp_refcnt >= 0 && pp->pp_refcnt < (int) md->pmd_npmc,
	    ("[pmc,%d] Illegal refcnt %d for process struct %p",
		__LINE__, pp->pp_refcnt, pp));

	if (pp->pp_refcnt != 0)	/* still a target of some PMC */
		return 0;

	pmc_remove_process_descriptor(pp);

	if (flags & PMC_FLAG_REMOVE)
		FREE(pp, M_PMC);

	PROC_LOCK(p);
	p->p_flag &= ~P_HWPMC;
	PROC_UNLOCK(p);

	return 0;
}

/*
 * Detach a process and optionally its descendants from a PMC.
 */

static int
pmc_detach_process(struct proc *p, struct pmc *pm)
{
	struct proc *top;

	sx_assert(&pmc_sx, SX_XLOCKED);

	PMCDBG(PRC,ATT,1, "detach pm=%p ri=%d proc=%p (%d, %s)", pm,
	    pm->pm_rowindex, p, p->p_pid, p->p_comm);

	if ((pm->pm_flags & PMC_F_DESCENDANTS) == 0)
		return pmc_detach_one_process(p, pm, PMC_FLAG_REMOVE);

	/*
	 * Traverse all children, detaching them from this PMC.  We
	 * ignore errors since we could be detaching a PMC from a
	 * partially attached proc tree.
	 */

	sx_slock(&proctree_lock);

	top = p;

	for (;;) {
		(void) pmc_detach_one_process(p, pm, PMC_FLAG_REMOVE);

		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				goto done;
			if (LIST_NEXT(p, p_sibling)) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}

 done:
	sx_sunlock(&proctree_lock);
	return 0;
}

/*
 * The 'hook' invoked from the kernel proper
 */


#if	DEBUG
const char *pmc_hooknames[] = {
	"",
	"EXIT",
	"EXEC",
	"FORK",
	"CSW-IN",
	"CSW-OUT"
};
#endif

static int
pmc_hook_handler(struct thread *td, int function, void *arg)
{

	KASSERT(td->td_proc->p_flag & P_HWPMC,
	    ("[pmc,%d] unregistered thread called pmc_hook()", __LINE__));

	PMCDBG(MOD,PMH,1, "hook td=%p func=%d \"%s\" arg=%p", td, function,
	    pmc_hooknames[function], arg);

	switch (function)
	{

	/*
	 * Process exit.
	 *
	 * Remove this process from all hash tables.  If this process
	 * owned any PMCs, turn off those PMCs and deallocate them,
	 * removing any associations with target processes.
	 *
	 * This function will be called by the last 'thread' of a
	 * process.
	 *
	 */

	case PMC_FN_PROCESS_EXIT: /* release PMCs */
	{
		int cpu;
		unsigned int ri;
		struct pmc *pm;
		struct pmc_hw *phw;
		struct pmc_process *pp;
		struct pmc_owner *po;
		struct proc *p;
		pmc_value_t newvalue, tmp;

		sx_assert(&pmc_sx, SX_XLOCKED);

		p = (struct proc *) arg;

		/*
		 * Since this code is invoked by the last thread in an
		 * exiting process, we would have context switched IN
		 * at some prior point.  Kernel mode context switches
		 * may happen any time, so we want to disable a context
		 * switch OUT till we get any PMCs targetting this
		 * process off the hardware.
		 *
		 * We also need to atomically remove this process'
		 * entry from our target process hash table, using
		 * PMC_FLAG_REMOVE.
		 */

		PMCDBG(PRC,EXT,1, "process-exit proc=%p (%d, %s)", p, p->p_pid,
		    p->p_comm);

		critical_enter(); /* no preemption */

		cpu = curthread->td_oncpu;

		if ((pp = pmc_find_process_descriptor(p,
			 PMC_FLAG_REMOVE)) != NULL) {

			PMCDBG(PRC,EXT,2,
			    "process-exit proc=%p pmc-process=%p", p, pp);

			/*
			 * This process could the target of some PMCs.
			 * Such PMCs will thus be running on currently
			 * executing CPU at this point in the code
			 * since we've disallowed context switches.
			 * We need to turn these PMCs off like we
			 * would do at context switch OUT time.
			 */

			for (ri = 0; ri < md->pmd_npmc; ri++) {

				/*
				 * Pick up the pmc pointer from hardware
				 * state similar to the CSW_OUT code.
				 */

				phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
				pm  = phw->phw_pmc;

				PMCDBG(PRC,EXT,2, "ri=%d pm=%p", ri, pm);

				if (pm == NULL ||
				    !PMC_IS_VIRTUAL_MODE(pm->pm_mode))
					continue;

				PMCDBG(PRC,EXT,2, "ppmcs[%d]=%p pm=%p "
				    "state=%d", ri, pp->pp_pmcs[ri].pp_pmc,
				    pm, pm->pm_state);

				KASSERT(pm->pm_rowindex == ri,
				    ("[pmc,%d] ri mismatch pmc(%d) ri(%d)",
					__LINE__, pm->pm_rowindex, ri));

				KASSERT(pm == pp->pp_pmcs[ri].pp_pmc,
				    ("[pmc,%d] pm %p != pp_pmcs[%d] %p",
					__LINE__, pm, ri,
					pp->pp_pmcs[ri].pp_pmc));

				(void) md->pmd_stop_pmc(cpu, ri);

				KASSERT(pm->pm_runcount > 0,
				    ("[pmc,%d] bad runcount ri %d rc %d",
					__LINE__, ri, pm->pm_runcount));

				if (pm->pm_state == PMC_STATE_RUNNING) {
					md->pmd_read_pmc(cpu, ri, &newvalue);
					tmp = newvalue -
					    PMC_PCPU_SAVED(cpu,ri);

					mtx_pool_lock_spin(pmc_mtxpool, pm);
					pm->pm_gv.pm_savedvalue += tmp;
					pp->pp_pmcs[ri].pp_pmcval += tmp;
					mtx_pool_unlock_spin(pmc_mtxpool, pm);
				}

				KASSERT((int) pm->pm_runcount >= 0,
				    ("[pmc,%d] runcount is %d", __LINE__, ri));

				atomic_subtract_rel_32(&pm->pm_runcount,1);
				(void) md->pmd_config_pmc(cpu, ri, NULL);
			}
			critical_exit(); /* ok to be pre-empted now */

			/*
			 * Unlink this process from the PMCs that are
			 * targetting it.  Log value at exit() time if
			 * requested.
			 */

			for (ri = 0; ri < md->pmd_npmc; ri++)
				if ((pm = pp->pp_pmcs[ri].pp_pmc) != NULL) {
					if (pm->pm_flags &
					    PMC_F_LOG_TC_PROCEXIT)
						pmc_log_process_exit(pm, pp);
					pmc_unlink_target_process(pm, pp);
				}

			FREE(pp, M_PMC);

		} else
			critical_exit(); /* pp == NULL */

		/*
		 * If the process owned PMCs, free them up and free up
		 * memory.
		 */

		if ((po = pmc_find_owner_descriptor(p)) != NULL) {
			pmc_remove_owner(po);
			FREE(po, M_PMC);
		}

	}
	break;

	/*
	 * Process exec()
	 */

	case PMC_FN_PROCESS_EXEC:
	{
		int *credentials_changed;
		unsigned int ri;
		struct pmc *pm;
		struct proc *p;
		struct pmc_owner *po;
		struct pmc_process *pp;

		sx_assert(&pmc_sx, SX_XLOCKED);

		/*
		 * PMCs are not inherited across an exec():  remove any
		 * PMCs that this process is the owner of.
		 */

		p = td->td_proc;

		if ((po = pmc_find_owner_descriptor(p)) != NULL) {
			pmc_remove_owner(po);
			FREE(po, M_PMC);
		}

		/*
		 * If this process is the target of a PMC, check if the new
		 * credentials are compatible with the owner's permissions.
		 */

		if ((pp = pmc_find_process_descriptor(p, 0)) == NULL)
			break;

		credentials_changed = arg;

		PMCDBG(PRC,EXC,1, "exec proc=%p (%d, %s) cred-changed=%d",
		    p, p->p_pid, p->p_comm, *credentials_changed);

		if (*credentials_changed == 0) /* credentials didn't change */
			break;

		/*
		 * If the newly exec()'ed process has a different credential
		 * than before, allow it to be the target of a PMC only if
		 * the PMC's owner has sufficient priviledge.
		 */

		for (ri = 0; ri < md->pmd_npmc; ri++)
			if ((pm = pp->pp_pmcs[ri].pp_pmc) != NULL)
				if (pmc_can_attach(pm, td->td_proc) != 0)
					pmc_detach_one_process(td->td_proc,
					    pm, PMC_FLAG_NONE);

		KASSERT(pp->pp_refcnt >= 0 && pp->pp_refcnt < (int) md->pmd_npmc,
		    ("[pmc,%d] Illegal ref count %d on pp %p", __LINE__,
			pp->pp_refcnt, pp));

		/*
		 * If this process is no longer the target of any
		 * PMCs, we can remove the process entry and free
		 * up space.
		 */

		if (pp->pp_refcnt == 0) {
			pmc_remove_process_descriptor(pp);
			FREE(pp, M_PMC);
		}
	}
	break;

	/*
	 * Process fork()
	 */

	case PMC_FN_PROCESS_FORK:
	{
		unsigned int ri;
		uint32_t do_descendants;
		struct pmc *pm;
		struct pmc_process *ppnew, *ppold;
		struct proc *newproc;

		sx_assert(&pmc_sx, SX_XLOCKED);

		newproc = (struct proc *) arg;

		PMCDBG(PMC,FRK,2, "process-fork p1=%p p2=%p",
		    curthread->td_proc, newproc);
		/*
		 * If the parent process (curthread->td_proc) is a
		 * target of any PMCs, look for PMCs that are to be
		 * inherited, and link these into the new process
		 * descriptor.
		 */

		if ((ppold = pmc_find_process_descriptor(
		    curthread->td_proc, PMC_FLAG_NONE)) == NULL)
			break;

		do_descendants = 0;
		for (ri = 0; ri < md->pmd_npmc; ri++)
			if ((pm = ppold->pp_pmcs[ri].pp_pmc) != NULL)
				do_descendants |=
				    pm->pm_flags & PMC_F_DESCENDANTS;
		if (do_descendants == 0) /* nothing to do */
			break;

		if ((ppnew = pmc_find_process_descriptor(newproc,
		    PMC_FLAG_ALLOCATE)) == NULL)
			return ENOMEM;

		/*
		 * Run through all PMCs targeting the old process and
		 * attach them to the new process.
		 */

		for (ri = 0; ri < md->pmd_npmc; ri++)
			if ((pm = ppold->pp_pmcs[ri].pp_pmc) != NULL &&
			    pm->pm_flags & PMC_F_DESCENDANTS)
				pmc_link_target_process(pm, ppnew);

		/*
		 * Now mark the new process as being tracked by this
		 * driver.
		 */

		PROC_LOCK(newproc);
		newproc->p_flag |= P_HWPMC;
		PROC_UNLOCK(newproc);

	}
	break;

	/*
	 * Thread context switch IN
	 */

	case PMC_FN_CSW_IN:
	{
		int cpu;
		unsigned int ri;
		struct pmc *pm;
		struct proc *p;
		struct pmc_cpu *pc;
		struct pmc_hw *phw;
		struct pmc_process *pp;
		pmc_value_t newvalue;

		p = td->td_proc;

		if ((pp = pmc_find_process_descriptor(p, PMC_FLAG_NONE)) == NULL)
			break;

		KASSERT(pp->pp_proc == td->td_proc,
		    ("[pmc,%d] not my thread state", __LINE__));

		critical_enter(); /* no preemption on this CPU */

		cpu = PCPU_GET(cpuid); /* td->td_oncpu is invalid */

		PMCDBG(CTX,SWI,1, "cpu=%d proc=%p (%d, %s) pp=%p", cpu, p,
		    p->p_pid, p->p_comm, pp);

		KASSERT(cpu >= 0 && cpu < mp_ncpus,
		    ("[pmc,%d] wierd CPU id %d", __LINE__, cpu));

		pc = pmc_pcpu[cpu];

		for (ri = 0; ri < md->pmd_npmc; ri++) {

			if ((pm = pp->pp_pmcs[ri].pp_pmc) == NULL)
				continue;

			KASSERT(PMC_IS_VIRTUAL_MODE(pm->pm_mode),
			    ("[pmc,%d] Target PMC in non-virtual mode (%d)",
				__LINE__, pm->pm_mode));

			KASSERT(pm->pm_rowindex == ri,
			    ("[pmc,%d] Row index mismatch pmc %d != ri %d",
				__LINE__, pm->pm_rowindex, ri));

			/*
			 * Only PMCs that are marked as 'RUNNING' need
			 * be placed on hardware.
			 */

			if (pm->pm_state != PMC_STATE_RUNNING)
				continue;

			/* increment PMC runcount */
			atomic_add_rel_32(&pm->pm_runcount, 1);

			/* configure the HWPMC we are going to use. */
			md->pmd_config_pmc(cpu, ri, pm);

			phw = pc->pc_hwpmcs[ri];

			KASSERT(phw != NULL,
			    ("[pmc,%d] null hw pointer", __LINE__));

			KASSERT(phw->phw_pmc == pm,
			    ("[pmc,%d] hw->pmc %p != pmc %p", __LINE__,
				phw->phw_pmc, pm));

			/* write out saved value and start the PMC */
			mtx_pool_lock_spin(pmc_mtxpool, pm);
			newvalue = PMC_PCPU_SAVED(cpu, ri) =
			    pm->pm_gv.pm_savedvalue;
			mtx_pool_unlock_spin(pmc_mtxpool, pm);

			md->pmd_write_pmc(cpu, ri, newvalue);
			md->pmd_start_pmc(cpu, ri);

		}

		/*
		 * perform any other architecture/cpu dependent thread
		 * switch-in actions.
		 */

		(void) (*md->pmd_switch_in)(pc);

		critical_exit();

	}
	break;

	/*
	 * Thread context switch OUT.
	 */

	case PMC_FN_CSW_OUT:
	{
		int cpu;
		unsigned int ri;
		struct pmc *pm;
		struct proc *p;
		struct pmc_cpu *pc;
		struct pmc_hw *phw;
		struct pmc_process *pp;
		pmc_value_t newvalue, tmp;

		/*
		 * Locate our process descriptor; this may be NULL if
		 * this process is exiting and we have already removed
		 * the process from the target process table.
		 *
		 * Note that due to kernel preemption, multiple
		 * context switches may happen while the process is
		 * exiting.
		 *
		 * Note also that if the target process cannot be
		 * found we still need to deconfigure any PMCs that
		 * are currently running on hardware.
		 */

		p = td->td_proc;
		pp = pmc_find_process_descriptor(p, PMC_FLAG_NONE);

		/*
		 * save PMCs
		 */

		critical_enter();

		cpu = PCPU_GET(cpuid); /* td->td_oncpu is invalid */

		PMCDBG(CTX,SWO,1, "cpu=%d proc=%p (%d, %s) pp=%p", cpu, p,
		    p->p_pid, p->p_comm, pp);

		KASSERT(cpu >= 0 && cpu < mp_ncpus,
		    ("[pmc,%d wierd CPU id %d", __LINE__, cpu));

		pc = pmc_pcpu[cpu];

		/*
		 * When a PMC gets unlinked from a target PMC, it will
		 * be removed from the target's pp_pmc[] array.
		 *
		 * However, on a MP system, the target could have been
		 * executing on another CPU at the time of the unlink.
		 * So, at context switch OUT time, we need to look at
		 * the hardware to determine if a PMC is scheduled on
		 * it.
		 */

		for (ri = 0; ri < md->pmd_npmc; ri++) {

			phw = pc->pc_hwpmcs[ri];
			pm  = phw->phw_pmc;

			if (pm == NULL)	/* nothing at this row index */
				continue;

			if (!PMC_IS_VIRTUAL_MODE(pm->pm_mode))
				continue; /* not a process virtual PMC */

			KASSERT(pm->pm_rowindex == ri,
			    ("[pmc,%d] ri mismatch pmc(%d) ri(%d)",
				__LINE__, pm->pm_rowindex, ri));

			/* Stop hardware */
			md->pmd_stop_pmc(cpu, ri);

			/* reduce this PMC's runcount */
			atomic_subtract_rel_32(&pm->pm_runcount, 1);

			/*
			 * If this PMC is associated with this process,
			 * save the reading.
			 */

			if (pp != NULL && pp->pp_pmcs[ri].pp_pmc != NULL) {

				KASSERT(pm == pp->pp_pmcs[ri].pp_pmc,
				    ("[pmc,%d] pm %p != pp_pmcs[%d] %p",
					__LINE__, pm, ri,
					pp->pp_pmcs[ri].pp_pmc));

				KASSERT(pp->pp_refcnt > 0,
				    ("[pmc,%d] pp refcnt = %d", __LINE__,
					pp->pp_refcnt));

				md->pmd_read_pmc(cpu, ri, &newvalue);

				tmp = newvalue - PMC_PCPU_SAVED(cpu,ri);

				KASSERT((int64_t) tmp >= 0,
				    ("[pmc,%d] negative increment cpu=%d "
					"ri=%d newvalue=%jx saved=%jx "
					"incr=%jx", __LINE__, cpu, ri,
					newvalue, PMC_PCPU_SAVED(cpu,ri),
					tmp));

				/*
				 * Increment the PMC's count and this
				 * target process's count by the difference
				 * between the current reading and the
				 * saved value at context switch in time.
				 */

				mtx_pool_lock_spin(pmc_mtxpool, pm);

				pm->pm_gv.pm_savedvalue += tmp;
				pp->pp_pmcs[ri].pp_pmcval += tmp;

				mtx_pool_unlock_spin(pmc_mtxpool, pm);

			}

			/* mark hardware as free */
			md->pmd_config_pmc(cpu, ri, NULL);
		}

		/*
		 * perform any other architecture/cpu dependent thread
		 * switch out functions.
		 */

		(void) (*md->pmd_switch_out)(pc);

		critical_exit();

	}
	break;

	default:
#if DEBUG
		KASSERT(0, ("[pmc,%d] unknown hook %d\n", __LINE__, function));
#endif
		break;

	}

	return 0;
}

/*
 * allocate a 'struct pmc_owner' descriptor in the owner hash table.
 */

static struct pmc_owner *
pmc_allocate_owner_descriptor(struct proc *p)
{
	uint32_t hindex;
	struct pmc_owner *po;
	struct pmc_ownerhash *poh;

	hindex = PMC_HASH_PTR(p, pmc_ownerhashmask);
	poh = &pmc_ownerhash[hindex];

	/* allocate space for N pointers and one descriptor struct */
	MALLOC(po, struct pmc_owner *, sizeof(struct pmc_owner),
	    M_PMC, M_WAITOK);

	po->po_flags = 0;
	po->po_owner = p;
	LIST_INIT(&po->po_pmcs);
	LIST_INSERT_HEAD(poh, po, po_next); /* insert into hash table */

	PMCDBG(OWN,ALL,1, "allocate-owner proc=%p (%d, %s) pmc-owner=%p",
	    p, p->p_pid, p->p_comm, po);

	return po;
}

/*
 * find the descriptor corresponding to process 'p', adding or removing it
 * as specified by 'mode'.
 */

static struct pmc_process *
pmc_find_process_descriptor(struct proc *p, uint32_t mode)
{
	uint32_t hindex;
	struct pmc_process *pp, *ppnew;
	struct pmc_processhash *pph;

	hindex = PMC_HASH_PTR(p, pmc_processhashmask);
	pph = &pmc_processhash[hindex];

	ppnew = NULL;

	/*
	 * Pre-allocate memory in the FIND_ALLOCATE case since we
	 * cannot call malloc(9) once we hold a spin lock.
	 */

	if (mode & PMC_FLAG_ALLOCATE) {
		/* allocate additional space for 'n' pmc pointers */
		MALLOC(ppnew, struct pmc_process *,
		    sizeof(struct pmc_process) + md->pmd_npmc *
		    sizeof(struct pmc_targetstate), M_PMC, M_ZERO|M_WAITOK);
	}

	mtx_lock_spin(&pmc_processhash_mtx);
	LIST_FOREACH(pp, pph, pp_next)
	    if (pp->pp_proc == p)
		    break;

	if ((mode & PMC_FLAG_REMOVE) && pp != NULL)
		LIST_REMOVE(pp, pp_next);

	if ((mode & PMC_FLAG_ALLOCATE) && pp == NULL &&
	    ppnew != NULL) {
		ppnew->pp_proc = p;
		LIST_INSERT_HEAD(pph, ppnew, pp_next);
		pp = ppnew;
		ppnew = NULL;
	}
	mtx_unlock_spin(&pmc_processhash_mtx);

	if (pp != NULL && ppnew != NULL)
		FREE(ppnew, M_PMC);

	return pp;
}

/*
 * remove a process descriptor from the process hash table.
 */

static void
pmc_remove_process_descriptor(struct pmc_process *pp)
{
	KASSERT(pp->pp_refcnt == 0,
	    ("[pmc,%d] Removing process descriptor %p with count %d",
		__LINE__, pp, pp->pp_refcnt));

	mtx_lock_spin(&pmc_processhash_mtx);
	LIST_REMOVE(pp, pp_next);
	mtx_unlock_spin(&pmc_processhash_mtx);
}


/*
 * find an owner descriptor corresponding to proc 'p'
 */

static struct pmc_owner *
pmc_find_owner_descriptor(struct proc *p)
{
	uint32_t hindex;
	struct pmc_owner *po;
	struct pmc_ownerhash *poh;

	hindex = PMC_HASH_PTR(p, pmc_ownerhashmask);
	poh = &pmc_ownerhash[hindex];

	po = NULL;
	LIST_FOREACH(po, poh, po_next)
	    if (po->po_owner == p)
		    break;

	PMCDBG(OWN,FND,1, "find-owner proc=%p (%d, %s) hindex=0x%x -> "
	    "pmc-owner=%p", p, p->p_pid, p->p_comm, hindex, po);

	return po;
}

/*
 * pmc_allocate_pmc_descriptor
 *
 * Allocate a pmc descriptor and initialize its
 * fields.
 */

static struct pmc *
pmc_allocate_pmc_descriptor(void)
{
	struct pmc *pmc;

	MALLOC(pmc, struct pmc *, sizeof(struct pmc), M_PMC, M_ZERO|M_WAITOK);

	if (pmc != NULL) {
		pmc->pm_owner = NULL;
		LIST_INIT(&pmc->pm_targets);
	}

	PMCDBG(PMC,ALL,1, "allocate-pmc -> pmc=%p", pmc);

	return pmc;
}

/*
 * Destroy a pmc descriptor.
 */

static void
pmc_destroy_pmc_descriptor(struct pmc *pm)
{
	(void) pm;

#if	DEBUG
	KASSERT(pm->pm_state == PMC_STATE_DELETED ||
	    pm->pm_state == PMC_STATE_FREE,
	    ("[pmc,%d] destroying non-deleted PMC", __LINE__));
	KASSERT(LIST_EMPTY(&pm->pm_targets),
	    ("[pmc,%d] destroying pmc with targets", __LINE__));
	KASSERT(pm->pm_owner == NULL,
	    ("[pmc,%d] destroying pmc attached to an owner", __LINE__));
	KASSERT(pm->pm_runcount == 0,
	    ("[pmc,%d] pmc has non-zero run count %d", __LINE__,
		pm->pm_runcount));
#endif
}

/*
 * This function does the following things:
 *
 *  - detaches the PMC from hardware
 *  - unlinks all target threads that were attached to it
 *  - removes the PMC from its owner's list
 *  - destroy's the PMC private mutex
 *
 * Once this function completes, the given pmc pointer can be safely
 * FREE'd by the caller.
 */

static void
pmc_release_pmc_descriptor(struct pmc *pm)
{
#if	DEBUG
	volatile int maxloop;
#endif
	u_int ri, cpu;
	u_char curpri;
	struct pmc_hw *phw;
	struct pmc_process *pp;
	struct pmc_target *ptgt, *tmp;
	struct pmc_binding pb;

	sx_assert(&pmc_sx, SX_XLOCKED);

	KASSERT(pm, ("[pmc,%d] null pmc", __LINE__));

	ri = pm->pm_rowindex;

	PMCDBG(PMC,REL,1, "release-pmc pmc=%p ri=%d mode=%d", pm, ri,
	    pm->pm_mode);

	/*
	 * First, we take the PMC off hardware.
	 */
	cpu = 0;
	if (PMC_IS_SYSTEM_MODE(pm->pm_mode)) {

		/*
		 * A system mode PMC runs on a specific CPU.  Switch
		 * to this CPU and turn hardware off.
		 */

		pmc_save_cpu_binding(&pb);

		cpu = pm->pm_gv.pm_cpu;

		if (pm->pm_state == PMC_STATE_RUNNING) {

			pmc_select_cpu(cpu);

			phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];

			KASSERT(phw->phw_pmc == pm,
			    ("[pmc, %d] pmc ptr ri(%d) hw(%p) pm(%p)",
				__LINE__, ri, phw->phw_pmc, pm));

			PMCDBG(PMC,REL,2, "stopping cpu=%d ri=%d", cpu, ri);

			critical_enter();
			md->pmd_stop_pmc(cpu, ri);
			critical_exit();
		}

		PMCDBG(PMC,REL,2, "decfg cpu=%d ri=%d", cpu, ri);

		critical_enter();
		md->pmd_config_pmc(cpu, ri, NULL);
		critical_exit();

		pm->pm_state = PMC_STATE_DELETED;

		pmc_restore_cpu_binding(&pb);

	} else if (PMC_IS_VIRTUAL_MODE(pm->pm_mode)) {

		/*
		 * A virtual PMC could be running on multiple CPUs at
		 * a given instant.
		 *
		 * By marking its state as DELETED, we ensure that
		 * this PMC is never further scheduled on hardware.
		 *
		 * Then we wait till all CPUs are done with this PMC.
		 */

		pm->pm_state = PMC_STATE_DELETED;


		/*
		 * Wait for the PMCs runcount to come to zero.
		 */

#if	DEBUG
		maxloop = 100 * mp_ncpus;
#endif

		while (atomic_load_acq_32(&pm->pm_runcount) > 0) {

#if	DEBUG
			maxloop--;
			KASSERT(maxloop > 0,
			    ("[pmc,%d] (ri%d, rc%d) waiting too long for "
				"pmc to be free", __LINE__, pm->pm_rowindex,
				pm->pm_runcount));
#endif

			mtx_lock_spin(&sched_lock);
			curpri = curthread->td_priority;
			mtx_unlock_spin(&sched_lock);

			(void) tsleep((void *) pmc_release_pmc_descriptor,
			    curpri, "pmcrel", 1);

		}

		/*
		 * At this point the PMC is off all CPUs and cannot be
		 * freshly scheduled onto a CPU.  It is now safe to
		 * unlink all targets from this PMC.  If a
		 * process-record's refcount falls to zero, we remove
		 * it from the hash table.  The module-wide SX lock
		 * protects us from races.
		 */

		LIST_FOREACH_SAFE(ptgt, &pm->pm_targets, pt_next, tmp) {
			pp = ptgt->pt_process;
			pmc_unlink_target_process(pm, pp); /* frees 'ptgt' */

			PMCDBG(PMC,REL,3, "pp->refcnt=%d", pp->pp_refcnt);

			/*
			 * If the target process record shows that no
			 * PMCs are attached to it, reclaim its space.
			 */

			if (pp->pp_refcnt == 0) {
				pmc_remove_process_descriptor(pp);
				FREE(pp, M_PMC);
			}
		}

		cpu = curthread->td_oncpu; /* setup cpu for pmd_release() */

	}

	/*
	 * Release any MD resources
	 */

	(void) md->pmd_release_pmc(cpu, ri, pm);

	/*
	 * Update row disposition
	 */

	if (PMC_IS_SYSTEM_MODE(pm->pm_mode))
		PMC_UNMARK_ROW_STANDALONE(ri);
	else
		PMC_UNMARK_ROW_THREAD(ri);

	/* unlink from the owner's list */
	if (pm->pm_owner)
		pmc_unlink_owner(pm);

	pmc_destroy_pmc_descriptor(pm);
}

/*
 * Register an owner and a pmc.
 */

static int
pmc_register_owner(struct proc *p, struct pmc *pmc)
{
	struct pmc_list	*pl;
	struct pmc_owner *po;

	sx_assert(&pmc_sx, SX_XLOCKED);

	MALLOC(pl, struct pmc_list *, sizeof(struct pmc_list), M_PMC,
	    M_WAITOK);

	if (pl == NULL)
		return ENOMEM;

	if ((po = pmc_find_owner_descriptor(p)) == NULL) {
		if ((po = pmc_allocate_owner_descriptor(p)) == NULL) {
			FREE(pl, M_PMC);
			return ENOMEM;
		}
		po->po_flags |= PMC_FLAG_IS_OWNER; /* real owner */
	}

	if (pmc->pm_mode == PMC_MODE_TS) {
		/* can have only one TS mode PMC per process */
		if (po->po_flags & PMC_FLAG_HAS_TS_PMC) {
			FREE(pl, M_PMC);
			return EINVAL;
		}
		po->po_flags |= PMC_FLAG_HAS_TS_PMC;
	}

	KASSERT(pmc->pm_owner == NULL,
	    ("[pmc,%d] attempting to own an initialized PMC", __LINE__));
	pmc->pm_owner  = po;

	pl->pl_pmc = pmc;

	LIST_INSERT_HEAD(&po->po_pmcs, pl, pl_next);

	PROC_LOCK(p);
	p->p_flag |= P_HWPMC;
	PROC_UNLOCK(p);

	PMCDBG(PMC,REG,1, "register-owner pmc-owner=%p pl=%p pmc=%p",
	    po, pl, pmc);

	return 0;
}

/*
 * Return the current row disposition:
 * == 0 => FREE
 *  > 0 => PROCESS MODE
 *  < 0 => SYSTEM MODE
 */

int
pmc_getrowdisp(int ri)
{
	return pmc_pmcdisp[ri];
}

/*
 * Check if a PMC at row index 'ri' can be allocated to the current
 * process.
 *
 * Allocation can fail if:
 *   - the current process is already being profiled by a PMC at index 'ri',
 *     attached to it via OP_PMCATTACH.
 *   - the current process has already allocated a PMC at index 'ri'
 *     via OP_ALLOCATE.
 */

static int
pmc_can_allocate_rowindex(struct proc *p, unsigned int ri)
{
	struct pmc_list *pl;
	struct pmc_owner *po;
	struct pmc_process *pp;

	PMCDBG(PMC,ALR,1, "can-allocate-rowindex proc=%p (%d, %s) ri=%d",
	    p, p->p_pid, p->p_comm, ri);

	/* we shouldn't have allocated a PMC at row index 'ri' */
	if ((po = pmc_find_owner_descriptor(p)) != NULL)
		LIST_FOREACH(pl, &po->po_pmcs, pl_next)
		    if (pl->pl_pmc->pm_rowindex == ri)
			    return EEXIST;

	/* we shouldn't be the target of any PMC ourselves at this index */
	if ((pp = pmc_find_process_descriptor(p, 0)) != NULL)
		if (pp->pp_pmcs[ri].pp_pmc)
			return EEXIST;

	PMCDBG(PMC,ALR,2, "can-allocate-rowindex proc=%p (%d, %s) ri=%d ok",
	    p, p->p_pid, p->p_comm, ri);

	return 0;
}

/*
 * Check if a given PMC at row index 'ri' can be currently used in
 * mode 'mode'.
 */

static int
pmc_can_allocate_row(int ri, enum pmc_mode mode)
{
	enum pmc_disp	disp;

	sx_assert(&pmc_sx, SX_XLOCKED);

	PMCDBG(PMC,ALR,1, "can-allocate-row ri=%d mode=%d", ri, mode);

	if (PMC_IS_SYSTEM_MODE(mode))
		disp = PMC_DISP_STANDALONE;
	else
		disp = PMC_DISP_THREAD;

	/*
	 * check disposition for PMC row 'ri':
	 *
	 * Expected disposition		Row-disposition		Result
	 *
	 * STANDALONE			STANDALONE or FREE	proceed
	 * STANDALONE			THREAD			fail
	 * THREAD			THREAD or FREE		proceed
	 * THREAD			STANDALONE		fail
	 */

	if (!PMC_ROW_DISP_IS_FREE(ri) &&
	    !(disp == PMC_DISP_THREAD && PMC_ROW_DISP_IS_THREAD(ri)) &&
	    !(disp == PMC_DISP_STANDALONE && PMC_ROW_DISP_IS_STANDALONE(ri)))
		return EBUSY;

	/*
	 * All OK
	 */

	PMCDBG(PMC,ALR,2, "can-allocate-row ri=%d mode=%d ok", ri, mode);

	return 0;

}

/*
 * Find a PMC descriptor with user handle 'pmc' for thread 'td'.
 */

static struct pmc *
pmc_find_pmc_descriptor_in_process(struct pmc_owner *po, pmc_id_t pmcid)
{
	struct pmc_list	*pl;

	KASSERT(pmcid < md->pmd_npmc,
	    ("[pmc,%d] Illegal pmc index %d (max %d)", __LINE__, pmcid,
		md->pmd_npmc));

	LIST_FOREACH(pl, &po->po_pmcs, pl_next)
	    if (pl->pl_pmc->pm_rowindex == pmcid)
		    return pl->pl_pmc;

	return NULL;
}

static int
pmc_find_pmc(pmc_id_t pmcid, struct pmc **pmc)
{

	struct pmc *pm;
	struct pmc_owner *po;

	PMCDBG(PMC,FND,1, "find-pmc id=%d", pmcid);

	if ((po = pmc_find_owner_descriptor(curthread->td_proc)) == NULL)
		return ESRCH;

	if ((pm = pmc_find_pmc_descriptor_in_process(po, pmcid)) == NULL)
		return EINVAL;

	PMCDBG(PMC,FND,2, "find-pmc id=%d -> pmc=%p", pmcid, pm);

	*pmc = pm;
	return 0;
}

/*
 * Start a PMC.
 */

static int
pmc_start(struct pmc *pm)
{
	int error, cpu, ri;
	struct pmc_binding pb;

	KASSERT(pm != NULL,
	    ("[pmc,%d] null pm", __LINE__));

	PMCDBG(PMC,OPS,1, "start pmc=%p mode=%d ri=%d", pm, pm->pm_mode,
	    pm->pm_rowindex);

	pm->pm_state = PMC_STATE_RUNNING;

	if (PMC_IS_VIRTUAL_MODE(pm->pm_mode)) {

		/*
		 * If a PMCATTACH hadn't been done on this
		 * PMC, attach this PMC to its owner process.
		 */

		if (LIST_EMPTY(&pm->pm_targets))
			return pmc_attach_process(pm->pm_owner->po_owner, pm);


		/*
		 * Nothing further to be done; thread context switch code
		 * will start/stop the PMC as appropriate.
		 */

		return 0;

	}

	/*
	 * A system-mode PMC.  Move to the CPU associated with this
	 * PMC, and start the hardware.
	 */

	pmc_save_cpu_binding(&pb);

	cpu = pm->pm_gv.pm_cpu;

	if (pmc_cpu_is_disabled(cpu))
		return ENXIO;

	ri  = pm->pm_rowindex;

	pmc_select_cpu(cpu);

	/*
	 * global PMCs are configured at allocation time
	 * so write out the initial value and start the PMC.
	 */

	if ((error = md->pmd_write_pmc(cpu, ri,
		 PMC_IS_SAMPLING_MODE(pm->pm_mode) ?
		 pm->pm_sc.pm_reloadcount :
		 pm->pm_sc.pm_initial)) == 0)
		error = md->pmd_start_pmc(cpu, ri);

	pmc_restore_cpu_binding(&pb);

	return error;
}

/*
 * Stop a PMC.
 */

static int
pmc_stop(struct pmc *pm)
{
	int error, cpu;
	struct pmc_binding pb;

	KASSERT(pm != NULL, ("[pmc,%d] null pmc", __LINE__));

	PMCDBG(PMC,OPS,1, "stop pmc=%p mode=%d ri=%d", pm, pm->pm_mode,
	    pm->pm_rowindex);

	pm->pm_state = PMC_STATE_STOPPED;

	/*
	 * If the PMC is a virtual mode one, changing the state to
	 * non-RUNNING is enough to ensure that the PMC never gets
	 * scheduled.
	 *
	 * If this PMC is current running on a CPU, then it will
	 * handled correctly at the time its target process is context
	 * switched out.
	 */

	if (PMC_IS_VIRTUAL_MODE(pm->pm_mode))
		return 0;

	/*
	 * A system-mode PMC.  Move to the CPU associated with
	 * this PMC, and stop the hardware.  We update the
	 * 'initial count' so that a subsequent PMCSTART will
	 * resume counting from the current hardware count.
	 */

	pmc_save_cpu_binding(&pb);

	cpu = pm->pm_gv.pm_cpu;

	if (pmc_cpu_is_disabled(cpu))
		return ENXIO;

	pmc_select_cpu(cpu);

	if ((error = md->pmd_stop_pmc(cpu, pm->pm_rowindex)) == 0)
		error = md->pmd_read_pmc(cpu, pm->pm_rowindex,
		    &pm->pm_sc.pm_initial);

	pmc_restore_cpu_binding(&pb);

	return error;
}


#if	DEBUG
static const char *pmc_op_to_name[] = {
#undef	__PMC_OP
#define	__PMC_OP(N, D)	#N ,
	__PMC_OPS()
	NULL
};
#endif

/*
 * The syscall interface
 */

#define	PMC_GET_SX_XLOCK(...) do {		\
	sx_xlock(&pmc_sx);			\
	if (pmc_hook == NULL) {			\
		sx_xunlock(&pmc_sx);		\
		return __VA_ARGS__;		\
	}					\
} while (0)

#define	PMC_DOWNGRADE_SX() do {			\
	sx_downgrade(&pmc_sx);			\
	is_sx_downgraded = 1;			\
} while (0)

static int
pmc_syscall_handler(struct thread *td, void *syscall_args)
{
	int error, is_sx_downgraded, op;
	struct pmc_syscall_args *c;
	void *arg;

	PMC_GET_SX_XLOCK(ENOSYS);

	is_sx_downgraded = 0;

	c = (struct pmc_syscall_args *) syscall_args;

	op = c->pmop_code;
	arg = c->pmop_data;

	PMCDBG(MOD,PMS,1, "syscall op=%d \"%s\" arg=%p", op,
	    pmc_op_to_name[op], arg);

	error = 0;
	atomic_add_int(&pmc_stats.pm_syscalls, 1);

	switch(op)
	{


	/*
	 * Configure a log file.
	 *
	 * XXX This OP will be reworked.
	 */

	case PMC_OP_CONFIGURELOG:
	{
		struct pmc_owner *po;
		struct pmc_op_configurelog cl;
		struct proc *p;

		sx_assert(&pmc_sx, SX_XLOCKED);

		if ((error = copyin(arg, &cl, sizeof(cl))) != 0)
			break;

		/* mark this process as owning a log file */
		p = td->td_proc;
		if ((po = pmc_find_owner_descriptor(p)) == NULL)
			if ((po = pmc_allocate_owner_descriptor(p)) == NULL)
				return ENOMEM;

		if ((error = pmc_configure_log(po, cl.pm_logfd)) != 0)
			break;

	}
	break;


	/*
	 * Retrieve hardware configuration.
	 */

	case PMC_OP_GETCPUINFO:	/* CPU information */
	{
		struct pmc_op_getcpuinfo gci;

		gci.pm_cputype = md->pmd_cputype;
		gci.pm_npmc    = md->pmd_npmc;
		gci.pm_nclass  = md->pmd_nclass;
		bcopy(md->pmd_classes, &gci.pm_classes,
		    sizeof(gci.pm_classes));
		gci.pm_ncpu    = mp_ncpus;
		error = copyout(&gci, arg, sizeof(gci));
	}
	break;


	/*
	 * Get module statistics
	 */

	case PMC_OP_GETDRIVERSTATS:
	{
		struct pmc_op_getdriverstats gms;

		bcopy(&pmc_stats, &gms, sizeof(gms));
		error = copyout(&gms, arg, sizeof(gms));
	}
	break;


	/*
	 * Retrieve module version number
	 */

	case PMC_OP_GETMODULEVERSION:
	{
		error = copyout(&_pmc_version.mv_version, arg, sizeof(int));
	}
	break;


	/*
	 * Retrieve the state of all the PMCs on a given
	 * CPU.
	 */

	case PMC_OP_GETPMCINFO:
	{
		uint32_t cpu, n, npmc;
		size_t pmcinfo_size;
		struct pmc *pm;
		struct pmc_info *p, *pmcinfo;
		struct pmc_op_getpmcinfo *gpi;
		struct pmc_owner *po;
		struct pmc_binding pb;

		PMC_DOWNGRADE_SX();

		gpi = (struct pmc_op_getpmcinfo *) arg;

		if ((error = copyin(&gpi->pm_cpu, &cpu, sizeof(cpu))) != 0)
			break;

		if (cpu >= (unsigned int) mp_ncpus) {
			error = EINVAL;
			break;
		}

		if (pmc_cpu_is_disabled(cpu)) {
			error = ENXIO;
			break;
		}

		/* switch to CPU 'cpu' */
		pmc_save_cpu_binding(&pb);
		pmc_select_cpu(cpu);

		npmc = md->pmd_npmc;

		pmcinfo_size = npmc * sizeof(struct pmc_info);
		MALLOC(pmcinfo, struct pmc_info *, pmcinfo_size, M_PMC,
		    M_WAITOK);

		p = pmcinfo;

		for (n = 0; n < md->pmd_npmc; n++, p++) {

			if ((error = md->pmd_describe(cpu, n, p, &pm)) != 0)
				break;

			if (PMC_ROW_DISP_IS_STANDALONE(n))
				p->pm_rowdisp = PMC_DISP_STANDALONE;
			else if (PMC_ROW_DISP_IS_THREAD(n))
				p->pm_rowdisp = PMC_DISP_THREAD;
			else
				p->pm_rowdisp = PMC_DISP_FREE;

			p->pm_ownerpid = -1;

			if (pm == NULL)	/* no PMC associated */
				continue;

			po = pm->pm_owner;

			KASSERT(po->po_owner != NULL,
			    ("[pmc,%d] pmc_owner had a null proc pointer",
				__LINE__));

			p->pm_ownerpid = po->po_owner->p_pid;
			p->pm_mode     = pm->pm_mode;
			p->pm_event    = pm->pm_event;
			p->pm_flags    = pm->pm_flags;

			if (PMC_IS_SAMPLING_MODE(pm->pm_mode))
				p->pm_reloadcount =
				    pm->pm_sc.pm_reloadcount;
		}

		pmc_restore_cpu_binding(&pb);

		/* now copy out the PMC info collected */
		if (error == 0)
			error = copyout(pmcinfo, &gpi->pm_pmcs, pmcinfo_size);

		FREE(pmcinfo, M_PMC);
	}
	break;


	/*
	 * Set the administrative state of a PMC.  I.e. whether
	 * the PMC is to be used or not.
	 */

	case PMC_OP_PMCADMIN:
	{
		int cpu, ri;
		enum pmc_state request;
		struct pmc_cpu *pc;
		struct pmc_hw *phw;
		struct pmc_op_pmcadmin pma;
		struct pmc_binding pb;

		sx_assert(&pmc_sx, SX_XLOCKED);

		KASSERT(td == curthread,
		    ("[pmc,%d] td != curthread", __LINE__));

		if (suser(td) || jailed(td->td_ucred)) {
			error =  EPERM;
			break;
		}

		if ((error = copyin(arg, &pma, sizeof(pma))) != 0)
			break;

		cpu = pma.pm_cpu;

		if (cpu < 0 || cpu >= mp_ncpus) {
			error = EINVAL;
			break;
		}

		if (pmc_cpu_is_disabled(cpu)) {
			error = ENXIO;
			break;
		}

		request = pma.pm_state;

		if (request != PMC_STATE_DISABLED &&
		    request != PMC_STATE_FREE) {
			error = EINVAL;
			break;
		}

		ri = pma.pm_pmc; /* pmc id == row index */
		if (ri < 0 || ri >= (int) md->pmd_npmc) {
			error = EINVAL;
			break;
		}

		/*
		 * We can't disable a PMC with a row-index allocated
		 * for process virtual PMCs.
		 */

		if (PMC_ROW_DISP_IS_THREAD(ri) &&
		    request == PMC_STATE_DISABLED) {
			error = EBUSY;
			break;
		}

		/*
		 * otherwise, this PMC on this CPU is either free or
		 * in system-wide mode.
		 */

		pmc_save_cpu_binding(&pb);
		pmc_select_cpu(cpu);

		pc  = pmc_pcpu[cpu];
		phw = pc->pc_hwpmcs[ri];

		/*
		 * XXX do we need some kind of 'forced' disable?
		 */

		if (phw->phw_pmc == NULL) {
			if (request == PMC_STATE_DISABLED &&
			    (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED)) {
				phw->phw_state &= ~PMC_PHW_FLAG_IS_ENABLED;
				PMC_MARK_ROW_STANDALONE(ri);
			} else if (request == PMC_STATE_FREE &&
			    (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) == 0) {
				phw->phw_state |=  PMC_PHW_FLAG_IS_ENABLED;
				PMC_UNMARK_ROW_STANDALONE(ri);
			}
			/* other cases are a no-op */
		} else
			error = EBUSY;

		pmc_restore_cpu_binding(&pb);
	}
	break;


	/*
	 * Allocate a PMC.
	 */

	case PMC_OP_PMCALLOCATE:
	{
		uint32_t caps;
		u_int cpu;
		int n;
		enum pmc_mode mode;
		struct pmc *pmc;
		struct pmc_op_pmcallocate pa;
		struct pmc_binding pb;

		if ((error = copyin(arg, &pa, sizeof(pa))) != 0)
			break;

		caps = pa.pm_caps;
		mode = pa.pm_mode;
		cpu  = pa.pm_cpu;

		if ((mode != PMC_MODE_SS  &&  mode != PMC_MODE_SC  &&
		     mode != PMC_MODE_TS  &&  mode != PMC_MODE_TC) ||
		    (cpu != (u_int) PMC_CPU_ANY && cpu >= (u_int) mp_ncpus)) {
			error = EINVAL;
			break;
		}

		/*
		 * Virtual PMCs should only ask for a default CPU.
		 * System mode PMCs need to specify a non-default CPU.
		 */

		if ((PMC_IS_VIRTUAL_MODE(mode) && cpu != (u_int) PMC_CPU_ANY) ||
		    (PMC_IS_SYSTEM_MODE(mode) && cpu == (u_int) PMC_CPU_ANY)) {
			error = EINVAL;
			break;
		}

		/*
		 * Check that a disabled CPU is not being asked for.
		 */

		if (PMC_IS_SYSTEM_MODE(mode) && pmc_cpu_is_disabled(cpu)) {
			error = ENXIO;
			break;
		}

		/*
		 * Refuse an allocation for a system-wide PMC if this
		 * process has been jailed, or if this process lacks
		 * super-user credentials and the sysctl tunable
		 * 'security.bsd.unprivileged_syspmcs' is zero.
		 */

		if (PMC_IS_SYSTEM_MODE(mode)) {
			if (jailed(curthread->td_ucred))
				error = EPERM;
			else if (suser(curthread) &&
			    (pmc_unprivileged_syspmcs == 0))
				error = EPERM;
		}

		if (error)
			break;

		/*
		 * Look for valid values for 'pm_flags'
		 */

		if ((pa.pm_flags & ~(PMC_F_DESCENDANTS|PMC_F_LOG_TC_CSW))
		    != 0) {
			error = EINVAL;
			break;
		}

		/*
		 * All sampling mode PMCs need to be able to interrupt the
		 * CPU.
		 */

		if (PMC_IS_SAMPLING_MODE(mode)) {
			caps |= PMC_CAP_INTERRUPT;
			error = ENOSYS; /* for snapshot 6 */
			break;
		}

		PMCDBG(PMC,ALL,2, "event=%d caps=0x%x mode=%d cpu=%d",
		    pa.pm_ev, caps, mode, cpu);

		pmc = pmc_allocate_pmc_descriptor();
		pmc->pm_event = pa.pm_ev;
		pmc->pm_class = pa.pm_class;
		pmc->pm_state = PMC_STATE_FREE;
		pmc->pm_mode  = mode;
		pmc->pm_caps  = caps;
		pmc->pm_flags = pa.pm_flags;

		/* switch thread to CPU 'cpu' */
		pmc_save_cpu_binding(&pb);

#define	PMC_IS_SHAREABLE_PMC(cpu, n)				\
	(pmc_pcpu[(cpu)]->pc_hwpmcs[(n)]->phw_state &		\
	 PMC_PHW_FLAG_IS_SHAREABLE)
#define	PMC_IS_UNALLOCATED(cpu, n)				\
	(pmc_pcpu[(cpu)]->pc_hwpmcs[(n)]->phw_pmc == NULL)

		if (PMC_IS_SYSTEM_MODE(mode)) {
			pmc_select_cpu(cpu);
			for (n = 0; n < (int) md->pmd_npmc; n++)
				if (pmc_can_allocate_row(n, mode) == 0 &&
				    pmc_can_allocate_rowindex(
					    curthread->td_proc, n) == 0 &&
				    (PMC_IS_UNALLOCATED(cpu, n) ||
				     PMC_IS_SHAREABLE_PMC(cpu, n)) &&
				    md->pmd_allocate_pmc(cpu, n, pmc,
					&pa) == 0)
					break;
		} else {
			/* Process virtual mode */
			for (n = 0; n < (int) md->pmd_npmc; n++) {
				if (pmc_can_allocate_row(n, mode) == 0 &&
				    pmc_can_allocate_rowindex(
					    curthread->td_proc, n) == 0 &&
				    md->pmd_allocate_pmc(curthread->td_oncpu,
					n, pmc, &pa) == 0)
					break;
			}
		}

#undef	PMC_IS_UNALLOCATED
#undef	PMC_IS_SHAREABLE_PMC

		pmc_restore_cpu_binding(&pb);

		if (n == (int) md->pmd_npmc) {
			pmc_destroy_pmc_descriptor(pmc);
			FREE(pmc, M_PMC);
			pmc = NULL;
			error = EINVAL;
			break;
		}

		PMCDBG(PMC,ALL,2, "ev=%d class=%d mode=%d -> n=%d",
		    pmc->pm_event, pmc->pm_class, pmc->pm_mode, n);

		/*
		 * Configure global pmc's immediately
		 */

		if (PMC_IS_SYSTEM_MODE(pmc->pm_mode))
			if ((error = md->pmd_config_pmc(cpu, n, pmc)) != 0) {
				(void) md->pmd_release_pmc(cpu, n, pmc);
				pmc_destroy_pmc_descriptor(pmc);
				FREE(pmc, M_PMC);
				pmc = NULL;
				break;
			}

		/*
		 * Mark the row index allocated.
		 */

		pmc->pm_rowindex = n;
		pmc->pm_state    = PMC_STATE_ALLOCATED;

		/*
		 * mark row disposition
		 */

		if (PMC_IS_SYSTEM_MODE(mode))
			PMC_MARK_ROW_STANDALONE(n);
		else
			PMC_MARK_ROW_THREAD(n);

		/*
		 * If this is a system-wide CPU, mark the CPU it
		 * was allocated on.
		 */

		if (PMC_IS_SYSTEM_MODE(mode))
			pmc->pm_gv.pm_cpu = cpu;

		/*
		 * Register this PMC with the current thread as its owner.
		 */

		if ((error =
		    pmc_register_owner(curthread->td_proc, pmc)) != 0) {
			pmc_release_pmc_descriptor(pmc);
			FREE(pmc, M_PMC);
			pmc = NULL;
			break;
		}

		/*
		 * Return the allocated index.
		 */

		pa.pm_pmcid = n;

		error = copyout(&pa, arg, sizeof(pa));
	}
	break;


	/*
	 * Attach a PMC to a process.
	 */

	case PMC_OP_PMCATTACH:
	{
		struct pmc *pm;
		struct proc *p;
		struct pmc_op_pmcattach a;

		sx_assert(&pmc_sx, SX_XLOCKED);

		if ((error = copyin(arg, &a, sizeof(a))) != 0)
			break;

		if (a.pm_pid < 0) {
			error = EINVAL;
			break;
		} else if (a.pm_pid == 0)
			a.pm_pid = td->td_proc->p_pid;

		if ((error = pmc_find_pmc(a.pm_pmc, &pm)) != 0)
			break;

		if (PMC_IS_SYSTEM_MODE(pm->pm_mode)) {
			error = EINVAL;
			break;
		}

		/* PMCs may be (re)attached only when allocated or stopped */
		if (pm->pm_state == PMC_STATE_RUNNING) {
			error = EBUSY;
			break;
		} else if (pm->pm_state != PMC_STATE_ALLOCATED &&
		    pm->pm_state != PMC_STATE_STOPPED) {
			error = EINVAL;
			break;
		}

		/* lookup pid */
		if ((p = pfind(a.pm_pid)) == NULL) {
			error = ESRCH;
			break;
		}

		/*
		 * Ignore processes that are working on exiting.
		 */
		if (p->p_flag & P_WEXIT) {
			error = ESRCH;
			PROC_UNLOCK(p);	/* pfind() returns a locked process */
			break;
		}

		/*
		 * we are allowed to attach a PMC to a process if
		 * we can debug it.
		 */
		error = p_candebug(curthread, p);

		PROC_UNLOCK(p);

		if (error == 0)
			error = pmc_attach_process(p, pm);
	}
	break;


	/*
	 * Detach an attached PMC from a process.
	 */

	case PMC_OP_PMCDETACH:
	{
		struct pmc *pm;
		struct proc *p;
		struct pmc_op_pmcattach a;

		if ((error = copyin(arg, &a, sizeof(a))) != 0)
			break;

		if (a.pm_pid < 0) {
			error = EINVAL;
			break;
		} else if (a.pm_pid == 0)
			a.pm_pid = td->td_proc->p_pid;

		if ((error = pmc_find_pmc(a.pm_pmc, &pm)) != 0)
			break;

		if ((p = pfind(a.pm_pid)) == NULL) {
			error = ESRCH;
			break;
		}

		/*
		 * Treat processes that are in the process of exiting
		 * as if they were not present.
		 */

		if (p->p_flag & P_WEXIT)
			error = ESRCH;

		PROC_UNLOCK(p);	/* pfind() returns a locked process */

		if (error == 0)
			error = pmc_detach_process(p, pm);
	}
	break;


	/*
	 * Release an allocated PMC
	 */

	case PMC_OP_PMCRELEASE:
	{
		pmc_id_t pmcid;
		struct pmc *pm;
		struct pmc_owner *po;
		struct pmc_op_simple sp;

		/*
		 * Find PMC pointer for the named PMC.
		 *
		 * Use pmc_release_pmc_descriptor() to switch off the
		 * PMC, remove all its target threads, and remove the
		 * PMC from its owner's list.
		 *
		 * Remove the owner record if this is the last PMC
		 * owned.
		 *
		 * Free up space.
		 */

		if ((error = copyin(arg, &sp, sizeof(sp))) != 0)
			break;

		pmcid = sp.pm_pmcid;

		if ((error = pmc_find_pmc(pmcid, &pm)) != 0)
			break;

		po = pm->pm_owner;
		pmc_release_pmc_descriptor(pm);
		pmc_maybe_remove_owner(po);

		FREE(pm, M_PMC);
	}
	break;


	/*
	 * Read and/or write a PMC.
	 */

	case PMC_OP_PMCRW:
	{
		uint32_t cpu, ri;
		struct pmc *pm;
		struct pmc_op_pmcrw *pprw;
		struct pmc_op_pmcrw prw;
		struct pmc_binding pb;
		pmc_value_t oldvalue;

		PMC_DOWNGRADE_SX();

		if ((error = copyin(arg, &prw, sizeof(prw))) != 0)
			break;

		ri = 0;
		PMCDBG(PMC,OPS,1, "rw id=%d flags=0x%x", prw.pm_pmcid,
		    prw.pm_flags);

		/* must have at least one flag set */
		if ((prw.pm_flags & (PMC_F_OLDVALUE|PMC_F_NEWVALUE)) == 0) {
			error = EINVAL;
			break;
		}

		/* locate pmc descriptor */
		if ((error = pmc_find_pmc(prw.pm_pmcid, &pm)) != 0)
			break;

		/* Can't read a PMC that hasn't been started. */
		if (pm->pm_state != PMC_STATE_ALLOCATED &&
		    pm->pm_state != PMC_STATE_STOPPED &&
		    pm->pm_state != PMC_STATE_RUNNING) {
			error = EINVAL;
			break;
		}

		/* writing a new value is allowed only for 'STOPPED' pmcs */
		if (pm->pm_state == PMC_STATE_RUNNING &&
		    (prw.pm_flags & PMC_F_NEWVALUE)) {
			error = EBUSY;
			break;
		}

		if (PMC_IS_VIRTUAL_MODE(pm->pm_mode)) {

			/* read/write the saved value in the PMC record */
			mtx_pool_lock_spin(pmc_mtxpool, pm);
			if (prw.pm_flags & PMC_F_OLDVALUE)
				oldvalue = pm->pm_gv.pm_savedvalue;
			if (prw.pm_flags & PMC_F_NEWVALUE)
				pm->pm_gv.pm_savedvalue = prw.pm_value;
			mtx_pool_unlock_spin(pmc_mtxpool, pm);

		} else { /* System mode PMCs */
			cpu = pm->pm_gv.pm_cpu;
			ri  = pm->pm_rowindex;

			if (pmc_cpu_is_disabled(cpu)) {
				error = ENXIO;
				break;
			}

			/* move this thread to CPU 'cpu' */
			pmc_save_cpu_binding(&pb);
			pmc_select_cpu(cpu);

			/* save old value */
			if (prw.pm_flags & PMC_F_OLDVALUE)
				if ((error = (*md->pmd_read_pmc)(cpu, ri,
					 &oldvalue)))
					goto error;
			/* write out new value */
			if (prw.pm_flags & PMC_F_NEWVALUE)
				error = (*md->pmd_write_pmc)(cpu, ri,
				    prw.pm_value);
		error:
			pmc_restore_cpu_binding(&pb);
			if (error)
				break;
		}

		pprw = (struct pmc_op_pmcrw *) arg;

#if	DEBUG
		if (prw.pm_flags & PMC_F_NEWVALUE)
			PMCDBG(PMC,OPS,2, "rw id=%d new %jx -> old %jx",
			    ri, prw.pm_value, oldvalue);
		else
			PMCDBG(PMC,OPS,2, "rw id=%d -> old %jx", ri, oldvalue);
#endif

		/* return old value if requested */
		if (prw.pm_flags & PMC_F_OLDVALUE)
			if ((error = copyout(&oldvalue, &pprw->pm_value,
				 sizeof(prw.pm_value))))
				break;

		/*
		 * send a signal (SIGIO) to the owner if it is trying to read
		 * a PMC with no target processes attached.
		 */

		if (LIST_EMPTY(&pm->pm_targets) &&
		    (prw.pm_flags & PMC_F_OLDVALUE)) {
			PROC_LOCK(curthread->td_proc);
			psignal(curthread->td_proc, SIGIO);
			PROC_UNLOCK(curthread->td_proc);
		}
	}
	break;


	/*
	 * Set the sampling rate for a sampling mode PMC and the
	 * initial count for a counting mode PMC.
	 */

	case PMC_OP_PMCSETCOUNT:
	{
		struct pmc *pm;
		struct pmc_op_pmcsetcount sc;

		PMC_DOWNGRADE_SX();

		if ((error = copyin(arg, &sc, sizeof(sc))) != 0)
			break;

		if ((error = pmc_find_pmc(sc.pm_pmcid, &pm)) != 0)
			break;

		if (pm->pm_state == PMC_STATE_RUNNING) {
			error = EBUSY;
			break;
		}

		if (PMC_IS_SAMPLING_MODE(pm->pm_mode))
			pm->pm_sc.pm_reloadcount = sc.pm_count;
		else
			pm->pm_sc.pm_initial = sc.pm_count;
	}
	break;


	/*
	 * Start a PMC.
	 */

	case PMC_OP_PMCSTART:
	{
		pmc_id_t pmcid;
		struct pmc *pm;
		struct pmc_op_simple sp;

		sx_assert(&pmc_sx, SX_XLOCKED);

		if ((error = copyin(arg, &sp, sizeof(sp))) != 0)
			break;

		pmcid = sp.pm_pmcid;

		if ((error = pmc_find_pmc(pmcid, &pm)) != 0)
			break;

		KASSERT(pmcid == pm->pm_rowindex,
		    ("[pmc,%d] row index %d != id %d", __LINE__,
			pm->pm_rowindex, pmcid));

		if (pm->pm_state == PMC_STATE_RUNNING) /* already running */
			break;
		else if (pm->pm_state != PMC_STATE_STOPPED &&
		    pm->pm_state != PMC_STATE_ALLOCATED) {
			error = EINVAL;
			break;
		}

		error = pmc_start(pm);
	}
	break;


	/*
	 * Stop a PMC.
	 */

	case PMC_OP_PMCSTOP:
	{
		pmc_id_t pmcid;
		struct pmc *pm;
		struct pmc_op_simple sp;

		PMC_DOWNGRADE_SX();

		if ((error = copyin(arg, &sp, sizeof(sp))) != 0)
			break;

		pmcid = sp.pm_pmcid;

		/*
		 * Mark the PMC as inactive and invoke the MD stop
		 * routines if needed.
		 */

		if ((error = pmc_find_pmc(pmcid, &pm)) != 0)
			break;

		KASSERT(pmcid == pm->pm_rowindex,
		    ("[pmc,%d] row index %d != pmcid %d", __LINE__,
			pm->pm_rowindex, pmcid));

		if (pm->pm_state == PMC_STATE_STOPPED) /* already stopped */
			break;
		else if (pm->pm_state != PMC_STATE_RUNNING) {
			error = EINVAL;
			break;
		}

		error = pmc_stop(pm);
	}
	break;


	/*
	 * Write a user-entry to the log file.
	 */

	case PMC_OP_WRITELOG:
	{

		PMC_DOWNGRADE_SX();

		/*
		 * flush all per-cpu hash tables
		 * append user-log entry
		 */

		error = ENOSYS;
	}
	break;


#if __i386__ || __amd64__

	/*
	 * Machine dependent operation for i386-class processors.
	 *
	 * Retrieve the MSR number associated with the counter
	 * 'pmc_id'.  This allows processes to directly use RDPMC
	 * instructions to read their PMCs, without the overhead of a
	 * system call.
	 */

	case PMC_OP_PMCX86GETMSR:
	{
		int ri;
		struct pmc	*pm;
		struct pmc_op_x86_getmsr gm;

		PMC_DOWNGRADE_SX();

		/* CPU has no 'GETMSR' support */
		if (md->pmd_get_msr == NULL) {
			error = ENOSYS;
			break;
		}

		if ((error = copyin(arg, &gm, sizeof(gm))) != 0)
			break;

		if ((error = pmc_find_pmc(gm.pm_pmcid, &pm)) != 0)
			break;

		/*
		 * The allocated PMC needs to be a process virtual PMC,
		 * i.e., of type T[CS].
		 *
		 * Global PMCs can only be read using the PMCREAD
		 * operation since they may be allocated on a
		 * different CPU than the one we could be running on
		 * at the time of the read.
		 */

		if (!PMC_IS_VIRTUAL_MODE(pm->pm_mode)) {
			error = EINVAL;
			break;
		}

		ri = pm->pm_rowindex;

		if ((error = (*md->pmd_get_msr)(ri, &gm.pm_msr)) < 0)
			break;
		if ((error = copyout(&gm, arg, sizeof(gm))) < 0)
			break;
	}
	break;
#endif

	default:
		error = EINVAL;
		break;
	}

	if (is_sx_downgraded)
		sx_sunlock(&pmc_sx);
	else
		sx_xunlock(&pmc_sx);

	if (error)
		atomic_add_int(&pmc_stats.pm_syscall_errors, 1);

	return error;
}

/*
 * Helper functions
 */

/*
 * Configure a log file.
 */

static int
pmc_configure_log(struct pmc_owner *po, int logfd)
{
	struct proc *p;

	return ENOSYS; /* for now */

	p = po->po_owner;

	if (po->po_logfd < 0 && logfd < 0) /* nothing to do */
		return 0;

	if (po->po_logfd >= 0 && logfd < 0) {
		/* deconfigure log */
		/* XXX */
		po->po_flags &= ~PMC_FLAG_OWNS_LOGFILE;
		pmc_maybe_remove_owner(po);

	} else if (po->po_logfd < 0 && logfd >= 0) {
		/* configure log file */
		/* XXX */
		po->po_flags |= PMC_FLAG_OWNS_LOGFILE;

		/* mark process as using HWPMCs */
		PROC_LOCK(p);
		p->p_flag |= P_HWPMC;
		PROC_UNLOCK(p);
	} else
		return EBUSY;

	return 0;
}

/*
 * Log an exit event to the PMC owner's log file.
 */

static void
pmc_log_process_exit(struct pmc *pm, struct pmc_process *pp)
{
	KASSERT(pm->pm_flags & PMC_F_LOG_TC_PROCEXIT,
	    ("[pmc,%d] log-process-exit called gratuitously", __LINE__));

	(void) pm;
	(void) pp;

	return;
}

/*
 * Event handlers.
 */

/*
 * Handle a process exit.
 *
 * XXX This eventhandler gets called early in the exit process.
 * Consider using a 'hook' invocation from thread_exit() or equivalent
 * spot.  Another negative is that kse_exit doesn't seem to call
 * exit1() [??].
 */

static void
pmc_process_exit(void *arg __unused, struct proc *p)
{
	int is_using_hwpmcs;

	PROC_LOCK(p);
	is_using_hwpmcs = p->p_flag & P_HWPMC;
	PROC_UNLOCK(p);

	if (is_using_hwpmcs) {
		PMCDBG(PRC,EXT,1,"process-exit proc=%p (%d, %s)", p, p->p_pid,
		    p->p_comm);

		PMC_GET_SX_XLOCK();
		(void) pmc_hook_handler(curthread, PMC_FN_PROCESS_EXIT,
		    (void *) p);
		sx_xunlock(&pmc_sx);
	}
}

/*
 * Handle a process fork.
 *
 * If the parent process 'p1' is under HWPMC monitoring, then copy
 * over any attached PMCs that have 'do_descendants' semantics.
 */

static void
pmc_process_fork(void *arg __unused, struct proc *p1, struct proc *p2,
    int flags)
{
	int is_using_hwpmcs;

	(void) flags;		/* unused parameter */

	PROC_LOCK(p1);
	is_using_hwpmcs = p1->p_flag & P_HWPMC;
	PROC_UNLOCK(p1);

	if (is_using_hwpmcs) {
		PMCDBG(PMC,FRK,1, "process-fork proc=%p (%d, %s)", p1,
		    p1->p_pid, p1->p_comm);
		PMC_GET_SX_XLOCK();
		(void) pmc_hook_handler(curthread, PMC_FN_PROCESS_FORK,
		    (void *) p2);
		sx_xunlock(&pmc_sx);
	}
}


/*
 * initialization
 */

static const char *pmc_name_of_pmcclass[] = {
#undef	__PMC_CLASS
#define	__PMC_CLASS(N) #N ,
	__PMC_CLASSES()
};

static int
pmc_initialize(void)
{
	int error, cpu, n;
	struct pmc_binding pb;

	md = NULL;
	error = 0;

#if	DEBUG
	/* parse debug flags first */
	if (TUNABLE_STR_FETCH(PMC_SYSCTL_NAME_PREFIX "debugflags",
		pmc_debugstr, sizeof(pmc_debugstr)))
		pmc_debugflags_parse(pmc_debugstr,
		    pmc_debugstr+strlen(pmc_debugstr));
#endif

	PMCDBG(MOD,INI,0, "PMC Initialize (version %x)", PMC_VERSION);

	/*
	 * check sysctl parameters
	 */

	if (pmc_hashsize <= 0) {
		(void) printf("pmc: sysctl variable \""
		    PMC_SYSCTL_NAME_PREFIX "hashsize\" must be greater than "
		    "zero\n");
		pmc_hashsize = PMC_HASH_SIZE;
	}

#if	defined(__i386__)
	/* determine the CPU kind.  This is i386 specific */
	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		md = pmc_amd_initialize();
	else if (strcmp(cpu_vendor, "GenuineIntel") == 0)
		md = pmc_intel_initialize();
	/* XXX: what about the other i386 CPU manufacturers? */
#elif	defined(__amd64__)
	if (strcmp(cpu_vendor, "AuthenticAMD") == 0)
		md = pmc_amd_initialize();
#else  /* other architectures */
	md = NULL;
#endif

	if (md == NULL || md->pmd_init == NULL)
		return ENOSYS;

	/* allocate space for the per-cpu array */
	MALLOC(pmc_pcpu, struct pmc_cpu **, mp_ncpus * sizeof(struct pmc_cpu *),
	    M_PMC, M_WAITOK|M_ZERO);

	/* per-cpu 'saved values' for managing process-mode PMCs */
	MALLOC(pmc_pcpu_saved, pmc_value_t *,
	    sizeof(pmc_value_t) * mp_ncpus * md->pmd_npmc, M_PMC, M_WAITOK);

	/* perform cpu dependent initialization */
	pmc_save_cpu_binding(&pb);
	for (cpu = 0; cpu < mp_ncpus; cpu++) {
		if (pmc_cpu_is_disabled(cpu))
			continue;
		pmc_select_cpu(cpu);
		if ((error = md->pmd_init(cpu)) != 0)
			break;
	}
	pmc_restore_cpu_binding(&pb);

	if (error != 0)
		return error;

	/* allocate space for the row disposition array */
	pmc_pmcdisp = malloc(sizeof(enum pmc_mode) * md->pmd_npmc,
	    M_PMC, M_WAITOK|M_ZERO);

	KASSERT(pmc_pmcdisp != NULL,
	    ("[pmc,%d] pmcdisp allocation returned NULL", __LINE__));

	/* mark all PMCs as available */
	for (n = 0; n < (int) md->pmd_npmc; n++)
		PMC_MARK_ROW_FREE(n);

	/* allocate thread hash tables */
	pmc_ownerhash = hashinit(pmc_hashsize, M_PMC,
	    &pmc_ownerhashmask);

	pmc_processhash = hashinit(pmc_hashsize, M_PMC,
	    &pmc_processhashmask);
	mtx_init(&pmc_processhash_mtx, "pmc-process-hash", "pmc", MTX_SPIN);

	/* allocate a pool of spin mutexes */
	pmc_mtxpool = mtx_pool_create("pmc", pmc_mtxpool_size, MTX_SPIN);

	PMCDBG(MOD,INI,1, "pmc_ownerhash=%p, mask=0x%lx "
	    "targethash=%p mask=0x%lx", pmc_ownerhash, pmc_ownerhashmask,
	    pmc_processhash, pmc_processhashmask);

	/* register process {exit,fork,exec} handlers */
	pmc_exit_tag = EVENTHANDLER_REGISTER(process_exit,
	    pmc_process_exit, NULL, EVENTHANDLER_PRI_ANY);
	pmc_fork_tag = EVENTHANDLER_REGISTER(process_fork,
	    pmc_process_fork, NULL, EVENTHANDLER_PRI_ANY);

	/* set hook functions */
	pmc_intr = md->pmd_intr;
	pmc_hook = pmc_hook_handler;

	if (error == 0) {
		printf(PMC_MODULE_NAME ":");
		for (n = 0; n < (int) md->pmd_nclass; n++)
			printf(" %s(%d)",
			    pmc_name_of_pmcclass[md->pmd_classes[n]],
			    md->pmd_nclasspmcs[n]);
		printf("\n");
	}

	return error;
}

/* prepare to be unloaded */
static void
pmc_cleanup(void)
{
	int cpu;
	struct pmc_ownerhash *ph;
	struct pmc_owner *po, *tmp;
	struct pmc_binding pb;
#if	DEBUG
	struct pmc_processhash *prh;
#endif

	PMCDBG(MOD,INI,0, "%s", "cleanup");

	pmc_intr = NULL;	/* no more interrupts please */

	sx_xlock(&pmc_sx);
	if (pmc_hook == NULL) {	/* being unloaded already */
		sx_xunlock(&pmc_sx);
		return;
	}

	pmc_hook = NULL; /* prevent new threads from entering module */

	/* deregister event handlers */
	EVENTHANDLER_DEREGISTER(process_fork, pmc_fork_tag);
	EVENTHANDLER_DEREGISTER(process_exit, pmc_exit_tag);

	/* send SIGBUS to all owner threads, free up allocations */
	if (pmc_ownerhash)
		for (ph = pmc_ownerhash;
		     ph <= &pmc_ownerhash[pmc_ownerhashmask];
		     ph++) {
			LIST_FOREACH_SAFE(po, ph, po_next, tmp) {
				pmc_remove_owner(po);

				/* send SIGBUS to owner processes */
				PMCDBG(MOD,INI,2, "cleanup signal proc=%p "
				    "(%d, %s)", po->po_owner,
				    po->po_owner->p_pid,
				    po->po_owner->p_comm);

				PROC_LOCK(po->po_owner);
				psignal(po->po_owner, SIGBUS);
				PROC_UNLOCK(po->po_owner);
				FREE(po, M_PMC);
			}
		}

	/* reclaim allocated data structures */
	if (pmc_mtxpool)
		mtx_pool_destroy(&pmc_mtxpool);

	mtx_destroy(&pmc_processhash_mtx);
	if (pmc_processhash) {
#if	DEBUG
		struct pmc_process *pp;

		PMCDBG(MOD,INI,3, "%s", "destroy process hash");
		for (prh = pmc_processhash;
		     prh <= &pmc_processhash[pmc_processhashmask];
		     prh++)
			LIST_FOREACH(pp, prh, pp_next)
			    PMCDBG(MOD,INI,3, "pid=%d", pp->pp_proc->p_pid);
#endif

		hashdestroy(pmc_processhash, M_PMC, pmc_processhashmask);
		pmc_processhash = NULL;
	}

	if (pmc_ownerhash) {
		PMCDBG(MOD,INI,3, "%s", "destroy owner hash");
		hashdestroy(pmc_ownerhash, M_PMC, pmc_ownerhashmask);
		pmc_ownerhash = NULL;
	}

 	/* do processor dependent cleanup */
	PMCDBG(MOD,INI,3, "%s", "md cleanup");
	if (md) {
		pmc_save_cpu_binding(&pb);
		for (cpu = 0; cpu < mp_ncpus; cpu++) {
			PMCDBG(MOD,INI,1,"pmc-cleanup cpu=%d pcs=%p",
			    cpu, pmc_pcpu[cpu]);
			if (pmc_cpu_is_disabled(cpu))
				continue;
			pmc_select_cpu(cpu);
			if (pmc_pcpu[cpu])
				(void) md->pmd_cleanup(cpu);
		}
		FREE(md, M_PMC);
		md = NULL;
		pmc_restore_cpu_binding(&pb);
	}

	/* deallocate per-cpu structures */
	FREE(pmc_pcpu, M_PMC);
	pmc_pcpu = NULL;

	FREE(pmc_pcpu_saved, M_PMC);
	pmc_pcpu_saved = NULL;

	if (pmc_pmcdisp) {
		FREE(pmc_pmcdisp, M_PMC);
		pmc_pmcdisp = NULL;
	}

	sx_xunlock(&pmc_sx); 	/* we are done */
}

/*
 * The function called at load/unload.
 */

static int
load (struct module *module __unused, int cmd, void *arg __unused)
{
	int error;

	error = 0;

	switch (cmd) {
	case MOD_LOAD :
		/* initialize the subsystem */
		error = pmc_initialize();
		if (error != 0)
			break;
		PMCDBG(MOD,INI,1, "syscall=%d ncpus=%d",
		    pmc_syscall_num, mp_ncpus);
		break;


	case MOD_UNLOAD :
	case MOD_SHUTDOWN:
		pmc_cleanup();
		PMCDBG(MOD,INI,1, "%s", "unloaded");
		break;

	default :
		error = EINVAL;	/* XXX should panic(9) */
		break;
	}

	return error;
}

/* memory pool */
MALLOC_DEFINE(M_PMC, "pmc", "Memory space for the PMC module");
