/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>
#include <sys/mutex.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nfs/xdr_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfs_lock.h>

static MALLOC_DEFINE(M_NFSSVC, "NFS srvsock", "Nfs server structure");

static void	nfssvc_iod(void *);

#define	TRUE	1
#define	FALSE	0

static int nfs_asyncdaemon[NFS_MAXASYNCDAEMON];

SYSCTL_DECL(_vfs_nfs);

static void
nfsiod_setup(void *dummy)
{
	int i;
	int error;
	struct proc *p;

	for (i = 0; i < 4; i++) {
		error = kthread_create(nfssvc_iod, NULL, &p, RFHIGHPID,
		    "nfsiod %d", i);
		if (error)
			panic("nfsiod_setup: kthread_create error %d", error);
	}
}
SYSINIT(nfsiod, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, nfsiod_setup, NULL);

static int nfs_defect = 0;
SYSCTL_INT(_vfs_nfs, OID_AUTO, defect, CTLFLAG_RW, &nfs_defect, 0, "");

int
nfsclnt(struct thread *td, struct nfsclnt_args *uap)
{
	struct lockd_ans la;
	int error;

	if ((uap->flag & NFSCLNT_LOCKDANS) != 0) {
		error = copyin(uap->argp, &la, sizeof(la));
		return (error != 0 ? error : nfslockdans(td, &la));
	}
	return EINVAL;
}

/*
 * Asynchronous I/O daemons for client nfs.
 * They do read-ahead and write-behind operations on the block I/O cache.
 * Never returns unless it fails or gets killed.
 */
static void
nfssvc_iod(void *dummy)
{
	struct buf *bp;
	int i, myiod;
	struct nfsmount *nmp;
	int error = 0;

	mtx_lock(&Giant);
	/*
	 * Assign my position or return error if too many already running
	 */
	myiod = -1;
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++)
		if (nfs_asyncdaemon[i] == 0) {
			nfs_asyncdaemon[i]++;
			myiod = i;
			break;
		}
	if (myiod == -1)
		return /* XXX (EBUSY) */;
	nfs_numasync++;
	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
	    while (((nmp = nfs_iodmount[myiod]) == NULL
		    || !TAILQ_FIRST(&nmp->nm_bufq))
		   && error == 0) {
		if (nmp)
		    nmp->nm_bufqiods--;
		nfs_iodwant[myiod] = curthread->td_proc;
		nfs_iodmount[myiod] = NULL;
		error = tsleep((caddr_t)&nfs_iodwant[myiod],
			PWAIT | PCATCH, "nfsidl", 0);
	    }
	    if (error) {
		nfs_asyncdaemon[myiod] = 0;
		if (nmp)
		    nmp->nm_bufqiods--;
		nfs_iodwant[myiod] = NULL;
		nfs_iodmount[myiod] = NULL;
		nfs_numasync--;
		return /* XXX (error) */;
	    }
	    while ((bp = TAILQ_FIRST(&nmp->nm_bufq)) != NULL) {
		/* Take one off the front of the list */
		TAILQ_REMOVE(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen--;
		if (nmp->nm_bufqwant && nmp->nm_bufqlen <= nfs_numasync) {
		    nmp->nm_bufqwant = FALSE;
		    wakeup(&nmp->nm_bufq);
		}
		if (bp->b_iocmd == BIO_READ)
		    (void) nfs_doio(bp, bp->b_rcred, (struct thread *)0);
		else
		    (void) nfs_doio(bp, bp->b_wcred, (struct thread *)0);
		/*
		 * If there are more than one iod on this mount, then defect
		 * so that the iods can be shared out fairly between the mounts
		 */
		if (nfs_defect && nmp->nm_bufqiods > 1) {
		    NFS_DPF(ASYNCIO,
			    ("nfssvc_iod: iod %d defecting from mount %p\n",
			     myiod, nmp));
		    nfs_iodmount[myiod] = NULL;
		    nmp->nm_bufqiods--;
		    break;
		}
	    }
	}
}
