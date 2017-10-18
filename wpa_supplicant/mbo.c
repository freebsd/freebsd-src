/*
 * wpa_supplicant - MBO
 *
 * Copyright(c) 2015 Intel Deutschland GmbH
 * Contact Information:
 * Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/gas.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"
#include "scan.h"

/* type + length + oui + oui type */
#define MBO_IE_HEADER 6


static int wpas_mbo_validate_non_pref_chan(u8 oper_class, u8 chan, u8 reason)
{
	if (reason > MBO_NON_PREF_CHAN_REASON_INT_INTERFERENCE)
		return -1;

	/* Only checking the validity of the channel and oper_class */
	if (ieee80211_chan_to_freq(NULL, oper_class, chan) == -1)
		return -1;

	return 0;
}


const u8 * wpas_mbo_get_bss_attr(struct wpa_bss *bss, enum mbo_attr_id attr)
{
	const u8 *mbo, *end;

	if (!bss)
		return NULL;

	mbo = wpa_bss_get_vendor_ie(bss, MBO_IE_VENDOR_TYPE);
	if (!mbo)
		return NULL;

	end = mbo + 2 + mbo[1];
	mbo += MBO_IE_HEADER;

	return get_ie(mbo, end - mbo, attr);
}


static void wpas_mbo_non_pref_chan_attr_body(struct wpa_supplicant *wpa_s,
					     struct wpabuf *mbo,
					     u8 start, u8 end)
{
	u8 i;

	wpabuf_put_u8(mbo, wpa_s->non_pref_chan[start].oper_class);

	for (i = start; i < end; i++)
		wpabuf_put_u8(mbo, wpa_s->non_pref_chan[i].chan);

	wpabuf_put_u8(mbo, wpa_s->non_pref_chan[start].preference);
	wpabuf_put_u8(mbo, wpa_s->non_pref_chan[start].reason);
}


static void wpas_mbo_non_pref_chan_attr(struct wpa_supplicant *wpa_s,
					struct wpabuf *mbo, u8 start, u8 end)
{
	size_t size = end - start + 3;

	if (size + 2 > wpabuf_tailroom(mbo))
		return;

	wpabuf_put_u8(mbo, MBO_ATTR_ID_NON_PREF_CHAN_REPORT);
	wpabuf_put_u8(mbo, size); /* Length */

	wpas_mbo_non_pref_chan_attr_body(wpa_s, mbo, start, end);
}


