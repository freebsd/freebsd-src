/*
 * DFS - Dynamic Frequency Selection
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2013-2017, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "beacon.h"
#include "ap_drv_ops.h"
#include "drivers/driver.h"
#include "dfs.h"


enum dfs_channel_type {
	DFS_ANY_CHANNEL,
	DFS_AVAILABLE, /* non-radar or radar-available */
	DFS_NO_CAC_YET, /* radar-not-yet-available */
};

static struct hostapd_channel_data *
dfs_downgrade_bandwidth(struct hostapd_iface *iface, int *secondary_channel,
			u8 *oper_centr_freq_seg0_idx,
			u8 *oper_centr_freq_seg1_idx,
			enum dfs_channel_type *channel_type);


static bool dfs_use_radar_background(struct hostapd_iface *iface)
{
	return (iface->drv_flags2 & WPA_DRIVER_FLAGS2_RADAR_BACKGROUND) &&
		iface->conf->enable_background_radar;
}


static int dfs_get_used_n_chans(struct hostapd_iface *iface, int *seg1)
{
	int n_chans = 1;

	*seg1 = 0;

	if (iface->conf->ieee80211n && iface->conf->secondary_channel)
		n_chans = 2;

	if (iface->conf->ieee80211ac || iface->conf->ieee80211ax) {
		switch (hostapd_get_oper_chwidth(iface->conf)) {
		case CONF_OPER_CHWIDTH_USE_HT:
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			n_chans = 4;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			n_chans = 8;
			break;
		case CONF_OPER_CHWIDTH_80P80MHZ:
			n_chans = 4;
			*seg1 = 4;
			break;
		default:
			break;
		}
	}

	return n_chans;
}


/* dfs_channel_available: select new channel according to type parameter */
static int dfs_channel_available(struct hostapd_channel_data *chan,
				 enum dfs_channel_type type)
{
	if (type == DFS_NO_CAC_YET) {
		/* Select only radar channel where CAC has not been
		 * performed yet
		 */
		if ((chan->flag & HOSTAPD_CHAN_RADAR) &&
		    (chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		     HOSTAPD_CHAN_DFS_USABLE)
			return 1;
		return 0;
	}

	/*
	 * When radar detection happens, CSA is performed. However, there's no
	 * time for CAC, so radar channels must be skipped when finding a new
	 * channel for CSA, unless they are available for immediate use.
	 */
	if (type == DFS_AVAILABLE && (chan->flag & HOSTAPD_CHAN_RADAR) &&
	    ((chan->flag & HOSTAPD_CHAN_DFS_MASK) !=
	     HOSTAPD_CHAN_DFS_AVAILABLE))
		return 0;

	if (chan->flag & HOSTAPD_CHAN_DISABLED)
		return 0;
	if ((chan->flag & HOSTAPD_CHAN_RADAR) &&
	    ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
	     HOSTAPD_CHAN_DFS_UNAVAILABLE))
		return 0;
	return 1;
}


static int dfs_is_chan_allowed(struct hostapd_channel_data *chan, int n_chans)
{
	/*
	 * The tables contain first valid channel number based on channel width.
	 * We will also choose this first channel as the control one.
	 */
	int allowed_40[] = { 36, 44, 52, 60, 100, 108, 116, 124, 132, 149, 157,
			     165, 173, 184, 192 };
	/*
	 * VHT80, valid channels based on center frequency:
	 * 42, 58, 106, 122, 138, 155, 171
	 */
	int allowed_80[] = { 36, 52, 100, 116, 132, 149, 165 };
	/*
	 * VHT160 valid channels based on center frequency:
	 * 50, 114, 163
	 */
	int allowed_160[] = { 36, 100, 149 };
	int *allowed = allowed_40;
	unsigned int i, allowed_no = 0;

	switch (n_chans) {
	case 2:
		allowed = allowed_40;
		allowed_no = ARRAY_SIZE(allowed_40);
		break;
	case 4:
		allowed = allowed_80;
		allowed_no = ARRAY_SIZE(allowed_80);
		break;
	case 8:
		allowed = allowed_160;
		allowed_no = ARRAY_SIZE(allowed_160);
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unknown width for %d channels", n_chans);
		break;
	}

	for (i = 0; i < allowed_no; i++) {
		if (chan->chan == allowed[i])
			return 1;
	}

	return 0;
}


static struct hostapd_channel_data *
dfs_get_chan_data(struct hostapd_hw_modes *mode, int freq, int first_chan_idx)
{
	int i;

	for (i = first_chan_idx; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == freq)
			return &mode->channels[i];
	}

	return NULL;
}


static int dfs_chan_range_available(struct hostapd_hw_modes *mode,
				    int first_chan_idx, int num_chans,
				    enum dfs_channel_type type)
{
	struct hostapd_channel_data *first_chan, *chan;
	int i;
	u32 bw = num_chan_to_bw(num_chans);

	if (first_chan_idx + num_chans > mode->num_channels) {
		wpa_printf(MSG_DEBUG,
			   "DFS: some channels in range not defined");
		return 0;
	}

	first_chan = &mode->channels[first_chan_idx];

	/* hostapd DFS implementation assumes the first channel as primary.
	 * If it's not allowed to use the first channel as primary, decline the
	 * whole channel range. */
	if (!chan_pri_allowed(first_chan)) {
		wpa_printf(MSG_DEBUG, "DFS: primary channel not allowed");
		return 0;
	}

	for (i = 0; i < num_chans; i++) {
		chan = dfs_get_chan_data(mode, first_chan->freq + i * 20,
					 first_chan_idx);
		if (!chan) {
			wpa_printf(MSG_DEBUG, "DFS: no channel data for %d",
				   first_chan->freq + i * 20);
			return 0;
		}

		/* HT 40 MHz secondary channel availability checked only for
		 * primary channel */
		if (!chan_bw_allowed(chan, bw, 1, !i)) {
			wpa_printf(MSG_DEBUG, "DFS: bw now allowed for %d",
				   first_chan->freq + i * 20);
			return 0;
		}

		if (!dfs_channel_available(chan, type)) {
			wpa_printf(MSG_DEBUG, "DFS: channel not available %d",
				   first_chan->freq + i * 20);
			return 0;
		}
	}

