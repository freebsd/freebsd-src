/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
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
 *	$Id: ibcs2_file.c,v 1.1 1994/10/14 08:53:01 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/exec.h>
#include <vm/vm.h>
#include <ufs/ufs/dir.h>


struct ibcs2_close_args {
	int fd;
};

int
ibcs2_close(struct proc *p, struct ibcs2_close_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'close' fd=%d\n", args->fd);
	return close(p, args, retval);
}

struct ibcs2_creat_args {
	char	*fname;
	int	fmode;
};

int
ibcs2_creat(struct proc *p, struct ibcs2_creat_args *args, int *retval)
{
	struct args {
		char	*fname;
		int	mode;
		int	crtmode;
	} bsd_open_args;

	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'creat' name=%s, mode=%x\n", 
			args->fname, args->fmode);
	bsd_open_args.fname = args->fname;
	bsd_open_args.crtmode = args->fmode;
	bsd_open_args.mode = O_WRONLY | O_CREAT | O_TRUNC;
	return open(p, &bsd_open_args, retval);
}

int
ibcs2_dup(struct proc *p, void *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'dup'\n");
	return dup(p, args, retval);
}

struct ibcs2_flock {
        short   	l_type;
        short   	l_whence;
        ibcs2_off_t	l_start;
        ibcs2_off_t	l_len;
        short   	l_sysid;
        ibcs2_pid_t	l_pid;
};

static void
ibcs2_to_bsd_flock(struct ibcs2_flock *ibcs2_flock, struct flock *bsd_flock)
{
	switch (ibcs2_flock->l_type) {
	case IBCS2_F_RDLCK:
		bsd_flock->l_type = F_RDLCK;
		break;
	case IBCS2_F_WRLCK:
		bsd_flock->l_type = F_WRLCK;
		break;
	case IBCS2_F_UNLCK:
		bsd_flock->l_type = F_UNLCK;
		break;
	}
	bsd_flock->l_whence = ibcs2_flock->l_whence;
	bsd_flock->l_start = (off_t)ibcs2_flock->l_start;
	bsd_flock->l_len = (off_t)ibcs2_flock->l_len;
	bsd_flock->l_pid = (pid_t)ibcs2_flock->l_pid;
}

static void
bsd_to_ibcs2_flock(struct flock *bsd_flock, struct ibcs2_flock *ibcs2_flock)
{
	switch (bsd_flock->l_type) {
	case F_RDLCK:
		ibcs2_flock->l_type = IBCS2_F_RDLCK;
		break;
	case F_WRLCK:
		ibcs2_flock->l_type = IBCS2_F_WRLCK;
		break;
	case F_UNLCK:
		ibcs2_flock->l_type = IBCS2_F_UNLCK;
		break;
	}
	ibcs2_flock->l_whence = bsd_flock->l_whence;
	ibcs2_flock->l_start = (ibcs2_off_t)bsd_flock->l_start;
	ibcs2_flock->l_len = (ibcs2_off_t)bsd_flock->l_len;
	ibcs2_flock->l_sysid = 0;
	ibcs2_flock->l_pid = (ibcs2_pid_t)bsd_flock->l_pid;
}

struct ibcs2_fcntl_args {
  	int fd;
  	int cmd;
  	int arg;
};

