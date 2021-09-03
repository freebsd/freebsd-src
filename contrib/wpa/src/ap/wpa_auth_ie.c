/*
 * hostapd - WPA/RSN IE and KDE definitions
 * Copyright (c) 2004-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "ap_config.h"
#include "ieee802_11.h"
#include "wpa_auth.h"
#include "pmksa_cache_auth.h"
#include "wpa_auth_ie.h"
#include "wpa_auth_i.h"


#ifdef CONFIG_RSN_TESTING
int rsn_testing = 0;
#endif /* CONFIG_RSN_TESTING */


static int wpa_write_wpa_ie(struct wpa_auth_config *conf, u8 *buf, size_t len)
{
	struct wpa_ie_hdr *hdr;
	int num_suites;
	u8 *pos, *count;
	u32 suite;

	hdr = (struct wpa_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_VENDOR_SPECIFIC;
	RSN_SELECTOR_PUT(hdr->oui, WPA_OUI_TYPE);
	WPA_PUT_LE16(hdr->version, WPA_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_WPA, conf->wpa_group);
	if (suite == 0) {
		wpa_printf(MSG_DEBUG, "Invalid group cipher (%d).",
			   conf->wpa_group);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += WPA_SELECTOR_LEN;

	count = pos;
	pos += 2;

	num_suites = wpa_cipher_put_suites(pos, conf->wpa_pairwise);
	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid pairwise cipher (%d).",
			   conf->wpa_pairwise);
		return -1;
	}
	pos += num_suites * WPA_SELECTOR_LEN;
	WPA_PUT_LE16(count, num_suites);

	num_suites = 0;
	count = pos;
	pos += 2;

	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
		pos += WPA_SELECTOR_LEN;
		num_suites++;
	}

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid key management type (%d).",
			   conf->wpa_key_mgmt);
		return -1;
	}
	WPA_PUT_LE16(count, num_suites);

	/* WPA Capabilities; use defaults, so no need to include it */

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


static u16 wpa_own_rsn_capab(struct wpa_auth_config *conf)
{
	u16 capab = 0;

	if (conf->rsn_preauth)
		capab |= WPA_CAPABILITY_PREAUTH;
	if (conf->wmm_enabled) {
		/* 4 PTKSA replay counters when using WMM */
		capab |= (RSN_NUM_REPLAY_COUNTERS_16 << 2);
	}
	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		capab |= WPA_CAPABILITY_MFPC;
		if (conf->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED)
			capab |= WPA_CAPABILITY_MFPR;
	}
#ifdef CONFIG_OCV
	if (conf->ocv)
		capab |= WPA_CAPABILITY_OCVC;
#endif /* CONFIG_OCV */
#ifdef CONFIG_RSN_TESTING
	if (rsn_testing)
		capab |= BIT(8) | BIT(15);
#endif /* CONFIG_RSN_TESTING */
	if (conf->extended_key_id)
		capab |= WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST;

	return capab;
}


int wpa_write_rsn_ie(struct wpa_auth_config *conf, u8 *buf, size_t len,
		     const u8 *pmkid)
{
	struct rsn_ie_hdr *hdr;
	int num_suites, res;
	u8 *pos, *count;
	u32 suite;

	hdr = (struct rsn_ie_hdr *) buf;
	hdr->elem_id = WLAN_EID_RSN;
	WPA_PUT_LE16(hdr->version, RSN_VERSION);
	pos = (u8 *) (hdr + 1);

	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, conf->wpa_group);
	if (suite == 0) {
		wpa_printf(MSG_DEBUG, "Invalid group cipher (%d).",
			   conf->wpa_group);
		return -1;
	}
	RSN_SELECTOR_PUT(pos, suite);
	pos += RSN_SELECTOR_LEN;

	num_suites = 0;
	count = pos;
	pos += 2;

#ifdef CONFIG_RSN_TESTING
	if (rsn_testing) {
		RSN_SELECTOR_PUT(pos, RSN_SELECTOR(0x12, 0x34, 0x56, 1));
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_RSN_TESTING */

	res = rsn_cipher_put_suites(pos, conf->rsn_pairwise);
	num_suites += res;
	pos += res * RSN_SELECTOR_LEN;

#ifdef CONFIG_RSN_TESTING
	if (rsn_testing) {
		RSN_SELECTOR_PUT(pos, RSN_SELECTOR(0x12, 0x34, 0x56, 2));
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_RSN_TESTING */

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid pairwise cipher (%d).",
			   conf->rsn_pairwise);
		return -1;
	}
	WPA_PUT_LE16(count, num_suites);

	num_suites = 0;
	count = pos;
	pos += 2;

