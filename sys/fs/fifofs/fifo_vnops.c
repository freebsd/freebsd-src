/*-
 * Copyright (c) 1990, 1993, 1995
 *	The Regents of the University of California.
 * Copyright (c) 2005 Robert N. M. Watson
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
 *	@(#)fifo_vnops.c	8.10 (Berkeley) 5/27/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/event.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/proc.h> /* XXXKSE */
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <fs/fifofs/fifo.h>

static fo_rdwr_t        fifo_read_f;
static fo_rdwr_t        fifo_write_f;
static fo_ioctl_t       fifo_ioctl_f;
static fo_poll_t        fifo_poll_f;
static fo_kqfilter_t    fifo_kqfilter_f;
static fo_stat_t        fifo_stat_f;
static fo_close_t       fifo_close_f;

struct fileops fifo_ops_f = {
	.fo_read =      fifo_read_f,
	.fo_write =     fifo_write_f,
	.fo_ioctl =     fifo_ioctl_f,
	.fo_poll =      fifo_poll_f,
	.fo_kqfilter =  fifo_kqfilter_f,
	.fo_stat =      fifo_stat_f,
	.fo_close =     fifo_close_f,
	.fo_flags =     DFLAG_PASSABLE
};

/*
 * This structure is associated with the FIFO vnode and stores the state
 * associated with the FIFO.
 *
 * XXXRW: The presence of an sx lock here is undesirable, and exists to avoid
 * exposing threading race conditions in the socket code that have not yet
 * been resolved.  Once those problems are resolved, the sx lock here should
 * be removed.
 */
struct fifoinfo {
	struct socket	*fi_readsock;
	struct socket	*fi_writesock;
	long		fi_readers;
	long		fi_writers;
	struct sx	fi_sx;
};

static vop_print_t	fifo_print;
static vop_open_t	fifo_open;
static vop_close_t	fifo_close;
static vop_ioctl_t	fifo_ioctl;
static vop_kqfilter_t	fifo_kqfilter;
static vop_pathconf_t	fifo_pathconf;
static vop_advlock_t	fifo_advlock;

static void	filt_fifordetach(struct knote *kn);
static int	filt_fiforead(struct knote *kn, long hint);
static void	filt_fifowdetach(struct knote *kn);
static int	filt_fifowrite(struct knote *kn, long hint);
static void	filt_fifodetach_notsup(struct knote *kn);
static int	filt_fifo_notsup(struct knote *kn, long hint);

static struct filterops fiforead_filtops =
	{ 1, NULL, filt_fifordetach, filt_fiforead };
static struct filterops fifowrite_filtops =
	{ 1, NULL, filt_fifowdetach, filt_fifowrite };
static struct filterops fifo_notsup_filtops =
	{ 1, NULL, filt_fifodetach_notsup, filt_fifo_notsup };

struct vop_vector fifo_specops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		VOP_EBADF,
	.vop_advlock =		fifo_advlock,
	.vop_close =		fifo_close,
	.vop_create =		VOP_PANIC,
	.vop_getattr =		VOP_EBADF,
	.vop_ioctl =		fifo_ioctl,
	.vop_kqfilter =		fifo_kqfilter,
	.vop_lease =		VOP_NULL,
	.vop_link =		VOP_PANIC,
	.vop_mkdir =		VOP_PANIC,
	.vop_mknod =		VOP_PANIC,
	.vop_open =		fifo_open,
	.vop_pathconf =		fifo_pathconf,
	.vop_print =		fifo_print,
	.vop_read =		VOP_PANIC,
	.vop_readdir =		VOP_PANIC,
	.vop_readlink =		VOP_PANIC,
	.vop_reallocblks =	VOP_PANIC,
	.vop_reclaim =		VOP_NULL,
	.vop_remove =		VOP_PANIC,
	.vop_rename =		VOP_PANIC,
	.vop_rmdir =		VOP_PANIC,
	.vop_setattr =		VOP_EBADF,
	.vop_symlink =		VOP_PANIC,
	.vop_write =		VOP_PANIC,
};

struct mtx fifo_mtx;
MTX_SYSINIT(fifo, &fifo_mtx, "fifo mutex", MTX_DEF);

