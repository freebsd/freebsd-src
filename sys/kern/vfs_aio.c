/*
 * Copyright (c) 1997 John S. Dyson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. John S. Dyson's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * DISCLAIMER:  This code isn't warranted to do anything useful.  Anything
 * bad that happens because of using this software isn't the responsibility
 * of the author.  This software is distributed AS-IS.
 *
 * $Id: vfs_aio.c,v 1.6 1997/10/11 01:07:03 dyson Exp $
 */

/*
 * This file contains support for the POSIX.4 AIO facility.
 *
 * The initial version provides only the (bogus) synchronous semantics
 * but will support async in the future.  Note that a bit
 * in a private field allows the user mode subroutine to adapt
 * the kernel operations to true POSIX.4 for future compatibility.
 *
 * This code is used to support true POSIX.4 AIO/LIO with the help
 * of a user mode subroutine package.  Note that eventually more support
 * will be pushed into the kernel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/aio.h>
#include <sys/shm.h>

#include <machine/cpu.h>

MALLOC_DEFINE(M_AIO, "AIO", "AIO structure(s)");

#define AIOCBLIST_CANCELLED	0x1
#define AIOCBLIST_RUNDOWN	0x4
#define AIOCBLIST_ASYNCFREE	0x8
#define AIOCBLIST_SUSPEND	0x10

#if 0
#define DEBUGAIO
#define DIAGNOSTIC
#endif

#define DEBUGAIO 1

static	int jobrefid;

#define JOBST_NULL		0x0
#define	JOBST_JOBQPROC		0x1
#define JOBST_JOBQGLOBAL	0x2
#define JOBST_JOBRUNNING	0x3
#define JOBST_JOBFINISHED	0x4

#define MAX_AIO_PER_PROC	32
#define MAX_AIO_QUEUE_PER_PROC	256 /* Bigger than AIO_LISTIO_MAX */
#define MAX_AIO_PROCS		128
#define	MAX_AIO_QUEUE		1024 /* Bigger than AIO_LISTIO_MAX */
#define TARGET_AIO_PROCS	64

int max_aio_procs = MAX_AIO_PROCS;
int num_aio_procs = 0;
int target_aio_procs = TARGET_AIO_PROCS;
int max_queue_count = MAX_AIO_QUEUE;
int num_queue_count = 0;

int max_aio_per_proc = MAX_AIO_PER_PROC,
	max_aio_queue_per_proc=MAX_AIO_QUEUE_PER_PROC;


SYSCTL_NODE(_vfs, OID_AUTO, aio, CTLFLAG_RW, 0, "AIO mgmt");

SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_per_proc,
	CTLFLAG_RW, &max_aio_per_proc, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_queue_per_proc,
	CTLFLAG_RW, &max_aio_queue_per_proc, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_procs,
	CTLFLAG_RW, &max_aio_procs, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, num_aio_procs,
	CTLFLAG_RD, &num_aio_procs, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, num_queue_count,
	CTLFLAG_RD, &num_queue_count, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_queue,
	CTLFLAG_RW, &max_queue_count, 0, "");

SYSCTL_INT(_vfs_aio, OID_AUTO, target_aio_procs,
	CTLFLAG_RW, &target_aio_procs, 0, "");

#if DEBUGAIO > 0
static int debugaio;
SYSCTL_INT(_vfs_aio, OID_AUTO, debugaio, CTLFLAG_RW, &debugaio, 0, "");
#endif

/*
 * Job queue item
 */
struct aiocblist {
	TAILQ_ENTRY (aiocblist) list;		/* List of jobs */
	TAILQ_ENTRY (aiocblist) plist;		/* List of jobs for proc */
	int	jobflags;
	int	jobstate;
	struct	proc *userproc;			/* User process */
	struct	aioproclist	*jobaioproc;	/* AIO process descriptor */
	struct	aiocb uaiocb;			/* Kernel I/O control block */
};

#define AIOP_FREE	0x1			/* proc on free queue */
/*
 * AIO process info
 */
struct aioproclist {
	int aioprocflags;			/* AIO proc flags */
	TAILQ_ENTRY(aioproclist) list;		/* List of processes */
	struct proc *aioproc;			/* The AIO thread */
	TAILQ_HEAD (,aiocblist) jobtorun;	/* suggested job to run */
};

struct kaioinfo {
	int	kaio_maxactive_count;	/* maximum number of AIOs */
	int	kaio_active_count;	/* number of currently used AIOs */
	int	kaio_qallowed_count;	/* maxiumu size of AIO queue */
	int	kaio_queue_count;	/* size of AIO queue */
	TAILQ_HEAD (,aiocblist)	kaio_jobqueue;	/* job queue for process */
	TAILQ_HEAD (,aiocblist)	kaio_jobdone;	/* done queue for process */
};

TAILQ_HEAD (,aioproclist) aio_freeproc, aio_activeproc;
TAILQ_HEAD(,aiocblist) aio_jobs;			/* Async job list */
TAILQ_HEAD(,aiocblist) aio_freejobs;


