/*
 * Received frame processing
 * Copyright (c) 2010-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/crc32.h"
#include "utils/radiotap.h"
#include "utils/radiotap_iter.h"
#include "common/ieee802_11_defs.h"
#include "common/qca-vendor.h"
#include "wlantest.h"


static struct wlantest_sta * rx_get_sta(struct wlantest *wt,
					const struct ieee80211_hdr *hdr,
					size_t len, int *to_ap)
{
	u16 fc;
	const u8 *sta_addr, *bssid;
	struct wlantest_bss *bss;

	*to_ap = 0;
	if (hdr->addr1[0] & 0x01)
		return NULL; /* Ignore group addressed frames */

	fc = le_to_host16(hdr->frame_control);
	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		if (len < 24)
			return NULL;
		bssid = hdr->addr3;
		if (os_memcmp(bssid, hdr->addr2, ETH_ALEN) == 0) {
			sta_addr = hdr->addr1;
			*to_ap = 0;
		} else {
			if (os_memcmp(bssid, hdr->addr1, ETH_ALEN) != 0)
				return NULL; /* Unsupported STA-to-STA frame */
			sta_addr = hdr->addr2;
			*to_ap = 1;
		}
		break;
	case WLAN_FC_TYPE_DATA:
		if (len < 24)
			return NULL;
		switch (fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
		case 0:
			return NULL; /* IBSS not supported */
		case WLAN_FC_FROMDS:
			sta_addr = hdr->addr1;
			bssid = hdr->addr2;
			*to_ap = 0;
			break;
		case WLAN_FC_TODS:
			sta_addr = hdr->addr2;
			bssid = hdr->addr1;
			*to_ap = 1;
			break;
		case WLAN_FC_TODS | WLAN_FC_FROMDS:
			return NULL; /* WDS not supported */
		default:
			return NULL;
		}
		break;
	case WLAN_FC_TYPE_CTRL:
		if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PSPOLL &&
		    len >= 16) {
			sta_addr = hdr->addr2;
			bssid = hdr->addr1;
			*to_ap = 1;
		} else
			return NULL;
		break;
	default:
		return NULL;
	}

	bss = bss_find(wt, bssid);
	if (bss == NULL)
		return NULL;
	return sta_find(bss, sta_addr);
}


static void rx_update_ps(struct wlantest *wt, const struct ieee80211_hdr *hdr,
			 size_t len, struct wlantest_sta *sta, int to_ap)
{
	u16 fc, type, stype;

	if (sta == NULL)
		return;

	fc = le_to_host16(hdr->frame_control);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	if (!to_ap) {
		if (sta->pwrmgt && !sta->pspoll) {
			u16 seq_ctrl = le_to_host16(hdr->seq_ctrl);
			add_note(wt, MSG_DEBUG, "AP " MACSTR " sent a frame "
				 "(%u:%u) to a sleeping STA " MACSTR
				 " (seq=%u)",
				 MAC2STR(sta->bss->bssid),
				 type, stype, MAC2STR(sta->addr),
				 WLAN_GET_SEQ_SEQ(seq_ctrl));
		} else
			sta->pspoll = 0;
		return;
	}

	sta->pspoll = 0;

	if (type == WLAN_FC_TYPE_DATA || type == WLAN_FC_TYPE_MGMT ||
	    (type == WLAN_FC_TYPE_CTRL && stype == WLAN_FC_STYPE_PSPOLL)) {
		/*
		 * In theory, the PS state changes only at the end of the frame
		 * exchange that is ACKed by the AP. However, most cases are
		 * handled with this simpler implementation that does not
		 * maintain state through the frame exchange.
		 */
		if (sta->pwrmgt && !(fc & WLAN_FC_PWRMGT)) {
			add_note(wt, MSG_DEBUG, "STA " MACSTR " woke up from "
				 "sleep", MAC2STR(sta->addr));
			sta->pwrmgt = 0;
		} else if (!sta->pwrmgt && (fc & WLAN_FC_PWRMGT)) {
			add_note(wt, MSG_DEBUG, "STA " MACSTR " went to sleep",
				 MAC2STR(sta->addr));
			sta->pwrmgt = 1;
		}
	}

	if (type == WLAN_FC_TYPE_CTRL && stype == WLAN_FC_STYPE_PSPOLL)
		sta->pspoll = 1;
}


