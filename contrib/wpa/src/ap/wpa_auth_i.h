/*
 * hostapd - IEEE 802.11i-2004 / WPA Authenticator: Internal definitions
 * Copyright (c) 2004-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_AUTH_I_H
#define WPA_AUTH_I_H

#include "utils/list.h"

/* max(dot11RSNAConfigGroupUpdateCount,dot11RSNAConfigPairwiseUpdateCount) */
#define RSNA_MAX_EAPOL_RETRIES 4

struct wpa_group;

struct wpa_state_machine {
	struct wpa_authenticator *wpa_auth;
	struct wpa_group *group;

	u8 addr[ETH_ALEN];
	u8 p2p_dev_addr[ETH_ALEN];
	u16 auth_alg;

	enum {
		WPA_PTK_INITIALIZE, WPA_PTK_DISCONNECT, WPA_PTK_DISCONNECTED,
		WPA_PTK_AUTHENTICATION, WPA_PTK_AUTHENTICATION2,
		WPA_PTK_INITPMK, WPA_PTK_INITPSK, WPA_PTK_PTKSTART,
		WPA_PTK_PTKCALCNEGOTIATING, WPA_PTK_PTKCALCNEGOTIATING2,
		WPA_PTK_PTKINITNEGOTIATING, WPA_PTK_PTKINITDONE
	} wpa_ptk_state;

	enum {
		WPA_PTK_GROUP_IDLE = 0,
		WPA_PTK_GROUP_REKEYNEGOTIATING,
		WPA_PTK_GROUP_REKEYESTABLISHED,
		WPA_PTK_GROUP_KEYERROR
	} wpa_ptk_group_state;

	bool Init;
	bool DeauthenticationRequest;
	bool AuthenticationRequest;
	bool ReAuthenticationRequest;
	bool Disconnect;
	u16 disconnect_reason; /* specific reason code to use with Disconnect */
	u32 TimeoutCtr;
	u32 GTimeoutCtr;
	bool TimeoutEvt;
	bool EAPOLKeyReceived;
	bool EAPOLKeyPairwise;
	bool EAPOLKeyRequest;
	bool MICVerified;
	bool GUpdateStationKeys;
	u8 ANonce[WPA_NONCE_LEN];
	u8 SNonce[WPA_NONCE_LEN];
	u8 alt_SNonce[WPA_NONCE_LEN];
	u8 alt_replay_counter[WPA_REPLAY_COUNTER_LEN];
	u8 PMK[PMK_LEN_MAX];
	unsigned int pmk_len;
	u8 pmkid[PMKID_LEN]; /* valid if pmkid_set == 1 */
	struct wpa_ptk PTK;
	u8 keyidx_active;
	bool use_ext_key_id;
	bool PTK_valid;
	bool pairwise_set;
	bool tk_already_set;
	int keycount;
	bool Pair;
	struct wpa_key_replay_counter {
		u8 counter[WPA_REPLAY_COUNTER_LEN];
		bool valid;
	} key_replay[RSNA_MAX_EAPOL_RETRIES],
		prev_key_replay[RSNA_MAX_EAPOL_RETRIES];
	bool PInitAKeys; /* WPA only, not in IEEE 802.11i */
	bool PTKRequest; /* not in IEEE 802.11i state machine */
	bool has_GTK;
	bool PtkGroupInit; /* init request for PTK Group state machine */

	u8 *last_rx_eapol_key; /* starting from IEEE 802.1X header */
	size_t last_rx_eapol_key_len;

	unsigned int changed:1;
	unsigned int in_step_loop:1;
	unsigned int pending_deinit:1;
	unsigned int started:1;
	unsigned int mgmt_frame_prot:1;
	unsigned int mfpr:1;
	unsigned int rx_eapol_key_secure:1;
	unsigned int update_snonce:1;
	unsigned int alt_snonce_valid:1;
	unsigned int waiting_radius_psk:1;
#ifdef CONFIG_IEEE80211R_AP
	unsigned int ft_completed:1;
	unsigned int pmk_r1_name_valid:1;
#endif /* CONFIG_IEEE80211R_AP */
	unsigned int is_wnmsleep:1;
	unsigned int pmkid_set:1;

	unsigned int ptkstart_without_success;

#ifdef CONFIG_OCV
	int ocv_enabled;
#endif /* CONFIG_OCV */

	u8 req_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int req_replay_counter_used;

	u8 *wpa_ie;
	size_t wpa_ie_len;
	u8 *rsnxe;
	size_t rsnxe_len;

	enum {
		WPA_VERSION_NO_WPA = 0 /* WPA not used */,
		WPA_VERSION_WPA = 1 /* WPA / IEEE 802.11i/D3.0 */,
		WPA_VERSION_WPA2 = 2 /* WPA2 / IEEE 802.11i */
	} wpa;
	int pairwise; /* Pairwise cipher suite, WPA_CIPHER_* */
	int wpa_key_mgmt; /* the selected WPA_KEY_MGMT_* */
	struct rsn_pmksa_cache_entry *pmksa;

