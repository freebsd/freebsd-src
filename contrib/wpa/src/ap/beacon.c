/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2008-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "crypto/sha1.h"
#include "wps/wps_defs.h"
#include "p2p/p2p.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "wpa_auth.h"
#include "wmm.h"
#include "ap_config.h"
#include "sta_info.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "hs20.h"
#include "dfs.h"
#include "taxonomy.h"
#include "ieee802_11_auth.h"


#ifdef NEED_AP_MLME

static u8 * hostapd_eid_bss_load(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	if (len < 2 + 5)
		return eid;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->bss_load_test_set) {
		*eid++ = WLAN_EID_BSS_LOAD;
		*eid++ = 5;
		os_memcpy(eid, hapd->conf->bss_load_test, 5);
		eid += 5;
		return eid;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (hapd->conf->bss_load_update_period) {
		*eid++ = WLAN_EID_BSS_LOAD;
		*eid++ = 5;
		WPA_PUT_LE16(eid, hapd->num_sta);
		eid += 2;
		*eid++ = hapd->iface->channel_utilization;
		WPA_PUT_LE16(eid, 0); /* no available admission capabity */
		eid += 2;
	}
	return eid;
}


static u8 ieee802_11_erp_info(struct hostapd_data *hapd)
{
	u8 erp = 0;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return 0;

	if (hapd->iface->olbc)
		erp |= ERP_INFO_USE_PROTECTION;
	if (hapd->iface->num_sta_non_erp > 0) {
		erp |= ERP_INFO_NON_ERP_PRESENT |
			ERP_INFO_USE_PROTECTION;
	}
	if (hapd->iface->num_sta_no_short_preamble > 0 ||
	    hapd->iconf->preamble == LONG_PREAMBLE)
		erp |= ERP_INFO_BARKER_PREAMBLE_MODE;

	return erp;
}


static u8 * hostapd_eid_ds_params(struct hostapd_data *hapd, u8 *eid)
{
	enum hostapd_hw_mode hw_mode = hapd->iconf->hw_mode;

	if (hw_mode != HOSTAPD_MODE_IEEE80211G &&
	    hw_mode != HOSTAPD_MODE_IEEE80211B)
		return eid;

	*eid++ = WLAN_EID_DS_PARAMS;
	*eid++ = 1;
	*eid++ = hapd->iconf->channel;
	return eid;
}


static u8 * hostapd_eid_erp_info(struct hostapd_data *hapd, u8 *eid)
{
	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return eid;

	/* Set NonERP_present and use_protection bits if there
	 * are any associated NonERP stations. */
	/* TODO: use_protection bit can be set to zero even if
	 * there are NonERP stations present. This optimization
	 * might be useful if NonERP stations are "quiet".
	 * See 802.11g/D6 E-1 for recommended practice.
	 * In addition, Non ERP present might be set, if AP detects Non ERP
	 * operation on other APs. */

	/* Add ERP Information element */
	*eid++ = WLAN_EID_ERP_INFO;
	*eid++ = 1;
	*eid++ = ieee802_11_erp_info(hapd);

	return eid;
}


static u8 * hostapd_eid_pwr_constraint(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 local_pwr_constraint = 0;
	int dfs;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211A)
		return eid;

	/* Let host drivers add this IE if DFS support is offloaded */
	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return eid;

	/*
	 * There is no DFS support and power constraint was not directly
	 * requested by config option.
	 */
	if (!hapd->iconf->ieee80211h &&
	    hapd->iconf->local_pwr_constraint == -1)
		return eid;

	/* Check if DFS is required by regulatory. */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0) {
		wpa_printf(MSG_WARNING, "Failed to check if DFS is required; ret=%d",
			   dfs);
		dfs = 0;
	}

	if (dfs == 0 && hapd->iconf->local_pwr_constraint == -1)
		return eid;

	/*
	 * ieee80211h (DFS) is enabled so Power Constraint element shall
	 * be added when running on DFS channel whenever local_pwr_constraint
	 * is configured or not. In order to meet regulations when TPC is not
	 * implemented using a transmit power that is below the legal maximum
	 * (including any mitigation factor) should help. In this case,
	 * indicate 3 dB below maximum allowed transmit power.
	 */
	if (hapd->iconf->local_pwr_constraint == -1)
		local_pwr_constraint = 3;

	/*
	 * A STA that is not an AP shall use a transmit power less than or
	 * equal to the local maximum transmit power level for the channel.
	 * The local maximum transmit power can be calculated from the formula:
	 * local max TX pwr = max TX pwr - local pwr constraint
	 * Where max TX pwr is maximum transmit power level specified for
	 * channel in Country element and local pwr constraint is specified
	 * for channel in this Power Constraint element.
	 */

	/* Element ID */
	*pos++ = WLAN_EID_PWR_CONSTRAINT;
	/* Length */
	*pos++ = 1;
	/* Local Power Constraint */
	if (local_pwr_constraint)
		*pos++ = local_pwr_constraint;
	else
		*pos++ = hapd->iconf->local_pwr_constraint;

	return pos;
}


static u8 * hostapd_eid_country_add(struct hostapd_data *hapd, u8 *pos,
				    u8 *end, int chan_spacing,
				    struct hostapd_channel_data *start,
				    struct hostapd_channel_data *prev)
{
	if (end - pos < 3)
		return pos;

	/* first channel number */
	*pos++ = start->chan;
	/* number of channels */
	*pos++ = (prev->chan - start->chan) / chan_spacing + 1;
	/* maximum transmit power level */
	if (!is_6ghz_op_class(hapd->iconf->op_class))
		*pos++ = start->max_tx_power;
	else
		*pos++ = 0; /* Reserved when operating on the 6 GHz band */

	return pos;
}


static u8 * hostapd_fill_subband_triplets(struct hostapd_data *hapd, u8 *pos,
					    u8 *end)
{
	int i;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *start, *prev;
	int chan_spacing = 1;

	mode = hapd->iface->current_mode;
	if (mode->mode == HOSTAPD_MODE_IEEE80211A)
		chan_spacing = 4;

	start = prev = NULL;
	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (start && prev &&
		    prev->chan + chan_spacing == chan->chan &&
		    start->max_tx_power == chan->max_tx_power) {
			prev = chan;
			continue; /* can use same entry */
		}

		if (start && prev)
			pos = hostapd_eid_country_add(hapd, pos, end,
						      chan_spacing,
						      start, prev);

		/* Start new group */
		start = prev = chan;
	}

	if (start) {
		pos = hostapd_eid_country_add(hapd, pos, end, chan_spacing,
					      start, prev);
	}

	return pos;
}


static u8 * hostapd_eid_country(struct hostapd_data *hapd, u8 *eid,
				int max_len)
{
	u8 *pos = eid;
	u8 *end = eid + max_len;

	if (!hapd->iconf->ieee80211d || max_len < 6 ||
	    hapd->iface->current_mode == NULL)
		return eid;

	*pos++ = WLAN_EID_COUNTRY;
	pos++; /* length will be set later */
	os_memcpy(pos, hapd->iconf->country, 3); /* e.g., 'US ' */
	pos += 3;

	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		/* Force the third octet of the country string to indicate
		 * Global Operating Class (Table E-4) */
		eid[4] = 0x04;

		/* Operating Triplet field */
		/* Operating Extension Identifier (>= 201 to indicate this is
		 * not a Subband Triplet field) */
		*pos++ = 201;
		/* Operating Class */
		*pos++ = hapd->iconf->op_class;
		/* Coverage Class */
		*pos++ = 0;
		/* Subband Triplets are required only for the 20 MHz case */
		if (hapd->iconf->op_class == 131 ||
		    hapd->iconf->op_class == 136)
			pos = hostapd_fill_subband_triplets(hapd, pos, end);
	} else {
		pos = hostapd_fill_subband_triplets(hapd, pos, end);
	}

	if ((pos - eid) & 1) {
		if (end - pos < 1)
			return eid;
		*pos++ = 0; /* pad for 16-bit alignment */
	}

	eid[1] = (pos - eid) - 2;

	return pos;
}


const u8 * hostapd_wpa_ie(struct hostapd_data *hapd, u8 eid)
{
	const u8 *ies;
	size_t ies_len;

	ies = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ies_len);
	if (!ies)
		return NULL;

	return get_ie(ies, ies_len, eid);
}


static const u8 * hostapd_vendor_wpa_ie(struct hostapd_data *hapd,
					u32 vendor_type)
{
	const u8 *ies;
	size_t ies_len;

	ies = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ies_len);
	if (!ies)
		return NULL;

	return get_vendor_ie(ies, ies_len, vendor_type);
}