static int rx_duplicate(struct wlantest *wt, const struct ieee80211_hdr *hdr,
			size_t len, struct wlantest_sta *sta, int to_ap)
{
	u16 fc;
	int tid = 16;
	le16 *seq_ctrl;

	if (sta == NULL)
		return 0;

	fc = le_to_host16(hdr->frame_control);
	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_DATA &&
	    (WLAN_FC_GET_STYPE(fc) & 0x08) && len >= 26) {
		const u8 *qos = ((const u8 *) hdr) + 24;
		tid = qos[0] & 0x0f;
	}

	if (to_ap)
		seq_ctrl = &sta->seq_ctrl_to_ap[tid];
	else
		seq_ctrl = &sta->seq_ctrl_to_sta[tid];

	if ((fc & WLAN_FC_RETRY) && hdr->seq_ctrl == *seq_ctrl &&
	    !sta->allow_duplicate) {
		u16 s = le_to_host16(hdr->seq_ctrl);
		add_note(wt, MSG_MSGDUMP, "Ignore duplicated frame (seq=%u "
			 "frag=%u A1=" MACSTR " A2=" MACSTR ")",
			 WLAN_GET_SEQ_SEQ(s), WLAN_GET_SEQ_FRAG(s),
			 MAC2STR(hdr->addr1), MAC2STR(hdr->addr2));
		return 1;
	}

	*seq_ctrl = hdr->seq_ctrl;
	sta->allow_duplicate = 0;

	return 0;
}


static void rx_ack(struct wlantest *wt, const struct ieee80211_hdr *hdr)
{
	struct ieee80211_hdr *last = (struct ieee80211_hdr *) wt->last_hdr;
	u16 fc;

	if (wt->last_len < 24 || (last->addr1[0] & 0x01) ||
	    os_memcmp(hdr->addr1, last->addr2, ETH_ALEN) != 0) {
		add_note(wt, MSG_MSGDUMP, "Unknown Ack frame (previous frame "
			 "not seen)");
		return;
	}

	/* Ack to the previous frame */
	fc = le_to_host16(last->frame_control);
	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT)
		rx_mgmt_ack(wt, last);
}


static void rx_frame(struct wlantest *wt, const u8 *data, size_t len)
{
	const struct ieee80211_hdr *hdr;
	u16 fc;
	struct wlantest_sta *sta;
	int to_ap;

	wpa_hexdump(MSG_EXCESSIVE, "RX frame", data, len);
	if (len < 2)
		return;

	hdr = (const struct ieee80211_hdr *) data;
	fc = le_to_host16(hdr->frame_control);
	if (fc & WLAN_FC_PVER) {
		wpa_printf(MSG_DEBUG, "Drop RX frame with unexpected pver=%d",
			   fc & WLAN_FC_PVER);
		return;
	}

	sta = rx_get_sta(wt, hdr, len, &to_ap);

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		if (len < 24)
			break;
		if (rx_duplicate(wt, hdr, len, sta, to_ap))
			break;
		rx_update_ps(wt, hdr, len, sta, to_ap);
		rx_mgmt(wt, data, len);
		break;
	case WLAN_FC_TYPE_CTRL:
		if (len < 10)
			break;
		wt->rx_ctrl++;
		rx_update_ps(wt, hdr, len, sta, to_ap);
		if (WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACK)
			rx_ack(wt, hdr);
		break;
	case WLAN_FC_TYPE_DATA:
		if (len < 24)
			break;
		if (rx_duplicate(wt, hdr, len, sta, to_ap))
			break;
		rx_update_ps(wt, hdr, len, sta, to_ap);
		rx_data(wt, data, len);
		break;
	default:
		wpa_printf(MSG_DEBUG, "Drop RX frame with unexpected type %d",
			   WLAN_FC_GET_TYPE(fc));
		break;
	}

	os_memcpy(wt->last_hdr, data, len > sizeof(wt->last_hdr) ?
		  sizeof(wt->last_hdr) : len);
	wt->last_len = len;
}


static void tx_status(struct wlantest *wt, const u8 *data, size_t len, int ack)
{
	wpa_printf(MSG_DEBUG, "TX status: ack=%d", ack);
	wpa_hexdump(MSG_EXCESSIVE, "TX status frame", data, len);
}


static int check_fcs(const u8 *frame, size_t frame_len, const u8 *fcs)
{
	if (WPA_GET_LE32(fcs) != crc32(frame, frame_len))
		return -1;
	return 0;
}