int
ibcs2_fcntl(struct proc *p, struct ibcs2_fcntl_args *args, int *retval)
{
	int error, result;
	struct fcntl_args {
  		int fd;
  		int cmd;
  		int arg;
	} fcntl_args; 
	struct ibcs2_flock ibcs2_flock;
	struct flock *bsd_flock = (struct flock *)UA_ALLOC();

	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'fcntl' fd=%d, cmd=%d arg=%d\n",
			args->fd, args->cmd, args->arg);

  	fcntl_args.fd = args->fd;
  	fcntl_args.arg = 0;

  	switch (args->cmd) {
	case IBCS2_F_DUPFD:
  		fcntl_args.cmd = F_DUPFD;
		return fcntl(p, &fcntl_args, retval);

	case IBCS2_F_GETFD:
  		fcntl_args.cmd = F_GETFD;
		return fcntl(p, &fcntl_args, retval);

	case IBCS2_F_SETFD:
  		fcntl_args.cmd = F_SETFD;
		return fcntl(p, &fcntl_args, retval);

	case IBCS2_F_GETFL:
  		fcntl_args.cmd = F_GETFL;
		error = fcntl(p, &fcntl_args, &result);
		*retval = 0;
    		if (result & O_RDONLY)   *retval |= IBCS2_RDONLY;
    		if (result & O_WRONLY)   *retval |= IBCS2_WRONLY;
    		if (result & O_RDWR)     *retval |= IBCS2_RDWR;
    		if (result & O_NDELAY)   *retval |= IBCS2_NONBLOCK;
    		if (result & O_APPEND)   *retval |= IBCS2_APPEND;
    		if (result & O_FSYNC)    *retval |= IBCS2_SYNC;
		return error;

  	case IBCS2_F_SETFL:
    		if (args->arg & IBCS2_NDELAY)   fcntl_args.arg |= O_NONBLOCK;
    		if (args->arg & IBCS2_APPEND)   fcntl_args.arg |= O_APPEND;
    		if (args->arg & IBCS2_SYNC)     fcntl_args.arg |= O_FSYNC;
  		fcntl_args.cmd = F_SETFL;
		return fcntl(p, &fcntl_args, retval);
  	
	case IBCS2_F_GETLK:
		if ((error = copyin((caddr_t)args->arg, (caddr_t)&ibcs2_flock,
			       sizeof(struct ibcs2_flock)))) 
			return error;
		ibcs2_to_bsd_flock(&ibcs2_flock, bsd_flock);
		fcntl_args.cmd = F_GETLK;
		fcntl_args.arg = (int)bsd_flock;
		if (error = fcntl(p, &fcntl_args, retval))
			return error;
		bsd_to_ibcs2_flock(bsd_flock, &ibcs2_flock);
		return copyout((caddr_t)&ibcs2_flock, (caddr_t)args->arg,
			       sizeof(struct ibcs2_flock));

	case IBCS2_F_SETLK:
		if ((error = copyin((caddr_t)args->arg, (caddr_t)&ibcs2_flock,
			       sizeof(struct ibcs2_flock)))) 
			return error;
		ibcs2_to_bsd_flock(&ibcs2_flock, bsd_flock);
		fcntl_args.cmd = F_SETLK;
		fcntl_args.arg = (int)bsd_flock;
		return fcntl(p, &fcntl_args, retval);

	case IBCS2_F_SETLKW:
		if ((error = copyin((caddr_t)args->arg, (caddr_t)&ibcs2_flock,
			       sizeof(struct ibcs2_flock)))) 
			return error;
		ibcs2_to_bsd_flock(&ibcs2_flock, bsd_flock);
		fcntl_args.cmd = F_SETLKW;
		fcntl_args.arg = (int)bsd_flock;
		return fcntl(p, &fcntl_args, retval);
	}
	return EINVAL;
}

struct ibcs2_dirent {
	ibcs2_ino_t d_ino;
	ibcs2_off_t d_off;
	unsigned short d_reclen;
	char d_name[256];
};

struct ibcs2_getdents_args {
	int	fd;
	char	*buf;
	int	nbytes;
};

#define	BSD_DIRENT(cp) ((struct direct *)(cp))
#define	IBCS2_RECLEN(p) \
	(((2*sizeof(long)+sizeof(short)+BSD_DIRENT(p)->d_namlen+1)+3)&~3)

