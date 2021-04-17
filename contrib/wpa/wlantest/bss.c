/*
 * BSS list
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/sha1.h"
#include "wlantest.h"


struct wlantest_bss * bss_find(struct wlantest *wt, const u8 *bssid)
{
	struct wlantest_bss *bss;

	dl_list_for_each(bss, &wt->bss, struct wlantest_bss, list) {
		if (os_memcmp(bss->bssid, bssid, ETH_ALEN) == 0)
			return bss;
	}

	return NULL;
}


struct wlantest_bss * bss_get(struct wlantest *wt, const u8 *bssid)
{
	struct wlantest_bss *bss;

	if (bssid[0] & 0x01)
		return NULL; /* Skip group addressed frames */

	bss = bss_find(wt, bssid);
	if (bss)
		return bss;

	bss = os_zalloc(sizeof(*bss));
	if (bss == NULL)
		return NULL;
	dl_list_init(&bss->sta);
	dl_list_init(&bss->pmk);
	dl_list_init(&bss->tdls);
	os_memcpy(bss->bssid, bssid, ETH_ALEN);
	dl_list_add(&wt->bss, &bss->list);
	wpa_printf(MSG_DEBUG, "Discovered new BSS - " MACSTR,
		   MAC2STR(bss->bssid));
	return bss;
}


void pmk_deinit(struct wlantest_pmk *pmk)
{
	dl_list_del(&pmk->list);
	os_free(pmk);
}


void tdls_deinit(struct wlantest_tdls *tdls)
{
	dl_list_del(&tdls->list);
	os_free(tdls);
}


void bss_deinit(struct wlantest_bss *bss)
{
	struct wlantest_sta *sta, *n;
	struct wlantest_pmk *pmk, *np;
	struct wlantest_tdls *tdls, *nt;
	dl_list_for_each_safe(sta, n, &bss->sta, struct wlantest_sta, list)
		sta_deinit(sta);
	dl_list_for_each_safe(pmk, np, &bss->pmk, struct wlantest_pmk, list)
		pmk_deinit(pmk);
	dl_list_for_each_safe(tdls, nt, &bss->tdls, struct wlantest_tdls, list)
		tdls_deinit(tdls);
	dl_list_del(&bss->list);
	os_free(bss);
}


int bss_add_pmk_from_passphrase(struct wlantest_bss *bss,
				const char *passphrase)
{
	struct wlantest_pmk *pmk;

	pmk = os_zalloc(sizeof(*pmk));
	if (pmk == NULL)
		return -1;
	if (pbkdf2_sha1(passphrase, bss->ssid, bss->ssid_len, 4096,
			pmk->pmk, PMK_LEN) < 0) {
		os_free(pmk);
		return -1;
	}

	wpa_printf(MSG_INFO, "Add possible PMK for BSSID " MACSTR
		   " based on passphrase '%s'",
		   MAC2STR(bss->bssid), passphrase);
	wpa_hexdump(MSG_DEBUG, "Possible PMK", pmk->pmk, PMK_LEN);
	dl_list_add(&bss->pmk, &pmk->list);

	return 0;
}


static void bss_add_pmk(struct wlantest *wt, struct wlantest_bss *bss)
{
	struct wlantest_passphrase *p;

	dl_list_for_each(p, &wt->passphrase, struct wlantest_passphrase, list)
	{
		if (!is_zero_ether_addr(p->bssid) &&
		    os_memcmp(p->bssid, bss->bssid, ETH_ALEN) != 0)
			continue;
		if (p->ssid_len &&
		    (p->ssid_len != bss->ssid_len ||
		     os_memcmp(p->ssid, bss->ssid, p->ssid_len) != 0))
			continue;

		if (bss_add_pmk_from_passphrase(bss, p->passphrase) < 0)
			break;
	}
}


void bss_update(struct wlantest *wt, struct wlantest_bss *bss,
		struct ieee802_11_elems *elems, int beacon)
{
	struct wpa_ie_data data;
	int update = 0;