void aio_init_aioinfo(struct proc *p) ;
void aio_onceonly(void *) ;
int aio_free_entry(struct aiocblist *aiocbe);
void aio_cancel_internal(struct aiocblist *aiocbe);
void aio_process(struct aiocblist *aiocbe);
void pmap_newvmspace(struct vmspace *);
static int aio_newproc(void) ;
static int aio_aqueue(struct proc *p, struct aiocb *job, int type) ;
static void aio_marksuspend(struct proc *p, int njobs, int *joblist, int set) ;

SYSINIT(aio, SI_SUB_VFS, SI_ORDER_ANY, aio_onceonly, NULL);


/*
 * Startup initialization
 */
void
aio_onceonly(void *na) {
	TAILQ_INIT(&aio_freeproc);
	TAILQ_INIT(&aio_activeproc);
	TAILQ_INIT(&aio_jobs);
	TAILQ_INIT(&aio_freejobs);
}

/*
 * Init the per-process aioinfo structure.
 */
void
aio_init_aioinfo(struct proc *p) {
	struct kaioinfo *ki;
	if (p->p_aioinfo == NULL) {
		ki = malloc(sizeof (struct kaioinfo), M_AIO, M_WAITOK);
		p->p_aioinfo = ki;
		ki->kaio_maxactive_count = max_aio_per_proc;
		ki->kaio_active_count = 0;
		ki->kaio_qallowed_count = max_aio_queue_per_proc;
		ki->kaio_queue_count = 0;
		TAILQ_INIT(&ki->kaio_jobdone);
		TAILQ_INIT(&ki->kaio_jobqueue);
	}
}

/*
 * Free a job entry.  Wait for completion if it is currently
 * active, but don't delay forever.  If we delay, we return
 * a flag that says that we have to restart the queue scan.
 */
int
aio_free_entry(struct aiocblist *aiocbe) {
	struct kaioinfo *ki;
	struct aioproclist *aiop;
	struct proc *p;

	if (aiocbe->jobstate == JOBST_NULL)
		panic("aio_free_entry: freeing already free job");

	p = aiocbe->userproc;
	ki = p->p_aioinfo;
	if (ki == NULL)
		panic("aio_free_entry: missing p->p_aioinfo");

	if (aiocbe->jobstate == JOBST_JOBRUNNING) {
		if (aiocbe->jobflags & AIOCBLIST_ASYNCFREE)
			return 0;
		aiocbe->jobflags |= AIOCBLIST_RUNDOWN;
		tsleep(aiocbe, PRIBIO|PCATCH, "jobwai", 0);
/*
		if (tsleep(aiocbe, PRIBIO|PCATCH, "jobwai", hz*5)) {
			aiocbe->jobflags |= AIOCBLIST_ASYNCFREE;
			aiocbe->jobflags &= ~AIOCBLIST_RUNDOWN;
			return 1;
		}
		aiocbe->jobflags &= ~AIOCBLIST_RUNDOWN;
*/
	}
	aiocbe->jobflags &= ~AIOCBLIST_ASYNCFREE;

	if (ki->kaio_queue_count <= 0)
		panic("aio_free_entry: process queue size <= 0");
	if (num_queue_count <= 0)
		panic("aio_free_entry: system wide queue size <= 0");
	
	--ki->kaio_queue_count;
	--num_queue_count;
#if DEBUGAIO > 0
	if (debugaio > 0)
		printf("freeing entry: %d, %d\n",
			ki->kaio_queue_count, num_queue_count);
#endif
		
	if ( aiocbe->jobstate == JOBST_JOBQPROC) {
		aiop = aiocbe->jobaioproc;
		TAILQ_REMOVE(&aiop->jobtorun, aiocbe, list);
	} else if ( aiocbe->jobstate == JOBST_JOBQGLOBAL) {
		TAILQ_REMOVE(&aio_jobs, aiocbe, list);
	} else if ( aiocbe->jobstate == JOBST_JOBFINISHED) {
		ki = p->p_aioinfo;
		TAILQ_REMOVE(&ki->kaio_jobdone, aiocbe, plist);
	}
	TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
	aiocbe->jobstate = JOBST_NULL;
	return 0;
}

/*
 * Rundown the jobs for a given process.  
 */
void
aio_proc_rundown(struct proc *p) {
	struct kaioinfo *ki;
	struct aiocblist *aiocbe, *aiocbn;
	
	ki = p->p_aioinfo;
	if (ki == NULL)
		return;

	while (ki->kaio_active_count > 0) {
		if (tsleep(ki, PRIBIO, "kaiowt", 60 * hz))
			break;
	}

#if DEBUGAIO > 0
	if (debugaio > 0)
		printf("Proc rundown: %d %d\n",
			num_queue_count, ki->kaio_queue_count);
#endif

restart1:
	for ( aiocbe = TAILQ_FIRST(&ki->kaio_jobdone);
		aiocbe;
		aiocbe = aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		if (aio_free_entry(aiocbe))
			goto restart1;
	}

restart2:
	for ( aiocbe = TAILQ_FIRST(&ki->kaio_jobqueue);
		aiocbe;
		aiocbe = aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		if (aio_free_entry(aiocbe))
			goto restart2;
	}
	free(ki, M_AIO);
	p->p_aioinfo = NULL;
}

