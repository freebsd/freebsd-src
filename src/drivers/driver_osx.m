/*
 * WPA Supplicant - Mac OS X Apple80211 driver interface
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
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
#define Boolean __DummyBoolean
#include <CoreFoundation/CoreFoundation.h>
#undef Boolean

#include "common.h"
#include "driver.h"
#include "eloop.h"

#include "Apple80211.h"

struct wpa_driver_osx_data {
	void *ctx;
	WirelessRef wireless_ctx;
	CFArrayRef scan_results;
};


#ifndef CONFIG_NO_STDOUT_DEBUG
extern int wpa_debug_level;

static void dump_dict_cb(const void *key, const void *value, void *context)
{
        if (MSG_DEBUG < wpa_debug_level)
                return;

	wpa_printf(MSG_DEBUG, "Key:");
	CFShow(key);
	wpa_printf(MSG_DEBUG, "Value:");
	CFShow(value);
}
#endif /* CONFIG_NO_STDOUT_DEBUG */


static void wpa_driver_osx_dump_dict(CFDictionaryRef dict, const char *title)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	wpa_printf(MSG_DEBUG, "OSX: Dump dictionary %s - %u entries",
		   title, (unsigned int) CFDictionaryGetCount(dict));
	CFDictionaryApplyFunction(dict, dump_dict_cb, NULL);
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static int wpa_driver_osx_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;
	WirelessInfo info;
	int len;

	err = WirelessGetInfo(drv->wireless_ctx, &info);
	if (err) {
		wpa_printf(MSG_DEBUG, "OSX: WirelessGetInfo failed: %d",
			   (int) err);
		return -1;
	}
	if (!info.power) {
		wpa_printf(MSG_DEBUG, "OSX: Wireless device power off");
		return -1;
	}

	for (len = 0; len < 32; len++)
		if (info.ssid[len] == 0)
			break;

	os_memcpy(ssid, info.ssid, len);
	return len;
}


static int wpa_driver_osx_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;
	WirelessInfo info;

	err = WirelessGetInfo(drv->wireless_ctx, &info);
	if (err) {
		wpa_printf(MSG_DEBUG, "OSX: WirelessGetInfo failed: %d",
			   (int) err);
		return -1;
	}
	if (!info.power) {
		wpa_printf(MSG_DEBUG, "OSX: Wireless device power off");
		return -1;
	}

	os_memcpy(bssid, info.bssID, ETH_ALEN);
	return 0;
}


static void wpa_driver_osx_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


static int wpa_driver_osx_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;

	if (drv->scan_results) {
		CFRelease(drv->scan_results);
		drv->scan_results = NULL;
	}

	if (ssid) {
		CFStringRef data;
		data = CFStringCreateWithBytes(kCFAllocatorDefault,
					       ssid, ssid_len,
					       kCFStringEncodingISOLatin1,
					       FALSE);
		if (data == NULL) {
			wpa_printf(MSG_DEBUG, "CFStringCreateWithBytes "
				   "failed");
			return -1;
		}

		err = WirelessDirectedScan(drv->wireless_ctx,
					   &drv->scan_results, 0, data);
		CFRelease(data);
		if (err) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessDirectedScan "
				   "failed: 0x%08x", (unsigned int) err);
			return -1;
		}
	} else {
		err = WirelessScan(drv->wireless_ctx, &drv->scan_results, 0);
		if (err) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessScan failed: "
				   "0x%08x", (unsigned int) err);
			return -1;
		}
	}

	eloop_register_timeout(0, 0, wpa_driver_osx_scan_timeout, drv,
			       drv->ctx);
	return 0;
}


