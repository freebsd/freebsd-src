/*
 * hostapd / WPS integration
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
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

#include "hostapd.h"
#include "driver.h"
#include "eloop.h"
#include "uuid.h"
#include "wpa_ctrl.h"
#include "ieee802_11_defs.h"
#include "sta_info.h"
#include "eapol_sm.h"
#include "wps/wps.h"
#include "wps/wps_defs.h"
#include "wps/wps_dev_attr.h"
#include "wps_hostapd.h"


#ifdef CONFIG_WPS_UPNP
#include "wps/wps_upnp.h"
static int hostapd_wps_upnp_init(struct hostapd_data *hapd,
				 struct wps_context *wps);
static void hostapd_wps_upnp_deinit(struct hostapd_data *hapd);
#endif /* CONFIG_WPS_UPNP */


static int hostapd_wps_new_psk_cb(void *ctx, const u8 *mac_addr, const u8 *psk,
				  size_t psk_len)
{
	struct hostapd_data *hapd = ctx;
	struct hostapd_wpa_psk *p;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	wpa_printf(MSG_DEBUG, "Received new WPA/WPA2-PSK from WPS for STA "
		   MACSTR, MAC2STR(mac_addr));
	wpa_hexdump_key(MSG_DEBUG, "Per-device PSK", psk, psk_len);

	if (psk_len != PMK_LEN) {
		wpa_printf(MSG_DEBUG, "Unexpected PSK length %lu",
			   (unsigned long) psk_len);
		return -1;
	}

	/* Add the new PSK to runtime PSK list */
	p = os_zalloc(sizeof(*p));
	if (p == NULL)
		return -1;
	os_memcpy(p->addr, mac_addr, ETH_ALEN);
	os_memcpy(p->psk, psk, PMK_LEN);

	p->next = ssid->wpa_psk;
	ssid->wpa_psk = p;

	if (ssid->wpa_psk_file) {
		FILE *f;
		char hex[PMK_LEN * 2 + 1];
		/* Add the new PSK to PSK list file */
		f = fopen(ssid->wpa_psk_file, "a");
		if (f == NULL) {
			wpa_printf(MSG_DEBUG, "Failed to add the PSK to "
				   "'%s'", ssid->wpa_psk_file);
			return -1;
		}

		wpa_snprintf_hex(hex, sizeof(hex), psk, psk_len);
		fprintf(f, MACSTR " %s\n", MAC2STR(mac_addr), hex);
		fclose(f);
	}

	return 0;
}


static int hostapd_wps_set_ie_cb(void *ctx, const u8 *beacon_ie,
				 size_t beacon_ie_len, const u8 *probe_resp_ie,
				 size_t probe_resp_ie_len)
{
	struct hostapd_data *hapd = ctx;

	os_free(hapd->wps_beacon_ie);
	if (beacon_ie_len == 0) {
		hapd->wps_beacon_ie = NULL;
		hapd->wps_beacon_ie_len = 0;
	} else {
		hapd->wps_beacon_ie = os_malloc(beacon_ie_len);
		if (hapd->wps_beacon_ie == NULL) {
			hapd->wps_beacon_ie_len = 0;
			return -1;
		}
		os_memcpy(hapd->wps_beacon_ie, beacon_ie, beacon_ie_len);
		hapd->wps_beacon_ie_len = beacon_ie_len;
	}
	hostapd_set_wps_beacon_ie(hapd, hapd->wps_beacon_ie,
				  hapd->wps_beacon_ie_len);

	os_free(hapd->wps_probe_resp_ie);
	if (probe_resp_ie_len == 0) {
		hapd->wps_probe_resp_ie = NULL;
		hapd->wps_probe_resp_ie_len = 0;
	} else {
		hapd->wps_probe_resp_ie = os_malloc(probe_resp_ie_len);
		if (hapd->wps_probe_resp_ie == NULL) {
			hapd->wps_probe_resp_ie_len = 0;
			return -1;
		}
		os_memcpy(hapd->wps_probe_resp_ie, probe_resp_ie,
			  probe_resp_ie_len);
		hapd->wps_probe_resp_ie_len = probe_resp_ie_len;
	}
	hostapd_set_wps_probe_resp_ie(hapd, hapd->wps_probe_resp_ie,
				      hapd->wps_probe_resp_ie_len);

	return 0;
}


