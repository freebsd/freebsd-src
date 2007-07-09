/*
 * hostapd / Configuration reloading
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
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
#include "beacon.h"
#include "hw_features.h"
#include "driver.h"
#include "sta_info.h"
#include "radius_client.h"
#include "ieee802_11.h"
#include "iapp.h"
#include "ap_list.h"
#include "wpa.h"
#include "vlan_init.h"
#include "ieee802_11_auth.h"
#include "ieee802_1x.h"
#include "accounting.h"
#include "eloop.h"


/**
 * struct hostapd_config_change - Configuration change information
 * This is for two purposes:
 * - Storing configuration information in the hostapd_iface during
 *   the asynchronous parts of reconfiguration.
 * - Passing configuration information for per-station reconfiguration.
 */
struct hostapd_config_change {
	struct hostapd_data *hapd;
	struct hostapd_config *newconf, *oldconf;
	struct hostapd_bss_config *newbss, *oldbss;
	int mac_acl_changed;
	int num_sta_remove; /* number of STAs that need to be removed */
	int beacon_changed;
	struct hostapd_iface *hapd_iface;
	struct hostapd_data **new_hapd, **old_hapd;
	int num_old_hapd;
};


static int hostapd_config_reload_sta(struct hostapd_data *hapd,
				     struct sta_info *sta, void *data)
{
	struct hostapd_config_change *change = data;
	struct hostapd_bss_config *newbss, *oldbss;
	int deauth = 0;
	u8 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;

	newbss = change->newbss;
	oldbss = change->oldbss;
	hapd = change->hapd;

	if (sta->ssid == &oldbss->ssid) {
		sta->ssid = &newbss->ssid;

		if (newbss->ssid.ssid_len != oldbss->ssid.ssid_len ||
		    memcmp(newbss->ssid.ssid, oldbss->ssid.ssid,
			   newbss->ssid.ssid_len) != 0) {
			/* main SSID was changed - kick STA out */
			deauth++;
		}
	}
	sta->ssid_probe = sta->ssid;

	/*
	 * If MAC ACL configuration has changed, deauthenticate stations that
	 * have been removed from accepted list or have been added to denied
	 * list. If external RADIUS server is used for ACL, all stations are
	 * deauthenticated and they will need to authenticate again. This
	 * limits sudden load on the RADIUS server since the verification will
	 * be done over the time needed for the STAs to reauthenticate
	 * themselves.
	 */
	if (change->mac_acl_changed &&
	    (newbss->macaddr_acl == USE_EXTERNAL_RADIUS_AUTH ||
	     !hostapd_allowed_address(hapd, sta->addr, NULL, 0, NULL, NULL,
				      NULL)))
		deauth++;

	if (newbss->ieee802_1x != oldbss->ieee802_1x &&
	    sta->ssid == &hapd->conf->ssid)
		deauth++;

	if (newbss->wpa != oldbss->wpa)
		deauth++;

	if (!newbss->wme_enabled && (sta->flags & WLAN_STA_WME))
		deauth++;

	if (newbss->auth_algs != oldbss->auth_algs &&
	    ((sta->auth_alg == WLAN_AUTH_OPEN &&
	      !(newbss->auth_algs & HOSTAPD_AUTH_OPEN)) ||
	     (sta->auth_alg == WLAN_AUTH_SHARED_KEY &&
	      !(newbss->auth_algs & HOSTAPD_AUTH_SHARED_KEY))))
		deauth++;

	if (change->num_sta_remove > 0) {
		deauth++;
		reason = WLAN_REASON_DISASSOC_AP_BUSY;
	}

	if (deauth) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "STA " MACSTR
			      " deauthenticated during config reloading "
			      "(reason=%d)\n", MAC2STR(sta->addr), reason);
		ieee802_11_send_deauth(hapd, sta->addr, reason);
		ap_sta_deauthenticate(hapd, sta, reason);
		change->num_sta_remove--;
	}

	return 0;
}


