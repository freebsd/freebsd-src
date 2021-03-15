/*
 * Copyright (C) 2015-2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2019-2021 Matt Dunwoodie <ncon@noconroy.net>
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2021 Kyle Evans <kevans@FreeBSD.org>
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

/* TODO audit imports */
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
#include <sys/jail.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/rmlock.h>
#include <sys/protosw.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/kdb.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/gtaskqueue.h>
#include <sys/smp.h>
#include <sys/nv.h>

#include <net/bpf.h>

#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/radix.h>

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

#include <machine/in_cksum.h>

#include "support.h"
#include "wg_noise.h"
#include "wg_cookie.h"
#include "if_wg.h"

/* It'd be nice to use IF_MAXMTU, but that means more complicated mbuf allocations,
 * so instead just do the biggest mbuf we can easily allocate minus the usual maximum
 * IPv6 overhead of 80 bytes. If somebody wants bigger frames, we can revisit this. */
#define	MAX_MTU			(MJUM16BYTES - 80)

#define	DEFAULT_MTU		1420

#define MAX_STAGED_PKT		128
#define MAX_QUEUED_PKT		1024
#define MAX_QUEUED_PKT_MASK	(MAX_QUEUED_PKT - 1)

#define MAX_QUEUED_HANDSHAKES	4096

#define HASHTABLE_PEER_SIZE	(1 << 11)
#define HASHTABLE_INDEX_SIZE	(1 << 13)
#define MAX_PEERS_PER_IFACE	(1 << 20)

#define REKEY_TIMEOUT		5
#define REKEY_TIMEOUT_JITTER	334 /* 1/3 sec, round for arc4random_uniform */
#define KEEPALIVE_TIMEOUT	10
#define MAX_TIMER_HANDSHAKES	(90 / REKEY_TIMEOUT)
#define NEW_HANDSHAKE_TIMEOUT	(REKEY_TIMEOUT + KEEPALIVE_TIMEOUT)
#define UNDERLOAD_TIMEOUT	1

#define DPRINTF(sc,  ...) if (wireguard_debug) if_printf(sc->sc_ifp, ##__VA_ARGS__)

/* First byte indicating packet type on the wire */
#define WG_PKT_INITIATION htole32(1)
#define WG_PKT_RESPONSE htole32(2)
#define WG_PKT_COOKIE htole32(3)
#define WG_PKT_DATA htole32(4)

#define WG_PKT_WITH_PADDING(n)	(((n) + (16-1)) & (~(16-1)))
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
	uint8_t			nonce[sizeof(uint64_t)];
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

struct wg_tag {
	struct m_tag		 t_tag;
	struct wg_endpoint	 t_endpoint;
	struct wg_peer		*t_peer;
	struct mbuf		*t_mbuf;
	int			 t_done;
	int			 t_mtu;
};

struct wg_index {
	LIST_ENTRY(wg_index)	 i_entry;
	SLIST_ENTRY(wg_index)	 i_unused_entry;
	uint32_t		 i_key;
	struct noise_remote	*i_value;
};

struct wg_timers {
	/* t_lock is for blocking wg_timers_event_* when setting t_disabled. */
	struct rwlock		 t_lock;

	int			 t_disabled;
	int			 t_need_another_keepalive;
	uint16_t		 t_persistent_keepalive_interval;
	struct callout		 t_new_handshake;
	struct callout		 t_send_keepalive;
	struct callout		 t_retry_handshake;
	struct callout		 t_zero_key_material;
	struct callout		 t_persistent_keepalive;

	struct mtx		 t_handshake_mtx;
	struct timespec		 t_handshake_last_sent;
	struct timespec		 t_handshake_complete;
	volatile int		 t_handshake_retries;
};

struct wg_aip {
	struct radix_node	 r_nodes[2];
	CK_LIST_ENTRY(wg_aip)	 r_entry;
	struct sockaddr_storage	 r_addr;
	struct sockaddr_storage	 r_mask;
	struct wg_peer		*r_peer;
};

struct wg_queue {
	struct mtx	q_mtx;
	struct mbufq	q;
};

struct wg_peer {
	CK_LIST_ENTRY(wg_peer)		 p_hash_entry;
	CK_LIST_ENTRY(wg_peer)		 p_entry;
	uint64_t			 p_id;
	struct wg_softc			*p_sc;

	struct noise_remote		 p_remote;
	struct cookie_maker		 p_cookie;
	struct wg_timers		 p_timers;

	struct rwlock			 p_endpoint_lock;
	struct wg_endpoint		 p_endpoint;

	SLIST_HEAD(,wg_index)		 p_unused_index;
	struct wg_index			 p_index[3];

	struct wg_queue	 		 p_stage_queue;
	struct wg_queue	 		 p_encap_queue;
	struct wg_queue	 		 p_decap_queue;

	struct grouptask		 p_clear_secrets;
	struct grouptask		 p_send_initiation;
	struct grouptask		 p_send_keepalive;
	struct grouptask		 p_send;
	struct grouptask		 p_recv;

	counter_u64_t			 p_tx_bytes;
	counter_u64_t			 p_rx_bytes;

	CK_LIST_HEAD(, wg_aip)		 p_aips;
	struct mtx			 p_lock;
	struct epoch_context		 p_ctx;
};

enum route_direction {
	/* TODO OpenBSD doesn't use IN/OUT, instead passes the address buffer
	 * directly to route_lookup. */
	IN,
	OUT,
};

struct wg_aip_table {
	size_t 			 t_count;
	struct radix_node_head	*t_ip;
	struct radix_node_head	*t_ip6;
};

struct wg_allowedip {
	uint16_t family;
	union {
		struct in_addr ip4;
		struct in6_addr ip6;
	};
	uint8_t cidr;
};

struct wg_hashtable {
	struct mtx			 h_mtx;
	SIPHASH_KEY			 h_secret;
	CK_LIST_HEAD(, wg_peer)		 h_peers_list;
	CK_LIST_HEAD(, wg_peer)		*h_peers;
	u_long				 h_peers_mask;
	size_t				 h_num_peers;
};

struct wg_socket {
	struct mtx	 so_mtx;
	struct socket	*so_so4;
	struct socket	*so_so6;
	uint32_t	 so_user_cookie;
	in_port_t	 so_port;
};

struct wg_softc {
	LIST_ENTRY(wg_softc)	 sc_entry;
	struct ifnet		*sc_ifp;
	int			 sc_flags;

	struct ucred		*sc_ucred;
	struct wg_socket	 sc_socket;
	struct wg_hashtable	 sc_hashtable;
	struct wg_aip_table	 sc_aips;

	struct mbufq		 sc_handshake_queue;
	struct grouptask	 sc_handshake;

	struct noise_local	 sc_local;
	struct cookie_checker	 sc_cookie;

	struct buf_ring		*sc_encap_ring;
	struct buf_ring		*sc_decap_ring;

	struct grouptask	*sc_encrypt;
	struct grouptask	*sc_decrypt;

	struct rwlock		 sc_index_lock;
	LIST_HEAD(,wg_index)	*sc_index;
	u_long			 sc_index_mask;

	struct sx		 sc_lock;
	volatile u_int		 sc_peer_count;
};

#define	WGF_DYING	0x0001

/* TODO the following defines are freebsd specific, we should see what is
 * necessary and cleanup from there (i suspect a lot can be junked). */

#ifndef ENOKEY
#define	ENOKEY	ENOTCAPABLE
#endif

#if __FreeBSD_version > 1300000
typedef void timeout_t (void *);
#endif

#define	GROUPTASK_DRAIN(gtask)			\
	gtaskqueue_drain((gtask)->gt_taskqueue, &(gtask)->gt_task)

#define MTAG_WIREGUARD	0xBEAD
#define M_ENQUEUED	M_PROTO1

static int clone_count;
static uma_zone_t ratelimit_zone;
static int wireguard_debug;
static volatile unsigned long peer_counter = 0;
static const char wgname[] = "wg";
static unsigned wg_osd_jail_slot;

static struct sx wg_sx;
SX_SYSINIT(wg_sx, &wg_sx, "wg_sx");

static LIST_HEAD(, wg_softc)	wg_list = LIST_HEAD_INITIALIZER(wg_list);

SYSCTL_NODE(_net, OID_AUTO, wg, CTLFLAG_RW, 0, "WireGuard");
SYSCTL_INT(_net_wg, OID_AUTO, debug, CTLFLAG_RWTUN, &wireguard_debug, 0,
	"enable debug logging");

TASKQGROUP_DECLARE(if_io_tqg);

MALLOC_DEFINE(M_WG, "WG", "wireguard");
VNET_DEFINE_STATIC(struct if_clone *, wg_cloner);


#define	V_wg_cloner	VNET(wg_cloner)
#define	WG_CAPS		IFCAP_LINKSTATE
#define	ph_family	PH_loc.eight[5]

struct wg_timespec64 {
	uint64_t	tv_sec;
	uint64_t	tv_nsec;
};

struct wg_peer_export {
	struct sockaddr_storage		endpoint;
	struct timespec			last_handshake;
	uint8_t				public_key[WG_KEY_SIZE];
	uint8_t				preshared_key[NOISE_SYMMETRIC_KEY_LEN];
	size_t				endpoint_sz;
	struct wg_allowedip		*aip;
	uint64_t			rx_bytes;
	uint64_t			tx_bytes;
	int				aip_count;
	uint16_t			persistent_keepalive;
};

