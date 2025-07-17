/*
 * Internal WPA/RSN supplicant state machine definitions
 * Copyright (c) 2004-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_I_H
#define WPA_I_H

#include "utils/list.h"

struct wpa_tdls_peer;
struct wpa_eapol_key;

struct pasn_ft_r1kh {
	u8 bssid[ETH_ALEN];
	u8 r1kh_id[FT_R1KH_ID_LEN];
};

/**
 * struct wpa_sm - Internal WPA state machine data
 */
struct wpa_sm {
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	struct wpa_ptk ptk, tptk;
	int ptk_set, tptk_set;
	bool tk_set; /* Whether any TK is configured to the driver */
	unsigned int msg_3_of_4_ok:1;
	u8 snonce[WPA_NONCE_LEN];
	u8 anonce[WPA_NONCE_LEN]; /* ANonce from the last 1/4 msg */
	int renew_snonce;
	u8 rx_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int rx_replay_counter_set;
	u8 request_counter[WPA_REPLAY_COUNTER_LEN];
	struct wpa_gtk gtk;
	struct wpa_gtk gtk_wnm_sleep;
	struct wpa_igtk igtk;
	struct wpa_igtk igtk_wnm_sleep;
	struct wpa_bigtk bigtk;
	struct wpa_bigtk bigtk_wnm_sleep;

	struct eapol_sm *eapol; /* EAPOL state machine from upper level code */

	struct rsn_pmksa_cache *pmksa; /* PMKSA cache */
	struct rsn_pmksa_cache_entry *cur_pmksa; /* current PMKSA entry */
	struct dl_list pmksa_candidates;

	struct l2_packet_data *l2_preauth;
	struct l2_packet_data *l2_preauth_br;
	struct l2_packet_data *l2_tdls;
	u8 preauth_bssid[ETH_ALEN]; /* current RSN pre-auth peer or
				     * 00:00:00:00:00:00 if no pre-auth is
				     * in progress */
	struct eapol_sm *preauth_eapol;

	struct wpa_sm_ctx *ctx;

	void *scard_ctx; /* context for smartcard callbacks */
	int fast_reauth; /* whether EAP fast re-authentication is enabled */

	void *network_ctx;
	int allowed_pairwise_cipher; /* bitfield of WPA_CIPHER_* */
	int proactive_key_caching;
	int eap_workaround;
	void *eap_conf_ctx;
	u8 ssid[32];
	size_t ssid_len;
	int wpa_ptk_rekey;
	int wpa_deny_ptk0_rekey:1;
	int p2p;
	int wpa_rsc_relaxation;
	int owe_ptk_workaround;
	int beacon_prot;
	int ext_key_id; /* whether Extended Key ID is enabled */
	int use_ext_key_id; /* whether Extended Key ID has been detected
			     * to be used */
	int keyidx_active; /* Key ID for the active TK */

	/*
	 * If set Key Derivation Key should be derived as part of PMK to
	 * PTK derivation regardless of advertised capabilities.
	 */
	bool force_kdk_derivation;

	u8 own_addr[ETH_ALEN];
	const char *ifname;
	const char *bridge_ifname;
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
	unsigned int mgmt_group_cipher;

	int rsn_enabled; /* Whether RSN is enabled in configuration */
	int mfp; /* 0 = disabled, 1 = optional, 2 = mandatory */
	int ocv; /* Operating Channel Validation */
	enum sae_pwe sae_pwe; /* SAE PWE generation options */

	unsigned int sae_pk:1; /* whether SAE-PK is used */
	unsigned int secure_ltf:1;
	unsigned int secure_rtt:1;
	unsigned int prot_range_neg:1;
	unsigned int ssid_protection:1;

	u8 *assoc_wpa_ie; /* Own WPA/RSN IE from (Re)AssocReq */
	size_t assoc_wpa_ie_len;
	u8 *assoc_rsnxe; /* Own RSNXE from (Re)AssocReq */
	size_t assoc_rsnxe_len;
	u8 *ap_wpa_ie, *ap_rsn_ie, *ap_rsnxe;
	size_t ap_wpa_ie_len, ap_rsn_ie_len, ap_rsnxe_len;

#ifdef CONFIG_TDLS
	struct wpa_tdls_peer *tdls;
	int tdls_prohibited;
	int tdls_chan_switch_prohibited;
	int tdls_disabled;