static void wpas_mbo_non_pref_chan_subelem_hdr(struct wpabuf *mbo, u8 len)
{
	wpabuf_put_u8(mbo, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(mbo, len); /* Length */
	wpabuf_put_be24(mbo, OUI_WFA);
	wpabuf_put_u8(mbo, MBO_ATTR_ID_NON_PREF_CHAN_REPORT);
}


static void wpas_mbo_non_pref_chan_subelement(struct wpa_supplicant *wpa_s,
					      struct wpabuf *mbo, u8 start,
					      u8 end)
{
	size_t size = end - start + 7;

	if (size + 2 > wpabuf_tailroom(mbo))
		return;

	wpas_mbo_non_pref_chan_subelem_hdr(mbo, size);
	wpas_mbo_non_pref_chan_attr_body(wpa_s, mbo, start, end);
}


static void wpas_mbo_non_pref_chan_attrs(struct wpa_supplicant *wpa_s,
					 struct wpabuf *mbo, int subelement)
{
	u8 i, start = 0;
	struct wpa_mbo_non_pref_channel *start_pref;

	if (!wpa_s->non_pref_chan || !wpa_s->non_pref_chan_num) {
		if (subelement)
			wpas_mbo_non_pref_chan_subelem_hdr(mbo, 4);
		return;
	}
	start_pref = &wpa_s->non_pref_chan[0];

	for (i = 1; i <= wpa_s->non_pref_chan_num; i++) {
		struct wpa_mbo_non_pref_channel *non_pref = NULL;

		if (i < wpa_s->non_pref_chan_num)
			non_pref = &wpa_s->non_pref_chan[i];
		if (!non_pref ||
		    non_pref->oper_class != start_pref->oper_class ||
		    non_pref->reason != start_pref->reason ||
		    non_pref->preference != start_pref->preference) {
			if (subelement)
				wpas_mbo_non_pref_chan_subelement(wpa_s, mbo,
								  start, i);
			else
				wpas_mbo_non_pref_chan_attr(wpa_s, mbo, start,
							    i);

			if (!non_pref)
				return;

			start = i;
			start_pref = non_pref;
		}
	}
}


int wpas_mbo_ie(struct wpa_supplicant *wpa_s, u8 *buf, size_t len)
{
	struct wpabuf *mbo;
	int res;

	if (len < MBO_IE_HEADER + 3 + 7)
		return 0;

	/* Leave room for the MBO IE header */
	mbo = wpabuf_alloc(len - MBO_IE_HEADER);
	if (!mbo)
		return 0;

	/* Add non-preferred channels attribute */
	wpas_mbo_non_pref_chan_attrs(wpa_s, mbo, 0);

	/*
	 * Send cellular capabilities attribute even if AP does not advertise
	 * cellular capabilities.
	 */
	wpabuf_put_u8(mbo, MBO_ATTR_ID_CELL_DATA_CAPA);
	wpabuf_put_u8(mbo, 1);
	wpabuf_put_u8(mbo, wpa_s->conf->mbo_cell_capa);

	res = mbo_add_ie(buf, len, wpabuf_head_u8(mbo), wpabuf_len(mbo));
	if (!res)
		wpa_printf(MSG_ERROR, "Failed to add MBO IE");

	wpabuf_free(mbo);
	return res;
}


static void wpas_mbo_send_wnm_notification(struct wpa_supplicant *wpa_s,
					   const u8 *data, size_t len)
{
	struct wpabuf *buf;
	int res;

	/*
	 * Send WNM-Notification Request frame only in case of a change in
	 * non-preferred channels list during association, if the AP supports
	 * MBO.
	 */
	if (wpa_s->wpa_state != WPA_COMPLETED || !wpa_s->current_bss ||
	    !wpa_bss_get_vendor_ie(wpa_s->current_bss, MBO_IE_VENDOR_TYPE))
		return;

	buf = wpabuf_alloc(4 + len);
	if (!buf)
		return;

	wpabuf_put_u8(buf, WLAN_ACTION_WNM);
	wpabuf_put_u8(buf, WNM_NOTIFICATION_REQ);
	wpa_s->mbo_wnm_token++;
	if (wpa_s->mbo_wnm_token == 0)
		wpa_s->mbo_wnm_token++;
	wpabuf_put_u8(buf, wpa_s->mbo_wnm_token);
	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC); /* Type */

	wpabuf_put_data(buf, data, len);

	res = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (res < 0)
		wpa_printf(MSG_DEBUG,
			   "Failed to send WNM-Notification Request frame with non-preferred channel list");

	wpabuf_free(buf);
}


static void wpas_mbo_non_pref_chan_changed(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(512);
	if (!buf)
		return;

	wpas_mbo_non_pref_chan_attrs(wpa_s, buf, 1);
	wpas_mbo_send_wnm_notification(wpa_s, wpabuf_head_u8(buf),
				       wpabuf_len(buf));
	wpabuf_free(buf);
}


static int wpa_non_pref_chan_is_eq(struct wpa_mbo_non_pref_channel *a,
				   struct wpa_mbo_non_pref_channel *b)
{
	return a->oper_class == b->oper_class && a->chan == b->chan;
}


/*
 * wpa_non_pref_chan_cmp - Compare two channels for sorting
 *
 * In MBO IE non-preferred channel subelement we can put many channels in an
 * attribute if they are in the same operating class and have the same
 * preference and reason. To make it easy for the functions that build
 * the IE attributes and WNM Request subelements, save the channels sorted
 * by their oper_class and reason.
 */
static int wpa_non_pref_chan_cmp(const void *_a, const void *_b)
{
	const struct wpa_mbo_non_pref_channel *a = _a, *b = _b;

	if (a->oper_class != b->oper_class)
		return a->oper_class - b->oper_class;
	if (a->reason != b->reason)
		return a->reason - b->reason;
	return a->preference - b->preference;
}


