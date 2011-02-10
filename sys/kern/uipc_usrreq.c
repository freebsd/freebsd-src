/*-
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2004-2008 Robert N. M. Watson
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
 *	SEQPACKET, RDM
 *	rethink name space problems
 *	need a proper out-of-band
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>		/* XXX must be before <sys/file.h> */
#include <sys/eventhandler.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/protosw.h>
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

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <security/mac/mac_framework.h>

#include <vm/uma.h>

static uma_zone_t	unp_zone;
static unp_gen_t	unp_gencnt;
static u_int		unp_count;	/* Count of local sockets. */
static ino_t		unp_ino;	/* Prototype for fake inode numbers. */
static int		unp_rights;	/* File descriptors in flight. */
static struct unp_head	unp_shead;	/* List of local stream sockets. */
static struct unp_head	unp_dhead;	/* List of local datagram sockets. */

static const struct sockaddr	sun_noname = { sizeof(sun_noname), AF_LOCAL };

/*
 * Garbage collection of cyclic file descriptor/socket references occurs
 * asynchronously in a taskqueue context in order to avoid recursion and
 * reentrance in the UNIX domain socket, file descriptor, and socket layer
 * code.  See unp_gc() for a full description.
 */
static struct task	unp_gc_task;

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
static u_long	unpdg_sendspace = 2*1024;	/* really max datagram size */
static u_long	unpdg_recvspace = 4*1024;

SYSCTL_NODE(_net, PF_LOCAL, local, CTLFLAG_RW, 0, "Local domain");
SYSCTL_NODE(_net_local, SOCK_STREAM, stream, CTLFLAG_RW, 0, "SOCK_STREAM");
SYSCTL_NODE(_net_local, SOCK_DGRAM, dgram, CTLFLAG_RW, 0, "SOCK_DGRAM");

SYSCTL_ULONG(_net_local_stream, OID_AUTO, sendspace, CTLFLAG_RW,
	   &unpst_sendspace, 0, "");
SYSCTL_ULONG(_net_local_stream, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpst_recvspace, 0, "");
SYSCTL_ULONG(_net_local_dgram, OID_AUTO, maxdgram, CTLFLAG_RW,
	   &unpdg_sendspace, 0, "");
SYSCTL_ULONG(_net_local_dgram, OID_AUTO, recvspace, CTLFLAG_RW,
	   &unpdg_recvspace, 0, "");
SYSCTL_INT(_net_local, OID_AUTO, inflight, CTLFLAG_RD, &unp_rights, 0, "");

/*-
 * Locking and synchronization:
 *
 * The global UNIX domain socket rwlock (unp_global_rwlock) protects all
 * global variables, including the linked lists tracking the set of allocated
 * UNIX domain sockets.  The global rwlock also serves to prevent deadlock
 * when more than one PCB lock is acquired at a time (i.e., during
 * connect()).  Finally, the global rwlock protects uncounted references from
 * vnodes to sockets bound to those vnodes: to safely dereference the
 * v_socket pointer, the global rwlock must be held while a full reference is
 * acquired.
 *
 * UNIX domain sockets each have an unpcb hung off of their so_pcb pointer,
 * allocated in pru_attach() and freed in pru_detach().  The validity of that
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
 * unp->unp_conn->unp_socket, you need unlock the lock on unp, not unp_conn,
 * as unp_socket remains valid as long as the reference to unp_conn is valid.
 *
 * Fields of unpcbss are locked using a per-unpcb lock, unp_mtx.  Individual
 * atomic reads without the lock may be performed "lockless", but more
 * complex reads and read-modify-writes require the mutex to be held.  No
 * lock order is defined between unpcb locks -- multiple unpcb locks may be
 * acquired at the same time only when holding the global UNIX domain socket
 * rwlock exclusively, which prevents deadlocks.
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
static struct rwlock	unp_global_rwlock;

#define	UNP_GLOBAL_LOCK_INIT()		rw_init(&unp_global_rwlock,	\
					    "unp_global_rwlock")

#define	UNP_GLOBAL_LOCK_ASSERT()	rw_assert(&unp_global_rwlock,	\
					    RA_LOCKED)
#define	UNP_GLOBAL_UNLOCK_ASSERT()	rw_assert(&unp_global_rwlock,	\
					    RA_UNLOCKED)

#define	UNP_GLOBAL_WLOCK()		rw_wlock(&unp_global_rwlock)
#define	UNP_GLOBAL_WUNLOCK()		rw_wunlock(&unp_global_rwlock)
#define	UNP_GLOBAL_WLOCK_ASSERT()	rw_assert(&unp_global_rwlock,	\
					    RA_WLOCKED)
#define	UNP_GLOBAL_WOWNED()		rw_wowned(&unp_global_rwlock)

#define	UNP_GLOBAL_RLOCK()		rw_rlock(&unp_global_rwlock)
#define	UNP_GLOBAL_RUNLOCK()		rw_runlock(&unp_global_rwlock)
#define	UNP_GLOBAL_RLOCK_ASSERT()	rw_assert(&unp_global_rwlock,	\
					    RA_RLOCKED)

#define UNP_PCB_LOCK_INIT(unp)		mtx_init(&(unp)->unp_mtx,	\
					    "unp_mtx", "unp_mtx",	\
					    MTX_DUPOK|MTX_DEF|MTX_RECURSE)
#define	UNP_PCB_LOCK_DESTROY(unp)	mtx_destroy(&(unp)->unp_mtx)
#define	UNP_PCB_LOCK(unp)		mtx_lock(&(unp)->unp_mtx)
#define	UNP_PCB_UNLOCK(unp)		mtx_unlock(&(unp)->unp_mtx)
#define	UNP_PCB_LOCK_ASSERT(unp)	mtx_assert(&(unp)->unp_mtx, MA_OWNED)

static int	uipc_connect2(struct socket *, struct socket *);
static int	uipc_ctloutput(struct socket *, struct sockopt *);
static int	unp_connect(struct socket *, struct sockaddr *,
		    struct thread *);
static int	unp_connect2(struct socket *so, struct socket *so2, int);
static void	unp_disconnect(struct unpcb *unp, struct unpcb *unp2);
static void	unp_dispose(struct mbuf *);
static void	unp_shutdown(struct unpcb *);
static void	unp_drop(struct unpcb *, int);
static void	unp_gc(__unused void *, int);
static void	unp_scan(struct mbuf *, void (*)(struct file *));
static void	unp_mark(struct file *);
static void	unp_discard(struct file *);
static void	unp_freerights(struct file **, int);
static void	unp_init(void);
static int	unp_internalize(struct mbuf **, struct thread *);
static int	unp_externalize(struct mbuf *, struct mbuf **);
static struct mbuf	*unp_addsockcred(struct thread *, struct mbuf *);

/*
 * Definitions of protocols supported in the LOCAL domain.
 */
static struct domain localdomain;
static struct pr_usrreqs uipc_usrreqs_dgram, uipc_usrreqs_stream;
static struct protosw localsw[] = {
{
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&localdomain,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS,
	.pr_ctloutput =		&uipc_ctloutput,
	.pr_usrreqs =		&uipc_usrreqs_stream
},
{
	.pr_type =		SOCK_DGRAM,
	.pr_domain =		&localdomain,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_RIGHTS,
	.pr_usrreqs =		&uipc_usrreqs_dgram
},
};

