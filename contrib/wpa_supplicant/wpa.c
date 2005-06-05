/*
 * WPA Supplicant
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <netinet/in.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <string.h>
#include <time.h>

#include "common.h"
#include "md5.h"
#include "sha1.h"
#include "rc4.h"
#include "aes_wrap.h"
#include "wpa.h"
#include "driver.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "l2_packet.h"
#include "eapol_sm.h"
#include "wpa_supplicant_i.h"

static void rsn_preauth_candidate_process(struct wpa_supplicant *wpa_s);

#define PMKID_CANDIDATE_PRIO_SCAN 1000

/* TODO: make these configurable */
static const int dot11RSNAConfigPMKLifetime = 43200;
static const int dot11RSNAConfigPMKReauthThreshold = 70;
static const int dot11RSNAConfigSATimeout = 60;
static const int pmksa_cache_max_entries = 32;

static const int WPA_SELECTOR_LEN = 4;
static const u8 WPA_OUI_TYPE[] = { 0x00, 0x50, 0xf2, 1 };
static const u16 WPA_VERSION = 1;
static const u8 WPA_AUTH_KEY_MGMT_NONE[] = { 0x00, 0x50, 0xf2, 0 };
static const u8 WPA_AUTH_KEY_MGMT_UNSPEC_802_1X[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 WPA_CIPHER_SUITE_NONE[] = { 0x00, 0x50, 0xf2, 0 };
static const u8 WPA_CIPHER_SUITE_WEP40[] = { 0x00, 0x50, 0xf2, 1 };
static const u8 WPA_CIPHER_SUITE_TKIP[] = { 0x00, 0x50, 0xf2, 2 };
static const u8 WPA_CIPHER_SUITE_WRAP[] = { 0x00, 0x50, 0xf2, 3 };
static const u8 WPA_CIPHER_SUITE_CCMP[] = { 0x00, 0x50, 0xf2, 4 };
static const u8 WPA_CIPHER_SUITE_WEP104[] = { 0x00, 0x50, 0xf2, 5 };

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
 *    (default: unspec 802.1x)
 * WPA Capabilities (2 octets, little endian) (default: 0)
 */

struct wpa_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 oui[3];
	u8 oui_type;
	u16 version;
} __attribute__ ((packed));


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

/* 1/4: PMKID
 * 2/4: RSN IE
 * 3/4: one or two RSN IEs + GTK IE (encrypted)
 * 4/4: empty
 * 1/2: GTK IE (encrypted)
 * 2/2: empty
 */

/* RSN IE version 1
 * 0x01 0x00 (version; little endian)
 * (all following fields are optional:)
 * Group Suite Selector (4 octets) (default: CCMP)
 * Pairwise Suite Count (2 octets, little endian) (default: 1)
 * Pairwise Suite List (4 * n octets) (default: CCMP)
 * Authenticated Key Management Suite Count (2 octets, little endian)
 *    (default: 1)
 * Authenticated Key Management Suite List (4 * n octets)
 *    (default: unspec 802.1x)
 * RSN Capabilities (2 octets, little endian) (default: 0)
 * PMKID Count (2 octets) (default: 0)
 * PMKID List (16 * n octets)
 */

struct rsn_ie_hdr {
	u8 elem_id; /* WLAN_EID_RSN */
	u8 len;
	u16 version;
} __attribute__ ((packed));


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
	if (memcmp(s, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_WPA_NONE;
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


static void rsn_pmkid(u8 *pmk, u8 *aa, u8 *spa, u8 *pmkid)
{
	char *title = "PMK Name";
	const unsigned char *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA1_MAC_LEN];

	addr[0] = (unsigned char *) title;
	addr[1] = aa;
	addr[2] = spa;

	hmac_sha1_vector(pmk, PMK_LEN, 3, addr, len, hash);
	memcpy(pmkid, hash, PMKID_LEN);
}


static void pmksa_cache_set_expiration(struct wpa_supplicant *wpa_s);


static void pmksa_cache_free_entry(struct wpa_supplicant *wpa_s,
				   struct rsn_pmksa_cache *entry)
{
	free(entry);
	wpa_s->pmksa_count--;
	if (wpa_s->cur_pmksa == entry) {
		wpa_printf(MSG_DEBUG, "RSN: removed current PMKSA entry");
		/* TODO: should drop PMK and PTK and trigger new key
		 * negotiation */
		wpa_s->cur_pmksa = NULL;
	}
}


static void pmksa_cache_expire(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	time_t now;

	time(&now);
	while (wpa_s->pmksa && wpa_s->pmksa->expiration <= now) {
		struct rsn_pmksa_cache *entry = wpa_s->pmksa;
		wpa_s->pmksa = entry->next;
		wpa_printf(MSG_DEBUG, "RSN: expired PMKSA cache entry for "
			   MACSTR, MAC2STR(entry->aa));
		pmksa_cache_free_entry(wpa_s, entry);
	}

	pmksa_cache_set_expiration(wpa_s);
}


static void pmksa_cache_set_expiration(struct wpa_supplicant *wpa_s)
{
	int sec;
	eloop_cancel_timeout(pmksa_cache_expire, wpa_s, NULL);
	if (wpa_s->pmksa == NULL)
		return;
	sec = wpa_s->pmksa->expiration - time(NULL);
	if (sec < 0)
		sec = 0;
	eloop_register_timeout(sec + 1, 0, pmksa_cache_expire, wpa_s, NULL);
}


static void pmksa_cache_add(struct wpa_supplicant *wpa_s, u8 *pmk,
			    size_t pmk_len, u8 *aa, u8 *spa)
{
	struct rsn_pmksa_cache *entry, *pos, *prev;

	if (wpa_s->proto != WPA_PROTO_RSN || pmk_len > PMK_LEN)
		return;

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return;
	memset(entry, 0, sizeof(*entry));
	memcpy(entry->pmk, pmk, pmk_len);
	entry->pmk_len = pmk_len;
	rsn_pmkid(pmk, aa, spa, entry->pmkid);
	entry->expiration = time(NULL) + dot11RSNAConfigPMKLifetime;
	entry->akmp = WPA_KEY_MGMT_IEEE8021X;
	memcpy(entry->aa, aa, ETH_ALEN);

	/* Replace an old entry for the same Authenticator (if found) with the
	 * new entry */
	pos = wpa_s->pmksa;
	prev = NULL;
	while (pos) {
		if (memcmp(aa, pos->aa, ETH_ALEN) == 0) {
			if (prev == NULL)
				wpa_s->pmksa = pos->next;
			else
				prev->next = pos->next;
			pmksa_cache_free_entry(wpa_s, pos);
			break;
		}
		prev = pos;
		pos = pos->next;
	}

	if (wpa_s->pmksa_count >= pmksa_cache_max_entries && wpa_s->pmksa) {
		/* Remove the oldest entry to make room for the new entry */
		pos = wpa_s->pmksa;
		wpa_s->pmksa = pos->next;
		wpa_printf(MSG_DEBUG, "RSN: removed the oldest PMKSA cache "
			   "entry (for " MACSTR ") to make room for new one",
			   MAC2STR(pos->aa));
		wpa_drv_remove_pmkid(wpa_s, pos->aa, pos->pmkid);
		pmksa_cache_free_entry(wpa_s, pos);
	}

	/* Add the new entry; order by expiration time */
	pos = wpa_s->pmksa;
	prev = NULL;
	while (pos) {
		if (pos->expiration > entry->expiration)
			break;
		prev = pos;
		pos = pos->next;
	}
	if (prev == NULL) {
		entry->next = wpa_s->pmksa;
		wpa_s->pmksa = entry;
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	wpa_s->pmksa_count++;
	wpa_printf(MSG_DEBUG, "RSN: added PMKSA cache entry for " MACSTR,
		   MAC2STR(entry->aa));
	wpa_drv_add_pmkid(wpa_s, entry->aa, entry->pmkid);
}


void pmksa_cache_free(struct wpa_supplicant *wpa_s)
{
	struct rsn_pmksa_cache *entry, *prev;

	entry = wpa_s->pmksa;
	wpa_s->pmksa = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		free(prev);
	}
	pmksa_cache_set_expiration(wpa_s);
	wpa_s->cur_pmksa = NULL;
}


struct rsn_pmksa_cache * pmksa_cache_get(struct wpa_supplicant *wpa_s,
					 u8 *aa, u8 *pmkid)
{
	struct rsn_pmksa_cache *entry = wpa_s->pmksa;
	while (entry) {
		if ((aa == NULL || memcmp(entry->aa, aa, ETH_ALEN) == 0) &&
		    (pmkid == NULL ||
		     memcmp(entry->pmkid, pmkid, PMKID_LEN) == 0))
			return entry;
		entry = entry->next;
	}
	return NULL;
}


int pmksa_cache_list(struct wpa_supplicant *wpa_s, char *buf, size_t len)
{
	int i, j;
	char *pos = buf;
	struct rsn_pmksa_cache *entry;
	time_t now;

	time(&now);
	pos += snprintf(pos, buf + len - pos,
			"Index / AA / PMKID / expiration (in seconds)\n");
	i = 0;
	entry = wpa_s->pmksa;
	while (entry) {
		i++;
		pos += snprintf(pos, buf + len - pos, "%d " MACSTR " ",
				i, MAC2STR(entry->aa));
		for (j = 0; j < PMKID_LEN; j++)
			pos += snprintf(pos, buf + len - pos, "%02x",
					entry->pmkid[j]);
		pos += snprintf(pos, buf + len - pos, " %d\n",
				(int) (entry->expiration - now));
		entry = entry->next;
	}
	return pos - buf;
}


void pmksa_candidate_free(struct wpa_supplicant *wpa_s)
{
	struct rsn_pmksa_candidate *entry, *prev;

	entry = wpa_s->pmksa_candidates;
	wpa_s->pmksa_candidates = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		free(prev);
	}
}


