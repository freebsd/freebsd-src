/*
 * Received Data frame processing
 * Copyright (c) 2010-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "wlantest.h"


static const char * data_stype(u16 stype)
{
	switch (stype) {
	case WLAN_FC_STYPE_DATA:
		return "DATA";
	case WLAN_FC_STYPE_DATA_CFACK:
		return "DATA-CFACK";
	case WLAN_FC_STYPE_DATA_CFPOLL:
		return "DATA-CFPOLL";
	case WLAN_FC_STYPE_DATA_CFACKPOLL:
		return "DATA-CFACKPOLL";
	case WLAN_FC_STYPE_NULLFUNC:
		return "NULLFUNC";
	case WLAN_FC_STYPE_CFACK:
		return "CFACK";
	case WLAN_FC_STYPE_CFPOLL:
		return "CFPOLL";
	case WLAN_FC_STYPE_CFACKPOLL:
		return "CFACKPOLL";
	case WLAN_FC_STYPE_QOS_DATA:
		return "QOSDATA";
	case WLAN_FC_STYPE_QOS_DATA_CFACK:
		return "QOSDATA-CFACK";
	case WLAN_FC_STYPE_QOS_DATA_CFPOLL:
		return "QOSDATA-CFPOLL";
	case WLAN_FC_STYPE_QOS_DATA_CFACKPOLL:
		return "QOSDATA-CFACKPOLL";
	case WLAN_FC_STYPE_QOS_NULL:
		return "QOS-NULL";
	case WLAN_FC_STYPE_QOS_CFPOLL:
		return "QOS-CFPOLL";
	case WLAN_FC_STYPE_QOS_CFACKPOLL:
		return "QOS-CFACKPOLL";
	}
	return "??";
}


static void rx_data_eth(struct wlantest *wt, const u8 *bssid,
			const u8 *sta_addr, const u8 *dst, const u8 *src,
			u16 ethertype, const u8 *data, size_t len, int prot,
			const u8 *peer_addr);

static void rx_data_vlan(struct wlantest *wt, const u8 *bssid,
			 const u8 *sta_addr, const u8 *dst, const u8 *src,
			 const u8 *data, size_t len, int prot,
			 const u8 *peer_addr)
{
	u16 tag;

	if (len < 4)
		return;
	tag = WPA_GET_BE16(data);
	wpa_printf(MSG_MSGDUMP, "VLAN tag: Priority=%u ID=%u",
		   tag >> 12, tag & 0x0ffff);
	/* ignore VLAN information and process the original frame */
	rx_data_eth(wt, bssid, sta_addr, dst, src, WPA_GET_BE16(data + 2),
		    data + 4, len - 4, prot, peer_addr);
}


static void rx_data_eth(struct wlantest *wt, const u8 *bssid,
			const u8 *sta_addr, const u8 *dst, const u8 *src,
			u16 ethertype, const u8 *data, size_t len, int prot,
			const u8 *peer_addr)
{
	switch (ethertype) {
	case ETH_P_PAE:
		rx_data_eapol(wt, bssid, sta_addr, dst, src, data, len, prot);
		break;
	case ETH_P_IP:
		rx_data_ip(wt, bssid, sta_addr, dst, src, data, len,
			   peer_addr);
		break;
	case 0x890d:
		rx_data_80211_encap(wt, bssid, sta_addr, dst, src, data, len);
		break;
	case ETH_P_8021Q:
		rx_data_vlan(wt, bssid, sta_addr, dst, src, data, len, prot,
			     peer_addr);
		break;
	}
}


static void rx_data_process(struct wlantest *wt, struct wlantest_bss *bss,
			    const u8 *bssid,
			    const u8 *sta_addr,
			    const u8 *dst, const u8 *src,
			    const u8 *data, size_t len, int prot,
			    const u8 *peer_addr, const u8 *qos)
{
	if (len == 0)
		return;

	if (bss && bss->mesh && qos && !(qos[0] & BIT(7)) &&
	    (qos[1] & BIT(0))) {
		u8 addr_ext_mode;
		size_t mesh_control_len = 6;

		/* Skip Mesh Control field if this is not an A-MSDU */
		if (len < mesh_control_len) {
			wpa_printf(MSG_DEBUG,
				   "Not enough room for Mesh Control field");
			return;
		}

		addr_ext_mode = data[0] & 0x03;
		if (addr_ext_mode == 3) {
			wpa_printf(MSG_DEBUG,
				   "Reserved Mesh Control :: Address Extension Mode");
			return;
		}

		mesh_control_len += addr_ext_mode * ETH_ALEN;
		if (len < mesh_control_len) {
			wpa_printf(MSG_DEBUG,
				   "Not enough room for Mesh Address Extension");
			return;
		}

		len -= mesh_control_len;
		data += mesh_control_len;
	}

	if (len >= 8 && os_memcmp(data, "\xaa\xaa\x03\x00\x00\x00", 6) == 0) {
		rx_data_eth(wt, bssid, sta_addr, dst, src,
			    WPA_GET_BE16(data + 6), data + 8, len - 8, prot,
			    peer_addr);
		return;
	}

	wpa_hexdump(MSG_DEBUG, "Unrecognized LLC", data, len > 8 ? 8 : len);
}


