/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2022 Rubicon Communications, LLC (Netgate)
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf_ring.h>
#include <sys/epoch.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/nv.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/rmlock.h>
#include <sys/sdt.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <machine/atomic.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/netisr.h>
#include <net/route/nhop.h>

#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet6/ip6_var.h>
#include <netinet6/in6_fib.h>

#include <machine/in_cksum.h>

#include <opencrypto/cryptodev.h>

#include "if_ovpn.h"

struct ovpn_kkey_dir {
	int			refcount;
	uint8_t			key[32];
	uint8_t			keylen;
	uint8_t			nonce[8];
	uint8_t			noncelen;
	enum ovpn_key_cipher	cipher;
	crypto_session_t	cryptoid;

	struct mtx		replay_mtx;
	/*
	 * Last seen gapless sequence number. New rx seq numbers must be
	 * strictly higher than this.
	 */
	uint32_t		rx_seq;
	uint32_t		tx_seq;

	/* Seen packets, relative to rx_seq. bit(0) will always be 0. */
	uint64_t		rx_window;
};

struct ovpn_kkey {
	struct ovpn_kkey_dir	*encrypt;
	struct ovpn_kkey_dir	*decrypt;
	uint8_t			 keyid;
	uint32_t		 peerid;
};

struct ovpn_keepalive {
	uint32_t	interval;
	uint32_t	timeout;
};

struct ovpn_wire_header {
	uint32_t	 opcode; /* opcode, key id, peer id */
	uint32_t	 seq;
	uint8_t		 auth_tag[16];
};

struct ovpn_peer_counters {
	uint64_t	pkt_in;
	uint64_t	pkt_out;
	uint64_t	bytes_in;
	uint64_t	bytes_out;
};
#define OVPN_PEER_COUNTER_SIZE (sizeof(struct ovpn_peer_counters)/sizeof(uint64_t))

struct ovpn_notification {
	enum ovpn_notif_type	type;
	uint32_t		peerid;

	/* Delete notification */
	enum ovpn_del_reason	del_reason;
	struct ovpn_peer_counters	counters;
};

struct ovpn_softc;

struct ovpn_kpeer {
	RB_ENTRY(ovpn_kpeer)	 tree;
	int			 refcount;
	uint32_t		 peerid;

	struct ovpn_softc	*sc;
	struct sockaddr_storage	 local;
	struct sockaddr_storage	 remote;

	struct in_addr		 vpn4;
	struct in6_addr		 vpn6;

	struct ovpn_kkey	 keys[2];

	enum ovpn_del_reason	 del_reason;
	struct ovpn_keepalive	 keepalive;
	uint32_t		*last_active;
	struct callout		 ping_send;
	struct callout		 ping_rcv;

	counter_u64_t		 counters[OVPN_PEER_COUNTER_SIZE];
};

struct ovpn_counters {
	uint64_t	lost_ctrl_pkts_in;
	uint64_t	lost_ctrl_pkts_out;
	uint64_t	lost_data_pkts_in;
	uint64_t	lost_data_pkts_out;
	uint64_t	nomem_data_pkts_in;
	uint64_t	nomem_data_pkts_out;
	uint64_t	received_ctrl_pkts;
	uint64_t	received_data_pkts;
	uint64_t	sent_ctrl_pkts;
	uint64_t	sent_data_pkts;

	uint64_t	transport_bytes_sent;
	uint64_t	transport_bytes_received;
	uint64_t	tunnel_bytes_sent;
	uint64_t	tunnel_bytes_received;
};
#define OVPN_COUNTER_SIZE (sizeof(struct ovpn_counters)/sizeof(uint64_t))

RB_HEAD(ovpn_kpeers, ovpn_kpeer);

struct ovpn_softc {
	int			 refcount;
	struct rmlock		 lock;
	struct ifnet		*ifp;
	struct socket		*so;
	int			 peercount;
	struct ovpn_kpeers	 peers;

	/* Pending notification */
	struct buf_ring		*notifring;

	counter_u64_t 		 counters[OVPN_COUNTER_SIZE];

	struct epoch_context	 epoch_ctx;
};

static struct ovpn_kpeer *ovpn_find_peer(struct ovpn_softc *, uint32_t);
static bool ovpn_udp_input(struct mbuf *, int, struct inpcb *,
    const struct sockaddr *, void *);
static int ovpn_transmit_to_peer(struct ifnet *, struct mbuf *,
    struct ovpn_kpeer *, struct rm_priotracker *);
static int ovpn_encap(struct ovpn_softc *, uint32_t, struct mbuf *);
static int ovpn_get_af(struct mbuf *);
static void ovpn_free_kkey_dir(struct ovpn_kkey_dir *);
static bool ovpn_check_replay(struct ovpn_kkey_dir *, uint32_t);
static int ovpn_peer_compare(struct ovpn_kpeer *, struct ovpn_kpeer *);

static RB_PROTOTYPE(ovpn_kpeers, ovpn_kpeer, tree, ovpn_peer_compare);
static RB_GENERATE(ovpn_kpeers, ovpn_kpeer, tree, ovpn_peer_compare);

#define OVPN_MTU_MIN		576
#define OVPN_MTU_MAX		(IP_MAXPACKET - sizeof(struct ip) - \
    sizeof(struct udphdr) - sizeof(struct ovpn_wire_header))

#define OVPN_OP_DATA_V2		0x09
#define OVPN_OP_SHIFT		3
#define OVPN_SEQ_ROTATE		0x80000000

VNET_DEFINE_STATIC(struct if_clone *, ovpn_cloner);
#define	V_ovpn_cloner	VNET(ovpn_cloner)

#define OVPN_RLOCK_TRACKER	struct rm_priotracker _ovpn_lock_tracker; \
    struct rm_priotracker *_ovpn_lock_trackerp = &_ovpn_lock_tracker
#define OVPN_RLOCK(sc)		rm_rlock(&(sc)->lock, _ovpn_lock_trackerp)
#define OVPN_RUNLOCK(sc)	rm_runlock(&(sc)->lock, _ovpn_lock_trackerp)
#define OVPN_WLOCK(sc)		rm_wlock(&(sc)->lock)
#define OVPN_WUNLOCK(sc)	rm_wunlock(&(sc)->lock)
#define OVPN_ASSERT(sc)		rm_assert(&(sc)->lock, RA_LOCKED)
#define OVPN_RASSERT(sc)	rm_assert(&(sc)->lock, RA_RLOCKED)
#define OVPN_WASSERT(sc)	rm_assert(&(sc)->lock, RA_WLOCKED)
#define OVPN_UNLOCK_ASSERT(sc)	rm_assert(&(sc)->lock, RA_UNLOCKED)

#define OVPN_COUNTER(sc, name) \
	((sc)->counters[offsetof(struct ovpn_counters, name)/sizeof(uint64_t)])
#define OVPN_PEER_COUNTER(peer, name) \
	((peer)->counters[offsetof(struct ovpn_peer_counters, name) / \
	 sizeof(uint64_t)])

#define OVPN_COUNTER_ADD(sc, name, val)	\
	counter_u64_add(OVPN_COUNTER(sc, name), val)
#define OVPN_PEER_COUNTER_ADD(p, name, val)	\
	counter_u64_add(OVPN_PEER_COUNTER(p, name), val)

#define TO_IN(x)		((struct sockaddr_in *)(x))
#define TO_IN6(x)		((struct sockaddr_in6 *)(x))

SDT_PROVIDER_DEFINE(if_ovpn);
SDT_PROBE_DEFINE1(if_ovpn, tx, transmit, start, "struct mbuf *");
SDT_PROBE_DEFINE2(if_ovpn, tx, route, ip4, "struct in_addr *", "struct ovpn_kpeer *");
SDT_PROBE_DEFINE2(if_ovpn, tx, route, ip6, "struct in6_addr *", "struct ovpn_kpeer *");

static const char ovpnname[] = "ovpn";
static const char ovpngroupname[] = "openvpn";

static MALLOC_DEFINE(M_OVPN, ovpnname, "OpenVPN DCO Interface");

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_OTHER, openvpn, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "OpenVPN DCO Interface");
VNET_DEFINE_STATIC(int, replay_protection) = 0;
#define	V_replay_protection	VNET(replay_protection)
SYSCTL_INT(_net_link_openvpn, OID_AUTO, replay_protection, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(replay_protection), 0, "Validate sequence numbers");

VNET_DEFINE_STATIC(int, async_crypto);
#define	V_async_crypto		VNET(async_crypto)
SYSCTL_INT(_net_link_openvpn, OID_AUTO, async_crypto,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(async_crypto), 0,
	"Use asynchronous mode to parallelize crypto jobs.");

VNET_DEFINE_STATIC(int, async_netisr_queue);
#define	V_async_netisr_queue		VNET(async_netisr_queue)
SYSCTL_INT(_net_link_openvpn, OID_AUTO, netisr_queue,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(async_netisr_queue), 0,
	"Use netisr_queue() rather than netisr_dispatch().");

static int
ovpn_peer_compare(struct ovpn_kpeer *a, struct ovpn_kpeer *b)
{
	return (a->peerid - b->peerid);
}

static struct ovpn_kpeer *
ovpn_find_peer(struct ovpn_softc *sc, uint32_t peerid)
{
	struct ovpn_kpeer p;

	OVPN_ASSERT(sc);

	p.peerid = peerid;

	return (RB_FIND(ovpn_kpeers, &sc->peers, &p));
}

static struct ovpn_kpeer *
ovpn_find_only_peer(struct ovpn_softc *sc)
{
	OVPN_ASSERT(sc);

	return (RB_ROOT(&sc->peers));
}

