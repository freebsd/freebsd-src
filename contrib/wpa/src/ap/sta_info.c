/*
 * hostapd / Station table
 * Copyright (c) 2002-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "common/sae.h"
#include "common/dpp.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "p2p/p2p.h"
#include "fst/fst.h"
#include "crypto/crypto.h"
#include "hostapd.h"
#include "accounting.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "ieee802_11_auth.h"
#include "wpa_auth.h"
#include "preauth_auth.h"
#include "ap_config.h"
#include "beacon.h"
#include "ap_mlme.h"
#include "vlan_init.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "gas_serv.h"
#include "wnm_ap.h"
#include "mbo_ap.h"
#include "ndisc_snoop.h"
#include "sta_info.h"
#include "vlan.h"
#include "wps_hostapd.h"

static void ap_sta_remove_in_other_bss(struct hostapd_data *hapd,
				       struct sta_info *sta);
static void ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx);
static void ap_handle_session_warning_timer(void *eloop_ctx, void *timeout_ctx);
static void ap_sta_deauth_cb_timeout(void *eloop_ctx, void *timeout_ctx);
static void ap_sta_disassoc_cb_timeout(void *eloop_ctx, void *timeout_ctx);
static void ap_sa_query_timer(void *eloop_ctx, void *timeout_ctx);
static int ap_sta_remove(struct hostapd_data *hapd, struct sta_info *sta);
static void ap_sta_delayed_1x_auth_fail_cb(void *eloop_ctx, void *timeout_ctx);

int ap_for_each_sta(struct hostapd_data *hapd,
		    int (*cb)(struct hostapd_data *hapd, struct sta_info *sta,
			      void *ctx),
		    void *ctx)
{
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (cb(hapd, sta, ctx))
			return 1;
	}

	return 0;
}


struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta)];
	while (s != NULL && os_memcmp(s->addr, sta, 6) != 0)
		s = s->hnext;
	return s;
}


#ifdef CONFIG_P2P
struct sta_info * ap_get_sta_p2p(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		const u8 *p2p_dev_addr;

		if (sta->p2p_ie == NULL)
			continue;

		p2p_dev_addr = p2p_get_go_dev_addr(sta->p2p_ie);
		if (p2p_dev_addr == NULL)
			continue;

		if (ether_addr_equal(p2p_dev_addr, addr))
			return sta;
	}

	return NULL;
}
#endif /* CONFIG_P2P */


static void ap_sta_list_del(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sta_info *tmp;

	if (hapd->sta_list == sta) {
		hapd->sta_list = sta->next;
		return;
	}

	tmp = hapd->sta_list;
	while (tmp != NULL && tmp->next != sta)
		tmp = tmp->next;
	if (tmp == NULL) {
		wpa_printf(MSG_DEBUG, "Could not remove STA " MACSTR " from "
			   "list.", MAC2STR(sta->addr));
	} else
		tmp->next = sta->next;
}


void ap_sta_hash_add(struct hostapd_data *hapd, struct sta_info *sta)
{
	sta->hnext = hapd->sta_hash[STA_HASH(sta->addr)];
	hapd->sta_hash[STA_HASH(sta->addr)] = sta;
}


static void ap_sta_hash_del(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta->addr)];
	if (s == NULL) return;
	if (os_memcmp(s->addr, sta->addr, 6) == 0) {
		hapd->sta_hash[STA_HASH(sta->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL &&
	       !ether_addr_equal(s->hnext->addr, sta->addr))
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		wpa_printf(MSG_DEBUG, "AP: could not remove STA " MACSTR
			   " from hash table", MAC2STR(sta->addr));
}


void ap_sta_ip6addr_del(struct hostapd_data *hapd, struct sta_info *sta)
{
	sta_ip6addr_del(hapd, sta);
}


#ifdef CONFIG_PASN

void ap_free_sta_pasn(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->pasn) {
		wpa_printf(MSG_DEBUG, "PASN: Free PASN context: " MACSTR,
			   MAC2STR(sta->addr));

		if (sta->pasn->ecdh)
			crypto_ecdh_deinit(sta->pasn->ecdh);

		wpabuf_free(sta->pasn->secret);
		sta->pasn->secret = NULL;

#ifdef CONFIG_SAE
		sae_clear_data(&sta->pasn->sae);
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
		/* In practice this pointer should be NULL */
		wpabuf_free(sta->pasn->fils.erp_resp);
		sta->pasn->fils.erp_resp = NULL;
#endif /* CONFIG_FILS */

		pasn_data_deinit(sta->pasn);
		sta->pasn = NULL;
	}
}

#endif /* CONFIG_PASN */


static void __ap_free_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
#ifdef CONFIG_IEEE80211BE
	if (hostapd_sta_is_link_sta(hapd, sta) &&
	    !hostapd_drv_link_sta_remove(hapd, sta->addr))
		return;
#endif /* CONFIG_IEEE80211BE */

	hostapd_drv_sta_remove(hapd, sta->addr);
}


#ifdef CONFIG_IEEE80211BE
static void clear_wpa_sm_for_each_partner_link(struct hostapd_data *hapd,
					       struct sta_info *psta)
{
	struct sta_info *lsta;
	struct hostapd_data *lhapd;

	if (!ap_sta_is_mld(hapd, psta))
		return;

	for_each_mld_link(lhapd, hapd) {
		if (lhapd == hapd)
			continue;

		lsta = ap_get_sta(lhapd, psta->addr);
		if (lsta)
			lsta->wpa_sm = NULL;
	}
}
#endif /* CONFIG_IEEE80211BE */


void ap_free_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	int set_beacon = 0;

	accounting_sta_stop(hapd, sta);

	/* just in case */
	ap_sta_set_authorized(hapd, sta, 0);
	hostapd_set_sta_flags(hapd, sta);

	if ((sta->flags & WLAN_STA_WDS) ||
	    (sta->flags & WLAN_STA_MULTI_AP &&
	     (hapd->conf->multi_ap & BACKHAUL_BSS) &&
	     hapd->conf->wds_sta &&
	     !(sta->flags & WLAN_STA_WPS)))
		hostapd_set_wds_sta(hapd, NULL, sta->addr, sta->aid, 0);

	if (sta->ipaddr)
		hostapd_drv_br_delete_ip_neigh(hapd, 4, (u8 *) &sta->ipaddr);
	ap_sta_ip6addr_del(hapd, sta);

	if (!hapd->iface->driver_ap_teardown &&
	    !(sta->flags & WLAN_STA_PREAUTH)) {
		__ap_free_sta(hapd, sta);
		sta->added_unassoc = 0;
	}

	ap_sta_hash_del(hapd, sta);
	ap_sta_list_del(hapd, sta);

	if (sta->aid > 0)
		hapd->sta_aid[(sta->aid - 1) / 32] &=
			~BIT((sta->aid - 1) % 32);

	hapd->num_sta--;
	if (sta->nonerp_set) {
		sta->nonerp_set = 0;
		hapd->iface->num_sta_non_erp--;
		if (hapd->iface->num_sta_non_erp == 0)
			set_beacon++;
	}

	if (sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 0;
		hapd->iface->num_sta_no_short_slot_time--;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_slot_time == 0)
			set_beacon++;
	}

	if (sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 0;
		hapd->iface->num_sta_no_short_preamble--;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 0)
			set_beacon++;
	}

	if (sta->no_ht_gf_set) {
		sta->no_ht_gf_set = 0;
		hapd->iface->num_sta_ht_no_gf--;
	}

	if (sta->no_ht_set) {
		sta->no_ht_set = 0;
		hapd->iface->num_sta_no_ht--;
	}

	if (sta->ht_20mhz_set) {
		sta->ht_20mhz_set = 0;
		hapd->iface->num_sta_ht_20mhz--;
	}