static struct wg_tag *wg_tag_get(struct mbuf *);
static struct wg_endpoint *wg_mbuf_endpoint_get(struct mbuf *);
static int wg_socket_init(struct wg_softc *, in_port_t);
static int wg_socket_bind(struct socket *, struct socket *, in_port_t *);
static void wg_socket_set(struct wg_softc *, struct socket *, struct socket *);
static void wg_socket_uninit(struct wg_softc *);
static void wg_socket_set_cookie(struct wg_softc *, uint32_t);
static int wg_send(struct wg_softc *, struct wg_endpoint *, struct mbuf *);
static void wg_timers_event_data_sent(struct wg_timers *);
static void wg_timers_event_data_received(struct wg_timers *);
static void wg_timers_event_any_authenticated_packet_sent(struct wg_timers *);
static void wg_timers_event_any_authenticated_packet_received(struct wg_timers *);
static void wg_timers_event_any_authenticated_packet_traversal(struct wg_timers *);
static void wg_timers_event_handshake_initiated(struct wg_timers *);
static void wg_timers_event_handshake_responded(struct wg_timers *);
static void wg_timers_event_handshake_complete(struct wg_timers *);
static void wg_timers_event_session_derived(struct wg_timers *);
static void wg_timers_event_want_initiation(struct wg_timers *);
static void wg_timers_event_reset_handshake_last_sent(struct wg_timers *);
static void wg_timers_run_send_initiation(struct wg_timers *, int);
static void wg_timers_run_retry_handshake(struct wg_timers *);
static void wg_timers_run_send_keepalive(struct wg_timers *);
static void wg_timers_run_new_handshake(struct wg_timers *);
static void wg_timers_run_zero_key_material(struct wg_timers *);
static void wg_timers_run_persistent_keepalive(struct wg_timers *);
static void wg_timers_init(struct wg_timers *);
static void wg_timers_enable(struct wg_timers *);
static void wg_timers_disable(struct wg_timers *);
static void wg_timers_set_persistent_keepalive(struct wg_timers *, uint16_t);
static void wg_timers_get_last_handshake(struct wg_timers *, struct timespec *);
static int wg_timers_expired_handshake_last_sent(struct wg_timers *);
static int wg_timers_check_handshake_last_sent(struct wg_timers *);
static void wg_queue_init(struct wg_queue *, const char *);
static void wg_queue_deinit(struct wg_queue *);
static void wg_queue_purge(struct wg_queue *);
static struct mbuf *wg_queue_dequeue(struct wg_queue *, struct wg_tag **);
static int wg_queue_len(struct wg_queue *);
static int wg_queue_in(struct wg_peer *, struct mbuf *);
static void wg_queue_out(struct wg_peer *);
static void wg_queue_stage(struct wg_peer *, struct mbuf *);
static int wg_aip_init(struct wg_aip_table *);
static void wg_aip_destroy(struct wg_aip_table *);
static void wg_aip_populate_aip4(struct wg_aip *, const struct in_addr *, uint8_t);
static void wg_aip_populate_aip6(struct wg_aip *, const struct in6_addr *, uint8_t);
static int wg_aip_add(struct wg_aip_table *, struct wg_peer *, const struct wg_allowedip *);
static int wg_peer_remove(struct radix_node *, void *);
static void wg_peer_remove_all(struct wg_softc *);
static int wg_aip_delete(struct wg_aip_table *, struct wg_peer *);
static struct wg_peer *wg_aip_lookup(struct wg_aip_table *, struct mbuf *, enum route_direction);
static void wg_hashtable_init(struct wg_hashtable *);
static void wg_hashtable_destroy(struct wg_hashtable *);
static void wg_hashtable_peer_insert(struct wg_hashtable *, struct wg_peer *);
static struct wg_peer *wg_peer_lookup(struct wg_softc *, const uint8_t [32]);
static void wg_hashtable_peer_remove(struct wg_hashtable *, struct wg_peer *);
static int wg_cookie_validate_packet(struct cookie_checker *, struct mbuf *, int);
static struct wg_peer *wg_peer_alloc(struct wg_softc *);
static void wg_peer_free_deferred(epoch_context_t);
static void wg_peer_destroy(struct wg_peer *);
static void wg_peer_send_buf(struct wg_peer *, uint8_t *, size_t);
static void wg_send_initiation(struct wg_peer *);
static void wg_send_response(struct wg_peer *);
static void wg_send_cookie(struct wg_softc *, struct cookie_macs *, uint32_t, struct mbuf *);
static void wg_peer_set_endpoint_from_tag(struct wg_peer *, struct wg_tag *);
static void wg_peer_clear_src(struct wg_peer *);
static void wg_peer_get_endpoint(struct wg_peer *, struct wg_endpoint *);
static void wg_deliver_out(struct wg_peer *);
static void wg_deliver_in(struct wg_peer *);
static void wg_send_buf(struct wg_softc *, struct wg_endpoint *, uint8_t *, size_t);
static void wg_send_keepalive(struct wg_peer *);
static void wg_handshake(struct wg_softc *, struct mbuf *);
static void wg_encap(struct wg_softc *, struct mbuf *);
static void wg_decap(struct wg_softc *, struct mbuf *);
static void wg_softc_handshake_receive(struct wg_softc *);
static void wg_softc_decrypt(struct wg_softc *);
static void wg_softc_encrypt(struct wg_softc *);
static struct noise_remote *wg_remote_get(struct wg_softc *, uint8_t [NOISE_PUBLIC_KEY_LEN]);
static uint32_t wg_index_set(struct wg_softc *, struct noise_remote *);
static struct noise_remote *wg_index_get(struct wg_softc *, uint32_t);
static void wg_index_drop(struct wg_softc *, uint32_t);
static int wg_update_endpoint_addrs(struct wg_endpoint *, const struct sockaddr *, struct ifnet *);
static void wg_input(struct mbuf *, int, struct inpcb *, const struct sockaddr *, void *);
static void wg_encrypt_dispatch(struct wg_softc *);
static void wg_decrypt_dispatch(struct wg_softc *);
static void crypto_taskq_setup(struct wg_softc *);
static void crypto_taskq_destroy(struct wg_softc *);
static int wg_clone_create(struct if_clone *, int, caddr_t);
static void wg_qflush(struct ifnet *);
static int wg_transmit(struct ifnet *, struct mbuf *);
static int wg_output(struct ifnet *, struct mbuf *, const struct sockaddr *, struct route *);
static void wg_clone_destroy(struct ifnet *);
static int wg_peer_to_export(struct wg_peer *, struct wg_peer_export *);
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
static void wg_module_init(void);
static void wg_module_deinit(void);

/* TODO Peer */
static struct wg_peer *
wg_peer_alloc(struct wg_softc *sc)
{
	struct wg_peer *peer;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	peer = malloc(sizeof(*peer), M_WG, M_WAITOK|M_ZERO);
	peer->p_sc = sc;
	peer->p_id = peer_counter++;
	CK_LIST_INIT(&peer->p_aips);

	rw_init(&peer->p_endpoint_lock, "wg_peer_endpoint");
	wg_queue_init(&peer->p_stage_queue, "stageq");
	wg_queue_init(&peer->p_encap_queue, "txq");
	wg_queue_init(&peer->p_decap_queue, "rxq");

	GROUPTASK_INIT(&peer->p_send_initiation, 0, (gtask_fn_t *)wg_send_initiation, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send_initiation, peer, NULL, NULL, "wg initiation");
	GROUPTASK_INIT(&peer->p_send_keepalive, 0, (gtask_fn_t *)wg_send_keepalive, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send_keepalive, peer, NULL, NULL, "wg keepalive");
	GROUPTASK_INIT(&peer->p_clear_secrets, 0, (gtask_fn_t *)noise_remote_clear, &peer->p_remote);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_clear_secrets,
	    &peer->p_remote, NULL, NULL, "wg clear secrets");

	GROUPTASK_INIT(&peer->p_send, 0, (gtask_fn_t *)wg_deliver_out, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_send, peer, NULL, NULL, "wg send");
	GROUPTASK_INIT(&peer->p_recv, 0, (gtask_fn_t *)wg_deliver_in, peer);
	taskqgroup_attach(qgroup_if_io_tqg, &peer->p_recv, peer, NULL, NULL, "wg recv");

	wg_timers_init(&peer->p_timers);

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

#define WG_HASHTABLE_PEER_FOREACH(peer, i, ht) \
	for (i = 0; i < HASHTABLE_PEER_SIZE; i++) \
		LIST_FOREACH(peer, &(ht)->h_peers[i], p_hash_entry)
#define WG_HASHTABLE_PEER_FOREACH_SAFE(peer, i, ht, tpeer) \
	for (i = 0; i < HASHTABLE_PEER_SIZE; i++) \
		CK_LIST_FOREACH_SAFE(peer, &(ht)->h_peers[i], p_hash_entry, tpeer)
static void
wg_hashtable_init(struct wg_hashtable *ht)
{
	mtx_init(&ht->h_mtx, "hash lock", NULL, MTX_DEF);
	arc4random_buf(&ht->h_secret, sizeof(ht->h_secret));
	ht->h_num_peers = 0;
	ht->h_peers = hashinit(HASHTABLE_PEER_SIZE, M_DEVBUF,
			&ht->h_peers_mask);
}

static void
wg_hashtable_destroy(struct wg_hashtable *ht)
{
	MPASS(ht->h_num_peers == 0);
	mtx_destroy(&ht->h_mtx);
	hashdestroy(ht->h_peers, M_DEVBUF, ht->h_peers_mask);
}

static void
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

static struct wg_peer *
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

static void
wg_hashtable_peer_remove(struct wg_hashtable *ht, struct wg_peer *peer)
{
	mtx_lock(&ht->h_mtx);
	ht->h_num_peers--;
	CK_LIST_REMOVE(peer, p_hash_entry);
	CK_LIST_REMOVE(peer, p_entry);
	mtx_unlock(&ht->h_mtx);
}

static void
wg_peer_free_deferred(epoch_context_t ctx)
{
	struct wg_peer *peer = __containerof(ctx, struct wg_peer, p_ctx);
	counter_u64_free(peer->p_tx_bytes);
	counter_u64_free(peer->p_rx_bytes);
	rw_destroy(&peer->p_timers.t_lock);
	rw_destroy(&peer->p_endpoint_lock);
	free(peer, M_WG);
}

static void
wg_peer_destroy(struct wg_peer *peer)
{
	/* Callers should already have called:
	 *    wg_hashtable_peer_remove(&sc->sc_hashtable, peer);
	 */
	wg_aip_delete(&peer->p_sc->sc_aips, peer);
	MPASS(CK_LIST_EMPTY(&peer->p_aips));

	/* We disable all timers, so we can't call the following tasks. */
	wg_timers_disable(&peer->p_timers);

	/* Ensure the tasks have finished running */
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

	wg_queue_deinit(&peer->p_decap_queue);
	wg_queue_deinit(&peer->p_encap_queue);
	wg_queue_deinit(&peer->p_stage_queue);

	/* Final cleanup */
	--peer->p_sc->sc_peer_count;
	noise_remote_clear(&peer->p_remote);
	DPRINTF(peer->p_sc, "Peer %llu destroyed\n", (unsigned long long)peer->p_id);
	NET_EPOCH_CALL(wg_peer_free_deferred, &peer->p_ctx);
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

/* Allowed IP */
static int
wg_aip_init(struct wg_aip_table *tbl)
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

static void
wg_aip_destroy(struct wg_aip_table *tbl)
{
	RADIX_NODE_HEAD_DESTROY(tbl->t_ip);
	free(tbl->t_ip, M_RTABLE);
#ifdef INET6
	RADIX_NODE_HEAD_DESTROY(tbl->t_ip6);
	free(tbl->t_ip6, M_RTABLE);
#endif
}

static void
wg_aip_populate_aip4(struct wg_aip *aip, const struct in_addr *addr,
    uint8_t mask)
{
	struct sockaddr_in *raddr, *rmask;
	uint8_t *p;
	unsigned int i;

	raddr = (struct sockaddr_in *)&aip->r_addr;
	rmask = (struct sockaddr_in *)&aip->r_mask;

	raddr->sin_len = sizeof(*raddr);
	raddr->sin_family = AF_INET;
	raddr->sin_addr = *addr;

	rmask->sin_len = sizeof(*rmask);
	p = (uint8_t *)&rmask->sin_addr.s_addr;
	for (i = 0; i < mask / NBBY; i++)
		p[i] = 0xff;
	if ((mask % NBBY) != 0)
		p[i] = (0xff00 >> (mask % NBBY)) & 0xff;
	raddr->sin_addr.s_addr &= rmask->sin_addr.s_addr;
}

static void
wg_aip_populate_aip6(struct wg_aip *aip, const struct in6_addr *addr,
    uint8_t mask)
{
	struct sockaddr_in6 *raddr, *rmask;

	raddr = (struct sockaddr_in6 *)&aip->r_addr;
	rmask = (struct sockaddr_in6 *)&aip->r_mask;

	raddr->sin6_len = sizeof(*raddr);
	raddr->sin6_family = AF_INET6;
	raddr->sin6_addr = *addr;

	rmask->sin6_len = sizeof(*rmask);
	in6_prefixlen2mask(&rmask->sin6_addr, mask);
	for (int i = 0; i < 4; ++i)
		raddr->sin6_addr.__u6_addr.__u6_addr32[i] &= rmask->sin6_addr.__u6_addr.__u6_addr32[i];
}

/* wg_aip_take assumes that the caller guarantees the allowed-ip exists. */
static void
wg_aip_take(struct radix_node_head *root, struct wg_peer *peer,
    struct wg_aip *route)
{
	struct radix_node *node;
	struct wg_peer *ppeer;

	RADIX_NODE_HEAD_LOCK_ASSERT(root);

	node = root->rnh_lookup(&route->r_addr, &route->r_mask,
	    &root->rh);
	MPASS(node != NULL);

	route = (struct wg_aip *)node;
	ppeer = route->r_peer;
	if (ppeer != peer) {
		route->r_peer = peer;

		CK_LIST_REMOVE(route, r_entry);
		CK_LIST_INSERT_HEAD(&peer->p_aips, route, r_entry);
	}
}

static int
wg_aip_add(struct wg_aip_table *tbl, struct wg_peer *peer,
			 const struct wg_allowedip *aip)
{
	struct radix_node	*node;
	struct radix_node_head	*root;
	struct wg_aip *route;
	sa_family_t family;
	bool needfree = false;

	family = aip->family;
	if (family != AF_INET && family != AF_INET6) {
		return (EINVAL);
	}

	route = malloc(sizeof(*route), M_WG, M_WAITOK|M_ZERO);
	switch (family) {
	case AF_INET:
		root = tbl->t_ip;

		wg_aip_populate_aip4(route, &aip->ip4, aip->cidr);
		break;
	case AF_INET6:
		root = tbl->t_ip6;

		wg_aip_populate_aip6(route, &aip->ip6, aip->cidr);
		break;
	}

	route->r_peer = peer;

	RADIX_NODE_HEAD_LOCK(root);
	node = root->rnh_addaddr(&route->r_addr, &route->r_mask, &root->rh,
							route->r_nodes);
	if (node == route->r_nodes) {
		tbl->t_count++;
		CK_LIST_INSERT_HEAD(&peer->p_aips, route, r_entry);
	} else {
		needfree = true;
		wg_aip_take(root, peer, route);
	}
	RADIX_NODE_HEAD_UNLOCK(root);
	if (needfree) {
		free(route, M_WG);
	}
	return (0);
}

static struct wg_peer *
wg_aip_lookup(struct wg_aip_table *tbl, struct mbuf *m,
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
		return NULL;

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
		return (NULL);
	}
	RADIX_NODE_HEAD_RLOCK(root);
	if ((node = root->rnh_matchaddr(addr, &root->rh)) != NULL) {
		peer = ((struct wg_aip *) node)->r_peer;
	}
	RADIX_NODE_HEAD_RUNLOCK(root);
	return (peer);
}

