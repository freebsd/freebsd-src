/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2008, Jouni Malinen <j@w1.fi>
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

#ifndef CONFIG_NATIVE_WINDOWS

#include <net/if.h>

#include "eloop.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "beacon.h"
#include "hw_features.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "ieee802_11_auth.h"
#include "sta_info.h"
#include "rc4.h"
#include "ieee802_1x.h"
#include "wpa.h"
#include "wme.h"
#include "ap_list.h"
#include "accounting.h"
#include "driver.h"
#include "mlme.h"


u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	int i, num, count;

	if (hapd->iface->current_rates == NULL)
		return eid;

	*pos++ = WLAN_EID_SUPP_RATES;
	num = hapd->iface->num_rates;
	if (num > 8) {
		/* rest of the rates are encoded in Extended supported
		 * rates element */
		num = 8;
	}

	*pos++ = num;
	count = 0;
	for (i = 0, count = 0; i < hapd->iface->num_rates && count < num;
	     i++) {
		count++;
		*pos = hapd->iface->current_rates[i].rate / 5;
		if (hapd->iface->current_rates[i].flags & HOSTAPD_RATE_BASIC)
			*pos |= 0x80;
		pos++;
	}

	return pos;
}


u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	int i, num, count;

	if (hapd->iface->current_rates == NULL)
		return eid;

	num = hapd->iface->num_rates;
	if (num <= 8)
		return eid;
	num -= 8;

	*pos++ = WLAN_EID_EXT_SUPP_RATES;
	*pos++ = num;
	count = 0;
	for (i = 0, count = 0; i < hapd->iface->num_rates && count < num + 8;
	     i++) {
		count++;
		if (count <= 8)
			continue; /* already in SuppRates IE */
		*pos = hapd->iface->current_rates[i].rate / 5;
		if (hapd->iface->current_rates[i].flags & HOSTAPD_RATE_BASIC)
			*pos |= 0x80;
		pos++;
	}

	return pos;
}


u8 * hostapd_eid_ht_capabilities_info(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_IEEE80211N
	struct ieee80211_ht_capability *cap;
	u8 *pos = eid;

	if (!hapd->iconf->ieee80211n)
		return eid;

	*pos++ = WLAN_EID_HT_CAP;
	*pos++ = sizeof(*cap);

	cap = (struct ieee80211_ht_capability *) pos;
	os_memset(cap, 0, sizeof(*cap));
	SET_2BIT_U8(&cap->mac_ht_params_info,
		    MAC_HT_PARAM_INFO_MAX_RX_AMPDU_FACTOR_OFFSET,
		    MAX_RX_AMPDU_FACTOR_64KB);

	cap->capabilities_info = host_to_le16(hapd->iconf->ht_capab);

	cap->supported_mcs_set[0] = 0xff;
	cap->supported_mcs_set[1] = 0xff;

 	pos += sizeof(*cap);

	return pos;
#else /* CONFIG_IEEE80211N */
	return eid;
#endif /* CONFIG_IEEE80211N */
}


u8 * hostapd_eid_ht_operation(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_IEEE80211N
	struct ieee80211_ht_operation *oper;
	u8 *pos = eid;

	if (!hapd->iconf->ieee80211n)
		return eid;

	*pos++ = WLAN_EID_HT_OPERATION;
	*pos++ = sizeof(*oper);

	oper = (struct ieee80211_ht_operation *) pos;
	os_memset(oper, 0, sizeof(*oper));

	oper->control_chan = hapd->iconf->channel;
	oper->operation_mode = host_to_le16(hapd->iface->ht_op_mode);
	if (hapd->iconf->secondary_channel == 1)
		oper->ht_param |= HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE |
			HT_INFO_HT_PARAM_REC_TRANS_CHNL_WIDTH;
	if (hapd->iconf->secondary_channel == -1)
		oper->ht_param |= HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW |
			HT_INFO_HT_PARAM_REC_TRANS_CHNL_WIDTH;

	pos += sizeof(*oper);

	return pos;
#else /* CONFIG_IEEE80211N */
	return eid;
#endif /* CONFIG_IEEE80211N */
}


#ifdef CONFIG_IEEE80211N

/*
op_mode
Set to 0 (HT pure) under the followign conditions
	- all STAs in the BSS are 20/40 MHz HT in 20/40 MHz BSS or
	- all STAs in the BSS are 20 MHz HT in 20 MHz BSS
Set to 1 (HT non-member protection) if there may be non-HT STAs
	in both the primary and the secondary channel
Set to 2 if only HT STAs are associated in BSS,
	however and at least one 20 MHz HT STA is associated
Set to 3 (HT mixed mode) when one or more non-HT STAs are associated
	(currently non-GF HT station is considered as non-HT STA also)
*/
int hostapd_ht_operation_update(struct hostapd_iface *iface)
{
	u16 cur_op_mode, new_op_mode;
	int op_mode_changes = 0;

	if (!iface->conf->ieee80211n || iface->conf->ht_op_mode_fixed)
		return 0;

	wpa_printf(MSG_DEBUG, "%s current operation mode=0x%X",
		   __func__, iface->ht_op_mode);

	if (!(iface->ht_op_mode & HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT)
	    && iface->num_sta_ht_no_gf) {
		iface->ht_op_mode |=
			HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT;
		op_mode_changes++;
	} else if ((iface->ht_op_mode &
		    HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT) &&
		   iface->num_sta_ht_no_gf == 0) {
		iface->ht_op_mode &=
			~HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT;
		op_mode_changes++;
	}

	if (!(iface->ht_op_mode & HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT) &&
	    (iface->num_sta_no_ht || iface->olbc_ht)) {
		iface->ht_op_mode |= HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT;
		op_mode_changes++;
	} else if ((iface->ht_op_mode &
		    HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT) &&
		   (iface->num_sta_no_ht == 0 && !iface->olbc_ht)) {
		iface->ht_op_mode &=
			~HT_INFO_OPERATION_MODE_NON_HT_STA_PRESENT;
		op_mode_changes++;
	}

	/* Note: currently we switch to the MIXED op mode if HT non-greenfield
	 * station is associated. Probably it's a theoretical case, since
	 * it looks like all known HT STAs support greenfield.
	 */
	new_op_mode = 0;
	if (iface->num_sta_no_ht ||
	    (iface->ht_op_mode & HT_INFO_OPERATION_MODE_NON_GF_DEVS_PRESENT))
		new_op_mode = OP_MODE_MIXED;
	else if ((iface->conf->ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET)
		 && iface->num_sta_ht_20mhz)
		new_op_mode = OP_MODE_20MHZ_HT_STA_ASSOCED;
	else if (iface->olbc_ht)
		new_op_mode = OP_MODE_MAY_BE_LEGACY_STAS;
	else
		new_op_mode = OP_MODE_PURE;

	cur_op_mode = iface->ht_op_mode & HT_INFO_OPERATION_MODE_OP_MODE_MASK;
	if (cur_op_mode != new_op_mode) {
		iface->ht_op_mode &= ~HT_INFO_OPERATION_MODE_OP_MODE_MASK;
		iface->ht_op_mode |= new_op_mode;
		op_mode_changes++;
	}

	wpa_printf(MSG_DEBUG, "%s new operation mode=0x%X changes=%d",
		   __func__, iface->ht_op_mode, op_mode_changes);

	return op_mode_changes;
}

