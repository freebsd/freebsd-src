/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
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
#ifndef CONFIG_NATIVE_WINDOWS
#include <syslog.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "eloop.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "beacon.h"
#include "hw_features.h"
#include "accounting.h"
#include "eapol_sm.h"
#include "iapp.h"
#include "ap.h"
#include "ieee802_11_auth.h"
#include "ap_list.h"
#include "sta_info.h"
#include "driver.h"
#include "radius/radius_client.h"
#include "radius/radius_server.h"
#include "wpa.h"
#include "preauth.h"
#include "wme.h"
#include "vlan_init.h"
#include "ctrl_iface.h"
#include "tls.h"
#include "eap_server/eap_sim_db.h"
#include "eap_server/eap.h"
#include "eap_server/tncs.h"
#include "version.h"
#include "l2_packet/l2_packet.h"
#include "wps_hostapd.h"


static int hostapd_radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user);
static int hostapd_flush_old_stations(struct hostapd_data *hapd);
static int hostapd_setup_wpa(struct hostapd_data *hapd);
static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd);

struct hapd_interfaces {
	size_t count;
	struct hostapd_iface **iface;
};

unsigned char rfc1042_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };


extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;


static void hostapd_logger_cb(void *ctx, const u8 *addr, unsigned int module,
			      int level, const char *txt, size_t len)
{
	struct hostapd_data *hapd = ctx;
	char *format, *module_str;
	int maxlen;
	int conf_syslog_level, conf_stdout_level;
	unsigned int conf_syslog, conf_stdout;

	maxlen = len + 100;
	format = os_malloc(maxlen);
	if (!format)
		return;

	if (hapd && hapd->conf) {
		conf_syslog_level = hapd->conf->logger_syslog_level;
		conf_stdout_level = hapd->conf->logger_stdout_level;
		conf_syslog = hapd->conf->logger_syslog;
		conf_stdout = hapd->conf->logger_stdout;
	} else {
		conf_syslog_level = conf_stdout_level = 0;
		conf_syslog = conf_stdout = (unsigned int) -1;
	}

	switch (module) {
	case HOSTAPD_MODULE_IEEE80211:
		module_str = "IEEE 802.11";
		break;
	case HOSTAPD_MODULE_IEEE8021X:
		module_str = "IEEE 802.1X";
		break;
	case HOSTAPD_MODULE_RADIUS:
		module_str = "RADIUS";
		break;
	case HOSTAPD_MODULE_WPA:
		module_str = "WPA";
		break;
	case HOSTAPD_MODULE_DRIVER:
		module_str = "DRIVER";
		break;
	case HOSTAPD_MODULE_IAPP:
		module_str = "IAPP";
		break;
	case HOSTAPD_MODULE_MLME:
		module_str = "MLME";
		break;
	default:
		module_str = NULL;
		break;
	}

	if (hapd && hapd->conf && addr)
		os_snprintf(format, maxlen, "%s: STA " MACSTR "%s%s: %s",
			    hapd->conf->iface, MAC2STR(addr),
			    module_str ? " " : "", module_str, txt);
	else if (hapd && hapd->conf)
		os_snprintf(format, maxlen, "%s:%s%s %s",
			    hapd->conf->iface, module_str ? " " : "",
			    module_str, txt);
	else if (addr)
		os_snprintf(format, maxlen, "STA " MACSTR "%s%s: %s",
			    MAC2STR(addr), module_str ? " " : "",
			    module_str, txt);
	else
		os_snprintf(format, maxlen, "%s%s%s",
			    module_str, module_str ? ": " : "", txt);

	if ((conf_stdout & module) && level >= conf_stdout_level) {
		wpa_debug_print_timestamp();
		printf("%s\n", format);
	}

#ifndef CONFIG_NATIVE_WINDOWS
	if ((conf_syslog & module) && level >= conf_syslog_level) {
		int priority;
		switch (level) {
		case HOSTAPD_LEVEL_DEBUG_VERBOSE:
		case HOSTAPD_LEVEL_DEBUG:
			priority = LOG_DEBUG;
			break;
		case HOSTAPD_LEVEL_INFO:
			priority = LOG_INFO;
			break;
		case HOSTAPD_LEVEL_NOTICE:
			priority = LOG_NOTICE;
			break;
		case HOSTAPD_LEVEL_WARNING:
			priority = LOG_WARNING;
			break;
		default:
			priority = LOG_INFO;
			break;
		}
		syslog(priority, "%s", format);
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	os_free(format);
}


static void hostapd_deauth_all_stas(struct hostapd_data *hapd)
{
	u8 addr[ETH_ALEN];

	/* New Prism2.5/3 STA firmware versions seem to have issues with this
	 * broadcast deauth frame. This gets the firmware in odd state where
	 * nothing works correctly, so let's skip sending this for the hostap
	 * driver. */

	if (hapd->driver && os_strcmp(hapd->driver->name, "hostap") != 0) {
		os_memset(addr, 0xff, ETH_ALEN);
		hostapd_sta_deauth(hapd, addr,
				   WLAN_REASON_PREV_AUTH_NOT_VALID);
	}
}


/**
 * hostapd_prune_associations - Remove extraneous associations
 * @hapd: Pointer to BSS data for the most recent association
 * @sta: Pointer to the associated STA data
 *
 * This function looks through all radios and BSS's for previous
 * (stale) associations of STA. If any are found they are removed.
 */
static void hostapd_prune_associations(struct hostapd_data *hapd,
				       struct sta_info *sta)
{
	struct sta_info *osta;
	struct hostapd_data *ohapd;
	size_t i, j;
	struct hapd_interfaces *interfaces = eloop_get_user_data();

	for (i = 0; i < interfaces->count; i++) {
		for (j = 0; j < interfaces->iface[i]->num_bss; j++) {
			ohapd = interfaces->iface[i]->bss[j];
			if (ohapd == hapd)
				continue;
			osta = ap_get_sta(ohapd, sta->addr);
			if (!osta)
				continue;

			ap_sta_disassociate(ohapd, osta,
					    WLAN_REASON_UNSPECIFIED);
		}
	}
}


/**
 * hostapd_new_assoc_sta - Notify that a new station associated with the AP
 * @hapd: Pointer to BSS data
 * @sta: Pointer to the associated STA data
 * @reassoc: 1 to indicate this was a re-association; 0 = first association
 *
 * This function will be called whenever a station associates with the AP. It
 * can be called for ieee802_11.c for drivers that export MLME to hostapd and
 * from driver_*.c for drivers that take care of management frames (IEEE 802.11
 * authentication and association) internally.
 */
void hostapd_new_assoc_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc)
{
	if (hapd->tkip_countermeasures) {
		hostapd_sta_deauth(hapd, sta->addr,
				   WLAN_REASON_MICHAEL_MIC_FAILURE);
		return;
	}

	hostapd_prune_associations(hapd, sta);

	/* IEEE 802.11F (IAPP) */
	if (hapd->conf->ieee802_11f)
		iapp_new_station(hapd->iapp, sta);

	/* Start accounting here, if IEEE 802.1X and WPA are not used.
	 * IEEE 802.1X/WPA code will start accounting after the station has
	 * been authorized. */
	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa)
		accounting_sta_start(hapd, sta);

	hostapd_wmm_sta_config(hapd, sta);

	/* Start IEEE 802.1X authentication process for new stations */
	ieee802_1x_new_station(hapd, sta);
	if (reassoc) {
		if (sta->auth_alg != WLAN_AUTH_FT &&
		    !(sta->flags & (WLAN_STA_WPS | WLAN_STA_MAYBE_WPS)))
			wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
	} else
		wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm);
}