/*
 * Dispose of fifo resources.
 */
static void
fifo_cleanup(struct vnode *vp)
{
	struct fifoinfo *fip = vp->v_fifoinfo;

	ASSERT_VOP_LOCKED(vp, "fifo_cleanup");
	if (fip->fi_readers == 0 && fip->fi_writers == 0) {
		vp->v_fifoinfo = NULL;
		(void)soclose(fip->fi_readsock);
		(void)soclose(fip->fi_writesock);
		sx_destroy(&fip->fi_sx);
		FREE(fip, M_VNODE);
	}
}

/*
 * Open called to set up a new instance of a fifo or
 * to find an active instance of a fifo.
 */
/* ARGSUSED */
static int
fifo_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fifoinfo *fip;
	struct thread *td = ap->a_td;
	struct ucred *cred = ap->a_cred;
	struct socket *rso, *wso;
	struct file *fp;
	int error;

	ASSERT_VOP_LOCKED(vp, "fifo_open");
	if ((fip = vp->v_fifoinfo) == NULL) {
		MALLOC(fip, struct fifoinfo *, sizeof(*fip), M_VNODE,
		    M_WAITOK | M_ZERO);
		error = socreate(AF_LOCAL, &rso, SOCK_STREAM, 0, cred, td);
		if (error)
			goto fail1;
		fip->fi_readsock = rso;
		error = socreate(AF_LOCAL, &wso, SOCK_STREAM, 0, cred, td);
		if (error)
			goto fail2;
		fip->fi_writesock = wso;
		error = soconnect2(wso, rso);
		if (error) {
			(void)soclose(wso);
fail2:
			(void)soclose(rso);
fail1:
			free(fip, M_VNODE);
			return (error);
		}
		fip->fi_readers = fip->fi_writers = 0;
		sx_init(&fip->fi_sx, "fifo_sx");
		wso->so_snd.sb_lowat = PIPE_BUF;
		SOCKBUF_LOCK(&rso->so_rcv);
		rso->so_rcv.sb_state |= SBS_CANTRCVMORE;
		SOCKBUF_UNLOCK(&rso->so_rcv);
		KASSERT(vp->v_fifoinfo == NULL,
		    ("fifo_open: v_fifoinfo race"));
		vp->v_fifoinfo = fip;
	}

	/*
	 * General access to fi_readers and fi_writers is protected using
	 * the vnode lock.
	 *
	 * Protect the increment of fi_readers and fi_writers and the
	 * associated calls to wakeup() with the fifo mutex in addition
	 * to the vnode lock.  This allows the vnode lock to be dropped
	 * for the msleep() calls below, and using the fifo mutex with
	 * msleep() prevents the wakeup from being missed.
	 */
	mtx_lock(&fifo_mtx);
	if (ap->a_mode & FREAD) {
		fip->fi_readers++;
		if (fip->fi_readers == 1) {
			SOCKBUF_LOCK(&fip->fi_writesock->so_snd);
			fip->fi_writesock->so_snd.sb_state &= ~SBS_CANTSENDMORE;
			SOCKBUF_UNLOCK(&fip->fi_writesock->so_snd);
			if (fip->fi_writers > 0) {
				wakeup(&fip->fi_writers);
				sowwakeup(fip->fi_writesock);
			}
		}
	}
	if (ap->a_mode & FWRITE) {
		if ((ap->a_mode & O_NONBLOCK) && fip->fi_readers == 0) {
			mtx_unlock(&fifo_mtx);
			return (ENXIO);
		}
		fip->fi_writers++;
		if (fip->fi_writers == 1) {
			SOCKBUF_LOCK(&fip->fi_readsock->so_rcv);
			fip->fi_readsock->so_rcv.sb_state &= ~SBS_CANTRCVMORE;
			SOCKBUF_UNLOCK(&fip->fi_readsock->so_rcv);
			if (fip->fi_readers > 0) {
				wakeup(&fip->fi_readers);
				sorwakeup(fip->fi_readsock);
			}
		}
	}
	if ((ap->a_mode & O_NONBLOCK) == 0) {
		if ((ap->a_mode & FREAD) && fip->fi_writers == 0) {
			VOP_UNLOCK(vp, 0, td);
			error = msleep(&fip->fi_readers, &fifo_mtx,
			    PDROP | PCATCH | PSOCK, "fifoor", 0);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			if (error) {
				fip->fi_readers--;
				if (fip->fi_readers == 0) {
					socantsendmore(fip->fi_writesock);
					fifo_cleanup(vp);
				}
				return (error);
			}
			mtx_lock(&fifo_mtx);
			/*
			 * We must have got woken up because we had a writer.
			 * That (and not still having one) is the condition
			 * that we must wait for.
			 */
		}
		if ((ap->a_mode & FWRITE) && fip->fi_readers == 0) {
			VOP_UNLOCK(vp, 0, td);
			error = msleep(&fip->fi_writers, &fifo_mtx,
			    PDROP | PCATCH | PSOCK, "fifoow", 0);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
			if (error) {
				fip->fi_writers--;
				if (fip->fi_writers == 0) {
					socantrcvmore(fip->fi_readsock);
					fifo_cleanup(vp);
				}
				return (error);
			}
			/*
			 * We must have got woken up because we had
			 * a reader.  That (and not still having one)
			 * is the condition that we must wait for.
			 */
			mtx_lock(&fifo_mtx);
		}
	}
	mtx_unlock(&fifo_mtx);
	KASSERT(ap->a_fdidx >= 0, ("can't fifo/vnode bypass %d", ap->a_fdidx));
	fp = ap->a_td->td_proc->p_fd->fd_ofiles[ap->a_fdidx];
	KASSERT(fp->f_ops == &badfileops, ("not badfileops in fifo_open"));
	fp->f_ops = &fifo_ops_f;
	fp->f_data = fip;
	return (0);
}