static void hostapd_reconfig_tx_queue_params(struct hostapd_data *hapd,
					     struct hostapd_config *newconf,
					     struct hostapd_config *oldconf)
{
	int i;
	struct hostapd_tx_queue_params *o, *n;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		o = &oldconf->tx_queue[i];
		n = &newconf->tx_queue[i];

		if (!n->configured)
			continue;

		if ((n->aifs != o->aifs || n->cwmin != o->cwmin ||
		     n->cwmax != o->cwmax || n->burst != o->burst) &&
		    hostapd_set_tx_queue_params(hapd, i, n->aifs, n->cwmin,
						n->cwmax, n->burst))
			printf("Failed to set TX queue parameters for queue %d"
			       ".\n", i);
	}
}


static int hostapd_reconfig_wme(struct hostapd_data *hapd,
				struct hostapd_config *newconf,
				struct hostapd_config *oldconf)
{
	int beacon_changed = 0;
	size_t i;
	struct hostapd_wme_ac_params *o, *n;

	for (i = 0; i < sizeof(newconf->wme_ac_params) /
			sizeof(newconf->wme_ac_params[0]); i++) {
		o = &oldconf->wme_ac_params[i];
		n = &newconf->wme_ac_params[i];
		if (n->cwmin != o->cwmin ||
		    n->cwmax != o->cwmax ||
		    n->aifs != o->aifs ||
		    n->txopLimit != o->txopLimit ||
		    n->admission_control_mandatory !=
		    o->admission_control_mandatory) {
			beacon_changed++;
			hapd->parameter_set_count++;
		}
	}

	return beacon_changed;
}


static int rate_array_diff(int *a1, int *a2)
{
	int i;

	if (a1 == NULL && a2 == NULL)
		return 0;
	if (a1 == NULL || a2 == NULL)
		return 1;

	i = 0;
	for (;;) {
		if (a1[i] != a2[i])
			return 1;
		if (a1[i] == -1)
			break;
		i++;
	}

	return 0;
}


static int hostapd_acl_diff(struct hostapd_bss_config *a,
			    struct hostapd_bss_config *b)
{
	int i;

	if (a->macaddr_acl != b->macaddr_acl ||
	    a->num_accept_mac != b->num_accept_mac ||
	    a->num_deny_mac != b->num_deny_mac)
		return 1;

	for (i = 0; i < a->num_accept_mac; i++) {
		if (memcmp(a->accept_mac[i], b->accept_mac[i], ETH_ALEN) != 0)
			return 1;
	}

	for (i = 0; i < a->num_deny_mac; i++) {
		if (memcmp(a->deny_mac[i], b->deny_mac[i], ETH_ALEN) != 0)
			return 1;
	}

	return 0;
}


/**
 * reload_iface2 - Part 2 of reload_iface
 * @hapd_iface: Pointer to hostapd interface data.
 */
static void reload_iface2(struct hostapd_iface *hapd_iface)
{
	struct hostapd_data *hapd = hapd_iface->bss[0];
	struct hostapd_config *newconf = hapd_iface->change->newconf;
	struct hostapd_config *oldconf = hapd_iface->change->oldconf;
	int beacon_changed = hapd_iface->change->beacon_changed;
	hostapd_iface_cb cb = hapd_iface->reload_iface_cb;

	if (newconf->preamble != oldconf->preamble) {
		if (hostapd_set_preamble(hapd, hapd->iconf->preamble))
			printf("Could not set preamble for kernel driver\n");
		beacon_changed++;
	}

	if (newconf->beacon_int != oldconf->beacon_int) {
		/* Need to change beacon interval if it has changed or if
		 * auto channel selection was used. */
		if (hostapd_set_beacon_int(hapd, newconf->beacon_int))
			printf("Could not set beacon interval for kernel "
			       "driver\n");
		if (newconf->beacon_int != oldconf->beacon_int)
			beacon_changed++;
	}

	if (newconf->cts_protection_type != oldconf->cts_protection_type)
		beacon_changed++;

	if (newconf->rts_threshold > -1 &&
	    newconf->rts_threshold != oldconf->rts_threshold &&
	    hostapd_set_rts(hapd, newconf->rts_threshold))
		printf("Could not set RTS threshold for kernel driver\n");

	if (newconf->fragm_threshold > -1 &&
	    newconf->fragm_threshold != oldconf->fragm_threshold &&
	    hostapd_set_frag(hapd, newconf->fragm_threshold))
		printf("Could not set fragmentation threshold for kernel "
		       "driver\n");

	hostapd_reconfig_tx_queue_params(hapd, newconf, oldconf);

	if (hostapd_reconfig_wme(hapd, newconf, oldconf) > 0)
		beacon_changed++;

	ap_list_reconfig(hapd_iface, oldconf);

	hapd_iface->change->beacon_changed = beacon_changed;

	hapd_iface->reload_iface_cb = NULL;
	cb(hapd_iface, 0);
}


