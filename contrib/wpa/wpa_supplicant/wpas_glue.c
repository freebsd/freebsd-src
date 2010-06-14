/*
 * WPA Supplicant - Glue code to setup EAPOL and RSN modules
 * Copyright (c) 2003-2008, Jouni Malinen <j@w1.fi>
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

#include "common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "wpa.h"
#include "eloop.h"
#include "config.h"
#include "l2_packet/l2_packet.h"
#include "wpa_common.h"
#include "wpa_supplicant_i.h"
#include "pmksa_cache.h"
#include "mlme.h"
#include "ieee802_11_defs.h"
#include "wpa_ctrl.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"


#ifndef CONFIG_NO_CONFIG_BLOBS
#if defined(IEEE8021X_EAPOL) || !defined(CONFIG_NO_WPA)
static void wpa_supplicant_set_config_blob(void *ctx,
					   struct wpa_config_blob *blob)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_config_set_blob(wpa_s->conf, blob);
	if (wpa_s->conf->update_config) {
		int ret = wpa_config_write(wpa_s->confname, wpa_s->conf);
		if (ret) {
			wpa_printf(MSG_DEBUG, "Failed to update config after "
				   "blob set");
		}
	}
}


static const struct wpa_config_blob *
wpa_supplicant_get_config_blob(void *ctx, const char *name)
{
	struct wpa_supplicant *wpa_s = ctx;
	return wpa_config_get_blob(wpa_s->conf, name);
}
#endif /* defined(IEEE8021X_EAPOL) || !defined(CONFIG_NO_WPA) */
#endif /* CONFIG_NO_CONFIG_BLOBS */


#if defined(IEEE8021X_EAPOL) || !defined(CONFIG_NO_WPA)
static u8 * wpa_alloc_eapol(const struct wpa_supplicant *wpa_s, u8 type,
			    const void *data, u16 data_len,
			    size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

	*msg_len = sizeof(*hdr) + data_len;
	hdr = os_malloc(*msg_len);
	if (hdr == NULL)
		return NULL;

	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = type;
	hdr->length = host_to_be16(data_len);

	if (data)
		os_memcpy(hdr + 1, data, data_len);
	else
		os_memset(hdr + 1, 0, data_len);

	if (data_pos)
		*data_pos = hdr + 1;

	return (u8 *) hdr;
}


/**
 * wpa_ether_send - Send Ethernet frame
 * @wpa_s: Pointer to wpa_supplicant data
 * @dest: Destination MAC address
 * @proto: Ethertype in host byte order
 * @buf: Frame payload starting from IEEE 802.1X header
 * @len: Frame payload length
 * Returns: >=0 on success, <0 on failure
 */
static int wpa_ether_send(struct wpa_supplicant *wpa_s, const u8 *dest,
			  u16 proto, const u8 *buf, size_t len)
{
	if (wpa_s->l2) {
		return l2_packet_send(wpa_s->l2, dest, proto, buf, len);
	}

	return wpa_drv_send_eapol(wpa_s, dest, proto, buf, len);
}
#endif /* IEEE8021X_EAPOL || !CONFIG_NO_WPA */


#ifdef IEEE8021X_EAPOL

/**
 * wpa_supplicant_eapol_send - Send IEEE 802.1X EAPOL packet to Authenticator
 * @ctx: Pointer to wpa_supplicant data (wpa_s)
 * @type: IEEE 802.1X packet type (IEEE802_1X_TYPE_*)
 * @buf: EAPOL payload (after IEEE 802.1X header)
 * @len: EAPOL payload length
 * Returns: >=0 on success, <0 on failure
 *
 * This function adds Ethernet and IEEE 802.1X header and sends the EAPOL frame
 * to the current Authenticator.
 */
