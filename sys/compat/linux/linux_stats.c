/*-
 * SPDX-License-Identifier: BSD-2-Clause
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

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/dirent.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <security/audit/audit.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux.h>
#include <compat/linux/linux_file.h>
#include <compat/linux/linux_util.h>


static int
linux_kern_fstat(struct thread *td, int fd, struct stat *sbp)
{
	struct vnode *vp;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);

	error = fget(td, fd, &cap_fstat_rights, &fp);
	if (__predict_false(error != 0))
		return (error);

	AUDIT_ARG_FILE(td->td_proc, fp);

	error = fo_stat(fp, sbp, td->td_ucred);
	if (error == 0 && (vp = fp->f_vnode) != NULL)
		translate_vnhook_major_minor(vp, sbp);
	fdrop(fp, td);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrstat_error(sbp, error);
#endif
	return (error);
}

static int
linux_kern_statat(struct thread *td, int flag, int fd, const char *path,
    enum uio_seg pathseg, struct stat *sbp)
{
	struct nameidata nd;
	int error;

	if ((flag & ~(AT_SYMLINK_NOFOLLOW | AT_RESOLVE_BENEATH |
	    AT_EMPTY_PATH)) != 0)
		return (EINVAL);

	NDINIT_ATRIGHTS(&nd, LOOKUP, at2cnpflags(flag, AT_RESOLVE_BENEATH |
	    AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH) | LOCKSHARED | LOCKLEAF |
	    AUDITVNODE1, pathseg, path, fd, &cap_fstat_rights);

	if ((error = namei(&nd)) != 0) {
		if (error == ENOTDIR &&
		    (nd.ni_resflags & NIRES_EMPTYPATH) != 0)
			error = linux_kern_fstat(td, fd, sbp);
		return (error);
	}
	error = VOP_STAT(nd.ni_vp, sbp, td->td_ucred, NOCRED);
	if (error == 0)
		translate_vnhook_major_minor(nd.ni_vp, sbp);
	NDFREE_PNBUF(&nd);
	vput(nd.ni_vp);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrstat_error(sbp, error);
#endif
	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
static int
linux_kern_stat(struct thread *td, const char *path, enum uio_seg pathseg,
    struct stat *sbp)
{

	return (linux_kern_statat(td, 0, AT_FDCWD, path, pathseg, sbp));
}

static int
linux_kern_lstat(struct thread *td, const char *path, enum uio_seg pathseg,
    struct stat *sbp)
{

	return (linux_kern_statat(td, AT_SYMLINK_NOFOLLOW, AT_FDCWD, path,
	    pathseg, sbp));
}
#endif

static int
newstat_copyout(struct stat *buf, void *ubuf)
{
	struct l_newstat tbuf;

	bzero(&tbuf, sizeof(tbuf));
	tbuf.st_dev = linux_new_encode_dev(buf->st_dev);
	tbuf.st_ino = buf->st_ino;
	tbuf.st_mode = buf->st_mode;
	tbuf.st_nlink = buf->st_nlink;
	tbuf.st_uid = buf->st_uid;
	tbuf.st_gid = buf->st_gid;
	tbuf.st_rdev = linux_new_encode_dev(buf->st_rdev);
	tbuf.st_size = buf->st_size;
	tbuf.st_atim.tv_sec = buf->st_atim.tv_sec;
	tbuf.st_atim.tv_nsec = buf->st_atim.tv_nsec;
	tbuf.st_mtim.tv_sec = buf->st_mtim.tv_sec;
	tbuf.st_mtim.tv_nsec = buf->st_mtim.tv_nsec;
	tbuf.st_ctim.tv_sec = buf->st_ctim.tv_sec;
	tbuf.st_ctim.tv_nsec = buf->st_ctim.tv_nsec;
	tbuf.st_blksize = buf->st_blksize;
	tbuf.st_blocks = buf->st_blocks;

	return (copyout(&tbuf, ubuf, sizeof(tbuf)));
}


#ifdef LINUX_LEGACY_SYSCALLS
int
linux_newstat(struct thread *td, struct linux_newstat_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_stat(td, args->path, UIO_USERSPACE, &buf);
	if (error)
		return (error);
	return (newstat_copyout(&buf, args->buf));
}

int
linux_newlstat(struct thread *td, struct linux_newlstat_args *args)
{
	struct stat sb;
	int error;

	error = linux_kern_lstat(td, args->path, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	return (newstat_copyout(&sb, args->buf));
}
#endif

int
linux_newfstat(struct thread *td, struct linux_newfstat_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_fstat(td, args->fd, &buf);
	if (!error)
		error = newstat_copyout(&buf, args->buf);

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))

static __inline uint16_t
linux_old_encode_dev(dev_t _dev)
{

	return (_dev == NODEV ? 0 : linux_encode_dev(major(_dev), minor(_dev)));
}

static int
old_stat_copyout(struct stat *buf, void *ubuf)
{
	struct l_old_stat lbuf;

	bzero(&lbuf, sizeof(lbuf));
	lbuf.st_dev = linux_old_encode_dev(buf->st_dev);
	lbuf.st_ino = buf->st_ino;
	lbuf.st_mode = buf->st_mode;
	lbuf.st_nlink = buf->st_nlink;
	lbuf.st_uid = buf->st_uid;
	lbuf.st_gid = buf->st_gid;
	lbuf.st_rdev = linux_old_encode_dev(buf->st_rdev);
	lbuf.st_size = MIN(buf->st_size, INT32_MAX);
	lbuf.st_atim.tv_sec = buf->st_atim.tv_sec;
	lbuf.st_atim.tv_nsec = buf->st_atim.tv_nsec;
	lbuf.st_mtim.tv_sec = buf->st_mtim.tv_sec;
	lbuf.st_mtim.tv_nsec = buf->st_mtim.tv_nsec;
	lbuf.st_ctim.tv_sec = buf->st_ctim.tv_sec;
	lbuf.st_ctim.tv_nsec = buf->st_ctim.tv_nsec;
	lbuf.st_blksize = buf->st_blksize;
	lbuf.st_blocks = buf->st_blocks;
	lbuf.st_flags = buf->st_flags;
	lbuf.st_gen = buf->st_gen;

	return (copyout(&lbuf, ubuf, sizeof(lbuf)));
}

int
linux_stat(struct thread *td, struct linux_stat_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_stat(td, args->path, UIO_USERSPACE, &buf);
	if (error) {
		return (error);
	}
	return (old_stat_copyout(&buf, args->up));
}

int
linux_lstat(struct thread *td, struct linux_lstat_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_lstat(td, args->path, UIO_USERSPACE, &buf);
	if (error) {
		return (error);
	}
	return (old_stat_copyout(&buf, args->up));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

struct l_statfs {
	l_long		f_type;
	l_long		f_bsize;
	l_long		f_blocks;
	l_long		f_bfree;
	l_long		f_bavail;
	l_long		f_files;
	l_long		f_ffree;
	l_fsid_t	f_fsid;
	l_long		f_namelen;
	l_long		f_frsize;
	l_long		f_flags;
	l_long		f_spare[4];
};

#define	LINUX_CODA_SUPER_MAGIC	0x73757245L
#define	LINUX_EXT2_SUPER_MAGIC	0xEF53L
#define	LINUX_HPFS_SUPER_MAGIC	0xf995e849L
#define	LINUX_ISOFS_SUPER_MAGIC	0x9660L
#define	LINUX_MSDOS_SUPER_MAGIC	0x4d44L
#define	LINUX_NCP_SUPER_MAGIC	0x564cL
#define	LINUX_NFS_SUPER_MAGIC	0x6969L
#define	LINUX_NTFS_SUPER_MAGIC	0x5346544EL
#define	LINUX_PROC_SUPER_MAGIC	0x9fa0L
#define	LINUX_UFS_SUPER_MAGIC	0x00011954L	/* XXX - UFS_MAGIC in Linux */
#define	LINUX_ZFS_SUPER_MAGIC	0x2FC12FC1
#define	LINUX_DEVFS_SUPER_MAGIC	0x1373L
#define	LINUX_SHMFS_MAGIC	0x01021994

