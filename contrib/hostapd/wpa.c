/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / WPA Authenticator
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
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "hostapd.h"
#include "eapol_sm.h"
#include "wpa.h"
#include "driver.h"
#include "sha1.h"
#include "md5.h"
#include "rc4.h"
#include "aes_wrap.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "eloop.h"
#include "sta_info.h"
#include "l2_packet.h"
#include "accounting.h"
#include "hostap_common.h"


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpa_sm_step(struct wpa_state_machine *sm);
static int wpa_verify_key_mic(struct wpa_ptk *PTK, u8 *data, size_t data_len);
static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx);
static void wpa_group_sm_step(struct hostapd_data *hapd);
static void pmksa_cache_free(struct hostapd_data *hapd);
static struct rsn_pmksa_cache * pmksa_cache_get(struct hostapd_data *hapd,
						u8 *spa, u8 *pmkid);


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
static const int pmksa_cache_max_entries = 1024;


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
 * GroupKey and STAKey require encryption, otherwise, encryption is optional.
 */
static const u8 RSN_KEY_DATA_GROUPKEY[] = { 0x00, 0x0f, 0xac, 1 };
static const u8 RSN_KEY_DATA_STAKEY[] = { 0x00, 0x0f, 0xac, 2 };
static const u8 RSN_KEY_DATA_MAC_ADDR[] = { 0x00, 0x0f, 0xac, 3 };
static const u8 RSN_KEY_DATA_PMKID[] = { 0x00, 0x0f, 0xac, 4 };

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
} __attribute__ ((packed));


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
 */

struct rsn_ie_hdr {
	u8 elem_id; /* WLAN_EID_RSN */
	u8 len;
	u16 version;
} __attribute__ ((packed));


static int wpa_write_wpa_ie(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	struct wpa_ie_hdr *hdr;
	int num_suites;
	u8 *pos, *count;

	hdr = (struct wpa_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_GENERIC;
	memcpy(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN);
	hdr->version = host_to_le16(WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	if (hapd->conf->wpa_group == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_WEP104) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_WEP40) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN);
	} else {
		printf("Invalid group cipher (%d).\n", hapd->conf->wpa_group);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (hapd->conf->wpa_pairwise & WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_pairwise & WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_pairwise & WPA_CIPHER_NONE) {
		memcpy(pos, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		printf("Invalid pairwise cipher (%d).\n",
		       hapd->conf->wpa_pairwise);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       WPA_SELECTOR_LEN);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		printf("Invalid key management type (%d).\n",
		       hapd->conf->wpa_key_mgmt);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


static int wpa_write_rsn_ie(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	struct rsn_ie_hdr *hdr;
	int num_suites;
	u8 *pos, *count;

	hdr = (struct rsn_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_RSN;
	pos = (u8 *) &hdr->version;
	*pos++ = RSN_VERSION & 0xff;
	*pos++ = RSN_VERSION >> 8;
	pos = (u8 *) (hdr + 1);

	if (hapd->conf->wpa_group == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_WEP104) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN);
	} else if (hapd->conf->wpa_group == WPA_CIPHER_WEP40) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN);
	} else {
		printf("Invalid group cipher (%d).\n", hapd->conf->wpa_group);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (hapd->conf->wpa_pairwise & WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_pairwise & WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_pairwise & WPA_CIPHER_NONE) {
		memcpy(pos, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		printf("Invalid pairwise cipher (%d).\n",
		       hapd->conf->wpa_pairwise);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	num_suites = 0;
	count = pos;
	pos += 2;

	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       RSN_SELECTOR_LEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		printf("Invalid key management type (%d).\n",
		       hapd->conf->wpa_key_mgmt);
		return -1;
	}
	*count++ = num_suites & 0xff;
	*count = (num_suites >> 8) & 0xff;

	/* RSN Capabilities */
	*pos++ = hapd->conf->rsn_preauth ? BIT(0) : 0;
	*pos++ = 0;

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


static int wpa_gen_wpa_ie(struct hostapd_data *hapd)
{
	u8 *pos, buf[100];
	int res;

	pos = buf;

	if (hapd->conf->wpa & HOSTAPD_WPA_VERSION_WPA2) {
		res = wpa_write_rsn_ie(hapd, pos, buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}
	if (hapd->conf->wpa & HOSTAPD_WPA_VERSION_WPA) {
		res = wpa_write_wpa_ie(hapd, pos, buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}

	free(hapd->wpa_ie);
	hapd->wpa_ie = malloc(pos - buf);
	if (hapd->wpa_ie == NULL)
		return -1;
	memcpy(hapd->wpa_ie, buf, pos - buf);
	hapd->wpa_ie_len = pos - buf;

	return 0;
}


static void wpa_sta_disconnect(struct hostapd_data *hapd, struct sta_info *sta)
{
	hostapd_sta_deauth(hapd, sta->addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC | WLAN_STA_AUTHORIZED);
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(0, 0, ap_handle_timer, hapd, sta);
	sta->timeout_next = STA_REMOVE;
}


static void wpa_rekey_gmk(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;

	if (hapd->wpa_auth) {
		if (hostapd_get_rand(hapd->wpa_auth->GMK, WPA_GMK_LEN)) {
			printf("Failed to get random data for WPA "
			       "initialization.\n");
		} else {
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG,
				       "GMK rekeyd");
		}
	}

	if (hapd->conf->wpa_gmk_rekey) {
		eloop_register_timeout(hapd->conf->wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, hapd, NULL);
	}
}


static void wpa_rekey_gtk(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;

	if (hapd->wpa_auth) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_DEBUG, "rekeying GTK");
		hapd->wpa_auth->GTKReKey = TRUE;
		do {
			hapd->wpa_auth->changed = FALSE;
			wpa_group_sm_step(hapd);
		} while (hapd->wpa_auth->changed);
	}
	if (hapd->conf->wpa_group_rekey) {
		eloop_register_timeout(hapd->conf->wpa_group_rekey, 0,
				       wpa_rekey_gtk, hapd, NULL);
	}
}


#ifdef CONFIG_RSN_PREAUTH

static void rsn_preauth_receive(void *ctx, const u8 *src_addr,
				const u8 *buf, size_t len)
{
	struct rsn_preauth_interface *piface = ctx;
	struct hostapd_data *hapd = piface->hapd;
	struct ieee802_1x_hdr *hdr;
	struct sta_info *sta;
	struct l2_ethhdr *ethhdr;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN: receive pre-auth packet "
		      "from interface '%s'\n", piface->ifname);
	if (len < sizeof(*ethhdr) + sizeof(*hdr)) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN: too short pre-auth "
			      "packet (len=%lu)\n", (unsigned long) len);
		return;
	}

	ethhdr = (struct l2_ethhdr *) buf;
	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);

	if (memcmp(ethhdr->h_dest, hapd->own_addr, ETH_ALEN) != 0) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN: pre-auth for "
			      "foreign address " MACSTR "\n",
			      MAC2STR(ethhdr->h_dest));
		return;
	}

	sta = ap_get_sta(hapd, ethhdr->h_source);
	if (sta && (sta->flags & WLAN_STA_ASSOC)) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN: pre-auth for "
			      "already association STA " MACSTR "\n",
			      MAC2STR(sta->addr));
		return;
	}
	if (!sta && hdr->type == IEEE802_1X_TYPE_EAPOL_START) {
		sta = (struct sta_info *) malloc(sizeof(struct sta_info));
		if (sta == NULL)
			return;
		memset(sta, 0, sizeof(*sta));
		memcpy(sta->addr, ethhdr->h_source, ETH_ALEN);
		sta->flags = WLAN_STA_PREAUTH;
		sta->next = hapd->sta_list;
		sta->wpa = WPA_VERSION_WPA2;
		hapd->sta_list = sta;
		hapd->num_sta++;
		ap_sta_hash_add(hapd, sta);

		ieee802_1x_new_station(hapd, sta);
		if (sta->eapol_sm == NULL) {
			ap_free_sta(hapd, sta);
			sta = NULL;
		} else {
			sta->eapol_sm->radius_identifier = -1;
			sta->eapol_sm->portValid = TRUE;
			sta->eapol_sm->flags |= EAPOL_SM_PREAUTH;
		}
	}
	if (sta == NULL)
		return;
	sta->preauth_iface = piface;
	ieee802_1x_receive(hapd, ethhdr->h_source, (u8 *) (ethhdr + 1),
			   len - sizeof(*ethhdr));
}


static int rsn_preauth_iface_add(struct hostapd_data *hapd, const char *ifname)
{
	struct rsn_preauth_interface *piface;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN pre-auth interface '%s'\n",
		      ifname);

	piface = malloc(sizeof(*piface));
	if (piface == NULL)
		return -1;
	memset(piface, 0, sizeof(*piface));
	piface->hapd = hapd;

	piface->ifname = strdup(ifname);
	if (piface->ifname == NULL) {
		goto fail1;
	}

	piface->l2 = l2_packet_init(piface->ifname, NULL, ETH_P_PREAUTH,
				    rsn_preauth_receive, piface, 1);
	if (piface->l2 == NULL) {
		printf("Failed to open register layer 2 access to "
		       "ETH_P_PREAUTH\n");
		goto fail2;
	}

	piface->next = hapd->preauth_iface;
	hapd->preauth_iface = piface;
	return 0;

