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
#include <sys/systm.h>
#include <sys/vnode.h>

#include <machine/../linux/linux.h>
#include <linux_proto.h>
#include <compat/linux/linux_util.h>

struct linux_newstat {
#ifdef __alpha__
	u_int	stat_dev;
	u_int	stat_ino;
	u_int	stat_mode;
	u_int	stat_nlink;
	u_int	stat_uid;
	u_int	stat_gid;
	u_int	stat_rdev;
	long	stat_size;
	u_long	stat_atime;
	u_long	stat_mtime;
	u_long	stat_ctime;
	u_int	stat_blksize;
	int		stat_blocks;
	u_int	stat_flags;
	u_int	stat_gen;
#else
	u_short	stat_dev;
	u_short	__pad1;
	u_long	stat_ino;
	u_short	stat_mode;
	u_short	stat_nlink;
	u_short	stat_uid;
	u_short	stat_gid;
	u_short	stat_rdev;
	u_short	__pad2;
	u_long	stat_size;
	u_long	stat_blksize;
	u_long	stat_blocks;
	u_long	stat_atime;
	u_long	__unused1;
	u_long	stat_mtime;
	u_long	__unused2;
	u_long	stat_ctime;
	u_long	__unused3;
	u_long	__unused4;
	u_long	__unused5;
#endif
};


struct linux_ustat 
{
	int	f_tfree;
	u_long	f_tinode;
	char	f_fname[6];
	char	f_fpack[6];
};

static int
newstat_copyout(struct stat *buf, void *ubuf)
{
	struct linux_newstat tbuf;

	tbuf.stat_dev = uminor(buf->st_dev) | (umajor(buf->st_dev) << 8);
	tbuf.stat_ino = buf->st_ino;
	tbuf.stat_mode = buf->st_mode;
	tbuf.stat_nlink = buf->st_nlink;
	tbuf.stat_uid = buf->st_uid;
	tbuf.stat_gid = buf->st_gid;
	tbuf.stat_rdev = buf->st_rdev;
	tbuf.stat_size = buf->st_size;
	tbuf.stat_atime = buf->st_atime;
	tbuf.stat_mtime = buf->st_mtime;
	tbuf.stat_ctime = buf->st_ctime;
	tbuf.stat_blksize = buf->st_blksize;
	tbuf.stat_blocks = buf->st_blocks;

	return (copyout(&tbuf, ubuf, sizeof(tbuf)));
}

int
linux_newstat(struct proc *p, struct linux_newstat_args *args)
{
	struct stat buf;
	struct nameidata nd;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	printf("Linux-emul(%ld): newstat(%s, *)\n", (long)p->p_pid,
	       args->path);
#endif

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	       args->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);

	error = vn_stat(nd.ni_vp, &buf, p);
	vput(nd.ni_vp);
	if (error)
		return (error);

	return (newstat_copyout(&buf, args->buf));
}

/*
 * Get file status; this version does not follow links.
 */
int
linux_newlstat(p, uap)
	struct proc *p;
	struct linux_newlstat_args *uap;
{
	int error;
	struct vnode *vp;
	struct stat sb;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, uap->path);

#ifdef DEBUG
	printf("Linux-emul(%ld): newlstat(%s, *)\n", (long)p->p_pid,
	       uap->path);
#endif

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | NOOBJ, UIO_USERSPACE,
	       uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF); 

	vp = nd.ni_vp;
	error = vn_stat(vp, &sb, p);
	vput(vp);
	if (error)
		return (error);

	return (newstat_copyout(&sb, uap->buf));
}

int
linux_newfstat(struct proc *p, struct linux_newfstat_args *args)
{
	struct filedesc *fdp;
	struct file *fp;
	struct stat buf;
	int error;

	fdp = p->p_fd;

#ifdef DEBUG
	printf("Linux-emul(%ld): newfstat(%d, *)\n", (long)p->p_pid, args->fd);
#endif

	if ((unsigned)args->fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[args->fd]) == NULL)
		return (EBADF);

	error = fo_stat(fp, &buf, p);
	if (!error)
		error = newstat_copyout(&buf, args->buf);

	return (error);
}

struct linux_statfs_buf {
	int ftype;
	int fbsize;
	int fblocks;
	int fbfree;
	int fbavail;
	int ffiles;
	int fffree;
	linux_fsid_t ffsid;
	int fnamelen;
	int fspare[6];
};

#ifndef VT_NWFS
#define	VT_NWFS	VT_TFS	/* XXX - bug compatibility with sys/nwfs/nwfs_node.h */
#endif

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

/*
 * ext2fs uses the VT_UFS tag. A mounted ext2 filesystem will therefore
 * be seen as an ufs/mfs filesystem.
 */
