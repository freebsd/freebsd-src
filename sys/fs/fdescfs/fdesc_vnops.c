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
 *	@(#)fdesc_vnops.c	8.9 (Berkeley) 1/21/94
 *
 * $Id: fdesc_vnops.c,v 1.17 1996/06/12 05:09:52 gpalmer Exp $
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>	/* boottime */
#include <sys/resourcevar.h>
#include <sys/filedesc.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/socketvar.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <miscfs/fdesc/fdesc.h>

extern	struct cdevsw ctty_cdevsw;

#define cttyvp(p) ((p)->p_flag & P_CONTROLT ? (p)->p_session->s_ttyvp : NULL)

#define FDL_WANT	0x01
#define FDL_LOCKED	0x02
static int fdcache_lock;

dev_t devctty;

#if (FD_STDIN != FD_STDOUT-1) || (FD_STDOUT != FD_STDERR-1)
FD_STDIN, FD_STDOUT, FD_STDERR must be a sequence n, n+1, n+2
#endif

#define	NFDCACHE 4
#define	FD_NHASH(ix) ((ix) & NFDCACHE-1)

/*
 * Cache head
 */
struct fdcache {
	struct fdescnode	*fc_forw;
	struct fdescnode	*fc_back;
};

static struct fdcache fdcache[NFDCACHE];

static int	fdesc_attr __P((int fd, struct vattr *vap, struct ucred *cred,
				struct proc *p));
static int	fdesc_badop __P((void));
static int	fdesc_enotsupp __P((void));
static int	fdesc_getattr __P((struct vop_getattr_args *ap));
static struct fdcache *
		fdesc_hash __P((int ix));
static int	fdesc_inactive __P((struct vop_inactive_args *ap));
static int	fdesc_ioctl __P((struct vop_ioctl_args *ap));
static int	fdesc_lookup __P((struct vop_lookup_args *ap));
static int	fdesc_open __P((struct vop_open_args *ap));
static int	fdesc_pathconf __P((struct vop_pathconf_args *ap));
static int	fdesc_print __P((struct vop_print_args *ap));
static int	fdesc_read __P((struct vop_read_args *ap));
static int	fdesc_readdir __P((struct vop_readdir_args *ap));
static int	fdesc_readlink __P((struct vop_readlink_args *ap));
static int	fdesc_reclaim __P((struct vop_reclaim_args *ap));
static int	fdesc_select __P((struct vop_select_args *ap));
static int	fdesc_setattr __P((struct vop_setattr_args *ap));
static int	fdesc_vfree __P((struct vop_vfree_args *ap));
static int	fdesc_write __P((struct vop_write_args *ap));

/*
 * Initialise cache headers
 */
int
fdesc_init()
{
	struct fdcache *fc;

	devctty = makedev(nchrdev, 0);

	for (fc = fdcache; fc < fdcache + NFDCACHE; fc++)
		fc->fc_forw = fc->fc_back = (struct fdescnode *) fc;
	return (0);
}

/*
 * Compute hash list for given target vnode
 */
static struct fdcache *
fdesc_hash(ix)
	int ix;
{

	return (&fdcache[FD_NHASH(ix)]);
}

int
fdesc_allocvp(ftype, ix, mp, vpp)
	fdntype ftype;
	int ix;
	struct mount *mp;
	struct vnode **vpp;
{
	struct fdcache *fc;
	struct fdescnode *fd;
	int error = 0;

loop:
	fc = fdesc_hash(ix);
	for (fd = fc->fc_forw; fd != (struct fdescnode *) fc; fd = fd->fd_forw) {
		if (fd->fd_ix == ix && fd->fd_vnode->v_mount == mp) {
			if (vget(fd->fd_vnode, 0))
				goto loop;
			*vpp = fd->fd_vnode;
			return (error);
		}
	}

	/*
	 * otherwise lock the array while we call getnewvnode
	 * since that can block.
	 */
	if (fdcache_lock & FDL_LOCKED) {
		fdcache_lock |= FDL_WANT;
		(void) tsleep((caddr_t) &fdcache_lock, PINOD, "fdalvp", 0);
		goto loop;
	}
	fdcache_lock |= FDL_LOCKED;

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(fd, struct fdescnode *, sizeof(struct fdescnode), M_TEMP, M_WAITOK);

	error = getnewvnode(VT_FDESC, mp, fdesc_vnodeop_p, vpp);
	if (error) {
		FREE(fd, M_TEMP);
		goto out;
	}
	(*vpp)->v_data = fd;
	fd->fd_vnode = *vpp;
	fd->fd_type = ftype;
	fd->fd_fd = -1;
	fd->fd_link = 0;
	fd->fd_ix = ix;
	fc = fdesc_hash(ix);
	insque(fd, fc);

out:;
	fdcache_lock &= ~FDL_LOCKED;

	if (fdcache_lock & FDL_WANT) {
		fdcache_lock &= ~FDL_WANT;
		wakeup((caddr_t) &fdcache_lock);
	}

	return (error);
}

/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
static int
fdesc_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname;
	struct proc *p;
	int nfiles;
	unsigned fd = 0;
	int error;
	struct vnode *fvp;
	char *ln;

	if (ap->a_cnp->cn_nameiop == DELETE || 
	    ap->a_cnp->cn_nameiop == RENAME) {
		error = EROFS;
		goto bad;
	}

	pname = ap->a_cnp->cn_nameptr;
	if (ap->a_cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		VOP_LOCK(dvp);
		return (0);
	}

	p = ap->a_cnp->cn_proc;
	nfiles = p->p_fd->fd_nfiles;

	switch (VTOFDESC(dvp)->fd_type) {
	default:
	case Flink:
	case Fdesc:
	case Fctty:
		error = ENOTDIR;
		goto bad;

	case Froot:
		if (ap->a_cnp->cn_namelen == 2 && bcmp(pname, "fd", 2) == 0) {
			error = fdesc_allocvp(Fdevfd, FD_DEVFD, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			*vpp = fvp;
			fvp->v_type = VDIR;
			VOP_LOCK(fvp);
			return (0);
		}

		if (ap->a_cnp->cn_namelen == 3 && bcmp(pname, "tty", 3) == 0) {
			struct vnode *ttyvp = cttyvp(p);
			if (ttyvp == NULL) {
				error = ENXIO;
				goto bad;
			}
			error = fdesc_allocvp(Fctty, FD_CTTY, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			*vpp = fvp;
			fvp->v_type = VFIFO;
			VOP_LOCK(fvp);
			return (0);
		}

		ln = 0;
		switch (ap->a_cnp->cn_namelen) {
		case 5:
			if (bcmp(pname, "stdin", 5) == 0) {
				ln = "fd/0";
				fd = FD_STDIN;
			}
			break;
		case 6:
			if (bcmp(pname, "stdout", 6) == 0) {
				ln = "fd/1";
				fd = FD_STDOUT;
			} else
			if (bcmp(pname, "stderr", 6) == 0) {
				ln = "fd/2";
				fd = FD_STDERR;
			}
			break;
		}

		if (ln) {
			error = fdesc_allocvp(Flink, fd, dvp->v_mount, &fvp);
			if (error)
				goto bad;
			VTOFDESC(fvp)->fd_link = ln;
			*vpp = fvp;
			fvp->v_type = VLNK;
			VOP_LOCK(fvp);
			return (0);
		} else {
			error = ENOENT;
			goto bad;
		}

		/* FALL THROUGH */

	case Fdevfd:
		if (ap->a_cnp->cn_namelen == 2 && bcmp(pname, "..", 2) == 0) {
			error = fdesc_root(dvp->v_mount, vpp);
			return (error);
		}

		fd = 0;
		while (*pname >= '0' && *pname <= '9') {
			fd = 10 * fd + *pname++ - '0';
			if (fd >= nfiles)
				break;
		}

		if (*pname != '\0') {
			error = ENOENT;
			goto bad;
		}

		if (fd >= nfiles || p->p_fd->fd_ofiles[fd] == NULL) {
			error = EBADF;
			goto bad;
		}

		error = fdesc_allocvp(Fdesc, FD_DESC+fd, dvp->v_mount, &fvp);
		if (error)
			goto bad;
		VTOFDESC(fvp)->fd_fd = fd;
		*vpp = fvp;
		return (0);
	}

bad:;
	*vpp = NULL;
	return (error);
}

static int
fdesc_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int error = 0;

	switch (VTOFDESC(vp)->fd_type) {
	case Fdesc:
		/*
		 * XXX Kludge: set p->p_dupfd to contain the value of the
		 * the file descriptor being sought for duplication. The error
		 * return ensures that the vnode for this device will be
		 * released by vn_open. Open will detect this special error and
		 * take the actions in dupfdopen.  Other callers of vn_open or
		 * VOP_OPEN will simply report the error.
		 */
		ap->a_p->p_dupfd = VTOFDESC(vp)->fd_fd;	/* XXX */
		error = ENODEV;
		break;

	case Fctty:
		error = (*ctty_cdevsw.d_open)(devctty, ap->a_mode, 0, ap->a_p);
		break;
	}

	return (error);
}

static int
fdesc_attr(fd, vap, cred, p)
	int fd;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct stat stb;
	int error;

	if (fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL)
		return (EBADF);

	switch (fp->f_type) {
	case DTYPE_VNODE:
		error = VOP_GETATTR((struct vnode *) fp->f_data, vap, cred, p);
		if (error == 0 && vap->va_type == VDIR) {
			/*
			 * don't allow directories to show up because
			 * that causes loops in the namespace.
			 */
			vap->va_type = VFIFO;
		}
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &stb);
		if (error == 0) {
			vattr_null(vap);
			vap->va_type = VSOCK;
			vap->va_mode = stb.st_mode;
			vap->va_nlink = stb.st_nlink;
			vap->va_uid = stb.st_uid;
			vap->va_gid = stb.st_gid;
			vap->va_fsid = stb.st_dev;
			vap->va_fileid = stb.st_ino;
			vap->va_size = stb.st_size;
			vap->va_blocksize = stb.st_blksize;
			vap->va_atime = stb.st_atimespec;
			vap->va_mtime = stb.st_mtimespec;
			vap->va_ctime = stb.st_ctimespec;
			vap->va_gen = stb.st_gen;
			vap->va_flags = stb.st_flags;
			vap->va_rdev = stb.st_rdev;
			vap->va_bytes = stb.st_blocks * stb.st_blksize;
		}
		break;

	default:
		panic("fdesc attr");
		break;
	}

	return (error);
}

