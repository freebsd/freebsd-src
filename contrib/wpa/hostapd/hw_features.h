/*
 * hostapd / Hardware feature query and different modes
 * Copyright 2002-2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef HW_FEATURES_H
#define HW_FEATURES_H

#define HOSTAPD_CHAN_DISABLED 0x00000001
#define HOSTAPD_CHAN_PASSIVE_SCAN 0x00000002
#define HOSTAPD_CHAN_NO_IBSS 0x00000004
#define HOSTAPD_CHAN_RADAR 0x00000008

struct hostapd_channel_data {
	short chan; /* channel number (IEEE 802.11) */
	short freq; /* frequency in MHz */
	int flag; /* flag for hostapd use (HOSTAPD_CHAN_*) */
	u8 max_tx_power; /* maximum transmit power in dBm */
};

#define HOSTAPD_RATE_ERP 0x00000001
#define HOSTAPD_RATE_BASIC 0x00000002
#define HOSTAPD_RATE_PREAMBLE2 0x00000004
#define HOSTAPD_RATE_SUPPORTED 0x00000010
#define HOSTAPD_RATE_OFDM 0x00000020
#define HOSTAPD_RATE_CCK 0x00000040
#define HOSTAPD_RATE_MANDATORY 0x00000100

struct hostapd_rate_data {
	int rate; /* rate in 100 kbps */
	int flags; /* HOSTAPD_RATE_ flags */
};

struct hostapd_hw_modes {
	int mode;
	int num_channels;
	struct hostapd_channel_data *channels;
	int num_rates;
	struct hostapd_rate_data *rates;
	u16 ht_capab;
};


void hostapd_free_hw_features(struct hostapd_hw_modes *hw_features,
			      size_t num_hw_features);
int hostapd_get_hw_features(struct hostapd_iface *iface);
int hostapd_select_hw_mode(struct hostapd_iface *iface);
const char * hostapd_hw_mode_txt(int mode);
int hostapd_hw_get_freq(struct hostapd_data *hapd, int chan);
int hostapd_hw_get_channel(struct hostapd_data *hapd, int freq);

#endif /* HW_FEATURES_H */