/*
 * Select a job to run (called by an AIO daemon)
 */
static struct aiocblist *
aio_selectjob(struct aioproclist *aiop) {

	struct aiocblist *aiocbe;

	aiocbe = TAILQ_FIRST(&aiop->jobtorun);
	if (aiocbe) {
		TAILQ_REMOVE(&aiop->jobtorun, aiocbe, list);
		return aiocbe;
	}

	for (aiocbe = TAILQ_FIRST(&aio_jobs);
		aiocbe;
		aiocbe = TAILQ_NEXT(aiocbe, list)) {
		struct kaioinfo *ki;
		struct proc *userp;

		userp = aiocbe->userproc;
		ki = userp->p_aioinfo;

		if (ki->kaio_active_count < ki->kaio_maxactive_count) {
			TAILQ_REMOVE(&aio_jobs, aiocbe, list);
			return aiocbe;
		}
	}

	return NULL;
}

/*
 * The AIO activity proper.
 */
void
aio_process(struct aiocblist *aiocbe) {
	struct filedesc *fdp;
	struct proc *userp;
	struct aiocb *cb;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	int error;
	off_t offset;

	userp = aiocbe->userproc;
	cb = &aiocbe->uaiocb;

#if DEBUGAIO > 0
	if (debugaio > 1)
		printf("AIO %s, fd: %d, offset: 0x%x, address: 0x%x, size: %d\n",
			cb->aio_lio_opcode == LIO_READ?"Read":"Write",
			cb->aio_fildes, (int) cb->aio_offset,
				cb->aio_buf, cb->aio_nbytes);
#endif
#if SLOW
	tsleep(curproc, PVM, "aioprc", hz);
#endif
	fdp = curproc->p_fd;
	/*
	 * Range check file descriptor
	 */
	fd = cb->aio_fildes;
	fp = fdp->fd_ofiles[fd];

	aiov.iov_base = cb->aio_buf;
	aiov.iov_len = cb->aio_nbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = offset = cb->aio_offset;
	auio.uio_resid = cb->aio_nbytes;
	cnt = cb->aio_nbytes;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = curproc;

	if (cb->aio_lio_opcode == LIO_READ) {
		auio.uio_rw = UIO_READ;
		error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	} else {
		auio.uio_rw = UIO_WRITE;
		error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred);
	}

	if (error) {
		if (auio.uio_resid != cnt) {
			if (error == ERESTART || error == EINTR || error == EWOULDBLOCK)
				error = 0;
			if ((error == EPIPE) && (cb->aio_lio_opcode == LIO_WRITE))
				psignal(userp, SIGPIPE);
		}
	}
#if DEBUGAIO > 0
	if (debugaio > 1)
		printf("%s complete: error: %d, status: %d, nio: %d, resid: %d, offset: %d\n",
	cb->aio_lio_opcode == LIO_READ?"Read":"Write",
error, cnt, cnt - auio.uio_resid, auio.uio_resid, (int) offset & 0xffffffff);
#endif

	cnt -= auio.uio_resid;
	cb->_aiocb_private.error = error;
	cb->_aiocb_private.status = cnt;
	
	return;

}

/*
 * The AIO daemon.
 */
static void
aio_startproc(void *uproc)
{
	struct aioproclist *aiop;

	/*
	 * Allocate and ready the aio control info
	 */
	aiop = malloc(sizeof *aiop, M_AIO, M_WAITOK);
	aiop->aioproc = curproc;
	aiop->aioprocflags |= AIOP_FREE;
	TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);
	TAILQ_INIT(&aiop->jobtorun);

	/*
	 * Get rid of current address space
	 */
	if (curproc->p_vmspace->vm_refcnt == 1) {
		if (curproc->p_vmspace->vm_shm)
			shmexit(curproc);
		pmap_remove_pages(&curproc->p_vmspace->vm_pmap, 0, USRSTACK);
		vm_map_remove(&curproc->p_vmspace->vm_map, 0, USRSTACK);
	} else {
		vmspace_exec(curproc);
	}

	/*
	 * Make up a name for the daemon
	 */
	strcpy(curproc->p_comm, "aiodaemon");

	/*
	 * Get rid of our current filedescriptors
	 */
	fdfree(curproc);
	curproc->p_fd = NULL;
	curproc->p_ucred = crcopy(curproc->p_ucred);
	curproc->p_ucred->cr_uid = 0;
	curproc->p_ucred->cr_groups[0] = 1;
	curproc->p_flag |= P_SYSTEM;

#if DEBUGAIO > 0
	if (debugaio > 2)
		printf("Started new process: %d\n", curproc->p_pid);
