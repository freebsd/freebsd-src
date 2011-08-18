/*-
 * Copyright (c) 1995 Terrence R. Lambert
 * All rights reserved.
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)init_main.c	8.9 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_init_path.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/loginclass.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/cpuset.h>

#include <machine/cpu.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/copyright.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>

void mi_startup(void);				/* Should be elsewhere */

/* Components of the first process -- never freed. */
static struct session session0;
static struct pgrp pgrp0;
struct	proc proc0;
struct	thread thread0 __aligned(16);
struct	vmspace vmspace0;
struct	proc *initproc;

int	boothowto = 0;		/* initialized so that it can be patched */
SYSCTL_INT(_debug, OID_AUTO, boothowto, CTLFLAG_RD, &boothowto, 0,
	"Boot control flags, passed from loader");
int	bootverbose;
SYSCTL_INT(_debug, OID_AUTO, bootverbose, CTLFLAG_RW, &bootverbose, 0,
	"Control the output of verbose kernel messages");

/*
 * This ensures that there is at least one entry so that the sysinit_set
 * symbol is not undefined.  A sybsystem ID of SI_SUB_DUMMY is never
 * executed.
 */
SYSINIT(placeholder, SI_SUB_DUMMY, SI_ORDER_ANY, NULL, NULL);

/*
 * The sysinit table itself.  Items are checked off as the are run.
 * If we want to register new sysinit types, add them to newsysinit.
 */
SET_DECLARE(sysinit_set, struct sysinit);
struct sysinit **sysinit, **sysinit_end;
struct sysinit **newsysinit, **newsysinit_end;

/*
 * Merge a new sysinit set into the current set, reallocating it if
 * necessary.  This can only be called after malloc is running.
 */
void
sysinit_add(struct sysinit **set, struct sysinit **set_end)
{
	struct sysinit **newset;
	struct sysinit **sipp;
	struct sysinit **xipp;
	int count;

	count = set_end - set;
	if (newsysinit)
		count += newsysinit_end - newsysinit;
	else
		count += sysinit_end - sysinit;
	newset = malloc(count * sizeof(*sipp), M_TEMP, M_NOWAIT);
	if (newset == NULL)
		panic("cannot malloc for sysinit");
	xipp = newset;
	if (newsysinit)
		for (sipp = newsysinit; sipp < newsysinit_end; sipp++)
			*xipp++ = *sipp;
	else
		for (sipp = sysinit; sipp < sysinit_end; sipp++)
			*xipp++ = *sipp;
	for (sipp = set; sipp < set_end; sipp++)
		*xipp++ = *sipp;
	if (newsysinit)
		free(newsysinit, M_TEMP);
	newsysinit = newset;
	newsysinit_end = newset + count;
}

/*
 * System startup; initialize the world, create process 0, mount root
 * filesystem, and fork to create init and pagedaemon.  Most of the
 * hard work is done in the lower-level initialization routines including
 * startup(), which does memory initialization and autoconfiguration.
 *
 * This allows simple addition of new kernel subsystems that require
 * boot time initialization.  It also allows substitution of subsystem
 * (for instance, a scheduler, kernel profiler, or VM system) by object
 * module.  Finally, it allows for optional "kernel threads".
 */