#ifdef CONFIG_RSN_TESTING
	if (rsn_testing) {
		RSN_SELECTOR_PUT(pos, RSN_SELECTOR(0x12, 0x34, 0x56, 1));
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_RSN_TESTING */

	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_UNSPEC_802_1X);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#ifdef CONFIG_SHA384
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_SHA384 */
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_PSK) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_PSK);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_IEEE80211R_AP */
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SHA256);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PSK_SHA256);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#ifdef CONFIG_SAE
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_SAE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_SAE */
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#ifdef CONFIG_FILS
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA256);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FILS_SHA384);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA256);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA384);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_IEEE80211R_AP */
#endif /* CONFIG_FILS */
#ifdef CONFIG_OWE
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OWE);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_DPP);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_DPP */
#ifdef CONFIG_HS20
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_OSEN) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_OSEN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_HS20 */
#ifdef CONFIG_PASN
	if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PASN) {
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PASN);
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_PASN */

#ifdef CONFIG_RSN_TESTING
	if (rsn_testing) {
		RSN_SELECTOR_PUT(pos, RSN_SELECTOR(0x12, 0x34, 0x56, 2));
		pos += RSN_SELECTOR_LEN;
		num_suites++;
	}
#endif /* CONFIG_RSN_TESTING */

	if (num_suites == 0) {
		wpa_printf(MSG_DEBUG, "Invalid key management type (%d).",
			   conf->wpa_key_mgmt);
		return -1;
	}
	WPA_PUT_LE16(count, num_suites);

	/* RSN Capabilities */
	WPA_PUT_LE16(pos, wpa_own_rsn_capab(conf));
	pos += 2;

	if (pmkid) {
		if (2 + PMKID_LEN > buf + len - pos)
			return -1;
		/* PMKID Count */
		WPA_PUT_LE16(pos, 1);
		pos += 2;
		os_memcpy(pos, pmkid, PMKID_LEN);
		pos += PMKID_LEN;
	}

	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION &&
	    conf->group_mgmt_cipher != WPA_CIPHER_AES_128_CMAC) {
		if (2 + 4 > buf + len - pos)
			return -1;
		if (pmkid == NULL) {
			/* PMKID Count */
			WPA_PUT_LE16(pos, 0);
			pos += 2;
		}

		/* Management Group Cipher Suite */
		switch (conf->group_mgmt_cipher) {
		case WPA_CIPHER_AES_128_CMAC:
			RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_AES_128_CMAC);
			break;
		case WPA_CIPHER_BIP_GMAC_128:
			RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_GMAC_128);
			break;
		case WPA_CIPHER_BIP_GMAC_256:
			RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_GMAC_256);
			break;
		case WPA_CIPHER_BIP_CMAC_256:
			RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_CMAC_256);
			break;
		default:
			wpa_printf(MSG_DEBUG,
				   "Invalid group management cipher (0x%x)",
				   conf->group_mgmt_cipher);
			return -1;
		}
		pos += RSN_SELECTOR_LEN;
	}

#ifdef CONFIG_RSN_TESTING
	if (rsn_testing) {
		/*
		 * Fill in any defined fields and add extra data to the end of
		 * the element.
		 */
		int pmkid_count_set = pmkid != NULL;
		if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION)
			pmkid_count_set = 1;
		/* PMKID Count */
		WPA_PUT_LE16(pos, 0);
		pos += 2;
		if (conf->ieee80211w == NO_MGMT_FRAME_PROTECTION) {
			/* Management Group Cipher Suite */
			RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_AES_128_CMAC);
			pos += RSN_SELECTOR_LEN;
		}

		os_memset(pos, 0x12, 17);
		pos += 17;
	}
#endif /* CONFIG_RSN_TESTING */

	hdr->len = (pos - buf) - 2;

	return pos - buf;
}


int wpa_write_rsnxe(struct wpa_auth_config *conf, u8 *buf, size_t len)
{
	u8 *pos = buf;
	u16 capab = 0;
	size_t flen;

	if (wpa_key_mgmt_sae(conf->wpa_key_mgmt) &&
	    (conf->sae_pwe == 1 || conf->sae_pwe == 2 || conf->sae_pk)) {
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
#ifdef CONFIG_SAE_PK
		if (conf->sae_pk)
			capab |= BIT(WLAN_RSNX_CAPAB_SAE_PK);
#endif /* CONFIG_SAE_PK */
	}

	if (conf->secure_ltf)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (conf->secure_rtt)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (conf->prot_range_neg)
		capab |= BIT(WLAN_RSNX_CAPAB_PROT_RANGE_NEG);

	flen = (capab & 0xff00) ? 2 : 1;
	if (!capab)
		return 0; /* no supported extended RSN capabilities */
	if (len < 2 + flen)
		return -1;
	capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

	*pos++ = WLAN_EID_RSNX;
	*pos++ = flen;
	*pos++ = capab & 0x00ff;
	capab >>= 8;
	if (capab)
		*pos++ = capab;

	return pos - buf;
}


