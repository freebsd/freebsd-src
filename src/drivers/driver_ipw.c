/*
 * WPA Supplicant - driver interaction with Linux ipw2100/2200 drivers
 * Copyright (c) 2005 Zhu Yi <yi.zhu@intel.com>
 * Copyright (c) 2004 Lubomir Gelo <lgelo@cnc.sk>
 * Copyright (c) 2003-2004, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * Please note that ipw2100/2200 drivers change to use generic Linux wireless
 * extensions if the kernel includes support for WE-18 or newer (Linux 2.6.13
 * or newer). driver_wext.c should be used in those cases.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "driver_wext.h"

struct wpa_driver_ipw_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
};

/* following definitions must be kept in sync with ipw2100.c and ipw2200.c */

#define IPW_IOCTL_WPA_SUPPLICANT		SIOCIWFIRSTPRIV+30

#define IPW_CMD_SET_WPA_PARAM			1
#define	IPW_CMD_SET_WPA_IE			2
#define IPW_CMD_SET_ENCRYPTION			3
#define IPW_CMD_MLME				4

#define IPW_PARAM_WPA_ENABLED			1
#define IPW_PARAM_TKIP_COUNTERMEASURES		2
#define IPW_PARAM_DROP_UNENCRYPTED		3
#define IPW_PARAM_PRIVACY_INVOKED		4
#define IPW_PARAM_AUTH_ALGS			5
#define IPW_PARAM_IEEE_802_1X			6

#define IPW_MLME_STA_DEAUTH			1
#define IPW_MLME_STA_DISASSOC			2

#define IPW_CRYPT_ERR_UNKNOWN_ALG		2
#define IPW_CRYPT_ERR_UNKNOWN_ADDR		3
#define IPW_CRYPT_ERR_CRYPT_INIT_FAILED		4
#define IPW_CRYPT_ERR_KEY_SET_FAILED		5
#define IPW_CRYPT_ERR_TX_KEY_SET_FAILED		6
#define IPW_CRYPT_ERR_CARD_CONF_FAILED		7

#define	IPW_CRYPT_ALG_NAME_LEN			16

struct ipw_param {
	u32 cmd;
	u8 sta_addr[ETH_ALEN];
        union {
		struct {
			u8 name;
			u32 value;
		} wpa_param;
		struct {
			u32 len;
			u8 reserved[32];
			u8 data[0];
		} wpa_ie;
	        struct{
			u32 command;
    			u32 reason_code;
		} mlme;
		struct {
			u8 alg[IPW_CRYPT_ALG_NAME_LEN];
			u8 set_tx;
			u32 err;
			u8 idx;
			u8 seq[8];
			u16 key_len;
			u8 key[0];
		} crypt;

	} u;
};

/* end of ipw2100.c and ipw2200.c code */

static int wpa_driver_ipw_set_auth_alg(void *priv, int auth_alg);

static int ipw_ioctl(struct wpa_driver_ipw_data *drv,
		     struct ipw_param *param, int len, int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) param;
	iwr.u.data.length = len;

	if (ioctl(drv->sock, IPW_IOCTL_WPA_SUPPLICANT, &iwr) < 0) {
		int ret = errno;
		if (show_err) 
			perror("ioctl[IPW_IOCTL_WPA_SUPPLICANT]");
		return ret;
	}

	return 0;
}


static void ipw_show_set_key_error(struct ipw_param *param)
{
	switch (param->u.crypt.err) {
	case IPW_CRYPT_ERR_UNKNOWN_ALG:
		wpa_printf(MSG_INFO, "Unknown algorithm '%s'.",
			   param->u.crypt.alg);
		wpa_printf(MSG_INFO, "You may need to load kernel module to "
			   "register that algorithm.");
		wpa_printf(MSG_INFO, "E.g., 'modprobe ieee80211_crypt_wep' for"
			   " WEP.");
		break;
	case IPW_CRYPT_ERR_UNKNOWN_ADDR:
		wpa_printf(MSG_INFO, "Unknown address " MACSTR ".",
			   MAC2STR(param->sta_addr));
		break;
	case IPW_CRYPT_ERR_CRYPT_INIT_FAILED:
		wpa_printf(MSG_INFO, "Crypt algorithm initialization failed.");
		break;
	case IPW_CRYPT_ERR_KEY_SET_FAILED:
		wpa_printf(MSG_INFO, "Key setting failed.");
		break;
	case IPW_CRYPT_ERR_TX_KEY_SET_FAILED:
		wpa_printf(MSG_INFO, "TX key index setting failed.");
		break;
	case IPW_CRYPT_ERR_CARD_CONF_FAILED:
		wpa_printf(MSG_INFO, "Card configuration failed.");
		break;
	}
}


