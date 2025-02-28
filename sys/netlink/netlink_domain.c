/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
 * Copyright (c) 2023 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file contains socket and protocol bindings for netlink.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/domain.h>
#include <sys/jail.h>
#include <sys/mbuf.h>
#include <sys/osd.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/ck.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/priv.h>
#include <sys/uio.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>

#define	DEBUG_MOD_NAME	nl_domain
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

_Static_assert((NLP_MAX_GROUPS % 64) == 0,
    "NLP_MAX_GROUPS has to be multiple of 64");
_Static_assert(NLP_MAX_GROUPS >= 64,
    "NLP_MAX_GROUPS has to be at least 64");

#define	NLCTL_TRACKER		struct rm_priotracker nl_tracker
#define	NLCTL_RLOCK()		rm_rlock(&V_nl_ctl.ctl_lock, &nl_tracker)
#define	NLCTL_RUNLOCK()		rm_runlock(&V_nl_ctl.ctl_lock, &nl_tracker)
#define	NLCTL_LOCK_ASSERT()	rm_assert(&V_nl_ctl.ctl_lock, RA_LOCKED)

#define	NLCTL_WLOCK()		rm_wlock(&V_nl_ctl.ctl_lock)
#define	NLCTL_WUNLOCK()		rm_wunlock(&V_nl_ctl.ctl_lock)
#define	NLCTL_WLOCK_ASSERT()	rm_assert(&V_nl_ctl.ctl_lock, RA_WLOCKED)

static u_long nl_sendspace = NLSNDQ;
SYSCTL_ULONG(_net_netlink, OID_AUTO, sendspace, CTLFLAG_RW, &nl_sendspace, 0,
    "Default netlink socket send space");

static u_long nl_recvspace = NLSNDQ;
SYSCTL_ULONG(_net_netlink, OID_AUTO, recvspace, CTLFLAG_RW, &nl_recvspace, 0,
    "Default netlink socket receive space");

extern u_long sb_max_adj;
static u_long nl_maxsockbuf = 512 * 1024 * 1024; /* 512M, XXX: init based on physmem */
static int sysctl_handle_nl_maxsockbuf(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_net_netlink, OID_AUTO, nl_maxsockbuf,
    CTLTYPE_ULONG | CTLFLAG_RW | CTLFLAG_MPSAFE, &nl_maxsockbuf, 0,
    sysctl_handle_nl_maxsockbuf, "LU",
    "Maximum Netlink socket buffer size");


static unsigned int osd_slot_id = 0;

void
nl_osd_register(void)
{
	osd_slot_id = osd_register(OSD_THREAD, NULL, NULL);
}

void
nl_osd_unregister(void)
{
	osd_deregister(OSD_THREAD, osd_slot_id);
}

struct nlpcb *
_nl_get_thread_nlp(struct thread *td)
{
	return (osd_get(OSD_THREAD, &td->td_osd, osd_slot_id));
}

void
nl_set_thread_nlp(struct thread *td, struct nlpcb *nlp)
{
	NLP_LOG(LOG_DEBUG2, nlp, "Set thread %p nlp to %p (slot %u)", td, nlp, osd_slot_id);
	if (osd_set(OSD_THREAD, &td->td_osd, osd_slot_id, nlp) == 0)
		return;
	/* Failed, need to realloc */
	void **rsv = osd_reserve(osd_slot_id);
	osd_set_reserved(OSD_THREAD, &td->td_osd, osd_slot_id, rsv, nlp);
}

/*
 * Looks up a nlpcb struct based on the @portid. Need to claim nlsock_mtx.
 * Returns nlpcb pointer if present else NULL
 */
static struct nlpcb *
nl_port_lookup(uint32_t port_id)
{
	struct nlpcb *nlp;

	CK_LIST_FOREACH(nlp, &V_nl_ctl.ctl_port_head, nl_port_next) {
		if (nlp->nl_port == port_id)
			return (nlp);
	}
	return (NULL);
}

