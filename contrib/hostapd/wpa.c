/*
 * hostapd - IEEE 802.11i-2004 / WPA Authenticator
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * $FreeBSD$
 */

#include "includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "hostapd.h"
#include "eapol_sm.h"
#include "wpa.h"
#include "wme.h"
#include "sha1.h"
#include "md5.h"
#include "rc4.h"
#include "aes_wrap.h"
#include "crypto.h"
#include "eloop.h"
#include "ieee802_11.h"
#include "pmksa_cache.h"
#include "state_machine.h"

#define STATE_MACHINE_DATA struct wpa_state_machine
#define STATE_MACHINE_DEBUG_PREFIX "WPA"
#define STATE_MACHINE_ADDR sm->addr


#define RSN_NUM_REPLAY_COUNTERS_1 0
#define RSN_NUM_REPLAY_COUNTERS_2 1
#define RSN_NUM_REPLAY_COUNTERS_4 2
#define RSN_NUM_REPLAY_COUNTERS_16 3


struct wpa_group;

struct wpa_stsl_negotiation {
	struct wpa_stsl_negotiation *next;
	u8 initiator[ETH_ALEN];
	u8 peer[ETH_ALEN];
};


struct wpa_state_machine {
	struct wpa_authenticator *wpa_auth;
	struct wpa_group *group;

	u8 addr[ETH_ALEN];

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

	Boolean Init;
	Boolean DeauthenticationRequest;
	Boolean AuthenticationRequest;
	Boolean ReAuthenticationRequest;
	Boolean Disconnect;
	int TimeoutCtr;
	int GTimeoutCtr;
	Boolean TimeoutEvt;
	Boolean EAPOLKeyReceived;
	Boolean EAPOLKeyPairwise;
	Boolean EAPOLKeyRequest;
	Boolean MICVerified;
	Boolean GUpdateStationKeys;
	u8 ANonce[WPA_NONCE_LEN];
	u8 SNonce[WPA_NONCE_LEN];
	u8 PMK[WPA_PMK_LEN];
	struct wpa_ptk PTK;
	Boolean PTK_valid;
	Boolean pairwise_set;
	int keycount;
	Boolean Pair;
	u8 key_replay_counter[WPA_REPLAY_COUNTER_LEN];
	Boolean key_replay_counter_valid;
	Boolean PInitAKeys; /* WPA only, not in IEEE 802.11i */
	Boolean PTKRequest; /* not in IEEE 802.11i state machine */
	Boolean has_GTK;

	u8 *last_rx_eapol_key; /* starting from IEEE 802.1X header */
	size_t last_rx_eapol_key_len;

	unsigned int changed:1;
	unsigned int in_step_loop:1;
	unsigned int pending_deinit:1;
	unsigned int started:1;
	unsigned int sta_counted:1;
	unsigned int mgmt_frame_prot:1;

	u8 req_replay_counter[WPA_REPLAY_COUNTER_LEN];
	int req_replay_counter_used;

	u8 *wpa_ie;
	size_t wpa_ie_len;

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
};


/* per group key state machine data */
struct wpa_group {
	struct wpa_group *next;
	int vlan_id;

	Boolean GInit;
	int GNoStations;
	int GKeyDoneStations;
	Boolean GTKReKey;
	int GTK_len;
	int GN, GM;
	Boolean GTKAuthenticator;
	u8 Counter[WPA_NONCE_LEN];

	enum {
		WPA_GROUP_GTK_INIT = 0,
		WPA_GROUP_SETKEYS, WPA_GROUP_SETKEYSDONE
	} wpa_group_state;

	u8 GMK[WPA_GMK_LEN];
	u8 GTK[2][WPA_GTK_MAX_LEN];
	u8 GNonce[WPA_NONCE_LEN];
	Boolean changed;
#ifdef CONFIG_IEEE80211W
	u8 DGTK[WPA_DGTK_LEN];
	u8 IGTK[2][WPA_IGTK_LEN];
#endif /* CONFIG_IEEE80211W */
};


/* per authenticator data */
struct wpa_authenticator {
	struct wpa_group *group;

	unsigned int dot11RSNAStatsTKIPRemoteMICFailures;
	u8 dot11RSNAAuthenticationSuiteSelected[4];
	u8 dot11RSNAPairwiseCipherSelected[4];
	u8 dot11RSNAGroupCipherSelected[4];
	u8 dot11RSNAPMKIDUsed[PMKID_LEN];
	u8 dot11RSNAAuthenticationSuiteRequested[4]; /* FIX: update */
	u8 dot11RSNAPairwiseCipherRequested[4]; /* FIX: update */
	u8 dot11RSNAGroupCipherRequested[4]; /* FIX: update */
	unsigned int dot11RSNATKIPCounterMeasuresInvoked;
	unsigned int dot11RSNA4WayHandshakeFailures;

	struct wpa_stsl_negotiation *stsl_negotiations;

	struct wpa_auth_config conf;
	struct wpa_auth_callbacks cb;

	u8 *wpa_ie;
	size_t wpa_ie_len;

	u8 addr[ETH_ALEN];

	struct rsn_pmksa_cache *pmksa;
};


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpa_sm_step(struct wpa_state_machine *sm);
static int wpa_verify_key_mic(struct wpa_ptk *PTK, u8 *data, size_t data_len);
static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx);
static void wpa_group_sm_step(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group);
static int wpa_stsl_remove(struct wpa_authenticator *wpa_auth,
			   struct wpa_stsl_negotiation *neg);
static void __wpa_send_eapol(struct wpa_authenticator *wpa_auth,
			     struct wpa_state_machine *sm, int key_info,
			     const u8 *key_rsc, const u8 *nonce,
			     const u8 *kde, size_t kde_len,
			     int keyidx, int encr, int force_version);

/* Default timeouts are 100 ms, but this seems to be a bit too fast for most
 * WPA Supplicants, so use a bit longer timeout. */
static const u32 dot11RSNAConfigGroupUpdateTimeOut = 1000; /* ms */
static const u32 dot11RSNAConfigGroupUpdateCount = 3;
static const u32 dot11RSNAConfigPairwiseUpdateTimeOut = 1000; /* ms */
static const u32 dot11RSNAConfigPairwiseUpdateCount = 3;

/* TODO: make these configurable */
static const int dot11RSNAConfigPMKLifetime = 43200;
static const int dot11RSNAConfigPMKReauthThreshold = 70;
static const int dot11RSNAConfigSATimeout = 60;