fail2:
	free(piface->ifname);
fail1:
	free(piface);
	return -1;
}


static void rsn_preauth_iface_deinit(struct hostapd_data *hapd)
{
	struct rsn_preauth_interface *piface, *prev;

	piface = hapd->preauth_iface;
	hapd->preauth_iface = NULL;
	while (piface) {
		prev = piface;
		piface = piface->next;
		l2_packet_deinit(prev->l2);
		free(prev->ifname);
		free(prev);
	}
}


static int rsn_preauth_iface_init(struct hostapd_data *hapd)
{
	char *tmp, *start, *end;

	if (hapd->conf->rsn_preauth_interfaces == NULL)
		return 0;

	tmp = strdup(hapd->conf->rsn_preauth_interfaces);
	if (tmp == NULL)
		return -1;
	start = tmp;
	for (;;) {
		while (*start == ' ')
			start++;
		if (*start == '\0')
			break;
		end = strchr(start, ' ');
		if (end)
			*end = '\0';

		if (rsn_preauth_iface_add(hapd, start)) {
			rsn_preauth_iface_deinit(hapd);
			return -1;
		}

		if (end)
			start = end + 1;
		else
			break;
	}
	free(tmp);
	return 0;
}


static void rsn_preauth_finished_cb(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	wpa_printf(MSG_DEBUG, "RSN: Removing pre-authentication STA entry for "
		   MACSTR, MAC2STR(sta->addr));
	ap_free_sta(hapd, sta);
}


void rsn_preauth_finished(struct hostapd_data *hapd, struct sta_info *sta,
			  int success)
{
	u8 *key;
	size_t len;
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_INFO, "pre-authentication %s",
		       success ? "succeeded" : "failed");

	key = ieee802_1x_get_key_crypt(sta->eapol_sm, &len);
	if (success && key) {
		pmksa_cache_add(hapd, sta, key, dot11RSNAConfigPMKLifetime);
	}

	/*
	 * Finish STA entry removal from timeout in order to avoid freeing
	 * STA data before the caller has finished processing.
	 */
	eloop_register_timeout(0, 0, rsn_preauth_finished_cb, hapd, sta);
}


void rsn_preauth_send(struct hostapd_data *hapd, struct sta_info *sta,
		      u8 *buf, size_t len)
{
	struct rsn_preauth_interface *piface;
	struct l2_ethhdr *ethhdr;

	piface = hapd->preauth_iface;
	while (piface) {
		if (piface == sta->preauth_iface)
			break;
		piface = piface->next;
	}

	if (piface == NULL) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "RSN: Could not find "
			      "pre-authentication interface for " MACSTR "\n",
			      MAC2STR(sta->addr));
		return;
	}

	ethhdr = malloc(sizeof(*ethhdr) + len);
	if (ethhdr == NULL)
		return;

	memcpy(ethhdr->h_dest, sta->addr, ETH_ALEN);
	memcpy(ethhdr->h_source, hapd->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_PREAUTH);
	memcpy(ethhdr + 1, buf, len);

	if (l2_packet_send(piface->l2, sta->addr, ETH_P_PREAUTH, (u8 *) ethhdr,
			   sizeof(*ethhdr) + len) < 0) {
		printf("Failed to send preauth packet using l2_packet_send\n");
	}
	free(ethhdr);
}

#else /* CONFIG_RSN_PREAUTH */

static inline int rsn_preauth_iface_init(struct hostapd_data *hapd)
{
	return 0;
}

static inline void rsn_preauth_iface_deinit(struct hostapd_data *hapd)
{
}

static void rsn_preauth_finished_cb(void *eloop_ctx, void *timeout_ctx)
{
}

void rsn_preauth_finished(struct hostapd_data *hapd, struct sta_info *sta,
			  int success)
{
}

void rsn_preauth_send(struct hostapd_data *hapd, struct sta_info *sta,
		      u8 *buf, size_t len)
{
}

#endif /* CONFIG_RSN_PREAUTH */


int wpa_init(struct hostapd_data *hapd)
{
	u8 rkey[32];
	u8 buf[ETH_ALEN + 8];

	if (rsn_preauth_iface_init(hapd))
		return -1;

	if (hostapd_set_privacy(hapd, 1)) {
		printf("Could not set PrivacyInvoked for interface %s\n",
		       hapd->conf->iface);
		return -1;
	}

	if (wpa_gen_wpa_ie(hapd)) {
		printf("Could not generate WPA IE.\n");
		return -1;
	}

	if (hostapd_set_generic_elem(hapd, hapd->wpa_ie, hapd->wpa_ie_len)) {
		printf("Failed to configure WPA IE for the kernel driver.\n");
		return -1;
	}

	hapd->wpa_auth = malloc(sizeof(struct wpa_authenticator));
	if (hapd->wpa_auth == NULL)
		return -1;
	memset(hapd->wpa_auth, 0, sizeof(struct wpa_authenticator));
	hapd->wpa_auth->GTKAuthenticator = TRUE;
	switch (hapd->conf->wpa_group) {
	case WPA_CIPHER_CCMP:
		hapd->wpa_auth->GTK_len = 16;
		break;
	case WPA_CIPHER_TKIP:
		hapd->wpa_auth->GTK_len = 32;
		break;
	case WPA_CIPHER_WEP104:
		hapd->wpa_auth->GTK_len = 13;
		break;
	case WPA_CIPHER_WEP40:
		hapd->wpa_auth->GTK_len = 5;
		break;
	}

	/* Counter = PRF-256(Random number, "Init Counter",
	 *                   Local MAC Address || Time)
	 */
	memcpy(buf, hapd->own_addr, ETH_ALEN);
	hostapd_get_ntp_timestamp(buf + ETH_ALEN);
	if (hostapd_get_rand(rkey, sizeof(rkey)) ||
	    hostapd_get_rand(hapd->wpa_auth->GMK, WPA_GMK_LEN)) {
		printf("Failed to get random data for WPA initialization.\n");
		free(hapd->wpa_auth);
		hapd->wpa_auth = NULL;
		return -1;
	}

	sha1_prf(rkey, sizeof(rkey), "Init Counter", buf, sizeof(buf),
		 hapd->wpa_auth->Counter, WPA_NONCE_LEN);

	if (hapd->conf->wpa_gmk_rekey) {
		eloop_register_timeout(hapd->conf->wpa_gmk_rekey, 0,
				       wpa_rekey_gmk, hapd, NULL);
	}

	if (hapd->conf->wpa_group_rekey) {
		eloop_register_timeout(hapd->conf->wpa_group_rekey, 0,
				       wpa_rekey_gtk, hapd, NULL);
	}

	hapd->wpa_auth->GInit = TRUE;
	wpa_group_sm_step(hapd);
	hapd->wpa_auth->GInit = FALSE;
	wpa_group_sm_step(hapd);

	return 0;
}


void wpa_deinit(struct hostapd_data *hapd)
{
	rsn_preauth_iface_deinit(hapd);

	eloop_cancel_timeout(wpa_rekey_gmk, hapd, NULL);
	eloop_cancel_timeout(wpa_rekey_gtk, hapd, NULL);

	if (hostapd_set_privacy(hapd, 0)) {
		printf("Could not disable PrivacyInvoked for interface %s\n",
		       hapd->conf->iface);
	}

	if (hostapd_set_generic_elem(hapd, (u8 *) "", 0)) {
		printf("Could not remove generic information element from "
		       "interface %s\n", hapd->conf->iface);
	}

	free(hapd->wpa_ie);
	hapd->wpa_ie = NULL;
	free(hapd->wpa_auth);
	hapd->wpa_auth = NULL;

	pmksa_cache_free(hapd);
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


static void rsn_pmkid(const u8 *pmk, const u8 *aa, const u8 *spa, u8 *pmkid)
{
	char *title = "PMK Name";
	const u8 *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA1_MAC_LEN];

	addr[0] = (u8 *) title;
	addr[1] = aa;
	addr[2] = spa;

	hmac_sha1_vector(pmk, PMK_LEN, 3, addr, len, hash);
	memcpy(pmkid, hash, PMKID_LEN);
}


static void pmksa_cache_set_expiration(struct hostapd_data *hapd);


static void _pmksa_cache_free_entry(struct rsn_pmksa_cache *entry)
{
	if (entry == NULL)
		return;
	free(entry->identity);
	ieee802_1x_free_radius_class(&entry->radius_class);
	free(entry);
}


static void pmksa_cache_free_entry(struct hostapd_data *hapd,
				   struct rsn_pmksa_cache *entry)
{
	struct sta_info *sta;
	struct rsn_pmksa_cache *pos, *prev;
	hapd->pmksa_count--;
	for (sta = hapd->sta_list; sta != NULL; sta = sta->next) {
		if (sta->pmksa == entry)
			sta->pmksa = NULL;
	}
	pos = hapd->pmkid[PMKID_HASH(entry->pmkid)];
	prev = NULL;
	while (pos) {
		if (pos == entry) {
			if (prev != NULL) {
				prev->hnext = pos->hnext;
			} else {
				hapd->pmkid[PMKID_HASH(entry->pmkid)] =
					pos->hnext;
			}
			break;
		}
		prev = pos;
		pos = pos->hnext;
	}