static u8 * try_ptk(int pairwise_cipher, struct wpa_ptk *ptk,
		    const struct ieee80211_hdr *hdr,
		    const u8 *data, size_t data_len, size_t *decrypted_len)
{
	u8 *decrypted;
	unsigned int tk_len = ptk->tk_len;

	decrypted = NULL;
	if ((pairwise_cipher == WPA_CIPHER_CCMP ||
	     pairwise_cipher == 0) && tk_len == 16) {
		decrypted = ccmp_decrypt(ptk->tk, hdr, data,
					 data_len, decrypted_len);
	} else if ((pairwise_cipher == WPA_CIPHER_CCMP_256 ||
		    pairwise_cipher == 0) && tk_len == 32) {
		decrypted = ccmp_256_decrypt(ptk->tk, hdr, data,
					     data_len, decrypted_len);
	} else if ((pairwise_cipher == WPA_CIPHER_GCMP ||
		    pairwise_cipher == WPA_CIPHER_GCMP_256 ||
		    pairwise_cipher == 0) &&
		   (tk_len == 16 || tk_len == 32)) {
		decrypted = gcmp_decrypt(ptk->tk, tk_len, hdr,
					 data, data_len, decrypted_len);
	} else if ((pairwise_cipher == WPA_CIPHER_TKIP ||
		    pairwise_cipher == 0) && tk_len == 32) {
		decrypted = tkip_decrypt(ptk->tk, hdr, data, data_len,
					 decrypted_len);
	}

	return decrypted;
}


static u8 * try_all_ptk(struct wlantest *wt, int pairwise_cipher,
			const struct ieee80211_hdr *hdr, int keyid,
			const u8 *data, size_t data_len, size_t *decrypted_len)
{
	struct wlantest_ptk *ptk;
	u8 *decrypted;
	int prev_level = wpa_debug_level;

	wpa_debug_level = MSG_WARNING;
	dl_list_for_each(ptk, &wt->ptk, struct wlantest_ptk, list) {
		decrypted = try_ptk(pairwise_cipher, &ptk->ptk, hdr,
				    data, data_len, decrypted_len);
		if (decrypted) {
			wpa_debug_level = prev_level;
			add_note(wt, MSG_DEBUG,
				 "Found PTK match from list of all known PTKs");
			write_decrypted_note(wt, decrypted, ptk->ptk.tk,
					     ptk->ptk.tk_len, keyid);
			return decrypted;
		}
	}
	wpa_debug_level = prev_level;

	return NULL;
}


static void check_plaintext_prot(struct wlantest *wt,
				 const struct ieee80211_hdr *hdr,
				 const u8 *data, size_t len)
{
	if (len < 8 + 3 || data[8] != 0xaa || data[9] != 0xaa ||
	    data[10] != 0x03)
		return;

	add_note(wt, MSG_DEBUG,
		 "Plaintext payload in protected frame");
	wpa_printf(MSG_INFO, "Plaintext payload in protected frame #%u: A2="
		   MACSTR " seq=%u",
		   wt->frame_num, MAC2STR(hdr->addr2),
		   WLAN_GET_SEQ_SEQ(le_to_host16(hdr->seq_ctrl)));
}


