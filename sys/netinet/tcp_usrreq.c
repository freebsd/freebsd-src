/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2006-2007 Robert N. M. Watson
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
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
 */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/arb.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/refcount.h>
#include <sys/kernel.h>
#include <sys/ktls.h>
#include <sys/qmath.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif /* INET6 */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/stats.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcpip.h>
#include <netinet/cc/cc.h>
#include <netinet/tcp_fastopen.h>
#include <netinet/tcp_hpts.h>
#ifdef TCP_OFFLOAD
#include <netinet/tcp_offload.h>
#endif
#include <netipsec/ipsec_support.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

/*
 * TCP protocol interface to socket abstraction.
 */
#ifdef INET
static int	tcp_connect(struct tcpcb *, struct sockaddr_in *,
		    struct thread *td);
#endif /* INET */
#ifdef INET6
static int	tcp6_connect(struct tcpcb *, struct sockaddr_in6 *,
		    struct thread *td);
#endif /* INET6 */
static void	tcp_disconnect(struct tcpcb *);
static void	tcp_usrclosed(struct tcpcb *);
static void	tcp_fill_info(const struct tcpcb *, struct tcp_info *);

static int	tcp_pru_options_support(struct tcpcb *tp, int flags);

static void
tcp_bblog_pru(struct tcpcb *tp, uint32_t pru, int error)
{
	struct tcp_log_buffer *lgb;

	KASSERT(tp != NULL, ("tcp_bblog_pru: tp == NULL"));
	INP_WLOCK_ASSERT(tptoinpcb(tp));
	if (tcp_bblogging_on(tp)) {
		lgb = tcp_log_event(tp, NULL, NULL, NULL, TCP_LOG_PRU, error,
		    0, NULL, false, NULL, NULL, 0, NULL);
	} else {
		lgb = NULL;
	}
	if (lgb != NULL) {
		if (error >= 0) {
			lgb->tlb_errno = (uint32_t)error;
		}
		lgb->tlb_flex1 = pru;
	}
}

/*
 * TCP attaches to socket via pr_attach(), reserving space,
 * and an internet control block.
 */
static int
tcp_usr_attach(struct socket *so, int proto, struct thread *td)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	int error;

	inp = sotoinpcb(so);
	KASSERT(inp == NULL, ("tcp_usr_attach: inp != NULL"));

	error = soreserve(so, V_tcp_sendspace, V_tcp_recvspace);
	if (error)
		goto out;

	so->so_rcv.sb_flags |= SB_AUTOSIZE;
	so->so_snd.sb_flags |= SB_AUTOSIZE;
	error = in_pcballoc(so, &V_tcbinfo);
	if (error)
		goto out;
	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp, NULL);
	if (tp == NULL) {
		error = ENOBUFS;
		in_pcbfree(inp);
		goto out;
	}
	tp->t_state = TCPS_CLOSED;
	tcp_bblog_pru(tp, PRU_ATTACH, error);
	INP_WUNLOCK(inp);
	TCPSTATES_INC(TCPS_CLOSED);
out:
	TCP_PROBE2(debug__user, tp, PRU_ATTACH);
	return (error);
}

/*
 * tcp_usr_detach is called when the socket layer loses its final reference
 * to the socket, be it a file descriptor reference, a reference from TCP,
 * etc.  At this point, there is only one case in which we will keep around
 * inpcb state: time wait.
 */
static void
tcp_usr_detach(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("%s: inp == NULL", __func__));
	INP_WLOCK(inp);
	KASSERT(so->so_pcb == inp && inp->inp_socket == so,
		("%s: socket %p inp %p mismatch", __func__, so, inp));

	tp = intotcpcb(inp);

	KASSERT(inp->inp_flags & INP_DROPPED ||
	    tp->t_state < TCPS_SYN_SENT,
	    ("%s: inp %p not dropped or embryonic", __func__, inp));

	tcp_discardcb(tp);
	in_pcbfree(inp);
}

#ifdef INET
/*
 * Give the socket an address.
 */
static int
tcp_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_bind: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	tp = intotcpcb(inp);

	sinp = (struct sockaddr_in *)nam;
	if (nam->sa_family != AF_INET) {
		/*
		 * Preserve compatibility with old programs.
		 */
		if (nam->sa_family != AF_UNSPEC ||
		    nam->sa_len < offsetof(struct sockaddr_in, sin_zero) ||
		    sinp->sin_addr.s_addr != INADDR_ANY) {
			error = EAFNOSUPPORT;
			goto out;
		}
		nam->sa_family = AF_INET;
	}
	if (nam->sa_len != sizeof(*sinp)) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
		error = EAFNOSUPPORT;
		goto out;
	}
	INP_HASH_WLOCK(&V_tcbinfo);
	error = in_pcbbind(inp, sinp, V_tcp_bind_all_fibs ? 0 : INPBIND_FIB,
	    td->td_ucred);
	INP_HASH_WUNLOCK(&V_tcbinfo);
out:
	tcp_bblog_pru(tp, PRU_BIND, error);
	TCP_PROBE2(debug__user, tp, PRU_BIND);
	INP_WUNLOCK(inp);

	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6;
	u_char vflagsav;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_bind: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	tp = intotcpcb(inp);

	vflagsav = inp->inp_vflag;

	sin6 = (struct sockaddr_in6 *)nam;
	if (nam->sa_family != AF_INET6) {
		error = EAFNOSUPPORT;
		goto out;
	}
	if (nam->sa_len != sizeof(*sin6)) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		error = EAFNOSUPPORT;
		goto out;
	}

	INP_HASH_WLOCK(&V_tcbinfo);
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
#ifdef INET
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
		else if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6);
			if (IN_MULTICAST(ntohl(sin.sin_addr.s_addr))) {
				error = EAFNOSUPPORT;
				INP_HASH_WUNLOCK(&V_tcbinfo);
				goto out;
			}
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, &sin, 0, td->td_ucred);
			INP_HASH_WUNLOCK(&V_tcbinfo);
			goto out;
		}
	}
#endif
	error = in6_pcbbind(inp, sin6, V_tcp_bind_all_fibs ? 0 : INPBIND_FIB,
	    td->td_ucred);
	INP_HASH_WUNLOCK(&V_tcbinfo);
out:
	if (error != 0)
		inp->inp_vflag = vflagsav;
	tcp_bblog_pru(tp, PRU_BIND, error);
	TCP_PROBE2(debug__user, tp, PRU_BIND);
	INP_WUNLOCK(inp);
	return (error);
}
#endif /* INET6 */

#ifdef INET
/*
 * Prepare to accept connections.
 */
static int
tcp_usr_listen(struct socket *so, int backlog, struct thread *td)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	int error = 0;
	bool already_listening;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_listen: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	tp = intotcpcb(inp);

	SOCK_LOCK(so);
	already_listening = SOLISTENING(so);
	error = solisten_proto_check(so);
	if (error != 0) {
		SOCK_UNLOCK(so);
		goto out;
	}
	if (inp->inp_lport == 0) {
		INP_HASH_WLOCK(&V_tcbinfo);
		error = in_pcbbind(inp, NULL,
		    V_tcp_bind_all_fibs ? 0 : INPBIND_FIB, td->td_ucred);
		INP_HASH_WUNLOCK(&V_tcbinfo);
	}
	if (error == 0) {
		tcp_state_change(tp, TCPS_LISTEN);
		solisten_proto(so, backlog);
#ifdef TCP_OFFLOAD
		if ((so->so_options & SO_NO_OFFLOAD) == 0)
			tcp_offload_listen_start(tp);
#endif
	} else {
		solisten_proto_abort(so);
	}
	SOCK_UNLOCK(so);
	if (already_listening)
		goto out;

	if (error == 0)
		in_pcblisten(inp);
	if (tp->t_flags & TF_FASTOPEN)
		tp->t_tfo_pending = tcp_fastopen_alloc_counter();

out:
	tcp_bblog_pru(tp, PRU_LISTEN, error);
	TCP_PROBE2(debug__user, tp, PRU_LISTEN);
	INP_WUNLOCK(inp);
	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_listen(struct socket *so, int backlog, struct thread *td)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	u_char vflagsav;
	int error = 0;
	bool already_listening;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_listen: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	tp = intotcpcb(inp);

	vflagsav = inp->inp_vflag;

	SOCK_LOCK(so);
	already_listening = SOLISTENING(so);
	error = solisten_proto_check(so);
	if (error != 0) {
		SOCK_UNLOCK(so);
		goto out;
	}
	INP_HASH_WLOCK(&V_tcbinfo);
	if (inp->inp_lport == 0) {
		inp->inp_vflag &= ~INP_IPV4;
		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0)
			inp->inp_vflag |= INP_IPV4;
		error = in6_pcbbind(inp, NULL,
		    V_tcp_bind_all_fibs ? 0 : INPBIND_FIB, td->td_ucred);
	}
	INP_HASH_WUNLOCK(&V_tcbinfo);
	if (error == 0) {
		tcp_state_change(tp, TCPS_LISTEN);
		solisten_proto(so, backlog);
#ifdef TCP_OFFLOAD
		if ((so->so_options & SO_NO_OFFLOAD) == 0)
			tcp_offload_listen_start(tp);
#endif
	} else {
		solisten_proto_abort(so);
	}
	SOCK_UNLOCK(so);
	if (already_listening)
		goto out;

	if (error == 0)
		in_pcblisten(inp);
	if (tp->t_flags & TF_FASTOPEN)
		tp->t_tfo_pending = tcp_fastopen_alloc_counter();

	if (error != 0)
		inp->inp_vflag = vflagsav;

out:
	tcp_bblog_pru(tp, PRU_LISTEN, error);
	TCP_PROBE2(debug__user, tp, PRU_LISTEN);
	INP_WUNLOCK(inp);
	return (error);
}
#endif /* INET6 */

#ifdef INET
/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
static int
tcp_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct epoch_tracker et;
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_connect: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNREFUSED);
	}
	tp = intotcpcb(inp);

	sinp = (struct sockaddr_in *)nam;
	if (nam->sa_family != AF_INET) {
		error = EAFNOSUPPORT;
		goto out;
	}
	if (nam->sa_len != sizeof (*sinp)) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
		error = EAFNOSUPPORT;
		goto out;
	}
	if (ntohl(sinp->sin_addr.s_addr) == INADDR_BROADCAST) {
		error = EACCES;
		goto out;
	}
	if ((error = prison_remote_ip4(td->td_ucred, &sinp->sin_addr)) != 0)
		goto out;
	if (SOLISTENING(so) || so->so_options & SO_REUSEPORT_LB) {
		error = EOPNOTSUPP;
		goto out;
	}
	NET_EPOCH_ENTER(et);
	if ((error = tcp_connect(tp, sinp, td)) != 0)
		goto out_in_epoch;
#ifdef TCP_OFFLOAD
	if (registered_toedevs > 0 &&
	    (so->so_options & SO_NO_OFFLOAD) == 0 &&
	    (error = tcp_offload_connect(so, nam)) == 0)
		goto out_in_epoch;