static u8 * wpa_write_osen(struct wpa_auth_config *conf, u8 *eid)
{
	u8 *len;
	u16 capab;

	*eid++ = WLAN_EID_VENDOR_SPECIFIC;
	len = eid++; /* to be filled */
	WPA_PUT_BE24(eid, OUI_WFA);
	eid += 3;
	*eid++ = HS20_OSEN_OUI_TYPE;

	/* Group Data Cipher Suite */
	RSN_SELECTOR_PUT(eid, RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED);
	eid += RSN_SELECTOR_LEN;

	/* Pairwise Cipher Suite Count and List */
	WPA_PUT_LE16(eid, 1);
	eid += 2;
	RSN_SELECTOR_PUT(eid, RSN_CIPHER_SUITE_CCMP);
	eid += RSN_SELECTOR_LEN;

	/* AKM Suite Count and List */
	WPA_PUT_LE16(eid, 1);
	eid += 2;
	RSN_SELECTOR_PUT(eid, RSN_AUTH_KEY_MGMT_OSEN);
	eid += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	capab = 0;
	if (conf->wmm_enabled) {
		/* 4 PTKSA replay counters when using WMM */
		capab |= (RSN_NUM_REPLAY_COUNTERS_16 << 2);
	}
	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		capab |= WPA_CAPABILITY_MFPC;
		if (conf->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED)
			capab |= WPA_CAPABILITY_MFPR;
	}
#ifdef CONFIG_OCV
	if (conf->ocv)
		capab |= WPA_CAPABILITY_OCVC;
#endif /* CONFIG_OCV */
	WPA_PUT_LE16(eid, capab);
	eid += 2;

	*len = eid - len - 1;

	return eid;
}


int wpa_auth_gen_wpa_ie(struct wpa_authenticator *wpa_auth)
{
	u8 *pos, buf[128];
	int res;

#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_auth->conf.own_ie_override_len) {
		wpa_hexdump(MSG_DEBUG, "WPA: Forced own IE(s) for testing",
			    wpa_auth->conf.own_ie_override,
			    wpa_auth->conf.own_ie_override_len);
		os_free(wpa_auth->wpa_ie);
		wpa_auth->wpa_ie =
			os_malloc(wpa_auth->conf.own_ie_override_len);
		if (wpa_auth->wpa_ie == NULL)
			return -1;
		os_memcpy(wpa_auth->wpa_ie, wpa_auth->conf.own_ie_override,
			  wpa_auth->conf.own_ie_override_len);
		wpa_auth->wpa_ie_len = wpa_auth->conf.own_ie_override_len;
		return 0;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	pos = buf;

	if (wpa_auth->conf.wpa == WPA_PROTO_OSEN) {
		pos = wpa_write_osen(&wpa_auth->conf, pos);
	}
	if (wpa_auth->conf.wpa & WPA_PROTO_RSN) {
		res = wpa_write_rsn_ie(&wpa_auth->conf,
				       pos, buf + sizeof(buf) - pos, NULL);
		if (res < 0)
			return res;
		pos += res;
		res = wpa_write_rsnxe(&wpa_auth->conf, pos,
				      buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(wpa_auth->conf.wpa_key_mgmt)) {
		res = wpa_write_mdie(&wpa_auth->conf, pos,
				     buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}
#endif /* CONFIG_IEEE80211R_AP */
	if (wpa_auth->conf.wpa & WPA_PROTO_WPA) {
		res = wpa_write_wpa_ie(&wpa_auth->conf,
				       pos, buf + sizeof(buf) - pos);
		if (res < 0)
			return res;
		pos += res;
	}

	os_free(wpa_auth->wpa_ie);
	wpa_auth->wpa_ie = os_malloc(pos - buf);
	if (wpa_auth->wpa_ie == NULL)
		return -1;
	os_memcpy(wpa_auth->wpa_ie, buf, pos - buf);
	wpa_auth->wpa_ie_len = pos - buf;

	return 0;
}


u8 * wpa_add_kde(u8 *pos, u32 kde, const u8 *data, size_t data_len,
		 const u8 *data2, size_t data2_len)
{
	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = RSN_SELECTOR_LEN + data_len + data2_len;
	RSN_SELECTOR_PUT(pos, kde);
	pos += RSN_SELECTOR_LEN;
	os_memcpy(pos, data, data_len);
	pos += data_len;
	if (data2) {
		os_memcpy(pos, data2, data2_len);
		pos += data2_len;
	}
	return pos;
}


struct wpa_auth_okc_iter_data {
	struct rsn_pmksa_cache_entry *pmksa;
	const u8 *aa;
	const u8 *spa;
	const u8 *pmkid;
};


static int wpa_auth_okc_iter(struct wpa_authenticator *a, void *ctx)
{
	struct wpa_auth_okc_iter_data *data = ctx;
	data->pmksa = pmksa_cache_get_okc(a->pmksa, data->aa, data->spa,
					  data->pmkid);
	if (data->pmksa)
		return 1;
	return 0;
}


enum wpa_validate_result
wpa_validate_wpa_ie(struct wpa_authenticator *wpa_auth,
		    struct wpa_state_machine *sm, int freq,
		    const u8 *wpa_ie, size_t wpa_ie_len,
		    const u8 *rsnxe, size_t rsnxe_len,
		    const u8 *mdie, size_t mdie_len,
		    const u8 *owe_dh, size_t owe_dh_len)
{
	struct wpa_auth_config *conf = &wpa_auth->conf;
	struct wpa_ie_data data;
	int ciphers, key_mgmt, res, version;
	u32 selector;
	size_t i;
	const u8 *pmkid = NULL;

	if (wpa_auth == NULL || sm == NULL)
		return WPA_NOT_ENABLED;

	if (wpa_ie == NULL || wpa_ie_len < 1)
		return WPA_INVALID_IE;

	if (wpa_ie[0] == WLAN_EID_RSN)
		version = WPA_PROTO_RSN;
	else
		version = WPA_PROTO_WPA;

	if (!(wpa_auth->conf.wpa & version)) {
		wpa_printf(MSG_DEBUG, "Invalid WPA proto (%d) from " MACSTR,
			   version, MAC2STR(sm->addr));
		return WPA_INVALID_PROTO;
	}

	if (version == WPA_PROTO_RSN) {
		res = wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, &data);
		if (!data.has_pairwise)
			data.pairwise_cipher = wpa_default_rsn_cipher(freq);
		if (!data.has_group)
			data.group_cipher = wpa_default_rsn_cipher(freq);

		if (wpa_key_mgmt_ft(data.key_mgmt) && !mdie &&
		    !wpa_key_mgmt_only_ft(data.key_mgmt)) {
			/* Workaround for some HP and Epson printers that seem
			 * to incorrectly copy the FT-PSK + WPA-PSK AKMs from AP
			 * advertised RSNE to Association Request frame. */
			wpa_printf(MSG_DEBUG,
				   "RSN: FT set in RSNE AKM but MDE is missing from "
				   MACSTR
				   " - ignore FT AKM(s) because there's also a non-FT AKM",
				   MAC2STR(sm->addr));
			data.key_mgmt &= ~WPA_KEY_MGMT_FT;
		}

		selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (0) {
		}
		else if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
			selector = RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
		else if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B)
			selector = RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
#ifdef CONFIG_FILS
#ifdef CONFIG_IEEE80211R_AP
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384)
			selector = RSN_AUTH_KEY_MGMT_FT_FILS_SHA384;
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256)
			selector = RSN_AUTH_KEY_MGMT_FT_FILS_SHA256;
