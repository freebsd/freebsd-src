/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2021, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#ifdef CONFIG_SQLITE
#include <sqlite3.h>
#endif /* CONFIG_SQLITE */

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/crc32.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "common/hw_features_common.h"
#include "radius/radius_client.h"
#include "radius/radius_das.h"
#include "eap_server/tncs.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "fst/fst.h"
#include "hostapd.h"
#include "authsrv.h"
#include "sta_info.h"
#include "accounting.h"
#include "ap_list.h"
#include "beacon.h"
#include "ieee802_1x.h"
#include "ieee802_11_auth.h"
#include "vlan_init.h"
#include "wpa_auth.h"
#include "wps_hostapd.h"
#include "dpp_hostapd.h"
#include "nan_usd_ap.h"
#include "gas_query_ap.h"
#include "hw_features.h"
#include "wpa_auth_glue.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "p2p_hostapd.h"
#include "gas_serv.h"
#include "dfs.h"
#include "ieee802_11.h"
#include "bss_load.h"
#include "x_snoop.h"
#include "dhcp_snoop.h"
#include "ndisc_snoop.h"
#include "neighbor_db.h"
#include "rrm.h"
#include "fils_hlp.h"
#include "acs.h"
#include "hs20.h"
#include "airtime_policy.h"
#include "wpa_auth_kay.h"
#include "hw_features.h"


static int hostapd_flush_old_stations(struct hostapd_data *hapd, u16 reason);
#ifdef CONFIG_WEP
static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd);
static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd);
#endif /* CONFIG_WEP */
static int setup_interface2(struct hostapd_iface *iface);
static void channel_list_update_timeout(void *eloop_ctx, void *timeout_ctx);
static void hostapd_interface_setup_failure_handler(void *eloop_ctx,
						    void *timeout_ctx);
#ifdef CONFIG_IEEE80211AX
static void hostapd_switch_color_timeout_handler(void *eloop_data,
						 void *user_ctx);
#endif /* CONFIG_IEEE80211AX */


int hostapd_for_each_interface(struct hapd_interfaces *interfaces,
			       int (*cb)(struct hostapd_iface *iface,
					 void *ctx), void *ctx)
{
	size_t i;
	int ret;

	for (i = 0; i < interfaces->count; i++) {
		if (!interfaces->iface[i])
			continue;
		ret = cb(interfaces->iface[i], ctx);
		if (ret)
			return ret;
	}

	return 0;
}


struct hostapd_data * hostapd_mbssid_get_tx_bss(struct hostapd_data *hapd)
{
	if (hapd->iconf->mbssid)
		return hapd->iface->bss[0];

	return hapd;
}


int hostapd_mbssid_get_bss_index(struct hostapd_data *hapd)
{
	if (hapd->iconf->mbssid) {
		size_t i;

		for (i = 1; i < hapd->iface->num_bss; i++)
			if (hapd->iface->bss[i] == hapd)
				return i;
	}

	return 0;
}


void hostapd_reconfig_encryption(struct hostapd_data *hapd)
{
	if (hapd->wpa_auth)
		return;

	hostapd_set_privacy(hapd, 0);
#ifdef CONFIG_WEP
	hostapd_setup_encryption(hapd->conf->iface, hapd);
#endif /* CONFIG_WEP */
}


static void hostapd_reload_bss(struct hostapd_data *hapd)
{
	struct hostapd_ssid *ssid;

	if (!hapd->started)
		return;

	if (hapd->conf->wmm_enabled < 0)
		hapd->conf->wmm_enabled = hapd->iconf->ieee80211n |
			hapd->iconf->ieee80211ax;

#ifndef CONFIG_NO_RADIUS
	radius_client_reconfig(hapd->radius, hapd->conf->radius);
#endif /* CONFIG_NO_RADIUS */

	ssid = &hapd->conf->ssid;
	if (!ssid->wpa_psk_set && ssid->wpa_psk && !ssid->wpa_psk->next &&
	    ssid->wpa_passphrase_set && ssid->wpa_passphrase) {
		/*
		 * Force PSK to be derived again since SSID or passphrase may
		 * have changed.
		 */
		hostapd_config_clear_wpa_psk(&hapd->conf->ssid.wpa_psk);
	}
	if (hostapd_setup_wpa_psk(hapd->conf)) {
		wpa_printf(MSG_ERROR, "Failed to re-configure WPA PSK "
			   "after reloading configuration");
	}

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 1);
	else
		hostapd_set_drv_ieee8021x(hapd, hapd->conf->iface, 0);

	if ((hapd->conf->wpa || hapd->conf->osen) && hapd->wpa_auth == NULL) {
		hostapd_setup_wpa(hapd);
		if (hapd->wpa_auth)
			wpa_init_keys(hapd->wpa_auth);
	} else if (hapd->conf->wpa) {
		const u8 *wpa_ie;
		size_t wpa_ie_len;
		hostapd_reconfig_wpa(hapd);
		wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
		if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len))
			wpa_printf(MSG_ERROR, "Failed to configure WPA IE for "
				   "the kernel driver.");
	} else if (hapd->wpa_auth) {
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;
		hostapd_set_privacy(hapd, 0);
#ifdef CONFIG_WEP
		hostapd_setup_encryption(hapd->conf->iface, hapd);
#endif /* CONFIG_WEP */
		hostapd_set_generic_elem(hapd, (u8 *) "", 0);
	}

	hostapd_neighbor_sync_own_report(hapd);

	ieee802_11_set_beacon(hapd);
	hostapd_update_wps(hapd);

	if (hapd->conf->ssid.ssid_set &&
	    hostapd_set_ssid(hapd, hapd->conf->ssid.ssid,
			     hapd->conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		/* try to continue */
	}
	wpa_printf(MSG_DEBUG, "Reconfigured interface %s", hapd->conf->iface);
}


static void hostapd_clear_old_bss(struct hostapd_data *bss)
{
	wpa_printf(MSG_DEBUG, "BSS %s changed - clear old state",
		   bss->conf->iface);

	/*
	 * Deauthenticate all stations since the new configuration may not
	 * allow them to use the BSS anymore.
	 */
	hostapd_flush_old_stations(bss, WLAN_REASON_PREV_AUTH_NOT_VALID);
#ifdef CONFIG_WEP
	hostapd_broadcast_wep_clear(bss);
#endif /* CONFIG_WEP */

#ifndef CONFIG_NO_RADIUS
	/* TODO: update dynamic data based on changed configuration
	 * items (e.g., open/close sockets, etc.) */
	radius_client_flush(bss->radius, 0);
#endif /* CONFIG_NO_RADIUS */
}


static void hostapd_clear_old(struct hostapd_iface *iface)
{
	size_t j;

	for (j = 0; j < iface->num_bss; j++)
		hostapd_clear_old_bss(iface->bss[j]);
}


static int hostapd_iface_conf_changed(struct hostapd_config *newconf,
				      struct hostapd_config *oldconf)
{
	size_t i;

	if (newconf->num_bss != oldconf->num_bss)
		return 1;

	for (i = 0; i < newconf->num_bss; i++) {
		if (os_strcmp(newconf->bss[i]->iface,
			      oldconf->bss[i]->iface) != 0)
			return 1;
	}

	return 0;
}


int hostapd_reload_config(struct hostapd_iface *iface)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_config *newconf, *oldconf;
	size_t j;

	if (iface->config_fname == NULL) {
		/* Only in-memory config in use - assume it has been updated */
		hostapd_clear_old(iface);
		for (j = 0; j < iface->num_bss; j++)
			hostapd_reload_bss(iface->bss[j]);
		return 0;
	}

	if (iface->interfaces == NULL ||
	    iface->interfaces->config_read_cb == NULL)
		return -1;
	newconf = iface->interfaces->config_read_cb(iface->config_fname);
	if (newconf == NULL)
		return -1;

	oldconf = hapd->iconf;
	if (hostapd_iface_conf_changed(newconf, oldconf)) {
		char *fname;
		int res;

		hostapd_clear_old(iface);

		wpa_printf(MSG_DEBUG,
			   "Configuration changes include interface/BSS modification - force full disable+enable sequence");
		fname = os_strdup(iface->config_fname);
		if (!fname) {
			hostapd_config_free(newconf);
			return -1;
		}
		hostapd_remove_iface(interfaces, hapd->conf->iface);
		iface = hostapd_init(interfaces, fname);
		os_free(fname);
		hostapd_config_free(newconf);
		if (!iface) {
			wpa_printf(MSG_ERROR,
				   "Failed to initialize interface on config reload");
			return -1;
		}
		iface->interfaces = interfaces;
		interfaces->iface[interfaces->count] = iface;
		interfaces->count++;
		res = hostapd_enable_iface(iface);
		if (res < 0)
			wpa_printf(MSG_ERROR,
				   "Failed to enable interface on config reload");
		return res;
	}
	iface->conf = newconf;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (!hapd->conf->config_id || !newconf->bss[j]->config_id ||
		    os_strcmp(hapd->conf->config_id,
			      newconf->bss[j]->config_id) != 0)
			hostapd_clear_old_bss(hapd);
		hapd->iconf = newconf;
		hapd->iconf->channel = oldconf->channel;
		hapd->iconf->acs = oldconf->acs;
		hapd->iconf->secondary_channel = oldconf->secondary_channel;
		hapd->iconf->ieee80211n = oldconf->ieee80211n;
		hapd->iconf->ieee80211ac = oldconf->ieee80211ac;
		hapd->iconf->ht_capab = oldconf->ht_capab;
		hapd->iconf->vht_capab = oldconf->vht_capab;
		hostapd_set_oper_chwidth(hapd->iconf,
					 hostapd_get_oper_chwidth(oldconf));
		hostapd_set_oper_centr_freq_seg0_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg0_idx(oldconf));
		hostapd_set_oper_centr_freq_seg1_idx(
			hapd->iconf,
			hostapd_get_oper_centr_freq_seg1_idx(oldconf));
		hapd->conf = newconf->bss[j];
		hostapd_reload_bss(hapd);
	}

	hostapd_config_free(oldconf);


	return 0;
}


#ifdef CONFIG_WEP

static void hostapd_broadcast_key_clear_iface(struct hostapd_data *hapd,
					      const char *ifname)
{
	int i;

	if (!ifname || !hapd->drv_priv)
		return;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE, NULL, i, 0,
					0, NULL, 0, NULL, 0, KEY_FLAG_GROUP)) {
			wpa_printf(MSG_DEBUG, "Failed to clear default "
				   "encryption keys (ifname=%s keyidx=%d)",
				   ifname, i);
		}
	}
	if (hapd->conf->ieee80211w) {
		for (i = NUM_WEP_KEYS; i < NUM_WEP_KEYS + 2; i++) {
			if (hostapd_drv_set_key(ifname, hapd, WPA_ALG_NONE,
						NULL, i, 0, 0, NULL,
						0, NULL, 0, KEY_FLAG_GROUP)) {
				wpa_printf(MSG_DEBUG, "Failed to clear "
					   "default mgmt encryption keys "
					   "(ifname=%s keyidx=%d)", ifname, i);
			}
		}
	}
}


static int hostapd_broadcast_wep_clear(struct hostapd_data *hapd)
{
	hostapd_broadcast_key_clear_iface(hapd, hapd->conf->iface);
	return 0;
}


static int hostapd_broadcast_wep_set(struct hostapd_data *hapd)
{
	int errors = 0, idx;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	idx = ssid->wep.idx;
	if (ssid->wep.default_len && ssid->wep.key[idx] &&
	    hostapd_drv_set_key(hapd->conf->iface,
				hapd, WPA_ALG_WEP, broadcast_ether_addr, idx, 0,
				1, NULL, 0, ssid->wep.key[idx],
				ssid->wep.len[idx],
				KEY_FLAG_GROUP_RX_TX_DEFAULT)) {
		wpa_printf(MSG_WARNING, "Could not set WEP encryption.");
		errors++;
	}

	return errors;
}

#endif /* CONFIG_WEP */


#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_TESTING_OPTIONS

#define TU_TO_USEC(_val) ((_val) * 1024)

static void hostapd_link_remove_timeout_handler(void *eloop_data,
						void *user_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_data;

	if (hapd->eht_mld_link_removal_count == 0)
		return;
	hapd->eht_mld_link_removal_count--;

	wpa_printf(MSG_DEBUG, "MLD: Remove link_id=%u in %u beacons",
		   hapd->mld_link_id,
		   hapd->eht_mld_link_removal_count);

	ieee802_11_set_beacon(hapd);

	if (!hapd->eht_mld_link_removal_count) {
		hostapd_free_link_stas(hapd);
		hostapd_disable_iface(hapd->iface);
		return;
	}

	eloop_register_timeout(0, TU_TO_USEC(hapd->iconf->beacon_int),
			       hostapd_link_remove_timeout_handler,
			       hapd, NULL);
}


int hostapd_link_remove(struct hostapd_data *hapd, u32 count)
{
	if (!hapd->conf->mld_ap)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "MLD: Remove link_id=%u in %u beacons",
		   hapd->mld_link_id, count);

	hapd->eht_mld_link_removal_count = count;
	hapd->eht_mld_bss_param_change++;

	eloop_register_timeout(0, TU_TO_USEC(hapd->iconf->beacon_int),
			       hostapd_link_remove_timeout_handler,
			       hapd, NULL);

	ieee802_11_set_beacon(hapd);
	return 0;
}

#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */


void hostapd_free_hapd_data(struct hostapd_data *hapd)
{
	os_free(hapd->probereq_cb);
	hapd->probereq_cb = NULL;
	hapd->num_probereq_cb = 0;

#ifdef CONFIG_P2P
	wpabuf_free(hapd->p2p_beacon_ie);
	hapd->p2p_beacon_ie = NULL;
	wpabuf_free(hapd->p2p_probe_resp_ie);
	hapd->p2p_probe_resp_ie = NULL;
#endif /* CONFIG_P2P */

	if (!hapd->started) {
		wpa_printf(MSG_ERROR, "%s: Interface %s wasn't started",
			   __func__, hapd->conf ? hapd->conf->iface : "N/A");
		return;
	}
	hapd->started = 0;
	hapd->beacon_set_done = 0;

	wpa_printf(MSG_DEBUG, "%s(%s)", __func__, hapd->conf->iface);
	accounting_deinit(hapd);
	hostapd_deinit_wpa(hapd);
	vlan_deinit(hapd);
	hostapd_acl_deinit(hapd);
#ifndef CONFIG_NO_RADIUS
	if (hostapd_mld_is_first_bss(hapd)) {
#ifdef CONFIG_IEEE80211BE
		struct hapd_interfaces *ifaces = hapd->iface->interfaces;
		size_t i;

		for (i = 0; i < ifaces->count; i++) {
			struct hostapd_iface *iface = ifaces->iface[i];
			size_t j;

			for (j = 0; iface && j < iface->num_bss; j++) {
				struct hostapd_data *h = iface->bss[j];

				if (hapd == h)
					continue;
				if (h->radius == hapd->radius)
					h->radius = NULL;
				if (h->radius_das == hapd->radius_das)
					h->radius_das = NULL;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		radius_client_deinit(hapd->radius);
		radius_das_deinit(hapd->radius_das);
	}
	hapd->radius = NULL;
	hapd->radius_das = NULL;
#endif /* CONFIG_NO_RADIUS */

	hostapd_deinit_wps(hapd);
	ieee802_1x_dealloc_kay_sm_hapd(hapd);
#ifdef CONFIG_DPP
	hostapd_dpp_deinit(hapd);
	gas_query_ap_deinit(hapd->gas);
	hapd->gas = NULL;
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	hostapd_nan_usd_deinit(hapd);
#endif /* CONFIG_NAN_USD */

	authsrv_deinit(hapd);

	if (hapd->interface_added) {
		hapd->interface_added = 0;
		if (hostapd_if_remove(hapd, WPA_IF_AP_BSS, hapd->conf->iface)) {
			wpa_printf(MSG_WARNING,
				   "Failed to remove BSS interface %s",
				   hapd->conf->iface);
			hapd->interface_added = 1;
		} else {
			/*
			 * Since this was a dynamically added interface, the
			 * driver wrapper may have removed its internal instance
			 * and hapd->drv_priv is not valid anymore.
			 */
			hapd->drv_priv = NULL;
		}
	}

#ifdef CONFIG_IEEE80211BE
	/* If the interface was not added as well as it is not the first BSS,
	 * at least the link should be removed here since deinit will take care
	 * of only the first BSS. */
	if (hapd->conf->mld_ap && !hapd->interface_added &&
	    hapd->iface->bss[0] != hapd)
		hostapd_if_link_remove(hapd, WPA_IF_AP_BSS, hapd->conf->iface,
				       hapd->mld_link_id);
#endif /* CONFIG_IEEE80211BE */

	wpabuf_free(hapd->time_adv);
	hapd->time_adv = NULL;

#ifdef CONFIG_INTERWORKING
	gas_serv_deinit(hapd);
#endif /* CONFIG_INTERWORKING */

	bss_load_update_deinit(hapd);
	ndisc_snoop_deinit(hapd);
	dhcp_snoop_deinit(hapd);
	x_snoop_deinit(hapd);

#ifdef CONFIG_SQLITE
	bin_clear_free(hapd->tmp_eap_user.identity,
		       hapd->tmp_eap_user.identity_len);
	bin_clear_free(hapd->tmp_eap_user.password,
		       hapd->tmp_eap_user.password_len);
	os_memset(&hapd->tmp_eap_user, 0, sizeof(hapd->tmp_eap_user));
#endif /* CONFIG_SQLITE */

#ifdef CONFIG_MESH
	wpabuf_free(hapd->mesh_pending_auth);
	hapd->mesh_pending_auth = NULL;
	/* handling setup failure is already done */
	hapd->setup_complete_cb = NULL;
#endif /* CONFIG_MESH */

#ifndef CONFIG_NO_RRM
	hostapd_clean_rrm(hapd);
#endif /* CONFIG_NO_RRM */
	fils_hlp_deinit(hapd);

#ifdef CONFIG_OCV
	eloop_cancel_timeout(hostapd_ocv_check_csa_sa_query, hapd, NULL);
#endif /* CONFIG_OCV */

#ifdef CONFIG_SAE
	{
		struct hostapd_sae_commit_queue *q;

		while ((q = dl_list_first(&hapd->sae_commit_queue,
					  struct hostapd_sae_commit_queue,
					  list))) {
			dl_list_del(&q->list);
			os_free(q);
		}
	}
	eloop_cancel_timeout(auth_sae_process_commit, hapd, NULL);
#endif /* CONFIG_SAE */

#ifdef CONFIG_IEEE80211AX
	eloop_cancel_timeout(hostapd_switch_color_timeout_handler, hapd, NULL);
#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_IEEE80211BE
	eloop_cancel_timeout(hostapd_link_remove_timeout_handler, hapd, NULL);
#endif /* CONFIG_IEEE80211BE */
#endif /* CONFIG_TESTING_OPTIONS */

#endif /* CONFIG_IEEE80211AX */
}


/* hostapd_bss_link_deinit - Per-BSS ML cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to unlink the BSS from the AP MLD.
 * If the BSS being removed is the first link, the next link becomes the first
 * link.
 */
static void hostapd_bss_link_deinit(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (!hapd->conf || !hapd->conf->mld_ap)
		return;

	if (!hapd->mld->num_links)
		return;

	/* If not started, not yet linked to the MLD. However, the first
	 * BSS is always linked since it is linked during driver_init(), and
	 * hence, need to remove it from the AP MLD.
	 */
	if (!hapd->started && hapd->iface->bss[0] != hapd)
		return;

	/* The first BSS can also be only linked when at least driver_init() is
	 * executed. But if previous interface fails, it is not, and hence,
	 * safe to skip.
	 */
	if (hapd->iface->bss[0] == hapd && !hapd->drv_priv)
		return;

	hostapd_mld_remove_link(hapd);
#endif /* CONFIG_IEEE80211BE */
}


/**
 * hostapd_cleanup - Per-BSS cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to free all per-BSS data structures and resources.
 * Most of the modules that are initialized in hostapd_setup_bss() are
 * deinitialized here.
 */
static void hostapd_cleanup(struct hostapd_data *hapd)
{
	wpa_printf(MSG_DEBUG, "%s(hapd=%p (%s))", __func__, hapd,
		   hapd->conf ? hapd->conf->iface : "N/A");
	if (hapd->iface->interfaces &&
	    hapd->iface->interfaces->ctrl_iface_deinit) {
		wpa_msg(hapd->msg_ctx, MSG_INFO, WPA_EVENT_TERMINATING);
		hapd->iface->interfaces->ctrl_iface_deinit(hapd);
	}
	hostapd_free_hapd_data(hapd);
}


static void sta_track_deinit(struct hostapd_iface *iface)
{
	struct hostapd_sta_info *info;

	if (!iface->num_sta_seen)
		return;

	while ((info = dl_list_first(&iface->sta_seen, struct hostapd_sta_info,
				     list))) {
		dl_list_del(&info->list);
		iface->num_sta_seen--;
		sta_track_del(info);
	}
}


void hostapd_cleanup_iface_partial(struct hostapd_iface *iface)
{
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);
#ifdef NEED_AP_MLME
	hostapd_stop_setup_timers(iface);
#endif /* NEED_AP_MLME */
	if (iface->current_mode)
		acs_cleanup(iface);
	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = NULL;
	iface->current_mode = NULL;
	os_free(iface->current_rates);
	iface->current_rates = NULL;
	os_free(iface->basic_rates);
	iface->basic_rates = NULL;
	iface->cac_started = 0;
	ap_list_deinit(iface);
	sta_track_deinit(iface);
	airtime_policy_update_deinit(iface);
}


/**
 * hostapd_cleanup_iface - Complete per-interface cleanup
 * @iface: Pointer to interface data
 *
 * This function is called after per-BSS data structures are deinitialized
 * with hostapd_cleanup().
 */
static void hostapd_cleanup_iface(struct hostapd_iface *iface)
{
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	eloop_cancel_timeout(hostapd_interface_setup_failure_handler, iface,
			     NULL);

	hostapd_cleanup_iface_partial(iface);
	hostapd_config_free(iface->conf);
	iface->conf = NULL;

	os_free(iface->config_fname);
	os_free(iface->bss);
	wpa_printf(MSG_DEBUG, "%s: free iface=%p", __func__, iface);
	os_free(iface);
}


#ifdef CONFIG_WEP

static void hostapd_clear_wep(struct hostapd_data *hapd)
{
	if (hapd->drv_priv && !hapd->iface->driver_ap_teardown && hapd->conf) {
		hostapd_set_privacy(hapd, 0);
		hostapd_broadcast_wep_clear(hapd);
	}
}


static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd)
{
	int i;

	hostapd_broadcast_wep_set(hapd);

	if (hapd->conf->ssid.wep.default_len) {
		hostapd_set_privacy(hapd, 1);
		return 0;
	}

	/*
	 * When IEEE 802.1X is not enabled, the driver may need to know how to
	 * set authentication algorithms for static WEP.
	 */
	hostapd_drv_set_authmode(hapd, hapd->conf->auth_algs);

	for (i = 0; i < 4; i++) {
		if (hapd->conf->ssid.wep.key[i] &&
		    hostapd_drv_set_key(iface, hapd, WPA_ALG_WEP, NULL, i, 0,
					i == hapd->conf->ssid.wep.idx, NULL, 0,
					hapd->conf->ssid.wep.key[i],
					hapd->conf->ssid.wep.len[i],
					i == hapd->conf->ssid.wep.idx ?
					KEY_FLAG_GROUP_RX_TX_DEFAULT :
					KEY_FLAG_GROUP_RX_TX)) {
			wpa_printf(MSG_WARNING, "Could not set WEP "
				   "encryption.");
			return -1;
		}
		if (hapd->conf->ssid.wep.key[i] &&
		    i == hapd->conf->ssid.wep.idx)
			hostapd_set_privacy(hapd, 1);
	}

	return 0;
}

