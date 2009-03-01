/*
 * WPA Supplicant - PS3 Linux wireless extension driver interface
 * Copyright 2007, 2008 Sony Corporation
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
#include "wpa_common.h"
#include "driver.h"
#include "eloop.h"
#include "driver_wext.h"
#include "ieee802_11_defs.h"

static int wpa_driver_ps3_set_wpa_key(struct wpa_driver_wext_data *drv,
				struct wpa_driver_associate_params *params)
{
	int ret, i;
	struct iwreq iwr;
	char *buf, *str;

	if (!params->psk && !params->passphrase) {
		wpa_printf(MSG_INFO, "%s:no PSK error", __func__);
		return -EINVAL;
	}

	os_memset(&iwr, 0, sizeof(iwr));
	if (params->psk) {
		/* includes null */
		iwr.u.data.length = PMK_LEN * 2 + 1;
		buf = os_malloc(iwr.u.data.length);
		if (!buf)
			return -ENOMEM;
		str = buf;
		for (i = 0; i < PMK_LEN; i++) {
			str += snprintf(str, iwr.u.data.length - (str - buf),
					"%02x", params->psk[i]);
		}
	} else if (params->passphrase) {
		/* including quotations and null */
		iwr.u.data.length = strlen(params->passphrase) + 3;
		buf = os_malloc(iwr.u.data.length);
		if (!buf)
			return -ENOMEM;
		buf[0] = '"';
		os_memcpy(buf + 1, params->passphrase, iwr.u.data.length - 3);
		buf[iwr.u.data.length - 2] = '"';
		buf[iwr.u.data.length - 1] = '\0';
	} else
		return -EINVAL;
	iwr.u.data.pointer = (caddr_t) buf;
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	ret = ioctl(drv->ioctl_sock, SIOCIWFIRSTPRIV, &iwr);
	os_free(buf);

	return ret;
}

static int wpa_driver_ps3_set_wep_keys(struct wpa_driver_wext_data *drv,
				struct wpa_driver_associate_params *params)
{
	int ret, i;
	struct iwreq iwr;

	for (i = 0; i < 4; i++) {
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
		iwr.u.encoding.flags = i + 1;
		if (params->wep_key_len[i]) {
			iwr.u.encoding.pointer = (caddr_t) params->wep_key[i];
			iwr.u.encoding.length = params->wep_key_len[i];
		} else
			iwr.u.encoding.flags = IW_ENCODE_NOKEY |
				IW_ENCODE_DISABLED;

		if (ioctl(drv->ioctl_sock, SIOCSIWENCODE, &iwr) < 0) {
			perror("ioctl[SIOCSIWENCODE]");
			ret = -1;
		}
	}
	return ret;
}

static int wpa_driver_ps3_associate(void *priv,
				    struct wpa_driver_associate_params *params)
{
	struct wpa_driver_wext_data *drv = priv;
	int ret, value;

	wpa_printf(MSG_DEBUG, "%s: <-", __func__);

	/* clear BSSID */
	if (!params->bssid &&
	    wpa_driver_wext_set_bssid(drv, NULL) < 0)
		ret = -1;

	if (wpa_driver_wext_set_mode(drv, params->mode) < 0)
		ret = -1;

	if (params->wpa_ie == NULL || params->wpa_ie_len == 0)
		value = IW_AUTH_WPA_VERSION_DISABLED;
	else if (params->wpa_ie[0] == WLAN_EID_RSN)
		value = IW_AUTH_WPA_VERSION_WPA2;
	else
		value = IW_AUTH_WPA_VERSION_WPA;
	if (wpa_driver_wext_set_auth_param(drv,
					   IW_AUTH_WPA_VERSION, value) < 0)
		ret = -1;
	value = wpa_driver_wext_cipher2wext(params->pairwise_suite);
	if (wpa_driver_wext_set_auth_param(drv,
					   IW_AUTH_CIPHER_PAIRWISE, value) < 0)
		ret = -1;
	value = wpa_driver_wext_cipher2wext(params->group_suite);
	if (wpa_driver_wext_set_auth_param(drv,
					   IW_AUTH_CIPHER_GROUP, value) < 0)
		ret = -1;
	value = wpa_driver_wext_keymgmt2wext(params->key_mgmt_suite);
	if (wpa_driver_wext_set_auth_param(drv, IW_AUTH_KEY_MGMT, value) < 0)
		ret = -1;

	/* set selected BSSID */
	if (params->bssid &&
	    wpa_driver_wext_set_bssid(drv, params->bssid) < 0)
		ret = -1;

	switch (params->group_suite) {
	case CIPHER_NONE:
		ret = 0;
		break;
	case CIPHER_WEP40:
	case CIPHER_WEP104:
		ret = wpa_driver_ps3_set_wep_keys(drv, params);
		break;
	case CIPHER_TKIP:
	case CIPHER_CCMP:
		ret = wpa_driver_ps3_set_wpa_key(drv, params);
		break;
	}

	/* start to associate */
	ret = wpa_driver_wext_set_ssid(drv, params->ssid, params->ssid_len);

	wpa_printf(MSG_DEBUG, "%s: ->", __func__);

	return ret;
}

static int wpa_driver_ps3_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	int ret;
	wpa_printf(MSG_DEBUG, "%s:<-", __func__);

	ret = wpa_driver_wext_get_capa(priv, capa);
	if (ret) {
		wpa_printf(MSG_INFO, "%s: base wext returns error %d",
			   __func__, ret);
		return ret;
	}
	/* PS3 hypervisor does association and 4way handshake by itself */
	capa->flags |= WPA_DRIVER_FLAGS_4WAY_HANDSHAKE;
	wpa_printf(MSG_DEBUG, "%s:->", __func__);
	return 0;
}

const struct wpa_driver_ops wpa_driver_ps3_ops = {
	.name = "ps3",
	.desc = "PLAYSTATION3 Linux wireless extension driver",
	.get_bssid = wpa_driver_wext_get_bssid,
	.get_ssid = wpa_driver_wext_get_ssid,
	.scan = wpa_driver_wext_scan,
	.get_scan_results2 = wpa_driver_wext_get_scan_results,
	.associate = wpa_driver_ps3_associate, /* PS3 */
	.init = wpa_driver_wext_init,
	.deinit = wpa_driver_wext_deinit,
	.get_capa = wpa_driver_ps3_get_capa, /* PS3 */
};
