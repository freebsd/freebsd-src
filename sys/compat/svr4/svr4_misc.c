/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * SVR4 compatibility module.
 *
 * SVR4 system calls that are implemented differently in BSD are
 * handled here.
 */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_sysconfig.h>
#include <compat/svr4/svr4_dirent.h>
#include <compat/svr4/svr4_acl.h>
#include <compat/svr4/svr4_ulimit.h>
#include <compat/svr4/svr4_statvfs.h>
#include <compat/svr4/svr4_hrt.h>
#include <compat/svr4/svr4_mman.h>
#include <compat/svr4/svr4_wait.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#if defined(__FreeBSD__)
#include <vm/uma.h>
#endif

#if defined(NetBSD)
# if defined(UVM)
#  include <uvm/uvm_extern.h>
# endif
#endif

#define	BSD_DIRENT(cp)		((struct dirent *)(cp))

static int svr4_mknod(struct thread *, register_t *, char *,
    svr4_mode_t, svr4_dev_t);

static __inline clock_t timeval_to_clock_t(struct timeval *);
static int svr4_setinfo	(struct proc *, int, svr4_siginfo_t *);

struct svr4_hrtcntl_args;
static int svr4_hrtcntl	(struct thread *, struct svr4_hrtcntl_args *,
    register_t *);
static void bsd_statfs_to_svr4_statvfs(const struct statfs *,
    struct svr4_statvfs *);
static void bsd_statfs_to_svr4_statvfs64(const struct statfs *,
    struct svr4_statvfs64 *);
static struct proc *svr4_pfind(pid_t pid);

/* BOGUS noop */
#if defined(BOGUS)
int
svr4_sys_setitimer(td, uap)
        register struct thread *td;
	struct svr4_sys_setitimer_args *uap;
{
        td->td_retval[0] = 0;
	return 0;
}
#endif

int
svr4_sys_wait(td, uap)
	struct thread *td;
	struct svr4_sys_wait_args *uap;
{
	struct wait_args w4;
	int error, *retval = td->td_retval, st, sig;
	size_t sz = sizeof(*SCARG(&w4, status));

	SCARG(&w4, rusage) = NULL;
	SCARG(&w4, options) = 0;

	if (SCARG(uap, status) == NULL) {
		caddr_t sg = stackgap_init();

		SCARG(&w4, status) = stackgap_alloc(&sg, sz);
	}
	else
		SCARG(&w4, status) = SCARG(uap, status);

	SCARG(&w4, pid) = WAIT_ANY;

	if ((error = wait4(td, &w4)) != 0)
		return error;
      
	if ((error = copyin(SCARG(&w4, status), &st, sizeof(st))) != 0)
		return error;

	if (WIFSIGNALED(st)) {
		sig = WTERMSIG(st);
		if (sig >= 0 && sig < NSIG)
			st = (st & ~0177) | SVR4_BSD2SVR4_SIG(sig);
	} else if (WIFSTOPPED(st)) {
		sig = WSTOPSIG(st);
		if (sig >= 0 && sig < NSIG)
			st = (st & ~0xff00) | (SVR4_BSD2SVR4_SIG(sig) << 8);
	}

	/*
	 * It looks like wait(2) on svr4/solaris/2.4 returns
	 * the status in retval[1], and the pid on retval[0].
	 */
	retval[1] = st;

	if (SCARG(uap, status))
		if ((error = copyout(&st, SCARG(uap, status), sizeof(st))) != 0)
			return error;

	return 0;
}

int
svr4_sys_execv(td, uap)
	struct thread *td;
	struct svr4_sys_execv_args *uap;
{
	struct execve_args ap;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, SCARG(uap, path));

	SCARG(&ap, fname) = SCARG(uap, path);
	SCARG(&ap, argv) = SCARG(uap, argp);
	SCARG(&ap, envv) = NULL;

	return execve(td, &ap);
}

int
svr4_sys_execve(td, uap)
	struct thread *td;
	struct svr4_sys_execve_args *uap;
{
	struct execve_args ap;
	caddr_t sg;

	sg = stackgap_init();
	CHECKALTEXIST(td, &sg, uap->path);

	SCARG(&ap, fname) = SCARG(uap, path);
	SCARG(&ap, argv) = SCARG(uap, argp);
	SCARG(&ap, envv) = SCARG(uap, envp);

	return execve(td, &ap);
}

int
svr4_sys_time(td, v)
	struct thread *td;
	struct svr4_sys_time_args *v;
{
	struct svr4_sys_time_args *uap = v;
	int error = 0;
	struct timeval tv;

	microtime(&tv);
	if (SCARG(uap, t))
		error = copyout(&tv.tv_sec, SCARG(uap, t),
				sizeof(*(SCARG(uap, t))));
	td->td_retval[0] = (int) tv.tv_sec;

	return error;
}


/*
 * Read SVR4-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  
 *
 * This code is ported from the Linux emulator:  Changes to the VFS interface
 * between FreeBSD and NetBSD have made it simpler to port it from there than
 * to adapt the NetBSD version.
 */