static const int WPA_SELECTOR_LEN = 4;
static const u8 WPA_OUI_TYPE[] = { 0x00, 0x50, 0xf2, 1 };
static const u16 WPA_VERSION = 1;
static const u8 WPA_AUTH_KEY_MGMT_UNSPEC_802_1X[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 WPA_CIPHER_SUITE_NONE[] = { 0x00, 0x50, 0xf2, 0 };
static const u8 WPA_CIPHER_SUITE_WEP40[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 WPA_CIPHER_SUITE_TKIP[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 WPA_CIPHER_SUITE_WRAP[] = { 0x00, 0x50, 0xf2, 3 };
static const u8 WPA_CIPHER_SUITE_CCMP[] = { 0x00, 0x50, 0xf2, 4 };
static const u8 WPA_CIPHER_SUITE_WEP104[] = { 0x00, 0x50, 0xf2, 5 };
#ifdef CONFIG_IEEE80211W
static const u8 RSN_CIPHER_SUITE_AES_128_CMAC[] = { 0x00, 0x0f, 0xac, 6 };
#endif /* CONFIG_IEEE80211W */

static const int RSN_SELECTOR_LEN = 4;
static const u16 RSN_VERSION = 1;
static const u8 RSN_AUTH_KEY_MGMT_UNSPEC_802_1X[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 RSN_CIPHER_SUITE_NONE[] = { 0x00, 0x0f, 0xac, 0 };
static const u8 RSN_CIPHER_SUITE_WEP40[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 RSN_CIPHER_SUITE_TKIP[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 RSN_CIPHER_SUITE_WRAP[] = { 0x00, 0x0f, 0xac, 3 };
static const u8 RSN_CIPHER_SUITE_CCMP[] = { 0x00, 0x0f, 0xac, 4 };
static const u8 RSN_CIPHER_SUITE_WEP104[] = { 0x00, 0x0f, 0xac, 5 };

/* EAPOL-Key Key Data Encapsulation
 * GroupKey and PeerKey require encryption, otherwise, encryption is optional.
 */
static const u8 RSN_KEY_DATA_GROUPKEY[] = { 0x00, 0x0f, 0xac, 1 };
#if 0
static const u8 RSN_KEY_DATA_STAKEY[] = { 0x00, 0x0f, 0xac, 2 };
#endif
static const u8 RSN_KEY_DATA_MAC_ADDR[] = { 0x00, 0x0f, 0xac, 3 };
static const u8 RSN_KEY_DATA_PMKID[] = { 0x00, 0x0f, 0xac, 4 };
#ifdef CONFIG_PEERKEY
static const u8 RSN_KEY_DATA_SMK[] = { 0x00, 0x0f, 0xac, 5 };
static const u8 RSN_KEY_DATA_NONCE[] = { 0x00, 0x0f, 0xac, 6 };
static const u8 RSN_KEY_DATA_LIFETIME[] = { 0x00, 0x0f, 0xac, 7 };
static const u8 RSN_KEY_DATA_ERROR[] = { 0x00, 0x0f, 0xac, 8 };
#endif /* CONFIG_PEERKEY */
#ifdef CONFIG_IEEE80211W
/* FIX: IEEE 802.11w/D1.0 is using subtypes 5 and 6 for these, but they were
 * already taken by 802.11ma (PeerKey). Need to update the values here once
 * IEEE 802.11w fixes these. */
static const u8 RSN_KEY_DATA_DHV[] = { 0x00, 0x0f, 0xac, 9 };
static const u8 RSN_KEY_DATA_IGTK[] = { 0x00, 0x0f, 0xac, 10 };
#endif /* CONFIG_IEEE80211W */

#ifdef CONFIG_PEERKEY
enum {
	STK_MUI_4WAY_STA_AP = 1,
	STK_MUI_4WAY_STAT_STA = 2,
	STK_MUI_GTK = 3,
	STK_MUI_SMK = 4
};

enum {
	STK_ERR_STA_NR = 1,
	STK_ERR_STA_NRSN = 2,
	STK_ERR_CPHR_NS = 3,
	STK_ERR_NO_STSL = 4
};
#endif /* CONFIG_PEERKEY */

#define GENERIC_INFO_ELEM 0xdd
#define RSN_INFO_ELEM 0x30

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

/* WPA IE version 1
 * 00-50-f2:1 (OUI:OUI type)
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: TKIP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: TKIP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * WPA Capabilities (2 octets, little endian) (default: 0)
 */

struct wpa_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 oui[3];
	u8 oui_type;
	u16 version;
} STRUCT_PACKED;


/* RSN IE version 1
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: CCMP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: CCMP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1X)
 * RSN Capabilities (2 octets, little endian) (default: 0)
 * PMKID Count (2 octets) (default: 0)
 * PMKID List (16 * n octets)
 * Management Group Cipher Suite (4 octets) (default: AES-128-CMAC)
 */

struct rsn_ie_hdr {
	u8 elem_id; /* WLAN_EID_RSN */
	u8 len;
	u16 version;
} STRUCT_PACKED;


struct rsn_error_kde {
	u16 mui;
	u16 error_type;
} STRUCT_PACKED;


#ifdef CONFIG_IEEE80211W
struct wpa_dhv_kde {
	u8 dhv[WPA_DHV_LEN];
} STRUCT_PACKED;

struct wpa_igtk_kde {
	u8 keyid[2];
	u8 pn[6];
	u8 igtk[WPA_IGTK_LEN];
} STRUCT_PACKED;
#endif /* CONFIG_IEEE80211W */

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */


static inline void wpa_auth_mic_failure_report(
	struct wpa_authenticator *wpa_auth, const u8 *addr)
{
	if (wpa_auth->cb.mic_failure_report)
		wpa_auth->cb.mic_failure_report(wpa_auth->cb.ctx, addr);
}


static inline void wpa_auth_set_eapol(struct wpa_authenticator *wpa_auth,
				      const u8 *addr, wpa_eapol_variable var,
				      int value)
{
	if (wpa_auth->cb.set_eapol)
		wpa_auth->cb.set_eapol(wpa_auth->cb.ctx, addr, var, value);
}


static inline int wpa_auth_get_eapol(struct wpa_authenticator *wpa_auth,
				     const u8 *addr, wpa_eapol_variable var)
{
	if (wpa_auth->cb.get_eapol == NULL)
		return -1;
	return wpa_auth->cb.get_eapol(wpa_auth->cb.ctx, addr, var);
}


static inline const u8 * wpa_auth_get_psk(struct wpa_authenticator *wpa_auth,
					  const u8 *addr, const u8 *prev_psk)
{
	if (wpa_auth->cb.get_psk == NULL)
		return NULL;
	return wpa_auth->cb.get_psk(wpa_auth->cb.ctx, addr, prev_psk);
}


static inline int wpa_auth_get_pmk(struct wpa_authenticator *wpa_auth,
				   const u8 *addr, u8 *pmk, size_t *len)
{
	if (wpa_auth->cb.get_pmk == NULL)
		return -1;
	return wpa_auth->cb.get_pmk(wpa_auth->cb.ctx, addr, pmk, len);
}


static inline int wpa_auth_set_key(struct wpa_authenticator *wpa_auth,
				   int vlan_id,
				   const char *alg, const u8 *addr, int idx,
				   u8 *key, size_t key_len)
{
	if (wpa_auth->cb.set_key == NULL)
		return -1;
	return wpa_auth->cb.set_key(wpa_auth->cb.ctx, vlan_id, alg, addr, idx,
				    key, key_len);
}


static inline int wpa_auth_get_seqnum(struct wpa_authenticator *wpa_auth,
				      const u8 *addr, int idx, u8 *seq)
{
	if (wpa_auth->cb.get_seqnum == NULL)
		return -1;
	return wpa_auth->cb.get_seqnum(wpa_auth->cb.ctx, addr, idx, seq);
}


static inline int wpa_auth_get_seqnum_igtk(struct wpa_authenticator *wpa_auth,
					   const u8 *addr, int idx, u8 *seq)
{
	if (wpa_auth->cb.get_seqnum_igtk == NULL)
		return -1;
	return wpa_auth->cb.get_seqnum_igtk(wpa_auth->cb.ctx, addr, idx, seq);
}


static inline int
wpa_auth_send_eapol(struct wpa_authenticator *wpa_auth, const u8 *addr,
		    const u8 *data, size_t data_len, int encrypt)
{
	if (wpa_auth->cb.send_eapol == NULL)
		return -1;
	return wpa_auth->cb.send_eapol(wpa_auth->cb.ctx, addr, data, data_len,
				       encrypt);
}


static inline int wpa_auth_for_each_sta(struct wpa_authenticator *wpa_auth,
					int (*cb)(struct wpa_state_machine *sm,
						  void *ctx),
					void *cb_ctx)
{
	if (wpa_auth->cb.for_each_sta == NULL)
		return 0;
	return wpa_auth->cb.for_each_sta(wpa_auth->cb.ctx, cb, cb_ctx);
}


static void wpa_auth_logger(struct wpa_authenticator *wpa_auth, const u8 *addr,
			    logger_level level, const char *txt)
{
	if (wpa_auth->cb.logger == NULL)
		return;
	wpa_auth->cb.logger(wpa_auth->cb.ctx, addr, level, txt);
}


static void wpa_auth_vlogger(struct wpa_authenticator *wpa_auth,
			     const u8 *addr, logger_level level,
			     const char *fmt, ...)
{
	char *format;
	int maxlen;
	va_list ap;

	if (wpa_auth->cb.logger == NULL)
		return;

	maxlen = strlen(fmt) + 100;
	format = malloc(maxlen);
	if (!format)
		return;

	va_start(ap, fmt);
	vsnprintf(format, maxlen, fmt, ap);
	va_end(ap);

	wpa_auth_logger(wpa_auth, addr, level, format);

	free(format);
}


static int wpa_write_wpa_ie(struct wpa_auth_config *conf, u8 *buf, size_t len)
{
	struct wpa_ie_hdr *hdr;
	int num_suites;
	u8 *pos, *count;

	hdr = (struct wpa_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_GENERIC;
	memcpy(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN);
	hdr->version = host_to_le16(WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	if (conf->wpa_group == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_WEP104) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_WEP40) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "Invalid group cipher (%d).",
			   conf->wpa_group);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (conf->wpa_pairwise & WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_pairwise & WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_pairwise & WPA_CIPHER_NONE) {
		memcpy(pos, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid pairwise cipher (%d).",
			   conf->wpa_pairwise);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid key management type (%d).",
			   conf->wpa_key_mgmt);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


static int wpa_write_rsn_ie(struct wpa_auth_config *conf, u8 *buf, size_t len)
{
	struct rsn_ie_hdr *hdr;
	int num_suites;
	u8 *pos, *count;
	u16 capab;

	hdr = (struct rsn_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_RSN;
	pos = (u8 *) &hdr->version;
	*pos++ = RSN_VERSION & 0xff;
	*pos++ = RSN_VERSION >> 8;
	pos = (u8 *) (hdr + 1);

	if (conf->wpa_group == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_WEP104) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN);
	} else if (conf->wpa_group == WPA_CIPHER_WEP40) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "Invalid group cipher (%d).",
			   conf->wpa_group);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (conf->wpa_pairwise & WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_pairwise & WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_pairwise & WPA_CIPHER_NONE) {
		memcpy(pos, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid pairwise cipher (%d).",
			   conf->wpa_pairwise);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid key management type (%d).",
			   conf->wpa_key_mgmt);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	/* RSN Capabilities */
	capab = 0;
	if (conf->rsn_preauth)
		capab |= WPA_CAPABILITY_PREAUTH;
	if (conf->peerkey)
		capab |= WPA_CAPABILITY_PEERKEY_ENABLED;
	if (conf->wme_enabled) {
		/* 4 PTKSA replay counters when using WME */
		capab |= (RSN_NUM_REPLAY_COUNTERS_16 << 2);
	}
#ifdef CONFIG_IEEE80211W
	if (conf->ieee80211w != WPA_NO_IEEE80211W)
		capab |= WPA_CAPABILITY_MGMT_FRAME_PROTECTION;
#endif /* CONFIG_IEEE80211W */
	*pos++ = capab & 0xff;
	*pos++ = capab >> 8;

#ifdef CONFIG_IEEE80211W
	if (conf->ieee80211w != WPA_NO_IEEE80211W) {
		if (pos + 2 + 4 > buf + len)
			return -1;
		/* PMKID Count */
		WPA_PUT_LE16(pos, 0);
		pos += 2;

		/* Management Group Cipher Suite */
		memcpy(pos, RSN_CIPHER_SUITE_AES_128_CMAC, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
	}
#endif /* CONFIG_IEEE80211W */

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


static int wpa_gen_wpa_ie(struct wpa_authenticator *wpa_auth)
{
	u8 *pos, buf[100];
	int res;

	pos = buf;

	if (wpa_auth->conf.wpa & HOSTAPD_WPA_VERSION_WPA2) {
		res = wpa_write_rsn_ie(&wpa_auth->conf,
				       pos, buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}
	if (wpa_auth->conf.wpa & HOSTAPD_WPA_VERSION_WPA) {
		res = wpa_write_wpa_ie(&wpa_auth->conf,
				       pos, buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}

	free(wpa_auth->wpa_ie);
	wpa_auth->wpa_ie = malloc(pos - buf);
	if (wpa_auth->wpa_ie == NULL)
		return -1;
	memcpy(wpa_auth->wpa_ie, buf, pos - buf);
	wpa_auth->wpa_ie_len = pos - buf;

	return 0;
}


static void wpa_sta_disconnect(struct wpa_authenticator *wpa_auth,
			       const u8 *addr)
{
	if (wpa_auth->cb.disconnect == NULL)
		return;
	wpa_auth->cb.disconnect(wpa_auth->cb.ctx, addr,
				WLAN_REASON_PREV_AUTH_NOT_VALID);
}


static void wpa_rekey_gmk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;

	if (hostapd_get_rand(wpa_auth->group->GMK, WPA_GMK_LEN)) {
		wpa_printf(MSG_ERROR, "Failed to get random data for WPA "
			   "initialization.");
	} else {
		wpa_auth_logger(wpa_auth, NULL, LOGGER_DEBUG, "GMK rekeyd");
	}

	if (wpa_auth->conf.wpa_gmk_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, wpa_auth, NULL);
	}
}


static void wpa_rekey_gtk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;
	struct wpa_group *group;

	wpa_auth_logger(wpa_auth, NULL, LOGGER_DEBUG, "rekeying GTK");
	for (group = wpa_auth->group; group; group = group->next) {
		group->GTKReKey = TRUE;
		do {
			group->changed = FALSE;
			wpa_group_sm_step(wpa_auth, group);
		} while (group->changed);
	}

	if (wpa_auth->conf.wpa_group_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_group_rekey,
				       0, wpa_rekey_gtk, wpa_auth, NULL);
	}
}


static int wpa_auth_pmksa_clear_cb(struct wpa_state_machine *sm, void *ctx)
{
	if (sm->pmksa == ctx)
		sm->pmksa = NULL;
	return 0;
}


static void wpa_auth_pmksa_free_cb(struct rsn_pmksa_cache_entry *entry,
				   void *ctx)
{
	struct wpa_authenticator *wpa_auth = ctx;
	wpa_auth_for_each_sta(wpa_auth, wpa_auth_pmksa_clear_cb, entry);
}


static struct wpa_group * wpa_group_init(struct wpa_authenticator *wpa_auth,
					 int vlan_id)
{
	struct wpa_group *group;
	u8 buf[ETH_ALEN + 8 + sizeof(group)];
	u8 rkey[32];

	group = wpa_zalloc(sizeof(struct wpa_group));
	if (group == NULL)
		return NULL;

	group->GTKAuthenticator = TRUE;
	group->vlan_id = vlan_id;

	switch (wpa_auth->conf.wpa_group) {
	case WPA_CIPHER_CCMP:
		group->GTK_len = 16;
		break;
	case WPA_CIPHER_TKIP:
		group->GTK_len = 32;
		break;
	case WPA_CIPHER_WEP104:
		group->GTK_len = 13;
		break;
	case WPA_CIPHER_WEP40:
		group->GTK_len = 5;
		break;
	}

	/* Counter = PRF-256(Random number, "Init Counter",
	 *                   Local MAC Address || Time)
	 */
	memcpy(buf, wpa_auth->addr, ETH_ALEN);
	wpa_get_ntp_timestamp(buf + ETH_ALEN);
	memcpy(buf + ETH_ALEN + 8, &group, sizeof(group));
	if (hostapd_get_rand(rkey, sizeof(rkey)) ||
	    hostapd_get_rand(group->GMK, WPA_GMK_LEN)) {
		wpa_printf(MSG_ERROR, "Failed to get random data for WPA "
			   "initialization.");
		free(group);
		return NULL;
	}

	sha1_prf(rkey, sizeof(rkey), "Init Counter", buf, sizeof(buf),
		 group->Counter, WPA_NONCE_LEN);

	group->GInit = TRUE;
	wpa_group_sm_step(wpa_auth, group);
	group->GInit = FALSE;
	wpa_group_sm_step(wpa_auth, group);

	return group;
}


/**
 * wpa_init - Initialize WPA authenticator
 * @addr: Authenticator address
 * @conf: Configuration for WPA authenticator
 * Returns: Pointer to WPA authenticator data or %NULL on failure
 */
struct wpa_authenticator * wpa_init(const u8 *addr,
				    struct wpa_auth_config *conf,
				    struct wpa_auth_callbacks *cb)
{
	struct wpa_authenticator *wpa_auth;

	wpa_auth = wpa_zalloc(sizeof(struct wpa_authenticator));
	if (wpa_auth == NULL)
		return NULL;
	memcpy(wpa_auth->addr, addr, ETH_ALEN);
	memcpy(&wpa_auth->conf, conf, sizeof(*conf));
	memcpy(&wpa_auth->cb, cb, sizeof(*cb));

	if (wpa_gen_wpa_ie(wpa_auth)) {
		wpa_printf(MSG_ERROR, "Could not generate WPA IE.");
		free(wpa_auth);
		return NULL;
	}

	wpa_auth->group = wpa_group_init(wpa_auth, 0);
	if (wpa_auth->group == NULL) {
		free(wpa_auth->wpa_ie);
		free(wpa_auth);
		return NULL;
	}

	wpa_auth->pmksa = pmksa_cache_init(wpa_auth_pmksa_free_cb, wpa_auth);
	if (wpa_auth->pmksa == NULL) {
		wpa_printf(MSG_ERROR, "PMKSA cache initialization failed.");
		free(wpa_auth->wpa_ie);
		free(wpa_auth);
		return NULL;
	}

	if (wpa_auth->conf.wpa_gmk_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, wpa_auth, NULL);
	}

	if (wpa_auth->conf.wpa_group_rekey) {
		eloop_register_timeout(wpa_auth->conf.wpa_group_rekey, 0,
				       wpa_rekey_gtk, wpa_auth, NULL);
	}

	return wpa_auth;
}


/**
 * wpa_deinit - Deinitialize WPA authenticator
 * @wpa_auth: Pointer to WPA authenticator data from wpa_init()
 */
void wpa_deinit(struct wpa_authenticator *wpa_auth)
{
	struct wpa_group *group, *prev;

	eloop_cancel_timeout(wpa_rekey_gmk, wpa_auth, NULL);
	eloop_cancel_timeout(wpa_rekey_gtk, wpa_auth, NULL);

	while (wpa_auth->stsl_negotiations)
		wpa_stsl_remove(wpa_auth, wpa_auth->stsl_negotiations);

	pmksa_cache_deinit(wpa_auth->pmksa);

	free(wpa_auth->wpa_ie);

	group = wpa_auth->group;
	while (group) {
		prev = group;
		group = group->next;
		free(prev);
	}

	free(wpa_auth);
}


/**
 * wpa_reconfig - Update WPA authenticator configuration
 * @wpa_auth: Pointer to WPA authenticator data from wpa_init()
 * @conf: Configuration for WPA authenticator
 */
int wpa_reconfig(struct wpa_authenticator *wpa_auth,
		 struct wpa_auth_config *conf)
{
	if (wpa_auth == NULL)
		return 0;

	memcpy(&wpa_auth->conf, conf, sizeof(*conf));
	/*
	 * TODO:
	 * Disassociate stations if configuration changed
	 * Update WPA/RSN IE
	 */
	return 0;
}


static int wpa_selector_to_bitfield(u8 *s)
{
	if (memcmp(s, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40;
	if (memcmp(s, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP;
	if (memcmp(s, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP;
	if (memcmp(s, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104;
	return 0;
}


static int wpa_key_mgmt_to_bitfield(u8 *s)
{
	if (memcmp(s, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X;
	if (memcmp(s, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X, WPA_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK;
	return 0;
}


static int rsn_selector_to_bitfield(u8 *s)
{
	if (memcmp(s, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_NONE;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP40;
	if (memcmp(s, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_TKIP;
	if (memcmp(s, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_CCMP;
	if (memcmp(s, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_WEP104;
#ifdef CONFIG_IEEE80211W
	if (memcmp(s, RSN_CIPHER_SUITE_AES_128_CMAC, RSN_SELECTOR_LEN) == 0)
		return WPA_CIPHER_AES_128_CMAC;
#endif /* CONFIG_IEEE80211W */
	return 0;
}


static int rsn_key_mgmt_to_bitfield(u8 *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK;
	return 0;
}


static u8 * wpa_add_kde(u8 *pos, const u8 *kde, const u8 *data,
			size_t data_len, const u8 *data2, size_t data2_len)
{
	*pos++ = GENERIC_INFO_ELEM;
	*pos++ = RSN_SELECTOR_LEN + data_len + data2_len;
	memcpy(pos, kde, RSN_SELECTOR_LEN);
	pos += RSN_SELECTOR_LEN;
	memcpy(pos, data, data_len);
	pos += data_len;
	if (data2) {
		memcpy(pos, data2, data2_len);
		pos += data2_len;
	}
	return pos;
}


struct wpa_ie_data {
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int capabilities;
	size_t num_pmkid;
	u8 *pmkid;
	int mgmt_group_cipher;
};


static int wpa_parse_wpa_ie_wpa(const u8 *wpa_ie, size_t wpa_ie_len,
				struct wpa_ie_data *data)
{
	struct wpa_ie_hdr *hdr;
	u8 *pos;
	int left;
	int i, count;

	memset(data, 0, sizeof(*data));
	data->pairwise_cipher = WPA_CIPHER_TKIP;
	data->group_cipher = WPA_CIPHER_TKIP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	data->mgmt_group_cipher = 0;

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr))
		return -1;

	hdr = (struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != WLAN_EID_GENERIC ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN) != 0 ||
	    le_to_host16(hdr->version) != WPA_VERSION) {
		return -2;
	}

	pos = (u8 *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0)
		  return -3;

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN)
			return -4;
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1)
		return -5;

	if (left >= 2) {
		data->key_mgmt = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN)
			return -6;
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1)
		return -7;

	if (left >= 2) {
		data->capabilities = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
		return -8;
	}

	return 0;
}


static int wpa_parse_wpa_ie_rsn(const u8 *rsn_ie, size_t rsn_ie_len,
				struct wpa_ie_data *data)
{
	struct rsn_ie_hdr *hdr;
	u8 *pos;
	int left;
	int i, count;

	memset(data, 0, sizeof(*data));
	data->pairwise_cipher = WPA_CIPHER_CCMP;
	data->group_cipher = WPA_CIPHER_CCMP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
#ifdef CONFIG_IEEE80211W
	data->mgmt_group_cipher = WPA_CIPHER_AES_128_CMAC;
#else /* CONFIG_IEEE80211W */
	data->mgmt_group_cipher = 0;
#endif /* CONFIG_IEEE80211W */

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr))
		return -1;

	hdr = (struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != WLAN_EID_RSN ||
	    hdr->len != rsn_ie_len - 2 ||
	    le_to_host16(hdr->version) != RSN_VERSION) {
		return -2;
	}

	pos = (u8 *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0)
		  return -3;

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN)
			return -4;
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1)
		return -5;

	if (left >= 2) {
		data->key_mgmt = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN)
			return -6;
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1)
		return -7;

	if (left >= 2) {
		data->capabilities = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (left < (int) data->num_pmkid * PMKID_LEN) {
			wpa_printf(MSG_DEBUG, "RSN: too short RSN IE for "
				   "PMKIDs (num=%lu, left=%d)",
				   (unsigned long) data->num_pmkid, left);
			return -9;
		}
		data->pmkid = pos;
		pos += data->num_pmkid * PMKID_LEN;
		left -= data->num_pmkid * PMKID_LEN;
	}

#ifdef CONFIG_IEEE80211W
	if (left >= 4) {
		data->mgmt_group_cipher = rsn_selector_to_bitfield(pos);
		if (data->mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC) {
			wpa_printf(MSG_DEBUG, "RSN: Unsupported management "
				   "group cipher 0x%x",
				   data->mgmt_group_cipher);
			return -10;
		}
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	}
#endif /* CONFIG_IEEE80211W */

	if (left > 0) {
		return -8;
	}

	return 0;
}


int wpa_validate_wpa_ie(struct wpa_authenticator *wpa_auth,
			struct wpa_state_machine *sm,
			const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct wpa_ie_data data;
	int ciphers, key_mgmt, res, version;
	const u8 *selector;
	size_t i;

	if (wpa_auth == NULL || sm == NULL)
		return WPA_NOT_ENABLED;

	if (wpa_ie == NULL || wpa_ie_len < 1)
		return WPA_INVALID_IE;

	if (wpa_ie[0] == WLAN_EID_RSN)
		version = HOSTAPD_WPA_VERSION_WPA2;
	else
		version = HOSTAPD_WPA_VERSION_WPA;

	if (version == HOSTAPD_WPA_VERSION_WPA2) {
		res = wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, &data);

		selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
		memcpy(wpa_auth->dot11RSNAAuthenticationSuiteSelected,
		       selector, RSN_SELECTOR_LEN);

		selector = RSN_CIPHER_SUITE_CCMP;
		if (data.pairwise_cipher & WPA_CIPHER_CCMP)
			selector = RSN_CIPHER_SUITE_CCMP;
		else if (data.pairwise_cipher & WPA_CIPHER_TKIP)
			selector = RSN_CIPHER_SUITE_TKIP;
		else if (data.pairwise_cipher & WPA_CIPHER_WEP104)
			selector = RSN_CIPHER_SUITE_WEP104;
		else if (data.pairwise_cipher & WPA_CIPHER_WEP40)
			selector = RSN_CIPHER_SUITE_WEP40;
		else if (data.pairwise_cipher & WPA_CIPHER_NONE)
			selector = RSN_CIPHER_SUITE_NONE;
		memcpy(wpa_auth->dot11RSNAPairwiseCipherSelected,
		       selector, RSN_SELECTOR_LEN);

		selector = RSN_CIPHER_SUITE_CCMP;
		if (data.group_cipher & WPA_CIPHER_CCMP)
			selector = RSN_CIPHER_SUITE_CCMP;
		else if (data.group_cipher & WPA_CIPHER_TKIP)
			selector = RSN_CIPHER_SUITE_TKIP;
		else if (data.group_cipher & WPA_CIPHER_WEP104)
			selector = RSN_CIPHER_SUITE_WEP104;
		else if (data.group_cipher & WPA_CIPHER_WEP40)
			selector = RSN_CIPHER_SUITE_WEP40;
		else if (data.group_cipher & WPA_CIPHER_NONE)
			selector = RSN_CIPHER_SUITE_NONE;
		memcpy(wpa_auth->dot11RSNAGroupCipherSelected,
		       selector, RSN_SELECTOR_LEN);
	} else {
		res = wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, &data);

		selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X;
		memcpy(wpa_auth->dot11RSNAAuthenticationSuiteSelected,
		       selector, WPA_SELECTOR_LEN);

		selector = WPA_CIPHER_SUITE_TKIP;
		if (data.pairwise_cipher & WPA_CIPHER_CCMP)
			selector = WPA_CIPHER_SUITE_CCMP;
		else if (data.pairwise_cipher & WPA_CIPHER_TKIP)
			selector = WPA_CIPHER_SUITE_TKIP;
		else if (data.pairwise_cipher & WPA_CIPHER_WEP104)
			selector = WPA_CIPHER_SUITE_WEP104;
		else if (data.pairwise_cipher & WPA_CIPHER_WEP40)
			selector = WPA_CIPHER_SUITE_WEP40;
		else if (data.pairwise_cipher & WPA_CIPHER_NONE)
			selector = WPA_CIPHER_SUITE_NONE;
		memcpy(wpa_auth->dot11RSNAPairwiseCipherSelected,
		       selector, WPA_SELECTOR_LEN);

		selector = WPA_CIPHER_SUITE_TKIP;
		if (data.group_cipher & WPA_CIPHER_CCMP)
			selector = WPA_CIPHER_SUITE_CCMP;
		else if (data.group_cipher & WPA_CIPHER_TKIP)
			selector = WPA_CIPHER_SUITE_TKIP;
		else if (data.group_cipher & WPA_CIPHER_WEP104)
			selector = WPA_CIPHER_SUITE_WEP104;
		else if (data.group_cipher & WPA_CIPHER_WEP40)
			selector = WPA_CIPHER_SUITE_WEP40;
		else if (data.group_cipher & WPA_CIPHER_NONE)
			selector = WPA_CIPHER_SUITE_NONE;
		memcpy(wpa_auth->dot11RSNAGroupCipherSelected,
		       selector, WPA_SELECTOR_LEN);
	}
	if (res) {
		wpa_printf(MSG_DEBUG, "Failed to parse WPA/RSN IE from "
			   MACSTR " (res=%d)", MAC2STR(sm->addr), res);
		wpa_hexdump(MSG_DEBUG, "WPA/RSN IE", wpa_ie, wpa_ie_len);
		return WPA_INVALID_IE;
	}

	if (data.group_cipher != wpa_auth->conf.wpa_group) {
		wpa_printf(MSG_DEBUG, "Invalid WPA group cipher (0x%x) from "
			   MACSTR, data.group_cipher, MAC2STR(sm->addr));
		return WPA_INVALID_GROUP;
	}

	key_mgmt = data.key_mgmt & wpa_auth->conf.wpa_key_mgmt;
	if (!key_mgmt) {
		wpa_printf(MSG_DEBUG, "Invalid WPA key mgmt (0x%x) from "
			   MACSTR, data.key_mgmt, MAC2STR(sm->addr));
		return WPA_INVALID_AKMP;
	}
	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	else
		sm->wpa_key_mgmt = WPA_KEY_MGMT_PSK;

	ciphers = data.pairwise_cipher & wpa_auth->conf.wpa_pairwise;
	if (!ciphers) {
		wpa_printf(MSG_DEBUG, "Invalid WPA pairwise cipher (0x%x) "
			   "from " MACSTR,
			   data.pairwise_cipher, MAC2STR(sm->addr));
		return WPA_INVALID_PAIRWISE;
	}

#ifdef CONFIG_IEEE80211W
	if (wpa_auth->conf.ieee80211w == WPA_IEEE80211W_REQUIRED) {
		if (!(data.capabilities &
		      WPA_CAPABILITY_MGMT_FRAME_PROTECTION)) {
			wpa_printf(MSG_DEBUG, "Management frame protection "
				   "required, but client did not enable it");
			return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
		}

		if (ciphers & WPA_CIPHER_TKIP) {
			wpa_printf(MSG_DEBUG, "Management frame protection "
				   "cannot use TKIP");
			return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
		}

		if (data.mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC) {
			wpa_printf(MSG_DEBUG, "Unsupported management group "
				   "cipher %d", data.mgmt_group_cipher);
			return WPA_INVALID_MGMT_GROUP_CIPHER;
		}
	}

	if (wpa_auth->conf.ieee80211w == WPA_NO_IEEE80211W ||
	    !(data.capabilities & WPA_CAPABILITY_MGMT_FRAME_PROTECTION))
		sm->mgmt_frame_prot = 0;
	else
		sm->mgmt_frame_prot = 1;
#endif /* CONFIG_IEEE80211W */

	if (ciphers & WPA_CIPHER_CCMP)
		sm->pairwise = WPA_CIPHER_CCMP;
	else
		sm->pairwise = WPA_CIPHER_TKIP;

	/* TODO: clear WPA/WPA2 state if STA changes from one to another */
	if (wpa_ie[0] == WLAN_EID_RSN)
		sm->wpa = WPA_VERSION_WPA2;
	else
		sm->wpa = WPA_VERSION_WPA;

	for (i = 0; i < data.num_pmkid; i++) {
		wpa_hexdump(MSG_DEBUG, "RSN IE: STA PMKID",
			    &data.pmkid[i * PMKID_LEN], PMKID_LEN);
		sm->pmksa = pmksa_cache_get(wpa_auth->pmksa, sm->addr,
					    &data.pmkid[i * PMKID_LEN]);
		if (sm->pmksa) {
			wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
					 "PMKID found from PMKSA cache "
					 "eap_type=%d vlan_id=%d",
					 sm->pmksa->eap_type_authsrv,
					 sm->pmksa->vlan_id);
			memcpy(wpa_auth->dot11RSNAPMKIDUsed,
			       sm->pmksa->pmkid, PMKID_LEN);
			break;
		}
	}

	if (sm->wpa_ie == NULL || sm->wpa_ie_len < wpa_ie_len) {
		free(sm->wpa_ie);
		sm->wpa_ie = malloc(wpa_ie_len);
		if (sm->wpa_ie == NULL)
			return WPA_ALLOC_FAIL;
	}
	memcpy(sm->wpa_ie, wpa_ie, wpa_ie_len);
	sm->wpa_ie_len = wpa_ie_len;

	return WPA_IE_OK;
}


struct wpa_eapol_ie_parse {
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	const u8 *rsn_ie;
	size_t rsn_ie_len;
	const u8 *pmkid;
	const u8 *gtk;
	size_t gtk_len;
	const u8 *mac_addr;
	size_t mac_addr_len;
#ifdef CONFIG_PEERKEY
	const u8 *smk;
	size_t smk_len;
	const u8 *nonce;
	size_t nonce_len;
	const u8 *lifetime;
	size_t lifetime_len;
	const u8 *error;
	size_t error_len;
#endif /* CONFIG_PEERKEY */
};


/**
 * wpa_parse_generic - Parse EAPOL-Key Key Data Generic IEs
 * @pos: Pointer to the IE header
 * @end: Pointer to the end of the Key Data buffer
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, 1 if end mark is found, -1 on failure
 */
static int wpa_parse_generic(const u8 *pos, const u8 *end,
			     struct wpa_eapol_ie_parse *ie)
{
	if (pos[1] == 0)
		return 1;

	if (pos[1] >= 6 &&
	    memcmp(pos + 2, WPA_OUI_TYPE, WPA_SELECTOR_LEN) == 0 &&
	    pos[2 + WPA_SELECTOR_LEN] == 1 &&
	    pos[2 + WPA_SELECTOR_LEN + 1] == 0) {
		ie->wpa_ie = pos;
		ie->wpa_ie_len = pos[1] + 2;
		return 0;
	}

	if (pos + 1 + RSN_SELECTOR_LEN < end &&
	    pos[1] >= RSN_SELECTOR_LEN + PMKID_LEN &&
	    memcmp(pos + 2, RSN_KEY_DATA_PMKID, RSN_SELECTOR_LEN) == 0) {
		ie->pmkid = pos + 2 + RSN_SELECTOR_LEN;
		return 0;
	}

	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_GROUPKEY, RSN_SELECTOR_LEN) == 0) {
		ie->gtk = pos + 2 + RSN_SELECTOR_LEN;
		ie->gtk_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}

	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_MAC_ADDR, RSN_SELECTOR_LEN) == 0) {
		ie->mac_addr = pos + 2 + RSN_SELECTOR_LEN;
		ie->mac_addr_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}

#ifdef CONFIG_PEERKEY
	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_SMK, RSN_SELECTOR_LEN) == 0) {
		ie->smk = pos + 2 + RSN_SELECTOR_LEN;
		ie->smk_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}

	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_NONCE, RSN_SELECTOR_LEN) == 0) {
		ie->nonce = pos + 2 + RSN_SELECTOR_LEN;
		ie->nonce_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}

	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_LIFETIME, RSN_SELECTOR_LEN) == 0) {
		ie->lifetime = pos + 2 + RSN_SELECTOR_LEN;
		ie->lifetime_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}

	if (pos[1] > RSN_SELECTOR_LEN + 2 &&
	    memcmp(pos + 2, RSN_KEY_DATA_ERROR, RSN_SELECTOR_LEN) == 0) {
		ie->error = pos + 2 + RSN_SELECTOR_LEN;
		ie->error_len = pos[1] - RSN_SELECTOR_LEN;
		return 0;
	}