#endif /* CONFIG_IEEE80211N */


u16 hostapd_own_capab_info(struct hostapd_data *hapd, struct sta_info *sta,
			   int probe)
{
	int capab = WLAN_CAPABILITY_ESS;
	int privacy;

	if (hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		capab |= WLAN_CAPABILITY_SHORT_PREAMBLE;

	privacy = hapd->conf->ssid.wep.keys_set;

	if (hapd->conf->ieee802_1x &&
	    (hapd->conf->default_wep_key_len ||
	     hapd->conf->individual_wep_key_len))
		privacy = 1;

	if (hapd->conf->wpa)
		privacy = 1;

	if (sta) {
		int policy, def_klen;
		if (probe && sta->ssid_probe) {
			policy = sta->ssid_probe->security_policy;
			def_klen = sta->ssid_probe->wep.default_len;
		} else {
			policy = sta->ssid->security_policy;
			def_klen = sta->ssid->wep.default_len;
		}
		privacy = policy != SECURITY_PLAINTEXT;
		if (policy == SECURITY_IEEE_802_1X && def_klen == 0)
			privacy = 0;
	}

	if (privacy)
		capab |= WLAN_CAPABILITY_PRIVACY;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hapd->iface->num_sta_no_short_slot_time == 0)
		capab |= WLAN_CAPABILITY_SHORT_SLOT_TIME;

	return capab;
}


#ifdef CONFIG_IEEE80211W
static u8 * hostapd_eid_assoc_comeback_time(struct hostapd_data *hapd,
					    struct sta_info *sta, u8 *eid)
{
	u8 *pos = eid;
	u32 timeout, tu;
	struct os_time now, passed;

	*pos++ = WLAN_EID_TIMEOUT_INTERVAL;
	*pos++ = 5;
	*pos++ = WLAN_TIMEOUT_ASSOC_COMEBACK;
	os_get_time(&now);
	os_time_sub(&now, &sta->sa_query_start, &passed);
	tu = (passed.sec * 1000000 + passed.usec) / 1024;
	if (hapd->conf->assoc_sa_query_max_timeout > tu)
		timeout = hapd->conf->assoc_sa_query_max_timeout - tu;
	else
		timeout = 0;
	if (timeout < hapd->conf->assoc_sa_query_max_timeout)
		timeout++; /* add some extra time for local timers */
	WPA_PUT_LE32(pos, timeout);
	pos += 4;

	return pos;
}
#endif /* CONFIG_IEEE80211W */


void ieee802_11_print_ssid(char *buf, const u8 *ssid, u8 len)
{
	int i;
	if (len > HOSTAPD_MAX_SSID_LEN)
		len = HOSTAPD_MAX_SSID_LEN;
	for (i = 0; i < len; i++) {
		if (ssid[i] >= 32 && ssid[i] < 127)
			buf[i] = ssid[i];
		else
			buf[i] = '.';
	}
	buf[len] = '\0';
}


/**
 * ieee802_11_send_deauth - Send Deauthentication frame
 * @hapd: hostapd BSS data
 * @addr: Address of the destination STA
 * @reason: Reason code for Deauthentication
 */
void ieee802_11_send_deauth(struct hostapd_data *hapd, u8 *addr, u16 reason)
{
	struct ieee80211_mgmt mgmt;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "deauthenticate - reason %d", reason);
	os_memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DEAUTH);
	os_memcpy(mgmt.da, addr, ETH_ALEN);
	os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
	mgmt.u.deauth.reason_code = host_to_le16(reason);
	if (hostapd_send_mgmt_frame(hapd, &mgmt, IEEE80211_HDRLEN +
				    sizeof(mgmt.u.deauth), 0) < 0)
		perror("ieee802_11_send_deauth: send");
}


static u16 auth_shared_key(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 auth_transaction, u8 *challenge, int iswep)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication (shared key, transaction %d)",
		       auth_transaction);

	if (auth_transaction == 1) {
		if (!sta->challenge) {
			/* Generate a pseudo-random challenge */
			u8 key[8];
			time_t now;
			int r;
			sta->challenge = os_zalloc(WLAN_AUTH_CHALLENGE_LEN);
			if (sta->challenge == NULL)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			now = time(NULL);
			r = random();
			os_memcpy(key, &now, 4);
			os_memcpy(key + 4, &r, 4);
			rc4_skip(key, sizeof(key), 0,
				 sta->challenge, WLAN_AUTH_CHALLENGE_LEN);
		}
		return 0;
	}

	if (auth_transaction != 3)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* Transaction 3 */
	if (!iswep || !sta->challenge || !challenge ||
	    os_memcmp(sta->challenge, challenge, WLAN_AUTH_CHALLENGE_LEN)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "shared key authentication - invalid "
			       "challenge-response");
		return WLAN_STATUS_CHALLENGE_FAIL;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication OK (shared key)");
#ifdef IEEE80211_REQUIRE_AUTH_ACK
	/* Station will be marked authenticated if it ACKs the
	 * authentication reply. */
#else
	sta->flags |= WLAN_STA_AUTH;
	wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
#endif
	os_free(sta->challenge);
	sta->challenge = NULL;

	return 0;
}


static void send_auth_reply(struct hostapd_data *hapd,
			    const u8 *dst, const u8 *bssid,
			    u16 auth_alg, u16 auth_transaction, u16 resp,
			    const u8 *ies, size_t ies_len)
{
	struct ieee80211_mgmt *reply;
	u8 *buf;
	size_t rlen;

	rlen = IEEE80211_HDRLEN + sizeof(reply->u.auth) + ies_len;
	buf = os_zalloc(rlen);
	if (buf == NULL)
		return;

	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					    WLAN_FC_STYPE_AUTH);
	os_memcpy(reply->da, dst, ETH_ALEN);
	os_memcpy(reply->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply->bssid, bssid, ETH_ALEN);

	reply->u.auth.auth_alg = host_to_le16(auth_alg);
	reply->u.auth.auth_transaction = host_to_le16(auth_transaction);
	reply->u.auth.status_code = host_to_le16(resp);

	if (ies && ies_len)
		os_memcpy(reply->u.auth.variable, ies, ies_len);

	wpa_printf(MSG_DEBUG, "authentication reply: STA=" MACSTR
		   " auth_alg=%d auth_transaction=%d resp=%d (IE len=%lu)",
		   MAC2STR(dst), auth_alg, auth_transaction,
		   resp, (unsigned long) ies_len);
	if (hostapd_send_mgmt_frame(hapd, reply, rlen, 0) < 0)
		perror("send_auth_reply: send");

	os_free(buf);
}


