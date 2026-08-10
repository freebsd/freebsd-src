/*
 * PASN info for initiator and responder
 *
 * Copyright (C) 2019, Intel Corporation
 * Copyright (c) 2022, Jouni Malinen <j@w1.fi>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PASN_COMMON_H
#define PASN_COMMON_H

#include "common/wpa_common.h"
#ifdef CONFIG_SAE
#include "common/sae.h"
#endif /* CONFIG_SAE */

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of ECC groups supported for PASN */
#define MAX_NUM_OF_PASN_GROUPS 10

enum pasn_fils_state {
	PASN_FILS_STATE_NONE = 0,
	PASN_FILS_STATE_PENDING_AS,
	PASN_FILS_STATE_COMPLETE
};

struct pasn_fils {
	u8 state;
	u8 nonce[NONCE_LEN];
	u8 anonce[NONCE_LEN];
	u8 session[FILS_SESSION_LEN];
	u8 erp_pmkid[PMKID_LEN];
	bool completed;
	struct wpabuf *erp_resp;
};

struct pasn_data {
	/* External modules access below variables using setter and getter
	 * functions */
	int akmp;
	int cipher;
	u8 own_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];
	u8 bssid[ETH_ALEN];
	struct rsn_pmksa_cache *pmksa;
	bool derive_kdk;
	size_t kdk_len;
	void *cb_ctx;
	unsigned int auth_alg;
	u8 mld_addr[ETH_ALEN];
	bool is_ml_peer;
	int group_cipher;
	int group_mgmt_cipher;
	u16 rsn_capab;

#ifdef CONFIG_SAE
	struct sae_pt *pt;
#endif /* CONFIG_SAE */

	/* Responder */
	int wpa_key_mgmt;
	int rsn_pairwise;
	u64 rsnxe_capab;
	u8 *rsnxe_ie;
	bool custom_pmkid_valid;
	u8 custom_pmkid[PMKID_LEN];
	enum mfp_options ieee80211w;

	/* Counter from the decrypted password identifier blob (set when
	 * get_pt_for_pw_id() resolves an encrypted identifier) */
	unsigned int sae_pw_id_counter;
	/* Decrypted (real) password identifier resolved from an encrypted
	 * identifier blob; NULL for plaintext identifiers. Owned by pasn_data
	 * and freed by pasn_data_deinit(). */
	u8 *dec_pw_id;
	size_t dec_pw_id_len;

	/*
	 * Extra elements to add into Authentication frames. These can be used,
	 * e.g., for Wi-Fi Aware use cases.
	 */
	const u8 *extra_ies;
	size_t extra_ies_len;

	/* External modules do not access below variables */
	bool derive_kek;
	size_t kek_len;
	u16 group;
	u16 rejected_groups[MAX_NUM_OF_PASN_GROUPS];
	unsigned int rejected_group_idx;
	u16 ap_supported_groups[MAX_NUM_OF_PASN_GROUPS];
	unsigned int ap_supported_group_idx;
	bool secure_ltf;
	int freq;

	u8 trans_seq;
	u8 status;

	size_t pmk_len;
	u8 pmk[PMK_LEN_MAX];
	bool using_pmksa;
	enum rsn_hash_alg hash_alg;

	struct wpabuf *auth1;

	struct wpabuf *beacon_rsne_rsnxe;
	struct wpa_ptk ptk;
	struct crypto_ecdh *ecdh;

	struct wpabuf *comeback;
	u16 comeback_after;

#ifdef CONFIG_SAE
	struct sae_data sae;
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	bool fils_eapol;
	bool fils_wd_valid;
	struct pasn_fils fils;
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211R
	u8 pmk_r1[PMK_LEN_MAX];
	size_t pmk_r1_len;
	u8 pmk_r1_name[WPA_PMK_NAME_LEN];
