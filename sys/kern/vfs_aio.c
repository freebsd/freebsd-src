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
 * $FreeBSD$
 */

/*
 * This file contains support for the POSIX 1003.1B AIO/LIO facility.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <sys/aio.h>

#include <machine/limits.h>

#include "opt_vfs_aio.h"

/*
 * Counter for allocating reference ids to new jobs.  Wrapped to 1 on
 * overflow.
 */
static	long jobrefid;

#define JOBST_NULL		0x0
#define JOBST_JOBQGLOBAL	0x2
#define JOBST_JOBRUNNING	0x3
#define JOBST_JOBFINISHED	0x4
#define	JOBST_JOBQBUF		0x5
#define	JOBST_JOBBFINISHED	0x6

#ifndef MAX_AIO_PER_PROC
#define MAX_AIO_PER_PROC	32
#endif

#ifndef MAX_AIO_QUEUE_PER_PROC
#define MAX_AIO_QUEUE_PER_PROC	256 /* Bigger than AIO_LISTIO_MAX */
#endif

#ifndef MAX_AIO_PROCS
#define MAX_AIO_PROCS		32
#endif

#ifndef MAX_AIO_QUEUE
#define	MAX_AIO_QUEUE		1024 /* Bigger than AIO_LISTIO_MAX */
#endif

#ifndef TARGET_AIO_PROCS
#define TARGET_AIO_PROCS	4
#endif

#ifndef MAX_BUF_AIO
#define MAX_BUF_AIO		16
#endif

#ifndef AIOD_TIMEOUT_DEFAULT
#define	AIOD_TIMEOUT_DEFAULT	(10 * hz)
#endif

#ifndef AIOD_LIFETIME_DEFAULT
#define AIOD_LIFETIME_DEFAULT	(30 * hz)
#endif

SYSCTL_NODE(_vfs, OID_AUTO, aio, CTLFLAG_RW, 0, "Async IO management");

static int max_aio_procs = MAX_AIO_PROCS;
SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_procs,
	CTLFLAG_RW, &max_aio_procs, 0,
	"Maximum number of kernel threads to use for handling async IO ");

static int num_aio_procs = 0;
SYSCTL_INT(_vfs_aio, OID_AUTO, num_aio_procs,
	CTLFLAG_RD, &num_aio_procs, 0,
	"Number of presently active kernel threads for async IO");

/*
 * The code will adjust the actual number of AIO processes towards this
 * number when it gets a chance.
 */
static int target_aio_procs = TARGET_AIO_PROCS;
SYSCTL_INT(_vfs_aio, OID_AUTO, target_aio_procs, CTLFLAG_RW, &target_aio_procs,
	0, "Preferred number of ready kernel threads for async IO");

static int max_queue_count = MAX_AIO_QUEUE;
SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_queue, CTLFLAG_RW, &max_queue_count, 0,
    "Maximum number of aio requests to queue, globally");

static int num_queue_count = 0;
SYSCTL_INT(_vfs_aio, OID_AUTO, num_queue_count, CTLFLAG_RD, &num_queue_count, 0,
    "Number of queued aio requests");

static int num_buf_aio = 0;
SYSCTL_INT(_vfs_aio, OID_AUTO, num_buf_aio, CTLFLAG_RD, &num_buf_aio, 0,
    "Number of aio requests presently handled by the buf subsystem");

/* Number of async I/O thread in the process of being started */
/* XXX This should be local to _aio_aqueue() */
static int num_aio_resv_start = 0;

static int aiod_timeout;
SYSCTL_INT(_vfs_aio, OID_AUTO, aiod_timeout, CTLFLAG_RW, &aiod_timeout, 0,
    "Timeout value for synchronous aio operations");

static int aiod_lifetime;
SYSCTL_INT(_vfs_aio, OID_AUTO, aiod_lifetime, CTLFLAG_RW, &aiod_lifetime, 0,
    "Maximum lifetime for idle aiod");

static int unloadable = 0;
SYSCTL_INT(_vfs_aio, OID_AUTO, unloadable, CTLFLAG_RW, &unloadable, 0,
    "Allow unload of aio (not recommended)");


static int max_aio_per_proc = MAX_AIO_PER_PROC;
SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_per_proc, CTLFLAG_RW, &max_aio_per_proc,
    0, "Maximum active aio requests per process (stored in the process)");

static int max_aio_queue_per_proc = MAX_AIO_QUEUE_PER_PROC;
SYSCTL_INT(_vfs_aio, OID_AUTO, max_aio_queue_per_proc, CTLFLAG_RW,
    &max_aio_queue_per_proc, 0,
    "Maximum queued aio requests per process (stored in the process)");

static int max_buf_aio = MAX_BUF_AIO;
SYSCTL_INT(_vfs_aio, OID_AUTO, max_buf_aio, CTLFLAG_RW, &max_buf_aio, 0,
    "Maximum buf aio requests per process (stored in the process)");

struct aiocblist {
        TAILQ_ENTRY(aiocblist) list;	/* List of jobs */
        TAILQ_ENTRY(aiocblist) plist;	/* List of jobs for proc */
        int	jobflags;
        int	jobstate;
	int	inputcharge;
	int	outputcharge;
	struct	callout_handle timeouthandle;
        struct	buf *bp;		/* Buffer pointer */
        struct	proc *userproc;		/* User process */ /* Not td! */
        struct	file *fd_file;		/* Pointer to file structure */ 
        struct	aio_liojob *lio;	/* Optional lio job */
        struct	aiocb *uuaiocb;		/* Pointer in userspace of aiocb */
	struct	klist klist;		/* list of knotes */
        struct	aiocb uaiocb;		/* Kernel I/O control block */
};

/* jobflags */
#define AIOCBLIST_RUNDOWN       0x4
#define AIOCBLIST_ASYNCFREE     0x8
#define AIOCBLIST_DONE          0x10

/*
 * AIO process info
 */
#define AIOP_FREE	0x1			/* proc on free queue */
#define AIOP_SCHED	0x2			/* proc explicitly scheduled */

struct aiothreadlist {
	int aiothreadflags;			/* AIO proc flags */
	TAILQ_ENTRY(aiothreadlist) list;	/* List of processes */
	struct thread *aiothread;		/* The AIO thread */
};

/*
 * data-structure for lio signal management
 */
struct aio_liojob {
	int	lioj_flags;
	int	lioj_buffer_count;
	int	lioj_buffer_finished_count;
	int	lioj_queue_count;
	int	lioj_queue_finished_count;
	struct	sigevent lioj_signal;	/* signal on all I/O done */
	TAILQ_ENTRY(aio_liojob) lioj_list;
	struct	kaioinfo *lioj_ki;
};
#define	LIOJ_SIGNAL		0x1	/* signal on all done (lio) */
#define	LIOJ_SIGNAL_POSTED	0x2	/* signal has been posted */

/*
 * per process aio data structure
 */
struct kaioinfo {
	int	kaio_flags;		/* per process kaio flags */
	int	kaio_maxactive_count;	/* maximum number of AIOs */
	int	kaio_active_count;	/* number of currently used AIOs */
	int	kaio_qallowed_count;	/* maxiumu size of AIO queue */
	int	kaio_queue_count;	/* size of AIO queue */
	int	kaio_ballowed_count;	/* maximum number of buffers */
	int	kaio_queue_finished_count; /* number of daemon jobs finished */
	int	kaio_buffer_count;	/* number of physio buffers */
	int	kaio_buffer_finished_count; /* count of I/O done */
	struct 	proc *kaio_p;		/* process that uses this kaio block */
	TAILQ_HEAD(,aio_liojob) kaio_liojoblist; /* list of lio jobs */
	TAILQ_HEAD(,aiocblist) kaio_jobqueue;	/* job queue for process */
	TAILQ_HEAD(,aiocblist) kaio_jobdone;	/* done queue for process */
	TAILQ_HEAD(,aiocblist) kaio_bufqueue;	/* buffer job queue for process */
	TAILQ_HEAD(,aiocblist) kaio_bufdone;	/* buffer done queue for process */
	TAILQ_HEAD(,aiocblist) kaio_sockqueue;	/* queue for aios waiting on sockets */
};

#define KAIO_RUNDOWN	0x1	/* process is being run down */
#define KAIO_WAKEUP	0x2	/* wakeup process when there is a significant event */

static TAILQ_HEAD(,aiothreadlist) aio_activeproc;	/* Active daemons */
static TAILQ_HEAD(,aiothreadlist) aio_freeproc;		/* Idle daemons */
static TAILQ_HEAD(,aiocblist) aio_jobs;			/* Async job list */
static TAILQ_HEAD(,aiocblist) aio_bufjobs;		/* Phys I/O job list */

static void	aio_init_aioinfo(struct proc *p);
static void	aio_onceonly(void);
static int	aio_free_entry(struct aiocblist *aiocbe);
static void	aio_process(struct aiocblist *aiocbe);
static int	aio_newproc(void);
static int	aio_aqueue(struct thread *td, struct aiocb *job, int type);
static void	aio_physwakeup(struct buf *bp);
static void	aio_proc_rundown(struct proc *p);
static int	aio_fphysio(struct aiocblist *aiocbe);
static int	aio_qphysio(struct proc *p, struct aiocblist *iocb);
static void	aio_daemon(void *uproc);
static void	aio_swake_cb(struct socket *, struct sockbuf *);
static int	aio_unload(void);
static void	process_signal(void *aioj);
static int	filt_aioattach(struct knote *kn);
static void	filt_aiodetach(struct knote *kn);
static int	filt_aio(struct knote *kn, long hint);