static void hostapd_wps_pin_needed_cb(void *ctx, const u8 *uuid_e,
				      const struct wps_device_data *dev)
{
	struct hostapd_data *hapd = ctx;
	char uuid[40], txt[400];
	int len;
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	wpa_printf(MSG_DEBUG, "WPS: PIN needed for E-UUID %s", uuid);
	len = os_snprintf(txt, sizeof(txt), WPS_EVENT_PIN_NEEDED
			  "%s " MACSTR " [%s|%s|%s|%s|%s|%d-%08X-%d]",
			  uuid, MAC2STR(dev->mac_addr), dev->device_name,
			  dev->manufacturer, dev->model_name,
			  dev->model_number, dev->serial_number,
			  dev->categ, dev->oui, dev->sub_categ);
	if (len > 0 && len < (int) sizeof(txt))
		wpa_msg(hapd, MSG_INFO, "%s", txt);

	if (hapd->conf->wps_pin_requests) {
		FILE *f;
		struct os_time t;
		f = fopen(hapd->conf->wps_pin_requests, "a");
		if (f == NULL)
			return;
		os_get_time(&t);
		fprintf(f, "%ld\t%s\t" MACSTR "\t%s\t%s\t%s\t%s\t%s"
			"\t%d-%08X-%d\n",
			t.sec, uuid, MAC2STR(dev->mac_addr), dev->device_name,
			dev->manufacturer, dev->model_name, dev->model_number,
			dev->serial_number,
			dev->categ, dev->oui, dev->sub_categ);
		fclose(f);
	}
}


static void hostapd_wps_reg_success_cb(void *ctx, const u8 *mac_addr,
				       const u8 *uuid_e)
{
	struct hostapd_data *hapd = ctx;
	char uuid[40];
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	wpa_msg(hapd, MSG_INFO, WPS_EVENT_REG_SUCCESS MACSTR " %s",
		MAC2STR(mac_addr), uuid);
}


static int str_starts(const char *str, const char *start)
{
	return os_strncmp(str, start, os_strlen(start)) == 0;
}


static void wps_reload_config(void *eloop_data, void *user_ctx)
{
	struct hostapd_iface *iface = eloop_data;

	wpa_printf(MSG_DEBUG, "WPS: Reload configuration data");
	if (hostapd_reload_config(iface) < 0) {
		wpa_printf(MSG_WARNING, "WPS: Failed to reload the updated "
			   "configuration");
	}
}