#endif /* CONFIG_IEEE80211R */
	/* Note that this pointers to RSN PMKSA cache are actually defined
	 * differently for the PASN initiator (using RSN Supplicant
	 * implementation) and PASN responser (using RSN Authenticator
	 * implementation). Functions cannot be mixed between those cases. */
	struct rsn_pmksa_cache_entry *pmksa_entry;
	struct eapol_sm *eapol;
	int fast_reauth;
#ifdef CONFIG_TESTING_OPTIONS
	int corrupt_mic;
	/*
	 * Override Supported Groups element in the second PASN Authentication
	 * frame for group negotiation testing.
	 */
	const int *pasn_test_groups;
#endif /* CONFIG_TESTING_OPTIONS */
	int network_id;
	void *network_ctx;

	u8 wrapped_data_format;
	struct wpabuf *secret;

	/* Responder */
	bool noauth; /* Whether PASN without mutual authentication is enabled */
#ifdef CONFIG_ENC_ASSOC
	bool eppke_unauth; /* Whether unauthenticated EPPKE is enabled */
#endif /* CONFIG_ENC_ASSOC */
	int disable_pmksa_caching;
	int *pasn_groups;
	int use_anti_clogging;
	u8 *rsn_ie;
	size_t rsn_ie_len;

	u8 *comeback_key;
	struct os_reltime last_comeback_key_update;
	u16 comeback_idx;
	u16 *comeback_pending_idx;
	struct wpabuf *frame;
#ifdef CONFIG_ENC_ASSOC
	bool authorized;
	bool tk_configured;
#endif /* CONFIG_ENC_ASSOC */
#ifdef CONFIG_PMKSA_PRIVACY
	bool pmksa_caching_privacy;
	u8 epp_pmkid_cur[PMKID_LEN];
#endif /* CONFIG_PMKSA_PRIVACY */

	/**
	 * send_mgmt - Function handler to transmit a Management frame
	 * @ctx: Callback context from cb_ctx
	 * @frame_buf : Frame to transmit
	 * @frame_len: Length of frame to transmit
	 * @freq: Frequency in MHz for the channel on which to transmit
	 * @wait_dur: How many milliseconds to wait for a response frame
	 * Returns: 0 on success, -1 on failure
	 */
	int (*send_mgmt)(void *ctx, const u8 *data, size_t data_len, int noack,
			 unsigned int freq, unsigned int wait);
	/**
	 * validate_custom_pmkid - Handler to validate vendor specific PMKID
	 * @ctx: Callback context from cb_ctx
	 * @addr : MAC address of the peer
	 * @pmkid: Custom PMKID
	 * Returns: 0 on success (valid PMKID), -1 on failure
	 */
	int (*validate_custom_pmkid)(void *ctx, const u8 *addr,
				     const u8 *pmkid);

	int (*prepare_data_element)(void *ctx, const u8 *peer_addr);

	int (*parse_data_element)(void *ctx, const u8 *data, size_t len);
#ifdef CONFIG_ENC_ASSOC
	int (*eppke_set_key)(void *ctx, enum wpa_alg alg, const u8 *addr,
			     int vlan_id, const u8 *key, size_t key_len);
#endif /* CONFIG_ENC_ASSOC */
	struct rsn_pmksa_cache_entry *
	(*pmksa_cache_search)(void *ctx, const u8 *spa, const u8 *pmkid,
			      bool is_ml);
#ifdef CONFIG_SAE
	/**
	 * get_pt_for_pw_id - Look up SAE PT for a given password identifier
	 * @ctx: Callback context from cb_ctx
	 * @pw_id: Password identifier received in the SAE commit frame
	 * @pw_id_len: Length of the password identifier
	 * @group: SAE group being used
	 * @password: Output pointer to the matching password string
	 * @counter: Output counter value from the decrypted identifier blob
	 *	(set to 0 for plaintext identifiers)
	 * @dec_pw_id: Output pointer to the decrypted (real) password
	 *	identifier for encrypted blobs; set to NULL for plaintext
	 *	identifiers. The caller takes ownership and must free with
	 *	os_free().
	 * @dec_pw_id_len: Output length of the decrypted password identifier
	 * Returns: SAE PT on success, NULL if not found
	 *
	 * This callback is invoked by the PASN responder when processing an
	 * SAE commit frame that contains a password identifier, allowing the
	 * AP to look up the correct PT at commit-processing time rather than
	 * at PASN-setup time.
	 */
	struct sae_pt * (*get_pt_for_pw_id)(void *ctx,
					    const u8 *pw_id, size_t pw_id_len,
					    int group,
					    const char **password,
					    unsigned int *counter,
					    u8 **dec_pw_id,
					    size_t *dec_pw_id_len);