#ifdef CONFIG_TAXONOMY
	wpabuf_free(sta->probe_ie_taxonomy);
	sta->probe_ie_taxonomy = NULL;
	wpabuf_free(sta->assoc_ie_taxonomy);
	sta->assoc_ie_taxonomy = NULL;
#endif /* CONFIG_TAXONOMY */

	ht40_intolerant_remove(hapd->iface, sta);

#ifdef CONFIG_P2P
	if (sta->no_p2p_set) {
		sta->no_p2p_set = 0;
		hapd->num_sta_no_p2p--;
		if (hapd->num_sta_no_p2p == 0)
			hostapd_p2p_non_p2p_sta_disconnected(hapd);
	}
#endif /* CONFIG_P2P */

#ifdef NEED_AP_MLME
	if (hostapd_ht_operation_update(hapd->iface) > 0)
		set_beacon++;
#endif /* NEED_AP_MLME */

#ifdef CONFIG_MESH
	if (hapd->mesh_sta_free_cb)
		hapd->mesh_sta_free_cb(hapd, sta);
#endif /* CONFIG_MESH */

	if (set_beacon)
		ieee802_11_update_beacons(hapd->iface);

	wpa_printf(MSG_DEBUG, "%s: cancel ap_handle_timer for " MACSTR,
		   __func__, MAC2STR(sta->addr));
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
	eloop_cancel_timeout(ap_handle_session_warning_timer, hapd, sta);
	ap_sta_clear_disconnect_timeouts(hapd, sta);
	sae_clear_retransmit_timer(hapd, sta);

	ieee802_1x_free_station(hapd, sta);

#ifdef CONFIG_IEEE80211BE
	if (!ap_sta_is_mld(hapd, sta) ||
	    hapd->mld_link_id == sta->mld_assoc_link_id) {
		wpa_auth_sta_deinit(sta->wpa_sm);
		/* Remove references from partner links. */
		clear_wpa_sm_for_each_partner_link(hapd, sta);
	}

	/* Release group references in case non-association link STA is removed
	 * before association link STA */
	if (hostapd_sta_is_link_sta(hapd, sta))
		wpa_release_link_auth_ref(sta->wpa_sm, hapd->mld_link_id);
#else /* CONFIG_IEEE80211BE */
	wpa_auth_sta_deinit(sta->wpa_sm);
#endif /* CONFIG_IEEE80211BE */

	rsn_preauth_free_station(hapd, sta);
#ifndef CONFIG_NO_RADIUS
	if (hapd->radius)
		radius_client_flush_auth(hapd->radius, sta->addr);
#endif /* CONFIG_NO_RADIUS */

#ifndef CONFIG_NO_VLAN
	/*
	 * sta->wpa_sm->group needs to be released before so that
	 * vlan_remove_dynamic() can check that no stations are left on the
	 * AP_VLAN netdev.
	 */
	if (sta->vlan_id)
		vlan_remove_dynamic(hapd, sta->vlan_id);
	if (sta->vlan_id_bound) {
		/*
		 * Need to remove the STA entry before potentially removing the
		 * VLAN.
		 */
		if (hapd->iface->driver_ap_teardown &&
		    !(sta->flags & WLAN_STA_PREAUTH)) {
			hostapd_drv_sta_remove(hapd, sta->addr);
			sta->added_unassoc = 0;
		}
		vlan_remove_dynamic(hapd, sta->vlan_id_bound);
	}
#endif /* CONFIG_NO_VLAN */

	os_free(sta->challenge);

	os_free(sta->sa_query_trans_id);
	eloop_cancel_timeout(ap_sa_query_timer, hapd, sta);

#ifdef CONFIG_P2P
	p2p_group_notif_disassoc(hapd->p2p_group, sta->addr);
#endif /* CONFIG_P2P */

#ifdef CONFIG_INTERWORKING
	if (sta->gas_dialog) {
		int i;

		for (i = 0; i < GAS_DIALOG_MAX; i++)
			gas_serv_dialog_clear(&sta->gas_dialog[i]);
		os_free(sta->gas_dialog);
	}
#endif /* CONFIG_INTERWORKING */

	wpabuf_free(sta->wps_ie);
	wpabuf_free(sta->p2p_ie);
	wpabuf_free(sta->hs20_ie);
	wpabuf_free(sta->roaming_consortium);
#ifdef CONFIG_FST
	wpabuf_free(sta->mb_ies);
#endif /* CONFIG_FST */

	os_free(sta->ht_capabilities);
	os_free(sta->vht_capabilities);
	os_free(sta->vht_operation);
	os_free(sta->he_capab);
	os_free(sta->he_6ghz_capab);
	os_free(sta->eht_capab);
	hostapd_free_psk_list(sta->psk);
	os_free(sta->identity);
	os_free(sta->radius_cui);
	os_free(sta->remediation_url);
	os_free(sta->t_c_url);
	wpabuf_free(sta->hs20_deauth_req);
	os_free(sta->hs20_session_info_url);

#ifdef CONFIG_SAE
	sae_clear_data(sta->sae);
	os_free(sta->sae);
#endif /* CONFIG_SAE */

	mbo_ap_sta_free(sta);
	os_free(sta->supp_op_classes);

#ifdef CONFIG_FILS
	os_free(sta->fils_pending_assoc_req);
	wpabuf_free(sta->fils_hlp_resp);
	wpabuf_free(sta->hlp_dhcp_discover);
	eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
#ifdef CONFIG_FILS_SK_PFS
	crypto_ecdh_deinit(sta->fils_ecdh);
	wpabuf_clear_free(sta->fils_dh_ss);
	wpabuf_free(sta->fils_g_sta);
#endif /* CONFIG_FILS_SK_PFS */
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	bin_clear_free(sta->owe_pmk, sta->owe_pmk_len);
	crypto_ecdh_deinit(sta->owe_ecdh);
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
	dpp_pfs_free(sta->dpp_pfs);
	sta->dpp_pfs = NULL;
#endif /* CONFIG_DPP2 */

	os_free(sta->ext_capability);

#ifdef CONFIG_WNM_AP
	eloop_cancel_timeout(ap_sta_reset_steer_flag_timer, hapd, sta);
#endif /* CONFIG_WNM_AP */

#ifdef CONFIG_PASN
	ap_free_sta_pasn(hapd, sta);
#endif /* CONFIG_PASN */

	os_free(sta->ifname_wds);

#ifdef CONFIG_IEEE80211BE
	ap_sta_free_sta_profile(&sta->mld_info);
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_TESTING_OPTIONS
	os_free(sta->sae_postponed_commit);
	forced_memzero(sta->last_tk, WPA_TK_MAX_LEN);
#endif /* CONFIG_TESTING_OPTIONS */

	os_free(sta);
}


void hostapd_free_stas(struct hostapd_data *hapd)
{
	struct sta_info *sta, *prev;

	sta = hapd->sta_list;

	while (sta) {
		prev = sta;
		if (sta->flags & WLAN_STA_AUTH) {
			mlme_deauthenticate_indication(
				hapd, sta, WLAN_REASON_UNSPECIFIED);
		}
		sta = sta->next;
		wpa_printf(MSG_DEBUG, "Removing station " MACSTR,
			   MAC2STR(prev->addr));
		ap_free_sta(hapd, prev);
	}
}


#ifdef CONFIG_IEEE80211BE
void hostapd_free_link_stas(struct hostapd_data *hapd)
{
	struct sta_info *sta, *prev;

	sta = hapd->sta_list;
	while (sta) {
		prev = sta;
		sta = sta->next;

		if (!hostapd_sta_is_link_sta(hapd, prev))
			continue;

		wpa_printf(MSG_DEBUG, "Removing link station from MLD " MACSTR,
			   MAC2STR(prev->addr));
		ap_free_sta(hapd, prev);
	}
}
#endif /* CONFIG_IEEE80211BE */


