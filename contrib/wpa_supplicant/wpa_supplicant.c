/*
 * WPA Supplicant
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <sys/socket.h>
#include <sys/un.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <unistd.h>
#include <ctype.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <netinet/in.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <fcntl.h>

#define OPENSSL_DISABLE_OLD_DES_SUPPORT
#include "common.h"
#include "eapol_sm.h"
#include "eap.h"
#include "wpa.h"
#include "driver.h"
#include "eloop.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "l2_packet.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"
#include "version.h"

static const char *wpa_supplicant_version =
"wpa_supplicant v" VERSION_STR "\n"
"Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi> and contributors";

static const char *wpa_supplicant_license =
"This program is free software. You can distribute it and/or modify it\n"
"under the terms of the GNU General Public License version 2.\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license. See README and COPYING for more details.\n"
#ifdef EAP_TLS_FUNCS
"\nThis product includes software developed by the OpenSSL Project\n"
"for use in the OpenSSL Toolkit (http://www.openssl.org/)\n"
#endif /* EAP_TLS_FUNCS */
;

static const char *wpa_supplicant_full_license =
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License version 2 as\n"
"published by the Free Software Foundation.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license.\n"
"\n"
"Redistribution and use in source and binary forms, with or without\n"
"modification, are permitted provided that the following conditions are\n"
"met:\n"
"\n"
"1. Redistributions of source code must retain the above copyright\n"
"   notice, this list of conditions and the following disclaimer.\n"
"\n"
"2. Redistributions in binary form must reproduce the above copyright\n"
"   notice, this list of conditions and the following disclaimer in the\n"
"   documentation and/or other materials provided with the distribution.\n"
"\n"
"3. Neither the name(s) of the above-listed copyright holder(s) nor the\n"
"   names of its contributors may be used to endorse or promote products\n"
"   derived from this software without specific prior written permission.\n"
"\n"
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
"\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
"LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
"A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
"OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
"LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
"DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
"THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n";

extern struct wpa_driver_ops *wpa_supplicant_drivers[];

static void wpa_supplicant_scan_results(struct wpa_supplicant *wpa_s);
static int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s,
				      int wait_for_interface);
static void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
				     struct wpa_scan_result *bss,
				     struct wpa_ssid *ssid);
static int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s,
				     struct wpa_scan_result *bss,
				     struct wpa_ssid *ssid,
				     u8 *wpa_ie, int *wpa_ie_len);


extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;


void wpa_msg(struct wpa_supplicant *wpa_s, int level, char *fmt, ...)
{
	va_list ap;
	char *buf;
	const int buflen = 2048;
	int len;

	buf = malloc(buflen);
	if (buf == NULL) {
		printf("Failed to allocate message buffer for:\n");
		va_start(ap, fmt);
		vprintf(fmt, ap);
		printf("\n");
		va_end(ap);
		return;
	}
	va_start(ap, fmt);
	len = vsnprintf(buf, buflen, fmt, ap);
	va_end(ap);
	wpa_printf(level, "%s", buf);
	wpa_supplicant_ctrl_iface_send(wpa_s, level, buf, len);
	free(buf);
}


int wpa_eapol_send(void *ctx, int type, u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	u8 *msg, *dst, bssid[ETH_ALEN];
	size_t msglen;
	struct l2_ethhdr *ethhdr;
	struct ieee802_1x_hdr *hdr;
	int res;

	/* TODO: could add l2_packet_sendmsg that allows fragments to avoid
	 * extra copy here */

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_NONE) {
		/* Current SSID is not using IEEE 802.1X/EAP, so drop possible
		 * EAPOL frames (mainly, EAPOL-Start) from EAPOL state
		 * machines. */
		wpa_printf(MSG_DEBUG, "WPA: drop TX EAPOL in non-IEEE 802.1X "
			   "mode (type=%d len=%lu)", type,
			   (unsigned long) len);
		return -1;
	}

	if (wpa_s->cur_pmksa && type == IEEE802_1X_TYPE_EAPOL_START) {
		/* Trying to use PMKSA caching - do not send EAPOL-Start frames
		 * since they will trigger full EAPOL authentication. */
		wpa_printf(MSG_DEBUG, "RSN: PMKSA caching - do not send "
			   "EAPOL-Start");
		return -1;
	}

	if (memcmp(wpa_s->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0) {
		wpa_printf(MSG_DEBUG, "BSSID not set when trying to send an "
			   "EAPOL frame");
		if (wpa_drv_get_bssid(wpa_s, bssid) == 0 &&
		    memcmp(bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) != 0) {
			dst = bssid;
			wpa_printf(MSG_DEBUG, "Using current BSSID " MACSTR
				   " from the driver as the EAPOL destination",
				   MAC2STR(dst));
		} else {
			dst = wpa_s->last_eapol_src;
			wpa_printf(MSG_DEBUG, "Using the source address of the"
				   " last received EAPOL frame " MACSTR " as "
				   "the EAPOL destination",
				   MAC2STR(dst));
		}
	} else {
		/* BSSID was already set (from (Re)Assoc event, so use it as
		 * the EAPOL destination. */
		dst = wpa_s->bssid;
	}

	msglen = sizeof(*ethhdr) + sizeof(*hdr) + len;
	msg = malloc(msglen);
	if (msg == NULL)
		return -1;

	ethhdr = (struct l2_ethhdr *) msg;
	memcpy(ethhdr->h_dest, dst, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_EAPOL);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = type;
	hdr->length = htons(len);

	memcpy((u8 *) (hdr + 1), buf, len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", msg, msglen);
	res = l2_packet_send(wpa_s->l2, msg, msglen);
	free(msg);
	return res;
}


int wpa_eapol_send_preauth(void *ctx, int type, u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	u8 *msg;
	size_t msglen;
	struct l2_ethhdr *ethhdr;
	struct ieee802_1x_hdr *hdr;
	int res;

	/* TODO: could add l2_packet_sendmsg that allows fragments to avoid
	 * extra copy here */

	if (wpa_s->l2_preauth == NULL)
		return -1;

	msglen = sizeof(*ethhdr) + sizeof(*hdr) + len;
	msg = malloc(msglen);
	if (msg == NULL)
		return -1;

	ethhdr = (struct l2_ethhdr *) msg;
	memcpy(ethhdr->h_dest, wpa_s->preauth_bssid, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_RSN_PREAUTH);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = type;
	hdr->length = htons(len);

	memcpy((u8 *) (hdr + 1), buf, len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL (preauth)", msg, msglen);
	res = l2_packet_send(wpa_s->l2_preauth, msg, msglen);
	free(msg);
	return res;
}


/**
 * wpa_eapol_set_wep_key - set WEP key for the driver
 * @ctx: pointer to wpa_supplicant data
 * @unicast: 1 = individual unicast key, 0 = broadcast key
 * @keyidx: WEP key index (0..3)
 * @key: pointer to key data
 * @keylen: key length in bytes
 *
 * Returns 0 on success or < 0 on error.
 */
static int wpa_eapol_set_wep_key(void *ctx, int unicast, int keyidx,
				 u8 *key, size_t keylen)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_s->keys_cleared = 0;
	return wpa_drv_set_key(wpa_s, WPA_ALG_WEP,
			       unicast ? wpa_s->bssid :
			       (u8 *) "\xff\xff\xff\xff\xff\xff",
			       keyidx, unicast, (u8 *) "", 0, key, keylen);
}


/* Configure default/group WEP key for static WEP */
static int wpa_set_wep_key(void *ctx, int set_tx, int keyidx, const u8 *key,
			   size_t keylen)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_s->keys_cleared = 0;
	return wpa_drv_set_key(wpa_s, WPA_ALG_WEP,
			       (u8 *) "\xff\xff\xff\xff\xff\xff",
			       keyidx, set_tx, (u8 *) "", 0, key, keylen);
}


static int wpa_supplicant_set_wpa_none_key(struct wpa_supplicant *wpa_s,
					   struct wpa_ssid *ssid)
{
	u8 key[32];
	size_t keylen;
	wpa_alg alg;
	u8 seq[6] = { 0 };

	/* IBSS/WPA-None uses only one key (Group) for both receiving and
	 * sending unicast and multicast packets. */

	if (ssid->mode != IEEE80211_MODE_IBSS) {
		wpa_printf(MSG_INFO, "WPA: Invalid mode %d (not IBSS/ad-hoc) "
			   "for WPA-None", ssid->mode);
		return -1;
	}

	if (!ssid->psk_set) {
		wpa_printf(MSG_INFO, "WPA: No PSK configured for WPA-None");
		return -1;
	}

	switch (wpa_s->group_cipher) {
	case WPA_CIPHER_CCMP:
		memcpy(key, ssid->psk, 16);
		keylen = 16;
		alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_TKIP:
		/* WPA-None uses the same Michael MIC key for both TX and RX */
		memcpy(key, ssid->psk, 16 + 8);
		memcpy(key + 16 + 8, ssid->psk + 16, 8);
		keylen = 32;
		alg = WPA_ALG_TKIP;
		break;
	default:
		wpa_printf(MSG_INFO, "WPA: Invalid group cipher %d for "
			   "WPA-None", wpa_s->group_cipher);
		return -1;
	}

	/* TODO: should actually remember the previously used seq#, both for TX
	 * and RX from each STA.. */

	return wpa_drv_set_key(wpa_s, alg, (u8 *) "\xff\xff\xff\xff\xff\xff",
			       0, 1, seq, 6, key, keylen);
}


void wpa_supplicant_notify_eapol_done(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_msg(wpa_s, MSG_DEBUG, "WPA: EAPOL processing complete");
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
	wpa_supplicant_cancel_auth_timeout(wpa_s);
}


