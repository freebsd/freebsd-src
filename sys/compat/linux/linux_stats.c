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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_util.h>

static int
newstat_copyout(struct stat *buf, void *ubuf)
{
	struct l_newstat tbuf;
	struct cdevsw *cdevsw;
	dev_t dev;

	tbuf.st_dev = uminor(buf->st_dev) | (umajor(buf->st_dev) << 8);
	tbuf.st_ino = buf->st_ino;
	tbuf.st_mode = buf->st_mode;
	tbuf.st_nlink = buf->st_nlink;
	tbuf.st_uid = buf->st_uid;
	tbuf.st_gid = buf->st_gid;
	tbuf.st_rdev = buf->st_rdev;
	tbuf.st_size = buf->st_size;
	tbuf.st_atime = buf->st_atime;
	tbuf.st_mtime = buf->st_mtime;
	tbuf.st_ctime = buf->st_ctime;
	tbuf.st_blksize = buf->st_blksize;
	tbuf.st_blocks = buf->st_blocks;

	/* Lie about disk drives which are character devices
	 * in FreeBSD but block devices under Linux.
	 */
	if (S_ISCHR(tbuf.st_mode) &&
	    (dev = udev2dev(buf->st_rdev, 0)) != NODEV) {
		cdevsw = devsw(dev);
		if (cdevsw != NULL && (cdevsw->d_flags & D_DISK)) {
			tbuf.st_mode &= ~S_IFMT;
			tbuf.st_mode |= S_IFBLK;

			/* XXX this may not be quite right */
			/* Map major number to 0 */
			tbuf.st_dev = uminor(buf->st_dev) & 0xf;
			tbuf.st_rdev = buf->st_rdev & 0xff;
		}
	}

	return (copyout(&tbuf, ubuf, sizeof(tbuf)));
}

int
linux_newstat(struct thread *td, struct linux_newstat_args *args)
{
	struct stat buf;
	struct nameidata nd;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(newstat))
		printf(ARGS(newstat, "%s, *"), args->path);
#endif

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	    args->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = vn_stat(nd.ni_vp, &buf, td);
	vput(nd.ni_vp);
	if (error)
		return (error);

	return (newstat_copyout(&buf, args->buf));
}

int
linux_newlstat(struct thread *td, struct linux_newlstat_args *args)
{
	int error;
	struct stat sb;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(newlstat))
		printf(ARGS(newlstat, "%s, *"), args->path);
#endif

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	    args->path, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF); 

	error = vn_stat(nd.ni_vp, &sb, td);
	vput(nd.ni_vp);
	if (error)
		return (error);

	return (newstat_copyout(&sb, args->buf));
}

int
linux_newfstat(struct thread *td, struct linux_newfstat_args *args)
{
	struct file *fp;
	struct stat buf;
	int error;

#ifdef DEBUG
	if (ldebug(newfstat))
		printf(ARGS(newfstat, "%d, *"), args->fd);
#endif

	fp = ffind_hold(td, args->fd);
	if (fp == NULL)
		return (EBADF);

	error = fo_stat(fp, &buf, td);
	fdrop(fp, td);
	if (!error)
		error = newstat_copyout(&buf, args->buf);

	return (error);
}