static int wpa_supplicant_eapol_send(void *ctx, int type, const u8 *buf,
				     size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	u8 *msg, *dst, bssid[ETH_ALEN];
	size_t msglen;
	int res;

	/* TODO: could add l2_packet_sendmsg that allows fragments to avoid
	 * extra copy here */

	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_NONE) {
		/* Current SSID is not using IEEE 802.1X/EAP, so drop possible
		 * EAPOL frames (mainly, EAPOL-Start) from EAPOL state
		 * machines. */
		wpa_printf(MSG_DEBUG, "WPA: drop TX EAPOL in non-IEEE 802.1X "
			   "mode (type=%d len=%lu)", type,
			   (unsigned long) len);
		return -1;
	}

	if (pmksa_cache_get_current(wpa_s->wpa) &&
	    type == IEEE802_1X_TYPE_EAPOL_START) {
		/* Trying to use PMKSA caching - do not send EAPOL-Start frames
		 * since they will trigger full EAPOL authentication. */
		wpa_printf(MSG_DEBUG, "RSN: PMKSA caching - do not send "
			   "EAPOL-Start");
		return -1;
	}

	if (is_zero_ether_addr(wpa_s->bssid)) {
		wpa_printf(MSG_DEBUG, "BSSID not set when trying to send an "
			   "EAPOL frame");
		if (wpa_drv_get_bssid(wpa_s, bssid) == 0 &&
		    !is_zero_ether_addr(bssid)) {
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

	msg = wpa_alloc_eapol(wpa_s, type, buf, len, &msglen, NULL);
	if (msg == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "TX EAPOL: dst=" MACSTR, MAC2STR(dst));
	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", msg, msglen);
	res = wpa_ether_send(wpa_s, dst, ETH_P_EAPOL, msg, msglen);
	os_free(msg);
	return res;
}


/**
 * wpa_eapol_set_wep_key - set WEP key for the driver
 * @ctx: Pointer to wpa_supplicant data (wpa_s)
 * @unicast: 1 = individual unicast key, 0 = broadcast key
 * @keyidx: WEP key index (0..3)
 * @key: Pointer to key data
 * @keylen: Key length in bytes
 * Returns: 0 on success or < 0 on error.
 */
static int wpa_eapol_set_wep_key(void *ctx, int unicast, int keyidx,
				 const u8 *key, size_t keylen)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		int cipher = (keylen == 5) ? WPA_CIPHER_WEP40 :
			WPA_CIPHER_WEP104;
		if (unicast)
			wpa_s->pairwise_cipher = cipher;
		else
			wpa_s->group_cipher = cipher;
	}
	return wpa_drv_set_key(wpa_s, WPA_ALG_WEP,
			       unicast ? wpa_s->bssid :
			       (u8 *) "\xff\xff\xff\xff\xff\xff",
			       keyidx, unicast, (u8 *) "", 0, key, keylen);
}


static void wpa_supplicant_aborted_cached(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_sm_aborted_cached(wpa_s->wpa);
}


static void wpa_supplicant_eapol_cb(struct eapol_sm *eapol, int success,
				    void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	int res, pmk_len;
	u8 pmk[PMK_LEN];

	wpa_printf(MSG_DEBUG, "EAPOL authentication completed %ssuccessfully",
		   success ? "" : "un");

	if (wpas_wps_eapol_cb(wpa_s) > 0)
		return;

	if (!success) {
		/*
		 * Make sure we do not get stuck here waiting for long EAPOL
		 * timeout if the AP does not disconnect in case of
		 * authentication failure.
		 */
		wpa_supplicant_req_auth_timeout(wpa_s, 2, 0);
	}

	if (!success || !wpa_s->driver_4way_handshake)
		return;

	if (!wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt))
		return;

	wpa_printf(MSG_DEBUG, "Configure PMK for driver-based RSN 4-way "
		   "handshake");

	pmk_len = PMK_LEN;
	res = eapol_sm_get_key(eapol, pmk, PMK_LEN);
	if (res) {
		/*
		 * EAP-LEAP is an exception from other EAP methods: it
		 * uses only 16-byte PMK.
		 */
		res = eapol_sm_get_key(eapol, pmk, 16);
		pmk_len = 16;
	}

	if (res) {
		wpa_printf(MSG_DEBUG, "Failed to get PMK from EAPOL state "
			   "machines");
		return;
	}

	if (wpa_drv_set_key(wpa_s, WPA_ALG_PMK, NULL, 0, 0, NULL, 0, pmk,
			    pmk_len)) {
		wpa_printf(MSG_DEBUG, "Failed to set PMK to the driver");
	}

	wpa_supplicant_cancel_scan(wpa_s);
	wpa_supplicant_cancel_auth_timeout(wpa_s);
	wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);

}