static uint16_t
ovpn_get_port(struct sockaddr_storage *s)
{
	switch (s->ss_family) {
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *)s;
		return (in->sin_port);
	}
	case AF_INET6: {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)s;
		return (in6->sin6_port);
	}
	default:
		panic("Unsupported address family %d", s->ss_family);
	}
}

static int
ovpn_nvlist_to_sockaddr(const nvlist_t *nvl, struct sockaddr_storage *sa)
{
	int af;

	if (! nvlist_exists_number(nvl, "af"))
		return (EINVAL);
	if (! nvlist_exists_binary(nvl, "address"))
		return (EINVAL);
	if (! nvlist_exists_number(nvl, "port"))
		return (EINVAL);

	af = nvlist_get_number(nvl, "af");

	switch (af) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *)sa;
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "address", &len);
		in->sin_family = af;
		if (len != sizeof(in->sin_addr))
			return (EINVAL);

		memcpy(&in->sin_addr, addr, sizeof(in->sin_addr));
		in->sin_port = nvlist_get_number(nvl, "port");
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)sa;
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "address", &len);
		in6->sin6_family = af;
		if (len != sizeof(in6->sin6_addr))
			return (EINVAL);

		memcpy(&in6->sin6_addr, addr, sizeof(in6->sin6_addr));
		in6->sin6_port = nvlist_get_number(nvl, "port");
		break;
	}
#endif
	default:
		return (EINVAL);
	}

	return (0);
}

static bool
ovpn_has_peers(struct ovpn_softc *sc)
{
	OVPN_ASSERT(sc);

	return (sc->peercount > 0);
}

static void
ovpn_rele_so(struct ovpn_softc *sc, struct ovpn_kpeer *peer)
{
	bool has_peers;

	OVPN_WASSERT(sc);

	if (sc->so == NULL)
		return;

	has_peers = ovpn_has_peers(sc);

	/* Only remove the tunnel function if we're releasing the socket for
	 * the last peer. */
	if (! has_peers)
		(void)udp_set_kernel_tunneling(sc->so, NULL, NULL, NULL);

	sorele(sc->so);

	if (! has_peers)
		sc->so = NULL;
}

static void
ovpn_notify_del_peer(struct ovpn_softc *sc, struct ovpn_kpeer *peer)
{
	struct ovpn_notification *n;

	OVPN_WASSERT(sc);

	n = malloc(sizeof(*n), M_OVPN, M_NOWAIT);
	if (n == NULL)
		return;

	n->peerid = peer->peerid;
	n->type = OVPN_NOTIF_DEL_PEER;
	n->del_reason = peer->del_reason;

	n->counters.pkt_in = counter_u64_fetch(OVPN_PEER_COUNTER(peer, pkt_in));
	n->counters.pkt_out = counter_u64_fetch(OVPN_PEER_COUNTER(peer, pkt_out));
	n->counters.bytes_in = counter_u64_fetch(OVPN_PEER_COUNTER(peer, bytes_in));
	n->counters.bytes_out = counter_u64_fetch(OVPN_PEER_COUNTER(peer, bytes_out));

	if (buf_ring_enqueue(sc->notifring, n) != 0) {
		free(n, M_OVPN);
	} else if (sc->so != NULL) {
		/* Wake up userspace */
		sc->so->so_error = EAGAIN;
		sorwakeup(sc->so);
		sowwakeup(sc->so);
	}
}

static void
ovpn_notify_key_rotation(struct ovpn_softc *sc, struct ovpn_kpeer *peer)
{
	struct ovpn_notification *n;

	n = malloc(sizeof(*n), M_OVPN, M_NOWAIT | M_ZERO);
	if (n == NULL)
		return;

	n->peerid = peer->peerid;
	n->type = OVPN_NOTIF_ROTATE_KEY;

	if (buf_ring_enqueue(sc->notifring, n) != 0) {
		free(n, M_OVPN);
	} else if (sc->so != NULL) {
		/* Wake up userspace */
		sc->so->so_error = EAGAIN;
		sorwakeup(sc->so);
		sowwakeup(sc->so);
	}
}

static void
ovpn_peer_release_ref(struct ovpn_kpeer *peer, bool locked)
{
	struct ovpn_softc *sc;

	CURVNET_ASSERT_SET();

	atomic_add_int(&peer->refcount, -1);

	if (atomic_load_int(&peer->refcount) > 0)
		return;

	sc = peer->sc;

	if (! locked) {
		OVPN_WLOCK(sc);

		/* Might have changed before we acquired the lock. */
		if (atomic_load_int(&peer->refcount) > 0) {
			OVPN_WUNLOCK(sc);
			return;
		}
	}

	OVPN_ASSERT(sc);

	/* The peer should have been removed from the list already. */
	MPASS(ovpn_find_peer(sc, peer->peerid) == NULL);

	ovpn_notify_del_peer(sc, peer);

	for (int i = 0; i < 2; i++) {
		ovpn_free_kkey_dir(peer->keys[i].encrypt);
		ovpn_free_kkey_dir(peer->keys[i].decrypt);
	}

	ovpn_rele_so(sc, peer);

	callout_stop(&peer->ping_send);
	callout_stop(&peer->ping_rcv);
	uma_zfree_pcpu(pcpu_zone_4, peer->last_active);
	free(peer, M_OVPN);

	if (! locked)
		OVPN_WUNLOCK(sc);
}

static int
ovpn_new_peer(struct ifnet *ifp, const nvlist_t *nvl)
{
#ifdef INET6
	struct epoch_tracker et;
#endif
	struct sockaddr_storage remote;
	struct ovpn_kpeer *peer = NULL;
	struct file *fp = NULL;
	struct sockaddr *name = NULL;
	struct ovpn_softc *sc = ifp->if_softc;
	struct thread *td = curthread;
	struct socket *so = NULL;
	int fd;
	uint32_t peerid;
	int ret = 0;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "fd"))
		return (EINVAL);

	if (! nvlist_exists_nvlist(nvl, "remote"))
		return (EINVAL);

	peerid = nvlist_get_number(nvl, "peerid");

	ret = ovpn_nvlist_to_sockaddr(nvlist_get_nvlist(nvl, "remote"),
	    &remote);
	if (ret != 0)
		return (ret);

	fd = nvlist_get_number(nvl, "fd");

	/* Look up the userspace process and use the fd to find the socket. */
	ret = getsock(td, fd, &cap_connect_rights, &fp);
	if (ret != 0)
		return (ret);

	so = fp->f_data;

	peer = malloc(sizeof(*peer), M_OVPN, M_WAITOK | M_ZERO);
	peer->peerid = peerid;
	peer->sc = sc;
	peer->refcount = 1;
	peer->last_active = uma_zalloc_pcpu(pcpu_zone_4, M_WAITOK | M_ZERO);
	COUNTER_ARRAY_ALLOC(peer->counters, OVPN_PEER_COUNTER_SIZE, M_WAITOK);

	if (nvlist_exists_binary(nvl, "vpn_ipv4")) {
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "vpn_ipv4", &len);
		if (len != sizeof(peer->vpn4)) {
			ret = EINVAL;
			goto error;
		}
		memcpy(&peer->vpn4, addr, len);
	}

	if (nvlist_exists_binary(nvl, "vpn_ipv6")) {
		size_t len;
		const void *addr = nvlist_get_binary(nvl, "vpn_ipv6", &len);
		if (len != sizeof(peer->vpn6)) {
			ret = EINVAL;
			goto error;
		}
		memcpy(&peer->vpn6, addr, len);
	}

	callout_init_rm(&peer->ping_send, &sc->lock, CALLOUT_SHAREDLOCK);
	callout_init_rm(&peer->ping_rcv, &sc->lock, 0);

	ret = so->so_proto->pr_sockaddr(so, &name);
	if (ret)
		goto error;

	if (ovpn_get_port((struct sockaddr_storage *)name) == 0) {
		ret = EINVAL;
		goto error;
	}
	if (name->sa_family != remote.ss_family) {
		ret = EINVAL;
		goto error;
	}

	memcpy(&peer->local, name, name->sa_len);
	memcpy(&peer->remote, &remote, sizeof(remote));
	free(name, M_SONAME);
	name = NULL;

	if (peer->local.ss_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&TO_IN6(&peer->remote)->sin6_addr)) {
		/* V4 mapped address, so treat this as v4, not v6. */
		in6_sin6_2_sin_in_sock((struct sockaddr *)&peer->local);
		in6_sin6_2_sin_in_sock((struct sockaddr *)&peer->remote);
	}

#ifdef INET6
	if (peer->local.ss_family == AF_INET6 &&
	    IN6_IS_ADDR_UNSPECIFIED(&TO_IN6(&peer->local)->sin6_addr)) {
		NET_EPOCH_ENTER(et);
		ret = in6_selectsrc_addr(curthread->td_proc->p_fibnum,
		    &TO_IN6(&peer->remote)->sin6_addr,
		    0, NULL, &TO_IN6(&peer->local)->sin6_addr, NULL);
		NET_EPOCH_EXIT(et);
		if (ret != 0) {
			goto error;
		}
	}
#endif
	OVPN_WLOCK(sc);

	/* Disallow peer id re-use. */
	if (ovpn_find_peer(sc, peerid) != NULL) {
		ret = EEXIST;
		goto error_locked;
	}

	/* Make sure this is really a UDP socket. */
	if (so->so_type != SOCK_DGRAM || so->so_proto->pr_type != SOCK_DGRAM) {
		ret = EPROTOTYPE;
		goto error_locked;
	}

	/* Must be the same socket as for other peers on this interface. */
	if (sc->so != NULL && so != sc->so)
		goto error_locked;

	if (sc->so == NULL)
		sc->so = so;

	/* Insert the peer into the list. */
	RB_INSERT(ovpn_kpeers, &sc->peers, peer);
	sc->peercount++;
	soref(sc->so);

	ret = udp_set_kernel_tunneling(sc->so, ovpn_udp_input, NULL, sc);
	if (ret == EBUSY) {
		/* Fine, another peer already set the input function. */
		ret = 0;
	}
	if (ret != 0) {
		RB_REMOVE(ovpn_kpeers, &sc->peers, peer);
		sc->peercount--;
		goto error_locked;
	}

	OVPN_WUNLOCK(sc);

	goto done;

