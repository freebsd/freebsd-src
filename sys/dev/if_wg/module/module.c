/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019-2020 Rubicon Communications, LLC (Netgate)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/priv.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/queue.h>
#include <sys/smp.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_var.h>
#include <net/iflib.h>
#include <net/if_clone.h>
#include <net/radix.h>
#include <net/bpf.h>
#include <net/mp_ring.h>

#include "ifdi_if.h"

#include <sys/wg_module.h>
#include <crypto/zinc.h>
#include <sys/wg_noise.h>
#include <sys/if_wg_session_vars.h>
#include <sys/if_wg_session.h>

MALLOC_DEFINE(M_WG, "WG", "wireguard");

#define	WG_CAPS		IFCAP_LINKSTATE
#define	ph_family	PH_loc.eight[5]

TASKQGROUP_DECLARE(if_io_tqg);

static int clone_count;
uma_zone_t ratelimit_zone;

void
wg_encrypt_dispatch(struct wg_softc *sc)
{
	for (int i = 0; i < mp_ncpus; i++) {
		if (sc->sc_encrypt[i].gt_task.ta_flags & TASK_ENQUEUED)
			continue;
		GROUPTASK_ENQUEUE(&sc->sc_encrypt[i]);
	}
}

void
wg_decrypt_dispatch(struct wg_softc *sc)
{
	for (int i = 0; i < mp_ncpus; i++) {
		if (sc->sc_decrypt[i].gt_task.ta_flags & TASK_ENQUEUED)
			continue;
		GROUPTASK_ENQUEUE(&sc->sc_decrypt[i]);
	}
}

