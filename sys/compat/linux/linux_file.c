/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1994-1995 Søren Schmidt
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#ifdef COMPAT_LINUX32
#include <compat/freebsd32/freebsd32_misc.h>
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_file.h>

static int	linux_common_open(struct thread *, int, const char *, int, int,
		    enum uio_seg);
static int	linux_getdents_error(struct thread *, int, int);

static struct bsd_to_linux_bitmap seal_bitmap[] = {
	BITMAP_1t1_LINUX(F_SEAL_SEAL),
	BITMAP_1t1_LINUX(F_SEAL_SHRINK),
	BITMAP_1t1_LINUX(F_SEAL_GROW),
	BITMAP_1t1_LINUX(F_SEAL_WRITE),
};

#define	MFD_HUGETLB_ENTRY(_size)					\
	{								\
		.bsd_value = MFD_HUGE_##_size,				\
		.linux_value = LINUX_HUGETLB_FLAG_ENCODE_##_size	\
	}
static struct bsd_to_linux_bitmap mfd_bitmap[] = {
	BITMAP_1t1_LINUX(MFD_CLOEXEC),
	BITMAP_1t1_LINUX(MFD_ALLOW_SEALING),
	BITMAP_1t1_LINUX(MFD_HUGETLB),
	MFD_HUGETLB_ENTRY(64KB),
	MFD_HUGETLB_ENTRY(512KB),
	MFD_HUGETLB_ENTRY(1MB),
	MFD_HUGETLB_ENTRY(2MB),
	MFD_HUGETLB_ENTRY(8MB),
	MFD_HUGETLB_ENTRY(16MB),
	MFD_HUGETLB_ENTRY(32MB),
	MFD_HUGETLB_ENTRY(256MB),
	MFD_HUGETLB_ENTRY(512MB),
	MFD_HUGETLB_ENTRY(1GB),
	MFD_HUGETLB_ENTRY(2GB),
	MFD_HUGETLB_ENTRY(16GB),
};
#undef MFD_HUGETLB_ENTRY

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_creat(struct thread *td, struct linux_creat_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_openat(td, AT_FDCWD, args->path, UIO_USERSPACE,
		    O_WRONLY | O_CREAT | O_TRUNC, args->mode));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_openat(td, AT_FDCWD, path, UIO_SYSSPACE,
	    O_WRONLY | O_CREAT | O_TRUNC, args->mode);
	LFREEPATH(path);
	return (error);
}
#endif

static int
linux_common_openflags(int l_flags)
{
	int bsd_flags;

	bsd_flags = 0;
	switch (l_flags & LINUX_O_ACCMODE) {
	case LINUX_O_WRONLY:
		bsd_flags |= O_WRONLY;
		break;
	case LINUX_O_RDWR:
		bsd_flags |= O_RDWR;
		break;
	default:
		bsd_flags |= O_RDONLY;
	}
	if (l_flags & LINUX_O_NDELAY)
		bsd_flags |= O_NONBLOCK;
	if (l_flags & LINUX_O_APPEND)
		bsd_flags |= O_APPEND;
	if (l_flags & LINUX_O_SYNC)
		bsd_flags |= O_FSYNC;
	if (l_flags & LINUX_O_CLOEXEC)
		bsd_flags |= O_CLOEXEC;
	if (l_flags & LINUX_O_NONBLOCK)
		bsd_flags |= O_NONBLOCK;
	if (l_flags & LINUX_O_ASYNC)
		bsd_flags |= O_ASYNC;
	if (l_flags & LINUX_O_CREAT)
		bsd_flags |= O_CREAT;
	if (l_flags & LINUX_O_TRUNC)
		bsd_flags |= O_TRUNC;
	if (l_flags & LINUX_O_EXCL)
		bsd_flags |= O_EXCL;
	if (l_flags & LINUX_O_NOCTTY)
		bsd_flags |= O_NOCTTY;
	if (l_flags & LINUX_O_DIRECT)
		bsd_flags |= O_DIRECT;
	if (l_flags & LINUX_O_NOFOLLOW)
		bsd_flags |= O_NOFOLLOW;
	if (l_flags & LINUX_O_DIRECTORY)
		bsd_flags |= O_DIRECTORY;
	/* XXX LINUX_O_NOATIME: unable to be easily implemented. */
	return (bsd_flags);
}

static int
linux_common_open(struct thread *td, int dirfd, const char *path, int l_flags,
    int mode, enum uio_seg seg)
{
	struct proc *p = td->td_proc;
	struct file *fp;
	int fd;
	int bsd_flags, error;

	bsd_flags = linux_common_openflags(l_flags);
	error = kern_openat(td, dirfd, path, seg, bsd_flags, mode);
	if (error != 0) {
		if (error == EMLINK)
			error = ELOOP;
		goto done;
	}
	if (p->p_flag & P_CONTROLT)
		goto done;
	if (bsd_flags & O_NOCTTY)
		goto done;

	/*
	 * XXX In between kern_openat() and fget(), another process
	 * having the same filedesc could use that fd without
	 * checking below.
	*/
	fd = td->td_retval[0];
	if (fget(td, fd, &cap_ioctl_rights, &fp) == 0) {
		if (fp->f_type != DTYPE_VNODE) {
			fdrop(fp, td);
			goto done;
		}
		sx_slock(&proctree_lock);
		PROC_LOCK(p);
		if (SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);
			/* XXXPJD: Verify if TIOCSCTTY is allowed. */
			(void) fo_ioctl(fp, TIOCSCTTY, (caddr_t) 0,
			    td->td_ucred, td);
		} else {
			PROC_UNLOCK(p);
			sx_sunlock(&proctree_lock);
		}
		fdrop(fp, td);
	}

done:
	return (error);
}

int
linux_openat(struct thread *td, struct linux_openat_args *args)
{
	char *path;
	int dfd, error;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		return (linux_common_open(td, dfd, args->filename, args->flags,
		    args->mode, UIO_USERSPACE));
	}
	if (args->flags & LINUX_O_CREAT)
		LCONVPATH_AT(td, args->filename, &path, 1, dfd);
	else
		LCONVPATH_AT(td, args->filename, &path, 0, dfd);

	error = linux_common_open(td, dfd, path, args->flags, args->mode,
	    UIO_SYSSPACE);
	LFREEPATH(path);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_open(struct thread *td, struct linux_open_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (linux_common_open(td, AT_FDCWD, args->path, args->flags,
		    args->mode, UIO_USERSPACE));
	}
	if (args->flags & LINUX_O_CREAT)
		LCONVPATHCREAT(td, args->path, &path);
	else
		LCONVPATHEXIST(td, args->path, &path);

	error = linux_common_open(td, AT_FDCWD, path, args->flags, args->mode,
	    UIO_SYSSPACE);
	LFREEPATH(path);
	return (error);
}
#endif

int
linux_name_to_handle_at(struct thread *td,
    struct linux_name_to_handle_at_args *args)
{
	static const l_int valid_flags = (LINUX_AT_SYMLINK_FOLLOW |
	    LINUX_AT_EMPTY_PATH);
	static const l_uint fh_size = sizeof(fhandle_t);

