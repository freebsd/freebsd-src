/*
 * Operating classes
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
#include "common/ieee802_11_common.h"
#include "wpa_supplicant_i.h"
#include "bss.h"


static enum chan_allowed allow_channel(struct hostapd_hw_modes *mode,
				       u8 op_class, u8 chan,
				       unsigned int *flags)
{
	int i;
	bool is_6ghz = op_class >= 131 && op_class <= 136;

	for (i = 0; i < mode->num_channels; i++) {
		bool chan_is_6ghz;

		chan_is_6ghz = mode->channels[i].freq >= 5935 &&
			mode->channels[i].freq <= 7115;
		if (is_6ghz == chan_is_6ghz && mode->channels[i].chan == chan)
			break;
	}

	if (i == mode->num_channels ||
	    (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED))
		return NOT_ALLOWED;

	if (flags)
		*flags = mode->channels[i].flag;

	if (mode->channels[i].flag & HOSTAPD_CHAN_NO_IR)
		return NO_IR;

	return ALLOWED;
}


static int get_center_80mhz(struct hostapd_hw_modes *mode, u8 channel,
			    const u8 *center_channels, size_t num_chan)
{
	size_t i;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	for (i = 0; i < num_chan; i++) {
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


static enum chan_allowed verify_80mhz(struct hostapd_hw_modes *mode,
				      u8 op_class, u8 channel)
{
	u8 center_chan;
	unsigned int i;
	unsigned int no_ir = 0;
	const u8 *center_channels;
	size_t num_chan;
	const u8 center_channels_5ghz[] = { 42, 58, 106, 122, 138, 155, 171 };
	const u8 center_channels_6ghz[] = { 7, 23, 39, 55, 71, 87, 103, 119,
					    135, 151, 167, 183, 199, 215 };

	if (is_6ghz_op_class(op_class)) {
		center_channels = center_channels_6ghz;
		num_chan = ARRAY_SIZE(center_channels_6ghz);
	} else {
		center_channels = center_channels_5ghz;
		num_chan = ARRAY_SIZE(center_channels_5ghz);
	}

	center_chan = get_center_80mhz(mode, channel, center_channels,
				       num_chan);
	if (!center_chan)
		return NOT_ALLOWED;

	/* check all the channels are available */
	for (i = 0; i < 4; i++) {
		unsigned int flags;
		u8 adj_chan = center_chan - 6 + i * 4;

		if (allow_channel(mode, op_class, adj_chan, &flags) ==
		    NOT_ALLOWED)
			return NOT_ALLOWED;

		if ((i == 0 && !(flags & HOSTAPD_CHAN_VHT_10_70)) ||
		    (i == 1 && !(flags & HOSTAPD_CHAN_VHT_30_50)) ||
		    (i == 2 && !(flags & HOSTAPD_CHAN_VHT_50_30)) ||
		    (i == 3 && !(flags & HOSTAPD_CHAN_VHT_70_10)))
			return NOT_ALLOWED;

		if (flags & HOSTAPD_CHAN_NO_IR)
			no_ir = 1;
	}

	if (no_ir)
		return NO_IR;

	return ALLOWED;
}


static int get_center_160mhz(struct hostapd_hw_modes *mode, u8 channel,
			     const u8 *center_channels, size_t num_chan)
{
	unsigned int i;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	for (i = 0; i < num_chan; i++) {
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
				       u8 op_class, u8 channel)
{
	u8 center_chan;
	unsigned int i;
	unsigned int no_ir = 0;
	const u8 *center_channels;
	size_t num_chan;
	const u8 center_channels_5ghz[] = { 50, 114, 163 };
	const u8 center_channels_6ghz[] = { 15, 47, 79, 111, 143, 175, 207 };

	if (is_6ghz_op_class(op_class)) {
		center_channels = center_channels_6ghz;
		num_chan = ARRAY_SIZE(center_channels_6ghz);
	} else {
		center_channels = center_channels_5ghz;
		num_chan = ARRAY_SIZE(center_channels_5ghz);
	}

	center_chan = get_center_160mhz(mode, channel, center_channels,
					num_chan);
	if (!center_chan)
		return NOT_ALLOWED;

	/* Check all the channels are available */
	for (i = 0; i < 8; i++) {
		unsigned int flags;
		u8 adj_chan = center_chan - 14 + i * 4;

		if (allow_channel(mode, op_class, adj_chan, &flags) ==
		    NOT_ALLOWED)
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

		if (flags & HOSTAPD_CHAN_NO_IR)
			no_ir = 1;
	}

	if (no_ir)
		return NO_IR;

	return ALLOWED;
}


enum chan_allowed verify_channel(struct hostapd_hw_modes *mode, u8 op_class,
				 u8 channel, u8 bw)
{
	unsigned int flag = 0;
	enum chan_allowed res, res2;

	res2 = res = allow_channel(mode, op_class, channel, &flag);
	if (bw == BW40MINUS || (bw == BW40 && (((channel - 1) / 4) % 2))) {
		if (!(flag & HOSTAPD_CHAN_HT40MINUS))
			return NOT_ALLOWED;
		res2 = allow_channel(mode, op_class, channel - 4, NULL);
	} else if (bw == BW40PLUS ||
		   (bw == BW40 && !(((channel - 1) / 4) % 2))) {
		if (!(flag & HOSTAPD_CHAN_HT40PLUS))
			return NOT_ALLOWED;
		res2 = allow_channel(mode, op_class, channel + 4, NULL);
	} else if (bw == BW80) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 80 MHz specific version.
		 */
		res2 = res = verify_80mhz(mode, op_class, channel);
	} else if (bw == BW160) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 160 MHz specific version.
		 */
		res2 = res = verify_160mhz(mode, op_class, channel);
	} else if (bw == BW80P80) {
		/*
		 * channel is a center channel and as such, not necessarily a
		 * valid 20 MHz channels. Override earlier allow_channel()
		 * result and use only the 80 MHz specific version.
		 */
		res2 = res = verify_80mhz(mode, op_class, channel);
	}

	if (res == NOT_ALLOWED || res2 == NOT_ALLOWED)
		return NOT_ALLOWED;

	if (res == NO_IR || res2 == NO_IR)
		return NO_IR;

	return ALLOWED;
}