static void rx_data_bss_prot_group(struct wlantest *wt,
				   const struct ieee80211_hdr *hdr,
				   size_t hdrlen,
				   const u8 *qos, const u8 *dst, const u8 *src,
				   const u8 *data, size_t len)
{
	struct wlantest_bss *bss;
	int keyid;
	u8 *decrypted = NULL;
	size_t dlen;
	u8 pn[6];
	int replay = 0;

	bss = bss_get(wt, hdr->addr2);
	if (bss == NULL)
		return;
	if (len < 4) {
		add_note(wt, MSG_INFO, "Too short group addressed data frame");
		return;
	}

	if (bss->group_cipher & (WPA_CIPHER_TKIP | WPA_CIPHER_CCMP) &&
	    !(data[3] & 0x20)) {
		add_note(wt, MSG_INFO, "Expected TKIP/CCMP frame from "
			 MACSTR " did not have ExtIV bit set to 1",
			 MAC2STR(bss->bssid));
		return;
	}

	if (bss->group_cipher == WPA_CIPHER_TKIP) {
		if (data[3] & 0x1f) {
			add_note(wt, MSG_INFO, "TKIP frame from " MACSTR
				 " used non-zero reserved bit",
				 MAC2STR(bss->bssid));
		}
		if (data[1] != ((data[0] | 0x20) & 0x7f)) {
			add_note(wt, MSG_INFO, "TKIP frame from " MACSTR
				 " used incorrect WEPSeed[1] (was 0x%x, "
				 "expected 0x%x)",
				 MAC2STR(bss->bssid), data[1],
				 (data[0] | 0x20) & 0x7f);
		}
	} else if (bss->group_cipher == WPA_CIPHER_CCMP) {
		if (data[2] != 0 || (data[3] & 0x1f) != 0) {
			add_note(wt, MSG_INFO, "CCMP frame from " MACSTR
				 " used non-zero reserved bit",
				 MAC2STR(bss->bssid));
		}
	}

	check_plaintext_prot(wt, hdr, data, len);
	keyid = data[3] >> 6;
	if (bss->gtk_len[keyid] == 0 &&
	    (bss->group_cipher != WPA_CIPHER_WEP40 ||
	     dl_list_empty(&wt->wep))) {
		decrypted = try_all_ptk(wt, bss->group_cipher, hdr, keyid,
					data, len, &dlen);
		if (decrypted)
			goto process;
		add_note(wt, MSG_MSGDUMP,
			 "No GTK known to decrypt the frame (A2=" MACSTR
			 " KeyID=%d)",
			 MAC2STR(hdr->addr2), keyid);
		return;
	}

	if (bss->group_cipher == WPA_CIPHER_TKIP)
		tkip_get_pn(pn, data);
	else if (bss->group_cipher == WPA_CIPHER_WEP40)
		goto skip_replay_det;
	else
		ccmp_get_pn(pn, data);
	if (os_memcmp(pn, bss->rsc[keyid], 6) <= 0) {
		u16 seq_ctrl = le_to_host16(hdr->seq_ctrl);
		char pn_hex[6 * 2 + 1], rsc_hex[6 * 2 + 1];

		wpa_snprintf_hex(pn_hex, sizeof(pn_hex), pn, 6);
		wpa_snprintf_hex(rsc_hex, sizeof(rsc_hex), bss->rsc[keyid], 6);
		add_note(wt, MSG_INFO, "replay detected: A1=" MACSTR
			 " A2=" MACSTR " A3=" MACSTR
			 " seq=%u frag=%u%s keyid=%d #%u %s<=%s",
			 MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			 MAC2STR(hdr->addr3),
			 WLAN_GET_SEQ_SEQ(seq_ctrl),
			 WLAN_GET_SEQ_FRAG(seq_ctrl),
			 (le_to_host16(hdr->frame_control) & WLAN_FC_RETRY) ?
			 " Retry" : "",
			 keyid, wt->frame_num, pn_hex, rsc_hex);
		replay = 1;
	}

skip_replay_det:
	if (bss->group_cipher == WPA_CIPHER_TKIP)
		decrypted = tkip_decrypt(bss->gtk[keyid], hdr, data, len,
					 &dlen);
	else if (bss->group_cipher == WPA_CIPHER_WEP40)
		decrypted = wep_decrypt(wt, hdr, data, len, &dlen);
	else if (bss->group_cipher == WPA_CIPHER_CCMP)
		decrypted = ccmp_decrypt(bss->gtk[keyid], hdr, data, len,
					 &dlen);
	else if (bss->group_cipher == WPA_CIPHER_CCMP_256)
		decrypted = ccmp_256_decrypt(bss->gtk[keyid], hdr, data, len,
					     &dlen);
	else if (bss->group_cipher == WPA_CIPHER_GCMP ||
		 bss->group_cipher == WPA_CIPHER_GCMP_256)
		decrypted = gcmp_decrypt(bss->gtk[keyid], bss->gtk_len[keyid],
					 hdr, data, len, &dlen);

	if (decrypted) {
		char gtk[65];

		wpa_snprintf_hex(gtk, sizeof(gtk), bss->gtk[keyid],
				 bss->gtk_len[keyid]);
		add_note(wt, MSG_EXCESSIVE, "GTK[%d] %s", keyid, gtk);
	process:
		rx_data_process(wt, bss, bss->bssid, NULL, dst, src, decrypted,
				dlen, 1, NULL, qos);
		if (!replay)
			os_memcpy(bss->rsc[keyid], pn, 6);
		write_pcap_decrypted(wt, (const u8 *) hdr, hdrlen,
				     decrypted, dlen);
	} else {
		wpa_printf(MSG_DEBUG, "Failed to decrypt frame (group) #%u A2="
			   MACSTR " seq=%u",
			   wt->frame_num, MAC2STR(hdr->addr2),
			   WLAN_GET_SEQ_SEQ(le_to_host16(hdr->seq_ctrl)));
		add_note(wt, MSG_DEBUG, "Failed to decrypt frame (group)");
	}
	os_free(decrypted);
}


