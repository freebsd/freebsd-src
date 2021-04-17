/*
 * Received Management frame processing
 * Copyright (c) 2010-2020, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "crypto/aes_wrap.h"
#include "wlantest.h"


static int check_mmie_mic(unsigned int mgmt_group_cipher,
			  const u8 *igtk, size_t igtk_len,
			  const u8 *data, size_t len);


static const char * mgmt_stype(u16 stype)
{
	switch (stype) {
	case WLAN_FC_STYPE_ASSOC_REQ:
		return "ASSOC-REQ";
	case WLAN_FC_STYPE_ASSOC_RESP:
		return "ASSOC-RESP";
	case WLAN_FC_STYPE_REASSOC_REQ:
		return "REASSOC-REQ";
	case WLAN_FC_STYPE_REASSOC_RESP:
		return "REASSOC-RESP";
	case WLAN_FC_STYPE_PROBE_REQ:
		return "PROBE-REQ";
	case WLAN_FC_STYPE_PROBE_RESP:
		return "PROBE-RESP";
	case WLAN_FC_STYPE_BEACON:
		return "BEACON";
	case WLAN_FC_STYPE_ATIM:
		return "ATIM";
	case WLAN_FC_STYPE_DISASSOC:
		return "DISASSOC";
	case WLAN_FC_STYPE_AUTH:
		return "AUTH";
	case WLAN_FC_STYPE_DEAUTH:
		return "DEAUTH";
	case WLAN_FC_STYPE_ACTION:
		return "ACTION";
	case WLAN_FC_STYPE_ACTION_NO_ACK:
		return "ACTION-NO-ACK";
	}
	return "??";
}


static void rx_mgmt_beacon(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct ieee802_11_elems elems;
	size_t offset;
	const u8 *mme;
	size_t mic_len;
	u16 keyid;

	mgmt = (const struct ieee80211_mgmt *) data;
	offset = mgmt->u.beacon.variable - data;
	if (len < offset)
		return;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (bss->proberesp_seen)
		return; /* do not override with Beacon data */
	bss->capab_info = le_to_host16(mgmt->u.beacon.capab_info);
	if (ieee802_11_parse_elems(mgmt->u.beacon.variable, len - offset,
				   &elems, 0) == ParseFailed) {
		if (bss->parse_error_reported)
			return;
		add_note(wt, MSG_INFO, "Invalid IEs in a Beacon frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		bss->parse_error_reported = 1;
		return;
	}

	bss_update(wt, bss, &elems, 1);

	mme = get_ie(mgmt->u.beacon.variable, len - offset, WLAN_EID_MMIE);
	if (!mme) {
		if (bss->bigtk_idx) {
			add_note(wt, MSG_INFO,
				 "Unexpected unprotected Beacon frame from "
				 MACSTR, MAC2STR(mgmt->sa));
			bss->counters[WLANTEST_BSS_COUNTER_MISSING_BIP_MMIE]++;
		}
		return;
	}

	mic_len = bss->mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC ? 8 : 16;
	if (len < 24 + 10 + mic_len ||
	    data[len - (10 + mic_len)] != WLAN_EID_MMIE ||
	    data[len - (10 + mic_len - 1)] != 8 + mic_len) {
		add_note(wt, MSG_INFO, "Invalid MME in a Beacon frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	mme += 2;
	keyid = WPA_GET_LE16(mme);
	if (keyid < 6 || keyid > 7) {
		add_note(wt, MSG_INFO, "Unexpected MME KeyID %u from " MACSTR,
			 keyid, MAC2STR(mgmt->sa));
		bss->counters[WLANTEST_BSS_COUNTER_INVALID_BIP_MMIE]++;
		return;
	}

	wpa_printf(MSG_DEBUG, "Beacon frame MME KeyID %u", keyid);
	wpa_hexdump(MSG_MSGDUMP, "MME IPN", mme + 2, 6);
	wpa_hexdump(MSG_MSGDUMP, "MME MIC", mme + 8, mic_len);

	if (!bss->igtk_len[keyid]) {
		add_note(wt, MSG_DEBUG, "No BIGTK known to validate BIP frame");
		return;
	}

	if (os_memcmp(mme + 2, bss->ipn[keyid], 6) <= 0) {
		add_note(wt, MSG_INFO, "BIP replay detected: SA=" MACSTR,
			 MAC2STR(mgmt->sa));
		wpa_hexdump(MSG_INFO, "RX IPN", mme + 2, 6);
		wpa_hexdump(MSG_INFO, "Last RX IPN", bss->ipn[keyid], 6);
	}

	if (check_mmie_mic(bss->mgmt_group_cipher, bss->igtk[keyid],
			   bss->igtk_len[keyid], data, len) < 0) {
		add_note(wt, MSG_INFO, "Invalid MME MIC in a Beacon frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		bss->counters[WLANTEST_BSS_COUNTER_INVALID_BIP_MMIE]++;
		return;
	}

	add_note(wt, MSG_DEBUG, "Valid MME MIC in Beacon frame");
	os_memcpy(bss->ipn[keyid], mme + 2, 6);
}


static void rx_mgmt_probe_resp(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct ieee802_11_elems elems;
	size_t offset;

	mgmt = (const struct ieee80211_mgmt *) data;
	offset = mgmt->u.probe_resp.variable - data;
	if (len < offset)
		return;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;

	bss->counters[WLANTEST_BSS_COUNTER_PROBE_RESPONSE]++;
	bss->capab_info = le_to_host16(mgmt->u.probe_resp.capab_info);
	if (ieee802_11_parse_elems(mgmt->u.probe_resp.variable, len - offset,
				   &elems, 0) == ParseFailed) {
		if (bss->parse_error_reported)
			return;
		add_note(wt, MSG_INFO, "Invalid IEs in a Probe Response frame "
			 "from " MACSTR, MAC2STR(mgmt->sa));
		bss->parse_error_reported = 1;
		return;
	}

	bss_update(wt, bss, &elems, 2);
}


static void process_fils_auth(struct wlantest *wt, struct wlantest_bss *bss,
			      struct wlantest_sta *sta,
			      const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct ieee802_11_elems elems;
	u16 trans;
	struct wpa_ie_data data;

	if (sta->auth_alg != WLAN_AUTH_FILS_SK ||
	    len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth))
		return;

	trans = le_to_host16(mgmt->u.auth.auth_transaction);

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - IEEE80211_HDRLEN -
				   sizeof(mgmt->u.auth), &elems, 0) ==
	    ParseFailed)
		return;

	if (trans == 1) {
		if (!elems.rsn_ie) {
			add_note(wt, MSG_INFO,
				 "FILS Authentication frame missing RSNE");
			return;
		}
		if (wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2,
					 elems.rsn_ie_len + 2, &data) < 0) {
			add_note(wt, MSG_INFO,
				 "Invalid RSNE in FILS Authentication frame");
			return;
		}
		sta->key_mgmt = data.key_mgmt;
		sta->pairwise_cipher = data.pairwise_cipher;
	}

	if (!elems.fils_nonce) {
		add_note(wt, MSG_INFO,
			 "FILS Authentication frame missing nonce");
		return;
	}

	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		os_memcpy(sta->anonce, elems.fils_nonce, FILS_NONCE_LEN);
	else
		os_memcpy(sta->snonce, elems.fils_nonce, FILS_NONCE_LEN);
}


static void process_ft_auth(struct wlantest *wt, struct wlantest_bss *bss,
			    struct wlantest_sta *sta,
			    const struct ieee80211_mgmt *mgmt, size_t len)
{
	u16 trans;
	struct wpa_ft_ies parse;
	struct wpa_ptk ptk;
	u8 ptk_name[WPA_PMK_NAME_LEN];
	struct wlantest_bss *old_bss;
	struct wlantest_sta *old_sta = NULL;

	if (sta->auth_alg != WLAN_AUTH_FT ||
	    len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth))
		return;

	trans = le_to_host16(mgmt->u.auth.auth_transaction);

	if (wpa_ft_parse_ies(mgmt->u.auth.variable,
			     len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth),
			     &parse, -1)) {
		add_note(wt, MSG_INFO,
			 "Could not parse FT Authentication Response frame");
		return;
	}

	if (trans == 1) {
		sta->key_mgmt = parse.key_mgmt;
		sta->pairwise_cipher = parse.pairwise_cipher;
		return;
	}

	if (trans != 2)
		return;

	/* TODO: Should find the latest updated PMK-R0 value here instead
	 * copying the one from the first found matching old STA entry. */
	dl_list_for_each(old_bss, &wt->bss, struct wlantest_bss, list) {
		if (old_bss == bss)
			continue;
		old_sta = sta_find(old_bss, sta->addr);
		if (old_sta)
			break;
	}
	if (!old_sta)
		return;

	os_memcpy(sta->pmk_r0, old_sta->pmk_r0, old_sta->pmk_r0_len);
	sta->pmk_r0_len = old_sta->pmk_r0_len;
	os_memcpy(sta->pmk_r0_name, old_sta->pmk_r0_name,
		  sizeof(sta->pmk_r0_name));

	if (parse.r1kh_id)
		os_memcpy(bss->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);

	if (wpa_derive_pmk_r1(sta->pmk_r0, sta->pmk_r0_len, sta->pmk_r0_name,
			      bss->r1kh_id, sta->addr, sta->pmk_r1,
			      sta->pmk_r1_name) < 0)
		return;
	sta->pmk_r1_len = sta->pmk_r0_len;

	if (!parse.fte_anonce || !parse.fte_snonce ||
	    wpa_pmk_r1_to_ptk(sta->pmk_r1, sta->pmk_r1_len, parse.fte_snonce,
			      parse.fte_anonce, sta->addr, bss->bssid,
			      sta->pmk_r1_name, &ptk, ptk_name, sta->key_mgmt,
			      sta->pairwise_cipher, 0) < 0)
		return;

	add_note(wt, MSG_DEBUG, "Derived new PTK");
	os_memcpy(&sta->ptk, &ptk, sizeof(ptk));
	sta->ptk_set = 1;
	os_memset(sta->rsc_tods, 0, sizeof(sta->rsc_tods));
	os_memset(sta->rsc_fromds, 0, sizeof(sta->rsc_fromds));
}


