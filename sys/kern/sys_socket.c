/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1990, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/aio.h>
#include <sys/domain.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/sigio.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/filio.h>			/* XXX */
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/taskqueue.h>
#include <sys/uio.h>
#include <sys/ucred.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/user.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

static SYSCTL_NODE(_kern_ipc, OID_AUTO, aio, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
    "socket AIO stats");

static int empty_results;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, empty_results, CTLFLAG_RD, &empty_results,
    0, "socket operation returned EAGAIN");

static int empty_retries;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, empty_retries, CTLFLAG_RD, &empty_retries,
    0, "socket operation retries");

static fo_rdwr_t soo_read;
static fo_rdwr_t soo_write;
static fo_ioctl_t soo_ioctl;
static fo_poll_t soo_poll;
extern fo_kqfilter_t soo_kqfilter;
static fo_stat_t soo_stat;
static fo_close_t soo_close;
static fo_chmod_t soo_chmod;
static fo_fill_kinfo_t soo_fill_kinfo;
static fo_aio_queue_t soo_aio_queue;

static void	soo_aio_cancel(struct kaiocb *job);

const struct fileops socketops = {
	.fo_read = soo_read,
	.fo_write = soo_write,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = soo_ioctl,
	.fo_poll = soo_poll,
	.fo_kqfilter = soo_kqfilter,
	.fo_stat = soo_stat,
	.fo_close = soo_close,
	.fo_chmod = soo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = soo_fill_kinfo,
	.fo_aio_queue = soo_aio_queue,
	.fo_cmp = file_kcmp_generic,
	.fo_flags = DFLAG_PASSABLE
};

static int
soo_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct socket *so = fp->f_data;
	int error;

#ifdef MAC
	error = mac_socket_check_receive(active_cred, so);
	if (error)
		return (error);
#endif
	error = soreceive(so, 0, uio, 0, 0, 0);
	return (error);
}

static int
soo_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct socket *so = fp->f_data;
	int error;

#ifdef MAC
	error = mac_socket_check_send(active_cred, so);
	if (error)
		return (error);
#endif
	error = sousrsend(so, NULL, uio, NULL, 0, NULL);
	return (error);
}

static int
soo_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *active_cred,
    struct thread *td)
{
	struct socket *so = fp->f_data;
	int error = 0;