static void
nlp_join_group(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id < NLP_MAX_GROUPS);
	NLCTL_WLOCK_ASSERT();

	/* TODO: add family handler callback */
	if (!nlp_unconstrained_vnet(nlp))
		return;

	BIT_SET(NLP_MAX_GROUPS, group_id, &nlp->nl_groups);
}

static void
nlp_leave_group(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id < NLP_MAX_GROUPS);
	NLCTL_WLOCK_ASSERT();

	BIT_CLR(NLP_MAX_GROUPS, group_id, &nlp->nl_groups);
}

static bool
nlp_memberof_group(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id < NLP_MAX_GROUPS);
	NLCTL_LOCK_ASSERT();

	return (BIT_ISSET(NLP_MAX_GROUPS, group_id, &nlp->nl_groups));
}

static uint32_t
nlp_get_groups_compat(struct nlpcb *nlp)
{
	uint32_t groups_mask = 0;

	NLCTL_LOCK_ASSERT();

	for (int i = 0; i < 32; i++) {
		if (nlp_memberof_group(nlp, i + 1))
			groups_mask |= (1 << i);
	}

	return (groups_mask);
}

static struct nl_buf *
nl_buf_copy(struct nl_buf *nb)
{
	struct nl_buf *copy;

	copy = nl_buf_alloc(nb->buflen, M_NOWAIT);
	if (__predict_false(copy == NULL))
		return (NULL);
	memcpy(copy, nb, sizeof(*nb) + nb->buflen);

	return (copy);
}

/*
 * Broadcasts in the writer's buffer.
 */
bool
nl_send_group(struct nl_writer *nw)
{
	struct nl_buf *nb = nw->buf;
	struct nlpcb *nlp_last = NULL;
	struct nlpcb *nlp;
	NLCTL_TRACKER;

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		struct nlmsghdr *hdr = (struct nlmsghdr *)nb->data;
		NL_LOG(LOG_DEBUG2, "MCAST len %u msg type %d len %u to group %d/%d",
		    nb->datalen, hdr->nlmsg_type, hdr->nlmsg_len,
		    nw->group.proto, nw->group.id);
	}

	nw->buf = NULL;

	NLCTL_RLOCK();
	CK_LIST_FOREACH(nlp, &V_nl_ctl.ctl_pcb_head, nl_next) {
		if ((nw->group.priv == 0 || priv_check_cred(
		    nlp->nl_socket->so_cred, nw->group.priv) == 0) &&
		    nlp->nl_proto == nw->group.proto &&
		    nlp_memberof_group(nlp, nw->group.id)) {
			if (nlp_last != NULL) {
				struct nl_buf *copy;

				copy = nl_buf_copy(nb);
				if (copy != NULL) {
					nw->buf = copy;
					(void)nl_send(nw, nlp_last);
				} else {
					NLP_LOCK(nlp_last);
					if (nlp_last->nl_socket != NULL)
						sorwakeup(nlp_last->nl_socket);
					NLP_UNLOCK(nlp_last);
				}
			}
			nlp_last = nlp;
		}
	}
	if (nlp_last != NULL) {
		nw->buf = nb;
		(void)nl_send(nw, nlp_last);
	} else
		nl_buf_free(nb);

	NLCTL_RUNLOCK();

	return (true);
}

void
nl_clear_group(u_int group)
{
	struct nlpcb *nlp;

	NLCTL_WLOCK();
	CK_LIST_FOREACH(nlp, &V_nl_ctl.ctl_pcb_head, nl_next)
		if (nlp_memberof_group(nlp, group))
			nlp_leave_group(nlp, group);
	NLCTL_WUNLOCK();
}

static uint32_t
nl_find_port(void)
{
	/*
	 * app can open multiple netlink sockets.
	 * Start with current pid, if already taken,
	 * try random numbers in 65k..256k+65k space,
	 * avoiding clash with pids.
	 */
	if (nl_port_lookup(curproc->p_pid) == NULL)
		return (curproc->p_pid);
	for (int i = 0; i < 16; i++) {
		uint32_t nl_port = (arc4random() % 65536) + 65536 * 4;
		if (nl_port_lookup(nl_port) == 0)
			return (nl_port);
		NL_LOG(LOG_DEBUG3, "tried %u\n", nl_port);
	}
	return (curproc->p_pid);
}

