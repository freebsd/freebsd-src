/*
 * WPA Supplicant - driver interaction with Linux Host AP driver
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
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
#include <sys/ioctl.h>

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "driver_hostap.h"


struct wpa_driver_hostap_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
	int current_mode; /* infra/adhoc */
};


static int hostapd_ioctl(struct wpa_driver_hostap_data *drv,
			 struct prism2_hostapd_param *param,
			 int len, int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) param;
	iwr.u.data.length = len;

	if (ioctl(drv->sock, PRISM2_IOCTL_HOSTAPD, &iwr) < 0) {
		int ret = errno;
		if (show_err) 
			perror("ioctl[PRISM2_IOCTL_HOSTAPD]");
		return ret;
	}

	return 0;
}


static int wpa_driver_hostap_set_wpa_ie(struct wpa_driver_hostap_data *drv,
					const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct prism2_hostapd_param *param;
	int res;
	size_t blen = PRISM2_HOSTAPD_GENERIC_ELEMENT_HDR_LEN + wpa_ie_len;
	if (blen < sizeof(*param))
		blen = sizeof(*param);

	param = os_zalloc(blen);
	if (param == NULL)
		return -1;

	param->cmd = PRISM2_HOSTAPD_SET_GENERIC_ELEMENT;
	param->u.generic_elem.len = wpa_ie_len;
	os_memcpy(param->u.generic_elem.data, wpa_ie, wpa_ie_len);
	res = hostapd_ioctl(drv, param, blen, 1);

	os_free(param);

	return res;
}


static int prism2param(struct wpa_driver_hostap_data *drv, int param,
		       int value)
{
	struct iwreq iwr;
	int *i, ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	i = (int *) iwr.u.name;
	*i++ = param;
	*i++ = value;

	if (ioctl(drv->sock, PRISM2_IOCTL_PRISM2_PARAM, &iwr) < 0) {
		perror("ioctl[PRISM2_IOCTL_PRISM2_PARAM]");
		ret = -1;
	}
	return ret;
}


static int wpa_driver_hostap_set_wpa(void *priv, int enabled)
{
	struct wpa_driver_hostap_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);

	if (!enabled && wpa_driver_hostap_set_wpa_ie(drv, NULL, 0) < 0)
		ret = -1;
	if (prism2param(drv, PRISM2_PARAM_HOST_ROAMING, enabled ? 2 : 0) < 0)
		ret = -1;
	if (prism2param(drv, PRISM2_PARAM_WPA, enabled) < 0)
		ret = -1;

	return ret;
}


static void show_set_key_error(struct prism2_hostapd_param *param)
{
	switch (param->u.crypt.err) {
	case HOSTAP_CRYPT_ERR_UNKNOWN_ALG:
		wpa_printf(MSG_INFO, "Unknown algorithm '%s'.",
			   param->u.crypt.alg);
		wpa_printf(MSG_INFO, "You may need to load kernel module to "
			   "register that algorithm.");
		wpa_printf(MSG_INFO, "E.g., 'modprobe hostap_crypt_wep' for "
			   "WEP.");
		break;
	case HOSTAP_CRYPT_ERR_UNKNOWN_ADDR:
		wpa_printf(MSG_INFO, "Unknown address " MACSTR ".",
			   MAC2STR(param->sta_addr));
		break;
	case HOSTAP_CRYPT_ERR_CRYPT_INIT_FAILED:
		wpa_printf(MSG_INFO, "Crypt algorithm initialization failed.");
		break;
	case HOSTAP_CRYPT_ERR_KEY_SET_FAILED:
		wpa_printf(MSG_INFO, "Key setting failed.");
		break;
	case HOSTAP_CRYPT_ERR_TX_KEY_SET_FAILED:
		wpa_printf(MSG_INFO, "TX key index setting failed.");
		break;
	case HOSTAP_CRYPT_ERR_CARD_CONF_FAILED:
		wpa_printf(MSG_INFO, "Card configuration failed.");
		break;
	}
}