/*
 * Now unused vnode ioctl routine.
 */
/* ARGSUSED */
static int
fifo_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	printf("WARNING: fifo_ioctl called unexpectedly\n");
	return (ENOTTY);
}

/*
 * Now unused vnode kqfilter routine.
 */
/* ARGSUSED */
static int
fifo_kqfilter(ap)
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap;
{

	printf("WARNING: fifo_kqfilter called unexpectedly\n");
	return (EINVAL);
}

static void
filt_fifordetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SOCKBUF_LOCK(&so->so_rcv);
	knlist_remove(&so->so_rcv.sb_sel.si_note, kn, 1);
	if (knlist_empty(&so->so_rcv.sb_sel.si_note))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
	SOCKBUF_UNLOCK(&so->so_rcv);
}

static int
filt_fiforead(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	} else {
		kn->kn_flags &= ~EV_EOF;
		return (kn->kn_data > 0);
	}
}

static void
filt_fifowdetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SOCKBUF_LOCK(&so->so_snd);
	knlist_remove(&so->so_snd.sb_sel.si_note, kn, 1);
	if (knlist_empty(&so->so_snd.sb_sel.si_note))
		so->so_snd.sb_flags &= ~SB_KNOTE;
	SOCKBUF_UNLOCK(&so->so_snd);
}

static int
filt_fifowrite(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SOCKBUF_LOCK_ASSERT(&so->so_snd);
	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	} else {
		kn->kn_flags &= ~EV_EOF;
	        return (kn->kn_data >= so->so_snd.sb_lowat);
	}
}

static void
filt_fifodetach_notsup(struct knote *kn)
{

}

static int
filt_fifo_notsup(struct knote *kn, long hint)
{

	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
static int
fifo_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fifoinfo *fip = vp->v_fifoinfo;

	ASSERT_VOP_LOCKED(vp, "fifo_close");
	if (ap->a_fflag & FREAD) {
		fip->fi_readers--;
		if (fip->fi_readers == 0)
			socantsendmore(fip->fi_writesock);
	}
	if (ap->a_fflag & FWRITE) {
		fip->fi_writers--;
		if (fip->fi_writers == 0)
			socantrcvmore(fip->fi_readsock);
	}
	fifo_cleanup(vp);
	return (0);
}

/*
 * Print out internal contents of a fifo vnode.
 */