	if (bss->capab_info != bss->prev_capab_info)
		update = 1;

	if (beacon && (!elems->ssid || elems->ssid_len > 32)) {
		wpa_printf(MSG_INFO,
			   "Invalid or missing SSID in a %s frame for " MACSTR,
			   beacon == 1 ? "Beacon" : "Probe Response",
			   MAC2STR(bss->bssid));
		bss->parse_error_reported = 1;
		return;
	}

	if (beacon &&
	    (bss->ssid_len != elems->ssid_len ||
	     os_memcmp(bss->ssid, elems->ssid, bss->ssid_len) != 0)) {
		wpa_printf(MSG_DEBUG, "Store SSID '%s' for BSSID " MACSTR,
			   wpa_ssid_txt(elems->ssid, elems->ssid_len),
			   MAC2STR(bss->bssid));
		os_memcpy(bss->ssid, elems->ssid, elems->ssid_len);
		bss->ssid_len = elems->ssid_len;
		bss_add_pmk(wt, bss);
	}

	if (elems->osen == NULL) {
		if (bss->osenie[0]) {
			add_note(wt, MSG_INFO, "BSS " MACSTR
				 " - OSEN IE removed", MAC2STR(bss->bssid));
			bss->rsnie[0] = 0;
			update = 1;
		}
	} else {
		if (bss->osenie[0] == 0 ||
		    os_memcmp(bss->osenie, elems->osen - 2,
			      elems->osen_len + 2) != 0) {
			wpa_printf(MSG_INFO, "BSS " MACSTR " - OSEN IE "
				   "stored", MAC2STR(bss->bssid));
			wpa_hexdump(MSG_DEBUG, "OSEN IE", elems->osen - 2,
				    elems->osen_len + 2);
			update = 1;
		}
		os_memcpy(bss->osenie, elems->osen - 2,
			  elems->osen_len + 2);
	}

	/* S1G does not include RSNE in beacon, so only clear it from
	 * Probe Response frames. Note this assumes short beacons were dropped
	 * due to missing SSID above.
	 */
	if (!elems->rsn_ie && (!elems->s1g_capab || beacon != 1)) {
		if (bss->rsnie[0]) {
			add_note(wt, MSG_INFO, "BSS " MACSTR
				 " - RSN IE removed", MAC2STR(bss->bssid));
			bss->rsnie[0] = 0;
			update = 1;
		}
	} else if (elems->rsn_ie) {
		if (bss->rsnie[0] == 0 ||
		    os_memcmp(bss->rsnie, elems->rsn_ie - 2,
			      elems->rsn_ie_len + 2) != 0) {
			wpa_printf(MSG_INFO, "BSS " MACSTR " - RSN IE "
				   "stored", MAC2STR(bss->bssid));
			wpa_hexdump(MSG_DEBUG, "RSN IE", elems->rsn_ie - 2,
				    elems->rsn_ie_len + 2);
			update = 1;
		}
		os_memcpy(bss->rsnie, elems->rsn_ie - 2,
			  elems->rsn_ie_len + 2);
	}

	if (elems->wpa_ie == NULL) {
		if (bss->wpaie[0]) {
			add_note(wt, MSG_INFO, "BSS " MACSTR
				 " - WPA IE removed", MAC2STR(bss->bssid));
			bss->wpaie[0] = 0;
			update = 1;
		}
	} else {
		if (bss->wpaie[0] == 0 ||
		    os_memcmp(bss->wpaie, elems->wpa_ie - 2,
			      elems->wpa_ie_len + 2) != 0) {
			wpa_printf(MSG_INFO, "BSS " MACSTR " - WPA IE "
				   "stored", MAC2STR(bss->bssid));
			wpa_hexdump(MSG_DEBUG, "WPA IE", elems->wpa_ie - 2,
				    elems->wpa_ie_len + 2);
			update = 1;
		}
		os_memcpy(bss->wpaie, elems->wpa_ie - 2,
			  elems->wpa_ie_len + 2);
	}