static int wpa_driver_hostap_set_key(void *priv, wpa_alg alg,
				     const u8 *addr, int key_idx,
				     int set_tx, const u8 *seq, size_t seq_len,
				     const u8 *key, size_t key_len)
{
	struct wpa_driver_hostap_data *drv = priv;
	struct prism2_hostapd_param *param;
	u8 *buf;
	size_t blen;
	int ret = 0;
	char *alg_name;

	switch (alg) {
	case WPA_ALG_NONE:
		alg_name = "none";
		break;
	case WPA_ALG_WEP:
		alg_name = "WEP";
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		break;
	default:
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: alg=%s key_idx=%d set_tx=%d seq_len=%lu "
		   "key_len=%lu", __FUNCTION__, alg_name, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	if (seq_len > 8)
		return -2;

	blen = sizeof(*param) + key_len;
	buf = os_zalloc(blen);
	if (buf == NULL)
		return -1;

	param = (struct prism2_hostapd_param *) buf;
	param->cmd = PRISM2_SET_ENCRYPTION;
	/* TODO: In theory, STA in client mode can use five keys; four default
	 * keys for receiving (with keyidx 0..3) and one individual key for
	 * both transmitting and receiving (keyidx 0) _unicast_ packets. Now,
	 * keyidx 0 is reserved for this unicast use and default keys can only
	 * use keyidx 1..3 (i.e., default key with keyidx 0 is not supported).
	 * This should be fine for more or less all cases, but for completeness
	 * sake, the driver could be enhanced to support the missing key. */
#if 0
	if (addr == NULL)
		os_memset(param->sta_addr, 0xff, ETH_ALEN);
	else
		os_memcpy(param->sta_addr, addr, ETH_ALEN);
#else
	os_memset(param->sta_addr, 0xff, ETH_ALEN);
#endif
	os_strlcpy((char *) param->u.crypt.alg, alg_name,
		   HOSTAP_CRYPT_ALG_NAME_LEN);
	param->u.crypt.flags = set_tx ? HOSTAP_CRYPT_FLAG_SET_TX_KEY : 0;
	param->u.crypt.idx = key_idx;
	os_memcpy(param->u.crypt.seq, seq, seq_len);
	param->u.crypt.key_len = key_len;
	os_memcpy((u8 *) (param + 1), key, key_len);

	if (hostapd_ioctl(drv, param, blen, 1)) {
		wpa_printf(MSG_WARNING, "Failed to set encryption.");
		show_set_key_error(param);
		ret = -1;
	}
	os_free(buf);

	return ret;
}


static int wpa_driver_hostap_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_hostap_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return prism2param(drv, PRISM2_PARAM_TKIP_COUNTERMEASURES, enabled);
}


static int wpa_driver_hostap_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_hostap_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return prism2param(drv, PRISM2_PARAM_DROP_UNENCRYPTED, enabled);
}


static int wpa_driver_hostap_reset(struct wpa_driver_hostap_data *drv,
				   int type)
{
	struct iwreq iwr;
	int *i, ret = 0;

	wpa_printf(MSG_DEBUG, "%s: type=%d", __FUNCTION__, type);

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	i = (int *) iwr.u.name;
	*i++ = type;

	if (ioctl(drv->sock, PRISM2_IOCTL_RESET, &iwr) < 0) {
	        perror("ioctl[PRISM2_IOCTL_RESET]");
	        ret = -1;
	}
	return ret;
}