static void
crypto_taskq_setup(struct wg_softc *sc)
{
	device_t dev = iflib_get_dev(sc->wg_ctx);

	sc->sc_encrypt = malloc(sizeof(struct grouptask)*mp_ncpus, M_WG, M_WAITOK);
	sc->sc_decrypt = malloc(sizeof(struct grouptask)*mp_ncpus, M_WG, M_WAITOK);

	for (int i = 0; i < mp_ncpus; i++) {
		GROUPTASK_INIT(&sc->sc_encrypt[i], 0,
		     (gtask_fn_t *)wg_softc_encrypt, sc);
		taskqgroup_attach_cpu(qgroup_if_io_tqg, &sc->sc_encrypt[i], sc,  i, dev, NULL, "wg encrypt");
		GROUPTASK_INIT(&sc->sc_decrypt[i], 0,
		    (gtask_fn_t *)wg_softc_decrypt, sc);
		taskqgroup_attach_cpu(qgroup_if_io_tqg, &sc->sc_decrypt[i], sc, i, dev, NULL, "wg decrypt");
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
wg_cloneattach(if_ctx_t ctx, struct if_clone *ifc, const char *name, caddr_t params)
{
	struct wg_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx;
	device_t dev;
	struct iovec iov;
	nvlist_t *nvl;
	void *packed;
	struct noise_local *local;
	uint8_t			 public[WG_KEY_SIZE];
	struct noise_upcall	 noise_upcall;
	int err;
	uint16_t listen_port;
	const void *key;
	size_t size;

	err = 0;
	dev = iflib_get_dev(ctx);
	if (params == NULL) {
		key = NULL;
		listen_port = 0;
		nvl = NULL;
		packed = NULL;
		goto unpacked;
	}
	if (copyin(params, &iov, sizeof(iov)))
		return (EFAULT);
	/* check that this is reasonable */
	size = iov.iov_len;
	packed = malloc(size, M_TEMP, M_WAITOK);
	if (copyin(iov.iov_base, packed, size)) {
		err = EFAULT;
		goto out;
	}
	nvl = nvlist_unpack(packed, size, 0);
	if (nvl == NULL) {
		device_printf(dev, "%s nvlist_unpack failed\n", __func__);
		err = EBADMSG;
		goto out;
	}
	if (!nvlist_exists_number(nvl, "listen-port")) {
		device_printf(dev, "%s listen-port not set\n", __func__);
		err = EBADMSG;
		goto nvl_out;
	}
	listen_port = nvlist_get_number(nvl, "listen-port");

	if (!nvlist_exists_binary(nvl, "private-key")) {
		device_printf(dev, "%s private-key not set\n", __func__);
		err = EBADMSG;
		goto nvl_out;
	}
	key = nvlist_get_binary(nvl, "private-key", &size);
	if (size != CURVE25519_KEY_SIZE) {
		device_printf(dev, "%s bad length for private-key %zu\n", __func__, size);
		err = EBADMSG;
		goto nvl_out;
	}
unpacked:
	local = &sc->sc_local;
	noise_upcall.u_arg = sc;
	noise_upcall.u_remote_get =
		(struct noise_remote *(*)(void *, uint8_t *))wg_remote_get;
	noise_upcall.u_index_set =
		(uint32_t (*)(void *, struct noise_remote *))wg_index_set;
	noise_upcall.u_index_drop =
		(void (*)(void *, uint32_t))wg_index_drop;
	noise_local_init(local, &noise_upcall);
	cookie_checker_init(&sc->sc_cookie, ratelimit_zone);

	sc->sc_socket.so_port = listen_port;

	if (key != NULL) {
		noise_local_set_private(local, __DECONST(uint8_t *, key));
		noise_local_keys(local, public, NULL);
		cookie_checker_update(&sc->sc_cookie, public);
	}
	atomic_add_int(&clone_count, 1);
	scctx = sc->shared = iflib_get_softc_ctx(ctx);
	scctx->isc_capenable = WG_CAPS;
	scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_TSO | CSUM_IP6_TCP \
		| CSUM_IP6_UDP | CSUM_IP6_TCP;
	sc->wg_ctx = ctx;
	sc->sc_ifp = iflib_get_ifp(ctx);

	mbufq_init(&sc->sc_handshake_queue, MAX_QUEUED_INCOMING_HANDSHAKES);
	mtx_init(&sc->sc_mtx, NULL, "wg softc lock",  MTX_DEF);
	rw_init(&sc->sc_index_lock, "wg index lock");
	sc->sc_encap_ring = buf_ring_alloc(MAX_QUEUED_PACKETS, M_WG, M_WAITOK, &sc->sc_mtx);
	sc->sc_decap_ring = buf_ring_alloc(MAX_QUEUED_PACKETS, M_WG, M_WAITOK, &sc->sc_mtx);
	GROUPTASK_INIT(&sc->sc_handshake, 0,
	    (gtask_fn_t *)wg_softc_handshake_receive, sc);
	taskqgroup_attach(qgroup_if_io_tqg, &sc->sc_handshake, sc, dev, NULL, "wg tx initiation");
	crypto_taskq_setup(sc);
 nvl_out:
	if (nvl != NULL)
		nvlist_destroy(nvl);
out:
	free(packed, M_TEMP);
	return (err);
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
	sc = iflib_get_softc(ifp->if_softc);
	if ((t = wg_tag_get(m)) == NULL) {
		rc = ENOBUFS;
		goto early_out;
	}
	af = m->m_pkthdr.ph_family;
	BPF_MTAP2(ifp, &af, sizeof(af), m);

	NET_EPOCH_ENTER(et);
	peer = wg_route_lookup(&sc->sc_routes, m, OUT);
	if (__predict_false(peer == NULL)) {
		rc = ENOKEY;
		printf("peer not found - dropping %p\n", m);
		/* XXX log */
		goto err;
	}

	family = atomic_load_acq(peer->p_endpoint.e_remote.r_sa.sa_family);
	if (__predict_false(family != AF_INET && family != AF_INET6)) {
		rc = EHOSTUNREACH;
		/* XXX log */
		goto err;
	}
	t->t_peer = peer;
	t->t_mbuf = NULL;
	t->t_done = 0;
	t->t_mtu = ifp->if_mtu;

	rc = wg_queue_out(peer, m);
	if (rc == 0)
		wg_encrypt_dispatch(peer->p_sc);
	NET_EPOCH_EXIT(et);
	return (rc); 
err:
	NET_EPOCH_EXIT(et);
early_out:
	if_inc_counter(sc->sc_ifp, IFCOUNTER_OERRORS, 1);
	/* XXX send ICMP unreachable */
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
wg_attach_post(if_ctx_t ctx)
{
	struct ifnet *ifp;
	struct wg_softc *sc;

	sc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);
	if_setmtu(ifp, ETHERMTU - 80);

	if_setflagbits(ifp, IFF_NOARP, IFF_POINTOPOINT);
	ifp->if_transmit = wg_transmit;
	ifp->if_output = wg_output;

	wg_hashtable_init(&sc->sc_hashtable);
	sc->sc_index = hashinit(HASHTABLE_INDEX_SIZE, M_DEVBUF, &sc->sc_index_mask);
	wg_route_init(&sc->sc_routes);

	return (0);
}

static int
wg_mtu_set(if_ctx_t ctx, uint32_t mtu)
{

	return (0);
}

static int
wg_set_promisc(if_ctx_t ctx, int flags)
{

	return (0);
}

static int
wg_detach(if_ctx_t ctx)
{
	struct wg_softc *sc;

	sc = iflib_get_softc(ctx);
	if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
	NET_EPOCH_WAIT();
	wg_socket_reinit(sc, NULL, NULL);
	taskqgroup_drain_all(qgroup_if_io_tqg);
	pause("link_down", hz/4);
	wg_peer_remove_all(sc);
	pause("link_down", hz);
	mtx_destroy(&sc->sc_mtx);
	rw_destroy(&sc->sc_index_lock);
	taskqgroup_detach(qgroup_if_io_tqg, &sc->sc_handshake);
	crypto_taskq_destroy(sc);
	buf_ring_free(sc->sc_encap_ring, M_WG);
	buf_ring_free(sc->sc_decap_ring, M_WG);

	wg_route_destroy(&sc->sc_routes);
	wg_hashtable_destroy(&sc->sc_hashtable);
	atomic_add_int(&clone_count, -1);
	return (0);
}

static void
wg_init(if_ctx_t ctx)
{
	struct ifnet *ifp;
	struct wg_softc *sc;
	int rc;

	sc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);
	rc = wg_socket_init(sc);
	if (rc)
		return;
	if_link_state_change(ifp, LINK_STATE_UP);
}

static void
wg_stop(if_ctx_t ctx)
{
	struct wg_softc *sc;
	struct ifnet *ifp;

	sc  = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);
	if_link_state_change(ifp, LINK_STATE_DOWN);
}

