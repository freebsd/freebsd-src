/*-
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
 */

/*
 * This file contains support for the POSIX 1003.1B AIO/LIO facility.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/eventhandler.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/kthread.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/protosw.h>
#include <sys/sema.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <machine/atomic.h>

#include <posix4/posix4.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <sys/aio.h>

#include "opt_vfs_aio.h"

/*
 * Counter for allocating reference ids to new jobs.  Wrapped to 1 on
 * overflow.
 */
static	long jobrefid;

#define JOBST_NULL		0x0
#define JOBST_JOBQSOCK		0x1
#define JOBST_JOBQGLOBAL	0x2
#define JOBST_JOBRUNNING	0x3
#define JOBST_JOBFINISHED	0x4
#define	JOBST_JOBQBUF		0x5

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

static SYSCTL_NODE(_vfs, OID_AUTO, aio, CTLFLAG_RW, 0, "Async IO management");

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

typedef struct oaiocb {
	int	aio_fildes;		/* File descriptor */
	off_t	aio_offset;		/* File offset for I/O */
	volatile void *aio_buf;         /* I/O buffer in process space */
	size_t	aio_nbytes;		/* Number of bytes for I/O */
	struct	osigevent aio_sigevent;	/* Signal to deliver */
	int	aio_lio_opcode;		/* LIO opcode */
	int	aio_reqprio;		/* Request priority -- ignored */
	struct	__aiocb_private	_aiocb_private;
} oaiocb_t;

struct aiocblist {
	TAILQ_ENTRY(aiocblist) list;	/* List of jobs */
	TAILQ_ENTRY(aiocblist) plist;	/* List of jobs for proc */
	TAILQ_ENTRY(aiocblist) allist;
	int	jobflags;
	int	jobstate;
	int	inputcharge;
	int	outputcharge;
	struct	buf *bp;		/* Buffer pointer */
	struct	proc *userproc;		/* User process */
	struct  ucred *cred;		/* Active credential when created */
	struct	file *fd_file;		/* Pointer to file structure */
	struct	aioliojob *lio;		/* Optional lio job */
	struct	aiocb *uuaiocb;		/* Pointer in userspace of aiocb */
	struct	knlist klist;		/* list of knotes */
	struct	aiocb uaiocb;		/* Kernel I/O control block */
	ksiginfo_t ksi;			/* Realtime signal info */
	struct task	biotask;
};

/* jobflags */
#define AIOCBLIST_RUNDOWN	0x04
#define AIOCBLIST_DONE		0x10
#define AIOCBLIST_BUFDONE	0x20

/*
 * AIO process info
 */
#define AIOP_FREE	0x1			/* proc on free queue */

struct aiothreadlist {
	int aiothreadflags;			/* AIO proc flags */
	TAILQ_ENTRY(aiothreadlist) list;	/* List of processes */
	struct thread *aiothread;		/* The AIO thread */
};

/*
 * data-structure for lio signal management
 */
struct aioliojob {
	int	lioj_flags;
	int	lioj_count;
	int	lioj_finished_count;
	int	lioj_ref_count;
	struct	sigevent lioj_signal;	/* signal on all I/O done */
	TAILQ_ENTRY(aioliojob) lioj_list;
	struct  knlist klist;		/* list of knotes */
	ksiginfo_t lioj_ksi;	/* Realtime signal info */
};

#define	LIOJ_SIGNAL		0x1	/* signal on all done (lio) */
#define	LIOJ_SIGNAL_POSTED	0x2	/* signal has been posted */
#define LIOJ_KEVENT_POSTED	0x4	/* kevent triggered */

/*
 * per process aio data structure
 */
struct kaioinfo {
	int	kaio_flags;		/* per process kaio flags */
	int	kaio_maxactive_count;	/* maximum number of AIOs */
	int	kaio_active_count;	/* number of currently used AIOs */
	int	kaio_qallowed_count;	/* maxiumu size of AIO queue */
	int	kaio_count;		/* size of AIO queue */
	int	kaio_ballowed_count;	/* maximum number of buffers */
	int	kaio_buffer_count;	/* number of physio buffers */
	TAILQ_HEAD(,aiocblist) kaio_all;	/* all AIOs in the process */
	TAILQ_HEAD(,aiocblist) kaio_done;	/* done queue for process */
	TAILQ_HEAD(,aioliojob) kaio_liojoblist; /* list of lio jobs */
	TAILQ_HEAD(,aiocblist) kaio_jobqueue;	/* job queue for process */
	TAILQ_HEAD(,aiocblist) kaio_bufqueue;	/* buffer job queue for process */
	TAILQ_HEAD(,aiocblist) kaio_sockqueue;	/* queue for aios waiting on sockets */
};

#define KAIO_RUNDOWN	0x1	/* process is being run down */
#define KAIO_WAKEUP	0x2	/* wakeup process when there is a significant event */

static TAILQ_HEAD(,aiothreadlist) aio_freeproc;		/* Idle daemons */
static struct sema aio_newproc_sem;
static struct mtx aio_job_mtx;
static struct mtx aio_sock_mtx;
static TAILQ_HEAD(,aiocblist) aio_jobs;			/* Async job list */
static struct unrhdr *aiod_unr;

static void	aio_init_aioinfo(struct proc *p);
static void	aio_onceonly(void);
static int	aio_free_entry(struct aiocblist *aiocbe);
static void	aio_process(struct aiocblist *aiocbe);
static int	aio_newproc(int *);
static int	aio_aqueue(struct thread *td, struct aiocb *job, int type,
			int osigev);
static void	aio_physwakeup(struct buf *bp);
static void	aio_proc_rundown(void *arg, struct proc *p);
static int	aio_qphysio(struct proc *p, struct aiocblist *iocb);
static void	biohelper(void *, int);
static void	aio_daemon(void *param);
static void	aio_swake_cb(struct socket *, struct sockbuf *);
static int	aio_unload(void);
static int	filt_aioattach(struct knote *kn);
static void	filt_aiodetach(struct knote *kn);
static int	filt_aio(struct knote *kn, long hint);
static int	filt_lioattach(struct knote *kn);
static void	filt_liodetach(struct knote *kn);
static int	filt_lio(struct knote *kn, long hint);
#define DONE_BUF 1
#define DONE_QUEUE 2
static void	aio_bio_done_notify( struct proc *userp, struct aiocblist *aiocbe, int type);
static int	do_lio_listio(struct thread *td, struct lio_listio_args *uap,
			int oldsigev);

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
static struct filterops lio_filtops =
	{ 0, filt_lioattach, filt_liodetach, filt_lio };

static eventhandler_tag exit_tag, exec_tag;