static int wpa_parse_wpa_ie_wpa(struct wpa_supplicant *wpa_s, u8 *wpa_ie,
				size_t wpa_ie_len, struct wpa_ie_data *data)
{
	struct wpa_ie_hdr *hdr;
	u8 *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_WPA;
	data->pairwise_cipher = WPA_CIPHER_TKIP;
	data->group_cipher = WPA_CIPHER_TKIP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (wpa_ie_len == 0) {
		/* No WPA IE - fail silently */
		return -1;
	}

	if (wpa_ie_len < sizeof(struct wpa_ie_hdr)) {
		wpa_printf(MSG_DEBUG, "%s: ie len too short %lu",
			   __func__, (unsigned long) wpa_ie_len);
		return -1;
	}

	hdr = (struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != GENERIC_INFO_ELEM ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN) != 0 ||
	    le_to_host16(hdr->version) != WPA_VERSION) {
		wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
			   __func__);
		return -1;
	}

	pos = (u8 *) (hdr + 1);
	left = wpa_ie_len - sizeof(*hdr);

	if (left >= WPA_SELECTOR_LEN) {
		data->group_cipher = wpa_selector_to_bitfield(pos);
		pos += WPA_SELECTOR_LEN;
		left -= WPA_SELECTOR_LEN;
	} else if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie length mismatch, %u too much",
			   __func__, left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (pairwise), "
				   "count %u left %u", __func__, count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= wpa_selector_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for key mgmt)",
			   __func__);
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * WPA_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (key mgmt), "
				   "count %u left %u", __func__, count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= wpa_key_mgmt_to_bitfield(pos);
			pos += WPA_SELECTOR_LEN;
			left -= WPA_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for capabilities)",
			   __func__);
		return -1;
	}

	if (left >= 2) {
		data->capabilities = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
	}

	if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie has %u trailing bytes",
			   __func__, left);
		return -1;
	}

	return 0;
}


static int wpa_parse_wpa_ie_rsn(struct wpa_supplicant *wpa_s, u8 *rsn_ie,
				size_t rsn_ie_len, struct wpa_ie_data *data)
{
	struct rsn_ie_hdr *hdr;
	u8 *pos;
	int left;
	int i, count;

	data->proto = WPA_PROTO_RSN;
	data->pairwise_cipher = WPA_CIPHER_CCMP;
	data->group_cipher = WPA_CIPHER_CCMP;
	data->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
	data->capabilities = 0;
	data->pmkid = NULL;
	data->num_pmkid = 0;

	if (rsn_ie_len == 0) {
		/* No RSN IE - fail silently */
		return -1;
	}

	if (rsn_ie_len < sizeof(struct rsn_ie_hdr)) {
		wpa_printf(MSG_DEBUG, "%s: ie len too short %lu",
			   __func__, (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != RSN_INFO_ELEM ||
	    hdr->len != rsn_ie_len - 2 ||
	    le_to_host16(hdr->version) != RSN_VERSION) {
		wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
			   __func__);
		return -1;
	}

	pos = (u8 *) (hdr + 1);
	left = rsn_ie_len - sizeof(*hdr);

	if (left >= RSN_SELECTOR_LEN) {
		data->group_cipher = rsn_selector_to_bitfield(pos);
		pos += RSN_SELECTOR_LEN;
		left -= RSN_SELECTOR_LEN;
	} else if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie length mismatch, %u too much",
			   __func__, left);
		return -1;
	}

	if (left >= 2) {
		data->pairwise_cipher = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (pairwise), "
				   "count %u left %u", __func__, count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->pairwise_cipher |= rsn_selector_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for key mgmt)",
			   __func__);
		return -1;
	}

	if (left >= 2) {
		data->key_mgmt = 0;
		count = pos[0] | (pos[1] << 8);
		pos += 2;
		left -= 2;
		if (count == 0 || left < count * RSN_SELECTOR_LEN) {
			wpa_printf(MSG_DEBUG, "%s: ie count botch (key mgmt), "
				   "count %u left %u", __func__, count, left);
			return -1;
		}
		for (i = 0; i < count; i++) {
			data->key_mgmt |= rsn_key_mgmt_to_bitfield(pos);
			pos += RSN_SELECTOR_LEN;
			left -= RSN_SELECTOR_LEN;
		}
	} else if (left == 1) {
		wpa_printf(MSG_DEBUG, "%s: ie too short (for capabilities)",
			   __func__);
		return -1;
	}

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
			wpa_printf(MSG_DEBUG, "%s: PMKID underflow "
				   "(num_pmkid=%d left=%d)",
				   __func__, data->num_pmkid, left);
			data->num_pmkid = 0;
		} else {
			data->pmkid = pos;
			pos += data->num_pmkid * PMKID_LEN;
			left -= data->num_pmkid * PMKID_LEN;
		}
	}

	if (left > 0) {
		wpa_printf(MSG_DEBUG, "%s: ie has %u trailing bytes - ignored",
			   __func__, left);
	}

	return 0;
}


int wpa_parse_wpa_ie(struct wpa_supplicant *wpa_s, u8 *wpa_ie,
		     size_t wpa_ie_len, struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == RSN_INFO_ELEM)
		return wpa_parse_wpa_ie_rsn(wpa_s, wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_s, wpa_ie, wpa_ie_len, data);
}


static int wpa_gen_wpa_ie_wpa(struct wpa_supplicant *wpa_s, u8 *wpa_ie)
{
	u8 *pos;
	struct wpa_ie_hdr *hdr;

	hdr = (struct wpa_ie_hdr *) wpa_ie;
	hdr->elem_id = GENERIC_INFO_ELEM;
	memcpy(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN);
	hdr->version = host_to_le16(WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	if (wpa_s->group_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_WEP104) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_WEP40) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   wpa_s->group_cipher);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (wpa_s->pairwise_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (wpa_s->pairwise_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (wpa_s->pairwise_cipher == WPA_CIPHER_NONE) {
		memcpy(pos, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   wpa_s->pairwise_cipher);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN);
	} else if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       WPA_SELECTOR_LEN);
	} else if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   wpa_s->key_mgmt);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - wpa_ie) - 2;

	return pos - wpa_ie;
}


static int wpa_gen_wpa_ie_rsn(struct wpa_supplicant *wpa_s, u8 *rsn_ie)
{
	u8 *pos;
	struct rsn_ie_hdr *hdr;

	hdr = (struct rsn_ie_hdr *) rsn_ie;
	hdr->elem_id = RSN_INFO_ELEM;
	hdr->version = host_to_le16(RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	if (wpa_s->group_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_WEP104) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN);
	} else if (wpa_s->group_cipher == WPA_CIPHER_WEP40) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   wpa_s->group_cipher);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (wpa_s->pairwise_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (wpa_s->pairwise_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (wpa_s->pairwise_cipher == WPA_CIPHER_NONE) {
		memcpy(pos, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   wpa_s->pairwise_cipher);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN);
	} else if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   wpa_s->key_mgmt);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	*pos++ = 0;
	*pos++ = 0;

	if (wpa_s->cur_pmksa) {
		/* PMKID Count (2 octets, little endian) */
		*pos++ = 1;
		*pos++ = 0;
		/* PMKID */
		memcpy(pos, wpa_s->cur_pmksa->pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	hdr->len = (pos - rsn_ie) - 2;

	return pos - rsn_ie;
}


int wpa_gen_wpa_ie(struct wpa_supplicant *wpa_s, u8 *wpa_ie)
{
	if (wpa_s->proto == WPA_PROTO_RSN)
		return wpa_gen_wpa_ie_rsn(wpa_s, wpa_ie);
	else
		return wpa_gen_wpa_ie_wpa(wpa_s, wpa_ie);
}


static void wpa_pmk_to_ptk(u8 *pmk, size_t pmk_len, u8 *addr1, u8 *addr2,
			   u8 *nonce1, u8 *nonce2, u8 *ptk, size_t ptk_len)
{
	u8 data[2 * ETH_ALEN + 2 * 32];

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

	if (memcmp(nonce1, nonce2, 32) < 0) {
		memcpy(data + 2 * ETH_ALEN, nonce1, 32);
		memcpy(data + 2 * ETH_ALEN + 32, nonce2, 32);
	} else {
		memcpy(data + 2 * ETH_ALEN, nonce2, 32);
		memcpy(data + 2 * ETH_ALEN + 32, nonce1, 32);
	}

	sha1_prf(pmk, pmk_len, "Pairwise key expansion", data, sizeof(data),
		 ptk, ptk_len);

	wpa_hexdump_key(MSG_DEBUG, "WPA: PMK", pmk, pmk_len);
	wpa_hexdump_key(MSG_DEBUG, "WPA: PTK", ptk, ptk_len);
}


struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *entry;
	u8 ssid[MAX_SSID_LEN];
	int ssid_len;
	u8 bssid[ETH_ALEN];

	ssid_len = wpa_drv_get_ssid(wpa_s, ssid);
	if (ssid_len < 0) {
		wpa_printf(MSG_WARNING, "Could not read SSID from driver.");
		return NULL;
	}

	if (wpa_drv_get_bssid(wpa_s, bssid) < 0) {
		wpa_printf(MSG_WARNING, "Could not read BSSID from driver.");
		return NULL;
	}

	entry = wpa_s->conf->ssid;
	while (entry) {
		if (ssid_len == entry->ssid_len &&
		    memcmp(ssid, entry->ssid, ssid_len) == 0 &&
		    (!entry->bssid_set ||
		     memcmp(bssid, entry->bssid, ETH_ALEN) == 0))
			return entry;
		entry = entry->next;
	}

	return NULL;
}


static void wpa_eapol_key_mic(u8 *key, int ver, u8 *buf, size_t len, u8 *mic)
{
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
		hmac_md5(key, 16, buf, len, mic);
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		u8 hash[SHA1_MAC_LEN];
		hmac_sha1(key, 16, buf, len, hash);
		memcpy(mic, hash, MD5_MAC_LEN);
	}
}