	return 1;
}


static int is_in_chanlist(struct hostapd_iface *iface,
			  struct hostapd_channel_data *chan)
{
	if (!iface->conf->acs_ch_list.num)
		return 1;

	return freq_range_list_includes(&iface->conf->acs_ch_list, chan->chan);
}


/*
 * The function assumes HT40+ operation.
 * Make sure to adjust the following variables after calling this:
 *  - hapd->secondary_channel
 *  - hapd->vht/he_oper_centr_freq_seg0_idx
 *  - hapd->vht/he_oper_centr_freq_seg1_idx
 */
static int dfs_find_channel(struct hostapd_iface *iface,
			    struct hostapd_channel_data **ret_chan,
			    int idx, enum dfs_channel_type type)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int i, channel_idx = 0, n_chans, n_chans1;

	mode = iface->current_mode;
	n_chans = dfs_get_used_n_chans(iface, &n_chans1);

	wpa_printf(MSG_DEBUG, "DFS new chan checking %d channels", n_chans);
	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		/* Skip HT40/VHT incompatible channels */
		if (iface->conf->ieee80211n &&
		    iface->conf->secondary_channel &&
		    (!dfs_is_chan_allowed(chan, n_chans) ||
		     !(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40P))) {
			wpa_printf(MSG_DEBUG,
				   "DFS: channel %d (%d) is incompatible",
				   chan->freq, chan->chan);
			continue;
		}

		/* Skip incompatible chandefs */
		if (!dfs_chan_range_available(mode, i, n_chans, type)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: range not available for %d (%d)",
				   chan->freq, chan->chan);
			continue;
		}

		if (!is_in_chanlist(iface, chan)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: channel %d (%d) not in chanlist",
				   chan->freq, chan->chan);
			continue;
		}

		if (chan->max_tx_power < iface->conf->min_tx_power)
			continue;

		if ((chan->flag & HOSTAPD_CHAN_INDOOR_ONLY) &&
		    iface->conf->country[2] == 0x4f)
			continue;

		if (ret_chan && idx == channel_idx) {
			wpa_printf(MSG_DEBUG, "Selected channel %d (%d)",
				   chan->freq, chan->chan);
			*ret_chan = chan;
			return idx;
		}
		wpa_printf(MSG_DEBUG, "Adding channel %d (%d)",
			   chan->freq, chan->chan);
		channel_idx++;
	}
	return channel_idx;
}


static void dfs_adjust_center_freq(struct hostapd_iface *iface,
				   struct hostapd_channel_data *chan,
				   int secondary_channel,
				   int sec_chan_idx_80p80,
				   u8 *oper_centr_freq_seg0_idx,
				   u8 *oper_centr_freq_seg1_idx)
{
	if (!iface->conf->ieee80211ac && !iface->conf->ieee80211ax)
		return;

	if (!chan)
		return;

	*oper_centr_freq_seg1_idx = 0;

	switch (hostapd_get_oper_chwidth(iface->conf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (secondary_channel == 1)
			*oper_centr_freq_seg0_idx = chan->chan + 2;
		else if (secondary_channel == -1)
			*oper_centr_freq_seg0_idx = chan->chan - 2;
		else
			*oper_centr_freq_seg0_idx = chan->chan;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 6;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 14;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 6;
		*oper_centr_freq_seg1_idx = sec_chan_idx_80p80 + 6;
		break;

	default:
		wpa_printf(MSG_INFO,
			   "DFS: Unsupported channel width configuration");
		*oper_centr_freq_seg0_idx = 0;
		break;
	}

	wpa_printf(MSG_DEBUG, "DFS adjusting VHT center frequency: %d, %d",
		   *oper_centr_freq_seg0_idx,
		   *oper_centr_freq_seg1_idx);
}


/* Return start channel idx we will use for mode->channels[idx] */
static int dfs_get_start_chan_idx(struct hostapd_iface *iface, int *seg1_start)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int channel_no = iface->conf->channel;
	int res = -1, i;
	int chan_seg1 = -1;

	*seg1_start = -1;

	/* HT40- */
	if (iface->conf->ieee80211n && iface->conf->secondary_channel == -1)
		channel_no -= 4;

	/* VHT/HE/EHT */
	if (iface->conf->ieee80211ac || iface->conf->ieee80211ax ||
	    iface->conf->ieee80211be) {
		switch (hostapd_get_oper_chwidth(iface->conf)) {
		case CONF_OPER_CHWIDTH_USE_HT:
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 6;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 14;
			break;
		case CONF_OPER_CHWIDTH_80P80MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 6;
			chan_seg1 = hostapd_get_oper_centr_freq_seg1_idx(
				iface->conf) - 6;
			break;
		case CONF_OPER_CHWIDTH_320MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 30;
			break;
		default:
			wpa_printf(MSG_INFO,
				   "DFS only EHT20/40/80/160/80+80/320 is supported now");
			channel_no = -1;
			break;
		}
	}

	/* Get idx */
	mode = iface->current_mode;
	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if (chan->chan == channel_no) {
			res = i;
			break;
		}
	}

	if (res != -1 && chan_seg1 > -1) {
		int found = 0;

		/* Get idx for seg1 */
		mode = iface->current_mode;
		for (i = 0; i < mode->num_channels; i++) {
			chan = &mode->channels[i];
			if (chan->chan == chan_seg1) {
				*seg1_start = i;
				found = 1;
				break;
			}
		}
		if (!found)
			res = -1;
	}

	if (res == -1) {
		wpa_printf(MSG_DEBUG,
			   "DFS chan_idx seems wrong; num-ch: %d ch-no: %d conf-ch-no: %d 11n: %d sec-ch: %d vht-oper-width: %d",
			   mode->num_channels, channel_no, iface->conf->channel,
			   iface->conf->ieee80211n,
			   iface->conf->secondary_channel,
			   hostapd_get_oper_chwidth(iface->conf));

		for (i = 0; i < mode->num_channels; i++) {
			wpa_printf(MSG_DEBUG, "Available channel: %d",
				   mode->channels[i].chan);
		}
	}

	return res;
}


