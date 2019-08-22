/*
 * hostapd / IEEE 802.11ax HE
 * Copyright (c) 2016-2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2019 John Crispin <john@phrozen.org>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "ap_config.h"
#include "beacon.h"
#include "sta_info.h"
#include "ieee802_11.h"
#include "dfs.h"

static u8 ieee80211_he_ppet_size(u8 ppe_thres_hdr, const u8 *phy_cap_info)
{
	u8 sz = 0, ru;

	if ((phy_cap_info[HE_PHYCAP_PPE_THRESHOLD_PRESENT_IDX] &
	     HE_PHYCAP_PPE_THRESHOLD_PRESENT) == 0)
		return 0;

	ru = (ppe_thres_hdr >> HE_PPE_THRES_RU_INDEX_BITMASK_SHIFT) &
		HE_PPE_THRES_RU_INDEX_BITMASK_MASK;
	while (ru) {
		if (ru & 0x1)
			sz++;
		ru >>= 1;
	}

	sz *= 1 + (ppe_thres_hdr & HE_PPE_THRES_NSS_MASK);
	sz = (sz * 6) + 7;
	if (sz % 8)
		sz += 8;
	sz /= 8;

	return sz;
}


u8 * hostapd_eid_he_capab(struct hostapd_data *hapd, u8 *eid,
			  enum ieee80211_op_mode opmode)
{
	struct ieee80211_he_capabilities *cap;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;
	u8 he_oper_chwidth = ~HE_PHYCAP_CHANNEL_WIDTH_MASK;
	u8 *pos = eid;
	u8 ie_size = 0, mcs_nss_size = 0, ppet_size = 0;

	if (!mode)
		return eid;

	ie_size = sizeof(struct ieee80211_he_capabilities);
	ppet_size = ieee80211_he_ppet_size(mode->he_capab[opmode].ppet[0],
					   mode->he_capab[opmode].phy_cap);

	switch (hapd->iface->conf->he_oper_chwidth) {
	case CHANWIDTH_80P80MHZ:
		he_oper_chwidth |=
			HE_PHYCAP_CHANNEL_WIDTH_SET_80PLUS80MHZ_IN_5G;
		mcs_nss_size += 4;
		/* fall through */
	case CHANWIDTH_160MHZ:
		he_oper_chwidth |= HE_PHYCAP_CHANNEL_WIDTH_SET_160MHZ_IN_5G;
		mcs_nss_size += 4;
		/* fall through */
	case CHANWIDTH_80MHZ:
	case CHANWIDTH_USE_HT:
		he_oper_chwidth |= HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_IN_2G |
			HE_PHYCAP_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G;
		mcs_nss_size += 4;
		break;
	}

	ie_size += mcs_nss_size + ppet_size;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + ie_size;
	*pos++ = WLAN_EID_EXT_HE_CAPABILITIES;

	cap = (struct ieee80211_he_capabilities *) pos;
	os_memset(cap, 0, sizeof(*cap));

	os_memcpy(cap->he_mac_capab_info, mode->he_capab[opmode].mac_cap,
		  HE_MAX_MAC_CAPAB_SIZE);
	os_memcpy(cap->he_phy_capab_info, mode->he_capab[opmode].phy_cap,
		  HE_MAX_PHY_CAPAB_SIZE);
	os_memcpy(cap->optional, mode->he_capab[opmode].mcs, mcs_nss_size);
	if (ppet_size)
		os_memcpy(&cap->optional[mcs_nss_size],
			  mode->he_capab[opmode].ppet,  ppet_size);

	if (hapd->iface->conf->he_phy_capab.he_su_beamformer)
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMER_CAPAB_IDX] |=
			HE_PHYCAP_SU_BEAMFORMER_CAPAB;
	else
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMER_CAPAB_IDX] &=
			~HE_PHYCAP_SU_BEAMFORMER_CAPAB;

	if (hapd->iface->conf->he_phy_capab.he_su_beamformee)
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMEE_CAPAB_IDX] |=
			HE_PHYCAP_SU_BEAMFORMEE_CAPAB;
	else
		cap->he_phy_capab_info[HE_PHYCAP_SU_BEAMFORMEE_CAPAB_IDX] &=
			~HE_PHYCAP_SU_BEAMFORMEE_CAPAB;

	if (hapd->iface->conf->he_phy_capab.he_mu_beamformer)
		cap->he_phy_capab_info[HE_PHYCAP_MU_BEAMFORMER_CAPAB_IDX] |=
			HE_PHYCAP_MU_BEAMFORMER_CAPAB;
	else
		cap->he_phy_capab_info[HE_PHYCAP_MU_BEAMFORMER_CAPAB_IDX] &=
			~HE_PHYCAP_MU_BEAMFORMER_CAPAB;

	cap->he_phy_capab_info[HE_PHYCAP_CHANNEL_WIDTH_SET_IDX] &=
		he_oper_chwidth;

	pos += ie_size;

	return pos;
}