void wpa_supplicant_key_request(struct wpa_supplicant *wpa_s,
				int error, int pairwise)
{
	int rlen;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *reply;
	unsigned char *rbuf;
	struct l2_ethhdr *ethhdr;
	int key_info, ver;
	u8 bssid[ETH_ALEN];

	if (wpa_s->pairwise_cipher == WPA_CIPHER_CCMP)
		ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	else
		ver = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;

	if (wpa_drv_get_bssid(wpa_s, bssid) < 0) {
		wpa_printf(MSG_WARNING, "Failed to read BSSID for EAPOL-Key "
			   "request");
		return;
	}

	rlen = sizeof(*ethhdr) + sizeof(*hdr) + sizeof(*reply);
	rbuf = malloc(rlen);
	if (rbuf == NULL)
		return;

	memset(rbuf, 0, rlen);
	ethhdr = (struct l2_ethhdr *) rbuf;
	memcpy(ethhdr->h_dest, bssid, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_EAPOL);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(sizeof(*reply));

	reply = (struct wpa_eapol_key *) (hdr + 1);
	reply->type = wpa_s->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info = WPA_KEY_INFO_REQUEST | ver;
	if (wpa_s->ptk_set)
		key_info |= WPA_KEY_INFO_MIC;
	if (error)
		key_info |= WPA_KEY_INFO_ERROR;
	if (pairwise)
		key_info |= WPA_KEY_INFO_KEY_TYPE;
	reply->key_info = host_to_be16(key_info);
	reply->key_length = 0;
	memcpy(reply->replay_counter, wpa_s->request_counter,
	       WPA_REPLAY_COUNTER_LEN);
	inc_byte_array(wpa_s->request_counter, WPA_REPLAY_COUNTER_LEN);

	reply->key_data_length = host_to_be16(0);

	if (key_info & WPA_KEY_INFO_MIC) {
		wpa_eapol_key_mic(wpa_s->ptk.mic_key, ver, (u8 *) hdr,
			 rlen - sizeof(*ethhdr), reply->key_mic);
	}

	wpa_printf(MSG_INFO, "WPA: Sending EAPOL-Key Request (error=%d "
		   "pairwise=%d ptk_set=%d len=%d)",
		   error, pairwise, wpa_s->ptk_set, rlen);
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key Request", rbuf, rlen);
	l2_packet_send(wpa_s->l2, rbuf, rlen);
	eapol_sm_notify_tx_eapol_key(wpa_s->eapol);
	free(rbuf);
}


static void wpa_supplicant_process_1_of_4(struct wpa_supplicant *wpa_s,
					  unsigned char *src_addr,
					  struct wpa_eapol_key *key, int ver)
{
	int rlen;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *reply;
	unsigned char *rbuf;
	struct l2_ethhdr *ethhdr;
	struct wpa_ssid *ssid;
	struct wpa_ptk *ptk;
	u8 buf[8], wpa_ie_buf[80], *wpa_ie, *pmkid = NULL;
	int wpa_ie_len;
	int abort_cached = 0;

	wpa_s->wpa_state = WPA_4WAY_HANDSHAKE;
	wpa_printf(MSG_DEBUG, "WPA: RX message 1 of 4-Way Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL) {
		wpa_printf(MSG_WARNING, "WPA: No SSID info found (msg 1 of "
			   "4).");
		return;
	}

	if (wpa_s->proto == WPA_PROTO_RSN) {
		/* RSN: msg 1/4 should contain PMKID for the selected PMK */
		u8 *pos = (u8 *) (key + 1);
		u8 *end = pos + be_to_host16(key->key_data_length);
		wpa_hexdump(MSG_DEBUG, "RSN: msg 1/4 key data",
			    pos, be_to_host16(key->key_data_length));
		while (pos + 1 < end) {
			if (pos + 2 + pos[1] > end) {
				wpa_printf(MSG_DEBUG, "RSN: key data "
					   "underflow (ie=%d len=%d)",
					   pos[0], pos[1]);
				break;
			}
			if (pos[0] == GENERIC_INFO_ELEM &&
			    pos + 1 + RSN_SELECTOR_LEN < end &&
			    pos[1] >= RSN_SELECTOR_LEN + PMKID_LEN &&
			    memcmp(pos + 2, RSN_KEY_DATA_PMKID,
				   RSN_SELECTOR_LEN) == 0) {
				pmkid = pos + 2 + RSN_SELECTOR_LEN;
				wpa_hexdump(MSG_DEBUG, "RSN: PMKID from "
					    "Authenticator", pmkid, PMKID_LEN);
				break;
			} else if (pos[0] == GENERIC_INFO_ELEM &&
				   pos[1] == 0)
				break;
			pos += 2 + pos[1];
		}
	}

	if (wpa_s->assoc_wpa_ie) {
		/* The driver reported a WPA IE that may be different from the
		 * one that the Supplicant would use. Message 2/4 has to use
		 * the exact copy of the WPA IE from the Association Request,
		 * so use the value reported by the driver. */
		wpa_ie = wpa_s->assoc_wpa_ie;
		wpa_ie_len = wpa_s->assoc_wpa_ie_len;
	} else {
		wpa_ie = wpa_ie_buf;
		wpa_ie_len = wpa_gen_wpa_ie(wpa_s, wpa_ie);
		if (wpa_ie_len < 0) {
			wpa_printf(MSG_WARNING, "WPA: Failed to generate "
				   "WPA IE (for msg 2 of 4).");
			return;
		}
		wpa_hexdump(MSG_DEBUG, "WPA: WPA IE for msg 2/4",
			    wpa_ie, wpa_ie_len);
	}

	rlen = sizeof(*ethhdr) + sizeof(*hdr) + sizeof(*reply) + wpa_ie_len;
	rbuf = malloc(rlen);
	if (rbuf == NULL)
		return;

	memset(rbuf, 0, rlen);
	ethhdr = (struct l2_ethhdr *) rbuf;
	memcpy(ethhdr->h_dest, src_addr, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_EAPOL);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(sizeof(*reply) + wpa_ie_len);

	reply = (struct wpa_eapol_key *) (hdr + 1);
	reply->type = wpa_s->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	reply->key_info = host_to_be16(ver | WPA_KEY_INFO_KEY_TYPE |
				       WPA_KEY_INFO_MIC);
	reply->key_length = key->key_length;
	memcpy(reply->replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);

	reply->key_data_length = host_to_be16(wpa_ie_len);
	memcpy(reply + 1, wpa_ie, wpa_ie_len);

	if (wpa_s->renew_snonce) {
		if (hostapd_get_rand(wpa_s->snonce, WPA_NONCE_LEN)) {
			wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to get "
				"random data for SNonce");
			return;
		}
		wpa_s->renew_snonce = 0;
		wpa_hexdump(MSG_DEBUG, "WPA: Renewed SNonce",
			    wpa_s->snonce, WPA_NONCE_LEN);
	}
	memcpy(reply->key_nonce, wpa_s->snonce, WPA_NONCE_LEN);
	ptk = &wpa_s->tptk;
	memcpy(wpa_s->anonce, key->key_nonce, WPA_NONCE_LEN);
	if (pmkid && !wpa_s->cur_pmksa) {
		/* When using drivers that generate RSN IE, wpa_supplicant may
		 * not have enough time to get the association information
		 * event before receiving this 1/4 message, so try to find a
		 * matching PMKSA cache entry here. */
		wpa_s->cur_pmksa = pmksa_cache_get(wpa_s, src_addr, pmkid);
		if (wpa_s->cur_pmksa) {
			wpa_printf(MSG_DEBUG, "RSN: found matching PMKID from "
				   "PMKSA cache");
		} else {
			wpa_printf(MSG_DEBUG, "RSN: no matching PMKID found");
			abort_cached = 1;
		}
	}

	if (pmkid && wpa_s->cur_pmksa &&
	    memcmp(pmkid, wpa_s->cur_pmksa->pmkid, PMKID_LEN) == 0) {
		wpa_hexdump(MSG_DEBUG, "RSN: matched PMKID", pmkid, PMKID_LEN);
		memcpy(wpa_s->pmk, wpa_s->cur_pmksa->pmk, PMK_LEN);
		wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from PMKSA cache",
				wpa_s->pmk, PMK_LEN);
		eapol_sm_notify_cached(wpa_s->eapol);
	} else if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X && wpa_s->eapol) {
		int res, pmk_len;
		pmk_len = PMK_LEN;
		res = eapol_sm_get_key(wpa_s->eapol, wpa_s->pmk, PMK_LEN);
#ifdef EAP_LEAP
		if (res) {
			res = eapol_sm_get_key(wpa_s->eapol, wpa_s->pmk, 16);
			pmk_len = 16;
		}
#endif /* EAP_LEAP */
		if (res == 0) {
			wpa_hexdump_key(MSG_DEBUG, "WPA: PMK from EAPOL state "
					"machines", wpa_s->pmk, pmk_len);
			wpa_s->pmk_len = pmk_len;
			pmksa_cache_add(wpa_s, wpa_s->pmk, pmk_len, src_addr,
					wpa_s->own_addr);
			if (!wpa_s->cur_pmksa && pmkid &&
			    pmksa_cache_get(wpa_s, src_addr, pmkid)) {
				wpa_printf(MSG_DEBUG, "RSN: the new PMK "
					   "matches with the PMKID");
				abort_cached = 0;
			}
		} else {
			wpa_msg(wpa_s, MSG_WARNING,
				"WPA: Failed to get master session key from "
				"EAPOL state machines");
			wpa_msg(wpa_s, MSG_WARNING,
				"WPA: Key handshake aborted");
			if (wpa_s->cur_pmksa) {
				wpa_printf(MSG_DEBUG, "RSN: Cancelled PMKSA "
					   "caching attempt");
				wpa_s->cur_pmksa = NULL;
				abort_cached = 1;
			} else {
				return;
			}
		}
#ifdef CONFIG_XSUPPLICANT_IFACE
	} else if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X &&
		   !wpa_s->ext_pmk_received) {
		wpa_printf(MSG_INFO, "WPA: Master session has not yet "
			   "been received from the external IEEE "
			   "802.1X Supplicant - ignoring WPA "
			   "EAPOL-Key frame");
		return;
#endif /* CONFIG_XSUPPLICANT_IFACE */
	}

	if (abort_cached && wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		/* Send EAPOL-Start to trigger full EAP authentication. */
		wpa_printf(MSG_DEBUG, "RSN: no PMKSA entry found - trigger "
			   "full EAP authenication");
		wpa_eapol_send(wpa_s, IEEE802_1X_TYPE_EAPOL_START,
			       (u8 *) "", 0);
		return;
	}

	wpa_pmk_to_ptk(wpa_s->pmk, wpa_s->pmk_len, wpa_s->own_addr, src_addr,
		       wpa_s->snonce, key->key_nonce,
		       (u8 *) ptk, sizeof(*ptk));
	/* Supplicant: swap tx/rx Mic keys */
	memcpy(buf, ptk->u.auth.tx_mic_key, 8);
	memcpy(ptk->u.auth.tx_mic_key, ptk->u.auth.rx_mic_key, 8);
	memcpy(ptk->u.auth.rx_mic_key, buf, 8);
	wpa_s->tptk_set = 1;
	wpa_eapol_key_mic(wpa_s->tptk.mic_key, ver, (u8 *) hdr,
			  rlen - sizeof(*ethhdr), reply->key_mic);
	wpa_hexdump(MSG_DEBUG, "WPA: EAPOL-Key MIC", reply->key_mic, 16);

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 2/4");
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key 2/4", rbuf, rlen);
	l2_packet_send(wpa_s->l2, rbuf, rlen);
	eapol_sm_notify_tx_eapol_key(wpa_s->eapol);
	free(rbuf);
}