/*
 * Zones for:
 * 	kaio	Per process async io info
 *	aiop	async io thread data
 *	aiocb	async io jobs
 *	aiol	list io job pointer - internal to aio_suspend XXX
 *	aiolio	list io jobs
 */
static uma_zone_t kaio_zone, aiop_zone, aiocb_zone, aiol_zone, aiolio_zone;

/* kqueue filters for aio */
static struct filterops aio_filtops =
	{ 0, filt_aioattach, filt_aiodetach, filt_aio };

/*
 * Main operations function for use as a kernel module.
 */
static int
aio_modload(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD:
		aio_onceonly();
		break;
	case MOD_UNLOAD:
		error = aio_unload();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static moduledata_t aio_mod = {
	"aio",
	&aio_modload,
	NULL
};

SYSCALL_MODULE_HELPER(aio_return);
SYSCALL_MODULE_HELPER(aio_suspend);
SYSCALL_MODULE_HELPER(aio_cancel);
SYSCALL_MODULE_HELPER(aio_error);
SYSCALL_MODULE_HELPER(aio_read);
SYSCALL_MODULE_HELPER(aio_write);
SYSCALL_MODULE_HELPER(aio_waitcomplete);
SYSCALL_MODULE_HELPER(lio_listio);

DECLARE_MODULE(aio, aio_mod,
	SI_SUB_VFS, SI_ORDER_ANY);
MODULE_VERSION(aio, 1);

/*
 * Startup initialization
 */
static void
aio_onceonly(void)
{

	/* XXX: should probably just use so->callback */
	aio_swake = &aio_swake_cb;
	at_exit(aio_proc_rundown);
	at_exec(aio_proc_rundown);
	kqueue_add_filteropts(EVFILT_AIO, &aio_filtops);
	TAILQ_INIT(&aio_freeproc);
	TAILQ_INIT(&aio_activeproc);
	TAILQ_INIT(&aio_jobs);
	TAILQ_INIT(&aio_bufjobs);
	kaio_zone = uma_zcreate("AIO", sizeof(struct kaioinfo), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiop_zone = uma_zcreate("AIOP", sizeof(struct aiothreadlist), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiocb_zone = uma_zcreate("AIOCB", sizeof(struct aiocblist), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiol_zone = uma_zcreate("AIOL", AIO_LISTIO_MAX*sizeof(intptr_t) , NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiolio_zone = uma_zcreate("AIOLIO", sizeof(struct aio_liojob), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiod_timeout = AIOD_TIMEOUT_DEFAULT;
	aiod_lifetime = AIOD_LIFETIME_DEFAULT;
	jobrefid = 1;
}

/*
 * Callback for unload of AIO when used as a module.
 */
static int
aio_unload(void)
{

	/*
	 * XXX: no unloads by default, it's too dangerous.
	 * perhaps we could do it if locked out callers and then
	 * did an aio_proc_rundown() on each process.
	 */
	if (!unloadable)
		return (EOPNOTSUPP);

	aio_swake = NULL;
	rm_at_exit(aio_proc_rundown);
	rm_at_exec(aio_proc_rundown);
	kqueue_del_filteropts(EVFILT_AIO);
	return (0);
}

/*
 * Init the per-process aioinfo structure.  The aioinfo limits are set
 * per-process for user limit (resource) management.
 */
static void
aio_init_aioinfo(struct proc *p)
{
	struct kaioinfo *ki;
	if (p->p_aioinfo == NULL) {
		ki = uma_zalloc(kaio_zone, M_WAITOK);
		p->p_aioinfo = ki;
		ki->kaio_flags = 0;
		ki->kaio_maxactive_count = max_aio_per_proc;
		ki->kaio_active_count = 0;
		ki->kaio_qallowed_count = max_aio_queue_per_proc;
		ki->kaio_queue_count = 0;
		ki->kaio_ballowed_count = max_buf_aio;
		ki->kaio_buffer_count = 0;
		ki->kaio_buffer_finished_count = 0;
		ki->kaio_p = p;
		TAILQ_INIT(&ki->kaio_jobdone);
		TAILQ_INIT(&ki->kaio_jobqueue);
		TAILQ_INIT(&ki->kaio_bufdone);
		TAILQ_INIT(&ki->kaio_bufqueue);
		TAILQ_INIT(&ki->kaio_liojoblist);
		TAILQ_INIT(&ki->kaio_sockqueue);
	}
	
	while (num_aio_procs < target_aio_procs)
		aio_newproc();
}

/*
 * Free a job entry.  Wait for completion if it is currently active, but don't
 * delay forever.  If we delay, we return a flag that says that we have to
 * restart the queue scan.
 */
static int
aio_free_entry(struct aiocblist *aiocbe)
{
	struct kaioinfo *ki;
	struct aio_liojob *lj;
	struct proc *p;
	int error;
	int s;

	if (aiocbe->jobstate == JOBST_NULL)
		panic("aio_free_entry: freeing already free job");

	p = aiocbe->userproc;
	ki = p->p_aioinfo;
	lj = aiocbe->lio;
	if (ki == NULL)
		panic("aio_free_entry: missing p->p_aioinfo");

	while (aiocbe->jobstate == JOBST_JOBRUNNING) {
		if (aiocbe->jobflags & AIOCBLIST_ASYNCFREE)
			return 0;
		aiocbe->jobflags |= AIOCBLIST_RUNDOWN;
		tsleep(aiocbe, PRIBIO, "jobwai", 0);
	}
	aiocbe->jobflags &= ~AIOCBLIST_ASYNCFREE;

	if (aiocbe->bp == NULL) {
		if (ki->kaio_queue_count <= 0)
			panic("aio_free_entry: process queue size <= 0");
		if (num_queue_count <= 0)
			panic("aio_free_entry: system wide queue size <= 0");
	
		if (lj) {
			lj->lioj_queue_count--;
			if (aiocbe->jobflags & AIOCBLIST_DONE)
				lj->lioj_queue_finished_count--;
		}
		ki->kaio_queue_count--;
		if (aiocbe->jobflags & AIOCBLIST_DONE)
			ki->kaio_queue_finished_count--;
		num_queue_count--;
	} else {
		if (lj) {
			lj->lioj_buffer_count--;
			if (aiocbe->jobflags & AIOCBLIST_DONE)
				lj->lioj_buffer_finished_count--;
		}
		if (aiocbe->jobflags & AIOCBLIST_DONE)
			ki->kaio_buffer_finished_count--;
		ki->kaio_buffer_count--;
		num_buf_aio--;
	}

	/* aiocbe is going away, we need to destroy any knotes */
	/* XXXKSE Note the thread here is used to eventually find the 
	 * owning process again, but it is also used to do a fo_close
	 * and that requires the thread. (but does it require the
	 * OWNING thread? (or maybe the running thread?)
	 * There is a semantic problem here... 
	 */
	knote_remove(FIRST_THREAD_IN_PROC(p), &aiocbe->klist); /* XXXKSE */

	if ((ki->kaio_flags & KAIO_WAKEUP) || ((ki->kaio_flags & KAIO_RUNDOWN)
	    && ((ki->kaio_buffer_count == 0) && (ki->kaio_queue_count == 0)))) {
		ki->kaio_flags &= ~KAIO_WAKEUP;
		wakeup(p);
	}

	if (aiocbe->jobstate == JOBST_JOBQBUF) {
		if ((error = aio_fphysio(aiocbe)) != 0)
			return error;
		if (aiocbe->jobstate != JOBST_JOBBFINISHED)
			panic("aio_free_entry: invalid physio finish-up state");
		s = splbio();
		TAILQ_REMOVE(&ki->kaio_bufdone, aiocbe, plist);
		splx(s);
	} else if (aiocbe->jobstate == JOBST_JOBQGLOBAL) {
		s = splnet();
		TAILQ_REMOVE(&aio_jobs, aiocbe, list);
		TAILQ_REMOVE(&ki->kaio_jobqueue, aiocbe, plist);
		splx(s);
	} else if (aiocbe->jobstate == JOBST_JOBFINISHED)
		TAILQ_REMOVE(&ki->kaio_jobdone, aiocbe, plist);
	else if (aiocbe->jobstate == JOBST_JOBBFINISHED) {
		s = splbio();
		TAILQ_REMOVE(&ki->kaio_bufdone, aiocbe, plist);
		splx(s);
		if (aiocbe->bp) {
			vunmapbuf(aiocbe->bp);
			relpbuf(aiocbe->bp, NULL);
			aiocbe->bp = NULL;
		}
	}
	if (lj && (lj->lioj_buffer_count == 0) && (lj->lioj_queue_count == 0)) {
		TAILQ_REMOVE(&ki->kaio_liojoblist, lj, lioj_list);
		uma_zfree(aiolio_zone, lj);
	}
	aiocbe->jobstate = JOBST_NULL;
	untimeout(process_signal, aiocbe, aiocbe->timeouthandle);
	fdrop(aiocbe->fd_file, curthread);
	uma_zfree(aiocb_zone, aiocbe);
	return 0;
}

/*
 * Rundown the jobs for a given process.  
 */
static void
aio_proc_rundown(struct proc *p)
{
	int s;
	struct kaioinfo *ki;
	struct aio_liojob *lj, *ljn;
	struct aiocblist *aiocbe, *aiocbn;
	struct file *fp;
	struct socket *so;

	ki = p->p_aioinfo;
	if (ki == NULL)
		return;

	ki->kaio_flags |= LIOJ_SIGNAL_POSTED;
	while ((ki->kaio_active_count > 0) || (ki->kaio_buffer_count >
	    ki->kaio_buffer_finished_count)) {
		ki->kaio_flags |= KAIO_RUNDOWN;
		if (tsleep(p, PRIBIO, "kaiowt", aiod_timeout))
			break;
	}

	/*
	 * Move any aio ops that are waiting on socket I/O to the normal job
	 * queues so they are cleaned up with any others.
	 */
	s = splnet();
	for (aiocbe = TAILQ_FIRST(&ki->kaio_sockqueue); aiocbe; aiocbe =
	    aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		fp = aiocbe->fd_file;
		if (fp != NULL) {
			so = (struct socket *)fp->f_data;
			TAILQ_REMOVE(&so->so_aiojobq, aiocbe, list);
			if (TAILQ_EMPTY(&so->so_aiojobq)) {
				so->so_snd.sb_flags &= ~SB_AIO;
				so->so_rcv.sb_flags &= ~SB_AIO;
			}
		}
		TAILQ_REMOVE(&ki->kaio_sockqueue, aiocbe, plist);
		TAILQ_INSERT_HEAD(&aio_jobs, aiocbe, list);
		TAILQ_INSERT_HEAD(&ki->kaio_jobqueue, aiocbe, plist);
	}
	splx(s);

restart1:
	for (aiocbe = TAILQ_FIRST(&ki->kaio_jobdone); aiocbe; aiocbe = aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		if (aio_free_entry(aiocbe))
			goto restart1;
	}

restart2:
	for (aiocbe = TAILQ_FIRST(&ki->kaio_jobqueue); aiocbe; aiocbe =
	    aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		if (aio_free_entry(aiocbe))
			goto restart2;
	}

/*
 * Note the use of lots of splbio here, trying to avoid splbio for long chains
 * of I/O.  Probably unnecessary.
 */
restart3:
	s = splbio();
	while (TAILQ_FIRST(&ki->kaio_bufqueue)) {
		ki->kaio_flags |= KAIO_WAKEUP;
		tsleep(p, PRIBIO, "aioprn", 0);
		splx(s);
		goto restart3;
	}
	splx(s);

restart4:
	s = splbio();
	for (aiocbe = TAILQ_FIRST(&ki->kaio_bufdone); aiocbe; aiocbe = aiocbn) {
		aiocbn = TAILQ_NEXT(aiocbe, plist);
		if (aio_free_entry(aiocbe)) {
			splx(s);
			goto restart4;
		}
	}
	splx(s);

        /*
         * If we've slept, jobs might have moved from one queue to another.
         * Retry rundown if we didn't manage to empty the queues.
         */
        if (TAILQ_FIRST(&ki->kaio_jobdone) != NULL ||
	    TAILQ_FIRST(&ki->kaio_jobqueue) != NULL ||
	    TAILQ_FIRST(&ki->kaio_bufqueue) != NULL ||
	    TAILQ_FIRST(&ki->kaio_bufdone) != NULL)
		goto restart1;

	for (lj = TAILQ_FIRST(&ki->kaio_liojoblist); lj; lj = ljn) {
		ljn = TAILQ_NEXT(lj, lioj_list);
		if ((lj->lioj_buffer_count == 0) && (lj->lioj_queue_count ==
		    0)) {
			TAILQ_REMOVE(&ki->kaio_liojoblist, lj, lioj_list);
			uma_zfree(aiolio_zone, lj);
		} else {
#ifdef DIAGNOSTIC
			printf("LIO job not cleaned up: B:%d, BF:%d, Q:%d, "
			    "QF:%d\n", lj->lioj_buffer_count,
			    lj->lioj_buffer_finished_count,
			    lj->lioj_queue_count,
			    lj->lioj_queue_finished_count);
#endif
		}
	}

	uma_zfree(kaio_zone, ki);
	p->p_aioinfo = NULL;
}

/*
 * Select a job to run (called by an AIO daemon).
 */
static struct aiocblist *
aio_selectjob(struct aiothreadlist *aiop)
{
	int s;
	struct aiocblist *aiocbe;
	struct kaioinfo *ki;
	struct proc *userp;

	s = splnet();
	for (aiocbe = TAILQ_FIRST(&aio_jobs); aiocbe; aiocbe =
	    TAILQ_NEXT(aiocbe, list)) {
		userp = aiocbe->userproc;
		ki = userp->p_aioinfo;

		if (ki->kaio_active_count < ki->kaio_maxactive_count) {
			TAILQ_REMOVE(&aio_jobs, aiocbe, list);
			splx(s);
			return aiocbe;
		}
	}
	splx(s);

	return NULL;
}

/*
 * The AIO processing activity.  This is the code that does the I/O request for
 * the non-physio version of the operations.  The normal vn operations are used,
 * and this code should work in all instances for every type of file, including
 * pipes, sockets, fifos, and regular files.
 */
static void
aio_process(struct aiocblist *aiocbe)
{
	struct thread *td;
	struct proc *mycp;
	struct aiocb *cb;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	int cnt;
	int error;
	int oublock_st, oublock_end;
	int inblock_st, inblock_end;

	td = curthread;
	mycp = td->td_proc;
	cb = &aiocbe->uaiocb;
	fp = aiocbe->fd_file;

	aiov.iov_base = (void *)(uintptr_t)cb->aio_buf;
	aiov.iov_len = cb->aio_nbytes;

	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = cb->aio_offset;
	auio.uio_resid = cb->aio_nbytes;
	cnt = cb->aio_nbytes;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_td = td;

	inblock_st = mycp->p_stats->p_ru.ru_inblock;
	oublock_st = mycp->p_stats->p_ru.ru_oublock;
	/*
	 * _aio_aqueue() acquires a reference to the file that is
	 * released in aio_free_entry().
	 */
	if (cb->aio_lio_opcode == LIO_READ) {
		auio.uio_rw = UIO_READ;
		error = fo_read(fp, &auio, fp->f_cred, FOF_OFFSET, td);
	} else {
		auio.uio_rw = UIO_WRITE;
		error = fo_write(fp, &auio, fp->f_cred, FOF_OFFSET, td);
	}
	inblock_end = mycp->p_stats->p_ru.ru_inblock;
	oublock_end = mycp->p_stats->p_ru.ru_oublock;

	aiocbe->inputcharge = inblock_end - inblock_st;
	aiocbe->outputcharge = oublock_end - oublock_st;

	if ((error) && (auio.uio_resid != cnt)) {
		if (error == ERESTART || error == EINTR || error == EWOULDBLOCK)
			error = 0;
		if ((error == EPIPE) && (cb->aio_lio_opcode == LIO_WRITE)) {
			PROC_LOCK(aiocbe->userproc);
			psignal(aiocbe->userproc, SIGPIPE);
			PROC_UNLOCK(aiocbe->userproc);
		}
	}

	cnt -= auio.uio_resid;
	cb->_aiocb_private.error = error;
	cb->_aiocb_private.status = cnt;
}

/*
 * The AIO daemon, most of the actual work is done in aio_process,
 * but the setup (and address space mgmt) is done in this routine.
 */
static void
aio_daemon(void *uproc)
{
	int s;
	struct aio_liojob *lj;
	struct aiocb *cb;
	struct aiocblist *aiocbe;
	struct aiothreadlist *aiop;
	struct kaioinfo *ki;
	struct proc *curcp, *mycp, *userp;
	struct vmspace *myvm, *tmpvm;
	struct thread *td = curthread;
	struct pgrp *newpgrp;
	struct session *newsess;

	mtx_lock(&Giant);
	/*
	 * Local copies of curproc (cp) and vmspace (myvm)
	 */
	mycp = td->td_proc;
	myvm = mycp->p_vmspace;

	if (mycp->p_textvp) {
		vrele(mycp->p_textvp);
		mycp->p_textvp = NULL;
	}

	/*
	 * Allocate and ready the aio control info.  There is one aiop structure
	 * per daemon.
	 */
	aiop = uma_zalloc(aiop_zone, M_WAITOK);
	aiop->aiothread = td;
	aiop->aiothreadflags |= AIOP_FREE;

	s = splnet();

	/*
	 * Place thread (lightweight process) onto the AIO free thread list.
	 */
	if (TAILQ_EMPTY(&aio_freeproc))
		wakeup(&aio_freeproc);
	TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);

	splx(s);

	/*
	 * Get rid of our current filedescriptors.  AIOD's don't need any
	 * filedescriptors, except as temporarily inherited from the client.
	 */
	fdfree(td);
	mycp->p_fd = NULL;

	mtx_unlock(&Giant);
	/* The daemon resides in its own pgrp. */
	MALLOC(newpgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		M_WAITOK | M_ZERO);
	MALLOC(newsess, struct session *, sizeof(struct session), M_SESSION,
		M_WAITOK | M_ZERO);

	sx_xlock(&proctree_lock);
	enterpgrp(mycp, mycp->p_pid, newpgrp, newsess);
	sx_xunlock(&proctree_lock);
	mtx_lock(&Giant);

	/* Mark special process type. */
	mycp->p_flag |= P_SYSTEM;

	/*
	 * Wakeup parent process.  (Parent sleeps to keep from blasting away
	 * and creating too many daemons.)
	 */
	wakeup(mycp);

	for (;;) {
		/*
		 * curcp is the current daemon process context.
		 * userp is the current user process context.
		 */
		curcp = mycp;

		/*
		 * Take daemon off of free queue
		 */
		if (aiop->aiothreadflags & AIOP_FREE) {
			s = splnet();
			TAILQ_REMOVE(&aio_freeproc, aiop, list);
			TAILQ_INSERT_TAIL(&aio_activeproc, aiop, list);
			aiop->aiothreadflags &= ~AIOP_FREE;
			splx(s);
		}
		aiop->aiothreadflags &= ~AIOP_SCHED;

		/*
		 * Check for jobs.
		 */
		while ((aiocbe = aio_selectjob(aiop)) != NULL) {
			cb = &aiocbe->uaiocb;
			userp = aiocbe->userproc;

			aiocbe->jobstate = JOBST_JOBRUNNING;

			/*
			 * Connect to process address space for user program.
			 */
			if (userp != curcp) {
				/*
				 * Save the current address space that we are
				 * connected to.
				 */
				tmpvm = mycp->p_vmspace;
				
				/*
				 * Point to the new user address space, and
				 * refer to it.
				 */
				mycp->p_vmspace = userp->p_vmspace;
				mycp->p_vmspace->vm_refcnt++;
				
				/* Activate the new mapping. */
				pmap_activate(FIRST_THREAD_IN_PROC(mycp));
				
				/*
				 * If the old address space wasn't the daemons
				 * own address space, then we need to remove the
				 * daemon's reference from the other process
				 * that it was acting on behalf of.
				 */
				if (tmpvm != myvm) {
					vmspace_free(tmpvm);
				}
				curcp = userp;
			}

			ki = userp->p_aioinfo;
			lj = aiocbe->lio;

			/* Account for currently active jobs. */
			ki->kaio_active_count++;

			/* Do the I/O function. */
			aio_process(aiocbe);

			/* Decrement the active job count. */
			ki->kaio_active_count--;

			/*
			 * Increment the completion count for wakeup/signal
			 * comparisons.
			 */
			aiocbe->jobflags |= AIOCBLIST_DONE;
			ki->kaio_queue_finished_count++;
			if (lj)
				lj->lioj_queue_finished_count++;
			if ((ki->kaio_flags & KAIO_WAKEUP) || ((ki->kaio_flags
			    & KAIO_RUNDOWN) && (ki->kaio_active_count == 0))) {
				ki->kaio_flags &= ~KAIO_WAKEUP;
				wakeup(userp);
			}

			s = splbio();
			if (lj && (lj->lioj_flags &
			    (LIOJ_SIGNAL|LIOJ_SIGNAL_POSTED)) == LIOJ_SIGNAL) {
				if ((lj->lioj_queue_finished_count ==
				    lj->lioj_queue_count) &&
				    (lj->lioj_buffer_finished_count ==
				    lj->lioj_buffer_count)) {
					PROC_LOCK(userp);
					psignal(userp,
					    lj->lioj_signal.sigev_signo);
					PROC_UNLOCK(userp);
					lj->lioj_flags |= LIOJ_SIGNAL_POSTED;
				}
			}
			splx(s);

			aiocbe->jobstate = JOBST_JOBFINISHED;

			/*
			 * If the I/O request should be automatically rundown,
			 * do the needed cleanup.  Otherwise, place the queue
			 * entry for the just finished I/O request into the done
			 * queue for the associated client.
			 */
			s = splnet();
			if (aiocbe->jobflags & AIOCBLIST_ASYNCFREE) {
				aiocbe->jobflags &= ~AIOCBLIST_ASYNCFREE;
				uma_zfree(aiocb_zone, aiocbe);
			} else {
				TAILQ_REMOVE(&ki->kaio_jobqueue, aiocbe, plist);
				TAILQ_INSERT_TAIL(&ki->kaio_jobdone, aiocbe,
				    plist);
			}
			splx(s);
			KNOTE(&aiocbe->klist, 0);

			if (aiocbe->jobflags & AIOCBLIST_RUNDOWN) {
				wakeup(aiocbe);
				aiocbe->jobflags &= ~AIOCBLIST_RUNDOWN;
			}

			if (cb->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
				PROC_LOCK(userp);
				psignal(userp, cb->aio_sigevent.sigev_signo);
				PROC_UNLOCK(userp);
			}
		}

		/*
		 * Disconnect from user address space.
		 */
		if (curcp != mycp) {
			/* Get the user address space to disconnect from. */
			tmpvm = mycp->p_vmspace;
			
			/* Get original address space for daemon. */
			mycp->p_vmspace = myvm;
			
			/* Activate the daemon's address space. */
			pmap_activate(FIRST_THREAD_IN_PROC(mycp));
#ifdef DIAGNOSTIC
			if (tmpvm == myvm) {
				printf("AIOD: vmspace problem -- %d\n",
				    mycp->p_pid);
			}
#endif
			/* Remove our vmspace reference. */
			vmspace_free(tmpvm);
			
			curcp = mycp;
		}

		/*
		 * If we are the first to be put onto the free queue, wakeup
		 * anyone waiting for a daemon.
		 */
		s = splnet();
		TAILQ_REMOVE(&aio_activeproc, aiop, list);
		if (TAILQ_EMPTY(&aio_freeproc))
			wakeup(&aio_freeproc);
		TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);
		aiop->aiothreadflags |= AIOP_FREE;
		splx(s);

		/*
		 * If daemon is inactive for a long time, allow it to exit,
		 * thereby freeing resources.
		 */
		if ((aiop->aiothreadflags & AIOP_SCHED) == 0 &&
		    tsleep(aiop->aiothread, PRIBIO, "aiordy", aiod_lifetime)) {
			s = splnet();
			if (TAILQ_EMPTY(&aio_jobs)) {
				if ((aiop->aiothreadflags & AIOP_FREE) &&
				    (num_aio_procs > target_aio_procs)) {
					TAILQ_REMOVE(&aio_freeproc, aiop, list);
					splx(s);
					uma_zfree(aiop_zone, aiop);
					num_aio_procs--;
#ifdef DIAGNOSTIC
					if (mycp->p_vmspace->vm_refcnt <= 1) {
						printf("AIOD: bad vm refcnt for"
						    " exiting daemon: %d\n",
						    mycp->p_vmspace->vm_refcnt);
					}
#endif
					kthread_exit(0);
				}
			}
			splx(s);
		}
	}
}

/*
 * Create a new AIO daemon.  This is mostly a kernel-thread fork routine.  The
 * AIO daemon modifies its environment itself.
 */
static int
aio_newproc()
{
	int error;
	struct proc *p;

	error = kthread_create(aio_daemon, curproc, &p, RFNOWAIT, "aiod%d",
			       num_aio_procs);
	if (error)
		return error;

	/*
	 * Wait until daemon is started, but continue on just in case to
	 * handle error conditions.
	 */
	error = tsleep(p, PZERO, "aiosta", aiod_timeout);

	num_aio_procs++;

	return error;
}

/*
 * Try the high-performance, low-overhead physio method for eligible
 * VCHR devices.  This method doesn't use an aio helper thread, and
 * thus has very low overhead. 
 *
 * Assumes that the caller, _aio_aqueue(), has incremented the file
 * structure's reference count, preventing its deallocation for the
 * duration of this call. 
 */
static int
aio_qphysio(struct proc *p, struct aiocblist *aiocbe)
{
	int error;
	struct aiocb *cb;
	struct file *fp;
	struct buf *bp;
	struct vnode *vp;
	struct kaioinfo *ki;
	struct aio_liojob *lj;
	int s;
	int notify;

	cb = &aiocbe->uaiocb;
	fp = aiocbe->fd_file;

	if (fp->f_type != DTYPE_VNODE) 
		return (-1);

	vp = (struct vnode *)fp->f_data;

	/*
	 * If its not a disk, we don't want to return a positive error.
	 * It causes the aio code to not fall through to try the thread
	 * way when you're talking to a regular file.
	 */
	if (!vn_isdisk(vp, &error)) {
		if (error == ENOTBLK)
			return (-1);
		else
			return (error);
	}

 	if (cb->aio_nbytes % vp->v_rdev->si_bsize_phys)
		return (-1);

	if (cb->aio_nbytes >
	    MAXPHYS - (((vm_offset_t) cb->aio_buf) & PAGE_MASK))
		return (-1);

	ki = p->p_aioinfo;
	if (ki->kaio_buffer_count >= ki->kaio_ballowed_count) 
		return (-1);

	ki->kaio_buffer_count++;

	lj = aiocbe->lio;
	if (lj)
		lj->lioj_buffer_count++;

	/* Create and build a buffer header for a transfer. */
	bp = (struct buf *)getpbuf(NULL);
	BUF_KERNPROC(bp);

	/*
	 * Get a copy of the kva from the physical buffer.
	 */
	bp->b_caller1 = p;
	bp->b_dev = vp->v_rdev;
	error = bp->b_error = 0;

	bp->b_bcount = cb->aio_nbytes;
	bp->b_bufsize = cb->aio_nbytes;
	bp->b_flags = B_PHYS;
	bp->b_iodone = aio_physwakeup;
	bp->b_saveaddr = bp->b_data;
	bp->b_data = (void *)(uintptr_t)cb->aio_buf;
	bp->b_blkno = btodb(cb->aio_offset);

	if (cb->aio_lio_opcode == LIO_WRITE) {
		bp->b_iocmd = BIO_WRITE;
		if (!useracc(bp->b_data, bp->b_bufsize, VM_PROT_READ)) {
			error = EFAULT;
			goto doerror;
		}
	} else {
		bp->b_iocmd = BIO_READ;
		if (!useracc(bp->b_data, bp->b_bufsize, VM_PROT_WRITE)) {
			error = EFAULT;
			goto doerror;
		}
	}

	/* Bring buffer into kernel space. */
	vmapbuf(bp);

	s = splbio();
	aiocbe->bp = bp;
	bp->b_spc = (void *)aiocbe;
	TAILQ_INSERT_TAIL(&aio_bufjobs, aiocbe, list);
	TAILQ_INSERT_TAIL(&ki->kaio_bufqueue, aiocbe, plist);
	aiocbe->jobstate = JOBST_JOBQBUF;
	cb->_aiocb_private.status = cb->aio_nbytes;
	num_buf_aio++;
	bp->b_error = 0;

	splx(s);
	
	/* Perform transfer. */
	DEV_STRATEGY(bp, 0);

	notify = 0;
	s = splbio();
	
	/*
	 * If we had an error invoking the request, or an error in processing
	 * the request before we have returned, we process it as an error in
	 * transfer.  Note that such an I/O error is not indicated immediately,
	 * but is returned using the aio_error mechanism.  In this case,
	 * aio_suspend will return immediately.
	 */
	if (bp->b_error || (bp->b_ioflags & BIO_ERROR)) {
		struct aiocb *job = aiocbe->uuaiocb;

		aiocbe->uaiocb._aiocb_private.status = 0;
		suword(&job->_aiocb_private.status, 0);
		aiocbe->uaiocb._aiocb_private.error = bp->b_error;
		suword(&job->_aiocb_private.error, bp->b_error);

		ki->kaio_buffer_finished_count++;

		if (aiocbe->jobstate != JOBST_JOBBFINISHED) {
			aiocbe->jobstate = JOBST_JOBBFINISHED;
			aiocbe->jobflags |= AIOCBLIST_DONE;
			TAILQ_REMOVE(&aio_bufjobs, aiocbe, list);
			TAILQ_REMOVE(&ki->kaio_bufqueue, aiocbe, plist);
			TAILQ_INSERT_TAIL(&ki->kaio_bufdone, aiocbe, plist);
			notify = 1;
		}
	}
	splx(s);
	if (notify)
		KNOTE(&aiocbe->klist, 0);
	return 0;

doerror:
	ki->kaio_buffer_count--;
	if (lj)
		lj->lioj_buffer_count--;
	aiocbe->bp = NULL;
	relpbuf(bp, NULL);
	return error;
}

/*
 * This waits/tests physio completion.
 */
static int
aio_fphysio(struct aiocblist *iocb)
{
	int s;
	struct buf *bp;
	int error;

	bp = iocb->bp;

	s = splbio();
	while ((bp->b_flags & B_DONE) == 0) {
		if (tsleep(bp, PRIBIO, "physstr", aiod_timeout)) {
			if ((bp->b_flags & B_DONE) == 0) {
				splx(s);
				return EINPROGRESS;
			} else
				break;
		}
	}
	splx(s);

	/* Release mapping into kernel space. */
	vunmapbuf(bp);
	iocb->bp = 0;

	error = 0;
	
	/* Check for an error. */
	if (bp->b_ioflags & BIO_ERROR)
		error = bp->b_error;

	relpbuf(bp, NULL);
	return (error);
}

/*
 * Wake up aio requests that may be serviceable now.
 */
static void
aio_swake_cb(struct socket *so, struct sockbuf *sb)
{
	struct aiocblist *cb,*cbn;
	struct proc *p;
	struct kaioinfo *ki = NULL;
	int opcode, wakecount = 0;
	struct aiothreadlist *aiop;

	if (sb == &so->so_snd) {
		opcode = LIO_WRITE;
		so->so_snd.sb_flags &= ~SB_AIO;
	} else {
		opcode = LIO_READ;
		so->so_rcv.sb_flags &= ~SB_AIO;
	}

	for (cb = TAILQ_FIRST(&so->so_aiojobq); cb; cb = cbn) {
		cbn = TAILQ_NEXT(cb, list);
		if (opcode == cb->uaiocb.aio_lio_opcode) {
			p = cb->userproc;
			ki = p->p_aioinfo;
			TAILQ_REMOVE(&so->so_aiojobq, cb, list);
			TAILQ_REMOVE(&ki->kaio_sockqueue, cb, plist);
			TAILQ_INSERT_TAIL(&aio_jobs, cb, list);
			TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, cb, plist);
			wakecount++;
			if (cb->jobstate != JOBST_JOBQGLOBAL)
				panic("invalid queue value");
		}
	}

	while (wakecount--) {
		if ((aiop = TAILQ_FIRST(&aio_freeproc)) != 0) {
			TAILQ_REMOVE(&aio_freeproc, aiop, list);
			TAILQ_INSERT_TAIL(&aio_activeproc, aiop, list);
			aiop->aiothreadflags &= ~AIOP_FREE;
			wakeup(aiop->aiothread);
		}
	}
}

/*
 * Queue a new AIO request.  Choosing either the threaded or direct physio VCHR
 * technique is done in this code.
 */
static int
_aio_aqueue(struct thread *td, struct aiocb *job, struct aio_liojob *lj, int type)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp;
	struct file *fp;
	unsigned int fd;
	struct socket *so;
	int s;
	int error;
	int opcode, user_opcode;
	struct aiocblist *aiocbe;
	struct aiothreadlist *aiop;
	struct kaioinfo *ki;
	struct kevent kev;
	struct kqueue *kq;
	struct file *kq_fp;

	aiocbe = uma_zalloc(aiocb_zone, M_WAITOK);
	aiocbe->inputcharge = 0;
	aiocbe->outputcharge = 0;
	callout_handle_init(&aiocbe->timeouthandle);
	SLIST_INIT(&aiocbe->klist);

	suword(&job->_aiocb_private.status, -1);
	suword(&job->_aiocb_private.error, 0);
	suword(&job->_aiocb_private.kernelinfo, -1);

	error = copyin(job, &aiocbe->uaiocb, sizeof(aiocbe->uaiocb));
	if (error) {
		suword(&job->_aiocb_private.error, error);
		uma_zfree(aiocb_zone, aiocbe);
		return error;
	}
	if (aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_SIGNAL &&
		!_SIG_VALID(aiocbe->uaiocb.aio_sigevent.sigev_signo)) {
		uma_zfree(aiocb_zone, aiocbe);
		return EINVAL;
	}

	/* Save userspace address of the job info. */
	aiocbe->uuaiocb = job;

	/* Get the opcode. */
	user_opcode = aiocbe->uaiocb.aio_lio_opcode;
	if (type != LIO_NOP)
		aiocbe->uaiocb.aio_lio_opcode = type;
	opcode = aiocbe->uaiocb.aio_lio_opcode;

	/* Get the fd info for process. */
	fdp = p->p_fd;

	/*
	 * Range check file descriptor.
	 */
	fd = aiocbe->uaiocb.aio_fildes;
	if (fd >= fdp->fd_nfiles) {
		uma_zfree(aiocb_zone, aiocbe);
		if (type == 0)
			suword(&job->_aiocb_private.error, EBADF);
		return EBADF;
	}

	fp = aiocbe->fd_file = fdp->fd_ofiles[fd];
	if ((fp == NULL) || ((opcode == LIO_WRITE) && ((fp->f_flag & FWRITE) ==
	    0))) {
		uma_zfree(aiocb_zone, aiocbe);
		if (type == 0)
			suword(&job->_aiocb_private.error, EBADF);
		return EBADF;
	}
	fhold(fp);

	if (aiocbe->uaiocb.aio_offset == -1LL) {
		error = EINVAL;
		goto aqueue_fail;
	}
	error = suword(&job->_aiocb_private.kernelinfo, jobrefid);
	if (error) {
		error = EINVAL;
		goto aqueue_fail;
	}
	aiocbe->uaiocb._aiocb_private.kernelinfo = (void *)(intptr_t)jobrefid;
	if (jobrefid == LONG_MAX)
		jobrefid = 1;
	else
		jobrefid++;
	
	if (opcode == LIO_NOP) {
		fdrop(fp, td);
		uma_zfree(aiocb_zone, aiocbe);
		if (type == 0) {
			suword(&job->_aiocb_private.error, 0);
			suword(&job->_aiocb_private.status, 0);
			suword(&job->_aiocb_private.kernelinfo, 0);
		}
		return 0;
	}
	if ((opcode != LIO_READ) && (opcode != LIO_WRITE)) {
		if (type == 0)
			suword(&job->_aiocb_private.status, 0);
		error = EINVAL;
		goto aqueue_fail;
	}

	if (aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_KEVENT) {
		kev.ident = aiocbe->uaiocb.aio_sigevent.sigev_notify_kqueue;
		kev.udata = aiocbe->uaiocb.aio_sigevent.sigev_value.sigval_ptr;
	}
	else {
		/*
		 * This method for requesting kevent-based notification won't
		 * work on the alpha, since we're passing in a pointer
		 * via aio_lio_opcode, which is an int.  Use the SIGEV_KEVENT-
		 * based method instead.
		 */
		if (user_opcode == LIO_NOP || user_opcode == LIO_READ ||
		    user_opcode == LIO_WRITE)
			goto no_kqueue;

		error = copyin((struct kevent *)(uintptr_t)user_opcode,
		    &kev, sizeof(kev));
		if (error)
			goto aqueue_fail;
	}
	if ((u_int)kev.ident >= fdp->fd_nfiles ||
	    (kq_fp = fdp->fd_ofiles[kev.ident]) == NULL ||
	    (kq_fp->f_type != DTYPE_KQUEUE)) {
		error = EBADF;
		goto aqueue_fail;
	}
	kq = (struct kqueue *)kq_fp->f_data;
	kev.ident = (uintptr_t)aiocbe;
	kev.filter = EVFILT_AIO;
	kev.flags = EV_ADD | EV_ENABLE | EV_FLAG1;
	error = kqueue_register(kq, &kev, td);
aqueue_fail:
	if (error) {
		fdrop(fp, td);
		uma_zfree(aiocb_zone, aiocbe);
		if (type == 0)
			suword(&job->_aiocb_private.error, error);
		goto done;
	}
no_kqueue:

	suword(&job->_aiocb_private.error, EINPROGRESS);
	aiocbe->uaiocb._aiocb_private.error = EINPROGRESS;
	aiocbe->userproc = p;
	aiocbe->jobflags = 0;
	aiocbe->lio = lj;
	ki = p->p_aioinfo;

	if (fp->f_type == DTYPE_SOCKET) {
		/*
		 * Alternate queueing for socket ops: Reach down into the
		 * descriptor to get the socket data.  Then check to see if the
		 * socket is ready to be read or written (based on the requested
		 * operation).
		 *
		 * If it is not ready for io, then queue the aiocbe on the
		 * socket, and set the flags so we get a call when sbnotify()
		 * happens.
		 */
		so = (struct socket *)fp->f_data;
		s = splnet();
		if (((opcode == LIO_READ) && (!soreadable(so))) || ((opcode ==
		    LIO_WRITE) && (!sowriteable(so)))) {
			TAILQ_INSERT_TAIL(&so->so_aiojobq, aiocbe, list);
			TAILQ_INSERT_TAIL(&ki->kaio_sockqueue, aiocbe, plist);
			if (opcode == LIO_READ)
				so->so_rcv.sb_flags |= SB_AIO;
			else
				so->so_snd.sb_flags |= SB_AIO;
			aiocbe->jobstate = JOBST_JOBQGLOBAL; /* XXX */
			ki->kaio_queue_count++;
			num_queue_count++;
			splx(s);
			error = 0;
			goto done;
		}
		splx(s);
	}

	if ((error = aio_qphysio(p, aiocbe)) == 0)
		goto done;
	if (error > 0) {
		suword(&job->_aiocb_private.status, 0);
		aiocbe->uaiocb._aiocb_private.error = error;
		suword(&job->_aiocb_private.error, error);
		goto done;
	}

	/* No buffer for daemon I/O. */
	aiocbe->bp = NULL;

	ki->kaio_queue_count++;
	if (lj)
		lj->lioj_queue_count++;
	s = splnet();
	TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, aiocbe, plist);
	TAILQ_INSERT_TAIL(&aio_jobs, aiocbe, list);
	splx(s);
	aiocbe->jobstate = JOBST_JOBQGLOBAL;

	num_queue_count++;
	error = 0;

	/*
	 * If we don't have a free AIO process, and we are below our quota, then
	 * start one.  Otherwise, depend on the subsequent I/O completions to
	 * pick-up this job.  If we don't sucessfully create the new process
	 * (thread) due to resource issues, we return an error for now (EAGAIN),
	 * which is likely not the correct thing to do.
	 */
	s = splnet();
retryproc:
	if ((aiop = TAILQ_FIRST(&aio_freeproc)) != NULL) {
		TAILQ_REMOVE(&aio_freeproc, aiop, list);
		TAILQ_INSERT_TAIL(&aio_activeproc, aiop, list);
		aiop->aiothreadflags &= ~AIOP_FREE;
		wakeup(aiop->aiothread);
	} else if (((num_aio_resv_start + num_aio_procs) < max_aio_procs) &&
	    ((ki->kaio_active_count + num_aio_resv_start) <
	    ki->kaio_maxactive_count)) {
		num_aio_resv_start++;
		if ((error = aio_newproc()) == 0) {
			num_aio_resv_start--;
			goto retryproc;
		}
		num_aio_resv_start--;
	}
	splx(s);
done:
	return error;
}

/*
 * This routine queues an AIO request, checking for quotas.
 */
static int
aio_aqueue(struct thread *td, struct aiocb *job, int type)
{
	struct proc *p = td->td_proc;
	struct kaioinfo *ki;

	if (p->p_aioinfo == NULL)
		aio_init_aioinfo(p);

	if (num_queue_count >= max_queue_count)
		return EAGAIN;

	ki = p->p_aioinfo;
	if (ki->kaio_queue_count >= ki->kaio_qallowed_count)
		return EAGAIN;

	return _aio_aqueue(td, job, NULL, type);
}

/*
 * Support the aio_return system call, as a side-effect, kernel resources are
 * released.
 */
int
aio_return(struct thread *td, struct aio_return_args *uap)
{
	struct proc *p = td->td_proc;
	int s;
	long jobref;
	struct aiocblist *cb, *ncb;
	struct aiocb *ujob;
	struct kaioinfo *ki;

	ujob = uap->aiocbp;
	jobref = fuword(&ujob->_aiocb_private.kernelinfo);
	if (jobref == -1 || jobref == 0)
		return EINVAL;

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EINVAL;
	TAILQ_FOREACH(cb, &ki->kaio_jobdone, plist) {
		if (((intptr_t) cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			if (cb->uaiocb.aio_lio_opcode == LIO_WRITE) {
				p->p_stats->p_ru.ru_oublock +=
				    cb->outputcharge;
				cb->outputcharge = 0;
			} else if (cb->uaiocb.aio_lio_opcode == LIO_READ) {
				p->p_stats->p_ru.ru_inblock += cb->inputcharge;
				cb->inputcharge = 0;
			}
			goto done;
		}
	}
	s = splbio();
	for (cb = TAILQ_FIRST(&ki->kaio_bufdone); cb; cb = ncb) {
		ncb = TAILQ_NEXT(cb, plist);
		if (((intptr_t) cb->uaiocb._aiocb_private.kernelinfo)
		    == jobref) {
			break;
		}
	}
	splx(s);
 done:
	if (cb != NULL) {
		if (ujob == cb->uuaiocb) {
			td->td_retval[0] =
			    cb->uaiocb._aiocb_private.status;
		} else
			td->td_retval[0] = EFAULT;
		aio_free_entry(cb);
		return (0);
	}
	return (EINVAL);
}

/*
 * Allow a process to wakeup when any of the I/O requests are completed.
 */
int
aio_suspend(struct thread *td, struct aio_suspend_args *uap)
{
	struct proc *p = td->td_proc;
	struct timeval atv;
	struct timespec ts;
	struct aiocb *const *cbptr, *cbp;
	struct kaioinfo *ki;
	struct aiocblist *cb;
	int i;
	int njoblist;
	int error, s, timo;
	long *ijoblist;
	struct aiocb **ujoblist;
	
	if (uap->nent > AIO_LISTIO_MAX)
		return EINVAL;

	timo = 0;
	if (uap->timeout) {
		/* Get timespec struct. */
		if ((error = copyin(uap->timeout, &ts, sizeof(ts))) != 0)
			return error;

		if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
			return (EINVAL);

		TIMESPEC_TO_TIMEVAL(&atv, &ts);
		if (itimerfix(&atv))
			return (EINVAL);
		timo = tvtohz(&atv);
	}

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EAGAIN;

	njoblist = 0;
	ijoblist = uma_zalloc(aiol_zone, M_WAITOK);
	ujoblist = uma_zalloc(aiol_zone, M_WAITOK);
	cbptr = uap->aiocbp;

	for (i = 0; i < uap->nent; i++) {
		cbp = (struct aiocb *)(intptr_t)fuword(&cbptr[i]);
		if (cbp == 0)
			continue;
		ujoblist[njoblist] = cbp;
		ijoblist[njoblist] = fuword(&cbp->_aiocb_private.kernelinfo);
		njoblist++;
	}

	if (njoblist == 0) {
		uma_zfree(aiol_zone, ijoblist);
		uma_zfree(aiol_zone, ujoblist);
		return 0;
	}

	error = 0;
	for (;;) {
		TAILQ_FOREACH(cb, &ki->kaio_jobdone, plist) {
			for (i = 0; i < njoblist; i++) {
				if (((intptr_t)
				    cb->uaiocb._aiocb_private.kernelinfo) ==
				    ijoblist[i]) {
					if (ujoblist[i] != cb->uuaiocb)
						error = EINVAL;
					uma_zfree(aiol_zone, ijoblist);
					uma_zfree(aiol_zone, ujoblist);
					return error;
				}
			}
		}

		s = splbio();
		for (cb = TAILQ_FIRST(&ki->kaio_bufdone); cb; cb =
		    TAILQ_NEXT(cb, plist)) {
			for (i = 0; i < njoblist; i++) {
				if (((intptr_t)
				    cb->uaiocb._aiocb_private.kernelinfo) ==
				    ijoblist[i]) {
					splx(s);
					if (ujoblist[i] != cb->uuaiocb)
						error = EINVAL;
					uma_zfree(aiol_zone, ijoblist);
					uma_zfree(aiol_zone, ujoblist);
					return error;
				}
			}
		}

		ki->kaio_flags |= KAIO_WAKEUP;
		error = tsleep(p, PRIBIO | PCATCH, "aiospn", timo);
		splx(s);

		if (error == ERESTART || error == EINTR) {
			uma_zfree(aiol_zone, ijoblist);
			uma_zfree(aiol_zone, ujoblist);
			return EINTR;
		} else if (error == EWOULDBLOCK) {
			uma_zfree(aiol_zone, ijoblist);
			uma_zfree(aiol_zone, ujoblist);
			return EAGAIN;
		}
	}

/* NOTREACHED */
	return EINVAL;
}

/*
 * aio_cancel cancels any non-physio aio operations not currently in
 * progress.
 */
int
aio_cancel(struct thread *td, struct aio_cancel_args *uap)
{
	struct proc *p = td->td_proc;
	struct kaioinfo *ki;
	struct aiocblist *cbe, *cbn;
	struct file *fp;
	struct filedesc *fdp;
	struct socket *so;
	struct proc *po;
	int s,error;
	int cancelled=0;
	int notcancelled=0;
	struct vnode *vp;

	fdp = p->p_fd;
	if ((u_int)uap->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[uap->fd]) == NULL)
		return (EBADF);

        if (fp->f_type == DTYPE_VNODE) {
		vp = (struct vnode *)fp->f_data;
		
		if (vn_isdisk(vp,&error)) {
			td->td_retval[0] = AIO_NOTCANCELED;
        	        return 0;
		}
	} else if (fp->f_type == DTYPE_SOCKET) {
		so = (struct socket *)fp->f_data;

		s = splnet();

		for (cbe = TAILQ_FIRST(&so->so_aiojobq); cbe; cbe = cbn) {
			cbn = TAILQ_NEXT(cbe, list);
			if ((uap->aiocbp == NULL) ||
				(uap->aiocbp == cbe->uuaiocb) ) {
				po = cbe->userproc;
				ki = po->p_aioinfo;
				TAILQ_REMOVE(&so->so_aiojobq, cbe, list);
				TAILQ_REMOVE(&ki->kaio_sockqueue, cbe, plist);
				TAILQ_INSERT_TAIL(&ki->kaio_jobdone, cbe, plist);
				if (ki->kaio_flags & KAIO_WAKEUP) {
					wakeup(po);
				}
				cbe->jobstate = JOBST_JOBFINISHED;
				cbe->uaiocb._aiocb_private.status=-1;
				cbe->uaiocb._aiocb_private.error=ECANCELED;
				cancelled++;
/* XXX cancelled, knote? */
			        if (cbe->uaiocb.aio_sigevent.sigev_notify ==
				    SIGEV_SIGNAL) {
					PROC_LOCK(cbe->userproc);
					psignal(cbe->userproc, cbe->uaiocb.aio_sigevent.sigev_signo);
					PROC_UNLOCK(cbe->userproc);
				}
				if (uap->aiocbp) 
					break;
			}
		}
		splx(s);

		if ((cancelled) && (uap->aiocbp)) {
			td->td_retval[0] = AIO_CANCELED;
			return 0;
		}
	}
	ki=p->p_aioinfo;
	s = splnet();

	for (cbe = TAILQ_FIRST(&ki->kaio_jobqueue); cbe; cbe = cbn) {
		cbn = TAILQ_NEXT(cbe, plist);

		if ((uap->fd == cbe->uaiocb.aio_fildes) &&
		    ((uap->aiocbp == NULL ) || 
		     (uap->aiocbp == cbe->uuaiocb))) {
			
			if (cbe->jobstate == JOBST_JOBQGLOBAL) {
				TAILQ_REMOVE(&aio_jobs, cbe, list);
                                TAILQ_REMOVE(&ki->kaio_jobqueue, cbe, plist);
                                TAILQ_INSERT_TAIL(&ki->kaio_jobdone, cbe,
                                    plist);
				cancelled++;
				ki->kaio_queue_finished_count++;
				cbe->jobstate = JOBST_JOBFINISHED;
				cbe->uaiocb._aiocb_private.status = -1;
				cbe->uaiocb._aiocb_private.error = ECANCELED;
/* XXX cancelled, knote? */
			        if (cbe->uaiocb.aio_sigevent.sigev_notify ==
				    SIGEV_SIGNAL) {
					PROC_LOCK(cbe->userproc);
					psignal(cbe->userproc, cbe->uaiocb.aio_sigevent.sigev_signo);
					PROC_UNLOCK(cbe->userproc);
				}
			} else {
				notcancelled++;
			}
		}
	}
	splx(s);

	if (notcancelled) {
		td->td_retval[0] = AIO_NOTCANCELED;
		return 0;
	}
	if (cancelled) {
		td->td_retval[0] = AIO_CANCELED;
		return 0;
	}
	td->td_retval[0] = AIO_ALLDONE;

	return 0;
}

/*
 * aio_error is implemented in the kernel level for compatibility purposes only.
 * For a user mode async implementation, it would be best to do it in a userland
 * subroutine.
 */
int
aio_error(struct thread *td, struct aio_error_args *uap)
{
	struct proc *p = td->td_proc;
	int s;
	struct aiocblist *cb;
	struct kaioinfo *ki;
	long jobref;

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EINVAL;

	jobref = fuword(&uap->aiocbp->_aiocb_private.kernelinfo);
	if ((jobref == -1) || (jobref == 0))
		return EINVAL;

	TAILQ_FOREACH(cb, &ki->kaio_jobdone, plist) {
		if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			td->td_retval[0] = cb->uaiocb._aiocb_private.error;
			return 0;
		}
	}

	s = splnet();

	for (cb = TAILQ_FIRST(&ki->kaio_jobqueue); cb; cb = TAILQ_NEXT(cb,
	    plist)) {
		if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			td->td_retval[0] = EINPROGRESS;
			splx(s);
			return 0;
		}
	}

	for (cb = TAILQ_FIRST(&ki->kaio_sockqueue); cb; cb = TAILQ_NEXT(cb,
	    plist)) {
		if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			td->td_retval[0] = EINPROGRESS;
			splx(s);
			return 0;
		}
	}
	splx(s);

	s = splbio();
	for (cb = TAILQ_FIRST(&ki->kaio_bufdone); cb; cb = TAILQ_NEXT(cb,
	    plist)) {
		if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			td->td_retval[0] = cb->uaiocb._aiocb_private.error;
			splx(s);
			return 0;
		}
	}

	for (cb = TAILQ_FIRST(&ki->kaio_bufqueue); cb; cb = TAILQ_NEXT(cb,
	    plist)) {
		if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo) ==
		    jobref) {
			td->td_retval[0] = EINPROGRESS;
			splx(s);
			return 0;
		}
	}
	splx(s);

