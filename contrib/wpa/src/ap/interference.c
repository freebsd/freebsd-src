/*
 * Management of interference due to incumbent signals in 6 GHz band
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/hw_features_common.h"
#include "common/ieee802_11_common.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "interference.h"


#define BASE_6GHZ_FREQ 5950
/* Channel 2 (op class 136) has a center frequency of 5935 MHz. */
#define BASE_6GHZ_CH2_FREQ 5935

enum chan_seg {
	SEG_PRI20		  = 0x1,
	SEG_SEC20		  = 0x2,
	SEG_SEC40_LOW		  = 0x4,
	SEG_SEC40_UP		  = 0x8,
	SEG_SEC40		  = 0xC,
	SEG_SEC80_LOW		  = 0x10,
	SEG_SEC80_LOW_UP	  = 0x20,
	SEG_SEC80_UP_LOW	  = 0x40,
	SEG_SEC80_UP		  = 0x80,
	SEG_SEC80		  = 0xF0,
	SEG_SEC160_LOW		  = 0x0100,
	SEG_SEC160_LOW_UP	  = 0x0200,
	SEG_SEC160_LOW_UP_UP	  = 0x0400,
	SEG_SEC160_LOW_UP_UP_UP   = 0x0800,
	SEG_SEC160_UP_LOW_LOW_LOW = 0x1000,
	SEG_SEC160_UP_LOW_LOW	  = 0x2000,
	SEG_SEC160_UP_LOW	  = 0x4000,
	SEG_SEC160_UP		  = 0x8000,
	SEG_SEC160		  = 0xFF00,
};


static bool is_chan_disabled(struct hostapd_hw_modes *mode, int chan_num)
{
	struct hostapd_channel_data *chan;
	int i;

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if (chan->chan == chan_num)
			return !!(chan->flag & HOSTAPD_CHAN_DISABLED);
	}

	return true;
}


/*
 * chan_range_available - Check whether the channel can operate in the given
 * bandwidth in 6 GHz
 * @first_chan_idx - Channel index of the first 20 MHz channel in a segment
 * @num_chans - Number of 20 MHz channels needed for the operating bandwidth
 * @allowed_arr - Range of channels allowed in the given bandwidth
 * @allowed_arr_size - Number of channels allowed in the given bandwidth
 * Returns: Whether the channel can operation in the given bandwidth
 */
static bool chan_range_available(struct hostapd_hw_modes *mode,
				 unsigned int first_chan_idx,
				 unsigned int num_chans,
				 const int *allowed_arr,
				 unsigned int allowed_arr_size)
{
	struct hostapd_channel_data *first_chan;
	unsigned int i;

	first_chan = &mode->channels[first_chan_idx];

	if (!chan_pri_allowed(first_chan)) {
		wpa_printf(MSG_DEBUG, "Intf: Primary channel %d not allowed",
			   first_chan->chan);
		return false;
	}

	/* 20 MHz channel, so no need to check the range */
	if (num_chans == 1)
		return true;

	if (allowed_arr) {
		for (i = 0; i < allowed_arr_size; i++) {
			if (first_chan->chan == allowed_arr[i])
				break;
		}
		if (i == allowed_arr_size)
			return false;
	}

	/*
	 * Check whether all the 20 MHz channels in the given operating range
	 * are enabled.
	 */
	for (i = 1; i < num_chans; i++) {
		if (is_chan_disabled(mode, first_chan->chan + i * 4))
			return false;
	}

	return true;
}


static bool is_in_chanlist(struct hostapd_iface *iface,
			   struct hostapd_channel_data *chan)
{
	if (!iface->conf->acs_ch_list.num)
		return true;

	return freq_range_list_includes(&iface->conf->acs_ch_list, chan->chan);
}