static void wpa_supplicant_key_neg_complete(struct wpa_supplicant *wpa_s,
					    u8 *addr, int secure)
{
	wpa_msg(wpa_s, MSG_INFO, "WPA: Key negotiation completed with "
		MACSTR " [PTK=%s GTK=%s]", MAC2STR(addr),
		wpa_cipher_txt(wpa_s->pairwise_cipher),
		wpa_cipher_txt(wpa_s->group_cipher));
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
	wpa_supplicant_cancel_auth_timeout(wpa_s);
	wpa_s->wpa_state = WPA_COMPLETED;

	if (secure) {
		/* MLME.SETPROTECTION.request(TA, Tx_Rx) */
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK)
			eapol_sm_notify_eap_success(wpa_s->eapol, TRUE);
		rsn_preauth_candidate_process(wpa_s);
	}
}


static int wpa_supplicant_install_ptk(struct wpa_supplicant *wpa_s,
				      unsigned char *src_addr,
				      struct wpa_eapol_key *key)
{
	int alg, keylen, rsclen;
	u8 *key_rsc;
	u8 null_rsc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	wpa_printf(MSG_DEBUG, "WPA: Installing PTK to the driver.");

	switch (wpa_s->pairwise_cipher) {
	case WPA_CIPHER_CCMP:
		alg = WPA_ALG_CCMP;
		keylen = 16;
		rsclen = 6;
		break;
	case WPA_CIPHER_TKIP:
		alg = WPA_ALG_TKIP;
		keylen = 32;
		rsclen = 6;
		break;
	case WPA_CIPHER_NONE:
		wpa_printf(MSG_DEBUG, "WPA: Pairwise Cipher Suite: "
			   "NONE - do not use pairwise keys");
		return 0;
	default:
		wpa_printf(MSG_WARNING, "WPA: Unsupported pairwise cipher %d",
			   wpa_s->pairwise_cipher);
		return -1;
	}

	if (wpa_s->proto == WPA_PROTO_RSN) {
		key_rsc = null_rsc;
	} else {
		key_rsc = key->key_rsc;
		wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, rsclen);
	}

	wpa_s->keys_cleared = 0;
	if (wpa_drv_set_key(wpa_s, alg, src_addr, 0, 1, key_rsc, rsclen,
			    (u8 *) &wpa_s->ptk.tk1, keylen) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set PTK to the "
			   "driver.");
		return -1;
	}
	return 0;
}


static int wpa_supplicant_check_group_cipher(struct wpa_supplicant *wpa_s,
					     int keylen, int maxkeylen,
					     int *key_rsc_len, int *alg)
{
	switch (wpa_s->group_cipher) {
	case WPA_CIPHER_CCMP:
		if (keylen != 16 || maxkeylen < 16) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported CCMP Group "
				   "Cipher key length %d (%d).",
				   keylen, maxkeylen);
			return -1;
		}
		*key_rsc_len = 6;
		*alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_TKIP:
		if (keylen != 32 || maxkeylen < 32) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported TKIP Group "
				   "Cipher key length %d (%d).",
				   keylen, maxkeylen);
			return -1;
		}
		*key_rsc_len = 6;
		*alg = WPA_ALG_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		if (keylen != 13 || maxkeylen < 13) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported WEP104 Group"
				   " Cipher key length %d (%d).",
				   keylen, maxkeylen);
			return -1;
		}
		*key_rsc_len = 0;
		*alg = WPA_ALG_WEP;
		break;
	case WPA_CIPHER_WEP40:
		if (keylen != 5 || maxkeylen < 5) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported WEP40 Group "
				   "Cipher key length %d (%d).",
				   keylen, maxkeylen);
			return -1;
		}
		*key_rsc_len = 0;
		*alg = WPA_ALG_WEP;
		break;
	default:
		wpa_printf(MSG_WARNING, "WPA: Unsupport Group Cipher %d",
			   wpa_s->group_cipher);
		return -1;
	}

	return 0;
}


static int wpa_supplicant_install_gtk(struct wpa_supplicant *wpa_s,
				      struct wpa_eapol_key *key, int alg,
				      u8 *gtk, int gtk_len, int keyidx,
				      int key_rsc_len, int tx)
{
	wpa_hexdump_key(MSG_DEBUG, "WPA: Group Key", gtk, gtk_len);
	wpa_printf(MSG_DEBUG, "WPA: Installing GTK to the driver "
		   "(keyidx=%d tx=%d).", keyidx, tx);
	wpa_hexdump(MSG_DEBUG, "WPA: RSC", key->key_rsc, key_rsc_len);
	if (wpa_s->group_cipher == WPA_CIPHER_TKIP) {
		/* Swap Tx/Rx keys for Michael MIC */
		u8 tmpbuf[8];
		memcpy(tmpbuf, gtk + 16, 8);
		memcpy(gtk + 16, gtk + 24, 8);
		memcpy(gtk + 24, tmpbuf, 8);
	}
	wpa_s->keys_cleared = 0;
	if (wpa_s->pairwise_cipher == WPA_CIPHER_NONE) {
		if (wpa_drv_set_key(wpa_s, alg,
				    (u8 *) "\xff\xff\xff\xff\xff\xff",
				    keyidx, 1, key->key_rsc, key_rsc_len,
				    gtk, gtk_len) < 0) {
			wpa_printf(MSG_WARNING, "WPA: Failed to set "
				   "GTK to the driver (Group only).");
			return -1;
		}
	} else if (wpa_drv_set_key(wpa_s, alg,
				   (u8 *) "\xff\xff\xff\xff\xff\xff",
				   keyidx, tx, key->key_rsc, key_rsc_len,
				   gtk, gtk_len) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set GTK to "
			   "the driver.");
		return -1;
	}

	return 0;
}


static int wpa_supplicant_pairwise_gtk(struct wpa_supplicant *wpa_s,
				       unsigned char *src_addr,
				       struct wpa_eapol_key *key,
				       u8 *gtk, int gtk_len, int key_info)
{
	int keyidx, tx, key_rsc_len = 0, alg;

	wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in pairwise handshake",
			gtk, gtk_len);

	keyidx = gtk[0] & 0x3;
	tx = !!(gtk[0] & BIT(2));
	if (tx && wpa_s->pairwise_cipher != WPA_CIPHER_NONE) {
		/* Ignore Tx bit in GTK IE if a pairwise key is used. One AP
		 * seemed to set this bit (incorrectly, since Tx is only when
		 * doing Group Key only APs) and without this workaround, the
		 * data connection does not work because wpa_supplicant
		 * configured non-zero keyidx to be used for unicast. */
		wpa_printf(MSG_INFO, "RSN: Tx bit set for GTK IE, but "
			   "pairwise keys are used - ignore Tx bit");
		tx = 0;
	}
	gtk += 2;
	gtk_len -= 2;

	if (wpa_supplicant_check_group_cipher(wpa_s, gtk_len, gtk_len,
					      &key_rsc_len, &alg)) {
		return -1;
	}

	if (wpa_supplicant_install_gtk(wpa_s, key, alg, gtk, gtk_len, keyidx,
				       key_rsc_len, tx)) {
		return -1;
	}

	wpa_supplicant_key_neg_complete(wpa_s, src_addr,
					key_info & WPA_KEY_INFO_SECURE);
	return 0;
}