static int wpas_op_class_supported(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid,
				   const struct oper_class_map *op_class)
{
	int chan;
	size_t i;
	struct hostapd_hw_modes *mode;
	int found;
	int z;
	int freq2 = 0;
	int freq5 = 0;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, op_class->mode,
			is_6ghz_op_class(op_class->op_class));
	if (!mode)
		return 0;

	/* If we are configured to disable certain things, take that into
	 * account here. */
	if (ssid && ssid->freq_list && ssid->freq_list[0]) {
		for (z = 0; ; z++) {
			int f = ssid->freq_list[z];

			if (f == 0)
				break; /* end of list */
			if (f > 4000 && f < 6000)
				freq5 = 1;
			else if (f > 2400 && f < 2500)
				freq2 = 1;
		}
	} else {
		/* No frequencies specified, can use anything hardware supports.
		 */
		freq2 = freq5 = 1;
	}

	if (op_class->op_class >= 115 && op_class->op_class <= 130 && !freq5)
		return 0;
	if (op_class->op_class >= 81 && op_class->op_class <= 84 && !freq2)
		return 0;

#ifdef CONFIG_HT_OVERRIDES
	if (ssid && ssid->disable_ht) {
		switch (op_class->op_class) {
		case 83:
		case 84:
		case 104:
		case 105:
		case 116:
		case 117:
		case 119:
		case 120:
		case 122:
		case 123:
		case 126:
		case 127:
		case 128:
		case 129:
		case 130:
			/* Disable >= 40 MHz channels if HT is disabled */
			return 0;
		}
	}
#endif /* CONFIG_HT_OVERRIDES */

#ifdef CONFIG_VHT_OVERRIDES
	if (ssid && ssid->disable_vht) {
		if (op_class->op_class >= 128 && op_class->op_class <= 130) {
			/* Disable >= 80 MHz channels if VHT is disabled */
			return 0;
		}
	}