#if (0)
	/*
	 * Hack for lio.
	 */
	status = fuword(&uap->aiocbp->_aiocb_private.status);
	if (status == -1)
		return fuword(&uap->aiocbp->_aiocb_private.error);
#endif
	return EINVAL;
}

/* syscall - asynchronous read from a file (REALTIME) */
int
aio_read(struct thread *td, struct aio_read_args *uap)
{

	return aio_aqueue(td, uap->aiocbp, LIO_READ);
}

/* syscall - asynchronous write to a file (REALTIME) */
int
aio_write(struct thread *td, struct aio_write_args *uap)
{

	return aio_aqueue(td, uap->aiocbp, LIO_WRITE);
}

/* syscall - XXX undocumented */
int
lio_listio(struct thread *td, struct lio_listio_args *uap)
{
	struct proc *p = td->td_proc;
	int nent, nentqueued;
	struct aiocb *iocb, * const *cbptr;
	struct aiocblist *cb;
	struct kaioinfo *ki;
	struct aio_liojob *lj;
	int error, runningcode;
	int nerror;
	int i;
	int s;

	if ((uap->mode != LIO_NOWAIT) && (uap->mode != LIO_WAIT))
		return EINVAL;

	nent = uap->nent;
	if (nent > AIO_LISTIO_MAX)
		return EINVAL;

	if (p->p_aioinfo == NULL)
		aio_init_aioinfo(p);

	if ((nent + num_queue_count) > max_queue_count)
		return EAGAIN;

	ki = p->p_aioinfo;
	if ((nent + ki->kaio_queue_count) > ki->kaio_qallowed_count)
		return EAGAIN;

	lj = uma_zalloc(aiolio_zone, M_WAITOK);
	if (!lj)
		return EAGAIN;

	lj->lioj_flags = 0;
	lj->lioj_buffer_count = 0;
	lj->lioj_buffer_finished_count = 0;
	lj->lioj_queue_count = 0;
	lj->lioj_queue_finished_count = 0;
	lj->lioj_ki = ki;

	/*
	 * Setup signal.
	 */
	if (uap->sig && (uap->mode == LIO_NOWAIT)) {
		error = copyin(uap->sig, &lj->lioj_signal,
			       sizeof(lj->lioj_signal));
		if (error) {
			uma_zfree(aiolio_zone, lj);
			return error;
		}
		if (!_SIG_VALID(lj->lioj_signal.sigev_signo)) {
			uma_zfree(aiolio_zone, lj);
			return EINVAL;
		}
		lj->lioj_flags |= LIOJ_SIGNAL;
		lj->lioj_flags &= ~LIOJ_SIGNAL_POSTED;
	} else
		lj->lioj_flags &= ~LIOJ_SIGNAL;

	TAILQ_INSERT_TAIL(&ki->kaio_liojoblist, lj, lioj_list);
	/*
	 * Get pointers to the list of I/O requests.
	 */
	nerror = 0;
	nentqueued = 0;
	cbptr = uap->acb_list;
	for (i = 0; i < uap->nent; i++) {
		iocb = (struct aiocb *)(intptr_t)fuword(&cbptr[i]);
		if (((intptr_t)iocb != -1) && ((intptr_t)iocb != NULL)) {
			error = _aio_aqueue(td, iocb, lj, 0);
			if (error == 0)
				nentqueued++;
			else
				nerror++;
		}
	}

	/*
	 * If we haven't queued any, then just return error.
	 */
	if (nentqueued == 0)
		return 0;

	/*
	 * Calculate the appropriate error return.
	 */
	runningcode = 0;
	if (nerror)
		runningcode = EIO;

	if (uap->mode == LIO_WAIT) {
		int command, found, jobref;
		
		for (;;) {
			found = 0;
			for (i = 0; i < uap->nent; i++) {
				/*
				 * Fetch address of the control buf pointer in
				 * user space.
				 */
				iocb = (struct aiocb *)
				    (intptr_t)fuword(&cbptr[i]);
				if (((intptr_t)iocb == -1) || ((intptr_t)iocb
				    == 0))
					continue;

				/*
				 * Fetch the associated command from user space.
				 */
				command = fuword(&iocb->aio_lio_opcode);
				if (command == LIO_NOP) {
					found++;
					continue;
				}

				jobref = fuword(&iocb->_aiocb_private.kernelinfo);

				TAILQ_FOREACH(cb, &ki->kaio_jobdone, plist) {
					if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo)
					    == jobref) {
						if (cb->uaiocb.aio_lio_opcode
						    == LIO_WRITE) {
							p->p_stats->p_ru.ru_oublock
							    +=
							    cb->outputcharge;
							cb->outputcharge = 0;
						} else if (cb->uaiocb.aio_lio_opcode
						    == LIO_READ) {
							p->p_stats->p_ru.ru_inblock
							    += cb->inputcharge;
							cb->inputcharge = 0;
						}
						found++;
						break;
					}
				}

				s = splbio();
				TAILQ_FOREACH(cb, &ki->kaio_bufdone, plist) {
					if (((intptr_t)cb->uaiocb._aiocb_private.kernelinfo)
					    == jobref) {
						found++;
						break;
					}
				}
				splx(s);
			}

			/*
			 * If all I/Os have been disposed of, then we can
			 * return.
			 */
			if (found == nentqueued)
				return runningcode;
			
			ki->kaio_flags |= KAIO_WAKEUP;
			error = tsleep(p, PRIBIO | PCATCH, "aiospn", 0);

			if (error == EINTR)
				return EINTR;
			else if (error == EWOULDBLOCK)
				return EAGAIN;
		}
	}

	return runningcode;
}