/* At least one channel have radar flag */
static int dfs_check_chans_radar(struct hostapd_iface *iface,
				 int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i, res = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		if (start_chan_idx + i >= mode->num_channels)
			break;
		channel = &mode->channels[start_chan_idx + i];
		if (channel->flag & HOSTAPD_CHAN_RADAR)
			res++;
	}

	return res;
}


/* All channels available */
static int dfs_check_chans_available(struct hostapd_iface *iface,
				     int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		channel = &mode->channels[start_chan_idx + i];

		if (channel->flag & HOSTAPD_CHAN_DISABLED)
			break;

		if (!(channel->flag & HOSTAPD_CHAN_RADAR))
			continue;

		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) !=
		    HOSTAPD_CHAN_DFS_AVAILABLE)
			break;
	}

	return i == n_chans;
}


/* At least one channel unavailable */
static int dfs_check_chans_unavailable(struct hostapd_iface *iface,
				       int start_chan_idx,
				       int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i, res = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		channel = &mode->channels[start_chan_idx + i];
		if (channel->flag & HOSTAPD_CHAN_DISABLED)
			res++;
		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_UNAVAILABLE)
			res++;
	}

	return res;
}


static struct hostapd_channel_data *
dfs_get_valid_channel(struct hostapd_iface *iface,
		      int *secondary_channel,
		      u8 *oper_centr_freq_seg0_idx,
		      u8 *oper_centr_freq_seg1_idx,
		      enum dfs_channel_type type)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan = NULL;
	struct hostapd_channel_data *chan2 = NULL;
	int num_available_chandefs;
	int chan_idx, chan_idx2;
	int sec_chan_idx_80p80 = -1;
	int i;
	u32 _rand;

	wpa_printf(MSG_DEBUG, "DFS: Selecting random channel");
	*secondary_channel = 0;
	*oper_centr_freq_seg0_idx = 0;
	*oper_centr_freq_seg1_idx = 0;

	if (iface->current_mode == NULL)
		return NULL;

	mode = iface->current_mode;
	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return NULL;

	/* Get the count first */
	num_available_chandefs = dfs_find_channel(iface, NULL, 0, type);
	wpa_printf(MSG_DEBUG, "DFS: num_available_chandefs=%d",
		   num_available_chandefs);
	if (num_available_chandefs == 0)
		return NULL;

	if (os_get_random((u8 *) &_rand, sizeof(_rand)) < 0)
		return NULL;
	chan_idx = _rand % num_available_chandefs;
	wpa_printf(MSG_DEBUG, "DFS: Picked random entry from the list: %d/%d",
		   chan_idx, num_available_chandefs);
	dfs_find_channel(iface, &chan, chan_idx, type);
	if (!chan) {
		wpa_printf(MSG_DEBUG, "DFS: no random channel found");
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "DFS: got random channel %d (%d)",
		   chan->freq, chan->chan);

	/* dfs_find_channel() calculations assume HT40+ */
	if (iface->conf->secondary_channel)
		*secondary_channel = 1;
	else
		*secondary_channel = 0;

	/* Get secondary channel for HT80P80 */
	if (hostapd_get_oper_chwidth(iface->conf) ==
	    CONF_OPER_CHWIDTH_80P80MHZ) {
		if (num_available_chandefs <= 1) {
			wpa_printf(MSG_ERROR,
				   "only 1 valid chan, can't support 80+80");
			return NULL;
		}

		/*
		 * Loop all channels except channel1 to find a valid channel2
		 * that is not adjacent to channel1.
		 */
		for (i = 0; i < num_available_chandefs - 1; i++) {
			/* start from chan_idx + 1, end when chan_idx - 1 */
			chan_idx2 = (chan_idx + 1 + i) % num_available_chandefs;
			dfs_find_channel(iface, &chan2, chan_idx2, type);
			if (chan2 && abs(chan2->chan - chan->chan) > 12) {
				/* two channels are not adjacent */
				sec_chan_idx_80p80 = chan2->chan;
				wpa_printf(MSG_DEBUG,
					   "DFS: got second chan: %d (%d)",
					   chan2->freq, chan2->chan);
				break;
			}
		}

		/* Check if we got a valid secondary channel which is not
		 * adjacent to the first channel.
		 */
		if (sec_chan_idx_80p80 == -1) {
			wpa_printf(MSG_INFO,
				   "DFS: failed to get chan2 for 80+80");
			return NULL;
		}
	}

	dfs_adjust_center_freq(iface, chan,
			       *secondary_channel,
			       sec_chan_idx_80p80,
			       oper_centr_freq_seg0_idx,
			       oper_centr_freq_seg1_idx);

	return chan;
}


static int dfs_set_valid_channel(struct hostapd_iface *iface, int skip_radar)
{
	struct hostapd_channel_data *channel;
	u8 cf1 = 0, cf2 = 0;
	int sec = 0;

	channel = dfs_get_valid_channel(iface, &sec, &cf1, &cf2,
					skip_radar ? DFS_AVAILABLE :
					DFS_ANY_CHANNEL);
	if (!channel) {
		wpa_printf(MSG_ERROR, "could not get valid channel");
		return -1;
	}

	iface->freq = channel->freq;
	iface->conf->channel = channel->chan;
	iface->conf->secondary_channel = sec;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, cf1);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, cf2);

	return 0;
}