static struct domain localdomain = {
	.dom_family =		AF_LOCAL,
	.dom_name =		"local",
	.dom_init =		unp_init,
	.dom_externalize =	unp_externalize,
	.dom_dispose =		unp_dispose,
	.dom_protosw =		localsw,
	.dom_protoswNPROTOSW =	&localsw[sizeof(localsw)/sizeof(localsw[0])]
};
DOMAIN_SET(local);

static void
uipc_abort(struct socket *so)
{
	struct unpcb *unp, *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_abort: unp == NULL"));

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		UNP_PCB_LOCK(unp2);
		unp_drop(unp2, ECONNABORTED);
		UNP_PCB_UNLOCK(unp2);
	}
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
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
	UNP_GLOBAL_RLOCK();
	unp2 = unp->unp_conn;
	if (unp2 != NULL && unp2->unp_addr != NULL) {
		UNP_PCB_LOCK(unp2);
		sa = (struct sockaddr *) unp2->unp_addr;
		bcopy(sa, *nam, sa->sa_len);
		UNP_PCB_UNLOCK(unp2);
	} else {
		sa = &sun_noname;
		bcopy(sa, *nam, sa->sa_len);
	}
	UNP_GLOBAL_RUNLOCK();
	return (0);
}

static int
uipc_attach(struct socket *so, int proto, struct thread *td)
{
	u_long sendspace, recvspace;
	struct unpcb *unp;
	int error, locked;

	KASSERT(so->so_pcb == NULL, ("uipc_attach: so_pcb != NULL"));
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {
		case SOCK_STREAM:
			sendspace = unpst_sendspace;
			recvspace = unpst_recvspace;
			break;

		case SOCK_DGRAM:
			sendspace = unpdg_sendspace;
			recvspace = unpdg_recvspace;
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
	unp->unp_refcount = 1;

	/*
	 * uipc_attach() may be called indirectly from within the UNIX domain
	 * socket code via sonewconn() in unp_connect().  Since rwlocks can
	 * not be recursed, we do the closest thing.
	 */
	locked = 0;
	if (!UNP_GLOBAL_WOWNED()) {
		UNP_GLOBAL_WLOCK();
		locked = 1;
	}
	unp->unp_gencnt = ++unp_gencnt;
	unp_count++;
	LIST_INSERT_HEAD(so->so_type == SOCK_DGRAM ? &unp_dhead : &unp_shead,
	    unp, unp_link);
	if (locked)
		UNP_GLOBAL_WUNLOCK();

	return (0);
}

static int
uipc_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	struct vattr vattr;
	int error, namelen, vfslocked;
	struct nameidata nd;
	struct unpcb *unp;
	struct vnode *vp;
	struct mount *mp;
	char *buf;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_bind: unp == NULL"));

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
	vfslocked = 0;
	NDINIT(&nd, CREATE, MPSAFE | NOFOLLOW | LOCKPARENT | SAVENAME,
	    UIO_SYSSPACE, buf, td);
/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	error = namei(&nd);
	if (error)
		goto error;
	vp = nd.ni_vp;
	vfslocked = NDHASGIANT(&nd);
	if (vp != NULL || vn_start_write(nd.ni_dvp, &mp, V_NOWAIT) != 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp != NULL) {
			vrele(vp);
			error = EADDRINUSE;
			goto error;
		}
		error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH);
		if (error)
			goto error;
		VFS_UNLOCK_GIANT(vfslocked);
		goto restart;
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = (ACCESSPERMS & ~td->td_proc->p_fd->fd_cmask);
#ifdef MAC
	error = mac_check_vnode_create(td->td_ucred, nd.ni_dvp, &nd.ni_cnd,
	    &vattr);
#endif
	if (error == 0) {
		VOP_LEASE(nd.ni_dvp, td, td->td_ucred, LEASE_WRITE);
		error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vput(nd.ni_dvp);
	if (error) {
		vn_finished_write(mp);
		goto error;
	}
	vp = nd.ni_vp;
	ASSERT_VOP_ELOCKED(vp, "uipc_bind");
	soun = (struct sockaddr_un *)sodupsockaddr(nam, M_WAITOK);

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	vp->v_socket = unp->unp_socket;
	unp->unp_vnode = vp;
	unp->unp_addr = soun;
	unp->unp_flags &= ~UNP_BINDING;
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);
	free(buf, M_TEMP);
	return (0);

error:
	VFS_UNLOCK_GIANT(vfslocked);
	UNP_PCB_LOCK(unp);
	unp->unp_flags &= ~UNP_BINDING;
	UNP_PCB_UNLOCK(unp);
	free(buf, M_TEMP);
	return (error);
}

static int
uipc_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error;

	KASSERT(td == curthread, ("uipc_connect: td != curthread"));
	UNP_GLOBAL_WLOCK();
	error = unp_connect(so, nam, td);
	UNP_GLOBAL_WUNLOCK();
	return (error);
}

static void
uipc_close(struct socket *so)
{
	struct unpcb *unp, *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_close: unp == NULL"));

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		UNP_PCB_LOCK(unp2);
		unp_disconnect(unp, unp2);
		UNP_PCB_UNLOCK(unp2);
	}
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
}

static int
uipc_connect2(struct socket *so1, struct socket *so2)
{
	struct unpcb *unp, *unp2;
	int error;

	UNP_GLOBAL_WLOCK();
	unp = so1->so_pcb;
	KASSERT(unp != NULL, ("uipc_connect2: unp == NULL"));
	UNP_PCB_LOCK(unp);
	unp2 = so2->so_pcb;
	KASSERT(unp2 != NULL, ("uipc_connect2: unp2 == NULL"));
	UNP_PCB_LOCK(unp2);
	error = unp_connect2(so1, so2, PRU_CONNECT2);
	UNP_PCB_UNLOCK(unp2);
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
	return (error);
}

static void
uipc_detach(struct socket *so)
{
	struct unpcb *unp, *unp2;
	struct sockaddr_un *saved_unp_addr;
	struct vnode *vp;
	int freeunp, local_unp_rights;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_detach: unp == NULL"));

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);

	LIST_REMOVE(unp, unp_link);
	unp->unp_gencnt = ++unp_gencnt;
	--unp_count;

	/*
	 * XXXRW: Should assert vp->v_socket == so.
	 */
	if ((vp = unp->unp_vnode) != NULL) {
		unp->unp_vnode->v_socket = NULL;
		unp->unp_vnode = NULL;
	}
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		UNP_PCB_LOCK(unp2);
		unp_disconnect(unp, unp2);
		UNP_PCB_UNLOCK(unp2);
	}

	/*
	 * We hold the global lock exclusively, so it's OK to acquire
	 * multiple pcb locks at a time.
	 */
	while (!LIST_EMPTY(&unp->unp_refs)) {
		struct unpcb *ref = LIST_FIRST(&unp->unp_refs);

		UNP_PCB_LOCK(ref);
		unp_drop(ref, ECONNRESET);
		UNP_PCB_UNLOCK(ref);
	}
	local_unp_rights = unp_rights;
	UNP_GLOBAL_WUNLOCK();
	unp->unp_socket->so_pcb = NULL;
	saved_unp_addr = unp->unp_addr;
	unp->unp_addr = NULL;
	unp->unp_refcount--;
	freeunp = (unp->unp_refcount == 0);
	if (saved_unp_addr != NULL)
		FREE(saved_unp_addr, M_SONAME);
	if (freeunp) {
		UNP_PCB_LOCK_DESTROY(unp);
		uma_zfree(unp_zone, unp);
	} else
		UNP_PCB_UNLOCK(unp);
	if (vp) {
		int vfslocked;

		vfslocked = VFS_LOCK_GIANT(vp->v_mount);
		vrele(vp);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	if (local_unp_rights)
		taskqueue_enqueue(taskqueue_thread, &unp_gc_task);
}