u8 * hostapd_eid_he_operation(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_he_operation *oper;
	u8 *pos = eid;
	int oper_size = 6;
	u32 params = 0;

	if (!hapd->iface->current_mode)
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + oper_size;
	*pos++ = WLAN_EID_EXT_HE_OPERATION;

	oper = (struct ieee80211_he_operation *) pos;
	os_memset(oper, 0, sizeof(*oper));

	if (hapd->iface->conf->he_op.he_default_pe_duration)
		params |= (hapd->iface->conf->he_op.he_default_pe_duration <<
			   HE_OPERATION_DFLT_PE_DURATION_OFFSET);

	if (hapd->iface->conf->he_op.he_twt_required)
		params |= HE_OPERATION_TWT_REQUIRED;

	if (hapd->iface->conf->he_op.he_rts_threshold)
		params |= (hapd->iface->conf->he_op.he_rts_threshold <<
			   HE_OPERATION_RTS_THRESHOLD_OFFSET);

	if (hapd->iface->conf->he_op.he_bss_color)
		params |= (hapd->iface->conf->he_op.he_bss_color <<
			   HE_OPERATION_BSS_COLOR_OFFSET);

	/* HE minimum required basic MCS and NSS for STAs */
	oper->he_mcs_nss_set =
		host_to_le16(hapd->iface->conf->he_op.he_basic_mcs_nss_set);

	/* TODO: conditional MaxBSSID Indicator subfield */

	oper->he_oper_params = host_to_le32(params);

	pos += oper_size;

	return pos;
}


u8 * hostapd_eid_he_mu_edca_parameter_set(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_he_mu_edca_parameter_set *edca;
	u8 *pos;
	size_t i;

	pos = (u8 *) &hapd->iface->conf->he_mu_edca;
	for (i = 0; i < sizeof(*edca); i++) {
		if (pos[i])
			break;
	}
	if (i == sizeof(*edca))
		return eid; /* no MU EDCA Parameters configured */

	pos = eid;
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + sizeof(*edca);
	*pos++ = WLAN_EID_EXT_HE_MU_EDCA_PARAMS;

	edca = (struct ieee80211_he_mu_edca_parameter_set *) pos;
	os_memcpy(edca, &hapd->iface->conf->he_mu_edca, sizeof(*edca));

	wpa_hexdump(MSG_DEBUG, "HE: MU EDCA Parameter Set element",
		    pos, sizeof(*edca));

	pos += sizeof(*edca);

	return pos;
}


u8 * hostapd_eid_spatial_reuse(struct hostapd_data *hapd, u8 *eid)
{
	struct ieee80211_spatial_reuse *spr;
	u8 *pos = eid, *spr_param;
	u8 sz = 1;

	if (!hapd->iface->conf->spr.sr_control)
		return eid;

	if (hapd->iface->conf->spr.sr_control &
	    SPATIAL_REUSE_NON_SRG_OFFSET_PRESENT)
		sz++;

	if (hapd->iface->conf->spr.sr_control &
	    SPATIAL_REUSE_SRG_INFORMATION_PRESENT)
		sz += 18;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 1 + sz;
	*pos++ = WLAN_EID_EXT_SPATIAL_REUSE;

	spr = (struct ieee80211_spatial_reuse *) pos;
	os_memset(spr, 0, sizeof(*spr));

	spr->sr_ctrl = hapd->iface->conf->spr.sr_control;
	pos++;
	spr_param = spr->params;
	if (spr->sr_ctrl & SPATIAL_REUSE_NON_SRG_OFFSET_PRESENT) {
		*spr_param++ =
			hapd->iface->conf->spr.non_srg_obss_pd_max_offset;
		pos++;
	}
	if (spr->sr_ctrl & SPATIAL_REUSE_SRG_INFORMATION_PRESENT) {
		*spr_param++ = hapd->iface->conf->spr.srg_obss_pd_min_offset;
		*spr_param++ = hapd->iface->conf->spr.srg_obss_pd_max_offset;
		pos += 18;
	}

	return pos;
}


