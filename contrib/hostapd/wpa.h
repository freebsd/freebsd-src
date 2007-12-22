/*
 * hostapd - IEEE 802.11i-2004 / WPA Authenticator
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_H
#define WPA_H

#include "wpa_common.h"

#define WPA_PMK_LEN PMK_LEN
#define WPA_GMK_LEN 32
#define WPA_GTK_MAX_LEN 32
#define PMKID_LEN 16

#define WPA_CAPABILITY_PREAUTH BIT(0)
#define WPA_CAPABILITY_MGMT_FRAME_PROTECTION BIT(6)
#define WPA_CAPABILITY_PEERKEY_ENABLED BIT(9)

struct wpa_eapol_key {
	u8 type;
	u16 key_info;
	u16 key_length;
	u8 replay_counter[WPA_REPLAY_COUNTER_LEN];
	u8 key_nonce[WPA_NONCE_LEN];
	u8 key_iv[16];
	u8 key_rsc[WPA_KEY_RSC_LEN];
	u8 key_id[8]; /* Reserved */
	u8 key_mic[16];
	u16 key_data_length;
	/* followed by key_data_length bytes of key_data */
} __attribute__ ((packed));

#define WPA_KEY_INFO_TYPE_MASK (BIT(0) | BIT(1) | BIT(2))
#define WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 BIT(0)
#define WPA_KEY_INFO_TYPE_HMAC_SHA1_AES BIT(1)
#define WPA_KEY_INFO_KEY_TYPE BIT(3) /* 1 = Pairwise, 0 = Group key */
/* bit4..5 is used in WPA, but is reserved in IEEE 802.11i/RSN */
#define WPA_KEY_INFO_KEY_INDEX_MASK (BIT(4) | BIT(5))
#define WPA_KEY_INFO_KEY_INDEX_SHIFT 4
#define WPA_KEY_INFO_INSTALL BIT(6) /* pairwise */
#define WPA_KEY_INFO_TXRX BIT(6) /* group */
#define WPA_KEY_INFO_ACK BIT(7)
#define WPA_KEY_INFO_MIC BIT(8)
#define WPA_KEY_INFO_SECURE BIT(9)
#define WPA_KEY_INFO_ERROR BIT(10)
#define WPA_KEY_INFO_REQUEST BIT(11)
#define WPA_KEY_INFO_ENCR_KEY_DATA BIT(12)
#define WPA_KEY_INFO_SMK_MESSAGE BIT(13)


/* per STA state machine data */

struct wpa_ptk {
	u8 mic_key[16]; /* EAPOL-Key MIC Key (MK) */
	u8 encr_key[16]; /* EAPOL-Key Encryption Key (EK) */
	u8 tk1[16]; /* Temporal Key 1 (TK1) */
	union {
		u8 tk2[16]; /* Temporal Key 2 (TK2) */
		struct {
			u8 tx_mic_key[8];
			u8 rx_mic_key[8];
		} auth;
	} u;
} __attribute__ ((packed));

struct wpa_authenticator;
struct wpa_state_machine;
struct rsn_pmksa_cache_entry;


struct wpa_auth_config {
	int wpa;
	int wpa_key_mgmt;
	int wpa_pairwise;
	int wpa_group;
	int wpa_group_rekey;
	int wpa_strict_rekey;
	int wpa_gmk_rekey;
	int rsn_preauth;
	int eapol_version;
	int peerkey;
	int wme_enabled;
#ifdef CONFIG_IEEE80211W
	enum {
		WPA_NO_IEEE80211W = 0,
		WPA_IEEE80211W_OPTIONAL = 1,
		WPA_IEEE80211W_REQUIRED = 2
	} ieee80211w;
#endif /* CONFIG_IEEE80211W */
};

typedef enum {
	LOGGER_DEBUG, LOGGER_INFO, LOGGER_WARNING
} logger_level;

typedef enum {
	WPA_EAPOL_portEnabled, WPA_EAPOL_portValid, WPA_EAPOL_authorized,
	WPA_EAPOL_portControl_Auto, WPA_EAPOL_keyRun, WPA_EAPOL_keyAvailable,
	WPA_EAPOL_keyDone, WPA_EAPOL_inc_EapolFramesTx
} wpa_eapol_variable;