	pos = hapd->pmksa;
	prev = NULL;
	while (pos) {
		if (pos == entry) {
			if (prev != NULL)
				prev->next = pos->next;
			else
				hapd->pmksa = pos->next;
			break;
		}
		prev = pos;
		pos = pos->next;
	}
	_pmksa_cache_free_entry(entry);
}


static void pmksa_cache_expire(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	time_t now;

	time(&now);
	while (hapd->pmksa && hapd->pmksa->expiration <= now) {
		struct rsn_pmksa_cache *entry = hapd->pmksa;
		hapd->pmksa = entry->next;
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "RSN: expired PMKSA cache entry for "
			      MACSTR, MAC2STR(entry->spa));
		pmksa_cache_free_entry(hapd, entry);
	}

	pmksa_cache_set_expiration(hapd);
}


static void pmksa_cache_set_expiration(struct hostapd_data *hapd)
{
	int sec;
	eloop_cancel_timeout(pmksa_cache_expire, hapd, NULL);
	if (hapd->pmksa == NULL)
		return;
	sec = hapd->pmksa->expiration - time(NULL);
	if (sec < 0)
		sec = 0;
	eloop_register_timeout(sec + 1, 0, pmksa_cache_expire, hapd, NULL);
}


static void pmksa_cache_from_eapol_data(struct rsn_pmksa_cache *entry,
					struct eapol_state_machine *eapol)
{
	if (eapol == NULL)
		return;

	if (eapol->identity) {
		entry->identity = malloc(eapol->identity_len);
		if (entry->identity) {
			entry->identity_len = eapol->identity_len;
			memcpy(entry->identity, eapol->identity,
			       eapol->identity_len);
		}
	}

	ieee802_1x_copy_radius_class(&entry->radius_class,
				     &eapol->radius_class);
}


static void pmksa_cache_to_eapol_data(struct rsn_pmksa_cache *entry,
				      struct eapol_state_machine *eapol)
{
	if (entry == NULL || eapol == NULL)
		return;

	if (entry->identity) {
		free(eapol->identity);
		eapol->identity = malloc(entry->identity_len);
		if (eapol->identity) {
			eapol->identity_len = entry->identity_len;
			memcpy(eapol->identity, entry->identity,
			       entry->identity_len);
		}
		wpa_hexdump_ascii(MSG_DEBUG, "STA identity from PMKSA",
				  eapol->identity, eapol->identity_len);
	}

	ieee802_1x_free_radius_class(&eapol->radius_class);
	ieee802_1x_copy_radius_class(&eapol->radius_class,
				     &entry->radius_class);
	if (eapol->radius_class.attr) {
		wpa_printf(MSG_DEBUG, "Copied %lu Class attribute(s) from "
			   "PMKSA", (unsigned long) eapol->radius_class.count);
	}
}


void pmksa_cache_add(struct hostapd_data *hapd, struct sta_info *sta, u8 *pmk,
		     int session_timeout)
{
	struct rsn_pmksa_cache *entry, *pos, *prev;

	if (sta->wpa != WPA_VERSION_WPA2)
		return;

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return;
	memset(entry, 0, sizeof(*entry));
	memcpy(entry->pmk, pmk, PMK_LEN);
	rsn_pmkid(pmk, hapd->own_addr, sta->addr, entry->pmkid);
	time(&entry->expiration);
	if (session_timeout > 0)
		entry->expiration += session_timeout;
	else
		entry->expiration += dot11RSNAConfigPMKLifetime;
	entry->akmp = WPA_KEY_MGMT_IEEE8021X;
	memcpy(entry->spa, sta->addr, ETH_ALEN);
	pmksa_cache_from_eapol_data(entry, sta->eapol_sm);

	/* Replace an old entry for the same STA (if found) with the new entry
	 */
	pos = pmksa_cache_get(hapd, sta->addr, NULL);
	if (pos)
		pmksa_cache_free_entry(hapd, pos);

	if (hapd->pmksa_count >= pmksa_cache_max_entries && hapd->pmksa) {
		/* Remove the oldest entry to make room for the new entry */
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "RSN: removed the oldest PMKSA cache entry (for "
			      MACSTR ") to make room for new one",
			      MAC2STR(hapd->pmksa->spa));
		pmksa_cache_free_entry(hapd, hapd->pmksa);
	}

	/* Add the new entry; order by expiration time */
	pos = hapd->pmksa;
	prev = NULL;
	while (pos) {
		if (pos->expiration > entry->expiration)
			break;
		prev = pos;
		pos = pos->next;
	}
	if (prev == NULL) {
		entry->next = hapd->pmksa;
		hapd->pmksa = entry;
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	entry->hnext = hapd->pmkid[PMKID_HASH(entry->pmkid)];
	hapd->pmkid[PMKID_HASH(entry->pmkid)] = entry;

	hapd->pmksa_count++;
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG,
		       "added PMKSA cache entry");
	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
		hostapd_hexdump("RSN: added PMKID", entry->pmkid, PMKID_LEN);
	}
}


static void pmksa_cache_free(struct hostapd_data *hapd)
{
	struct rsn_pmksa_cache *entry, *prev;
	int i;
	struct sta_info *sta;

	entry = hapd->pmksa;
	hapd->pmksa = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		_pmksa_cache_free_entry(prev);
	}
	eloop_cancel_timeout(pmksa_cache_expire, hapd, NULL);
	for (i = 0; i < PMKID_HASH_SIZE; i++)
		hapd->pmkid[i] = NULL;
	for (sta = hapd->sta_list; sta; sta = sta->next)
		sta->pmksa = NULL;
}


static struct rsn_pmksa_cache * pmksa_cache_get(struct hostapd_data *hapd,
						u8 *spa, u8 *pmkid)
{
	struct rsn_pmksa_cache *entry;

	if (pmkid)
		entry = hapd->pmkid[PMKID_HASH(pmkid)];
	else
		entry = hapd->pmksa;
	while (entry) {
		if ((spa == NULL || memcmp(entry->spa, spa, ETH_ALEN) == 0) &&
		    (pmkid == NULL ||
		     memcmp(entry->pmkid, pmkid, PMKID_LEN) == 0))
			return entry;
		entry = pmkid ? entry->hnext : entry->next;
	}
	return NULL;
}


struct wpa_ie_data {
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int capabilities;
	size_t num_pmkid;
	u8 *pmkid;
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
		if (left < data->num_pmkid * PMKID_LEN) {
			printf("RSN: too short RSN IE for PMKIDs "
			       "(num=%lu, left=%d)\n",
			       (unsigned long) data->num_pmkid, left);
			return -9;
		}
		data->pmkid = pos;
		pos += data->num_pmkid * PMKID_LEN;
		left -= data->num_pmkid * PMKID_LEN;
	}

	if (left > 0) {
		return -8;
	}

	return 0;
}


int wpa_validate_wpa_ie(struct hostapd_data *hapd, struct sta_info *sta,
			const u8 *wpa_ie, size_t wpa_ie_len, int version)
{
	struct wpa_ie_data data;
	int ciphers, key_mgmt, res, i;
	const u8 *selector;

	if (version == HOSTAPD_WPA_VERSION_WPA2) {
		res = wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, &data);

		selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
		memcpy(hapd->wpa_auth->dot11RSNAAuthenticationSuiteSelected,
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
		memcpy(hapd->wpa_auth->dot11RSNAPairwiseCipherSelected,
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
		memcpy(hapd->wpa_auth->dot11RSNAGroupCipherSelected,
		       selector, RSN_SELECTOR_LEN);
	} else {
		res = wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, &data);

		selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X;
		memcpy(hapd->wpa_auth->dot11RSNAAuthenticationSuiteSelected,
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
		memcpy(hapd->wpa_auth->dot11RSNAPairwiseCipherSelected,
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
		memcpy(hapd->wpa_auth->dot11RSNAGroupCipherSelected,
		       selector, WPA_SELECTOR_LEN);
	}
	if (res) {
		printf("Failed to parse WPA/RSN IE from " MACSTR " (res=%d)\n",
		       MAC2STR(sta->addr), res);
		hostapd_hexdump("WPA/RSN IE", wpa_ie, wpa_ie_len);
		return WPA_INVALID_IE;
	}

	if (data.group_cipher != hapd->conf->wpa_group) {
		printf("Invalid WPA group cipher (0x%x) from " MACSTR "\n",
		       data.group_cipher, MAC2STR(sta->addr));
		return WPA_INVALID_GROUP;
	}

	key_mgmt = data.key_mgmt & hapd->conf->wpa_key_mgmt;
	if (!key_mgmt) {
		printf("Invalid WPA key mgmt (0x%x) from " MACSTR "\n",
		       data.key_mgmt, MAC2STR(sta->addr));
		return WPA_INVALID_AKMP;
	}
	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X)
		sta->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	else
		sta->wpa_key_mgmt = WPA_KEY_MGMT_PSK;

	ciphers = data.pairwise_cipher & hapd->conf->wpa_pairwise;
	if (!ciphers) {
		printf("Invalid WPA pairwise cipher (0x%x) from " MACSTR "\n",
		       data.pairwise_cipher, MAC2STR(sta->addr));
		return WPA_INVALID_PAIRWISE;
	}

	if (ciphers & WPA_CIPHER_CCMP)
		sta->pairwise = WPA_CIPHER_CCMP;
	else
		sta->pairwise = WPA_CIPHER_TKIP;

	/* TODO: clear WPA/WPA2 state if STA changes from one to another */
	if (wpa_ie[0] == WLAN_EID_RSN)
		sta->wpa = WPA_VERSION_WPA2;
	else
		sta->wpa = WPA_VERSION_WPA;

	for (i = 0; i < data.num_pmkid; i++) {
		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
			hostapd_hexdump("RSN IE: STA PMKID",
					&data.pmkid[i * PMKID_LEN], PMKID_LEN);
		}
		sta->pmksa = pmksa_cache_get(hapd, sta->addr,
					     &data.pmkid[i * PMKID_LEN]);
		if (sta->pmksa) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG,
				       "PMKID found from PMKSA cache");
			if (hapd->wpa_auth) {
				memcpy(hapd->wpa_auth->dot11RSNAPMKIDUsed,
				       sta->pmksa->pmkid, PMKID_LEN);
			}
			break;
		}
	}

	return WPA_IE_OK;
}


