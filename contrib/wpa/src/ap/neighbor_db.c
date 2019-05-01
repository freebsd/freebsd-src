/*
 * hostapd / Neighboring APs DB
 * Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "neighbor_db.h"


struct hostapd_neighbor_entry *
hostapd_neighbor_get(struct hostapd_data *hapd, const u8 *bssid,
		     const struct wpa_ssid_value *ssid)
{
	struct hostapd_neighbor_entry *nr;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (os_memcmp(bssid, nr->bssid, ETH_ALEN) == 0 &&
		    (!ssid ||
		     (ssid->ssid_len == nr->ssid.ssid_len &&
		      os_memcmp(ssid->ssid, nr->ssid.ssid,
				ssid->ssid_len) == 0)))
			return nr;
	}
	return NULL;
}


static void hostapd_neighbor_clear_entry(struct hostapd_neighbor_entry *nr)
{
	wpabuf_free(nr->nr);
	nr->nr = NULL;
	wpabuf_free(nr->lci);
	nr->lci = NULL;
	wpabuf_free(nr->civic);
	nr->civic = NULL;
	os_memset(nr->bssid, 0, sizeof(nr->bssid));
	os_memset(&nr->ssid, 0, sizeof(nr->ssid));
	nr->stationary = 0;
}


static struct hostapd_neighbor_entry *
hostapd_neighbor_add(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr;

	nr = os_zalloc(sizeof(struct hostapd_neighbor_entry));
	if (!nr)
		return NULL;

	dl_list_add(&hapd->nr_db, &nr->list);

	return nr;
}


int hostapd_neighbor_set(struct hostapd_data *hapd, const u8 *bssid,
			 const struct wpa_ssid_value *ssid,
			 const struct wpabuf *nr, const struct wpabuf *lci,
			 const struct wpabuf *civic, int stationary)
{
	struct hostapd_neighbor_entry *entry;

	entry = hostapd_neighbor_get(hapd, bssid, ssid);
	if (!entry)
		entry = hostapd_neighbor_add(hapd);
	if (!entry)
		return -1;

	hostapd_neighbor_clear_entry(entry);

	os_memcpy(entry->bssid, bssid, ETH_ALEN);
	os_memcpy(&entry->ssid, ssid, sizeof(entry->ssid));

	entry->nr = wpabuf_dup(nr);
	if (!entry->nr)
		goto fail;

	if (lci && wpabuf_len(lci)) {
		entry->lci = wpabuf_dup(lci);
		if (!entry->lci || os_get_time(&entry->lci_date))
			goto fail;
	}

	if (civic && wpabuf_len(civic)) {
		entry->civic = wpabuf_dup(civic);
		if (!entry->civic)
			goto fail;
	}

	entry->stationary = stationary;

	return 0;

fail:
	hostapd_neighbor_remove(hapd, bssid, ssid);
	return -1;
}


int hostapd_neighbor_remove(struct hostapd_data *hapd, const u8 *bssid,
			    const struct wpa_ssid_value *ssid)
{
	struct hostapd_neighbor_entry *nr;

	nr = hostapd_neighbor_get(hapd, bssid, ssid);
	if (!nr)
		return -1;

	hostapd_neighbor_clear_entry(nr);
	dl_list_del(&nr->list);
	os_free(nr);

	return 0;
}


void hostapd_free_neighbor_db(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr, *prev;

	dl_list_for_each_safe(nr, prev, &hapd->nr_db,
			      struct hostapd_neighbor_entry, list) {
		hostapd_neighbor_clear_entry(nr);
		dl_list_del(&nr->list);
		os_free(nr);
	}
}


#ifdef NEED_AP_MLME
static enum nr_chan_width hostapd_get_nr_chan_width(struct hostapd_data *hapd,
						    int ht, int vht)
{
	if (!ht && !vht)
		return NR_CHAN_WIDTH_20;
	if (!hapd->iconf->secondary_channel)
		return NR_CHAN_WIDTH_20;
	if (!vht || hapd->iconf->vht_oper_chwidth == VHT_CHANWIDTH_USE_HT)
		return NR_CHAN_WIDTH_40;
	if (hapd->iconf->vht_oper_chwidth == VHT_CHANWIDTH_80MHZ)
		return NR_CHAN_WIDTH_80;
	if (hapd->iconf->vht_oper_chwidth == VHT_CHANWIDTH_160MHZ)
		return NR_CHAN_WIDTH_160;
	if (hapd->iconf->vht_oper_chwidth == VHT_CHANWIDTH_80P80MHZ)
		return NR_CHAN_WIDTH_80P80;
	return NR_CHAN_WIDTH_20;
}
#endif /* NEED_AP_MLME */


