/*
 * wpa_supplicant - IBSS RSN
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "l2_packet/l2_packet.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/wpa_ie.h"
#include "ap/wpa_auth.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "ibss_rsn.h"


static void ibss_rsn_free(struct ibss_rsn_peer *peer)
{
	wpa_auth_sta_deinit(peer->auth);
	wpa_sm_deinit(peer->supp);
	os_free(peer);
}


static void supp_set_state(void *ctx, enum wpa_states state)
{
	struct ibss_rsn_peer *peer = ctx;
	peer->supp_state = state;
}


static int supp_ether_send(void *ctx, const u8 *dest, u16 proto, const u8 *buf,
			   size_t len)
{
	struct ibss_rsn_peer *peer = ctx;
	struct wpa_supplicant *wpa_s = peer->ibss_rsn->wpa_s;

	wpa_printf(MSG_DEBUG, "SUPP: %s(dest=" MACSTR " proto=0x%04x "
		   "len=%lu)",
		   __func__, MAC2STR(dest), proto, (unsigned long) len);

	if (wpa_s->l2)
		return l2_packet_send(wpa_s->l2, dest, proto, buf, len);

	return wpa_drv_send_eapol(wpa_s, dest, proto, buf, len);
}


static u8 * supp_alloc_eapol(void *ctx, u8 type, const void *data,
			     u16 data_len, size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

	wpa_printf(MSG_DEBUG, "SUPP: %s(type=%d data_len=%d)",
		   __func__, type, data_len);

	*msg_len = sizeof(*hdr) + data_len;
	hdr = os_malloc(*msg_len);
	if (hdr == NULL)
		return NULL;

	hdr->version = 2;
	hdr->type = type;
	hdr->length = host_to_be16(data_len);

	if (data)
		os_memcpy(hdr + 1, data, data_len);
	else
		os_memset(hdr + 1, 0, data_len);

	if (data_pos)
		*data_pos = hdr + 1;

	return (u8 *) hdr;
}


static int supp_get_beacon_ie(void *ctx)
{
	struct ibss_rsn_peer *peer = ctx;

	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);
	/* TODO: get correct RSN IE */
	return wpa_sm_set_ap_rsn_ie(peer->supp,
				    (u8 *) "\x30\x14\x01\x00"
				    "\x00\x0f\xac\x04"
				    "\x01\x00\x00\x0f\xac\x04"
				    "\x01\x00\x00\x0f\xac\x02"
				    "\x00\x00", 22);
}


static int supp_set_key(void *ctx, enum wpa_alg alg,
			const u8 *addr, int key_idx, int set_tx,
			const u8 *seq, size_t seq_len,
			const u8 *key, size_t key_len)
{
	struct ibss_rsn_peer *peer = ctx;

	wpa_printf(MSG_DEBUG, "SUPP: %s(alg=%d addr=" MACSTR " key_idx=%d "
		   "set_tx=%d)",
		   __func__, alg, MAC2STR(addr), key_idx, set_tx);
	wpa_hexdump(MSG_DEBUG, "SUPP: set_key - seq", seq, seq_len);
	wpa_hexdump_key(MSG_DEBUG, "SUPP: set_key - key", key, key_len);

	if (key_idx == 0) {
		/*
		 * In IBSS RSN, the pairwise key from the 4-way handshake
		 * initiated by the peer with highest MAC address is used.
		 */
		if (os_memcmp(peer->ibss_rsn->wpa_s->own_addr, peer->addr,
			      ETH_ALEN) > 0) {
			wpa_printf(MSG_DEBUG, "SUPP: Do not use this PTK");
			return 0;
		}
	}

	return wpa_drv_set_key(peer->ibss_rsn->wpa_s, alg, addr, key_idx,
			       set_tx, seq, seq_len, key, key_len);
}


static void * supp_get_network_ctx(void *ctx)
{
	struct ibss_rsn_peer *peer = ctx;
	return wpa_supplicant_get_ssid(peer->ibss_rsn->wpa_s);
}


static int supp_mlme_setprotection(void *ctx, const u8 *addr,
				   int protection_type, int key_type)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(addr=" MACSTR " protection_type=%d "
		   "key_type=%d)",
		   __func__, MAC2STR(addr), protection_type, key_type);
	return 0;
}


