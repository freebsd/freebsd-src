/*
 * Copyright (c) 1995 Steven Wallace
 * Copyright (c) 1994, 1995 Scott Bartram
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 * from: Header: sun_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
 *
 *	@(#)sun_misc.c	8.1 (Berkeley) 6/18/93
 *
 * $FreeBSD$
 */

/*
 * IBCS2 compatibility module.
 *
 * IBCS2 system calls that are implemented differently in BSD are
 * handled here.
 */
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/file.h>			/* Must come after sys/malloc.h */
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <machine/cpu.h>

#include <i386/ibcs2/ibcs2_dirent.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_unistd.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_utime.h>
#include <i386/ibcs2/ibcs2_xenix.h>

int
ibcs2_ulimit(td, uap)
	struct thread *td;
	struct ibcs2_ulimit_args *uap;
{
#ifdef notyet
	int error;
	struct rlimit rl;
	struct setrlimit_args {
		int resource;
		struct rlimit *rlp;
	} sra;
#endif
#define IBCS2_GETFSIZE		1
#define IBCS2_SETFSIZE		2
#define IBCS2_GETPSIZE		3
#define IBCS2_GETDTABLESIZE	4
	
	switch (uap->cmd) {
	case IBCS2_GETFSIZE:
		td->td_retval[0] = td->td_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur;
		if (td->td_retval[0] == -1) td->td_retval[0] = 0x7fffffff;
		return 0;
	case IBCS2_SETFSIZE:	/* XXX - fix this */
#ifdef notyet
		rl.rlim_cur = uap->newlimit;
		sra.resource = RLIMIT_FSIZE;
		sra.rlp = &rl;
		error = setrlimit(td, &sra);
		if (!error)
			td->td_retval[0] = td->td_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur;
		else
			DPRINTF(("failed "));
		return error;
#else
		td->td_retval[0] = uap->newlimit;
		return 0;
#endif
	case IBCS2_GETPSIZE:
		mtx_assert(&Giant, MA_OWNED);
		td->td_retval[0] = td->td_proc->p_rlimit[RLIMIT_RSS].rlim_cur; /* XXX */
		return 0;
	case IBCS2_GETDTABLESIZE:
		uap->cmd = IBCS2_SC_OPEN_MAX;
		return ibcs2_sysconf(td, (struct ibcs2_sysconf_args *)uap);
	default:
		return ENOSYS;
	}
}

#define IBCS2_WSTOPPED       0177
#define IBCS2_STOPCODE(sig)  ((sig) << 8 | IBCS2_WSTOPPED)
int
ibcs2_wait(td, uap)
	struct thread *td;
	struct ibcs2_wait_args *uap;
{
	int error, status;
	struct wait_args w4;
        struct trapframe *tf = td->td_frame;
	
	w4.rusage = NULL;
        if ((tf->tf_eflags & (PSL_Z|PSL_PF|PSL_N|PSL_V))
            == (PSL_Z|PSL_PF|PSL_N|PSL_V)) {
		/* waitpid */
		w4.pid = uap->a1;
		w4.status = (int *)uap->a2;
		w4.options = uap->a3;
	} else {
		/* wait */
		w4.pid = WAIT_ANY;
		w4.status = (int *)uap->a1;
		w4.options = 0;
	}
	if ((error = wait4(td, &w4)) != 0)
		return error;
	if (w4.status)	{	/* this is real iBCS brain-damage */
		error = copyin((caddr_t)w4.status, (caddr_t)&status,
			       sizeof(w4.status));
		if(error)
		  return error;

		/* convert status/signal result */
		if(WIFSTOPPED(status))
			status =
			  IBCS2_STOPCODE(bsd_to_ibcs2_sig[_SIG_IDX(WSTOPSIG(status))]);
		else if(WIFSIGNALED(status))
			status = bsd_to_ibcs2_sig[_SIG_IDX(WTERMSIG(status))];
		/* else exit status -- identical */

		/* record result/status */
		td->td_retval[1] = status;
		return copyout((caddr_t)&status, (caddr_t)w4.status,
			       sizeof(w4.status));
	}

	return 0;
}

int
ibcs2_execv(td, uap)
	struct thread *td;
	struct ibcs2_execv_args *uap;
{
	struct execve_args ea;
	caddr_t sg = stackgap_init();

