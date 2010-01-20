/*
 * WPA Supplicant - WPA state machine and EAPOL-Key processing
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
#ifndef CONFIG_NATIVE_WINDOWS
#include <netinet/in.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <string.h>

#include "common.h"
#include "md5.h"
#include "sha1.h"
#include "rc4.h"
#include "aes_wrap.h"
#include "wpa.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "l2_packet.h"
#include "eapol_sm.h"
#include "preauth.h"
#include "wpa_i.h"


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
 *    (default: unspec 802.1X)
 * WPA Capabilities (2 octets, little endian) (default: 0)
 */

struct wpa_ie_hdr {
	u8 elem_id;
	u8 len;
	u8 oui[3];
	u8 oui_type;
	u8 version[2];
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
 *    (default: unspec 802.1X)
 * RSN Capabilities (2 octets, little endian) (default: 0)
 * PMKID Count (2 octets) (default: 0)
 * PMKID List (16 * n octets)
 */

struct rsn_ie_hdr {
	u8 elem_id; /* WLAN_EID_RSN */
	u8 len;
	u8 version[2];
} __attribute__ ((packed));


struct wpa_eapol_key {
	u8 type;
	/* Note: key_info, key_length, and key_data_length are unaligned */
	u8 key_info[2];
	u8 key_length[2];
	u8 replay_counter[WPA_REPLAY_COUNTER_LEN];
	u8 key_nonce[WPA_NONCE_LEN];
	u8 key_iv[16];
	u8 key_rsc[8];
	u8 key_id[8]; /* Reserved in IEEE 802.11i/RSN */
	u8 key_mic[16];
	u8 key_data_length[2];
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
#define WPA_KEY_INFO_ENCR_KEY_DATA BIT(12) /* IEEE 802.11i/RSN only */



/**
 * wpa_cipher_txt - Convert cipher suite to a text string
 * @cipher: Cipher suite (WPA_CIPHER_* enum)
 * Returns: Pointer to a text string of the cipher suite name
 */
static const char * wpa_cipher_txt(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE:
		return "NONE";
	case WPA_CIPHER_WEP40:
		return "WEP-40";
	case WPA_CIPHER_WEP104:
		return "WEP-104";
	case WPA_CIPHER_TKIP:
		return "TKIP";
	case WPA_CIPHER_CCMP:
		return "CCMP";
	default:
		return "UNKNOWN";
	}
}


/**
 * wpa_key_mgmt_txt - Convert key management suite to a text string
 * @key_mgmt: Key management suite (WPA_KEY_MGMT_* enum)
 * @proto: WPA/WPA2 version (WPA_PROTO_*)
 * Returns: Pointer to a text string of the key management suite name
 */
static const char * wpa_key_mgmt_txt(int key_mgmt, int proto)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
		return proto == WPA_PROTO_RSN ?
			"WPA2/IEEE 802.1X/EAP" : "WPA/IEEE 802.1X/EAP";
	case WPA_KEY_MGMT_PSK:
		return proto == WPA_PROTO_RSN ?
			"WPA2-PSK" : "WPA-PSK";
	case WPA_KEY_MGMT_NONE:
		return "NONE";
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA:
		return "IEEE 802.1X (no WPA)";
	default:
		return "UNKNOWN";
	}
}


static int wpa_selector_to_bitfield(const u8 *s)
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


static int wpa_key_mgmt_to_bitfield(const u8 *s)
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


static int rsn_selector_to_bitfield(const u8 *s)
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


static int rsn_key_mgmt_to_bitfield(const u8 *s)
{
	if (memcmp(s, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN) == 0)
		return WPA_KEY_MGMT_IEEE8021X;
	if (memcmp(s, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X, RSN_SELECTOR_LEN) ==
	    0)
		return WPA_KEY_MGMT_PSK;
	return 0;
}


static int wpa_parse_wpa_ie_wpa(const u8 *wpa_ie, size_t wpa_ie_len,
				struct wpa_ie_data *data)
{
	const struct wpa_ie_hdr *hdr;
	const u8 *pos;
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

	hdr = (const struct wpa_ie_hdr *) wpa_ie;

	if (hdr->elem_id != GENERIC_INFO_ELEM ||
	    hdr->len != wpa_ie_len - 2 ||
	    memcmp(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN) != 0 ||
	    WPA_GET_LE16(hdr->version) != WPA_VERSION) {
		wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
			   __func__);
		return -1;
	}

	pos = (const u8 *) (hdr + 1);
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
		count = WPA_GET_LE16(pos);
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
		count = WPA_GET_LE16(pos);
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
		data->capabilities = WPA_GET_LE16(pos);
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


static int wpa_parse_wpa_ie_rsn(const u8 *rsn_ie, size_t rsn_ie_len,
				struct wpa_ie_data *data)
{
	const struct rsn_ie_hdr *hdr;
	const u8 *pos;
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

	hdr = (const struct rsn_ie_hdr *) rsn_ie;

	if (hdr->elem_id != RSN_INFO_ELEM ||
	    hdr->len != rsn_ie_len - 2 ||
	    WPA_GET_LE16(hdr->version) != RSN_VERSION) {
		wpa_printf(MSG_DEBUG, "%s: malformed ie or unknown version",
			   __func__);
		return -1;
	}