static int wpa_driver_hostap_mlme(struct wpa_driver_hostap_data *drv,
				  const u8 *addr, int cmd, int reason_code)
{
	struct prism2_hostapd_param param;
	int ret;

	/* There does not seem to be a better way of deauthenticating or
	 * disassociating with Prism2/2.5/3 than sending the management frame
	 * and then resetting the Port0 to make sure both the AP and the STA
	 * end up in disconnected state. */
	os_memset(&param, 0, sizeof(param));
	param.cmd = PRISM2_HOSTAPD_MLME;
	os_memcpy(param.sta_addr, addr, ETH_ALEN);
	param.u.mlme.cmd = cmd;
	param.u.mlme.reason_code = reason_code;
	ret = hostapd_ioctl(drv, &param, sizeof(param), 1);
	if (ret == 0) {
		os_sleep(0, 100000);
		ret = wpa_driver_hostap_reset(drv, 2);
	}
	return ret;
}


static int wpa_driver_hostap_deauthenticate(void *priv, const u8 *addr,
					    int reason_code)
{
	struct wpa_driver_hostap_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_hostap_mlme(drv, addr, MLME_STA_DEAUTH,
				      reason_code);
}


static int wpa_driver_hostap_disassociate(void *priv, const u8 *addr,
					  int reason_code)
{
	struct wpa_driver_hostap_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_hostap_mlme(drv, addr, MLME_STA_DISASSOC,
				      reason_code);
}


static int
wpa_driver_hostap_associate(void *priv,
			    struct wpa_driver_associate_params *params)
{
	struct wpa_driver_hostap_data *drv = priv;
	int ret = 0;
	int allow_unencrypted_eapol;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	if (params->mode != drv->current_mode) {
		/* At the moment, Host AP driver requires host_roaming=2 for
		 * infrastructure mode and host_roaming=0 for adhoc. */
		if (prism2param(drv, PRISM2_PARAM_HOST_ROAMING,
				params->mode == IEEE80211_MODE_IBSS ? 0 : 2) <
		    0) {
			wpa_printf(MSG_DEBUG, "%s: failed to set host_roaming",
				   __func__);
		}
		drv->current_mode = params->mode;
	}

	if (prism2param(drv, PRISM2_PARAM_PRIVACY_INVOKED,
			params->key_mgmt_suite != KEY_MGMT_NONE) < 0)
		ret = -1;
	if (wpa_driver_hostap_set_wpa_ie(drv, params->wpa_ie,
					 params->wpa_ie_len) < 0)
		ret = -1;
	if (wpa_driver_wext_set_mode(drv->wext, params->mode) < 0)
		ret = -1;
	if (params->freq &&
	    wpa_driver_wext_set_freq(drv->wext, params->freq) < 0)
		ret = -1;
	if (wpa_driver_wext_set_ssid(drv->wext, params->ssid, params->ssid_len)
	    < 0)
		ret = -1;
	if (wpa_driver_wext_set_bssid(drv->wext, params->bssid) < 0)
		ret = -1;

	/* Allow unencrypted EAPOL messages even if pairwise keys are set when
	 * not using WPA. IEEE 802.1X specifies that these frames are not
	 * encrypted, but WPA encrypts them when pairwise keys are in use. */
	if (params->key_mgmt_suite == KEY_MGMT_802_1X ||
	    params->key_mgmt_suite == KEY_MGMT_PSK)
		allow_unencrypted_eapol = 0;
	else
		allow_unencrypted_eapol = 1;
	
	if (prism2param(drv, PRISM2_PARAM_IEEE_802_1X,
			allow_unencrypted_eapol) < 0) {
		wpa_printf(MSG_DEBUG, "hostap: Failed to configure "
			   "ieee_802_1x param");
		/* Ignore this error.. driver_hostap.c can also be used with
		 * other drivers that do not support this prism2_param. */
	}

	return ret;
}