#endif /* CONFIG_IEEE80211R_AP */
		else if (data.key_mgmt & WPA_KEY_MGMT_FILS_SHA384)
			selector = RSN_AUTH_KEY_MGMT_FILS_SHA384;
		else if (data.key_mgmt & WPA_KEY_MGMT_FILS_SHA256)
			selector = RSN_AUTH_KEY_MGMT_FILS_SHA256;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R_AP
#ifdef CONFIG_SHA384
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384)
			selector = RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384;
#endif /* CONFIG_SHA384 */
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X)
			selector = RSN_AUTH_KEY_MGMT_FT_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_PSK)
			selector = RSN_AUTH_KEY_MGMT_FT_PSK;
#endif /* CONFIG_IEEE80211R_AP */
		else if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256)
			selector = RSN_AUTH_KEY_MGMT_802_1X_SHA256;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK_SHA256)
			selector = RSN_AUTH_KEY_MGMT_PSK_SHA256;
#ifdef CONFIG_SAE
		else if (data.key_mgmt & WPA_KEY_MGMT_SAE)
			selector = RSN_AUTH_KEY_MGMT_SAE;
		else if (data.key_mgmt & WPA_KEY_MGMT_FT_SAE)
			selector = RSN_AUTH_KEY_MGMT_FT_SAE;
#endif /* CONFIG_SAE */
		else if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
#ifdef CONFIG_OWE
		else if (data.key_mgmt & WPA_KEY_MGMT_OWE)
			selector = RSN_AUTH_KEY_MGMT_OWE;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
		else if (data.key_mgmt & WPA_KEY_MGMT_DPP)
			selector = RSN_AUTH_KEY_MGMT_DPP;
#endif /* CONFIG_DPP */
#ifdef CONFIG_HS20
		else if (data.key_mgmt & WPA_KEY_MGMT_OSEN)
			selector = RSN_AUTH_KEY_MGMT_OSEN;
#endif /* CONFIG_HS20 */
		wpa_auth->dot11RSNAAuthenticationSuiteSelected = selector;

		selector = wpa_cipher_to_suite(WPA_PROTO_RSN,
					       data.pairwise_cipher);
		if (!selector)
			selector = RSN_CIPHER_SUITE_CCMP;
		wpa_auth->dot11RSNAPairwiseCipherSelected = selector;

		selector = wpa_cipher_to_suite(WPA_PROTO_RSN,
					       data.group_cipher);
		if (!selector)
			selector = RSN_CIPHER_SUITE_CCMP;
		wpa_auth->dot11RSNAGroupCipherSelected = selector;
	} else {
		res = wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, &data);

		selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			selector = WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
		else if (data.key_mgmt & WPA_KEY_MGMT_PSK)
			selector = WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X;
		wpa_auth->dot11RSNAAuthenticationSuiteSelected = selector;

		selector = wpa_cipher_to_suite(WPA_PROTO_WPA,
					       data.pairwise_cipher);
		if (!selector)
			selector = RSN_CIPHER_SUITE_TKIP;
		wpa_auth->dot11RSNAPairwiseCipherSelected = selector;

		selector = wpa_cipher_to_suite(WPA_PROTO_WPA,
					       data.group_cipher);
		if (!selector)
			selector = WPA_CIPHER_SUITE_TKIP;
		wpa_auth->dot11RSNAGroupCipherSelected = selector;
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
	if (0) {
	}
	else if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_SUITE_B_192;
	else if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_SUITE_B;