	pos = (const u8 *) (hdr + 1);
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
		count = WPA_GET_LE16(pos);
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
		count = WPA_GET_LE16(pos);
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
		data->capabilities = WPA_GET_LE16(pos);
		pos += 2;
		left -= 2;
	}

	if (left >= 2) {
		data->num_pmkid = WPA_GET_LE16(pos);
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


/**
 * wpa_parse_wpa_ie - Parse WPA/RSN IE
 * @wpa_ie: Pointer to WPA or RSN IE
 * @wpa_ie_len: Length of the WPA/RSN IE
 * @data: Pointer to data area for parsing results
 * Returns: 0 on success, -1 on failure
 *
 * Parse the contents of WPA or RSN IE and write the parsed data into data.
 */
int wpa_parse_wpa_ie(const u8 *wpa_ie, size_t wpa_ie_len,
		     struct wpa_ie_data *data)
{
	if (wpa_ie_len >= 1 && wpa_ie[0] == RSN_INFO_ELEM)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	else
		return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}


static int wpa_gen_wpa_ie_wpa(u8 *wpa_ie, size_t wpa_ie_len,
			      int pairwise_cipher, int group_cipher,
			      int key_mgmt)
{
	u8 *pos;
	struct wpa_ie_hdr *hdr;

	if (wpa_ie_len < sizeof(*hdr) + WPA_SELECTOR_LEN +
	    2 + WPA_SELECTOR_LEN + 2 + WPA_SELECTOR_LEN)
		return -1;

	hdr = (struct wpa_ie_hdr *) wpa_ie;
	hdr->elem_id = GENERIC_INFO_ELEM;
	memcpy(&hdr->oui, WPA_OUI_TYPE, WPA_SELECTOR_LEN);
	WPA_PUT_LE16(hdr->version, WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	if (group_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_WEP104) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP104, WPA_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_WEP40) {
		memcpy(pos, WPA_CIPHER_SUITE_WEP40, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (pairwise_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, WPA_CIPHER_SUITE_CCMP, WPA_SELECTOR_LEN);
	} else if (pairwise_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, WPA_CIPHER_SUITE_TKIP, WPA_SELECTOR_LEN);
	} else if (pairwise_cipher == WPA_CIPHER_NONE) {
		memcpy(pos, WPA_CIPHER_SUITE_NONE, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X, WPA_SELECTOR_LEN);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       WPA_SELECTOR_LEN);
	} else if (key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		memcpy(pos, WPA_AUTH_KEY_MGMT_NONE, WPA_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - wpa_ie) - 2;

	WPA_ASSERT(pos - wpa_ie <= wpa_ie_len);

	return pos - wpa_ie;
}


static int wpa_gen_wpa_ie_rsn(u8 *rsn_ie, size_t rsn_ie_len,
			      int pairwise_cipher, int group_cipher,
			      int key_mgmt, struct wpa_sm *sm)
{
	u8 *pos;
	struct rsn_ie_hdr *hdr;

	if (rsn_ie_len < sizeof(*hdr) + RSN_SELECTOR_LEN +
	    2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN + 2 +
	    (sm->cur_pmksa ? 2 + PMKID_LEN : 0))
		return -1;

	hdr = (struct rsn_ie_hdr *) rsn_ie;
	hdr->elem_id = RSN_INFO_ELEM;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	if (group_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_WEP104) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP104, RSN_SELECTOR_LEN);
	} else if (group_cipher == WPA_CIPHER_WEP40) {
		memcpy(pos, RSN_CIPHER_SUITE_WEP40, RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (pairwise_cipher == WPA_CIPHER_CCMP) {
		memcpy(pos, RSN_CIPHER_SUITE_CCMP, RSN_SELECTOR_LEN);
	} else if (pairwise_cipher == WPA_CIPHER_TKIP) {
		memcpy(pos, RSN_CIPHER_SUITE_TKIP, RSN_SELECTOR_LEN);
	} else if (pairwise_cipher == WPA_CIPHER_NONE) {
		memcpy(pos, RSN_CIPHER_SUITE_NONE, RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X, RSN_SELECTOR_LEN);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		memcpy(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X,
		       RSN_SELECTOR_LEN);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	*pos++ = 0;
	*pos++ = 0;

	if (sm->cur_pmksa) {
		/* PMKID Count (2 octets, little endian) */
		*pos++ = 1;
		*pos++ = 0;
		/* PMKID */
		memcpy(pos, sm->cur_pmksa->pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	hdr->len = (pos - rsn_ie) - 2;

	WPA_ASSERT(pos - rsn_ie <= rsn_ie_len);

	return pos - rsn_ie;
}


/**
 * wpa_gen_wpa_ie - Generate WPA/RSN IE based on current security policy
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @wpa_ie: Pointer to memory area for the generated WPA/RSN IE
 * @wpa_ie_len: Maximum length of the generated WPA/RSN IE
 * Returns: Length of the generated WPA/RSN IE or -1 on failure
 */
static int wpa_gen_wpa_ie(struct wpa_sm *sm, u8 *wpa_ie, size_t wpa_ie_len)
{
	if (sm->proto == WPA_PROTO_RSN)
		return wpa_gen_wpa_ie_rsn(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt, sm);
	else
		return wpa_gen_wpa_ie_wpa(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt);
}


/**
 * wpa_pmk_to_ptk - Calculate PTK from PMK, addresses, and nonces
 * @pmk: Pairwise master key
 * @addr1: AA or SA
 * @addr2: SA or AA
 * @nonce1: ANonce or SNonce
 * @nonce2: SNonce or ANonce
 * @ptk: Buffer for pairwise transient key
 * @ptk_len: Length of PTK
 *
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 * PTK = PRF-X(PMK, "Pairwise key expansion",
 *             Min(AA, SA) || Max(AA, SA) ||
 *             Min(ANonce, SNonce) || Max(ANonce, SNonce))
 */
static void wpa_pmk_to_ptk(const u8 *pmk, size_t pmk_len,
			   const u8 *addr1, const u8 *addr2,
			   const u8 *nonce1, const u8 *nonce2,
			   u8 *ptk, size_t ptk_len)
{
	u8 data[2 * ETH_ALEN + 2 * 32];

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


/**
 * wpa_eapol_key_mic - Calculate EAPOL-Key MIC
 * @key: EAPOL-Key Key Confirmation Key (KCK)
 * @ver: Key descriptor version (WPA_KEY_INFO_TYPE_*)
 * @buf: Pointer to the beginning of the EAPOL header (version field)
 * @len: Length of the EAPOL frame (from EAPOL header to the end of the frame)
 * @mic: Pointer to the buffer to which the EAPOL-Key MIC is written
 *
 * Calculate EAPOL-Key MIC for an EAPOL-Key packet. The EAPOL-Key MIC field has
 * to be cleared (all zeroes) when calling this function.
 *
 * Note: 'IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames' has an error in the
 * description of the Key MIC calculation. It includes packet data from the
 * beginning of the EAPOL-Key header, not EAPOL header. This incorrect change
 * happened during final editing of the standard and the correct behavior is
 * defined in the last draft (IEEE 802.11i/D10).
 */
static void wpa_eapol_key_mic(const u8 *key, int ver,
			      const u8 *buf, size_t len, u8 *mic)
{
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
		hmac_md5(key, 16, buf, len, mic);
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		u8 hash[SHA1_MAC_LEN];
		hmac_sha1(key, 16, buf, len, hash);
		memcpy(mic, hash, MD5_MAC_LEN);
	}
}


static void wpa_eapol_key_send(struct wpa_sm *sm, const u8 *kck,
			       int ver, const u8 *dest, u16 proto,
			       u8 *msg, size_t msg_len, u8 *key_mic)
{
	if (key_mic) {
		wpa_eapol_key_mic(kck, ver, msg, msg_len, key_mic);
	}
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key", msg, msg_len);
	wpa_sm_ether_send(sm, dest, proto, msg, msg_len);
	eapol_sm_notify_tx_eapol_key(sm->eapol);
	free(msg);
}


/**
 * wpa_sm_key_request - Send EAPOL-Key Request
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @error: Indicate whether this is an Michael MIC error report
 * @pairwise: 1 = error report for pairwise packet, 0 = for group packet
 * Returns: Pointer to the current network structure or %NULL on failure
 *
 * Send an EAPOL-Key Request to the current authenticator. This function is
 * used to request rekeying and it is usually called when a local Michael MIC
 * failure is detected.
 */
void wpa_sm_key_request(struct wpa_sm *sm, int error, int pairwise)
{
	size_t rlen;
	struct wpa_eapol_key *reply;
	int key_info, ver;
	u8 bssid[ETH_ALEN], *rbuf;

	if (sm->pairwise_cipher == WPA_CIPHER_CCMP)
		ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	else
		ver = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;

	if (wpa_sm_get_bssid(sm, bssid) < 0) {
		wpa_printf(MSG_WARNING, "Failed to read BSSID for EAPOL-Key "
			   "request");
		return;
	}

	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  sizeof(*reply), &rlen, (void *) &reply);
	if (rbuf == NULL)
		return;

	reply->type = sm->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info = WPA_KEY_INFO_REQUEST | ver;
	if (sm->ptk_set)
		key_info |= WPA_KEY_INFO_MIC;
	if (error)
		key_info |= WPA_KEY_INFO_ERROR;
	if (pairwise)
		key_info |= WPA_KEY_INFO_KEY_TYPE;
	WPA_PUT_BE16(reply->key_info, key_info);
	WPA_PUT_BE16(reply->key_length, 0);
	memcpy(reply->replay_counter, sm->request_counter,
	       WPA_REPLAY_COUNTER_LEN);
	inc_byte_array(sm->request_counter, WPA_REPLAY_COUNTER_LEN);

	WPA_PUT_BE16(reply->key_data_length, 0);

	wpa_printf(MSG_INFO, "WPA: Sending EAPOL-Key Request (error=%d "
		   "pairwise=%d ptk_set=%d len=%lu)",
		   error, pairwise, sm->ptk_set, (unsigned long) rlen);
	wpa_eapol_key_send(sm, sm->ptk.kck, ver, bssid, ETH_P_EAPOL,
			   rbuf, rlen, key_info & WPA_KEY_INFO_MIC ?
			   reply->key_mic : NULL);
}


struct wpa_eapol_ie_parse {
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	const u8 *rsn_ie;
	size_t rsn_ie_len;
	const u8 *pmkid;
	const u8 *gtk;
	size_t gtk_len;
};


/**
 * wpa_supplicant_parse_generic - Parse EAPOL-Key Key Data Generic IEs
 * @pos: Pointer to the IE header
 * @end: Pointer to the end of the Key Data buffer
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, 1 if end mark is found, -1 on failure
 */
static int wpa_supplicant_parse_generic(const u8 *pos, const u8 *end,
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
	}

	return 0;
}


/**
 * wpa_supplicant_parse_ies - Parse EAPOL-Key Key Data IEs
 * @buf: Pointer to the Key Data buffer
 * @len: Key Data Length
 * @ie: Pointer to parsed IE data
 * Returns: 0 on success, -1 on failure
 */
static int wpa_supplicant_parse_ies(const u8 *buf, size_t len,
				    struct wpa_eapol_ie_parse *ie)
{
	const u8 *pos, *end;
	int ret = 0;

	memset(ie, 0, sizeof(*ie));
	for (pos = buf, end = pos + len; pos + 1 < end; pos += 2 + pos[1]) {
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
			ret = wpa_supplicant_parse_generic(pos, end, ie);
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


static int wpa_supplicant_get_pmk(struct wpa_sm *sm,
				  const unsigned char *src_addr,
				  const u8 *pmkid)
{
	int abort_cached = 0;

	if (pmkid && !sm->cur_pmksa) {
		/* When using drivers that generate RSN IE, wpa_supplicant may
		 * not have enough time to get the association information
		 * event before receiving this 1/4 message, so try to find a
		 * matching PMKSA cache entry here. */
		sm->cur_pmksa = pmksa_cache_get(sm, src_addr, pmkid);
		if (sm->cur_pmksa) {
			wpa_printf(MSG_DEBUG, "RSN: found matching PMKID from "
				   "PMKSA cache");
		} else {
			wpa_printf(MSG_DEBUG, "RSN: no matching PMKID found");
			abort_cached = 1;
		}
	}

	if (pmkid && sm->cur_pmksa &&
	    memcmp(pmkid, sm->cur_pmksa->pmkid, PMKID_LEN) == 0) {
		wpa_hexdump(MSG_DEBUG, "RSN: matched PMKID", pmkid, PMKID_LEN);
		wpa_sm_set_pmk_from_pmksa(sm);
		wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from PMKSA cache",
				sm->pmk, sm->pmk_len);
		eapol_sm_notify_cached(sm->eapol);
	} else if (sm->key_mgmt == WPA_KEY_MGMT_IEEE8021X && sm->eapol) {
		int res, pmk_len;
		pmk_len = PMK_LEN;
		res = eapol_sm_get_key(sm->eapol, sm->pmk, PMK_LEN);
#ifdef EAP_LEAP
		if (res) {
			res = eapol_sm_get_key(sm->eapol, sm->pmk, 16);
			pmk_len = 16;
		}
#endif /* EAP_LEAP */
		if (res == 0) {
			wpa_hexdump_key(MSG_DEBUG, "WPA: PMK from EAPOL state "
					"machines", sm->pmk, pmk_len);
			sm->pmk_len = pmk_len;
			pmksa_cache_add(sm, sm->pmk, pmk_len, src_addr,
					sm->own_addr, sm->cur_ssid);
			if (!sm->cur_pmksa && pmkid &&
			    pmksa_cache_get(sm, src_addr, pmkid)) {
				wpa_printf(MSG_DEBUG, "RSN: the new PMK "
					   "matches with the PMKID");
				abort_cached = 0;
			}
		} else {
			wpa_msg(sm->ctx->ctx, MSG_WARNING,
				"WPA: Failed to get master session key from "
				"EAPOL state machines");
			wpa_msg(sm->ctx->ctx, MSG_WARNING,
				"WPA: Key handshake aborted");
			if (sm->cur_pmksa) {
				wpa_printf(MSG_DEBUG, "RSN: Cancelled PMKSA "
					   "caching attempt");
				sm->cur_pmksa = NULL;
				abort_cached = 1;
			} else {
				return -1;
			}
		}
	}

	if (abort_cached && sm->key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		/* Send EAPOL-Start to trigger full EAP authentication. */
		u8 *buf;
		size_t buflen;

		wpa_printf(MSG_DEBUG, "RSN: no PMKSA entry found - trigger "
			   "full EAP authentication");
		buf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_START,
					 NULL, 0, &buflen, NULL);
		if (buf) {
			wpa_sm_ether_send(sm, sm->bssid, ETH_P_EAPOL,
					  buf, buflen);
			free(buf);
		}

		return -1;
	}

	return 0;
}


static int wpa_supplicant_send_2_of_4(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      const struct wpa_eapol_key *key,
				      int ver)
{
	size_t rlen;
	struct wpa_eapol_key *reply;
	struct wpa_ptk *ptk;
	u8 buf[8], *rbuf, *wpa_ie;
	int wpa_ie_len;

	if (sm->assoc_wpa_ie == NULL) {
		wpa_printf(MSG_WARNING, "WPA: No assoc_wpa_ie set - cannot "
			   "generate msg 2/4");
		return -1;
	}

	wpa_ie = sm->assoc_wpa_ie;
	wpa_ie_len = sm->assoc_wpa_ie_len;
	wpa_hexdump(MSG_DEBUG, "WPA: WPA IE for msg 2/4", wpa_ie, wpa_ie_len);

	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY,
				  NULL, sizeof(*reply) + wpa_ie_len,
				  &rlen, (void *) &reply);
	if (rbuf == NULL)
		return -1;

	reply->type = sm->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	WPA_PUT_BE16(reply->key_info,
		     ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC);
	if (sm->proto == WPA_PROTO_RSN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		memcpy(reply->key_length, key->key_length, 2);
	memcpy(reply->replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);

	WPA_PUT_BE16(reply->key_data_length, wpa_ie_len);
	memcpy(reply + 1, wpa_ie, wpa_ie_len);

	if (sm->renew_snonce) {
		if (hostapd_get_rand(sm->snonce, WPA_NONCE_LEN)) {
			wpa_msg(sm->ctx->ctx, MSG_WARNING,
				"WPA: Failed to get random data for SNonce");
			free(rbuf);
			return -1;
		}
		sm->renew_snonce = 0;
		wpa_hexdump(MSG_DEBUG, "WPA: Renewed SNonce",
			    sm->snonce, WPA_NONCE_LEN);
	}
	memcpy(reply->key_nonce, sm->snonce, WPA_NONCE_LEN);

	/* Calculate PTK which will be stored as a temporary PTK until it has
	 * been verified when processing message 3/4. */
	ptk = &sm->tptk;
	wpa_pmk_to_ptk(sm->pmk, sm->pmk_len, sm->own_addr, src_addr,
		       sm->snonce, key->key_nonce,
		       (u8 *) ptk, sizeof(*ptk));
	/* Supplicant: swap tx/rx Mic keys */
	memcpy(buf, ptk->u.auth.tx_mic_key, 8);
	memcpy(ptk->u.auth.tx_mic_key, ptk->u.auth.rx_mic_key, 8);
	memcpy(ptk->u.auth.rx_mic_key, buf, 8);
	sm->tptk_set = 1;

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 2/4");
	wpa_eapol_key_send(sm, ptk->kck, ver, src_addr, ETH_P_EAPOL,
			   rbuf, rlen, reply->key_mic);

	return 0;
}


static void wpa_supplicant_process_1_of_4(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  u16 ver)
{
	struct wpa_eapol_ie_parse ie;

	if (wpa_sm_get_ssid(sm) == NULL) {
		wpa_printf(MSG_WARNING, "WPA: No SSID info found (msg 1 of "
			   "4).");
		return;
	}

	wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
	wpa_printf(MSG_DEBUG, "WPA: RX message 1 of 4-Way Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	memset(&ie, 0, sizeof(ie));

	if (sm->proto == WPA_PROTO_RSN) {
		/* RSN: msg 1/4 should contain PMKID for the selected PMK */
		const u8 *buf = (const u8 *) (key + 1);
		size_t len = WPA_GET_BE16(key->key_data_length);
		wpa_hexdump(MSG_DEBUG, "RSN: msg 1/4 key data", buf, len);
		wpa_supplicant_parse_ies(buf, len, &ie);
		if (ie.pmkid) {
			wpa_hexdump(MSG_DEBUG, "RSN: PMKID from "
				    "Authenticator", ie.pmkid, PMKID_LEN);
		}
	}

	if (wpa_supplicant_get_pmk(sm, src_addr, ie.pmkid))
		return;

	if (wpa_supplicant_send_2_of_4(sm, src_addr, key, ver))
		return;

	memcpy(sm->anonce, key->key_nonce, WPA_NONCE_LEN);
}


static void wpa_sm_start_preauth(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;
	rsn_preauth_candidate_process(sm);
}


static void wpa_supplicant_key_neg_complete(struct wpa_sm *sm,
					    const u8 *addr, int secure)
{
	wpa_msg(sm->ctx->ctx, MSG_INFO, "WPA: Key negotiation completed with "
		MACSTR " [PTK=%s GTK=%s]", MAC2STR(addr),
		wpa_cipher_txt(sm->pairwise_cipher),
		wpa_cipher_txt(sm->group_cipher));
	eloop_cancel_timeout(sm->ctx->scan, sm->ctx->ctx, NULL);
	wpa_sm_cancel_auth_timeout(sm);
	wpa_sm_set_state(sm, WPA_COMPLETED);

	if (secure) {
		/* MLME.SETPROTECTION.request(TA, Tx_Rx) */
		eapol_sm_notify_portValid(sm->eapol, TRUE);
		if (sm->key_mgmt == WPA_KEY_MGMT_PSK)
			eapol_sm_notify_eap_success(sm->eapol, TRUE);
		/*
		 * Start preauthentication after a short wait to avoid a
		 * possible race condition between the data receive and key
		 * configuration after the 4-Way Handshake. This increases the
		 * likelyhood of the first preauth EAPOL-Start frame getting to
		 * the target AP.
		 */
		eloop_register_timeout(1, 0, wpa_sm_start_preauth, sm, NULL);
	}

	if (sm->cur_pmksa && sm->cur_pmksa->opportunistic) {
		wpa_printf(MSG_DEBUG, "RSN: Authenticator accepted "
			   "opportunistic PMKSA entry - marking it valid");
		sm->cur_pmksa->opportunistic = 0;
	}
}


static int wpa_supplicant_install_ptk(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      const struct wpa_eapol_key *key)
{
	int alg, keylen, rsclen;
	const u8 *key_rsc;
	u8 null_rsc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	wpa_printf(MSG_DEBUG, "WPA: Installing PTK to the driver.");

	switch (sm->pairwise_cipher) {
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
			   sm->pairwise_cipher);
		return -1;
	}

	if (sm->proto == WPA_PROTO_RSN) {
		key_rsc = null_rsc;
	} else {
		key_rsc = key->key_rsc;
		wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, rsclen);
	}

	if (wpa_sm_set_key(sm, alg, src_addr, 0, 1, key_rsc, rsclen,
			   (u8 *) &sm->ptk.tk1, keylen) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set PTK to the "
			   "driver.");
		return -1;
	}
	return 0;
}


