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
 *  $Id: linux_file.c,v 1.3 1995/10/10 23:13:27 swallace Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/exec.h>
#include <sys/dirent.h>

#include <ufs/ufs/dir.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

struct linux_creat_args {
    char    *path;
    int mode;
};

int
linux_creat(struct proc *p, struct linux_creat_args *args, int *retval)
{
    struct {
	char *path;
	int flags;
	int mode;
    } bsd_open_args;

#ifdef DEBUG
    printf("Linux-emul(%d): creat(%s, %d)\n", 
	   p->p_pid, args->path, args->mode);
#endif
    bsd_open_args.path = args->path;
    bsd_open_args.mode = args->mode;
    bsd_open_args.flags = O_WRONLY | O_CREAT | O_TRUNC;
    return open(p, &bsd_open_args, retval);
}

struct linux_open_args {
    char *path;
    int flags;
    int mode;
};

int
linux_open(struct proc *p, struct linux_open_args *args, int *retval)
{
    struct {
	char *path;
	int flags;
	int mode;
    } bsd_open_args;
    int error;
    
#ifdef DEBUG
    printf("Linux-emul(%d): open(%s, 0x%x, 0x%x)\n", 
	   p->p_pid, args->path, args->flags, args->mode);
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

    error = open(p, &bsd_open_args, retval);
    if (!error && !(bsd_open_args.flags & O_NOCTTY) && 
	SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
	struct filedesc *fdp = p->p_fd;
	struct file *fp = fdp->fd_ofiles[*retval];

	if (fp->f_type == DTYPE_VNODE)
	    (fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, (caddr_t) 0, p);
    }
    return error;
}

struct linux_flock {
    short l_type;
    short l_whence;
    linux_off_t l_start;
    linux_off_t l_len;
    linux_pid_t l_pid;
};

static void
linux_to_bsd_flock(struct linux_flock *linux_flock, struct flock *bsd_flock)
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
    }
    bsd_flock->l_whence = linux_flock->l_whence;
    bsd_flock->l_start = (off_t)linux_flock->l_start;
    bsd_flock->l_len = (off_t)linux_flock->l_len;
    bsd_flock->l_pid = (pid_t)linux_flock->l_pid;
}

static void
bsd_to_linux_flock(struct flock *bsd_flock, struct linux_flock *linux_flock)
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
    linux_flock->l_start = (linux_off_t)bsd_flock->l_start;
    linux_flock->l_len = (linux_off_t)bsd_flock->l_len;
    linux_flock->l_pid = (linux_pid_t)bsd_flock->l_pid;
}

struct linux_fcntl_args {
    int fd;
    int cmd;
    int arg;
};

int
linux_fcntl(struct proc *p, struct linux_fcntl_args *args, int *retval)
{
    int error, result;
    struct fcntl_args {
	int fd;
	int cmd;
	int arg;
    } fcntl_args; 
    struct linux_flock linux_flock;
    struct flock *bsd_flock = 
	(struct flock *)ua_alloc_init(sizeof(struct flock));

#ifdef DEBUG
    printf("Linux-emul(%d): fcntl(%d, %08x, *)\n",
	   p->p_pid, args->fd, args->cmd);
#endif
    fcntl_args.fd = args->fd;
    fcntl_args.arg = 0;

    switch (args->cmd) {
    case LINUX_F_DUPFD:
	fcntl_args.cmd = F_DUPFD;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_GETFD:
	fcntl_args.cmd = F_GETFD;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_SETFD:
	fcntl_args.cmd = F_SETFD;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_GETFL:
	fcntl_args.cmd = F_GETFL;
	error = fcntl(p, &fcntl_args, &result);
	*retval = 0;
	if (result & O_RDONLY) *retval |= LINUX_O_RDONLY;
	if (result & O_WRONLY) *retval |= LINUX_O_WRONLY;
	if (result & O_RDWR) *retval |= LINUX_O_RDWR;
	if (result & O_NDELAY) *retval |= LINUX_O_NONBLOCK;
	if (result & O_APPEND) *retval |= LINUX_O_APPEND;
	if (result & O_FSYNC) *retval |= LINUX_O_SYNC;
	return error;

    case LINUX_F_SETFL:
	if (args->arg & LINUX_O_NDELAY) fcntl_args.arg |= O_NONBLOCK;
	if (args->arg & LINUX_O_APPEND) fcntl_args.arg |= O_APPEND;
	if (args->arg & LINUX_O_SYNC) fcntl_args.arg |= O_FSYNC;
	fcntl_args.cmd = F_SETFL;
	return fcntl(p, &fcntl_args, retval);
    
    case LINUX_F_GETLK:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_GETLK;
	fcntl_args.arg = (int)bsd_flock;
	if (error = fcntl(p, &fcntl_args, retval))
	    return error;
	bsd_to_linux_flock(bsd_flock, &linux_flock);
	return copyout((caddr_t)&linux_flock, (caddr_t)args->arg,
		       sizeof(struct linux_flock));

    case LINUX_F_SETLK:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_SETLK;
	fcntl_args.arg = (int)bsd_flock;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_SETLKW:
	if ((error = copyin((caddr_t)args->arg, (caddr_t)&linux_flock,
		   	    sizeof(struct linux_flock)))) 
	    return error;
	linux_to_bsd_flock(&linux_flock, bsd_flock);
	fcntl_args.cmd = F_SETLKW;
	fcntl_args.arg = (int)bsd_flock;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_SETOWN:
	fcntl_args.cmd = F_SETOWN;
	return fcntl(p, &fcntl_args, retval);

    case LINUX_F_GETOWN:
	fcntl_args.cmd = F_GETOWN;
	return fcntl(p, &fcntl_args, retval);
    }
    return EINVAL;
}