static int
fdesc_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	unsigned fd;
	int error = 0;

	switch (VTOFDESC(vp)->fd_type) {
	case Froot:
	case Fdevfd:
	case Flink:
	case Fctty:
		bzero((caddr_t) vap, sizeof(*vap));
		vattr_null(vap);
		vap->va_fileid = VTOFDESC(vp)->fd_ix;

		switch (VTOFDESC(vp)->fd_type) {
		case Flink:
			vap->va_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
			vap->va_type = VLNK;
			vap->va_nlink = 1;
			vap->va_size = strlen(VTOFDESC(vp)->fd_link);
			break;

		case Fctty:
			vap->va_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
			vap->va_type = VFIFO;
			vap->va_nlink = 1;
			vap->va_size = 0;
			break;

		default:
			vap->va_mode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
			vap->va_type = VDIR;
			vap->va_nlink = 2;
			vap->va_size = DEV_BSIZE;
			break;
		}
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
		vap->va_blocksize = DEV_BSIZE;
		vap->va_atime.ts_sec = boottime.tv_sec;
		vap->va_atime.ts_nsec = 0;
		vap->va_mtime = vap->va_atime;
		vap->va_ctime = vap->va_mtime;
		vap->va_gen = 0;
		vap->va_flags = 0;
		vap->va_rdev = 0;
		vap->va_bytes = 0;
		break;

	case Fdesc:
		fd = VTOFDESC(vp)->fd_fd;
		error = fdesc_attr(fd, vap, ap->a_cred, ap->a_p);
		break;

	default:
		panic("fdesc_getattr");
		break;
	}

	if (error == 0)
		vp->v_type = vap->va_type;

	return (error);
}