#ifdef CONFIG_FILS
#ifdef CONFIG_IEEE80211R_AP
	else if (key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_FILS_SHA384;
	else if (data.key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_FILS_SHA256;
#endif /* CONFIG_IEEE80211R_AP */
	else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA384)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FILS_SHA384;
	else if (key_mgmt & WPA_KEY_MGMT_FILS_SHA256)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FILS_SHA256;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R_AP
#ifdef CONFIG_SHA384
	else if (key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_IEEE8021X_SHA384;
#endif /* CONFIG_SHA384 */
	else if (key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_IEEE8021X;
	else if (key_mgmt & WPA_KEY_MGMT_FT_PSK)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_PSK;
#endif /* CONFIG_IEEE80211R_AP */
	else if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_SHA256;
	else if (key_mgmt & WPA_KEY_MGMT_PSK_SHA256)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
#ifdef CONFIG_SAE
	else if (key_mgmt & WPA_KEY_MGMT_SAE)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_SAE;
	else if (key_mgmt & WPA_KEY_MGMT_FT_SAE)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_FT_SAE;
#endif /* CONFIG_SAE */
	else if (key_mgmt & WPA_KEY_MGMT_IEEE8021X)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X;
#ifdef CONFIG_OWE
	else if (key_mgmt & WPA_KEY_MGMT_OWE)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_OWE;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP
	else if (key_mgmt & WPA_KEY_MGMT_DPP)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_DPP;
#endif /* CONFIG_DPP */
#ifdef CONFIG_HS20
	else if (key_mgmt & WPA_KEY_MGMT_OSEN)
		sm->wpa_key_mgmt = WPA_KEY_MGMT_OSEN;
#endif /* CONFIG_HS20 */
	else
		sm->wpa_key_mgmt = WPA_KEY_MGMT_PSK;

	if (version == WPA_PROTO_RSN)
		ciphers = data.pairwise_cipher & wpa_auth->conf.rsn_pairwise;
	else
		ciphers = data.pairwise_cipher & wpa_auth->conf.wpa_pairwise;
	if (!ciphers) {
		wpa_printf(MSG_DEBUG, "Invalid %s pairwise cipher (0x%x) "
			   "from " MACSTR,
			   version == WPA_PROTO_RSN ? "RSN" : "WPA",
			   data.pairwise_cipher, MAC2STR(sm->addr));
		return WPA_INVALID_PAIRWISE;
	}

	if (wpa_auth->conf.ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED) {
		if (!(data.capabilities & WPA_CAPABILITY_MFPC)) {
			wpa_printf(MSG_DEBUG, "Management frame protection "
				   "required, but client did not enable it");
			return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
		}

		if (data.mgmt_group_cipher != wpa_auth->conf.group_mgmt_cipher)
		{
			wpa_printf(MSG_DEBUG, "Unsupported management group "
				   "cipher %d", data.mgmt_group_cipher);
			return WPA_INVALID_MGMT_GROUP_CIPHER;
		}
	}

#ifdef CONFIG_SAE
	if (wpa_auth->conf.ieee80211w == MGMT_FRAME_PROTECTION_OPTIONAL &&
	    wpa_auth->conf.sae_require_mfp &&
	    wpa_key_mgmt_sae(sm->wpa_key_mgmt) &&
	    !(data.capabilities & WPA_CAPABILITY_MFPC)) {
		wpa_printf(MSG_DEBUG,
			   "Management frame protection required with SAE, but client did not enable it");
		return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_OCV
	if (wpa_auth->conf.ocv && (data.capabilities & WPA_CAPABILITY_OCVC) &&
	    !(data.capabilities & WPA_CAPABILITY_MFPC)) {
		/* Some legacy MFP incapable STAs wrongly copy OCVC bit from
		 * AP RSN capabilities. To improve interoperability with such
		 * legacy STAs allow connection without enabling OCV when the
		 * workaround mode (ocv=2) is enabled.
		 */
		if (wpa_auth->conf.ocv == 2) {
			wpa_printf(MSG_DEBUG,
				   "Allow connecting MFP incapable and OCV capable STA without enabling OCV");
			wpa_auth_set_ocv(sm, 0);
		} else {
			wpa_printf(MSG_DEBUG,
				   "Management frame protection required with OCV, but client did not enable it");
			return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
		}
	} else {
		wpa_auth_set_ocv(sm, (data.capabilities & WPA_CAPABILITY_OCVC) ?
				 wpa_auth->conf.ocv : 0);
	}
#endif /* CONFIG_OCV */

	if (wpa_auth->conf.ieee80211w == NO_MGMT_FRAME_PROTECTION ||
	    !(data.capabilities & WPA_CAPABILITY_MFPC))
		sm->mgmt_frame_prot = 0;
	else
		sm->mgmt_frame_prot = 1;

	if (sm->mgmt_frame_prot && (ciphers & WPA_CIPHER_TKIP)) {
		    wpa_printf(MSG_DEBUG,
			       "Management frame protection cannot use TKIP");
		    return WPA_MGMT_FRAME_PROTECTION_VIOLATION;
	}

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(sm->wpa_key_mgmt)) {
		if (mdie == NULL || mdie_len < MOBILITY_DOMAIN_ID_LEN + 1) {
			wpa_printf(MSG_DEBUG, "RSN: Trying to use FT, but "
				   "MDIE not included");
			return WPA_INVALID_MDIE;
		}
		if (os_memcmp(mdie, wpa_auth->conf.mobility_domain,
			      MOBILITY_DOMAIN_ID_LEN) != 0) {
			wpa_hexdump(MSG_DEBUG, "RSN: Attempted to use unknown "
				    "MDIE", mdie, MOBILITY_DOMAIN_ID_LEN);
			return WPA_INVALID_MDIE;
		}
	} else if (mdie != NULL) {
		wpa_printf(MSG_DEBUG,
			   "RSN: Trying to use non-FT AKM suite, but MDIE included");
		return WPA_INVALID_AKMP;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_OWE
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_OWE && !owe_dh) {
		wpa_printf(MSG_DEBUG,
			   "OWE: No Diffie-Hellman Parameter element");
		return WPA_INVALID_AKMP;
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP &&
	    ((conf->dpp_pfs == 1 && !owe_dh) ||
	     (conf->dpp_pfs == 2 && owe_dh))) {
		wpa_printf(MSG_DEBUG, "DPP: PFS %s",
			   conf->dpp_pfs == 1 ? "required" : "not allowed");
		return WPA_DENIED_OTHER_REASON;
	}
#endif /* CONFIG_DPP2 */

	sm->pairwise = wpa_pick_pairwise_cipher(ciphers, 0);
	if (sm->pairwise < 0)
		return WPA_INVALID_PAIRWISE;

	/* TODO: clear WPA/WPA2 state if STA changes from one to another */
	if (wpa_ie[0] == WLAN_EID_RSN)
		sm->wpa = WPA_VERSION_WPA2;
	else
		sm->wpa = WPA_VERSION_WPA;

#if defined(CONFIG_IEEE80211R_AP) && defined(CONFIG_FILS)
	if ((sm->wpa_key_mgmt == WPA_KEY_MGMT_FT_FILS_SHA256 ||
	     sm->wpa_key_mgmt == WPA_KEY_MGMT_FT_FILS_SHA384) &&
	    (sm->auth_alg == WLAN_AUTH_FILS_SK ||
	     sm->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sm->auth_alg == WLAN_AUTH_FILS_PK) &&
	    (data.num_pmkid != 1 || !data.pmkid || !sm->pmk_r1_name_valid ||
	     os_memcmp_const(data.pmkid, sm->pmk_r1_name,
			     WPA_PMK_NAME_LEN) != 0)) {
		wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
				 "No PMKR1Name match for FILS+FT");
		return WPA_INVALID_PMKID;
	}
#endif /* CONFIG_IEEE80211R_AP && CONFIG_FILS */

	sm->pmksa = NULL;
	for (i = 0; i < data.num_pmkid; i++) {
		wpa_hexdump(MSG_DEBUG, "RSN IE: STA PMKID",
			    &data.pmkid[i * PMKID_LEN], PMKID_LEN);
		sm->pmksa = pmksa_cache_auth_get(wpa_auth->pmksa, sm->addr,
						 &data.pmkid[i * PMKID_LEN]);
		if (sm->pmksa) {
			pmkid = sm->pmksa->pmkid;
			break;
		}
	}
	for (i = 0; sm->pmksa == NULL && wpa_auth->conf.okc &&
		     i < data.num_pmkid; i++) {
		struct wpa_auth_okc_iter_data idata;
		idata.pmksa = NULL;
		idata.aa = wpa_auth->addr;
		idata.spa = sm->addr;
		idata.pmkid = &data.pmkid[i * PMKID_LEN];
		wpa_auth_for_each_auth(wpa_auth, wpa_auth_okc_iter, &idata);
		if (idata.pmksa) {
			wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
					 "OKC match for PMKID");
			sm->pmksa = pmksa_cache_add_okc(wpa_auth->pmksa,
							idata.pmksa,
							wpa_auth->addr,
							idata.pmkid);
			pmkid = idata.pmkid;
			break;
		}
	}
	if (sm->pmksa && pmkid) {
		struct vlan_description *vlan;

		vlan = sm->pmksa->vlan_desc;
		wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
				 "PMKID found from PMKSA cache eap_type=%d vlan=%d%s",
				 sm->pmksa->eap_type_authsrv,
				 vlan ? vlan->untagged : 0,
				 (vlan && vlan->tagged[0]) ? "+" : "");
		os_memcpy(wpa_auth->dot11RSNAPMKIDUsed, pmkid, PMKID_LEN);
	}