TASKQUEUE_DEFINE_THREAD(aiod_bio);

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
SYSCALL_MODULE_HELPER(oaio_read);
SYSCALL_MODULE_HELPER(oaio_write);
SYSCALL_MODULE_HELPER(olio_listio);

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
	exit_tag = EVENTHANDLER_REGISTER(process_exit, aio_proc_rundown, NULL,
	    EVENTHANDLER_PRI_ANY);
	exec_tag = EVENTHANDLER_REGISTER(process_exec, aio_proc_rundown, NULL,
	    EVENTHANDLER_PRI_ANY);
	kqueue_add_filteropts(EVFILT_AIO, &aio_filtops);
	kqueue_add_filteropts(EVFILT_LIO, &lio_filtops);
	TAILQ_INIT(&aio_freeproc);
	sema_init(&aio_newproc_sem, 0, "aio_new_proc");
	mtx_init(&aio_job_mtx, "aio_job", NULL, MTX_DEF);
	mtx_init(&aio_sock_mtx, "aio_sock", NULL, MTX_DEF);
	TAILQ_INIT(&aio_jobs);
	aiod_unr = new_unrhdr(1, INT_MAX, NULL);
	kaio_zone = uma_zcreate("AIO", sizeof(struct kaioinfo), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiop_zone = uma_zcreate("AIOP", sizeof(struct aiothreadlist), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiocb_zone = uma_zcreate("AIOCB", sizeof(struct aiocblist), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiol_zone = uma_zcreate("AIOL", AIO_LISTIO_MAX*sizeof(intptr_t) , NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiolio_zone = uma_zcreate("AIOLIO", sizeof(struct aioliojob), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	aiod_timeout = AIOD_TIMEOUT_DEFAULT;
	aiod_lifetime = AIOD_LIFETIME_DEFAULT;
	jobrefid = 1;
	async_io_version = _POSIX_VERSION;
	p31b_setcfg(CTL_P1003_1B_AIO_LISTIO_MAX, AIO_LISTIO_MAX);
	p31b_setcfg(CTL_P1003_1B_AIO_MAX, MAX_AIO_QUEUE);
	p31b_setcfg(CTL_P1003_1B_AIO_PRIO_DELTA_MAX, 0);
}

/*
 * Callback for unload of AIO when used as a module.
 */
static int
aio_unload(void)
{
	int error;

	/*
	 * XXX: no unloads by default, it's too dangerous.
	 * perhaps we could do it if locked out callers and then
	 * did an aio_proc_rundown() on each process.
	 *
	 * jhb: aio_proc_rundown() needs to run on curproc though,
	 * so I don't think that would fly.
	 */
	if (!unloadable)
		return (EOPNOTSUPP);

	error = kqueue_del_filteropts(EVFILT_AIO);
	if (error)
		return error;
	async_io_version = 0;
	aio_swake = NULL;
	taskqueue_free(taskqueue_aiod_bio);
	delete_unrhdr(aiod_unr);
	EVENTHANDLER_DEREGISTER(process_exit, exit_tag);
	EVENTHANDLER_DEREGISTER(process_exec, exec_tag);
	mtx_destroy(&aio_job_mtx);
	mtx_destroy(&aio_sock_mtx);
	sema_destroy(&aio_newproc_sem);
	p31b_setcfg(CTL_P1003_1B_AIO_LISTIO_MAX, -1);
	p31b_setcfg(CTL_P1003_1B_AIO_MAX, -1);
	p31b_setcfg(CTL_P1003_1B_AIO_PRIO_DELTA_MAX, -1);
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

	ki = uma_zalloc(kaio_zone, M_WAITOK);
	ki->kaio_flags = 0;
	ki->kaio_maxactive_count = max_aio_per_proc;
	ki->kaio_active_count = 0;
	ki->kaio_qallowed_count = max_aio_queue_per_proc;
	ki->kaio_count = 0;
	ki->kaio_ballowed_count = max_buf_aio;
	ki->kaio_buffer_count = 0;
	TAILQ_INIT(&ki->kaio_all);
	TAILQ_INIT(&ki->kaio_done);
	TAILQ_INIT(&ki->kaio_jobqueue);
	TAILQ_INIT(&ki->kaio_bufqueue);
	TAILQ_INIT(&ki->kaio_liojoblist);
	TAILQ_INIT(&ki->kaio_sockqueue);
	PROC_LOCK(p);
	if (p->p_aioinfo == NULL) {
		p->p_aioinfo = ki;
		PROC_UNLOCK(p);
	} else {
		PROC_UNLOCK(p);
		uma_zfree(kaio_zone, ki);
	}

	while (num_aio_procs < target_aio_procs)
		aio_newproc(NULL);
}

static int
aio_sendsig(struct proc *p, struct sigevent *sigev, ksiginfo_t *ksi)
{
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (!KSI_ONQ(ksi)) {
		ksi->ksi_code = SI_ASYNCIO;
		ksi->ksi_flags |= KSI_EXT | KSI_INS;
		return (psignal_event(p, sigev, ksi));
	}
	return (0);
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
	struct aioliojob *lj;
	struct proc *p;

	p = aiocbe->userproc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS(curproc == p);
	MPASS(aiocbe->jobstate == JOBST_JOBFINISHED);

	ki = p->p_aioinfo;
	MPASS(ki != NULL);

	atomic_subtract_int(&num_queue_count, 1);

	ki->kaio_count--;
	MPASS(ki->kaio_count >= 0);

	lj = aiocbe->lio;
	if (lj) {
		lj->lioj_count--;
		lj->lioj_finished_count--;

		if (lj->lioj_count == 0 && lj->lioj_ref_count == 0) {
			TAILQ_REMOVE(&ki->kaio_liojoblist, lj, lioj_list);
			/* lio is going away, we need to destroy any knotes */
			knlist_delete(&lj->klist, curthread, 1);
			sigqueue_take(&lj->lioj_ksi);
			uma_zfree(aiolio_zone, lj);
		}
	}

	TAILQ_REMOVE(&ki->kaio_done, aiocbe, plist);
	TAILQ_REMOVE(&ki->kaio_all, aiocbe, allist);

	/* aiocbe is going away, we need to destroy any knotes */
	knlist_delete(&aiocbe->klist, curthread, 1);
	sigqueue_take(&aiocbe->ksi);

 	MPASS(aiocbe->bp == NULL);
	aiocbe->jobstate = JOBST_NULL;

	/* Wake up anyone who has interest to do cleanup work. */
	if (ki->kaio_flags & (KAIO_WAKEUP | KAIO_RUNDOWN)) {
		ki->kaio_flags &= ~KAIO_WAKEUP;
		wakeup(&p->p_aioinfo);
	}
	PROC_UNLOCK(p);

	/*
	 * The thread argument here is used to find the owning process
	 * and is also passed to fo_close() which may pass it to various
	 * places such as devsw close() routines.  Because of that, we
	 * need a thread pointer from the process owning the job that is
	 * persistent and won't disappear out from under us or move to
	 * another process.
	 *
	 * Currently, all the callers of this function call it to remove
	 * an aiocblist from the current process' job list either via a
	 * syscall or due to the current process calling exit() or
	 * execve().  Thus, we know that p == curproc.  We also know that
	 * curthread can't exit since we are curthread.
	 *
	 * Therefore, we use curthread as the thread to pass to
	 * knlist_delete().  This does mean that it is possible for the
	 * thread pointer at close time to differ from the thread pointer
	 * at open time, but this is already true of file descriptors in
	 * a multithreaded process.
	 */
	fdrop(aiocbe->fd_file, curthread);
	crfree(aiocbe->cred);
	uma_zfree(aiocb_zone, aiocbe);
	PROC_LOCK(p);

	return (0);
}

/*
 * Rundown the jobs for a given process.
 */
static void
aio_proc_rundown(void *arg, struct proc *p)
{
	struct kaioinfo *ki;
	struct aioliojob *lj;
	struct aiocblist *cbe, *cbn;
	struct file *fp;
	struct socket *so;

	KASSERT(curthread->td_proc == p,
	    ("%s: called on non-curproc", __func__));
	ki = p->p_aioinfo;
	if (ki == NULL)
		return;

	PROC_LOCK(p);

restart:
	ki->kaio_flags |= KAIO_RUNDOWN;

	/*
	 * Try to cancel all pending requests. This code simulates
	 * aio_cancel on all pending I/O requests.
	 */
	while ((cbe = TAILQ_FIRST(&ki->kaio_sockqueue))) {
		fp = cbe->fd_file;
		so = fp->f_data;
		mtx_lock(&aio_sock_mtx);
		TAILQ_REMOVE(&so->so_aiojobq, cbe, list);
		mtx_unlock(&aio_sock_mtx);
		TAILQ_REMOVE(&ki->kaio_sockqueue, cbe, plist);
		TAILQ_INSERT_HEAD(&ki->kaio_jobqueue, cbe, plist);
		cbe->jobstate = JOBST_JOBQGLOBAL;
	}

	TAILQ_FOREACH_SAFE(cbe, &ki->kaio_jobqueue, plist, cbn) {
		mtx_lock(&aio_job_mtx);
		if (cbe->jobstate == JOBST_JOBQGLOBAL) {
			TAILQ_REMOVE(&aio_jobs, cbe, list);
			mtx_unlock(&aio_job_mtx);
			cbe->jobstate = JOBST_JOBFINISHED;
			cbe->uaiocb._aiocb_private.status = -1;
			cbe->uaiocb._aiocb_private.error = ECANCELED;
			TAILQ_REMOVE(&ki->kaio_jobqueue, cbe, plist);
			aio_bio_done_notify(p, cbe, DONE_QUEUE);
		} else {
			mtx_unlock(&aio_job_mtx);
		}
	}

	if (TAILQ_FIRST(&ki->kaio_sockqueue))
		goto restart;

	/* Wait for all running I/O to be finished */
	if (TAILQ_FIRST(&ki->kaio_bufqueue) ||
	    TAILQ_FIRST(&ki->kaio_jobqueue)) {
		ki->kaio_flags |= KAIO_WAKEUP;
		msleep(&p->p_aioinfo, &p->p_mtx, PRIBIO, "aioprn", hz);
		goto restart;
	}

	/* Free all completed I/O requests. */
	while ((cbe = TAILQ_FIRST(&ki->kaio_done)) != NULL)
		aio_free_entry(cbe);

	while ((lj = TAILQ_FIRST(&ki->kaio_liojoblist)) != NULL) {
		if (lj->lioj_count == 0 && lj->lioj_ref_count == 0) {
			TAILQ_REMOVE(&ki->kaio_liojoblist, lj, lioj_list);
			knlist_delete(&lj->klist, curthread, 1);
			sigqueue_take(&lj->lioj_ksi);
			uma_zfree(aiolio_zone, lj);
		} else {
			panic("LIO job not cleaned up: C:%d, FC:%d, RC:%d\n",
			    lj->lioj_count, lj->lioj_finished_count,
			    lj->lioj_ref_count);
		}
	}

	uma_zfree(kaio_zone, ki);
	p->p_aioinfo = NULL;
	PROC_UNLOCK(p);
}

/*
 * Select a job to run (called by an AIO daemon).
 */
static struct aiocblist *
aio_selectjob(struct aiothreadlist *aiop)
{
	struct aiocblist *aiocbe;
	struct kaioinfo *ki;
	struct proc *userp;

	mtx_assert(&aio_job_mtx, MA_OWNED);
	TAILQ_FOREACH(aiocbe, &aio_jobs, list) {
		userp = aiocbe->userproc;
		ki = userp->p_aioinfo;

		if (ki->kaio_active_count < ki->kaio_maxactive_count) {
			TAILQ_REMOVE(&aio_jobs, aiocbe, list);
			/* Account for currently active jobs. */
			ki->kaio_active_count++;
			aiocbe->jobstate = JOBST_JOBRUNNING;
			break;
		}
	}
	return (aiocbe);
}

/*
 * The AIO processing activity.  This is the code that does the I/O request for
 * the non-physio version of the operations.  The normal vn operations are used,
 * and this code should work in all instances for every type of file, including
 * pipes, sockets, fifos, and regular files.
 *
 * XXX I don't think these code work well with pipes, sockets and fifo, the
 * problem is the aiod threads can be blocked if there is not data or no
 * buffer space, and file was not opened with O_NONBLOCK, all aiod threads
 * will be blocked if there is couple of such processes. We need a FOF_OFFSET
 * like flag to override f_flag to tell low level system to do non-blocking
 * I/O, we can not muck O_NONBLOCK because there is full of race between
 * userland and aiod threads, although there is a trigger mechanism for socket,
 * but it also does not work well if userland is misbehaviored.
 */
static void
aio_process(struct aiocblist *aiocbe)
{
	struct ucred *td_savedcred;
	struct thread *td;
	struct proc *mycp;
	struct aiocb *cb;
	struct file *fp;
	struct socket *so;
	struct uio auio;
	struct iovec aiov;
	int cnt;
	int error;
	int oublock_st, oublock_end;
	int inblock_st, inblock_end;

	td = curthread;
	td_savedcred = td->td_ucred;
	td->td_ucred = aiocbe->cred;
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
			int sigpipe = 1;
			if (fp->f_type == DTYPE_SOCKET) {
				so = fp->f_data;
				if (so->so_options & SO_NOSIGPIPE)
					sigpipe = 0;
			}
			if (sigpipe) {
				PROC_LOCK(aiocbe->userproc);
				psignal(aiocbe->userproc, SIGPIPE);
				PROC_UNLOCK(aiocbe->userproc);
			}
		}
	}

	cnt -= auio.uio_resid;
	cb->_aiocb_private.error = error;
	cb->_aiocb_private.status = cnt;
	td->td_ucred = td_savedcred;
}

static void
aio_bio_done_notify(struct proc *userp, struct aiocblist *aiocbe, int type)
{
	struct aioliojob *lj;
	struct kaioinfo *ki;
	int lj_done;

	PROC_LOCK_ASSERT(userp, MA_OWNED);
	ki = userp->p_aioinfo;
	lj = aiocbe->lio;
	lj_done = 0;
	if (lj) {
		lj->lioj_finished_count++;
		if (lj->lioj_count == lj->lioj_finished_count)
			lj_done = 1;
	}
	if (type == DONE_QUEUE) {
		aiocbe->jobflags |= AIOCBLIST_DONE;
	} else {
		aiocbe->jobflags |= AIOCBLIST_BUFDONE;
		ki->kaio_buffer_count--;
	}
	TAILQ_INSERT_TAIL(&ki->kaio_done, aiocbe, plist);
	aiocbe->jobstate = JOBST_JOBFINISHED;
	if (aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_SIGNAL ||
	    aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_THREAD_ID)
		aio_sendsig(userp, &aiocbe->uaiocb.aio_sigevent, &aiocbe->ksi);

	KNOTE_LOCKED(&aiocbe->klist, 1);

	if (lj_done) {
		if (lj->lioj_signal.sigev_notify == SIGEV_KEVENT) {
			lj->lioj_flags |= LIOJ_KEVENT_POSTED;
			KNOTE_LOCKED(&lj->klist, 1);
		}
		if ((lj->lioj_flags & (LIOJ_SIGNAL|LIOJ_SIGNAL_POSTED))
		    == LIOJ_SIGNAL
		    && (lj->lioj_signal.sigev_notify == SIGEV_SIGNAL ||
		        lj->lioj_signal.sigev_notify == SIGEV_THREAD_ID)) {
			aio_sendsig(userp, &lj->lioj_signal, &lj->lioj_ksi);
			lj->lioj_flags |= LIOJ_SIGNAL_POSTED;
		}
	}
	if (ki->kaio_flags & (KAIO_RUNDOWN|KAIO_WAKEUP)) {
		ki->kaio_flags &= ~KAIO_WAKEUP;
		wakeup(&userp->p_aioinfo);
	}
}

/*
 * The AIO daemon, most of the actual work is done in aio_process,
 * but the setup (and address space mgmt) is done in this routine.
 */
static void
aio_daemon(void *_id)
{
	struct aiocblist *aiocbe;
	struct aiothreadlist *aiop;
	struct kaioinfo *ki;
	struct proc *curcp, *mycp, *userp;
	struct vmspace *myvm, *tmpvm;
	struct thread *td = curthread;
	struct pgrp *newpgrp;
	struct session *newsess;
	int id = (intptr_t)_id;

	/*
	 * Local copies of curproc (cp) and vmspace (myvm)
	 */
	mycp = td->td_proc;
	myvm = mycp->p_vmspace;

	KASSERT(mycp->p_textvp == NULL, ("kthread has a textvp"));

	/*
	 * Allocate and ready the aio control info.  There is one aiop structure
	 * per daemon.
	 */
	aiop = uma_zalloc(aiop_zone, M_WAITOK);
	aiop->aiothread = td;
	aiop->aiothreadflags |= AIOP_FREE;

	/*
	 * Place thread (lightweight process) onto the AIO free thread list.
	 */
	mtx_lock(&aio_job_mtx);
	TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);
	mtx_unlock(&aio_job_mtx);

	/*
	 * Get rid of our current filedescriptors.  AIOD's don't need any
	 * filedescriptors, except as temporarily inherited from the client.
	 */
	fdfree(td);

	/* The daemon resides in its own pgrp. */
	MALLOC(newpgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		M_WAITOK | M_ZERO);
	MALLOC(newsess, struct session *, sizeof(struct session), M_SESSION,
		M_WAITOK | M_ZERO);

	sx_xlock(&proctree_lock);
	enterpgrp(mycp, mycp->p_pid, newpgrp, newsess);
	sx_xunlock(&proctree_lock);

	/*
	 * Wakeup parent process.  (Parent sleeps to keep from blasting away
	 * and creating too many daemons.)
	 */
	sema_post(&aio_newproc_sem);

	mtx_lock(&aio_job_mtx);
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
			TAILQ_REMOVE(&aio_freeproc, aiop, list);
			aiop->aiothreadflags &= ~AIOP_FREE;
		}

		/*
		 * Check for jobs.
		 */
		while ((aiocbe = aio_selectjob(aiop)) != NULL) {
			mtx_unlock(&aio_job_mtx);
			userp = aiocbe->userproc;

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
				atomic_add_int(&mycp->p_vmspace->vm_refcnt, 1);

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

			/* Do the I/O function. */
			aio_process(aiocbe);

			mtx_lock(&aio_job_mtx);
			/* Decrement the active job count. */
			ki->kaio_active_count--;
			mtx_unlock(&aio_job_mtx);

			PROC_LOCK(userp);
			TAILQ_REMOVE(&ki->kaio_jobqueue, aiocbe, plist);
			aio_bio_done_notify(userp, aiocbe, DONE_QUEUE);
			if (aiocbe->jobflags & AIOCBLIST_RUNDOWN) {
				wakeup(aiocbe);
				aiocbe->jobflags &= ~AIOCBLIST_RUNDOWN;
			}
			PROC_UNLOCK(userp);

			mtx_lock(&aio_job_mtx);
		}

		/*
		 * Disconnect from user address space.
		 */
		if (curcp != mycp) {

			mtx_unlock(&aio_job_mtx);

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

			mtx_lock(&aio_job_mtx);
			/*
			 * We have to restart to avoid race, we only sleep if
			 * no job can be selected, that should be
			 * curcp == mycp.
			 */
			continue;
		}

		mtx_assert(&aio_job_mtx, MA_OWNED);

		TAILQ_INSERT_HEAD(&aio_freeproc, aiop, list);
		aiop->aiothreadflags |= AIOP_FREE;

		/*
		 * If daemon is inactive for a long time, allow it to exit,
		 * thereby freeing resources.
		 */
		if (msleep(aiop->aiothread, &aio_job_mtx, PRIBIO, "aiordy",
		    aiod_lifetime)) {
			if (TAILQ_EMPTY(&aio_jobs)) {
				if ((aiop->aiothreadflags & AIOP_FREE) &&
				    (num_aio_procs > target_aio_procs)) {
					TAILQ_REMOVE(&aio_freeproc, aiop, list);
					num_aio_procs--;
					mtx_unlock(&aio_job_mtx);
					uma_zfree(aiop_zone, aiop);
					free_unr(aiod_unr, id);
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
		}
	}
	mtx_unlock(&aio_job_mtx);
	panic("shouldn't be here\n");
}

/*
 * Create a new AIO daemon. This is mostly a kernel-thread fork routine. The
 * AIO daemon modifies its environment itself.
 */
static int
aio_newproc(int *start)
{
	int error;
	struct proc *p;
	int id;

	id = alloc_unr(aiod_unr);
	error = kthread_create(aio_daemon, (void *)(intptr_t)id, &p,
		RFNOWAIT, 0, "aiod%d", id);
	if (error == 0) {
		/*
		 * Wait until daemon is started.
		 */
		sema_wait(&aio_newproc_sem);
		mtx_lock(&aio_job_mtx);
		num_aio_procs++;
		if (start != NULL)
			*start--;
		mtx_unlock(&aio_job_mtx);
	} else {
		free_unr(aiod_unr, id);
	}
	return (error);
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
	struct aiocb *cb;
	struct file *fp;
	struct buf *bp;
	struct vnode *vp;
	struct kaioinfo *ki;
	struct aioliojob *lj;
	int error;

	cb = &aiocbe->uaiocb;
	fp = aiocbe->fd_file;

	if (fp->f_type != DTYPE_VNODE)
		return (-1);

	vp = fp->f_vnode;

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

 	if (cb->aio_nbytes % vp->v_bufobj.bo_bsize)
		return (-1);

	if (cb->aio_nbytes > vp->v_rdev->si_iosize_max)
		return (-1);

	if (cb->aio_nbytes >
	    MAXPHYS - (((vm_offset_t) cb->aio_buf) & PAGE_MASK))
		return (-1);

	ki = p->p_aioinfo;
	if (ki->kaio_buffer_count >= ki->kaio_ballowed_count)
		return (-1);

	/* Create and build a buffer header for a transfer. */
	bp = (struct buf *)getpbuf(NULL);
	BUF_KERNPROC(bp);

	PROC_LOCK(p);
	ki->kaio_count++;
	ki->kaio_buffer_count++;
	lj = aiocbe->lio;
	if (lj)
		lj->lioj_count++;
	PROC_UNLOCK(p);

	/*
	 * Get a copy of the kva from the physical buffer.
	 */
	error = 0;

	bp->b_bcount = cb->aio_nbytes;
	bp->b_bufsize = cb->aio_nbytes;
	bp->b_iodone = aio_physwakeup;
	bp->b_saveaddr = bp->b_data;
	bp->b_data = (void *)(uintptr_t)cb->aio_buf;
	bp->b_offset = cb->aio_offset;
	bp->b_iooffset = cb->aio_offset;
	bp->b_blkno = btodb(cb->aio_offset);
	bp->b_iocmd = cb->aio_lio_opcode == LIO_WRITE ? BIO_WRITE : BIO_READ;

	/*
	 * Bring buffer into kernel space.
	 */
	if (vmapbuf(bp) < 0) {
		error = EFAULT;
		goto doerror;
	}

	PROC_LOCK(p);
	aiocbe->bp = bp;
	bp->b_caller1 = (void *)aiocbe;
	TAILQ_INSERT_TAIL(&ki->kaio_bufqueue, aiocbe, plist);
	TAILQ_INSERT_TAIL(&ki->kaio_all, aiocbe, allist);
	aiocbe->jobstate = JOBST_JOBQBUF;
	cb->_aiocb_private.status = cb->aio_nbytes;
	PROC_UNLOCK(p);

	atomic_add_int(&num_queue_count, 1);
	atomic_add_int(&num_buf_aio, 1);

	bp->b_error = 0;

	TASK_INIT(&aiocbe->biotask, 0, biohelper, aiocbe);

	/* Perform transfer. */
	dev_strategy(vp->v_rdev, bp);
	return (0);

doerror:
	PROC_LOCK(p);
	ki->kaio_count--;
	ki->kaio_buffer_count--;
	if (lj)
		lj->lioj_count--;
	aiocbe->bp = NULL;
	PROC_UNLOCK(p);
	relpbuf(bp, NULL);
	return (error);
}

/*
 * Wake up aio requests that may be serviceable now.
 */
static void
aio_swake_cb(struct socket *so, struct sockbuf *sb)
{
	struct aiocblist *cb, *cbn;
	struct proc *p;
	struct kaioinfo *ki = NULL;
	int opcode, wakecount = 0;
	struct aiothreadlist *aiop;

	if (sb == &so->so_snd) {
		opcode = LIO_WRITE;
		SOCKBUF_LOCK(&so->so_snd);
		so->so_snd.sb_flags &= ~SB_AIO;
		SOCKBUF_UNLOCK(&so->so_snd);
	} else {
		opcode = LIO_READ;
		SOCKBUF_LOCK(&so->so_rcv);
		so->so_rcv.sb_flags &= ~SB_AIO;
		SOCKBUF_UNLOCK(&so->so_rcv);
	}

	mtx_lock(&aio_sock_mtx);
	TAILQ_FOREACH_SAFE(cb, &so->so_aiojobq, list, cbn) {
		if (opcode == cb->uaiocb.aio_lio_opcode) {
			if (cb->jobstate != JOBST_JOBQGLOBAL)
				panic("invalid queue value");
			p = cb->userproc;
			ki = p->p_aioinfo;
			TAILQ_REMOVE(&so->so_aiojobq, cb, list);
			PROC_LOCK(p);
			TAILQ_REMOVE(&ki->kaio_sockqueue, cb, plist);
			/*
			 * XXX check AIO_RUNDOWN, and don't put on
			 * jobqueue if it was set.
			 */
			TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, cb, plist);
			cb->jobstate = JOBST_JOBQGLOBAL;
			mtx_lock(&aio_job_mtx);
			TAILQ_INSERT_TAIL(&aio_jobs, cb, list);
			mtx_unlock(&aio_job_mtx);
			PROC_UNLOCK(p);
			wakecount++;
		}
	}
	mtx_unlock(&aio_sock_mtx);

	while (wakecount--) {
		mtx_lock(&aio_job_mtx);
		if ((aiop = TAILQ_FIRST(&aio_freeproc)) != NULL) {
			TAILQ_REMOVE(&aio_freeproc, aiop, list);
			aiop->aiothreadflags &= ~AIOP_FREE;
			wakeup(aiop->aiothread);
		}
		mtx_unlock(&aio_job_mtx);
	}
}

