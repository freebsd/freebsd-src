/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California. All Rights Reserved.
 * Copyright (c) 2004-2009 Robert N. M. Watson All Rights Reserved.
 * Copyright (c) 2018 Matthew Macy
 * Copyright (c) 2022 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	From: @(#)uipc_usrreq.c	8.3 (Berkeley) 1/4/94
 */

/*
 * UNIX Domain (Local) Sockets
 *
 * This is an implementation of UNIX (local) domain sockets.  Each socket has
 * an associated struct unpcb (UNIX protocol control block).  Stream sockets
 * may be connected to 0 or 1 other socket.  Datagram sockets may be
 * connected to 0, 1, or many other sockets.  Sockets may be created and
 * connected in pairs (socketpair(2)), or bound/connected to using the file
 * system name space.  For most purposes, only the receive socket buffer is
 * used, as sending on one socket delivers directly to the receive socket
 * buffer of a second socket.
 *
 * The implementation is substantially complicated by the fact that
 * "ancillary data", such as file descriptors or credentials, may be passed
 * across UNIX domain sockets.  The potential for passing UNIX domain sockets
 * over other UNIX domain sockets requires the implementation of a simple
 * garbage collector to find and tear down cycles of disconnected sockets.
 *
 * TODO:
 *	RDM
 *	rethink name space problems
 *	need a proper out-of-band
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/vnode.h>

#include <net/vnet.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <security/mac/mac_framework.h>

#include <vm/uma.h>

MALLOC_DECLARE(M_FILECAPS);

static struct domain localdomain;

static uma_zone_t	unp_zone;
static unp_gen_t	unp_gencnt;	/* (l) */
static u_int		unp_count;	/* (l) Count of local sockets. */
static ino_t		unp_ino;	/* Prototype for fake inode numbers. */
static int		unp_rights;	/* (g) File descriptors in flight. */
static struct unp_head	unp_shead;	/* (l) List of stream sockets. */
static struct unp_head	unp_dhead;	/* (l) List of datagram sockets. */
static struct unp_head	unp_sphead;	/* (l) List of seqpacket sockets. */

struct unp_defer {
	SLIST_ENTRY(unp_defer) ud_link;
	struct file *ud_fp;
};
static SLIST_HEAD(, unp_defer) unp_defers;
static int unp_defers_count;

static const struct sockaddr	sun_noname = { sizeof(sun_noname), AF_LOCAL };

/*
 * Garbage collection of cyclic file descriptor/socket references occurs
 * asynchronously in a taskqueue context in order to avoid recursion and
 * reentrance in the UNIX domain socket, file descriptor, and socket layer
 * code.  See unp_gc() for a full description.
 */
static struct timeout_task unp_gc_task;

/*
 * The close of unix domain sockets attached as SCM_RIGHTS is
 * postponed to the taskqueue, to avoid arbitrary recursion depth.
 * The attached sockets might have another sockets attached.
 */
static struct task	unp_defer_task;

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering for
 * stream sockets, although the total for sender and receiver is actually
 * only PIPSIZ.
 *
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should be
 * large enough for at least one max-size datagram plus address.
 */
#ifndef PIPSIZ
#define	PIPSIZ	8192
#endif
static u_long	unpst_sendspace = PIPSIZ;
static u_long	unpst_recvspace = PIPSIZ;
static u_long	unpdg_maxdgram = 2*1024;
static u_long	unpdg_recvspace = 16*1024;	/* support 8KB syslog msgs */
static u_long	unpsp_sendspace = PIPSIZ;	/* really max datagram size */
static u_long	unpsp_recvspace = PIPSIZ;

static SYSCTL_NODE(_net, PF_LOCAL, local, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Local domain");
static SYSCTL_NODE(_net_local, SOCK_STREAM, stream,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "SOCK_STREAM");
static SYSCTL_NODE(_net_local, SOCK_DGRAM, dgram,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "SOCK_DGRAM");
static SYSCTL_NODE(_net_local, SOCK_SEQPACKET, seqpacket,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "SOCK_SEQPACKET");

SYSCTL_ULONG(_net_local_stream, OID_AUTO, sendspace, CTLFLAG_RW,
	   &unpst_sendspace, 0, "Default stream send space.");
SYSCTL_ULONG(_net_local_stream, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpst_recvspace, 0, "Default stream receive space.");
SYSCTL_ULONG(_net_local_dgram, OID_AUTO, maxdgram, CTLFLAG_RW,
	   &unpdg_maxdgram, 0, "Maximum datagram size.");
SYSCTL_ULONG(_net_local_dgram, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpdg_recvspace, 0, "Default datagram receive space.");
SYSCTL_ULONG(_net_local_seqpacket, OID_AUTO, maxseqpacket, CTLFLAG_RW,
	   &unpsp_sendspace, 0, "Default seqpacket send space.");
SYSCTL_ULONG(_net_local_seqpacket, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpsp_recvspace, 0, "Default seqpacket receive space.");
SYSCTL_INT(_net_local, OID_AUTO, inflight, CTLFLAG_RD, &unp_rights, 0,
    "File descriptors in flight.");
SYSCTL_INT(_net_local, OID_AUTO, deferred, CTLFLAG_RD,
    &unp_defers_count, 0,
    "File descriptors deferred to taskqueue for close.");

/*
 * Locking and synchronization:
 *
 * Several types of locks exist in the local domain socket implementation:
 * - a global linkage lock
 * - a global connection list lock
 * - the mtxpool lock
 * - per-unpcb mutexes
 *
 * The linkage lock protects the global socket lists, the generation number
 * counter and garbage collector state.
 *
 * The connection list lock protects the list of referring sockets in a datagram
 * socket PCB.  This lock is also overloaded to protect a global list of
 * sockets whose buffers contain socket references in the form of SCM_RIGHTS
 * messages.  To avoid recursion, such references are released by a dedicated
 * thread.
 *
 * The mtxpool lock protects the vnode from being modified while referenced.
 * Lock ordering rules require that it be acquired before any PCB locks.
 *
 * The unpcb lock (unp_mtx) protects the most commonly referenced fields in the
 * unpcb.  This includes the unp_conn field, which either links two connected
 * PCBs together (for connected socket types) or points at the destination
 * socket (for connectionless socket types).  The operations of creating or
 * destroying a connection therefore involve locking multiple PCBs.  To avoid
 * lock order reversals, in some cases this involves dropping a PCB lock and
 * using a reference counter to maintain liveness.
 *
 * UNIX domain sockets each have an unpcb hung off of their so_pcb pointer,
 * allocated in pr_attach() and freed in pr_detach().  The validity of that
 * pointer is an invariant, so no lock is required to dereference the so_pcb
 * pointer if a valid socket reference is held by the caller.  In practice,
 * this is always true during operations performed on a socket.  Each unpcb
 * has a back-pointer to its socket, unp_socket, which will be stable under
 * the same circumstances.
 *
 * This pointer may only be safely dereferenced as long as a valid reference
 * to the unpcb is held.  Typically, this reference will be from the socket,
 * or from another unpcb when the referring unpcb's lock is held (in order
 * that the reference not be invalidated during use).  For example, to follow
 * unp->unp_conn->unp_socket, you need to hold a lock on unp_conn to guarantee
 * that detach is not run clearing unp_socket.
 *
 * Blocking with UNIX domain sockets is a tricky issue: unlike most network
 * protocols, bind() is a non-atomic operation, and connect() requires
 * potential sleeping in the protocol, due to potentially waiting on local or
 * distributed file systems.  We try to separate "lookup" operations, which
 * may sleep, and the IPC operations themselves, which typically can occur
 * with relative atomicity as locks can be held over the entire operation.
 *
 * Another tricky issue is simultaneous multi-threaded or multi-process
 * access to a single UNIX domain socket.  These are handled by the flags
 * UNP_CONNECTING and UNP_BINDING, which prevent concurrent connecting or
 * binding, both of which involve dropping UNIX domain socket locks in order
 * to perform namei() and other file system operations.
 */
static struct rwlock	unp_link_rwlock;
static struct mtx	unp_defers_lock;

#define	UNP_LINK_LOCK_INIT()		rw_init(&unp_link_rwlock,	\
					    "unp_link_rwlock")

#define	UNP_LINK_LOCK_ASSERT()		rw_assert(&unp_link_rwlock,	\
					    RA_LOCKED)
#define	UNP_LINK_UNLOCK_ASSERT()	rw_assert(&unp_link_rwlock,	\
					    RA_UNLOCKED)

#define	UNP_LINK_RLOCK()		rw_rlock(&unp_link_rwlock)
#define	UNP_LINK_RUNLOCK()		rw_runlock(&unp_link_rwlock)
#define	UNP_LINK_WLOCK()		rw_wlock(&unp_link_rwlock)
#define	UNP_LINK_WUNLOCK()		rw_wunlock(&unp_link_rwlock)
#define	UNP_LINK_WLOCK_ASSERT()		rw_assert(&unp_link_rwlock,	\
					    RA_WLOCKED)
#define	UNP_LINK_WOWNED()		rw_wowned(&unp_link_rwlock)

#define	UNP_DEFERRED_LOCK_INIT()	mtx_init(&unp_defers_lock, \
					    "unp_defer", NULL, MTX_DEF)
#define	UNP_DEFERRED_LOCK()		mtx_lock(&unp_defers_lock)
#define	UNP_DEFERRED_UNLOCK()		mtx_unlock(&unp_defers_lock)

#define UNP_REF_LIST_LOCK()		UNP_DEFERRED_LOCK();
#define UNP_REF_LIST_UNLOCK()		UNP_DEFERRED_UNLOCK();

#define UNP_PCB_LOCK_INIT(unp)		mtx_init(&(unp)->unp_mtx,	\
					    "unp", "unp",	\
					    MTX_DUPOK|MTX_DEF)
#define	UNP_PCB_LOCK_DESTROY(unp)	mtx_destroy(&(unp)->unp_mtx)
#define	UNP_PCB_LOCKPTR(unp)		(&(unp)->unp_mtx)
#define	UNP_PCB_LOCK(unp)		mtx_lock(&(unp)->unp_mtx)
#define	UNP_PCB_TRYLOCK(unp)		mtx_trylock(&(unp)->unp_mtx)
#define	UNP_PCB_UNLOCK(unp)		mtx_unlock(&(unp)->unp_mtx)
#define	UNP_PCB_OWNED(unp)		mtx_owned(&(unp)->unp_mtx)
#define	UNP_PCB_LOCK_ASSERT(unp)	mtx_assert(&(unp)->unp_mtx, MA_OWNED)
#define	UNP_PCB_UNLOCK_ASSERT(unp)	mtx_assert(&(unp)->unp_mtx, MA_NOTOWNED)

static int	uipc_connect2(struct socket *, struct socket *);
static int	uipc_ctloutput(struct socket *, struct sockopt *);
static int	unp_connect(struct socket *, struct sockaddr *,
		    struct thread *);
static int	unp_connectat(int, struct socket *, struct sockaddr *,
		    struct thread *, bool);
typedef enum { PRU_CONNECT, PRU_CONNECT2 } conn2_how;
static void	unp_connect2(struct socket *so, struct socket *so2, conn2_how);
static void	unp_disconnect(struct unpcb *unp, struct unpcb *unp2);
static void	unp_dispose(struct socket *so);
static void	unp_shutdown(struct unpcb *);
static void	unp_drop(struct unpcb *);
static void	unp_gc(__unused void *, int);
static void	unp_scan(struct mbuf *, void (*)(struct filedescent **, int));
static void	unp_discard(struct file *);
static void	unp_freerights(struct filedescent **, int);
static int	unp_internalize(struct mbuf **, struct thread *,
		    struct mbuf **, u_int *, u_int *);
static void	unp_internalize_fp(struct file *);
static int	unp_externalize(struct mbuf *, struct mbuf **, int);
static int	unp_externalize_fp(struct file *);
static struct mbuf	*unp_addsockcred(struct thread *, struct mbuf *,
		    int, struct mbuf **, u_int *, u_int *);
static void	unp_process_defers(void * __unused, int);

static void
unp_pcb_hold(struct unpcb *unp)
{
	u_int old __unused;

	old = refcount_acquire(&unp->unp_refcount);
	KASSERT(old > 0, ("%s: unpcb %p has no references", __func__, unp));
}

static __result_use_check bool
unp_pcb_rele(struct unpcb *unp)
{
	bool ret;

	UNP_PCB_LOCK_ASSERT(unp);

	if ((ret = refcount_release(&unp->unp_refcount))) {
		UNP_PCB_UNLOCK(unp);
		UNP_PCB_LOCK_DESTROY(unp);
		uma_zfree(unp_zone, unp);
	}
	return (ret);
}

static void
unp_pcb_rele_notlast(struct unpcb *unp)
{
	bool ret __unused;

	ret = refcount_release(&unp->unp_refcount);
	KASSERT(!ret, ("%s: unpcb %p has no references", __func__, unp));
}

static void
unp_pcb_lock_pair(struct unpcb *unp, struct unpcb *unp2)
{
	UNP_PCB_UNLOCK_ASSERT(unp);
	UNP_PCB_UNLOCK_ASSERT(unp2);

	if (unp == unp2) {
		UNP_PCB_LOCK(unp);
	} else if ((uintptr_t)unp2 > (uintptr_t)unp) {
		UNP_PCB_LOCK(unp);
		UNP_PCB_LOCK(unp2);
	} else {
		UNP_PCB_LOCK(unp2);
		UNP_PCB_LOCK(unp);
	}
}

static void
unp_pcb_unlock_pair(struct unpcb *unp, struct unpcb *unp2)
{
	UNP_PCB_UNLOCK(unp);
	if (unp != unp2)
		UNP_PCB_UNLOCK(unp2);
}

/*
 * Try to lock the connected peer of an already locked socket.  In some cases
 * this requires that we unlock the current socket.  The pairbusy counter is
 * used to block concurrent connection attempts while the lock is dropped.  The
 * caller must be careful to revalidate PCB state.
 */
static struct unpcb *
unp_pcb_lock_peer(struct unpcb *unp)
{
	struct unpcb *unp2;

	UNP_PCB_LOCK_ASSERT(unp);
	unp2 = unp->unp_conn;
	if (unp2 == NULL)
		return (NULL);
	if (__predict_false(unp == unp2))
		return (unp);

	UNP_PCB_UNLOCK_ASSERT(unp2);

	if (__predict_true(UNP_PCB_TRYLOCK(unp2)))
		return (unp2);
	if ((uintptr_t)unp2 > (uintptr_t)unp) {
		UNP_PCB_LOCK(unp2);
		return (unp2);
	}
	unp->unp_pairbusy++;
	unp_pcb_hold(unp2);
	UNP_PCB_UNLOCK(unp);

	UNP_PCB_LOCK(unp2);
	UNP_PCB_LOCK(unp);
	KASSERT(unp->unp_conn == unp2 || unp->unp_conn == NULL,
	    ("%s: socket %p was reconnected", __func__, unp));
	if (--unp->unp_pairbusy == 0 && (unp->unp_flags & UNP_WAITING) != 0) {
		unp->unp_flags &= ~UNP_WAITING;
		wakeup(unp);
	}
	if (unp_pcb_rele(unp2)) {
		/* unp2 is unlocked. */
		return (NULL);
	}
	if (unp->unp_conn == NULL) {
		UNP_PCB_UNLOCK(unp2);
		return (NULL);
	}
	return (unp2);
}