        CHECKALTEXIST(td, &sg, uap->path);
	ea.fname = uap->path;
	ea.argv = uap->argp;
	ea.envv = NULL;
	return execve(td, &ea);
}

int
ibcs2_execve(td, uap) 
        struct thread *td;
        struct ibcs2_execve_args *uap;
{
        caddr_t sg = stackgap_init();
        CHECKALTEXIST(td, &sg, uap->path);
        return execve(td, (struct execve_args *)uap);
}

int
ibcs2_umount(td, uap)
	struct thread *td;
	struct ibcs2_umount_args *uap;
{
	struct unmount_args um;

	um.path = uap->name;
	um.flags = 0;
	return unmount(td, &um);
}

int
ibcs2_mount(td, uap)
	struct thread *td;
	struct ibcs2_mount_args *uap;
{
#ifdef notyet
	int oflags = uap->flags, nflags, error;
	char fsname[MFSNAMELEN];

	if (oflags & (IBCS2_MS_NOSUB | IBCS2_MS_SYS5))
		return (EINVAL);
	if ((oflags & IBCS2_MS_NEWTYPE) == 0)
		return (EINVAL);
	nflags = 0;
	if (oflags & IBCS2_MS_RDONLY)
		nflags |= MNT_RDONLY;
	if (oflags & IBCS2_MS_NOSUID)
		nflags |= MNT_NOSUID;
	if (oflags & IBCS2_MS_REMOUNT)
		nflags |= MNT_UPDATE;
	uap->flags = nflags;

	if (error = copyinstr((caddr_t)uap->type, fsname, sizeof fsname,
			      (u_int *)0))
		return (error);

	if (strcmp(fsname, "4.2") == 0) {
		uap->type = (caddr_t)STACK_ALLOC();
		if (error = copyout("ufs", uap->type, sizeof("ufs")))
			return (error);
	} else if (strcmp(fsname, "nfs") == 0) {
		struct ibcs2_nfs_args sna;
		struct sockaddr_in sain;
		struct nfs_args na;
		struct sockaddr sa;

		if (error = copyin(uap->data, &sna, sizeof sna))
			return (error);
		if (error = copyin(sna.addr, &sain, sizeof sain))
			return (error);
		bcopy(&sain, &sa, sizeof sa);
		sa.sa_len = sizeof(sain);
		uap->data = (caddr_t)STACK_ALLOC();
		na.addr = (struct sockaddr *)((int)uap->data + sizeof na);
		na.sotype = SOCK_DGRAM;
		na.proto = IPPROTO_UDP;
		na.fh = (nfsv2fh_t *)sna.fh;
		na.flags = sna.flags;
		na.wsize = sna.wsize;
		na.rsize = sna.rsize;
		na.timeo = sna.timeo;
		na.retrans = sna.retrans;
		na.hostname = sna.hostname;

		if (error = copyout(&sa, na.addr, sizeof sa))
			return (error);
		if (error = copyout(&na, uap->data, sizeof na))
			return (error);
	}
	return (mount(td, uap));
#else
	return EINVAL;
#endif
}

/*
 * Read iBCS2-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  Like
 * SunOS, we squish out `empty' entries.
 *
 * This is quite ugly, but what do you expect from compatibility code?
 */