/**
 * ap_handle_timer - Per STA timer handler
 * @eloop_ctx: struct hostapd_data *
 * @timeout_ctx: struct sta_info *
 *
 * This function is called to check station activity and to remove inactive
 * stations.
 */
void ap_handle_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	unsigned long next_time = 0;
	int reason;
	int max_inactivity = hapd->conf->ap_max_inactivity;

	wpa_printf(MSG_DEBUG, "%s: %s: " MACSTR " flags=0x%x timeout_next=%d",
		   hapd->conf->iface, __func__, MAC2STR(sta->addr), sta->flags,
		   sta->timeout_next);
	if (sta->timeout_next == STA_REMOVE) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "local deauth request");
		ap_free_sta(hapd, sta);
		return;
	}

	if (sta->max_idle_period)
		max_inactivity = (sta->max_idle_period * 1024 + 999) / 1000;

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    (sta->timeout_next == STA_NULLFUNC ||
	     sta->timeout_next == STA_DISASSOC)) {
		int inactive_sec;
		/*
		 * Add random value to timeout so that we don't end up bouncing
		 * all stations at the same time if we have lots of associated
		 * stations that are idle (but keep re-associating).
		 */
		int fuzz = os_random() % 20;
		inactive_sec = hostapd_drv_get_inact_sec(hapd, sta->addr);
		if (inactive_sec == -1) {
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Check inactivity: Could not "
				"get station info from kernel driver for "
				MACSTR, MAC2STR(sta->addr));
			/*
			 * The driver may not support this functionality.
			 * Anyway, try again after the next inactivity timeout,
			 * but do not disconnect the station now.
			 */
			next_time = max_inactivity + fuzz;
		} else if (inactive_sec == -ENOENT) {
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Station " MACSTR " has lost its driver entry",
				MAC2STR(sta->addr));

			/* Avoid sending client probe on removed client */
			sta->timeout_next = STA_DISASSOC;
			goto skip_poll;
		} else if (inactive_sec < max_inactivity) {
			/* station activity detected; reset timeout state */
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Station " MACSTR " has been active %is ago",
				MAC2STR(sta->addr), inactive_sec);
			sta->timeout_next = STA_NULLFUNC;
			next_time = max_inactivity + fuzz - inactive_sec;
		} else {
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Station " MACSTR " has been "
				"inactive too long: %d sec, max allowed: %d",
				MAC2STR(sta->addr), inactive_sec,
				max_inactivity);

			if (hapd->conf->skip_inactivity_poll)
				sta->timeout_next = STA_DISASSOC;
		}
	}

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    sta->timeout_next == STA_DISASSOC &&
	    !(sta->flags & WLAN_STA_PENDING_POLL) &&
	    !hapd->conf->skip_inactivity_poll) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Station " MACSTR
			" has ACKed data poll", MAC2STR(sta->addr));
		/* data nullfunc frame poll did not produce TX errors; assume
		 * station ACKed it */
		sta->timeout_next = STA_NULLFUNC;
		next_time = max_inactivity;
	}

skip_poll:
	if (next_time) {
		wpa_printf(MSG_DEBUG, "%s: register ap_handle_timer timeout "
			   "for " MACSTR " (%lu seconds)",
			   __func__, MAC2STR(sta->addr), next_time);
		eloop_register_timeout(next_time, 0, ap_handle_timer, hapd,
				       sta);
		return;
	}

	if (sta->timeout_next == STA_NULLFUNC &&
	    (sta->flags & WLAN_STA_ASSOC)) {
		wpa_printf(MSG_DEBUG, "  Polling STA");
		sta->flags |= WLAN_STA_PENDING_POLL;
		hostapd_drv_poll_client(hapd, hapd->own_addr, sta->addr,
					sta->flags & WLAN_STA_WMM);
	} else if (sta->timeout_next != STA_REMOVE) {
		int deauth = sta->timeout_next == STA_DEAUTH;

		if (!deauth && !(sta->flags & WLAN_STA_ASSOC)) {
			/* Cannot disassociate not-associated STA, so move
			 * directly to deauthentication. */
			sta->timeout_next = STA_DEAUTH;
			deauth = 1;
		}

		wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
			"Timeout, sending %s info to STA " MACSTR,
			deauth ? "deauthentication" : "disassociation",
			MAC2STR(sta->addr));

		if (deauth) {
			hostapd_drv_sta_deauth(
				hapd, sta->addr,
				WLAN_REASON_PREV_AUTH_NOT_VALID);
		} else {
			reason = (sta->timeout_next == STA_DISASSOC) ?
				WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY :
				WLAN_REASON_PREV_AUTH_NOT_VALID;

			hostapd_drv_sta_disassoc(hapd, sta->addr, reason);
		}
	}

	switch (sta->timeout_next) {
	case STA_NULLFUNC:
		sta->timeout_next = STA_DISASSOC;
		wpa_printf(MSG_DEBUG, "%s: register ap_handle_timer timeout "
			   "for " MACSTR " (%d seconds - AP_DISASSOC_DELAY)",
			   __func__, MAC2STR(sta->addr), AP_DISASSOC_DELAY);
		eloop_register_timeout(AP_DISASSOC_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		break;
	case STA_DISASSOC:
	case STA_DISASSOC_FROM_CLI:
		ap_sta_set_authorized(hapd, sta, 0);
		sta->flags &= ~WLAN_STA_ASSOC;
		hostapd_set_sta_flags(hapd, sta);
		ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		accounting_sta_stop(hapd, sta);
		ieee802_1x_free_station(hapd, sta);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "disassociated due to "
			       "inactivity");
		reason = (sta->timeout_next == STA_DISASSOC) ?
			WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY :
			WLAN_REASON_PREV_AUTH_NOT_VALID;
		sta->timeout_next = STA_DEAUTH;
		wpa_printf(MSG_DEBUG, "%s: register ap_handle_timer timeout "
			   "for " MACSTR " (%d seconds - AP_DEAUTH_DELAY)",
			   __func__, MAC2STR(sta->addr), AP_DEAUTH_DELAY);
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		mlme_disassociate_indication(hapd, sta, reason);
		break;
	case STA_DEAUTH:
	case STA_REMOVE:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "inactivity (timer DEAUTH/REMOVE)");
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		mlme_deauthenticate_indication(
			hapd, sta,
			WLAN_REASON_PREV_AUTH_NOT_VALID);
		ap_free_sta(hapd, sta);
		break;
	}
}


static void ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	wpa_printf(MSG_DEBUG, "%s: Session timer for STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));
	if (!(sta->flags & (WLAN_STA_AUTH | WLAN_STA_ASSOC |
			    WLAN_STA_AUTHORIZED))) {
		if (sta->flags & WLAN_STA_GAS) {
			wpa_printf(MSG_DEBUG, "GAS: Remove temporary STA "
				   "entry " MACSTR, MAC2STR(sta->addr));
			ap_free_sta(hapd, sta);
		}
		return;
	}

	hostapd_drv_sta_deauth(hapd, sta->addr,
			       WLAN_REASON_PREV_AUTH_NOT_VALID);
	mlme_deauthenticate_indication(hapd, sta,
				       WLAN_REASON_PREV_AUTH_NOT_VALID);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
		       "session timeout");
	sta->acct_terminate_cause =
		RADIUS_ACCT_TERMINATE_CAUSE_SESSION_TIMEOUT;
	ap_free_sta(hapd, sta);
}