#endif /* CONFIG_PEERKEY */

	return 0;
}


/**
 * wpa_parse_kde_ies - Parse EAPOL-Key Key Data IEs
 * @buf: Pointer to the Key Data buffer
 * @len: Key Data Length
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, -1 on failure
 */
static int wpa_parse_kde_ies(const u8 *buf, size_t len,
			     struct wpa_eapol_ie_parse *ie)
{
	const u8 *pos, *end;
	int ret = 0;

	memset(ie, 0, sizeof(*ie));
	for (pos = buf, end = pos + len; pos + 1 < end; pos += 2 + pos[1]) {
		if (pos[0] == 0xdd &&
		    ((pos == buf + len - 1) || pos[1] == 0)) {
			/* Ignore padding */
			break;
		}
		if (pos + 2 + pos[1] > end) {
			wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key Key Data "
				   "underflow (ie=%d len=%d)", pos[0], pos[1]);
			ret = -1;
			break;
		}
		if (*pos == RSN_INFO_ELEM) {
			ie->rsn_ie = pos;
			ie->rsn_ie_len = pos[1] + 2;
		} else if (*pos == GENERIC_INFO_ELEM) {
			ret = wpa_parse_generic(pos, end, ie);
			if (ret < 0)
				break;
			if (ret > 0) {
				ret = 0;
				break;
			}
		} else {
			wpa_hexdump(MSG_DEBUG, "WPA: Unrecognized EAPOL-Key "
				    "Key Data IE", pos, 2 + pos[1]);
		}
	}

	return ret;
}


struct wpa_state_machine *
wpa_auth_sta_init(struct wpa_authenticator *wpa_auth, const u8 *addr)
{
	struct wpa_state_machine *sm;

	sm = wpa_zalloc(sizeof(struct wpa_state_machine));
	if (sm == NULL)
		return NULL;
	memcpy(sm->addr, addr, ETH_ALEN);

	sm->wpa_auth = wpa_auth;
	sm->group = wpa_auth->group;

	return sm;
}


void wpa_auth_sta_associated(struct wpa_authenticator *wpa_auth,
			     struct wpa_state_machine *sm)
{
	if (wpa_auth == NULL || !wpa_auth->conf.wpa || sm == NULL)
		return;

	if (sm->started) {
		memset(sm->key_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
		sm->ReAuthenticationRequest = TRUE;
		wpa_sm_step(sm);
		return;
	}

	wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG,
			"start authentication");
	sm->started = 1;

	sm->Init = TRUE;
	wpa_sm_step(sm);
	sm->Init = FALSE;
	sm->AuthenticationRequest = TRUE;
	wpa_sm_step(sm);
}


static void wpa_free_sta_sm(struct wpa_state_machine *sm)
{
	free(sm->last_rx_eapol_key);
	free(sm->wpa_ie);
	free(sm);
}


void wpa_auth_sta_deinit(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return;

	if (sm->wpa_auth->conf.wpa_strict_rekey && sm->has_GTK) {
		wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
				"strict rekeying - force GTK rekey since STA "
				"is leaving");
		eloop_cancel_timeout(wpa_rekey_gtk, sm->wpa_auth, NULL);
		eloop_register_timeout(0, 500000, wpa_rekey_gtk, sm->wpa_auth,
				       NULL);
	}

	eloop_cancel_timeout(wpa_send_eapol_timeout, sm->wpa_auth, sm);
	eloop_cancel_timeout(wpa_sm_call_step, sm, NULL);
	if (sm->in_step_loop) {
		/* Must not free state machine while wpa_sm_step() is running.
		 * Freeing will be completed in the end of wpa_sm_step(). */
		wpa_printf(MSG_DEBUG, "WPA: Registering pending STA state "
			   "machine deinit for " MACSTR, MAC2STR(sm->addr));
		sm->pending_deinit = 1;
	} else
		wpa_free_sta_sm(sm);
}