void wpa_new_station(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct wpa_state_machine *sm;

	if (!hapd->conf->wpa)
		return;

	if (sta->wpa_sm) {
		sm = sta->wpa_sm;
		memset(sm->key_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
		sm->ReAuthenticationRequest = TRUE;
		wpa_sm_step(sm);
		return;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG, "start authentication");
	sm = malloc(sizeof(struct wpa_state_machine));
	if (sm == NULL)
		return;
	memset(sm, 0, sizeof(struct wpa_state_machine));

	sm->hapd = hapd;
	sm->sta = sta;
	sta->wpa_sm = sm;

	sm->Init = TRUE;
	wpa_sm_step(sm);
	sm->Init = FALSE;
	sm->AuthenticationRequest = TRUE;
	wpa_sm_step(sm);
}


void wpa_free_station(struct sta_info *sta)
{
	struct wpa_state_machine *sm = sta->wpa_sm;

	if (sm == NULL)
		return;

	if (sm->hapd->conf->wpa_strict_rekey && sm->has_GTK) {
		hostapd_logger(sm->hapd, sta->addr, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_DEBUG, "strict rekeying - force "
			       "GTK rekey since STA is leaving");
		eloop_cancel_timeout(wpa_rekey_gtk, sm->hapd, NULL);
		eloop_register_timeout(0, 500000, wpa_rekey_gtk, sm->hapd,
				       NULL);
	}

	eloop_cancel_timeout(wpa_send_eapol_timeout, sm->hapd, sta);
	eloop_cancel_timeout(wpa_sm_call_step, sm->hapd, sta->wpa_sm);
	eloop_cancel_timeout(rsn_preauth_finished_cb, sm->hapd, sta);
	free(sm->last_rx_eapol_key);
	free(sm);
	sta->wpa_sm = NULL;
}


static void wpa_request_new_ptk(struct hostapd_data *hapd,
				struct sta_info *sta)
{
	struct wpa_state_machine *sm = sta->wpa_sm;

	if (sm == NULL)
		return;

	sm->PTKRequest = TRUE;
	sm->PTK_valid = 0;
}


void wpa_receive(struct hostapd_data *hapd, struct sta_info *sta,
		 u8 *data, size_t data_len)
{
	struct wpa_state_machine *sm = sta->wpa_sm;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info, key_data_length;
	enum { PAIRWISE_2, PAIRWISE_4, GROUP_2, REQUEST } msg;
	char *msgtxt;

	if (!hapd->conf->wpa)
		return;

	if (sm == NULL)
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

	if (key_info & WPA_KEY_INFO_REQUEST) {
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
		if (sta->req_replay_counter_used &&
		    memcmp(key->replay_counter, sta->req_replay_counter,
			   WPA_REPLAY_COUNTER_LEN) <= 0) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_WARNING,
				       "received EAPOL-Key request with "
				       "replayed counter");
			return;
		}
	}

	if (!(key_info & WPA_KEY_INFO_REQUEST) &&
	    (!sm->key_replay_counter_valid ||
	     memcmp(key->replay_counter, sm->key_replay_counter,
		    WPA_REPLAY_COUNTER_LEN) != 0)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_INFO,
			       "received EAPOL-Key %s with unexpected replay "
			       "counter", msgtxt);
		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
			hostapd_hexdump("expected replay counter",
					sm->key_replay_counter,
					WPA_REPLAY_COUNTER_LEN);
			hostapd_hexdump("received replay counter",
					key->replay_counter,
					WPA_REPLAY_COUNTER_LEN);
		}
		return;
	}

	switch (msg) {
	case PAIRWISE_2:
		if (sm->wpa_ptk_state != WPA_PTK_PTKSTART &&
		    sm->wpa_ptk_state != WPA_PTK_PTKCALCNEGOTIATING) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key msg 2/4 in invalid"
				       " state (%d) - dropped",
				       sm->wpa_ptk_state);
			return;
		}
		if (sta->wpa_ie == NULL ||
		    sta->wpa_ie_len != key_data_length ||
		    memcmp(sta->wpa_ie, key + 1, key_data_length) != 0) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "WPA IE from (Re)AssocReq did not match"
				       " with msg 2/4");
			if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
				if (sta->wpa_ie) {
					hostapd_hexdump("WPA IE in AssocReq",
							sta->wpa_ie,
							sta->wpa_ie_len);
				}
				hostapd_hexdump("WPA IE in msg 2/4",
						(u8 *) (key + 1),
						key_data_length);
			}
			/* MLME-DEAUTHENTICATE.request */
			wpa_sta_disconnect(hapd, sta);
			return;
		}
		break;
	case PAIRWISE_4:
		if (sm->wpa_ptk_state != WPA_PTK_PTKINITNEGOTIATING ||
		    !sm->PTK_valid) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key msg 4/4 in invalid"
				       " state (%d) - dropped",
				       sm->wpa_ptk_state);
			return;
		}
		break;
	case GROUP_2:
		if (sm->wpa_ptk_group_state != WPA_PTK_GROUP_REKEYNEGOTIATING
		    || !sm->PTK_valid) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key msg 2/2 in invalid"
				       " state (%d) - dropped",
				       sm->wpa_ptk_group_state);
			return;
		}
		break;
	case REQUEST:
		break;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG, "received EAPOL-Key frame (%s)",
		       msgtxt);

	if (key_info & WPA_KEY_INFO_ACK) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_INFO,
			       "received invalid EAPOL-Key: Key Ack set");
		return;
	}

	if (!(key_info & WPA_KEY_INFO_MIC)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_INFO,
			       "received invalid EAPOL-Key: Key MIC not set");
		return;
	}

	sm->MICVerified = FALSE;
	if (sm->PTK_valid) {
		if (wpa_verify_key_mic(&sm->PTK, data, data_len)) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key with invalid MIC");
			return;
		}
		sm->MICVerified = TRUE;
		eloop_cancel_timeout(wpa_send_eapol_timeout, sta->wpa_sm->hapd,
				     sta);
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		if (sm->MICVerified) {
			sta->req_replay_counter_used = 1;
			memcpy(sta->req_replay_counter, key->replay_counter,
			       WPA_REPLAY_COUNTER_LEN);
		} else {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key request with "
				       "invalid MIC");
			return;
		}

		if (key_info & WPA_KEY_INFO_ERROR) {
			/* Supplicant reported a Michael MIC error */
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key Error Request "
				       "(STA detected Michael MIC failure)");
			ieee80211_michael_mic_failure(hapd, sta->addr, 0);
			sta->dot11RSNAStatsTKIPRemoteMICFailures++;
			hapd->wpa_auth->dot11RSNAStatsTKIPRemoteMICFailures++;
			/* Error report is not a request for a new key
			 * handshake, but since Authenticator may do it, let's
			 * change the keys now anyway. */
			wpa_request_new_ptk(hapd, sta);
		} else if (key_info & WPA_KEY_INFO_KEY_TYPE) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key Request for new "
				       "4-Way Handshake");
			wpa_request_new_ptk(hapd, sta);
		} else {
			/* TODO: this could also be a request for STAKey
			 * if Key Data fields contains peer MAC address KDE.
			 * STAKey request should have 0xdd <len> 00-0F-AC:2 in
			 * the beginning of Key Data */
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "received EAPOL-Key Request for GTK "
				       "rekeying");
			wpa_request_new_ptk(hapd, sta);
			eloop_cancel_timeout(wpa_rekey_gtk, hapd, NULL);
			wpa_rekey_gtk(hapd, NULL);
		}
	} else {
		/* Do not allow the same key replay counter to be reused. */
		sm->key_replay_counter_valid = FALSE;
	}

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