static void rx_mgmt_auth(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u16 alg, trans, status;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	if (len < 24 + 6) {
		add_note(wt, MSG_INFO, "Too short Authentication frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	alg = le_to_host16(mgmt->u.auth.auth_alg);
	sta->auth_alg = alg;
	trans = le_to_host16(mgmt->u.auth.auth_transaction);
	status = le_to_host16(mgmt->u.auth.status_code);

	wpa_printf(MSG_DEBUG, "AUTH " MACSTR " -> " MACSTR
		   " (alg=%u trans=%u status=%u)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), alg, trans, status);

	if (alg == 0 && trans == 2 && status == 0) {
		if (sta->state == STATE1) {
			add_note(wt, MSG_DEBUG, "STA " MACSTR
				 " moved to State 2 with " MACSTR,
				 MAC2STR(sta->addr), MAC2STR(bss->bssid));
			sta->state = STATE2;
		}
	}

	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta->counters[WLANTEST_STA_COUNTER_AUTH_RX]++;
	else
		sta->counters[WLANTEST_STA_COUNTER_AUTH_TX]++;

	process_fils_auth(wt, bss, sta, mgmt, len);
	process_ft_auth(wt, bss, sta, mgmt, len);
}


static void deauth_all_stas(struct wlantest *wt, struct wlantest_bss *bss)
{
	struct wlantest_sta *sta;
	dl_list_for_each(sta, &bss->sta, struct wlantest_sta, list) {
		if (sta->state == STATE1)
			continue;
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 1 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE1;
	}
}


static void tdls_link_down(struct wlantest *wt, struct wlantest_bss *bss,
			   struct wlantest_sta *sta)
{
	struct wlantest_tdls *tdls;
	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if ((tdls->init == sta || tdls->resp == sta) && tdls->link_up)
		{
			add_note(wt, MSG_DEBUG, "TDLS: Set link down based on "
				 "STA deauth/disassoc");
			tdls->link_up = 0;
		}
	}
}


static void rx_mgmt_deauth(struct wlantest *wt, const u8 *data, size_t len,
			   int valid)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u16 fc, reason;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);

	if (len < 24 + 2) {
		add_note(wt, MSG_INFO, "Too short Deauthentication frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	reason = le_to_host16(mgmt->u.deauth.reason_code);
	wpa_printf(MSG_DEBUG, "DEAUTH " MACSTR " -> " MACSTR
		   " (reason=%u) (valid=%d)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da),
		   reason, valid);
	wpa_hexdump(MSG_MSGDUMP, "DEAUTH payload", data + 24, len - 24);

	if (sta == NULL) {
		if (valid && mgmt->da[0] == 0xff)
			deauth_all_stas(wt, bss);
		return;
	}

	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0) {
		sta->counters[valid ? WLANTEST_STA_COUNTER_VALID_DEAUTH_RX :
			      WLANTEST_STA_COUNTER_INVALID_DEAUTH_RX]++;
		if (sta->pwrmgt && !sta->pspoll)
			sta->counters[WLANTEST_STA_COUNTER_DEAUTH_RX_ASLEEP]++;
		else
			sta->counters[WLANTEST_STA_COUNTER_DEAUTH_RX_AWAKE]++;

		fc = le_to_host16(mgmt->frame_control);
		if (!(fc & WLAN_FC_ISWEP) && reason == 6)
			sta->counters[WLANTEST_STA_COUNTER_DEAUTH_RX_RC6]++;
		else if (!(fc & WLAN_FC_ISWEP) && reason == 7)
			sta->counters[WLANTEST_STA_COUNTER_DEAUTH_RX_RC7]++;
	} else
		sta->counters[valid ? WLANTEST_STA_COUNTER_VALID_DEAUTH_TX :
			      WLANTEST_STA_COUNTER_INVALID_DEAUTH_TX]++;

	if (!valid) {
		add_note(wt, MSG_INFO, "Do not change STA " MACSTR " State "
			 "since Disassociation frame was not protected "
			 "correctly", MAC2STR(sta->addr));
		return;
	}

	if (sta->state != STATE1) {
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 1 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE1;
	}
	tdls_link_down(wt, bss, sta);
}


static const u8 * get_fils_session(const u8 *ies, size_t ies_len)
{
	const u8 *ie, *end;

	ie = ies;
	end = ((const u8 *) ie) + ies_len;
	while (ie + 1 < end) {
		if (ie + 2 + ie[1] > end)
			break;
		if (ie[0] == WLAN_EID_EXTENSION &&
		    ie[1] >= 1 + FILS_SESSION_LEN &&
		    ie[2] == WLAN_EID_EXT_FILS_SESSION)
			return ie;
		ie += 2 + ie[1];
	}
	return NULL;
}


static int try_rmsk(struct wlantest *wt, struct wlantest_bss *bss,
		    struct wlantest_sta *sta, struct wlantest_pmk *pmk,
		    const u8 *frame_start, const u8 *frame_ad,
		    const u8 *frame_ad_end, const u8 *encr_end)
{
	size_t pmk_len = 0;
	u8 pmk_buf[PMK_LEN_MAX];
	struct wpa_ptk ptk;
	u8 ick[FILS_ICK_MAX_LEN];
	size_t ick_len;
	const u8 *aad[5];
	size_t aad_len[5];
	u8 buf[2000];

	if (fils_rmsk_to_pmk(sta->key_mgmt, pmk->pmk, pmk->pmk_len,
			     sta->snonce, sta->anonce, NULL, 0,
			     pmk_buf, &pmk_len) < 0)
		return -1;

	if (fils_pmk_to_ptk(pmk_buf, pmk_len, sta->addr, bss->bssid,
			    sta->snonce, sta->anonce, NULL, 0,
			    &ptk, ick, &ick_len,
			    sta->key_mgmt, sta->pairwise_cipher,
			    NULL, NULL, 0) < 0)
		return -1;

	/* Check AES-SIV decryption with the derived key */

	/* AES-SIV AAD vectors */

	/* The STA's MAC address */
	aad[0] = sta->addr;
	aad_len[0] = ETH_ALEN;
	/* The AP's BSSID */
	aad[1] = bss->bssid;
	aad_len[1] = ETH_ALEN;
	/* The STA's nonce */
	aad[2] = sta->snonce;
	aad_len[2] = FILS_NONCE_LEN;
	/* The AP's nonce */
	aad[3] = sta->anonce;
	aad_len[3] = FILS_NONCE_LEN;
	/*
	 * The (Re)Association Request frame from the Capability Information
	 * field to the FILS Session element (both inclusive).
	 */
	aad[4] = frame_ad;
	aad_len[4] = frame_ad_end - frame_ad;

	if (encr_end - frame_ad_end < AES_BLOCK_SIZE ||
	    encr_end - frame_ad_end > sizeof(buf))
		return -1;
	if (aes_siv_decrypt(ptk.kek, ptk.kek_len,
			    frame_ad_end, encr_end - frame_ad_end,
			    5, aad, aad_len, buf) < 0) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Derived PTK did not match AES-SIV data");
		return -1;
	}

	add_note(wt, MSG_DEBUG, "Derived FILS PTK");
	os_memcpy(&sta->ptk, &ptk, sizeof(ptk));
	sta->ptk_set = 1;
	sta->counters[WLANTEST_STA_COUNTER_PTK_LEARNED]++;
	wpa_hexdump(MSG_DEBUG, "FILS: Decrypted Association Request elements",
		    buf, encr_end - frame_ad_end - AES_BLOCK_SIZE);

	if (wt->write_pcap_dumper || wt->pcapng) {
		write_pcap_decrypted(wt, frame_start,
				     frame_ad_end - frame_start,
				     buf,
				     encr_end - frame_ad_end - AES_BLOCK_SIZE);
	}

	return 0;
}


static void derive_fils_keys(struct wlantest *wt, struct wlantest_bss *bss,
			     struct wlantest_sta *sta, const u8 *frame_start,
			     const u8 *frame_ad, const u8 *frame_ad_end,
			     const u8 *encr_end)
{
	struct wlantest_pmk *pmk;

	wpa_printf(MSG_DEBUG, "Trying to derive PTK for " MACSTR
		   " from FILS rMSK", MAC2STR(sta->addr));

	dl_list_for_each(pmk, &bss->pmk, struct wlantest_pmk,
			 list) {
		wpa_printf(MSG_DEBUG, "Try per-BSS PMK");
		if (try_rmsk(wt, bss, sta, pmk, frame_start, frame_ad,
			     frame_ad_end, encr_end) == 0)
			return;
	}

	dl_list_for_each(pmk, &wt->pmk, struct wlantest_pmk, list) {
		wpa_printf(MSG_DEBUG, "Try global PMK");
		if (try_rmsk(wt, bss, sta, pmk, frame_start, frame_ad,
			     frame_ad_end, encr_end) == 0)
			return;
	}
}