static int
uipc_disconnect(struct socket *so)
{
	struct unpcb *unp, *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_disconnect: unp == NULL"));

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		UNP_PCB_LOCK(unp2);
		unp_disconnect(unp, unp2);
		UNP_PCB_UNLOCK(unp2);
	}
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
	return (0);
}

static int
uipc_listen(struct socket *so, int backlog, struct thread *td)
{
	struct unpcb *unp;
	int error;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_listen: unp == NULL"));

	UNP_PCB_LOCK(unp);
	if (unp->unp_vnode == NULL) {
		UNP_PCB_UNLOCK(unp);
		return (EINVAL);
	}

	SOCK_LOCK(so);
	error = solisten_proto_check(so);
	if (error == 0) {
		cru2x(td->td_ucred, &unp->unp_peercred);
		unp->unp_flags |= UNP_HAVEPCCACHED;
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
	UNP_GLOBAL_RLOCK();
	/*
	 * XXX: It seems that this test always fails even when connection is
	 * established.  So, this else clause is added as workaround to
	 * return PF_LOCAL sockaddr.
	 */
	unp2 = unp->unp_conn;
	if (unp2 != NULL) {
		UNP_PCB_LOCK(unp2);
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
	UNP_GLOBAL_RUNLOCK();
	return (0);
}

static int
uipc_rcvd(struct socket *so, int flags)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;
	u_int mbcnt, sbcc;
	u_long newhiwat;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_rcvd: unp == NULL"));

	if (so->so_type == SOCK_DGRAM)
		panic("uipc_rcvd DGRAM?");

	if (so->so_type != SOCK_STREAM)
		panic("uipc_rcvd unknown socktype");

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
	sbcc = so->so_rcv.sb_cc;
	SOCKBUF_UNLOCK(&so->so_rcv);
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (unp2 == NULL) {
		UNP_PCB_UNLOCK(unp);
		return (0);
	}
	so2 = unp2->unp_socket;
	SOCKBUF_LOCK(&so2->so_snd);
	so2->so_snd.sb_mbmax += unp->unp_mbcnt - mbcnt;
	newhiwat = so2->so_snd.sb_hiwat + unp->unp_cc - sbcc;
	(void)chgsbsize(so2->so_cred->cr_uidinfo, &so2->so_snd.sb_hiwat,
	    newhiwat, RLIM_INFINITY);
	sowwakeup_locked(so2);
	unp->unp_mbcnt = mbcnt;
	unp->unp_cc = sbcc;
	UNP_PCB_UNLOCK(unp);
	return (0);
}

static int
uipc_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
    struct mbuf *control, struct thread *td)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;
	u_int mbcnt_delta, sbcc;
	u_long newhiwat;
	int error = 0;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_send: unp == NULL"));

	if (flags & PRUS_OOB) {
		error = EOPNOTSUPP;
		goto release;
	}
	if (control != NULL && (error = unp_internalize(&control, td)))
		goto release;
	if ((nam != NULL) || (flags & PRUS_EOF))
		UNP_GLOBAL_WLOCK();
	else
		UNP_GLOBAL_RLOCK();
	switch (so->so_type) {
	case SOCK_DGRAM:
	{
		const struct sockaddr *from;

		unp2 = unp->unp_conn;
		if (nam != NULL) {
			UNP_GLOBAL_WLOCK_ASSERT();
			if (unp2 != NULL) {
				error = EISCONN;
				break;
			}
			error = unp_connect(so, nam, td);
			if (error)
				break;
			unp2 = unp->unp_conn;
		}

		/*
		 * Because connect() and send() are non-atomic in a sendto()
		 * with a target address, it's possible that the socket will
		 * have disconnected before the send() can run.  In that case
		 * return the slightly counter-intuitive but otherwise
		 * correct error that the socket is not connected.
		 */
		if (unp2 == NULL) {
			error = ENOTCONN;
			break;
		}
		/* Lockless read. */
		if (unp2->unp_flags & UNP_WANTCRED)
			control = unp_addsockcred(td, control);
		UNP_PCB_LOCK(unp);
		if (unp->unp_addr != NULL)
			from = (struct sockaddr *)unp->unp_addr;
		else
			from = &sun_noname;
		so2 = unp2->unp_socket;
		SOCKBUF_LOCK(&so2->so_rcv);
		if (sbappendaddr_locked(&so2->so_rcv, from, m, control)) {
			sorwakeup_locked(so2);
			m = NULL;
			control = NULL;
		} else {
			SOCKBUF_UNLOCK(&so2->so_rcv);
			error = ENOBUFS;
		}
		if (nam != NULL) {
			UNP_GLOBAL_WLOCK_ASSERT();
			UNP_PCB_LOCK(unp2);
			unp_disconnect(unp, unp2);
			UNP_PCB_UNLOCK(unp2);
		}
		UNP_PCB_UNLOCK(unp);
		break;
	}

	case SOCK_STREAM:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (nam != NULL) {
				UNP_GLOBAL_WLOCK_ASSERT();
				error = unp_connect(so, nam, td);
				if (error)
					break;	/* XXX */
			} else {
				error = ENOTCONN;
				break;
			}
		}

		/* Lockless read. */
		if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
			error = EPIPE;
			break;
		}

		/*
		 * Because connect() and send() are non-atomic in a sendto()
		 * with a target address, it's possible that the socket will
		 * have disconnected before the send() can run.  In that case
		 * return the slightly counter-intuitive but otherwise
		 * correct error that the socket is not connected.
		 *
		 * Locking here must be done carefully: the global lock
		 * prevents interconnections between unpcbs from changing, so
		 * we can traverse from unp to unp2 without acquiring unp's
		 * lock.  Socket buffer locks follow unpcb locks, so we can
		 * acquire both remote and lock socket buffer locks.
		 */
		unp2 = unp->unp_conn;
		if (unp2 == NULL) {
			error = ENOTCONN;
			break;
		}
		so2 = unp2->unp_socket;
		UNP_PCB_LOCK(unp2);
		SOCKBUF_LOCK(&so2->so_rcv);
		if (unp2->unp_flags & UNP_WANTCRED) {
			/*
			 * Credentials are passed only once on SOCK_STREAM.
			 */
			unp2->unp_flags &= ~UNP_WANTCRED;
			control = unp_addsockcred(td, control);
		}
		/*
		 * Send to paired receive port, and then reduce send buffer
		 * hiwater marks to maintain backpressure.  Wake up readers.
		 */
		if (control != NULL) {
			if (sbappendcontrol_locked(&so2->so_rcv, m, control))
				control = NULL;
		} else
			sbappend_locked(&so2->so_rcv, m);
		mbcnt_delta = so2->so_rcv.sb_mbcnt - unp2->unp_mbcnt;
		unp2->unp_mbcnt = so2->so_rcv.sb_mbcnt;
		sbcc = so2->so_rcv.sb_cc;
		sorwakeup_locked(so2);

		SOCKBUF_LOCK(&so->so_snd);
		newhiwat = so->so_snd.sb_hiwat - (sbcc - unp2->unp_cc);
		(void)chgsbsize(so->so_cred->cr_uidinfo, &so->so_snd.sb_hiwat,
		    newhiwat, RLIM_INFINITY);
		so->so_snd.sb_mbmax -= mbcnt_delta;
		SOCKBUF_UNLOCK(&so->so_snd);
		unp2->unp_cc = sbcc;
		UNP_PCB_UNLOCK(unp2);
		m = NULL;
		break;

	default:
		panic("uipc_send unknown socktype");
	}

	/*
	 * PRUS_EOF is equivalent to pru_send followed by pru_shutdown.
	 */
	if (flags & PRUS_EOF) {
		UNP_PCB_LOCK(unp);
		socantsendmore(so);
		unp_shutdown(unp);
		UNP_PCB_UNLOCK(unp);
	}

	if ((nam != NULL) || (flags & PRUS_EOF))
		UNP_GLOBAL_WUNLOCK();
	else
		UNP_GLOBAL_RUNLOCK();

	if (control != NULL && error != 0)
		unp_dispose(control);