static struct wpa_blacklist *
wpa_blacklist_get(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_blacklist *e;

	e = wpa_s->blacklist;
	while (e) {
		if (memcmp(e->bssid, bssid, ETH_ALEN) == 0)
			return e;
		e = e->next;
	}

	return NULL;
}


static int wpa_blacklist_add(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_blacklist *e;

	e = wpa_blacklist_get(wpa_s, bssid);
	if (e) {
		e->count++;
		wpa_printf(MSG_DEBUG, "BSSID " MACSTR " blacklist count "
			   "incremented to %d",
			   MAC2STR(bssid), e->count);
		return 0;
	}

	e = malloc(sizeof(*e));
	if (e == NULL)
		return -1;
	memset(e, 0, sizeof(*e));
	memcpy(e->bssid, bssid, ETH_ALEN);
	e->count = 1;
	e->next = wpa_s->blacklist;
	wpa_s->blacklist = e;
	wpa_printf(MSG_DEBUG, "Added BSSID " MACSTR " into blacklist",
		   MAC2STR(bssid));

	return 0;
}


static int wpa_blacklist_del(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_blacklist *e, *prev = NULL;

	e = wpa_s->blacklist;
	while (e) {
		if (memcmp(e->bssid, bssid, ETH_ALEN) == 0) {
			if (prev == NULL) {
				wpa_s->blacklist = e->next;
			} else {
				prev->next = e->next;
			}
			wpa_printf(MSG_DEBUG, "Removed BSSID " MACSTR " from "
				   "blacklist", MAC2STR(bssid));
			free(e);
			return 0;
		}
		prev = e;
		e = e->next;
	}
	return -1;
}


static void wpa_blacklist_clear(struct wpa_supplicant *wpa_s)
{
	struct wpa_blacklist *e, *prev;

	e = wpa_s->blacklist;
	wpa_s->blacklist = NULL;
	while (e) {
		prev = e;
		e = e->next;
		wpa_printf(MSG_DEBUG, "Removed BSSID " MACSTR " from "
			   "blacklist (clear)", MAC2STR(prev->bssid));
		free(prev);
	}
}


const char * wpa_ssid_txt(u8 *ssid, size_t ssid_len)
{
	static char ssid_txt[MAX_SSID_LEN + 1];
	char *pos;

	if (ssid_len > MAX_SSID_LEN)
		ssid_len = MAX_SSID_LEN;
	memcpy(ssid_txt, ssid, ssid_len);
	ssid_txt[ssid_len] = '\0';
	for (pos = ssid_txt; *pos != '\0'; pos++) {
		if ((u8) *pos < 32 || (u8) *pos >= 127)
			*pos = '_';
	}
	return ssid_txt;
}


void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec)
{
	wpa_msg(wpa_s, MSG_DEBUG, "Setting scan request: %d sec %d usec",
		sec, usec);
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
	eloop_register_timeout(sec, usec, wpa_supplicant_scan, wpa_s, NULL);
}


void wpa_supplicant_cancel_scan(struct wpa_supplicant *wpa_s)
{
	wpa_msg(wpa_s, MSG_DEBUG, "Cancelling scan request");
	eloop_cancel_timeout(wpa_supplicant_scan, wpa_s, NULL);
}


static void wpa_supplicant_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_msg(wpa_s, MSG_INFO, "Authentication with " MACSTR " timed out.",
		MAC2STR(wpa_s->bssid));
	wpa_blacklist_add(wpa_s, wpa_s->bssid);
	wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     int sec, int usec)
{
	wpa_msg(wpa_s, MSG_DEBUG, "Setting authentication timeout: %d sec "
		"%d usec", sec, usec);
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
	eloop_register_timeout(sec, usec, wpa_supplicant_timeout, wpa_s, NULL);
}


void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s)
{
	wpa_msg(wpa_s, MSG_DEBUG, "Cancelling authentication timeout");
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
	wpa_blacklist_del(wpa_s, wpa_s->bssid);
}


static void wpa_supplicant_initiate_eapol(struct wpa_supplicant *wpa_s)
{
	struct eapol_config eapol_conf;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK) {
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
		eapol_sm_notify_eap_fail(wpa_s->eapol, FALSE);
	}
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		eapol_sm_notify_portControl(wpa_s->eapol, ForceAuthorized);
	else
		eapol_sm_notify_portControl(wpa_s->eapol, Auto);

	memset(&eapol_conf, 0, sizeof(eapol_conf));
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		eapol_conf.accept_802_1x_keys = 1;
		eapol_conf.required_keys = 0;
		if (ssid->eapol_flags & EAPOL_FLAG_REQUIRE_KEY_UNICAST) {
			eapol_conf.required_keys |= EAPOL_REQUIRE_KEY_UNICAST;
		}
		if (ssid->eapol_flags & EAPOL_FLAG_REQUIRE_KEY_BROADCAST) {
			eapol_conf.required_keys |=
				EAPOL_REQUIRE_KEY_BROADCAST;
		}
	}
	eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;
	eapol_sm_notify_config(wpa_s->eapol, ssid, &eapol_conf);
}


static void wpa_supplicant_set_non_wpa_policy(struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid)
{
	int i;

	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA)
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X_NO_WPA;
	else
		wpa_s->key_mgmt = WPA_KEY_MGMT_NONE;
	free(wpa_s->ap_wpa_ie);
	wpa_s->ap_wpa_ie = NULL;
	wpa_s->ap_wpa_ie_len = 0;
	free(wpa_s->ap_rsn_ie);
	wpa_s->ap_rsn_ie = NULL;
	wpa_s->ap_rsn_ie_len = 0;
	free(wpa_s->assoc_wpa_ie);
	wpa_s->assoc_wpa_ie = NULL;
	wpa_s->assoc_wpa_ie_len = 0;
	wpa_s->pairwise_cipher = WPA_CIPHER_NONE;
	wpa_s->group_cipher = WPA_CIPHER_NONE;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i] > 5) {
			wpa_s->pairwise_cipher = WPA_CIPHER_WEP104;
			wpa_s->group_cipher = WPA_CIPHER_WEP104;
			break;
		} else if (ssid->wep_key_len[i] > 0) {
			wpa_s->pairwise_cipher = WPA_CIPHER_WEP40;
			wpa_s->group_cipher = WPA_CIPHER_WEP40;
			break;
		}
	}

	wpa_s->cur_pmksa = NULL;
}


static int wpa_supplicant_select_config(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;

	if (wpa_s->conf->ap_scan == 1)
		return 0;

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL) {
		wpa_printf(MSG_INFO, "No network configuration found for the "
			   "current AP");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Network configuration found for the current "
		   "AP");
	if (ssid->key_mgmt & (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X |
			      WPA_KEY_MGMT_WPA_NONE)) {
		u8 wpa_ie[80];
		int wpa_ie_len;
		wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					  wpa_ie, &wpa_ie_len);
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
	}

	wpa_s->current_ssid = ssid;
	wpa_supplicant_initiate_eapol(wpa_s);

	return 0;
}


static void wpa_supplicant_cleanup(struct wpa_supplicant *wpa_s)
{
	scard_deinit(wpa_s->scard);
	wpa_s->scard = NULL;
	eapol_sm_register_scard_ctx(wpa_s->eapol, NULL);
	l2_packet_deinit(wpa_s->l2);
	wpa_s->l2 = NULL;

#ifdef CONFIG_XSUPPLICANT_IFACE
	if (wpa_s->dot1x_s > -1) {
		close(wpa_s->dot1x_s);
		wpa_s->dot1x_s = -1;
	}
#endif /* CONFIG_XSUPPLICANT_IFACE */

	wpa_supplicant_ctrl_iface_deinit(wpa_s);
	if (wpa_s->conf != NULL) {
		wpa_config_free(wpa_s->conf);
		wpa_s->conf = NULL;
	}

	free(wpa_s->assoc_wpa_ie);
	wpa_s->assoc_wpa_ie = NULL;

	free(wpa_s->ap_wpa_ie);
	wpa_s->ap_wpa_ie = NULL;
	free(wpa_s->ap_rsn_ie);
	wpa_s->ap_rsn_ie = NULL;

	free(wpa_s->confname);
	wpa_s->confname = NULL;

	eapol_sm_deinit(wpa_s->eapol);
	wpa_s->eapol = NULL;

	rsn_preauth_deinit(wpa_s);

	pmksa_candidate_free(wpa_s);
	pmksa_cache_free(wpa_s);
	wpa_blacklist_clear(wpa_s);

	free(wpa_s->scan_results);
	wpa_s->scan_results = NULL;
	wpa_s->num_scan_results = 0;
}


static void wpa_clear_keys(struct wpa_supplicant *wpa_s, u8 *addr)
{
	u8 *bcast = (u8 *) "\xff\xff\xff\xff\xff\xff";

	if (wpa_s->keys_cleared) {
		/* Some drivers (e.g., ndiswrapper & NDIS drivers) seem to have
		 * timing issues with keys being cleared just before new keys
		 * are set or just after association or something similar. This
		 * shows up in group key handshake failing often because of the
		 * client not receiving the first encrypted packets correctly.
		 * Skipping some of the extra key clearing steps seems to help
		 * in completing group key handshake more reliably. */
		wpa_printf(MSG_DEBUG, "No keys have been configured - "
			   "skip key clearing");
		return;
	}

	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, bcast, 0, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, bcast, 1, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, bcast, 2, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, bcast, 3, 0, NULL, 0, NULL, 0);
	if (addr) {
		wpa_drv_set_key(wpa_s, WPA_ALG_NONE, addr, 0, 0, NULL, 0, NULL,
				0);
	}
	wpa_s->keys_cleared = 1;
}