struct peer_del_arg {
	struct radix_node_head * pda_head;
	struct wg_peer *pda_peer;
	struct wg_aip_table *pda_tbl;
};

static int
wg_peer_remove(struct radix_node *rn, void *arg)
{
	struct peer_del_arg *pda = arg;
	struct wg_peer *peer = pda->pda_peer;
	struct radix_node_head * rnh = pda->pda_head;
	struct wg_aip_table *tbl = pda->pda_tbl;
	struct wg_aip *route = (struct wg_aip *)rn;
	struct radix_node *x;

	if (route->r_peer != peer)
		return (0);
	x = (struct radix_node *)rnh->rnh_deladdr(&route->r_addr,
	    &route->r_mask, &rnh->rh);
	if (x != NULL)	 {
		tbl->t_count--;
		CK_LIST_REMOVE(route, r_entry);
		free(route, M_WG);
	}
	return (0);
}

static void
wg_peer_remove_all(struct wg_softc *sc)
{
	struct wg_peer *peer, *tpeer;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	CK_LIST_FOREACH_SAFE(peer, &sc->sc_hashtable.h_peers_list,
			     p_entry, tpeer) {
		wg_hashtable_peer_remove(&sc->sc_hashtable, peer);
		wg_peer_destroy(peer);
	}
}

static int
wg_aip_delete(struct wg_aip_table *tbl, struct wg_peer *peer)
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

static int
wg_socket_init(struct wg_softc *sc, in_port_t port)
{
	struct thread *td;
	struct ucred *cred;
	struct socket *so4, *so6;
	int rc;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	td = curthread;
	if (sc->sc_ucred == NULL)
		return (EBUSY);
	cred = crhold(sc->sc_ucred);

	/*
	 * For socket creation, we use the creds of the thread that created the
	 * tunnel rather than the current thread to maintain the semantics that
	 * WireGuard has on Linux with network namespaces -- that the sockets
	 * are created in their home vnet so that they can be configured and
	 * functionally attached to a foreign vnet as the jail's only interface
	 * to the network.
	 */
	rc = socreate(AF_INET, &so4, SOCK_DGRAM, IPPROTO_UDP, cred, td);
	if (rc)
		goto out;

	rc = udp_set_kernel_tunneling(so4, wg_input, NULL, sc);
	/*
	 * udp_set_kernel_tunneling can only fail if there is already a tunneling function set.
	 * This should never happen with a new socket.
	 */
	MPASS(rc == 0);

	rc = socreate(AF_INET6, &so6, SOCK_DGRAM, IPPROTO_UDP, cred, td);
	if (rc) {
		SOCK_LOCK(so4);
		sofree(so4);
		goto out;
	}
	rc = udp_set_kernel_tunneling(so6, wg_input, NULL, sc);
	MPASS(rc == 0);

	so4->so_user_cookie = so6->so_user_cookie = sc->sc_socket.so_user_cookie;

	rc = wg_socket_bind(so4, so6, &port);
	if (rc == 0) {
		sc->sc_socket.so_port = port;
		wg_socket_set(sc, so4, so6);
	}
out:
	crfree(cred);
	return (rc);
}

static void wg_socket_set_cookie(struct wg_softc *sc, uint32_t user_cookie)
{
	struct wg_socket *so = &sc->sc_socket;

	sx_assert(&sc->sc_lock, SX_XLOCKED);

	so->so_user_cookie = user_cookie;
	if (so->so_so4)
		so->so_so4->so_user_cookie = user_cookie;
	if (so->so_so6)
		so->so_so6->so_user_cookie = user_cookie;
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

union wg_sockaddr {
	struct sockaddr sa;
	struct sockaddr_in in4;
	struct sockaddr_in6 in6;
};

static int
wg_socket_bind(struct socket *so4, struct socket *so6, in_port_t *requested_port)
{
	int rc;
	struct thread *td;
	union wg_sockaddr laddr;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	in_port_t port = *requested_port;

	td = curthread;
	bzero(&laddr, sizeof(laddr));
	sin = &laddr.in4;
	sin->sin_len = sizeof(laddr.in4);
	sin->sin_family = AF_INET;
	sin->sin_port = htons(port);
	sin->sin_addr = (struct in_addr) { 0 };

	if ((rc = sobind(so4, &laddr.sa, td)) != 0)
		return (rc);

	if (port == 0) {
		rc = sogetsockaddr(so4, (struct sockaddr **)&sin);
		if (rc != 0)
			return (rc);
		port = ntohs(sin->sin_port);
		free(sin, M_SONAME);
	}

	sin6 = &laddr.in6;
	sin6->sin6_len = sizeof(laddr.in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = htons(port);
	sin6->sin6_addr = (struct in6_addr) { .s6_addr = { 0 } };
	rc = sobind(so6, &laddr.sa, td);
	if (rc != 0)
		return (rc);
	*requested_port = port;
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
		ret = wg_send(sc, e, m);
	}
	if (ret)
		DPRINTF(sc, "Unable to send packet: %d\n", ret);
}

/* TODO Tag */
static struct wg_tag *
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

/* Timers */
static void
wg_timers_init(struct wg_timers *t)
{
	bzero(t, sizeof(*t));

	t->t_disabled = 1;
	rw_init(&t->t_lock, "wg peer timers");
	callout_init(&t->t_retry_handshake, true);
	callout_init(&t->t_send_keepalive, true);
	callout_init(&t->t_new_handshake, true);
	callout_init(&t->t_zero_key_material, true);
	callout_init(&t->t_persistent_keepalive, true);
}

static void
wg_timers_enable(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	t->t_disabled = 0;
	rw_wunlock(&t->t_lock);
	wg_timers_run_persistent_keepalive(t);
}

static void
wg_timers_disable(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	t->t_disabled = 1;
	t->t_need_another_keepalive = 0;
	rw_wunlock(&t->t_lock);

	callout_stop(&t->t_retry_handshake);
	callout_stop(&t->t_send_keepalive);
	callout_stop(&t->t_new_handshake);
	callout_stop(&t->t_zero_key_material);
	callout_stop(&t->t_persistent_keepalive);
}

static void
wg_timers_set_persistent_keepalive(struct wg_timers *t, uint16_t interval)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled) {
		t->t_persistent_keepalive_interval = interval;
		wg_timers_run_persistent_keepalive(t);
	}
	rw_runlock(&t->t_lock);
}

static void
wg_timers_get_last_handshake(struct wg_timers *t, struct timespec *time)
{
	rw_rlock(&t->t_lock);
	time->tv_sec = t->t_handshake_complete.tv_sec;
	time->tv_nsec = t->t_handshake_complete.tv_nsec;
	rw_runlock(&t->t_lock);
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

	rw_wlock(&t->t_lock);
	if ((ret = wg_timers_expired_handshake_last_sent(t)) == ETIMEDOUT)
		getnanouptime(&t->t_handshake_last_sent);
	rw_wunlock(&t->t_lock);
	return (ret);
}

/* Should be called after an authenticated data packet is sent. */
static void
wg_timers_event_data_sent(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled && !callout_pending(&t->t_new_handshake))
		callout_reset(&t->t_new_handshake, MSEC_2_TICKS(
		    NEW_HANDSHAKE_TIMEOUT * 1000 +
		    arc4random_uniform(REKEY_TIMEOUT_JITTER)),
		    (timeout_t *)wg_timers_run_new_handshake, t);
	rw_runlock(&t->t_lock);
}

/* Should be called after an authenticated data packet is received. */
static void
wg_timers_event_data_received(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled) {
		if (!callout_pending(&t->t_send_keepalive)) {
			callout_reset(&t->t_send_keepalive,
			    MSEC_2_TICKS(KEEPALIVE_TIMEOUT * 1000),
			    (timeout_t *)wg_timers_run_send_keepalive, t);
		} else {
			t->t_need_another_keepalive = 1;
		}
	}
	rw_runlock(&t->t_lock);
}

/*
 * Should be called after any type of authenticated packet is sent, whether
 * keepalive, data, or handshake.
 */
static void
wg_timers_event_any_authenticated_packet_sent(struct wg_timers *t)
{
	callout_stop(&t->t_send_keepalive);
}

/*
 * Should be called after any type of authenticated packet is received, whether
 * keepalive, data, or handshake.
 */
static void
wg_timers_event_any_authenticated_packet_received(struct wg_timers *t)
{
	callout_stop(&t->t_new_handshake);
}

/*
 * Should be called before a packet with authentication, whether
 * keepalive, data, or handshake is sent, or after one is received.
 */
static void
wg_timers_event_any_authenticated_packet_traversal(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled && t->t_persistent_keepalive_interval > 0)
		callout_reset(&t->t_persistent_keepalive,
		     MSEC_2_TICKS(t->t_persistent_keepalive_interval * 1000),
		    (timeout_t *)wg_timers_run_persistent_keepalive, t);
	rw_runlock(&t->t_lock);
}

/* Should be called after a handshake initiation message is sent. */
static void
wg_timers_event_handshake_initiated(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled)
		callout_reset(&t->t_retry_handshake, MSEC_2_TICKS(
		    REKEY_TIMEOUT * 1000 +
		    arc4random_uniform(REKEY_TIMEOUT_JITTER)),
		    (timeout_t *)wg_timers_run_retry_handshake, t);
	rw_runlock(&t->t_lock);
}

static void
wg_timers_event_handshake_responded(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	getnanouptime(&t->t_handshake_last_sent);
	rw_wunlock(&t->t_lock);
}

/*
 * Should be called after a handshake response message is received and processed
 * or when getting key confirmation via the first data message.
 */
static void
wg_timers_event_handshake_complete(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	if (!t->t_disabled) {
		callout_stop(&t->t_retry_handshake);
		t->t_handshake_retries = 0;
		getnanotime(&t->t_handshake_complete);
		wg_timers_run_send_keepalive(t);
	}
	rw_wunlock(&t->t_lock);
}

/*
 * Should be called after an ephemeral key is created, which is before sending a
 * handshake response or after receiving a handshake response.
 */
static void
wg_timers_event_session_derived(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled) {
		callout_reset(&t->t_zero_key_material,
		    MSEC_2_TICKS(REJECT_AFTER_TIME * 3 * 1000),
		    (timeout_t *)wg_timers_run_zero_key_material, t);
	}
	rw_runlock(&t->t_lock);
}

static void
wg_timers_event_want_initiation(struct wg_timers *t)
{
	rw_rlock(&t->t_lock);
	if (!t->t_disabled)
		wg_timers_run_send_initiation(t, 0);
	rw_runlock(&t->t_lock);
}