#ifdef CONFIG_SAE
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_SAE && data.num_pmkid &&
	    !sm->pmksa) {
		wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
				 "No PMKSA cache entry found for SAE");
		return WPA_INVALID_PMKID;
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_DPP
	if (sm->wpa_key_mgmt == WPA_KEY_MGMT_DPP && !sm->pmksa) {
		wpa_auth_vlogger(wpa_auth, sm->addr, LOGGER_DEBUG,
				 "No PMKSA cache entry found for DPP");
		return WPA_INVALID_PMKID;
	}
#endif /* CONFIG_DPP */

	if (conf->extended_key_id && sm->wpa == WPA_VERSION_WPA2 &&
	    sm->pairwise != WPA_CIPHER_TKIP &&
	    (data.capabilities & WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST)) {
		sm->use_ext_key_id = true;
		if (conf->extended_key_id == 2 &&
		    !wpa_key_mgmt_ft(sm->wpa_key_mgmt) &&
		    !wpa_key_mgmt_fils(sm->wpa_key_mgmt))
			sm->keyidx_active = 1;
		else
			sm->keyidx_active = 0;
		wpa_printf(MSG_DEBUG,
			   "RSN: Extended Key ID supported (start with %d)",
			   sm->keyidx_active);
	} else {
		sm->use_ext_key_id = false;
	}

	if (sm->wpa_ie == NULL || sm->wpa_ie_len < wpa_ie_len) {
		os_free(sm->wpa_ie);
		sm->wpa_ie = os_malloc(wpa_ie_len);
		if (sm->wpa_ie == NULL)
			return WPA_ALLOC_FAIL;
	}
	os_memcpy(sm->wpa_ie, wpa_ie, wpa_ie_len);
	sm->wpa_ie_len = wpa_ie_len;

	if (rsnxe && rsnxe_len) {
		if (!sm->rsnxe || sm->rsnxe_len < rsnxe_len) {
			os_free(sm->rsnxe);
			sm->rsnxe = os_malloc(rsnxe_len);
			if (!sm->rsnxe)
				return WPA_ALLOC_FAIL;
		}
		os_memcpy(sm->rsnxe, rsnxe, rsnxe_len);
		sm->rsnxe_len = rsnxe_len;
	} else {
		os_free(sm->rsnxe);
		sm->rsnxe = NULL;
		sm->rsnxe_len = 0;
	}

	return WPA_IE_OK;
}


