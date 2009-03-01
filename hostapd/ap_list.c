/*
 * hostapd / AP table
 * Copyright (c) 2002-2003, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2006, Devicescape Software, Inc.
 * Copyright (c) 2007-2008, Intel Corporation
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
#include "ieee802_11.h"
#include "eloop.h"
#include "ap_list.h"
#include "hw_features.h"
#include "beacon.h"


struct ieee80211_frame_info {
	u32 version;
	u32 length;
	u64 mactime;
	u64 hosttime;
	u32 phytype;
	u32 channel;
	u32 datarate;
	u32 antenna;
	u32 priority;
	u32 ssi_type;
	u32 ssi_signal;
	u32 ssi_noise;
	u32 preamble;
	u32 encoding;

	/* Note: this structure is otherwise identical to capture format used
	 * in linux-wlan-ng, but this additional field is used to provide meta
	 * data about the frame to hostapd. This was the easiest method for
	 * providing this information, but this might change in the future. */
	u32 msg_type;
} __attribute__ ((packed));


enum ieee80211_phytype {
	ieee80211_phytype_fhss_dot11_97  = 1,
	ieee80211_phytype_dsss_dot11_97  = 2,
	ieee80211_phytype_irbaseband     = 3,
	ieee80211_phytype_dsss_dot11_b   = 4,
	ieee80211_phytype_pbcc_dot11_b   = 5,
	ieee80211_phytype_ofdm_dot11_g   = 6,
	ieee80211_phytype_pbcc_dot11_g   = 7,
	ieee80211_phytype_ofdm_dot11_a   = 8,
	ieee80211_phytype_dsss_dot11_turbog = 255,
	ieee80211_phytype_dsss_dot11_turbo = 256,
};


/* AP list is a double linked list with head->prev pointing to the end of the
 * list and tail->next = NULL. Entries are moved to the head of the list
 * whenever a beacon has been received from the AP in question. The tail entry
 * in this link will thus be the least recently used entry. */


static void ap_list_new_ap(struct hostapd_iface *iface, struct ap_info *ap)
{
	wpa_printf(MSG_DEBUG, "New AP detected: " MACSTR, MAC2STR(ap->addr));

	/* TODO: could send a notification message to an external program that
	 * would then determine whether a rogue AP has been detected */
}


static void ap_list_expired_ap(struct hostapd_iface *iface, struct ap_info *ap)
{
	wpa_printf(MSG_DEBUG, "AP info expired: " MACSTR, MAC2STR(ap->addr));

	/* TODO: could send a notification message to an external program */
}


static int ap_list_beacon_olbc(struct hostapd_iface *iface, struct ap_info *ap)
{
	int i;

	if (iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G ||
	    ap->phytype != ieee80211_phytype_pbcc_dot11_g ||
	    iface->conf->channel != ap->channel)
		return 0;

	if (ap->erp != -1 && (ap->erp & ERP_INFO_NON_ERP_PRESENT))
		return 1;

	for (i = 0; i < WLAN_SUPP_RATES_MAX; i++) {
		int rate = (ap->supported_rates[i] & 0x7f) * 5;
		if (rate == 60 || rate == 90 || rate > 110)
			return 0;
	}

	return 1;
}


#ifdef CONFIG_IEEE80211N
static int ap_list_beacon_olbc_ht(struct hostapd_iface *iface,
				  struct ap_info *ap)
{
	return !ap->ht_support;
}
#endif /* CONFIG_IEEE80211N */


struct ap_info * ap_get_ap(struct hostapd_iface *iface, u8 *ap)
{
	struct ap_info *s;

	s = iface->ap_hash[STA_HASH(ap)];
	while (s != NULL && os_memcmp(s->addr, ap, ETH_ALEN) != 0)
		s = s->hnext;
	return s;
}


static void ap_ap_list_add(struct hostapd_iface *iface, struct ap_info *ap)
{
	if (iface->ap_list) {
		ap->prev = iface->ap_list->prev;
		iface->ap_list->prev = ap;
	} else
		ap->prev = ap;
	ap->next = iface->ap_list;
	iface->ap_list = ap;
}


static void ap_ap_list_del(struct hostapd_iface *iface, struct ap_info *ap)
{
	if (iface->ap_list == ap)
		iface->ap_list = ap->next;
	else
		ap->prev->next = ap->next;

	if (ap->next)
		ap->next->prev = ap->prev;
	else if (iface->ap_list)
		iface->ap_list->prev = ap->prev;
}


static void ap_ap_iter_list_add(struct hostapd_iface *iface,
				struct ap_info *ap)
{
	if (iface->ap_iter_list) {
		ap->iter_prev = iface->ap_iter_list->iter_prev;
		iface->ap_iter_list->iter_prev = ap;
	} else
		ap->iter_prev = ap;
	ap->iter_next = iface->ap_iter_list;
	iface->ap_iter_list = ap;
}


