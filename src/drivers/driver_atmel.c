/*
 * WPA Supplicant - Driver interaction with Atmel Wireless LAN drivers
 * Copyright (c) 2000-2005, ATMEL Corporation
 * Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi>
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

/******************************************************************************
	Copyright 2000-2001 ATMEL Corporation.
	
    WPA Supplicant - driver interaction with Atmel Wireless lan drivers.
    
    This is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Atmel wireless lan drivers; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

******************************************************************************/

/*
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "driver_wext.h"

struct wpa_driver_atmel_data {
	void *wext; /* private data for driver_wext */
	void *ctx;
	char ifname[IFNAMSIZ + 1];
	int sock;
};


#define ATMEL_WPA_IOCTL                (SIOCIWFIRSTPRIV + 2)
#define ATMEL_WPA_IOCTL_PARAM          (SIOCIWFIRSTPRIV + 3)
#define ATMEL_WPA_IOCTL_GET_PARAM      (SIOCIWFIRSTPRIV + 4)


/* ATMEL_WPA_IOCTL ioctl() cmd: */
enum {
    SET_WPA_ENCRYPTION  = 1,
    SET_CIPHER_SUITES   = 2,
    MLME_STA_DEAUTH     = 3,
    MLME_STA_DISASSOC   = 4
};

/* ATMEL_WPA_IOCTL_PARAM ioctl() cmd: */
enum {
            ATMEL_PARAM_WPA = 1,
            ATMEL_PARAM_PRIVACY_INVOKED = 2,
            ATMEL_PARAM_WPA_TYPE = 3
};

#define MAX_KEY_LENGTH      40

struct atmel_param{
    unsigned char sta_addr[6];
        int     cmd;
        u8      alg;
        u8      key_idx;
        u8      set_tx;
        u8      seq[8];
        u8      seq_len;
        u16     key_len;
        u8      key[MAX_KEY_LENGTH];
    struct{
        int     reason_code;
        u8      state;
    }mlme;
    u8          pairwise_suite;
    u8          group_suite;
    u8          key_mgmt_suite;
};

    
    
static int atmel_ioctl(struct wpa_driver_atmel_data *drv,
		       struct atmel_param *param,
		       int len, int show_err)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) param;
	iwr.u.data.length = len;

	if (ioctl(drv->sock, ATMEL_WPA_IOCTL, &iwr) < 0) {
		int ret;
		ret = errno;
		if (show_err) 
			perror("ioctl[ATMEL_WPA_IOCTL]");
		return ret;
	}

	return 0;
}


static int atmel2param(struct wpa_driver_atmel_data *drv, int param, int value)
{
	struct iwreq iwr;
	int *i, ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	i = (int *) iwr.u.name;
	*i++ = param;
	*i++ = value;

	if (ioctl(drv->sock, ATMEL_WPA_IOCTL_PARAM, &iwr) < 0) {
		perror("ioctl[ATMEL_WPA_IOCTL_PARAM]");
		ret = -1;
	}
	return ret;
}


#if 0
static int wpa_driver_atmel_set_wpa_ie(struct wpa_driver_atmel_data *drv,
				       const char *wpa_ie, size_t wpa_ie_len)
{
	struct atmel_param *param;
	int res;
	size_t blen = ATMEL_GENERIC_ELEMENT_HDR_LEN + wpa_ie_len;
	if (blen < sizeof(*param))
		blen = sizeof(*param);

	param = os_zalloc(blen);
	if (param == NULL)
		return -1;

	param->cmd = ATMEL_SET_GENERIC_ELEMENT;
	param->u.generic_elem.len = wpa_ie_len;
	os_memcpy(param->u.generic_elem.data, wpa_ie, wpa_ie_len);
	res = atmel_ioctl(drv, param, blen, 1);

	os_free(param);

	return res;
}
#endif


static int wpa_driver_atmel_set_wpa(void *priv, int enabled)
{
	struct wpa_driver_atmel_data *drv = priv;
        int ret = 0;
	
        printf("wpa_driver_atmel_set_wpa %s\n", drv->ifname);

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);

#if 0
	if (!enabled && wpa_driver_atmel_set_wpa_ie(drv, NULL, 0) < 0)
		ret = -1;
