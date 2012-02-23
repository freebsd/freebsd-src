/*
 * WPA Supplicant - driver interaction with Linux ndiswrapper
 * Copyright (c) 2004-2006, Giridhar Pemmasani <giri@lmc.cs.sunysb.edu>
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
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
 * Please note that ndiswrapper supports WPA configuration via Linux wireless
 * extensions and if the kernel includes support for this, driver_wext.c should
 * be used instead of this driver wrapper.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "driver_wext.h"

struct wpa_driver_ndiswrapper_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
};


struct wpa_key {
	enum wpa_alg alg;
	const u8 *addr;
	int key_index;
	int set_tx;
	const u8 *seq;
	size_t seq_len;
	const u8 *key;
	size_t key_len;
};

struct wpa_assoc_info {
	const u8 *bssid;
	const u8 *ssid;
	size_t ssid_len;
	int freq;
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	enum wpa_cipher pairwise_suite;
	enum wpa_cipher group_suite;
	enum wpa_key_mgmt key_mgmt_suite;
	int auth_alg;
	int mode;
};

#define PRIV_RESET	 		SIOCIWFIRSTPRIV+0
#define WPA_SET_WPA 			SIOCIWFIRSTPRIV+1
#define WPA_SET_KEY 			SIOCIWFIRSTPRIV+2
#define WPA_ASSOCIATE		 	SIOCIWFIRSTPRIV+3
#define WPA_DISASSOCIATE 		SIOCIWFIRSTPRIV+4
#define WPA_DROP_UNENCRYPTED 		SIOCIWFIRSTPRIV+5
#define WPA_SET_COUNTERMEASURES 	SIOCIWFIRSTPRIV+6
#define WPA_DEAUTHENTICATE	 	SIOCIWFIRSTPRIV+7
#define WPA_SET_AUTH_ALG	 	SIOCIWFIRSTPRIV+8
#define WPA_INIT			SIOCIWFIRSTPRIV+9
#define WPA_DEINIT			SIOCIWFIRSTPRIV+10
#define WPA_GET_CAPA		 	SIOCIWFIRSTPRIV+11

static int wpa_ndiswrapper_set_auth_alg(void *priv, int auth_alg);

static int get_socket(void)
{
	static const int families[] = {
		AF_INET, AF_IPX, AF_AX25, AF_APPLETALK
	};
	unsigned int i;
	int sock;

	for (i = 0; i < sizeof(families) / sizeof(int); ++i) {
		sock = socket(families[i], SOCK_DGRAM, 0);
		if (sock >= 0)
			return sock;
	}

	return -1;
}

static int iw_set_ext(struct wpa_driver_ndiswrapper_data *drv, int request,
		      struct iwreq *pwrq)
{
	os_strlcpy(pwrq->ifr_name, drv->ifname, IFNAMSIZ);
	return ioctl(drv->sock, request, pwrq);
}

static int wpa_ndiswrapper_set_wpa(void *priv, int enabled)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	struct iwreq priv_req;
	int ret = 0;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.data.flags = enabled;
	if (iw_set_ext(drv, WPA_SET_WPA, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int wpa_ndiswrapper_set_key(const char *ifname, void *priv,
				   enum wpa_alg alg, const u8 *addr,
				   int key_idx, int set_tx,
				   const u8 *seq, size_t seq_len,
				   const u8 *key, size_t key_len)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	struct wpa_key wpa_key;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	wpa_key.alg = alg;
	wpa_key.addr = addr;
	wpa_key.key_index = key_idx;
	wpa_key.set_tx = set_tx;
	wpa_key.seq = seq;
	wpa_key.seq_len = seq_len;
	wpa_key.key = key;
	wpa_key.key_len = key_len;

	priv_req.u.data.pointer = (void *)&wpa_key;
	priv_req.u.data.length = sizeof(wpa_key);

	if (iw_set_ext(drv, WPA_SET_KEY, &priv_req) < 0)
		ret = -1;

	if (alg == WPA_ALG_NONE) {
		/*
		 * ndiswrapper did not seem to be clearing keys properly in
		 * some cases with WPA_SET_KEY. For example, roaming from WPA
		 * enabled AP to plaintext one seemed to fail since the driver
		 * did not associate. Try to make sure the keys are cleared so
		 * that plaintext APs can be used in all cases.
		 */
		wpa_driver_wext_set_key(ifname, drv->wext, alg, addr, key_idx,
					set_tx, seq, seq_len, key, key_len);
	}

	return ret;
}

static int wpa_ndiswrapper_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.param.value = enabled;
	if (iw_set_ext(drv, WPA_SET_COUNTERMEASURES, &priv_req) < 0)
		ret = -1;

	return ret;
}