#ifdef CONFIG_HS20
int wpa_validate_osen(struct wpa_authenticator *wpa_auth,
		      struct wpa_state_machine *sm,
		      const u8 *osen_ie, size_t osen_ie_len)
{
	if (wpa_auth == NULL || sm == NULL)
		return -1;

	/* TODO: parse OSEN element */
	sm->wpa_key_mgmt = WPA_KEY_MGMT_OSEN;
	sm->mgmt_frame_prot = 1;
	sm->pairwise = WPA_CIPHER_CCMP;
	sm->wpa = WPA_VERSION_WPA2;

	if (sm->wpa_ie == NULL || sm->wpa_ie_len < osen_ie_len) {
		os_free(sm->wpa_ie);
		sm->wpa_ie = os_malloc(osen_ie_len);
		if (sm->wpa_ie == NULL)
			return -1;
	}

	os_memcpy(sm->wpa_ie, osen_ie, osen_ie_len);
	sm->wpa_ie_len = osen_ie_len;

	return 0;
}

#endif /* CONFIG_HS20 */


int wpa_auth_uses_mfp(struct wpa_state_machine *sm)
{
	return sm ? sm->mgmt_frame_prot : 0;
}


#ifdef CONFIG_OCV

void wpa_auth_set_ocv(struct wpa_state_machine *sm, int ocv)
{
	if (sm)
		sm->ocv_enabled = ocv;
}


int wpa_auth_uses_ocv(struct wpa_state_machine *sm)
{
	return sm ? sm->ocv_enabled : 0;
}

#endif /* CONFIG_OCV */


