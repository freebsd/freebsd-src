/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/vnode.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_util.h>

#ifndef __alpha__
int
linux_creat(struct thread *td, struct linux_creat_args *args)
{
    struct open_args /* {
	char *path;
	int flags;
	int mode;
    } */ bsd_open_args;
    caddr_t sg;

    sg = stackgap_init();
    CHECKALTCREAT(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(creat))
		printf(ARGS(creat, "%s, %d"), args->path, args->mode);
#endif
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;
    bsd_open_args.flags = O_WRONLY | O_CREAT | O_TRUNC;
    return open(td, &bsd_open_args);
}
#endif /*!__alpha__*/

int
linux_open(struct thread *td, struct linux_open_args *args)
{
    struct open_args /* {
	char *path;
	int flags;
	int mode;
    } */ bsd_open_args;
    struct proc *p = td->td_proc;
    int error;
    caddr_t sg;

    sg = stackgap_init();
    
    if (args->flags & LINUX_O_CREAT)
	CHECKALTCREAT(td, &sg, args->path);
    else
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(open))
		printf(ARGS(open, "%s, 0x%x, 0x%x"),
		    args->path, args->flags, args->mode);
#endif
    bsd_open_args.flags = 0;
    if (args->flags & LINUX_O_RDONLY)
	bsd_open_args.flags |= O_RDONLY;
    if (args->flags & LINUX_O_WRONLY) 
	bsd_open_args.flags |= O_WRONLY;
    if (args->flags & LINUX_O_RDWR)
	bsd_open_args.flags |= O_RDWR;
    if (args->flags & LINUX_O_NDELAY)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_O_APPEND)
	bsd_open_args.flags |= O_APPEND;
    if (args->flags & LINUX_O_SYNC)
	bsd_open_args.flags |= O_FSYNC;
    if (args->flags & LINUX_O_NONBLOCK)
	bsd_open_args.flags |= O_NONBLOCK;
    if (args->flags & LINUX_FASYNC)
	bsd_open_args.flags |= O_ASYNC;
    if (args->flags & LINUX_O_CREAT)
	bsd_open_args.flags |= O_CREAT;
    if (args->flags & LINUX_O_TRUNC)
	bsd_open_args.flags |= O_TRUNC;
    if (args->flags & LINUX_O_EXCL)
	bsd_open_args.flags |= O_EXCL;
    if (args->flags & LINUX_O_NOCTTY)
	bsd_open_args.flags |= O_NOCTTY;
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;

    error = open(td, &bsd_open_args);
    PROC_LOCK(p);
    if (!error && !(bsd_open_args.flags & O_NOCTTY) && 
	SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
	struct file *fp;

	fp = ffind_hold(td, td->td_retval[0]);
	PROC_UNLOCK(p);
	if (fp->f_type == DTYPE_VNODE)
	    fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0, td);
	fdrop(fp, td);
    } else
	PROC_UNLOCK(p);
#ifdef DEBUG
	if (ldebug(open))
		printf(LMSG("open returns error %d"), error);
#endif
    return error;
}

int
linux_lseek(struct thread *td, struct linux_lseek_args *args)
{

    struct lseek_args /* {
	int fd;
	int pad;
	off_t offset;
	int whence;
    } */ tmp_args;
    int error;

#ifdef DEBUG
	if (ldebug(lseek))
		printf(ARGS(lseek, "%d, %ld, %d"),
		    args->fdes, (long)args->off, args->whence);
#endif
    tmp_args.fd = args->fdes;
    tmp_args.offset = (off_t)args->off;
    tmp_args.whence = args->whence;
    error = lseek(td, &tmp_args);
    return error;
}