#endif
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	error = tcp_output(tp);
	KASSERT(error >= 0, ("TCP stack %s requested tcp_drop(%p) at connect()"
	    ", error code %d", tp->t_fb->tfb_tcp_block_name, tp, -error));
out_in_epoch:
	NET_EPOCH_EXIT(et);
out:
	tcp_bblog_pru(tp, PRU_CONNECT, error);
	TCP_PROBE2(debug__user, tp, PRU_CONNECT);
	INP_WUNLOCK(inp);
	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct epoch_tracker et;
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6;
	u_int8_t incflagsav;
	u_char vflagsav;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_connect: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNREFUSED);
	}
	tp = intotcpcb(inp);

	vflagsav = inp->inp_vflag;
	incflagsav = inp->inp_inc.inc_flags;

	sin6 = (struct sockaddr_in6 *)nam;
	if (nam->sa_family != AF_INET6) {
		error = EAFNOSUPPORT;
		goto out;
	}
	if (nam->sa_len != sizeof (*sin6)) {
		error = EINVAL;
		goto out;
	}
	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		error = EAFNOSUPPORT;
		goto out;
	}
	if (SOLISTENING(so) || so->so_options & SO_REUSEPORT_LB) {
		error = EOPNOTSUPP;
		goto out;
	}
#ifdef INET
	/*
	 * XXXRW: Some confusion: V4/V6 flags relate to binding, and
	 * therefore probably require the hash lock, which isn't held here.
	 * Is this a significant problem?
	 */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		struct sockaddr_in sin;

		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
			error = EINVAL;
			goto out;
		}
		if ((inp->inp_vflag & INP_IPV4) == 0) {
			error = EAFNOSUPPORT;
			goto out;
		}

		in6_sin6_2_sin(&sin, sin6);
		if (IN_MULTICAST(ntohl(sin.sin_addr.s_addr))) {
			error = EAFNOSUPPORT;
			goto out;
		}
		if (ntohl(sin.sin_addr.s_addr) == INADDR_BROADCAST) {
			error = EACCES;
			goto out;
		}
		if ((error = prison_remote_ip4(td->td_ucred,
		    &sin.sin_addr)) != 0)
			goto out;
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		NET_EPOCH_ENTER(et);
		if ((error = tcp_connect(tp, &sin, td)) != 0)
			goto out_in_epoch;
#ifdef TCP_OFFLOAD
		if (registered_toedevs > 0 &&
		    (so->so_options & SO_NO_OFFLOAD) == 0 &&
		    (error = tcp_offload_connect(so, nam)) == 0)
			goto out_in_epoch;
#endif
		error = tcp_output(tp);
		goto out_in_epoch;
	} else {
		if ((inp->inp_vflag & INP_IPV6) == 0) {
			error = EAFNOSUPPORT;
			goto out;
		}
	}
#endif
	if ((error = prison_remote_ip6(td->td_ucred, &sin6->sin6_addr)) != 0)
		goto out;
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	inp->inp_inc.inc_flags |= INC_ISIPV6;
	NET_EPOCH_ENTER(et);
	if ((error = tcp6_connect(tp, sin6, td)) != 0)
		goto out_in_epoch;
#ifdef TCP_OFFLOAD
	if (registered_toedevs > 0 &&
	    (so->so_options & SO_NO_OFFLOAD) == 0 &&
	    (error = tcp_offload_connect(so, nam)) == 0)
		goto out_in_epoch;
#endif
	tcp_timer_activate(tp, TT_KEEP, TP_KEEPINIT(tp));
	error = tcp_output(tp);
out_in_epoch:
	NET_EPOCH_EXIT(et);
out:
	KASSERT(error >= 0, ("TCP stack %s requested tcp_drop(%p) at connect()"
	    ", error code %d", tp->t_fb->tfb_tcp_block_name, tp, -error));
	/*
	 * If the implicit bind in the connect call fails, restore
	 * the flags we modified.
	 */
	if (error != 0 && inp->inp_lport == 0) {
		inp->inp_vflag = vflagsav;
		inp->inp_inc.inc_flags = incflagsav;
	}

	tcp_bblog_pru(tp, PRU_CONNECT, error);
	TCP_PROBE2(debug__user, tp, PRU_CONNECT);
	INP_WUNLOCK(inp);
	return (error);
}
#endif /* INET6 */

/*
 * Initiate disconnect from peer.
 * If connection never passed embryonic stage, just drop;
 * else if don't need to let data drain, then can just drop anyways,
 * else have to begin TCP shutdown process: mark socket disconnecting,
 * drain unread data, state switch to reflect user close, and
 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
 * when peer sends FIN and acks ours.
 *
 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
 */
static int
tcp_usr_disconnect(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_disconnect: inp == NULL"));
	INP_WLOCK(inp);
	tp = intotcpcb(inp);

	if (tp->t_state == TCPS_TIME_WAIT)
		goto out;
	tcp_disconnect(tp);
out:
	tcp_bblog_pru(tp, PRU_DISCONNECT, 0);
	TCP_PROBE2(debug__user, tp, PRU_DISCONNECT);
	INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
	return (0);
}

#ifdef INET
/*
 * Accept a connection.  Essentially all the work is done at higher levels;
 * just return the address of the peer, storing through addr.
 */
static int
tcp_usr_accept(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	int error = 0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_accept: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNABORTED);
	}
	tp = intotcpcb(inp);

	if (so->so_state & SS_ISDISCONNECTED)
		error = ECONNABORTED;
	else
		*(struct sockaddr_in *)sa = (struct sockaddr_in ){
			.sin_family = AF_INET,
			.sin_len = sizeof(struct sockaddr_in),
			.sin_port = inp->inp_fport,
			.sin_addr = inp->inp_faddr,
		};
	tcp_bblog_pru(tp, PRU_ACCEPT, error);
	TCP_PROBE2(debug__user, tp, PRU_ACCEPT);
	INP_WUNLOCK(inp);

	return (error);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_usr_accept(struct socket *so, struct sockaddr *sa)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	int error = 0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp6_usr_accept: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNABORTED);
	}
	tp = intotcpcb(inp);

	if (so->so_state & SS_ISDISCONNECTED) {
		error = ECONNABORTED;
	} else {
		if (inp->inp_vflag & INP_IPV4) {
			struct sockaddr_in sin = {
				.sin_family = AF_INET,
				.sin_len = sizeof(struct sockaddr_in),
				.sin_port = inp->inp_fport,
				.sin_addr = inp->inp_faddr,
			};
			in6_sin_2_v4mapsin6(&sin, (struct sockaddr_in6 *)sa);
		} else {
			*(struct sockaddr_in6 *)sa = (struct sockaddr_in6 ){
				.sin6_family = AF_INET6,
				.sin6_len = sizeof(struct sockaddr_in6),
				.sin6_port = inp->inp_fport,
				.sin6_addr = inp->in6p_faddr,
			};
			/* XXX: should catch errors */
			(void)sa6_recoverscope((struct sockaddr_in6 *)sa);
		}
	}

	tcp_bblog_pru(tp, PRU_ACCEPT, error);
	TCP_PROBE2(debug__user, tp, PRU_ACCEPT);
	INP_WUNLOCK(inp);

	return (error);
}
#endif /* INET6 */

/*
 * Mark the connection as being incapable of further output.
 */
static int
tcp_usr_shutdown(struct socket *so, enum shutdown_how how)
{
	struct epoch_tracker et;
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	int error = 0;

	SOCK_LOCK(so);
	if (SOLISTENING(so)) {
		if (how != SHUT_WR) {
			so->so_error = ECONNABORTED;
			solisten_wakeup(so);	/* unlocks so */
		} else
			SOCK_UNLOCK(so);
		return (ENOTCONN);
	} else if ((so->so_state &
	    (SS_ISCONNECTED | SS_ISCONNECTING | SS_ISDISCONNECTING)) == 0) {
		SOCK_UNLOCK(so);
		return (ENOTCONN);
	}
	SOCK_UNLOCK(so);

	switch (how) {
	case SHUT_RD:
		sorflush(so);
		break;
	case SHUT_RDWR:
		sorflush(so);
		/* FALLTHROUGH */
	case SHUT_WR:
		/*
		 * XXXGL: mimicing old soshutdown() here. But shouldn't we
		 * return ECONNRESEST for SHUT_RD as well?
		 */
		INP_WLOCK(inp);
		if (inp->inp_flags & INP_DROPPED) {
			INP_WUNLOCK(inp);
			return (ECONNRESET);
		}

		socantsendmore(so);
		NET_EPOCH_ENTER(et);
		tcp_usrclosed(tp);
		error = tcp_output_nodrop(tp);
		tcp_bblog_pru(tp, PRU_SHUTDOWN, error);
		TCP_PROBE2(debug__user, tp, PRU_SHUTDOWN);
		error = tcp_unlock_or_drop(tp, error);
		NET_EPOCH_EXIT(et);
	}
	wakeup(&so->so_timeo);

	return (error);
}

/*
 * After a receive, possibly send window update to peer.
 */
static int
tcp_usr_rcvd(struct socket *so, int flags)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct tcpcb *tp;
	int outrv = 0, error = 0;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_rcvd: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);

	NET_EPOCH_ENTER(et);
	/*
	 * For passively-created TFO connections, don't attempt a window
	 * update while still in SYN_RECEIVED as this may trigger an early
	 * SYN|ACK.  It is preferable to have the SYN|ACK be sent along with
	 * application response data, or failing that, when the DELACK timer
	 * expires.
	 */
	if ((tp->t_flags & TF_FASTOPEN) && (tp->t_state == TCPS_SYN_RECEIVED))
		goto out;
#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE)
		tcp_offload_rcvd(tp);
	else
#endif
		outrv = tcp_output_nodrop(tp);
out:
	tcp_bblog_pru(tp, PRU_RCVD, error);
	TCP_PROBE2(debug__user, tp, PRU_RCVD);
	(void) tcp_unlock_or_drop(tp, outrv);
	NET_EPOCH_EXIT(et);
	return (error);
}

/*
 * Do a send by putting data in output queue and updating urgent
 * marker if URG set.  Possibly send more data.  Unlike the other
 * pr_*() routines, the mbuf chains are our responsibility.  We
 * must either enqueue them or free them.  The other pr_*() routines
 * generally are caller-frees.
 */