#endif
	wakeup(&aio_freeproc);

	while(1) {
		struct vmspace *myvm, *tmpvm;
		struct proc *cp = curproc;
		struct proc *up = NULL;
		struct	aiocblist *aiocbe;

		if ((aiop->aioprocflags & AIOP_FREE) == 0) {
			TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);
			aiop->aioprocflags |= AIOP_FREE;
		}
		if (tsleep(cp, PRIBIO, "aiordy", hz*30)) {
			if ((num_aio_procs > target_aio_procs) &&
				(TAILQ_FIRST(&aiop->jobtorun) == NULL))
				exit1(curproc, 0);
		}

		if (aiop->aioprocflags & AIOP_FREE) {
			TAILQ_REMOVE(&aio_freeproc, aiop, list);
			TAILQ_INSERT_TAIL(&aio_activeproc, aiop, list);
			aiop->aioprocflags &= ~AIOP_FREE;
		}

		myvm = curproc->p_vmspace;

		while ( aiocbe = aio_selectjob(aiop)) {
			struct aiocb *cb;
			struct kaioinfo *ki;
			struct proc *userp;

			cb = &aiocbe->uaiocb;
			userp = aiocbe->userproc;
			ki = userp->p_aioinfo;

			aiocbe->jobstate = JOBST_JOBRUNNING;
			if (userp != cp) {
				tmpvm = curproc->p_vmspace;
				curproc->p_vmspace = userp->p_vmspace;
				++curproc->p_vmspace->vm_refcnt;
				pmap_activate(curproc);
				if (tmpvm != myvm) {
					vmspace_free(tmpvm);
				}
				if (curproc->p_fd)
					fdfree(curproc);
				curproc->p_fd = fdshare(userp);
				cp = userp;
			}

			ki->kaio_active_count++;
#if DEBUGAIO > 0
			if (debugaio > 0)
				printf("process: pid: %d(%d), active: %d, queue: %d\n",
					cb->_aiocb_private.kernelinfo,
					userp->p_pid, ki->kaio_active_count, ki->kaio_queue_count);
#endif
			aiocbe->jobaioproc = aiop;
			aio_process(aiocbe);
			--ki->kaio_active_count;
			if (ki->kaio_active_count == 0)
				wakeup(ki);
#if DEBUGAIO > 0
			if (debugaio > 0)
				printf("DONE process: pid: %d(%d), active: %d, queue: %d\n",
					cb->_aiocb_private.kernelinfo,
					userp->p_pid, ki->kaio_active_count, ki->kaio_queue_count);
#endif

			aiocbe->jobstate = JOBST_JOBFINISHED;

			if (aiocbe->jobflags & AIOCBLIST_ASYNCFREE) {
				aiocbe->jobflags &= ~AIOCBLIST_ASYNCFREE;
				TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
			} else {
				TAILQ_REMOVE(&ki->kaio_jobqueue,
					aiocbe, plist);
				TAILQ_INSERT_TAIL(&ki->kaio_jobdone,
					aiocbe, plist);
			}

			if (aiocbe->jobflags & AIOCBLIST_RUNDOWN) {
				wakeup(aiocbe);
				aiocbe->jobflags &= ~AIOCBLIST_RUNDOWN;
			}

			if (aiocbe->jobflags & AIOCBLIST_SUSPEND) {
				wakeup(userp);
				aiocbe->jobflags &= ~AIOCBLIST_SUSPEND;
			}

			if (cb->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
				psignal(userp, cb->aio_sigevent.sigev_signo);
			}
		}

		if (cp != curproc) {
			tmpvm = curproc->p_vmspace;
			curproc->p_vmspace = myvm;
			pmap_activate(curproc);
			vmspace_free(tmpvm);
			if (curproc->p_fd)
				fdfree(curproc);
			curproc->p_fd = NULL;
			cp = curproc;
		}
	}
}

/*
 * Create a new AIO daemon.
 */
static int
aio_newproc() {
	int error;
	int rval[2];
	struct rfork_args rfa;
	struct proc *p;

	rfa.flags = RFMEM | RFPROC | RFCFDG;

	if (error = rfork(curproc, &rfa, &rval[0]))
		return error;

	cpu_set_fork_handler(p = pfind(rval[0]), aio_startproc, curproc);

#if DEBUGAIO > 0
	if (debugaio > 2)
		printf("Waiting for new process: %d, count: %d\n",
			curproc->p_pid, num_aio_procs);
#endif

	error = tsleep(&aio_freeproc, PZERO, "aiosta", 5*hz);
	++num_aio_procs;

	return error;

}

/*
 * Queue a new AIO request.
 */
static int
_aio_aqueue(struct proc *p, struct aiocb *job, int type) {
	struct filedesc *fdp;
	struct file *fp;
	unsigned int fd;

	int error;
	int opcode;
	struct aiocblist *aiocbe;
	struct aioproclist *aiop;
	struct kaioinfo *ki;

	if (aiocbe = TAILQ_FIRST(&aio_freejobs)) {
		TAILQ_REMOVE(&aio_freejobs, aiocbe, list);
	} else {
		aiocbe = malloc (sizeof *aiocbe, M_AIO, M_WAITOK);
	}

	error = copyin((caddr_t)job,
		(caddr_t) &aiocbe->uaiocb, sizeof aiocbe->uaiocb);
	if (error) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("aio_aqueue: Copyin error: %d\n", error);
#endif
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		return error;
	}

	/*
	 * Get the opcode
	 */
	if (type != LIO_NOP) {
		aiocbe->uaiocb.aio_lio_opcode = type;
	}
	opcode = aiocbe->uaiocb.aio_lio_opcode;

	/*
	 * Get the fd info for process
	 */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor
	 */
	fd = aiocbe->uaiocb.aio_fildes;
	if (fd >= fdp->fd_nfiles) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