static int set_dfs_state_freq(struct hostapd_iface *iface, int freq, u32 state)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan = NULL;
	int i;

	mode = iface->current_mode;
	if (mode == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "set_dfs_state 0x%X for %d MHz", state, freq);
	for (i = 0; i < iface->current_mode->num_channels; i++) {
		chan = &iface->current_mode->channels[i];
		if (chan->freq == freq) {
			if (chan->flag & HOSTAPD_CHAN_RADAR) {
				chan->flag &= ~HOSTAPD_CHAN_DFS_MASK;
				chan->flag |= state;
				return 1; /* Channel found */
			}
		}
	}
	wpa_printf(MSG_WARNING, "Can't set DFS state for freq %d MHz", freq);
	return 0;
}


static int set_dfs_state(struct hostapd_iface *iface, int freq, int ht_enabled,
			 int chan_offset, int chan_width, int cf1,
			 int cf2, u32 state)
{
	int n_chans = 1, i;
	struct hostapd_hw_modes *mode;
	int frequency = freq;
	int frequency2 = 0;
	int ret = 0;

	mode = iface->current_mode;
	if (mode == NULL)
		return 0;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A) {
		wpa_printf(MSG_WARNING, "current_mode != IEEE80211A");
		return 0;
	}

	/* Seems cf1 and chan_width is enough here */
	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		n_chans = 1;
		if (frequency == 0)
			frequency = cf1;
		break;
	case CHAN_WIDTH_40:
		n_chans = 2;
		frequency = cf1 - 10;
		break;
	case CHAN_WIDTH_80:
		n_chans = 4;
		frequency = cf1 - 30;
		break;
	case CHAN_WIDTH_80P80:
		n_chans = 4;
		frequency = cf1 - 30;
		frequency2 = cf2 - 30;
		break;
	case CHAN_WIDTH_160:
		n_chans = 8;
		frequency = cf1 - 70;
		break;
	default:
		wpa_printf(MSG_INFO, "DFS chan_width %d not supported",
			   chan_width);
		break;
	}

	wpa_printf(MSG_DEBUG, "DFS freq: %dMHz, n_chans: %d", frequency,
		   n_chans);
	for (i = 0; i < n_chans; i++) {
		ret += set_dfs_state_freq(iface, frequency, state);
		frequency = frequency + 20;

		if (chan_width == CHAN_WIDTH_80P80) {
			ret += set_dfs_state_freq(iface, frequency2, state);
			frequency2 = frequency2 + 20;
		}
	}

	return ret;
}


static int dfs_are_channels_overlapped(struct hostapd_iface *iface, int freq,
				       int chan_width, int cf1, int cf2)
{
	int start_chan_idx, start_chan_idx1;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int n_chans, n_chans1, i, j, frequency = freq, radar_n_chans = 1;
	u8 radar_chan;
	int res = 0;

	/* Our configuration */
	mode = iface->current_mode;
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1);
	n_chans = dfs_get_used_n_chans(iface, &n_chans1);

	/* Check we are on DFS channel(s) */
	if (!dfs_check_chans_radar(iface, start_chan_idx, n_chans))
		return 0;

	/* Reported via radar event */
	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		radar_n_chans = 1;
		if (frequency == 0)
			frequency = cf1;
		break;
	case CHAN_WIDTH_40:
		radar_n_chans = 2;
		frequency = cf1 - 10;
		break;
	case CHAN_WIDTH_80:
		radar_n_chans = 4;
		frequency = cf1 - 30;
		break;
	case CHAN_WIDTH_160:
		radar_n_chans = 8;
		frequency = cf1 - 70;
		break;
	default:
		wpa_printf(MSG_INFO, "DFS chan_width %d not supported",
			   chan_width);
		break;
	}

	ieee80211_freq_to_chan(frequency, &radar_chan);

	for (i = 0; i < n_chans; i++) {
		chan = &mode->channels[start_chan_idx + i];
		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;
		for (j = 0; j < radar_n_chans; j++) {
			wpa_printf(MSG_DEBUG, "checking our: %d, radar: %d",
				   chan->chan, radar_chan + j * 4);
			if (chan->chan == radar_chan + j * 4)
				res++;
		}
	}

	wpa_printf(MSG_DEBUG, "overlapped: %d", res);

	return res;
}


static unsigned int dfs_get_cac_time(struct hostapd_iface *iface,
				     int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i;
	unsigned int cac_time_ms = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		if (start_chan_idx + i >= mode->num_channels)
			break;
		channel = &mode->channels[start_chan_idx + i];
		if (!(channel->flag & HOSTAPD_CHAN_RADAR))
			continue;
		if (channel->dfs_cac_ms > cac_time_ms)
			cac_time_ms = channel->dfs_cac_ms;
	}

	return cac_time_ms;
}


/*
 * Main DFS handler
 * 1 - continue channel/ap setup
 * 0 - channel/ap setup will be continued after CAC
 * -1 - hit critical error
 */