static u8 * hostapd_get_rsne(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_wpa_ie(hapd, WLAN_EID_RSN);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_mde(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_wpa_ie(hapd, WLAN_EID_MOBILITY_DOMAIN);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_rsnxe(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->no_beacon_rsnxe) {
		wpa_printf(MSG_INFO, "TESTING: Do not add RSNXE into Beacon");
		return pos;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	ie = hostapd_wpa_ie(hapd, WLAN_EID_RSNX);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_wpa_ie(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, WPA_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_osen_ie(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, OSEN_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_eid_csa(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iface->cs_oper_class && hapd->iconf->ecsa_ie_only)
		return eid;
#endif /* CONFIG_TESTING_OPTIONS */

	if (!hapd->cs_freq_params.channel)
		return eid;

	*eid++ = WLAN_EID_CHANNEL_SWITCH;
	*eid++ = 3;
	*eid++ = hapd->cs_block_tx;
	*eid++ = hapd->cs_freq_params.channel;
	*eid++ = hapd->cs_count;

	return eid;
}


static u8 * hostapd_eid_ecsa(struct hostapd_data *hapd, u8 *eid)
{
	if (!hapd->cs_freq_params.channel || !hapd->iface->cs_oper_class)
		return eid;

	*eid++ = WLAN_EID_EXT_CHANSWITCH_ANN;
	*eid++ = 4;
	*eid++ = hapd->cs_block_tx;
	*eid++ = hapd->iface->cs_oper_class;
	*eid++ = hapd->cs_freq_params.channel;
	*eid++ = hapd->cs_count;

	return eid;
}


static u8 * hostapd_eid_supported_op_classes(struct hostapd_data *hapd, u8 *eid)
{
	u8 op_class, channel;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA) ||
	    !hapd->iface->freq)
		return eid;

	if (ieee80211_freq_to_channel_ext(hapd->iface->freq,
					  hapd->iconf->secondary_channel,
					  hostapd_get_oper_chwidth(hapd->iconf),
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return eid;

	*eid++ = WLAN_EID_SUPPORTED_OPERATING_CLASSES;
	*eid++ = 2;

	/* Current Operating Class */
	*eid++ = op_class;

	/* TODO: Advertise all the supported operating classes */
	*eid++ = 0;

	return eid;
}


static int
ieee802_11_build_ap_params_mbssid(struct hostapd_data *hapd,
				  struct wpa_driver_ap_params *params)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data *tx_bss;
	size_t len, rnr_len = 0;
	u8 elem_count = 0, *elem = NULL, **elem_offset = NULL, *end;
	u8 rnr_elem_count = 0, *rnr_elem = NULL, **rnr_elem_offset = NULL;
	size_t i;

	if (!iface->mbssid_max_interfaces ||
	    iface->num_bss > iface->mbssid_max_interfaces ||
	    (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED &&
	     !iface->ema_max_periodicity))
		goto fail;

	/* Make sure bss->xrates_supported is set for all BSSs to know whether
	 * it need to be non-inherited. */
	for (i = 0; i < iface->num_bss; i++) {
		u8 buf[100];

		hostapd_eid_ext_supp_rates(iface->bss[i], buf);
	}

	tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	len = hostapd_eid_mbssid_len(tx_bss, WLAN_FC_STYPE_BEACON, &elem_count,
				     NULL, 0, &rnr_len);
	if (!len || (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED &&
		     elem_count > iface->ema_max_periodicity))
		goto fail;

	elem = os_zalloc(len);
	if (!elem)
		goto fail;

	elem_offset = os_zalloc(elem_count * sizeof(u8 *));
	if (!elem_offset)
		goto fail;

	if (rnr_len) {
		rnr_elem = os_zalloc(rnr_len);
		if (!rnr_elem)
			goto fail;

		rnr_elem_offset = os_calloc(elem_count + 1, sizeof(u8 *));
		if (!rnr_elem_offset)
			goto fail;
	}

	end = hostapd_eid_mbssid(tx_bss, elem, elem + len, WLAN_FC_STYPE_BEACON,
				 elem_count, elem_offset, NULL, 0, rnr_elem,
				 &rnr_elem_count, rnr_elem_offset, rnr_len);

	params->mbssid_tx_iface = tx_bss->conf->iface;
	params->mbssid_index = hostapd_mbssid_get_bss_index(hapd);
	params->mbssid_elem = elem;
	params->mbssid_elem_len = end - elem;
	params->mbssid_elem_count = elem_count;
	params->mbssid_elem_offset = elem_offset;
	params->rnr_elem = rnr_elem;
	params->rnr_elem_len = rnr_len;
	params->rnr_elem_count = rnr_elem_count;
	params->rnr_elem_offset = rnr_elem_offset;
	if (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED)
		params->ema = true;

	return 0;

fail:
	os_free(rnr_elem);
	os_free(rnr_elem_offset);
	os_free(elem_offset);
	os_free(elem);
	wpa_printf(MSG_ERROR, "MBSSID: Configuration failed");
	return -1;
}


static u8 * hostapd_eid_mbssid_config(struct hostapd_data *hapd, u8 *eid,
				      u8 mbssid_elem_count)
{
	struct hostapd_iface *iface = hapd->iface;

	if (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED) {
		*eid++ = WLAN_EID_EXTENSION;
		*eid++ = 3;
		*eid++ = WLAN_EID_EXT_MULTIPLE_BSSID_CONFIGURATION;
		*eid++ = iface->num_bss;
		*eid++ = mbssid_elem_count;
	}

	return eid;
}


static size_t he_elem_len(struct hostapd_data *hapd)
{
	size_t len = 0;

#ifdef CONFIG_IEEE80211AX
	if (!hapd->iconf->ieee80211ax || hapd->conf->disable_11ax)
		return len;

	len += 3 + sizeof(struct ieee80211_he_capabilities) +
		3 + sizeof(struct ieee80211_he_operation) +
		3 + sizeof(struct ieee80211_he_mu_edca_parameter_set) +
		3 + sizeof(struct ieee80211_spatial_reuse);
	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		len += sizeof(struct ieee80211_he_6ghz_oper_info) +
			3 + sizeof(struct ieee80211_he_6ghz_band_cap);
		/* An additional Transmit Power Envelope element for
		 * subordinate client */
		if (he_reg_is_indoor(hapd->iconf->he_6ghz_reg_pwr_type))
			len += 4;

		/* An additional Transmit Power Envelope element for
		 * default client with unit interpretation of regulatory
		 * client EIRP */
		if (hapd->iconf->reg_def_cli_eirp != -1 &&
		    he_reg_is_sp(hapd->iconf->he_6ghz_reg_pwr_type))
			len += 4;
	}
#endif /* CONFIG_IEEE80211AX */

	return len;
}


struct probe_resp_params {
	const struct ieee80211_mgmt *req;
	bool is_p2p;

	/* Generated IEs will be included inside an ML element */
	bool is_ml_sta_info;
	struct hostapd_data *mld_ap;
	struct mld_info *mld_info;

	struct ieee80211_mgmt *resp;
	size_t resp_len;
	u8 *csa_pos;
	u8 *ecsa_pos;
	const u8 *known_bss;
	u8 known_bss_len;

#ifdef CONFIG_IEEE80211AX
	u8 *cca_pos;
#endif /* CONFIG_IEEE80211AX */
};


static void hostapd_free_probe_resp_params(struct probe_resp_params *params)
{
#ifdef CONFIG_IEEE80211BE
	if (!params)
		return;
	ap_sta_free_sta_profile(params->mld_info);
	os_free(params->mld_info);
	params->mld_info = NULL;
#endif /* CONFIG_IEEE80211BE */
}


static size_t hostapd_probe_resp_elems_len(struct hostapd_data *hapd,
					   struct probe_resp_params *params)
{
	size_t buflen = 0;

#ifdef CONFIG_WPS
	if (hapd->wps_probe_resp_ie)
		buflen += wpabuf_len(hapd->wps_probe_resp_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_probe_resp_ie)
		buflen += wpabuf_len(hapd->p2p_probe_resp_ie);
#endif /* CONFIG_P2P */
#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		buflen += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */
	if (hapd->conf->vendor_elements)
		buflen += wpabuf_len(hapd->conf->vendor_elements);
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->presp_elements)
		buflen += wpabuf_len(hapd->conf->presp_elements);
#endif /* CONFIG_TESTING_OPTIONS */
	if (hapd->conf->vendor_vht) {
		buflen += 5 + 2 + sizeof(struct ieee80211_vht_capabilities) +
			2 + sizeof(struct ieee80211_vht_operation);
	}

	buflen += he_elem_len(hapd);

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		buflen += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		buflen += 3 + sizeof(struct ieee80211_eht_operation);
		if (hapd->iconf->punct_bitmap)
			buflen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

		if (!params->is_ml_sta_info && hapd->conf->mld_ap) {
			struct hostapd_data *ml_elem_ap =
				params->mld_ap ? params->mld_ap : hapd;

			buflen += hostapd_eid_eht_ml_beacon_len(
				ml_elem_ap, params->mld_info, !!params->mld_ap);
		}
	}
#endif /* CONFIG_IEEE80211BE */

	buflen += hostapd_eid_mbssid_len(hapd, WLAN_FC_STYPE_PROBE_RESP, NULL,
					 params->known_bss,
					 params->known_bss_len, NULL);
	if (!params->is_ml_sta_info)
		buflen += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_PROBE_RESP,
					      true);
	buflen += hostapd_mbo_ie_len(hapd);
	buflen += hostapd_eid_owe_trans_len(hapd);
	buflen += hostapd_eid_dpp_cc_len(hapd);

	return buflen;
}