error_locked:
	OVPN_WUNLOCK(sc);
error:
	free(name, M_SONAME);
	COUNTER_ARRAY_FREE(peer->counters, OVPN_PEER_COUNTER_SIZE);
	uma_zfree_pcpu(pcpu_zone_4, peer->last_active);
	free(peer, M_OVPN);
done:
	if (fp != NULL)
		fdrop(fp, td);

	return (ret);
}

static int
_ovpn_del_peer(struct ovpn_softc *sc, struct ovpn_kpeer *peer)
{
	struct ovpn_kpeer *tmp __diagused;

	OVPN_WASSERT(sc);
	CURVNET_ASSERT_SET();

	MPASS(RB_FIND(ovpn_kpeers, &sc->peers, peer) == peer);

	tmp = RB_REMOVE(ovpn_kpeers, &sc->peers, peer);
	MPASS(tmp != NULL);

	sc->peercount--;

	ovpn_peer_release_ref(peer, true);

	return (0);
}

static int
ovpn_del_peer(struct ifnet *ifp, nvlist_t *nvl)
{
	struct ovpn_softc *sc = ifp->if_softc;
	struct ovpn_kpeer *peer;
	uint32_t peerid;
	int ret;

	OVPN_WASSERT(sc);

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);

	peerid = nvlist_get_number(nvl, "peerid");

	peer = ovpn_find_peer(sc, peerid);
	if (peer == NULL)
		return (ENOENT);

	peer->del_reason = OVPN_DEL_REASON_REQUESTED;
	ret = _ovpn_del_peer(sc, peer);

	return (ret);
}

static int
ovpn_create_kkey_dir(struct ovpn_kkey_dir **kdirp,
    const nvlist_t *nvl)
{
	struct crypto_session_params csp;
	struct ovpn_kkey_dir *kdir;
	const char *ciphername;
	enum ovpn_key_cipher cipher;
	const void *key, *iv;
	size_t keylen = 0, ivlen = 0;
	int error;

	if (! nvlist_exists_string(nvl, "cipher"))
		return (EINVAL);
	ciphername = nvlist_get_string(nvl, "cipher");

	if (strcmp(ciphername, "none") == 0)
		cipher = OVPN_CIPHER_ALG_NONE;
	else if (strcmp(ciphername, "AES-256-GCM") == 0 ||
	    strcmp(ciphername, "AES-192-GCM") == 0 ||
	    strcmp(ciphername, "AES-128-GCM") == 0)
		cipher = OVPN_CIPHER_ALG_AES_GCM;
	else if (strcmp(ciphername, "CHACHA20-POLY1305") == 0)
		cipher = OVPN_CIPHER_ALG_CHACHA20_POLY1305;
	else
		return (EINVAL);

	if (cipher != OVPN_CIPHER_ALG_NONE) {
		if (! nvlist_exists_binary(nvl, "key"))
			return (EINVAL);
		key = nvlist_get_binary(nvl, "key", &keylen);
		if (keylen > sizeof(kdir->key))
			return (E2BIG);

		if (! nvlist_exists_binary(nvl, "iv"))
			return (EINVAL);
		iv = nvlist_get_binary(nvl, "iv", &ivlen);
		if (ivlen != 8)
			return (E2BIG);
	}

	kdir = malloc(sizeof(struct ovpn_kkey_dir), M_OVPN,
	    M_WAITOK | M_ZERO);

	kdir->cipher = cipher;
	kdir->keylen = keylen;
	kdir->tx_seq = 1;
	memcpy(kdir->key, key, keylen);
	kdir->noncelen = ivlen;
	memcpy(kdir->nonce, iv, ivlen);

	if (kdir->cipher != OVPN_CIPHER_ALG_NONE) {
		/* Crypto init */
		bzero(&csp, sizeof(csp));
		csp.csp_mode = CSP_MODE_AEAD;

		if (kdir->cipher == OVPN_CIPHER_ALG_CHACHA20_POLY1305)
			csp.csp_cipher_alg = CRYPTO_CHACHA20_POLY1305;
		else
			csp.csp_cipher_alg = CRYPTO_AES_NIST_GCM_16;

		csp.csp_flags |= CSP_F_SEPARATE_AAD;

		csp.csp_cipher_klen = kdir->keylen;
		csp.csp_cipher_key = kdir->key;
		csp.csp_ivlen = 96 / 8;

		error = crypto_newsession(&kdir->cryptoid, &csp,
		    CRYPTOCAP_F_HARDWARE | CRYPTOCAP_F_SOFTWARE);
		if (error) {
			free(kdir, M_OVPN);
			return (error);
		}
	}

	mtx_init(&kdir->replay_mtx, "if_ovpn rx replay", NULL, MTX_DEF);
	*kdirp = kdir;

	return (0);
}

static void
ovpn_free_kkey_dir(struct ovpn_kkey_dir *kdir)
{
	if (kdir == NULL)
		return;

	mtx_destroy(&kdir->replay_mtx);

	crypto_freesession(kdir->cryptoid);
	free(kdir, M_OVPN);
}

static int
ovpn_set_key(struct ifnet *ifp, const nvlist_t *nvl)
{
	struct ovpn_softc *sc = ifp->if_softc;
	struct ovpn_kkey_dir *enc, *dec;
	struct ovpn_kpeer *peer;
	int slot, keyid, peerid;
	int error;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "slot"))
		return (EINVAL);
	slot = nvlist_get_number(nvl, "slot");

	if (! nvlist_exists_number(nvl, "keyid"))
		return (EINVAL);
	keyid = nvlist_get_number(nvl, "keyid");

	if (! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);
	peerid = nvlist_get_number(nvl, "peerid");

	if (slot != OVPN_KEY_SLOT_PRIMARY &&
	    slot != OVPN_KEY_SLOT_SECONDARY)
		return (EINVAL);

	if (! nvlist_exists_nvlist(nvl, "encrypt") ||
	    ! nvlist_exists_nvlist(nvl, "decrypt"))
		return (EINVAL);

	error = ovpn_create_kkey_dir(&enc, nvlist_get_nvlist(nvl, "encrypt"));
	if (error)
		return (error);

	error = ovpn_create_kkey_dir(&dec, nvlist_get_nvlist(nvl, "decrypt"));
	if (error) {
		ovpn_free_kkey_dir(enc);
		return (error);
	}

	OVPN_WLOCK(sc);

	peer = ovpn_find_peer(sc, peerid);
	if (peer == NULL) {
		ovpn_free_kkey_dir(dec);
		ovpn_free_kkey_dir(enc);
		OVPN_WUNLOCK(sc);
		return (ENOENT);
	}

	ovpn_free_kkey_dir(peer->keys[slot].encrypt);
	ovpn_free_kkey_dir(peer->keys[slot].decrypt);

	peer->keys[slot].encrypt = enc;
	peer->keys[slot].decrypt = dec;

	peer->keys[slot].keyid = keyid;
	peer->keys[slot].peerid = peerid;

	OVPN_WUNLOCK(sc);

	return (0);
}

static int
ovpn_check_key(struct ovpn_softc *sc, struct ovpn_kpeer *peer, enum ovpn_key_slot slot)
{
	OVPN_ASSERT(sc);

	if (peer->keys[slot].encrypt == NULL)
		return (ENOLINK);

	if (peer->keys[slot].decrypt == NULL)
		return (ENOLINK);

	return (0);
}

static int
ovpn_start(struct ifnet *ifp)
{
	struct ovpn_softc *sc = ifp->if_softc;

	OVPN_WLOCK(sc);

	ifp->if_flags |= IFF_UP;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	if_link_state_change(ifp, LINK_STATE_UP);

	OVPN_WUNLOCK(sc);

	return (0);
}

static int
ovpn_swap_keys(struct ifnet *ifp, nvlist_t *nvl)
{
	struct ovpn_softc *sc = ifp->if_softc;
	struct ovpn_kpeer *peer;
	struct ovpn_kkey tmpkey;
	int error;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);

	OVPN_WLOCK(sc);

	peer = ovpn_find_peer(sc, nvlist_get_number(nvl, "peerid"));
	if (peer == NULL) {
		OVPN_WUNLOCK(sc);
		return (ENOENT);
	}

	/* Check that we have a second key to swap to. */
	error = ovpn_check_key(sc, peer, OVPN_KEY_SLOT_SECONDARY);
	if (error) {
		OVPN_WUNLOCK(sc);
		return (error);
	}

	tmpkey = peer->keys[0];
	peer->keys[0] = peer->keys[1];
	peer->keys[1] = tmpkey;

	OVPN_WUNLOCK(sc);

	return (0);
}

static int
ovpn_del_key(struct ifnet *ifp, const nvlist_t *nvl)
{
	enum ovpn_key_slot slot;
	struct ovpn_kpeer *peer;
	struct ovpn_softc *sc = ifp->if_softc;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "slot"))
		return (EINVAL);
	slot = nvlist_get_number(nvl, "slot");

	if (slot != OVPN_KEY_SLOT_PRIMARY &&
	    slot != OVPN_KEY_SLOT_SECONDARY)
		return (EINVAL);

	OVPN_WLOCK(sc);

	peer = ovpn_find_peer(sc, nvlist_get_number(nvl, "peerid"));
	if (peer == NULL) {
		OVPN_WUNLOCK(sc);
		return (ENOENT);
	}

	ovpn_free_kkey_dir(peer->keys[slot].encrypt);
	ovpn_free_kkey_dir(peer->keys[slot].decrypt);

	peer->keys[slot].encrypt = NULL;
	peer->keys[slot].decrypt = NULL;

	peer->keys[slot].keyid = 0;
	peer->keys[slot].peerid = 0;

	OVPN_WUNLOCK(sc);

	return (0);
}