static int
fdesc_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct filedesc *fdp = ap->a_p->p_fd;
	struct file *fp;
	unsigned fd;
	int error;

	/*
	 * Can't mess with the root vnode
	 */
	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fdesc:
		break;

	case Fctty:
		return (0);

	default:
		return (EACCES);
	}

	fd = VTOFDESC(ap->a_vp)->fd_fd;
	if (fd >= fdp->fd_nfiles || (fp = fdp->fd_ofiles[fd]) == NULL) {
		return (EBADF);
	}

	/*
	 * Can setattr the underlying vnode, but not sockets!
	 */
	switch (fp->f_type) {
	case DTYPE_VNODE:
		error = VOP_SETATTR((struct vnode *) fp->f_data, ap->a_vap, ap->a_cred, ap->a_p);
		break;

	case DTYPE_SOCKET:
		error = 0;
		break;

	default:
		error = EBADF;
		break;
	}

	return (error);
}

#define UIO_MX 16

static struct dirtmp {
	u_long d_fileno;
	u_short d_reclen;
	u_short d_namlen;
	char d_name[8];
} rootent[] = {
	{ FD_DEVFD, UIO_MX, 2, "fd" },
	{ FD_STDIN, UIO_MX, 5, "stdin" },
	{ FD_STDOUT, UIO_MX, 6, "stdout" },
	{ FD_STDERR, UIO_MX, 6, "stderr" },
	{ FD_CTTY, UIO_MX, 3, "tty" },
	{ 0 }
};

