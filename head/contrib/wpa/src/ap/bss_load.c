/*
 * BSS Load Element / Channel Utilization
 * Copyright (c) 2014, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "hostapd.h"
#include "bss_load.h"
#include "ap_drv_ops.h"
#include "beacon.h"


static void update_channel_utilization(void *eloop_data, void *user_data)
{
	struct hostapd_data *hapd = eloop_data;
	unsigned int sec, usec;
	int err;

	if (!(hapd->beacon_set_done && hapd->started))
		return;

	err = hostapd_drv_get_survey(hapd, hapd->iface->freq);
	if (err) {
		wpa_printf(MSG_ERROR, "BSS Load: Failed to get survey data");
		return;
	}

	ieee802_11_set_beacon(hapd);

	sec = ((hapd->bss_load_update_timeout / 1000) * 1024) / 1000;
	usec = (hapd->bss_load_update_timeout % 1000) * 1024;
	eloop_register_timeout(sec, usec, update_channel_utilization, hapd,
			       NULL);
}


int bss_load_update_init(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	struct hostapd_config *iconf = hapd->iconf;
	unsigned int sec, usec;

	if (!conf->bss_load_update_period || !iconf->beacon_int)
		return -1;

	hapd->bss_load_update_timeout = conf->bss_load_update_period *
					iconf->beacon_int;
	sec = ((hapd->bss_load_update_timeout / 1000) * 1024) / 1000;
	usec = (hapd->bss_load_update_timeout % 1000) * 1024;
	eloop_register_timeout(sec, usec, update_channel_utilization, hapd,
			       NULL);
	return 0;
}


void bss_load_update_deinit(struct hostapd_data *hapd)
{
	eloop_cancel_timeout(update_channel_utilization, hapd, NULL);
}
