/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)portal_vnops.c	8.14 (Berkeley) 5/21/95
 *
 * $FreeBSD$
 */

/*
 * Portal Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <fs/portalfs/portal.h>

static int portal_fileid = PORTAL_ROOTFILEID+1;

static void	portal_closefd(struct thread *td, int fd);
static int	portal_connect(struct socket *so, struct socket *so2);
static int	portal_getattr(struct vop_getattr_args *ap);
static int	portal_lookup(struct vop_lookup_args *ap);
static int	portal_open(struct vop_open_args *ap);
static int	portal_readdir(struct vop_readdir_args *ap);
static int	portal_reclaim(struct vop_reclaim_args *ap);
static int	portal_setattr(struct vop_setattr_args *ap);

static void
portal_closefd(td, fd)
	struct thread *td;
	int fd;
{
	int error;
	struct close_args ua;

	ua.fd = fd;
	error = close(td, &ua);
	/*
	 * We should never get an error, and there isn't anything
	 * we could do if we got one, so just print a message.
	 */
	if (error)
		printf("portal_closefd: error = %d\n", error);
}

/*
 * vp is the current namei directory
 * cnp is the name to locate in that directory...
 */
static int
portal_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct thread *td = cnp->cn_thread;
	struct portalnode *pt;
	int error;
	struct vnode *fvp = 0;
	char *path;
	int size;

	*vpp = NULLVP;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(pt, struct portalnode *, sizeof(struct portalnode),
		M_TEMP, M_WAITOK);

	error = getnewvnode("portal", dvp->v_mount, portal_vnodeop_p, &fvp);
	if (error) {
		FREE(pt, M_TEMP);
		goto bad;
	}
	fvp->v_type = VREG;
	fvp->v_data = pt;
	/*
	 * Save all of the remaining pathname and
	 * advance the namei next pointer to the end
	 * of the string.
	 */
	for (size = 0, path = pname; *path; path++)
		size++;
	cnp->cn_consume = size - cnp->cn_namelen;

	pt->pt_arg = malloc(size+1, M_TEMP, M_WAITOK);
	pt->pt_size = size+1;
	bcopy(pname, pt->pt_arg, pt->pt_size);
	pt->pt_fileid = portal_fileid++;

	*vpp = fvp;
	vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY, td);
	/*
	 * As we are the last component of the path name, fix up
	 * the locking on the directory node.
	 */
	if ((cnp->cn_flags & LOCKPARENT) == 0) {
		VOP_UNLOCK(dvp, 0, td);
		cnp->cn_flags |= PDIRUNLOCK;
	}
	return (0);

bad:;
	if (fvp)
		vrele(fvp);
	return (error);
}

static int
portal_connect(so, so2)
	struct socket *so;
	struct socket *so2;
{
	/* from unp_connect, bypassing the namei stuff... */
	struct socket *so3;
	struct unpcb *unp2;
	struct unpcb *unp3;

	if (so2 == 0)
		return (ECONNREFUSED);

	if (so->so_type != so2->so_type)
		return (EPROTOTYPE);

	if ((so2->so_options & SO_ACCEPTCONN) == 0)
		return (ECONNREFUSED);

	if ((so3 = sonewconn(so2, 0)) == 0)
		return (ECONNREFUSED);

	unp2 = sotounpcb(so2);
	unp3 = sotounpcb(so3);
	if (unp2->unp_addr)
		unp3->unp_addr = (struct sockaddr_un *)
			dup_sockaddr((struct sockaddr *)unp2->unp_addr, 0);
	so2 = so3;

	return (unp_connect2(so, so2));
}