#ifdef EAP_SERVER
static int hostapd_sim_db_cb_sta(struct hostapd_data *hapd,
				 struct sta_info *sta, void *ctx)
{
	if (eapol_auth_eap_pending_cb(sta->eapol_sm, ctx) == 0)
		return 1;
	return 0;
}


static void hostapd_sim_db_cb(void *ctx, void *session_ctx)
{
	struct hostapd_data *hapd = ctx;
	if (ap_for_each_sta(hapd, hostapd_sim_db_cb_sta, session_ctx) == 0)
		radius_server_eap_pending_cb(hapd->radius_srv, session_ctx);
}
#endif /* EAP_SERVER */


/**
 * handle_term - SIGINT and SIGTERM handler to terminate hostapd process
 */
static void handle_term(int sig, void *eloop_ctx, void *signal_ctx)
{
	wpa_printf(MSG_DEBUG, "Signal %d received - terminating", sig);
	eloop_terminate();
}


static void hostapd_wpa_auth_conf(struct hostapd_bss_config *conf,
				  struct wpa_auth_config *wconf)
{
	wconf->wpa = conf->wpa;
	wconf->wpa_key_mgmt = conf->wpa_key_mgmt;
	wconf->wpa_pairwise = conf->wpa_pairwise;
	wconf->wpa_group = conf->wpa_group;
	wconf->wpa_group_rekey = conf->wpa_group_rekey;
	wconf->wpa_strict_rekey = conf->wpa_strict_rekey;
	wconf->wpa_gmk_rekey = conf->wpa_gmk_rekey;
	wconf->wpa_ptk_rekey = conf->wpa_ptk_rekey;
	wconf->rsn_pairwise = conf->rsn_pairwise;
	wconf->rsn_preauth = conf->rsn_preauth;
	wconf->eapol_version = conf->eapol_version;
	wconf->peerkey = conf->peerkey;
	wconf->wmm_enabled = conf->wmm_enabled;
	wconf->okc = conf->okc;
#ifdef CONFIG_IEEE80211W
	wconf->ieee80211w = conf->ieee80211w;
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_IEEE80211R
	wconf->ssid_len = conf->ssid.ssid_len;
	if (wconf->ssid_len > SSID_LEN)
		wconf->ssid_len = SSID_LEN;
	os_memcpy(wconf->ssid, conf->ssid.ssid, wconf->ssid_len);
	os_memcpy(wconf->mobility_domain, conf->mobility_domain,
		  MOBILITY_DOMAIN_ID_LEN);
	if (conf->nas_identifier &&
	    os_strlen(conf->nas_identifier) <= FT_R0KH_ID_MAX_LEN) {
		wconf->r0_key_holder_len = os_strlen(conf->nas_identifier);
		os_memcpy(wconf->r0_key_holder, conf->nas_identifier,
			  wconf->r0_key_holder_len);
	}
	os_memcpy(wconf->r1_key_holder, conf->r1_key_holder, FT_R1KH_ID_LEN);
	wconf->r0_key_lifetime = conf->r0_key_lifetime;
	wconf->reassociation_deadline = conf->reassociation_deadline;
	wconf->r0kh_list = conf->r0kh_list;
	wconf->r1kh_list = conf->r1kh_list;
	wconf->pmk_r1_push = conf->pmk_r1_push;
#endif /* CONFIG_IEEE80211R */
}


