/*
 * Operating Channel Validation (OCV)
 * Copyright (c) 2018, Mathy Vanhoef
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "drivers/driver.h"
#include "common/ieee802_11_common.h"
#include "ocv.h"

/**
 * Caller of OCV functionality may use various debug output functions, so store
 * the error here and let the caller use an appropriate debug output function.
 */
char ocv_errorstr[256];


int ocv_derive_all_parameters(struct oci_info *oci)
{
	const struct oper_class_map *op_class_map;

	oci->freq = ieee80211_chan_to_freq(NULL, oci->op_class, oci->channel);
	if (oci->freq < 0) {
		wpa_printf(MSG_INFO,
			   "Error interpreting OCI: unrecognized opclass/channel pair (%d/%d)",
			   oci->op_class, oci->channel);
		return -1;
	}

	op_class_map = get_oper_class(NULL, oci->op_class);
	if (!op_class_map) {
		wpa_printf(MSG_INFO,
			   "Error interpreting OCI: Unrecognized opclass (%d)",
			   oci->op_class);
		return -1;
	}

	oci->chanwidth = oper_class_bw_to_int(op_class_map);
	oci->sec_channel = 0;
	if (op_class_map->bw == BW40PLUS)
		oci->sec_channel = 1;
	else if (op_class_map->bw == BW40MINUS)
		oci->sec_channel = -1;

	return 0;
}


int ocv_insert_oci(struct wpa_channel_info *ci, u8 **argpos)
{
	u8 op_class, channel;
	u8 *pos = *argpos;

	if (ieee80211_chaninfo_to_channel(ci->frequency, ci->chanwidth,
					  ci->sec_channel,
					  &op_class, &channel) < 0) {
		wpa_printf(MSG_WARNING,
			   "Cannot determine operating class and channel for OCI element");
		return -1;
	}

	*pos++ = op_class;
	*pos++ = channel;
	*pos++ = ci->seg1_idx;

	*argpos = pos;
	return 0;
}


int ocv_insert_oci_kde(struct wpa_channel_info *ci, u8 **argpos)
{
	u8 *pos = *argpos;

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = RSN_SELECTOR_LEN + 3;
	RSN_SELECTOR_PUT(pos, RSN_KEY_DATA_OCI);
	pos += RSN_SELECTOR_LEN;

	*argpos = pos;
	return ocv_insert_oci(ci, argpos);
}


int ocv_insert_extended_oci(struct wpa_channel_info *ci, u8 *pos)
{
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + OCV_OCI_LEN;
	*pos++ = WLAN_EID_EXT_OCV_OCI;
	return ocv_insert_oci(ci, &pos);
}


int ocv_verify_tx_params(const u8 *oci_ie, size_t oci_ie_len,
			 struct wpa_channel_info *ci, int tx_chanwidth,
			 int tx_seg1_idx)
{
	struct oci_info oci;

	if (!oci_ie) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: did not receive mandatory OCI");
		return -1;
	}

	if (oci_ie_len != 3) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: received OCI of unexpected length (%d)",
			    (int) oci_ie_len);
		return -1;
	}

	os_memset(&oci, 0, sizeof(oci));
	oci.op_class = oci_ie[0];
	oci.channel = oci_ie[1];
	oci.seg1_idx = oci_ie[2];
	if (ocv_derive_all_parameters(&oci) != 0) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: unable to interpret received OCI");
		return -1;
	}

	/* Primary frequency used to send frames to STA must match the STA's */
	if ((int) ci->frequency != oci.freq) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: primary channel mismatch in received OCI (we use %d but receiver is using %d)",
			    ci->frequency, oci.freq);
		return -1;
	}

	/* We shouldn't transmit with a higher bandwidth than the STA supports
	 */
	if (tx_chanwidth > oci.chanwidth) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: channel bandwidth mismatch in received OCI (we use %d but receiver only supports %d)",
			    tx_chanwidth, oci.chanwidth);
		return -1;
	}

	/*
	 * Secondary channel only needs be checked for 40 MHz in the 2.4 GHz
	 * band. In the 5 GHz band it's verified through the primary frequency.
	 * Note that the field ci->sec_channel is only filled in when we use
	 * 40 MHz.
	 */
	if (tx_chanwidth == 40 && ci->frequency < 2500 &&
	    ci->sec_channel != oci.sec_channel) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: secondary channel mismatch in received OCI (we use %d but receiver is using %d)",
			    ci->sec_channel, oci.sec_channel);
		return -1;
	}

	/*
	 * When using a 160 or 80+80 MHz channel to transmit, verify that we use
	 * the same segments as the receiver by comparing frequency segment 1.
	 */
	if ((ci->chanwidth == CHAN_WIDTH_160 ||
	     ci->chanwidth == CHAN_WIDTH_80P80) &&
	    tx_seg1_idx != oci.seg1_idx) {
		os_snprintf(ocv_errorstr, sizeof(ocv_errorstr),
			    "OCV failed: frequency segment 1 mismatch in received OCI (we use %d but receiver is using %d)",
			    tx_seg1_idx, oci.seg1_idx);
		return -1;
	}

	return 0;
}