void ap_sta_replenish_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			      u32 session_timeout)
{
	if (eloop_replenish_timeout(session_timeout, 0,
				    ap_handle_session_timer, hapd, sta) == 1) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "setting session timeout "
			       "to %d seconds", session_timeout);
	}
}


void ap_sta_session_timeout(struct hostapd_data *hapd, struct sta_info *sta,
			    u32 session_timeout)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "setting session timeout to %d "
		       "seconds", session_timeout);
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
	eloop_register_timeout(session_timeout, 0, ap_handle_session_timer,
			       hapd, sta);
}


void ap_sta_no_session_timeout(struct hostapd_data *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
}


static void ap_handle_session_warning_timer(void *eloop_ctx, void *timeout_ctx)
{
#ifdef CONFIG_WNM_AP
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	wpa_printf(MSG_DEBUG, "%s: WNM: Session warning time reached for "
		   MACSTR, hapd->conf->iface, MAC2STR(sta->addr));
	if (sta->hs20_session_info_url == NULL)
		return;

	wnm_send_ess_disassoc_imminent(hapd, sta, sta->hs20_session_info_url,
				       sta->hs20_disassoc_timer);
#endif /* CONFIG_WNM_AP */
}


void ap_sta_session_warning_timeout(struct hostapd_data *hapd,
				    struct sta_info *sta, int warning_time)
{
	eloop_cancel_timeout(ap_handle_session_warning_timer, hapd, sta);
	eloop_register_timeout(warning_time, 0, ap_handle_session_warning_timer,
			       hapd, sta);
}


struct sta_info * ap_sta_add(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;
	int i;
	int max_inactivity = hapd->conf->ap_max_inactivity;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return sta;

	wpa_printf(MSG_DEBUG, "  New STA");
	if (hapd->num_sta >= hapd->conf->max_num_sta) {
		/* FIX: might try to remove some old STAs first? */
		wpa_printf(MSG_DEBUG, "no more room for new STAs (%d/%d)",
			   hapd->num_sta, hapd->conf->max_num_sta);
		return NULL;
	}

	sta = os_zalloc(sizeof(struct sta_info));
	if (sta == NULL) {
		wpa_printf(MSG_ERROR, "malloc failed");
		return NULL;
	}
	sta->acct_interim_interval = hapd->conf->acct_interim_interval;
	if (accounting_sta_get_id(hapd, sta) < 0) {
		os_free(sta);
		return NULL;
	}

	for (i = 0; i < WLAN_SUPP_RATES_MAX; i++) {
		if (!hapd->iface->basic_rates)
			break;
		if (hapd->iface->basic_rates[i] < 0)
			break;
		sta->supported_rates[i] = hapd->iface->basic_rates[i] / 5;
	}
	sta->supported_rates_len = i;

	if (sta->max_idle_period)
		max_inactivity = (sta->max_idle_period * 1024 + 999) / 1000;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_INACTIVITY_TIMER)) {
		wpa_printf(MSG_DEBUG, "%s: register ap_handle_timer timeout "
			   "for " MACSTR " (%d seconds - ap_max_inactivity)",
			   __func__, MAC2STR(addr),
			   max_inactivity);
		eloop_register_timeout(max_inactivity, 0,
				       ap_handle_timer, hapd, sta);
	}

	/* initialize STA info data */
	os_memcpy(sta->addr, addr, ETH_ALEN);
	sta->next = hapd->sta_list;
	hapd->sta_list = sta;
	hapd->num_sta++;
	ap_sta_hash_add(hapd, sta);
	ap_sta_remove_in_other_bss(hapd, sta);
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;
	dl_list_init(&sta->ip6addr);

#ifdef CONFIG_TAXONOMY
	sta_track_claim_taxonomy_info(hapd->iface, addr,
				      &sta->probe_ie_taxonomy);
#endif /* CONFIG_TAXONOMY */

	return sta;
}


static int ap_sta_remove(struct hostapd_data *hapd, struct sta_info *sta)
{
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);

	if (sta->ipaddr)
		hostapd_drv_br_delete_ip_neigh(hapd, 4, (u8 *) &sta->ipaddr);
	ap_sta_ip6addr_del(hapd, sta);

	wpa_printf(MSG_DEBUG, "%s: Removing STA " MACSTR " from kernel driver",
		   hapd->conf->iface, MAC2STR(sta->addr));
	if (hostapd_drv_sta_remove(hapd, sta->addr) &&
	    sta->flags & WLAN_STA_ASSOC) {
		wpa_printf(MSG_DEBUG, "%s: Could not remove station " MACSTR
			   " from kernel driver",
			   hapd->conf->iface, MAC2STR(sta->addr));
		return -1;
	}
	sta->added_unassoc = 0;
	return 0;
}


static void ap_sta_remove_in_other_bss(struct hostapd_data *hapd,
				       struct sta_info *sta)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t i;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		struct sta_info *sta2;
		/* bss should always be set during operation, but it may be
		 * NULL during reconfiguration. Assume the STA is not
		 * associated to another BSS in that case to avoid NULL pointer
		 * dereferences. */
		if (bss == hapd || bss == NULL)
			continue;
		sta2 = ap_get_sta(bss, sta->addr);
		if (!sta2)
			continue;

		wpa_printf(MSG_DEBUG, "%s: disconnect old STA " MACSTR
			   " association from another BSS %s",
			   hapd->conf->iface, MAC2STR(sta2->addr),
			   bss->conf->iface);
		ap_sta_disconnect(bss, sta2, sta2->addr,
				  WLAN_REASON_PREV_AUTH_NOT_VALID);
	}
}


static void ap_sta_disassoc_cb_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	wpa_printf(MSG_DEBUG, "%s: Disassociation callback for STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));
	ap_sta_remove(hapd, sta);
	mlme_disassociate_indication(hapd, sta, sta->disassoc_reason);
}


static void ap_sta_disconnect_common(struct hostapd_data *hapd,
				     struct sta_info *sta, unsigned int timeout)
{
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;

	ap_sta_set_authorized(hapd, sta, 0);
	hostapd_set_sta_flags(hapd, sta);

	wpa_printf(MSG_DEBUG,
		   "reschedule ap_handle_timer timeout (%u sec) for " MACSTR,
		   MAC2STR(sta->addr), timeout);

	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(timeout, 0, ap_handle_timer, hapd, sta);
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(hapd, sta);
#ifdef CONFIG_IEEE80211BE
	if (!hapd->conf->mld_ap ||
	    hapd->mld_link_id == sta->mld_assoc_link_id) {
		wpa_auth_sta_deinit(sta->wpa_sm);
		clear_wpa_sm_for_each_partner_link(hapd, sta);
	}
#else /* CONFIG_IEEE80211BE */
	wpa_auth_sta_deinit(sta->wpa_sm);
#endif /* CONFIG_IEEE80211BE */

	sta->wpa_sm = NULL;
}


static void ap_sta_handle_disassociate(struct hostapd_data *hapd,
				       struct sta_info *sta, u16 reason)
{
	wpa_printf(MSG_DEBUG, "%s: disassociate STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD) {
		/* Skip deauthentication in DMG/IEEE 802.11ad */
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC |
				WLAN_STA_ASSOC_REQ_OK);
		sta->timeout_next = STA_REMOVE;
	} else {
		sta->flags &= ~(WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK);
		sta->timeout_next = STA_DEAUTH;
	}

	ap_sta_disconnect_common(hapd, sta, AP_MAX_INACTIVITY_AFTER_DISASSOC);

	sta->disassoc_reason = reason;
	sta->flags |= WLAN_STA_PENDING_DISASSOC_CB;
	eloop_cancel_timeout(ap_sta_disassoc_cb_timeout, hapd, sta);
	eloop_register_timeout(hapd->iface->drv_flags &
			       WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS ? 2 : 0, 0,
			       ap_sta_disassoc_cb_timeout, hapd, sta);
}