/**
 * reload_iface2_handler - Handler that calls reload_face2
 * @eloop_data: Stores the struct hostapd_iface for the interface.
 * @user_ctx: Unused.
 */
static void reload_iface2_handler(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *hapd_iface = eloop_data;

	reload_iface2(hapd_iface);
}


/**
 * reload_hw_mode_done - Callback for after the HW mode is setup
 * @hapd_iface: Pointer to interface data.
 * @status: Status of the HW mode setup.
 */
static void reload_hw_mode_done(struct hostapd_iface *hapd_iface, int status)
{
	struct hostapd_data *hapd = hapd_iface->bss[0];
	struct hostapd_config_change *change = hapd_iface->change;
	struct hostapd_config *newconf = change->newconf;
	hostapd_iface_cb cb;
	int freq;

	if (status) {
		printf("Failed to select hw_mode.\n");

		cb = hapd_iface->reload_iface_cb;
		hapd_iface->reload_iface_cb = NULL;
		cb(hapd_iface, -1);

		return;
	}

	freq = hostapd_hw_get_freq(hapd, newconf->channel);
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Mode: %s  Channel: %d  Frequency: %d MHz\n",
		      hostapd_hw_mode_txt(newconf->hw_mode),
		      newconf->channel, freq);

	if (hostapd_set_freq(hapd, newconf->hw_mode, freq)) {
		printf("Could not set channel %d (%d MHz) for kernel "
		       "driver\n", newconf->channel, freq);
	}

	change->beacon_changed++;

	reload_iface2(hapd_iface);
}


/**
 * hostapd_config_reload_iface_start - Start interface reload
 * @hapd_iface: Pointer to interface data.
 * @cb: The function to callback when done.
 * Returns:  0 if it starts successfully; cb will be called when done.
 *          -1 on failure; cb will not be called.
 */
static int hostapd_config_reload_iface_start(struct hostapd_iface *hapd_iface,
					     hostapd_iface_cb cb)
{
	struct hostapd_config_change *change = hapd_iface->change;
	struct hostapd_config *newconf = change->newconf;
	struct hostapd_config *oldconf = change->oldconf;
	struct hostapd_data *hapd = hapd_iface->bss[0];

	if (hapd_iface->reload_iface_cb) {
		wpa_printf(MSG_DEBUG,
			   "%s: Interface reload already in progress.",
			   hapd_iface->bss[0]->conf->iface);
		return -1;
	}

	hapd_iface->reload_iface_cb = cb;

	if (newconf->bridge_packets != oldconf->bridge_packets &&
	    hapd->iconf->bridge_packets != INTERNAL_BRIDGE_DO_NOT_CONTROL &&
	    hostapd_set_internal_bridge(hapd, hapd->iconf->bridge_packets))
		printf("Failed to set bridge_packets for kernel driver\n");

	if (newconf->channel != oldconf->channel ||
	    newconf->hw_mode != oldconf->hw_mode ||
	    rate_array_diff(newconf->supported_rates,
			    oldconf->supported_rates) ||
	    rate_array_diff(newconf->basic_rates, oldconf->basic_rates)) {
		hostapd_free_stas(hapd);

		if (hostapd_get_hw_features(hapd_iface)) {
			printf("Could not read HW feature info from the kernel"
			       " driver.\n");
			hapd_iface->reload_iface_cb = NULL;
			return -1;
		}

		if (hostapd_select_hw_mode_start(hapd_iface,
						 reload_hw_mode_done)) {
			printf("Failed to start select hw_mode.\n");
			hapd_iface->reload_iface_cb = NULL;
			return -1;
		}

		return 0;
	}

	eloop_register_timeout(0, 0, reload_iface2_handler, hapd_iface, NULL);
	return 0;
}