static void wpa_pmk_to_ptk(struct hostapd_data *hapd, const u8 *pmk,
			   const u8 *addr1, const u8 *addr2,
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

	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
		hostapd_hexdump("PMK", pmk, WPA_PMK_LEN);
		hostapd_hexdump("PTK", ptk, ptk_len);
	}
}


static void wpa_gmk_to_gtk(struct hostapd_data *hapd, u8 *gmk,
			   u8 *addr, u8 *gnonce, u8 *gtk, size_t gtk_len)
{
	u8 data[ETH_ALEN + WPA_NONCE_LEN];

	/* GTK = PRF-X(GMK, "Group key expansion", AA || GNonce) */
	memcpy(data, addr, ETH_ALEN);
	memcpy(data + ETH_ALEN, gnonce, WPA_NONCE_LEN);

	sha1_prf(gmk, WPA_GMK_LEN, "Group key expansion",
		 data, sizeof(data), gtk, gtk_len);

	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
		hostapd_hexdump("GMK", gmk, WPA_GMK_LEN);
		hostapd_hexdump("GTK", gtk, gtk_len);
	}
}


static void wpa_send_eapol_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	if (!sta->wpa_sm || !(sta->flags & WLAN_STA_ASSOC))
		return;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG, "EAPOL-Key timeout");
	sta->wpa_sm->TimeoutEvt = TRUE;
	wpa_sm_step(sta->wpa_sm);
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


static void wpa_send_eapol(struct hostapd_data *hapd, struct sta_info *sta,
			   int secure, int mic, int ack, int install,
			   int pairwise, u8 *key_rsc, u8 *nonce,
			   u8 *ie, size_t ie_len, u8 *gtk, size_t gtk_len,
			   int keyidx)
{
	struct wpa_state_machine *sm = sta->wpa_sm;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	size_t len;
	int key_info, alg;
	int timeout_ms;
	int key_data_len, pad_len = 0;
	u8 *buf, *pos;

	if (sm == NULL)
		return;

	len = sizeof(struct ieee802_1x_hdr) + sizeof(struct wpa_eapol_key);

	if (sta->wpa == WPA_VERSION_WPA2) {
		key_data_len = ie_len + gtk_len;
		if (gtk_len)
			key_data_len += 2 + RSN_SELECTOR_LEN + 2;
	} else {
		if (pairwise) {
			/* WPA does not include GTK in 4-Way Handshake */
			gtk = NULL;
			gtk_len = 0;

			/* key_rsc is for group key, so mask it out in case of
			 * WPA Pairwise key negotiation. */
			key_rsc = NULL;
		}
		key_data_len = ie_len + gtk_len;
	}

	if (sta->pairwise == WPA_CIPHER_CCMP) {
		key_info = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
		if (gtk) {
			pad_len = key_data_len % 8;
			if (pad_len)
				pad_len = 8 - pad_len;
			key_data_len += pad_len + 8;
		}
	} else {
		key_info = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;
	}

	len += key_data_len;

	hdr = malloc(len);
	if (hdr == NULL)
		return;
	memset(hdr, 0, len);
	hdr->version = hapd->conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(len  - sizeof(*hdr));
	key = (struct wpa_eapol_key *) (hdr + 1);

	key->type = sta->wpa == WPA_VERSION_WPA2 ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	if (secure)
		key_info |= WPA_KEY_INFO_SECURE;
	if (mic)
		key_info |= WPA_KEY_INFO_MIC;
	if (ack)
		key_info |= WPA_KEY_INFO_ACK;
	if (install)
		key_info |= WPA_KEY_INFO_INSTALL;
	if (pairwise)
		key_info |= WPA_KEY_INFO_KEY_TYPE;
	if (gtk && sta->wpa == WPA_VERSION_WPA2)
		key_info |= WPA_KEY_INFO_ENCR_KEY_DATA;
	if (sta->wpa != WPA_VERSION_WPA2) {
		if (pairwise)
			keyidx = 0;
		key_info |= keyidx << WPA_KEY_INFO_KEY_INDEX_SHIFT;
	}
	key->key_info = htons(key_info);

	alg = pairwise ? sta->pairwise : hapd->conf->wpa_group;
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

	inc_byte_array(sm->key_replay_counter, WPA_REPLAY_COUNTER_LEN);
	memcpy(key->replay_counter, sm->key_replay_counter,
	       WPA_REPLAY_COUNTER_LEN);
	sm->key_replay_counter_valid = TRUE;

	if (nonce)
		memcpy(key->key_nonce, nonce, WPA_NONCE_LEN);

	if (key_rsc)
		memcpy(key->key_rsc, key_rsc, WPA_KEY_RSC_LEN);

	if (ie && !gtk) {
		memcpy(key + 1, ie, ie_len);
		key->key_data_length = htons(ie_len);
	} else if (gtk) {
		buf = malloc(key_data_len);
		if (buf == NULL) {
			free(hdr);
			return;
		}
		memset(buf, 0, key_data_len);
		pos = buf;
		if (ie) {
			memcpy(pos, ie, ie_len);
			pos += ie_len;
		}
		if (sta->wpa == WPA_VERSION_WPA2) {
			*pos++ = WLAN_EID_GENERIC;
			*pos++ = RSN_SELECTOR_LEN + 2 + gtk_len;
			memcpy(pos, RSN_KEY_DATA_GROUPKEY, RSN_SELECTOR_LEN);
			pos += RSN_SELECTOR_LEN;
			*pos++ = keyidx & 0x03;
			*pos++ = 0;
		}
		memcpy(pos, gtk, gtk_len);
		pos += gtk_len;
		if (pad_len)
			*pos++ = 0xdd;

		if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MINIMAL)) {
			hostapd_hexdump("Plaintext EAPOL-Key Key Data",
					buf, key_data_len);
		}
		if (key_info & WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
			aes_wrap(sm->PTK.encr_key, (key_data_len - 8) / 8, buf,
				 (u8 *) (key + 1));
			key->key_data_length = htons(key_data_len);
		} else {
			u8 ek[32];
			memcpy(key->key_iv,
			       hapd->wpa_auth->Counter + WPA_NONCE_LEN - 16,
			       16);
			inc_byte_array(hapd->wpa_auth->Counter, WPA_NONCE_LEN);
			memcpy(ek, key->key_iv, 16);
			memcpy(ek + 16, sm->PTK.encr_key, 16);
			memcpy(key + 1, buf, key_data_len);
			rc4_skip(ek, 32, 256, (u8 *) (key + 1), key_data_len);
			key->key_data_length = htons(key_data_len);
		}
		free(buf);
	}

	if (mic) {
		if (!sm->PTK_valid) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG, "PTK not valid "
				       "when sending EAPOL-Key frame");
			free(hdr);
			return;
		}
		wpa_calc_eapol_key_mic(key_info & WPA_KEY_INFO_TYPE_MASK,
				       sm->PTK.mic_key, (u8 *) hdr, len,
				       key->key_mic);
	}

	if (sta->eapol_sm)
		sta->eapol_sm->dot1xAuthEapolFramesTx++;
	hostapd_send_eapol(hapd, sta->addr, (u8 *) hdr, len, sm->pairwise_set);
	free(hdr);

	timeout_ms = pairwise ? dot11RSNAConfigPairwiseUpdateTimeOut :
		dot11RSNAConfigGroupUpdateTimeOut;
	eloop_register_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000,
			       wpa_send_eapol_timeout, hapd, sta);
}


static int wpa_verify_key_mic(struct wpa_ptk *PTK, u8 *data, size_t data_len)
{
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info;
	int type, ret = 0;
	u8 mic[16];

	if (data_len < sizeof(*hdr) + sizeof(*key))
		return -1;

	hdr = (struct ieee802_1x_hdr *) data;
	key = (struct wpa_eapol_key *) (hdr + 1);
	key_info = ntohs(key->key_info);
	type = key_info & WPA_KEY_INFO_TYPE_MASK;
	memcpy(mic, key->key_mic, 16);
	memset(key->key_mic, 0, 16);
	if (wpa_calc_eapol_key_mic(key_info & WPA_KEY_INFO_TYPE_MASK,
				   PTK->mic_key, data, data_len, key->key_mic)
	    || memcmp(mic, key->key_mic, 16) != 0)
		ret = -1;
	memcpy(key->key_mic, mic, 16);
	return ret;
}


void wpa_sm_event(struct hostapd_data *hapd, struct sta_info *sta,
		  wpa_event event)
{
	struct wpa_state_machine *sm = sta->wpa_sm;
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG,
		       "event %d notification", event);
	if (sm == NULL)
		return;

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

	if (event != WPA_REAUTH_EAPOL) {
		sm->pairwise_set = FALSE;
		hostapd_set_encryption(sm->hapd, "none", sm->sta->addr, 0,
				       (u8 *) "", 0);
	}

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


/* Definitions for clarifying state machine implementation */
#define SM_STATE(machine, state) \
static void sm_ ## machine ## _ ## state ## _Enter(struct wpa_state_machine \
*sm)