int
ibcs2_getdents(td, uap)
	struct thread *td;
	register struct ibcs2_getdents_args *uap;
{
	register struct vnode *vp;
	register caddr_t inp, buf;	/* BSD-format */
	register int len, reclen;	/* BSD-format */
	register caddr_t outp;		/* iBCS2-format */
	register int resid;		/* iBCS2-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct ibcs2_dirent idb;
	off_t off;			/* true file offset */
	int buflen, error, eofflag;
	u_long *cookies = NULL, *cookiep;
	int ncookies;
#define	BSD_DIRENT(cp)		((struct dirent *)(cp))
#define	IBCS2_RECLEN(reclen)	(reclen + sizeof(u_short))

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {	/* XXX  vnode readdir op should do this */
		fdrop(fp, td);
		return (EINVAL);
	}

	off = fp->f_offset;
#define	DIRBLKSIZ	512		/* XXX we used to use ufs's DIRBLKSIZ */
	buflen = max(DIRBLKSIZ, uap->nbytes);
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

#ifdef MAC
	error = mac_check_vnode_readdir(td->td_ucred, vp);
	if (error)
		goto out;
#endif

	/*
	 * First we read into the malloc'ed buffer, then
	 * we massage it into user space, one record at a time.
	 */
	if ((error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies, &cookies)) != 0)
		goto out;
	inp = buf;
	outp = uap->buf;
	resid = uap->nbytes;
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
			len -= BSD_DIRENT(inp)->d_reclen;
			inp += BSD_DIRENT(inp)->d_reclen;
			cookiep++;
			ncookies--;
		}
	}

	for (; len > 0; len -= reclen) {
		if (cookiep && ncookies == 0)
			break;
		reclen = BSD_DIRENT(inp)->d_reclen;
		if (reclen & 3) {
		        printf("ibcs2_getdents: reclen=%d\n", reclen);
		        error = EFAULT;
			goto out;
		}
		if (BSD_DIRENT(inp)->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			if (cookiep) {
				off = *cookiep++;
				ncookies--;
			} else
				off += reclen;
			continue;
		}
		if (reclen > len || resid < IBCS2_RECLEN(reclen)) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make an iBCS2-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = (ibcs2_ino_t)BSD_DIRENT(inp)->d_fileno;
		idb.d_off = (ibcs2_off_t)off;
		idb.d_reclen = (u_short)IBCS2_RECLEN(reclen);
		if ((error = copyout((caddr_t)&idb, outp, 10)) != 0 ||
		    (error = copyout(BSD_DIRENT(inp)->d_name, outp + 10,
				     BSD_DIRENT(inp)->d_namlen + 1)) != 0)
			goto out;
		/* advance past this real entry */
		if (cookiep) {
			off = *cookiep++;
			ncookies--;
		} else
			off += reclen;
		inp += reclen;
		/* advance output past iBCS2-shaped entry */
		outp += IBCS2_RECLEN(reclen);
		resid -= IBCS2_RECLEN(reclen);
	}
	/* if we squished out the whole block, try again */
	if (outp == uap->buf)
		goto again;
	fp->f_offset = off;		/* update the vnode offset */
eof:
	td->td_retval[0] = uap->nbytes - resid;
out:
	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	if (cookies)
		free(cookies, M_TEMP);
	free(buf, M_TEMP);
	return (error);
}

int
ibcs2_read(td, uap)
	struct thread *td;
	struct ibcs2_read_args *uap;
{
	register struct vnode *vp;
	register caddr_t inp, buf;	/* BSD-format */
	register int len, reclen;	/* BSD-format */
	register caddr_t outp;		/* iBCS2-format */
	register int resid;		/* iBCS2-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct ibcs2_direct {
		ibcs2_ino_t ino;
		char name[14];
	} idb;
	off_t off;			/* true file offset */
	int buflen, error, eofflag, size;
	u_long *cookies = NULL, *cookiep;
	int ncookies;

	if ((error = getvnode(td->td_proc->p_fd, uap->fd, &fp)) != 0) {
		if (error == EINVAL)
			return read(td, (struct read_args *)uap);
		else
			return error;
	}
	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return read(td, (struct read_args *)uap);
	}

	off = fp->f_offset;
	if (vp->v_type != VDIR)
		return read(td, (struct read_args *)uap);

	DPRINTF(("ibcs2_read: read directory\n"));

	buflen = max(DIRBLKSIZ, uap->nbytes);
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

#ifdef MAC
	error = mac_check_vnode_readdir(td->td_ucred, vp);
	if (error)
		goto out;
#endif

	/*
	 * First we read into the malloc'ed buffer, then
	 * we massage it into user space, one record at a time.
	 */
	if ((error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies, &cookies)) != 0) {
		DPRINTF(("VOP_READDIR failed: %d\n", error));
		goto out;
	}
	inp = buf;
	outp = uap->buf;
	resid = uap->nbytes;
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
			len -= BSD_DIRENT(inp)->d_reclen;
			inp += BSD_DIRENT(inp)->d_reclen;
			cookiep++;
			ncookies--;
		}
	}

	for (; len > 0 && resid > 0; len -= reclen) {
		if (cookiep && ncookies == 0)
			break;
		reclen = BSD_DIRENT(inp)->d_reclen;
		if (reclen & 3) {
		        printf("ibcs2_read: reclen=%d\n", reclen);
		        error = EFAULT;
			goto out;
		}
		if (BSD_DIRENT(inp)->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			if (cookiep) {
				off = *cookiep++;
				ncookies--;
			} else
				off += reclen;
			continue;
		}
		if (reclen > len || resid < sizeof(struct ibcs2_direct)) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make an iBCS2-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 *
		 * TODO: if length(filename) > 14, then break filename into
		 * multiple entries and set inode = 0xffff except last
		 */
		idb.ino = (BSD_DIRENT(inp)->d_fileno > 0xfffe) ? 0xfffe :
			BSD_DIRENT(inp)->d_fileno;
		(void)copystr(BSD_DIRENT(inp)->d_name, idb.name, 14, &size);
		bzero(idb.name + size, 14 - size);
		if ((error = copyout(&idb, outp, sizeof(struct ibcs2_direct))) != 0)
			goto out;
		/* advance past this real entry */
		if (cookiep) {
			off = *cookiep++;
			ncookies--;
		} else
			off += reclen;
		inp += reclen;
		/* advance output past iBCS2-shaped entry */
		outp += sizeof(struct ibcs2_direct);
		resid -= sizeof(struct ibcs2_direct);
	}
	/* if we squished out the whole block, try again */
	if (outp == uap->buf)
		goto again;
	fp->f_offset = off;		/* update the vnode offset */