static int wpa_supplicant_check_group_cipher(int group_cipher,
					     int keylen, int maxkeylen,
					     int *key_rsc_len, int *alg)
{
	int ret = 0;

	switch (group_cipher) {
	case WPA_CIPHER_CCMP:
		if (keylen != 16 || maxkeylen < 16) {
			ret = -1;
			break;
		}
		*key_rsc_len = 6;
		*alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_TKIP:
		if (keylen != 32 || maxkeylen < 32) {
			ret = -1;
			break;
		}
		*key_rsc_len = 6;
		*alg = WPA_ALG_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		if (keylen != 13 || maxkeylen < 13) {
			ret = -1;
			break;
		}
		*key_rsc_len = 0;
		*alg = WPA_ALG_WEP;
		break;
	case WPA_CIPHER_WEP40:
		if (keylen != 5 || maxkeylen < 5) {
			ret = -1;
			break;
		}
		*key_rsc_len = 0;
		*alg = WPA_ALG_WEP;
		break;
	default:
		wpa_printf(MSG_WARNING, "WPA: Unsupported Group Cipher %d",
			   group_cipher);
		return -1;
	}

	if (ret < 0 ) {
		wpa_printf(MSG_WARNING, "WPA: Unsupported %s Group Cipher key "
			   "length %d (%d).",
			   wpa_cipher_txt(group_cipher), keylen, maxkeylen);
	}

	return ret;
}


