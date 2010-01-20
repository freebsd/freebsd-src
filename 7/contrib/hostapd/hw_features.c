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

#include "includes.h"

#include "hostapd.h"
#include "hw_features.h"
#include "driver.h"
#include "config.h"
#include "ieee802_11.h"
#include "eloop.h"


void hostapd_free_hw_features(struct hostapd_hw_modes *hw_features,
			      size_t num_hw_features)
{
	size_t i;

	if (hw_features == NULL)
		return;

	for (i = 0; i < num_hw_features; i++) {
		free(hw_features[i].channels);
		free(hw_features[i].rates);
	}

	free(hw_features);
}


int hostapd_get_hw_features(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int ret = 0, i, j;
	u16 num_modes, flags;
	struct hostapd_hw_modes *modes;

	modes = hostapd_get_hw_feature_data(hapd, &num_modes, &flags);
	if (modes == NULL) {
		hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Fetching hardware channel/rate support not "
			       "supported.");
		return -1;
	}

	iface->hw_flags = flags;

	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = modes;
	iface->num_hw_features = num_modes;

	for (i = 0; i < num_modes; i++) {
		struct hostapd_hw_modes *feature = &modes[i];
		/* set flag for channels we can use in current regulatory
		 * domain */
		for (j = 0; j < feature->num_channels; j++) {
			/* TODO: add regulatory domain lookup */
			unsigned char power_level = 0;
			unsigned char antenna_max = 0;

			if ((feature->mode == HOSTAPD_MODE_IEEE80211G ||
			     feature->mode == HOSTAPD_MODE_IEEE80211B) &&
			    feature->channels[j].chan >= 1 &&
			    feature->channels[j].chan <= 11) {
				power_level = 20;
				feature->channels[j].flag |=
					HOSTAPD_CHAN_W_SCAN;
			} else
				feature->channels[j].flag &=
					~HOSTAPD_CHAN_W_SCAN;

			hostapd_set_channel_flag(hapd, feature->mode,
						 feature->channels[j].chan,
						 feature->channels[j].flag,
						 power_level,
						 antenna_max);
		}
	}

	return ret;
}


static int hostapd_prepare_rates(struct hostapd_data *hapd,
				 struct hostapd_hw_modes *mode)
{
	int i, num_basic_rates = 0;
	int basic_rates_a[] = { 60, 120, 240, -1 };
	int basic_rates_b[] = { 10, 20, -1 };
	int basic_rates_g[] = { 10, 20, 55, 110, -1 };
	int *basic_rates;

	if (hapd->iconf->basic_rates)
		basic_rates = hapd->iconf->basic_rates;
	else switch (mode->mode) {
	case HOSTAPD_MODE_IEEE80211A:
		basic_rates = basic_rates_a;
		break;
	case HOSTAPD_MODE_IEEE80211B:
		basic_rates = basic_rates_b;
		break;
	case HOSTAPD_MODE_IEEE80211G:
		basic_rates = basic_rates_g;
		break;
	default:
		return -1;
	}

	if (hostapd_set_rate_sets(hapd, hapd->iconf->supported_rates,
				  basic_rates, mode->mode)) {
		printf("Failed to update rate sets in kernel module\n");
	}

	free(hapd->iface->current_rates);
	hapd->iface->num_rates = 0;

	hapd->iface->current_rates =
		malloc(mode->num_rates * sizeof(struct hostapd_rate_data));
	if (!hapd->iface->current_rates) {
		printf("Failed to allocate memory for rate table.\n");
		return -1;
	}

