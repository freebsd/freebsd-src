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

#include "opt_init_path.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
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

#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/copyright.h>

void mi_startup(void);				/* Should be elsewhere */

/* Components of the first process -- never freed. */
static struct session session0;
static struct pgrp pgrp0;
struct	proc proc0;
struct	thread thread0;
struct	ksegrp ksegrp0;
struct	vmspace vmspace0;
struct	proc *initproc;

int	boothowto = 0;		/* initialized so that it can be patched */
SYSCTL_INT(_debug, OID_AUTO, boothowto, CTLFLAG_RD, &boothowto, 0, "");
int	bootverbose;
SYSCTL_INT(_debug, OID_AUTO, bootverbose, CTLFLAG_RW, &bootverbose, 0, "");

/*
 * This ensures that there is at least one entry so that the sysinit_set
 * symbol is not undefined.  A sybsystem ID of SI_SUB_DUMMY is never
 * executed.
 */
SYSINIT(placeholder, SI_SUB_DUMMY, SI_ORDER_ANY, NULL, NULL)

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

		/* Call function */
		(*((*sipp)->func))((*sipp)->udata);

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
print_caddr_t(void *data __unused)
{
	printf("%s", (char *)data);
}
SYSINIT(announce, SI_SUB_COPYRIGHT, SI_ORDER_FIRST, print_caddr_t, copyright)
SYSINIT(version, SI_SUB_COPYRIGHT, SI_ORDER_SECOND, print_caddr_t, version)

#ifdef WITNESS
static char wit_warn[] =
     "WARNING: WITNESS option enabled, expect reduced performance.\n";
SYSINIT(witwarn, SI_SUB_COPYRIGHT, SI_ORDER_SECOND + 1,
   print_caddr_t, wit_warn)
#endif

#ifdef DIAGNOSTIC
static char diag_warn[] =
     "WARNING: DIAGNOSTIC option enabled, expect reduced performance.\n";
SYSINIT(diagwarn, SI_SUB_COPYRIGHT, SI_ORDER_SECOND + 2,
    print_caddr_t, diag_warn)
#endif

static void
set_boot_verbose(void *data __unused)
{

	if (boothowto & RB_VERBOSE)
		bootverbose++;
}
SYSINIT(boot_verbose, SI_SUB_TUNABLES, SI_ORDER_ANY, set_boot_verbose, NULL)

struct sysentvec null_sysvec = {
	0,
	NULL,
	0,
	0,
	NULL,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"null",
	NULL,
	NULL,
	0,
	PAGE_SIZE,
	VM_MIN_ADDRESS,
	VM_MAXUSER_ADDRESS,
	USRSTACK,
	PS_STRINGS,
	VM_PROT_ALL,
	NULL,
	NULL,
	NULL
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
	unsigned i;
	struct thread *td;
	struct ksegrp *kg;

	GIANT_REQUIRED;
	p = &proc0;
	td = &thread0;
	kg = &ksegrp0;

	/*
	 * Initialize magic number.
	 */
	p->p_magic = P_MAGIC;

	/*
	 * Initialize thread, process and ksegrp structures.
	 */
	procinit();	/* set up proc zone */
	threadinit();	/* set up thead, upcall and KSEGRP zones */

	/*
	 * Initialise scheduler resources.
	 * Add scheduler specific parts to proc, ksegrp, thread as needed.
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
	session0.s_count = 1;
	session0.s_leader = p;

	p->p_sysent = &null_sysvec;
	p->p_flag = P_SYSTEM;
	p->p_sflag = PS_INMEM;
	p->p_state = PRS_NORMAL;
	knlist_init(&p->p_klist, &p->p_mtx);
	p->p_nice = NZERO;
	td->td_state = TDS_RUNNING;
	kg->kg_pri_class = PRI_TIMESHARE;
	kg->kg_user_pri = PUSER;
	td->td_priority = PVM;
	td->td_base_pri = PUSER;
	td->td_oncpu = 0;
	p->p_peers = 0;
	p->p_leader = p;


	bcopy("swapper", p->p_comm, sizeof ("swapper"));

	callout_init(&p->p_itcallout, CALLOUT_MPSAFE);
	callout_init(&td->td_slpcallout, CALLOUT_MPSAFE);

	/* Create credentials. */
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */
	p->p_ucred->cr_uidinfo = uifind(0);
	p->p_ucred->cr_ruidinfo = uifind(0);
	p->p_ucred->cr_prison = NULL;	/* Don't jail it. */
#ifdef MAC
	mac_create_proc0(p->p_ucred);
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
	i = ptoa(cnt.v_free_count);
	p->p_limit->pl_rlimit[RLIMIT_RSS].rlim_max = i;
	p->p_limit->pl_rlimit[RLIMIT_MEMLOCK].rlim_max = i;
	p->p_limit->pl_rlimit[RLIMIT_MEMLOCK].rlim_cur = i / 3;
	p->p_cpulimit = RLIM_INFINITY;