static void
uipc_abort(struct socket *so)
{
	struct unpcb *unp, *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_abort: unp == NULL"));
	UNP_PCB_UNLOCK_ASSERT(unp);

	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		unp_pcb_hold(unp2);
		UNP_PCB_UNLOCK(unp);
		unp_drop(unp2);
	} else
		UNP_PCB_UNLOCK(unp);
}

static int
uipc_accept(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp, *unp2;
	const struct sockaddr *sa;

	/*
	 * Pass back name of connected socket, if it was bound and we are
	 * still connected (our peer may have closed already!).
	 */
	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_accept: unp == NULL"));

	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	UNP_PCB_LOCK(unp);
	unp2 = unp_pcb_lock_peer(unp);
	if (unp2 != NULL && unp2->unp_addr != NULL)
		sa = (struct sockaddr *)unp2->unp_addr;
	else
		sa = &sun_noname;
	bcopy(sa, *nam, sa->sa_len);
	if (unp2 != NULL)
		unp_pcb_unlock_pair(unp, unp2);
	else
		UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_attach(struct socket *so, int proto, struct thread *td)
{
	u_long sendspace, recvspace;
	struct unpcb *unp;
	int error;
	bool locked;

	KASSERT(so->so_pcb == NULL, ("uipc_attach: so_pcb != NULL"));
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {
		case SOCK_STREAM:
			sendspace = unpst_sendspace;
			recvspace = unpst_recvspace;
			break;

		case SOCK_DGRAM:
			STAILQ_INIT(&so->so_rcv.uxdg_mb);
			STAILQ_INIT(&so->so_snd.uxdg_mb);
			TAILQ_INIT(&so->so_rcv.uxdg_conns);
			/*
			 * Since send buffer is either bypassed or is a part
			 * of one-to-many receive buffer, we assign both space
			 * limits to unpdg_recvspace.
			 */
			sendspace = recvspace = unpdg_recvspace;
			break;

		case SOCK_SEQPACKET:
			sendspace = unpsp_sendspace;
			recvspace = unpsp_recvspace;
			break;

		default:
			panic("uipc_attach");
		}
		error = soreserve(so, sendspace, recvspace);
		if (error)
			return (error);
	}
	unp = uma_zalloc(unp_zone, M_NOWAIT | M_ZERO);
	if (unp == NULL)
		return (ENOBUFS);
	LIST_INIT(&unp->unp_refs);
	UNP_PCB_LOCK_INIT(unp);
	unp->unp_socket = so;
	so->so_pcb = unp;
	refcount_init(&unp->unp_refcount, 1);

	if ((locked = UNP_LINK_WOWNED()) == false)
		UNP_LINK_WLOCK();

	unp->unp_gencnt = ++unp_gencnt;
	unp->unp_ino = ++unp_ino;
	unp_count++;
	switch (so->so_type) {
	case SOCK_STREAM:
		LIST_INSERT_HEAD(&unp_shead, unp, unp_link);
		break;

	case SOCK_DGRAM:
		LIST_INSERT_HEAD(&unp_dhead, unp, unp_link);
		break;

	case SOCK_SEQPACKET:
		LIST_INSERT_HEAD(&unp_sphead, unp, unp_link);
		break;

	default:
		panic("uipc_attach");
	}

	if (locked == false)
		UNP_LINK_WUNLOCK();

	return (0);
}

static int
uipc_bindat(int fd, struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	struct vattr vattr;
	int error, namelen;
	struct nameidata nd;
	struct unpcb *unp;
	struct vnode *vp;
	struct mount *mp;
	cap_rights_t rights;
	char *buf;

	if (nam->sa_family != AF_UNIX)
		return (EAFNOSUPPORT);

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_bind: unp == NULL"));

	if (soun->sun_len > sizeof(struct sockaddr_un))
		return (EINVAL);
	namelen = soun->sun_len - offsetof(struct sockaddr_un, sun_path);
	if (namelen <= 0)
		return (EINVAL);

	/*
	 * We don't allow simultaneous bind() calls on a single UNIX domain
	 * socket, so flag in-progress operations, and return an error if an
	 * operation is already in progress.
	 *
	 * Historically, we have not allowed a socket to be rebound, so this
	 * also returns an error.  Not allowing re-binding simplifies the
	 * implementation and avoids a great many possible failure modes.
	 */
	UNP_PCB_LOCK(unp);
	if (unp->unp_vnode != NULL) {
		UNP_PCB_UNLOCK(unp);
		return (EINVAL);
	}
	if (unp->unp_flags & UNP_BINDING) {
		UNP_PCB_UNLOCK(unp);
		return (EALREADY);
	}
	unp->unp_flags |= UNP_BINDING;
	UNP_PCB_UNLOCK(unp);

	buf = malloc(namelen + 1, M_TEMP, M_WAITOK);
	bcopy(soun->sun_path, buf, namelen);
	buf[namelen] = 0;

restart:
	NDINIT_ATRIGHTS(&nd, CREATE, NOFOLLOW | LOCKPARENT | NOCACHE,
	    UIO_SYSSPACE, buf, fd, cap_rights_init_one(&rights, CAP_BINDAT));
/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	error = namei(&nd);
	if (error)
		goto error;
	vp = nd.ni_vp;
	if (vp != NULL || vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE_PNBUF(&nd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp != NULL) {
			vrele(vp);
			error = EADDRINUSE;
			goto error;
		}
		error = vn_start_write(NULL, &mp, V_XSLEEP | V_PCATCH);
		if (error)
			goto error;
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = (ACCESSPERMS & ~td->td_proc->p_pd->pd_cmask);
#ifdef MAC
	error = mac_vnode_check_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
#endif
	if (error == 0)
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	NDFREE_PNBUF(&nd);
	if (error) {
		VOP_VPUT_PAIR(nd.ni_dvp, NULL, true);
		vn_finished_write(mp);
		if (error == ERELOOKUP)
			goto restart;
		goto error;
	}
	vp = nd.ni_vp;
	ASSERT_VOP_ELOCKED(vp, "uipc_bind");
	soun = (struct sockaddr_un *)sodupsockaddr(nam, M_WAITOK);

	UNP_PCB_LOCK(unp);
	VOP_UNP_BIND(vp, unp);
	unp->unp_vnode = vp;
	unp->unp_addr = soun;
	unp->unp_flags &= ~UNP_BINDING;
	UNP_PCB_UNLOCK(unp);
	vref(vp);
	VOP_VPUT_PAIR(nd.ni_dvp, &vp, true);
	vn_finished_write(mp);
	free(buf, M_TEMP);
	return (0);

error:
	UNP_PCB_LOCK(unp);
	unp->unp_flags &= ~UNP_BINDING;
	UNP_PCB_UNLOCK(unp);
	free(buf, M_TEMP);
	return (error);
}

static int
uipc_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{

	return (uipc_bindat(AT_FDCWD, so, nam, td));
}

static int
uipc_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error;

	KASSERT(td == curthread, ("uipc_connect: td != curthread"));
	error = unp_connect(so, nam, td);
	return (error);
}

static int
uipc_connectat(int fd, struct socket *so, struct sockaddr *nam,
    struct thread *td)
{
	int error;

	KASSERT(td == curthread, ("uipc_connectat: td != curthread"));
	error = unp_connectat(fd, so, nam, td, false);
	return (error);
}

static void
uipc_close(struct socket *so)
{
	struct unpcb *unp, *unp2;
	struct vnode *vp = NULL;
	struct mtx *vplock;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_close: unp == NULL"));

	vplock = NULL;
	if ((vp = unp->unp_vnode) != NULL) {
		vplock = mtx_pool_find(mtxpool_sleep, vp);
		mtx_lock(vplock);
	}
	UNP_PCB_LOCK(unp);
	if (vp && unp->unp_vnode == NULL) {
		mtx_unlock(vplock);
		vp = NULL;
	}
	if (vp != NULL) {
		VOP_UNP_DETACH(vp);
		unp->unp_vnode = NULL;
	}
	if ((unp2 = unp_pcb_lock_peer(unp)) != NULL)
		unp_disconnect(unp, unp2);
	else
		UNP_PCB_UNLOCK(unp);
	if (vp) {
		mtx_unlock(vplock);
		vrele(vp);
	}
}

static int
uipc_connect2(struct socket *so1, struct socket *so2)
{
	struct unpcb *unp, *unp2;

	if (so1->so_type != so2->so_type)
		return (EPROTOTYPE);

	unp = so1->so_pcb;
	KASSERT(unp != NULL, ("uipc_connect2: unp == NULL"));
	unp2 = so2->so_pcb;
	KASSERT(unp2 != NULL, ("uipc_connect2: unp2 == NULL"));
	unp_pcb_lock_pair(unp, unp2);
	unp_connect2(so1, so2, PRU_CONNECT2);
	unp_pcb_unlock_pair(unp, unp2);

	return (0);
}

static void
uipc_detach(struct socket *so)
{
	struct unpcb *unp, *unp2;
	struct mtx *vplock;
	struct vnode *vp;
	int local_unp_rights;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_detach: unp == NULL"));

	vp = NULL;
	vplock = NULL;

	UNP_LINK_WLOCK();
	LIST_REMOVE(unp, unp_link);
	if (unp->unp_gcflag & UNPGC_DEAD)
		LIST_REMOVE(unp, unp_dead);
	unp->unp_gencnt = ++unp_gencnt;
	--unp_count;
	UNP_LINK_WUNLOCK();

	UNP_PCB_UNLOCK_ASSERT(unp);
 restart:
	if ((vp = unp->unp_vnode) != NULL) {
		vplock = mtx_pool_find(mtxpool_sleep, vp);
		mtx_lock(vplock);
	}
	UNP_PCB_LOCK(unp);
	if (unp->unp_vnode != vp && unp->unp_vnode != NULL) {
		if (vplock)
			mtx_unlock(vplock);
		UNP_PCB_UNLOCK(unp);
		goto restart;
	}
	if ((vp = unp->unp_vnode) != NULL) {
		VOP_UNP_DETACH(vp);
		unp->unp_vnode = NULL;
	}
	if ((unp2 = unp_pcb_lock_peer(unp)) != NULL)
		unp_disconnect(unp, unp2);
	else
		UNP_PCB_UNLOCK(unp);

	UNP_REF_LIST_LOCK();
	while (!LIST_EMPTY(&unp->unp_refs)) {
		struct unpcb *ref = LIST_FIRST(&unp->unp_refs);

		unp_pcb_hold(ref);
		UNP_REF_LIST_UNLOCK();

		MPASS(ref != unp);
		UNP_PCB_UNLOCK_ASSERT(ref);
		unp_drop(ref);
		UNP_REF_LIST_LOCK();
	}
	UNP_REF_LIST_UNLOCK();

	UNP_PCB_LOCK(unp);
	local_unp_rights = unp_rights;
	unp->unp_socket->so_pcb = NULL;
	unp->unp_socket = NULL;
	free(unp->unp_addr, M_SONAME);
	unp->unp_addr = NULL;
	if (!unp_pcb_rele(unp))
		UNP_PCB_UNLOCK(unp);
	if (vp) {
		mtx_unlock(vplock);
		vrele(vp);
	}
	if (local_unp_rights)
		taskqueue_enqueue_timeout(taskqueue_thread, &unp_gc_task, -1);

	switch (so->so_type) {
	case SOCK_DGRAM:
		/*
		 * Everything should have been unlinked/freed by unp_dispose()
		 * and/or unp_disconnect().
		 */
		MPASS(so->so_rcv.uxdg_peeked == NULL);
		MPASS(STAILQ_EMPTY(&so->so_rcv.uxdg_mb));
		MPASS(TAILQ_EMPTY(&so->so_rcv.uxdg_conns));
		MPASS(STAILQ_EMPTY(&so->so_snd.uxdg_mb));
	}
}

static int
uipc_disconnect(struct socket *so)
{
	struct unpcb *unp, *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_disconnect: unp == NULL"));

	UNP_PCB_LOCK(unp);
	if ((unp2 = unp_pcb_lock_peer(unp)) != NULL)
		unp_disconnect(unp, unp2);
	else
		UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_listen(struct socket *so, int backlog, struct thread *td)
{
	struct unpcb *unp;
	int error;

	MPASS(so->so_type != SOCK_DGRAM);

	/*
	 * Synchronize with concurrent connection attempts.
	 */
	error = 0;
	unp = sotounpcb(so);
	UNP_PCB_LOCK(unp);
	if (unp->unp_conn != NULL || (unp->unp_flags & UNP_CONNECTING) != 0)
		error = EINVAL;
	else if (unp->unp_vnode == NULL)
		error = EDESTADDRREQ;
	if (error != 0) {
		UNP_PCB_UNLOCK(unp);
		return (error);
	}

	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0) {
		cru2xt(td, &unp->unp_peercred);
		solisten_proto(so, backlog);
	}
	SOCK_UNLOCK(so);
	UNP_PCB_UNLOCK(unp);
	return (error);
}