static int
tcp_usr_send(struct socket *so, int flags, struct mbuf *m,
    struct sockaddr *nam, struct mbuf *control, struct thread *td)
{
	struct epoch_tracker et;
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
#ifdef INET
#ifdef INET6
	struct sockaddr_in sin;
#endif
	struct sockaddr_in *sinp;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
	int isipv6;
#endif
	u_int8_t incflagsav;
	u_char vflagsav;
	bool restoreflags;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_send: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		if (m != NULL && (flags & PRUS_NOTREADY) == 0)
			m_freem(m);
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);

	vflagsav = inp->inp_vflag;
	incflagsav = inp->inp_inc.inc_flags;
	restoreflags = false;

	NET_EPOCH_ENTER(et);
	if (control != NULL) {
		/* TCP doesn't do control messages (rights, creds, etc) */
		if (control->m_len > 0) {
			m_freem(control);
			error = EINVAL;
			goto out;
		}
		m_freem(control);	/* empty control, just free it */
	}

	if ((flags & PRUS_OOB) != 0 &&
	    (error = tcp_pru_options_support(tp, PRUS_OOB)) != 0)
		goto out;

	if (nam != NULL && tp->t_state < TCPS_SYN_SENT) {
		if (tp->t_state == TCPS_LISTEN) {
			error = EINVAL;
			goto out;
		}
		switch (nam->sa_family) {
#ifdef INET
		case AF_INET:
			sinp = (struct sockaddr_in *)nam;
			if (sinp->sin_len != sizeof(struct sockaddr_in)) {
				error = EINVAL;
				goto out;
			}
			if ((inp->inp_vflag & INP_IPV6) != 0) {
				error = EAFNOSUPPORT;
				goto out;
			}
			if (IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
				error = EAFNOSUPPORT;
				goto out;
			}
			if (ntohl(sinp->sin_addr.s_addr) == INADDR_BROADCAST) {
				error = EACCES;
				goto out;
			}
			if ((error = prison_remote_ip4(td->td_ucred,
			    &sinp->sin_addr)))
				goto out;
#ifdef INET6
			isipv6 = 0;
#endif
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)nam;
			if (sin6->sin6_len != sizeof(*sin6)) {
				error = EINVAL;
				goto out;
			}
			if ((inp->inp_vflag & INP_IPV6PROTO) == 0) {
				error = EAFNOSUPPORT;
				goto out;
			}
			if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
				error = EAFNOSUPPORT;
				goto out;
			}
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
#ifdef INET
				if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
					error = EINVAL;
					goto out;
				}
				if ((inp->inp_vflag & INP_IPV4) == 0) {
					error = EAFNOSUPPORT;
					goto out;
				}
				restoreflags = true;
				inp->inp_vflag &= ~INP_IPV6;
				sinp = &sin;
				in6_sin6_2_sin(sinp, sin6);
				if (IN_MULTICAST(
				    ntohl(sinp->sin_addr.s_addr))) {
					error = EAFNOSUPPORT;
					goto out;
				}
				if ((error = prison_remote_ip4(td->td_ucred,
				    &sinp->sin_addr)))
					goto out;
				isipv6 = 0;
#else /* !INET */
				error = EAFNOSUPPORT;
				goto out;
#endif /* INET */
			} else {
				if ((inp->inp_vflag & INP_IPV6) == 0) {
					error = EAFNOSUPPORT;
					goto out;
				}
				restoreflags = true;
				inp->inp_vflag &= ~INP_IPV4;
				inp->inp_inc.inc_flags |= INC_ISIPV6;
				if ((error = prison_remote_ip6(td->td_ucred,
				    &sin6->sin6_addr)))
					goto out;
				isipv6 = 1;
			}
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			goto out;
		}
	}
	if (!(flags & PRUS_OOB)) {
		if (tp->t_acktime == 0)
			tp->t_acktime = ticks;
		sbappendstream(&so->so_snd, m, flags);
		m = NULL;
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			KASSERT(tp->t_state == TCPS_CLOSED,
			    ("%s: tp %p is listening", __func__, tp));

			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg using peer's cached MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, sin6, td);
#endif /* INET6 */
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
				error = tcp_connect(tp, sinp, td);
#endif
			/*
			 * The bind operation in tcp_connect succeeded. We
			 * no longer want to restore the flags if later
			 * operations fail.
			 */
			if (error == 0 || inp->inp_lport != 0)
				restoreflags = false;

			if (error) {
				/* m is freed if PRUS_NOTREADY is unset. */
				sbflush(&so->so_snd);
				goto out;
			}
			if (tp->t_flags & TF_FASTOPEN)
				tcp_fastopen_connect(tp);
			else {
				tp->snd_wnd = TTCP_CLIENT_SND_WND;
				tcp_mss(tp, -1);
			}
		}
		if (flags & PRUS_EOF) {
			/*
			 * Close the send side of the connection after
			 * the data is sent.
			 */
			socantsendmore(so);
			tcp_usrclosed(tp);
		}
		if (TCPS_HAVEESTABLISHED(tp->t_state) &&
		    ((tp->t_flags2 & TF2_FBYTES_COMPLETE) == 0) &&
		    (tp->t_fbyte_out == 0) &&
		    (so->so_snd.sb_ccc > 0)) {
			tp->t_fbyte_out = ticks;
			if (tp->t_fbyte_out == 0)
				tp->t_fbyte_out = 1;
			if (tp->t_fbyte_out && tp->t_fbyte_in)
				tp->t_flags2 |= TF2_FBYTES_COMPLETE;
		}
		if (!(inp->inp_flags & INP_DROPPED) &&
		    !(flags & PRUS_NOTREADY)) {
			if (flags & PRUS_MORETOCOME)
				tp->t_flags |= TF_MORETOCOME;
			error = tcp_output_nodrop(tp);
			if (flags & PRUS_MORETOCOME)
				tp->t_flags &= ~TF_MORETOCOME;
		}
	} else {
		/*
		 * XXXRW: PRUS_EOF not implemented with PRUS_OOB?
		 */
		SOCK_SENDBUF_LOCK(so);
		if (sbspace(&so->so_snd) < -512) {
			SOCK_SENDBUF_UNLOCK(so);
			error = ENOBUFS;
			goto out;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		if (tp->t_acktime == 0)
			tp->t_acktime = ticks;
		sbappendstream_locked(&so->so_snd, m, flags);
		SOCK_SENDBUF_UNLOCK(so);
		m = NULL;
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg using peer's cached MSS.
			 */

			/*
			 * Not going to contemplate SYN|URG
			 */
			if (tp->t_flags & TF_FASTOPEN)
				tp->t_flags &= ~TF_FASTOPEN;
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, sin6, td);
#endif /* INET6 */
#if defined(INET6) && defined(INET)
			else
#endif
#ifdef INET
				error = tcp_connect(tp, sinp, td);
#endif
			/*
			 * The bind operation in tcp_connect succeeded. We
			 * no longer want to restore the flags if later
			 * operations fail.
			 */
			if (error == 0 || inp->inp_lport != 0)
				restoreflags = false;

			if (error != 0) {
				/* m is freed if PRUS_NOTREADY is unset. */
				sbflush(&so->so_snd);
				goto out;
			}
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}
		tp->snd_up = tp->snd_una + sbavail(&so->so_snd);
		if ((flags & PRUS_NOTREADY) == 0) {
			tp->t_flags |= TF_FORCEDATA;
			error = tcp_output_nodrop(tp);
			tp->t_flags &= ~TF_FORCEDATA;
		}
	}
	TCP_LOG_EVENT(tp, NULL,
	    &inp->inp_socket->so_rcv,
	    &inp->inp_socket->so_snd,
	    TCP_LOG_USERSEND, error,
	    0, NULL, false);

out:
	/*
	 * In case of PRUS_NOTREADY, the caller or tcp_usr_ready() is
	 * responsible for freeing memory.
	 */
	if (m != NULL && (flags & PRUS_NOTREADY) == 0)
		m_freem(m);

	/*
	 * If the request was unsuccessful and we changed flags,
	 * restore the original flags.
	 */
	if (error != 0 && restoreflags) {
		inp->inp_vflag = vflagsav;
		inp->inp_inc.inc_flags = incflagsav;
	}
	tcp_bblog_pru(tp, (flags & PRUS_OOB) ? PRU_SENDOOB :
		      ((flags & PRUS_EOF) ? PRU_SEND_EOF : PRU_SEND), error);
	TCP_PROBE2(debug__user, tp, (flags & PRUS_OOB) ? PRU_SENDOOB :
		   ((flags & PRUS_EOF) ? PRU_SEND_EOF : PRU_SEND));
	error = tcp_unlock_or_drop(tp, error);
	NET_EPOCH_EXIT(et);
	return (error);
}

static int
tcp_usr_ready(struct socket *so, struct mbuf *m, int count)
{
	struct epoch_tracker et;
	struct inpcb *inp;
	struct tcpcb *tp;
	int error;

	inp = sotoinpcb(so);
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		mb_free_notready(m, count);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);

	SOCK_SENDBUF_LOCK(so);
	error = sbready(&so->so_snd, m, count);
	SOCK_SENDBUF_UNLOCK(so);
	if (error) {
		INP_WUNLOCK(inp);
		return (error);
	}
	NET_EPOCH_ENTER(et);
	error = tcp_output_unlock(tp);
	NET_EPOCH_EXIT(et);

	return (error);
}

/*
 * Abort the TCP.  Drop the connection abruptly.
 */
static void
tcp_usr_abort(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	struct epoch_tracker et;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_abort: inp == NULL"));

	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	KASSERT(inp->inp_socket != NULL,
	    ("tcp_usr_abort: inp_socket == NULL"));

	/*
	 * If we still have full TCP state, and we're not dropped, drop.
	 */
	if (!(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		tp = tcp_drop(tp, ECONNABORTED);
		if (tp == NULL)
			goto dropped;
		tcp_bblog_pru(tp, PRU_ABORT, 0);
		TCP_PROBE2(debug__user, tp, PRU_ABORT);
	}
	if (!(inp->inp_flags & INP_DROPPED)) {
		soref(so);
		inp->inp_flags |= INP_SOCKREF;
	}
	INP_WUNLOCK(inp);
dropped:
	NET_EPOCH_EXIT(et);
}

/*
 * TCP socket is closed.  Start friendly disconnect.
 */
static void
tcp_usr_close(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *tp;
	struct epoch_tracker et;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_close: inp == NULL"));

	NET_EPOCH_ENTER(et);
	INP_WLOCK(inp);
	KASSERT(inp->inp_socket != NULL,
	    ("tcp_usr_close: inp_socket == NULL"));

	/*
	 * If we are still connected and we're not dropped, initiate
	 * a disconnect.
	 */
	if (!(inp->inp_flags & INP_DROPPED)) {
		tp = intotcpcb(inp);
		if (tp->t_state != TCPS_TIME_WAIT) {
			tp->t_flags |= TF_CLOSED;
			tcp_disconnect(tp);
			tcp_bblog_pru(tp, PRU_CLOSE, 0);
			TCP_PROBE2(debug__user, tp, PRU_CLOSE);
		}
	}
	if (!(inp->inp_flags & INP_DROPPED)) {
		soref(so);
		inp->inp_flags |= INP_SOCKREF;
	}
	INP_WUNLOCK(inp);
	NET_EPOCH_EXIT(et);
}

static int
tcp_pru_options_support(struct tcpcb *tp, int flags)
{
	/*
	 * If the specific TCP stack has a pru_options
	 * specified then it does not always support
	 * all the PRU_XX options and we must ask it.
	 * If the function is not specified then all
	 * of the PRU_XX options are supported.
	 */
	int ret = 0;

	if (tp->t_fb->tfb_pru_options) {
		ret = (*tp->t_fb->tfb_pru_options)(tp, flags);
	}
	return (ret);
}

/*
 * Receive out-of-band data.
 */
static int
tcp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_usr_rcvoob: inp == NULL"));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);

	error = tcp_pru_options_support(tp, PRUS_OOB);
	if (error) {
		goto out;
	}
	if ((so->so_oobmark == 0 &&
	     (so->so_rcv.sb_state & SBS_RCVATMARK) == 0) ||
	    so->so_options & SO_OOBINLINE ||
	    tp->t_oobflags & TCPOOB_HADDATA) {
		error = EINVAL;
		goto out;
	}
	if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
		error = EWOULDBLOCK;
		goto out;
	}
	m->m_len = 1;
	*mtod(m, caddr_t) = tp->t_iobc;
	if ((flags & MSG_PEEK) == 0)
		tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);