	u32 dot11RSNAStatsTKIPLocalMICFailures;
	u32 dot11RSNAStatsTKIPRemoteMICFailures;

#ifdef CONFIG_IEEE80211R_AP
	u8 xxkey[PMK_LEN_MAX]; /* PSK or the second 256 bits of MSK, or the
				* first 384 bits of MSK */
	size_t xxkey_len;
	u8 pmk_r1[PMK_LEN_MAX];
	unsigned int pmk_r1_len;
	u8 pmk_r1_name[WPA_PMK_NAME_LEN]; /* PMKR1Name derived from FT Auth
					   * Request */
	u8 r0kh_id[FT_R0KH_ID_MAX_LEN]; /* R0KH-ID from FT Auth Request */
	size_t r0kh_id_len;
	u8 *assoc_resp_ftie;

	void (*ft_pending_cb)(void *ctx, const u8 *dst,
			      u16 auth_transaction, u16 status,
			      const u8 *ies, size_t ies_len);
	void *ft_pending_cb_ctx;
	struct wpabuf *ft_pending_req_ies;
	u8 ft_pending_pull_nonce[FT_RRB_NONCE_LEN];
	u8 ft_pending_auth_transaction;
	u8 ft_pending_current_ap[ETH_ALEN];
	int ft_pending_pull_left_retries;
#endif /* CONFIG_IEEE80211R_AP */

	int pending_1_of_4_timeout;

#ifdef CONFIG_P2P
	u8 ip_addr[4];
	unsigned int ip_addr_bit;
#endif /* CONFIG_P2P */

#ifdef CONFIG_FILS
	u8 fils_key_auth_sta[FILS_MAX_KEY_AUTH_LEN];
	u8 fils_key_auth_ap[FILS_MAX_KEY_AUTH_LEN];
	size_t fils_key_auth_len;
	unsigned int fils_completed:1;
#endif /* CONFIG_FILS */

#ifdef CONFIG_DPP2
	struct wpabuf *dpp_z;
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	void (*eapol_status_cb)(void *ctx1, void *ctx2);
	void *eapol_status_cb_ctx1;
	void *eapol_status_cb_ctx2;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211BE
	u8 peer_mld_addr[ETH_ALEN];
	s8 mld_assoc_link_id;
	u8 n_mld_affiliated_links;

	struct mld_link {
		bool valid;
		u8 peer_addr[ETH_ALEN];

		struct wpa_authenticator *wpa_auth;
	} mld_links[MAX_NUM_MLD_LINKS];
#endif /* CONFIG_IEEE80211BE */

	bool ssid_protection;
};


/* per group key state machine data */
struct wpa_group {
	struct wpa_group *next;
	int vlan_id;

	bool GInit;
	int GKeyDoneStations;
	bool GTKReKey;
	int GTK_len;
	int GN, GM;
	bool GTKAuthenticator;
	u8 Counter[WPA_NONCE_LEN];

	enum {
		WPA_GROUP_GTK_INIT = 0,
		WPA_GROUP_SETKEYS, WPA_GROUP_SETKEYSDONE,
		WPA_GROUP_FATAL_FAILURE
	} wpa_group_state;

	u8 GMK[WPA_GMK_LEN];
	u8 GTK[2][WPA_GTK_MAX_LEN];
	u8 GNonce[WPA_NONCE_LEN];
	bool changed;
	bool first_sta_seen;
	bool reject_4way_hs_for_entropy;
	u8 IGTK[2][WPA_IGTK_MAX_LEN];
	u8 BIGTK[2][WPA_IGTK_MAX_LEN];
	int GN_igtk, GM_igtk;
	int GN_bigtk, GM_bigtk;
	bool bigtk_set;
	bool bigtk_configured;
	/* Number of references except those in struct wpa_group->next */
	unsigned int references;
	unsigned int num_setup_iface;
};


struct wpa_ft_pmk_cache;

/* per authenticator data */
struct wpa_authenticator {
	struct wpa_group *group;

	unsigned int dot11RSNAStatsTKIPRemoteMICFailures;
	u32 dot11RSNAAuthenticationSuiteSelected;
	u32 dot11RSNAPairwiseCipherSelected;
	u32 dot11RSNAGroupCipherSelected;
	u8 dot11RSNAPMKIDUsed[PMKID_LEN];
	u32 dot11RSNAAuthenticationSuiteRequested; /* FIX: update */
	u32 dot11RSNAPairwiseCipherRequested; /* FIX: update */
	u32 dot11RSNAGroupCipherRequested; /* FIX: update */
	unsigned int dot11RSNATKIPCounterMeasuresInvoked;
	unsigned int dot11RSNA4WayHandshakeFailures;