static void wpa_supplicant_notify_eapol_done(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_msg(wpa_s, MSG_DEBUG, "WPA: EAPOL processing complete");
	if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {
		wpa_supplicant_set_state(wpa_s, WPA_4WAY_HANDSHAKE);
	} else {
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
	}
}

#endif /* IEEE8021X_EAPOL */


#ifndef CONFIG_NO_WPA

static int wpa_get_beacon_ie(struct wpa_supplicant *wpa_s)
{
	size_t i;
	int ret = 0;
	struct wpa_scan_res *curr = NULL;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	const u8 *ie;

	if (wpa_s->scan_res == NULL)
		return -1;

	for (i = 0; i < wpa_s->scan_res->num; i++) {
		struct wpa_scan_res *r = wpa_s->scan_res->res[i];
		if (os_memcmp(r->bssid, wpa_s->bssid, ETH_ALEN) != 0)
			continue;
		ie = wpa_scan_get_ie(r, WLAN_EID_SSID);
		if (ssid == NULL ||
		    ((ie && ie[1] == ssid->ssid_len &&
		      os_memcmp(ie + 2, ssid->ssid, ssid->ssid_len) == 0) ||
		     ssid->ssid_len == 0)) {
			curr = r;
			break;
		}
	}

	if (curr) {
		ie = wpa_scan_get_vendor_ie(curr, WPA_IE_VENDOR_TYPE);
		if (wpa_sm_set_ap_wpa_ie(wpa_s->wpa, ie, ie ? 2 + ie[1] : 0))
			ret = -1;

		ie = wpa_scan_get_ie(curr, WLAN_EID_RSN);
		if (wpa_sm_set_ap_rsn_ie(wpa_s->wpa, ie, ie ? 2 + ie[1] : 0))
			ret = -1;
	} else {
		ret = -1;
	}

	return ret;
}


static int wpa_supplicant_get_beacon_ie(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
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


static u8 * _wpa_alloc_eapol(void *wpa_s, u8 type,
			     const void *data, u16 data_len,
			     size_t *msg_len, void **data_pos)
{
	return wpa_alloc_eapol(wpa_s, type, data, data_len, msg_len, data_pos);
}


static int _wpa_ether_send(void *wpa_s, const u8 *dest, u16 proto,
			   const u8 *buf, size_t len)
{
	return wpa_ether_send(wpa_s, dest, proto, buf, len);
}


static void _wpa_supplicant_cancel_auth_timeout(void *wpa_s)
{
	wpa_supplicant_cancel_auth_timeout(wpa_s);
}


static void _wpa_supplicant_set_state(void *wpa_s, wpa_states state)
{
	wpa_supplicant_set_state(wpa_s, state);
}


/**
 * wpa_supplicant_get_state - Get the connection state
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: The current connection state (WPA_*)
 */
static wpa_states wpa_supplicant_get_state(struct wpa_supplicant *wpa_s)
{
	return wpa_s->wpa_state;
}


static wpa_states _wpa_supplicant_get_state(void *wpa_s)
{
	return wpa_supplicant_get_state(wpa_s);
}


static void _wpa_supplicant_disassociate(void *wpa_s, int reason_code)
{
	wpa_supplicant_disassociate(wpa_s, reason_code);
	/* Schedule a scan to make sure we continue looking for networks */
	wpa_supplicant_req_scan(wpa_s, 5, 0);
}


static void _wpa_supplicant_deauthenticate(void *wpa_s, int reason_code)
{
	wpa_supplicant_deauthenticate(wpa_s, reason_code);
	/* Schedule a scan to make sure we continue looking for networks */
	wpa_supplicant_req_scan(wpa_s, 5, 0);
}


static void * wpa_supplicant_get_network_ctx(void *wpa_s)
{
	return wpa_supplicant_get_ssid(wpa_s);
}


static int wpa_supplicant_get_bssid(void *ctx, u8 *bssid)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->use_client_mlme) {
		os_memcpy(bssid, wpa_s->bssid, ETH_ALEN);
		return 0;
	}
	return wpa_drv_get_bssid(wpa_s, bssid);
}


