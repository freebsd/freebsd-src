/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
 * All rights reserved.
 *
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	$Id: ibcs2_stats.c,v 1.1 1994/10/14 08:53:09 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

struct ibcs2_stat {
  	ibcs2_dev_t	stat_dev;
  	ibcs2_ino_t	stat_ino;
  	ibcs2_mode_t	stat_mode;
  	ibcs2_nlink_t	stat_nlink;
  	ibcs2_uid_t	stat_uid;
  	ibcs2_gid_t	stat_gid;
  	ibcs2_dev_t	stat_rdev;
  	ibcs2_size_t	stat_size;
  	ibcs2_time_t	stat_atime;
  	ibcs2_time_t	stat_mtime;
  	ibcs2_time_t	stat_ctime;
};

struct ibcs2_stat_args {
  	char *path;
  	struct ibcs2_stat *buf;
};

static int
stat_copyout(struct stat *buf, void *ubuf)
{
  	struct ibcs2_stat tbuf;

	tbuf.stat_dev = buf->st_dev;
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
	return copyout(&tbuf, ubuf, sizeof(tbuf));
}

int
ibcs2_stat(struct proc *p, struct ibcs2_stat_args *args, int *retval)
{
  	struct stat buf;
  	struct ibcs2_stat tbuf;
  	struct nameidata nd;
  	int error;

	if (ibcs2_trace & IBCS2_TRACE_STATS)
		printf("IBCS2: 'stat' path=%s\n", args->path);

  	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | FOLLOW;
	nd.ni_cnd.cn_proc = curproc;
	nd.ni_cnd.cn_cred = curproc->p_cred->pc_ucred;
  	nd.ni_segflg = UIO_USERSPACE;
  	nd.ni_dirp = args->path;
  	error = namei(&nd);

  	if (!error) {
    		error = vn_stat(nd.ni_vp, &buf, p);
    		vput(nd.ni_vp);
  	}

  	if (!error)
		error = stat_copyout(&buf, args->buf);

    	return error;
}

int
ibcs2_lstat(struct proc *p, struct ibcs2_stat_args *args, int *retval)
{
  	struct stat buf;
  	struct ibcs2_stat tbuf;
  	struct nameidata nd;
  	int error;

	if (ibcs2_trace & IBCS2_TRACE_STATS)
		printf("IBCS2: 'lstat' path=%s\n", args->path);
  	nd.ni_cnd.cn_nameiop = LOOKUP;
	nd.ni_cnd.cn_flags = LOCKLEAF | FOLLOW;
	nd.ni_cnd.cn_proc = curproc;
	nd.ni_cnd.cn_cred = curproc->p_cred->pc_ucred;
  	nd.ni_segflg = UIO_USERSPACE;
  	nd.ni_dirp = args->path;
  	error = namei(&nd);

  	if (!error) {
    		error = vn_stat(nd.ni_vp, &buf, p);
    		vput(nd.ni_vp);
  	}

  	if (!error)
		error = stat_copyout(&buf, args->buf);

    	return error;
}

struct ibcs2_fstat_args {
	int fd;
  	struct ibcs2_stat *buf;
};

int
ibcs2_fstat(struct proc *p, struct ibcs2_fstat_args *args, int *retval)
{
  	struct ibcs2_stat tbuf;
  	struct filedesc *fdp = p->p_fd;
  	struct file *fp;
  	struct stat buf;
  	int error;

	if (ibcs2_trace & IBCS2_TRACE_STATS)
		printf("IBCS2: 'fstat' fd=%d\n", args->fd);
  	if ((unsigned)args->fd >= fdp->fd_nfiles
	    || (fp = fdp->fd_ofiles[args->fd]) == NULL)
    		return EBADF;

  	switch (fp->f_type) {
  	case DTYPE_VNODE:
    		error = vn_stat((struct vnode *)fp->f_data, &buf, p);
    		break;

  	case DTYPE_SOCKET:
    		error = soo_stat((struct socket *)fp->f_data, &buf);
    		break;

  	default:
    		panic("IBCS2 fstat");
    		/*NOTREACHED*/
  	}
  	if (!error)
		error = stat_copyout(&buf, args->buf);

    	return error;
}

struct ibcs2_statfs {
  	short	f_fstyp;
  	long	f_bsize;
  	long	f_frsize;
  	long	f_blocks;
  	long	f_bfree;
  	long	f_files;
  	long	f_ffree;
  	char	f_fname[6];
  	char	f_fpack[6];
};

struct ibcs2_statfs_args {
	char *path;
	struct statfs *buf;
	int len;
	int fstyp;
};

int
ibcs2_statfs(struct proc *p, struct ibcs2_statfs_args *args, int *retval)
{
	struct mount *mp;
	struct nameidata *ndp;
	struct statfs *sp;
	struct nameidata nd;
	struct ibcs2_statfs tmp;
	int error;

	if (ibcs2_trace & IBCS2_TRACE_STATS)
		printf("IBCS2: 'statfs' path=%s\n", args->path);
	ndp = &nd;
  	ndp->ni_cnd.cn_nameiop = LOOKUP;
	ndp->ni_cnd.cn_flags = FOLLOW;
	ndp->ni_cnd.cn_proc = curproc;
	ndp->ni_cnd.cn_cred = curproc->p_cred->pc_ucred;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = args->path;
	if (error = namei(ndp))
		return error;
	mp = ndp->ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(ndp->ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return error;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	tmp.f_fstyp = sp->f_type;
	tmp.f_bsize = sp->f_bsize;
	tmp.f_frsize = sp->f_iosize;
	tmp.f_blocks = sp->f_blocks;
	tmp.f_bfree = sp->f_bfree;
  	tmp.f_ffree = sp->f_ffree;
	tmp.f_files = sp->f_files;
	bcopy (sp->f_mntonname, tmp.f_fname, 6);
	bcopy (sp->f_mntfromname, tmp.f_fpack, 6);
	return copyout((caddr_t)&tmp, (caddr_t)args->buf, args->len);
}

struct ibcs2_fstatfs_args {
	int fd;
	struct statfs *buf;
};

int
ibcs2_fstatfs(struct proc *p, struct ibcs2_fstatfs_args *args, int *retval)
{
	struct file *fp;
	struct mount *mp;
	struct statfs *sp;
	struct ibcs2_statfs tmp;
	int error;

	if (ibcs2_trace & IBCS2_TRACE_STATS)
		printf("IBCS2: 'fstatfs' fd=%d\n", args->fd);
	if (error = getvnode(p->p_fd, args->fd, &fp))
		return error;
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return error;
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	tmp.f_fstyp = sp->f_type;
	tmp.f_bsize = sp->f_bsize;
	tmp.f_frsize = sp->f_iosize;
	tmp.f_blocks = sp->f_blocks;
	tmp.f_bfree = sp->f_bfree;
  	tmp.f_ffree = sp->f_ffree;
	tmp.f_files = sp->f_files;
	bcopy (sp->f_mntonname, tmp.f_fname, 6);
	bcopy (sp->f_mntfromname, tmp.f_fpack, 6);
	return copyout((caddr_t)&tmp, (caddr_t)args->buf,
		       sizeof(struct statfs));
}