struct linux_lseek_args {
    int fdes;
    unsigned long off;
    int whence;
};

int
linux_lseek(struct proc *p, struct linux_lseek_args *args, int *retval)
{

    struct lseek_args {
	int fdes;
	int pad;
	off_t off;
	int whence;
    } tmp_args;
    off_t tmp_retval;
    int error;

#ifdef DEBUG
    printf("Linux-emul(%d): lseek(%d, %d, %d)\n",
	   p->p_pid, args->fdes, args->off, args->whence);
#endif
    tmp_args.fdes = args->fdes;
    tmp_args.off = (off_t)args->off;
    tmp_args.whence = args->whence;
    error = lseek(p, &tmp_args, &tmp_retval);
    *retval = (int)tmp_retval;
    return error;
}

struct linux_dirent {
    long dino;
    linux_off_t doff;
    unsigned short dreclen;
    char dname[LINUX_NAME_MAX + 1];
};

#define LINUX_RECLEN(de,namlen) \
    ALIGN((((char *)&(de)->dname - (char *)de) + (namlen) + 1))

struct linux_readdir_args {
    int fd;
    struct linux_dirent *dent;
    unsigned int count;
};

int
linux_readdir(struct proc *p, struct linux_readdir_args *args, int *retval)
{
    register struct dirent *bdp;
    struct vnode *vp;
    caddr_t inp, buf;		/* BSD-format */
    int len, reclen;		/* BSD-format */
    caddr_t outp;		/* Linux-format */
    int resid, linuxreclen=0;	/* Linux-format */
    struct file *fp;
    struct uio auio;
    struct iovec aiov;
    struct vattr va;
    off_t off;
    struct linux_dirent linux_dirent;
    int buflen, error, eofflag, nbytes, justone, blockoff;

#ifdef DEBUG
    printf("Linux-emul(%d): readdir(%d, *, %d)\n",
	   p->p_pid, args->fd, args->count);
#endif
    if ((error = getvnode(p->p_fd, args->fd, &fp)) != 0) {
	return (error);
}

    if ((fp->f_flag & FREAD) == 0)
	return (EBADF);

    vp = (struct vnode *) fp->f_data;

    if (vp->v_type != VDIR)
	return (EINVAL);

    if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p))) {
	return error;
    }

    nbytes = args->count;
    if (nbytes == 1) {
	nbytes = sizeof (struct linux_dirent);
	justone = 1;
    }
    else
	justone = 0;

    off = fp->f_offset;
    blockoff = off % DIRBLKSIZ;
    buflen = max(DIRBLKSIZ, nbytes + blockoff);
    buflen = min(buflen, MAXBSIZE);
    buf = malloc(buflen, M_TEMP, M_WAITOK);
    VOP_LOCK(vp);
again:
    aiov.iov_base = buf;
    aiov.iov_len = buflen;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_rw = UIO_READ;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_procp = p;
    auio.uio_resid = buflen;
    auio.uio_offset = off - (off_t)blockoff;

    error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, (int *) NULL,
			(u_int **) NULL);
    if (error) {
	goto out;
}

    inp = buf;
    inp += blockoff;
    outp = (caddr_t) args->dent;
    resid = nbytes;
    if ((len = buflen - auio.uio_resid - blockoff) == 0) {
	goto eof;
      }

    while (len > 0) {
	bdp = (struct dirent *) inp;
	reclen = bdp->d_reclen;
	if (reclen & 3) {
	    printf("linux_readdir: reclen=%d\n", reclen);
	    error = EFAULT;
	    goto out;
	}
  
	if (bdp->d_fileno == 0) {
	    inp += reclen;
	    off += reclen;
	    len -= reclen;
	    continue;
	}
	linuxreclen = LINUX_RECLEN(&linux_dirent, bdp->d_namlen);
	if (reclen > len || resid < linuxreclen) {
	    outp++;
	    break;
	}
	linux_dirent.dino = (long) bdp->d_fileno;
	linux_dirent.doff = (linux_off_t) linuxreclen;
	linux_dirent.dreclen = (u_short) bdp->d_namlen;
	strcpy(linux_dirent.dname, bdp->d_name);
	if ((error = copyout((caddr_t)&linux_dirent, outp, linuxreclen))) {
	    goto out;
	  }
	inp += reclen;
	off += reclen;
	outp += linuxreclen;
	resid -= linuxreclen;
	len -= reclen;
	if (justone)
	    break;
    }

    if (outp == (caddr_t) args->dent)
	goto again;
    fp->f_offset = off;

    if (justone)
	nbytes = resid + linuxreclen;

eof:
    *retval = nbytes - resid;
out:
    VOP_UNLOCK(vp);
    free(buf, M_TEMP);
    return error;
}