static long
bsd_to_linux_ftype(const char *fstypename)
{
	int i;
	static struct {const char *bsd_name; long linux_type;} b2l_tbl[] = {
		{"ufs",     LINUX_UFS_SUPER_MAGIC},
		{"zfs",     LINUX_ZFS_SUPER_MAGIC},
		{"cd9660",  LINUX_ISOFS_SUPER_MAGIC},
		{"nfs",     LINUX_NFS_SUPER_MAGIC},
		{"ext2fs",  LINUX_EXT2_SUPER_MAGIC},
		{"procfs",  LINUX_PROC_SUPER_MAGIC},
		{"msdosfs", LINUX_MSDOS_SUPER_MAGIC},
		{"ntfs",    LINUX_NTFS_SUPER_MAGIC},
		{"nwfs",    LINUX_NCP_SUPER_MAGIC},
		{"hpfs",    LINUX_HPFS_SUPER_MAGIC},
		{"coda",    LINUX_CODA_SUPER_MAGIC},
		{"devfs",   LINUX_DEVFS_SUPER_MAGIC},
		{"tmpfs",   LINUX_SHMFS_MAGIC},
		{NULL,      0L}};

	for (i = 0; b2l_tbl[i].bsd_name != NULL; i++)
		if (strcmp(b2l_tbl[i].bsd_name, fstypename) == 0)
			return (b2l_tbl[i].linux_type);

	return (0L);
}