static int get_center_freq_6g(int chan_idx, enum chan_width chan_width,
			      int *center_freq)
{
	if (!center_freq)
		return -1;

	*center_freq = 0;

	switch (chan_width) {
	case CHAN_WIDTH_20:
		if (chan_idx == 2) {
			*center_freq = BASE_6GHZ_CH2_FREQ;
			break;
		}
		if (chan_idx >= 1 && chan_idx <= 233)
			*center_freq = ((chan_idx / 4) * 4 + 1) * 5 +
				BASE_6GHZ_FREQ;
		break;
	case CHAN_WIDTH_40:
		if (chan_idx >= 1 && chan_idx <= 229)
			*center_freq = ((chan_idx / 8) * 8 + 3) * 5 +
				BASE_6GHZ_FREQ;
		break;
	case CHAN_WIDTH_80:
		if (chan_idx >= 1 && chan_idx <= 221)
			*center_freq = ((chan_idx / 16) * 16 + 7) * 5 +
				BASE_6GHZ_FREQ;
		break;
	case CHAN_WIDTH_160:
		if (chan_idx >= 1 && chan_idx <= 221)
			*center_freq = ((chan_idx / 32) * 32 + 15) * 5 +
				BASE_6GHZ_FREQ;
		break;
	case CHAN_WIDTH_320:
		if (chan_idx >= 1 && chan_idx <= 221)
			*center_freq = ((chan_idx / 32) * 32 + 31) * 5 +
				BASE_6GHZ_FREQ;
		break;
	default:
		break;
	}

	if (*center_freq == 0)
		return -1;

	return 0;
}


static bool is_interference_in_chanlist(int freq_start, int freq_end,
					const int *interference_freqs)
{
	int i;

	for (i = freq_start; i <= freq_end; i += 20) {
		if (int_array_includes(interference_freqs, i))
			return true;
	}
	return false;
}



static const int * get_allowed_channel_array(int num_chans, unsigned int *size)
{
	static const int allowed_40_6g[] = { 1, 9, 17, 25, 33, 41, 49, 57, 65,
					     73, 81, 89, 97, 105, 113, 121, 129,
					     137, 145, 153, 161, 169, 177, 185,
					     193, 201, 209, 217, 225, 233 };
	static const int allowed_80_6g[] = { 1, 17, 33, 49, 65, 81, 97, 113,
					     129, 145, 161, 177,
					     193, 209 };
	static const int allowed_160_6g[] = { 1, 33, 65, 97, 129, 161, 193 };
	static const int allowed_320_6g[] = { 1, 65, 129, 33, 97, 161 };

	switch (num_chans) {
	case 2:
		*size = ARRAY_SIZE(allowed_40_6g);
		return allowed_40_6g;
	case 4:
		*size = ARRAY_SIZE(allowed_80_6g);
		return allowed_80_6g;
	case 8:
		*size = ARRAY_SIZE(allowed_160_6g);
		return allowed_160_6g;
	case 16:
		*size = ARRAY_SIZE(allowed_320_6g);
		return allowed_320_6g;
	default:
		*size = 0;
		return NULL;
	}
}


/**
 * intf_find_channel_list - Find the list of channels that can operate with
 * channel width chan_width and not present within the range of current
 * operating range.
 * @chan_width - Channel width to be checked
 * @chandef_list - Pointer array to hold the list of valid available chandef
 * Returns: The total number of available chandefs that support the provided
 * bandwidth
 */