static void
ovpn_send_ping(void *arg)
{
	static const uint8_t ping_str[] = {
		0x2a, 0x18, 0x7b, 0xf3, 0x64, 0x1e, 0xb4, 0xcb,
		0x07, 0xed, 0x2d, 0x0a, 0x98, 0x1f, 0xc7, 0x48
	};

	struct epoch_tracker et;
	struct ovpn_kpeer *peer = arg;
	struct ovpn_softc *sc = peer->sc;
	struct mbuf *m;

	OVPN_RASSERT(sc);

	/* Ensure we repeat! */
	callout_reset(&peer->ping_send, peer->keepalive.interval * hz,
	    ovpn_send_ping, peer);

	m = m_get2(sizeof(ping_str), M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return;

	m_copyback(m, 0, sizeof(ping_str), ping_str);
	m->m_len = m->m_pkthdr.len = sizeof(ping_str);

	CURVNET_SET(sc->ifp->if_vnet);
	NET_EPOCH_ENTER(et);
	(void)ovpn_transmit_to_peer(sc->ifp, m, peer, NULL);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
}

static void
ovpn_timeout(void *arg)
{
	struct ovpn_kpeer *peer = arg;
	struct ovpn_softc *sc = peer->sc;
	uint32_t last, _last_active;
	int ret __diagused;
	int cpu;

	OVPN_WASSERT(sc);

	last = 0;
	CPU_FOREACH(cpu) {
		_last_active = *zpcpu_get_cpu(peer->last_active, cpu);
		if (_last_active > last)
			last = _last_active;
	}

	if (last + peer->keepalive.timeout > time_uptime) {
		callout_reset(&peer->ping_rcv,
		    (peer->keepalive.timeout - (time_uptime - last)) * hz,
		    ovpn_timeout, peer);
		return;
	}

	CURVNET_SET(sc->ifp->if_vnet);
	peer->del_reason = OVPN_DEL_REASON_TIMEOUT;
	ret = _ovpn_del_peer(sc, peer);
	MPASS(ret == 0);
	CURVNET_RESTORE();
}

static int
ovpn_set_peer(struct ifnet *ifp, const nvlist_t *nvl)
{
	struct ovpn_softc *sc = ifp->if_softc;
	struct ovpn_kpeer *peer;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "interval") ||
	    ! nvlist_exists_number(nvl, "timeout") ||
	    ! nvlist_exists_number(nvl, "peerid"))
		return (EINVAL);

	OVPN_WLOCK(sc);

	peer = ovpn_find_peer(sc, nvlist_get_number(nvl, "peerid"));
	if (peer == NULL) {
		OVPN_WUNLOCK(sc);
		return (ENOENT);
	}

	peer->keepalive.interval = nvlist_get_number(nvl, "interval");
	peer->keepalive.timeout = nvlist_get_number(nvl, "timeout");

	if (peer->keepalive.interval > 0)
		callout_reset(&peer->ping_send, peer->keepalive.interval * hz,
		    ovpn_send_ping, peer);
	if (peer->keepalive.timeout > 0)
		callout_reset(&peer->ping_rcv, peer->keepalive.timeout * hz,
		    ovpn_timeout, peer);

	OVPN_WUNLOCK(sc);

	return (0);
}

static int
ovpn_set_ifmode(struct ifnet *ifp, const nvlist_t *nvl)
{
	struct ovpn_softc *sc = ifp->if_softc;
	int ifmode;

	if (nvl == NULL)
		return (EINVAL);

	if (! nvlist_exists_number(nvl, "ifmode") )
		return (EINVAL);

	ifmode = nvlist_get_number(nvl, "ifmode");

	OVPN_WLOCK(sc);

	/* deny this if UP */
	if (ifp->if_flags & IFF_UP) {
		OVPN_WUNLOCK(sc);
		return (EBUSY);
	}

	switch (ifmode & ~IFF_MULTICAST) {
	case IFF_POINTOPOINT:
	case IFF_BROADCAST:
		ifp->if_flags &=
		    ~(IFF_BROADCAST|IFF_POINTOPOINT|IFF_MULTICAST);
		ifp->if_flags |= ifmode;
		break;
	default:
		OVPN_WUNLOCK(sc);
		return (EINVAL);
	}

	OVPN_WUNLOCK(sc);

	return (0);
}

static int
ovpn_ioctl_set(struct ifnet *ifp, struct ifdrv *ifd)
{
	struct ovpn_softc *sc = ifp->if_softc;
	uint8_t *buf = NULL;
	nvlist_t *nvl = NULL;
	int ret;

	if (ifd->ifd_len != 0) {
		if (ifd->ifd_len > OVPN_MAX_REQUEST_SIZE)
			return (E2BIG);

		buf = malloc(ifd->ifd_len, M_OVPN, M_WAITOK);

		ret = copyin(ifd->ifd_data, buf, ifd->ifd_len);
		if (ret != 0) {
			free(buf, M_OVPN);
			return (ret);
		}

		nvl = nvlist_unpack(buf, ifd->ifd_len, 0);
		free(buf, M_OVPN);
		if (nvl == NULL) {
			return (EINVAL);
		}
	}

	switch (ifd->ifd_cmd) {
	case OVPN_NEW_PEER:
		ret = ovpn_new_peer(ifp, nvl);
		break;
	case OVPN_DEL_PEER:
		OVPN_WLOCK(sc);
		ret = ovpn_del_peer(ifp, nvl);
		OVPN_WUNLOCK(sc);
		break;
	case OVPN_NEW_KEY:
		ret = ovpn_set_key(ifp, nvl);
		break;
	case OVPN_START_VPN:
		ret = ovpn_start(ifp);
		break;
	case OVPN_SWAP_KEYS:
		ret = ovpn_swap_keys(ifp, nvl);
		break;
	case OVPN_DEL_KEY:
		ret = ovpn_del_key(ifp, nvl);
		break;
	case OVPN_SET_PEER:
		ret = ovpn_set_peer(ifp, nvl);
		break;
	case OVPN_SET_IFMODE:
		ret = ovpn_set_ifmode(ifp, nvl);
		break;
	default:
		ret = ENOTSUP;
	}

	nvlist_destroy(nvl);
	return (ret);
}

static int
ovpn_add_counters(nvlist_t *parent, const char *name, counter_u64_t in,
    counter_u64_t out)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

	nvlist_add_number(nvl, "in", counter_u64_fetch(in));
	nvlist_add_number(nvl, "out", counter_u64_fetch(out));

	nvlist_add_nvlist(parent, name, nvl);

	nvlist_destroy(nvl);

	return (0);
}

static int
ovpn_get_stats(struct ovpn_softc *sc, nvlist_t **onvl)
{
	nvlist_t *nvl;
	int ret;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

#define OVPN_COUNTER_OUT(name, in, out) \
	do { \
		ret = ovpn_add_counters(nvl, name, OVPN_COUNTER(sc, in), \
		    OVPN_COUNTER(sc, out)); \
		if (ret != 0) \
			goto error; \
	} while(0)

	OVPN_COUNTER_OUT("lost_ctrl", lost_ctrl_pkts_in, lost_ctrl_pkts_out);
	OVPN_COUNTER_OUT("lost_data", lost_data_pkts_in, lost_data_pkts_out);
	OVPN_COUNTER_OUT("nomem_data", nomem_data_pkts_in,
	    nomem_data_pkts_out);
	OVPN_COUNTER_OUT("data", received_data_pkts, sent_data_pkts);
	OVPN_COUNTER_OUT("ctrl", received_ctrl_pkts, sent_ctrl_pkts);
	OVPN_COUNTER_OUT("tunnel", tunnel_bytes_received,
	    tunnel_bytes_received);
	OVPN_COUNTER_OUT("transport", transport_bytes_received,
	    transport_bytes_received);
#undef OVPN_COUNTER_OUT

	*onvl = nvl;

	return (0);

error:
	nvlist_destroy(nvl);
	return (ret);
}

static int
ovpn_get_peer_stats(struct ovpn_softc *sc, nvlist_t **nvl)
{
	struct ovpn_kpeer *peer;
	nvlist_t *nvpeer = NULL;
	int ret;

	OVPN_RLOCK_TRACKER;

	*nvl = nvlist_create(0);
	if (*nvl == NULL)
		return (ENOMEM);

#define OVPN_PEER_COUNTER_OUT(name, in, out) \
	do { \
		ret = ovpn_add_counters(nvpeer, name, \
		    OVPN_PEER_COUNTER(peer, in), OVPN_PEER_COUNTER(peer, out)); \
		if (ret != 0) \
			goto error; \
	} while(0)

	OVPN_RLOCK(sc);
	RB_FOREACH(peer, ovpn_kpeers, &sc->peers) {
		nvpeer = nvlist_create(0);
		if (nvpeer == NULL) {
			OVPN_RUNLOCK(sc);
			nvlist_destroy(*nvl);
			*nvl = NULL;
			return (ENOMEM);
		}

		nvlist_add_number(nvpeer, "peerid", peer->peerid);

		OVPN_PEER_COUNTER_OUT("packets", pkt_in, pkt_out);
		OVPN_PEER_COUNTER_OUT("bytes", bytes_in, bytes_out);

		nvlist_append_nvlist_array(*nvl, "peers", nvpeer);
		nvlist_destroy(nvpeer);
	}
#undef OVPN_PEER_COUNTER_OUT
	OVPN_RUNLOCK(sc);

	return (0);

error:
	nvlist_destroy(nvpeer);
	nvlist_destroy(*nvl);
	*nvl = NULL;
	return (ret);
}