static void
wg_timers_event_reset_handshake_last_sent(struct wg_timers *t)
{
	rw_wlock(&t->t_lock);
	t->t_handshake_last_sent.tv_sec -= (REKEY_TIMEOUT + 1);
	rw_wunlock(&t->t_lock);
}

static void
wg_timers_run_send_initiation(struct wg_timers *t, int is_retry)
{
	struct wg_peer	 *peer = __containerof(t, struct wg_peer, p_timers);
	if (!is_retry)
		t->t_handshake_retries = 0;
	if (wg_timers_expired_handshake_last_sent(t) == ETIMEDOUT)
		GROUPTASK_ENQUEUE(&peer->p_send_initiation);
}

static void
wg_timers_run_retry_handshake(struct wg_timers *t)
{
	struct wg_peer	*peer = __containerof(t, struct wg_peer, p_timers);

	rw_wlock(&t->t_lock);
	if (t->t_handshake_retries <= MAX_TIMER_HANDSHAKES) {
		t->t_handshake_retries++;
		rw_wunlock(&t->t_lock);

		DPRINTF(peer->p_sc, "Handshake for peer %llu did not complete "
		    "after %d seconds, retrying (try %d)\n",
			(unsigned long long)peer->p_id,
		    REKEY_TIMEOUT, t->t_handshake_retries + 1);
		wg_peer_clear_src(peer);
		wg_timers_run_send_initiation(t, 1);
	} else {
		rw_wunlock(&t->t_lock);

		DPRINTF(peer->p_sc, "Handshake for peer %llu did not complete "
		    "after %d retries, giving up\n",
			(unsigned long long) peer->p_id, MAX_TIMER_HANDSHAKES + 2);

		callout_stop(&t->t_send_keepalive);
		wg_queue_purge(&peer->p_stage_queue);
		if (!callout_pending(&t->t_zero_key_material))
			callout_reset(&t->t_zero_key_material,
			    MSEC_2_TICKS(REJECT_AFTER_TIME * 3 * 1000),
			    (timeout_t *)wg_timers_run_zero_key_material, t);
	}
}

static void
wg_timers_run_send_keepalive(struct wg_timers *t)
{
	struct wg_peer	*peer = __containerof(t, struct wg_peer, p_timers);

	GROUPTASK_ENQUEUE(&peer->p_send_keepalive);
	if (t->t_need_another_keepalive) {
		t->t_need_another_keepalive = 0;
		callout_reset(&t->t_send_keepalive,
		    MSEC_2_TICKS(KEEPALIVE_TIMEOUT * 1000),
		    (timeout_t *)wg_timers_run_send_keepalive, t);
	}
}

static void
wg_timers_run_new_handshake(struct wg_timers *t)
{
	struct wg_peer	*peer = __containerof(t, struct wg_peer, p_timers);

	DPRINTF(peer->p_sc, "Retrying handshake with peer %llu because we "
	    "stopped hearing back after %d seconds\n",
		(unsigned long long)peer->p_id, NEW_HANDSHAKE_TIMEOUT);
	wg_peer_clear_src(peer);

	wg_timers_run_send_initiation(t, 0);
}

static void
wg_timers_run_zero_key_material(struct wg_timers *t)
{
	struct wg_peer *peer = __containerof(t, struct wg_peer, p_timers);

	DPRINTF(peer->p_sc, "Zeroing out all keys for peer %llu, since we "
	    "haven't received a new one in %d seconds\n",
		(unsigned long long)peer->p_id, REJECT_AFTER_TIME * 3);
	GROUPTASK_ENQUEUE(&peer->p_clear_secrets);
}

static void
wg_timers_run_persistent_keepalive(struct wg_timers *t)
{
	struct wg_peer	 *peer = __containerof(t, struct wg_peer, p_timers);

	if (t->t_persistent_keepalive_interval != 0)
		GROUPTASK_ENQUEUE(&peer->p_send_keepalive);
}

/* TODO Handshake */
static void
wg_peer_send_buf(struct wg_peer *peer, uint8_t *buf, size_t len)
{
	struct wg_endpoint endpoint;

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

	if (wg_timers_check_handshake_last_sent(&peer->p_timers) != ETIMEDOUT)
		return;
	DPRINTF(peer->p_sc, "Sending handshake initiation to peer %llu\n",
		(unsigned long long)peer->p_id);

	NET_EPOCH_ENTER(et);
	if (noise_create_initiation(&peer->p_remote, &pkt.s_idx, pkt.ue,
	    pkt.es, pkt.ets) != 0)
		goto out;
	pkt.t = WG_PKT_INITIATION;
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	    sizeof(pkt)-sizeof(pkt.m));
	wg_peer_send_buf(peer, (uint8_t *)&pkt, sizeof(pkt));
	wg_timers_event_handshake_initiated(&peer->p_timers);
out:
	NET_EPOCH_EXIT(et);
}

static void
wg_send_response(struct wg_peer *peer)
{
	struct wg_pkt_response pkt;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);

	DPRINTF(peer->p_sc, "Sending handshake response to peer %llu\n",
	    (unsigned long long)peer->p_id);

	if (noise_create_response(&peer->p_remote, &pkt.s_idx, &pkt.r_idx,
	    pkt.ue, pkt.en) != 0)
		goto out;
	if (noise_remote_begin_session(&peer->p_remote) != 0)
		goto out;

	wg_timers_event_session_derived(&peer->p_timers);
	pkt.t = WG_PKT_RESPONSE;
	cookie_maker_mac(&peer->p_cookie, &pkt.m, &pkt,
	     sizeof(pkt)-sizeof(pkt.m));
	wg_timers_event_handshake_responded(&peer->p_timers);
	wg_peer_send_buf(peer, (uint8_t*)&pkt, sizeof(pkt));
out:
	NET_EPOCH_EXIT(et);
}

static void
wg_send_cookie(struct wg_softc *sc, struct cookie_macs *cm, uint32_t idx,
    struct mbuf *m)
{
	struct wg_pkt_cookie	pkt;
	struct wg_endpoint *e;

	DPRINTF(sc, "Sending cookie response for denied handshake message\n");

	pkt.t = WG_PKT_COOKIE;
	pkt.r_idx = idx;

	e = wg_mbuf_endpoint_get(m);
	cookie_checker_create_payload(&sc->sc_cookie, cm, pkt.nonce,
	    pkt.ec, &e->e_remote.r_sa);
	wg_send_buf(sc, e, (uint8_t *)&pkt, sizeof(pkt));
}

static void
wg_send_keepalive(struct wg_peer *peer)
{
	struct mbuf *m = NULL;
	struct wg_tag *t;
	struct epoch_tracker et;

	if (wg_queue_len(&peer->p_stage_queue) != 0) {
		NET_EPOCH_ENTER(et);
		goto send;
	}
	if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
		return;
	if ((t = wg_tag_get(m)) == NULL) {
		m_freem(m);
		return;
	}
	t->t_peer = peer;
	t->t_mbuf = NULL;
	t->t_done = 0;
	t->t_mtu = 0; /* MTU == 0 OK for keepalive */

	NET_EPOCH_ENTER(et);
	wg_queue_stage(peer, m);
send:
	wg_queue_out(peer);
	NET_EPOCH_EXIT(et);
}

static int
wg_cookie_validate_packet(struct cookie_checker *checker, struct mbuf *m,
    int under_load)
{
	struct wg_pkt_initiation *init;
	struct wg_pkt_response *resp;
	struct cookie_macs *macs;
	struct wg_endpoint *e;
	int type, size;
	void *data;

	type = *mtod(m, uint32_t *);
	data = m->m_data;
	e = wg_mbuf_endpoint_get(m);
	if (type == WG_PKT_INITIATION) {
		init = mtod(m, struct wg_pkt_initiation *);
		macs = &init->m;
		size = sizeof(*init) - sizeof(*macs);
	} else if (type == WG_PKT_RESPONSE) {
		resp = mtod(m, struct wg_pkt_response *);
		macs = &resp->m;
		size = sizeof(*resp) - sizeof(*macs);
	} else
		return 0;

	return (cookie_checker_validate_macs(checker, macs, data, size,
	    under_load, &e->e_remote.r_sa));
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
	static const struct timeval underload_interval = { UNDERLOAD_TIMEOUT, 0 };
	bool packet_needs_cookie = false;
	int underload, res;

	underload = mbufq_len(&sc->sc_handshake_queue) >=
			MAX_QUEUED_HANDSHAKES / 8;
	if (underload)
		getmicrouptime(&wg_last_underload);
	else if (wg_last_underload.tv_sec != 0) {
		if (!ratecheck(&wg_last_underload, &underload_interval))
			underload = 1;
		else
			bzero(&wg_last_underload, sizeof(wg_last_underload));
	}

	res = wg_cookie_validate_packet(&sc->sc_cookie, m, underload);

	if (res && res != EAGAIN) {
		printf("validate_packet got %d\n", res);
		goto free;
	}
	if (res == EINVAL) {
		DPRINTF(sc, "Invalid initiation MAC\n");
		goto free;
	} else if (res == ECONNREFUSED) {
		DPRINTF(sc, "Handshake ratelimited\n");
		goto free;
	} else if (res == EAGAIN) {
		packet_needs_cookie = true;
	} else if (res != 0) {
		DPRINTF(sc, "Unexpected handshake ratelimit response: %d\n", res);
		goto free;
	}

	t = wg_tag_get(m);
	switch (*mtod(m, uint32_t *)) {
	case WG_PKT_INITIATION:
		init = mtod(m, struct wg_pkt_initiation *);

		if (packet_needs_cookie) {
			wg_send_cookie(sc, &init->m, init->s_idx, m);
			goto free;
		}
		if (noise_consume_initiation(&sc->sc_local, &remote,
		    init->s_idx, init->ue, init->es, init->ets) != 0) {
			DPRINTF(sc, "Invalid handshake initiation");
			goto free;
		}

		peer = __containerof(remote, struct wg_peer, p_remote);
		DPRINTF(sc, "Receiving handshake initiation from peer %llu\n",
		    (unsigned long long)peer->p_id);
		counter_u64_add(peer->p_rx_bytes, sizeof(*init));
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, sizeof(*init));
		wg_peer_set_endpoint_from_tag(peer, t);
		wg_send_response(peer);
		break;
	case WG_PKT_RESPONSE:
		resp = mtod(m, struct wg_pkt_response *);

		if (packet_needs_cookie) {
			wg_send_cookie(sc, &resp->m, resp->s_idx, m);
			goto free;
		}

		if ((remote = wg_index_get(sc, resp->r_idx)) == NULL) {
			DPRINTF(sc, "Unknown handshake response\n");
			goto free;
		}
		peer = __containerof(remote, struct wg_peer, p_remote);
		if (noise_consume_response(remote, resp->s_idx, resp->r_idx,
		    resp->ue, resp->en) != 0) {
			DPRINTF(sc, "Invalid handshake response\n");
			goto free;
		}

		DPRINTF(sc, "Receiving handshake response from peer %llu\n",
				(unsigned long long)peer->p_id);
		counter_u64_add(peer->p_rx_bytes, sizeof(*resp));
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, sizeof(*resp));
		wg_peer_set_endpoint_from_tag(peer, t);
		if (noise_remote_begin_session(&peer->p_remote) == 0) {
			wg_timers_event_session_derived(&peer->p_timers);
			wg_timers_event_handshake_complete(&peer->p_timers);
		}
		break;
	case WG_PKT_COOKIE:
		cook = mtod(m, struct wg_pkt_cookie *);

		if ((remote = wg_index_get(sc, cook->r_idx)) == NULL) {
			DPRINTF(sc, "Unknown cookie index\n");
			goto free;
		}

		peer = __containerof(remote, struct wg_peer, p_remote);

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
	m_freem(m);
}

