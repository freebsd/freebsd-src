/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
#include <sys/priv.h> /* priv_check */

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
#define	NLCTL_RLOCK(_ctl)	rm_rlock(&((_ctl)->ctl_lock), &nl_tracker)
#define	NLCTL_RUNLOCK(_ctl)	rm_runlock(&((_ctl)->ctl_lock), &nl_tracker)

#define	NLCTL_WLOCK(_ctl)	rm_wlock(&((_ctl)->ctl_lock))
#define	NLCTL_WUNLOCK(_ctl)	rm_wunlock(&((_ctl)->ctl_lock))

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

	CK_LIST_FOREACH(nlp, &V_nl_ctl->ctl_port_head, nl_port_next) {
		if (nlp->nl_port == port_id)
			return (nlp);
	}
	return (NULL);
}

static void
nl_add_group_locked(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id <= NLP_MAX_GROUPS);
	--group_id;

	/* TODO: add family handler callback */
	if (!nlp_unconstrained_vnet(nlp))
		return;

	nlp->nl_groups[group_id / 64] |= (uint64_t)1 << (group_id % 64);
}

static void
nl_del_group_locked(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id <= NLP_MAX_GROUPS);
	--group_id;

	nlp->nl_groups[group_id / 64] &= ~((uint64_t)1 << (group_id % 64));
}

static bool
nl_isset_group_locked(struct nlpcb *nlp, unsigned int group_id)
{
	MPASS(group_id <= NLP_MAX_GROUPS);
	--group_id;

	return (nlp->nl_groups[group_id / 64] & ((uint64_t)1 << (group_id % 64)));
}

static uint32_t
nl_get_groups_compat(struct nlpcb *nlp)
{
	uint32_t groups_mask = 0;

	for (int i = 0; i < 32; i++) {
		if (nl_isset_group_locked(nlp, i + 1))
			groups_mask |= (1 << i);
	}

	return (groups_mask);
}

static void
nl_send_one_group(struct mbuf *m, struct nlpcb *nlp, int num_messages,
    int io_flags)
{
	if (__predict_false(nlp->nl_flags & NLF_MSG_INFO))
		nl_add_msg_info(m);
	nl_send_one(m, nlp, num_messages, io_flags);
}

/*
 * Broadcasts message @m to the protocol @proto group specified by @group_id
 */
void
nl_send_group(struct mbuf *m, int num_messages, int proto, int group_id)
{
	struct nlpcb *nlp_last = NULL;
	struct nlpcb *nlp;
	NLCTL_TRACKER;

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		struct nlmsghdr *hdr = mtod(m, struct nlmsghdr *);
		NL_LOG(LOG_DEBUG2, "MCAST mbuf len %u msg type %d len %u to group %d/%d",
		    m->m_len, hdr->nlmsg_type, hdr->nlmsg_len, proto, group_id);
	}

	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	if (__predict_false(ctl == NULL)) {
		/*
		 * Can be the case when notification is sent within VNET
		 * which doesn't have any netlink sockets.
		 */
		m_freem(m);
		return;
	}

	NLCTL_RLOCK(ctl);

	int io_flags = NL_IOF_UNTRANSLATED;

	CK_LIST_FOREACH(nlp, &ctl->ctl_pcb_head, nl_next) {
		if (nl_isset_group_locked(nlp, group_id) && nlp->nl_proto == proto) {
			if (nlp_last != NULL) {
				struct mbuf *m_copy;
				m_copy = m_copym(m, 0, M_COPYALL, M_NOWAIT);
				if (m_copy != NULL)
					nl_send_one_group(m_copy, nlp_last,
					    num_messages, io_flags);
				else {
					NLP_LOCK(nlp_last);
					if (nlp_last->nl_socket != NULL)
						sorwakeup(nlp_last->nl_socket);
					NLP_UNLOCK(nlp_last);
				}
			}
			nlp_last = nlp;
		}
	}
	if (nlp_last != NULL)
		nl_send_one_group(m, nlp_last, num_messages, io_flags);
	else
		m_freem(m);

	NLCTL_RUNLOCK(ctl);
}

bool
nl_has_listeners(int netlink_family, uint32_t groups_mask)
{
	return (V_nl_ctl != NULL);
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
		CK_LIST_INSERT_HEAD(&V_nl_ctl->ctl_port_head, nlp, nl_port_next);
	}
	for (int i = 0; i < 32; i++) {
		if (snl->nl_groups & ((uint32_t)1 << i))
			nl_add_group_locked(nlp, i + 1);
		else
			nl_del_group_locked(nlp, i + 1);
	}

	return (0);
}