static u8 * hostapd_probe_resp_fill_elems(struct hostapd_data *hapd,
					  struct probe_resp_params *params,
					  u8 *pos, size_t len)
{
	u8 *csa_pos;
	u8 *epos;

	epos = pos + len;

	if (!params->is_ml_sta_info) {
		*pos++ = WLAN_EID_SSID;
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memcpy(pos, hapd->conf->ssid.ssid,
			  hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	pos = hostapd_eid_country(hapd, pos, epos - pos);

	/* Power Constraint element */
	pos = hostapd_eid_pwr_constraint(hapd, pos);

	/*
	 * CSA IE
	 * TODO: This should be included inside the ML sta profile
	 */
	if (!params->is_ml_sta_info) {
		csa_pos = hostapd_eid_csa(hapd, pos);
		if (csa_pos != pos)
			params->csa_pos = csa_pos - 1;
		else
			params->csa_pos = NULL;
		pos = csa_pos;
	}

	/* ERP Information element */
	pos = hostapd_eid_erp_info(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	pos = hostapd_get_rsne(hapd, pos, epos - pos);
	pos = hostapd_eid_bss_load(hapd, pos, epos - pos);
	pos = hostapd_eid_mbssid(hapd, pos, epos, WLAN_FC_STYPE_PROBE_RESP, 0,
				 NULL, params->known_bss, params->known_bss_len,
				 NULL, NULL, NULL, 0);
	pos = hostapd_eid_rm_enabled_capab(hapd, pos, epos - pos);
	pos = hostapd_get_mde(hapd, pos, epos - pos);

	/*
	 * eCSA IE
	 * TODO: This should be included inside the ML sta profile
	 */
	if (!params->is_ml_sta_info) {
		csa_pos = hostapd_eid_ecsa(hapd, pos);
		if (csa_pos != pos)
			params->ecsa_pos = csa_pos - 1;
		else
			params->ecsa_pos = NULL;
		pos = csa_pos;
	}

	pos = hostapd_eid_supported_op_classes(hapd, pos);
	pos = hostapd_eid_ht_capabilities(hapd, pos);
	pos = hostapd_eid_ht_operation(hapd, pos);

	/* Probe Response frames always include all non-TX profiles except
	 * when a list of known BSSes is included in the Probe Request frame. */
	pos = hostapd_eid_ext_capab(hapd, pos,
				    hapd->iconf->mbssid >= MBSSID_ENABLED &&
				    !params->known_bss_len);

	pos = hostapd_eid_time_adv(hapd, pos);
	pos = hostapd_eid_time_zone(hapd, pos);

	pos = hostapd_eid_interworking(hapd, pos);
	pos = hostapd_eid_adv_proto(hapd, pos);
	pos = hostapd_eid_roaming_consortium(hapd, pos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(pos, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		pos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		pos = hostapd_eid_vht_capabilities(hapd, pos, 0);
		pos = hostapd_eid_vht_operation(hapd, pos);
		pos = hostapd_eid_txpower_envelope(hapd, pos);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax &&
	    is_6ghz_op_class(hapd->iconf->op_class))
		pos = hostapd_eid_txpower_envelope(hapd, pos);
#endif /* CONFIG_IEEE80211AX */

	pos = hostapd_eid_wb_chsw_wrapper(hapd, pos);

	if (!params->is_ml_sta_info)
		pos = hostapd_eid_rnr(hapd, pos, WLAN_FC_STYPE_PROBE_RESP,
				      true);
	pos = hostapd_eid_fils_indic(hapd, pos, 0);
	pos = hostapd_get_rsnxe(hapd, pos, epos - pos);

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax) {
		u8 *cca_pos;

		pos = hostapd_eid_he_capab(hapd, pos, IEEE80211_MODE_AP);
		pos = hostapd_eid_he_operation(hapd, pos);

		/* BSS Color Change Announcement element */
		cca_pos = hostapd_eid_cca(hapd, pos);
		if (cca_pos != pos)
			params->cca_pos = cca_pos - 2;
		else
			params->cca_pos = NULL;
		pos = cca_pos;

		pos = hostapd_eid_spatial_reuse(hapd, pos);
		pos = hostapd_eid_he_mu_edca_parameter_set(hapd, pos);
		pos = hostapd_eid_he_6ghz_band_cap(hapd, pos);
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		struct hostapd_data *ml_elem_ap =
			params->mld_ap ? params->mld_ap : hapd;

		if (ml_elem_ap->conf->mld_ap)
			pos = hostapd_eid_eht_ml_beacon(
				ml_elem_ap, params->mld_info,
				pos, !!params->mld_ap);

		pos = hostapd_eid_eht_capab(hapd, pos, IEEE80211_MODE_AP);
		pos = hostapd_eid_eht_operation(hapd, pos);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht)
		pos = hostapd_eid_vendor_vht(hapd, pos);
#endif /* CONFIG_IEEE80211AC */

	/* WPA / OSEN */
	pos = hostapd_get_wpa_ie(hapd, pos, epos - pos);
	pos = hostapd_get_osen_ie(hapd, pos, epos - pos);

	/* Wi-Fi Alliance WMM */
	pos = hostapd_eid_wmm(hapd, pos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->wps_probe_resp_ie),
			  wpabuf_len(hapd->wps_probe_resp_ie));
		pos += wpabuf_len(hapd->wps_probe_resp_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && params->is_p2p &&
	    hapd->p2p_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->p2p_probe_resp_ie),
			  wpabuf_len(hapd->p2p_probe_resp_ie));
		pos += wpabuf_len(hapd->p2p_probe_resp_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		pos = hostapd_eid_p2p_manage(hapd, pos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	pos = hostapd_eid_hs20_indication(hapd, pos);
#endif /* CONFIG_HS20 */

	pos = hostapd_eid_mbo(hapd, pos, epos - pos);
	pos = hostapd_eid_owe_trans(hapd, pos, epos - pos);
	pos = hostapd_eid_dpp_cc(hapd, pos, epos - pos);

	if (hapd->conf->vendor_elements) {
		os_memcpy(pos, wpabuf_head(hapd->conf->vendor_elements),
			  wpabuf_len(hapd->conf->vendor_elements));
		pos += wpabuf_len(hapd->conf->vendor_elements);
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->presp_elements) {
		os_memcpy(pos, wpabuf_head(hapd->conf->presp_elements),
			  wpabuf_len(hapd->conf->presp_elements));
		pos += wpabuf_len(hapd->conf->presp_elements);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	return pos;
}


static void hostapd_gen_probe_resp(struct hostapd_data *hapd,
				   struct probe_resp_params *params)
{
	u8 *pos;
	size_t buflen;

	hapd = hostapd_mbssid_get_tx_bss(hapd);

#define MAX_PROBERESP_LEN 768
	buflen = MAX_PROBERESP_LEN;
	buflen += hostapd_probe_resp_elems_len(hapd, params);
	params->resp = os_zalloc(buflen);
	if (!params->resp) {
		params->resp_len = 0;
		return;
	}

	params->resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						   WLAN_FC_STYPE_PROBE_RESP);
	/* Unicast the response to all requests on bands other than 6 GHz. For
	 * the 6 GHz, unicast is used only if the actual SSID is not included in
	 * the Beacon frames. Otherwise, broadcast response is used per IEEE
	 * Std 802.11ax-2021, 26.17.2.3.2. Broadcast address is also used for
	 * the Probe Response frame template for the unsolicited (i.e., not as
	 * a response to a specific request) case. */
	if (params->req && (!is_6ghz_op_class(hapd->iconf->op_class) ||
		    hapd->conf->ignore_broadcast_ssid))
		os_memcpy(params->resp->da, params->req->sa, ETH_ALEN);
	else
		os_memset(params->resp->da, 0xff, ETH_ALEN);
	os_memcpy(params->resp->sa, hapd->own_addr, ETH_ALEN);

	os_memcpy(params->resp->bssid, hapd->own_addr, ETH_ALEN);
	params->resp->u.probe_resp.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	params->resp->u.probe_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd));

	pos = hostapd_probe_resp_fill_elems(hapd, params,
					    params->resp->u.probe_resp.variable,
					    buflen);

	params->resp_len = pos - (u8 *) params->resp;
}


#ifdef CONFIG_IEEE80211BE
static void hostapd_fill_probe_resp_ml_params(struct hostapd_data *hapd,
					      struct probe_resp_params *params,
					      const struct ieee80211_mgmt *mgmt,
					      int mld_id, u16 links)
{
	struct probe_resp_params sta_info_params;
	struct hostapd_data *link;

	params->mld_ap = NULL;
	params->mld_info = os_zalloc(sizeof(*params->mld_info));
	if (!params->mld_info)
		return;

	wpa_printf(MSG_DEBUG,
		   "MLD: Got ML probe request with AP MLD ID %d for links %04x",
		   mld_id, links);

	for_each_mld_link(link, hapd) {
		struct mld_link_info *link_info;
		size_t buflen;
		u8 mld_link_id = link->mld_link_id;
		u8 *epos;
		u8 buf[EHT_ML_MAX_STA_PROF_LEN];

		/*
		 * Set mld_ap iff the ML probe request explicitly
		 * requested a specific MLD ID. In that case, the targeted
		 * AP may have been a nontransmitted BSSID on the same
		 * interface.
		 */
		if (mld_id != -1 && link->iface == hapd->iface)
			params->mld_ap = link;

		/* Never duplicate main Probe Response frame body */
		if (link == hapd)
			continue;

		/* Only include requested links */
		if (!(BIT(mld_link_id) & links))
			continue;

		link_info = &params->mld_info->links[mld_link_id];

		sta_info_params.req = params->req;
		sta_info_params.is_p2p = false;
		sta_info_params.is_ml_sta_info = true;
		sta_info_params.mld_ap = NULL;
		sta_info_params.mld_info = NULL;

		buflen = MAX_PROBERESP_LEN;
		buflen += hostapd_probe_resp_elems_len(link, &sta_info_params);

		if (buflen > EHT_ML_MAX_STA_PROF_LEN) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Not including link %d in ML probe response (%zu bytes is too long)",
				   mld_link_id, buflen);
			goto fail;
		}

		/*
		 * NOTE: This does not properly handle inheritance and
		 * various other things.
		 */
		link_info->valid = true;
		epos = buf;

		/* Capabilities is the only fixed parameter */
		WPA_PUT_LE16(epos, hostapd_own_capab_info(hapd));
		epos += 2;

		epos = hostapd_probe_resp_fill_elems(
			link, &sta_info_params, epos,
			EHT_ML_MAX_STA_PROF_LEN - 2);
		link_info->resp_sta_profile_len = epos - buf;
		os_free(link_info->resp_sta_profile);
		link_info->resp_sta_profile = os_memdup(
			buf, link_info->resp_sta_profile_len);
		if (!link_info->resp_sta_profile)
			link_info->resp_sta_profile_len = 0;
		os_memcpy(link_info->local_addr, link->own_addr, ETH_ALEN);

		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe response includes link sta info for %d: %u bytes (estimate %zu)",
			   mld_link_id, link_info->resp_sta_profile_len,
			   buflen);
	}

	if (mld_id != -1 && !params->mld_ap) {
		wpa_printf(MSG_DEBUG,
			   "MLD: No nontransmitted BSSID for MLD ID %d",
			   mld_id);
		goto fail;
	}

	return;

fail:
	hostapd_free_probe_resp_params(params);
	params->mld_ap = NULL;
	params->mld_info = NULL;
}
#endif /* CONFIG_IEEE80211BE */


enum ssid_match_result {
	NO_SSID_MATCH,
	EXACT_SSID_MATCH,
	WILDCARD_SSID_MATCH,
	CO_LOCATED_SSID_MATCH,
};

static enum ssid_match_result ssid_match(struct hostapd_data *hapd,
					 const u8 *ssid, size_t ssid_len,
					 const u8 *ssid_list,
					 size_t ssid_list_len,
					 const u8 *short_ssid_list,
					 size_t short_ssid_list_len)
{
	const u8 *pos, *end;
	struct hostapd_iface *iface = hapd->iface;
	int wildcard = 0;
	size_t i, j;

	if (ssid_len == 0)
		wildcard = 1;
	if (ssid_len == hapd->conf->ssid.ssid_len &&
	    os_memcmp(ssid, hapd->conf->ssid.ssid, ssid_len) == 0)
		return EXACT_SSID_MATCH;

	if (ssid_list) {
		pos = ssid_list;
		end = ssid_list + ssid_list_len;
		while (end - pos >= 2) {
			if (2 + pos[1] > end - pos)
				break;
			if (pos[1] == 0)
				wildcard = 1;
			if (pos[1] == hapd->conf->ssid.ssid_len &&
			    os_memcmp(pos + 2, hapd->conf->ssid.ssid,
				      pos[1]) == 0)
				return EXACT_SSID_MATCH;
			pos += 2 + pos[1];
		}
	}

	if (short_ssid_list) {
		pos = short_ssid_list;
		end = short_ssid_list + short_ssid_list_len;
		while (end - pos >= 4) {
			if (hapd->conf->ssid.short_ssid == WPA_GET_LE32(pos))
				return EXACT_SSID_MATCH;
			pos += 4;
		}
	}

	if (wildcard)
		return WILDCARD_SSID_MATCH;

	if (!iface->interfaces || iface->interfaces->count <= 1 ||
	    is_6ghz_op_class(hapd->iconf->op_class))
		return NO_SSID_MATCH;

	for (i = 0; i < iface->interfaces->count; i++) {
		struct hostapd_iface *colocated;

		colocated = iface->interfaces->iface[i];

		if (colocated == iface ||
		    !is_6ghz_op_class(colocated->conf->op_class))
			continue;

		for (j = 0; j < colocated->num_bss; j++) {
			struct hostapd_bss_config *conf;

			conf = colocated->bss[j]->conf;
			if (ssid_len == conf->ssid.ssid_len &&
			    os_memcmp(ssid, conf->ssid.ssid, ssid_len) == 0)
				return CO_LOCATED_SSID_MATCH;
		}
	}

	return NO_SSID_MATCH;
}