eof:
	td->td_retval[0] = uap->nbytes - resid;
out:
	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	if (cookies)
		free(cookies, M_TEMP);
	free(buf, M_TEMP);
	return (error);
}

int
ibcs2_mknod(td, uap)
	struct thread *td;
	struct ibcs2_mknod_args *uap;
{
        caddr_t sg = stackgap_init();

        CHECKALTCREAT(td, &sg, uap->path);
	if (S_ISFIFO(uap->mode)) {
                struct mkfifo_args ap;
                ap.path = uap->path;
                ap.mode = uap->mode;
		return mkfifo(td, &ap);
	} else {
                struct mknod_args ap;
                ap.path = uap->path;
                ap.mode = uap->mode;
                ap.dev = uap->dev;
                return mknod(td, &ap);
	}
}

int
ibcs2_getgroups(td, uap)
	struct thread *td;
	struct ibcs2_getgroups_args *uap;
{
	int error, i;
	ibcs2_gid_t *iset = NULL;
	struct getgroups_args sa;
	gid_t *gp;
	caddr_t sg = stackgap_init();

	sa.gidsetsize = uap->gidsetsize;
	if (uap->gidsetsize) {
		sa.gidset = stackgap_alloc(&sg, NGROUPS_MAX *
						    sizeof(gid_t *));
		iset = stackgap_alloc(&sg, uap->gidsetsize *
				      sizeof(ibcs2_gid_t));
	}
	if ((error = getgroups(td, &sa)) != 0)
		return error;
	if (uap->gidsetsize == 0)
		return 0;

	for (i = 0, gp = sa.gidset; i < td->td_retval[0]; i++)
		iset[i] = (ibcs2_gid_t)*gp++;
	if (td->td_retval[0] && (error = copyout((caddr_t)iset,
					  (caddr_t)uap->gidset,
					  sizeof(ibcs2_gid_t) * td->td_retval[0])))
		return error;
        return 0;
}

int
ibcs2_setgroups(td, uap)
	struct thread *td;
	struct ibcs2_setgroups_args *uap;
{
	int error, i;
	ibcs2_gid_t *iset;
	struct setgroups_args sa;
	gid_t *gp;
	caddr_t sg = stackgap_init();

	sa.gidsetsize = uap->gidsetsize;
	sa.gidset = stackgap_alloc(&sg, sa.gidsetsize *
					    sizeof(gid_t *));
	iset = stackgap_alloc(&sg, sa.gidsetsize *
			      sizeof(ibcs2_gid_t *));
	if (sa.gidsetsize) {
		if ((error = copyin((caddr_t)uap->gidset, (caddr_t)iset, 
				   sizeof(ibcs2_gid_t *) *
				   uap->gidsetsize)) != 0)
			return error;
	}
	for (i = 0, gp = sa.gidset; i < sa.gidsetsize; i++)
		*gp++ = (gid_t)iset[i];
	return setgroups(td, &sa);
}

int
ibcs2_setuid(td, uap)
	struct thread *td;
	struct ibcs2_setuid_args *uap;
{
	struct setuid_args sa;

	sa.uid = (uid_t)uap->uid;
	return setuid(td, &sa);
}

int
ibcs2_setgid(td, uap)
	struct thread *td;
	struct ibcs2_setgid_args *uap;
{
	struct setgid_args sa;