static int
fdesc_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct filedesc *fdp;
	int i;
	int error;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		return (0);

	case Fdesc:
		return (ENOTDIR);

	default:
		break;
	}

	fdp = uio->uio_procp->p_fd;

	if (VTOFDESC(ap->a_vp)->fd_type == Froot) {
		struct dirent d;
		struct dirent *dp = &d;
		struct dirtmp *dt;

		i = uio->uio_offset / UIO_MX;
		error = 0;

		while (uio->uio_resid > 0) {
			dt = &rootent[i];
			if (dt->d_fileno == 0) {
				/**eofflagp = 1;*/
				break;
			}
			i++;

			switch (dt->d_fileno) {
			case FD_CTTY:
				if (cttyvp(uio->uio_procp) == NULL)
					continue;
				break;

			case FD_STDIN:
			case FD_STDOUT:
			case FD_STDERR:
				if ((dt->d_fileno-FD_STDIN) >= fdp->fd_nfiles)
					continue;
				if (fdp->fd_ofiles[dt->d_fileno-FD_STDIN] == NULL)
					continue;
				break;
			}
			bzero((caddr_t) dp, UIO_MX);
			dp->d_fileno = dt->d_fileno;
			dp->d_namlen = dt->d_namlen;
			dp->d_type = DT_UNKNOWN;
			dp->d_reclen = dt->d_reclen;
			bcopy(dt->d_name, dp->d_name, dp->d_namlen+1);
			error = uiomove((caddr_t) dp, UIO_MX, uio);
			if (error)
				break;
		}
		uio->uio_offset = i * UIO_MX;
		return (error);
	}

	i = uio->uio_offset / UIO_MX;
	error = 0;
	while (uio->uio_resid > 0) {
		if (i >= fdp->fd_nfiles)
			break;

		if (fdp->fd_ofiles[i] != NULL) {
			struct dirent d;
			struct dirent *dp = &d;

			bzero((caddr_t) dp, UIO_MX);

			dp->d_namlen = sprintf(dp->d_name, "%d", i);
			dp->d_reclen = UIO_MX;
			dp->d_type = DT_UNKNOWN;
			dp->d_fileno = i + FD_STDIN;
			/*
			 * And ship to userland
			 */
			error = uiomove((caddr_t) dp, UIO_MX, uio);
			if (error)
				break;
		}
		i++;
	}

	uio->uio_offset = i * UIO_MX;
	return (error);
}