	switch (cmd) {
	case FIONBIO:
		SOCK_LOCK(so);
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		SOCK_UNLOCK(so);
		break;

	case FIOASYNC:
		if (*(int *)data) {
			SOCK_LOCK(so);
			so->so_state |= SS_ASYNC;
			if (SOLISTENING(so)) {
				so->sol_sbrcv_flags |= SB_ASYNC;
				so->sol_sbsnd_flags |= SB_ASYNC;
			} else {
				SOCK_RECVBUF_LOCK(so);
				so->so_rcv.sb_flags |= SB_ASYNC;
				SOCK_RECVBUF_UNLOCK(so);
				SOCK_SENDBUF_LOCK(so);
				so->so_snd.sb_flags |= SB_ASYNC;
				SOCK_SENDBUF_UNLOCK(so);
			}
			SOCK_UNLOCK(so);
		} else {
			SOCK_LOCK(so);
			so->so_state &= ~SS_ASYNC;
			if (SOLISTENING(so)) {
				so->sol_sbrcv_flags &= ~SB_ASYNC;
				so->sol_sbsnd_flags &= ~SB_ASYNC;
			} else {
				SOCK_RECVBUF_LOCK(so);
				so->so_rcv.sb_flags &= ~SB_ASYNC;
				SOCK_RECVBUF_UNLOCK(so);
				SOCK_SENDBUF_LOCK(so);
				so->so_snd.sb_flags &= ~SB_ASYNC;
				SOCK_SENDBUF_UNLOCK(so);
			}
			SOCK_UNLOCK(so);
		}
		break;

	case FIONREAD:
		SOCK_RECVBUF_LOCK(so);
		if (SOLISTENING(so)) {
			error = EINVAL;
		} else {
			*(int *)data = sbavail(&so->so_rcv) - so->so_rcv.sb_ctl;
		}
		SOCK_RECVBUF_UNLOCK(so);
		break;

	case FIONWRITE:
		/* Unlocked read. */
		if (SOLISTENING(so)) {
			error = EINVAL;
		} else {
			*(int *)data = sbavail(&so->so_snd);
		}
		break;

	case FIONSPACE:
		/* Unlocked read. */
		if (SOLISTENING(so)) {
			error = EINVAL;
		} else {
			if ((so->so_snd.sb_hiwat < sbused(&so->so_snd)) ||
			    (so->so_snd.sb_mbmax < so->so_snd.sb_mbcnt)) {
				*(int *)data = 0;
			} else {
				*(int *)data = sbspace(&so->so_snd);
			}
		}
		break;

	case FIOSETOWN:
		error = fsetown(*(int *)data, &so->so_sigio);
		break;

	case FIOGETOWN:
		*(int *)data = fgetown(&so->so_sigio);
		break;

	case SIOCSPGRP:
		error = fsetown(-(*(int *)data), &so->so_sigio);
		break;

	case SIOCGPGRP:
		*(int *)data = -fgetown(&so->so_sigio);
		break;

	case SIOCATMARK:
		/* Unlocked read. */
		if (SOLISTENING(so)) {
			error = EINVAL;
		} else {
			*(int *)data = (so->so_rcv.sb_state & SBS_RCVATMARK) != 0;
		}
		break;
	default:
		/*
		 * Interface/routing/protocol specific ioctls: interface and
		 * routing ioctls should have a different entry since a
		 * socket is unnecessary.
		 */
		if (IOCGROUP(cmd) == 'i')
			error = ifioctl(so, cmd, data, td);
		else if (IOCGROUP(cmd) == 'r') {
			CURVNET_SET(so->so_vnet);
			error = rtioctl_fib(cmd, data, so->so_fibnum);
			CURVNET_RESTORE();
		} else {
			CURVNET_SET(so->so_vnet);
			error = so->so_proto->pr_control(so, cmd, data, 0, td);
			CURVNET_RESTORE();
		}
		break;
	}
	return (error);
}

static int
soo_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct socket *so = fp->f_data;
#ifdef MAC
	int error;

	error = mac_socket_check_poll(active_cred, so);
	if (error)
		return (error);
#endif
	return (so->so_proto->pr_sopoll(so, events, td));
}

static int
soo_stat(struct file *fp, struct stat *ub, struct ucred *active_cred)
{
	struct socket *so = fp->f_data;
	int error = 0;

	bzero((caddr_t)ub, sizeof (*ub));
	ub->st_mode = S_IFSOCK;
#ifdef MAC
	error = mac_socket_check_stat(active_cred, so);
	if (error)
		return (error);
#endif
	SOCK_LOCK(so);
	if (!SOLISTENING(so)) {
		struct sockbuf *sb;

		/*
		 * If SBS_CANTRCVMORE is set, but there's still data left
		 * in the receive buffer, the socket is still readable.
		 */
		sb = &so->so_rcv;
		SOCK_RECVBUF_LOCK(so);
		if ((sb->sb_state & SBS_CANTRCVMORE) == 0 || sbavail(sb))
			ub->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
		ub->st_size = sbavail(sb) - sb->sb_ctl;
		SOCK_RECVBUF_UNLOCK(so);

		sb = &so->so_snd;
		SOCK_SENDBUF_LOCK(so);
		if ((sb->sb_state & SBS_CANTSENDMORE) == 0)
			ub->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
		SOCK_SENDBUF_UNLOCK(so);
	}
	ub->st_uid = so->so_cred->cr_uid;
	ub->st_gid = so->so_cred->cr_gid;
	if (so->so_proto->pr_sense)
		error = so->so_proto->pr_sense(so, ub);
	SOCK_UNLOCK(so);
	return (error);
}

/*
 * API socket close on file pointer.  We call soclose() to close the socket
 * (including initiating closing protocols).  soclose() will sorele() the
 * file reference but the actual socket will not go away until the socket's
 * ref count hits 0.
 */
static int
soo_close(struct file *fp, struct thread *td)
{
	int error = 0;
	struct socket *so;

	so = fp->f_data;
	fp->f_ops = &badfileops;
	fp->f_data = NULL;

	if (so)
		error = soclose(so);
	return (error);
}