static int hostapd_wps_cred_cb(void *ctx, const struct wps_credential *cred)
{
	struct hostapd_data *hapd = ctx;
	FILE *oconf, *nconf;
	size_t len, i;
	char *tmp_fname;
	char buf[1024];
	int multi_bss;
	int wpa;

	wpa_hexdump_key(MSG_DEBUG, "WPS: Received Credential attribute",
			cred->cred_attr, cred->cred_attr_len);

	wpa_printf(MSG_DEBUG, "WPS: Received new AP Settings");
	wpa_hexdump_ascii(MSG_DEBUG, "WPS: SSID", cred->ssid, cred->ssid_len);
	wpa_printf(MSG_DEBUG, "WPS: Authentication Type 0x%x",
		   cred->auth_type);
	wpa_printf(MSG_DEBUG, "WPS: Encryption Type 0x%x", cred->encr_type);
	wpa_printf(MSG_DEBUG, "WPS: Network Key Index %d", cred->key_idx);
	wpa_hexdump_key(MSG_DEBUG, "WPS: Network Key",
			cred->key, cred->key_len);
	wpa_printf(MSG_DEBUG, "WPS: MAC Address " MACSTR,
		   MAC2STR(cred->mac_addr));

	if ((hapd->conf->wps_cred_processing == 1 ||
	     hapd->conf->wps_cred_processing == 2) && cred->cred_attr) {
		size_t blen = cred->cred_attr_len * 2 + 1;
		char *_buf = os_malloc(blen);
		if (_buf) {
			wpa_snprintf_hex(_buf, blen,
					 cred->cred_attr, cred->cred_attr_len);
			wpa_msg(hapd, MSG_INFO, "%s%s",
				WPS_EVENT_NEW_AP_SETTINGS, _buf);
			os_free(_buf);
		}
	} else
		wpa_msg(hapd, MSG_INFO, WPS_EVENT_NEW_AP_SETTINGS);

	if (hapd->conf->wps_cred_processing == 1)
		return 0;

	os_memcpy(hapd->wps->ssid, cred->ssid, cred->ssid_len);
	hapd->wps->ssid_len = cred->ssid_len;
	hapd->wps->encr_types = cred->encr_type;
	hapd->wps->auth_types = cred->auth_type;
	if (cred->key_len == 0) {
		os_free(hapd->wps->network_key);
		hapd->wps->network_key = NULL;
		hapd->wps->network_key_len = 0;
	} else {
		if (hapd->wps->network_key == NULL ||
		    hapd->wps->network_key_len < cred->key_len) {
			hapd->wps->network_key_len = 0;
			os_free(hapd->wps->network_key);
			hapd->wps->network_key = os_malloc(cred->key_len);
			if (hapd->wps->network_key == NULL)
				return -1;
		}
		hapd->wps->network_key_len = cred->key_len;
		os_memcpy(hapd->wps->network_key, cred->key, cred->key_len);
	}
	hapd->wps->wps_state = WPS_STATE_CONFIGURED;

	len = os_strlen(hapd->iface->config_fname) + 5;
	tmp_fname = os_malloc(len);
	if (tmp_fname == NULL)
		return -1;
	os_snprintf(tmp_fname, len, "%s-new", hapd->iface->config_fname);

	oconf = fopen(hapd->iface->config_fname, "r");
	if (oconf == NULL) {
		wpa_printf(MSG_WARNING, "WPS: Could not open current "
			   "configuration file");
		os_free(tmp_fname);
		return -1;
	}

	nconf = fopen(tmp_fname, "w");
	if (nconf == NULL) {
		wpa_printf(MSG_WARNING, "WPS: Could not write updated "
			   "configuration file");
		os_free(tmp_fname);
		fclose(oconf);
		return -1;
	}

	fprintf(nconf, "# WPS configuration - START\n");

	fprintf(nconf, "wps_state=2\n");

	fprintf(nconf, "ssid=");
	for (i = 0; i < cred->ssid_len; i++)
		fputc(cred->ssid[i], nconf);
	fprintf(nconf, "\n");

	if ((cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA2PSK)) &&
	    (cred->auth_type & (WPS_AUTH_WPA | WPS_AUTH_WPAPSK)))
		wpa = 3;
	else if (cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA2PSK))
		wpa = 2;
	else if (cred->auth_type & (WPS_AUTH_WPA | WPS_AUTH_WPAPSK))
		wpa = 1;
	else
		wpa = 0;

	if (wpa) {
		char *prefix;
		fprintf(nconf, "wpa=%d\n", wpa);

		fprintf(nconf, "wpa_key_mgmt=");
		prefix = "";
		if (cred->auth_type & (WPS_AUTH_WPA2 | WPS_AUTH_WPA)) {
			fprintf(nconf, "WPA-EAP");
			prefix = " ";
		}
		if (cred->auth_type & (WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK))
			fprintf(nconf, "%sWPA-PSK", prefix);
		fprintf(nconf, "\n");

		fprintf(nconf, "wpa_pairwise=");
		prefix = "";
		if (cred->encr_type & WPS_ENCR_AES) {
			fprintf(nconf, "CCMP");
			prefix = " ";
		}
		if (cred->encr_type & WPS_ENCR_TKIP) {
			fprintf(nconf, "%sTKIP", prefix);
		}
		fprintf(nconf, "\n");

		if (cred->key_len >= 8 && cred->key_len < 64) {
			fprintf(nconf, "wpa_passphrase=");
			for (i = 0; i < cred->key_len; i++)
				fputc(cred->key[i], nconf);
			fprintf(nconf, "\n");
		} else if (cred->key_len == 64) {
			fprintf(nconf, "wpa_psk=");
			for (i = 0; i < cred->key_len; i++)
				fputc(cred->key[i], nconf);
			fprintf(nconf, "\n");
		} else {
			wpa_printf(MSG_WARNING, "WPS: Invalid key length %lu "
				   "for WPA/WPA2",
				   (unsigned long) cred->key_len);
		}

		fprintf(nconf, "auth_algs=1\n");
	} else {
		if ((cred->auth_type & WPS_AUTH_OPEN) &&
		    (cred->auth_type & WPS_AUTH_SHARED))
			fprintf(nconf, "auth_algs=3\n");
		else if (cred->auth_type & WPS_AUTH_SHARED)
			fprintf(nconf, "auth_algs=2\n");
		else
			fprintf(nconf, "auth_algs=1\n");

		if (cred->encr_type & WPS_ENCR_WEP && cred->key_idx <= 4) {
			int key_idx = cred->key_idx;
			if (key_idx)
				key_idx--;
			fprintf(nconf, "wep_default_key=%d\n", key_idx);
			fprintf(nconf, "wep_key%d=", key_idx);
			if (cred->key_len == 10 || cred->key_len == 26) {
				/* WEP key as a hex string */
				for (i = 0; i < cred->key_len; i++)
					fputc(cred->key[i], nconf);
			} else {
				/* Raw WEP key; convert to hex */
				for (i = 0; i < cred->key_len; i++)
					fprintf(nconf, "%02x", cred->key[i]);
			}
			fprintf(nconf, "\n");
		}
	}

	fprintf(nconf, "# WPS configuration - END\n");

	multi_bss = 0;
	while (fgets(buf, sizeof(buf), oconf)) {
		if (os_strncmp(buf, "bss=", 4) == 0)
			multi_bss = 1;
		if (!multi_bss &&
		    (str_starts(buf, "ssid=") ||
		     str_starts(buf, "auth_algs=") ||
		     str_starts(buf, "wps_state=") ||
		     str_starts(buf, "wpa=") ||
		     str_starts(buf, "wpa_psk=") ||
		     str_starts(buf, "wpa_pairwise=") ||
		     str_starts(buf, "rsn_pairwise=") ||
		     str_starts(buf, "wpa_key_mgmt=") ||
		     str_starts(buf, "wpa_passphrase="))) {
			fprintf(nconf, "#WPS# %s", buf);
		} else
			fprintf(nconf, "%s", buf);
	}

	fclose(nconf);
	fclose(oconf);

	if (rename(tmp_fname, hapd->iface->config_fname) < 0) {
		wpa_printf(MSG_WARNING, "WPS: Failed to rename the updated "
			   "configuration file: %s", strerror(errno));
		os_free(tmp_fname);
		return -1;
	}

	os_free(tmp_fname);

	/* Schedule configuration reload after short period of time to allow
	 * EAP-WSC to be finished.
	 */
	eloop_register_timeout(0, 100000, wps_reload_config, hapd->iface,
			       NULL);

	/* TODO: dualband AP may need to update multiple configuration files */

	wpa_printf(MSG_DEBUG, "WPS: AP configuration updated");

	return 0;
}


