/* SPDX-License-Identifier: ISC
 *
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2019-2021 Matt Dunwoodie <ncon@noconroy.net>
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2021 Kyle Evans <kevans@FreeBSD.org>
 * Copyright (c) 2022 The FreeBSD Foundation
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/endian.h>
#include <sys/gtaskqueue.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/nv.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <machine/_inttypes.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/radix.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet6/nd6.h>

#include "wg_noise.h"
#include "wg_cookie.h"
#include "version.h"
#include "if_wg.h"

#define DEFAULT_MTU		(ETHERMTU - 80)
#define MAX_MTU			(IF_MAXMTU - 80)

#define MAX_STAGED_PKT		128
#define MAX_QUEUED_PKT		1024
#define MAX_QUEUED_PKT_MASK	(MAX_QUEUED_PKT - 1)

#define MAX_QUEUED_HANDSHAKES	4096

#define REKEY_TIMEOUT_JITTER	334 /* 1/3 sec, round for arc4random_uniform */
#define MAX_TIMER_HANDSHAKES	(90 / REKEY_TIMEOUT)
#define NEW_HANDSHAKE_TIMEOUT	(REKEY_TIMEOUT + KEEPALIVE_TIMEOUT)
#define UNDERLOAD_TIMEOUT	1

#define DPRINTF(sc, ...) if (sc->sc_ifp->if_flags & IFF_DEBUG) if_printf(sc->sc_ifp, ##__VA_ARGS__)

/* First byte indicating packet type on the wire */
#define WG_PKT_INITIATION htole32(1)
#define WG_PKT_RESPONSE htole32(2)
#define WG_PKT_COOKIE htole32(3)
#define WG_PKT_DATA htole32(4)

#define WG_PKT_PADDING		16
#define WG_KEY_SIZE		32

struct wg_pkt_initiation {
	uint32_t		t;
	uint32_t		s_idx;
	uint8_t			ue[NOISE_PUBLIC_KEY_LEN];
	uint8_t			es[NOISE_PUBLIC_KEY_LEN + NOISE_AUTHTAG_LEN];
	uint8_t			ets[NOISE_TIMESTAMP_LEN + NOISE_AUTHTAG_LEN];
	struct cookie_macs	m;
};

struct wg_pkt_response {
	uint32_t		t;
	uint32_t		s_idx;
	uint32_t		r_idx;
	uint8_t			ue[NOISE_PUBLIC_KEY_LEN];
	uint8_t			en[0 + NOISE_AUTHTAG_LEN];
	struct cookie_macs	m;
};

struct wg_pkt_cookie {
	uint32_t		t;
	uint32_t		r_idx;
	uint8_t			nonce[COOKIE_NONCE_SIZE];
	uint8_t			ec[COOKIE_ENCRYPTED_SIZE];
};

struct wg_pkt_data {
	uint32_t		t;
	uint32_t		r_idx;
	uint64_t		nonce;
	uint8_t			buf[];
};

struct wg_endpoint {
	union {
		struct sockaddr		r_sa;
		struct sockaddr_in	r_sin;
#ifdef INET6
		struct sockaddr_in6	r_sin6;
#endif
	} e_remote;
	union {
		struct in_addr		l_in;
#ifdef INET6
		struct in6_pktinfo	l_pktinfo6;
#define l_in6 l_pktinfo6.ipi6_addr
#endif
	} e_local;
};

struct aip_addr {
	uint8_t		length;
	union {
		uint8_t		bytes[16];
		uint32_t	ip;
		uint32_t	ip6[4];
		struct in_addr	in;
		struct in6_addr	in6;
	};
};

struct wg_aip {
	struct radix_node	 a_nodes[2];
	LIST_ENTRY(wg_aip)	 a_entry;
	struct aip_addr		 a_addr;
	struct aip_addr		 a_mask;
	struct wg_peer		*a_peer;
	sa_family_t		 a_af;
};

struct wg_packet {
	STAILQ_ENTRY(wg_packet)	 p_serial;
	STAILQ_ENTRY(wg_packet)	 p_parallel;
	struct wg_endpoint	 p_endpoint;
	struct noise_keypair	*p_keypair;
	uint64_t		 p_nonce;
	struct mbuf		*p_mbuf;
	int			 p_mtu;
	sa_family_t		 p_af;
	enum wg_ring_state {
		WG_PACKET_UNCRYPTED,
		WG_PACKET_CRYPTED,
		WG_PACKET_DEAD,
	}			 p_state;
};

STAILQ_HEAD(wg_packet_list, wg_packet);

struct wg_queue {
	struct mtx		 q_mtx;
	struct wg_packet_list	 q_queue;
	size_t			 q_len;
};

struct wg_peer {
	TAILQ_ENTRY(wg_peer)		 p_entry;
	uint64_t			 p_id;
	struct wg_softc			*p_sc;

	struct noise_remote		*p_remote;
	struct cookie_maker		 p_cookie;

	struct rwlock			 p_endpoint_lock;
	struct wg_endpoint		 p_endpoint;

	struct wg_queue	 		 p_stage_queue;
	struct wg_queue	 		 p_encrypt_serial;
	struct wg_queue	 		 p_decrypt_serial;

	bool				 p_enabled;
	bool				 p_need_another_keepalive;
	uint16_t			 p_persistent_keepalive_interval;
	struct callout			 p_new_handshake;
	struct callout			 p_send_keepalive;
	struct callout			 p_retry_handshake;
	struct callout			 p_zero_key_material;
	struct callout			 p_persistent_keepalive;

	struct mtx			 p_handshake_mtx;
	struct timespec			 p_handshake_complete;	/* nanotime */
	int				 p_handshake_retries;

	struct grouptask		 p_send;
	struct grouptask		 p_recv;

	counter_u64_t			 p_tx_bytes;
	counter_u64_t			 p_rx_bytes;

	LIST_HEAD(, wg_aip)		 p_aips;
	size_t				 p_aips_num;
};

struct wg_socket {
	struct socket	*so_so4;
	struct socket	*so_so6;
	uint32_t	 so_user_cookie;
	int		 so_fibnum;
	in_port_t	 so_port;
};

struct wg_softc {
	LIST_ENTRY(wg_softc)	 sc_entry;
	struct ifnet		*sc_ifp;
	int			 sc_flags;

	struct ucred		*sc_ucred;
	struct wg_socket	 sc_socket;

	TAILQ_HEAD(,wg_peer)	 sc_peers;
	size_t			 sc_peers_num;

	struct noise_local	*sc_local;
	struct cookie_checker	 sc_cookie;

	struct radix_node_head	*sc_aip4;
	struct radix_node_head	*sc_aip6;

	struct grouptask	 sc_handshake;
	struct wg_queue		 sc_handshake_queue;

	struct grouptask	*sc_encrypt;
	struct grouptask	*sc_decrypt;
	struct wg_queue		 sc_encrypt_parallel;
	struct wg_queue		 sc_decrypt_parallel;
	u_int			 sc_encrypt_last_cpu;
	u_int			 sc_decrypt_last_cpu;

	struct sx		 sc_lock;
};

#define	WGF_DYING	0x0001

#define MAX_LOOPS	8
#define MTAG_WGLOOP	0x77676c70 /* wglp */
#ifndef ENOKEY
#define	ENOKEY	ENOTCAPABLE
#endif

#define	GROUPTASK_DRAIN(gtask)			\
	gtaskqueue_drain((gtask)->gt_taskqueue, &(gtask)->gt_task)

#define BPF_MTAP2_AF(ifp, m, af) do { \
		uint32_t __bpf_tap_af = (af); \
		BPF_MTAP2(ifp, &__bpf_tap_af, sizeof(__bpf_tap_af), m); \
	} while (0)

static int clone_count;
static uma_zone_t wg_packet_zone;
static volatile unsigned long peer_counter = 0;
static const char wgname[] = "wg";
static unsigned wg_osd_jail_slot;

static struct sx wg_sx;
SX_SYSINIT(wg_sx, &wg_sx, "wg_sx");

static LIST_HEAD(, wg_softc) wg_list = LIST_HEAD_INITIALIZER(wg_list);

static TASKQGROUP_DEFINE(wg_tqg, mp_ncpus, 1);

MALLOC_DEFINE(M_WG, "WG", "wireguard");

VNET_DEFINE_STATIC(struct if_clone *, wg_cloner);

#define	V_wg_cloner	VNET(wg_cloner)
#define	WG_CAPS		IFCAP_LINKSTATE

struct wg_timespec64 {
	uint64_t	tv_sec;
	uint64_t	tv_nsec;
};

static int wg_socket_init(struct wg_softc *, in_port_t);
static int wg_socket_bind(struct socket **, struct socket **, in_port_t *);
static void wg_socket_set(struct wg_softc *, struct socket *, struct socket *);
static void wg_socket_uninit(struct wg_softc *);
static int wg_socket_set_sockopt(struct socket *, struct socket *, int, void *, size_t);
static int wg_socket_set_cookie(struct wg_softc *, uint32_t);
static int wg_socket_set_fibnum(struct wg_softc *, int);
static int wg_send(struct wg_softc *, struct wg_endpoint *, struct mbuf *);
static void wg_timers_enable(struct wg_peer *);
static void wg_timers_disable(struct wg_peer *);
static void wg_timers_set_persistent_keepalive(struct wg_peer *, uint16_t);
static void wg_timers_get_last_handshake(struct wg_peer *, struct wg_timespec64 *);
static void wg_timers_event_data_sent(struct wg_peer *);
static void wg_timers_event_data_received(struct wg_peer *);
static void wg_timers_event_any_authenticated_packet_sent(struct wg_peer *);
static void wg_timers_event_any_authenticated_packet_received(struct wg_peer *);
static void wg_timers_event_any_authenticated_packet_traversal(struct wg_peer *);
static void wg_timers_event_handshake_initiated(struct wg_peer *);
static void wg_timers_event_handshake_complete(struct wg_peer *);
static void wg_timers_event_session_derived(struct wg_peer *);
static void wg_timers_event_want_initiation(struct wg_peer *);
static void wg_timers_run_send_initiation(struct wg_peer *, bool);
static void wg_timers_run_retry_handshake(void *);
static void wg_timers_run_send_keepalive(void *);
static void wg_timers_run_new_handshake(void *);
static void wg_timers_run_zero_key_material(void *);
static void wg_timers_run_persistent_keepalive(void *);
static int wg_aip_add(struct wg_softc *, struct wg_peer *, sa_family_t, const void *, uint8_t);
static struct wg_peer *wg_aip_lookup(struct wg_softc *, sa_family_t, void *);
static void wg_aip_remove_all(struct wg_softc *, struct wg_peer *);
static struct wg_peer *wg_peer_alloc(struct wg_softc *, const uint8_t [WG_KEY_SIZE]);
static void wg_peer_free_deferred(struct noise_remote *);
static void wg_peer_destroy(struct wg_peer *);
static void wg_peer_destroy_all(struct wg_softc *);
static void wg_peer_send_buf(struct wg_peer *, uint8_t *, size_t);
static void wg_send_initiation(struct wg_peer *);
static void wg_send_response(struct wg_peer *);
static void wg_send_cookie(struct wg_softc *, struct cookie_macs *, uint32_t, struct wg_endpoint *);
static void wg_peer_set_endpoint(struct wg_peer *, struct wg_endpoint *);
static void wg_peer_clear_src(struct wg_peer *);
static void wg_peer_get_endpoint(struct wg_peer *, struct wg_endpoint *);
static void wg_send_buf(struct wg_softc *, struct wg_endpoint *, uint8_t *, size_t);
static void wg_send_keepalive(struct wg_peer *);
static void wg_handshake(struct wg_softc *, struct wg_packet *);
static void wg_encrypt(struct wg_softc *, struct wg_packet *);
static void wg_decrypt(struct wg_softc *, struct wg_packet *);
static void wg_softc_handshake_receive(struct wg_softc *);
static void wg_softc_decrypt(struct wg_softc *);
static void wg_softc_encrypt(struct wg_softc *);
static void wg_encrypt_dispatch(struct wg_softc *);
static void wg_decrypt_dispatch(struct wg_softc *);
static void wg_deliver_out(struct wg_peer *);
static void wg_deliver_in(struct wg_peer *);
static struct wg_packet *wg_packet_alloc(struct mbuf *);
static void wg_packet_free(struct wg_packet *);
static void wg_queue_init(struct wg_queue *, const char *);
static void wg_queue_deinit(struct wg_queue *);
static size_t wg_queue_len(struct wg_queue *);
static int wg_queue_enqueue_handshake(struct wg_queue *, struct wg_packet *);
static struct wg_packet *wg_queue_dequeue_handshake(struct wg_queue *);
static void wg_queue_push_staged(struct wg_queue *, struct wg_packet *);
static void wg_queue_enlist_staged(struct wg_queue *, struct wg_packet_list *);
static void wg_queue_delist_staged(struct wg_queue *, struct wg_packet_list *);
static void wg_queue_purge(struct wg_queue *);
static int wg_queue_both(struct wg_queue *, struct wg_queue *, struct wg_packet *);
static struct wg_packet *wg_queue_dequeue_serial(struct wg_queue *);
static struct wg_packet *wg_queue_dequeue_parallel(struct wg_queue *);
static void wg_input(struct mbuf *, int, struct inpcb *, const struct sockaddr *, void *);
static void wg_peer_send_staged(struct wg_peer *);
static int wg_clone_create(struct if_clone *ifc, char *name, size_t len,
	struct ifc_data *ifd, struct ifnet **ifpp);