	sa.gid = (gid_t)uap->gid;
	return setgid(td, &sa);
}

int
ibcs2_time(td, uap)
	struct thread *td;
	struct ibcs2_time_args *uap;
{
	struct timeval tv;

	microtime(&tv);
	td->td_retval[0] = tv.tv_sec;
	if (uap->tp)
		return copyout((caddr_t)&tv.tv_sec, (caddr_t)uap->tp,
			       sizeof(ibcs2_time_t));
	else
		return 0;
}

int
ibcs2_pathconf(td, uap)
	struct thread *td;
	struct ibcs2_pathconf_args *uap;
{
	uap->name++;	/* iBCS2 _PC_* defines are offset by one */
        return pathconf(td, (struct pathconf_args *)uap);
}

int
ibcs2_fpathconf(td, uap)
	struct thread *td;
	struct ibcs2_fpathconf_args *uap;
{
	uap->name++;	/* iBCS2 _PC_* defines are offset by one */
        return fpathconf(td, (struct fpathconf_args *)uap);
}

int
ibcs2_sysconf(td, uap)
	struct thread *td;
	struct ibcs2_sysconf_args *uap;
{
	int mib[2], value, len, error;
	struct sysctl_args sa;
	struct __getrlimit_args ga;

	switch(uap->name) {
	case IBCS2_SC_ARG_MAX:
		mib[1] = KERN_ARGMAX;
		break;

	case IBCS2_SC_CHILD_MAX:
	    {
		caddr_t sg = stackgap_init();

		ga.which = RLIMIT_NPROC;
		ga.rlp = stackgap_alloc(&sg, sizeof(struct rlimit *));
		if ((error = getrlimit(td, &ga)) != 0)
			return error;
		td->td_retval[0] = ga.rlp->rlim_cur;
		return 0;
	    }

	case IBCS2_SC_CLK_TCK:
		td->td_retval[0] = hz;
		return 0;

	case IBCS2_SC_NGROUPS_MAX:
		mib[1] = KERN_NGROUPS;
		break;

	case IBCS2_SC_OPEN_MAX:
	    {
		caddr_t sg = stackgap_init();

		ga.which = RLIMIT_NOFILE;
		ga.rlp = stackgap_alloc(&sg, sizeof(struct rlimit *));
		if ((error = getrlimit(td, &ga)) != 0)
			return error;
		td->td_retval[0] = ga.rlp->rlim_cur;
		return 0;
	    }
		
	case IBCS2_SC_JOB_CONTROL:
		mib[1] = KERN_JOB_CONTROL;
		break;
		
	case IBCS2_SC_SAVED_IDS:
		mib[1] = KERN_SAVED_IDS;
		break;
		
	case IBCS2_SC_VERSION:
		mib[1] = KERN_POSIX1;
		break;
		
	case IBCS2_SC_PASS_MAX:
		td->td_retval[0] = 128;		/* XXX - should we create PASS_MAX ? */
		return 0;

	case IBCS2_SC_XOPEN_VERSION:
		td->td_retval[0] = 2;		/* XXX: What should that be? */
		return 0;
		
	default:
		return EINVAL;
	}

	mib[0] = CTL_KERN;
	len = sizeof(value);
	sa.name = mib;
	sa.namelen = 2;
	sa.old = &value;
	sa.oldlenp = &len;
	sa.new = NULL;
	sa.newlen = 0;
	if ((error = __sysctl(td, &sa)) != 0)
		return error;
	td->td_retval[0] = value;
	return 0;
}

int
ibcs2_alarm(td, uap)
	struct thread *td;
	struct ibcs2_alarm_args *uap;
{
	int error;
        struct itimerval *itp, *oitp;
	struct setitimer_args sa;
	caddr_t sg = stackgap_init();

        itp = stackgap_alloc(&sg, sizeof(*itp));
	oitp = stackgap_alloc(&sg, sizeof(*oitp));
        timevalclear(&itp->it_interval);
        itp->it_value.tv_sec = uap->sec;
        itp->it_value.tv_usec = 0;

	sa.which = ITIMER_REAL;
	sa.itv = itp;
	sa.oitv = oitp;
        error = setitimer(td, &sa);
	if (error)
		return error;
        if (oitp->it_value.tv_usec)
                oitp->it_value.tv_sec++;
        td->td_retval[0] = oitp->it_value.tv_sec;
        return 0;
}