static int
fdesc_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int error;

	if (vp->v_type != VLNK)
		return (EPERM);

	if (VTOFDESC(vp)->fd_type == Flink) {
		char *ln = VTOFDESC(vp)->fd_link;
		error = uiomove(ln, strlen(ln), ap->a_uio);
	} else {
		error = EOPNOTSUPP;
	}

	return (error);
}

static int
fdesc_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = (*ctty_cdevsw.d_read)(devctty, ap->a_uio, ap->a_ioflag);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
fdesc_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = (*ctty_cdevsw.d_write)(devctty, ap->a_uio, ap->a_ioflag);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
fdesc_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = (*ctty_cdevsw.d_ioctl)(devctty, ap->a_command,
					ap->a_data, ap->a_fflag, ap->a_p);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
fdesc_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error = EOPNOTSUPP;

	switch (VTOFDESC(ap->a_vp)->fd_type) {
	case Fctty:
		error = (*ctty_cdevsw.d_select)(devctty, ap->a_fflags, ap->a_p);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
fdesc_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Clear out the v_type field to avoid
	 * nasty things happening in vgone().
	 */
	vp->v_type = VNON;
	return (0);
}

static int
fdesc_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	remque(VTOFDESC(vp));
	FREE(vp->v_data, M_TEMP);
	vp->v_data = 0;

	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
static int
fdesc_pathconf(ap)
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
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Print out the contents of a /dev/fd vnode.
 */
/* ARGSUSED */
static int
fdesc_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_NON, fdesc vnode\n");
	return (0);
}

/*void*/
static int
fdesc_vfree(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{

	return (0);
}

/*
 * /dev/fd vnode unsupported operation
 */
static int
fdesc_enotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * /dev/fd "should never get here" operation
 */
static int
fdesc_badop()
{

	panic("fdesc: bad op");
	/* NOTREACHED */
}

#define fdesc_create ((int (*) __P((struct  vop_create_args *)))fdesc_enotsupp)
#define fdesc_mknod ((int (*) __P((struct  vop_mknod_args *)))fdesc_enotsupp)
#define fdesc_close ((int (*) __P((struct  vop_close_args *)))nullop)
#define fdesc_access ((int (*) __P((struct  vop_access_args *)))nullop)
#define fdesc_mmap ((int (*) __P((struct  vop_mmap_args *)))fdesc_enotsupp)
#define fdesc_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define fdesc_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
#define fdesc_remove ((int (*) __P((struct  vop_remove_args *)))fdesc_enotsupp)
#define fdesc_link ((int (*) __P((struct  vop_link_args *)))fdesc_enotsupp)
#define fdesc_rename ((int (*) __P((struct  vop_rename_args *)))fdesc_enotsupp)
#define fdesc_mkdir ((int (*) __P((struct  vop_mkdir_args *)))fdesc_enotsupp)
#define fdesc_rmdir ((int (*) __P((struct  vop_rmdir_args *)))fdesc_enotsupp)
#define fdesc_symlink ((int (*) __P((struct vop_symlink_args *)))fdesc_enotsupp)
#define fdesc_abortop ((int (*) __P((struct  vop_abortop_args *)))nullop)
#define fdesc_lock ((int (*) __P((struct  vop_lock_args *)))nullop)
#define fdesc_unlock ((int (*) __P((struct  vop_unlock_args *)))nullop)
#define fdesc_bmap ((int (*) __P((struct  vop_bmap_args *)))fdesc_badop)
#define fdesc_strategy ((int (*) __P((struct  vop_strategy_args *)))fdesc_badop)
#define fdesc_islocked ((int (*) __P((struct  vop_islocked_args *)))nullop)
#define fdesc_advlock ((int (*) __P((struct vop_advlock_args *)))fdesc_enotsupp)
#define fdesc_blkatoff \
	((int (*) __P((struct  vop_blkatoff_args *)))fdesc_enotsupp)
#define fdesc_vget ((int (*) __P((struct  vop_vget_args *)))fdesc_enotsupp)
#define fdesc_valloc ((int(*) __P(( \
		struct vnode *pvp, \
		int mode, \
		struct ucred *cred, \
		struct vnode **vpp))) fdesc_enotsupp)