static void supp_cancel_auth_timeout(void *ctx)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);
}


int ibss_rsn_supp_init(struct ibss_rsn_peer *peer, const u8 *own_addr,
		       const u8 *psk)
{
	struct wpa_sm_ctx *ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return -1;

	ctx->ctx = peer;
	ctx->msg_ctx = peer->ibss_rsn->wpa_s;
	ctx->set_state = supp_set_state;
	ctx->ether_send = supp_ether_send;
	ctx->get_beacon_ie = supp_get_beacon_ie;
	ctx->alloc_eapol = supp_alloc_eapol;
	ctx->set_key = supp_set_key;
	ctx->get_network_ctx = supp_get_network_ctx;
	ctx->mlme_setprotection = supp_mlme_setprotection;
	ctx->cancel_auth_timeout = supp_cancel_auth_timeout;
	peer->supp = wpa_sm_init(ctx);
	if (peer->supp == NULL) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_init() failed");
		return -1;
	}

	wpa_sm_set_own_addr(peer->supp, own_addr);
	wpa_sm_set_param(peer->supp, WPA_PARAM_RSN_ENABLED, 1);
	wpa_sm_set_param(peer->supp, WPA_PARAM_PROTO, WPA_PROTO_RSN);
	wpa_sm_set_param(peer->supp, WPA_PARAM_PAIRWISE, WPA_CIPHER_CCMP);
	wpa_sm_set_param(peer->supp, WPA_PARAM_GROUP, WPA_CIPHER_CCMP);
	wpa_sm_set_param(peer->supp, WPA_PARAM_KEY_MGMT, WPA_KEY_MGMT_PSK);
	wpa_sm_set_pmk(peer->supp, psk, PMK_LEN);

	peer->supp_ie_len = sizeof(peer->supp_ie);
	if (wpa_sm_set_assoc_wpa_ie_default(peer->supp, peer->supp_ie,
					    &peer->supp_ie_len) < 0) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_set_assoc_wpa_ie_default()"
			   " failed");
		return -1;
	}

	wpa_sm_notify_assoc(peer->supp, peer->addr);

	return 0;
}


static void auth_logger(void *ctx, const u8 *addr, logger_level level,
			const char *txt)
{
	if (addr)
		wpa_printf(MSG_DEBUG, "AUTH: " MACSTR " - %s",
			   MAC2STR(addr), txt);
	else
		wpa_printf(MSG_DEBUG, "AUTH: %s", txt);
}


static const u8 * auth_get_psk(void *ctx, const u8 *addr, const u8 *prev_psk)
{
	struct ibss_rsn *ibss_rsn = ctx;
	wpa_printf(MSG_DEBUG, "AUTH: %s (addr=" MACSTR " prev_psk=%p)",
		   __func__, MAC2STR(addr), prev_psk);
	if (prev_psk)
		return NULL;
	return ibss_rsn->psk;
}


static int auth_send_eapol(void *ctx, const u8 *addr, const u8 *data,
			   size_t data_len, int encrypt)
{
	struct ibss_rsn *ibss_rsn = ctx;
	struct wpa_supplicant *wpa_s = ibss_rsn->wpa_s;

	wpa_printf(MSG_DEBUG, "AUTH: %s(addr=" MACSTR " data_len=%lu "
		   "encrypt=%d)",
		   __func__, MAC2STR(addr), (unsigned long) data_len, encrypt);

	if (wpa_s->l2)
		return l2_packet_send(wpa_s->l2, addr, ETH_P_EAPOL, data,
				      data_len);

	return wpa_drv_send_eapol(wpa_s, addr, ETH_P_EAPOL, data, data_len);
}


