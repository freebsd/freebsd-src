/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / Station table
 * Copyright (c) 2002-2004, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "hostapd.h"
#include "sta_info.h"
#include "eloop.h"
#include "accounting.h"
#include "ieee802_1x.h"
#include "ieee802_11.h"
#include "radius.h"
#include "eapol_sm.h"
#include "wpa.h"
#include "radius_client.h"
#include "driver.h"


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


struct sta_info* ap_get_sta(hostapd *hapd, u8 *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta)];
	while (s != NULL && memcmp(s->addr, sta, 6) != 0)
		s = s->hnext;
	return s;
}


static void ap_sta_list_del(hostapd *hapd, struct sta_info *sta)
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
		printf("Could not remove STA " MACSTR " from list.\n",
		       MAC2STR(sta->addr));
	} else
		tmp->next = sta->next;
}


void ap_sta_hash_add(hostapd *hapd, struct sta_info *sta)
{
	sta->hnext = hapd->sta_hash[STA_HASH(sta->addr)];
	hapd->sta_hash[STA_HASH(sta->addr)] = sta;
}


static void ap_sta_hash_del(hostapd *hapd, struct sta_info *sta)
{
	struct sta_info *s;

	s = hapd->sta_hash[STA_HASH(sta->addr)];
	if (s == NULL) return;
	if (memcmp(s->addr, sta->addr, 6) == 0) {
		hapd->sta_hash[STA_HASH(sta->addr)] = s->hnext;
		return;
	}

	while (s->hnext != NULL && memcmp(s->hnext->addr, sta->addr, 6) != 0)
		s = s->hnext;
	if (s->hnext != NULL)
		s->hnext = s->hnext->hnext;
	else
		printf("AP: could not remove STA " MACSTR " from hash table\n",
		       MAC2STR(sta->addr));
}


void ap_free_sta(hostapd *hapd, struct sta_info *sta)
{
	accounting_sta_stop(hapd, sta);
	if (!(sta->flags & WLAN_STA_PREAUTH))
		hostapd_sta_remove(hapd, sta->addr);

	ap_sta_hash_del(hapd, sta);
	ap_sta_list_del(hapd, sta);

	if (sta->aid > 0)
		hapd->sta_aid[sta->aid - 1] = NULL;

	hapd->num_sta--;
	eloop_cancel_timeout(ap_handle_timer, hapd, sta);

	ieee802_1x_free_station(sta);
	wpa_free_station(sta);
	radius_client_flush_auth(hapd->radius, sta->addr);

	if (sta->last_assoc_req)
		free(sta->last_assoc_req);

	free(sta->challenge);
	free(sta->wpa_ie);

	free(sta);
}


void hostapd_free_stas(hostapd *hapd)
{
	struct sta_info *sta, *prev;

	sta = hapd->sta_list;

	while (sta) {
		prev = sta;
		sta = sta->next;
		printf("Removing station " MACSTR "\n", MAC2STR(prev->addr));
		ap_free_sta(hapd, prev);
	}
}