void hostapd_get_he_capab(struct hostapd_data *hapd,
			  const struct ieee80211_he_capabilities *he_cap,
			  struct ieee80211_he_capabilities *neg_he_cap,
			  size_t he_capab_len)
{
	if (!he_cap)
		return;

	if (he_capab_len > sizeof(*neg_he_cap))
		he_capab_len = sizeof(*neg_he_cap);
	/* TODO: mask out unsupported features */

	os_memcpy(neg_he_cap, he_cap, he_capab_len);
}


static int check_valid_he_mcs(struct hostapd_data *hapd, const u8 *sta_he_capab,
			      enum ieee80211_op_mode opmode)
{
	u16 sta_rx_mcs_set, ap_tx_mcs_set;
	u8 mcs_count = 0;
	const u16 *ap_mcs_set, *sta_mcs_set;
	int i;

	if (!hapd->iface->current_mode)
		return 1;
	ap_mcs_set = (u16 *) hapd->iface->current_mode->he_capab[opmode].mcs;
	sta_mcs_set = (u16 *) ((const struct ieee80211_he_capabilities *)
			       sta_he_capab)->optional;

	/*
	 * Disable HE capabilities for STAs for which there is not even a single
	 * allowed MCS in any supported number of streams, i.e., STA is
	 * advertising 3 (not supported) as HE MCS rates for all supported
	 * band/stream cases.
	 */
	switch (hapd->iface->conf->he_oper_chwidth) {
	case CHANWIDTH_80P80MHZ:
		mcs_count = 3;
		break;
	case CHANWIDTH_160MHZ:
		mcs_count = 2;
		break;
	default:
		mcs_count = 1;
		break;
	}

	for (i = 0; i < mcs_count; i++) {
		int j;

		/* AP Tx MCS map vs. STA Rx MCS map */
		sta_rx_mcs_set = WPA_GET_LE16((const u8 *) &sta_mcs_set[i * 2]);
		ap_tx_mcs_set = WPA_GET_LE16((const u8 *)
					     &ap_mcs_set[(i * 2) + 1]);

		for (j = 0; j < HE_NSS_MAX_STREAMS; j++) {
			if (((ap_tx_mcs_set >> (j * 2)) & 0x3) == 3)
				continue;

			if (((sta_rx_mcs_set >> (j * 2)) & 0x3) == 3)
				continue;

			return 1;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "No matching HE MCS found between AP TX and STA RX");

	return 0;
}


u16 copy_sta_he_capab(struct hostapd_data *hapd, struct sta_info *sta,
		      enum ieee80211_op_mode opmode, const u8 *he_capab,
		      size_t he_capab_len)
{
	if (!he_capab || !hapd->iconf->ieee80211ax ||
	    !check_valid_he_mcs(hapd, he_capab, opmode) ||
	    he_capab_len > sizeof(struct ieee80211_he_capabilities)) {
		sta->flags &= ~WLAN_STA_HE;
		os_free(sta->he_capab);
		sta->he_capab = NULL;
		return WLAN_STATUS_SUCCESS;
	}

	if (!sta->he_capab) {
		sta->he_capab =
			os_zalloc(sizeof(struct ieee80211_he_capabilities));
		if (!sta->he_capab)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->flags |= WLAN_STA_HE;
	os_memset(sta->he_capab, 0, sizeof(struct ieee80211_he_capabilities));
	os_memcpy(sta->he_capab, he_capab, he_capab_len);
	sta->he_capab_len = he_capab_len;

	return WLAN_STATUS_SUCCESS;
}