static nvlist_t *
wg_peer_to_nvl(struct wg_peer *peer)
{
	struct wg_route *rt;
	int i, count;
	nvlist_t *nvl;
	caddr_t key;
	struct wg_allowedip *aip;

	if ((nvl = nvlist_create(0)) == NULL)
		return (NULL);
	key = peer->p_remote.r_public;
	nvlist_add_binary(nvl, "public-key", key, WG_KEY_SIZE);
	nvlist_add_binary(nvl, "endpoint", &peer->p_endpoint.e_remote, sizeof(struct sockaddr));
	i = count = 0;
	CK_LIST_FOREACH(rt, &peer->p_routes, r_entry) {
		count++;
	}
	aip = malloc(count*sizeof(*aip), M_TEMP, M_WAITOK);
	CK_LIST_FOREACH(rt, &peer->p_routes, r_entry) {
		memcpy(&aip[i++], &rt->r_cidr, sizeof(*aip));
	}
	nvlist_add_binary(nvl, "allowed-ips", aip, count*sizeof(*aip));
	free(aip, M_TEMP);
	return (nvl);
}

static int
wg_marshal_peers(struct wg_softc *sc, nvlist_t **nvlp, nvlist_t ***nvl_arrayp, int *peer_countp)
{
	struct wg_peer *peer;
	int err, i, peer_count;
	nvlist_t *nvl, **nvl_array;
	struct epoch_tracker et;
#ifdef INVARIANTS
	void *packed;
	size_t size;
#endif
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
		printf("no peers found\n");
		return (ENOENT);
	}

	if (nvlp && (nvl = nvlist_create(0)) == NULL)
		return (ENOMEM);
	err = i = 0;
	nvl_array = malloc(peer_count*sizeof(void*), M_TEMP, M_WAITOK);
	NET_EPOCH_ENTER(et);
	CK_LIST_FOREACH(peer, &sc->sc_hashtable.h_peers_list, p_entry) {
		nvl_array[i] = wg_peer_to_nvl(peer);
		if (nvl_array[i] == NULL) {
			printf("wg_peer_to_nvl failed on %d peer\n", i);
			break;
		}
#ifdef INVARIANTS
		packed = nvlist_pack(nvl_array[i], &size);
		if (packed == NULL) {
			printf("nvlist_pack(%p, %p) => %d",
				   nvl_array[i], &size, nvlist_error(nvl));
		}
		free(packed, M_NVLIST);
#endif	
		i++;
		if (i == peer_count)
			break;
	}
	NET_EPOCH_EXIT(et);
	*peer_countp = peer_count = i;
	if (peer_count == 0) {
		printf("no peers found in list\n");
		err = ENOENT;
		goto out;
	}
	if (nvl) {
		nvlist_add_nvlist_array(nvl, "peer-list",
		    (const nvlist_t * const *)nvl_array, peer_count);
		if ((err = nvlist_error(nvl))) {
			printf("nvlist_add_nvlist_array(%p, \"peer-list\", %p, %d) => %d\n",
			    nvl, nvl_array, peer_count, err);
			goto out;
		}
		*nvlp = nvl;
	}
	*nvl_arrayp = nvl_array;
	return (0);
 out:
	return (err);
}