void
mi_startup(void)
{

	register struct sysinit **sipp;		/* system initialization*/
	register struct sysinit **xipp;		/* interior loop of sort*/
	register struct sysinit *save;		/* bubble*/

#if defined(VERBOSE_SYSINIT)
	int last;
	int verbose;
#endif

	if (boothowto & RB_VERBOSE)
		bootverbose++;

	if (sysinit == NULL) {
		sysinit = SET_BEGIN(sysinit_set);
		sysinit_end = SET_LIMIT(sysinit_set);
	}

restart:
	/*
	 * Perform a bubble sort of the system initialization objects by
	 * their subsystem (primary key) and order (secondary key).
	 */
	for (sipp = sysinit; sipp < sysinit_end; sipp++) {
		for (xipp = sipp + 1; xipp < sysinit_end; xipp++) {
			if ((*sipp)->subsystem < (*xipp)->subsystem ||
			     ((*sipp)->subsystem == (*xipp)->subsystem &&
			      (*sipp)->order <= (*xipp)->order))
				continue;	/* skip*/
			save = *sipp;
			*sipp = *xipp;
			*xipp = save;
		}
	}

#if defined(VERBOSE_SYSINIT)
	last = SI_SUB_COPYRIGHT;
	verbose = 0;
#if !defined(DDB)
	printf("VERBOSE_SYSINIT: DDB not enabled, symbol lookups disabled.\n");
#endif
#endif

	/*
	 * Traverse the (now) ordered list of system initialization tasks.
	 * Perform each task, and continue on to the next task.
	 *
	 * The last item on the list is expected to be the scheduler,
	 * which will not return.
	 */
	for (sipp = sysinit; sipp < sysinit_end; sipp++) {

		if ((*sipp)->subsystem == SI_SUB_DUMMY)
			continue;	/* skip dummy task(s)*/

		if ((*sipp)->subsystem == SI_SUB_DONE)
			continue;

#if defined(VERBOSE_SYSINIT)
		if ((*sipp)->subsystem > last) {
			verbose = 1;
			last = (*sipp)->subsystem;
			printf("subsystem %x\n", last);
		}
		if (verbose) {
#if defined(DDB)
			const char *name;
			c_db_sym_t sym;
			db_expr_t  offset;

			sym = db_search_symbol((vm_offset_t)(*sipp)->func,
			    DB_STGY_PROC, &offset);
			db_symbol_values(sym, &name, NULL);
			if (name != NULL)
				printf("   %s(%p)... ", name, (*sipp)->udata);
			else
#endif
				printf("   %p(%p)... ", (*sipp)->func,
				    (*sipp)->udata);
		}
#endif

		/* Call function */
		(*((*sipp)->func))((*sipp)->udata);

#if defined(VERBOSE_SYSINIT)
		if (verbose)
			printf("done.\n");
#endif

		/* Check off the one we're just done */
		(*sipp)->subsystem = SI_SUB_DONE;

		/* Check if we've installed more sysinit items via KLD */
		if (newsysinit != NULL) {
			if (sysinit != SET_BEGIN(sysinit_set))
				free(sysinit, M_TEMP);
			sysinit = newsysinit;
			sysinit_end = newsysinit_end;
			newsysinit = NULL;
			newsysinit_end = NULL;
			goto restart;
		}
	}

	panic("Shouldn't get here!");
	/* NOTREACHED*/
}


/*
 ***************************************************************************
 ****
 **** The following SYSINIT's belong elsewhere, but have not yet
 **** been moved.
 ****
 ***************************************************************************
 */
static void
print_caddr_t(void *data)
{
	printf("%s", (char *)data);
}

static void
print_version(void *data __unused)
{
	int len;

	/* Strip a trailing newline from version. */
	len = strlen(version);
	while (len > 0 && version[len - 1] == '\n')
		len--;
	printf("%.*s %s\n", len, version, machine);
}

SYSINIT(announce, SI_SUB_COPYRIGHT, SI_ORDER_FIRST, print_caddr_t,
    copyright);
SYSINIT(trademark, SI_SUB_COPYRIGHT, SI_ORDER_SECOND, print_caddr_t,
    trademark);
SYSINIT(version, SI_SUB_COPYRIGHT, SI_ORDER_THIRD, print_version, NULL);

#ifdef WITNESS
static char wit_warn[] =
     "WARNING: WITNESS option enabled, expect reduced performance.\n";
SYSINIT(witwarn, SI_SUB_COPYRIGHT, SI_ORDER_THIRD + 1,
   print_caddr_t, wit_warn);
SYSINIT(witwarn2, SI_SUB_RUN_SCHEDULER, SI_ORDER_THIRD + 1,
   print_caddr_t, wit_warn);
#endif

#ifdef DIAGNOSTIC
static char diag_warn[] =
     "WARNING: DIAGNOSTIC option enabled, expect reduced performance.\n";
SYSINIT(diagwarn, SI_SUB_COPYRIGHT, SI_ORDER_THIRD + 2,
    print_caddr_t, diag_warn);
SYSINIT(diagwarn2, SI_SUB_RUN_SCHEDULER, SI_ORDER_THIRD + 2,
    print_caddr_t, diag_warn);