static void ap_sta_deauth_cb_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	wpa_printf(MSG_DEBUG, "%s: Deauthentication callback for STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));
	ap_sta_remove(hapd, sta);
	mlme_deauthenticate_indication(hapd, sta, sta->deauth_reason);
}


static void ap_sta_handle_deauthenticate(struct hostapd_data *hapd,
					 struct sta_info *sta, u16 reason)
{
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD) {
		/* Deauthentication is not used in DMG/IEEE 802.11ad;
		 * disassociate the STA instead. */
		ap_sta_disassociate(hapd, sta, reason);
		return;
	}

	wpa_printf(MSG_DEBUG, "%s: deauthenticate STA " MACSTR,
		   hapd->conf->iface, MAC2STR(sta->addr));

	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK);

	sta->timeout_next = STA_REMOVE;
	ap_sta_disconnect_common(hapd, sta, AP_MAX_INACTIVITY_AFTER_DEAUTH);

	sta->deauth_reason = reason;
	sta->flags |= WLAN_STA_PENDING_DEAUTH_CB;
	eloop_cancel_timeout(ap_sta_deauth_cb_timeout, hapd, sta);
	eloop_register_timeout(hapd->iface->drv_flags &
			       WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS ? 2 : 0, 0,
			       ap_sta_deauth_cb_timeout, hapd, sta);
}


static bool ap_sta_ml_disconnect(struct hostapd_data *hapd,
				 struct sta_info *sta, u16 reason,
				 bool disassoc)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *assoc_hapd, *tmp_hapd;
	struct sta_info *assoc_sta;
	unsigned int i, link_id;
	struct hapd_interfaces *interfaces;

	if (!hostapd_is_mld_ap(hapd))
		return false;

	/*
	 * Get the station on which the association was performed, as it holds
	 * the information about all the other links.
	 */
	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta)
		return false;
	interfaces = assoc_hapd->iface->interfaces;

	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (!assoc_sta->mld_info.links[link_id].valid)
			continue;

		for (i = 0; i < interfaces->count; i++) {
			struct sta_info *tmp_sta;

			tmp_hapd = interfaces->iface[i]->bss[0];

			if (!hostapd_is_ml_partner(tmp_hapd, assoc_hapd))
				continue;

			for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
			     tmp_sta = tmp_sta->next) {
				/*
				 * Handle the station on which the association
				 * was done only after all other link station
				 * are removed. Since there is a only a single
				 * station per hapd with the same association
				 * link simply break;
				 */
				if (tmp_sta == assoc_sta)
					break;

				if (tmp_sta->mld_assoc_link_id !=
				    assoc_sta->mld_assoc_link_id ||
				    tmp_sta->aid != assoc_sta->aid)
					continue;

				if (disassoc)
					ap_sta_handle_disassociate(tmp_hapd,
								   tmp_sta,
								   reason);
				else
					ap_sta_handle_deauthenticate(tmp_hapd,
								     tmp_sta,
								     reason);

				break;
			}
		}
	}

	/* Disconnect the station on which the association was performed. */
	if (disassoc)
		ap_sta_handle_disassociate(assoc_hapd, assoc_sta, reason);
	else
		ap_sta_handle_deauthenticate(assoc_hapd, assoc_sta, reason);

	return true;
#else /* CONFIG_IEEE80211BE */
	return false;
#endif /* CONFIG_IEEE80211BE */
}


void ap_sta_disassociate(struct hostapd_data *hapd, struct sta_info *sta,
			 u16 reason)
{
	if (ap_sta_ml_disconnect(hapd, sta, reason, true))
		return;

	ap_sta_handle_disassociate(hapd, sta, reason);
}


void ap_sta_deauthenticate(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 reason)
{
	if (ap_sta_ml_disconnect(hapd, sta, reason, false))
		return;

	ap_sta_handle_deauthenticate(hapd, sta, reason);
}


#ifdef CONFIG_WPS
int ap_sta_wps_cancel(struct hostapd_data *hapd,
		      struct sta_info *sta, void *ctx)
{
	if (sta && (sta->flags & WLAN_STA_WPS)) {
		ap_sta_deauthenticate(hapd, sta,
				      WLAN_REASON_PREV_AUTH_NOT_VALID);
		wpa_printf(MSG_DEBUG, "WPS: %s: Deauth sta=" MACSTR,
			   __func__, MAC2STR(sta->addr));
		return 1;
	}

	return 0;
}
#endif /* CONFIG_WPS */


static int ap_sta_get_free_vlan_id(struct hostapd_data *hapd)
{
	struct hostapd_vlan *vlan;
	int vlan_id = MAX_VLAN_ID + 2;

retry:
	for (vlan = hapd->conf->vlan; vlan; vlan = vlan->next) {
		if (vlan->vlan_id == vlan_id) {
			vlan_id++;
			goto retry;
		}
	}
	return vlan_id;
}


int ap_sta_set_vlan(struct hostapd_data *hapd, struct sta_info *sta,
		    struct vlan_description *vlan_desc)
{
	struct hostapd_vlan *vlan = NULL, *wildcard_vlan = NULL;
	int old_vlan_id, vlan_id = 0, ret = 0;

	/* Check if there is something to do */
	if (hapd->conf->ssid.per_sta_vif && !sta->vlan_id) {
		/* This sta is lacking its own vif */
	} else if (hapd->conf->ssid.dynamic_vlan == DYNAMIC_VLAN_DISABLED &&
		   !hapd->conf->ssid.per_sta_vif && sta->vlan_id) {
		/* sta->vlan_id needs to be reset */
	} else if (!vlan_compare(vlan_desc, sta->vlan_desc)) {
		return 0; /* nothing to change */
	}

	/* Now the real VLAN changed or the STA just needs its own vif */
	if (hapd->conf->ssid.per_sta_vif) {
		/* Assign a new vif, always */
		/* find a free vlan_id sufficiently big */
		vlan_id = ap_sta_get_free_vlan_id(hapd);
		/* Get wildcard VLAN */
		for (vlan = hapd->conf->vlan; vlan; vlan = vlan->next) {
			if (vlan->vlan_id == VLAN_ID_WILDCARD)
				break;
		}
		if (!vlan) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "per_sta_vif missing wildcard");
			vlan_id = 0;
			ret = -1;
			goto done;
		}
	} else if (vlan_desc && vlan_desc->notempty) {
		for (vlan = hapd->conf->vlan; vlan; vlan = vlan->next) {
			if (!vlan_compare(&vlan->vlan_desc, vlan_desc))
				break;
			if (vlan->vlan_id == VLAN_ID_WILDCARD)
				wildcard_vlan = vlan;
		}
		if (vlan) {
			vlan_id = vlan->vlan_id;
		} else if (wildcard_vlan) {
			vlan = wildcard_vlan;
			vlan_id = vlan_desc->untagged;
			if (vlan_desc->tagged[0]) {
				/* Tagged VLAN configuration */
				vlan_id = ap_sta_get_free_vlan_id(hapd);
			}
		} else {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "missing vlan and wildcard for vlan=%d%s",
				       vlan_desc->untagged,
				       vlan_desc->tagged[0] ? "+" : "");
			vlan_id = 0;
			ret = -1;
			goto done;
		}
	}

	if (vlan && vlan->vlan_id == VLAN_ID_WILDCARD) {
		vlan = vlan_add_dynamic(hapd, vlan, vlan_id, vlan_desc);
		if (vlan == NULL) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "could not add dynamic VLAN interface for vlan=%d%s",
				       vlan_desc ? vlan_desc->untagged : -1,
				       (vlan_desc && vlan_desc->tagged[0]) ?
				       "+" : "");
			vlan_id = 0;
			ret = -1;
			goto done;
		}

		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "added new dynamic VLAN interface '%s'",
			       vlan->ifname);
	} else if (vlan && vlan->dynamic_vlan > 0) {
		vlan->dynamic_vlan++;
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "updated existing dynamic VLAN interface '%s'",
			       vlan->ifname);
	}