static void
wg_softc_handshake_receive(struct wg_softc *sc)
{
	struct mbuf *m;

	while ((m = mbufq_dequeue(&sc->sc_handshake_queue)) != NULL)
		wg_handshake(sc, m);
}

/* TODO Encrypt */
static void
wg_encap(struct wg_softc *sc, struct mbuf *m)
{
	struct wg_pkt_data *data;
	size_t padding_len, plaintext_len, out_len;
	struct mbuf *mc;
	struct wg_peer *peer;
	struct wg_tag *t;
	uint64_t nonce;
	int res, allocation_order;

	NET_EPOCH_ASSERT();
	t = wg_tag_get(m);
	peer = t->t_peer;

	plaintext_len = MIN(WG_PKT_WITH_PADDING(m->m_pkthdr.len), t->t_mtu);
	padding_len = plaintext_len - m->m_pkthdr.len;
	out_len = sizeof(struct wg_pkt_data) + plaintext_len + NOISE_AUTHTAG_LEN;

	if (out_len <= MCLBYTES)
		allocation_order = MCLBYTES;
	else if (out_len <= MJUMPAGESIZE)
		allocation_order = MJUMPAGESIZE;
	else if (out_len <= MJUM9BYTES)
		allocation_order = MJUM9BYTES;
	else if (out_len <= MJUM16BYTES)
		allocation_order = MJUM16BYTES;
	else
		goto error;

	if ((mc = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, allocation_order)) == NULL)
		goto error;

	data = mtod(mc, struct wg_pkt_data *);
	m_copydata(m, 0, m->m_pkthdr.len, data->buf);
	bzero(data->buf + m->m_pkthdr.len, padding_len);

	data->t = WG_PKT_DATA;

	res = noise_remote_encrypt(&peer->p_remote, &data->r_idx, &nonce,
	    data->buf, plaintext_len);
	nonce = htole64(nonce); /* Wire format is little endian. */
	memcpy(data->nonce, &nonce, sizeof(data->nonce));

	if (__predict_false(res)) {
		if (res == EINVAL) {
			wg_timers_event_want_initiation(&peer->p_timers);
			m_freem(mc);
			goto error;
		} else if (res == ESTALE) {
			wg_timers_event_want_initiation(&peer->p_timers);
		} else {
			m_freem(mc);
			goto error;
		}
	}

	/* A packet with length 0 is a keepalive packet */
	if (m->m_pkthdr.len == 0)
		DPRINTF(sc, "Sending keepalive packet to peer %llu\n",
		    (unsigned long long)peer->p_id);
	/*
	 * Set the correct output value here since it will be copied
	 * when we move the pkthdr in send.
	 */
	mc->m_len = mc->m_pkthdr.len = out_len;
	mc->m_flags &= ~(M_MCAST | M_BCAST);

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
	uint64_t nonce;
	int res;

	NET_EPOCH_ASSERT();
	data = mtod(m, struct wg_pkt_data *);
	plaintext_len = m->m_pkthdr.len - sizeof(struct wg_pkt_data);

	t = wg_tag_get(m);
	peer = t->t_peer;

	memcpy(&nonce, data->nonce, sizeof(nonce));
	nonce = le64toh(nonce); /* Wire format is little endian. */

	res = noise_remote_decrypt(&peer->p_remote, data->r_idx, nonce,
	    data->buf, plaintext_len);

	if (__predict_false(res)) {
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

	/* Remove the data header, and crypto mac tail from the packet */
	m_adj(m, sizeof(struct wg_pkt_data));
	m_adj(m, -NOISE_AUTHTAG_LEN);

	/* A packet with length 0 is a keepalive packet */
	if (m->m_pkthdr.len == 0) {
		DPRINTF(peer->p_sc, "Receiving keepalive packet from peer "
		    "%llu\n", (unsigned long long)peer->p_id);
		goto done;
	}

	version = mtod(m, struct ip *)->ip_v;
	if (!((version == 4 && m->m_pkthdr.len >= sizeof(struct ip)) ||
	    (version == 6 && m->m_pkthdr.len >= sizeof(struct ip6_hdr)))) {
		DPRINTF(peer->p_sc, "Packet is neither ipv4 nor ipv6 from peer "
				"%llu\n", (unsigned long long)peer->p_id);
		goto error;
	}

	routed_peer = wg_aip_lookup(&peer->p_sc->sc_aips, m, IN);
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

static void
wg_softc_decrypt(struct wg_softc *sc)
{
	struct epoch_tracker et;
	struct mbuf *m;

	NET_EPOCH_ENTER(et);
	while ((m = buf_ring_dequeue_mc(sc->sc_decap_ring)) != NULL)
		wg_decap(sc, m);
	NET_EPOCH_EXIT(et);
}

static void
wg_softc_encrypt(struct wg_softc *sc)
{
	struct mbuf *m;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	while ((m = buf_ring_dequeue_mc(sc->sc_encap_ring)) != NULL)
		wg_encap(sc, m);
	NET_EPOCH_EXIT(et);
}

static void
wg_encrypt_dispatch(struct wg_softc *sc)
{
	for (int i = 0; i < mp_ncpus; i++) {
		if (sc->sc_encrypt[i].gt_task.ta_flags & TASK_ENQUEUED)
			continue;
		GROUPTASK_ENQUEUE(&sc->sc_encrypt[i]);
	}
}

static void
wg_decrypt_dispatch(struct wg_softc *sc)
{
	for (int i = 0; i < mp_ncpus; i++) {
		if (sc->sc_decrypt[i].gt_task.ta_flags & TASK_ENQUEUED)
			continue;
		GROUPTASK_ENQUEUE(&sc->sc_decrypt[i]);
	}
}

static void
wg_deliver_out(struct wg_peer *peer)
{
	struct epoch_tracker et;
	struct wg_tag *t;
	struct mbuf *m;
	struct wg_endpoint endpoint;
	size_t len;
	int ret;

	NET_EPOCH_ENTER(et);
	if (peer->p_sc->sc_ifp->if_link_state == LINK_STATE_DOWN)
		goto done;

	wg_peer_get_endpoint(peer, &endpoint);

	while ((m = wg_queue_dequeue(&peer->p_encap_queue, &t)) != NULL) {
		/* t_mbuf will contain the encrypted packet */
		if (t->t_mbuf == NULL) {
			if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OERRORS, 1);
			m_freem(m);
			continue;
		}
		len = t->t_mbuf->m_pkthdr.len;
		ret = wg_send(peer->p_sc, &endpoint, t->t_mbuf);

		if (ret == 0) {
			wg_timers_event_any_authenticated_packet_traversal(
			    &peer->p_timers);
			wg_timers_event_any_authenticated_packet_sent(
			    &peer->p_timers);

			if (m->m_pkthdr.len != 0)
				wg_timers_event_data_sent(&peer->p_timers);
			counter_u64_add(peer->p_tx_bytes, len);
		} else if (ret == EADDRNOTAVAIL) {
			wg_peer_clear_src(peer);
			wg_peer_get_endpoint(peer, &endpoint);
		}
		m_freem(m);
	}
done:
	NET_EPOCH_EXIT(et);
}

static void
wg_deliver_in(struct wg_peer *peer)
{
	struct mbuf *m;
	struct ifnet *ifp;
	struct wg_softc *sc;
	struct epoch_tracker et;
	struct wg_tag *t;
	uint32_t af;
	int version;

	NET_EPOCH_ENTER(et);
	sc = peer->p_sc;
	ifp = sc->sc_ifp;

	while ((m = wg_queue_dequeue(&peer->p_decap_queue, &t)) != NULL) {
		/* t_mbuf will contain the encrypted packet */
		if (t->t_mbuf == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			m_freem(m);
			continue;
		}
		MPASS(m == t->t_mbuf);

		wg_timers_event_any_authenticated_packet_received(
		    &peer->p_timers);
		wg_timers_event_any_authenticated_packet_traversal(
		    &peer->p_timers);

		counter_u64_add(peer->p_rx_bytes, m->m_pkthdr.len + sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
		if_inc_counter(sc->sc_ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len + sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN);

		if (m->m_pkthdr.len == 0) {
			m_freem(m);
			continue;
		}

		m->m_flags &= ~(M_MCAST | M_BCAST);
		m->m_pkthdr.rcvif = ifp;
		version = mtod(m, struct ip *)->ip_v;
		if (version == IPVERSION) {
			af = AF_INET;
			BPF_MTAP2(ifp, &af, sizeof(af), m);
			CURVNET_SET(ifp->if_vnet);
			ip_input(m);
			CURVNET_RESTORE();
		} else if (version == 6) {
			af = AF_INET6;
			BPF_MTAP2(ifp, &af, sizeof(af), m);
			CURVNET_SET(ifp->if_vnet);
			ip6_input(m);
			CURVNET_RESTORE();
		} else
			m_freem(m);

		wg_timers_event_data_received(&peer->p_timers);
	}
	NET_EPOCH_EXIT(et);
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
		m_freem(m);
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

static void
wg_queue_stage(struct wg_peer *peer, struct mbuf *m)
{
	struct wg_queue *q = &peer->p_stage_queue;
	mtx_lock(&q->q_mtx);
	STAILQ_INSERT_TAIL(&q->q.mq_head, m, m_stailqpkt);
	q->q.mq_len++;
	while (mbufq_full(&q->q)) {
		m = mbufq_dequeue(&q->q);
		if (m) {
			m_freem(m);
			if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
		}
	}
	mtx_unlock(&q->q_mtx);
}

static void
wg_queue_out(struct wg_peer *peer)
{
	struct buf_ring *parallel = peer->p_sc->sc_encap_ring;
	struct wg_queue		*serial = &peer->p_encap_queue;
	struct wg_tag		*t;
	struct mbufq		 staged;
	struct mbuf		*m;

	if (noise_remote_ready(&peer->p_remote) != 0) {
		if (wg_queue_len(&peer->p_stage_queue))
			wg_timers_event_want_initiation(&peer->p_timers);
		return;
	}

	/* We first "steal" the staged queue to a local queue, so that we can do these
	 * remaining operations without having to hold the staged queue mutex. */
	STAILQ_INIT(&staged.mq_head);
	mtx_lock(&peer->p_stage_queue.q_mtx);
	STAILQ_SWAP(&staged.mq_head, &peer->p_stage_queue.q.mq_head, mbuf);
	staged.mq_len = peer->p_stage_queue.q.mq_len;
	peer->p_stage_queue.q.mq_len = 0;
	staged.mq_maxlen = peer->p_stage_queue.q.mq_maxlen;
	mtx_unlock(&peer->p_stage_queue.q_mtx);

	while ((m = mbufq_dequeue(&staged)) != NULL) {
		if ((t = wg_tag_get(m)) == NULL) {
			m_freem(m);
			continue;
		}
		t->t_peer = peer;
		mtx_lock(&serial->q_mtx);
		if (mbufq_enqueue(&serial->q, m) != 0) {
			m_freem(m);
			if_inc_counter(peer->p_sc->sc_ifp, IFCOUNTER_OQDROPS, 1);
		} else {
			m->m_flags |= M_ENQUEUED;
			if (buf_ring_enqueue(parallel, m)) {
				t = wg_tag_get(m);
				t->t_done = 1;
			}
		}
		mtx_unlock(&serial->q_mtx);
	}
	wg_encrypt_dispatch(peer->p_sc);
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
	/* This access races. We might consider adding locking here. */
	return (mbufq_len(&q->q));
}

static void
wg_queue_init(struct wg_queue *q, const char *name)
{
	mtx_init(&q->q_mtx, name, NULL, MTX_DEF);
	mbufq_init(&q->q, MAX_QUEUED_PKT);
}

static void
wg_queue_deinit(struct wg_queue *q)
{
	wg_queue_purge(q);
	mtx_destroy(&q->q_mtx);
}

static void
wg_queue_purge(struct wg_queue *q)
{
	mtx_lock(&q->q_mtx);
	mbufq_drain(&q->q);
	mtx_unlock(&q->q_mtx);
}

/* TODO Indexes */
static struct noise_remote *
wg_remote_get(struct wg_softc *sc, uint8_t public[NOISE_PUBLIC_KEY_LEN])
{
	struct wg_peer *peer;

	if ((peer = wg_peer_lookup(sc, public)) == NULL)
		return (NULL);
	return (&peer->p_remote);
}

static uint32_t
wg_index_set(struct wg_softc *sc, struct noise_remote *remote)
{
	struct wg_index *index, *iter;
	struct wg_peer	*peer;
	uint32_t	 key;

	/* We can modify this without a lock as wg_index_set, wg_index_drop are
	 * guaranteed to be serialised (per remote). */
	peer = __containerof(remote, struct wg_peer, p_remote);
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

static struct noise_remote *
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

static void
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
	peer = __containerof(iter->i_value, struct wg_peer, p_remote);
	MPASS(peer != NULL);
	SLIST_INSERT_HEAD(&peer->p_unused_index, iter, i_unused_entry);
}

static int
wg_update_endpoint_addrs(struct wg_endpoint *e, const struct sockaddr *srcsa,
    struct ifnet *rcvif)
{
	const struct sockaddr_in *sa4;
#ifdef INET6
	const struct sockaddr_in6 *sa6;
#endif
	int ret = 0;

	/*
	 * UDP passes a 2-element sockaddr array: first element is the
	 * source addr/port, second the destination addr/port.
	 */
	if (srcsa->sa_family == AF_INET) {
		sa4 = (const struct sockaddr_in *)srcsa;
		e->e_remote.r_sin = sa4[0];
		e->e_local.l_in = sa4[1].sin_addr;
#ifdef INET6
	} else if (srcsa->sa_family == AF_INET6) {
		sa6 = (const struct sockaddr_in6 *)srcsa;
		e->e_remote.r_sin6 = sa6[0];
		e->e_local.l_in6 = sa6[1].sin6_addr;
#endif
	} else {
		ret = EAFNOSUPPORT;
	}

	return (ret);
}

static void
wg_input(struct mbuf *m0, int offset, struct inpcb *inpcb,
		 const struct sockaddr *srcsa, void *_sc)
{
	struct wg_pkt_data *pkt_data;
	struct wg_endpoint *e;
	struct wg_softc *sc = _sc;
	struct mbuf *m;
	int pktlen, pkttype;
	struct noise_remote *remote;
	struct wg_tag *t;
	void *data;

	/* Caller provided us with srcsa, no need for this header. */
	m_adj(m0, offset + sizeof(struct udphdr));

	/*
	 * Ensure mbuf has at least enough contiguous data to peel off our
	 * headers at the beginning.
	 */
	if ((m = m_defrag(m0, M_NOWAIT)) == NULL) {
		m_freem(m0);
		return;
	}
	data = mtod(m, void *);
	pkttype = *(uint32_t*)data;
	t = wg_tag_get(m);
	if (t == NULL) {
		goto free;
	}
	e = wg_mbuf_endpoint_get(m);

	if (wg_update_endpoint_addrs(e, srcsa, m->m_pkthdr.rcvif)) {
		goto free;
	}

	pktlen = m->m_pkthdr.len;

	if ((pktlen == sizeof(struct wg_pkt_initiation) &&
		 pkttype == WG_PKT_INITIATION) ||
		(pktlen == sizeof(struct wg_pkt_response) &&
		 pkttype == WG_PKT_RESPONSE) ||
		(pktlen == sizeof(struct wg_pkt_cookie) &&
		 pkttype == WG_PKT_COOKIE)) {
		if (mbufq_enqueue(&sc->sc_handshake_queue, m) == 0) {
			GROUPTASK_ENQUEUE(&sc->sc_handshake);
		} else {
			DPRINTF(sc, "Dropping handshake packet\n");
			m_freem(m);
		}
	} else if (pktlen >= sizeof(struct wg_pkt_data) + NOISE_AUTHTAG_LEN
	    && pkttype == WG_PKT_DATA) {

		pkt_data = data;
		remote = wg_index_get(sc, pkt_data->r_idx);
		if (remote == NULL) {
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);
			m_freem(m);
		} else if (buf_ring_count(sc->sc_decap_ring) > MAX_QUEUED_PKT) {
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IQDROPS, 1);
			m_freem(m);
		} else {
			t->t_peer = __containerof(remote, struct wg_peer,
			    p_remote);
			t->t_mbuf = NULL;
			t->t_done = 0;

			wg_queue_in(t->t_peer, m);
			wg_decrypt_dispatch(sc);
		}
	} else {
free:
		m_freem(m);
	}
}