#ifndef __alpha__
int
linux_llseek(struct thread *td, struct linux_llseek_args *args)
{
	struct lseek_args bsd_args;
	int error;
	off_t off;

#ifdef DEBUG
	if (ldebug(llseek))
		printf(ARGS(llseek, "%d, %d:%d, %d"),
		    args->fd, args->ohigh, args->olow, args->whence);
#endif
	off = (args->olow) | (((off_t) args->ohigh) << 32);

	bsd_args.fd = args->fd;
	bsd_args.offset = off;
	bsd_args.whence = args->whence;

	if ((error = lseek(td, &bsd_args)))
		return error;

	if ((error = copyout(td->td_retval, (caddr_t)args->res, sizeof (off_t))))
		return error;

	td->td_retval[0] = 0;
	return 0;
}
#endif /*!__alpha__*/

#ifndef __alpha__
int
linux_readdir(struct thread *td, struct linux_readdir_args *args)
{
	struct linux_getdents_args lda;

	lda.fd = args->fd;
	lda.dent = args->dent;
	lda.count = 1;
	return linux_getdents(td, &lda);
}
#endif /*!__alpha__*/

/*
 * Note that linux_getdents(2) and linux_getdents64(2) have the same
 * arguments. They only differ in the definition of struct dirent they
 * operate on. We use this to common the code, with the exception of
 * accessing struct dirent. Note that linux_readdir(2) is implemented
 * by means of linux_getdents(2). In this case we never operate on
 * struct dirent64 and thus don't need to handle it...
 */

struct l_dirent {
	l_long		d_ino;
	l_off_t		d_off;
	l_ushort	d_reclen;
	char		d_name[LINUX_NAME_MAX + 1];
};

struct l_dirent64 {
	uint64_t	d_ino;
	int64_t		d_off;
	l_ushort	d_reclen;
	u_char		d_type;
	char		d_name[LINUX_NAME_MAX + 1];
};

#define LINUX_RECLEN(de,namlen) \
    ALIGN((((char *)&(de)->d_name - (char *)de) + (namlen) + 1))

#define	LINUX_DIRBLKSIZ		512