static void wg_qflush(struct ifnet *);
static inline int determine_af_and_pullup(struct mbuf **m, sa_family_t *af);
static int wg_xmit(struct ifnet *, struct mbuf *, sa_family_t, uint32_t);
static int wg_transmit(struct ifnet *, struct mbuf *);
static int wg_output(struct ifnet *, struct mbuf *, const struct sockaddr *, struct route *);
static int wg_clone_destroy(struct if_clone *ifc, struct ifnet *ifp,
	uint32_t flags);
static bool wgc_privileged(struct wg_softc *);
static int wgc_get(struct wg_softc *, struct wg_data_io *);
static int wgc_set(struct wg_softc *, struct wg_data_io *);
static int wg_up(struct wg_softc *);
static void wg_down(struct wg_softc *);
static void wg_reassign(struct ifnet *, struct vnet *, char *unused);
static void wg_init(void *);
static int wg_ioctl(struct ifnet *, u_long, caddr_t);
static void vnet_wg_init(const void *);
static void vnet_wg_uninit(const void *);
static int wg_module_init(void);
static void wg_module_deinit(void);

/* TODO Peer */
static struct wg_peer *
wg_peer_alloc(struct wg_softc *sc, const uint8_t pub_key[WG_KEY_SIZE])
{
	struct wg_peer *peer;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	peer = malloc(sizeof(*peer), M_WG, M_WAITOK | M_ZERO);
	peer->p_remote = noise_remote_alloc(sc->sc_local, peer, pub_key);
	peer->p_tx_bytes = counter_u64_alloc(M_WAITOK);
	peer->p_rx_bytes = counter_u64_alloc(M_WAITOK);
	peer->p_id = peer_counter++;
	peer->p_sc = sc;

	cookie_maker_init(&peer->p_cookie, pub_key);

	rw_init(&peer->p_endpoint_lock, "wg_peer_endpoint");

	wg_queue_init(&peer->p_stage_queue, "stageq");
	wg_queue_init(&peer->p_encrypt_serial, "txq");
	wg_queue_init(&peer->p_decrypt_serial, "rxq");

	peer->p_enabled = false;
	peer->p_need_another_keepalive = false;
	peer->p_persistent_keepalive_interval = 0;
	callout_init(&peer->p_new_handshake, true);
	callout_init(&peer->p_send_keepalive, true);
	callout_init(&peer->p_retry_handshake, true);
	callout_init(&peer->p_persistent_keepalive, true);
	callout_init(&peer->p_zero_key_material, true);

	mtx_init(&peer->p_handshake_mtx, "peer handshake", NULL, MTX_DEF);
	bzero(&peer->p_handshake_complete, sizeof(peer->p_handshake_complete));
	peer->p_handshake_retries = 0;

	GROUPTASK_INIT(&peer->p_send, 0, (gtask_fn_t *)wg_deliver_out, peer);
	taskqgroup_attach(qgroup_wg_tqg, &peer->p_send, peer, NULL, NULL, "wg send");
	GROUPTASK_INIT(&peer->p_recv, 0, (gtask_fn_t *)wg_deliver_in, peer);
	taskqgroup_attach(qgroup_wg_tqg, &peer->p_recv, peer, NULL, NULL, "wg recv");

	LIST_INIT(&peer->p_aips);
	peer->p_aips_num = 0;

	return (peer);
}

static void
wg_peer_free_deferred(struct noise_remote *r)
{
	struct wg_peer *peer = noise_remote_arg(r);

	/* While there are no references remaining, we may still have
	 * p_{send,recv} executing (think empty queue, but wg_deliver_{in,out}
	 * needs to check the queue. We should wait for them and then free. */
	GROUPTASK_DRAIN(&peer->p_recv);
	GROUPTASK_DRAIN(&peer->p_send);
	taskqgroup_detach(qgroup_wg_tqg, &peer->p_recv);
	taskqgroup_detach(qgroup_wg_tqg, &peer->p_send);

	wg_queue_deinit(&peer->p_decrypt_serial);
	wg_queue_deinit(&peer->p_encrypt_serial);
	wg_queue_deinit(&peer->p_stage_queue);

	counter_u64_free(peer->p_tx_bytes);
	counter_u64_free(peer->p_rx_bytes);
	rw_destroy(&peer->p_endpoint_lock);
	mtx_destroy(&peer->p_handshake_mtx);

	cookie_maker_free(&peer->p_cookie);

	free(peer, M_WG);
}

static void
wg_peer_destroy(struct wg_peer *peer)
{
	struct wg_softc *sc = peer->p_sc;
	sx_assert(&sc->sc_lock, SX_XLOCKED);

	/* Disable remote and timers. This will prevent any new handshakes
	 * occuring. */
	noise_remote_disable(peer->p_remote);
	wg_timers_disable(peer);

	/* Now we can remove all allowed IPs so no more packets will be routed
	 * to the peer. */
	wg_aip_remove_all(sc, peer);

	/* Remove peer from the interface, then free. Some references may still
	 * exist to p_remote, so noise_remote_free will wait until they're all
	 * put to call wg_peer_free_deferred. */
	sc->sc_peers_num--;
	TAILQ_REMOVE(&sc->sc_peers, peer, p_entry);
	DPRINTF(sc, "Peer %" PRIu64 " destroyed\n", peer->p_id);
	noise_remote_free(peer->p_remote, wg_peer_free_deferred);
}

static void
wg_peer_destroy_all(struct wg_softc *sc)
{
	struct wg_peer *peer, *tpeer;
	TAILQ_FOREACH_SAFE(peer, &sc->sc_peers, p_entry, tpeer)
		wg_peer_destroy(peer);
}

static void
wg_peer_set_endpoint(struct wg_peer *peer, struct wg_endpoint *e)
{
	MPASS(e->e_remote.r_sa.sa_family != 0);
	if (memcmp(e, &peer->p_endpoint, sizeof(*e)) == 0)
		return;

	rw_wlock(&peer->p_endpoint_lock);
	peer->p_endpoint = *e;
	rw_wunlock(&peer->p_endpoint_lock);
}

static void
wg_peer_clear_src(struct wg_peer *peer)
{
	rw_wlock(&peer->p_endpoint_lock);
	bzero(&peer->p_endpoint.e_local, sizeof(peer->p_endpoint.e_local));
	rw_wunlock(&peer->p_endpoint_lock);
}

static void
wg_peer_get_endpoint(struct wg_peer *peer, struct wg_endpoint *e)
{
	rw_rlock(&peer->p_endpoint_lock);
	*e = peer->p_endpoint;
	rw_runlock(&peer->p_endpoint_lock);
}

/* Allowed IP */
static int
wg_aip_add(struct wg_softc *sc, struct wg_peer *peer, sa_family_t af, const void *addr, uint8_t cidr)
{
	struct radix_node_head	*root;
	struct radix_node	*node;
	struct wg_aip		*aip;
	int			 ret = 0;

	aip = malloc(sizeof(*aip), M_WG, M_WAITOK | M_ZERO);
	aip->a_peer = peer;
	aip->a_af = af;

	switch (af) {
#ifdef INET
	case AF_INET:
		if (cidr > 32) cidr = 32;
		root = sc->sc_aip4;
		aip->a_addr.in = *(const struct in_addr *)addr;
		aip->a_mask.ip = htonl(~((1LL << (32 - cidr)) - 1) & 0xffffffff);
		aip->a_addr.ip &= aip->a_mask.ip;
		aip->a_addr.length = aip->a_mask.length = offsetof(struct aip_addr, in) + sizeof(struct in_addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (cidr > 128) cidr = 128;
		root = sc->sc_aip6;
		aip->a_addr.in6 = *(const struct in6_addr *)addr;
		in6_prefixlen2mask(&aip->a_mask.in6, cidr);
		for (int i = 0; i < 4; i++)
			aip->a_addr.ip6[i] &= aip->a_mask.ip6[i];
		aip->a_addr.length = aip->a_mask.length = offsetof(struct aip_addr, in6) + sizeof(struct in6_addr);
		break;
#endif
	default:
		free(aip, M_WG);
		return (EAFNOSUPPORT);
	}

	RADIX_NODE_HEAD_LOCK(root);
	node = root->rnh_addaddr(&aip->a_addr, &aip->a_mask, &root->rh, aip->a_nodes);
	if (node == aip->a_nodes) {
		LIST_INSERT_HEAD(&peer->p_aips, aip, a_entry);
		peer->p_aips_num++;
	} else if (!node)
		node = root->rnh_lookup(&aip->a_addr, &aip->a_mask, &root->rh);
	if (!node) {
		free(aip, M_WG);
		ret = ENOMEM;
	} else if (node != aip->a_nodes) {
		free(aip, M_WG);
		aip = (struct wg_aip *)node;
		if (aip->a_peer != peer) {
			LIST_REMOVE(aip, a_entry);
			aip->a_peer->p_aips_num--;
			aip->a_peer = peer;
			LIST_INSERT_HEAD(&peer->p_aips, aip, a_entry);
			aip->a_peer->p_aips_num++;
		}
	}
	RADIX_NODE_HEAD_UNLOCK(root);
	return (ret);
}

static struct wg_peer *
wg_aip_lookup(struct wg_softc *sc, sa_family_t af, void *a)
{
	struct radix_node_head	*root;
	struct radix_node	*node;
	struct wg_peer		*peer;
	struct aip_addr		 addr;
	RADIX_NODE_HEAD_RLOCK_TRACKER;

	switch (af) {
	case AF_INET:
		root = sc->sc_aip4;
		memcpy(&addr.in, a, sizeof(addr.in));
		addr.length = offsetof(struct aip_addr, in) + sizeof(struct in_addr);
		break;
	case AF_INET6:
		root = sc->sc_aip6;
		memcpy(&addr.in6, a, sizeof(addr.in6));
		addr.length = offsetof(struct aip_addr, in6) + sizeof(struct in6_addr);
		break;
	default:
		return NULL;
	}

	RADIX_NODE_HEAD_RLOCK(root);
	node = root->rnh_matchaddr(&addr, &root->rh);
	if (node != NULL) {
		peer = ((struct wg_aip *)node)->a_peer;
		noise_remote_ref(peer->p_remote);
	} else {
		peer = NULL;
	}
	RADIX_NODE_HEAD_RUNLOCK(root);

	return (peer);
}

static void
wg_aip_remove_all(struct wg_softc *sc, struct wg_peer *peer)
{
	struct wg_aip		*aip, *taip;

	RADIX_NODE_HEAD_LOCK(sc->sc_aip4);
	LIST_FOREACH_SAFE(aip, &peer->p_aips, a_entry, taip) {
		if (aip->a_af == AF_INET) {
			if (sc->sc_aip4->rnh_deladdr(&aip->a_addr, &aip->a_mask, &sc->sc_aip4->rh) == NULL)
				panic("failed to delete aip %p", aip);
			LIST_REMOVE(aip, a_entry);
			peer->p_aips_num--;
			free(aip, M_WG);
		}
	}
	RADIX_NODE_HEAD_UNLOCK(sc->sc_aip4);

	RADIX_NODE_HEAD_LOCK(sc->sc_aip6);
	LIST_FOREACH_SAFE(aip, &peer->p_aips, a_entry, taip) {
		if (aip->a_af == AF_INET6) {
			if (sc->sc_aip6->rnh_deladdr(&aip->a_addr, &aip->a_mask, &sc->sc_aip6->rh) == NULL)
				panic("failed to delete aip %p", aip);
			LIST_REMOVE(aip, a_entry);
			peer->p_aips_num--;
			free(aip, M_WG);
		}
	}
	RADIX_NODE_HEAD_UNLOCK(sc->sc_aip6);

	if (!LIST_EMPTY(&peer->p_aips) || peer->p_aips_num != 0)
		panic("wg_aip_remove_all could not delete all %p", peer);
}

static int
wg_socket_init(struct wg_softc *sc, in_port_t port)
{
	struct ucred *cred = sc->sc_ucred;
	struct socket *so4 = NULL, *so6 = NULL;
	int rc;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	if (!cred)
		return (EBUSY);

	/*
	 * For socket creation, we use the creds of the thread that created the
	 * tunnel rather than the current thread to maintain the semantics that
	 * WireGuard has on Linux with network namespaces -- that the sockets
	 * are created in their home vnet so that they can be configured and
	 * functionally attached to a foreign vnet as the jail's only interface
	 * to the network.
	 */
#ifdef INET
	rc = socreate(AF_INET, &so4, SOCK_DGRAM, IPPROTO_UDP, cred, curthread);
	if (rc)
		goto out;

	rc = udp_set_kernel_tunneling(so4, wg_input, NULL, sc);
	/*
	 * udp_set_kernel_tunneling can only fail if there is already a tunneling function set.
	 * This should never happen with a new socket.
	 */
	MPASS(rc == 0);
#endif

#ifdef INET6
	rc = socreate(AF_INET6, &so6, SOCK_DGRAM, IPPROTO_UDP, cred, curthread);
	if (rc)
		goto out;
	rc = udp_set_kernel_tunneling(so6, wg_input, NULL, sc);
	MPASS(rc == 0);
#endif

	if (sc->sc_socket.so_user_cookie) {
		rc = wg_socket_set_sockopt(so4, so6, SO_USER_COOKIE, &sc->sc_socket.so_user_cookie, sizeof(sc->sc_socket.so_user_cookie));
		if (rc)
			goto out;
	}
	rc = wg_socket_set_sockopt(so4, so6, SO_SETFIB, &sc->sc_socket.so_fibnum, sizeof(sc->sc_socket.so_fibnum));
	if (rc)
		goto out;

	rc = wg_socket_bind(&so4, &so6, &port);
	if (!rc) {
		sc->sc_socket.so_port = port;
		wg_socket_set(sc, so4, so6);
	}
out:
	if (rc) {
		if (so4 != NULL)
			soclose(so4);
		if (so6 != NULL)
			soclose(so6);
	}
	return (rc);
}

static int wg_socket_set_sockopt(struct socket *so4, struct socket *so6, int name, void *val, size_t len)
{
	int ret4 = 0, ret6 = 0;
	struct sockopt sopt = {
		.sopt_dir = SOPT_SET,
		.sopt_level = SOL_SOCKET,
		.sopt_name = name,
		.sopt_val = val,
		.sopt_valsize = len
	};

	if (so4)
		ret4 = sosetopt(so4, &sopt);
	if (so6)
		ret6 = sosetopt(so6, &sopt);
	return (ret4 ?: ret6);
}

static int wg_socket_set_cookie(struct wg_softc *sc, uint32_t user_cookie)
{
	struct wg_socket *so = &sc->sc_socket;
	int ret;

	sx_assert(&sc->sc_lock, SX_XLOCKED);
	ret = wg_socket_set_sockopt(so->so_so4, so->so_so6, SO_USER_COOKIE, &user_cookie, sizeof(user_cookie));
	if (!ret)
		so->so_user_cookie = user_cookie;
	return (ret);
}

static int wg_socket_set_fibnum(struct wg_softc *sc, int fibnum)
{
	struct wg_socket *so = &sc->sc_socket;
	int ret;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	ret = wg_socket_set_sockopt(so->so_so4, so->so_so6, SO_SETFIB, &fibnum, sizeof(fibnum));
	if (!ret)
		so->so_fibnum = fibnum;
	return (ret);
}

static void
wg_socket_uninit(struct wg_softc *sc)
{
	wg_socket_set(sc, NULL, NULL);
}

static void
wg_socket_set(struct wg_softc *sc, struct socket *new_so4, struct socket *new_so6)
{
	struct wg_socket *so = &sc->sc_socket;
	struct socket *so4, *so6;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	so4 = atomic_load_ptr(&so->so_so4);
	so6 = atomic_load_ptr(&so->so_so6);
	atomic_store_ptr(&so->so_so4, new_so4);
	atomic_store_ptr(&so->so_so6, new_so6);

	if (!so4 && !so6)
		return;
	NET_EPOCH_WAIT();
	if (so4)
		soclose(so4);
	if (so6)
		soclose(so6);
}

static int
wg_socket_bind(struct socket **in_so4, struct socket **in_so6, in_port_t *requested_port)
{
	struct socket *so4 = *in_so4, *so6 = *in_so6;
	int ret4 = 0, ret6 = 0;
	in_port_t port = *requested_port;
	struct sockaddr_in sin = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};
	struct sockaddr_in6 sin6 = {
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_family = AF_INET6,
		.sin6_port = htons(port)
	};

	if (so4) {
		ret4 = sobind(so4, (struct sockaddr *)&sin, curthread);
		if (ret4 && ret4 != EADDRNOTAVAIL)
			return (ret4);
		if (!ret4 && !sin.sin_port) {
			struct sockaddr_in *bound_sin;
			int ret = so4->so_proto->pr_usrreqs->pru_sockaddr(so4,
			    (struct sockaddr **)&bound_sin);
			if (ret)
				return (ret);
			port = ntohs(bound_sin->sin_port);
			sin6.sin6_port = bound_sin->sin_port;
			free(bound_sin, M_SONAME);
		}
	}

	if (so6) {
		ret6 = sobind(so6, (struct sockaddr *)&sin6, curthread);
		if (ret6 && ret6 != EADDRNOTAVAIL)
			return (ret6);
		if (!ret6 && !sin6.sin6_port) {
			struct sockaddr_in6 *bound_sin6;
			int ret = so6->so_proto->pr_usrreqs->pru_sockaddr(so6,
			    (struct sockaddr **)&bound_sin6);
			if (ret)
				return (ret);
			port = ntohs(bound_sin6->sin6_port);
			free(bound_sin6, M_SONAME);
		}
	}

	if (ret4 && ret6)
		return (ret4);
	*requested_port = port;
	if (ret4 && !ret6 && so4) {
		soclose(so4);
		*in_so4 = NULL;
	} else if (ret6 && !ret4 && so6) {
		soclose(so6);
		*in_so6 = NULL;
	}
	return (0);
}