static long
bsd_to_linux_ftype(int tag)
{

	switch (tag) {
	case VT_CODA:
		return (LINUX_CODA_SUPER_MAGIC);
	case VT_HPFS:
		return (LINUX_HPFS_SUPER_MAGIC);
	case VT_ISOFS:
		return (LINUX_ISOFS_SUPER_MAGIC);
	case VT_MFS:
		return (LINUX_UFS_SUPER_MAGIC);
	case VT_MSDOSFS:
		return (LINUX_MSDOS_SUPER_MAGIC);
	case VT_NFS:
		return (LINUX_NFS_SUPER_MAGIC);
	case VT_NTFS:
		return (LINUX_NTFS_SUPER_MAGIC);
	case VT_NWFS:
		return (LINUX_NCP_SUPER_MAGIC);
	case VT_PROCFS:
		return (LINUX_PROC_SUPER_MAGIC);
	case VT_UFS:
		return (LINUX_UFS_SUPER_MAGIC);
	}

	return (0L);
}

int
linux_statfs(struct proc *p, struct linux_statfs_args *args)
{
	struct mount *mp;
	struct nameidata *ndp;
	struct statfs *bsd_statfs;
	struct nameidata nd;
	struct linux_statfs_buf linux_statfs_buf;
	int error;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, args->path);

#ifdef DEBUG
	printf("Linux-emul(%d): statfs(%s, *)\n", p->p_pid, args->path);
#endif
	ndp = &nd;
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args->path, curproc);
	error = namei(ndp);
	if (error)
		return error;
	NDFREE(ndp, NDF_ONLY_PNBUF);
	mp = ndp->ni_vp->v_mount;
	bsd_statfs = &mp->mnt_stat;
	vrele(ndp->ni_vp);
	error = VFS_STATFS(mp, bsd_statfs, p);
	if (error)
		return error;
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs_buf.ftype = bsd_to_linux_ftype(bsd_statfs->f_type);
	linux_statfs_buf.fbsize = bsd_statfs->f_bsize;
	linux_statfs_buf.fblocks = bsd_statfs->f_blocks;
	linux_statfs_buf.fbfree = bsd_statfs->f_bfree;
	linux_statfs_buf.fbavail = bsd_statfs->f_bavail;
  	linux_statfs_buf.fffree = bsd_statfs->f_ffree;
	linux_statfs_buf.ffiles = bsd_statfs->f_files;
	linux_statfs_buf.ffsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs_buf.ffsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs_buf.fnamelen = MAXNAMLEN;
	return copyout((caddr_t)&linux_statfs_buf, (caddr_t)args->buf,
		       sizeof(struct linux_statfs_buf));
}

int
linux_fstatfs(struct proc *p, struct linux_fstatfs_args *args)
{
	struct file *fp;
	struct mount *mp;
	struct statfs *bsd_statfs;
	struct linux_statfs_buf linux_statfs_buf;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%d): fstatfs(%d, *)\n", p->p_pid, args->fd);
#endif
	error = getvnode(p->p_fd, args->fd, &fp);
	if (error)
		return error;
	mp = ((struct vnode *)fp->f_data)->v_mount;
	bsd_statfs = &mp->mnt_stat;
	error = VFS_STATFS(mp, bsd_statfs, p);
	if (error)
		return error;
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs_buf.ftype = bsd_to_linux_ftype(bsd_statfs->f_type);
	linux_statfs_buf.fbsize = bsd_statfs->f_bsize;
	linux_statfs_buf.fblocks = bsd_statfs->f_blocks;
	linux_statfs_buf.fbfree = bsd_statfs->f_bfree;
	linux_statfs_buf.fbavail = bsd_statfs->f_bavail;
  	linux_statfs_buf.fffree = bsd_statfs->f_ffree;
	linux_statfs_buf.ffiles = bsd_statfs->f_files;
	linux_statfs_buf.ffsid.val[0] = bsd_statfs->f_fsid.val[0];
	linux_statfs_buf.ffsid.val[1] = bsd_statfs->f_fsid.val[1];
	linux_statfs_buf.fnamelen = MAXNAMLEN;
	return copyout((caddr_t)&linux_statfs_buf, (caddr_t)args->buf,
		       sizeof(struct linux_statfs_buf));
}

int
linux_ustat(p, uap)
	struct proc *p;
	struct linux_ustat_args *uap;
{
	struct linux_ustat lu;
	dev_t dev;
	struct vnode *vp;
	struct statfs *stat;
	int error;

#ifdef DEBUG
	printf("Linux-emul(%ld): ustat(%d, *)\n", (long)p->p_pid, uap->dev);
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
	dev = makedev(uap->dev >> 8, uap->dev & 0xFF);
	if (vfinddev(dev, VCHR, &vp)) {
		if (vp->v_mount == NULL)
			return (EINVAL);
		stat = &(vp->v_mount->mnt_stat);
		error = VFS_STATFS(vp->v_mount, stat, p);
		if (error)
			return (error);

		lu.f_tfree = stat->f_bfree;
		lu.f_tinode = stat->f_ffree;
	}

	return (copyout(&lu, uap->ubuf, sizeof(lu)));
}