#endif /* CONFIG_WEP */


static int hostapd_flush_old_stations(struct hostapd_data *hapd, u16 reason)
{
	int ret = 0;
	u8 addr[ETH_ALEN];

	if (hostapd_drv_none(hapd) || hapd->drv_priv == NULL)
		return 0;

	if (!hapd->iface->driver_ap_teardown) {
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Flushing old station entries");

		if (hostapd_flush(hapd)) {
			wpa_msg(hapd->msg_ctx, MSG_WARNING,
				"Could not connect to kernel driver");
			ret = -1;
		}
	}
	if (hapd->conf && hapd->conf->broadcast_deauth) {
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Deauthenticate all stations");
		os_memset(addr, 0xff, ETH_ALEN);
		hostapd_drv_sta_deauth(hapd, addr, reason);
	}
	hostapd_free_stas(hapd);

	return ret;
}


void hostapd_bss_deinit_no_free(struct hostapd_data *hapd)
{
	hostapd_free_stas(hapd);
	hostapd_flush_old_stations(hapd, WLAN_REASON_DEAUTH_LEAVING);
#ifdef CONFIG_WEP
	hostapd_clear_wep(hapd);
#endif /* CONFIG_WEP */
}


/**
 * hostapd_validate_bssid_configuration - Validate BSSID configuration
 * @iface: Pointer to interface data
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to validate that the configured BSSIDs are valid.
 */
static int hostapd_validate_bssid_configuration(struct hostapd_iface *iface)
{
	u8 mask[ETH_ALEN] = { 0 };
	struct hostapd_data *hapd = iface->bss[0];
	unsigned int i = iface->conf->num_bss, bits = 0, j;
	int auto_addr = 0;

	if (hostapd_drv_none(hapd))
		return 0;

	if (iface->conf->use_driver_iface_addr)
		return 0;

	/* Generate BSSID mask that is large enough to cover the BSSIDs. */

	/* Determine the bits necessary to cover the number of BSSIDs. */
	for (i--; i; i >>= 1)
		bits++;

	/* Determine the bits necessary to any configured BSSIDs,
	   if they are higher than the number of BSSIDs. */
	for (j = 0; j < iface->conf->num_bss; j++) {
		if (is_zero_ether_addr(iface->conf->bss[j]->bssid)) {
			if (j)
				auto_addr++;
			continue;
		}

		for (i = 0; i < ETH_ALEN; i++) {
			mask[i] |=
				iface->conf->bss[j]->bssid[i] ^
				hapd->own_addr[i];
		}
	}

	if (!auto_addr)
		goto skip_mask_ext;

	for (i = 0; i < ETH_ALEN && mask[i] == 0; i++)
		;
	j = 0;
	if (i < ETH_ALEN) {
		j = (5 - i) * 8;

		while (mask[i] != 0) {
			mask[i] >>= 1;
			j++;
		}
	}

	if (bits < j)
		bits = j;

	if (bits > 40) {
		wpa_printf(MSG_ERROR, "Too many bits in the BSSID mask (%u)",
			   bits);
		return -1;
	}

	os_memset(mask, 0xff, ETH_ALEN);
	j = bits / 8;
	for (i = 5; i > 5 - j; i--)
		mask[i] = 0;
	j = bits % 8;
	while (j) {
		j--;
		mask[i] <<= 1;
	}

skip_mask_ext:
	wpa_printf(MSG_DEBUG, "BSS count %lu, BSSID mask " MACSTR " (%d bits)",
		   (unsigned long) iface->conf->num_bss, MAC2STR(mask), bits);

	if (!auto_addr)
		return 0;

	for (i = 0; i < ETH_ALEN; i++) {
		if ((hapd->own_addr[i] & mask[i]) != hapd->own_addr[i]) {
			wpa_printf(MSG_ERROR, "Invalid BSSID mask " MACSTR
				   " for start address " MACSTR ".",
				   MAC2STR(mask), MAC2STR(hapd->own_addr));
			wpa_printf(MSG_ERROR, "Start address must be the "
				   "first address in the block (i.e., addr "
				   "AND mask == addr).");
			return -1;
		}
	}

	return 0;
}


static int mac_in_conf(struct hostapd_config *conf, const void *a)
{
	size_t i;

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_mac_comp(conf->bss[i]->bssid, a) == 0) {
			return 1;
		}
	}

	return 0;
}


#ifndef CONFIG_NO_RADIUS

static int hostapd_das_nas_mismatch(struct hostapd_data *hapd,
				    struct radius_das_attrs *attr)
{
	if (attr->nas_identifier &&
	    (!hapd->conf->nas_identifier ||
	     os_strlen(hapd->conf->nas_identifier) !=
	     attr->nas_identifier_len ||
	     os_memcmp(hapd->conf->nas_identifier, attr->nas_identifier,
		       attr->nas_identifier_len) != 0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-Identifier mismatch");
		return 1;
	}

	if (attr->nas_ip_addr &&
	    (hapd->conf->own_ip_addr.af != AF_INET ||
	     os_memcmp(&hapd->conf->own_ip_addr.u.v4, attr->nas_ip_addr, 4) !=
	     0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-IP-Address mismatch");
		return 1;
	}

#ifdef CONFIG_IPV6
	if (attr->nas_ipv6_addr &&
	    (hapd->conf->own_ip_addr.af != AF_INET6 ||
	     os_memcmp(&hapd->conf->own_ip_addr.u.v6, attr->nas_ipv6_addr, 16)
	     != 0)) {
		wpa_printf(MSG_DEBUG, "RADIUS DAS: NAS-IPv6-Address mismatch");
		return 1;
	}
#endif /* CONFIG_IPV6 */

	return 0;
}


static struct sta_info * hostapd_das_find_sta(struct hostapd_data *hapd,
					      struct radius_das_attrs *attr,
					      int *multi)
{
	struct sta_info *selected, *sta;
	char buf[128];
	int num_attr = 0;
	int count;

	*multi = 0;

	for (sta = hapd->sta_list; sta; sta = sta->next)
		sta->radius_das_match = 1;

	if (attr->sta_addr) {
		num_attr++;
		sta = ap_get_sta(hapd, attr->sta_addr);
		if (!sta) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No Calling-Station-Id match");
			return NULL;
		}

		selected = sta;
		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (sta != selected)
				sta->radius_das_match = 0;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: Calling-Station-Id match");
	}

	if (attr->acct_session_id) {
		num_attr++;
		if (attr->acct_session_id_len != 16) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Acct-Session-Id cannot match");
			return NULL;
		}
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (!sta->radius_das_match)
				continue;
			os_snprintf(buf, sizeof(buf), "%016llX",
				    (unsigned long long) sta->acct_session_id);
			if (os_memcmp(attr->acct_session_id, buf, 16) != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Acct-Session-Id check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: Acct-Session-Id match");
	}

	if (attr->acct_multi_session_id) {
		num_attr++;
		if (attr->acct_multi_session_id_len != 16) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Acct-Multi-Session-Id cannot match");
			return NULL;
		}
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (!sta->radius_das_match)
				continue;
			if (!sta->eapol_sm ||
			    !sta->eapol_sm->acct_multi_session_id) {
				sta->radius_das_match = 0;
				continue;
			}
			os_snprintf(buf, sizeof(buf), "%016llX",
				    (unsigned long long)
				    sta->eapol_sm->acct_multi_session_id);
			if (os_memcmp(attr->acct_multi_session_id, buf, 16) !=
			    0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Acct-Multi-Session-Id check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: Acct-Multi-Session-Id match");
	}

	if (attr->cui) {
		num_attr++;
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			struct wpabuf *cui;

			if (!sta->radius_das_match)
				continue;
			cui = ieee802_1x_get_radius_cui(sta->eapol_sm);
			if (!cui || wpabuf_len(cui) != attr->cui_len ||
			    os_memcmp(wpabuf_head(cui), attr->cui,
				      attr->cui_len) != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after Chargeable-User-Identity check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: Chargeable-User-Identity match");
	}

	if (attr->user_name) {
		num_attr++;
		count = 0;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			u8 *identity;
			size_t identity_len;

			if (!sta->radius_das_match)
				continue;
			identity = ieee802_1x_get_identity(sta->eapol_sm,
							   &identity_len);
			if (!identity ||
			    identity_len != attr->user_name_len ||
			    os_memcmp(identity, attr->user_name, identity_len)
			    != 0)
				sta->radius_das_match = 0;
			else
				count++;
		}

		if (count == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: No matches remaining after User-Name check");
			return NULL;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: User-Name match");
	}

	if (num_attr == 0) {
		/*
		 * In theory, we could match all current associations, but it
		 * seems safer to just reject requests that do not include any
		 * session identification attributes.
		 */
		wpa_printf(MSG_DEBUG,
			   "RADIUS DAS: No session identification attributes included");
		return NULL;
	}

	selected = NULL;
	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->radius_das_match) {
			if (selected) {
				*multi = 1;
				return NULL;
			}
			selected = sta;
		}
	}

	return selected;
}


static int hostapd_das_disconnect_pmksa(struct hostapd_data *hapd,
					struct radius_das_attrs *attr)
{
	if (!hapd->wpa_auth)
		return -1;
	return wpa_auth_radius_das_disconnect_pmksa(hapd->wpa_auth, attr);
}


static enum radius_das_res
hostapd_das_disconnect(void *ctx, struct radius_das_attrs *attr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int multi;

	if (hostapd_das_nas_mismatch(hapd, attr))
		return RADIUS_DAS_NAS_MISMATCH;

	sta = hostapd_das_find_sta(hapd, attr, &multi);
	if (sta == NULL) {
		if (multi) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Multiple sessions match - not supported");
			return RADIUS_DAS_MULTI_SESSION_MATCH;
		}
		if (hostapd_das_disconnect_pmksa(hapd, attr) == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: PMKSA cache entry matched");
			return RADIUS_DAS_SUCCESS;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: No matching session found");
		return RADIUS_DAS_SESSION_NOT_FOUND;
	}

	wpa_printf(MSG_DEBUG, "RADIUS DAS: Found a matching session " MACSTR
		   " - disconnecting", MAC2STR(sta->addr));
	wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);

	hostapd_drv_sta_deauth(hapd, sta->addr,
			       WLAN_REASON_PREV_AUTH_NOT_VALID);
	ap_sta_deauthenticate(hapd, sta, WLAN_REASON_PREV_AUTH_NOT_VALID);

	return RADIUS_DAS_SUCCESS;
}


#ifdef CONFIG_HS20
static enum radius_das_res
hostapd_das_coa(void *ctx, struct radius_das_attrs *attr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int multi;

	if (hostapd_das_nas_mismatch(hapd, attr))
		return RADIUS_DAS_NAS_MISMATCH;

	sta = hostapd_das_find_sta(hapd, attr, &multi);
	if (!sta) {
		if (multi) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS DAS: Multiple sessions match - not supported");
			return RADIUS_DAS_MULTI_SESSION_MATCH;
		}
		wpa_printf(MSG_DEBUG, "RADIUS DAS: No matching session found");
		return RADIUS_DAS_SESSION_NOT_FOUND;
	}

	wpa_printf(MSG_DEBUG, "RADIUS DAS: Found a matching session " MACSTR
		   " - CoA", MAC2STR(sta->addr));

	if (attr->hs20_t_c_filtering) {
		if (attr->hs20_t_c_filtering[0] & BIT(0)) {
			wpa_printf(MSG_DEBUG,
				   "HS 2.0: Unexpected Terms and Conditions filtering required in CoA-Request");
			return RADIUS_DAS_COA_FAILED;
		}

		hs20_t_c_filtering(hapd, sta, 0);
	}

	return RADIUS_DAS_SUCCESS;
}
#else /* CONFIG_HS20 */
#define hostapd_das_coa NULL
#endif /* CONFIG_HS20 */


#ifdef CONFIG_SQLITE

static int db_table_exists(sqlite3 *db, const char *name)
{
	char cmd[128];

	os_snprintf(cmd, sizeof(cmd), "SELECT 1 FROM %s;", name);
	return sqlite3_exec(db, cmd, NULL, NULL, NULL) == SQLITE_OK;
}


static int db_table_create_radius_attributes(sqlite3 *db)
{
	char *err = NULL;
	const char *sql =
		"CREATE TABLE radius_attributes("
		" id INTEGER PRIMARY KEY,"
		" sta TEXT,"
		" reqtype TEXT,"
		" attr TEXT"
		");"
		"CREATE INDEX idx_sta_reqtype ON radius_attributes(sta,reqtype);";

	wpa_printf(MSG_DEBUG,
		   "Adding database table for RADIUS attribute information");
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		wpa_printf(MSG_ERROR, "SQLite error: %s", err);
		sqlite3_free(err);
		return -1;
	}

	return 0;
}

#endif /* CONFIG_SQLITE */

#endif /* CONFIG_NO_RADIUS */


static int hostapd_start_beacon(struct hostapd_data *hapd,
				bool flush_old_stations)
{
	struct hostapd_bss_config *conf = hapd->conf;

	if (!conf->start_disabled && ieee802_11_set_beacon(hapd) < 0)
		return -1;

	if (flush_old_stations && !conf->start_disabled &&
	    conf->broadcast_deauth) {
		u8 addr[ETH_ALEN];

		/* Should any previously associated STA not have noticed that
		 * the AP had stopped and restarted, send one more
		 * deauthentication notification now that the AP is ready to
		 * operate. */
		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Deauthenticate all stations at BSS start");
		os_memset(addr, 0xff, ETH_ALEN);
		hostapd_drv_sta_deauth(hapd, addr,
				       WLAN_REASON_PREV_AUTH_NOT_VALID);
	}

	if (hapd->driver && hapd->driver->set_operstate)
		hapd->driver->set_operstate(hapd->drv_priv, 1);

	return 0;
}


