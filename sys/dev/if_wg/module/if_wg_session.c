/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2019-2020 Matt Dunwoodie <ncon@noconroy.net>
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */



#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <vm/uma.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/sockio.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/protosw.h>
#include <sys/endian.h>
#include <sys/kdb.h>
#include <sys/sysctl.h>

#include <net/bpf.h>


#include <sys/support.h>
#include <sys/if_wg_session.h>
#include <sys/if_wg_session_vars.h>
#include <sys/syslog.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet/udp_var.h>

#include <crypto/blake2s.h>
#include <crypto/curve25519.h>
#include <machine/in_cksum.h>

#define MAX_STAGED_PKT		128
#define MAX_QUEUED_PKT		512

#define	GROUPTASK_DRAIN(gtask)			\
	gtaskqueue_drain((gtask)->gt_taskqueue, &(gtask)->gt_task)
TASKQGROUP_DECLARE(if_io_tqg);

struct wg_pkt_initiation {
	uint32_t		t;
	struct noise_initiation init;
	struct cookie_macs	m;
} __packed;

struct wg_pkt_response {
	uint32_t		t;
	struct noise_response	resp;
	struct cookie_macs	m;
} __packed;

struct wg_pkt_cookie {
	uint32_t		t;
	uint32_t		r_idx;
	uint8_t			nonce[COOKIE_XNONCE_SIZE];
	uint8_t			ec[COOKIE_ENCRYPTED_SIZE];
} __packed;

struct wg_pkt_data {
	uint32_t		t;
	struct noise_data	data;
} __packed;

#define MTAG_WIREGUARD 0xBEAD
#define WG_PKT_WITH_PADDING(n)	(((n) + (16-1)) & (~(16-1)))

SYSCTL_NODE(_net, OID_AUTO, wg, CTLFLAG_RW, 0, "Wireguard");
static int wireguard_debug;
SYSCTL_INT(_net_wg, OID_AUTO, debug, CTLFLAG_RWTUN, &wireguard_debug, 0,
	"enable debug logging");


#define DPRINTF(sc,  ...) if (wireguard_debug) if_printf(sc->sc_ifp, ##__VA_ARGS__)

/* Socket */
static int	wg_socket_bind(struct wg_softc *sc, struct wg_socket *);
static int	wg_send(struct wg_softc *, struct wg_endpoint *, struct mbuf *);

/* Timers */
static int	wg_timers_expired_handshake_last_sent(struct wg_timers *);


static void	wg_timers_event_data_sent(struct wg_timers *);
static void	wg_timers_event_data_received(struct wg_timers *);
static void	wg_timers_event_any_authenticated_packet_sent(struct wg_timers *);
static void	wg_timers_event_any_authenticated_packet_received(struct wg_timers *);
static void	wg_timers_event_handshake_initiated(struct wg_timers *);
static void	wg_timers_event_handshake_responded(struct wg_timers *);
static void	wg_timers_event_handshake_complete(struct wg_timers *);
static void	wg_timers_event_session_derived(struct wg_timers *);
static void	wg_timers_event_any_authenticated_packet_traversal(struct wg_timers *);
static void	wg_timers_event_want_initiation(struct wg_timers *);

static void	wg_timers_run_send_initiation(struct wg_timers *, int);
static void	wg_timers_run_retry_handshake(struct wg_timers *);
static void	wg_timers_run_send_keepalive(struct wg_timers *);
static void	wg_timers_run_new_handshake(struct wg_timers *);
static void	wg_timers_run_zero_key_material(struct wg_timers *);
static void	wg_timers_run_persistent_keepalive(struct wg_timers *);

static void	wg_peer_timers_init(struct wg_peer *);
static void	wg_timers_disable(struct wg_timers *);

/* Queue */
static int	wg_queue_in(struct wg_peer *, struct mbuf *);
static struct mbuf *wg_queue_dequeue(struct wg_queue *, struct wg_tag **);

/* Cookie */

static int wg_cookie_validate_packet(struct cookie_checker *, struct mbuf *,
    int);

/* Peer */
static void	wg_send_initiation(struct wg_peer *);
static void	wg_send_cookie(struct wg_softc *, struct cookie_macs *, uint32_t, struct mbuf *);

static void	wg_peer_set_endpoint_from_tag(struct wg_peer *, struct wg_tag *);
static void	wg_peer_clear_src(struct wg_peer *);
static void	wg_peer_get_endpoint(struct wg_peer *, struct wg_endpoint *);

static void	wg_deliver_out(struct wg_peer *);
static void	wg_deliver_in(struct wg_peer *);
static void	wg_send_buf(struct wg_softc *, struct wg_endpoint *, uint8_t *, size_t);


static void	wg_send_keepalive(struct wg_peer *);

/* Packet */
static struct wg_endpoint *wg_mbuf_endpoint_get(struct mbuf *);

static void	wg_handshake(struct wg_softc *, struct mbuf *);
static void	wg_encap(struct wg_softc *, struct mbuf *);
static void	wg_decap(struct wg_softc *, struct mbuf *);

/* Interface */
static void wg_input(struct mbuf *m, int offset, struct inpcb *inpcb,
    const struct sockaddr *srcsa, void *_sc);

/* Globals */

#define UNDERLOAD_TIMEOUT	1

static volatile unsigned long peer_counter = 0;
static struct timeval	underload_interval = { UNDERLOAD_TIMEOUT, 0 };

#define M_ENQUEUED	M_PROTO1

static void
wg_m_freem(struct mbuf *m)
{
	MPASS((m->m_flags & M_ENQUEUED) == 0);
	m_freem(m);
}

static void
m_calchdrlen(struct mbuf *m)
{
	struct mbuf *n;
	int plen = 0;

	MPASS(m->m_flags & M_PKTHDR);
	for (n = m; n; n = n->m_next)
		plen += n->m_len;
	m->m_pkthdr.len = plen;
}

static inline int
callout_del(struct callout *c)
{
	return (callout_stop(c) > 0);
}

struct wg_tag *
wg_tag_get(struct mbuf *m)
{
	struct m_tag *tag;

	tag = m_tag_find(m, MTAG_WIREGUARD, NULL);
	if (tag == NULL) {
		tag = m_tag_get(MTAG_WIREGUARD, sizeof(struct wg_tag), M_NOWAIT|M_ZERO);
		m_tag_prepend(m, tag);
		MPASS(!SLIST_EMPTY(&m->m_pkthdr.tags));
		MPASS(m_tag_locate(m, MTAG_ABI_COMPAT, MTAG_WIREGUARD, NULL) == tag);
	}
	return (struct wg_tag *)tag;
}

static struct wg_endpoint *
wg_mbuf_endpoint_get(struct mbuf *m)
{
	struct wg_tag *hdr;

	if ((hdr = wg_tag_get(m)) == NULL)
		return (NULL);

	return (&hdr->t_endpoint);
}

/* Socket */

static int
wg_socket_reuse(struct wg_softc *sc, struct socket *so)
{
	struct sockopt sopt;
	int error, val = 1;
	struct ifnet *ifp;

	bzero(&sopt, sizeof(sopt));
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_REUSEPORT;
	sopt.sopt_val = &val;
	sopt.sopt_valsize = sizeof(val);
	error = sosetopt(so, &sopt);
	if (error) {
		ifp = iflib_get_ifp(sc->wg_ctx);
		if_printf(ifp,
				  "cannot set REUSEPORT socket opt: %d\n", error);
	}
	sopt.sopt_name = SO_REUSEADDR;
	error = sosetopt(so, &sopt);
	if (error) {
		ifp = iflib_get_ifp(sc->wg_ctx);
		if_printf(ifp,
				  "cannot set REUSEADDDR socket opt: %d\n", error);
	}
	return (error);
}