static int
uipc_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp, *unp2;
	const struct sockaddr *sa;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_peeraddr: unp == NULL"));

	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);

	UNP_PCB_LOCK(unp);
	unp2 = unp_pcb_lock_peer(unp);
	if (unp2 != NULL) {
		if (unp2->unp_addr != NULL)
			sa = (struct sockaddr *) unp2->unp_addr;
		else
			sa = &sun_noname;
		bcopy(sa, *nam, sa->sa_len);
		UNP_PCB_UNLOCK(unp2);
	} else {
		sa = &sun_noname;
		bcopy(sa, *nam, sa->sa_len);
	}
	UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_rcvd(struct socket *so, int flags)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;
	u_int mbcnt, sbcc;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("%s: unp == NULL", __func__));
	KASSERT(so->so_type == SOCK_STREAM || so->so_type == SOCK_SEQPACKET,
	    ("%s: socktype %d", __func__, so->so_type));

	/*
	 * Adjust backpressure on sender and wakeup any waiting to write.
	 *
	 * The unp lock is acquired to maintain the validity of the unp_conn
	 * pointer; no lock on unp2 is required as unp2->unp_socket will be
	 * static as long as we don't permit unp2 to disconnect from unp,
	 * which is prevented by the lock on unp.  We cache values from
	 * so_rcv to avoid holding the so_rcv lock over the entire
	 * transaction on the remote so_snd.
	 */
	SOCKBUF_LOCK(&so->so_rcv);
	mbcnt = so->so_rcv.sb_mbcnt;
	sbcc = sbavail(&so->so_rcv);
	SOCKBUF_UNLOCK(&so->so_rcv);
	/*
	 * There is a benign race condition at this point.  If we're planning to
	 * clear SB_STOP, but uipc_send is called on the connected socket at
	 * this instant, it might add data to the sockbuf and set SB_STOP.  Then
	 * we would erroneously clear SB_STOP below, even though the sockbuf is
	 * full.  The race is benign because the only ill effect is to allow the
	 * sockbuf to exceed its size limit, and the size limits are not
	 * strictly guaranteed anyway.
	 */
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 == NULL) {
		UNP_PCB_UNLOCK(unp);
		return (0);
	}
	so2 = unp2->unp_socket;
	SOCKBUF_LOCK(&so2->so_snd);
	if (sbcc < so2->so_snd.sb_hiwat && mbcnt < so2->so_snd.sb_mbmax)
		so2->so_snd.sb_flags &= ~SB_STOP;
	sowwakeup_locked(so2);
	UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;
	u_int mbcnt, sbcc;
	int error;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("%s: unp == NULL", __func__));
	KASSERT(so->so_type == SOCK_STREAM || so->so_type == SOCK_SEQPACKET,
	    ("%s: socktype %d", __func__, so->so_type));

	error = 0;
	if (flags & PRUS_OOB) {
		error = EOPNOTSUPP;
		goto release;
	}
	if (control != NULL &&
	    (error = unp_internalize(&control, td, NULL, NULL, NULL)))
		goto release;

	unp2 = NULL;
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		if (nam != NULL) {
			if ((error = unp_connect(so, nam, td)) != 0)
				goto out;
		} else {
			error = ENOTCONN;
			goto out;
		}
	}

	UNP_PCB_LOCK(unp);
	if ((unp2 = unp_pcb_lock_peer(unp)) == NULL) {
		UNP_PCB_UNLOCK(unp);
		error = ENOTCONN;
		goto out;
	} else if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		unp_pcb_unlock_pair(unp, unp2);
		error = EPIPE;
		goto out;
	}
	UNP_PCB_UNLOCK(unp);
	if ((so2 = unp2->unp_socket) == NULL) {
		UNP_PCB_UNLOCK(unp2);
		error = ENOTCONN;
		goto out;
	}
	SOCKBUF_LOCK(&so2->so_rcv);
	if (unp2->unp_flags & UNP_WANTCRED_MASK) {
		/*
		 * Credentials are passed only once on SOCK_STREAM and
		 * SOCK_SEQPACKET (LOCAL_CREDS => WANTCRED_ONESHOT), or
		 * forever (LOCAL_CREDS_PERSISTENT => WANTCRED_ALWAYS).
		 */
		control = unp_addsockcred(td, control, unp2->unp_flags, NULL,
		    NULL, NULL);
		unp2->unp_flags &= ~UNP_WANTCRED_ONESHOT;
	}

	/*
	 * Send to paired receive port and wake up readers.  Don't
	 * check for space available in the receive buffer if we're
	 * attaching ancillary data; Unix domain sockets only check
	 * for space in the sending sockbuf, and that check is
	 * performed one level up the stack.  At that level we cannot
	 * precisely account for the amount of buffer space used
	 * (e.g., because control messages are not yet internalized).
	 */
	switch (so->so_type) {
	case SOCK_STREAM:
		if (control != NULL) {
			sbappendcontrol_locked(&so2->so_rcv, m,
			    control, flags);
			control = NULL;
		} else
			sbappend_locked(&so2->so_rcv, m, flags);
		break;

	case SOCK_SEQPACKET:
		if (sbappendaddr_nospacecheck_locked(&so2->so_rcv,
		    &sun_noname, m, control))
			control = NULL;
		break;
	}

	mbcnt = so2->so_rcv.sb_mbcnt;
	sbcc = sbavail(&so2->so_rcv);
	if (sbcc)
		sorwakeup_locked(so2);
	else
		SOCKBUF_UNLOCK(&so2->so_rcv);

	/*
	 * The PCB lock on unp2 protects the SB_STOP flag.  Without it,
	 * it would be possible for uipc_rcvd to be called at this
	 * point, drain the receiving sockbuf, clear SB_STOP, and then
	 * we would set SB_STOP below.  That could lead to an empty
	 * sockbuf having SB_STOP set
	 */
	SOCKBUF_LOCK(&so->so_snd);
	if (sbcc >= so->so_snd.sb_hiwat || mbcnt >= so->so_snd.sb_mbmax)
		so->so_snd.sb_flags |= SB_STOP;
	SOCKBUF_UNLOCK(&so->so_snd);
	UNP_PCB_UNLOCK(unp2);
	m = NULL;
out:
	/*
	 * PRUS_EOF is equivalent to pr_send followed by pr_shutdown.
	 */
	if (flags & PRUS_EOF) {
		UNP_PCB_LOCK(unp);
		socantsendmore(so);
		unp_shutdown(unp);
		UNP_PCB_UNLOCK(unp);
	}
	if (control != NULL && error != 0)
		unp_scan(control, unp_freerights);

release:
	if (control != NULL)
		m_freem(control);
	/*
	 * In case of PRUS_NOTREADY, uipc_ready() is responsible
	 * for freeing memory.
	 */   
	if (m != NULL && (flags & PRUS_NOTREADY) == 0)
		m_freem(m);
	return (error);
}

/* PF_UNIX/SOCK_DGRAM version of sbspace() */
static inline bool
uipc_dgram_sbspace(struct sockbuf *sb, u_int cc, u_int mbcnt)
{
	u_int bleft, mleft;

	/*
	 * Negative space may happen if send(2) is followed by
	 * setsockopt(SO_SNDBUF/SO_RCVBUF) that shrinks maximum.
	 */
	if (__predict_false(sb->sb_hiwat < sb->uxdg_cc ||
	    sb->sb_mbmax < sb->uxdg_mbcnt))
		return (false);

	if (__predict_false(sb->sb_state & SBS_CANTRCVMORE))
		return (false);

	bleft = sb->sb_hiwat - sb->uxdg_cc;
	mleft = sb->sb_mbmax - sb->uxdg_mbcnt;

	return (bleft >= cc && mleft >= mbcnt);
}

/*
 * PF_UNIX/SOCK_DGRAM send
 *
 * Allocate a record consisting of 3 mbufs in the sequence of
 * from -> control -> data and append it to the socket buffer.
 *
 * The first mbuf carries sender's name and is a pkthdr that stores
 * overall length of datagram, its memory consumption and control length.
 */
#define	ctllen	PH_loc.thirtytwo[1]
_Static_assert(offsetof(struct pkthdr, memlen) + sizeof(u_int) <=
    offsetof(struct pkthdr, ctllen), "unix/dgram can not store ctllen");
static int
uipc_sosend_dgram(struct socket *so, struct sockaddr *addr, struct uio *uio,
    struct mbuf *m, struct mbuf *c, int flags, struct thread *td)
{
	struct unpcb *unp, *unp2;
	const struct sockaddr *from;
	struct socket *so2;
	struct sockbuf *sb;
	struct mbuf *f, *clast;
	u_int cc, ctl, mbcnt;
	u_int dcc __diagused, dctl __diagused, dmbcnt __diagused;
	int error;

	MPASS((uio != NULL && m == NULL) || (m != NULL && uio == NULL));

	error = 0;
	f = NULL;
	ctl = 0;

	if (__predict_false(flags & MSG_OOB)) {
		error = EOPNOTSUPP;
		goto out;
	}
	if (m == NULL) {
		if (__predict_false(uio->uio_resid > unpdg_maxdgram)) {
			error = EMSGSIZE;
			goto out;
		}
		m = m_uiotombuf(uio, M_WAITOK, 0, max_hdr, M_PKTHDR);
		if (__predict_false(m == NULL)) {
			error = EFAULT;
			goto out;
		}
		f = m_gethdr(M_WAITOK, MT_SONAME);
		cc = m->m_pkthdr.len;
		mbcnt = MSIZE + m->m_pkthdr.memlen;
		if (c != NULL &&
		    (error = unp_internalize(&c, td, &clast, &ctl, &mbcnt)))
			goto out;
	} else {
		/* pr_sosend() with mbuf usually is a kernel thread. */

		M_ASSERTPKTHDR(m);
		if (__predict_false(c != NULL))
			panic("%s: control from a kernel thread", __func__);

		if (__predict_false(m->m_pkthdr.len > unpdg_maxdgram)) {
			error = EMSGSIZE;
			goto out;
		}
		if ((f = m_gethdr(M_NOWAIT, MT_SONAME)) == NULL) {
			error = ENOBUFS;
			goto out;
		}
		/* Condition the foreign mbuf to our standards. */
		m_clrprotoflags(m);
		m_tag_delete_chain(m, NULL);
		m->m_pkthdr.rcvif = NULL;
		m->m_pkthdr.flowid = 0;
		m->m_pkthdr.csum_flags = 0;
		m->m_pkthdr.fibnum = 0;
		m->m_pkthdr.rsstype = 0;

		cc = m->m_pkthdr.len;
		mbcnt = MSIZE;
		for (struct mbuf *mb = m; mb != NULL; mb = mb->m_next) {
			mbcnt += MSIZE;
			if (mb->m_flags & M_EXT)
				mbcnt += mb->m_ext.ext_size;
		}
	}

	unp = sotounpcb(so);
	MPASS(unp);

	/*
	 * XXXGL: would be cool to fully remove so_snd out of the equation
	 * and avoid this lock, which is not only extraneous, but also being
	 * released, thus still leaving possibility for a race.  We can easily
	 * handle SBS_CANTSENDMORE/SS_ISCONNECTED complement in unpcb, but it
	 * is more difficult to invent something to handle so_error.
	 */
	error = SOCK_IO_SEND_LOCK(so, SBLOCKWAIT(flags));
	if (error)
		goto out2;
	SOCK_SENDBUF_LOCK(so);
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		SOCK_SENDBUF_UNLOCK(so);
		error = EPIPE;
		goto out3;
	}
	if (so->so_error != 0) {
		error = so->so_error;
		so->so_error = 0;
		SOCK_SENDBUF_UNLOCK(so);
		goto out3;
	}
	if (((so->so_state & SS_ISCONNECTED) == 0) && addr == NULL) {
		SOCK_SENDBUF_UNLOCK(so);
		error = EDESTADDRREQ;
		goto out3;
	}
	SOCK_SENDBUF_UNLOCK(so);

	if (addr != NULL) {
		if ((error = unp_connectat(AT_FDCWD, so, addr, td, true)))
			goto out3;
		UNP_PCB_LOCK_ASSERT(unp);
		unp2 = unp->unp_conn;
		UNP_PCB_LOCK_ASSERT(unp2);
	} else {
		UNP_PCB_LOCK(unp);
		unp2 = unp_pcb_lock_peer(unp);
		if (unp2 == NULL) {
			UNP_PCB_UNLOCK(unp);
			error = ENOTCONN;
			goto out3;
		}
	}

	if (unp2->unp_flags & UNP_WANTCRED_MASK)
		c = unp_addsockcred(td, c, unp2->unp_flags, &clast, &ctl,
		    &mbcnt);
	if (unp->unp_addr != NULL)
		from = (struct sockaddr *)unp->unp_addr;
	else
		from = &sun_noname;
	f->m_len = from->sa_len;
	MPASS(from->sa_len <= MLEN);
	bcopy(from, mtod(f, void *), from->sa_len);
	ctl += f->m_len;

	/*
	 * Concatenate mbufs: from -> control -> data.
	 * Save overall cc and mbcnt in "from" mbuf.
	 */
	if (c != NULL) {
#ifdef INVARIANTS
		struct mbuf *mc;

		for (mc = c; mc->m_next != NULL; mc = mc->m_next);
		MPASS(mc == clast);
#endif
		f->m_next = c;
		clast->m_next = m;
		c = NULL;
	} else
		f->m_next = m;
	m = NULL;
#ifdef INVARIANTS
	dcc = dctl = dmbcnt = 0;
	for (struct mbuf *mb = f; mb != NULL; mb = mb->m_next) {
		if (mb->m_type == MT_DATA)
			dcc += mb->m_len;
		else
			dctl += mb->m_len;
		dmbcnt += MSIZE;
		if (mb->m_flags & M_EXT)
			dmbcnt += mb->m_ext.ext_size;
	}
	MPASS(dcc == cc);
	MPASS(dctl == ctl);
	MPASS(dmbcnt == mbcnt);
#endif
	f->m_pkthdr.len = cc + ctl;
	f->m_pkthdr.memlen = mbcnt;
	f->m_pkthdr.ctllen = ctl;

	/*
	 * Destination socket buffer selection.
	 *
	 * Unconnected sends, when !(so->so_state & SS_ISCONNECTED) and the
	 * destination address is supplied, create a temporary connection for
	 * the run time of the function (see call to unp_connectat() above and
	 * to unp_disconnect() below).  We distinguish them by condition of
	 * (addr != NULL).  We intentionally avoid adding 'bool connected' for
	 * that condition, since, again, through the run time of this code we
	 * are always connected.  For such "unconnected" sends, the destination
	 * buffer would be the receive buffer of destination socket so2.
	 *
	 * For connected sends, data lands on the send buffer of the sender's
	 * socket "so".  Then, if we just added the very first datagram
	 * on this send buffer, we need to add the send buffer on to the
	 * receiving socket's buffer list.  We put ourselves on top of the
	 * list.  Such logic gives infrequent senders priority over frequent
	 * senders.
	 *
	 * Note on byte count management. As long as event methods kevent(2),
	 * select(2) are not protocol specific (yet), we need to maintain
	 * meaningful values on the receive buffer.  So, the receive buffer
	 * would accumulate counters from all connected buffers potentially
	 * having sb_ccc > sb_hiwat or sb_mbcnt > sb_mbmax.
	 */
	so2 = unp2->unp_socket;
	sb = (addr == NULL) ? &so->so_snd : &so2->so_rcv;
	SOCK_RECVBUF_LOCK(so2);
	if (uipc_dgram_sbspace(sb, cc + ctl, mbcnt)) {
		if (addr == NULL && STAILQ_EMPTY(&sb->uxdg_mb))
			TAILQ_INSERT_HEAD(&so2->so_rcv.uxdg_conns, &so->so_snd,
			    uxdg_clist);
		STAILQ_INSERT_TAIL(&sb->uxdg_mb, f, m_stailqpkt);
		sb->uxdg_cc += cc + ctl;
		sb->uxdg_ctl += ctl;
		sb->uxdg_mbcnt += mbcnt;
		so2->so_rcv.sb_acc += cc + ctl;
		so2->so_rcv.sb_ccc += cc + ctl;
		so2->so_rcv.sb_ctl += ctl;
		so2->so_rcv.sb_mbcnt += mbcnt;
		sorwakeup_locked(so2);
		f = NULL;
	} else {
		soroverflow_locked(so2);
		error = ENOBUFS;
		if (f->m_next->m_type == MT_CONTROL)
			unp_scan(f->m_next, unp_freerights);
	}

	if (addr != NULL)
		unp_disconnect(unp, unp2);
	else
		unp_pcb_unlock_pair(unp, unp2);

	td->td_ru.ru_msgsnd++;