int hostapd_reload_config(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_config *newconf, *oldconf;
	struct wpa_auth_config wpa_auth_conf;
	size_t j;

	newconf = hostapd_config_read(iface->config_fname);
	if (newconf == NULL)
		return -1;

	/*
	 * Deauthenticate all stations since the new configuration may not
	 * allow them to use the BSS anymore.
	 */
	for (j = 0; j < iface->num_bss; j++)
		hostapd_flush_old_stations(iface->bss[j]);

	/* TODO: update dynamic data based on changed configuration
	 * items (e.g., open/close sockets, etc.) */
	radius_client_flush(hapd->radius, 0);

	oldconf = hapd->iconf;
	hapd->iconf = newconf;
	hapd->conf = &newconf->bss[0];
	iface->conf = newconf;

	if (hostapd_setup_wpa_psk(hapd->conf)) {
		wpa_printf(MSG_ERROR, "Failed to re-configure WPA PSK "
			   "after reloading configuration");
	}

	if (hapd->conf->wpa && hapd->wpa_auth == NULL)
		hostapd_setup_wpa(hapd);
	else if (hapd->conf->wpa) {
		hostapd_wpa_auth_conf(&newconf->bss[0], &wpa_auth_conf);
		wpa_reconfig(hapd->wpa_auth, &wpa_auth_conf);
	} else if (hapd->wpa_auth) {
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;
		hostapd_set_privacy(hapd, 0);
		hostapd_setup_encryption(hapd->conf->iface, hapd);
	}

	ieee802_11_set_beacon(hapd);

	if (hapd->conf->ssid.ssid_set &&
	    hostapd_set_ssid(hapd, (u8 *) hapd->conf->ssid.ssid,
			     hapd->conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		/* try to continue */
	}

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		hostapd_set_ieee8021x(hapd->conf->iface, hapd, 1);

	hostapd_config_free(oldconf);

	wpa_printf(MSG_DEBUG, "Reconfigured interface %s", hapd->conf->iface);

	return 0;
}


#ifndef CONFIG_NATIVE_WINDOWS
/**
 * handle_reload - SIGHUP handler to reload configuration
 */
static void handle_reload(int sig, void *eloop_ctx, void *signal_ctx)
{
	struct hapd_interfaces *hapds = (struct hapd_interfaces *) eloop_ctx;
	size_t i;

	wpa_printf(MSG_DEBUG, "Signal %d received - reloading configuration",
		   sig);

	for (i = 0; i < hapds->count; i++) {
		if (hostapd_reload_config(hapds->iface[i]) < 0) {
			wpa_printf(MSG_WARNING, "Failed to read new "
				   "configuration file - continuing with "
				   "old.");
			continue;
		}
	}
}


#ifdef HOSTAPD_DUMP_STATE
/**
 * hostapd_dump_state - SIGUSR1 handler to dump hostapd state to a text file
 */
static void hostapd_dump_state(struct hostapd_data *hapd)
{
	FILE *f;
	time_t now;
	struct sta_info *sta;
	int i;
	char *buf;

	if (!hapd->conf->dump_log_name) {
		wpa_printf(MSG_DEBUG, "Dump file not defined - ignoring dump "
			   "request");
		return;
	}

	wpa_printf(MSG_DEBUG, "Dumping hostapd state to '%s'",
		   hapd->conf->dump_log_name);
	f = fopen(hapd->conf->dump_log_name, "w");
	if (f == NULL) {
		wpa_printf(MSG_WARNING, "Could not open dump file '%s' for "
			   "writing.", hapd->conf->dump_log_name);
		return;
	}

	time(&now);
	fprintf(f, "hostapd state dump - %s", ctime(&now));
	fprintf(f, "num_sta=%d num_sta_non_erp=%d "
		"num_sta_no_short_slot_time=%d\n"
		"num_sta_no_short_preamble=%d\n",
		hapd->num_sta, hapd->iface->num_sta_non_erp,
		hapd->iface->num_sta_no_short_slot_time,
		hapd->iface->num_sta_no_short_preamble);

	for (sta = hapd->sta_list; sta != NULL; sta = sta->next) {
		fprintf(f, "\nSTA=" MACSTR "\n", MAC2STR(sta->addr));

		fprintf(f,
			"  AID=%d flags=0x%x %s%s%s%s%s%s%s%s%s%s%s%s%s%s\n"
			"  capability=0x%x listen_interval=%d\n",
			sta->aid,
			sta->flags,
			(sta->flags & WLAN_STA_AUTH ? "[AUTH]" : ""),
			(sta->flags & WLAN_STA_ASSOC ? "[ASSOC]" : ""),
			(sta->flags & WLAN_STA_PS ? "[PS]" : ""),
			(sta->flags & WLAN_STA_TIM ? "[TIM]" : ""),
			(sta->flags & WLAN_STA_PERM ? "[PERM]" : ""),
			(sta->flags & WLAN_STA_AUTHORIZED ? "[AUTHORIZED]" :
			 ""),
			(sta->flags & WLAN_STA_PENDING_POLL ? "[PENDING_POLL" :
			 ""),
			(sta->flags & WLAN_STA_SHORT_PREAMBLE ?
			 "[SHORT_PREAMBLE]" : ""),
			(sta->flags & WLAN_STA_PREAUTH ? "[PREAUTH]" : ""),
			(sta->flags & WLAN_STA_WMM ? "[WMM]" : ""),
			(sta->flags & WLAN_STA_MFP ? "[MFP]" : ""),
			(sta->flags & WLAN_STA_WPS ? "[WPS]" : ""),
			(sta->flags & WLAN_STA_MAYBE_WPS ? "[MAYBE_WPS]" : ""),
			(sta->flags & WLAN_STA_NONERP ? "[NonERP]" : ""),
			sta->capability,
			sta->listen_interval);

		fprintf(f, "  supported_rates=");
		for (i = 0; i < sta->supported_rates_len; i++)
			fprintf(f, "%02x ", sta->supported_rates[i]);
		fprintf(f, "\n");

		fprintf(f,
			"  timeout_next=%s\n",
			(sta->timeout_next == STA_NULLFUNC ? "NULLFUNC POLL" :
			 (sta->timeout_next == STA_DISASSOC ? "DISASSOC" :
			  "DEAUTH")));

		ieee802_1x_dump_state(f, "  ", sta);
	}

	buf = os_malloc(4096);
	if (buf) {
		int count = radius_client_get_mib(hapd->radius, buf, 4096);
		if (count < 0)
			count = 0;
		else if (count > 4095)
			count = 4095;
		buf[count] = '\0';
		fprintf(f, "%s", buf);

		count = radius_server_get_mib(hapd->radius_srv, buf, 4096);
		if (count < 0)
			count = 0;
		else if (count > 4095)
			count = 4095;
		buf[count] = '\0';
		fprintf(f, "%s", buf);
		os_free(buf);
	}
	fclose(f);
}
#endif /* HOSTAPD_DUMP_STATE */


static void handle_dump_state(int sig, void *eloop_ctx, void *signal_ctx)
{
#ifdef HOSTAPD_DUMP_STATE
	struct hapd_interfaces *hapds = (struct hapd_interfaces *) eloop_ctx;
	size_t i, j;

	for (i = 0; i < hapds->count; i++) {
		struct hostapd_iface *hapd_iface = hapds->iface[i];
		for (j = 0; j < hapd_iface->num_bss; j++)
			hostapd_dump_state(hapd_iface->bss[j]);
	}
#endif /* HOSTAPD_DUMP_STATE */
}
#endif /* CONFIG_NATIVE_WINDOWS */

static void hostapd_broadcast_key_clear_iface(struct hostapd_data *hapd,
					      char *ifname)
{
	int i;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (hostapd_set_encryption(ifname, hapd, "none", NULL, i, NULL,
					   0, i == 0 ? 1 : 0)) {
			wpa_printf(MSG_DEBUG, "Failed to clear default "
				   "encryption keys (ifname=%s keyidx=%d)",
				   ifname, i);
		}
	}
#ifdef CONFIG_IEEE80211W
	if (hapd->conf->ieee80211w) {
		for (i = NUM_WEP_KEYS; i < NUM_WEP_KEYS + 2; i++) {
			if (hostapd_set_encryption(ifname, hapd, "none", NULL,
						   i, NULL, 0,
						   i == 0 ? 1 : 0)) {
				wpa_printf(MSG_DEBUG, "Failed to clear "
					   "default mgmt encryption keys "
					   "(ifname=%s keyidx=%d)", ifname, i);
			}
		}
	}
#endif /* CONFIG_IEEE80211W */
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
	if (ssid->wep.default_len &&
	    hostapd_set_encryption(hapd->conf->iface,
				   hapd, "WEP", NULL, idx,
			 	   ssid->wep.key[idx],
			 	   ssid->wep.len[idx],
				   idx == ssid->wep.idx)) {
		wpa_printf(MSG_WARNING, "Could not set WEP encryption.");
		errors++;
	}

	if (ssid->dyn_vlan_keys) {
		size_t i;
		for (i = 0; i <= ssid->max_dyn_vlan_keys; i++) {
			const char *ifname;
			struct hostapd_wep_keys *key = ssid->dyn_vlan_keys[i];
			if (key == NULL)
				continue;
			ifname = hostapd_get_vlan_id_ifname(hapd->conf->vlan,
							    i);
			if (ifname == NULL)
				continue;

			idx = key->idx;
			if (hostapd_set_encryption(ifname, hapd, "WEP", NULL,
						   idx, key->key[idx],
						   key->len[idx],
						   idx == key->idx)) {
				wpa_printf(MSG_WARNING, "Could not set "
					   "dynamic VLAN WEP encryption.");
				errors++;
			}
		}
	}

	return errors;
}

/**
 * hostapd_cleanup - Per-BSS cleanup (deinitialization)
 * @hapd: Pointer to BSS data
 *
 * This function is used to free all per-BSS data structures and resources.
 * This gets called in a loop for each BSS between calls to
 * hostapd_cleanup_iface_pre() and hostapd_cleanup_iface() when an interface
 * is deinitialized. Most of the modules that are initialized in
 * hostapd_setup_bss() are deinitialized here.
 */
static void hostapd_cleanup(struct hostapd_data *hapd)
{
	hostapd_ctrl_iface_deinit(hapd);

	os_free(hapd->default_wep_key);
	hapd->default_wep_key = NULL;
	iapp_deinit(hapd->iapp);
	hapd->iapp = NULL;
	accounting_deinit(hapd);
	rsn_preauth_iface_deinit(hapd);
	if (hapd->wpa_auth) {
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;

		if (hostapd_set_privacy(hapd, 0)) {
			wpa_printf(MSG_DEBUG, "Could not disable "
				   "PrivacyInvoked for interface %s",
				   hapd->conf->iface);
		}

		if (hostapd_set_generic_elem(hapd, (u8 *) "", 0)) {
			wpa_printf(MSG_DEBUG, "Could not remove generic "
				   "information element from interface %s",
				   hapd->conf->iface);
		}
	}
	ieee802_1x_deinit(hapd);
	vlan_deinit(hapd);
	hostapd_acl_deinit(hapd);
	radius_client_deinit(hapd->radius);
	hapd->radius = NULL;
	radius_server_deinit(hapd->radius_srv);
	hapd->radius_srv = NULL;

#ifdef CONFIG_IEEE80211R
	l2_packet_deinit(hapd->l2);
#endif /* CONFIG_IEEE80211R */

	hostapd_deinit_wps(hapd);

	hostapd_wireless_event_deinit(hapd);

#ifdef EAP_TLS_FUNCS
	if (hapd->ssl_ctx) {
		tls_deinit(hapd->ssl_ctx);
		hapd->ssl_ctx = NULL;
	}
#endif /* EAP_TLS_FUNCS */

#ifdef EAP_SERVER
	if (hapd->eap_sim_db_priv) {
		eap_sim_db_deinit(hapd->eap_sim_db_priv);
		hapd->eap_sim_db_priv = NULL;
	}
#endif /* EAP_SERVER */

	if (hapd->interface_added &&
	    hostapd_bss_remove(hapd, hapd->conf->iface)) {
		wpa_printf(MSG_WARNING, "Failed to remove BSS interface %s",
			   hapd->conf->iface);
	}
}