int
wg_socket_init(struct wg_softc *sc)
{
	struct thread *td;
	struct wg_socket *so;
	struct ifnet *ifp;
	int rc;

	so = &sc->sc_socket;
	td = curthread;
	ifp = iflib_get_ifp(sc->wg_ctx);
	rc = socreate(AF_INET, &so->so_so4, SOCK_DGRAM, IPPROTO_UDP, td->td_ucred, td);
	if (rc) {
		if_printf(ifp, "can't create AF_INET socket\n");
		return (rc);
	}
	rc = wg_socket_reuse(sc, so->so_so4);
	if (rc)
		goto fail;
	rc = udp_set_kernel_tunneling(so->so_so4, wg_input, NULL, sc);
	if_printf(ifp, "sc=%p\n", sc);
	/*
	 * udp_set_kernel_tunneling can only fail if there is already a tunneling function set.
	 * This should never happen with a new socket.
	 */
	MPASS(rc == 0);
	
	rc = socreate(AF_INET6, &so->so_so6, SOCK_DGRAM, IPPROTO_UDP, td->td_ucred, td);
	if (rc) {
		if_printf(ifp, "can't create AF_INET6 socket\n");

		goto fail;
	}
	rc = wg_socket_reuse(sc, so->so_so6);
	if (rc) {
		SOCK_LOCK(so->so_so6);
		sofree(so->so_so6);
		goto fail;
	}
	rc = udp_set_kernel_tunneling(so->so_so6, wg_input, NULL, sc);
	MPASS(rc == 0);

	rc = wg_socket_bind(sc, so);
	return (rc);
fail:
	SOCK_LOCK(so->so_so4);
	sofree(so->so_so4);
	return (rc);
}

void
wg_socket_reinit(struct wg_softc *sc, struct socket *new4,
    struct socket *new6)
{
	struct wg_socket *so;

	so = &sc->sc_socket;

	if (so->so_so4)
		soclose(so->so_so4);
	so->so_so4 = new4;
	if (so->so_so6)
		soclose(so->so_so6);
	so->so_so6 = new6;
}

int
wg_socket_close(struct wg_socket *so)
{
	int ret = 0;
	if ((ret = soclose(so->so_so4)) != 0)
		goto leave;
	if ((ret = soclose(so->so_so6)) != 0)
		goto leave;
leave:
	return ret;
}

union wg_sockaddr {
	struct sockaddr sa;
	struct sockaddr_in in4;
	struct sockaddr_in6 in6;
};

int
wg_socket_bind(struct wg_softc *sc, struct wg_socket *so)
{
	int rc;
	struct thread *td;
	union wg_sockaddr laddr;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct ifnet *ifp;

	if (so->so_port == 0)
		return (0);
	td = curthread;
	bzero(&laddr, sizeof(laddr));
	ifp = iflib_get_ifp(sc->wg_ctx);
	sin = &laddr.in4;
	sin->sin_len = sizeof(laddr.in4);
	sin->sin_family = AF_INET;
	sin->sin_port = htons(so->so_port);
	sin->sin_addr = (struct in_addr) { 0 };

	if ((rc = sobind(so->so_so4, &laddr.sa, td)) != 0) {
		if_printf(ifp, "can't bind AF_INET socket %d\n", rc);
		return (rc);
	}
	sin6 = &laddr.in6;
	sin6->sin6_len = sizeof(laddr.in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = htons(so->so_port);
	sin6->sin6_addr = (struct in6_addr) { .s6_addr = { 0 } };

	rc = sobind(so->so_so6, &laddr.sa, td);
	if (rc)
		if_printf(ifp, "can't bind AF_INET6 socket %d\n", rc);
	return (rc);
}

static int
wg_send(struct wg_softc *sc, struct wg_endpoint *e, struct mbuf *m)
{
	struct epoch_tracker et;
	struct sockaddr *sa;
	struct wg_socket *so = &sc->sc_socket;
	struct mbuf	 *control = NULL;
	int		 ret = 0;

	/* Get local control address before locking */
	if (e->e_remote.r_sa.sa_family == AF_INET) {
		if (e->e_local.l_in.s_addr != INADDR_ANY)
			control = sbcreatecontrol((caddr_t)&e->e_local.l_in,
			    sizeof(struct in_addr), IP_SENDSRCADDR,
			    IPPROTO_IP);
	} else if (e->e_remote.r_sa.sa_family == AF_INET6) {
		if (!IN6_IS_ADDR_UNSPECIFIED(&e->e_local.l_in6))
			control = sbcreatecontrol((caddr_t)&e->e_local.l_pktinfo6,
			    sizeof(struct in6_pktinfo), IPV6_PKTINFO,
			    IPPROTO_IPV6);
	} else {
		return (EAFNOSUPPORT);
	}

	/* Get remote address */
	sa = &e->e_remote.r_sa;

	NET_EPOCH_ENTER(et);
	if (sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		goto done;
	if (e->e_remote.r_sa.sa_family == AF_INET && so->so_so4 != NULL)
		ret = sosend(so->so_so4, sa, NULL, m, control, 0, curthread);
	else if (e->e_remote.r_sa.sa_family == AF_INET6 && so->so_so6 != NULL)
		ret = sosend(so->so_so6, sa, NULL, m, control, 0, curthread);
	else {
		ret = ENOTCONN;
		wg_m_freem(control);
		wg_m_freem(m);
	}
done:
	NET_EPOCH_EXIT(et);
	return (ret);
}

/* Timers */
/* Should be called after an authenticated data packet is sent. */
static void
wg_timers_event_data_sent(struct wg_timers *t)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);

	if (!t->t_disabled && !callout_pending(&t->t_new_handshake))
		callout_reset(&t->t_new_handshake,
		    NEW_HANDSHAKE_TIMEOUT * hz + (random() % REKEY_TIMEOUT_JITTER),
		    (timeout_t *)wg_timers_run_new_handshake, t);
	NET_EPOCH_EXIT(et);
}

/* Should be called after an authenticated data packet is received. */
static void
wg_timers_event_data_received(struct wg_timers *t)
{
	struct epoch_tracker et;

	if (t->t_disabled)
		return;
	NET_EPOCH_ENTER(et);
	if (!callout_pending(&t->t_send_keepalive)) {
		callout_reset(&t->t_send_keepalive, KEEPALIVE_TIMEOUT*hz,
		    (timeout_t *)wg_timers_run_send_keepalive, t);
	} else {
		t->t_need_another_keepalive = 1;
	}
	NET_EPOCH_EXIT(et);
}

/*
 * Should be called after any type of authenticated packet is sent, whether
 * keepalive, data, or handshake.
 */
static void
wg_timers_event_any_authenticated_packet_sent(struct wg_timers *t)
{
	callout_del(&t->t_send_keepalive);
}

/*
 * Should be called after any type of authenticated packet is received, whether
 * keepalive, data, or handshake.
 */
static void
wg_timers_event_any_authenticated_packet_received(struct wg_timers *t)
{
	callout_del(&t->t_new_handshake);
}

/*
 * Should be called before a packet with authentication, whether
 * keepalive, data, or handshake is sent, or after one is received.
 */
static void
wg_timers_event_any_authenticated_packet_traversal(struct wg_timers *t)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	if (!t->t_disabled && t->t_persistent_keepalive_interval > 0)
		callout_reset(&t->t_persistent_keepalive,
		     t->t_persistent_keepalive_interval *hz,
		    (timeout_t *)wg_timers_run_persistent_keepalive, t);
	NET_EPOCH_EXIT(et);
}

/* Should be called after a handshake initiation message is sent. */
static void
wg_timers_event_handshake_initiated(struct wg_timers *t)
{

	if (t->t_disabled)
		return;
	callout_reset(&t->t_retry_handshake,
	    REKEY_TIMEOUT * hz + random() % REKEY_TIMEOUT_JITTER,
	    (timeout_t *)wg_timers_run_retry_handshake, t);
}

static void
wg_timers_event_handshake_responded(struct wg_timers *t)
{
	getnanouptime(&t->t_handshake_last_sent);
}

/*
 * Should be called after a handshake response message is received and processed
 * or when getting key confirmation via the first data message.
 */
static void
wg_timers_event_handshake_complete(struct wg_timers *t)
{
	if (t->t_disabled)
		return;

	callout_del(&t->t_retry_handshake);
	t->t_handshake_retries = 0;
	getnanotime(&t->t_handshake_complete);
	wg_timers_run_send_keepalive(t);
}

/*
 * Should be called after an ephemeral key is created, which is before sending a
 * handshake response or after receiving a handshake response.
 */