static int
wg_send(struct wg_softc *sc, struct wg_endpoint *e, struct mbuf *m)
{
	struct epoch_tracker et;
	struct sockaddr *sa;
	struct wg_socket *so = &sc->sc_socket;
	struct socket *so4, *so6;
	struct mbuf *control = NULL;
	int ret = 0;
	size_t len = m->m_pkthdr.len;

	/* Get local control address before locking */
	if (e->e_remote.r_sa.sa_family == AF_INET) {
		if (e->e_local.l_in.s_addr != INADDR_ANY)
			control = sbcreatecontrol((caddr_t)&e->e_local.l_in,
			    sizeof(struct in_addr), IP_SENDSRCADDR,
			    IPPROTO_IP);
#ifdef INET6
	} else if (e->e_remote.r_sa.sa_family == AF_INET6) {
		if (!IN6_IS_ADDR_UNSPECIFIED(&e->e_local.l_in6))
			control = sbcreatecontrol((caddr_t)&e->e_local.l_pktinfo6,
			    sizeof(struct in6_pktinfo), IPV6_PKTINFO,
			    IPPROTO_IPV6);
#endif
	} else {
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	/* Get remote address */
	sa = &e->e_remote.r_sa;

	NET_EPOCH_ENTER(et);
	so4 = atomic_load_ptr(&so->so_so4);
	so6 = atomic_load_ptr(&so->so_so6);
	if (e->e_remote.r_sa.sa_family == AF_INET && so4 != NULL)
		ret = sosend(so4, sa, NULL, m, control, 0, curthread);
	else if (e->e_remote.r_sa.sa_family == AF_INET6 && so6 != NULL)
		ret = sosend(so6, sa, NULL, m, control, 0, curthread);
	else {
		ret = ENOTCONN;
		m_freem(control);
		m_freem(m);
	}
	NET_EPOCH_EXIT(et);
	if (ret == 0) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OBYTES, len);
	}
	return (ret);
}

static void
wg_send_buf(struct wg_softc *sc, struct wg_endpoint *e, uint8_t *buf, size_t len)
{
	struct mbuf	*m;
	int		 ret = 0;
	bool		 retried = false;

retry:
	m = m_get2(len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (!m) {
		ret = ENOMEM;
		goto out;
	}
	m_copyback(m, 0, len, buf);

	if (ret == 0) {
		ret = wg_send(sc, e, m);
		/* Retry if we couldn't bind to e->e_local */
		if (ret == EADDRNOTAVAIL && !retried) {
			bzero(&e->e_local, sizeof(e->e_local));
			retried = true;
			goto retry;
		}
	} else {
		ret = wg_send(sc, e, m);
	}
out:
	if (ret)
		DPRINTF(sc, "Unable to send packet: %d\n", ret);
}

/* Timers */
static void
wg_timers_enable(struct wg_peer *peer)
{
	atomic_store_bool(&peer->p_enabled, true);
	wg_timers_run_persistent_keepalive(peer);
}

static void
wg_timers_disable(struct wg_peer *peer)
{
	/* By setting p_enabled = false, then calling NET_EPOCH_WAIT, we can be
	 * sure no new handshakes are created after the wait. This is because
	 * all callout_resets (scheduling the callout) are guarded by
	 * p_enabled. We can be sure all sections that read p_enabled and then
	 * optionally call callout_reset are finished as they are surrounded by
	 * NET_EPOCH_{ENTER,EXIT}.
	 *
	 * However, as new callouts may be scheduled during NET_EPOCH_WAIT (but
	 * not after), we stop all callouts leaving no callouts active.
	 *
	 * We should also pull NET_EPOCH_WAIT out of the FOREACH(peer) loops, but the
	 * performance impact is acceptable for the time being. */
	atomic_store_bool(&peer->p_enabled, false);
	NET_EPOCH_WAIT();
	atomic_store_bool(&peer->p_need_another_keepalive, false);

	callout_stop(&peer->p_new_handshake);
	callout_stop(&peer->p_send_keepalive);
	callout_stop(&peer->p_retry_handshake);
	callout_stop(&peer->p_persistent_keepalive);
	callout_stop(&peer->p_zero_key_material);
}

static void
wg_timers_set_persistent_keepalive(struct wg_peer *peer, uint16_t interval)
{
	struct epoch_tracker et;
	if (interval != peer->p_persistent_keepalive_interval) {
		atomic_store_16(&peer->p_persistent_keepalive_interval, interval);
		NET_EPOCH_ENTER(et);
		if (atomic_load_bool(&peer->p_enabled))
			wg_timers_run_persistent_keepalive(peer);
		NET_EPOCH_EXIT(et);
	}
}

static void
wg_timers_get_last_handshake(struct wg_peer *peer, struct wg_timespec64 *time)
{
	mtx_lock(&peer->p_handshake_mtx);
	time->tv_sec = peer->p_handshake_complete.tv_sec;
	time->tv_nsec = peer->p_handshake_complete.tv_nsec;
	mtx_unlock(&peer->p_handshake_mtx);
}

static void
wg_timers_event_data_sent(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled) &&
	    !callout_pending(&peer->p_new_handshake))
		callout_reset(&peer->p_new_handshake, MSEC_2_TICKS(
		    NEW_HANDSHAKE_TIMEOUT * 1000 +
		    arc4random_uniform(REKEY_TIMEOUT_JITTER)),
		    wg_timers_run_new_handshake, peer);
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_data_received(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled)) {
		if (!callout_pending(&peer->p_send_keepalive))
			callout_reset(&peer->p_send_keepalive,
			    MSEC_2_TICKS(KEEPALIVE_TIMEOUT * 1000),
			    wg_timers_run_send_keepalive, peer);
		else
			atomic_store_bool(&peer->p_need_another_keepalive,
			    true);
	}
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_any_authenticated_packet_sent(struct wg_peer *peer)
{
	callout_stop(&peer->p_send_keepalive);
}

static void
wg_timers_event_any_authenticated_packet_received(struct wg_peer *peer)
{
	callout_stop(&peer->p_new_handshake);
}

static void
wg_timers_event_any_authenticated_packet_traversal(struct wg_peer *peer)
{
	struct epoch_tracker et;
	uint16_t interval;
	NET_EPOCH_ENTER(et);
	interval = atomic_load_16(&peer->p_persistent_keepalive_interval);
	if (atomic_load_bool(&peer->p_enabled) && interval > 0)
		callout_reset(&peer->p_persistent_keepalive,
		     MSEC_2_TICKS(interval * 1000),
		     wg_timers_run_persistent_keepalive, peer);
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_handshake_initiated(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled))
		callout_reset(&peer->p_retry_handshake, MSEC_2_TICKS(
		    REKEY_TIMEOUT * 1000 +
		    arc4random_uniform(REKEY_TIMEOUT_JITTER)),
		    wg_timers_run_retry_handshake, peer);
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_handshake_complete(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled)) {
		mtx_lock(&peer->p_handshake_mtx);
		callout_stop(&peer->p_retry_handshake);
		peer->p_handshake_retries = 0;
		getnanotime(&peer->p_handshake_complete);
		mtx_unlock(&peer->p_handshake_mtx);
		wg_timers_run_send_keepalive(peer);
	}
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_session_derived(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled))
		callout_reset(&peer->p_zero_key_material,
		    MSEC_2_TICKS(REJECT_AFTER_TIME * 3 * 1000),
		    wg_timers_run_zero_key_material, peer);
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_event_want_initiation(struct wg_peer *peer)
{
	struct epoch_tracker et;
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled))
		wg_timers_run_send_initiation(peer, false);
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_run_send_initiation(struct wg_peer *peer, bool is_retry)
{
	if (!is_retry)
		peer->p_handshake_retries = 0;
	if (noise_remote_initiation_expired(peer->p_remote) == ETIMEDOUT)
		wg_send_initiation(peer);
}