#endif

static int
null_fetch_syscall_args(struct thread *td __unused,
    struct syscall_args *sa __unused)
{

	panic("null_fetch_syscall_args");
}

static void
null_set_syscall_retval(struct thread *td __unused, int error __unused)
{

	panic("null_set_syscall_retval");
}

struct sysentvec null_sysvec = {
	.sv_size	= 0,
	.sv_table	= NULL,
	.sv_mask	= 0,
	.sv_sigsize	= 0,
	.sv_sigtbl	= NULL,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= NULL,
	.sv_sendsig	= NULL,
	.sv_sigcode	= NULL,
	.sv_szsigcode	= NULL,
	.sv_prepsyscall	= NULL,
	.sv_name	= "null",
	.sv_coredump	= NULL,
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= 0,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings	= NULL,
	.sv_setregs	= NULL,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= 0,
	.sv_set_syscall_retval = null_set_syscall_retval,
	.sv_fetch_syscall_args = null_fetch_syscall_args,
	.sv_syscallnames = NULL,
	.sv_schedtail	= NULL,
};

/*
 ***************************************************************************
 ****
 **** The two following SYSINIT's are proc0 specific glue code.  I am not
 **** convinced that they can not be safely combined, but their order of
 **** operation has been maintained as the same as the original init_main.c
 **** for right now.
 ****
 **** These probably belong in init_proc.c or kern_proc.c, since they
 **** deal with proc0 (the fork template process).
 ****
 ***************************************************************************
 */