static void
wg_timers_event_session_derived(struct wg_timers *t)
{
	if (t->t_disabled)
		return;

	callout_reset(&t->t_zero_key_material,
	    REJECT_AFTER_TIME * 3 * hz,
	    (timeout_t *)wg_timers_run_zero_key_material, t);
}

static void
wg_timers_event_want_initiation(struct wg_timers *t)
{
	if (t->t_disabled)
		return;

	wg_timers_run_send_initiation(t, 0);
}

static void
wg_grouptask_enqueue(struct wg_peer *peer, struct grouptask *task)
{
	if (peer->p_sc->sc_ifp->if_link_state == LINK_STATE_UP)
		GROUPTASK_ENQUEUE(task);
}

static void
wg_timers_run_send_initiation(struct wg_timers *t, int is_retry)
{
	struct wg_peer	 *peer = CONTAINER_OF(t, struct wg_peer, p_timers);

	if (!is_retry)
		t->t_handshake_retries = 0;
	if (wg_timers_expired_handshake_last_sent(t) == ETIMEDOUT)
		wg_grouptask_enqueue(peer, &peer->p_send_initiation);
}

static void
wg_timers_run_retry_handshake(struct wg_timers *t)
{
	struct wg_peer	*peer = CONTAINER_OF(t, struct wg_peer, p_timers);
	int		 retries;

	retries = atomic_fetchadd_int(&t->t_handshake_retries, 1);

	if (retries <= MAX_TIMER_HANDSHAKES) {
		DPRINTF(peer->p_sc, "Handshake for peer %llu did not complete "
		    "after %d seconds, retrying (try %d)\n",
			(unsigned long long)peer->p_id,
		    REKEY_TIMEOUT, t->t_handshake_retries + 1);
		wg_peer_clear_src(peer);
		wg_timers_run_send_initiation(t, 1);
	} else {
		DPRINTF(peer->p_sc, "Handshake for peer %llu did not complete "
		    "after %d retries, giving up\n",
			(unsigned long long) peer->p_id, MAX_TIMER_HANDSHAKES + 2);

		callout_del(&t->t_send_keepalive);
		if (!callout_pending(&t->t_zero_key_material))
			callout_reset(&t->t_zero_key_material, REJECT_AFTER_TIME * 3 * hz,
			    (timeout_t *)wg_timers_run_zero_key_material, t);
	}
}

static void
wg_timers_run_send_keepalive(struct wg_timers *t)
{
	struct wg_peer	*peer = CONTAINER_OF(t, struct wg_peer, p_timers);

	wg_grouptask_enqueue(peer, &peer->p_send_keepalive);
	if (t->t_need_another_keepalive) {
		t->t_need_another_keepalive = 0;
		callout_reset(&t->t_send_keepalive,
		    KEEPALIVE_TIMEOUT*hz,
		     (timeout_t *)wg_timers_run_send_keepalive, t);
	}
}

static void
wg_timers_run_new_handshake(struct wg_timers *t)
{
	struct wg_peer	*peer = CONTAINER_OF(t, struct wg_peer, p_timers);

	DPRINTF(peer->p_sc, "Retrying handshake with peer %llu because we "
	    "stopped hearing back after %d seconds\n",
		(unsigned long long)peer->p_id, NEW_HANDSHAKE_TIMEOUT);
	wg_peer_clear_src(peer);

	wg_timers_run_send_initiation(t, 0);
}

static void
wg_timers_run_zero_key_material(struct wg_timers *t)
{
	struct wg_peer *peer = CONTAINER_OF(t, struct wg_peer, p_timers);

	DPRINTF(peer->p_sc, "Zeroing out all keys for peer %llu, since we "
	    "haven't received a new one in %d seconds\n",
		(unsigned long long)peer->p_id, REJECT_AFTER_TIME * 3);
	GROUPTASK_ENQUEUE(&peer->p_clear_secrets);
}

static void
wg_timers_run_persistent_keepalive(struct wg_timers *t)
{
	struct wg_peer	 *peer = CONTAINER_OF(t, struct wg_peer, p_timers);

	if (t->t_persistent_keepalive_interval != 0)
		wg_grouptask_enqueue(peer, &peer->p_send_keepalive);
}

static void
wg_peer_timers_init(struct wg_peer *peer)
{
	struct wg_timers *t = &peer->p_timers;

	bzero(t, sizeof(*t));

	rw_init(&peer->p_timers.t_lock, "wg_peer_timers");
	callout_init(&t->t_retry_handshake, true);
	callout_init(&t->t_send_keepalive, true);
	callout_init(&t->t_new_handshake, true);
	callout_init(&t->t_zero_key_material, true);
	callout_init(&t->t_persistent_keepalive, true);
}

static void
wg_timers_disable(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	t->t_disabled = 1;
	t->t_need_another_keepalive = 0;
	rw_wunlock(&t->t_lock);

	callout_del(&t->t_retry_handshake);
	callout_del(&t->t_send_keepalive);
	callout_del(&t->t_new_handshake);
	callout_del(&t->t_zero_key_material);
	callout_del(&t->t_persistent_keepalive);
}

void
wg_timers_set_persistent_keepalive(struct wg_timers *t, uint16_t interval)
{
	if (t->t_disabled)
		return;
	t->t_persistent_keepalive_interval = interval;
	wg_timers_run_persistent_keepalive(t);
}

int
wg_timers_get_persistent_keepalive(struct wg_timers *t, uint16_t *interval)
{
	*interval = t->t_persistent_keepalive_interval;
	return *interval > 0 ? 0 : ENOENT;
}

void
wg_timers_get_last_handshake(struct wg_timers *t, struct timespec *time)
{
	time->tv_sec = t->t_handshake_complete.tv_sec;
	time->tv_nsec = t->t_handshake_complete.tv_nsec;
}

static int
wg_timers_expired_handshake_last_sent(struct wg_timers *t)
{
	struct timespec uptime;
	struct timespec expire = { .tv_sec = REKEY_TIMEOUT, .tv_nsec = 0 };

	getnanouptime(&uptime);
	timespecadd(&t->t_handshake_last_sent, &expire, &expire);
	return timespeccmp(&uptime, &expire, >) ? ETIMEDOUT : 0;
}

static int
wg_timers_check_handshake_last_sent(struct wg_timers *t)
{
	int ret;

	if ((ret = wg_timers_expired_handshake_last_sent(t)) == ETIMEDOUT)
		getnanouptime(&t->t_handshake_last_sent);
	return (ret);
}

/* Queue */
void
wg_queue_init(struct wg_queue *q, const char *name)
{
	mtx_init(&q->q_mtx, name, NULL, MTX_DEF);
	mbufq_init(&q->q, MAX_QUEUED_PKT);
}

void
wg_queue_deinit(struct wg_queue*q)
{
	mtx_lock(&q->q_mtx);
	mbufq_drain(&q->q);
	mtx_unlock(&q->q_mtx);
	mtx_destroy(&q->q_mtx);
}

static struct mbuf *
wg_queue_dequeue(struct wg_queue *q, struct wg_tag **t)
{
	struct mbuf *m_, *m;

	m = NULL;
	mtx_lock(&q->q_mtx);
	m_ = mbufq_first(&q->q);
	if (m_ != NULL && (*t = wg_tag_get(m_))->t_done) {
		m = mbufq_dequeue(&q->q);
		m->m_flags &= ~M_ENQUEUED;
	}
	mtx_unlock(&q->q_mtx);
	return (m);
}

static int
wg_queue_len(struct wg_queue *q)
{

	return (mbufq_len(&q->q));
}

static int
wg_queue_in(struct wg_peer *peer, struct mbuf *m)
{
	struct buf_ring *parallel = peer->p_sc->sc_decap_ring;
	struct wg_queue		*serial = &peer->p_decap_queue;
	struct wg_tag		*t;
	int rc;

	MPASS(wg_tag_get(m) != NULL);

	mtx_lock(&serial->q_mtx);
	if ((rc = mbufq_enqueue(&serial->q, m)) == ENOBUFS) {
		wg_m_freem(m);
		if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
	} else {
		m->m_flags |= M_ENQUEUED;
		rc = buf_ring_enqueue(parallel, m);
		if (rc == ENOBUFS) {
			t = wg_tag_get(m);
			t->t_done = 1;
		}
	}
	mtx_unlock(&serial->q_mtx);
	return (rc);
}