	/* The driver supports TDLS */
	int tdls_supported;

	/*
	 * The driver requires explicit discovery/setup/teardown frames sent
	 * to it via tdls_mgmt.
	 */
	int tdls_external_setup;

	/* The driver supports TDLS channel switching */
	int tdls_chan_switch;
#endif /* CONFIG_TDLS */

#ifdef CONFIG_IEEE80211R
	u8 xxkey[PMK_LEN_MAX]; /* PSK or the second 256 bits of MSK, or the
				* first 384 bits of MSK */
	size_t xxkey_len;
	u8 pmk_r0[PMK_LEN_MAX];
	size_t pmk_r0_len;
	u8 pmk_r0_name[WPA_PMK_NAME_LEN];
	u8 pmk_r1[PMK_LEN_MAX];
	size_t pmk_r1_len;
	u8 pmk_r1_name[WPA_PMK_NAME_LEN];
	u8 mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	u8 key_mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	u8 r0kh_id[FT_R0KH_ID_MAX_LEN];
	size_t r0kh_id_len;
	u8 r1kh_id[FT_R1KH_ID_LEN];
	unsigned int ft_completed:1;
	unsigned int ft_reassoc_completed:1;
	unsigned int ft_protocol:1;
	int over_the_ds_in_progress;
	u8 target_ap[ETH_ALEN]; /* over-the-DS target AP */
	int set_ptk_after_assoc;
	u8 mdie_ft_capab; /* FT Capability and Policy from target AP MDIE */
	u8 *assoc_resp_ies; /* MDIE and FTIE from (Re)Association Response */
	size_t assoc_resp_ies_len;
#ifdef CONFIG_PASN
	/*
	 * Currently, the WPA state machine stores the PMK-R1, PMK-R1-Name and
	 * R1KH-ID only for the current association. As PMK-R1 is required to
	 * perform PASN authentication with FT, store the R1KH-ID for previous
	 * associations, which would later be used to derive the PMK-R1 as part
	 * of the PASN authentication flow.
	 */
	struct pasn_ft_r1kh *pasn_r1kh;
	unsigned int n_pasn_r1kh;
#endif /* CONFIG_PASN */
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_P2P
	u8 p2p_ip_addr[3 * 4];
#endif /* CONFIG_P2P */

#ifdef CONFIG_TESTING_OPTIONS
	struct wpabuf *test_assoc_ie;
	struct wpabuf *test_eapol_m2_elems;
	struct wpabuf *test_eapol_m4_elems;
	int ft_rsnxe_used;
	unsigned int oci_freq_override_eapol;
	unsigned int oci_freq_override_eapol_g2;
	unsigned int oci_freq_override_ft_assoc;
	unsigned int oci_freq_override_fils_assoc;
	unsigned int disable_eapol_g2_tx;
	bool encrypt_eapol_m2;
	bool encrypt_eapol_m4;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_FILS
	u8 fils_nonce[FILS_NONCE_LEN];
	u8 fils_session[FILS_SESSION_LEN];
	u8 fils_anonce[FILS_NONCE_LEN];
	u8 fils_key_auth_ap[FILS_MAX_KEY_AUTH_LEN];
	u8 fils_key_auth_sta[FILS_MAX_KEY_AUTH_LEN];
	size_t fils_key_auth_len;
	unsigned int fils_completed:1;
	unsigned int fils_erp_pmkid_set:1;
	unsigned int fils_cache_id_set:1;
	u8 fils_erp_pmkid[PMKID_LEN];
	u8 fils_cache_id[FILS_CACHE_ID_LEN];
	struct crypto_ecdh *fils_ecdh;
	int fils_dh_group;
	size_t fils_dh_elem_len;
	struct wpabuf *fils_ft_ies;
	u8 fils_ft[FILS_FT_MAX_LEN];
	size_t fils_ft_len;
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	struct crypto_ecdh *owe_ecdh;
	u16 owe_group;
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
	struct wpabuf *dpp_z;
	int dpp_pfs;
#endif /* CONFIG_DPP2 */
	struct wpa_sm_mlo mlo;

	bool wmm_enabled;
	bool driver_bss_selection;
	bool ft_prepend_pmkid;
};