static int
wg_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct wg_softc *sc;
	sa_family_t family;
	struct epoch_tracker et;
	struct wg_peer *peer;
	struct wg_tag *t;
	uint32_t af;
	int rc;

	/*
	 * Work around lifetime issue in the ipv6 mld code.
	 */
	if (__predict_false(ifp->if_flags & IFF_DYING))
		return (ENXIO);

	rc = 0;
	sc = ifp->if_softc;
	if ((t = wg_tag_get(m)) == NULL) {
		rc = ENOBUFS;
		goto early_out;
	}
	af = m->m_pkthdr.ph_family;
	BPF_MTAP2(ifp, &af, sizeof(af), m);

	NET_EPOCH_ENTER(et);
	peer = wg_aip_lookup(&sc->sc_aips, m, OUT);
	if (__predict_false(peer == NULL)) {
		rc = ENOKEY;
		goto err;
	}

	family = peer->p_endpoint.e_remote.r_sa.sa_family;
	if (__predict_false(family != AF_INET && family != AF_INET6)) {
		DPRINTF(sc, "No valid endpoint has been configured or "
			    "discovered for peer %llu\n", (unsigned long long)peer->p_id);

		rc = EHOSTUNREACH;
		goto err;
	}
	t->t_peer = peer;
	t->t_mbuf = NULL;
	t->t_done = 0;
	t->t_mtu = ifp->if_mtu;

	wg_queue_stage(peer, m);
	wg_queue_out(peer);
	NET_EPOCH_EXIT(et);
	return (rc);
err:
	NET_EPOCH_EXIT(et);
early_out:
	if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
	/* TODO: send ICMP unreachable */
	m_free(m);
	return (rc);
}

static int
wg_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *sa, struct route *rt)
{
	m->m_pkthdr.ph_family =  sa->sa_family;
	return (wg_transmit(ifp, m));
}

static int
wg_peer_add(struct wg_softc *sc, const nvlist_t *nvl)
{
	uint8_t			 public[WG_KEY_SIZE];
	const void *pub_key;
	const struct sockaddr *endpoint;
	int err;
	size_t size;
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
	if (noise_local_keys(&sc->sc_local, public, NULL) == 0 &&
	    bcmp(public, pub_key, WG_KEY_SIZE) == 0) {
		return (0); // Silently ignored; not actually a failure.
	}
	peer = wg_peer_lookup(sc, pub_key);
	if (nvlist_exists_bool(nvl, "remove") &&
		nvlist_get_bool(nvl, "remove")) {
		if (peer != NULL) {
			wg_hashtable_peer_remove(&sc->sc_hashtable, peer);
			wg_peer_destroy(peer);
		}
		return (0);
	}
	if (nvlist_exists_bool(nvl, "replace-allowedips") &&
		nvlist_get_bool(nvl, "replace-allowedips") &&
	    peer != NULL) {

		wg_aip_delete(&peer->p_sc->sc_aips, peer);
	}
	if (peer == NULL) {
		if (sc->sc_peer_count >= MAX_PEERS_PER_IFACE)
			return (E2BIG);
		sc->sc_peer_count++;

		need_insert = true;
		peer = wg_peer_alloc(sc);
		MPASS(peer != NULL);
		noise_remote_init(&peer->p_remote, pub_key, &sc->sc_local);
		cookie_maker_init(&peer->p_cookie, pub_key);
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
		const void *key;

		key = nvlist_get_binary(nvl, "preshared-key", &size);
		if (size != WG_KEY_SIZE) {
			err = EINVAL;
			goto out;
		}
		noise_remote_set_psk(&peer->p_remote, key);
	}
	if (nvlist_exists_number(nvl, "persistent-keepalive-interval")) {
		uint64_t pki = nvlist_get_number(nvl, "persistent-keepalive-interval");
		if (pki > UINT16_MAX) {
			err = EINVAL;
			goto out;
		}
		wg_timers_set_persistent_keepalive(&peer->p_timers, pki);
	}
	if (nvlist_exists_nvlist_array(nvl, "allowed-ips")) {
		const void *binary;
		uint64_t cidr;
		const nvlist_t * const * aipl;
		struct wg_allowedip aip;
		size_t allowedip_count;

		aipl = nvlist_get_nvlist_array(nvl, "allowed-ips",
		    &allowedip_count);
		for (size_t idx = 0; idx < allowedip_count; idx++) {
			if (!nvlist_exists_number(aipl[idx], "cidr"))
				continue;
			cidr = nvlist_get_number(aipl[idx], "cidr");
			if (nvlist_exists_binary(aipl[idx], "ipv4")) {
				binary = nvlist_get_binary(aipl[idx], "ipv4", &size);
				if (binary == NULL || cidr > 32 || size != sizeof(aip.ip4)) {
					err = EINVAL;
					goto out;
				}
				aip.family = AF_INET;
				memcpy(&aip.ip4, binary, sizeof(aip.ip4));
			} else if (nvlist_exists_binary(aipl[idx], "ipv6")) {
				binary = nvlist_get_binary(aipl[idx], "ipv6", &size);
				if (binary == NULL || cidr > 128 || size != sizeof(aip.ip6)) {
					err = EINVAL;
					goto out;
				}
				aip.family = AF_INET6;
				memcpy(&aip.ip6, binary, sizeof(aip.ip6));
			} else {
				continue;
			}
			aip.cidr = cidr;

			if ((err = wg_aip_add(&sc->sc_aips, peer, &aip)) != 0) {
				goto out;
			}
		}
	}
	if (need_insert) {
		wg_hashtable_peer_insert(&sc->sc_hashtable, peer);
		if (sc->sc_ifp->if_link_state == LINK_STATE_UP)
			wg_timers_enable(&peer->p_timers);
	}
	return (0);

out:
	if (need_insert) /* If we fail, only destroy if it was new. */
		wg_peer_destroy(peer);
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

	sx_xlock(&sc->sc_lock);

	nvlpacked = malloc(wgd->wgd_size, M_TEMP, M_WAITOK);
	err = copyin(wgd->wgd_data, nvlpacked, wgd->wgd_size);
	if (err)
		goto out;
	nvl = nvlist_unpack(nvlpacked, wgd->wgd_size, 0);
	if (nvl == NULL) {
		err = EBADMSG;
		goto out;
	}
	if (nvlist_exists_bool(nvl, "replace-peers") &&
		nvlist_get_bool(nvl, "replace-peers"))
		wg_peer_remove_all(sc);
	if (nvlist_exists_number(nvl, "listen-port")) {
		uint64_t new_port = nvlist_get_number(nvl, "listen-port");
		if (new_port > UINT16_MAX) {
			err = EINVAL;
			goto out;
		}
		if (new_port != sc->sc_socket.so_port) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if ((err = wg_socket_init(sc, new_port)) != 0)
					goto out;
			} else
				sc->sc_socket.so_port = new_port;
		}
	}
	if (nvlist_exists_binary(nvl, "private-key")) {
		const void *key = nvlist_get_binary(nvl, "private-key", &size);
		if (size != WG_KEY_SIZE) {
			err = EINVAL;
			goto out;
		}

		if (noise_local_keys(&sc->sc_local, NULL, private) != 0 ||
		    timingsafe_bcmp(private, key, WG_KEY_SIZE) != 0) {
			struct noise_local *local;
			struct wg_peer *peer;
			struct wg_hashtable *ht = &sc->sc_hashtable;
			bool has_identity;

			if (curve25519_generate_public(public, key)) {
				/* Peer conflict: remove conflicting peer. */
				if ((peer = wg_peer_lookup(sc, public)) !=
				    NULL) {
					wg_hashtable_peer_remove(ht, peer);
					wg_peer_destroy(peer);
				}
			}

			/*
			 * Set the private key and invalidate all existing
			 * handshakes.
			 */
			local = &sc->sc_local;
			noise_local_lock_identity(local);
			/* Note: we might be removing the private key. */
			has_identity = noise_local_set_private(local, key) == 0;
			mtx_lock(&ht->h_mtx);
			CK_LIST_FOREACH(peer, &ht->h_peers_list, p_entry) {
				noise_remote_precompute(&peer->p_remote);
				wg_timers_event_reset_handshake_last_sent(
				    &peer->p_timers);
				noise_remote_expire_current(&peer->p_remote);
			}
			mtx_unlock(&ht->h_mtx);
			cookie_checker_update(&sc->sc_cookie,
			    has_identity ? public : NULL);
			noise_local_unlock_identity(local);
		}
	}
	if (nvlist_exists_number(nvl, "user-cookie")) {
		uint64_t user_cookie = nvlist_get_number(nvl, "user-cookie");
		if (user_cookie > UINT32_MAX) {
			err = EINVAL;
			goto out;
		}
		wg_socket_set_cookie(sc, user_cookie);
	}
	if (nvlist_exists_nvlist_array(nvl, "peers")) {
		size_t peercount;
		const nvlist_t * const*nvl_peers;

		nvl_peers = nvlist_get_nvlist_array(nvl, "peers", &peercount);
		for (int i = 0; i < peercount; i++) {
			err = wg_peer_add(sc, nvl_peers[i]);
			if (err != 0)
				goto out;
		}
	}

	nvlist_destroy(nvl);
