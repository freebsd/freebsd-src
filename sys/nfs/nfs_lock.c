/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      from BSDI nfs_lock.c,v 2.4 1998/12/14 23:49:56 jch Exp
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>			/* for hz */
#include <sys/lockf.h>			/* for hz */

#include <net/if.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_lock.h>
#include <nfs/nlminfo.h>

#define NFSOWNER_1ST_LEVEL_START	1	       /* initial entries */
#define NFSOWNER_2ND_LEVEL	      256		/* some power of 2 */

#define NFSOWNER(tbl, i)	\
		(tbl)[(i) / NFSOWNER_2ND_LEVEL][(i) % NFSOWNER_2ND_LEVEL]

/*
 * XXX
 * We have to let the process know if the call succeeded.  I'm using an extra
 * field in the p_nlminfo field in the proc structure, as it is already for 
 * lockd stuff.
 */

/*
 * nfs_advlock --
 *      NFS advisory byte-level locks.
 */
int
nfs_dolock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t a_id;
		int  a_op;
		struct flock *a_fl;
		int a_flags;
	} */ *ap;
{
	LOCKD_MSG msg;
	struct nameidata nd;
	struct proc *p;
	uid_t	saved_uid;
	struct vnode *vp, *wvp;
	int error, error1;
	struct flock *fl;
	int fmode, ioflg;

	p = curproc;
	vp = ap->a_vp;
	fl = ap->a_fl;

	/*
	 * the NLM protocol doesn't allow the server to return an error
	 * on ranges, so we do it.  Note that we should be returning 
	 * EOVERFLOW in some cases, but we don't have it.
	 */
	if (fl->l_start < 0 || fl->l_len < 0 ||
	    ((fl->l_len != 0 &&
	     (fl->l_start + fl->l_len - 1) < 0)))
		return (EINVAL);

	/*
	 * Fill in the information structure.
	 */
	msg.lm_version = LOCKD_MSG_VERSION;
	msg.lm_msg_ident.pid = p->p_pid;
	/*
	 * if there is no nfsowner table yet, allocate one.
	 */
	if (p->p_nlminfo == NULL) {
		MALLOC(p->p_nlminfo, struct nlminfo *,
			sizeof(struct nlminfo), M_LOCKF, M_WAITOK | M_ZERO);
		p->p_nlminfo->pid_start = p->p_stats->p_start;
	}
	msg.lm_msg_ident.pid_start = p->p_nlminfo->pid_start;
	msg.lm_msg_ident.msg_seq = ++(p->p_nlminfo->msg_seq);

	msg.lm_fl = *fl;
	msg.lm_wait = ap->a_flags & F_WAIT;
	msg.lm_getlk = ap->a_op == F_GETLK;
	/*
	 * XXX  -- I think this is wrong for anything other AF_INET.
	 */
	msg.lm_addr = *(VFSTONFS(vp->v_mount)->nm_nam);
	msg.lm_fh_len = NFS_ISV3(vp) ? VTONFS(vp)->n_fhsize : NFSX_V2FH;
	bcopy(VTONFS(vp)->n_fhp, msg.lm_fh, msg.lm_fh_len);
	msg.lm_nfsv3 = NFS_ISV3(vp);
	msg.lm_cred = *(p->p_ucred);

	/*
	 * Open the lock fifo.  If for any reason we don't find the fifo, it
	 * means that the lock daemon isn't running.  Translate any missing
	 * file error message for the user, otherwise the application will
	 * complain that the user's file is missing, which isn't the case.
	 * Note that we use proc0's cred, so the fifo is opened as root.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, _PATH_LCKFIFO, p);

	/*
	 * XXX Hack to temporarily allow this process (regardless of it's creds)
	 * to open the fifo we need to write to. vn_open() really should
	 * take a ucred (and once it does, this code should be fixed to use
	 * proc0's ucred.
	 */
	saved_uid = p->p_ucred->cr_uid;
	p->p_ucred->cr_uid = 0;		/* temporarly run the vn_open as root */

	fmode = FFLAGS(O_WRONLY);
	error = vn_open(&nd, &fmode, 0);
	p->p_ucred->cr_uid = saved_uid;
	if (error != 0) {
		return (error == ENOENT ? EOPNOTSUPP : error);
	}
	wvp = nd.ni_vp;
	VOP_UNLOCK(wvp, 0, p);		/* vn_open leaves it locked */


	ioflg = IO_UNIT;
	for (;;) {
		VOP_LEASE(wvp, p, proc0.p_ucred, LEASE_WRITE);

		error = vn_rdwr(UIO_WRITE, wvp, (caddr_t)&msg, sizeof(msg), 0,
		    UIO_SYSSPACE, ioflg, proc0.p_ucred, NULL, p);

		if (error && (((ioflg & IO_NDELAY) == 0) || error != EAGAIN)) {
			break;
		}
		/*
		 * If we're locking a file, wait for an answer.  Unlocks succeed
		 * immediately.
		 */
		if (fl->l_type == F_UNLCK)
			/*
			 * XXX this isn't exactly correct.  The client side
			 * needs to continue sending it's unlock until
			 * it gets a responce back.
			 */
			break;

		/*
		 * retry after 20 seconds if we haven't gotten a responce yet.
		 * This number was picked out of thin air... but is longer
		 * then even a reasonably loaded system should take (at least
		 * on a local network).  XXX Probably should use a back-off 
		 * scheme.
		 */
		if ((error = tsleep((void *)p->p_nlminfo, 
					PCATCH | PUSER, "lockd", 20*hz)) != 0) {
			if (error == EWOULDBLOCK) {
				/*
				 * We timed out, so we rewrite the request
				 * to the fifo, but only if it isn't already
				 * full.
				 */
				ioflg |= IO_NDELAY;
				continue;
			}

			break;
		}

		if (msg.lm_getlk && p->p_nlminfo->retcode == 0) {
			if (p->p_nlminfo->set_getlk_pid) {
				fl->l_pid = p->p_nlminfo->getlk_pid;
			} else {
				fl->l_type = F_UNLCK;
			}
		}
		error = p->p_nlminfo->retcode;
		break;
	}

	if ((error1 = vn_close(wvp, FWRITE, proc0.p_ucred, p)) && error == 0)
		return (error1);

	return (error);
}

