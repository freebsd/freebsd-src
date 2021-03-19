/*
 * Received Data frame processing for TDLS packets
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "crypto/sha256.h"
#include "crypto/crypto.h"
#include "crypto/aes_wrap.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "wlantest.h"


static struct wlantest_tdls * get_tdls(struct wlantest *wt, const u8 *linkid,
				       int create_new, const u8 *bssid)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *init, *resp;
	struct wlantest_tdls *tdls;

	bss = bss_find(wt, linkid);
	if (bss == NULL && bssid) {
		bss = bss_find(wt, bssid);
		if (bss)
			add_note(wt, MSG_INFO, "TDLS: Incorrect BSSID " MACSTR
				 " in LinkId?! (init=" MACSTR " resp="
				 MACSTR ")",
				 MAC2STR(linkid), MAC2STR(linkid + ETH_ALEN),
				 MAC2STR(linkid + 2 * ETH_ALEN));
	}
	if (bss == NULL)
		return NULL;

	init = sta_find(bss, linkid + ETH_ALEN);
	if (init == NULL)
		return NULL;

	resp = sta_find(bss, linkid + 2 * ETH_ALEN);
	if (resp == NULL)
		return NULL;

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if (tdls->init == init && tdls->resp == resp)
			return tdls;
	}

	if (!create_new)
		return NULL;

	add_note(wt, MSG_DEBUG, "Add new TDLS link context: initiator " MACSTR
		 " responder " MACSTR " BSSID " MACSTR,
		 MAC2STR(linkid + ETH_ALEN),
		 MAC2STR(linkid + 2 * ETH_ALEN),
		 MAC2STR(bssid));

	tdls = os_zalloc(sizeof(*tdls));
	if (tdls == NULL)
		return NULL;
	tdls->init = init;
	tdls->resp = resp;
	dl_list_add(&bss->tdls, &tdls->list);
	return tdls;
}


static int tdls_derive_tpk(struct wlantest_tdls *tdls, const u8 *bssid,
			   const u8 *ftie, u8 ftie_len)
{
	const struct rsn_ftie *f;
	u8 key_input[SHA256_MAC_LEN];
	const u8 *nonce[2];
	size_t len[2];
	u8 data[3 * ETH_ALEN];

	if (ftie == NULL || ftie_len < sizeof(struct rsn_ftie))
		return 0;

	f = (const struct rsn_ftie *) ftie;
	wpa_hexdump(MSG_DEBUG, "TDLS ANonce", f->anonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "TDLS SNonce", f->snonce, WPA_NONCE_LEN);

	/*
	 * IEEE Std 802.11z-2010 8.5.9.1:
	 * TPK-Key-Input = SHA-256(min(SNonce, ANonce) || max(SNonce, ANonce))
	 */
	len[0] = WPA_NONCE_LEN;
	len[1] = WPA_NONCE_LEN;
	if (os_memcmp(f->anonce, f->snonce, WPA_NONCE_LEN) < 0) {
		nonce[0] = f->anonce;
		nonce[1] = f->snonce;
	} else {
		nonce[0] = f->snonce;
		nonce[1] = f->anonce;
	}
	sha256_vector(2, nonce, len, key_input);
	wpa_hexdump_key(MSG_DEBUG, "TDLS: TPK-Key-Input",
			key_input, SHA256_MAC_LEN);

	/*
	 * TPK-Key-Data = KDF-N_KEY(TPK-Key-Input, "TDLS PMK",
	 *	min(MAC_I, MAC_R) || max(MAC_I, MAC_R) || BSSID || N_KEY)
	 * TODO: is N_KEY really included in KDF Context and if so, in which
	 * presentation format (little endian 16-bit?) is it used? It gets
	 * added by the KDF anyway..
	 */

	if (os_memcmp(tdls->init->addr, tdls->resp->addr, ETH_ALEN) < 0) {
		os_memcpy(data, tdls->init->addr, ETH_ALEN);
		os_memcpy(data + ETH_ALEN, tdls->resp->addr, ETH_ALEN);
	} else {
		os_memcpy(data, tdls->resp->addr, ETH_ALEN);
		os_memcpy(data + ETH_ALEN, tdls->init->addr, ETH_ALEN);
	}
	os_memcpy(data + 2 * ETH_ALEN, bssid, ETH_ALEN);
	wpa_hexdump(MSG_DEBUG, "TDLS: KDF Context", data, sizeof(data));

	sha256_prf(key_input, SHA256_MAC_LEN, "TDLS PMK", data, sizeof(data),
		   (u8 *) &tdls->tpk, sizeof(tdls->tpk));
	wpa_hexdump_key(MSG_DEBUG, "TDLS: TPK-KCK",
			tdls->tpk.kck, sizeof(tdls->tpk.kck));
	wpa_hexdump_key(MSG_DEBUG, "TDLS: TPK-TK",
			tdls->tpk.tk, sizeof(tdls->tpk.tk));

	return 1;
}