static void wpa_supplicant_stop_countermeasures(void *eloop_ctx,
						void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->countermeasures) {
		wpa_s->countermeasures = 0;
		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_msg(wpa_s, MSG_INFO, "WPA: TKIP countermeasures stopped");
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
}


static void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s)
{
	wpa_s->wpa_state = WPA_DISCONNECTED;
	memset(wpa_s->bssid, 0, ETH_ALEN);
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK)
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
}


static void wpa_find_assoc_pmkid(struct wpa_supplicant *wpa_s)
{
	struct wpa_ie_data ie;
	int i;

	if (wpa_parse_wpa_ie(wpa_s, wpa_s->assoc_wpa_ie,
			     wpa_s->assoc_wpa_ie_len, &ie) < 0 ||
	    ie.pmkid == NULL)
		return;

	for (i = 0; i < ie.num_pmkid; i++) {
		wpa_s->cur_pmksa = pmksa_cache_get(wpa_s, NULL,
						   ie.pmkid + i * PMKID_LEN);
		if (wpa_s->cur_pmksa) {
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 1);
			break;
		}
	}

	wpa_printf(MSG_DEBUG, "RSN: PMKID from assoc IE %sfound from PMKSA "
		   "cache", wpa_s->cur_pmksa ? "" : "not ");
}


static void wpa_supplicant_add_pmkid_candidate(struct wpa_supplicant *wpa_s,
					       union wpa_event_data *data)
{
	if (data == NULL) {
		wpa_printf(MSG_DEBUG, "RSN: No data in PMKID candidate event");
		return;
	}
	wpa_printf(MSG_DEBUG, "RSN: PMKID candidate event - bssid=" MACSTR
		   " index=%d preauth=%d",
		   MAC2STR(data->pmkid_candidate.bssid),
		   data->pmkid_candidate.index,
		   data->pmkid_candidate.preauth);

	if (!data->pmkid_candidate.preauth) {
		wpa_printf(MSG_DEBUG, "RSN: Ignored PMKID candidate without "
			   "preauth flag");
		return;
	}

	pmksa_candidate_add(wpa_s, data->pmkid_candidate.bssid,
			    data->pmkid_candidate.index);
}


static int wpa_supplicant_dynamic_keys(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		return 0;

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    wpa_s->current_ssid &&
	    !(wpa_s->current_ssid->eapol_flags &
	      (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
	       EAPOL_FLAG_REQUIRE_KEY_BROADCAST))) {
		/* IEEE 802.1X, but not using dynamic WEP keys (i.e., either
		 * plaintext or static WEP keys). */
		return 0;
	}

	return 1;
}


static void wpa_supplicant_associnfo(struct wpa_supplicant *wpa_s,
				     union wpa_event_data *data)
{
	int l, len;
	u8 *p;

	wpa_printf(MSG_DEBUG, "Association info event");
	wpa_hexdump(MSG_DEBUG, "req_ies", data->assoc_info.req_ies,
		    data->assoc_info.req_ies_len);
	wpa_hexdump(MSG_DEBUG, "resp_ies", data->assoc_info.resp_ies,
		    data->assoc_info.resp_ies_len);
	if (wpa_s->assoc_wpa_ie) {
		free(wpa_s->assoc_wpa_ie);
		wpa_s->assoc_wpa_ie = NULL;
		wpa_s->assoc_wpa_ie_len = 0;
	}

	p = data->assoc_info.req_ies;
	l = data->assoc_info.req_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IE, if present. */
	while (l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in assoc_info",
				    p, l);
			break;
		}
		if ((p[0] == GENERIC_INFO_ELEM && p[1] >= 6 &&
		     (memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0)) ||
		    (p[0] == RSN_INFO_ELEM && p[1] >= 2)) {
			wpa_s->assoc_wpa_ie = malloc(len);
			if (wpa_s->assoc_wpa_ie == NULL)
				break;
			wpa_s->assoc_wpa_ie_len = len;
			memcpy(wpa_s->assoc_wpa_ie, p, len);
			wpa_hexdump(MSG_DEBUG, "assoc_wpa_ie",
				    wpa_s->assoc_wpa_ie,
				    wpa_s->assoc_wpa_ie_len);
			wpa_find_assoc_pmkid(wpa_s);
			break;
		}
		l -= len;
		p += len;
	}

	/* WPA/RSN IE from Beacon/ProbeResp */
	free(wpa_s->ap_wpa_ie);
	wpa_s->ap_wpa_ie = NULL;
	wpa_s->ap_wpa_ie_len = 0;
	free(wpa_s->ap_rsn_ie);
	wpa_s->ap_rsn_ie = NULL;
	wpa_s->ap_rsn_ie_len = 0;

	p = data->assoc_info.beacon_ies;
	l = data->assoc_info.beacon_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IEs, if present.
	 */
	while (l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in beacon_ies",
				    p, l);
			break;
		}
		if (wpa_s->ap_wpa_ie == NULL &&
		    p[0] == GENERIC_INFO_ELEM && p[1] >= 6 &&
		    memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0) {
			wpa_s->ap_wpa_ie = malloc(len);
			if (wpa_s->ap_wpa_ie) {
				memcpy(wpa_s->ap_wpa_ie, p, len);
				wpa_s->ap_wpa_ie_len = len;
			}
		}

		if (wpa_s->ap_rsn_ie == NULL &&
		    p[0] == RSN_INFO_ELEM && p[1] >= 2) {
			wpa_s->ap_rsn_ie = malloc(len);
			if (wpa_s->ap_rsn_ie) {
				memcpy(wpa_s->ap_rsn_ie, p, len);
				wpa_s->ap_rsn_ie_len = len;
			}

		}

		l -= len;
		p += len;
	}

}


void wpa_supplicant_event(struct wpa_supplicant *wpa_s, wpa_event_type event,
			  union wpa_event_data *data)
{
	int pairwise;
	time_t now;
	u8 bssid[ETH_ALEN];

	switch (event) {
	case EVENT_ASSOC:
		wpa_s->wpa_state = WPA_ASSOCIATED;
		wpa_printf(MSG_DEBUG, "Association event - clear replay "
			   "counter");
		memset(wpa_s->rx_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
		wpa_s->rx_replay_counter_set = 0;
		wpa_s->renew_snonce = 1;
		if (wpa_drv_get_bssid(wpa_s, bssid) >= 0 &&
		    memcmp(bssid, wpa_s->bssid, ETH_ALEN) != 0) {
			wpa_msg(wpa_s, MSG_DEBUG, "Associated to a new BSS: "
				"BSSID=" MACSTR, MAC2STR(bssid));
			memcpy(wpa_s->bssid, bssid, ETH_ALEN);
			if (wpa_supplicant_dynamic_keys(wpa_s)) {
				wpa_clear_keys(wpa_s, bssid);
			}
			wpa_supplicant_select_config(wpa_s);
		}
		wpa_msg(wpa_s, MSG_INFO, "Associated with " MACSTR,
			MAC2STR(bssid));
		/* Set portEnabled first to FALSE in order to get EAP state
		 * machine out of the SUCCESS state and eapSuccess cleared.
		 * Without this, EAPOL PAE state machine may transit to
		 * AUTHENTICATING state based on obsolete eapSuccess and then
		 * trigger BE_AUTH to SUCCESS and PAE to AUTHENTICATED without
		 * ever giving chance to EAP state machine to reset the state.
		 */
		eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
		eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_PSK)
			eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
		/* 802.1X::portControl = Auto */
		eapol_sm_notify_portEnabled(wpa_s->eapol, TRUE);
		wpa_s->eapol_received = 0;
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
		    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
			wpa_supplicant_cancel_auth_timeout(wpa_s);
		} else {
			/* Timeout for receiving the first EAPOL packet */
			wpa_supplicant_req_auth_timeout(wpa_s, 10, 0);
		}
		break;
	case EVENT_DISASSOC:
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
			/* At least Host AP driver and a Prism3 card seemed to
			 * be generating streams of disconnected events when
			 * configuring IBSS for WPA-None. Ignore them for now.
			 */
			wpa_printf(MSG_DEBUG, "Disconnect event - ignore in "
				   "IBSS/WPA-None mode");
			break;
		}
		if (wpa_s->wpa_state >= WPA_ASSOCIATED)
			wpa_supplicant_req_scan(wpa_s, 0, 100000);
		wpa_blacklist_add(wpa_s, wpa_s->bssid);
		wpa_supplicant_mark_disassoc(wpa_s);
		wpa_msg(wpa_s, MSG_INFO, "Disconnect event - remove keys");
		if (wpa_supplicant_dynamic_keys(wpa_s)) {
			wpa_s->keys_cleared = 0;
			wpa_clear_keys(wpa_s, wpa_s->bssid);
		}
		break;
	case EVENT_MICHAEL_MIC_FAILURE:
		wpa_msg(wpa_s, MSG_WARNING, "Michael MIC failure detected");
		pairwise = (data && data->michael_mic_failure.unicast);
		wpa_supplicant_key_request(wpa_s, 1, pairwise);
		time(&now);
		if (wpa_s->last_michael_mic_error &&
		    now - wpa_s->last_michael_mic_error <= 60) {
			/* initialize countermeasures */
			wpa_s->countermeasures = 1;
			wpa_msg(wpa_s, MSG_WARNING, "TKIP countermeasures "
				"started");

			/* Need to wait for completion of request frame. We do
			 * not get any callback for the message completion, so
			 * just wait a short while and hope for the best. */
			usleep(10000);

			wpa_drv_set_countermeasures(wpa_s, 1);
			wpa_supplicant_deauthenticate(
				wpa_s, REASON_MICHAEL_MIC_FAILURE);
			eloop_cancel_timeout(
				wpa_supplicant_stop_countermeasures, wpa_s,
				NULL);
			eloop_register_timeout(
				60, 0, wpa_supplicant_stop_countermeasures,
				wpa_s, NULL);
			/* TODO: mark the AP rejected for 60 second. STA is
			 * allowed to associate with another AP.. */
		}
		wpa_s->last_michael_mic_error = now;
		break;
	case EVENT_SCAN_RESULTS:
		wpa_supplicant_scan_results(wpa_s);
		break;
	case EVENT_ASSOCINFO:
		wpa_supplicant_associnfo(wpa_s, data);
		break;
	case EVENT_INTERFACE_STATUS:
		if (strcmp(wpa_s->ifname, data->interface_status.ifname) != 0)
			break;
		switch (data->interface_status.ievent) {
		case EVENT_INTERFACE_ADDED:
			if (!wpa_s->interface_removed)
				break;
			wpa_s->interface_removed = 0;
			wpa_printf(MSG_DEBUG, "Configured interface was "
				   "added.");
			if (wpa_supplicant_driver_init(wpa_s, 1) < 0) {
				wpa_printf(MSG_INFO, "Failed to initialize "
					   "the driver after interface was "
					   "added.");
			}
			break;
		case EVENT_INTERFACE_REMOVED:
			wpa_printf(MSG_DEBUG, "Configured interface was "
				   "removed.");
			wpa_s->interface_removed = 1;
			wpa_supplicant_mark_disassoc(wpa_s);
			l2_packet_deinit(wpa_s->l2);
			break;
		}
		break;
	case EVENT_PMKID_CANDIDATE:
		wpa_supplicant_add_pmkid_candidate(wpa_s, data);
		break;
	default:
		wpa_printf(MSG_INFO, "Unknown event %d", event);
		break;
	}
}