out:
	free(nvlpacked, M_TEMP);
	sx_xunlock(&sc->sc_lock);
	return (err);
}

static unsigned int
in_mask2len(struct in_addr *mask)
{
	unsigned int x, y;
	uint8_t *p;

	p = (uint8_t *)mask;
	for (x = 0; x < sizeof(*mask); x++) {
		if (p[x] != 0xff)
			break;
	}
	y = 0;
	if (x < sizeof(*mask)) {
		for (y = 0; y < NBBY; y++) {
			if ((p[x] & (0x80 >> y)) == 0)
				break;
		}
	}
	return x * NBBY + y;
}

static int
wg_peer_to_export(struct wg_peer *peer, struct wg_peer_export *exp)
{
	struct wg_endpoint *ep;
	struct wg_aip *rt;
	struct noise_remote *remote;
	int i;

	/* Non-sleepable context. */
	NET_EPOCH_ASSERT();

	bzero(&exp->endpoint, sizeof(exp->endpoint));
	remote = &peer->p_remote;
	ep = &peer->p_endpoint;
	if (ep->e_remote.r_sa.sa_family != 0) {
		exp->endpoint_sz = (ep->e_remote.r_sa.sa_family == AF_INET) ?
		    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

		memcpy(&exp->endpoint, &ep->e_remote, exp->endpoint_sz);
	}

	/* We always export it. */
	(void)noise_remote_keys(remote, exp->public_key, exp->preshared_key);
	exp->persistent_keepalive =
	    peer->p_timers.t_persistent_keepalive_interval;
	wg_timers_get_last_handshake(&peer->p_timers, &exp->last_handshake);
	exp->rx_bytes = counter_u64_fetch(peer->p_rx_bytes);
	exp->tx_bytes = counter_u64_fetch(peer->p_tx_bytes);

	exp->aip_count = 0;
	CK_LIST_FOREACH(rt, &peer->p_aips, r_entry) {
		exp->aip_count++;
	}

	/* Early success; no allowed-ips to copy out. */
	if (exp->aip_count == 0)
		return (0);

	exp->aip = malloc(exp->aip_count * sizeof(*exp->aip), M_TEMP, M_NOWAIT);
	if (exp->aip == NULL)
		return (ENOMEM);

	i = 0;
	CK_LIST_FOREACH(rt, &peer->p_aips, r_entry) {
		exp->aip[i].family = rt->r_addr.ss_family;
		if (exp->aip[i].family == AF_INET) {
			struct sockaddr_in *sin =
			    (struct sockaddr_in *)&rt->r_addr;

			exp->aip[i].ip4 = sin->sin_addr;

			sin = (struct sockaddr_in *)&rt->r_mask;
			exp->aip[i].cidr = in_mask2len(&sin->sin_addr);
		} else if (exp->aip[i].family == AF_INET6) {
			struct sockaddr_in6 *sin6 =
			    (struct sockaddr_in6 *)&rt->r_addr;

			exp->aip[i].ip6 = sin6->sin6_addr;

			sin6 = (struct sockaddr_in6 *)&rt->r_mask;
			exp->aip[i].cidr = in6_mask2len(&sin6->sin6_addr, NULL);
		}
		i++;
		if (i == exp->aip_count)
			break;
	}

	/* Again, AllowedIPs might have shrank; update it. */
	exp->aip_count = i;

	return (0);
}

static nvlist_t *
wg_peer_export_to_nvl(struct wg_softc *sc, struct wg_peer_export *exp)
{
	struct wg_timespec64 ts64;
	nvlist_t *nvl, **nvl_aips;
	size_t i;
	uint16_t family;

	nvl_aips = NULL;
	if ((nvl = nvlist_create(0)) == NULL)
		return (NULL);

	nvlist_add_binary(nvl, "public-key", exp->public_key,
	    sizeof(exp->public_key));
	if (wgc_privileged(sc))
		nvlist_add_binary(nvl, "preshared-key", exp->preshared_key,
		    sizeof(exp->preshared_key));
	if (exp->endpoint_sz != 0)
		nvlist_add_binary(nvl, "endpoint", &exp->endpoint,
		    exp->endpoint_sz);

	if (exp->aip_count != 0) {
		nvl_aips = mallocarray(exp->aip_count, sizeof(*nvl_aips),
		    M_WG, M_WAITOK | M_ZERO);
	}

	for (i = 0; i < exp->aip_count; i++) {
		nvl_aips[i] = nvlist_create(0);
		if (nvl_aips[i] == NULL)
			goto err;
		family = exp->aip[i].family;
		nvlist_add_number(nvl_aips[i], "cidr", exp->aip[i].cidr);
		if (family == AF_INET)
			nvlist_add_binary(nvl_aips[i], "ipv4",
			    &exp->aip[i].ip4, sizeof(exp->aip[i].ip4));
		else if (family == AF_INET6)
			nvlist_add_binary(nvl_aips[i], "ipv6",
			    &exp->aip[i].ip6, sizeof(exp->aip[i].ip6));
	}

	if (i != 0) {
		nvlist_add_nvlist_array(nvl, "allowed-ips",
		    (const nvlist_t *const *)nvl_aips, i);
	}

	for (i = 0; i < exp->aip_count; ++i)
		nvlist_destroy(nvl_aips[i]);

	free(nvl_aips, M_WG);
	nvl_aips = NULL;

	ts64.tv_sec = exp->last_handshake.tv_sec;
	ts64.tv_nsec = exp->last_handshake.tv_nsec;
	nvlist_add_binary(nvl, "last-handshake-time", &ts64, sizeof(ts64));

	if (exp->persistent_keepalive != 0)
		nvlist_add_number(nvl, "persistent-keepalive-interval",
		    exp->persistent_keepalive);

	if (exp->rx_bytes != 0)
		nvlist_add_number(nvl, "rx-bytes", exp->rx_bytes);
	if (exp->tx_bytes != 0)
		nvlist_add_number(nvl, "tx-bytes", exp->tx_bytes);

	return (nvl);
err:
	for (i = 0; i < exp->aip_count && nvl_aips[i] != NULL; i++) {
		nvlist_destroy(nvl_aips[i]);
	}

	free(nvl_aips, M_WG);
	nvlist_destroy(nvl);
	return (NULL);
}

static int
wg_marshal_peers(struct wg_softc *sc, nvlist_t **nvlp, nvlist_t ***nvl_arrayp, int *peer_countp)
{
	struct wg_peer *peer;
	int err, i, peer_count;
	nvlist_t *nvl, **nvl_array;
	struct epoch_tracker et;
	struct wg_peer_export *wpe;

	nvl = NULL;
	nvl_array = NULL;
	if (nvl_arrayp)
		*nvl_arrayp = NULL;
	if (nvlp)
		*nvlp = NULL;
	if (peer_countp)
		*peer_countp = 0;
	peer_count = sc->sc_hashtable.h_num_peers;
	if (peer_count == 0) {
		return (ENOENT);
	}

	if (nvlp && (nvl = nvlist_create(0)) == NULL)
		return (ENOMEM);

	err = i = 0;
	nvl_array = malloc(peer_count*sizeof(void*), M_TEMP, M_WAITOK | M_ZERO);
	wpe = malloc(peer_count*sizeof(*wpe), M_TEMP, M_WAITOK | M_ZERO);

	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(peer, &sc->sc_hashtable.h_peers_list, p_entry) {
		if ((err = wg_peer_to_export(peer, &wpe[i])) != 0) {
			break;
		}

		i++;
		if (i == peer_count)
			break;
	}
	NET_EPOCH_EXIT(et);

	if (err != 0)
		goto out;

	/* Update the peer count, in case we found fewer entries. */
	*peer_countp = peer_count = i;
	if (peer_count == 0) {
		err = ENOENT;
		goto out;
	}

	for (i = 0; i < peer_count; i++) {
		int idx;

		/*
		 * Peers are added to the list in reverse order, effectively,
		 * because it's simpler/quicker to add at the head every time.
		 *
		 * Export them in reverse order.  No worries if we fail mid-way
		 * through, the cleanup below will DTRT.
		 */
		idx = peer_count - i - 1;
		nvl_array[idx] = wg_peer_export_to_nvl(sc, &wpe[i]);
		if (nvl_array[idx] == NULL) {
			break;
		}
	}

	if (i < peer_count) {
		/* Error! */
		*peer_countp = 0;
		err = ENOMEM;
	} else if (nvl) {
		nvlist_add_nvlist_array(nvl, "peers",
		    (const nvlist_t * const *)nvl_array, peer_count);
		if ((err = nvlist_error(nvl))) {
			goto out;
		}
		*nvlp = nvl;
	}
	*nvl_arrayp = nvl_array;
 out:
	if (err != 0) {
		/* Note that nvl_array is populated in reverse order. */
		for (i = 0; i < peer_count; i++) {
			nvlist_destroy(nvl_array[i]);
		}

		free(nvl_array, M_TEMP);
		if (nvl != NULL)
			nvlist_destroy(nvl);
	}

	for (i = 0; i < peer_count; i++)
		free(wpe[i].aip, M_TEMP);
	free(wpe, M_TEMP);
	return (err);
}

static int
wgc_get(struct wg_softc *sc, struct wg_data_io *wgd)
{
	nvlist_t *nvl, **nvl_array;
	void *packed;
	size_t size;
	int peer_count, err;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

	sx_slock(&sc->sc_lock);

	err = 0;
	packed = NULL;
	if (sc->sc_socket.so_port != 0)
		nvlist_add_number(nvl, "listen-port", sc->sc_socket.so_port);
	if (sc->sc_socket.so_user_cookie != 0)
		nvlist_add_number(nvl, "user-cookie", sc->sc_socket.so_user_cookie);
	if (sc->sc_local.l_has_identity) {
		nvlist_add_binary(nvl, "public-key", sc->sc_local.l_public, WG_KEY_SIZE);
		if (wgc_privileged(sc))
			nvlist_add_binary(nvl, "private-key", sc->sc_local.l_private, WG_KEY_SIZE);
	}
	if (sc->sc_hashtable.h_num_peers > 0) {
		err = wg_marshal_peers(sc, NULL, &nvl_array, &peer_count);
		if (err)
			goto out_nvl;
		nvlist_add_nvlist_array(nvl, "peers",
		    (const nvlist_t * const *)nvl_array, peer_count);
	}
	packed = nvlist_pack(nvl, &size);
	if (packed == NULL) {
		err = ENOMEM;
		goto out_nvl;
	}
	if (wgd->wgd_size == 0) {
		wgd->wgd_size = size;
		goto out_packed;
	}
	if (wgd->wgd_size < size) {
		err = ENOSPC;
		goto out_packed;
	}
	if (wgd->wgd_data == NULL) {
		err = EFAULT;
		goto out_packed;
	}
	err = copyout(packed, wgd->wgd_data, size);
	wgd->wgd_size = size;

out_packed:
	free(packed, M_NVLIST);
out_nvl:
	nvlist_destroy(nvl);
	sx_sunlock(&sc->sc_lock);
	return (err);
}

