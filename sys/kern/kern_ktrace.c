/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	from: @(#)kern_ktrace.c	7.15 (Berkeley) 6/21/91
 *	$Id: kern_ktrace.c,v 1.4 1993/10/16 15:24:20 rgrimes Exp $
 */

#ifdef KTRACE

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "file.h"
#include "namei.h"
#include "vnode.h"
#include "ktrace.h"
#include "malloc.h"
#include "syslog.h"

struct ktr_header *
ktrgetheader(type)
{
	register struct ktr_header *kth;
	struct proc *p = curproc;	/* XXX */

	MALLOC(kth, struct ktr_header *, sizeof (struct ktr_header), 
		M_TEMP, M_WAITOK);
	kth->ktr_type = type;
	microtime(&kth->ktr_time);
	kth->ktr_pid = p->p_pid;
	bcopy(p->p_comm, kth->ktr_comm, MAXCOMLEN);
	return (kth);
}

ktrsyscall(vp, code, narg, args)
	struct vnode *vp;
	int code, narg, args[];
{
	struct	ktr_header *kth = ktrgetheader(KTR_SYSCALL);
	struct	ktr_syscall *ktp;
	register len = sizeof(struct ktr_syscall) + (narg * sizeof(int));
	int 	*argp, i;

	MALLOC(ktp, struct ktr_syscall *, len, M_TEMP, M_WAITOK);
	ktp->ktr_code = code;
	ktp->ktr_narg = narg;
	argp = (int *)((char *)ktp + sizeof(struct ktr_syscall));
	for (i = 0; i < narg; i++)
		*argp++ = args[i];
	kth->ktr_buf = (caddr_t)ktp;
	kth->ktr_len = len;
	ktrwrite(vp, kth);
	FREE(ktp, M_TEMP);
	FREE(kth, M_TEMP);
}

ktrsysret(vp, code, error, retval)
	struct vnode *vp;
	int code, error, retval;
{
	struct ktr_header *kth = ktrgetheader(KTR_SYSRET);
	struct ktr_sysret ktp;

	ktp.ktr_code = code;
	ktp.ktr_error = error;
	ktp.ktr_retval = retval;		/* what about val2 ? */

	kth->ktr_buf = (caddr_t)&ktp;
	kth->ktr_len = sizeof(struct ktr_sysret);

	ktrwrite(vp, kth);
	FREE(kth, M_TEMP);
}

ktrnamei(vp, path)
	struct vnode *vp;
	char *path;
{
	struct ktr_header *kth = ktrgetheader(KTR_NAMEI);

	kth->ktr_len = strlen(path);
	kth->ktr_buf = path;

	ktrwrite(vp, kth);
	FREE(kth, M_TEMP);
}

ktrgenio(vp, fd, rw, iov, len, error)
	struct vnode *vp;
	int fd;
	enum uio_rw rw;
	register struct iovec *iov;
{
	struct ktr_header *kth = ktrgetheader(KTR_GENIO);
	register struct ktr_genio *ktp;
	register caddr_t cp;
	register int resid = len, cnt;
	
	if (error)
		return;
	MALLOC(ktp, struct ktr_genio *, sizeof(struct ktr_genio) + len,
		M_TEMP, M_WAITOK);
	ktp->ktr_fd = fd;
	ktp->ktr_rw = rw;
	cp = (caddr_t)((char *)ktp + sizeof (struct ktr_genio));
	while (resid > 0) {
		if ((cnt = iov->iov_len) > resid)
			cnt = resid;
		if (copyin(iov->iov_base, cp, (unsigned)cnt))
			goto done;
		cp += cnt;
		resid -= cnt;
		iov++;
	}
	kth->ktr_buf = (caddr_t)ktp;
	kth->ktr_len = sizeof (struct ktr_genio) + len;

	ktrwrite(vp, kth);
done:
	FREE(kth, M_TEMP);
	FREE(ktp, M_TEMP);
}

ktrpsig(vp, sig, action, mask, code)
	struct	vnode *vp;
	sig_t	action;
{
	struct ktr_header *kth = ktrgetheader(KTR_PSIG);
	struct ktr_psig	kp;

	kp.signo = (char)sig;
	kp.action = action;
	kp.mask = mask;
	kp.code = code;
	kth->ktr_buf = (caddr_t)&kp;
	kth->ktr_len = sizeof (struct ktr_psig);

	ktrwrite(vp, kth);
	FREE(kth, M_TEMP);
}

/* Interface and common routines */

/*
 * ktrace system call
 */

struct ktrace_args {
	char	*fname;
	int	ops;
	int	facs;
	int	pid;
};

/* ARGSUSED */
ktrace(curp, uap, retval)
	struct proc *curp;
	register struct ktrace_args *uap;
	int *retval;
{
	register struct vnode *vp = NULL;
	register struct proc *p;
	struct pgrp *pg;
	int facs = uap->facs & ~KTRFAC_ROOT;
	int ops = KTROP(uap->ops);
	int descend = uap->ops & KTRFLAG_DESCEND;
	int ret = 0;
	int error = 0;
	struct nameidata nd;

	if (ops != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		nd.ni_segflg = UIO_USERSPACE;
		nd.ni_dirp = uap->fname;
		if (error = vn_open(&nd, curp, FREAD|FWRITE, 0))
			return (error);
		vp = nd.ni_vp;
		VOP_UNLOCK(vp);
		if (vp->v_type != VREG) {
			(void) vn_close(vp, FREAD|FWRITE, curp->p_ucred, curp);
			return (EACCES);
		}
	}
	/*
	 * Clear all uses of the tracefile
	 */
	if (ops == KTROP_CLEARFILE) {
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_tracep == vp) {
				if (ktrcanset(curp, p)) {
					p->p_tracep = NULL;
					p->p_traceflag = 0;
					(void) vn_close(vp, FREAD|FWRITE,
						p->p_ucred, p);
				} else
					error = EPERM;
			}
		}
		goto done;
	}
	/*
	 * need something to (un)trace (XXX - why is this here?)
	 */
	if (!facs) {
		error = EINVAL;
		goto done;
	}
	/* 
	 * do it
	 */
	if (uap->pid < 0) {
		/*
		 * by process group
		 */
		pg = pgfind(-uap->pid);
		if (pg == NULL) {
			error = ESRCH;
			goto done;
		}
		for (p = pg->pg_mem; p != NULL; p = p->p_pgrpnxt)
			if (descend)
				ret |= ktrsetchildren(curp, p, ops, facs, vp);
			else 
				ret |= ktrops(curp, p, ops, facs, vp);
					
	} else {
		/*
		 * by pid
		 */
		p = pfind(uap->pid);
		if (p == NULL) {
			error = ESRCH;
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(curp, p, ops, facs, vp);
		else
			ret |= ktrops(curp, p, ops, facs, vp);
	}
	if (!ret)
		error = EPERM;
done:
	if (vp != NULL)
		(void) vn_close(vp, FWRITE, curp->p_ucred, curp);
	return (error);
}