out3:
	SOCK_IO_SEND_UNLOCK(so);
out2:
	if (c)
		unp_scan(c, unp_freerights);
out:
	if (f)
		m_freem(f);
	if (c)
		m_freem(c);
	if (m)
		m_freem(m);

	return (error);
}

/*
 * PF_UNIX/SOCK_DGRAM receive with MSG_PEEK.
 * The mbuf has already been unlinked from the uxdg_mb of socket buffer
 * and needs to be linked onto uxdg_peeked of receive socket buffer.
 */
static int
uipc_peek_dgram(struct socket *so, struct mbuf *m, struct sockaddr **psa,
    struct uio *uio, struct mbuf **controlp, int *flagsp)
{
	ssize_t len = 0;
	int error;

	so->so_rcv.uxdg_peeked = m;
	so->so_rcv.uxdg_cc += m->m_pkthdr.len;
	so->so_rcv.uxdg_ctl += m->m_pkthdr.ctllen;
	so->so_rcv.uxdg_mbcnt += m->m_pkthdr.memlen;
	SOCK_RECVBUF_UNLOCK(so);

	KASSERT(m->m_type == MT_SONAME, ("m->m_type == %d", m->m_type));
	if (psa != NULL)
		*psa = sodupsockaddr(mtod(m, struct sockaddr *), M_WAITOK);

	m = m->m_next;
	KASSERT(m, ("%s: no data or control after soname", __func__));

	/*
	 * With MSG_PEEK the control isn't executed, just copied.
	 */
	while (m != NULL && m->m_type == MT_CONTROL) {
		if (controlp != NULL) {
			*controlp = m_copym(m, 0, m->m_len, M_WAITOK);
			controlp = &(*controlp)->m_next;
		}
		m = m->m_next;
	}
	KASSERT(m == NULL || m->m_type == MT_DATA,
	    ("%s: not MT_DATA mbuf %p", __func__, m));
	while (m != NULL && uio->uio_resid > 0) {
		len = uio->uio_resid;
		if (len > m->m_len)
			len = m->m_len;
		error = uiomove(mtod(m, char *), (int)len, uio);
		if (error) {
			SOCK_IO_RECV_UNLOCK(so);
			return (error);
		}
		if (len == m->m_len)
			m = m->m_next;
	}
	SOCK_IO_RECV_UNLOCK(so);

	if (flagsp != NULL) {
		if (m != NULL) {
			if (*flagsp & MSG_TRUNC) {
				/* Report real length of the packet */
				uio->uio_resid -= m_length(m, NULL) - len;
			}
			*flagsp |= MSG_TRUNC;
		} else
			*flagsp &= ~MSG_TRUNC;
	}

	return (0);
}

/*
 * PF_UNIX/SOCK_DGRAM receive
 */
static int
uipc_soreceive_dgram(struct socket *so, struct sockaddr **psa, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct sockbuf *sb = NULL;
	struct mbuf *m;
	int flags, error;
	ssize_t len = 0;
	bool nonblock;

	MPASS(mp0 == NULL);

	if (psa != NULL)
		*psa = NULL;
	if (controlp != NULL)
		*controlp = NULL;

	flags = flagsp != NULL ? *flagsp : 0;
	nonblock = (so->so_state & SS_NBIO) ||
	    (flags & (MSG_DONTWAIT | MSG_NBIO));

	error = SOCK_IO_RECV_LOCK(so, SBLOCKWAIT(flags));
	if (__predict_false(error))
		return (error);

	/*
	 * Loop blocking while waiting for a datagram.  Prioritize connected
	 * peers over unconnected sends.  Set sb to selected socket buffer
	 * containing an mbuf on exit from the wait loop.  A datagram that
	 * had already been peeked at has top priority.
	 */
	SOCK_RECVBUF_LOCK(so);
	while ((m = so->so_rcv.uxdg_peeked) == NULL &&
	    (sb = TAILQ_FIRST(&so->so_rcv.uxdg_conns)) == NULL &&
	    (m = STAILQ_FIRST(&so->so_rcv.uxdg_mb)) == NULL) {
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			SOCK_RECVBUF_UNLOCK(so);
			SOCK_IO_RECV_UNLOCK(so);
			return (error);
		}
		if (so->so_rcv.sb_state & SBS_CANTRCVMORE ||
		    uio->uio_resid == 0) {
			SOCK_RECVBUF_UNLOCK(so);
			SOCK_IO_RECV_UNLOCK(so);
			return (0);
		}
		if (nonblock) {
			SOCK_RECVBUF_UNLOCK(so);
			SOCK_IO_RECV_UNLOCK(so);
			return (EWOULDBLOCK);
		}
		error = sbwait(so, SO_RCV);
		if (error) {
			SOCK_RECVBUF_UNLOCK(so);
			SOCK_IO_RECV_UNLOCK(so);
			return (error);
		}
	}

	if (sb == NULL)
		sb = &so->so_rcv;
	else if (m == NULL)
		m = STAILQ_FIRST(&sb->uxdg_mb);
	else
		MPASS(m == so->so_rcv.uxdg_peeked);

	MPASS(sb->uxdg_cc > 0);
	M_ASSERTPKTHDR(m);
	KASSERT(m->m_type == MT_SONAME, ("m->m_type == %d", m->m_type));

	if (uio->uio_td)
		uio->uio_td->td_ru.ru_msgrcv++;

	if (__predict_true(m != so->so_rcv.uxdg_peeked)) {
		STAILQ_REMOVE_HEAD(&sb->uxdg_mb, m_stailqpkt);
		if (STAILQ_EMPTY(&sb->uxdg_mb) && sb != &so->so_rcv)
			TAILQ_REMOVE(&so->so_rcv.uxdg_conns, sb, uxdg_clist);
	} else
		so->so_rcv.uxdg_peeked = NULL;

	sb->uxdg_cc -= m->m_pkthdr.len;
	sb->uxdg_ctl -= m->m_pkthdr.ctllen;
	sb->uxdg_mbcnt -= m->m_pkthdr.memlen;

	if (__predict_false(flags & MSG_PEEK))
		return (uipc_peek_dgram(so, m, psa, uio, controlp, flagsp));

	so->so_rcv.sb_acc -= m->m_pkthdr.len;
	so->so_rcv.sb_ccc -= m->m_pkthdr.len;
	so->so_rcv.sb_ctl -= m->m_pkthdr.ctllen;
	so->so_rcv.sb_mbcnt -= m->m_pkthdr.memlen;
	SOCK_RECVBUF_UNLOCK(so);

	if (psa != NULL)
		*psa = sodupsockaddr(mtod(m, struct sockaddr *), M_WAITOK);
	m = m_free(m);
	KASSERT(m, ("%s: no data or control after soname", __func__));

	/*
	 * Packet to copyout() is now in 'm' and it is disconnected from the
	 * queue.
	 *
	 * Process one or more MT_CONTROL mbufs present before any data mbufs
	 * in the first mbuf chain on the socket buffer.  We call into the
	 * unp_externalize() to perform externalization (or freeing if
	 * controlp == NULL). In some cases there can be only MT_CONTROL mbufs
	 * without MT_DATA mbufs.
	 */
	while (m != NULL && m->m_type == MT_CONTROL) {
		struct mbuf *cm;

		/* XXXGL: unp_externalize() is also dom_externalize() KBI and
		 * it frees whole chain, so we must disconnect the mbuf.
		 */
		cm = m; m = m->m_next; cm->m_next = NULL;
		error = unp_externalize(cm, controlp, flags);
		if (error != 0) {
			SOCK_IO_RECV_UNLOCK(so);
			unp_scan(m, unp_freerights);
			m_freem(m);
			return (error);
		}
		if (controlp != NULL) {
			while (*controlp != NULL)
				controlp = &(*controlp)->m_next;
		}
	}
	KASSERT(m == NULL || m->m_type == MT_DATA,
	    ("%s: not MT_DATA mbuf %p", __func__, m));
	while (m != NULL && uio->uio_resid > 0) {
		len = uio->uio_resid;
		if (len > m->m_len)
			len = m->m_len;
		error = uiomove(mtod(m, char *), (int)len, uio);
		if (error) {
			SOCK_IO_RECV_UNLOCK(so);
			m_freem(m);
			return (error);
		}
		if (len == m->m_len)
			m = m_free(m);
		else {
			m->m_data += len;
			m->m_len -= len;
		}
	}
	SOCK_IO_RECV_UNLOCK(so);

	if (m != NULL) {
		if (flagsp != NULL) {
			if (flags & MSG_TRUNC) {
				/* Report real length of the packet */
				uio->uio_resid -= m_length(m, NULL);
			}
			*flagsp |= MSG_TRUNC;
		}
		m_freem(m);
	} else if (flagsp != NULL)
		*flagsp &= ~MSG_TRUNC;

	return (0);
}

static bool
uipc_ready_scan(struct socket *so, struct mbuf *m, int count, int *errorp)
{
	struct mbuf *mb, *n;
	struct sockbuf *sb;

	SOCK_LOCK(so);
	if (SOLISTENING(so)) {
		SOCK_UNLOCK(so);
		return (false);
	}
	mb = NULL;
	sb = &so->so_rcv;
	SOCKBUF_LOCK(sb);
	if (sb->sb_fnrdy != NULL) {
		for (mb = sb->sb_mb, n = mb->m_nextpkt; mb != NULL;) {
			if (mb == m) {
				*errorp = sbready(sb, m, count);
				break;
			}
			mb = mb->m_next;
			if (mb == NULL) {
				mb = n;
				if (mb != NULL)
					n = mb->m_nextpkt;
			}
		}
	}
	SOCKBUF_UNLOCK(sb);
	SOCK_UNLOCK(so);
	return (mb != NULL);
}

static int
uipc_ready(struct socket *so, struct mbuf *m, int count)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;
	int error, i;

	unp = sotounpcb(so);

	KASSERT(so->so_type == SOCK_STREAM,
	    ("%s: unexpected socket type for %p", __func__, so));

	UNP_PCB_LOCK(unp);
	if ((unp2 = unp_pcb_lock_peer(unp)) != NULL) {
		UNP_PCB_UNLOCK(unp);
		so2 = unp2->unp_socket;
		SOCKBUF_LOCK(&so2->so_rcv);
		if ((error = sbready(&so2->so_rcv, m, count)) == 0)
			sorwakeup_locked(so2);
		else
			SOCKBUF_UNLOCK(&so2->so_rcv);
		UNP_PCB_UNLOCK(unp2);
		return (error);
	}
	UNP_PCB_UNLOCK(unp);

	/*
	 * The receiving socket has been disconnected, but may still be valid.
	 * In this case, the now-ready mbufs are still present in its socket
	 * buffer, so perform an exhaustive search before giving up and freeing
	 * the mbufs.
	 */
	UNP_LINK_RLOCK();
	LIST_FOREACH(unp, &unp_shead, unp_link) {
		if (uipc_ready_scan(unp->unp_socket, m, count, &error))
			break;
	}
	UNP_LINK_RUNLOCK();

	if (unp == NULL) {
		for (i = 0; i < count; i++)
			m = m_free(m);
		error = ECONNRESET;
	}
	return (error);
}

static int
uipc_sense(struct socket *so, struct stat *sb)
{
	struct unpcb *unp;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_sense: unp == NULL"));

	sb->st_blksize = so->so_snd.sb_hiwat;
	sb->st_dev = NODEV;
	sb->st_ino = unp->unp_ino;
	return (0);
}