struct wpa_gtk_data {
	int alg, tx, key_rsc_len, keyidx;
	u8 gtk[32];
	int gtk_len;
};


static int wpa_supplicant_install_gtk(struct wpa_sm *sm,
				      const struct wpa_gtk_data *gd,
				      const u8 *key_rsc)
{
	const u8 *_gtk = gd->gtk;
	u8 gtk_buf[32];

	wpa_hexdump_key(MSG_DEBUG, "WPA: Group Key", gd->gtk, gd->gtk_len);
	wpa_printf(MSG_DEBUG, "WPA: Installing GTK to the driver "
		   "(keyidx=%d tx=%d).", gd->keyidx, gd->tx);
	wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, gd->key_rsc_len);
	if (sm->group_cipher == WPA_CIPHER_TKIP) {
		/* Swap Tx/Rx keys for Michael MIC */
		memcpy(gtk_buf, gd->gtk, 16);
		memcpy(gtk_buf + 16, gd->gtk + 24, 8);
		memcpy(gtk_buf + 24, gd->gtk + 16, 8);
		_gtk = gtk_buf;
	}
	if (sm->pairwise_cipher == WPA_CIPHER_NONE) {
		if (wpa_sm_set_key(sm, gd->alg,
				   (u8 *) "\xff\xff\xff\xff\xff\xff",
				   gd->keyidx, 1, key_rsc, gd->key_rsc_len,
				   _gtk, gd->gtk_len) < 0) {
			wpa_printf(MSG_WARNING, "WPA: Failed to set "
				   "GTK to the driver (Group only).");
			return -1;
		}
	} else if (wpa_sm_set_key(sm, gd->alg,
				  (u8 *) "\xff\xff\xff\xff\xff\xff",
				  gd->keyidx, gd->tx, key_rsc, gd->key_rsc_len,
				  _gtk, gd->gtk_len) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set GTK to "
			   "the driver.");
		return -1;
	}

	return 0;
}


static int wpa_supplicant_gtk_tx_bit_workaround(const struct wpa_sm *sm,
						int tx)
{
	if (tx && sm->pairwise_cipher != WPA_CIPHER_NONE) {
		/* Ignore Tx bit for GTK if a pairwise key is used. One AP
		 * seemed to set this bit (incorrectly, since Tx is only when
		 * doing Group Key only APs) and without this workaround, the
		 * data connection does not work because wpa_supplicant
		 * configured non-zero keyidx to be used for unicast. */
		wpa_printf(MSG_INFO, "WPA: Tx bit set for GTK, but pairwise "
			   "keys are used - ignore Tx bit");
		return 0;
	}
	return tx;
}


static int wpa_supplicant_pairwise_gtk(struct wpa_sm *sm,
				       const unsigned char *src_addr,
				       const struct wpa_eapol_key *key,
				       const u8 *gtk, int gtk_len,
				       int key_info)
{
	struct wpa_gtk_data gd;

	/*
	 * IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames - Figure 43x
	 * GTK KDE format:
	 * KeyID[bits 0-1], Tx [bit 2], Reserved [bits 3-7]
	 * Reserved [bits 0-7]
	 * GTK
	 */

	memset(&gd, 0, sizeof(gd));
	wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in pairwise handshake",
			gtk, gtk_len);

	if (gtk_len < 2 || gtk_len - 2 > sizeof(gd.gtk))
		return -1;

	gd.keyidx = gtk[0] & 0x3;
	gd.tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
						     !!(gtk[0] & BIT(2)));
	gtk += 2;
	gtk_len -= 2;

	memcpy(gd.gtk, gtk, gtk_len);
	gd.gtk_len = gtk_len;

	if (wpa_supplicant_check_group_cipher(sm->group_cipher,
					      gtk_len, gtk_len,
					      &gd.key_rsc_len, &gd.alg) ||
	    wpa_supplicant_install_gtk(sm, &gd, key->key_rsc)) {
		wpa_printf(MSG_DEBUG, "RSN: Failed to install GTK");
		return -1;
	}

	wpa_supplicant_key_neg_complete(sm, src_addr,
					key_info & WPA_KEY_INFO_SECURE);
	return 0;
}


static void wpa_report_ie_mismatch(struct wpa_sm *sm,
				   const char *reason, const u8 *src_addr,
				   const u8 *wpa_ie, size_t wpa_ie_len,
				   const u8 *rsn_ie, size_t rsn_ie_len)
{
	wpa_msg(sm->ctx->ctx, MSG_WARNING, "WPA: %s (src=" MACSTR ")",
		reason, MAC2STR(src_addr));

	if (sm->ap_wpa_ie) {
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in Beacon/ProbeResp",
			    sm->ap_wpa_ie, sm->ap_wpa_ie_len);
	}
	if (wpa_ie) {
		if (!sm->ap_wpa_ie) {
			wpa_printf(MSG_INFO, "WPA: No WPA IE in "
				   "Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in 3/4 msg",
			    wpa_ie, wpa_ie_len);
	}

	if (sm->ap_rsn_ie) {
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in Beacon/ProbeResp",
			    sm->ap_rsn_ie, sm->ap_rsn_ie_len);
	}
	if (rsn_ie) {
		if (!sm->ap_rsn_ie) {
			wpa_printf(MSG_INFO, "WPA: No RSN IE in "
				   "Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in 3/4 msg",
			    rsn_ie, rsn_ie_len);
	}

	wpa_sm_disassociate(sm, REASON_IE_IN_4WAY_DIFFERS);
	wpa_sm_req_scan(sm, 0, 0);
}


static int wpa_supplicant_validate_ie(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      struct wpa_eapol_ie_parse *ie)
{
	struct wpa_ssid *ssid = sm->cur_ssid;

	if (sm->ap_wpa_ie == NULL && sm->ap_rsn_ie == NULL) {
		wpa_printf(MSG_DEBUG, "WPA: No WPA/RSN IE for this AP known. "
			   "Trying to get from scan results");
		if (wpa_sm_get_beacon_ie(sm) < 0) {
			wpa_printf(MSG_WARNING, "WPA: Could not find AP from "
				   "the scan results");
		} else {
			wpa_printf(MSG_DEBUG, "WPA: Found the current AP from "
				   "updated scan results");
		}
	}

	if ((ie->wpa_ie && sm->ap_wpa_ie &&
	     (ie->wpa_ie_len != sm->ap_wpa_ie_len ||
	      memcmp(ie->wpa_ie, sm->ap_wpa_ie, ie->wpa_ie_len) != 0)) ||
	    (ie->rsn_ie && sm->ap_rsn_ie &&
	     (ie->rsn_ie_len != sm->ap_rsn_ie_len ||
	      memcmp(ie->rsn_ie, sm->ap_rsn_ie, ie->rsn_ie_len) != 0))) {
		wpa_report_ie_mismatch(sm, "IE in 3/4 msg does not match "
				       "with IE in Beacon/ProbeResp",
				       src_addr, ie->wpa_ie, ie->wpa_ie_len,
				       ie->rsn_ie, ie->rsn_ie_len);
		return -1;
	}

	if (sm->proto == WPA_PROTO_WPA &&
	    ie->rsn_ie && sm->ap_rsn_ie == NULL &&
	    ssid && (ssid->proto & WPA_PROTO_RSN)) {
		wpa_report_ie_mismatch(sm, "Possible downgrade attack "
				       "detected - RSN was enabled and RSN IE "
				       "was in msg 3/4, but not in "
				       "Beacon/ProbeResp",
				       src_addr, ie->wpa_ie, ie->wpa_ie_len,
				       ie->rsn_ie, ie->rsn_ie_len);
		return -1;
	}

	return 0;
}