release:
	if (control != NULL)
		m_freem(control);
	if (m != NULL)
		m_freem(m);
	return (error);
}

static int
uipc_sense(struct socket *so, struct stat *sb)
{
	struct unpcb *unp, *unp2;
	struct socket *so2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_sense: unp == NULL"));

	sb->st_blksize = so->so_snd.sb_hiwat;
	UNP_GLOBAL_RLOCK();
	UNP_PCB_LOCK(unp);
	unp2 = unp->unp_conn;
	if (so->so_type == SOCK_STREAM && unp2 != NULL) {
		so2 = unp2->unp_socket;
		sb->st_blksize += so2->so_rcv.sb_cc;
	}
	sb->st_dev = NODEV;
	if (unp->unp_ino == 0)
		unp->unp_ino = (++unp_ino == 0) ? ++unp_ino : unp_ino;
	sb->st_ino = unp->unp_ino;
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_RUNLOCK();
	return (0);
}

static int
uipc_shutdown(struct socket *so)
{
	struct unpcb *unp;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("uipc_shutdown: unp == NULL"));

	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	socantsendmore(so);
	unp_shutdown(unp);
	UNP_PCB_UNLOCK(unp);
	UNP_GLOBAL_WUNLOCK();
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

static struct pr_usrreqs uipc_usrreqs_dgram = {
	.pru_abort = 		uipc_abort,
	.pru_accept =		uipc_accept,
	.pru_attach =		uipc_attach,
	.pru_bind =		uipc_bind,
	.pru_connect =		uipc_connect,
	.pru_connect2 =		uipc_connect2,
	.pru_detach =		uipc_detach,
	.pru_disconnect =	uipc_disconnect,
	.pru_listen =		uipc_listen,
	.pru_peeraddr =		uipc_peeraddr,
	.pru_rcvd =		uipc_rcvd,
	.pru_send =		uipc_send,
	.pru_sense =		uipc_sense,
	.pru_shutdown =		uipc_shutdown,
	.pru_sockaddr =		uipc_sockaddr,
	.pru_soreceive =	soreceive_dgram,
	.pru_close =		uipc_close,
};

static struct pr_usrreqs uipc_usrreqs_stream = {
	.pru_abort = 		uipc_abort,
	.pru_accept =		uipc_accept,
	.pru_attach =		uipc_attach,
	.pru_bind =		uipc_bind,
	.pru_connect =		uipc_connect,
	.pru_connect2 =		uipc_connect2,
	.pru_detach =		uipc_detach,
	.pru_disconnect =	uipc_disconnect,
	.pru_listen =		uipc_listen,
	.pru_peeraddr =		uipc_peeraddr,
	.pru_rcvd =		uipc_rcvd,
	.pru_send =		uipc_send,
	.pru_sense =		uipc_sense,
	.pru_shutdown =		uipc_shutdown,
	.pru_sockaddr =		uipc_sockaddr,
	.pru_soreceive =	soreceive_generic,
	.pru_close =		uipc_close,
};

static int
uipc_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct unpcb *unp;
	struct xucred xu;
	int error, optval;

	if (sopt->sopt_level != 0)
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
			optval = unp->unp_flags & UNP_WANTCRED ? 1 : 0;
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
		case LOCAL_CONNWAIT:
			error = sooptcopyin(sopt, &optval, sizeof(optval),
					    sizeof(optval));
			if (error)
				break;