int wpas_mbo_update_non_pref_chan(struct wpa_supplicant *wpa_s,
				  const char *non_pref_chan)
{
	char *cmd, *token, *context = NULL;
	struct wpa_mbo_non_pref_channel *chans = NULL, *tmp_chans;
	size_t num = 0, size = 0;
	unsigned i;

	wpa_printf(MSG_DEBUG, "MBO: Update non-preferred channels, non_pref_chan=%s",
		   non_pref_chan ? non_pref_chan : "N/A");

	/*
	 * The shortest channel configuration is 10 characters - commas, 3
	 * colons, and 4 values that one of them (oper_class) is 2 digits or
	 * more.
	 */
	if (!non_pref_chan || os_strlen(non_pref_chan) < 10)
		goto update;

	cmd = os_strdup(non_pref_chan);
	if (!cmd)
		return -1;

	while ((token = str_token(cmd, " ", &context))) {
		struct wpa_mbo_non_pref_channel *chan;
		int ret;
		unsigned int _oper_class;
		unsigned int _chan;
		unsigned int _preference;
		unsigned int _reason;

		if (num == size) {
			size = size ? size * 2 : 1;
			tmp_chans = os_realloc_array(chans, size,
						     sizeof(*chans));
			if (!tmp_chans) {
				wpa_printf(MSG_ERROR,
					   "Couldn't reallocate non_pref_chan");
				goto fail;
			}
			chans = tmp_chans;
		}

		chan = &chans[num];

		ret = sscanf(token, "%u:%u:%u:%u", &_oper_class,
			     &_chan, &_preference, &_reason);
		if (ret != 4 ||
		    _oper_class > 255 || _chan > 255 ||
		    _preference > 255 || _reason > 65535 ) {
			wpa_printf(MSG_ERROR, "Invalid non-pref chan input %s",
				   token);
			goto fail;
		}
		chan->oper_class = _oper_class;
		chan->chan = _chan;
		chan->preference = _preference;
		chan->reason = _reason;

		if (wpas_mbo_validate_non_pref_chan(chan->oper_class,
						    chan->chan, chan->reason)) {
			wpa_printf(MSG_ERROR,
				   "Invalid non_pref_chan: oper class %d chan %d reason %d",
				   chan->oper_class, chan->chan, chan->reason);
			goto fail;
		}

		for (i = 0; i < num; i++)
			if (wpa_non_pref_chan_is_eq(chan, &chans[i]))
				break;
		if (i != num) {
			wpa_printf(MSG_ERROR,
				   "oper class %d chan %d is duplicated",
				   chan->oper_class, chan->chan);
			goto fail;
		}

		num++;
	}

	os_free(cmd);

	if (chans) {
		qsort(chans, num, sizeof(struct wpa_mbo_non_pref_channel),
		      wpa_non_pref_chan_cmp);
	}

update:
	os_free(wpa_s->non_pref_chan);
	wpa_s->non_pref_chan = chans;
	wpa_s->non_pref_chan_num = num;
	wpas_mbo_non_pref_chan_changed(wpa_s);

	return 0;

fail:
	os_free(chans);
	os_free(cmd);
	return -1;
}


void wpas_mbo_scan_ie(struct wpa_supplicant *wpa_s, struct wpabuf *ie)
{
	wpabuf_put_u8(ie, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(ie, 7);
	wpabuf_put_be24(ie, OUI_WFA);
	wpabuf_put_u8(ie, MBO_OUI_TYPE);

	wpabuf_put_u8(ie, MBO_ATTR_ID_CELL_DATA_CAPA);
	wpabuf_put_u8(ie, 1);
	wpabuf_put_u8(ie, wpa_s->conf->mbo_cell_capa);
}


enum chan_allowed {
	NOT_ALLOWED, ALLOWED
};

static enum chan_allowed allow_channel(struct hostapd_hw_modes *mode, u8 chan,
				       unsigned int *flags)
{
	int i;

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == chan)
			break;
	}

	if (i == mode->num_channels ||
	    (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED))
		return NOT_ALLOWED;

	if (flags)
		*flags = mode->channels[i].flag;

	return ALLOWED;
}


static int get_center_80mhz(struct hostapd_hw_modes *mode, u8 channel)
{
	u8 center_channels[] = {42, 58, 106, 122, 138, 155};
	size_t i;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	for (i = 0; i < ARRAY_SIZE(center_channels); i++) {
		/*
		 * In 80 MHz, the bandwidth "spans" 12 channels (e.g., 36-48),
		 * so the center channel is 6 channels away from the start/end.
		 */
		if (channel >= center_channels[i] - 6 &&
		    channel <= center_channels[i] + 6)
			return center_channels[i];
	}

	return 0;
}


static enum chan_allowed verify_80mhz(struct hostapd_hw_modes *mode, u8 channel)
{
	u8 center_chan;
	unsigned int i;

	center_chan = get_center_80mhz(mode, channel);
	if (!center_chan)
		return NOT_ALLOWED;