done:
	old_vlan_id = sta->vlan_id;
	sta->vlan_id = vlan_id;
	sta->vlan_desc = vlan ? &vlan->vlan_desc : NULL;

	if (vlan_id != old_vlan_id && old_vlan_id)
		vlan_remove_dynamic(hapd, old_vlan_id);

	return ret;
}


int ap_sta_bind_vlan(struct hostapd_data *hapd, struct sta_info *sta)
{
#ifndef CONFIG_NO_VLAN
	const char *iface;
	struct hostapd_vlan *vlan = NULL;
	int ret;
	int old_vlanid = sta->vlan_id_bound;
	int mld_link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		mld_link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if ((sta->flags & WLAN_STA_WDS) && sta->vlan_id == 0) {
		wpa_printf(MSG_DEBUG,
			   "Do not override WDS VLAN assignment for STA "
			   MACSTR, MAC2STR(sta->addr));
		return 0;
	}

	iface = hapd->conf->iface;
	if (hapd->conf->ssid.vlan[0])
		iface = hapd->conf->ssid.vlan;

	if (sta->vlan_id > 0) {
		for (vlan = hapd->conf->vlan; vlan; vlan = vlan->next) {
			if (vlan->vlan_id == sta->vlan_id)
				break;
		}
		if (vlan)
			iface = vlan->ifname;
	}

	/*
	 * Do not increment ref counters if the VLAN ID remains same, but do
	 * not skip hostapd_drv_set_sta_vlan() as hostapd_drv_sta_remove() might
	 * have been called before.
	 */
	if (sta->vlan_id == old_vlanid)
		goto skip_counting;

	if (sta->vlan_id > 0 && !vlan &&
	    !(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "could not find VLAN for "
			       "binding station to (vlan_id=%d)",
			       sta->vlan_id);
		ret = -1;
		goto done;
	} else if (vlan && vlan->dynamic_vlan > 0) {
		vlan->dynamic_vlan++;
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "updated existing dynamic VLAN interface '%s'",
			       iface);
	}

	/* ref counters have been increased, so mark the station */
	sta->vlan_id_bound = sta->vlan_id;

skip_counting:
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "binding station to interface "
		       "'%s'", iface);

	if (wpa_auth_sta_set_vlan(sta->wpa_sm, sta->vlan_id) < 0)
		wpa_printf(MSG_INFO, "Failed to update VLAN-ID for WPA");

	ret = hostapd_drv_set_sta_vlan(iface, hapd, sta->addr, sta->vlan_id,
				       mld_link_id);
	if (ret < 0) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "could not bind the STA "
			       "entry to vlan_id=%d", sta->vlan_id);
	}

	/* During 1x reauth, if the vlan id changes, then remove the old id. */
	if (old_vlanid > 0 && old_vlanid != sta->vlan_id)
		vlan_remove_dynamic(hapd, old_vlanid);
done:

	return ret;
#else /* CONFIG_NO_VLAN */
	return 0;
#endif /* CONFIG_NO_VLAN */
}


int ap_check_sa_query_timeout(struct hostapd_data *hapd, struct sta_info *sta)
{
	u32 tu;
	struct os_reltime now, passed;
	os_get_reltime(&now);
	os_reltime_sub(&now, &sta->sa_query_start, &passed);
	tu = (passed.sec * 1000000 + passed.usec) / 1024;
	if (hapd->conf->assoc_sa_query_max_timeout < tu) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "association SA Query timed out");
		sta->sa_query_timed_out = 1;
		os_free(sta->sa_query_trans_id);
		sta->sa_query_trans_id = NULL;
		sta->sa_query_count = 0;
		eloop_cancel_timeout(ap_sa_query_timer, hapd, sta);
		return 1;
	}

	return 0;
}


static void ap_sa_query_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	unsigned int timeout, sec, usec;
	u8 *trans_id, *nbuf;

	wpa_printf(MSG_DEBUG, "%s: SA Query timer for STA " MACSTR
		   " (count=%d)",
		   hapd->conf->iface, MAC2STR(sta->addr), sta->sa_query_count);

	if (sta->sa_query_count > 0 &&
	    ap_check_sa_query_timeout(hapd, sta))
		return;
	if (sta->sa_query_count >= 1000)
		return;

	nbuf = os_realloc_array(sta->sa_query_trans_id,
				sta->sa_query_count + 1,
				WLAN_SA_QUERY_TR_ID_LEN);
	if (nbuf == NULL)
		return;
	if (sta->sa_query_count == 0) {
		/* Starting a new SA Query procedure */
		os_get_reltime(&sta->sa_query_start);
	}
	trans_id = nbuf + sta->sa_query_count * WLAN_SA_QUERY_TR_ID_LEN;
	sta->sa_query_trans_id = nbuf;
	sta->sa_query_count++;

	if (os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0) {
		/*
		 * We don't really care which ID is used here, so simply
		 * hardcode this if the mostly theoretical os_get_random()
		 * failure happens.
		 */
		trans_id[0] = 0x12;
		trans_id[1] = 0x34;
	}

	timeout = hapd->conf->assoc_sa_query_retry_timeout;
	sec = ((timeout / 1000) * 1024) / 1000;
	usec = (timeout % 1000) * 1024;
	eloop_register_timeout(sec, usec, ap_sa_query_timer, hapd, sta);

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "association SA Query attempt %d", sta->sa_query_count);

	ieee802_11_send_sa_query_req(hapd, sta->addr, trans_id);
}


void ap_sta_start_sa_query(struct hostapd_data *hapd, struct sta_info *sta)
{
	ap_sa_query_timer(hapd, sta);
}


void ap_sta_stop_sa_query(struct hostapd_data *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(ap_sa_query_timer, hapd, sta);
	os_free(sta->sa_query_trans_id);
	sta->sa_query_trans_id = NULL;
	sta->sa_query_count = 0;
}


const char * ap_sta_wpa_get_keyid(struct hostapd_data *hapd,
				  struct sta_info *sta)
{
	struct hostapd_wpa_psk *psk;
	struct hostapd_ssid *ssid;
	const u8 *pmk;
	int pmk_len;

	ssid = &hapd->conf->ssid;

	pmk = wpa_auth_get_pmk(sta->wpa_sm, &pmk_len);
	if (!pmk || pmk_len != PMK_LEN)
		return NULL;

	for (psk = ssid->wpa_psk; psk; psk = psk->next)
		if (os_memcmp(pmk, psk->psk, PMK_LEN) == 0)
			break;
	if (!psk || !psk->keyid[0])
		return NULL;

	return psk->keyid;
}


const u8 * ap_sta_wpa_get_dpp_pkhash(struct hostapd_data *hapd,
				     struct sta_info *sta)
{
	return wpa_auth_get_dpp_pkhash(sta->wpa_sm);
}


bool ap_sta_set_authorized_flag(struct hostapd_data *hapd, struct sta_info *sta,
				int authorized)
{
	if (!!authorized == !!(sta->flags & WLAN_STA_AUTHORIZED))
		return false;

	if (authorized) {
		int mld_assoc_link_id = -1;

#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta)) {
			if (sta->mld_assoc_link_id == hapd->mld_link_id)
				mld_assoc_link_id = sta->mld_assoc_link_id;
			else
				mld_assoc_link_id = -2;
		}
#endif /* CONFIG_IEEE80211BE */
		if (mld_assoc_link_id != -2)
			hostapd_prune_associations(hapd, sta->addr,
						   mld_assoc_link_id);
		sta->flags |= WLAN_STA_AUTHORIZED;
	} else {
		sta->flags &= ~WLAN_STA_AUTHORIZED;
	}

	return true;
}