/* ARGSUSED*/
static void
proc0_init(void *dummy __unused)
{
	struct proc *p;
	struct thread *td;
	vm_paddr_t pageablemem;
	int i;

	GIANT_REQUIRED;
	p = &proc0;
	td = &thread0;
	
	/*
	 * Initialize magic number and osrel.
	 */
	p->p_magic = P_MAGIC;
	p->p_osrel = osreldate;

	/*
	 * Initialize thread and process structures.
	 */
	procinit();	/* set up proc zone */
	threadinit();	/* set up UMA zones */

	/*
	 * Initialise scheduler resources.
	 * Add scheduler specific parts to proc, thread as needed.
	 */
	schedinit();	/* scheduler gets its house in order */
	/*
	 * Initialize sleep queue hash table
	 */
	sleepinit();

	/*
	 * additional VM structures
	 */
	vm_init2();

	/*
	 * Create process 0 (the swapper).
	 */
	LIST_INSERT_HEAD(&allproc, p, p_list);
	LIST_INSERT_HEAD(PIDHASH(0), p, p_hash);
	mtx_init(&pgrp0.pg_mtx, "process group", NULL, MTX_DEF | MTX_DUPOK);
	p->p_pgrp = &pgrp0;
	LIST_INSERT_HEAD(PGRPHASH(0), &pgrp0, pg_hash);
	LIST_INIT(&pgrp0.pg_members);
	LIST_INSERT_HEAD(&pgrp0.pg_members, p, p_pglist);

	pgrp0.pg_session = &session0;
	mtx_init(&session0.s_mtx, "session", NULL, MTX_DEF);
	refcount_init(&session0.s_count, 1);
	session0.s_leader = p;

	p->p_sysent = &null_sysvec;
	p->p_flag = P_SYSTEM | P_INMEM;
	p->p_state = PRS_NORMAL;
	knlist_init_mtx(&p->p_klist, &p->p_mtx);
	STAILQ_INIT(&p->p_ktr);
	p->p_nice = NZERO;
	td->td_tid = PID_MAX + 1;
	LIST_INSERT_HEAD(TIDHASH(td->td_tid), td, td_hash);
	td->td_state = TDS_RUNNING;
	td->td_pri_class = PRI_TIMESHARE;
	td->td_user_pri = PUSER;
	td->td_base_user_pri = PUSER;
	td->td_lend_user_pri = PRI_MAX;
	td->td_priority = PVM;
	td->td_base_pri = PVM;
	td->td_oncpu = 0;
	td->td_flags = TDF_INMEM|TDP_KTHREAD;
	td->td_cpuset = cpuset_thread0();
	prison0.pr_cpuset = cpuset_ref(td->td_cpuset);
	p->p_peers = 0;
	p->p_leader = p;


	strncpy(p->p_comm, "kernel", sizeof (p->p_comm));
	strncpy(td->td_name, "swapper", sizeof (td->td_name));

	callout_init(&p->p_itcallout, CALLOUT_MPSAFE);
	callout_init_mtx(&p->p_limco, &p->p_mtx, 0);
	callout_init(&td->td_slpcallout, CALLOUT_MPSAFE);

	/* Create credentials. */
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */
	p->p_ucred->cr_uidinfo = uifind(0);
	p->p_ucred->cr_ruidinfo = uifind(0);
	p->p_ucred->cr_prison = &prison0;
	p->p_ucred->cr_loginclass = loginclass_find("default");
#ifdef AUDIT
	audit_cred_kproc0(p->p_ucred);
#endif
#ifdef MAC
	mac_cred_create_swapper(p->p_ucred);
#endif
	td->td_ucred = crhold(p->p_ucred);

	/* Create sigacts. */
	p->p_sigacts = sigacts_alloc();

	/* Initialize signal state for process 0. */
	siginit(&proc0);

	/* Create the file descriptor table. */
	p->p_fd = fdinit(NULL);
	p->p_fdtol = NULL;

	/* Create the limits structures. */
	p->p_limit = lim_alloc();
	for (i = 0; i < RLIM_NLIMITS; i++)
		p->p_limit->pl_rlimit[i].rlim_cur =
		    p->p_limit->pl_rlimit[i].rlim_max = RLIM_INFINITY;
	p->p_limit->pl_rlimit[RLIMIT_NOFILE].rlim_cur =
	    p->p_limit->pl_rlimit[RLIMIT_NOFILE].rlim_max = maxfiles;
	p->p_limit->pl_rlimit[RLIMIT_NPROC].rlim_cur =
	    p->p_limit->pl_rlimit[RLIMIT_NPROC].rlim_max = maxproc;
	p->p_limit->pl_rlimit[RLIMIT_DATA].rlim_cur = dfldsiz;
	p->p_limit->pl_rlimit[RLIMIT_DATA].rlim_max = maxdsiz;
	p->p_limit->pl_rlimit[RLIMIT_STACK].rlim_cur = dflssiz;
	p->p_limit->pl_rlimit[RLIMIT_STACK].rlim_max = maxssiz;
	/* Cast to avoid overflow on i386/PAE. */
	pageablemem = ptoa((vm_paddr_t)cnt.v_free_count);
	p->p_limit->pl_rlimit[RLIMIT_RSS].rlim_cur =
	    p->p_limit->pl_rlimit[RLIMIT_RSS].rlim_max = pageablemem;
	p->p_limit->pl_rlimit[RLIMIT_MEMLOCK].rlim_cur = pageablemem / 3;
	p->p_limit->pl_rlimit[RLIMIT_MEMLOCK].rlim_max = pageablemem;
	p->p_cpulimit = RLIM_INFINITY;

	/* Initialize resource accounting structures. */
	racct_create(&p->p_racct);

	p->p_stats = pstats_alloc();

	/* Allocate a prototype map so we have something to fork. */
	pmap_pinit0(vmspace_pmap(&vmspace0));
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;

	/*
	 * proc0 is not expected to enter usermode, so there is no special
	 * handling for sv_minuser here, like is done for exec_new_vmspace().
	 */
	vm_map_init(&vmspace0.vm_map, vmspace_pmap(&vmspace0),
	    p->p_sysent->sv_minuser, p->p_sysent->sv_maxuser);

	/*
	 * Call the init and ctor for the new thread and proc.  We wait
	 * to do this until all other structures are fairly sane.
	 */
	EVENTHANDLER_INVOKE(process_init, p);
	EVENTHANDLER_INVOKE(thread_init, td);
	EVENTHANDLER_INVOKE(process_ctor, p);
	EVENTHANDLER_INVOKE(thread_ctor, td);

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(p->p_ucred->cr_ruidinfo, 1, 0);
	PROC_LOCK(p);
	racct_add_force(p, RACCT_NPROC, 1);
	PROC_UNLOCK(p);
}
SYSINIT(p0init, SI_SUB_INTRINSIC, SI_ORDER_FIRST, proc0_init, NULL);