static int
getdents_common(struct thread *td, struct linux_getdents64_args *args,
    int is64bit)
{
	register struct dirent *bdp;
	struct vnode *vp;
	caddr_t inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* Linux-format */
	int resid, linuxreclen=0;	/* Linux-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct vattr va;
	off_t off;
	struct l_dirent linux_dirent;
	struct l_dirent64 linux_dirent64;
	int buflen, error, eofflag, nbytes, justone;
	u_long *cookies = NULL, *cookiep;
	int ncookies;

	if ((error = getvnode(td->td_proc->p_fd, args->fd, &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	vp = (struct vnode *) fp->f_data;
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (EINVAL);
	}

	if ((error = VOP_GETATTR(vp, &va, td->td_proc->p_ucred, td))) {
		fdrop(fp, td);
		return (error);
	}

	nbytes = args->count;
	if (nbytes == 1) {
		/* readdir(2) case. Always struct dirent. */
		if (is64bit) {
			fdrop(fp, td);
			return (EINVAL);
		}
		nbytes = sizeof(linux_dirent);
		justone = 1;
	} else
		justone = 0;

	off = fp->f_offset;

	buflen = max(LINUX_DIRBLKSIZ, nbytes);
	buflen = min(buflen, MAXBSIZE);
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_td = td;
	auio.uio_resid = buflen;
	auio.uio_offset = off;

	if (cookies) {
		free(cookies, M_TEMP);
		cookies = NULL;
	}

	if ((error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies,
		 &cookies)))
		goto out;

	inp = buf;
	outp = (caddr_t)args->dirent;
	resid = nbytes;
	if ((len = buflen - auio.uio_resid) <= 0)
		goto eof;

	cookiep = cookies;

	if (cookies) {
		/*
		 * When using cookies, the vfs has the option of reading from
		 * a different offset than that supplied (UFS truncates the
		 * offset to a block boundary to make sure that it never reads
		 * partway through a directory entry, even if the directory
		 * has been compacted).
		 */
		while (len > 0 && ncookies > 0 && *cookiep <= off) {
			bdp = (struct dirent *) inp;
			len -= bdp->d_reclen;
			inp += bdp->d_reclen;
			cookiep++;
			ncookies--;
		}
	}

	while (len > 0) {
		if (cookiep && ncookies == 0)
			break;
		bdp = (struct dirent *) inp;
		reclen = bdp->d_reclen;
		if (reclen & 3) {
			error = EFAULT;
			goto out;
		}

		if (bdp->d_fileno == 0) {
			inp += reclen;
			if (cookiep) {
				off = *cookiep++;
				ncookies--;
			} else
				off += reclen;

			len -= reclen;
			continue;
		}

		linuxreclen = (is64bit)
		    ? LINUX_RECLEN(&linux_dirent64, bdp->d_namlen)
		    : LINUX_RECLEN(&linux_dirent, bdp->d_namlen);

		if (reclen > len || resid < linuxreclen) {
			outp++;
			break;
		}

		if (justone) {
			/* readdir(2) case. */
			linux_dirent.d_ino = (l_long)bdp->d_fileno;
			linux_dirent.d_off = (l_off_t)linuxreclen;
			linux_dirent.d_reclen = (l_ushort)bdp->d_namlen;
			strcpy(linux_dirent.d_name, bdp->d_name);
			error = copyout(&linux_dirent, outp, linuxreclen);
		} else {
			if (is64bit) {
				linux_dirent64.d_ino = bdp->d_fileno;
				linux_dirent64.d_off = (cookiep)
				    ? (l_off_t)*cookiep
				    : (l_off_t)(off + reclen);
				linux_dirent64.d_reclen =
				    (l_ushort)linuxreclen;
				linux_dirent64.d_type = bdp->d_type;
				strcpy(linux_dirent64.d_name, bdp->d_name);
				error = copyout(&linux_dirent64, outp,
				    linuxreclen);
			} else {
				linux_dirent.d_ino = bdp->d_fileno;
				linux_dirent.d_off = (cookiep)
				    ? (l_off_t)*cookiep
				    : (l_off_t)(off + reclen);
				linux_dirent.d_reclen = (l_ushort)linuxreclen;
				strcpy(linux_dirent.d_name, bdp->d_name);
				error = copyout(&linux_dirent, outp,
				    linuxreclen);
			}
		}
		if (error)
			goto out;

		inp += reclen;
		if (cookiep) {
			off = *cookiep++;
			ncookies--;
		} else
			off += reclen;

		outp += linuxreclen;
		resid -= linuxreclen;
		len -= reclen;
		if (justone)
			break;
	}

	if (outp == (caddr_t)args->dirent)
		goto again;

	fp->f_offset = off;
	if (justone)
		nbytes = resid + linuxreclen;

eof:
	td->td_retval[0] = nbytes - resid;

out:
	if (cookies)
		free(cookies, M_TEMP);

	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	free(buf, M_TEMP);
	return (error);
}

int
linux_getdents(struct thread *td, struct linux_getdents_args *args)
{

#ifdef DEBUG
	if (ldebug(getdents))
		printf(ARGS(getdents, "%d, *, %d"), args->fd, args->count);
#endif

	return (getdents_common(td, (struct linux_getdents64_args*)args, 0));
}

int
linux_getdents64(struct thread *td, struct linux_getdents64_args *args)
{

#ifdef DEBUG
	if (ldebug(getdents64))
		printf(ARGS(getdents64, "%d, *, %d"), args->fd, args->count);
#endif

	return (getdents_common(td, args, 1));
}

/*
 * These exist mainly for hooks for doing /compat/linux translation.
 */

int
linux_access(struct thread *td, struct linux_access_args *args)
{
	struct access_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(access))
		printf(ARGS(access, "%s, %d"), args->path, args->flags);
#endif
	bsd.path = args->path;
	bsd.flags = args->flags;

	return access(td, &bsd);
}

int
linux_unlink(struct thread *td, struct linux_unlink_args *args)
{
	struct unlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(unlink))
		printf(ARGS(unlink, "%s"), args->path);
#endif
	bsd.path = args->path;

	return unlink(td, &bsd);
}

int
linux_chdir(struct thread *td, struct linux_chdir_args *args)
{
	struct chdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chdir))
		printf(ARGS(chdir, "%s"), args->path);