int hostapd_handle_dfs(struct hostapd_iface *iface)
{
	int res, n_chans, n_chans1, start_chan_idx, start_chan_idx1;
	int skip_radar = 0;

	if (is_6ghz_freq(iface->freq))
		return 1;

	if (!iface->current_mode) {
		/*
		 * This can happen with drivers that do not provide mode
		 * information and as such, cannot really use hostapd for DFS.
		 */
		wpa_printf(MSG_DEBUG,
			   "DFS: No current_mode information - assume no need to perform DFS operations by hostapd");
		return 1;
	}

	iface->cac_started = 0;

	do {
		/* Get start (first) channel for current configuration */
		start_chan_idx = dfs_get_start_chan_idx(iface,
							&start_chan_idx1);
		if (start_chan_idx == -1)
			return -1;

		/* Get number of used channels, depend on width */
		n_chans = dfs_get_used_n_chans(iface, &n_chans1);

		/* Setup CAC time */
		iface->dfs_cac_ms = dfs_get_cac_time(iface, start_chan_idx,
						     n_chans);

		/* Check if any of configured channels require DFS */
		res = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
		wpa_printf(MSG_DEBUG,
			   "DFS %d channels required radar detection",
			   res);
		if (!res)
			return 1;

		/* Check if all channels are DFS available */
		res = dfs_check_chans_available(iface, start_chan_idx, n_chans);
		wpa_printf(MSG_DEBUG,
			   "DFS all channels available, (SKIP CAC): %s",
			   res ? "yes" : "no");
		if (res)
			return 1;

		/* Check if any of configured channels is unavailable */
		res = dfs_check_chans_unavailable(iface, start_chan_idx,
						  n_chans);
		wpa_printf(MSG_DEBUG, "DFS %d chans unavailable - choose other channel: %s",
			   res, res ? "yes": "no");
		if (res) {
			if (dfs_set_valid_channel(iface, skip_radar) < 0) {
				hostapd_set_state(iface, HAPD_IFACE_DFS);
				return 0;
			}
		}
	} while (res);

	/* Finally start CAC */
	hostapd_set_state(iface, HAPD_IFACE_DFS);
	wpa_printf(MSG_DEBUG, "DFS start CAC on %d MHz%s", iface->freq,
		   dfs_use_radar_background(iface) ? " (background)" : "");
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d sec_chan=%d, width=%d, seg0=%d, seg1=%d, cac_time=%ds",
		iface->freq,
		iface->conf->channel, iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		iface->dfs_cac_ms / 1000);

	res = hostapd_start_dfs_cac(
		iface, iface->conf->hw_mode, iface->freq, iface->conf->channel,
		iface->conf->ieee80211n, iface->conf->ieee80211ac,
		iface->conf->ieee80211ax, iface->conf->ieee80211be,
		iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		dfs_use_radar_background(iface));

	if (res) {
		wpa_printf(MSG_ERROR, "DFS start_dfs_cac() failed, %d", res);
		return -1;
	}

	if (dfs_use_radar_background(iface)) {
		/* Cache background radar parameters. */
		iface->radar_background.channel = iface->conf->channel;
		iface->radar_background.secondary_channel =
			iface->conf->secondary_channel;
		iface->radar_background.freq = iface->freq;
		iface->radar_background.centr_freq_seg0_idx =
			hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
		iface->radar_background.centr_freq_seg1_idx =
			hostapd_get_oper_centr_freq_seg1_idx(iface->conf);

		/*
		 * Let's select a random channel according to the
		 * regulations and perform CAC on dedicated radar chain.
		 */
		res = dfs_set_valid_channel(iface, 1);
		if (res < 0)
			return res;

		iface->radar_background.temp_ch = 1;
		return 1;
	}

	return 0;
}


int hostapd_is_dfs_chan_available(struct hostapd_iface *iface)
{
	int n_chans, n_chans1, start_chan_idx, start_chan_idx1;

	/* Get the start (first) channel for current configuration */
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1);
	if (start_chan_idx < 0)
		return 0;

	/* Get the number of used channels, depending on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1);

	/* Check if all channels are DFS available */
	return dfs_check_chans_available(iface, start_chan_idx, n_chans);
}


static int hostapd_dfs_request_channel_switch(struct hostapd_iface *iface,
					      int channel, int freq,
					      int secondary_channel,
					      u8 current_vht_oper_chwidth,
					      u8 oper_centr_freq_seg0_idx,
					      u8 oper_centr_freq_seg1_idx)
{
	struct hostapd_hw_modes *cmode = iface->current_mode;
	int ieee80211_mode = IEEE80211_MODE_AP, err;
	struct csa_settings csa_settings;
	u8 new_vht_oper_chwidth;
	unsigned int i;
	unsigned int num_err = 0;

	wpa_printf(MSG_DEBUG, "DFS will switch to a new channel %d", channel);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NEW_CHANNEL
		"freq=%d chan=%d sec_chan=%d", freq, channel,
		secondary_channel);

	new_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	hostapd_set_oper_chwidth(iface->conf, current_vht_oper_chwidth);

	/* Setup CSA request */
	os_memset(&csa_settings, 0, sizeof(csa_settings));
	csa_settings.cs_count = 5;
	csa_settings.block_tx = 1;
	csa_settings.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (iface->bss[0]->conf->mld_ap)
		csa_settings.link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_MESH
	if (iface->mconf)
		ieee80211_mode = IEEE80211_MODE_MESH;
#endif /* CONFIG_MESH */
	err = hostapd_set_freq_params(&csa_settings.freq_params,
				      iface->conf->hw_mode,
				      freq, channel,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      secondary_channel,
				      new_vht_oper_chwidth,
				      oper_centr_freq_seg0_idx,
				      oper_centr_freq_seg1_idx,
				      cmode->vht_capab,
				      &cmode->he_capab[ieee80211_mode],
				      &cmode->eht_capab[ieee80211_mode],
				      hostapd_get_punct_bitmap(iface->bss[0]));

	if (err) {
		wpa_printf(MSG_ERROR,
			   "DFS failed to calculate CSA freq params");
		hostapd_disable_iface(iface);
		return err;
	}

	for (i = 0; i < iface->num_bss; i++) {
		err = hostapd_switch_channel(iface->bss[i], &csa_settings);
		if (err)
			num_err++;
	}

	if (num_err == iface->num_bss) {
		wpa_printf(MSG_WARNING,
			   "DFS failed to schedule CSA (%d) - trying fallback",
			   err);
		iface->freq = freq;
		iface->conf->channel = channel;
		iface->conf->secondary_channel = secondary_channel;
		hostapd_set_oper_chwidth(iface->conf, new_vht_oper_chwidth);
		hostapd_set_oper_centr_freq_seg0_idx(iface->conf,
						     oper_centr_freq_seg0_idx);
		hostapd_set_oper_centr_freq_seg1_idx(iface->conf,
						     oper_centr_freq_seg1_idx);

		hostapd_disable_iface(iface);
		hostapd_enable_iface(iface);

		return 0;
	}

	/* Channel configuration will be updated once CSA completes and
	 * ch_switch_notify event is received */
	wpa_printf(MSG_DEBUG, "DFS waiting channel switch event");

	return 0;
}