	if (elems->mdie)
		os_memcpy(bss->mdid, elems->mdie, 2);

	bss->mesh = elems->mesh_id != NULL;

	if (!update)
		return;

	if (beacon == 1)
		bss->beacon_seen = 1;
	else if (beacon == 2)
		bss->proberesp_seen = 1;
	bss->ies_set = 1;
	bss->prev_capab_info = bss->capab_info;
	bss->proto = 0;
	bss->pairwise_cipher = 0;
	bss->group_cipher = 0;
	bss->key_mgmt = 0;
	bss->rsn_capab = 0;
	bss->mgmt_group_cipher = 0;

	if (bss->wpaie[0]) {
		if (wpa_parse_wpa_ie_wpa(bss->wpaie, 2 + bss->wpaie[1], &data)
		    < 0) {
			add_note(wt, MSG_INFO, "Failed to parse WPA IE from "
				 MACSTR, MAC2STR(bss->bssid));
		} else {
			bss->proto |= data.proto;
			bss->pairwise_cipher |= data.pairwise_cipher;
			bss->group_cipher |= data.group_cipher;
			bss->key_mgmt |= data.key_mgmt;
			bss->rsn_capab = data.capabilities;
			bss->mgmt_group_cipher |= data.mgmt_group_cipher;
		}
	}

	if (bss->rsnie[0]) {
		if (wpa_parse_wpa_ie_rsn(bss->rsnie, 2 + bss->rsnie[1], &data)
		    < 0) {
			add_note(wt, MSG_INFO, "Failed to parse RSN IE from "
				 MACSTR, MAC2STR(bss->bssid));
		} else {
			bss->proto |= data.proto;
			bss->pairwise_cipher |= data.pairwise_cipher;
			bss->group_cipher |= data.group_cipher;
			bss->key_mgmt |= data.key_mgmt;
			bss->rsn_capab = data.capabilities;
			bss->mgmt_group_cipher |= data.mgmt_group_cipher;
		}
	}

	if (bss->osenie[0]) {
		bss->proto |= WPA_PROTO_OSEN;
		bss->pairwise_cipher |= WPA_CIPHER_CCMP;
		bss->group_cipher |= WPA_CIPHER_CCMP;
		bss->key_mgmt |= WPA_KEY_MGMT_OSEN;
	}

	if (!(bss->proto & WPA_PROTO_RSN) ||
	    !(bss->rsn_capab & WPA_CAPABILITY_MFPC))
		bss->mgmt_group_cipher = 0;

	if (!bss->wpaie[0] && !bss->rsnie[0] && !bss->osenie[0] &&
	    (bss->capab_info & WLAN_CAPABILITY_PRIVACY))
		bss->group_cipher = WPA_CIPHER_WEP40;

