/*
 * WPA Supplicant - RSN pre-authentication and PMKSA caching
 * Copyright (c) 2003-2006, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <sys/time.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <netinet/in.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <string.h>
#include <time.h>

#include "common.h"
#include "sha1.h"
#include "wpa.h"
#include "driver.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "l2_packet.h"
#include "eapol_sm.h"
#include "preauth.h"
#include "wpa_i.h"


#define PMKID_CANDIDATE_PRIO_SCAN 1000
static const int pmksa_cache_max_entries = 32;


struct rsn_pmksa_candidate {
	struct rsn_pmksa_candidate *next;
	u8 bssid[ETH_ALEN];
	int priority;
};


/**
 * rsn_pmkid - Calculate PMK identifier
 * @pmk: Pairwise master key
 * @aa: Authenticator address
 * @spa: Supplicant address
 *
 * IEEE Std 802.11i-2004 - 8.5.1.2 Pairwise key hierarchy
 * PMKID = HMAC-SHA1-128(PMK, "PMK Name" || AA || SPA)
 */
static void rsn_pmkid(const u8 *pmk, const u8 *aa, const u8 *spa, u8 *pmkid)
{
	char *title = "PMK Name";
	const unsigned char *addr[3];
	const size_t len[3] = { 8, ETH_ALEN, ETH_ALEN };
	unsigned char hash[SHA1_MAC_LEN];

	addr[0] = (unsigned char *) title;
	addr[1] = aa;
	addr[2] = spa;

	hmac_sha1_vector(pmk, PMK_LEN, 3, addr, len, hash);
	memcpy(pmkid, hash, PMKID_LEN);
}


static void pmksa_cache_set_expiration(struct wpa_sm *sm);


static void pmksa_cache_free_entry(struct wpa_sm *sm,
				   struct rsn_pmksa_cache *entry, int replace)
{
	int current;

	current = sm->cur_pmksa == entry ||
		(sm->pmk_len == entry->pmk_len &&
		 memcmp(sm->pmk, entry->pmk, sm->pmk_len) == 0);

	free(entry);
	sm->pmksa_count--;

	if (current) {
		wpa_printf(MSG_DEBUG, "RSN: removed current PMKSA entry");
		sm->cur_pmksa = NULL;

		if (replace) {
			/* A new entry is being added, so no need to
			 * deauthenticate in this case. This happens when EAP
			 * authentication is completed again (reauth or failed
			 * PMKSA caching attempt). */
			return;
		}

		memset(sm->pmk, 0, sizeof(sm->pmk));
		wpa_sm_deauthenticate(sm, REASON_UNSPECIFIED);
		wpa_sm_req_scan(sm, 0, 0);
	}
}


static void pmksa_cache_expire(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;
	time_t now;

	time(&now);
	while (sm->pmksa && sm->pmksa->expiration <= now) {
		struct rsn_pmksa_cache *entry = sm->pmksa;
		sm->pmksa = entry->next;
		wpa_printf(MSG_DEBUG, "RSN: expired PMKSA cache entry for "
			   MACSTR, MAC2STR(entry->aa));
		pmksa_cache_free_entry(sm, entry, 0);
	}

	pmksa_cache_set_expiration(sm);
}


static void pmksa_cache_reauth(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;
	sm->cur_pmksa = NULL;
	eapol_sm_request_reauth(sm->eapol);
}


static void pmksa_cache_set_expiration(struct wpa_sm *sm)
{
	int sec;
	struct rsn_pmksa_cache *entry;

	eloop_cancel_timeout(pmksa_cache_expire, sm, NULL);
	eloop_cancel_timeout(pmksa_cache_reauth, sm, NULL);
	if (sm->pmksa == NULL)
		return;
	sec = sm->pmksa->expiration - time(NULL);
	if (sec < 0)
		sec = 0;
	eloop_register_timeout(sec + 1, 0, pmksa_cache_expire, sm, NULL);

	entry = sm->cur_pmksa ? sm->cur_pmksa :
		pmksa_cache_get(sm, sm->bssid, NULL);
	if (entry) {
		sec = sm->pmksa->reauth_time - time(NULL);
		if (sec < 0)
			sec = 0;
		eloop_register_timeout(sec, 0, pmksa_cache_reauth, sm, NULL);
	}
}