int
wg_queue_out(struct wg_peer *peer, struct mbuf *m)
{
	struct buf_ring *parallel = peer->p_sc->sc_encap_ring;
	struct wg_queue		*serial = &peer->p_encap_queue;
	struct wg_tag		*t;
	int rc;

	if ((t = wg_tag_get(m)) == NULL) {
		wg_m_freem(m);
		return (ENOMEM);
	}
	t->t_peer = peer;
	mtx_lock(&serial->q_mtx);
	if ((rc = mbufq_enqueue(&serial->q, m)) == ENOBUFS) {
		wg_m_freem(m);
		if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
	} else {
		m->m_flags |= M_ENQUEUED;
		rc = buf_ring_enqueue(parallel, m);
		if (rc == ENOBUFS) {
			t = wg_tag_get(m);
			t->t_done = 1;
		}
	}
	mtx_unlock(&serial->q_mtx);
	return (rc);
}

/* Route */
int
wg_route_init(struct wg_route_table *tbl)
{
	int rc;

	tbl->t_count = 0;
	rc = rn_inithead((void **)&tbl->t_ip,
	    offsetof(struct sockaddr_in, sin_addr) * NBBY);

	if (rc == 0)
		return (ENOMEM);
	RADIX_NODE_HEAD_LOCK_INIT(tbl->t_ip);
#ifdef INET6
	rc = rn_inithead((void **)&tbl->t_ip6,
	    offsetof(struct sockaddr_in6, sin6_addr) * NBBY);
	if (rc == 0) {
		free(tbl->t_ip, M_RTABLE);
		return (ENOMEM);
	}
	RADIX_NODE_HEAD_LOCK_INIT(tbl->t_ip6);
#endif
	return (0);
}

void
wg_route_destroy(struct wg_route_table *tbl)
{
	RADIX_NODE_HEAD_DESTROY(tbl->t_ip);
	free(tbl->t_ip, M_RTABLE);
#ifdef INET6
	RADIX_NODE_HEAD_DESTROY(tbl->t_ip6);
	free(tbl->t_ip6, M_RTABLE);
#endif
}

int
wg_route_add(struct wg_route_table *tbl, struct wg_peer *peer,
			 const struct wg_allowedip *cidr_)
{
	struct radix_node	*node;
	struct radix_node_head	*root;
	struct wg_route *route;
	sa_family_t family;
	struct wg_allowedip *cidr;
	bool needfree = false;

	family = cidr_->a_addr.ss_family;
	if (family == AF_INET) {
		root = tbl->t_ip;
	} else if (family == AF_INET6) {
		root = tbl->t_ip6;
	} else {
		printf("bad sa_family %d\n", cidr_->a_addr.ss_family);
		return (EINVAL);
	}
	route = malloc(sizeof(*route), M_WG, M_WAITOK|M_ZERO);
	route->r_cidr = *cidr_;
	route->r_peer = peer;
	cidr = &route->r_cidr;

	RADIX_NODE_HEAD_LOCK(root);
	node = root->rnh_addaddr(&cidr->a_addr, &cidr->a_mask, &root->rh,
							route->r_nodes);
	if (node == route->r_nodes) {
		tbl->t_count++;
		CK_LIST_INSERT_HEAD(&peer->p_routes, route, r_entry);
	} else {
		needfree = true;
	}
	RADIX_NODE_HEAD_UNLOCK(root);
	if (needfree) {
		free(route, M_WG);
	}
	return (0);
}

struct peer_del_arg {
	struct radix_node_head * pda_head;
	struct wg_peer *pda_peer;
	struct wg_route_table *pda_tbl;
};

static int
wg_peer_remove(struct radix_node *rn, void *arg)
{
	struct peer_del_arg *pda = arg;
	struct wg_peer *peer = pda->pda_peer;
	struct radix_node_head * rnh = pda->pda_head;
	struct wg_route_table *tbl = pda->pda_tbl;
	struct wg_route *route = (struct wg_route *)rn;
	struct radix_node *x;

	if (route->r_peer != peer)
		return (0);
	x = (struct radix_node *)rnh->rnh_deladdr(&route->r_cidr.a_addr, NULL, &rnh->rh);
	if (x != NULL)	 {
		tbl->t_count--;
		CK_LIST_REMOVE(route, r_entry);
		free(route, M_WG);
	}
	return (0);
}

int
wg_route_delete(struct wg_route_table *tbl, struct wg_peer *peer)
{
	struct peer_del_arg pda;

	pda.pda_peer = peer;
	pda.pda_tbl = tbl;
	RADIX_NODE_HEAD_LOCK(tbl->t_ip);
	pda.pda_head = tbl->t_ip;
	rn_walktree(&tbl->t_ip->rh, wg_peer_remove, &pda);
	RADIX_NODE_HEAD_UNLOCK(tbl->t_ip);

	RADIX_NODE_HEAD_LOCK(tbl->t_ip6);
	pda.pda_head = tbl->t_ip6;
	rn_walktree(&tbl->t_ip6->rh, wg_peer_remove, &pda);
	RADIX_NODE_HEAD_UNLOCK(tbl->t_ip6);
	return (0);
}

struct wg_peer *
wg_route_lookup(struct wg_route_table *tbl, struct mbuf *m,
		enum route_direction dir)
{
	RADIX_NODE_HEAD_RLOCK_TRACKER;
	struct ip *iphdr;
	struct ip6_hdr *ip6hdr;
	struct radix_node_head *root;
	struct radix_node	*node;
	struct wg_peer	*peer = NULL;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	void *addr;
	int version;

	NET_EPOCH_ASSERT();
	iphdr = mtod(m, struct ip *);
	version = iphdr->ip_v;

	if (__predict_false(dir != IN && dir != OUT))
		panic("invalid route dir: %d\n", dir);

	if (version == 4) {
		root = tbl->t_ip;
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(struct sockaddr_in);
		if (dir == IN)
			sin.sin_addr = iphdr->ip_src;
		else
			sin.sin_addr = iphdr->ip_dst;
		addr = &sin;
	} else if (version == 6) {
		ip6hdr = mtod(m, struct ip6_hdr *);
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(struct sockaddr_in6);

		root = tbl->t_ip6;
		if (dir == IN)
			addr = &ip6hdr->ip6_src;
		else
			addr = &ip6hdr->ip6_dst;
		memcpy(&sin6.sin6_addr, addr, sizeof(sin6.sin6_addr));
		addr = &sin6;
	} else  {
		log(LOG_WARNING, "%s bad version %d\n", __func__, version);
		return (NULL);
	}
	RADIX_NODE_HEAD_RLOCK(root);
	if ((node = root->rnh_matchaddr(addr, &root->rh)) != NULL) {
		peer = ((struct wg_route *) node)->r_peer;
	} else {
		log(LOG_WARNING, "matchaddr failed\n");
	}
	RADIX_NODE_HEAD_RUNLOCK(root);
	return (peer);
}

/* Hashtable */
#define WG_HASHTABLE_PEER_FOREACH(peer, i, ht) \
	for (i = 0; i < HASHTABLE_PEER_SIZE; i++) \
		LIST_FOREACH(peer, &(ht)->h_peers[i], p_hash_entry)

#define WG_HASHTABLE_PEER_FOREACH_SAFE(peer, i, ht, tpeer) \
	for (i = 0; i < HASHTABLE_PEER_SIZE; i++) \
		CK_LIST_FOREACH_SAFE(peer, &(ht)->h_peers[i], p_hash_entry, tpeer)

void
wg_hashtable_init(struct wg_hashtable *ht)
{
	mtx_init(&ht->h_mtx, "hash lock", NULL, MTX_DEF);
	arc4random_buf(&ht->h_secret, sizeof(ht->h_secret));
	ht->h_num_peers = 0;
	ht->h_num_keys = 0;
	ht->h_peers = hashinit(HASHTABLE_PEER_SIZE, M_DEVBUF,
			&ht->h_peers_mask);
	ht->h_keys = hashinit(HASHTABLE_INDEX_SIZE, M_DEVBUF,
			&ht->h_keys_mask);
}