static void wpa_supplicant_terminate(int sig, void *eloop_ctx,
				     void *signal_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	for (wpa_s = wpa_s->head; wpa_s; wpa_s = wpa_s->next) {
		wpa_msg(wpa_s, MSG_INFO, "Signal %d received - terminating",
			sig);
	}
	eloop_terminate();
}


int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s)
{
	struct wpa_config *conf;
	int reconf_ctrl;
	if (wpa_s->confname == NULL)
		return -1;
	conf = wpa_config_read(wpa_s->confname);
	if (conf == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "Failed to parse the configuration "
			"file '%s' - exiting", wpa_s->confname);
		return -1;
	}

	reconf_ctrl = !!conf->ctrl_interface != !!wpa_s->conf->ctrl_interface
		|| (conf->ctrl_interface && wpa_s->conf->ctrl_interface &&
		    strcmp(conf->ctrl_interface, wpa_s->conf->ctrl_interface)
		    != 0);

	if (reconf_ctrl)
		wpa_supplicant_ctrl_iface_deinit(wpa_s);

	wpa_s->current_ssid = NULL;
	eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
	rsn_preauth_deinit(wpa_s);
	wpa_config_free(wpa_s->conf);
	wpa_s->conf = conf;
	if (reconf_ctrl)
		wpa_supplicant_ctrl_iface_init(wpa_s);
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
	wpa_msg(wpa_s, MSG_DEBUG, "Reconfiguration completed");
	return 0;
}


#ifndef CONFIG_NATIVE_WINDOWS
static void wpa_supplicant_reconfig(int sig, void *eloop_ctx,
				    void *signal_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_printf(MSG_DEBUG, "Signal %d received - reconfiguring", sig);
	for (wpa_s = wpa_s->head; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_supplicant_reload_configuration(wpa_s) < 0) {
			eloop_terminate();
		}
	}
}
#endif /* CONFIG_NATIVE_WINDOWS */


static void wpa_supplicant_gen_assoc_event(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;
	union wpa_event_data data;

	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL)
		return;

	wpa_printf(MSG_DEBUG, "Already associated with a configured network - "
		   "generating associated event");
	memset(&data, 0, sizeof(data));
	wpa_supplicant_event(wpa_s, EVENT_ASSOC, &data);
}


void wpa_supplicant_scan(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_ssid *ssid;

	if (wpa_s->conf->ap_scan == 0) {
		wpa_supplicant_gen_assoc_event(wpa_s);
		return;
	}

	if (wpa_s->conf->ap_scan == 2) {
		ssid = wpa_s->conf->ssid;
		if (ssid == NULL)
			return;
		wpa_supplicant_associate(wpa_s, NULL, ssid);
		return;
	}

	if (wpa_s->wpa_state == WPA_DISCONNECTED)
		wpa_s->wpa_state = WPA_SCANNING;

	ssid = wpa_s->conf->ssid;
	if (wpa_s->prev_scan_ssid != BROADCAST_SSID_SCAN) {
		while (ssid) {
			if (ssid == wpa_s->prev_scan_ssid) {
				ssid = ssid->next;
				break;
			}
			ssid = ssid->next;
		}
	}
	while (ssid) {
		if (ssid->scan_ssid)
			break;
		ssid = ssid->next;
	}

	wpa_printf(MSG_DEBUG, "Starting AP scan (%s SSID)",
		   ssid ? "specific": "broadcast");
	if (ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "Scan SSID",
				  ssid->ssid, ssid->ssid_len);
		wpa_s->prev_scan_ssid = ssid;
	} else
		wpa_s->prev_scan_ssid = BROADCAST_SSID_SCAN;

	if (wpa_drv_scan(wpa_s, ssid ? ssid->ssid : NULL,
			 ssid ? ssid->ssid_len : 0)) {
		wpa_printf(MSG_WARNING, "Failed to initiate AP scan.");
		wpa_supplicant_req_scan(wpa_s, 10, 0);
	}
}


static wpa_cipher cipher_suite2driver(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE:
		return CIPHER_NONE;
	case WPA_CIPHER_WEP40:
		return CIPHER_WEP40;
	case WPA_CIPHER_WEP104:
		return CIPHER_WEP104;
	case WPA_CIPHER_CCMP:
		return CIPHER_CCMP;
	case WPA_CIPHER_TKIP:
	default:
		return CIPHER_TKIP;
	}
}


static wpa_key_mgmt key_mgmt2driver(int key_mgmt)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_NONE:
		return KEY_MGMT_NONE;
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA:
		return KEY_MGMT_802_1X_NO_WPA;
	case WPA_KEY_MGMT_IEEE8021X:
		return KEY_MGMT_802_1X;
	case WPA_KEY_MGMT_WPA_NONE:
		return KEY_MGMT_WPA_NONE;
	case WPA_KEY_MGMT_PSK:
	default:
		return KEY_MGMT_PSK;
	}
}


static int wpa_supplicant_suites_from_ai(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 struct wpa_ie_data *ie) {
	if (wpa_s->assoc_wpa_ie == NULL)
		return -1;

	if (wpa_parse_wpa_ie(wpa_s, wpa_s->assoc_wpa_ie,
			     wpa_s->assoc_wpa_ie_len, ie)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Failed to parse WPA IE from "
			"association info");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "WPA: Using WPA IE from AssocReq to set cipher "
		   "suites");
	if (!(ie->group_cipher & ssid->group_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled group "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->group_cipher, ssid->group_cipher);
		return -1;
	}
	if (!(ie->pairwise_cipher & ssid->pairwise_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled pairwise "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->pairwise_cipher, ssid->pairwise_cipher);
		return -1;
	}
	if (!(ie->key_mgmt & ssid->key_mgmt)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled key "
			"management 0x%x (mask 0x%x) - reject",
			ie->key_mgmt, ssid->key_mgmt);
		return -1;
	}

	return 0;
}