#if DEBUGAIO > 0
			if (debugaio > 0)
				printf("aio_aqueue: Null type\n");
#endif
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, EBADF);
		}
		return EBADF;
	}

#if DEBUGAIO > 0
	if (debugaio > 3)
		printf("aio_aqueue: fd: %d, cmd: %d, buf: %d, cnt: %d, fileoffset: %d\n",
			aiocbe->uaiocb.aio_fildes,
			aiocbe->uaiocb.aio_lio_opcode,
			(int) aiocbe->uaiocb.aio_buf & 0xffffffff,
			aiocbe->uaiocb.aio_nbytes,
			(int) aiocbe->uaiocb.aio_offset & 0xffffffff);
#endif
		

	fp = fdp->fd_ofiles[fd];
	if ((fp == NULL) ||
		((opcode == LIO_WRITE) && ((fp->f_flag & FWRITE) == 0))) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, EBADF);
		}
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("aio_aqueue: Bad file descriptor\n");
#endif
		return EBADF;
	}

	if (aiocbe->uaiocb.aio_offset == -1LL) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, EINVAL);
		}
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("aio_aqueue: bad offset\n");
#endif
		return EINVAL;
	}

#if DEBUGAIO > 0
	if (debugaio > 2)
		printf("job addr: 0x%x, 0x%x, %d\n", job, &job->_aiocb_private.kernelinfo, jobrefid);
#endif

	error = suword(&job->_aiocb_private.kernelinfo, jobrefid);
	if (error) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, EINVAL);
		}
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("aio_aqueue: fetch of kernelinfo from user space\n");
#endif
		return error;
	}

	aiocbe->uaiocb._aiocb_private.kernelinfo = (void *)jobrefid;
#if DEBUGAIO > 0
	if (debugaio > 2)
		printf("aio_aqueue: New job: %d...  ", jobrefid);
#endif
	++jobrefid;
	
	if (opcode == LIO_NOP) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, 0);
		}
		return 0;
	}

	if ((opcode != LIO_NOP) &&
		(opcode != LIO_READ) && (opcode != LIO_WRITE)) {
		TAILQ_INSERT_HEAD(&aio_freejobs, aiocbe, list);
		if (type == 0) {
			suword(&job->_aiocb_private.status, -1);
			suword(&job->_aiocb_private.error, EINVAL);
		}
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("aio_aqueue: invalid LIO op: %d\n", opcode);
#endif
		return EINVAL;
	}

	suword(&job->_aiocb_private.error, 0);
	suword(&job->_aiocb_private.status, 0);
	aiocbe->userproc = p;
	aiocbe->jobflags = 0;
	ki = p->p_aioinfo;
	++num_queue_count;
	++ki->kaio_queue_count;

retryproc:
	if (aiop = TAILQ_FIRST(&aio_freeproc)) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("found a free AIO process\n");
#endif
		TAILQ_REMOVE(&aio_freeproc, aiop, list);
		TAILQ_INSERT_TAIL(&aio_activeproc, aiop, list);
		aiop->aioprocflags &= ~AIOP_FREE;
		TAILQ_INSERT_TAIL(&aiop->jobtorun, aiocbe, list);
		TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, aiocbe, plist);
		aiocbe->jobstate = JOBST_JOBQPROC;

		aiocbe->jobaioproc = aiop;
		wakeup(aiop->aioproc);
	} else if ((num_aio_procs < max_aio_procs) &&
			(ki->kaio_active_count < ki->kaio_maxactive_count)) {
#if DEBUGAIO > 0
		if (debugaio > 1) {
			printf("aio_aqueue: starting new proc: num_aio_procs(%d), max_aio_procs(%d)\n", num_aio_procs, max_aio_procs);
			printf("            ki->kaio_active_count(%d), ki->kaio_maxactive_count(%d)\n", ki->kaio_active_count, ki->kaio_maxactive_count);
		}
#endif
		if (error = aio_newproc()) {
#if DEBUGAIO > 0
			if (debugaio > 0)
				printf("aio_aqueue: problem sleeping for starting proc: %d\n",
					error);
#endif
		}
		goto retryproc;
	} else {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("queuing to global queue\n");
#endif
		TAILQ_INSERT_TAIL(&aio_jobs, aiocbe, list);
		TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, aiocbe, plist);
		aiocbe->jobstate = JOBST_JOBQGLOBAL;
	}

	return 0;
}