static u8 * try_ptk_decrypt(struct wlantest *wt, struct wlantest_sta *sta,
			    const struct ieee80211_hdr *hdr, int keyid,
			    const u8 *data, size_t len,
			    const u8 *tk, size_t tk_len, size_t *dlen)
{
	u8 *decrypted = NULL;

	if (sta->pairwise_cipher == WPA_CIPHER_CCMP_256)
		decrypted = ccmp_256_decrypt(tk, hdr, data, len, dlen);
	else if (sta->pairwise_cipher == WPA_CIPHER_GCMP ||
		 sta->pairwise_cipher == WPA_CIPHER_GCMP_256)
		decrypted = gcmp_decrypt(tk, tk_len, hdr, data, len, dlen);
	else
		decrypted = ccmp_decrypt(tk, hdr, data, len, dlen);
	write_decrypted_note(wt, decrypted, tk, tk_len, keyid);

	return decrypted;
}


static void rx_data_bss_prot(struct wlantest *wt,
			     const struct ieee80211_hdr *hdr, size_t hdrlen,
			     const u8 *qos, const u8 *dst, const u8 *src,
			     const u8 *data, size_t len)
{
	struct wlantest_bss *bss, *bss2;
	struct wlantest_sta *sta, *sta2;
	int keyid;
	u16 fc = le_to_host16(hdr->frame_control);
	u8 *decrypted = NULL;
	size_t dlen;
	int tid;
	u8 pn[6], *rsc = NULL;
	struct wlantest_tdls *tdls = NULL, *found;
	const u8 *tk = NULL;
	int ptk_iter_done = 0;
	int try_ptk_iter = 0;
	int replay = 0;
	int only_zero_tk = 0;
	u16 seq_ctrl = le_to_host16(hdr->seq_ctrl);

	if (hdr->addr1[0] & 0x01) {
		rx_data_bss_prot_group(wt, hdr, hdrlen, qos, dst, src,
				       data, len);
		return;
	}

	if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
	    (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
		bss = bss_find(wt, hdr->addr1);
		if (bss) {
			sta = sta_find(bss, hdr->addr2);
			if (sta) {
				sta->counters[
					WLANTEST_STA_COUNTER_PROT_DATA_TX]++;
			}
			if (!sta || !sta->ptk_set) {
				bss2 = bss_find(wt, hdr->addr2);
				if (bss2) {
					sta2 = sta_find(bss2, hdr->addr1);
					if (sta2 && (!sta || sta2->ptk_set)) {
						bss = bss2;
						sta = sta2;
					}
				}
			}
		} else {
			bss = bss_find(wt, hdr->addr2);
			if (!bss)
				return;
			sta = sta_find(bss, hdr->addr1);
		}
	} else if (fc & WLAN_FC_TODS) {
		bss = bss_get(wt, hdr->addr1);
		if (bss == NULL)
			return;
		sta = sta_get(bss, hdr->addr2);
		if (sta)
			sta->counters[WLANTEST_STA_COUNTER_PROT_DATA_TX]++;
	} else if (fc & WLAN_FC_FROMDS) {
		bss = bss_get(wt, hdr->addr2);
		if (bss == NULL)
			return;
		sta = sta_get(bss, hdr->addr1);
	} else {
		bss = bss_get(wt, hdr->addr3);
		if (bss == NULL)
			return;
		sta = sta_find(bss, hdr->addr2);
		sta2 = sta_find(bss, hdr->addr1);
		if (sta == NULL || sta2 == NULL)
			return;
		found = NULL;
		dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list)
		{
			if ((tdls->init == sta && tdls->resp == sta2) ||
			    (tdls->init == sta2 && tdls->resp == sta)) {
				found = tdls;
				if (tdls->link_up)
					break;
			}
		}
		if (found) {
			if (!found->link_up)
				add_note(wt, MSG_DEBUG,
					 "TDLS: Link not up, but Data "
					 "frame seen");
			tk = found->tpk.tk;
			tdls = found;
		}
	}
	check_plaintext_prot(wt, hdr, data, len);
	if ((sta == NULL ||
	     (!sta->ptk_set && sta->pairwise_cipher != WPA_CIPHER_WEP40)) &&
	    tk == NULL) {
		add_note(wt, MSG_MSGDUMP, "No PTK known to decrypt the frame");
		if (dl_list_empty(&wt->ptk)) {
			if (len >= 4 && sta) {
				keyid = data[3] >> 6;
				only_zero_tk = 1;
				goto check_zero_tk;
			}
			return;
		}

		try_ptk_iter = 1;
	}

	if (len < 4) {
		add_note(wt, MSG_INFO, "Too short encrypted data frame");
		return;
	}

	if (sta == NULL)
		return;
	if (sta->pairwise_cipher & (WPA_CIPHER_TKIP | WPA_CIPHER_CCMP) &&
	    !(data[3] & 0x20)) {
		add_note(wt, MSG_INFO, "Expected TKIP/CCMP frame from "
			 MACSTR " did not have ExtIV bit set to 1",
			 MAC2STR(src));
		return;
	}

	if (tk == NULL && sta->pairwise_cipher == WPA_CIPHER_TKIP) {
		if (data[3] & 0x1f) {
			add_note(wt, MSG_INFO, "TKIP frame from " MACSTR
				 " used non-zero reserved bit",
				 MAC2STR(hdr->addr2));
		}
		if (data[1] != ((data[0] | 0x20) & 0x7f)) {
			add_note(wt, MSG_INFO, "TKIP frame from " MACSTR
				 " used incorrect WEPSeed[1] (was 0x%x, "
				 "expected 0x%x)",
				 MAC2STR(hdr->addr2), data[1],
				 (data[0] | 0x20) & 0x7f);
		}
	} else if (tk || sta->pairwise_cipher == WPA_CIPHER_CCMP) {
		if (data[2] != 0 || (data[3] & 0x1f) != 0) {
			add_note(wt, MSG_INFO, "CCMP frame from " MACSTR
				 " used non-zero reserved bit",
				 MAC2STR(hdr->addr2));
		}
	}

	keyid = data[3] >> 6;
	if (keyid != 0 &&
	    (!(sta->rsn_capab & WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST) ||
	     !(bss->rsn_capab & WPA_CAPABILITY_EXT_KEY_ID_FOR_UNICAST) ||
	     keyid != 1)) {
		add_note(wt, MSG_INFO,
			 "Unexpected KeyID %d in individually addressed Data frame from "
			 MACSTR,
			 keyid, MAC2STR(hdr->addr2));
	}

	if (qos) {
		tid = qos[0] & 0x0f;
		if (fc & WLAN_FC_TODS)
			sta->tx_tid[tid]++;
		else
			sta->rx_tid[tid]++;
	} else {
		tid = 0;
		if (fc & WLAN_FC_TODS)
			sta->tx_tid[16]++;
		else
			sta->rx_tid[16]++;
	}
	if (tk) {
		if (os_memcmp(hdr->addr2, tdls->init->addr, ETH_ALEN) == 0)
			rsc = tdls->rsc_init[tid];
		else
			rsc = tdls->rsc_resp[tid];
	} else if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
		   (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
		if (os_memcmp(sta->addr, hdr->addr2, ETH_ALEN) == 0)
			rsc = sta->rsc_tods[tid];
		else
			rsc = sta->rsc_fromds[tid];
	} else if (fc & WLAN_FC_TODS)
		rsc = sta->rsc_tods[tid];
	else
		rsc = sta->rsc_fromds[tid];


	if (tk == NULL && sta->pairwise_cipher == WPA_CIPHER_TKIP)
		tkip_get_pn(pn, data);
	else if (sta->pairwise_cipher == WPA_CIPHER_WEP40)
		goto skip_replay_det;
	else
		ccmp_get_pn(pn, data);
	if (os_memcmp(pn, rsc, 6) <= 0) {
		char pn_hex[6 * 2 + 1], rsc_hex[6 * 2 + 1];

		wpa_snprintf_hex(pn_hex, sizeof(pn_hex), pn, 6);
		wpa_snprintf_hex(rsc_hex, sizeof(rsc_hex), rsc, 6);
		add_note(wt, MSG_INFO, "replay detected: A1=" MACSTR
			 " A2=" MACSTR " A3=" MACSTR
			 " seq=%u frag=%u%s keyid=%d tid=%d #%u %s<=%s",
			 MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			 MAC2STR(hdr->addr3),
			 WLAN_GET_SEQ_SEQ(seq_ctrl),
			 WLAN_GET_SEQ_FRAG(seq_ctrl),
			 (le_to_host16(hdr->frame_control) &  WLAN_FC_RETRY) ?
			 " Retry" : "",
			 keyid, tid, wt->frame_num, pn_hex, rsc_hex);
		replay = 1;
	}