int
ibcs2_times(td, uap)
	struct thread *td;
	struct ibcs2_times_args *uap;
{
	int error;
	struct getrusage_args ga;
	struct tms tms;
        struct timeval t;
	caddr_t sg = stackgap_init();
        struct rusage *ru = stackgap_alloc(&sg, sizeof(*ru));
#define CONVTCK(r)      (r.tv_sec * hz + r.tv_usec / (1000000 / hz))

	ga.who = RUSAGE_SELF;
	ga.rusage = ru;
	error = getrusage(td, &ga);
	if (error)
                return error;
        tms.tms_utime = CONVTCK(ru->ru_utime);
        tms.tms_stime = CONVTCK(ru->ru_stime);

	ga.who = RUSAGE_CHILDREN;
        error = getrusage(td, &ga);
	if (error)
		return error;
        tms.tms_cutime = CONVTCK(ru->ru_utime);
        tms.tms_cstime = CONVTCK(ru->ru_stime);

	microtime(&t);
        td->td_retval[0] = CONVTCK(t);
	
	return copyout((caddr_t)&tms, (caddr_t)uap->tp,
		       sizeof(struct tms));
}

int
ibcs2_stime(td, uap)
	struct thread *td;
	struct ibcs2_stime_args *uap;
{
	int error;
	struct settimeofday_args sa;
	caddr_t sg = stackgap_init();

	sa.tv = stackgap_alloc(&sg, sizeof(*sa.tv));
	sa.tzp = NULL;
	if ((error = copyin((caddr_t)uap->timep,
			   &(sa.tv->tv_sec), sizeof(long))) != 0)
		return error;
	sa.tv->tv_usec = 0;
	if ((error = settimeofday(td, &sa)) != 0)
		return EPERM;
	return 0;
}

int
ibcs2_utime(td, uap)
	struct thread *td;
	struct ibcs2_utime_args *uap;
{
	int error;
	struct utimes_args sa;
	struct timeval *tp;
	caddr_t sg = stackgap_init();

        CHECKALTEXIST(td, &sg, uap->path);
	sa.path = uap->path;
	if (uap->buf) {
		struct ibcs2_utimbuf ubuf;

		if ((error = copyin((caddr_t)uap->buf, (caddr_t)&ubuf,
				   sizeof(ubuf))) != 0)
			return error;
		sa.tptr = stackgap_alloc(&sg,
						  2 * sizeof(struct timeval *));
		tp = (struct timeval *)sa.tptr;
		tp->tv_sec = ubuf.actime;
		tp->tv_usec = 0;
		tp++;
		tp->tv_sec = ubuf.modtime;
		tp->tv_usec = 0;
	} else
		sa.tptr = NULL;
	return utimes(td, &sa);
}

int
ibcs2_nice(td, uap)
	struct thread *td;
	struct ibcs2_nice_args *uap;
{
	int error;
	struct setpriority_args sa;

	sa.which = PRIO_PROCESS;
	sa.who = 0;
	sa.prio = td->td_ksegrp->kg_nice + uap->incr;
	if ((error = setpriority(td, &sa)) != 0)
		return EPERM;
	td->td_retval[0] = td->td_ksegrp->kg_nice;
	return 0;
}

/*
 * iBCS2 getpgrp, setpgrp, setsid, and setpgid
 */

int
ibcs2_pgrpsys(td, uap)
	struct thread *td;
	struct ibcs2_pgrpsys_args *uap;
{
	struct proc *p = td->td_proc;
	switch (uap->type) {
	case 0:			/* getpgrp */
		PROC_LOCK(p);
		td->td_retval[0] = p->p_pgrp->pg_id;
		PROC_UNLOCK(p);
		return 0;

	case 1:			/* setpgrp */
	    {
		struct setpgid_args sa;

		sa.pid = 0;
		sa.pgid = 0;
		setpgid(td, &sa);
		PROC_LOCK(p);
		td->td_retval[0] = p->p_pgrp->pg_id;
		PROC_UNLOCK(p);
		return 0;
	    }

	case 2:			/* setpgid */
	    {
		struct setpgid_args sa;

		sa.pid = uap->pid;
		sa.pgid = uap->pgid;
		return setpgid(td, &sa);
	    }

	case 3:			/* setsid */
		return setsid(td, NULL);

	default:
		return EINVAL;
	}
}

/*
 * XXX - need to check for nested calls
 */