static int
nl_pru_attach(struct socket *so, int proto, struct thread *td)
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

	/* Create per-VNET state on first socket init */
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	if (ctl == NULL)
		ctl = vnet_nl_ctl_init();
	KASSERT(V_nl_ctl != NULL, ("nl_attach: vnet_sock_init() failed"));

	MPASS(sotonlpcb(so) == NULL);

	nlp = malloc(sizeof(struct nlpcb), M_PCB, M_WAITOK | M_ZERO);
	error = soreserve(so, nl_sendspace, nl_recvspace);
	if (error != 0) {
		free(nlp, M_PCB);
		return (error);
	}
	so->so_pcb = nlp;
	nlp->nl_socket = so;
	/* Copy so_cred to avoid having socket_var.h in every header */
	nlp->nl_cred = so->so_cred;
	nlp->nl_proto = proto;
	nlp->nl_process_id = curproc->p_pid;
	nlp->nl_linux = is_linux;
	nlp->nl_active = true;
	nlp->nl_unconstrained_vnet = !jailed_without_vnet(so->so_cred);
	nlp->nl_need_thread_setup = true;
	NLP_LOCK_INIT(nlp);
	refcount_init(&nlp->nl_refcount, 1);
	nl_init_io(nlp);

	nlp->nl_taskqueue = taskqueue_create("netlink_socket", M_WAITOK,
	    taskqueue_thread_enqueue, &nlp->nl_taskqueue);
	TASK_INIT(&nlp->nl_task, 0, nl_taskqueue_handler, nlp);
	taskqueue_start_threads(&nlp->nl_taskqueue, 1, PWAIT,
	    "netlink_socket (PID %u)", nlp->nl_process_id);

	NLCTL_WLOCK(ctl);
	/* XXX: check ctl is still alive */
	CK_LIST_INSERT_HEAD(&ctl->ctl_pcb_head, nlp, nl_next);
	NLCTL_WUNLOCK(ctl);

	soisconnected(so);

	return (0);
}

static void
nl_pru_abort(struct socket *so)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);
	soisdisconnected(so);
}

static int
nl_pru_bind(struct socket *so, struct sockaddr *sa, struct thread *td)
{
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	struct nlpcb *nlp = sotonlpcb(so);
	struct sockaddr_nl *snl = (struct sockaddr_nl *)sa;
	int error;

	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	if (snl->nl_len != sizeof(*snl)) {
		NL_LOG(LOG_DEBUG, "socket %p, wrong sizeof(), ignoring bind()", so);
		return (EINVAL);
	}


	NLCTL_WLOCK(ctl);
	NLP_LOCK(nlp);
	error = nl_bind_locked(nlp, snl);
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK(ctl);
	NL_LOG(LOG_DEBUG2, "socket %p, bind() to %u, groups %u, error %d", so,
	    snl->nl_pid, snl->nl_groups, error);

	return (error);
}


static int
nl_assign_port(struct nlpcb *nlp, uint32_t port_id)
{
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	struct sockaddr_nl snl = {
		.nl_pid = port_id,
	};
	int error;

	NLCTL_WLOCK(ctl);
	NLP_LOCK(nlp);
	snl.nl_groups = nl_get_groups_compat(nlp);
	error = nl_bind_locked(nlp, &snl);
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK(ctl);

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
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	uint32_t port_id = candidate_id;
	NLCTL_TRACKER;
	bool exist;
	int error = EADDRINUSE;

	for (int i = 0; i < 10; i++) {
		NL_LOG(LOG_DEBUG3, "socket %p, trying to assign port %d", nlp->nl_socket, port_id);
		NLCTL_RLOCK(ctl);
		exist = nl_port_lookup(port_id) != 0;
		NLCTL_RUNLOCK(ctl);
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
nl_pru_connect(struct socket *so, struct sockaddr *sa, struct thread *td)
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
destroy_nlpcb(struct nlpcb *nlp)
{
	NLP_LOCK(nlp);
	nl_free_io(nlp);
	NLP_LOCK_DESTROY(nlp);
	free(nlp, M_PCB);
}

static void
destroy_nlpcb_epoch(epoch_context_t ctx)
{
	struct nlpcb *nlp;

	nlp = __containerof(ctx, struct nlpcb, nl_epoch_ctx);

	destroy_nlpcb(nlp);
}


static void
nl_pru_detach(struct socket *so)
{
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
	MPASS(sotonlpcb(so) != NULL);
	struct nlpcb *nlp;

	NL_LOG(LOG_DEBUG2, "detaching socket %p, PID %d", so, curproc->p_pid);
	nlp = sotonlpcb(so);

	/* Mark as inactive so no new work can be enqueued */
	NLP_LOCK(nlp);
	bool was_bound = nlp->nl_bound;
	nlp->nl_active = false;
	NLP_UNLOCK(nlp);

	/* Wait till all scheduled work has been completed  */
	taskqueue_drain_all(nlp->nl_taskqueue);
	taskqueue_free(nlp->nl_taskqueue);

	NLCTL_WLOCK(ctl);
	NLP_LOCK(nlp);
	if (was_bound) {
		CK_LIST_REMOVE(nlp, nl_port_next);
		NL_LOG(LOG_DEBUG3, "socket %p, unlinking bound pid %u", so, nlp->nl_port);
	}
	CK_LIST_REMOVE(nlp, nl_next);
	nlp->nl_socket = NULL;
	NLP_UNLOCK(nlp);
	NLCTL_WUNLOCK(ctl);

	so->so_pcb = NULL;

	NL_LOG(LOG_DEBUG3, "socket %p, detached", so);

	/* XXX: is delayed free needed? */
	NET_EPOCH_CALL(destroy_nlpcb_epoch, &nlp->nl_epoch_ctx);
}

static int
nl_pru_disconnect(struct socket *so)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);
	return (ENOTCONN);
}