skip_replay_det:
	if (tk) {
		if (sta->pairwise_cipher == WPA_CIPHER_CCMP_256) {
			decrypted = ccmp_256_decrypt(tk, hdr, data, len, &dlen);
			write_decrypted_note(wt, decrypted, tk, 32, keyid);
		} else if (sta->pairwise_cipher == WPA_CIPHER_GCMP ||
			   sta->pairwise_cipher == WPA_CIPHER_GCMP_256) {
			decrypted = gcmp_decrypt(tk, sta->ptk.tk_len, hdr, data,
						 len, &dlen);
			write_decrypted_note(wt, decrypted, tk, sta->ptk.tk_len,
					     keyid);
		} else {
			decrypted = ccmp_decrypt(tk, hdr, data, len, &dlen);
			write_decrypted_note(wt, decrypted, tk, 16, keyid);
		}
	} else if (sta->pairwise_cipher == WPA_CIPHER_TKIP) {
		decrypted = tkip_decrypt(sta->ptk.tk, hdr, data, len, &dlen);
		write_decrypted_note(wt, decrypted, sta->ptk.tk, 32, keyid);
	} else if (sta->pairwise_cipher == WPA_CIPHER_WEP40) {
		decrypted = wep_decrypt(wt, hdr, data, len, &dlen);
	} else if (sta->ptk_set) {
		decrypted = try_ptk_decrypt(wt, sta, hdr, keyid, data, len,
					    sta->ptk.tk, sta->ptk.tk_len,
					    &dlen);
	} else {
		decrypted = try_all_ptk(wt, sta->pairwise_cipher, hdr, keyid,
					data, len, &dlen);
		ptk_iter_done = 1;
	}
	if (!decrypted && !ptk_iter_done) {
		decrypted = try_all_ptk(wt, sta->pairwise_cipher, hdr, keyid,
					data, len, &dlen);
		if (decrypted) {
			add_note(wt, MSG_DEBUG, "Current PTK did not work, but found a match from all known PTKs");
		}
	}