#endif
	bsd.path = args->path;

	return chdir(td, &bsd);
}

int
linux_chmod(struct thread *td, struct linux_chmod_args *args)
{
	struct chmod_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chmod))
		printf(ARGS(chmod, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;

	return chmod(td, &bsd);
}

int
linux_mkdir(struct thread *td, struct linux_mkdir_args *args)
{
	struct mkdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTCREAT(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(mkdir))
		printf(ARGS(mkdir, "%s, %d"), args->path, args->mode);
#endif
	bsd.path = args->path;
	bsd.mode = args->mode;

	return mkdir(td, &bsd);
}

int
linux_rmdir(struct thread *td, struct linux_rmdir_args *args)
{
	struct rmdir_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(rmdir))
		printf(ARGS(rmdir, "%s"), args->path);
#endif
	bsd.path = args->path;

	return rmdir(td, &bsd);
}

int
linux_rename(struct thread *td, struct linux_rename_args *args)
{
	struct rename_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->from);
	CHECKALTCREAT(td, &sg, args->to);

#ifdef DEBUG
	if (ldebug(rename))
		printf(ARGS(rename, "%s, %s"), args->from, args->to);
#endif
	bsd.from = args->from;
	bsd.to = args->to;

	return rename(td, &bsd);
}

int
linux_symlink(struct thread *td, struct linux_symlink_args *args)
{
	struct symlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);
	CHECKALTCREAT(td, &sg, args->to);

#ifdef DEBUG
	if (ldebug(symlink))
		printf(ARGS(symlink, "%s, %s"), args->path, args->to);
#endif
	bsd.path = args->path;
	bsd.link = args->to;

	return symlink(td, &bsd);
}

int
linux_readlink(struct thread *td, struct linux_readlink_args *args)
{
	struct readlink_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->name);

#ifdef DEBUG
	if (ldebug(readlink))
		printf(ARGS(readlink, "%s, %p, %d"),
		    args->name, (void *)args->buf, args->count);
#endif
	bsd.path = args->name;
	bsd.buf = args->buf;
	bsd.count = args->count;

	return readlink(td, &bsd);
}

int
linux_truncate(struct thread *td, struct linux_truncate_args *args)
{
	struct truncate_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(truncate))
		printf(ARGS(truncate, "%s, %ld"), args->path,
		    (long)args->length);
#endif
	bsd.path = args->path;
	bsd.length = args->length;

	return truncate(td, &bsd);
}

int
linux_link(struct thread *td, struct linux_link_args *args)
{
    struct link_args bsd;
    caddr_t sg;

    sg = stackgap_init();
    CHECKALTEXIST(td, &sg, args->path);
    CHECKALTCREAT(td, &sg, args->to);

#ifdef DEBUG
	if (ldebug(link))
		printf(ARGS(link, "%s, %s"), args->path, args->to);
#endif

    bsd.path = args->path;
    bsd.link = args->to;

    return link(td, &bsd);
}

#ifndef __alpha__
int
linux_fdatasync(td, uap)
	struct thread *td;
	struct linux_fdatasync_args *uap;
{
	struct fsync_args bsd;

	bsd.fd = uap->fd;
	return fsync(td, &bsd);
}
#endif /*!__alpha__*/

int
linux_pread(td, uap)
	struct thread *td;
	struct linux_pread_args *uap;
{
	struct pread_args bsd;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	return pread(td, &bsd);
}

int
linux_pwrite(td, uap)
	struct thread *td;
	struct linux_pwrite_args *uap;
{
	struct pwrite_args bsd;

	bsd.fd = uap->fd;
	bsd.buf = uap->buf;
	bsd.nbyte = uap->nbyte;
	bsd.offset = uap->offset;
	return pwrite(td, &bsd);
}