#ifdef CONFIG_IEEE80211R
static void handle_auth_ft_finish(void *ctx, const u8 *dst, const u8 *bssid,
				  u16 auth_transaction, u16 status,
				  const u8 *ies, size_t ies_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	send_auth_reply(hapd, dst, bssid, WLAN_AUTH_FT, auth_transaction,
			status, ies, ies_len);

	if (status != WLAN_STATUS_SUCCESS)
		return;

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL)
		return;

	hostapd_logger(hapd, dst, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "authentication OK (FT)");
	sta->flags |= WLAN_STA_AUTH;
	mlme_authenticate_indication(hapd, sta);
}
#endif /* CONFIG_IEEE80211R */


static void handle_auth(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
			size_t len)
{
	u16 auth_alg, auth_transaction, status_code;
	u16 resp = WLAN_STATUS_SUCCESS;
	struct sta_info *sta = NULL;
	int res;
	u16 fc;
	u8 *challenge = NULL;
	u32 session_timeout, acct_interim_interval;
	int vlan_id = 0;
	u8 resp_ies[2 + WLAN_AUTH_CHALLENGE_LEN];
	size_t resp_ies_len = 0;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		printf("handle_auth - too short payload (len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);
	fc = le_to_host16(mgmt->frame_control);

	if (len >= IEEE80211_HDRLEN + sizeof(mgmt->u.auth) +
	    2 + WLAN_AUTH_CHALLENGE_LEN &&
	    mgmt->u.auth.variable[0] == WLAN_EID_CHALLENGE &&
	    mgmt->u.auth.variable[1] == WLAN_AUTH_CHALLENGE_LEN)
		challenge = &mgmt->u.auth.variable[2];

	wpa_printf(MSG_DEBUG, "authentication: STA=" MACSTR " auth_alg=%d "
		   "auth_transaction=%d status_code=%d wep=%d%s",
		   MAC2STR(mgmt->sa), auth_alg, auth_transaction,
		   status_code, !!(fc & WLAN_FC_ISWEP),
		   challenge ? " challenge" : "");

	if (hapd->tkip_countermeasures) {
		resp = WLAN_REASON_MICHAEL_MIC_FAILURE;
		goto fail;
	}

	if (!(((hapd->conf->auth_algs & WPA_AUTH_ALG_OPEN) &&
	       auth_alg == WLAN_AUTH_OPEN) ||
#ifdef CONFIG_IEEE80211R
	      (hapd->conf->wpa &&
	       (hapd->conf->wpa_key_mgmt &
		(WPA_KEY_MGMT_FT_IEEE8021X | WPA_KEY_MGMT_FT_PSK)) &&
	       auth_alg == WLAN_AUTH_FT) ||
#endif /* CONFIG_IEEE80211R */
	      ((hapd->conf->auth_algs & WPA_AUTH_ALG_SHARED) &&
	       auth_alg == WLAN_AUTH_SHARED_KEY))) {
		printf("Unsupported authentication algorithm (%d)\n",
		       auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}

	if (!(auth_transaction == 1 ||
	      (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 3))) {
		printf("Unknown authentication transaction number (%d)\n",
		       auth_transaction);
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
		goto fail;
	}

	if (os_memcmp(mgmt->sa, hapd->own_addr, ETH_ALEN) == 0) {
		printf("Station " MACSTR " not allowed to authenticate.\n",
		       MAC2STR(mgmt->sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	res = hostapd_allowed_address(hapd, mgmt->sa, (u8 *) mgmt, len,
				      &session_timeout,
				      &acct_interim_interval, &vlan_id);
	if (res == HOSTAPD_ACL_REJECT) {
		printf("Station " MACSTR " not allowed to authenticate.\n",
		       MAC2STR(mgmt->sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	if (res == HOSTAPD_ACL_PENDING) {
		wpa_printf(MSG_DEBUG, "Authentication frame from " MACSTR
			   " waiting for an external authentication",
			   MAC2STR(mgmt->sa));
		/* Authentication code will re-send the authentication frame
		 * after it has received (and cached) information from the
		 * external source. */
		return;
	}

	sta = ap_sta_add(hapd, mgmt->sa);
	if (!sta) {
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (vlan_id > 0) {
		if (hostapd_get_vlan_id_ifname(hapd->conf->vlan,
					       vlan_id) == NULL) {
			hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
				       HOSTAPD_LEVEL_INFO, "Invalid VLAN ID "
				       "%d received from RADIUS server",
				       vlan_id);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		sta->vlan_id = vlan_id;
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO, "VLAN ID %d", sta->vlan_id);
	}

	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);

	if (hapd->conf->radius->acct_interim_interval == 0 &&
	    acct_interim_interval)
		sta->acct_interim_interval = acct_interim_interval;
	if (res == HOSTAPD_ACL_ACCEPT_TIMEOUT)
		ap_sta_session_timeout(hapd, sta, session_timeout);
	else
		ap_sta_no_session_timeout(hapd, sta);

	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication OK (open system)");
#ifdef IEEE80211_REQUIRE_AUTH_ACK
		/* Station will be marked authenticated if it ACKs the
		 * authentication reply. */
#else
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_OPEN;
		mlme_authenticate_indication(hapd, sta);
#endif
		break;
	case WLAN_AUTH_SHARED_KEY:
		resp = auth_shared_key(hapd, sta, auth_transaction, challenge,
				       fc & WLAN_FC_ISWEP);
		sta->auth_alg = WLAN_AUTH_SHARED_KEY;
		mlme_authenticate_indication(hapd, sta);
		if (sta->challenge && auth_transaction == 1) {
			resp_ies[0] = WLAN_EID_CHALLENGE;
			resp_ies[1] = WLAN_AUTH_CHALLENGE_LEN;
			os_memcpy(resp_ies + 2, sta->challenge,
				  WLAN_AUTH_CHALLENGE_LEN);
			resp_ies_len = 2 + WLAN_AUTH_CHALLENGE_LEN;
		}
		break;
#ifdef CONFIG_IEEE80211R
	case WLAN_AUTH_FT:
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_DEBUG, "FT: Failed to initialize WPA "
				   "state machine");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_ft_process_auth(sta->wpa_sm, mgmt->bssid,
				    auth_transaction, mgmt->u.auth.variable,
				    len - IEEE80211_HDRLEN -
				    sizeof(mgmt->u.auth),
				    handle_auth_ft_finish, hapd);
		/* handle_auth_ft_finish() callback will complete auth. */
		return;
#endif /* CONFIG_IEEE80211R */
	}

 fail:
	send_auth_reply(hapd, mgmt->sa, mgmt->bssid, auth_alg,
			auth_transaction + 1, resp, resp_ies, resp_ies_len);
}


static void handle_assoc(struct hostapd_data *hapd,
			 struct ieee80211_mgmt *mgmt, size_t len, int reassoc)
{
	u16 capab_info, listen_interval;
	u16 resp = WLAN_STATUS_SUCCESS;
	u8 *pos, *wpa_ie;
	size_t wpa_ie_len;
	int send_deauth = 0, send_len, left, i;
	struct sta_info *sta;
	struct ieee802_11_elems elems;
	u8 buf[sizeof(struct ieee80211_mgmt) + 512];
	struct ieee80211_mgmt *reply;

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_req) :
				      sizeof(mgmt->u.assoc_req))) {
		printf("handle_assoc(reassoc=%d) - too short payload (len=%lu)"
		       "\n", reassoc, (unsigned long) len);
		return;
	}

	if (reassoc) {
		capab_info = le_to_host16(mgmt->u.reassoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.reassoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "reassociation request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d current_ap="
			   MACSTR,
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   MAC2STR(mgmt->u.reassoc_req.current_ap));
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
		pos = mgmt->u.reassoc_req.variable;
	} else {
		capab_info = le_to_host16(mgmt->u.assoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.assoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "association request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d",
			   MAC2STR(mgmt->sa), capab_info, listen_interval);
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
		pos = mgmt->u.assoc_req.variable;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
#ifdef CONFIG_IEEE80211R
	if (sta && sta->auth_alg == WLAN_AUTH_FT &&
	    (sta->flags & WLAN_STA_AUTH) == 0) {
		wpa_printf(MSG_DEBUG, "FT: Allow STA " MACSTR " to associate "
			   "prior to authentication since it is using "
			   "over-the-DS FT", MAC2STR(mgmt->sa));
	} else
#endif /* CONFIG_IEEE80211R */
	if (sta == NULL || (sta->flags & WLAN_STA_AUTH) == 0) {
		printf("STA " MACSTR " trying to associate before "
		       "authentication\n", MAC2STR(mgmt->sa));
		if (sta) {
			printf("  sta: addr=" MACSTR " aid=%d flags=0x%04x\n",
			       MAC2STR(sta->addr), sta->aid, sta->flags);
		}
		send_deauth = 1;
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (hapd->tkip_countermeasures) {
		resp = WLAN_REASON_MICHAEL_MIC_FAILURE;
		goto fail;
	}

	if (listen_interval > hapd->conf->max_listen_interval) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Too large Listen Interval (%d)",
			       listen_interval);
		resp = WLAN_STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE;
		goto fail;
	}

	sta->capability = capab_info;
	sta->listen_interval = listen_interval;

	/* followed by SSID and Supported rates; and HT capabilities if 802.11n
	 * is used */
	if (ieee802_11_parse_elems(pos, left, &elems, 1) == ParseFailed ||
	    !elems.ssid) {
		printf("STA " MACSTR " sent invalid association request\n",
		       MAC2STR(sta->addr));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (elems.ssid_len != hapd->conf->ssid.ssid_len ||
	    os_memcmp(elems.ssid, hapd->conf->ssid.ssid, elems.ssid_len) != 0)
	{
		char ssid_txt[33];
		ieee802_11_print_ssid(ssid_txt, elems.ssid, elems.ssid_len);
		printf("Station " MACSTR " tried to associate with "
		       "unknown SSID '%s'\n", MAC2STR(sta->addr), ssid_txt);
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	sta->flags &= ~WLAN_STA_WMM;
	if (elems.wmm && hapd->conf->wmm_enabled) {
		if (hostapd_eid_wmm_valid(hapd, elems.wmm, elems.wmm_len))
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG,
				       "invalid WMM element in association "
				       "request");
		else
			sta->flags |= WLAN_STA_WMM;
	}

	if (!elems.supp_rates) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "No supported rates element in AssocReq");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (elems.supp_rates_len > sizeof(sta->supported_rates)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Invalid supported rates element length %d",
			       elems.supp_rates_len);
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	os_memset(sta->supported_rates, 0, sizeof(sta->supported_rates));
	os_memcpy(sta->supported_rates, elems.supp_rates,
		  elems.supp_rates_len);
	sta->supported_rates_len = elems.supp_rates_len;

	if (elems.ext_supp_rates) {
		if (elems.supp_rates_len + elems.ext_supp_rates_len >
		    sizeof(sta->supported_rates)) {
			hostapd_logger(hapd, mgmt->sa,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Invalid supported rates element length"
				       " %d+%d", elems.supp_rates_len,
				       elems.ext_supp_rates_len);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		os_memcpy(sta->supported_rates + elems.supp_rates_len,
			  elems.ext_supp_rates, elems.ext_supp_rates_len);
		sta->supported_rates_len += elems.ext_supp_rates_len;
	}

#ifdef CONFIG_IEEE80211N
	/* save HT capabilities in the sta object */
	os_memset(&sta->ht_capabilities, 0, sizeof(sta->ht_capabilities));
	if (elems.ht_capabilities &&
	    elems.ht_capabilities_len >=
	    sizeof(struct ieee80211_ht_capability)) {
		sta->flags |= WLAN_STA_HT;
		sta->ht_capabilities.id = WLAN_EID_HT_CAP;
		sta->ht_capabilities.length =
			sizeof(struct ieee80211_ht_capability);
		os_memcpy(&sta->ht_capabilities.data,
			  elems.ht_capabilities,
			  sizeof(struct ieee80211_ht_capability));
	} else
		sta->flags &= ~WLAN_STA_HT;
#endif /* CONFIG_IEEE80211N */

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && elems.rsn_ie) {
		wpa_ie = elems.rsn_ie;
		wpa_ie_len = elems.rsn_ie_len;
	} else if ((hapd->conf->wpa & WPA_PROTO_WPA) &&
		   elems.wpa_ie) {
		wpa_ie = elems.wpa_ie;
		wpa_ie_len = elems.wpa_ie_len;
	} else {
		wpa_ie = NULL;
		wpa_ie_len = 0;
	}
#ifdef CONFIG_WPS
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS);
	if (hapd->conf->wps_state && wpa_ie == NULL) {
		if (elems.wps_ie) {
			wpa_printf(MSG_DEBUG, "STA included WPS IE in "
				   "(Re)Association Request - assume WPS is "
				   "used");
			sta->flags |= WLAN_STA_WPS;
			wpabuf_free(sta->wps_ie);
			sta->wps_ie = wpabuf_alloc_copy(elems.wps_ie + 4,
							elems.wps_ie_len - 4);
		} else {
			wpa_printf(MSG_DEBUG, "STA did not include WPA/RSN IE "
				   "in (Re)Association Request - possible WPS "
				   "use");
			sta->flags |= WLAN_STA_MAYBE_WPS;
		}
	} else
#endif /* CONFIG_WPS */
	if (hapd->conf->wpa && wpa_ie == NULL) {
		printf("STA " MACSTR ": No WPA/RSN IE in association "
		       "request\n", MAC2STR(sta->addr));
		resp = WLAN_STATUS_INVALID_IE;
		goto fail;
	}

	if (hapd->conf->wpa && wpa_ie) {
		int res;
		wpa_ie -= 2;
		wpa_ie_len += 2;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr);
		if (sta->wpa_sm == NULL) {
			printf("Failed to initialize WPA state machine\n");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  wpa_ie, wpa_ie_len,
					  elems.mdie, elems.mdie_len);
		if (res == WPA_INVALID_GROUP)
			resp = WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
		else if (res == WPA_INVALID_PAIRWISE)
			resp = WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
		else if (res == WPA_INVALID_AKMP)
			resp = WLAN_STATUS_AKMP_NOT_VALID;
		else if (res == WPA_ALLOC_FAIL)
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
#ifdef CONFIG_IEEE80211W
		else if (res == WPA_MGMT_FRAME_PROTECTION_VIOLATION)
			resp = WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		else if (res == WPA_INVALID_MGMT_GROUP_CIPHER)
			resp = WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
#endif /* CONFIG_IEEE80211W */
		else if (res == WPA_INVALID_MDIE)
			resp = WLAN_STATUS_INVALID_MDIE;
		else if (res != WPA_IE_OK)
			resp = WLAN_STATUS_INVALID_IE;
		if (resp != WLAN_STATUS_SUCCESS)
			goto fail;
#ifdef CONFIG_IEEE80211W
		if ((sta->flags & WLAN_STA_MFP) && !sta->sa_query_timed_out &&
		    sta->sa_query_count > 0)
			ap_check_sa_query_timeout(hapd, sta);
		if ((sta->flags & WLAN_STA_MFP) && !sta->sa_query_timed_out &&
		    (!reassoc || sta->auth_alg != WLAN_AUTH_FT)) {
			/*
			 * STA has already been associated with MFP and SA
			 * Query timeout has not been reached. Reject the
			 * association attempt temporarily and start SA Query,
			 * if one is not pending.
			 */

			if (sta->sa_query_count == 0)
				ap_sta_start_sa_query(hapd, sta);

			resp = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
			goto fail;
		}

		if (wpa_auth_uses_mfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_MFP;
		else
			sta->flags &= ~WLAN_STA_MFP;
#endif /* CONFIG_IEEE80211W */

#ifdef CONFIG_IEEE80211R
		if (sta->auth_alg == WLAN_AUTH_FT) {
			if (!reassoc) {
				wpa_printf(MSG_DEBUG, "FT: " MACSTR " tried "
					   "to use association (not "
					   "re-association) with FT auth_alg",
					   MAC2STR(sta->addr));
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			resp = wpa_ft_validate_reassoc(sta->wpa_sm, pos, left);
			if (resp != WLAN_STATUS_SUCCESS)
				goto fail;
		}
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211N
		if ((sta->flags & WLAN_STA_HT) &&
		    wpa_auth_get_pairwise(sta->wpa_sm) == WPA_CIPHER_TKIP) {
			wpa_printf(MSG_DEBUG, "HT: " MACSTR " tried to "
				   "use TKIP with HT association",
				   MAC2STR(sta->addr));
			resp = WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
			goto fail;
		}
#endif /* CONFIG_IEEE80211N */
	} else
		wpa_auth_sta_no_wpa(sta->wpa_sm);

	if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		sta->flags |= WLAN_STA_NONERP;
	for (i = 0; i < sta->supported_rates_len; i++) {
		if ((sta->supported_rates[i] & 0x7f) > 22) {
			sta->flags &= ~WLAN_STA_NONERP;
			break;
		}
	}
	if (sta->flags & WLAN_STA_NONERP && !sta->nonerp_set) {
		sta->nonerp_set = 1;
		hapd->iface->num_sta_non_erp++;
		if (hapd->iface->num_sta_non_erp == 1)
			ieee802_11_set_beacons(hapd->iface);
	}

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_SLOT_TIME) &&
	    !sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 1;
		hapd->iface->num_sta_no_short_slot_time++;
		if (hapd->iface->current_mode->mode ==
		    HOSTAPD_MODE_IEEE80211G &&
		    hapd->iface->num_sta_no_short_slot_time == 1)
			ieee802_11_set_beacons(hapd->iface);
	}

	if (sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sta->flags |= WLAN_STA_SHORT_PREAMBLE;
	else
		sta->flags &= ~WLAN_STA_SHORT_PREAMBLE;

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE) &&
	    !sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 1;
		hapd->iface->num_sta_no_short_preamble++;
		if (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 1)
			ieee802_11_set_beacons(hapd->iface);
	}