/**
 * hostapd_cleanup_iface_pre - Preliminary per-interface cleanup
 * @iface: Pointer to interface data
 *
 * This function is called before per-BSS data structures are deinitialized
 * with hostapd_cleanup().
 */
static void hostapd_cleanup_iface_pre(struct hostapd_iface *iface)
{
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
	hostapd_free_hw_features(iface->hw_features, iface->num_hw_features);
	iface->hw_features = NULL;
	os_free(iface->current_rates);
	iface->current_rates = NULL;
	ap_list_deinit(iface);
	hostapd_config_free(iface->conf);
	iface->conf = NULL;

	os_free(iface->config_fname);
	os_free(iface->bss);
	os_free(iface);
}


static int hostapd_setup_encryption(char *iface, struct hostapd_data *hapd)
{
	int i;

	hostapd_broadcast_wep_set(hapd);

	if (hapd->conf->ssid.wep.default_len)
		return 0;

	for (i = 0; i < 4; i++) {
		if (hapd->conf->ssid.wep.key[i] &&
		    hostapd_set_encryption(iface, hapd, "WEP", NULL,
					   i, hapd->conf->ssid.wep.key[i],
					   hapd->conf->ssid.wep.len[i],
					   i == hapd->conf->ssid.wep.idx)) {
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


static int hostapd_flush_old_stations(struct hostapd_data *hapd)
{
	int ret = 0;

	if (hostapd_drv_none(hapd))
		return 0;

	wpa_printf(MSG_DEBUG, "Flushing old station entries");
	if (hostapd_flush(hapd)) {
		wpa_printf(MSG_WARNING, "Could not connect to kernel driver.");
		ret = -1;
	}
	wpa_printf(MSG_DEBUG, "Deauthenticate all stations");
	hostapd_deauth_all_stas(hapd);

	return ret;
}


static void hostapd_wpa_auth_logger(void *ctx, const u8 *addr,
				    logger_level level, const char *txt)
{
	struct hostapd_data *hapd = ctx;
	int hlevel;

	switch (level) {
	case LOGGER_WARNING:
		hlevel = HOSTAPD_LEVEL_WARNING;
		break;
	case LOGGER_INFO:
		hlevel = HOSTAPD_LEVEL_INFO;
		break;
	case LOGGER_DEBUG:
	default:
		hlevel = HOSTAPD_LEVEL_DEBUG;
		break;
	}

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_WPA, hlevel, "%s", txt);
}


static void hostapd_wpa_auth_disconnect(void *ctx, const u8 *addr,
					u16 reason)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "%s: WPA authenticator requests disconnect: "
		   "STA " MACSTR " reason %d",
		   __func__, MAC2STR(addr), reason);

	sta = ap_get_sta(hapd, addr);
	hostapd_sta_deauth(hapd, addr, reason);
	if (sta == NULL)
		return;
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC | WLAN_STA_AUTHORIZED);
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(0, 0, ap_handle_timer, hapd, sta);
	sta->timeout_next = STA_REMOVE;
}


static void hostapd_wpa_auth_mic_failure_report(void *ctx, const u8 *addr)
{
	struct hostapd_data *hapd = ctx;
	ieee80211_michael_mic_failure(hapd, addr, 0);
}


static void hostapd_wpa_auth_set_eapol(void *ctx, const u8 *addr,
				       wpa_eapol_variable var, int value)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return;
	switch (var) {
	case WPA_EAPOL_portEnabled:
		ieee802_1x_notify_port_enabled(sta->eapol_sm, value);
		break;
	case WPA_EAPOL_portValid:
		ieee802_1x_notify_port_valid(sta->eapol_sm, value);
		break;
	case WPA_EAPOL_authorized:
		ieee802_1x_set_sta_authorized(hapd, sta, value);
		break;
	case WPA_EAPOL_portControl_Auto:
		if (sta->eapol_sm)
			sta->eapol_sm->portControl = Auto;
		break;
	case WPA_EAPOL_keyRun:
		if (sta->eapol_sm)
			sta->eapol_sm->keyRun = value ? TRUE : FALSE;
		break;
	case WPA_EAPOL_keyAvailable:
		if (sta->eapol_sm)
			sta->eapol_sm->eap_if->eapKeyAvailable =
				value ? TRUE : FALSE;
		break;
	case WPA_EAPOL_keyDone:
		if (sta->eapol_sm)
			sta->eapol_sm->keyDone = value ? TRUE : FALSE;
		break;
	case WPA_EAPOL_inc_EapolFramesTx:
		if (sta->eapol_sm)
			sta->eapol_sm->dot1xAuthEapolFramesTx++;
		break;
	}
}


static int hostapd_wpa_auth_get_eapol(void *ctx, const u8 *addr,
				      wpa_eapol_variable var)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	if (sta == NULL || sta->eapol_sm == NULL)
		return -1;
	switch (var) {
	case WPA_EAPOL_keyRun:
		return sta->eapol_sm->keyRun;
	case WPA_EAPOL_keyAvailable:
		return sta->eapol_sm->eap_if->eapKeyAvailable;
	default:
		return -1;
	}
}


static const u8 * hostapd_wpa_auth_get_psk(void *ctx, const u8 *addr,
					   const u8 *prev_psk)
{
	struct hostapd_data *hapd = ctx;
	return hostapd_get_psk(hapd->conf, addr, prev_psk);
}


static int hostapd_wpa_auth_get_msk(void *ctx, const u8 *addr, u8 *msk,
				    size_t *len)
{
	struct hostapd_data *hapd = ctx;
	const u8 *key;
	size_t keylen;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return -1;

	key = ieee802_1x_get_key(sta->eapol_sm, &keylen);
	if (key == NULL)
		return -1;

	if (keylen > *len)
		keylen = *len;
	os_memcpy(msk, key, keylen);
	*len = keylen;

	return 0;
}


static int hostapd_wpa_auth_set_key(void *ctx, int vlan_id, const char *alg,
				    const u8 *addr, int idx, u8 *key,
				    size_t key_len)
{
	struct hostapd_data *hapd = ctx;
	const char *ifname = hapd->conf->iface;

	if (vlan_id > 0) {
		ifname = hostapd_get_vlan_id_ifname(hapd->conf->vlan, vlan_id);
		if (ifname == NULL)
			return -1;
	}

	return hostapd_set_encryption(ifname, hapd, alg, addr, idx,
				      key, key_len, 1);
}


static int hostapd_wpa_auth_get_seqnum(void *ctx, const u8 *addr, int idx,
				       u8 *seq)
{
	struct hostapd_data *hapd = ctx;
	return hostapd_get_seqnum(hapd->conf->iface, hapd, addr, idx, seq);
}


static int hostapd_wpa_auth_get_seqnum_igtk(void *ctx, const u8 *addr, int idx,
					    u8 *seq)
{
	struct hostapd_data *hapd = ctx;
	return hostapd_get_seqnum_igtk(hapd->conf->iface, hapd, addr, idx,
				       seq);
}


static int hostapd_wpa_auth_send_eapol(void *ctx, const u8 *addr,
				       const u8 *data, size_t data_len,
				       int encrypt)
{
	struct hostapd_data *hapd = ctx;
	return hostapd_send_eapol(hapd, addr, data, data_len, encrypt);
}