static int
ovpn_poll_pkt(struct ovpn_softc *sc, nvlist_t **onvl)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

	nvlist_add_number(nvl, "pending", buf_ring_count(sc->notifring));

	*onvl = nvl;

	return (0);
}

static void
ovpn_notif_add_counters(nvlist_t *parent, struct ovpn_notification *n)
{
	nvlist_t *nvl;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return;

	nvlist_add_number(nvl, "in", n->counters.pkt_in);
	nvlist_add_number(nvl, "out", n->counters.pkt_out);

	nvlist_add_nvlist(parent, "packets", nvl);
	nvlist_destroy(nvl);

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return;

	nvlist_add_number(nvl, "in", n->counters.bytes_in);
	nvlist_add_number(nvl, "out", n->counters.bytes_out);

	nvlist_add_nvlist(parent, "bytes", nvl);
	nvlist_destroy(nvl);
}

static int
opvn_get_pkt(struct ovpn_softc *sc, nvlist_t **onvl)
{
	struct ovpn_notification *n;
	nvlist_t *nvl;

	/* Check if we have notifications pending. */
	n = buf_ring_dequeue_mc(sc->notifring);
	if (n == NULL)
		return (ENOENT);

	nvl = nvlist_create(0);
	if (nvl == NULL) {
		free(n, M_OVPN);
		return (ENOMEM);
	}
	nvlist_add_number(nvl, "peerid", n->peerid);
	nvlist_add_number(nvl, "notification", n->type);
	if (n->type == OVPN_NOTIF_DEL_PEER) {
		nvlist_add_number(nvl, "del_reason", n->del_reason);

		/* No error handling, because we want to send the notification
		 * even if we can't attach the counters. */
		ovpn_notif_add_counters(nvl, n);
	}
	free(n, M_OVPN);

	*onvl = nvl;

	return (0);
}

static int
ovpn_ioctl_get(struct ifnet *ifp, struct ifdrv *ifd)
{
	struct ovpn_softc *sc = ifp->if_softc;
	nvlist_t *nvl = NULL;
	int error;

	switch (ifd->ifd_cmd) {
	case OVPN_GET_STATS:
		error = ovpn_get_stats(sc, &nvl);
		break;
	case OVPN_GET_PEER_STATS:
		error = ovpn_get_peer_stats(sc, &nvl);
		break;
	case OVPN_POLL_PKT:
		error = ovpn_poll_pkt(sc, &nvl);
		break;
	case OVPN_GET_PKT:
		error = opvn_get_pkt(sc, &nvl);
		break;
	default:
		error = ENOTSUP;
		break;
	}

	if (error == 0) {
		void *packed = NULL;
		size_t len;

		MPASS(nvl != NULL);

		packed = nvlist_pack(nvl, &len);
		if (! packed) {
			nvlist_destroy(nvl);
			return (ENOMEM);
		}

		if (len > ifd->ifd_len) {
			free(packed, M_NVLIST);
			nvlist_destroy(nvl);
			return (ENOSPC);
		}

		error = copyout(packed, ifd->ifd_data, len);
		ifd->ifd_len = len;

		free(packed, M_NVLIST);
		nvlist_destroy(nvl);
	}

	return (error);
}

static int
ovpn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifdrv *ifd;
	int error;

	CURVNET_ASSERT_SET();

	switch (cmd) {
	case SIOCSDRVSPEC:
	case SIOCGDRVSPEC:
		error = priv_check(curthread, PRIV_NET_OVPN);
		if (error)
			return (error);
		break;
	}

	switch (cmd) {
	case SIOCSDRVSPEC:
		ifd = (struct ifdrv *)data;
		error = ovpn_ioctl_set(ifp, ifd);
		break;
	case SIOCGDRVSPEC:
		ifd = (struct ifdrv *)data;
		error = ovpn_ioctl_get(ifp, ifd);
		break;
	case SIOCSIFMTU: {
		struct ifreq *ifr = (struct ifreq *)data;
		if (ifr->ifr_mtu < OVPN_MTU_MIN || ifr->ifr_mtu > OVPN_MTU_MAX)
			return (EINVAL);

		ifp->if_mtu = ifr->ifr_mtu;
		return (0);
	}
	case SIOCSIFADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCGIFMTU:
	case SIOCSIFFLAGS:
		return (0);
	default:
		error = EINVAL;
	}

	return (error);
}

static int
ovpn_encrypt_tx_cb(struct cryptop *crp)
{
	struct epoch_tracker et;
	struct ovpn_kpeer *peer = crp->crp_opaque;
	struct ovpn_softc *sc = peer->sc;
	struct mbuf *m = crp->crp_buf.cb_mbuf;
	int tunnel_len;
	int ret;

	CURVNET_SET(sc->ifp->if_vnet);
	NET_EPOCH_ENTER(et);

	if (crp->crp_etype != 0) {
		crypto_freereq(crp);
		ovpn_peer_release_ref(peer, false);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();
		OVPN_COUNTER_ADD(sc, lost_data_pkts_out, 1);
		m_freem(m);
		return (0);
	}

	MPASS(crp->crp_buf.cb_type == CRYPTO_BUF_MBUF);

	tunnel_len = m->m_pkthdr.len - sizeof(struct ovpn_wire_header);
	ret = ovpn_encap(sc, peer->peerid, m);
	if (ret == 0) {
		OVPN_COUNTER_ADD(sc, sent_data_pkts, 1);
		OVPN_COUNTER_ADD(sc, tunnel_bytes_sent, tunnel_len);
	}

	crypto_freereq(crp);
	ovpn_peer_release_ref(peer, false);

	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();

	return (0);
}

static void
ovpn_finish_rx(struct ovpn_softc *sc, struct mbuf *m,
    struct ovpn_kpeer *peer, struct ovpn_kkey *key, uint32_t seq,
    struct rm_priotracker *_ovpn_lock_trackerp)
{
	uint32_t af;

	OVPN_RASSERT(sc);
	NET_EPOCH_ASSERT();

	/* Replay protection. */
	if (V_replay_protection && ! ovpn_check_replay(key->decrypt, seq)) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		m_freem(m);
		return;
	}

	critical_enter();
	*zpcpu_get(peer->last_active) = time_uptime;
	critical_exit();

	OVPN_RUNLOCK(sc);

	OVPN_COUNTER_ADD(sc, received_data_pkts, 1);
	OVPN_COUNTER_ADD(sc, tunnel_bytes_received, m->m_pkthdr.len);
	OVPN_PEER_COUNTER_ADD(peer, pkt_in, 1);
	OVPN_PEER_COUNTER_ADD(peer, bytes_in, m->m_pkthdr.len);

	/* Receive the packet on our interface. */
	m->m_pkthdr.rcvif = sc->ifp;

	/* Clear checksum flags in case the real hardware set them. */
	m->m_pkthdr.csum_flags = 0;

	/* Ensure we can read the first byte. */
	m = m_pullup(m, 1);
	if (m == NULL) {
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_in, 1);
		return;
	}

	/*
	 * Check for address family, and disregard any control packets (e.g.
	 * keepalive).
	 */
	af = ovpn_get_af(m);
	if (af != 0) {
		BPF_MTAP2(sc->ifp, &af, sizeof(af), m);
		if (V_async_netisr_queue)
			netisr_queue(af == AF_INET ? NETISR_IP : NETISR_IPV6, m);
		else
			netisr_dispatch(af == AF_INET ? NETISR_IP : NETISR_IPV6, m);
	} else {
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		m_freem(m);
	}
}

static struct ovpn_kkey *
ovpn_find_key(struct ovpn_softc *sc, struct ovpn_kpeer *peer,
    const struct ovpn_wire_header *ohdr)
{
	struct ovpn_kkey *key = NULL;
	uint8_t keyid;

	OVPN_RASSERT(sc);

	keyid = (ntohl(ohdr->opcode) >> 24) & 0x07;

	if (peer->keys[0].keyid == keyid)
		key = &peer->keys[0];
	else if (peer->keys[1].keyid == keyid)
		key = &peer->keys[1];

	return (key);
}

static int
ovpn_decrypt_rx_cb(struct cryptop *crp)
{
	struct epoch_tracker et;
	struct ovpn_softc *sc = crp->crp_opaque;
	struct mbuf *m = crp->crp_buf.cb_mbuf;
	struct ovpn_kkey *key;
	struct ovpn_kpeer *peer;
	struct ovpn_wire_header *ohdr;
	uint32_t peerid;

	OVPN_RLOCK_TRACKER;

	OVPN_RLOCK(sc);

	MPASS(crp->crp_buf.cb_type == CRYPTO_BUF_MBUF);

	if (crp->crp_etype != 0) {
		crypto_freereq(crp);
		atomic_add_int(&sc->refcount, -1);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		OVPN_RUNLOCK(sc);
		m_freem(m);
		return (0);
	}

	CURVNET_SET(sc->ifp->if_vnet);

	ohdr = mtodo(m, sizeof(struct udphdr));

	peerid = ntohl(ohdr->opcode) & 0x00ffffff;
	peer = ovpn_find_peer(sc, peerid);
	if (peer == NULL) {
		/* No such peer. Drop packet. */
		crypto_freereq(crp);
		atomic_add_int(&sc->refcount, -1);
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		m_freem(m);
		CURVNET_RESTORE();
		return (0);
	}

	key = ovpn_find_key(sc, peer, ohdr);
	if (key == NULL) {
		crypto_freereq(crp);
		atomic_add_int(&sc->refcount, -1);
		/*
		 * Has this key been removed between us starting the decrypt
		 * and finishing it?
		 */
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		m_freem(m);
		CURVNET_RESTORE();
		return (0);
	}

	/* Now remove the outer headers */
	m_adj_decap(m, sizeof(struct udphdr) +
	    sizeof(struct ovpn_wire_header));

	NET_EPOCH_ENTER(et);
	ovpn_finish_rx(sc, m, peer, key, ntohl(ohdr->seq), _ovpn_lock_trackerp);
	NET_EPOCH_EXIT(et);
	OVPN_UNLOCK_ASSERT(sc);

	CURVNET_RESTORE();

	crypto_freereq(crp);
	atomic_add_int(&sc->refcount, -1);

	return (0);
}