#ifdef CONFIG_IEEE80211N
	if (sta->flags & WLAN_STA_HT) {
		u16 ht_capab = le_to_host16(
			sta->ht_capabilities.data.capabilities_info);
		wpa_printf(MSG_DEBUG, "HT: STA " MACSTR " HT Capabilities "
			   "Info: 0x%04x", MAC2STR(sta->addr), ht_capab);
		if ((ht_capab & HT_CAP_INFO_GREEN_FIELD) == 0) {
			if (!sta->no_ht_gf_set) {
				sta->no_ht_gf_set = 1;
				hapd->iface->num_sta_ht_no_gf++;
			}
			wpa_printf(MSG_DEBUG, "%s STA " MACSTR " - no "
				   "greenfield, num of non-gf stations %d",
				   __func__, MAC2STR(sta->addr),
				   hapd->iface->num_sta_ht_no_gf);
		}
		if ((ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET) == 0) {
			if (!sta->ht_20mhz_set) {
				sta->ht_20mhz_set = 1;
				hapd->iface->num_sta_ht_20mhz++;
			}
			wpa_printf(MSG_DEBUG, "%s STA " MACSTR " - 20 MHz HT, "
				   "num of 20MHz HT STAs %d",
				   __func__, MAC2STR(sta->addr),
				   hapd->iface->num_sta_ht_20mhz);
		}
	} else {
		if (!sta->no_ht_set) {
			sta->no_ht_set = 1;
			hapd->iface->num_sta_no_ht++;
		}
		if (hapd->iconf->ieee80211n) {
			wpa_printf(MSG_DEBUG, "%s STA " MACSTR
				   " - no HT, num of non-HT stations %d",
				   __func__, MAC2STR(sta->addr),
				   hapd->iface->num_sta_no_ht);
		}
	}

	if (hostapd_ht_operation_update(hapd->iface) > 0)
		ieee802_11_set_beacons(hapd->iface);