#define	OPTSET(bit) do {						\
	UNP_PCB_LOCK(unp);						\
	if (optval)							\
		unp->unp_flags |= bit;					\
	else								\
		unp->unp_flags &= ~bit;					\
	UNP_PCB_UNLOCK(unp);						\
} while (0)

			switch (sopt->sopt_name) {
			case LOCAL_CREDS:
				OPTSET(UNP_WANTCRED);
				break;

			case LOCAL_CONNWAIT:
				OPTSET(UNP_CONNWAIT);
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
	struct sockaddr_un *soun = (struct sockaddr_un *)nam;
	struct vnode *vp;
	struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	int error, len, vfslocked;
	struct nameidata nd;
	char buf[SOCK_MAXADDRLEN];
	struct sockaddr *sa;

	UNP_GLOBAL_WLOCK_ASSERT();

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect: unp == NULL"));

	len = nam->sa_len - offsetof(struct sockaddr_un, sun_path);
	if (len <= 0)
		return (EINVAL);
	bcopy(soun->sun_path, buf, len);
	buf[len] = 0;

	UNP_PCB_LOCK(unp);
	if (unp->unp_flags & UNP_CONNECTING) {
		UNP_PCB_UNLOCK(unp);
		return (EALREADY);
	}
	UNP_GLOBAL_WUNLOCK();
	unp->unp_flags |= UNP_CONNECTING;
	UNP_PCB_UNLOCK(unp);

	sa = malloc(sizeof(struct sockaddr_un), M_SONAME, M_WAITOK);
	NDINIT(&nd, LOOKUP, MPSAFE | FOLLOW | LOCKLEAF, UIO_SYSSPACE, buf,
	    td);
	error = namei(&nd);
	if (error)
		vp = NULL;
	else
		vp = nd.ni_vp;
	ASSERT_VOP_LOCKED(vp, "unp_connect");
	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	if (error)
		goto bad;

	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
#ifdef MAC
	error = mac_check_vnode_open(td->td_ucred, vp, VWRITE | VREAD);
	if (error)
		goto bad;
#endif
	error = VOP_ACCESS(vp, VWRITE, td->td_ucred, td);
	if (error)
		goto bad;
	VFS_UNLOCK_GIANT(vfslocked);

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect: unp == NULL"));

	/*
	 * Lock global lock for two reasons: make sure v_socket is stable,
	 * and to protect simultaneous locking of multiple pcbs.
	 */
	UNP_GLOBAL_WLOCK();
	so2 = vp->v_socket;
	if (so2 == NULL) {
		error = ECONNREFUSED;
		goto bad2;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad2;
	}
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		if (so2->so_options & SO_ACCEPTCONN) {
			/*
			 * We can't drop the global lock here or 'so2' may
			 * become invalid.  As a result, we need to handle
			 * possibly lock recursion in uipc_attach.
			 */
			so3 = sonewconn(so2, 0);
		} else
			so3 = NULL;
		if (so3 == NULL) {
			error = ECONNREFUSED;
			goto bad2;
		}
		unp = sotounpcb(so);
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		UNP_PCB_LOCK(unp);
		UNP_PCB_LOCK(unp2);
		UNP_PCB_LOCK(unp3);
		if (unp2->unp_addr != NULL) {
			bcopy(unp2->unp_addr, sa, unp2->unp_addr->sun_len);
			unp3->unp_addr = (struct sockaddr_un *) sa;
			sa = NULL;
		}

		/*
		 * The connecter's (client's) credentials are copied from its
		 * process structure at the time of connect() (which is now).
		 */
		cru2x(td->td_ucred, &unp3->unp_peercred);
		unp3->unp_flags |= UNP_HAVEPC;

		/*
		 * The receiver's (server's) credentials are copied from the
		 * unp_peercred member of socket on which the former called
		 * listen(); uipc_listen() cached that process's credentials
		 * at that time so we can use them now.
		 */
		KASSERT(unp2->unp_flags & UNP_HAVEPCCACHED,
		    ("unp_connect: listener without cached peercred"));
		memcpy(&unp->unp_peercred, &unp2->unp_peercred,
		    sizeof(unp->unp_peercred));
		unp->unp_flags |= UNP_HAVEPC;
		if (unp2->unp_flags & UNP_WANTCRED)
			unp3->unp_flags |= UNP_WANTCRED;
		UNP_PCB_UNLOCK(unp3);
		UNP_PCB_UNLOCK(unp2);
		UNP_PCB_UNLOCK(unp);
#ifdef MAC
		SOCK_LOCK(so);
		mac_set_socket_peer_from_socket(so, so3);
		mac_set_socket_peer_from_socket(so3, so);
		SOCK_UNLOCK(so);
#endif

		so2 = so3;
	}
	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect: unp == NULL"));
	unp2 = sotounpcb(so2);
	KASSERT(unp2 != NULL, ("unp_connect: unp2 == NULL"));
	UNP_PCB_LOCK(unp);
	UNP_PCB_LOCK(unp2);
	error = unp_connect2(so, so2, PRU_CONNECT);
	UNP_PCB_UNLOCK(unp2);
	UNP_PCB_UNLOCK(unp);
bad2:
	UNP_GLOBAL_WUNLOCK();
	if (vfslocked)
		/* 
		 * Giant has been previously acquired. This means filesystem
		 * isn't MPSAFE.  Do it once again.
		 */
		mtx_lock(&Giant);
bad:
	if (vp != NULL)
		vput(vp);
	VFS_UNLOCK_GIANT(vfslocked);
	free(sa, M_SONAME);
	UNP_GLOBAL_WLOCK();
	UNP_PCB_LOCK(unp);
	unp->unp_flags &= ~UNP_CONNECTING;
	UNP_PCB_UNLOCK(unp);
	return (error);
}