static void wpa_request_new_ptk(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return;

	sm->PTKRequest = TRUE;
	sm->PTK_valid = 0;
}


#ifdef CONFIG_PEERKEY
static void wpa_stsl_step(void *eloop_ctx, void *timeout_ctx)
{
#if 0
	struct wpa_authenticator *wpa_auth = eloop_ctx;
	struct wpa_stsl_negotiation *neg = timeout_ctx;
#endif

	/* TODO: ? */
}


struct wpa_stsl_search {
	const u8 *addr;
	struct wpa_state_machine *sm;
};


static int wpa_stsl_select_sta(struct wpa_state_machine *sm, void *ctx)
{
	struct wpa_stsl_search *search = ctx;
	if (memcmp(search->addr, sm->addr, ETH_ALEN) == 0) {
		search->sm = sm;
		return 1;
	}
	return 0;
}


static void wpa_smk_send_error(struct wpa_authenticator *wpa_auth,
			       struct wpa_state_machine *sm, const u8 *peer,
			       u16 mui, u16 error_type)
{
	u8 kde[2 + RSN_SELECTOR_LEN + ETH_ALEN +
	       2 + RSN_SELECTOR_LEN + sizeof(struct rsn_error_kde)];
	size_t kde_len;
	u8 *pos;
	struct rsn_error_kde error;

	wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG,
			"Sending SMK Error");

	kde_len = 2 + RSN_SELECTOR_LEN + sizeof(struct rsn_error_kde);
	pos = kde;

	if (peer) {
		pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, peer, ETH_ALEN,
				  NULL, 0);
		kde_len += 2 + RSN_SELECTOR_LEN + ETH_ALEN;
	}

	error.mui = host_to_be16(mui);
	error.error_type = host_to_be16(error_type);
	pos = wpa_add_kde(pos, RSN_KEY_DATA_ERROR,
			  (u8 *) &error, sizeof(error), NULL, 0);

	__wpa_send_eapol(wpa_auth, sm,
			 WPA_KEY_INFO_SECURE | WPA_KEY_INFO_MIC |
			 WPA_KEY_INFO_SMK_MESSAGE | WPA_KEY_INFO_ERROR,
			 NULL, NULL, kde, kde_len, 0, 0, 0);
}


static void wpa_smk_m1(struct wpa_authenticator *wpa_auth,
		       struct wpa_state_machine *sm,
		       struct wpa_eapol_key *key)
{
	struct wpa_eapol_ie_parse kde;
	struct wpa_stsl_search search;
	u8 *buf, *pos;
	size_t buf_len;

	if (wpa_parse_kde_ies((const u8 *) (key + 1),
			      ntohs(key->key_data_length), &kde) < 0) {
		wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK M1");
		return;
	}

	if (kde.rsn_ie == NULL || kde.mac_addr == NULL ||
	    kde.mac_addr_len < ETH_ALEN) {
		wpa_printf(MSG_INFO, "RSN: No RSN IE or MAC address KDE in "
			   "SMK M1");
		return;
	}

	/* Initiator = sm->addr; Peer = kde.mac_addr */

	search.addr = kde.mac_addr;
	search.sm = NULL;
	if (wpa_auth_for_each_sta(wpa_auth, wpa_stsl_select_sta, &search) ==
	    0 || search.sm == NULL) {
		wpa_printf(MSG_DEBUG, "RSN: SMK handshake with " MACSTR
			   " aborted - STA not associated anymore",
			   MAC2STR(kde.mac_addr));
		wpa_smk_send_error(wpa_auth, sm, kde.mac_addr, STK_MUI_SMK,
				   STK_ERR_STA_NR);
		/* FIX: wpa_stsl_remove(wpa_auth, neg); */
		return;
	}

	buf_len = kde.rsn_ie_len + 2 + RSN_SELECTOR_LEN + ETH_ALEN;
	buf = malloc(buf_len);
	if (buf == NULL)
		return;
	/* Initiator RSN IE */
	memcpy(buf, kde.rsn_ie, kde.rsn_ie_len);
	pos = buf + kde.rsn_ie_len;
	/* Initiator MAC Address */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, sm->addr, ETH_ALEN,
			  NULL, 0);

	/* SMK M2:
	 * EAPOL-Key(S=1, M=1, A=1, I=0, K=0, SM=1, KeyRSC=0, Nonce=INonce,
	 *           MIC=MIC, DataKDs=(RSNIE_I, MAC_I KDE)
	 */

	wpa_auth_logger(wpa_auth, search.sm->addr, LOGGER_DEBUG,
			"Sending SMK M2");

	__wpa_send_eapol(wpa_auth, search.sm,
			 WPA_KEY_INFO_SECURE | WPA_KEY_INFO_MIC |
			 WPA_KEY_INFO_ACK | WPA_KEY_INFO_SMK_MESSAGE,
			 NULL, key->key_nonce, buf, buf_len, 0, 0, 0);

	free(buf);

}


static void wpa_send_smk_m4(struct wpa_authenticator *wpa_auth,
			    struct wpa_state_machine *sm,
			    struct wpa_eapol_key *key,
			    struct wpa_eapol_ie_parse *kde,
			    const u8 *smk)
{
	u8 *buf, *pos;
	size_t buf_len;
	u32 lifetime;

	/* SMK M4:
	 * EAPOL-Key(S=1, M=1, A=0, I=1, K=0, SM=1, KeyRSC=0, Nonce=PNonce,
	 *           MIC=MIC, DataKDs=(MAC_I KDE, INonce KDE, SMK KDE,
	 *           Lifetime KDE)
	 */

	buf_len = 2 + RSN_SELECTOR_LEN + ETH_ALEN +
		2 + RSN_SELECTOR_LEN + WPA_NONCE_LEN +
		2 + RSN_SELECTOR_LEN + WPA_PMK_LEN + WPA_NONCE_LEN +
		2 + RSN_SELECTOR_LEN + sizeof(lifetime);
	pos = buf = malloc(buf_len);
	if (buf == NULL)
		return;

	/* Initiator MAC Address */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, kde->mac_addr, ETH_ALEN,
			  NULL, 0);

	/* Initiator Nonce */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_NONCE, kde->nonce, WPA_NONCE_LEN,
			  NULL, 0);

	/* SMK with PNonce */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_SMK, smk, WPA_PMK_LEN,
			  key->key_nonce, WPA_NONCE_LEN);

	/* Lifetime */
	lifetime = htonl(43200); /* dot11RSNAConfigSMKLifetime */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_LIFETIME,
			  (u8 *) &lifetime, sizeof(lifetime), NULL, 0);

	wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			"Sending SMK M4");

	__wpa_send_eapol(wpa_auth, sm,
			 WPA_KEY_INFO_SECURE | WPA_KEY_INFO_MIC |
			 WPA_KEY_INFO_INSTALL | WPA_KEY_INFO_SMK_MESSAGE,
			 NULL, key->key_nonce, buf, buf_len, 0, 1, 0);

	free(buf);
}


static void wpa_send_smk_m5(struct wpa_authenticator *wpa_auth,
			    struct wpa_state_machine *sm,
			    struct wpa_eapol_key *key,
			    struct wpa_eapol_ie_parse *kde,
			    const u8 *smk, const u8 *peer)
{
	u8 *buf, *pos;
	size_t buf_len;
	u32 lifetime;

	/* SMK M5:
	 * EAPOL-Key(S=1, M=1, A=0, I=0, K=0, SM=1, KeyRSC=0, Nonce=INonce,
	 *           MIC=MIC, DataKDs=(RSNIE_P, MAC_P KDE, PNonce, SMK KDE,
	 *                             Lifetime KDE))
	 */

	buf_len = kde->rsn_ie_len +
		2 + RSN_SELECTOR_LEN + ETH_ALEN +
		2 + RSN_SELECTOR_LEN + WPA_NONCE_LEN +
		2 + RSN_SELECTOR_LEN + WPA_PMK_LEN + WPA_NONCE_LEN +
		2 + RSN_SELECTOR_LEN + sizeof(lifetime);
	pos = buf = malloc(buf_len);
	if (buf == NULL)
		return;

	/* Peer RSN IE */
	memcpy(buf, kde->rsn_ie, kde->rsn_ie_len);
	pos = buf + kde->rsn_ie_len;

	/* Peer MAC Address */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, peer, ETH_ALEN, NULL, 0);

	/* PNonce */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_NONCE, key->key_nonce,
			  WPA_NONCE_LEN, NULL, 0);

	/* SMK and INonce */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_SMK, smk, WPA_PMK_LEN,
			  kde->nonce, WPA_NONCE_LEN);

	/* Lifetime */
	lifetime = htonl(43200); /* dot11RSNAConfigSMKLifetime */
	pos = wpa_add_kde(pos, RSN_KEY_DATA_LIFETIME,
			  (u8 *) &lifetime, sizeof(lifetime), NULL, 0);

	wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			"Sending SMK M5");

	__wpa_send_eapol(wpa_auth, sm,
			 WPA_KEY_INFO_SECURE | WPA_KEY_INFO_MIC |
			 WPA_KEY_INFO_SMK_MESSAGE,
			 NULL, kde->nonce, buf, buf_len, 0, 1, 0);

	free(buf);
}


static void wpa_smk_m3(struct wpa_authenticator *wpa_auth,
		       struct wpa_state_machine *sm,
		       struct wpa_eapol_key *key)
{
	struct wpa_eapol_ie_parse kde;
	struct wpa_stsl_search search;
	u8 smk[32], buf[ETH_ALEN + 8 + 2 * WPA_NONCE_LEN], *pos;

	if (wpa_parse_kde_ies((const u8 *) (key + 1),
			      ntohs(key->key_data_length), &kde) < 0) {
		wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK M3");
		return;
	}

	if (kde.rsn_ie == NULL ||
	    kde.mac_addr == NULL || kde.mac_addr_len < ETH_ALEN ||
	    kde.nonce == NULL || kde.nonce_len < WPA_NONCE_LEN) {
		wpa_printf(MSG_INFO, "RSN: No RSN IE, MAC address KDE, or "
			   "Nonce KDE in SMK M3");
		return;
	}

	/* Peer = sm->addr; Initiator = kde.mac_addr;
	 * Peer Nonce = key->key_nonce; Initiator Nonce = kde.nonce */

	search.addr = kde.mac_addr;
	search.sm = NULL;
	if (wpa_auth_for_each_sta(wpa_auth, wpa_stsl_select_sta, &search) ==
	    0 || search.sm == NULL) {
		wpa_printf(MSG_DEBUG, "RSN: SMK handshake with " MACSTR
			   " aborted - STA not associated anymore",
			   MAC2STR(kde.mac_addr));
		wpa_smk_send_error(wpa_auth, sm, kde.mac_addr, STK_MUI_SMK,
				   STK_ERR_STA_NR);
		/* FIX: wpa_stsl_remove(wpa_auth, neg); */
		return;
	}

	if (hostapd_get_rand(smk, WPA_PMK_LEN)) {
		wpa_printf(MSG_DEBUG, "RSN: Failed to generate SMK");
		return;
	}

	/* SMK = PRF-256(Random number, "SMK Derivation",
	 *               AA || Time || INonce || PNonce)
	 */
	memcpy(buf, wpa_auth->addr, ETH_ALEN);
	pos = buf + ETH_ALEN;
	wpa_get_ntp_timestamp(pos);
	pos += 8;
	memcpy(pos, kde.nonce, WPA_NONCE_LEN);
	pos += WPA_NONCE_LEN;
	memcpy(pos, key->key_nonce, WPA_NONCE_LEN);
	sha1_prf(smk, WPA_PMK_LEN, "SMK Derivation", buf, sizeof(buf),
		 smk, WPA_PMK_LEN);

	wpa_hexdump_key(MSG_DEBUG, "RSN: SMK", smk, WPA_PMK_LEN);

	wpa_send_smk_m4(wpa_auth, sm, key, &kde, smk);
	wpa_send_smk_m5(wpa_auth, search.sm, key, &kde, smk, sm->addr);

	/* Authenticator does not need SMK anymore and it is required to forget
	 * it. */
	memset(smk, 0, sizeof(*smk));
}


static void wpa_smk_error(struct wpa_authenticator *wpa_auth,
			  struct wpa_state_machine *sm,
			  struct wpa_eapol_key *key)
{
	struct wpa_eapol_ie_parse kde;
	struct wpa_stsl_search search;
	struct rsn_error_kde error;
	u16 mui, error_type;

	if (wpa_parse_kde_ies((const u8 *) (key + 1),
			      ntohs(key->key_data_length), &kde) < 0) {
		wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK Error");
		return;
	}

	if (kde.mac_addr == NULL || kde.mac_addr_len < ETH_ALEN ||
	    kde.error == NULL || kde.error_len < sizeof(error)) {
		wpa_printf(MSG_INFO, "RSN: No MAC address or Error KDE in "
			   "SMK Error");
		return;
	}

	search.addr = kde.mac_addr;
	search.sm = NULL;
	if (wpa_auth_for_each_sta(wpa_auth, wpa_stsl_select_sta, &search) ==
	    0 || search.sm == NULL) {
		wpa_printf(MSG_DEBUG, "RSN: Peer STA " MACSTR " not "
			   "associated for SMK Error message from " MACSTR,
			   MAC2STR(kde.mac_addr), MAC2STR(sm->addr));
		return;
	}

	memcpy(&error, kde.error, sizeof(error));
	mui = be_to_host16(error.mui);
	error_type = be_to_host16(error.error_type);
	wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_INFO,
			 "STA reported SMK Error: Peer " MACSTR
			 " MUI %d Error Type %d",
			 MAC2STR(kde.mac_addr), mui, error_type);

	wpa_smk_send_error(wpa_auth, search.sm, sm->addr, mui, error_type);
}
#endif /* CONFIG_PEERKEY */


static int wpa_stsl_remove(struct wpa_authenticator *wpa_auth,
			   struct wpa_stsl_negotiation *neg)
{
#ifdef CONFIG_PEERKEY
	struct wpa_stsl_negotiation *pos, *prev;

	if (wpa_auth == NULL)
		return -1;
	pos = wpa_auth->stsl_negotiations;
	prev = NULL;
	while (pos) {
		if (pos == neg) {
			if (prev)
				prev->next = pos->next;
			else
				wpa_auth->stsl_negotiations = pos->next;

			eloop_cancel_timeout(wpa_stsl_step, wpa_auth, pos);
			free(pos);
			return 0;
		}
		prev = pos;
		pos = pos->next;
	}
#endif /* CONFIG_PEERKEY */

	return -1;
}