static int
portal_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct socket *so = 0;
	struct portalnode *pt;
	struct thread *td = ap->a_td;
	struct vnode *vp = ap->a_vp;
	int s;
	struct uio auio;
	struct iovec aiov[2];
	int res;
	struct mbuf *cm = 0;
	struct cmsghdr *cmsg;
	int newfds;
	int *ip;
	int fd;
	int error;
	int len;
	struct portalmount *fmp;
	struct file *fp;
	struct portal_cred pcred;

	/*
	 * Nothing to do when opening the root node.
	 */
	if (vp->v_vflag & VV_ROOT)
		return (0);

	/*
	 * Can't be opened unless the caller is set up
	 * to deal with the side effects.  Check for this
	 * by testing whether the p_dupfd has been set.
	 */
	if (td->td_dupfd >= 0)
		return (ENODEV);

	pt = VTOPORTAL(vp);
	fmp = VFSTOPORTAL(vp->v_mount);

	/*
	 * Create a new socket.
	 */
	error = socreate(AF_UNIX, &so, SOCK_STREAM, 0, ap->a_td->td_ucred,
	    ap->a_td);
	if (error)
		goto bad;

	/*
	 * Reserve some buffer space
	 */
	res = pt->pt_size + sizeof(pcred) + 512;	/* XXX */
	error = soreserve(so, res, res);
	if (error)
		goto bad;

	/*
	 * Kick off connection
	 */
	error = portal_connect(so, fmp->pm_server->f_data);
	if (error)
		goto bad;

	/*
	 * Wait for connection to complete
	 */
	/*
	 * XXX: Since the mount point is holding a reference on the
	 * underlying server socket, it is not easy to find out whether
	 * the server process is still running.  To handle this problem
	 * we loop waiting for the new socket to be connected (something
	 * which will only happen if the server is still running) or for
	 * the reference count on the server socket to drop to 1, which
	 * will happen if the server dies.  Sleep for 5 second intervals
	 * and keep polling the reference count.   XXX.
	 */
	s = splnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		if (fmp->pm_server->f_count == 1) {
			error = ECONNREFUSED;
			splx(s);
			goto bad;
		}
		(void) tsleep((caddr_t) &so->so_timeo, PSOCK, "portalcon", 5 * hz);
	}
	splx(s);

	if (so->so_error) {
		error = so->so_error;
		goto bad;
	}

	/*
	 * Set miscellaneous flags
	 */
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_timeo = 0;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;


	pcred.pcr_flag = ap->a_mode;
	pcred.pcr_uid = ap->a_cred->cr_uid;
	pcred.pcr_ngroups = ap->a_cred->cr_ngroups;
	bcopy(ap->a_cred->cr_groups, pcred.pcr_groups, NGROUPS * sizeof(gid_t));
	aiov[0].iov_base = (caddr_t) &pcred;
	aiov[0].iov_len = sizeof(pcred);
	aiov[1].iov_base = pt->pt_arg;
	aiov[1].iov_len = pt->pt_size;
	auio.uio_iov = aiov;
	auio.uio_iovcnt = 2;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	auio.uio_offset = 0;
	auio.uio_resid = aiov[0].iov_len + aiov[1].iov_len;

	error = sosend(so, (struct sockaddr *) 0, &auio,
			(struct mbuf *) 0, (struct mbuf *) 0, 0 , td);
	if (error)
		goto bad;

	len = auio.uio_resid = sizeof(int);
	do {
		struct mbuf *m = 0;
		int flags = MSG_WAITALL;
		error = soreceive(so, (struct sockaddr **) 0, &auio,
					&m, &cm, &flags);
		if (error)
			goto bad;

		/*
		 * Grab an error code from the mbuf.
		 */
		if (m) {
			m = m_pullup(m, sizeof(int));	/* Needed? */
			if (m) {
				error = *(mtod(m, int *));
				m_freem(m);
			} else {
				error = EINVAL;
			}
		} else {
			if (cm == 0) {
				error = ECONNRESET;	 /* XXX */
#ifdef notdef
				break;
#endif
			}
		}
	} while (cm == 0 && auio.uio_resid == len && !error);

	if (cm == 0)
		goto bad;

	if (auio.uio_resid) {
		error = 0;
#ifdef notdef
		error = EMSGSIZE;
		goto bad;
#endif
	}

	/*
	 * XXX: Break apart the control message, and retrieve the
	 * received file descriptor.  Note that more than one descriptor
	 * may have been received, or that the rights chain may have more
	 * than a single mbuf in it.  What to do?
	 */
	cmsg = mtod(cm, struct cmsghdr *);
	newfds = (cmsg->cmsg_len - sizeof(*cmsg)) / sizeof (int);
	if (newfds == 0) {
		error = ECONNREFUSED;
		goto bad;
	}
	/*
	 * At this point the rights message consists of a control message
	 * header, followed by a data region containing a vector of
	 * integer file descriptors.  The fds were allocated by the action
	 * of receiving the control message.
	 */
	ip = (int *) (cmsg + 1);
	fd = *ip++;
	if (newfds > 1) {
		/*
		 * Close extra fds.
		 */
		int i;
		printf("portal_open: %d extra fds\n", newfds - 1);
		for (i = 1; i < newfds; i++) {
			portal_closefd(td, *ip);
			ip++;
		}
	}

	/*
	 * Check that the mode the file is being opened for is a subset
	 * of the mode of the existing descriptor.
	 */
	if ((error = fget(td, fd, &fp)) != 0)
		goto bad;
	if (((ap->a_mode & (FREAD|FWRITE)) | fp->f_flag) != fp->f_flag) {
		fdrop(fp, td);
		portal_closefd(td, fd);
		error = EACCES;
		goto bad;
	}
	fdrop(fp, td);

	/*
	 * Save the dup fd in the proc structure then return the
	 * special error code (ENXIO) which causes magic things to
	 * happen in vn_open.  The whole concept is, well, hmmm.
	 */
	td->td_dupfd = fd;
	error = ENXIO;