check_zero_tk:
	if (!decrypted) {
		struct wpa_ptk zero_ptk;
		int old_debug_level = wpa_debug_level;

		os_memset(&zero_ptk, 0, sizeof(zero_ptk));
		zero_ptk.tk_len = wpa_cipher_key_len(sta->pairwise_cipher);
		wpa_debug_level = MSG_ERROR;
		decrypted = try_ptk(sta->pairwise_cipher, &zero_ptk, hdr,
				    data, len, &dlen);
		wpa_debug_level = old_debug_level;
		if (decrypted) {
			add_note(wt, MSG_DEBUG,
				 "Frame was encrypted with zero TK");
			wpa_printf(MSG_INFO, "Zero TK used in frame #%u: A2="
				   MACSTR " seq=%u",
				   wt->frame_num, MAC2STR(hdr->addr2),
				   WLAN_GET_SEQ_SEQ(
					   le_to_host16(hdr->seq_ctrl)));
			write_decrypted_note(wt, decrypted, zero_ptk.tk,
					     zero_ptk.tk_len, keyid);
		}
	}
	if (decrypted) {
		u16 fc = le_to_host16(hdr->frame_control);
		const u8 *peer_addr = NULL;
		if (!(fc & (WLAN_FC_FROMDS | WLAN_FC_TODS)))
			peer_addr = hdr->addr1;
		if (!replay && rsc)
			os_memcpy(rsc, pn, 6);
		rx_data_process(wt, bss, bss->bssid, sta->addr, dst, src,
				decrypted, dlen, 1, peer_addr, qos);
		write_pcap_decrypted(wt, (const u8 *) hdr, hdrlen,
				     decrypted, dlen);
	} else if (sta->tptk_set) {
		/* Check whether TPTK has a matching TK that could be used to
		 * decrypt the frame. That could happen if EAPOL-Key msg 4/4
		 * was missing in the capture and this was PTK rekeying. */
		decrypted = try_ptk_decrypt(wt, sta, hdr, keyid, data, len,
					    sta->tptk.tk, sta->tptk.tk_len,
					    &dlen);
		if (decrypted) {
			add_note(wt, MSG_DEBUG,
				 "Update PTK (rekeying; no valid EAPOL-Key msg 4/4 seen)");
			os_memcpy(&sta->ptk, &sta->tptk, sizeof(sta->ptk));
			sta->ptk_set = 1;
			sta->tptk_set = 0;
			os_memset(sta->rsc_tods, 0, sizeof(sta->rsc_tods));
			os_memset(sta->rsc_fromds, 0, sizeof(sta->rsc_fromds));
		}
	} else {
		if (!try_ptk_iter && !only_zero_tk) {
			wpa_printf(MSG_DEBUG,
				   "Failed to decrypt frame #%u A2=" MACSTR
				   " seq=%u",
				   wt->frame_num, MAC2STR(hdr->addr2),
				   WLAN_GET_SEQ_SEQ(seq_ctrl));
			add_note(wt, MSG_DEBUG, "Failed to decrypt frame");
		}

		/* Assume the frame was corrupted and there was no FCS to check.
		 * Allow retry of this particular frame to be processed so that
		 * it could end up getting decrypted if it was received without
		 * corruption. */
		sta->allow_duplicate = 1;
	}
	os_free(decrypted);
}


