/*
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
 * $FreeBSD$
 */

#include "opt_init_path.h"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/sysent.h>
#include <sys/reboot.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#include <sys/unistd.h>
#include <sys/malloc.h>
#include <sys/conf.h>

#include <machine/cpu.h>
#include <machine/globals.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <sys/copyright.h>

extern struct linker_set	sysinit_set;	/* XXX */

void mi_startup(void);				/* Should be elsewhere */

/* Components of the first process -- never freed. */
static struct session session0;
static struct pgrp pgrp0;
struct	proc proc0;
static struct pcred cred0;
static struct procsig procsig0;
static struct filedesc0 filedesc0;
static struct plimit limit0;
static struct vmspace vmspace0;
struct	proc *initproc;

int cmask = CMASK;
extern	struct user *proc0paddr;

struct	vnode *rootvp;
int	boothowto = 0;		/* initialized so that it can be patched */
SYSCTL_INT(_debug, OID_AUTO, boothowto, CTLFLAG_RD, &boothowto, 0, "");

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
struct sysinit **sysinit = (struct sysinit **)sysinit_set.ls_items;
struct sysinit **newsysinit;

/*
 * Merge a new sysinit set into the current set, reallocating it if
 * necessary.  This can only be called after malloc is running.
 */
void
sysinit_add(struct sysinit **set)
{
	struct sysinit **newset;
	struct sysinit **sipp;
	struct sysinit **xipp;
	int count = 0;

	if (newsysinit)
		for (sipp = newsysinit; *sipp; sipp++)
			count++;
	else
		for (sipp = sysinit; *sipp; sipp++)
			count++;
	for (sipp = set; *sipp; sipp++)
		count++;
	count++;		/* Trailing NULL */
	newset = malloc(count * sizeof(*sipp), M_TEMP, M_NOWAIT);
	if (newset == NULL)
		panic("cannot malloc for sysinit");
	xipp = newset;
	if (newsysinit)
		for (sipp = newsysinit; *sipp; sipp++)
			*xipp++ = *sipp;
	else
		for (sipp = sysinit; *sipp; sipp++)
			*xipp++ = *sipp;
	for (sipp = set; *sipp; sipp++)
		*xipp++ = *sipp;
	*xipp = NULL;
	if (newsysinit)
		free(newsysinit, M_TEMP);
	newsysinit = newset;
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

restart:
	/*
	 * Perform a bubble sort of the system initialization objects by
	 * their subsystem (primary key) and order (secondary key).
	 */
	for (sipp = sysinit; *sipp; sipp++) {
		for (xipp = sipp + 1; *xipp; xipp++) {
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
	for (sipp = sysinit; *sipp; sipp++) {

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
			if (sysinit != (struct sysinit **)sysinit_set.ls_items)
				free(sysinit, M_TEMP);
			sysinit = newsysinit;
			newsysinit = NULL;
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


/*
 ***************************************************************************
 ****
 **** The two following SYSINT's are proc0 specific glue code.  I am not
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
	register struct proc		*p;
	register struct filedesc0	*fdp;
	register unsigned i;

	p = &proc0;

	/*
	 * Initialize magic number.
	 */
	p->p_magic = P_MAGIC;

	/*
	 * Initialize process and pgrp structures.
	 */
	procinit();

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
	p->p_pgrp = &pgrp0;
	LIST_INSERT_HEAD(PGRPHASH(0), &pgrp0, pg_hash);
	LIST_INIT(&pgrp0.pg_members);
	LIST_INSERT_HEAD(&pgrp0.pg_members, p, p_pglist);

	pgrp0.pg_session = &session0;
	session0.s_count = 1;
	session0.s_leader = p;

#ifdef __ELF__
	p->p_sysent = &elf_freebsd_sysvec;
#else
	p->p_sysent = &aout_sysvec;
#endif

	p->p_flag = P_INMEM | P_SYSTEM;
	p->p_stat = SRUN;
	p->p_nice = NZERO;
	p->p_rtprio.type = RTP_PRIO_NORMAL;
	p->p_rtprio.prio = 0;

	p->p_peers = 0;
	p->p_leader = p;

	bcopy("swapper", p->p_comm, sizeof ("swapper"));

	callout_init(&p->p_itcallout, 0);
	callout_init(&p->p_slpcallout, 1);

	/* Create credentials. */
	cred0.p_refcnt = 1;
	cred0.p_uidinfo = uifind(0);
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */
	p->p_ucred->cr_uidinfo = uifind(0);

	/* Don't jail it */
	p->p_prison = 0;

	/* Create procsig. */
	p->p_procsig = &procsig0;
	p->p_procsig->ps_refcnt = 1;

	/* Initialize signal state for process 0. */
	siginit(&proc0);

	/* Create the file descriptor table. */
	fdp = &filedesc0;
	p->p_fd = &fdp->fd_fd;
	fdp->fd_fd.fd_refcnt = 1;
	fdp->fd_fd.fd_cmask = cmask;
	fdp->fd_fd.fd_ofiles = fdp->fd_dfiles;
	fdp->fd_fd.fd_ofileflags = fdp->fd_dfileflags;
	fdp->fd_fd.fd_nfiles = NDFILE;

	/* Create the limits structures. */
	p->p_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur =
		    limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_cur =
	    limit0.pl_rlimit[RLIMIT_NOFILE].rlim_max = maxfiles;
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur =
	    limit0.pl_rlimit[RLIMIT_NPROC].rlim_max = maxproc;
	i = ptoa(cnt.v_free_count);
	limit0.pl_rlimit[RLIMIT_RSS].rlim_max = i;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_max = i;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_cur = i / 3;
	limit0.p_cpulimit = RLIM_INFINITY;
	limit0.p_refcnt = 1;

	/* Allocate a prototype map so we have something to fork. */
	pmap_pinit0(vmspace_pmap(&vmspace0));
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;
	vm_map_init(&vmspace0.vm_map, round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAXUSER_ADDRESS));
	vmspace0.vm_map.pmap = vmspace_pmap(&vmspace0);
	p->p_addr = proc0paddr;				/* XXX */

	/*
	 * We continue to place resource usage info and signal
	 * actions in the user struct so they're pageable.
	 */
	p->p_stats = &p->p_addr->u_stats;
	p->p_sigacts = &p->p_addr->u_sigacts;

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(cred0.p_uidinfo, 1, 0);

	LIST_INIT(&p->p_heldmtx);
	LIST_INIT(&p->p_contested);

	/*
	 * Initialize the current process pointer (curproc) before
	 * any possible traps/probes to simplify trap processing.
	 */
	PCPU_SET(curproc, p);

	/*
	 * Enter the Giant mutex.
	 * XXX This should be done BEFORE cpu_startup().
	 */
	mtx_enter(&Giant, MTX_DEF);

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
	 * time from the file system.  Pretend that proc0 started now.
	 */
	lockmgr(&allproc_lock, LK_SHARED, NULL, CURPROC);
	LIST_FOREACH(p, &allproc, p_list) {
		microtime(&p->p_stats->p_start);
		p->p_runtime = 0;
	}
	lockmgr(&allproc_lock, LK_RELEASE, NULL, CURPROC);
	microuptime(&switchtime);
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
SYSCTL_STRING(_kern, OID_AUTO, init_path, CTLFLAG_RD, init_path, 0, "");

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
	struct proc *p;

	mtx_enter(&Giant, MTX_DEF);

	p = curproc;

	/* Get the vnode for '/'.  Set p->p_fd->fd_cdir to reference it. */
	if (VFS_ROOT(TAILQ_FIRST(&mountlist), &rootvnode))
		panic("cannot find root vnode");
	p->p_fd->fd_cdir = rootvnode;
	VREF(p->p_fd->fd_cdir);
	p->p_fd->fd_rdir = rootvnode;
	VOP_UNLOCK(rootvnode, 0, p);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
	addr = trunc_page(USRSTACK - PAGE_SIZE);
	if (vm_map_find(&p->p_vmspace->vm_map, NULL, 0, &addr, PAGE_SIZE,
			FALSE, VM_PROT_ALL, VM_PROT_ALL, 0) != 0)
		panic("init: couldn't allocate argument space");
	p->p_vmspace->vm_maxsaddr = (caddr_t)addr;
	p->p_vmspace->vm_ssize = 1;

	if ((var = getenv("init_path")) != NULL) {
		strncpy(init_path, var, sizeof init_path);
		init_path[sizeof init_path - 1] = 0;
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
		ucp = (char *)USRSTACK;
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
		if (devfs_present) {
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
		if ((error = execve(p, &args)) == 0) {
			mtx_exit(&Giant, MTX_DEF);
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
	int error;

	error = fork1(&proc0, RFFDG | RFPROC | RFSTOPPED, &initproc);
	if (error)
		panic("cannot fork init: %d\n", error);
	initproc->p_flag |= P_INMEM | P_SYSTEM;
	cpu_set_fork_handler(initproc, start_init, NULL);
}
SYSINIT(init,SI_SUB_CREATE_INIT, SI_ORDER_FIRST, create_init, NULL)

/*
 * Make it runnable now.
 */
static void
kick_init(const void *udata __unused)
{
	mtx_enter(&sched_lock, MTX_SPIN);
	initproc->p_stat = SRUN;
	setrunqueue(initproc);
	mtx_exit(&sched_lock, MTX_SPIN);
}
SYSINIT(kickinit,SI_SUB_KTHREAD_INIT, SI_ORDER_FIRST, kick_init, NULL)