out:
	tcp_bblog_pru(tp, PRU_RCVOOB, error);
	TCP_PROBE2(debug__user, tp, PRU_RCVOOB);
	INP_WUNLOCK(inp);
	return (error);
}

#ifdef INET
struct protosw tcp_protosw = {
	.pr_type =		SOCK_STREAM,
	.pr_protocol =		IPPROTO_TCP,
	.pr_flags =		PR_CONNREQUIRED | PR_IMPLOPCL | PR_WANTRCVD |
				    PR_CAPATTACH,
	.pr_ctloutput =		tcp_ctloutput,
	.pr_abort =		tcp_usr_abort,
	.pr_accept =		tcp_usr_accept,
	.pr_attach =		tcp_usr_attach,
	.pr_bind =		tcp_usr_bind,
	.pr_connect =		tcp_usr_connect,
	.pr_control =		in_control,
	.pr_detach =		tcp_usr_detach,
	.pr_disconnect =	tcp_usr_disconnect,
	.pr_listen =		tcp_usr_listen,
	.pr_peeraddr =		in_getpeeraddr,
	.pr_rcvd =		tcp_usr_rcvd,
	.pr_rcvoob =		tcp_usr_rcvoob,
	.pr_send =		tcp_usr_send,
	.pr_sendfile_wait =	sendfile_wait_generic,
	.pr_ready =		tcp_usr_ready,
	.pr_shutdown =		tcp_usr_shutdown,
	.pr_sockaddr =		in_getsockaddr,
	.pr_sosetlabel =	in_pcbsosetlabel,
	.pr_close =		tcp_usr_close,
};
#endif /* INET */

#ifdef INET6
struct protosw tcp6_protosw = {
	.pr_type =		SOCK_STREAM,
	.pr_protocol =		IPPROTO_TCP,
	.pr_flags =		PR_CONNREQUIRED | PR_IMPLOPCL |PR_WANTRCVD |
				    PR_CAPATTACH,
	.pr_ctloutput =		tcp_ctloutput,
	.pr_abort =		tcp_usr_abort,
	.pr_accept =		tcp6_usr_accept,
	.pr_attach =		tcp_usr_attach,
	.pr_bind =		tcp6_usr_bind,
	.pr_connect =		tcp6_usr_connect,
	.pr_control =		in6_control,
	.pr_detach =		tcp_usr_detach,
	.pr_disconnect =	tcp_usr_disconnect,
	.pr_listen =		tcp6_usr_listen,
	.pr_peeraddr =		in6_mapped_peeraddr,
	.pr_rcvd =		tcp_usr_rcvd,
	.pr_rcvoob =		tcp_usr_rcvoob,
	.pr_send =		tcp_usr_send,
	.pr_sendfile_wait =	sendfile_wait_generic,
	.pr_ready =		tcp_usr_ready,
	.pr_shutdown =		tcp_usr_shutdown,
	.pr_sockaddr =		in6_mapped_sockaddr,
	.pr_sosetlabel =	in_pcbsosetlabel,
	.pr_close =		tcp_usr_close,
};
#endif /* INET6 */

#ifdef INET
/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct sockaddr_in.  Call in_pcbconnect() to choose local host address
 * and assign a local port number and install the inpcb into the hash.
 * Initialize connection parameters and enter SYN-SENT state.
 */
static int
tcp_connect(struct tcpcb *tp, struct sockaddr_in *sin, struct thread *td)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);
	int error;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	if (__predict_false((so->so_state &
	    (SS_ISCONNECTING | SS_ISCONNECTED | SS_ISDISCONNECTING |
	    SS_ISDISCONNECTED)) != 0))
		return (EISCONN);

	INP_HASH_WLOCK(&V_tcbinfo);
	error = in_pcbconnect(inp, sin, td->td_ucred);
	INP_HASH_WUNLOCK(&V_tcbinfo);
	if (error != 0)
		return (error);

	/*
	 * Compute window scaling to request:
	 * Scale to fit into sweet spot.  See tcp_syncache.c.
	 * XXX: This should move to tcp_output().
	 */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT_INC(tcps_connattempt);
	tcp_state_change(tp, TCPS_SYN_SENT);
	tp->iss = tcp_new_isn(&inp->inp_inc);
	if (tp->t_flags & TF_REQ_TSTMP)
		tp->ts_offset = tcp_new_ts_offset(&inp->inp_inc);
	tcp_sendseqinit(tp);

	return (0);
}
#endif /* INET */

#ifdef INET6
static int
tcp6_connect(struct tcpcb *tp, struct sockaddr_in6 *sin6, struct thread *td)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);
	int error;

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	if (__predict_false((so->so_state &
	    (SS_ISCONNECTING | SS_ISCONNECTED)) != 0))
		return (EISCONN);

	INP_HASH_WLOCK(&V_tcbinfo);
	error = in6_pcbconnect(inp, sin6, td->td_ucred, true);
	INP_HASH_WUNLOCK(&V_tcbinfo);
	if (error != 0)
		return (error);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < sb_max)
		tp->request_r_scale++;

	soisconnecting(so);
	TCPSTAT_INC(tcps_connattempt);
	tcp_state_change(tp, TCPS_SYN_SENT);
	tp->iss = tcp_new_isn(&inp->inp_inc);
	if (tp->t_flags & TF_REQ_TSTMP)
		tp->ts_offset = tcp_new_ts_offset(&inp->inp_inc);
	tcp_sendseqinit(tp);

	return (0);
}
#endif /* INET6 */

/*
 * Export TCP internal state information via a struct tcp_info, based on the
 * Linux 2.6 API.  Not ABI compatible as our constants are mapped differently
 * (TCP state machine, etc).  We export all information using FreeBSD-native
 * constants -- for example, the numeric values for tcpi_state will differ
 * from Linux.
 */
void
tcp_fill_info(const struct tcpcb *tp, struct tcp_info *ti)
{

	INP_LOCK_ASSERT(tptoinpcb(tp));
	bzero(ti, sizeof(*ti));

	ti->tcpi_state = tp->t_state;
	if ((tp->t_flags & TF_REQ_TSTMP) && (tp->t_flags & TF_RCVD_TSTMP))
		ti->tcpi_options |= TCPI_OPT_TIMESTAMPS;
	if (tp->t_flags & TF_SACK_PERMIT)
		ti->tcpi_options |= TCPI_OPT_SACK;
	if ((tp->t_flags & TF_REQ_SCALE) && (tp->t_flags & TF_RCVD_SCALE)) {
		ti->tcpi_options |= TCPI_OPT_WSCALE;
		ti->tcpi_snd_wscale = tp->snd_scale;
		ti->tcpi_rcv_wscale = tp->rcv_scale;
	}
	switch (tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT)) {
		case TF2_ECN_PERMIT:
			ti->tcpi_options |= TCPI_OPT_ECN;
			break;
		case TF2_ACE_PERMIT:
			/* FALLTHROUGH */
		case TF2_ECN_PERMIT | TF2_ACE_PERMIT:
			ti->tcpi_options |= TCPI_OPT_ACE;
			break;
		default:
			break;
	}
	if (tp->t_flags & TF_FASTOPEN)
		ti->tcpi_options |= TCPI_OPT_TFO;

	ti->tcpi_rto = tp->t_rxtcur * tick;
	ti->tcpi_last_data_recv = ((uint32_t)ticks - tp->t_rcvtime) * tick;
	ti->tcpi_rtt = ((u_int64_t)tp->t_srtt * tick) >> TCP_RTT_SHIFT;
	ti->tcpi_rttvar = ((u_int64_t)tp->t_rttvar * tick) >> TCP_RTTVAR_SHIFT;

	ti->tcpi_snd_ssthresh = tp->snd_ssthresh;
	ti->tcpi_snd_cwnd = tp->snd_cwnd;

	/*
	 * FreeBSD-specific extension fields for tcp_info.
	 */
	ti->tcpi_rcv_space = tp->rcv_wnd;
	ti->tcpi_rcv_nxt = tp->rcv_nxt;
	ti->tcpi_snd_wnd = tp->snd_wnd;
	ti->tcpi_snd_bwnd = 0;		/* Unused, kept for compat. */
	ti->tcpi_snd_nxt = tp->snd_nxt;
	ti->tcpi_snd_mss = tp->t_maxseg;
	ti->tcpi_rcv_mss = tp->t_maxseg;
	ti->tcpi_snd_rexmitpack = tp->t_sndrexmitpack;
	ti->tcpi_rcv_ooopack = tp->t_rcvoopack;
	ti->tcpi_snd_zerowin = tp->t_sndzerowin;
	ti->tcpi_snd_una = tp->snd_una;
	ti->tcpi_snd_max = tp->snd_max;
	ti->tcpi_rcv_numsacks = tp->rcv_numsacks;
	ti->tcpi_rcv_adv = tp->rcv_adv;
	ti->tcpi_dupacks = tp->t_dupacks;
	ti->tcpi_rttmin = tp->t_rttlow;
#ifdef TCP_OFFLOAD
	if (tp->t_flags & TF_TOE) {
		ti->tcpi_options |= TCPI_OPT_TOE;
		tcp_offload_tcp_info(tp, ti);
	}
#endif
	/*
	 * AccECN related counters.
	 */
	if ((tp->t_flags2 & (TF2_ECN_PERMIT | TF2_ACE_PERMIT)) ==
	    (TF2_ECN_PERMIT | TF2_ACE_PERMIT))
		/*
		 * Internal counter starts at 5 for AccECN
		 * but 0 for RFC3168 ECN.
		 */
		ti->tcpi_delivered_ce = tp->t_scep - 5;
	else
		ti->tcpi_delivered_ce = tp->t_scep;
	ti->tcpi_received_ce = tp->t_rcep;
}

/*
 * tcp_ctloutput() must drop the inpcb lock before performing copyin on
 * socket option arguments.  When it re-acquires the lock after the copy, it
 * has to revalidate that the connection is still valid for the socket
 * option.
 */
#define INP_WLOCK_RECHECK_CLEANUP(inp, cleanup) do {			\
	INP_WLOCK(inp);							\
	if (inp->inp_flags & INP_DROPPED) {				\
		INP_WUNLOCK(inp);					\
		cleanup;						\
		return (ECONNRESET);					\
	}								\
	tp = intotcpcb(inp);						\
} while(0)
#define INP_WLOCK_RECHECK(inp) INP_WLOCK_RECHECK_CLEANUP((inp), /* noop */)