static int wpa_driver_hostap_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_hostap_data *drv = priv;
	struct prism2_hostapd_param param;
	int ret;

	if (ssid == NULL) {
		/* Use standard Linux Wireless Extensions ioctl if possible
		 * because some drivers using hostap code in wpa_supplicant
		 * might not support Host AP specific scan request (with SSID
		 * info). */
		return wpa_driver_wext_scan(drv->wext, ssid, ssid_len);
	}

	if (ssid_len > 32)
		ssid_len = 32;

	os_memset(&param, 0, sizeof(param));
	param.cmd = PRISM2_HOSTAPD_SCAN_REQ;
	param.u.scan_req.ssid_len = ssid_len;
	os_memcpy(param.u.scan_req.ssid, ssid, ssid_len);
	ret = hostapd_ioctl(drv, &param, sizeof(param), 1);

	/* Not all drivers generate "scan completed" wireless event, so try to
	 * read results after a timeout. */
	eloop_cancel_timeout(wpa_driver_wext_scan_timeout, drv->wext,
			     drv->ctx);
	eloop_register_timeout(3, 0, wpa_driver_wext_scan_timeout, drv->wext,
			       drv->ctx);

	return ret;
}


static int wpa_driver_hostap_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_hostap_data *drv = priv;
	int algs = 0;

	if (auth_alg & AUTH_ALG_OPEN_SYSTEM)
		algs |= 1;
	if (auth_alg & AUTH_ALG_SHARED_KEY)
		algs |= 2;
	if (auth_alg & AUTH_ALG_LEAP)
		algs |= 4;
	if (algs == 0)
		algs = 1; /* at least one algorithm should be set */

	return prism2param(drv, PRISM2_PARAM_AP_AUTH_ALGS, algs);
}


static int wpa_driver_hostap_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_hostap_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_hostap_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_hostap_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static struct wpa_scan_results * wpa_driver_hostap_get_scan_results(void *priv)
{
	struct wpa_driver_hostap_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext);
}


static int wpa_driver_hostap_set_operstate(void *priv, int state)
{
	struct wpa_driver_hostap_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


static void * wpa_driver_hostap_init(void *ctx, const char *ifname)
{
	struct wpa_driver_hostap_data *drv;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->wext = wpa_driver_wext_init(ctx, ifname);
	if (drv->wext == NULL) {
		os_free(drv);
		return NULL;
	}

	drv->ctx = ctx;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0) {
		perror("socket");
		wpa_driver_wext_deinit(drv->wext);
		os_free(drv);
		return NULL;
	}

	if (os_strncmp(ifname, "wlan", 4) == 0) {
		/*
		 * Host AP driver may use both wlan# and wifi# interface in
		 * wireless events.
		 */
		char ifname2[IFNAMSIZ + 1];
		os_strlcpy(ifname2, ifname, sizeof(ifname2));
		os_memcpy(ifname2, "wifi", 4);
		wpa_driver_wext_alternative_ifindex(drv->wext, ifname2);
	}

	return drv;
}


static void wpa_driver_hostap_deinit(void *priv)
{
	struct wpa_driver_hostap_data *drv = priv;
	wpa_driver_wext_deinit(drv->wext);
	close(drv->sock);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_hostap_ops = {
	.name = "hostap",
	.desc = "Host AP driver (Intersil Prism2/2.5/3)",
	.get_bssid = wpa_driver_hostap_get_bssid,
	.get_ssid = wpa_driver_hostap_get_ssid,
	.set_wpa = wpa_driver_hostap_set_wpa,
	.set_key = wpa_driver_hostap_set_key,
	.set_countermeasures = wpa_driver_hostap_set_countermeasures,
	.set_drop_unencrypted = wpa_driver_hostap_set_drop_unencrypted,
	.scan = wpa_driver_hostap_scan,
	.get_scan_results2 = wpa_driver_hostap_get_scan_results,
	.deauthenticate = wpa_driver_hostap_deauthenticate,
	.disassociate = wpa_driver_hostap_disassociate,
	.associate = wpa_driver_hostap_associate,
	.set_auth_alg = wpa_driver_hostap_set_auth_alg,
	.init = wpa_driver_hostap_init,
	.deinit = wpa_driver_hostap_deinit,
	.set_operstate = wpa_driver_hostap_set_operstate,
};