void
wg_hashtable_destroy(struct wg_hashtable *ht)
{
	MPASS(ht->h_num_peers == 0);
	MPASS(ht->h_num_keys == 0);
	mtx_destroy(&ht->h_mtx);
	hashdestroy(ht->h_peers, M_DEVBUF, ht->h_peers_mask);
	hashdestroy(ht->h_keys, M_DEVBUF, ht->h_keys_mask);
}

void
wg_hashtable_peer_insert(struct wg_hashtable *ht, struct wg_peer *peer)
{
	uint64_t key;

	key = siphash24(&ht->h_secret, peer->p_remote.r_public,
			sizeof(peer->p_remote.r_public));

	mtx_lock(&ht->h_mtx);
	ht->h_num_peers++;
	CK_LIST_INSERT_HEAD(&ht->h_peers[key & ht->h_peers_mask], peer, p_hash_entry);
	CK_LIST_INSERT_HEAD(&ht->h_peers_list, peer, p_entry);
	mtx_unlock(&ht->h_mtx);
}

struct wg_peer *
wg_peer_lookup(struct wg_softc *sc,
    const uint8_t pubkey[WG_KEY_SIZE])
{
	struct wg_hashtable *ht = &sc->sc_hashtable;
	uint64_t key;
	struct wg_peer *i = NULL;

	key = siphash24(&ht->h_secret, pubkey, WG_KEY_SIZE);

	mtx_lock(&ht->h_mtx);
	CK_LIST_FOREACH(i, &ht->h_peers[key & ht->h_peers_mask], p_hash_entry) {
		if (timingsafe_bcmp(i->p_remote.r_public, pubkey,
					WG_KEY_SIZE) == 0)
			break;
	}
	mtx_unlock(&ht->h_mtx);

	return i;
}

void
wg_hashtable_peer_remove(struct wg_hashtable *ht, struct wg_peer *peer)
{
	mtx_lock(&ht->h_mtx);
	ht->h_num_peers--;
	CK_LIST_REMOVE(peer, p_hash_entry);
	CK_LIST_REMOVE(peer, p_entry);
	mtx_unlock(&ht->h_mtx);
}

/* Cookie */
static int
wg_cookie_validate_packet(struct cookie_checker *checker, struct mbuf *m,
    int under_load)
{
	struct wg_endpoint *e;
	void *data;
	struct wg_pkt_initiation	*init;
	struct wg_pkt_response	*resp;
	struct cookie_macs *macs;
	int type, size;

	type = le32toh(*mtod(m, uint32_t *));
	data = m->m_data;
	e = wg_mbuf_endpoint_get(m);
	if (type == MESSAGE_HANDSHAKE_INITIATION) {
		init = mtod(m, struct wg_pkt_initiation *);
		macs = &init->m;
		size = sizeof(*init) - sizeof(*macs);
	} else if (type == MESSAGE_HANDSHAKE_RESPONSE) {
		resp = mtod(m, struct wg_pkt_response *);
		macs = &resp->m;
		size = sizeof(*resp) - sizeof(*macs);
	} else
		return EINVAL;

	return (cookie_checker_validate_macs(checker, macs, data, size,
	    under_load, &e->e_remote.r_sa));
}

/* Peer */
struct wg_peer *
wg_peer_alloc(struct wg_softc *sc)
{
	struct wg_peer *peer;
	device_t dev;

	dev = iflib_get_dev(sc->wg_ctx);
	peer = malloc(sizeof(*peer), M_WG, M_WAITOK|M_ZERO);
	peer->p_sc = sc;
	peer->p_id = atomic_fetchadd_long(&peer_counter, 1);
	CK_LIST_INIT(&peer->p_routes);

	rw_init(&peer->p_endpoint_lock, "wg_peer_endpoint");
	wg_queue_init(&peer->p_encap_queue, "sendq");
	wg_queue_init(&peer->p_decap_queue, "rxq");

	GROUPTASK_INIT(&peer->p_send_initiation, 0, (gtask_fn_t *)wg_send_initiation, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send_initiation, peer, dev, NULL, "wg initiation");
	GROUPTASK_INIT(&peer->p_send_keepalive, 0, (gtask_fn_t *)wg_send_keepalive, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send_keepalive, peer, dev, NULL, "wg keepalive");
	GROUPTASK_INIT(&peer->p_clear_secrets, 0, (gtask_fn_t *)noise_remote_clear, &peer->p_remote);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_clear_secrets, &peer->p_remote, dev, NULL, "wg clear secrets");

	GROUPTASK_INIT(&peer->p_send, 0, (gtask_fn_t *)wg_deliver_out, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send, peer, dev, NULL, "wg send");
	GROUPTASK_INIT(&peer->p_recv, 0, (gtask_fn_t *)wg_deliver_in, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_recv, peer, dev, NULL, "wg recv");

	wg_peer_timers_init(peer);

	peer->p_tx_bytes = counter_u64_alloc(M_WAITOK);
	peer->p_rx_bytes = counter_u64_alloc(M_WAITOK);

	SLIST_INIT(&peer->p_unused_index);
	SLIST_INSERT_HEAD(&peer->p_unused_index, &peer->p_index[0],
	    i_unused_entry);
	SLIST_INSERT_HEAD(&peer->p_unused_index, &peer->p_index[1],
	    i_unused_entry);
	SLIST_INSERT_HEAD(&peer->p_unused_index, &peer->p_index[2],
	    i_unused_entry);

	return (peer);
}

static void
wg_peer_free_deferred(epoch_context_t ctx)
{
	struct wg_peer *peer;

	peer = __containerof(ctx, struct wg_peer, p_ctx);
	counter_u64_free(peer->p_tx_bytes);
	counter_u64_free(peer->p_rx_bytes);

	DPRINTF(peer->p_sc, "Peer %llu destroyed\n", (unsigned long long)peer->p_id);
	rw_destroy(&peer->p_timers.t_lock);
	rw_destroy(&peer->p_endpoint_lock);
	zfree(peer, M_WG);
}

void
wg_peer_destroy(struct wg_peer *peer)
{

	/* We first remove the peer from the hash table and route table, so
	 * that it cannot be referenced again */
	wg_route_delete(&peer->p_sc->sc_routes, peer);
	MPASS(CK_LIST_EMPTY(&peer->p_routes));

	/* TODO currently, if there is a timer added after here, then the peer
	 * can hang around for longer than we want. */
	wg_timers_disable(&peer->p_timers);
	GROUPTASK_DRAIN(&peer->p_clear_secrets);
	GROUPTASK_DRAIN(&peer->p_send_initiation);
	GROUPTASK_DRAIN(&peer->p_send_keepalive);
	GROUPTASK_DRAIN(&peer->p_recv);
	GROUPTASK_DRAIN(&peer->p_send);
	taskqgroup_detach(qgroup_if_io_tqg, &peer->p_clear_secrets);
	taskqgroup_detach(qgroup_if_io_tqg, &peer->p_send_initiation);
	taskqgroup_detach(qgroup_if_io_tqg, &peer->p_send_keepalive);
	taskqgroup_detach(qgroup_if_io_tqg, &peer->p_recv);
	taskqgroup_detach(qgroup_if_io_tqg, &peer->p_send);
	wg_queue_deinit(&peer->p_encap_queue);
	wg_queue_deinit(&peer->p_decap_queue);
	NET_EPOCH_CALL(wg_peer_free_deferred, &peer->p_ctx);
}

static void
wg_peer_send_buf(struct wg_peer *peer, uint8_t *buf, size_t len)
{
	struct wg_endpoint	 endpoint;

	counter_u64_add(peer->p_tx_bytes, len);
	wg_timers_event_any_authenticated_packet_traversal(&peer->p_timers);
	wg_timers_event_any_authenticated_packet_sent(&peer->p_timers);
	wg_peer_get_endpoint(peer, &endpoint);
	wg_send_buf(peer->p_sc, &endpoint, buf, len);
}