static int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s,
				     struct wpa_scan_result *bss,
				     struct wpa_ssid *ssid,
				     u8 *wpa_ie, int *wpa_ie_len)
{
	struct wpa_ie_data ie;
	int sel, proto;
	u8 *ap_ie;
	size_t ap_ie_len;

	if (bss && bss->rsn_ie_len && (ssid->proto & WPA_PROTO_RSN)) {
		wpa_msg(wpa_s, MSG_DEBUG, "RSN: using IEEE 802.11i/D9.0");
		proto = WPA_PROTO_RSN;
		ap_ie = bss->rsn_ie;
		ap_ie_len = bss->rsn_ie_len;
	} else if (bss) {
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using IEEE 802.11i/D3.0");
		proto = WPA_PROTO_WPA;
		ap_ie = bss->wpa_ie;
		ap_ie_len = bss->wpa_ie_len;
	} else {
		if (ssid->proto & WPA_PROTO_RSN)
			proto = WPA_PROTO_RSN;
		else
			proto = WPA_PROTO_WPA;
		ap_ie = NULL;
		ap_ie_len = 0;
		if (wpa_supplicant_suites_from_ai(wpa_s, ssid, &ie) < 0) {
			memset(&ie, 0, sizeof(ie));
			ie.group_cipher = ssid->group_cipher;
			ie.pairwise_cipher = ssid->pairwise_cipher;
			ie.key_mgmt = ssid->key_mgmt;
			wpa_printf(MSG_DEBUG, "WPA: Set cipher suites based "
				   "on configuration");
		}
	}

	if (ap_ie && wpa_parse_wpa_ie(wpa_s, ap_ie, ap_ie_len, &ie)) {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to parse WPA IE for "
			"the selected BSS.");
		return -1;
	}
	wpa_printf(MSG_DEBUG, "WPA: Selected cipher suites: group %d "
		   "pairwise %d key_mgmt %d",
		   ie.group_cipher, ie.pairwise_cipher, ie.key_mgmt);

	wpa_s->proto = proto;

	free(wpa_s->ap_wpa_ie);
	wpa_s->ap_wpa_ie = NULL;
	wpa_s->ap_wpa_ie_len = 0;
	if (bss && bss->wpa_ie_len) {
		wpa_s->ap_wpa_ie = malloc(bss->wpa_ie_len);
		if (wpa_s->ap_wpa_ie == NULL) {
			wpa_printf(MSG_INFO, "WPA: malloc failed");
			return -1;
		}
		memcpy(wpa_s->ap_wpa_ie, bss->wpa_ie, bss->wpa_ie_len);
		wpa_s->ap_wpa_ie_len = bss->wpa_ie_len;
	}

	free(wpa_s->ap_rsn_ie);
	wpa_s->ap_rsn_ie = NULL;
	wpa_s->ap_rsn_ie_len = 0;
	if (bss && bss->rsn_ie_len) {
		wpa_s->ap_rsn_ie = malloc(bss->rsn_ie_len);
		if (wpa_s->ap_rsn_ie == NULL) {
			wpa_printf(MSG_INFO, "WPA: malloc failed");
			return -1;
		}
		memcpy(wpa_s->ap_rsn_ie, bss->rsn_ie, bss->rsn_ie_len);
		wpa_s->ap_rsn_ie_len = bss->rsn_ie_len;
	}

	sel = ie.group_cipher & ssid->group_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->group_cipher = WPA_CIPHER_CCMP;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using GTK CCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->group_cipher = WPA_CIPHER_TKIP;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using GTK TKIP");
	} else if (sel & WPA_CIPHER_WEP104) {
		wpa_s->group_cipher = WPA_CIPHER_WEP104;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP104");
	} else if (sel & WPA_CIPHER_WEP40) {
		wpa_s->group_cipher = WPA_CIPHER_WEP40;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP40");
	} else {
		wpa_printf(MSG_WARNING, "WPA: Failed to select group cipher.");
		return -1;
	}

	sel = ie.pairwise_cipher & ssid->pairwise_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_CCMP;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using PTK CCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_TKIP;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using PTK TKIP");
	} else if (sel & WPA_CIPHER_NONE) {
		wpa_s->pairwise_cipher = WPA_CIPHER_NONE;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using PTK NONE");
	} else {
		wpa_printf(MSG_WARNING, "WPA: Failed to select pairwise "
			   "cipher.");
		return -1;
	}

	sel = ie.key_mgmt & ssid->key_mgmt;
	if (sel & WPA_KEY_MGMT_IEEE8021X) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT 802.1X");
	} else if (sel & WPA_KEY_MGMT_PSK) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_PSK;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-PSK");
	} else if (sel & WPA_KEY_MGMT_WPA_NONE) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_WPA_NONE;
		wpa_msg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-NONE");
	} else {
		wpa_printf(MSG_WARNING, "WPA: Failed to select authenticated "
			   "key management type.");
		return -1;
	}

	*wpa_ie_len = wpa_gen_wpa_ie(wpa_s, wpa_ie);
	if (*wpa_ie_len < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to generate WPA IE.");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "WPA: Own WPA IE", wpa_ie, *wpa_ie_len);
	if (wpa_s->assoc_wpa_ie == NULL) {
		/*
		 * Make a copy of the WPA/RSN IE so that 4-Way Handshake gets
		 * the correct version of the IE even if PMKSA caching is
		 * aborted (which would remove PMKID from IE generation).
		 */
		wpa_s->assoc_wpa_ie = malloc(*wpa_ie_len);
		if (wpa_s->assoc_wpa_ie) {
			memcpy(wpa_s->assoc_wpa_ie, wpa_ie, *wpa_ie_len);
			wpa_s->assoc_wpa_ie_len = *wpa_ie_len;
		}
	}

	if (ssid->key_mgmt & WPA_KEY_MGMT_PSK) {
		wpa_s->pmk_len = PMK_LEN;
		memcpy(wpa_s->pmk, ssid->psk, PMK_LEN);
	} else if (wpa_s->cur_pmksa) {
		wpa_s->pmk_len = wpa_s->cur_pmksa->pmk_len;
		memcpy(wpa_s->pmk, wpa_s->cur_pmksa->pmk, wpa_s->pmk_len);
	} else {
		wpa_s->pmk_len = PMK_LEN;
		memset(wpa_s->pmk, 0, PMK_LEN);
#ifdef CONFIG_XSUPPLICANT_IFACE
		wpa_s->ext_pmk_received = 0;
#endif /* CONFIG_XSUPPLICANT_IFACE */
	}

	return 0;
}


static void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
				     struct wpa_scan_result *bss,
				     struct wpa_ssid *ssid)
{
	u8 wpa_ie[80];
	int wpa_ie_len;
	int use_crypt;
	int algs = AUTH_ALG_OPEN_SYSTEM;
	int cipher_pairwise, cipher_group;
	struct wpa_driver_associate_params params;
	int wep_keys_set = 0;
	struct wpa_driver_capa capa;

	wpa_s->reassociate = 0;
	if (bss) {
		wpa_msg(wpa_s, MSG_INFO, "Trying to associate with " MACSTR
			" (SSID='%s' freq=%d MHz)", MAC2STR(bss->bssid),
			wpa_ssid_txt(ssid->ssid, ssid->ssid_len), bss->freq);
		memset(wpa_s->bssid, 0, ETH_ALEN);
	} else {
		wpa_msg(wpa_s, MSG_INFO, "Trying to associate with SSID '%s'",
			wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
	}
	wpa_supplicant_cancel_scan(wpa_s);

	/* Starting new association, so clear the possibly used WPA IE from the
	 * previous association. */
	free(wpa_s->assoc_wpa_ie);
	wpa_s->assoc_wpa_ie = NULL;
	wpa_s->assoc_wpa_ie_len = 0;

	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if (ssid->leap) {
			if (ssid->non_leap == 0)
				algs = AUTH_ALG_LEAP;
			else
				algs |= AUTH_ALG_LEAP;
		}
	}
	wpa_printf(MSG_DEBUG, "Automatic auth_alg selection: 0x%x", algs);
	if (ssid->auth_alg) {
		algs = 0;
		if (ssid->auth_alg & WPA_AUTH_ALG_OPEN)
			algs |= AUTH_ALG_OPEN_SYSTEM;
		if (ssid->auth_alg & WPA_AUTH_ALG_SHARED)
			algs |= AUTH_ALG_SHARED_KEY;
		if (ssid->auth_alg & WPA_AUTH_ALG_LEAP)
			algs |= AUTH_ALG_LEAP;
		wpa_printf(MSG_DEBUG, "Overriding auth_alg selection: 0x%x",
			   algs);
	}
	wpa_drv_set_auth_alg(wpa_s, algs);

	if (bss && (bss->wpa_ie_len || bss->rsn_ie_len) &&
	    (ssid->key_mgmt & (WPA_KEY_MGMT_IEEE8021X | WPA_KEY_MGMT_PSK))) {
		wpa_s->cur_pmksa = pmksa_cache_get(wpa_s, bss->bssid, NULL);
		if (wpa_s->cur_pmksa) {
			wpa_hexdump(MSG_DEBUG, "RSN: PMKID",
				    wpa_s->cur_pmksa->pmkid, PMKID_LEN);
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 1);
		}
		if (wpa_supplicant_set_suites(wpa_s, bss, ssid,
					      wpa_ie, &wpa_ie_len)) {
			wpa_printf(MSG_WARNING, "WPA: Failed to set WPA key "
				   "management and encryption suites");
			return;
		}
	} else if (ssid->key_mgmt &
		   (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X |
		    WPA_KEY_MGMT_WPA_NONE)) {
		if (wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					      wpa_ie, &wpa_ie_len)) {
			wpa_printf(MSG_WARNING, "WPA: Failed to set WPA key "
				   "management and encryption suites (no scan "
				   "results)");
			return;
		}
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_ie_len = 0;
	}

	wpa_clear_keys(wpa_s, bss ? bss->bssid : NULL);
	use_crypt = 1;
	cipher_pairwise = cipher_suite2driver(wpa_s->pairwise_cipher);
	cipher_group = cipher_suite2driver(wpa_s->group_cipher);
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		int i;
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE)
			use_crypt = 0;
		for (i = 0; i < NUM_WEP_KEYS; i++) {
			if (ssid->wep_key_len[i]) {
				use_crypt = 1;
				wep_keys_set = 1;
				wpa_set_wep_key(wpa_s,
						i == ssid->wep_tx_keyidx,
						i, ssid->wep_key[i],
						ssid->wep_key_len[i]);
			}
		}
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if ((ssid->eapol_flags &
		     (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
		      EAPOL_FLAG_REQUIRE_KEY_BROADCAST)) == 0 &&
		    !wep_keys_set) {
			use_crypt = 0;
		} else {
			/* Assume that dynamic WEP-104 keys will be used and
			 * set cipher suites in order for drivers to expect
			 * encryption. */
			cipher_pairwise = cipher_group = CIPHER_WEP104;
		}
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/* Set the key before (and later after) association */
		wpa_supplicant_set_wpa_none_key(wpa_s, ssid);
	}

	wpa_drv_set_drop_unencrypted(wpa_s, use_crypt);
	wpa_s->wpa_state = WPA_ASSOCIATING;
	memset(&params, 0, sizeof(params));
	if (bss) {
		params.bssid = bss->bssid;
		params.ssid = bss->ssid;
		params.ssid_len = bss->ssid_len;
		params.freq = bss->freq;
	} else {
		params.ssid = ssid->ssid;
		params.ssid_len = ssid->ssid_len;
	}
	params.wpa_ie = wpa_ie;
	params.wpa_ie_len = wpa_ie_len;
	params.pairwise_suite = cipher_pairwise;
	params.group_suite = cipher_group;
	params.key_mgmt_suite = key_mgmt2driver(wpa_s->key_mgmt);
	params.auth_alg = algs;
	params.mode = ssid->mode;
	if (wpa_drv_associate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "Association request to the driver "
			"failed");
		/* try to continue anyway; new association will be tried again
		 * after timeout */
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/* Set the key after the association just in case association
		 * cleared the previously configured key. */
		wpa_supplicant_set_wpa_none_key(wpa_s, ssid);
		/* No need to timeout authentication since there is no key
		 * management. */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
	} else {
		/* Timeout for IEEE 802.11 authentication and association */
		wpa_supplicant_req_auth_timeout(wpa_s, 5, 0);
	}

	if (wep_keys_set && wpa_drv_get_capa(wpa_s, &capa) == 0 &&
	    capa.flags & WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC) {
		/* Set static WEP keys again */
		int i;
		for (i = 0; i < NUM_WEP_KEYS; i++) {
			if (ssid->wep_key_len[i]) {
				wpa_set_wep_key(wpa_s,
						i == ssid->wep_tx_keyidx,
						i, ssid->wep_key[i],
						ssid->wep_key_len[i]);
			}
		}
	}

	wpa_s->current_ssid = ssid;
	wpa_supplicant_initiate_eapol(wpa_s);
}