void sta_track_expire(struct hostapd_iface *iface, int force)
{
	struct os_reltime now;
	struct hostapd_sta_info *info;

	if (!iface->num_sta_seen)
		return;

	os_get_reltime(&now);
	while ((info = dl_list_first(&iface->sta_seen, struct hostapd_sta_info,
				     list))) {
		if (!force &&
		    !os_reltime_expired(&now, &info->last_seen,
					iface->conf->track_sta_max_age))
			break;
		force = 0;

		wpa_printf(MSG_MSGDUMP, "%s: Expire STA tracking entry for "
			   MACSTR, iface->bss[0]->conf->iface,
			   MAC2STR(info->addr));
		dl_list_del(&info->list);
		iface->num_sta_seen--;
		sta_track_del(info);
	}
}


static struct hostapd_sta_info * sta_track_get(struct hostapd_iface *iface,
					       const u8 *addr)
{
	struct hostapd_sta_info *info;

	dl_list_for_each(info, &iface->sta_seen, struct hostapd_sta_info, list)
		if (ether_addr_equal(addr, info->addr))
			return info;

	return NULL;
}


void sta_track_add(struct hostapd_iface *iface, const u8 *addr, int ssi_signal)
{
	struct hostapd_sta_info *info;

	info = sta_track_get(iface, addr);
	if (info) {
		/* Move the most recent entry to the end of the list */
		dl_list_del(&info->list);
		dl_list_add_tail(&iface->sta_seen, &info->list);
		os_get_reltime(&info->last_seen);
		info->ssi_signal = ssi_signal;
		return;
	}

	/* Add a new entry */
	info = os_zalloc(sizeof(*info));
	if (info == NULL)
		return;
	os_memcpy(info->addr, addr, ETH_ALEN);
	os_get_reltime(&info->last_seen);
	info->ssi_signal = ssi_signal;

	if (iface->num_sta_seen >= iface->conf->track_sta_max_num) {
		/* Expire oldest entry to make room for a new one */
		sta_track_expire(iface, 1);
	}

	wpa_printf(MSG_MSGDUMP, "%s: Add STA tracking entry for "
		   MACSTR, iface->bss[0]->conf->iface, MAC2STR(addr));
	dl_list_add_tail(&iface->sta_seen, &info->list);
	iface->num_sta_seen++;
}


struct hostapd_data *
sta_track_seen_on(struct hostapd_iface *iface, const u8 *addr,
		  const char *ifname)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_data *hapd = NULL;

		iface = interfaces->iface[i];
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				break;
			hapd = NULL;
		}

		if (hapd && sta_track_get(iface, addr))
			return hapd;
	}

	return NULL;
}


#ifdef CONFIG_TAXONOMY
void sta_track_claim_taxonomy_info(struct hostapd_iface *iface, const u8 *addr,
				   struct wpabuf **probe_ie_taxonomy)
{
	struct hostapd_sta_info *info;

	info = sta_track_get(iface, addr);
	if (!info)
		return;

	wpabuf_free(*probe_ie_taxonomy);
	*probe_ie_taxonomy = info->probe_ie_taxonomy;
	info->probe_ie_taxonomy = NULL;
}
#endif /* CONFIG_TAXONOMY */


#ifdef CONFIG_IEEE80211BE
static bool parse_ml_probe_req(const struct ieee80211_eht_ml *ml, size_t ml_len,
			       int *mld_id, u16 *links)
{
	u16 ml_control;
	const struct element *sub;
	const u8 *pos;
	size_t len;

	*mld_id = -1;
	*links = 0xffff;

	if (ml_len < sizeof(struct ieee80211_eht_ml))
		return false;

	ml_control = le_to_host16(ml->ml_control);
	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_PROBE_REQ) {
		wpa_printf(MSG_DEBUG, "MLD: Not an ML probe req");
		return false;
	}

	if (sizeof(struct ieee80211_eht_ml) + 1 > ml_len) {
		wpa_printf(MSG_DEBUG, "MLD: ML probe req too short");
		return false;
	}

	pos = ml->variable;
	len = pos[0];
	if (len < 1 || sizeof(struct ieee80211_eht_ml) + len > ml_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe request with invalid length");
		return false;
	}

	if (ml_control & EHT_ML_PRES_BM_PROBE_REQ_AP_MLD_ID) {
		if (len < 2) {
			wpa_printf(MSG_DEBUG,
				   "MLD: ML probe req too short for MLD ID");
			return false;
		}

		*mld_id = pos[1];
	}
	pos += len;

	/* Parse subelements (if there are any) */
	len = ml_len - len - sizeof(struct ieee80211_eht_ml);
	for_each_element_id(sub, 0, pos, len) {
		const struct ieee80211_eht_per_sta_profile *sta;
		u16 sta_control;

		if (*links == 0xffff)
			*links = 0;

		if (sub->datalen <
		    sizeof(struct ieee80211_eht_per_sta_profile)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: ML probe req %d too short for sta profile",
				   sub->datalen);
			return false;
		}

		sta = (struct ieee80211_eht_per_sta_profile *) sub->data;

		/*
		 * Extract the link ID, do not return whether a complete or
		 * partial profile was requested.
		 */
		sta_control = le_to_host16(sta->sta_control);
		*links |= BIT(sta_control & EHT_PER_STA_CTRL_LINK_ID_MSK);
	}

	if (!for_each_element_completed(sub, pos, len)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe req sub-elements parsing error");
		return false;
	}

	return true;
}
#endif /* CONFIG_IEEE80211BE */


void handle_probe_req(struct hostapd_data *hapd,
		      const struct ieee80211_mgmt *mgmt, size_t len,
		      int ssi_signal)
{
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ie_len;
	size_t i;
	int noack;
	enum ssid_match_result res;
	int ret;
	u16 csa_offs[2];
	size_t csa_offs_len;
	struct radius_sta rad_info;
	struct probe_resp_params params;
#ifdef CONFIG_IEEE80211BE
	int mld_id;
	u16 links;
#endif /* CONFIG_IEEE80211BE */

	if (hapd->iconf->rssi_ignore_probe_request && ssi_signal &&
	    ssi_signal < hapd->iconf->rssi_ignore_probe_request)
		return;

	if (len < IEEE80211_HDRLEN)
		return;
	ie = ((const u8 *) mgmt) + IEEE80211_HDRLEN;
	if (hapd->iconf->track_sta_max_num)
		sta_track_add(hapd->iface, mgmt->sa, ssi_signal);
	ie_len = len - IEEE80211_HDRLEN;

	ret = hostapd_allowed_address(hapd, mgmt->sa, (const u8 *) mgmt, len,
				      &rad_info, 1);
	if (ret == HOSTAPD_ACL_REJECT) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"Ignore Probe Request frame from " MACSTR
			" due to ACL reject ", MAC2STR(mgmt->sa));
		return;
	}

	for (i = 0; hapd->probereq_cb && i < hapd->num_probereq_cb; i++)
		if (hapd->probereq_cb[i].cb(hapd->probereq_cb[i].ctx,
					    mgmt->sa, mgmt->da, mgmt->bssid,
					    ie, ie_len, ssi_signal) > 0)
			return;

	if (!hapd->conf->send_probe_response)
		return;

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "Could not parse ProbeReq from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	if ((!elems.ssid || !elems.supp_rates)) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " sent probe request "
			   "without SSID or supported rates element",
			   MAC2STR(mgmt->sa));
		return;
	}

	/*
	 * No need to reply if the Probe Request frame was sent on an adjacent
	 * channel. IEEE Std 802.11-2012 describes this as a requirement for an
	 * AP with dot11RadioMeasurementActivated set to true, but strictly
	 * speaking does not allow such ignoring of Probe Request frames if
	 * dot11RadioMeasurementActivated is false. Anyway, this can help reduce
	 * number of unnecessary Probe Response frames for cases where the STA
	 * is less likely to see them (Probe Request frame sent on a
	 * neighboring, but partially overlapping, channel).
	 */
	if (elems.ds_params &&
	    hapd->iface->current_mode &&
	    (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G ||
	     hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211B) &&
	    hapd->iconf->channel != elems.ds_params[0]) {
		wpa_printf(MSG_DEBUG,
			   "Ignore Probe Request due to DS Params mismatch: chan=%u != ds.chan=%u",
			   hapd->iconf->channel, elems.ds_params[0]);
		return;
	}

#ifdef CONFIG_P2P
	if (hapd->p2p && hapd->p2p_group && elems.wps_ie) {
		struct wpabuf *wps;
		wps = ieee802_11_vendor_ie_concat(ie, ie_len, WPS_DEV_OUI_WFA);
		if (wps && !p2p_group_match_dev_type(hapd->p2p_group, wps)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Requested Device "
				   "Type");
			wpabuf_free(wps);
			return;
		}
		wpabuf_free(wps);
	}

	if (hapd->p2p && hapd->p2p_group && elems.p2p) {
		struct wpabuf *p2p;
		p2p = ieee802_11_vendor_ie_concat(ie, ie_len, P2P_IE_VENDOR_TYPE);
		if (p2p && !p2p_group_match_dev_id(hapd->p2p_group, p2p)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Device ID");
			wpabuf_free(p2p);
			return;
		}
		wpabuf_free(p2p);
	}