static int wpa_supplicant_send_4_of_4(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      const struct wpa_eapol_key *key,
				      u16 ver, u16 key_info)
{
	size_t rlen;
	struct wpa_eapol_key *reply;
	u8 *rbuf;

	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  sizeof(*reply), &rlen, (void *) &reply);
	if (rbuf == NULL)
		return -1;

	reply->type = sm->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info &= WPA_KEY_INFO_SECURE;
	key_info |= ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC;
	WPA_PUT_BE16(reply->key_info, key_info);
	if (sm->proto == WPA_PROTO_RSN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		memcpy(reply->key_length, key->key_length, 2);
	memcpy(reply->replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);

	WPA_PUT_BE16(reply->key_data_length, 0);

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 4/4");
	wpa_eapol_key_send(sm, sm->ptk.kck, ver, src_addr, ETH_P_EAPOL,
			   rbuf, rlen, reply->key_mic);

	return 0;
}


static void wpa_supplicant_process_3_of_4(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  int extra_len, u16 ver)
{
	u16 key_info, keylen, len;
	const u8 *pos;
	struct wpa_eapol_ie_parse ie;

	wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
	wpa_printf(MSG_DEBUG, "WPA: RX message 3 of 4-Way Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	key_info = WPA_GET_BE16(key->key_info);

	pos = (const u8 *) (key + 1);
	len = WPA_GET_BE16(key->key_data_length);
	wpa_hexdump(MSG_DEBUG, "WPA: IE KeyData", pos, len);
	wpa_supplicant_parse_ies(pos, len, &ie);
	if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_printf(MSG_WARNING, "WPA: GTK IE in unencrypted key data");
		return;
	}

	if (wpa_supplicant_validate_ie(sm, src_addr, &ie) < 0)
		return;

	if (memcmp(sm->anonce, key->key_nonce, WPA_NONCE_LEN) != 0) {
		wpa_printf(MSG_WARNING, "WPA: ANonce from message 1 of 4-Way "
			   "Handshake differs from 3 of 4-Way Handshake - drop"
			   " packet (src=" MACSTR ")", MAC2STR(src_addr));
		return;
	}

	keylen = WPA_GET_BE16(key->key_length);
	switch (sm->pairwise_cipher) {
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

	if (wpa_supplicant_send_4_of_4(sm, src_addr, key, ver, key_info))
		return;

	/* SNonce was successfully used in msg 3/4, so mark it to be renewed
	 * for the next 4-Way Handshake. If msg 3 is received again, the old
	 * SNonce will still be used to avoid changing PTK. */
	sm->renew_snonce = 1;

	if (key_info & WPA_KEY_INFO_INSTALL) {
		wpa_supplicant_install_ptk(sm, src_addr, key);
	}

	if (key_info & WPA_KEY_INFO_SECURE) {
		/* MLME.SETPROTECTION.request(TA, Tx_Rx) */
		eapol_sm_notify_portValid(sm->eapol, TRUE);
	}
	wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);

	if (ie.gtk &&
	    wpa_supplicant_pairwise_gtk(sm, src_addr, key,
					ie.gtk, ie.gtk_len, key_info) < 0) {
		wpa_printf(MSG_INFO, "RSN: Failed to configure GTK");
	}
}


static int wpa_supplicant_process_1_of_2_rsn(struct wpa_sm *sm,
					     const u8 *keydata,
					     size_t keydatalen,
					     int key_info,
					     struct wpa_gtk_data *gd)
{
	int maxkeylen;
	struct wpa_eapol_ie_parse ie;

	wpa_hexdump(MSG_DEBUG, "RSN: msg 1/2 key data", keydata, keydatalen);
	wpa_supplicant_parse_ies(keydata, keydatalen, &ie);
	if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_printf(MSG_WARNING, "WPA: GTK IE in unencrypted key data");
		return -1;
	}
	if (ie.gtk == NULL) {
		wpa_printf(MSG_INFO, "WPA: No GTK IE in Group Key msg 1/2");
		return -1;
	}
	maxkeylen = gd->gtk_len = ie.gtk_len - 2;

	if (wpa_supplicant_check_group_cipher(sm->group_cipher,
					      gd->gtk_len, maxkeylen,
					      &gd->key_rsc_len, &gd->alg))
		return -1;

	wpa_hexdump(MSG_DEBUG, "RSN: received GTK in group key handshake",
		    ie.gtk, ie.gtk_len);
	gd->keyidx = ie.gtk[0] & 0x3;
	gd->tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
						      !!(ie.gtk[0] & BIT(2)));
	if (ie.gtk_len - 2 > sizeof(gd->gtk)) {
		wpa_printf(MSG_INFO, "RSN: Too long GTK in GTK IE "
			   "(len=%lu)", (unsigned long) ie.gtk_len - 2);
		return -1;
	}
	memcpy(gd->gtk, ie.gtk + 2, ie.gtk_len - 2);

	return 0;
}


static int wpa_supplicant_process_1_of_2_wpa(struct wpa_sm *sm,
					     const struct wpa_eapol_key *key,
					     size_t keydatalen, int key_info,
					     int extra_len, u16 ver,
					     struct wpa_gtk_data *gd)
{
	int maxkeylen;
	u8 ek[32];

	gd->gtk_len = WPA_GET_BE16(key->key_length);
	maxkeylen = keydatalen;
	if (keydatalen > extra_len) {
		wpa_printf(MSG_INFO, "WPA: Truncated EAPOL-Key packet:"
			   " key_data_length=%lu > extra_len=%d",
			   (unsigned long) keydatalen, extra_len);
		return -1;
	}
	if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES)
		maxkeylen -= 8;

	if (wpa_supplicant_check_group_cipher(sm->group_cipher,
					      gd->gtk_len, maxkeylen,
					      &gd->key_rsc_len, &gd->alg))
		return -1;

	gd->keyidx = (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
		WPA_KEY_INFO_KEY_INDEX_SHIFT;
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
		memcpy(ek, key->key_iv, 16);
		memcpy(ek + 16, sm->ptk.kek, 16);
		if (keydatalen > sizeof(gd->gtk)) {
			wpa_printf(MSG_WARNING, "WPA: RC4 key data "
				   "too long (%lu)",
				   (unsigned long) keydatalen);
			return -1;
		}
		memcpy(gd->gtk, key + 1, keydatalen);
		rc4_skip(ek, 32, 256, gd->gtk, keydatalen);
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		if (keydatalen % 8) {
			wpa_printf(MSG_WARNING, "WPA: Unsupported AES-WRAP "
				   "len %lu", (unsigned long) keydatalen);
			return -1;
		}
		if (maxkeylen > sizeof(gd->gtk)) {
			wpa_printf(MSG_WARNING, "WPA: AES-WRAP key data "
				   "too long (keydatalen=%lu maxkeylen=%lu)",
				   (unsigned long) keydatalen,
				   (unsigned long) maxkeylen);
			return -1;
		}
		if (aes_unwrap(sm->ptk.kek, maxkeylen / 8,
			       (const u8 *) (key + 1), gd->gtk)) {
			wpa_printf(MSG_WARNING, "WPA: AES unwrap "
				   "failed - could not decrypt GTK");
			return -1;
		}
	}
	gd->tx = wpa_supplicant_gtk_tx_bit_workaround(
		sm, !!(key_info & WPA_KEY_INFO_TXRX));
	return 0;
}


static int wpa_supplicant_send_2_of_2(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      const struct wpa_eapol_key *key,
				      int ver, u16 key_info)
{
	size_t rlen;
	struct wpa_eapol_key *reply;
	u8 *rbuf;

	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  sizeof(*reply), &rlen, (void *) &reply);
	if (rbuf == NULL)
		return -1;

	reply->type = sm->proto == WPA_PROTO_RSN ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info &= WPA_KEY_INFO_KEY_INDEX_MASK;
	key_info |= ver | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE;
	WPA_PUT_BE16(reply->key_info, key_info);
	if (sm->proto == WPA_PROTO_RSN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		memcpy(reply->key_length, key->key_length, 2);
	memcpy(reply->replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);

	WPA_PUT_BE16(reply->key_data_length, 0);

	wpa_printf(MSG_DEBUG, "WPA: Sending EAPOL-Key 2/2");
	wpa_eapol_key_send(sm, sm->ptk.kck, ver, src_addr, ETH_P_EAPOL,
			   rbuf, rlen, reply->key_mic);

	return 0;
}


static void wpa_supplicant_process_1_of_2(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  int extra_len, u16 ver)
{
	u16 key_info, keydatalen;
	int rekey;
	struct wpa_gtk_data gd;

	memset(&gd, 0, sizeof(gd));

	rekey = wpa_sm_get_state(sm) == WPA_COMPLETED;
	wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);
	wpa_printf(MSG_DEBUG, "WPA: RX message 1 of Group Key Handshake from "
		   MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	key_info = WPA_GET_BE16(key->key_info);
	keydatalen = WPA_GET_BE16(key->key_data_length);

	if (sm->proto == WPA_PROTO_RSN) {
		if (wpa_supplicant_process_1_of_2_rsn(sm,
						      (const u8 *) (key + 1),
						      keydatalen, key_info,
						      &gd))
			return;
	} else {
		if (wpa_supplicant_process_1_of_2_wpa(sm, key, keydatalen,
						      key_info, extra_len,
						      ver, &gd))
			return;
	}

	if (wpa_supplicant_install_gtk(sm, &gd, key->key_rsc) ||
	    wpa_supplicant_send_2_of_2(sm, src_addr, key, ver, key_info))
		return;

	if (rekey) {
		wpa_msg(sm->ctx->ctx, MSG_INFO, "WPA: Group rekeying "
			"completed with " MACSTR " [GTK=%s]",
			MAC2STR(src_addr), wpa_cipher_txt(sm->group_cipher));
		wpa_sm_set_state(sm, WPA_COMPLETED);
	} else {
		wpa_supplicant_key_neg_complete(sm, src_addr,
						key_info &
						WPA_KEY_INFO_SECURE);
	}
}