	struct wpa_auth_config conf;
	const struct wpa_auth_callbacks *cb;
	void *cb_ctx;

	u8 *wpa_ie;
	size_t wpa_ie_len;

	u8 addr[ETH_ALEN];

	struct rsn_pmksa_cache *pmksa;
	struct wpa_ft_pmk_cache *ft_pmk_cache;

	bool non_tx_beacon_prot;

#ifdef CONFIG_P2P
	struct bitfield *ip_pool;
#endif /* CONFIG_P2P */

#ifdef CONFIG_IEEE80211BE
	bool is_ml;
	u8 mld_addr[ETH_ALEN];
	u8 link_id;
	bool primary_auth;
#endif /* CONFIG_IEEE80211BE */
};


#ifdef CONFIG_IEEE80211R_AP

#define FT_REMOTE_SEQ_BACKLOG 16
struct ft_remote_seq_rx {
	u32 dom;
	struct os_reltime time_offset; /* local time - offset = remote time */

	/* accepted sequence numbers: (offset ... offset + 0x40000000]
	 *   (except those in last)
	 * dropped sequence numbers: (offset - 0x40000000 ... offset]
	 * all others trigger SEQ_REQ message (except first message)
	 */
	u32 last[FT_REMOTE_SEQ_BACKLOG];
	unsigned int num_last;
	u32 offsetidx;

	struct dl_list queue; /* send nonces + rrb msgs awaiting seq resp */
};

struct ft_remote_seq_tx {
	u32 dom; /* non zero if initialized */
	u32 seq;
};

struct ft_remote_seq {
	struct ft_remote_seq_rx rx;
	struct ft_remote_seq_tx tx;
};

#endif /* CONFIG_IEEE80211R_AP */


int wpa_write_rsn_ie(struct wpa_auth_config *conf, u8 *buf, size_t len,
		     const u8 *pmkid);
int wpa_write_rsnxe(struct wpa_auth_config *conf, u8 *buf, size_t len);
void wpa_auth_logger(struct wpa_authenticator *wpa_auth, const u8 *addr,
		     logger_level level, const char *txt);
void wpa_auth_vlogger(struct wpa_authenticator *wpa_auth, const u8 *addr,
		      logger_level level, const char *fmt, ...)
	PRINTF_FORMAT(4, 5);
void __wpa_send_eapol(struct wpa_authenticator *wpa_auth,
		      struct wpa_state_machine *sm, int key_info,
		      const u8 *key_rsc, const u8 *nonce,
		      const u8 *kde, size_t kde_len,
		      int keyidx, int encr, int force_version);
int wpa_auth_for_each_sta(struct wpa_authenticator *wpa_auth,
			  int (*cb)(struct wpa_state_machine *sm, void *ctx),
			  void *cb_ctx);
int wpa_auth_for_each_auth(struct wpa_authenticator *wpa_auth,
			   int (*cb)(struct wpa_authenticator *a, void *ctx),
			   void *cb_ctx);
void wpa_auth_store_ptksa(struct wpa_authenticator *wpa_auth,
			  const u8 *addr, int cipher,
			  u32 life_time, const struct wpa_ptk *ptk);

#ifdef CONFIG_IEEE80211R_AP
int wpa_write_mdie(struct wpa_auth_config *conf, u8 *buf, size_t len);
int wpa_write_ftie(struct wpa_auth_config *conf, int key_mgmt, size_t key_len,
		   const u8 *r0kh_id, size_t r0kh_id_len,
		   const u8 *anonce, const u8 *snonce,
		   u8 *buf, size_t len, const u8 *subelem,
		   size_t subelem_len, int rsnxe_used);
int wpa_auth_derive_ptk_ft(struct wpa_state_machine *sm, struct wpa_ptk *ptk,
			   u8 *pmk_r0, u8 *pmk_r1, u8 *pmk_r0_name,
			   size_t *key_len, size_t kdk_len);
void wpa_auth_ft_store_keys(struct wpa_state_machine *sm, const u8 *pmk_r0,
			    const u8 *pmk_r1, const u8 *pmk_r0_name,
			    size_t key_len);
struct wpa_ft_pmk_cache * wpa_ft_pmk_cache_init(void);
void wpa_ft_pmk_cache_deinit(struct wpa_ft_pmk_cache *cache);
void wpa_ft_install_ptk(struct wpa_state_machine *sm, int retry);
int wpa_ft_store_pmk_fils(struct wpa_state_machine *sm, const u8 *pmk_r0,
			  const u8 *pmk_r0_name);
#endif /* CONFIG_IEEE80211R_AP */

#endif /* WPA_AUTH_I_H */