int
svr4_sys_getdents64(td, uap)
	struct thread *td;
	struct svr4_sys_getdents64_args *uap;
{
	register struct dirent *bdp;
	struct vnode *vp;
	caddr_t inp, buf;		/* BSD-format */
	int len, reclen;		/* BSD-format */
	caddr_t outp;			/* SVR4-format */
	int resid, svr4reclen=0;	/* SVR4-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct vattr va;
	off_t off;
	struct svr4_dirent64 svr4_dirent;
	int buflen, error, eofflag, nbytes, justone;
	u_long *cookies = NULL, *cookiep;
	int ncookies;

	DPRINTF(("svr4_sys_getdents64(%d, *, %d)\n",
		SCARG(uap, fd), SCARG(uap, nbytes)));
	if ((error = getvnode(td->td_proc->p_fd, SCARG(uap, fd), &fp)) != 0) {
		return (error);
	}

	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	vp = (struct vnode *) fp->f_data;

	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (EINVAL);
	}

	if ((error = VOP_GETATTR(vp, &va, td->td_ucred, td))) {
		fdrop(fp, td);
		return error;
	}

	nbytes = SCARG(uap, nbytes);
	if (nbytes == 1) {
		nbytes = sizeof (struct svr4_dirent64);
		justone = 1;
	}
	else
		justone = 0;

	off = fp->f_offset;
#define	DIRBLKSIZ	512		/* XXX we used to use ufs's DIRBLKSIZ */
	buflen = max(DIRBLKSIZ, nbytes);
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

	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag,
						&ncookies, &cookies);
	if (error) {
		goto out;
	}

	inp = buf;
	outp = (caddr_t) SCARG(uap, dp);
	resid = nbytes;
	if ((len = buflen - auio.uio_resid) <= 0) {
		goto eof;
	}

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
			DPRINTF(("svr4_readdir: reclen=%d\n", reclen));
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
		svr4reclen = SVR4_RECLEN(&svr4_dirent, bdp->d_namlen);
		if (reclen > len || resid < svr4reclen) {
			outp++;
			break;
		}
		svr4_dirent.d_ino = (long) bdp->d_fileno;
		if (justone) {
			/*
			 * old svr4-style readdir usage.
			 */
			svr4_dirent.d_off = (svr4_off_t) svr4reclen;
			svr4_dirent.d_reclen = (u_short) bdp->d_namlen;
		} else {
			svr4_dirent.d_off = (svr4_off_t)(off + reclen);
			svr4_dirent.d_reclen = (u_short) svr4reclen;
		}
		strcpy(svr4_dirent.d_name, bdp->d_name);
		if ((error = copyout((caddr_t)&svr4_dirent, outp, svr4reclen)))
			goto out;
		inp += reclen;
		if (cookiep) {
			off = *cookiep++;
			ncookies--;
		} else
			off += reclen;
		outp += svr4reclen;
		resid -= svr4reclen;
		len -= reclen;
		if (justone)
			break;
    	}

	if (outp == (caddr_t) SCARG(uap, dp))
		goto again;
	fp->f_offset = off;

	if (justone)
		nbytes = resid + svr4reclen;

eof:
	td->td_retval[0] = nbytes - resid;
out:
	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	if (cookies)
		free(cookies, M_TEMP);
	free(buf, M_TEMP);
	return error;
}


int
svr4_sys_getdents(td, uap)
	struct thread *td;
	struct svr4_sys_getdents_args *uap;
{
	struct dirent *bdp;
	struct vnode *vp;
	caddr_t inp, buf;	/* BSD-format */
	int len, reclen;	/* BSD-format */
	caddr_t outp;		/* SVR4-format */
	int resid, svr4_reclen;	/* SVR4-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct svr4_dirent idb;
	off_t off;		/* true file offset */
	int buflen, error, eofflag;
	u_long *cookiebuf = NULL, *cookie;
	int ncookies = 0, *retval = td->td_retval;

