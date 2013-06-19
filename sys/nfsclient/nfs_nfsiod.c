/*-
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
#include <sys/taskqueue.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/xdr_subs.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>
#include <nfs/nfs_lock.h>

static MALLOC_DEFINE(M_NFSSVC, "nfsclient_srvsock", "Nfs server structure");

static void	nfssvc_iod(void *);

static int nfs_asyncdaemon[NFS_MAXASYNCDAEMON];

SYSCTL_DECL(_vfs_oldnfs);

/* Maximum number of seconds a nfsiod kthread will sleep before exiting */
static unsigned int nfs_iodmaxidle = 120;
SYSCTL_UINT(_vfs_oldnfs, OID_AUTO, iodmaxidle, CTLFLAG_RW, &nfs_iodmaxidle, 0,
    "Max number of seconds an nfsiod kthread will sleep before exiting");

/* Maximum number of nfsiod kthreads */
unsigned int nfs_iodmax = 20;

/* Minimum number of nfsiod kthreads to keep as spares */
static unsigned int nfs_iodmin = 0;

static int nfs_nfsiodnew_sync(void);

static int
sysctl_iodmin(SYSCTL_HANDLER_ARGS)
{
	int error, i;
	int newmin;

	newmin = nfs_iodmin;
	error = sysctl_handle_int(oidp, &newmin, 0, req);
	if (error || (req->newptr == NULL))
		return (error);
	mtx_lock(&nfs_iod_mtx);
	if (newmin > nfs_iodmax) {
		error = EINVAL;
		goto out;
	}
	nfs_iodmin = newmin;
	if (nfs_numasync >= nfs_iodmin)
		goto out;
	/*
	 * If the current number of nfsiod is lower
	 * than the new minimum, create some more.
	 */
	for (i = nfs_iodmin - nfs_numasync; i > 0; i--)
		nfs_nfsiodnew_sync();
out:
	mtx_unlock(&nfs_iod_mtx);	
	return (0);
}
SYSCTL_PROC(_vfs_oldnfs, OID_AUTO, iodmin, CTLTYPE_UINT | CTLFLAG_RW, 0,
    sizeof (nfs_iodmin), sysctl_iodmin, "IU",
    "Min number of nfsiod kthreads to keep as spares");


static int
sysctl_iodmax(SYSCTL_HANDLER_ARGS)
{
	int error, i;
	int iod, newmax;

	newmax = nfs_iodmax;
	error = sysctl_handle_int(oidp, &newmax, 0, req);
	if (error || (req->newptr == NULL))
		return (error);
	if (newmax > NFS_MAXASYNCDAEMON)
		return (EINVAL);
	mtx_lock(&nfs_iod_mtx);
	nfs_iodmax = newmax;
	if (nfs_numasync <= nfs_iodmax)
		goto out;
	/*
	 * If there are some asleep nfsiods that should
	 * exit, wakeup() them so that they check nfs_iodmax
	 * and exit.  Those who are active will exit as
	 * soon as they finish I/O.
	 */
	iod = nfs_numasync - 1;
	for (i = 0; i < nfs_numasync - nfs_iodmax; i++) {
		if (nfs_iodwant[iod] == NFSIOD_AVAILABLE)
			wakeup(&nfs_iodwant[iod]);
		iod--;
	}
out:
	mtx_unlock(&nfs_iod_mtx);
	return (0);
}
SYSCTL_PROC(_vfs_oldnfs, OID_AUTO, iodmax, CTLTYPE_UINT | CTLFLAG_RW, 0,
    sizeof (nfs_iodmax), sysctl_iodmax, "IU",
    "Max number of nfsiod kthreads");

static int
nfs_nfsiodnew_sync(void)
{
	int error, i;

	mtx_assert(&nfs_iod_mtx, MA_OWNED);
	for (i = 0; i < nfs_iodmax; i++) {
		if (nfs_asyncdaemon[i] == 0) {
			nfs_asyncdaemon[i] = 1;
			break;
		}
	}
	if (i == nfs_iodmax)
		return (0);
	mtx_unlock(&nfs_iod_mtx);
	error = kproc_create(nfssvc_iod, nfs_asyncdaemon + i, NULL,
	    RFHIGHPID, 0, "nfsiod %d", i);
	mtx_lock(&nfs_iod_mtx);
	if (error == 0) {
		nfs_numasync++;
		nfs_iodwant[i] = NFSIOD_AVAILABLE;
	} else
		nfs_asyncdaemon[i] = 0;
	return (error);
}

void
nfs_nfsiodnew_tq(__unused void *arg, int pending)
{

	mtx_lock(&nfs_iod_mtx);
	while (pending > 0) {
		pending--;
		nfs_nfsiodnew_sync();
	}
	mtx_unlock(&nfs_iod_mtx);
}