static int
wgc_get(struct wg_softc *sc, struct ifdrv *ifd)
{
	nvlist_t *nvl, **nvl_array;
	void *packed;
	size_t size;
	int peer_count, err;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		return (ENOMEM);

	err = 0;
	packed = NULL;
	if (sc->sc_socket.so_port != 0)
		nvlist_add_number(nvl, "listen-port", sc->sc_socket.so_port);
	if (sc->sc_local.l_has_identity) {
		nvlist_add_binary(nvl, "public-key", sc->sc_local.l_public, WG_KEY_SIZE);
		if (curthread->td_ucred->cr_uid == 0)
			nvlist_add_binary(nvl, "private-key", sc->sc_local.l_private, WG_KEY_SIZE);
	}
	if (sc->sc_hashtable.h_num_peers > 0) {
		err = wg_marshal_peers(sc, NULL, &nvl_array, &peer_count);
		if (err)
			goto out;
		nvlist_add_nvlist_array(nvl, "peer-list",
		    (const nvlist_t * const *)nvl_array, peer_count);
	}
	packed = nvlist_pack(nvl, &size);
	if (packed == NULL)
		return (ENOMEM);
	if (ifd->ifd_len == 0) {
		ifd->ifd_len = size;
		goto out;
	}
	if (ifd->ifd_len < size) {
		err = ENOSPC;
		goto out;
	}
	if (ifd->ifd_data == NULL) {
		err = EFAULT;
		goto out;
	}
	err = copyout(packed, ifd->ifd_data, size);
	ifd->ifd_len = size;
 out:
	nvlist_destroy(nvl);
	free(packed, M_NVLIST);
	return (err);
}

static bool
wg_allowedip_valid(const struct wg_allowedip *wip)
{

	return (true);
}