static int wpa_ndiswrapper_set_drop_unencrypted(void *priv,
						int enabled)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.param.value = enabled;
	if (iw_set_ext(drv, WPA_DROP_UNENCRYPTED, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int wpa_ndiswrapper_deauthenticate(void *priv, const u8 *addr,
					  int reason_code)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.param.value = reason_code;
	os_memcpy(&priv_req.u.ap_addr.sa_data, addr, ETH_ALEN);
	if (iw_set_ext(drv, WPA_DEAUTHENTICATE, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int wpa_ndiswrapper_disassociate(void *priv, const u8 *addr,
					int reason_code)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	os_memcpy(&priv_req.u.ap_addr.sa_data, addr, ETH_ALEN);
	if (iw_set_ext(drv, WPA_DISASSOCIATE, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int
wpa_ndiswrapper_associate(void *priv,
			  struct wpa_driver_associate_params *params)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct wpa_assoc_info wpa_assoc_info;
	struct iwreq priv_req;

	if (wpa_ndiswrapper_set_drop_unencrypted(drv,
						 params->drop_unencrypted) < 0)
		ret = -1;
	if (wpa_ndiswrapper_set_auth_alg(drv, params->auth_alg) < 0)
		ret = -1;

	os_memset(&priv_req, 0, sizeof(priv_req));
	os_memset(&wpa_assoc_info, 0, sizeof(wpa_assoc_info));

	wpa_assoc_info.bssid = params->bssid;
	wpa_assoc_info.ssid = params->ssid;
	wpa_assoc_info.ssid_len = params->ssid_len;
	wpa_assoc_info.freq = params->freq;
	wpa_assoc_info.wpa_ie = params->wpa_ie;
	wpa_assoc_info.wpa_ie_len = params->wpa_ie_len;
	wpa_assoc_info.pairwise_suite = params->pairwise_suite;
	wpa_assoc_info.group_suite = params->group_suite;
	wpa_assoc_info.key_mgmt_suite = params->key_mgmt_suite;
	wpa_assoc_info.auth_alg = params->auth_alg;
	wpa_assoc_info.mode = params->mode;

	priv_req.u.data.pointer = (void *)&wpa_assoc_info;
	priv_req.u.data.length = sizeof(wpa_assoc_info);

	if (iw_set_ext(drv, WPA_ASSOCIATE, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int wpa_ndiswrapper_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.param.value = auth_alg;
	if (iw_set_ext(drv, WPA_SET_AUTH_ALG, &priv_req) < 0)
		ret = -1;
	return ret;
}

static int wpa_ndiswrapper_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_ndiswrapper_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static int wpa_ndiswrapper_scan(void *priv,
				struct wpa_driver_scan_params *params)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	return wpa_driver_wext_scan(drv->wext, params);
}


static struct wpa_scan_results * wpa_ndiswrapper_get_scan_results(void *priv)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext);
}


static int wpa_ndiswrapper_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	int ret = 0;
	struct iwreq priv_req;

	os_memset(&priv_req, 0, sizeof(priv_req));

	priv_req.u.data.pointer = (void *) capa;
	priv_req.u.data.length = sizeof(*capa);
	if (iw_set_ext(drv, WPA_GET_CAPA, &priv_req) < 0)
		ret = -1;
	return ret;
	
}


static int wpa_ndiswrapper_set_operstate(void *priv, int state)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


static void * wpa_ndiswrapper_init(void *ctx, const char *ifname)
{
	struct wpa_driver_ndiswrapper_data *drv;

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
	drv->sock = get_socket();
	if (drv->sock < 0) {
		wpa_driver_wext_deinit(drv->wext);
		os_free(drv);
		return NULL;
	}

	wpa_ndiswrapper_set_wpa(drv, 1);

	return drv;
}


static void wpa_ndiswrapper_deinit(void *priv)
{
	struct wpa_driver_ndiswrapper_data *drv = priv;
	wpa_ndiswrapper_set_wpa(drv, 0);
	wpa_driver_wext_deinit(drv->wext);
	close(drv->sock);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_ndiswrapper_ops = {
	.name = "ndiswrapper",
	.desc = "Linux ndiswrapper (deprecated; use wext)",
	.set_key = wpa_ndiswrapper_set_key,
	.set_countermeasures = wpa_ndiswrapper_set_countermeasures,
	.deauthenticate = wpa_ndiswrapper_deauthenticate,
	.disassociate = wpa_ndiswrapper_disassociate,
	.associate = wpa_ndiswrapper_associate,

	.get_bssid = wpa_ndiswrapper_get_bssid,
	.get_ssid = wpa_ndiswrapper_get_ssid,
	.scan2 = wpa_ndiswrapper_scan,
	.get_scan_results2 = wpa_ndiswrapper_get_scan_results,
	.init = wpa_ndiswrapper_init,
	.deinit = wpa_ndiswrapper_deinit,
	.get_capa = wpa_ndiswrapper_get_capa,
	.set_operstate = wpa_ndiswrapper_set_operstate,
};