static int
soo_chmod(struct file *fp, mode_t mode, struct ucred *cred, struct thread *td)
{
	struct socket *so;
	int error;

	so = fp->f_data;
	if (so->so_proto->pr_chmod != NULL)
		error = so->so_proto->pr_chmod(so, mode, cred, td);
	else
		error = EINVAL;
	return (error);
}

static int
soo_fill_kinfo(struct file *fp, struct kinfo_file *kif, struct filedesc *fdp)
{
	struct sockaddr_storage ss = { .ss_len = sizeof(ss) };
	struct unpcb *unpcb;
	struct socket *so;
	int error;

	kif->kf_type = KF_TYPE_SOCKET;
	so = fp->f_data;
	CURVNET_SET(so->so_vnet);
	kif->kf_un.kf_sock.kf_sock_domain0 =
	    so->so_proto->pr_domain->dom_family;
	kif->kf_un.kf_sock.kf_sock_type0 = so->so_type;
	kif->kf_un.kf_sock.kf_sock_protocol0 = so->so_proto->pr_protocol;
	kif->kf_un.kf_sock.kf_sock_pcb = (uintptr_t)so->so_pcb;
	switch (kif->kf_un.kf_sock.kf_sock_domain0) {
	case AF_INET:
	case AF_INET6:
		/* XXX: kf_sock_inpcb is obsolete.  It may be removed. */
		kif->kf_un.kf_sock.kf_sock_inpcb = (uintptr_t)so->so_pcb;
		kif->kf_un.kf_sock.kf_sock_rcv_sb_state =
		    so->so_rcv.sb_state;
		kif->kf_un.kf_sock.kf_sock_snd_sb_state =
		    so->so_snd.sb_state;
		kif->kf_un.kf_sock.kf_sock_sendq =
		    sbused(&so->so_snd);
		kif->kf_un.kf_sock.kf_sock_recvq =
		    sbused(&so->so_rcv);
		break;
	case AF_UNIX:
		if (so->so_pcb != NULL) {
			unpcb = (struct unpcb *)(so->so_pcb);
			if (unpcb->unp_conn) {
				kif->kf_un.kf_sock.kf_sock_unpconn =
				    (uintptr_t)unpcb->unp_conn;
				kif->kf_un.kf_sock.kf_sock_rcv_sb_state =
				    so->so_rcv.sb_state;
				kif->kf_un.kf_sock.kf_sock_snd_sb_state =
				    so->so_snd.sb_state;
				kif->kf_un.kf_sock.kf_sock_sendq =
				    sbused(&so->so_snd);
				kif->kf_un.kf_sock.kf_sock_recvq =
				    sbused(&so->so_rcv);
			}
		}
		break;
	}
	error = sosockaddr(so, (struct sockaddr *)&ss);
	if (error == 0 &&
	    ss.ss_len <= sizeof(kif->kf_un.kf_sock.kf_sa_local)) {
		bcopy(&ss, &kif->kf_un.kf_sock.kf_sa_local, ss.ss_len);
	}
	ss.ss_len = sizeof(ss);
	error = sopeeraddr(so, (struct sockaddr *)&ss);
	if (error == 0 &&
	    ss.ss_len <= sizeof(kif->kf_un.kf_sock.kf_sa_peer)) {
		bcopy(&ss, &kif->kf_un.kf_sock.kf_sa_peer, ss.ss_len);
	}
	strncpy(kif->kf_path, so->so_proto->pr_domain->dom_name,
	    sizeof(kif->kf_path));
	CURVNET_RESTORE();
	return (0);	
}

/*
 * Use the 'backend3' field in AIO jobs to store the amount of data
 * completed by the AIO job so far.
 */
#define	aio_done	backend3

static STAILQ_HEAD(, task) soaio_jobs;
static struct mtx soaio_jobs_lock;
static struct task soaio_kproc_task;
static int soaio_starting, soaio_idle, soaio_queued;
static struct unrhdr *soaio_kproc_unr;

static int soaio_max_procs = MAX_AIO_PROCS;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, max_procs, CTLFLAG_RW, &soaio_max_procs, 0,
    "Maximum number of kernel processes to use for async socket IO");