static int intf_find_channel_list(struct hostapd_iface *iface, int chan_width,
				  struct hostapd_channel_data **chandef_list,
				  const int *interference_freqs)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	int new_center_freq, new_start_freq, new_end_freq;
	const int *allowed_arr = NULL;
	unsigned int allowed_arr_size = 0;
	struct hostapd_channel_data *chan;
	int i;
	unsigned int channel_idx = 0, n_chans;
	int ret;

	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		n_chans = 1;
		break;
	case CHAN_WIDTH_40:
		n_chans = 2;
		break;
	case CHAN_WIDTH_80:
		n_chans = 4;
		break;
	case CHAN_WIDTH_80P80:
	case CHAN_WIDTH_160:
		n_chans = 8;
		break;
	case CHAN_WIDTH_320:
		n_chans = 16;
		break;
	default:
		n_chans = 1;
		break;
	}

	allowed_arr = get_allowed_channel_array(n_chans, &allowed_arr_size);

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (!chan_in_current_hw_info(iface->current_hw_info, chan)) {
			wpa_printf(MSG_DEBUG,
				   "Intf: Channel %d (%d) is not under current hardware index",
				   chan->freq, chan->chan);
			continue;
		}

		/* Skip incompatible chandefs */
		if (!chan_range_available(mode, i, n_chans,
					  allowed_arr, allowed_arr_size)) {
			wpa_printf(MSG_DEBUG,
				   "Intf: Range not available for chan %d (%d)",
				   chan->freq, chan->chan);
			continue;
		}

		if (!is_in_chanlist(iface, chan)) {
			wpa_printf(MSG_DEBUG,
				   "Intf: Channel %d (%d) not in chanlist",
				   chan->freq, chan->chan);
			continue;
		}

		ret = get_center_freq_6g(chan->chan, chan_width,
					 &new_center_freq);
		if (ret) {
			wpa_printf(MSG_INFO,
				   "Intf: Couldn't find center freq for chan %d chan_width %d",
				   chan->chan, chan_width);
			return 0;
		}

		new_start_freq = (new_center_freq -
				  channel_width_to_int(chan_width) / 2) + 10;
		new_end_freq = (new_center_freq +
				channel_width_to_int(chan_width) / 2) - 10;

		if (is_interference_in_chanlist(new_start_freq, new_end_freq,
						interference_freqs)) {
			wpa_printf(MSG_DEBUG,
				   "Intf: Found frequency which has interference in channel (%d)",
				   chan->chan);
			continue;
		}

		wpa_printf(MSG_DEBUG,
			   "Intf: Adding channel %d (%d) to valid chandef list",
			   chan->freq, chan->chan);
		chandef_list[channel_idx] = chan;
		channel_idx++;
	}

	return channel_idx;
}


static enum chan_width downgrade_bandwidth(enum chan_width chan_width)
{
	switch (chan_width) {
	case CHAN_WIDTH_320:
		return CHAN_WIDTH_160;
	case CHAN_WIDTH_160:
		return CHAN_WIDTH_80;
	case CHAN_WIDTH_80:
		return CHAN_WIDTH_40;
	case CHAN_WIDTH_40:
		return CHAN_WIDTH_20;
	default:
		return CHAN_WIDTH_20_NOHT;
	}
}


/*
 * hostapd_incumbt_sig_intf_detected - Incumbent signal interference is detected
 * in the operating channel. The interference channel information is available
 * as a bitmap(chan_bw_interference_bitmap). If interference has occurred in
 * the primary channel, do a complete channel switch to a different channel;
 * otherwise, reduce the operating bandwidth and continue AP operation using
 * the same primary channel.
 */