int
fifo_printinfo(vp)
	struct vnode *vp;
{
	register struct fifoinfo *fip = vp->v_fifoinfo;

	printf(", fifo with %ld readers and %ld writers",
		fip->fi_readers, fip->fi_writers);
	return (0);
}

/*
 * Print out the contents of a fifo vnode.
 */
static int
fifo_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	fifo_printinfo(ap->a_vp);
	printf("\n");
	return (0);
}

/*
 * Return POSIX pathconf information applicable to fifo's.
 */
static int
fifo_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Fifo advisory byte-level locks.
 */
/* ARGSUSED */
static int
fifo_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{

	return (ap->a_flags & F_FLOCK ? EOPNOTSUPP : EINVAL);
}

static int
fifo_close_f(struct file *fp, struct thread *td)
{

	return (vnops.fo_close(fp, td));
}

/*
 * The implementation of ioctl() for named fifos is complicated by the fact
 * that we permit O_RDWR fifo file descriptors, meaning that the actions of
 * ioctls may have to be applied to both the underlying sockets rather than
 * just one.  The original implementation simply forward the ioctl to one
 * or both sockets based on fp->f_flag.  We now consider each ioctl
 * separately, as the composition effect requires careful ordering.
 *
 * We do not blindly pass all ioctls through to the socket in order to avoid
 * providing unnecessary ioctls that might be improperly depended on by
 * applications (such as socket-specific, routing, and interface ioctls).
 *
 * Unlike sys_pipe.c, fifos do not implement the deprecated TIOCSPGRP and
 * TIOCGPGRP ioctls.  Earlier implementations of fifos did forward SIOCSPGRP
 * and SIOCGPGRP ioctls, so we might need to re-add those here.
 */
static int
fifo_ioctl_f(struct file *fp, u_long com, void *data, struct ucred *cred,
    struct thread *td)
{
	struct fifoinfo *fi;
	struct file filetmp;	/* Local, so need not be locked. */
	int error;

	error = ENOTTY;
	fi = fp->f_data;

	switch (com) {
	case FIONBIO:
		/*
		 * Non-blocking I/O is implemented at the fifo layer using
		 * MSG_NBIO, so does not need to be forwarded down the stack.
		 */
		return (0);

	case FIOASYNC:
	case FIOSETOWN:
	case FIOGETOWN:
		/*
		 * These socket ioctls don't have any ordering requirements,
		 * so are called in an arbitrary order, and only on the
		 * sockets indicated by the file descriptor rights.
		 *
		 * XXXRW: If O_RDWR and the read socket accepts an ioctl but
		 * the write socket doesn't, the socketpair is left in an
		 * inconsistent state.
		 */
		if (fp->f_flag & FREAD) {
			filetmp.f_data = fi->fi_readsock;
			filetmp.f_cred = cred;
			error = soo_ioctl(&filetmp, com, data, cred, td);
			if (error)
				return (error);
		}
		if (fp->f_flag & FWRITE) {
			filetmp.f_data = fi->fi_writesock;
			filetmp.f_cred = cred;
			error = soo_ioctl(&filetmp, com, data, cred, td);
		}
		return (error);

	case FIONREAD:
		/*
		 * FIONREAD will return 0 for non-readable descriptors, and
		 * the results of FIONREAD on the read socket for readable
		 * descriptors.
		 */
		if (!(fp->f_flag & FREAD)) {
			*(int *)data = 0;
			return (0);
		}
		filetmp.f_data = fi->fi_readsock;
		filetmp.f_cred = cred;
		return (soo_ioctl(&filetmp, com, data, cred, td));

	default:
		return (ENOTTY);
	}
}

/*
 * Because fifos are now a file descriptor layer object, EVFILT_VNODE is not
 * implemented.  Likely, fifo_kqfilter() should be removed, and
 * fifo_kqfilter_f() should know how to forward the request to the underling
 * vnode using f_vnode in the file descriptor here.
 */