#define SM_ENTRY(machine, _state, _data) \
sm->changed = TRUE; \
sm->_data ## _ ## state = machine ## _ ## _state; \
if (sm->hapd->conf->debug >= HOSTAPD_DEBUG_MINIMAL) \
	printf("WPA: " MACSTR " " #machine " entering state " #_state \
		"\n", MAC2STR(sm->sta->addr));

#define SM_ENTER(machine, state) sm_ ## machine ## _ ## state ## _Enter(sm)

#define SM_STEP(machine) \
static void sm_ ## machine ## _Step(struct wpa_state_machine *sm)

#define SM_STEP_RUN(machine) sm_ ## machine ## _Step(sm)


SM_STATE(WPA_PTK, INITIALIZE)
{
	struct hostapd_data *hapd = sm->hapd;

	SM_ENTRY(WPA_PTK, INITIALIZE, wpa_ptk);
	if (sm->Init) {
		/* Init flag is not cleared here, so avoid busy
		 * loop by claiming nothing changed. */
		sm->changed = FALSE;
	}

	sm->keycount = 0;
	if (sm->GUpdateStationKeys)
		hapd->wpa_auth->GKeyDoneStations--;
	sm->GUpdateStationKeys = FALSE;
	if (sm->sta->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = FALSE;
	if (1 /* Unicast cipher supported AND (ESS OR ((IBSS or WDS) and
	       * Local AA > Remote AA)) */) {
		sm->Pair = TRUE;
	}
	ieee802_1x_notify_port_enabled(sm->sta->eapol_sm, 0);
	hostapd_set_encryption(sm->hapd, "none", sm->sta->addr, 0, (u8 *) "",
			       0);
	sm->pairwise_set = FALSE;
	sm->PTK_valid = FALSE;
	memset(&sm->PTK, 0, sizeof(sm->PTK));
	ieee802_1x_notify_port_valid(sm->sta->eapol_sm, 0);
	sm->TimeoutCtr = 0;
	if (sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
		ieee802_1x_set_sta_authorized(sm->hapd, sm->sta, 0);
}


SM_STATE(WPA_PTK, DISCONNECT)
{
	SM_ENTRY(WPA_PTK, DISCONNECT, wpa_ptk);
	sm->Disconnect = FALSE;
	wpa_sta_disconnect(sm->hapd, sm->sta);
}


SM_STATE(WPA_PTK, DISCONNECTED)
{
	SM_ENTRY(WPA_PTK, DISCONNECTED, wpa_ptk);
	sm->hapd->wpa_auth->GNoStations--;
	sm->DeauthenticationRequest = FALSE;
}


SM_STATE(WPA_PTK, AUTHENTICATION)
{
	SM_ENTRY(WPA_PTK, AUTHENTICATION, wpa_ptk);
	sm->hapd->wpa_auth->GNoStations++;
	memset(&sm->PTK, 0, sizeof(sm->PTK));
	sm->PTK_valid = FALSE;
	if (sm->sta->eapol_sm) {
		sm->sta->eapol_sm->portControl = Auto;
		sm->sta->eapol_sm->portEnabled = TRUE;
	}
	sm->AuthenticationRequest = FALSE;
}


SM_STATE(WPA_PTK, AUTHENTICATION2)
{
	SM_ENTRY(WPA_PTK, AUTHENTICATION2, wpa_ptk);
	memcpy(sm->ANonce, sm->hapd->wpa_auth->Counter, WPA_NONCE_LEN);
	inc_byte_array(sm->hapd->wpa_auth->Counter, WPA_NONCE_LEN);
	sm->ReAuthenticationRequest = FALSE;
	/* IEEE 802.11i/D9.0 does not clear TimeoutCtr here, but this is more
	 * logical place than INITIALIZE since AUTHENTICATION2 can be
	 * re-entered on ReAuthenticationRequest without going through
	 * INITIALIZE. */
	sm->TimeoutCtr = 0;
}


SM_STATE(WPA_PTK, INITPMK)
{
	u8 *key;
	size_t len;
	SM_ENTRY(WPA_PTK, INITPMK, wpa_ptk);
	if (sm->sta->pmksa) {
		wpa_printf(MSG_DEBUG, "WPA: PMK from PMKSA cache");
		memcpy(sm->PMK, sm->sta->pmksa->pmk, WPA_PMK_LEN);
		pmksa_cache_to_eapol_data(sm->sta->pmksa, sm->sta->eapol_sm);
	} else if ((key = ieee802_1x_get_key_crypt(sm->sta->eapol_sm, &len))) {
		wpa_printf(MSG_DEBUG, "WPA: PMK from EAPOL state machine "
			   "(len=%lu)", (unsigned long) len);
		if (len > WPA_PMK_LEN)
			len = WPA_PMK_LEN;
		memcpy(sm->PMK, key, len);
	} else {
		wpa_printf(MSG_DEBUG, "WPA: Could not get PMK");
	}
	sm->sta->req_replay_counter_used = 0;
	/* IEEE 802.11i/D9.0 does not set keyRun to FALSE, but not doing this
	 * will break reauthentication since EAPOL state machines may not be
	 * get into AUTHENTICATING state that clears keyRun before WPA state
	 * machine enters AUTHENTICATION2 state and goes immediately to INITPMK
	 * state and takes PMK from the previously used AAA Key. This will
	 * eventually fail in 4-Way Handshake because Supplicant uses PMK
	 * derived from the new AAA Key. Setting keyRun = FALSE here seems to
	 * be good workaround for this issue. */
	if (sm->sta->eapol_sm)
		sm->sta->eapol_sm->keyRun = FALSE;
}


SM_STATE(WPA_PTK, INITPSK)
{
	const u8 *psk;
	SM_ENTRY(WPA_PTK, INITPSK, wpa_ptk);
	psk = hostapd_get_psk(sm->hapd->conf, sm->sta->addr, NULL);
	if (psk)
		memcpy(sm->PMK, psk, WPA_PMK_LEN);
	sm->sta->req_replay_counter_used = 0;
}


SM_STATE(WPA_PTK, PTKSTART)
{
	u8 *pmkid = NULL;
	size_t pmkid_len = 0;

	SM_ENTRY(WPA_PTK, PTKSTART, wpa_ptk);
	sm->PTKRequest = FALSE;
	sm->TimeoutEvt = FALSE;
	hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG,
		       "sending 1/4 msg of 4-Way Handshake");
	if (sm->sta->pmksa &&
	    (pmkid = malloc(2 + RSN_SELECTOR_LEN + PMKID_LEN))) {
		pmkid_len = 2 + RSN_SELECTOR_LEN + PMKID_LEN;
		pmkid[0] = WLAN_EID_GENERIC;
		pmkid[1] = RSN_SELECTOR_LEN + PMKID_LEN;
		memcpy(&pmkid[2], RSN_KEY_DATA_PMKID, RSN_SELECTOR_LEN);
		memcpy(&pmkid[2 + RSN_SELECTOR_LEN], sm->sta->pmksa->pmkid,
		       PMKID_LEN);
	}
	wpa_send_eapol(sm->hapd, sm->sta, 0, 0, 1, 0, 1, NULL, sm->ANonce,
		       pmkid, pmkid_len, NULL, 0, 0);
	free(pmkid);
	sm->TimeoutCtr++;
}


SM_STATE(WPA_PTK, PTKCALCNEGOTIATING)
{
	struct wpa_ptk PTK;
	int ok = 0;
	const u8 *pmk = NULL;

	SM_ENTRY(WPA_PTK, PTKCALCNEGOTIATING, wpa_ptk);
	sm->EAPOLKeyReceived = FALSE;

	/* WPA with IEEE 802.1X: use the derived PMK from EAP
	 * WPA-PSK: iterate through possible PSKs and select the one matching
	 * the packet */
	for (;;) {
		if (sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
			pmk = hostapd_get_psk(sm->hapd->conf, sm->sta->addr,
					      pmk);
			if (pmk == NULL)
				break;
		} else
			pmk = sm->PMK;

		wpa_pmk_to_ptk(sm->hapd, pmk, sm->hapd->own_addr,
			       sm->sta->addr, sm->ANonce, sm->SNonce,
			       (u8 *) &PTK, sizeof(PTK));

		if (wpa_verify_key_mic(&PTK, sm->last_rx_eapol_key,
				       sm->last_rx_eapol_key_len) == 0) {
			ok = 1;
			break;
		}

		if (sm->sta->wpa_key_mgmt != WPA_KEY_MGMT_PSK)
			break;
	}

	if (!ok) {
		hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
			       HOSTAPD_LEVEL_DEBUG, "invalid MIC in msg 2/4 "
			       "of 4-Way Handshake");
		return;
	}

	eloop_cancel_timeout(wpa_send_eapol_timeout, sm->hapd, sm->sta);

	if (sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK) {
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
	SM_ENTRY(WPA_PTK, PTKCALCNEGOTIATING2, wpa_ptk);
	sm->TimeoutCtr = 0;
}


SM_STATE(WPA_PTK, PTKINITNEGOTIATING)
{
	u8 rsc[WPA_KEY_RSC_LEN];
	struct wpa_authenticator *gsm = sm->hapd->wpa_auth;
	u8 *wpa_ie;
	int wpa_ie_len;

	SM_ENTRY(WPA_PTK, PTKINITNEGOTIATING, wpa_ptk);
	sm->TimeoutEvt = FALSE;
	/* Send EAPOL(1, 1, 1, Pair, P, RSC, ANonce, MIC(PTK), RSNIE, GTK[GN])
	 */
	memset(rsc, 0, WPA_KEY_RSC_LEN);
	hostapd_get_seqnum(sm->hapd, NULL, gsm->GN, rsc);
	wpa_ie = sm->hapd->wpa_ie;
	wpa_ie_len = sm->hapd->wpa_ie_len;
	if (sm->sta->wpa == WPA_VERSION_WPA &&
	    (sm->hapd->conf->wpa & HOSTAPD_WPA_VERSION_WPA2) &&
	    wpa_ie_len > wpa_ie[1] + 2 && wpa_ie[0] == WLAN_EID_RSN) {
		/* WPA-only STA, remove RSN IE */
		wpa_ie = wpa_ie + wpa_ie[1] + 2;
		wpa_ie_len = wpa_ie[1] + 2;
	}
	hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG,
		       "sending 3/4 msg of 4-Way Handshake");
	wpa_send_eapol(sm->hapd, sm->sta,
		       sm->sta->wpa == WPA_VERSION_WPA2 ? 1 : 0,
		       1, 1, 1, 1, rsc, sm->ANonce,
		       wpa_ie, wpa_ie_len,
		       gsm->GTK[gsm->GN - 1], gsm->GTK_len, gsm->GN);
	sm->TimeoutCtr++;
}