void ap_handle_timer(void *eloop_ctx, void *timeout_ctx)
{
	hostapd *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;
	unsigned long next_time = 0;

	if (sta->timeout_next == STA_REMOVE) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "local deauth request");
		ap_free_sta(hapd, sta);
		return;
	}

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    (sta->timeout_next == STA_NULLFUNC ||
	     sta->timeout_next == STA_DISASSOC)) {
		int inactive_sec;
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "Checking STA " MACSTR " inactivity:\n",
			      MAC2STR(sta->addr));
		inactive_sec = hostapd_get_inact_sec(hapd, sta->addr);
		if (inactive_sec == -1) {
			printf("  Could not get station info from kernel "
			       "driver for " MACSTR ".\n",
			       MAC2STR(sta->addr));
		} else if (inactive_sec < AP_MAX_INACTIVITY &&
			   sta->flags & WLAN_STA_ASSOC) {
			/* station activity detected; reset timeout state */
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "  Station has been active\n");
			sta->timeout_next = STA_NULLFUNC;
			next_time = AP_MAX_INACTIVITY - inactive_sec;
		}
	}

	if ((sta->flags & WLAN_STA_ASSOC) &&
	    sta->timeout_next == STA_DISASSOC &&
	    !(sta->flags & WLAN_STA_PENDING_POLL)) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "  Station has ACKed data poll\n");
		/* data nullfunc frame poll did not produce TX errors; assume
		 * station ACKed it */
		sta->timeout_next = STA_NULLFUNC;
		next_time = AP_MAX_INACTIVITY;
	}

	if (next_time) {
		eloop_register_timeout(next_time, 0, ap_handle_timer, hapd,
				       sta);
		return;
	}

	if (sta->timeout_next == STA_NULLFUNC &&
	    (sta->flags & WLAN_STA_ASSOC)) {
		/* send data frame to poll STA and check whether this frame
		 * is ACKed */
		struct ieee80211_hdr hdr;

		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "  Polling STA with data frame\n");
		sta->flags |= WLAN_STA_PENDING_POLL;

		/* FIX: WLAN_FC_STYPE_NULLFUNC would be more appropriate, but
		 * it is apparently not retried so TX Exc events are not
		 * received for it */
		memset(&hdr, 0, sizeof(hdr));
		hdr.frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_DATA, WLAN_FC_STYPE_DATA);
		hdr.frame_control |= host_to_le16(BIT(1));
		hdr.frame_control |= host_to_le16(WLAN_FC_FROMDS);
		memcpy(hdr.IEEE80211_DA_FROMDS, sta->addr, ETH_ALEN);
		memcpy(hdr.IEEE80211_BSSID_FROMDS, hapd->own_addr, ETH_ALEN);
		memcpy(hdr.IEEE80211_SA_FROMDS, hapd->own_addr, ETH_ALEN);

		if (hostapd_send_mgmt_frame(hapd, &hdr, sizeof(hdr), 0) < 0)
			perror("ap_handle_timer: send");
	} else if (sta->timeout_next != STA_REMOVE) {
		int deauth = sta->timeout_next == STA_DEAUTH;

		printf("  Sending %s info to STA " MACSTR "\n",
		       deauth ? "deauthentication" : "disassociation",
		       MAC2STR(sta->addr));

		if (deauth) {
			hostapd_sta_deauth(hapd, sta->addr,
					   WLAN_REASON_PREV_AUTH_NOT_VALID);
		} else {
			hostapd_sta_disassoc(
				hapd, sta->addr,
				WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY);
		}
	}

	switch (sta->timeout_next) {
	case STA_NULLFUNC:
		sta->timeout_next = STA_DISASSOC;
		eloop_register_timeout(AP_DISASSOC_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		break;
	case STA_DISASSOC:
		sta->flags &= ~WLAN_STA_ASSOC;
		ieee802_1x_set_port_enabled(hapd, sta, 0);
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		accounting_sta_stop(hapd, sta);
		ieee802_1x_free_station(sta);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "disassociated due to "
			       "inactivity");
		sta->timeout_next = STA_DEAUTH;
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
		break;
	case STA_DEAUTH:
	case STA_REMOVE:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
			       "inactivity");
		if (!sta->acct_terminate_cause)
			sta->acct_terminate_cause =
				RADIUS_ACCT_TERMINATE_CAUSE_IDLE_TIMEOUT;
		ap_free_sta(hapd, sta);
		break;
	}
}


void ap_handle_session_timer(void *eloop_ctx, void *timeout_ctx)
{
	hostapd *hapd = eloop_ctx;
	struct sta_info *sta = timeout_ctx;

	if (!(sta->flags & WLAN_STA_AUTH))
		return;

	hostapd_sta_deauth(hapd, sta->addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "deauthenticated due to "
		       "session timeout");
	sta->acct_terminate_cause =
		RADIUS_ACCT_TERMINATE_CAUSE_SESSION_TIMEOUT;
	ap_free_sta(hapd, sta);
}


void ap_sta_session_timeout(hostapd *hapd, struct sta_info *sta,
			    u32 session_timeout)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "setting session timeout to %d "
		       "seconds", session_timeout);
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
	eloop_register_timeout(session_timeout, 0, ap_handle_session_timer,
			       hapd, sta);
}


void ap_sta_no_session_timeout(hostapd *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(ap_handle_session_timer, hapd, sta);
}


struct sta_info * ap_sta_add(struct hostapd_data *hapd, u8 *addr)
{
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return sta;

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "  New STA\n");
	if (hapd->num_sta >= MAX_STA_COUNT) {
		/* FIX: might try to remove some old STAs first? */
		printf("  no more room for new STAs (%d/%d)\n",
		       hapd->num_sta, MAX_STA_COUNT);
		return NULL;
	}

	sta = (struct sta_info *) malloc(sizeof(struct sta_info));
	if (sta == NULL) {
		printf("  malloc failed\n");
		return NULL;
	}
	memset(sta, 0, sizeof(struct sta_info));
	sta->acct_interim_interval = hapd->conf->radius_acct_interim_interval;

	/* initialize STA info data */
	eloop_register_timeout(AP_MAX_INACTIVITY, 0, ap_handle_timer,
			       hapd, sta);
	memcpy(sta->addr, addr, ETH_ALEN);
	sta->next = hapd->sta_list;
	hapd->sta_list = sta;
	hapd->num_sta++;
	ap_sta_hash_add(hapd, sta);

	return sta;
}