static int tdls_verify_mic(struct wlantest *wt, struct wlantest_tdls *tdls,
			   u8 trans_seq, struct ieee802_11_elems *elems)
{
	u8 *buf, *pos;
	int len;
	u8 mic[16];
	int ret;
	const struct rsn_ftie *rx_ftie;
	struct rsn_ftie *tmp_ftie;

	if (elems->link_id == NULL || elems->rsn_ie == NULL ||
	    elems->timeout_int == NULL || elems->ftie == NULL ||
	    elems->ftie_len < sizeof(struct rsn_ftie))
		return -1;

	len = 2 * ETH_ALEN + 1 + 2 + 18 + 2 + elems->rsn_ie_len +
		2 + 5 + 2 + elems->ftie_len;

	buf = os_zalloc(len);
	if (buf == NULL)
		return -1;

	pos = buf;
	/* 1) TDLS initiator STA MAC address */
	os_memcpy(pos, elems->link_id + ETH_ALEN, ETH_ALEN);
	pos += ETH_ALEN;
	/* 2) TDLS responder STA MAC address */
	os_memcpy(pos, elems->link_id + 2 * ETH_ALEN, ETH_ALEN);
	pos += ETH_ALEN;
	/* 3) Transaction Sequence number */
	*pos++ = trans_seq;
	/* 4) Link Identifier IE */
	os_memcpy(pos, elems->link_id - 2, 2 + 18);
	pos += 2 + 18;
	/* 5) RSN IE */
	os_memcpy(pos, elems->rsn_ie - 2, 2 + elems->rsn_ie_len);
	pos += 2 + elems->rsn_ie_len;
	/* 6) Timeout Interval IE */
	os_memcpy(pos, elems->timeout_int - 2, 2 + 5);
	pos += 2 + 5;
	/* 7) FTIE, with the MIC field of the FTIE set to 0 */
	os_memcpy(pos, elems->ftie - 2, 2 + elems->ftie_len);
	pos += 2;
	tmp_ftie = (struct rsn_ftie *) pos;
	os_memset(tmp_ftie->mic, 0, 16);
	pos += elems->ftie_len;

	wpa_hexdump(MSG_DEBUG, "TDLS: Data for FTIE MIC", buf, pos - buf);
	wpa_hexdump_key(MSG_DEBUG, "TDLS: KCK", tdls->tpk.kck, 16);
	ret = omac1_aes_128(tdls->tpk.kck, buf, pos - buf, mic);
	os_free(buf);
	if (ret)
		return -1;
	wpa_hexdump(MSG_DEBUG, "TDLS: FTIE MIC", mic, 16);
	rx_ftie = (const struct rsn_ftie *) elems->ftie;

	if (os_memcmp(mic, rx_ftie->mic, 16) == 0) {
		add_note(wt, MSG_DEBUG, "TDLS: Valid MIC");
		return 0;
	}
	add_note(wt, MSG_DEBUG, "TDLS: Invalid MIC");
	return -1;
}