#endif /* CONFIG_P2P */

	if (hapd->conf->ignore_broadcast_ssid && elems.ssid_len == 0 &&
	    elems.ssid_list_len == 0 && elems.short_ssid_list_len == 0) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	    elems.ssid_len == P2P_WILDCARD_SSID_LEN &&
	    os_memcmp(elems.ssid, P2P_WILDCARD_SSID,
		      P2P_WILDCARD_SSID_LEN) == 0) {
		/* Process P2P Wildcard SSID like Wildcard SSID */
		elems.ssid_len = 0;
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_TAXONOMY
	{
		struct sta_info *sta;
		struct hostapd_sta_info *info;

		if ((sta = ap_get_sta(hapd, mgmt->sa)) != NULL) {
			taxonomy_sta_info_probe_req(hapd, sta, ie, ie_len);
		} else if ((info = sta_track_get(hapd->iface,
						 mgmt->sa)) != NULL) {
			taxonomy_hostapd_sta_info_probe_req(hapd, info,
							    ie, ie_len);
		}
	}
#endif /* CONFIG_TAXONOMY */

	res = ssid_match(hapd, elems.ssid, elems.ssid_len,
			 elems.ssid_list, elems.ssid_list_len,
			 elems.short_ssid_list, elems.short_ssid_list_len);
	if (res == NO_SSID_MATCH) {
		if (!(mgmt->da[0] & 0x01)) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for foreign SSID '%s' (DA " MACSTR ")%s",
				   MAC2STR(mgmt->sa),
				   wpa_ssid_txt(elems.ssid, elems.ssid_len),
				   MAC2STR(mgmt->da),
				   elems.ssid_list ? " (SSID list)" : "");
		}
		return;
	}

	if (hapd->conf->ignore_broadcast_ssid && res == WILDCARD_SSID_MATCH) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_INTERWORKING
	if (hapd->conf->interworking &&
	    elems.interworking && elems.interworking_len >= 1) {
		u8 ant = elems.interworking[0] & 0x0f;
		if (ant != INTERWORKING_ANT_WILDCARD &&
		    ant != hapd->conf->access_network_type) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching ANT %u ignored",
				   MAC2STR(mgmt->sa), ant);
			return;
		}
	}

	if (hapd->conf->interworking && elems.interworking &&
	    (elems.interworking_len == 7 || elems.interworking_len == 9)) {
		const u8 *hessid;
		if (elems.interworking_len == 7)
			hessid = elems.interworking + 1;
		else
			hessid = elems.interworking + 1 + 2;
		if (!is_broadcast_ether_addr(hessid) &&
		    !ether_addr_equal(hessid, hapd->conf->hessid)) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching HESSID " MACSTR
				   " ignored",
				   MAC2STR(mgmt->sa), MAC2STR(hessid));
			return;
		}
	}
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	    supp_rates_11b_only(&elems)) {
		/* Indicates support for 11b rates only */
		wpa_printf(MSG_EXCESSIVE, "P2P: Ignore Probe Request from "
			   MACSTR " with only 802.11b rates",
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_P2P */

	/* TODO: verify that supp_rates contains at least one matching rate
	 * with AP configuration */

	if (hapd->conf->no_probe_resp_if_seen_on &&
	    is_multicast_ether_addr(mgmt->da) &&
	    is_multicast_ether_addr(mgmt->bssid) &&
	    sta_track_seen_on(hapd->iface, mgmt->sa,
			      hapd->conf->no_probe_resp_if_seen_on)) {
		wpa_printf(MSG_MSGDUMP, "%s: Ignore Probe Request from " MACSTR
			   " since STA has been seen on %s",
			   hapd->conf->iface, MAC2STR(mgmt->sa),
			   hapd->conf->no_probe_resp_if_seen_on);
		return;
	}

	if (hapd->conf->no_probe_resp_if_max_sta &&
	    is_multicast_ether_addr(mgmt->da) &&
	    is_multicast_ether_addr(mgmt->bssid) &&
	    hapd->num_sta >= hapd->conf->max_num_sta &&
	    !ap_get_sta(hapd, mgmt->sa)) {
		wpa_printf(MSG_MSGDUMP, "%s: Ignore Probe Request from " MACSTR
			   " since no room for additional STA",
			   hapd->conf->iface, MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iconf->ignore_probe_probability > 0.0 &&
	    drand48() < hapd->iconf->ignore_probe_probability) {
		wpa_printf(MSG_INFO,
			   "TESTING: ignoring probe request from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Do not send Probe Response frame from a non-transmitting multiple
	 * BSSID profile unless the Probe Request frame is directed at that
	 * particular BSS. */
	if (hapd != hostapd_mbssid_get_tx_bss(hapd) && res != EXACT_SSID_MATCH)
		return;

	wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, RX_PROBE_REQUEST "sa=" MACSTR
		     " signal=%d", MAC2STR(mgmt->sa), ssi_signal);

	os_memset(&params, 0, sizeof(params));

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && elems.probe_req_mle &&
	    parse_ml_probe_req((struct ieee80211_eht_ml *) elems.probe_req_mle,
			       elems.probe_req_mle_len, &mld_id, &links)) {
		hostapd_fill_probe_resp_ml_params(hapd, &params, mgmt,
						  mld_id, links);
	}
#endif /* CONFIG_IEEE80211BE */

	params.req = mgmt;
	params.is_p2p = !!elems.p2p;
	params.known_bss = elems.mbssid_known_bss;
	params.known_bss_len = elems.mbssid_known_bss_len;
	params.is_ml_sta_info = false;

	hostapd_gen_probe_resp(hapd, &params);

	hostapd_free_probe_resp_params(&params);

	if (!params.resp)
		return;

	/*
	 * If this is a broadcast probe request, apply no ack policy to avoid
	 * excessive retries.
	 */
	noack = !!(res == WILDCARD_SSID_MATCH &&
		   is_broadcast_ether_addr(mgmt->da));

	csa_offs_len = 0;
	if (hapd->csa_in_progress) {
		if (params.csa_pos)
			csa_offs[csa_offs_len++] =
				params.csa_pos - (u8 *) params.resp;

		if (params.ecsa_pos)
			csa_offs[csa_offs_len++] =
				params.ecsa_pos - (u8 *) params.resp;
	}

	ret = hostapd_drv_send_mlme(hapd, params.resp, params.resp_len, noack,
				    csa_offs_len ? csa_offs : NULL,
				    csa_offs_len, 0);

	if (ret < 0)
		wpa_printf(MSG_INFO, "handle_probe_req: send failed");

	os_free(params.resp);

	wpa_printf(MSG_EXCESSIVE, "STA " MACSTR " sent probe request for %s "
		   "SSID", MAC2STR(mgmt->sa),
		   elems.ssid_len == 0 ? "broadcast" : "our");
}


static u8 * hostapd_probe_resp_offloads(struct hostapd_data *hapd,
					size_t *resp_len)
{
	struct probe_resp_params params;

	/* check probe response offloading caps and print warnings */
	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_PROBE_RESP_OFFLOAD))
		return NULL;

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie &&
	    (!(hapd->iface->probe_resp_offloads &
	       (WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS |
		WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS2))))
		wpa_printf(MSG_WARNING, "Device is trying to offload WPS "
			   "Probe Response while not supporting this");
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_probe_resp_ie &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_P2P))
		wpa_printf(MSG_WARNING, "Device is trying to offload P2P "
			   "Probe Response while not supporting this");
#endif  /* CONFIG_P2P */

	if (hapd->conf->interworking &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_INTERWORKING))
		wpa_printf(MSG_WARNING, "Device is trying to offload "
			   "Interworking Probe Response while not supporting "
			   "this");

	/* Generate a Probe Response template for the non-P2P case */
	os_memset(&params, 0, sizeof(params));
	params.req = NULL;
	params.is_p2p = false;
	params.known_bss = NULL;
	params.known_bss_len = 0;
	params.is_ml_sta_info = false;
	params.mld_ap = NULL;
	params.mld_info = NULL;

	hostapd_gen_probe_resp(hapd, &params);
	*resp_len = params.resp_len;
	if (!params.resp)
		return NULL;

	/* TODO: Avoid passing these through struct hostapd_data */
	if (params.csa_pos)
		hapd->cs_c_off_proberesp = params.csa_pos - (u8 *) params.resp;
	if (params.ecsa_pos)
		hapd->cs_c_off_ecsa_proberesp = params.ecsa_pos -
			(u8 *) params.resp;
#ifdef CONFIG_IEEE80211AX
	if (params.cca_pos)
		hapd->cca_c_off_proberesp = params.cca_pos - (u8 *) params.resp;
#endif /* CONFIG_IEEE80211AX */

	return (u8 *) params.resp;
}

#endif /* NEED_AP_MLME */


#ifdef CONFIG_IEEE80211AX
/* Unsolicited broadcast Probe Response transmission, 6 GHz only */
u8 * hostapd_unsol_bcast_probe_resp(struct hostapd_data *hapd,
				    struct unsol_bcast_probe_resp *ubpr)
{
	struct probe_resp_params probe_params;

	if (!is_6ghz_op_class(hapd->iconf->op_class))
		return NULL;

	ubpr->unsol_bcast_probe_resp_interval =
		hapd->conf->unsol_bcast_probe_resp_interval;

	os_memset(&probe_params, 0, sizeof(probe_params));
	probe_params.req = NULL;
	probe_params.is_p2p = false;
	probe_params.known_bss = NULL;
	probe_params.known_bss_len = 0;
	probe_params.is_ml_sta_info = false;
	probe_params.mld_ap = NULL;
	probe_params.mld_info = NULL;

	hostapd_gen_probe_resp(hapd, &probe_params);
	ubpr->unsol_bcast_probe_resp_tmpl_len = probe_params.resp_len;
	return (u8 *) probe_params.resp;
}
#endif /* CONFIG_IEEE80211AX */


void sta_track_del(struct hostapd_sta_info *info)
{
#ifdef CONFIG_TAXONOMY
	wpabuf_free(info->probe_ie_taxonomy);
	info->probe_ie_taxonomy = NULL;
#endif /* CONFIG_TAXONOMY */
	os_free(info);
}


#ifdef CONFIG_FILS

static u16 hostapd_gen_fils_discovery_phy_index(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be)
		return FD_CAP_PHY_INDEX_EHT;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax)
		return FD_CAP_PHY_INDEX_HE;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac)
		return FD_CAP_PHY_INDEX_VHT;
#endif /* CONFIG_IEEE80211AC */

	if (hapd->iconf->ieee80211n && !hapd->conf->disable_11n)
		return FD_CAP_PHY_INDEX_HT;

	return 0;
}