static int
aio_aqueue(struct proc *p, struct aiocb *job, int type) {
	struct kaioinfo *ki;

	if (p->p_aioinfo == NULL) {
		aio_init_aioinfo(p);
	}

	if (num_queue_count >= max_queue_count)
		return EAGAIN;

	ki = p->p_aioinfo;
	if (ki->kaio_queue_count >= ki->kaio_qallowed_count)
		return EAGAIN;

	return _aio_aqueue(p, job, type);
}

/*
 * Support the aio_return system call
 */
int
aio_return(struct proc *p, struct aio_return_args *uap, int *retval) {
	int jobref, status;
	struct aiocblist *cb;
	struct kaioinfo *ki;
	struct proc *userp;

	ki = p->p_aioinfo;
	if (ki == NULL) {
		return EINVAL;
	}

	jobref = fuword(&uap->aiocbp->_aiocb_private.kernelinfo);
	if (jobref == -1)
		return EINVAL;

#if DEBUGAIO > 0
	if (debugaio > 0)
		printf("aio_return: jobref: %d\n", jobref);
#endif

	
	for (cb = TAILQ_FIRST(&ki->kaio_jobdone);
		cb;
		cb = TAILQ_NEXT(cb, plist)) {
		if (((int) cb->uaiocb._aiocb_private.kernelinfo) == jobref) {
			retval[0] = cb->uaiocb._aiocb_private.status;
			aio_free_entry(cb);
			return 0;
		}
	}

	status = fuword(&uap->aiocbp->_aiocb_private.status);
	if (status == -1)
		return 0;
			
	return (EINVAL);
}

/*
 * Rundown the jobs for a given process.  
 */
void
aio_marksuspend(struct proc *p, int njobs, int *joblist, int set) {
	struct aiocblist *aiocbe;
	struct kaioinfo *ki;
	
	ki = p->p_aioinfo;
	if (ki == NULL)
		return;

	for (aiocbe = TAILQ_FIRST(&ki->kaio_jobqueue);
		aiocbe;
		aiocbe = TAILQ_NEXT(aiocbe, plist)) {

		if (njobs) {

			int i;

			for(i = 0; i < njobs; i++) {
				if (((int) aiocbe->uaiocb._aiocb_private.kernelinfo) == joblist[i])
					break;
			}

			if (i == njobs)
				continue;
		}

		if (set)
			aiocbe->jobflags |= AIOCBLIST_SUSPEND;
		else
			aiocbe->jobflags &= ~AIOCBLIST_SUSPEND;
	}
}

/*
 * Allow a process to wakeup when any of the I/O requests are
 * completed.
 */
int
aio_suspend(struct proc *p, struct aio_suspend_args *uap, int *retval) {
	struct timeval atv, utv;
	struct timespec ts;
	struct aiocb *const *cbptr, *cbp;
	struct kaioinfo *ki;
	struct aiocblist *cb;
	int i;
	int error, s, timo;
	int *joblist;

	
	timo = 0;
	if (uap->timeout) {
		/*
		 * Get timespec struct
		 */
		if (error = copyin((caddr_t) uap->timeout, (caddr_t) &ts, sizeof ts)) {
			return error;
		}

		if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
			return (EINVAL);

		TIMESPEC_TO_TIMEVAL(&atv, &ts)
		if (itimerfix(&atv))
			return (EINVAL);
		/*
		 * XXX this is not as careful as settimeofday() about minimising
		 * interrupt latency.  The hzto() interface is inconvenient as usual.
		 */
		s = splclock();
		timevaladd(&atv, &time);
		timo = hzto(&atv);
		splx(s);
		if (timo == 0)
			timo = 1;
	}

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EAGAIN;

	joblist = malloc(uap->nent * sizeof(int), M_TEMP, M_WAITOK);
	cbptr = uap->aiocbp;

	for(i=0;i<uap->nent;i++) {
		cbp = (struct aiocb *) fuword((caddr_t) &cbptr[i]);
#if DEBUGAIO > 1
		if (debugaio > 2)
			printf("cbp: %x\n", cbp);
#endif
		joblist[i] = fuword(&cbp->_aiocb_private.kernelinfo);
		cbptr++;
	}


	while (1) {
		for (cb = TAILQ_FIRST(&ki->kaio_jobdone);
			cb;
			cb = TAILQ_NEXT(cb, plist)) {
			for(i=0;i<uap->nent;i++) {
				if (((int) cb->uaiocb._aiocb_private.kernelinfo) == joblist[i]) {
					free(joblist, M_TEMP);
					return 0;
				}
			}
		}

#if DEBUGAIO > 0
	if (debugaio > 0) {
		printf("Suspend, timeout: %d clocks, jobs:", timo);
		for(i=0;i<uap->nent;i++)
			printf(" %d", joblist[i]);
		printf("\n");
	}
#endif

		aio_marksuspend(p, uap->nent, joblist, 1);
#if DEBUGAIO > 0
		if (debugaio > 2) {
			printf("Suspending -- waiting for all I/O's to complete: ");
			for(i=0;i<uap->nent;i++)
				printf(" %d", joblist[i]);
			printf("\n");
		}
#endif
		error = tsleep(p, PRIBIO|PCATCH, "aiospn", timo);
		aio_marksuspend(p, uap->nent, joblist, 0);

		if (error == EINTR) {
#if DEBUGAIO > 0
			if (debugaio > 2)
				printf(" signal\n");
#endif
			free(joblist, M_TEMP);
			return EINTR;
		} else if (error == EWOULDBLOCK) {
#if DEBUGAIO > 0
			if (debugaio > 2)
				printf(" timeout\n");
#endif
			free(joblist, M_TEMP);
			return EAGAIN;
		}
#if DEBUGAIO > 0
		if (debugaio > 2)
			printf("\n");
#endif
	}

/* NOTREACHED */
	return EINVAL;
}