static int
nl_bind_locked(struct nlpcb *nlp, struct sockaddr_nl *snl)
{
	if (nlp->nl_bound) {
		if (nlp->nl_port != snl->nl_pid) {
			NL_LOG(LOG_DEBUG,
			    "bind() failed: program pid %d "
			    "is different from provided pid %d",
			    nlp->nl_port, snl->nl_pid);
			return (EINVAL); // XXX: better error
		}
	} else {
		if (snl->nl_pid == 0)
			snl->nl_pid = nl_find_port();
		if (nl_port_lookup(snl->nl_pid) != NULL)
			return (EADDRINUSE);
		nlp->nl_port = snl->nl_pid;
		nlp->nl_bound = true;
		CK_LIST_INSERT_HEAD(&V_nl_ctl.ctl_port_head, nlp, nl_port_next);
	}
	for (int i = 0; i < 32; i++) {
		if (snl->nl_groups & ((uint32_t)1 << i))
			nlp_join_group(nlp, i + 1);
		else
			nlp_leave_group(nlp, i + 1);
	}

	return (0);
}

static int
nl_attach(struct socket *so, int proto, struct thread *td)
{
	struct nlpcb *nlp;
	int error;

	if (__predict_false(netlink_unloading != 0))
		return (EAFNOSUPPORT);

	error = nl_verify_proto(proto);
	if (error != 0)
		return (error);

	bool is_linux = SV_PROC_ABI(td->td_proc) == SV_ABI_LINUX;
	NL_LOG(LOG_DEBUG2, "socket %p, %sPID %d: attaching socket to %s",
	    so, is_linux ? "(linux) " : "", curproc->p_pid,
	    nl_get_proto_name(proto));

	nlp = malloc(sizeof(struct nlpcb), M_PCB, M_WAITOK | M_ZERO);
	error = soreserve(so, nl_sendspace, nl_recvspace);
	if (error != 0) {
		free(nlp, M_PCB);
		return (error);
	}
	TAILQ_INIT(&so->so_rcv.nl_queue);
	TAILQ_INIT(&so->so_snd.nl_queue);
	so->so_pcb = nlp;
	nlp->nl_socket = so;
	nlp->nl_proto = proto;
	nlp->nl_process_id = curproc->p_pid;
	nlp->nl_linux = is_linux;
	nlp->nl_unconstrained_vnet = !jailed_without_vnet(so->so_cred);
	nlp->nl_need_thread_setup = true;
	NLP_LOCK_INIT(nlp);
	refcount_init(&nlp->nl_refcount, 1);

	nlp->nl_taskqueue = taskqueue_create("netlink_socket", M_WAITOK,
	    taskqueue_thread_enqueue, &nlp->nl_taskqueue);
	TASK_INIT(&nlp->nl_task, 0, nl_taskqueue_handler, nlp);
	taskqueue_start_threads(&nlp->nl_taskqueue, 1, PWAIT,
	    "netlink_socket (PID %u)", nlp->nl_process_id);

	NLCTL_WLOCK();
	CK_LIST_INSERT_HEAD(&V_nl_ctl.ctl_pcb_head, nlp, nl_next);
	NLCTL_WUNLOCK();

	soisconnected(so);

	return (0);
}

static int
nl_bind(struct socket *so, struct sockaddr *sa, struct thread *td)
{
	struct nlpcb *nlp = sotonlpcb(so);
	struct sockaddr_nl *snl = (struct sockaddr_nl *)sa;
	int error;

	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	if (snl->nl_len != sizeof(*snl)) {
		NL_LOG(LOG_DEBUG, "socket %p, wrong sizeof(), ignoring bind()", so);
		return (EINVAL);
	}


	NLCTL_WLOCK();
	NLP_LOCK(nlp);
	error = nl_bind_locked(nlp, snl);
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK();
	NL_LOG(LOG_DEBUG2, "socket %p, bind() to %u, groups %u, error %d", so,
	    snl->nl_pid, snl->nl_groups, error);

	return (error);
}