#ifdef CONFIG_OWE
u8 * wpa_auth_write_assoc_resp_owe(struct wpa_state_machine *sm,
				   u8 *pos, size_t max_len,
				   const u8 *req_ies, size_t req_ies_len)
{
	int res;
	struct wpa_auth_config *conf;

	if (!sm)
		return pos;
	conf = &sm->wpa_auth->conf;

#ifdef CONFIG_TESTING_OPTIONS
	if (conf->own_ie_override_len) {
		if (max_len < conf->own_ie_override_len)
			return NULL;
		wpa_hexdump(MSG_DEBUG, "WPA: Forced own IE(s) for testing",
			    conf->own_ie_override, conf->own_ie_override_len);
		os_memcpy(pos, conf->own_ie_override,
			  conf->own_ie_override_len);
		return pos + conf->own_ie_override_len;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	res = wpa_write_rsn_ie(conf, pos, max_len,
			       sm->pmksa ? sm->pmksa->pmkid : NULL);
	if (res < 0)
		return pos;
	return pos + res;
}
#endif /* CONFIG_OWE */


#ifdef CONFIG_FILS

u8 * wpa_auth_write_assoc_resp_fils(struct wpa_state_machine *sm,
				    u8 *pos, size_t max_len,
				    const u8 *req_ies, size_t req_ies_len)
{
	int res;

	if (!sm ||
	    sm->wpa_key_mgmt & (WPA_KEY_MGMT_FT_FILS_SHA256 |
				WPA_KEY_MGMT_FT_FILS_SHA384))
		return pos;

	res = wpa_write_rsn_ie(&sm->wpa_auth->conf, pos, max_len, NULL);
	if (res < 0)
		return pos;
	return pos + res;
}


bool wpa_auth_write_fd_rsn_info(struct wpa_authenticator *wpa_auth,
				u8 *fd_rsn_info)
{
	struct wpa_auth_config *conf;
	u32 selectors = 0;
	u8 *pos = fd_rsn_info;
	int i, res;
	u32 cipher, suite, selector, mask;
	u8 tmp[10 * RSN_SELECTOR_LEN];

	if (!wpa_auth)
		return false;
	conf = &wpa_auth->conf;

	if (!(conf->wpa & WPA_PROTO_RSN))
		return false;

	/* RSN Capability (B0..B15) */
	WPA_PUT_LE16(pos, wpa_own_rsn_capab(conf));
	pos += 2;

	/* Group Data Cipher Suite Selector (B16..B21) */
	suite = wpa_cipher_to_suite(WPA_PROTO_RSN, conf->wpa_group);
	if (suite == RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED)
		cipher = 63; /* No cipher suite selected */
	else if ((suite >> 8) == 0x000fac && ((suite & 0xff) <= 13))
		cipher = suite & 0xff;
	else
		cipher = 62; /* vendor specific */
	selectors |= cipher;

	/* Group Management Cipher Suite Selector (B22..B27) */
	cipher = 63; /* Default to no cipher suite selected */
	if (conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		switch (conf->group_mgmt_cipher) {
		case WPA_CIPHER_AES_128_CMAC:
			cipher = RSN_CIPHER_SUITE_AES_128_CMAC & 0xff;
			break;
		case WPA_CIPHER_BIP_GMAC_128:
			cipher = RSN_CIPHER_SUITE_BIP_GMAC_128 & 0xff;
			break;
		case WPA_CIPHER_BIP_GMAC_256:
			cipher = RSN_CIPHER_SUITE_BIP_GMAC_256 & 0xff;
			break;
		case WPA_CIPHER_BIP_CMAC_256:
			cipher = RSN_CIPHER_SUITE_BIP_CMAC_256 & 0xff;
			break;
		}
	}
	selectors |= cipher << 6;

	/* Pairwise Cipher Suite Selector (B28..B33) */
	cipher = 63; /* Default to no cipher suite selected */
	res = rsn_cipher_put_suites(tmp, conf->rsn_pairwise);
	if (res == 1 && tmp[0] == 0x00 && tmp[1] == 0x0f && tmp[2] == 0xac &&
	    tmp[3] <= 13)
		cipher = tmp[3];
	selectors |= cipher << 12;

	/* AKM Suite Selector (B34..B39) */
	selector = 0; /* default to AKM from RSNE in Beacon/Probe Response */
	mask = WPA_KEY_MGMT_FILS_SHA256 | WPA_KEY_MGMT_FILS_SHA384 |
		WPA_KEY_MGMT_FT_FILS_SHA384;
	if ((conf->wpa_key_mgmt & mask) && (conf->wpa_key_mgmt & ~mask) == 0) {
		suite = conf->wpa_key_mgmt & mask;
		if (suite == WPA_KEY_MGMT_FILS_SHA256)
			selector = 1; /* 00-0f-ac:14 */
		else if (suite == WPA_KEY_MGMT_FILS_SHA384)
			selector = 2; /* 00-0f-ac:15 */
		else if (suite == (WPA_KEY_MGMT_FILS_SHA256 |
				   WPA_KEY_MGMT_FILS_SHA384))
			selector = 3; /* 00-0f-ac:14 or 00-0f-ac:15 */
		else if (suite == WPA_KEY_MGMT_FT_FILS_SHA384)
			selector = 4; /* 00-0f-ac:17 */
	}
	selectors |= selector << 18;

	for (i = 0; i < 3; i++) {
		*pos++ = selectors & 0xff;
		selectors >>= 8;
	}

	return true;
}

#endif /* CONFIG_FILS */