static int ipw_set_wpa_ie(struct wpa_driver_ipw_data *drv,
			  const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct ipw_param *param;
	int ret;
	size_t blen = sizeof(*param) + wpa_ie_len;

	param = os_zalloc(blen);
	if (param == NULL)
		return -1;

	param->cmd = IPW_CMD_SET_WPA_IE;
	param->u.wpa_ie.len = wpa_ie_len;
	os_memcpy(param->u.wpa_ie.data, wpa_ie, wpa_ie_len);
	
	ret = ipw_ioctl(drv, param, blen, 1);

	os_free(param);
	return ret;
}


static int ipw_set_wpa_param(struct wpa_driver_ipw_data *drv, u8 name,
			     u32 value)
{
	struct ipw_param param;

	os_memset(&param, 0, sizeof(param));
	param.cmd = IPW_CMD_SET_WPA_PARAM;
	param.u.wpa_param.name = name;
	param.u.wpa_param.value = value;

	return ipw_ioctl(drv, &param, sizeof(param), 1);
}


static int ipw_mlme(struct wpa_driver_ipw_data *drv, const u8 *addr,
		    int cmd, int reason)
{
	struct ipw_param param;

	os_memset(&param, 0, sizeof(param));
	os_memcpy(param.sta_addr, addr, ETH_ALEN);	
	param.cmd = IPW_CMD_MLME;
	param.u.mlme.command = cmd;
	param.u.mlme.reason_code = reason;

	return ipw_ioctl(drv, &param, sizeof(param), 1);
}


static int wpa_driver_ipw_set_wpa(void *priv, int enabled)
{
	struct wpa_driver_ipw_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);

	if (!enabled && ipw_set_wpa_ie(drv, NULL, 0) < 0)
		ret = -1;

	if (ipw_set_wpa_param(drv, IPW_PARAM_WPA_ENABLED, enabled) < 0)
		ret = -1;

	return ret;
}


static int wpa_driver_ipw_set_key(const char *ifname, void *priv,
				  enum wpa_alg alg, const u8 *addr,
				  int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len)
{
	struct wpa_driver_ipw_data *drv = priv;
	struct ipw_param *param;
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

	param = (struct ipw_param *) buf;
	param->cmd = IPW_CMD_SET_ENCRYPTION;
	os_memset(param->sta_addr, 0xff, ETH_ALEN);
	os_strlcpy((char *) param->u.crypt.alg, alg_name,
		   IPW_CRYPT_ALG_NAME_LEN);
	param->u.crypt.set_tx = set_tx ? 1 : 0;
	param->u.crypt.idx = key_idx;
	os_memcpy(param->u.crypt.seq, seq, seq_len);
	param->u.crypt.key_len = key_len;
	os_memcpy((u8 *) (param + 1), key, key_len);

	if (ipw_ioctl(drv, param, blen, 1)) {
		wpa_printf(MSG_WARNING, "Failed to set encryption.");
		ipw_show_set_key_error(param);
		ret = -1;
	}
	os_free(buf);

	return ret;
}


static int wpa_driver_ipw_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_ipw_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return ipw_set_wpa_param(drv, IPW_PARAM_TKIP_COUNTERMEASURES,
				     enabled);

}


static int wpa_driver_ipw_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_ipw_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return ipw_set_wpa_param(drv, IPW_PARAM_DROP_UNENCRYPTED,
				     enabled);
}


static int wpa_driver_ipw_deauthenticate(void *priv, const u8 *addr,
					 int reason_code)
{
	struct wpa_driver_ipw_data *drv = priv;
	return ipw_mlme(drv, addr, IPW_MLME_STA_DEAUTH, reason_code);
}


static int wpa_driver_ipw_disassociate(void *priv, const u8 *addr,
				       int reason_code)
{
	struct wpa_driver_ipw_data *drv = priv;
	return ipw_mlme(drv, addr, IPW_MLME_STA_DISASSOC, reason_code);
}