int
tcp_ctloutput_set(struct inpcb *inp, struct sockopt *sopt)
{
	struct socket *so = inp->inp_socket;
	struct tcpcb *tp = intotcpcb(inp);
	int error = 0;

	MPASS(sopt->sopt_dir == SOPT_SET);
	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("inp_flags == %x", inp->inp_flags));
	KASSERT(so != NULL, ("inp_socket == NULL"));

	if (sopt->sopt_level != IPPROTO_TCP) {
		INP_WUNLOCK(inp);
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6PROTO)
			error = ip6_ctloutput(so, sopt);
#endif
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET
			error = ip_ctloutput(so, sopt);
#endif
		/*
		 * When an IP-level socket option affects TCP, pass control
		 * down to stack tfb_tcp_ctloutput, otherwise return what
		 * IP level returned.
		 */
		switch (sopt->sopt_level) {
#ifdef INET6
		case IPPROTO_IPV6:
			if ((inp->inp_vflag & INP_IPV6PROTO) == 0)
				return (error);
			switch (sopt->sopt_name) {
			case IPV6_TCLASS:
				/* Notify tcp stacks that care (e.g. RACK). */
				break;
			case IPV6_USE_MIN_MTU:
				/* Update t_maxseg accordingly. */
				break;
			default:
				return (error);
			}
			break;
#endif
#ifdef INET
		case IPPROTO_IP:
			switch (sopt->sopt_name) {
			case IP_TOS:
				inp->inp_ip_tos &= ~IPTOS_ECN_MASK;
				break;
			case IP_TTL:
				/* Notify tcp stacks that care (e.g. RACK). */
				break;
			default:
				return (error);
			}
			break;
#endif
		default:
			return (error);
		}
		INP_WLOCK_RECHECK(inp);
	} else if (sopt->sopt_name == TCP_FUNCTION_BLK) {
		/*
		 * Protect the TCP option TCP_FUNCTION_BLK so
		 * that a sub-function can *never* overwrite this.
		 */
		struct tcp_function_set fsn;
		struct tcp_function_block *blk;
		void *ptr = NULL;

		INP_WUNLOCK(inp);
		error = sooptcopyin(sopt, &fsn, sizeof fsn, sizeof fsn);
		if (error)
			return (error);

		INP_WLOCK_RECHECK(inp);

		blk = find_and_ref_tcp_functions(&fsn);
		if (blk == NULL) {
			INP_WUNLOCK(inp);
			return (ENOENT);
		}
		if (tp->t_fb == blk) {
			/* You already have this */
			refcount_release(&blk->tfb_refcnt);
			INP_WUNLOCK(inp);
			return (0);
		}
		if (blk->tfb_flags & TCP_FUNC_BEING_REMOVED) {
			refcount_release(&blk->tfb_refcnt);
			INP_WUNLOCK(inp);
			return (ENOENT);
		}
		error = (*blk->tfb_tcp_handoff_ok)(tp);
		if (error) {
			refcount_release(&blk->tfb_refcnt);
			INP_WUNLOCK(inp);
			return (error);
		}
		/*
		 * Ensure the new stack takes ownership with a
		 * clean slate on peak rate threshold.
		 */
		if (tp->t_fb->tfb_tcp_timer_stop_all != NULL)
			tp->t_fb->tfb_tcp_timer_stop_all(tp);
		if (blk->tfb_tcp_fb_init) {
			error = (*blk->tfb_tcp_fb_init)(tp, &ptr);
			if (error) {
				/*
				 * Release the ref count the lookup
				 * acquired.
				 */ 
				refcount_release(&blk->tfb_refcnt);
				/* 
				 * Now there is a chance that the
				 * init() function mucked with some
				 * things before it failed, such as
				 * hpts or inp_flags2 or timer granularity.
				 * It should not of, but lets give the old
				 * stack a chance to reset to a known good state.
				 */
				if (tp->t_fb->tfb_switch_failed) {
					(*tp->t_fb->tfb_switch_failed)(tp);
				}
			 	goto err_out;
			}
		}
		if (tp->t_fb->tfb_tcp_fb_fini) {
			struct epoch_tracker et;
			/*
			 * Tell the stack to cleanup with 0 i.e.
			 * the tcb is not going away.
			 */
			NET_EPOCH_ENTER(et);
			(*tp->t_fb->tfb_tcp_fb_fini)(tp, 0);
			NET_EPOCH_EXIT(et);
		}
		/*
		 * Release the old refcnt, the
		 * lookup acquired a ref on the
		 * new one already.
		 */
		refcount_release(&tp->t_fb->tfb_refcnt);
		/* 
		 * Set in the new stack.
		 */
		tp->t_fb = blk;
		tp->t_fb_ptr = ptr;
#ifdef TCP_OFFLOAD
		if (tp->t_flags & TF_TOE) {
			tcp_offload_ctloutput(tp, sopt->sopt_dir,
			     sopt->sopt_name);
		}
#endif
err_out:
		INP_WUNLOCK(inp);
		return (error);

	}

	/* Pass in the INP locked, callee must unlock it. */
	return (tp->t_fb->tfb_tcp_ctloutput(tp, sopt));
}

static int
tcp_ctloutput_get(struct inpcb *inp, struct sockopt *sopt)
{
	struct socket *so = inp->inp_socket;
	struct tcpcb *tp = intotcpcb(inp);
	int error = 0;

	MPASS(sopt->sopt_dir == SOPT_GET);
	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("inp_flags == %x", inp->inp_flags));
	KASSERT(so != NULL, ("inp_socket == NULL"));

	if (sopt->sopt_level != IPPROTO_TCP) {
		INP_WUNLOCK(inp);
#ifdef INET6
		if (inp->inp_vflag & INP_IPV6PROTO)
			error = ip6_ctloutput(so, sopt);
#endif /* INET6 */
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET
			error = ip_ctloutput(so, sopt);
#endif
		return (error);
	}
	if (((sopt->sopt_name == TCP_FUNCTION_BLK) ||
	     (sopt->sopt_name == TCP_FUNCTION_ALIAS))) {
		struct tcp_function_set fsn;

		if (sopt->sopt_name == TCP_FUNCTION_ALIAS) {
			memset(&fsn, 0, sizeof(fsn));
			find_tcp_function_alias(tp->t_fb, &fsn);
		} else {
			strncpy(fsn.function_set_name,
			    tp->t_fb->tfb_tcp_block_name,
			    TCP_FUNCTION_NAME_LEN_MAX);
			fsn.function_set_name[TCP_FUNCTION_NAME_LEN_MAX - 1] = '\0';
		}
		fsn.pcbcnt = tp->t_fb->tfb_refcnt;
		INP_WUNLOCK(inp);
		error = sooptcopyout(sopt, &fsn, sizeof fsn);
		return (error);
	}

	/* Pass in the INP locked, callee must unlock it. */
	return (tp->t_fb->tfb_tcp_ctloutput(tp, sopt));
}

int
tcp_ctloutput(struct socket *so, struct sockopt *sopt)
{
	struct	inpcb *inp;

	inp = sotoinpcb(so);
	KASSERT(inp != NULL, ("tcp_ctloutput: inp == NULL"));

	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		return (ECONNRESET);
	}
	if (sopt->sopt_dir == SOPT_SET)
		return (tcp_ctloutput_set(inp, sopt));
	else if (sopt->sopt_dir == SOPT_GET)
		return (tcp_ctloutput_get(inp, sopt));
	else
		panic("%s: sopt_dir $%d", __func__, sopt->sopt_dir);
}

/*
 * If this assert becomes untrue, we need to change the size of the buf
 * variable in tcp_default_ctloutput().
 */
#ifdef CTASSERT
CTASSERT(TCP_CA_NAME_MAX <= TCP_LOG_ID_LEN);
CTASSERT(TCP_LOG_REASON_LEN <= TCP_LOG_ID_LEN);
#endif

extern struct cc_algo newreno_cc_algo;

static int
tcp_set_cc_mod(struct inpcb *inp, struct sockopt *sopt)
{
	struct cc_algo *algo;
	void *ptr = NULL;
	struct tcpcb *tp;
	struct cc_var cc_mem;
	char	buf[TCP_CA_NAME_MAX];
	size_t mem_sz;
	int error;

	INP_WUNLOCK(inp);
	error = sooptcopyin(sopt, buf, TCP_CA_NAME_MAX - 1, 1);
	if (error)
		return(error);
	buf[sopt->sopt_valsize] = '\0';
	CC_LIST_RLOCK();
	STAILQ_FOREACH(algo, &cc_list, entries) {
		if (strncmp(buf, algo->name,
			    TCP_CA_NAME_MAX) == 0) {
			if (algo->flags & CC_MODULE_BEING_REMOVED) {
				/* We can't "see" modules being unloaded */
				continue;
			}
			break;
		}
	}
	if (algo == NULL) {
		CC_LIST_RUNLOCK();
		return(ESRCH);
	}
	/* 
	 * With a reference the algorithm cannot be removed
	 * so we hold a reference through the change process.
	 */
	cc_refer(algo);
	CC_LIST_RUNLOCK();
	if (algo->cb_init != NULL) {
		/* We can now pre-get the memory for the CC */
		mem_sz = (*algo->cc_data_sz)();
		if (mem_sz == 0) {
			goto no_mem_needed;
		}
		ptr = malloc(mem_sz, M_CC_MEM, M_WAITOK);
	} else {
no_mem_needed:
		mem_sz = 0;
		ptr = NULL;
	}
	/*
	 * Make sure its all clean and zero and also get
	 * back the inplock.
	 */
	memset(&cc_mem, 0, sizeof(cc_mem));
	INP_WLOCK(inp);
	if (inp->inp_flags & INP_DROPPED) {
		INP_WUNLOCK(inp);
		if (ptr)
			free(ptr, M_CC_MEM);
		/* Release our temp reference */
		CC_LIST_RLOCK();
		cc_release(algo);
		CC_LIST_RUNLOCK();
		return (ECONNRESET);
	}
	tp = intotcpcb(inp);
	if (ptr != NULL)
		memset(ptr, 0, mem_sz);
	cc_mem.tp = tp;
	/*
	 * We once again hold a write lock over the tcb so it's
	 * safe to do these things without ordering concerns.
	 * Note here we init into stack memory.
	 */
	if (algo->cb_init != NULL)
		error = algo->cb_init(&cc_mem, ptr);
	else
		error = 0;
	/*
	 * The CC algorithms, when given their memory
	 * should not fail we could in theory have a
	 * KASSERT here.
	 */
	if (error == 0) {
		/*
		 * Touchdown, lets go ahead and move the
		 * connection to the new CC module by
		 * copying in the cc_mem after we call
		 * the old ones cleanup (if any).
		 */
		if (CC_ALGO(tp)->cb_destroy != NULL)
			CC_ALGO(tp)->cb_destroy(&tp->t_ccv);
		/* Detach the old CC from the tcpcb  */
		cc_detach(tp);
		/* Copy in our temp memory that was inited */
		memcpy(&tp->t_ccv, &cc_mem, sizeof(struct cc_var));
		/* Now attach the new, which takes a reference */
		cc_attach(tp, algo);
		/* Ok now are we where we have gotten past any conn_init? */
		if (TCPS_HAVEESTABLISHED(tp->t_state) && (CC_ALGO(tp)->conn_init != NULL)) {
			/* Yep run the connection init for the new CC */
			CC_ALGO(tp)->conn_init(&tp->t_ccv);
		}
	} else if (ptr)
		free(ptr, M_CC_MEM);
	INP_WUNLOCK(inp);
	/* Now lets release our temp reference */
	CC_LIST_RLOCK();
	cc_release(algo);
	CC_LIST_RUNLOCK();
	return (error);
}