/*
 * Queue a new AIO request.  Choosing either the threaded or direct physio VCHR
 * technique is done in this code.
 */
static int
_aio_aqueue(struct thread *td, struct aiocb *job, struct aioliojob *lj,
	int type, int oldsigev)
{
	struct proc *p = td->td_proc;
	struct file *fp;
	struct socket *so;
	struct aiocblist *aiocbe;
	struct aiothreadlist *aiop;
	struct kaioinfo *ki;
	struct kevent kev;
	struct kqueue *kq;
	struct file *kq_fp;
	struct sockbuf *sb;
	int opcode;
	int error;
	int fd;
	int jid;

	ki = p->p_aioinfo;

	aiocbe = uma_zalloc(aiocb_zone, M_WAITOK | M_ZERO);
	aiocbe->inputcharge = 0;
	aiocbe->outputcharge = 0;
	knlist_init(&aiocbe->klist, &p->p_mtx, NULL, NULL, NULL);

	suword(&job->_aiocb_private.status, -1);
	suword(&job->_aiocb_private.error, 0);
	suword(&job->_aiocb_private.kernelinfo, -1);

	if (oldsigev) {
		bzero(&aiocbe->uaiocb, sizeof(struct aiocb));
		error = copyin(job, &aiocbe->uaiocb, sizeof(struct oaiocb));
		bcopy(&aiocbe->uaiocb.__spare__, &aiocbe->uaiocb.aio_sigevent,
			sizeof(struct osigevent));
	} else {
		error = copyin(job, &aiocbe->uaiocb, sizeof(struct aiocb));
	}
	if (error) {
		suword(&job->_aiocb_private.error, error);
		uma_zfree(aiocb_zone, aiocbe);
		return (error);
	}
	if ((aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_SIGNAL ||
	     aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_THREAD_ID) &&
		!_SIG_VALID(aiocbe->uaiocb.aio_sigevent.sigev_signo)) {
		uma_zfree(aiocb_zone, aiocbe);
		return (EINVAL);
	}

	ksiginfo_init(&aiocbe->ksi);

	/* Save userspace address of the job info. */
	aiocbe->uuaiocb = job;

	/* Get the opcode. */
	if (type != LIO_NOP)
		aiocbe->uaiocb.aio_lio_opcode = type;
	opcode = aiocbe->uaiocb.aio_lio_opcode;

	/* Fetch the file object for the specified file descriptor. */
	fd = aiocbe->uaiocb.aio_fildes;
	switch (opcode) {
	case LIO_WRITE:
		error = fget_write(td, fd, &fp);
		break;
	case LIO_READ:
		error = fget_read(td, fd, &fp);
		break;
	default:
		error = fget(td, fd, &fp);
	}
	if (error) {
		uma_zfree(aiocb_zone, aiocbe);
		suword(&job->_aiocb_private.error, EBADF);
		return (error);
	}
	aiocbe->fd_file = fp;

	if (aiocbe->uaiocb.aio_offset == -1LL) {
		error = EINVAL;
		goto aqueue_fail;
	}

	mtx_lock(&aio_job_mtx);
	jid = jobrefid;
	if (jobrefid == LONG_MAX)
		jobrefid = 1;
	else
		jobrefid++;
	mtx_unlock(&aio_job_mtx);

	error = suword(&job->_aiocb_private.kernelinfo, jid);
	if (error) {
		error = EINVAL;
		goto aqueue_fail;
	}
	aiocbe->uaiocb._aiocb_private.kernelinfo = (void *)(intptr_t)jid;

	if (opcode == LIO_NOP) {
		fdrop(fp, td);
		uma_zfree(aiocb_zone, aiocbe);
		return (0);
	}
	if ((opcode != LIO_READ) && (opcode != LIO_WRITE)) {
		error = EINVAL;
		goto aqueue_fail;
	}

	if (aiocbe->uaiocb.aio_sigevent.sigev_notify == SIGEV_KEVENT) {
		kev.ident = aiocbe->uaiocb.aio_sigevent.sigev_notify_kqueue;
	} else
		goto no_kqueue;
	error = fget(td, (u_int)kev.ident, &kq_fp);
	if (error)
		goto aqueue_fail;
	if (kq_fp->f_type != DTYPE_KQUEUE) {
		fdrop(kq_fp, td);
		error = EBADF;
		goto aqueue_fail;
	}
	kq = kq_fp->f_data;
	kev.ident = (uintptr_t)aiocbe->uuaiocb;
	kev.filter = EVFILT_AIO;
	kev.flags = EV_ADD | EV_ENABLE | EV_FLAG1;
	kev.data = (intptr_t)aiocbe;
	kev.udata = aiocbe->uaiocb.aio_sigevent.sigev_value.sival_ptr;
	error = kqueue_register(kq, &kev, td, 1);
	fdrop(kq_fp, td);
aqueue_fail:
	if (error) {
		fdrop(fp, td);
		uma_zfree(aiocb_zone, aiocbe);
		suword(&job->_aiocb_private.error, error);
		goto done;
	}
no_kqueue:

	suword(&job->_aiocb_private.error, EINPROGRESS);
	aiocbe->uaiocb._aiocb_private.error = EINPROGRESS;
	aiocbe->userproc = p;
	aiocbe->cred = crhold(td->td_ucred);
	aiocbe->jobflags = 0;
	aiocbe->lio = lj;

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
		 *
		 * Note if opcode is neither LIO_WRITE nor LIO_READ we lock
		 * and unlock the snd sockbuf for no reason.
		 */
		so = fp->f_data;
		sb = (opcode == LIO_READ) ? &so->so_rcv : &so->so_snd;
		SOCKBUF_LOCK(sb);
		if (((opcode == LIO_READ) && (!soreadable(so))) || ((opcode ==
		    LIO_WRITE) && (!sowriteable(so)))) {
			mtx_lock(&aio_sock_mtx);
			TAILQ_INSERT_TAIL(&so->so_aiojobq, aiocbe, list);
			mtx_unlock(&aio_sock_mtx);

			sb->sb_flags |= SB_AIO;
			PROC_LOCK(p);
			TAILQ_INSERT_TAIL(&ki->kaio_sockqueue, aiocbe, plist);
			TAILQ_INSERT_TAIL(&ki->kaio_all, aiocbe, allist);
			aiocbe->jobstate = JOBST_JOBQSOCK;
			ki->kaio_count++;
			if (lj)
				lj->lioj_count++;
			PROC_UNLOCK(p);
			SOCKBUF_UNLOCK(sb);
			atomic_add_int(&num_queue_count, 1);
			error = 0;
			goto done;
		}
		SOCKBUF_UNLOCK(sb);
	}

	if ((error = aio_qphysio(p, aiocbe)) == 0)
		goto done;