	for (i = 0; i < mode->num_rates; i++) {
		struct hostapd_rate_data *rate;

		if (hapd->iconf->supported_rates &&
		    !hostapd_rate_found(hapd->iconf->supported_rates,
					mode->rates[i].rate))
			continue;

		rate = &hapd->iface->current_rates[hapd->iface->num_rates];
		memcpy(rate, &mode->rates[i],
		       sizeof(struct hostapd_rate_data));
		if (hostapd_rate_found(basic_rates, rate->rate)) {
			rate->flags |= HOSTAPD_RATE_BASIC;
			num_basic_rates++;
		} else
			rate->flags &= ~HOSTAPD_RATE_BASIC;
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "RATE[%d] rate=%d flags=0x%x\n",
			      hapd->iface->num_rates, rate->rate, rate->flags);
		hapd->iface->num_rates++;
	}

	if (hapd->iface->num_rates == 0 || num_basic_rates == 0) {
		printf("No rates remaining in supported/basic rate sets "
		       "(%d,%d).\n", hapd->iface->num_rates, num_basic_rates);
		return -1;
	}

	return 0;
}


static void select_hw_mode_start(void *eloop_data, void *user_ctx);
static void select_hw_mode2_handler(void *eloop_data, void *user_ctx);

/**
 * select_hw_mode_finalize - Finish select HW mode & call the callback
 * @iface: Pointer to interface data.
 * @status: Status of the select HW mode (0 on success; -1 on failure).
 * Returns: 0 on success; -1 on failure (e.g., was not in progress).
 */
static int select_hw_mode_finalize(struct hostapd_iface *iface, int status)
{
	hostapd_iface_cb cb;

	if (!iface->hw_mode_sel_cb)
		return -1;

	eloop_cancel_timeout(select_hw_mode_start, iface, NULL);
	eloop_cancel_timeout(select_hw_mode2_handler, iface, NULL);

	cb = iface->hw_mode_sel_cb;

	iface->hw_mode_sel_cb = NULL;

	cb(iface, status);

	return 0;
}


/**
 * select_hw_mode2 - Select the hardware mode (part 2)
 * @iface: Pointer to interface data.
 * @status: Status of auto chanel selection.
 *
 * Setup the rates and passive scanning based on the configuration.
 */
static void select_hw_mode2(struct hostapd_iface *iface, int status)
{
	int ret = status;
	if (ret)
		goto fail;

	if (iface->current_mode == NULL) {
		hostapd_logger(iface->bss[0], NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "Hardware does not support configured channel");
		ret = -1;
		goto fail;
	}

	if (hostapd_prepare_rates(iface->bss[0], iface->current_mode)) {
		printf("Failed to prepare rates table.\n");
		hostapd_logger(iface->bss[0], NULL, HOSTAPD_MODULE_IEEE80211,
					   HOSTAPD_LEVEL_WARNING,
					   "Failed to prepare rates table.");
		ret = -1;
		goto fail;
	}

	ret = hostapd_passive_scan(iface->bss[0], 0,
				   iface->conf->passive_scan_mode,
				   iface->conf->passive_scan_interval,
				   iface->conf->passive_scan_listen,
				   NULL, NULL);
	if (ret) {
		printf("Could not set passive scanning: %s\n", strerror(ret));
		ret = 0;
	}

fail:
	select_hw_mode_finalize(iface, ret);
}


/**
 * select_hw_mode2_handler - Calls select_hw_mode2 when auto chan isn't used
 * @eloop_data: Stores the struct hostapd_iface * for the interface.
 * @user_ctx: Unused.
 */
static void select_hw_mode2_handler(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;

	select_hw_mode2(iface, 0);
}


/**
 * select_hw_mode1 - Select the hardware mode (part 1)
 * @iface: Pointer to interface data.
 * Returns: 0 on success; -1 on failure.
 *
 * Setup the hardware mode and channel based on the configuration.
 * Schedules select_hw_mode2() to be called immediately or after automatic
 * channel selection takes place.
 */
