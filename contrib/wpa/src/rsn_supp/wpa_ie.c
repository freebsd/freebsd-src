/*
 * wpa_supplicant - WPA/RSN IE and KDE processing
 * Copyright (c) 2003-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "wpa.h"
#include "pmksa_cache.h"
#include "common/ieee802_11_defs.h"
#include "wpa_i.h"
#include "wpa_ie.h"


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
	if (wpa_ie_len >= 1 && wpa_ie[0] == WLAN_EID_RSN)
		return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
	if (wpa_ie_len >= 6 && wpa_ie[0] == WLAN_EID_VENDOR_SPECIFIC &&
	    wpa_ie[1] >= 4 && WPA_GET_BE32(&wpa_ie[2]) == OSEN_IE_VENDOR_TYPE)
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
	u32 suite;

	if (wpa_ie_len < sizeof(*hdr) + WPA_SELECTOR_LEN +
	    2 + WPA_SELECTOR_LEN + 2 + WPA_SELECTOR_LEN)
		return -1;

	hdr = (struct wpa_ie_hdr *) wpa_ie;
	hdr->elem_id = WLAN_EID_VENDOR_SPECIFIC;
	RSN_SELECTOR_PUT(hdr->oui, WPA_OUI_TYPE);
	WPA_PUT_LE16(hdr->version, WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_WPA, group_cipher);
	if (suite == 0) {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	suite = wpa_cipher_to_suite(WPA_PROTO_WPA, pairwise_cipher);
	if (suite == 0 ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += WPA_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_NONE);
	} else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_CCKM);
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += WPA_SELECTOR_LEN;

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - wpa_ie) - 2;

	WPA_ASSERT((size_t) (pos - wpa_ie) <= wpa_ie_len);

	return pos - wpa_ie;
}


u16 rsn_supp_capab(struct wpa_sm *sm)
{
	u16 capab = 0;

	if (sm->wmm_enabled) {
		/* Advertise 16 PTKSA replay counters when using WMM */
		capab |= RSN_NUM_REPLAY_COUNTERS_16 << 2;
	}
	if (sm->mfp)
		capab |= WPA_CAPABILITY_MFPC;
	if (sm->mfp == 2)
		capab |= WPA_CAPABILITY_MFPR;
	if (sm->ocv)
		capab |= WPA_CAPABILITY_OCVC;
	if (sm->ext_key_id)
		capab |= WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST;

	return capab;
}


static int wpa_gen_wpa_ie_rsn(u8 *rsn_ie, size_t rsn_ie_len,
			      int pairwise_cipher, int group_cipher,
			      int key_mgmt, int mgmt_group_cipher,
			      struct wpa_sm *sm)
{
	u8 *pos;
	struct rsn_ie_hdr *hdr;
	u32 suite;

	if (rsn_ie_len < sizeof(*hdr) + RSN_SELECTOR_LEN +
	    2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN + 2 +
	    (sm->cur_pmksa ? 2 + PMKID_LEN : 0)) {
		wpa_printf(MSG_DEBUG, "RSN: Too short IE buffer (%lu bytes)",
			   (unsigned long) rsn_ie_len);
		return -1;
	}

	hdr = (struct rsn_ie_hdr *) rsn_ie;
	hdr->elem_id = WLAN_EID_RSN;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, group_cipher);
	if (suite == 0) {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, pairwise_cipher);
	if (suite == 0 ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	*pos++ = 1;
	*pos++ = 0;
	if (key_mgmt == WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X);
	} else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_CCKM);
#ifdef CONFIG_IEEE80211R
	} else if (key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X);
#ifdef CONFIG_SHA384
	} else if (key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384);
#endif /* CONFIG_SHA384 */
	} else if (key_mgmt == WPA_KEY_MGMT_FT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_PSK);
#endif /* CONFIG_IEEE80211R */
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SHA256);
	} else if (key_mgmt == WPA_KEY_MGMT_PSK_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_SHA256);
#ifdef CONFIG_SAE
	} else if (key_mgmt == WPA_KEY_MGMT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
	} else if (key_mgmt == WPA_KEY_MGMT_SAE_EXT_KEY) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE_EXT_KEY);
	} else if (key_mgmt == WPA_KEY_MGMT_FT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE);
	} else if (key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY);
#endif /* CONFIG_SAE */
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192);
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B);
#ifdef CONFIG_FILS
	} else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA256);
	} else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA384);
#ifdef CONFIG_IEEE80211R
	} else if (key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA256);
	} else if (key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA384);
#endif /* CONFIG_IEEE80211R */
#endif /* CONFIG_FILS */
#ifdef CONFIG_OWE
	} else if (key_mgmt & WPA_KEY_MGMT_OWE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OWE);
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	} else if (key_mgmt & WPA_KEY_MGMT_DPP) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_DPP);
#endif /* CONFIG_DPP */
#ifdef CONFIG_HS20
	} else if (key_mgmt & WPA_KEY_MGMT_OSEN) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OSEN);