static int
wg_peer_add(struct wg_softc *sc, const nvlist_t *nvl)
{
	uint8_t			 public[WG_KEY_SIZE];
	const void *pub_key;
	const struct sockaddr *endpoint;
	int i, err, allowedip_count;
	device_t dev;
	size_t size;
	struct wg_peer *peer = NULL;
	bool need_insert = false;
	dev = iflib_get_dev(sc->wg_ctx);

	if (!nvlist_exists_binary(nvl, "public-key")) {
		device_printf(dev, "peer has no public-key\n");
		return (EINVAL);
	}
	pub_key = nvlist_get_binary(nvl, "public-key", &size);
	if (size != CURVE25519_KEY_SIZE) {
		device_printf(dev, "%s bad length for public-key %zu\n", __func__, size);
		return (EINVAL);
	}
	if (noise_local_keys(&sc->sc_local, public, NULL) == 0 &&
	    bcmp(public, pub_key, WG_KEY_SIZE) == 0) {
		device_printf(dev, "public-key for peer already in use by host\n");
		return (EINVAL);
	}
	peer = wg_peer_lookup(sc, pub_key);
	if (nvlist_exists_bool(nvl, "peer-remove") &&
		nvlist_get_bool(nvl, "peer-remove")) {
		if (peer != NULL) {
			wg_hashtable_peer_remove(&sc->sc_hashtable, peer);
			wg_peer_destroy(peer);
			/* XXX free */
			printf("peer removed\n");
		}
		return (0);
	}
	if (nvlist_exists_bool(nvl, "replace-allowedips") &&
		nvlist_get_bool(nvl, "replace-allowedips") &&
	    peer != NULL) {

		wg_route_delete(&peer->p_sc->sc_routes, peer);
	}
	if (peer == NULL) {
		need_insert = true;
		peer = wg_peer_alloc(sc);
		noise_remote_init(&peer->p_remote, pub_key, &sc->sc_local);
		cookie_maker_init(&peer->p_cookie, pub_key);
	}
	if (nvlist_exists_binary(nvl, "endpoint")) {
		endpoint = nvlist_get_binary(nvl, "endpoint", &size);
		if (size != sizeof(*endpoint)) {
			device_printf(dev, "%s bad length for endpoint %zu\n", __func__, size);
			err = EBADMSG;
			goto out;
		}
		memcpy(&peer->p_endpoint.e_remote, endpoint,
		    sizeof(peer->p_endpoint.e_remote));
	}
	if (nvlist_exists_binary(nvl, "pre-shared-key")) {
		const void *key;

		key = nvlist_get_binary(nvl, "pre-shared-key", &size);
		noise_remote_set_psk(&peer->p_remote, key);
	}
	if (nvlist_exists_number(nvl, "persistent-keepalive-interval")) {
		uint16_t pki;

		pki = nvlist_get_number(nvl, "persistent-keepalive-interval");
		wg_timers_set_persistent_keepalive(&peer->p_timers, pki);
	}
	if (nvlist_exists_binary(nvl, "allowed-ips")) {
		const struct wg_allowedip *aip, *aip_base;

		aip = aip_base = nvlist_get_binary(nvl, "allowed-ips", &size);
		if (size % sizeof(struct wg_allowedip) != 0) {
			device_printf(dev, "%s bad length for allowed-ips %zu not integer multiple of struct size\n", __func__, size);
			err = EBADMSG;
			goto out;
		}
		allowedip_count = size/sizeof(struct wg_allowedip);
		for (i = 0; i < allowedip_count; i++) {
			if (!wg_allowedip_valid(&aip_base[i])) {
				device_printf(dev, "%s allowedip %d not valid\n", __func__, i);
				err = EBADMSG;
				goto out;
			}
		}
		for (int i = 0; i < allowedip_count; i++, aip++) {
			if ((err = wg_route_add(&sc->sc_routes, peer, aip)) != 0) {
				printf("route add %d failed -> %d\n", i, err);
			}
		}
	}
	if (need_insert)
		wg_hashtable_peer_insert(&sc->sc_hashtable, peer);
	return (0);

out:
	wg_peer_destroy(peer);
	return (err);
}

static int
wgc_set(struct wg_softc *sc, struct ifdrv *ifd)
{
	uint8_t			 public[WG_KEY_SIZE];
	void *nvlpacked;
	nvlist_t *nvl;
	device_t dev;
	ssize_t size;
	int err;

	if (ifd->ifd_len == 0 || ifd->ifd_data == NULL)
		return (EFAULT);

	dev = iflib_get_dev(sc->wg_ctx);
	nvlpacked = malloc(ifd->ifd_len, M_TEMP, M_WAITOK);
	err = copyin(ifd->ifd_data, nvlpacked, ifd->ifd_len);
	if (err)
		goto out;
	nvl = nvlist_unpack(nvlpacked, ifd->ifd_len, 0);
	if (nvl == NULL) {
		device_printf(dev, "%s nvlist_unpack failed\n", __func__);
		err = EBADMSG;
		goto out;
	}
	if (nvlist_exists_bool(nvl, "replace-peers") &&
		nvlist_get_bool(nvl, "replace-peers"))
		wg_peer_remove_all(sc);
	if (nvlist_exists_number(nvl, "listen-port")) {
		int listen_port __unused = nvlist_get_number(nvl, "listen-port");
			/*
			 * Set listen port
			 */
		if_link_state_change(sc->sc_ifp, LINK_STATE_DOWN);
		pause("link_down", hz/4);
		wg_socket_reinit(sc, NULL, NULL);
		sc->sc_socket.so_port = listen_port;
		if ((err = wg_socket_init(sc)) != 0)
			goto out;
	   if_link_state_change(sc->sc_ifp, LINK_STATE_UP);
	}
	if (nvlist_exists_binary(nvl, "private-key")) {
		struct noise_local *local;
		const void *key = nvlist_get_binary(nvl, "private-key", &size);

		if (size != CURVE25519_KEY_SIZE) {
			device_printf(dev, "%s bad length for private-key %zu\n", __func__, size);
			err = EBADMSG;
			goto nvl_out;
		}
		/*
		 * set private key
		 */
		local = &sc->sc_local;
		noise_local_set_private(local, __DECONST(uint8_t *, key));
		noise_local_keys(local, public, NULL);
		cookie_checker_update(&sc->sc_cookie, public);
	}
	if (nvlist_exists_number(nvl, "user-cookie")) {
		sc->sc_user_cookie = nvlist_get_number(nvl, "user-cookie");
		/*
		 * setsockopt
		 */
	}
	if (nvlist_exists_nvlist_array(nvl, "peer-list")) {
		size_t peercount;
		const nvlist_t * const*nvl_peers;

		nvl_peers = nvlist_get_nvlist_array(nvl, "peer-list", &peercount);
		for (int i = 0; i < peercount; i++) {
			wg_peer_add(sc, nvl_peers[i]);
		}
	}
nvl_out:
	nvlist_destroy(nvl);
out:
	free(nvlpacked, M_TEMP);
	return (err);
}