void wpa_receive(struct wpa_authenticator *wpa_auth,
		 struct wpa_state_machine *sm,
		 u8 *data, size_t data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info, key_data_length;
	enum { PAIRWISE_2, PAIRWISE_4, GROUP_2, REQUEST,
	       SMK_M1, SMK_M3, SMK_ERROR } msg;
	char *msgtxt;
	struct wpa_eapol_ie_parse kde;

	if (wpa_auth == NULL || !wpa_auth->conf.wpa || sm == NULL)
		return;

	if (data_len < sizeof(*hdr) + sizeof(*key))
		return;

	hdr = (struct ieee802_1x_hdr *) data;
	key = (struct wpa_eapol_key *) (hdr + 1);
	key_info = ntohs(key->key_info);
	key_data_length = ntohs(key->key_data_length);
	if (key_data_length > data_len - sizeof(*hdr) - sizeof(*key)) {
		wpa_printf(MSG_INFO, "WPA: Invalid EAPOL-Key frame - "
			   "key_data overflow (%d > %lu)",
			   key_data_length,
			   (unsigned long) (data_len - sizeof(*hdr) -
					    sizeof(*key)));
		return;
	}

	/* FIX: verify that the EAPOL-Key frame was encrypted if pairwise keys
	 * are set */

	if ((key_info & (WPA_KEY_INFO_SMK_MESSAGE | WPA_KEY_INFO_REQUEST)) ==
	    (WPA_KEY_INFO_SMK_MESSAGE | WPA_KEY_INFO_REQUEST)) {
		if (key_info & WPA_KEY_INFO_ERROR) {
			msg = SMK_ERROR;
			msgtxt = "SMK Error";
		} else {
			msg = SMK_M1;
			msgtxt = "SMK M1";
		}
	} else if (key_info & WPA_KEY_INFO_SMK_MESSAGE) {
		msg = SMK_M3;
		msgtxt = "SMK M3";
	} else if (key_info & WPA_KEY_INFO_REQUEST) {
		msg = REQUEST;
		msgtxt = "Request";
	} else if (!(key_info & WPA_KEY_INFO_KEY_TYPE)) {
		msg = GROUP_2;
		msgtxt = "2/2 Group";
	} else if (key_data_length == 0) {
		msg = PAIRWISE_4;
		msgtxt = "4/4 Pairwise";
	} else {
		msg = PAIRWISE_2;
		msgtxt = "2/4 Pairwise";
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		if (sm->req_replay_counter_used &&
		    memcmp(key->replay_counter, sm->req_replay_counter,
			   WPA_REPLAY_COUNTER_LEN) <= 0) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_WARNING,
					"received EAPOL-Key request with "
					"replayed counter");
			return;
		}
	}

	if (!(key_info & WPA_KEY_INFO_REQUEST) &&
	    (!sm->key_replay_counter_valid ||
	     memcmp(key->replay_counter, sm->key_replay_counter,
		    WPA_REPLAY_COUNTER_LEN) != 0)) {
		wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_INFO,
				 "received EAPOL-Key %s with unexpected "
				 "replay counter", msgtxt);
		wpa_hexdump(MSG_DEBUG, "expected replay counter",
			    sm->key_replay_counter, WPA_REPLAY_COUNTER_LEN);
		wpa_hexdump(MSG_DEBUG, "received replay counter",
			    key->replay_counter, WPA_REPLAY_COUNTER_LEN);
		return;
	}

	switch (msg) {
	case PAIRWISE_2:
		if (sm->wpa_ptk_state != WPA_PTK_PTKSTART &&
		    sm->wpa_ptk_state != WPA_PTK_PTKCALCNEGOTIATING) {
			wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_INFO,
					 "received EAPOL-Key msg 2/4 in "
					 "invalid state (%d) - dropped",
					 sm->wpa_ptk_state);
			return;
		}
		if (sm->wpa_ie == NULL ||
		    sm->wpa_ie_len != key_data_length ||
		    memcmp(sm->wpa_ie, key + 1, key_data_length) != 0) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"WPA IE from (Re)AssocReq did not "
					"match with msg 2/4");
			if (sm->wpa_ie) {
				wpa_hexdump(MSG_DEBUG, "WPA IE in AssocReq",
					    sm->wpa_ie, sm->wpa_ie_len);
			}
			wpa_hexdump(MSG_DEBUG, "WPA IE in msg 2/4",
				    (u8 *) (key + 1), key_data_length);
			/* MLME-DEAUTHENTICATE.request */
			wpa_sta_disconnect(wpa_auth, sm->addr);
			return;
		}
		break;
	case PAIRWISE_4:
		if (sm->wpa_ptk_state != WPA_PTK_PTKINITNEGOTIATING ||
		    !sm->PTK_valid) {
			wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_INFO,
					 "received EAPOL-Key msg 4/4 in "
					 "invalid state (%d) - dropped",
					 sm->wpa_ptk_state);
			return;
		}
		break;
	case GROUP_2:
		if (sm->wpa_ptk_group_state != WPA_PTK_GROUP_REKEYNEGOTIATING
		    || !sm->PTK_valid) {
			wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_INFO,
					 "received EAPOL-Key msg 2/2 in "
					 "invalid state (%d) - dropped",
					 sm->wpa_ptk_group_state);
			return;
		}
		break;
#ifdef CONFIG_PEERKEY
	case SMK_M1:
	case SMK_M3:
	case SMK_ERROR:
		if (!wpa_auth->conf.peerkey) {
			wpa_printf(MSG_DEBUG, "RSN: SMK M1/M3/Error, but "
				   "PeerKey use disabled - ignoring message");
			return;
		}
		if (!sm->PTK_valid) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key msg SMK in "
					"invalid state - dropped");
			return;
		}
		break;
#else /* CONFIG_PEERKEY */
	case SMK_M1:
	case SMK_M3:
	case SMK_ERROR:
		return; /* STSL disabled - ignore SMK messages */
#endif /* CONFIG_PEERKEY */
	case REQUEST:
		break;
	}

	wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
			 "received EAPOL-Key frame (%s)", msgtxt);

	if (key_info & WPA_KEY_INFO_ACK) {
		wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
				"received invalid EAPOL-Key: Key Ack set");
		return;
	}

	if (!(key_info & WPA_KEY_INFO_MIC)) {
		wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
				"received invalid EAPOL-Key: Key MIC not set");
		return;
	}

	sm->MICVerified = FALSE;
	if (sm->PTK_valid) {
		if (wpa_verify_key_mic(&sm->PTK, data, data_len)) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key with invalid MIC");
			return;
		}
		sm->MICVerified = TRUE;
		eloop_cancel_timeout(wpa_send_eapol_timeout, wpa_auth, sm);
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		if (sm->MICVerified) {
			sm->req_replay_counter_used = 1;
			memcpy(sm->req_replay_counter, key->replay_counter,
			       WPA_REPLAY_COUNTER_LEN);
		} else {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key request with "
					"invalid MIC");
			return;
		}

		/*
		 * TODO: should decrypt key data field if encryption was used;
		 * even though MAC address KDE is not normally encrypted,
		 * supplicant is allowed to encrypt it.
		 */
		if (msg == SMK_ERROR) {
#ifdef CONFIG_PEERKEY
			wpa_smk_error(wpa_auth, sm, key);
#endif /* CONFIG_PEERKEY */
			return;
		} else if (key_info & WPA_KEY_INFO_ERROR) {
			/* Supplicant reported a Michael MIC error */
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key Error Request "
					"(STA detected Michael MIC failure)");
			wpa_auth_mic_failure_report(wpa_auth, sm->addr);
			sm->dot11RSNAStatsTKIPRemoteMICFailures++;
			wpa_auth->dot11RSNAStatsTKIPRemoteMICFailures++;
			/* Error report is not a request for a new key
			 * handshake, but since Authenticator may do it, let's
			 * change the keys now anyway. */
			wpa_request_new_ptk(sm);
		} else if (key_info & WPA_KEY_INFO_KEY_TYPE) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key Request for new "
					"4-Way Handshake");
			wpa_request_new_ptk(sm);
#ifdef CONFIG_PEERKEY
		} else if (msg == SMK_M1) {
			wpa_smk_m1(wpa_auth, sm, key);
#endif /* CONFIG_PEERKEY */
		} else if (key_data_length > 0 &&
			   wpa_parse_kde_ies((const u8 *) (key + 1),
					     key_data_length, &kde) == 0 &&
			   kde.mac_addr) {
		} else {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_INFO,
					"received EAPOL-Key Request for GTK "
					"rekeying");
			/* FIX: why was this triggering PTK rekeying for the
			 * STA that requested Group Key rekeying?? */
			/* wpa_request_new_ptk(sta->wpa_sm); */
			eloop_cancel_timeout(wpa_rekey_gtk, wpa_auth, NULL);
			wpa_rekey_gtk(wpa_auth, NULL);
		}
	} else {
		/* Do not allow the same key replay counter to be reused. */
		sm->key_replay_counter_valid = FALSE;
	}

#ifdef CONFIG_PEERKEY
	if (msg == SMK_M3) {
		wpa_smk_m3(wpa_auth, sm, key);
		return;
	}
#endif /* CONFIG_PEERKEY */

	free(sm->last_rx_eapol_key);
	sm->last_rx_eapol_key = malloc(data_len);
	if (sm->last_rx_eapol_key == NULL)
		return;
	memcpy(sm->last_rx_eapol_key, data, data_len);
	sm->last_rx_eapol_key_len = data_len;

	sm->EAPOLKeyReceived = TRUE;
	sm->EAPOLKeyPairwise = !!(key_info & WPA_KEY_INFO_KEY_TYPE);
	sm->EAPOLKeyRequest = !!(key_info & WPA_KEY_INFO_REQUEST);
	memcpy(sm->SNonce, key->key_nonce, WPA_NONCE_LEN);
	wpa_sm_step(sm);
}


static void wpa_pmk_to_ptk(const u8 *pmk, const u8 *addr1, const u8 *addr2,
			   const u8 *nonce1, const u8 *nonce2,
			   u8 *ptk, size_t ptk_len)
{
	u8 data[2 * ETH_ALEN + 2 * WPA_NONCE_LEN];

	/* PTK = PRF-X(PMK, "Pairwise key expansion",
	 *             Min(AA, SA) || Max(AA, SA) ||
	 *             Min(ANonce, SNonce) || Max(ANonce, SNonce)) */

	if (memcmp(addr1, addr2, ETH_ALEN) < 0) {
		memcpy(data, addr1, ETH_ALEN);
		memcpy(data + ETH_ALEN, addr2, ETH_ALEN);
	} else {
		memcpy(data, addr2, ETH_ALEN);
		memcpy(data + ETH_ALEN, addr1, ETH_ALEN);
	}

	if (memcmp(nonce1, nonce2, WPA_NONCE_LEN) < 0) {
		memcpy(data + 2 * ETH_ALEN, nonce1, WPA_NONCE_LEN);
		memcpy(data + 2 * ETH_ALEN + WPA_NONCE_LEN, nonce2,
		       WPA_NONCE_LEN);
	} else {
		memcpy(data + 2 * ETH_ALEN, nonce2, WPA_NONCE_LEN);
		memcpy(data + 2 * ETH_ALEN + WPA_NONCE_LEN, nonce1,
		       WPA_NONCE_LEN);
	}

	sha1_prf(pmk, WPA_PMK_LEN, "Pairwise key expansion",
		 data, sizeof(data), ptk, ptk_len);

	wpa_hexdump_key(MSG_DEBUG, "PMK", pmk, WPA_PMK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "PTK", ptk, ptk_len);
}


static void wpa_gmk_to_gtk(const u8 *gmk, const u8 *addr, const u8 *gnonce,
			   u8 *gtk, size_t gtk_len)
{
	u8 data[ETH_ALEN + WPA_NONCE_LEN];

	/* GTK = PRF-X(GMK, "Group key expansion", AA || GNonce) */
	memcpy(data, addr, ETH_ALEN);
	memcpy(data + ETH_ALEN, gnonce, WPA_NONCE_LEN);

	sha1_prf(gmk, WPA_GMK_LEN, "Group key expansion",
		 data, sizeof(data), gtk, gtk_len);

	wpa_hexdump_key(MSG_DEBUG, "GMK", gmk, WPA_GMK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "GTK", gtk, gtk_len);
}


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_authenticator *wpa_auth = eloop_ctx;
	struct wpa_state_machine *sm = timeout_ctx;

	wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG, "EAPOL-Key timeout");
	sm->TimeoutEvt = TRUE;
	wpa_sm_step(sm);
}


static int wpa_calc_eapol_key_mic(int ver, u8 *key, u8 *data, size_t len,
				  u8 *mic)
{
	u8 hash[SHA1_MAC_LEN];

	switch (ver) {
	case WPA_KEY_INFO_TYPE_HMAC_MD5_RC4:
		hmac_md5(key, 16, data, len, mic);
		break;
	case WPA_KEY_INFO_TYPE_HMAC_SHA1_AES:
		hmac_sha1(key, 16, data, len, hash);
		memcpy(mic, hash, MD5_MAC_LEN);
		break;
	default:
		return -1;
	}
	return 0;
}


static void __wpa_send_eapol(struct wpa_authenticator *wpa_auth,
			     struct wpa_state_machine *sm, int key_info,
			     const u8 *key_rsc, const u8 *nonce,
			     const u8 *kde, size_t kde_len,
			     int keyidx, int encr, int force_version)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	size_t len;
	int alg;
	int key_data_len, pad_len = 0;
	u8 *buf, *pos;
	int version, pairwise;

	len = sizeof(struct ieee802_1x_hdr) + sizeof(struct wpa_eapol_key);

	if (force_version)
		version = force_version;
	else if (sm->pairwise == WPA_CIPHER_CCMP)
		version = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	else
		version = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;

	pairwise = key_info & WPA_KEY_INFO_KEY_TYPE;

	wpa_printf(MSG_DEBUG, "WPA: Send EAPOL(secure=%d mic=%d ack=%d "
		   "install=%d pairwise=%d kde_len=%lu keyidx=%d encr=%d)",
		   (key_info & WPA_KEY_INFO_SECURE) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_MIC) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_ACK) ? 1 : 0,
		   (key_info & WPA_KEY_INFO_INSTALL) ? 1 : 0,
		   pairwise, (unsigned long) kde_len, keyidx, encr);

	key_data_len = kde_len;

	if (version == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES && encr) {
		pad_len = key_data_len % 8;
		if (pad_len)
			pad_len = 8 - pad_len;
		key_data_len += pad_len + 8;
	}

	len += key_data_len;

	hdr = wpa_zalloc(len);
	if (hdr == NULL)
		return;
	hdr->version = wpa_auth->conf.eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(len  - sizeof(*hdr));
	key = (struct wpa_eapol_key *) (hdr + 1);

	key->type = sm->wpa == WPA_VERSION_WPA2 ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info |= version;
	if (encr && sm->wpa == WPA_VERSION_WPA2)
		key_info |= WPA_KEY_INFO_ENCR_KEY_DATA;
	if (sm->wpa != WPA_VERSION_WPA2)
		key_info |= keyidx << WPA_KEY_INFO_KEY_INDEX_SHIFT;
	key->key_info = htons(key_info);

	alg = pairwise ? sm->pairwise : wpa_auth->conf.wpa_group;
	switch (alg) {
	case WPA_CIPHER_CCMP:
		key->key_length = htons(16);
		break;
	case WPA_CIPHER_TKIP:
		key->key_length = htons(32);
		break;
	case WPA_CIPHER_WEP40:
		key->key_length = htons(5);
		break;
	case WPA_CIPHER_WEP104:
		key->key_length = htons(13);
		break;
	}
	if (key_info & WPA_KEY_INFO_SMK_MESSAGE)
		key->key_length = htons(0);

	/* FIX: STSL: what to use as key_replay_counter? */
	inc_byte_array(sm->key_replay_counter, WPA_REPLAY_COUNTER_LEN);
	memcpy(key->replay_counter, sm->key_replay_counter,
	       WPA_REPLAY_COUNTER_LEN);
	sm->key_replay_counter_valid = TRUE;

	if (nonce)
		memcpy(key->key_nonce, nonce, WPA_NONCE_LEN);

	if (key_rsc)
		memcpy(key->key_rsc, key_rsc, WPA_KEY_RSC_LEN);

	if (kde && !encr) {
		memcpy(key + 1, kde, kde_len);
		key->key_data_length = htons(kde_len);
	} else if (encr && kde) {
		buf = wpa_zalloc(key_data_len);
		if (buf == NULL) {
			free(hdr);
			return;
		}
		pos = buf;
		memcpy(pos, kde, kde_len);
		pos += kde_len;

		if (pad_len)
			*pos++ = 0xdd;

		wpa_hexdump_key(MSG_DEBUG, "Plaintext EAPOL-Key Key Data",
				buf, key_data_len);
		if (version == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
			aes_wrap(sm->PTK.encr_key, (key_data_len - 8) / 8, buf,
				 (u8 *) (key + 1));
			key->key_data_length = htons(key_data_len);
		} else {
			u8 ek[32];
			memcpy(key->key_iv,
			       sm->group->Counter + WPA_NONCE_LEN - 16, 16);
			inc_byte_array(sm->group->Counter, WPA_NONCE_LEN);
			memcpy(ek, key->key_iv, 16);
			memcpy(ek + 16, sm->PTK.encr_key, 16);
			memcpy(key + 1, buf, key_data_len);
			rc4_skip(ek, 32, 256, (u8 *) (key + 1), key_data_len);
			key->key_data_length = htons(key_data_len);
		}
		free(buf);
	}

	if (key_info & WPA_KEY_INFO_MIC) {
		if (!sm->PTK_valid) {
			wpa_auth_logger(wpa_auth, sm->addr, LOGGER_DEBUG,
					"PTK not valid when sending EAPOL-Key "
					"frame");
			free(hdr);
			return;
		}
		wpa_calc_eapol_key_mic(version,
				       sm->PTK.mic_key, (u8 *) hdr, len,
				       key->key_mic);
	}

	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_inc_EapolFramesTx,
			   1);
	wpa_auth_send_eapol(wpa_auth, sm->addr, (u8 *) hdr, len,
			    sm->pairwise_set);
	free(hdr);
}