static void rx_data_bss(struct wlantest *wt, const struct ieee80211_hdr *hdr,
			size_t hdrlen, const u8 *qos, const u8 *dst,
			const u8 *src, const u8 *data, size_t len)
{
	u16 fc = le_to_host16(hdr->frame_control);
	int prot = !!(fc & WLAN_FC_ISWEP);

	if (qos) {
		u8 ack = (qos[0] & 0x60) >> 5;
		wpa_printf(MSG_MSGDUMP, "BSS DATA: " MACSTR " -> " MACSTR
			   " len=%u%s tid=%u%s%s",
			   MAC2STR(src), MAC2STR(dst), (unsigned int) len,
			   prot ? " Prot" : "", qos[0] & 0x0f,
			   (qos[0] & 0x10) ? " EOSP" : "",
			   ack == 0 ? "" :
			   (ack == 1 ? " NoAck" :
			    (ack == 2 ? " NoExpAck" : " BA")));
	} else {
		wpa_printf(MSG_MSGDUMP, "BSS DATA: " MACSTR " -> " MACSTR
			   " len=%u%s",
			   MAC2STR(src), MAC2STR(dst), (unsigned int) len,
			   prot ? " Prot" : "");
	}

	if (prot)
		rx_data_bss_prot(wt, hdr, hdrlen, qos, dst, src, data, len);
	else {
		const u8 *bssid, *sta_addr, *peer_addr;
		struct wlantest_bss *bss;

		if (fc & WLAN_FC_TODS) {
			bssid = hdr->addr1;
			sta_addr = hdr->addr2;
			peer_addr = NULL;
		} else if (fc & WLAN_FC_FROMDS) {
			bssid = hdr->addr2;
			sta_addr = hdr->addr1;
			peer_addr = NULL;
		} else {
			bssid = hdr->addr3;
			sta_addr = hdr->addr2;
			peer_addr = hdr->addr1;
		}

		bss = bss_get(wt, bssid);
		if (bss) {
			struct wlantest_sta *sta = sta_get(bss, sta_addr);

			if (sta) {
				if (qos) {
					int tid = qos[0] & 0x0f;
					if (fc & WLAN_FC_TODS)
						sta->tx_tid[tid]++;
					else
						sta->rx_tid[tid]++;
				} else {
					if (fc & WLAN_FC_TODS)
						sta->tx_tid[16]++;
					else
						sta->rx_tid[16]++;
				}
			}
		}

		rx_data_process(wt, bss, bssid, sta_addr, dst, src, data, len,
				0, peer_addr, qos);
	}
}


static struct wlantest_tdls * get_tdls(struct wlantest *wt, const u8 *bssid,
				       const u8 *sta1_addr,
				       const u8 *sta2_addr)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *sta1, *sta2;
	struct wlantest_tdls *tdls, *found = NULL;

	bss = bss_find(wt, bssid);
	if (bss == NULL)
		return NULL;
	sta1 = sta_find(bss, sta1_addr);
	if (sta1 == NULL)
		return NULL;
	sta2 = sta_find(bss, sta2_addr);
	if (sta2 == NULL)
		return NULL;

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if ((tdls->init == sta1 && tdls->resp == sta2) ||
		    (tdls->init == sta2 && tdls->resp == sta1)) {
			found = tdls;
			if (tdls->link_up)
				break;
		}
	}

	return found;
}