bad:;
	/*
	 * And discard the control message.
	 */
	if (cm) {
		m_freem(cm);
	}

	if (so) {
		soshutdown(so, 2);
		soclose(so);
	}
	return (error);
}

static int
portal_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_size = DEV_BSIZE;
	vap->va_blocksize = DEV_BSIZE;
	nanotime(&vap->va_atime);
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_mtime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	/* vap->va_qbytes = 0; */
	vap->va_bytes = 0;
	/* vap->va_qsize = 0; */
	if (vp->v_vflag & VV_ROOT) {
		vap->va_type = VDIR;
		vap->va_mode = S_IRUSR|S_IWUSR|S_IXUSR|
				S_IRGRP|S_IWGRP|S_IXGRP|
				S_IROTH|S_IWOTH|S_IXOTH;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
	} else {
		vap->va_type = VREG;
		vap->va_mode = S_IRUSR|S_IWUSR|
				S_IRGRP|S_IWGRP|
				S_IROTH|S_IWOTH;
		vap->va_nlink = 1;
		vap->va_fileid = VTOPORTAL(vp)->pt_fileid;
	}
	return (0);
}

static int
portal_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	/*
	 * Can't mess with the root vnode
	 */
	if (ap->a_vp->v_vflag & VV_ROOT)
		return (EACCES);

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	return (0);
}

/*
 * Fake readdir, just return empty directory.
 * It is hard to deal with '.' and '..' so don't bother.
 */
static int
portal_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{

	/*
	 * We don't allow exporting portal mounts, and currently local
	 * requests do not need cookies.
	 */
	if (ap->a_ncookies)
		panic("portal_readdir: not hungry");

	return (0);
}

static int
portal_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct portalnode *pt = VTOPORTAL(ap->a_vp);

	if (pt->pt_arg) {
		free((caddr_t) pt->pt_arg, M_TEMP);
		pt->pt_arg = 0;
	}
	FREE(ap->a_vp->v_data, M_TEMP);
	ap->a_vp->v_data = 0;

	return (0);
}

vop_t **portal_vnodeop_p;
static struct vnodeopv_entry_desc portal_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) vop_null },
	{ &vop_getattr_desc,		(vop_t *) portal_getattr },
	{ &vop_lookup_desc,		(vop_t *) portal_lookup },
	{ &vop_open_desc,		(vop_t *) portal_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_print_desc,		(vop_t *) vop_null },
	{ &vop_readdir_desc,		(vop_t *) portal_readdir },
	{ &vop_reclaim_desc,		(vop_t *) portal_reclaim },
	{ &vop_setattr_desc,		(vop_t *) portal_setattr },
	{ NULL, NULL }
};
static struct vnodeopv_desc portal_vnodeop_opv_desc =
	{ &portal_vnodeop_p, portal_vnodeop_entries };

VNODEOP_SET(portal_vnodeop_opv_desc);