static void
wg_timers_run_retry_handshake(void *_peer)
{
	struct epoch_tracker et;
	struct wg_peer *peer = _peer;

	mtx_lock(&peer->p_handshake_mtx);
	if (peer->p_handshake_retries <= MAX_TIMER_HANDSHAKES) {
		peer->p_handshake_retries++;
		mtx_unlock(&peer->p_handshake_mtx);

		DPRINTF(peer->p_sc, "Handshake for peer %" PRIu64 " did not complete "
		    "after %d seconds, retrying (try %d)\n", peer->p_id,
		    REKEY_TIMEOUT, peer->p_handshake_retries + 1);
		wg_peer_clear_src(peer);
		wg_timers_run_send_initiation(peer, true);
	} else {
		mtx_unlock(&peer->p_handshake_mtx);

		DPRINTF(peer->p_sc, "Handshake for peer %" PRIu64 " did not complete "
		    "after %d retries, giving up\n", peer->p_id,
		    MAX_TIMER_HANDSHAKES + 2);

		callout_stop(&peer->p_send_keepalive);
		wg_queue_purge(&peer->p_stage_queue);
		NET_EPOCH_ENTER(et);
		if (atomic_load_bool(&peer->p_enabled) &&
		    !callout_pending(&peer->p_zero_key_material))
			callout_reset(&peer->p_zero_key_material,
			    MSEC_2_TICKS(REJECT_AFTER_TIME * 3 * 1000),
			    wg_timers_run_zero_key_material, peer);
		NET_EPOCH_EXIT(et);
	}
}

static void
wg_timers_run_send_keepalive(void *_peer)
{
	struct epoch_tracker et;
	struct wg_peer *peer = _peer;

	wg_send_keepalive(peer);
	NET_EPOCH_ENTER(et);
	if (atomic_load_bool(&peer->p_enabled) &&
	    atomic_load_bool(&peer->p_need_another_keepalive)) {
		atomic_store_bool(&peer->p_need_another_keepalive, false);
		callout_reset(&peer->p_send_keepalive,
		    MSEC_2_TICKS(KEEPALIVE_TIMEOUT * 1000),
		    wg_timers_run_send_keepalive, peer);
	}
	NET_EPOCH_EXIT(et);
}

static void
wg_timers_run_new_handshake(void *_peer)
{
	struct wg_peer *peer = _peer;

	DPRINTF(peer->p_sc, "Retrying handshake with peer %" PRIu64 " because we "
	    "stopped hearing back after %d seconds\n",
	    peer->p_id, NEW_HANDSHAKE_TIMEOUT);

	wg_peer_clear_src(peer);
	wg_timers_run_send_initiation(peer, false);
}

static void
wg_timers_run_zero_key_material(void *_peer)
{
	struct wg_peer *peer = _peer;

	DPRINTF(peer->p_sc, "Zeroing out keys for peer %" PRIu64 ", since we "
	    "haven't received a new one in %d seconds\n",
	    peer->p_id, REJECT_AFTER_TIME * 3);
	noise_remote_keypairs_clear(peer->p_remote);
}

static void
wg_timers_run_persistent_keepalive(void *_peer)
{
	struct wg_peer *peer = _peer;

	if (atomic_load_16(&peer->p_persistent_keepalive_interval) > 0)
		wg_send_keepalive(peer);
}

/* TODO Handshake */
static void
wg_peer_send_buf(struct wg_peer *peer, uint8_t *buf, size_t len)
{
	struct wg_endpoint endpoint;

	counter_u64_add(peer->p_tx_bytes, len);
	wg_timers_event_any_authenticated_packet_traversal(peer);
	wg_timers_event_any_authenticated_packet_sent(peer);
	wg_peer_get_endpoint(peer, &endpoint);
	wg_send_buf(peer->p_sc, &endpoint, buf, len);
}

static void
wg_send_initiation(struct wg_peer *peer)
{
	struct wg_pkt_initiation pkt;

	if (noise_create_initiation(peer->p_remote, &pkt.s_idx, pkt.ue,
	    pkt.es, pkt.ets) != 0)
		return;

	DPRINTF(peer->p_sc, "Sending handshake initiation to peer %" PRIu64 "\n", peer->p_id);

	pkt.t = WG_PKT_INITIATION;
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	    sizeof(pkt) - sizeof(pkt.m));
	wg_peer_send_buf(peer, (uint8_t *)&pkt, sizeof(pkt));
	wg_timers_event_handshake_initiated(peer);
}

static void
wg_send_response(struct wg_peer *peer)
{
	struct wg_pkt_response pkt;

	if (noise_create_response(peer->p_remote, &pkt.s_idx, &pkt.r_idx,
	    pkt.ue, pkt.en) != 0)
		return;

	DPRINTF(peer->p_sc, "Sending handshake response to peer %" PRIu64 "\n", peer->p_id);

	wg_timers_event_session_derived(peer);
	pkt.t = WG_PKT_RESPONSE;
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	     sizeof(pkt)-sizeof(pkt.m));
	wg_peer_send_buf(peer, (uint8_t*)&pkt, sizeof(pkt));
}

static void
wg_send_cookie(struct wg_softc *sc, struct cookie_macs *cm, uint32_t idx,
    struct wg_endpoint *e)
{
	struct wg_pkt_cookie	pkt;

	DPRINTF(sc, "Sending cookie response for denied handshake message\n");

	pkt.t = WG_PKT_COOKIE;
	pkt.r_idx = idx;

	cookie_checker_create_payload(&sc->sc_cookie, cm, pkt.nonce,
	    pkt.ec, &e->e_remote.r_sa);
	wg_send_buf(sc, e, (uint8_t *)&pkt, sizeof(pkt));
}

static void
wg_send_keepalive(struct wg_peer *peer)
{
	struct wg_packet *pkt;
	struct mbuf *m;

	if (wg_queue_len(&peer->p_stage_queue) > 0)
		goto send;
	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return;
	if ((pkt = wg_packet_alloc(m)) == NULL) {
		m_freem(m);
		return;
	}
	wg_queue_push_staged(&peer->p_stage_queue, pkt);
	DPRINTF(peer->p_sc, "Sending keepalive packet to peer %" PRIu64 "\n", peer->p_id);
send:
	wg_peer_send_staged(peer);
}

static void
wg_handshake(struct wg_softc *sc, struct wg_packet *pkt)
{
	struct wg_pkt_initiation	*init;
	struct wg_pkt_response		*resp;
	struct wg_pkt_cookie		*cook;
	struct wg_endpoint		*e;
	struct wg_peer			*peer;
	struct mbuf			*m;
	struct noise_remote		*remote = NULL;
	int				 res;
	bool				 underload = false;
	static sbintime_t		 wg_last_underload; /* sbinuptime */

	underload = wg_queue_len(&sc->sc_handshake_queue) >= MAX_QUEUED_HANDSHAKES / 8;
	if (underload) {
		wg_last_underload = getsbinuptime();
	} else if (wg_last_underload) {
		underload = wg_last_underload + UNDERLOAD_TIMEOUT * SBT_1S > getsbinuptime();
		if (!underload)
			wg_last_underload = 0;
	}

	m = pkt->p_mbuf;
	e = &pkt->p_endpoint;

	if ((pkt->p_mbuf = m = m_pullup(m, m->m_pkthdr.len)) == NULL)
		goto error;

	switch (*mtod(m, uint32_t *)) {
	case WG_PKT_INITIATION:
		init = mtod(m, struct wg_pkt_initiation *);

		res = cookie_checker_validate_macs(&sc->sc_cookie, &init->m,
				init, sizeof(*init) - sizeof(init->m),
				underload, &e->e_remote.r_sa,
				sc->sc_ifp->if_vnet);

		if (res == EINVAL) {
			DPRINTF(sc, "Invalid initiation MAC\n");
			goto error;
		} else if (res == ECONNREFUSED) {
			DPRINTF(sc, "Handshake ratelimited\n");
			goto error;
		} else if (res == EAGAIN) {
			wg_send_cookie(sc, &init->m, init->s_idx, e);
			goto error;
		} else if (res != 0) {
			panic("unexpected response: %d\n", res);
		}

		if (noise_consume_initiation(sc->sc_local, &remote,
		    init->s_idx, init->ue, init->es, init->ets) != 0) {
			DPRINTF(sc, "Invalid handshake initiation\n");
			goto error;
		}

		peer = noise_remote_arg(remote);

		DPRINTF(sc, "Receiving handshake initiation from peer %" PRIu64 "\n", peer->p_id);

		wg_peer_set_endpoint(peer, e);
		wg_send_response(peer);
		break;
	case WG_PKT_RESPONSE:
		resp = mtod(m, struct wg_pkt_response *);

		res = cookie_checker_validate_macs(&sc->sc_cookie, &resp->m,
				resp, sizeof(*resp) - sizeof(resp->m),
				underload, &e->e_remote.r_sa,
				sc->sc_ifp->if_vnet);

		if (res == EINVAL) {
			DPRINTF(sc, "Invalid response MAC\n");
			goto error;
		} else if (res == ECONNREFUSED) {
			DPRINTF(sc, "Handshake ratelimited\n");
			goto error;
		} else if (res == EAGAIN) {
			wg_send_cookie(sc, &resp->m, resp->s_idx, e);
			goto error;
		} else if (res != 0) {
			panic("unexpected response: %d\n", res);
		}

		if (noise_consume_response(sc->sc_local, &remote,
		    resp->s_idx, resp->r_idx, resp->ue, resp->en) != 0) {
			DPRINTF(sc, "Invalid handshake response\n");
			goto error;
		}

		peer = noise_remote_arg(remote);
		DPRINTF(sc, "Receiving handshake response from peer %" PRIu64 "\n", peer->p_id);

		wg_peer_set_endpoint(peer, e);
		wg_timers_event_session_derived(peer);
		wg_timers_event_handshake_complete(peer);
		break;
	case WG_PKT_COOKIE:
		cook = mtod(m, struct wg_pkt_cookie *);

		if ((remote = noise_remote_index(sc->sc_local, cook->r_idx)) == NULL) {
			DPRINTF(sc, "Unknown cookie index\n");
			goto error;
		}

		peer = noise_remote_arg(remote);

		if (cookie_maker_consume_payload(&peer->p_cookie,
		    cook->nonce, cook->ec) == 0) {
			DPRINTF(sc, "Receiving cookie response\n");
		} else {
			DPRINTF(sc, "Could not decrypt cookie response\n");
			goto error;
		}

		goto not_authenticated;
	default:
		panic("invalid packet in handshake queue");
	}

	wg_timers_event_any_authenticated_packet_received(peer);
	wg_timers_event_any_authenticated_packet_traversal(peer);

not_authenticated:
	counter_u64_add(peer->p_rx_bytes, m->m_pkthdr.len);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
error:
	if (remote != NULL)
		noise_remote_put(remote);
	wg_packet_free(pkt);
}

static void
wg_softc_handshake_receive(struct wg_softc *sc)
{
	struct wg_packet *pkt;
	while ((pkt = wg_queue_dequeue_handshake(&sc->sc_handshake_queue)) != NULL)
		wg_handshake(sc, pkt);
}

static void
wg_mbuf_reset(struct mbuf *m)
{

	struct m_tag *t, *tmp;

	/*
	 * We want to reset the mbuf to a newly allocated state, containing
	 * just the packet contents. Unfortunately FreeBSD doesn't seem to
	 * offer this anywhere, so we have to make it up as we go. If we can
	 * get this in kern/kern_mbuf.c, that would be best.
	 *
	 * Notice: this may break things unexpectedly but it is better to fail
	 *         closed in the extreme case than leak informtion in every
	 *         case.
	 *
	 * With that said, all this attempts to do is remove any extraneous
	 * information that could be present.
	 */

	M_ASSERTPKTHDR(m);

	m->m_flags &= ~(M_BCAST|M_MCAST|M_VLANTAG|M_PROMISC|M_PROTOFLAGS);

	M_HASHTYPE_CLEAR(m);
#ifdef NUMA
        m->m_pkthdr.numa_domain = M_NODOM;
#endif
	SLIST_FOREACH_SAFE(t, &m->m_pkthdr.tags, m_tag_link, tmp) {
		if ((t->m_tag_id != 0 || t->m_tag_cookie != MTAG_WGLOOP) &&
		    t->m_tag_id != PACKET_TAG_MACLABEL)
			m_tag_delete(m, t);
	}

	KASSERT((m->m_pkthdr.csum_flags & CSUM_SND_TAG) == 0,
	    ("%s: mbuf %p has a send tag", __func__, m));

	m->m_pkthdr.csum_flags = 0;
	m->m_pkthdr.PH_per.sixtyfour[0] = 0;
	m->m_pkthdr.PH_loc.sixtyfour[0] = 0;
}

static inline unsigned int
calculate_padding(struct wg_packet *pkt)
{
	unsigned int padded_size, last_unit = pkt->p_mbuf->m_pkthdr.len;

	/* Keepalive packets don't set p_mtu, but also have a length of zero. */
	if (__predict_false(pkt->p_mtu == 0)) {
		padded_size = (last_unit + (WG_PKT_PADDING - 1)) &
		    ~(WG_PKT_PADDING - 1);
		return (padded_size - last_unit);
	}

	if (__predict_false(last_unit > pkt->p_mtu))
		last_unit %= pkt->p_mtu;

	padded_size = (last_unit + (WG_PKT_PADDING - 1)) & ~(WG_PKT_PADDING - 1);
	if (pkt->p_mtu < padded_size)
		padded_size = pkt->p_mtu;
	return (padded_size - last_unit);
}