#endif /* CONFIG_IEEE80211N */

	/* get a unique AID */
	if (sta->aid > 0) {
		wpa_printf(MSG_DEBUG, "  old AID %d", sta->aid);
	} else {
		for (sta->aid = 1; sta->aid <= MAX_AID_TABLE_SIZE; sta->aid++)
			if (hapd->sta_aid[sta->aid - 1] == NULL)
				break;
		if (sta->aid > MAX_AID_TABLE_SIZE) {
			sta->aid = 0;
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			wpa_printf(MSG_ERROR, "  no room for more AIDs");
			goto fail;
		} else {
			hapd->sta_aid[sta->aid - 1] = sta;
			wpa_printf(MSG_DEBUG, "  new AID %d", sta->aid);
		}
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "association OK (aid %d)", sta->aid);
	/* Station will be marked associated, after it acknowledges AssocResp
	 */

#ifdef CONFIG_IEEE80211W
	if ((sta->flags & WLAN_STA_MFP) && sta->sa_query_timed_out) {
		wpa_printf(MSG_DEBUG, "Allowing %sassociation after timed out "
			   "SA Query procedure", reassoc ? "re" : "");
		/* TODO: Send a protected Disassociate frame to the STA using
		 * the old key and Reason Code "Previous Authentication no
		 * longer valid". Make sure this is only sent protected since
		 * unprotected frame would be received by the STA that is now
		 * trying to associate.
		 */
	}