/* XXX - All fields of type l_int are defined as l_long on i386 */
struct l_statfs {
	l_int		f_type;
	l_int		f_bsize;
	l_int		f_blocks;
	l_int		f_bfree;
	l_int		f_bavail;
	l_int		f_files;
	l_int		f_ffree;
	l_fsid_t	f_fsid;
	l_int		f_namelen;
	l_int		f_spare[6];
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

static long
bsd_to_linux_ftype(const char *fstypename)
{
	int i;
	static struct {const char *bsd_name; long linux_type;} b2l_tbl[] = {
		{"ufs",     LINUX_UFS_SUPER_MAGIC},
		{"cd9660",  LINUX_ISOFS_SUPER_MAGIC},
		{"nfs",     LINUX_NFS_SUPER_MAGIC},
		{"ext2fs",  LINUX_EXT2_SUPER_MAGIC},
		{"procfs",  LINUX_PROC_SUPER_MAGIC},
		{"msdosfs", LINUX_MSDOS_SUPER_MAGIC},
		{"ntfs",    LINUX_NTFS_SUPER_MAGIC},
		{"nwfs",    LINUX_NCP_SUPER_MAGIC},
		{"hpfs",    LINUX_HPFS_SUPER_MAGIC},
		{"coda",    LINUX_CODA_SUPER_MAGIC},
		{NULL,      0L}};

	for (i = 0; b2l_tbl[i].bsd_name != NULL; i++)
		if (strcmp(b2l_tbl[i].bsd_name, fstypename) == 0)
			return (b2l_tbl[i].linux_type);

	return (0L);
}

int
linux_statfs(struct thread *td, struct linux_statfs_args *args)
{
	struct mount *mp;
	struct nameidata *ndp;
	struct statfs *bsd_statfs;
	struct nameidata nd;
	struct l_statfs linux_statfs;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->path);

#ifdef DEBUG
	if (ldebug(statfs))
		printf(ARGS(statfs, "%s, *"), args->path);
#endif
	ndp = &nd;
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args->path, curthread);
	error = namei(ndp);
	if (error)
		return error;
	NDFREE(ndp, NDF_ONLY_PNBUF);
	mp = ndp->ni_vp->v_mount;
	bsd_statfs = &mp->mnt_stat;
	vrele(ndp->ni_vp);
	error = VFS_STATFS(mp, bsd_statfs, td);
	if (error)
		return error;
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs.f_type = bsd_to_linux_ftype(bsd_statfs->f_fstypename);
	linux_statfs.f_bsize = bsd_statfs->f_bsize;
	linux_statfs.f_blocks = bsd_statfs->f_blocks;
	linux_statfs.f_bfree = bsd_statfs->f_bfree;
	linux_statfs.f_bavail = bsd_statfs->f_bavail;
  	linux_statfs.f_ffree = bsd_statfs->f_ffree;
	linux_statfs.f_files = bsd_statfs->f_files;
	linux_statfs.f_fsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs.f_fsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs.f_namelen = MAXNAMLEN;
	return copyout((caddr_t)&linux_statfs, (caddr_t)args->buf,
	    sizeof(linux_statfs));
}

int
linux_fstatfs(struct thread *td, struct linux_fstatfs_args *args)
{
	struct file *fp;
	struct mount *mp;
	struct statfs *bsd_statfs;
	struct l_statfs linux_statfs;
	int error;

#ifdef DEBUG
	if (ldebug(fstatfs))
		printf(ARGS(fstatfs, "%d, *"), args->fd);
#endif
	error = getvnode(td->td_proc->p_fd, args->fd, &fp);
	if (error)
		return error;
	mp = ((struct vnode *)fp->f_data)->v_mount;
	bsd_statfs = &mp->mnt_stat;
	error = VFS_STATFS(mp, bsd_statfs, td);
	if (error) {
		fdrop(fp, td);
		return error;
	}
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs.f_type = bsd_to_linux_ftype(bsd_statfs->f_fstypename);
	linux_statfs.f_bsize = bsd_statfs->f_bsize;
	linux_statfs.f_blocks = bsd_statfs->f_blocks;
	linux_statfs.f_bfree = bsd_statfs->f_bfree;
	linux_statfs.f_bavail = bsd_statfs->f_bavail;
  	linux_statfs.f_ffree = bsd_statfs->f_ffree;
	linux_statfs.f_files = bsd_statfs->f_files;
	linux_statfs.f_fsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs.f_fsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs.f_namelen = MAXNAMLEN;
	error = copyout((caddr_t)&linux_statfs, (caddr_t)args->buf,
	    sizeof(linux_statfs));
	fdrop(fp, td);
	return error;
}