#endif /* CONFIG_VHT_OVERRIDES */

	if (op_class->op_class == 128) {
		u8 channels[] = { 42, 58, 106, 122, 138, 155, 171 };

		for (i = 0; i < ARRAY_SIZE(channels); i++) {
			if (verify_channel(mode, op_class->op_class,
					   channels[i], op_class->bw) !=
			    NOT_ALLOWED)
				return 1;
		}

		return 0;
	}

	if (op_class->op_class == 129) {
		/* Check if either 160 MHz channels is allowed */
		return verify_channel(mode, op_class->op_class, 50,
				      op_class->bw) != NOT_ALLOWED ||
			verify_channel(mode, op_class->op_class, 114,
				       op_class->bw) != NOT_ALLOWED ||
			verify_channel(mode, op_class->op_class, 163,
				       op_class->bw) != NOT_ALLOWED;
	}

	if (op_class->op_class == 130) {
		/* Need at least two non-contiguous 80 MHz segments */
		found = 0;

		if (verify_channel(mode, op_class->op_class, 42,
				   op_class->bw) != NOT_ALLOWED ||
		    verify_channel(mode, op_class->op_class, 58,
				   op_class->bw) != NOT_ALLOWED)
			found++;
		if (verify_channel(mode, op_class->op_class, 106,
				   op_class->bw) != NOT_ALLOWED ||
		    verify_channel(mode, op_class->op_class, 122,
				   op_class->bw) != NOT_ALLOWED ||
		    verify_channel(mode, op_class->op_class, 138,
				   op_class->bw) != NOT_ALLOWED ||
		    verify_channel(mode, op_class->op_class, 155,
				   op_class->bw) != NOT_ALLOWED ||
		    verify_channel(mode, op_class->op_class, 171,
				   op_class->bw) != NOT_ALLOWED)
			found++;
		if (verify_channel(mode, op_class->op_class, 106,
				   op_class->bw) != NOT_ALLOWED &&
		    verify_channel(mode, op_class->op_class, 138,
				   op_class->bw) != NOT_ALLOWED)
			found++;
		if (verify_channel(mode, op_class->op_class, 122,
				   op_class->bw) != NOT_ALLOWED &&
		    verify_channel(mode, op_class->op_class, 155,
				   op_class->bw) != NOT_ALLOWED)
			found++;
		if (verify_channel(mode, op_class->op_class, 138,
				   op_class->bw) != NOT_ALLOWED &&
		    verify_channel(mode, op_class->op_class, 171,
				   op_class->bw) != NOT_ALLOWED)
			found++;

		if (found >= 2)
			return 1;

		return 0;
	}

	if (op_class->op_class == 135) {
		/* Need at least two 80 MHz segments which do not fall under the
		 * same 160 MHz segment to support 80+80 in 6 GHz.
		 */
		int first_seg = 0;
		int curr_seg = 0;

		for (chan = op_class->min_chan; chan <= op_class->max_chan;
		     chan += op_class->inc) {
			curr_seg++;
			if (verify_channel(mode, op_class->op_class, chan,
					   op_class->bw) != NOT_ALLOWED) {
				if (!first_seg) {
					first_seg = curr_seg;
					continue;
				}

				/* Supported if at least two non-consecutive 80
				 * MHz segments allowed.
				 */
				if ((curr_seg - first_seg) > 1)
					return 1;

				/* Supported even if the 80 MHz segments are
				 * consecutive when they do not fall under the
				 * same 160 MHz segment.
				 */
				if ((first_seg % 2) == 0)
					return 1;
			}
		}

		return 0;
	}

	found = 0;
	for (chan = op_class->min_chan; chan <= op_class->max_chan;
	     chan += op_class->inc) {
		if (verify_channel(mode, op_class->op_class, chan,
				   op_class->bw) != NOT_ALLOWED) {
			found = 1;
			break;
		}
	}

	return found;
}


static int wpas_sta_secondary_channel_offset(struct wpa_bss *bss, u8 *current,
					     u8 *channel)
{

	const u8 *ies;
	u8 phy_type;
	size_t ies_len;

	if (!bss)
		return -1;
	ies = wpa_bss_ie_ptr(bss);
	ies_len = bss->ie_len ? bss->ie_len : bss->beacon_ie_len;
	return wpas_get_op_chan_phy(bss->freq, ies, ies_len, current,
				    channel, &phy_type);
}


size_t wpas_supp_op_class_ie(struct wpa_supplicant *wpa_s,
			     struct wpa_ssid *ssid,
			     struct wpa_bss *bss, u8 *pos, size_t len)
{
	struct wpabuf *buf;
	u8 op, current, chan;
	u8 *ie_len;
	size_t res;

	/*
	 * Determine the current operating class correct mode based on
	 * advertised BSS capabilities, if available. Fall back to a less
	 * accurate guess based on frequency if the needed IEs are not available
	 * or used.
	 */
	if (wpas_sta_secondary_channel_offset(bss, &current, &chan) < 0 &&
	    ieee80211_freq_to_channel_ext(bss->freq, 0, CHANWIDTH_USE_HT,
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
		if (wpas_op_class_supported(wpa_s, ssid, &global_op_class[op]))
			wpabuf_put_u8(buf, global_op_class[op].op_class);
	}

	*ie_len = wpabuf_len(buf) - 2;
	if (*ie_len < 2) {
		wpa_printf(MSG_DEBUG,
			   "No supported operating classes IE to add");
		res = 0;
	} else if (wpabuf_len(buf) > len) {
		wpa_printf(MSG_ERROR,
			   "Supported operating classes IE exceeds maximum buffer length");
		res = 0;
	} else {
		os_memcpy(pos, wpabuf_head(buf), wpabuf_len(buf));
		res = wpabuf_len(buf);
		wpa_hexdump_buf(MSG_DEBUG,
				"Added supported operating classes IE", buf);
	}

	wpabuf_free(buf);
	return res;
}


int * wpas_supp_op_classes(struct wpa_supplicant *wpa_s)
{
	int op;
	unsigned int pos, max_num = 0;
	int *classes;

	for (op = 0; global_op_class[op].op_class; op++)
		max_num++;
	classes = os_zalloc((max_num + 1) * sizeof(int));
	if (!classes)
		return NULL;

	for (op = 0, pos = 0; global_op_class[op].op_class; op++) {
		if (wpas_op_class_supported(wpa_s, NULL, &global_op_class[op]))
			classes[pos++] = global_op_class[op].op_class;
	}

	return classes;
}