static int
wpa_driver_ipw_associate(void *priv, struct wpa_driver_associate_params *params)
{
	struct wpa_driver_ipw_data *drv = priv;
	int ret = 0;
	int unencrypted_eapol;

	if (wpa_driver_ipw_set_auth_alg(drv, params->auth_alg) < 0)
		ret = -1;
	if (wpa_driver_ipw_set_drop_unencrypted(drv, params->drop_unencrypted)
	    < 0)
		ret = -1;
	if (ipw_set_wpa_ie(drv, params->wpa_ie, params->wpa_ie_len) < 0)
		ret = -1;
	if (wpa_driver_wext_set_ssid(drv->wext, params->ssid,
				     params->ssid_len) < 0)
		ret = -1;
	if (wpa_driver_wext_set_bssid(drv->wext, params->bssid) < 0)
		ret = -1;

	if (params->key_mgmt_suite == KEY_MGMT_802_1X ||
	    params->key_mgmt_suite == KEY_MGMT_PSK)
		unencrypted_eapol = 0;
	else
		unencrypted_eapol = 1;
	
	if (ipw_set_wpa_param(drv, IPW_PARAM_IEEE_802_1X,
			      unencrypted_eapol) < 0) {
		wpa_printf(MSG_DEBUG, "ipw: Failed to configure "
			   "ieee_802_1x param");
	}

	return ret;
}


static int wpa_driver_ipw_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_ipw_data *drv = priv;
	int algs = 0;

	if (auth_alg & WPA_AUTH_ALG_OPEN)
		algs |= 1;
	if (auth_alg & WPA_AUTH_ALG_SHARED)
		algs |= 2;
	if (auth_alg & WPA_AUTH_ALG_LEAP)
		algs |= 4;
	if (algs == 0)
		algs = 1; /* at least one algorithm should be set */

	wpa_printf(MSG_DEBUG, "%s: auth_alg=0x%x", __FUNCTION__, algs);
	return ipw_set_wpa_param(drv, IPW_PARAM_AUTH_ALGS, algs);
}


static int wpa_driver_ipw_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_ipw_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_ipw_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_ipw_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static int wpa_driver_ipw_scan(void *priv,
			       struct wpa_driver_scan_params *params)
{
	struct wpa_driver_ipw_data *drv = priv;
	return wpa_driver_wext_scan(drv->wext, params);
}


static struct wpa_scan_results * wpa_driver_ipw_get_scan_results(void *priv)
{
	struct wpa_driver_ipw_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext);
}


static int wpa_driver_ipw_set_operstate(void *priv, int state)
{
	struct wpa_driver_ipw_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


static void * wpa_driver_ipw_init(void *ctx, const char *ifname)
{
	struct wpa_driver_ipw_data *drv;
	int ver;

	wpa_printf(MSG_DEBUG, "%s is called", __FUNCTION__);
	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->wext = wpa_driver_wext_init(ctx, ifname);
	if (drv->wext == NULL) {
		os_free(drv);
		return NULL;
	}

	ver = wpa_driver_wext_get_version(drv->wext);
	if (ver >= 18) {
		wpa_printf(MSG_WARNING, "Linux wireless extensions version %d "
			   "detected.", ver);
		wpa_printf(MSG_WARNING, "ipw2x00 driver uses driver_wext "
			   "(-Dwext) instead of driver_ipw.");
	}

	drv->ctx = ctx;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0) {
		wpa_driver_wext_deinit(drv->wext);
		os_free(drv);
		return NULL;
	}

	wpa_driver_ipw_set_wpa(drv, 1);

	return drv;
}


static void wpa_driver_ipw_deinit(void *priv)
{
	struct wpa_driver_ipw_data *drv = priv;
	wpa_driver_ipw_set_wpa(drv, 0);
	wpa_driver_wext_deinit(drv->wext);
	close(drv->sock);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_ipw_ops = {
	.name = "ipw",
	.desc = "Intel ipw2100/2200 driver (old; use wext with Linux 2.6.13 "
	"or newer)",
	.get_bssid = wpa_driver_ipw_get_bssid,
	.get_ssid = wpa_driver_ipw_get_ssid,
	.set_key = wpa_driver_ipw_set_key,
	.set_countermeasures = wpa_driver_ipw_set_countermeasures,
	.scan2 = wpa_driver_ipw_scan,
	.get_scan_results2 = wpa_driver_ipw_get_scan_results,
	.deauthenticate = wpa_driver_ipw_deauthenticate,
	.disassociate = wpa_driver_ipw_disassociate,
	.associate = wpa_driver_ipw_associate,
	.init = wpa_driver_ipw_init,
	.deinit = wpa_driver_ipw_deinit,
	.set_operstate = wpa_driver_ipw_set_operstate,
};