static void hostapd_reconfig_bss(struct hostapd_data *hapd,
				 struct hostapd_bss_config *newbss,
				 struct hostapd_bss_config *oldbss,
				 struct hostapd_config *oldconf,
				 int beacon_changed)
{
	struct hostapd_config_change change;
	int encr_changed = 0;
	struct radius_client_data *old_radius;

	radius_client_flush(hapd->radius, 0);

	if (hostapd_set_dtim_period(hapd, newbss->dtim_period))
		printf("Could not set DTIM period for kernel driver\n");

	if (newbss->ssid.ssid_len != oldbss->ssid.ssid_len ||
	    memcmp(newbss->ssid.ssid, oldbss->ssid.ssid,
		   newbss->ssid.ssid_len) != 0) {
		if (hostapd_set_ssid(hapd, (u8 *) newbss->ssid.ssid,
				     newbss->ssid.ssid_len))
			printf("Could not set SSID for kernel driver\n");
		beacon_changed++;
	}

	if (newbss->ignore_broadcast_ssid != oldbss->ignore_broadcast_ssid)
		beacon_changed++;

	if (hostapd_wep_key_cmp(&newbss->ssid.wep, &oldbss->ssid.wep)) {
		encr_changed++;
		beacon_changed++;
	}

	vlan_reconfig(hapd, oldconf, oldbss);

	if (beacon_changed) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Updating beacon frame "
			      "information\n");
		ieee802_11_set_beacon(hapd);
	}

	change.hapd = hapd;
	change.oldconf = oldconf;
	change.newconf = hapd->iconf;
	change.oldbss = oldbss;
	change.newbss = newbss;
	change.mac_acl_changed = hostapd_acl_diff(newbss, oldbss);
	if (newbss->max_num_sta != oldbss->max_num_sta &&
	    newbss->max_num_sta < hapd->num_sta) {
		change.num_sta_remove = hapd->num_sta - newbss->max_num_sta;
	} else
		change.num_sta_remove = 0;
	ap_for_each_sta(hapd, hostapd_config_reload_sta, &change);

	old_radius = hapd->radius;
	hapd->radius = radius_client_reconfig(hapd->radius, hapd,
					      oldbss->radius, newbss->radius);
	hapd->radius_client_reconfigured = old_radius != hapd->radius ||
		hostapd_ip_diff(&newbss->own_ip_addr, &oldbss->own_ip_addr);

	ieee802_1x_reconfig(hapd, oldconf, oldbss);
	iapp_reconfig(hapd, oldconf, oldbss);

	hostapd_acl_reconfig(hapd, oldconf);
	accounting_reconfig(hapd, oldconf);
}


/**
 * config_reload2 - Part 2 of configuration reloading
 * @hapd_iface:
 */