int
ibcs2_getdents(struct proc *p, struct ibcs2_getdents_args *args, int *retval)
{
	struct vnode *vp;
	struct ibcs2_dirent dir;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	off_t off;
	caddr_t inp, buf, outp;
	int len, reclen, resid;
	int buflen, error, eofflag;

	if (ibcs2_trace & IBCS2_TRACE_FILE)
		printf("IBCS2: 'getdents' fd = %d, size = %d\n",
			args->fd, args->nbytes);
	if ((error = getvnode(p->p_fd, args->fd, &fp)) != 0)
		return error;
	if ((fp->f_flag & FREAD) == 0)
		return EBADF;
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR)
		return EINVAL;
	buflen = min(DEFAULT_PAGE_SIZE, max(DIRBLKSIZ, args->nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	VOP_LOCK(vp);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off & ~(DIRBLKSIZ-1);
	if (error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, NULL, NULL))
		goto out;
	inp = buf + (off & (DIRBLKSIZ-1));
	buflen -= off & (DIRBLKSIZ-1);
	outp = args->buf;
	resid = args->nbytes;
	if ((len = (buflen - auio.uio_resid)) <= 0)
		goto eof;
	for (; len > 0 && resid > 0; len -= reclen) {
		reclen = BSD_DIRENT(inp)->d_reclen;
		if (reclen & 3)
			panic("ibcs2_getdents");
		if (BSD_DIRENT(inp)->d_ino == 0) {
			off += reclen;
			inp += reclen;
			continue;
		}
		if (IBCS2_RECLEN(inp) > len || resid < IBCS2_RECLEN(inp)) {
			outp++;
			break;
		}
		bzero(&dir, sizeof(struct ibcs2_dirent));
		dir.d_ino = BSD_DIRENT(inp)->d_ino;
		dir.d_off = (ibcs2_off_t)off;
		dir.d_reclen = IBCS2_RECLEN(inp);
		bcopy(BSD_DIRENT(inp)->d_name, &dir.d_name, 
		      BSD_DIRENT(inp)->d_namlen);
		if (error = copyout(&dir, outp, dir.d_reclen))
			goto out;
		outp += IBCS2_RECLEN(inp);
		resid -= IBCS2_RECLEN(inp);
		off += reclen;
		inp += reclen;
	}
	if (outp == args->buf)
		goto again;
	fp->f_offset = off;
eof:
	*retval = args->nbytes - resid;
out:
	VOP_UNLOCK(vp);
	free(buf, M_TEMP);
	return error;
}

struct ibcs2_open_args {
	char	*fname;
	int	fmode;
	int	crtmode;
};

int
ibcs2_open(struct proc *p, struct ibcs2_open_args *args, int *retval)
{
	int bsd_mode = 0;
	int noctty = args->fmode & 0x8000;
	int error;
	
	if (ibcs2_trace & IBCS2_TRACE_FILE)
		printf("IBCS2: 'open' name=%s, flags=%x, mode=%x\n",
			args->fname, args->fmode, args->crtmode);
    	if (args->fmode & IBCS2_RDONLY)   bsd_mode |= O_RDONLY;
    	if (args->fmode & IBCS2_WRONLY)   bsd_mode |= O_WRONLY;
    	if (args->fmode & IBCS2_RDWR)     bsd_mode |= O_RDWR;
    	if (args->fmode & IBCS2_NDELAY)   bsd_mode |= O_NONBLOCK;
    	if (args->fmode & IBCS2_APPEND)   bsd_mode |= O_APPEND;
    	if (args->fmode & IBCS2_SYNC)     bsd_mode |= O_FSYNC;
    	if (args->fmode & IBCS2_NONBLOCK) bsd_mode |= O_NONBLOCK;
	if (args->fmode & IBCS2_PRIV) 	  bsd_mode |= O_EXLOCK;
    	if (args->fmode & IBCS2_CREAT)    bsd_mode |= O_CREAT;
    	if (args->fmode & IBCS2_TRUNC)    bsd_mode |= O_TRUNC | O_CREAT;
    	if (args->fmode & IBCS2_EXCL)     bsd_mode |= O_EXCL;
    	if (args->fmode & IBCS2_NOCTTY)   bsd_mode |= O_NOCTTY;
	args->fmode = bsd_mode;
	error = open(p, args, retval);
	if (!error && !noctty && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
		struct filedesc *fdp = p->p_fd;
		struct file *fp = fdp->fd_ofiles[*retval];

		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, (caddr_t) 0, p);
	}
	return error;
}