	fhandle_t fh;
	l_uint fh_bytes;
	l_int mount_id;
	int error, fd, bsd_flags;

	if (args->flags & ~valid_flags)
		return (EINVAL);
	if (args->flags & LINUX_AT_EMPTY_PATH)
		/* XXX: not supported yet */
		return (EOPNOTSUPP);

	fd = args->dirfd;
	if (fd == LINUX_AT_FDCWD)
		fd = AT_FDCWD;

	bsd_flags = 0;
	if (!(args->flags & LINUX_AT_SYMLINK_FOLLOW))
		bsd_flags |= AT_SYMLINK_NOFOLLOW;

	if (!LUSECONVPATH(td)) {
		error = kern_getfhat(td, bsd_flags, fd, args->name,
		    UIO_USERSPACE, &fh, UIO_SYSSPACE);
	} else {
		char *path;

		LCONVPATH_AT(td, args->name, &path, 0, fd);
		error = kern_getfhat(td, bsd_flags, fd, path, UIO_SYSSPACE,
		    &fh, UIO_SYSSPACE);
		LFREEPATH(path);
	}
	if (error != 0)
		return (error);

	/* Emit mount_id -- required before EOVERFLOW case. */
	mount_id = (fh.fh_fsid.val[0] ^ fh.fh_fsid.val[1]);
	error = copyout(&mount_id, args->mnt_id, sizeof(mount_id));
	if (error != 0)
		return (error);

	/* Check if there is room for handle. */
	error = copyin(&args->handle->handle_bytes, &fh_bytes,
	    sizeof(fh_bytes));
	if (error != 0)
		return (error);

	if (fh_bytes < fh_size) {
		error = copyout(&fh_size, &args->handle->handle_bytes,
		    sizeof(fh_size));
		if (error == 0)
			error = EOVERFLOW;
		return (error);
	}

	/* Emit handle. */
	mount_id = 0;
	/*
	 * We don't use handle_type for anything yet, but initialize a known
	 * value.
	 */
	error = copyout(&mount_id, &args->handle->handle_type,
	    sizeof(mount_id));
	if (error != 0)
		return (error);

	error = copyout(&fh, &args->handle->f_handle,
	    sizeof(fh));
	return (error);
}

int
linux_open_by_handle_at(struct thread *td,
    struct linux_open_by_handle_at_args *args)
{
	l_uint fh_bytes;
	int bsd_flags, error;

	error = copyin(&args->handle->handle_bytes, &fh_bytes,
	    sizeof(fh_bytes));
	if (error != 0)
		return (error);

	if (fh_bytes < sizeof(fhandle_t))
		return (EINVAL);

	bsd_flags = linux_common_openflags(args->flags);
	return (kern_fhopen(td, (void *)&args->handle->f_handle, bsd_flags));
}