static void wpa_send_eapol(struct wpa_authenticator *wpa_auth,
			   struct wpa_state_machine *sm, int key_info,
			   const u8 *key_rsc, const u8 *nonce,
			   const u8 *kde, size_t kde_len,
			   int keyidx, int encr)
{
	int timeout_ms;
	int pairwise = key_info & WPA_KEY_INFO_KEY_TYPE;

	if (sm == NULL)
		return;

	__wpa_send_eapol(wpa_auth, sm, key_info, key_rsc, nonce, kde, kde_len,
			 keyidx, encr, 0);

	timeout_ms = pairwise ? dot11RSNAConfigPairwiseUpdateTimeOut :
		dot11RSNAConfigGroupUpdateTimeOut;
	eloop_register_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000,
			       wpa_send_eapol_timeout, wpa_auth, sm);
}


static int wpa_verify_key_mic(struct wpa_ptk *PTK, u8 *data, size_t data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info;
	int ret = 0;
	u8 mic[16];

	if (data_len < sizeof(*hdr) + sizeof(*key))
		return -1;

	hdr = (struct ieee802_1x_hdr *) data;
	key = (struct wpa_eapol_key *) (hdr + 1);
	key_info = ntohs(key->key_info);
	memcpy(mic, key->key_mic, 16);
	memset(key->key_mic, 0, 16);
	if (wpa_calc_eapol_key_mic(key_info & WPA_KEY_INFO_TYPE_MASK,
				   PTK->mic_key, data, data_len, key->key_mic)
	    || memcmp(mic, key->key_mic, 16) != 0)
		ret = -1;
	memcpy(key->key_mic, mic, 16);
	return ret;
}


void wpa_remove_ptk(struct wpa_state_machine *sm)
{
	sm->PTK_valid = FALSE;
	memset(&sm->PTK, 0, sizeof(sm->PTK));
	wpa_auth_set_key(sm->wpa_auth, 0, "none", sm->addr, 0, (u8 *) "", 0);
	sm->pairwise_set = FALSE;
}


void wpa_auth_sm_event(struct wpa_state_machine *sm, wpa_event event)
{
	if (sm == NULL)
		return;

	wpa_auth_vlogger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			 "event %d notification", event);

	switch (event) {
	case WPA_AUTH:
	case WPA_ASSOC:
		break;
	case WPA_DEAUTH:
	case WPA_DISASSOC:
		sm->DeauthenticationRequest = TRUE;
		break;
	case WPA_REAUTH:
	case WPA_REAUTH_EAPOL:
		sm->ReAuthenticationRequest = TRUE;
		break;
	}

	sm->PTK_valid = FALSE;
	memset(&sm->PTK, 0, sizeof(sm->PTK));

	if (event != WPA_REAUTH_EAPOL)
		wpa_remove_ptk(sm);

	wpa_sm_step(sm);
}


static const char * wpa_alg_txt(int alg)
{
	switch (alg) {
	case WPA_CIPHER_CCMP:
		return "CCMP";
	case WPA_CIPHER_TKIP:
		return "TKIP";
	case WPA_CIPHER_WEP104:
	case WPA_CIPHER_WEP40:
		return "WEP";
	default:
		return "";
	}
}


SM_STATE(WPA_PTK, INITIALIZE)
{
	SM_ENTRY_MA(WPA_PTK, INITIALIZE, wpa_ptk);
	if (sm->Init) {
		/* Init flag is not cleared here, so avoid busy
		 * loop by claiming nothing changed. */
		sm->changed = FALSE;
	}

	sm->keycount = 0;
	if (sm->GUpdateStationKeys)
		sm->group->GKeyDoneStations--;
	sm->GUpdateStationKeys = FALSE;
	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = FALSE;
	if (1 /* Unicast cipher supported AND (ESS OR ((IBSS or WDS) and
	       * Local AA > Remote AA)) */) {
		sm->Pair = TRUE;
	}
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portEnabled, 0);
	wpa_remove_ptk(sm);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portValid, 0);
	sm->TimeoutCtr = 0;
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
		wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
				   WPA_EAPOL_authorized, 0);
	}
}


SM_STATE(WPA_PTK, DISCONNECT)
{
	SM_ENTRY_MA(WPA_PTK, DISCONNECT, wpa_ptk);
	sm->Disconnect = FALSE;
	wpa_sta_disconnect(sm->wpa_auth, sm->addr);
}


SM_STATE(WPA_PTK, DISCONNECTED)
{
	SM_ENTRY_MA(WPA_PTK, DISCONNECTED, wpa_ptk);
	if (sm->sta_counted) {
		sm->group->GNoStations--;
		sm->sta_counted = 0;
	} else {
		wpa_printf(MSG_DEBUG, "WPA: WPA_PTK::DISCONNECTED - did not "
			   "decrease GNoStations (STA " MACSTR ")",
			   MAC2STR(sm->addr));
	}
	sm->DeauthenticationRequest = FALSE;
}


SM_STATE(WPA_PTK, AUTHENTICATION)
{
	SM_ENTRY_MA(WPA_PTK, AUTHENTICATION, wpa_ptk);
	if (!sm->sta_counted) {
		sm->group->GNoStations++;
		sm->sta_counted = 1;
	} else {
		wpa_printf(MSG_DEBUG, "WPA: WPA_PTK::DISCONNECTED - did not "
			   "increase GNoStations (STA " MACSTR ")",
			   MAC2STR(sm->addr));
	}
	memset(&sm->PTK, 0, sizeof(sm->PTK));
	sm->PTK_valid = FALSE;
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portControl_Auto,
			   1);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portEnabled, 1);
	sm->AuthenticationRequest = FALSE;
}


SM_STATE(WPA_PTK, AUTHENTICATION2)
{
	SM_ENTRY_MA(WPA_PTK, AUTHENTICATION2, wpa_ptk);
	memcpy(sm->ANonce, sm->group->Counter, WPA_NONCE_LEN);
	inc_byte_array(sm->group->Counter, WPA_NONCE_LEN);
	sm->ReAuthenticationRequest = FALSE;
	/* IEEE 802.11i does not clear TimeoutCtr here, but this is more
	 * logical place than INITIALIZE since AUTHENTICATION2 can be
	 * re-entered on ReAuthenticationRequest without going through
	 * INITIALIZE. */
	sm->TimeoutCtr = 0;
}


SM_STATE(WPA_PTK, INITPMK)
{
	size_t len = WPA_PMK_LEN;

	SM_ENTRY_MA(WPA_PTK, INITPMK, wpa_ptk);
	if (sm->pmksa) {
		wpa_printf(MSG_DEBUG, "WPA: PMK from PMKSA cache");
		memcpy(sm->PMK, sm->pmksa->pmk, WPA_PMK_LEN);
	} else if (wpa_auth_get_pmk(sm->wpa_auth, sm->addr, sm->PMK, &len) ==
		   0) {
		wpa_printf(MSG_DEBUG, "WPA: PMK from EAPOL state machine "
			   "(len=%lu)", (unsigned long) len);
	} else {
		wpa_printf(MSG_DEBUG, "WPA: Could not get PMK");
	}

	sm->req_replay_counter_used = 0;
	/* IEEE 802.11i does not set keyRun to FALSE, but not doing this
	 * will break reauthentication since EAPOL state machines may not be
	 * get into AUTHENTICATING state that clears keyRun before WPA state
	 * machine enters AUTHENTICATION2 state and goes immediately to INITPMK
	 * state and takes PMK from the previously used AAA Key. This will
	 * eventually fail in 4-Way Handshake because Supplicant uses PMK
	 * derived from the new AAA Key. Setting keyRun = FALSE here seems to
	 * be good workaround for this issue. */
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyRun, 0);
}


SM_STATE(WPA_PTK, INITPSK)
{
	const u8 *psk;
	SM_ENTRY_MA(WPA_PTK, INITPSK, wpa_ptk);
	psk = wpa_auth_get_psk(sm->wpa_auth, sm->addr, NULL);
	if (psk)
		memcpy(sm->PMK, psk, WPA_PMK_LEN);
	sm->req_replay_counter_used = 0;
}


SM_STATE(WPA_PTK, PTKSTART)
{
	u8 buf[2 + RSN_SELECTOR_LEN + PMKID_LEN], *pmkid = NULL;
	size_t pmkid_len = 0;

	SM_ENTRY_MA(WPA_PTK, PTKSTART, wpa_ptk);
	sm->PTKRequest = FALSE;
	sm->TimeoutEvt = FALSE;
	wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			"sending 1/4 msg of 4-Way Handshake");
	/*
	 * TODO: Could add PMKID even with WPA2-PSK, but only if there is only
	 * one possible PSK for this STA.
	 */
	if (sm->wpa == WPA_VERSION_WPA2 &&
	    sm->wpa_key_mgmt != WPA_KEY_MGMT_PSK) {
		pmkid = buf;
		pmkid_len = 2 + RSN_SELECTOR_LEN + PMKID_LEN;
		pmkid[0] = WLAN_EID_GENERIC;
		pmkid[1] = RSN_SELECTOR_LEN + PMKID_LEN;
		memcpy(&pmkid[2], RSN_KEY_DATA_PMKID, RSN_SELECTOR_LEN);
		if (sm->pmksa)
			memcpy(&pmkid[2 + RSN_SELECTOR_LEN], sm->pmksa->pmkid,
			       PMKID_LEN);
		else {
			/*
			 * Calculate PMKID since no PMKSA cache entry was
			 * available with pre-calculated PMKID.
			 */
			rsn_pmkid(sm->PMK, WPA_PMK_LEN, sm->wpa_auth->addr,
				  sm->addr, &pmkid[2 + RSN_SELECTOR_LEN]);
		}
	}
	wpa_send_eapol(sm->wpa_auth, sm,
		       WPA_KEY_INFO_ACK | WPA_KEY_INFO_KEY_TYPE, NULL,
		       sm->ANonce, pmkid, pmkid_len, 0, 0);
	sm->TimeoutCtr++;
}


SM_STATE(WPA_PTK, PTKCALCNEGOTIATING)
{
	struct wpa_ptk PTK;
	int ok = 0;
	const u8 *pmk = NULL;

	SM_ENTRY_MA(WPA_PTK, PTKCALCNEGOTIATING, wpa_ptk);
	sm->EAPOLKeyReceived = FALSE;

	/* WPA with IEEE 802.1X: use the derived PMK from EAP
	 * WPA-PSK: iterate through possible PSKs and select the one matching
	 * the packet */
	for (;;) {
		if (sm->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
			pmk = wpa_auth_get_psk(sm->wpa_auth, sm->addr, pmk);
			if (pmk == NULL)
				break;
		} else
			pmk = sm->PMK;

		wpa_pmk_to_ptk(pmk, sm->wpa_auth->addr, sm->addr,
			       sm->ANonce, sm->SNonce,
			       (u8 *) &PTK, sizeof(PTK));

		if (wpa_verify_key_mic(&PTK, sm->last_rx_eapol_key,
				       sm->last_rx_eapol_key_len) == 0) {
			ok = 1;
			break;
		}

		if (sm->wpa_key_mgmt != WPA_KEY_MGMT_PSK)
			break;
	}

	if (!ok) {
		wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
				"invalid MIC in msg 2/4 of 4-Way Handshake");
		return;
	}

	eloop_cancel_timeout(wpa_send_eapol_timeout, sm->wpa_auth, sm);

	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
		/* PSK may have changed from the previous choice, so update
		 * state machine data based on whatever PSK was selected here.
		 */
		memcpy(sm->PMK, pmk, WPA_PMK_LEN);
	}

	sm->MICVerified = TRUE;

	memcpy(&sm->PTK, &PTK, sizeof(PTK));
	sm->PTK_valid = TRUE;
}


SM_STATE(WPA_PTK, PTKCALCNEGOTIATING2)
{
	SM_ENTRY_MA(WPA_PTK, PTKCALCNEGOTIATING2, wpa_ptk);
	sm->TimeoutCtr = 0;
}


#ifdef CONFIG_IEEE80211W

static int ieee80211w_kde_len(struct wpa_state_machine *sm)
{
	if (sm->mgmt_frame_prot) {
		return 2 + RSN_SELECTOR_LEN + sizeof(struct wpa_dhv_kde) +
			2 + RSN_SELECTOR_LEN + sizeof(struct wpa_igtk_kde);
	}

	return 0;
}


static u8 * ieee80211w_kde_add(struct wpa_state_machine *sm, u8 *pos)
{
	struct wpa_dhv_kde dhv;
	struct wpa_igtk_kde igtk;
	struct wpa_group *gsm = sm->group;
	u8 mac[32];
	const u8 *addr[3];
	size_t len[3];

	if (!sm->mgmt_frame_prot)
		return pos;

	addr[0] = sm->wpa_auth->addr;
	len[0] = ETH_ALEN;
	addr[1] = sm->addr;
	len[1] = ETH_ALEN;
	addr[2] = gsm->DGTK;
	len[2] = WPA_DGTK_LEN;
	sha256_vector(3, addr, len, mac);
	memcpy(dhv.dhv, mac, WPA_DHV_LEN);
	wpa_hexdump_key(MSG_DEBUG, "WPA: DHV", dhv.dhv, WPA_DHV_LEN);
	pos = wpa_add_kde(pos, RSN_KEY_DATA_DHV,
			  (const u8 *) &dhv, sizeof(dhv), NULL, 0);

	igtk.keyid[0] = gsm->GN;
	igtk.keyid[1] = 0;
	if (wpa_auth_get_seqnum_igtk(sm->wpa_auth, NULL, gsm->GN, igtk.pn) < 0)
		memset(igtk.pn, 0, sizeof(igtk.pn));
	memcpy(igtk.igtk, gsm->IGTK[gsm->GN - 1], WPA_IGTK_LEN);
	pos = wpa_add_kde(pos, RSN_KEY_DATA_IGTK,
			  (const u8 *) &igtk, sizeof(igtk), NULL, 0);

	return pos;
}

#else /* CONFIG_IEEE80211W */

static int ieee80211w_kde_len(struct wpa_state_machine *sm)
{
	return 0;
}