static void
wg_encrypt(struct wg_softc *sc, struct wg_packet *pkt)
{
	static const uint8_t	 padding[WG_PKT_PADDING] = { 0 };
	struct wg_pkt_data	*data;
	struct wg_peer		*peer;
	struct noise_remote	*remote;
	struct mbuf		*m;
	uint32_t		 idx;
	unsigned int		 padlen;
	enum wg_ring_state	 state = WG_PACKET_DEAD;

	remote = noise_keypair_remote(pkt->p_keypair);
	peer = noise_remote_arg(remote);
	m = pkt->p_mbuf;

	/* Pad the packet */
	padlen = calculate_padding(pkt);
	if (padlen != 0 && !m_append(m, padlen, padding))
		goto out;

	/* Do encryption */
	if (noise_keypair_encrypt(pkt->p_keypair, &idx, pkt->p_nonce, m) != 0)
		goto out;

	/* Put header into packet */
	M_PREPEND(m, sizeof(struct wg_pkt_data), M_NOWAIT);
	if (m == NULL)
		goto out;
	data = mtod(m, struct wg_pkt_data *);
	data->t = WG_PKT_DATA;
	data->r_idx = idx;
	data->nonce = htole64(pkt->p_nonce);

	wg_mbuf_reset(m);
	state = WG_PACKET_CRYPTED;
out:
	pkt->p_mbuf = m;
	atomic_store_rel_int(&pkt->p_state, state);
	GROUPTASK_ENQUEUE(&peer->p_send);
	noise_remote_put(remote);
}

static void
wg_decrypt(struct wg_softc *sc, struct wg_packet *pkt)
{
	struct wg_peer		*peer, *allowed_peer;
	struct noise_remote	*remote;
	struct mbuf		*m;
	int			 len;
	enum wg_ring_state	 state = WG_PACKET_DEAD;

	remote = noise_keypair_remote(pkt->p_keypair);
	peer = noise_remote_arg(remote);
	m = pkt->p_mbuf;

	/* Read nonce and then adjust to remove the header. */
	pkt->p_nonce = le64toh(mtod(m, struct wg_pkt_data *)->nonce);
	m_adj(m, sizeof(struct wg_pkt_data));

	if (noise_keypair_decrypt(pkt->p_keypair, pkt->p_nonce, m) != 0)
		goto out;

	/* A packet with length 0 is a keepalive packet */
	if (__predict_false(m->m_pkthdr.len == 0)) {
		DPRINTF(sc, "Receiving keepalive packet from peer "
		    "%" PRIu64 "\n", peer->p_id);
		state = WG_PACKET_CRYPTED;
		goto out;
	}

	/*
	 * We can let the network stack handle the intricate validation of the
	 * IP header, we just worry about the sizeof and the version, so we can
	 * read the source address in wg_aip_lookup.
	 */

	if (determine_af_and_pullup(&m, &pkt->p_af) == 0) {
		if (pkt->p_af == AF_INET) {
			struct ip *ip = mtod(m, struct ip *);
			allowed_peer = wg_aip_lookup(sc, AF_INET, &ip->ip_src);
			len = ntohs(ip->ip_len);
			if (len >= sizeof(struct ip) && len < m->m_pkthdr.len)
				m_adj(m, len - m->m_pkthdr.len);
		} else if (pkt->p_af == AF_INET6) {
			struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
			allowed_peer = wg_aip_lookup(sc, AF_INET6, &ip6->ip6_src);
			len = ntohs(ip6->ip6_plen) + sizeof(struct ip6_hdr);
			if (len < m->m_pkthdr.len)
				m_adj(m, len - m->m_pkthdr.len);
		} else
			panic("determine_af_and_pullup returned unexpected value");
	} else {
		DPRINTF(sc, "Packet is neither ipv4 nor ipv6 from peer %" PRIu64 "\n", peer->p_id);
		goto out;
	}

	/* We only want to compare the address, not dereference, so drop the ref. */
	if (allowed_peer != NULL)
		noise_remote_put(allowed_peer->p_remote);

	if (__predict_false(peer != allowed_peer)) {
		DPRINTF(sc, "Packet has unallowed src IP from peer %" PRIu64 "\n", peer->p_id);
		goto out;
	}

	wg_mbuf_reset(m);
	state = WG_PACKET_CRYPTED;
out:
	pkt->p_mbuf = m;
	atomic_store_rel_int(&pkt->p_state, state);
	GROUPTASK_ENQUEUE(&peer->p_recv);
	noise_remote_put(remote);
}

static void
wg_softc_decrypt(struct wg_softc *sc)
{
	struct wg_packet *pkt;

	while ((pkt = wg_queue_dequeue_parallel(&sc->sc_decrypt_parallel)) != NULL)
		wg_decrypt(sc, pkt);
}

static void
wg_softc_encrypt(struct wg_softc *sc)
{
	struct wg_packet *pkt;

	while ((pkt = wg_queue_dequeue_parallel(&sc->sc_encrypt_parallel)) != NULL)
		wg_encrypt(sc, pkt);
}

static void
wg_encrypt_dispatch(struct wg_softc *sc)
{
	/*
	 * The update to encrypt_last_cpu is racey such that we may
	 * reschedule the task for the same CPU multiple times, but
	 * the race doesn't really matter.
	 */
	u_int cpu = (sc->sc_encrypt_last_cpu + 1) % mp_ncpus;
	sc->sc_encrypt_last_cpu = cpu;
	GROUPTASK_ENQUEUE(&sc->sc_encrypt[cpu]);
}

static void
wg_decrypt_dispatch(struct wg_softc *sc)
{
	u_int cpu = (sc->sc_decrypt_last_cpu + 1) % mp_ncpus;
	sc->sc_decrypt_last_cpu = cpu;
	GROUPTASK_ENQUEUE(&sc->sc_decrypt[cpu]);
}

static void
wg_deliver_out(struct wg_peer *peer)
{
	struct wg_endpoint	 endpoint;
	struct wg_softc		*sc = peer->p_sc;
	struct wg_packet	*pkt;
	struct mbuf		*m;
	int			 rc, len;

	wg_peer_get_endpoint(peer, &endpoint);

	while ((pkt = wg_queue_dequeue_serial(&peer->p_encrypt_serial)) != NULL) {
		if (atomic_load_acq_int(&pkt->p_state) != WG_PACKET_CRYPTED)
			goto error;

		m = pkt->p_mbuf;
		pkt->p_mbuf = NULL;

		len = m->m_pkthdr.len;

		wg_timers_event_any_authenticated_packet_traversal(peer);
		wg_timers_event_any_authenticated_packet_sent(peer);
		rc = wg_send(sc, &endpoint, m);
		if (rc == 0) {
			if (len > (sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN))
				wg_timers_event_data_sent(peer);
			counter_u64_add(peer->p_tx_bytes, len);
		} else if (rc == EADDRNOTAVAIL) {
			wg_peer_clear_src(peer);
			wg_peer_get_endpoint(peer, &endpoint);
			goto error;
		} else {
			goto error;
		}
		wg_packet_free(pkt);
		if (noise_keep_key_fresh_send(peer->p_remote))
			wg_timers_event_want_initiation(peer);
		continue;
error:
		if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
		wg_packet_free(pkt);
	}
}

static void
wg_deliver_in(struct wg_peer *peer)
{
	struct wg_softc		*sc = peer->p_sc;
	struct ifnet		*ifp = sc->sc_ifp;
	struct wg_packet	*pkt;
	struct mbuf		*m;
	struct epoch_tracker	 et;

	while ((pkt = wg_queue_dequeue_serial(&peer->p_decrypt_serial)) != NULL) {
		if (atomic_load_acq_int(&pkt->p_state) != WG_PACKET_CRYPTED)
			goto error;

		m = pkt->p_mbuf;
		if (noise_keypair_nonce_check(pkt->p_keypair, pkt->p_nonce) != 0)
			goto error;

		if (noise_keypair_received_with(pkt->p_keypair) == ECONNRESET)
			wg_timers_event_handshake_complete(peer);

		wg_timers_event_any_authenticated_packet_received(peer);
		wg_timers_event_any_authenticated_packet_traversal(peer);
		wg_peer_set_endpoint(peer, &pkt->p_endpoint);

		counter_u64_add(peer->p_rx_bytes, m->m_pkthdr.len +
		    sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len +
		    sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN);

		if (m->m_pkthdr.len == 0)
			goto done;

		MPASS(pkt->p_af == AF_INET || pkt->p_af == AF_INET6);
		pkt->p_mbuf = NULL;

		m->m_pkthdr.rcvif = ifp;

		NET_EPOCH_ENTER(et);
		BPF_MTAP2_AF(ifp, m, pkt->p_af);

		CURVNET_SET(ifp->if_vnet);
		M_SETFIB(m, ifp->if_fib);
		if (pkt->p_af == AF_INET)
			netisr_dispatch(NETISR_IP, m);
		if (pkt->p_af == AF_INET6)
			netisr_dispatch(NETISR_IPV6, m);
		CURVNET_RESTORE();
		NET_EPOCH_EXIT(et);

		wg_timers_event_data_received(peer);

done:
		if (noise_keep_key_fresh_recv(peer->p_remote))
			wg_timers_event_want_initiation(peer);
		wg_packet_free(pkt);
		continue;
error:
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		wg_packet_free(pkt);
	}
}

static struct wg_packet *
wg_packet_alloc(struct mbuf *m)
{
	struct wg_packet *pkt;

	if ((pkt = uma_zalloc(wg_packet_zone, M_NOWAIT | M_ZERO)) == NULL)
		return (NULL);
	pkt->p_mbuf = m;
	return (pkt);
}

static void
wg_packet_free(struct wg_packet *pkt)
{
	if (pkt->p_keypair != NULL)
		noise_keypair_put(pkt->p_keypair);
	if (pkt->p_mbuf != NULL)
		m_freem(pkt->p_mbuf);
	uma_zfree(wg_packet_zone, pkt);
}

static void
wg_queue_init(struct wg_queue *queue, const char *name)
{
	mtx_init(&queue->q_mtx, name, NULL, MTX_DEF);
	STAILQ_INIT(&queue->q_queue);
	queue->q_len = 0;
}

static void
wg_queue_deinit(struct wg_queue *queue)
{
	wg_queue_purge(queue);
	mtx_destroy(&queue->q_mtx);
}

static size_t
wg_queue_len(struct wg_queue *queue)
{
	return (queue->q_len);
}

static int
wg_queue_enqueue_handshake(struct wg_queue *hs, struct wg_packet *pkt)
{
	int ret = 0;
	mtx_lock(&hs->q_mtx);
	if (hs->q_len < MAX_QUEUED_HANDSHAKES) {
		STAILQ_INSERT_TAIL(&hs->q_queue, pkt, p_parallel);
		hs->q_len++;
	} else {
		ret = ENOBUFS;
	}
	mtx_unlock(&hs->q_mtx);
	if (ret != 0)
		wg_packet_free(pkt);
	return (ret);
}

static struct wg_packet *
wg_queue_dequeue_handshake(struct wg_queue *hs)
{
	struct wg_packet *pkt;
	mtx_lock(&hs->q_mtx);
	if ((pkt = STAILQ_FIRST(&hs->q_queue)) != NULL) {
		STAILQ_REMOVE_HEAD(&hs->q_queue, p_parallel);
		hs->q_len--;
	}
	mtx_unlock(&hs->q_mtx);
	return (pkt);
}

static void
wg_queue_push_staged(struct wg_queue *staged, struct wg_packet *pkt)
{
	struct wg_packet *old = NULL;

	mtx_lock(&staged->q_mtx);
	if (staged->q_len >= MAX_STAGED_PKT) {
		old = STAILQ_FIRST(&staged->q_queue);
		STAILQ_REMOVE_HEAD(&staged->q_queue, p_parallel);
		staged->q_len--;
	}
	STAILQ_INSERT_TAIL(&staged->q_queue, pkt, p_parallel);
	staged->q_len++;
	mtx_unlock(&staged->q_mtx);

	if (old != NULL)
		wg_packet_free(old);
}

static void
wg_queue_enlist_staged(struct wg_queue *staged, struct wg_packet_list *list)
{
	struct wg_packet *pkt, *tpkt;
	STAILQ_FOREACH_SAFE(pkt, list, p_parallel, tpkt)
		wg_queue_push_staged(staged, pkt);
}

static void
wg_queue_delist_staged(struct wg_queue *staged, struct wg_packet_list *list)
{
	STAILQ_INIT(list);
	mtx_lock(&staged->q_mtx);
	STAILQ_CONCAT(list, &staged->q_queue);
	staged->q_len = 0;
	mtx_unlock(&staged->q_mtx);
}

static void
wg_queue_purge(struct wg_queue *staged)
{
	struct wg_packet_list list;
	struct wg_packet *pkt, *tpkt;
	wg_queue_delist_staged(staged, &list);
	STAILQ_FOREACH_SAFE(pkt, &list, p_parallel, tpkt)
		wg_packet_free(pkt);
}

static int
wg_queue_both(struct wg_queue *parallel, struct wg_queue *serial, struct wg_packet *pkt)
{
	pkt->p_state = WG_PACKET_UNCRYPTED;

	mtx_lock(&serial->q_mtx);
	if (serial->q_len < MAX_QUEUED_PKT) {
		serial->q_len++;
		STAILQ_INSERT_TAIL(&serial->q_queue, pkt, p_serial);
	} else {
		mtx_unlock(&serial->q_mtx);
		wg_packet_free(pkt);
		return (ENOBUFS);
	}
	mtx_unlock(&serial->q_mtx);

	mtx_lock(&parallel->q_mtx);
	if (parallel->q_len < MAX_QUEUED_PKT) {
		parallel->q_len++;
		STAILQ_INSERT_TAIL(&parallel->q_queue, pkt, p_parallel);
	} else {
		mtx_unlock(&parallel->q_mtx);
		pkt->p_state = WG_PACKET_DEAD;
		return (ENOBUFS);
	}
	mtx_unlock(&parallel->q_mtx);

	return (0);
}