#if 0
	if (error > 0) {
		aiocbe->uaiocb._aiocb_private.error = error;
		suword(&job->_aiocb_private.error, error);
		goto done;
	}
#endif
	/* No buffer for daemon I/O. */
	aiocbe->bp = NULL;

	PROC_LOCK(p);
	ki->kaio_count++;
	if (lj)
		lj->lioj_count++;
	TAILQ_INSERT_TAIL(&ki->kaio_jobqueue, aiocbe, plist);
	TAILQ_INSERT_TAIL(&ki->kaio_all, aiocbe, allist);

	mtx_lock(&aio_job_mtx);
	TAILQ_INSERT_TAIL(&aio_jobs, aiocbe, list);
	aiocbe->jobstate = JOBST_JOBQGLOBAL;
	PROC_UNLOCK(p);

	atomic_add_int(&num_queue_count, 1);

	/*
	 * If we don't have a free AIO process, and we are below our quota, then
	 * start one.  Otherwise, depend on the subsequent I/O completions to
	 * pick-up this job.  If we don't sucessfully create the new process
	 * (thread) due to resource issues, we return an error for now (EAGAIN),
	 * which is likely not the correct thing to do.
	 */
retryproc:
	error = 0;
	if ((aiop = TAILQ_FIRST(&aio_freeproc)) != NULL) {
		TAILQ_REMOVE(&aio_freeproc, aiop, list);
		aiop->aiothreadflags &= ~AIOP_FREE;
		wakeup(aiop->aiothread);
	} else if (((num_aio_resv_start + num_aio_procs) < max_aio_procs) &&
	    ((ki->kaio_active_count + num_aio_resv_start) <
	    ki->kaio_maxactive_count)) {
		num_aio_resv_start++;
		mtx_unlock(&aio_job_mtx);
		error = aio_newproc(&num_aio_resv_start);
		mtx_lock(&aio_job_mtx);
		if (error) {
			num_aio_resv_start--;
			goto retryproc;
		}
	}
	mtx_unlock(&aio_job_mtx);