/**
 * pmksa_cache_add - Add a PMKSA cache entry
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @pmk: The new pairwise master key
 * @pmk_len: PMK length in bytes, usually PMK_LEN (32)
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @ssid: The network configuration for which this PMK is being added
 * Returns: Pointer to the added PMKSA cache entry or %NULL on error
 *
 * This function create a PMKSA entry for a new PMK and adds it to the PMKSA
 * cache. If an old entry is already in the cache for the same Authenticator,
 * this entry will be replaced with the new entry. PMKID will be calculated
 * based on the PMK and the driver interface is notified of the new PMKID.
 */
struct rsn_pmksa_cache *
pmksa_cache_add(struct wpa_sm *sm, const u8 *pmk,
		size_t pmk_len, const u8 *aa, const u8 *spa,
		struct wpa_ssid *ssid)
{
	struct rsn_pmksa_cache *entry, *pos, *prev;
	time_t now;

	if (sm->proto != WPA_PROTO_RSN || pmk_len > PMK_LEN)
		return NULL;

	entry = malloc(sizeof(*entry));
	if (entry == NULL)
		return NULL;
	memset(entry, 0, sizeof(*entry));
	memcpy(entry->pmk, pmk, pmk_len);
	entry->pmk_len = pmk_len;
	rsn_pmkid(pmk, aa, spa, entry->pmkid);
	now = time(NULL);
	entry->expiration = now + sm->dot11RSNAConfigPMKLifetime;
	entry->reauth_time = now + sm->dot11RSNAConfigPMKLifetime *
		sm->dot11RSNAConfigPMKReauthThreshold / 100;
	entry->akmp = WPA_KEY_MGMT_IEEE8021X;
	memcpy(entry->aa, aa, ETH_ALEN);
	entry->ssid = ssid;

	/* Replace an old entry for the same Authenticator (if found) with the
	 * new entry */
	pos = sm->pmksa;
	prev = NULL;
	while (pos) {
		if (memcmp(aa, pos->aa, ETH_ALEN) == 0) {
			if (pos->pmk_len == pmk_len &&
			    memcmp(pos->pmk, pmk, pmk_len) == 0 &&
			    memcmp(pos->pmkid, entry->pmkid, PMKID_LEN) == 0) {
				wpa_printf(MSG_DEBUG, "WPA: reusing previous "
					   "PMKSA entry");
				free(entry);
				return pos;
			}
			if (prev == NULL)
				sm->pmksa = pos->next;
			else
				prev->next = pos->next;
			wpa_printf(MSG_DEBUG, "RSN: Replace PMKSA entry for "
				   "the current AP");
			pmksa_cache_free_entry(sm, pos, 1);
			break;
		}
		prev = pos;
		pos = pos->next;
	}

	if (sm->pmksa_count >= pmksa_cache_max_entries && sm->pmksa) {
		/* Remove the oldest entry to make room for the new entry */
		pos = sm->pmksa;
		sm->pmksa = pos->next;
		wpa_printf(MSG_DEBUG, "RSN: removed the oldest PMKSA cache "
			   "entry (for " MACSTR ") to make room for new one",
			   MAC2STR(pos->aa));
		wpa_sm_remove_pmkid(sm, pos->aa, pos->pmkid);
		pmksa_cache_free_entry(sm, pos, 0);
	}