static void rx_mgmt_assoc_req(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ie_len;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	if (len < 24 + 4) {
		add_note(wt, MSG_INFO, "Too short Association Request frame "
			 "from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "ASSOCREQ " MACSTR " -> " MACSTR
		   " (capab=0x%x listen_int=%u)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da),
		   le_to_host16(mgmt->u.assoc_req.capab_info),
		   le_to_host16(mgmt->u.assoc_req.listen_interval));

	sta->counters[WLANTEST_STA_COUNTER_ASSOCREQ_TX]++;

	ie = mgmt->u.assoc_req.variable;
	ie_len = len - (mgmt->u.assoc_req.variable - data);

	if (sta->auth_alg == WLAN_AUTH_FILS_SK) {
		const u8 *session, *frame_ad, *frame_ad_end, *encr_end;

		session = get_fils_session(ie, ie_len);
		if (session) {
			frame_ad = (const u8 *) &mgmt->u.assoc_req.capab_info;
			frame_ad_end = session + 2 + session[1];
			encr_end = data + len;
			derive_fils_keys(wt, bss, sta, data, frame_ad,
					 frame_ad_end, encr_end);
			ie_len = session - ie;
		}
	}

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		add_note(wt, MSG_INFO, "Invalid IEs in Association Request "
			 "frame from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	sta->assocreq_capab_info = le_to_host16(mgmt->u.assoc_req.capab_info);
	sta->assocreq_listen_int =
		le_to_host16(mgmt->u.assoc_req.listen_interval);
	os_free(sta->assocreq_ies);
	sta->assocreq_ies_len = len - (mgmt->u.assoc_req.variable - data);
	sta->assocreq_ies = os_malloc(sta->assocreq_ies_len);
	if (sta->assocreq_ies)
		os_memcpy(sta->assocreq_ies, mgmt->u.assoc_req.variable,
			  sta->assocreq_ies_len);

	sta->assocreq_seen = 1;
	sta_update_assoc(sta, &elems);
}


static void decrypt_fils_assoc_resp(struct wlantest *wt,
				    struct wlantest_bss *bss,
				    struct wlantest_sta *sta,
				    const u8 *frame_start, const u8 *frame_ad,
				    const u8 *frame_ad_end, const u8 *encr_end)
{
	const u8 *aad[5];
	size_t aad_len[5];
	u8 buf[2000];

	if (!sta->ptk_set)
		return;

	/* Check AES-SIV decryption with the derived key */

	/* AES-SIV AAD vectors */

	/* The AP's BSSID */
	aad[0] = bss->bssid;
	aad_len[0] = ETH_ALEN;
	/* The STA's MAC address */
	aad[1] = sta->addr;
	aad_len[1] = ETH_ALEN;
	/* The AP's nonce */
	aad[2] = sta->anonce;
	aad_len[2] = FILS_NONCE_LEN;
	/* The STA's nonce */
	aad[3] = sta->snonce;
	aad_len[3] = FILS_NONCE_LEN;
	/*
	 * The (Re)Association Response frame from the Capability Information
	 * field to the FILS Session element (both inclusive).
	 */
	aad[4] = frame_ad;
	aad_len[4] = frame_ad_end - frame_ad;

	if (encr_end - frame_ad_end < AES_BLOCK_SIZE ||
	    encr_end - frame_ad_end > sizeof(buf))
		return;
	if (aes_siv_decrypt(sta->ptk.kek, sta->ptk.kek_len,
			    frame_ad_end, encr_end - frame_ad_end,
			    5, aad, aad_len, buf) < 0) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Derived PTK did not match AES-SIV data");
		return;
	}

	wpa_hexdump(MSG_DEBUG, "FILS: Decrypted Association Response elements",
		    buf, encr_end - frame_ad_end - AES_BLOCK_SIZE);

	if (wt->write_pcap_dumper || wt->pcapng) {
		write_pcap_decrypted(wt, frame_start,
				     frame_ad_end - frame_start,
				     buf,
				     encr_end - frame_ad_end - AES_BLOCK_SIZE);
	}
}


static void rx_mgmt_assoc_resp(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u16 capab, status, aid;
	const u8 *ies;
	size_t ies_len;
	struct wpa_ft_ies parse;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	sta = sta_get(bss, mgmt->da);
	if (sta == NULL)
		return;

	if (len < 24 + 6) {
		add_note(wt, MSG_INFO, "Too short Association Response frame "
			 "from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	ies = mgmt->u.assoc_resp.variable;
	ies_len = len - (mgmt->u.assoc_resp.variable - data);

	capab = le_to_host16(mgmt->u.assoc_resp.capab_info);
	status = le_to_host16(mgmt->u.assoc_resp.status_code);
	aid = le_to_host16(mgmt->u.assoc_resp.aid);

	wpa_printf(MSG_DEBUG, "ASSOCRESP " MACSTR " -> " MACSTR
		   " (capab=0x%x status=%u aid=%u)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), capab, status,
		   aid & 0x3fff);

	if (sta->auth_alg == WLAN_AUTH_FILS_SK) {
		const u8 *session, *frame_ad, *frame_ad_end, *encr_end;

		session = get_fils_session(ies, ies_len);
		if (session) {
			frame_ad = (const u8 *) &mgmt->u.assoc_resp.capab_info;
			frame_ad_end = session + 2 + session[1];
			encr_end = data + len;
			decrypt_fils_assoc_resp(wt, bss, sta, data, frame_ad,
						frame_ad_end, encr_end);
			ies_len = session - ies;
		}
	}

	if (status == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY) {
		struct ieee802_11_elems elems;
		if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) ==
		    ParseFailed) {
			add_note(wt, MSG_INFO, "Failed to parse IEs in "
				 "AssocResp from " MACSTR,
				 MAC2STR(mgmt->sa));
		} else if (elems.timeout_int == NULL ||
			   elems.timeout_int[0] !=
			   WLAN_TIMEOUT_ASSOC_COMEBACK) {
			add_note(wt, MSG_INFO, "No valid Timeout Interval IE "
				 "with Assoc Comeback time in AssocResp "
				 "(status=30) from " MACSTR,
				 MAC2STR(mgmt->sa));
		} else {
			sta->counters[
				WLANTEST_STA_COUNTER_ASSOCRESP_COMEBACK]++;
		}
	}

	if (status)
		return;

	if ((aid & 0xc000) != 0xc000) {
		add_note(wt, MSG_DEBUG, "Two MSBs of the AID were not set to 1 "
			 "in Association Response from " MACSTR,
			 MAC2STR(mgmt->sa));
	}
	sta->aid = aid & 0xc000;

	if (sta->state < STATE2) {
		add_note(wt, MSG_DEBUG,
			 "STA " MACSTR " was not in State 2 when "
			 "getting associated", MAC2STR(sta->addr));
	}

	if (sta->state < STATE3) {
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 3 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE3;
	}

	if (wpa_ft_parse_ies(ies, ies_len, &parse, 0) == 0) {
		if (parse.r0kh_id) {
			os_memcpy(bss->r0kh_id, parse.r0kh_id,
				  parse.r0kh_id_len);
			bss->r0kh_id_len = parse.r0kh_id_len;
		}
		if (parse.r1kh_id)
			os_memcpy(bss->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);
	}
}


static void rx_mgmt_reassoc_req(struct wlantest *wt, const u8 *data,
				size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ie_len;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	if (len < 24 + 4 + ETH_ALEN) {
		add_note(wt, MSG_INFO, "Too short Reassociation Request frame "
			 "from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "REASSOCREQ " MACSTR " -> " MACSTR
		   " (capab=0x%x listen_int=%u current_ap=" MACSTR ")",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da),
		   le_to_host16(mgmt->u.reassoc_req.capab_info),
		   le_to_host16(mgmt->u.reassoc_req.listen_interval),
		   MAC2STR(mgmt->u.reassoc_req.current_ap));

	sta->counters[WLANTEST_STA_COUNTER_REASSOCREQ_TX]++;

	ie = mgmt->u.reassoc_req.variable;
	ie_len = len - (mgmt->u.reassoc_req.variable - data);

	if (sta->auth_alg == WLAN_AUTH_FILS_SK) {
		const u8 *session, *frame_ad, *frame_ad_end, *encr_end;

		session = get_fils_session(ie, ie_len);
		if (session) {
			frame_ad = (const u8 *) &mgmt->u.reassoc_req.capab_info;
			frame_ad_end = session + 2 + session[1];
			encr_end = data + len;
			derive_fils_keys(wt, bss, sta, data, frame_ad,
					 frame_ad_end, encr_end);
			ie_len = session - ie;
		}
	}

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		add_note(wt, MSG_INFO, "Invalid IEs in Reassociation Request "
			 "frame from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	sta->assocreq_capab_info =
		le_to_host16(mgmt->u.reassoc_req.capab_info);
	sta->assocreq_listen_int =
		le_to_host16(mgmt->u.reassoc_req.listen_interval);
	os_free(sta->assocreq_ies);
	sta->assocreq_ies_len = len - (mgmt->u.reassoc_req.variable - data);
	sta->assocreq_ies = os_malloc(sta->assocreq_ies_len);
	if (sta->assocreq_ies)
		os_memcpy(sta->assocreq_ies, mgmt->u.reassoc_req.variable,
			  sta->assocreq_ies_len);

	sta->assocreq_seen = 1;
	sta_update_assoc(sta, &elems);

	if (elems.ftie) {
		struct wpa_ft_ies parse;
		int use_sha384;
		struct rsn_mdie *mde;
		const u8 *anonce, *snonce, *fte_mic;
		u8 fte_elem_count;
		unsigned int count;
		u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN];
		size_t mic_len = 16;
		const u8 *kck;
		size_t kck_len;

		use_sha384 = wpa_key_mgmt_sha384(sta->key_mgmt);

		if (wpa_ft_parse_ies(ie, ie_len, &parse, use_sha384) < 0) {
			add_note(wt, MSG_INFO, "FT: Failed to parse FT IEs");
			return;
		}

		if (!parse.rsn) {
			add_note(wt, MSG_INFO, "FT: No RSNE in Reassoc Req");
			return;
		}

		if (!parse.rsn_pmkid) {
			add_note(wt, MSG_INFO, "FT: No PMKID in RSNE");
			return;
		}

		if (os_memcmp_const(parse.rsn_pmkid, sta->pmk_r1_name,
				    WPA_PMK_NAME_LEN) != 0) {
			add_note(wt, MSG_INFO,
				 "FT: PMKID in Reassoc Req did not match PMKR1Name");
			wpa_hexdump(MSG_DEBUG,
				    "FT: Received RSNE[PMKR1Name]",
				    parse.rsn_pmkid, WPA_PMK_NAME_LEN);
			wpa_hexdump(MSG_DEBUG,
				    "FT: Previously derived PMKR1Name",
				    sta->pmk_r1_name, WPA_PMK_NAME_LEN);
			return;
		}

		mde = (struct rsn_mdie *) parse.mdie;
		if (!mde || parse.mdie_len < sizeof(*mde) ||
		    os_memcmp(mde->mobility_domain, bss->mdid,
			      MOBILITY_DOMAIN_ID_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: Invalid MDE");
		}

		if (use_sha384) {
			struct rsn_ftie_sha384 *fte;

			fte = (struct rsn_ftie_sha384 *) parse.ftie;
			if (!fte || parse.ftie_len < sizeof(*fte)) {
				add_note(wt, MSG_INFO, "FT: Invalid FTE");
				return;
			}

			anonce = fte->anonce;
			snonce = fte->snonce;
			fte_elem_count = fte->mic_control[1];
			fte_mic = fte->mic;
		} else {
			struct rsn_ftie *fte;

			fte = (struct rsn_ftie *) parse.ftie;
			if (!fte || parse.ftie_len < sizeof(*fte)) {
				add_note(wt, MSG_INFO, "FT: Invalid FTIE");
				return;
			}

			anonce = fte->anonce;
			snonce = fte->snonce;
			fte_elem_count = fte->mic_control[1];
			fte_mic = fte->mic;
		}

		if (os_memcmp(snonce, sta->snonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: SNonce mismatch in FTIE");
			wpa_hexdump(MSG_DEBUG, "FT: Received SNonce",
				    snonce, WPA_NONCE_LEN);
			wpa_hexdump(MSG_DEBUG, "FT: Expected SNonce",
				    sta->snonce, WPA_NONCE_LEN);
			return;
		}

		if (os_memcmp(anonce, sta->anonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: ANonce mismatch in FTIE");
			wpa_hexdump(MSG_DEBUG, "FT: Received ANonce",
				    anonce, WPA_NONCE_LEN);
			wpa_hexdump(MSG_DEBUG, "FT: Expected ANonce",
				    sta->anonce, WPA_NONCE_LEN);
			return;
		}

		if (!parse.r0kh_id) {
			add_note(wt, MSG_INFO, "FT: No R0KH-ID subelem in FTE");
			return;
		}
		os_memcpy(bss->r0kh_id, parse.r0kh_id, parse.r0kh_id_len);
		bss->r0kh_id_len = parse.r0kh_id_len;

		if (!parse.r1kh_id) {
			add_note(wt, MSG_INFO, "FT: No R1KH-ID subelem in FTE");
			return;
		}

		os_memcpy(bss->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);

		if (!parse.rsn_pmkid ||
		    os_memcmp_const(parse.rsn_pmkid, sta->pmk_r1_name,
				    WPA_PMK_NAME_LEN)) {
			add_note(wt, MSG_INFO,
				 "FT: No matching PMKR1Name (PMKID) in RSNE (pmkid=%d)",
				 !!parse.rsn_pmkid);
			return;
		}

		count = 3;
		if (parse.ric)
			count += ieee802_11_ie_count(parse.ric, parse.ric_len);
		if (parse.rsnxe)
			count++;
		if (fte_elem_count != count) {
			add_note(wt, MSG_INFO,
				 "FT: Unexpected IE count in MIC Control: received %u expected %u",
				 fte_elem_count, count);
			return;
		}

		if (wpa_key_mgmt_fils(sta->key_mgmt)) {
			kck = sta->ptk.kck2;
			kck_len = sta->ptk.kck2_len;
		} else {
			kck = sta->ptk.kck;
			kck_len = sta->ptk.kck_len;
		}
		if (wpa_ft_mic(kck, kck_len, sta->addr, bss->bssid, 5,
			       parse.mdie - 2, parse.mdie_len + 2,
			       parse.ftie - 2, parse.ftie_len + 2,
			       parse.rsn - 2, parse.rsn_len + 2,
			       parse.ric, parse.ric_len,
			       parse.rsnxe ? parse.rsnxe - 2 : NULL,
			       parse.rsnxe ? parse.rsnxe_len + 2 : 0,
			       mic) < 0) {
			add_note(wt, MSG_INFO, "FT: Failed to calculate MIC");
			return;
		}

		if (os_memcmp_const(mic, fte_mic, mic_len) != 0) {
			add_note(wt, MSG_INFO, "FT: Invalid MIC in FTE");
			wpa_printf(MSG_DEBUG,
				   "FT: addr=" MACSTR " auth_addr=" MACSTR,
				   MAC2STR(sta->addr),
				   MAC2STR(bss->bssid));
			wpa_hexdump(MSG_MSGDUMP, "FT: Received MIC",
				    fte_mic, mic_len);
			wpa_hexdump(MSG_MSGDUMP, "FT: Calculated MIC",
				    mic, mic_len);
			wpa_hexdump(MSG_MSGDUMP, "FT: MDE",
				    parse.mdie - 2, parse.mdie_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: FTE",
				    parse.ftie - 2, parse.ftie_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: RSN",
				    parse.rsn - 2, parse.rsn_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: RSNXE",
				    parse.rsnxe ? parse.rsnxe - 2 : NULL,
				    parse.rsnxe ? parse.rsnxe_len + 2 : 0);
			return;
		}

		add_note(wt, MSG_INFO, "FT: Valid FTE MIC");
	}
}


static void process_gtk_subelem(struct wlantest *wt, struct wlantest_bss *bss,
				struct wlantest_sta *sta,
				const u8 *kek, size_t kek_len,
				const u8 *gtk_elem,
				size_t gtk_elem_len)
{
	u8 gtk[32];
	int keyidx;
	enum wpa_alg alg;
	size_t gtk_len, keylen;
	const u8 *rsc;

	if (!gtk_elem) {
		add_note(wt, MSG_INFO, "FT: No GTK included in FTE");
		return;
	}

	wpa_hexdump(MSG_DEBUG, "FT: Received GTK in Reassoc Resp",
		    gtk_elem, gtk_elem_len);

	if (gtk_elem_len < 11 + 24 || (gtk_elem_len - 11) % 8 ||
	    gtk_elem_len - 19 > sizeof(gtk)) {
		add_note(wt, MSG_INFO, "FT: Invalid GTK sub-elem length %zu",
			 gtk_elem_len);
		return;
	}
	gtk_len = gtk_elem_len - 19;
	if (aes_unwrap(kek, kek_len, gtk_len / 8, gtk_elem + 11, gtk)) {
		add_note(wt, MSG_INFO,
			 "FT: AES unwrap failed - could not decrypt GTK");
		return;
	}

	keylen = wpa_cipher_key_len(bss->group_cipher);
	alg = wpa_cipher_to_alg(bss->group_cipher);
	if (alg == WPA_ALG_NONE) {
		add_note(wt, MSG_INFO, "FT: Unsupported Group Cipher %d",
			 bss->group_cipher);
		return;
	}

	if (gtk_len < keylen) {
		add_note(wt, MSG_INFO, "FT: Too short GTK in FTE");
		return;
	}

	/* Key Info[2] | Key Length[1] | RSC[8] | Key[5..32]. */

	keyidx = WPA_GET_LE16(gtk_elem) & 0x03;

	if (gtk_elem[2] != keylen) {
		add_note(wt, MSG_INFO,
			 "FT: GTK length mismatch: received %u negotiated %zu",
			 gtk_elem[2], keylen);
		return;
	}

	add_note(wt, MSG_DEBUG, "GTK KeyID=%u", keyidx);
	wpa_hexdump(MSG_DEBUG, "FT: GTK from Reassoc Resp", gtk, keylen);
	if (bss->group_cipher == WPA_CIPHER_TKIP) {
		/* Swap Tx/Rx keys for Michael MIC */
		u8 tmp[8];

		os_memcpy(tmp, gtk + 16, 8);
		os_memcpy(gtk + 16, gtk + 24, 8);
		os_memcpy(gtk + 24, tmp, 8);
	}

	bss->gtk_len[keyidx] = gtk_len;
	sta->gtk_len = gtk_len;
	os_memcpy(bss->gtk[keyidx], gtk, gtk_len);
	os_memcpy(sta->gtk, gtk, gtk_len);
	rsc = gtk_elem + 2;
	bss->rsc[keyidx][0] = rsc[5];
	bss->rsc[keyidx][1] = rsc[4];
	bss->rsc[keyidx][2] = rsc[3];
	bss->rsc[keyidx][3] = rsc[2];
	bss->rsc[keyidx][4] = rsc[1];
	bss->rsc[keyidx][5] = rsc[0];
	bss->gtk_idx = keyidx;
	sta->gtk_idx = keyidx;
	wpa_hexdump(MSG_DEBUG, "RSC", bss->rsc[keyidx], 6);
}


static void process_igtk_subelem(struct wlantest *wt, struct wlantest_bss *bss,
				 struct wlantest_sta *sta,
				 const u8 *kek, size_t kek_len,
				 const u8 *igtk_elem, size_t igtk_elem_len)
{
	u8 igtk[WPA_IGTK_MAX_LEN];
	size_t igtk_len;
	u16 keyidx;
	const u8 *ipn;

	if (bss->mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC &&
	    bss->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_128 &&
	    bss->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_256 &&
	    bss->mgmt_group_cipher != WPA_CIPHER_BIP_CMAC_256)
		return;

	if (!igtk_elem) {
		add_note(wt, MSG_INFO, "FT: No IGTK included in FTE");
		return;
	}

	wpa_hexdump(MSG_DEBUG, "FT: Received IGTK in Reassoc Resp",
		    igtk_elem, igtk_elem_len);

	igtk_len = wpa_cipher_key_len(bss->mgmt_group_cipher);
	if (igtk_elem_len != 2 + 6 + 1 + igtk_len + 8) {
		add_note(wt, MSG_INFO, "FT: Invalid IGTK sub-elem length %zu",
			 igtk_elem_len);
		return;
	}
	if (igtk_elem[8] != igtk_len) {
		add_note(wt, MSG_INFO,
			 "FT: Invalid IGTK sub-elem Key Length %d",
			 igtk_elem[8]);
		return;
	}

	if (aes_unwrap(kek, kek_len, igtk_len / 8, igtk_elem + 9, igtk)) {
		add_note(wt, MSG_INFO,
			 "FT: AES unwrap failed - could not decrypt IGTK");
		return;
	}

	/* KeyID[2] | IPN[6] | Key Length[1] | Key[16+8] */

	keyidx = WPA_GET_LE16(igtk_elem);

	wpa_hexdump(MSG_DEBUG, "FT: IGTK from Reassoc Resp", igtk, igtk_len);

	if (keyidx < 4 || keyidx > 5) {
		add_note(wt, MSG_INFO, "Unexpected IGTK KeyID %u", keyidx);
		return;
	}

	add_note(wt, MSG_DEBUG, "IGTK KeyID %u", keyidx);
	wpa_hexdump(MSG_DEBUG, "IPN", igtk_elem + 2, 6);
	wpa_hexdump(MSG_DEBUG, "IGTK", igtk, igtk_len);
	os_memcpy(bss->igtk[keyidx], igtk, igtk_len);
	bss->igtk_len[keyidx] = igtk_len;
	ipn = igtk_elem + 2;
	bss->ipn[keyidx][0] = ipn[5];
	bss->ipn[keyidx][1] = ipn[4];
	bss->ipn[keyidx][2] = ipn[3];
	bss->ipn[keyidx][3] = ipn[2];
	bss->ipn[keyidx][4] = ipn[1];
	bss->ipn[keyidx][5] = ipn[0];
	bss->igtk_idx = keyidx;
}


static void process_bigtk_subelem(struct wlantest *wt, struct wlantest_bss *bss,
				  struct wlantest_sta *sta,
				  const u8 *kek, size_t kek_len,
				  const u8 *bigtk_elem, size_t bigtk_elem_len)
{
	u8 bigtk[WPA_BIGTK_MAX_LEN];
	size_t bigtk_len;
	u16 keyidx;
	const u8 *ipn;

	if (!bigtk_elem ||
	    (bss->mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC &&
	     bss->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_128 &&
	     bss->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_256 &&
	     bss->mgmt_group_cipher != WPA_CIPHER_BIP_CMAC_256))
	    return;

	wpa_hexdump_key(MSG_DEBUG, "FT: Received BIGTK in Reassoc Resp",
			bigtk_elem, bigtk_elem_len);

	bigtk_len = wpa_cipher_key_len(bss->mgmt_group_cipher);
	if (bigtk_elem_len != 2 + 6 + 1 + bigtk_len + 8) {
		add_note(wt, MSG_INFO,
			 "FT: Invalid BIGTK sub-elem length %zu",
			 bigtk_elem_len);
		return;
	}
	if (bigtk_elem[8] != bigtk_len) {
		add_note(wt, MSG_INFO,
			 "FT: Invalid BIGTK sub-elem Key Length %d",
			 bigtk_elem[8]);
		return;
	}

	if (aes_unwrap(kek, kek_len, bigtk_len / 8, bigtk_elem + 9, bigtk)) {
		add_note(wt, MSG_INFO,
			 "FT: AES unwrap failed - could not decrypt BIGTK");
		return;
	}

	/* KeyID[2] | IPN[6] | Key Length[1] | Key[16+8] */

	keyidx = WPA_GET_LE16(bigtk_elem);

	wpa_hexdump(MSG_DEBUG, "FT: BIGTK from Reassoc Resp", bigtk, bigtk_len);

	if (keyidx < 6 || keyidx > 7) {
		add_note(wt, MSG_INFO, "Unexpected BIGTK KeyID %u", keyidx);
		return;
	}

	add_note(wt, MSG_DEBUG, "BIGTK KeyID %u", keyidx);
	wpa_hexdump(MSG_DEBUG, "BIPN", bigtk_elem + 2, 6);
	wpa_hexdump(MSG_DEBUG, "BIGTK", bigtk, bigtk_len);
	os_memcpy(bss->igtk[keyidx], bigtk, bigtk_len);
	bss->igtk_len[keyidx] = bigtk_len;
	ipn = bigtk_elem + 2;
	bss->ipn[keyidx][0] = ipn[5];
	bss->ipn[keyidx][1] = ipn[4];
	bss->ipn[keyidx][2] = ipn[3];
	bss->ipn[keyidx][3] = ipn[2];
	bss->ipn[keyidx][4] = ipn[1];
	bss->ipn[keyidx][5] = ipn[0];
	bss->bigtk_idx = keyidx;
}


static void rx_mgmt_reassoc_resp(struct wlantest *wt, const u8 *data,
				 size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u16 capab, status, aid;
	const u8 *ies;
	size_t ies_len;
	struct ieee802_11_elems elems;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	sta = sta_get(bss, mgmt->da);
	if (sta == NULL)
		return;

	if (len < 24 + 6) {
		add_note(wt, MSG_INFO, "Too short Reassociation Response frame "
			 "from " MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	ies = mgmt->u.reassoc_resp.variable;
	ies_len = len - (mgmt->u.reassoc_resp.variable - data);

	capab = le_to_host16(mgmt->u.reassoc_resp.capab_info);
	status = le_to_host16(mgmt->u.reassoc_resp.status_code);
	aid = le_to_host16(mgmt->u.reassoc_resp.aid);

	wpa_printf(MSG_DEBUG, "REASSOCRESP " MACSTR " -> " MACSTR
		   " (capab=0x%x status=%u aid=%u)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), capab, status,
		   aid & 0x3fff);

	if (sta->auth_alg == WLAN_AUTH_FILS_SK) {
		const u8 *session, *frame_ad, *frame_ad_end, *encr_end;

		session = get_fils_session(ies, ies_len);
		if (session) {
			frame_ad = (const u8 *)
				&mgmt->u.reassoc_resp.capab_info;
			frame_ad_end = session + 2 + session[1];
			encr_end = data + len;
			decrypt_fils_assoc_resp(wt, bss, sta, data, frame_ad,
						frame_ad_end, encr_end);
			ies_len = session - ies;
		}
	}

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) == ParseFailed) {
		add_note(wt, MSG_INFO,
			 "Failed to parse IEs in ReassocResp from " MACSTR,
			 MAC2STR(mgmt->sa));
	}

	if (status == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY) {
		if (!elems.timeout_int ||
		    elems.timeout_int[0] != WLAN_TIMEOUT_ASSOC_COMEBACK) {
			add_note(wt, MSG_INFO, "No valid Timeout Interval IE "
				 "with Assoc Comeback time in ReassocResp "
				 "(status=30) from " MACSTR,
				 MAC2STR(mgmt->sa));
		} else {
			sta->counters[
				WLANTEST_STA_COUNTER_REASSOCRESP_COMEBACK]++;
		}
	}

	if (status)
		return;

	if ((aid & 0xc000) != 0xc000) {
		add_note(wt, MSG_DEBUG, "Two MSBs of the AID were not set to 1 "
			 "in Reassociation Response from " MACSTR,
			 MAC2STR(mgmt->sa));
	}
	sta->aid = aid & 0xc000;

	if (sta->state < STATE2 && !sta->ft_over_ds) {
		add_note(wt, MSG_DEBUG,
			 "STA " MACSTR " was not in State 2 when "
			 "getting associated", MAC2STR(sta->addr));
	}

	if (sta->state < STATE3) {
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 3 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE3;
	}

	if (elems.ftie) {
		struct wpa_ft_ies parse;
		int use_sha384;
		struct rsn_mdie *mde;
		const u8 *anonce, *snonce, *fte_mic;
		u8 fte_elem_count;
		unsigned int count;
		u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN];
		size_t mic_len = 16;
		const u8 *kck, *kek;
		size_t kck_len, kek_len;

		use_sha384 = wpa_key_mgmt_sha384(sta->key_mgmt);

		if (wpa_ft_parse_ies(ies, ies_len, &parse, use_sha384) < 0) {
			add_note(wt, MSG_INFO, "FT: Failed to parse FT IEs");
			return;
		}

		if (!parse.rsn) {
			add_note(wt, MSG_INFO, "FT: No RSNE in Reassoc Resp");
			return;
		}

		if (!parse.rsn_pmkid) {
			add_note(wt, MSG_INFO, "FT: No PMKID in RSNE");
			return;
		}

		if (os_memcmp_const(parse.rsn_pmkid, sta->pmk_r1_name,
				    WPA_PMK_NAME_LEN) != 0) {
			add_note(wt, MSG_INFO,
				 "FT: PMKID in Reassoc Resp did not match PMKR1Name");
			wpa_hexdump(MSG_DEBUG,
				    "FT: Received RSNE[PMKR1Name]",
				    parse.rsn_pmkid, WPA_PMK_NAME_LEN);
			wpa_hexdump(MSG_DEBUG,
				    "FT: Previously derived PMKR1Name",
				    sta->pmk_r1_name, WPA_PMK_NAME_LEN);
			return;
		}

		mde = (struct rsn_mdie *) parse.mdie;
		if (!mde || parse.mdie_len < sizeof(*mde) ||
		    os_memcmp(mde->mobility_domain, bss->mdid,
			      MOBILITY_DOMAIN_ID_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: Invalid MDE");
		}

		if (use_sha384) {
			struct rsn_ftie_sha384 *fte;

			fte = (struct rsn_ftie_sha384 *) parse.ftie;
			if (!fte || parse.ftie_len < sizeof(*fte)) {
				add_note(wt, MSG_INFO, "FT: Invalid FTE");
				return;
			}

			anonce = fte->anonce;
			snonce = fte->snonce;
			fte_elem_count = fte->mic_control[1];
			fte_mic = fte->mic;
		} else {
			struct rsn_ftie *fte;

			fte = (struct rsn_ftie *) parse.ftie;
			if (!fte || parse.ftie_len < sizeof(*fte)) {
				add_note(wt, MSG_INFO, "FT: Invalid FTIE");
				return;
			}

			anonce = fte->anonce;
			snonce = fte->snonce;
			fte_elem_count = fte->mic_control[1];
			fte_mic = fte->mic;
		}

		if (os_memcmp(snonce, sta->snonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: SNonce mismatch in FTIE");
			wpa_hexdump(MSG_DEBUG, "FT: Received SNonce",
				    snonce, WPA_NONCE_LEN);
			wpa_hexdump(MSG_DEBUG, "FT: Expected SNonce",
				    sta->snonce, WPA_NONCE_LEN);
			return;
		}

		if (os_memcmp(anonce, sta->anonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "FT: ANonce mismatch in FTIE");
			wpa_hexdump(MSG_DEBUG, "FT: Received ANonce",
				    anonce, WPA_NONCE_LEN);
			wpa_hexdump(MSG_DEBUG, "FT: Expected ANonce",
				    sta->anonce, WPA_NONCE_LEN);
			return;
		}

		if (!parse.r0kh_id) {
			add_note(wt, MSG_INFO, "FT: No R0KH-ID subelem in FTE");
			return;
		}

		if (parse.r0kh_id_len != bss->r0kh_id_len ||
		    os_memcmp_const(parse.r0kh_id, bss->r0kh_id,
				    parse.r0kh_id_len) != 0) {
			add_note(wt, MSG_INFO,
				 "FT: R0KH-ID in FTE did not match the current R0KH-ID");
			wpa_hexdump(MSG_DEBUG, "FT: R0KH-ID in FTIE",
				    parse.r0kh_id, parse.r0kh_id_len);
			wpa_hexdump(MSG_DEBUG, "FT: The current R0KH-ID",
				    bss->r0kh_id, bss->r0kh_id_len);
			os_memcpy(bss->r0kh_id, parse.r0kh_id,
				  parse.r0kh_id_len);
			bss->r0kh_id_len = parse.r0kh_id_len;
		}

		if (!parse.r1kh_id) {
			add_note(wt, MSG_INFO, "FT: No R1KH-ID subelem in FTE");
			return;
		}

		if (os_memcmp_const(parse.r1kh_id, bss->r1kh_id,
				    FT_R1KH_ID_LEN) != 0) {
			add_note(wt, MSG_INFO,
				 "FT: Unknown R1KH-ID used in ReassocResp");
			os_memcpy(bss->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);
		}

		count = 3;
		if (parse.ric)
			count += ieee802_11_ie_count(parse.ric, parse.ric_len);
		if (parse.rsnxe)
			count++;
		if (fte_elem_count != count) {
			add_note(wt, MSG_INFO,
				 "FT: Unexpected IE count in MIC Control: received %u expected %u",
				 fte_elem_count, count);
			return;
		}

		if (wpa_key_mgmt_fils(sta->key_mgmt)) {
			kck = sta->ptk.kck2;
			kck_len = sta->ptk.kck2_len;
			kek = sta->ptk.kek2;
			kek_len = sta->ptk.kek2_len;
		} else {
			kck = sta->ptk.kck;
			kck_len = sta->ptk.kck_len;
			kek = sta->ptk.kek;
			kek_len = sta->ptk.kek_len;
		}
		if (wpa_ft_mic(kck, kck_len, sta->addr, bss->bssid, 6,
			       parse.mdie - 2, parse.mdie_len + 2,
			       parse.ftie - 2, parse.ftie_len + 2,
			       parse.rsn - 2, parse.rsn_len + 2,
			       parse.ric, parse.ric_len,
			       parse.rsnxe ? parse.rsnxe - 2 : NULL,
			       parse.rsnxe ? parse.rsnxe_len + 2 : 0,
			       mic) < 0) {
			add_note(wt, MSG_INFO, "FT: Failed to calculate MIC");
			return;
		}

		if (os_memcmp_const(mic, fte_mic, mic_len) != 0) {
			add_note(wt, MSG_INFO, "FT: Invalid MIC in FTE");
			wpa_printf(MSG_DEBUG,
				   "FT: addr=" MACSTR " auth_addr=" MACSTR,
				   MAC2STR(sta->addr),
				   MAC2STR(bss->bssid));
			wpa_hexdump(MSG_MSGDUMP, "FT: Received MIC",
				    fte_mic, mic_len);
			wpa_hexdump(MSG_MSGDUMP, "FT: Calculated MIC",
				    mic, mic_len);
			wpa_hexdump(MSG_MSGDUMP, "FT: MDE",
				    parse.mdie - 2, parse.mdie_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: FTE",
				    parse.ftie - 2, parse.ftie_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: RSN",
				    parse.rsn - 2, parse.rsn_len + 2);
			wpa_hexdump(MSG_MSGDUMP, "FT: RSNXE",
				    parse.rsnxe ? parse.rsnxe - 2 : NULL,
				    parse.rsnxe ? parse.rsnxe_len + 2 : 0);
			return;
		}

		add_note(wt, MSG_INFO, "FT: Valid FTE MIC");

		if (wpa_compare_rsn_ie(wpa_key_mgmt_ft(sta->key_mgmt),
				       bss->rsnie, 2 + bss->rsnie[1],
				       parse.rsn - 2, parse.rsn_len + 2)) {
			add_note(wt, MSG_INFO,
				 "FT: RSNE mismatch between Beacon/ProbeResp and FT protocol Reassociation Response frame");
			wpa_hexdump(MSG_INFO, "RSNE in Beacon/ProbeResp",
				    &bss->rsnie[2], bss->rsnie[1]);
			wpa_hexdump(MSG_INFO,
				    "RSNE in FT protocol Reassociation Response frame",
				    parse.rsn ? parse.rsn - 2 : NULL,
				    parse.rsn ? parse.rsn_len + 2 : 0);
		}

		process_gtk_subelem(wt, bss, sta, kek, kek_len,
				    parse.gtk, parse.gtk_len);
		process_igtk_subelem(wt, bss, sta, kek, kek_len,
				     parse.igtk, parse.igtk_len);
		process_bigtk_subelem(wt, bss, sta, kek, kek_len,
				      parse.bigtk, parse.bigtk_len);
	}
}


static void disassoc_all_stas(struct wlantest *wt, struct wlantest_bss *bss)
{
	struct wlantest_sta *sta;
	dl_list_for_each(sta, &bss->sta, struct wlantest_sta, list) {
		if (sta->state <= STATE2)
			continue;
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 2 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE2;
	}
}


static void rx_mgmt_disassoc(struct wlantest *wt, const u8 *data, size_t len,
			     int valid)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u16 fc, reason;

	mgmt = (const struct ieee80211_mgmt *) data;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);

	if (len < 24 + 2) {
		add_note(wt, MSG_INFO, "Too short Disassociation frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	reason = le_to_host16(mgmt->u.disassoc.reason_code);
	wpa_printf(MSG_DEBUG, "DISASSOC " MACSTR " -> " MACSTR
		   " (reason=%u) (valid=%d)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da),
		   reason, valid);
	wpa_hexdump(MSG_MSGDUMP, "DISASSOC payload", data + 24, len - 24);

	if (sta == NULL) {
		if (valid && mgmt->da[0] == 0xff)
			disassoc_all_stas(wt, bss);
		return;
	}

	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0) {
		sta->counters[valid ? WLANTEST_STA_COUNTER_VALID_DISASSOC_RX :
			      WLANTEST_STA_COUNTER_INVALID_DISASSOC_RX]++;
		if (sta->pwrmgt && !sta->pspoll)
			sta->counters[
				WLANTEST_STA_COUNTER_DISASSOC_RX_ASLEEP]++;
		else
			sta->counters[
				WLANTEST_STA_COUNTER_DISASSOC_RX_AWAKE]++;

		fc = le_to_host16(mgmt->frame_control);
		if (!(fc & WLAN_FC_ISWEP) && reason == 6)
			sta->counters[WLANTEST_STA_COUNTER_DISASSOC_RX_RC6]++;
		else if (!(fc & WLAN_FC_ISWEP) && reason == 7)
			sta->counters[WLANTEST_STA_COUNTER_DISASSOC_RX_RC7]++;
	} else
		sta->counters[valid ? WLANTEST_STA_COUNTER_VALID_DISASSOC_TX :
			      WLANTEST_STA_COUNTER_INVALID_DISASSOC_TX]++;

	if (!valid) {
		add_note(wt, MSG_INFO, "Do not change STA " MACSTR " State "
			 "since Disassociation frame was not protected "
			 "correctly", MAC2STR(sta->addr));
		return;
	}

	if (sta->state < STATE2) {
		add_note(wt, MSG_DEBUG,
			 "STA " MACSTR " was not in State 2 or 3 "
			 "when getting disassociated", MAC2STR(sta->addr));
	}

	if (sta->state > STATE2) {
		add_note(wt, MSG_DEBUG, "STA " MACSTR
			 " moved to State 2 with " MACSTR,
			 MAC2STR(sta->addr), MAC2STR(bss->bssid));
		sta->state = STATE2;
	}
	tdls_link_down(wt, bss, sta);
}


static void rx_mgmt_action_ft_request(struct wlantest *wt,
				      const struct ieee80211_mgmt *mgmt,
				      size_t len)
{
	const u8 *ies;
	size_t ies_len;
	struct wpa_ft_ies parse;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	if (len < 24 + 2 + 2 * ETH_ALEN) {
		add_note(wt, MSG_INFO, "Too short FT Request frame");
		return;
	}

	wpa_printf(MSG_DEBUG, "FT Request: STA Address: " MACSTR
		   " Target AP Address: " MACSTR,
		   MAC2STR(mgmt->u.action.u.ft_action_req.sta_addr),
		   MAC2STR(mgmt->u.action.u.ft_action_req.target_ap_addr));
	ies = mgmt->u.action.u.ft_action_req.variable;
	ies_len = len - (24 + 2 + 2 * ETH_ALEN);
	wpa_hexdump(MSG_DEBUG, "FT Request frame body", ies, ies_len);

	if (wpa_ft_parse_ies(ies, ies_len, &parse, -1)) {
		add_note(wt, MSG_INFO, "Could not parse FT Request frame body");
		return;
	}

	bss = bss_get(wt, mgmt->u.action.u.ft_action_resp.target_ap_addr);
	if (!bss) {
		add_note(wt, MSG_INFO, "No BSS entry for Target AP");
		return;
	}

	sta = sta_get(bss, mgmt->sa);
	if (!sta)
		return;

	sta->ft_over_ds = true;
	sta->key_mgmt = parse.key_mgmt;
	sta->pairwise_cipher = parse.pairwise_cipher;
}


static void rx_mgmt_action_ft_response(struct wlantest *wt,
				       struct wlantest_sta *sta,
				       const struct ieee80211_mgmt *mgmt,
				       size_t len)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *new_sta;
	const u8 *ies;
	size_t ies_len;
	struct wpa_ft_ies parse;
	struct wpa_ptk ptk;
	u8 ptk_name[WPA_PMK_NAME_LEN];

	if (len < 24 + 2 + 2 * ETH_ALEN + 2) {
		add_note(wt, MSG_INFO, "Too short FT Response frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "FT Response: STA Address: " MACSTR
		   " Target AP Address: " MACSTR " Status Code: %u",
		   MAC2STR(mgmt->u.action.u.ft_action_resp.sta_addr),
		   MAC2STR(mgmt->u.action.u.ft_action_resp.target_ap_addr),
		   le_to_host16(mgmt->u.action.u.ft_action_resp.status_code));
	ies = mgmt->u.action.u.ft_action_req.variable;
	ies_len = len - (24 + 2 + 2 * ETH_ALEN);
	wpa_hexdump(MSG_DEBUG, "FT Response frame body", ies, ies_len);

	if (wpa_ft_parse_ies(ies, ies_len, &parse, -1)) {
		add_note(wt, MSG_INFO,
			 "Could not parse FT Response frame body");
		return;
	}

	bss = bss_get(wt, mgmt->u.action.u.ft_action_resp.target_ap_addr);
	if (!bss) {
		add_note(wt, MSG_INFO, "No BSS entry for Target AP");
		return;
	}

	if (parse.r1kh_id)
		os_memcpy(bss->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);

	if (wpa_derive_pmk_r1(sta->pmk_r0, sta->pmk_r0_len, sta->pmk_r0_name,
			      bss->r1kh_id, sta->addr, sta->pmk_r1,
			      sta->pmk_r1_name) < 0)
		return;
	sta->pmk_r1_len = sta->pmk_r0_len;

	new_sta = sta_get(bss, sta->addr);
	if (!new_sta)
		return;
	os_memcpy(new_sta->pmk_r0, sta->pmk_r0, sta->pmk_r0_len);
	new_sta->pmk_r0_len = sta->pmk_r0_len;
	os_memcpy(new_sta->pmk_r0_name, sta->pmk_r0_name,
		  sizeof(sta->pmk_r0_name));
	os_memcpy(new_sta->pmk_r1, sta->pmk_r1, sta->pmk_r1_len);
	new_sta->pmk_r1_len = sta->pmk_r1_len;
	os_memcpy(new_sta->pmk_r1_name, sta->pmk_r1_name,
		  sizeof(sta->pmk_r1_name));
	if (!parse.fte_anonce || !parse.fte_snonce ||
	    wpa_pmk_r1_to_ptk(sta->pmk_r1, sta->pmk_r1_len, parse.fte_snonce,
			      parse.fte_anonce, new_sta->addr, bss->bssid,
			      sta->pmk_r1_name, &ptk, ptk_name,
			      new_sta->key_mgmt, new_sta->pairwise_cipher,
			      0) < 0)
		return;

	add_note(wt, MSG_DEBUG, "Derived new PTK");
	os_memcpy(&new_sta->ptk, &ptk, sizeof(ptk));
	new_sta->ptk_set = 1;
	os_memset(new_sta->rsc_tods, 0, sizeof(new_sta->rsc_tods));
	os_memset(new_sta->rsc_fromds, 0, sizeof(new_sta->rsc_fromds));
	os_memcpy(new_sta->snonce, parse.fte_snonce, WPA_NONCE_LEN);
	os_memcpy(new_sta->anonce, parse.fte_anonce, WPA_NONCE_LEN);
}


static void rx_mgmt_action_ft(struct wlantest *wt, struct wlantest_sta *sta,
			      const struct ieee80211_mgmt *mgmt,
			      size_t len, int valid)
{
	if (len < 24 + 2) {
		add_note(wt, MSG_INFO, "Too short FT Action frame from " MACSTR,
			 MAC2STR(mgmt->sa));
		return;
	}

	switch (mgmt->u.action.u.ft_action_req.action) {
	case 1:
		rx_mgmt_action_ft_request(wt, mgmt, len);
		break;
	case 2:
		rx_mgmt_action_ft_response(wt, sta, mgmt, len);
		break;
	default:
		add_note(wt, MSG_INFO, "Unsupported FT action value %u from "
			 MACSTR, mgmt->u.action.u.ft_action_req.action,
			 MAC2STR(mgmt->sa));
	}
}


static void rx_mgmt_action_sa_query_req(struct wlantest *wt,
					struct wlantest_sta *sta,
					const struct ieee80211_mgmt *mgmt,
					size_t len, int valid)
{
	const u8 *rx_id;
	u8 *id;

	rx_id = (const u8 *) mgmt->u.action.u.sa_query_req.trans_id;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		id = sta->ap_sa_query_tr;
	else
		id = sta->sta_sa_query_tr;
	add_note(wt, MSG_INFO, "SA Query Request " MACSTR " -> " MACSTR
		 " (trans_id=%02x%02x)%s",
		 MAC2STR(mgmt->sa), MAC2STR(mgmt->da), rx_id[0], rx_id[1],
		 valid ? "" : " (invalid protection)");
	os_memcpy(id, mgmt->u.action.u.sa_query_req.trans_id, 2);
	if (os_memcmp(mgmt->sa, sta->addr, ETH_ALEN) == 0)
		sta->counters[valid ?
			      WLANTEST_STA_COUNTER_VALID_SAQUERYREQ_TX :
			      WLANTEST_STA_COUNTER_INVALID_SAQUERYREQ_TX]++;
	else
		sta->counters[valid ?
			      WLANTEST_STA_COUNTER_VALID_SAQUERYREQ_RX :
			      WLANTEST_STA_COUNTER_INVALID_SAQUERYREQ_RX]++;
}


static void rx_mgmt_action_sa_query_resp(struct wlantest *wt,
					 struct wlantest_sta *sta,
					 const struct ieee80211_mgmt *mgmt,
					 size_t len, int valid)
{
	const u8 *rx_id;
	u8 *id;
	int match;

	rx_id = (const u8 *) mgmt->u.action.u.sa_query_resp.trans_id;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		id = sta->sta_sa_query_tr;
	else
		id = sta->ap_sa_query_tr;
	match = os_memcmp(rx_id, id, 2) == 0;
	add_note(wt, MSG_INFO, "SA Query Response " MACSTR " -> " MACSTR
		 " (trans_id=%02x%02x; %s)%s",
		 MAC2STR(mgmt->sa), MAC2STR(mgmt->da), rx_id[0], rx_id[1],
		 match ? "match" : "mismatch",
		 valid ? "" : " (invalid protection)");
	if (os_memcmp(mgmt->sa, sta->addr, ETH_ALEN) == 0)
		sta->counters[(valid && match) ?
			      WLANTEST_STA_COUNTER_VALID_SAQUERYRESP_TX :
			      WLANTEST_STA_COUNTER_INVALID_SAQUERYRESP_TX]++;
	else
		sta->counters[(valid && match) ?
			      WLANTEST_STA_COUNTER_VALID_SAQUERYRESP_RX :
			      WLANTEST_STA_COUNTER_INVALID_SAQUERYRESP_RX]++;
}


static void rx_mgmt_action_sa_query(struct wlantest *wt,
				    struct wlantest_sta *sta,
				    const struct ieee80211_mgmt *mgmt,
				    size_t len, int valid)
{
	if (len < 24 + 2 + WLAN_SA_QUERY_TR_ID_LEN) {
		add_note(wt, MSG_INFO, "Too short SA Query frame from " MACSTR,
			 MAC2STR(mgmt->sa));
		return;
	}

	if (len > 24 + 2 + WLAN_SA_QUERY_TR_ID_LEN) {
		size_t elen = len - (24 + 2 + WLAN_SA_QUERY_TR_ID_LEN);
		add_note(wt, MSG_INFO, "Unexpected %u octets of extra data at "
			 "the end of SA Query frame from " MACSTR,
			 (unsigned) elen, MAC2STR(mgmt->sa));
		wpa_hexdump(MSG_INFO, "SA Query extra data",
			    ((const u8 *) mgmt) + len - elen, elen);
	}

	switch (mgmt->u.action.u.sa_query_req.action) {
	case WLAN_SA_QUERY_REQUEST:
		rx_mgmt_action_sa_query_req(wt, sta, mgmt, len, valid);
		break;
	case WLAN_SA_QUERY_RESPONSE:
		rx_mgmt_action_sa_query_resp(wt, sta, mgmt, len, valid);
		break;
	default:
		add_note(wt, MSG_INFO, "Unexpected SA Query action value %u "
			 "from " MACSTR,
			 mgmt->u.action.u.sa_query_req.action,
			 MAC2STR(mgmt->sa));
	}
}


static void
rx_mgmt_location_measurement_report(struct wlantest *wt,
				    const struct ieee80211_mgmt *mgmt,
				    size_t len, bool no_ack)
{
	const u8 *pos = mgmt->u.action.u.public_action.variable;
	const u8 *end = ((const u8 *) mgmt) + len;

	if (end - pos < 1) {
		add_note(wt, MSG_INFO,
			 "Too short Location Measurement Report frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "Location Measurement Report " MACSTR " --> "
		   MACSTR " (dialog token %u)",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), *pos);
	pos++;

	if (!no_ack)
		add_note(wt, MSG_INFO,
			 "Protected Fine Timing Measurement Report incorrectly as an Action frame from "
			 MACSTR, MAC2STR(mgmt->sa));

	wpa_hexdump(MSG_MSGDUMP, "Location Measurement Report contents",
		    pos, end - pos);
}


static void rx_mgmt_action_no_bss_public(struct wlantest *wt,
					 const struct ieee80211_mgmt *mgmt,
					 size_t len, bool no_ack)
{
	switch (mgmt->u.action.u.public_action.action) {
	case WLAN_PA_LOCATION_MEASUREMENT_REPORT:
		rx_mgmt_location_measurement_report(wt, mgmt, len, no_ack);
		break;
	}
}


static void rx_mgmt_prot_ftm_request(struct wlantest *wt,
				     const struct ieee80211_mgmt *mgmt,
				     size_t len, bool no_ack)
{
	wpa_printf(MSG_DEBUG, "Protected Fine Timing Measurement Request "
		   MACSTR " --> " MACSTR,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	if (no_ack)
		add_note(wt, MSG_INFO,
			 "Protected Fine Timing Measurement Request incorrectly as an Action No Ack frame from "
			 MACSTR, MAC2STR(mgmt->sa));
}


static void rx_mgmt_prot_ftm(struct wlantest *wt,
			     const struct ieee80211_mgmt *mgmt,
			     size_t len, bool no_ack)
{
	wpa_printf(MSG_DEBUG, "Protected Fine Timing Measurement "
		   MACSTR " --> " MACSTR,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	if (no_ack)
		add_note(wt, MSG_INFO,
			 "Protected Fine Timing Measurement incorrectly as an Action No Ack frame from "
			 MACSTR, MAC2STR(mgmt->sa));
}


static void rx_mgmt_prot_ftm_report(struct wlantest *wt,
				     const struct ieee80211_mgmt *mgmt,
				     size_t len, bool no_ack)
{
	wpa_printf(MSG_DEBUG, "Protected Fine Timing Measurement Report "
		   MACSTR " --> " MACSTR,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	if (!no_ack)
		add_note(wt, MSG_INFO,
			 "Protected Fine Timing Measurement Report incorrectly as an Action frame from "
			 MACSTR, MAC2STR(mgmt->sa));
}


static void
rx_mgmt_action_no_bss_protected_ftm(struct wlantest *wt,
				    const struct ieee80211_mgmt *mgmt,
				    size_t len, bool no_ack)
{
	switch (mgmt->u.action.u.public_action.action) {
	case WLAN_PROT_FTM_REQUEST:
		rx_mgmt_prot_ftm_request(wt, mgmt, len, no_ack);
		break;
	case WLAN_PROT_FTM:
		rx_mgmt_prot_ftm(wt, mgmt, len, no_ack);
		break;
	case WLAN_PROT_FTM_REPORT:
		rx_mgmt_prot_ftm_report(wt, mgmt, len, no_ack);
		break;
	}
}


static void rx_mgmt_action_no_bss(struct wlantest *wt,
				  const struct ieee80211_mgmt *mgmt, size_t len,
				  bool no_ack)
{
	switch (mgmt->u.action.category) {
	case WLAN_ACTION_PUBLIC:
		rx_mgmt_action_no_bss_public(wt, mgmt, len, no_ack);
		break;
	case WLAN_ACTION_PROTECTED_FTM:
		rx_mgmt_action_no_bss_protected_ftm(wt, mgmt, len, no_ack);
		break;
	}
}


static void rx_mgmt_action(struct wlantest *wt, const u8 *data, size_t len,
			   int valid, bool no_ack)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	mgmt = (const struct ieee80211_mgmt *) data;
	if (mgmt->da[0] & 0x01) {
		add_note(wt, MSG_DEBUG, "Group addressed Action frame: DA="
			 MACSTR " SA=" MACSTR " BSSID=" MACSTR
			 " category=%u",
			 MAC2STR(mgmt->da), MAC2STR(mgmt->sa),
			 MAC2STR(mgmt->bssid), mgmt->u.action.category);
		return; /* Ignore group addressed Action frames for now */
	}

	if (len < 24 + 2) {
		add_note(wt, MSG_INFO, "Too short Action frame from " MACSTR,
			 MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "ACTION%s " MACSTR " -> " MACSTR
		   " BSSID=" MACSTR " (category=%u) (valid=%d)",
		   no_ack ? "-NO-ACK" : "",
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), MAC2STR(mgmt->bssid),
		   mgmt->u.action.category, valid);
	wpa_hexdump(MSG_MSGDUMP, "ACTION payload", data + 24, len - 24);

	if (is_broadcast_ether_addr(mgmt->bssid)) {
		rx_mgmt_action_no_bss(wt, mgmt, len, no_ack);
		return;
	}
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	if (mgmt->u.action.category != WLAN_ACTION_PUBLIC &&
	    sta->state < STATE3) {
		add_note(wt, MSG_INFO, "Action frame sent when STA is not in "
			 "State 3 (SA=" MACSTR " DATA=" MACSTR ")",
			 MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	}

	switch (mgmt->u.action.category) {
	case WLAN_ACTION_FT:
		rx_mgmt_action_ft(wt, sta, mgmt, len, valid);
		break;
	case WLAN_ACTION_SA_QUERY:
		rx_mgmt_action_sa_query(wt, sta, mgmt, len, valid);
		break;
	}
}


static int check_mmie_mic(unsigned int mgmt_group_cipher,
			  const u8 *igtk, size_t igtk_len,
			  const u8 *data, size_t len)
{
	u8 *buf;
	u8 mic[16];
	u16 fc;
	const struct ieee80211_hdr *hdr;
	int ret, mic_len;

	if (!mgmt_group_cipher || igtk_len < 16)
		return -1;
	mic_len = mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC ? 8 : 16;

	if (len < 24 || len - 24 < mic_len)
		return -1;

	buf = os_malloc(len + 20 - 24);
	if (buf == NULL)
		return -1;

	/* BIP AAD: FC(masked) A1 A2 A3 */
	hdr = (const struct ieee80211_hdr *) data;
	fc = le_to_host16(hdr->frame_control);
	fc &= ~(WLAN_FC_RETRY | WLAN_FC_PWRMGT | WLAN_FC_MOREDATA);
	WPA_PUT_LE16(buf, fc);
	os_memcpy(buf + 2, hdr->addr1, 3 * ETH_ALEN);

	/* Frame body with MMIE MIC masked to zero */
	os_memcpy(buf + 20, data + 24, len - 24 - mic_len);
	os_memset(buf + 20 + len - 24 - mic_len, 0, mic_len);

	if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON) {
		/* Timestamp field masked to zero */
		os_memset(buf + 20, 0, 8);
	}

	wpa_hexdump(MSG_MSGDUMP, "BIP: AAD|Body(masked)", buf, len + 20 - 24);
	/* MIC = L(AES-128-CMAC(AAD || Frame Body(masked)), 0, 64) */
	if (mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC) {
		ret = omac1_aes_128(igtk, buf, len + 20 - 24, mic);
	} else if (mgmt_group_cipher == WPA_CIPHER_BIP_CMAC_256) {
		ret = omac1_aes_256(igtk, buf, len + 20 - 24, mic);
	} else if (mgmt_group_cipher == WPA_CIPHER_BIP_GMAC_128 ||
		 mgmt_group_cipher == WPA_CIPHER_BIP_GMAC_256) {
		u8 nonce[12], *npos;
		const u8 *ipn;

		ipn = data + len - mic_len - 6;

		/* Nonce: A2 | IPN */
		os_memcpy(nonce, hdr->addr2, ETH_ALEN);
		npos = nonce + ETH_ALEN;
		*npos++ = ipn[5];
		*npos++ = ipn[4];
		*npos++ = ipn[3];
		*npos++ = ipn[2];
		*npos++ = ipn[1];
		*npos++ = ipn[0];

		ret = aes_gmac(igtk, igtk_len, nonce, sizeof(nonce),
			       buf, len + 20 - 24, mic);
	} else {
		ret = -1;
	}
	if (ret < 0) {
		os_free(buf);
		return -1;
	}

	os_free(buf);

	if (os_memcmp(data + len - mic_len, mic, mic_len) != 0)
		return -1;

	return 0;
}


static int check_bip(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	const u8 *mmie;
	u16 keyid;
	struct wlantest_bss *bss;
	size_t mic_len;

	mgmt = (const struct ieee80211_mgmt *) data;
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (stype == WLAN_FC_STYPE_ACTION ||
	    stype == WLAN_FC_STYPE_ACTION_NO_ACK) {
		if (len < 24 + 1)
			return 0;
		if (mgmt->u.action.category == WLAN_ACTION_PUBLIC)
			return 0; /* Not a robust management frame */
	}

	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return 0; /* No key known yet */

	mic_len = bss->mgmt_group_cipher == WPA_CIPHER_AES_128_CMAC ? 8 : 16;

	if (len < 24 + 10 + mic_len ||
	    data[len - (10 + mic_len)] != WLAN_EID_MMIE ||
	    data[len - (10 + mic_len - 1)] != 8 + mic_len) {
		/* No MMIE */
		if (bss->rsn_capab & WPA_CAPABILITY_MFPC) {
			add_note(wt, MSG_INFO, "Robust group-addressed "
				 "management frame sent without BIP by "
				 MACSTR, MAC2STR(mgmt->sa));
			bss->counters[WLANTEST_BSS_COUNTER_MISSING_BIP_MMIE]++;
			return -1;
		}
		return 0;
	}

	mmie = data + len - (8 + mic_len);
	keyid = WPA_GET_LE16(mmie);
	if (keyid & 0xf000) {
		add_note(wt, MSG_INFO, "MMIE KeyID reserved bits not zero "
			 "(%04x) from " MACSTR, keyid, MAC2STR(mgmt->sa));
		keyid &= 0x0fff;
	}
	if (keyid < 4 || keyid > 5) {
		add_note(wt, MSG_INFO, "Unexpected MMIE KeyID %u from " MACSTR,
			 keyid, MAC2STR(mgmt->sa));
		bss->counters[WLANTEST_BSS_COUNTER_INVALID_BIP_MMIE]++;
		return 0;
	}
	wpa_printf(MSG_DEBUG, "MMIE KeyID %u", keyid);
	wpa_hexdump(MSG_MSGDUMP, "MMIE IPN", mmie + 2, 6);
	wpa_hexdump(MSG_MSGDUMP, "MMIE MIC", mmie + 8, mic_len);

	if (!bss->igtk_len[keyid]) {
		add_note(wt, MSG_DEBUG, "No IGTK known to validate BIP frame");
		return 0;
	}

	if (os_memcmp(mmie + 2, bss->ipn[keyid], 6) <= 0) {
		add_note(wt, MSG_INFO, "BIP replay detected: SA=" MACSTR,
			 MAC2STR(mgmt->sa));
		wpa_hexdump(MSG_INFO, "RX IPN", mmie + 2, 6);
		wpa_hexdump(MSG_INFO, "Last RX IPN", bss->ipn[keyid], 6);
	}

	if (check_mmie_mic(bss->mgmt_group_cipher, bss->igtk[keyid],
			   bss->igtk_len[keyid], data, len) < 0) {
		add_note(wt, MSG_INFO, "Invalid MMIE MIC in a frame from "
			 MACSTR, MAC2STR(mgmt->sa));
		bss->counters[WLANTEST_BSS_COUNTER_INVALID_BIP_MMIE]++;
		return -1;
	}

	add_note(wt, MSG_DEBUG, "Valid MMIE MIC");
	os_memcpy(bss->ipn[keyid], mmie + 2, 6);
	bss->counters[WLANTEST_BSS_COUNTER_VALID_BIP_MMIE]++;

	if (stype == WLAN_FC_STYPE_DEAUTH)
		bss->counters[WLANTEST_BSS_COUNTER_BIP_DEAUTH]++;
	else if (stype == WLAN_FC_STYPE_DISASSOC)
		bss->counters[WLANTEST_BSS_COUNTER_BIP_DISASSOC]++;

	return 0;
}


static u8 * try_tk(struct wpa_ptk *ptk, const u8 *data, size_t len,
		   size_t *dlen)
{
	const struct ieee80211_hdr *hdr;
	u8 *decrypted, *frame;

	hdr = (const struct ieee80211_hdr *) data;
	decrypted = ccmp_decrypt(ptk->tk, hdr, data + 24, len - 24, dlen);
	if (!decrypted)
		return NULL;

	frame = os_malloc(24 + *dlen);
	if (frame) {
		os_memcpy(frame, data, 24);
		os_memcpy(frame + 24, decrypted, *dlen);
		*dlen += 24;
	}
	os_free(decrypted);
	return frame;
}


static u8 * mgmt_ccmp_decrypt_tk(struct wlantest *wt, const u8 *data,
				 size_t len, size_t *dlen)
{
	struct wlantest_ptk *ptk;
	u8 *decrypted;
	int prev_level = wpa_debug_level;
	int keyid;

	keyid = data[24 + 3] >> 6;

	wpa_debug_level = MSG_WARNING;
	dl_list_for_each(ptk, &wt->ptk, struct wlantest_ptk, list) {
		decrypted = try_tk(&ptk->ptk, data, len, dlen);
		if (decrypted) {
			wpa_debug_level = prev_level;
			add_note(wt, MSG_DEBUG,
				 "Found TK match from the list of all known TKs");
			write_decrypted_note(wt, decrypted, ptk->ptk.tk,
					     ptk->ptk.tk_len, keyid);
			return decrypted;
		}
	}
	wpa_debug_level = prev_level;

	return NULL;
}


static u8 * mgmt_ccmp_decrypt(struct wlantest *wt, const u8 *data, size_t len,
			      size_t *dlen)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	const struct ieee80211_hdr *hdr;
	int keyid;
	u8 *decrypted, *frame = NULL;
	u8 pn[6], *rsc;
	u16 fc;
	u8 mask;

	hdr = (const struct ieee80211_hdr *) data;
	fc = le_to_host16(hdr->frame_control);

	if (len < 24 + 4)
		return NULL;

	if (!(data[24 + 3] & 0x20)) {
		add_note(wt, MSG_INFO, "Expected CCMP frame from " MACSTR
			 " did not have ExtIV bit set to 1",
			 MAC2STR(hdr->addr2));
		return NULL;
	}

	mask = 0x1f;
	if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION ||
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION_NO_ACK)
		mask &= ~0x10; /* FTM */
	if (data[24 + 2] != 0 || (data[24 + 3] & mask) != 0) {
		add_note(wt, MSG_INFO, "CCMP mgmt frame from " MACSTR " used "
			 "non-zero reserved bit", MAC2STR(hdr->addr2));
	}

	keyid = data[24 + 3] >> 6;
	if (keyid != 0) {
		add_note(wt, MSG_INFO, "Unexpected non-zero KeyID %d in "
			 "individually addressed Management frame from "
			 MACSTR, keyid, MAC2STR(hdr->addr2));
	}

	bss = bss_get(wt, hdr->addr3);
	if (bss == NULL)
		return mgmt_ccmp_decrypt_tk(wt, data, len, dlen);
	if (os_memcmp(hdr->addr1, hdr->addr3, ETH_ALEN) == 0)
		sta = sta_get(bss, hdr->addr2);
	else
		sta = sta_get(bss, hdr->addr1);
	if (sta == NULL || !sta->ptk_set) {
		decrypted = mgmt_ccmp_decrypt_tk(wt, data, len, dlen);
		if (!decrypted)
			add_note(wt, MSG_MSGDUMP,
				 "No PTK known to decrypt the frame");
		return decrypted;
	}

	if (os_memcmp(hdr->addr1, hdr->addr3, ETH_ALEN) == 0)
		rsc = sta->rsc_tods[16];
	else
		rsc = sta->rsc_fromds[16];

	ccmp_get_pn(pn, data + 24);
	if (os_memcmp(pn, rsc, 6) <= 0) {
		u16 seq_ctrl = le_to_host16(hdr->seq_ctrl);
		add_note(wt, MSG_INFO, "CCMP/TKIP replay detected: A1=" MACSTR
			 " A2=" MACSTR " A3=" MACSTR " seq=%u frag=%u%s",
			 MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			 MAC2STR(hdr->addr3),
			 WLAN_GET_SEQ_SEQ(seq_ctrl),
			 WLAN_GET_SEQ_FRAG(seq_ctrl),
			 (le_to_host16(hdr->frame_control) & WLAN_FC_RETRY) ?
			 " Retry" : "");
		wpa_hexdump(MSG_INFO, "RX PN", pn, 6);
		wpa_hexdump(MSG_INFO, "RSC", rsc, 6);
	}

	decrypted = ccmp_decrypt(sta->ptk.tk, hdr, data + 24, len - 24, dlen);
	if (decrypted) {
		os_memcpy(rsc, pn, 6);
		frame = os_malloc(24 + *dlen);
		if (frame) {
			os_memcpy(frame, data, 24);
			os_memcpy(frame + 24, decrypted, *dlen);
			*dlen += 24;
		}
	} else {
		/* Assume the frame was corrupted and there was no FCS to check.
		 * Allow retry of this particular frame to be processed so that
		 * it could end up getting decrypted if it was received without
		 * corruption. */
		sta->allow_duplicate = 1;
	}

	os_free(decrypted);

	return frame;
}


static int check_mgmt_ccmp(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	u16 fc;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	mgmt = (const struct ieee80211_mgmt *) data;
	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION ||
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION_NO_ACK) {
		if (len > 24 &&
		    mgmt->u.action.category == WLAN_ACTION_PUBLIC)
			return 0; /* Not a robust management frame */
	}

	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return 0;
	if (os_memcmp(mgmt->da, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->sa);
	else
		sta = sta_get(bss, mgmt->da);
	if (sta == NULL)
		return 0;

	if ((bss->rsn_capab & WPA_CAPABILITY_MFPC) &&
	    (sta->rsn_capab & WPA_CAPABILITY_MFPC) &&
	    (sta->state == STATE3 ||
	     WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION ||
	     WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION_NO_ACK)) {
		add_note(wt, MSG_INFO, "Robust individually-addressed "
			 "management frame sent without CCMP by "
			 MACSTR, MAC2STR(mgmt->sa));
		return -1;
	}

	return 0;
}


void rx_mgmt(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_hdr *hdr;
	u16 fc, stype;
	int valid = 1;
	u8 *decrypted = NULL;
	size_t dlen;

	if (len < 24)
		return;

	hdr = (const struct ieee80211_hdr *) data;
	fc = le_to_host16(hdr->frame_control);
	wt->rx_mgmt++;
	stype = WLAN_FC_GET_STYPE(fc);

	if ((hdr->addr1[0] & 0x01) &&
	    (stype == WLAN_FC_STYPE_DEAUTH ||
	     stype == WLAN_FC_STYPE_DISASSOC ||
	     stype == WLAN_FC_STYPE_ACTION ||
	     stype == WLAN_FC_STYPE_ACTION_NO_ACK)) {
		if (check_bip(wt, data, len) < 0)
			valid = 0;
	}

	wpa_printf((stype == WLAN_FC_STYPE_BEACON ||
		    stype == WLAN_FC_STYPE_PROBE_RESP ||
		    stype == WLAN_FC_STYPE_PROBE_REQ) ?
		   MSG_EXCESSIVE : MSG_MSGDUMP,
		   "MGMT %s%s%s DA=" MACSTR " SA=" MACSTR " BSSID=" MACSTR,
		   mgmt_stype(stype),
		   fc & WLAN_FC_PWRMGT ? " PwrMgt" : "",
		   fc & WLAN_FC_ISWEP ? " Prot" : "",
		   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
		   MAC2STR(hdr->addr3));

	if ((fc & WLAN_FC_ISWEP) &&
	    !(hdr->addr1[0] & 0x01) &&
	    (stype == WLAN_FC_STYPE_DEAUTH ||
	     stype == WLAN_FC_STYPE_DISASSOC ||
	     stype == WLAN_FC_STYPE_ACTION ||
	     stype == WLAN_FC_STYPE_ACTION_NO_ACK)) {
		decrypted = mgmt_ccmp_decrypt(wt, data, len, &dlen);
		if (decrypted) {
			write_pcap_decrypted(wt, decrypted, dlen, NULL, 0);
			data = decrypted;
			len = dlen;
		} else
			valid = 0;
	}

	if (!(fc & WLAN_FC_ISWEP) &&
	    !(hdr->addr1[0] & 0x01) &&
	    (stype == WLAN_FC_STYPE_DEAUTH ||
	     stype == WLAN_FC_STYPE_DISASSOC ||
	     stype == WLAN_FC_STYPE_ACTION ||
	     stype == WLAN_FC_STYPE_ACTION_NO_ACK)) {
		if (check_mgmt_ccmp(wt, data, len) < 0)
			valid = 0;
	}

	switch (stype) {
	case WLAN_FC_STYPE_BEACON:
		rx_mgmt_beacon(wt, data, len);
		break;
	case WLAN_FC_STYPE_PROBE_RESP:
		rx_mgmt_probe_resp(wt, data, len);
		break;
	case WLAN_FC_STYPE_AUTH:
		rx_mgmt_auth(wt, data, len);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		rx_mgmt_deauth(wt, data, len, valid);
		break;
	case WLAN_FC_STYPE_ASSOC_REQ:
		rx_mgmt_assoc_req(wt, data, len);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		rx_mgmt_assoc_resp(wt, data, len);
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		rx_mgmt_reassoc_req(wt, data, len);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		rx_mgmt_reassoc_resp(wt, data, len);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		rx_mgmt_disassoc(wt, data, len, valid);
		break;
	case WLAN_FC_STYPE_ACTION:
		rx_mgmt_action(wt, data, len, valid, false);
		break;
	case WLAN_FC_STYPE_ACTION_NO_ACK:
		rx_mgmt_action(wt, data, len, valid, true);
		break;
	}

	os_free(decrypted);

	wt->last_mgmt_valid = valid;
}


static void rx_mgmt_deauth_ack(struct wlantest *wt,
			       const struct ieee80211_hdr *hdr)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	mgmt = (const struct ieee80211_mgmt *) hdr;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	add_note(wt, MSG_DEBUG, "DEAUTH from " MACSTR " acknowledged by "
		 MACSTR, MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0) {
		int c;
		c = wt->last_mgmt_valid ?
			WLANTEST_STA_COUNTER_VALID_DEAUTH_RX_ACK :
			WLANTEST_STA_COUNTER_INVALID_DEAUTH_RX_ACK;
		sta->counters[c]++;
	}
}


static void rx_mgmt_disassoc_ack(struct wlantest *wt,
				 const struct ieee80211_hdr *hdr)
{
	const struct ieee80211_mgmt *mgmt;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	mgmt = (const struct ieee80211_mgmt *) hdr;
	bss = bss_get(wt, mgmt->bssid);
	if (bss == NULL)
		return;
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0)
		sta = sta_get(bss, mgmt->da);
	else
		sta = sta_get(bss, mgmt->sa);
	if (sta == NULL)
		return;

	add_note(wt, MSG_DEBUG, "DISASSOC from " MACSTR " acknowledged by "
		 MACSTR, MAC2STR(mgmt->sa), MAC2STR(mgmt->da));
	if (os_memcmp(mgmt->sa, mgmt->bssid, ETH_ALEN) == 0) {
		int c;
		c = wt->last_mgmt_valid ?
			WLANTEST_STA_COUNTER_VALID_DISASSOC_RX_ACK :
			WLANTEST_STA_COUNTER_INVALID_DISASSOC_RX_ACK;
		sta->counters[c]++;
	}
}


void rx_mgmt_ack(struct wlantest *wt, const struct ieee80211_hdr *hdr)
{
	u16 fc, stype;
	fc = le_to_host16(hdr->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	wpa_printf(MSG_MSGDUMP, "MGMT ACK: stype=%u a1=" MACSTR " a2=" MACSTR
		   " a3=" MACSTR,
		   stype, MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
		   MAC2STR(hdr->addr3));

	switch (stype) {
	case WLAN_FC_STYPE_DEAUTH:
		rx_mgmt_deauth_ack(wt, hdr);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		rx_mgmt_disassoc_ack(wt, hdr);
		break;
	}
}