static void hostapd_pwd_auth_fail(struct hostapd_data *hapd,
				  struct wps_event_pwd_auth_fail *data)
{
	FILE *f;

	if (!data->enrollee)
		return;

	/*
	 * Registrar failed to prove its knowledge of the AP PIN. Lock AP setup
	 * if this happens multiple times.
	 */
	hapd->ap_pin_failures++;
	if (hapd->ap_pin_failures < 4)
		return;

	wpa_msg(hapd, MSG_INFO, WPS_EVENT_AP_SETUP_LOCKED);
	hapd->wps->ap_setup_locked = 1;

	wps_registrar_update_ie(hapd->wps->registrar);

	if (hapd->conf->wps_cred_processing == 1)
		return;

	f = fopen(hapd->iface->config_fname, "a");
	if (f == NULL) {
		wpa_printf(MSG_WARNING, "WPS: Could not append to the current "
			   "configuration file");
		return;
	}

	fprintf(f, "# WPS AP Setup Locked based on possible attack\n");
	fprintf(f, "ap_setup_locked=1\n");
	fclose(f);

	/* TODO: dualband AP may need to update multiple configuration files */

	wpa_printf(MSG_DEBUG, "WPS: AP configuration updated");
}


static void hostapd_wps_event_cb(void *ctx, enum wps_event event,
				 union wps_event_data *data)
{
	struct hostapd_data *hapd = ctx;

	if (event == WPS_EV_PWD_AUTH_FAIL)
		hostapd_pwd_auth_fail(hapd, &data->pwd_auth_fail);
}


static void hostapd_wps_clear_ies(struct hostapd_data *hapd)
{
	os_free(hapd->wps_beacon_ie);
	hapd->wps_beacon_ie = NULL;
	hapd->wps_beacon_ie_len = 0;
	hostapd_set_wps_beacon_ie(hapd, NULL, 0);

	os_free(hapd->wps_probe_resp_ie);
	hapd->wps_probe_resp_ie = NULL;
	hapd->wps_probe_resp_ie_len = 0;
	hostapd_set_wps_probe_resp_ie(hapd, NULL, 0);
}


int hostapd_init_wps(struct hostapd_data *hapd,
		     struct hostapd_bss_config *conf)
{
	struct wps_context *wps;
	struct wps_registrar_config cfg;

	if (conf->wps_state == 0) {
		hostapd_wps_clear_ies(hapd);
		return 0;
	}

	wps = os_zalloc(sizeof(*wps));
	if (wps == NULL)
		return -1;

	wps->cred_cb = hostapd_wps_cred_cb;
	wps->event_cb = hostapd_wps_event_cb;
	wps->cb_ctx = hapd;