static void config_reload2(struct hostapd_iface *hapd_iface, int status)
{
	struct hostapd_config_change *change = hapd_iface->change;
	struct hostapd_data *hapd = change->hapd;
	struct hostapd_config *newconf = change->newconf;
	struct hostapd_config *oldconf = change->oldconf;
	int beacon_changed = change->beacon_changed;
	struct hostapd_data **new_hapd = change->new_hapd;
	struct hostapd_data **old_hapd = change->old_hapd;
	int num_old_hapd = change->num_old_hapd;
	size_t i, j, max_bss, same_bssid;
	struct hostapd_bss_config *newbss, *oldbss;
	u8 *prev_addr;
	hostapd_iface_cb cb;

	free(change);
	hapd_iface->change = NULL;

	if (status) {
		printf("Failed to setup new interface config\n");

		cb = hapd_iface->config_reload_cb;
		hapd_iface->config_reload_cb = NULL;

		/* Invalid configuration - cleanup and terminate hostapd */
		hapd_iface->bss = old_hapd;
		hapd_iface->num_bss = num_old_hapd;
		hapd_iface->conf = hapd->iconf = oldconf;
		hapd->conf = &oldconf->bss[0];
		hostapd_config_free(newconf);
		free(new_hapd);

		cb(hapd_iface, -2);

		return;
	}

	/*
	 * If any BSSes have been removed, added, or had their BSSIDs changed,
	 * completely remove and reinitialize such BSSes and all the BSSes
	 * following them since their BSSID might have changed.
	 */
	max_bss = oldconf->num_bss;
	if (max_bss > newconf->num_bss)
		max_bss = newconf->num_bss;

	for (i = 0; i < max_bss; i++) {
		if (strcmp(oldconf->bss[i].iface, newconf->bss[i].iface) != 0
		    || hostapd_mac_comp(oldconf->bss[i].bssid,
					newconf->bss[i].bssid) != 0)
			break;
	}
	same_bssid = i;

	for (i = 0; i < oldconf->num_bss; i++) {
		oldbss = &oldconf->bss[i];
		newbss = NULL;
		for (j = 0; j < newconf->num_bss; j++) {
			if (strcmp(oldbss->iface, newconf->bss[j].iface) == 0)
			{
				newbss = &newconf->bss[j];
				break;
			}
		}

		if (newbss && i < same_bssid) {
			hapd = hapd_iface->bss[j] = old_hapd[i];
			hapd->iconf = newconf;
			hapd->conf = newbss;
			hostapd_reconfig_bss(hapd, newbss, oldbss, oldconf,
					     beacon_changed);
		} else {
			hapd = old_hapd[i];
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "Removing BSS (ifname %s)\n",
				      hapd->conf->iface);
			hostapd_free_stas(hapd);
			/* Send broadcast deauthentication for this BSS, but do
			 * not clear all STAs from the driver since other BSSes
			 * may have STA entries. The driver will remove all STA
			 * entries for this BSS anyway when the interface is
			 * being removed. */
#if 0
			hostapd_deauth_all_stas(hapd);
			hostapd_cleanup(hapd);
#endif

			free(hapd);
		}
	}


	prev_addr = hapd_iface->bss[0]->own_addr;
	hapd = hapd_iface->bss[0];
	for (j = 0; j < newconf->num_bss; j++) {
		if (hapd_iface->bss[j] != NULL) {
			prev_addr = hapd_iface->bss[j]->own_addr;
			continue;
		}

		newbss = &newconf->bss[j];

		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Reconfiguration: adding "
			      "new BSS (ifname=%s)\n", newbss->iface);

#if 0
		hapd = hapd_iface->bss[j] =
			hostapd_alloc_bss_data(hapd_iface, newconf, newbss);
#endif
		if (hapd == NULL) {
			printf("Failed to initialize new BSS\n");
			/* FIX: This one is somewhat hard to recover
			 * from.. Would need to remove this BSS from
			 * conf and BSS list. */
			exit(1);
		}
		hapd->driver = hapd_iface->bss[0]->driver;
		hapd->iface = hapd_iface;
		hapd->iconf = newconf;
		hapd->conf = newbss;

		memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0)
			prev_addr = hapd->own_addr;

#if 0
		if (hostapd_setup_bss(hapd, j == 0)) {
			printf("Failed to setup new BSS\n");
			/* FIX */
			exit(1);
		}