static int
bsd_to_linux_mnt_flags(int f_flags)
{
	int flags = LINUX_ST_VALID;

	if (f_flags & MNT_RDONLY)
		flags |= LINUX_ST_RDONLY;
	if (f_flags & MNT_NOEXEC)
		flags |= LINUX_ST_NOEXEC;
	if (f_flags & MNT_NOSUID)
		flags |= LINUX_ST_NOSUID;
	if (f_flags & MNT_NOATIME)
		flags |= LINUX_ST_NOATIME;
	if (f_flags & MNT_NOSYMFOLLOW)
		flags |= LINUX_ST_NOSYMFOLLOW;
	if (f_flags & MNT_SYNCHRONOUS)
		flags |= LINUX_ST_SYNCHRONOUS;

	return (flags);
}

static int
bsd_to_linux_statfs(struct statfs *bsd_statfs, struct l_statfs *linux_statfs)
{

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
	statfs_scale_blocks(bsd_statfs, INT32_MAX);
#endif
	linux_statfs->f_type = bsd_to_linux_ftype(bsd_statfs->f_fstypename);
	linux_statfs->f_bsize = bsd_statfs->f_bsize;
	linux_statfs->f_blocks = bsd_statfs->f_blocks;
	linux_statfs->f_bfree = bsd_statfs->f_bfree;
	linux_statfs->f_bavail = bsd_statfs->f_bavail;
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
	linux_statfs->f_ffree = MIN(bsd_statfs->f_ffree, INT32_MAX);
	linux_statfs->f_files = MIN(bsd_statfs->f_files, INT32_MAX);
#else
	linux_statfs->f_ffree = bsd_statfs->f_ffree;
	linux_statfs->f_files = bsd_statfs->f_files;
#endif
	linux_statfs->f_fsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs->f_fsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs->f_namelen = MAXNAMLEN;
	linux_statfs->f_frsize = bsd_statfs->f_bsize;
	linux_statfs->f_flags = bsd_to_linux_mnt_flags(bsd_statfs->f_flags);
	memset(linux_statfs->f_spare, 0, sizeof(linux_statfs->f_spare));

	return (0);
}