	os_memset(&cfg, 0, sizeof(cfg));
	wps->wps_state = hapd->conf->wps_state;
	wps->ap_setup_locked = hapd->conf->ap_setup_locked;
	if (is_nil_uuid(hapd->conf->uuid)) {
		uuid_gen_mac_addr(hapd->own_addr, wps->uuid);
		wpa_hexdump(MSG_DEBUG, "WPS: UUID based on MAC address",
			    wps->uuid, UUID_LEN);
	} else
		os_memcpy(wps->uuid, hapd->conf->uuid, UUID_LEN);
	wps->ssid_len = hapd->conf->ssid.ssid_len;
	os_memcpy(wps->ssid, hapd->conf->ssid.ssid, wps->ssid_len);
	wps->ap = 1;
	os_memcpy(wps->dev.mac_addr, hapd->own_addr, ETH_ALEN);
	wps->dev.device_name = hapd->conf->device_name ?
		os_strdup(hapd->conf->device_name) : NULL;
	wps->dev.manufacturer = hapd->conf->manufacturer ?
		os_strdup(hapd->conf->manufacturer) : NULL;
	wps->dev.model_name = hapd->conf->model_name ?
		os_strdup(hapd->conf->model_name) : NULL;
	wps->dev.model_number = hapd->conf->model_number ?
		os_strdup(hapd->conf->model_number) : NULL;
	wps->dev.serial_number = hapd->conf->serial_number ?
		os_strdup(hapd->conf->serial_number) : NULL;
	if (hapd->conf->config_methods) {
		char *m = hapd->conf->config_methods;
		if (os_strstr(m, "label"))
			wps->config_methods |= WPS_CONFIG_LABEL;
		if (os_strstr(m, "display"))
			wps->config_methods |= WPS_CONFIG_DISPLAY;
		if (os_strstr(m, "push_button"))
			wps->config_methods |= WPS_CONFIG_PUSHBUTTON;
		if (os_strstr(m, "keypad"))
			wps->config_methods |= WPS_CONFIG_KEYPAD;
	}
	if (hapd->conf->device_type) {
		char *pos;
		u8 oui[4];
		/* <categ>-<OUI>-<subcateg> */
		wps->dev.categ = atoi(hapd->conf->device_type);
		pos = os_strchr(hapd->conf->device_type, '-');
		if (pos == NULL) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type");
			os_free(wps);
			return -1;
		}
		pos++;
		if (hexstr2bin(pos, oui, 4)) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type OUI");
			os_free(wps);
			return -1;
		}
		wps->dev.oui = WPA_GET_BE32(oui);
		pos = os_strchr(pos, '-');
		if (pos == NULL) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type");
			os_free(wps);
			return -1;
		}
		pos++;
		wps->dev.sub_categ = atoi(pos);
	}
	wps->dev.os_version = WPA_GET_BE32(hapd->conf->os_version);
	wps->dev.rf_bands = hapd->iconf->hw_mode == HOSTAPD_MODE_IEEE80211A ?
		WPS_RF_50GHZ : WPS_RF_24GHZ; /* FIX: dualband AP */

	if (conf->wpa & WPA_PROTO_RSN) {
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK)
			wps->auth_types |= WPS_AUTH_WPA2PSK;
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			wps->auth_types |= WPS_AUTH_WPA2;

		if (conf->rsn_pairwise & WPA_CIPHER_CCMP)
			wps->encr_types |= WPS_ENCR_AES;
		if (conf->rsn_pairwise & WPA_CIPHER_TKIP)
			wps->encr_types |= WPS_ENCR_TKIP;
	}

	if (conf->wpa & WPA_PROTO_WPA) {
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK)
			wps->auth_types |= WPS_AUTH_WPAPSK;
		if (conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X)
			wps->auth_types |= WPS_AUTH_WPA;

		if (conf->wpa_pairwise & WPA_CIPHER_CCMP)
			wps->encr_types |= WPS_ENCR_AES;
		if (conf->wpa_pairwise & WPA_CIPHER_TKIP)
			wps->encr_types |= WPS_ENCR_TKIP;
	}

	if (conf->ssid.security_policy == SECURITY_PLAINTEXT) {
		wps->encr_types |= WPS_ENCR_NONE;
		wps->auth_types |= WPS_AUTH_OPEN;
	} else if (conf->ssid.security_policy == SECURITY_STATIC_WEP) {
		wps->encr_types |= WPS_ENCR_WEP;
		if (conf->auth_algs & WPA_AUTH_ALG_OPEN)
			wps->auth_types |= WPS_AUTH_OPEN;
		if (conf->auth_algs & WPA_AUTH_ALG_SHARED)
			wps->auth_types |= WPS_AUTH_SHARED;
	} else if (conf->ssid.security_policy == SECURITY_IEEE_802_1X) {
		wps->auth_types |= WPS_AUTH_OPEN;
		if (conf->default_wep_key_len)
			wps->encr_types |= WPS_ENCR_WEP;
		else
			wps->encr_types |= WPS_ENCR_NONE;
	}

	if (conf->ssid.wpa_psk_file) {
		/* Use per-device PSKs */
	} else if (conf->ssid.wpa_passphrase) {
		wps->network_key = (u8 *) os_strdup(conf->ssid.wpa_passphrase);
		wps->network_key_len = os_strlen(conf->ssid.wpa_passphrase);
	} else if (conf->ssid.wpa_psk) {
		wps->network_key = os_malloc(2 * PMK_LEN + 1);
		if (wps->network_key == NULL) {
			os_free(wps);
			return -1;
		}
		wpa_snprintf_hex((char *) wps->network_key, 2 * PMK_LEN + 1,
				 conf->ssid.wpa_psk->psk, PMK_LEN);
		wps->network_key_len = 2 * PMK_LEN;
	} else if (conf->ssid.wep.keys_set && conf->ssid.wep.key[0]) {
		wps->network_key = os_malloc(conf->ssid.wep.len[0]);
		if (wps->network_key == NULL) {
			os_free(wps);
			return -1;
		}
		os_memcpy(wps->network_key, conf->ssid.wep.key[0],
			  conf->ssid.wep.len[0]);
		wps->network_key_len = conf->ssid.wep.len[0];
	}

	if (conf->wps_state == WPS_STATE_NOT_CONFIGURED) {
		/* Override parameters to enable security by default */
		wps->auth_types = WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK;
		wps->encr_types = WPS_ENCR_AES | WPS_ENCR_TKIP;
	}

	wps->ap_settings = conf->ap_settings;
	wps->ap_settings_len = conf->ap_settings_len;

	cfg.new_psk_cb = hostapd_wps_new_psk_cb;
	cfg.set_ie_cb = hostapd_wps_set_ie_cb;
	cfg.pin_needed_cb = hostapd_wps_pin_needed_cb;
	cfg.reg_success_cb = hostapd_wps_reg_success_cb;
	cfg.cb_ctx = hapd;
	cfg.skip_cred_build = conf->skip_cred_build;
	cfg.extra_cred = conf->extra_cred;
	cfg.extra_cred_len = conf->extra_cred_len;
	cfg.disable_auto_conf = (hapd->conf->wps_cred_processing == 1) &&
		conf->skip_cred_build;
	if (conf->ssid.security_policy == SECURITY_STATIC_WEP)
		cfg.static_wep_only = 1;

	wps->registrar = wps_registrar_init(wps, &cfg);
	if (wps->registrar == NULL) {
		printf("Failed to initialize WPS Registrar\n");
		os_free(wps->network_key);
		os_free(wps);
		return -1;
	}