static int wpa_supplicant_verify_eapol_key_mic(struct wpa_sm *sm,
					       struct wpa_eapol_key *key,
					       u16 ver,
					       const u8 *buf, size_t len)
{
	u8 mic[16];
	int ok = 0;

	memcpy(mic, key->key_mic, 16);
	if (sm->tptk_set) {
		memset(key->key_mic, 0, 16);
		wpa_eapol_key_mic(sm->tptk.kck, ver, buf, len,
				  key->key_mic);
		if (memcmp(mic, key->key_mic, 16) != 0) {
			wpa_printf(MSG_WARNING, "WPA: Invalid EAPOL-Key MIC "
				   "when using TPTK - ignoring TPTK");
		} else {
			ok = 1;
			sm->tptk_set = 0;
			sm->ptk_set = 1;
			memcpy(&sm->ptk, &sm->tptk, sizeof(sm->ptk));
		}
	}

	if (!ok && sm->ptk_set) {
		memset(key->key_mic, 0, 16);
		wpa_eapol_key_mic(sm->ptk.kck, ver, buf, len,
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

	memcpy(sm->rx_replay_counter, key->replay_counter,
	       WPA_REPLAY_COUNTER_LEN);
	sm->rx_replay_counter_set = 1;
	return 0;
}


/* Decrypt RSN EAPOL-Key key data (RC4 or AES-WRAP) */
static int wpa_supplicant_decrypt_key_data(struct wpa_sm *sm,
					   struct wpa_eapol_key *key, u16 ver)
{
	u16 keydatalen = WPA_GET_BE16(key->key_data_length);

	wpa_hexdump(MSG_DEBUG, "RSN: encrypted key data",
		    (u8 *) (key + 1), keydatalen);
	if (!sm->ptk_set) {
		wpa_printf(MSG_WARNING, "WPA: PTK not available, "
			   "cannot decrypt EAPOL-Key key data.");
		return -1;
	}

	/* Decrypt key data here so that this operation does not need
	 * to be implemented separately for each message type. */
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4) {
		u8 ek[32];
		memcpy(ek, key->key_iv, 16);
		memcpy(ek + 16, sm->ptk.kek, 16);
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
		if (aes_unwrap(sm->ptk.kek, keydatalen / 8,
			       (u8 *) (key + 1), buf)) {
			free(buf);
			wpa_printf(MSG_WARNING, "WPA: AES unwrap failed - "
				   "could not decrypt EAPOL-Key key data");
			return -1;
		}
		memcpy(key + 1, buf, keydatalen);
		free(buf);
		WPA_PUT_BE16(key->key_data_length, keydatalen);
	}
	wpa_hexdump_key(MSG_DEBUG, "WPA: decrypted EAPOL-Key key data",
			(u8 *) (key + 1), keydatalen);
	return 0;
}


/**
 * wpa_sm_rx_eapol - Process received WPA EAPOL frames
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @src_addr: Source MAC address of the EAPOL packet
 * @buf: Pointer to the beginning of the EAPOL data (EAPOL header)
 * @len: Length of the EAPOL frame
 * Returns: 1 = WPA EAPOL-Key processed, 0 = not a WPA EAPOL-Key, -1 failure
 *
 * This function is called for each received EAPOL frame. Other than EAPOL-Key
 * frames can be skipped if filtering is done elsewhere. wpa_sm_rx_eapol() is
 * only processing WPA and WPA2 EAPOL-Key frames.
 *
 * The received EAPOL-Key packets are validated and valid packets are replied
 * to. In addition, key material (PTK, GTK) is configured at the end of a
 * successful key handshake.
 */
int wpa_sm_rx_eapol(struct wpa_sm *sm, const u8 *src_addr,
		    const u8 *buf, size_t len)
{
	size_t plen, data_len, extra_len;
	struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	u16 key_info, ver;
	u8 *tmp;
	int ret = -1;

	if (len < sizeof(*hdr) + sizeof(*key)) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame too short to be a WPA "
			   "EAPOL-Key (len %lu, expecting at least %lu)",
			   (unsigned long) len,
			   (unsigned long) sizeof(*hdr) + sizeof(*key));
		return 0;
	}

	tmp = malloc(len);
	if (tmp == NULL)
		return -1;
	memcpy(tmp, buf, len);

	hdr = (struct ieee802_1x_hdr *) tmp;
	key = (struct wpa_eapol_key *) (hdr + 1);
	plen = ntohs(hdr->length);
	data_len = plen + sizeof(*hdr);
	wpa_printf(MSG_DEBUG, "IEEE 802.1X RX: version=%d type=%d length=%lu",
		   hdr->version, hdr->type, (unsigned long) plen);

	if (hdr->version < EAPOL_VERSION) {
		/* TODO: backwards compatibility */
	}
	if (hdr->type != IEEE802_1X_TYPE_EAPOL_KEY) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame (type %u) discarded, "
			"not a Key frame", hdr->type);
		if (sm->cur_pmksa) {
			wpa_printf(MSG_DEBUG, "WPA: Cancelling PMKSA caching "
				   "attempt - attempt full EAP "
				   "authentication");
			eapol_sm_notify_pmkid_attempt(sm->eapol, 0);
		}
		ret = 0;
		goto out;
	}
	if (plen > len - sizeof(*hdr) || plen < sizeof(*key)) {
		wpa_printf(MSG_DEBUG, "WPA: EAPOL frame payload size %lu "
			   "invalid (frame size %lu)",
			   (unsigned long) plen, (unsigned long) len);
		ret = 0;
		goto out;
	}

	wpa_printf(MSG_DEBUG, "  EAPOL-Key type=%d", key->type);
	if (key->type != EAPOL_KEY_TYPE_WPA && key->type != EAPOL_KEY_TYPE_RSN)
	{
		wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key type (%d) unknown, "
			   "discarded", key->type);
		ret = 0;
		goto out;
	}

	eapol_sm_notify_lower_layer_success(sm->eapol);
	wpa_hexdump(MSG_MSGDUMP, "WPA: RX EAPOL-Key", tmp, len);
	if (data_len < len) {
		wpa_printf(MSG_DEBUG, "WPA: ignoring %lu bytes after the IEEE "
			   "802.1X data", (unsigned long) len - data_len);
	}
	key_info = WPA_GET_BE16(key->key_info);
	ver = key_info & WPA_KEY_INFO_TYPE_MASK;
	if (ver != WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_printf(MSG_INFO, "WPA: Unsupported EAPOL-Key descriptor "
			   "version %d.", ver);
		goto out;
	}

	if (sm->pairwise_cipher == WPA_CIPHER_CCMP &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_printf(MSG_INFO, "WPA: CCMP is used, but EAPOL-Key "
			   "descriptor version (%d) is not 2.", ver);
		if (sm->group_cipher != WPA_CIPHER_CCMP &&
		    !(key_info & WPA_KEY_INFO_KEY_TYPE)) {
			/* Earlier versions of IEEE 802.11i did not explicitly
			 * require version 2 descriptor for all EAPOL-Key
			 * packets, so allow group keys to use version 1 if
			 * CCMP is not used for them. */
			wpa_printf(MSG_INFO, "WPA: Backwards compatibility: "
				   "allow invalid version for non-CCMP group "
				   "keys");
		} else
			goto out;
	}

	if (sm->rx_replay_counter_set &&
	    memcmp(key->replay_counter, sm->rx_replay_counter,
		   WPA_REPLAY_COUNTER_LEN) <= 0) {
		wpa_printf(MSG_WARNING, "WPA: EAPOL-Key Replay Counter did not"
			   " increase - dropping packet");
		goto out;
	}

	if (!(key_info & WPA_KEY_INFO_ACK)) {
		wpa_printf(MSG_INFO, "WPA: No Ack bit in key_info");
		goto out;
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		wpa_printf(MSG_INFO, "WPA: EAPOL-Key with Request bit - "
			   "dropped");
		goto out;
	}

	if ((key_info & WPA_KEY_INFO_MIC) &&
	    wpa_supplicant_verify_eapol_key_mic(sm, key, ver, tmp, data_len))
		goto out;

	extra_len = data_len - sizeof(*hdr) - sizeof(*key);

	if (WPA_GET_BE16(key->key_data_length) > extra_len) {
		wpa_msg(sm->ctx->ctx, MSG_INFO, "WPA: Invalid EAPOL-Key "
			"frame - key_data overflow (%d > %lu)",
			WPA_GET_BE16(key->key_data_length),
			(unsigned long) extra_len);
		goto out;
	}

	if (sm->proto == WPA_PROTO_RSN &&
	    (key_info & WPA_KEY_INFO_ENCR_KEY_DATA) &&
	    wpa_supplicant_decrypt_key_data(sm, key, ver))
		goto out;

	if (key_info & WPA_KEY_INFO_KEY_TYPE) {
		if (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) {
			wpa_printf(MSG_WARNING, "WPA: Ignored EAPOL-Key "
				   "(Pairwise) with non-zero key index");
			goto out;
		}
		if (key_info & WPA_KEY_INFO_MIC) {
			/* 3/4 4-Way Handshake */
			wpa_supplicant_process_3_of_4(sm, src_addr, key,
						      extra_len, ver);
		} else {
			/* 1/4 4-Way Handshake */
			wpa_supplicant_process_1_of_4(sm, src_addr, key,
						      ver);
		}
	} else {
		if (key_info & WPA_KEY_INFO_MIC) {
			/* 1/2 Group Key Handshake */
			wpa_supplicant_process_1_of_2(sm, src_addr, key,
						      extra_len, ver);
		} else {
			wpa_printf(MSG_WARNING, "WPA: EAPOL-Key (Group) "
				   "without Mic bit - dropped");
		}
	}

	ret = 1;