#endif /* CONFIG_HS20 */
#ifdef CONFIG_SHA384
	} else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SHA384);
#endif /* CONFIG_SHA384 */
	} else {
		wpa_printf(MSG_WARNING, "Invalid key management type (%d).",
			   key_mgmt);
		return -1;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	WPA_PUT_LE16(pos, rsn_supp_capab(sm));
	pos += 2;

	if (sm->cur_pmksa) {
		/* PMKID Count (2 octets, little endian) */
		*pos++ = 1;
		*pos++ = 0;
		/* PMKID */
		os_memcpy(pos, sm->cur_pmksa->pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	if (wpa_cipher_valid_mgmt_group(mgmt_group_cipher)) {
		if (!sm->cur_pmksa) {
			/* PMKID Count */
			WPA_PUT_LE16(pos, 0);
			pos += 2;
		}

		/* Management Group Cipher Suite */
		RSN_SELECTOR_PUT(pos, wpa_cipher_to_suite(WPA_PROTO_RSN,
							  mgmt_group_cipher));
		pos += RSN_SELECTOR_LEN;
	}

	hdr->len = (pos - rsn_ie) - 2;

	WPA_ASSERT((size_t) (pos - rsn_ie) <= rsn_ie_len);

	return pos - rsn_ie;
}


#ifdef CONFIG_HS20
static int wpa_gen_wpa_ie_osen(u8 *wpa_ie, size_t wpa_ie_len,
			       int pairwise_cipher, int group_cipher,
			       int key_mgmt)
{
	u8 *pos, *len;
	u32 suite;

	if (wpa_ie_len < 2 + 4 + RSN_SELECTOR_LEN +
	    2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN)
		return -1;

	pos = wpa_ie;
	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	len = pos++; /* to be filled */
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = HS20_OSEN_OUI_TYPE;

	/* Group Data Cipher Suite */
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, group_cipher);
	if (suite == 0) {
		wpa_printf(MSG_WARNING, "Invalid group cipher (%d).",
			   group_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* Pairwise Cipher Suite Count and List */
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, pairwise_cipher);
	if (suite == 0 ||
	    (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
	     pairwise_cipher != WPA_CIPHER_NONE)) {
		wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
			   pairwise_cipher);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	/* AKM Suite Count and List */
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OSEN);
	pos += RSN_SELECTOR_LEN;

	*len = pos - len - 1;

	WPA_ASSERT((size_t) (pos - wpa_ie) <= wpa_ie_len);

	return pos - wpa_ie;
}
#endif /* CONFIG_HS20 */


/**
 * wpa_gen_wpa_ie - Generate WPA/RSN IE based on current security policy
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @wpa_ie: Pointer to memory area for the generated WPA/RSN IE
 * @wpa_ie_len: Maximum length of the generated WPA/RSN IE
 * Returns: Length of the generated WPA/RSN IE or -1 on failure
 */
int wpa_gen_wpa_ie(struct wpa_sm *sm, u8 *wpa_ie, size_t wpa_ie_len)
{
	if (sm->proto == WPA_PROTO_RSN)
		return wpa_gen_wpa_ie_rsn(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt, sm->mgmt_group_cipher,
					  sm);
#ifdef CONFIG_HS20
	else if (sm->proto == WPA_PROTO_OSEN)
		return wpa_gen_wpa_ie_osen(wpa_ie, wpa_ie_len,
					   sm->pairwise_cipher,
					   sm->group_cipher,
					   sm->key_mgmt);
#endif /* CONFIG_HS20 */
	else
		return wpa_gen_wpa_ie_wpa(wpa_ie, wpa_ie_len,
					  sm->pairwise_cipher,
					  sm->group_cipher,
					  sm->key_mgmt);
}


int wpa_gen_rsnxe(struct wpa_sm *sm, u8 *rsnxe, size_t rsnxe_len)
{
	u8 *pos = rsnxe;
	u32 capab = 0, tmp;
	size_t flen;

	if (wpa_key_mgmt_sae(sm->key_mgmt) &&
	    (sm->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
	     sm->sae_pwe == SAE_PWE_BOTH || sm->sae_pk)) {
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
#ifdef CONFIG_SAE_PK
		if (sm->sae_pk)
			capab |= BIT(WLAN_RSNX_CAPAB_SAE_PK);
#endif /* CONFIG_SAE_PK */
	}

	if (sm->secure_ltf)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (sm->secure_rtt)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (sm->prot_range_neg)
		capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
	if (sm->ssid_protection)
		capab |= BIT(WLAN_RSNX_CAPAB_SSID_PROTECTION);

	if (!capab)
		return 0; /* no supported extended RSN capabilities */
	tmp = capab;
	flen = 0;
	while (tmp) {
		flen++;
		tmp >>= 8;
	}
	if (rsnxe_len < 2 + flen)
		return -1;
	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	*pos++ = WLAN_EID_RSNX;
	*pos++ = flen;
	while (capab) {
		*pos++ = capab & 0xff;
		capab >>= 8;
	}

	return pos - rsnxe;
}