#ifdef CONFIG_WPS_UPNP
	wps->friendly_name = hapd->conf->friendly_name;
	wps->manufacturer_url = hapd->conf->manufacturer_url;
	wps->model_description = hapd->conf->model_description;
	wps->model_url = hapd->conf->model_url;
	wps->upc = hapd->conf->upc;

	if (hostapd_wps_upnp_init(hapd, wps) < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize WPS UPnP");
		wps_registrar_deinit(wps->registrar);
		os_free(wps->network_key);
		os_free(wps);
		return -1;
	}
#endif /* CONFIG_WPS_UPNP */

	hapd->wps = wps;

	return 0;
}


void hostapd_deinit_wps(struct hostapd_data *hapd)
{
	if (hapd->wps == NULL)
		return;
#ifdef CONFIG_WPS_UPNP
	hostapd_wps_upnp_deinit(hapd);
#endif /* CONFIG_WPS_UPNP */
	wps_registrar_deinit(hapd->wps->registrar);
	os_free(hapd->wps->network_key);
	wps_device_data_free(&hapd->wps->dev);
	wps_free_pending_msgs(hapd->wps->upnp_msgs);
	os_free(hapd->wps);
	hapd->wps = NULL;
	hostapd_wps_clear_ies(hapd);
}


int hostapd_wps_add_pin(struct hostapd_data *hapd, const char *uuid,
			const char *pin, int timeout)
{
	u8 u[UUID_LEN];
	int any = 0;

	if (hapd->wps == NULL)
		return -1;
	if (os_strcmp(uuid, "any") == 0)
		any = 1;
	else if (uuid_str2bin(uuid, u))
		return -1;
	return wps_registrar_add_pin(hapd->wps->registrar, any ? NULL : u,
				     (const u8 *) pin, os_strlen(pin),
				     timeout);
}