int
linux_lseek(struct thread *td, struct linux_lseek_args *args)
{

	return (kern_lseek(td, args->fdes, args->off, args->whence));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_llseek(struct thread *td, struct linux_llseek_args *args)
{
	int error;
	off_t off;

	off = (args->olow) | (((off_t) args->ohigh) << 32);

	error = kern_lseek(td, args->fd, off, args->whence);
	if (error != 0)
		return (error);

	error = copyout(td->td_retval, args->res, sizeof(off_t));
	if (error != 0)
		return (error);

	td->td_retval[0] = 0;
	return (0);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

/*
 * Note that linux_getdents(2) and linux_getdents64(2) have the same
 * arguments. They only differ in the definition of struct dirent they
 * operate on.
 * Note that linux_readdir(2) is a special case of linux_getdents(2)
 * where count is always equals 1, meaning that the buffer is one
 * dirent-structure in size and that the code can't handle more anyway.
 * Note that linux_readdir(2) can't be implemented by means of linux_getdents(2)
 * as in case when the *dent buffer size is equal to 1 linux_getdents(2) will
 * trash user stack.
 */

static int
linux_getdents_error(struct thread *td, int fd, int err)
{
	struct vnode *vp;
	struct file *fp;
	int error;

	/* Linux return ENOTDIR in case when fd is not a directory. */
	error = getvnode(td, fd, &cap_read_rights, &fp);
	if (error != 0)
		return (error);
	vp = fp->f_vnode;
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (ENOTDIR);
	}
	fdrop(fp, td);
	return (err);
}

struct l_dirent {
	l_ulong		d_ino;
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

/*
 * Linux uses the last byte in the dirent buffer to store d_type,
 * at least glibc-2.7 requires it. That is why l_dirent is padded with 2 bytes.
 */
#define LINUX_RECLEN(namlen)						\
    roundup(offsetof(struct l_dirent, d_name) + (namlen) + 2, sizeof(l_ulong))

#define LINUX_RECLEN64(namlen)						\
    roundup(offsetof(struct l_dirent64, d_name) + (namlen) + 1,		\
    sizeof(uint64_t))

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_getdents(struct thread *td, struct linux_getdents_args *args)
{
	struct dirent *bdp;
	caddr_t inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* Linux-format */
	int resid, linuxreclen;		/* Linux-format */
	caddr_t lbuf;			/* Linux-format */
	off_t base;
	struct l_dirent *linux_dirent;
	int buflen, error;
	size_t retval;

	buflen = min(args->count, MAXBSIZE);
	buf = malloc(buflen, M_TEMP, M_WAITOK);

	error = kern_getdirentries(td, args->fd, buf, buflen,
	    &base, NULL, UIO_SYSSPACE);
	if (error != 0) {
		error = linux_getdents_error(td, args->fd, error);
		goto out1;
	}

	lbuf = malloc(LINUX_RECLEN(LINUX_NAME_MAX), M_TEMP, M_WAITOK | M_ZERO);

	len = td->td_retval[0];
	inp = buf;
	outp = (caddr_t)args->dent;
	resid = args->count;
	retval = 0;

	while (len > 0) {
		bdp = (struct dirent *) inp;
		reclen = bdp->d_reclen;
		linuxreclen = LINUX_RECLEN(bdp->d_namlen);
		/*
		 * No more space in the user supplied dirent buffer.
		 * Return EINVAL.
		 */
		if (resid < linuxreclen) {
			error = EINVAL;
			goto out;
		}

		linux_dirent = (struct l_dirent*)lbuf;
		linux_dirent->d_ino = bdp->d_fileno;
		linux_dirent->d_off = base + reclen;
		linux_dirent->d_reclen = linuxreclen;
		/*
		 * Copy d_type to last byte of l_dirent buffer
		 */
		lbuf[linuxreclen - 1] = bdp->d_type;
		strlcpy(linux_dirent->d_name, bdp->d_name,
		    linuxreclen - offsetof(struct l_dirent, d_name)-1);
		error = copyout(linux_dirent, outp, linuxreclen);
		if (error != 0)
			goto out;

		inp += reclen;
		base += reclen;
		len -= reclen;

		retval += linuxreclen;
		outp += linuxreclen;
		resid -= linuxreclen;
	}
	td->td_retval[0] = retval;

out:
	free(lbuf, M_TEMP);
out1:
	free(buf, M_TEMP);
	return (error);
}
#endif

int
linux_getdents64(struct thread *td, struct linux_getdents64_args *args)
{
	struct dirent *bdp;
	caddr_t inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* Linux-format */
	int resid, linuxreclen;		/* Linux-format */
	caddr_t lbuf;			/* Linux-format */
	off_t base;
	struct l_dirent64 *linux_dirent64;
	int buflen, error;
	size_t retval;

	buflen = min(args->count, MAXBSIZE);
	buf = malloc(buflen, M_TEMP, M_WAITOK);

	error = kern_getdirentries(td, args->fd, buf, buflen,
	    &base, NULL, UIO_SYSSPACE);
	if (error != 0) {
		error = linux_getdents_error(td, args->fd, error);
		goto out1;
	}

	lbuf = malloc(LINUX_RECLEN64(LINUX_NAME_MAX), M_TEMP, M_WAITOK | M_ZERO);

	len = td->td_retval[0];
	inp = buf;
	outp = (caddr_t)args->dirent;
	resid = args->count;
	retval = 0;

	while (len > 0) {
		bdp = (struct dirent *) inp;
		reclen = bdp->d_reclen;
		linuxreclen = LINUX_RECLEN64(bdp->d_namlen);
		/*
		 * No more space in the user supplied dirent buffer.
		 * Return EINVAL.
		 */
		if (resid < linuxreclen) {
			error = EINVAL;
			goto out;
		}

		linux_dirent64 = (struct l_dirent64*)lbuf;
		linux_dirent64->d_ino = bdp->d_fileno;
		linux_dirent64->d_off = base + reclen;
		linux_dirent64->d_reclen = linuxreclen;
		linux_dirent64->d_type = bdp->d_type;
		strlcpy(linux_dirent64->d_name, bdp->d_name,
		    linuxreclen - offsetof(struct l_dirent64, d_name));
		error = copyout(linux_dirent64, outp, linuxreclen);
		if (error != 0)
			goto out;

		inp += reclen;
		base += reclen;
		len -= reclen;

		retval += linuxreclen;
		outp += linuxreclen;
		resid -= linuxreclen;
	}
	td->td_retval[0] = retval;

out:
	free(lbuf, M_TEMP);
out1:
	free(buf, M_TEMP);
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_readdir(struct thread *td, struct linux_readdir_args *args)
{
	struct dirent *bdp;
	caddr_t buf;			/* BSD-format */
	int linuxreclen;		/* Linux-format */
	caddr_t lbuf;			/* Linux-format */
	off_t base;
	struct l_dirent *linux_dirent;
	int buflen, error;

	buflen = LINUX_RECLEN(LINUX_NAME_MAX);
	buf = malloc(buflen, M_TEMP, M_WAITOK);

	error = kern_getdirentries(td, args->fd, buf, buflen,
	    &base, NULL, UIO_SYSSPACE);
	if (error != 0) {
		error = linux_getdents_error(td, args->fd, error);
		goto out;
	}
	if (td->td_retval[0] == 0)
		goto out;

	lbuf = malloc(LINUX_RECLEN(LINUX_NAME_MAX), M_TEMP, M_WAITOK | M_ZERO);

	bdp = (struct dirent *) buf;
	linuxreclen = LINUX_RECLEN(bdp->d_namlen);

	linux_dirent = (struct l_dirent*)lbuf;
	linux_dirent->d_ino = bdp->d_fileno;
	linux_dirent->d_off = linuxreclen;
	linux_dirent->d_reclen = bdp->d_namlen;
	strlcpy(linux_dirent->d_name, bdp->d_name,
	    linuxreclen - offsetof(struct l_dirent, d_name));
	error = copyout(linux_dirent, args->dent, linuxreclen);
	if (error == 0)
		td->td_retval[0] = linuxreclen;

	free(lbuf, M_TEMP);
out:
	free(buf, M_TEMP);
	return (error);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

/*
 * These exist mainly for hooks for doing /compat/linux translation.
 */

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_access(struct thread *td, struct linux_access_args *args)
{
	char *path;
	int error;

	/* Linux convention. */
	if (args->amode & ~(F_OK | X_OK | W_OK | R_OK))
		return (EINVAL);

	if (!LUSECONVPATH(td)) {
		error = kern_accessat(td, AT_FDCWD, args->path, UIO_USERSPACE, 0,
		    args->amode);
	} else {
		LCONVPATHEXIST(td, args->path, &path);
		error = kern_accessat(td, AT_FDCWD, path, UIO_SYSSPACE, 0,
		    args->amode);
		LFREEPATH(path);
	}

	return (error);
}
#endif

int
linux_faccessat(struct thread *td, struct linux_faccessat_args *args)
{
	char *path;
	int error, dfd;

	/* Linux convention. */
	if (args->amode & ~(F_OK | X_OK | W_OK | R_OK))
		return (EINVAL);

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		error = kern_accessat(td, dfd, args->filename, UIO_USERSPACE, 0, args->amode);
	} else {
		LCONVPATHEXIST_AT(td, args->filename, &path, dfd);
		error = kern_accessat(td, dfd, path, UIO_SYSSPACE, 0, args->amode);
		LFREEPATH(path);
	}

	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_unlink(struct thread *td, struct linux_unlink_args *args)
{
	char *path;
	int error;
	struct stat st;

	if (!LUSECONVPATH(td)) {
		error = kern_funlinkat(td, AT_FDCWD, args->path, FD_NONE,
		    UIO_USERSPACE, 0, 0);
		if (error == EPERM) {
			/* Introduce POSIX noncompliant behaviour of Linux */
			if (kern_statat(td, 0, AT_FDCWD, args->path,
			    UIO_SYSSPACE, &st, NULL) == 0) {
				if (S_ISDIR(st.st_mode))
					error = EISDIR;
			}
		}
	} else {
		LCONVPATHEXIST(td, args->path, &path);
		error = kern_funlinkat(td, AT_FDCWD, path, FD_NONE, UIO_SYSSPACE, 0, 0);
		if (error == EPERM) {
			/* Introduce POSIX noncompliant behaviour of Linux */
			if (kern_statat(td, 0, AT_FDCWD, path, UIO_SYSSPACE, &st,
			    NULL) == 0) {
				if (S_ISDIR(st.st_mode))
					error = EISDIR;
			}
		}
		LFREEPATH(path);
	}

	return (error);
}
#endif

static int
linux_unlinkat_impl(struct thread *td, enum uio_seg pathseg, const char *path,
    int dfd, struct linux_unlinkat_args *args)
{
	struct stat st;
	int error;

	if (args->flag & LINUX_AT_REMOVEDIR)
		error = kern_frmdirat(td, dfd, path, FD_NONE, pathseg, 0);
	else
		error = kern_funlinkat(td, dfd, path, FD_NONE, pathseg, 0, 0);
	if (error == EPERM && !(args->flag & LINUX_AT_REMOVEDIR)) {
		/* Introduce POSIX noncompliant behaviour of Linux */
		if (kern_statat(td, AT_SYMLINK_NOFOLLOW, dfd, path,
		    UIO_SYSSPACE, &st, NULL) == 0 && S_ISDIR(st.st_mode))
			error = EISDIR;
	}
	return (error);
}

int
linux_unlinkat(struct thread *td, struct linux_unlinkat_args *args)
{
	char *path;
	int error, dfd;

	if (args->flag & ~LINUX_AT_REMOVEDIR)
		return (EINVAL);
	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		return (linux_unlinkat_impl(td, UIO_USERSPACE, args->pathname,
		    dfd, args));
	}
	LCONVPATHEXIST_AT(td, args->pathname, &path, dfd);
	error = linux_unlinkat_impl(td, UIO_SYSSPACE, path, dfd, args);
	LFREEPATH(path);
	return (error);
}
int
linux_chdir(struct thread *td, struct linux_chdir_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_chdir(td, args->path, UIO_USERSPACE));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_chdir(td, path, UIO_SYSSPACE);
	LFREEPATH(path);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_chmod(struct thread *td, struct linux_chmod_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_fchmodat(td, AT_FDCWD, args->path, UIO_USERSPACE,
		    args->mode, 0));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_fchmodat(td, AT_FDCWD, path, UIO_SYSSPACE, args->mode, 0);
	LFREEPATH(path);
	return (error);
}
#endif

int
linux_fchmodat(struct thread *td, struct linux_fchmodat_args *args)
{
	char *path;
	int error, dfd;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		return (kern_fchmodat(td, dfd, args->filename, UIO_USERSPACE,
		    args->mode, 0));
	}
	LCONVPATHEXIST_AT(td, args->filename, &path, dfd);
	error = kern_fchmodat(td, dfd, path, UIO_SYSSPACE, args->mode, 0);
	LFREEPATH(path);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_mkdir(struct thread *td, struct linux_mkdir_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_mkdirat(td, AT_FDCWD, args->path, UIO_USERSPACE, args->mode));
	}
	LCONVPATHCREAT(td, args->path, &path);
	error = kern_mkdirat(td, AT_FDCWD, path, UIO_SYSSPACE, args->mode);
	LFREEPATH(path);
	return (error);
}
#endif