static int
nl_assign_port(struct nlpcb *nlp, uint32_t port_id)
{
	struct sockaddr_nl snl = {
		.nl_pid = port_id,
	};
	int error;

	NLCTL_WLOCK();
	NLP_LOCK(nlp);
	snl.nl_groups = nlp_get_groups_compat(nlp);
	error = nl_bind_locked(nlp, &snl);
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK();

	NL_LOG(LOG_DEBUG3, "socket %p, port assign: %d, error: %d", nlp->nl_socket, port_id, error);
	return (error);
}

/*
 * nl_autobind_port binds a unused portid to @nlp
 * @nlp: pcb data for the netlink socket
 * @candidate_id: first id to consider
 */
static int
nl_autobind_port(struct nlpcb *nlp, uint32_t candidate_id)
{
	uint32_t port_id = candidate_id;
	NLCTL_TRACKER;
	bool exist;
	int error = EADDRINUSE;

	for (int i = 0; i < 10; i++) {
		NL_LOG(LOG_DEBUG3, "socket %p, trying to assign port %d", nlp->nl_socket, port_id);
		NLCTL_RLOCK();
		exist = nl_port_lookup(port_id) != 0;
		NLCTL_RUNLOCK();
		if (!exist) {
			error = nl_assign_port(nlp, port_id);
			if (error != EADDRINUSE)
				break;
		}
		port_id++;
	}
	NL_LOG(LOG_DEBUG3, "socket %p, autobind to %d, error: %d", nlp->nl_socket, port_id, error);
	return (error);
}

static int
nl_connect(struct socket *so, struct sockaddr *sa, struct thread *td)
{
	struct sockaddr_nl *snl = (struct sockaddr_nl *)sa;
	struct nlpcb *nlp;

	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	if (snl->nl_len != sizeof(*snl)) {
		NL_LOG(LOG_DEBUG, "socket %p, wrong sizeof(), ignoring bind()", so);
		return (EINVAL);
	}

	nlp = sotonlpcb(so);
	if (!nlp->nl_bound) {
		int error = nl_autobind_port(nlp, td->td_proc->p_pid);
		if (error != 0) {
			NL_LOG(LOG_DEBUG, "socket %p, nl_autobind() failed: %d", so, error);
			return (error);
		}
	}
	/* XXX: Handle socket flags & multicast */
	soisconnected(so);

	NL_LOG(LOG_DEBUG2, "socket %p, connect to %u", so, snl->nl_pid);

	return (0);
}

static void
destroy_nlpcb_epoch(epoch_context_t ctx)
{
	struct nlpcb *nlp;

	nlp = __containerof(ctx, struct nlpcb, nl_epoch_ctx);

	NLP_LOCK_DESTROY(nlp);
	free(nlp, M_PCB);
}

static void
nl_close(struct socket *so)
{
	MPASS(sotonlpcb(so) != NULL);
	struct nlpcb *nlp;
	struct nl_buf *nb;

	NL_LOG(LOG_DEBUG2, "detaching socket %p, PID %d", so, curproc->p_pid);
	nlp = sotonlpcb(so);

	/* Mark as inactive so no new work can be enqueued */
	NLP_LOCK(nlp);
	bool was_bound = nlp->nl_bound;
	NLP_UNLOCK(nlp);

	/* Wait till all scheduled work has been completed  */
	taskqueue_drain_all(nlp->nl_taskqueue);
	taskqueue_free(nlp->nl_taskqueue);

	NLCTL_WLOCK();
	NLP_LOCK(nlp);
	if (was_bound) {
		CK_LIST_REMOVE(nlp, nl_port_next);
		NL_LOG(LOG_DEBUG3, "socket %p, unlinking bound pid %u", so, nlp->nl_port);
	}
	CK_LIST_REMOVE(nlp, nl_next);
	nlp->nl_socket = NULL;
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK();

	so->so_pcb = NULL;

	while ((nb = TAILQ_FIRST(&so->so_snd.nl_queue)) != NULL) {
		TAILQ_REMOVE(&so->so_snd.nl_queue, nb, tailq);
		nl_buf_free(nb);
	}
	while ((nb = TAILQ_FIRST(&so->so_rcv.nl_queue)) != NULL) {
		TAILQ_REMOVE(&so->so_rcv.nl_queue, nb, tailq);
		nl_buf_free(nb);
	}

	NL_LOG(LOG_DEBUG3, "socket %p, detached", so);

	/* XXX: is delayed free needed? */
	NET_EPOCH_CALL(destroy_nlpcb_epoch, &nlp->nl_epoch_ctx);
}