static int
unp_connect2(struct socket *so, struct socket *so2, int req)
{
	struct unpcb *unp;
	struct unpcb *unp2;

	unp = sotounpcb(so);
	KASSERT(unp != NULL, ("unp_connect2: unp == NULL"));
	unp2 = sotounpcb(so2);
	KASSERT(unp2 != NULL, ("unp_connect2: unp2 == NULL"));

	UNP_GLOBAL_WLOCK_ASSERT();
	UNP_PCB_LOCK_ASSERT(unp);
	UNP_PCB_LOCK_ASSERT(unp2);

	if (so2->so_type != so->so_type)
		return (EPROTOTYPE);
	unp->unp_conn = unp2;

	switch (so->so_type) {
	case SOCK_DGRAM:
		LIST_INSERT_HEAD(&unp2->unp_refs, unp, unp_reflink);
		soisconnected(so);
		break;

	case SOCK_STREAM:
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
	return (0);
}

static void
unp_disconnect(struct unpcb *unp, struct unpcb *unp2)
{
	struct socket *so;

	KASSERT(unp2 != NULL, ("unp_disconnect: unp2 == NULL"));

	UNP_GLOBAL_WLOCK_ASSERT();
	UNP_PCB_LOCK_ASSERT(unp);
	UNP_PCB_LOCK_ASSERT(unp2);

	unp->unp_conn = NULL;
	switch (unp->unp_socket->so_type) {
	case SOCK_DGRAM:
		LIST_REMOVE(unp, unp_reflink);
		so = unp->unp_socket;
		SOCK_LOCK(so);
		so->so_state &= ~SS_ISCONNECTED;
		SOCK_UNLOCK(so);
		break;

	case SOCK_STREAM:
		soisdisconnected(unp->unp_socket);
		unp2->unp_conn = NULL;
		soisdisconnected(unp2->unp_socket);
		break;
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
	int error, i, n;
	int freeunp;
	struct unpcb *unp, **unp_list;
	unp_gen_t gencnt;
	struct xunpgen *xug;
	struct unp_head *head;
	struct xunpcb *xu;

	head = ((intptr_t)arg1 == SOCK_DGRAM ? &unp_dhead : &unp_shead);

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
	xug = malloc(sizeof(*xug), M_TEMP, M_WAITOK);
	UNP_GLOBAL_RLOCK();
	gencnt = unp_gencnt;
	n = unp_count;
	UNP_GLOBAL_RUNLOCK();

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

	UNP_GLOBAL_RLOCK();
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
			unp->unp_refcount++;
		}
		UNP_PCB_UNLOCK(unp);
	}
	UNP_GLOBAL_RUNLOCK();
	n = i;			/* In case we lost some during malloc. */

	error = 0;
	xu = malloc(sizeof(*xu), M_TEMP, M_WAITOK | M_ZERO);
	for (i = 0; i < n; i++) {
		unp = unp_list[i];
		UNP_PCB_LOCK(unp);
		unp->unp_refcount--;
	        if (unp->unp_refcount != 0 && unp->unp_gencnt <= gencnt) {
			xu->xu_len = sizeof *xu;
			xu->xu_unpp = unp;
			/*
			 * XXX - need more locking here to protect against
			 * connect/disconnect races for SMP.
			 */
			if (unp->unp_addr != NULL)
				bcopy(unp->unp_addr, &xu->xu_addr,
				      unp->unp_addr->sun_len);
			if (unp->unp_conn != NULL &&
			    unp->unp_conn->unp_addr != NULL)
				bcopy(unp->unp_conn->unp_addr,
				      &xu->xu_caddr,
				      unp->unp_conn->unp_addr->sun_len);
			bcopy(unp, &xu->xu_unp, sizeof *unp);
			sotoxsocket(unp->unp_socket, &xu->xu_socket);
			UNP_PCB_UNLOCK(unp);
			error = SYSCTL_OUT(req, xu, sizeof *xu);
		} else {
			freeunp = (unp->unp_refcount == 0);
			UNP_PCB_UNLOCK(unp);
			if (freeunp) {
				UNP_PCB_LOCK_DESTROY(unp);
				uma_zfree(unp_zone, unp);
			}
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

SYSCTL_PROC(_net_local_dgram, OID_AUTO, pcblist, CTLFLAG_RD,
	    (caddr_t)(long)SOCK_DGRAM, 0, unp_pcblist, "S,xunpcb",
	    "List of active local datagram sockets");
SYSCTL_PROC(_net_local_stream, OID_AUTO, pcblist, CTLFLAG_RD,
	    (caddr_t)(long)SOCK_STREAM, 0, unp_pcblist, "S,xunpcb",
	    "List of active local stream sockets");

static void
unp_shutdown(struct unpcb *unp)
{
	struct unpcb *unp2;
	struct socket *so;

	UNP_GLOBAL_WLOCK_ASSERT();
	UNP_PCB_LOCK_ASSERT(unp);

	unp2 = unp->unp_conn;
	if (unp->unp_socket->so_type == SOCK_STREAM && unp2 != NULL) {
		so = unp2->unp_socket;
		if (so != NULL)
			socantrcvmore(so);
	}
}

static void
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;
	struct unpcb *unp2;

	UNP_GLOBAL_WLOCK_ASSERT();
	UNP_PCB_LOCK_ASSERT(unp);

	so->so_error = errno;
	unp2 = unp->unp_conn;
	if (unp2 == NULL)
		return;
	UNP_PCB_LOCK(unp2);
	unp_disconnect(unp, unp2);
	UNP_PCB_UNLOCK(unp2);
}

static void
unp_freerights(struct file **rp, int fdcount)
{
	int i;
	struct file *fp;

	for (i = 0; i < fdcount; i++) {
		fp = *rp;
		*rp++ = NULL;
		unp_discard(fp);
	}
}

static int
unp_externalize(struct mbuf *control, struct mbuf **controlp)
{
	struct thread *td = curthread;		/* XXX */
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	int i;
	int *fdp;
	struct file **rp;
	struct file *fp;
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error, newfds;
	int f;
	u_int newlen;

	UNP_GLOBAL_UNLOCK_ASSERT();

	error = 0;
	if (controlp != NULL) /* controlp == NULL => free control messages */
		*controlp = NULL;
	while (cm != NULL) {
		if (sizeof(*cm) > clen || cm->cmsg_len > clen) {
			error = EINVAL;
			break;
		}
		data = CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;
		if (cm->cmsg_level == SOL_SOCKET
		    && cm->cmsg_type == SCM_RIGHTS) {
			newfds = datalen / sizeof(struct file *);
			rp = data;

			/* If we're not outputting the descriptors free them. */
			if (error || controlp == NULL) {
				unp_freerights(rp, newfds);
				goto next;
			}
			FILEDESC_XLOCK(td->td_proc->p_fd);
			/* if the new FD's will not fit free them.  */
			if (!fdavail(td, newfds)) {
				FILEDESC_XUNLOCK(td->td_proc->p_fd);
				error = EMSGSIZE;
				unp_freerights(rp, newfds);
				goto next;
			}

			/*
			 * Now change each pointer to an fd in the global
			 * table to an integer that is the index to the local
			 * fd table entry that we set up to point to the
			 * global one we are transferring.
			 */
			newlen = newfds * sizeof(int);
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET);
			if (*controlp == NULL) {
				FILEDESC_XUNLOCK(td->td_proc->p_fd);
				error = E2BIG;
				unp_freerights(rp, newfds);
				goto next;
			}

			fdp = (int *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			for (i = 0; i < newfds; i++) {
				if (fdalloc(td, 0, &f))
					panic("unp_externalize fdalloc failed");
				fp = *rp++;
				td->td_proc->p_fd->fd_ofiles[f] = fp;
				FILE_LOCK(fp);
				fp->f_msgcount--;
				FILE_UNLOCK(fp);
				UNP_GLOBAL_WLOCK();
				unp_rights--;
				UNP_GLOBAL_WUNLOCK();
				*fdp++ = f;
			}
			FILEDESC_XUNLOCK(td->td_proc->p_fd);
		} else {
			/* We can just copy anything else across. */
			if (error || controlp == NULL)
				goto next;
			*controlp = sbcreatecontrol(NULL, datalen,
			    cm->cmsg_type, cm->cmsg_level);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto next;
			}
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

static void
unp_init(void)
{

	unp_zone = uma_zcreate("unpcb", sizeof(struct unpcb), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	if (unp_zone == NULL)
		panic("unp_init");
	uma_zone_set_max(unp_zone, maxsockets);
	EVENTHANDLER_REGISTER(maxsockets_change, unp_zone_change,
	    NULL, EVENTHANDLER_PRI_ANY);
	LIST_INIT(&unp_dhead);
	LIST_INIT(&unp_shead);
	TASK_INIT(&unp_gc_task, 0, unp_gc, NULL);
	UNP_GLOBAL_LOCK_INIT();
}

static int
unp_internalize(struct mbuf **controlp, struct thread *td)
{
	struct mbuf *control = *controlp;
	struct proc *p = td->td_proc;
	struct filedesc *fdescp = p->p_fd;
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	struct cmsgcred *cmcred;
	struct file **rp;
	struct file *fp;
	struct timeval *tv;
	int i, fd, *fdp;
	void *data;
	socklen_t clen = control->m_len, datalen;
	int error, oldfds;
	u_int newlen;

	UNP_GLOBAL_UNLOCK_ASSERT();

	error = 0;
	*controlp = NULL;
	while (cm != NULL) {
		if (sizeof(*cm) > clen || cm->cmsg_level != SOL_SOCKET
		    || cm->cmsg_len > clen) {
			error = EINVAL;
			goto out;
		}
		data = CMSG_DATA(cm);
		datalen = (caddr_t)cm + cm->cmsg_len - (caddr_t)data;

		switch (cm->cmsg_type) {
		/*
		 * Fill in credential information.
		 */
		case SCM_CREDS:
			*controlp = sbcreatecontrol(NULL, sizeof(*cmcred),
			    SCM_CREDS, SOL_SOCKET);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto out;
			}
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
			/*
			 * Check that all the FDs passed in refer to legal
			 * files.  If not, reject the entire operation.
			 */
			fdp = data;
			FILEDESC_SLOCK(fdescp);
			for (i = 0; i < oldfds; i++) {
				fd = *fdp++;
				if ((unsigned)fd >= fdescp->fd_nfiles ||
				    fdescp->fd_ofiles[fd] == NULL) {
					FILEDESC_SUNLOCK(fdescp);
					error = EBADF;
					goto out;
				}
				fp = fdescp->fd_ofiles[fd];
				if (!(fp->f_ops->fo_flags & DFLAG_PASSABLE)) {
					FILEDESC_SUNLOCK(fdescp);
					error = EOPNOTSUPP;
					goto out;
				}

			}

			/*
			 * Now replace the integer FDs with pointers to the
			 * associated global file table entry..
			 */
			newlen = oldfds * sizeof(struct file *);
			*controlp = sbcreatecontrol(NULL, newlen,
			    SCM_RIGHTS, SOL_SOCKET);
			if (*controlp == NULL) {
				FILEDESC_SUNLOCK(fdescp);
				error = E2BIG;
				goto out;
			}
			fdp = data;
			rp = (struct file **)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			for (i = 0; i < oldfds; i++) {
				fp = fdescp->fd_ofiles[*fdp++];
				*rp++ = fp;
				FILE_LOCK(fp);
				fp->f_count++;
				fp->f_msgcount++;
				FILE_UNLOCK(fp);
				UNP_GLOBAL_WLOCK();
				unp_rights++;
				UNP_GLOBAL_WUNLOCK();
			}
			FILEDESC_SUNLOCK(fdescp);
			break;

		case SCM_TIMESTAMP:
			*controlp = sbcreatecontrol(NULL, sizeof(*tv),
			    SCM_TIMESTAMP, SOL_SOCKET);
			if (*controlp == NULL) {
				error = ENOBUFS;
				goto out;
			}
			tv = (struct timeval *)
			    CMSG_DATA(mtod(*controlp, struct cmsghdr *));
			microtime(tv);
			break;

		default:
			error = EINVAL;
			goto out;
		}

		controlp = &(*controlp)->m_next;
		if (CMSG_SPACE(datalen) < clen) {
			clen -= CMSG_SPACE(datalen);
			cm = (struct cmsghdr *)
			    ((caddr_t)cm + CMSG_SPACE(datalen));
		} else {
			clen = 0;
			cm = NULL;
		}
	}

out:
	m_freem(control);
	return (error);
}

static struct mbuf *
unp_addsockcred(struct thread *td, struct mbuf *control)
{
	struct mbuf *m, *n, *n_prev;
	struct sockcred *sc;
	const struct cmsghdr *cm;
	int ngroups;
	int i;

	ngroups = MIN(td->td_ucred->cr_ngroups, CMGROUP_MAX);
	m = sbcreatecontrol(NULL, SOCKCREDSIZE(ngroups), SCM_CREDS, SOL_SOCKET);
	if (m == NULL)
		return (control);

	sc = (struct sockcred *) CMSG_DATA(mtod(m, struct cmsghdr *));
	sc->sc_uid = td->td_ucred->cr_ruid;
	sc->sc_euid = td->td_ucred->cr_uid;
	sc->sc_gid = td->td_ucred->cr_rgid;
	sc->sc_egid = td->td_ucred->cr_gid;
	sc->sc_ngroups = ngroups;
	for (i = 0; i < sc->sc_ngroups; i++)
		sc->sc_groups[i] = td->td_ucred->cr_groups[i];

	/*
	 * Unlink SCM_CREDS control messages (struct cmsgcred), since just
	 * created SCM_CREDS control message (struct sockcred) has another
	 * format.
	 */
	if (control != NULL)
		for (n = control, n_prev = NULL; n != NULL;) {
			cm = mtod(n, struct cmsghdr *);
    			if (cm->cmsg_level == SOL_SOCKET &&
			    cm->cmsg_type == SCM_CREDS) {
    				if (n_prev == NULL)
					control = n->m_next;
				else
					n_prev->m_next = n->m_next;
				n = m_free(n);
			} else {
				n_prev = n;
				n = n->m_next;
			}
		}

	/* Prepend it to the head. */
	m->m_next = control;
	return (m);
}

/*
 * unp_defer indicates whether additional work has been defered for a future
 * pass through unp_gc().  It is thread local and does not require explicit
 * synchronization.
 */
static int	unp_defer;

static int unp_taskcount;
SYSCTL_INT(_net_local, OID_AUTO, taskcount, CTLFLAG_RD, &unp_taskcount, 0, "");

static int unp_recycled;
SYSCTL_INT(_net_local, OID_AUTO, recycled, CTLFLAG_RD, &unp_recycled, 0, "");

static void
unp_gc(__unused void *arg, int pending)
{
	struct file *fp, *nextfp;
	struct socket *so;
	struct socket *soa;
	struct file **extra_ref, **fpp;
	int nunref, i;
	int nfiles_snap;
	int nfiles_slack = 20;

	unp_taskcount++;
	unp_defer = 0;

	/*
	 * Before going through all this, set all FDs to be NOT deferred and
	 * NOT externally accessible.
	 */
	sx_slock(&filelist_lock);
	LIST_FOREACH(fp, &filehead, f_list)
		fp->f_gcflag &= ~(FMARK|FDEFER);
	do {
		KASSERT(unp_defer >= 0, ("unp_gc: unp_defer %d", unp_defer));
		LIST_FOREACH(fp, &filehead, f_list) {
			FILE_LOCK(fp);
			/*
			 * If the file is not open, skip it -- could be a
			 * file in the process of being opened, or in the
			 * process of being closed.  If the file is
			 * "closing", it may have been marked for deferred
			 * consideration.  Clear the flag now if so.
			 */
			if (fp->f_count == 0) {
				if (fp->f_gcflag & FDEFER)
					unp_defer--;
				fp->f_gcflag &= ~(FMARK|FDEFER);
				FILE_UNLOCK(fp);
				continue;
			}

			/*
			 * If we already marked it as 'defer' in a
			 * previous pass, then try to process it this
			 * time and un-mark it.
			 */
			if (fp->f_gcflag & FDEFER) {
				fp->f_gcflag &= ~FDEFER;
				unp_defer--;
			} else {
				/*
				 * If it's not deferred, then check if it's
				 * already marked.. if so skip it
				 */
				if (fp->f_gcflag & FMARK) {
					FILE_UNLOCK(fp);
					continue;
				}

				/*
				 * If all references are from messages in
				 * transit, then skip it. it's not externally
				 * accessible.
				 */
				if (fp->f_count == fp->f_msgcount) {
					FILE_UNLOCK(fp);
					continue;
				}

				/*
				 * If it got this far then it must be
				 * externally accessible.
				 */
				fp->f_gcflag |= FMARK;
			}

			/*
			 * Either it was deferred, or it is externally
			 * accessible and not already marked so.  Now check
			 * if it is possibly one of OUR sockets.
			 */
			if (fp->f_type != DTYPE_SOCKET ||
			    (so = fp->f_data) == NULL) {
				FILE_UNLOCK(fp);
				continue;
			}

			if (so->so_proto->pr_domain != &localdomain ||
			    (so->so_proto->pr_flags & PR_RIGHTS) == 0) {
				FILE_UNLOCK(fp);				
				continue;
			}

			/*
			 * Tell any other threads that do a subsequent
			 * fdrop() that we are scanning the message
			 * buffers.
			 */
			fp->f_gcflag |= FWAIT;
			FILE_UNLOCK(fp);

			/*
			 * So, Ok, it's one of our sockets and it IS
			 * externally accessible (or was deferred).  Now we
			 * look to see if we hold any file descriptors in its
			 * message buffers. Follow those links and mark them
			 * as accessible too.
			 */
			SOCKBUF_LOCK(&so->so_rcv);
			unp_scan(so->so_rcv.sb_mb, unp_mark);
			SOCKBUF_UNLOCK(&so->so_rcv);

			/*
			 * If socket is in listening state, then sockets
			 * in its accept queue are accessible, and so
			 * are any descriptors in those sockets' receive
			 * queues.
			 */
			ACCEPT_LOCK();
			TAILQ_FOREACH(soa, &so->so_comp, so_list) {
			    SOCKBUF_LOCK(&soa->so_rcv);
			    unp_scan(soa->so_rcv.sb_mb, unp_mark);
			    SOCKBUF_UNLOCK(&soa->so_rcv);
			}
			ACCEPT_UNLOCK();

			/*
			 * Wake up any threads waiting in fdrop().
			 */
			FILE_LOCK(fp);
			fp->f_gcflag &= ~FWAIT;
			wakeup(&fp->f_gcflag);
			FILE_UNLOCK(fp);
		}
	} while (unp_defer);
	sx_sunlock(&filelist_lock);

	/*
	 * XXXRW: The following comments need updating for a post-SMPng and
	 * deferred unp_gc() world, but are still generally accurate.
	 *
	 * We grab an extra reference to each of the file table entries that
	 * are not otherwise accessible and then free the rights that are
	 * stored in messages on them.
	 *
	 * The bug in the orginal code is a little tricky, so I'll describe
	 * what's wrong with it here.
	 *
	 * It is incorrect to simply unp_discard each entry for f_msgcount
	 * times -- consider the case of sockets A and B that contain
	 * references to each other.  On a last close of some other socket,
	 * we trigger a gc since the number of outstanding rights (unp_rights)
	 * is non-zero.  If during the sweep phase the gc code unp_discards,
	 * we end up doing a (full) closef on the descriptor.  A closef on A
	 * results in the following chain.  Closef calls soo_close, which
	 * calls soclose.   Soclose calls first (through the switch
	 * uipc_usrreq) unp_detach, which re-invokes unp_gc.  Unp_gc simply
	 * returns because the previous instance had set unp_gcing, and we
	 * return all the way back to soclose, which marks the socket with
	 * SS_NOFDREF, and then calls sofree.  Sofree calls sorflush to free
	 * up the rights that are queued in messages on the socket A, i.e.,
	 * the reference on B.  The sorflush calls via the dom_dispose switch
	 * unp_dispose, which unp_scans with unp_discard.  This second
	 * instance of unp_discard just calls closef on B.
	 *
	 * Well, a similar chain occurs on B, resulting in a sorflush on B,
	 * which results in another closef on A.  Unfortunately, A is already
	 * being closed, and the descriptor has already been marked with
	 * SS_NOFDREF, and soclose panics at this point.
	 *
	 * Here, we first take an extra reference to each inaccessible
	 * descriptor.  Then, we call sorflush ourself, since we know it is a
	 * Unix domain socket anyhow.  After we destroy all the rights
	 * carried in messages, we do a last closef to get rid of our extra
	 * reference.  This is the last close, and the unp_detach etc will
	 * shut down the socket.
	 *
	 * 91/09/19, bsy@cs.cmu.edu
	 */
again:
	nfiles_snap = openfiles + nfiles_slack;	/* some slack */
	extra_ref = malloc(nfiles_snap * sizeof(struct file *), M_TEMP,
	    M_WAITOK);
	sx_slock(&filelist_lock);
	if (nfiles_snap < openfiles) {
		sx_sunlock(&filelist_lock);
		free(extra_ref, M_TEMP);
		nfiles_slack += 20;
		goto again;
	}
	for (nunref = 0, fp = LIST_FIRST(&filehead), fpp = extra_ref;
	    fp != NULL; fp = nextfp) {
		nextfp = LIST_NEXT(fp, f_list);
		FILE_LOCK(fp);

		/*
		 * If it's not open, skip it
		 */
		if (fp->f_count == 0) {
			FILE_UNLOCK(fp);
			continue;
		}

		/*
		 * If all refs are from msgs, and it's not marked accessible
		 * then it must be referenced from some unreachable cycle of
		 * (shut-down) FDs, so include it in our list of FDs to
		 * remove.
		 */
		if (fp->f_count == fp->f_msgcount && !(fp->f_gcflag & FMARK)) {
			*fpp++ = fp;
			nunref++;
			fp->f_count++;
		}
		FILE_UNLOCK(fp);
	}
	sx_sunlock(&filelist_lock);

	/*
	 * For each FD on our hit list, do the following two things:
	 */
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp) {
		struct file *tfp = *fpp;
		FILE_LOCK(tfp);
		if (tfp->f_type == DTYPE_SOCKET &&
		    tfp->f_data != NULL) {
			FILE_UNLOCK(tfp);
			sorflush(tfp->f_data);
		} else {
			FILE_UNLOCK(tfp);
		}
	}
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp) {
		closef(*fpp, (struct thread *) NULL);
		unp_recycled++;
	}
	free(extra_ref, M_TEMP);
}

static void
unp_dispose(struct mbuf *m)
{

	if (m)
		unp_scan(m, unp_discard);
}

static void
unp_scan(struct mbuf *m0, void (*op)(struct file *))
{
	struct mbuf *m;
	struct file **rp;
	struct cmsghdr *cm;
	void *data;
	int i;
	socklen_t clen, datalen;
	int qfds;

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
					qfds = datalen / sizeof (struct file *);
					rp = data;
					for (i = 0; i < qfds; i++)
						(*op)(*rp++);
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
		m0 = m0->m_act;
	}
}

static void
unp_mark(struct file *fp)
{

	/* XXXRW: Should probably assert file list lock here. */

	if (fp->f_gcflag & FMARK)
		return;
	unp_defer++;
	fp->f_gcflag |= (FMARK|FDEFER);
}

static void
unp_discard(struct file *fp)
{

	UNP_GLOBAL_WLOCK();
	FILE_LOCK(fp);
	fp->f_msgcount--;
	unp_rights--;
	FILE_UNLOCK(fp);
	UNP_GLOBAL_WUNLOCK();
	(void) closef(fp, (struct thread *)NULL);
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
	if (unp_flags & UNP_HAVEPCCACHED) {
		db_printf("%sUNP_HAVEPCCACHED", comma ? ", " : "");
		comma = 1;
	}
	if (unp_flags & UNP_WANTCRED) {
		db_printf("%sUNP_WANTCRED", comma ? ", " : "");
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
	db_printf("cr_version: %u   cr_uid: %u   cr_ngroups: %d\n",
	    xu->cr_version, xu->cr_uid, xu->cr_ngroups);
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

	db_printf("unp_ino: %d   unp_conn: %p\n", unp->unp_ino,
	    unp->unp_conn);

	db_printf("unp_refs:\n");
	db_print_unprefs(2, &unp->unp_refs);

	/* XXXRW: Would be nice to print the full address, if any. */
	db_printf("unp_addr: %p\n", unp->unp_addr);

	db_printf("unp_cc: %d   unp_mbcnt: %d   unp_gencnt: %llu\n",
	    unp->unp_cc, unp->unp_mbcnt,
	    (unsigned long long)unp->unp_gencnt);

	db_printf("unp_flags: %x (", unp->unp_flags);
	db_print_unpflags(unp->unp_flags);
	db_printf(")\n");

	db_printf("unp_peercred:\n");
	db_print_xucred(2, &unp->unp_peercred);

	db_printf("unp_refcount: %u\n", unp->unp_refcount);
}
#endif