static inline void wpa_sm_set_state(struct wpa_sm *sm, enum wpa_states state)
{
	WPA_ASSERT(sm->ctx->set_state);
	sm->ctx->set_state(sm->ctx->ctx, state);
}

static inline enum wpa_states wpa_sm_get_state(struct wpa_sm *sm)
{
	WPA_ASSERT(sm->ctx->get_state);
	return sm->ctx->get_state(sm->ctx->ctx);
}

static inline void wpa_sm_deauthenticate(struct wpa_sm *sm, u16 reason_code)
{
	WPA_ASSERT(sm->ctx->deauthenticate);
	sm->ctx->deauthenticate(sm->ctx->ctx, reason_code);
}

static inline int wpa_sm_set_key(struct wpa_sm *sm, int link_id,
				 enum wpa_alg alg, const u8 *addr, int key_idx,
				 int set_tx, const u8 *seq, size_t seq_len,
				 const u8 *key, size_t key_len,
				 enum key_flag key_flag)
{
	WPA_ASSERT(sm->ctx->set_key);
	return sm->ctx->set_key(sm->ctx->ctx, link_id, alg, addr, key_idx,
				set_tx, seq, seq_len, key, key_len, key_flag);
}

static inline void wpa_sm_reconnect(struct wpa_sm *sm)
{
	WPA_ASSERT(sm->ctx->reconnect);
	sm->ctx->reconnect(sm->ctx->ctx);
}

static inline void * wpa_sm_get_network_ctx(struct wpa_sm *sm)
{
	WPA_ASSERT(sm->ctx->get_network_ctx);
	return sm->ctx->get_network_ctx(sm->ctx->ctx);
}

static inline int wpa_sm_get_bssid(struct wpa_sm *sm, u8 *bssid)
{
	WPA_ASSERT(sm->ctx->get_bssid);
	return sm->ctx->get_bssid(sm->ctx->ctx, bssid);
}

static inline int wpa_sm_ether_send(struct wpa_sm *sm, const u8 *dest,
				    u16 proto, const u8 *buf, size_t len)
{
	WPA_ASSERT(sm->ctx->ether_send);
	return sm->ctx->ether_send(sm->ctx->ctx, dest, proto, buf, len);
}

static inline int wpa_sm_get_beacon_ie(struct wpa_sm *sm)
{
	WPA_ASSERT(sm->ctx->get_beacon_ie);
	return sm->ctx->get_beacon_ie(sm->ctx->ctx);
}

static inline void wpa_sm_cancel_auth_timeout(struct wpa_sm *sm)
{
	WPA_ASSERT(sm->ctx->cancel_auth_timeout);
	sm->ctx->cancel_auth_timeout(sm->ctx->ctx);
}

static inline u8 * wpa_sm_alloc_eapol(struct wpa_sm *sm, u8 type,
				      const void *data, u16 data_len,
				      size_t *msg_len, void **data_pos)
{
	WPA_ASSERT(sm->ctx->alloc_eapol);
	return sm->ctx->alloc_eapol(sm->ctx->ctx, type, data, data_len,
				    msg_len, data_pos);
}

static inline int wpa_sm_add_pmkid(struct wpa_sm *sm, void *network_ctx,
				   const u8 *bssid, const u8 *pmkid,
				   const u8 *cache_id, const u8 *pmk,
				   size_t pmk_len, u32 pmk_lifetime,
				   u8 pmk_reauth_threshold, int akmp)
{
	WPA_ASSERT(sm->ctx->add_pmkid);
	return sm->ctx->add_pmkid(sm->ctx->ctx, network_ctx, bssid, pmkid,
				  cache_id, pmk, pmk_len, pmk_lifetime,
				  pmk_reauth_threshold, akmp);
}

static inline int wpa_sm_remove_pmkid(struct wpa_sm *sm, void *network_ctx,
				      const u8 *bssid, const u8 *pmkid,
				      const u8 *cache_id)
{
	WPA_ASSERT(sm->ctx->remove_pmkid);
	return sm->ctx->remove_pmkid(sm->ctx->ctx, network_ctx, bssid, pmkid,
				     cache_id);
}