static void rx_data_tdls_setup_request(struct wlantest *wt, const u8 *bssid,
				       const u8 *sta_addr, const u8 *dst,
				       const u8 *src,
				       const u8 *data, size_t len)
{
	struct ieee802_11_elems elems;
	struct wlantest_tdls *tdls;
	u8 linkid[3 * ETH_ALEN];

	if (len < 3) {
		add_note(wt, MSG_INFO, "Too short TDLS Setup Request " MACSTR
			 " -> " MACSTR, MAC2STR(src), MAC2STR(dst));
		return;
	}
	wpa_printf(MSG_DEBUG, "TDLS Setup Request " MACSTR " -> "
		   MACSTR, MAC2STR(src), MAC2STR(dst));

	if (ieee802_11_parse_elems(data + 3, len - 3, &elems, 1) ==
	    ParseFailed || elems.link_id == NULL)
		return;
	wpa_printf(MSG_DEBUG, "TDLS Link Identifier: BSSID " MACSTR
		   " initiator STA " MACSTR " responder STA " MACSTR,
		   MAC2STR(elems.link_id), MAC2STR(elems.link_id + ETH_ALEN),
		   MAC2STR(elems.link_id + 2 * ETH_ALEN));
	tdls = get_tdls(wt, elems.link_id, 1, bssid);
	if (tdls) {
		tdls->counters[WLANTEST_TDLS_COUNTER_SETUP_REQ]++;
		tdls->dialog_token = data[0];
		if (elems.ftie && elems.ftie_len >= sizeof(struct rsn_ftie)) {
			const struct rsn_ftie *f;
			f = (const struct rsn_ftie *) elems.ftie;
			os_memcpy(tdls->inonce, f->snonce, WPA_NONCE_LEN);
		}
	}

	/* Check whether reverse direction context exists already */
	os_memcpy(linkid, bssid, ETH_ALEN);
	os_memcpy(linkid + ETH_ALEN, dst, ETH_ALEN);
	os_memcpy(linkid + 2 * ETH_ALEN, src, ETH_ALEN);
	tdls = get_tdls(wt, linkid, 0, bssid);
	if (tdls)
		add_note(wt, MSG_INFO, "Reverse direction TDLS context exists");
}


static void rx_data_tdls_setup_response_failure(struct wlantest *wt,
						const u8 *bssid,
						const u8 *sta_addr,
						u8 dialog_token, u16 status)
{
	struct wlantest_bss *bss;
	struct wlantest_tdls *tdls;
	struct wlantest_sta *sta;

	if (status == WLAN_STATUS_SUCCESS) {
		add_note(wt, MSG_INFO, "TDLS: Invalid TDLS Setup Response from "
			 MACSTR, MAC2STR(sta_addr));
		return;
	}

	bss = bss_find(wt, bssid);
	if (!bss)
		return;
	sta = sta_find(bss, sta_addr);
	if (!sta)
		return;

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if (tdls->resp == sta) {
			if (dialog_token != tdls->dialog_token) {
				add_note(wt, MSG_DEBUG, "TDLS: Dialog token "
					 "mismatch in TDLS Setup Response "
					 "(failure)");
				break;
			}
			add_note(wt, MSG_DEBUG, "TDLS: Found matching TDLS "
				 "setup session based on dialog token");
			tdls->counters[
				WLANTEST_TDLS_COUNTER_SETUP_RESP_FAIL]++;
			break;
		}
	}
}