static u8 * ieee80211w_kde_add(struct wpa_state_machine *sm, u8 *pos)
{
	return pos;
}

#endif /* CONFIG_IEEE80211W */


SM_STATE(WPA_PTK, PTKINITNEGOTIATING)
{
	u8 rsc[WPA_KEY_RSC_LEN], *_rsc, *gtk, *kde, *pos;
	size_t gtk_len, kde_len;
	struct wpa_group *gsm = sm->group;
	u8 *wpa_ie;
	int wpa_ie_len, secure, keyidx, encr = 0;

	SM_ENTRY_MA(WPA_PTK, PTKINITNEGOTIATING, wpa_ptk);
	sm->TimeoutEvt = FALSE;
	/* Send EAPOL(1, 1, 1, Pair, P, RSC, ANonce, MIC(PTK), RSNIE, GTK[GN])
	 */
	memset(rsc, 0, WPA_KEY_RSC_LEN);
	wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, rsc);
	wpa_ie = sm->wpa_auth->wpa_ie;
	wpa_ie_len = sm->wpa_auth->wpa_ie_len;
	if (sm->wpa == WPA_VERSION_WPA &&
	    (sm->wpa_auth->conf.wpa & HOSTAPD_WPA_VERSION_WPA2) &&
	    wpa_ie_len > wpa_ie[1] + 2 && wpa_ie[0] == WLAN_EID_RSN) {
		/* WPA-only STA, remove RSN IE */
		wpa_ie = wpa_ie + wpa_ie[1] + 2;
		wpa_ie_len = wpa_ie[1] + 2;
	}
	wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			"sending 3/4 msg of 4-Way Handshake");
	if (sm->wpa == WPA_VERSION_WPA2) {
		/* WPA2 send GTK in the 4-way handshake */
		secure = 1;
		gtk = gsm->GTK[gsm->GN - 1];
		gtk_len = gsm->GTK_len;
		keyidx = gsm->GN;
		_rsc = rsc;
		encr = 1;
	} else {
		/* WPA does not include GTK in msg 3/4 */
		secure = 0;
		gtk = NULL;
		gtk_len = 0;
		keyidx = 0;
		_rsc = NULL;
	}

	kde_len = wpa_ie_len + ieee80211w_kde_len(sm);
	if (gtk)
		kde_len += 2 + RSN_SELECTOR_LEN + 2 + gtk_len;
	kde = malloc(kde_len);
	if (kde == NULL)
		return;

	pos = kde;
	memcpy(pos, wpa_ie, wpa_ie_len);
	pos += wpa_ie_len;
	if (gtk) {
		u8 hdr[2];
		hdr[0] = keyidx & 0x03;
		hdr[1] = 0;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gtk, gtk_len);
	}
	pos = ieee80211w_kde_add(sm, pos);

	wpa_send_eapol(sm->wpa_auth, sm,
		       (secure ? WPA_KEY_INFO_SECURE : 0) | WPA_KEY_INFO_MIC |
		       WPA_KEY_INFO_ACK | WPA_KEY_INFO_INSTALL |
		       WPA_KEY_INFO_KEY_TYPE,
		       _rsc, sm->ANonce, kde, pos - kde, keyidx, encr);
	free(kde);
	sm->TimeoutCtr++;
}


SM_STATE(WPA_PTK, PTKINITDONE)
{
	SM_ENTRY_MA(WPA_PTK, PTKINITDONE, wpa_ptk);
	sm->EAPOLKeyReceived = FALSE;
	if (sm->Pair) {
		char *alg;
		int klen;
		if (sm->pairwise == WPA_CIPHER_TKIP) {
			alg = "TKIP";
			klen = 32;
		} else {
			alg = "CCMP";
			klen = 16;
		}
		if (wpa_auth_set_key(sm->wpa_auth, 0, alg, sm->addr, 0,
				     sm->PTK.tk1, klen)) {
			wpa_sta_disconnect(sm->wpa_auth, sm->addr);
			return;
		}
		/* FIX: MLME-SetProtection.Request(TA, Tx_Rx) */
		sm->pairwise_set = TRUE;

		if (sm->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
			wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
					   WPA_EAPOL_authorized, 1);
		}
	}

	if (0 /* IBSS == TRUE */) {
		sm->keycount++;
		if (sm->keycount == 2) {
			wpa_auth_set_eapol(sm->wpa_auth, sm->addr,
					   WPA_EAPOL_portValid, 1);
		}
	} else {
		wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_portValid,
				   1);
	}
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyAvailable, 0);
	wpa_auth_set_eapol(sm->wpa_auth, sm->addr, WPA_EAPOL_keyDone, 1);
	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = TRUE;
	else
		sm->has_GTK = TRUE;
	wpa_auth_vlogger(sm->wpa_auth, sm->addr, LOGGER_INFO,
			 "pairwise key handshake completed (%s)",
			 sm->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
}


SM_STEP(WPA_PTK)
{
	struct wpa_authenticator *wpa_auth = sm->wpa_auth;

	if (sm->Init)
		SM_ENTER(WPA_PTK, INITIALIZE);
	else if (sm->Disconnect
		 /* || FIX: dot11RSNAConfigSALifetime timeout */)
		SM_ENTER(WPA_PTK, DISCONNECT);
	else if (sm->DeauthenticationRequest)
		SM_ENTER(WPA_PTK, DISCONNECTED);
	else if (sm->AuthenticationRequest)
		SM_ENTER(WPA_PTK, AUTHENTICATION);
	else if (sm->ReAuthenticationRequest)
		SM_ENTER(WPA_PTK, AUTHENTICATION2);
	else if (sm->PTKRequest)
		SM_ENTER(WPA_PTK, PTKSTART);
	else switch (sm->wpa_ptk_state) {
	case WPA_PTK_INITIALIZE:
		break;
	case WPA_PTK_DISCONNECT:
		SM_ENTER(WPA_PTK, DISCONNECTED);
		break;
	case WPA_PTK_DISCONNECTED:
		SM_ENTER(WPA_PTK, INITIALIZE);
		break;
	case WPA_PTK_AUTHENTICATION:
		SM_ENTER(WPA_PTK, AUTHENTICATION2);
		break;
	case WPA_PTK_AUTHENTICATION2:
		if ((sm->wpa_key_mgmt == WPA_KEY_MGMT_IEEE8021X) &&
		    wpa_auth_get_eapol(sm->wpa_auth, sm->addr,
				       WPA_EAPOL_keyRun) > 0)
			SM_ENTER(WPA_PTK, INITPMK);
		else if ((sm->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
			 /* FIX: && 802.1X::keyRun */)
			SM_ENTER(WPA_PTK, INITPSK);
		break;
	case WPA_PTK_INITPMK:
		if (wpa_auth_get_eapol(sm->wpa_auth, sm->addr,
				       WPA_EAPOL_keyAvailable) > 0)
			SM_ENTER(WPA_PTK, PTKSTART);
		else {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_INITPSK:
		if (wpa_auth_get_psk(sm->wpa_auth, sm->addr, NULL))
			SM_ENTER(WPA_PTK, PTKSTART);
		else {
			wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_INFO,
					"no PSK configured for the STA");
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_PTKSTART:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    sm->EAPOLKeyPairwise)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->TimeoutCtr >
			 (int) dot11RSNAConfigPairwiseUpdateCount) {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		} else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKSTART);
		break;
	case WPA_PTK_PTKCALCNEGOTIATING:
		if (sm->MICVerified)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING2);
		else if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
			 sm->EAPOLKeyPairwise)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKSTART);
		break;
	case WPA_PTK_PTKCALCNEGOTIATING2:
		SM_ENTER(WPA_PTK, PTKINITNEGOTIATING);
		break;
	case WPA_PTK_PTKINITNEGOTIATING:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    sm->EAPOLKeyPairwise && sm->MICVerified)
			SM_ENTER(WPA_PTK, PTKINITDONE);
		else if (sm->TimeoutCtr >
			 (int) dot11RSNAConfigPairwiseUpdateCount) {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		} else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK, PTKINITNEGOTIATING);
		break;
	case WPA_PTK_PTKINITDONE:
		break;
	}
}


SM_STATE(WPA_PTK_GROUP, IDLE)
{
	SM_ENTRY_MA(WPA_PTK_GROUP, IDLE, wpa_ptk_group);
	if (sm->Init) {
		/* Init flag is not cleared here, so avoid busy
		 * loop by claiming nothing changed. */
		sm->changed = FALSE;
	}
	sm->GTimeoutCtr = 0;
}


SM_STATE(WPA_PTK_GROUP, REKEYNEGOTIATING)
{
	u8 rsc[WPA_KEY_RSC_LEN];
	struct wpa_group *gsm = sm->group;
	u8 *kde, *pos, hdr[2];
	size_t kde_len;

	SM_ENTRY_MA(WPA_PTK_GROUP, REKEYNEGOTIATING, wpa_ptk_group);
	if (sm->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = FALSE;
	sm->TimeoutEvt = FALSE;
	/* Send EAPOL(1, 1, 1, !Pair, G, RSC, GNonce, MIC(PTK), GTK[GN]) */
	memset(rsc, 0, WPA_KEY_RSC_LEN);
	if (gsm->wpa_group_state == WPA_GROUP_SETKEYSDONE)
		wpa_auth_get_seqnum(sm->wpa_auth, NULL, gsm->GN, rsc);
	wpa_auth_logger(sm->wpa_auth, sm->addr, LOGGER_DEBUG,
			"sending 1/2 msg of Group Key Handshake");

	if (sm->wpa == WPA_VERSION_WPA2) {
		kde_len = 2 + RSN_SELECTOR_LEN + 2 + gsm->GTK_len +
			ieee80211w_kde_len(sm);
		kde = malloc(kde_len);
		if (kde == NULL)
			return;

		pos = kde;
		hdr[0] = gsm->GN & 0x03;
		hdr[1] = 0;
		pos = wpa_add_kde(pos, RSN_KEY_DATA_GROUPKEY, hdr, 2,
				  gsm->GTK[gsm->GN - 1], gsm->GTK_len);
		pos = ieee80211w_kde_add(sm, pos);
	} else {
		kde = gsm->GTK[gsm->GN - 1];
		pos = kde + gsm->GTK_len;
	}

	wpa_send_eapol(sm->wpa_auth, sm,
		       WPA_KEY_INFO_SECURE | WPA_KEY_INFO_MIC |
		       WPA_KEY_INFO_ACK |
		       (!sm->Pair ? WPA_KEY_INFO_INSTALL : 0),
		       rsc, gsm->GNonce, kde, pos - kde, gsm->GN, 1);
	if (sm->wpa == WPA_VERSION_WPA2)
		free(kde);
	sm->GTimeoutCtr++;
}


SM_STATE(WPA_PTK_GROUP, REKEYESTABLISHED)
{
	SM_ENTRY_MA(WPA_PTK_GROUP, REKEYESTABLISHED, wpa_ptk_group);
	sm->EAPOLKeyReceived = FALSE;
	sm->GUpdateStationKeys = FALSE;
	sm->group->GKeyDoneStations--;
	sm->GTimeoutCtr = 0;
	/* FIX: MLME.SetProtection.Request(TA, Tx_Rx) */
	wpa_auth_vlogger(sm->wpa_auth, sm->addr, LOGGER_INFO,
			 "group key handshake completed (%s)",
			 sm->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
	sm->has_GTK = TRUE;
}


SM_STATE(WPA_PTK_GROUP, KEYERROR)
{
	SM_ENTRY_MA(WPA_PTK_GROUP, KEYERROR, wpa_ptk_group);
	sm->group->GKeyDoneStations--;
	sm->GUpdateStationKeys = FALSE;
	sm->Disconnect = TRUE;
}


SM_STEP(WPA_PTK_GROUP)
{
	if (sm->Init)
		SM_ENTER(WPA_PTK_GROUP, IDLE);
	else switch (sm->wpa_ptk_group_state) {
	case WPA_PTK_GROUP_IDLE:
		if (sm->GUpdateStationKeys ||
		    (sm->wpa == WPA_VERSION_WPA && sm->PInitAKeys))
			SM_ENTER(WPA_PTK_GROUP, REKEYNEGOTIATING);
		break;
	case WPA_PTK_GROUP_REKEYNEGOTIATING:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    !sm->EAPOLKeyPairwise && sm->MICVerified)
			SM_ENTER(WPA_PTK_GROUP, REKEYESTABLISHED);
		else if (sm->GTimeoutCtr >
			 (int) dot11RSNAConfigGroupUpdateCount)
			SM_ENTER(WPA_PTK_GROUP, KEYERROR);
		else if (sm->TimeoutEvt)
			SM_ENTER(WPA_PTK_GROUP, REKEYNEGOTIATING);
		break;
	case WPA_PTK_GROUP_KEYERROR:
		SM_ENTER(WPA_PTK_GROUP, IDLE);
		break;
	case WPA_PTK_GROUP_REKEYESTABLISHED:
		SM_ENTER(WPA_PTK_GROUP, IDLE);
		break;
	}
}


static void wpa_gtk_update(struct wpa_authenticator *wpa_auth,
			   struct wpa_group *group)
{
	/* FIX: is this the correct way of getting GNonce? */
	memcpy(group->GNonce, group->Counter, WPA_NONCE_LEN);
	inc_byte_array(group->Counter, WPA_NONCE_LEN);
	wpa_gmk_to_gtk(group->GMK, wpa_auth->addr, group->GNonce,
		       group->GTK[group->GN - 1], group->GTK_len);

#ifdef CONFIG_IEEE80211W
	if (wpa_auth->conf.ieee80211w != WPA_NO_IEEE80211W) {
		hostapd_get_rand(group->DGTK, WPA_DGTK_LEN);
		wpa_hexdump_key(MSG_DEBUG, "DGTK", group->DGTK, WPA_DGTK_LEN);
		hostapd_get_rand(group->IGTK[group->GN - 1], WPA_IGTK_LEN);
		wpa_hexdump_key(MSG_DEBUG, "IGTK",
				group->IGTK[group->GN - 1], WPA_IGTK_LEN);
	}
#endif /* CONFIG_IEEE80211W */
}


static void wpa_group_gtk_init(struct wpa_authenticator *wpa_auth,
			       struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG, "WPA: group state machine entering state "
		   "GTK_INIT (VLAN-ID %d)", group->vlan_id);
	group->changed = FALSE; /* GInit is not cleared here; avoid loop */
	group->wpa_group_state = WPA_GROUP_GTK_INIT;

	/* GTK[0..N] = 0 */
	memset(group->GTK, 0, sizeof(group->GTK));
	group->GN = 1;
	group->GM = 2;
	/* GTK[GN] = CalcGTK() */
	wpa_gtk_update(wpa_auth, group);
}


static int wpa_group_update_sta(struct wpa_state_machine *sm, void *ctx)
{
	sm->GUpdateStationKeys = TRUE;
	wpa_sm_step(sm);
	return 0;
}


static void wpa_group_setkeys(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group)
{
	int tmp;

	wpa_printf(MSG_DEBUG, "WPA: group state machine entering state "
		   "SETKEYS (VLAN-ID %d)", group->vlan_id);
	group->changed = TRUE;
	group->wpa_group_state = WPA_GROUP_SETKEYS;
	group->GTKReKey = FALSE;
	tmp = group->GM;
	group->GM = group->GN;
	group->GN = tmp;
	group->GKeyDoneStations = group->GNoStations;
	wpa_gtk_update(wpa_auth, group);

	wpa_auth_for_each_sta(wpa_auth, wpa_group_update_sta, NULL);
}