int hostapd_incumbt_sig_intf_detected(struct hostapd_iface *iface, int freq,
				      int chan_width, int cf1, int cf2,
				      u32 chan_bw_interference_bitmap)
{
	struct csa_settings settings;
	struct hostapd_channel_data *chan_data = NULL;
	struct hostapd_channel_data *chan_temp;
	struct hostapd_channel_data **available_chandef_list = NULL;
	int ret = 0;
	unsigned int i;
	u32 _rand;
	u32 chan_idx;
	int num_available_chandefs = 0;
	enum chan_width new_chan_width;
	int new_center_freq;
	int current_start_freq;
	int temp_width;
	struct hostapd_hw_modes *mode = iface->current_mode;
	int *interference_freqs = NULL;
	int primary_chan_bit = -1;
	int segment_freq;
	int start_freq = (cf1 - channel_width_to_int(chan_width) / 2) + 10;
	u8 channel_no;
	unsigned int num_segments;
	int new_bw;

	wpa_printf(MSG_DEBUG,
		   "Intf: input freq=%d, chan_width=%d, cf1=%d, cf2=%d, chan_bw_interference_bitmap=0x%x",
		   freq, chan_width, cf1, cf2, chan_bw_interference_bitmap);

	num_segments = channel_width_to_int(chan_width) / 20;
	if (!num_segments) {
		wpa_printf(MSG_INFO, "Intf: Invalid channel width %d",
			   chan_width);
		return -1;
	}

	/* Determine which 20 MHz segment contains the primary channel */
	for (i = 0; i < num_segments; i++) {
		segment_freq = start_freq + i * 20;
		if (segment_freq == freq) {
			primary_chan_bit = i;
			break;
		}
	}

	/* Check whether interference has occurred in primary 20 MHz channel */
	if (primary_chan_bit >= 0 &&
	    (chan_bw_interference_bitmap & BIT(primary_chan_bit))) {
		available_chandef_list = os_calloc(
			mode->num_channels,
			sizeof(struct hostapd_channel_data *));
		if (!available_chandef_list) {
			wpa_printf(MSG_ERROR,
				   "Intf: Failed to allocate available_chandef_list");
			return -1;
		}

		/* Store frequencies with interference in interference_freqs */
		current_start_freq =
			(cf1 - channel_width_to_int(chan_width) / 2) + 10;
		for (i = 0; i < num_segments; i++) {
			if (chan_bw_interference_bitmap & BIT(i)) {
				wpa_printf(MSG_DEBUG,
					   "Intf: Found incumbent signal interference in frequency %d",
					   current_start_freq + 20 * i);
				int_array_add_unique(&interference_freqs,
						     current_start_freq +
						     20 * i);
				if (!interference_freqs) {
					wpa_printf(MSG_ERROR,
						   "Intf: Failed to store interference frequencies");
					ret = -1;
					goto exit;
				}
			}
		}

		/* Find a random channel to be switched */
		temp_width = chan_width;

		while (temp_width > CHAN_WIDTH_20_NOHT) {
			num_available_chandefs =
				intf_find_channel_list(iface, temp_width,
						       available_chandef_list,
						       interference_freqs);
			if (num_available_chandefs > 0)
				break;
			wpa_printf(MSG_DEBUG, "Intf: Downgrading bandwidth");
			temp_width = downgrade_bandwidth(temp_width);
		}

		if (num_available_chandefs == 0) {
			wpa_printf(MSG_INFO, "Intf: No available_chandefs");
			goto exit;
		}

		if (os_get_random((u8 *) &_rand, sizeof(_rand)) < 0)
			_rand = os_random();

		chan_idx = _rand % num_available_chandefs;
		chan_data = available_chandef_list[chan_idx];
		new_chan_width = temp_width;

		wpa_printf(MSG_DEBUG, "Intf: Got random channel %d (%d)",
			   chan_data->freq, chan_data->chan);
	} else {
		/*
		 * Interference is not present in the primary 20 MHz, so
		 * reduce bandwidth.
		 */
		u8 seg0 = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
		u8 seg1 = hostapd_get_oper_centr_freq_seg1_idx(iface->conf);
		enum oper_chan_width oper_chwidth;
		int j;

		for (j = 0; j < mode->num_channels; j++) {
			chan_temp = &mode->channels[j];
			if (chan_temp->freq == freq)
				chan_data = chan_temp;
		}
		if (!chan_data) {
			wpa_printf(MSG_INFO, "Intf: No channel found");
			goto exit;
		}

		oper_chwidth = chan_width_to_oper_chwidth(chan_width);

		if (chan_width > CHAN_WIDTH_40) {
			punct_update_legacy_bw(chan_bw_interference_bitmap,
					       iface->conf->channel,
					       &oper_chwidth, &seg0, &seg1);
			if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ)
				new_chan_width = CHAN_WIDTH_160;
			else if (oper_chwidth == CONF_OPER_CHWIDTH_80MHZ)
				new_chan_width = CHAN_WIDTH_80;
			else if (seg0 == 0)
				new_chan_width = CHAN_WIDTH_20;
			else
				new_chan_width = CHAN_WIDTH_40;
		} else {
			new_chan_width = CHAN_WIDTH_20;
		}
	}

	if (new_chan_width > CHAN_WIDTH_20) {
		ret = get_center_freq_6g(chan_data->chan, new_chan_width,
					 &new_center_freq);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Intf: Couldn't find center freq for chan : %d chan_width : %d",
				   chan_data->chan, new_chan_width);
			goto exit;
		}
	} else {
		new_center_freq = chan_data->freq;
	}

	ieee80211_freq_to_chan(chan_data->freq, &channel_no);

	os_memset(&settings, 0, sizeof(settings));
	settings.cs_count = 5;
	settings.freq_params.freq = chan_data->freq;

	switch (new_chan_width) {
	case CHAN_WIDTH_40:
		new_bw = 40;
		break;
	case CHAN_WIDTH_80P80:
	case CHAN_WIDTH_80:
		new_bw = 80;
		break;
	case CHAN_WIDTH_160:
		new_bw = 160;
		break;
	case CHAN_WIDTH_320:
		new_bw = 320;
		break;
	default:
		new_bw = 20;
		break;
	}

	settings.freq_params.bandwidth = new_bw;
	settings.freq_params.channel = channel_no;
	settings.freq_params.center_freq1 = new_center_freq;
	settings.freq_params.ht_enabled = iface->conf->ieee80211n;
	settings.freq_params.vht_enabled = iface->conf->ieee80211ac;
	settings.freq_params.he_enabled = iface->conf->ieee80211ax;
	settings.freq_params.eht_enabled = iface->conf->ieee80211be;
	wpa_printf(MSG_DEBUG,
		   "Intf: channel=%u, freq=%d, bw=%d, center_freq1=%d",
		   settings.freq_params.channel,
		   settings.freq_params.freq,
		   settings.freq_params.bandwidth,
		   settings.freq_params.center_freq1);

	if (chan_data->freq == iface->freq) {
		if (hostapd_change_config_freq(iface->bss[0], iface->conf,
					       &settings.freq_params, NULL)) {
			wpa_printf(MSG_INFO,
				   "Intf: Failed to update bandwidth");
			ret = -1;
			goto exit;
		}
		if (hostapd_set_freq(
			    iface->bss[0], iface->conf->hw_mode, iface->freq,
			    iface->conf->channel, iface->conf->enable_edmg,
			    iface->conf->edmg_channel, iface->conf->ieee80211n,
			    iface->conf->ieee80211ac,
			    iface->conf->ieee80211ax,
			    iface->conf->ieee80211be,
			    iface->conf->secondary_channel,
			    hostapd_get_oper_chwidth(iface->conf),
			    hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
			    hostapd_get_oper_centr_freq_seg1_idx(iface->conf))) {
			wpa_printf(MSG_ERROR,
				   "Intf: Failed to apply bandwidth update");
			ret = -1;
			goto exit;
		}
		ieee802_11_set_beacons(iface);
		goto exit;
	}

	/* Channel and bandwidth have been decided, triggering channel switch */
	for (i = 0; i < iface->num_bss; i++) {
		/* Save CHAN_SWITCH VHT and HE config */
		hostapd_chan_switch_config(iface->bss[i],
					   &settings.freq_params);

		ret = hostapd_switch_channel(iface->bss[i], &settings);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Intf: Channel switch failed");
			break;
		}
	}

exit:
	os_free(available_chandef_list);
	os_free(interference_freqs);
	return ret;
}