	/* check all the channels are available */
	for (i = 0; i < 4; i++) {
		unsigned int flags;
		u8 adj_chan = center_chan - 6 + i * 4;

		if (allow_channel(mode, adj_chan, &flags) == NOT_ALLOWED)
			return NOT_ALLOWED;

		if ((i == 0 && !(flags & HOSTAPD_CHAN_VHT_10_70)) ||
		    (i == 1 && !(flags & HOSTAPD_CHAN_VHT_30_50)) ||
		    (i == 2 && !(flags & HOSTAPD_CHAN_VHT_50_30)) ||
		    (i == 3 && !(flags & HOSTAPD_CHAN_VHT_70_10)))
			return NOT_ALLOWED;
	}

	return ALLOWED;
}


static int get_center_160mhz(struct hostapd_hw_modes *mode, u8 channel)
{
	u8 center_channels[] = { 50, 114 };
	unsigned int i;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	for (i = 0; i < ARRAY_SIZE(center_channels); i++) {
		/*
		 * In 160 MHz, the bandwidth "spans" 28 channels (e.g., 36-64),
		 * so the center channel is 14 channels away from the start/end.
		 */
		if (channel >= center_channels[i] - 14 &&
		    channel <= center_channels[i] + 14)
			return center_channels[i];
	}

	return 0;
}


static enum chan_allowed verify_160mhz(struct hostapd_hw_modes *mode,
				       u8 channel)
{
	u8 center_chan;
	unsigned int i;

	center_chan = get_center_160mhz(mode, channel);
	if (!center_chan)
		return NOT_ALLOWED;

	/* Check all the channels are available */
	for (i = 0; i < 8; i++) {
		unsigned int flags;
		u8 adj_chan = center_chan - 14 + i * 4;

		if (allow_channel(mode, adj_chan, &flags) == NOT_ALLOWED)
			return NOT_ALLOWED;

		if ((i == 0 && !(flags & HOSTAPD_CHAN_VHT_10_150)) ||
		    (i == 1 && !(flags & HOSTAPD_CHAN_VHT_30_130)) ||
		    (i == 2 && !(flags & HOSTAPD_CHAN_VHT_50_110)) ||
		    (i == 3 && !(flags & HOSTAPD_CHAN_VHT_70_90)) ||
		    (i == 4 && !(flags & HOSTAPD_CHAN_VHT_90_70)) ||
		    (i == 5 && !(flags & HOSTAPD_CHAN_VHT_110_50)) ||
		    (i == 6 && !(flags & HOSTAPD_CHAN_VHT_130_30)) ||
		    (i == 7 && !(flags & HOSTAPD_CHAN_VHT_150_10)))
			return NOT_ALLOWED;
	}

	return ALLOWED;
}


static enum chan_allowed verify_channel(struct hostapd_hw_modes *mode,
					u8 channel, u8 bw)
{
	unsigned int flag = 0;
	enum chan_allowed res, res2;

	res2 = res = allow_channel(mode, channel, &flag);
	if (bw == BW40MINUS) {
		if (!(flag & HOSTAPD_CHAN_HT40MINUS))
			return NOT_ALLOWED;
		res2 = allow_channel(mode, channel - 4, NULL);
	} else if (bw == BW40PLUS) {
		if (!(flag & HOSTAPD_CHAN_HT40PLUS))
			return NOT_ALLOWED;
		res2 = allow_channel(mode, channel + 4, NULL);
	} else if (bw == BW80) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 80 MHz specific version.
		 */
		res2 = res = verify_80mhz(mode, channel);
	} else if (bw == BW160) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 160 MHz specific version.
		 */
		res2 = res = verify_160mhz(mode, channel);
	} else if (bw == BW80P80) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 80 MHz specific version.
		 */
		res2 = res = verify_80mhz(mode, channel);
	}

	if (res == NOT_ALLOWED || res2 == NOT_ALLOWED)
		return NOT_ALLOWED;

	return ALLOWED;
}