int
linux_mount(struct thread *td, struct linux_mount_args *args)
{
	struct ufs_args ufs;
        char fstypename[MFSNAMELEN];
        char mntonname[MNAMELEN], mntfromname[MNAMELEN];
	int error;
	int fsflags;
	const char *fstype;
	void *fsdata;

        error = copyinstr(args->filesystemtype, fstypename, MFSNAMELEN - 1,
	    NULL);
	if (error)
                return (error);
        error = copyinstr(args->specialfile, mntfromname, MFSNAMELEN - 1, NULL);
	if (error)
                return (error);
        error = copyinstr(args->dir, mntonname, MFSNAMELEN - 1, NULL);
	if (error)
                return (error);

#ifdef DEBUG
	if (ldebug(mount))
		printf(ARGS(mount, "%s, %s, %s"),
		    fstypename, mntfromname, mntonname);
#endif

	if (strcmp(fstypename, "ext2") == 0) {
		fstype = "ext2fs";
		fsdata = &ufs;
		ufs.fspec = mntfromname;
#define DEFAULT_ROOTID		-2
		ufs.export.ex_root = DEFAULT_ROOTID;
		ufs.export.ex_flags =
		    args->rwflag & LINUX_MS_RDONLY ? MNT_EXRDONLY : 0;
	} else if (strcmp(fstypename, "proc") == 0) {
		fstype = "linprocfs";
		fsdata = NULL;
	} else {
		return (ENODEV);
	}

	fsflags = 0;

	if ((args->rwflag & 0xffff0000) == 0xc0ed0000) {
		/*
		 * Linux SYNC flag is not included; the closest equivalent
		 * FreeBSD has is !ASYNC, which is our default.
		 */
		if (args->rwflag & LINUX_MS_RDONLY)
			fsflags |= MNT_RDONLY; 
		if (args->rwflag & LINUX_MS_NOSUID)
			fsflags |= MNT_NOSUID; 
		if (args->rwflag & LINUX_MS_NODEV)
			fsflags |= MNT_NODEV; 
		if (args->rwflag & LINUX_MS_NOEXEC)
			fsflags |= MNT_NOEXEC; 
		if (args->rwflag & LINUX_MS_REMOUNT)
			fsflags |= MNT_UPDATE; 
	}

	return (vfs_mount(td, fstype, mntonname, fsflags, fsdata));
}

int
linux_oldumount(struct thread *td, struct linux_oldumount_args *args)
{
	struct linux_umount_args args2;

	args2.path = args->path;
	args2.flags = 0;
	return (linux_umount(td, &args2));
}

int
linux_umount(struct thread *td, struct linux_umount_args *args)
{
	struct unmount_args bsd;

	bsd.path = args->path;
	bsd.flags = args->flags;	/* XXX correct? */
	return (unmount(td, &bsd));
}

/*
 * fcntl family of syscalls
 */

struct l_flock {
	l_short		l_type;
	l_short		l_whence;
	l_off_t		l_start;
	l_off_t		l_len;
	l_pid_t		l_pid;
};