static void
wg_send_initiation(struct wg_peer *peer)
{
	struct wg_pkt_initiation pkt;
	struct epoch_tracker et;
	int ret;

	if (wg_timers_check_handshake_last_sent(&peer->p_timers) != ETIMEDOUT)
		return;

	NET_EPOCH_ENTER(et);
	ret = noise_create_initiation(&peer->p_remote, &pkt.init);
	if (ret)
		goto out;
	pkt.t = le32toh(MESSAGE_HANDSHAKE_INITIATION);
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	    sizeof(pkt)-sizeof(pkt.m));
	wg_peer_send_buf(peer, (uint8_t *)&pkt, sizeof(pkt));
	wg_timers_event_handshake_initiated(&peer->p_timers);
out:
	NET_EPOCH_EXIT(et);
}

static int
wg_send_response(struct wg_peer *peer)
{
	struct wg_pkt_response pkt;
	struct epoch_tracker et;
	int ret;

	NET_EPOCH_ENTER(et);

	DPRINTF(peer->p_sc, "Sending handshake response to peer %llu\n",
	    (unsigned long long)peer->p_id);

	ret = noise_create_response(&peer->p_remote, &pkt.resp);
	if (ret)
		goto out;
	pkt.t = MESSAGE_HANDSHAKE_RESPONSE;
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	     sizeof(pkt)-sizeof(pkt.m));
	wg_peer_send_buf(peer, (uint8_t*)&pkt, sizeof(pkt));
	wg_timers_event_handshake_responded(&peer->p_timers);
out:
	NET_EPOCH_EXIT(et);
	return (ret);
}

static void
wg_send_cookie(struct wg_softc *sc, struct cookie_macs *cm, uint32_t idx,
    struct mbuf *m)
{
	struct wg_pkt_cookie	pkt;
	struct wg_endpoint *e;

	DPRINTF(sc, "Sending cookie response for denied handshake message\n");

	pkt.t = le32toh(MESSAGE_HANDSHAKE_COOKIE);
	pkt.r_idx = idx;

	e = wg_mbuf_endpoint_get(m);
	cookie_checker_create_payload(&sc->sc_cookie, cm, pkt.nonce,
	    pkt.ec, &e->e_remote.r_sa);
	wg_send_buf(sc, e, (uint8_t *)&pkt, sizeof(pkt));
}

static void
wg_peer_set_endpoint_from_tag(struct wg_peer *peer, struct wg_tag *t)
{
	struct wg_endpoint *e = &t->t_endpoint;

	MPASS(e->e_remote.r_sa.sa_family != 0);
	if (memcmp(e, &peer->p_endpoint, sizeof(*e)) == 0)
		return;

	peer->p_endpoint = *e;
}

static void
wg_peer_clear_src(struct wg_peer *peer)
{
	rw_rlock(&peer->p_endpoint_lock);
	bzero(&peer->p_endpoint.e_local, sizeof(peer->p_endpoint.e_local));
	rw_runlock(&peer->p_endpoint_lock);
}

static void
wg_peer_get_endpoint(struct wg_peer *p, struct wg_endpoint *e)
{
	memcpy(e, &p->p_endpoint, sizeof(*e));
}

static void
wg_deliver_out(struct wg_peer *peer)
{
	struct epoch_tracker et;
	struct wg_tag *t;
	struct mbuf *m;
	struct wg_endpoint endpoint;
	int ret;

	NET_EPOCH_ENTER(et);
	if (peer->p_sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		goto done;

	wg_peer_get_endpoint(peer, &endpoint);

	while ((m = wg_queue_dequeue(&peer->p_encap_queue, &t)) != NULL) {
		/* t_mbuf will contain the encrypted packet */
		if (t->t_mbuf == NULL){
			if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OERRORS, 1);
			wg_m_freem(m);
			continue;
		}
		M_MOVE_PKTHDR(t->t_mbuf, m);
		ret = wg_send(peer->p_sc, &endpoint, t->t_mbuf);

		if (ret == 0) {
			wg_timers_event_any_authenticated_packet_traversal(
			    &peer->p_timers);
			wg_timers_event_any_authenticated_packet_sent(
			    &peer->p_timers);

			if (m->m_pkthdr.len != 0)
				wg_timers_event_data_sent(&peer->p_timers);
		} else if (ret == EADDRNOTAVAIL) {
			wg_peer_clear_src(peer);
			wg_peer_get_endpoint(peer, &endpoint);
		}
		wg_m_freem(m);
	}
done:
	NET_EPOCH_EXIT(et);
}

static void
wg_deliver_in(struct wg_peer *peer)
{
	struct mbuf *m;
	struct wg_softc *sc;
	struct wg_socket *so;
	struct epoch_tracker et;
	struct wg_tag *t;
	struct inpcb *inp;
	uint32_t af;
	int version;


	NET_EPOCH_ENTER(et);
	sc = peer->p_sc;
	if (sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		goto done;

	so = &sc->sc_socket;

	while ((m = wg_queue_dequeue(&peer->p_decap_queue, &t)) != NULL) {
		/* t_mbuf will contain the encrypted packet */
		if (t->t_mbuf == NULL){
			if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_IERRORS, 1);
			wg_m_freem(m);
			continue;
		}
		MPASS(m == t->t_mbuf);

		wg_timers_event_any_authenticated_packet_received(
		    &peer->p_timers);
		wg_timers_event_any_authenticated_packet_traversal(
		    &peer->p_timers);

		if (m->m_pkthdr.len == 0) {
			wg_m_freem(m);
			continue;
		}
		counter_u64_add(peer->p_rx_bytes, m->m_pkthdr.len);

		m->m_flags &= ~(M_MCAST | M_BCAST);
		m->m_pkthdr.rcvif = sc->sc_ifp;
		version = mtod(m, struct ip *)->ip_v;
		if (version == IPVERSION) {
			af = AF_INET;
			BPF_MTAP2(sc->sc_ifp, &af, sizeof(af), m);
			inp = sotoinpcb(so->so_so4);
			CURVNET_SET(inp->inp_vnet);
			ip_input(m);
			CURVNET_RESTORE();
		}	else if (version == 6) {
			af = AF_INET;
			BPF_MTAP2(sc->sc_ifp, &af, sizeof(af), m);
			inp = sotoinpcb(so->so_so6);
			CURVNET_SET(inp->inp_vnet);
			ip6_input(m);
			CURVNET_RESTORE();
		} else
			wg_m_freem(m);

		wg_timers_event_data_received(&peer->p_timers);
	}
done:
	NET_EPOCH_EXIT(et);
}

static void
wg_send_buf(struct wg_softc *sc, struct wg_endpoint *e, uint8_t *buf,
    size_t len)
{
	struct mbuf	*m;
	int		 ret = 0;

retry:
	m = m_gethdr(M_WAITOK, MT_DATA);
	m->m_len = 0;
	m_copyback(m, 0, len, buf);

	if (ret == 0) {
		ret = wg_send(sc, e, m);
		/* Retry if we couldn't bind to e->e_local */
		if (ret == EADDRNOTAVAIL) {
			bzero(&e->e_local, sizeof(e->e_local));
			goto retry;
		}
	} else {
		wg_send(sc, e, m);
	}
}

static void
wg_send_keepalive(struct wg_peer *peer)
{
	struct mbuf *m = NULL;
	struct wg_tag *t;
	struct epoch_tracker et;

	if (wg_queue_len(&peer->p_encap_queue) != 0)
		goto send;
	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return;
	if ((t = wg_tag_get(m)) == NULL) {
		wg_m_freem(m);
		return;
	}
	t->t_peer = peer;
	t->t_mbuf = NULL;
	t->t_done = 0;
	t->t_mtu = 0; /* MTU == 0 OK for keepalive */
send:
	NET_EPOCH_ENTER(et);
	if (m != NULL)
		wg_queue_out(peer, m);
	if (noise_remote_ready(&peer->p_remote) == 0) {
		wg_encrypt_dispatch(peer->p_sc);
	} else {
		wg_timers_event_want_initiation(&peer->p_timers);
	}
	NET_EPOCH_EXIT(et);
}