static int hostapd_wpa_auth_for_each_sta(
	void *ctx, int (*cb)(struct wpa_state_machine *sm, void *ctx),
	void *cb_ctx)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->wpa_sm && cb(sta->wpa_sm, cb_ctx))
			return 1;
	}
	return 0;
}


static int hostapd_wpa_auth_for_each_auth(
	void *ctx, int (*cb)(struct wpa_authenticator *sm, void *ctx),
	void *cb_ctx)
{
	struct hostapd_data *ohapd;
	size_t i, j;
	struct hapd_interfaces *interfaces = eloop_get_user_data();

	for (i = 0; i < interfaces->count; i++) {
		for (j = 0; j < interfaces->iface[i]->num_bss; j++) {
			ohapd = interfaces->iface[i]->bss[j];
			if (cb(ohapd->wpa_auth, cb_ctx))
				return 1;
		}
	}

	return 0;
}


static int hostapd_wpa_auth_send_ether(void *ctx, const u8 *dst, u16 proto,
				       const u8 *data, size_t data_len)
{
	struct hostapd_data *hapd = ctx;

	if (hapd->driver && hapd->driver->send_ether)
		return hapd->driver->send_ether(hapd->drv_priv, dst,
						hapd->own_addr, proto,
						data, data_len);
	if (hapd->l2 == NULL)
		return -1;
	return l2_packet_send(hapd->l2, dst, proto, data, data_len);
}


#ifdef CONFIG_IEEE80211R

static int hostapd_wpa_auth_send_ft_action(void *ctx, const u8 *dst,
					   const u8 *data, size_t data_len)
{
	struct hostapd_data *hapd = ctx;
	int res;
	struct ieee80211_mgmt *m;
	size_t mlen;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL || sta->wpa_sm == NULL)
		return -1;

	m = os_zalloc(sizeof(*m) + data_len);
	if (m == NULL)
		return -1;
	mlen = ((u8 *) &m->u - (u8 *) m) + data_len;
	m->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					WLAN_FC_STYPE_ACTION);
	os_memcpy(m->da, dst, ETH_ALEN);
	os_memcpy(m->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(m->bssid, hapd->own_addr, ETH_ALEN);
	os_memcpy(&m->u, data, data_len);

	res = hostapd_send_mgmt_frame(hapd, (u8 *) m, mlen, 0);
	os_free(m);
	return res;
}


static struct wpa_state_machine *
hostapd_wpa_auth_add_sta(void *ctx, const u8 *sta_addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_sta_add(hapd, sta_addr);
	if (sta == NULL)
		return NULL;
	if (sta->wpa_sm)
		return sta->wpa_sm;

	sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr);
	if (sta->wpa_sm == NULL) {
		ap_free_sta(hapd, sta);
		return NULL;
	}
	sta->auth_alg = WLAN_AUTH_FT;

	return sta->wpa_sm;
}


static void hostapd_rrb_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct hostapd_data *hapd = ctx;
	wpa_ft_rrb_rx(hapd->wpa_auth, src_addr, buf, len);
}

#endif /* CONFIG_IEEE80211R */


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
	int res;

	if (hostapd_drv_none(hapd))
		return 0;

	/* Generate BSSID mask that is large enough to cover the BSSIDs. */

	/* Determine the bits necessary to cover the number of BSSIDs. */
	for (i--; i; i >>= 1)
		bits++;

	/* Determine the bits necessary to any configured BSSIDs,
	   if they are higher than the number of BSSIDs. */
	for (j = 0; j < iface->conf->num_bss; j++) {
		if (hostapd_mac_comp_empty(iface->conf->bss[j].bssid) == 0)
			continue;

		for (i = 0; i < ETH_ALEN; i++) {
			mask[i] |=
				iface->conf->bss[j].bssid[i] ^
				hapd->own_addr[i];
		}
	}

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

	if (bits > 40)
		return -1;

	os_memset(mask, 0xff, ETH_ALEN);
	j = bits / 8;
	for (i = 5; i > 5 - j; i--)
		mask[i] = 0;
	j = bits % 8;
	while (j--)
		mask[i] <<= 1;

	wpa_printf(MSG_DEBUG, "BSS count %lu, BSSID mask " MACSTR " (%d bits)",
		   (unsigned long) iface->conf->num_bss, MAC2STR(mask), bits);

	res = hostapd_valid_bss_mask(hapd, hapd->own_addr, mask);
	if (res == 0)
		return 0;

	if (res < 0) {
		wpa_printf(MSG_ERROR, "Driver did not accept BSSID mask "
			   MACSTR " for start address " MACSTR ".",
			   MAC2STR(mask), MAC2STR(hapd->own_addr));
		return -1;
	}

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
		if (hostapd_mac_comp(conf->bss[i].bssid, a) == 0) {
			return 1;
		}
	}

	return 0;
}


static int hostapd_setup_wpa(struct hostapd_data *hapd)
{
	struct wpa_auth_config _conf;
	struct wpa_auth_callbacks cb;
	const u8 *wpa_ie;
	size_t wpa_ie_len;

	hostapd_wpa_auth_conf(hapd->conf, &_conf);
	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = hapd;
	cb.logger = hostapd_wpa_auth_logger;
	cb.disconnect = hostapd_wpa_auth_disconnect;
	cb.mic_failure_report = hostapd_wpa_auth_mic_failure_report;
	cb.set_eapol = hostapd_wpa_auth_set_eapol;
	cb.get_eapol = hostapd_wpa_auth_get_eapol;
	cb.get_psk = hostapd_wpa_auth_get_psk;
	cb.get_msk = hostapd_wpa_auth_get_msk;
	cb.set_key = hostapd_wpa_auth_set_key;
	cb.get_seqnum = hostapd_wpa_auth_get_seqnum;
	cb.get_seqnum_igtk = hostapd_wpa_auth_get_seqnum_igtk;
	cb.send_eapol = hostapd_wpa_auth_send_eapol;
	cb.for_each_sta = hostapd_wpa_auth_for_each_sta;
	cb.for_each_auth = hostapd_wpa_auth_for_each_auth;
	cb.send_ether = hostapd_wpa_auth_send_ether;
#ifdef CONFIG_IEEE80211R
	cb.send_ft_action = hostapd_wpa_auth_send_ft_action;
	cb.add_sta = hostapd_wpa_auth_add_sta;
#endif /* CONFIG_IEEE80211R */
	hapd->wpa_auth = wpa_init(hapd->own_addr, &_conf, &cb);
	if (hapd->wpa_auth == NULL) {
		wpa_printf(MSG_ERROR, "WPA initialization failed.");
		return -1;
	}

	if (hostapd_set_privacy(hapd, 1)) {
		wpa_printf(MSG_ERROR, "Could not set PrivacyInvoked "
			   "for interface %s", hapd->conf->iface);
		return -1;
	}

	wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
	if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len)) {
		wpa_printf(MSG_ERROR, "Failed to configure WPA IE for "
			   "the kernel driver.");
		return -1;
	}

	if (rsn_preauth_iface_init(hapd)) {
		wpa_printf(MSG_ERROR, "Initialization of RSN "
			   "pre-authentication failed.");
		return -1;
	}

	return 0;

}