void wlantest_process(struct wlantest *wt, const u8 *data, size_t len)
{
	struct ieee80211_radiotap_iterator iter;
	int ret;
	int rxflags = 0, txflags = 0, failed = 0, fcs = 0;
	const u8 *frame, *fcspos;
	size_t frame_len;

	if (wt->ethernet)
		return;

	wpa_hexdump(MSG_EXCESSIVE, "Process data", data, len);

	if (ieee80211_radiotap_iterator_init(&iter, (void *) data, len, NULL)) {
		add_note(wt, MSG_INFO, "Invalid radiotap frame");
		return;
	}

	for (;;) {
		ret = ieee80211_radiotap_iterator_next(&iter);
		wpa_printf(MSG_EXCESSIVE, "radiotap iter: %d "
			   "this_arg_index=%d", ret, iter.this_arg_index);
		if (ret == -ENOENT)
			break;
		if (ret) {
			add_note(wt, MSG_INFO, "Invalid radiotap header: %d",
				 ret);
			return;
		}
		switch (iter.this_arg_index) {
		case IEEE80211_RADIOTAP_FLAGS:
			if (*iter.this_arg & IEEE80211_RADIOTAP_F_FCS)
				fcs = 1;
			break;
		case IEEE80211_RADIOTAP_RX_FLAGS:
			rxflags = 1;
			break;
		case IEEE80211_RADIOTAP_TX_FLAGS:
			txflags = 1;
			failed = le_to_host16((*(u16 *) iter.this_arg)) &
				IEEE80211_RADIOTAP_F_TX_FAIL;
			break;
		case IEEE80211_RADIOTAP_VENDOR_NAMESPACE:
			if (WPA_GET_BE24(iter.this_arg) == OUI_QCA &&
			    iter.this_arg[3] == QCA_RADIOTAP_VID_WLANTEST) {
				add_note(wt, MSG_DEBUG,
					 "Skip frame inserted by wlantest");
				return;
			}
		}
	}

	frame = data + iter._max_length;
	frame_len = len - iter._max_length;

	if (fcs && frame_len >= 4) {
		frame_len -= 4;
		fcspos = frame + frame_len;
		if (check_fcs(frame, frame_len, fcspos) < 0) {
			add_note(wt, MSG_EXCESSIVE, "Drop RX frame with "
				 "invalid FCS");
			wt->fcs_error++;
			return;
		}
	}

	if (rxflags && txflags)
		return;
	if (!txflags)
		rx_frame(wt, frame, frame_len);
	else {
		add_note(wt, MSG_EXCESSIVE, "TX status - process as RX of "
			 "local frame");
		tx_status(wt, frame, frame_len, !failed);
		/* Process as RX frame to support local monitor interface */
		rx_frame(wt, frame, frame_len);
	}
}


void wlantest_process_prism(struct wlantest *wt, const u8 *data, size_t len)
{
	int fcs = 0;
	const u8 *frame, *fcspos;
	size_t frame_len;
	u32 hdrlen;

	wpa_hexdump(MSG_EXCESSIVE, "Process data", data, len);

	if (len < 8)
		return;
	hdrlen = WPA_GET_LE32(data + 4);

	if (len < hdrlen) {
		wpa_printf(MSG_INFO, "Too short frame to include prism "
			   "header");
		return;
	}

	frame = data + hdrlen;
	frame_len = len - hdrlen;
	fcs = 1;

	if (fcs && frame_len >= 4) {
		frame_len -= 4;
		fcspos = frame + frame_len;
		if (check_fcs(frame, frame_len, fcspos) < 0) {
			add_note(wt, MSG_EXCESSIVE, "Drop RX frame with "
				 "invalid FCS");
			wt->fcs_error++;
			return;
		}
	}

	rx_frame(wt, frame, frame_len);
}


void wlantest_process_80211(struct wlantest *wt, const u8 *data, size_t len)
{
	wpa_hexdump(MSG_EXCESSIVE, "Process data", data, len);

	if (wt->assume_fcs && len >= 4) {
		const u8 *fcspos;

		len -= 4;
		fcspos = data + len;
		if (check_fcs(data, len, fcspos) < 0) {
			add_note(wt, MSG_EXCESSIVE, "Drop RX frame with "
				 "invalid FCS");
			wt->fcs_error++;
			return;
		}
	}

	rx_frame(wt, data, len);
}