static struct wg_packet *
wg_queue_dequeue_serial(struct wg_queue *serial)
{
	struct wg_packet *pkt = NULL;
	mtx_lock(&serial->q_mtx);
	if (serial->q_len > 0 && STAILQ_FIRST(&serial->q_queue)->p_state != WG_PACKET_UNCRYPTED) {
		serial->q_len--;
		pkt = STAILQ_FIRST(&serial->q_queue);
		STAILQ_REMOVE_HEAD(&serial->q_queue, p_serial);
	}
	mtx_unlock(&serial->q_mtx);
	return (pkt);
}

static struct wg_packet *
wg_queue_dequeue_parallel(struct wg_queue *parallel)
{
	struct wg_packet *pkt = NULL;
	mtx_lock(&parallel->q_mtx);
	if (parallel->q_len > 0) {
		parallel->q_len--;
		pkt = STAILQ_FIRST(&parallel->q_queue);
		STAILQ_REMOVE_HEAD(&parallel->q_queue, p_parallel);
	}
	mtx_unlock(&parallel->q_mtx);
	return (pkt);
}

static void
wg_input(struct mbuf *m, int offset, struct inpcb *inpcb,
    const struct sockaddr *sa, void *_sc)
{
#ifdef INET
	const struct sockaddr_in	*sin;
#endif
#ifdef INET6
	const struct sockaddr_in6	*sin6;
#endif
	struct noise_remote		*remote;
	struct wg_pkt_data		*data;
	struct wg_packet		*pkt;
	struct wg_peer			*peer;
	struct wg_softc			*sc = _sc;
	struct mbuf			*defragged;

	defragged = m_defrag(m, M_NOWAIT);
	if (defragged)
		m = defragged;
	m = m_unshare(m, M_NOWAIT);
	if (!m) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
		return;
	}

	/* Caller provided us with `sa`, no need for this header. */
	m_adj(m, offset + sizeof(struct udphdr));

	/* Pullup enough to read packet type */
	if ((m = m_pullup(m, sizeof(uint32_t))) == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
		return;
	}

	if ((pkt = wg_packet_alloc(m)) == NULL) {
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
		m_freem(m);
		return;
	}

	/* Save send/recv address and port for later. */
	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		sin = (const struct sockaddr_in *)sa;
		pkt->p_endpoint.e_remote.r_sin = sin[0];
		pkt->p_endpoint.e_local.l_in = sin[1].sin_addr;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (const struct sockaddr_in6 *)sa;
		pkt->p_endpoint.e_remote.r_sin6 = sin6[0];
		pkt->p_endpoint.e_local.l_in6 = sin6[1].sin6_addr;
		break;
#endif
	default:
		goto error;
	}

	if ((m->m_pkthdr.len == sizeof(struct wg_pkt_initiation) &&
		*mtod(m, uint32_t *) == WG_PKT_INITIATION) ||
	    (m->m_pkthdr.len == sizeof(struct wg_pkt_response) &&
		*mtod(m, uint32_t *) == WG_PKT_RESPONSE) ||
	    (m->m_pkthdr.len == sizeof(struct wg_pkt_cookie) &&
		*mtod(m, uint32_t *) == WG_PKT_COOKIE)) {

		if (wg_queue_enqueue_handshake(&sc->sc_handshake_queue, pkt) != 0) {
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
			DPRINTF(sc, "Dropping handshake packet\n");
		}
		GROUPTASK_ENQUEUE(&sc->sc_handshake);
	} else if (m->m_pkthdr.len >= sizeof(struct wg_pkt_data) +
	    NOISE_AUTHTAG_LEN && *mtod(m, uint32_t *) == WG_PKT_DATA) {

		/* Pullup whole header to read r_idx below. */
		if ((pkt->p_mbuf = m_pullup(m, sizeof(struct wg_pkt_data))) == NULL)
			goto error;

		data = mtod(pkt->p_mbuf, struct wg_pkt_data *);
		if ((pkt->p_keypair = noise_keypair_lookup(sc->sc_local, data->r_idx)) == NULL)
			goto error;

		remote = noise_keypair_remote(pkt->p_keypair);
		peer = noise_remote_arg(remote);
		if (wg_queue_both(&sc->sc_decrypt_parallel, &peer->p_decrypt_serial, pkt) != 0)
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
		wg_decrypt_dispatch(sc);
		noise_remote_put(remote);
	} else {
		goto error;
	}
	return;
error:
	if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);
	wg_packet_free(pkt);
}

static void
wg_peer_send_staged(struct wg_peer *peer)
{
	struct wg_packet_list	 list;
	struct noise_keypair	*keypair;
	struct wg_packet	*pkt, *tpkt;
	struct wg_softc		*sc = peer->p_sc;

	wg_queue_delist_staged(&peer->p_stage_queue, &list);

	if (STAILQ_EMPTY(&list))
		return;

	if ((keypair = noise_keypair_current(peer->p_remote)) == NULL)
		goto error;

	STAILQ_FOREACH(pkt, &list, p_parallel) {
		if (noise_keypair_nonce_next(keypair, &pkt->p_nonce) != 0)
			goto error_keypair;
	}
	STAILQ_FOREACH_SAFE(pkt, &list, p_parallel, tpkt) {
		pkt->p_keypair = noise_keypair_ref(keypair);
		if (wg_queue_both(&sc->sc_encrypt_parallel, &peer->p_encrypt_serial, pkt) != 0)
			if_inc_counter(sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
	}
	wg_encrypt_dispatch(sc);
	noise_keypair_put(keypair);
	return;

error_keypair:
	noise_keypair_put(keypair);
error:
	wg_queue_enlist_staged(&peer->p_stage_queue, &list);
	wg_timers_event_want_initiation(peer);
}

static inline void
xmit_err(struct ifnet *ifp, struct mbuf *m, struct wg_packet *pkt, sa_family_t af)
{
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	switch (af) {
#ifdef INET
	case AF_INET:
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, 0);
		if (pkt)
			pkt->p_mbuf = NULL;
		m = NULL;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		icmp6_error(m, ICMP6_DST_UNREACH, 0, 0);
		if (pkt)
			pkt->p_mbuf = NULL;
		m = NULL;
		break;
#endif
	}
	if (pkt)
		wg_packet_free(pkt);
	else if (m)
		m_freem(m);
}

static int
wg_xmit(struct ifnet *ifp, struct mbuf *m, sa_family_t af, uint32_t mtu)
{
	struct wg_packet	*pkt = NULL;
	struct wg_softc		*sc = ifp->if_softc;
	struct wg_peer		*peer;
	int			 rc = 0;
	sa_family_t		 peer_af;

	/* Work around lifetime issue in the ipv6 mld code. */
	if (__predict_false((ifp->if_flags & IFF_DYING) || !sc)) {
		rc = ENXIO;
		goto err_xmit;
	}

	if ((pkt = wg_packet_alloc(m)) == NULL) {
		rc = ENOBUFS;
		goto err_xmit;
	}
	pkt->p_mtu = mtu;
	pkt->p_af = af;

	if (af == AF_INET) {
		peer = wg_aip_lookup(sc, AF_INET, &mtod(m, struct ip *)->ip_dst);
	} else if (af == AF_INET6) {
		peer = wg_aip_lookup(sc, AF_INET6, &mtod(m, struct ip6_hdr *)->ip6_dst);
	} else {
		rc = EAFNOSUPPORT;
		goto err_xmit;
	}

	BPF_MTAP2_AF(ifp, m, pkt->p_af);

	if (__predict_false(peer == NULL)) {
		rc = ENOKEY;
		goto err_xmit;
	}

	if (__predict_false(if_tunnel_check_nesting(ifp, m, MTAG_WGLOOP, MAX_LOOPS))) {
		DPRINTF(sc, "Packet looped");
		rc = ELOOP;
		goto err_peer;
	}

	peer_af = peer->p_endpoint.e_remote.r_sa.sa_family;
	if (__predict_false(peer_af != AF_INET && peer_af != AF_INET6)) {
		DPRINTF(sc, "No valid endpoint has been configured or "
			    "discovered for peer %" PRIu64 "\n", peer->p_id);
		rc = EHOSTUNREACH;
		goto err_peer;
	}

	wg_queue_push_staged(&peer->p_stage_queue, pkt);
	wg_peer_send_staged(peer);
	noise_remote_put(peer->p_remote);
	return (0);

err_peer:
	noise_remote_put(peer->p_remote);
err_xmit:
	xmit_err(ifp, m, pkt, af);
	return (rc);
}

static inline int
determine_af_and_pullup(struct mbuf **m, sa_family_t *af)
{
	u_char ipv;
	if ((*m)->m_pkthdr.len >= sizeof(struct ip6_hdr))
		*m = m_pullup(*m, sizeof(struct ip6_hdr));
	else if ((*m)->m_pkthdr.len >= sizeof(struct ip))
		*m = m_pullup(*m, sizeof(struct ip));
	else
		return (EAFNOSUPPORT);
	if (*m == NULL)
		return (ENOBUFS);
	ipv = mtod(*m, struct ip *)->ip_v;
	if (ipv == 4)
		*af = AF_INET;
	else if (ipv == 6 && (*m)->m_pkthdr.len >= sizeof(struct ip6_hdr))
		*af = AF_INET6;
	else
		return (EAFNOSUPPORT);
	return (0);
}

static int
wg_transmit(struct ifnet *ifp, struct mbuf *m)
{
	sa_family_t af;
	int ret;
	struct mbuf *defragged;

	defragged = m_defrag(m, M_NOWAIT);
	if (defragged)
		m = defragged;
	m = m_unshare(m, M_NOWAIT);
	if (!m) {
		xmit_err(ifp, m, NULL, AF_UNSPEC);
		return (ENOBUFS);
	}

	ret = determine_af_and_pullup(&m, &af);
	if (ret) {
		xmit_err(ifp, m, NULL, AF_UNSPEC);
		return (ret);
	}
	return (wg_xmit(ifp, m, af, ifp->if_mtu));
}

static int
wg_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst, struct route *ro)
{
	sa_family_t parsed_af;
	uint32_t af, mtu;
	int ret;
	struct mbuf *defragged;

	if (dst->sa_family == AF_UNSPEC)
		memcpy(&af, dst->sa_data, sizeof(af));
	else
		af = dst->sa_family;
	if (af == AF_UNSPEC) {
		xmit_err(ifp, m, NULL, af);
		return (EAFNOSUPPORT);
	}

	defragged = m_defrag(m, M_NOWAIT);
	if (defragged)
		m = defragged;
	m = m_unshare(m, M_NOWAIT);
	if (!m) {
		xmit_err(ifp, m, NULL, AF_UNSPEC);
		return (ENOBUFS);
	}

	ret = determine_af_and_pullup(&m, &parsed_af);
	if (ret) {
		xmit_err(ifp, m, NULL, AF_UNSPEC);
		return (ret);
	}
	if (parsed_af != af) {
		xmit_err(ifp, m, NULL, AF_UNSPEC);
		return (EAFNOSUPPORT);
	}
	mtu = (ro != NULL && ro->ro_mtu > 0) ? ro->ro_mtu : ifp->if_mtu;
	return (wg_xmit(ifp, m, parsed_af, mtu));
}

