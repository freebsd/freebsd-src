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
 *  $Id: linux_stats.c,v 1.8 1997/02/22 09:38:25 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/pipe.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <i386/linux/linux_util.h>

struct linux_newstat {
    unsigned short stat_dev;
    unsigned short __pad1;
    unsigned long stat_ino;
    unsigned short stat_mode;
    unsigned short stat_nlink;
    unsigned short stat_uid;
    unsigned short stat_gid;
    unsigned short stat_rdev;
    unsigned short __pad2;
    unsigned long stat_size;
    unsigned long stat_blksize;
    unsigned long stat_blocks;
    unsigned long stat_atime;
    unsigned long __unused1;
    unsigned long stat_mtime;
    unsigned long __unused2;
    unsigned long stat_ctime;
    unsigned long __unused3;
    unsigned long __unused4;
    unsigned long __unused5;
};


static int
newstat_copyout(struct stat *buf, void *ubuf)
{
    struct linux_newstat tbuf;

    tbuf.stat_dev = (buf->st_dev & 0xff) | ((buf->st_dev & 0xff00)<<10);
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
    return copyout(&tbuf, ubuf, sizeof(tbuf));
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
    printf("Linux-emul(%d): newstat(%s, *)\n", p->p_pid, args->path);
#endif
    NDINIT(&nd, LOOKUP, LOCKLEAF|FOLLOW, UIO_USERSPACE, args->path, p);
    error = namei(&nd);
    if (!error) {
	error = vn_stat(nd.ni_vp, &buf, p);
	vput(nd.ni_vp);
    }
    if (!error) 
	error = newstat_copyout(&buf, args->buf);
    return error;
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
	struct vnode *vp, *dvp;
	struct stat sb, sb1;
	struct nameidata nd;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(p, &sg, uap->path);
  
#ifdef DEBUG
	printf("Linux-emul(%d): newlstat(%s, *)\n", p->p_pid, uap->path);
#endif
	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF | LOCKPARENT, UIO_USERSPACE,
	    uap->path, p);
	error = namei(&nd);
	if (error)
		return (error);
	/*
	 * For symbolic links, always return the attributes of its
	 * containing directory, except for mode, size, and links.
	 */
	vp = nd.ni_vp;
	dvp = nd.ni_dvp;
	if (vp->v_type != VLNK) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
		error = vn_stat(vp, &sb, p);
		vput(vp);
		if (error)
			return (error);
	} else {
		error = vn_stat(dvp, &sb, p);
		vput(dvp);
		if (error) {
			vput(vp);
			return (error);
		}
		error = vn_stat(vp, &sb1, p);
		vput(vp);
		if (error)
			return (error);
		sb.st_mode &= ~S_IFDIR;
		sb.st_mode |= S_IFLNK;
		sb.st_nlink = sb1.st_nlink;
		sb.st_size = sb1.st_size;
		sb.st_blocks = sb1.st_blocks;
	}
	error = newstat_copyout(&sb, uap->buf);
	return (error);
}

int
linux_newfstat(struct proc *p, struct linux_newfstat_args *args)
{
    struct filedesc *fdp = p->p_fd;
    struct file *fp;
    struct stat buf;
    int error;
  
#ifdef DEBUG
    printf("Linux-emul(%d): newfstat(%d, *)\n", p->p_pid, args->fd);
#endif
    if ((unsigned)args->fd >= fdp->fd_nfiles 
	|| (fp = fdp->fd_ofiles[args->fd]) == NULL)
	return EBADF;
    switch (fp->f_type) {
    case DTYPE_FIFO:
    case DTYPE_VNODE:
	error = vn_stat((struct vnode *)fp->f_data, &buf, p);
	break;
    case DTYPE_SOCKET:
	error = soo_stat((struct socket *)fp->f_data, &buf);
	break;
    case DTYPE_PIPE:
	error = pipe_stat((struct pipe *)fp->f_data, &buf);
	break;
    default:
	panic("LINUX newfstat");
    }
    if (!error)
	error = newstat_copyout(&buf, args->buf);
    return error;
}

struct linux_statfs_buf {
	long ftype;
	long fbsize;
	long fblocks;
	long fbfree;
	long fbavail;
	long ffiles;
	long fffree;
	linux_fsid_t ffsid;
	long fnamelen;
	long fspare[6];
};

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
	if (error = namei(ndp))
		return error;
	mp = ndp->ni_vp->v_mount;
	bsd_statfs = &mp->mnt_stat;
	vrele(ndp->ni_vp);
	if (error = VFS_STATFS(mp, bsd_statfs, p))
		return error;
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs_buf.ftype = bsd_statfs->f_type;
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
	if (error = getvnode(p->p_fd, args->fd, &fp))
		return error;
	mp = ((struct vnode *)fp->f_data)->v_mount;
	bsd_statfs = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, bsd_statfs, p))
		return error;
	bsd_statfs->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	linux_statfs_buf.ftype = bsd_statfs->f_type;
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