static void rx_data_tdls_setup_response(struct wlantest *wt, const u8 *bssid,
					const u8 *sta_addr, const u8 *dst,
					const u8 *src,
					const u8 *data, size_t len)
{
	u16 status;
	struct ieee802_11_elems elems;
	struct wlantest_tdls *tdls;

	if (len < 3) {
		add_note(wt, MSG_INFO, "Too short TDLS Setup Response " MACSTR
			 " -> " MACSTR, MAC2STR(src), MAC2STR(dst));
		return;
	}
	status = WPA_GET_LE16(data);
	wpa_printf(MSG_DEBUG, "TDLS Setup Response " MACSTR " -> "
		   MACSTR " (status %d)",
		   MAC2STR(src), MAC2STR(dst), status);
	if (len < 5 && status == 0) {
		add_note(wt, MSG_INFO, "Too short TDLS Setup Response " MACSTR
			 " -> " MACSTR, MAC2STR(src), MAC2STR(dst));
		return;
	}

	if (len < 5 ||
	    ieee802_11_parse_elems(data + 5, len - 5, &elems, 1) ==
	    ParseFailed || elems.link_id == NULL) {
		/* Need to match TDLS link based on Dialog Token */
		rx_data_tdls_setup_response_failure(wt, bssid, sta_addr,
						    data[2], status);
		return;
	}
	wpa_printf(MSG_DEBUG, "TDLS Link Identifier: BSSID " MACSTR
		   " initiator STA " MACSTR " responder STA " MACSTR,
		   MAC2STR(elems.link_id), MAC2STR(elems.link_id + ETH_ALEN),
		   MAC2STR(elems.link_id + 2 * ETH_ALEN));

	tdls = get_tdls(wt, elems.link_id, 1, bssid);
	if (!tdls) {
		add_note(wt, MSG_INFO, "No match TDLS context found");
		return;
	}
	if (status)
		tdls->counters[WLANTEST_TDLS_COUNTER_SETUP_RESP_FAIL]++;
	else
		tdls->counters[WLANTEST_TDLS_COUNTER_SETUP_RESP_OK]++;

	if (status != WLAN_STATUS_SUCCESS)
		return;

	if (elems.ftie && elems.ftie_len >= sizeof(struct rsn_ftie)) {
		const struct rsn_ftie *f;
		f = (const struct rsn_ftie *) elems.ftie;
		if (os_memcmp(tdls->inonce, f->snonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "Mismatch in TDLS initiator "
				 "nonce");
		}
		os_memcpy(tdls->rnonce, f->anonce, WPA_NONCE_LEN);
	}

	if (tdls_derive_tpk(tdls, bssid, elems.ftie, elems.ftie_len) < 1)
		return;
	if (tdls_verify_mic(wt, tdls, 2, &elems) == 0) {
		tdls->dialog_token = data[2];
		add_note(wt, MSG_DEBUG, "TDLS: Dialog Token for the link: %u",
			 tdls->dialog_token);
	}
}


static void rx_data_tdls_setup_confirm_failure(struct wlantest *wt,
					       const u8 *bssid,
					       const u8 *src,
					       u8 dialog_token, u16 status)
{
	struct wlantest_bss *bss;
	struct wlantest_tdls *tdls;
	struct wlantest_sta *sta;

	if (status == WLAN_STATUS_SUCCESS) {
		add_note(wt, MSG_INFO, "TDLS: Invalid TDLS Setup Confirm from "
			 MACSTR, MAC2STR(src));
		return;
	}

	bss = bss_find(wt, bssid);
	if (!bss)
		return;
	sta = sta_find(bss, src);
	if (!sta)
		return;

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if (tdls->init == sta) {
			if (dialog_token != tdls->dialog_token) {
				add_note(wt, MSG_DEBUG, "TDLS: Dialog token "
					 "mismatch in TDLS Setup Confirm "
					 "(failure)");
				break;
			}
			add_note(wt, MSG_DEBUG, "TDLS: Found matching TDLS "
				 "setup session based on dialog token");
			tdls->counters[
				WLANTEST_TDLS_COUNTER_SETUP_CONF_FAIL]++;
			break;
		}
	}
}