done:
	return (error);
}

/*
 * This routine queues an AIO request, checking for quotas.
 */
static int
aio_aqueue(struct thread *td, struct aiocb *job, int type, int oldsigev)
{
	struct proc *p = td->td_proc;
	struct kaioinfo *ki;

	if (p->p_aioinfo == NULL)
		aio_init_aioinfo(p);

	if (num_queue_count >= max_queue_count)
		return (EAGAIN);

	ki = p->p_aioinfo;
	if (ki->kaio_count >= ki->kaio_qallowed_count)
		return (EAGAIN);

	return _aio_aqueue(td, job, NULL, type, oldsigev);
}

/*
 * Support the aio_return system call, as a side-effect, kernel resources are
 * released.
 */
int
aio_return(struct thread *td, struct aio_return_args *uap)
{
	struct proc *p = td->td_proc;
	struct aiocblist *cb;
	struct aiocb *uaiocb;
	struct kaioinfo *ki;
	int status, error;

	ki = p->p_aioinfo;
	if (ki == NULL)
		return (EINVAL);
	uaiocb = uap->aiocbp;
	PROC_LOCK(p);
	TAILQ_FOREACH(cb, &ki->kaio_done, plist) {
		if (cb->uuaiocb == uaiocb)
			break;
	}
	if (cb != NULL) {
		MPASS(cb->jobstate == JOBST_JOBFINISHED);
		status = cb->uaiocb._aiocb_private.status;
		error = cb->uaiocb._aiocb_private.error;
		td->td_retval[0] = status;
		if (cb->uaiocb.aio_lio_opcode == LIO_WRITE) {
			p->p_stats->p_ru.ru_oublock +=
			    cb->outputcharge;
			cb->outputcharge = 0;
		} else if (cb->uaiocb.aio_lio_opcode == LIO_READ) {
			p->p_stats->p_ru.ru_inblock += cb->inputcharge;
			cb->inputcharge = 0;
		}
		aio_free_entry(cb);
		suword(&uaiocb->_aiocb_private.error, error);
		suword(&uaiocb->_aiocb_private.status, status);
		error = 0;
	} else
		error = EINVAL;
	PROC_UNLOCK(p);
	return (error);
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
	struct aiocblist *cb, *cbfirst;
	struct aiocb **ujoblist;
	int njoblist;
	int error;
	int timo;
	int i;

	if (uap->nent < 0 || uap->nent > AIO_LISTIO_MAX)
		return (EINVAL);

	timo = 0;
	if (uap->timeout) {
		/* Get timespec struct. */
		if ((error = copyin(uap->timeout, &ts, sizeof(ts))) != 0)
			return (error);

		if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000)
			return (EINVAL);

		TIMESPEC_TO_TIMEVAL(&atv, &ts);
		if (itimerfix(&atv))
			return (EINVAL);
		timo = tvtohz(&atv);
	}

	ki = p->p_aioinfo;
	if (ki == NULL)
		return (EAGAIN);

	njoblist = 0;
	ujoblist = uma_zalloc(aiol_zone, M_WAITOK);
	cbptr = uap->aiocbp;

	for (i = 0; i < uap->nent; i++) {
		cbp = (struct aiocb *)(intptr_t)fuword(&cbptr[i]);
		if (cbp == 0)
			continue;
		ujoblist[njoblist] = cbp;
		njoblist++;
	}

	if (njoblist == 0) {
		uma_zfree(aiol_zone, ujoblist);
		return (0);
	}

	PROC_LOCK(p);
	for (;;) {
		cbfirst = NULL;
		error = 0;
		TAILQ_FOREACH(cb, &ki->kaio_all, allist) {
			for (i = 0; i < njoblist; i++) {
				if (cb->uuaiocb == ujoblist[i]) {
					if (cbfirst == NULL)
						cbfirst = cb;
					if (cb->jobstate == JOBST_JOBFINISHED)
						goto RETURN;
				}
			}
		}
		/* All tasks were finished. */
		if (cbfirst == NULL)
			break;

		ki->kaio_flags |= KAIO_WAKEUP;
		error = msleep(&p->p_aioinfo, &p->p_mtx, PRIBIO | PCATCH,
		    "aiospn", timo);
		if (error == ERESTART)
			error = EINTR;
		if (error)
			break;
	}