int
tcp_default_ctloutput(struct tcpcb *tp, struct sockopt *sopt)
{
	struct inpcb *inp = tptoinpcb(tp);
	int	error, opt, optval;
	u_int	ui;
	struct	tcp_info ti;
#ifdef KERN_TLS
	struct tls_enable tls;
	struct socket *so = inp->inp_socket;
#endif
	char	*pbuf, buf[TCP_LOG_ID_LEN];
#ifdef STATS
	struct statsblob *sbp;
#endif
	size_t	len;

	INP_WLOCK_ASSERT(inp);
	KASSERT((inp->inp_flags & INP_DROPPED) == 0,
	    ("inp_flags == %x", inp->inp_flags));
	KASSERT(inp->inp_socket != NULL, ("inp_socket == NULL"));

	switch (sopt->sopt_level) {
#ifdef INET6
	case IPPROTO_IPV6:
		MPASS(inp->inp_vflag & INP_IPV6PROTO);
		switch (sopt->sopt_name) {
		case IPV6_USE_MIN_MTU:
			tcp6_use_min_mtu(tp);
			/* FALLTHROUGH */
		}
		INP_WUNLOCK(inp);
		return (0);
#endif
#ifdef INET
	case IPPROTO_IP:
		INP_WUNLOCK(inp);
		return (0);
#endif
	}

	/*
	 * For TCP_CCALGOOPT forward the control to CC module, for both
	 * SOPT_SET and SOPT_GET.
	 */
	switch (sopt->sopt_name) {
	case TCP_CCALGOOPT:
		INP_WUNLOCK(inp);
		if (sopt->sopt_valsize > CC_ALGOOPT_LIMIT)
			return (EINVAL);
		pbuf = malloc(sopt->sopt_valsize, M_TEMP, M_WAITOK | M_ZERO);
		error = sooptcopyin(sopt, pbuf, sopt->sopt_valsize,
		    sopt->sopt_valsize);
		if (error) {
			free(pbuf, M_TEMP);
			return (error);
		}
		INP_WLOCK_RECHECK_CLEANUP(inp, free(pbuf, M_TEMP));
		if (CC_ALGO(tp)->ctl_output != NULL)
			error = CC_ALGO(tp)->ctl_output(&tp->t_ccv, sopt, pbuf);
		else
			error = ENOENT;
		INP_WUNLOCK(inp);
		if (error == 0 && sopt->sopt_dir == SOPT_GET)
			error = sooptcopyout(sopt, pbuf, sopt->sopt_valsize);
		free(pbuf, M_TEMP);
		return (error);
	}

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		case TCP_MD5SIG:
			INP_WUNLOCK(inp);
			if (!TCPMD5_ENABLED())
				return (ENOPROTOOPT);
			error = TCPMD5_PCBCTL(inp, sopt);
			if (error)
				return (error);
			INP_WLOCK_RECHECK(inp);
			goto unlock_and_done;
#endif /* IPSEC */

		case TCP_NODELAY:
		case TCP_NOOPT:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			switch (sopt->sopt_name) {
			case TCP_NODELAY:
				opt = TF_NODELAY;
				break;
			case TCP_NOOPT:
				opt = TF_NOOPT;
				break;
			default:
				opt = 0; /* dead code to fool gcc */
				break;
			}

			if (optval)
				tp->t_flags |= opt;
			else
				tp->t_flags &= ~opt;
unlock_and_done:
#ifdef TCP_OFFLOAD
			if (tp->t_flags & TF_TOE) {
				tcp_offload_ctloutput(tp, sopt->sopt_dir,
				    sopt->sopt_name);
			}
#endif
			INP_WUNLOCK(inp);
			break;

		case TCP_NOPUSH:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			if (optval)
				tp->t_flags |= TF_NOPUSH;
			else if (tp->t_flags & TF_NOPUSH) {
				tp->t_flags &= ~TF_NOPUSH;
				if (TCPS_HAVEESTABLISHED(tp->t_state)) {
					struct epoch_tracker et;

					NET_EPOCH_ENTER(et);
					error = tcp_output_nodrop(tp);
					NET_EPOCH_EXIT(et);
				}
			}
			goto unlock_and_done;

		case TCP_REMOTE_UDP_ENCAPS_PORT:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);
			if ((optval < TCP_TUNNELING_PORT_MIN) ||
			    (optval > TCP_TUNNELING_PORT_MAX)) {
				/* Its got to be in range */
				return (EINVAL);
			}
			if ((V_tcp_udp_tunneling_port == 0) && (optval != 0)) {
				/* You have to have enabled a UDP tunneling port first */
				return (EINVAL);
			}
			INP_WLOCK_RECHECK(inp);
			if (tp->t_state != TCPS_CLOSED) {
				/* You can't change after you are connected */
				error = EINVAL;
			} else {
				/* Ok we are all good set the port */
				tp->t_port = htons(optval);
			}
			goto unlock_and_done;

		case TCP_MAXSEG:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			if (optval > 0 && optval <= tp->t_maxseg &&
			    optval + 40 >= V_tcp_minmss) {
				tp->t_maxseg = optval;
				if (tp->t_maxseg < V_tcp_mssdflt) {
					/*
					 * The MSS is so small we should not process incoming
					 * SACK's since we are subject to attack in such a
					 * case.
					 */
					tp->t_flags2 |= TF2_PROC_SACK_PROHIBIT;
				} else {
					tp->t_flags2 &= ~TF2_PROC_SACK_PROHIBIT;
				}
			} else
				error = EINVAL;
			goto unlock_and_done;

		case TCP_INFO:
			INP_WUNLOCK(inp);
			error = EINVAL;
			break;

		case TCP_STATS:
			INP_WUNLOCK(inp);
#ifdef STATS
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			if (optval > 0)
				sbp = stats_blob_alloc(
				    V_tcp_perconn_stats_dflt_tpl, 0);
			else
				sbp = NULL;

			INP_WLOCK_RECHECK(inp);
			if ((tp->t_stats != NULL && sbp == NULL) ||
			    (tp->t_stats == NULL && sbp != NULL)) {
				struct statsblob *t = tp->t_stats;
				tp->t_stats = sbp;
				sbp = t;
			}
			INP_WUNLOCK(inp);

			stats_blob_destroy(sbp);
#else
			return (EOPNOTSUPP);
#endif /* !STATS */
			break;

		case TCP_CONGESTION:
			error = tcp_set_cc_mod(inp, sopt);
			break;

		case TCP_REUSPORT_LB_NUMA:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof(optval),
			    sizeof(optval));
			INP_WLOCK_RECHECK(inp);
			if (!error)
				error = in_pcblbgroup_numa(inp, optval);
			INP_WUNLOCK(inp);
			break;

#ifdef KERN_TLS
		case TCP_TXTLS_ENABLE:
			INP_WUNLOCK(inp);
			error = ktls_copyin_tls_enable(sopt, &tls);
			if (error != 0)
				break;
			error = ktls_enable_tx(so, &tls);
			ktls_cleanup_tls_enable(&tls);
			break;
		case TCP_TXTLS_MODE:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &ui, sizeof(ui), sizeof(ui));
			if (error != 0)
				return (error);

			INP_WLOCK_RECHECK(inp);
			error = ktls_set_tx_mode(so, ui);
			INP_WUNLOCK(inp);
			break;
		case TCP_RXTLS_ENABLE:
			INP_WUNLOCK(inp);
			error = ktls_copyin_tls_enable(sopt, &tls);
			if (error != 0)
				break;
			error = ktls_enable_rx(so, &tls);
			ktls_cleanup_tls_enable(&tls);
			break;
#endif
		case TCP_MAXUNACKTIME:
		case TCP_KEEPIDLE:
		case TCP_KEEPINTVL:
		case TCP_KEEPINIT:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &ui, sizeof(ui), sizeof(ui));
			if (error)
				return (error);

			if (ui > (UINT_MAX / hz)) {
				error = EINVAL;
				break;
			}
			ui *= hz;

			INP_WLOCK_RECHECK(inp);
			switch (sopt->sopt_name) {
			case TCP_MAXUNACKTIME:
				tp->t_maxunacktime = ui;
				break;

			case TCP_KEEPIDLE:
				tp->t_keepidle = ui;
				/*
				 * XXX: better check current remaining
				 * timeout and "merge" it with new value.
				 */
				if ((tp->t_state > TCPS_LISTEN) &&
				    (tp->t_state <= TCPS_CLOSING))
					tcp_timer_activate(tp, TT_KEEP,
					    TP_KEEPIDLE(tp));
				break;
			case TCP_KEEPINTVL:
				tp->t_keepintvl = ui;
				if ((tp->t_state == TCPS_FIN_WAIT_2) &&
				    (TP_MAXIDLE(tp) > 0))
					tcp_timer_activate(tp, TT_2MSL,
					    TP_MAXIDLE(tp));
				break;
			case TCP_KEEPINIT:
				tp->t_keepinit = ui;
				if (tp->t_state == TCPS_SYN_RECEIVED ||
				    tp->t_state == TCPS_SYN_SENT)
					tcp_timer_activate(tp, TT_KEEP,
					    TP_KEEPINIT(tp));
				break;
			}
			goto unlock_and_done;

		case TCP_KEEPCNT:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &ui, sizeof(ui), sizeof(ui));
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			tp->t_keepcnt = ui;
			if ((tp->t_state == TCPS_FIN_WAIT_2) &&
			    (TP_MAXIDLE(tp) > 0))
				tcp_timer_activate(tp, TT_2MSL,
				    TP_MAXIDLE(tp));
			goto unlock_and_done;

		case TCP_FASTOPEN: {
			struct tcp_fastopen tfo_optval;

			INP_WUNLOCK(inp);
			if (!V_tcp_fastopen_client_enable &&
			    !V_tcp_fastopen_server_enable)
				return (EPERM);

			error = sooptcopyin(sopt, &tfo_optval,
				    sizeof(tfo_optval), sizeof(int));
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			if ((tp->t_state != TCPS_CLOSED) &&
			    (tp->t_state != TCPS_LISTEN)) {
				error = EINVAL;
				goto unlock_and_done;
			}
			if (tfo_optval.enable) {
				if (tp->t_state == TCPS_LISTEN) {
					if (!V_tcp_fastopen_server_enable) {
						error = EPERM;
						goto unlock_and_done;
					}

					if (tp->t_tfo_pending == NULL)
						tp->t_tfo_pending =
						    tcp_fastopen_alloc_counter();
				} else {
					/*
					 * If a pre-shared key was provided,
					 * stash it in the client cookie
					 * field of the tcpcb for use during
					 * connect.
					 */
					if (sopt->sopt_valsize ==
					    sizeof(tfo_optval)) {
						memcpy(tp->t_tfo_cookie.client,
						       tfo_optval.psk,
						       TCP_FASTOPEN_PSK_LEN);
						tp->t_tfo_client_cookie_len =
						    TCP_FASTOPEN_PSK_LEN;
					}
				}
				tp->t_flags |= TF_FASTOPEN;
			} else
				tp->t_flags &= ~TF_FASTOPEN;
			goto unlock_and_done;
		}