static void hostapd_dfs_update_background_chain(struct hostapd_iface *iface)
{
	int sec = 0;
	enum dfs_channel_type channel_type = DFS_NO_CAC_YET;
	struct hostapd_channel_data *channel;
	u8 oper_centr_freq_seg0_idx = 0;
	u8 oper_centr_freq_seg1_idx = 0;

	/*
	 * Allow selection of DFS channel in ETSI to comply with
	 * uniform spreading.
	 */
	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI)
		channel_type = DFS_ANY_CHANNEL;

	channel = dfs_get_valid_channel(iface, &sec, &oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);
	if (!channel ||
	    channel->chan == iface->conf->channel ||
	    channel->chan == iface->radar_background.channel)
		channel = dfs_downgrade_bandwidth(iface, &sec,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &channel_type);
	if (!channel ||
	    hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
				  channel->freq, channel->chan,
				  iface->conf->ieee80211n,
				  iface->conf->ieee80211ac,
				  iface->conf->ieee80211ax,
				  iface->conf->ieee80211be,
				  sec, hostapd_get_oper_chwidth(iface->conf),
				  oper_centr_freq_seg0_idx,
				  oper_centr_freq_seg1_idx, true)) {
		wpa_printf(MSG_ERROR, "DFS failed to start CAC offchannel");
		iface->radar_background.channel = -1;
		return;
	}

	iface->radar_background.channel = channel->chan;
	iface->radar_background.freq = channel->freq;
	iface->radar_background.secondary_channel = sec;
	iface->radar_background.centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
	iface->radar_background.centr_freq_seg1_idx = oper_centr_freq_seg1_idx;

	wpa_printf(MSG_DEBUG,
		   "%s: setting background chain to chan %d (%d MHz)",
		   __func__, channel->chan, channel->freq);
}


static bool
hostapd_dfs_is_background_event(struct hostapd_iface *iface, int freq)
{
	return dfs_use_radar_background(iface) &&
		iface->radar_background.channel != -1 &&
		iface->radar_background.freq == freq;
}


static int
hostapd_dfs_start_channel_switch_background(struct hostapd_iface *iface)
{
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	iface->conf->channel = iface->radar_background.channel;
	iface->freq = iface->radar_background.freq;
	iface->conf->secondary_channel =
		iface->radar_background.secondary_channel;
	hostapd_set_oper_centr_freq_seg0_idx(
		iface->conf, iface->radar_background.centr_freq_seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(
		iface->conf, iface->radar_background.centr_freq_seg1_idx);

	hostapd_dfs_update_background_chain(iface);

	return hostapd_dfs_request_channel_switch(
		iface, iface->conf->channel, iface->freq,
		iface->conf->secondary_channel, current_vht_oper_chwidth,
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf));
}


int hostapd_dfs_complete_cac(struct hostapd_iface *iface, int success, int freq,
			     int ht_enabled, int chan_offset, int chan_width,
			     int cf1, int cf2)
{
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_COMPLETED
		"success=%d freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d radar_detected=%d",
		success, freq, ht_enabled, chan_offset, chan_width, cf1, cf2,
		iface->radar_detected);

	if (success) {
		/* Complete iface/ap configuration */
		if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) {
			/* Complete AP configuration for the first bring up. If
			 * a radar was detected in this channel, interface setup
			 * will be handled in
			 * 1. hostapd_event_ch_switch() if switching to a
			 *    non-DFS channel
			 * 2. on next CAC complete event if switching to another
			 *    DFS channel.
			 */
			if (iface->state != HAPD_IFACE_ENABLED &&
			    !iface->radar_detected)
				hostapd_setup_interface_complete(iface, 0);
			else
				iface->cac_started = 0;
		} else {
			set_dfs_state(iface, freq, ht_enabled, chan_offset,
				      chan_width, cf1, cf2,
				      HOSTAPD_CHAN_DFS_AVAILABLE);

			/*
			 * Radar event from background chain for the selected
			 * channel. Perform CSA, move the main chain to the
			 * selected channel and configure the background chain
			 * to a new DFS channel.
			 */
			if (hostapd_dfs_is_background_event(iface, freq)) {
				iface->radar_background.cac_started = 0;
				if (!iface->radar_background.temp_ch)
					return 0;

				iface->radar_background.temp_ch = 0;
				return hostapd_dfs_start_channel_switch_background(iface);
			}

			/*
			 * Just mark the channel available when CAC completion
			 * event is received in enabled state. CAC result could
			 * have been propagated from another radio having the
			 * same regulatory configuration. When CAC completion is
			 * received during non-HAPD_IFACE_ENABLED state, make
			 * sure the configured channel is available because this
			 * CAC completion event could have been propagated from
			 * another radio.
			 */
			if (iface->state != HAPD_IFACE_ENABLED &&
			    hostapd_is_dfs_chan_available(iface)) {
				hostapd_setup_interface_complete(iface, 0);
				iface->cac_started = 0;
			}
		}
	} else if (hostapd_dfs_is_background_event(iface, freq)) {
		iface->radar_background.cac_started = 0;
		hostapd_dfs_update_background_chain(iface);
	}

	iface->radar_detected = false;
	return 0;
}


int hostapd_dfs_pre_cac_expired(struct hostapd_iface *iface, int freq,
				int ht_enabled, int chan_offset, int chan_width,
				int cf1, int cf2)
{
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_PRE_CAC_EXPIRED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2);

	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
		      cf1, cf2, HOSTAPD_CHAN_DFS_USABLE);

	return 0;
}


static struct hostapd_channel_data *
dfs_downgrade_bandwidth(struct hostapd_iface *iface, int *secondary_channel,
			u8 *oper_centr_freq_seg0_idx,
			u8 *oper_centr_freq_seg1_idx,
			enum dfs_channel_type *channel_type)
{
	struct hostapd_channel_data *channel;