static u16 hostapd_gen_fils_discovery_nss(struct hostapd_hw_modes *mode,
					  u16 phy_index, u8 he_mcs_nss_size)
{
	u16 nss = 0;

	if (!mode)
		return 0;

	if (phy_index == FD_CAP_PHY_INDEX_HE) {
		const u8 *he_mcs = mode->he_capab[IEEE80211_MODE_AP].mcs;
		int i;
		u16 mcs[6];

		os_memset(mcs, 0xff, 6 * sizeof(u16));

		if (he_mcs_nss_size == 4) {
			mcs[0] = WPA_GET_LE16(&he_mcs[0]);
			mcs[1] = WPA_GET_LE16(&he_mcs[2]);
		}

		if (he_mcs_nss_size == 8) {
			mcs[2] = WPA_GET_LE16(&he_mcs[4]);
			mcs[3] = WPA_GET_LE16(&he_mcs[6]);
		}

		if (he_mcs_nss_size == 12) {
			mcs[4] = WPA_GET_LE16(&he_mcs[8]);
			mcs[5] = WPA_GET_LE16(&he_mcs[10]);
		}

		for (i = 0; i < HE_NSS_MAX_STREAMS; i++) {
			u16 nss_mask = 0x3 << (i * 2);

			/*
			 * If Tx and/or Rx indicate support for a given NSS,
			 * count it towards the maximum NSS.
			 */
			if (he_mcs_nss_size == 4 &&
			    (((mcs[0] & nss_mask) != nss_mask) ||
			     ((mcs[1] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}

			if (he_mcs_nss_size == 8 &&
			    (((mcs[2] & nss_mask) != nss_mask) ||
			     ((mcs[3] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}

			if (he_mcs_nss_size == 12 &&
			    (((mcs[4] & nss_mask) != nss_mask) ||
			     ((mcs[5] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}
		}
	} else if (phy_index == FD_CAP_PHY_INDEX_EHT) {
		u8 rx_nss, tx_nss, max_nss = 0, i;
		u8 *mcs = mode->eht_capab[IEEE80211_MODE_AP].mcs;

		/*
		 * The Supported EHT-MCS And NSS Set field for the AP contains
		 * one to three EHT-MCS Map fields based on the supported
		 * bandwidth. Check the first byte (max NSS for Rx/Tx that
		 * supports EHT-MCS 0-9) for each bandwidth (<= 80,
		 * 160, 320) to find the maximum NSS. This assumes that
		 * the lowest MCS rates support the largest number of spatial
		 * streams. If values are different between Tx, Rx or the
		 * bandwidths, choose the highest value.
		 */
		for (i = 0; i < 3; i++) {
			rx_nss = mcs[3 * i] & 0x0F;
			if (rx_nss > max_nss)
				max_nss = rx_nss;

			tx_nss = (mcs[3 * i] & 0xF0) >> 4;
			if (tx_nss > max_nss)
				max_nss = tx_nss;
		}

		nss = max_nss;
	}

	if (nss > 4)
		return FD_CAP_NSS_5_8 << FD_CAP_NSS_SHIFT;
	if (nss)
		return (nss - 1) << FD_CAP_NSS_SHIFT;

	return 0;
}


static u16 hostapd_fils_discovery_cap(struct hostapd_data *hapd)
{
	u16 cap_info, phy_index;
	u8 chwidth = FD_CAP_BSS_CHWIDTH_20, he_mcs_nss_size = 4;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;

	cap_info = FD_CAP_ESS;
	if (hapd->conf->wpa)
		cap_info |= FD_CAP_PRIVACY;

	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		switch (hapd->iconf->op_class) {
		case 137:
			chwidth = FD_CAP_BSS_CHWIDTH_320;
			break;
		case 135:
			he_mcs_nss_size += 4;
			/* fallthrough */
		case 134:
			he_mcs_nss_size += 4;
			chwidth = FD_CAP_BSS_CHWIDTH_160_80_80;
			break;
		case 133:
			chwidth = FD_CAP_BSS_CHWIDTH_80;
			break;
		case 132:
			chwidth = FD_CAP_BSS_CHWIDTH_40;
			break;
		}
	} else {
		switch (hostapd_get_oper_chwidth(hapd->iconf)) {
		case CONF_OPER_CHWIDTH_80P80MHZ:
			he_mcs_nss_size += 4;
			/* fallthrough */
		case CONF_OPER_CHWIDTH_160MHZ:
			he_mcs_nss_size += 4;
			chwidth = FD_CAP_BSS_CHWIDTH_160_80_80;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			chwidth = FD_CAP_BSS_CHWIDTH_80;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			if (hapd->iconf->secondary_channel)
				chwidth = FD_CAP_BSS_CHWIDTH_40;
			else
				chwidth = FD_CAP_BSS_CHWIDTH_20;
			break;
		default:
			break;
		}
	}

	phy_index = hostapd_gen_fils_discovery_phy_index(hapd);
	cap_info |= phy_index << FD_CAP_PHY_INDEX_SHIFT;
	cap_info |= chwidth << FD_CAP_BSS_CHWIDTH_SHIFT;
	cap_info |= hostapd_gen_fils_discovery_nss(mode, phy_index,
						   he_mcs_nss_size);
	return cap_info;
}


static u8 * hostapd_gen_fils_discovery(struct hostapd_data *hapd, size_t *len)
{
	struct ieee80211_mgmt *head;
	const u8 *mobility_domain;
	u8 *pos, *length_pos, buf[200];
	u16 ctl = 0;
	u8 fd_rsn_info[5];
	size_t total_len, buf_len;

	total_len = 24 + 2 + 12;

	/* FILS Discovery Frame Control */
	ctl = (sizeof(hapd->conf->ssid.short_ssid) - 1) |
		FD_FRAME_CTL_SHORT_SSID_PRESENT |
		FD_FRAME_CTL_LENGTH_PRESENT |
		FD_FRAME_CTL_CAP_PRESENT;
	total_len += 4 + 1 + 2;

	/* Fill primary channel information for 6 GHz channels with over 20 MHz
	 * bandwidth, if the primary channel is not a PSC */
	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    !is_6ghz_psc_frequency(ieee80211_chan_to_freq(
					   NULL, hapd->iconf->op_class,
					   hapd->iconf->channel)) &&
	    op_class_to_bandwidth(hapd->iconf->op_class) > 20) {
		ctl |= FD_FRAME_CTL_PRI_CHAN_PRESENT;
		total_len += 2;
	}

	/* Check for optional subfields and calculate length */
	if (wpa_auth_write_fd_rsn_info(hapd->wpa_auth, fd_rsn_info)) {
		ctl |= FD_FRAME_CTL_RSN_INFO_PRESENT;
		total_len += sizeof(fd_rsn_info);
	}

	mobility_domain = hostapd_wpa_ie(hapd, WLAN_EID_MOBILITY_DOMAIN);
	if (mobility_domain) {
		ctl |= FD_FRAME_CTL_MD_PRESENT;
		total_len += 3;
	}

	total_len += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_ACTION, true);

	pos = hostapd_eid_fils_indic(hapd, buf, 0);
	buf_len = pos - buf;
	total_len += buf_len;

	/* he_elem_len() may return too large a value for FD frame, but that is
	 * fine here since this is used as the maximum length of the buffer. */
	total_len += he_elem_len(hapd);

	head = os_zalloc(total_len);
	if (!head)
		return NULL;

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memset(head->da, 0xff, ETH_ALEN);
	os_memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(head->bssid, hapd->own_addr, ETH_ALEN);

	head->u.action.category = WLAN_ACTION_PUBLIC;
	head->u.action.u.public_action.action = WLAN_PA_FILS_DISCOVERY;

	pos = &head->u.action.u.public_action.variable[0];

	/* FILS Discovery Information field */

	/* FILS Discovery Frame Control */
	WPA_PUT_LE16(pos, ctl);
	pos += 2;

	/* Hardware or low-level driver will fill in the Timestamp value */
	pos += 8;

	/* Beacon Interval */
	WPA_PUT_LE16(pos, hapd->iconf->beacon_int);
	pos += 2;

	/* Short SSID */
	WPA_PUT_LE32(pos, hapd->conf->ssid.short_ssid);
	pos += sizeof(hapd->conf->ssid.short_ssid);

	/* Store position of FILS discovery information element Length field */
	length_pos = pos++;

	/* FD Capability */
	WPA_PUT_LE16(pos, hostapd_fils_discovery_cap(hapd));
	pos += 2;

	/* Operating Class and Primary Channel - if a 6 GHz chan is non PSC */
	if (ctl & FD_FRAME_CTL_PRI_CHAN_PRESENT) {
		*pos++ = hapd->iconf->op_class;
		*pos++ = hapd->iconf->channel;
	}

	/* AP Configuration Sequence Number - not present */

	/* Access Network Options - not present */

	/* FD RSN Information */
	if (ctl & FD_FRAME_CTL_RSN_INFO_PRESENT) {
		os_memcpy(pos, fd_rsn_info, sizeof(fd_rsn_info));
		pos += sizeof(fd_rsn_info);
	}

	/* Channel Center Frequency Segment 1 - not present */

	/* Mobility Domain */
	if (ctl & FD_FRAME_CTL_MD_PRESENT) {
		os_memcpy(pos, &mobility_domain[2], 3);
		pos += 3;
	}

	/* Fill in the Length field value */
	*length_pos = pos - (length_pos + 1);

	pos = hostapd_eid_rnr(hapd, pos, WLAN_FC_STYPE_ACTION, true);

	/* FILS Indication element */
	if (buf_len) {
		os_memcpy(pos, buf, buf_len);
		pos += buf_len;
	}

	if (is_6ghz_op_class(hapd->iconf->op_class))
		pos = hostapd_eid_txpower_envelope(hapd, pos);

	*len = pos - (u8 *) head;
	wpa_hexdump(MSG_DEBUG, "FILS Discovery frame template",
		    head, pos - (u8 *) head);
	return (u8 *) head;
}


/* Configure FILS Discovery frame transmission parameters */
static u8 * hostapd_fils_discovery(struct hostapd_data *hapd,
				   struct wpa_driver_ap_params *params)
{
	params->fd_max_int = hapd->conf->fils_discovery_max_int;
	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    params->fd_max_int > FD_MAX_INTERVAL_6GHZ)
		params->fd_max_int = FD_MAX_INTERVAL_6GHZ;

	params->fd_min_int = hapd->conf->fils_discovery_min_int;
	if (params->fd_min_int > params->fd_max_int)
		params->fd_min_int = params->fd_max_int;

	if (params->fd_max_int)
		return hostapd_gen_fils_discovery(hapd,
						  &params->fd_frame_tmpl_len);

	return NULL;
}

#endif /* CONFIG_FILS */


int ieee802_11_build_ap_params(struct hostapd_data *hapd,
			       struct wpa_driver_ap_params *params)
{
	struct ieee80211_mgmt *head = NULL;
	u8 *tail = NULL;
	size_t head_len = 0, tail_len = 0;
	u8 *resp = NULL;
	size_t resp_len = 0;
#ifdef NEED_AP_MLME
	u16 capab_info;
	u8 *pos, *tailpos, *tailend, *csa_pos;
	bool complete = false;
#endif /* NEED_AP_MLME */

	os_memset(params, 0, sizeof(*params));

#ifdef NEED_AP_MLME
#define BEACON_HEAD_BUF_SIZE 256
#define BEACON_TAIL_BUF_SIZE 512
	head = os_zalloc(BEACON_HEAD_BUF_SIZE);
	tail_len = BEACON_TAIL_BUF_SIZE;
#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie)
		tail_len += wpabuf_len(hapd->wps_beacon_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_beacon_ie)
		tail_len += wpabuf_len(hapd->p2p_beacon_ie);
#endif /* CONFIG_P2P */
#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		tail_len += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */
	if (hapd->conf->vendor_elements)
		tail_len += wpabuf_len(hapd->conf->vendor_elements);

#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht) {
		tail_len += 5 + 2 + sizeof(struct ieee80211_vht_capabilities) +
			2 + sizeof(struct ieee80211_vht_operation);
	}
#endif /* CONFIG_IEEE80211AC */

	tail_len += he_elem_len(hapd);

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		tail_len += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		tail_len += 3 + sizeof(struct ieee80211_eht_operation);
		if (hapd->iconf->punct_bitmap)
			tail_len += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

		/*
		 * TODO: Multi-Link element has variable length and can be
		 * long based on the common info and number of per
		 * station profiles. For now use 256.
		 */
		if (hapd->conf->mld_ap)
			tail_len += 256;
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
	    hapd == hostapd_mbssid_get_tx_bss(hapd))
		tail_len += 5; /* Multiple BSSID Configuration element */
	tail_len += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_BEACON, true);
	tail_len += hostapd_mbo_ie_len(hapd);
	tail_len += hostapd_eid_owe_trans_len(hapd);
	tail_len += hostapd_eid_dpp_cc_len(hapd);

	tailpos = tail = os_malloc(tail_len);
	if (head == NULL || tail == NULL) {
		wpa_printf(MSG_ERROR, "Failed to set beacon data");
		os_free(head);
		os_free(tail);
		return -1;
	}
	tailend = tail + tail_len;

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_BEACON);
	head->duration = host_to_le16(0);
	os_memset(head->da, 0xff, ETH_ALEN);

	os_memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(head->bssid, hapd->own_addr, ETH_ALEN);
	head->u.beacon.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	capab_info = hostapd_own_capab_info(hapd);
	head->u.beacon.capab_info = host_to_le16(capab_info);
	pos = &head->u.beacon.variable[0];

	/* SSID */
	*pos++ = WLAN_EID_SSID;
	if (hapd->conf->ignore_broadcast_ssid == 2) {
		/* clear the data, but keep the correct length of the SSID */
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memset(pos, 0, hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	} else if (hapd->conf->ignore_broadcast_ssid) {
		*pos++ = 0; /* empty SSID */
	} else {
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memcpy(pos, hapd->conf->ssid.ssid,
			  hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	head_len = pos - (u8 *) head;

	tailpos = hostapd_eid_country(hapd, tailpos, tailend - tailpos);

	/* Power Constraint element */
	tailpos = hostapd_eid_pwr_constraint(hapd, tailpos);

	/* CSA IE */
	csa_pos = hostapd_eid_csa(hapd, tailpos);
	if (csa_pos != tailpos)
		hapd->cs_c_off_beacon = csa_pos - tail - 1;
	tailpos = csa_pos;

	/* ERP Information element */
	tailpos = hostapd_eid_erp_info(hapd, tailpos);

	/* Extended supported rates */
	tailpos = hostapd_eid_ext_supp_rates(hapd, tailpos);

	tailpos = hostapd_get_rsne(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_bss_load(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_rm_enabled_capab(hapd, tailpos,
					       tailend - tailpos);
	tailpos = hostapd_get_mde(hapd, tailpos, tailend - tailpos);

	/* eCSA IE */
	csa_pos = hostapd_eid_ecsa(hapd, tailpos);
	if (csa_pos != tailpos)
		hapd->cs_c_off_ecsa_beacon = csa_pos - tail - 1;
	tailpos = csa_pos;

	tailpos = hostapd_eid_supported_op_classes(hapd, tailpos);
	tailpos = hostapd_eid_ht_capabilities(hapd, tailpos);
	tailpos = hostapd_eid_ht_operation(hapd, tailpos);

	if (hapd->iconf->mbssid && hapd->iconf->num_bss > 1) {
		if (ieee802_11_build_ap_params_mbssid(hapd, params)) {
			os_free(head);
			os_free(tail);
			wpa_printf(MSG_ERROR,
				   "MBSSID: Failed to set beacon data");
			return -1;
		}
		complete = hapd->iconf->mbssid == MBSSID_ENABLED ||
			(hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
			 params->mbssid_elem_count == 1);
	}

	tailpos = hostapd_eid_ext_capab(hapd, tailpos, complete);

	/*
	 * TODO: Time Advertisement element should only be included in some
	 * DTIM Beacon frames.
	 */
	tailpos = hostapd_eid_time_adv(hapd, tailpos);

	tailpos = hostapd_eid_interworking(hapd, tailpos);
	tailpos = hostapd_eid_adv_proto(hapd, tailpos);
	tailpos = hostapd_eid_roaming_consortium(hapd, tailpos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(tailpos, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		tailpos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		tailpos = hostapd_eid_vht_capabilities(hapd, tailpos, 0);
		tailpos = hostapd_eid_vht_operation(hapd, tailpos);
		tailpos = hostapd_eid_txpower_envelope(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax &&
	    is_6ghz_op_class(hapd->iconf->op_class))
		tailpos = hostapd_eid_txpower_envelope(hapd, tailpos);
#endif /* CONFIG_IEEE80211AX */

	tailpos = hostapd_eid_wb_chsw_wrapper(hapd, tailpos);

	tailpos = hostapd_eid_rnr(hapd, tailpos, WLAN_FC_STYPE_BEACON, true);
	tailpos = hostapd_eid_fils_indic(hapd, tailpos, 0);
	tailpos = hostapd_get_rsnxe(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_mbssid_config(hapd, tailpos,
					    params->mbssid_elem_count);

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax) {
		u8 *cca_pos;

		tailpos = hostapd_eid_he_capab(hapd, tailpos,
					       IEEE80211_MODE_AP);
		tailpos = hostapd_eid_he_operation(hapd, tailpos);

		/* BSS Color Change Announcement element */
		cca_pos = hostapd_eid_cca(hapd, tailpos);
		if (cca_pos != tailpos)
			hapd->cca_c_off_beacon = cca_pos - tail - 2;
		tailpos = cca_pos;

		tailpos = hostapd_eid_spatial_reuse(hapd, tailpos);
		tailpos = hostapd_eid_he_mu_edca_parameter_set(hapd, tailpos);
		tailpos = hostapd_eid_he_6ghz_band_cap(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		if (hapd->conf->mld_ap)
			tailpos = hostapd_eid_eht_ml_beacon(hapd, NULL,
							    tailpos, false);
		tailpos = hostapd_eid_eht_capab(hapd, tailpos,
						IEEE80211_MODE_AP);
		tailpos = hostapd_eid_eht_operation(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht)
		tailpos = hostapd_eid_vendor_vht(hapd, tailpos);
#endif /* CONFIG_IEEE80211AC */

	/* WPA / OSEN */
	tailpos = hostapd_get_wpa_ie(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_get_osen_ie(hapd, tailpos, tailend - tailpos);

	/* Wi-Fi Alliance WMM */
	tailpos = hostapd_eid_wmm(hapd, tailpos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->wps_beacon_ie),
			  wpabuf_len(hapd->wps_beacon_ie));
		tailpos += wpabuf_len(hapd->wps_beacon_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->p2p_beacon_ie),
			  wpabuf_len(hapd->p2p_beacon_ie));
		tailpos += wpabuf_len(hapd->p2p_beacon_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		tailpos = hostapd_eid_p2p_manage(hapd, tailpos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	tailpos = hostapd_eid_hs20_indication(hapd, tailpos);
#endif /* CONFIG_HS20 */

	tailpos = hostapd_eid_mbo(hapd, tailpos, tail + tail_len - tailpos);
	tailpos = hostapd_eid_owe_trans(hapd, tailpos,
					tail + tail_len - tailpos);
	tailpos = hostapd_eid_dpp_cc(hapd, tailpos, tail + tail_len - tailpos);

	if (hapd->conf->vendor_elements) {
		os_memcpy(tailpos, wpabuf_head(hapd->conf->vendor_elements),
			  wpabuf_len(hapd->conf->vendor_elements));
		tailpos += wpabuf_len(hapd->conf->vendor_elements);
	}

	tail_len = tailpos > tail ? tailpos - tail : 0;

	resp = hostapd_probe_resp_offloads(hapd, &resp_len);
#endif /* NEED_AP_MLME */

	/* If key management offload is enabled, configure PSK to the driver. */
	if (wpa_key_mgmt_wpa_psk_no_sae(hapd->conf->wpa_key_mgmt) &&
	    (hapd->iface->drv_flags2 &
	     WPA_DRIVER_FLAGS2_4WAY_HANDSHAKE_AP_PSK)) {
		if (hapd->conf->ssid.wpa_psk && hapd->conf->ssid.wpa_psk_set) {
			os_memcpy(params->psk, hapd->conf->ssid.wpa_psk->psk,
				  PMK_LEN);
			params->psk_len = PMK_LEN;
		} else if (hapd->conf->ssid.wpa_passphrase &&
			   pbkdf2_sha1(hapd->conf->ssid.wpa_passphrase,
				       hapd->conf->ssid.ssid,
				       hapd->conf->ssid.ssid_len, 4096,
				       params->psk, PMK_LEN) == 0) {
			params->psk_len = PMK_LEN;
		}
	}

#ifdef CONFIG_SAE
	/* If SAE offload is enabled, provide password to lower layer for
	 * SAE authentication and PMK generation.
	 */
	if (wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt) &&
	    (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SAE_OFFLOAD_AP)) {
		if (hostapd_sae_pk_in_use(hapd->conf)) {
			wpa_printf(MSG_ERROR,
				   "SAE PK not supported with SAE offload");
			return -1;
		}

		if (hostapd_sae_pw_id_in_use(hapd->conf)) {
			wpa_printf(MSG_ERROR,
				   "SAE Password Identifiers not supported with SAE offload");
			return -1;
		}

		params->sae_password = sae_get_password(hapd, NULL, NULL, NULL,
							NULL, NULL);
		if (!params->sae_password) {
			wpa_printf(MSG_ERROR, "SAE password not configured for offload");
			return -1;
		}
	}
#endif /* CONFIG_SAE */

	params->head = (u8 *) head;
	params->head_len = head_len;
	params->tail = tail;
	params->tail_len = tail_len;
	params->proberesp = resp;
	params->proberesp_len = resp_len;
	params->dtim_period = hapd->conf->dtim_period;
	params->beacon_int = hapd->iconf->beacon_int;
	params->basic_rates = hapd->iface->basic_rates;
	params->beacon_rate = hapd->iconf->beacon_rate;
	params->rate_type = hapd->iconf->rate_type;
	params->ssid = hapd->conf->ssid.ssid;
	params->ssid_len = hapd->conf->ssid.ssid_len;
	if ((hapd->conf->wpa & (WPA_PROTO_WPA | WPA_PROTO_RSN)) ==
	    (WPA_PROTO_WPA | WPA_PROTO_RSN))
		params->pairwise_ciphers = hapd->conf->wpa_pairwise |
			hapd->conf->rsn_pairwise;
	else if (hapd->conf->wpa & WPA_PROTO_RSN)
		params->pairwise_ciphers = hapd->conf->rsn_pairwise;
	else if (hapd->conf->wpa & WPA_PROTO_WPA)
		params->pairwise_ciphers = hapd->conf->wpa_pairwise;
	params->group_cipher = hapd->conf->wpa_group;
	params->key_mgmt_suites = hapd->conf->wpa_key_mgmt;
	params->auth_algs = hapd->conf->auth_algs;
	params->wpa_version = hapd->conf->wpa;
	params->privacy = hapd->conf->wpa;
#ifdef CONFIG_WEP
	params->privacy |= hapd->conf->ssid.wep.keys_set ||
		(hapd->conf->ieee802_1x &&
		 (hapd->conf->default_wep_key_len ||
		  hapd->conf->individual_wep_key_len));
#endif /* CONFIG_WEP */
	switch (hapd->conf->ignore_broadcast_ssid) {
	case 0:
		params->hide_ssid = NO_SSID_HIDING;
		break;
	case 1:
		params->hide_ssid = HIDDEN_SSID_ZERO_LEN;
		break;
	case 2:
		params->hide_ssid = HIDDEN_SSID_ZERO_CONTENTS;
		break;
	}
	params->isolate = hapd->conf->isolate;
#ifdef NEED_AP_MLME
	params->cts_protect = !!(ieee802_11_erp_info(hapd) &
				ERP_INFO_USE_PROTECTION);
	params->preamble = hapd->iface->num_sta_no_short_preamble == 0 &&
		hapd->iconf->preamble == SHORT_PREAMBLE;
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		params->short_slot_time =
			hapd->iface->num_sta_no_short_slot_time > 0 ? 0 : 1;
	else
		params->short_slot_time = -1;
	if (!hapd->iconf->ieee80211n || hapd->conf->disable_11n)
		params->ht_opmode = -1;
	else
		params->ht_opmode = hapd->iface->ht_op_mode;
#endif /* NEED_AP_MLME */
	params->interworking = hapd->conf->interworking;
	if (hapd->conf->interworking &&
	    !is_zero_ether_addr(hapd->conf->hessid))
		params->hessid = hapd->conf->hessid;
	params->access_network_type = hapd->conf->access_network_type;
	params->ap_max_inactivity = hapd->conf->ap_max_inactivity;
#ifdef CONFIG_P2P
	params->p2p_go_ctwindow = hapd->iconf->p2p_go_ctwindow;
#endif /* CONFIG_P2P */
#ifdef CONFIG_HS20
	params->disable_dgaf = hapd->conf->disable_dgaf;
	if (hapd->conf->osen) {
		params->privacy = 1;
		params->osen = 1;
	}
#endif /* CONFIG_HS20 */
	params->multicast_to_unicast = hapd->conf->multicast_to_unicast;
	params->pbss = hapd->conf->pbss;

	if (hapd->conf->ftm_responder) {
		if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_FTM_RESPONDER) {
			params->ftm_responder = 1;
			params->lci = hapd->iface->conf->lci;
			params->civic = hapd->iface->conf->civic;
		} else {
			wpa_printf(MSG_WARNING,
				   "Not configuring FTM responder as the driver doesn't advertise support for it");
		}
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && hapd->iconf->ieee80211be &&
	    !hapd->conf->disable_11be) {
		params->mld_ap = true;
		params->mld_link_id = hapd->mld_link_id;
	}
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


void ieee802_11_free_ap_params(struct wpa_driver_ap_params *params)
{
	os_free(params->tail);
	params->tail = NULL;
	os_free(params->head);
	params->head = NULL;
	os_free(params->proberesp);
	params->proberesp = NULL;
	os_free(params->mbssid_elem);
	params->mbssid_elem = NULL;
	os_free(params->mbssid_elem_offset);
	params->mbssid_elem_offset = NULL;
	os_free(params->rnr_elem);
	params->rnr_elem = NULL;
	os_free(params->rnr_elem_offset);
	params->rnr_elem_offset = NULL;
#ifdef CONFIG_FILS
	os_free(params->fd_frame_tmpl);
	params->fd_frame_tmpl = NULL;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211AX
	os_free(params->ubpr.unsol_bcast_probe_resp_tmpl);
	params->ubpr.unsol_bcast_probe_resp_tmpl = NULL;
#endif /* CONFIG_IEEE80211AX */
	os_free(params->allowed_freqs);
	params->allowed_freqs = NULL;
}


static int __ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct wpa_driver_ap_params params;
	struct hostapd_freq_params freq;
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	struct hostapd_hw_modes *cmode = iface->current_mode;
	struct wpabuf *beacon, *proberesp, *assocresp;
	bool twt_he_responder = false;
	int res, ret = -1, i;
	struct hostapd_hw_modes *mode;

	if (!hapd->drv_priv) {
		wpa_printf(MSG_ERROR, "Interface is disabled");
		return -1;
	}

	if (hapd->csa_in_progress) {
		wpa_printf(MSG_ERROR, "Cannot set beacons during CSA period");
		return -1;
	}

	hapd->beacon_set_done = 1;

	if (ieee802_11_build_ap_params(hapd, &params) < 0)
		return -1;

	if (hostapd_build_ap_extra_ies(hapd, &beacon, &proberesp, &assocresp) <
	    0)
		goto fail;

	params.beacon_ies = beacon;
	params.proberesp_ies = proberesp;
	params.assocresp_ies = assocresp;
	params.reenable = hapd->reenable_beacon;
#ifdef CONFIG_IEEE80211AX
	params.he_spr_ctrl = hapd->iface->conf->spr.sr_control;
	params.he_spr_non_srg_obss_pd_max_offset =
		hapd->iface->conf->spr.non_srg_obss_pd_max_offset;
	params.he_spr_srg_obss_pd_min_offset =
		hapd->iface->conf->spr.srg_obss_pd_min_offset;
	params.he_spr_srg_obss_pd_max_offset =
		hapd->iface->conf->spr.srg_obss_pd_max_offset;
	os_memcpy(params.he_spr_bss_color_bitmap,
		  hapd->iface->conf->spr.srg_bss_color_bitmap, 8);
	os_memcpy(params.he_spr_partial_bssid_bitmap,
		  hapd->iface->conf->spr.srg_partial_bssid_bitmap, 8);
	params.he_bss_color_disabled =
		hapd->iface->conf->he_op.he_bss_color_disabled;
	params.he_bss_color_partial =
		hapd->iface->conf->he_op.he_bss_color_partial;
	params.he_bss_color = hapd->iface->conf->he_op.he_bss_color;
	twt_he_responder = hostapd_get_he_twt_responder(hapd,
							IEEE80211_MODE_AP);
	params.ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &params.ubpr);
#endif /* CONFIG_IEEE80211AX */
	params.twt_responder =
		twt_he_responder || hostapd_get_ht_vht_twt_responder(hapd);
	hapd->reenable_beacon = 0;
#ifdef CONFIG_SAE
	params.sae_pwe = hapd->conf->sae_pwe;
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	params.fd_frame_tmpl = hostapd_fils_discovery(hapd, &params);
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
	params.punct_bitmap = iconf->punct_bitmap;
#endif /* CONFIG_IEEE80211BE */

	if (cmode &&
	    hostapd_set_freq_params(&freq, iconf->hw_mode, iface->freq,
				    iconf->channel, iconf->enable_edmg,
				    iconf->edmg_channel, iconf->ieee80211n,
				    iconf->ieee80211ac, iconf->ieee80211ax,
				    iconf->ieee80211be,
				    iconf->secondary_channel,
				    hostapd_get_oper_chwidth(iconf),
				    hostapd_get_oper_centr_freq_seg0_idx(iconf),
				    hostapd_get_oper_centr_freq_seg1_idx(iconf),
				    cmode->vht_capab,
				    &cmode->he_capab[IEEE80211_MODE_AP],
				    &cmode->eht_capab[IEEE80211_MODE_AP],
				    hostapd_get_punct_bitmap(hapd)) == 0) {
		freq.link_id = -1;
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap)
			freq.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
		params.freq = &freq;
	}

	for (i = 0; i < hapd->iface->num_hw_features; i++) {
		mode = &hapd->iface->hw_features[i];

		if (iconf->hw_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    iconf->hw_mode != mode->mode)
			continue;

		hostapd_get_hw_mode_any_channels(hapd, mode,
						 !(iconf->acs_freq_list.num ||
						   iconf->acs_ch_list.num),
						 true, &params.allowed_freqs);
	}

	res = hostapd_drv_set_ap(hapd, &params);
	hostapd_free_ap_extra_ies(hapd, beacon, proberesp, assocresp);
	if (res)
		wpa_printf(MSG_ERROR, "Failed to set beacon parameters");
	else
		ret = 0;
fail:
	ieee802_11_free_ap_params(&params);
	return ret;
}


void ieee802_11_set_beacon_per_bss_only(struct hostapd_data *hapd)
{
	__ieee802_11_set_beacon(hapd);
}


int ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	int ret;
	size_t i, j;
	bool is_6g, hapd_mld = false;

	ret = __ieee802_11_set_beacon(hapd);
	if (ret != 0)
		return ret;

	if (!iface->interfaces || iface->interfaces->count <= 1)
		return 0;

#ifdef CONFIG_IEEE80211BE
	hapd_mld = hapd->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

	/* Update Beacon frames in case of 6 GHz colocation or AP MLD */
	is_6g = is_6ghz_op_class(iface->conf->op_class);
	for (j = 0; j < iface->interfaces->count; j++) {
		struct hostapd_iface *other;
		bool other_iface_6g;

		other = iface->interfaces->iface[j];
		if (other == iface || !other || !other->conf)
			continue;

		other_iface_6g = is_6ghz_op_class(other->conf->op_class);

		if (is_6g == other_iface_6g && !hapd_mld)
			continue;

		for (i = 0; i < other->num_bss; i++) {
#ifdef CONFIG_IEEE80211BE
			if (is_6g == other_iface_6g &&
			    !(hapd_mld && other->bss[i]->conf->mld_ap &&
			      hostapd_is_ml_partner(hapd, other->bss[i])))
				continue;
#endif /* CONFIG_IEEE80211BE */

			if (other->bss[i] && other->bss[i]->started)
				__ieee802_11_set_beacon(other->bss[i]);
		}
	}

	return 0;
}


int ieee802_11_set_beacons(struct hostapd_iface *iface)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->started &&
		    ieee802_11_set_beacon(iface->bss[i]) < 0)
			ret = -1;
	}

	return ret;
}


/* only update beacons if started */
int ieee802_11_update_beacons(struct hostapd_iface *iface)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->beacon_set_done && iface->bss[i]->started &&
		    ieee802_11_set_beacon(iface->bss[i]) < 0)
			ret = -1;
	}

	return ret;
}

#endif /* CONFIG_NATIVE_WINDOWS */