void ap_sta_set_authorized_event(struct hostapd_data *hapd,
				 struct sta_info *sta, int authorized)
{
	const u8 *dev_addr = NULL;
	char buf[100];
#ifdef CONFIG_P2P
	u8 addr[ETH_ALEN];
	u8 ip_addr_buf[4];
#endif /* CONFIG_P2P */
	const u8 *ip_ptr = NULL;

#ifdef CONFIG_P2P
	if (hapd->p2p_group == NULL) {
		if (sta->p2p_ie != NULL &&
		    p2p_parse_dev_addr_in_p2p_ie(sta->p2p_ie, addr) == 0)
			dev_addr = addr;
	} else
		dev_addr = p2p_group_get_dev_addr(hapd->p2p_group, sta->addr);

	if (dev_addr)
		os_snprintf(buf, sizeof(buf), MACSTR " p2p_dev_addr=" MACSTR,
			    MAC2STR(sta->addr), MAC2STR(dev_addr));
	else
#endif /* CONFIG_P2P */
		os_snprintf(buf, sizeof(buf), MACSTR, MAC2STR(sta->addr));

	if (authorized) {
		const u8 *dpp_pkhash;
		const char *keyid;
		char dpp_pkhash_buf[100];
		char keyid_buf[100];
		char ip_addr[100];

		dpp_pkhash_buf[0] = '\0';
		keyid_buf[0] = '\0';
		ip_addr[0] = '\0';
#ifdef CONFIG_P2P
		if (wpa_auth_get_ip_addr(sta->wpa_sm, ip_addr_buf) == 0) {
			os_snprintf(ip_addr, sizeof(ip_addr),
				    " ip_addr=%u.%u.%u.%u",
				    ip_addr_buf[0], ip_addr_buf[1],
				    ip_addr_buf[2], ip_addr_buf[3]);
			ip_ptr = ip_addr_buf;
		}
#endif /* CONFIG_P2P */

		keyid = ap_sta_wpa_get_keyid(hapd, sta);
		if (keyid) {
			os_snprintf(keyid_buf, sizeof(keyid_buf),
				    " keyid=%s", keyid);
		}

		dpp_pkhash = ap_sta_wpa_get_dpp_pkhash(hapd, sta);
		if (dpp_pkhash) {
			const char *prefix = " dpp_pkhash=";
			size_t plen = os_strlen(prefix);

			os_strlcpy(dpp_pkhash_buf, prefix,
				   sizeof(dpp_pkhash_buf));
			wpa_snprintf_hex(&dpp_pkhash_buf[plen],
					 sizeof(dpp_pkhash_buf) - plen,
					 dpp_pkhash, SHA256_MAC_LEN);
		}

		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_STA_CONNECTED "%s%s%s%s",
			buf, ip_addr, keyid_buf, dpp_pkhash_buf);

		if (hapd->msg_ctx_parent &&
		    hapd->msg_ctx_parent != hapd->msg_ctx)
			wpa_msg_no_global(hapd->msg_ctx_parent, MSG_INFO,
					  AP_STA_CONNECTED "%s%s%s%s",
					  buf, ip_addr, keyid_buf,
					  dpp_pkhash_buf);
	} else {
		wpa_msg(hapd->msg_ctx, MSG_INFO, AP_STA_DISCONNECTED "%s", buf);

		if (hapd->msg_ctx_parent &&
		    hapd->msg_ctx_parent != hapd->msg_ctx)
			wpa_msg_no_global(hapd->msg_ctx_parent, MSG_INFO,
					  AP_STA_DISCONNECTED "%s", buf);
	}

	if (hapd->sta_authorized_cb)
		hapd->sta_authorized_cb(hapd->sta_authorized_cb_ctx,
					sta->addr, authorized, dev_addr,
					ip_ptr);

#ifdef CONFIG_FST
	if (hapd->iface->fst) {
		if (authorized)
			fst_notify_peer_connected(hapd->iface->fst, sta->addr);
		else
			fst_notify_peer_disconnected(hapd->iface->fst,
						     sta->addr);
	}
#endif /* CONFIG_FST */
}


void ap_sta_set_authorized(struct hostapd_data *hapd, struct sta_info *sta,
			   int authorized)
{
	if (!ap_sta_set_authorized_flag(hapd, sta, authorized))
		return;
	ap_sta_set_authorized_event(hapd, sta, authorized);
}


void ap_sta_disconnect(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *addr, u16 reason)
{
	if (sta)
		wpa_printf(MSG_DEBUG, "%s: %s STA " MACSTR " reason=%u",
			   hapd->conf->iface, __func__, MAC2STR(sta->addr),
			   reason);
	else if (addr)
		wpa_printf(MSG_DEBUG, "%s: %s addr " MACSTR " reason=%u",
			   hapd->conf->iface, __func__, MAC2STR(addr),
			   reason);

	if (sta == NULL && addr)
		sta = ap_get_sta(hapd, addr);

	if (addr)
		hostapd_drv_sta_deauth(hapd, addr, reason);

	if (sta == NULL)
		return;
	ap_sta_set_authorized(hapd, sta, 0);
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	wpa_printf(MSG_DEBUG, "%s: %s: reschedule ap_handle_timer timeout "
		   "for " MACSTR " (%d seconds - "
		   "AP_MAX_INACTIVITY_AFTER_DEAUTH)",
		   hapd->conf->iface, __func__, MAC2STR(sta->addr),
		   AP_MAX_INACTIVITY_AFTER_DEAUTH);
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);
	eloop_register_timeout(AP_MAX_INACTIVITY_AFTER_DEAUTH, 0,
			       ap_handle_timer, hapd, sta);
	sta->timeout_next = STA_REMOVE;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD) {
		/* Deauthentication is not used in DMG/IEEE 802.11ad;
		 * disassociate the STA instead. */
		sta->disassoc_reason = reason;
		sta->flags |= WLAN_STA_PENDING_DISASSOC_CB;
		eloop_cancel_timeout(ap_sta_disassoc_cb_timeout, hapd, sta);
		eloop_register_timeout(hapd->iface->drv_flags &
				       WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS ?
				       2 : 0, 0, ap_sta_disassoc_cb_timeout,
				       hapd, sta);
		return;
	}

	sta->deauth_reason = reason;
	sta->flags |= WLAN_STA_PENDING_DEAUTH_CB;
	eloop_cancel_timeout(ap_sta_deauth_cb_timeout, hapd, sta);
	eloop_register_timeout(hapd->iface->drv_flags &
			       WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS ? 2 : 0, 0,
			       ap_sta_deauth_cb_timeout, hapd, sta);
}


void ap_sta_deauth_cb(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (!(sta->flags & WLAN_STA_PENDING_DEAUTH_CB)) {
		wpa_printf(MSG_DEBUG, "Ignore deauth cb for test frame");
		return;
	}
	sta->flags &= ~WLAN_STA_PENDING_DEAUTH_CB;
	eloop_cancel_timeout(ap_sta_deauth_cb_timeout, hapd, sta);
	ap_sta_deauth_cb_timeout(hapd, sta);
}


void ap_sta_disassoc_cb(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (!(sta->flags & WLAN_STA_PENDING_DISASSOC_CB)) {
		wpa_printf(MSG_DEBUG, "Ignore disassoc cb for test frame");
		return;
	}
	sta->flags &= ~WLAN_STA_PENDING_DISASSOC_CB;
	eloop_cancel_timeout(ap_sta_disassoc_cb_timeout, hapd, sta);
	ap_sta_disassoc_cb_timeout(hapd, sta);
}