	for (;;) {
		channel = dfs_get_valid_channel(iface, secondary_channel,
						oper_centr_freq_seg0_idx,
						oper_centr_freq_seg1_idx,
						*channel_type);
		if (channel) {
			wpa_printf(MSG_DEBUG, "DFS: Selected channel: %d",
				   channel->chan);
			return channel;
		}

		if (*channel_type != DFS_ANY_CHANNEL) {
			*channel_type = DFS_ANY_CHANNEL;
		} else {
			int oper_chwidth;

			oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
			if (oper_chwidth == CONF_OPER_CHWIDTH_USE_HT)
				break;
			*channel_type = DFS_AVAILABLE;
			hostapd_set_oper_chwidth(iface->conf, oper_chwidth - 1);
		}
	}

	wpa_printf(MSG_INFO,
		   "%s: no DFS channels left, waiting for NOP to finish",
		   __func__);
	return NULL;
}


static int hostapd_dfs_start_channel_switch_cac(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *channel;
	int secondary_channel;
	u8 oper_centr_freq_seg0_idx = 0;
	u8 oper_centr_freq_seg1_idx = 0;
	enum dfs_channel_type channel_type = DFS_ANY_CHANNEL;
	int err = 1;

	/* Radar detected during active CAC */
	iface->cac_started = 0;
	channel = dfs_get_valid_channel(iface, &secondary_channel,
					&oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);

	if (!channel) {
		channel = dfs_downgrade_bandwidth(iface, &secondary_channel,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &channel_type);
		if (!channel) {
			wpa_printf(MSG_ERROR, "No valid channel available");
			return err;
		}
	}

	wpa_printf(MSG_DEBUG, "DFS will switch to a new channel %d",
		   channel->chan);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NEW_CHANNEL
		"freq=%d chan=%d sec_chan=%d", channel->freq,
		channel->chan, secondary_channel);

	iface->freq = channel->freq;
	iface->conf->channel = channel->chan;
	iface->conf->secondary_channel = secondary_channel;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf,
					     oper_centr_freq_seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf,
					     oper_centr_freq_seg1_idx);
	err = 0;

	hostapd_setup_interface_complete(iface, err);
	return err;
}


static int
hostapd_dfs_background_start_channel_switch(struct hostapd_iface *iface,
					    int freq)
{
	if (!dfs_use_radar_background(iface))
		return -1; /* Background radar chain not supported. */

	wpa_printf(MSG_DEBUG,
		   "%s called (background CAC active: %s, CSA active: %s)",
		   __func__, iface->radar_background.cac_started ? "yes" : "no",
		   hostapd_csa_in_progress(iface) ? "yes" : "no");

	/* Check if CSA in progress */
	if (hostapd_csa_in_progress(iface))
		return 0;

	if (hostapd_dfs_is_background_event(iface, freq)) {
		/*
		 * Radar pattern is reported on the background chain.
		 * Just select a new random channel according to the
		 * regulations for monitoring.
		 */
		hostapd_dfs_update_background_chain(iface);
		return 0;
	}

	/*
	 * If background radar detection is supported and the radar channel
	 * monitored by the background chain is available switch to it without
	 * waiting for the CAC.
	 */
	if (iface->radar_background.channel == -1)
		return -1; /* Background radar chain not available. */

	if (iface->radar_background.cac_started) {
		/*
		 * Background channel not available yet. Perform CAC on the
		 * main chain.
		 */
		iface->radar_background.temp_ch = 1;
		return -1;
	}

	return hostapd_dfs_start_channel_switch_background(iface);
}


static int hostapd_dfs_start_channel_switch(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *channel;
	int secondary_channel;
	u8 oper_centr_freq_seg0_idx;
	u8 oper_centr_freq_seg1_idx;
	enum dfs_channel_type channel_type = DFS_AVAILABLE;
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	wpa_printf(MSG_DEBUG, "%s called (CAC active: %s, CSA active: %s)",
		   __func__, iface->cac_started ? "yes" : "no",
		   hostapd_csa_in_progress(iface) ? "yes" : "no");

	/* Check if CSA in progress */
	if (hostapd_csa_in_progress(iface))
		return 0;

	/* Check if active CAC */
	if (iface->cac_started)
		return hostapd_dfs_start_channel_switch_cac(iface);

	/*
	 * Allow selection of DFS channel in ETSI to comply with
	 * uniform spreading.
	 */
	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI)
		channel_type = DFS_ANY_CHANNEL;

	/* Perform channel switch/CSA */
	channel = dfs_get_valid_channel(iface, &secondary_channel,
					&oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);

	if (!channel) {
		/*
		 * If there is no channel to switch immediately to, check if
		 * there is another channel where we can switch even if it
		 * requires to perform a CAC first.
		 */
		channel_type = DFS_ANY_CHANNEL;
		channel = dfs_downgrade_bandwidth(iface, &secondary_channel,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &channel_type);
		if (!channel) {
			/*
			 * Toggle interface state to enter DFS state
			 * until NOP is finished.
			 */
			hostapd_disable_iface(iface);
			hostapd_enable_iface(iface);
			return 0;
		}

		if (channel_type == DFS_ANY_CHANNEL) {
			iface->freq = channel->freq;
			iface->conf->channel = channel->chan;
			iface->conf->secondary_channel = secondary_channel;
			hostapd_set_oper_centr_freq_seg0_idx(
				iface->conf, oper_centr_freq_seg0_idx);
			hostapd_set_oper_centr_freq_seg1_idx(
				iface->conf, oper_centr_freq_seg1_idx);

			hostapd_disable_iface(iface);
			hostapd_enable_iface(iface);
			return 0;
		}
	}

	return hostapd_dfs_request_channel_switch(iface, channel->chan,
						  channel->freq,
						  secondary_channel,
						  current_vht_oper_chwidth,
						  oper_centr_freq_seg0_idx,
						  oper_centr_freq_seg1_idx);
}