out:
	free(tmp);
	return ret;
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


static const u8 * wpa_key_mgmt_suite(struct wpa_sm *sm)
{
	static const u8 *dummy = (u8 *) "\x00\x00\x00\x00";
	switch (sm->key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_UNSPEC_802_1X :
			WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
	case WPA_KEY_MGMT_PSK:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X :
			WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	case WPA_KEY_MGMT_WPA_NONE:
		return WPA_AUTH_KEY_MGMT_NONE;
	default:
		return dummy;
	}
}


static const u8 * wpa_cipher_suite(struct wpa_sm *sm, int cipher)
{
	static const u8 *dummy = (u8 *) "\x00\x00\x00\x00";
	switch (cipher) {
	case WPA_CIPHER_CCMP:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_CCMP : WPA_CIPHER_SUITE_CCMP);
	case WPA_CIPHER_TKIP:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_TKIP : WPA_CIPHER_SUITE_TKIP);
	case WPA_CIPHER_WEP104:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_WEP104 : WPA_CIPHER_SUITE_WEP104);
	case WPA_CIPHER_WEP40:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_WEP40 : WPA_CIPHER_SUITE_WEP40);
	case WPA_CIPHER_NONE:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_CIPHER_SUITE_NONE : WPA_CIPHER_SUITE_NONE);
	default:
		return dummy;
	}
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) (s)[0], (s)[1], (s)[2], (s)[3]

/**
 * wpa_sm_get_mib - Dump text list of MIB entries
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for the list
 * @buflen: Length of the buffer
 * Returns: Number of bytes written to buffer
 *
 * This function is used fetch dot11 MIB variables.
 */
int wpa_sm_get_mib(struct wpa_sm *sm, char *buf, size_t buflen)
{
	int len, i;
	char pmkid_txt[PMKID_LEN * 2 + 1];
	int rsna;

	if (sm->cur_pmksa) {
		char *pos = pmkid_txt;
		for (i = 0; i < PMKID_LEN; i++) {
			pos += sprintf(pos, "%02x", sm->cur_pmksa->pmkid[i]);
		}
	} else
		pmkid_txt[0] = '\0';

	if ((sm->key_mgmt == WPA_KEY_MGMT_PSK ||
	     sm->key_mgmt == WPA_KEY_MGMT_IEEE8021X) &&
	    sm->proto == WPA_PROTO_RSN)
		rsna = 1;
	else
		rsna = 0;

	len = snprintf(buf, buflen,
		       "dot11RSNAOptionImplemented=TRUE\n"
		       "dot11RSNAPreauthenticationImplemented=TRUE\n"
		       "dot11RSNAEnabled=%s\n"
		       "dot11RSNAPreauthenticationEnabled=%s\n"
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
		       "dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n"
		       "dot11RSNA4WayHandshakeFailures=%u\n",
		       rsna ? "TRUE" : "FALSE",
		       rsna ? "TRUE" : "FALSE",
		       RSN_VERSION,
		       wpa_cipher_bits(sm->group_cipher),
		       sm->dot11RSNAConfigPMKLifetime,
		       sm->dot11RSNAConfigPMKReauthThreshold,
		       sm->dot11RSNAConfigSATimeout,
		       RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
		       RSN_SUITE_ARG(wpa_cipher_suite(sm,
						      sm->pairwise_cipher)),
		       RSN_SUITE_ARG(wpa_cipher_suite(sm, sm->group_cipher)),
		       pmkid_txt,
		       RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
		       RSN_SUITE_ARG(wpa_cipher_suite(sm,
						      sm->pairwise_cipher)),
		       RSN_SUITE_ARG(wpa_cipher_suite(sm, sm->group_cipher)),
		       sm->dot11RSNA4WayHandshakeFailures);
	return len;
}


/**
 * wpa_sm_init - Initialize WPA state machine
 * @ctx: Context pointer for callbacks
 * Returns: Pointer to the allocated WPA state machine data
 *
 * This function is used to allocate a new WPA state machine and the returned
 * value is passed to all WPA state machine calls.
 */
struct wpa_sm * wpa_sm_init(struct wpa_sm_ctx *ctx)
{
	struct wpa_sm *sm;

	sm = malloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	memset(sm, 0, sizeof(*sm));
	sm->renew_snonce = 1;
	sm->ctx = ctx;

	sm->dot11RSNAConfigPMKLifetime = 43200;
	sm->dot11RSNAConfigPMKReauthThreshold = 70;
	sm->dot11RSNAConfigSATimeout = 60;

	return sm;
}


/**
 * wpa_sm_deinit - Deinitialize WPA state machine
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void wpa_sm_deinit(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;
	eloop_cancel_timeout(wpa_sm_start_preauth, sm, 0);
	free(sm->assoc_wpa_ie);
	free(sm->ap_wpa_ie);
	free(sm->ap_rsn_ie);
	free(sm->ctx);
	free(sm);
}


/**
 * wpa_sm_notify_assoc - Notify WPA state machine about association
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @bssid: The BSSID of the new association
 *
 * This function is called to let WPA state machine know that the connection
 * was established.
 */