int hostapd_wps_button_pushed(struct hostapd_data *hapd)
{
	if (hapd->wps == NULL)
		return -1;
	return wps_registrar_button_pushed(hapd->wps->registrar);
}


void hostapd_wps_probe_req_rx(struct hostapd_data *hapd, const u8 *addr,
			      const u8 *ie, size_t ie_len)
{
	struct wpabuf *wps_ie;
	const u8 *end, *pos, *wps;

	if (hapd->wps == NULL)
		return;

	pos = ie;
	end = ie + ie_len;
	wps = NULL;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			return;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    WPA_GET_BE32(&pos[2]) == WPS_DEV_OUI_WFA) {
			wps = pos;
			break;
		}
		pos += 2 + pos[1];
	}

	if (wps == NULL)
		return; /* No WPS IE in Probe Request */

	wps_ie = wpabuf_alloc(ie_len);
	if (wps_ie == NULL)
		return;

	/* There may be multiple WPS IEs in the message, so need to concatenate
	 * their WPS Data fields */
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    WPA_GET_BE32(&pos[2]) == WPS_DEV_OUI_WFA)
			wpabuf_put_data(wps_ie, pos + 6, pos[1] - 4);
		pos += 2 + pos[1];
	}

	if (wpabuf_len(wps_ie) > 0) {
		wps_registrar_probe_req_rx(hapd->wps->registrar, addr, wps_ie);
#ifdef CONFIG_WPS_UPNP
		/* FIX: what exactly should be included in the WLANEvent?
		 * WPS attributes? Full ProbeReq frame? */
		upnp_wps_device_send_wlan_event(hapd->wps_upnp, addr,
						UPNP_WPS_WLANEVENT_TYPE_PROBE,
						wps_ie);
#endif /* CONFIG_WPS_UPNP */
	}

	wpabuf_free(wps_ie);
}


#ifdef CONFIG_WPS_UPNP

static struct wpabuf *
hostapd_rx_req_get_device_info(void *priv, struct upnp_wps_peer *peer)
{
	struct hostapd_data *hapd = priv;
	struct wps_config cfg;
	struct wps_data *wps;
	enum wsc_op_code op_code;
	struct wpabuf *m1;

	/*
	 * Request for DeviceInfo, i.e., M1 TLVs. This is a start of WPS
	 * registration over UPnP with the AP acting as an Enrollee. It should
	 * be noted that this is frequently used just to get the device data,
	 * i.e., there may not be any intent to actually complete the
	 * registration.
	 */

	if (peer->wps)
		wps_deinit(peer->wps);

	os_memset(&cfg, 0, sizeof(cfg));
	cfg.wps = hapd->wps;
	cfg.pin = (u8 *) hapd->conf->ap_pin;
	cfg.pin_len = os_strlen(hapd->conf->ap_pin);
	wps = wps_init(&cfg);
	if (wps == NULL)
		return NULL;

	m1 = wps_get_msg(wps, &op_code);
	if (m1 == NULL) {
		wps_deinit(wps);
		return NULL;
	}

	peer->wps = wps;

	return m1;
}


static struct wpabuf *
hostapd_rx_req_put_message(void *priv, struct upnp_wps_peer *peer,
			   const struct wpabuf *msg)
{
	enum wps_process_res res;
	enum wsc_op_code op_code;

	/* PutMessage: msg = InMessage, return OutMessage */
	res = wps_process_msg(peer->wps, WSC_UPnP, msg);
	if (res == WPS_FAILURE)
		return NULL;
	return wps_get_msg(peer->wps, &op_code);
}


static struct wpabuf *
hostapd_rx_req_get_ap_settings(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return NULL;
}