int hostapd_dfs_radar_detected(struct hostapd_iface *iface, int freq,
			       int ht_enabled, int chan_offset, int chan_width,
			       int cf1, int cf2)
{
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_RADAR_DETECTED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2);

	iface->radar_detected = true;

	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	if (!iface->conf->ieee80211h)
		return 0;

	/* mark radar frequency as invalid */
	if (!set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
			   cf1, cf2, HOSTAPD_CHAN_DFS_UNAVAILABLE))
		return 0;

	if (!hostapd_dfs_is_background_event(iface, freq)) {
		/* Skip if reported radar event not overlapped our channels */
		if (!dfs_are_channels_overlapped(iface, freq, chan_width,
						 cf1, cf2))
			return 0;
	}

	if (hostapd_dfs_background_start_channel_switch(iface, freq)) {
		/* Radar detected while operating, switch the channel. */
		return hostapd_dfs_start_channel_switch(iface);
	}

	return 0;
}


int hostapd_dfs_nop_finished(struct hostapd_iface *iface, int freq,
			     int ht_enabled, int chan_offset, int chan_width,
			     int cf1, int cf2)
{
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NOP_FINISHED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2);

	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	/* TODO add correct implementation here */
	set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
		      cf1, cf2, HOSTAPD_CHAN_DFS_USABLE);

	if (iface->state == HAPD_IFACE_DFS && !iface->cac_started) {
		/* Handle cases where all channels were initially unavailable */
		hostapd_handle_dfs(iface);
	} else if (dfs_use_radar_background(iface) &&
		   iface->radar_background.channel == -1) {
		/* Reset radar background chain if disabled */
		hostapd_dfs_update_background_chain(iface);
	}

	return 0;
}


int hostapd_is_dfs_required(struct hostapd_iface *iface)
{
	int n_chans, n_chans1, start_chan_idx, start_chan_idx1, res;

	if ((!(iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) &&
	     !iface->conf->ieee80211h) ||
	    !iface->current_mode ||
	    iface->current_mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	/* Get start (first) channel for current configuration */
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1);
	if (start_chan_idx == -1)
		return -1;

	/* Get number of used channels, depend on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1);

	/* Check if any of configured channels require DFS */
	res = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
	if (res)
		return res;
	if (start_chan_idx1 >= 0 && n_chans1 > 0)
		res = dfs_check_chans_radar(iface, start_chan_idx1, n_chans1);
	return res;
}


int hostapd_dfs_start_cac(struct hostapd_iface *iface, int freq,
			  int ht_enabled, int chan_offset, int chan_width,
			  int cf1, int cf2)
{
	if (hostapd_dfs_is_background_event(iface, freq)) {
		iface->radar_background.cac_started = 1;
	} else {
		/* This is called when the driver indicates that an offloaded
		 * DFS has started CAC. radar_detected might be set for previous
		 * DFS channel. Clear it for this new CAC process. */
		hostapd_set_state(iface, HAPD_IFACE_DFS);
		iface->cac_started = 1;

		/* Clear radar_detected in case it is for the previous
		 * frequency. Also remove disabled link's information in RNR
		 * element from other links. */
		iface->radar_detected = false;
		if (iface->interfaces && iface->interfaces->count > 1)
			ieee802_11_set_beacons(iface);
	}
	/* TODO: How to check CAC time for ETSI weather channels? */
	iface->dfs_cac_ms = 60000;
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d chan_offset=%d width=%d seg0=%d "
		"seg1=%d cac_time=%ds%s",
		freq, (freq - 5000) / 5, chan_offset, chan_width, cf1, cf2,
		iface->dfs_cac_ms / 1000,
		hostapd_dfs_is_background_event(iface, freq) ?
		" (background)" : "");

	os_get_reltime(&iface->dfs_cac_start);
	return 0;
}


/*
 * Main DFS handler for offloaded case.
 * 2 - continue channel/AP setup for non-DFS channel
 * 1 - continue channel/AP setup for DFS channel
 * 0 - channel/AP setup will be continued after CAC
 * -1 - hit critical error
 */
int hostapd_handle_dfs_offload(struct hostapd_iface *iface)
{
	int dfs_res;

	wpa_printf(MSG_DEBUG, "%s: iface->cac_started: %d",
		   __func__, iface->cac_started);

	/*
	 * If DFS has already been started, then we are being called from a
	 * callback to continue AP/channel setup. Reset the CAC start flag and
	 * return.
	 */
	if (iface->cac_started) {
		wpa_printf(MSG_DEBUG, "%s: iface->cac_started: %d",
			   __func__, iface->cac_started);
		iface->cac_started = 0;
		return 1;
	}

	dfs_res = hostapd_is_dfs_required(iface);
	if (dfs_res > 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: freq %d MHz requires DFS for %d chans",
			   __func__, iface->freq, dfs_res);
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "%s: freq %d MHz does not require DFS. Continue channel/AP setup",
		   __func__, iface->freq);
	return 2;
}


int hostapd_is_dfs_overlap(struct hostapd_iface *iface, enum chan_width width,
			   int center_freq)
{
	struct hostapd_channel_data *chan;
	struct hostapd_hw_modes *mode = iface->current_mode;
	int half_width;
	int res = 0;
	int i;

	if (!iface->conf->ieee80211h || !mode ||
	    mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	switch (width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		half_width = 10;
		break;
	case CHAN_WIDTH_40:
		half_width = 20;
		break;
	case CHAN_WIDTH_80:
	case CHAN_WIDTH_80P80:
		half_width = 40;
		break;
	case CHAN_WIDTH_160:
		half_width = 80;
		break;
	default:
		wpa_printf(MSG_WARNING, "DFS chanwidth %d not supported",
			   width);
		return 0;
	}

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;

		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_AVAILABLE)
			continue;

		if (center_freq - chan->freq < half_width &&
		    chan->freq - center_freq < half_width)
			res++;
	}

	wpa_printf(MSG_DEBUG, "DFS CAC required: (%d, %d): in range: %s",
		   center_freq - half_width, center_freq + half_width,
		   res ? "yes" : "no");

	return res;
}