void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,
				 int reason_code)
{
	u8 *addr = NULL;
	wpa_s->wpa_state = WPA_DISCONNECTED;
	if (memcmp(wpa_s->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) != 0) {
		wpa_drv_disassociate(wpa_s, wpa_s->bssid, reason_code);
		addr = wpa_s->bssid;
	}
	wpa_clear_keys(wpa_s, addr);
	wpa_s->current_ssid = NULL;
	eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
}


void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code)
{
	u8 *addr = NULL;
	wpa_s->wpa_state = WPA_DISCONNECTED;
	if (memcmp(wpa_s->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) != 0) {
		wpa_drv_deauthenticate(wpa_s, wpa_s->bssid, reason_code);
		addr = wpa_s->bssid;
	}
	wpa_clear_keys(wpa_s, addr);
	wpa_s->current_ssid = NULL;
	eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
}


static void wpa_supplicant_imsi_identity(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid)
{
	int aka = 0;
	u8 *pos = ssid->eap_methods;

	while (pos && *pos != EAP_TYPE_NONE) {
		if (*pos == EAP_TYPE_AKA) {
			aka = 1;
			break;
		}
		pos++;
	}

	if (ssid->identity == NULL && wpa_s->imsi) {
		ssid->identity = malloc(1 + wpa_s->imsi_len);
		if (ssid->identity) {
			ssid->identity[0] = aka ? '0' : '1';
			memcpy(ssid->identity + 1, wpa_s->imsi,
			       wpa_s->imsi_len);
			ssid->identity_len = 1 + wpa_s->imsi_len;
			wpa_hexdump_ascii(MSG_DEBUG, "permanent identity from "
					  "IMSI", ssid->identity,
					  ssid->identity_len);
		}
	}
}


static void wpa_supplicant_scard_init(struct wpa_supplicant *wpa_s,
				      struct wpa_ssid *ssid)
{
	char buf[100];
	size_t len;

	if (ssid->pcsc == NULL)
		return;
	if (wpa_s->scard != NULL) {
		wpa_supplicant_imsi_identity(wpa_s, ssid);
		return;
	}
	wpa_printf(MSG_DEBUG, "Selected network is configured to use SIM - "
		   "initialize PCSC");
	wpa_s->scard = scard_init(SCARD_TRY_BOTH, ssid->pin);
	if (wpa_s->scard == NULL) {
		wpa_printf(MSG_WARNING, "Failed to initialize SIM "
			   "(pcsc-lite)");
		/* TODO: what to do here? */
		return;
	}
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);

	len = sizeof(buf);
	if (scard_get_imsi(wpa_s->scard, buf, &len)) {
		wpa_printf(MSG_WARNING, "Failed to get IMSI from SIM");
		/* TODO: what to do here? */
		return;
	}

	wpa_hexdump_ascii(MSG_DEBUG, "IMSI", (u8 *) buf, len);
	free(wpa_s->imsi);
	wpa_s->imsi = malloc(len);
	if (wpa_s->imsi) {
		memcpy(wpa_s->imsi, buf, len);
		wpa_s->imsi_len = len;
		wpa_supplicant_imsi_identity(wpa_s, ssid);
	}
}


static struct wpa_scan_result *
wpa_supplicant_select_bss(struct wpa_supplicant *wpa_s, struct wpa_ssid *group,
			  struct wpa_scan_result *results, int num,
			  struct wpa_ssid **selected_ssid)
{
	struct wpa_ssid *ssid;
	struct wpa_scan_result *bss, *selected = NULL;
	int i;
	struct wpa_blacklist *e;

	wpa_printf(MSG_DEBUG, "Selecting BSS from priority group %d",
		   group->priority);

	bss = NULL;
	ssid = NULL;
	/* First, try to find WPA-enabled AP */
	for (i = 0; i < num && !selected; i++) {
		bss = &results[i];
		wpa_printf(MSG_DEBUG, "%d: " MACSTR " ssid='%s' "
			   "wpa_ie_len=%lu rsn_ie_len=%lu",
			   i, MAC2STR(bss->bssid),
			   wpa_ssid_txt(bss->ssid, bss->ssid_len),
			   (unsigned long) bss->wpa_ie_len,
			   (unsigned long) bss->rsn_ie_len);
		if ((e = wpa_blacklist_get(wpa_s, bss->bssid)) &&
		    e->count > 1) {
			wpa_printf(MSG_DEBUG, "   skip - blacklisted");
			continue;
		}

		if (bss->wpa_ie_len == 0 && bss->rsn_ie_len == 0) {
			wpa_printf(MSG_DEBUG, "   skip - no WPA/RSN IE");
			continue;
		}

		for (ssid = group; ssid; ssid = ssid->pnext) {
			struct wpa_ie_data ie;
			if (bss->ssid_len != ssid->ssid_len ||
			    memcmp(bss->ssid, ssid->ssid,
				   bss->ssid_len) != 0) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "SSID mismatch");
				continue;
			}
			if (ssid->bssid_set &&
			    memcmp(bss->bssid, ssid->bssid, ETH_ALEN) != 0) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "BSSID mismatch");
				continue;
			}
			if (!(((ssid->proto & WPA_PROTO_RSN) &&
			       wpa_parse_wpa_ie(wpa_s, bss->rsn_ie,
						bss->rsn_ie_len, &ie) == 0) ||
			      ((ssid->proto & WPA_PROTO_WPA) &&
			       wpa_parse_wpa_ie(wpa_s, bss->wpa_ie,
						bss->wpa_ie_len, &ie) == 0))) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "could not parse WPA/RSN IE");
				continue;
			}
			if (!(ie.proto & ssid->proto)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "proto mismatch");
				continue;
			}
			if (!(ie.pairwise_cipher & ssid->pairwise_cipher)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "PTK cipher mismatch");
				continue;
			}
			if (!(ie.group_cipher & ssid->group_cipher)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "GTK cipher mismatch");
				continue;
			}
			if (!(ie.key_mgmt & ssid->key_mgmt)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "key mgmt mismatch");
				continue;
			}

			selected = bss;
			*selected_ssid = ssid;
			wpa_printf(MSG_DEBUG, "   selected");
			break;
		}
	}

	/* If no WPA-enabled AP found, try to find non-WPA AP, if configuration
	 * allows this. */
	for (i = 0; i < num && !selected; i++) {
		bss = &results[i];
		if ((e = wpa_blacklist_get(wpa_s, bss->bssid)) &&
		    e->count > 1) {
			continue;
		}
		for (ssid = group; ssid; ssid = ssid->pnext) {
			if (bss->ssid_len == ssid->ssid_len &&
			    memcmp(bss->ssid, ssid->ssid, bss->ssid_len) == 0
			    &&
			    (!ssid->bssid_set ||
			     memcmp(bss->bssid, ssid->bssid, ETH_ALEN) == 0) &&
			    ((ssid->key_mgmt & WPA_KEY_MGMT_NONE) ||
			     (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA)))
			{
				selected = bss;
				*selected_ssid = ssid;
				wpa_printf(MSG_DEBUG, "   selected non-WPA AP "
					   MACSTR " ssid='%s'",
					   MAC2STR(bss->bssid),
					   wpa_ssid_txt(bss->ssid,
							bss->ssid_len));
				break;
			}
		}
	}

	return selected;
}


static int wpa_supplicant_get_scan_results(struct wpa_supplicant *wpa_s)
{
#define SCAN_AP_LIMIT 128
	struct wpa_scan_result *results, *tmp;
	int num;

	results = malloc(SCAN_AP_LIMIT * sizeof(struct wpa_scan_result));
	if (results == NULL) {
		wpa_printf(MSG_WARNING, "Failed to allocate memory for scan "
			   "results");
		return -1;
	}

	num = wpa_drv_get_scan_results(wpa_s, results, SCAN_AP_LIMIT);
	wpa_printf(MSG_DEBUG, "Scan results: %d", num);
	if (num < 0) {
		wpa_printf(MSG_DEBUG, "Failed to get scan results");
		free(results);
		return -1;
	}
	if (num > SCAN_AP_LIMIT) {
		wpa_printf(MSG_INFO, "Not enough room for all APs (%d < %d)",
			   num, SCAN_AP_LIMIT);
		num = SCAN_AP_LIMIT;
	}

	/* Free unneeded memory for unused scan result entries */
	tmp = realloc(results, num * sizeof(struct wpa_scan_result));
	if (tmp || num == 0) {
		results = tmp;
	}

	free(wpa_s->scan_results);
	wpa_s->scan_results = results;
	wpa_s->num_scan_results = num;

	return 0;
}