static int hostapd_rx_req_set_ap_settings(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_del_ap_settings(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static struct wpabuf *
hostapd_rx_req_get_sta_settings(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return NULL;
}


static int hostapd_rx_req_set_sta_settings(void *priv,
					   const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_del_sta_settings(void *priv,
					   const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_put_wlan_response(
	void *priv, enum upnp_wps_wlanevent_type ev_type,
	const u8 *mac_addr, const struct wpabuf *msg,
	enum wps_msg_type msg_type)
{
	struct hostapd_data *hapd = priv;
	struct sta_info *sta;
	struct upnp_pending_message *p;

	wpa_printf(MSG_DEBUG, "WPS UPnP: PutWLANResponse ev_type=%d mac_addr="
		   MACSTR, ev_type, MAC2STR(mac_addr));
	wpa_hexdump(MSG_MSGDUMP, "WPS UPnP: PutWLANResponse NewMessage",
		    wpabuf_head(msg), wpabuf_len(msg));
	if (ev_type != UPNP_WPS_WLANEVENT_TYPE_EAP) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: Ignored unexpected "
			   "PutWLANResponse WLANEventType %d", ev_type);
		return -1;
	}

	/*
	 * EAP response to ongoing to WPS Registration. Send it to EAP-WSC
	 * server implementation for delivery to the peer.
	 */

	sta = ap_get_sta(hapd, mac_addr);
	if (!sta) {
		/*
		 * Workaround - Intel wsccmd uses bogus NewWLANEventMAC:
		 * Pick STA that is in an ongoing WPS registration without
		 * checking the MAC address.
		 */
		wpa_printf(MSG_DEBUG, "WPS UPnP: No matching STA found based "
			   "on NewWLANEventMAC; try wildcard match");
		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (sta->eapol_sm && (sta->flags & WLAN_STA_WPS))
				break;
		}
	}

	if (!sta) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: No matching STA found");
		return 0;
	}

	p = os_zalloc(sizeof(*p));
	if (p == NULL)
		return -1;
	os_memcpy(p->addr, sta->addr, ETH_ALEN);
	p->msg = wpabuf_dup(msg);
	p->type = msg_type;
	p->next = hapd->wps->upnp_msgs;
	hapd->wps->upnp_msgs = p;

	return eapol_auth_eap_pending_cb(sta->eapol_sm, sta->eapol_sm->eap);
}


static int hostapd_rx_req_set_selected_registrar(void *priv,
						 const struct wpabuf *msg)
{
	struct hostapd_data *hapd = priv;
	return wps_registrar_set_selected_registrar(hapd->wps->registrar, msg);
}


static int hostapd_rx_req_reboot_ap(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_reset_ap(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_reboot_sta(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_rx_req_reset_sta(void *priv, const struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS UPnP: TODO %s", __func__);
	return -1;
}


static int hostapd_wps_upnp_init(struct hostapd_data *hapd,
				 struct wps_context *wps)
{
	struct upnp_wps_device_ctx *ctx;

	if (!hapd->conf->upnp_iface)
		return 0;
	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
		return -1;

	ctx->rx_req_get_device_info = hostapd_rx_req_get_device_info;
	ctx->rx_req_put_message = hostapd_rx_req_put_message;
	ctx->rx_req_get_ap_settings = hostapd_rx_req_get_ap_settings;
	ctx->rx_req_set_ap_settings = hostapd_rx_req_set_ap_settings;
	ctx->rx_req_del_ap_settings = hostapd_rx_req_del_ap_settings;
	ctx->rx_req_get_sta_settings = hostapd_rx_req_get_sta_settings;
	ctx->rx_req_set_sta_settings = hostapd_rx_req_set_sta_settings;
	ctx->rx_req_del_sta_settings = hostapd_rx_req_del_sta_settings;
	ctx->rx_req_put_wlan_response = hostapd_rx_req_put_wlan_response;
	ctx->rx_req_set_selected_registrar =
		hostapd_rx_req_set_selected_registrar;
	ctx->rx_req_reboot_ap = hostapd_rx_req_reboot_ap;
	ctx->rx_req_reset_ap = hostapd_rx_req_reset_ap;
	ctx->rx_req_reboot_sta = hostapd_rx_req_reboot_sta;
	ctx->rx_req_reset_sta = hostapd_rx_req_reset_sta;

	hapd->wps_upnp = upnp_wps_device_init(ctx, wps, hapd);
	if (hapd->wps_upnp == NULL) {
		os_free(ctx);
		return -1;
	}
	wps->wps_upnp = hapd->wps_upnp;

	if (upnp_wps_device_start(hapd->wps_upnp, hapd->conf->upnp_iface)) {
		upnp_wps_device_deinit(hapd->wps_upnp);
		hapd->wps_upnp = NULL;
		return -1;
	}

	return 0;
}


static void hostapd_wps_upnp_deinit(struct hostapd_data *hapd)
{
	upnp_wps_device_deinit(hapd->wps_upnp);
}

#endif /* CONFIG_WPS_UPNP */