void hostapd_neighbor_set_own_report(struct hostapd_data *hapd)
{
#ifdef NEED_AP_MLME
	u16 capab = hostapd_own_capab_info(hapd);
	int ht = hapd->iconf->ieee80211n && !hapd->conf->disable_11n;
	int vht = hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac;
	struct wpa_ssid_value ssid;
	u8 channel, op_class;
	u8 center_freq1_idx = 0, center_freq2_idx = 0;
	enum nr_chan_width width;
	u32 bssid_info;
	struct wpabuf *nr;

	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT))
		return;

	bssid_info = 3; /* AP is reachable */
	bssid_info |= NEI_REP_BSSID_INFO_SECURITY; /* "same as the AP" */
	bssid_info |= NEI_REP_BSSID_INFO_KEY_SCOPE; /* "same as the AP" */

	if (capab & WLAN_CAPABILITY_SPECTRUM_MGMT)
		bssid_info |= NEI_REP_BSSID_INFO_SPECTRUM_MGMT;

	bssid_info |= NEI_REP_BSSID_INFO_RM; /* RRM is supported */

	if (hapd->conf->wmm_enabled) {
		bssid_info |= NEI_REP_BSSID_INFO_QOS;

		if (hapd->conf->wmm_uapsd &&
		    (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_UAPSD))
			bssid_info |= NEI_REP_BSSID_INFO_APSD;
	}

	if (ht) {
		bssid_info |= NEI_REP_BSSID_INFO_HT |
			NEI_REP_BSSID_INFO_DELAYED_BA;

		/* VHT bit added in IEEE P802.11-REVmc/D4.3 */
		if (vht)
			bssid_info |= NEI_REP_BSSID_INFO_VHT;
	}

	/* TODO: Set NEI_REP_BSSID_INFO_MOBILITY_DOMAIN if MDE is set */

	if (ieee80211_freq_to_channel_ext(hapd->iface->freq,
					  hapd->iconf->secondary_channel,
					  hapd->iconf->vht_oper_chwidth,
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return;
	width = hostapd_get_nr_chan_width(hapd, ht, vht);
	if (vht) {
		center_freq1_idx = hapd->iconf->vht_oper_centr_freq_seg0_idx;
		if (width == NR_CHAN_WIDTH_80P80)
			center_freq2_idx =
				hapd->iconf->vht_oper_centr_freq_seg1_idx;
	} else if (ht) {
		ieee80211_freq_to_chan(hapd->iface->freq +
				       10 * hapd->iconf->secondary_channel,
				       &center_freq1_idx);
	}

	ssid.ssid_len = hapd->conf->ssid.ssid_len;
	os_memcpy(ssid.ssid, hapd->conf->ssid.ssid, ssid.ssid_len);

	/*
	 * Neighbor Report element size = BSSID + BSSID info + op_class + chan +
	 * phy type + wide bandwidth channel subelement.
	 */
	nr = wpabuf_alloc(ETH_ALEN + 4 + 1 + 1 + 1 + 5);
	if (!nr)
		return;

	wpabuf_put_data(nr, hapd->own_addr, ETH_ALEN);
	wpabuf_put_le32(nr, bssid_info);
	wpabuf_put_u8(nr, op_class);
	wpabuf_put_u8(nr, channel);
	wpabuf_put_u8(nr, ieee80211_get_phy_type(hapd->iface->freq, ht, vht));

	/*
	 * Wide Bandwidth Channel subelement may be needed to allow the
	 * receiving STA to send packets to the AP. See IEEE P802.11-REVmc/D5.0
	 * Figure 9-301.
	 */
	wpabuf_put_u8(nr, WNM_NEIGHBOR_WIDE_BW_CHAN);
	wpabuf_put_u8(nr, 3);
	wpabuf_put_u8(nr, width);
	wpabuf_put_u8(nr, center_freq1_idx);
	wpabuf_put_u8(nr, center_freq2_idx);

	hostapd_neighbor_set(hapd, hapd->own_addr, &ssid, nr, hapd->iconf->lci,
			     hapd->iconf->civic, hapd->iconf->stationary_ap);

	wpabuf_free(nr);
#endif /* NEED_AP_MLME */
}