static inline int wpa_sm_mlme_setprotection(struct wpa_sm *sm, const u8 *addr,
					    int protect_type, int key_type)
{
	WPA_ASSERT(sm->ctx->mlme_setprotection);
	return sm->ctx->mlme_setprotection(sm->ctx->ctx, addr, protect_type,
					   key_type);
}

static inline int wpa_sm_update_ft_ies(struct wpa_sm *sm, const u8 *md,
				       const u8 *ies, size_t ies_len)
{
	if (sm->ctx->update_ft_ies)
		return sm->ctx->update_ft_ies(sm->ctx->ctx, md, ies, ies_len);
	return -1;
}

static inline int wpa_sm_send_ft_action(struct wpa_sm *sm, u8 action,
					const u8 *target_ap,
					const u8 *ies, size_t ies_len)
{
	if (sm->ctx->send_ft_action)
		return sm->ctx->send_ft_action(sm->ctx->ctx, action, target_ap,
					       ies, ies_len);
	return -1;
}

static inline int wpa_sm_mark_authenticated(struct wpa_sm *sm,
					    const u8 *target_ap)
{
	if (sm->ctx->mark_authenticated)
		return sm->ctx->mark_authenticated(sm->ctx->ctx, target_ap);
	return -1;
}

static inline void wpa_sm_set_rekey_offload(struct wpa_sm *sm)
{
	if (!sm->ctx->set_rekey_offload)
		return;
	sm->ctx->set_rekey_offload(sm->ctx->ctx, sm->ptk.kek, sm->ptk.kek_len,
				   sm->ptk.kck, sm->ptk.kck_len,
				   sm->rx_replay_counter);
}

#ifdef CONFIG_TDLS
static inline int wpa_sm_tdls_get_capa(struct wpa_sm *sm,
				       int *tdls_supported,
				       int *tdls_ext_setup,
				       int *tdls_chan_switch)
{
	if (sm->ctx->tdls_get_capa)
		return sm->ctx->tdls_get_capa(sm->ctx->ctx, tdls_supported,
					      tdls_ext_setup, tdls_chan_switch);
	return -1;
}

static inline int wpa_sm_send_tdls_mgmt(struct wpa_sm *sm, const u8 *dst,
					u8 action_code, u8 dialog_token,
					u16 status_code, u32 peer_capab,
					int initiator, const u8 *buf,
					size_t len, int link_id)
{
	if (sm->ctx->send_tdls_mgmt)
		return sm->ctx->send_tdls_mgmt(sm->ctx->ctx, dst, action_code,
					       dialog_token, status_code,
					       peer_capab, initiator, buf,
					       len, link_id);
	return -1;
}

static inline int wpa_sm_tdls_oper(struct wpa_sm *sm, int oper,
				   const u8 *peer)
{
	if (sm->ctx->tdls_oper)
		return sm->ctx->tdls_oper(sm->ctx->ctx, oper, peer);
	return -1;
}

static inline int
wpa_sm_tdls_peer_addset(struct wpa_sm *sm, const u8 *addr, int add,
			u16 aid, u16 capability, const u8 *supp_rates,
			size_t supp_rates_len,
			const struct ieee80211_ht_capabilities *ht_capab,
			const struct ieee80211_vht_capabilities *vht_capab,
			const struct ieee80211_he_capabilities *he_capab,
			size_t he_capab_len,
			const struct ieee80211_he_6ghz_band_cap *he_6ghz_capab,
			u8 qosinfo, int wmm, const u8 *ext_capab,
			size_t ext_capab_len, const u8 *supp_channels,
			size_t supp_channels_len, const u8 *supp_oper_classes,
			size_t supp_oper_classes_len,
			const struct ieee80211_eht_capabilities *eht_capab,
			size_t eht_capab_len, int mld_link_id)
{
	if (sm->ctx->tdls_peer_addset)
		return sm->ctx->tdls_peer_addset(sm->ctx->ctx, addr, add,
						 aid, capability, supp_rates,
						 supp_rates_len, ht_capab,
						 vht_capab,
						 he_capab, he_capab_len,
						 he_6ghz_capab, qosinfo, wmm,
						 ext_capab, ext_capab_len,
						 supp_channels,
						 supp_channels_len,
						 supp_oper_classes,
						 supp_oper_classes_len,
						 eht_capab, eht_capab_len,
						 mld_link_id);
	return -1;
}