	if ((error = getvnode(td->td_proc->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR) {
		fdrop(fp, td);
		return (EINVAL);
	}

	buflen = min(MAXBSIZE, SCARG(uap, nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	off = fp->f_offset;
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

#ifdef MAC
	error = mac_check_vnode_readdir(td->td_ucred, vp);
	if (error)
		goto out;
#endif

	/*
         * First we read into the malloc'ed buffer, then
         * we massage it into user space, one record at a time.
         */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, &ncookies,
	    &cookiebuf);
	if (error) {
		goto out;
	}

	inp = buf;
	outp = SCARG(uap, buf);
	resid = SCARG(uap, nbytes);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("svr4_sys_getdents64: bad reclen");
		off = *cookie++;	/* each entry points to the next */
		if ((off >> 32) != 0) {
			uprintf("svr4_sys_getdents64: dir offset too large for emulated program");
			error = EINVAL;
			goto out;
		}
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			continue;
		}
		svr4_reclen = SVR4_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < svr4_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a SVR4-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = (svr4_ino_t)bdp->d_fileno;
		idb.d_off = (svr4_off_t)off;
		idb.d_reclen = (u_short)svr4_reclen;
		strcpy(idb.d_name, bdp->d_name);
		if ((error = copyout((caddr_t)&idb, outp, svr4_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past SVR4-shaped entry */
		outp += svr4_reclen;
		resid -= svr4_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf))
		goto again;
	fp->f_offset = off;	/* update the vnode offset */

eof:
	*retval = SCARG(uap, nbytes) - resid;
out:
	VOP_UNLOCK(vp, 0, td);
	fdrop(fp, td);
	if (cookiebuf)
		free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
	return error;
}


int
svr4_sys_mmap(td, uap)
	struct thread *td;
	struct svr4_sys_mmap_args *uap;
{
	struct mmap_args	 mm;
	int             *retval;

	retval = td->td_retval;
#define _MAP_NEW	0x80000000
	/*
         * Verify the arguments.
         */
	if (SCARG(uap, prot) & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return EINVAL;	/* XXX still needed? */

	if (SCARG(uap, len) == 0)
		return EINVAL;

	SCARG(&mm, prot) = SCARG(uap, prot);
	SCARG(&mm, len) = SCARG(uap, len);
	SCARG(&mm, flags) = SCARG(uap, flags) & ~_MAP_NEW;
	SCARG(&mm, fd) = SCARG(uap, fd);
	SCARG(&mm, addr) = SCARG(uap, addr);
	SCARG(&mm, pos) = SCARG(uap, pos);

	return mmap(td, &mm);
}

int
svr4_sys_mmap64(td, uap)
	struct thread *td;
	struct svr4_sys_mmap64_args *uap;
{
	struct mmap_args	 mm;
	void		*rp;

#define _MAP_NEW	0x80000000
	/*
         * Verify the arguments.
         */
	if (SCARG(uap, prot) & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return EINVAL;	/* XXX still needed? */

	if (SCARG(uap, len) == 0)
		return EINVAL;

	SCARG(&mm, prot) = SCARG(uap, prot);
	SCARG(&mm, len) = SCARG(uap, len);
	SCARG(&mm, flags) = SCARG(uap, flags) & ~_MAP_NEW;
	SCARG(&mm, fd) = SCARG(uap, fd);
	SCARG(&mm, addr) = SCARG(uap, addr);
	SCARG(&mm, pos) = SCARG(uap, pos);

	rp = (void *) round_page((vm_offset_t)(td->td_proc->p_vmspace->vm_daddr + maxdsiz));
	if ((SCARG(&mm, flags) & MAP_FIXED) == 0 &&
	    SCARG(&mm, addr) != 0 && (void *)SCARG(&mm, addr) < rp)
		SCARG(&mm, addr) = rp;

	return mmap(td, &mm);
}


int
svr4_sys_fchroot(td, uap)
	struct thread *td;
	struct svr4_sys_fchroot_args *uap;
{
	struct filedesc	*fdp = td->td_proc->p_fd;
	struct vnode	*vp, *vpold;
	struct file	*fp;
	int		 error;

	if ((error = suser(td)) != 0)
		return error;
	if ((error = getvnode(fdp, SCARG(uap, fd), &fp)) != 0)
		return error;
	vp = (struct vnode *) fp->f_data;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	VOP_UNLOCK(vp, 0, td);
	if (error) {
		fdrop(fp, td);
		return error;
	}
	VREF(vp);
	FILEDESC_LOCK(fdp);
	vpold = fdp->fd_rdir;
	fdp->fd_rdir = vp;
	FILEDESC_UNLOCK(fdp);
	if (vpold != NULL)
		vrele(vpold);
	fdrop(fp, td);
	return 0;
}


static int
svr4_mknod(td, retval, path, mode, dev)
	struct thread *td;
	register_t *retval;
	char *path;
	svr4_mode_t mode;
	svr4_dev_t dev;
{
	caddr_t sg = stackgap_init();

	CHECKALTEXIST(td, &sg, path);

	if (S_ISFIFO(mode)) {
		struct mkfifo_args ap;
		SCARG(&ap, path) = path;
		SCARG(&ap, mode) = mode;
		return mkfifo(td, &ap);
	} else {
		struct mknod_args ap;
		SCARG(&ap, path) = path;
		SCARG(&ap, mode) = mode;
		SCARG(&ap, dev) = dev;
		return mknod(td, &ap);
	}
}


int
svr4_sys_mknod(td, uap)
	register struct thread *td;
	struct svr4_sys_mknod_args *uap;
{
        int *retval = td->td_retval;
	return svr4_mknod(td, retval,
			  SCARG(uap, path), SCARG(uap, mode),
			  (svr4_dev_t)svr4_to_bsd_odev_t(SCARG(uap, dev)));
}


int
svr4_sys_xmknod(td, uap)
	struct thread *td;
	struct svr4_sys_xmknod_args *uap;
{
        int *retval = td->td_retval;
	return svr4_mknod(td, retval,
			  SCARG(uap, path), SCARG(uap, mode),
			  (svr4_dev_t)svr4_to_bsd_dev_t(SCARG(uap, dev)));
}


int
svr4_sys_vhangup(td, uap)
	struct thread *td;
	struct svr4_sys_vhangup_args *uap;
{
	return 0;
}


int
svr4_sys_sysconfig(td, uap)
	struct thread *td;
	struct svr4_sys_sysconfig_args *uap;
{
	int *retval;

	retval = &(td->td_retval[0]);

	switch (SCARG(uap, name)) {
	case SVR4_CONFIG_UNUSED:
		*retval = 0;
		break;
	case SVR4_CONFIG_NGROUPS:
		*retval = NGROUPS_MAX;
		break;
	case SVR4_CONFIG_CHILD_MAX:
		*retval = maxproc;
		break;
	case SVR4_CONFIG_OPEN_FILES:
		*retval = maxfiles;
		break;
	case SVR4_CONFIG_POSIX_VER:
		*retval = 198808;
		break;
	case SVR4_CONFIG_PAGESIZE:
		*retval = PAGE_SIZE;
		break;
	case SVR4_CONFIG_CLK_TCK:
		*retval = 60;	/* should this be `hz', ie. 100? */
		break;
	case SVR4_CONFIG_XOPEN_VER:
		*retval = 2;	/* XXX: What should that be? */
		break;
	case SVR4_CONFIG_PROF_TCK:
		*retval = 60;	/* XXX: What should that be? */
		break;
	case SVR4_CONFIG_NPROC_CONF:
		*retval = 1;	/* Only one processor for now */
		break;
	case SVR4_CONFIG_NPROC_ONLN:
		*retval = 1;	/* And it better be online */
		break;
	case SVR4_CONFIG_AIO_LISTIO_MAX:
	case SVR4_CONFIG_AIO_MAX:
	case SVR4_CONFIG_AIO_PRIO_DELTA_MAX:
		*retval = 0;	/* No aio support */
		break;
	case SVR4_CONFIG_DELAYTIMER_MAX:
		*retval = 0;	/* No delaytimer support */
		break;
	case SVR4_CONFIG_MQ_OPEN_MAX:
		*retval = msginfo.msgmni;
		break;
	case SVR4_CONFIG_MQ_PRIO_MAX:
		*retval = 0;	/* XXX: Don't know */
		break;
	case SVR4_CONFIG_RTSIG_MAX:
		*retval = 0;
		break;
	case SVR4_CONFIG_SEM_NSEMS_MAX:
		*retval = seminfo.semmni;
		break;
	case SVR4_CONFIG_SEM_VALUE_MAX:
		*retval = seminfo.semvmx;
		break;
	case SVR4_CONFIG_SIGQUEUE_MAX:
		*retval = 0;	/* XXX: Don't know */
		break;
	case SVR4_CONFIG_SIGRT_MIN:
	case SVR4_CONFIG_SIGRT_MAX:
		*retval = 0;	/* No real time signals */
		break;
	case SVR4_CONFIG_TIMER_MAX:
		*retval = 3;	/* XXX: real, virtual, profiling */
		break;
#if defined(NOTYET)
	case SVR4_CONFIG_PHYS_PAGES:
#if defined(UVM)
		*retval = uvmexp.free;	/* XXX: free instead of total */
#else
		*retval = cnt.v_free_count;	/* XXX: free instead of total */
#endif
		break;
	case SVR4_CONFIG_AVPHYS_PAGES:
#if defined(UVM)
		*retval = uvmexp.active;	/* XXX: active instead of avg */
#else
		*retval = cnt.v_active_count;	/* XXX: active instead of avg */
#endif
		break;
#endif /* NOTYET */

	default:
		return EINVAL;
	}
	return 0;
}

extern int swap_pager_full;

/* ARGSUSED */
int
svr4_sys_break(td, uap)
	struct thread *td;
	struct svr4_sys_break_args *uap;
{
	struct proc *p = td->td_proc;
	struct vmspace *vm = p->p_vmspace;
	vm_offset_t new, old, base, ns;
	int rv;

	base = round_page((vm_offset_t) vm->vm_daddr);
	ns = (vm_offset_t)SCARG(uap, nsize);
	new = round_page(ns);
	/* For p_rlimit. */
	mtx_assert(&Giant, MA_OWNED);
	if (new > base) {
	  if ((new - base) > (unsigned) td->td_proc->p_rlimit[RLIMIT_DATA].rlim_cur) {
			return ENOMEM;
	  }
	  if (new >= VM_MAXUSER_ADDRESS) {
	    return (ENOMEM);
	  }
	} else if (new < base) {
		/*
		 * This is simply an invalid value.  If someone wants to
		 * do fancy address space manipulations, mmap and munmap
		 * can do most of what the user would want.
		 */
		return EINVAL;
	}

	old = base + ctob(vm->vm_dsize);

	if (new > old) {
		vm_size_t diff;
		diff = new - old;
		if (vm->vm_map.size + diff > p->p_rlimit[RLIMIT_VMEM].rlim_cur)
			return(ENOMEM);
		rv = vm_map_find(&vm->vm_map, NULL, 0, &old, diff, FALSE,
			VM_PROT_ALL, VM_PROT_ALL, 0);
		if (rv != KERN_SUCCESS) {
			return (ENOMEM);
		}
		vm->vm_dsize += btoc(diff);
	} else if (new < old) {
		rv = vm_map_remove(&vm->vm_map, new, old);
		if (rv != KERN_SUCCESS) {
			return (ENOMEM);
		}
		vm->vm_dsize -= btoc(old - new);
	}

	return (0);
}

static __inline clock_t
timeval_to_clock_t(tv)
	struct timeval *tv;
{
	return tv->tv_sec * hz + tv->tv_usec / (1000000 / hz);
}


int
svr4_sys_times(td, uap)
	struct thread *td;
	struct svr4_sys_times_args *uap;
{
	int			 error, *retval = td->td_retval;
	struct tms		 tms;
	struct timeval		 t;
	struct rusage		*ru;
	struct rusage		 r;
	struct getrusage_args 	 ga;

	caddr_t sg = stackgap_init();
	ru = stackgap_alloc(&sg, sizeof(struct rusage));

	SCARG(&ga, who) = RUSAGE_SELF;
	SCARG(&ga, rusage) = ru;

	error = getrusage(td, &ga);
	if (error)
		return error;

	if ((error = copyin(ru, &r, sizeof r)) != 0)
		return error;

	tms.tms_utime = timeval_to_clock_t(&r.ru_utime);
	tms.tms_stime = timeval_to_clock_t(&r.ru_stime);

	SCARG(&ga, who) = RUSAGE_CHILDREN;
	error = getrusage(td, &ga);
	if (error)
		return error;

	if ((error = copyin(ru, &r, sizeof r)) != 0)
		return error;

	tms.tms_cutime = timeval_to_clock_t(&r.ru_utime);
	tms.tms_cstime = timeval_to_clock_t(&r.ru_stime);

	microtime(&t);
	*retval = timeval_to_clock_t(&t);

	return copyout(&tms, SCARG(uap, tp), sizeof(tms));
}


int
svr4_sys_ulimit(td, uap)
	struct thread *td;
	struct svr4_sys_ulimit_args *uap;
{
        int *retval = td->td_retval;

	switch (SCARG(uap, cmd)) {
	case SVR4_GFILLIM:
		/* For p_rlimit below. */
		mtx_assert(&Giant, MA_OWNED);
		*retval = td->td_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur / 512;
		if (*retval == -1)
			*retval = 0x7fffffff;
		return 0;

	case SVR4_SFILLIM:
		{
			int error;
			struct __setrlimit_args srl;
			struct rlimit krl;
			caddr_t sg = stackgap_init();
			struct rlimit *url = (struct rlimit *) 
				stackgap_alloc(&sg, sizeof *url);

			krl.rlim_cur = SCARG(uap, newlimit) * 512;
			mtx_assert(&Giant, MA_OWNED);
			krl.rlim_max = td->td_proc->p_rlimit[RLIMIT_FSIZE].rlim_max;

			error = copyout(&krl, url, sizeof(*url));
			if (error)
				return error;

			SCARG(&srl, which) = RLIMIT_FSIZE;
			SCARG(&srl, rlp) = url;

			error = setrlimit(td, &srl);
			if (error)
				return error;

			mtx_assert(&Giant, MA_OWNED);
			*retval = td->td_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur;
			if (*retval == -1)
				*retval = 0x7fffffff;
			return 0;
		}

	case SVR4_GMEMLIM:
		{
			struct vmspace *vm = td->td_proc->p_vmspace;
			register_t r;

			mtx_assert(&Giant, MA_OWNED);
			r = td->td_proc->p_rlimit[RLIMIT_DATA].rlim_cur;

			if (r == -1)
				r = 0x7fffffff;
			r += (long) vm->vm_daddr;
			if (r < 0)
				r = 0x7fffffff;
			*retval = r;
			return 0;
		}

	case SVR4_GDESLIM:
		mtx_assert(&Giant, MA_OWNED);
		*retval = td->td_proc->p_rlimit[RLIMIT_NOFILE].rlim_cur;
		if (*retval == -1)
			*retval = 0x7fffffff;
		return 0;

	default:
		return EINVAL;
	}
}

static struct proc *
svr4_pfind(pid)
	pid_t pid;
{
	struct proc *p;

	/* look in the live processes */
	if ((p = pfind(pid)) == NULL)
		/* look in the zombies */
		p = zpfind(pid);

	return p;
}


int
svr4_sys_pgrpsys(td, uap)
	struct thread *td;
	struct svr4_sys_pgrpsys_args *uap;
{
        int *retval = td->td_retval;
	struct proc *p = td->td_proc;

	switch (SCARG(uap, cmd)) {
	case 1:			/* setpgrp() */
		/*
		 * SVR4 setpgrp() (which takes no arguments) has the
		 * semantics that the session ID is also created anew, so
		 * in almost every sense, setpgrp() is identical to
		 * setsid() for SVR4.  (Under BSD, the difference is that
		 * a setpgid(0,0) will not create a new session.)
		 */
		setsid(td, NULL);
		/*FALLTHROUGH*/

	case 0:			/* getpgrp() */
		PROC_LOCK(p);
		*retval = p->p_pgrp->pg_id;
		PROC_UNLOCK(p);
		return 0;

	case 2:			/* getsid(pid) */
		if (SCARG(uap, pid) == 0)
			PROC_LOCK(p);
		else if ((p = svr4_pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;
		/*
		 * This has already been initialized to the pid of
		 * the session leader.
		 */
		*retval = (register_t) p->p_session->s_sid;
		PROC_UNLOCK(p);
		return 0;

	case 3:			/* setsid() */
		return setsid(td, NULL);

	case 4:			/* getpgid(pid) */

		if (SCARG(uap, pid) == 0)
			PROC_LOCK(p);
		else if ((p = svr4_pfind(SCARG(uap, pid))) == NULL)
			return ESRCH;

		*retval = (int) p->p_pgrp->pg_id;
		PROC_UNLOCK(p);
		return 0;

	case 5:			/* setpgid(pid, pgid); */
		{
			struct setpgid_args sa;

			SCARG(&sa, pid) = SCARG(uap, pid);
			SCARG(&sa, pgid) = SCARG(uap, pgid);
			return setpgid(td, &sa);
		}

	default:
		return EINVAL;
	}
}

#define syscallarg(x)   union { x datum; register_t pad; }

struct svr4_hrtcntl_args {
	int 			cmd;
	int 			fun;
	int 			clk;
	svr4_hrt_interval_t *	iv;
	svr4_hrt_time_t *	ti;
};


static int
svr4_hrtcntl(td, uap, retval)
	struct thread *td;
	struct svr4_hrtcntl_args *uap;
	register_t *retval;
{
	switch (SCARG(uap, fun)) {
	case SVR4_HRT_CNTL_RES:
		DPRINTF(("htrcntl(RES)\n"));
		*retval = SVR4_HRT_USEC;
		return 0;

	case SVR4_HRT_CNTL_TOFD:
		DPRINTF(("htrcntl(TOFD)\n"));
		{
			struct timeval tv;
			svr4_hrt_time_t t;
			if (SCARG(uap, clk) != SVR4_HRT_CLK_STD) {
				DPRINTF(("clk == %d\n", SCARG(uap, clk)));
				return EINVAL;
			}
			if (SCARG(uap, ti) == NULL) {
				DPRINTF(("ti NULL\n"));
				return EINVAL;
			}
			microtime(&tv);
			t.h_sec = tv.tv_sec;
			t.h_rem = tv.tv_usec;
			t.h_res = SVR4_HRT_USEC;
			return copyout(&t, SCARG(uap, ti), sizeof(t));
		}

	case SVR4_HRT_CNTL_START:
		DPRINTF(("htrcntl(START)\n"));
		return ENOSYS;

	case SVR4_HRT_CNTL_GET:
		DPRINTF(("htrcntl(GET)\n"));
		return ENOSYS;
	default:
		DPRINTF(("Bad htrcntl command %d\n", SCARG(uap, fun)));
		return ENOSYS;
	}
}


int
svr4_sys_hrtsys(td, uap) 
	struct thread *td;
	struct svr4_sys_hrtsys_args *uap;
{
        int *retval = td->td_retval;

	switch (SCARG(uap, cmd)) {
	case SVR4_HRT_CNTL:
		return svr4_hrtcntl(td, (struct svr4_hrtcntl_args *) uap,
				    retval);

	case SVR4_HRT_ALRM:
		DPRINTF(("hrtalarm\n"));
		return ENOSYS;

	case SVR4_HRT_SLP:
		DPRINTF(("hrtsleep\n"));
		return ENOSYS;

	case SVR4_HRT_CAN:
		DPRINTF(("hrtcancel\n"));
		return ENOSYS;

	default:
		DPRINTF(("Bad hrtsys command %d\n", SCARG(uap, cmd)));
		return EINVAL;
	}
}


static int
svr4_setinfo(p, st, s)
	struct proc *p;
	int st;
	svr4_siginfo_t *s;
{
	svr4_siginfo_t i;
	int sig;

	memset(&i, 0, sizeof(i));

	i.si_signo = SVR4_SIGCHLD;
	i.si_errno = 0;	/* XXX? */

	if (p) {
		i.si_pid = p->p_pid;
		mtx_lock_spin(&sched_lock);
		if (p->p_state == PRS_ZOMBIE) {
			i.si_stime = p->p_ru->ru_stime.tv_sec;
			i.si_utime = p->p_ru->ru_utime.tv_sec;
		}
		else {
			i.si_stime = p->p_stats->p_ru.ru_stime.tv_sec;
			i.si_utime = p->p_stats->p_ru.ru_utime.tv_sec;
		}
		mtx_unlock_spin(&sched_lock);
	}

	if (WIFEXITED(st)) {
		i.si_status = WEXITSTATUS(st);
		i.si_code = SVR4_CLD_EXITED;
	} else if (WIFSTOPPED(st)) {
		sig = WSTOPSIG(st);
		if (sig >= 0 && sig < NSIG)
			i.si_status = SVR4_BSD2SVR4_SIG(sig);

		if (i.si_status == SVR4_SIGCONT)
			i.si_code = SVR4_CLD_CONTINUED;
		else
			i.si_code = SVR4_CLD_STOPPED;
	} else {
		sig = WTERMSIG(st);
		if (sig >= 0 && sig < NSIG)
			i.si_status = SVR4_BSD2SVR4_SIG(sig);

		if (WCOREDUMP(st))
			i.si_code = SVR4_CLD_DUMPED;
		else
			i.si_code = SVR4_CLD_KILLED;
	}

	DPRINTF(("siginfo [pid %ld signo %d code %d errno %d status %d]\n",
		 i.si_pid, i.si_signo, i.si_code, i.si_errno, i.si_status));

	return copyout(&i, s, sizeof(i));
}


int
svr4_sys_waitsys(td, uap)
	struct thread *td;
	struct svr4_sys_waitsys_args *uap;
{
	int nfound;
	int error, *retval = td->td_retval;
	struct proc *q, *t;


	switch (SCARG(uap, grp)) {
	case SVR4_P_PID:	
		break;

	case SVR4_P_PGID:
		PROC_LOCK(td->td_proc);
		SCARG(uap, id) = -td->td_proc->p_pgid;
		PROC_UNLOCK(td->td_proc);
		break;

	case SVR4_P_ALL:
		SCARG(uap, id) = WAIT_ANY;
		break;

	default:
		return EINVAL;
	}

	DPRINTF(("waitsys(%d, %d, %p, %x)\n", 
	         SCARG(uap, grp), SCARG(uap, id),
		 SCARG(uap, info), SCARG(uap, options)));

loop:
	nfound = 0;
	sx_slock(&proctree_lock);
	LIST_FOREACH(q, &td->td_proc->p_children, p_sibling) {
		PROC_LOCK(q);
		if (SCARG(uap, id) != WAIT_ANY &&
		    q->p_pid != SCARG(uap, id) &&
		    q->p_pgid != -SCARG(uap, id)) {
			PROC_UNLOCK(q);
			DPRINTF(("pid %d pgid %d != %d\n", q->p_pid,
				 q->p_pgid, SCARG(uap, id)));
			continue;
		}
		nfound++;
		mtx_lock_spin(&sched_lock);
		if ((q->p_state == PRS_ZOMBIE) && 
		    ((SCARG(uap, options) & (SVR4_WEXITED|SVR4_WTRAPPED)))) {
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(q);
			sx_sunlock(&proctree_lock);
			*retval = 0;
			DPRINTF(("found %d\n", q->p_pid));
			error = svr4_setinfo(q, q->p_xstat, SCARG(uap, info));
			if (error != 0)
				return error;


		        if ((SCARG(uap, options) & SVR4_WNOWAIT)) {
				DPRINTF(("Don't wait\n"));
				return 0;
			}

			/*
			 * If we got the child via ptrace(2) or procfs, and
			 * the parent is different (meaning the process was
			 * attached, rather than run as a child), then we need
			 * to give it back to the old parent, and send the
			 * parent a SIGCHLD.  The rest of the cleanup will be
			 * done when the old parent waits on the child.
			 */
			sx_xlock(&proctree_lock);
			PROC_LOCK(q);
			if (q->p_flag & P_TRACED) {
				if (q->p_oppid != q->p_pptr->p_pid) {
					PROC_UNLOCK(q);
					t = pfind(q->p_oppid);
					if (t == NULL) {
						t = initproc;
						PROC_LOCK(initproc);
					}
					PROC_LOCK(q);
					proc_reparent(q, t);
 					q->p_oppid = 0;
					q->p_flag &= ~(P_TRACED | P_WAITED);
					PROC_UNLOCK(q);
					psignal(t, SIGCHLD);
					wakeup(t);
					PROC_UNLOCK(t);
					sx_xunlock(&proctree_lock);
					return 0;
				}
			}
			PROC_UNLOCK(q);
			sx_xunlock(&proctree_lock);
			q->p_xstat = 0;
			ruadd(&td->td_proc->p_stats->p_cru, q->p_ru);
			FREE(q->p_ru, M_ZOMBIE);
			q->p_ru = 0;

			/*
			 * Decrement the count of procs running with this uid.
			 */
			(void)chgproccnt(q->p_ucred->cr_ruidinfo, -1, 0);

			/*
			 * Release reference to text vnode.
			 */
			if (q->p_textvp)
				vrele(q->p_textvp);

			/*
			 * Free up credentials.
			 */
			crfree(q->p_ucred);
			q->p_ucred = NULL;

			/*
			 * Remove unused arguments
			 */
			pargs_drop(q->p_args);
			PROC_UNLOCK(q);

			/*
			 * Finally finished with old proc entry.
			 * Unlink it from its process group and free it.
			 */
			sx_xlock(&proctree_lock);
			leavepgrp(q);

			sx_xlock(&allproc_lock);
			LIST_REMOVE(q, p_list); /* off zombproc */
			sx_xunlock(&allproc_lock);

			LIST_REMOVE(q, p_sibling);
			sx_xunlock(&proctree_lock);

			PROC_LOCK(q);
			if (--q->p_procsig->ps_refcnt == 0) {
				if (q->p_sigacts != &q->p_uarea->u_sigacts)
					FREE(q->p_sigacts, M_SUBPROC);
				FREE(q->p_procsig, M_SUBPROC);
				q->p_procsig = NULL;
			}
			PROC_UNLOCK(q);

			/*
			 * Give machine-dependent layer a chance
			 * to free anything that cpu_exit couldn't
			 * release while still running in process context.
			 */
			cpu_wait(q);
#if defined(__NetBSD__)
			pool_put(&proc_pool, q);
#endif
#ifdef __FreeBSD__
			mtx_destroy(&q->p_mtx);
			uma_zfree(proc_zone, q);
#endif
			nprocs--;
			return 0;
		}
		/* XXXKSE this needs clarification */
		if (P_SHOULDSTOP(q) && ((q->p_flag & P_WAITED) == 0) &&
		    (q->p_flag & P_TRACED ||
		     (SCARG(uap, options) & (SVR4_WSTOPPED|SVR4_WCONTINUED)))) {
			mtx_unlock_spin(&sched_lock);
			DPRINTF(("jobcontrol %d\n", q->p_pid));
		        if (((SCARG(uap, options) & SVR4_WNOWAIT)) == 0)
				q->p_flag |= P_WAITED;
			PROC_UNLOCK(q);
			*retval = 0;
			return svr4_setinfo(q, W_STOPCODE(q->p_xstat),
					    SCARG(uap, info));
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(q);
	}

	if (nfound == 0)
		return ECHILD;

	if (SCARG(uap, options) & SVR4_WNOHANG) {
		*retval = 0;
		if ((error = svr4_setinfo(NULL, 0, SCARG(uap, info))) != 0)
			return error;
		return 0;
	}

	if ((error = tsleep((caddr_t)td->td_proc, PWAIT | PCATCH, "svr4_wait", 0)) != 0)
		return error;
	goto loop;
}


static void
bsd_statfs_to_svr4_statvfs(bfs, sfs)
	const struct statfs *bfs;
	struct svr4_statvfs *sfs;
{
	sfs->f_bsize = bfs->f_iosize; /* XXX */
	sfs->f_frsize = bfs->f_bsize;
	sfs->f_blocks = bfs->f_blocks;
	sfs->f_bfree = bfs->f_bfree;
	sfs->f_bavail = bfs->f_bavail;
	sfs->f_files = bfs->f_files;
	sfs->f_ffree = bfs->f_ffree;
	sfs->f_favail = bfs->f_ffree;
	sfs->f_fsid = bfs->f_fsid.val[0];
	memcpy(sfs->f_basetype, bfs->f_fstypename, sizeof(sfs->f_basetype));
	sfs->f_flag = 0;
	if (bfs->f_flags & MNT_RDONLY)
		sfs->f_flag |= SVR4_ST_RDONLY;
	if (bfs->f_flags & MNT_NOSUID)
		sfs->f_flag |= SVR4_ST_NOSUID;
	sfs->f_namemax = MAXNAMLEN;
	memcpy(sfs->f_fstr, bfs->f_fstypename, sizeof(sfs->f_fstr)); /* XXX */
	memset(sfs->f_filler, 0, sizeof(sfs->f_filler));
}


static void
bsd_statfs_to_svr4_statvfs64(bfs, sfs)
	const struct statfs *bfs;
	struct svr4_statvfs64 *sfs;
{
	sfs->f_bsize = bfs->f_iosize; /* XXX */
	sfs->f_frsize = bfs->f_bsize;
	sfs->f_blocks = bfs->f_blocks;
	sfs->f_bfree = bfs->f_bfree;
	sfs->f_bavail = bfs->f_bavail;
	sfs->f_files = bfs->f_files;
	sfs->f_ffree = bfs->f_ffree;
	sfs->f_favail = bfs->f_ffree;
	sfs->f_fsid = bfs->f_fsid.val[0];
	memcpy(sfs->f_basetype, bfs->f_fstypename, sizeof(sfs->f_basetype));
	sfs->f_flag = 0;
	if (bfs->f_flags & MNT_RDONLY)
		sfs->f_flag |= SVR4_ST_RDONLY;
	if (bfs->f_flags & MNT_NOSUID)
		sfs->f_flag |= SVR4_ST_NOSUID;
	sfs->f_namemax = MAXNAMLEN;
	memcpy(sfs->f_fstr, bfs->f_fstypename, sizeof(sfs->f_fstr)); /* XXX */
	memset(sfs->f_filler, 0, sizeof(sfs->f_filler));
}


int
svr4_sys_statvfs(td, uap)
	struct thread *td;
	struct svr4_sys_statvfs_args *uap;
{
	struct statfs_args	fs_args;
	caddr_t sg = stackgap_init();
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs sfs;
	int error;

	CHECKALTEXIST(td, &sg, SCARG(uap, path));
	SCARG(&fs_args, path) = SCARG(uap, path);
	SCARG(&fs_args, buf) = fs;

	if ((error = statfs(td, &fs_args)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_fstatvfs(td, uap)
	struct thread *td;
	struct svr4_sys_fstatvfs_args *uap;
{
	struct fstatfs_args	fs_args;
	caddr_t sg = stackgap_init();
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs sfs;
	int error;

	SCARG(&fs_args, fd) = SCARG(uap, fd);
	SCARG(&fs_args, buf) = fs;

	if ((error = fstatfs(td, &fs_args)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_statvfs64(td, uap)
	struct thread *td;
	struct svr4_sys_statvfs64_args *uap;
{
	struct statfs_args	fs_args;
	caddr_t sg = stackgap_init();
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs64 sfs;
	int error;

	CHECKALTEXIST(td, &sg, SCARG(uap, path));
	SCARG(&fs_args, path) = SCARG(uap, path);
	SCARG(&fs_args, buf) = fs;

	if ((error = statfs(td, &fs_args)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs64(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}


int
svr4_sys_fstatvfs64(td, uap) 
	struct thread *td;
	struct svr4_sys_fstatvfs64_args *uap;
{
	struct fstatfs_args	fs_args;
	caddr_t sg = stackgap_init();
	struct statfs *fs = stackgap_alloc(&sg, sizeof(struct statfs));
	struct statfs bfs;
	struct svr4_statvfs64 sfs;
	int error;

	SCARG(&fs_args, fd) = SCARG(uap, fd);
	SCARG(&fs_args, buf) = fs;

	if ((error = fstatfs(td, &fs_args)) != 0)
		return error;

	if ((error = copyin(fs, &bfs, sizeof(bfs))) != 0)
		return error;

	bsd_statfs_to_svr4_statvfs64(&bfs, &sfs);

	return copyout(&sfs, SCARG(uap, fs), sizeof(sfs));
}

int
svr4_sys_alarm(td, uap)
	struct thread *td;
	struct svr4_sys_alarm_args *uap;
{
	int error;
        struct itimerval *itp, *oitp;
	struct setitimer_args sa;
	caddr_t sg = stackgap_init();

        itp = stackgap_alloc(&sg, sizeof(*itp));
	oitp = stackgap_alloc(&sg, sizeof(*oitp));
        timevalclear(&itp->it_interval);
        itp->it_value.tv_sec = SCARG(uap, sec);
        itp->it_value.tv_usec = 0;

	SCARG(&sa, which) = ITIMER_REAL;
	SCARG(&sa, itv) = itp;
	SCARG(&sa, oitv) = oitp;
        error = setitimer(td, &sa);
	if (error)
		return error;
        if (oitp->it_value.tv_usec)
                oitp->it_value.tv_sec++;
        td->td_retval[0] = oitp->it_value.tv_sec;
        return 0;

}

int
svr4_sys_gettimeofday(td, uap)
	struct thread *td;
	struct svr4_sys_gettimeofday_args *uap;
{
	if (SCARG(uap, tp)) {
		struct timeval atv;

		microtime(&atv);
		return copyout(&atv, SCARG(uap, tp), sizeof (atv));
	}

	return 0;
}

int
svr4_sys_facl(td, uap)
	struct thread *td;
	struct svr4_sys_facl_args *uap;
{
	int *retval;

	retval = td->td_retval;
	*retval = 0;

	switch (SCARG(uap, cmd)) {
	case SVR4_SYS_SETACL:
		/* We don't support acls on any filesystem */
		return ENOSYS;

	case SVR4_SYS_GETACL:
		return copyout(retval, &SCARG(uap, num),
		    sizeof(SCARG(uap, num)));

	case SVR4_SYS_GETACLCNT:
		return 0;

	default:
		return EINVAL;
	}
}


int
svr4_sys_acl(td, uap)
	struct thread *td;
	struct svr4_sys_acl_args *uap;
{
	/* XXX: for now the same */
	return svr4_sys_facl(td, (struct svr4_sys_facl_args *)uap);
}

int
svr4_sys_auditsys(td, uap)
	struct thread *td;
	struct svr4_sys_auditsys_args *uap;
{
	/*
	 * XXX: Big brother is *not* watching.
	 */
	return 0;
}

int
svr4_sys_memcntl(td, uap)
	struct thread *td;
	struct svr4_sys_memcntl_args *uap;
{
	switch (SCARG(uap, cmd)) {
	case SVR4_MC_SYNC:
		{
			struct msync_args msa;

			SCARG(&msa, addr) = SCARG(uap, addr);
			SCARG(&msa, len) = SCARG(uap, len);
			SCARG(&msa, flags) = (int)SCARG(uap, arg);

			return msync(td, &msa);
		}
	case SVR4_MC_ADVISE:
		{
			struct madvise_args maa;

			SCARG(&maa, addr) = SCARG(uap, addr);
			SCARG(&maa, len) = SCARG(uap, len);
			SCARG(&maa, behav) = (int)SCARG(uap, arg);

			return madvise(td, &maa);
		}
	case SVR4_MC_LOCK:
	case SVR4_MC_UNLOCK:
	case SVR4_MC_LOCKAS:
	case SVR4_MC_UNLOCKAS:
		return EOPNOTSUPP;
	default:
		return ENOSYS;
	}
}


int
svr4_sys_nice(td, uap)
	struct thread *td;
	struct svr4_sys_nice_args *uap;
{
	struct setpriority_args ap;
	int error;

	SCARG(&ap, which) = PRIO_PROCESS;
	SCARG(&ap, who) = 0;
	SCARG(&ap, prio) = SCARG(uap, prio);

	if ((error = setpriority(td, &ap)) != 0)
		return error;

	/* the cast is stupid, but the structures are the same */
	if ((error = getpriority(td, (struct getpriority_args *)&ap)) != 0)
		return error;

	return 0;
}

int
svr4_sys_resolvepath(td, uap)
	struct thread *td;
	struct svr4_sys_resolvepath_args *uap;
{
	struct nameidata nd;
	int error, *retval = td->td_retval;

	NDINIT(&nd, LOOKUP, NOFOLLOW | SAVENAME, UIO_USERSPACE,
	    SCARG(uap, path), td);

	if ((error = namei(&nd)) != 0)
		return error;

	if ((error = copyout(nd.ni_cnd.cn_pnbuf, SCARG(uap, buf),
	    SCARG(uap, bufsiz))) != 0)
		goto bad;

	*retval = strlen(nd.ni_cnd.cn_pnbuf) < SCARG(uap, bufsiz) ? 
	  strlen(nd.ni_cnd.cn_pnbuf) + 1 : SCARG(uap, bufsiz);
bad:
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_vp);
	return error;
}