static int wpa_supplicant_set_key(void *_wpa_s, wpa_alg alg,
				  const u8 *addr, int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len)
{
	struct wpa_supplicant *wpa_s = _wpa_s;
	if (alg == WPA_ALG_TKIP && key_idx == 0 && key_len == 32) {
		/* Clear the MIC error counter when setting a new PTK. */
		wpa_s->mic_errors_seen = 0;
	}
	return wpa_drv_set_key(wpa_s, alg, addr, key_idx, set_tx, seq, seq_len,
			       key, key_len);
}


static int wpa_supplicant_mlme_setprotection(void *wpa_s, const u8 *addr,
					     int protection_type,
					     int key_type)
{
	return wpa_drv_mlme_setprotection(wpa_s, addr, protection_type,
					  key_type);
}


static int wpa_supplicant_add_pmkid(void *wpa_s,
				    const u8 *bssid, const u8 *pmkid)
{
	return wpa_drv_add_pmkid(wpa_s, bssid, pmkid);
}


static int wpa_supplicant_remove_pmkid(void *wpa_s,
				       const u8 *bssid, const u8 *pmkid)
{
	return wpa_drv_remove_pmkid(wpa_s, bssid, pmkid);
}


#ifdef CONFIG_IEEE80211R
static int wpa_supplicant_update_ft_ies(void *ctx, const u8 *md,
					const u8 *ies, size_t ies_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->use_client_mlme)
		return ieee80211_sta_update_ft_ies(wpa_s, md, ies, ies_len);
	return wpa_drv_update_ft_ies(wpa_s, md, ies, ies_len);
}


static int wpa_supplicant_send_ft_action(void *ctx, u8 action,
					 const u8 *target_ap,
					 const u8 *ies, size_t ies_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s->use_client_mlme)
		return ieee80211_sta_send_ft_action(wpa_s, action, target_ap,
						    ies, ies_len);
	return wpa_drv_send_ft_action(wpa_s, action, target_ap, ies, ies_len);
}
#endif /* CONFIG_IEEE80211R */

#endif /* CONFIG_NO_WPA */


#if defined(CONFIG_CTRL_IFACE) || !defined(CONFIG_NO_STDOUT_DEBUG)
static void wpa_supplicant_eap_param_needed(void *ctx, const char *field,
					    const char *txt)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	char *buf;
	size_t buflen;
	int len;

	if (ssid == NULL)
		return;

	buflen = 100 + os_strlen(txt) + ssid->ssid_len;
	buf = os_malloc(buflen);
	if (buf == NULL)
		return;
	len = os_snprintf(buf, buflen,
			  WPA_CTRL_REQ "%s-%d:%s needed for SSID ",
			  field, ssid->id, txt);
	if (len < 0 || (size_t) len >= buflen) {
		os_free(buf);
		return;
	}
	if (ssid->ssid && buflen > len + ssid->ssid_len) {
		os_memcpy(buf + len, ssid->ssid, ssid->ssid_len);
		len += ssid->ssid_len;
		buf[len] = '\0';
	}
	buf[buflen - 1] = '\0';
	wpa_msg(wpa_s, MSG_INFO, "%s", buf);
	os_free(buf);
}
#else /* CONFIG_CTRL_IFACE || !CONFIG_NO_STDOUT_DEBUG */
#define wpa_supplicant_eap_param_needed NULL
#endif /* CONFIG_CTRL_IFACE || !CONFIG_NO_STDOUT_DEBUG */


int wpa_supplicant_init_eapol(struct wpa_supplicant *wpa_s)
{
#ifdef IEEE8021X_EAPOL
	struct eapol_ctx *ctx;
	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate EAPOL context.");
		return -1;
	}

	ctx->ctx = wpa_s;
	ctx->msg_ctx = wpa_s;
	ctx->eapol_send_ctx = wpa_s;
	ctx->preauth = 0;
	ctx->eapol_done_cb = wpa_supplicant_notify_eapol_done;
	ctx->eapol_send = wpa_supplicant_eapol_send;
	ctx->set_wep_key = wpa_eapol_set_wep_key;
	ctx->set_config_blob = wpa_supplicant_set_config_blob;
	ctx->get_config_blob = wpa_supplicant_get_config_blob;
	ctx->aborted_cached = wpa_supplicant_aborted_cached;