struct wpa_auth_callbacks {
	void *ctx;
	void (*logger)(void *ctx, const u8 *addr, logger_level level,
		       const char *txt);
	void (*disconnect)(void *ctx, const u8 *addr, u16 reason);
	void (*mic_failure_report)(void *ctx, const u8 *addr);
	void (*set_eapol)(void *ctx, const u8 *addr, wpa_eapol_variable var,
			  int value);
	int (*get_eapol)(void *ctx, const u8 *addr, wpa_eapol_variable var);
	const u8 * (*get_psk)(void *ctx, const u8 *addr, const u8 *prev_psk);
	int (*get_pmk)(void *ctx, const u8 *addr, u8 *pmk, size_t *len);
	int (*set_key)(void *ctx, int vlan_id, const char *alg, const u8 *addr,
		       int idx, u8 *key, size_t key_len);
	int (*get_seqnum)(void *ctx, const u8 *addr, int idx, u8 *seq);
	int (*get_seqnum_igtk)(void *ctx, const u8 *addr, int idx, u8 *seq);
	int (*send_eapol)(void *ctx, const u8 *addr, const u8 *data,
			  size_t data_len, int encrypt);
	int (*for_each_sta)(void *ctx, int (*cb)(struct wpa_state_machine *sm,
						 void *ctx), void *cb_ctx);
};

struct wpa_authenticator * wpa_init(const u8 *addr,
				    struct wpa_auth_config *conf,
				    struct wpa_auth_callbacks *cb);
void wpa_deinit(struct wpa_authenticator *wpa_auth);
int wpa_reconfig(struct wpa_authenticator *wpa_auth,
		 struct wpa_auth_config *conf);

enum {
	WPA_IE_OK, WPA_INVALID_IE, WPA_INVALID_GROUP, WPA_INVALID_PAIRWISE,
	WPA_INVALID_AKMP, WPA_NOT_ENABLED, WPA_ALLOC_FAIL,
	WPA_MGMT_FRAME_PROTECTION_VIOLATION, WPA_INVALID_MGMT_GROUP_CIPHER
};
	
int wpa_validate_wpa_ie(struct wpa_authenticator *wpa_auth,
			struct wpa_state_machine *sm,
			const u8 *wpa_ie, size_t wpa_ie_len);
struct wpa_state_machine *
wpa_auth_sta_init(struct wpa_authenticator *wpa_auth, const u8 *addr);
void wpa_auth_sta_associated(struct wpa_authenticator *wpa_auth,
			     struct wpa_state_machine *sm);
void wpa_auth_sta_deinit(struct wpa_state_machine *sm);
void wpa_receive(struct wpa_authenticator *wpa_auth,
		 struct wpa_state_machine *sm,
		 u8 *data, size_t data_len);
typedef enum {
	WPA_AUTH, WPA_ASSOC, WPA_DISASSOC, WPA_DEAUTH, WPA_REAUTH,
	WPA_REAUTH_EAPOL
} wpa_event;
void wpa_remove_ptk(struct wpa_state_machine *sm);
void wpa_auth_sm_event(struct wpa_state_machine *sm, wpa_event event);
void wpa_auth_sm_notify(struct wpa_state_machine *sm);
void wpa_gtk_rekey(struct wpa_authenticator *wpa_auth);
int wpa_get_mib(struct wpa_authenticator *wpa_auth, char *buf, size_t buflen);
int wpa_get_mib_sta(struct wpa_state_machine *sm, char *buf, size_t buflen);
void wpa_auth_countermeasures_start(struct wpa_authenticator *wpa_auth);
int wpa_auth_pairwise_set(struct wpa_state_machine *sm);
int wpa_auth_sta_key_mgmt(struct wpa_state_machine *sm);
int wpa_auth_sta_wpa_version(struct wpa_state_machine *sm);
int wpa_auth_sta_clear_pmksa(struct wpa_state_machine *sm,
			     struct rsn_pmksa_cache_entry *entry);
struct rsn_pmksa_cache_entry *
wpa_auth_sta_get_pmksa(struct wpa_state_machine *sm);
void wpa_auth_sta_local_mic_failure_report(struct wpa_state_machine *sm);
const u8 * wpa_auth_get_wpa_ie(struct wpa_authenticator *wpa_auth,
			       size_t *len);
int wpa_auth_pmksa_add(struct wpa_state_machine *sm, const u8 *pmk,
		       int session_timeout, struct eapol_state_machine *eapol);
int wpa_auth_pmksa_add_preauth(struct wpa_authenticator *wpa_auth,
			       const u8 *pmk, size_t len, const u8 *sta_addr,
			       int session_timeout,
			       struct eapol_state_machine *eapol);
int wpa_auth_sta_set_vlan(struct wpa_state_machine *sm, int vlan_id);

#endif /* WPA_H */