#endif
	if (atmel2param(drv, ATMEL_PARAM_PRIVACY_INVOKED, enabled) < 0)
		ret = -1;
	if (atmel2param(drv, ATMEL_PARAM_WPA, enabled) < 0)
		ret = -1;

	return ret;
}


static int wpa_driver_atmel_set_key(const char *ifname, void *priv,
				    enum wpa_alg alg, const u8 *addr,
				    int key_idx, int set_tx,
				    const u8 *seq, size_t seq_len,
				    const u8 *key, size_t key_len)
{
	struct wpa_driver_atmel_data *drv = priv;
	int ret = 0;
        struct atmel_param *param;
	u8 *buf;
        u8 alg_type;
        
	size_t blen;
	char *alg_name;

	switch (alg) {
	case WPA_ALG_NONE:
		alg_name = "none";
                alg_type = 0;
		break;
	case WPA_ALG_WEP:
		alg_name = "WEP";
		alg_type = 1;
                break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		alg_type = 2;
                break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		alg_type = 3;
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

	param = (struct atmel_param *) buf;
        
        param->cmd = SET_WPA_ENCRYPTION; 
        
        if (addr == NULL)
		os_memset(param->sta_addr, 0xff, ETH_ALEN);
	else
		os_memcpy(param->sta_addr, addr, ETH_ALEN);
        
        param->alg = alg_type;
        param->key_idx = key_idx;
        param->set_tx = set_tx;
        os_memcpy(param->seq, seq, seq_len);
        param->seq_len = seq_len;
        param->key_len = key_len;
	os_memcpy((u8 *)param->key, key, key_len);
	
        if (atmel_ioctl(drv, param, blen, 1)) {
		wpa_printf(MSG_WARNING, "Failed to set encryption.");
		/* TODO: show key error*/
		ret = -1;
	}
	os_free(buf);

	return ret;
}


static int wpa_driver_atmel_set_countermeasures(void *priv,
						 int enabled)
{
	/* FIX */
	printf("wpa_driver_atmel_set_countermeasures - not yet "
	       "implemented\n");
	return 0;
}


static int wpa_driver_atmel_mlme(void *priv, const u8 *addr, int cmd,
				 int reason_code)
{
	struct wpa_driver_atmel_data *drv = priv;
	struct atmel_param param;
	int ret;
        int mgmt_error = 0xaa;
        
	os_memset(&param, 0, sizeof(param));
	os_memcpy(param.sta_addr, addr, ETH_ALEN);
	param.cmd = cmd;
	param.mlme.reason_code = reason_code;
        param.mlme.state = mgmt_error;
	ret = atmel_ioctl(drv, &param, sizeof(param), 1);
	return ret;
}


#if 0
static int wpa_driver_atmel_set_suites(struct wpa_driver_atmel_data *drv,
				       u8 pairwise_suite, u8 group_suite,
				       u8 key_mgmt_suite)
{
	struct atmel_param param;
	int ret;
        
	os_memset(&param, 0, sizeof(param));
        param.cmd = SET_CIPHER_SUITES;
        param.pairwise_suite = pairwise_suite;
        param.group_suite = group_suite;
        param.key_mgmt_suite = key_mgmt_suite;
	        
	ret = atmel_ioctl(drv, &param, sizeof(param), 1);
	return ret;
}
#endif


static int wpa_driver_atmel_deauthenticate(void *priv, const u8 *addr,
					   int reason_code)
{
	struct wpa_driver_atmel_data *drv = priv;
	printf("wpa_driver_atmel_deauthenticate\n");
        wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_atmel_mlme(drv, addr, MLME_STA_DEAUTH,
				     reason_code);

}


static int wpa_driver_atmel_disassociate(void *priv, const u8 *addr,
					 int reason_code)
{
	struct wpa_driver_atmel_data *drv = priv;
	printf("wpa_driver_atmel_disassociate\n");
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_atmel_mlme(drv, addr, MLME_STA_DISASSOC,
				     reason_code);

}


#if 0
/* Atmel driver uses specific values for each cipher suite */
static int convertSuiteToDriver(enum wpa_cipher suite)
{
    u8 suite_type;
    
    switch(suite) {
        case CIPHER_NONE:
                suite_type =  0;
                break;
        case CIPHER_WEP40:
                suite_type =  1;
                break;
        case CIPHER_TKIP:
                suite_type = 2;
                break;
        case CIPHER_WEP104:
                suite_type = 5;
                break;
        case CIPHER_CCMP:
                suite_type = 3;
                break;
        default:
                suite_type = 2;
    }
    
    return suite_type;

}
#endif
    
static int
wpa_driver_atmel_associate(void *priv,
			   struct wpa_driver_associate_params *params)
{
	struct wpa_driver_atmel_data *drv = priv;
	int ret = 0;
#if 0
        u8 pairwise_suite_driver;
        u8 group_suite_driver;
        u8 key_mgmt_suite_driver;

        pairwise_suite_driver = convertSuiteToDriver(params->pairwise_suite);
        group_suite_driver    = convertSuiteToDriver(params->group_suite);
        key_mgmt_suite_driver = convertSuiteToDriver(params->key_mgmt_suite);

        if (wpa_driver_atmel_set_suites(drv, pairwise_suite_driver,
					group_suite_driver,
					key_mgmt_suite_driver) < 0){
		printf("wpa_driver_atmel_set_suites.\n");
                ret = -1;
        }
        if (wpa_driver_wext_set_freq(drv->wext, params->freq) < 0) {
	        printf("wpa_driver_atmel_set_freq.\n");
		ret = -1;
        }
#endif
	if (wpa_driver_wext_set_ssid(drv->wext, params->ssid, params->ssid_len)
	    < 0) {
	        printf("FAILED : wpa_driver_atmel_set_ssid.\n");
		ret = -1;
        }
	if (wpa_driver_wext_set_bssid(drv->wext, params->bssid) < 0) {
	        printf("FAILED : wpa_driver_atmel_set_bssid.\n");
		ret = -1;
        }

	return ret;
}


static int wpa_driver_atmel_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_atmel_data *drv = priv;
	return wpa_driver_wext_get_bssid(drv->wext, bssid);
}