static int auth_set_key(void *ctx, int vlan_id, enum wpa_alg alg,
			const u8 *addr, int idx, u8 *key, size_t key_len)
{
	struct ibss_rsn *ibss_rsn = ctx;
	u8 seq[6];

	os_memset(seq, 0, sizeof(seq));

	if (addr) {
		wpa_printf(MSG_DEBUG, "AUTH: %s(alg=%d addr=" MACSTR
			   " key_idx=%d)",
			   __func__, alg, MAC2STR(addr), idx);
	} else {
		wpa_printf(MSG_DEBUG, "AUTH: %s(alg=%d key_idx=%d)",
			   __func__, alg, idx);
	}
	wpa_hexdump_key(MSG_DEBUG, "AUTH: set_key - key", key, key_len);

	if (idx == 0) {
		/*
		 * In IBSS RSN, the pairwise key from the 4-way handshake
		 * initiated by the peer with highest MAC address is used.
		 */
		if (addr == NULL ||
		    os_memcmp(ibss_rsn->wpa_s->own_addr, addr, ETH_ALEN) < 0) {
			wpa_printf(MSG_DEBUG, "AUTH: Do not use this PTK");
			return 0;
		}
	}

	return wpa_drv_set_key(ibss_rsn->wpa_s, alg, addr, idx,
			       1, seq, 6, key, key_len);
}


static int ibss_rsn_auth_init_group(struct ibss_rsn *ibss_rsn,
				    const u8 *own_addr)
{
	struct wpa_auth_config conf;
	struct wpa_auth_callbacks cb;

	wpa_printf(MSG_DEBUG, "AUTH: Initializing group state machine");

	os_memset(&conf, 0, sizeof(conf));
	conf.wpa = 2;
	conf.wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	conf.wpa_pairwise = WPA_CIPHER_CCMP;
	conf.rsn_pairwise = WPA_CIPHER_CCMP;
	conf.wpa_group = WPA_CIPHER_CCMP;
	conf.eapol_version = 2;

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = ibss_rsn;
	cb.logger = auth_logger;
	cb.send_eapol = auth_send_eapol;
	cb.get_psk = auth_get_psk;
	cb.set_key = auth_set_key;

	ibss_rsn->auth_group = wpa_init(own_addr, &conf, &cb);
	if (ibss_rsn->auth_group == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_init() failed");
		return -1;
	}

	return 0;
}


static int ibss_rsn_auth_init(struct ibss_rsn *ibss_rsn,
			      struct ibss_rsn_peer *peer)
{
	peer->auth = wpa_auth_sta_init(ibss_rsn->auth_group, peer->addr);
	if (peer->auth == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_auth_sta_init() failed");
		return -1;
	}

	/* TODO: get peer RSN IE with Probe Request */
	if (wpa_validate_wpa_ie(ibss_rsn->auth_group, peer->auth,
				(u8 *) "\x30\x14\x01\x00"
				"\x00\x0f\xac\x04"
				"\x01\x00\x00\x0f\xac\x04"
				"\x01\x00\x00\x0f\xac\x02"
				"\x00\x00", 22, NULL, 0) !=
	    WPA_IE_OK) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_validate_wpa_ie() failed");
		return -1;
	}

	if (wpa_auth_sm_event(peer->auth, WPA_ASSOC))
		return -1;

	if (wpa_auth_sta_associated(ibss_rsn->auth_group, peer->auth))
		return -1;

	return 0;
}


int ibss_rsn_start(struct ibss_rsn *ibss_rsn, const u8 *addr)
{
	struct ibss_rsn_peer *peer;

	wpa_printf(MSG_DEBUG, "RSN: Starting IBSS Authenticator and "
		   "Supplicant for peer " MACSTR, MAC2STR(addr));

	peer = os_zalloc(sizeof(*peer));
	if (peer == NULL)
		return -1;

	peer->ibss_rsn = ibss_rsn;
	os_memcpy(peer->addr, addr, ETH_ALEN);

	if (ibss_rsn_supp_init(peer, ibss_rsn->wpa_s->own_addr, ibss_rsn->psk)
	    < 0) {
		ibss_rsn_free(peer);
		return -1;
	}

	if (ibss_rsn_auth_init(ibss_rsn, peer) < 0) {
		ibss_rsn_free(peer);
		return -1;
	}

	peer->next = ibss_rsn->peers;
	ibss_rsn->peers = peer;

	return 0;
}


struct ibss_rsn * ibss_rsn_init(struct wpa_supplicant *wpa_s)
{
	struct ibss_rsn *ibss_rsn;

	ibss_rsn = os_zalloc(sizeof(*ibss_rsn));
	if (ibss_rsn == NULL)
		return NULL;
	ibss_rsn->wpa_s = wpa_s;