/* Packet */
static void
verify_endpoint(struct mbuf *m)
{
#ifdef INVARIANTS
	struct wg_endpoint *e = wg_mbuf_endpoint_get(m);

	MPASS(e->e_remote.r_sa.sa_family != 0);
#endif
}

static void
wg_handshake(struct wg_softc *sc, struct mbuf *m)
{
	struct wg_pkt_initiation *init;
	struct wg_pkt_response *resp;
	struct noise_remote	*remote;
	struct wg_pkt_cookie		*cook;
	struct wg_peer	*peer;
	struct wg_tag *t;

	/* This is global, so that our load calculation applies to the whole
	 * system. We don't care about races with it at all.
	 */
	static struct timeval wg_last_underload;
	int packet_needs_cookie;
	int underload, res;

	underload = mbufq_len(&sc->sc_handshake_queue) >=
			MAX_QUEUED_INCOMING_HANDSHAKES / 8;
	if (underload)
		getmicrouptime(&wg_last_underload);
	else if (wg_last_underload.tv_sec != 0) {
		if (!ratecheck(&wg_last_underload, &underload_interval))
			underload = 1;
		else
			bzero(&wg_last_underload, sizeof(wg_last_underload));
	}

    res = wg_cookie_validate_packet(&sc->sc_cookie, m,
	    underload);

	if (res && res != EAGAIN) {
		printf("validate_packet got %d\n", res);
		goto free;
	}
	packet_needs_cookie = (res == EAGAIN);

	t = wg_tag_get(m);
	switch (le32toh(*mtod(m, uint32_t *))) {
	case MESSAGE_HANDSHAKE_INITIATION:
		init = mtod(m, struct wg_pkt_initiation *);

		if (packet_needs_cookie) {
			wg_send_cookie(sc, &init->m, init->init.s_idx, m);
			return;
		}
		if (noise_consume_initiation(&sc->sc_local, &remote,
		    &init->init) != 0) {
			DPRINTF(sc, "Invalid handshake initiation");
			goto free;
		}

		peer = CONTAINER_OF(remote, struct wg_peer, p_remote);
		DPRINTF(sc, "Receiving handshake initiation from peer %llu\n",
		    (unsigned long long)peer->p_id);
		res = wg_send_response(peer);
		if (res == 0 && noise_remote_begin_session(&peer->p_remote) == 0)
			wg_timers_event_session_derived(&peer->p_timers);
		break;
	case MESSAGE_HANDSHAKE_RESPONSE:
		resp = mtod(m, struct wg_pkt_response *);

		if (packet_needs_cookie) {
			wg_send_cookie(sc, &resp->m, resp->resp.s_idx, m);
			return;
		}

		if ((remote = wg_index_get(sc, resp->resp.r_idx)) == NULL) {
			DPRINTF(sc, "Unknown handshake response\n");
			goto free;
		}
		peer = CONTAINER_OF(remote, struct wg_peer, p_remote);

		if (noise_consume_response(remote, &resp->resp) != 0) {
			DPRINTF(sc, "Invalid handshake response\n");
			goto free;
		}

		DPRINTF(sc, "Receiving handshake response from peer %llu\n",
				(unsigned long long)peer->p_id);
		counter_u64_add(peer->p_rx_bytes, sizeof(*resp));
		wg_peer_set_endpoint_from_tag(peer, t);
		if (noise_remote_begin_session(&peer->p_remote) == 0) {
			wg_timers_event_session_derived(&peer->p_timers);
			wg_timers_event_handshake_complete(&peer->p_timers);
		}
		break;
	case MESSAGE_HANDSHAKE_COOKIE:
		cook = mtod(m, struct wg_pkt_cookie *);

		if ((remote = wg_index_get(sc, cook->r_idx)) == NULL) {
			DPRINTF(sc, "Unknown cookie index\n");
			goto free;
		}

		peer = CONTAINER_OF(remote, struct wg_peer, p_remote);

		if (cookie_maker_consume_payload(&peer->p_cookie,
		    cook->nonce, cook->ec) != 0) {
			DPRINTF(sc, "Could not decrypt cookie response\n");
			goto free;
		}

		DPRINTF(sc, "Receiving cookie response\n");
		goto free;
	default:
		goto free;
	}
	MPASS(peer != NULL);
	wg_timers_event_any_authenticated_packet_received(&peer->p_timers);
	wg_timers_event_any_authenticated_packet_traversal(&peer->p_timers);

free:
	wg_m_freem(m);
}

static void
wg_encap(struct wg_softc *sc, struct mbuf *m)
{
	struct wg_pkt_data *data;
	size_t padding_len, plaintext_len, out_len;
	struct mbuf *mc;
	struct wg_peer *peer;
	struct wg_tag *t;
	int res;

	if (sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		return;

	NET_EPOCH_ASSERT();
	t = wg_tag_get(m);
	peer = t->t_peer;

	plaintext_len = MIN(WG_PKT_WITH_PADDING(m->m_pkthdr.len), t->t_mtu);
	padding_len = plaintext_len - m->m_pkthdr.len;
	out_len = sizeof(struct wg_pkt_data) + plaintext_len + NOISE_MAC_SIZE;

	if ((mc = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, MCLBYTES)) == NULL)
		goto error;

	data = mtod(mc, struct wg_pkt_data *);
	m_copydata(m, 0, m->m_pkthdr.len, data->data.buf);
	bzero(data->data.buf + m->m_pkthdr.len, padding_len);

	data->t = htole32(MESSAGE_DATA);

	res = noise_remote_encrypt(&peer->p_remote, &data->data, plaintext_len);

	if (__predict_false(res)) {
		if (res == EINVAL) {
			wg_timers_event_want_initiation(&peer->p_timers);
			wg_m_freem(mc);
			goto error;
		} else if (res == ESTALE) {
			wg_timers_event_want_initiation(&peer->p_timers);
		} else 
			panic("unexpected result: %d\n", res);
	}

	/* A packet with length 0 is a keepalive packet */
	if (m->m_pkthdr.len == 0)
		DPRINTF(sc, "Sending keepalive packet to peer %llu\n",
		    (unsigned long long)peer->p_id);
	/*
	 * Set the correct output value here since it will be copied
	 * when we move the pkthdr in send.
	 */
	m->m_pkthdr.len = out_len;
	mc->m_flags &= ~(M_MCAST | M_BCAST);
	mc->m_len = out_len;
	m_calchdrlen(mc);

	counter_u64_add(peer->p_tx_bytes, m->m_pkthdr.len);

	t->t_mbuf = mc;
 error:
	/* XXX membar ? */
	t->t_done = 1;
	GROUPTASK_ENQUEUE(&peer->p_send);
}

