/*
 * wpa_supplicant - Internal WPA state machine definitions
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#ifndef WPA_I_H
#define WPA_I_H

#define WPA_NONCE_LEN 32
#define WPA_REPLAY_COUNTER_LEN 8


struct rsn_pmksa_candidate;

/**
 * struct wpa_ptk - WPA Pairwise Transient Key
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 */
struct wpa_ptk {
	u8 kck[16]; /* EAPOL-Key Key Confirmation Key (KCK) */
	u8 kek[16]; /* EAPOL-Key Key Encryption Key (KEK) */
	u8 tk1[16]; /* Temporal Key 1 (TK1) */
	union {
		u8 tk2[16]; /* Temporal Key 2 (TK2) */
		struct {
			u8 tx_mic_key[8];
			u8 rx_mic_key[8];
		} auth;
	} u;
} __attribute__ ((packed));


/**
 * struct rsn_pmksa_cache - PMKSA cache entry
 */
struct rsn_pmksa_cache {
	struct rsn_pmksa_cache *next;
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN];
	size_t pmk_len;
	time_t expiration;
	time_t reauth_time;
	int akmp; /* WPA_KEY_MGMT_* */
	u8 aa[ETH_ALEN];
	struct wpa_ssid *ssid;
	int opportunistic;
};


/**
 * struct wpa_sm - Internal WPA state machine data
 */
struct wpa_sm {
	u8 pmk[PMK_LEN];
	size_t pmk_len;
	struct wpa_ptk ptk, tptk;
	int ptk_set, tptk_set;
	u8 snonce[WPA_NONCE_LEN];
	u8 anonce[WPA_NONCE_LEN]; /* ANonce from the last 1/4 msg */
	int renew_snonce;
	u8 rx_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int rx_replay_counter_set;
	u8 request_counter[WPA_REPLAY_COUNTER_LEN];

	struct eapol_sm *eapol; /* EAPOL state machine from upper level code */

	struct rsn_pmksa_cache *pmksa; /* PMKSA cache */
	struct rsn_pmksa_cache *cur_pmksa; /* current PMKSA entry */
	int pmksa_count; /* number of entries in PMKSA cache */
	struct rsn_pmksa_candidate *pmksa_candidates;

	struct l2_packet_data *l2_preauth;
	u8 preauth_bssid[ETH_ALEN]; /* current RSN pre-auth peer or
				     * 00:00:00:00:00:00 if no pre-auth is
				     * in progress */
	struct eapol_sm *preauth_eapol;

	struct wpa_sm_ctx *ctx;

	void *scard_ctx; /* context for smartcard callbacks */
	int fast_reauth; /* whether EAP fast re-authentication is enabled */

	struct wpa_ssid *cur_ssid;

	u8 own_addr[ETH_ALEN];
	const char *ifname;
	u8 bssid[ETH_ALEN];

	unsigned int dot11RSNAConfigPMKLifetime;
	unsigned int dot11RSNAConfigPMKReauthThreshold;
	unsigned int dot11RSNAConfigSATimeout;

	unsigned int dot11RSNA4WayHandshakeFailures;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	unsigned int proto;
	unsigned int pairwise_cipher;
	unsigned int group_cipher;
	unsigned int key_mgmt;

	u8 *assoc_wpa_ie; /* Own WPA/RSN IE from (Re)AssocReq */
	size_t assoc_wpa_ie_len;
	u8 *ap_wpa_ie, *ap_rsn_ie;
	size_t ap_wpa_ie_len, ap_rsn_ie_len;
};


static inline void wpa_sm_set_state(struct wpa_sm *sm, wpa_states state)
{
	sm->ctx->set_state(sm->ctx->ctx, state);
}

static inline wpa_states wpa_sm_get_state(struct wpa_sm *sm)
{
	return sm->ctx->get_state(sm->ctx->ctx);
}

static inline void wpa_sm_req_scan(struct wpa_sm *sm, int sec, int usec)
{
	sm->ctx->req_scan(sm->ctx->ctx, sec, usec);
}

static inline void wpa_sm_deauthenticate(struct wpa_sm *sm, int reason_code)
{
	sm->ctx->deauthenticate(sm->ctx->ctx, reason_code);
}

static inline void wpa_sm_disassociate(struct wpa_sm *sm, int reason_code)
{
	sm->ctx->disassociate(sm->ctx->ctx, reason_code);
}

static inline int wpa_sm_set_key(struct wpa_sm *sm, wpa_alg alg,
				 const u8 *addr, int key_idx, int set_tx,
				 const u8 *seq, size_t seq_len,
				 const u8 *key, size_t key_len)
{
	return sm->ctx->set_key(sm->ctx->ctx, alg, addr, key_idx, set_tx,
				seq, seq_len, key, key_len);
}

static inline struct wpa_ssid * wpa_sm_get_ssid(struct wpa_sm *sm)
{
	return sm->ctx->get_ssid(sm->ctx->ctx);
}

static inline int wpa_sm_get_bssid(struct wpa_sm *sm, u8 *bssid)
{
	return sm->ctx->get_bssid(sm->ctx->ctx, bssid);
}

static inline int wpa_sm_ether_send(struct wpa_sm *sm, const u8 *dest,
				    u16 proto, const u8 *buf, size_t len)
{
	return sm->ctx->ether_send(sm->ctx->ctx, dest, proto, buf, len);
}

static inline int wpa_sm_get_beacon_ie(struct wpa_sm *sm)
{
	return sm->ctx->get_beacon_ie(sm->ctx->ctx);
}

static inline void wpa_sm_cancel_auth_timeout(struct wpa_sm *sm)
{
	sm->ctx->cancel_auth_timeout(sm->ctx->ctx);
}

static inline u8 * wpa_sm_alloc_eapol(struct wpa_sm *sm, u8 type,
				      const void *data, u16 data_len,
				      size_t *msg_len, void **data_pos)
{
	return sm->ctx->alloc_eapol(sm->ctx->ctx, type, data, data_len,
				    msg_len, data_pos);
}

static inline int wpa_sm_add_pmkid(struct wpa_sm *sm, const u8 *bssid,
				   const u8 *pmkid)
{
	return sm->ctx->add_pmkid(sm->ctx->ctx, bssid, pmkid);
}

static inline int wpa_sm_remove_pmkid(struct wpa_sm *sm, const u8 *bssid,
				      const u8 *pmkid)
{
	return sm->ctx->remove_pmkid(sm->ctx->ctx, bssid, pmkid);
}

#endif /* WPA_I_H */