#endif /* CONFIG_SAE */
};

/* Initiator */
void wpa_pasn_reset(struct pasn_data *pasn);
int wpas_pasn_start(struct pasn_data *pasn, const u8 *own_addr,
		    const u8 *peer_addr, const u8 *bssid,
		    int akmp, int cipher, u16 group,
		    int freq, const u8 *beacon_rsne, u8 beacon_rsne_len,
		    const u8 *beacon_rsnxe, u8 beacon_rsnxe_len,
		    const struct wpabuf *comeback);
struct wpabuf * wpas_pasn_build_auth_1(struct pasn_data *pasn,
				       const struct wpabuf *comeback,
				       bool verify, bool full_hdr);
struct wpabuf * wpas_pasn_build_auth_3(struct pasn_data *pasn, bool full_hdr);
int wpa_pasn_verify(struct pasn_data *pasn, const u8 *own_addr,
		    const u8 *peer_addr, const u8 *bssid,
		    int akmp, int cipher, u16 group,
		    int freq, const u8 *beacon_rsne, u8 beacon_rsne_len,
		    const u8 *beacon_rsnxe, u8 beacon_rsnxe_len,
		    const struct wpabuf *comeback);
int wpa_pasn_auth_rx(struct pasn_data *pasn, const u8 *data, size_t len,
		     struct wpa_pasn_params_data *pasn_params);
int wpa_pasn_auth_tx_status(struct pasn_data *pasn,
			    const u8 *data, size_t data_len, u8 acked);
int wpas_parse_pasn_frame(struct pasn_data *pasn, u16 auth_type,
			  u16 auth_transaction, u16 status_code,
			  const u8 *frame_data, size_t frame_data_len,
			  struct wpa_pasn_params_data *pasn_params);

/* Responder */
int handle_auth_pasn_1(struct pasn_data *pasn,
		       const u8 *own_addr, const u8 *peer_addr,
		       const struct ieee80211_mgmt *mgmt, size_t len,
		       bool reject);
int handle_auth_pasn_3(struct pasn_data *pasn, const u8 *own_addr,
		       const u8 *peer_addr,
		       const struct ieee80211_mgmt *mgmt, size_t len);
int handle_auth_pasn_resp(struct pasn_data *pasn, const u8 *own_addr,
			  const u8 *peer_addr,
			  struct rsn_pmksa_cache_entry *pmksa, u16 status);

struct pasn_data * pasn_data_init(void);
void pasn_data_deinit(struct pasn_data *pasn);
void pasn_register_callbacks(struct pasn_data *pasn, void *cb_ctx,
			     int (*send_mgmt)(void *ctx, const u8 *data,
					      size_t data_len, int noack,
					      unsigned int freq,
					      unsigned int wait),
			     int (*validate_custom_pmkid)(void *ctx,
							  const u8 *addr,
							  const u8 *pmkid),
			     int (*eppke_set_key)(void *ctx, enum wpa_alg alg,
						  const u8 *addr, int vlan_id,
						  const u8 *key,
						  size_t key_len),
			     struct rsn_pmksa_cache_entry *
			     (*pmksa_cache_search)(void *ctx, const u8 *spa,
						   const u8 *pmkid,
						   bool is_ml));