static int wpas_op_class_supported(struct wpa_supplicant *wpa_s,
				   const struct oper_class_map *op_class)
{
	int chan;
	size_t i;
	struct hostapd_hw_modes *mode;
	int found;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, op_class->mode);
	if (!mode)
		return 0;

	if (op_class->op_class == 128) {
		u8 channels[] = { 42, 58, 106, 122, 138, 155 };

		for (i = 0; i < ARRAY_SIZE(channels); i++) {
			if (verify_channel(mode, channels[i], op_class->bw) ==
			    ALLOWED)
				return 1;
		}

		return 0;
	}

	if (op_class->op_class == 129) {
		/* Check if either 160 MHz channels is allowed */
		return verify_channel(mode, 50, op_class->bw) == ALLOWED ||
			verify_channel(mode, 114, op_class->bw) == ALLOWED;
	}

	if (op_class->op_class == 130) {
		/* Need at least two non-contiguous 80 MHz segments */
		found = 0;

		if (verify_channel(mode, 42, op_class->bw) == ALLOWED ||
		    verify_channel(mode, 58, op_class->bw) == ALLOWED)
			found++;
		if (verify_channel(mode, 106, op_class->bw) == ALLOWED ||
		    verify_channel(mode, 122, op_class->bw) == ALLOWED ||
		    verify_channel(mode, 138, op_class->bw) == ALLOWED)
			found++;
		if (verify_channel(mode, 106, op_class->bw) == ALLOWED &&
		    verify_channel(mode, 138, op_class->bw) == ALLOWED)
			found++;
		if (verify_channel(mode, 155, op_class->bw) == ALLOWED)
			found++;

		if (found >= 2)
			return 1;

		return 0;
	}

	found = 0;
	for (chan = op_class->min_chan; chan <= op_class->max_chan;
	     chan += op_class->inc) {
		if (verify_channel(mode, chan, op_class->bw) == ALLOWED) {
			found = 1;
			break;
		}
	}

	return found;
}


int wpas_mbo_supp_op_class_ie(struct wpa_supplicant *wpa_s, int freq, u8 *pos,
			      size_t len)
{
	struct wpabuf *buf;
	u8 op, current, chan;
	u8 *ie_len;
	int res;

	/*
	 * Assume 20 MHz channel for now.
	 * TODO: Use the secondary channel and VHT channel width that will be
	 * used after association.
	 */
	if (ieee80211_freq_to_channel_ext(freq, 0, VHT_CHANWIDTH_USE_HT,
					  &current, &chan) == NUM_HOSTAPD_MODES)
		return 0;

	/*
	 * Need 3 bytes for EID, length, and current operating class, plus
	 * 1 byte for every other supported operating class.
	 */
	buf = wpabuf_alloc(global_op_class_size + 3);
	if (!buf)
		return 0;

	wpabuf_put_u8(buf, WLAN_EID_SUPPORTED_OPERATING_CLASSES);
	/* Will set the length later, putting a placeholder */
	ie_len = wpabuf_put(buf, 1);
	wpabuf_put_u8(buf, current);

	for (op = 0; global_op_class[op].op_class; op++) {
		if (wpas_op_class_supported(wpa_s, &global_op_class[op]))
			wpabuf_put_u8(buf, global_op_class[op].op_class);
	}

	*ie_len = wpabuf_len(buf) - 2;
	if (*ie_len < 2 || wpabuf_len(buf) > len) {
		wpa_printf(MSG_ERROR,
			   "Failed to add supported operating classes IE");
		res = 0;
	} else {
		os_memcpy(pos, wpabuf_head(buf), wpabuf_len(buf));
		res = wpabuf_len(buf);
		wpa_hexdump_buf(MSG_DEBUG,
				"MBO: Added supported operating classes IE",
				buf);
	}

	wpabuf_free(buf);
	return res;
}