/*
 * aio_cancel at the kernel level is a NOOP right now.  It
 * might be possible to support it partially in user mode, or
 * in kernel mode later on.
 */
int
aio_cancel(struct proc *p, struct aio_cancel_args *uap, int *retval) {
	return AIO_NOTCANCELLED;
}

/*
 * aio_error is implemented in the kernel level for compatibility
 * purposes only.  For a user mode async implementation, it would be
 * best to do it in a userland subroutine.
 */
int
aio_error(struct proc *p, struct aio_error_args *uap, int *retval) {
	int activeflag, errorcode;
	struct aiocblist *cb;
	struct kaioinfo *ki;
	int jobref;
	int error, status;

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EINVAL;

	jobref = fuword(&uap->aiocbp->_aiocb_private.kernelinfo);
	if (jobref == -1)
		return EFAULT;

	for (cb = TAILQ_FIRST(&ki->kaio_jobdone);
		cb;
		cb = TAILQ_NEXT(cb, plist)) {

		if (((int) cb->uaiocb._aiocb_private.kernelinfo) == jobref) {
			retval[0] = cb->uaiocb._aiocb_private.error;
			return 0;
		}
	}

	for (cb = TAILQ_FIRST(&ki->kaio_jobqueue);
		cb;
		cb = TAILQ_NEXT(cb, plist)) {

		if (((int) cb->uaiocb._aiocb_private.kernelinfo) == jobref) {
			retval[0] = EINPROGRESS;
			return 0;
		}
	}

	/*
	 * Hack for lio
	 */
	status = fuword(&uap->aiocbp->_aiocb_private.status);
	if (status == -1) {
		return fuword(&uap->aiocbp->_aiocb_private.error);
	}
	return EINVAL;
}

int
aio_read(struct proc *p, struct aio_read_args *uap, int *retval) {
	struct filedesc *fdp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	struct aiocb iocb;
	int error, pmodes;

	pmodes = fuword(&uap->aiocbp->_aiocb_private.privatemodes);
	if ((pmodes & AIO_PMODE_SYNC) == 0) {
#if DEBUGAIO > 1
		if (debugaio > 2)
			printf("queueing aio_read\n");
#endif
		return aio_aqueue(p, (struct aiocb *) uap->aiocbp, LIO_READ);
	}

	/*
	 * Get control block
	 */
	if (error = copyin((caddr_t) uap->aiocbp, (caddr_t) &iocb, sizeof iocb))
		return error;

	/*
	 * Get the fd info for process
	 */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor
	 */
	fd = iocb.aio_fildes;
	if (fd >= fdp->fd_nfiles)
		return EBADF;
	fp = fdp->fd_ofiles[fd];
	if ((fp == NULL) || ((fp->f_flag & FREAD) == 0))
		return EBADF;
	if (iocb.aio_offset == -1LL)
		return EINVAL;

	auio.uio_resid = iocb.aio_nbytes;
	if (auio.uio_resid < 0)
		return (EINVAL);

	/*
	 * Process sync simply -- queue async request.
	 */
	if ((iocb._aiocb_private.privatemodes & AIO_PMODE_SYNC) == 0) {
		return aio_aqueue(p, (struct aiocb *) uap->aiocbp, LIO_READ);
	}

	aiov.iov_base = iocb.aio_buf;
	aiov.iov_len = iocb.aio_nbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = iocb.aio_offset;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	cnt = iocb.aio_nbytes;
	error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	if (error &&
		(auio.uio_resid != cnt) &&
		(error == ERESTART || error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
	*retval = cnt;
	return error;
}

int
aio_write(struct proc *p, struct aio_write_args *uap, int *retval) {
	struct filedesc *fdp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	unsigned int fd;
	int cnt;
	struct aiocb iocb;
	int error;
	int pmodes;

	/*
	 * Process sync simply -- queue async request.
	 */
	pmodes = fuword(&uap->aiocbp->_aiocb_private.privatemodes);
	if ((pmodes & AIO_PMODE_SYNC) == 0) {
#if DEBUGAIO > 1
		if (debugaio > 2)
			printf("queing aio_write\n");
#endif
		return aio_aqueue(p, (struct aiocb *) uap->aiocbp, LIO_WRITE);
	}

	if (error = copyin((caddr_t) uap->aiocbp, (caddr_t) &iocb, sizeof iocb))
		return error;

	/*
	 * Get the fd info for process
	 */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor
	 */
	fd = iocb.aio_fildes;
	if (fd >= fdp->fd_nfiles)
		return EBADF;
	fp = fdp->fd_ofiles[fd];
	if ((fp == NULL) || ((fp->f_flag & FWRITE) == 0))
		return EBADF;
	if (iocb.aio_offset == -1LL)
		return EINVAL;

	aiov.iov_base = iocb.aio_buf;
	aiov.iov_len = iocb.aio_nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = iocb.aio_offset;

	auio.uio_resid = iocb.aio_nbytes;
	if (auio.uio_resid < 0)
		return (EINVAL);

	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;

	cnt = iocb.aio_nbytes;
	error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred);
	if (error) {
		if (auio.uio_resid != cnt) {
			if (error == ERESTART || error == EINTR || error == EWOULDBLOCK)
				error = 0;
			if (error == EPIPE)
				psignal(p, SIGPIPE);
		}
	}
	cnt -= auio.uio_resid;
	*retval = cnt;
	return error;
}

int
lio_listio(struct proc *p, struct lio_listio_args *uap, int *retval) {
	int cnt, nent, nentqueued;
	struct aiocb *iocb, * const *cbptr;
	struct aiocblist *cb;
	struct kaioinfo *ki;
	int error, runningcode;
	int i;

	if ((uap->mode != LIO_NOWAIT) && (uap->mode != LIO_WAIT)) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("lio_listio: bad mode: %d\n", uap->mode);
#endif
		return EINVAL;
	}

	nent = uap->nent;
	if (nent > AIO_LISTIO_MAX) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("lio_listio: nent > AIO_LISTIO_MAX: %d > %d\n", nent, AIO_LISTIO_MAX);
#endif
		return EINVAL;
	}

	if (p->p_aioinfo == NULL) {
		aio_init_aioinfo(p);
	}

	if ((nent + num_queue_count) > max_queue_count) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("lio_listio: (nent(%d) + num_queue_count(%d)) > max_queue_count(%d)\n", nent, num_queue_count, max_queue_count);