#ifdef TCP_BLACKBOX
		case TCP_LOG:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, &optval, sizeof optval,
			    sizeof optval);
			if (error)
				return (error);

			INP_WLOCK_RECHECK(inp);
			error = tcp_log_state_change(tp, optval);
			goto unlock_and_done;

		case TCP_LOGBUF:
			INP_WUNLOCK(inp);
			error = EINVAL;
			break;

		case TCP_LOGID:
			INP_WUNLOCK(inp);
			error = sooptcopyin(sopt, buf, TCP_LOG_ID_LEN - 1, 0);
			if (error)
				break;
			buf[sopt->sopt_valsize] = '\0';
			INP_WLOCK_RECHECK(inp);
			error = tcp_log_set_id(tp, buf);
			/* tcp_log_set_id() unlocks the INP. */
			break;

		case TCP_LOGDUMP:
		case TCP_LOGDUMPID:
			INP_WUNLOCK(inp);
			error =
			    sooptcopyin(sopt, buf, TCP_LOG_REASON_LEN - 1, 0);
			if (error)
				break;
			buf[sopt->sopt_valsize] = '\0';
			INP_WLOCK_RECHECK(inp);
			if (sopt->sopt_name == TCP_LOGDUMP) {
				error = tcp_log_dump_tp_logbuf(tp, buf,
				    M_WAITOK, true);
				INP_WUNLOCK(inp);
			} else {
				tcp_log_dump_tp_bucket_logbufs(tp, buf);
				/*
				 * tcp_log_dump_tp_bucket_logbufs() drops the
				 * INP lock.
				 */
			}
			break;
#endif

		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		tp = intotcpcb(inp);
		switch (sopt->sopt_name) {
#if defined(IPSEC_SUPPORT) || defined(TCP_SIGNATURE)
		case TCP_MD5SIG:
			INP_WUNLOCK(inp);
			if (!TCPMD5_ENABLED())
				return (ENOPROTOOPT);
			error = TCPMD5_PCBCTL(inp, sopt);
			break;
#endif

		case TCP_NODELAY:
			optval = tp->t_flags & TF_NODELAY;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_MAXSEG:
			optval = tp->t_maxseg;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_REMOTE_UDP_ENCAPS_PORT:
			optval = ntohs(tp->t_port);
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOOPT:
			optval = tp->t_flags & TF_NOOPT;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_NOPUSH:
			optval = tp->t_flags & TF_NOPUSH;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
		case TCP_INFO:
			tcp_fill_info(tp, &ti);
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &ti, sizeof ti);
			break;
		case TCP_STATS:
			{
#ifdef STATS
			int nheld;
			TYPEOF_MEMBER(struct statsblob, flags) sbflags = 0;

			error = 0;
			socklen_t outsbsz = sopt->sopt_valsize;
			if (tp->t_stats == NULL)
				error = ENOENT;
			else if (outsbsz >= tp->t_stats->cursz)
				outsbsz = tp->t_stats->cursz;
			else if (outsbsz >= sizeof(struct statsblob))
				outsbsz = sizeof(struct statsblob);
			else
				error = EINVAL;
			INP_WUNLOCK(inp);
			if (error)
				break;

			sbp = sopt->sopt_val;
			nheld = atop(round_page(((vm_offset_t)sbp) +
			    (vm_size_t)outsbsz) - trunc_page((vm_offset_t)sbp));
			vm_page_t ma[nheld];
			if (vm_fault_quick_hold_pages(
			    &curproc->p_vmspace->vm_map, (vm_offset_t)sbp,
			    outsbsz, VM_PROT_READ | VM_PROT_WRITE, ma,
			    nheld) < 0) {
				error = EFAULT;
				break;
			}

			if ((error = copyin_nofault(&(sbp->flags), &sbflags,
			    SIZEOF_MEMBER(struct statsblob, flags))))
				goto unhold;

			INP_WLOCK_RECHECK(inp);
			error = stats_blob_snapshot(&sbp, outsbsz, tp->t_stats,
			    sbflags | SB_CLONE_USRDSTNOFAULT);
			INP_WUNLOCK(inp);
			sopt->sopt_valsize = outsbsz;
unhold:
			vm_page_unhold_pages(ma, nheld);
#else
			INP_WUNLOCK(inp);
			error = EOPNOTSUPP;
#endif /* !STATS */
			break;
			}
		case TCP_CONGESTION:
			len = strlcpy(buf, CC_ALGO(tp)->name, TCP_CA_NAME_MAX);
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, buf, len + 1);
			break;
		case TCP_MAXUNACKTIME:
		case TCP_KEEPIDLE:
		case TCP_KEEPINTVL:
		case TCP_KEEPINIT:
		case TCP_KEEPCNT:
			switch (sopt->sopt_name) {
			case TCP_MAXUNACKTIME:
				ui = TP_MAXUNACKTIME(tp) / hz;
				break;
			case TCP_KEEPIDLE:
				ui = TP_KEEPIDLE(tp) / hz;
				break;
			case TCP_KEEPINTVL:
				ui = TP_KEEPINTVL(tp) / hz;
				break;
			case TCP_KEEPINIT:
				ui = TP_KEEPINIT(tp) / hz;
				break;
			case TCP_KEEPCNT:
				ui = TP_KEEPCNT(tp);
				break;
			}
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &ui, sizeof(ui));
			break;
		case TCP_FASTOPEN:
			optval = tp->t_flags & TF_FASTOPEN;
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof optval);
			break;
#ifdef TCP_BLACKBOX
		case TCP_LOG:
			optval = tcp_get_bblog_state(tp);
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, &optval, sizeof(optval));
			break;
		case TCP_LOGBUF:
			/* tcp_log_getlogbuf() does INP_WUNLOCK(inp) */
			error = tcp_log_getlogbuf(sopt, tp);
			break;
		case TCP_LOGID:
			len = tcp_log_get_id(tp, buf);
			INP_WUNLOCK(inp);
			error = sooptcopyout(sopt, buf, len + 1);
			break;
		case TCP_LOGDUMP:
		case TCP_LOGDUMPID:
			INP_WUNLOCK(inp);
			error = EINVAL;
			break;
#endif
#ifdef KERN_TLS
		case TCP_TXTLS_MODE:
			error = ktls_get_tx_mode(so, &optval);
			INP_WUNLOCK(inp);
			if (error == 0)
				error = sooptcopyout(sopt, &optval,
				    sizeof(optval));
			break;
		case TCP_RXTLS_MODE:
			error = ktls_get_rx_mode(so, &optval);
			INP_WUNLOCK(inp);
			if (error == 0)
				error = sooptcopyout(sopt, &optval,
				    sizeof(optval));
			break;