static int
uipc_shutdown(struct socket *so)
{
	struct unpcb *unp;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_shutdown: unp == NULL"));

	UNP_PCB_LOCK(unp);
	socantsendmore(so);
	unp_shutdown(unp);
	UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct unpcb *unp;
	const struct sockaddr *sa;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_sockaddr: unp == NULL"));

	*nam = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	UNP_PCB_LOCK(unp);
	if (unp->unp_addr != NULL)
		sa = (struct sockaddr *) unp->unp_addr;
	else
		sa = &sun_noname;
	bcopy(sa, *nam, sa->sa_len);
	UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct unpcb *unp;
	struct xucred xu;
	int error, optval;

	if (sopt->sopt_level != SOL_LOCAL)
		return (EINVAL);

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_ctloutput: unp == NULL"));
	error = 0;
	switch (sopt->sopt_dir) {
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case LOCAL_PEERCRED:
			UNP_PCB_LOCK(unp);
			if (unp->unp_flags & UNP_HAVEPC)
				xu = unp->unp_peercred;
			else {
				if (so->so_type == SOCK_STREAM)
					error = ENOTCONN;
				else
					error = EINVAL;
			}
			UNP_PCB_UNLOCK(unp);
			if (error == 0)
				error = sooptcopyout(sopt, &xu, sizeof(xu));
			break;

		case LOCAL_CREDS:
			/* Unlocked read. */
			optval = unp->unp_flags & UNP_WANTCRED_ONESHOT ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;

		case LOCAL_CREDS_PERSISTENT:
			/* Unlocked read. */
			optval = unp->unp_flags & UNP_WANTCRED_ALWAYS ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;

		case LOCAL_CONNWAIT:
			/* Unlocked read. */
			optval = unp->unp_flags & UNP_CONNWAIT ? 1 : 0;
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;

		default:
			error = EOPNOTSUPP;
			break;
		}
		break;

	case SOPT_SET:
		switch (sopt->sopt_name) {
		case LOCAL_CREDS:
		case LOCAL_CREDS_PERSISTENT:
		case LOCAL_CONNWAIT:
			error = sooptcopyin(sopt, &optval, sizeof(optval),
					    sizeof(optval));
			if (error)
				break;

#define	OPTSET(bit, exclusive) do {					\
	UNP_PCB_LOCK(unp);						\
	if (optval) {							\
		if ((unp->unp_flags & (exclusive)) != 0) {		\
			UNP_PCB_UNLOCK(unp);				\
			error = EINVAL;					\
			break;						\
		}							\
		unp->unp_flags |= (bit);				\
	} else								\
		unp->unp_flags &= ~(bit);				\
	UNP_PCB_UNLOCK(unp);						\
} while (0)

			switch (sopt->sopt_name) {
			case LOCAL_CREDS:
				OPTSET(UNP_WANTCRED_ONESHOT, UNP_WANTCRED_ALWAYS);
				break;

			case LOCAL_CREDS_PERSISTENT:
				OPTSET(UNP_WANTCRED_ALWAYS, UNP_WANTCRED_ONESHOT);
				break;

			case LOCAL_CONNWAIT:
				OPTSET(UNP_CONNWAIT, 0);
				break;

			default:
				break;
			}
			break;
#undef	OPTSET
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static int
unp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{

	return (unp_connectat(AT_FDCWD, so, nam, td, false));
}

static int
unp_connectat(int fd, struct socket *so, struct sockaddr *nam,
    struct thread *td, bool return_locked)
{
	struct mtx *vplock;
	struct sockaddr_un *soun;
	struct vnode *vp;
	struct socket *so2;
	struct unpcb *unp, *unp2, *unp3;
	struct nameidata nd;
	char buf[SOCK_MAXADDRLEN];
	struct sockaddr *sa;
	cap_rights_t rights;
	int error, len;
	bool connreq;

	if (nam->sa_family != AF_UNIX)
		return (EAFNOSUPPORT);
	if (nam->sa_len > sizeof(struct sockaddr_un))
		return (EINVAL);
	len = nam->sa_len - offsetof(struct sockaddr_un, sun_path);
	if (len <= 0)
		return (EINVAL);
	soun = (struct sockaddr_un *)nam;
	bcopy(soun->sun_path, buf, len);
	buf[len] = 0;

	error = 0;
	unp = sotounpcb(so);
	UNP_PCB_LOCK(unp);
	for (;;) {
		/*
		 * Wait for connection state to stabilize.  If a connection
		 * already exists, give up.  For datagram sockets, which permit
		 * multiple consecutive connect(2) calls, upper layers are
		 * responsible for disconnecting in advance of a subsequent
		 * connect(2), but this is not synchronized with PCB connection
		 * state.
		 *
		 * Also make sure that no threads are currently attempting to
		 * lock the peer socket, to ensure that unp_conn cannot
		 * transition between two valid sockets while locks are dropped.
		 */
		if (SOLISTENING(so))
			error = EOPNOTSUPP;
		else if (unp->unp_conn != NULL)
			error = EISCONN;
		else if ((unp->unp_flags & UNP_CONNECTING) != 0) {
			error = EALREADY;
		}
		if (error != 0) {
			UNP_PCB_UNLOCK(unp);
			return (error);
		}
		if (unp->unp_pairbusy > 0) {
			unp->unp_flags |= UNP_WAITING;
			mtx_sleep(unp, UNP_PCB_LOCKPTR(unp), 0, "unpeer", 0);
			continue;
		}
		break;
	}
	unp->unp_flags |= UNP_CONNECTING;
	UNP_PCB_UNLOCK(unp);

	connreq = (so->so_proto->pr_flags & PR_CONNREQUIRED) != 0;
	if (connreq)
		sa = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	else
		sa = NULL;
	NDINIT_ATRIGHTS(&nd, LOOKUP, FOLLOW | LOCKSHARED | LOCKLEAF,
	    UIO_SYSSPACE, buf, fd, cap_rights_init_one(&rights, CAP_CONNECTAT));
	error = namei(&nd);
	if (error)
		vp = NULL;
	else
		vp = nd.ni_vp;
	ASSERT_VOP_LOCKED(vp, "unp_connect");
	if (error)
		goto bad;
	NDFREE_PNBUF(&nd);

	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
#ifdef MAC
	error = mac_vnode_check_open(td->td_ucred, vp, VWRITE | VREAD);
	if (error)
		goto bad;
#endif
	error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td);
	if (error)
		goto bad;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect: unp == NULL"));

	vplock = mtx_pool_find(mtxpool_sleep, vp);
	mtx_lock(vplock);
	VOP_UNP_CONNECT(vp, &unp2);
	if (unp2 == NULL) {
		error = ECONNREFUSED;
		goto bad2;
	}
	so2 = unp2->unp_socket;
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad2;
	}
	if (connreq) {
		if (SOLISTENING(so2)) {
			CURVNET_SET(so2->so_vnet);
			so2 = sonewconn(so2, 0);
			CURVNET_RESTORE();
		} else
			so2 = NULL;
		if (so2 == NULL) {
			error = ECONNREFUSED;
			goto bad2;
		}
		unp3 = sotounpcb(so2);
		unp_pcb_lock_pair(unp2, unp3);
		if (unp2->unp_addr != NULL) {
			bcopy(unp2->unp_addr, sa, unp2->unp_addr->sun_len);
			unp3->unp_addr = (struct sockaddr_un *) sa;
			sa = NULL;
		}

		unp_copy_peercred(td, unp3, unp, unp2);

		UNP_PCB_UNLOCK(unp2);
		unp2 = unp3;

		/*
		 * It is safe to block on the PCB lock here since unp2 is
		 * nascent and cannot be connected to any other sockets.
		 */
		UNP_PCB_LOCK(unp);
#ifdef MAC
		mac_socketpeer_set_from_socket(so, so2);
		mac_socketpeer_set_from_socket(so2, so);
#endif
	} else {
		unp_pcb_lock_pair(unp, unp2);
	}
	KASSERT(unp2 != NULL && so2 != NULL && unp2->unp_socket == so2 &&
	    sotounpcb(so2) == unp2,
	    ("%s: unp2 %p so2 %p", __func__, unp2, so2));
	unp_connect2(so, so2, PRU_CONNECT);
	KASSERT((unp->unp_flags & UNP_CONNECTING) != 0,
	    ("%s: unp %p has UNP_CONNECTING clear", __func__, unp));
	unp->unp_flags &= ~UNP_CONNECTING;
	if (!return_locked)
		unp_pcb_unlock_pair(unp, unp2);
bad2:
	mtx_unlock(vplock);
bad:
	if (vp != NULL) {
		/*
		 * If we are returning locked (called via uipc_sosend_dgram()),
		 * we need to be sure that vput() won't sleep.  This is
		 * guaranteed by VOP_UNP_CONNECT() call above and unp2 lock.
		 * SOCK_STREAM/SEQPACKET can't request return_locked (yet).
		 */
		MPASS(!(return_locked && connreq));
		vput(vp);
	}
	free(sa, M_SONAME);
	if (__predict_false(error)) {
		UNP_PCB_LOCK(unp);
		KASSERT((unp->unp_flags & UNP_CONNECTING) != 0,
		    ("%s: unp %p has UNP_CONNECTING clear", __func__, unp));
		unp->unp_flags &= ~UNP_CONNECTING;
		UNP_PCB_UNLOCK(unp);
	}
	return (error);
}

/*
 * Set socket peer credentials at connection time.
 *
 * The client's PCB credentials are copied from its process structure.  The
 * server's PCB credentials are copied from the socket on which it called
 * listen(2).  uipc_listen cached that process's credentials at the time.
 */
void
unp_copy_peercred(struct thread *td, struct unpcb *client_unp,
    struct unpcb *server_unp, struct unpcb *listen_unp)
{
	cru2xt(td, &client_unp->unp_peercred);
	client_unp->unp_flags |= UNP_HAVEPC;

	memcpy(&server_unp->unp_peercred, &listen_unp->unp_peercred,
	    sizeof(server_unp->unp_peercred));
	server_unp->unp_flags |= UNP_HAVEPC;
	client_unp->unp_flags |= (listen_unp->unp_flags & UNP_WANTCRED_MASK);
}

static void
unp_connect2(struct socket *so, struct socket *so2, conn2_how req)
{
	struct unpcb *unp;
	struct unpcb *unp2;

	MPASS(so2->so_type == so->so_type);
	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect2: unp == NULL"));
	unp2 = sotounpcb(so2);
	KASSERT(unp2 != NULL, ("unp_connect2: unp2 == NULL"));

	UNP_PCB_LOCK_ASSERT(unp);
	UNP_PCB_LOCK_ASSERT(unp2);
	KASSERT(unp->unp_conn == NULL,
	    ("%s: socket %p is already connected", __func__, unp));

	unp->unp_conn = unp2;
	unp_pcb_hold(unp2);
	unp_pcb_hold(unp);
	switch (so->so_type) {
	case SOCK_DGRAM:
		UNP_REF_LIST_LOCK();
		LIST_INSERT_HEAD(&unp2->unp_refs, unp, unp_reflink);
		UNP_REF_LIST_UNLOCK();
		soisconnected(so);
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		KASSERT(unp2->unp_conn == NULL,
		    ("%s: socket %p is already connected", __func__, unp2));
		unp2->unp_conn = unp;
		if (req == PRU_CONNECT &&
		    ((unp->unp_flags | unp2->unp_flags) & UNP_CONNWAIT))
			soisconnecting(so);
		else
			soisconnected(so);
		soisconnected(so2);
		break;

	default:
		panic("unp_connect2");
	}
}

static void
unp_disconnect(struct unpcb *unp, struct unpcb *unp2)
{
	struct socket *so, *so2;
	struct mbuf *m = NULL;
#ifdef INVARIANTS
	struct unpcb *unptmp;
#endif

	UNP_PCB_LOCK_ASSERT(unp);
	UNP_PCB_LOCK_ASSERT(unp2);
	KASSERT(unp->unp_conn == unp2,
	    ("%s: unpcb %p is not connected to %p", __func__, unp, unp2));

	unp->unp_conn = NULL;
	so = unp->unp_socket;
	so2 = unp2->unp_socket;
	switch (unp->unp_socket->so_type) {
	case SOCK_DGRAM:
		/*
		 * Remove our send socket buffer from the peer's receive buffer.
		 * Move the data to the receive buffer only if it is empty.
		 * This is a protection against a scenario where a peer
		 * connects, floods and disconnects, effectively blocking
		 * sendto() from unconnected sockets.
		 */
		SOCK_RECVBUF_LOCK(so2);
		if (!STAILQ_EMPTY(&so->so_snd.uxdg_mb)) {
			TAILQ_REMOVE(&so2->so_rcv.uxdg_conns, &so->so_snd,
			    uxdg_clist);
			if (__predict_true((so2->so_rcv.sb_state &
			    SBS_CANTRCVMORE) == 0) &&
			    STAILQ_EMPTY(&so2->so_rcv.uxdg_mb)) {
				STAILQ_CONCAT(&so2->so_rcv.uxdg_mb,
				    &so->so_snd.uxdg_mb);
				so2->so_rcv.uxdg_cc += so->so_snd.uxdg_cc;
				so2->so_rcv.uxdg_ctl += so->so_snd.uxdg_ctl;
				so2->so_rcv.uxdg_mbcnt += so->so_snd.uxdg_mbcnt;
			} else {
				m = STAILQ_FIRST(&so->so_snd.uxdg_mb);
				STAILQ_INIT(&so->so_snd.uxdg_mb);
				so2->so_rcv.sb_acc -= so->so_snd.uxdg_cc;
				so2->so_rcv.sb_ccc -= so->so_snd.uxdg_cc;
				so2->so_rcv.sb_ctl -= so->so_snd.uxdg_ctl;
				so2->so_rcv.sb_mbcnt -= so->so_snd.uxdg_mbcnt;
			}
			/* Note: so may reconnect. */
			so->so_snd.uxdg_cc = 0;
			so->so_snd.uxdg_ctl = 0;
			so->so_snd.uxdg_mbcnt = 0;
		}
		SOCK_RECVBUF_UNLOCK(so2);
		UNP_REF_LIST_LOCK();
#ifdef INVARIANTS
		LIST_FOREACH(unptmp, &unp2->unp_refs, unp_reflink) {
			if (unptmp == unp)
				break;
		}
		KASSERT(unptmp != NULL,
		    ("%s: %p not found in reflist of %p", __func__, unp, unp2));
#endif
		LIST_REMOVE(unp, unp_reflink);
		UNP_REF_LIST_UNLOCK();
		if (so) {
			SOCK_LOCK(so);
			so->so_state &= ~SS_ISCONNECTED;
			SOCK_UNLOCK(so);
		}
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		if (so)
			soisdisconnected(so);
		MPASS(unp2->unp_conn == unp);
		unp2->unp_conn = NULL;
		if (so2)
			soisdisconnected(so2);
		break;
	}

	if (unp == unp2) {
		unp_pcb_rele_notlast(unp);
		if (!unp_pcb_rele(unp))
			UNP_PCB_UNLOCK(unp);
	} else {
		if (!unp_pcb_rele(unp))
			UNP_PCB_UNLOCK(unp);
		if (!unp_pcb_rele(unp2))
			UNP_PCB_UNLOCK(unp2);
	}

	if (m != NULL) {
		unp_scan(m, unp_freerights);
		m_freem(m);
	}
}

/*
 * unp_pcblist() walks the global list of struct unpcb's to generate a
 * pointer list, bumping the refcount on each unpcb.  It then copies them out
 * sequentially, validating the generation number on each to see if it has
 * been detached.  All of this is necessary because copyout() may sleep on
 * disk I/O.
 */