static int hostapd_setup_radius_srv(struct hostapd_data *hapd,
				    struct hostapd_bss_config *conf)
{
	struct radius_server_conf srv;
	os_memset(&srv, 0, sizeof(srv));
	srv.client_file = conf->radius_server_clients;
	srv.auth_port = conf->radius_server_auth_port;
	srv.conf_ctx = conf;
	srv.eap_sim_db_priv = hapd->eap_sim_db_priv;
	srv.ssl_ctx = hapd->ssl_ctx;
	srv.pac_opaque_encr_key = conf->pac_opaque_encr_key;
	srv.eap_fast_a_id = conf->eap_fast_a_id;
	srv.eap_fast_a_id_len = conf->eap_fast_a_id_len;
	srv.eap_fast_a_id_info = conf->eap_fast_a_id_info;
	srv.eap_fast_prov = conf->eap_fast_prov;
	srv.pac_key_lifetime = conf->pac_key_lifetime;
	srv.pac_key_refresh_time = conf->pac_key_refresh_time;
	srv.eap_sim_aka_result_ind = conf->eap_sim_aka_result_ind;
	srv.tnc = conf->tnc;
	srv.wps = hapd->wps;
	srv.ipv6 = conf->radius_server_ipv6;
	srv.get_eap_user = hostapd_radius_get_eap_user;
	srv.eap_req_id_text = conf->eap_req_id_text;
	srv.eap_req_id_text_len = conf->eap_req_id_text_len;

	hapd->radius_srv = radius_server_init(&srv);
	if (hapd->radius_srv == NULL) {
		wpa_printf(MSG_ERROR, "RADIUS server initialization failed.");
		return -1;
	}

	return 0;
}


/**
 * hostapd_setup_bss - Per-BSS setup (initialization)
 * @hapd: Pointer to BSS data
 * @first: Whether this BSS is the first BSS of an interface
 *
 * This function is used to initialize all per-BSS data structures and
 * resources. This gets called in a loop for each BSS when an interface is
 * initialized. Most of the modules that are initialized here will be
 * deinitialized in hostapd_cleanup().
 */
static int hostapd_setup_bss(struct hostapd_data *hapd, int first)
{
	struct hostapd_bss_config *conf = hapd->conf;
	u8 ssid[HOSTAPD_MAX_SSID_LEN + 1];
	int ssid_len, set_ssid;

	if (!first) {
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0) {
			/* Allocate the next available BSSID. */
			do {
				inc_byte_array(hapd->own_addr, ETH_ALEN);
			} while (mac_in_conf(hapd->iconf, hapd->own_addr));
		} else {
			/* Allocate the configured BSSID. */
			os_memcpy(hapd->own_addr, hapd->conf->bssid, ETH_ALEN);

			if (hostapd_mac_comp(hapd->own_addr,
					     hapd->iface->bss[0]->own_addr) ==
			    0) {
				wpa_printf(MSG_ERROR, "BSS '%s' may not have "
					   "BSSID set to the MAC address of "
					   "the radio", hapd->conf->iface);
				return -1;
			}
		}

		hapd->interface_added = 1;
		if (hostapd_bss_add(hapd->iface->bss[0], hapd->conf->iface,
				    hapd->own_addr)) {
			wpa_printf(MSG_ERROR, "Failed to add BSS (BSSID="
				   MACSTR ")", MAC2STR(hapd->own_addr));
			return -1;
		}
	}

	hostapd_flush_old_stations(hapd);
	hostapd_set_privacy(hapd, 0);

	hostapd_broadcast_wep_clear(hapd);
	if (hostapd_setup_encryption(hapd->conf->iface, hapd))
		return -1;

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
		conf->ssid.ssid[conf->ssid.ssid_len] = '\0';
	}

	if (!hostapd_drv_none(hapd)) {
		wpa_printf(MSG_ERROR, "Using interface %s with hwaddr " MACSTR
			   " and ssid '%s'",
			   hapd->conf->iface, MAC2STR(hapd->own_addr),
			   hapd->conf->ssid.ssid);
	}

	if (hostapd_setup_wpa_psk(conf)) {
		wpa_printf(MSG_ERROR, "WPA-PSK setup failed.");
		return -1;
	}

	/* Set flag for whether SSID is broadcast in beacons */
	if (hostapd_set_broadcast_ssid(hapd,
				       !!hapd->conf->ignore_broadcast_ssid)) {
		wpa_printf(MSG_ERROR, "Could not set broadcast SSID flag for "
			   "kernel driver");
		return -1;
	}

	if (hostapd_set_dtim_period(hapd, hapd->conf->dtim_period)) {
		wpa_printf(MSG_ERROR, "Could not set DTIM period for kernel "
			   "driver");
		return -1;
	}

	/* Set SSID for the kernel driver (to be used in beacon and probe
	 * response frames) */
	if (set_ssid && hostapd_set_ssid(hapd, (u8 *) conf->ssid.ssid,
					 conf->ssid.ssid_len)) {
		wpa_printf(MSG_ERROR, "Could not set SSID for kernel driver");
		return -1;
	}

	if (wpa_debug_level == MSG_MSGDUMP)
		conf->radius->msg_dumps = 1;
	hapd->radius = radius_client_init(hapd, conf->radius);
	if (hapd->radius == NULL) {
		wpa_printf(MSG_ERROR, "RADIUS client initialization failed.");
		return -1;
	}

	if (hostapd_acl_init(hapd)) {
		wpa_printf(MSG_ERROR, "ACL initialization failed.");
		return -1;
	}
	if (hostapd_init_wps(hapd, conf))
		return -1;

	if (ieee802_1x_init(hapd)) {
		wpa_printf(MSG_ERROR, "IEEE 802.1X initialization failed.");
		return -1;
	}

	if (hapd->conf->wpa && hostapd_setup_wpa(hapd))
		return -1;

	if (accounting_init(hapd)) {
		wpa_printf(MSG_ERROR, "Accounting initialization failed.");
		return -1;
	}

	if (hapd->conf->ieee802_11f &&
	    (hapd->iapp = iapp_init(hapd, hapd->conf->iapp_iface)) == NULL) {
		wpa_printf(MSG_ERROR, "IEEE 802.11F (IAPP) initialization "
			   "failed.");
		return -1;
	}

	if (hostapd_ctrl_iface_init(hapd)) {
		wpa_printf(MSG_ERROR, "Failed to setup control interface");
		return -1;
	}

	if (!hostapd_drv_none(hapd) && vlan_init(hapd)) {
		wpa_printf(MSG_ERROR, "VLAN initialization failed.");
		return -1;
	}

#ifdef CONFIG_IEEE80211R
	if (!hostapd_drv_none(hapd)) {
		hapd->l2 = l2_packet_init(hapd->conf->iface, NULL, ETH_P_RRB,
					  hostapd_rrb_receive, hapd, 0);
		if (hapd->l2 == NULL &&
		    (hapd->driver == NULL ||
		     hapd->driver->send_ether == NULL)) {
			wpa_printf(MSG_ERROR, "Failed to open l2_packet "
				   "interface");
			return -1;
		}
	}
#endif /* CONFIG_IEEE80211R */

	ieee802_11_set_beacon(hapd);

	if (conf->radius_server_clients &&
	    hostapd_setup_radius_srv(hapd, conf))
		return -1;

	return 0;
}


static void hostapd_tx_queue_params(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int i;
	struct hostapd_tx_queue_params *p;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		p = &iface->conf->tx_queue[i];

		if (!p->configured)
			continue;

		if (hostapd_set_tx_queue_params(hapd, i, p->aifs, p->cwmin,
						p->cwmax, p->burst)) {
			wpa_printf(MSG_DEBUG, "Failed to set TX queue "
				   "parameters for queue %d.", i);
			/* Continue anyway */
		}
	}
}