static int
nl_disconnect(struct socket *so)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);
	return (ENOTCONN);
}

static int
nl_sockaddr(struct socket *so, struct sockaddr *sa)
{

	*(struct sockaddr_nl *)sa = (struct sockaddr_nl ){
		/* TODO: set other fields */
		.nl_len = sizeof(struct sockaddr_nl),
		.nl_family = AF_NETLINK,
		.nl_pid = sotonlpcb(so)->nl_port,
	};

	return (0);
}

static int
nl_sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
    struct mbuf *m, struct mbuf *control, int flags, struct thread *td)
{
	struct nlpcb *nlp = sotonlpcb(so);
	struct sockbuf *sb = &so->so_snd;
	struct nl_buf *nb;
	size_t len;
	int error;

	MPASS(m == NULL && uio != NULL);

	if (__predict_false(control != NULL)) {
		m_freem(control);
		return (EINVAL);
	}

	if (__predict_false(flags & MSG_OOB))	/* XXXGL: or just ignore? */
		return (EOPNOTSUPP);

	if (__predict_false(uio->uio_resid < sizeof(struct nlmsghdr)))
		return (ENOBUFS);		/* XXXGL: any better error? */

	if (__predict_false(uio->uio_resid > sb->sb_hiwat))
		return (EMSGSIZE);

	error = SOCK_IO_SEND_LOCK(so, SBLOCKWAIT(flags));
	if (error)
		return (error);

	len = roundup2(uio->uio_resid, 8) + SCRATCH_BUFFER_SIZE;
	if (nlp->nl_linux)
		len += roundup2(uio->uio_resid, 8);
	nb = nl_buf_alloc(len, M_WAITOK);
	nb->datalen = uio->uio_resid;
	error = uiomove(&nb->data[0], uio->uio_resid, uio);
	if (__predict_false(error))
		goto out;

        NL_LOG(LOG_DEBUG2, "sending message to kernel %u bytes", nb->datalen);

	SOCK_SENDBUF_LOCK(so);
restart:
	if (sb->sb_hiwat - sb->sb_ccc >= nb->datalen) {
		TAILQ_INSERT_TAIL(&sb->nl_queue, nb, tailq);
		sb->sb_acc += nb->datalen;
		sb->sb_ccc += nb->datalen;
		nb = NULL;
	} else if ((so->so_state & SS_NBIO) ||
	    (flags & (MSG_NBIO | MSG_DONTWAIT)) != 0) {
		SOCK_SENDBUF_UNLOCK(so);
		error = EWOULDBLOCK;
		goto out;
	} else {
		if ((error = sbwait(so, SO_SND)) != 0) {
			SOCK_SENDBUF_UNLOCK(so);
			goto out;
		} else
			goto restart;
	}
	SOCK_SENDBUF_UNLOCK(so);

	if (nb == NULL) {
		NL_LOG(LOG_DEBUG3, "success");
		NLP_LOCK(nlp);
		nl_schedule_taskqueue(nlp);
		NLP_UNLOCK(nlp);
	}

out:
	SOCK_IO_SEND_UNLOCK(so);
	if (nb != NULL) {
		NL_LOG(LOG_DEBUG3, "failure, error %d", error);
		nl_buf_free(nb);
	}
	return (error);
}