static int
unp_pcblist(SYSCTL_HANDLER_ARGS)
{
	struct unpcb *unp, **unp_list;
	unp_gen_t gencnt;
	struct xunpgen *xug;
	struct unp_head *head;
	struct xunpcb *xu;
	u_int i;
	int error, n;

	switch ((intptr_t)arg1) {
	case SOCK_STREAM:
		head = &unp_shead;
		break;

	case SOCK_DGRAM:
		head = &unp_dhead;
		break;

	case SOCK_SEQPACKET:
		head = &unp_sphead;
		break;

	default:
		panic("unp_pcblist: arg1 %d", (int)(intptr_t)arg1);
	}

	/*
	 * The process of preparing the PCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == NULL) {
		n = unp_count;
		req->oldidx = 2 * (sizeof *xug)
			+ (n + n/8) * sizeof(struct xunpcb);
		return (0);
	}

	if (req->newptr != NULL)
		return (EPERM);

	/*
	 * OK, now we're committed to doing something.
	 */
	xug = malloc(sizeof(*xug), M_TEMP, M_WAITOK | M_ZERO);
	UNP_LINK_RLOCK();
	gencnt = unp_gencnt;
	n = unp_count;
	UNP_LINK_RUNLOCK();

	xug->xug_len = sizeof *xug;
	xug->xug_count = n;
	xug->xug_gen = gencnt;
	xug->xug_sogen = so_gencnt;
	error = SYSCTL_OUT(req, xug, sizeof *xug);
	if (error) {
		free(xug, M_TEMP);
		return (error);
	}

	unp_list = malloc(n * sizeof *unp_list, M_TEMP, M_WAITOK);

	UNP_LINK_RLOCK();
	for (unp = LIST_FIRST(head), i = 0; unp && i < n;
	     unp = LIST_NEXT(unp, unp_link)) {
		UNP_PCB_LOCK(unp);
		if (unp->unp_gencnt <= gencnt) {
			if (cr_cansee(req->td->td_ucred,
			    unp->unp_socket->so_cred)) {
				UNP_PCB_UNLOCK(unp);
				continue;
			}
			unp_list[i++] = unp;
			unp_pcb_hold(unp);
		}
		UNP_PCB_UNLOCK(unp);
	}
	UNP_LINK_RUNLOCK();
	n = i;			/* In case we lost some during malloc. */

	error = 0;
	xu = malloc(sizeof(*xu), M_TEMP, M_WAITOK | M_ZERO);
	for (i = 0; i < n; i++) {
		unp = unp_list[i];
		UNP_PCB_LOCK(unp);
		if (unp_pcb_rele(unp))
			continue;

		if (unp->unp_gencnt <= gencnt) {
			xu->xu_len = sizeof *xu;
			xu->xu_unpp = (uintptr_t)unp;
			/*
			 * XXX - need more locking here to protect against
			 * connect/disconnect races for SMP.
			 */
			if (unp->unp_addr != NULL)
				bcopy(unp->unp_addr, &xu->xu_addr,
				      unp->unp_addr->sun_len);
			else
				bzero(&xu->xu_addr, sizeof(xu->xu_addr));
			if (unp->unp_conn != NULL &&
			    unp->unp_conn->unp_addr != NULL)
				bcopy(unp->unp_conn->unp_addr,
				      &xu->xu_caddr,
				      unp->unp_conn->unp_addr->sun_len);
			else
				bzero(&xu->xu_caddr, sizeof(xu->xu_caddr));
			xu->unp_vnode = (uintptr_t)unp->unp_vnode;
			xu->unp_conn = (uintptr_t)unp->unp_conn;
			xu->xu_firstref = (uintptr_t)LIST_FIRST(&unp->unp_refs);
			xu->xu_nextref = (uintptr_t)LIST_NEXT(unp, unp_reflink);
			xu->unp_gencnt = unp->unp_gencnt;
			sotoxsocket(unp->unp_socket, &xu->xu_socket);
			UNP_PCB_UNLOCK(unp);
			error = SYSCTL_OUT(req, xu, sizeof *xu);
		} else {
			UNP_PCB_UNLOCK(unp);
		}
	}
	free(xu, M_TEMP);
	if (!error) {
		/*
		 * Give the user an updated idea of our state.  If the
		 * generation differs from what we told her before, she knows
		 * that something happened while we were processing this
		 * request, and it might be necessary to retry.
		 */
		xug->xug_gen = unp_gencnt;
		xug->xug_sogen = so_gencnt;
		xug->xug_count = unp_count;
		error = SYSCTL_OUT(req, xug, sizeof *xug);
	}
	free(unp_list, M_TEMP);
	free(xug, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_local_dgram, OID_AUTO, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE,
    (void *)(intptr_t)SOCK_DGRAM, 0, unp_pcblist, "S,xunpcb",
    "List of active local datagram sockets");
SYSCTL_PROC(_net_local_stream, OID_AUTO, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE,
    (void *)(intptr_t)SOCK_STREAM, 0, unp_pcblist, "S,xunpcb",
    "List of active local stream sockets");
SYSCTL_PROC(_net_local_seqpacket, OID_AUTO, pcblist,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE,
    (void *)(intptr_t)SOCK_SEQPACKET, 0, unp_pcblist, "S,xunpcb",
    "List of active local seqpacket sockets");

static void
unp_shutdown(struct unpcb *unp)
{
	struct unpcb *unp2;
	struct socket *so;

	UNP_PCB_LOCK_ASSERT(unp);

	unp2 = unp->unp_conn;
	if ((unp->unp_socket->so_type == SOCK_STREAM ||
	    (unp->unp_socket->so_type == SOCK_SEQPACKET)) && unp2 != NULL) {
		so = unp2->unp_socket;
		if (so != NULL)
			socantrcvmore(so);
	}
}

static void
unp_drop(struct unpcb *unp)
{
	struct socket *so;
	struct unpcb *unp2;

	/*
	 * Regardless of whether the socket's peer dropped the connection
	 * with this socket by aborting or disconnecting, POSIX requires
	 * that ECONNRESET is returned.
	 */

	UNP_PCB_LOCK(unp);
	so = unp->unp_socket;
	if (so)
		so->so_error = ECONNRESET;
	if ((unp2 = unp_pcb_lock_peer(unp)) != NULL) {
		/* Last reference dropped in unp_disconnect(). */
		unp_pcb_rele_notlast(unp);
		unp_disconnect(unp, unp2);
	} else if (!unp_pcb_rele(unp)) {
		UNP_PCB_UNLOCK(unp);
	}
}

static void
unp_freerights(struct filedescent **fdep, int fdcount)
{
	struct file *fp;
	int i;

	KASSERT(fdcount > 0, ("%s: fdcount %d", __func__, fdcount));

	for (i = 0; i < fdcount; i++) {
		fp = fdep[i]->fde_file;
		filecaps_free(&fdep[i]->fde_caps);
		unp_discard(fp);
	}
	free(fdep[0], M_FILECAPS);
}

static int
unp_externalize(struct mbuf *control, struct mbuf **controlp, int flags)
{
	struct thread *td = curthread;		/* XXX */
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	int i;
	int *fdp;
	struct filedesc *fdesc = td->td_proc->p_fd;
	struct filedescent **fdep;
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error, newfds;
	u_int newlen;

	UNP_LINK_UNLOCK_ASSERT();

	error = 0;
	if (controlp != NULL) /* controlp == NULL => free control messages */
		*controlp = NULL;
	while (cm != NULL) {
		MPASS(clen >= sizeof(*cm) && clen >= cm->cmsg_len);

		data = CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;
		if (cm->cmsg_level == SOL_SOCKET
		    && cm->cmsg_type == SCM_RIGHTS) {
			newfds = datalen / sizeof(*fdep);
			if (newfds == 0)
				goto next;
			fdep = data;

			/* If we're not outputting the descriptors free them. */
			if (error || controlp == NULL) {
				unp_freerights(fdep, newfds);
				goto next;
			}
			FILEDESC_XLOCK(fdesc);

			/*
			 * Now change each pointer to an fd in the global
			 * table to an integer that is the index to the local
			 * fd table entry that we set up to point to the
			 * global one we are transferring.
			 */
			newlen = newfds * sizeof(int);
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET, M_WAITOK);

			fdp = (int *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			if ((error = fdallocn(td, 0, fdp, newfds))) {
				FILEDESC_XUNLOCK(fdesc);
				unp_freerights(fdep, newfds);
				m_freem(*controlp);
				*controlp = NULL;
				goto next;
			}
			for (i = 0; i < newfds; i++, fdp++) {
				_finstall(fdesc, fdep[i]->fde_file, *fdp,
				    (flags & MSG_CMSG_CLOEXEC) != 0 ? O_CLOEXEC : 0,
				    &fdep[i]->fde_caps);
				unp_externalize_fp(fdep[i]->fde_file);
			}

			/*
			 * The new type indicates that the mbuf data refers to
			 * kernel resources that may need to be released before
			 * the mbuf is freed.
			 */
			m_chtype(*controlp, MT_EXTCONTROL);
			FILEDESC_XUNLOCK(fdesc);
			free(fdep[0], M_FILECAPS);
		} else {
			/* We can just copy anything else across. */
			if (error || controlp == NULL)
				goto next;
			*controlp = sbcreatecontrol(NULL, datalen,
			    cm->cmsg_type, cm->cmsg_level, M_WAITOK);
			bcopy(data,
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *)),
			    datalen);
		}
		controlp = &(*controlp)->m_next;

next:
		if (CMSG_SPACE(datalen) < clen) {
			clen -= CMSG_SPACE(datalen);
			cm = (struct cmsghdr *)
			    ((caddr_t)cm + CMSG_SPACE(datalen));
		} else {
			clen = 0;
			cm = NULL;
		}
	}

	m_freem(control);
	return (error);
}

static void
unp_zone_change(void *tag)
{

	uma_zone_set_max(unp_zone, maxsockets);
}

#ifdef INVARIANTS
static void
unp_zdtor(void *mem, int size __unused, void *arg __unused)
{
	struct unpcb *unp;

	unp = mem;

	KASSERT(LIST_EMPTY(&unp->unp_refs),
	    ("%s: unpcb %p has lingering refs", __func__, unp));
	KASSERT(unp->unp_socket == NULL,
	    ("%s: unpcb %p has socket backpointer", __func__, unp));
	KASSERT(unp->unp_vnode == NULL,
	    ("%s: unpcb %p has vnode references", __func__, unp));
	KASSERT(unp->unp_conn == NULL,
	    ("%s: unpcb %p is still connected", __func__, unp));
	KASSERT(unp->unp_addr == NULL,
	    ("%s: unpcb %p has leaked addr", __func__, unp));
}
#endif

static void
unp_init(void *arg __unused)
{
	uma_dtor dtor;

#ifdef INVARIANTS
	dtor = unp_zdtor;
#else
	dtor = NULL;
#endif
	unp_zone = uma_zcreate("unpcb", sizeof(struct unpcb), NULL, dtor,
	    NULL, NULL, UMA_ALIGN_CACHE, 0);
	uma_zone_set_max(unp_zone, maxsockets);
	uma_zone_set_warning(unp_zone, "kern.ipc.maxsockets limit reached");
	EVENTHANDLER_REGISTER(maxsockets_change, unp_zone_change,
	    NULL, EVENTHANDLER_PRI_ANY);
	LIST_INIT(&unp_dhead);
	LIST_INIT(&unp_shead);
	LIST_INIT(&unp_sphead);
	SLIST_INIT(&unp_defers);
	TIMEOUT_TASK_INIT(taskqueue_thread, &unp_gc_task, 0, unp_gc, NULL);
	TASK_INIT(&unp_defer_task, 0, unp_process_defers, NULL);
	UNP_LINK_LOCK_INIT();
	UNP_DEFERRED_LOCK_INIT();
}
SYSINIT(unp_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_SECOND, unp_init, NULL);

static void
unp_internalize_cleanup_rights(struct mbuf *control)
{
	struct cmsghdr *cp;
	struct mbuf *m;
	void *data;
	socklen_t datalen;

	for (m = control; m != NULL; m = m->m_next) {
		cp = mtod(m, struct cmsghdr *);
		if (cp->cmsg_level != SOL_SOCKET ||
		    cp->cmsg_type != SCM_RIGHTS)
			continue;
		data = CMSG_DATA(cp);
		datalen = (caddr_t)cp + cp->cmsg_len - (caddr_t)data;
		unp_freerights(data, datalen / sizeof(struct filedesc *));
	}
}

static int
unp_internalize(struct mbuf **controlp, struct thread *td,
    struct mbuf **clast, u_int *space, u_int *mbcnt)
{
	struct mbuf *control, **initial_controlp;
	struct proc *p;
	struct filedesc *fdesc;
	struct bintime *bt;
	struct cmsghdr *cm;
	struct cmsgcred *cmcred;
	struct filedescent *fde, **fdep, *fdev;
	struct file *fp;
	struct timeval *tv;
	struct timespec *ts;
	void *data;
	socklen_t clen, datalen;
	int i, j, error, *fdp, oldfds;
	u_int newlen;

	MPASS((*controlp)->m_next == NULL); /* COMPAT_OLDSOCK may violate */
	UNP_LINK_UNLOCK_ASSERT();