void pasn_enable_kdk_derivation(struct pasn_data *pasn);
void pasn_disable_kdk_derivation(struct pasn_data *pasn);

void pasn_set_akmp(struct pasn_data *pasn, int akmp);
void pasn_set_cipher(struct pasn_data *pasn, int cipher);
void pasn_set_own_addr(struct pasn_data *pasn, const u8 *addr);
void pasn_set_own_mld_addr(struct pasn_data *pasn, const u8 *addr);
void pasn_set_peer_addr(struct pasn_data *pasn, const u8 *addr);
void pasn_set_bssid(struct pasn_data *pasn, const u8 *addr);
void pasn_set_initiator_pmksa(struct pasn_data *pasn,
			      struct rsn_pmksa_cache *pmksa);
void pasn_set_responder_pmksa(struct pasn_data *pasn,
			      struct rsn_pmksa_cache *pmksa);
int pasn_set_pt(struct pasn_data *pasn, struct sae_pt *pt);
struct rsn_pmksa_cache * pasn_initiator_pmksa_cache_init(void);
void pasn_initiator_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa);
int pasn_initiator_pmksa_cache_add(struct rsn_pmksa_cache *pmksa,
				   const u8 *own_addr, const u8 *bssid,
				   const u8 *pmk, size_t pmk_len,
				   const u8 *pmkid, int akmp);
int pasn_initiator_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
				   const u8 *bssid, u8 *pmkid, u8 *pmk,
				   size_t *pmk_len);
void pasn_initiator_pmksa_cache_remove(struct rsn_pmksa_cache *pmksa,
				       const u8 *bssid);
void pasn_initiator_pmksa_cache_flush(struct rsn_pmksa_cache *pmksa);

/* Responder */
void pasn_set_noauth(struct pasn_data *pasn, bool noauth);
void pasn_set_wpa_key_mgmt(struct pasn_data *pasn, int key_mgmt);
void pasn_set_rsn_pairwise(struct pasn_data *pasn, int rsn_pairwise);
void pasn_set_rsne(struct pasn_data *pasn, const u8 *rsne);
void pasn_set_rsnxe_caps(struct pasn_data *pasn, u64 rsnxe_capab);
void pasn_set_rsnxe_ie(struct pasn_data *pasn, const u8 *rsnxe_ie);
void pasn_set_custom_pmkid(struct pasn_data *pasn, const u8 *pmkid);
int pasn_set_extra_ies(struct pasn_data *pasn, const u8 *extra_ies,
		       size_t extra_ies_len);
void pasn_set_mfp(struct pasn_data *pasn, enum mfp_options mfp);

struct rsn_pmksa_cache * pasn_responder_pmksa_cache_init(void);
void pasn_responder_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa);
int pasn_responder_pmksa_cache_add(struct rsn_pmksa_cache *pmksa,
				   const u8 *own_addr, const u8 *bssid,
				   const u8 *pmk, size_t pmk_len,
				   const u8 *pmkid, int akmp);
int pasn_responder_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
				   const u8 *bssid, u8 *pmkid, u8 *pmk,
				   size_t *pmk_len);
void pasn_responder_pmksa_cache_remove(struct rsn_pmksa_cache *pmksa,
				       const u8 *bssid);
void pasn_responder_pmksa_cache_flush(struct rsn_pmksa_cache *pmksa);

int pasn_get_akmp(struct pasn_data *pasn);
int pasn_get_cipher(struct pasn_data *pasn);
size_t pasn_get_pmk_len(struct pasn_data *pasn);
u8 * pasn_get_pmk(struct pasn_data *pasn);
struct wpa_ptk * pasn_get_ptk(struct pasn_data *pasn);
int pasn_add_encrypted_data(struct pasn_data *pasn, struct wpabuf *buf,
			    const u8 *data, size_t data_len);
int pasn_parse_encrypted_data(struct pasn_data *pasn, const u8 *data,
			      size_t len);

#ifdef __cplusplus
}
#endif
#endif /* PASN_COMMON_H */