static void wpa_supplicant_scan_results(struct wpa_supplicant *wpa_s)
{
	int num, prio;
	struct wpa_scan_result *selected = NULL;
	struct wpa_ssid *ssid;
	struct wpa_scan_result *results;

	if (wpa_supplicant_get_scan_results(wpa_s) < 0) {
		wpa_printf(MSG_DEBUG, "Failed to get scan results - try "
			   "scanning again");
		wpa_supplicant_req_scan(wpa_s, 1, 0);
		return;
	}
	results = wpa_s->scan_results;
	num = wpa_s->num_scan_results;

	while (selected == NULL) {
		for (prio = 0; prio < wpa_s->conf->num_prio; prio++) {
			selected = wpa_supplicant_select_bss(
				wpa_s, wpa_s->conf->pssid[prio], results, num,
				&ssid);
			if (selected)
				break;
		}

		if (selected == NULL && wpa_s->blacklist) {
			wpa_printf(MSG_DEBUG, "No APs found - clear blacklist "
				   "and try again");
			wpa_blacklist_clear(wpa_s);
		} else if (selected == NULL) {
			break;
		}
	}

	if (selected) {
		if (wpa_s->reassociate ||
		    memcmp(selected->bssid, wpa_s->bssid, ETH_ALEN) != 0) {
			wpa_supplicant_scard_init(wpa_s, ssid);
			wpa_supplicant_associate(wpa_s, selected, ssid);
		} else {
			wpa_printf(MSG_DEBUG, "Already associated with the "
				   "selected AP.");
		}
		rsn_preauth_scan_results(wpa_s, results, num);
	} else {
		wpa_printf(MSG_DEBUG, "No suitable AP found.");
		wpa_supplicant_req_scan(wpa_s, 5, 0);
	}
}


static int wpa_get_beacon_ie(struct wpa_supplicant *wpa_s)
{
	int i, ret = 0;
	struct wpa_scan_result *results, *curr = NULL;

	results = wpa_s->scan_results;
	if (results == NULL) {
		return -1;
	}

	for (i = 0; i < wpa_s->num_scan_results; i++) {
		if (memcmp(results[i].bssid, wpa_s->bssid, ETH_ALEN) == 0) {
			curr = &results[i];
			break;
		}
	}

	if (curr) {
		free(wpa_s->ap_wpa_ie);
		wpa_s->ap_wpa_ie_len = curr->wpa_ie_len;
		if (curr->wpa_ie_len) {
			wpa_s->ap_wpa_ie = malloc(wpa_s->ap_wpa_ie_len);
			if (wpa_s->ap_wpa_ie) {
				memcpy(wpa_s->ap_wpa_ie, curr->wpa_ie,
				       curr->wpa_ie_len);
			} else {
				ret = -1;
			}
		} else {
			wpa_s->ap_wpa_ie = NULL;
		}

		free(wpa_s->ap_rsn_ie);
		wpa_s->ap_rsn_ie_len = curr->rsn_ie_len;
		if (curr->rsn_ie_len) {
			wpa_s->ap_rsn_ie = malloc(wpa_s->ap_rsn_ie_len);
			if (wpa_s->ap_rsn_ie) {
				memcpy(wpa_s->ap_rsn_ie, curr->rsn_ie,
				       curr->rsn_ie_len);
			} else {
				ret = -1;
			}
		} else {
			wpa_s->ap_rsn_ie = NULL;
		}
	} else {
		ret = -1;
	}

	return ret;
}


int wpa_supplicant_get_beacon_ie(struct wpa_supplicant *wpa_s)
{
	if (wpa_get_beacon_ie(wpa_s) == 0) {
		return 0;
	}

	/* No WPA/RSN IE found in the cached scan results. Try to get updated
	 * scan results from the driver. */
	if (wpa_supplicant_get_scan_results(wpa_s) < 0) {
		return -1;
	}

	return wpa_get_beacon_ie(wpa_s);
}


#ifdef CONFIG_XSUPPLICANT_IFACE
static void wpa_supplicant_dot1x_receive(int sock, void *eloop_ctx,
					 void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	u8 buf[128];
	int res;

	res = recv(sock, buf, sizeof(buf), 0);
	wpa_printf(MSG_DEBUG, "WPA: Receive from dot1x (Xsupplicant) socket "
		   "==> %d", res);
	if (res < 0) {
		perror("recv");
		return;
	}

	if (res != PMK_LEN) {
		wpa_printf(MSG_WARNING, "WPA: Invalid master key length (%d) "
			   "from dot1x", res);
		return;
	}

	wpa_hexdump(MSG_DEBUG, "WPA: Master key (dot1x)", buf, PMK_LEN);
	if (wpa_s->key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		memcpy(wpa_s->pmk, buf, PMK_LEN);
		wpa_s->ext_pmk_received = 1;
	} else {
		wpa_printf(MSG_INFO, "WPA: Not in IEEE 802.1X mode - dropping "
			   "dot1x PMK update (%d)", wpa_s->key_mgmt);
	}
}


static int wpa_supplicant_802_1x_init(struct wpa_supplicant *wpa_s)
{
	int s;
	struct sockaddr_un addr;

	s = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	addr.sun_path[0] = '\0';
	snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1,
		 "wpa_supplicant");
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

	wpa_s->dot1x_s = s;
	eloop_register_read_sock(s, wpa_supplicant_dot1x_receive, wpa_s,
				 NULL);
	return 0;
}
#endif /* CONFIG_XSUPPLICANT_IFACE */


static int wpa_supplicant_set_driver(struct wpa_supplicant *wpa_s,
				     const char *name)
{
	int i;

	if (wpa_s == NULL)
		return -1;

	if (wpa_supplicant_drivers[0] == NULL) {
		wpa_printf(MSG_ERROR, "No driver interfaces build into "
			   "wpa_supplicant.");
		return -1;
	}

	if (name == NULL) {
		/* default to first driver in the list */
		wpa_s->driver = wpa_supplicant_drivers[0];
		return 0;
	}

	for (i = 0; wpa_supplicant_drivers[i]; i++) {
		if (strcmp(name, wpa_supplicant_drivers[i]->name) == 0) {
			wpa_s->driver = wpa_supplicant_drivers[i];
			return 0;
		}
	}

	printf("Unsupported driver '%s'.\n", name);
	return -1;
}


static void wpa_supplicant_fd_workaround(void)
{
	int s, i;
	/* When started from pcmcia-cs scripts, wpa_supplicant might start with
	 * fd 0, 1, and 2 closed. This will cause some issues because many
	 * places in wpa_supplicant are still printing out to stdout. As a
	 * workaround, make sure that fd's 0, 1, and 2 are not used for other
	 * sockets. */
	for (i = 0; i < 3; i++) {
		s = open("/dev/null", O_RDWR);
		if (s > 2) {
			close(s);
			break;
		}
	}
}


static int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s,
				      int wait_for_interface)
{
	static int interface_count = 0;

	for (;;) {
		wpa_s->l2 = l2_packet_init(wpa_s->ifname,
					   wpa_drv_get_mac_addr(wpa_s),
					   ETH_P_EAPOL,
					   wpa_supplicant_rx_eapol, wpa_s);
		if (wpa_s->l2)
			break;
		else if (!wait_for_interface)
			return -1;
		printf("Waiting for interface..\n");
		sleep(5);
	}

	if (l2_packet_get_own_addr(wpa_s->l2, wpa_s->own_addr)) {
		fprintf(stderr, "Failed to get own L2 address\n");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Own MAC address: " MACSTR,
		   MAC2STR(wpa_s->own_addr));

	if (wpa_drv_set_wpa(wpa_s, 1) < 0) {
		fprintf(stderr, "Failed to enable WPA in the driver.\n");
		return -1;
	}

	wpa_clear_keys(wpa_s, NULL);

	/* Make sure that TKIP countermeasures are not left enabled (could
	 * happen if wpa_supplicant is killed during countermeasures. */
	wpa_drv_set_countermeasures(wpa_s, 0);

	wpa_drv_set_drop_unencrypted(wpa_s, 1);

	wpa_s->prev_scan_ssid = BROADCAST_SSID_SCAN;
	wpa_supplicant_req_scan(wpa_s, interface_count, 100000);
	interface_count++;

	return 0;
}


static void usage(void)
{
	int i;
	printf("%s\n\n%s\n"
	       "usage:\n"
	       "  wpa_supplicant [-BddehLqqvw] -i<ifname> -c<config file> "
	       "[-D<driver>] \\\n"
	       "      [-P<pid file>] "
	       "[-N -i<ifname> -c<conf> [-D<driver>] ...]\n"
	       "\n"
	       "drivers:\n",
	       wpa_supplicant_version, wpa_supplicant_license);

	for (i = 0; wpa_supplicant_drivers[i]; i++) {
		printf("  %s = %s\n",
		       wpa_supplicant_drivers[i]->name,
		       wpa_supplicant_drivers[i]->desc);
	}

	printf("options:\n"
	       "  -B = run daemon in the background\n"
	       "  -d = increase debugging verbosity (-dd even more)\n"
	       "  -K = include keys (passwords, etc.) in debug output\n"
	       "  -t = include timestamp in debug messages\n"
#ifdef CONFIG_XSUPPLICANT_IFACE
#ifdef IEEE8021X_EAPOL
	       "  -e = use external IEEE 802.1X Supplicant (e.g., "
	       "xsupplicant)\n"
	       "       (this disables the internal Supplicant)\n"
#endif /* IEEE8021X_EAPOL */
#endif /* CONFIG_XSUPPLICANT_IFACE */
	       "  -h = show this help text\n"
	       "  -L = show license (GPL and BSD)\n"
	       "  -q = decrease debugging verbosity (-qq even less)\n"
	       "  -v = show version\n"
	       "  -w = wait for interface to be added, if needed\n"
	       "  -N = start describing new interface\n");
}