	/* Add the new entry; order by expiration time */
	pos = sm->pmksa;
	prev = NULL;
	while (pos) {
		if (pos->expiration > entry->expiration)
			break;
		prev = pos;
		pos = pos->next;
	}
	if (prev == NULL) {
		entry->next = sm->pmksa;
		sm->pmksa = entry;
		pmksa_cache_set_expiration(sm);
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	sm->pmksa_count++;
	wpa_printf(MSG_DEBUG, "RSN: added PMKSA cache entry for " MACSTR,
		   MAC2STR(entry->aa));
	wpa_sm_add_pmkid(sm, entry->aa, entry->pmkid);

	return entry;
}


/**
 * pmksa_cache_free - Free all entries in PMKSA cache
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void pmksa_cache_free(struct wpa_sm *sm)
{
	struct rsn_pmksa_cache *entry, *prev;

	if (sm == NULL)
		return;

	entry = sm->pmksa;
	sm->pmksa = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		free(prev);
	}
	pmksa_cache_set_expiration(sm);
	sm->cur_pmksa = NULL;
}


/**
 * pmksa_cache_get - Fetch a PMKSA cache entry
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @aa: Authenticator address or %NULL to match any
 * @pmkid: PMKID or %NULL to match any
 * Returns: Pointer to PMKSA cache entry or %NULL if no match was found
 */
struct rsn_pmksa_cache * pmksa_cache_get(struct wpa_sm *sm,
					 const u8 *aa, const u8 *pmkid)
{
	struct rsn_pmksa_cache *entry = sm->pmksa;
	while (entry) {
		if ((aa == NULL || memcmp(entry->aa, aa, ETH_ALEN) == 0) &&
		    (pmkid == NULL ||
		     memcmp(entry->pmkid, pmkid, PMKID_LEN) == 0))
			return entry;
		entry = entry->next;
	}
	return NULL;
}


/**
 * pmksa_cache_notify_reconfig - Reconfiguration notification for PMKSA cache
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * Clear references to old data structures when wpa_supplicant is reconfigured.
 */
void pmksa_cache_notify_reconfig(struct wpa_sm *sm)
{
	struct rsn_pmksa_cache *entry = sm->pmksa;
	while (entry) {
		entry->ssid = NULL;
		entry = entry->next;
	}
}


static struct rsn_pmksa_cache *
pmksa_cache_clone_entry(struct wpa_sm *sm,
			const struct rsn_pmksa_cache *old_entry, const u8 *aa)
{
	struct rsn_pmksa_cache *new_entry;

	new_entry = pmksa_cache_add(sm, old_entry->pmk, old_entry->pmk_len,
				    aa, sm->own_addr, old_entry->ssid);
	if (new_entry == NULL)
		return NULL;

	/* TODO: reorder entries based on expiration time? */
	new_entry->expiration = old_entry->expiration;
	new_entry->opportunistic = 1;

	return new_entry;
}


/**
 * pmksa_cache_get_opportunistic - Try to get an opportunistic PMKSA entry
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ssid: Pointer to the current network configuration
 * @aa: Authenticator address for the new AP
 * Returns: Pointer to a new PMKSA cache entry or %NULL if not available
 *
 * Try to create a new PMKSA cache entry opportunistically by guessing that the
 * new AP is sharing the same PMK as another AP that has the same SSID and has
 * already an entry in PMKSA cache.
 */
static struct rsn_pmksa_cache *
pmksa_cache_get_opportunistic(struct wpa_sm *sm,
			      struct wpa_ssid *ssid, const u8 *aa)
{
	struct rsn_pmksa_cache *entry = sm->pmksa;

	if (ssid == NULL)
		return NULL;
	while (entry) {
		if (entry->ssid == ssid) {
			entry = pmksa_cache_clone_entry(sm, entry, aa);
			if (entry) {
				wpa_printf(MSG_DEBUG, "RSN: added "
					   "opportunistic PMKSA cache entry "
					   "for " MACSTR, MAC2STR(aa));
			}
			return entry;
		}
		entry = entry->next;
	}
	return NULL;
}


/**
 * pmksa_cache_get_current - Get the current used PMKSA entry
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * Returns: Pointer to the current PMKSA cache entry or %NULL if not available
 */
struct rsn_pmksa_cache * pmksa_cache_get_current(struct wpa_sm *sm)
{
	if (sm == NULL)
		return NULL;
	return sm->cur_pmksa;
}


/**
 * pmksa_cache_clear_current - Clear the current PMKSA entry selection
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void pmksa_cache_clear_current(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;
	sm->cur_pmksa = NULL;
}


/**
 * pmksa_cache_set_current - Set the current PMKSA entry selection
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @pmkid: PMKID for selecting PMKSA or %NULL if not used
 * @bssid: BSSID for PMKSA  or %NULL if not used
 * @ssid: The network configuration for the current network
 * @try_opportunistic: Whether to allow opportunistic PMKSA caching
 * Returns: 0 if PMKSA was found or -1 if no matching entry was found
 */
int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid,
			    const u8 *bssid, struct wpa_ssid *ssid,
			    int try_opportunistic)
{
	sm->cur_pmksa = NULL;
	if (pmkid)
		sm->cur_pmksa = pmksa_cache_get(sm, NULL, pmkid);
	if (sm->cur_pmksa == NULL && bssid)
		sm->cur_pmksa = pmksa_cache_get(sm, bssid, NULL);
	if (sm->cur_pmksa == NULL && try_opportunistic)
		sm->cur_pmksa = pmksa_cache_get_opportunistic(sm, ssid, bssid);
	if (sm->cur_pmksa) {
		wpa_hexdump(MSG_DEBUG, "RSN: PMKID",
			    sm->cur_pmksa->pmkid, PMKID_LEN);
		return 0;
	}
	return -1;
}