ktrops(curp, p, ops, facs, vp)
	struct proc *curp, *p;
	struct vnode *vp;
{

	if (!ktrcanset(curp, p))
		return (0);
	if (ops == KTROP_SET) {
		if (p->p_tracep != vp) { 
			/*
			 * if trace file already in use, relinquish
			 */
			if (p->p_tracep != NULL)
				vrele(p->p_tracep);
			VREF(vp);
			p->p_tracep = vp;
		}
		p->p_traceflag |= facs;
		if (curp->p_ucred->cr_uid == 0)
			p->p_traceflag |= KTRFAC_ROOT;
	} else {	
		/* KTROP_CLEAR */
		if (((p->p_traceflag &= ~facs) & KTRFAC_MASK) == 0) {
			/* no more tracing */
			p->p_traceflag = 0;
			if (p->p_tracep != NULL) {
				vrele(p->p_tracep);
				p->p_tracep = NULL;
			}
		}
	}

	return (1);
}

ktrsetchildren(curp, top, ops, facs, vp)
	struct proc *curp, *top;
	struct vnode *vp;
{
	register struct proc *p;
	register int ret = 0;

	p = top;
	for (;;) {
		ret |= ktrops(curp, p, ops, facs, vp);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (p->p_cptr)
			p = p->p_cptr;
		else if (p == top)
			return (ret);
		else if (p->p_osptr)
			p = p->p_osptr;
		else for (;;) {
			p = p->p_pptr;
			if (p == top)
				return (ret);
			if (p->p_osptr) {
				p = p->p_osptr;
				break;
			}
		}
	}
	/*NOTREACHED*/
}

ktrwrite(vp, kth)
	struct vnode *vp;
	register struct ktr_header *kth;
{
	struct uio auio;
	struct iovec aiov[2];
	register struct proc *p = curproc;	/* XXX */
	int error;

	if (vp == NULL)
		return;
	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_procp = (struct proc *)0;
	if (kth->ktr_len > 0) {
		auio.uio_iovcnt++;
		aiov[1].iov_base = kth->ktr_buf;
		aiov[1].iov_len = kth->ktr_len;
		auio.uio_resid += kth->ktr_len;
	}
	VOP_LOCK(vp);
	error = VOP_WRITE(vp, &auio, IO_UNIT|IO_APPEND, p->p_ucred);
	VOP_UNLOCK(vp);
	if (!error)
		return;
	/*
	 * If error encountered, give up tracing on this vnode.
	 */
	log(LOG_NOTICE, "ktrace write failed, errno %d, tracing stopped\n",
	    error);
	for (p = allproc; p != NULL; p = p->p_nxt) {
		if (p->p_tracep == vp) {
			p->p_tracep = NULL;
			p->p_traceflag = 0;
			vrele(vp);
		}
	}
}

/*
 * Return true if caller has permission to set the ktracing state
 * of target.  Essentially, the target can't possess any
 * more permissions than the caller.  KTRFAC_ROOT signifies that
 * root previously set the tracing status on the target process, and 
 * so, only root may further change it.
 *
 * TODO: check groups.  use caller effective gid.
 */
ktrcanset(callp, targetp)
	struct proc *callp, *targetp;
{
	register struct pcred *caller = callp->p_cred;
	register struct pcred *target = targetp->p_cred;

	if ((caller->pc_ucred->cr_uid == target->p_ruid &&
	     target->p_ruid == target->p_svuid &&
	     caller->p_rgid == target->p_rgid &&	/* XXX */
	     target->p_rgid == target->p_svgid &&
	     (targetp->p_traceflag & KTRFAC_ROOT) == 0) ||
	     caller->pc_ucred->cr_uid == 0)
		return (1);

	return (0);
}

#endif