struct l_ustat 
{
	l_daddr_t	f_tfree;
	l_ino_t		f_tinode;
	char		f_fname[6];
	char		f_fpack[6];
};

int
linux_ustat(struct thread *td, struct linux_ustat_args *args)
{
	struct l_ustat lu;
	dev_t dev;
	struct vnode *vp;
	struct statfs *stat;
	int error;

#ifdef DEBUG
	if (ldebug(ustat))
		printf(ARGS(ustat, "%d, *"), args->dev);
#endif

	/*
	 * lu.f_fname and lu.f_fpack are not used. They are always zeroed.
	 * lu.f_tinode and lu.f_tfree are set from the device's super block.
	 */
	bzero(&lu, sizeof(lu));

	/*
	 * XXX - Don't return an error if we can't find a vnode for the
	 * device. Our dev_t is 32-bits whereas Linux only has a 16-bits
	 * dev_t. The dev_t that is used now may as well be a truncated
	 * dev_t returned from previous syscalls. Just return a bzeroed
	 * ustat in that case.
	 */
	dev = makedev(args->dev >> 8, args->dev & 0xFF);
	if (vfinddev(dev, VCHR, &vp)) {
		if (vp->v_mount == NULL)
			return (EINVAL);
		stat = &(vp->v_mount->mnt_stat);
		error = VFS_STATFS(vp->v_mount, stat, td);
		if (error)
			return (error);

		lu.f_tfree = stat->f_bfree;
		lu.f_tinode = stat->f_ffree;
	}

	return (copyout(&lu, args->ubuf, sizeof(lu)));
}

#if defined(__i386__)

static int
stat64_copyout(struct stat *buf, void *ubuf)
{
	struct l_stat64 lbuf;

	bzero(&lbuf, sizeof(lbuf));
	lbuf.st_dev = uminor(buf->st_dev) | (umajor(buf->st_dev) << 8);
	lbuf.st_ino = buf->st_ino;
	lbuf.st_mode = buf->st_mode;
	lbuf.st_nlink = buf->st_nlink;
	lbuf.st_uid = buf->st_uid;
	lbuf.st_gid = buf->st_gid;
	lbuf.st_rdev = buf->st_rdev;
	lbuf.st_size = buf->st_size;
	lbuf.st_atime = buf->st_atime;
	lbuf.st_mtime = buf->st_mtime;
	lbuf.st_ctime = buf->st_ctime;
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
	struct nameidata nd;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->filename);

#ifdef DEBUG
	if (ldebug(stat64))
		printf(ARGS(stat64, "%s, *"), args->filename);
#endif

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	    args->filename, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = vn_stat(nd.ni_vp, &buf, td);
	vput(nd.ni_vp);
	if (error)
		return (error);

	return (stat64_copyout(&buf, args->statbuf));
}

int
linux_lstat64(struct thread *td, struct linux_lstat64_args *args)
{
	int error;
	struct stat sb;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, args->filename);

#ifdef DEBUG
	if (ldebug(lstat64))
		printf(ARGS(lstat64, "%s, *"), args->filename);
#endif

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	    args->filename, td);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF); 

	error = vn_stat(nd.ni_vp, &sb, td);
	vput(nd.ni_vp);
	if (error)
		return (error);

	return (stat64_copyout(&sb, args->statbuf));
}

int
linux_fstat64(struct thread *td, struct linux_fstat64_args *args)
{
	struct filedesc *fdp;
	struct file *fp;
	struct stat buf;
	int error;

#ifdef DEBUG
	if (ldebug(fstat64))
		printf(ARGS(fstat64, "%d, *"), args->fd);
#endif

	fdp = td->td_proc->p_fd;
	if ((unsigned)args->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[args->fd]) == NULL)
		return (EBADF);

	error = fo_stat(fp, &buf, td);
	if (!error)
		error = stat64_copyout(&buf, args->statbuf);

	return (error);
}

#endif /* __i386__ */