static int soaio_num_procs;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, num_procs, CTLFLAG_RD, &soaio_num_procs, 0,
    "Number of active kernel processes for async socket IO");

static int soaio_target_procs = TARGET_AIO_PROCS;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, target_procs, CTLFLAG_RD,
    &soaio_target_procs, 0,
    "Preferred number of ready kernel processes for async socket IO");

static int soaio_lifetime;
SYSCTL_INT(_kern_ipc_aio, OID_AUTO, lifetime, CTLFLAG_RW, &soaio_lifetime, 0,
    "Maximum lifetime for idle aiod");

static void
soaio_kproc_loop(void *arg)
{
	struct proc *p;
	struct vmspace *myvm;
	struct task *task;
	int error, id, pending;

	id = (intptr_t)arg;

	/*
	 * Grab an extra reference on the daemon's vmspace so that it
	 * doesn't get freed by jobs that switch to a different
	 * vmspace.
	 */
	p = curproc;
	myvm = vmspace_acquire_ref(p);

	mtx_lock(&soaio_jobs_lock);
	MPASS(soaio_starting > 0);
	soaio_starting--;
	for (;;) {
		while (!STAILQ_EMPTY(&soaio_jobs)) {
			task = STAILQ_FIRST(&soaio_jobs);
			STAILQ_REMOVE_HEAD(&soaio_jobs, ta_link);
			soaio_queued--;
			pending = task->ta_pending;
			task->ta_pending = 0;
			mtx_unlock(&soaio_jobs_lock);

			task->ta_func(task->ta_context, pending);

			mtx_lock(&soaio_jobs_lock);
		}
		MPASS(soaio_queued == 0);

		if (p->p_vmspace != myvm) {
			mtx_unlock(&soaio_jobs_lock);
			vmspace_switch_aio(myvm);
			mtx_lock(&soaio_jobs_lock);
			continue;
		}

		soaio_idle++;
		error = mtx_sleep(&soaio_idle, &soaio_jobs_lock, 0, "-",
		    soaio_lifetime);
		soaio_idle--;
		if (error == EWOULDBLOCK && STAILQ_EMPTY(&soaio_jobs) &&
		    soaio_num_procs > soaio_target_procs)
			break;
	}
	soaio_num_procs--;
	mtx_unlock(&soaio_jobs_lock);
	free_unr(soaio_kproc_unr, id);
	kproc_exit(0);
}

static void
soaio_kproc_create(void *context, int pending)
{
	struct proc *p;
	int error, id;

	mtx_lock(&soaio_jobs_lock);
	for (;;) {
		if (soaio_num_procs < soaio_target_procs) {
			/* Must create */
		} else if (soaio_num_procs >= soaio_max_procs) {
			/*
			 * Hit the limit on kernel processes, don't
			 * create another one.
			 */
			break;
		} else if (soaio_queued <= soaio_idle + soaio_starting) {
			/*
			 * No more AIO jobs waiting for a process to be
			 * created, so stop.
			 */
			break;
		}
		soaio_starting++;
		mtx_unlock(&soaio_jobs_lock);

		id = alloc_unr(soaio_kproc_unr);
		error = kproc_create(soaio_kproc_loop, (void *)(intptr_t)id,
		    &p, 0, 0, "soaiod%d", id);
		if (error != 0) {
			free_unr(soaio_kproc_unr, id);
			mtx_lock(&soaio_jobs_lock);
			soaio_starting--;
			break;
		}

		mtx_lock(&soaio_jobs_lock);
		soaio_num_procs++;
	}
	mtx_unlock(&soaio_jobs_lock);
}

void
soaio_enqueue(struct task *task)
{

	mtx_lock(&soaio_jobs_lock);
	MPASS(task->ta_pending == 0);
	task->ta_pending++;
	STAILQ_INSERT_TAIL(&soaio_jobs, task, ta_link);
	soaio_queued++;
	if (soaio_queued <= soaio_idle)
		wakeup_one(&soaio_idle);
	else if (soaio_num_procs < soaio_max_procs)
		taskqueue_enqueue(taskqueue_thread, &soaio_kproc_task);
	mtx_unlock(&soaio_jobs_lock);
}