/* Create control data for recvmsg(2) on Netlink socket. */
static struct mbuf *
nl_createcontrol(struct nlpcb *nlp)
{
	struct {
		struct nlattr nla;
		uint32_t val;
	} data[] = {
		{
			.nla.nla_len = sizeof(struct nlattr) + sizeof(uint32_t),
			.nla.nla_type = NLMSGINFO_ATTR_PROCESS_ID,
			.val = nlp->nl_process_id,
		},
		{
			.nla.nla_len = sizeof(struct nlattr) + sizeof(uint32_t),
			.nla.nla_type = NLMSGINFO_ATTR_PORT_ID,
			.val = nlp->nl_port,
		},
	};

	return (sbcreatecontrol(data, sizeof(data), NETLINK_MSG_INFO,
	    SOL_NETLINK, M_WAITOK));
}

static int
nl_soreceive(struct socket *so, struct sockaddr **psa, struct uio *uio,
    struct mbuf **mp, struct mbuf **controlp, int *flagsp)
{
	static const struct sockaddr_nl nl_empty_src = {
		.nl_len = sizeof(struct sockaddr_nl),
		.nl_family = PF_NETLINK,
		.nl_pid = 0 /* comes from the kernel */
	};
	struct sockbuf *sb = &so->so_rcv;
	struct nlpcb *nlp = sotonlpcb(so);
	struct nl_buf *first, *last, *nb, *next;
	struct nlmsghdr *hdr;
	int flags, error;
	u_int len, overflow, partoff, partlen, msgrcv, datalen;
	bool nonblock, trunc, peek;

	MPASS(mp == NULL && uio != NULL);

	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);

	if (psa != NULL)
		*psa = sodupsockaddr((const struct sockaddr *)&nl_empty_src,
		    M_WAITOK);

	if (controlp != NULL && (nlp->nl_flags & NLF_MSG_INFO))
		*controlp = nl_createcontrol(nlp);

	flags = flagsp != NULL ? *flagsp & ~MSG_TRUNC : 0;
	trunc = flagsp != NULL ? *flagsp & MSG_TRUNC : false;
	nonblock = (so->so_state & SS_NBIO) ||
	    (flags & (MSG_DONTWAIT | MSG_NBIO));
	peek = flags & MSG_PEEK;

	error = SOCK_IO_RECV_LOCK(so, SBLOCKWAIT(flags));
	if (__predict_false(error))
		return (error);

	len = 0;
	overflow = 0;
	msgrcv = 0;
	datalen = 0;

	SOCK_RECVBUF_LOCK(so);
	while ((first = TAILQ_FIRST(&sb->nl_queue)) == NULL) {
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

	/*
	 * Netlink socket buffer consists of a queue of nl_bufs, but for the
	 * userland there should be no boundaries.  However, there are Netlink
	 * messages, that shouldn't be split.  Internal invariant is that a
	 * message never spans two nl_bufs.
	 * If a large userland buffer is provided, we would traverse the queue
	 * until either queue end is reached or the buffer is fulfilled.  If
	 * an application provides a buffer that isn't able to fit a single
	 * message, we would truncate it and lose its tail.  This is the only
	 * condition where we would lose data.  If buffer is able to fit at
	 * least one message, we would return it and won't truncate the next.
	 *
	 * We use same code for normal and MSG_PEEK case.  At first queue pass
	 * we scan nl_bufs and count lenght.  In case we can read entire buffer
	 * at one write everything is trivial.  In case we can not, we save
	 * pointer to the last (or partial) nl_buf and in the !peek case we
	 * split the queue into two pieces.  We can safely drop the queue lock,
	 * as kernel would only append nl_bufs to the end of the queue, and
	 * we are the exclusive owner of queue beginning due to sleepable lock.
	 * At the second pass we copy data out and in !peek case free nl_bufs.
	 */
	TAILQ_FOREACH(nb, &sb->nl_queue, tailq) {
		u_int offset;

		MPASS(nb->offset < nb->datalen);
		offset = nb->offset;
		while (offset < nb->datalen) {
			hdr = (struct nlmsghdr *)&nb->data[offset];
			MPASS(nb->offset + hdr->nlmsg_len <= nb->datalen);
			if (uio->uio_resid < len + hdr->nlmsg_len) {
				overflow = len + hdr->nlmsg_len -
				    uio->uio_resid;
				partoff = nb->offset;
				if (offset > partoff) {
					partlen = offset - partoff;
					if (!peek) {
						nb->offset = offset;
						datalen += partlen;
					}
				} else if (len == 0 && uio->uio_resid > 0) {
					flags |= MSG_TRUNC;
					partlen = uio->uio_resid;
					if (peek)
						goto nospace;
					datalen += hdr->nlmsg_len;
					if (nb->offset + hdr->nlmsg_len ==
					    nb->datalen) {
						/*
						 * Avoid leaving empty nb.
						 * Process last nb normally.
						 * Trust uiomove() to care
						 * about negative uio_resid.
						 */
						nb = TAILQ_NEXT(nb, tailq);
						overflow = 0;
						partlen = 0;
					} else
						nb->offset += hdr->nlmsg_len;
					msgrcv++;
				} else
					partlen = 0;
				goto nospace;
			}
			len += hdr->nlmsg_len;
			offset += hdr->nlmsg_len;
			MPASS(offset <= nb->buflen);
			msgrcv++;
		}
		MPASS(offset == nb->datalen);
		datalen += nb->datalen - nb->offset;
	}
nospace:
	last = nb;
	if (!peek) {
		if (last == NULL)
			TAILQ_INIT(&sb->nl_queue);
		else {
			/* XXXGL: create TAILQ_SPLIT */
			TAILQ_FIRST(&sb->nl_queue) = last;
			last->tailq.tqe_prev = &TAILQ_FIRST(&sb->nl_queue);
		}
		MPASS(sb->sb_acc >= datalen);
		sb->sb_acc -= datalen;
		sb->sb_ccc -= datalen;
	}
	SOCK_RECVBUF_UNLOCK(so);

	for (nb = first; nb != last; nb = next) {
		next = TAILQ_NEXT(nb, tailq);
		if (__predict_true(error == 0))
			error = uiomove(&nb->data[nb->offset],
			    (int)(nb->datalen - nb->offset), uio);
		if (!peek)
			nl_buf_free(nb);
	}
	if (last != NULL && partlen > 0 && __predict_true(error == 0))
		error = uiomove(&nb->data[partoff], (int)partlen, uio);

	if (trunc && overflow > 0) {
		uio->uio_resid -= overflow;
		MPASS(uio->uio_resid < 0);
	} else
		MPASS(uio->uio_resid >= 0);

	if (uio->uio_td)
		uio->uio_td->td_ru.ru_msgrcv += msgrcv;

	if (flagsp != NULL)
		*flagsp |= flags;

	SOCK_IO_RECV_UNLOCK(so);

	nl_on_transmit(sotonlpcb(so));

	return (error);
}

