/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * grab bag of accessor routines that will either be moved to netinet
 * or removed
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_offload.h>
#include <netinet/tcp_syncache.h>
#include <netinet/toedev.h>

#include <dev/cxgb/ulp/tom/cxgb_tcp_offload.h>


/*
 * This file contains code as a short-term staging area before it is moved in 
 * to sys/netinet/tcp_offload.c
 */

void
tcp_offload_twstart(struct tcpcb *tp)
{

	INP_INFO_WLOCK(&tcbinfo);
	inp_wlock(tp->t_inpcb);
	tcp_twstart(tp);
	INP_INFO_WUNLOCK(&tcbinfo);
}

void
tcp_offload_twstart_disconnect(struct tcpcb *tp)
{
	struct socket *so;
	
	INP_INFO_WLOCK(&tcbinfo);
	inp_wlock(tp->t_inpcb);
	so = tp->t_inpcb->inp_socket;	
	tcp_twstart(tp);
	if (so)
		soisdisconnected(so);	
	INP_INFO_WUNLOCK(&tcbinfo);
}

struct tcpcb *
tcp_offload_close(struct tcpcb *tp)
{
	
	INP_INFO_WLOCK(&tcbinfo);
	INP_WLOCK(tp->t_inpcb);
	tp = tcp_close(tp);
	INP_INFO_WUNLOCK(&tcbinfo);
	if (tp)
		INP_WUNLOCK(tp->t_inpcb);

	return (tp);
}

struct tcpcb *
tcp_offload_drop(struct tcpcb *tp, int error)
{
	
	INP_INFO_WLOCK(&tcbinfo);
	INP_WLOCK(tp->t_inpcb);
	tp = tcp_drop(tp, error);
	INP_INFO_WUNLOCK(&tcbinfo);
	if (tp)
		INP_WUNLOCK(tp->t_inpcb);

	return (tp);
}

void
inp_apply_all(void (*func)(struct inpcb *, void *), void *arg)
{
	struct inpcb *inp;
	
	INP_INFO_RLOCK(&tcbinfo);
	LIST_FOREACH(inp, tcbinfo.ipi_listhead, inp_list) {
		INP_WLOCK(inp);
		func(inp, arg);		
		INP_WUNLOCK(inp);		
	}
	INP_INFO_RUNLOCK(&tcbinfo);
}

struct socket *
inp_inpcbtosocket(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);	
	return (inp->inp_socket);	
}

struct tcpcb *
inp_inpcbtotcpcb(struct inpcb *inp)
{

	INP_WLOCK_ASSERT(inp);	
	return ((struct tcpcb *)inp->inp_ppcb);
}

int
inp_ip_tos_get(const struct inpcb *inp)
{

	return (inp->inp_ip_tos);
}

void
inp_ip_tos_set(struct inpcb *inp, int val)
{

	inp->inp_ip_tos = val;
}

void 
inp_4tuple_get(const struct inpcb *inp, uint32_t *laddr, uint16_t *lp, uint32_t *faddr, uint16_t *fp)
{

	memcpy(laddr, &inp->inp_laddr, 4);
	memcpy(faddr, &inp->inp_faddr, 4);

	*lp = inp->inp_lport;
	*fp = inp->inp_fport;
}

void
so_listeners_apply_all(struct socket *so, void (*func)(struct socket *, void *), void *arg)
{
	
	TAILQ_FOREACH(so, &so->so_comp, so_list)
		func(so, arg);
}

struct tcpcb *
so_sototcpcb(struct socket *so)
{

	return (sototcpcb(so));
}

struct inpcb *
so_sotoinpcb(struct socket *so)
{

	return (sotoinpcb(so));
}

struct sockbuf *
so_sockbuf_rcv(struct socket *so)
{

	return (&so->so_rcv);
}

struct sockbuf *
so_sockbuf_snd(struct socket *so)
{

	return (&so->so_snd);
}

int
so_state_get(const struct socket *so)
{

	return (so->so_state);
}

void
so_state_set(struct socket *so, int val)
{

	so->so_state = val;
}

int
so_options_get(const struct socket *so)
{

	return (so->so_options);
}

void
so_options_set(struct socket *so, int val)
{

	so->so_options = val;
}

int
so_error_get(const struct socket *so)
{

	return (so->so_error);
}

void
so_error_set(struct socket *so, int val)
{

	so->so_error = val;
}

int
so_linger_get(const struct socket *so)
{

	return (so->so_linger);
}

void
so_linger_set(struct socket *so, int val)
{

	so->so_linger = val;
}

struct protosw *
so_protosw_get(const struct socket *so)
{

	return (so->so_proto);
}

void
so_protosw_set(struct socket *so, struct protosw *val)
{

	so->so_proto = val;
}

void
so_sorwakeup(struct socket *so)
{

	sorwakeup(so);
}

void
so_sowwakeup(struct socket *so)
{

	sowwakeup(so);
}

void
so_sorwakeup_locked(struct socket *so)
{

	sorwakeup_locked(so);
}

void
so_sowwakeup_locked(struct socket *so)
{

	sowwakeup_locked(so);
}

void
so_lock(struct socket *so)
{
	SOCK_LOCK(so);
}

void
so_unlock(struct socket *so)
{
	SOCK_UNLOCK(so);
}

void
sockbuf_lock(struct sockbuf *sb)
{

	SOCKBUF_LOCK(sb);
}

void
sockbuf_lock_assert(struct sockbuf *sb)
{

	SOCKBUF_LOCK_ASSERT(sb);
}

void
sockbuf_unlock(struct sockbuf *sb)
{

	SOCKBUF_UNLOCK(sb);
}

int
sockbuf_sbspace(struct sockbuf *sb)
{

	return (sbspace(sb));
}

int
syncache_offload_expand(struct in_conninfo *inc, struct tcpopt *to, struct tcphdr *th,
    struct socket **lsop, struct mbuf *m)
{
	int rc;
	
	INP_INFO_WLOCK(&tcbinfo);
	rc = syncache_expand(inc, to, th, lsop, m);
	INP_INFO_WUNLOCK(&tcbinfo);

	return (rc);
}