/**
 * pmksa_cache_list - Dump text list of entries in PMKSA cache
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for the list
 * @len: Length of the buffer
 * Returns: number of bytes written to buffer
 *
 * This function is used to generate a text format representation of the
 * current PMKSA cache contents for the ctrl_iface PMKSA command.
 */
int pmksa_cache_list(struct wpa_sm *sm, char *buf, size_t len)
{
	int i, j;
	char *pos = buf;
	struct rsn_pmksa_cache *entry;
	time_t now;

	time(&now);
	pos += snprintf(pos, buf + len - pos,
			"Index / AA / PMKID / expiration (in seconds) / "
			"opportunistic\n");
	i = 0;
	entry = sm->pmksa;
	while (entry) {
		i++;
		pos += snprintf(pos, buf + len - pos, "%d " MACSTR " ",
				i, MAC2STR(entry->aa));
		for (j = 0; j < PMKID_LEN; j++)
			pos += snprintf(pos, buf + len - pos, "%02x",
					entry->pmkid[j]);
		pos += snprintf(pos, buf + len - pos, " %d %d\n",
				(int) (entry->expiration - now),
				entry->opportunistic);
		entry = entry->next;
	}
	return pos - buf;
}


/**
 * pmksa_candidate_free - Free all entries in PMKSA candidate list
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void pmksa_candidate_free(struct wpa_sm *sm)
{
	struct rsn_pmksa_candidate *entry, *prev;

	if (sm == NULL)
		return;

	entry = sm->pmksa_candidates;
	sm->pmksa_candidates = NULL;
	while (entry) {
		prev = entry;
		entry = entry->next;
		free(prev);
	}
}


#ifdef IEEE8021X_EAPOL

static void rsn_preauth_receive(void *ctx, const u8 *src_addr,
				const u8 *buf, size_t len)
{
	struct wpa_sm *sm = ctx;

	wpa_printf(MSG_DEBUG, "RX pre-auth from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX pre-auth", buf, len);

	if (sm->preauth_eapol == NULL ||
	    memcmp(sm->preauth_bssid, "\x00\x00\x00\x00\x00\x00",
		   ETH_ALEN) == 0 ||
	    memcmp(sm->preauth_bssid, src_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_WARNING, "RSN pre-auth frame received from "
			   "unexpected source " MACSTR " - dropped",
			   MAC2STR(src_addr));
		return;
	}

	eapol_sm_rx_eapol(sm->preauth_eapol, src_addr, buf, len);
}


static void rsn_preauth_eapol_cb(struct eapol_sm *eapol, int success,
				 void *ctx)
{
	struct wpa_sm *sm = ctx;
	u8 pmk[PMK_LEN];

	if (success) {
		int res, pmk_len;
		pmk_len = PMK_LEN;
		res = eapol_sm_get_key(eapol, pmk, PMK_LEN);
#ifdef EAP_LEAP
		if (res) {
			res = eapol_sm_get_key(eapol, pmk, 16);
			pmk_len = 16;
		}
#endif /* EAP_LEAP */
		if (res == 0) {
			wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from pre-auth",
					pmk, pmk_len);
			sm->pmk_len = pmk_len;
			pmksa_cache_add(sm, pmk, pmk_len,
					sm->preauth_bssid, sm->own_addr,
					sm->cur_ssid);
		} else {
			wpa_msg(sm->ctx->ctx, MSG_INFO, "RSN: failed to get "
				"master session key from pre-auth EAPOL state "
				"machines");
			success = 0;
		}
	}

	wpa_msg(sm->ctx->ctx, MSG_INFO, "RSN: pre-authentication with " MACSTR
		" %s", MAC2STR(sm->preauth_bssid),
		success ? "completed successfully" : "failed");

	rsn_preauth_deinit(sm);
	rsn_preauth_candidate_process(sm);
}


