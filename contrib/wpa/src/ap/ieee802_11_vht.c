/*
 * hostapd / IEEE 802.11ac VHT
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of BSD license
 *
 * See README and COPYING for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "hostapd.h"
#include "ap_config.h"
#include "sta_info.h"
#include "beacon.h"
#include "ieee802_11.h"


u8 * hostapd_eid_vht_capabilities(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_vht_capabilities *cap;
	u8 *pos = eid;

	if (!hapd->iconf->ieee80211ac || !hapd->iface->current_mode ||
	    hapd->conf->disable_11ac)
		return eid;

	*pos++ = WLAN_EID_VHT_CAP;
	*pos++ = sizeof(*cap);

	cap = (struct ieee80211_vht_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));
	cap->vht_capabilities_info = host_to_le32(
		hapd->iface->current_mode->vht_capab);

	/* Supported MCS set comes from hw */
	os_memcpy(cap->vht_supported_mcs_set,
	          hapd->iface->current_mode->vht_mcs_set, 8);

	pos += sizeof(*cap);

	return pos;
}


u8 * hostapd_eid_vht_operation(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_vht_operation *oper;
	u8 *pos = eid;

	if (!hapd->iconf->ieee80211ac || hapd->conf->disable_11ac)
		return eid;

	*pos++ = WLAN_EID_VHT_OPERATION;
	*pos++ = sizeof(*oper);

	oper = (struct ieee80211_vht_operation *) pos;
	os_memset(oper, 0, sizeof(*oper));

	/*
	 * center freq = 5 GHz + (5 * index)
	 * So index 42 gives center freq 5.210 GHz
	 * which is channel 42 in 5G band
	 */
	oper->vht_op_info_chan_center_freq_seg0_idx =
		hapd->iconf->vht_oper_centr_freq_seg0_idx;
	oper->vht_op_info_chan_center_freq_seg1_idx =
		hapd->iconf->vht_oper_centr_freq_seg1_idx;

	oper->vht_op_info_chwidth = hapd->iconf->vht_oper_chwidth;

	/* VHT Basic MCS set comes from hw */
	/* Hard code 1 stream, MCS0-7 is a min Basic VHT MCS rates */
	oper->vht_basic_mcs_set = host_to_le16(0xfffc);
	pos += sizeof(*oper);

	return pos;
}


u16 copy_sta_vht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_capab, size_t vht_capab_len)
{
	/* Disable VHT caps for STAs associated to no-VHT BSSes. */
	if (!vht_capab ||
	    vht_capab_len < sizeof(struct ieee80211_vht_capabilities) ||
	    hapd->conf->disable_11ac) {
		sta->flags &= ~WLAN_STA_VHT;
		os_free(sta->vht_capabilities);
		sta->vht_capabilities = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (sta->vht_capabilities == NULL) {
		sta->vht_capabilities =
			os_zalloc(sizeof(struct ieee80211_vht_capabilities));
		if (sta->vht_capabilities == NULL)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_VHT;
	os_memcpy(sta->vht_capabilities, vht_capab,
		  sizeof(struct ieee80211_vht_capabilities));

	return WLAN_STATUS_SUCCESS;
}