static int
nl_getoptflag(int sopt_name)
{
	switch (sopt_name) {
	case NETLINK_CAP_ACK:
		return (NLF_CAP_ACK);
	case NETLINK_EXT_ACK:
		return (NLF_EXT_ACK);
	case NETLINK_GET_STRICT_CHK:
		return (NLF_STRICT);
	case NETLINK_MSG_INFO:
		return (NLF_MSG_INFO);
	}

	return (0);
}

static int
nl_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct nlpcb *nlp = sotonlpcb(so);
	uint32_t flag;
	int optval, error = 0;
	NLCTL_TRACKER;

	NL_LOG(LOG_DEBUG2, "%ssockopt(%p, %d)", (sopt->sopt_dir) ? "set" : "get",
	    so, sopt->sopt_name);

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case NETLINK_ADD_MEMBERSHIP:
		case NETLINK_DROP_MEMBERSHIP:
			error = sooptcopyin(sopt, &optval, sizeof(optval), sizeof(optval));
			if (error != 0)
				break;
			if (optval <= 0 || optval >= NLP_MAX_GROUPS) {
				error = ERANGE;
				break;
			}
			NL_LOG(LOG_DEBUG2, "ADD/DEL group %d", (uint32_t)optval);

			NLCTL_WLOCK();
			if (sopt->sopt_name == NETLINK_ADD_MEMBERSHIP)
				nlp_join_group(nlp, optval);
			else
				nlp_leave_group(nlp, optval);
			NLCTL_WUNLOCK();
			break;
		case NETLINK_CAP_ACK:
		case NETLINK_EXT_ACK:
		case NETLINK_GET_STRICT_CHK:
		case NETLINK_MSG_INFO:
			error = sooptcopyin(sopt, &optval, sizeof(optval), sizeof(optval));
			if (error != 0)
				break;

			flag = nl_getoptflag(sopt->sopt_name);

			if ((flag == NLF_MSG_INFO) && nlp->nl_linux) {
				error = EINVAL;
				break;
			}

			NLCTL_WLOCK();
			if (optval != 0)
				nlp->nl_flags |= flag;
			else
				nlp->nl_flags &= ~flag;
			NLCTL_WUNLOCK();
			break;
		default:
			error = ENOPROTOOPT;
		}
		break;
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case NETLINK_LIST_MEMBERSHIPS:
			NLCTL_RLOCK();
			optval = nlp_get_groups_compat(nlp);
			NLCTL_RUNLOCK();
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		case NETLINK_CAP_ACK:
		case NETLINK_EXT_ACK:
		case NETLINK_GET_STRICT_CHK:
		case NETLINK_MSG_INFO:
			NLCTL_RLOCK();
			optval = (nlp->nl_flags & nl_getoptflag(sopt->sopt_name)) != 0;
			NLCTL_RUNLOCK();
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		default:
			error = ENOPROTOOPT;
		}
		break;
	default:
		error = ENOPROTOOPT;
	}

	return (error);
}