#ifndef CONFIG_NO_RADIUS
static int hostapd_bss_radius_init(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf;

	if (!hapd)
		return -1;

	conf = hapd->conf;

	if (hapd->radius) {
		wpa_printf(MSG_DEBUG,
			   "Skipping RADIUS client init (already done)");
		return 0;
	}

	hapd->radius = radius_client_init(hapd, conf->radius);
	if (!hapd->radius) {
		wpa_printf(MSG_ERROR,
			   "RADIUS client initialization failed.");
		return -1;
	}

	if (conf->radius_das_port) {
		struct radius_das_conf das_conf;

		os_memset(&das_conf, 0, sizeof(das_conf));
		das_conf.port = conf->radius_das_port;
		das_conf.shared_secret = conf->radius_das_shared_secret;
		das_conf.shared_secret_len =
			conf->radius_das_shared_secret_len;
		das_conf.client_addr = &conf->radius_das_client_addr;
		das_conf.time_window = conf->radius_das_time_window;
		das_conf.require_event_timestamp =
			conf->radius_das_require_event_timestamp;
		das_conf.require_message_authenticator =
			conf->radius_das_require_message_authenticator;
		das_conf.ctx = hapd;
		das_conf.disconnect = hostapd_das_disconnect;
		das_conf.coa = hostapd_das_coa;
		hapd->radius_das = radius_das_init(&das_conf);
		if (!hapd->radius_das) {
			wpa_printf(MSG_ERROR,
				   "RADIUS DAS initialization failed.");
			return -1;
		}
	}

	return 0;
}
#endif /* CONFIG_NO_RADIUS */


/**
 * hostapd_setup_bss - Per-BSS setup (initialization)
 * @hapd: Pointer to BSS data
 * @first: Whether this BSS is the first BSS of an interface; -1 = not first,
 *	but interface may exist
 * @start_beacon: Whether Beacon frame template should be configured and
 *	transmission of Beaconf rames started at this time. This is used when
 *	MBSSID element is enabled where the information regarding all BSSes
 *	should be retrieved before configuring the Beacon frame template. The
 *	calling functions are responsible for configuring the Beacon frame
 *	explicitly if this is set to false.
 *
 * This function is used to initialize all per-BSS data structures and
 * resources. This gets called in a loop for each BSS when an interface is
 * initialized. Most of the modules that are initialized here will be
 * deinitialized in hostapd_cleanup().
 */
static int hostapd_setup_bss(struct hostapd_data *hapd, int first,
			     bool start_beacon)
{
	struct hostapd_bss_config *conf = hapd->conf;
	u8 ssid[SSID_MAX_LEN + 1];
	int ssid_len, set_ssid;
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN];
	int flush_old_stations = 1;

	if (!hostapd_mld_is_first_bss(hapd))
		wpa_printf(MSG_DEBUG,
			   "MLD: %s: Setting non-first BSS", __func__);

	wpa_printf(MSG_DEBUG, "%s(hapd=%p (%s), first=%d)",
		   __func__, hapd, conf->iface, first);

#ifdef EAP_SERVER_TNC
	if (conf->tnc && tncs_global_init() < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize TNCS");
		return -1;
	}
#endif /* EAP_SERVER_TNC */

	if (hapd->started) {
		wpa_printf(MSG_ERROR, "%s: Interface %s was already started",
			   __func__, conf->iface);
		return -1;
	}
	hapd->started = 1;

	if (!first || first == -1) {
		u8 *addr = hapd->own_addr;

		if (!is_zero_ether_addr(conf->bssid)) {
			/* Allocate the configured BSSID. */
			os_memcpy(hapd->own_addr, conf->bssid, ETH_ALEN);

			if (hostapd_mac_comp(hapd->own_addr,
					     hapd->iface->bss[0]->own_addr) ==
			    0) {
				wpa_printf(MSG_ERROR, "BSS '%s' may not have "
					   "BSSID set to the MAC address of "
					   "the radio", conf->iface);
				return -1;
			}
		} else if (hapd->iconf->use_driver_iface_addr) {
			addr = NULL;
		} else {
			/* Allocate the next available BSSID. */
			do {
				inc_byte_array(hapd->own_addr, ETH_ALEN);
			} while (mac_in_conf(hapd->iconf, hapd->own_addr));
		}

#ifdef CONFIG_IEEE80211BE
		if (conf->mld_ap) {
			struct hostapd_data *h_hapd;

			h_hapd = hostapd_mld_get_first_bss(hapd);
			if (h_hapd) {
				hapd->drv_priv = h_hapd->drv_priv;
				hapd->interface_added = h_hapd->interface_added;
				hostapd_mld_add_link(hapd);
				wpa_printf(MSG_DEBUG,
					   "Setup of non first link (%d) BSS of MLD %s",
					   hapd->mld_link_id, hapd->conf->iface);
				goto setup_mld;
			}
		}
#endif /* CONFIG_IEEE80211BE */

		hapd->interface_added = 1;
		if (hostapd_if_add(hapd->iface->bss[0], WPA_IF_AP_BSS,
				   conf->iface, addr, hapd,
				   &hapd->drv_priv, force_ifname, if_addr,
				   conf->bridge[0] ? conf->bridge : NULL,
				   first == -1)) {
			wpa_printf(MSG_ERROR, "Failed to add BSS (BSSID="
				   MACSTR ")", MAC2STR(hapd->own_addr));
			hapd->interface_added = 0;
			return -1;
		}

		if (!addr)
			os_memcpy(hapd->own_addr, if_addr, ETH_ALEN);

#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap) {
			wpa_printf(MSG_DEBUG,
				   "Setup of first link (%d) BSS of MLD %s",
				   hapd->mld_link_id, hapd->conf->iface);
			os_memcpy(hapd->mld->mld_addr, hapd->own_addr,
				  ETH_ALEN);
			hostapd_mld_add_link(hapd);
		}
#endif /* CONFIG_IEEE80211BE */
	}

#ifdef CONFIG_IEEE80211BE
setup_mld:
	if (hapd->conf->mld_ap && !first) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Set link_id=%u, mld_addr=" MACSTR
			   ", own_addr=" MACSTR,
			   hapd->mld_link_id, MAC2STR(hapd->mld->mld_addr),
			   MAC2STR(hapd->own_addr));

		if (hostapd_drv_link_add(hapd, hapd->mld_link_id,
					 hapd->own_addr))
			return -1;
	}
#endif /* CONFIG_IEEE80211BE */

	if (conf->wmm_enabled < 0)
		conf->wmm_enabled = hapd->iconf->ieee80211n |
			hapd->iconf->ieee80211ax;

#ifdef CONFIG_IEEE80211R_AP
	if (is_zero_ether_addr(conf->r1_key_holder))
		os_memcpy(conf->r1_key_holder, hapd->own_addr, ETH_ALEN);
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_MESH
	if ((hapd->conf->mesh & MESH_ENABLED) && hapd->iface->mconf == NULL)
		flush_old_stations = 0;
#endif /* CONFIG_MESH */

	if (flush_old_stations)
		hostapd_flush(hapd);
	hostapd_set_privacy(hapd, 0);

#ifdef CONFIG_WEP
	if (!hostapd_drv_nl80211(hapd))
		hostapd_broadcast_wep_clear(hapd);
	if (hostapd_setup_encryption(conf->iface, hapd))
		return -1;
#endif /* CONFIG_WEP */

	/*
	 * Fetch the SSID from the system and use it or,
	 * if one was specified in the config file, verify they
	 * match.
	 */
	ssid_len = hostapd_get_ssid(hapd, ssid, sizeof(ssid));
	if (ssid_len < 0) {
		wpa_printf(MSG_ERROR, "Could not read SSID from system");
		return -1;
	}
	if (conf->ssid.ssid_set) {
		/*
		 * If SSID is specified in the config file and it differs
		 * from what is being used then force installation of the
		 * new SSID.
		 */
		set_ssid = (conf->ssid.ssid_len != (size_t) ssid_len ||
			    os_memcmp(conf->ssid.ssid, ssid, ssid_len) != 0);
	} else {
		/*
		 * No SSID in the config file; just use the one we got
		 * from the system.
		 */
		set_ssid = 0;
		conf->ssid.ssid_len = ssid_len;
		os_memcpy(conf->ssid.ssid, ssid, conf->ssid.ssid_len);
	}

	/*
	 * Short SSID calculation is identical to FCS and it is defined in
	 * IEEE P802.11-REVmd/D3.0, 9.4.2.170.3 (Calculating the Short-SSID).
	 */
	conf->ssid.short_ssid = ieee80211_crc32(conf->ssid.ssid,
						conf->ssid.ssid_len);

	if (!hostapd_drv_none(hapd)) {
		wpa_printf(MSG_DEBUG, "Using interface %s with hwaddr " MACSTR
			   " and ssid \"%s\"",
			   conf->iface, MAC2STR(hapd->own_addr),
			   wpa_ssid_txt(conf->ssid.ssid, conf->ssid.ssid_len));
	}

	if (hostapd_setup_wpa_psk(conf)) {
		wpa_printf(MSG_ERROR, "WPA-PSK setup failed.");
		return -1;
	}

	/* Set SSID for the kernel driver (to be used in beacon and probe
	 * response frames) */
	if (set_ssid && hostapd_set_ssid(hapd, conf->ssid.ssid,
					 conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		return -1;
	}

	if (wpa_debug_level <= MSG_MSGDUMP)
		conf->radius->msg_dumps = 1;
#ifndef CONFIG_NO_RADIUS

#ifdef CONFIG_SQLITE
	if (conf->radius_req_attr_sqlite) {
		if (sqlite3_open(conf->radius_req_attr_sqlite,
				 &hapd->rad_attr_db)) {
			wpa_printf(MSG_ERROR, "Could not open SQLite file '%s'",
				   conf->radius_req_attr_sqlite);
			return -1;
		}

		wpa_printf(MSG_DEBUG, "Opening RADIUS attribute database: %s",
			   conf->radius_req_attr_sqlite);
		if (!db_table_exists(hapd->rad_attr_db, "radius_attributes") &&
		    db_table_create_radius_attributes(hapd->rad_attr_db) < 0)
			return -1;
	}
#endif /* CONFIG_SQLITE */

	if (hostapd_mld_is_first_bss(hapd)) {
		if (hostapd_bss_radius_init(hapd))
			return -1;
	} else {
#ifdef CONFIG_IEEE80211BE
		struct hostapd_data *f_bss;

		f_bss = hostapd_mld_get_first_bss(hapd);
		if (!f_bss)
			return -1;

		if (!f_bss->radius) {
			wpa_printf(MSG_DEBUG,
				   "MLD: First BSS RADIUS client does not exist. Init on its behalf");

			if (hostapd_bss_radius_init(f_bss))
				return -1;
		}

		wpa_printf(MSG_DEBUG,
			   "MLD: Using RADIUS client of the first BSS");
		hapd->radius = f_bss->radius;
		hapd->radius_das = f_bss->radius_das;
#endif /* CONFIG_IEEE80211BE */
	}
#endif /* CONFIG_NO_RADIUS */

	if (hostapd_acl_init(hapd)) {
		wpa_printf(MSG_ERROR, "ACL initialization failed.");
		return -1;
	}
	if (hostapd_init_wps(hapd, conf))
		return -1;

#ifdef CONFIG_DPP
	hapd->gas = gas_query_ap_init(hapd, hapd->msg_ctx);
	if (!hapd->gas)
		return -1;
	if (hostapd_dpp_init(hapd))
		return -1;
#endif /* CONFIG_DPP */

#ifdef CONFIG_NAN_USD
	if (hostapd_nan_usd_init(hapd) < 0)
		return -1;
#endif /* CONFIG_NAN_USD */

	if (authsrv_init(hapd) < 0)
		return -1;

	if (ieee802_1x_init(hapd)) {
		wpa_printf(MSG_ERROR, "IEEE 802.1X initialization failed.");
		return -1;
	}

	if ((conf->wpa || conf->osen) && hostapd_setup_wpa(hapd))
		return -1;

	if (accounting_init(hapd)) {
		wpa_printf(MSG_ERROR, "Accounting initialization failed.");
		return -1;
	}

#ifdef CONFIG_INTERWORKING
	if (gas_serv_init(hapd)) {
		wpa_printf(MSG_ERROR, "GAS server initialization failed");
		return -1;
	}
#endif /* CONFIG_INTERWORKING */

	if (conf->qos_map_set_len &&
	    hostapd_drv_set_qos_map(hapd, conf->qos_map_set,
				    conf->qos_map_set_len)) {
		wpa_printf(MSG_ERROR, "Failed to initialize QoS Map");
		return -1;
	}

	if (conf->bss_load_update_period && bss_load_update_init(hapd)) {
		wpa_printf(MSG_ERROR, "BSS Load initialization failed");
		return -1;
	}

	if (conf->bridge[0]) {
		/* Set explicitly configured bridge parameters that might have
		 * been lost if the interface has been removed out of the
		 * bridge. */

		/* multicast to unicast on bridge ports */
		if (conf->bridge_multicast_to_unicast)
			hostapd_drv_br_port_set_attr(
				hapd, DRV_BR_PORT_ATTR_MCAST2UCAST, 1);

		/* hairpin mode */
		if (conf->bridge_hairpin)
			hostapd_drv_br_port_set_attr(
				hapd, DRV_BR_PORT_ATTR_HAIRPIN_MODE, 1);
	}

	if (conf->proxy_arp) {
		if (x_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Generic snooping infrastructure initialization failed");
			return -1;
		}

		if (dhcp_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "DHCP snooping initialization failed");
			return -1;
		}

		if (ndisc_snoop_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Neighbor Discovery snooping initialization failed");
			return -1;
		}
	}

	if (!hostapd_drv_none(hapd) && vlan_init(hapd)) {
		wpa_printf(MSG_ERROR, "VLAN initialization failed.");
		return -1;
	}

	if (start_beacon && hostapd_start_beacon(hapd, flush_old_stations) < 0)
		return -1;

	if (hapd->wpa_auth && wpa_init_keys(hapd->wpa_auth) < 0)
		return -1;

	return 0;
}


static void hostapd_tx_queue_params(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i;
	struct hostapd_tx_queue_params *p;

#ifdef CONFIG_MESH
	if ((hapd->conf->mesh & MESH_ENABLED) && iface->mconf == NULL)
		return;
#endif /* CONFIG_MESH */

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		p = &iface->conf->tx_queue[i];

		if (hostapd_set_tx_queue_params(hapd, i, p->aifs, p->cwmin,
						p->cwmax, p->burst)) {
			wpa_printf(MSG_DEBUG, "Failed to set TX queue "
				   "parameters for queue %d.", i);
			/* Continue anyway */
		}
	}
}


static int hostapd_set_acl_list(struct hostapd_data *hapd,
				struct mac_acl_entry *mac_acl,
				int n_entries, u8 accept_acl)
{
	struct hostapd_acl_params *acl_params;
	int i, err;

	acl_params = os_zalloc(sizeof(*acl_params) +
			       (n_entries * sizeof(acl_params->mac_acl[0])));
	if (!acl_params)
		return -ENOMEM;

	for (i = 0; i < n_entries; i++)
		os_memcpy(acl_params->mac_acl[i].addr, mac_acl[i].addr,
			  ETH_ALEN);

	acl_params->acl_policy = accept_acl;
	acl_params->num_mac_acl = n_entries;

	err = hostapd_drv_set_acl(hapd, acl_params);

	os_free(acl_params);

	return err;
}


int hostapd_set_acl(struct hostapd_data *hapd)
{
	struct hostapd_config *conf = hapd->iconf;
	int err = 0;
	u8 accept_acl;

	if (hapd->iface->drv_max_acl_mac_addrs == 0)
		return 0;

	if (conf->bss[0]->macaddr_acl == DENY_UNLESS_ACCEPTED) {
		accept_acl = 1;
		err = hostapd_set_acl_list(hapd, conf->bss[0]->accept_mac,
					   conf->bss[0]->num_accept_mac,
					   accept_acl);
		if (err) {
			wpa_printf(MSG_DEBUG, "Failed to set accept acl");
			return -1;
		}
	} else if (conf->bss[0]->macaddr_acl == ACCEPT_UNLESS_DENIED) {
		accept_acl = 0;
		err = hostapd_set_acl_list(hapd, conf->bss[0]->deny_mac,
					   conf->bss[0]->num_deny_mac,
					   accept_acl);
		if (err) {
			wpa_printf(MSG_DEBUG, "Failed to set deny acl");
			return -1;
		}
	}
	return err;
}


static int start_ctrl_iface_bss(struct hostapd_data *hapd)
{
	if (!hapd->iface->interfaces ||
	    !hapd->iface->interfaces->ctrl_iface_init)
		return 0;

	if (hapd->iface->interfaces->ctrl_iface_init(hapd)) {
		wpa_printf(MSG_ERROR,
			   "Failed to setup control interface for %s",
			   hapd->conf->iface);
		return -1;
	}

	return 0;
}


static int start_ctrl_iface(struct hostapd_iface *iface)
{
	size_t i;

	if (!iface->interfaces || !iface->interfaces->ctrl_iface_init)
		return 0;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];
		if (iface->interfaces->ctrl_iface_init(hapd)) {
			wpa_printf(MSG_ERROR,
				   "Failed to setup control interface for %s",
				   hapd->conf->iface);
			return -1;
		}
	}

	return 0;
}


/* When NO_IR flag is set and AP is stopped, clean up BSS parameters without
 * deinitializing the driver and the control interfaces. A subsequent
 * REG_CHANGE event can bring the AP back up.
 */
static void hostapd_no_ir_cleanup(struct hostapd_data *bss)
{
	hostapd_bss_deinit_no_free(bss);
	hostapd_bss_link_deinit(bss);
	hostapd_free_hapd_data(bss);
	hostapd_cleanup_iface_partial(bss->iface);
}