static inline int
wpa_sm_tdls_enable_channel_switch(struct wpa_sm *sm, const u8 *addr,
				  u8 oper_class,
				  const struct hostapd_freq_params *freq_params)
{
	if (sm->ctx->tdls_enable_channel_switch)
		return sm->ctx->tdls_enable_channel_switch(sm->ctx->ctx, addr,
							   oper_class,
							   freq_params);
	return -1;
}

static inline int
wpa_sm_tdls_disable_channel_switch(struct wpa_sm *sm, const u8 *addr)
{
	if (sm->ctx->tdls_disable_channel_switch)
		return sm->ctx->tdls_disable_channel_switch(sm->ctx->ctx, addr);
	return -1;
}
#endif /* CONFIG_TDLS */

static inline int wpa_sm_key_mgmt_set_pmk(struct wpa_sm *sm,
					  const u8 *pmk, size_t pmk_len)
{
	if (!sm->ctx->key_mgmt_set_pmk)
		return -1;
	return sm->ctx->key_mgmt_set_pmk(sm->ctx->ctx, pmk, pmk_len);
}

static inline void wpa_sm_fils_hlp_rx(struct wpa_sm *sm,
				      const u8 *dst, const u8 *src,
				      const u8 *pkt, size_t pkt_len)
{
	if (sm->ctx->fils_hlp_rx)
		sm->ctx->fils_hlp_rx(sm->ctx->ctx, dst, src, pkt, pkt_len);
}

static inline int wpa_sm_channel_info(struct wpa_sm *sm,
				      struct wpa_channel_info *ci)
{
	if (!sm->ctx->channel_info)
		return -1;
	return sm->ctx->channel_info(sm->ctx->ctx, ci);
}

static inline void wpa_sm_transition_disable(struct wpa_sm *sm, u8 bitmap)
{
	if (sm->ctx->transition_disable)
		sm->ctx->transition_disable(sm->ctx->ctx, bitmap);
}

static inline void wpa_sm_store_ptk(struct wpa_sm *sm,
				    const u8 *addr, int cipher,
				    u32 life_time, struct wpa_ptk *ptk)
{
	if (sm->ctx->store_ptk)
		sm->ctx->store_ptk(sm->ctx->ctx, addr, cipher, life_time,
				   ptk);
}

#ifdef CONFIG_PASN
static inline int wpa_sm_set_ltf_keyseed(struct wpa_sm *sm, const u8 *own_addr,
					 const u8 *peer_addr,
					 size_t ltf_keyseed_len,
					 const u8 *ltf_keyseed)
{
	WPA_ASSERT(sm->ctx->set_ltf_keyseed);
	return sm->ctx->set_ltf_keyseed(sm->ctx->ctx, own_addr, peer_addr,
					ltf_keyseed_len, ltf_keyseed);
}
#endif /* CONFIG_PASN */

static inline void
wpa_sm_notify_pmksa_cache_entry(struct wpa_sm *sm,
				struct rsn_pmksa_cache_entry *entry)
{
	if (sm->ctx->notify_pmksa_cache_entry)
		sm->ctx->notify_pmksa_cache_entry(sm->ctx->ctx, entry);
}

static inline void wpa_sm_ssid_verified(struct wpa_sm *sm)
{
	if (sm->ctx->ssid_verified)
		sm->ctx->ssid_verified(sm->ctx->ctx);
}

int wpa_eapol_key_send(struct wpa_sm *sm, struct wpa_ptk *ptk,
		       int ver, const u8 *dest, u16 proto,
		       u8 *msg, size_t msg_len, u8 *key_mic);
int wpa_supplicant_send_2_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       int ver, const u8 *nonce,
			       const u8 *wpa_ie, size_t wpa_ie_len,
			       struct wpa_ptk *ptk);
int wpa_supplicant_send_4_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       u16 ver, u16 key_info,
			       struct wpa_ptk *ptk);

int wpa_derive_ptk_ft(struct wpa_sm *sm, const unsigned char *src_addr,
		      const struct wpa_eapol_key *key, struct wpa_ptk *ptk);

void wpa_tdls_assoc(struct wpa_sm *sm);
void wpa_tdls_disassoc(struct wpa_sm *sm);

#endif /* WPA_I_H */