static void wpa_group_setkeysdone(struct wpa_authenticator *wpa_auth,
				  struct wpa_group *group)
{
	wpa_printf(MSG_DEBUG, "WPA: group state machine entering state "
		   "SETKEYSDONE (VLAN-ID %d)", group->vlan_id);
	group->changed = TRUE;
	group->wpa_group_state = WPA_GROUP_SETKEYSDONE;
	wpa_auth_set_key(wpa_auth, group->vlan_id,
			 wpa_alg_txt(wpa_auth->conf.wpa_group),
			 NULL, group->GN, group->GTK[group->GN - 1],
			 group->GTK_len);

#ifdef CONFIG_IEEE80211W
	if (wpa_auth->conf.ieee80211w != WPA_NO_IEEE80211W) {
		wpa_auth_set_key(wpa_auth, group->vlan_id, "IGTK",
				 NULL, group->GN, group->IGTK[group->GN - 1],
				 WPA_IGTK_LEN);
		wpa_auth_set_key(wpa_auth, group->vlan_id, "DGTK",
				 NULL, 0, group->DGTK, WPA_DGTK_LEN);
	}
#endif /* CONFIG_IEEE80211W */
}


static void wpa_group_sm_step(struct wpa_authenticator *wpa_auth,
			      struct wpa_group *group)
{
	if (group->GInit) {
		wpa_group_gtk_init(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_GTK_INIT &&
		   group->GTKAuthenticator) {
		wpa_group_setkeysdone(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_SETKEYSDONE &&
		   group->GTKReKey) {
		wpa_group_setkeys(wpa_auth, group);
	} else if (group->wpa_group_state == WPA_GROUP_SETKEYS) {
		if (group->GKeyDoneStations == 0)
			wpa_group_setkeysdone(wpa_auth, group);
		else if (group->GTKReKey)
			wpa_group_setkeys(wpa_auth, group);
	}
}


static void wpa_sm_step(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return;

	if (sm->in_step_loop) {
		/* This should not happen, but if it does, make sure we do not
		 * end up freeing the state machine too early by exiting the
		 * recursive call. */
		wpa_printf(MSG_ERROR, "WPA: wpa_sm_step() called recursively");
		return;
	}

	sm->in_step_loop = 1;
	do {
		if (sm->pending_deinit)
			break;

		sm->changed = FALSE;
		sm->wpa_auth->group->changed = FALSE;

		SM_STEP_RUN(WPA_PTK);
		if (sm->pending_deinit)
			break;
		SM_STEP_RUN(WPA_PTK_GROUP);
		if (sm->pending_deinit)
			break;
		wpa_group_sm_step(sm->wpa_auth, sm->group);
	} while (sm->changed || sm->wpa_auth->group->changed);
	sm->in_step_loop = 0;

	if (sm->pending_deinit) {
		wpa_printf(MSG_DEBUG, "WPA: Completing pending STA state "
			   "machine deinit for " MACSTR, MAC2STR(sm->addr));
		wpa_free_sta_sm(sm);
	}
}


static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_state_machine *sm = eloop_ctx;
	wpa_sm_step(sm);
}


void wpa_auth_sm_notify(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return;
	eloop_register_timeout(0, 0, wpa_sm_call_step, sm, NULL);
}


void wpa_gtk_rekey(struct wpa_authenticator *wpa_auth)
{
	int tmp, i;
	struct wpa_group *group;

	if (wpa_auth == NULL)
		return;

	group = wpa_auth->group;

	for (i = 0; i < 2; i++) {
		tmp = group->GM;
		group->GM = group->GN;
		group->GN = tmp;
		wpa_gtk_update(wpa_auth, group);
	}
}


static const char * wpa_bool_txt(int bool)
{
	return bool ? "TRUE" : "FALSE";
}


static int wpa_cipher_bits(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_CCMP:
		return 128;
	case WPA_CIPHER_TKIP:
		return 256;
	case WPA_CIPHER_WEP104:
		return 104;
	case WPA_CIPHER_WEP40:
		return 40;
	default:
		return 0;
	}
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) (s)[0], (s)[1], (s)[2], (s)[3]

int wpa_get_mib(struct wpa_authenticator *wpa_auth, char *buf, size_t buflen)
{
	int len = 0, ret;
	char pmkid_txt[PMKID_LEN * 2 + 1];

	if (wpa_auth == NULL)
		return len;

	ret = snprintf(buf + len, buflen - len,
		       "dot11RSNAOptionImplemented=TRUE\n"
#ifdef CONFIG_RSN_PREAUTH
		       "dot11RSNAPreauthenticationImplemented=TRUE\n"
#else /* CONFIG_RSN_PREAUTH */
		       "dot11RSNAPreauthenticationImplemented=FALSE\n"
#endif /* CONFIG_RSN_PREAUTH */
		       "dot11RSNAEnabled=%s\n"
		       "dot11RSNAPreauthenticationEnabled=%s\n",
		       wpa_bool_txt(wpa_auth->conf.wpa &
				    HOSTAPD_WPA_VERSION_WPA2),
		       wpa_bool_txt(wpa_auth->conf.rsn_preauth));
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	wpa_snprintf_hex(pmkid_txt, sizeof(pmkid_txt),
			 wpa_auth->dot11RSNAPMKIDUsed, PMKID_LEN);

	ret = snprintf(buf + len, buflen - len,
		       "dot11RSNAConfigVersion=%u\n"
		       "dot11RSNAConfigPairwiseKeysSupported=9999\n"
		       /* FIX: dot11RSNAConfigGroupCipher */
		       /* FIX: dot11RSNAConfigGroupRekeyMethod */
		       /* FIX: dot11RSNAConfigGroupRekeyTime */
		       /* FIX: dot11RSNAConfigGroupRekeyPackets */
		       "dot11RSNAConfigGroupRekeyStrict=%u\n"
		       "dot11RSNAConfigGroupUpdateCount=%u\n"
		       "dot11RSNAConfigPairwiseUpdateCount=%u\n"
		       "dot11RSNAConfigGroupCipherSize=%u\n"
		       "dot11RSNAConfigPMKLifetime=%u\n"
		       "dot11RSNAConfigPMKReauthThreshold=%u\n"
		       "dot11RSNAConfigNumberOfPTKSAReplayCounters=0\n"
		       "dot11RSNAConfigSATimeout=%u\n"
		       "dot11RSNAAuthenticationSuiteSelected=" RSN_SUITE "\n"
		       "dot11RSNAPairwiseCipherSelected=" RSN_SUITE "\n"
		       "dot11RSNAGroupCipherSelected=" RSN_SUITE "\n"
		       "dot11RSNAPMKIDUsed=%s\n"
		       "dot11RSNAAuthenticationSuiteRequested=" RSN_SUITE "\n"
		       "dot11RSNAPairwiseCipherRequested=" RSN_SUITE "\n"
		       "dot11RSNAGroupCipherRequested=" RSN_SUITE "\n"
		       "dot11RSNATKIPCounterMeasuresInvoked=%u\n"
		       "dot11RSNA4WayHandshakeFailures=%u\n"
		       "dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n",
		       RSN_VERSION,
		       !!wpa_auth->conf.wpa_strict_rekey,
		       dot11RSNAConfigGroupUpdateCount,
		       dot11RSNAConfigPairwiseUpdateCount,
		       wpa_cipher_bits(wpa_auth->conf.wpa_group),
		       dot11RSNAConfigPMKLifetime,
		       dot11RSNAConfigPMKReauthThreshold,
		       dot11RSNAConfigSATimeout,
		       RSN_SUITE_ARG(wpa_auth->
				     dot11RSNAAuthenticationSuiteSelected),
		       RSN_SUITE_ARG(wpa_auth->
				     dot11RSNAPairwiseCipherSelected),
		       RSN_SUITE_ARG(wpa_auth->dot11RSNAGroupCipherSelected),
		       pmkid_txt,
		       RSN_SUITE_ARG(wpa_auth->
				     dot11RSNAAuthenticationSuiteRequested),
		       RSN_SUITE_ARG(wpa_auth->
				     dot11RSNAPairwiseCipherRequested),
		       RSN_SUITE_ARG(wpa_auth->dot11RSNAGroupCipherRequested),
		       wpa_auth->dot11RSNATKIPCounterMeasuresInvoked,
		       wpa_auth->dot11RSNA4WayHandshakeFailures);
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	/* TODO: dot11RSNAConfigPairwiseCiphersTable */
	/* TODO: dot11RSNAConfigAuthenticationSuitesTable */

	/* Private MIB */
	ret = snprintf(buf + len, buflen - len, "hostapdWPAGroupState=%d\n",
		       wpa_auth->group->wpa_group_state);
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	return len;
}


int wpa_get_mib_sta(struct wpa_state_machine *sm, char *buf, size_t buflen)
{
	int len = 0, ret;
	u8 not_used[4] = { 0, 0, 0, 0 };
	const u8 *pairwise = not_used;

	if (sm == NULL)
		return 0;

	/* TODO: FF-FF-FF-FF-FF-FF entry for broadcast/multicast stats */

	/* dot11RSNAStatsEntry */

	if (sm->wpa == WPA_VERSION_WPA) {
		if (sm->pairwise == WPA_CIPHER_CCMP)
			pairwise = WPA_CIPHER_SUITE_CCMP;
		else if (sm->pairwise == WPA_CIPHER_TKIP)
			pairwise = WPA_CIPHER_SUITE_TKIP;
		else if (sm->pairwise == WPA_CIPHER_WEP104)
			pairwise = WPA_CIPHER_SUITE_WEP104;
		else if (sm->pairwise == WPA_CIPHER_WEP40)
			pairwise = WPA_CIPHER_SUITE_WEP40;
		else if (sm->pairwise == WPA_CIPHER_NONE)
			pairwise = WPA_CIPHER_SUITE_NONE;
	} else if (sm->wpa == WPA_VERSION_WPA2) {
		if (sm->pairwise == WPA_CIPHER_CCMP)
			pairwise = RSN_CIPHER_SUITE_CCMP;
		else if (sm->pairwise == WPA_CIPHER_TKIP)
			pairwise = RSN_CIPHER_SUITE_TKIP;
		else if (sm->pairwise == WPA_CIPHER_WEP104)
			pairwise = RSN_CIPHER_SUITE_WEP104;
		else if (sm->pairwise == WPA_CIPHER_WEP40)
			pairwise = RSN_CIPHER_SUITE_WEP40;
		else if (sm->pairwise == WPA_CIPHER_NONE)
			pairwise = RSN_CIPHER_SUITE_NONE;
	} else
		return 0;

	ret = snprintf(buf + len, buflen - len,
		       /* TODO: dot11RSNAStatsIndex */
		       "dot11RSNAStatsSTAAddress=" MACSTR "\n"
		       "dot11RSNAStatsVersion=1\n"
		       "dot11RSNAStatsSelectedPairwiseCipher=" RSN_SUITE "\n"
		       /* TODO: dot11RSNAStatsTKIPICVErrors */
		       "dot11RSNAStatsTKIPLocalMICFailures=%u\n"
		       "dot11RSNAStatsTKIPRemoveMICFailures=%u\n"
		       /* TODO: dot11RSNAStatsCCMPReplays */
		       /* TODO: dot11RSNAStatsCCMPDecryptErrors */
		       /* TODO: dot11RSNAStatsTKIPReplays */,
		       MAC2STR(sm->addr),
		       RSN_SUITE_ARG(pairwise),
		       sm->dot11RSNAStatsTKIPLocalMICFailures,
		       sm->dot11RSNAStatsTKIPRemoteMICFailures);
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	/* Private MIB */
	ret = snprintf(buf + len, buflen - len,
		       "hostapdWPAPTKState=%d\n"
		       "hostapdWPAPTKGroupState=%d\n",
		       sm->wpa_ptk_state,
		       sm->wpa_ptk_group_state);
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	return len;
}


void wpa_auth_countermeasures_start(struct wpa_authenticator *wpa_auth)
{
	if (wpa_auth)
		wpa_auth->dot11RSNATKIPCounterMeasuresInvoked++;
}


int wpa_auth_pairwise_set(struct wpa_state_machine *sm)
{
	return sm && sm->pairwise_set;
}


int wpa_auth_sta_key_mgmt(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return -1;
	return sm->wpa_key_mgmt;
}


int wpa_auth_sta_wpa_version(struct wpa_state_machine *sm)
{
	if (sm == NULL)
		return 0;
	return sm->wpa;
}


int wpa_auth_sta_clear_pmksa(struct wpa_state_machine *sm,
			     struct rsn_pmksa_cache_entry *entry)
{
	if (sm == NULL || sm->pmksa != entry)
		return -1;
	sm->pmksa = NULL;
	return 0;
}


struct rsn_pmksa_cache_entry *
wpa_auth_sta_get_pmksa(struct wpa_state_machine *sm)
{
	return sm ? sm->pmksa : NULL;
}


void wpa_auth_sta_local_mic_failure_report(struct wpa_state_machine *sm)
{
	if (sm)
		sm->dot11RSNAStatsTKIPLocalMICFailures++;
}


const u8 * wpa_auth_get_wpa_ie(struct wpa_authenticator *wpa_auth, size_t *len)
{
	if (wpa_auth == NULL)
		return NULL;
	*len = wpa_auth->wpa_ie_len;
	return wpa_auth->wpa_ie;
}


int wpa_auth_pmksa_add(struct wpa_state_machine *sm, const u8 *pmk,
		       int session_timeout, struct eapol_state_machine *eapol)
{
	if (sm == NULL || sm->wpa != WPA_VERSION_WPA2)
		return -1;

	if (pmksa_cache_add(sm->wpa_auth->pmksa, pmk, WPA_PMK_LEN,
			    sm->wpa_auth->addr, sm->addr, session_timeout,
			    eapol))
		return 0;

	return -1;
}


int wpa_auth_pmksa_add_preauth(struct wpa_authenticator *wpa_auth,
			       const u8 *pmk, size_t len, const u8 *sta_addr,
			       int session_timeout,
			       struct eapol_state_machine *eapol)
{
	if (wpa_auth == NULL)
		return -1;

	if (pmksa_cache_add(wpa_auth->pmksa, pmk, len, wpa_auth->addr,
			    sta_addr, session_timeout, eapol))
		return 0;

	return -1;
}


static struct wpa_group *
wpa_auth_add_group(struct wpa_authenticator *wpa_auth, int vlan_id)
{
	struct wpa_group *group;

	if (wpa_auth == NULL || wpa_auth->group == NULL)
		return NULL;

	wpa_printf(MSG_DEBUG, "WPA: Add group state machine for VLAN-ID %d",
		   vlan_id);
	group = wpa_group_init(wpa_auth, vlan_id);
	if (group == NULL)
		return NULL;

	group->next = wpa_auth->group->next;
	wpa_auth->group->next = group;

	return group;
}


int wpa_auth_sta_set_vlan(struct wpa_state_machine *sm, int vlan_id)
{
	struct wpa_group *group;

	if (sm == NULL || sm->wpa_auth == NULL)
		return 0;

	group = sm->wpa_auth->group;
	while (group) {
		if (group->vlan_id == vlan_id)
			break;
		group = group->next;
	}

	if (group == NULL) {
		group = wpa_auth_add_group(sm->wpa_auth, vlan_id);
		if (group == NULL)
			return -1;
	}

	if (sm->group == group)
		return 0;

	wpa_printf(MSG_DEBUG, "WPA: Moving STA " MACSTR " to use group state "
		   "machine for VLAN ID %d", MAC2STR(sm->addr), vlan_id);

	if (sm->group && sm->group != group && sm->sta_counted) {
		sm->group->GNoStations--;
		sm->sta_counted = 0;
		wpa_printf(MSG_DEBUG, "WLA: Decreased GNoStations for the "
			   "previously used group state machine");
	}

	sm->group = group;
	return 0;
}

#endif /* CONFIG_NATIVE_WINDOWS */
