/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
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
#include "radius_client.h"
#include "radius_server.h"
#include "wpa.h"
#include "preauth.h"
#include "wme.h"
#include "vlan_init.h"
#include "ctrl_iface.h"
#include "tls.h"
#include "eap_sim_db.h"
#include "eap.h"
#include "version.h"


struct hapd_interfaces {
	size_t count;
	struct hostapd_iface **iface;
};

unsigned char rfc1042_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };


extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;


void hostapd_logger(struct hostapd_data *hapd, const u8 *addr,
		    unsigned int module, int level, const char *fmt, ...)
{
	char *format, *module_str;
	int maxlen;
	va_list ap;
	int conf_syslog_level, conf_stdout_level;
	unsigned int conf_syslog, conf_stdout;

	maxlen = strlen(fmt) + 100;
	format = malloc(maxlen);
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
		snprintf(format, maxlen, "%s: STA " MACSTR "%s%s: %s",
			 hapd->conf->iface, MAC2STR(addr),
			 module_str ? " " : "", module_str, fmt);
	else if (hapd && hapd->conf)
		snprintf(format, maxlen, "%s:%s%s %s",
			 hapd->conf->iface, module_str ? " " : "",
			 module_str, fmt);
	else if (addr)
		snprintf(format, maxlen, "STA " MACSTR "%s%s: %s",
			 MAC2STR(addr), module_str ? " " : "",
			 module_str, fmt);
	else
		snprintf(format, maxlen, "%s%s%s",
			 module_str, module_str ? ": " : "", fmt);

	if ((conf_stdout & module) && level >= conf_stdout_level) {
		wpa_debug_print_timestamp();
		va_start(ap, fmt);
		vprintf(format, ap);
		va_end(ap);
		printf("\n");
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
		va_start(ap, fmt);
		vsyslog(priority, format, ap);
		va_end(ap);
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	free(format);
}


const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen)
{
	if (buflen == 0 || addr == NULL)
		return NULL;

	if (addr->af == AF_INET) {
		snprintf(buf, buflen, "%s", inet_ntoa(addr->u.v4));
	} else {
		buf[0] = '\0';
	}
#ifdef CONFIG_IPV6
	if (addr->af == AF_INET6) {
		if (inet_ntop(AF_INET6, &addr->u.v6, buf, buflen) == NULL)
			buf[0] = '\0';
	}
#endif /* CONFIG_IPV6 */

	return buf;
}


int hostapd_ip_diff(struct hostapd_ip_addr *a, struct hostapd_ip_addr *b)
{
	if (a == NULL && b == NULL)
		return 0;
	if (a == NULL || b == NULL)
		return 1;

	switch (a->af) {
	case AF_INET:
		if (a->u.v4.s_addr != b->u.v4.s_addr)
			return 1;
		break;
#ifdef CONFIG_IPV6
	case AF_INET6:
		if (memcpy(&a->u.v6, &b->u.v6, sizeof(a->u.v6))
		    != 0)
			return 1;
		break;
#endif /* CONFIG_IPV6 */
	}

	return 0;
}