	if (ibss_rsn_auth_init_group(ibss_rsn, wpa_s->own_addr) < 0) {
		ibss_rsn_deinit(ibss_rsn);
		return NULL;
	}

	return ibss_rsn;
}


void ibss_rsn_deinit(struct ibss_rsn *ibss_rsn)
{
	struct ibss_rsn_peer *peer, *prev;

	if (ibss_rsn == NULL)
		return;

	peer = ibss_rsn->peers;
	while (peer) {
		prev = peer;
		peer = peer->next;
		ibss_rsn_free(prev);
	}

	wpa_deinit(ibss_rsn->auth_group);
	os_free(ibss_rsn);

}


static int ibss_rsn_eapol_dst_supp(const u8 *buf, size_t len)
{
	const struct ieee802_1x_hdr *hdr;
	const struct wpa_eapol_key *key;
	u16 key_info;
	size_t plen;

	/* TODO: Support other EAPOL packets than just EAPOL-Key */

	if (len < sizeof(*hdr) + sizeof(*key))
		return -1;

	hdr = (const struct ieee802_1x_hdr *) buf;
	key = (const struct wpa_eapol_key *) (hdr + 1);
	plen = be_to_host16(hdr->length);

	if (hdr->version < EAPOL_VERSION) {
		/* TODO: backwards compatibility */
	}
	if (hdr->type != IEEE802_1X_TYPE_EAPOL_KEY) {
		wpa_printf(MSG_DEBUG, "RSN: EAPOL frame (type %u) discarded, "
			"not a Key frame", hdr->type);
		return -1;
	}
	if (plen > len - sizeof(*hdr) || plen < sizeof(*key)) {
		wpa_printf(MSG_DEBUG, "RSN: EAPOL frame payload size %lu "
			   "invalid (frame size %lu)",
			   (unsigned long) plen, (unsigned long) len);
		return -1;
	}

	if (key->type != EAPOL_KEY_TYPE_RSN) {
		wpa_printf(MSG_DEBUG, "RSN: EAPOL-Key type (%d) unknown, "
			   "discarded", key->type);
		return -1;
	}

	key_info = WPA_GET_BE16(key->key_info);

	return !!(key_info & WPA_KEY_INFO_ACK);
}


static int ibss_rsn_process_rx_eapol(struct ibss_rsn *ibss_rsn,
				     struct ibss_rsn_peer *peer,
				     const u8 *buf, size_t len)
{
	int supp;
	u8 *tmp;

	supp = ibss_rsn_eapol_dst_supp(buf, len);
	if (supp < 0)
		return -1;

	tmp = os_malloc(len);
	if (tmp == NULL)
		return -1;
	os_memcpy(tmp, buf, len);
	if (supp) {
		wpa_printf(MSG_DEBUG, "RSN: IBSS RX EAPOL for Supplicant");
		wpa_sm_rx_eapol(peer->supp, peer->addr, tmp, len);
	} else {
		wpa_printf(MSG_DEBUG, "RSN: IBSS RX EAPOL for Authenticator");
		wpa_receive(ibss_rsn->auth_group, peer->auth, tmp, len);
	}
	os_free(tmp);

	return 1;
}


int ibss_rsn_rx_eapol(struct ibss_rsn *ibss_rsn, const u8 *src_addr,
		      const u8 *buf, size_t len)
{
	struct ibss_rsn_peer *peer;

	for (peer = ibss_rsn->peers; peer; peer = peer->next) {
		if (os_memcmp(src_addr, peer->addr, ETH_ALEN) == 0)
			return ibss_rsn_process_rx_eapol(ibss_rsn, peer,
							 buf, len);
	}

	if (ibss_rsn_eapol_dst_supp(buf, len) > 0) {
		/*
		 * Create new IBSS peer based on an EAPOL message from the peer
		 * Authenticator.
		 */
		if (ibss_rsn_start(ibss_rsn, src_addr) < 0)
			return -1;
		return ibss_rsn_process_rx_eapol(ibss_rsn, ibss_rsn->peers,
						 buf, len);
	}

	return 0;
}


void ibss_rsn_set_psk(struct ibss_rsn *ibss_rsn, const u8 *psk)
{
	os_memcpy(ibss_rsn->psk, psk, PMK_LEN);
}