static void wpa_report_ie_mismatch(struct wpa_supplicant *wpa_s,
				   const char *reason, const u8 *src_addr,
				   const u8 *wpa_ie, size_t wpa_ie_len,
				   const u8 *rsn_ie, size_t rsn_ie_len)
{
	wpa_msg(wpa_s, MSG_WARNING, "WPA: %s (src=" MACSTR ")",
		reason, MAC2STR(src_addr));

	if (wpa_s->ap_wpa_ie) {
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in Beacon/ProbeResp",
			    wpa_s->ap_wpa_ie, wpa_s->ap_wpa_ie_len);
	}
	if (wpa_ie) {
		if (!wpa_s->ap_wpa_ie) {
			wpa_printf(MSG_INFO, "WPA: No WPA IE in "
				   "Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in 3/4 msg",
			    wpa_ie, wpa_ie_len);
	}

	if (wpa_s->ap_rsn_ie) {
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in Beacon/ProbeResp",
			    wpa_s->ap_rsn_ie, wpa_s->ap_rsn_ie_len);
	}
	if (rsn_ie) {
		if (!wpa_s->ap_rsn_ie) {
			wpa_printf(MSG_INFO, "WPA: No RSN IE in "
				   "Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in 3/4 msg",
			    rsn_ie, rsn_ie_len);
	}

	wpa_supplicant_disassociate(wpa_s, REASON_IE_IN_4WAY_DIFFERS);
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


static void wpa_supplicant_process_3_of_4(struct wpa_supplicant *wpa_s,
					  unsigned char *src_addr,
					  struct wpa_eapol_key *key,
					  int extra_len, int ver)
{
	int rlen;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *reply;
	unsigned char *rbuf;
	struct l2_ethhdr *ethhdr;
	int key_info, wpa_ie_len = 0, rsn_ie_len = 0, keylen, gtk_len = 0;
	u8 *wpa_ie = NULL, *rsn_ie = NULL, *gtk = NULL;
	u8 *pos, *end;
	u16 len;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	wpa_s->wpa_state = WPA_4WAY_HANDSHAKE;
	wpa_printf(MSG_DEBUG, "WPA: RX message 3 of 4-Way Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	key_info = be_to_host16(key->key_info);

	pos = (u8 *) (key + 1);
	len = be_to_host16(key->key_data_length);
	end = pos + len;
	wpa_hexdump(MSG_DEBUG, "WPA: IE KeyData", pos, len);
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end) {
			wpa_printf(MSG_DEBUG, "WPA: key data underflow (ie=%d "
				   "len=%d)", pos[0], pos[1]);
			break;
		}
		if (*pos == RSN_INFO_ELEM) {
			rsn_ie = pos;
			rsn_ie_len = pos[1] + 2;
		} else if (*pos == GENERIC_INFO_ELEM && pos[1] >= 6 &&
			   memcmp(pos + 2, WPA_OUI_TYPE, WPA_SELECTOR_LEN) == 0
			   && pos[2 + WPA_SELECTOR_LEN] == 1 &&
			   pos[2 + WPA_SELECTOR_LEN + 1] == 0) {
			wpa_ie = pos;
			wpa_ie_len = pos[1] + 2;
		} else if (pos[0] == GENERIC_INFO_ELEM &&
			   pos[1] > RSN_SELECTOR_LEN + 2 &&
			   memcmp(pos + 2, RSN_KEY_DATA_GROUPKEY,
				  RSN_SELECTOR_LEN) == 0) {
			if (!(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
				wpa_printf(MSG_WARNING, "WPA: GTK IE in "
					   "unencrypted key data");
				return;
			}
			gtk = pos + 2 + RSN_SELECTOR_LEN;
			gtk_len = pos[1] - RSN_SELECTOR_LEN;
		} else if (pos[0] == GENERIC_INFO_ELEM && pos[1] == 0)
			break;

		pos += 2 + pos[1];
	}

	if (wpa_s->ap_wpa_ie == NULL && wpa_s->ap_rsn_ie == NULL) {
		wpa_printf(MSG_DEBUG, "WPA: No WPA/RSN IE for this AP known. "
			   "Trying to get from scan results");
		if (wpa_supplicant_get_beacon_ie(wpa_s) < 0) {
			wpa_printf(MSG_WARNING, "WPA: Could not find AP from "
				   "the scan results");
		} else {
			wpa_printf(MSG_DEBUG, "WPA: Found the current AP from "
				   "updated scan results");
		}
	}

	if ((wpa_ie && wpa_s->ap_wpa_ie &&
	     (wpa_ie_len != wpa_s->ap_wpa_ie_len ||
	      memcmp(wpa_ie, wpa_s->ap_wpa_ie, wpa_ie_len) != 0)) ||
	    (rsn_ie && wpa_s->ap_rsn_ie &&
	     (rsn_ie_len != wpa_s->ap_rsn_ie_len ||
	      memcmp(rsn_ie, wpa_s->ap_rsn_ie, rsn_ie_len) != 0))) {
		wpa_report_ie_mismatch(wpa_s, "IE in 3/4 msg does not match "
				       "with IE in Beacon/ProbeResp",
				       src_addr, wpa_ie, wpa_ie_len,
				       rsn_ie, rsn_ie_len);
		return;
	}

	if (wpa_s->proto == WPA_PROTO_WPA &&
	    rsn_ie && wpa_s->ap_rsn_ie == NULL &&
	    ssid && (ssid->proto & WPA_PROTO_RSN)) {
		wpa_report_ie_mismatch(wpa_s, "Possible downgrade attack "
				       "detected - RSN was enabled and RSN IE "
				       "was in msg 3/4, but not in "
				       "Beacon/ProbeResp",
				       src_addr, wpa_ie, wpa_ie_len,
				       rsn_ie, rsn_ie_len);
		return;
	}

	if (memcmp(wpa_s->anonce, key->key_nonce, WPA_NONCE_LEN) != 0) {
		wpa_printf(MSG_WARNING, "WPA: ANonce from message 1 of 4-Way "
			   "Handshake differs from 3 of 4-Way Handshake - drop"
			   " packet (src=" MACSTR ")", MAC2STR(src_addr));
		return;
	}

	keylen = be_to_host16(key->key_length);
	switch (wpa_s->pairwise_cipher) {
	case WPA_CIPHER_CCMP:
		if (keylen != 16) {
			wpa_printf(MSG_WARNING, "WPA: Invalid CCMP key length "
				   "%d (src=" MACSTR ")",
				   keylen, MAC2STR(src_addr));
			return;
		}
		break;
	case WPA_CIPHER_TKIP:
		if (keylen != 32) {
			wpa_printf(MSG_WARNING, "WPA: Invalid TKIP key length "
				   "%d (src=" MACSTR ")",
				   keylen, MAC2STR(src_addr));
			return;
		}
		break;
	}

	rlen = sizeof(*ethhdr) + sizeof(*hdr) + sizeof(*reply);
	rbuf = malloc(rlen);
	if (rbuf == NULL)
		return;

	memset(rbuf, 0, rlen);
	ethhdr = (struct l2_ethhdr *) rbuf;
	memcpy(ethhdr->h_dest, src_addr, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_EAPOL);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(sizeof(*reply));

	reply = (struct wpa_eapol_key *) (hdr + 1);
	reply->type = wpa_s->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	reply->key_info = host_to_be16(ver | WPA_KEY_INFO_KEY_TYPE |
				       WPA_KEY_INFO_MIC |
				       (key_info & WPA_KEY_INFO_SECURE));
	reply->key_length = key->key_length;
	memcpy(reply->replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);

	reply->key_data_length = host_to_be16(0);

	memcpy(reply->key_nonce, wpa_s->snonce, WPA_NONCE_LEN);
	wpa_eapol_key_mic(wpa_s->ptk.mic_key, ver, (u8 *) hdr,
			  rlen - sizeof(*ethhdr), reply->key_mic);

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 4/4");
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key 4/4", rbuf, rlen);
	l2_packet_send(wpa_s->l2, rbuf, rlen);
	eapol_sm_notify_tx_eapol_key(wpa_s->eapol);
	free(rbuf);

	/* SNonce was successfully used in msg 3/4, so mark it to be renewed
	 * for the next 4-Way Handshake. If msg 3 is received again, the old
	 * SNonce will still be used to avoid changing PTK. */
	wpa_s->renew_snonce = 1;

	if (key_info & WPA_KEY_INFO_INSTALL) {
		wpa_supplicant_install_ptk(wpa_s, src_addr, key);
	}

	if (key_info & WPA_KEY_INFO_SECURE) {
		/* MLME.SETPROTECTION.request(TA, Tx_Rx) */
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
	}
	wpa_s->wpa_state = WPA_GROUP_HANDSHAKE;

	if (gtk) {
		wpa_supplicant_pairwise_gtk(wpa_s, src_addr, key,
					    gtk, gtk_len, key_info);
	}
}


static void wpa_supplicant_process_1_of_2(struct wpa_supplicant *wpa_s,
					  unsigned char *src_addr,
					  struct wpa_eapol_key *key,
					  int extra_len, int ver)
{
	int rlen;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *reply;
	unsigned char *rbuf;
	struct l2_ethhdr *ethhdr;
	int key_info, keylen, keydatalen, maxkeylen, keyidx, key_rsc_len = 0;
	int alg, tx, rekey;
	u8 ek[32], gtk[32];
	u8 *gtk_ie = NULL;
	size_t gtk_ie_len = 0;

	rekey = wpa_s->wpa_state == WPA_COMPLETED;
	wpa_s->wpa_state = WPA_GROUP_HANDSHAKE;
	wpa_printf(MSG_DEBUG, "WPA: RX message 1 of Group Key Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	key_info = be_to_host16(key->key_info);
	keydatalen = be_to_host16(key->key_data_length);

	if (wpa_s->proto == WPA_PROTO_RSN) {
		u8 *pos = (u8 *) (key + 1);
		u8 *end = pos + keydatalen;
		while (pos + 1 < end) {
			if (pos + 2 + pos[1] > end) {
				wpa_printf(MSG_DEBUG, "RSN: key data "
					   "underflow (ie=%d len=%d)",
					   pos[0], pos[1]);
				break;
			}
			if (pos[0] == GENERIC_INFO_ELEM &&
			    pos + 1 + RSN_SELECTOR_LEN < end &&
			    pos[1] > RSN_SELECTOR_LEN + 2 &&
			    memcmp(pos + 2, RSN_KEY_DATA_GROUPKEY,
				   RSN_SELECTOR_LEN) == 0) {
				if (!(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
					wpa_printf(MSG_WARNING, "WPA: GTK IE "
						   "in unencrypted key data");
					return;
				}
				gtk_ie = pos + 2 + RSN_SELECTOR_LEN;
				gtk_ie_len = pos[1] - RSN_SELECTOR_LEN;
				break;
			} else if (pos[0] == GENERIC_INFO_ELEM &&
				   pos[1] == 0)
				break;

			pos += 2 + pos[1];
		}

		if (gtk_ie == NULL) {
			wpa_printf(MSG_INFO, "WPA: No GTK IE in Group Key "
				   "message 1/2");
			return;
		}
		maxkeylen = keylen = gtk_ie_len - 2;
	} else {
		keylen = be_to_host16(key->key_length);
		maxkeylen = keydatalen;
		if (keydatalen > extra_len) {
			wpa_printf(MSG_INFO, "WPA: Truncated EAPOL-Key packet:"
				   " key_data_length=%d > extra_len=%d",
				   keydatalen, extra_len);
			return;
		}
		if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES)
			maxkeylen -= 8;
	}

	if (wpa_supplicant_check_group_cipher(wpa_s, keylen, maxkeylen,
					      &key_rsc_len, &alg)) {
		return;
	}

	if (wpa_s->proto == WPA_PROTO_RSN) {
		wpa_hexdump(MSG_DEBUG,
			    "RSN: received GTK in group key handshake",
			    gtk_ie, gtk_ie_len);
		keyidx = gtk_ie[0] & 0x3;
		tx = !!(gtk_ie[0] & BIT(2));
		if (gtk_ie_len - 2 > sizeof(gtk)) {
			wpa_printf(MSG_INFO, "RSN: Too long GTK in GTK IE "
				   "(len=%lu)",
				   (unsigned long) gtk_ie_len - 2);
			return;
		}
		memcpy(gtk, gtk_ie + 2, gtk_ie_len - 2);
	} else {
		keyidx = (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
			WPA_KEY_INFO_KEY_INDEX_SHIFT;
		if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
			memcpy(ek, key->key_iv, 16);
			memcpy(ek + 16, wpa_s->ptk.encr_key, 16);
			rc4_skip(ek, 32, 256, (u8 *) (key + 1), keydatalen);
			memcpy(gtk, key + 1, keylen);
		} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
			if (keydatalen % 8) {
				wpa_printf(MSG_WARNING, "WPA: Unsupported "
					   "AES-WRAP len %d", keydatalen);
				return;
			}
			if (aes_unwrap(wpa_s->ptk.encr_key, maxkeylen / 8,
				       (u8 *) (key + 1), gtk)) {
				wpa_printf(MSG_WARNING, "WPA: AES unwrap "
					   "failed - could not decrypt GTK");
				return;
			}
		}
		tx = !!(key_info & WPA_KEY_INFO_TXRX);
	}

	if (tx && wpa_s->pairwise_cipher != WPA_CIPHER_NONE) {
		/* Ignore Tx bit in Group Key message if a pairwise key
		 * is used. Some APs seem to setting this bit
		 * (incorrectly, since Tx is only when doing Group Key
		 * only APs) and without this workaround, the data
		 * connection does not work because wpa_supplicant
		 * configured non-zero keyidx to be used for unicast.
		 */
		wpa_printf(MSG_INFO, "WPA: Tx bit set for GTK, but "
			   "pairwise keys are used - ignore Tx bit");
		tx = 0;
	}

	wpa_supplicant_install_gtk(wpa_s, key, alg, gtk, keylen, keyidx,
				   key_rsc_len, tx);

	rlen = sizeof(*ethhdr) + sizeof(*hdr) + sizeof(*reply);
	rbuf = malloc(rlen);
	if (rbuf == NULL)
		return;

	memset(rbuf, 0, rlen);
	ethhdr = (struct l2_ethhdr *) rbuf;
	memcpy(ethhdr->h_dest, src_addr, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_EAPOL);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = IEEE802_1X_TYPE_EAPOL_KEY;
	hdr->length = htons(sizeof(*reply));

	reply = (struct wpa_eapol_key *) (hdr + 1);
	reply->type = wpa_s->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	reply->key_info =
		host_to_be16(ver | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE |
			     (key_info & WPA_KEY_INFO_KEY_INDEX_MASK));
	reply->key_length = key->key_length;
	memcpy(reply->replay_counter, key->replay_counter,
		WPA_REPLAY_COUNTER_LEN);

	reply->key_data_length = host_to_be16(0);

	wpa_eapol_key_mic(wpa_s->ptk.mic_key, ver, (u8 *) hdr,
			  rlen - sizeof(*ethhdr), reply->key_mic);

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 2/2");
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key 2/2", rbuf, rlen);
	l2_packet_send(wpa_s->l2, rbuf, rlen);
	eapol_sm_notify_tx_eapol_key(wpa_s->eapol);
	free(rbuf);

	if (rekey) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Group rekeying completed with "
			MACSTR " [GTK=%s]", MAC2STR(src_addr),
			wpa_cipher_txt(wpa_s->group_cipher));
		wpa_s->wpa_state = WPA_COMPLETED;
	} else {
		wpa_supplicant_key_neg_complete(wpa_s, src_addr,
						key_info &
						WPA_KEY_INFO_SECURE);
	}
}