/*
 * This is a weird hack so that we can post a signal.  It is safe to do so from
 * a timeout routine, but *not* from an interrupt routine.
 */
static void
process_signal(void *aioj)
{
	struct aiocblist *aiocbe = aioj;
	struct aio_liojob *lj = aiocbe->lio;
	struct aiocb *cb = &aiocbe->uaiocb;

	if ((lj) && (lj->lioj_signal.sigev_notify == SIGEV_SIGNAL) &&
		(lj->lioj_queue_count == lj->lioj_queue_finished_count)) {
		PROC_LOCK(lj->lioj_ki->kaio_p);
		psignal(lj->lioj_ki->kaio_p, lj->lioj_signal.sigev_signo);
		PROC_UNLOCK(lj->lioj_ki->kaio_p);
		lj->lioj_flags |= LIOJ_SIGNAL_POSTED;
	}

	if (cb->aio_sigevent.sigev_notify == SIGEV_SIGNAL) {
		PROC_LOCK(aiocbe->userproc);
		psignal(aiocbe->userproc, cb->aio_sigevent.sigev_signo);
		PROC_UNLOCK(aiocbe->userproc);
	}
}

/*
 * Interrupt handler for physio, performs the necessary process wakeups, and
 * signals.
 */
static void
aio_physwakeup(struct buf *bp)
{
	struct aiocblist *aiocbe;
	struct proc *p;
	struct kaioinfo *ki;
	struct aio_liojob *lj;

	wakeup(bp);

	aiocbe = (struct aiocblist *)bp->b_spc;
	if (aiocbe) {
		p = bp->b_caller1;

		aiocbe->jobstate = JOBST_JOBBFINISHED;
		aiocbe->uaiocb._aiocb_private.status -= bp->b_resid;
		aiocbe->uaiocb._aiocb_private.error = 0;
		aiocbe->jobflags |= AIOCBLIST_DONE;

		if (bp->b_ioflags & BIO_ERROR)
			aiocbe->uaiocb._aiocb_private.error = bp->b_error;

		lj = aiocbe->lio;
		if (lj) {
			lj->lioj_buffer_finished_count++;
			
			/*
			 * wakeup/signal if all of the interrupt jobs are done.
			 */
			if (lj->lioj_buffer_finished_count ==
			    lj->lioj_buffer_count) {
				/*
				 * Post a signal if it is called for.
				 */
				if ((lj->lioj_flags &
				    (LIOJ_SIGNAL|LIOJ_SIGNAL_POSTED)) ==
				    LIOJ_SIGNAL) {
					lj->lioj_flags |= LIOJ_SIGNAL_POSTED;
					aiocbe->timeouthandle =
						timeout(process_signal,
							aiocbe, 0);
				}
			}
		}

		ki = p->p_aioinfo;
		if (ki) {
			ki->kaio_buffer_finished_count++;
			TAILQ_REMOVE(&aio_bufjobs, aiocbe, list);
			TAILQ_REMOVE(&ki->kaio_bufqueue, aiocbe, plist);
			TAILQ_INSERT_TAIL(&ki->kaio_bufdone, aiocbe, plist);

			KNOTE(&aiocbe->klist, 0);
			/* Do the wakeup. */
			if (ki->kaio_flags & (KAIO_RUNDOWN|KAIO_WAKEUP)) {
				ki->kaio_flags &= ~KAIO_WAKEUP;
				wakeup(p);
			}
		}

		if (aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_SIGNAL)
			aiocbe->timeouthandle =
				timeout(process_signal, aiocbe, 0);
	}
}