#endif
		return EAGAIN;
	}

	ki = p->p_aioinfo;
	if ((nent + ki->kaio_queue_count) > ki->kaio_qallowed_count) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("lio_listio: (nent(%d) + ki->kaio_queue_count(%d)) > ki->kaio_qallowed_count(%d)\n", nent, ki->kaio_queue_count, ki->kaio_qallowed_count);
#endif
		return EAGAIN;
	}

/*
	num_queue_count += nent;
	ki->kaio_queue_count += nent;
*/
	nentqueued = 0;

/*
 * get pointers to the list of I/O requests
	iocbvec = malloc(uap->nent * sizeof(struct aiocb *), M_TEMP, M_WAITOK);
 */

	cbptr = uap->acb_list;
	for(i = 0; i < uap->nent; i++) {
		iocb = (struct aiocb *) fuword((caddr_t) &cbptr[i]);
		error = _aio_aqueue(p, iocb, 0);
		if (error == 0)
			nentqueued++;
	}

	/*
	 * If we haven't queued any, then just return error
	 */
	if (nentqueued == 0) {
#if DEBUGAIO > 0
		if (debugaio > 0)
			printf("lio_listio: none queued\n");
#endif
		return EIO;
	}

#if DEBUGAIO > 0
	if (debugaio > 0)
		printf("lio_listio: %d queued\n", nentqueued);
#endif

	/*
	 * Calculate the appropriate error return
	 */
	runningcode = 0;
	if (nentqueued != nent)
		runningcode = EIO;

	if (uap->mode == LIO_WAIT) {
		while (1) {
			for(i = 0; i < uap->nent; i++) {
				int found;
				int jobref, command, status;

				/*
				 * Fetch address of the control buf pointer in user space
				 */
				iocb = (struct aiocb *) fuword((caddr_t) &cbptr[i]);

				/*
				 * Fetch the associated command from user space
				 */
				command = fuword(&iocb->aio_lio_opcode);
				if (command == LIO_NOP)
					continue;

				/*
				 * If the status shows error or complete, then skip this entry.
				 */
				status = fuword(&iocb->_aiocb_private.status);
				if (status != 0)
					continue;

				jobref = fuword(&iocb->_aiocb_private.kernelinfo);

				found = 0;
				for (cb = TAILQ_FIRST(&ki->kaio_jobdone);
					cb;
					cb = TAILQ_NEXT(cb, plist)) {
					if (((int) cb->uaiocb._aiocb_private.kernelinfo) == jobref) {
						found++;
						break;
					}
				}
				if (found == 0)
					break;
			}

			/*
			 * If all I/Os have been disposed of, then we can return
			 */
			if (i == uap->nent) {
				return runningcode;
			}

			aio_marksuspend(p, 0, 0, 1);
			error = tsleep(p, PRIBIO|PCATCH, "aiospn", 0);
			aio_marksuspend(p, 0, 0, 0);

			if (error == EINTR) {
				return EINTR;
			} else if (error == EWOULDBLOCK) {
				return EAGAIN;
			}

		}
	}

	return runningcode;
}