static int
wg_peer_add(struct wg_softc *sc, const nvlist_t *nvl)
{
	uint8_t			 public[WG_KEY_SIZE];
	const void *pub_key, *preshared_key = NULL;
	const struct sockaddr *endpoint;
	int err;
	size_t size;
	struct noise_remote *remote;
	struct wg_peer *peer = NULL;
	bool need_insert = false;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	if (!nvlist_exists_binary(nvl, "public-key")) {
		return (EINVAL);
	}
	pub_key = nvlist_get_binary(nvl, "public-key", &size);
	if (size != WG_KEY_SIZE) {
		return (EINVAL);
	}
	if (noise_local_keys(sc->sc_local, public, NULL) == 0 &&
	    bcmp(public, pub_key, WG_KEY_SIZE) == 0) {
		return (0); // Silently ignored; not actually a failure.
	}
	if ((remote = noise_remote_lookup(sc->sc_local, pub_key)) != NULL)
		peer = noise_remote_arg(remote);
	if (nvlist_exists_bool(nvl, "remove") &&
		nvlist_get_bool(nvl, "remove")) {
		if (remote != NULL) {
			wg_peer_destroy(peer);
			noise_remote_put(remote);
		}
		return (0);
	}
	if (nvlist_exists_bool(nvl, "replace-allowedips") &&
		nvlist_get_bool(nvl, "replace-allowedips") &&
	    peer != NULL) {

		wg_aip_remove_all(sc, peer);
	}
	if (peer == NULL) {
		peer = wg_peer_alloc(sc, pub_key);
		need_insert = true;
	}
	if (nvlist_exists_binary(nvl, "endpoint")) {
		endpoint = nvlist_get_binary(nvl, "endpoint", &size);
		if (size > sizeof(peer->p_endpoint.e_remote)) {
			err = EINVAL;
			goto out;
		}
		memcpy(&peer->p_endpoint.e_remote, endpoint, size);
	}
	if (nvlist_exists_binary(nvl, "preshared-key")) {
		preshared_key = nvlist_get_binary(nvl, "preshared-key", &size);
		if (size != WG_KEY_SIZE) {
			err = EINVAL;
			goto out;
		}
		noise_remote_set_psk(peer->p_remote, preshared_key);
	}
	if (nvlist_exists_number(nvl, "persistent-keepalive-interval")) {
		uint64_t pki = nvlist_get_number(nvl, "persistent-keepalive-interval");
		if (pki > UINT16_MAX) {
			err = EINVAL;
			goto out;
		}
		wg_timers_set_persistent_keepalive(peer, pki);
	}
	if (nvlist_exists_nvlist_array(nvl, "allowed-ips")) {
		const void *addr;
		uint64_t cidr;
		const nvlist_t * const * aipl;
		size_t allowedip_count;

		aipl = nvlist_get_nvlist_array(nvl, "allowed-ips", &allowedip_count);
		for (size_t idx = 0; idx < allowedip_count; idx++) {
			if (!nvlist_exists_number(aipl[idx], "cidr"))
				continue;
			cidr = nvlist_get_number(aipl[idx], "cidr");
			if (nvlist_exists_binary(aipl[idx], "ipv4")) {
				addr = nvlist_get_binary(aipl[idx], "ipv4", &size);
				if (addr == NULL || cidr > 32 || size != sizeof(struct in_addr)) {
					err = EINVAL;
					goto out;
				}
				if ((err = wg_aip_add(sc, peer, AF_INET, addr, cidr)) != 0)
					goto out;
			} else if (nvlist_exists_binary(aipl[idx], "ipv6")) {
				addr = nvlist_get_binary(aipl[idx], "ipv6", &size);
				if (addr == NULL || cidr > 128 || size != sizeof(struct in6_addr)) {
					err = EINVAL;
					goto out;
				}
				if ((err = wg_aip_add(sc, peer, AF_INET6, addr, cidr)) != 0)
					goto out;
			} else {
				continue;
			}
		}
	}
	if (need_insert) {
		if ((err = noise_remote_enable(peer->p_remote)) != 0)
			goto out;
		TAILQ_INSERT_TAIL(&sc->sc_peers, peer, p_entry);
		sc->sc_peers_num++;
		if (sc->sc_ifp->if_link_state == LINK_STATE_UP)
			wg_timers_enable(peer);
	}
	if (remote != NULL)
		noise_remote_put(remote);
	return (0);
out:
	if (need_insert) /* If we fail, only destroy if it was new. */
		wg_peer_destroy(peer);
	if (remote != NULL)
		noise_remote_put(remote);
	return (err);
}

static int
wgc_set(struct wg_softc *sc, struct wg_data_io *wgd)
{
	uint8_t public[WG_KEY_SIZE], private[WG_KEY_SIZE];
	struct ifnet *ifp;
	void *nvlpacked;
	nvlist_t *nvl;
	ssize_t size;
	int err;

	ifp = sc->sc_ifp;
	if (wgd->wgd_size == 0 || wgd->wgd_data == NULL)
		return (EFAULT);

	/* Can nvlists be streamed in? It's not nice to impose arbitrary limits like that but
	 * there needs to be _some_ limitation. */
	if (wgd->wgd_size >= UINT32_MAX / 2)
		return (E2BIG);

	nvlpacked = malloc(wgd->wgd_size, M_TEMP, M_WAITOK | M_ZERO);

	err = copyin(wgd->wgd_data, nvlpacked, wgd->wgd_size);
	if (err)
		goto out;
	nvl = nvlist_unpack(nvlpacked, wgd->wgd_size, 0);
	if (nvl == NULL) {
		err = EBADMSG;
		goto out;
	}
	sx_xlock(&sc->sc_lock);
	if (nvlist_exists_bool(nvl, "replace-peers") &&
		nvlist_get_bool(nvl, "replace-peers"))
		wg_peer_destroy_all(sc);
	if (nvlist_exists_number(nvl, "listen-port")) {
		uint64_t new_port = nvlist_get_number(nvl, "listen-port");
		if (new_port > UINT16_MAX) {
			err = EINVAL;
			goto out_locked;
		}
		if (new_port != sc->sc_socket.so_port) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if ((err = wg_socket_init(sc, new_port)) != 0)
					goto out_locked;
			} else
				sc->sc_socket.so_port = new_port;
		}
	}
	if (nvlist_exists_binary(nvl, "private-key")) {
		const void *key = nvlist_get_binary(nvl, "private-key", &size);
		if (size != WG_KEY_SIZE) {
			err = EINVAL;
			goto out_locked;
		}

		if (noise_local_keys(sc->sc_local, NULL, private) != 0 ||
		    timingsafe_bcmp(private, key, WG_KEY_SIZE) != 0) {
			struct wg_peer *peer;

			if (curve25519_generate_public(public, key)) {
				/* Peer conflict: remove conflicting peer. */
				struct noise_remote *remote;
				if ((remote = noise_remote_lookup(sc->sc_local,
				    public)) != NULL) {
					peer = noise_remote_arg(remote);
					wg_peer_destroy(peer);
					noise_remote_put(remote);
				}
			}

			/*
			 * Set the private key and invalidate all existing
			 * handshakes.
			 */
			/* Note: we might be removing the private key. */
			noise_local_private(sc->sc_local, key);
			if (noise_local_keys(sc->sc_local, NULL, NULL) == 0)
				cookie_checker_update(&sc->sc_cookie, public);
			else
				cookie_checker_update(&sc->sc_cookie, NULL);
		}
	}
	if (nvlist_exists_number(nvl, "user-cookie")) {
		uint64_t user_cookie = nvlist_get_number(nvl, "user-cookie");
		if (user_cookie > UINT32_MAX) {
			err = EINVAL;
			goto out_locked;
		}
		err = wg_socket_set_cookie(sc, user_cookie);
		if (err)
			goto out_locked;
	}
	if (nvlist_exists_nvlist_array(nvl, "peers")) {
		size_t peercount;
		const nvlist_t * const*nvl_peers;

		nvl_peers = nvlist_get_nvlist_array(nvl, "peers", &peercount);
		for (int i = 0; i < peercount; i++) {
			err = wg_peer_add(sc, nvl_peers[i]);
			if (err != 0)
				goto out_locked;
		}
	}

out_locked:
	sx_xunlock(&sc->sc_lock);
	nvlist_destroy(nvl);
out:
	zfree(nvlpacked, M_TEMP);
	return (err);
}

static int
wgc_get(struct wg_softc *sc, struct wg_data_io *wgd)
{
	uint8_t public_key[WG_KEY_SIZE] = { 0 };
	uint8_t private_key[WG_KEY_SIZE] = { 0 };
	uint8_t preshared_key[NOISE_SYMMETRIC_KEY_LEN] = { 0 };
	nvlist_t *nvl, *nvl_peer, *nvl_aip, **nvl_peers, **nvl_aips;
	size_t size, peer_count, aip_count, i, j;
	struct wg_timespec64 ts64;
	struct wg_peer *peer;
	struct wg_aip *aip;
	void *packed;
	int err = 0;

	nvl = nvlist_create(0);
	if (!nvl)
		return (ENOMEM);

	sx_slock(&sc->sc_lock);

	if (sc->sc_socket.so_port != 0)
		nvlist_add_number(nvl, "listen-port", sc->sc_socket.so_port);
	if (sc->sc_socket.so_user_cookie != 0)
		nvlist_add_number(nvl, "user-cookie", sc->sc_socket.so_user_cookie);
	if (noise_local_keys(sc->sc_local, public_key, private_key) == 0) {
		nvlist_add_binary(nvl, "public-key", public_key, WG_KEY_SIZE);
		if (wgc_privileged(sc))
			nvlist_add_binary(nvl, "private-key", private_key, WG_KEY_SIZE);
		explicit_bzero(private_key, sizeof(private_key));
	}
	peer_count = sc->sc_peers_num;
	if (peer_count) {
		nvl_peers = mallocarray(peer_count, sizeof(void *), M_NVLIST, M_WAITOK | M_ZERO);
		i = 0;
		TAILQ_FOREACH(peer, &sc->sc_peers, p_entry) {
			if (i >= peer_count)
				panic("peers changed from under us");

			nvl_peers[i++] = nvl_peer = nvlist_create(0);
			if (!nvl_peer) {
				err = ENOMEM;
				goto err_peer;
			}

			(void)noise_remote_keys(peer->p_remote, public_key, preshared_key);
			nvlist_add_binary(nvl_peer, "public-key", public_key, sizeof(public_key));
			if (wgc_privileged(sc))
				nvlist_add_binary(nvl_peer, "preshared-key", preshared_key, sizeof(preshared_key));
			explicit_bzero(preshared_key, sizeof(preshared_key));
			if (peer->p_endpoint.e_remote.r_sa.sa_family == AF_INET)
				nvlist_add_binary(nvl_peer, "endpoint", &peer->p_endpoint.e_remote, sizeof(struct sockaddr_in));
			else if (peer->p_endpoint.e_remote.r_sa.sa_family == AF_INET6)
				nvlist_add_binary(nvl_peer, "endpoint", &peer->p_endpoint.e_remote, sizeof(struct sockaddr_in6));
			wg_timers_get_last_handshake(peer, &ts64);
			nvlist_add_binary(nvl_peer, "last-handshake-time", &ts64, sizeof(ts64));
			nvlist_add_number(nvl_peer, "persistent-keepalive-interval", peer->p_persistent_keepalive_interval);
			nvlist_add_number(nvl_peer, "rx-bytes", counter_u64_fetch(peer->p_rx_bytes));
			nvlist_add_number(nvl_peer, "tx-bytes", counter_u64_fetch(peer->p_tx_bytes));

			aip_count = peer->p_aips_num;
			if (aip_count) {
				nvl_aips = mallocarray(aip_count, sizeof(void *), M_NVLIST, M_WAITOK | M_ZERO);
				j = 0;
				LIST_FOREACH(aip, &peer->p_aips, a_entry) {
					if (j >= aip_count)
						panic("aips changed from under us");

					nvl_aips[j++] = nvl_aip = nvlist_create(0);
					if (!nvl_aip) {
						err = ENOMEM;
						goto err_aip;
					}
					if (aip->a_af == AF_INET) {
						nvlist_add_binary(nvl_aip, "ipv4", &aip->a_addr.in, sizeof(aip->a_addr.in));
						nvlist_add_number(nvl_aip, "cidr", bitcount32(aip->a_mask.ip));
					}
#ifdef INET6
					else if (aip->a_af == AF_INET6) {
						nvlist_add_binary(nvl_aip, "ipv6", &aip->a_addr.in6, sizeof(aip->a_addr.in6));
						nvlist_add_number(nvl_aip, "cidr", in6_mask2len(&aip->a_mask.in6, NULL));
					}
#endif
				}
				nvlist_add_nvlist_array(nvl_peer, "allowed-ips", (const nvlist_t *const *)nvl_aips, aip_count);
			err_aip:
				for (j = 0; j < aip_count; ++j)
					nvlist_destroy(nvl_aips[j]);
				free(nvl_aips, M_NVLIST);
				if (err)
					goto err_peer;
			}
		}
		nvlist_add_nvlist_array(nvl, "peers", (const nvlist_t * const *)nvl_peers, peer_count);
	err_peer:
		for (i = 0; i < peer_count; ++i)
			nvlist_destroy(nvl_peers[i]);
		free(nvl_peers, M_NVLIST);
		if (err) {
			sx_sunlock(&sc->sc_lock);
			goto err;
		}
	}
	sx_sunlock(&sc->sc_lock);
	packed = nvlist_pack(nvl, &size);
	if (!packed) {
		err = ENOMEM;
		goto err;
	}
	if (!wgd->wgd_size) {
		wgd->wgd_size = size;
		goto out;
	}
	if (wgd->wgd_size < size) {
		err = ENOSPC;
		goto out;
	}
	err = copyout(packed, wgd->wgd_data, size);
	wgd->wgd_size = size;

out:
	zfree(packed, M_NVLIST);
err:
	nvlist_destroy(nvl);
	return (err);
}

static int
wg_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wg_data_io *wgd = (struct wg_data_io *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wg_softc *sc;
	int ret = 0;

	sx_slock(&wg_sx);
	sc = ifp->if_softc;
	if (!sc) {
		ret = ENXIO;
		goto out;
	}

	switch (cmd) {
	case SIOCSWG:
		ret = priv_check(curthread, PRIV_NET_WG);
		if (ret == 0)
			ret = wgc_set(sc, wgd);
		break;
	case SIOCGWG:
		ret = wgc_get(sc, wgd);
		break;
	/* Interface IOCTLs */
	case SIOCSIFADDR:
		/*
		 * This differs from *BSD norms, but is more uniform with how
		 * WireGuard behaves elsewhere.
		 */
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ret = wg_up(sc);
		else
			wg_down(sc);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu <= 0 || ifr->ifr_mtu > MAX_MTU)
			ret = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	case SIOCGTUNFIB:
		ifr->ifr_fib = sc->sc_socket.so_fibnum;
		break;
	case SIOCSTUNFIB:
		ret = priv_check(curthread, PRIV_NET_WG);
		if (ret)
			break;
		ret = priv_check(curthread, PRIV_NET_SETIFFIB);
		if (ret)
			break;
		sx_xlock(&sc->sc_lock);
		ret = wg_socket_set_fibnum(sc, ifr->ifr_fib);
		sx_xunlock(&sc->sc_lock);
		break;
	default:
		ret = ENOTTY;
	}