static int hostapd_no_ir_channel_list_updated(struct hostapd_iface *iface,
					      void *ctx)
{
	bool all_no_ir, is_6ghz;
	int i, j;
	struct hostapd_hw_modes *mode = NULL;

	if (hostapd_get_hw_features(iface))
		return 0;

	all_no_ir = true;
	is_6ghz = false;

	for (i = 0; i < iface->num_hw_features; i++) {
		mode = &iface->hw_features[i];

		if (mode->mode == iface->conf->hw_mode) {
			if (iface->freq > 0 &&
			    !hw_mode_get_channel(mode, iface->freq, NULL)) {
				mode = NULL;
				continue;
			}

			for (j = 0; j < mode->num_channels; j++) {
				if (!(mode->channels[j].flag &
				      HOSTAPD_CHAN_NO_IR))
					all_no_ir = false;

				if (is_6ghz_freq(mode->channels[j].freq))
					is_6ghz = true;
			}
			break;
		}
	}

	if (!mode || !is_6ghz)
		return 0;
	iface->current_mode = mode;

	if (iface->state == HAPD_IFACE_ENABLED) {
		if (!all_no_ir) {
			struct hostapd_channel_data *chan;

			chan = hw_get_channel_freq(iface->current_mode->mode,
						   iface->freq, NULL,
						   iface->hw_features,
						   iface->num_hw_features);

			if (!chan) {
				wpa_printf(MSG_ERROR,
					   "NO_IR: Could not derive chan from freq");
				return 0;
			}

			if (!(chan->flag & HOSTAPD_CHAN_NO_IR))
				return 0;
			wpa_printf(MSG_DEBUG,
				   "NO_IR: The current channel has NO_IR flag now, stop AP.");
		} else {
			wpa_printf(MSG_DEBUG,
				   "NO_IR: All chan in new chanlist are NO_IR, stop AP.");
		}

		hostapd_set_state(iface, HAPD_IFACE_NO_IR);
		iface->is_no_ir = true;
		hostapd_drv_stop_ap(iface->bss[0]);
		hostapd_no_ir_cleanup(iface->bss[0]);
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
	} else if (iface->state == HAPD_IFACE_NO_IR) {
		if (all_no_ir) {
			wpa_printf(MSG_DEBUG,
				   "NO_IR: AP in NO_IR and all chan in the new chanlist are NO_IR. Ignore");
			return 0;
		}

		if (!iface->conf->acs) {
			struct hostapd_channel_data *chan;

			chan = hw_get_channel_freq(iface->current_mode->mode,
						   iface->freq, NULL,
						   iface->hw_features,
						   iface->num_hw_features);
			if (!chan) {
				wpa_printf(MSG_ERROR,
					   "NO_IR: Could not derive chan from freq");
				return 0;
			}

			/* If the last operating channel is NO_IR, trigger ACS.
			 */
			if (chan->flag & HOSTAPD_CHAN_NO_IR) {
				iface->freq = 0;
				iface->conf->channel = 0;
				if (acs_init(iface) != HOSTAPD_CHAN_ACS)
					wpa_printf(MSG_ERROR,
						   "NO_IR: Could not start ACS");
				return 0;
			}
		}

		setup_interface2(iface);
	}

	return 0;
}


static void channel_list_update_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;

	if (!iface->wait_channel_update) {
		wpa_printf(MSG_INFO, "Channel list update timeout, but interface was not waiting for it");
		return;
	}

	/*
	 * It is possible that the existing channel list is acceptable, so try
	 * to proceed.
	 */
	wpa_printf(MSG_DEBUG, "Channel list update timeout - try to continue anyway");
	setup_interface2(iface);
}


void hostapd_channel_list_updated(struct hostapd_iface *iface, int initiator)
{
	if (initiator == REGDOM_SET_BY_DRIVER) {
		hostapd_for_each_interface(iface->interfaces,
					   hostapd_no_ir_channel_list_updated,
					   NULL);
		return;
	}

	if (!iface->wait_channel_update || initiator != REGDOM_SET_BY_USER)
		return;

	wpa_printf(MSG_DEBUG, "Channel list updated - continue setup");
	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);
	setup_interface2(iface);
}


static int setup_interface(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t i;

	/*
	 * It is possible that setup_interface() is called after the interface
	 * was disabled etc., in which case driver_ap_teardown is possibly set
	 * to 1. Clear it here so any other key/station deletion, which is not
	 * part of a teardown flow, would also call the relevant driver
	 * callbacks.
	 */
	iface->driver_ap_teardown = 0;

	if (!iface->phy[0]) {
		const char *phy = hostapd_drv_get_radio_name(hapd);
		if (phy) {
			wpa_printf(MSG_DEBUG, "phy: %s", phy);
			os_strlcpy(iface->phy, phy, sizeof(iface->phy));
		}
	}

	/*
	 * Make sure that all BSSes get configured with a pointer to the same
	 * driver interface.
	 */
	for (i = 1; i < iface->num_bss; i++) {
		iface->bss[i]->driver = hapd->driver;
		iface->bss[i]->drv_priv = hapd->drv_priv;
	}

	if (hostapd_validate_bssid_configuration(iface))
		return -1;

	/*
	 * Initialize control interfaces early to allow external monitoring of
	 * channel setup operations that may take considerable amount of time
	 * especially for DFS cases.
	 */
	if (start_ctrl_iface(iface))
		return -1;

	if (hapd->iconf->country[0] && hapd->iconf->country[1]) {
		char country[4], previous_country[4];

		hostapd_set_state(iface, HAPD_IFACE_COUNTRY_UPDATE);
		if (hostapd_get_country(hapd, previous_country) < 0)
			previous_country[0] = '\0';

		os_memcpy(country, hapd->iconf->country, 3);
		country[3] = '\0';
		if (hostapd_set_country(hapd, country) < 0) {
			wpa_printf(MSG_ERROR, "Failed to set country code");
			return -1;
		}

		wpa_printf(MSG_DEBUG, "Previous country code %s, new country code %s",
			   previous_country, country);

		if (os_strncmp(previous_country, country, 2) != 0) {
			wpa_printf(MSG_DEBUG, "Continue interface setup after channel list update");
			iface->wait_channel_update = 1;
			eloop_register_timeout(5, 0,
					       channel_list_update_timeout,
					       iface, NULL);
			return 0;
		}
	}

	return setup_interface2(iface);
}


static int configured_fixed_chan_to_freq(struct hostapd_iface *iface)
{
	int freq, i, j;

	if (!iface->conf->channel)
		return 0;
	if (iface->conf->op_class) {
		freq = ieee80211_chan_to_freq(NULL, iface->conf->op_class,
					      iface->conf->channel);
		if (freq < 0) {
			wpa_printf(MSG_INFO,
				   "Could not convert op_class %u channel %u to operating frequency",
				   iface->conf->op_class, iface->conf->channel);
			return -1;
		}
		iface->freq = freq;
		return 0;
	}

	/* Old configurations using only 2.4/5/60 GHz bands may not specify the
	 * op_class parameter. Select a matching channel from the configured
	 * mode using the channel parameter for these cases.
	 */
	for (j = 0; j < iface->num_hw_features; j++) {
		struct hostapd_hw_modes *mode = &iface->hw_features[j];

		if (iface->conf->hw_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    iface->conf->hw_mode != mode->mode)
			continue;
		for (i = 0; i < mode->num_channels; i++) {
			struct hostapd_channel_data *chan = &mode->channels[i];

			if (chan->chan == iface->conf->channel &&
			    !is_6ghz_freq(chan->freq)) {
				iface->freq = chan->freq;
				return 0;
			}
		}
	}

	wpa_printf(MSG_INFO, "Could not determine operating frequency");
	return -1;
}


static void hostapd_set_6ghz_sec_chan(struct hostapd_iface *iface)
{
	int bw;

	if (!is_6ghz_op_class(iface->conf->op_class))
		return;

	bw = op_class_to_bandwidth(iface->conf->op_class);
	/* Assign the secondary channel if absent in config for
	 * bandwidths > 20 MHz */
	if (bw >= 40 && !iface->conf->secondary_channel) {
		if (((iface->conf->channel - 1) / 4) % 2)
			iface->conf->secondary_channel = -1;
		else
			iface->conf->secondary_channel = 1;
	}
}


static int setup_interface2(struct hostapd_iface *iface)
{
	iface->wait_channel_update = 0;
	iface->is_no_ir = false;

#ifdef __FreeBSD
	/* XXX hostapd_get_hw_features() is an inline that always returns -1
	 * because MLME will not build under FreeBSD due to its use of
	 * Linux definitions. Normally FreeBSD would uncondionally execute the
	 * "Not all drivers support..." block. Instead we #ifdef out the entire
	 * block of code instead of maintaining the fallacy that
	 * hostapd_get_hw_features() returns anything meaninful.
	 *
	 * Ideally WANT_AP_MLME should be taught about FreeBSD data structures
	 * and defintions. Instead we do this to enable channel selection in
	 * hostapd.conf.
	 */
	iface->freq = iface->conf->channel;
#else
	if (hostapd_get_hw_features(iface)) {
		/* Not all drivers support this yet, so continue without hw
		 * feature data. */
	} else {
		int ret;

		if (iface->conf->acs && !iface->is_ch_switch_dfs) {
			iface->freq = 0;
			iface->conf->channel = 0;
		}
		iface->is_ch_switch_dfs = false;

		ret = configured_fixed_chan_to_freq(iface);
		if (ret < 0)
			goto fail;

		if (iface->conf->op_class) {
			enum oper_chan_width ch_width;

			ch_width = op_class_to_ch_width(iface->conf->op_class);
			hostapd_set_oper_chwidth(iface->conf, ch_width);
			hostapd_set_6ghz_sec_chan(iface);
		}

		ret = hostapd_select_hw_mode(iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Could not select hw_mode and "
				   "channel. (%d)", ret);
			goto fail;
		}
		if (ret == 1) {
			wpa_printf(MSG_DEBUG, "Interface initialization will be completed in a callback (ACS)");
			return 0;
		}
		ret = hostapd_check_edmg_capab(iface);
		if (ret < 0)
			goto fail;
		ret = hostapd_check_he_6ghz_capab(iface);
		if (ret < 0)
			goto fail;
		ret = hostapd_check_ht_capab(iface);
		if (ret < 0)
			goto fail;
		if (ret == 1) {
			wpa_printf(MSG_DEBUG, "Interface initialization will "
				   "be completed in a callback");
			return 0;
		}

		if (iface->conf->ieee80211h)
			wpa_printf(MSG_DEBUG, "DFS support is enabled");
	}
#endif
	return hostapd_setup_interface_complete(iface, 0);

fail:
	if (iface->is_no_ir) {
		/* If AP is in NO_IR state, it can be reenabled by the driver
		 * regulatory update and EVENT_CHANNEL_LIST_CHANGED. */
		hostapd_set_state(iface, HAPD_IFACE_NO_IR);
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
		return 0;
	}

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
	if (iface->interfaces && iface->interfaces->terminate_on_error)
		eloop_terminate();
	return -1;
}


#ifdef CONFIG_FST

static const u8 * fst_hostapd_get_bssid_cb(void *ctx)
{
	struct hostapd_data *hapd = ctx;

	return hapd->own_addr;
}


static void fst_hostapd_get_channel_info_cb(void *ctx,
					    enum hostapd_hw_mode *hw_mode,
					    u8 *channel)
{
	struct hostapd_data *hapd = ctx;

	*hw_mode = ieee80211_freq_to_chan(hapd->iface->freq, channel);
}


static int fst_hostapd_get_hw_modes_cb(void *ctx,
				       struct hostapd_hw_modes **modes)
{
	struct hostapd_data *hapd = ctx;

	*modes = hapd->iface->hw_features;
	return hapd->iface->num_hw_features;
}


static void fst_hostapd_set_ies_cb(void *ctx, const struct wpabuf *fst_ies)
{
	struct hostapd_data *hapd = ctx;

	if (hapd->iface->fst_ies != fst_ies) {
		hapd->iface->fst_ies = fst_ies;
		if (ieee802_11_set_beacon(hapd))
			wpa_printf(MSG_WARNING, "FST: Cannot set beacon");
	}
}


static int fst_hostapd_send_action_cb(void *ctx, const u8 *da,
				      struct wpabuf *buf)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_send_action(hapd, hapd->iface->freq, 0, da,
				       wpabuf_head(buf), wpabuf_len(buf));
}


static const struct wpabuf * fst_hostapd_get_mb_ie_cb(void *ctx, const u8 *addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);

	return sta ? sta->mb_ies : NULL;
}


static void fst_hostapd_update_mb_ie_cb(void *ctx, const u8 *addr,
					const u8 *buf, size_t size)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);

	if (sta) {
		struct mb_ies_info info;

		if (!mb_ies_info_by_ies(&info, buf, size)) {
			wpabuf_free(sta->mb_ies);
			sta->mb_ies = mb_ies_by_info(&info);
		}
	}
}


static const u8 * fst_hostapd_get_sta(struct fst_get_peer_ctx **get_ctx,
				      bool mb_only)
{
	struct sta_info *s = (struct sta_info *) *get_ctx;

	if (mb_only) {
		for (; s && !s->mb_ies; s = s->next)
			;
	}

	if (s) {
		*get_ctx = (struct fst_get_peer_ctx *) s->next;

		return s->addr;
	}

	*get_ctx = NULL;
	return NULL;
}


static const u8 * fst_hostapd_get_peer_first(void *ctx,
					     struct fst_get_peer_ctx **get_ctx,
					     bool mb_only)
{
	struct hostapd_data *hapd = ctx;

	*get_ctx = (struct fst_get_peer_ctx *) hapd->sta_list;

	return fst_hostapd_get_sta(get_ctx, mb_only);
}


static const u8 * fst_hostapd_get_peer_next(void *ctx,
					    struct fst_get_peer_ctx **get_ctx,
					    bool mb_only)
{
	return fst_hostapd_get_sta(get_ctx, mb_only);
}


void fst_hostapd_fill_iface_obj(struct hostapd_data *hapd,
				struct fst_wpa_obj *iface_obj)
{
	os_memset(iface_obj, 0, sizeof(*iface_obj));
	iface_obj->ctx = hapd;
	iface_obj->get_bssid = fst_hostapd_get_bssid_cb;
	iface_obj->get_channel_info = fst_hostapd_get_channel_info_cb;
	iface_obj->get_hw_modes = fst_hostapd_get_hw_modes_cb;
	iface_obj->set_ies = fst_hostapd_set_ies_cb;
	iface_obj->send_action = fst_hostapd_send_action_cb;
	iface_obj->get_mb_ie = fst_hostapd_get_mb_ie_cb;
	iface_obj->update_mb_ie = fst_hostapd_update_mb_ie_cb;
	iface_obj->get_peer_first = fst_hostapd_get_peer_first;
	iface_obj->get_peer_next = fst_hostapd_get_peer_next;
}

#endif /* CONFIG_FST */

#ifdef CONFIG_OWE

static int hostapd_owe_iface_iter(struct hostapd_iface *iface, void *ctx)
{
	struct hostapd_data *hapd = ctx;
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];

		if (os_strcmp(hapd->conf->owe_transition_ifname,
			      bss->conf->iface) != 0)
			continue;

		wpa_printf(MSG_DEBUG,
			   "OWE: ifname=%s found transition mode ifname=%s BSSID "
			   MACSTR " SSID %s",
			   hapd->conf->iface, bss->conf->iface,
			   MAC2STR(bss->own_addr),
			   wpa_ssid_txt(bss->conf->ssid.ssid,
					bss->conf->ssid.ssid_len));
		if (!bss->conf->ssid.ssid_set || !bss->conf->ssid.ssid_len ||
		    is_zero_ether_addr(bss->own_addr))
			continue;

		os_memcpy(hapd->conf->owe_transition_bssid, bss->own_addr,
			  ETH_ALEN);
		os_memcpy(hapd->conf->owe_transition_ssid,
			  bss->conf->ssid.ssid, bss->conf->ssid.ssid_len);
		hapd->conf->owe_transition_ssid_len = bss->conf->ssid.ssid_len;
		wpa_printf(MSG_DEBUG,
			   "OWE: Copied transition mode information");
		return 1;
	}

	return 0;
}


int hostapd_owe_trans_get_info(struct hostapd_data *hapd)
{
	if (hapd->conf->owe_transition_ssid_len > 0 &&
	    !is_zero_ether_addr(hapd->conf->owe_transition_bssid))
		return 0;

	/* Find transition mode SSID/BSSID information from a BSS operated by
	 * this hostapd instance. */
	if (!hapd->iface->interfaces ||
	    !hapd->iface->interfaces->for_each_interface)
		return hostapd_owe_iface_iter(hapd->iface, hapd);
	else
		return hapd->iface->interfaces->for_each_interface(
			hapd->iface->interfaces, hostapd_owe_iface_iter, hapd);
}


static int hostapd_owe_iface_iter2(struct hostapd_iface *iface, void *ctx)
{
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		int res;

		if (!bss->conf->owe_transition_ifname[0])
			continue;
		if (bss->iface->state != HAPD_IFACE_ENABLED) {
			wpa_printf(MSG_DEBUG,
				   "OWE: Interface %s state %s - defer beacon update",
				   bss->conf->iface,
				   hostapd_state_text(bss->iface->state));
			continue;
		}
		res = hostapd_owe_trans_get_info(bss);
		if (res == 0)
			continue;
		wpa_printf(MSG_DEBUG,
			   "OWE: Matching transition mode interface enabled - update beacon data for %s",
			   bss->conf->iface);
		ieee802_11_set_beacon(bss);
	}

	return 0;
}

#endif /* CONFIG_OWE */


static void hostapd_owe_update_trans(struct hostapd_iface *iface)
{
#ifdef CONFIG_OWE
	/* Check whether the enabled BSS can complete OWE transition mode
	 * configuration for any pending interface. */
	if (!iface->interfaces ||
	    !iface->interfaces->for_each_interface)
		hostapd_owe_iface_iter2(iface, NULL);
	else
		iface->interfaces->for_each_interface(
			iface->interfaces, hostapd_owe_iface_iter2, NULL);
#endif /* CONFIG_OWE */
}


static void hostapd_interface_setup_failure_handler(void *eloop_ctx,
						    void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	struct hostapd_data *hapd;

	if (iface->num_bss < 1 || !iface->bss || !iface->bss[0])
		return;
	hapd = iface->bss[0];
	if (hapd->setup_complete_cb)
		hapd->setup_complete_cb(hapd->setup_complete_cb_ctx);
}