/*
 * nfslockdans --
 *      NFS advisory byte-level locks answer from the lock daemon.
 */
int
nfslockdans(p, ansp)
	struct proc *p;
	struct lockd_ans *ansp;
{
	int error;

	/* Let root, or someone who once was root (lockd generally
	 * switches to the daemon uid once it is done setting up) make 
	 * this call
	 */
	if ((error = suser(p)) != 0 && p->p_cred->p_svuid != 0)
		return (error);

	/* the version should match, or we're out of sync */
	if (ansp->la_vers != LOCKD_ANS_VERSION)
		return (EINVAL);

	/* Find the process, set its return errno and wake it up. */
	if ((p = pfind(ansp->la_msg_ident.pid)) == NULL)
		return (ESRCH);

	/* verify the pid hasn't been reused (if we can), and it isn't waiting 
	 * for an answer from a more recent request.  We return an EPIPE if
	 * the match fails, because we've already used ESRCH above, and this
	 * is sort of like writing on a pipe after the reader has closed it.
	 */
	if (p->p_nlminfo == NULL ||
	    ((ansp->la_msg_ident.msg_seq != -1) &&
	      (timevalcmp(&p->p_nlminfo->pid_start,
			&ansp->la_msg_ident.pid_start, !=) ||
	       p->p_nlminfo->msg_seq != ansp->la_msg_ident.msg_seq)))
		return (EPIPE);

	p->p_nlminfo->retcode = ansp->la_errno;
	p->p_nlminfo->set_getlk_pid = ansp->la_set_getlk_pid;
	p->p_nlminfo->getlk_pid = ansp->la_getlk_pid;

	(void)wakeup((void *)p->p_nlminfo);

	return (0);
}