static void
soaio_init(void)
{

	soaio_lifetime = AIOD_LIFETIME_DEFAULT;
	STAILQ_INIT(&soaio_jobs);
	mtx_init(&soaio_jobs_lock, "soaio jobs", NULL, MTX_DEF);
	soaio_kproc_unr = new_unrhdr(1, INT_MAX, NULL);
	TASK_INIT(&soaio_kproc_task, 0, soaio_kproc_create, NULL);
}
SYSINIT(soaio, SI_SUB_VFS, SI_ORDER_ANY, soaio_init, NULL);

static __inline int
soaio_ready(struct socket *so, struct sockbuf *sb)
{
	return (sb == &so->so_rcv ? soreadable(so) : sowriteable(so));
}

static void
soaio_process_job(struct socket *so, sb_which which, struct kaiocb *job)
{
	struct ucred *td_savedcred;
	struct thread *td;
	struct sockbuf *sb = sobuf(so, which);
#ifdef MAC
	struct file *fp = job->fd_file;
#endif
	size_t cnt, done, job_total_nbytes __diagused;
	long ru_before;
	int error, flags;

	SOCK_BUF_UNLOCK(so, which);
	aio_switch_vmspace(job);
	td = curthread;
retry:
	td_savedcred = td->td_ucred;
	td->td_ucred = job->cred;

	job_total_nbytes = job->uiop->uio_resid + job->aio_done;
	done = job->aio_done;
	cnt = job->uiop->uio_resid;
	job->uiop->uio_offset = 0;
	job->uiop->uio_td = td;
	flags = MSG_NBIO;

	/*
	 * For resource usage accounting, only count a completed request
	 * as a single message to avoid counting multiple calls to
	 * sosend/soreceive on a blocking socket.
	 */

	if (sb == &so->so_rcv) {
		ru_before = td->td_ru.ru_msgrcv;
#ifdef MAC
		error = mac_socket_check_receive(fp->f_cred, so);
		if (error == 0)

#endif
			error = soreceive(so, NULL, job->uiop, NULL, NULL,
			    &flags);
		if (td->td_ru.ru_msgrcv != ru_before)
			job->msgrcv = 1;
	} else {
		if (!TAILQ_EMPTY(&sb->sb_aiojobq))
			flags |= MSG_MORETOCOME;
		ru_before = td->td_ru.ru_msgsnd;
#ifdef MAC
		error = mac_socket_check_send(fp->f_cred, so);
		if (error == 0)
#endif
			error = sousrsend(so, NULL, job->uiop, NULL, flags,
			    job->userproc);
		if (td->td_ru.ru_msgsnd != ru_before)
			job->msgsnd = 1;
	}

	done += cnt - job->uiop->uio_resid;
	job->aio_done = done;
	td->td_ucred = td_savedcred;

	if (error == EWOULDBLOCK) {
		/*
		 * The request was either partially completed or not
		 * completed at all due to racing with a read() or
		 * write() on the socket.  If the socket is
		 * non-blocking, return with any partial completion.
		 * If the socket is blocking or if no progress has
		 * been made, requeue this request at the head of the
		 * queue to try again when the socket is ready.
		 */
		MPASS(done != job_total_nbytes);
		SOCK_BUF_LOCK(so, which);
		if (done == 0 || !(so->so_state & SS_NBIO)) {
			empty_results++;
			if (soaio_ready(so, sb)) {
				empty_retries++;
				SOCK_BUF_UNLOCK(so, which);
				goto retry;
			}
			
			if (!aio_set_cancel_function(job, soo_aio_cancel)) {
				SOCK_BUF_UNLOCK(so, which);
				if (done != 0)
					aio_complete(job, done, 0);
				else
					aio_cancel(job);
				SOCK_BUF_LOCK(so, which);
			} else {
				TAILQ_INSERT_HEAD(&sb->sb_aiojobq, job, list);
			}
			return;
		}
		SOCK_BUF_UNLOCK(so, which);
	}		
	if (done != 0 && (error == ERESTART || error == EINTR ||
	    error == EWOULDBLOCK))
		error = 0;
	if (error)
		aio_complete(job, -1, error);
	else
		aio_complete(job, done, 0);
	SOCK_BUF_LOCK(so, which);
}