#endif /* CONFIG_IEEE80211W */

	if (reassoc) {
		os_memcpy(sta->previous_ap, mgmt->u.reassoc_req.current_ap,
			  ETH_ALEN);
	}

	if (sta->last_assoc_req)
		os_free(sta->last_assoc_req);
	sta->last_assoc_req = os_malloc(len);
	if (sta->last_assoc_req)
		os_memcpy(sta->last_assoc_req, mgmt, len);

	/* Make sure that the previously registered inactivity timer will not
	 * remove the STA immediately. */
	sta->timeout_next = STA_NULLFUNC;

 fail:
	os_memset(buf, 0, sizeof(buf));
	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT,
			     (send_deauth ? WLAN_FC_STYPE_DEAUTH :
			      (reassoc ? WLAN_FC_STYPE_REASSOC_RESP :
			       WLAN_FC_STYPE_ASSOC_RESP)));
	os_memcpy(reply->da, mgmt->sa, ETH_ALEN);
	os_memcpy(reply->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply->bssid, mgmt->bssid, ETH_ALEN);

	send_len = IEEE80211_HDRLEN;
	if (send_deauth) {
		send_len += sizeof(reply->u.deauth);
		reply->u.deauth.reason_code = host_to_le16(resp);
	} else {
		u8 *p;
		send_len += sizeof(reply->u.assoc_resp);
		reply->u.assoc_resp.capab_info =
			host_to_le16(hostapd_own_capab_info(hapd, sta, 0));
		reply->u.assoc_resp.status_code = host_to_le16(resp);
		reply->u.assoc_resp.aid = host_to_le16((sta ? sta->aid : 0)
						       | BIT(14) | BIT(15));
		/* Supported rates */
		p = hostapd_eid_supp_rates(hapd, reply->u.assoc_resp.variable);
		/* Extended supported rates */
		p = hostapd_eid_ext_supp_rates(hapd, p);
		if (sta->flags & WLAN_STA_WMM)
			p = hostapd_eid_wmm(hapd, p);

		p = hostapd_eid_ht_capabilities_info(hapd, p);
		p = hostapd_eid_ht_operation(hapd, p);

#ifdef CONFIG_IEEE80211R
		if (resp == WLAN_STATUS_SUCCESS) {
			/* IEEE 802.11r: Mobility Domain Information, Fast BSS
			 * Transition Information, RSN */
			p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, p,
							buf + sizeof(buf) - p,
							sta->auth_alg);
		}
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_IEEE80211W
		if (resp == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY)
			p = hostapd_eid_assoc_comeback_time(hapd, sta, p);
#endif /* CONFIG_IEEE80211W */

		send_len += p - reply->u.assoc_resp.variable;
	}

	if (hostapd_send_mgmt_frame(hapd, reply, send_len, 0) < 0)
		perror("handle_assoc: send");
}


static void handle_disassoc(struct hostapd_data *hapd,
			    struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.disassoc)) {
		printf("handle_disassoc - too short payload (len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	wpa_printf(MSG_DEBUG, "disassocation: STA=" MACSTR " reason_code=%d",
		   MAC2STR(mgmt->sa),
		   le_to_host16(mgmt->u.disassoc.reason_code));

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL) {
		printf("Station " MACSTR " trying to disassociate, but it "
		       "is not associated.\n", MAC2STR(mgmt->sa));
		return;
	}

	sta->flags &= ~WLAN_STA_ASSOC;
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	/* Stop Accounting and IEEE 802.1X sessions, but leave the STA
	 * authenticated. */
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(sta);
	hostapd_sta_remove(hapd, sta->addr);

	if (sta->timeout_next == STA_NULLFUNC ||
	    sta->timeout_next == STA_DISASSOC) {
		sta->timeout_next = STA_DEAUTH;
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
	}

	mlme_disassociate_indication(
		hapd, sta, le_to_host16(mgmt->u.disassoc.reason_code));
}


static void handle_deauth(struct hostapd_data *hapd,
			  struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.deauth)) {
		printf("handle_deauth - too short payload (len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	wpa_printf(MSG_DEBUG, "deauthentication: STA=" MACSTR
		   " reason_code=%d",
		   MAC2STR(mgmt->sa),
		   le_to_host16(mgmt->u.deauth.reason_code));

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL) {
		printf("Station " MACSTR " trying to deauthenticate, but it "
		       "is not authenticated.\n", MAC2STR(mgmt->sa));
		return;
	}

	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "deauthenticated");
	mlme_deauthenticate_indication(
		hapd, sta, le_to_host16(mgmt->u.deauth.reason_code));
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


static void handle_beacon(struct hostapd_data *hapd,
			  struct ieee80211_mgmt *mgmt, size_t len,
			  struct hostapd_frame_info *fi)
{
	struct ieee802_11_elems elems;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.beacon)) {
		printf("handle_beacon - too short payload (len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	(void) ieee802_11_parse_elems(mgmt->u.beacon.variable,
				      len - (IEEE80211_HDRLEN +
					     sizeof(mgmt->u.beacon)), &elems,
				      0);

	ap_list_process_beacon(hapd->iface, mgmt, &elems, fi);
}


#ifdef CONFIG_IEEE80211W

/* MLME-SAQuery.request */
void ieee802_11_send_sa_query_req(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *trans_id)
{
	struct ieee80211_mgmt mgmt;
	u8 *end;

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Sending SA Query Request to "
		   MACSTR, MAC2STR(addr));
	wpa_hexdump(MSG_DEBUG, "IEEE 802.11: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);

	os_memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt.da, addr, ETH_ALEN);
	os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
	mgmt.u.action.category = WLAN_ACTION_SA_QUERY;
	mgmt.u.action.u.sa_query_req.action = WLAN_SA_QUERY_REQUEST;
	os_memcpy(mgmt.u.action.u.sa_query_req.trans_id, trans_id,
		  WLAN_SA_QUERY_TR_ID_LEN);
	end = mgmt.u.action.u.sa_query_req.trans_id + WLAN_SA_QUERY_TR_ID_LEN;
	if (hostapd_send_mgmt_frame(hapd, &mgmt, end - (u8 *) &mgmt, 0) < 0)
		perror("ieee802_11_send_sa_query_req: send");
}


static void hostapd_sa_query_action(struct hostapd_data *hapd,
				    struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;
	u8 *end;
	int i;

	end = mgmt->u.action.u.sa_query_resp.trans_id +
		WLAN_SA_QUERY_TR_ID_LEN;
	if (((u8 *) mgmt) + len < end) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Too short SA Query Action "
			   "frame (len=%lu)", (unsigned long) len);
		return;
	}

	if (mgmt->u.action.u.sa_query_resp.action != WLAN_SA_QUERY_RESPONSE) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Unexpected SA Query "
			   "Action %d", mgmt->u.action.u.sa_query_resp.action);
		return;
	}

	wpa_printf(MSG_DEBUG, "IEEE 802.11: Received SA Query Response from "
		   MACSTR, MAC2STR(mgmt->sa));
	wpa_hexdump(MSG_DEBUG, "IEEE 802.11: SA Query Transaction ID",
		    mgmt->u.action.u.sa_query_resp.trans_id,
		    WLAN_SA_QUERY_TR_ID_LEN);

	/* MLME-SAQuery.confirm */

	sta = ap_get_sta(hapd, mgmt->sa);
	if (sta == NULL || sta->sa_query_trans_id == NULL) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: No matching STA with "
			   "pending SA Query request found");
		return;
	}

	for (i = 0; i < sta->sa_query_count; i++) {
		if (os_memcmp(sta->sa_query_trans_id +
			      i * WLAN_SA_QUERY_TR_ID_LEN,
			      mgmt->u.action.u.sa_query_resp.trans_id,
			      WLAN_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= sta->sa_query_count) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: No matching SA Query "
			   "transaction identifier found");
		return;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "Reply to pending SA Query received");
	ap_sta_stop_sa_query(hapd, sta);
}