void ap_sta_clear_disconnect_timeouts(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	if (eloop_cancel_timeout(ap_sta_deauth_cb_timeout, hapd, sta) > 0)
		wpa_printf(MSG_DEBUG,
			   "%s: Removed ap_sta_deauth_cb_timeout timeout for "
			   MACSTR,
			   hapd->conf->iface, MAC2STR(sta->addr));
	if (eloop_cancel_timeout(ap_sta_disassoc_cb_timeout, hapd, sta) > 0)
		wpa_printf(MSG_DEBUG,
			   "%s: Removed ap_sta_disassoc_cb_timeout timeout for "
			   MACSTR,
			   hapd->conf->iface, MAC2STR(sta->addr));
	if (eloop_cancel_timeout(ap_sta_delayed_1x_auth_fail_cb, hapd, sta) > 0)
	{
		wpa_printf(MSG_DEBUG,
			   "%s: Removed ap_sta_delayed_1x_auth_fail_cb timeout for "
			   MACSTR,
			   hapd->conf->iface, MAC2STR(sta->addr));
		if (sta->flags & WLAN_STA_WPS)
			hostapd_wps_eap_completed(hapd);
	}
}


int ap_sta_flags_txt(u32 flags, char *buf, size_t buflen)
{
	int res;

	buf[0] = '\0';
	res = os_snprintf(buf, buflen,
			  "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			  (flags & WLAN_STA_AUTH ? "[AUTH]" : ""),
			  (flags & WLAN_STA_ASSOC ? "[ASSOC]" : ""),
			  (flags & WLAN_STA_AUTHORIZED ? "[AUTHORIZED]" : ""),
			  (flags & WLAN_STA_PENDING_POLL ? "[PENDING_POLL" :
			   ""),
			  (flags & WLAN_STA_SHORT_PREAMBLE ?
			   "[SHORT_PREAMBLE]" : ""),
			  (flags & WLAN_STA_PREAUTH ? "[PREAUTH]" : ""),
			  (flags & WLAN_STA_WMM ? "[WMM]" : ""),
			  (flags & WLAN_STA_MFP ? "[MFP]" : ""),
			  (flags & WLAN_STA_WPS ? "[WPS]" : ""),
			  (flags & WLAN_STA_MAYBE_WPS ? "[MAYBE_WPS]" : ""),
			  (flags & WLAN_STA_WDS ? "[WDS]" : ""),
			  (flags & WLAN_STA_NONERP ? "[NonERP]" : ""),
			  (flags & WLAN_STA_WPS2 ? "[WPS2]" : ""),
			  (flags & WLAN_STA_GAS ? "[GAS]" : ""),
			  (flags & WLAN_STA_HT ? "[HT]" : ""),
			  (flags & WLAN_STA_VHT ? "[VHT]" : ""),
			  (flags & WLAN_STA_HE ? "[HE]" : ""),
			  (flags & WLAN_STA_EHT ? "[EHT]" : ""),
			  (flags & WLAN_STA_6GHZ ? "[6GHZ]" : ""),
			  (flags & WLAN_STA_VENDOR_VHT ? "[VENDOR_VHT]" : ""),
			  (flags & WLAN_STA_WNM_SLEEP_MODE ?
			   "[WNM_SLEEP_MODE]" : ""));
	if (os_snprintf_error(buflen, res))
		res = -1;

	return res;
}


static void ap_sta_delayed_1x_auth_fail_cb(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	u16 reason;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
		"IEEE 802.1X: Scheduled disconnection of " MACSTR
		" after EAP-Failure", MAC2STR(sta->addr));

	reason = sta->disconnect_reason_code;
	if (!reason)
		reason = WLAN_REASON_IEEE_802_1X_AUTH_FAILED;
	ap_sta_disconnect(hapd, sta, sta->addr, reason);
	if (sta->flags & WLAN_STA_WPS)
		hostapd_wps_eap_completed(hapd);
}


void ap_sta_delayed_1x_auth_fail_disconnect(struct hostapd_data *hapd,
					    struct sta_info *sta,
					    unsigned timeout)
{
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
		"IEEE 802.1X: Force disconnection of " MACSTR
		" after EAP-Failure in %u ms", MAC2STR(sta->addr), timeout);

	/*
	 * Add a small sleep to increase likelihood of previously requested
	 * EAP-Failure TX getting out before this should the driver reorder
	 * operations.
	 */
	eloop_cancel_timeout(ap_sta_delayed_1x_auth_fail_cb, hapd, sta);
	eloop_register_timeout(0, timeout * 1000,
			       ap_sta_delayed_1x_auth_fail_cb, hapd, sta);
}


int ap_sta_pending_delayed_1x_auth_fail_disconnect(struct hostapd_data *hapd,
						   struct sta_info *sta)
{
	return eloop_is_timeout_registered(ap_sta_delayed_1x_auth_fail_cb,
					   hapd, sta);
}


#ifdef CONFIG_IEEE80211BE
static void ap_sta_remove_link_sta(struct hostapd_data *hapd,
				   struct sta_info *sta)
{
	struct hostapd_data *tmp_hapd;

	for_each_mld_link(tmp_hapd, hapd) {
		struct sta_info *tmp_sta;

		if (hapd == tmp_hapd)
			continue;

		for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
		     tmp_sta = tmp_sta->next) {
			if (tmp_sta == sta ||
			    !ether_addr_equal(tmp_sta->addr, sta->addr))
				continue;

			ap_free_sta(tmp_hapd, tmp_sta);
			break;
		}
	}
}
#endif /* CONFIG_IEEE80211BE */


int ap_sta_re_add(struct hostapd_data *hapd, struct sta_info *sta)
{
	const u8 *mld_link_addr = NULL;
	bool mld_link_sta = false;

	/*
	 * If a station that is already associated to the AP, is trying to
	 * authenticate again, remove the STA entry, in order to make sure the
	 * STA PS state gets cleared and configuration gets updated. To handle
	 * this, station's added_unassoc flag is cleared once the station has
	 * completed association.
	 */

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		u8 mld_link_id = hapd->mld_link_id;

		mld_link_sta = sta->mld_assoc_link_id != mld_link_id;
		mld_link_addr = sta->mld_info.links[mld_link_id].peer_addr;

		/*
		 * In case the AP is affiliated with an AP MLD, we need to
		 * remove the station from all relevant links/APs.
		 */
		ap_sta_remove_link_sta(hapd, sta);
	}
#endif /* CONFIG_IEEE80211BE */

	ap_sta_set_authorized(hapd, sta, 0);
	hostapd_drv_sta_remove(hapd, sta->addr);
	sta->flags &= ~(WLAN_STA_ASSOC | WLAN_STA_AUTH | WLAN_STA_AUTHORIZED);

	if (hostapd_sta_add(hapd, sta->addr, 0, 0,
			    sta->supported_rates,
			    sta->supported_rates_len,
			    0, NULL, NULL, NULL, 0, NULL, 0, NULL,
			    sta->flags, 0, 0, 0, 0,
			    mld_link_addr, mld_link_sta)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "Could not add STA to kernel driver");
		return -1;
	}

	sta->added_unassoc = 1;
	return 0;
}


#ifdef CONFIG_IEEE80211BE
void ap_sta_free_sta_profile(struct mld_info *info)
{
	int i;

	if (!info)
		return;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		os_free(info->links[i].resp_sta_profile);
		info->links[i].resp_sta_profile = NULL;
	}
}
#endif /* CONFIG_IEEE80211BE */