static void
wg_decap(struct wg_softc *sc, struct mbuf *m)
{
	struct wg_pkt_data *data;
	struct wg_peer *peer, *routed_peer;
	struct wg_tag *t;
	size_t plaintext_len;
	uint8_t version;
	int res;

	if (sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		return;

	NET_EPOCH_ASSERT();
	data = mtod(m, struct wg_pkt_data *);
	plaintext_len = m->m_pkthdr.len - sizeof(struct wg_pkt_data);

	t = wg_tag_get(m);
	peer = t->t_peer;

	res = noise_remote_decrypt(&peer->p_remote, &data->data, plaintext_len);
	if (__predict_false(res)) {
		DPRINTF(sc, "noise_remote_decrypt fail %d \n", res);
		if (res == EINVAL) {
			goto error;
		} else if (res == ECONNRESET) {
			wg_timers_event_handshake_complete(&peer->p_timers);
		} else if (res == ESTALE) {
			wg_timers_event_want_initiation(&peer->p_timers);
		} else  {
			panic("unexpected response: %d\n", res);
		}
	}
	wg_peer_set_endpoint_from_tag(peer, t);
	counter_u64_add(peer->p_rx_bytes, m->m_pkthdr.len);

	/* Remove the data header, and crypto mac tail from the packet */
	m_adj(m, sizeof(struct wg_pkt_data));
	m_adj(m, -NOISE_MAC_SIZE);

	/* A packet with length 0 is a keepalive packet */
	if (m->m_pkthdr.len == 0) {
		DPRINTF(peer->p_sc, "Receiving keepalive packet from peer "
		    "%llu\n", (unsigned long long)peer->p_id);
		goto done;
	}

	version = mtod(m, struct ip *)->ip_v;
	if (version != IPVERSION && version != 6) {
		DPRINTF(peer->p_sc, "Packet is neither ipv4 nor ipv6 from peer "
				"%llu\n", (unsigned long long)peer->p_id);
		goto error;
	}

	routed_peer = wg_route_lookup(&peer->p_sc->sc_routes, m, IN);
	if (routed_peer != peer) {
		DPRINTF(peer->p_sc, "Packet has unallowed src IP from peer "
		    "%llu\n", (unsigned long long)peer->p_id);
		goto error;
	}

done:
	t->t_mbuf = m;
error:
	t->t_done = 1;
	GROUPTASK_ENQUEUE(&peer->p_recv);
}

void
wg_softc_handshake_receive(struct wg_softc *sc)
{
	struct mbuf *m;

	while ((m = mbufq_dequeue(&sc->sc_handshake_queue)) != NULL) {
		verify_endpoint(m);
		wg_handshake(sc, m);
	}
}

void
wg_softc_decrypt(struct wg_softc *sc)
{
	struct epoch_tracker et;
	struct mbuf *m;

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	if (__predict_false(!is_fpu_kern_thread(0)))
		fpu_kern_thread(FPU_KERN_NORMAL);
#endif
	NET_EPOCH_ENTER(et);
	while ((m = buf_ring_dequeue_mc(sc->sc_decap_ring)) != NULL)
		wg_decap(sc, m);
	NET_EPOCH_EXIT(et);
}

void
wg_softc_encrypt(struct wg_softc *sc)
{
	struct mbuf *m;
	struct epoch_tracker et;

#if defined(__aarch64__) || defined(__amd64__) || defined(__i386__)
	if (__predict_false(!is_fpu_kern_thread(0)))
		fpu_kern_thread(FPU_KERN_NORMAL);
#endif
	NET_EPOCH_ENTER(et);
	while ((m = buf_ring_dequeue_mc(sc->sc_encap_ring)) != NULL)
		wg_encap(sc, m);
	NET_EPOCH_EXIT(et);
}

struct noise_remote *
wg_remote_get(struct wg_softc *sc, uint8_t public[NOISE_KEY_SIZE])
{
	struct wg_peer *peer;

	if ((peer = wg_peer_lookup(sc, public)) == NULL)
		return (NULL);
	return (&peer->p_remote);
}

uint32_t
wg_index_set(struct wg_softc *sc, struct noise_remote *remote)
{
	struct wg_index *index, *iter;
	struct wg_peer	*peer;
	uint32_t	 key;

	/* We can modify this without a lock as wg_index_set, wg_index_drop are
	 * guaranteed to be serialised (per remote). */
	peer = CONTAINER_OF(remote, struct wg_peer, p_remote);
	index = SLIST_FIRST(&peer->p_unused_index);
	MPASS(index != NULL);
	SLIST_REMOVE_HEAD(&peer->p_unused_index, i_unused_entry);

	index->i_value = remote;

	rw_wlock(&sc->sc_index_lock);
assign_id:
	key = index->i_key = arc4random();
	key &= sc->sc_index_mask;
	LIST_FOREACH(iter, &sc->sc_index[key], i_entry)
		if (iter->i_key == index->i_key)
			goto assign_id;

	LIST_INSERT_HEAD(&sc->sc_index[key], index, i_entry);

	rw_wunlock(&sc->sc_index_lock);

	/* Likewise, no need to lock for index here. */
	return index->i_key;
}

struct noise_remote *
wg_index_get(struct wg_softc *sc, uint32_t key0)
{
	struct wg_index		*iter;
	struct noise_remote	*remote = NULL;
	uint32_t		 key = key0 & sc->sc_index_mask;

	rw_enter_read(&sc->sc_index_lock);
	LIST_FOREACH(iter, &sc->sc_index[key], i_entry)
		if (iter->i_key == key0) {
			remote = iter->i_value;
			break;
		}
	rw_exit_read(&sc->sc_index_lock);
	return remote;
}

void
wg_index_drop(struct wg_softc *sc, uint32_t key0)
{
	struct wg_index	*iter;
	struct wg_peer	*peer = NULL;
	uint32_t	 key = key0 & sc->sc_index_mask;

	rw_enter_write(&sc->sc_index_lock);
	LIST_FOREACH(iter, &sc->sc_index[key], i_entry)
		if (iter->i_key == key0) {
			LIST_REMOVE(iter, i_entry);
			break;
		}
	rw_exit_write(&sc->sc_index_lock);

	if (iter == NULL)
		return;

	/* We expect a peer */
	peer = CONTAINER_OF(iter->i_value, struct wg_peer, p_remote);
	MPASS(peer != NULL);
	SLIST_INSERT_HEAD(&peer->p_unused_index, iter, i_unused_entry);
}

static void
wg_input(struct mbuf *m0, int offset, struct inpcb *inpcb,
		 const struct sockaddr *srcsa, void *_sc)
{
	struct wg_pkt_data *pkt_data;
	struct wg_endpoint *e;
	struct wg_softc *sc = _sc;
	struct udphdr *uh;
	struct mbuf *m;
	int pktlen, pkttype, hlen;
	struct noise_remote *remote;
	struct wg_tag *t;
	void *data;

	uh = (struct udphdr *)(m0->m_data + offset);
	hlen = offset + sizeof(struct udphdr);

	m_adj(m0, hlen);

	if ((m = m_defrag(m0, M_NOWAIT)) == NULL) {
		DPRINTF(sc, "DEFRAG fail\n");
		return;
	}
	data = mtod(m, void *);
	pkttype = le32toh(*(uint32_t*)data);
	t = wg_tag_get(m);
	if (t == NULL) {
		DPRINTF(sc, "no tag\n");
		goto free;
	}
	e = wg_mbuf_endpoint_get(m);
	e->e_remote.r_sa = *srcsa;
	verify_endpoint(m);

	if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	pktlen = m->m_pkthdr.len;

	if ((pktlen == sizeof(struct wg_pkt_initiation) &&
		 pkttype == MESSAGE_HANDSHAKE_INITIATION) ||
		(pktlen == sizeof(struct wg_pkt_response) &&
		 pkttype == MESSAGE_HANDSHAKE_RESPONSE) ||
		(pktlen == sizeof(struct wg_pkt_cookie) &&
		 pkttype == MESSAGE_HANDSHAKE_COOKIE)) {
		verify_endpoint(m);
		if (mbufq_enqueue(&sc->sc_handshake_queue, m) == 0) {
			GROUPTASK_ENQUEUE(&sc->sc_handshake);
		} else
			DPRINTF(sc, "Dropping handshake packet\n");
	} else if (pktlen >= sizeof(struct wg_pkt_data) + NOISE_MAC_SIZE
	    && pkttype == MESSAGE_DATA) {

		pkt_data = data;
		remote = wg_index_get(sc, pkt_data->data.r_idx);
		if (remote == NULL) {
			DPRINTF(sc, "no remote\n");
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);
			wg_m_freem(m);
		} else if (buf_ring_count(sc->sc_decap_ring) > MAX_QUEUED_PACKETS) {
			DPRINTF(sc, "freeing excess packet on input\n");
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
			wg_m_freem(m);
		} else {
			t->t_peer = CONTAINER_OF(remote, struct wg_peer,
			    p_remote);
			t->t_mbuf = NULL;
			t->t_done = 0;

			wg_queue_in(t->t_peer, m);
			wg_decrypt_dispatch(sc);
		}
	} else {
		DPRINTF(sc, "Invalid packet\n");
free:
		wg_m_freem(m);
	}
}

void
wg_peer_remove_all(struct wg_softc *sc)
{
	struct wg_peer *peer, *tpeer;

	CK_LIST_FOREACH_SAFE(peer, &sc->sc_hashtable.h_peers_list,
	    p_entry, tpeer) {
		wg_hashtable_peer_remove(&peer->p_sc->sc_hashtable, peer);
		/* FIXME -- needs to be deferred */
		wg_peer_destroy(peer);
	}
}