RETURN:
	PROC_UNLOCK(p);
	uma_zfree(aiol_zone, ujoblist);
	return (error);
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
	struct socket *so;
	int error;
	int cancelled = 0;
	int notcancelled = 0;
	struct vnode *vp;

	/* Lookup file object. */
	error = fget(td, uap->fd, &fp);
	if (error)
		return (error);

	ki = p->p_aioinfo;
	if (ki == NULL)
		goto done;

	if (fp->f_type == DTYPE_VNODE) {
		vp = fp->f_vnode;
		if (vn_isdisk(vp, &error)) {
			fdrop(fp, td);
			td->td_retval[0] = AIO_NOTCANCELED;
			return (0);
		}
	} else if (fp->f_type == DTYPE_SOCKET) {
		so = fp->f_data;
		mtx_lock(&aio_sock_mtx);
		TAILQ_FOREACH_SAFE(cbe, &so->so_aiojobq, list, cbn) {
			if (cbe->userproc == p &&
			    (uap->aiocbp == NULL ||
			     uap->aiocbp == cbe->uuaiocb)) {
				TAILQ_REMOVE(&so->so_aiojobq, cbe, list);
				PROC_LOCK(p);
				TAILQ_REMOVE(&ki->kaio_sockqueue, cbe, plist);
				cbe->jobstate = JOBST_JOBRUNNING;
				cbe->uaiocb._aiocb_private.status = -1;
				cbe->uaiocb._aiocb_private.error = ECANCELED;
				aio_bio_done_notify(p, cbe, DONE_QUEUE);
				PROC_UNLOCK(p);
				cancelled++;
				if (uap->aiocbp != NULL)
					break;
			}
		}
		mtx_unlock(&aio_sock_mtx);
		if (cancelled && uap->aiocbp != NULL) {
			fdrop(fp, td);
			td->td_retval[0] = AIO_CANCELED;
			return (0);
		}
	}

	PROC_LOCK(p);
	TAILQ_FOREACH_SAFE(cbe, &ki->kaio_jobqueue, plist, cbn) {
		if ((uap->fd == cbe->uaiocb.aio_fildes) &&
		    ((uap->aiocbp == NULL) ||
		     (uap->aiocbp == cbe->uuaiocb))) {
			mtx_lock(&aio_job_mtx);
			if (cbe->jobstate == JOBST_JOBQGLOBAL) {
				TAILQ_REMOVE(&aio_jobs, cbe, list);
				mtx_unlock(&aio_job_mtx);
				TAILQ_REMOVE(&ki->kaio_jobqueue, cbe, plist);
				cbe->uaiocb._aiocb_private.status = -1;
				cbe->uaiocb._aiocb_private.error = ECANCELED;
				aio_bio_done_notify(p, cbe, DONE_QUEUE);
				cancelled++;
			} else {
				mtx_unlock(&aio_job_mtx);
				notcancelled++;
			}
		}
	}
	PROC_UNLOCK(p);