	p = td->td_proc;
	fdesc = p->p_fd;
	error = 0;
	control = *controlp;
	*controlp = NULL;
	initial_controlp = controlp;
	for (clen = control->m_len, cm = mtod(control, struct cmsghdr *),
	    data = CMSG_DATA(cm);

	    clen >= sizeof(*cm) && cm->cmsg_level == SOL_SOCKET &&
	    clen >= cm->cmsg_len && cm->cmsg_len >= sizeof(*cm) &&
	    (char *)cm + cm->cmsg_len >= (char *)data;

	    clen -= min(CMSG_SPACE(datalen), clen),
	    cm = (struct cmsghdr *) ((char *)cm + CMSG_SPACE(datalen)),
	    data = CMSG_DATA(cm)) {
		datalen = (char *)cm + cm->cmsg_len - (char *)data;
		switch (cm->cmsg_type) {
		case SCM_CREDS:
			*controlp = sbcreatecontrol(NULL, sizeof(*cmcred),
			    SCM_CREDS, SOL_SOCKET, M_WAITOK);
			cmcred = (struct cmsgcred *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			cmcred->cmcred_pid = p->p_pid;
			cmcred->cmcred_uid = td->td_ucred->cr_ruid;
			cmcred->cmcred_gid = td->td_ucred->cr_rgid;
			cmcred->cmcred_euid = td->td_ucred->cr_uid;
			cmcred->cmcred_ngroups = MIN(td->td_ucred->cr_ngroups,
			    CMGROUP_MAX);
			for (i = 0; i < cmcred->cmcred_ngroups; i++)
				cmcred->cmcred_groups[i] =
				    td->td_ucred->cr_groups[i];
			break;

		case SCM_RIGHTS:
			oldfds = datalen / sizeof (int);
			if (oldfds == 0)
				continue;
			/* On some machines sizeof pointer is bigger than
			 * sizeof int, so we need to check if data fits into
			 * single mbuf.  We could allocate several mbufs, and
			 * unp_externalize() should even properly handle that.
			 * But it is not worth to complicate the code for an
			 * insane scenario of passing over 200 file descriptors
			 * at once.
			 */
			newlen = oldfds * sizeof(fdep[0]);
			if (CMSG_SPACE(newlen) > MCLBYTES) {
				error = EMSGSIZE;
				goto out;
			}
			/*
			 * Check that all the FDs passed in refer to legal
			 * files.  If not, reject the entire operation.
			 */
			fdp = data;
			FILEDESC_SLOCK(fdesc);
			for (i = 0; i < oldfds; i++, fdp++) {
				fp = fget_noref(fdesc, *fdp);
				if (fp == NULL) {
					FILEDESC_SUNLOCK(fdesc);
					error = EBADF;
					goto out;
				}
				if (!(fp->f_ops->fo_flags & DFLAG_PASSABLE)) {
					FILEDESC_SUNLOCK(fdesc);
					error = EOPNOTSUPP;
					goto out;
				}
			}

			/*
			 * Now replace the integer FDs with pointers to the
			 * file structure and capability rights.
			 */
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET, M_WAITOK);
			fdp = data;
			for (i = 0; i < oldfds; i++, fdp++) {
				if (!fhold(fdesc->fd_ofiles[*fdp].fde_file)) {
					fdp = data;
					for (j = 0; j < i; j++, fdp++) {
						fdrop(fdesc->fd_ofiles[*fdp].
						    fde_file, td);
					}
					FILEDESC_SUNLOCK(fdesc);
					error = EBADF;
					goto out;
				}
			}
			fdp = data;
			fdep = (struct filedescent **)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			fdev = malloc(sizeof(*fdev) * oldfds, M_FILECAPS,
			    M_WAITOK);
			for (i = 0; i < oldfds; i++, fdev++, fdp++) {
				fde = &fdesc->fd_ofiles[*fdp];
				fdep[i] = fdev;
				fdep[i]->fde_file = fde->fde_file;
				filecaps_copy(&fde->fde_caps,
				    &fdep[i]->fde_caps, true);
				unp_internalize_fp(fdep[i]->fde_file);
			}
			FILEDESC_SUNLOCK(fdesc);
			break;

		case SCM_TIMESTAMP:
			*controlp = sbcreatecontrol(NULL, sizeof(*tv),
			    SCM_TIMESTAMP, SOL_SOCKET, M_WAITOK);
			tv = (struct timeval *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			microtime(tv);
			break;

		case SCM_BINTIME:
			*controlp = sbcreatecontrol(NULL, sizeof(*bt),
			    SCM_BINTIME, SOL_SOCKET, M_WAITOK);
			bt = (struct bintime *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			bintime(bt);
			break;

		case SCM_REALTIME:
			*controlp = sbcreatecontrol(NULL, sizeof(*ts),
			    SCM_REALTIME, SOL_SOCKET, M_WAITOK);
			ts = (struct timespec *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			nanotime(ts);
			break;

		case SCM_MONOTONIC:
			*controlp = sbcreatecontrol(NULL, sizeof(*ts),
			    SCM_MONOTONIC, SOL_SOCKET, M_WAITOK);
			ts = (struct timespec *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			nanouptime(ts);
			break;

		default:
			error = EINVAL;
			goto out;
		}

		if (space != NULL) {
			*space += (*controlp)->m_len;
			*mbcnt += MSIZE;
			if ((*controlp)->m_flags & M_EXT)
				*mbcnt += (*controlp)->m_ext.ext_size;
			*clast = *controlp;
		}
		controlp = &(*controlp)->m_next;
	}
	if (clen > 0)
		error = EINVAL;

out:
	if (error != 0 && initial_controlp != NULL)
		unp_internalize_cleanup_rights(*initial_controlp);
	m_freem(control);
	return (error);
}

static struct mbuf *
unp_addsockcred(struct thread *td, struct mbuf *control, int mode,
    struct mbuf **clast, u_int *space, u_int *mbcnt)
{
	struct mbuf *m, *n, *n_prev;
	const struct cmsghdr *cm;
	int ngroups, i, cmsgtype;
	size_t ctrlsz;

	ngroups = MIN(td->td_ucred->cr_ngroups, CMGROUP_MAX);
	if (mode & UNP_WANTCRED_ALWAYS) {
		ctrlsz = SOCKCRED2SIZE(ngroups);
		cmsgtype = SCM_CREDS2;
	} else {
		ctrlsz = SOCKCREDSIZE(ngroups);
		cmsgtype = SCM_CREDS;
	}

	m = sbcreatecontrol(NULL, ctrlsz, cmsgtype, SOL_SOCKET, M_NOWAIT);
	if (m == NULL)
		return (control);
	MPASS((m->m_flags & M_EXT) == 0 && m->m_next == NULL);

	if (mode & UNP_WANTCRED_ALWAYS) {
		struct sockcred2 *sc;

		sc = (void *)CMSG_DATA(mtod(m, struct cmsghdr *));
		sc->sc_version = 0;
		sc->sc_pid = td->td_proc->p_pid;
		sc->sc_uid = td->td_ucred->cr_ruid;
		sc->sc_euid = td->td_ucred->cr_uid;
		sc->sc_gid = td->td_ucred->cr_rgid;
		sc->sc_egid = td->td_ucred->cr_gid;
		sc->sc_ngroups = ngroups;
		for (i = 0; i < sc->sc_ngroups; i++)
			sc->sc_groups[i] = td->td_ucred->cr_groups[i];
	} else {
		struct sockcred *sc;

		sc = (void *)CMSG_DATA(mtod(m, struct cmsghdr *));
		sc->sc_uid = td->td_ucred->cr_ruid;
		sc->sc_euid = td->td_ucred->cr_uid;
		sc->sc_gid = td->td_ucred->cr_rgid;
		sc->sc_egid = td->td_ucred->cr_gid;
		sc->sc_ngroups = ngroups;
		for (i = 0; i < sc->sc_ngroups; i++)
			sc->sc_groups[i] = td->td_ucred->cr_groups[i];
	}

	/*
	 * Unlink SCM_CREDS control messages (struct cmsgcred), since just
	 * created SCM_CREDS control message (struct sockcred) has another
	 * format.
	 */
	if (control != NULL && cmsgtype == SCM_CREDS)
		for (n = control, n_prev = NULL; n != NULL;) {
			cm = mtod(n, struct cmsghdr *);
    			if (cm->cmsg_level == SOL_SOCKET &&
			    cm->cmsg_type == SCM_CREDS) {
    				if (n_prev == NULL)
					control = n->m_next;
				else
					n_prev->m_next = n->m_next;
				if (space != NULL) {
					MPASS(*space >= n->m_len);
					*space -= n->m_len;
					MPASS(*mbcnt >= MSIZE);
					*mbcnt -= MSIZE;
					if (n->m_flags & M_EXT) {
						MPASS(*mbcnt >=
						    n->m_ext.ext_size);
						*mbcnt -= n->m_ext.ext_size;
					}
					MPASS(clast);
					if (*clast == n) {
						MPASS(n->m_next == NULL);
						if (n_prev == NULL)
							*clast = m;
						else
							*clast = n_prev;
					}
				}
				n = m_free(n);
			} else {
				n_prev = n;
				n = n->m_next;
			}
		}

	/* Prepend it to the head. */
	m->m_next = control;
	if (space != NULL) {
		*space += m->m_len;
		*mbcnt += MSIZE;
		if (control == NULL)
			*clast = m;
	}
	return (m);
}

static struct unpcb *
fptounp(struct file *fp)
{
	struct socket *so;

	if (fp->f_type != DTYPE_SOCKET)
		return (NULL);
	if ((so = fp->f_data) == NULL)
		return (NULL);
	if (so->so_proto->pr_domain != &localdomain)
		return (NULL);
	return sotounpcb(so);
}

static void
unp_discard(struct file *fp)
{
	struct unp_defer *dr;

	if (unp_externalize_fp(fp)) {
		dr = malloc(sizeof(*dr), M_TEMP, M_WAITOK);
		dr->ud_fp = fp;
		UNP_DEFERRED_LOCK();
		SLIST_INSERT_HEAD(&unp_defers, dr, ud_link);
		UNP_DEFERRED_UNLOCK();
		atomic_add_int(&unp_defers_count, 1);
		taskqueue_enqueue(taskqueue_thread, &unp_defer_task);
	} else
		closef_nothread(fp);
}

static void
unp_process_defers(void *arg __unused, int pending)
{
	struct unp_defer *dr;
	SLIST_HEAD(, unp_defer) drl;
	int count;

	SLIST_INIT(&drl);
	for (;;) {
		UNP_DEFERRED_LOCK();
		if (SLIST_FIRST(&unp_defers) == NULL) {
			UNP_DEFERRED_UNLOCK();
			break;
		}
		SLIST_SWAP(&unp_defers, &drl, unp_defer);
		UNP_DEFERRED_UNLOCK();
		count = 0;
		while ((dr = SLIST_FIRST(&drl)) != NULL) {
			SLIST_REMOVE_HEAD(&drl, ud_link);
			closef_nothread(dr->ud_fp);
			free(dr, M_TEMP);
			count++;
		}
		atomic_add_int(&unp_defers_count, -count);
	}
}

static void
unp_internalize_fp(struct file *fp)
{
	struct unpcb *unp;

	UNP_LINK_WLOCK();
	if ((unp = fptounp(fp)) != NULL) {
		unp->unp_file = fp;
		unp->unp_msgcount++;
	}
	unp_rights++;
	UNP_LINK_WUNLOCK();
}

static int
unp_externalize_fp(struct file *fp)
{
	struct unpcb *unp;
	int ret;

	UNP_LINK_WLOCK();
	if ((unp = fptounp(fp)) != NULL) {
		unp->unp_msgcount--;
		ret = 1;
	} else
		ret = 0;
	unp_rights--;
	UNP_LINK_WUNLOCK();
	return (ret);
}

/*
 * unp_defer indicates whether additional work has been defered for a future
 * pass through unp_gc().  It is thread local and does not require explicit
 * synchronization.
 */
static int	unp_marked;

static void
unp_remove_dead_ref(struct filedescent **fdep, int fdcount)
{
	struct unpcb *unp;
	struct file *fp;
	int i;

	/*
	 * This function can only be called from the gc task.
	 */
	KASSERT(taskqueue_member(taskqueue_thread, curthread) != 0,
	    ("%s: not on gc callout", __func__));
	UNP_LINK_LOCK_ASSERT();

	for (i = 0; i < fdcount; i++) {
		fp = fdep[i]->fde_file;
		if ((unp = fptounp(fp)) == NULL)
			continue;
		if ((unp->unp_gcflag & UNPGC_DEAD) == 0)
			continue;
		unp->unp_gcrefs--;
	}
}

static void
unp_restore_undead_ref(struct filedescent **fdep, int fdcount)
{
	struct unpcb *unp;
	struct file *fp;
	int i;

	/*
	 * This function can only be called from the gc task.
	 */
	KASSERT(taskqueue_member(taskqueue_thread, curthread) != 0,
	    ("%s: not on gc callout", __func__));
	UNP_LINK_LOCK_ASSERT();

	for (i = 0; i < fdcount; i++) {
		fp = fdep[i]->fde_file;
		if ((unp = fptounp(fp)) == NULL)
			continue;
		if ((unp->unp_gcflag & UNPGC_DEAD) == 0)
			continue;
		unp->unp_gcrefs++;
		unp_marked++;
	}
}

static void
unp_scan_socket(struct socket *so, void (*op)(struct filedescent **, int))
{
	struct sockbuf *sb;

	SOCK_LOCK_ASSERT(so);

	if (sotounpcb(so)->unp_gcflag & UNPGC_IGNORE_RIGHTS)
		return;

	SOCK_RECVBUF_LOCK(so);
	switch (so->so_type) {
	case SOCK_DGRAM:
		unp_scan(STAILQ_FIRST(&so->so_rcv.uxdg_mb), op);
		unp_scan(so->so_rcv.uxdg_peeked, op);
		TAILQ_FOREACH(sb, &so->so_rcv.uxdg_conns, uxdg_clist)
			unp_scan(STAILQ_FIRST(&sb->uxdg_mb), op);
		break;
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		unp_scan(so->so_rcv.sb_mb, op);
		break;
	}
	SOCK_RECVBUF_UNLOCK(so);
}

static void
unp_gc_scan(struct unpcb *unp, void (*op)(struct filedescent **, int))
{
	struct socket *so, *soa;

	so = unp->unp_socket;
	SOCK_LOCK(so);
	if (SOLISTENING(so)) {
		/*
		 * Mark all sockets in our accept queue.
		 */
		TAILQ_FOREACH(soa, &so->sol_comp, so_list)
			unp_scan_socket(soa, op);
	} else {
		/*
		 * Mark all sockets we reference with RIGHTS.
		 */
		unp_scan_socket(so, op);
	}
	SOCK_UNLOCK(so);
}

static int unp_recycled;
SYSCTL_INT(_net_local, OID_AUTO, recycled, CTLFLAG_RD, &unp_recycled, 0, 
    "Number of unreachable sockets claimed by the garbage collector.");

static int unp_taskcount;
SYSCTL_INT(_net_local, OID_AUTO, taskcount, CTLFLAG_RD, &unp_taskcount, 0, 
    "Number of times the garbage collector has run.");

SYSCTL_UINT(_net_local, OID_AUTO, sockcount, CTLFLAG_RD, &unp_count, 0, 
    "Number of active local sockets.");

static void
unp_gc(__unused void *arg, int pending)
{
	struct unp_head *heads[] = { &unp_dhead, &unp_shead, &unp_sphead,
				    NULL };
	struct unp_head **head;
	struct unp_head unp_deadhead;	/* List of potentially-dead sockets. */
	struct file *f, **unref;
	struct unpcb *unp, *unptmp;
	int i, total, unp_unreachable;

	LIST_INIT(&unp_deadhead);
	unp_taskcount++;
	UNP_LINK_RLOCK();
	/*
	 * First determine which sockets may be in cycles.
	 */
	unp_unreachable = 0;

	for (head = heads; *head != NULL; head++)
		LIST_FOREACH(unp, *head, unp_link) {
			KASSERT((unp->unp_gcflag & ~UNPGC_IGNORE_RIGHTS) == 0,
			    ("%s: unp %p has unexpected gc flags 0x%x",
			    __func__, unp, (unsigned int)unp->unp_gcflag));

			f = unp->unp_file;

			/*
			 * Check for an unreachable socket potentially in a
			 * cycle.  It must be in a queue as indicated by
			 * msgcount, and this must equal the file reference
			 * count.  Note that when msgcount is 0 the file is
			 * NULL.
			 */
			if (f != NULL && unp->unp_msgcount != 0 &&
			    refcount_load(&f->f_count) == unp->unp_msgcount) {
				LIST_INSERT_HEAD(&unp_deadhead, unp, unp_dead);
				unp->unp_gcflag |= UNPGC_DEAD;
				unp->unp_gcrefs = unp->unp_msgcount;
				unp_unreachable++;
			}
		}

	/*
	 * Scan all sockets previously marked as potentially being in a cycle
	 * and remove the references each socket holds on any UNPGC_DEAD
	 * sockets in its queue.  After this step, all remaining references on
	 * sockets marked UNPGC_DEAD should not be part of any cycle.
	 */
	LIST_FOREACH(unp, &unp_deadhead, unp_dead)
		unp_gc_scan(unp, unp_remove_dead_ref);

	/*
	 * If a socket still has a non-negative refcount, it cannot be in a
	 * cycle.  In this case increment refcount of all children iteratively.
	 * Stop the scan once we do a complete loop without discovering
	 * a new reachable socket.
	 */
	do {
		unp_marked = 0;
		LIST_FOREACH_SAFE(unp, &unp_deadhead, unp_dead, unptmp)
			if (unp->unp_gcrefs > 0) {
				unp->unp_gcflag &= ~UNPGC_DEAD;
				LIST_REMOVE(unp, unp_dead);
				KASSERT(unp_unreachable > 0,
				    ("%s: unp_unreachable underflow.",
				    __func__));
				unp_unreachable--;
				unp_gc_scan(unp, unp_restore_undead_ref);
			}
	} while (unp_marked);

	UNP_LINK_RUNLOCK();

	if (unp_unreachable == 0)
		return;

	/*
	 * Allocate space for a local array of dead unpcbs.
	 * TODO: can this path be simplified by instead using the local
	 * dead list at unp_deadhead, after taking out references
	 * on the file object and/or unpcb and dropping the link lock?
	 */
	unref = malloc(unp_unreachable * sizeof(struct file *),
	    M_TEMP, M_WAITOK);

	/*
	 * Iterate looking for sockets which have been specifically marked
	 * as unreachable and store them locally.
	 */
	UNP_LINK_RLOCK();
	total = 0;
	LIST_FOREACH(unp, &unp_deadhead, unp_dead) {
		KASSERT((unp->unp_gcflag & UNPGC_DEAD) != 0,
		    ("%s: unp %p not marked UNPGC_DEAD", __func__, unp));
		unp->unp_gcflag &= ~UNPGC_DEAD;
		f = unp->unp_file;
		if (unp->unp_msgcount == 0 || f == NULL ||
		    refcount_load(&f->f_count) != unp->unp_msgcount ||
		    !fhold(f))
			continue;
		unref[total++] = f;
		KASSERT(total <= unp_unreachable,
		    ("%s: incorrect unreachable count.", __func__));
	}
	UNP_LINK_RUNLOCK();

	/*
	 * Now flush all sockets, free'ing rights.  This will free the
	 * struct files associated with these sockets but leave each socket
	 * with one remaining ref.
	 */
	for (i = 0; i < total; i++) {
		struct socket *so;

		so = unref[i]->f_data;
		CURVNET_SET(so->so_vnet);
		sorflush(so);
		CURVNET_RESTORE();
	}

	/*
	 * And finally release the sockets so they can be reclaimed.
	 */
	for (i = 0; i < total; i++)
		fdrop(unref[i], NULL);
	unp_recycled += total;
	free(unref, M_TEMP);
}

/*
 * Synchronize against unp_gc, which can trip over data as we are freeing it.
 */
static void
unp_dispose(struct socket *so)
{
	struct sockbuf *sb;
	struct unpcb *unp;
	struct mbuf *m;

	MPASS(!SOLISTENING(so));

	unp = sotounpcb(so);
	UNP_LINK_WLOCK();
	unp->unp_gcflag |= UNPGC_IGNORE_RIGHTS;
	UNP_LINK_WUNLOCK();

	/*
	 * Grab our special mbufs before calling sbrelease().
	 */
	SOCK_RECVBUF_LOCK(so);
	switch (so->so_type) {
	case SOCK_DGRAM:
		while ((sb = TAILQ_FIRST(&so->so_rcv.uxdg_conns)) != NULL) {
			STAILQ_CONCAT(&so->so_rcv.uxdg_mb, &sb->uxdg_mb);
			TAILQ_REMOVE(&so->so_rcv.uxdg_conns, sb, uxdg_clist);
			/* Note: socket of sb may reconnect. */
			sb->uxdg_cc = sb->uxdg_ctl = sb->uxdg_mbcnt = 0;
		}
		sb = &so->so_rcv;
		if (sb->uxdg_peeked != NULL) {
			STAILQ_INSERT_HEAD(&sb->uxdg_mb, sb->uxdg_peeked,
			    m_stailqpkt);
			sb->uxdg_peeked = NULL;
		}
		m = STAILQ_FIRST(&sb->uxdg_mb);
		STAILQ_INIT(&sb->uxdg_mb);
		/* XXX: our shortened sbrelease() */
		(void)chgsbsize(so->so_cred->cr_uidinfo, &sb->sb_hiwat, 0,
		    RLIM_INFINITY);
		/*
		 * XXXGL Mark sb with SBS_CANTRCVMORE.  This is needed to
		 * prevent uipc_sosend_dgram() or unp_disconnect() adding more
		 * data to the socket.
		 * We are now in dom_dispose and it could be a call from
		 * soshutdown() or from the final sofree().  The sofree() case
		 * is simple as it guarantees that no more sends will happen,
		 * however we can race with unp_disconnect() from our peer.
		 * The shutdown(2) case is more exotic.  It would call into
		 * dom_dispose() only if socket is SS_ISCONNECTED.  This is
		 * possible if we did connect(2) on this socket and we also
		 * had it bound with bind(2) and receive connections from other
		 * sockets.  Because soshutdown() violates POSIX (see comment
		 * there) we will end up here shutting down our receive side.
		 * Of course this will have affect not only on the peer we
		 * connect(2)ed to, but also on all of the peers who had
		 * connect(2)ed to us.  Their sends would end up with ENOBUFS.
		 */
		sb->sb_state |= SBS_CANTRCVMORE;
		break;
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		sb = &so->so_rcv;
		m = sbcut_locked(sb, sb->sb_ccc);
		KASSERT(sb->sb_ccc == 0 && sb->sb_mb == 0 && sb->sb_mbcnt == 0,
		    ("%s: ccc %u mb %p mbcnt %u", __func__,
		    sb->sb_ccc, (void *)sb->sb_mb, sb->sb_mbcnt));
		sbrelease_locked(so, SO_RCV);
		break;
	}
	SOCK_RECVBUF_UNLOCK(so);
	if (SOCK_IO_RECV_OWNED(so))
		SOCK_IO_RECV_UNLOCK(so);

	if (m != NULL) {
		unp_scan(m, unp_freerights);
		m_freem(m);
	}
}

static void
unp_scan(struct mbuf *m0, void (*op)(struct filedescent **, int))
{
	struct mbuf *m;
	struct cmsghdr *cm;
	void *data;
	socklen_t clen, datalen;

	while (m0 != NULL) {
		for (m = m0; m; m = m->m_next) {
			if (m->m_type != MT_CONTROL)
				continue;

			cm = mtod(m, struct cmsghdr *);
			clen = m->m_len;

			while (cm != NULL) {
				if (sizeof(*cm) > clen || cm->cmsg_len > clen)
					break;

				data = CMSG_DATA(cm);
				datalen = (caddr_t)cm + cm->cmsg_len
				    - (caddr_t)data;

				if (cm->cmsg_level == SOL_SOCKET &&
				    cm->cmsg_type == SCM_RIGHTS) {
					(*op)(data, datalen /
					    sizeof(struct filedescent *));
				}

				if (CMSG_SPACE(datalen) < clen) {
					clen -= CMSG_SPACE(datalen);
					cm = (struct cmsghdr *)
					    ((caddr_t)cm + CMSG_SPACE(datalen));
				} else {
					clen = 0;
					cm = NULL;
				}
			}
		}
		m0 = m0->m_nextpkt;
	}
}

/*
 * Definitions of protocols supported in the LOCAL domain.
 */
static struct protosw streamproto = {
	.pr_type =		SOCK_STREAM,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS|
				    PR_CAPATTACH,
	.pr_ctloutput =		&uipc_ctloutput,
	.pr_abort = 		uipc_abort,
	.pr_accept =		uipc_accept,
	.pr_attach =		uipc_attach,
	.pr_bind =		uipc_bind,
	.pr_bindat =		uipc_bindat,
	.pr_connect =		uipc_connect,
	.pr_connectat =		uipc_connectat,
	.pr_connect2 =		uipc_connect2,
	.pr_detach =		uipc_detach,
	.pr_disconnect =	uipc_disconnect,
	.pr_listen =		uipc_listen,
	.pr_peeraddr =		uipc_peeraddr,
	.pr_rcvd =		uipc_rcvd,
	.pr_send =		uipc_send,
	.pr_ready =		uipc_ready,
	.pr_sense =		uipc_sense,
	.pr_shutdown =		uipc_shutdown,
	.pr_sockaddr =		uipc_sockaddr,
	.pr_soreceive =		soreceive_generic,
	.pr_close =		uipc_close,
};

static struct protosw dgramproto = {
	.pr_type =		SOCK_DGRAM,
	.pr_flags =		PR_ATOMIC | PR_ADDR |PR_RIGHTS | PR_CAPATTACH |
				    PR_SOCKBUF,
	.pr_ctloutput =		&uipc_ctloutput,
	.pr_abort = 		uipc_abort,
	.pr_accept =		uipc_accept,
	.pr_attach =		uipc_attach,
	.pr_bind =		uipc_bind,
	.pr_bindat =		uipc_bindat,
	.pr_connect =		uipc_connect,
	.pr_connectat =		uipc_connectat,
	.pr_connect2 =		uipc_connect2,
	.pr_detach =		uipc_detach,
	.pr_disconnect =	uipc_disconnect,
	.pr_peeraddr =		uipc_peeraddr,
	.pr_sosend =		uipc_sosend_dgram,
	.pr_sense =		uipc_sense,
	.pr_shutdown =		uipc_shutdown,
	.pr_sockaddr =		uipc_sockaddr,
	.pr_soreceive =		uipc_soreceive_dgram,
	.pr_close =		uipc_close,
};

static struct protosw seqpacketproto = {
	.pr_type =		SOCK_SEQPACKET,
	/*
	 * XXXRW: For now, PR_ADDR because soreceive will bump into them
	 * due to our use of sbappendaddr.  A new sbappend variants is needed
	 * that supports both atomic record writes and control data.
	 */
	.pr_flags =		PR_ADDR|PR_ATOMIC|PR_CONNREQUIRED|
				    PR_WANTRCVD|PR_RIGHTS|PR_CAPATTACH,
	.pr_ctloutput =		&uipc_ctloutput,
	.pr_abort =		uipc_abort,
	.pr_accept =		uipc_accept,
	.pr_attach =		uipc_attach,
	.pr_bind =		uipc_bind,
	.pr_bindat =		uipc_bindat,
	.pr_connect =		uipc_connect,
	.pr_connectat =		uipc_connectat,
	.pr_connect2 =		uipc_connect2,
	.pr_detach =		uipc_detach,
	.pr_disconnect =	uipc_disconnect,
	.pr_listen =		uipc_listen,
	.pr_peeraddr =		uipc_peeraddr,
	.pr_rcvd =		uipc_rcvd,
	.pr_send =		uipc_send,
	.pr_sense =		uipc_sense,
	.pr_shutdown =		uipc_shutdown,
	.pr_sockaddr =		uipc_sockaddr,
	.pr_soreceive =		soreceive_generic,	/* XXX: or...? */
	.pr_close =		uipc_close,
};

static struct domain localdomain = {
	.dom_family =		AF_LOCAL,
	.dom_name =		"local",
	.dom_externalize =	unp_externalize,
	.dom_dispose =		unp_dispose,
	.dom_nprotosw =		3,
	.dom_protosw =		{
		&streamproto,
		&dgramproto,
		&seqpacketproto,
	}
};
DOMAIN_SET(local);

/*
 * A helper function called by VFS before socket-type vnode reclamation.
 * For an active vnode it clears unp_vnode pointer and decrements unp_vnode
 * use count.
 */
void
vfs_unp_reclaim(struct vnode *vp)
{
	struct unpcb *unp;
	int active;
	struct mtx *vplock;

	ASSERT_VOP_ELOCKED(vp, "vfs_unp_reclaim");
	KASSERT(vp->v_type == VSOCK,
	    ("vfs_unp_reclaim: vp->v_type != VSOCK"));

	active = 0;
	vplock = mtx_pool_find(mtxpool_sleep, vp);
	mtx_lock(vplock);
	VOP_UNP_CONNECT(vp, &unp);
	if (unp == NULL)
		goto done;
	UNP_PCB_LOCK(unp);
	if (unp->unp_vnode == vp) {
		VOP_UNP_DETACH(vp);
		unp->unp_vnode = NULL;
		active = 1;
	}
	UNP_PCB_UNLOCK(unp);
 done:
	mtx_unlock(vplock);
	if (active)
		vunref(vp);
}

#ifdef DDB
static void
db_print_indent(int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		db_printf(" ");
}

static void
db_print_unpflags(int unp_flags)
{
	int comma;

	comma = 0;
	if (unp_flags & UNP_HAVEPC) {
		db_printf("%sUNP_HAVEPC", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_WANTCRED_ALWAYS) {
		db_printf("%sUNP_WANTCRED_ALWAYS", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_WANTCRED_ONESHOT) {
		db_printf("%sUNP_WANTCRED_ONESHOT", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_CONNWAIT) {
		db_printf("%sUNP_CONNWAIT", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_CONNECTING) {
		db_printf("%sUNP_CONNECTING", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_BINDING) {
		db_printf("%sUNP_BINDING", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_xucred(int indent, struct xucred *xu)
{
	int comma, i;

	db_print_indent(indent);
	db_printf("cr_version: %u   cr_uid: %u   cr_pid: %d   cr_ngroups: %d\n",
	    xu->cr_version, xu->cr_uid, xu->cr_pid, xu->cr_ngroups);
	db_print_indent(indent);
	db_printf("cr_groups: ");
	comma = 0;
	for (i = 0; i < xu->cr_ngroups; i++) {
		db_printf("%s%u", comma ? ", " : "", xu->cr_groups[i]);
		comma = 1;
	}
	db_printf("\n");
}

static void
db_print_unprefs(int indent, struct unp_head *uh)
{
	struct unpcb *unp;
	int counter;

	counter = 0;
	LIST_FOREACH(unp, uh, unp_reflink) {
		if (counter % 4 == 0)
			db_print_indent(indent);
		db_printf("%p  ", unp);
		if (counter % 4 == 3)
			db_printf("\n");
		counter++;
	}
	if (counter != 0 && counter % 4 != 0)
		db_printf("\n");
}

DB_SHOW_COMMAND(unpcb, db_show_unpcb)
{
	struct unpcb *unp;

        if (!have_addr) {
                db_printf("usage: show unpcb <addr>\n");
                return;
        }
        unp = (struct unpcb *)addr;

	db_printf("unp_socket: %p   unp_vnode: %p\n", unp->unp_socket,
	    unp->unp_vnode);

	db_printf("unp_ino: %ju   unp_conn: %p\n", (uintmax_t)unp->unp_ino,
	    unp->unp_conn);

	db_printf("unp_refs:\n");
	db_print_unprefs(2, &unp->unp_refs);

	/* XXXRW: Would be nice to print the full address, if any. */
	db_printf("unp_addr: %p\n", unp->unp_addr);

	db_printf("unp_gencnt: %llu\n",
	    (unsigned long long)unp->unp_gencnt);

	db_printf("unp_flags: %x (", unp->unp_flags);
	db_print_unpflags(unp->unp_flags);
	db_printf(")\n");

	db_printf("unp_peercred:\n");
	db_print_xucred(2, &unp->unp_peercred);

	db_printf("unp_refcount: %u\n", unp->unp_refcount);
}
#endif