static void rx_data_tdls_setup_confirm(struct wlantest *wt, const u8 *bssid,
				       const u8 *sta_addr, const u8 *dst,
				       const u8 *src,
				       const u8 *data, size_t len)
{
	u16 status;
	struct ieee802_11_elems elems;
	struct wlantest_tdls *tdls;
	u8 link_id[3 * ETH_ALEN];

	if (len < 3) {
		add_note(wt, MSG_INFO, "Too short TDLS Setup Confirm " MACSTR
			 " -> " MACSTR, MAC2STR(src), MAC2STR(dst));
		return;
	}
	status = WPA_GET_LE16(data);
	wpa_printf(MSG_DEBUG, "TDLS Setup Confirm " MACSTR " -> "
		   MACSTR " (status %d)",
		   MAC2STR(src), MAC2STR(dst), status);

	if (ieee802_11_parse_elems(data + 3, len - 3, &elems, 1) ==
	    ParseFailed || elems.link_id == NULL) {
		/* Need to match TDLS link based on Dialog Token */
		rx_data_tdls_setup_confirm_failure(wt, bssid, src,
						   data[2], status);
		return;
	}
	wpa_printf(MSG_DEBUG, "TDLS Link Identifier: BSSID " MACSTR
		   " initiator STA " MACSTR " responder STA " MACSTR,
		   MAC2STR(elems.link_id), MAC2STR(elems.link_id + ETH_ALEN),
		   MAC2STR(elems.link_id + 2 * ETH_ALEN));

	tdls = get_tdls(wt, elems.link_id, 1, bssid);
	if (tdls == NULL)
		return;
	if (status)
		tdls->counters[WLANTEST_TDLS_COUNTER_SETUP_CONF_FAIL]++;
	else
		tdls->counters[WLANTEST_TDLS_COUNTER_SETUP_CONF_OK]++;

	if (status != WLAN_STATUS_SUCCESS)
		return;

	if (elems.ftie && elems.ftie_len >= sizeof(struct rsn_ftie)) {
		const struct rsn_ftie *f;
		f = (const struct rsn_ftie *) elems.ftie;
		if (os_memcmp(tdls->inonce, f->snonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "Mismatch in TDLS initiator "
				 "nonce");
		}
		if (os_memcmp(tdls->rnonce, f->anonce, WPA_NONCE_LEN) != 0) {
			add_note(wt, MSG_INFO, "Mismatch in TDLS responder "
				 "nonce");
		}
	}

	tdls->link_up = 1;
	if (tdls_derive_tpk(tdls, bssid, elems.ftie, elems.ftie_len) < 1) {
		if (elems.ftie == NULL)
			goto remove_reverse;
		return;
	}
	if (tdls_verify_mic(wt, tdls, 3, &elems) == 0) {
		tdls->dialog_token = data[2];
		add_note(wt, MSG_DEBUG, "TDLS: Link up - Dialog Token: %u",
			 tdls->dialog_token);
	}

remove_reverse:
	/*
	 * The TDLS link itself is bidirectional, but there is explicit
	 * initiator/responder roles. Remove the other direction of the link
	 * (if it exists) to make sure that the link counters are stored for
	 * the current TDLS entery.
	 */
	os_memcpy(link_id, elems.link_id, ETH_ALEN);
	os_memcpy(link_id + ETH_ALEN, elems.link_id + 2 * ETH_ALEN, ETH_ALEN);
	os_memcpy(link_id + 2 * ETH_ALEN, elems.link_id + ETH_ALEN, ETH_ALEN);
	tdls = get_tdls(wt, link_id, 0, bssid);
	if (tdls) {
		add_note(wt, MSG_DEBUG, "TDLS: Remove reverse link entry");
		tdls_deinit(tdls);
	}
}


static int tdls_verify_mic_teardown(struct wlantest *wt,
				    struct wlantest_tdls *tdls, u8 trans_seq,
				    const u8 *reason_code,
				    struct ieee802_11_elems *elems)
{
	u8 *buf, *pos;
	int len;
	u8 mic[16];
	int ret;
	const struct rsn_ftie *rx_ftie;
	struct rsn_ftie *tmp_ftie;

	if (elems->link_id == NULL || elems->ftie == NULL ||
	    elems->ftie_len < sizeof(struct rsn_ftie))
		return -1;

	len = 2 + 18 + 2 + 1 + 1 + 2 + elems->ftie_len;

	buf = os_zalloc(len);
	if (buf == NULL)
		return -1;

	pos = buf;
	/* 1) Link Identifier IE */
	os_memcpy(pos, elems->link_id - 2, 2 + 18);
	pos += 2 + 18;
	/* 2) Reason Code */
	os_memcpy(pos, reason_code, 2);
	pos += 2;
	/* 3) Dialog token */
	*pos++ = tdls->dialog_token;
	/* 4) Transaction Sequence number */
	*pos++ = trans_seq;
	/* 5) FTIE, with the MIC field of the FTIE set to 0 */
	os_memcpy(pos, elems->ftie - 2, 2 + elems->ftie_len);
	pos += 2;
	tmp_ftie = (struct rsn_ftie *) pos;
	os_memset(tmp_ftie->mic, 0, 16);
	pos += elems->ftie_len;