static int hostapd_setup_interface_complete_sync(struct hostapd_iface *iface,
						 int err)
{
	struct hostapd_data *hapd = iface->bss[0];
	size_t j;
	u8 *prev_addr;
	int delay_apply_cfg = 0;
	int res_dfs_offload = 0;

	if (err)
		goto fail;

	wpa_printf(MSG_DEBUG, "Completing interface initialization");
	if (iface->freq) {
#ifdef NEED_AP_MLME
		int res;
#endif /* NEED_AP_MLME */

		wpa_printf(MSG_DEBUG, "Mode: %s  Channel: %d  "
			   "Frequency: %d MHz",
			   hostapd_hw_mode_txt(iface->conf->hw_mode),
			   iface->conf->channel, iface->freq);

#ifdef NEED_AP_MLME
		/* Handle DFS only if it is not offloaded to the driver */
		if (!(iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)) {
			/* Check DFS */
			res = hostapd_handle_dfs(iface);
			if (res <= 0) {
				if (res < 0)
					goto fail;
				return res;
			}
		} else {
			/* If DFS is offloaded to the driver */
			res_dfs_offload = hostapd_handle_dfs_offload(iface);
			if (res_dfs_offload <= 0) {
				if (res_dfs_offload < 0)
					goto fail;
			} else {
				wpa_printf(MSG_DEBUG,
					   "Proceed with AP/channel setup");
				/*
				 * If this is a DFS channel, move to completing
				 * AP setup.
				 */
				if (res_dfs_offload == 1)
					goto dfs_offload;
				/* Otherwise fall through. */
			}
		}
#endif /* NEED_AP_MLME */

#ifdef CONFIG_MESH
		if (iface->mconf != NULL) {
			wpa_printf(MSG_DEBUG,
				   "%s: Mesh configuration will be applied while joining the mesh network",
				   iface->bss[0]->conf->iface);
			delay_apply_cfg = 1;
		}
#endif /* CONFIG_MESH */

		if (!delay_apply_cfg &&
		    hostapd_set_freq(hapd, hapd->iconf->hw_mode, iface->freq,
				     hapd->iconf->channel,
				     hapd->iconf->enable_edmg,
				     hapd->iconf->edmg_channel,
				     hapd->iconf->ieee80211n,
				     hapd->iconf->ieee80211ac,
				     hapd->iconf->ieee80211ax,
				     hapd->iconf->ieee80211be,
				     hapd->iconf->secondary_channel,
				     hostapd_get_oper_chwidth(hapd->iconf),
				     hostapd_get_oper_centr_freq_seg0_idx(
					     hapd->iconf),
				     hostapd_get_oper_centr_freq_seg1_idx(
					     hapd->iconf))) {
			wpa_printf(MSG_ERROR, "Could not set channel for "
				   "kernel driver");
			goto fail;
		}
	}

	if (iface->current_mode) {
		if (hostapd_prepare_rates(iface, iface->current_mode)) {
			wpa_printf(MSG_ERROR, "Failed to prepare rates "
				   "table.");
			hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_WARNING,
				       "Failed to prepare rates table.");
			goto fail;
		}
	}

	if (hapd->iconf->rts_threshold >= -1 &&
	    hostapd_set_rts(hapd, hapd->iconf->rts_threshold) &&
	    hapd->iconf->rts_threshold >= -1) {
		wpa_printf(MSG_ERROR, "Could not set RTS threshold for "
			   "kernel driver");
		goto fail;
	}

	if (hapd->iconf->fragm_threshold >= -1 &&
	    hostapd_set_frag(hapd, hapd->iconf->fragm_threshold) &&
	    hapd->iconf->fragm_threshold != -1) {
		wpa_printf(MSG_ERROR, "Could not set fragmentation threshold "
			   "for kernel driver");
		goto fail;
	}

	prev_addr = hapd->own_addr;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (j)
			os_memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hostapd_setup_bss(hapd, j == 0, !iface->conf->mbssid)) {
			for (;;) {
				hapd = iface->bss[j];
				hostapd_bss_deinit_no_free(hapd);
				hostapd_free_hapd_data(hapd);
				if (j == 0)
					break;
				j--;
			}
			goto fail;
		}
		if (is_zero_ether_addr(hapd->conf->bssid))
			prev_addr = hapd->own_addr;
	}

	if (hapd->iconf->mbssid) {
		for (j = 0; hapd->iconf->mbssid && j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			if (hostapd_start_beacon(hapd, true)) {
				for (;;) {
					hapd = iface->bss[j];
					hostapd_bss_deinit_no_free(hapd);
					hostapd_free_hapd_data(hapd);
					if (j == 0)
						break;
					j--;
				}
				goto fail;
			}
		}
	}

	hapd = iface->bss[0];

	hostapd_tx_queue_params(iface);

	ap_list_init(iface);

	hostapd_set_acl(hapd);

	if (hostapd_driver_commit(hapd) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		goto fail;
	}

	/*
	 * WPS UPnP module can be initialized only when the "upnp_iface" is up.
	 * If "interface" and "upnp_iface" are the same (e.g., non-bridge
	 * mode), the interface is up only after driver_commit, so initialize
	 * WPS after driver_commit.
	 */
	for (j = 0; j < iface->num_bss; j++) {
		if (hostapd_init_wps_complete(iface->bss[j]))
			goto fail;
	}

	if ((iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) &&
	    !res_dfs_offload) {
		/*
		 * If freq is DFS, and DFS is offloaded to the driver, then wait
		 * for CAC to complete.
		 */
		wpa_printf(MSG_DEBUG, "%s: Wait for CAC to complete", __func__);
		return res_dfs_offload;
	}

#ifdef NEED_AP_MLME
dfs_offload:
#endif /* NEED_AP_MLME */

#ifdef CONFIG_FST
	if (hapd->iconf->fst_cfg.group_id[0]) {
		struct fst_wpa_obj iface_obj;

		fst_hostapd_fill_iface_obj(hapd, &iface_obj);
		iface->fst = fst_attach(hapd->conf->iface, hapd->own_addr,
					&iface_obj, &hapd->iconf->fst_cfg);
		if (!iface->fst) {
			wpa_printf(MSG_ERROR, "Could not attach to FST %s",
				   hapd->iconf->fst_cfg.group_id);
			goto fail;
		}
	}
#endif /* CONFIG_FST */

	hostapd_set_state(iface, HAPD_IFACE_ENABLED);
	hostapd_owe_update_trans(iface);
	airtime_policy_update_init(iface);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);
	if (hapd->setup_complete_cb)
		hapd->setup_complete_cb(hapd->setup_complete_cb_ctx);

#ifdef CONFIG_MESH
	if (delay_apply_cfg && !iface->mconf) {
		wpa_printf(MSG_ERROR, "Error while completing mesh init");
		goto fail;
	}
#endif /* CONFIG_MESH */

	wpa_printf(MSG_DEBUG, "%s: Setup of interface done.",
		   iface->bss[0]->conf->iface);
	if (iface->interfaces && iface->interfaces->terminate_on_error > 0)
		iface->interfaces->terminate_on_error--;

	for (j = 0; j < iface->num_bss; j++)
		hostapd_neighbor_set_own_report(iface->bss[j]);

	if (iface->interfaces && iface->interfaces->count > 1)
		ieee802_11_set_beacons(iface);

	return 0;

fail:
	wpa_printf(MSG_ERROR, "Interface initialization failed");

	if (iface->is_no_ir) {
		hostapd_set_state(iface, HAPD_IFACE_NO_IR);
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
		return 0;
	}

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
#ifdef CONFIG_FST
	if (iface->fst) {
		fst_detach(iface->fst);
		iface->fst = NULL;
	}
#endif /* CONFIG_FST */

	if (iface->interfaces && iface->interfaces->terminate_on_error) {
		eloop_terminate();
	} else if (hapd->setup_complete_cb) {
		/*
		 * Calling hapd->setup_complete_cb directly may cause iface
		 * deinitialization which may be accessed later by the caller.
		 */
		eloop_register_timeout(0, 0,
				       hostapd_interface_setup_failure_handler,
				       iface, NULL);
	}

	return -1;
}


/**
 * hostapd_setup_interface_complete - Complete interface setup
 *
 * This function is called when previous steps in the interface setup has been
 * completed. This can also start operations, e.g., DFS, that will require
 * additional processing before interface is ready to be enabled. Such
 * operations will call this function from eloop callbacks when finished.
 */
int hostapd_setup_interface_complete(struct hostapd_iface *iface, int err)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	struct hostapd_data *hapd = iface->bss[0];
	unsigned int i;
	int not_ready_in_sync_ifaces = 0;

	if (!iface->need_to_start_in_sync)
		return hostapd_setup_interface_complete_sync(iface, err);

	if (err) {
		wpa_printf(MSG_ERROR, "Interface initialization failed");
		iface->need_to_start_in_sync = 0;

		if (iface->is_no_ir) {
			hostapd_set_state(iface, HAPD_IFACE_NO_IR);
			wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_NO_IR);
			return 0;
		}

		hostapd_set_state(iface, HAPD_IFACE_DISABLED);
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
		if (interfaces && interfaces->terminate_on_error)
			eloop_terminate();
		return -1;
	}

	if (iface->ready_to_start_in_sync) {
		/* Already in ready and waiting. should never happpen */
		return 0;
	}

	for (i = 0; i < interfaces->count; i++) {
		if (interfaces->iface[i]->need_to_start_in_sync &&
		    !interfaces->iface[i]->ready_to_start_in_sync)
			not_ready_in_sync_ifaces++;
	}

	/*
	 * Check if this is the last interface, if yes then start all the other
	 * waiting interfaces. If not, add this interface to the waiting list.
	 */
	if (not_ready_in_sync_ifaces > 1 && iface->state == HAPD_IFACE_DFS) {
		/*
		 * If this interface went through CAC, do not synchronize, just
		 * start immediately.
		 */
		iface->need_to_start_in_sync = 0;
		wpa_printf(MSG_INFO,
			   "%s: Finished CAC - bypass sync and start interface",
			   iface->bss[0]->conf->iface);
		return hostapd_setup_interface_complete_sync(iface, err);
	}

	if (not_ready_in_sync_ifaces > 1) {
		/* need to wait as there are other interfaces still coming up */
		iface->ready_to_start_in_sync = 1;
		wpa_printf(MSG_INFO,
			   "%s: Interface waiting to sync with other interfaces",
			   iface->bss[0]->conf->iface);
		return 0;
	}

	wpa_printf(MSG_INFO,
		   "%s: Last interface to sync - starting all interfaces",
		   iface->bss[0]->conf->iface);
	iface->need_to_start_in_sync = 0;
	hostapd_setup_interface_complete_sync(iface, err);
	for (i = 0; i < interfaces->count; i++) {
		if (interfaces->iface[i]->need_to_start_in_sync &&
		    interfaces->iface[i]->ready_to_start_in_sync) {
			hostapd_setup_interface_complete_sync(
				interfaces->iface[i], 0);
			/* Only once the interfaces are sync started */
			interfaces->iface[i]->need_to_start_in_sync = 0;
		}
	}

	return 0;
}


/**
 * hostapd_setup_interface - Setup of an interface
 * @iface: Pointer to interface data.
 * Returns: 0 on success, -1 on failure
 *
 * Initializes the driver interface, validates the configuration,
 * and sets driver parameters based on the configuration.
 * Flushes old stations, sets the channel, encryption,
 * beacons, and WDS links based on the configuration.
 *
 * If interface setup requires more time, e.g., to perform HT co-ex scans, ACS,
 * or DFS operations, this function returns 0 before such operations have been
 * completed. The pending operations are registered into eloop and will be
 * completed from eloop callbacks. Those callbacks end up calling
 * hostapd_setup_interface_complete() once setup has been completed.
 */
int hostapd_setup_interface(struct hostapd_iface *iface)
{
	int ret;

	if (!iface->conf)
		return -1;
	ret = setup_interface(iface);
	if (ret) {
		wpa_printf(MSG_ERROR, "%s: Unable to setup interface.",
			   iface->conf->bss[0]->iface);
		return -1;
	}

	return 0;
}


/**
 * hostapd_alloc_bss_data - Allocate and initialize per-BSS data
 * @hapd_iface: Pointer to interface data
 * @conf: Pointer to per-interface configuration
 * @bss: Pointer to per-BSS configuration for this BSS
 * Returns: Pointer to allocated BSS data
 *
 * This function is used to allocate per-BSS data structure. This data will be
 * freed after hostapd_cleanup() is called for it during interface
 * deinitialization.
 */
struct hostapd_data *
hostapd_alloc_bss_data(struct hostapd_iface *hapd_iface,
		       struct hostapd_config *conf,
		       struct hostapd_bss_config *bss)
{
	struct hostapd_data *hapd;

	hapd = os_zalloc(sizeof(*hapd));
	if (hapd == NULL)
		return NULL;

	hapd->new_assoc_sta_cb = hostapd_new_assoc_sta;
	hapd->iconf = conf;
	hapd->conf = bss;
	hapd->iface = hapd_iface;
	if (conf)
		hapd->driver = conf->driver;
	hapd->ctrl_sock = -1;
	dl_list_init(&hapd->ctrl_dst);
	dl_list_init(&hapd->nr_db);
	hapd->dhcp_sock = -1;
#ifdef CONFIG_IEEE80211R_AP
	dl_list_init(&hapd->l2_queue);
	dl_list_init(&hapd->l2_oui_queue);
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	dl_list_init(&hapd->sae_commit_queue);
#endif /* CONFIG_SAE */

	return hapd;
}


static void hostapd_bss_deinit(struct hostapd_data *hapd)
{
	if (!hapd)
		return;
	wpa_printf(MSG_DEBUG, "%s: deinit bss %s", __func__,
		   hapd->conf ? hapd->conf->iface : "N/A");
	hostapd_bss_deinit_no_free(hapd);
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
#ifdef CONFIG_SQLITE
	if (hapd->rad_attr_db) {
		sqlite3_close(hapd->rad_attr_db);
		hapd->rad_attr_db = NULL;
	}
#endif /* CONFIG_SQLITE */

	hostapd_bss_link_deinit(hapd);
	hostapd_cleanup(hapd);
}


void hostapd_interface_deinit(struct hostapd_iface *iface)
{
	int j;

	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	if (iface == NULL)
		return;

	hostapd_set_state(iface, HAPD_IFACE_DISABLED);

	eloop_cancel_timeout(channel_list_update_timeout, iface, NULL);
	iface->wait_channel_update = 0;
	iface->is_no_ir = false;

#ifdef CONFIG_FST
	if (iface->fst) {
		fst_detach(iface->fst);
		iface->fst = NULL;
	}
#endif /* CONFIG_FST */

	for (j = (int) iface->num_bss - 1; j >= 0; j--) {
		if (!iface->bss)
			break;
		hostapd_bss_deinit(iface->bss[j]);
	}

#ifdef NEED_AP_MLME
	hostapd_stop_setup_timers(iface);
	eloop_cancel_timeout(ap_ht2040_timeout, iface, NULL);
#endif /* NEED_AP_MLME */
}


#ifdef CONFIG_IEEE80211BE

static void hostapd_mld_ref_inc(struct hostapd_mld *mld)
{
	if (!mld)
		return;

	if (mld->refcount == HOSTAPD_MLD_MAX_REF_COUNT) {
		wpa_printf(MSG_ERROR, "AP MLD %s: Ref count overflow",
			   mld->name);
		return;
	}

	mld->refcount++;
}


static void hostapd_mld_ref_dec(struct hostapd_mld *mld)
{
	if (!mld)
		return;

	if (!mld->refcount) {
		wpa_printf(MSG_ERROR, "AP MLD %s: Ref count underflow",
			   mld->name);
		return;
	}

	mld->refcount--;
}

#endif /* CONFIG_IEEE80211BE */


void hostapd_interface_free(struct hostapd_iface *iface)
{
	size_t j;
	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	for (j = 0; j < iface->num_bss; j++) {
		if (!iface->bss)
			break;
#ifdef CONFIG_IEEE80211BE
		if (iface->bss[j])
			hostapd_mld_ref_dec(iface->bss[j]->mld);
#endif /* CONFIG_IEEE80211BE */
		wpa_printf(MSG_DEBUG, "%s: free hapd %p",
			   __func__, iface->bss[j]);
		os_free(iface->bss[j]);
	}
	hostapd_cleanup_iface(iface);
}


struct hostapd_iface * hostapd_alloc_iface(void)
{
	struct hostapd_iface *hapd_iface;

	hapd_iface = os_zalloc(sizeof(*hapd_iface));
	if (!hapd_iface)
		return NULL;

	dl_list_init(&hapd_iface->sta_seen);

	return hapd_iface;
}


#ifdef CONFIG_IEEE80211BE
static void hostapd_bss_alloc_link_id(struct hostapd_data *hapd)
{
	hapd->mld_link_id = hapd->mld->next_link_id++;
	wpa_printf(MSG_DEBUG, "AP MLD: %s: Link ID %d assigned.",
		   hapd->mld->name, hapd->mld_link_id);
}
#endif /* CONFIG_IEEE80211BE */


static void hostapd_bss_setup_multi_link(struct hostapd_data *hapd,
					 struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_mld *mld, **all_mld;
	struct hostapd_bss_config *conf;
	size_t i;

	conf = hapd->conf;

	if (!hapd->iconf || !hapd->iconf->ieee80211be || !conf->mld_ap ||
	    conf->disable_11be)
		return;

	for (i = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];

		if (!mld || os_strcmp(conf->iface, mld->name) != 0)
			continue;

		hapd->mld = mld;
		hostapd_mld_ref_inc(mld);
		hostapd_bss_alloc_link_id(hapd);
		break;
	}

	if (hapd->mld)
		return;

	mld = os_zalloc(sizeof(struct hostapd_mld));
	if (!mld)
		goto fail;

	os_strlcpy(mld->name, conf->iface, sizeof(conf->iface));
	dl_list_init(&mld->links);

	wpa_printf(MSG_DEBUG, "AP MLD %s created", mld->name);

	hapd->mld = mld;
	hostapd_mld_ref_inc(mld);
	hostapd_bss_alloc_link_id(hapd);

	all_mld = os_realloc_array(interfaces->mld, interfaces->mld_count + 1,
				   sizeof(struct hostapd_mld *));
	if (!all_mld)
		goto fail;

	interfaces->mld = all_mld;
	interfaces->mld[interfaces->mld_count] = mld;
	interfaces->mld_count++;

	return;
fail:
	if (!mld)
		return;

	wpa_printf(MSG_DEBUG, "AP MLD %s: free mld %p", mld->name, mld);
	os_free(mld);
	hapd->mld = NULL;
#endif /* CONFIG_IEEE80211BE */
}