	p->p_stats = pstats_alloc();

	/* Allocate a prototype map so we have something to fork. */
	pmap_pinit0(vmspace_pmap(&vmspace0));
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;
	vm_map_init(&vmspace0.vm_map, p->p_sysent->sv_minuser,
	    p->p_sysent->sv_maxuser);
	vmspace0.vm_map.pmap = vmspace_pmap(&vmspace0);

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(p->p_ucred->cr_ruidinfo, 1, 0);
}
SYSINIT(p0init, SI_SUB_INTRINSIC, SI_ORDER_FIRST, proc0_init, NULL)

/* ARGSUSED*/
static void
proc0_post(void *dummy __unused)
{
	struct timespec ts;
	struct proc *p;

	/*
	 * Now we can look at the time, having had a chance to verify the
	 * time from the filesystem.  Pretend that proc0 started now.
	 */
	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		microuptime(&p->p_stats->p_start);
		p->p_runtime.sec = 0;
		p->p_runtime.frac = 0;
	}
	sx_sunlock(&allproc_lock);
	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	/*
	 * Give the ``random'' number generator a thump.
	 */
	nanotime(&ts);
	srandom(ts.tv_sec ^ ts.tv_nsec);
}
SYSINIT(p0post, SI_SUB_INTRINSIC_POST, SI_ORDER_FIRST, proc0_post, NULL)

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
    "/sbin/init:/sbin/oinit:/sbin/init.bak:/stand/sysinstall";
#endif
SYSCTL_STRING(_kern, OID_AUTO, init_path, CTLFLAG_RD, init_path, 0,
	"Path used to search the init process");

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
	int init_does_devfs = 0;

	mtx_lock(&Giant);

	GIANT_REQUIRED;

	td = curthread;
	p = td->td_proc;

	vfs_mountroot();

	/* Get the vnode for '/'.  Set p->p_fd->fd_cdir to reference it. */
	if (VFS_ROOT(TAILQ_FIRST(&mountlist), &rootvnode, td))
		panic("cannot find root vnode");
	FILEDESC_LOCK(p->p_fd);
	p->p_fd->fd_cdir = rootvnode;
	VREF(p->p_fd->fd_cdir);
	p->p_fd->fd_rdir = rootvnode;
	VREF(p->p_fd->fd_rdir);
	FILEDESC_UNLOCK(p->p_fd);
	VOP_UNLOCK(rootvnode, 0, td);
#ifdef MAC
	mac_create_root_mount(td->td_ucred, TAILQ_FIRST(&mountlist));
#endif

	/*
	 * For disk based systems, we probably cannot do this yet
	 * since the fs will be read-only.  But a NFS root
	 * might be ok.  It is worth a shot.
	 */
	error = kern_mkdir(td, "/dev", UIO_SYSSPACE, 0700);
	if (error == EEXIST)
		error = 0;
	if (error == 0)
		error = kernel_vmount(0, "fstype", "devfs",
		    "fspath", "/dev", NULL);
	if (error != 0)
		init_does_devfs = 1;

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
		if (init_does_devfs) {
			(void)subyte(--ucp, 'd');
			options = 1;
		}

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
 * Like kthread_create(), but runs in it's own address space.
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

	error = fork1(&thread0, RFFDG | RFPROC | RFSTOPPED, 0, &initproc);
	if (error)
		panic("cannot fork init: %d\n", error);
	KASSERT(initproc->p_pid == 1, ("create_init: initproc->p_pid != 1"));
	/* divorce init's credentials from the kernel's */
	newcred = crget();
	PROC_LOCK(initproc);
	initproc->p_flag |= P_SYSTEM;
	oldcred = initproc->p_ucred;
	crcopy(newcred, oldcred);
#ifdef MAC
	mac_create_proc1(newcred);
#endif
	initproc->p_ucred = newcred;
	PROC_UNLOCK(initproc);
	crfree(oldcred);
	cred_update_thread(FIRST_THREAD_IN_PROC(initproc));
	mtx_lock_spin(&sched_lock);
	initproc->p_sflag |= PS_INMEM;
	mtx_unlock_spin(&sched_lock);
	cpu_set_fork_handler(FIRST_THREAD_IN_PROC(initproc), start_init, NULL);
}
SYSINIT(init, SI_SUB_CREATE_INIT, SI_ORDER_FIRST, create_init, NULL)

/*
 * Make it runnable now.
 */
static void
kick_init(const void *udata __unused)
{
	struct thread *td;

	td = FIRST_THREAD_IN_PROC(initproc);
	mtx_lock_spin(&sched_lock);
	TD_SET_CAN_RUN(td);
	setrunqueue(td, SRQ_BORING);	/* XXXKSE */
	mtx_unlock_spin(&sched_lock);
}
SYSINIT(kickinit, SI_SUB_KTHREAD_INIT, SI_ORDER_FIRST, kick_init, NULL)