	wpa_hexdump(MSG_DEBUG, "TDLS: Data for FTIE MIC", buf, pos - buf);
	wpa_hexdump_key(MSG_DEBUG, "TDLS: KCK", tdls->tpk.kck, 16);
	ret = omac1_aes_128(tdls->tpk.kck, buf, pos - buf, mic);
	os_free(buf);
	if (ret)
		return -1;
	wpa_hexdump(MSG_DEBUG, "TDLS: FTIE MIC", mic, 16);
	rx_ftie = (const struct rsn_ftie *) elems->ftie;

	if (os_memcmp(mic, rx_ftie->mic, 16) == 0) {
		add_note(wt, MSG_DEBUG, "TDLS: Valid MIC");
		return 0;
	}
	add_note(wt, MSG_DEBUG, "TDLS: Invalid MIC");
	return -1;
}


static void rx_data_tdls_teardown(struct wlantest *wt, const u8 *bssid,
				  const u8 *sta_addr, const u8 *dst,
				  const u8 *src,
				  const u8 *data, size_t len)
{
	u16 reason;
	struct ieee802_11_elems elems;
	struct wlantest_tdls *tdls;

	if (len < 2)
		return;
	reason = WPA_GET_LE16(data);
	wpa_printf(MSG_DEBUG, "TDLS Teardown " MACSTR " -> "
		   MACSTR " (reason %d)",
		   MAC2STR(src), MAC2STR(dst), reason);

	if (ieee802_11_parse_elems(data + 2, len - 2, &elems, 1) ==
	    ParseFailed || elems.link_id == NULL)
		return;
	wpa_printf(MSG_DEBUG, "TDLS Link Identifier: BSSID " MACSTR
		   " initiator STA " MACSTR " responder STA " MACSTR,
		   MAC2STR(elems.link_id), MAC2STR(elems.link_id + ETH_ALEN),
		   MAC2STR(elems.link_id + 2 * ETH_ALEN));

	tdls = get_tdls(wt, elems.link_id, 1, bssid);
	if (tdls) {
		if (tdls->link_up)
			add_note(wt, MSG_DEBUG, "TDLS: Link down");
		tdls->link_up = 0;
		tdls->counters[WLANTEST_TDLS_COUNTER_TEARDOWN]++;
		tdls_verify_mic_teardown(wt, tdls, 4, data, &elems);
	}
}


static void rx_data_tdls(struct wlantest *wt, const u8 *bssid,
			 const u8 *sta_addr, const u8 *dst, const u8 *src,
			 const u8 *data, size_t len)
{
	/* data contains the payload of a TDLS Action frame */
	if (len < 2 || data[0] != WLAN_ACTION_TDLS) {
		wpa_hexdump(MSG_DEBUG, "Unrecognized encapsulated TDLS frame",
			    data, len);
		return;
	}

	switch (data[1]) {
	case WLAN_TDLS_SETUP_REQUEST:
		rx_data_tdls_setup_request(wt, bssid, sta_addr, dst, src,
					   data + 2, len - 2);
		break;
	case WLAN_TDLS_SETUP_RESPONSE:
		rx_data_tdls_setup_response(wt, bssid, sta_addr, dst, src,
					    data + 2, len - 2);
		break;
	case WLAN_TDLS_SETUP_CONFIRM:
		rx_data_tdls_setup_confirm(wt, bssid, sta_addr, dst, src,
					   data + 2, len - 2);
		break;
	case WLAN_TDLS_TEARDOWN:
		rx_data_tdls_teardown(wt, bssid, sta_addr, dst, src, data + 2,
				      len - 2);
		break;
	case WLAN_TDLS_DISCOVERY_REQUEST:
		wpa_printf(MSG_DEBUG, "TDLS Discovery Request " MACSTR " -> "
			   MACSTR, MAC2STR(src), MAC2STR(dst));
		break;
	}
}


void rx_data_80211_encap(struct wlantest *wt, const u8 *bssid,
			 const u8 *sta_addr, const u8 *dst, const u8 *src,
			 const u8 *data, size_t len)
{
	wpa_hexdump(MSG_EXCESSIVE, "802.11 data encap frame", data, len);
	if (len < 1)
		return;
	if (data[0] == 0x02)
		rx_data_tdls(wt, bssid, sta_addr, dst, src, data + 1, len - 1);
}