static void
linux_to_bsd_flock(struct l_flock *linux_flock, struct flock *bsd_flock)
{
	switch (linux_flock->l_type) {
	case LINUX_F_RDLCK:
		bsd_flock->l_type = F_RDLCK;
		break;
	case LINUX_F_WRLCK:
		bsd_flock->l_type = F_WRLCK;
		break;
	case LINUX_F_UNLCK:
		bsd_flock->l_type = F_UNLCK;
		break;
	default:
		bsd_flock->l_type = -1;
		break;
	}
	bsd_flock->l_whence = linux_flock->l_whence;
	bsd_flock->l_start = (off_t)linux_flock->l_start;
	bsd_flock->l_len = (off_t)linux_flock->l_len;
	bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock(struct flock *bsd_flock, struct l_flock *linux_flock)
{
	switch (bsd_flock->l_type) {
	case F_RDLCK:
		linux_flock->l_type = LINUX_F_RDLCK;
		break;
	case F_WRLCK:
		linux_flock->l_type = LINUX_F_WRLCK;
		break;
	case F_UNLCK:
		linux_flock->l_type = LINUX_F_UNLCK;
		break;
	}
	linux_flock->l_whence = bsd_flock->l_whence;
	linux_flock->l_start = (l_off_t)bsd_flock->l_start;
	linux_flock->l_len = (l_off_t)bsd_flock->l_len;
	linux_flock->l_pid = (l_pid_t)bsd_flock->l_pid;
}

#if defined(__i386__)
struct l_flock64 {
	l_short		l_type;
	l_short		l_whence;
	l_loff_t	l_start;
	l_loff_t	l_len;
	l_pid_t		l_pid;
};

static void
linux_to_bsd_flock64(struct l_flock64 *linux_flock, struct flock *bsd_flock)
{
	switch (linux_flock->l_type) {
	case LINUX_F_RDLCK:
		bsd_flock->l_type = F_RDLCK;
		break;
	case LINUX_F_WRLCK:
		bsd_flock->l_type = F_WRLCK;
		break;
	case LINUX_F_UNLCK:
		bsd_flock->l_type = F_UNLCK;
		break;
	default:
		bsd_flock->l_type = -1;
		break;
	}
	bsd_flock->l_whence = linux_flock->l_whence;
	bsd_flock->l_start = (off_t)linux_flock->l_start;
	bsd_flock->l_len = (off_t)linux_flock->l_len;
	bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock64(struct flock *bsd_flock, struct l_flock64 *linux_flock)
{
	switch (bsd_flock->l_type) {
	case F_RDLCK:
		linux_flock->l_type = LINUX_F_RDLCK;
		break;
	case F_WRLCK:
		linux_flock->l_type = LINUX_F_WRLCK;
		break;
	case F_UNLCK:
		linux_flock->l_type = LINUX_F_UNLCK;
		break;
	}
	linux_flock->l_whence = bsd_flock->l_whence;
	linux_flock->l_start = (l_loff_t)bsd_flock->l_start;
	linux_flock->l_len = (l_loff_t)bsd_flock->l_len;
	linux_flock->l_pid = (l_pid_t)bsd_flock->l_pid;
}
#endif /* __i386__ */

#if defined(__alpha__)
#define	linux_fcntl64_args	linux_fcntl_args
#endif

static int
fcntl_common(struct thread *td, struct linux_fcntl64_args *args)
{
	struct fcntl_args fcntl_args;
	struct file *fp;
	int error, result;

	fcntl_args.fd = args->fd;

	switch (args->cmd) {
	case LINUX_F_DUPFD:
		fcntl_args.cmd = F_DUPFD;
		fcntl_args.arg = args->arg;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_GETFD:
		fcntl_args.cmd = F_GETFD;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_SETFD:
		fcntl_args.cmd = F_SETFD;
		fcntl_args.arg = args->arg;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_GETFL:
		fcntl_args.cmd = F_GETFL;
		error = fcntl(td, &fcntl_args);
		result = td->td_retval[0];
		td->td_retval[0] = 0;
		if (result & O_RDONLY)
			td->td_retval[0] |= LINUX_O_RDONLY;
		if (result & O_WRONLY)
			td->td_retval[0] |= LINUX_O_WRONLY;
		if (result & O_RDWR)
			td->td_retval[0] |= LINUX_O_RDWR;
		if (result & O_NDELAY)
			td->td_retval[0] |= LINUX_O_NONBLOCK;
		if (result & O_APPEND)
			td->td_retval[0] |= LINUX_O_APPEND;
		if (result & O_FSYNC)
			td->td_retval[0] |= LINUX_O_SYNC;
		if (result & O_ASYNC)
			td->td_retval[0] |= LINUX_FASYNC;
		return (error);

	case LINUX_F_SETFL:
		fcntl_args.arg = 0;
		if (args->arg & LINUX_O_NDELAY)
			fcntl_args.arg |= O_NONBLOCK;
		if (args->arg & LINUX_O_APPEND)
			fcntl_args.arg |= O_APPEND;
		if (args->arg & LINUX_O_SYNC)
			fcntl_args.arg |= O_FSYNC;
		if (args->arg & LINUX_FASYNC)
			fcntl_args.arg |= O_ASYNC;
		fcntl_args.cmd = F_SETFL;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_GETOWN:
		fcntl_args.cmd = F_GETOWN;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_SETOWN:
		/*
		 * XXX some Linux applications depend on F_SETOWN having no
		 * significant effect for pipes (SIGIO is not delivered for
		 * pipes under Linux-2.2.35 at least).
		 */
		fp = ffind_hold(td, args->fd);
		if (fp == NULL)
			return EBADF;
		if (fp->f_type == DTYPE_PIPE) {
			fdrop(fp, td);
			return (EINVAL);
		}
		fdrop(fp, td);

		fcntl_args.cmd = F_SETOWN;
		fcntl_args.arg = args->arg;
		return (fcntl(td, &fcntl_args));
	}

	return (EINVAL);
}

int
linux_fcntl(struct thread *td, struct linux_fcntl_args *args)
{
	struct linux_fcntl64_args args64;
	struct fcntl_args fcntl_args;
	struct l_flock linux_flock;
	struct flock *bsd_flock;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	bsd_flock = (struct flock *)stackgap_alloc(&sg, sizeof(bsd_flock));

#ifdef DEBUG
	if (ldebug(fcntl))
		printf(ARGS(fcntl, "%d, %08x, *"), args->fd, args->cmd);
#endif

	switch (args->cmd) {
	case LINUX_F_GETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_GETLK;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(td, &fcntl_args);
		if (error)
			return (error);
		bsd_to_linux_flock(bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (caddr_t)args->arg,
		    sizeof(linux_flock)));

	case LINUX_F_SETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLK;
		fcntl_args.arg = (long)bsd_flock;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_SETLKW:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLKW;
		fcntl_args.arg = (long)bsd_flock;
		return (fcntl(td, &fcntl_args));
	}

	args64.fd = args->fd;
	args64.cmd = args->cmd;
	args64.arg = args->arg;
	return (fcntl_common(td, &args64));
}

#if defined(__i386__)
int
linux_fcntl64(struct thread *td, struct linux_fcntl64_args *args)
{
	struct fcntl_args fcntl_args;
	struct l_flock64 linux_flock;
	struct flock *bsd_flock;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	bsd_flock = (struct flock *)stackgap_alloc(&sg, sizeof(bsd_flock));

#ifdef DEBUG
	if (ldebug(fcntl64))
		printf(ARGS(fcntl64, "%d, %08x, *"), args->fd, args->cmd);
#endif

	switch (args->cmd) {
	case LINUX_F_GETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_GETLK;
		fcntl_args.arg = (long)bsd_flock;
		error = fcntl(td, &fcntl_args);
		if (error)
			return (error);
		bsd_to_linux_flock64(bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (caddr_t)args->arg,
		    sizeof(linux_flock)));

	case LINUX_F_SETLK:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLK;
		fcntl_args.arg = (long)bsd_flock;
		return (fcntl(td, &fcntl_args));

	case LINUX_F_SETLKW:
		error = copyin((caddr_t)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, bsd_flock);
		fcntl_args.fd = args->fd;
		fcntl_args.cmd = F_SETLKW;
		fcntl_args.arg = (long)bsd_flock;
		return (fcntl(td, &fcntl_args));
	}

	return (fcntl_common(td, args));
}
#endif /* __i386__ */

int
linux_chown(struct thread *td, struct linux_chown_args *args)
{
	struct chown_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(chown))
		printf(ARGS(chown, "%s, %d, %d"), args->path, args->uid,
		    args->gid);
#endif

	bsd.path = args->path;
	bsd.uid = args->uid;
	bsd.gid = args->gid;
	return (chown(td, &bsd));
}

int
linux_lchown(struct thread *td, struct linux_lchown_args *args)
{
	struct lchown_args bsd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(lchown))
		printf(ARGS(lchown, "%s, %d, %d"), args->path, args->uid,
		    args->gid);
#endif

	bsd.path = args->path;
	bsd.uid = args->uid;
	bsd.gid = args->gid;
	return (lchown(td, &bsd));
}