struct ibcs2_read_args {
	int fd;
	char *buffer;
	unsigned int count;
};

int
ibcs2_read(struct proc *p, struct ibcs2_read_args *args, int *retval)
{
	struct ibcs2_dir {
		ibcs2_ino_t inode;
		char fname[14];
	} dirbuf;
	struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	caddr_t inp, buf, outp;
	off_t off;
	int buflen, error, eofflag;
	int len, reclen, resid;

	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'read' fd=%d, count=%d\n",
			args->fd, args->count);

	if (error = getvnode(p->p_fd, args->fd, &fp))
		if (error == EINVAL)
			return read(p, args, retval);
		else
			return error;
	if ((fp->f_flag & FREAD) == 0)
		return EBADF;
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {
		if (ibcs2_trace & IBCS2_TRACE_FILE)
			printf("normal read\n");
		return read(p, args, retval);
	}
	if (ibcs2_trace & IBCS2_TRACE_FILE)
		printf("read directory\n");
	buflen = min(DEFAULT_PAGE_SIZE, max(DIRBLKSIZ, args->count));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	VOP_LOCK(vp);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off & ~(DIRBLKSIZ-1);
	if (error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, NULL, NULL))
		goto out;
	inp = buf + (off & (DIRBLKSIZ-1));
	buflen -= off & (DIRBLKSIZ-1);
	outp = args->buffer;
	resid = args->count;
	if ((len = (buflen - auio.uio_resid)) == 0)
		goto eof;
	for (; len > 0 && resid > 0; len -= reclen) {
		reclen = BSD_DIRENT(inp)->d_reclen;
		if (reclen & 3)
			panic("ibcs2_read");
		if (BSD_DIRENT(inp)->d_ino == 0) {
			inp += reclen;
			off += reclen;
			continue;
		}
		if (reclen > len || resid < sizeof(struct ibcs2_dir)) {
			outp++;
			break;
		}
		/* 
		 * TODO: break up name if > 14 chars
		 * put 14 chars in each dir entry wtih d_ino = 0xffff
		 * and set the last dir entry's d_ino = inode
		 */
		dirbuf.inode = (BSD_DIRENT(inp)->d_ino > 0x0000fffe) ?
			0xfffe : BSD_DIRENT(inp)->d_ino;
		bcopy(BSD_DIRENT(inp)->d_name, &dirbuf.fname, 14); /* XXX */
		if (error = copyout(&dirbuf, outp, sizeof(dirbuf)))
			goto out;
		off += reclen;
		inp += reclen;
		outp += sizeof(struct ibcs2_dir);
		resid -= sizeof(struct ibcs2_dir);
	}
	if (outp == args->buffer)
		goto again;
	fp->f_offset = off;
eof:
	*retval = args->count - resid;
out:
	VOP_UNLOCK(vp);
	free(buf, M_TEMP);
	return error;
}

struct ibcs2_seek_args {
	int fdes;
	ibcs2_off_t off;
	int whence;
};

int
ibcs2_seek(struct proc *p, struct ibcs2_seek_args *args, int *retval)
{

	struct seek_args {
		int fdes;
		int pad;
		off_t off;
		int whence;
	} tmp_args;
	off_t tmp_retval;
	int error;

	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'seek' fd=%d, offset=%d, how=%d\n",
		       args->fdes, args->off, args->whence);
	tmp_args.fdes = args->fdes;
	tmp_args.off = (off_t)args->off;
	tmp_args.whence = args->whence;
	error = lseek(p, &tmp_args, &tmp_retval);
	*retval = (int)tmp_retval;
	return error;
}

struct ibcs2_write_args {
	int fd;
	char *buffer;
	unsigned int count;
};

int
ibcs2_write(struct proc *p, struct ibcs2_write_args *args, int *retval)
{
	if (ibcs2_trace & IBCS2_TRACE_FILE) 
		printf("IBCS2: 'write' fd=%d, count=%d\n",
			args->fd, args->count);
	return write(p, args, retval);
}