static void hostapd_cleanup_unused_mlds(struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_mld *mld, **all_mld;
	size_t i, j, num_mlds;
	bool forced_remove, remove;

	if (!interfaces->mld)
		return;

	num_mlds = interfaces->mld_count;

	for (i = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];
		if (!mld)
			continue;

		remove = false;
		forced_remove = false;

		if (!mld->refcount)
			remove = true;

		/* If MLD is still being referenced but the number of interfaces
		 * is zero, it is safe to force its deletion. Normally, this
		 * should not happen but even if it does, let us free the
		 * memory.
		 */
		if (!remove && !interfaces->count)
			forced_remove = true;

		if (!remove && !forced_remove)
			continue;

		wpa_printf(MSG_DEBUG, "AP MLD %s: Freed%s", mld->name,
			   forced_remove ? " (forced)" : "");
		os_free(mld);
		interfaces->mld[i] = NULL;
		num_mlds--;
	}

	if (!num_mlds) {
		interfaces->mld_count = 0;
		os_free(interfaces->mld);
		interfaces->mld = NULL;
		return;
	}

	all_mld = os_zalloc(num_mlds * sizeof(struct hostapd_mld *));
	if (!all_mld) {
		wpa_printf(MSG_ERROR,
			   "AP MLD: Failed to re-allocate the MLDs. Expect issues");
		return;
	}

	for (i = 0, j = 0; i < interfaces->mld_count; i++) {
		mld = interfaces->mld[i];
		if (!mld)
			continue;

		all_mld[j++] = mld;
	}

	/* This should not happen */
	if (j != num_mlds) {
		wpa_printf(MSG_DEBUG,
			   "AP MLD: Some error occurred while reallocating MLDs. Expect issues.");
		os_free(all_mld);
		return;
	}

	os_free(interfaces->mld);
	interfaces->mld = all_mld;
	interfaces->mld_count = num_mlds;
#endif /* CONFIG_IEEE80211BE */
}


/**
 * hostapd_init - Allocate and initialize per-interface data
 * @config_file: Path to the configuration file
 * Returns: Pointer to the allocated interface data or %NULL on failure
 *
 * This function is used to allocate main data structures for per-interface
 * data. The allocated data buffer will be freed by calling
 * hostapd_cleanup_iface().
 */
struct hostapd_iface * hostapd_init(struct hapd_interfaces *interfaces,
				    const char *config_file)
{
	struct hostapd_iface *hapd_iface = NULL;
	struct hostapd_config *conf = NULL;
	struct hostapd_data *hapd;
	size_t i;

	hapd_iface = hostapd_alloc_iface();
	if (hapd_iface == NULL)
		goto fail;

	hapd_iface->config_fname = os_strdup(config_file);
	if (hapd_iface->config_fname == NULL)
		goto fail;

	conf = interfaces->config_read_cb(hapd_iface->config_fname);
	if (conf == NULL)
		goto fail;
	hapd_iface->conf = conf;

	hapd_iface->num_bss = conf->num_bss;
	hapd_iface->bss = os_calloc(conf->num_bss,
				    sizeof(struct hostapd_data *));
	if (hapd_iface->bss == NULL)
		goto fail;

	for (i = 0; i < conf->num_bss; i++) {
		hapd = hapd_iface->bss[i] =
			hostapd_alloc_bss_data(hapd_iface, conf,
					       conf->bss[i]);
		if (hapd == NULL)
			goto fail;
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, interfaces);
	}

	hapd_iface->is_ch_switch_dfs = false;
	return hapd_iface;

fail:
	wpa_printf(MSG_ERROR, "Failed to set up interface with %s",
		   config_file);
	if (conf)
		hostapd_config_free(conf);
	if (hapd_iface) {
		os_free(hapd_iface->config_fname);
		os_free(hapd_iface->bss);
		wpa_printf(MSG_DEBUG, "%s: free iface %p",
			   __func__, hapd_iface);
		os_free(hapd_iface);
	}
	return NULL;
}


static int ifname_in_use(struct hapd_interfaces *interfaces, const char *ifname)
{
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd = iface->bss[j];
			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				return 1;
		}
	}

	return 0;
}


/**
 * hostapd_interface_init_bss - Read configuration file and init BSS data
 *
 * This function is used to parse configuration file for a BSS. This BSS is
 * added to an existing interface sharing the same radio (if any) or a new
 * interface is created if this is the first interface on a radio. This
 * allocate memory for the BSS. No actual driver operations are started.
 *
 * This is similar to hostapd_interface_init(), but for a case where the
 * configuration is used to add a single BSS instead of all BSSes for a radio.
 */
struct hostapd_iface *
hostapd_interface_init_bss(struct hapd_interfaces *interfaces, const char *phy,
			   const char *config_fname, int debug)
{
	struct hostapd_iface *new_iface = NULL, *iface = NULL;
	struct hostapd_data *hapd;
	int k;
	size_t i, bss_idx;

	if (!phy || !*phy)
		return NULL;

	for (i = 0; i < interfaces->count; i++) {
		if (os_strcmp(interfaces->iface[i]->phy, phy) == 0) {
			iface = interfaces->iface[i];
			break;
		}
	}

	wpa_printf(MSG_INFO, "Configuration file: %s (phy %s)%s",
		   config_fname, phy, iface ? "" : " --> new PHY");
	if (iface) {
		struct hostapd_config *conf;
		struct hostapd_bss_config **tmp_conf;
		struct hostapd_data **tmp_bss;
		struct hostapd_bss_config *bss;
		const char *ifname;

		/* Add new BSS to existing iface */
		conf = interfaces->config_read_cb(config_fname);
		if (conf == NULL)
			return NULL;
		if (conf->num_bss > 1) {
			wpa_printf(MSG_ERROR, "Multiple BSSes specified in BSS-config");
			hostapd_config_free(conf);
			return NULL;
		}

		ifname = conf->bss[0]->iface;
		if (ifname[0] != '\0' && ifname_in_use(interfaces, ifname)) {
			wpa_printf(MSG_ERROR,
				   "Interface name %s already in use", ifname);
			hostapd_config_free(conf);
			return NULL;
		}

		tmp_conf = os_realloc_array(
			iface->conf->bss, iface->conf->num_bss + 1,
			sizeof(struct hostapd_bss_config *));
		tmp_bss = os_realloc_array(iface->bss, iface->num_bss + 1,
					   sizeof(struct hostapd_data *));
		if (tmp_bss)
			iface->bss = tmp_bss;
		if (tmp_conf) {
			iface->conf->bss = tmp_conf;
			iface->conf->last_bss = tmp_conf[0];
		}
		if (tmp_bss == NULL || tmp_conf == NULL) {
			hostapd_config_free(conf);
			return NULL;
		}
		bss = iface->conf->bss[iface->conf->num_bss] = conf->bss[0];
		iface->conf->num_bss++;

		hapd = hostapd_alloc_bss_data(iface, iface->conf, bss);
		if (hapd == NULL) {
			iface->conf->num_bss--;
			hostapd_config_free(conf);
			return NULL;
		}
		iface->conf->last_bss = bss;
		iface->bss[iface->num_bss] = hapd;
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, interfaces);


		bss_idx = iface->num_bss++;
		conf->num_bss--;
		conf->bss[0] = NULL;
		hostapd_config_free(conf);
	} else {
		/* Add a new iface with the first BSS */
		new_iface = iface = hostapd_init(interfaces, config_fname);
		if (!iface)
			return NULL;
		os_strlcpy(iface->phy, phy, sizeof(iface->phy));
		iface->interfaces = interfaces;
		bss_idx = 0;
	}

	for (k = 0; k < debug; k++) {
		if (iface->bss[bss_idx]->conf->logger_stdout_level > 0)
			iface->bss[bss_idx]->conf->logger_stdout_level--;
	}

	if (iface->conf->bss[bss_idx]->iface[0] == '\0' &&
	    !hostapd_drv_none(iface->bss[bss_idx])) {
		wpa_printf(MSG_ERROR, "Interface name not specified in %s",
			   config_fname);
		if (new_iface)
			hostapd_interface_deinit_free(new_iface);
		return NULL;
	}

	return iface;
}


static void hostapd_cleanup_driver(const struct wpa_driver_ops *driver,
				   void *drv_priv, struct hostapd_iface *iface)
{
	if (!driver || !driver->hapd_deinit || !drv_priv)
		return;

#ifdef CONFIG_IEEE80211BE
	/* In case of non-ML operation, de-init. But if ML operation exist,
	 * even if that's the last BSS in the interface, the driver (drv) could
	 * be in use for a different AP MLD. Hence, need to check if drv is
	 * still being used by some other BSS before de-initiallizing. */
	if (!iface->bss[0]->conf->mld_ap) {
		driver->hapd_deinit(drv_priv);
	} else if (hostapd_mld_is_first_bss(iface->bss[0]) &&
		   driver->is_drv_shared &&
		   !driver->is_drv_shared(drv_priv, iface->bss[0])) {
		driver->hapd_deinit(drv_priv);
	} else if (hostapd_if_link_remove(iface->bss[0],
					  WPA_IF_AP_BSS,
					  iface->bss[0]->conf->iface,
					  iface->bss[0]->mld_link_id)) {
		wpa_printf(MSG_WARNING, "Failed to remove BSS interface %s",
			   iface->bss[0]->conf->iface);
	}
#else /* CONFIG_IEEE80211BE */
	driver->hapd_deinit(drv_priv);
#endif /* CONFIG_IEEE80211BE */
	iface->bss[0]->drv_priv = NULL;
}


void hostapd_interface_deinit_free(struct hostapd_iface *iface)
{
	const struct wpa_driver_ops *driver;
	void *drv_priv;

	wpa_printf(MSG_DEBUG, "%s(%p)", __func__, iface);
	if (iface == NULL)
		return;
	wpa_printf(MSG_DEBUG, "%s: num_bss=%u conf->num_bss=%u",
		   __func__, (unsigned int) iface->num_bss,
		   (unsigned int) iface->conf->num_bss);
	driver = iface->bss[0]->driver;
	drv_priv = iface->bss[0]->drv_priv;
	hostapd_interface_deinit(iface);
	wpa_printf(MSG_DEBUG, "%s: driver=%p drv_priv=%p -> hapd_deinit",
		   __func__, driver, drv_priv);
	hostapd_cleanup_driver(driver, drv_priv, iface);
	hostapd_interface_free(iface);
}


static void hostapd_deinit_driver(const struct wpa_driver_ops *driver,
				  void *drv_priv,
				  struct hostapd_iface *hapd_iface)
{
	size_t j;

	wpa_printf(MSG_DEBUG, "%s: driver=%p drv_priv=%p -> hapd_deinit",
		   __func__, driver, drv_priv);

	hostapd_cleanup_driver(driver, drv_priv, hapd_iface);

	if (driver && driver->hapd_deinit && drv_priv) {
		for (j = 0; j < hapd_iface->num_bss; j++) {
			wpa_printf(MSG_DEBUG, "%s:bss[%d]->drv_priv=%p",
				   __func__, (int) j,
				   hapd_iface->bss[j]->drv_priv);
			if (hapd_iface->bss[j]->drv_priv == drv_priv) {
				hapd_iface->bss[j]->drv_priv = NULL;
				hapd_iface->extended_capa = NULL;
				hapd_iface->extended_capa_mask = NULL;
				hapd_iface->extended_capa_len = 0;
			}
		}
	}
}


static void hostapd_refresh_all_iface_beacons(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (!hapd_iface->interfaces || hapd_iface->interfaces->count <= 1)
		return;

	for (j = 0; j < hapd_iface->interfaces->count; j++) {
		if (hapd_iface->interfaces->iface[j] == hapd_iface)
			continue;

		ieee802_11_update_beacons(hapd_iface->interfaces->iface[j]);
	}
}


int hostapd_enable_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;

	if (!hapd_iface)
		return -1;

	if (hapd_iface->enable_iface_cb)
		return hapd_iface->enable_iface_cb(hapd_iface);

	if (hapd_iface->bss[0]->drv_priv != NULL) {
		wpa_printf(MSG_ERROR, "Interface %s already enabled",
			   hapd_iface->conf->bss[0]->iface);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Enable interface %s",
		   hapd_iface->conf->bss[0]->iface);

	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_set_security_params(hapd_iface->conf->bss[j], 1);
	if (hostapd_config_check(hapd_iface->conf, 1) < 0) {
		wpa_printf(MSG_INFO, "Invalid configuration - cannot enable");
		return -1;
	}

	if (hapd_iface->interfaces == NULL ||
	    hapd_iface->interfaces->driver_init == NULL ||
	    hapd_iface->interfaces->driver_init(hapd_iface))
		return -1;

	if (hostapd_setup_interface(hapd_iface)) {
		hostapd_deinit_driver(hapd_iface->bss[0]->driver,
				      hapd_iface->bss[0]->drv_priv,
				      hapd_iface);
		return -1;
	}

	hostapd_refresh_all_iface_beacons(hapd_iface);

	return 0;
}


int hostapd_reload_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;

	wpa_printf(MSG_DEBUG, "Reload interface %s",
		   hapd_iface->conf->bss[0]->iface);
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_set_security_params(hapd_iface->conf->bss[j], 1);
	if (hostapd_config_check(hapd_iface->conf, 1) < 0) {
		wpa_printf(MSG_ERROR, "Updated configuration is invalid");
		return -1;
	}
	hostapd_clear_old(hapd_iface);
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_reload_bss(hapd_iface->bss[j]);

	return 0;
}


int hostapd_reload_bss_only(struct hostapd_data *bss)
{

	wpa_printf(MSG_DEBUG, "Reload BSS %s", bss->conf->iface);
	hostapd_set_security_params(bss->conf, 1);
	if (hostapd_config_check(bss->iconf, 1) < 0) {
		wpa_printf(MSG_ERROR, "Updated BSS configuration is invalid");
		return -1;
	}
	hostapd_clear_old_bss(bss);
	hostapd_reload_bss(bss);
	return 0;
}


int hostapd_disable_iface(struct hostapd_iface *hapd_iface)
{
	size_t j;
	const struct wpa_driver_ops *driver;
	void *drv_priv;

	if (hapd_iface == NULL)
		return -1;

	if (hapd_iface->disable_iface_cb)
		return hapd_iface->disable_iface_cb(hapd_iface);

	if (hapd_iface->bss[0]->drv_priv == NULL) {
		wpa_printf(MSG_INFO, "Interface %s already disabled",
			   hapd_iface->conf->bss[0]->iface);
		return -1;
	}

	wpa_msg(hapd_iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_DISABLED);
	driver = hapd_iface->bss[0]->driver;
	drv_priv = hapd_iface->bss[0]->drv_priv;

	hapd_iface->driver_ap_teardown =
		!!(hapd_iface->drv_flags &
		   WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);

#ifdef NEED_AP_MLME
	for (j = 0; j < hapd_iface->num_bss; j++)
		hostapd_cleanup_cs_params(hapd_iface->bss[j]);
#endif /* NEED_AP_MLME */

	/* same as hostapd_interface_deinit without deinitializing ctrl-iface */
	for (j = 0; j < hapd_iface->num_bss; j++) {
		struct hostapd_data *hapd = hapd_iface->bss[j];
		hostapd_bss_deinit_no_free(hapd);
		hostapd_bss_link_deinit(hapd);
		hostapd_free_hapd_data(hapd);
	}

	hostapd_deinit_driver(driver, drv_priv, hapd_iface);

	/* From hostapd_cleanup_iface: These were initialized in
	 * hostapd_setup_interface and hostapd_setup_interface_complete
	 */
	hostapd_cleanup_iface_partial(hapd_iface);

	wpa_printf(MSG_DEBUG, "Interface %s disabled",
		   hapd_iface->bss[0]->conf->iface);
	hostapd_set_state(hapd_iface, HAPD_IFACE_DISABLED);
	hostapd_refresh_all_iface_beacons(hapd_iface);
	return 0;
}


static struct hostapd_iface *
hostapd_iface_alloc(struct hapd_interfaces *interfaces)
{
	struct hostapd_iface **iface, *hapd_iface;

	iface = os_realloc_array(interfaces->iface, interfaces->count + 1,
				 sizeof(struct hostapd_iface *));
	if (iface == NULL)
		return NULL;
	interfaces->iface = iface;
	hapd_iface = interfaces->iface[interfaces->count] =
		hostapd_alloc_iface();
	if (hapd_iface == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory for "
			   "the interface", __func__);
		return NULL;
	}
	interfaces->count++;
	hapd_iface->interfaces = interfaces;

	return hapd_iface;
}


static struct hostapd_config *
hostapd_config_alloc(struct hapd_interfaces *interfaces, const char *ifname,
		     const char *ctrl_iface, const char *driver)
{
	struct hostapd_bss_config *bss;
	struct hostapd_config *conf;

	/* Allocates memory for bss and conf */
	conf = hostapd_config_defaults();
	if (conf == NULL) {
		 wpa_printf(MSG_ERROR, "%s: Failed to allocate memory for "
				"configuration", __func__);
		 return NULL;
	}

	if (driver) {
		int j;

		for (j = 0; wpa_drivers[j]; j++) {
			if (os_strcmp(driver, wpa_drivers[j]->name) == 0) {
				conf->driver = wpa_drivers[j];
				goto skip;
			}
		}

		wpa_printf(MSG_ERROR,
			   "Invalid/unknown driver '%s' - registering the default driver",
			   driver);
	}

	conf->driver = wpa_drivers[0];
	if (conf->driver == NULL) {
		wpa_printf(MSG_ERROR, "No driver wrappers registered!");
		hostapd_config_free(conf);
		return NULL;
	}

skip:
	bss = conf->last_bss = conf->bss[0];

	os_strlcpy(bss->iface, ifname, sizeof(bss->iface));
	bss->ctrl_interface = os_strdup(ctrl_iface);
	if (bss->ctrl_interface == NULL) {
		hostapd_config_free(conf);
		return NULL;
	}

	/* Reading configuration file skipped, will be done in SET!
	 * From reading the configuration till the end has to be done in
	 * SET
	 */
	return conf;
}


static int hostapd_data_alloc(struct hostapd_iface *hapd_iface,
			      struct hostapd_config *conf)
{
	size_t i;
	struct hostapd_data *hapd;

	hapd_iface->bss = os_calloc(conf->num_bss,
				    sizeof(struct hostapd_data *));
	if (hapd_iface->bss == NULL)
		return -1;

	for (i = 0; i < conf->num_bss; i++) {
		hapd = hapd_iface->bss[i] =
			hostapd_alloc_bss_data(hapd_iface, conf, conf->bss[i]);
		if (hapd == NULL) {
			while (i > 0) {
				i--;
				os_free(hapd_iface->bss[i]);
				hapd_iface->bss[i] = NULL;
			}
			os_free(hapd_iface->bss);
			hapd_iface->bss = NULL;
			return -1;
		}
		hapd->msg_ctx = hapd;
		hostapd_bss_setup_multi_link(hapd, hapd_iface->interfaces);
	}

	hapd_iface->conf = conf;
	hapd_iface->num_bss = conf->num_bss;

	return 0;
}