static int wpa_supplicant_verify_eapol_key_mic(struct wpa_supplicant *wpa_s,
					       struct wpa_eapol_key *key,
					       int ver, u8 *buf, size_t len)
{
	u8 mic[16];
	int ok = 0;

	memcpy(mic, key->key_mic, 16);
	if (wpa_s->tptk_set) {
		memset(key->key_mic, 0, 16);
		wpa_eapol_key_mic(wpa_s->tptk.mic_key, ver, buf, len,
				  key->key_mic);
		if (memcmp(mic, key->key_mic, 16) != 0) {
			wpa_printf(MSG_WARNING, "WPA: Invalid EAPOL-Key MIC "
				   "when using TPTK - ignoring TPTK");
		} else {
			ok = 1;
			wpa_s->tptk_set = 0;
			wpa_s->ptk_set = 1;
			memcpy(&wpa_s->ptk, &wpa_s->tptk, sizeof(wpa_s->ptk));
		}
	}

	if (!ok && wpa_s->ptk_set) {
		memset(key->key_mic, 0, 16);
		wpa_eapol_key_mic(wpa_s->ptk.mic_key, ver, buf, len,
				  key->key_mic);
		if (memcmp(mic, key->key_mic, 16) != 0) {
			wpa_printf(MSG_WARNING, "WPA: Invalid EAPOL-Key MIC "
				   "- dropping packet");
			return -1;
		}
		ok = 1;
	}

	if (!ok) {
		wpa_printf(MSG_WARNING, "WPA: Could not verify EAPOL-Key MIC "
			   "- dropping packet");
		return -1;
	}

	memcpy(wpa_s->rx_replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);
	wpa_s->rx_replay_counter_set = 1;
	return 0;
}


/* Decrypt RSN EAPOL-Key key data (RC4 or AES-WRAP) */
static int wpa_supplicant_decrypt_key_data(struct wpa_supplicant *wpa_s,
					   struct wpa_eapol_key *key, int ver)
{
	int keydatalen = be_to_host16(key->key_data_length);

	wpa_hexdump(MSG_DEBUG, "RSN: encrypted key data",
		    (u8 *) (key + 1), keydatalen);
	if (!wpa_s->ptk_set) {
		wpa_printf(MSG_WARNING, "WPA: PTK not available, "
			   "cannot decrypt EAPOL-Key key data.");
		return -1;
	}

	/* Decrypt key data here so that this operation does not need
	 * to be implemented separately for each message type. */
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
		u8 ek[32];
		memcpy(ek, key->key_iv, 16);
		memcpy(ek + 16, wpa_s->ptk.encr_key, 16);
		rc4_skip(ek, 32, 256, (u8 *) (key + 1), keydatalen);
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		u8 *buf;
		if (keydatalen % 8) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported "
				   "AES-WRAP len %d", keydatalen);
			return -1;
		}
		keydatalen -= 8; /* AES-WRAP adds 8 bytes */
		buf = malloc(keydatalen);
		if (buf == NULL) {
			wpa_printf(MSG_WARNING, "WPA: No memory for "
				   "AES-UNWRAP buffer");
			return -1;
		}
		if (aes_unwrap(wpa_s->ptk.encr_key, keydatalen / 8,
			       (u8 *) (key + 1), buf)) {
			free(buf);
			wpa_printf(MSG_WARNING, "WPA: AES unwrap failed - "
				   "could not decrypt EAPOL-Key key data");
			return -1;
		}
		memcpy(key + 1, buf, keydatalen);
		free(buf);
		key->key_data_length = host_to_be16(keydatalen);
	}
	wpa_hexdump_key(MSG_DEBUG, "WPA: decrypted EAPOL-Key key data",
			(u8 *) (key + 1), keydatalen);
	return 0;
}