static int
wg_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wg_data_io *wgd = (struct wg_data_io *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wg_softc	*sc = ifp->if_softc;
	int ret = 0;

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
		if ((ifp->if_flags & IFF_UP) != 0)
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
	default:
		ret = ENOTTY;
	}

	return ret;
}

static int
wg_up(struct wg_softc *sc)
{
	struct wg_hashtable *ht = &sc->sc_hashtable;
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
		mtx_lock(&ht->h_mtx);
		CK_LIST_FOREACH(peer, &ht->h_peers_list, p_entry) {
			wg_timers_enable(&peer->p_timers);
			wg_queue_out(peer);
		}
		mtx_unlock(&ht->h_mtx);

		if_link_state_change(sc->sc_ifp, LINK_STATE_UP);
	} else {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}
out:
	sx_xunlock(&sc->sc_lock);
	return (rc);
}

static void
wg_down(struct wg_softc *sc)
{
	struct wg_hashtable *ht = &sc->sc_hashtable;
	struct ifnet *ifp = sc->sc_ifp;
	struct wg_peer *peer;

	sx_xlock(&sc->sc_lock);
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		sx_xunlock(&sc->sc_lock);
		return;
	}
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	mtx_lock(&ht->h_mtx);
	CK_LIST_FOREACH(peer, &ht->h_peers_list, p_entry) {
                wg_queue_purge(&peer->p_stage_queue);
                wg_timers_disable(&peer->p_timers);
	}
	mtx_unlock(&ht->h_mtx);

	mbufq_drain(&sc->sc_handshake_queue);

	mtx_lock(&ht->h_mtx);
	CK_LIST_FOREACH(peer, &ht->h_peers_list, p_entry) {
                noise_remote_clear(&peer->p_remote);
                wg_timers_event_reset_handshake_last_sent(&peer->p_timers);
	}
	mtx_unlock(&ht->h_mtx);

	if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
	wg_socket_uninit(sc);

	sx_xunlock(&sc->sc_lock);
}

static void
crypto_taskq_setup(struct wg_softc *sc)
{

	sc->sc_encrypt = malloc(sizeof(struct grouptask)*mp_ncpus, M_WG, M_WAITOK);
	sc->sc_decrypt = malloc(sizeof(struct grouptask)*mp_ncpus, M_WG, M_WAITOK);

	for (int i = 0; i < mp_ncpus; i++) {
		GROUPTASK_INIT(&sc->sc_encrypt[i], 0,
		     (gtask_fn_t *)wg_softc_encrypt, sc);
		taskqgroup_attach_cpu(qgroup_if_io_tqg, &sc->sc_encrypt[i], sc, i, NULL, NULL, "wg encrypt");
		GROUPTASK_INIT(&sc->sc_decrypt[i], 0,
		    (gtask_fn_t *)wg_softc_decrypt, sc);
		taskqgroup_attach_cpu(qgroup_if_io_tqg, &sc->sc_decrypt[i], sc, i, NULL, NULL, "wg decrypt");
	}
}

static void
crypto_taskq_destroy(struct wg_softc *sc)
{
	for (int i = 0; i < mp_ncpus; i++) {
		taskqgroup_detach(qgroup_if_io_tqg, &sc->sc_encrypt[i]);
		taskqgroup_detach(qgroup_if_io_tqg, &sc->sc_decrypt[i]);
	}
	free(sc->sc_encrypt, M_WG);
	free(sc->sc_decrypt, M_WG);
}

static int
wg_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct wg_softc *sc;
	struct ifnet *ifp;
	struct noise_upcall noise_upcall;

	sc = malloc(sizeof(*sc), M_WG, M_WAITOK | M_ZERO);
	sc->sc_ucred = crhold(curthread->td_ucred);
	ifp = sc->sc_ifp = if_alloc(IFT_WIREGUARD);
	ifp->if_softc = sc;
	if_initname(ifp, wgname, unit);

	noise_upcall.u_arg = sc;
	noise_upcall.u_remote_get =
		(struct noise_remote *(*)(void *, uint8_t *))wg_remote_get;
	noise_upcall.u_index_set =
		(uint32_t (*)(void *, struct noise_remote *))wg_index_set;
	noise_upcall.u_index_drop =
		(void (*)(void *, uint32_t))wg_index_drop;
	noise_local_init(&sc->sc_local, &noise_upcall);
	cookie_checker_init(&sc->sc_cookie, ratelimit_zone);

	sc->sc_socket.so_port = 0;

	atomic_add_int(&clone_count, 1);
	ifp->if_capabilities = ifp->if_capenable = WG_CAPS;

	mbufq_init(&sc->sc_handshake_queue, MAX_QUEUED_HANDSHAKES);
	sx_init(&sc->sc_lock, "wg softc lock");
	rw_init(&sc->sc_index_lock, "wg index lock");
	sc->sc_peer_count = 0;
	sc->sc_encap_ring = buf_ring_alloc(MAX_QUEUED_PKT, M_WG, M_WAITOK, NULL);
	sc->sc_decap_ring = buf_ring_alloc(MAX_QUEUED_PKT, M_WG, M_WAITOK, NULL);
	GROUPTASK_INIT(&sc->sc_handshake, 0,
	    (gtask_fn_t *)wg_softc_handshake_receive, sc);
	taskqgroup_attach(qgroup_if_io_tqg, &sc->sc_handshake, sc, NULL, NULL, "wg tx initiation");
	crypto_taskq_setup(sc);

	wg_hashtable_init(&sc->sc_hashtable);
	sc->sc_index = hashinit(HASHTABLE_INDEX_SIZE, M_DEVBUF, &sc->sc_index_mask);
	wg_aip_init(&sc->sc_aips);

	if_setmtu(ifp, ETHERMTU - 80);
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_NOARP;
	ifp->if_init = wg_init;
	ifp->if_reassign = wg_reassign;
	ifp->if_qflush = wg_qflush;
	ifp->if_transmit = wg_transmit;
	ifp->if_output = wg_output;
	ifp->if_ioctl = wg_ioctl;

	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(uint32_t));

	sx_xlock(&wg_sx);
	LIST_INSERT_HEAD(&wg_list, sc, sc_entry);
	sx_xunlock(&wg_sx);

	return 0;
}

static void
wg_clone_destroy(struct ifnet *ifp)
{
	struct wg_softc *sc = ifp->if_softc;
	struct ucred *cred;

	sx_xlock(&wg_sx);
	sx_xlock(&sc->sc_lock);
	sc->sc_flags |= WGF_DYING;
	cred = sc->sc_ucred;
	sc->sc_ucred = NULL;
	sx_xunlock(&sc->sc_lock);
	LIST_REMOVE(sc, sc_entry);
	sx_xunlock(&wg_sx);

	if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);

	sx_xlock(&sc->sc_lock);
	wg_socket_uninit(sc);
	sx_xunlock(&sc->sc_lock);

	/*
	 * No guarantees that all traffic have passed until the epoch has
	 * elapsed with the socket closed.
	 */
	NET_EPOCH_WAIT();

	taskqgroup_drain_all(qgroup_if_io_tqg);
	sx_xlock(&sc->sc_lock);
	wg_peer_remove_all(sc);
	epoch_drain_callbacks(net_epoch_preempt);
	sx_xunlock(&sc->sc_lock);
	sx_destroy(&sc->sc_lock);
	rw_destroy(&sc->sc_index_lock);
	taskqgroup_detach(qgroup_if_io_tqg, &sc->sc_handshake);
	crypto_taskq_destroy(sc);
	buf_ring_free(sc->sc_encap_ring, M_WG);
	buf_ring_free(sc->sc_decap_ring, M_WG);

	wg_aip_destroy(&sc->sc_aips);
	wg_hashtable_destroy(&sc->sc_hashtable);

	if (cred != NULL)
		crfree(cred);
	if_detach(sc->sc_ifp);
	if_free(sc->sc_ifp);
	/* Ensure any local/private keys are cleaned up */
	explicit_bzero(sc, sizeof(*sc));
	free(sc, M_WG);

	atomic_add_int(&clone_count, -1);
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

	V_wg_cloner = if_clone_simple(wgname, wg_clone_create, wg_clone_destroy,
	    0);
}
VNET_SYSINIT(vnet_wg_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_wg_init, NULL);

static void
vnet_wg_uninit(const void *unused __unused)
{

	if_clone_detach(V_wg_cloner);
}
VNET_SYSUNINIT(vnet_wg_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_wg_uninit, NULL);

static int
wg_prison_remove(void *obj, void *data __unused)
{
	const struct prison *pr = obj;
	struct wg_softc *sc;
	struct ucred *cred;
	bool dying;

	/*
	 * Do a pass through all if_wg interfaces and release creds on any from
	 * the jail that are supposed to be going away.  This will, in turn, let
	 * the jail die so that we don't end up with Schrödinger's jail.
	 */
	sx_slock(&wg_sx);
	LIST_FOREACH(sc, &wg_list, sc_entry) {
		cred = NULL;

		sx_xlock(&sc->sc_lock);
		dying = (sc->sc_flags & WGF_DYING) != 0;
		if (!dying && sc->sc_ucred != NULL &&
		    sc->sc_ucred->cr_prison == pr) {
			/* Home jail is going away. */
			cred = sc->sc_ucred;
			sc->sc_ucred = NULL;

			sc->sc_flags |= WGF_DYING;
		}

		/*
		 * If this is our foreign vnet going away, we'll also down the
		 * link and kill the socket because traffic needs to stop.  Any
		 * address will be revoked in the rehoming process.
		 */
		if (cred != NULL || (!dying &&
		    sc->sc_ifp->if_vnet == pr->pr_vnet)) {
			if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
			/* Have to kill the sockets, as they also hold refs. */
			wg_socket_uninit(sc);
		}

		sx_xunlock(&sc->sc_lock);

		if (cred != NULL) {
			CURVNET_SET(sc->sc_ifp->if_vnet);
			if_purgeaddrs(sc->sc_ifp);
			CURVNET_RESTORE();
			crfree(cred);
		}
	}
	sx_sunlock(&wg_sx);

	return (0);
}

static void
wg_module_init(void)
{
	osd_method_t methods[PR_MAXMETHOD] = {
		[PR_METHOD_REMOVE] = wg_prison_remove,
	};

	ratelimit_zone = uma_zcreate("wg ratelimit", sizeof(struct ratelimit),
	     NULL, NULL, NULL, NULL, 0, 0);
	wg_osd_jail_slot = osd_jail_register(NULL, methods);
}

static void
wg_module_deinit(void)
{

	uma_zdestroy(ratelimit_zone);
	osd_jail_deregister(wg_osd_jail_slot);

	MPASS(LIST_EMPTY(&wg_list));
}

static int
wg_module_event_handler(module_t mod, int what, void *arg)
{

	switch (what) {
		case MOD_LOAD:
			wg_module_init();
			break;
		case MOD_UNLOAD:
			if (atomic_load_int(&clone_count) == 0)
				wg_module_deinit();
			else
				return (EBUSY);
			break;
		default:
			return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t wg_moduledata = {
	"wg",
	wg_module_event_handler,
	NULL
};

DECLARE_MODULE(wg, wg_moduledata, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(wg, 1);
MODULE_DEPEND(wg, crypto, 1, 1, 1);
