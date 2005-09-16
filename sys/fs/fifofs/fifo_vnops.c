/*-
 * Copyright (c) 1990, 1993, 1995
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
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
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

/*
 * This structure is associated with the FIFO vnode and stores
 * the state associated with the FIFO.
 */
struct fifoinfo {
	struct socket	*fi_readsock;
	struct socket	*fi_writesock;
	long		fi_readers;
	long		fi_writers;
};

static int	fifo_print(struct vop_print_args *);
static int	fifo_lookup(struct vop_lookup_args *);
static int	fifo_open(struct vop_open_args *);
static int	fifo_close(struct vop_close_args *);
static int	fifo_read(struct vop_read_args *);
static int	fifo_write(struct vop_write_args *);
static int	fifo_ioctl(struct vop_ioctl_args *);
static int	fifo_poll(struct vop_poll_args *);
static int	fifo_kqfilter(struct vop_kqfilter_args *);
static int	fifo_pathconf(struct vop_pathconf_args *);
static int	fifo_advlock(struct vop_advlock_args *);

static void	filt_fifordetach(struct knote *kn);
static int	filt_fiforead(struct knote *kn, long hint);
static void	filt_fifowdetach(struct knote *kn);
static int	filt_fifowrite(struct knote *kn, long hint);

static struct filterops fiforead_filtops =
	{ 1, NULL, filt_fifordetach, filt_fiforead };
static struct filterops fifowrite_filtops =
	{ 1, NULL, filt_fifowdetach, filt_fifowrite };

vop_t **fifo_vnodeop_p;
static struct vnodeopv_entry_desc fifo_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) vop_ebadf },
	{ &vop_advlock_desc,		(vop_t *) fifo_advlock },
	{ &vop_close_desc,		(vop_t *) fifo_close },
	{ &vop_create_desc,		(vop_t *) vop_panic },
	{ &vop_getattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_getwritemount_desc, 	(vop_t *) vop_stdgetwritemount },
	{ &vop_ioctl_desc,		(vop_t *) fifo_ioctl },
	{ &vop_kqfilter_desc,		(vop_t *) fifo_kqfilter },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_link_desc,		(vop_t *) vop_panic },
	{ &vop_lookup_desc,		(vop_t *) fifo_lookup },
	{ &vop_mkdir_desc,		(vop_t *) vop_panic },
	{ &vop_mknod_desc,		(vop_t *) vop_panic },
	{ &vop_open_desc,		(vop_t *) fifo_open },
	{ &vop_pathconf_desc,		(vop_t *) fifo_pathconf },
	{ &vop_poll_desc,		(vop_t *) fifo_poll },
	{ &vop_print_desc,		(vop_t *) fifo_print },
	{ &vop_read_desc,		(vop_t *) fifo_read },
	{ &vop_readdir_desc,		(vop_t *) vop_panic },
	{ &vop_readlink_desc,		(vop_t *) vop_panic },
	{ &vop_reallocblks_desc,	(vop_t *) vop_panic },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_remove_desc,		(vop_t *) vop_panic },
	{ &vop_rename_desc,		(vop_t *) vop_panic },
	{ &vop_rmdir_desc,		(vop_t *) vop_panic },
	{ &vop_setattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_symlink_desc,		(vop_t *) vop_panic },
	{ &vop_write_desc,		(vop_t *) fifo_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc fifo_vnodeop_opv_desc =
	{ &fifo_vnodeop_p, fifo_vnodeop_entries };

VNODEOP_SET(fifo_vnodeop_opv_desc);

struct mtx fifo_mtx;
MTX_SYSINIT(fifo, &fifo_mtx, "fifo mutex", MTX_DEF);

int
fifo_vnoperate(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	return (VOCALL(fifo_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
static int
fifo_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

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
	int error;

	if ((fip = vp->v_fifoinfo) == NULL) {
		MALLOC(fip, struct fifoinfo *, sizeof(*fip), M_VNODE, M_WAITOK);
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
			SOCKBUF_LOCK(&fip->fi_writesock->so_rcv);
			fip->fi_readsock->so_rcv.sb_state &= ~SBS_CANTRCVMORE;
			SOCKBUF_UNLOCK(&fip->fi_writesock->so_rcv);
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
			return (0);
		}
	}
	mtx_unlock(&fifo_mtx);
	return (0);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
static int
fifo_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct socket *rso = ap->a_vp->v_fifoinfo->fi_readsock;
	struct thread *td = uio->uio_td;
	int error, flags;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("fifo_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	VOP_UNLOCK(ap->a_vp, 0, td);
	flags = (ap->a_ioflag & IO_NDELAY) ? MSG_NBIO : 0;
	error = soreceive(rso, (struct sockaddr **)0, uio, (struct mbuf **)0,
	    (struct mbuf **)0, &flags);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY, td);
	return (error);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
static int
fifo_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct socket *wso = ap->a_vp->v_fifoinfo->fi_writesock;
	struct thread *td = ap->a_uio->uio_td;
	int error, flags;

#ifdef DIAGNOSTIC
	if (ap->a_uio->uio_rw != UIO_WRITE)
		panic("fifo_write mode");
#endif
	VOP_UNLOCK(ap->a_vp, 0, td);
	flags = (ap->a_ioflag & IO_NDELAY) ? MSG_NBIO : 0;
	error = sosend(wso, (struct sockaddr *)0, ap->a_uio, 0,
	    (struct mbuf *)0, flags, td);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY, td);
	return (error);
}

/*
 * Device ioctl operation.
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
	struct file filetmp;	/* Local, so need not be locked. */
	int error;

	if (ap->a_command == FIONBIO)
		return (0);
	if (ap->a_fflag & FREAD) {
		filetmp.f_data = ap->a_vp->v_fifoinfo->fi_readsock;
		filetmp.f_cred = ap->a_cred;
		error = soo_ioctl(&filetmp, ap->a_command, ap->a_data,
		    ap->a_td->td_ucred, ap->a_td);
		if (error)
			return (error);
	}
	if (ap->a_fflag & FWRITE) {
		filetmp.f_data = ap->a_vp->v_fifoinfo->fi_writesock;
		filetmp.f_cred = ap->a_cred;
		error = soo_ioctl(&filetmp, ap->a_command, ap->a_data,
		    ap->a_td->td_ucred, ap->a_td);
		if (error)
			return (error);
	}
	return (0);
}