static void ap_ap_iter_list_del(struct hostapd_iface *iface,
				struct ap_info *ap)
{
	if (iface->ap_iter_list == ap)
		iface->ap_iter_list = ap->iter_next;
	else
		ap->iter_prev->iter_next = ap->iter_next;

	if (ap->iter_next)
		ap->iter_next->iter_prev = ap->iter_prev;
	else if (iface->ap_iter_list)
		iface->ap_iter_list->iter_prev = ap->iter_prev;
}


static void ap_ap_hash_add(struct hostapd_iface *iface, struct ap_info *ap)
{
	ap->hnext = iface->ap_hash[STA_HASH(ap->addr)];
	iface->ap_hash[STA_HASH(ap->addr)] = ap;
}


static void ap_ap_hash_del(struct hostapd_iface *iface, struct ap_info *ap)
{
	struct ap_info *s;

	s = iface->ap_hash[STA_HASH(ap->addr)];
	if (s == NULL) return;
	if (os_memcmp(s->addr, ap->addr, ETH_ALEN) == 0) {
		iface->ap_hash[STA_HASH(ap->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL &&
	       os_memcmp(s->hnext->addr, ap->addr, ETH_ALEN) != 0)
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		printf("AP: could not remove AP " MACSTR " from hash table\n",
		       MAC2STR(ap->addr));
}


static void ap_free_ap(struct hostapd_iface *iface, struct ap_info *ap)
{
	ap_ap_hash_del(iface, ap);
	ap_ap_list_del(iface, ap);
	ap_ap_iter_list_del(iface, ap);

	iface->num_ap--;
	os_free(ap);
}


static void hostapd_free_aps(struct hostapd_iface *iface)
{
	struct ap_info *ap, *prev;

	ap = iface->ap_list;

	while (ap) {
		prev = ap;
		ap = ap->next;
		ap_free_ap(iface, prev);
	}

	iface->ap_list = NULL;
}


int ap_ap_for_each(struct hostapd_iface *iface,
		   int (*func)(struct ap_info *s, void *data), void *data)
{
	struct ap_info *s;
	int ret = 0;

	s = iface->ap_list;

	while (s) {
		ret = func(s, data);
		if (ret)
			break;
		s = s->next;
	}

	return ret;
}


static struct ap_info * ap_ap_add(struct hostapd_iface *iface, u8 *addr)
{
	struct ap_info *ap;

	ap = os_zalloc(sizeof(struct ap_info));
	if (ap == NULL)
		return NULL;

	/* initialize AP info data */
	os_memcpy(ap->addr, addr, ETH_ALEN);
	ap_ap_list_add(iface, ap);
	iface->num_ap++;
	ap_ap_hash_add(iface, ap);
	ap_ap_iter_list_add(iface, ap);

	if (iface->num_ap > iface->conf->ap_table_max_size && ap != ap->prev) {
		wpa_printf(MSG_DEBUG, "Removing the least recently used AP "
			   MACSTR " from AP table", MAC2STR(ap->prev->addr));
		if (iface->conf->passive_scan_interval > 0)
			ap_list_expired_ap(iface, ap->prev);
		ap_free_ap(iface, ap->prev);
	}

	return ap;
}


void ap_list_process_beacon(struct hostapd_iface *iface,
			    struct ieee80211_mgmt *mgmt,
			    struct ieee802_11_elems *elems,
			    struct hostapd_frame_info *fi)
{
	struct ap_info *ap;
	int new_ap = 0;
	size_t len;
	int set_beacon = 0;

	if (iface->conf->ap_table_max_size < 1)
		return;

	ap = ap_get_ap(iface, mgmt->bssid);
	if (!ap) {
		ap = ap_ap_add(iface, mgmt->bssid);
		if (!ap) {
			printf("Failed to allocate AP information entry\n");
			return;
		}
		new_ap = 1;
	}

	ap->beacon_int = le_to_host16(mgmt->u.beacon.beacon_int);
	ap->capability = le_to_host16(mgmt->u.beacon.capab_info);

	if (elems->ssid) {
		len = elems->ssid_len;
		if (len >= sizeof(ap->ssid))
			len = sizeof(ap->ssid) - 1;
		os_memcpy(ap->ssid, elems->ssid, len);
		ap->ssid[len] = '\0';
		ap->ssid_len = len;
	}

	os_memset(ap->supported_rates, 0, WLAN_SUPP_RATES_MAX);
	len = 0;
	if (elems->supp_rates) {
		len = elems->supp_rates_len;
		if (len > WLAN_SUPP_RATES_MAX)
			len = WLAN_SUPP_RATES_MAX;
		os_memcpy(ap->supported_rates, elems->supp_rates, len);
	}
	if (elems->ext_supp_rates) {
		int len2;
		if (len + elems->ext_supp_rates_len > WLAN_SUPP_RATES_MAX)
			len2 = WLAN_SUPP_RATES_MAX - len;
		else
			len2 = elems->ext_supp_rates_len;
		os_memcpy(ap->supported_rates + len, elems->ext_supp_rates,
			  len2);
	}

	ap->wpa = elems->wpa_ie != NULL;

	if (elems->erp_info && elems->erp_info_len == 1)
		ap->erp = elems->erp_info[0];
	else
		ap->erp = -1;

	if (elems->ds_params && elems->ds_params_len == 1)
		ap->channel = elems->ds_params[0];
	else if (fi)
		ap->channel = fi->channel;

	if (elems->ht_capabilities)
		ap->ht_support = 1;
	else
		ap->ht_support = 0;

	ap->num_beacons++;
	time(&ap->last_beacon);
	if (fi) {
		ap->phytype = fi->phytype;
		ap->ssi_signal = fi->ssi_signal;
		ap->datarate = fi->datarate;
	}

	if (new_ap) {
		if (iface->conf->passive_scan_interval > 0)
			ap_list_new_ap(iface, ap);
	} else if (ap != iface->ap_list) {
		/* move AP entry into the beginning of the list so that the
		 * oldest entry is always in the end of the list */
		ap_ap_list_del(iface, ap);
		ap_ap_list_add(iface, ap);
	}

	if (!iface->olbc &&
	    ap_list_beacon_olbc(iface, ap)) {
		iface->olbc = 1;
		wpa_printf(MSG_DEBUG, "OLBC AP detected: " MACSTR " - enable "
			   "protection", MAC2STR(ap->addr));
		set_beacon++;
	}

#ifdef CONFIG_IEEE80211N
	if (!iface->olbc_ht && ap_list_beacon_olbc_ht(iface, ap)) {
		iface->olbc_ht = 1;
		hostapd_ht_operation_update(iface);
		wpa_printf(MSG_DEBUG, "OLBC HT AP detected: " MACSTR
			   " - enable protection", MAC2STR(ap->addr));
		set_beacon++;
	}
#endif /* CONFIG_IEEE80211N */

	if (set_beacon)
		ieee802_11_set_beacons(iface);
}


static void ap_list_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	time_t now;
	struct ap_info *ap;
	int set_beacon = 0;

	eloop_register_timeout(10, 0, ap_list_timer, iface, NULL);

	if (!iface->ap_list)
		return;

	time(&now);

	/* FIX: it looks like jkm-Purina ended up in busy loop in this
	 * function. Apparently, something can still cause a loop in the AP
	 * list.. */

	while (iface->ap_list) {
		ap = iface->ap_list->prev;
		if (ap->last_beacon + iface->conf->ap_table_expiration_time >=
		    now)
			break;

		if (iface->conf->passive_scan_interval > 0)
			ap_list_expired_ap(iface, ap);
		ap_free_ap(iface, ap);
	}

	if (iface->olbc || iface->olbc_ht) {
		int olbc = 0;
		int olbc_ht = 0;

		ap = iface->ap_list;
		while (ap && (olbc == 0 || olbc_ht == 0)) {
			if (ap_list_beacon_olbc(iface, ap))
				olbc = 1;
#ifdef CONFIG_IEEE80211N
			if (ap_list_beacon_olbc_ht(iface, ap))
				olbc_ht = 1;
#endif /* CONFIG_IEEE80211N */
			ap = ap->next;
		}
		if (!olbc && iface->olbc) {
			wpa_printf(MSG_DEBUG, "OLBC not detected anymore");
			iface->olbc = 0;
			set_beacon++;
		}
#ifdef CONFIG_IEEE80211N
		if (!olbc_ht && iface->olbc_ht) {
			wpa_printf(MSG_DEBUG, "OLBC HT not detected anymore");
			iface->olbc_ht = 0;
			hostapd_ht_operation_update(iface);
			set_beacon++;
		}
#endif /* CONFIG_IEEE80211N */
	}

	if (set_beacon)
		ieee802_11_set_beacons(iface);
}


int ap_list_init(struct hostapd_iface *iface)
{
	eloop_register_timeout(10, 0, ap_list_timer, iface, NULL);
	return 0;
}


void ap_list_deinit(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(ap_list_timer, iface, NULL);
	hostapd_free_aps(iface);
}


int ap_list_reconfig(struct hostapd_iface *iface,
		     struct hostapd_config *oldconf)
{
	time_t now;
	struct ap_info *ap;

	if (iface->conf->ap_table_max_size == oldconf->ap_table_max_size &&
	    iface->conf->ap_table_expiration_time ==
	    oldconf->ap_table_expiration_time)
		return 0;

	time(&now);

	while (iface->ap_list) {
		ap = iface->ap_list->prev;
		if (iface->num_ap <= iface->conf->ap_table_max_size &&
		    ap->last_beacon + iface->conf->ap_table_expiration_time >=
		    now)
			break;

		if (iface->conf->passive_scan_interval > 0)
			ap_list_expired_ap(iface, iface->ap_list->prev);
		ap_free_ap(iface, iface->ap_list->prev);
	}

	return 0;
}