static int robust_action_frame(u8 category)
{
	return category != WLAN_ACTION_PUBLIC &&
		category != WLAN_ACTION_HT;
}
#endif /* CONFIG_IEEE80211W */


static void handle_action(struct hostapd_data *hapd,
			  struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + 1) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "handle_action - too short payload (len=%lu)",
			       (unsigned long) len);
		return;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
#ifdef CONFIG_IEEE80211W
	if (sta && (sta->flags & WLAN_STA_MFP) &&
	    !(mgmt->frame_control & host_to_le16(WLAN_FC_ISWEP) &&
	      robust_action_frame(mgmt->u.action.category))) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Dropped unprotected Robust Action frame from "
			       "an MFP STA");
		return;
	}
#endif /* CONFIG_IEEE80211W */

	switch (mgmt->u.action.category) {
#ifdef CONFIG_IEEE80211R
	case WLAN_ACTION_FT:
	{
		if (sta == NULL || !(sta->flags & WLAN_STA_ASSOC)) {
			wpa_printf(MSG_DEBUG, "IEEE 802.11: Ignored FT Action "
				   "frame from unassociated STA " MACSTR,
				   MAC2STR(mgmt->sa));
			return;
		}

		if (wpa_ft_action_rx(sta->wpa_sm, (u8 *) &mgmt->u.action,
				     len - IEEE80211_HDRLEN))
			break;

		return;
	}
#endif /* CONFIG_IEEE80211R */
	case WLAN_ACTION_WMM:
		hostapd_wmm_action(hapd, mgmt, len);
		return;
#ifdef CONFIG_IEEE80211W
	case WLAN_ACTION_SA_QUERY:
		hostapd_sa_query_action(hapd, mgmt, len);
		return;
#endif /* CONFIG_IEEE80211W */
	}

	hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "handle_action - unknown action category %d or invalid "
		       "frame",
		       mgmt->u.action.category);
	if (!(mgmt->da[0] & 0x01) && !(mgmt->u.action.category & 0x80) &&
	    !(mgmt->sa[0] & 0x01)) {
		/*
		 * IEEE 802.11-REVma/D9.0 - 7.3.1.11
		 * Return the Action frame to the source without change
		 * except that MSB of the Category set to 1.
		 */
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Return unknown Action "
			   "frame back to sender");
		os_memcpy(mgmt->da, mgmt->sa, ETH_ALEN);
		os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
		mgmt->u.action.category |= 0x80;

		hostapd_send_mgmt_frame(hapd, mgmt, len, 0);
	}
}


/**
 * ieee802_11_mgmt - process incoming IEEE 802.11 management frames
 * @hapd: hostapd BSS data structure (the BSS to which the management frame was
 * sent to)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @stype: management frame subtype from frame control field
 * @fi: meta data about received frame (signal level, etc.)
 *
 * Process all incoming IEEE 802.11 management frames. This will be called for
 * each frame received from the kernel driver through wlan#ap interface. In
 * addition, it can be called to re-inserted pending frames (e.g., when using
 * external RADIUS server as an MAC ACL).
 */
void ieee802_11_mgmt(struct hostapd_data *hapd, u8 *buf, size_t len, u16 stype,
		     struct hostapd_frame_info *fi)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) buf;
	int broadcast;

	if (stype == WLAN_FC_STYPE_BEACON) {
		handle_beacon(hapd, mgmt, len, fi);
		return;
	}

	if (fi && fi->passive_scan)
		return;

	broadcast = mgmt->bssid[0] == 0xff && mgmt->bssid[1] == 0xff &&
		mgmt->bssid[2] == 0xff && mgmt->bssid[3] == 0xff &&
		mgmt->bssid[4] == 0xff && mgmt->bssid[5] == 0xff;

	if (!broadcast &&
	    os_memcmp(mgmt->bssid, hapd->own_addr, ETH_ALEN) != 0) {
		printf("MGMT: BSSID=" MACSTR " not our address\n",
		       MAC2STR(mgmt->bssid));
		return;
	}


	if (stype == WLAN_FC_STYPE_PROBE_REQ) {
		handle_probe_req(hapd, mgmt, len);
		return;
	}

	if (os_memcmp(mgmt->da, hapd->own_addr, ETH_ALEN) != 0) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "MGMT: DA=" MACSTR " not our address",
			       MAC2STR(mgmt->da));
		return;
	}

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth");
		handle_auth(hapd, mgmt, len);
		break;
	case WLAN_FC_STYPE_ASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_req");
		handle_assoc(hapd, mgmt, len, 0);
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_req");
		handle_assoc(hapd, mgmt, len, 1);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wpa_printf(MSG_DEBUG, "mgmt::disassoc");
		handle_disassoc(hapd, mgmt, len);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		wpa_printf(MSG_DEBUG, "mgmt::deauth");
		handle_deauth(hapd, mgmt, len);
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action");
		handle_action(hapd, mgmt, len);
		break;
	default:
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "unknown mgmt frame subtype %d", stype);
		break;
	}
}


static void handle_auth_cb(struct hostapd_data *hapd,
			   struct ieee80211_mgmt *mgmt,
			   size_t len, int ok)
{
	u16 auth_alg, auth_transaction, status_code;
	struct sta_info *sta;

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "did not acknowledge authentication response");
		return;
	}

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		printf("handle_auth_cb - too short payload (len=%lu)\n",
		       (unsigned long) len);
		return;
	}

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		printf("handle_auth_cb: STA " MACSTR " not found\n",
		       MAC2STR(mgmt->da));
		return;
	}

	if (status_code == WLAN_STATUS_SUCCESS &&
	    ((auth_alg == WLAN_AUTH_OPEN && auth_transaction == 2) ||
	     (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 4))) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "authenticated");
		sta->flags |= WLAN_STA_AUTH;
	}
}


#ifdef CONFIG_IEEE80211N
static void
hostapd_get_ht_capab(struct hostapd_data *hapd,
		     struct ht_cap_ie *ht_cap_ie,
		     struct ht_cap_ie *neg_ht_cap_ie)
{
	u16 cap;

	os_memcpy(neg_ht_cap_ie, ht_cap_ie, sizeof(struct ht_cap_ie));
	cap = le_to_host16(neg_ht_cap_ie->data.capabilities_info);
	cap &= hapd->iconf->ht_capab;
	cap |= (hapd->iconf->ht_capab & HT_CAP_INFO_SMPS_DISABLED);