/* ARGSUSED */
static int
fifo_kqfilter(ap)
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap;
{
	struct fifoinfo *fi = ap->a_vp->v_fifoinfo;
	struct socket *so;
	struct sockbuf *sb;

	switch (ap->a_kn->kn_filter) {
	case EVFILT_READ:
		ap->a_kn->kn_fop = &fiforead_filtops;
		so = fi->fi_readsock;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		ap->a_kn->kn_fop = &fifowrite_filtops;
		so = fi->fi_writesock;
		sb = &so->so_snd;
		break;
	default:
		return (1);
	}

	ap->a_kn->kn_hook = (caddr_t)so;

	SOCKBUF_LOCK(sb);
	knlist_add(&sb->sb_sel.si_note, ap->a_kn, 1);
	sb->sb_flags |= SB_KNOTE;
	SOCKBUF_UNLOCK(sb);

	return (0);
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
	int need_lock, result;

	need_lock = !SOCKBUF_OWNED(&so->so_rcv);
	if (need_lock)
		SOCKBUF_LOCK(&so->so_rcv);
	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_rcv.sb_state & SBS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		result = 1;
	} else {
		kn->kn_flags &= ~EV_EOF;
		result = (kn->kn_data > 0);
	}
	if (need_lock)
		SOCKBUF_UNLOCK(&so->so_rcv);
	return (result);
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
	int need_lock, result;

	need_lock = !SOCKBUF_OWNED(&so->so_snd);
	if (need_lock)
		SOCKBUF_LOCK(&so->so_snd);
	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		result = 1;
	} else {
		kn->kn_flags &= ~EV_EOF;
	        result = (kn->kn_data >= so->so_snd.sb_lowat);
	}
	if (need_lock)
		SOCKBUF_UNLOCK(&so->so_snd);
	return (result);
}

/* ARGSUSED */
static int
fifo_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct file filetmp;
	int events, revents = 0;

	events = ap->a_events &
	    (POLLIN | POLLINIGNEOF | POLLPRI | POLLRDNORM | POLLRDBAND);
	if (events) {
		/*
		 * If POLLIN or POLLRDNORM is requested and POLLINIGNEOF is
		 * not, then convert the first two to the last one.  This
		 * tells the socket poll function to ignore EOF so that we
		 * block if there is no writer (and no data).  Callers can
		 * set POLLINIGNEOF to get non-blocking behavior.
		 */
		if (events & (POLLIN | POLLRDNORM) &&
		    !(events & POLLINIGNEOF)) {
			events &= ~(POLLIN | POLLRDNORM);
			events |= POLLINIGNEOF;
		}

		filetmp.f_data = ap->a_vp->v_fifoinfo->fi_readsock;
		filetmp.f_cred = ap->a_cred;
		if (filetmp.f_data)
			revents |= soo_poll(&filetmp, events,
			    ap->a_td->td_ucred, ap->a_td);

		/* Reverse the above conversion. */
		if ((revents & POLLINIGNEOF) &&
		    !(ap->a_events & POLLINIGNEOF)) {
			revents |= (ap->a_events & (POLLIN | POLLRDNORM));
			revents &= ~POLLINIGNEOF;
		}
	}
	events = ap->a_events & (POLLOUT | POLLWRNORM | POLLWRBAND);
	if (events) {
		filetmp.f_data = ap->a_vp->v_fifoinfo->fi_writesock;
		filetmp.f_cred = ap->a_cred;
		if (filetmp.f_data) {
			revents |= soo_poll(&filetmp, events,
			    ap->a_td->td_ucred, ap->a_td);
		}
	}
	return (revents);
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
	struct thread *td = ap->a_td;
	struct fifoinfo *fip = vp->v_fifoinfo;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

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
	VOP_UNLOCK(vp, 0, td);
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