void
nfs_nfsiodnew(void)
{

	mtx_assert(&nfs_iod_mtx, MA_OWNED);
	taskqueue_enqueue(taskqueue_thread, &nfs_nfsiodnew_task);
}

static void
nfsiod_setup(void *dummy)
{
	int error;

	TUNABLE_INT_FETCH("vfs.oldnfs.iodmin", &nfs_iodmin);
	mtx_lock(&nfs_iod_mtx);
	/* Silently limit the start number of nfsiod's */
	if (nfs_iodmin > NFS_MAXASYNCDAEMON)
		nfs_iodmin = NFS_MAXASYNCDAEMON;

	while (nfs_numasync < nfs_iodmin) {
		error = nfs_nfsiodnew_sync();
		if (error == -1)
			panic("nfsiod_setup: nfs_nfsiodnew failed");
	}
	mtx_unlock(&nfs_iod_mtx);
}
SYSINIT(nfsiod, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, nfsiod_setup, NULL);

static int nfs_defect = 0;
SYSCTL_INT(_vfs_oldnfs, OID_AUTO, defect, CTLFLAG_RW, &nfs_defect, 0,
    "Allow nfsiods to migrate serving different mounts");

/*
 * Asynchronous I/O daemons for client nfs.
 * They do read-ahead and write-behind operations on the block I/O cache.
 * Returns if we hit the timeout defined by the iodmaxidle sysctl.
 */
static void
nfssvc_iod(void *instance)
{
	struct buf *bp;
	struct nfsmount *nmp;
	int myiod, timo;
	int error = 0;

	mtx_lock(&nfs_iod_mtx);
	myiod = (int *)instance - nfs_asyncdaemon;
	/*
	 * Main loop
	 */
	for (;;) {
	    while (((nmp = nfs_iodmount[myiod]) == NULL)
		   || !TAILQ_FIRST(&nmp->nm_bufq)) {
		if (myiod >= nfs_iodmax)
			goto finish;
		if (nmp)
			nmp->nm_bufqiods--;
		if (nfs_iodwant[myiod] == NFSIOD_NOT_AVAILABLE)
			nfs_iodwant[myiod] = NFSIOD_AVAILABLE;
		nfs_iodmount[myiod] = NULL;
		/*
		 * Always keep at least nfs_iodmin kthreads.
		 */
		timo = (myiod < nfs_iodmin) ? 0 : nfs_iodmaxidle * hz;
		error = msleep(&nfs_iodwant[myiod], &nfs_iod_mtx, PWAIT | PCATCH,
		    "-", timo);
		if (error) {
			nmp = nfs_iodmount[myiod];
			/*
			 * Rechecking the nm_bufq closes a rare race where the 
			 * nfsiod is woken up at the exact time the idle timeout
			 * fires
			 */
			if (nmp && TAILQ_FIRST(&nmp->nm_bufq))
				error = 0;
			break;
		}
	    }
	    if (error)
		    break;
	    while ((bp = TAILQ_FIRST(&nmp->nm_bufq)) != NULL) {
	        int giant_locked = 0;
		    
		/* Take one off the front of the list */
		TAILQ_REMOVE(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen--;
		if (nmp->nm_bufqwant && nmp->nm_bufqlen <= nfs_numasync) {
		    nmp->nm_bufqwant = 0;
		    wakeup(&nmp->nm_bufq);
		}
		mtx_unlock(&nfs_iod_mtx);
		if (NFS_ISV4(bp->b_vp)) {
			giant_locked = 1;
			mtx_lock(&Giant);
		}
		if (bp->b_flags & B_DIRECT) {
			KASSERT((bp->b_iocmd == BIO_WRITE), ("nfscvs_iod: BIO_WRITE not set"));
			(void)nfs_doio_directwrite(bp);
		} else {
			if (bp->b_iocmd == BIO_READ)
				(void) nfs_doio(bp->b_vp, bp, bp->b_rcred, NULL);
			else
				(void) nfs_doio(bp->b_vp, bp, bp->b_wcred, NULL);
		}
		if (giant_locked)
			mtx_unlock(&Giant);
		mtx_lock(&nfs_iod_mtx);
		/*
		 * Make sure the nmp hasn't been dismounted as soon as
		 * nfs_doio() completes for the last buffer.
		 */
		nmp = nfs_iodmount[myiod];
		if (nmp == NULL)
			break;

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
finish:
	nfs_asyncdaemon[myiod] = 0;
	if (nmp)
	    nmp->nm_bufqiods--;
	nfs_iodwant[myiod] = NFSIOD_NOT_AVAILABLE;
	nfs_iodmount[myiod] = NULL;
	/* Someone may be waiting for the last nfsiod to terminate. */
	if (--nfs_numasync == 0)
		wakeup(&nfs_numasync);
	mtx_unlock(&nfs_iod_mtx);
	if ((error == 0) || (error == EWOULDBLOCK))
		kproc_exit(0);
	/* Abnormal termination */
	kproc_exit(1);
}