static void license(void)
{
	printf("%s\n\n%s\n",
	       wpa_supplicant_version, wpa_supplicant_full_license);
}


static struct wpa_supplicant * wpa_supplicant_alloc(void)
{
	struct wpa_supplicant *wpa_s;

	wpa_s = malloc(sizeof(*wpa_s));
	if (wpa_s == NULL)
		return NULL;
	memset(wpa_s, 0, sizeof(*wpa_s));
	wpa_s->ctrl_sock = -1;
#ifdef CONFIG_XSUPPLICANT_IFACE
	wpa_s->dot1x_s = -1;
#endif /* CONFIG_XSUPPLICANT_IFACE */

	return wpa_s;
}


static int wpa_supplicant_init(struct wpa_supplicant *wpa_s,
			       const char *confname, const char *driver,
			       const char *ifname)
{
	wpa_printf(MSG_DEBUG, "Initializing interface '%s' conf '%s' driver "
		   "'%s'", ifname, confname, driver ? driver : "default");

	if (wpa_supplicant_set_driver(wpa_s, driver) < 0) {
		return -1;
	}

	if (confname) {
		wpa_s->confname = rel2abs_path(confname);
		if (wpa_s->confname == NULL) {
			wpa_printf(MSG_ERROR, "Failed to get absolute path "
				   "for configuration file '%s'.", confname);
			return -1;
		}
		wpa_printf(MSG_DEBUG, "Configuration file '%s' -> '%s'",
			   confname, wpa_s->confname);
		wpa_s->conf = wpa_config_read(wpa_s->confname);
		if (wpa_s->conf == NULL) {
			printf("Failed to read configuration file '%s'.\n",
			       wpa_s->confname);
			return -1;
		}
	}

	if (wpa_s->conf == NULL || wpa_s->conf->ssid == NULL) {
		usage();
		printf("\nNo networks (SSID) configured.\n");
		return -1;
	}

	if (ifname == NULL) {
		usage();
		printf("\nInterface name is required.\n");
		return -1;
	}
	if (strlen(ifname) >= sizeof(wpa_s->ifname)) {
		printf("Too long interface name '%s'.\n", ifname);
		return -1;
	}
	strncpy(wpa_s->ifname, ifname, sizeof(wpa_s->ifname));

	return 0;
}


static int wpa_supplicant_init2(struct wpa_supplicant *wpa_s,
				int disable_eapol, int wait_for_interface)
{
	const char *ifname;

	wpa_printf(MSG_DEBUG, "Initializing interface (2) '%s'",
		   wpa_s->ifname);

	if (!disable_eapol) {
		struct eapol_ctx *ctx;
		ctx = malloc(sizeof(*ctx));
		if (ctx == NULL) {
			printf("Failed to allocate EAPOL context.\n");
			return -1;
		}
		memset(ctx, 0, sizeof(*ctx));
		ctx->ctx = wpa_s;
		ctx->msg_ctx = wpa_s;
		ctx->preauth = 0;
		ctx->eapol_done_cb = wpa_supplicant_notify_eapol_done;
		ctx->eapol_send = wpa_eapol_send;
		ctx->set_wep_key = wpa_eapol_set_wep_key;
		wpa_s->eapol = eapol_sm_init(ctx);
		if (wpa_s->eapol == NULL) {
			free(ctx);
			printf("Failed to initialize EAPOL state machines.\n");
			return -1;
		}
	}

	/* RSNA Supplicant Key Management - INITIALIZE */
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);

	/* Initialize driver interface and register driver event handler before
	 * L2 receive handler so that association events are processed before
	 * EAPOL-Key packets if both become available for the same select()
	 * call. */
	wpa_s->drv_priv = wpa_drv_init(wpa_s, wpa_s->ifname);
	if (wpa_s->drv_priv == NULL) {
		fprintf(stderr, "Failed to initialize driver interface\n");
		return -1;
	}

	ifname = wpa_drv_get_ifname(wpa_s);
	if (ifname && strcmp(ifname, wpa_s->ifname) != 0) {
		wpa_printf(MSG_DEBUG, "Driver interface replaced interface "
			   "name with '%s'", ifname);
		strncpy(wpa_s->ifname, ifname, sizeof(wpa_s->ifname));
	}

	wpa_s->renew_snonce = 1;
	if (wpa_supplicant_driver_init(wpa_s, wait_for_interface) < 0) {
		return -1;
	}

	if (wpa_supplicant_ctrl_iface_init(wpa_s)) {
		printf("Failed to initialize control interface '%s'.\n"
		       "You may have another wpa_supplicant process already "
		       "running or the file was\n"
		       "left by an unclean termination of wpa_supplicant in "
		       "which case you will need\n"
		       "to manually remove this file before starting "
		       "wpa_supplicant again.\n",
		       wpa_s->conf->ctrl_interface);
		return -1;
	}

#ifdef CONFIG_XSUPPLICANT_IFACE
	if (disable_eapol)
		wpa_supplicant_802_1x_init(wpa_s);
#endif /* CONFIG_XSUPPLICANT_IFACE */

	return 0;
}


static void wpa_supplicant_deinit(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->drv_priv) {
		if (wpa_drv_set_wpa(wpa_s, 0) < 0) {
			fprintf(stderr, "Failed to disable WPA in the "
				"driver.\n");
		}

		wpa_drv_set_drop_unencrypted(wpa_s, 0);
		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_clear_keys(wpa_s, NULL);

		wpa_drv_deinit(wpa_s);
	}
	wpa_supplicant_cleanup(wpa_s);
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant *head, *wpa_s;
	int c;
	const char *confname, *driver, *ifname;
	char *pid_file = NULL;
	int daemonize = 0, wait_for_interface = 0, disable_eapol = 0, exitcode;

#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		printf("Could not find a usable WinSock.dll\n");
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	head = wpa_s = wpa_supplicant_alloc();
	if (wpa_s == NULL)
		return -1;
	wpa_s->head = head;

	wpa_supplicant_fd_workaround();
	eloop_init(head);

	ifname = confname = driver = NULL;

	for (;;) {
		c = getopt(argc, argv, "Bc:D:dehi:KLNP:qtvw");
		if (c < 0)
			break;
		switch (c) {
		case 'B':
			daemonize++;
			break;
		case 'c':
			confname = optarg;
			break;
		case 'D':
			driver = optarg;
			break;
		case 'd':
			wpa_debug_level--;
			break;
#ifdef CONFIG_XSUPPLICANT_IFACE
#ifdef IEEE8021X_EAPOL
		case 'e':
			disable_eapol++;
			break;
#endif /* IEEE8021X_EAPOL */
#endif /* CONFIG_XSUPPLICANT_IFACE */
		case 'h':
			usage();
			return -1;
		case 'i':
			ifname = optarg;
			break;
		case 'K':
			wpa_debug_show_keys++;
			break;
		case 'L':
			license();
			return -1;
		case 'P':
			pid_file = rel2abs_path(optarg);
			break;
		case 'q':
			wpa_debug_level++;
			break;
		case 't':
			wpa_debug_timestamp++;
			break;
		case 'v':
			printf("%s\n", wpa_supplicant_version);
			return -1;
		case 'w':
			wait_for_interface++;
			break;
		case 'N':
			if (wpa_supplicant_init(wpa_s, confname, driver,
						ifname))
				return -1;
			wpa_s->next = wpa_supplicant_alloc();
			wpa_s = wpa_s->next;
			if (wpa_s == NULL)
				return -1;
			wpa_s->head = head;
			ifname = confname = driver = NULL;
			break;
		default:
			usage();
			return -1;
		}
	}

	if (wpa_supplicant_init(wpa_s, confname, driver, ifname))
		return -1;

	exitcode = 0;

	if (wait_for_interface && daemonize) {
		wpa_printf(MSG_DEBUG, "Daemonize..");
		if (daemon(0, 0)) {
			perror("daemon");
			exitcode = -1;
			goto cleanup;
		}
	}

	for (wpa_s = head; wpa_s; wpa_s = wpa_s->next) {
		if (wpa_supplicant_init2(wpa_s, disable_eapol,
					 wait_for_interface)) {
			exitcode = -1;
			goto cleanup;
		}
	}

	if (!wait_for_interface && daemonize) {
		wpa_printf(MSG_DEBUG, "Daemonize..");
		if (daemon(0, 0)) {
			perror("daemon");
			exitcode = -1;
			goto cleanup;
		}
	}

	if (pid_file) {
		FILE *f = fopen(pid_file, "w");
		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}

	eloop_register_signal(SIGINT, wpa_supplicant_terminate, NULL);
	eloop_register_signal(SIGTERM, wpa_supplicant_terminate, NULL);
#ifndef CONFIG_NATIVE_WINDOWS
	eloop_register_signal(SIGHUP, wpa_supplicant_reconfig, NULL);
#endif /* CONFIG_NATIVE_WINDOWS */

	eloop_run();

	for (wpa_s = head; wpa_s; wpa_s = wpa_s->next) {
		wpa_supplicant_deauthenticate(wpa_s, REASON_DEAUTH_LEAVING);
	}

cleanup:
	wpa_s = head;
	while (wpa_s) {
		struct wpa_supplicant *prev;
		wpa_supplicant_deinit(wpa_s);
		prev = wpa_s;
		wpa_s = wpa_s->next;
		free(prev);
	}

	eloop_destroy();

	if (pid_file) {
		unlink(pid_file);
		free(pid_file);
	}

#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */

	return exitcode;
}