/* ARGSUSED*/
static void
proc0_post(void *dummy __unused)
{
	struct timespec ts;
	struct proc *p;
	struct rusage ru;
	struct thread *td;

	/*
	 * Now we can look at the time, having had a chance to verify the
	 * time from the filesystem.  Pretend that proc0 started now.
	 */
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		microuptime(&p->p_stats->p_start);
		PROC_SLOCK(p);
		rufetch(p, &ru);	/* Clears thread stats */
		PROC_SUNLOCK(p);
		p->p_rux.rux_runtime = 0;
		p->p_rux.rux_uticks = 0;
		p->p_rux.rux_sticks = 0;
		p->p_rux.rux_iticks = 0;
		FOREACH_THREAD_IN_PROC(p, td) {
			td->td_runtime = 0;
		}
	}
	sx_sunlock(&allproc_lock);
	PCPU_SET(switchtime, cpu_ticks());
	PCPU_SET(switchticks, ticks);

	/*
	 * Give the ``random'' number generator a thump.
	 */
	nanotime(&ts);
	srandom(ts.tv_sec ^ ts.tv_nsec);
}
SYSINIT(p0post, SI_SUB_INTRINSIC_POST, SI_ORDER_FIRST, proc0_post, NULL);

static void
random_init(void *dummy __unused)
{

	/*
	 * After CPU has been started we have some randomness on most
	 * platforms via get_cyclecount().  For platforms that don't
	 * we will reseed random(9) in proc0_post() as well.
	 */
	srandom(get_cyclecount());
}
SYSINIT(random, SI_SUB_RANDOM, SI_ORDER_FIRST, random_init, NULL);

/*
 ***************************************************************************
 ****
 **** The following SYSINIT's and glue code should be moved to the
 **** respective files on a per subsystem basis.
 ****
 ***************************************************************************
 */


/*
 ***************************************************************************
 ****
 **** The following code probably belongs in another file, like
 **** kern/init_init.c.
 ****
 ***************************************************************************
 */

/*
 * List of paths to try when searching for "init".
 */
static char init_path[MAXPATHLEN] =
#ifdef	INIT_PATH
    __XSTRING(INIT_PATH);
#else
    "/sbin/init:/sbin/oinit:/sbin/init.bak:/rescue/init:/stand/sysinstall";
#endif
SYSCTL_STRING(_kern, OID_AUTO, init_path, CTLFLAG_RD, init_path, 0,
	"Path used to search the init process");

/*
 * Shutdown timeout of init(8).
 * Unused within kernel, but used to control init(8), hence do not remove.
 */
#ifndef INIT_SHUTDOWN_TIMEOUT
#define INIT_SHUTDOWN_TIMEOUT 120
#endif
static int init_shutdown_timeout = INIT_SHUTDOWN_TIMEOUT;
SYSCTL_INT(_kern, OID_AUTO, init_shutdown_timeout,
	CTLFLAG_RW, &init_shutdown_timeout, 0, "Shutdown timeout of init(8). "
	"Unused within kernel, but used to control init(8)");

/*
 * Start the initial user process; try exec'ing each pathname in init_path.
 * The program is invoked with one argument containing the boot flags.
 */