SM_STATE(WPA_PTK, PTKINITDONE)
{
	SM_ENTRY(WPA_PTK, PTKINITDONE, wpa_ptk);
	sm->EAPOLKeyReceived = FALSE;
	if (sm->Pair) {
		char *alg;
		int klen;
		if (sm->sta->pairwise == WPA_CIPHER_TKIP) {
			alg = "TKIP";
			klen = 32;
		} else {
			alg = "CCMP";
			klen = 16;
		}
		if (hostapd_set_encryption(sm->hapd, alg, sm->sta->addr, 0,
					   sm->PTK.tk1, klen)) {
			wpa_sta_disconnect(sm->hapd, sm->sta);
			return;
		}
		/* FIX: MLME-SetProtection.Request(TA, Tx_Rx) */
		sm->pairwise_set = TRUE;

		if (sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
			ieee802_1x_set_sta_authorized(sm->hapd, sm->sta, 1);
	}

	if (0 /* IBSS == TRUE */) {
		sm->keycount++;
		if (sm->keycount == 2) {
			ieee802_1x_notify_port_valid(sm->sta->eapol_sm, 1);
		}
	} else {
		ieee802_1x_notify_port_valid(sm->sta->eapol_sm, 1);
	}
	if (sm->sta->eapol_sm) {
		sm->sta->eapol_sm->keyAvailable = FALSE;
		sm->sta->eapol_sm->keyDone = TRUE;
	}
	if (sm->sta->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = TRUE;
	else
		sm->has_GTK = TRUE;
	hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_INFO, "pairwise key handshake completed "
		       "(%s)",
		       sm->sta->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
	if (sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
		accounting_sta_start(sm->hapd, sm->sta);
}


SM_STEP(WPA_PTK)
{
	struct wpa_authenticator *wpa_auth = sm->hapd->wpa_auth;

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
		if ((sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_IEEE8021X) &&
		    sm->sta->eapol_sm && sm->sta->eapol_sm->keyRun)
			SM_ENTER(WPA_PTK, INITPMK);
		else if ((sm->sta->wpa_key_mgmt == WPA_KEY_MGMT_PSK)
			 /* FIX: && 802.1X::keyRun */)
			SM_ENTER(WPA_PTK, INITPSK);
		break;
	case WPA_PTK_INITPMK:
		if (sm->sta->eapol_sm && sm->sta->eapol_sm->keyAvailable)
			SM_ENTER(WPA_PTK, PTKSTART);
		else {
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_INITPSK:
		if (hostapd_get_psk(sm->hapd->conf, sm->sta->addr, NULL))
			SM_ENTER(WPA_PTK, PTKSTART);
		else {
			hostapd_logger(sm->hapd, sm->sta->addr,
				       HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_INFO,
				       "no PSK configured for the STA");
			wpa_auth->dot11RSNA4WayHandshakeFailures++;
			SM_ENTER(WPA_PTK, DISCONNECT);
		}
		break;
	case WPA_PTK_PTKSTART:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    sm->EAPOLKeyPairwise)
			SM_ENTER(WPA_PTK, PTKCALCNEGOTIATING);
		else if (sm->TimeoutCtr > dot11RSNAConfigPairwiseUpdateCount) {
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
		else if (sm->TimeoutCtr > dot11RSNAConfigPairwiseUpdateCount) {
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
	SM_ENTRY(WPA_PTK_GROUP, IDLE, wpa_ptk_group);
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
	struct wpa_authenticator *gsm = sm->hapd->wpa_auth;

	SM_ENTRY(WPA_PTK_GROUP, REKEYNEGOTIATING, wpa_ptk_group);
	if (sm->sta->wpa == WPA_VERSION_WPA)
		sm->PInitAKeys = FALSE;
	sm->TimeoutEvt = FALSE;
	/* Send EAPOL(1, 1, 1, !Pair, G, RSC, GNonce, MIC(PTK), GTK[GN]) */
	memset(rsc, 0, WPA_KEY_RSC_LEN);
	if (gsm->wpa_group_state == WPA_GROUP_SETKEYSDONE)
		hostapd_get_seqnum(sm->hapd, NULL, gsm->GN, rsc);
	hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_DEBUG,
		       "sending 1/2 msg of Group Key Handshake");
	wpa_send_eapol(sm->hapd, sm->sta, 1, 1, 1, !sm->Pair, 0, rsc,
		       gsm->GNonce, NULL, 0,
		       gsm->GTK[gsm->GN - 1], gsm->GTK_len, gsm->GN);
	sm->GTimeoutCtr++;
}


SM_STATE(WPA_PTK_GROUP, REKEYESTABLISHED)
{
	SM_ENTRY(WPA_PTK_GROUP, REKEYESTABLISHED, wpa_ptk_group);
	sm->EAPOLKeyReceived = FALSE;
	sm->GUpdateStationKeys = FALSE;
	sm->hapd->wpa_auth->GKeyDoneStations--;
	sm->GTimeoutCtr = 0;
	/* FIX: MLME.SetProtection.Request(TA, Tx_Rx) */
	hostapd_logger(sm->hapd, sm->sta->addr, HOSTAPD_MODULE_WPA,
		       HOSTAPD_LEVEL_INFO, "group key handshake completed "
		       "(%s)",
		       sm->sta->wpa == WPA_VERSION_WPA ? "WPA" : "RSN");
	sm->has_GTK = TRUE;
}


SM_STATE(WPA_PTK_GROUP, KEYERROR)
{
	SM_ENTRY(WPA_PTK_GROUP, KEYERROR, wpa_ptk_group);
	sm->hapd->wpa_auth->GKeyDoneStations--;
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
		    (sm->sta->wpa == WPA_VERSION_WPA && sm->PInitAKeys))
			SM_ENTER(WPA_PTK_GROUP, REKEYNEGOTIATING);
		break;
	case WPA_PTK_GROUP_REKEYNEGOTIATING:
		if (sm->EAPOLKeyReceived && !sm->EAPOLKeyRequest &&
		    !sm->EAPOLKeyPairwise && sm->MICVerified)
			SM_ENTER(WPA_PTK_GROUP, REKEYESTABLISHED);
		else if (sm->GTimeoutCtr > dot11RSNAConfigGroupUpdateCount)
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


static void wpa_group_gtk_init(struct hostapd_data *hapd)
{
	struct wpa_authenticator *sm = hapd->wpa_auth;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "WPA: group state machine "
		      "entering state GTK_INIT\n");
	sm->changed = FALSE; /* GInit is not cleared here; avoid loop */
	sm->wpa_group_state = WPA_GROUP_GTK_INIT;

	/* GTK[0..N] = 0 */
	memset(sm->GTK, 0, sizeof(sm->GTK));
	sm->GN = 1;
	sm->GM = 2;
	/* GTK[GN] = CalcGTK() */
	/* FIX: is this the correct way of getting GNonce? */
	memcpy(sm->GNonce, sm->Counter, WPA_NONCE_LEN);
	inc_byte_array(sm->Counter, WPA_NONCE_LEN);
	wpa_gmk_to_gtk(hapd, sm->GMK, hapd->own_addr, sm->GNonce,
		       sm->GTK[sm->GN - 1], sm->GTK_len);
}


static void wpa_group_setkeys(struct hostapd_data *hapd)
{
	struct wpa_authenticator *sm = hapd->wpa_auth;
	struct sta_info *sta;
	int tmp;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "WPA: group state machine "
		      "entering state SETKEYS\n");
	sm->changed = TRUE;
	sm->wpa_group_state = WPA_GROUP_SETKEYS;
	sm->GTKReKey = FALSE;
	tmp = sm->GM;
	sm->GM = sm->GN;
	sm->GN = tmp;
	sm->GKeyDoneStations = sm->GNoStations;
	/* FIX: is this the correct way of getting GNonce? */
	memcpy(sm->GNonce, sm->Counter, WPA_NONCE_LEN);
	inc_byte_array(sm->Counter, WPA_NONCE_LEN);
	wpa_gmk_to_gtk(hapd, sm->GMK, hapd->own_addr, sm->GNonce,
		       sm->GTK[sm->GN - 1], sm->GTK_len);

	sta = hapd->sta_list;
	while (sta) {
		if (sta->wpa_sm) {
			sta->wpa_sm->GUpdateStationKeys = TRUE;
			wpa_sm_step(sta->wpa_sm);
		}
		sta = sta->next;
	}
}


static void wpa_group_setkeysdone(struct hostapd_data *hapd)
{
	struct wpa_authenticator *sm = hapd->wpa_auth;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "WPA: group state machine "
		      "entering state SETKEYSDONE\n");
	sm->changed = TRUE;
	sm->wpa_group_state = WPA_GROUP_SETKEYSDONE;
	hostapd_set_encryption(hapd, wpa_alg_txt(hapd->conf->wpa_group),
			       NULL, sm->GN, sm->GTK[sm->GN - 1], sm->GTK_len);
}


static void wpa_group_sm_step(struct hostapd_data *hapd)
{
	struct wpa_authenticator *sm = hapd->wpa_auth;

	if (sm->GInit) {
		wpa_group_gtk_init(hapd);
	} else if (sm->wpa_group_state == WPA_GROUP_GTK_INIT &&
		   sm->GTKAuthenticator) {
		wpa_group_setkeysdone(hapd);
	} else if (sm->wpa_group_state == WPA_GROUP_SETKEYSDONE &&
		   sm->GTKReKey) {
		wpa_group_setkeys(hapd);
	} else if (sm->wpa_group_state == WPA_GROUP_SETKEYS) {
		if (sm->GKeyDoneStations == 0)
			wpa_group_setkeysdone(hapd);
		else if (sm->GTKReKey)
			wpa_group_setkeys(hapd);
	}
}


static int wpa_sm_sta_entry_alive(struct hostapd_data *hapd, u8 *addr)
{
	struct sta_info *sta;
	sta = ap_get_sta(hapd, addr);
	if (sta == NULL || sta->wpa_sm == NULL)
		return 0;
	return 1;
}


static void wpa_sm_step(struct wpa_state_machine *sm)
{
	struct hostapd_data *hapd = sm->hapd;
	u8 addr[6];
	if (sm == NULL || sm->sta == NULL || sm->sta->wpa_sm == NULL)
		return;

	memcpy(addr, sm->sta->addr, 6);
	do {
		sm->changed = FALSE;
		sm->hapd->wpa_auth->changed = FALSE;

		SM_STEP_RUN(WPA_PTK);
		if (!wpa_sm_sta_entry_alive(hapd, addr))
			break;
		SM_STEP_RUN(WPA_PTK_GROUP);
		if (!wpa_sm_sta_entry_alive(hapd, addr))
			break;
		wpa_group_sm_step(sm->hapd);
		if (!wpa_sm_sta_entry_alive(hapd, addr))
			break;
	} while (sm->changed || sm->hapd->wpa_auth->changed);
}


static void wpa_sm_call_step(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_state_machine *sm = timeout_ctx;
	wpa_sm_step(sm);
}


void wpa_sm_notify(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->wpa_sm == NULL)
		return;
	eloop_register_timeout(0, 0, wpa_sm_call_step, hapd, sta->wpa_sm);
}