void wpas_mbo_ie_trans_req(struct wpa_supplicant *wpa_s, const u8 *mbo_ie,
			   size_t len)
{
	const u8 *pos, *cell_pref = NULL, *reason = NULL;
	u8 id, elen;
	u16 disallowed_sec = 0;

	if (len <= 4 || WPA_GET_BE24(mbo_ie) != OUI_WFA ||
	    mbo_ie[3] != MBO_OUI_TYPE)
		return;

	pos = mbo_ie + 4;
	len -= 4;

	while (len >= 2) {
		id = *pos++;
		elen = *pos++;
		len -= 2;

		if (elen > len)
			goto fail;

		switch (id) {
		case MBO_ATTR_ID_CELL_DATA_PREF:
			if (elen != 1)
				goto fail;

			if (wpa_s->conf->mbo_cell_capa ==
			    MBO_CELL_CAPA_AVAILABLE)
				cell_pref = pos;
			else
				wpa_printf(MSG_DEBUG,
					   "MBO: Station does not support Cellular data connection");
			break;
		case MBO_ATTR_ID_TRANSITION_REASON:
			if (elen != 1)
				goto fail;

			reason = pos;
			break;
		case MBO_ATTR_ID_ASSOC_RETRY_DELAY:
			if (elen != 2)
				goto fail;

			if (wpa_s->wnm_mode &
			    WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED) {
				wpa_printf(MSG_DEBUG,
					   "MBO: Unexpected association retry delay, BSS is terminating");
				goto fail;
			} else if (wpa_s->wnm_mode &
				   WNM_BSS_TM_REQ_DISASSOC_IMMINENT) {
				disallowed_sec = WPA_GET_LE16(pos);
			} else {
				wpa_printf(MSG_DEBUG,
					   "MBO: Association retry delay attribute not in disassoc imminent mode");
			}

			break;
		case MBO_ATTR_ID_AP_CAPA_IND:
		case MBO_ATTR_ID_NON_PREF_CHAN_REPORT:
		case MBO_ATTR_ID_CELL_DATA_CAPA:
		case MBO_ATTR_ID_ASSOC_DISALLOW:
		case MBO_ATTR_ID_TRANSITION_REJECT_REASON:
			wpa_printf(MSG_DEBUG,
				   "MBO: Attribute %d should not be included in BTM Request frame",
				   id);
			break;
		default:
			wpa_printf(MSG_DEBUG, "MBO: Unknown attribute id %u",
				   id);
			return;
		}

		pos += elen;
		len -= elen;
	}

	if (cell_pref)
		wpa_msg(wpa_s, MSG_INFO, MBO_CELL_PREFERENCE "preference=%u",
			*cell_pref);

	if (reason)
		wpa_msg(wpa_s, MSG_INFO, MBO_TRANSITION_REASON "reason=%u",
			*reason);

	if (disallowed_sec && wpa_s->current_bss)
		wpa_bss_tmp_disallow(wpa_s, wpa_s->current_bss->bssid,
				     disallowed_sec);

	return;
fail:
	wpa_printf(MSG_DEBUG, "MBO IE parsing failed (id=%u len=%u left=%zu)",
		   id, elen, len);
}


size_t wpas_mbo_ie_bss_trans_reject(struct wpa_supplicant *wpa_s, u8 *pos,
				    size_t len,
				    enum mbo_transition_reject_reason reason)
{
	u8 reject_attr[3];

	reject_attr[0] = MBO_ATTR_ID_TRANSITION_REJECT_REASON;
	reject_attr[1] = 1;
	reject_attr[2] = reason;

	return mbo_add_ie(pos, len, reject_attr, sizeof(reject_attr));
}


void wpas_mbo_update_cell_capa(struct wpa_supplicant *wpa_s, u8 mbo_cell_capa)
{
	u8 cell_capa[7];

	if (wpa_s->conf->mbo_cell_capa == mbo_cell_capa) {
		wpa_printf(MSG_DEBUG,
			   "MBO: Cellular capability already set to %u",
			   mbo_cell_capa);
		return;
	}

	wpa_s->conf->mbo_cell_capa = mbo_cell_capa;

	cell_capa[0] = WLAN_EID_VENDOR_SPECIFIC;
	cell_capa[1] = 5; /* Length */
	WPA_PUT_BE24(cell_capa + 2, OUI_WFA);
	cell_capa[5] = MBO_ATTR_ID_CELL_DATA_CAPA;
	cell_capa[6] = mbo_cell_capa;

	wpas_mbo_send_wnm_notification(wpa_s, cell_capa, 7);
	wpa_supplicant_set_default_scan_ies(wpa_s);
}


struct wpabuf * mbo_build_anqp_buf(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss)
{
	struct wpabuf *anqp_buf;
	u8 *len_pos;

	if (!wpa_bss_get_vendor_ie(bss, MBO_IE_VENDOR_TYPE)) {
		wpa_printf(MSG_INFO, "MBO: " MACSTR
			   " does not support MBO - cannot request MBO ANQP elements from it",
			   MAC2STR(bss->bssid));
		return NULL;
	}

	anqp_buf = wpabuf_alloc(10);
	if (!anqp_buf)
		return NULL;

	len_pos = gas_anqp_add_element(anqp_buf, ANQP_VENDOR_SPECIFIC);
	wpabuf_put_be24(anqp_buf, OUI_WFA);
	wpabuf_put_u8(anqp_buf, MBO_ANQP_OUI_TYPE);

	wpabuf_put_u8(anqp_buf, MBO_ANQP_SUBTYPE_CELL_CONN_PREF);
	gas_anqp_set_element_len(anqp_buf, len_pos);

	return anqp_buf;
}