int hostapd_add_iface(struct hapd_interfaces *interfaces, char *buf)
{
	struct hostapd_config *conf = NULL;
	struct hostapd_iface *hapd_iface = NULL, *new_iface = NULL;
	struct hostapd_data *hapd;
	char *ptr;
	size_t i, j;
	const char *conf_file = NULL, *phy_name = NULL;

	if (os_strncmp(buf, "bss_config=", 11) == 0) {
		char *pos;
		phy_name = buf + 11;
		pos = os_strchr(phy_name, ':');
		if (!pos)
			return -1;
		*pos++ = '\0';
		conf_file = pos;
		if (!os_strlen(conf_file))
			return -1;

		hapd_iface = hostapd_interface_init_bss(interfaces, phy_name,
							conf_file, 0);
		if (!hapd_iface)
			return -1;
		for (j = 0; j < interfaces->count; j++) {
			if (interfaces->iface[j] == hapd_iface)
				break;
		}
		if (j == interfaces->count) {
			struct hostapd_iface **tmp;
			tmp = os_realloc_array(interfaces->iface,
					       interfaces->count + 1,
					       sizeof(struct hostapd_iface *));
			if (!tmp) {
				hostapd_interface_deinit_free(hapd_iface);
				return -1;
			}
			interfaces->iface = tmp;
			interfaces->iface[interfaces->count++] = hapd_iface;
			new_iface = hapd_iface;
		}

		if (new_iface) {
			if (interfaces->driver_init(hapd_iface))
				goto fail;

			if (hostapd_setup_interface(hapd_iface)) {
				hostapd_deinit_driver(
					hapd_iface->bss[0]->driver,
					hapd_iface->bss[0]->drv_priv,
					hapd_iface);
				goto fail;
			}
		} else {
			/* Assign new BSS with bss[0]'s driver info */
			hapd = hapd_iface->bss[hapd_iface->num_bss - 1];
			hapd->driver = hapd_iface->bss[0]->driver;
			hapd->drv_priv = hapd_iface->bss[0]->drv_priv;
			os_memcpy(hapd->own_addr, hapd_iface->bss[0]->own_addr,
				  ETH_ALEN);

			if (start_ctrl_iface_bss(hapd) < 0 ||
			    (hapd_iface->state == HAPD_IFACE_ENABLED &&
			     hostapd_setup_bss(hapd, -1, true))) {
				hostapd_bss_link_deinit(hapd);
				hostapd_cleanup(hapd);
				hapd_iface->bss[hapd_iface->num_bss - 1] = NULL;
				hapd_iface->conf->num_bss--;
				hapd_iface->num_bss--;
				wpa_printf(MSG_DEBUG, "%s: free hapd %p %s",
					   __func__, hapd, hapd->conf->iface);
				hostapd_config_free_bss(hapd->conf);
				hapd->conf = NULL;
#ifdef CONFIG_IEEE80211BE
				hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
				os_free(hapd);
				return -1;
			}
		}
		hostapd_owe_update_trans(hapd_iface);
		return 0;
	}

	ptr = os_strchr(buf, ' ');
	if (ptr == NULL)
		return -1;
	*ptr++ = '\0';

	if (os_strncmp(ptr, "config=", 7) == 0)
		conf_file = ptr + 7;

	for (i = 0; i < interfaces->count; i++) {
		bool mld_ap = false;

#ifdef CONFIG_IEEE80211BE
		mld_ap = interfaces->iface[i]->conf->bss[0]->mld_ap;
#endif /* CONFIG_IEEE80211BE */

		if (!os_strcmp(interfaces->iface[i]->conf->bss[0]->iface,
			       buf) && !mld_ap) {
			wpa_printf(MSG_INFO, "Cannot add interface - it "
				   "already exists");
			return -1;
		}
	}

	hapd_iface = hostapd_iface_alloc(interfaces);
	if (hapd_iface == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for interface", __func__);
		goto fail;
	}
	new_iface = hapd_iface;

	if (conf_file && interfaces->config_read_cb) {
		conf = interfaces->config_read_cb(conf_file);
		if (conf && conf->bss)
			os_strlcpy(conf->bss[0]->iface, buf,
				   sizeof(conf->bss[0]->iface));
	} else {
		char *driver = os_strchr(ptr, ' ');

		if (driver)
			*driver++ = '\0';
		conf = hostapd_config_alloc(interfaces, buf, ptr, driver);
	}

	if (conf == NULL || conf->bss == NULL) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for configuration", __func__);
		goto fail;
	}

	if (hostapd_data_alloc(hapd_iface, conf) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to allocate memory "
			   "for hostapd", __func__);
		goto fail;
	}
	conf = NULL;

	if (start_ctrl_iface(hapd_iface) < 0)
		goto fail;

	wpa_printf(MSG_INFO, "Add interface '%s'",
		   hapd_iface->conf->bss[0]->iface);

	return 0;

fail:
	if (conf)
		hostapd_config_free(conf);
	if (hapd_iface) {
		if (hapd_iface->bss) {
			for (i = 0; i < hapd_iface->num_bss; i++) {
				hapd = hapd_iface->bss[i];
				if (!hapd)
					continue;
				if (hapd_iface->interfaces &&
				    hapd_iface->interfaces->ctrl_iface_deinit)
					hapd_iface->interfaces->
						ctrl_iface_deinit(hapd);
				wpa_printf(MSG_DEBUG, "%s: free hapd %p (%s)",
					   __func__, hapd_iface->bss[i],
					   hapd->conf->iface);
				hostapd_bss_link_deinit(hapd);
				hostapd_cleanup(hapd);
#ifdef CONFIG_IEEE80211BE
				hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
				os_free(hapd);
				hapd_iface->bss[i] = NULL;
			}
			os_free(hapd_iface->bss);
			hapd_iface->bss = NULL;
		}
		if (new_iface) {
			interfaces->count--;
			interfaces->iface[interfaces->count] = NULL;
			hostapd_cleanup_unused_mlds(interfaces);
		}
		hostapd_cleanup_iface(hapd_iface);
	}
	return -1;
}


static int hostapd_remove_bss(struct hostapd_iface *iface, unsigned int idx)
{
	size_t i;

	wpa_printf(MSG_INFO, "Remove BSS '%s'", iface->conf->bss[idx]->iface);

	/* Remove hostapd_data only if it has already been initialized */
	if (idx < iface->num_bss) {
		struct hostapd_data *hapd = iface->bss[idx];

		hostapd_bss_deinit(hapd);
		wpa_printf(MSG_DEBUG, "%s: free hapd %p (%s)",
			   __func__, hapd, hapd->conf->iface);
		hostapd_config_free_bss(hapd->conf);
		hapd->conf = NULL;
#ifdef CONFIG_IEEE80211BE
		hostapd_mld_ref_dec(hapd->mld);
#endif /* CONFIG_IEEE80211BE */
		os_free(hapd);

		iface->num_bss--;

		for (i = idx; i < iface->num_bss; i++)
			iface->bss[i] = iface->bss[i + 1];
	} else {
		hostapd_config_free_bss(iface->conf->bss[idx]);
		iface->conf->bss[idx] = NULL;
	}

	iface->conf->num_bss--;
	for (i = idx; i < iface->conf->num_bss; i++)
		iface->conf->bss[i] = iface->conf->bss[i + 1];

	return 0;
}


int hostapd_remove_iface(struct hapd_interfaces *interfaces, char *buf)
{
	struct hostapd_iface *hapd_iface;
	size_t i, j, k = 0;

	for (i = 0; i < interfaces->count; i++) {
		hapd_iface = interfaces->iface[i];
		if (hapd_iface == NULL)
			return -1;
		if (!os_strcmp(hapd_iface->conf->bss[0]->iface, buf)) {
			wpa_printf(MSG_INFO, "Remove interface '%s'", buf);
			hapd_iface->driver_ap_teardown =
				!!(hapd_iface->drv_flags &
				   WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);

			hostapd_interface_deinit_free(hapd_iface);
			k = i;
			while (k < (interfaces->count - 1)) {
				interfaces->iface[k] =
					interfaces->iface[k + 1];
				k++;
			}
			interfaces->count--;
			hostapd_cleanup_unused_mlds(interfaces);

			return 0;
		}

		for (j = 0; j < hapd_iface->conf->num_bss; j++) {
			if (!os_strcmp(hapd_iface->conf->bss[j]->iface, buf)) {
				hapd_iface->driver_ap_teardown =
					!(hapd_iface->drv_flags &
					  WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT);
				return hostapd_remove_bss(hapd_iface, j);
			}
		}
	}
	return -1;
}


/**
 * hostapd_new_assoc_sta - Notify that a new station associated with the AP
 * @hapd: Pointer to BSS data
 * @sta: Pointer to the associated STA data
 * @reassoc: 1 to indicate this was a re-association; 0 = first association
 *
 * This function will be called whenever a station associates with the AP. It
 * can be called from ieee802_11.c for drivers that export MLME to hostapd and
 * from drv_callbacks.c based on driver events for drivers that take care of
 * management frames (IEEE 802.11 authentication and association) internally.
 */
void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc)
{
	if (hapd->tkip_countermeasures) {
		hostapd_drv_sta_deauth(hapd, sta->addr,
				       WLAN_REASON_MICHAEL_MIC_FAILURE);
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    sta->mld_assoc_link_id != hapd->mld_link_id)
		return;
#endif /* CONFIG_IEEE80211BE */

	ap_sta_clear_disconnect_timeouts(hapd, sta);
	sta->post_csa_sa_query = 0;

#ifdef CONFIG_P2P
	if (sta->p2p_ie == NULL && !sta->no_p2p_set) {
		sta->no_p2p_set = 1;
		hapd->num_sta_no_p2p++;
		if (hapd->num_sta_no_p2p == 1)
			hostapd_p2p_non_p2p_sta_connected(hapd);
	}
#endif /* CONFIG_P2P */

	airtime_policy_new_sta(hapd, sta);

	/* Start accounting here, if IEEE 802.1X and WPA are not used.
	 * IEEE 802.1X/WPA code will start accounting after the station has
	 * been authorized. */
	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa && !hapd->conf->osen) {
		ap_sta_set_authorized(hapd, sta, 1);
		os_get_reltime(&sta->connected_time);
		accounting_sta_start(hapd, sta);
	}

	/* Start IEEE 802.1X authentication process for new stations */
	ieee802_1x_new_station(hapd, sta);
	if (reassoc) {
		if (sta->auth_alg != WLAN_AUTH_FT &&
		    sta->auth_alg != WLAN_AUTH_FILS_SK &&
		    sta->auth_alg != WLAN_AUTH_FILS_SK_PFS &&
		    sta->auth_alg != WLAN_AUTH_FILS_PK &&
		    !(sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS)))
			wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
	} else if (!(hapd->iface->drv_flags2 &
		     WPA_DRIVER_FLAGS2_4WAY_HANDSHAKE_AP_PSK)) {
		/* The 4-way handshake offloaded case will have this handled
		 * based on the port authorized event. */
		wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm);
	}

	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_WIRED) {
		if (eloop_cancel_timeout(ap_handle_timer, hapd, sta) > 0) {
			wpa_printf(MSG_DEBUG,
				   "%s: %s: canceled wired ap_handle_timer timeout for "
				   MACSTR,
				   hapd->conf->iface, __func__,
				   MAC2STR(sta->addr));
		}
	} else if (!(hapd->iface->drv_flags &
		     WPA_DRIVER_FLAGS_INACTIVITY_TIMER)) {
		wpa_printf(MSG_DEBUG,
			   "%s: %s: reschedule ap_handle_timer timeout for "
			   MACSTR " (%d seconds - ap_max_inactivity)",
			   hapd->conf->iface, __func__, MAC2STR(sta->addr),
			   hapd->conf->ap_max_inactivity);
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		eloop_register_timeout(hapd->conf->ap_max_inactivity, 0,
				       ap_handle_timer, hapd, sta);
	}

#ifdef CONFIG_MACSEC
	if (hapd->conf->wpa_key_mgmt == WPA_KEY_MGMT_NONE &&
	    hapd->conf->mka_psk_set)
		ieee802_1x_create_preshared_mka_hapd(hapd, sta);
	else
		ieee802_1x_alloc_kay_sm_hapd(hapd, sta);
#endif /* CONFIG_MACSEC */
}


const char * hostapd_state_text(enum hostapd_iface_state s)
{
	switch (s) {
	case HAPD_IFACE_UNINITIALIZED:
		return "UNINITIALIZED";
	case HAPD_IFACE_DISABLED:
		return "DISABLED";
	case HAPD_IFACE_COUNTRY_UPDATE:
		return "COUNTRY_UPDATE";
	case HAPD_IFACE_ACS:
		return "ACS";
	case HAPD_IFACE_HT_SCAN:
		return "HT_SCAN";
	case HAPD_IFACE_DFS:
		return "DFS";
	case HAPD_IFACE_ENABLED:
		return "ENABLED";
	case HAPD_IFACE_NO_IR:
		return "NO_IR";
	}

	return "UNKNOWN";
}


void hostapd_set_state(struct hostapd_iface *iface, enum hostapd_iface_state s)
{
	wpa_printf(MSG_INFO, "%s: interface state %s->%s",
		   iface->conf ? iface->conf->bss[0]->iface : "N/A",
		   hostapd_state_text(iface->state), hostapd_state_text(s));
	iface->state = s;
}


int hostapd_csa_in_progress(struct hostapd_iface *iface)
{
	unsigned int i;

	for (i = 0; i < iface->num_bss; i++)
		if (iface->bss[i]->csa_in_progress)
			return 1;
	return 0;
}


#ifdef NEED_AP_MLME

void free_beacon_data(struct beacon_data *beacon)
{
	os_free(beacon->head);
	beacon->head = NULL;
	os_free(beacon->tail);
	beacon->tail = NULL;
	os_free(beacon->probe_resp);
	beacon->probe_resp = NULL;
	os_free(beacon->beacon_ies);
	beacon->beacon_ies = NULL;
	os_free(beacon->proberesp_ies);
	beacon->proberesp_ies = NULL;
	os_free(beacon->assocresp_ies);
	beacon->assocresp_ies = NULL;
}


static int hostapd_build_beacon_data(struct hostapd_data *hapd,
				     struct beacon_data *beacon)
{
	struct wpabuf *beacon_extra, *proberesp_extra, *assocresp_extra;
	struct wpa_driver_ap_params params;
	int ret;

	os_memset(beacon, 0, sizeof(*beacon));
	ret = ieee802_11_build_ap_params(hapd, &params);
	if (ret < 0)
		return ret;

	ret = hostapd_build_ap_extra_ies(hapd, &beacon_extra,
					 &proberesp_extra,
					 &assocresp_extra);
	if (ret)
		goto free_ap_params;

	ret = -1;
	beacon->head = os_memdup(params.head, params.head_len);
	if (!beacon->head)
		goto free_ap_extra_ies;

	beacon->head_len = params.head_len;

	beacon->tail = os_memdup(params.tail, params.tail_len);
	if (!beacon->tail)
		goto free_beacon;

	beacon->tail_len = params.tail_len;

	if (params.proberesp != NULL) {
		beacon->probe_resp = os_memdup(params.proberesp,
					       params.proberesp_len);
		if (!beacon->probe_resp)
			goto free_beacon;

		beacon->probe_resp_len = params.proberesp_len;
	}

	/* copy the extra ies */
	if (beacon_extra) {
		beacon->beacon_ies = os_memdup(beacon_extra->buf,
					       wpabuf_len(beacon_extra));
		if (!beacon->beacon_ies)
			goto free_beacon;

		beacon->beacon_ies_len = wpabuf_len(beacon_extra);
	}

	if (proberesp_extra) {
		beacon->proberesp_ies = os_memdup(proberesp_extra->buf,
						  wpabuf_len(proberesp_extra));
		if (!beacon->proberesp_ies)
			goto free_beacon;

		beacon->proberesp_ies_len = wpabuf_len(proberesp_extra);
	}

	if (assocresp_extra) {
		beacon->assocresp_ies = os_memdup(assocresp_extra->buf,
						  wpabuf_len(assocresp_extra));
		if (!beacon->assocresp_ies)
			goto free_beacon;

		beacon->assocresp_ies_len = wpabuf_len(assocresp_extra);
	}

	ret = 0;
free_beacon:
	/* if the function fails, the caller should not free beacon data */
	if (ret)
		free_beacon_data(beacon);

free_ap_extra_ies:
	hostapd_free_ap_extra_ies(hapd, beacon_extra, proberesp_extra,
				  assocresp_extra);
free_ap_params:
	ieee802_11_free_ap_params(&params);
	return ret;
}


/*
 * TODO: This flow currently supports only changing channel and width within
 * the same hw_mode. Any other changes to MAC parameters or provided settings
 * are not supported.
 */
static int hostapd_change_config_freq(struct hostapd_data *hapd,
				      struct hostapd_config *conf,
				      struct hostapd_freq_params *params,
				      struct hostapd_freq_params *old_params)
{
	int channel;
	u8 seg0 = 0, seg1 = 0;
	struct hostapd_hw_modes *mode;

	if (!params->channel) {
		/* check if the new channel is supported by hw */
		params->channel = hostapd_hw_get_channel(hapd, params->freq);
	}

	channel = params->channel;
	if (!channel)
		return -1;

	hostapd_determine_mode(hapd->iface);
	mode = hapd->iface->current_mode;

	/* if a pointer to old_params is provided we save previous state */
	if (old_params &&
	    hostapd_set_freq_params(old_params, conf->hw_mode,
				    hostapd_hw_get_freq(hapd, conf->channel),
				    conf->channel, conf->enable_edmg,
				    conf->edmg_channel, conf->ieee80211n,
				    conf->ieee80211ac, conf->ieee80211ax,
				    conf->ieee80211be, conf->secondary_channel,
				    hostapd_get_oper_chwidth(conf),
				    hostapd_get_oper_centr_freq_seg0_idx(conf),
				    hostapd_get_oper_centr_freq_seg1_idx(conf),
				    conf->vht_capab,
				    mode ? &mode->he_capab[IEEE80211_MODE_AP] :
				    NULL,
				    mode ? &mode->eht_capab[IEEE80211_MODE_AP] :
				    NULL,
				    hostapd_get_punct_bitmap(hapd)))
		return -1;

	switch (params->bandwidth) {
	case 0:
	case 20:
		conf->ht_capab &= ~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		break;
	case 40:
	case 80:
	case 160:
	case 320:
		conf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		break;
	default:
		return -1;
	}