static int
wg_priv_ioctl(if_ctx_t ctx, u_long command, caddr_t data)
{
	struct wg_softc *sc = iflib_get_softc(ctx);
	struct ifdrv *ifd = (struct ifdrv *)data;
	int ifd_cmd;

	switch (command) {
		case SIOCGDRVSPEC:
		case SIOCSDRVSPEC:
			ifd_cmd = ifd->ifd_cmd;
			break;
		default:
			return (EINVAL);
	}
	switch (ifd_cmd) {
		case WGC_GET:
			return (wgc_get(sc, ifd));
			break;
		case WGC_SET:
			if (priv_check(curthread, PRIV_NET_HWIOCTL))
				return (EPERM);
			return (wgc_set(sc, ifd));
			break;
	}
	return (ENOTSUP);
}

static device_method_t wg_if_methods[] = {
	DEVMETHOD(ifdi_cloneattach, wg_cloneattach),
	DEVMETHOD(ifdi_attach_post, wg_attach_post),
	DEVMETHOD(ifdi_detach, wg_detach),
	DEVMETHOD(ifdi_init, wg_init),
	DEVMETHOD(ifdi_stop, wg_stop),
	DEVMETHOD(ifdi_priv_ioctl, wg_priv_ioctl),
	DEVMETHOD(ifdi_mtu_set, wg_mtu_set),
	DEVMETHOD(ifdi_promisc_set, wg_set_promisc),
	DEVMETHOD_END
};

static driver_t wg_iflib_driver = {
	"wg", wg_if_methods, sizeof(struct wg_softc)
};

char wg_driver_version[] = "0.0.1";

static struct if_shared_ctx wg_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_driver_version = wg_driver_version,
	.isc_driver = &wg_iflib_driver,
	.isc_flags = IFLIB_PSEUDO,
	.isc_name = "wg",
};

if_shared_ctx_t wg_sctx = &wg_sctx_init;
static if_pseudo_t wg_pseudo;


int
wg_ctx_init(void)
{
	ratelimit_zone = uma_zcreate("wg ratelimit", sizeof(struct ratelimit),
	     NULL, NULL, NULL, NULL, 0, 0);
	return (0);
}

void
wg_ctx_uninit(void)
{
	uma_zdestroy(ratelimit_zone);
}

static int
wg_module_init(void)
{
	int rc;

	if ((rc = wg_ctx_init()))
		return (rc);

	wg_pseudo = iflib_clone_register(wg_sctx);
	if (wg_pseudo == NULL)
		return (ENXIO);

	return (0);
}

static void
wg_module_deinit(void)
{
	wg_ctx_uninit();
	iflib_clone_deregister(wg_pseudo);
}

static int
wg_module_event_handler(module_t mod, int what, void *arg)
{
	int err;

	switch (what) {
		case MOD_LOAD:
			if ((err = wg_module_init()) != 0)
				return (err);
			break;
		case MOD_UNLOAD:
			if (clone_count == 0)
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
MODULE_DEPEND(wg, iflib, 1, 1, 1);
#if defined(__amd64__) || defined(__i386__)
/* Optimized blake2 implementations are only available on x86. */
MODULE_DEPEND(wg, blake2, 1, 1, 1);
#endif
MODULE_DEPEND(wg, crypto, 1, 1, 1);