static int wpa_driver_atmel_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_atmel_data *drv = priv;
	return wpa_driver_wext_get_ssid(drv->wext, ssid);
}


static int wpa_driver_atmel_scan(void *priv,
				 struct wpa_driver_scan_params *params)
{
	struct wpa_driver_atmel_data *drv = priv;
	return wpa_driver_wext_scan(drv->wext, params);
}


static struct wpa_scan_results * wpa_driver_atmel_get_scan_results(void *priv)
{
	struct wpa_driver_atmel_data *drv = priv;
	return wpa_driver_wext_get_scan_results(drv->wext);
}


static int wpa_driver_atmel_set_operstate(void *priv, int state)
{
	struct wpa_driver_atmel_data *drv = priv;
	return wpa_driver_wext_set_operstate(drv->wext, state);
}


static void * wpa_driver_atmel_init(void *ctx, const char *ifname)
{
	struct wpa_driver_atmel_data *drv;

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
		wpa_driver_wext_deinit(drv->wext);
		os_free(drv);
		return NULL;
	}

	wpa_driver_atmel_set_wpa(drv, 1);

	return drv;
}


static void wpa_driver_atmel_deinit(void *priv)
{
	struct wpa_driver_atmel_data *drv = priv;
	wpa_driver_atmel_set_wpa(drv, 0);
	wpa_driver_wext_deinit(drv->wext);
	close(drv->sock);
	os_free(drv);
}


const struct wpa_driver_ops wpa_driver_atmel_ops = {
	.name = "atmel",
	.desc = "ATMEL AT76C5XXx (USB, PCMCIA)",
	.get_bssid = wpa_driver_atmel_get_bssid,
	.get_ssid = wpa_driver_atmel_get_ssid,
	.set_key = wpa_driver_atmel_set_key,
	.init = wpa_driver_atmel_init,
	.deinit = wpa_driver_atmel_deinit,
	.set_countermeasures = wpa_driver_atmel_set_countermeasures,
	.scan2 = wpa_driver_atmel_scan,
	.get_scan_results2 = wpa_driver_atmel_get_scan_results,
	.deauthenticate = wpa_driver_atmel_deauthenticate,
	.disassociate = wpa_driver_atmel_disassociate,
	.associate = wpa_driver_atmel_associate,
	.set_operstate = wpa_driver_atmel_set_operstate,
};