static int
sysctl_handle_nl_maxsockbuf(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	u_long tmp_maxsockbuf = nl_maxsockbuf;

	error = sysctl_handle_long(oidp, &tmp_maxsockbuf, arg2, req);
	if (error || !req->newptr)
		return (error);
	if (tmp_maxsockbuf < MSIZE + MCLBYTES)
		return (EINVAL);
	nl_maxsockbuf = tmp_maxsockbuf;

	return (0);
}

static int
nl_setsbopt(struct socket *so, struct sockopt *sopt)
{
	int error, optval;
	bool result;

	if (sopt->sopt_name != SO_RCVBUF)
		return (sbsetopt(so, sopt));

	/* Allow to override max buffer size in certain conditions */

	error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
	if (error != 0)
		return (error);
	NL_LOG(LOG_DEBUG2, "socket %p, PID %d, SO_RCVBUF=%d", so, curproc->p_pid, optval);
	if (optval > sb_max_adj) {
		if (priv_check(curthread, PRIV_NET_ROUTE) != 0)
			return (EPERM);
	}

	SOCK_RECVBUF_LOCK(so);
	result = sbreserve_locked_limit(so, SO_RCV, optval, nl_maxsockbuf, curthread);
	SOCK_RECVBUF_UNLOCK(so);

	return (result ? 0 : ENOBUFS);
}

#define	NETLINK_PROTOSW						\
	.pr_flags = PR_ATOMIC | PR_ADDR | PR_SOCKBUF,		\
	.pr_ctloutput = nl_ctloutput,				\
	.pr_setsbopt = nl_setsbopt,				\
	.pr_attach = nl_attach,					\
	.pr_bind = nl_bind,					\
	.pr_connect = nl_connect,				\
	.pr_disconnect = nl_disconnect,				\
	.pr_sosend = nl_sosend,					\
	.pr_soreceive = nl_soreceive,				\
	.pr_sockaddr = nl_sockaddr,				\
	.pr_close = nl_close

static struct protosw netlink_raw_sw = {
	.pr_type = SOCK_RAW,
	NETLINK_PROTOSW
};

static struct protosw netlink_dgram_sw = {
	.pr_type = SOCK_DGRAM,
	NETLINK_PROTOSW
};

static struct domain netlinkdomain = {
	.dom_family = PF_NETLINK,
	.dom_name = "netlink",
	.dom_flags = DOMF_UNLOADABLE,
	.dom_nprotosw =		2,
	.dom_protosw =		{ &netlink_raw_sw, &netlink_dgram_sw },
};

DOMAIN_SET(netlink);