out:
	sx_sunlock(&wg_sx);
	return (ret);
}

static int
wg_up(struct wg_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct wg_peer *peer;
	int rc = EBUSY;

	sx_xlock(&sc->sc_lock);
	/* Jail's being removed, no more wg_up(). */
	if ((sc->sc_flags & WGF_DYING) != 0)
		goto out;

	/* Silent success if we're already running. */
	rc = 0;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		goto out;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	rc = wg_socket_init(sc, sc->sc_socket.so_port);
	if (rc == 0) {
		TAILQ_FOREACH(peer, &sc->sc_peers, p_entry)
			wg_timers_enable(peer);
		if_link_state_change(sc->sc_ifp, LINK_STATE_UP);
	} else {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		DPRINTF(sc, "Unable to initialize sockets: %d\n", rc);
	}
out:
	sx_xunlock(&sc->sc_lock);
	return (rc);
}

static void
wg_down(struct wg_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct wg_peer *peer;

	sx_xlock(&sc->sc_lock);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		sx_xunlock(&sc->sc_lock);
		return;
	}
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	TAILQ_FOREACH(peer, &sc->sc_peers, p_entry) {
		wg_queue_purge(&peer->p_stage_queue);
		wg_timers_disable(peer);
	}

	wg_queue_purge(&sc->sc_handshake_queue);

	TAILQ_FOREACH(peer, &sc->sc_peers, p_entry) {
		noise_remote_handshake_clear(peer->p_remote);
		noise_remote_keypairs_clear(peer->p_remote);
	}

	if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
	wg_socket_uninit(sc);

	sx_xunlock(&sc->sc_lock);
}

static int
wg_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct wg_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_WG, M_WAITOK | M_ZERO);

	sc->sc_local = noise_local_alloc(sc);

	sc->sc_encrypt = mallocarray(sizeof(struct grouptask), mp_ncpus, M_WG, M_WAITOK | M_ZERO);

	sc->sc_decrypt = mallocarray(sizeof(struct grouptask), mp_ncpus, M_WG, M_WAITOK | M_ZERO);

	if (!rn_inithead((void **)&sc->sc_aip4, offsetof(struct aip_addr, in) * NBBY))
		goto free_decrypt;

	if (!rn_inithead((void **)&sc->sc_aip6, offsetof(struct aip_addr, in6) * NBBY))
		goto free_aip4;

	atomic_add_int(&clone_count, 1);
	ifp = sc->sc_ifp = if_alloc(IFT_WIREGUARD);

	sc->sc_ucred = crhold(curthread->td_ucred);
	sc->sc_socket.so_fibnum = curthread->td_proc->p_fibnum;
	sc->sc_socket.so_port = 0;

	TAILQ_INIT(&sc->sc_peers);
	sc->sc_peers_num = 0;

	cookie_checker_init(&sc->sc_cookie);

	RADIX_NODE_HEAD_LOCK_INIT(sc->sc_aip4);
	RADIX_NODE_HEAD_LOCK_INIT(sc->sc_aip6);

	GROUPTASK_INIT(&sc->sc_handshake, 0, (gtask_fn_t *)wg_softc_handshake_receive, sc);
	taskqgroup_attach(qgroup_wg_tqg, &sc->sc_handshake, sc, NULL, NULL, "wg tx initiation");
	wg_queue_init(&sc->sc_handshake_queue, "hsq");

	for (int i = 0; i < mp_ncpus; i++) {
		GROUPTASK_INIT(&sc->sc_encrypt[i], 0,
		     (gtask_fn_t *)wg_softc_encrypt, sc);
		taskqgroup_attach_cpu(qgroup_wg_tqg, &sc->sc_encrypt[i], sc, i, NULL, NULL, "wg encrypt");
		GROUPTASK_INIT(&sc->sc_decrypt[i], 0,
		    (gtask_fn_t *)wg_softc_decrypt, sc);
		taskqgroup_attach_cpu(qgroup_wg_tqg, &sc->sc_decrypt[i], sc, i, NULL, NULL, "wg decrypt");
	}

	wg_queue_init(&sc->sc_encrypt_parallel, "encp");
	wg_queue_init(&sc->sc_decrypt_parallel, "decp");

	sx_init(&sc->sc_lock, "wg softc lock");

	ifp->if_softc = sc;
	ifp->if_capabilities = ifp->if_capenable = WG_CAPS;
	if_initname(ifp, wgname, ifd->unit);

	if_setmtu(ifp, DEFAULT_MTU);
	ifp->if_flags = IFF_NOARP | IFF_MULTICAST;
	ifp->if_init = wg_init;
	ifp->if_reassign = wg_reassign;
	ifp->if_qflush = wg_qflush;
	ifp->if_transmit = wg_transmit;
	ifp->if_output = wg_output;
	ifp->if_ioctl = wg_ioctl;
	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(uint32_t));
#ifdef INET6
	ND_IFINFO(ifp)->flags &= ~ND6_IFF_AUTO_LINKLOCAL;
	ND_IFINFO(ifp)->flags |= ND6_IFF_NO_DAD;
#endif
	sx_xlock(&wg_sx);
	LIST_INSERT_HEAD(&wg_list, sc, sc_entry);
	sx_xunlock(&wg_sx);
	*ifpp = ifp;
	return (0);
free_aip4:
	RADIX_NODE_HEAD_DESTROY(sc->sc_aip4);
	free(sc->sc_aip4, M_RTABLE);
free_decrypt:
	free(sc->sc_decrypt, M_WG);
	free(sc->sc_encrypt, M_WG);
	noise_local_free(sc->sc_local, NULL);
	free(sc, M_WG);
	return (ENOMEM);
}

static void
wg_clone_deferred_free(struct noise_local *l)
{
	struct wg_softc *sc = noise_local_arg(l);

	free(sc, M_WG);
	atomic_add_int(&clone_count, -1);
}

static int
wg_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct wg_softc *sc = ifp->if_softc;
	struct ucred *cred;

	sx_xlock(&wg_sx);
	ifp->if_softc = NULL;
	sx_xlock(&sc->sc_lock);
	sc->sc_flags |= WGF_DYING;
	cred = sc->sc_ucred;
	sc->sc_ucred = NULL;
	sx_xunlock(&sc->sc_lock);
	LIST_REMOVE(sc, sc_entry);
	sx_xunlock(&wg_sx);

	if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
	CURVNET_SET(sc->sc_ifp->if_vnet);
	if_purgeaddrs(sc->sc_ifp);
	CURVNET_RESTORE();

	sx_xlock(&sc->sc_lock);
	wg_socket_uninit(sc);
	sx_xunlock(&sc->sc_lock);

	/*
	 * No guarantees that all traffic have passed until the epoch has
	 * elapsed with the socket closed.
	 */
	NET_EPOCH_WAIT();

	taskqgroup_drain_all(qgroup_wg_tqg);
	sx_xlock(&sc->sc_lock);
	wg_peer_destroy_all(sc);
	NET_EPOCH_DRAIN_CALLBACKS();
	sx_xunlock(&sc->sc_lock);
	sx_destroy(&sc->sc_lock);
	taskqgroup_detach(qgroup_wg_tqg, &sc->sc_handshake);
	for (int i = 0; i < mp_ncpus; i++) {
		taskqgroup_detach(qgroup_wg_tqg, &sc->sc_encrypt[i]);
		taskqgroup_detach(qgroup_wg_tqg, &sc->sc_decrypt[i]);
	}
	free(sc->sc_encrypt, M_WG);
	free(sc->sc_decrypt, M_WG);
	wg_queue_deinit(&sc->sc_handshake_queue);
	wg_queue_deinit(&sc->sc_encrypt_parallel);
	wg_queue_deinit(&sc->sc_decrypt_parallel);

	RADIX_NODE_HEAD_DESTROY(sc->sc_aip4);
	RADIX_NODE_HEAD_DESTROY(sc->sc_aip6);
	rn_detachhead((void **)&sc->sc_aip4);
	rn_detachhead((void **)&sc->sc_aip6);

	cookie_checker_free(&sc->sc_cookie);

	if (cred != NULL)
		crfree(cred);
	bpfdetach(sc->sc_ifp);
	if_detach(sc->sc_ifp);
	if_free(sc->sc_ifp);

	noise_local_free(sc->sc_local, wg_clone_deferred_free);

	return (0);
}

static void
wg_qflush(struct ifnet *ifp __unused)
{
}

/*
 * Privileged information (private-key, preshared-key) are only exported for
 * root and jailed root by default.
 */
static bool
wgc_privileged(struct wg_softc *sc)
{
	struct thread *td;

	td = curthread;
	return (priv_check(td, PRIV_NET_WG) == 0);
}

static void
wg_reassign(struct ifnet *ifp, struct vnet *new_vnet __unused,
    char *unused __unused)
{
	struct wg_softc *sc;

	sc = ifp->if_softc;
	wg_down(sc);
}

static void
wg_init(void *xsc)
{
	struct wg_softc *sc;

	sc = xsc;
	wg_up(sc);
}

static void
vnet_wg_init(const void *unused __unused)
{
	struct if_clone_addreq req = {
		.create_f = wg_clone_create,
		.destroy_f = wg_clone_destroy,
		.flags = IFC_F_AUTOUNIT,
	};
	V_wg_cloner = ifc_attach_cloner(wgname, &req);
}
VNET_SYSINIT(vnet_wg_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
	     vnet_wg_init, NULL);

static void
vnet_wg_uninit(const void *unused __unused)
{
	if (V_wg_cloner)
		ifc_detach_cloner(V_wg_cloner);
}
VNET_SYSUNINIT(vnet_wg_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
	       vnet_wg_uninit, NULL);

static int
wg_prison_remove(void *obj, void *data __unused)
{
	const struct prison *pr = obj;
	struct wg_softc *sc;

	/*
	 * Do a pass through all if_wg interfaces and release creds on any from
	 * the jail that are supposed to be going away.  This will, in turn, let
	 * the jail die so that we don't end up with Schrödinger's jail.
	 */
	sx_slock(&wg_sx);
	LIST_FOREACH(sc, &wg_list, sc_entry) {
		sx_xlock(&sc->sc_lock);
		if (!(sc->sc_flags & WGF_DYING) && sc->sc_ucred && sc->sc_ucred->cr_prison == pr) {
			struct ucred *cred = sc->sc_ucred;
			DPRINTF(sc, "Creating jail exiting\n");
			if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
			wg_socket_uninit(sc);
			sc->sc_ucred = NULL;
			crfree(cred);
			sc->sc_flags |= WGF_DYING;
		}
		sx_xunlock(&sc->sc_lock);
	}
	sx_sunlock(&wg_sx);

	return (0);
}

#ifdef SELFTESTS
#include "selftest/allowedips.c"
static bool wg_run_selftests(void)
{
	bool ret = true;
	ret &= wg_allowedips_selftest();
	ret &= noise_counter_selftest();
	ret &= cookie_selftest();
	return ret;
}
#else
static inline bool wg_run_selftests(void) { return true; }
#endif

static int
wg_module_init(void)
{
	int ret;
	osd_method_t methods[PR_MAXMETHOD] = {
		[PR_METHOD_REMOVE] = wg_prison_remove,
	};

	if ((wg_packet_zone = uma_zcreate("wg packet", sizeof(struct wg_packet),
	     NULL, NULL, NULL, NULL, 0, 0)) == NULL)
		return (ENOMEM);
	ret = crypto_init();
	if (ret != 0)
		return (ret);
	ret = cookie_init();
	if (ret != 0)
		return (ret);

	wg_osd_jail_slot = osd_jail_register(NULL, methods);

	if (!wg_run_selftests())
		return (ENOTRECOVERABLE);

	return (0);
}

static void
wg_module_deinit(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	VNET_LIST_RLOCK();
	VNET_FOREACH(vnet_iter) {
		struct if_clone *clone = VNET_VNET(vnet_iter, wg_cloner);
		if (clone) {
			ifc_detach_cloner(clone);
			VNET_VNET(vnet_iter, wg_cloner) = NULL;
		}
	}
	VNET_LIST_RUNLOCK();
	NET_EPOCH_WAIT();
	MPASS(LIST_EMPTY(&wg_list));
	if (wg_osd_jail_slot != 0)
		osd_jail_deregister(wg_osd_jail_slot);
	cookie_deinit();
	crypto_deinit();
	if (wg_packet_zone != NULL)
		uma_zdestroy(wg_packet_zone);
}

static int
wg_module_event_handler(module_t mod, int what, void *arg)
{
	switch (what) {
		case MOD_LOAD:
			return wg_module_init();
		case MOD_UNLOAD:
			wg_module_deinit();
			break;
		default:
			return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t wg_moduledata = {
	wgname,
	wg_module_event_handler,
	NULL
};

DECLARE_MODULE(wg, wg_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(wg, WIREGUARD_VERSION);
MODULE_DEPEND(wg, crypto, 1, 1, 1);