int
linux_statfs(struct thread *td, struct linux_statfs_args *args)
{
	struct l_statfs linux_statfs;
	struct statfs *bsd_statfs;
	int error;

	bsd_statfs = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_statfs(td, args->path, UIO_USERSPACE, bsd_statfs);
	if (error == 0)
		error = bsd_to_linux_statfs(bsd_statfs, &linux_statfs);
	free(bsd_statfs, M_STATFS);
	if (error != 0)
		return (error);
	return (copyout(&linux_statfs, args->buf, sizeof(linux_statfs)));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
static void
bsd_to_linux_statfs64(struct statfs *bsd_statfs, struct l_statfs64 *linux_statfs)
{

	linux_statfs->f_type = bsd_to_linux_ftype(bsd_statfs->f_fstypename);
	linux_statfs->f_bsize = bsd_statfs->f_bsize;
	linux_statfs->f_blocks = bsd_statfs->f_blocks;
	linux_statfs->f_bfree = bsd_statfs->f_bfree;
	linux_statfs->f_bavail = bsd_statfs->f_bavail;
	linux_statfs->f_ffree = bsd_statfs->f_ffree;
	linux_statfs->f_files = bsd_statfs->f_files;
	linux_statfs->f_fsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs->f_fsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs->f_namelen = MAXNAMLEN;
	linux_statfs->f_frsize = bsd_statfs->f_bsize;
	linux_statfs->f_flags = bsd_to_linux_mnt_flags(bsd_statfs->f_flags);
	memset(linux_statfs->f_spare, 0, sizeof(linux_statfs->f_spare));
}

int
linux_statfs64(struct thread *td, struct linux_statfs64_args *args)
{
	struct l_statfs64 linux_statfs;
	struct statfs *bsd_statfs;
	int error;

	if (args->bufsize != sizeof(struct l_statfs64))
		return (EINVAL);

	bsd_statfs = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_statfs(td, args->path, UIO_USERSPACE, bsd_statfs);
	if (error == 0)
		bsd_to_linux_statfs64(bsd_statfs, &linux_statfs);
	free(bsd_statfs, M_STATFS);
	if (error != 0)
		return (error);
	return (copyout(&linux_statfs, args->buf, sizeof(linux_statfs)));
}

int
linux_fstatfs64(struct thread *td, struct linux_fstatfs64_args *args)
{
	struct l_statfs64 linux_statfs;
	struct statfs *bsd_statfs;
	int error;

	if (args->bufsize != sizeof(struct l_statfs64))
		return (EINVAL);

	bsd_statfs = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_fstatfs(td, args->fd, bsd_statfs);
	if (error == 0)
		bsd_to_linux_statfs64(bsd_statfs, &linux_statfs);
	free(bsd_statfs, M_STATFS);
	if (error != 0)
		return (error);
	return (copyout(&linux_statfs, args->buf, sizeof(linux_statfs)));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_fstatfs(struct thread *td, struct linux_fstatfs_args *args)
{
	struct l_statfs linux_statfs;
	struct statfs *bsd_statfs;
	int error;

	bsd_statfs = malloc(sizeof(struct statfs), M_STATFS, M_WAITOK);
	error = kern_fstatfs(td, args->fd, bsd_statfs);
	if (error == 0)
		error = bsd_to_linux_statfs(bsd_statfs, &linux_statfs);
	free(bsd_statfs, M_STATFS);
	if (error != 0)
		return (error);
	return (copyout(&linux_statfs, args->buf, sizeof(linux_statfs)));
}

struct l_ustat
{
	l_daddr_t	f_tfree;
	l_ino_t		f_tinode;
	char		f_fname[6];
	char		f_fpack[6];
};

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_ustat(struct thread *td, struct linux_ustat_args *args)
{

	return (EOPNOTSUPP);
}
#endif

/*
 * Convert Linux stat flags to BSD flags.  Return value indicates successful
 * conversion (no unknown flags).
 */
static bool
linux_to_bsd_stat_flags(int linux_flags, int *out_flags)
{
	int flags, unsupported;

	unsupported = linux_flags & ~(LINUX_AT_SYMLINK_NOFOLLOW |
	    LINUX_AT_EMPTY_PATH | LINUX_AT_NO_AUTOMOUNT);
	if (unsupported != 0) {
		*out_flags = unsupported;
		return (false);
	}

	flags = 0;
	if (linux_flags & LINUX_AT_SYMLINK_NOFOLLOW)
		flags |= AT_SYMLINK_NOFOLLOW;
	if (linux_flags & LINUX_AT_EMPTY_PATH)
		flags |= AT_EMPTY_PATH;
	*out_flags = flags;
	return (true);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))

static int
stat64_copyout(struct stat *buf, void *ubuf)
{
	struct l_stat64 lbuf;

	bzero(&lbuf, sizeof(lbuf));
	lbuf.st_dev = linux_new_encode_dev(buf->st_dev);
	lbuf.st_ino = buf->st_ino;
	lbuf.st_mode = buf->st_mode;
	lbuf.st_nlink = buf->st_nlink;
	lbuf.st_uid = buf->st_uid;
	lbuf.st_gid = buf->st_gid;
	lbuf.st_rdev = linux_new_encode_dev(buf->st_rdev);
	lbuf.st_size = buf->st_size;
	lbuf.st_atim.tv_sec = buf->st_atim.tv_sec;
	lbuf.st_atim.tv_nsec = buf->st_atim.tv_nsec;
	lbuf.st_mtim.tv_sec = buf->st_mtim.tv_sec;
	lbuf.st_mtim.tv_nsec = buf->st_mtim.tv_nsec;
	lbuf.st_ctim.tv_sec = buf->st_ctim.tv_sec;
	lbuf.st_ctim.tv_nsec = buf->st_ctim.tv_nsec;
	lbuf.st_blksize = buf->st_blksize;
	lbuf.st_blocks = buf->st_blocks;

	/*
	 * The __st_ino field makes all the difference. In the Linux kernel
	 * it is conditionally compiled based on STAT64_HAS_BROKEN_ST_INO,
	 * but without the assignment to __st_ino the runtime linker refuses
	 * to mmap(2) any shared libraries. I guess it's broken alright :-)
	 */
	lbuf.__st_ino = buf->st_ino;

	return (copyout(&lbuf, ubuf, sizeof(lbuf)));
}

int
linux_stat64(struct thread *td, struct linux_stat64_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_stat(td, args->filename, UIO_USERSPACE, &buf);
	if (error)
		return (error);
	return (stat64_copyout(&buf, args->statbuf));
}

int
linux_lstat64(struct thread *td, struct linux_lstat64_args *args)
{
	struct stat sb;
	int error;

	error = linux_kern_lstat(td, args->filename, UIO_USERSPACE, &sb);
	if (error)
		return (error);
	return (stat64_copyout(&sb, args->statbuf));
}

int
linux_fstat64(struct thread *td, struct linux_fstat64_args *args)
{
	struct stat buf;
	int error;

	error = linux_kern_fstat(td, args->fd, &buf);
	if (!error)
		error = stat64_copyout(&buf, args->statbuf);

	return (error);
}

int
linux_fstatat64(struct thread *td, struct linux_fstatat64_args *args)
{
	int error, dfd, flags;
	struct stat buf;

	if (!linux_to_bsd_stat_flags(args->flag, &flags)) {
		linux_msg(td, "fstatat64 unsupported flags 0x%x", flags);
		return (EINVAL);
	}

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	error = linux_kern_statat(td, flags, dfd, args->pathname,
	    UIO_USERSPACE, &buf);
	if (error == 0)
		error = stat64_copyout(&buf, args->statbuf);

	return (error);
}

#else /* __amd64__ && !COMPAT_LINUX32 */

int
linux_newfstatat(struct thread *td, struct linux_newfstatat_args *args)
{
	int error, dfd, flags;
	struct stat buf;

	if (!linux_to_bsd_stat_flags(args->flag, &flags)) {
		linux_msg(td, "fstatat unsupported flags 0x%x", flags);
		return (EINVAL);
	}

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	error = linux_kern_statat(td, flags, dfd, args->pathname,
	    UIO_USERSPACE, &buf);
	if (error == 0)
		error = newstat_copyout(&buf, args->statbuf);

	return (error);
}

#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_syncfs(struct thread *td, struct linux_syncfs_args *args)
{
	struct mount *mp;
	struct vnode *vp;
	int error, save;

	error = fgetvp(td, args->fd, &cap_fsync_rights, &vp);
	if (error != 0)
		/*
		 * Linux syncfs() returns only EBADF, however fgetvp()
		 * can return EINVAL in case of file descriptor does
		 * not represent a vnode. XXX.
		 */
		return (error);

	mp = vp->v_mount;
	mtx_lock(&mountlist_mtx);
	error = vfs_busy(mp, MBF_MNTLSTLOCK);
	if (error != 0) {
		/* See comment above. */
		mtx_unlock(&mountlist_mtx);
		goto out;
	}
	if ((mp->mnt_flag & MNT_RDONLY) == 0 &&
	    vn_start_write(NULL, &mp, V_NOWAIT) == 0) {
		save = curthread_pflags_set(TDP_SYNCIO);
		vfs_periodic(mp, MNT_NOWAIT);
		VFS_SYNC(mp, MNT_NOWAIT);
		curthread_pflags_restore(save);
		vn_finished_write(mp);
	}
	vfs_unbusy(mp);

 out:
	vrele(vp);
	return (error);
}

static int
statx_copyout(struct stat *buf, void *ubuf)
{
	struct l_statx tbuf;

	bzero(&tbuf, sizeof(tbuf));
	tbuf.stx_mask = STATX_ALL;
	tbuf.stx_blksize = buf->st_blksize;
	tbuf.stx_attributes = 0;
	tbuf.stx_nlink = buf->st_nlink;
	tbuf.stx_uid = buf->st_uid;
	tbuf.stx_gid = buf->st_gid;
	tbuf.stx_mode = buf->st_mode;
	tbuf.stx_ino = buf->st_ino;
	tbuf.stx_size = buf->st_size;
	tbuf.stx_blocks = buf->st_blocks;

	tbuf.stx_atime.tv_sec = buf->st_atim.tv_sec;
	tbuf.stx_atime.tv_nsec = buf->st_atim.tv_nsec;
	tbuf.stx_btime.tv_sec = buf->st_birthtim.tv_sec;
	tbuf.stx_btime.tv_nsec = buf->st_birthtim.tv_nsec;
	tbuf.stx_ctime.tv_sec = buf->st_ctim.tv_sec;
	tbuf.stx_ctime.tv_nsec = buf->st_ctim.tv_nsec;
	tbuf.stx_mtime.tv_sec = buf->st_mtim.tv_sec;
	tbuf.stx_mtime.tv_nsec = buf->st_mtim.tv_nsec;
	tbuf.stx_rdev_major = linux_encode_major(buf->st_rdev);
	tbuf.stx_rdev_minor = linux_encode_minor(buf->st_rdev);
	tbuf.stx_dev_major = linux_encode_major(buf->st_dev);
	tbuf.stx_dev_minor = linux_encode_minor(buf->st_dev);

	return (copyout(&tbuf, ubuf, sizeof(tbuf)));
}

int
linux_statx(struct thread *td, struct linux_statx_args *args)
{
	int error, dirfd, flags;
	struct stat buf;

	if (!linux_to_bsd_stat_flags(args->flags, &flags)) {
		linux_msg(td, "statx unsupported flags 0x%x", flags);
		return (EINVAL);
	}

	dirfd = (args->dirfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dirfd;
	error = linux_kern_statat(td, flags, dirfd, args->pathname,
	    UIO_USERSPACE, &buf);
	if (error == 0)
		error = statx_copyout(&buf, args->statxbuf);

	return (error);
}