static void add_direct_link(struct wlantest *wt, const u8 *bssid,
			    const u8 *sta1_addr, const u8 *sta2_addr)
{
	struct wlantest_tdls *tdls;

	tdls = get_tdls(wt, bssid, sta1_addr, sta2_addr);
	if (tdls == NULL)
		return;

	if (tdls->link_up)
		tdls->counters[WLANTEST_TDLS_COUNTER_VALID_DIRECT_LINK]++;
	else
		tdls->counters[WLANTEST_TDLS_COUNTER_INVALID_DIRECT_LINK]++;
}


static void add_ap_path(struct wlantest *wt, const u8 *bssid,
			const u8 *sta1_addr, const u8 *sta2_addr)
{
	struct wlantest_tdls *tdls;

	tdls = get_tdls(wt, bssid, sta1_addr, sta2_addr);
	if (tdls == NULL)
		return;

	if (tdls->link_up)
		tdls->counters[WLANTEST_TDLS_COUNTER_INVALID_AP_PATH]++;
	else
		tdls->counters[WLANTEST_TDLS_COUNTER_VALID_AP_PATH]++;
}


void rx_data(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_hdr *hdr;
	u16 fc, stype;
	size_t hdrlen;
	const u8 *qos = NULL;

	if (len < 24)
		return;

	hdr = (const struct ieee80211_hdr *) data;
	fc = le_to_host16(hdr->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);
	hdrlen = 24;
	if ((fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) ==
	    (WLAN_FC_TODS | WLAN_FC_FROMDS))
		hdrlen += ETH_ALEN;
	if (stype & 0x08) {
		qos = data + hdrlen;
		hdrlen += 2;
	}
	if (len < hdrlen)
		return;
	wt->rx_data++;

	switch (fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
	case 0:
		wpa_printf(MSG_EXCESSIVE, "DATA %s%s%s IBSS DA=" MACSTR " SA="
			   MACSTR " BSSID=" MACSTR,
			   data_stype(WLAN_FC_GET_STYPE(fc)),
			   fc & WLAN_FC_PWRMGT ? " PwrMgt" : "",
			   fc & WLAN_FC_ISWEP ? " Prot" : "",
			   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			   MAC2STR(hdr->addr3));
		add_direct_link(wt, hdr->addr3, hdr->addr1, hdr->addr2);
		rx_data_bss(wt, hdr, hdrlen, qos, hdr->addr1, hdr->addr2,
			    data + hdrlen, len - hdrlen);
		break;
	case WLAN_FC_FROMDS:
		wpa_printf(MSG_EXCESSIVE, "DATA %s%s%s FromDS DA=" MACSTR
			   " BSSID=" MACSTR " SA=" MACSTR,
			   data_stype(WLAN_FC_GET_STYPE(fc)),
			   fc & WLAN_FC_PWRMGT ? " PwrMgt" : "",
			   fc & WLAN_FC_ISWEP ? " Prot" : "",
			   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			   MAC2STR(hdr->addr3));
		add_ap_path(wt, hdr->addr2, hdr->addr1, hdr->addr3);
		rx_data_bss(wt, hdr, hdrlen, qos, hdr->addr1, hdr->addr3,
			    data + hdrlen, len - hdrlen);
		break;
	case WLAN_FC_TODS:
		wpa_printf(MSG_EXCESSIVE, "DATA %s%s%s ToDS BSSID=" MACSTR
			   " SA=" MACSTR " DA=" MACSTR,
			   data_stype(WLAN_FC_GET_STYPE(fc)),
			   fc & WLAN_FC_PWRMGT ? " PwrMgt" : "",
			   fc & WLAN_FC_ISWEP ? " Prot" : "",
			   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			   MAC2STR(hdr->addr3));
		add_ap_path(wt, hdr->addr1, hdr->addr3, hdr->addr2);
		rx_data_bss(wt, hdr, hdrlen, qos, hdr->addr3, hdr->addr2,
			    data + hdrlen, len - hdrlen);
		break;
	case WLAN_FC_TODS | WLAN_FC_FROMDS:
		wpa_printf(MSG_EXCESSIVE, "DATA %s%s%s WDS RA=" MACSTR " TA="
			   MACSTR " DA=" MACSTR " SA=" MACSTR,
			   data_stype(WLAN_FC_GET_STYPE(fc)),
			   fc & WLAN_FC_PWRMGT ? " PwrMgt" : "",
			   fc & WLAN_FC_ISWEP ? " Prot" : "",
			   MAC2STR(hdr->addr1), MAC2STR(hdr->addr2),
			   MAC2STR(hdr->addr3),
			   MAC2STR((const u8 *) (hdr + 1)));
		rx_data_bss(wt, hdr, hdrlen, qos, hdr->addr1, hdr->addr2,
			    data + hdrlen, len - hdrlen);
		break;
	}
}