#ifdef EAP_TLS_OPENSSL
	ctx->opensc_engine_path = wpa_s->conf->opensc_engine_path;
	ctx->pkcs11_engine_path = wpa_s->conf->pkcs11_engine_path;
	ctx->pkcs11_module_path = wpa_s->conf->pkcs11_module_path;
#endif /* EAP_TLS_OPENSSL */
	ctx->wps = wpa_s->wps;
	ctx->eap_param_needed = wpa_supplicant_eap_param_needed;
	ctx->cb = wpa_supplicant_eapol_cb;
	ctx->cb_ctx = wpa_s;
	wpa_s->eapol = eapol_sm_init(ctx);
	if (wpa_s->eapol == NULL) {
		os_free(ctx);
		wpa_printf(MSG_ERROR, "Failed to initialize EAPOL state "
			   "machines.");
		return -1;
	}
#endif /* IEEE8021X_EAPOL */

	return 0;
}


int wpa_supplicant_init_wpa(struct wpa_supplicant *wpa_s)
{
#ifndef CONFIG_NO_WPA
	struct wpa_sm_ctx *ctx;
	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate WPA context.");
		return -1;
	}

	ctx->ctx = wpa_s;
	ctx->set_state = _wpa_supplicant_set_state;
	ctx->get_state = _wpa_supplicant_get_state;
	ctx->deauthenticate = _wpa_supplicant_deauthenticate;
	ctx->disassociate = _wpa_supplicant_disassociate;
	ctx->set_key = wpa_supplicant_set_key;
	ctx->get_network_ctx = wpa_supplicant_get_network_ctx;
	ctx->get_bssid = wpa_supplicant_get_bssid;
	ctx->ether_send = _wpa_ether_send;
	ctx->get_beacon_ie = wpa_supplicant_get_beacon_ie;
	ctx->alloc_eapol = _wpa_alloc_eapol;
	ctx->cancel_auth_timeout = _wpa_supplicant_cancel_auth_timeout;
	ctx->add_pmkid = wpa_supplicant_add_pmkid;
	ctx->remove_pmkid = wpa_supplicant_remove_pmkid;
#ifndef CONFIG_NO_CONFIG_BLOBS
	ctx->set_config_blob = wpa_supplicant_set_config_blob;
	ctx->get_config_blob = wpa_supplicant_get_config_blob;
#endif /* CONFIG_NO_CONFIG_BLOBS */
	ctx->mlme_setprotection = wpa_supplicant_mlme_setprotection;
#ifdef CONFIG_IEEE80211R
	ctx->update_ft_ies = wpa_supplicant_update_ft_ies;
	ctx->send_ft_action = wpa_supplicant_send_ft_action;
#endif /* CONFIG_IEEE80211R */

	wpa_s->wpa = wpa_sm_init(ctx);
	if (wpa_s->wpa == NULL) {
		wpa_printf(MSG_ERROR, "Failed to initialize WPA state "
			   "machine");
		return -1;
	}
#endif /* CONFIG_NO_WPA */

	return 0;
}


void wpa_supplicant_rsn_supp_set_config(struct wpa_supplicant *wpa_s,
					struct wpa_ssid *ssid)
{
	struct rsn_supp_config conf;
	if (ssid) {
		os_memset(&conf, 0, sizeof(conf));
		conf.network_ctx = ssid;
		conf.peerkey_enabled = ssid->peerkey;
		conf.allowed_pairwise_cipher = ssid->pairwise_cipher;
#ifdef IEEE8021X_EAPOL
		conf.eap_workaround = ssid->eap_workaround;
		conf.eap_conf_ctx = &ssid->eap;
#endif /* IEEE8021X_EAPOL */
		conf.ssid = ssid->ssid;
		conf.ssid_len = ssid->ssid_len;
		conf.wpa_ptk_rekey = ssid->wpa_ptk_rekey;
	}
	wpa_sm_set_config(wpa_s->wpa, ssid ? &conf : NULL);
}