static int
nl_pru_shutdown(struct socket *so)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);
	socantsendmore(so);
	return (0);
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

static void
nl_pru_close(struct socket *so)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);
	soisdisconnected(so);
}

static int
nl_pru_output(struct mbuf *m, struct socket *so, ...)
{

	if (__predict_false(m == NULL ||
	    ((m->m_len < sizeof(struct nlmsghdr)) &&
		(m = m_pullup(m, sizeof(struct nlmsghdr))) == NULL)))
		return (ENOBUFS);
	MPASS((m->m_flags & M_PKTHDR) != 0);

	NL_LOG(LOG_DEBUG3, "sending message to kernel async processing");
	nl_receive_async(m, so);
	return (0);
}


static int
nl_pru_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *sa,
    struct mbuf *control, struct thread *td)
{
        NL_LOG(LOG_DEBUG2, "sending message to kernel");

	if (__predict_false(control != NULL)) {
		if (control->m_len) {
			m_freem(control);
			return (EINVAL);
		}
		m_freem(control);
	}

	return (nl_pru_output(m, so));
}

static int
nl_pru_rcvd(struct socket *so, int flags)
{
	NL_LOG(LOG_DEBUG3, "socket %p, PID %d", so, curproc->p_pid);
	MPASS(sotonlpcb(so) != NULL);

	nl_on_transmit(sotonlpcb(so));

	return (0);
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
	struct nl_control *ctl = atomic_load_ptr(&V_nl_ctl);
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

			NLCTL_WLOCK(ctl);
			if (sopt->sopt_name == NETLINK_ADD_MEMBERSHIP)
				nl_add_group_locked(nlp, optval);
			else
				nl_del_group_locked(nlp, optval);
			NLCTL_WUNLOCK(ctl);
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

			NLCTL_WLOCK(ctl);
			if (optval != 0)
				nlp->nl_flags |= flag;
			else
				nlp->nl_flags &= ~flag;
			NLCTL_WUNLOCK(ctl);
			break;
		default:
			error = ENOPROTOOPT;
		}
		break;
	case SOPT_GET:
		switch (sopt->sopt_name) {
		case NETLINK_LIST_MEMBERSHIPS:
			NLCTL_RLOCK(ctl);
			optval = nl_get_groups_compat(nlp);
			NLCTL_RUNLOCK(ctl);
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		case NETLINK_CAP_ACK:
		case NETLINK_EXT_ACK:
		case NETLINK_GET_STRICT_CHK:
		case NETLINK_MSG_INFO:
			NLCTL_RLOCK(ctl);
			optval = (nlp->nl_flags & nl_getoptflag(sopt->sopt_name)) != 0;
			NLCTL_RUNLOCK(ctl);
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
	.pr_flags = PR_ATOMIC | PR_ADDR | PR_WANTRCVD,		\
	.pr_ctloutput = nl_ctloutput,				\
	.pr_setsbopt = nl_setsbopt,				\
	.pr_abort = nl_pru_abort,				\
	.pr_attach = nl_pru_attach,				\
	.pr_bind = nl_pru_bind,					\
	.pr_connect = nl_pru_connect,				\
	.pr_detach = nl_pru_detach,				\
	.pr_disconnect = nl_pru_disconnect,			\
	.pr_send = nl_pru_send,					\
	.pr_rcvd = nl_pru_rcvd,					\
	.pr_shutdown = nl_pru_shutdown,				\
	.pr_sockaddr = nl_sockaddr,				\
	.pr_close = nl_pru_close

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