void wpa_gtk_rekey(struct hostapd_data *hapd)
{
	struct wpa_authenticator *sm = hapd->wpa_auth;
	int tmp, i;

	if (sm == NULL)
		return;

	for (i = 0; i < 2; i++) {
		tmp = sm->GM;
		sm->GM = sm->GN;
		sm->GN = tmp;
		memcpy(sm->GNonce, sm->Counter, WPA_NONCE_LEN);
		inc_byte_array(sm->Counter, WPA_NONCE_LEN);
		wpa_gmk_to_gtk(hapd, sm->GMK, hapd->own_addr, sm->GNonce,
			       sm->GTK[sm->GN - 1], sm->GTK_len);
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

int wpa_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	int len = 0, i;
	char pmkid_txt[PMKID_LEN * 2 + 1], *pos;

	len += snprintf(buf + len, buflen - len,
			"dot11RSNAOptionImplemented=TRUE\n"
			"dot11RSNAPreauthenticationImplemented=TRUE\n"
			"dot11RSNAEnabled=%s\n"
			"dot11RSNAPreauthenticationEnabled=%s\n",
			wpa_bool_txt(hapd->conf->wpa &
				     HOSTAPD_WPA_VERSION_WPA2),
			wpa_bool_txt(hapd->conf->rsn_preauth));

	if (hapd->wpa_auth == NULL)
		return len;

	pos = pmkid_txt;
	for (i = 0; i < PMKID_LEN; i++) {
		pos += sprintf(pos, "%02x",
			       hapd->wpa_auth->dot11RSNAPMKIDUsed[i]);
	}

	len += snprintf(buf + len, buflen - len,
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
			!!hapd->conf->wpa_strict_rekey,
			dot11RSNAConfigGroupUpdateCount,
			dot11RSNAConfigPairwiseUpdateCount,
			wpa_cipher_bits(hapd->conf->wpa_group),
			dot11RSNAConfigPMKLifetime,
			dot11RSNAConfigPMKReauthThreshold,
			dot11RSNAConfigSATimeout,
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAAuthenticationSuiteSelected),
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAPairwiseCipherSelected),
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAGroupCipherSelected),
			pmkid_txt,
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAAuthenticationSuiteRequested),
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAPairwiseCipherRequested),
			RSN_SUITE_ARG(hapd->wpa_auth->
				      dot11RSNAGroupCipherRequested),
			hapd->wpa_auth->dot11RSNATKIPCounterMeasuresInvoked,
			hapd->wpa_auth->dot11RSNA4WayHandshakeFailures);

	/* TODO: dot11RSNAConfigPairwiseCiphersTable */
	/* TODO: dot11RSNAConfigAuthenticationSuitesTable */

	/* Private MIB */
	len += snprintf(buf + len, buflen - len,
			"hostapdWPAGroupState=%d\n",
			hapd->wpa_auth->wpa_group_state);

	return len;
}


int wpa_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
		    char *buf, size_t buflen)
{
	int len = 0;
	u8 not_used[4] = { 0, 0, 0, 0 };
	const u8 *pairwise = not_used;

	/* TODO: FF-FF-FF-FF-FF-FF entry for broadcast/multicast stats */

	/* dot11RSNAStatsEntry */

	if (sta->wpa == WPA_VERSION_WPA) {
		if (sta->pairwise == WPA_CIPHER_CCMP)
			pairwise = WPA_CIPHER_SUITE_CCMP;
		else if (sta->pairwise == WPA_CIPHER_TKIP)
			pairwise = WPA_CIPHER_SUITE_TKIP;
		else if (sta->pairwise == WPA_CIPHER_WEP104)
			pairwise = WPA_CIPHER_SUITE_WEP104;
		else if (sta->pairwise == WPA_CIPHER_WEP40)
			pairwise = WPA_CIPHER_SUITE_WEP40;
		else if (sta->pairwise == WPA_CIPHER_NONE)
			pairwise = WPA_CIPHER_SUITE_NONE;
	} else if (sta->wpa == WPA_VERSION_WPA2) {
		if (sta->pairwise == WPA_CIPHER_CCMP)
			pairwise = RSN_CIPHER_SUITE_CCMP;
		else if (sta->pairwise == WPA_CIPHER_TKIP)
			pairwise = RSN_CIPHER_SUITE_TKIP;
		else if (sta->pairwise == WPA_CIPHER_WEP104)
			pairwise = RSN_CIPHER_SUITE_WEP104;
		else if (sta->pairwise == WPA_CIPHER_WEP40)
			pairwise = RSN_CIPHER_SUITE_WEP40;
		else if (sta->pairwise == WPA_CIPHER_NONE)
			pairwise = RSN_CIPHER_SUITE_NONE;
	} else
		return 0;

	len += snprintf(buf + len, buflen - len,
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
			MAC2STR(sta->addr),
			RSN_SUITE_ARG(pairwise),
			sta->dot11RSNAStatsTKIPLocalMICFailures,
			sta->dot11RSNAStatsTKIPRemoteMICFailures);

	if (sta->wpa_sm == NULL)
		return len;

	/* Private MIB */
	len += snprintf(buf + len, buflen - len,
			"hostapdWPAPTKState=%d\n"
			"hostapdWPAPTKGroupState=%d\n",
			sta->wpa_sm->wpa_ptk_state,
			sta->wpa_sm->wpa_ptk_group_state);

	return len;
}