int
linux_mkdirat(struct thread *td, struct linux_mkdirat_args *args)
{
	char *path;
	int error, dfd;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		return (kern_mkdirat(td, dfd, args->pathname, UIO_USERSPACE, args->mode));
	}
	LCONVPATHCREAT_AT(td, args->pathname, &path, dfd);
	error = kern_mkdirat(td, dfd, path, UIO_SYSSPACE, args->mode);
	LFREEPATH(path);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_rmdir(struct thread *td, struct linux_rmdir_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_frmdirat(td, AT_FDCWD, args->path, FD_NONE,
		    UIO_USERSPACE, 0));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_frmdirat(td, AT_FDCWD, path, FD_NONE, UIO_SYSSPACE, 0);
	LFREEPATH(path);
	return (error);
}

int
linux_rename(struct thread *td, struct linux_rename_args *args)
{
	char *from, *to;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_renameat(td, AT_FDCWD, args->from, AT_FDCWD,
		    args->to, UIO_USERSPACE));
	}
	LCONVPATHEXIST(td, args->from, &from);
	/* Expand LCONVPATHCREATE so that `from' can be freed on errors */
	error = linux_emul_convpath(td, args->to, UIO_USERSPACE, &to, 1, AT_FDCWD);
	if (to == NULL) {
		LFREEPATH(from);
		return (error);
	}
	error = kern_renameat(td, AT_FDCWD, from, AT_FDCWD, to, UIO_SYSSPACE);
	LFREEPATH(from);
	LFREEPATH(to);
	return (error);
}
#endif

int
linux_renameat(struct thread *td, struct linux_renameat_args *args)
{
	struct linux_renameat2_args renameat2_args = {
	    .olddfd = args->olddfd,
	    .oldname = args->oldname,
	    .newdfd = args->newdfd,
	    .newname = args->newname,
	    .flags = 0
	};

	return (linux_renameat2(td, &renameat2_args));
}