static void wpa_sm_rx_eapol(struct wpa_supplicant *wpa_s,
			    unsigned char *src_addr, unsigned char *buf,
			    size_t len)
{
	size_t plen, data_len, extra_len;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	int key_info, ver;

	hdr = (struct ieee802_1x_hdr *) buf;
	key = (struct wpa_eapol_key *) (hdr + 1);
	if (len < sizeof(*hdr) + sizeof(*key)) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame too short, len %lu, "
			   "expecting at least %lu",
			   (unsigned long) len,
			   (unsigned long) sizeof(*hdr) + sizeof(*key));
		return;
	}
	plen = ntohs(hdr->length);
	data_len = plen + sizeof(*hdr);
	wpa_printf(MSG_DEBUG, "IEEE 802.1X RX: version=%d type=%d length=%lu",
		   hdr->version, hdr->type, (unsigned long) plen);

	wpa_drv_poll(wpa_s);

	if (hdr->version < EAPOL_VERSION) {
		/* TODO: backwards compatibility */
	}
	if (hdr->type != IEEE802_1X_TYPE_EAPOL_KEY) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame (type %u) discarded, "
			"not a Key frame", hdr->type);
		if (wpa_s->cur_pmksa) {
			wpa_printf(MSG_DEBUG, "WPA: Cancelling PMKSA caching "
				   "attempt - attempt full EAP "
				   "authentication");
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 0);
		}
		return;
	}
	if (plen > len - sizeof(*hdr) || plen < sizeof(*key)) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame payload size %lu "
			   "invalid (frame size %lu)",
			   (unsigned long) plen, (unsigned long) len);
		return;
	}

	wpa_printf(MSG_DEBUG, "  EAPOL-Key type=%d", key->type);
	if (key->type != EAPOL_KEY_TYPE_WPA && key->type != EAPOL_KEY_TYPE_RSN)
	{
		wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key type (%d) unknown, "
			   "discarded", key->type);
		return;
	}

	wpa_hexdump(MSG_MSGDUMP, "WPA: RX EAPOL-Key", buf, len);
	if (data_len < len) {
		wpa_printf(MSG_DEBUG, "WPA: ignoring %lu bytes after the IEEE "
			   "802.1X data", (unsigned long) len - data_len);
	}
	key_info = be_to_host16(key->key_info);
	ver = key_info & WPA_KEY_INFO_TYPE_MASK;
	if (ver != WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_printf(MSG_INFO, "WPA: Unsupported EAPOL-Key descriptor "
			   "version %d.", ver);
		return;
	}

	if (wpa_s->pairwise_cipher == WPA_CIPHER_CCMP &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_printf(MSG_INFO, "WPA: CCMP is used, but EAPOL-Key "
			   "descriptor version (%d) is not 2.", ver);
		if (wpa_s->group_cipher != WPA_CIPHER_CCMP &&
		    !(key_info & WPA_KEY_INFO_KEY_TYPE)) {
			/* Earlier versions of IEEE 802.11i did not explicitly
			 * require version 2 descriptor for all EAPOL-Key
			 * packets, so allow group keys to use version 1 if
			 * CCMP is not used for them. */
			wpa_printf(MSG_INFO, "WPA: Backwards compatibility: "
				   "allow invalid version for non-CCMP group "
				   "keys");
		} else
			return;
	}

	if (wpa_s->rx_replay_counter_set &&
	    memcmp(key->replay_counter, wpa_s->rx_replay_counter,
		   WPA_REPLAY_COUNTER_LEN) <= 0) {
		wpa_printf(MSG_WARNING, "WPA: EAPOL-Key Replay Counter did not"
			   " increase - dropping packet");
		return;
	}

	if (!(key_info & WPA_KEY_INFO_ACK)) {
		wpa_printf(MSG_INFO, "WPA: No Ack bit in key_info");
		return;
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		wpa_printf(MSG_INFO, "WPA: EAPOL-Key with Request bit - "
			   "dropped");
		return;
	}

	if ((key_info & WPA_KEY_INFO_MIC) &&
	    wpa_supplicant_verify_eapol_key_mic(wpa_s, key, ver, buf,
						data_len))
		return;

	extra_len = data_len - sizeof(*hdr) - sizeof(*key);

	if (be_to_host16(key->key_data_length) > extra_len) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Invalid EAPOL-Key frame - "
			"key_data overflow (%d > %d)",
			be_to_host16(key->key_data_length), extra_len);
		return;
	}

	if (wpa_s->proto == WPA_PROTO_RSN &&
	    (key_info & WPA_KEY_INFO_ENCR_KEY_DATA) &&
	    wpa_supplicant_decrypt_key_data(wpa_s, key, ver))
		return;

	if (key_info & WPA_KEY_INFO_KEY_TYPE) {
		if (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) {
			wpa_printf(MSG_WARNING, "WPA: Ignored EAPOL-Key "
				   "(Pairwise) with non-zero key index");
			return;
		}
		if (key_info & WPA_KEY_INFO_MIC) {
			/* 3/4 4-Way Handshake */
			wpa_supplicant_process_3_of_4(wpa_s, src_addr, key,
						      extra_len, ver);
		} else {
			/* 1/4 4-Way Handshake */
			wpa_supplicant_process_1_of_4(wpa_s, src_addr, key,
						      ver);
		}
	} else {
		if (key_info & WPA_KEY_INFO_MIC) {
			/* 1/2 Group Key Handshake */
			wpa_supplicant_process_1_of_2(wpa_s, src_addr, key,
						      extra_len, ver);
		} else {
			wpa_printf(MSG_WARNING, "WPA: EAPOL-Key (Group) "
				   "without Mic bit - dropped");
		}
	}
}


void wpa_supplicant_rx_eapol(void *ctx, unsigned char *src_addr,
			     unsigned char *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "RX EAPOL from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX EAPOL", buf, len);

	if (wpa_s->eapol_received == 0) {
		/* Timeout for completing IEEE 802.1X and WPA authentication */
		wpa_supplicant_req_auth_timeout(
			wpa_s,
			(wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X ||
			 wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) ?
			70 : 10, 0);
	}
	wpa_s->eapol_received++;

	if (wpa_s->countermeasures) {
		wpa_printf(MSG_INFO, "WPA: Countermeasures - dropped EAPOL "
			   "packet");
		return;
	}

	/* Source address of the incoming EAPOL frame could be compared to the
	 * current BSSID. However, it is possible that a centralized
	 * Authenticator could be using another MAC address than the BSSID of
	 * an AP, so just allow any address to be used for now. The replies are
	 * still sent to the current BSSID (if available), though. */

	memcpy(wpa_s->last_eapol_src, src_addr, ETH_ALEN);
	eapol_sm_rx_eapol(wpa_s->eapol, src_addr, buf, len);
	wpa_sm_rx_eapol(wpa_s, src_addr, buf, len);
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


static const u8 * wpa_key_mgmt_suite(struct wpa_supplicant *wpa_s)
{
	static const u8 *dummy = (u8 *) "\x00\x00\x00\x00";
	switch (wpa_s->key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_UNSPEC_802_1X :
			WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
	case WPA_KEY_MGMT_PSK:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X :
			WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	case WPA_KEY_MGMT_WPA_NONE:
		return WPA_AUTH_KEY_MGMT_NONE;
	default:
		return dummy;
	}
}


static const u8 * wpa_cipher_suite(struct wpa_supplicant *wpa_s, int cipher)
{
	static const u8 *dummy = (u8 *) "\x00\x00\x00\x00";
	switch (cipher) {
	case WPA_CIPHER_CCMP:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_CCMP : WPA_CIPHER_SUITE_CCMP);
	case WPA_CIPHER_TKIP:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_TKIP : WPA_CIPHER_SUITE_TKIP);
	case WPA_CIPHER_WEP104:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_WEP104 : WPA_CIPHER_SUITE_WEP104);
	case WPA_CIPHER_WEP40:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_WEP40 : WPA_CIPHER_SUITE_WEP40);
	case WPA_CIPHER_NONE:
		return (wpa_s->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_NONE : WPA_CIPHER_SUITE_NONE);
	default:
		return dummy;
	}
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) (s)[0], (s)[1], (s)[2], (s)[3]

int wpa_get_mib(struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	int len, i;
	char pmkid_txt[PMKID_LEN * 2 + 1];

	if (wpa_s->cur_pmksa) {
		char *pos = pmkid_txt;
		for (i = 0; i < PMKID_LEN; i++) {
			pos += sprintf(pos, "%02x",
				       wpa_s->cur_pmksa->pmkid[i]);
		}
	} else
		pmkid_txt[0] = '\0';

	len = snprintf(buf, buflen,
		       "dot11RSNAConfigVersion=%d\n"
		       "dot11RSNAConfigPairwiseKeysSupported=5\n"
		       "dot11RSNAConfigGroupCipherSize=%d\n"
		       "dot11RSNAConfigPMKLifetime=%d\n"
		       "dot11RSNAConfigPMKReauthThreshold=%d\n"
		       "dot11RSNAConfigNumberOfPTKSAReplayCounters=1\n"
		       "dot11RSNAConfigSATimeout=%d\n"
		       "dot11RSNAAuthenticationSuiteSelected=" RSN_SUITE "\n"
		       "dot11RSNAPairwiseCipherSelected=" RSN_SUITE "\n"
		       "dot11RSNAGroupCipherSelected=" RSN_SUITE "\n"
		       "dot11RSNAPMKIDUsed=%s\n"
		       "dot11RSNAAuthenticationSuiteRequested=" RSN_SUITE "\n"
		       "dot11RSNAPairwiseCipherRequested=" RSN_SUITE "\n"
		       "dot11RSNAGroupCipherRequested=" RSN_SUITE "\n"
		       "dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n",
		       RSN_VERSION,
		       wpa_cipher_bits(wpa_s->group_cipher),
		       dot11RSNAConfigPMKLifetime,
		       dot11RSNAConfigPMKReauthThreshold,
		       dot11RSNAConfigSATimeout,
		       RSN_SUITE_ARG(wpa_key_mgmt_suite(wpa_s)),
		       RSN_SUITE_ARG(wpa_cipher_suite(wpa_s,
						      wpa_s->pairwise_cipher)),
		       RSN_SUITE_ARG(wpa_cipher_suite(wpa_s,
						      wpa_s->group_cipher)),
		       pmkid_txt,
		       RSN_SUITE_ARG(wpa_key_mgmt_suite(wpa_s)),
		       RSN_SUITE_ARG(wpa_cipher_suite(wpa_s,
						      wpa_s->pairwise_cipher)),
		       RSN_SUITE_ARG(wpa_cipher_suite(wpa_s,
						      wpa_s->group_cipher)));
	return len;
}


#ifdef IEEE8021X_EAPOL