static int
fifo_kqfilter_f(struct file *fp, struct knote *kn)
{
	struct fifoinfo *fi;
	struct socket *so;
	struct sockbuf *sb;

	fi = fp->f_data;

	/*
	 * If a filter is requested that is not supported by this file
	 * descriptor, don't return an error, but also don't ever generate an
	 * event.
	 */
	if ((kn->kn_filter == EVFILT_READ) && !(fp->f_flag & FREAD)) {
		kn->kn_fop = &fifo_notsup_filtops;
		return (0);
	}

	if ((kn->kn_filter == EVFILT_WRITE) && !(fp->f_flag & FWRITE)) {
		kn->kn_fop = &fifo_notsup_filtops;
		return (0);
	}

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &fiforead_filtops;
		so = fi->fi_readsock;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &fifowrite_filtops;
		so = fi->fi_writesock;
		sb = &so->so_snd;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)so;

	SOCKBUF_LOCK(sb);
	knlist_add(&sb->sb_sel.si_note, kn, 1);
	sb->sb_flags |= SB_KNOTE;
	SOCKBUF_UNLOCK(sb);

	return (0);
}

static int
fifo_poll_f(struct file *fp, int events, struct ucred *cred, struct thread *td)
{
	struct fifoinfo *fip;
	struct file filetmp;
	int levents, revents = 0;

	fip = fp->f_data;
	levents = events &
	    (POLLIN | POLLINIGNEOF | POLLPRI | POLLRDNORM | POLLRDBAND);
	if ((fp->f_flag & FREAD) && levents) {
		/*
		 * If POLLIN or POLLRDNORM is requested and POLLINIGNEOF is
		 * not, then convert the first two to the last one.  This
		 * tells the socket poll function to ignore EOF so that we
		 * block if there is no writer (and no data).  Callers can
		 * set POLLINIGNEOF to get non-blocking behavior.
		 */
		if (levents & (POLLIN | POLLRDNORM) &&
		    !(levents & POLLINIGNEOF)) {
			levents &= ~(POLLIN | POLLRDNORM);
			levents |= POLLINIGNEOF;
		}

		filetmp.f_data = fip->fi_readsock;
		filetmp.f_cred = cred;
		revents |= soo_poll(&filetmp, levents, cred, td);

		/* Reverse the above conversion. */
		if ((revents & POLLINIGNEOF) && !(events & POLLINIGNEOF)) {
			revents |= (events & (POLLIN | POLLRDNORM));
			revents &= ~POLLINIGNEOF;
		}
	}
	levents = events & (POLLOUT | POLLWRNORM | POLLWRBAND);
	if ((fp->f_flag & FWRITE) && levents) {
		filetmp.f_data = fip->fi_writesock;
		filetmp.f_cred = cred;
		revents |= soo_poll(&filetmp, levents, cred, td);
	}
	return (revents);
}

static int
fifo_read_f(struct file *fp, struct uio *uio, struct ucred *cred, int flags, struct thread *td)
{
	struct fifoinfo *fip;
	int error, sflags;

	fip = fp->f_data;
	KASSERT(uio->uio_rw == UIO_READ,("fifo_read mode"));
	if (uio->uio_resid == 0)
		return (0);
	sflags = (fp->f_flag & FNONBLOCK) ? MSG_NBIO : 0;
	sx_xlock(&fip->fi_sx);
	error = soreceive(fip->fi_readsock, NULL, uio, NULL, NULL, &sflags);
	sx_xunlock(&fip->fi_sx);
	return (error);
}

static int
fifo_stat_f(struct file *fp, struct stat *sb, struct ucred *cred, struct thread *td)
{

	return (vnops.fo_stat(fp, sb, cred, td));
}

static int
fifo_write_f(struct file *fp, struct uio *uio, struct ucred *cred, int flags, struct thread *td)
{
	struct fifoinfo *fip;
	int error, sflags;

	fip = fp->f_data;
	KASSERT(uio->uio_rw == UIO_WRITE,("fifo_write mode"));
	sflags = (fp->f_flag & FNONBLOCK) ? MSG_NBIO : 0;
	sx_xlock(&fip->fi_sx);
	error = sosend(fip->fi_writesock, NULL, uio, 0, NULL, sflags, td);
	sx_xunlock(&fip->fi_sx);
	return (error);
}