static int wpa_driver_osx_get_scan_results(void *priv,
					   struct wpa_scan_result *results,
					   size_t max_size)
{
	struct wpa_driver_osx_data *drv = priv;
	size_t i, num;

	if (drv->scan_results == NULL)
		return 0;

	num = CFArrayGetCount(drv->scan_results);
	if (num > max_size)
		num = max_size;
	os_memset(results, 0, num * sizeof(struct wpa_scan_result));

	for (i = 0; i < num; i++) {
		struct wpa_scan_result *res = &results[i];
		WirelessNetworkInfo *info;
		info = (WirelessNetworkInfo *)
			CFDataGetBytePtr(CFArrayGetValueAtIndex(
						 drv->scan_results, i));

		os_memcpy(res->bssid, info->bssid, ETH_ALEN);
		if (info->ssid_len > 32) {
			wpa_printf(MSG_DEBUG, "OSX: Invalid SSID length %d in "
				   "scan results", (int) info->ssid_len);
			continue;
		}
		os_memcpy(res->ssid, info->ssid, info->ssid_len);
		res->ssid_len = info->ssid_len;
		res->caps = info->capability;
		res->freq = 2407 + info->channel * 5;
		res->level = info->signal;
		res->noise = info->noise;
	}

	return num;
}


static void wpa_driver_osx_assoc_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_driver_osx_data *drv = eloop_ctx;
	u8 bssid[ETH_ALEN];
	CFDictionaryRef ai;

	if (wpa_driver_osx_get_bssid(drv, bssid) != 0) {
		eloop_register_timeout(1, 0, wpa_driver_osx_assoc_timeout,
				       drv, drv->ctx);
		return;
	}

	ai = WirelessGetAssociationInfo(drv->wireless_ctx);
	if (ai) {
		wpa_driver_osx_dump_dict(ai, "WirelessGetAssociationInfo");
		CFRelease(ai);
	} else {
		wpa_printf(MSG_DEBUG, "OSX: Failed to get association info");
	}

	wpa_supplicant_event(timeout_ctx, EVENT_ASSOC, NULL);
}


static int wpa_driver_osx_associate(void *priv,
				    struct wpa_driver_associate_params *params)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;
	CFDataRef ssid;
	CFStringRef key;
	int assoc_type;

	ssid = CFDataCreate(kCFAllocatorDefault, params->ssid,
			    params->ssid_len);
	if (ssid == NULL)
		return -1;

	/* TODO: support for WEP */
	if (params->key_mgmt_suite == KEY_MGMT_PSK) {
		if (params->passphrase == NULL)
			return -1;
		key = CFStringCreateWithCString(kCFAllocatorDefault,
						params->passphrase,
						kCFStringEncodingISOLatin1);
		if (key == NULL) {
			CFRelease(ssid);
			return -1;
		}
	} else
		key = NULL;

	if (params->key_mgmt_suite == KEY_MGMT_NONE)
		assoc_type = 0;
	else
		assoc_type = 4;

	wpa_printf(MSG_DEBUG, "OSX: WirelessAssociate(type=%d key=%p)",
		   assoc_type, key);
	err = WirelessAssociate(drv->wireless_ctx, assoc_type, ssid, key);
	CFRelease(ssid);
	if (key)
		CFRelease(key);
	if (err) {
		wpa_printf(MSG_DEBUG, "OSX: WirelessAssociate failed: 0x%08x",
			   (unsigned int) err);
		return -1;
	}

	/*
	 * Driver is actually already associated; report association from an
	 * eloop callback.
	 */
	eloop_cancel_timeout(wpa_driver_osx_assoc_timeout, drv, drv->ctx);
	eloop_register_timeout(0, 0, wpa_driver_osx_assoc_timeout, drv,
			       drv->ctx);

	return 0;
}


static int wpa_driver_osx_set_key(void *priv, wpa_alg alg, const u8 *addr,
				  int key_idx, int set_tx, const u8 *seq,
				  size_t seq_len, const u8 *key,
				  size_t key_len)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;

	if (alg == WPA_ALG_WEP) {
		err = WirelessSetKey(drv->wireless_ctx, 1, key_idx, key_len,
				     key);
		if (err != 0) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessSetKey failed: "
				   "0x%08x", (unsigned int) err);
			return -1;
		}

		return 0;
	}

	if (alg == WPA_ALG_PMK) {
		err = WirelessSetWPAKey(drv->wireless_ctx, 1, key_len, key);
		if (err != 0) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessSetWPAKey failed: "
				   "0x%08x", (unsigned int) err);
			return -1;
		}
		return 0;
	}

	wpa_printf(MSG_DEBUG, "OSX: Unsupported set_key alg %d", alg);
	return -1;
}