static void rsn_preauth_receive(void *ctx, unsigned char *src_addr,
				unsigned char *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "RX pre-auth from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX pre-auth", buf, len);

	if (wpa_s->preauth_eapol == NULL ||
	    memcmp(wpa_s->preauth_bssid, "\x00\x00\x00\x00\x00\x00",
		   ETH_ALEN) == 0 ||
	    memcmp(wpa_s->preauth_bssid, src_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_WARNING, "RSN pre-auth frame received from "
			   "unexpected source " MACSTR " - dropped",
			   MAC2STR(src_addr));
		return;
	}

	eapol_sm_rx_eapol(wpa_s->preauth_eapol, src_addr, buf, len);
}


static void rsn_preauth_eapol_cb(struct eapol_sm *eapol, int success,
				 void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	u8 pmk[PMK_LEN];

	wpa_msg(wpa_s, MSG_INFO, "RSN: pre-authentication with " MACSTR
		" %s", MAC2STR(wpa_s->preauth_bssid),
		success ? "completed successfully" : "failed");

	if (success) {
		int res, pmk_len;
		pmk_len = PMK_LEN;
		res = eapol_sm_get_key(eapol, pmk, PMK_LEN);
#ifdef EAP_LEAP
		if (res) {
			res = eapol_sm_get_key(eapol, pmk, 16);
			pmk_len = 16;
		}
#endif /* EAP_LEAP */
		if (res == 0) {
			wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from pre-auth",
					pmk, pmk_len);
			wpa_s->pmk_len = pmk_len;
			pmksa_cache_add(wpa_s, pmk, pmk_len,
					wpa_s->preauth_bssid, wpa_s->own_addr);
		} else {
			wpa_msg(wpa_s, MSG_INFO, "RSN: failed to get master "
				"session key from pre-auth EAPOL state "
				"machines");
		}
	}

	rsn_preauth_deinit(wpa_s);
	rsn_preauth_candidate_process(wpa_s);
}


static void rsn_preauth_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_msg(wpa_s, MSG_INFO, "RSN: pre-authentication with " MACSTR
		" timed out", MAC2STR(wpa_s->preauth_bssid));
	rsn_preauth_deinit(wpa_s);
	rsn_preauth_candidate_process(wpa_s);
}


int rsn_preauth_init(struct wpa_supplicant *wpa_s, u8 *dst)
{
	struct eapol_config eapol_conf;
	struct eapol_ctx *ctx;

	if (wpa_s->preauth_eapol)
		return -1;

	wpa_msg(wpa_s, MSG_DEBUG, "RSN: starting pre-authentication with "
		MACSTR, MAC2STR(dst));

	wpa_s->l2_preauth = l2_packet_init(wpa_s->ifname,
					   wpa_drv_get_mac_addr(wpa_s),
					   ETH_P_RSN_PREAUTH,
					   rsn_preauth_receive, wpa_s);
	if (wpa_s->l2_preauth == NULL) {
		wpa_printf(MSG_WARNING, "RSN: Failed to initialize L2 packet "
			   "processing for pre-authentication");
		return -2;
	}

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_WARNING, "Failed to allocate EAPOL context.");
		return -4;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = wpa_s;
	ctx->preauth = 1;
	ctx->cb = rsn_preauth_eapol_cb;
	ctx->cb_ctx = wpa_s;
	ctx->scard_ctx = wpa_s->scard;
	ctx->eapol_done_cb = wpa_supplicant_notify_eapol_done;
	ctx->eapol_send = wpa_eapol_send_preauth;

	wpa_s->preauth_eapol = eapol_sm_init(ctx);
	if (wpa_s->preauth_eapol == NULL) {
		free(ctx);
		wpa_printf(MSG_WARNING, "RSN: Failed to initialize EAPOL "
			   "state machines for pre-authentication");
		return -3;
	}
	memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.accept_802_1x_keys = 0;
	eapol_conf.required_keys = 0;
	eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	if (wpa_s->current_ssid)
		eapol_conf.workaround = wpa_s->current_ssid->eap_workaround;
	eapol_sm_notify_config(wpa_s->preauth_eapol, wpa_s->current_ssid,
			       &eapol_conf);
	memcpy(wpa_s->preauth_bssid, dst, ETH_ALEN);

	eapol_sm_notify_portValid(wpa_s->preauth_eapol, TRUE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(wpa_s->preauth_eapol, TRUE);

	eloop_register_timeout(60, 0, rsn_preauth_timeout, wpa_s, NULL);

	return 0;
}


void rsn_preauth_deinit(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->preauth_eapol)
		return;

	eloop_cancel_timeout(rsn_preauth_timeout, wpa_s, NULL);
	eapol_sm_deinit(wpa_s->preauth_eapol);
	wpa_s->preauth_eapol = NULL;
	memset(wpa_s->preauth_bssid, 0, ETH_ALEN);

	l2_packet_deinit(wpa_s->l2_preauth);
	wpa_s->l2_preauth = NULL;
}


static void rsn_preauth_candidate_process(struct wpa_supplicant *wpa_s)
{
	struct rsn_pmksa_candidate *candidate;

	if (wpa_s->pmksa_candidates == NULL)
		return;

	/* TODO: drop priority for old candidate entries */

	wpa_msg(wpa_s, MSG_DEBUG, "RSN: processing PMKSA candidate list");
	if (wpa_s->preauth_eapol ||
	    wpa_s->proto != WPA_PROTO_RSN ||
	    wpa_s->wpa_state != WPA_COMPLETED ||
	    wpa_s->key_mgmt != WPA_KEY_MGMT_IEEE8021X) {
		wpa_msg(wpa_s, MSG_DEBUG, "RSN: not in suitable state for new "
			"pre-authentication");
		return; /* invalid state for new pre-auth */
	}

	while (wpa_s->pmksa_candidates) {
		struct rsn_pmksa_cache *p = NULL;
		candidate = wpa_s->pmksa_candidates;
		p = pmksa_cache_get(wpa_s, candidate->bssid, NULL);
		if (memcmp(wpa_s->bssid, candidate->bssid, ETH_ALEN) != 0 &&
		    p == NULL) {
			wpa_msg(wpa_s, MSG_DEBUG, "RSN: PMKSA candidate "
				MACSTR " selected for pre-authentication",
				MAC2STR(candidate->bssid));
			wpa_s->pmksa_candidates = candidate->next;
			rsn_preauth_init(wpa_s, candidate->bssid);
			free(candidate);
			return;
		}
		wpa_msg(wpa_s, MSG_DEBUG, "RSN: PMKSA candidate " MACSTR
			" does not need pre-authentication anymore",
			MAC2STR(candidate->bssid));
		/* Some drivers (e.g., NDIS) expect to get notified about the
		 * PMKIDs again, so report the existing data now. */
		if (p)
			wpa_drv_add_pmkid(wpa_s, candidate->bssid, p->pmkid);

		wpa_s->pmksa_candidates = candidate->next;
		free(candidate);
	}
	wpa_msg(wpa_s, MSG_DEBUG, "RSN: no more pending PMKSA candidates");
}


void pmksa_candidate_add(struct wpa_supplicant *wpa_s, const u8 *bssid,
			 int prio)
{
	struct rsn_pmksa_candidate *cand, *prev, *pos;

	/* If BSSID already on candidate list, update the priority of the old
	 * entry. Do not override priority based on normal scan results. */
	prev = NULL;
	cand = wpa_s->pmksa_candidates;
	while (cand) {
		if (memcmp(cand->bssid, bssid, ETH_ALEN) == 0) {
			if (prev)
				prev->next = cand->next;
			else
				wpa_s->pmksa_candidates = cand->next;
			break;
		}
		prev = cand;
		cand = cand->next;
	}

	if (cand) {
		if (prio < PMKID_CANDIDATE_PRIO_SCAN)
			cand->priority = prio;
	} else {
		cand = malloc(sizeof(*cand));
		if (cand == NULL)
			return;
		memset(cand, 0, sizeof(*cand));
		memcpy(cand->bssid, bssid, ETH_ALEN);
		cand->priority = prio;
	}

	/* Add candidate to the list; order by increasing priority value. i.e.,
	 * highest priority (smallest value) first. */
	prev = NULL;
	pos = wpa_s->pmksa_candidates;
	while (pos) {
		if (cand->priority <= pos->priority)
			break;
		prev = pos;
		pos = pos->next;
	}
	cand->next = pos;
	if (prev)
		prev->next = cand;
	else
		wpa_s->pmksa_candidates = cand;

	wpa_msg(wpa_s, MSG_DEBUG, "RSN: added PMKSA cache "
		"candidate " MACSTR " prio %d", MAC2STR(bssid), prio);
	rsn_preauth_candidate_process(wpa_s);
}


/* TODO: schedule periodic scans if current AP supports preauth */
void rsn_preauth_scan_results(struct wpa_supplicant *wpa_s,
			      struct wpa_scan_result *results, int count)
{
	struct wpa_scan_result *r;
	struct wpa_ie_data ie;
	int i;

	if (wpa_s->current_ssid == NULL)
		return;

	pmksa_candidate_free(wpa_s);

	for (i = count - 1; i >= 0; i--) {
		r = &results[i];
		if (r->ssid_len == wpa_s->current_ssid->ssid_len &&
		    memcmp(r->ssid, wpa_s->current_ssid->ssid, r->ssid_len) ==
		    0 &&
		    memcmp(r->bssid, wpa_s->bssid, ETH_ALEN) != 0 &&
		    r->rsn_ie_len > 0 &&
		    wpa_parse_wpa_ie(wpa_s, r->rsn_ie, r->rsn_ie_len, &ie) ==
		    0 &&
		    (ie.capabilities & WPA_CAPABILITY_PREAUTH) &&
		    pmksa_cache_get(wpa_s, r->bssid, NULL) == NULL) {
			/* Give less priority to candidates found from normal
			 * scan results. */
			pmksa_candidate_add(wpa_s, r->bssid,
					    PMKID_CANDIDATE_PRIO_SCAN);
		}
	}
}

#else /* IEEE8021X_EAPOL */

static void rsn_preauth_candidate_process(struct wpa_supplicant *wpa_s)
{
}

#endif /* IEEE8021X_EAPOL */