static void
start_init(void *dummy)
{
	vm_offset_t addr;
	struct execve_args args;
	int options, error;
	char *var, *path, *next, *s;
	char *ucp, **uap, *arg0, *arg1;
	struct thread *td;
	struct proc *p;

	mtx_lock(&Giant);

	GIANT_REQUIRED;

	td = curthread;
	p = td->td_proc;

	vfs_mountroot();

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
	addr = p->p_sysent->sv_usrstack - PAGE_SIZE;
	if (vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &addr, PAGE_SIZE,
			FALSE, VM_PROT_ALL, VM_PROT_ALL, 0) != 0)
		panic("init: couldn't allocate argument space");
	p->p_vmspace->vm_maxsaddr = (caddr_t)addr;
	p->p_vmspace->vm_ssize = 1;

	if ((var = getenv("init_path")) != NULL) {
		strlcpy(init_path, var, sizeof(init_path));
		freeenv(var);
	}
	
	for (path = init_path; *path != '\0'; path = next) {
		while (*path == ':')
			path++;
		if (*path == '\0')
			break;
		for (next = path; *next != '\0' && *next != ':'; next++)
			/* nothing */ ;
		if (bootverbose)
			printf("start_init: trying %.*s\n", (int)(next - path),
			    path);
			
		/*
		 * Move out the boot flag argument.
		 */
		options = 0;
		ucp = (char *)p->p_sysent->sv_usrstack;
		(void)subyte(--ucp, 0);		/* trailing zero */
		if (boothowto & RB_SINGLE) {
			(void)subyte(--ucp, 's');
			options = 1;
		}
#ifdef notyet
                if (boothowto & RB_FASTBOOT) {
			(void)subyte(--ucp, 'f');
			options = 1;
		}
#endif

#ifdef BOOTCDROM
		(void)subyte(--ucp, 'C');
		options = 1;
#endif

		if (options == 0)
			(void)subyte(--ucp, '-');
		(void)subyte(--ucp, '-');		/* leading hyphen */
		arg1 = ucp;

		/*
		 * Move out the file name (also arg 0).
		 */
		(void)subyte(--ucp, 0);
		for (s = next - 1; s >= path; s--)
			(void)subyte(--ucp, *s);
		arg0 = ucp;

		/*
		 * Move out the arg pointers.
		 */
		uap = (char **)((intptr_t)ucp & ~(sizeof(intptr_t)-1));
		(void)suword((caddr_t)--uap, (long)0);	/* terminator */
		(void)suword((caddr_t)--uap, (long)(intptr_t)arg1);
		(void)suword((caddr_t)--uap, (long)(intptr_t)arg0);

		/*
		 * Point at the arguments.
		 */
		args.fname = arg0;
		args.argv = uap;
		args.envv = NULL;

		/*
		 * Now try to exec the program.  If can't for any reason
		 * other than it doesn't exist, complain.
		 *
		 * Otherwise, return via fork_trampoline() all the way
		 * to user mode as init!
		 */
		if ((error = execve(td, &args)) == 0) {
			mtx_unlock(&Giant);
			return;
		}
		if (error != ENOENT)
			printf("exec %.*s: error %d\n", (int)(next - path), 
			    path, error);
	}
	printf("init: not found in path %s\n", init_path);
	panic("no init");
}

/*
 * Like kproc_create(), but runs in it's own address space.
 * We do this early to reserve pid 1.
 *
 * Note special case - do not make it runnable yet.  Other work
 * in progress will change this more.
 */
static void
create_init(const void *udata __unused)
{
	struct ucred *newcred, *oldcred;
	int error;

	error = fork1(&thread0, RFFDG | RFPROC | RFSTOPPED, 0, &initproc,
	    NULL, 0);
	if (error)
		panic("cannot fork init: %d\n", error);
	KASSERT(initproc->p_pid == 1, ("create_init: initproc->p_pid != 1"));
	/* divorce init's credentials from the kernel's */
	newcred = crget();
	PROC_LOCK(initproc);
	initproc->p_flag |= P_SYSTEM | P_INMEM;
	oldcred = initproc->p_ucred;
	crcopy(newcred, oldcred);
#ifdef MAC
	mac_cred_create_init(newcred);
#endif
#ifdef AUDIT
	audit_cred_proc1(newcred);
#endif
	initproc->p_ucred = newcred;
	PROC_UNLOCK(initproc);
	crfree(oldcred);
	cred_update_thread(FIRST_THREAD_IN_PROC(initproc));
	cpu_set_fork_handler(FIRST_THREAD_IN_PROC(initproc), start_init, NULL);
}
SYSINIT(init, SI_SUB_CREATE_INIT, SI_ORDER_FIRST, create_init, NULL);

/*
 * Make it runnable now.
 */
static void
kick_init(const void *udata __unused)
{
	struct thread *td;

	td = FIRST_THREAD_IN_PROC(initproc);
	thread_lock(td);
	TD_SET_CAN_RUN(td);
	sched_add(td, SRQ_BORING);
	thread_unlock(td);
}
SYSINIT(kickinit, SI_SUB_KTHREAD_INIT, SI_ORDER_FIRST, kick_init, NULL);