done:
	fdrop(fp, td);
	if (notcancelled) {
		td->td_retval[0] = AIO_NOTCANCELED;
		return (0);
	}
	if (cancelled) {
		td->td_retval[0] = AIO_CANCELED;
		return (0);
	}
	td->td_retval[0] = AIO_ALLDONE;

	return (0);
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
	struct aiocblist *cb;
	struct kaioinfo *ki;
	int status;

	ki = p->p_aioinfo;
	if (ki == NULL) {
		td->td_retval[0] = EINVAL;
		return (0);
	}

	PROC_LOCK(p);
	TAILQ_FOREACH(cb, &ki->kaio_all, allist) {
		if (cb->uuaiocb == uap->aiocbp) {
			if (cb->jobstate == JOBST_JOBFINISHED)
				td->td_retval[0] =
					cb->uaiocb._aiocb_private.error;
			else
				td->td_retval[0] = EINPROGRESS;
			PROC_UNLOCK(p);
			return (0);
		}
	}
	PROC_UNLOCK(p);

	/*
	 * Hack for failure of _aio_aqueue.
	 */
	status = fuword(&uap->aiocbp->_aiocb_private.status);
	if (status == -1) {
		td->td_retval[0] = fuword(&uap->aiocbp->_aiocb_private.error);
		return (0);
	}

	td->td_retval[0] = EINVAL;
	return (0);
}

/* syscall - asynchronous read from a file (REALTIME) */
int
oaio_read(struct thread *td, struct oaio_read_args *uap)
{

	return aio_aqueue(td, (struct aiocb *)uap->aiocbp, LIO_READ, 1);
}

int
aio_read(struct thread *td, struct aio_read_args *uap)
{

	return aio_aqueue(td, uap->aiocbp, LIO_READ, 0);
}

/* syscall - asynchronous write to a file (REALTIME) */
int
oaio_write(struct thread *td, struct oaio_write_args *uap)
{

	return aio_aqueue(td, (struct aiocb *)uap->aiocbp, LIO_WRITE, 1);
}

int
aio_write(struct thread *td, struct aio_write_args *uap)
{

	return aio_aqueue(td, uap->aiocbp, LIO_WRITE, 0);
}

/* syscall - list directed I/O (REALTIME) */
int
olio_listio(struct thread *td, struct olio_listio_args *uap)
{
	return do_lio_listio(td, (struct lio_listio_args *)uap, 1);
}

/* syscall - list directed I/O (REALTIME) */
int
lio_listio(struct thread *td, struct lio_listio_args *uap)
{
	return do_lio_listio(td, uap, 0);
}

static int
do_lio_listio(struct thread *td, struct lio_listio_args *uap, int oldsigev)
{
	struct proc *p = td->td_proc;
	struct aiocb *iocb, * const *cbptr;
	struct kaioinfo *ki;
	struct aioliojob *lj;
	struct kevent kev;
	struct kqueue * kq;
	struct file *kq_fp;
	int nent;
	int error;
	int nerror;
	int i;

	if ((uap->mode != LIO_NOWAIT) && (uap->mode != LIO_WAIT))
		return (EINVAL);

	nent = uap->nent;
	if (nent < 0 || nent > AIO_LISTIO_MAX)
		return (EINVAL);

	if (p->p_aioinfo == NULL)
		aio_init_aioinfo(p);

	ki = p->p_aioinfo;

	lj = uma_zalloc(aiolio_zone, M_WAITOK);
	lj->lioj_flags = 0;
	lj->lioj_count = 0;
	lj->lioj_finished_count = 0;
	lj->lioj_ref_count = 0;
	knlist_init(&lj->klist, &p->p_mtx, NULL, NULL, NULL);
	ksiginfo_init(&lj->lioj_ksi);

	/*
	 * Setup signal.
	 */
	if (uap->sig && (uap->mode == LIO_NOWAIT)) {
		bzero(&lj->lioj_signal, sizeof(&lj->lioj_signal));
		error = copyin(uap->sig, &lj->lioj_signal,
				oldsigev ? sizeof(struct osigevent) :
					   sizeof(struct sigevent));
		if (error) {
			uma_zfree(aiolio_zone, lj);
			return (error);
		}

		if (lj->lioj_signal.sigev_notify == SIGEV_KEVENT) {
			/* Assume only new style KEVENT */
			error = fget(td, lj->lioj_signal.sigev_notify_kqueue,
				&kq_fp);
			if (error) {
				uma_zfree(aiolio_zone, lj);
				return (error);
			}
			if (kq_fp->f_type != DTYPE_KQUEUE) {
				fdrop(kq_fp, td);
				uma_zfree(aiolio_zone, lj);
				return (EBADF);
			}
			kq = (struct kqueue *)kq_fp->f_data;
			kev.filter = EVFILT_LIO;
			kev.flags = EV_ADD | EV_ENABLE | EV_FLAG1;
			kev.ident = (uintptr_t)lj; /* something unique */
			kev.data = (intptr_t)lj;
			/* pass user defined sigval data */
			kev.udata = lj->lioj_signal.sigev_value.sival_ptr;
			error = kqueue_register(kq, &kev, td, 1);
			fdrop(kq_fp, td);
			if (error) {
				uma_zfree(aiolio_zone, lj);
				return (error);
			}
		} else if (lj->lioj_signal.sigev_notify == SIGEV_NONE) {
			;
		} else if (!_SIG_VALID(lj->lioj_signal.sigev_signo)) {
			uma_zfree(aiolio_zone, lj);
			return EINVAL;
		} else {
			lj->lioj_flags |= LIOJ_SIGNAL;
		}
	}

	PROC_LOCK(p);
	TAILQ_INSERT_TAIL(&ki->kaio_liojoblist, lj, lioj_list);
	/*
	 * Add extra aiocb count to avoid the lio to be freed
	 * by other threads doing aio_waitcomplete or aio_return,
	 * and prevent event from being sent until we have queued
	 * all tasks.
	 */
	lj->lioj_count = 1;
	PROC_UNLOCK(p);

	/*
	 * Get pointers to the list of I/O requests.
	 */
	nerror = 0;
	cbptr = uap->acb_list;
	for (i = 0; i < uap->nent; i++) {
		iocb = (struct aiocb *)(intptr_t)fuword(&cbptr[i]);
		if (((intptr_t)iocb != -1) && ((intptr_t)iocb != 0)) {
			error = _aio_aqueue(td, iocb, lj, 0, oldsigev);
			if (error != 0)
				nerror++;
		}
	}

	error = 0;
	PROC_LOCK(p);
	if (uap->mode == LIO_WAIT) {
		while (lj->lioj_count - 1 != lj->lioj_finished_count) {
			ki->kaio_flags |= KAIO_WAKEUP;
			error = msleep(&p->p_aioinfo, &p->p_mtx,
			    PRIBIO | PCATCH, "aiospn", 0);
			if (error == ERESTART)
				error = EINTR;
			if (error)
				break;
		}
	} else {
		if (lj->lioj_count - 1 == lj->lioj_finished_count) {
			if (lj->lioj_signal.sigev_notify == SIGEV_KEVENT) {
				lj->lioj_flags |= LIOJ_KEVENT_POSTED;
				KNOTE_LOCKED(&lj->klist, 1);
			}
			if ((lj->lioj_flags & (LIOJ_SIGNAL|LIOJ_SIGNAL_POSTED))
			    == LIOJ_SIGNAL
			    && (lj->lioj_signal.sigev_notify == SIGEV_SIGNAL ||
			    lj->lioj_signal.sigev_notify == SIGEV_THREAD_ID)) {
				aio_sendsig(p, &lj->lioj_signal,
					    &lj->lioj_ksi);
				lj->lioj_flags |= LIOJ_SIGNAL_POSTED;
			}
		}
	}
	lj->lioj_count--;
	if (lj->lioj_count == 0) {
		TAILQ_REMOVE(&ki->kaio_liojoblist, lj, lioj_list);
		knlist_delete(&lj->klist, curthread, 1);
		sigqueue_take(&lj->lioj_ksi);
		PROC_UNLOCK(p);
		uma_zfree(aiolio_zone, lj);
	} else
		PROC_UNLOCK(p);

	if (nerror)
		return (EIO);
	return (error);
}