static int select_hw_mode1(struct hostapd_iface *iface)
{
	int i, j, ok;

	if (iface->num_hw_features < 1)
		return -1;

	iface->current_mode = NULL;
	for (i = 0; i < iface->num_hw_features; i++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[i];
		if (mode->mode == (int) iface->conf->hw_mode) {
			iface->current_mode = mode;
			break;
		}
	}

	if (iface->current_mode == NULL) {
		printf("Hardware does not support configured mode\n");
		hostapd_logger(iface->bss[0], NULL, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "Hardware does not support configured mode "
			       "(%d)", (int) iface->conf->hw_mode);
		return -1;
	}

	ok = 0;
	for (j = 0; j < iface->current_mode->num_channels; j++) {
		struct hostapd_channel_data *chan =
			&iface->current_mode->channels[j];
		if ((chan->flag & HOSTAPD_CHAN_W_SCAN) &&
		    (chan->chan == iface->conf->channel)) {
			ok = 1;
			break;
		}
	}
	if (ok == 0 && iface->conf->channel != 0) {
		hostapd_logger(iface->bss[0], NULL,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_WARNING,
			       "Configured channel (%d) not found from the "
			       "channel list of current mode (%d) %s",
			       iface->conf->channel,
			       iface->current_mode->mode,
			       hostapd_hw_mode_txt(iface->current_mode->mode));
		iface->current_mode = NULL;
	}

	/*
	 * Calls select_hw_mode2() via a handler, so that the function is
	 * always executed from eloop.
	 */
	eloop_register_timeout(0, 0, select_hw_mode2_handler, iface, NULL);
	return 0;
}


/**
 * select_hw_mode_start - Handler to start select HW mode
 * @eloop_data: Stores the struct hostapd_iface * for the interface.
 * @user_ctx: Unused.
 *
 * An eloop handler is used so that all errors can be processed by the
 * callback without introducing stack recursion.
 */
static void select_hw_mode_start(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = (struct hostapd_iface *)eloop_data;

	int ret;

	ret = select_hw_mode1(iface);
	if (ret)
		select_hw_mode_finalize(iface, ret);
}


/**
 * hostapd_select_hw_mode_start - Start selection of the hardware mode
 * @iface: Pointer to interface data.
 * @cb: The function to callback when done.
 * Returns:  0 if it starts successfully; cb will be called when done.
 *          -1 on failure; cb will not be called.
 *
 * Sets up the hardware mode, channel, rates, and passive scanning
 * based on the configuration.
 */
int hostapd_select_hw_mode_start(struct hostapd_iface *iface,
				 hostapd_iface_cb cb)
{
	if (iface->hw_mode_sel_cb) {
		wpa_printf(MSG_DEBUG,
			   "%s: Hardware mode select already in progress.",
			   iface->bss[0]->conf->iface);
		return -1;
	}

	iface->hw_mode_sel_cb = cb;

	eloop_register_timeout(0, 0, select_hw_mode_start, iface, NULL);

	return 0;
}


/**
 * hostapd_auto_chan_select_stop - Stops automatic channel selection
 * @iface: Pointer to interface data.
 * Returns:  0 if successfully stopped;
 *          -1 on failure (i.e., was not in progress)
 */
int hostapd_select_hw_mode_stop(struct hostapd_iface *iface)
{
	return select_hw_mode_finalize(iface, -1);
}


const char * hostapd_hw_mode_txt(int mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211A:
		return "IEEE 802.11a";
	case HOSTAPD_MODE_IEEE80211B:
		return "IEEE 802.11b";
	case HOSTAPD_MODE_IEEE80211G:
		return "IEEE 802.11g";
	default:
		return "UNKNOWN";
	}
}


int hostapd_hw_get_freq(struct hostapd_data *hapd, int chan)
{
	int i;

	if (!hapd->iface->current_mode)
		return 0;

	for (i = 0; i < hapd->iface->current_mode->num_channels; i++) {
		struct hostapd_channel_data *ch =
			&hapd->iface->current_mode->channels[i];
		if (ch->chan == chan)
			return ch->freq;
	}

	return 0;
}


int hostapd_hw_get_channel(struct hostapd_data *hapd, int freq)
{
	int i;

	if (!hapd->iface->current_mode)
		return 0;

	for (i = 0; i < hapd->iface->current_mode->num_channels; i++) {
		struct hostapd_channel_data *ch =
			&hapd->iface->current_mode->channels[i];
		if (ch->freq == freq)
			return ch->chan;
	}

	return 0;
}