static void
soaio_process_sb(struct socket *so, sb_which which)
{
	struct kaiocb *job;
	struct sockbuf *sb = sobuf(so, which);

	CURVNET_SET(so->so_vnet);
	SOCK_BUF_LOCK(so, which);
	while (!TAILQ_EMPTY(&sb->sb_aiojobq) && soaio_ready(so, sb)) {
		job = TAILQ_FIRST(&sb->sb_aiojobq);
		TAILQ_REMOVE(&sb->sb_aiojobq, job, list);
		if (!aio_clear_cancel_function(job))
			continue;

		soaio_process_job(so, which, job);
	}

	/*
	 * If there are still pending requests, the socket must not be
	 * ready so set SB_AIO to request a wakeup when the socket
	 * becomes ready.
	 */
	if (!TAILQ_EMPTY(&sb->sb_aiojobq))
		sb->sb_flags |= SB_AIO;
	sb->sb_flags &= ~SB_AIO_RUNNING;
	SOCK_BUF_UNLOCK(so, which);

	sorele(so);
	CURVNET_RESTORE();
}

void
soaio_rcv(void *context, int pending)
{
	struct socket *so;

	so = context;
	soaio_process_sb(so, SO_RCV);
}

void
soaio_snd(void *context, int pending)
{
	struct socket *so;

	so = context;
	soaio_process_sb(so, SO_SND);
}

void
sowakeup_aio(struct socket *so, sb_which which)
{
	struct sockbuf *sb = sobuf(so, which);

	SOCK_BUF_LOCK_ASSERT(so, which);

	sb->sb_flags &= ~SB_AIO;
	if (sb->sb_flags & SB_AIO_RUNNING)
		return;
	sb->sb_flags |= SB_AIO_RUNNING;
	soref(so);
	soaio_enqueue(&sb->sb_aiotask);
}

static void
soo_aio_cancel(struct kaiocb *job)
{
	struct socket *so;
	struct sockbuf *sb;
	long done;
	int opcode;
	sb_which which;

	so = job->fd_file->f_data;
	opcode = job->uaiocb.aio_lio_opcode;
	if (opcode & LIO_READ) {
		sb = &so->so_rcv;
		which = SO_RCV;
	} else {
		MPASS(opcode & LIO_WRITE);
		sb = &so->so_snd;
		which = SO_SND;
	}

	SOCK_BUF_LOCK(so, which);
	if (!aio_cancel_cleared(job))
		TAILQ_REMOVE(&sb->sb_aiojobq, job, list);
	if (TAILQ_EMPTY(&sb->sb_aiojobq))
		sb->sb_flags &= ~SB_AIO;
	SOCK_BUF_UNLOCK(so, which);

	done = job->aio_done;
	if (done != 0)
		aio_complete(job, done, 0);
	else
		aio_cancel(job);
}

static int
soo_aio_queue(struct file *fp, struct kaiocb *job)
{
	struct socket *so = fp->f_data;

	return (so->so_proto->pr_aio_queue(so, job));
}

int
soaio_queue_generic(struct socket *so, struct kaiocb *job)
{
	struct sockbuf *sb;
	sb_which which;

	/* Lock through the socket, since this may be a listening socket. */
	switch (job->uaiocb.aio_lio_opcode & (LIO_WRITE | LIO_READ)) {
	case LIO_READ:
		SOCK_RECVBUF_LOCK(so);
		sb = &so->so_rcv;
		which = SO_RCV;
		break;
	case LIO_WRITE:
		SOCK_SENDBUF_LOCK(so);
		sb = &so->so_snd;
		which = SO_SND;
		break;
	default:
		return (EINVAL);
	}

	if (SOLISTENING(so)) {
		SOCK_BUF_UNLOCK(so, which);
		return (EINVAL);
	}

	if (!aio_set_cancel_function(job, soo_aio_cancel))
		panic("new job was cancelled");
	TAILQ_INSERT_TAIL(&sb->sb_aiojobq, job, list);
	if (!(sb->sb_flags & SB_AIO_RUNNING)) {
		if (soaio_ready(so, sb))
			sowakeup_aio(so, which);
		else
			sb->sb_flags |= SB_AIO;
	}
	SOCK_BUF_UNLOCK(so, which);
	return (0);
}