/* syscall - wait for the next completion of an aio request */
int
aio_waitcomplete(struct thread *td, struct aio_waitcomplete_args *uap)
{
	struct proc *p = td->td_proc;
	struct timeval atv;
	struct timespec ts;
	struct kaioinfo *ki;
	struct aiocblist *cb = NULL;
	int error, s, timo;
	
	suword(uap->aiocbp, (int)NULL);

	timo = 0;
	if (uap->timeout) {
		/* Get timespec struct. */
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return error;

		if ((ts.tv_nsec < 0) || (ts.tv_nsec >= 1000000000))
			return (EINVAL);

		TIMESPEC_TO_TIMEVAL(&atv, &ts);
		if (itimerfix(&atv))
			return (EINVAL);
		timo = tvtohz(&atv);
	}

	ki = p->p_aioinfo;
	if (ki == NULL)
		return EAGAIN;

	for (;;) {
		if ((cb = TAILQ_FIRST(&ki->kaio_jobdone)) != 0) {
			suword(uap->aiocbp, (uintptr_t)cb->uuaiocb);
			td->td_retval[0] = cb->uaiocb._aiocb_private.status;
			if (cb->uaiocb.aio_lio_opcode == LIO_WRITE) {
				p->p_stats->p_ru.ru_oublock +=
				    cb->outputcharge;
				cb->outputcharge = 0;
			} else if (cb->uaiocb.aio_lio_opcode == LIO_READ) {
				p->p_stats->p_ru.ru_inblock += cb->inputcharge;
				cb->inputcharge = 0;
			}
			aio_free_entry(cb);
			return cb->uaiocb._aiocb_private.error;
		}

		s = splbio();
 		if ((cb = TAILQ_FIRST(&ki->kaio_bufdone)) != 0 ) {
			splx(s);
			suword(uap->aiocbp, (uintptr_t)cb->uuaiocb);
			td->td_retval[0] = cb->uaiocb._aiocb_private.status;
			aio_free_entry(cb);
			return cb->uaiocb._aiocb_private.error;
		}

		ki->kaio_flags |= KAIO_WAKEUP;
		error = tsleep(p, PRIBIO | PCATCH, "aiowc", timo);
		splx(s);

		if (error == ERESTART)
			return EINTR;
		else if (error < 0)
			return error;
		else if (error == EINTR)
			return EINTR;
		else if (error == EWOULDBLOCK)
			return EAGAIN;
	}
}

/* kqueue attach function */
static int
filt_aioattach(struct knote *kn)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_id;

	/*
	 * The aiocbe pointer must be validated before using it, so
	 * registration is restricted to the kernel; the user cannot
	 * set EV_FLAG1.
	 */
	if ((kn->kn_flags & EV_FLAG1) == 0)
		return (EPERM);
	kn->kn_flags &= ~EV_FLAG1;

	SLIST_INSERT_HEAD(&aiocbe->klist, kn, kn_selnext);

	return (0);
}

/* kqueue detach function */
static void
filt_aiodetach(struct knote *kn)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_id;

	SLIST_REMOVE(&aiocbe->klist, kn, knote, kn_selnext);
}

/* kqueue filter function */
/*ARGSUSED*/
static int
filt_aio(struct knote *kn, long hint)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_id;

	kn->kn_data = aiocbe->uaiocb._aiocb_private.error;
	if (aiocbe->jobstate != JOBST_JOBFINISHED &&
	    aiocbe->jobstate != JOBST_JOBBFINISHED)
		return (0);
	kn->kn_flags |= EV_EOF; 
	return (1);
}