static void hostapd_deauth_all_stas(struct hostapd_data *hapd)
{
#if 0
	u8 addr[ETH_ALEN];

	memset(addr, 0xff, ETH_ALEN);
	hostapd_sta_deauth(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
#else
	/* New Prism2.5/3 STA firmware versions seem to have issues with this
	 * broadcast deauth frame. This gets the firmware in odd state where
	 * nothing works correctly, so let's skip sending this for a while
	 * until the issue has been resolved. */
#endif
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

	hostapd_wme_sta_config(hapd, sta);

	/* Start IEEE 802.1X authentication process for new stations */
	ieee802_1x_new_station(hapd, sta);
	if (reassoc)
		wpa_auth_sm_event(sta->wpa_sm, WPA_REAUTH);
	else
		wpa_auth_sta_associated(hapd->wpa_auth, sta->wpa_sm);
}


#ifdef EAP_SERVER
static int hostapd_sim_db_cb_sta(struct hostapd_data *hapd,
				 struct sta_info *sta, void *ctx)
{
	if (eapol_sm_eap_pending_cb(sta->eapol_sm, ctx) == 0)
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


static void handle_term(int sig, void *eloop_ctx, void *signal_ctx)
{
	printf("Signal %d received - terminating\n", sig);
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
	wconf->rsn_preauth = conf->rsn_preauth;
	wconf->eapol_version = conf->eapol_version;
	wconf->peerkey = conf->peerkey;
	wconf->wme_enabled = conf->wme_enabled;
#ifdef CONFIG_IEEE80211W
	wconf->ieee80211w = conf->ieee80211w;
#endif /* CONFIG_IEEE80211W */
}


#ifndef CONFIG_NATIVE_WINDOWS
static void handle_reload(int sig, void *eloop_ctx, void *signal_ctx)
{
	struct hapd_interfaces *hapds = (struct hapd_interfaces *) eloop_ctx;
	struct hostapd_config *newconf;
	size_t i;
	struct wpa_auth_config wpa_auth_conf;

	printf("Signal %d received - reloading configuration\n", sig);

	for (i = 0; i < hapds->count; i++) {
		struct hostapd_data *hapd = hapds->iface[i]->bss[0];
		newconf = hostapd_config_read(hapds->iface[i]->config_fname);
		if (newconf == NULL) {
			printf("Failed to read new configuration file - "
			       "continuing with old.\n");
			continue;
		}
		/* TODO: update dynamic data based on changed configuration
		 * items (e.g., open/close sockets, remove stations added to
		 * deny list, etc.) */
		radius_client_flush(hapd->radius, 0);
		hostapd_config_free(hapd->iconf);

		hostapd_wpa_auth_conf(&newconf->bss[0], &wpa_auth_conf);
		wpa_reconfig(hapd->wpa_auth, &wpa_auth_conf);

		hapd->iconf = newconf;
		hapd->conf = &newconf->bss[0];
		hapds->iface[i]->conf = newconf;

		if (hostapd_setup_wpa_psk(hapd->conf)) {
			wpa_printf(MSG_ERROR, "Failed to re-configure WPA PSK "
				   "after reloading configuration");
		}
	}
}


#ifdef HOSTAPD_DUMP_STATE
static void hostapd_dump_state(struct hostapd_data *hapd)
{
	FILE *f;
	time_t now;
	struct sta_info *sta;
	int i;
	char *buf;

	if (!hapd->conf->dump_log_name) {
		printf("Dump file not defined - ignoring dump request\n");
		return;
	}

	printf("Dumping hostapd state to '%s'\n", hapd->conf->dump_log_name);
	f = fopen(hapd->conf->dump_log_name, "w");
	if (f == NULL) {
		printf("Could not open dump file '%s' for writing.\n",
		       hapd->conf->dump_log_name);
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
			"  AID=%d flags=0x%x %s%s%s%s%s%s%s%s%s%s\n"
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

	buf = malloc(4096);
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
		free(buf);
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
			printf("Failed to clear default encryption keys "
			       "(ifname=%s keyidx=%d)\n", ifname, i);
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
	if (ssid->wep.default_len &&
	    hostapd_set_encryption(hapd->conf->iface,
				   hapd, "WEP", NULL, idx,
			 	   ssid->wep.key[idx],
			 	   ssid->wep.len[idx],
				   idx == ssid->wep.idx)) {
		printf("Could not set WEP encryption.\n");
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
				printf("Could not set dynamic VLAN WEP "
				       "encryption.\n");
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

	free(hapd->default_wep_key);
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
		printf("Failed to remove BSS interface %s\n",
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
	free(iface->current_rates);
	iface->current_rates = NULL;
	ap_list_deinit(iface);
	hostapd_config_free(iface->conf);
	iface->conf = NULL;

	free(iface->config_fname);
	free(iface->bss);
	free(iface);
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
			printf("Could not set WEP encryption.\n");
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

	wpa_printf(MSG_DEBUG, "Flushing old station entries");
	if (hostapd_flush(hapd)) {
		printf("Could not connect to kernel driver.\n");
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
			sta->eapol_sm->keyAvailable = value ? TRUE : FALSE;
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
		return sta->eapol_sm->keyAvailable;
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


static int hostapd_wpa_auth_get_pmk(void *ctx, const u8 *addr, u8 *pmk,
				    size_t *len)
{
	struct hostapd_data *hapd = ctx;
	u8 *key;
	size_t keylen;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return -1;

	key = ieee802_1x_get_key_crypt(sta->eapol_sm, &keylen);
	if (key == NULL)
		return -1;

	if (keylen > *len)
		keylen = WPA_PMK_LEN;
	memcpy(pmk, key, keylen);
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

	memset(mask, 0xff, ETH_ALEN);
	j = bits / 8;
	for (i = 5; i > 5 - j; i--)
		mask[i] = 0;
	j = bits % 8;
	while (j--)
		mask[i] <<= 1;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "BSS count %lu, BSSID mask "
		      MACSTR " (%d bits)\n",
		      (unsigned long) iface->conf->num_bss, MAC2STR(mask),
		      bits);

	res = hostapd_valid_bss_mask(hapd, hapd->own_addr, mask);
	if (res == 0)
		return 0;

	if (res < 0) {
		printf("Driver did not accept BSSID mask " MACSTR " for start "
		       "address " MACSTR ".\n",
		       MAC2STR(mask), MAC2STR(hapd->own_addr));
		return -1;
	}

	for (i = 0; i < ETH_ALEN; i++) {
		if ((hapd->own_addr[i] & mask[i]) != hapd->own_addr[i]) {
			printf("Invalid BSSID mask " MACSTR " for start "
			       "address " MACSTR ".\n"
			       "Start address must be the first address in the"
			       " block (i.e., addr AND mask == addr).\n",
			       MAC2STR(mask), MAC2STR(hapd->own_addr));
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
			memcpy(hapd->own_addr, hapd->conf->bssid, ETH_ALEN);

			if (hostapd_mac_comp(hapd->own_addr,
					     hapd->iface->bss[0]->own_addr) ==
			    0) {
				printf("BSS '%s' may not have BSSID "
				       "set to the MAC address of the radio\n",
				       hapd->conf->iface);
				return -1;
			}
		}

		hapd->interface_added = 1;
		if (hostapd_bss_add(hapd->iface->bss[0], hapd->conf->iface,
				    hapd->own_addr)) {
			printf("Failed to add BSS (BSSID=" MACSTR ")\n",
			       MAC2STR(hapd->own_addr));
			return -1;
		}
	}

	/*
	 * Fetch the SSID from the system and use it or,
	 * if one was specified in the config file, verify they
	 * match.
	 */
	ssid_len = hostapd_get_ssid(hapd, ssid, sizeof(ssid));
	if (ssid_len < 0) {
		printf("Could not read SSID from system\n");
		return -1;
	}
	if (conf->ssid.ssid_set) {
		/*
		 * If SSID is specified in the config file and it differs
		 * from what is being used then force installation of the
		 * new SSID.
		 */
		set_ssid = (conf->ssid.ssid_len != (size_t) ssid_len ||
			    memcmp(conf->ssid.ssid, ssid, ssid_len) != 0);
	} else {
		/*
		 * No SSID in the config file; just use the one we got
		 * from the system.
		 */
		set_ssid = 0;
		conf->ssid.ssid_len = ssid_len;
		memcpy(conf->ssid.ssid, ssid, conf->ssid.ssid_len);
		conf->ssid.ssid[conf->ssid.ssid_len] = '\0';
	}

	printf("Using interface %s with hwaddr " MACSTR " and ssid '%s'\n",
	       hapd->conf->iface, MAC2STR(hapd->own_addr),
	       hapd->conf->ssid.ssid);

	if (hostapd_setup_wpa_psk(conf)) {
		printf("WPA-PSK setup failed.\n");
		return -1;
	}

	/* Set flag for whether SSID is broadcast in beacons */
	if (hostapd_set_broadcast_ssid(hapd,
				       !!hapd->conf->ignore_broadcast_ssid)) {
		printf("Could not set broadcast SSID flag for kernel "
		       "driver\n");
		return -1;
	}

	if (hostapd_set_dtim_period(hapd, hapd->conf->dtim_period)) {
		printf("Could not set DTIM period for kernel driver\n");
		return -1;
	}

	/* Set SSID for the kernel driver (to be used in beacon and probe
	 * response frames) */
	if (set_ssid && hostapd_set_ssid(hapd, (u8 *) conf->ssid.ssid,
					 conf->ssid.ssid_len)) {
		printf("Could not set SSID for kernel driver\n");
		return -1;
	}

	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MSGDUMPS))
		conf->radius->msg_dumps = 1;
	hapd->radius = radius_client_init(hapd, conf->radius);
	if (hapd->radius == NULL) {
		printf("RADIUS client initialization failed.\n");
		return -1;
	}

	if (hostapd_acl_init(hapd)) {
		printf("ACL initialization failed.\n");
		return -1;
	}
	if (ieee802_1x_init(hapd)) {
		printf("IEEE 802.1X initialization failed.\n");
		return -1;
	}

	if (hapd->conf->wpa) {
		struct wpa_auth_config _conf;
		struct wpa_auth_callbacks cb;
		const u8 *wpa_ie;
		size_t wpa_ie_len;

		hostapd_wpa_auth_conf(hapd->conf, &_conf);
		memset(&cb, 0, sizeof(cb));
		cb.ctx = hapd;
		cb.logger = hostapd_wpa_auth_logger;
		cb.disconnect = hostapd_wpa_auth_disconnect;
		cb.mic_failure_report = hostapd_wpa_auth_mic_failure_report;
		cb.set_eapol = hostapd_wpa_auth_set_eapol;
		cb.get_eapol = hostapd_wpa_auth_get_eapol;
		cb.get_psk = hostapd_wpa_auth_get_psk;
		cb.get_pmk = hostapd_wpa_auth_get_pmk;
		cb.set_key = hostapd_wpa_auth_set_key;
		cb.get_seqnum = hostapd_wpa_auth_get_seqnum;
		cb.get_seqnum_igtk = hostapd_wpa_auth_get_seqnum_igtk;
		cb.send_eapol = hostapd_wpa_auth_send_eapol;
		cb.for_each_sta = hostapd_wpa_auth_for_each_sta;
		hapd->wpa_auth = wpa_init(hapd->own_addr, &_conf, &cb);
		if (hapd->wpa_auth == NULL) {
			printf("WPA initialization failed.\n");
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
			printf("Initialization of RSN pre-authentication "
			       "failed.\n");
			return -1;
		}
	}

	if (accounting_init(hapd)) {
		printf("Accounting initialization failed.\n");
		return -1;
	}

	if (hapd->conf->ieee802_11f &&
	    (hapd->iapp = iapp_init(hapd, hapd->conf->iapp_iface)) == NULL) {
		printf("IEEE 802.11F (IAPP) initialization failed.\n");
		return -1;
	}

	if (hostapd_ctrl_iface_init(hapd)) {
		printf("Failed to setup control interface\n");
		return -1;
	}

	ieee802_11_set_beacon(hapd);

	if (vlan_init(hapd)) {
		printf("VLAN initialization failed.\n");
		return -1;
	}

	return 0;
}


/**
 * setup_interface2 - Setup (initialize) an interface (part 2)
 * @iface: Pointer to interface data.
 * Returns: 0 on success; -1 on failure.
 *
 * Flushes old stations, sets the channel, DFS parameters, encryption,
 * beacons, and WDS links based on the configuration.
 */
static int setup_interface2(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	int freq;
	size_t j;
	int ret = 0;
	u8 *prev_addr;

	hostapd_flush_old_stations(hapd);
	hostapd_set_privacy(hapd, 0);

	if (hapd->iconf->channel) {
		freq = hostapd_hw_get_freq(hapd, hapd->iconf->channel);
		printf("Mode: %s  Channel: %d  Frequency: %d MHz\n",
		       hostapd_hw_mode_txt(hapd->iconf->hw_mode),
		       hapd->iconf->channel, freq);

		if (hostapd_set_freq(hapd, hapd->iconf->hw_mode, freq)) {
			printf("Could not set channel for kernel driver\n");
			return -1;
		}
	}

	hostapd_broadcast_wep_clear(hapd);
	if (hostapd_setup_encryption(hapd->conf->iface, hapd))
		return -1;

	hostapd_set_beacon_int(hapd, hapd->iconf->beacon_int);
	ieee802_11_set_beacon(hapd);

	if (hapd->iconf->rts_threshold > -1 &&
	    hostapd_set_rts(hapd, hapd->iconf->rts_threshold)) {
		printf("Could not set RTS threshold for kernel driver\n");
		return -1;
	}

	if (hapd->iconf->fragm_threshold > -1 &&
	    hostapd_set_frag(hapd, hapd->iconf->fragm_threshold)) {
		printf("Could not set fragmentation threshold for kernel "
		       "driver\n");
		return -1;
	}

	prev_addr = hapd->own_addr;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (j)
			memcpy(hapd->own_addr, prev_addr, ETH_ALEN);
		if (hostapd_setup_bss(hapd, j == 0))
			return -1;
		if (hostapd_mac_comp_empty(hapd->conf->bssid) == 0)
			prev_addr = hapd->own_addr;
	}

	ap_list_init(iface);

	if (hostapd_driver_commit(hapd) < 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to commit driver "
			   "configuration", __func__);
		return -1;
	}

	return ret;
}


static void setup_interface_start(void *eloop_data, void *user_ctx);
static void setup_interface2_handler(void *eloop_data, void *user_ctx);

/**
 * setup_interface_finalize - Finish setup interface & call the callback
 * @iface: Pointer to interface data.
 * @status: Status of the setup interface (0 on success; -1 on failure).
 * Returns: 0 on success; -1 on failure (e.g., was not in progress).
 */
static int setup_interface_finalize(struct hostapd_iface *iface, int status)
{
	hostapd_iface_cb cb;

	if (!iface->setup_cb)
		return -1;
	
	eloop_cancel_timeout(setup_interface_start, iface, NULL);
	eloop_cancel_timeout(setup_interface2_handler, iface, NULL);
	hostapd_select_hw_mode_stop(iface);

	cb = iface->setup_cb;

	iface->setup_cb = NULL;

	cb(iface, status);

	return 0;
}


/**
 * setup_interface2_wrapper - Wrapper for setup_interface2()
 * @iface: Pointer to interface data.
 * @status: Status of the hw mode select.
 *
 * Wrapper for setup_interface2() to calls finalize function upon completion.
 */
static void setup_interface2_wrapper(struct hostapd_iface *iface, int status)
{
	int ret = status;
	if (ret)
		printf("Could not select hw_mode and channel. (%d)\n", ret);
	else
		ret = setup_interface2(iface);

	setup_interface_finalize(iface, ret);
}


/**
 * setup_interface2_handler - Used for immediate call of setup_interface2
 * @eloop_data: Stores the struct hostapd_iface * for the interface.
 * @user_ctx: Unused.
 */
static void setup_interface2_handler(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;

	setup_interface2_wrapper(iface, 0);
}


/**
 * setup_interface1 - Setup (initialize) an interface (part 1)
 * @iface: Pointer to interface data
 * Returns: 0 on success, -1 on failure
 *
 * Initializes the driver interface, validates the configuration,
 * and sets driver parameters based on the configuration.
 * Schedules setup_interface2() to be called immediately or after
 * hardware mode setup takes place. 
 */
static int setup_interface1(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct hostapd_bss_config *conf = hapd->conf;
	size_t i;
	char country[4];

	/*
	 * Initialize the driver interface and make sure that all BSSes get
	 * configured with a pointer to this driver interface.
	 */
	if (hostapd_driver_init(hapd)) {
		printf("%s driver initialization failed.\n",
			hapd->driver ? hapd->driver->name : "Unknown");
		hapd->driver = NULL;
		return -1;
	}
	for (i = 0; i < iface->num_bss; i++)
		iface->bss[i]->driver = hapd->driver;

	if (hostapd_validate_bssid_configuration(iface))
		return -1;

	memcpy(country, hapd->iconf->country, 3);
	country[3] = '\0';
	if (hostapd_set_country(hapd, country) < 0) {
		printf("Failed to set country code\n");
		return -1;
	}

	if (hapd->iconf->ieee80211d || hapd->iconf->ieee80211h) {
		if (hostapd_set_ieee80211d(hapd, 1) < 0) {
			printf("Failed to set ieee80211d (%d)\n",
			       hapd->iconf->ieee80211d);
			return -1;
		}
	}

	if (hapd->iconf->bridge_packets != INTERNAL_BRIDGE_DO_NOT_CONTROL &&
	    hostapd_set_internal_bridge(hapd, hapd->iconf->bridge_packets)) {
		printf("Failed to set bridge_packets for kernel driver\n");
		return -1;
	}

	if (conf->radius_server_clients) {
		struct radius_server_conf srv;
		memset(&srv, 0, sizeof(srv));
		srv.client_file = conf->radius_server_clients;
		srv.auth_port = conf->radius_server_auth_port;
		srv.hostapd_conf = conf;
		srv.eap_sim_db_priv = hapd->eap_sim_db_priv;
		srv.ssl_ctx = hapd->ssl_ctx;
		srv.ipv6 = conf->radius_server_ipv6;
		hapd->radius_srv = radius_server_init(&srv);
		if (hapd->radius_srv == NULL) {
			printf("RADIUS server initialization failed.\n");
			return -1;
		}
	}

	/* TODO: merge with hostapd_driver_init() ? */
	if (hostapd_wireless_event_init(hapd) < 0)
		return -1;

	if (hostapd_get_hw_features(iface)) {
		/* Not all drivers support this yet, so continue without hw
		 * feature data. */
	} else {
		return hostapd_select_hw_mode_start(iface,
						    setup_interface2_wrapper);
	}

	eloop_register_timeout(0, 0, setup_interface2_handler, iface, NULL);
	return 0;
}


/**
 * setup_interface_start - Handler to start setup interface
 * @eloop_data: Stores the struct hostapd_iface * for the interface.
 * @user_ctx: Unused.
 *
 * An eloop handler is used so that all errors can be processed by the
 * callback without introducing stack recursion.
 */
static void setup_interface_start(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;

	int ret;

	ret = setup_interface1(iface);
	if (ret)
		setup_interface_finalize(iface, ret);
}


/**
 * hostapd_setup_interface_start - Start the setup of an interface
 * @iface: Pointer to interface data.
 * @cb: The function to callback when done.
 * Returns:  0 if it starts successfully; cb will be called when done.
 *          -1 on failure; cb will not be called.
 *
 * Initializes the driver interface, validates the configuration,
 * and sets driver parameters based on the configuration.
 * Flushes old stations, sets the channel, DFS parameters, encryption,
 * beacons, and WDS links based on the configuration.
 */
int hostapd_setup_interface_start(struct hostapd_iface *iface,
				  hostapd_iface_cb cb)
{
	if (iface->setup_cb) {
		wpa_printf(MSG_DEBUG,
			   "%s: Interface setup already in progress.\n",
			   iface->bss[0]->conf->iface);
		return -1;
	}

	iface->setup_cb = cb;

	eloop_register_timeout(0, 0, setup_interface_start, iface, NULL);

	return 0;
}


/**
 * hostapd_setup_interace_stop - Stops the setup of an interface
 * @iface: Pointer to interface data
 * Returns:  0 if successfully stopped;
 *          -1 on failure (i.e., was not in progress)
 */
int hostapd_setup_interface_stop(struct hostapd_iface *iface)
{
	return setup_interface_finalize(iface, -1);
}


struct driver {
	struct driver *next;
	char *name;
	const struct driver_ops *ops;
};
static struct driver *drivers = NULL;

void driver_register(const char *name, const struct driver_ops *ops)
{
	struct driver *d;

	d = malloc(sizeof(struct driver));
	if (d == NULL) {
		printf("Failed to register driver %s!\n", name);
		return;
	}
	d->name = strdup(name);
	if (d->name == NULL) {
		printf("Failed to register driver %s!\n", name);
		free(d);
		return;
	}
	d->ops = ops;

	d->next = drivers;
	drivers = d;
}


void driver_unregister(const char *name)
{
	struct driver *p, **pp;

	for (pp = &drivers; (p = *pp) != NULL; pp = &p->next) {
		if (strcasecmp(p->name, name) == 0) {
			*pp = p->next;
			p->next = NULL;
			free(p->name);
			free(p);
			break;
		}
	}
}


static void driver_unregister_all(void)
{
	struct driver *p, *pp;
	p = drivers;
	drivers = NULL;
	while (p) {
		pp = p;
		p = p->next;
		free(pp->name);
		free(pp);
	}
}


const struct driver_ops * driver_lookup(const char *name)
{
	struct driver *p;

	if (strcmp(name, "default") == 0) {
		p = drivers;
		while (p && p->next)
			p = p->next;
		return p->ops;
	}

	for (p = drivers; p != NULL; p = p->next) {
		if (strcasecmp(p->name, name) == 0)
			return p->ops;
	}

	return NULL;
}


static void show_version(void)
{
	fprintf(stderr,
		"hostapd v" VERSION_STR "\n"
		"User space daemon for IEEE 802.11 AP management,\n"
		"IEEE 802.1X/WPA/WPA2/EAP/RADIUS Authenticator\n"
		"Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi> "
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

	hapd = wpa_zalloc(sizeof(*hapd));
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
	    (hapd->conf->ca_cert || hapd->conf->server_cert)) {
		struct tls_connection_params params;

		hapd->ssl_ctx = tls_init(NULL);
		if (hapd->ssl_ctx == NULL) {
			printf("Failed to initialize TLS\n");
			goto fail;
		}

		memset(&params, 0, sizeof(params));
		params.ca_cert = hapd->conf->ca_cert;
		params.client_cert = hapd->conf->server_cert;
		params.private_key = hapd->conf->private_key;
		params.private_key_passwd = hapd->conf->private_key_passwd;

		if (tls_global_set_params(hapd->ssl_ctx, &params)) {
			printf("Failed to set TLS parameters\n");
			goto fail;
		}

		if (tls_global_set_verify(hapd->ssl_ctx,
					  hapd->conf->check_crl)) {
			printf("Failed to enable check_crl\n");
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
			printf("Failed to initialize EAP-SIM database "
			       "interface\n");
			goto fail;
		}
	}
#endif /* EAP_SERVER */

	if (hapd->conf->assoc_ap)
		hapd->assoc_ap_state = WAIT_BEACON;

	/* FIX: need to fix this const vs. not */
	hapd->driver = (struct driver_ops *) hapd->iconf->driver;

	return hapd;

#if defined(EAP_TLS_FUNCS) || defined(EAP_SERVER)
fail:
#endif
	/* TODO: cleanup allocated resources(?) */
	free(hapd);
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

	hapd_iface = wpa_zalloc(sizeof(*hapd_iface));
	if (hapd_iface == NULL)
		goto fail;

	hapd_iface->config_fname = strdup(config_file);
	if (hapd_iface->config_fname == NULL)
		goto fail;

	conf = hostapd_config_read(hapd_iface->config_fname);
	if (conf == NULL)
		goto fail;
	hapd_iface->conf = conf;

	hapd_iface->num_bss = conf->num_bss;
	hapd_iface->bss = wpa_zalloc(conf->num_bss *
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

		free(hapd_iface->config_fname);
		free(hapd_iface->bss);
		free(hapd_iface);
	}
	return NULL;
}


/**
 * register_drivers - Register driver interfaces
 *
 * This function is generated by Makefile (into driver_conf.c) to call all
 * configured driver interfaces to register them to core hostapd.
 */
void register_drivers(void);


/**
 * setup_interface_done - Callback when an interface is done being setup.
 * @iface: Pointer to interface data.
 * @status: Status of the interface setup (0 on success; -1 on failure).
 */
static void setup_interface_done(struct hostapd_iface *iface, int status)
{
	if (status) {
		wpa_printf(MSG_DEBUG, "%s: Unable to setup interface.",
			   iface->bss[0]->conf->iface);
		eloop_terminate();
	} else
		wpa_printf(MSG_DEBUG, "%s: Setup of interface done.",
			   iface->bss[0]->conf->iface);
}


int main(int argc, char *argv[])
{
	struct hapd_interfaces interfaces;
	int ret = 1, k;
	size_t i, j;
	int c, debug = 0, daemonize = 0;
	const char *pid_file = NULL;

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
			pid_file = optarg;
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

	register_drivers();		/* NB: generated by Makefile */

	if (eap_server_register_methods()) {
		wpa_printf(MSG_ERROR, "Failed to register EAP methods");
		return -1;
	}

	interfaces.count = argc - optind;

	interfaces.iface = malloc(interfaces.count *
				  sizeof(struct hostapd_iface *));
	if (interfaces.iface == NULL) {
		printf("malloc failed\n");
		exit(1);
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
		printf("Configuration file: %s\n", argv[optind + i]);
		interfaces.iface[i] = hostapd_init(argv[optind + i]);
		if (!interfaces.iface[i])
			goto out;
		for (k = 0; k < debug; k++) {
			if (interfaces.iface[i]->bss[0]->conf->
			    logger_stdout_level > 0)
				interfaces.iface[i]->bss[0]->conf->
					logger_stdout_level--;
			interfaces.iface[i]->bss[0]->conf->debug++;
		}

		ret = hostapd_setup_interface_start(interfaces.iface[i],
						    setup_interface_done);
		if (ret)
			goto out;
	}

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
		hostapd_setup_interface_stop(interfaces.iface[i]);
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
			free(interfaces.iface[i]->bss[j]);
		hostapd_cleanup_iface(interfaces.iface[i]);
	}
	free(interfaces.iface);

	eloop_destroy();

#ifndef CONFIG_NATIVE_WINDOWS
	closelog();
#endif /* CONFIG_NATIVE_WINDOWS */

	eap_server_unregister_methods();

	driver_unregister_all();

	os_daemonize_terminate(pid_file);

	return ret;
}