	/* FIXME: Rx STBC needs to be handled specially */
	cap |= (hapd->iconf->ht_capab & HT_CAP_INFO_RX_STBC_MASK);
	neg_ht_cap_ie->data.capabilities_info = host_to_le16(cap);
}
#endif /* CONFIG_IEEE80211N */


static void handle_assoc_cb(struct hostapd_data *hapd,
			    struct ieee80211_mgmt *mgmt,
			    size_t len, int reassoc, int ok)
{
	u16 status;
	struct sta_info *sta;
	int new_assoc = 1;
#ifdef CONFIG_IEEE80211N
	struct ht_cap_ie ht_cap;
#endif /* CONFIG_IEEE80211N */
	struct ht_cap_ie *ht_cap_ptr = NULL;
	int set_flags, flags_and, flags_or;

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "did not acknowledge association response");
		return;
	}

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_resp) :
				      sizeof(mgmt->u.assoc_resp))) {
		printf("handle_assoc_cb(reassoc=%d) - too short payload "
		       "(len=%lu)\n", reassoc, (unsigned long) len);
		return;
	}

	if (reassoc)
		status = le_to_host16(mgmt->u.reassoc_resp.status_code);
	else
		status = le_to_host16(mgmt->u.assoc_resp.status_code);

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		printf("handle_assoc_cb: STA " MACSTR " not found\n",
		       MAC2STR(mgmt->da));
		return;
	}

	if (status != WLAN_STATUS_SUCCESS)
		goto fail;

	/* Stop previous accounting session, if one is started, and allocate
	 * new session id for the new session. */
	accounting_sta_stop(hapd, sta);

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "associated (aid %d)",
		       sta->aid);

	if (sta->flags & WLAN_STA_ASSOC)
		new_assoc = 0;
	sta->flags |= WLAN_STA_ASSOC;

	if (reassoc)
		mlme_reassociate_indication(hapd, sta);
	else
		mlme_associate_indication(hapd, sta);

#ifdef CONFIG_IEEE80211N
	if (sta->flags & WLAN_STA_HT) {
		ht_cap_ptr = &ht_cap;
		hostapd_get_ht_capab(hapd, &sta->ht_capabilities, ht_cap_ptr);
	}
#endif /* CONFIG_IEEE80211N */

#ifdef CONFIG_IEEE80211W
	sta->sa_query_timed_out = 0;
#endif /* CONFIG_IEEE80211W */

	/*
	 * Remove the STA entry in order to make sure the STA PS state gets
	 * cleared and configuration gets updated in case of reassociation back
	 * to the same AP.
	 */
	hostapd_sta_remove(hapd, sta->addr);

	if (hostapd_sta_add(hapd->conf->iface, hapd, sta->addr, sta->aid,
			    sta->capability, sta->supported_rates,
			    sta->supported_rates_len, 0, sta->listen_interval,
			    ht_cap_ptr))
	{
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "Could not add STA to kernel driver");
	}

	if (sta->eapol_sm == NULL) {
		/*
		 * This STA does not use RADIUS server for EAP authentication,
		 * so bind it to the selected VLAN interface now, since the
		 * interface selection is not going to change anymore.
		 */
		ap_sta_bind_vlan(hapd, sta, 0);
	} else if (sta->vlan_id) {
		/* VLAN ID already set (e.g., by PMKSA caching), so bind STA */
		ap_sta_bind_vlan(hapd, sta, 0);
	}

	set_flags = WLAN_STA_SHORT_PREAMBLE | WLAN_STA_WMM | WLAN_STA_MFP;
	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa &&
	    sta->flags & WLAN_STA_AUTHORIZED)
		set_flags |= WLAN_STA_AUTHORIZED;
	flags_or = sta->flags & set_flags;
	flags_and = sta->flags | ~set_flags;
	hostapd_sta_set_flags(hapd, sta->addr, sta->flags,
			      flags_or, flags_and);

	if (sta->auth_alg == WLAN_AUTH_FT)
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
	else
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);

	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);

 fail:
	/* Copy of the association request is not needed anymore */
	if (sta->last_assoc_req) {
		os_free(sta->last_assoc_req);
		sta->last_assoc_req = NULL;
	}
}


/**
 * ieee802_11_mgmt_cb - Process management frame TX status callback
 * @hapd: hostapd BSS data structure (the BSS from which the management frame
 * was sent from)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @stype: management frame subtype from frame control field
 * @ok: Whether the frame was ACK'ed
 */
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, u8 *buf, size_t len,
			u16 stype, int ok)
{
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) buf;

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth cb");
		handle_auth_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 0, ok);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 1, ok);
		break;
	case WLAN_FC_STYPE_PROBE_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::proberesp cb");
		break;
	case WLAN_FC_STYPE_DEAUTH:
		/* ignore */
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action cb");
		break;
	default:
		printf("unknown mgmt cb frame subtype %d\n", stype);
		break;
	}
}


static void ieee80211_tkip_countermeasures_stop(void *eloop_ctx,
						void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	hapd->tkip_countermeasures = 0;
	hostapd_set_countermeasures(hapd, 0);
	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "TKIP countermeasures ended");
}


static void ieee80211_tkip_countermeasures_start(struct hostapd_data *hapd)
{
	struct sta_info *sta;

	hostapd_logger(hapd, NULL, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "TKIP countermeasures initiated");

	wpa_auth_countermeasures_start(hapd->wpa_auth);
	hapd->tkip_countermeasures = 1;
	hostapd_set_countermeasures(hapd, 1);
	wpa_gtk_rekey(hapd->wpa_auth);
	eloop_cancel_timeout(ieee80211_tkip_countermeasures_stop, hapd, NULL);
	eloop_register_timeout(60, 0, ieee80211_tkip_countermeasures_stop,
			       hapd, NULL);
	for (sta = hapd->sta_list; sta != NULL; sta = sta->next) {
		hostapd_sta_deauth(hapd, sta->addr,
				   WLAN_REASON_MICHAEL_MIC_FAILURE);
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC |
				WLAN_STA_AUTHORIZED);
		hostapd_sta_remove(hapd, sta->addr);
	}
}


void ieee80211_michael_mic_failure(struct hostapd_data *hapd, const u8 *addr,
				   int local)
{
	time_t now;

	if (addr && local) {
		struct sta_info *sta = ap_get_sta(hapd, addr);
		if (sta != NULL) {
			wpa_auth_sta_local_mic_failure_report(sta->wpa_sm);
			hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Michael MIC failure detected in "
				       "received frame");
			mlme_michaelmicfailure_indication(hapd, addr);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "for not associated STA (" MACSTR
				   ") ignored", MAC2STR(addr));
			return;
		}
	}

	time(&now);
	if (now > hapd->michael_mic_failure + 60) {
		hapd->michael_mic_failures = 1;
	} else {
		hapd->michael_mic_failures++;
		if (hapd->michael_mic_failures > 1)
			ieee80211_tkip_countermeasures_start(hapd);
	}
	hapd->michael_mic_failure = now;
}


int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}


int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}

#endif /* CONFIG_NATIVE_WINDOWS */