	wpa_printf(MSG_INFO, "BSS " MACSTR
		   " proto=%s%s%s%s"
		   "pairwise=%s%s%s%s%s%s%s"
		   "group=%s%s%s%s%s%s%s%s%s"
		   "mgmt_group_cipher=%s%s%s%s%s"
		   "key_mgmt=%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
		   "rsn_capab=%s%s%s%s%s%s%s%s%s%s",
		   MAC2STR(bss->bssid),
		   bss->proto == 0 ? "OPEN " : "",
		   bss->proto & WPA_PROTO_WPA ? "WPA " : "",
		   bss->proto & WPA_PROTO_RSN ? "WPA2 " : "",
		   bss->proto & WPA_PROTO_OSEN ? "OSEN " : "",
		   bss->pairwise_cipher == 0 ? "N/A " : "",
		   bss->pairwise_cipher & WPA_CIPHER_NONE ? "NONE " : "",
		   bss->pairwise_cipher & WPA_CIPHER_TKIP ? "TKIP " : "",
		   bss->pairwise_cipher & WPA_CIPHER_CCMP ? "CCMP " : "",
		   bss->pairwise_cipher & WPA_CIPHER_CCMP_256 ? "CCMP-256 " :
		   "",
		   bss->pairwise_cipher & WPA_CIPHER_GCMP ? "GCMP " : "",
		   bss->pairwise_cipher & WPA_CIPHER_GCMP_256 ? "GCMP-256 " :
		   "",
		   bss->group_cipher == 0 ? "N/A " : "",
		   bss->group_cipher & WPA_CIPHER_NONE ? "NONE " : "",
		   bss->group_cipher & WPA_CIPHER_WEP40 ? "WEP40 " : "",
		   bss->group_cipher & WPA_CIPHER_WEP104 ? "WEP104 " : "",
		   bss->group_cipher & WPA_CIPHER_TKIP ? "TKIP " : "",
		   bss->group_cipher & WPA_CIPHER_CCMP ? "CCMP " : "",
		   bss->group_cipher & WPA_CIPHER_CCMP_256 ? "CCMP-256 " : "",
		   bss->group_cipher & WPA_CIPHER_GCMP ? "GCMP " : "",
		   bss->group_cipher & WPA_CIPHER_GCMP_256 ? "GCMP-256 " : "",
		   bss->mgmt_group_cipher == 0 ? "N/A " : "",
		   bss->mgmt_group_cipher & WPA_CIPHER_AES_128_CMAC ?
		   "BIP " : "",
		   bss->mgmt_group_cipher & WPA_CIPHER_BIP_GMAC_128 ?
		   "BIP-GMAC-128 " : "",
		   bss->mgmt_group_cipher & WPA_CIPHER_BIP_GMAC_256 ?
		   "BIP-GMAC-256 " : "",
		   bss->mgmt_group_cipher & WPA_CIPHER_BIP_CMAC_256 ?
		   "BIP-CMAC-256 " : "",
		   bss->key_mgmt == 0 ? "N/A " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_IEEE8021X ? "EAP " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_PSK ? "PSK " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_WPA_NONE ? "WPA-NONE " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X ? "FT-EAP " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_FT_PSK ? "FT-PSK " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256 ?
		   "EAP-SHA256 " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_PSK_SHA256 ?
		   "PSK-SHA256 " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_OWE ? "OWE " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_PASN ? "PASN " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_OSEN ? "OSEN " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_DPP ? "DPP " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B ?
		   "EAP-SUITE-B " : "",
		   bss->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192 ?
		   "EAP-SUITE-B-192 " : "",
		   bss->rsn_capab & WPA_CAPABILITY_PREAUTH ? "PREAUTH " : "",
		   bss->rsn_capab & WPA_CAPABILITY_NO_PAIRWISE ?
		   "NO_PAIRWISE " : "",
		   bss->rsn_capab & WPA_CAPABILITY_MFPR ? "MFPR " : "",
		   bss->rsn_capab & WPA_CAPABILITY_MFPC ? "MFPC " : "",
		   bss->rsn_capab & WPA_CAPABILITY_PEERKEY_ENABLED ?
		   "PEERKEY " : "",
		   bss->rsn_capab & WPA_CAPABILITY_SPP_A_MSDU_CAPABLE ?
		   "SPP-A-MSDU-CAPAB " : "",
		   bss->rsn_capab & WPA_CAPABILITY_SPP_A_MSDU_REQUIRED ?
		   "SPP-A-MSDU-REQUIRED " : "",
		   bss->rsn_capab & WPA_CAPABILITY_PBAC ? "PBAC " : "",
		   bss->rsn_capab & WPA_CAPABILITY_OCVC ? "OCVC " : "",
		   bss->rsn_capab & WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST ?
		   "ExtKeyID " : "");
}


void bss_flush(struct wlantest *wt)
{
	struct wlantest_bss *bss, *n;
	dl_list_for_each_safe(bss, n, &wt->bss, struct wlantest_bss, list)
		bss_deinit(bss);
}