#endif

	}

	free(old_hapd);
	hostapd_config_free(oldconf);

	cb = hapd_iface->config_reload_cb;
	hapd_iface->config_reload_cb = NULL;

	cb(hapd_iface, 0);
}


/**
 * hostapd_config_reload_start - Start reconfiguration of an interface
 * @hapd_iface: Pointer to hostapd interface data
 * @cb: Function to be called back when done.
 *      The status indicates:
 *       0 = success, new configuration in use;
 *      -1 = failed to update configuraiton, old configuration in use;
 *      -2 = failed to update configuration and failed to recover; caller
 *           should cleanup and terminate hostapd
 * Returns:
 *  0 = reconfiguration started;
 * -1 = failed to update configuration, old configuration in use;
 * -2 = failed to update configuration and failed to recover; caller
 *      should cleanup and terminate hostapd
 */
int hostapd_config_reload_start(struct hostapd_iface *hapd_iface,
				hostapd_iface_cb cb)
{
	struct hostapd_config *newconf, *oldconf;
	struct hostapd_config_change *change;
	struct hostapd_data *hapd = NULL;
	struct hostapd_data **old_hapd, **new_hapd;
	int num_old_hapd;

	if (hapd_iface->config_reload_cb) {
		wpa_printf(MSG_DEBUG, "%s: Config reload already in progress.",
			   hapd_iface->bss[0]->conf->iface);
		return -1;
	}

	newconf = hostapd_config_read(hapd_iface->config_fname);
	if (newconf == NULL) {
		printf("Failed to read new configuration file - continuing "
		       "with old.\n");
		return -1;
	}

	if (strcmp(newconf->bss[0].iface, hapd_iface->conf->bss[0].iface) !=
	    0) {
		printf("Interface name changing is not allowed in "
		       "configuration reloading (%s -> %s).\n",
		       hapd_iface->conf->bss[0].iface,  newconf->bss[0].iface);
		hostapd_config_free(newconf);
		return -1;
	}

	new_hapd = wpa_zalloc(newconf->num_bss *
			      sizeof(struct hostapd_data *));
	if (new_hapd == NULL) {
		hostapd_config_free(newconf);
		return -1;
	}
	old_hapd = hapd_iface->bss;
	num_old_hapd = hapd_iface->num_bss;

	hapd_iface->bss = new_hapd;
	hapd_iface->num_bss = newconf->num_bss;
	/*
	 * First BSS remains the same since interface name changing was
	 * prohibited above. Now, this is only used in
	 * hostapd_config_reload_iface() and following loop will anyway set
	 * this again.
	 */
	hapd = hapd_iface->bss[0] = old_hapd[0];

	oldconf = hapd_iface->conf;
	hapd->iconf = hapd_iface->conf = newconf;
	hapd->conf = &newconf->bss[0];

	change = wpa_zalloc(sizeof(struct hostapd_config_change));
	if (change == NULL) {
		hostapd_config_free(newconf);
		return -1;
	}

	change->hapd = hapd;
	change->newconf = newconf;
	change->oldconf = oldconf;
	change->beacon_changed = 0;
	change->hapd_iface = hapd_iface;
	change->new_hapd = new_hapd;
	change->old_hapd = old_hapd;
	change->num_old_hapd = num_old_hapd;

	hapd_iface->config_reload_cb = cb;
	hapd_iface->change = change;
	if (hostapd_config_reload_iface_start(hapd_iface, config_reload2)) {
		printf("Failed to start setup of new interface config\n");

		hapd_iface->config_reload_cb = NULL;
		free(change);
		hapd_iface->change = NULL;

		/* Invalid configuration - cleanup and terminate hostapd */
		hapd_iface->bss = old_hapd;
		hapd_iface->num_bss = num_old_hapd;
		hapd_iface->conf = hapd->iconf = oldconf;
		hapd->conf = &oldconf->bss[0];
		hostapd_config_free(newconf);
		free(new_hapd);
		return -2;
	}

	return 0;
}