static int hostapd_radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user)
{
	const struct hostapd_eap_user *eap_user;
	int i, count;

	eap_user = hostapd_get_eap_user(ctx, identity, identity_len, phase2);
	if (eap_user == NULL)
		return -1;

	if (user == NULL)
		return 0;

	os_memset(user, 0, sizeof(*user));
	count = EAP_USER_MAX_METHODS;
	if (count > EAP_MAX_METHODS)
		count = EAP_MAX_METHODS;
	for (i = 0; i < count; i++) {
		user->methods[i].vendor = eap_user->methods[i].vendor;
		user->methods[i].method = eap_user->methods[i].method;
	}

	if (eap_user->password) {
		user->password = os_malloc(eap_user->password_len);
		if (user->password == NULL)
			return -1;
		os_memcpy(user->password, eap_user->password,
			  eap_user->password_len);
		user->password_len = eap_user->password_len;
		user->password_hash = eap_user->password_hash;
	}
	user->force_version = eap_user->force_version;
	user->ttls_auth = eap_user->ttls_auth;

	return 0;
}


static int setup_interface(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_bss_config *conf = hapd->conf;
	size_t i;
	char country[4];
	u8 *b = conf->bssid;
	int freq;
	size_t j;
	u8 *prev_addr;

	/*
	 * Initialize the driver interface and make sure that all BSSes get
	 * configured with a pointer to this driver interface.
	 */
	if (b[0] | b[1] | b[2] | b[3] | b[4] | b[5]) {
		hapd->drv_priv = hostapd_driver_init_bssid(hapd, b);
	} else {
		hapd->drv_priv = hostapd_driver_init(hapd);
	}

	if (hapd->drv_priv == NULL) {
		wpa_printf(MSG_ERROR, "%s driver initialization failed.",
			   hapd->driver ? hapd->driver->name : "Unknown");
		hapd->driver = NULL;
		return -1;
	}
	for (i = 0; i < iface->num_bss; i++) {
		iface->bss[i]->driver = hapd->driver;
		iface->bss[i]->drv_priv = hapd->drv_priv;
	}

	if (hostapd_validate_bssid_configuration(iface))
		return -1;

#ifdef CONFIG_IEEE80211N
	SET_2BIT_LE16(&iface->ht_op_mode,
		      HT_INFO_OPERATION_MODE_OP_MODE_OFFSET,
		      OP_MODE_PURE);
#endif /* CONFIG_IEEE80211N */

	if (hapd->iconf->country[0] && hapd->iconf->country[1]) {
		os_memcpy(country, hapd->iconf->country, 3);
		country[3] = '\0';
		if (hostapd_set_country(hapd, country) < 0) {
			wpa_printf(MSG_ERROR, "Failed to set country code");
			return -1;
		}
	}

	if (hapd->iconf->ieee80211d &&
	    hostapd_set_ieee80211d(hapd, 1) < 0) {
		wpa_printf(MSG_ERROR, "Failed to set ieee80211d (%d)",
			   hapd->iconf->ieee80211d);
		return -1;
	}

	if (hapd->iconf->bridge_packets != INTERNAL_BRIDGE_DO_NOT_CONTROL &&
	    hostapd_set_internal_bridge(hapd, hapd->iconf->bridge_packets)) {
		wpa_printf(MSG_ERROR, "Failed to set bridge_packets for "
			   "kernel driver");
		return -1;
	}

	/* TODO: merge with hostapd_driver_init() ? */
	if (hostapd_wireless_event_init(hapd) < 0)
		return -1;

	if (hostapd_get_hw_features(iface)) {
		/* Not all drivers support this yet, so continue without hw
		 * feature data. */
	} else {
		int ret = hostapd_select_hw_mode(iface);
		if (ret < 0) {
			wpa_printf(MSG_ERROR, "Could not select hw_mode and "
				   "channel. (%d)", ret);
			return -1;
		}
	}

	if (hapd->iconf->channel) {
		freq = hostapd_hw_get_freq(hapd, hapd->iconf->channel);
		wpa_printf(MSG_DEBUG, "Mode: %s  Channel: %d  "
			   "Frequency: %d MHz",
			   hostapd_hw_mode_txt(hapd->iconf->hw_mode),
			   hapd->iconf->channel, freq);

		if (hostapd_set_freq(hapd, hapd->iconf->hw_mode, freq,
				     hapd->iconf->ieee80211n,
				     hapd->iconf->secondary_channel)) {
			wpa_printf(MSG_ERROR, "Could not set channel for "
				   "kernel driver");
			return -1;
		}
	}

	hostapd_set_beacon_int(hapd, hapd->iconf->beacon_int);

	if (hapd->iconf->rts_threshold > -1 &&
	    hostapd_set_rts(hapd, hapd->iconf->rts_threshold)) {
		wpa_printf(MSG_ERROR, "Could not set RTS threshold for "
			   "kernel driver");
		return -1;
	}

	if (hapd->iconf->fragm_threshold > -1 &&
	    hostapd_set_frag(hapd, hapd->iconf->fragm_threshold)) {
		wpa_printf(MSG_ERROR, "Could not set fragmentation threshold "
			   "for kernel driver");
		return -1;
	}

	prev_addr = hapd->own_addr;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (j)
			os_memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hostapd_setup_bss(hapd, j == 0))
			return -1;
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0)
			prev_addr = hapd->own_addr;
	}

	hostapd_tx_queue_params(iface);

	ap_list_init(iface);

	if (hostapd_driver_commit(hapd) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		return -1;
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
 */
static int hostapd_setup_interface(struct hostapd_iface *iface)
{
	int ret;

	ret = setup_interface(iface);
	if (ret) {
		wpa_printf(MSG_DEBUG, "%s: Unable to setup interface.",
			   iface->bss[0]->conf->iface);
		eloop_terminate();
		return -1;
	} else if (!hostapd_drv_none(iface->bss[0])) {
		wpa_printf(MSG_DEBUG, "%s: Setup of interface done.",
			   iface->bss[0]->conf->iface);
	}

	return 0;
}


static void show_version(void)
{
	fprintf(stderr,
		"hostapd v" VERSION_STR "\n"
		"User space daemon for IEEE 802.11 AP management,\n"
		"IEEE 802.1X/WPA/WPA2/EAP/RADIUS Authenticator\n"
		"Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi> "
		"and contributors\n");
}


static void usage(void)
{
	show_version();
	fprintf(stderr,
		"\n"
		"usage: hostapd [-hdBKtv] [-P <PID file>] "
		"<configuration file(s)>\n"
		"\n"
		"options:\n"
		"   -h   show this usage\n"
		"   -d   show more debug messages (-dd for even more)\n"
		"   -B   run daemon in the background\n"
		"   -P   PID file\n"
		"   -K   include key data in debug messages\n"
		"   -t   include timestamps in some debug messages\n"
		"   -v   show hostapd version\n");

	exit(1);
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
static struct hostapd_data *
hostapd_alloc_bss_data(struct hostapd_iface *hapd_iface,
		       struct hostapd_config *conf,
		       struct hostapd_bss_config *bss)
{
	struct hostapd_data *hapd;

	hapd = os_zalloc(sizeof(*hapd));
	if (hapd == NULL)
		return NULL;

	hapd->iconf = conf;
	hapd->conf = bss;
	hapd->iface = hapd_iface;

	if (hapd->conf->individual_wep_key_len > 0) {
		/* use key0 in individual key and key1 in broadcast key */
		hapd->default_wep_key_idx = 1;
	}

#ifdef EAP_TLS_FUNCS
	if (hapd->conf->eap_server &&
	    (hapd->conf->ca_cert || hapd->conf->server_cert ||
	     hapd->conf->dh_file)) {
		struct tls_connection_params params;

		hapd->ssl_ctx = tls_init(NULL);
		if (hapd->ssl_ctx == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize TLS");
			goto fail;
		}

		os_memset(&params, 0, sizeof(params));
		params.ca_cert = hapd->conf->ca_cert;
		params.client_cert = hapd->conf->server_cert;
		params.private_key = hapd->conf->private_key;
		params.private_key_passwd = hapd->conf->private_key_passwd;
		params.dh_file = hapd->conf->dh_file;

		if (tls_global_set_params(hapd->ssl_ctx, &params)) {
			wpa_printf(MSG_ERROR, "Failed to set TLS parameters");
			goto fail;
		}

		if (tls_global_set_verify(hapd->ssl_ctx,
					  hapd->conf->check_crl)) {
			wpa_printf(MSG_ERROR, "Failed to enable check_crl");
			goto fail;
		}
	}
#endif /* EAP_TLS_FUNCS */

#ifdef EAP_SERVER
	if (hapd->conf->eap_sim_db) {
		hapd->eap_sim_db_priv =
			eap_sim_db_init(hapd->conf->eap_sim_db,
					hostapd_sim_db_cb, hapd);
		if (hapd->eap_sim_db_priv == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize EAP-SIM "
				   "database interface");
			goto fail;
		}
	}
#endif /* EAP_SERVER */

	hapd->driver = hapd->iconf->driver;

	return hapd;

#if defined(EAP_TLS_FUNCS) || defined(EAP_SERVER)
fail:
#endif
	/* TODO: cleanup allocated resources(?) */
	os_free(hapd);
	return NULL;
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
static struct hostapd_iface * hostapd_init(const char *config_file)
{
	struct hostapd_iface *hapd_iface = NULL;
	struct hostapd_config *conf = NULL;
	struct hostapd_data *hapd;
	size_t i;

	hapd_iface = os_zalloc(sizeof(*hapd_iface));
	if (hapd_iface == NULL)
		goto fail;

	hapd_iface->config_fname = os_strdup(config_file);
	if (hapd_iface->config_fname == NULL)
		goto fail;

	conf = hostapd_config_read(hapd_iface->config_fname);
	if (conf == NULL)
		goto fail;
	hapd_iface->conf = conf;

	hapd_iface->num_bss = conf->num_bss;
	hapd_iface->bss = os_zalloc(conf->num_bss *
				    sizeof(struct hostapd_data *));
	if (hapd_iface->bss == NULL)
		goto fail;

	for (i = 0; i < conf->num_bss; i++) {
		hapd = hapd_iface->bss[i] =
			hostapd_alloc_bss_data(hapd_iface, conf,
					       &conf->bss[i]);
		if (hapd == NULL)
			goto fail;
	}

	return hapd_iface;

fail:
	if (conf)
		hostapd_config_free(conf);
	if (hapd_iface) {
		for (i = 0; hapd_iface->bss && i < hapd_iface->num_bss; i++) {
			hapd = hapd_iface->bss[i];
			if (hapd && hapd->ssl_ctx)
				tls_deinit(hapd->ssl_ctx);
		}

		os_free(hapd_iface->config_fname);
		os_free(hapd_iface->bss);
		os_free(hapd_iface);
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	struct hapd_interfaces interfaces;
	int ret = 1, k;
	size_t i, j;
	int c, debug = 0, daemonize = 0, tnc = 0;
	char *pid_file = NULL;

	hostapd_logger_register_cb(hostapd_logger_cb);

	for (;;) {
		c = getopt(argc, argv, "BdhKP:tv");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'd':
			debug++;
			if (wpa_debug_level > 0)
				wpa_debug_level--;
			break;
		case 'B':
			daemonize++;
			break;
		case 'K':
			wpa_debug_show_keys++;
			break;
		case 'P':
			os_free(pid_file);
			pid_file = os_rel2abs_path(optarg);
			break;
		case 't':
			wpa_debug_timestamp++;
			break;
		case 'v':
			show_version();
			exit(1);
			break;

		default:
			usage();
			break;
		}
	}

	if (optind == argc)
		usage();

#ifdef EAP_SERVER
	if (eap_server_register_methods()) {
		wpa_printf(MSG_ERROR, "Failed to register EAP methods");
		return -1;
	}
#endif /* EAP_SERVER */

	interfaces.count = argc - optind;

	interfaces.iface = os_malloc(interfaces.count *
				     sizeof(struct hostapd_iface *));
	if (interfaces.iface == NULL) {
		wpa_printf(MSG_ERROR, "malloc failed\n");
		return -1;
	}

	if (eloop_init(&interfaces)) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return -1;
	}

#ifndef CONFIG_NATIVE_WINDOWS
	eloop_register_signal(SIGHUP, handle_reload, NULL);
	eloop_register_signal(SIGUSR1, handle_dump_state, NULL);
#endif /* CONFIG_NATIVE_WINDOWS */
	eloop_register_signal_terminate(handle_term, NULL);

	/* Initialize interfaces */
	for (i = 0; i < interfaces.count; i++) {
		wpa_printf(MSG_ERROR, "Configuration file: %s",
			   argv[optind + i]);
		interfaces.iface[i] = hostapd_init(argv[optind + i]);
		if (!interfaces.iface[i])
			goto out;
		for (k = 0; k < debug; k++) {
			if (interfaces.iface[i]->bss[0]->conf->
			    logger_stdout_level > 0)
				interfaces.iface[i]->bss[0]->conf->
					logger_stdout_level--;
		}

		ret = hostapd_setup_interface(interfaces.iface[i]);
		if (ret)
			goto out;

		for (k = 0; k < (int) interfaces.iface[i]->num_bss; k++) {
			if (interfaces.iface[i]->bss[0]->conf->tnc)
				tnc++;
		}
	}

#ifdef EAP_TNC
	if (tnc && tncs_global_init() < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize TNCS");
		goto out;
	}
#endif /* EAP_TNC */

	if (daemonize && os_daemonize(pid_file)) {
		perror("daemon");
		goto out;
	}

#ifndef CONFIG_NATIVE_WINDOWS
	openlog("hostapd", 0, LOG_DAEMON);
#endif /* CONFIG_NATIVE_WINDOWS */

	eloop_run();

	/* Disconnect associated stations from all interfaces and BSSes */
	for (i = 0; i < interfaces.count; i++) {
		for (j = 0; j < interfaces.iface[i]->num_bss; j++) {
			struct hostapd_data *hapd =
				interfaces.iface[i]->bss[j];
			hostapd_free_stas(hapd);
			hostapd_flush_old_stations(hapd);
		}
	}

	ret = 0;

 out:
	/* Deinitialize all interfaces */
	for (i = 0; i < interfaces.count; i++) {
		if (!interfaces.iface[i])
			continue;
		hostapd_cleanup_iface_pre(interfaces.iface[i]);
		for (j = 0; j < interfaces.iface[i]->num_bss; j++) {
			struct hostapd_data *hapd =
				interfaces.iface[i]->bss[j];
			hostapd_cleanup(hapd);
			if (j == interfaces.iface[i]->num_bss - 1 &&
			    hapd->driver)
				hostapd_driver_deinit(hapd);
		}
		for (j = 0; j < interfaces.iface[i]->num_bss; j++)
			os_free(interfaces.iface[i]->bss[j]);
		hostapd_cleanup_iface(interfaces.iface[i]);
	}
	os_free(interfaces.iface);

#ifdef EAP_TNC
	tncs_global_deinit();
#endif /* EAP_TNC */

	eloop_destroy();

#ifndef CONFIG_NATIVE_WINDOWS
	closelog();
#endif /* CONFIG_NATIVE_WINDOWS */

#ifdef EAP_SERVER
	eap_server_unregister_methods();
#endif /* EAP_SERVER */

	os_daemonize_terminate(pid_file);
	os_free(pid_file);

	return ret;
}