static void rsn_preauth_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;

	wpa_msg(sm->ctx->ctx, MSG_INFO, "RSN: pre-authentication with " MACSTR
		" timed out", MAC2STR(sm->preauth_bssid));
	rsn_preauth_deinit(sm);
	rsn_preauth_candidate_process(sm);
}


static int rsn_preauth_eapol_send(void *ctx, int type, const u8 *buf,
				  size_t len)
{
	struct wpa_sm *sm = ctx;
	u8 *msg;
	size_t msglen;
	int res;

	/* TODO: could add l2_packet_sendmsg that allows fragments to avoid
	 * extra copy here */

	if (sm->l2_preauth == NULL)
		return -1;

	msg = wpa_sm_alloc_eapol(sm, type, buf, len, &msglen, NULL);
	if (msg == NULL)
		return -1;

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL (preauth)", msg, msglen);
	res = l2_packet_send(sm->l2_preauth, sm->preauth_bssid,
			     ETH_P_RSN_PREAUTH, msg, msglen);
	free(msg);
	return res;
}


/**
 * rsn_preauth_init - Start new RSN pre-authentication
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @dst: Authenticator address (BSSID) with which to preauthenticate
 * @config: Current network configuration
 * Returns: 0 on success, -1 on another pre-authentication is in progress,
 * -2 on layer 2 packet initialization failure, -3 on EAPOL state machine
 * initialization failure, -4 on memory allocation failure
 *
 * This function request an RSN pre-authentication with a given destination
 * address. This is usually called for PMKSA candidates found from scan results
 * or from driver reports. In addition, ctrl_iface PREAUTH command can trigger
 * pre-authentication.
 */
int rsn_preauth_init(struct wpa_sm *sm, const u8 *dst, struct wpa_ssid *config)
{
	struct eapol_config eapol_conf;
	struct eapol_ctx *ctx;

	if (sm->preauth_eapol)
		return -1;

	wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: starting pre-authentication "
		"with " MACSTR, MAC2STR(dst));

	sm->l2_preauth = l2_packet_init(sm->ifname, sm->own_addr,
					ETH_P_RSN_PREAUTH,
					rsn_preauth_receive, sm, 0);
	if (sm->l2_preauth == NULL) {
		wpa_printf(MSG_WARNING, "RSN: Failed to initialize L2 packet "
			   "processing for pre-authentication");
		return -2;
	}

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_WARNING, "Failed to allocate EAPOL context.");
		return -4;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = sm->ctx->ctx;
	ctx->msg_ctx = sm->ctx->ctx;
	ctx->preauth = 1;
	ctx->cb = rsn_preauth_eapol_cb;
	ctx->cb_ctx = sm;
	ctx->scard_ctx = sm->scard_ctx;
	ctx->eapol_send = rsn_preauth_eapol_send;
	ctx->eapol_send_ctx = sm;
	ctx->set_config_blob = sm->ctx->set_config_blob;
	ctx->get_config_blob = sm->ctx->get_config_blob;

	sm->preauth_eapol = eapol_sm_init(ctx);
	if (sm->preauth_eapol == NULL) {
		free(ctx);
		wpa_printf(MSG_WARNING, "RSN: Failed to initialize EAPOL "
			   "state machines for pre-authentication");
		return -3;
	}
	memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.accept_802_1x_keys = 0;
	eapol_conf.required_keys = 0;
	eapol_conf.fast_reauth = sm->fast_reauth;
	if (config)
		eapol_conf.workaround = config->eap_workaround;
	eapol_sm_notify_config(sm->preauth_eapol, config, &eapol_conf);
	/*
	 * Use a shorter startPeriod with preauthentication since the first
	 * preauth EAPOL-Start frame may end up being dropped due to race
	 * condition in the AP between the data receive and key configuration
	 * after the 4-Way Handshake.
	 */
	eapol_sm_configure(sm->preauth_eapol, -1, -1, 5, 6);
	memcpy(sm->preauth_bssid, dst, ETH_ALEN);

	eapol_sm_notify_portValid(sm->preauth_eapol, TRUE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(sm->preauth_eapol, TRUE);

	eloop_register_timeout(sm->dot11RSNAConfigSATimeout, 0,
			       rsn_preauth_timeout, sm, NULL);

	return 0;
}