static int
ovpn_get_af(struct mbuf *m)
{
	struct ip *ip;
	struct ip6_hdr *ip6;

	/*
	 * We should pullup, but we're only interested in the first byte, so
	 * that'll always be contiguous.
	 */
	ip = mtod(m, struct ip *);
	if (ip->ip_v == IPVERSION)
		return (AF_INET);

	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION)
		return (AF_INET6);

	return (0);
}

#ifdef INET
static struct ovpn_kpeer *
ovpn_find_peer_by_ip(struct ovpn_softc *sc, const struct in_addr addr)
{
	struct ovpn_kpeer *peer = NULL;

	OVPN_ASSERT(sc);

	/* TODO: Add a second RB so we can look up by IP. */
	RB_FOREACH(peer, ovpn_kpeers, &sc->peers) {
		if (addr.s_addr == peer->vpn4.s_addr)
			return (peer);
	}

	return (peer);
}
#endif

#ifdef INET6
static struct ovpn_kpeer *
ovpn_find_peer_by_ip6(struct ovpn_softc *sc, const struct in6_addr *addr)
{
	struct ovpn_kpeer *peer = NULL;

	OVPN_ASSERT(sc);

	/* TODO: Add a third RB so we can look up by IPv6 address. */
	RB_FOREACH(peer, ovpn_kpeers, &sc->peers) {
		if (memcmp(addr, &peer->vpn6, sizeof(*addr)) == 0)
			return (peer);
	}

	return (peer);
}
#endif

static struct ovpn_kpeer *
ovpn_route_peer(struct ovpn_softc *sc, struct mbuf **m0,
    const struct sockaddr *dst)
{
	struct ovpn_kpeer *peer = NULL;
	int af;

	NET_EPOCH_ASSERT();
	OVPN_ASSERT(sc);

	/* Shortcut if we're a client (or are a server and have only one client). */
	if (sc->peercount == 1)
		return (ovpn_find_only_peer(sc));

	if (dst != NULL)
		af = dst->sa_family;
	else
		af = ovpn_get_af(*m0);

	switch (af) {
#ifdef INET
	case AF_INET: {
		const struct sockaddr_in *sa = (const struct sockaddr_in *)dst;
		struct nhop_object *nh;
		const struct in_addr *ip_dst;

		if (sa != NULL) {
			ip_dst = &sa->sin_addr;
		} else {
			struct ip *ip;

			*m0 = m_pullup(*m0, sizeof(struct ip));
			if (*m0 == NULL)
				return (NULL);
			ip = mtod(*m0, struct ip *);
			ip_dst = &ip->ip_dst;
		}

		peer = ovpn_find_peer_by_ip(sc, *ip_dst);
		SDT_PROBE2(if_ovpn, tx, route, ip4, ip_dst, peer);
		if (peer == NULL) {
			nh = fib4_lookup(M_GETFIB(*m0), *ip_dst, 0,
			    NHR_NONE, 0);
			if (nh && (nh->nh_flags & NHF_GATEWAY)) {
				peer = ovpn_find_peer_by_ip(sc,
				    nh->gw4_sa.sin_addr);
				SDT_PROBE2(if_ovpn, tx, route, ip4,
				    &nh->gw4_sa.sin_addr, peer);
			}
		}
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		const struct sockaddr_in6 *sa6 =
		    (const struct sockaddr_in6 *)dst;
		struct nhop_object *nh;
		const struct in6_addr *ip6_dst;

		if (sa6 != NULL) {
			ip6_dst = &sa6->sin6_addr;
		} else {
			struct ip6_hdr *ip6;

			*m0 = m_pullup(*m0, sizeof(struct ip6_hdr));
			if (*m0 == NULL)
				return (NULL);
			ip6 = mtod(*m0, struct ip6_hdr *);
			ip6_dst = &ip6->ip6_dst;
		}

		peer = ovpn_find_peer_by_ip6(sc, ip6_dst);
		SDT_PROBE2(if_ovpn, tx, route, ip6, ip6_dst, peer);
		if (peer == NULL) {
			nh = fib6_lookup(M_GETFIB(*m0), ip6_dst, 0,
			    NHR_NONE, 0);
			if (nh && (nh->nh_flags & NHF_GATEWAY)) {
				peer = ovpn_find_peer_by_ip6(sc,
				    &nh->gw6_sa.sin6_addr);
				SDT_PROBE2(if_ovpn, tx, route, ip6,
				    &nh->gw6_sa.sin6_addr, peer);
			}
		}
		break;
	}
#endif
	}

	return (peer);
}

static int
ovpn_transmit(struct ifnet *ifp, struct mbuf *m)
{
	return (ifp->if_output(ifp, m, NULL, NULL));
}

static int
ovpn_transmit_to_peer(struct ifnet *ifp, struct mbuf *m,
    struct ovpn_kpeer *peer, struct rm_priotracker *_ovpn_lock_trackerp)
{
	struct ovpn_wire_header *ohdr;
	struct ovpn_kkey *key;
	struct ovpn_softc *sc;
	struct cryptop *crp;
	uint32_t af, seq;
	size_t len, ovpn_hdr_len;
	int tunnel_len;
	int ret;

	sc = ifp->if_softc;

	OVPN_RASSERT(sc);

	tunnel_len = m->m_pkthdr.len;

	key = &peer->keys[OVPN_KEY_SLOT_PRIMARY];
	if (key->encrypt == NULL) {
		if (_ovpn_lock_trackerp != NULL)
			OVPN_RUNLOCK(sc);
		m_freem(m);
		return (ENOLINK);
	}

	af = ovpn_get_af(m);
	/* Don't capture control packets. */
	if (af != 0)
		BPF_MTAP2(ifp, &af, sizeof(af), m);

	len = m->m_pkthdr.len;
	MPASS(len <= ifp->if_mtu);

	ovpn_hdr_len = sizeof(struct ovpn_wire_header);
	if (key->encrypt->cipher == OVPN_CIPHER_ALG_NONE)
		ovpn_hdr_len -= 16; /* No auth tag. */

	M_PREPEND(m, ovpn_hdr_len, M_NOWAIT);
	if (m == NULL) {
		if (_ovpn_lock_trackerp != NULL)
			OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
		return (ENOBUFS);
	}
	ohdr = mtod(m, struct ovpn_wire_header *);
	ohdr->opcode = (OVPN_OP_DATA_V2 << OVPN_OP_SHIFT) | key->keyid;
	ohdr->opcode <<= 24;
	ohdr->opcode |= key->peerid;
	ohdr->opcode = htonl(ohdr->opcode);

	seq = atomic_fetchadd_32(&peer->keys[OVPN_KEY_SLOT_PRIMARY].encrypt->tx_seq, 1);
	if (seq == OVPN_SEQ_ROTATE)
		ovpn_notify_key_rotation(sc, peer);

	seq = htonl(seq);
	ohdr->seq = seq;

	OVPN_PEER_COUNTER_ADD(peer, pkt_out, 1);
	OVPN_PEER_COUNTER_ADD(peer, bytes_out, len);

	if (key->encrypt->cipher == OVPN_CIPHER_ALG_NONE) {
		ret = ovpn_encap(sc, peer->peerid, m);
		if (_ovpn_lock_trackerp != NULL)
			OVPN_RUNLOCK(sc);
		if (ret == 0) {
			OVPN_COUNTER_ADD(sc, sent_data_pkts, 1);
			OVPN_COUNTER_ADD(sc, tunnel_bytes_sent, tunnel_len);
		}
		return (ret);
	}

	crp = crypto_getreq(key->encrypt->cryptoid, M_NOWAIT);
	if (crp == NULL) {
		if (_ovpn_lock_trackerp != NULL)
			OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
		m_freem(m);
		return (ENOBUFS);
	}

	/* Encryption covers only the payload, not the header. */
	crp->crp_payload_start = sizeof(*ohdr);
	crp->crp_payload_length = len;
	crp->crp_op = CRYPTO_OP_ENCRYPT;

	/*
	 * AAD data covers the ovpn_wire_header minus the auth
	 * tag.
	 */
	crp->crp_aad_length = sizeof(*ohdr) - sizeof(ohdr->auth_tag);
	crp->crp_aad = ohdr;
	crp->crp_aad_start = 0;
	crp->crp_op |= CRYPTO_OP_COMPUTE_DIGEST;
	crp->crp_digest_start = offsetof(struct ovpn_wire_header, auth_tag);

	crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	memcpy(crp->crp_iv, &seq, sizeof(seq));
	memcpy(crp->crp_iv + sizeof(seq), key->encrypt->nonce,
	    key->encrypt->noncelen);

	crypto_use_mbuf(crp, m);
	crp->crp_flags |= CRYPTO_F_CBIFSYNC;
	crp->crp_callback = ovpn_encrypt_tx_cb;
	crp->crp_opaque = peer;