int
ibcs2_plock(td, uap)
	struct thread *td;
	struct ibcs2_plock_args *uap;
{
	int error;
#define IBCS2_UNLOCK	0
#define IBCS2_PROCLOCK	1
#define IBCS2_TEXTLOCK	2
#define IBCS2_DATALOCK	4

	
        if ((error = suser(td)) != 0)
                return EPERM;
	switch(uap->cmd) {
	case IBCS2_UNLOCK:
	case IBCS2_PROCLOCK:
	case IBCS2_TEXTLOCK:
	case IBCS2_DATALOCK:
		return 0;	/* XXX - TODO */
	}
	return EINVAL;
}

int
ibcs2_uadmin(td, uap)
	struct thread *td;
	struct ibcs2_uadmin_args *uap;
{
#define SCO_A_REBOOT        1
#define SCO_A_SHUTDOWN      2
#define SCO_A_REMOUNT       4
#define SCO_A_CLOCK         8
#define SCO_A_SETCONFIG     128
#define SCO_A_GETDEV        130

#define SCO_AD_HALT         0
#define SCO_AD_BOOT         1
#define SCO_AD_IBOOT        2
#define SCO_AD_PWRDOWN      3
#define SCO_AD_PWRNAP       4

#define SCO_AD_PANICBOOT    1

#define SCO_AD_GETBMAJ      0
#define SCO_AD_GETCMAJ      1

        if (suser(td))
                return EPERM;

	switch(uap->cmd) {
	case SCO_A_REBOOT:
	case SCO_A_SHUTDOWN:
		switch(uap->func) {
			struct reboot_args r;
		case SCO_AD_HALT:
		case SCO_AD_PWRDOWN:
		case SCO_AD_PWRNAP:
			r.opt = RB_HALT;
			reboot(td, &r);
		case SCO_AD_BOOT:
		case SCO_AD_IBOOT:
			r.opt = RB_AUTOBOOT;
			reboot(td, &r);
		}
		return EINVAL;
	case SCO_A_REMOUNT:
	case SCO_A_CLOCK:
	case SCO_A_SETCONFIG:
		return 0;
	case SCO_A_GETDEV:
		return EINVAL;	/* XXX - TODO */
	}
	return EINVAL;
}

int
ibcs2_sysfs(td, uap)
	struct thread *td;
	struct ibcs2_sysfs_args *uap;
{
#define IBCS2_GETFSIND        1
#define IBCS2_GETFSTYP        2
#define IBCS2_GETNFSTYP       3

	switch(uap->cmd) {
	case IBCS2_GETFSIND:
	case IBCS2_GETFSTYP:
	case IBCS2_GETNFSTYP:
		break;
	}
	return EINVAL;		/* XXX - TODO */
}

int
ibcs2_unlink(td, uap)
	struct thread *td;
	struct ibcs2_unlink_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return unlink(td, (struct unlink_args *)uap);
}

int
ibcs2_chdir(td, uap)
	struct thread *td;
	struct ibcs2_chdir_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return chdir(td, (struct chdir_args *)uap);
}

int
ibcs2_chmod(td, uap)
	struct thread *td;
	struct ibcs2_chmod_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return chmod(td, (struct chmod_args *)uap);
}

int
ibcs2_chown(td, uap)
	struct thread *td;
	struct ibcs2_chown_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return chown(td, (struct chown_args *)uap);
}

int
ibcs2_rmdir(td, uap)
	struct thread *td;
	struct ibcs2_rmdir_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return rmdir(td, (struct rmdir_args *)uap);
}

int
ibcs2_mkdir(td, uap)
	struct thread *td;
	struct ibcs2_mkdir_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTCREAT(td, &sg, uap->path);
	return mkdir(td, (struct mkdir_args *)uap);
}

int
ibcs2_symlink(td, uap)
	struct thread *td;
	struct ibcs2_symlink_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	CHECKALTCREAT(td, &sg, uap->link);
	return symlink(td, (struct symlink_args *)uap);
}

int
ibcs2_rename(td, uap)
	struct thread *td;
	struct ibcs2_rename_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->from);
	CHECKALTCREAT(td, &sg, uap->to);
	return rename(td, (struct rename_args *)uap);
}

int
ibcs2_readlink(td, uap)
	struct thread *td;
	struct ibcs2_readlink_args *uap;
{
        caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, uap->path);
	return readlink(td, (struct readlink_args *) uap);
}