/**
 * rsn_preauth_deinit - Abort RSN pre-authentication
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * This function aborts the current RSN pre-authentication (if one is started)
 * and frees resources allocated for it.
 */
void rsn_preauth_deinit(struct wpa_sm *sm)
{
	if (sm == NULL || !sm->preauth_eapol)
		return;

	eloop_cancel_timeout(rsn_preauth_timeout, sm, NULL);
	eapol_sm_deinit(sm->preauth_eapol);
	sm->preauth_eapol = NULL;
	memset(sm->preauth_bssid, 0, ETH_ALEN);

	l2_packet_deinit(sm->l2_preauth);
	sm->l2_preauth = NULL;
}


/**
 * rsn_preauth_candidate_process - Process PMKSA candidates
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * Go through the PMKSA candidates and start pre-authentication if a candidate
 * without an existing PMKSA cache entry is found. Processed candidates will be
 * removed from the list.
 */
void rsn_preauth_candidate_process(struct wpa_sm *sm)
{
	struct rsn_pmksa_candidate *candidate;

	if (sm->pmksa_candidates == NULL)
		return;

	/* TODO: drop priority for old candidate entries */

	wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: processing PMKSA candidate "
		"list");
	if (sm->preauth_eapol ||
	    sm->proto != WPA_PROTO_RSN ||
	    wpa_sm_get_state(sm) != WPA_COMPLETED ||
	    sm->key_mgmt != WPA_KEY_MGMT_IEEE8021X) {
		wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: not in suitable state "
			"for new pre-authentication");
		return; /* invalid state for new pre-auth */
	}

	while (sm->pmksa_candidates) {
		struct rsn_pmksa_cache *p = NULL;
		candidate = sm->pmksa_candidates;
		p = pmksa_cache_get(sm, candidate->bssid, NULL);
		if (memcmp(sm->bssid, candidate->bssid, ETH_ALEN) != 0 &&
		    (p == NULL || p->opportunistic)) {
			wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: PMKSA "
				"candidate " MACSTR
				" selected for pre-authentication",
				MAC2STR(candidate->bssid));
			sm->pmksa_candidates = candidate->next;
			rsn_preauth_init(sm, candidate->bssid, sm->cur_ssid);
			free(candidate);
			return;
		}
		wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: PMKSA candidate "
			MACSTR " does not need pre-authentication anymore",
			MAC2STR(candidate->bssid));
		/* Some drivers (e.g., NDIS) expect to get notified about the
		 * PMKIDs again, so report the existing data now. */
		if (p) {
			wpa_sm_add_pmkid(sm, candidate->bssid, p->pmkid);
		}

		sm->pmksa_candidates = candidate->next;
		free(candidate);
	}
	wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: no more pending PMKSA "
		"candidates");
}


/**
 * pmksa_candidate_add - Add a new PMKSA candidate
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @bssid: BSSID (authenticator address) of the candidate
 * @prio: Priority (the smaller number, the higher priority)
 * @preauth: Whether the candidate AP advertises support for pre-authentication
 *
 * This function is used to add PMKSA candidates for RSN pre-authentication. It
 * is called from scan result processing and from driver events for PMKSA
 * candidates, i.e., EVENT_PMKID_CANDIDATE events to wpa_supplicant_event().
 */
