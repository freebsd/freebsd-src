/*
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
 *	@(#)fifo_vnops.c	8.10 (Berkeley) 5/27/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <miscfs/fifofs/fifo.h>

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

static int	fifo_badop __P((void));
static int	fifo_print __P((struct vop_print_args *));
static int	fifo_lookup __P((struct vop_lookup_args *));
static int	fifo_open __P((struct vop_open_args *));
static int	fifo_close __P((struct vop_close_args *));
static int	fifo_read __P((struct vop_read_args *));
static int	fifo_write __P((struct vop_write_args *));
static int	fifo_ioctl __P((struct vop_ioctl_args *));
static int	fifo_poll __P((struct vop_poll_args *));
static int	fifo_kqfilter __P((struct vop_kqfilter_args *));
static int	fifo_pathconf __P((struct vop_pathconf_args *));
static int	fifo_advlock __P((struct vop_advlock_args *));

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
	{ &vop_create_desc,		(vop_t *) fifo_badop },
	{ &vop_getattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_getwritemount_desc, 	(vop_t *) vop_stdgetwritemount },
	{ &vop_ioctl_desc,		(vop_t *) fifo_ioctl },
	{ &vop_lease_desc,		(vop_t *) vop_null },
	{ &vop_link_desc,		(vop_t *) fifo_badop },
	{ &vop_lookup_desc,		(vop_t *) fifo_lookup },
	{ &vop_mkdir_desc,		(vop_t *) fifo_badop },
	{ &vop_mknod_desc,		(vop_t *) fifo_badop },
	{ &vop_open_desc,		(vop_t *) fifo_open },
	{ &vop_pathconf_desc,		(vop_t *) fifo_pathconf },
	{ &vop_poll_desc,		(vop_t *) fifo_poll },
	{ &vop_kqfilter_desc,		(vop_t *) fifo_kqfilter },
	{ &vop_print_desc,		(vop_t *) fifo_print },
	{ &vop_read_desc,		(vop_t *) fifo_read },
	{ &vop_readdir_desc,		(vop_t *) fifo_badop },
	{ &vop_readlink_desc,		(vop_t *) fifo_badop },
	{ &vop_reallocblks_desc,	(vop_t *) fifo_badop },
	{ &vop_reclaim_desc,		(vop_t *) vop_null },
	{ &vop_remove_desc,		(vop_t *) fifo_badop },
	{ &vop_rename_desc,		(vop_t *) fifo_badop },
	{ &vop_rmdir_desc,		(vop_t *) fifo_badop },
	{ &vop_setattr_desc,		(vop_t *) vop_ebadf },
	{ &vop_symlink_desc,		(vop_t *) fifo_badop },
	{ &vop_write_desc,		(vop_t *) fifo_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc fifo_vnodeop_opv_desc =
	{ &fifo_vnodeop_p, fifo_vnodeop_entries };

VNODEOP_SET(fifo_vnodeop_opv_desc);

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
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct fifoinfo *fip;
	struct proc *p = ap->a_p;
	struct socket *rso, *wso;
	int error;

	if ((fip = vp->v_fifoinfo) == NULL) {
		MALLOC(fip, struct fifoinfo *, sizeof(*fip), M_VNODE, M_WAITOK);
		vp->v_fifoinfo = fip;
		error = socreate(AF_LOCAL, &rso, SOCK_STREAM, 0, ap->a_p);
		if (error) {
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		fip->fi_readsock = rso;
		error = socreate(AF_LOCAL, &wso, SOCK_STREAM, 0, ap->a_p);
		if (error) {
			(void)soclose(rso);
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		fip->fi_writesock = wso;
		error = unp_connect2(wso, rso);
		if (error) {
			(void)soclose(wso);
			(void)soclose(rso);
			free(fip, M_VNODE);
			vp->v_fifoinfo = NULL;
			return (error);
		}
		fip->fi_readers = fip->fi_writers = 0;
		wso->so_snd.sb_lowat = PIPE_BUF;
		rso->so_state |= SS_CANTRCVMORE;
	}
	if (ap->a_mode & FREAD) {
		fip->fi_readers++;
		if (fip->fi_readers == 1) {
			fip->fi_writesock->so_state &= ~SS_CANTSENDMORE;
			if (fip->fi_writers > 0)
				wakeup((caddr_t)&fip->fi_writers);
		}
	}
	if (ap->a_mode & FWRITE) {
		fip->fi_writers++;
		if (fip->fi_writers == 1) {
			fip->fi_readsock->so_state &= ~SS_CANTRCVMORE;
			if (fip->fi_readers > 0)
				wakeup((caddr_t)&fip->fi_readers);
		}
	}
	if ((ap->a_mode & FREAD) && (ap->a_mode & O_NONBLOCK) == 0) {
		while (fip->fi_writers == 0) {
			VOP_UNLOCK(vp, 0, p);
			error = tsleep((caddr_t)&fip->fi_readers,
			    PCATCH | PSOCK, "fifoor", 0);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
			if (error)
				goto bad;
		}
	}
	if (ap->a_mode & FWRITE) {
		if (ap->a_mode & O_NONBLOCK) {
			if (fip->fi_readers == 0) {
				error = ENXIO;
				goto bad;
			}
		} else {
			while (fip->fi_readers == 0) {
				VOP_UNLOCK(vp, 0, p);
				error = tsleep((caddr_t)&fip->fi_writers,
				    PCATCH | PSOCK, "fifoow", 0);
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
				if (error)
					goto bad;
			}
		}
	}
	return (0);
bad:
	VOP_CLOSE(vp, ap->a_mode, ap->a_cred, p);
	return (error);
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
	struct proc *p = uio->uio_procp;
	int error, startresid;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("fifo_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (ap->a_ioflag & IO_NDELAY)
		rso->so_state |= SS_NBIO;
	startresid = uio->uio_resid;
	VOP_UNLOCK(ap->a_vp, 0, p);
	error = soreceive(rso, (struct sockaddr **)0, uio, (struct mbuf **)0,
	    (struct mbuf **)0, (int *)0);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (ap->a_ioflag & IO_NDELAY)
		rso->so_state &= ~SS_NBIO;
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
	struct proc *p = ap->a_uio->uio_procp;
	int error;

#ifdef DIAGNOSTIC
	if (ap->a_uio->uio_rw != UIO_WRITE)
		panic("fifo_write mode");
#endif
	if (ap->a_ioflag & IO_NDELAY)
		wso->so_state |= SS_NBIO;
	VOP_UNLOCK(ap->a_vp, 0, p);
	error = sosend(wso, (struct sockaddr *)0, ap->a_uio, 0,
		       (struct mbuf *)0, 0, p);
	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (ap->a_ioflag & IO_NDELAY)
		wso->so_state &= ~SS_NBIO;
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
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct file filetmp;
	int error;

	if (ap->a_command == FIONBIO)
		return (0);
	if (ap->a_fflag & FREAD) {
		filetmp.f_data = (caddr_t)ap->a_vp->v_fifoinfo->fi_readsock;
		error = soo_ioctl(&filetmp, ap->a_command, ap->a_data, ap->a_p);
		if (error)
			return (error);
	}
	if (ap->a_fflag & FWRITE) {
		filetmp.f_data = (caddr_t)ap->a_vp->v_fifoinfo->fi_writesock;
		error = soo_ioctl(&filetmp, ap->a_command, ap->a_data, ap->a_p);
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
	struct socket *so = (struct socket *)ap->a_vp->v_fifoinfo->fi_readsock;
	struct sockbuf *sb;

	switch (ap->a_kn->kn_filter) {
	case EVFILT_READ:
		ap->a_kn->kn_fop = &fiforead_filtops;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		ap->a_kn->kn_fop = &fifowrite_filtops;
		sb = &so->so_snd;
		break;
	default:
		return (1);
	}

	ap->a_kn->kn_hook = (caddr_t)so;

	SLIST_INSERT_HEAD(&sb->sb_sel.si_note, ap->a_kn, kn_selnext);
	sb->sb_flags |= SB_KNOTE;

	return (0);
}

static void
filt_fifordetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SLIST_REMOVE(&so->so_rcv.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.si_note))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
}

static int
filt_fiforead(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	kn->kn_data = so->so_rcv.sb_cc;
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	kn->kn_flags &= ~EV_EOF;
	return (kn->kn_data > 0);
}

static void
filt_fifowdetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	SLIST_REMOVE(&so->so_snd.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.si_note))
		so->so_snd.sb_flags &= ~SB_KNOTE;
}

static int
filt_fifowrite(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_hook;

	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	kn->kn_flags &= ~EV_EOF;
	return (kn->kn_data >= so->so_snd.sb_lowat);
}

/* ARGSUSED */
static int
fifo_poll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct file filetmp;
	int revents = 0;

	if (ap->a_events & (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)) {
		filetmp.f_data = (caddr_t)ap->a_vp->v_fifoinfo->fi_readsock;
		if (filetmp.f_data)
			revents |= soo_poll(&filetmp, ap->a_events, ap->a_cred,
			    ap->a_p);
	}
	if (ap->a_events & (POLLOUT | POLLWRNORM | POLLWRBAND)) {
		filetmp.f_data = (caddr_t)ap->a_vp->v_fifoinfo->fi_writesock;
		if (filetmp.f_data)
			revents |= soo_poll(&filetmp, ap->a_events, ap->a_cred,
			    ap->a_p);
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
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct fifoinfo *fip = vp->v_fifoinfo;
	int error1, error2;

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
	if (vp->v_usecount > 1)
		return (0);
	error1 = soclose(fip->fi_readsock);
	error2 = soclose(fip->fi_writesock);
	FREE(fip, M_VNODE);
	vp->v_fifoinfo = NULL;
	if (error1)
		return (error1);
	return (error2);
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

	printf("tag VT_NON");
	fifo_printinfo(ap->a_vp);
	printf("\n");
	return (0);
}

/*
 * Return POSIX pathconf information applicable to fifo's.
 */
int
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

/*
 * Fifo bad operation
 */
static int
fifo_badop()
{

	panic("fifo_badop called");
	/* NOTREACHED */
}