static int wpa_driver_osx_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	os_memset(capa, 0, sizeof(*capa));

	capa->key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
	capa->enc = WPA_DRIVER_CAPA_ENC_WEP40 | WPA_DRIVER_CAPA_ENC_WEP104 |
		WPA_DRIVER_CAPA_ENC_TKIP | WPA_DRIVER_CAPA_ENC_CCMP;
	capa->auth = WPA_DRIVER_AUTH_OPEN | WPA_DRIVER_AUTH_SHARED |
		WPA_DRIVER_AUTH_LEAP;
	capa->flags = WPA_DRIVER_FLAGS_4WAY_HANDSHAKE;

	return 0;
}


static void * wpa_driver_osx_init(void *ctx, const char *ifname)
{
	struct wpa_driver_osx_data *drv;
	WirelessError err;
	u8 enabled, power;

	if (!WirelessIsAvailable()) {
		wpa_printf(MSG_ERROR, "OSX: No wireless interface available");
		return NULL;
	}

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->ctx = ctx;
	err = WirelessAttach(&drv->wireless_ctx, 0);
	if (err) {
		wpa_printf(MSG_ERROR, "OSX: WirelessAttach failed: %d",
			   (int) err);
		os_free(drv);
		return NULL;
	}

	err = WirelessGetEnabled(drv->wireless_ctx, &enabled);
	if (err)
		wpa_printf(MSG_DEBUG, "OSX: WirelessGetEnabled failed: 0x%08x",
			   (unsigned int) err);
	err = WirelessGetPower(drv->wireless_ctx, &power);
	if (err)
		wpa_printf(MSG_DEBUG, "OSX: WirelessGetPower failed: 0x%08x",
			   (unsigned int) err);

	wpa_printf(MSG_DEBUG, "OSX: Enabled=%d Power=%d", enabled, power);

	if (!enabled) {
		err = WirelessSetEnabled(drv->wireless_ctx, 1);
		if (err) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessSetEnabled failed:"
				   " 0x%08x", (unsigned int) err);
			WirelessDetach(drv->wireless_ctx);
			os_free(drv);
			return NULL;
		}
	}

	if (!power) {
		err = WirelessSetPower(drv->wireless_ctx, 1);
		if (err) {
			wpa_printf(MSG_DEBUG, "OSX: WirelessSetPower failed: "
				   "0x%08x", (unsigned int) err);
			WirelessDetach(drv->wireless_ctx);
			os_free(drv);
			return NULL;
		}
	}

	return drv;
}


static void wpa_driver_osx_deinit(void *priv)
{
	struct wpa_driver_osx_data *drv = priv;
	WirelessError err;

	eloop_cancel_timeout(wpa_driver_osx_scan_timeout, drv, drv->ctx);
	eloop_cancel_timeout(wpa_driver_osx_assoc_timeout, drv, drv->ctx);

	err = WirelessSetPower(drv->wireless_ctx, 0);
	if (err) {
		wpa_printf(MSG_DEBUG, "OSX: WirelessSetPower(0) failed: "
			   "0x%08x", (unsigned int) err);
	}

	err = WirelessDetach(drv->wireless_ctx);
	if (err) {
		wpa_printf(MSG_DEBUG, "OSX: WirelessDetach failed: 0x%08x",
			   (unsigned int) err);
	}

	if (drv->scan_results)
		CFRelease(drv->scan_results);

	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_osx_ops = {
	.name = "osx",
	.desc = "Mac OS X Apple80211 driver",
	.get_ssid = wpa_driver_osx_get_ssid,
	.get_bssid = wpa_driver_osx_get_bssid,
	.init = wpa_driver_osx_init,
	.deinit = wpa_driver_osx_deinit,
	.scan = wpa_driver_osx_scan,
	.get_scan_results = wpa_driver_osx_get_scan_results,
	.associate = wpa_driver_osx_associate,
	.set_key = wpa_driver_osx_set_key,
	.get_capa = wpa_driver_osx_get_capa,
};