void wpa_sm_notify_assoc(struct wpa_sm *sm, const u8 *bssid)
{
	if (sm == NULL)
		return;

	wpa_printf(MSG_DEBUG, "WPA: Association event - clear replay counter");
	memcpy(sm->bssid, bssid, ETH_ALEN);
	memset(sm->rx_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
	sm->rx_replay_counter_set = 0;
	sm->renew_snonce = 1;
	if (memcmp(sm->preauth_bssid, bssid, ETH_ALEN) == 0)
		rsn_preauth_deinit(sm);
}


/**
 * wpa_sm_notify_disassoc - Notify WPA state machine about disassociation
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * This function is called to let WPA state machine know that the connection
 * was lost. This will abort any existing pre-authentication session.
 */
void wpa_sm_notify_disassoc(struct wpa_sm *sm)
{
	rsn_preauth_deinit(sm);
	if (wpa_sm_get_state(sm) == WPA_4WAY_HANDSHAKE)
		sm->dot11RSNA4WayHandshakeFailures++;
}


/**
 * wpa_sm_set_pmk - Set PMK
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @pmk: The new PMK
 * @pmk_len: The length of the new PMK in bytes
 *
 * Configure the PMK for WPA state machine.
 */
void wpa_sm_set_pmk(struct wpa_sm *sm, const u8 *pmk, size_t pmk_len)
{
	if (sm == NULL)
		return;

	sm->pmk_len = pmk_len;
	memcpy(sm->pmk, pmk, pmk_len);
}


/**
 * wpa_sm_set_pmk_from_pmksa - Set PMK based on the current PMKSA
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * Take the PMK from the current PMKSA into use. If no PMKSA is active, the PMK
 * will be cleared.
 */
void wpa_sm_set_pmk_from_pmksa(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;

	if (sm->cur_pmksa) {
		sm->pmk_len = sm->cur_pmksa->pmk_len;
		memcpy(sm->pmk, sm->cur_pmksa->pmk, sm->pmk_len);
	} else {
		sm->pmk_len = PMK_LEN;
		memset(sm->pmk, 0, PMK_LEN);
	}
}


/**
 * wpa_sm_set_fast_reauth - Set fast reauthentication (EAP) enabled/disabled
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @fast_reauth: Whether fast reauthentication (EAP) is allowed
 */
void wpa_sm_set_fast_reauth(struct wpa_sm *sm, int fast_reauth)
{
	if (sm)
		sm->fast_reauth = fast_reauth;
}


/**
 * wpa_sm_set_scard_ctx - Set context pointer for smartcard callbacks
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @scard_ctx: Context pointer for smartcard related callback functions
 */
void wpa_sm_set_scard_ctx(struct wpa_sm *sm, void *scard_ctx)
{
	if (sm == NULL)
		return;
	sm->scard_ctx = scard_ctx;
	if (sm->preauth_eapol)
		eapol_sm_register_scard_ctx(sm->preauth_eapol, scard_ctx);
}


/**
 * wpa_sm_set_config - Notification of current configration change
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @config: Pointer to current network configuration
 *
 * Notify WPA state machine that configuration has changed. config will be
 * stored as a backpointer to network configuration. This can be %NULL to clear
 * the stored pointed.
 */
void wpa_sm_set_config(struct wpa_sm *sm, struct wpa_ssid *config)
{
	if (sm)
		sm->cur_ssid = config;
}


/**
 * wpa_sm_set_own_addr - Set own MAC address
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @addr: Own MAC address
 */
void wpa_sm_set_own_addr(struct wpa_sm *sm, const u8 *addr)
{
	if (sm)
		memcpy(sm->own_addr, addr, ETH_ALEN);
}


/**
 * wpa_sm_set_ifname - Set network interface name
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ifname: Interface name
 */
void wpa_sm_set_ifname(struct wpa_sm *sm, const char *ifname)
{
	if (sm)
		sm->ifname = ifname;
}


/**
 * wpa_sm_set_eapol - Set EAPOL state machine pointer
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @eapol: Pointer to EAPOL state machine allocated with eapol_sm_init()
 */
void wpa_sm_set_eapol(struct wpa_sm *sm, struct eapol_sm *eapol)
{
	if (sm)
		sm->eapol = eapol;
}


/**
 * wpa_sm_set_param - Set WPA state machine parameters
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @param: Parameter field
 * @value: Parameter value
 * Returns: 0 on success, -1 on failure
 */
int wpa_sm_set_param(struct wpa_sm *sm, enum wpa_sm_conf_params param,
		     unsigned int value)
{
	int ret = 0;

	if (sm == NULL)
		return -1;

	switch (param) {
	case RSNA_PMK_LIFETIME:
		if (value > 0)
			sm->dot11RSNAConfigPMKLifetime = value;
		else
			ret = -1;
		break;
	case RSNA_PMK_REAUTH_THRESHOLD:
		if (value > 0 && value <= 100)
			sm->dot11RSNAConfigPMKReauthThreshold = value;
		else
			ret = -1;
		break;
	case RSNA_SA_TIMEOUT:
		if (value > 0)
			sm->dot11RSNAConfigSATimeout = value;
		else
			ret = -1;
		break;
	case WPA_PARAM_PROTO:
		sm->proto = value;
		break;
	case WPA_PARAM_PAIRWISE:
		sm->pairwise_cipher = value;
		break;
	case WPA_PARAM_GROUP:
		sm->group_cipher = value;
		break;
	case WPA_PARAM_KEY_MGMT:
		sm->key_mgmt = value;
		break;
	}

	return ret;
}


/**
 * wpa_sm_get_param - Get WPA state machine parameters
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @param: Parameter field
 * Returns: Parameter value
 */
unsigned int wpa_sm_get_param(struct wpa_sm *sm, enum wpa_sm_conf_params param)
{
	if (sm == NULL)
		return 0;

	switch (param) {
	case RSNA_PMK_LIFETIME:
		return sm->dot11RSNAConfigPMKLifetime;
	case RSNA_PMK_REAUTH_THRESHOLD:
		return sm->dot11RSNAConfigPMKReauthThreshold;
	case RSNA_SA_TIMEOUT:
		return sm->dot11RSNAConfigSATimeout;
	case WPA_PARAM_PROTO:
		return sm->proto;
	case WPA_PARAM_PAIRWISE:
		return sm->pairwise_cipher;
	case WPA_PARAM_GROUP:
		return sm->group_cipher;
	case WPA_PARAM_KEY_MGMT:
		return sm->key_mgmt;
	default:
		return 0;
	}
}


/**
 * wpa_sm_get_status - Get WPA state machine
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for status information
 * @buflen: Maximum buffer length
 * @verbose: Whether to include verbose status information
 * Returns: Number of bytes written to buf.
 *
 * Query WPA state machine for status information. This function fills in
 * a text area with current status information. If the buffer (buf) is not
 * large enough, status information will be truncated to fit the buffer.
 */
int wpa_sm_get_status(struct wpa_sm *sm, char *buf, size_t buflen,
		      int verbose)
{
	char *pos = buf, *end = buf + buflen;

	pos += snprintf(pos, end - pos,
			"pairwise_cipher=%s\n"
			"group_cipher=%s\n"
			"key_mgmt=%s\n",
			wpa_cipher_txt(sm->pairwise_cipher),
			wpa_cipher_txt(sm->group_cipher),
			wpa_key_mgmt_txt(sm->key_mgmt, sm->proto));
	return pos - buf;
}


/**
 * wpa_sm_set_assoc_wpa_ie_default - Generate own WPA/RSN IE from configuration
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @wpa_ie: Pointer to buffer for WPA/RSN IE
 * @wpa_ie_len: Pointer to the length of the wpa_ie buffer
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the WPA/RSN IE used in (Re)Association
 * Request frame. The IE will be used to override the default value generated
 * with wpa_sm_set_assoc_wpa_ie_default().
 */
int wpa_sm_set_assoc_wpa_ie_default(struct wpa_sm *sm, u8 *wpa_ie,
				    size_t *wpa_ie_len)
{
	if (sm == NULL)
		return -1;

	*wpa_ie_len = wpa_gen_wpa_ie(sm, wpa_ie, *wpa_ie_len);
	if (*wpa_ie_len < 0)
		return -1;

	wpa_hexdump(MSG_DEBUG, "WPA: Set own WPA IE default",
		    wpa_ie, *wpa_ie_len);

	if (sm->assoc_wpa_ie == NULL) {
		/*
		 * Make a copy of the WPA/RSN IE so that 4-Way Handshake gets
		 * the correct version of the IE even if PMKSA caching is
		 * aborted (which would remove PMKID from IE generation).
		 */
		sm->assoc_wpa_ie = malloc(*wpa_ie_len);
		if (sm->assoc_wpa_ie == NULL)
			return -1;

		memcpy(sm->assoc_wpa_ie, wpa_ie, *wpa_ie_len);
		sm->assoc_wpa_ie_len = *wpa_ie_len;
	}

	return 0;
}


/**
 * wpa_sm_set_assoc_wpa_ie - Set own WPA/RSN IE from (Re)AssocReq
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the WPA/RSN IE used in (Re)Association
 * Request frame. The IE will be used to override the default value generated
 * with wpa_sm_set_assoc_wpa_ie_default().
 */
int wpa_sm_set_assoc_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	free(sm->assoc_wpa_ie);
	if (ie == NULL || len == 0) {
		wpa_printf(MSG_DEBUG, "WPA: clearing own WPA/RSN IE");
		sm->assoc_wpa_ie = NULL;
		sm->assoc_wpa_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set own WPA/RSN IE", ie, len);
		sm->assoc_wpa_ie = malloc(len);
		if (sm->assoc_wpa_ie == NULL)
			return -1;

		memcpy(sm->assoc_wpa_ie, ie, len);
		sm->assoc_wpa_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_set_ap_wpa_ie - Set AP WPA IE from Beacon/ProbeResp
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the WPA IE used in Beacon / Probe Response
 * frame.
 */
int wpa_sm_set_ap_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	free(sm->ap_wpa_ie);
	if (ie == NULL || len == 0) {
		wpa_printf(MSG_DEBUG, "WPA: clearing AP WPA IE");
		sm->ap_wpa_ie = NULL;
		sm->ap_wpa_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set AP WPA IE", ie, len);
		sm->ap_wpa_ie = malloc(len);
		if (sm->ap_wpa_ie == NULL)
			return -1;

		memcpy(sm->ap_wpa_ie, ie, len);
		sm->ap_wpa_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_set_ap_rsn_ie - Set AP RSN IE from Beacon/ProbeResp
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the RSN IE used in Beacon / Probe Response
 * frame.
 */
int wpa_sm_set_ap_rsn_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	free(sm->ap_rsn_ie);
	if (ie == NULL || len == 0) {
		wpa_printf(MSG_DEBUG, "WPA: clearing AP RSN IE");
		sm->ap_rsn_ie = NULL;
		sm->ap_rsn_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set AP RSN IE", ie, len);
		sm->ap_rsn_ie = malloc(len);
		if (sm->ap_rsn_ie == NULL)
			return -1;

		memcpy(sm->ap_rsn_ie, ie, len);
		sm->ap_rsn_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_parse_own_wpa_ie - Parse own WPA/RSN IE
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @data: Pointer to data area for parsing results
 * Returns: 0 on success, -1 if IE is not known, or -2 on parsing failure
 *
 * Parse the contents of the own WPA or RSN IE from (Re)AssocReq and write the
 * parsed data into data.
 */
int wpa_sm_parse_own_wpa_ie(struct wpa_sm *sm, struct wpa_ie_data *data)
{
	if (sm == NULL || sm->assoc_wpa_ie == NULL) {
		wpa_printf(MSG_DEBUG, "WPA: No WPA/RSN IE available from "
			   "association info");
		return -1;
	}
	if (wpa_parse_wpa_ie(sm->assoc_wpa_ie, sm->assoc_wpa_ie_len, data))
		return -2;
	return 0;
}