#define fdesc_truncate \
	((int (*) __P((struct  vop_truncate_args *)))fdesc_enotsupp)
#define fdesc_update ((int (*) __P((struct  vop_update_args *)))fdesc_enotsupp)
#define fdesc_bwrite ((int (*) __P((struct  vop_bwrite_args *)))fdesc_enotsupp)

static vop_t **fdesc_vnodeop_p;
static struct vnodeopv_entry_desc fdesc_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)fdesc_lookup },		/* lookup */
	{ &vop_create_desc, (vop_t *)fdesc_create },		/* create */
	{ &vop_mknod_desc, (vop_t *)fdesc_mknod },		/* mknod */
	{ &vop_open_desc, (vop_t *)fdesc_open },		/* open */
	{ &vop_close_desc, (vop_t *)fdesc_close },		/* close */
	{ &vop_access_desc, (vop_t *)fdesc_access },		/* access */
	{ &vop_getattr_desc, (vop_t *)fdesc_getattr },		/* getattr */
	{ &vop_setattr_desc, (vop_t *)fdesc_setattr },		/* setattr */
	{ &vop_read_desc, (vop_t *)fdesc_read },		/* read */
	{ &vop_write_desc, (vop_t *)fdesc_write },		/* write */
	{ &vop_ioctl_desc, (vop_t *)fdesc_ioctl },		/* ioctl */
	{ &vop_select_desc, (vop_t *)fdesc_select },		/* select */
	{ &vop_mmap_desc, (vop_t *)fdesc_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)fdesc_fsync },		/* fsync */
	{ &vop_seek_desc, (vop_t *)fdesc_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)fdesc_remove },		/* remove */
	{ &vop_link_desc, (vop_t *)fdesc_link },		/* link */
	{ &vop_rename_desc, (vop_t *)fdesc_rename },		/* rename */
	{ &vop_mkdir_desc, (vop_t *)fdesc_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)fdesc_rmdir },		/* rmdir */
	{ &vop_symlink_desc, (vop_t *)fdesc_symlink },		/* symlink */
	{ &vop_readdir_desc, (vop_t *)fdesc_readdir },		/* readdir */
	{ &vop_readlink_desc, (vop_t *)fdesc_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)fdesc_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *)fdesc_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)fdesc_reclaim },		/* reclaim */
	{ &vop_lock_desc, (vop_t *)fdesc_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)fdesc_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *)fdesc_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)fdesc_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)fdesc_print },		/* print */
	{ &vop_islocked_desc, (vop_t *)fdesc_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)fdesc_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)fdesc_advlock },		/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)fdesc_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)fdesc_valloc },		/* valloc */
	{ &vop_vfree_desc, (vop_t *)fdesc_vfree },		/* vfree */
	{ &vop_truncate_desc, (vop_t *)fdesc_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)fdesc_update },		/* update */
	{ &vop_bwrite_desc, (vop_t *)fdesc_bwrite },		/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc fdesc_vnodeop_opv_desc =
	{ &fdesc_vnodeop_p, fdesc_vnodeop_entries };

VNODEOP_SET(fdesc_vnodeop_opv_desc);