	switch (params->bandwidth) {
	case 0:
	case 20:
	case 40:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_USE_HT);
		break;
	case 80:
		if (params->center_freq2)
			hostapd_set_oper_chwidth(conf,
						 CONF_OPER_CHWIDTH_80P80MHZ);
		else
			hostapd_set_oper_chwidth(conf,
						 CONF_OPER_CHWIDTH_80MHZ);
		break;
	case 160:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_160MHZ);
		break;
	case 320:
		hostapd_set_oper_chwidth(conf, CONF_OPER_CHWIDTH_320MHZ);
		break;
	default:
		return -1;
	}

	conf->channel = channel;
	conf->ieee80211n = params->ht_enabled;
	conf->ieee80211ac = params->vht_enabled;
	conf->secondary_channel = params->sec_channel_offset;
	if (params->center_freq1 &&
	    ieee80211_freq_to_chan(params->center_freq1, &seg0) ==
	    NUM_HOSTAPD_MODES)
		return -1;
	if (params->center_freq2 &&
	    ieee80211_freq_to_chan(params->center_freq2,
				   &seg1) == NUM_HOSTAPD_MODES)
		return -1;
	hostapd_set_oper_centr_freq_seg0_idx(conf, seg0);
	hostapd_set_oper_centr_freq_seg1_idx(conf, seg1);

	/* TODO: maybe call here hostapd_config_check here? */

	return 0;
}


static int hostapd_fill_csa_settings(struct hostapd_data *hapd,
				     struct csa_settings *settings)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_freq_params old_freq;
	int ret;
#ifdef CONFIG_IEEE80211BE
	u16 old_punct_bitmap;
#endif /* CONFIG_IEEE80211BE */
	u8 chan, bandwidth;

	os_memset(&old_freq, 0, sizeof(old_freq));
	if (!iface || !iface->freq || hapd->csa_in_progress)
		return -1;

	switch (settings->freq_params.bandwidth) {
	case 80:
		if (settings->freq_params.center_freq2)
			bandwidth = CONF_OPER_CHWIDTH_80P80MHZ;
		else
			bandwidth = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case 160:
		bandwidth = CONF_OPER_CHWIDTH_160MHZ;
		break;
	case 320:
		bandwidth = CONF_OPER_CHWIDTH_320MHZ;
		break;
	default:
		bandwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	}

	if (ieee80211_freq_to_channel_ext(
		    settings->freq_params.freq,
		    settings->freq_params.sec_channel_offset,
		    bandwidth,
		    &hapd->iface->cs_oper_class,
		    &chan) == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG,
			   "invalid frequency for channel switch (freq=%d, sec_channel_offset=%d, vht_enabled=%d, he_enabled=%d, eht_enabled=%d)",
			   settings->freq_params.freq,
			   settings->freq_params.sec_channel_offset,
			   settings->freq_params.vht_enabled,
			   settings->freq_params.he_enabled,
			   settings->freq_params.eht_enabled);
		return -1;
	}

	settings->freq_params.channel = chan;

	ret = hostapd_change_config_freq(iface->bss[0], iface->conf,
					 &settings->freq_params,
					 &old_freq);
	if (ret)
		return ret;

#ifdef CONFIG_IEEE80211BE
	old_punct_bitmap = iface->conf->punct_bitmap;
	iface->conf->punct_bitmap = settings->punct_bitmap;
#endif /* CONFIG_IEEE80211BE */
	ret = hostapd_build_beacon_data(hapd, &settings->beacon_after);

	/* change back the configuration */
#ifdef CONFIG_IEEE80211BE
	iface->conf->punct_bitmap = old_punct_bitmap;
#endif /* CONFIG_IEEE80211BE */
	hostapd_change_config_freq(iface->bss[0], iface->conf,
				   &old_freq, NULL);

	if (ret)
		return ret;

	/* set channel switch parameters for csa ie */
	hapd->cs_freq_params = settings->freq_params;
	hapd->cs_count = settings->cs_count;
	hapd->cs_block_tx = settings->block_tx;

	ret = hostapd_build_beacon_data(hapd, &settings->beacon_csa);
	if (ret) {
		free_beacon_data(&settings->beacon_after);
		return ret;
	}

	settings->counter_offset_beacon[0] = hapd->cs_c_off_beacon;
	settings->counter_offset_presp[0] = hapd->cs_c_off_proberesp;
	settings->counter_offset_beacon[1] = hapd->cs_c_off_ecsa_beacon;
	settings->counter_offset_presp[1] = hapd->cs_c_off_ecsa_proberesp;
	settings->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		settings->link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	settings->ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &settings->ubpr);
#endif /* CONFIG_IEEE80211AX */

	return 0;
}


void hostapd_cleanup_cs_params(struct hostapd_data *hapd)
{
	os_memset(&hapd->cs_freq_params, 0, sizeof(hapd->cs_freq_params));
	hapd->cs_count = 0;
	hapd->cs_block_tx = 0;
	hapd->cs_c_off_beacon = 0;
	hapd->cs_c_off_proberesp = 0;
	hapd->csa_in_progress = 0;
	hapd->cs_c_off_ecsa_beacon = 0;
	hapd->cs_c_off_ecsa_proberesp = 0;
}


void hostapd_chan_switch_config(struct hostapd_data *hapd,
				struct hostapd_freq_params *freq_params)
{
	if (freq_params->eht_enabled)
		hapd->iconf->ch_switch_eht_config |= CH_SWITCH_EHT_ENABLED;
	else
		hapd->iconf->ch_switch_eht_config |= CH_SWITCH_EHT_DISABLED;

	if (freq_params->he_enabled)
		hapd->iconf->ch_switch_he_config |= CH_SWITCH_HE_ENABLED;
	else
		hapd->iconf->ch_switch_he_config |= CH_SWITCH_HE_DISABLED;

	if (freq_params->vht_enabled)
		hapd->iconf->ch_switch_vht_config |= CH_SWITCH_VHT_ENABLED;
	else
		hapd->iconf->ch_switch_vht_config |= CH_SWITCH_VHT_DISABLED;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "CHAN_SWITCH EHT config 0x%x HE config 0x%x VHT config 0x%x",
		       hapd->iconf->ch_switch_eht_config,
		       hapd->iconf->ch_switch_he_config,
		       hapd->iconf->ch_switch_vht_config);
}


int hostapd_switch_channel(struct hostapd_data *hapd,
			   struct csa_settings *settings)
{
	int ret;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA)) {
		wpa_printf(MSG_INFO, "CSA is not supported");
		return -1;
	}

	ret = hostapd_fill_csa_settings(hapd, settings);
	if (ret)
		return ret;

	ret = hostapd_drv_switch_channel(hapd, settings);
	free_beacon_data(&settings->beacon_csa);
	free_beacon_data(&settings->beacon_after);
#ifdef CONFIG_IEEE80211AX
	os_free(settings->ubpr.unsol_bcast_probe_resp_tmpl);
#endif /* CONFIG_IEEE80211AX */

	if (ret) {
		/* if we failed, clean cs parameters */
		hostapd_cleanup_cs_params(hapd);
		return ret;
	}

	hapd->csa_in_progress = 1;
	return 0;
}


void
hostapd_switch_channel_fallback(struct hostapd_iface *iface,
				const struct hostapd_freq_params *freq_params)
{
	u8 seg0_idx = 0, seg1_idx = 0;
	enum oper_chan_width bw = CONF_OPER_CHWIDTH_USE_HT;
	u8 op_class, chan = 0;

	wpa_printf(MSG_DEBUG, "Restarting all CSA-related BSSes");

	if (freq_params->center_freq1)
		ieee80211_freq_to_chan(freq_params->center_freq1, &seg0_idx);
	if (freq_params->center_freq2)
		ieee80211_freq_to_chan(freq_params->center_freq2, &seg1_idx);

	switch (freq_params->bandwidth) {
	case 0:
	case 20:
	case 40:
		bw = CONF_OPER_CHWIDTH_USE_HT;
		break;
	case 80:
		if (freq_params->center_freq2) {
			bw = CONF_OPER_CHWIDTH_80P80MHZ;
			iface->conf->vht_capab |=
				VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
		} else {
			bw = CONF_OPER_CHWIDTH_80MHZ;
		}
		break;
	case 160:
		bw = CONF_OPER_CHWIDTH_160MHZ;
		iface->conf->vht_capab |= VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
		break;
	case 320:
		bw = CONF_OPER_CHWIDTH_320MHZ;
		break;
	default:
		wpa_printf(MSG_WARNING, "Unknown CSA bandwidth: %d",
			   freq_params->bandwidth);
		break;
	}

	iface->freq = freq_params->freq;
	iface->conf->channel = freq_params->channel;
	iface->conf->secondary_channel = freq_params->sec_channel_offset;
	if (ieee80211_freq_to_channel_ext(freq_params->freq,
					  freq_params->sec_channel_offset, bw,
					  &op_class, &chan) ==
	    NUM_HOSTAPD_MODES ||
	    chan != freq_params->channel)
		wpa_printf(MSG_INFO, "CSA: Channel mismatch: %d -> %d",
			   freq_params->channel, chan);

	iface->conf->op_class = op_class;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, seg1_idx);
	hostapd_set_oper_chwidth(iface->conf, bw);
	iface->conf->ieee80211n = freq_params->ht_enabled;
	iface->conf->ieee80211ac = freq_params->vht_enabled;
	iface->conf->ieee80211ax = freq_params->he_enabled;
	iface->conf->ieee80211be = freq_params->eht_enabled;

	/*
	 * cs_params must not be cleared earlier because the freq_params
	 * argument may actually point to one of these.
	 * These params will be cleared during interface disable below.
	 */
	hostapd_disable_iface(iface);
	hostapd_enable_iface(iface);
}


#ifdef CONFIG_IEEE80211AX

void hostapd_cleanup_cca_params(struct hostapd_data *hapd)
{
	hapd->cca_count = 0;
	hapd->cca_color = 0;
	hapd->cca_c_off_beacon = 0;
	hapd->cca_c_off_proberesp = 0;
	hapd->cca_in_progress = false;
}


int hostapd_fill_cca_settings(struct hostapd_data *hapd,
			      struct cca_settings *settings)
{
	struct hostapd_iface *iface = hapd->iface;
	u8 old_color;
	int ret;

	if (!iface || iface->conf->he_op.he_bss_color_disabled)
		return -1;

	settings->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		settings->link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	old_color = iface->conf->he_op.he_bss_color;
	iface->conf->he_op.he_bss_color = hapd->cca_color;
	ret = hostapd_build_beacon_data(hapd, &settings->beacon_after);
	if (ret)
		return ret;

	iface->conf->he_op.he_bss_color = old_color;

	settings->cca_count = hapd->cca_count;
	settings->cca_color = hapd->cca_color,
	hapd->cca_in_progress = true;

	ret = hostapd_build_beacon_data(hapd, &settings->beacon_cca);
	if (ret) {
		free_beacon_data(&settings->beacon_after);
		return ret;
	}

	settings->ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &settings->ubpr);

	settings->counter_offset_beacon = hapd->cca_c_off_beacon;
	settings->counter_offset_presp = hapd->cca_c_off_proberesp;

	return 0;
}


static void hostapd_switch_color_timeout_handler(void *eloop_data,
						 void *user_ctx)
{
	struct hostapd_data *hapd = (struct hostapd_data *) eloop_data;
	os_time_t delta_t;
	unsigned int b;
	int i, r;

	 /* CCA can be triggered once the handler constantly receives
	  * color collision events to for at least
	  * DOT11BSS_COLOR_COLLISION_AP_PERIOD (50 s by default). */
	delta_t = hapd->last_color_collision.sec -
		hapd->first_color_collision.sec;
	if (delta_t < DOT11BSS_COLOR_COLLISION_AP_PERIOD)
		return;

	r = os_random() % HE_OPERATION_BSS_COLOR_MAX;
	for (i = 0; i < HE_OPERATION_BSS_COLOR_MAX; i++) {
		if (r && !(hapd->color_collision_bitmap & (1ULL << r)))
			break;

		r = (r + 1) % HE_OPERATION_BSS_COLOR_MAX;
	}

	if (i == HE_OPERATION_BSS_COLOR_MAX) {
		/* There are no free colors so turn BSS coloring off */
		wpa_printf(MSG_INFO,
			   "No free colors left, turning off BSS coloring");
		hapd->iface->conf->he_op.he_bss_color_disabled = 1;
		hapd->iface->conf->he_op.he_bss_color = os_random() % 63 + 1;
		for (b = 0; b < hapd->iface->num_bss; b++)
			ieee802_11_set_beacon(hapd->iface->bss[b]);
		return;
	}

	for (b = 0; b < hapd->iface->num_bss; b++) {
		struct hostapd_data *bss = hapd->iface->bss[b];
		struct cca_settings settings;
		int ret;

		hostapd_cleanup_cca_params(bss);
		bss->cca_color = r;
		bss->cca_count = 10;

		if (hostapd_fill_cca_settings(bss, &settings)) {
			hostapd_cleanup_cca_params(bss);
			continue;
		}

		ret = hostapd_drv_switch_color(bss, &settings);
		if (ret)
			hostapd_cleanup_cca_params(bss);

		free_beacon_data(&settings.beacon_cca);
		free_beacon_data(&settings.beacon_after);
		os_free(settings.ubpr.unsol_bcast_probe_resp_tmpl);
	}
}


void hostapd_switch_color(struct hostapd_data *hapd, u64 bitmap)
{
	struct os_reltime now;

	if (hapd->cca_in_progress)
		return;

	if (os_get_reltime(&now))
		return;

	hapd->color_collision_bitmap = bitmap;
	hapd->last_color_collision = now;

	if (eloop_is_timeout_registered(hostapd_switch_color_timeout_handler,
					hapd, NULL))
		return;

	hapd->first_color_collision = now;
	/* 10 s window as margin for persistent color collision reporting */
	eloop_register_timeout(DOT11BSS_COLOR_COLLISION_AP_PERIOD + 10, 0,
			       hostapd_switch_color_timeout_handler,
			       hapd, NULL);
}

#endif /* CONFIG_IEEE80211AX */

#endif /* NEED_AP_MLME */


struct hostapd_data * hostapd_get_iface(struct hapd_interfaces *interfaces,
					const char *ifname)
{
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd = iface->bss[j];

			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				return hapd;
		}
	}

	return NULL;
}


void hostapd_periodic_iface(struct hostapd_iface *iface)
{
	size_t i;

	ap_list_timer(iface);

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *hapd = iface->bss[i];

		if (!hapd->started)
			continue;

#ifndef CONFIG_NO_RADIUS
		hostapd_acl_expire(hapd);
#endif /* CONFIG_NO_RADIUS */
	}
}


#ifdef CONFIG_OCV
void hostapd_ocv_check_csa_sa_query(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "OCV: Post-CSA SA Query initiation check");

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (!sta->post_csa_sa_query)
			continue;

		wpa_printf(MSG_DEBUG, "OCV: OCVC STA " MACSTR
			   " did not start SA Query after CSA - disconnect",
			   MAC2STR(sta->addr));
		ap_sta_disconnect(hapd, sta, sta->addr,
				  WLAN_REASON_PREV_AUTH_NOT_VALID);
	}
}
#endif /* CONFIG_OCV */


#ifdef CONFIG_IEEE80211BE

struct hostapd_data * hostapd_mld_get_link_bss(struct hostapd_data *hapd,
					       u8 link_id)
{
	struct hostapd_iface *iface;
	struct hostapd_data *bss;
	unsigned int i, j;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];
		if (!iface)
			continue;

		for (j = 0; j < iface->num_bss; j++) {
			bss = iface->bss[j];

			if (!bss->conf->mld_ap ||
			    !hostapd_is_ml_partner(hapd, bss))
				continue;

			if (!bss->drv_priv)
				continue;

			if (bss->mld_link_id == link_id)
				return bss;
		}
	}

	return NULL;
}


bool hostapd_is_ml_partner(struct hostapd_data *hapd1,
			   struct hostapd_data *hapd2)
{
	if (!hapd1->conf->mld_ap || !hapd2->conf->mld_ap)
		return false;

	return !os_strcmp(hapd1->conf->iface, hapd2->conf->iface);
}


u8 hostapd_get_mld_id(struct hostapd_data *hapd)
{
	if (!hapd->conf->mld_ap)
		return 255;

	/* MLD ID 0 represents self */
	return 0;

	/* TODO: MLD ID for Multiple BSS cases */
}


int hostapd_mld_add_link(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return 0;

	/* Should not happen */
	if (!mld)
		return -1;

	dl_list_add_tail(&mld->links, &hapd->link);
	mld->num_links++;

	wpa_printf(MSG_DEBUG, "AP MLD %s: Link ID %d added. num_links: %d",
		   mld->name, hapd->mld_link_id, mld->num_links);

	if (mld->fbss)
		return 0;

	mld->fbss = hapd;
	wpa_printf(MSG_DEBUG, "AP MLD %s: First link BSS set to %p",
		   mld->name, mld->fbss);
	return 0;
}


int hostapd_mld_remove_link(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;
	struct hostapd_data *next_fbss;

	if (!hapd->conf->mld_ap)
		return 0;

	/* Should not happen */
	if (!mld)
		return -1;

	dl_list_del(&hapd->link);
	mld->num_links--;

	wpa_printf(MSG_DEBUG, "AP MLD %s: Link ID %d removed. num_links: %d",
		   mld->name, hapd->mld_link_id, mld->num_links);

	if (mld->fbss != hapd)
		return 0;

	/* If the list is empty, all links are removed */
	if (dl_list_empty(&mld->links)) {
		mld->fbss = NULL;
	} else {
		next_fbss = dl_list_entry(mld->links.next, struct hostapd_data,
					  link);
		mld->fbss = next_fbss;
	}

	wpa_printf(MSG_DEBUG, "AP MLD %s: First link BSS set to %p",
		   mld->name, mld->fbss);
	return 0;
}


bool hostapd_mld_is_first_bss(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return true;

	/* Should not happen */
	if (!mld)
		return false;

	/* If fbss is not set, it is safe to assume the caller is the first BSS.
	 */
	if (!mld->fbss)
		return true;

	return hapd == mld->fbss;
}


struct hostapd_data * hostapd_mld_get_first_bss(struct hostapd_data *hapd)
{
	struct hostapd_mld *mld = hapd->mld;

	if (!hapd->conf->mld_ap)
		return NULL;

	/* Should not happen */
	if (!mld)
		return NULL;

	return mld->fbss;
}

#endif /* CONFIG_IEEE80211BE */


u16 hostapd_get_punct_bitmap(struct hostapd_data *hapd)
{
	u16 punct_bitmap = 0;

#ifdef CONFIG_IEEE80211BE
	punct_bitmap = hapd->iconf->punct_bitmap;
#ifdef CONFIG_TESTING_OPTIONS
	if (!punct_bitmap)
		punct_bitmap = hapd->conf->eht_oper_puncturing_override;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */

	return punct_bitmap;
}