void pmksa_candidate_add(struct wpa_sm *sm, const u8 *bssid,
			 int prio, int preauth)
{
	struct rsn_pmksa_candidate *cand, *prev, *pos;

	if (sm->cur_ssid && sm->cur_ssid->proactive_key_caching)
		pmksa_cache_get_opportunistic(sm, sm->cur_ssid, bssid);

	if (!preauth) {
		wpa_printf(MSG_DEBUG, "RSN: Ignored PMKID candidate without "
			   "preauth flag");
		return;
	}

	/* If BSSID already on candidate list, update the priority of the old
	 * entry. Do not override priority based on normal scan results. */
	prev = NULL;
	cand = sm->pmksa_candidates;
	while (cand) {
		if (memcmp(cand->bssid, bssid, ETH_ALEN) == 0) {
			if (prev)
				prev->next = cand->next;
			else
				sm->pmksa_candidates = cand->next;
			break;
		}
		prev = cand;
		cand = cand->next;
	}

	if (cand) {
		if (prio < PMKID_CANDIDATE_PRIO_SCAN)
			cand->priority = prio;
	} else {
		cand = malloc(sizeof(*cand));
		if (cand == NULL)
			return;
		memset(cand, 0, sizeof(*cand));
		memcpy(cand->bssid, bssid, ETH_ALEN);
		cand->priority = prio;
	}

	/* Add candidate to the list; order by increasing priority value. i.e.,
	 * highest priority (smallest value) first. */
	prev = NULL;
	pos = sm->pmksa_candidates;
	while (pos) {
		if (cand->priority <= pos->priority)
			break;
		prev = pos;
		pos = pos->next;
	}
	cand->next = pos;
	if (prev)
		prev->next = cand;
	else
		sm->pmksa_candidates = cand;

	wpa_msg(sm->ctx->ctx, MSG_DEBUG, "RSN: added PMKSA cache "
		"candidate " MACSTR " prio %d", MAC2STR(bssid), prio);
	rsn_preauth_candidate_process(sm);
}


/* TODO: schedule periodic scans if current AP supports preauth */

/**
 * rsn_preauth_scan_results - Process scan results to find PMKSA candidates
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @results: Scan results
 * @count: Number of BSSes in scan results
 *
 * This functions goes through the scan results and adds all suitable APs
 * (Authenticators) into PMKSA candidate list.
 */
void rsn_preauth_scan_results(struct wpa_sm *sm,
			      struct wpa_scan_result *results, int count)
{
	struct wpa_scan_result *r;
	struct wpa_ie_data ie;
	int i;
	struct rsn_pmksa_cache *pmksa;

	if (sm->cur_ssid == NULL)
		return;

	/*
	 * TODO: is it ok to free all candidates? What about the entries
	 * received from EVENT_PMKID_CANDIDATE?
	 */
	pmksa_candidate_free(sm);

	for (i = count - 1; i >= 0; i--) {
		r = &results[i];
		if (r->ssid_len != sm->cur_ssid->ssid_len ||
		    memcmp(r->ssid, sm->cur_ssid->ssid,
			   r->ssid_len) != 0)
			continue;

		if (memcmp(r->bssid, sm->bssid, ETH_ALEN) == 0)
			continue;

		if (r->rsn_ie_len == 0 ||
		    wpa_parse_wpa_ie(r->rsn_ie, r->rsn_ie_len, &ie))
			continue;

		pmksa = pmksa_cache_get(sm, r->bssid, NULL);
		if (pmksa &&
		    (!pmksa->opportunistic ||
		     !(ie.capabilities & WPA_CAPABILITY_PREAUTH)))
			continue;

		/*
		 * Give less priority to candidates found from normal
		 * scan results.
		 */
		pmksa_candidate_add(sm, r->bssid,
				    PMKID_CANDIDATE_PRIO_SCAN,
				    ie.capabilities & WPA_CAPABILITY_PREAUTH);
	}
}


/**
 * rsn_preauth_get_status - Get pre-authentication status
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for status information
 * @buflen: Maximum buffer length
 * @verbose: Whether to include verbose status information
 * Returns: Number of bytes written to buf.
 *
 * Query WPA2 pre-authentication for status information. This function fills in
 * a text area with current status information. If the buffer (buf) is not
 * large enough, status information will be truncated to fit the buffer.
 */
int rsn_preauth_get_status(struct wpa_sm *sm, char *buf, size_t buflen,
			   int verbose)
{
	char *pos = buf, *end = buf + buflen;
	int res;

	if (sm->preauth_eapol) {
		pos += snprintf(pos, end - pos, "Pre-authentication "
				"EAPOL state machines:\n");
		res = eapol_sm_get_status(sm->preauth_eapol,
					  pos, end - pos, verbose);
		if (res >= 0)
			pos += res;
	}

	return pos - buf;
}


/**
 * rsn_preauth_in_progress - Verify whether pre-authentication is in progress
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
int rsn_preauth_in_progress(struct wpa_sm *sm)
{
	return sm->preauth_eapol != NULL;
}

#endif /* IEEE8021X_EAPOL */