	atomic_add_int(&peer->refcount, 1);
	if (_ovpn_lock_trackerp != NULL)
		OVPN_RUNLOCK(sc);
	if (V_async_crypto)
		ret = crypto_dispatch_async(crp, CRYPTO_ASYNC_ORDERED);
	else
		ret = crypto_dispatch(crp);
	if (ret) {
		OVPN_COUNTER_ADD(sc, lost_data_pkts_out, 1);
	}

	return (ret);
}

/*
 * Note: Expects to hold the read lock on entry, and will release it itself.
 */
static int
ovpn_encap(struct ovpn_softc *sc, uint32_t peerid, struct mbuf *m)
{
	struct udphdr *udp;
	struct ovpn_kpeer *peer;
	int len;

	OVPN_RLOCK_TRACKER;

	OVPN_RLOCK(sc);
	NET_EPOCH_ASSERT();

	peer = ovpn_find_peer(sc, peerid);
	if (peer == NULL || sc->ifp->if_link_state != LINK_STATE_UP) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_out, 1);
		m_freem(m);
		return (ENETDOWN);
	}

	len = m->m_pkthdr.len;

	M_PREPEND(m, sizeof(struct udphdr), M_NOWAIT);
	if (m == NULL) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
		m_freem(m);
		return (ENOBUFS);
	}
	udp = mtod(m, struct udphdr *);

	MPASS(peer->local.ss_family == peer->remote.ss_family);

	udp->uh_sport = ovpn_get_port(&peer->local);
	udp->uh_dport = ovpn_get_port(&peer->remote);
	udp->uh_ulen = htons(sizeof(struct udphdr) + len);

	switch (peer->remote.ss_family) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *in_local = TO_IN(&peer->local);
		struct sockaddr_in *in_remote = TO_IN(&peer->remote);
		struct ip *ip;

		/*
		 * This requires knowing the source IP, which we don't. Happily
		 * we're allowed to keep this at 0, and the checksum won't do
		 * anything the crypto won't already do.
		 */
		udp->uh_sum = 0;

		/* Set the checksum flags so we recalculate checksums. */
		m->m_pkthdr.csum_flags |= CSUM_IP;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);

		M_PREPEND(m, sizeof(struct ip), M_NOWAIT);
		if (m == NULL) {
			OVPN_RUNLOCK(sc);
			OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
			return (ENOBUFS);
		}
		ip = mtod(m, struct ip *);

		ip->ip_tos = 0;
		ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) +
		   len);
		ip->ip_off = 0;
		ip->ip_ttl = V_ip_defttl;
		ip->ip_p = IPPROTO_UDP;
		ip->ip_sum = 0;
		if (in_local->sin_port != 0)
			ip->ip_src = in_local->sin_addr;
		else
			ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst = in_remote->sin_addr;

		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, transport_bytes_sent, m->m_pkthdr.len);

		return (ip_output(m, NULL, NULL, 0, NULL, NULL));
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *in6_local = TO_IN6(&peer->local);
		struct sockaddr_in6 *in6_remote = TO_IN6(&peer->remote);
		struct ip6_hdr *ip6;

		M_PREPEND(m, sizeof(struct ip6_hdr), M_NOWAIT);
		if (m == NULL) {
			OVPN_RUNLOCK(sc);
			OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
			return (ENOBUFS);
		}
		m = m_pullup(m, sizeof(*ip6) + sizeof(*udp));
		if (m == NULL) {
			OVPN_RUNLOCK(sc);
			OVPN_COUNTER_ADD(sc, nomem_data_pkts_out, 1);
			return (ENOBUFS);
		}

		ip6 = mtod(m, struct ip6_hdr *);

		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_flow &= ~IPV6_FLOWINFO_MASK;
		ip6->ip6_plen = htons(sizeof(*ip6) + sizeof(struct udphdr) +
		    len);
		ip6->ip6_nxt = IPPROTO_UDP;
		ip6->ip6_hlim = V_ip6_defhlim;

		memcpy(&ip6->ip6_src, &in6_local->sin6_addr,
		    sizeof(ip6->ip6_src));
		memcpy(&ip6->ip6_dst, &in6_remote->sin6_addr,
		    sizeof(ip6->ip6_dst));

		udp = mtodo(m, sizeof(*ip6));
		udp->uh_sum = in6_cksum_pseudo(ip6,
		    m->m_pkthdr.len - sizeof(struct ip6_hdr),
		    IPPROTO_UDP, 0);

		m->m_pkthdr.csum_flags |= CSUM_UDP_IPV6;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);

		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, transport_bytes_sent, m->m_pkthdr.len);

		return (ip6_output(m, NULL, NULL, IPV6_UNSPECSRC, NULL, NULL,
		    NULL));
	}
#endif
	default:
		panic("Unsupported address family %d",
		    peer->remote.ss_family);
	}
}

static int
ovpn_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	struct route *ro)
{
	struct ovpn_softc *sc;
	struct ovpn_kpeer *peer;

	OVPN_RLOCK_TRACKER;

	sc = ifp->if_softc;

	OVPN_RLOCK(sc);

	SDT_PROBE1(if_ovpn, tx, transmit, start, m);

	if (__predict_false(ifp->if_link_state != LINK_STATE_UP)) {
		OVPN_COUNTER_ADD(sc, lost_data_pkts_out, 1);
		OVPN_RUNLOCK(sc);
		m_freem(m);
		return (ENETDOWN);
	}

	/**
	 * Only obey 'dst' (i.e. the gateway) if no route is supplied.
	 * That's our indication that we're being called through pf's route-to,
	 * and we should route according to 'dst' instead. We can't do so
	 * consistently, because the usual openvpn configuration sets the first
	 * non-server IP in the subnet as the gateway. If we always use that
	 * one we'd end up routing all traffic to the first client.
	 * tl;dr: 'ro == NULL' tells us pf is doing a route-to, and then but
	 * only then, we should treat 'dst' as the destination. */
	peer = ovpn_route_peer(sc, &m, ro == NULL ? dst : NULL);
	if (peer == NULL) {
		/* No destination. */
		OVPN_COUNTER_ADD(sc, lost_data_pkts_out, 1);
		OVPN_RUNLOCK(sc);
		m_freem(m);
		return (ENETDOWN);
	}

	return (ovpn_transmit_to_peer(ifp, m, peer, _ovpn_lock_trackerp));
}

static bool
ovpn_check_replay(struct ovpn_kkey_dir *key, uint32_t seq)
{
	uint32_t d;

	mtx_lock(&key->replay_mtx);

	/* Sequence number must be strictly greater than rx_seq */
	if (seq <= key->rx_seq) {
		mtx_unlock(&key->replay_mtx);
		return (false);
	}

	/* Large jump. The packet authenticated okay, so just accept that. */
	if (seq > (key->rx_seq + (sizeof(key->rx_window) * 8))) {
		key->rx_seq = seq;
		key->rx_window = 0;
		mtx_unlock(&key->replay_mtx);
		return (true);
	}

	/* Happy case. */
	if ((seq == key->rx_seq + 1) && key->rx_window == 0) {
		key->rx_seq++;
		mtx_unlock(&key->replay_mtx);
		return (true);
	}

	d = seq - key->rx_seq - 1;

	if (key->rx_window & ((uint64_t)1 << d)) {
		/* Dupe! */
		mtx_unlock(&key->replay_mtx);
		return (false);
	}

	key->rx_window |= (uint64_t)1 << d;

	while (key->rx_window & 1) {
		key->rx_seq++;
		key->rx_window >>= 1;
	}

	mtx_unlock(&key->replay_mtx);

	return (true);
}

static struct ovpn_kpeer *
ovpn_peer_from_mbuf(struct ovpn_softc *sc, struct mbuf *m, int off)
{
	struct ovpn_wire_header ohdr;
	uint32_t peerid;
	const size_t hdrlen = sizeof(ohdr) - sizeof(ohdr.auth_tag);

	OVPN_RASSERT(sc);

	if (m_length(m, NULL) < (off + sizeof(struct udphdr) + hdrlen))
		return (NULL);

	m_copydata(m, off + sizeof(struct udphdr), hdrlen, (caddr_t)&ohdr);

	peerid = ntohl(ohdr.opcode) & 0x00ffffff;

	return (ovpn_find_peer(sc, peerid));
}