int
linux_renameat2(struct thread *td, struct linux_renameat2_args *args)
{
	char *from, *to;
	int error, olddfd, newdfd;

	if (args->flags != 0) {
		if (args->flags & ~(LINUX_RENAME_EXCHANGE |
		    LINUX_RENAME_NOREPLACE | LINUX_RENAME_WHITEOUT))
			return (EINVAL);
		if (args->flags & LINUX_RENAME_EXCHANGE &&
		    args->flags & (LINUX_RENAME_NOREPLACE |
		    LINUX_RENAME_WHITEOUT))
			return (EINVAL);
#if 0
		/*
		 * This spams the console on Ubuntu Focal.
		 *
		 * What's needed here is a general mechanism to let users know
		 * about missing features without hogging the system.
		 */
		linux_msg(td, "renameat2 unsupported flags 0x%x",
		    args->flags);
#endif
		return (EINVAL);
	}

	olddfd = (args->olddfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->olddfd;
	newdfd = (args->newdfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->newdfd;
	if (!LUSECONVPATH(td)) {
		return (kern_renameat(td, olddfd, args->oldname, newdfd,
		    args->newname, UIO_USERSPACE));
	}
	LCONVPATHEXIST_AT(td, args->oldname, &from, olddfd);
	/* Expand LCONVPATHCREATE so that `from' can be freed on errors */
	error = linux_emul_convpath(td, args->newname, UIO_USERSPACE, &to, 1, newdfd);
	if (to == NULL) {
		LFREEPATH(from);
		return (error);
	}
	error = kern_renameat(td, olddfd, from, newdfd, to, UIO_SYSSPACE);
	LFREEPATH(from);
	LFREEPATH(to);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_symlink(struct thread *td, struct linux_symlink_args *args)
{
	char *path, *to;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_symlinkat(td, args->path, AT_FDCWD, args->to,
		    UIO_USERSPACE));
	}
	LCONVPATHEXIST(td, args->path, &path);
	/* Expand LCONVPATHCREATE so that `path' can be freed on errors */
	error = linux_emul_convpath(td, args->to, UIO_USERSPACE, &to, 1, AT_FDCWD);
	if (to == NULL) {
		LFREEPATH(path);
		return (error);
	}
	error = kern_symlinkat(td, path, AT_FDCWD, to, UIO_SYSSPACE);
	LFREEPATH(path);
	LFREEPATH(to);
	return (error);
}
#endif

int
linux_symlinkat(struct thread *td, struct linux_symlinkat_args *args)
{
	char *path, *to;
	int error, dfd;

	dfd = (args->newdfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->newdfd;
	if (!LUSECONVPATH(td)) {
		return (kern_symlinkat(td, args->oldname, dfd, args->newname,
		    UIO_USERSPACE));
	}
	LCONVPATHEXIST(td, args->oldname, &path);
	/* Expand LCONVPATHCREATE so that `path' can be freed on errors */
	error = linux_emul_convpath(td, args->newname, UIO_USERSPACE, &to, 1, dfd);
	if (to == NULL) {
		LFREEPATH(path);
		return (error);
	}
	error = kern_symlinkat(td, path, dfd, to, UIO_SYSSPACE);
	LFREEPATH(path);
	LFREEPATH(to);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_readlink(struct thread *td, struct linux_readlink_args *args)
{
	char *name;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_readlinkat(td, AT_FDCWD, args->name, UIO_USERSPACE,
		    args->buf, UIO_USERSPACE, args->count));
	}
	LCONVPATHEXIST(td, args->name, &name);
	error = kern_readlinkat(td, AT_FDCWD, name, UIO_SYSSPACE,
	    args->buf, UIO_USERSPACE, args->count);
	LFREEPATH(name);
	return (error);
}
#endif

int
linux_readlinkat(struct thread *td, struct linux_readlinkat_args *args)
{
	char *name;
	int error, dfd;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (!LUSECONVPATH(td)) {
		return (kern_readlinkat(td, dfd, args->path, UIO_USERSPACE,
		    args->buf, UIO_USERSPACE, args->bufsiz));
	}
	LCONVPATHEXIST_AT(td, args->path, &name, dfd);
	error = kern_readlinkat(td, dfd, name, UIO_SYSSPACE, args->buf,
	    UIO_USERSPACE, args->bufsiz);
	LFREEPATH(name);
	return (error);
}

int
linux_truncate(struct thread *td, struct linux_truncate_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_truncate(td, args->path, UIO_USERSPACE, args->length));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_truncate(td, path, UIO_SYSSPACE, args->length);
	LFREEPATH(path);
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_truncate64(struct thread *td, struct linux_truncate64_args *args)
{
	char *path;
	off_t length;
	int error;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	length = PAIR32TO64(off_t, args->length);
#else
	length = args->length;
#endif

	if (!LUSECONVPATH(td)) {
		return (kern_truncate(td, args->path, UIO_USERSPACE, length));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_truncate(td, path, UIO_SYSSPACE, length);
	LFREEPATH(path);
	return (error);
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_ftruncate(struct thread *td, struct linux_ftruncate_args *args)
{

	return (kern_ftruncate(td, args->fd, args->length));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_ftruncate64(struct thread *td, struct linux_ftruncate64_args *args)
{
	off_t length;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	length = PAIR32TO64(off_t, args->length);
#else
	length = args->length;
#endif

	return (kern_ftruncate(td, args->fd, length));
}
#endif

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_link(struct thread *td, struct linux_link_args *args)
{
	char *path, *to;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_linkat(td, AT_FDCWD, AT_FDCWD, args->path, args->to,
		    UIO_USERSPACE, FOLLOW));
	}
	LCONVPATHEXIST(td, args->path, &path);
	/* Expand LCONVPATHCREATE so that `path' can be freed on errors */
	error = linux_emul_convpath(td, args->to, UIO_USERSPACE, &to, 1, AT_FDCWD);
	if (to == NULL) {
		LFREEPATH(path);
		return (error);
	}
	error = kern_linkat(td, AT_FDCWD, AT_FDCWD, path, to, UIO_SYSSPACE,
	    FOLLOW);
	LFREEPATH(path);
	LFREEPATH(to);
	return (error);
}
#endif

int
linux_linkat(struct thread *td, struct linux_linkat_args *args)
{
	char *path, *to;
	int error, olddfd, newdfd, follow;

	if (args->flag & ~LINUX_AT_SYMLINK_FOLLOW)
		return (EINVAL);

	olddfd = (args->olddfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->olddfd;
	newdfd = (args->newdfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->newdfd;
	follow = (args->flag & LINUX_AT_SYMLINK_FOLLOW) == 0 ? NOFOLLOW :
	    FOLLOW;
	if (!LUSECONVPATH(td)) {
		return (kern_linkat(td, olddfd, newdfd, args->oldname,
		    args->newname, UIO_USERSPACE, follow));
	}
	LCONVPATHEXIST_AT(td, args->oldname, &path, olddfd);
	/* Expand LCONVPATHCREATE so that `path' can be freed on errors */
	error = linux_emul_convpath(td, args->newname, UIO_USERSPACE, &to, 1, newdfd);
	if (to == NULL) {
		LFREEPATH(path);
		return (error);
	}
	error = kern_linkat(td, olddfd, newdfd, path, to, UIO_SYSSPACE, follow);
	LFREEPATH(path);
	LFREEPATH(to);
	return (error);
}

int
linux_fdatasync(struct thread *td, struct linux_fdatasync_args *uap)
{

	return (kern_fsync(td, uap->fd, false));
}

int
linux_sync_file_range(struct thread *td, struct linux_sync_file_range_args *uap)
{
	off_t nbytes, offset;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	nbytes = PAIR32TO64(off_t, uap->nbytes);
	offset = PAIR32TO64(off_t, uap->offset);
#else
	nbytes = uap->nbytes;
	offset = uap->offset;
#endif

	if (offset < 0 || nbytes < 0 ||
	    (uap->flags & ~(LINUX_SYNC_FILE_RANGE_WAIT_BEFORE |
	    LINUX_SYNC_FILE_RANGE_WRITE |
	    LINUX_SYNC_FILE_RANGE_WAIT_AFTER)) != 0) {
		return (EINVAL);
	}

	return (kern_fsync(td, uap->fd, false));
}

int
linux_pread(struct thread *td, struct linux_pread_args *uap)
{
	struct vnode *vp;
	off_t offset;
	int error;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	offset = PAIR32TO64(off_t, uap->offset);
#else
	offset = uap->offset;
#endif

	error = kern_pread(td, uap->fd, uap->buf, uap->nbyte, offset);
	if (error == 0) {
		/* This seems to violate POSIX but Linux does it. */
		error = fgetvp(td, uap->fd, &cap_pread_rights, &vp);
		if (error != 0)
			return (error);
		if (vp->v_type == VDIR)
			error = EISDIR;
		vrele(vp);
	}
	return (error);
}

int
linux_pwrite(struct thread *td, struct linux_pwrite_args *uap)
{
	off_t offset;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	offset = PAIR32TO64(off_t, uap->offset);
#else
	offset = uap->offset;
#endif

	return (kern_pwrite(td, uap->fd, uap->buf, uap->nbyte, offset));
}

int
linux_preadv(struct thread *td, struct linux_preadv_args *uap)
{
	struct uio *auio;
	int error;
	off_t offset;

	/*
	 * According http://man7.org/linux/man-pages/man2/preadv.2.html#NOTES
	 * pos_l and pos_h, respectively, contain the
	 * low order and high order 32 bits of offset.
	 */
	offset = (((off_t)uap->pos_h << (sizeof(offset) * 4)) <<
	    (sizeof(offset) * 4)) | uap->pos_l;
	if (offset < 0)
		return (EINVAL);
#ifdef COMPAT_LINUX32
	error = linux32_copyinuio(PTRIN(uap->vec), uap->vlen, &auio);
#else
	error = copyinuio(uap->vec, uap->vlen, &auio);
#endif
	if (error != 0)
		return (error);
	error = kern_preadv(td, uap->fd, auio, offset);
	free(auio, M_IOV);
	return (error);
}

int
linux_pwritev(struct thread *td, struct linux_pwritev_args *uap)
{
	struct uio *auio;
	int error;
	off_t offset;

	/*
	 * According http://man7.org/linux/man-pages/man2/pwritev.2.html#NOTES
	 * pos_l and pos_h, respectively, contain the
	 * low order and high order 32 bits of offset.
	 */
	offset = (((off_t)uap->pos_h << (sizeof(offset) * 4)) <<
	    (sizeof(offset) * 4)) | uap->pos_l;
	if (offset < 0)
		return (EINVAL);
#ifdef COMPAT_LINUX32
	error = linux32_copyinuio(PTRIN(uap->vec), uap->vlen, &auio);
#else
	error = copyinuio(uap->vec, uap->vlen, &auio);
#endif
	if (error != 0)
		return (error);
	error = kern_pwritev(td, uap->fd, auio, offset);
	free(auio, M_IOV);
	return (error);
}

int
linux_mount(struct thread *td, struct linux_mount_args *args)
{
	struct mntarg *ma = NULL;
	char *fstypename, *mntonname, *mntfromname, *data;
	int error, fsflags;

	fstypename = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	mntonname = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	mntfromname = malloc(MNAMELEN, M_TEMP, M_WAITOK);
	data = NULL;
	error = copyinstr(args->filesystemtype, fstypename, MNAMELEN - 1,
	    NULL);
	if (error != 0)
		goto out;
	if (args->specialfile != NULL) {
		error = copyinstr(args->specialfile, mntfromname, MNAMELEN - 1, NULL);
		if (error != 0)
			goto out;
	} else {
		mntfromname[0] = '\0';
	}
	error = copyinstr(args->dir, mntonname, MNAMELEN - 1, NULL);
	if (error != 0)
		goto out;

	if (strcmp(fstypename, "ext2") == 0) {
		strcpy(fstypename, "ext2fs");
	} else if (strcmp(fstypename, "proc") == 0) {
		strcpy(fstypename, "linprocfs");
	} else if (strcmp(fstypename, "vfat") == 0) {
		strcpy(fstypename, "msdosfs");
	} else if (strcmp(fstypename, "fuse") == 0) {
		char *fuse_options, *fuse_option, *fuse_name;

		if (strcmp(mntfromname, "fuse") == 0)
			strcpy(mntfromname, "/dev/fuse");

		strcpy(fstypename, "fusefs");
		data = malloc(MNAMELEN, M_TEMP, M_WAITOK);
		error = copyinstr(args->data, data, MNAMELEN - 1, NULL);
		if (error != 0)
			goto out;

		fuse_options = data;
		while ((fuse_option = strsep(&fuse_options, ",")) != NULL) {
			fuse_name = strsep(&fuse_option, "=");
			if (fuse_name == NULL || fuse_option == NULL)
				goto out;
			ma = mount_arg(ma, fuse_name, fuse_option, -1);
		}

		/*
		 * The FUSE server uses Linux errno values instead of FreeBSD
		 * ones; add a flag to tell fuse(4) to do errno translation.
		 */
		ma = mount_arg(ma, "linux_errnos", "1", -1);
	}

	fsflags = 0;

	/*
	 * Linux SYNC flag is not included; the closest equivalent
	 * FreeBSD has is !ASYNC, which is our default.
	 */
	if (args->rwflag & LINUX_MS_RDONLY)
		fsflags |= MNT_RDONLY;
	if (args->rwflag & LINUX_MS_NOSUID)
		fsflags |= MNT_NOSUID;
	if (args->rwflag & LINUX_MS_NOEXEC)
		fsflags |= MNT_NOEXEC;
	if (args->rwflag & LINUX_MS_REMOUNT)
		fsflags |= MNT_UPDATE;

	ma = mount_arg(ma, "fstype", fstypename, -1);
	ma = mount_arg(ma, "fspath", mntonname, -1);
	ma = mount_arg(ma, "from", mntfromname, -1);
	error = kernel_mount(ma, fsflags);
out:
	free(fstypename, M_TEMP);
	free(mntonname, M_TEMP);
	free(mntfromname, M_TEMP);
	free(data, M_TEMP);
	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_oldumount(struct thread *td, struct linux_oldumount_args *args)
{

	return (kern_unmount(td, args->path, 0));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_umount(struct thread *td, struct linux_umount_args *args)
{
	int flags;

	flags = 0;
	if ((args->flags & LINUX_MNT_FORCE) != 0) {
		args->flags &= ~LINUX_MNT_FORCE;
		flags |= MNT_FORCE;
	}
	if (args->flags != 0) {
		linux_msg(td, "unsupported umount2 flags %#x", args->flags);
		return (EINVAL);
	}

	return (kern_unmount(td, args->path, flags));
}
#endif

/*
 * fcntl family of syscalls
 */

struct l_flock {
	l_short		l_type;
	l_short		l_whence;
	l_off_t		l_start;
	l_off_t		l_len;
	l_pid_t		l_pid;
}
#if defined(__amd64__) && defined(COMPAT_LINUX32)
__packed
#endif
;

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
	bsd_flock->l_sysid = 0;
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

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
struct l_flock64 {
	l_short		l_type;
	l_short		l_whence;
	l_loff_t	l_start;
	l_loff_t	l_len;
	l_pid_t		l_pid;
}
#if defined(__amd64__) && defined(COMPAT_LINUX32)
__packed
#endif
;

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
	bsd_flock->l_sysid = 0;
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
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

static int
fcntl_common(struct thread *td, struct linux_fcntl_args *args)
{
	struct l_flock linux_flock;
	struct flock bsd_flock;
	struct file *fp;
	long arg;
	int error, result;

	switch (args->cmd) {
	case LINUX_F_DUPFD:
		return (kern_fcntl(td, args->fd, F_DUPFD, args->arg));

	case LINUX_F_GETFD:
		return (kern_fcntl(td, args->fd, F_GETFD, 0));

	case LINUX_F_SETFD:
		return (kern_fcntl(td, args->fd, F_SETFD, args->arg));

	case LINUX_F_GETFL:
		error = kern_fcntl(td, args->fd, F_GETFL, 0);
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
			td->td_retval[0] |= LINUX_O_ASYNC;
#ifdef LINUX_O_NOFOLLOW
		if (result & O_NOFOLLOW)
			td->td_retval[0] |= LINUX_O_NOFOLLOW;
#endif
#ifdef LINUX_O_DIRECT
		if (result & O_DIRECT)
			td->td_retval[0] |= LINUX_O_DIRECT;
#endif
		return (error);

	case LINUX_F_SETFL:
		arg = 0;
		if (args->arg & LINUX_O_NDELAY)
			arg |= O_NONBLOCK;
		if (args->arg & LINUX_O_APPEND)
			arg |= O_APPEND;
		if (args->arg & LINUX_O_SYNC)
			arg |= O_FSYNC;
		if (args->arg & LINUX_O_ASYNC)
			arg |= O_ASYNC;
#ifdef LINUX_O_NOFOLLOW
		if (args->arg & LINUX_O_NOFOLLOW)
			arg |= O_NOFOLLOW;
#endif
#ifdef LINUX_O_DIRECT
		if (args->arg & LINUX_O_DIRECT)
			arg |= O_DIRECT;
#endif
		return (kern_fcntl(td, args->fd, F_SETFL, arg));

	case LINUX_F_GETLK:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, &bsd_flock);
		error = kern_fcntl(td, args->fd, F_GETLK, (intptr_t)&bsd_flock);
		if (error)
			return (error);
		bsd_to_linux_flock(&bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (void *)args->arg,
		    sizeof(linux_flock)));

	case LINUX_F_SETLK:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, &bsd_flock);
		return (kern_fcntl(td, args->fd, F_SETLK,
		    (intptr_t)&bsd_flock));

	case LINUX_F_SETLKW:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock(&linux_flock, &bsd_flock);
		return (kern_fcntl(td, args->fd, F_SETLKW,
		     (intptr_t)&bsd_flock));

	case LINUX_F_GETOWN:
		return (kern_fcntl(td, args->fd, F_GETOWN, 0));

	case LINUX_F_SETOWN:
		/*
		 * XXX some Linux applications depend on F_SETOWN having no
		 * significant effect for pipes (SIGIO is not delivered for
		 * pipes under Linux-2.2.35 at least).
		 */
		error = fget(td, args->fd,
		    &cap_fcntl_rights, &fp);
		if (error)
			return (error);
		if (fp->f_type == DTYPE_PIPE) {
			fdrop(fp, td);
			return (EINVAL);
		}
		fdrop(fp, td);

		return (kern_fcntl(td, args->fd, F_SETOWN, args->arg));

	case LINUX_F_DUPFD_CLOEXEC:
		return (kern_fcntl(td, args->fd, F_DUPFD_CLOEXEC, args->arg));
	/*
	 * Our F_SEAL_* values match Linux one for maximum compatibility.  So we
	 * only needed to account for different values for fcntl(2) commands.
	 */
	case LINUX_F_GET_SEALS:
		error = kern_fcntl(td, args->fd, F_GET_SEALS, 0);
		if (error != 0)
			return (error);
		td->td_retval[0] = bsd_to_linux_bits(td->td_retval[0],
		    seal_bitmap, 0);
		return (0);

	case LINUX_F_ADD_SEALS:
		return (kern_fcntl(td, args->fd, F_ADD_SEALS,
		    linux_to_bsd_bits(args->arg, seal_bitmap, 0)));
	default:
		linux_msg(td, "unsupported fcntl cmd %d", args->cmd);
		return (EINVAL);
	}
}

int
linux_fcntl(struct thread *td, struct linux_fcntl_args *args)
{

	return (fcntl_common(td, args));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_fcntl64(struct thread *td, struct linux_fcntl64_args *args)
{
	struct l_flock64 linux_flock;
	struct flock bsd_flock;
	struct linux_fcntl_args fcntl_args;
	int error;

	switch (args->cmd) {
	case LINUX_F_GETLK64:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, &bsd_flock);
		error = kern_fcntl(td, args->fd, F_GETLK, (intptr_t)&bsd_flock);
		if (error)
			return (error);
		bsd_to_linux_flock64(&bsd_flock, &linux_flock);
		return (copyout(&linux_flock, (void *)args->arg,
			    sizeof(linux_flock)));

	case LINUX_F_SETLK64:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, &bsd_flock);
		return (kern_fcntl(td, args->fd, F_SETLK,
		    (intptr_t)&bsd_flock));

	case LINUX_F_SETLKW64:
		error = copyin((void *)args->arg, &linux_flock,
		    sizeof(linux_flock));
		if (error)
			return (error);
		linux_to_bsd_flock64(&linux_flock, &bsd_flock);
		return (kern_fcntl(td, args->fd, F_SETLKW,
		    (intptr_t)&bsd_flock));
	}

	fcntl_args.fd = args->fd;
	fcntl_args.cmd = args->cmd;
	fcntl_args.arg = args->arg;
	return (fcntl_common(td, &fcntl_args));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_chown(struct thread *td, struct linux_chown_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_fchownat(td, AT_FDCWD, args->path, UIO_USERSPACE,
		    args->uid, args->gid, 0));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_fchownat(td, AT_FDCWD, path, UIO_SYSSPACE, args->uid,
	    args->gid, 0);
	LFREEPATH(path);
	return (error);
}
#endif

int
linux_fchownat(struct thread *td, struct linux_fchownat_args *args)
{
	char *path;
	int error, dfd, flag;

	if (args->flag & ~LINUX_AT_SYMLINK_NOFOLLOW)
		return (EINVAL);

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD :  args->dfd;
	flag = (args->flag & LINUX_AT_SYMLINK_NOFOLLOW) == 0 ? 0 :
	    AT_SYMLINK_NOFOLLOW;
	if (!LUSECONVPATH(td)) {
		return (kern_fchownat(td, dfd, args->filename, UIO_USERSPACE,
		    args->uid, args->gid, flag));
	}
	LCONVPATHEXIST_AT(td, args->filename, &path, dfd);
	error = kern_fchownat(td, dfd, path, UIO_SYSSPACE, args->uid, args->gid,
	    flag);
	LFREEPATH(path);
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_lchown(struct thread *td, struct linux_lchown_args *args)
{
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		return (kern_fchownat(td, AT_FDCWD, args->path, UIO_USERSPACE, args->uid,
		    args->gid, AT_SYMLINK_NOFOLLOW));
	}
	LCONVPATHEXIST(td, args->path, &path);
	error = kern_fchownat(td, AT_FDCWD, path, UIO_SYSSPACE, args->uid, args->gid,
	    AT_SYMLINK_NOFOLLOW);
	LFREEPATH(path);
	return (error);
}
#endif

static int
convert_fadvice(int advice)
{
	switch (advice) {
	case LINUX_POSIX_FADV_NORMAL:
		return (POSIX_FADV_NORMAL);
	case LINUX_POSIX_FADV_RANDOM:
		return (POSIX_FADV_RANDOM);
	case LINUX_POSIX_FADV_SEQUENTIAL:
		return (POSIX_FADV_SEQUENTIAL);
	case LINUX_POSIX_FADV_WILLNEED:
		return (POSIX_FADV_WILLNEED);
	case LINUX_POSIX_FADV_DONTNEED:
		return (POSIX_FADV_DONTNEED);
	case LINUX_POSIX_FADV_NOREUSE:
		return (POSIX_FADV_NOREUSE);
	default:
		return (-1);
	}
}

int
linux_fadvise64(struct thread *td, struct linux_fadvise64_args *args)
{
	off_t offset;
	int advice;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	offset = PAIR32TO64(off_t, args->offset);
#else
	offset = args->offset;
#endif

	advice = convert_fadvice(args->advice);
	if (advice == -1)
		return (EINVAL);
	return (kern_posix_fadvise(td, args->fd, offset, args->len, advice));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_fadvise64_64(struct thread *td, struct linux_fadvise64_64_args *args)
{
	off_t len, offset;
	int advice;

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	len = PAIR32TO64(off_t, args->len);
	offset = PAIR32TO64(off_t, args->offset);
#else
	len = args->len;
	offset = args->offset;
#endif

	advice = convert_fadvice(args->advice);
	if (advice == -1)
		return (EINVAL);
	return (kern_posix_fadvise(td, args->fd, offset, len, advice));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_pipe(struct thread *td, struct linux_pipe_args *args)
{
	int fildes[2];
	int error;

	error = kern_pipe(td, fildes, 0, NULL, NULL);
	if (error != 0)
		return (error);

	error = copyout(fildes, args->pipefds, sizeof(fildes));
	if (error != 0) {
		(void)kern_close(td, fildes[0]);
		(void)kern_close(td, fildes[1]);
	}

	return (error);
}
#endif

int
linux_pipe2(struct thread *td, struct linux_pipe2_args *args)
{
	int fildes[2];
	int error, flags;

	if ((args->flags & ~(LINUX_O_NONBLOCK | LINUX_O_CLOEXEC)) != 0)
		return (EINVAL);

	flags = 0;
	if ((args->flags & LINUX_O_NONBLOCK) != 0)
		flags |= O_NONBLOCK;
	if ((args->flags & LINUX_O_CLOEXEC) != 0)
		flags |= O_CLOEXEC;
	error = kern_pipe(td, fildes, flags, NULL, NULL);
	if (error != 0)
		return (error);

	error = copyout(fildes, args->pipefds, sizeof(fildes));
	if (error != 0) {
		(void)kern_close(td, fildes[0]);
		(void)kern_close(td, fildes[1]);
	}

	return (error);
}

int
linux_dup3(struct thread *td, struct linux_dup3_args *args)
{
	int cmd;
	intptr_t newfd;

	if (args->oldfd == args->newfd)
		return (EINVAL);
	if ((args->flags & ~LINUX_O_CLOEXEC) != 0)
		return (EINVAL);
	if (args->flags & LINUX_O_CLOEXEC)
		cmd = F_DUP2FD_CLOEXEC;
	else
		cmd = F_DUP2FD;

	newfd = args->newfd;
	return (kern_fcntl(td, args->oldfd, cmd, newfd));
}

int
linux_fallocate(struct thread *td, struct linux_fallocate_args *args)
{
	off_t len, offset;

	/*
	 * We emulate only posix_fallocate system call for which
	 * mode should be 0.
	 */
	if (args->mode != 0)
		return (EOPNOTSUPP);

#if defined(__amd64__) && defined(COMPAT_LINUX32)
	len = PAIR32TO64(off_t, args->len);
	offset = PAIR32TO64(off_t, args->offset);
#else
	len = args->len;
	offset = args->offset;
#endif

	return (kern_posix_fallocate(td, args->fd, offset, len));
}

int
linux_copy_file_range(struct thread *td, struct linux_copy_file_range_args
    *args)
{
	l_loff_t inoff, outoff, *inoffp, *outoffp;
	int error, flags;

	/*
	 * copy_file_range(2) on Linux doesn't define any flags (yet), so is
	 * the native implementation.  Enforce it.
	 */
	if (args->flags != 0) {
		linux_msg(td, "copy_file_range unsupported flags 0x%x",
		    args->flags);
		return (EINVAL);
	}
	flags = 0;
	inoffp = outoffp = NULL;
	if (args->off_in != NULL) {
		error = copyin(args->off_in, &inoff, sizeof(l_loff_t));
		if (error != 0)
			return (error);
		inoffp = &inoff;
	}
	if (args->off_out != NULL) {
		error = copyin(args->off_out, &outoff, sizeof(l_loff_t));
		if (error != 0)
			return (error);
		outoffp = &outoff;
	}

	error = kern_copy_file_range(td, args->fd_in, inoffp, args->fd_out,
	    outoffp, args->len, flags);
	if (error == 0 && args->off_in != NULL)
		error = copyout(inoffp, args->off_in, sizeof(l_loff_t));
	if (error == 0 && args->off_out != NULL)
		error = copyout(outoffp, args->off_out, sizeof(l_loff_t));
	return (error);
}

#define	LINUX_MEMFD_PREFIX	"memfd:"

int
linux_memfd_create(struct thread *td, struct linux_memfd_create_args *args)
{
	char memfd_name[LINUX_NAME_MAX + 1];
	int error, flags, shmflags, oflags;

	/*
	 * This is our clever trick to avoid the heap allocation to copy in the
	 * uname.  We don't really need to go this far out of our way, but it
	 * does keep the rest of this function fairly clean as they don't have
	 * to worry about cleanup on the way out.
	 */
	error = copyinstr(args->uname_ptr,
	    memfd_name + sizeof(LINUX_MEMFD_PREFIX) - 1,
	    LINUX_NAME_MAX - sizeof(LINUX_MEMFD_PREFIX) - 1, NULL);
	if (error != 0) {
		if (error == ENAMETOOLONG)
			error = EINVAL;
		return (error);
	}

	memcpy(memfd_name, LINUX_MEMFD_PREFIX, sizeof(LINUX_MEMFD_PREFIX) - 1);
	flags = linux_to_bsd_bits(args->flags, mfd_bitmap, 0);
	if ((flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_HUGETLB |
	    MFD_HUGE_MASK)) != 0)
		return (EINVAL);
	/* Size specified but no HUGETLB. */
	if ((flags & MFD_HUGE_MASK) != 0 && (flags & MFD_HUGETLB) == 0)
		return (EINVAL);
	/* We don't actually support HUGETLB. */
	if ((flags & MFD_HUGETLB) != 0)
		return (ENOSYS);
	oflags = O_RDWR;
	shmflags = SHM_GROW_ON_WRITE;
	if ((flags & MFD_CLOEXEC) != 0)
		oflags |= O_CLOEXEC;
	if ((flags & MFD_ALLOW_SEALING) != 0)
		shmflags |= SHM_ALLOW_SEALING;
	return (kern_shm_open2(td, SHM_ANON, oflags, 0, shmflags, NULL,
	    memfd_name));
}

int
linux_splice(struct thread *td, struct linux_splice_args *args)
{

	linux_msg(td, "syscall splice not really implemented");

	/*
	 * splice(2) is documented to return EINVAL in various circumstances;
	 * returning it instead of ENOSYS should hint the caller to use fallback
	 * instead.
	 */
	return (EINVAL);
}