#endif
		default:
			INP_WUNLOCK(inp);
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	return (error);
}
#undef INP_WLOCK_RECHECK
#undef INP_WLOCK_RECHECK_CLEANUP

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
static void
tcp_disconnect(struct tcpcb *tp)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct socket *so = tptosocket(tp);

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(inp);

	/*
	 * Neither tcp_close() nor tcp_drop() should return NULL, as the
	 * socket is still open.
	 */
	if (tp->t_state < TCPS_ESTABLISHED &&
	    !(tp->t_state > TCPS_LISTEN && (tp->t_flags & TF_FASTOPEN))) {
		tp = tcp_close(tp);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_close() returned NULL"));
	} else if ((so->so_options & SO_LINGER) && so->so_linger == 0) {
		tp = tcp_drop(tp, 0);
		KASSERT(tp != NULL,
		    ("tcp_disconnect: tcp_drop() returned NULL"));
	} else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tcp_usrclosed(tp);
		if (!(inp->inp_flags & INP_DROPPED))
			/* Ignore stack's drop request, we already at it. */
			(void)tcp_output_nodrop(tp);
	}
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
static void
tcp_usrclosed(struct tcpcb *tp)
{

	NET_EPOCH_ASSERT();
	INP_WLOCK_ASSERT(tptoinpcb(tp));

	switch (tp->t_state) {
	case TCPS_LISTEN:
#ifdef TCP_OFFLOAD
		tcp_offload_listen_stop(tp);
#endif
		tcp_state_change(tp, TCPS_CLOSED);
		/* FALLTHROUGH */
	case TCPS_CLOSED:
		tp = tcp_close(tp);
		/*
		 * tcp_close() should never return NULL here as the socket is
		 * still open.
		 */
		KASSERT(tp != NULL,
		    ("tcp_usrclosed: tcp_close() returned NULL"));
		break;

	case TCPS_SYN_SENT:
	case TCPS_SYN_RECEIVED:
		tp->t_flags |= TF_NEEDFIN;
		break;

	case TCPS_ESTABLISHED:
		tcp_state_change(tp, TCPS_FIN_WAIT_1);
		break;

	case TCPS_CLOSE_WAIT:
		tcp_state_change(tp, TCPS_LAST_ACK);
		break;
	}
	if (tp->t_acktime == 0)
		tp->t_acktime = ticks;
	if (tp->t_state >= TCPS_FIN_WAIT_2) {
		tcp_free_sackholes(tp);
		soisdisconnected(tptosocket(tp));
		/* Prevent the connection hanging in FIN_WAIT_2 forever. */
		if (tp->t_state == TCPS_FIN_WAIT_2) {
			int timeout;

			timeout = (tcp_fast_finwait2_recycle) ?
			    tcp_finwait2_timeout : TP_MAXIDLE(tp);
			tcp_timer_activate(tp, TT_2MSL, timeout);
		}
	}
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
db_print_tstate(int t_state)
{

	switch (t_state) {
	case TCPS_CLOSED:
		db_printf("TCPS_CLOSED");
		return;

	case TCPS_LISTEN:
		db_printf("TCPS_LISTEN");
		return;

	case TCPS_SYN_SENT:
		db_printf("TCPS_SYN_SENT");
		return;

	case TCPS_SYN_RECEIVED:
		db_printf("TCPS_SYN_RECEIVED");
		return;

	case TCPS_ESTABLISHED:
		db_printf("TCPS_ESTABLISHED");
		return;

	case TCPS_CLOSE_WAIT:
		db_printf("TCPS_CLOSE_WAIT");
		return;

	case TCPS_FIN_WAIT_1:
		db_printf("TCPS_FIN_WAIT_1");
		return;

	case TCPS_CLOSING:
		db_printf("TCPS_CLOSING");
		return;

	case TCPS_LAST_ACK:
		db_printf("TCPS_LAST_ACK");
		return;

	case TCPS_FIN_WAIT_2:
		db_printf("TCPS_FIN_WAIT_2");
		return;

	case TCPS_TIME_WAIT:
		db_printf("TCPS_TIME_WAIT");
		return;

	default:
		db_printf("unknown");
		return;
	}
}

static void
db_print_tflags(u_int t_flags)
{
	int comma;

	comma = 0;
	if (t_flags & TF_ACKNOW) {
		db_printf("%sTF_ACKNOW", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_DELACK) {
		db_printf("%sTF_DELACK", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NODELAY) {
		db_printf("%sTF_NODELAY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NOOPT) {
		db_printf("%sTF_NOOPT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SENTFIN) {
		db_printf("%sTF_SENTFIN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_REQ_SCALE) {
		db_printf("%sTF_REQ_SCALE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RCVD_SCALE) {
		db_printf("%sTF_RECVD_SCALE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_REQ_TSTMP) {
		db_printf("%sTF_REQ_TSTMP", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RCVD_TSTMP) {
		db_printf("%sTF_RCVD_TSTMP", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SACK_PERMIT) {
		db_printf("%sTF_SACK_PERMIT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NEEDSYN) {
		db_printf("%sTF_NEEDSYN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NEEDFIN) {
		db_printf("%sTF_NEEDFIN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_NOPUSH) {
		db_printf("%sTF_NOPUSH", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_PREVVALID) {
		db_printf("%sTF_PREVVALID", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_WAKESOR) {
		db_printf("%sTF_WAKESOR", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_GPUTINPROG) {
		db_printf("%sTF_GPUTINPROG", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_MORETOCOME) {
		db_printf("%sTF_MORETOCOME", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SONOTCONN) {
		db_printf("%sTF_SONOTCONN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_LASTIDLE) {
		db_printf("%sTF_LASTIDLE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_RXWIN0SENT) {
		db_printf("%sTF_RXWIN0SENT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_FASTRECOVERY) {
		db_printf("%sTF_FASTRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_WASFRECOVERY) {
		db_printf("%sTF_WASFRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SIGNATURE) {
		db_printf("%sTF_SIGNATURE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_FORCEDATA) {
		db_printf("%sTF_FORCEDATA", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_TSO) {
		db_printf("%sTF_TSO", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_TOE) {
		db_printf("%sTF_TOE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_CLOSED) {
		db_printf("%sTF_CLOSED", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_SENTSYN) {
		db_printf("%sTF_SENTSYN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_LRD) {
		db_printf("%sTF_LRD", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_CONGRECOVERY) {
		db_printf("%sTF_CONGRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_WASCRECOVERY) {
		db_printf("%sTF_WASCRECOVERY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags & TF_FASTOPEN) {
		db_printf("%sTF_FASTOPEN", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_tflags2(u_int t_flags2)
{
	int comma;

	comma = 0;
	if (t_flags2 & TF2_PLPMTU_BLACKHOLE) {
		db_printf("%sTF2_PLPMTU_BLACKHOLE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_PLPMTU_PMTUD) {
		db_printf("%sTF2_PLPMTU_PMTUD", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_PLPMTU_MAXSEGSNT) {
		db_printf("%sTF2_PLPMTU_MAXSEGSNT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_LOG_AUTO) {
		db_printf("%sTF2_LOG_AUTO", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_DROP_AF_DATA) {
		db_printf("%sTF2_DROP_AF_DATA", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_ECN_PERMIT) {
		db_printf("%sTF2_ECN_PERMIT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_ECN_SND_CWR) {
		db_printf("%sTF2_ECN_SND_CWR", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_ECN_SND_ECE) {
		db_printf("%sTF2_ECN_SND_ECE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_ACE_PERMIT) {
		db_printf("%sTF2_ACE_PERMIT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_HPTS_CPU_SET) {
		db_printf("%sTF2_HPTS_CPU_SET", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_FBYTES_COMPLETE) {
		db_printf("%sTF2_FBYTES_COMPLETE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_ECN_USE_ECT1) {
		db_printf("%sTF2_ECN_USE_ECT1", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_TCP_ACCOUNTING) {
		db_printf("%sTF2_TCP_ACCOUNTING", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_HPTS_CALLS) {
		db_printf("%sTF2_HPTS_CALLS", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_MBUF_L_ACKS) {
		db_printf("%sTF2_MBUF_L_ACKS", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_MBUF_ACKCMP) {
		db_printf("%sTF2_MBUF_ACKCMP", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_SUPPORTS_MBUFQ) {
		db_printf("%sTF2_SUPPORTS_MBUFQ", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_MBUF_QUEUE_READY) {
		db_printf("%sTF2_MBUF_QUEUE_READY", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_DONT_SACK_QUEUE) {
		db_printf("%sTF2_DONT_SACK_QUEUE", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_CANNOT_DO_ECN) {
		db_printf("%sTF2_CANNOT_DO_ECN", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_PROC_SACK_PROHIBIT) {
		db_printf("%sTF2_PROC_SACK_PROHIBIT", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_IPSEC_TSO) {
		db_printf("%sTF2_IPSEC_TSO", comma ? ", " : "");
		comma = 1;
	}
	if (t_flags2 & TF2_NO_ISS_CHECK) {
		db_printf("%sTF2_NO_ISS_CHECK", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_toobflags(char t_oobflags)
{
	int comma;

	comma = 0;
	if (t_oobflags & TCPOOB_HAVEDATA) {
		db_printf("%sTCPOOB_HAVEDATA", comma ? ", " : "");
		comma = 1;
	}
	if (t_oobflags & TCPOOB_HADDATA) {
		db_printf("%sTCPOOB_HADDATA", comma ? ", " : "");
		comma = 1;
	}
}

static void
db_print_tcpcb(struct tcpcb *tp, const char *name, int indent)
{

	db_print_indent(indent);
	db_printf("%s at %p\n", name, tp);

	indent += 2;

	db_print_indent(indent);
	db_printf("t_segq first: %p   t_segqlen: %d   t_dupacks: %d\n",
	   TAILQ_FIRST(&tp->t_segq), tp->t_segqlen, tp->t_dupacks);

	db_print_indent(indent);
	db_printf("t_callout: %p   t_timers: %p\n",
	    &tp->t_callout, &tp->t_timers);

	db_print_indent(indent);
	db_printf("t_state: %d (", tp->t_state);
	db_print_tstate(tp->t_state);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("t_flags: 0x%x (", tp->t_flags);
	db_print_tflags(tp->t_flags);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("t_flags2: 0x%x (", tp->t_flags2);
	db_print_tflags2(tp->t_flags2);
	db_printf(")\n");

	db_print_indent(indent);
	db_printf("snd_una: 0x%08x   snd_max: 0x%08x   snd_nxt: 0x%08x\n",
	    tp->snd_una, tp->snd_max, tp->snd_nxt);

	db_print_indent(indent);
	db_printf("snd_up: 0x%08x   snd_wl1: 0x%08x   snd_wl2: 0x%08x\n",
	   tp->snd_up, tp->snd_wl1, tp->snd_wl2);

	db_print_indent(indent);
	db_printf("iss: 0x%08x   irs: 0x%08x   rcv_nxt: 0x%08x\n",
	    tp->iss, tp->irs, tp->rcv_nxt);

	db_print_indent(indent);
	db_printf("rcv_adv: 0x%08x   rcv_wnd: %u   rcv_up: 0x%08x\n",
	    tp->rcv_adv, tp->rcv_wnd, tp->rcv_up);

	db_print_indent(indent);
	db_printf("snd_wnd: %u   snd_cwnd: %u\n",
	   tp->snd_wnd, tp->snd_cwnd);

	db_print_indent(indent);
	db_printf("snd_ssthresh: %u   snd_recover: "
	    "0x%08x\n", tp->snd_ssthresh, tp->snd_recover);

	db_print_indent(indent);
	db_printf("t_rcvtime: %u   t_startime: %u\n",
	    tp->t_rcvtime, tp->t_starttime);

	db_print_indent(indent);
	db_printf("t_rttime: %u   t_rtsq: 0x%08x\n",
	    tp->t_rtttime, tp->t_rtseq);

	db_print_indent(indent);
	db_printf("t_rxtcur: %d   t_maxseg: %u   t_srtt: %d\n",
	    tp->t_rxtcur, tp->t_maxseg, tp->t_srtt);

	db_print_indent(indent);
	db_printf("t_rttvar: %d   t_rxtshift: %d   t_rttmin: %u\n",
	    tp->t_rttvar, tp->t_rxtshift, tp->t_rttmin);

	db_print_indent(indent);
	db_printf("t_rttupdated: %u   max_sndwnd: %u   t_softerror: %d\n",
	    tp->t_rttupdated, tp->max_sndwnd, tp->t_softerror);

	db_print_indent(indent);
	db_printf("t_oobflags: 0x%x (", tp->t_oobflags);
	db_print_toobflags(tp->t_oobflags);
	db_printf(")   t_iobc: 0x%02x\n", tp->t_iobc);

	db_print_indent(indent);
	db_printf("snd_scale: %u   rcv_scale: %u   request_r_scale: %u\n",
	    tp->snd_scale, tp->rcv_scale, tp->request_r_scale);

	db_print_indent(indent);
	db_printf("ts_recent: %u   ts_recent_age: %u\n",
	    tp->ts_recent, tp->ts_recent_age);

	db_print_indent(indent);
	db_printf("ts_offset: %u   last_ack_sent: 0x%08x   snd_cwnd_prev: "
	    "%u\n", tp->ts_offset, tp->last_ack_sent, tp->snd_cwnd_prev);

	db_print_indent(indent);
	db_printf("snd_ssthresh_prev: %u   snd_recover_prev: 0x%08x   "
	    "t_badrxtwin: %u\n", tp->snd_ssthresh_prev,
	    tp->snd_recover_prev, tp->t_badrxtwin);

	db_print_indent(indent);
	db_printf("snd_numholes: %d  snd_holes first: %p\n",
	    tp->snd_numholes, TAILQ_FIRST(&tp->snd_holes));

	db_print_indent(indent);
	db_printf("snd_fack: 0x%08x   rcv_numsacks: %d\n",
	    tp->snd_fack, tp->rcv_numsacks);

	/* Skip sackblks, sackhint. */

	db_print_indent(indent);
	db_printf("t_rttlow: %d   rfbuf_ts: %u   rfbuf_cnt: %d\n",
	    tp->t_rttlow, tp->rfbuf_ts, tp->rfbuf_cnt);
}

DB_SHOW_COMMAND(tcpcb, db_show_tcpcb)
{
	struct tcpcb *tp;

	if (!have_addr) {
		db_printf("usage: show tcpcb <addr>\n");
		return;
	}
	tp = (struct tcpcb *)addr;

	db_print_tcpcb(tp, "tcpcb", 0);
}
#endif