static bool
ovpn_udp_input(struct mbuf *m, int off, struct inpcb *inp,
    const struct sockaddr *sa, void *ctx)
{
	struct ovpn_softc *sc = ctx;
	struct ovpn_wire_header tmphdr;
	struct ovpn_wire_header *ohdr;
	struct udphdr *uhdr;
	struct ovpn_kkey *key;
	struct cryptop *crp;
	struct ovpn_kpeer *peer;
	size_t ohdrlen;
	int ret;
	uint8_t op;

	OVPN_RLOCK_TRACKER;

	M_ASSERTPKTHDR(m);

	OVPN_COUNTER_ADD(sc, transport_bytes_received, m->m_pkthdr.len - off);

	ohdrlen = sizeof(*ohdr) - sizeof(ohdr->auth_tag);

	OVPN_RLOCK(sc);

	peer = ovpn_peer_from_mbuf(sc, m, off);
	if (peer == NULL) {
		OVPN_RUNLOCK(sc);
		return (false);
	}

	if (m_length(m, NULL) < (off + sizeof(*uhdr) + ohdrlen)) {
		/* Short packet. */
		OVPN_RUNLOCK(sc);
		return (false);
	}

	m_copydata(m, off + sizeof(*uhdr), ohdrlen, (caddr_t)&tmphdr);

	op = ntohl(tmphdr.opcode) >> 24 >> OVPN_OP_SHIFT;
	if (op != OVPN_OP_DATA_V2) {
		/* Control packet? */
		OVPN_RUNLOCK(sc);
		return (false);
	}

	m = m_pullup(m, off + sizeof(*uhdr) + ohdrlen);
	if (m == NULL) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_in, 1);
		return (true);
	}

	/*
	 * Simplify things by getting rid of the preceding headers, we don't
	 * care about them.
	 */
	m_adj_decap(m, off);

	uhdr = mtodo(m, 0);
	ohdr = mtodo(m, sizeof(*uhdr));

	key = ovpn_find_key(sc, peer, ohdr);
	if (key == NULL || key->decrypt == NULL) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
		m_freem(m);
		return (true);
	}

	if (key->decrypt->cipher == OVPN_CIPHER_ALG_NONE) {
		/* Now remove the outer headers */
		m_adj_decap(m, sizeof(struct udphdr) + ohdrlen);

		ohdr = mtodo(m, sizeof(*uhdr));

		ovpn_finish_rx(sc, m, peer, key, ntohl(ohdr->seq),
		    _ovpn_lock_trackerp);
		OVPN_UNLOCK_ASSERT(sc);
		return (true);
	}

	ohdrlen += sizeof(ohdr->auth_tag);

	m = m_pullup(m, sizeof(*uhdr) + ohdrlen);
	if (m == NULL) {
		OVPN_RUNLOCK(sc);
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_in, 1);
		return (true);
	}
	uhdr = mtodo(m, 0);
	ohdr = mtodo(m, sizeof(*uhdr));

	/* Decrypt */
	crp = crypto_getreq(key->decrypt->cryptoid, M_NOWAIT);
	if (crp == NULL) {
		OVPN_COUNTER_ADD(sc, nomem_data_pkts_in, 1);
		OVPN_RUNLOCK(sc);
		m_freem(m);
		return (true);
	}

	crp->crp_payload_start = sizeof(struct udphdr) + sizeof(*ohdr);
	crp->crp_payload_length = ntohs(uhdr->uh_ulen) -
	    sizeof(*uhdr) - sizeof(*ohdr);
	crp->crp_op = CRYPTO_OP_DECRYPT;

	/* AAD validation. */
	crp->crp_aad_length = sizeof(*ohdr) - sizeof(ohdr->auth_tag);
	crp->crp_aad = ohdr;
	crp->crp_aad_start = 0;
	crp->crp_op |= CRYPTO_OP_VERIFY_DIGEST;
	crp->crp_digest_start = sizeof(struct udphdr) +
	    offsetof(struct ovpn_wire_header, auth_tag);

	crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	memcpy(crp->crp_iv, &ohdr->seq, sizeof(ohdr->seq));
	memcpy(crp->crp_iv + sizeof(ohdr->seq), key->decrypt->nonce,
	    key->decrypt->noncelen);

	crypto_use_mbuf(crp, m);
	crp->crp_flags |= CRYPTO_F_CBIFSYNC;
	crp->crp_callback = ovpn_decrypt_rx_cb;
	crp->crp_opaque = sc;

	atomic_add_int(&sc->refcount, 1);
	OVPN_RUNLOCK(sc);
	if (V_async_crypto)
		ret = crypto_dispatch_async(crp, CRYPTO_ASYNC_ORDERED);
	else
		ret = crypto_dispatch(crp);
	if (ret != 0) {
		OVPN_COUNTER_ADD(sc, lost_data_pkts_in, 1);
	}

	return (true);
}

static void
ovpn_qflush(struct ifnet *ifp __unused)
{

}

static void
ovpn_flush_rxring(struct ovpn_softc *sc)
{
	struct ovpn_notification *n;

	OVPN_WASSERT(sc);

	while (! buf_ring_empty(sc->notifring)) {
		n = buf_ring_dequeue_sc(sc->notifring);
		free(n, M_OVPN);
	}
}

#ifdef VIMAGE
static void
ovpn_reassign(struct ifnet *ifp, struct vnet *new_vnet __unused,
    char *unused __unused)
{
	struct ovpn_softc *sc = ifp->if_softc;
	struct ovpn_kpeer *peer, *tmppeer;
	int ret __diagused;

	OVPN_WLOCK(sc);

	/* Flush keys & configuration. */
	RB_FOREACH_SAFE(peer, ovpn_kpeers, &sc->peers, tmppeer) {
		peer->del_reason = OVPN_DEL_REASON_REQUESTED;
		ret = _ovpn_del_peer(sc, peer);
		MPASS(ret == 0);
	}

	ovpn_flush_rxring(sc);

	OVPN_WUNLOCK(sc);
}
#endif

static int
ovpn_clone_match(struct if_clone *ifc, const char *name)
{
	/*
	 * Allow all names that start with 'ovpn', specifically because pfSense
	 * uses ovpnc1 / ovpns2
	 */
	return (strncmp(ovpnname, name, strlen(ovpnname)) == 0);
}

static int
ovpn_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct ovpn_softc *sc;
	struct ifnet *ifp;
	char *dp;
	int error, unit, wildcard;

	/* Try to see if a special unit was requested. */
	error = ifc_name2unit(name, &unit);
	if (error != 0)
		return (error);
	wildcard = (unit < 0);

	error = ifc_alloc_unit(ifc, &unit);
	if (error != 0)
		return (error);

	/*
	 * If no unit had been given, we need to adjust the ifName.
	 */
	for (dp = name; *dp != '\0'; dp++);
	if (wildcard) {
		error = snprintf(dp, len - (dp - name), "%d", unit);
		if (error > len - (dp - name)) {
			/* ifName too long. */
			ifc_free_unit(ifc, unit);
			return (ENOSPC);
		}
		dp += error;
	}

	/* Make sure it doesn't already exist. */
	if (ifunit(name) != NULL)
		return (EEXIST);

	sc = malloc(sizeof(struct ovpn_softc), M_OVPN, M_WAITOK | M_ZERO);
	sc->ifp = if_alloc(IFT_ENC);
	rm_init_flags(&sc->lock, "if_ovpn_lock", RM_RECURSE);
	sc->refcount = 0;

	sc->notifring = buf_ring_alloc(32, M_OVPN, M_WAITOK, NULL);

	COUNTER_ARRAY_ALLOC(sc->counters, OVPN_COUNTER_SIZE, M_WAITOK);

	ifp = sc->ifp;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, name, IFNAMSIZ);
	ifp->if_dname = ovpngroupname;
	ifp->if_dunit = unit;

	ifp->if_addrlen = 0;
	ifp->if_mtu = 1428;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_ioctl = ovpn_ioctl;
	ifp->if_transmit = ovpn_transmit;
	ifp->if_output = ovpn_output;
	ifp->if_qflush = ovpn_qflush;
#ifdef VIMAGE
	ifp->if_reassign = ovpn_reassign;
#endif
	ifp->if_capabilities |= IFCAP_LINKSTATE;
	ifp->if_capenable |= IFCAP_LINKSTATE;

	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(uint32_t));
	*ifpp = ifp;

	return (0);
}

static void
ovpn_clone_destroy_cb(struct epoch_context *ctx)
{
	struct ovpn_softc *sc;

	sc = __containerof(ctx, struct ovpn_softc, epoch_ctx);

	MPASS(sc->peercount == 0);
	MPASS(RB_EMPTY(&sc->peers));

	COUNTER_ARRAY_FREE(sc->counters, OVPN_COUNTER_SIZE);

	if_free(sc->ifp);
	free(sc, M_OVPN);
}

static int
ovpn_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct ovpn_softc *sc;
	struct ovpn_kpeer *peer, *tmppeer;
	int unit;
	int ret __diagused;

	sc = ifp->if_softc;
	unit = ifp->if_dunit;

	OVPN_WLOCK(sc);

	if (atomic_load_int(&sc->refcount) > 0) {
		OVPN_WUNLOCK(sc);
		return (EBUSY);
	}

	RB_FOREACH_SAFE(peer, ovpn_kpeers, &sc->peers, tmppeer) {
		peer->del_reason = OVPN_DEL_REASON_REQUESTED;
		ret = _ovpn_del_peer(sc, peer);
		MPASS(ret == 0);
	}

	ovpn_flush_rxring(sc);
	buf_ring_free(sc->notifring, M_OVPN);

	OVPN_WUNLOCK(sc);

	bpfdetach(ifp);
	if_detach(ifp);
	ifp->if_softc = NULL;

	NET_EPOCH_CALL(ovpn_clone_destroy_cb, &sc->epoch_ctx);

	if (unit != IF_DUNIT_NONE)
		ifc_free_unit(ifc, unit);

	NET_EPOCH_DRAIN_CALLBACKS();

	return (0);
}

static void
vnet_ovpn_init(const void *unused __unused)
{
	struct if_clone_addreq req = {
		.match_f = ovpn_clone_match,
		.create_f = ovpn_clone_create,
		.destroy_f = ovpn_clone_destroy,
	};
	V_ovpn_cloner = ifc_attach_cloner(ovpngroupname, &req);
}
VNET_SYSINIT(vnet_ovpn_init, SI_SUB_PSEUDO, SI_ORDER_ANY,
    vnet_ovpn_init, NULL);

static void
vnet_ovpn_uninit(const void *unused __unused)
{
	if_clone_detach(V_ovpn_cloner);
}
VNET_SYSUNINIT(vnet_ovpn_uninit, SI_SUB_PSEUDO, SI_ORDER_ANY,
    vnet_ovpn_uninit, NULL);

static int
ovpnmodevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		/* Done in vnet_ovpn_init() */
		break;
	case MOD_UNLOAD:
		/* Done in vnet_ovpn_uninit() */
		break;
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

static moduledata_t ovpn_mod = {
	"if_ovpn",
	ovpnmodevent,
	0
};

DECLARE_MODULE(if_ovpn, ovpn_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_ovpn, 1);