/*
 * Called from interrupt thread for physio, we should return as fast
 * as possible, so we schedule a biohelper task.
 */
static void
aio_physwakeup(struct buf *bp)
{
	struct aiocblist *aiocbe;

	aiocbe = (struct aiocblist *)bp->b_caller1;
	taskqueue_enqueue(taskqueue_aiod_bio, &aiocbe->biotask);
}

/*
 * Task routine to perform heavy tasks, process wakeup, and signals.
 */
static void
biohelper(void *context, int pending)
{
	struct aiocblist *aiocbe = context;
	struct buf *bp;
	struct proc *userp;
	int nblks;

	bp = aiocbe->bp;
	userp = aiocbe->userproc;
	PROC_LOCK(userp);
	aiocbe->uaiocb._aiocb_private.status -= bp->b_resid;
	aiocbe->uaiocb._aiocb_private.error = 0;
	if (bp->b_ioflags & BIO_ERROR)
		aiocbe->uaiocb._aiocb_private.error = bp->b_error;
	nblks = btodb(aiocbe->uaiocb.aio_nbytes);
	if (aiocbe->uaiocb.aio_lio_opcode == LIO_WRITE)
		aiocbe->outputcharge += nblks;
	else
		aiocbe->inputcharge += nblks;
	aiocbe->bp = NULL;
	TAILQ_REMOVE(&userp->p_aioinfo->kaio_bufqueue, aiocbe, plist);
	aio_bio_done_notify(userp, aiocbe, DONE_BUF);
	PROC_UNLOCK(userp);

	/* Release mapping into kernel space. */
	vunmapbuf(bp);
	relpbuf(bp, NULL);
	atomic_subtract_int(&num_buf_aio, 1);
}

/* syscall - wait for the next completion of an aio request */
int
aio_waitcomplete(struct thread *td, struct aio_waitcomplete_args *uap)
{
	struct proc *p = td->td_proc;
	struct timeval atv;
	struct timespec ts;
	struct kaioinfo *ki;
	struct aiocblist *cb;
	struct aiocb *uuaiocb;
	int error, status, timo;

	suword(uap->aiocbp, (long)NULL);

	timo = 0;
	if (uap->timeout) {
		/* Get timespec struct. */
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return (error);

		if ((ts.tv_nsec < 0) || (ts.tv_nsec >= 1000000000))
			return (EINVAL);

		TIMESPEC_TO_TIMEVAL(&atv, &ts);
		if (itimerfix(&atv))
			return (EINVAL);
		timo = tvtohz(&atv);
	}

	if (p->p_aioinfo == NULL)
		aio_init_aioinfo(p);
	ki = p->p_aioinfo;

	error = 0;
	cb = NULL;
	PROC_LOCK(p);
	while ((cb = TAILQ_FIRST(&ki->kaio_done)) == NULL) {
		ki->kaio_flags |= KAIO_WAKEUP;
		error = msleep(&p->p_aioinfo, &p->p_mtx, PRIBIO | PCATCH,
		    "aiowc", timo);
		if (error == ERESTART)
			error = EINTR;
		if (error)
			break;
	}

	if (cb != NULL) {
		MPASS(cb->jobstate == JOBST_JOBFINISHED);
		uuaiocb = cb->uuaiocb;
		status = cb->uaiocb._aiocb_private.status;
		error = cb->uaiocb._aiocb_private.error;
		td->td_retval[0] = status;
		if (cb->uaiocb.aio_lio_opcode == LIO_WRITE) {
			p->p_stats->p_ru.ru_oublock += cb->outputcharge;
			cb->outputcharge = 0;
		} else if (cb->uaiocb.aio_lio_opcode == LIO_READ) {
			p->p_stats->p_ru.ru_inblock += cb->inputcharge;
			cb->inputcharge = 0;
		}
		aio_free_entry(cb);
		PROC_UNLOCK(p);
		suword(uap->aiocbp, (long)uuaiocb);
		suword(&uuaiocb->_aiocb_private.error, error);
		suword(&uuaiocb->_aiocb_private.status, status);
	} else
		PROC_UNLOCK(p);

	return (error);
}

/* kqueue attach function */
static int
filt_aioattach(struct knote *kn)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_sdata;

	/*
	 * The aiocbe pointer must be validated before using it, so
	 * registration is restricted to the kernel; the user cannot
	 * set EV_FLAG1.
	 */
	if ((kn->kn_flags & EV_FLAG1) == 0)
		return (EPERM);
	kn->kn_flags &= ~EV_FLAG1;

	knlist_add(&aiocbe->klist, kn, 0);

	return (0);
}

/* kqueue detach function */
static void
filt_aiodetach(struct knote *kn)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_sdata;

	if (!knlist_empty(&aiocbe->klist))
		knlist_remove(&aiocbe->klist, kn, 0);
}

/* kqueue filter function */
/*ARGSUSED*/
static int
filt_aio(struct knote *kn, long hint)
{
	struct aiocblist *aiocbe = (struct aiocblist *)kn->kn_sdata;

	kn->kn_data = aiocbe->uaiocb._aiocb_private.error;
	if (aiocbe->jobstate != JOBST_JOBFINISHED)
		return (0);
	kn->kn_flags |= EV_EOF;
	return (1);
}

/* kqueue attach function */
static int
filt_lioattach(struct knote *kn)
{
	struct aioliojob * lj = (struct aioliojob *)kn->kn_sdata;

	/*
	 * The aioliojob pointer must be validated before using it, so
	 * registration is restricted to the kernel; the user cannot
	 * set EV_FLAG1.
	 */
	if ((kn->kn_flags & EV_FLAG1) == 0)
		return (EPERM);
	kn->kn_flags &= ~EV_FLAG1;

	knlist_add(&lj->klist, kn, 0);

	return (0);
}

/* kqueue detach function */
static void
filt_liodetach(struct knote *kn)
{
	struct aioliojob * lj = (struct aioliojob *)kn->kn_sdata;

	if (!knlist_empty(&lj->klist))
		knlist_remove(&lj->klist, kn, 0);
}

/* kqueue filter function */
/*ARGSUSED*/
static int
filt_lio(struct knote *kn, long hint)
{
	struct aioliojob * lj = (struct aioliojob *)kn->kn_sdata;

	return (lj->lioj_flags & LIOJ_KEVENT_POSTED);
}
