/*
 * WPA Supplicant / Configuration backend: Windows registry
 * Copyright (c) 2003-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file implements a configuration backend for Windows registry. All the
 * configuration information is stored in the registry and the format for
 * network configuration fields is same as described in the sample
 * configuration file, wpa_supplicant.conf.
 *
 * Configuration data is in
 * \a HKEY_LOCAL_MACHINE\\SOFTWARE\\%wpa_supplicant\\configs
 * key. Each configuration profile has its own key under this. In terms of text
 * files, each profile would map to a separate text file with possibly multiple
 * networks. Under each profile, there is a networks key that lists all
 * networks as a subkey. Each network has set of values in the same way as
 * network block in the configuration file. In addition, blobs subkey has
 * possible blobs as values.
 *
 * Example network configuration block:
 * \verbatim
HKEY_LOCAL_MACHINE\SOFTWARE\wpa_supplicant\configs\test\networks\0000
   ssid="example"
   key_mgmt=WPA-PSK
\endverbatim
 */

#include "includes.h"

#include "common.h"
#include "uuid.h"
#include "config.h"

#ifndef WPA_KEY_ROOT
#define WPA_KEY_ROOT HKEY_LOCAL_MACHINE
#endif
#ifndef WPA_KEY_PREFIX
#define WPA_KEY_PREFIX TEXT("SOFTWARE\\wpa_supplicant")
#endif

#ifdef UNICODE
#define TSTR "%S"
#else /* UNICODE */
#define TSTR "%s"
#endif /* UNICODE */


static int wpa_config_read_blobs(struct wpa_config *config, HKEY hk)
{
	struct wpa_config_blob *blob;
	int errors = 0;
	HKEY bhk;
	LONG ret;
	DWORD i;

	ret = RegOpenKeyEx(hk, TEXT("blobs"), 0, KEY_QUERY_VALUE, &bhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "Could not open wpa_supplicant config "
			   "blobs key");
		return 0; /* assume no blobs */
	}

	for (i = 0; ; i++) {
#define TNAMELEN 255
		TCHAR name[TNAMELEN];
		char data[4096];
		DWORD namelen, datalen, type;

		namelen = TNAMELEN;
		datalen = sizeof(data);
		ret = RegEnumValue(bhk, i, name, &namelen, NULL, &type,
				   (LPBYTE) data, &datalen);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumValue failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= TNAMELEN)
			namelen = TNAMELEN - 1;
		name[namelen] = TEXT('\0');
		wpa_unicode2ascii_inplace(name);

		if (datalen >= sizeof(data))
			datalen = sizeof(data) - 1;

		wpa_printf(MSG_MSGDUMP, "blob %d: field='%s' len %d",
			   (int) i, name, (int) datalen);

		blob = os_zalloc(sizeof(*blob));
		if (blob == NULL) {
			errors++;
			break;
		}
		blob->name = os_strdup((char *) name);
		blob->data = os_memdup(data, datalen);
		if (blob->name == NULL || blob->data == NULL) {
			wpa_config_free_blob(blob);
			errors++;
			break;
		}
		blob->len = datalen;

		wpa_config_set_blob(config, blob);
	}

	RegCloseKey(bhk);

	return errors ? -1 : 0;
}


static int wpa_config_read_reg_dword(HKEY hk, const TCHAR *name, int *_val)
{
	DWORD val, buflen;
	LONG ret;

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val)) {
		wpa_printf(MSG_DEBUG, TSTR "=%d", name, (int) val);
		*_val = val;
		return 0;
	}

	return -1;
}


static char * wpa_config_read_reg_string(HKEY hk, const TCHAR *name)
{
	DWORD buflen;
	LONG ret;
	TCHAR *val;

	buflen = 0;
	ret = RegQueryValueEx(hk, name, NULL, NULL, NULL, &buflen);
	if (ret != ERROR_SUCCESS)
		return NULL;
	val = os_malloc(buflen);
	if (val == NULL)
		return NULL;

	ret = RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE) val, &buflen);
	if (ret != ERROR_SUCCESS) {
		os_free(val);
		return NULL;
	}

	wpa_unicode2ascii_inplace(val);
	wpa_printf(MSG_DEBUG, TSTR "=%s", name, (char *) val);
	return (char *) val;
}


#ifdef CONFIG_WPS
static int wpa_config_read_global_uuid(struct wpa_config *config, HKEY hk)
{
	char *str;
	int ret = 0;

	str = wpa_config_read_reg_string(hk, TEXT("uuid"));
	if (str == NULL)
		return 0;

	if (uuid_str2bin(str, config->uuid))
		ret = -1;

	os_free(str);

	return ret;
}


static int wpa_config_read_global_os_version(struct wpa_config *config,
					     HKEY hk)
{
	char *str;
	int ret = 0;

	str = wpa_config_read_reg_string(hk, TEXT("os_version"));
	if (str == NULL)
		return 0;

	if (hexstr2bin(str, config->os_version, 4))
		ret = -1;

	os_free(str);

	return ret;
}
#endif /* CONFIG_WPS */


static int wpa_config_read_global(struct wpa_config *config, HKEY hk)
{
	int errors = 0;
	int val;

	wpa_config_read_reg_dword(hk, TEXT("ap_scan"), &config->ap_scan);
	wpa_config_read_reg_dword(hk, TEXT("fast_reauth"),
				  &config->fast_reauth);
	wpa_config_read_reg_dword(hk, TEXT("dot11RSNAConfigPMKLifetime"),
				  (int *) &config->dot11RSNAConfigPMKLifetime);
	wpa_config_read_reg_dword(hk,
				  TEXT("dot11RSNAConfigPMKReauthThreshold"),
				  (int *)
				  &config->dot11RSNAConfigPMKReauthThreshold);
	wpa_config_read_reg_dword(hk, TEXT("dot11RSNAConfigSATimeout"),
				  (int *) &config->dot11RSNAConfigSATimeout);
	wpa_config_read_reg_dword(hk, TEXT("update_config"),
				  &config->update_config);

	if (wpa_config_read_reg_dword(hk, TEXT("eapol_version"),
				      &config->eapol_version) == 0) {
		if (config->eapol_version < 1 ||
		    config->eapol_version > 2) {
			wpa_printf(MSG_ERROR, "Invalid EAPOL version (%d)",
				   config->eapol_version);
			errors++;
		}
	}

	config->ctrl_interface = wpa_config_read_reg_string(
		hk, TEXT("ctrl_interface"));

#ifdef CONFIG_WPS
	if (wpa_config_read_global_uuid(config, hk))
		errors++;
	wpa_config_read_reg_dword(hk, TEXT("auto_uuid"), &config->auto_uuid);
	config->device_name = wpa_config_read_reg_string(
		hk, TEXT("device_name"));
	config->manufacturer = wpa_config_read_reg_string(
		hk, TEXT("manufacturer"));
	config->model_name = wpa_config_read_reg_string(
		hk, TEXT("model_name"));
	config->serial_number = wpa_config_read_reg_string(
		hk, TEXT("serial_number"));
	{
		char *t = wpa_config_read_reg_string(
			hk, TEXT("device_type"));
		if (t && wps_dev_type_str2bin(t, config->device_type))
			errors++;
		os_free(t);
	}
	config->config_methods = wpa_config_read_reg_string(
		hk, TEXT("config_methods"));
	if (wpa_config_read_global_os_version(config, hk))
		errors++;
	wpa_config_read_reg_dword(hk, TEXT("wps_cred_processing"),
				  &config->wps_cred_processing);
	wpa_config_read_reg_dword(hk, TEXT("wps_cred_add_sae"),
				  &config->wps_cred_add_sae);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	config->p2p_ssid_postfix = wpa_config_read_reg_string(
		hk, TEXT("p2p_ssid_postfix"));
	wpa_config_read_reg_dword(hk, TEXT("p2p_group_idle"),
				  (int *) &config->p2p_group_idle);
#endif /* CONFIG_P2P */

	wpa_config_read_reg_dword(hk, TEXT("bss_max_count"),
				  (int *) &config->bss_max_count);
	wpa_config_read_reg_dword(hk, TEXT("filter_ssids"),
				  &config->filter_ssids);
	wpa_config_read_reg_dword(hk, TEXT("max_num_sta"),
				  (int *) &config->max_num_sta);
	wpa_config_read_reg_dword(hk, TEXT("disassoc_low_ack"),
				  (int *) &config->disassoc_low_ack);

	wpa_config_read_reg_dword(hk, TEXT("okc"), &config->okc);
	wpa_config_read_reg_dword(hk, TEXT("pmf"), &val);
	config->pmf = val;
	if (wpa_config_read_reg_dword(hk, TEXT("extended_key_id"),
				      &val) == 0) {
		if (val < 0 || val > 1) {
			wpa_printf(MSG_ERROR,
				   "Invalid Extended Key ID setting (%d)", val);
			errors++;
		}
		config->extended_key_id = val;
	}

	return errors ? -1 : 0;
}


static struct wpa_ssid * wpa_config_read_network(HKEY hk, const TCHAR *netw,
						 int id)
{
	HKEY nhk;
	LONG ret;
	DWORD i;
	struct wpa_ssid *ssid;
	int errors = 0;

	ret = RegOpenKeyEx(hk, netw, 0, KEY_QUERY_VALUE, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "Could not open wpa_supplicant config "
			   "network '" TSTR "'", netw);
		return NULL;
	}

	wpa_printf(MSG_MSGDUMP, "Start of a new network '" TSTR "'", netw);
	ssid = os_zalloc(sizeof(*ssid));
	if (ssid == NULL) {
		RegCloseKey(nhk);
		return NULL;
	}
	dl_list_init(&ssid->psk_list);
	ssid->id = id;

	wpa_config_set_network_defaults(ssid);

	for (i = 0; ; i++) {
		TCHAR name[255], data[1024];
		DWORD namelen, datalen, type;

		namelen = 255;
		datalen = sizeof(data);
		ret = RegEnumValue(nhk, i, name, &namelen, NULL, &type,
				   (LPBYTE) data, &datalen);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_ERROR, "RegEnumValue failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = TEXT('\0');

		if (datalen >= 1024)
			datalen = 1024 - 1;
		data[datalen] = TEXT('\0');

		wpa_unicode2ascii_inplace(name);
		wpa_unicode2ascii_inplace(data);
		if (wpa_config_set(ssid, (char *) name, (char *) data, 0) < 0)
			errors++;
	}

	RegCloseKey(nhk);

	if (ssid->passphrase) {
		if (ssid->psk_set) {
			wpa_printf(MSG_ERROR, "Both PSK and passphrase "
				   "configured for network '" TSTR "'.", netw);
			errors++;
		}
		wpa_config_update_psk(ssid);
	}

	if ((ssid->group_cipher & WPA_CIPHER_CCMP) &&
	    !(ssid->pairwise_cipher & WPA_CIPHER_CCMP) &&
	    !(ssid->pairwise_cipher & WPA_CIPHER_NONE)) {
		/* Group cipher cannot be stronger than the pairwise cipher. */
		wpa_printf(MSG_DEBUG, "Removed CCMP from group cipher "
			   "list since it was not allowed for pairwise "
			   "cipher for network '" TSTR "'.", netw);
		ssid->group_cipher &= ~WPA_CIPHER_CCMP;
	}

	if (errors) {
		wpa_config_free_ssid(ssid);
		ssid = NULL;
	}

	return ssid;
}


static int wpa_config_read_networks(struct wpa_config *config, HKEY hk)
{
	HKEY nhk;
	struct wpa_ssid *ssid, *tail = NULL, *head = NULL;
	int errors = 0;
	LONG ret;
	DWORD i;

	ret = RegOpenKeyEx(hk, TEXT("networks"), 0, KEY_ENUMERATE_SUB_KEYS,
			   &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant networks "
			   "registry key");
		return -1;
	}

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(nhk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumKeyEx failed: 0x%x",
				   (unsigned int) ret);
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

		ssid = wpa_config_read_network(nhk, name, i);
		if (ssid == NULL) {
			wpa_printf(MSG_ERROR, "Failed to parse network "
				   "profile '%s'.", name);
			errors++;
			continue;
		}
		if (head == NULL) {
			head = tail = ssid;
		} else {
			tail->next = ssid;
			tail = ssid;
		}
		if (wpa_config_add_prio_network(config, ssid)) {
			wpa_printf(MSG_ERROR, "Failed to add network profile "
				   "'%s' to priority list.", name);
			errors++;
			continue;
		}
	}

	RegCloseKey(nhk);

	config->ssid = head;

	return errors ? -1 : 0;
}


struct wpa_config * wpa_config_read(const char *name, struct wpa_config *cfgp)
{
	TCHAR buf[256];
	int errors = 0;
	struct wpa_config *config;
	HKEY hk;
	LONG ret;

	if (name == NULL)
		return NULL;
	if (cfgp)
		config = cfgp;
	else
		config = wpa_config_alloc_empty(NULL, NULL);
	if (config == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "Reading configuration profile '%s'", name);

#ifdef UNICODE
	_snwprintf(buf, 256, WPA_KEY_PREFIX TEXT("\\configs\\%S"), name);
#else /* UNICODE */
	os_snprintf(buf, 256, WPA_KEY_PREFIX TEXT("\\configs\\%s"), name);
#endif /* UNICODE */

	ret = RegOpenKeyEx(WPA_KEY_ROOT, buf, 0, KEY_QUERY_VALUE, &hk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant "
			   "configuration registry HKLM\\" TSTR, buf);
		os_free(config);
		return NULL;
	}

	if (wpa_config_read_global(config, hk))
		errors++;

	if (wpa_config_read_networks(config, hk))
		errors++;

	if (wpa_config_read_blobs(config, hk))
		errors++;

	wpa_config_debug_dump_networks(config);

	RegCloseKey(hk);

	if (errors) {
		wpa_config_free(config);
		config = NULL;
	}

	return config;
}


static int wpa_config_write_reg_dword(HKEY hk, const TCHAR *name, int val,
				      int def)
{
	LONG ret;
	DWORD _val = val;

	if (val == def) {
		RegDeleteValue(hk, name);
		return 0;
	}

	ret = RegSetValueEx(hk, name, 0, REG_DWORD, (LPBYTE) &_val,
			    sizeof(_val));
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set %s=%d: error %d",
			   name, val, (int) GetLastError());
		return -1;
	}

	return 0;
}


static int wpa_config_write_reg_string(HKEY hk, const char *name,
				       const char *val)
{
	LONG ret;
	TCHAR *_name, *_val;

	_name = wpa_strdup_tchar(name);
	if (_name == NULL)
		return -1;

	if (val == NULL) {
		RegDeleteValue(hk, _name);
		os_free(_name);
		return 0;
	}

	_val = wpa_strdup_tchar(val);
	if (_val == NULL) {
		os_free(_name);
		return -1;
	}
	ret = RegSetValueEx(hk, _name, 0, REG_SZ, (BYTE *) _val,
			    (os_strlen(val) + 1) * sizeof(TCHAR));
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set %s='%s': "
			   "error %d", name, val, (int) GetLastError());
		os_free(_name);
		os_free(_val);
		return -1;
	}

	os_free(_name);
	os_free(_val);
	return 0;
}


static int wpa_config_write_global(struct wpa_config *config, HKEY hk)
{
#ifdef CONFIG_CTRL_IFACE
	wpa_config_write_reg_string(hk, "ctrl_interface",
				    config->ctrl_interface);
#endif /* CONFIG_CTRL_IFACE */

	wpa_config_write_reg_dword(hk, TEXT("eapol_version"),
				   config->eapol_version,
				   DEFAULT_EAPOL_VERSION);
	wpa_config_write_reg_dword(hk, TEXT("ap_scan"), config->ap_scan,
				   DEFAULT_AP_SCAN);
	wpa_config_write_reg_dword(hk, TEXT("fast_reauth"),
				   config->fast_reauth, DEFAULT_FAST_REAUTH);
	wpa_config_write_reg_dword(hk, TEXT("dot11RSNAConfigPMKLifetime"),
				   config->dot11RSNAConfigPMKLifetime, 0);
	wpa_config_write_reg_dword(hk,
				   TEXT("dot11RSNAConfigPMKReauthThreshold"),
				   config->dot11RSNAConfigPMKReauthThreshold,
				   0);
	wpa_config_write_reg_dword(hk, TEXT("dot11RSNAConfigSATimeout"),
				   config->dot11RSNAConfigSATimeout, 0);
	wpa_config_write_reg_dword(hk, TEXT("update_config"),
				   config->update_config,
				   0);
#ifdef CONFIG_WPS
	if (!is_nil_uuid(config->uuid)) {
		char buf[40];
		uuid_bin2str(config->uuid, buf, sizeof(buf));
		wpa_config_write_reg_string(hk, "uuid", buf);
	}
	wpa_config_write_reg_dword(hk, TEXT("auto_uuid"), config->auto_uuid,
				   0);
	wpa_config_write_reg_string(hk, "device_name", config->device_name);
	wpa_config_write_reg_string(hk, "manufacturer", config->manufacturer);
	wpa_config_write_reg_string(hk, "model_name", config->model_name);
	wpa_config_write_reg_string(hk, "model_number", config->model_number);
	wpa_config_write_reg_string(hk, "serial_number",
				    config->serial_number);
	{
		char _buf[WPS_DEV_TYPE_BUFSIZE], *buf;
		buf = wps_dev_type_bin2str(config->device_type,
					   _buf, sizeof(_buf));
		wpa_config_write_reg_string(hk, "device_type", buf);
	}
	wpa_config_write_reg_string(hk, "config_methods",
				    config->config_methods);
	if (WPA_GET_BE32(config->os_version)) {
		char vbuf[10];
		os_snprintf(vbuf, sizeof(vbuf), "%08x",
			    WPA_GET_BE32(config->os_version));
		wpa_config_write_reg_string(hk, "os_version", vbuf);
	}
	wpa_config_write_reg_dword(hk, TEXT("wps_cred_processing"),
				   config->wps_cred_processing, 0);
	wpa_config_write_reg_dword(hk, TEXT("wps_cred_add_sae"),
				   config->wps_cred_add_sae, 0);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	wpa_config_write_reg_string(hk, "p2p_ssid_postfix",
				    config->p2p_ssid_postfix);
	wpa_config_write_reg_dword(hk, TEXT("p2p_group_idle"),
				   config->p2p_group_idle, 0);
#endif /* CONFIG_P2P */

	wpa_config_write_reg_dword(hk, TEXT("bss_max_count"),
				   config->bss_max_count,
				   DEFAULT_BSS_MAX_COUNT);
	wpa_config_write_reg_dword(hk, TEXT("filter_ssids"),
				   config->filter_ssids, 0);
	wpa_config_write_reg_dword(hk, TEXT("max_num_sta"),
				   config->max_num_sta, DEFAULT_MAX_NUM_STA);
	wpa_config_write_reg_dword(hk, TEXT("ap_isolate"),
				   config->ap_isolate, DEFAULT_AP_ISOLATE);
	wpa_config_write_reg_dword(hk, TEXT("disassoc_low_ack"),
				   config->disassoc_low_ack, 0);

	wpa_config_write_reg_dword(hk, TEXT("okc"), config->okc, 0);
	wpa_config_write_reg_dword(hk, TEXT("pmf"), config->pmf, 0);

	wpa_config_write_reg_dword(hk, TEXT("external_sim"),
				   config->external_sim, 0);

	return 0;
}


static int wpa_config_delete_subkeys(HKEY hk, const TCHAR *key)
{
	HKEY nhk;
	int i, errors = 0;
	LONG ret;

	ret = RegOpenKeyEx(hk, key, 0, KEY_ENUMERATE_SUB_KEYS | DELETE, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not open key '" TSTR
			   "' for subkey deletion: error 0x%x (%d)", key,
			   (unsigned int) ret, (int) GetLastError());
		return 0;
	}

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(nhk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegEnumKeyEx failed: 0x%x (%d)",
				   (unsigned int) ret, (int) GetLastError());
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = TEXT('\0');

		ret = RegDeleteKey(nhk, name);
		if (ret != ERROR_SUCCESS) {
			wpa_printf(MSG_DEBUG, "RegDeleteKey failed: 0x%x (%d)",
				   (unsigned int) ret, (int) GetLastError());
			errors++;
		}
	}

	RegCloseKey(nhk);

	return errors ? -1 : 0;
}


static void write_str(HKEY hk, const char *field, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, field);
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, field, value);
	os_free(value);
}


static void write_int(HKEY hk, const char *field, int value, int def)
{
	char val[20];
	if (value == def)
		return;
	os_snprintf(val, sizeof(val), "%d", value);
	wpa_config_write_reg_string(hk, field, val);
}


static void write_bssid(HKEY hk, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "bssid");
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, "bssid", value);
	os_free(value);
}


static void write_psk(HKEY hk, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "psk");
	if (value == NULL)
		return;
	wpa_config_write_reg_string(hk, "psk", value);
	os_free(value);
}


static void write_proto(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->proto == DEFAULT_PROTO)
		return;

	value = wpa_config_get(ssid, "proto");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "proto", value);
	os_free(value);
}


static void write_key_mgmt(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->key_mgmt == DEFAULT_KEY_MGMT)
		return;

	value = wpa_config_get(ssid, "key_mgmt");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "key_mgmt", value);
	os_free(value);
}


static void write_pairwise(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->pairwise_cipher == DEFAULT_PAIRWISE)
		return;

	value = wpa_config_get(ssid, "pairwise");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "pairwise", value);
	os_free(value);
}


static void write_group(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->group_cipher == DEFAULT_GROUP)
		return;

	value = wpa_config_get(ssid, "group");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "group", value);
	os_free(value);
}


static void write_auth_alg(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->auth_alg == 0)
		return;

	value = wpa_config_get(ssid, "auth_alg");
	if (value == NULL)
		return;
	if (value[0])
		wpa_config_write_reg_string(hk, "auth_alg", value);
	os_free(value);
}


#ifdef IEEE8021X_EAPOL
static void write_eap(HKEY hk, struct wpa_ssid *ssid)
{
	char *value;

	value = wpa_config_get(ssid, "eap");
	if (value == NULL)
		return;

	if (value[0])
		wpa_config_write_reg_string(hk, "eap", value);
	os_free(value);
}
#endif /* IEEE8021X_EAPOL */


#ifdef CONFIG_WEP
static void write_wep_key(HKEY hk, int idx, struct wpa_ssid *ssid)
{
	char field[20], *value;

	os_snprintf(field, sizeof(field), "wep_key%d", idx);
	value = wpa_config_get(ssid, field);
	if (value) {
		wpa_config_write_reg_string(hk, field, value);
		os_free(value);
	}
}
#endif /* CONFIG_WEP */


static int wpa_config_write_network(HKEY hk, struct wpa_ssid *ssid, int id)
{
	int errors = 0;
	HKEY nhk, netw;
	LONG ret;
	TCHAR name[5];

	ret = RegOpenKeyEx(hk, TEXT("networks"), 0, KEY_CREATE_SUB_KEY, &nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not open networks key "
			   "for subkey addition: error 0x%x (%d)",
			   (unsigned int) ret, (int) GetLastError());
		return 0;
	}

#ifdef UNICODE
	wsprintf(name, L"%04d", id);
#else /* UNICODE */
	os_snprintf(name, sizeof(name), "%04d", id);
#endif /* UNICODE */
	ret = RegCreateKeyEx(nhk, name, 0, NULL, 0, KEY_WRITE, NULL, &netw,
			     NULL);
	RegCloseKey(nhk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not add network key '%s':"
			   " error 0x%x (%d)",
			   name, (unsigned int) ret, (int) GetLastError());
		return -1;
	}

#define STR(t) write_str(netw, #t, ssid)
#define INT(t) write_int(netw, #t, ssid->t, 0)
#define INTe(t, m) write_int(netw, #t, ssid->eap.m, 0)
#define INT_DEF(t, def) write_int(netw, #t, ssid->t, def)
#define INT_DEFe(t, m, def) write_int(netw, #t, ssid->eap.m, def)

	STR(ssid);
	INT(scan_ssid);
	write_bssid(netw, ssid);
	write_psk(netw, ssid);
	STR(sae_password);
	STR(sae_password_id);
	write_proto(netw, ssid);
	write_key_mgmt(netw, ssid);
	write_pairwise(netw, ssid);
	write_group(netw, ssid);
	write_auth_alg(netw, ssid);
#ifdef IEEE8021X_EAPOL
	write_eap(netw, ssid);
	STR(identity);
	STR(anonymous_identity);
	STR(imsi_identity);
	STR(password);
	STR(ca_cert);
	STR(ca_path);
	STR(client_cert);
	STR(private_key);
	STR(private_key_passwd);
	STR(dh_file);
	STR(subject_match);
	STR(check_cert_subject);
	STR(altsubject_match);
	STR(ca_cert2);
	STR(ca_path2);
	STR(client_cert2);
	STR(private_key2);
	STR(private_key2_passwd);
	STR(dh_file2);
	STR(subject_match2);
	STR(check_cert_subject2);
	STR(altsubject_match2);
	STR(phase1);
	STR(phase2);
	STR(pcsc);
	STR(pin);
	STR(engine_id);
	STR(key_id);
	STR(cert_id);
	STR(ca_cert_id);
	STR(key2_id);
	STR(pin2);
	STR(engine2_id);
	STR(cert2_id);
	STR(ca_cert2_id);
	INTe(engine, cert.engine);
	INTe(engine2, phase2_cert.engine);
	INT_DEF(eapol_flags, DEFAULT_EAPOL_FLAGS);
#endif /* IEEE8021X_EAPOL */
#ifdef CONFIG_WEP
	{
		int i;

		for (i = 0; i < 4; i++)
			write_wep_key(netw, i, ssid);
		INT(wep_tx_keyidx);
	}
#endif /* CONFIG_WEP */
	INT(priority);
#ifdef IEEE8021X_EAPOL
	INT_DEF(eap_workaround, DEFAULT_EAP_WORKAROUND);
	STR(pac_file);
	INT_DEFe(fragment_size, fragment_size, DEFAULT_FRAGMENT_SIZE);
#endif /* IEEE8021X_EAPOL */
	INT(mode);
	write_int(netw, "proactive_key_caching", ssid->proactive_key_caching,
		  -1);
	INT(disabled);
	write_int(netw, "ieee80211w", ssid->ieee80211w,
		  MGMT_FRAME_PROTECTION_DEFAULT);
	STR(id_str);
#ifdef CONFIG_HS20
	INT(update_identifier);
#endif /* CONFIG_HS20 */
	INT(group_rekey);
	INT(ft_eap_pmksa_caching);

#undef STR
#undef INT
#undef INT_DEF

	RegCloseKey(netw);

	return errors ? -1 : 0;
}


static int wpa_config_write_blob(HKEY hk, struct wpa_config_blob *blob)
{
	HKEY bhk;
	LONG ret;
	TCHAR *name;

	ret = RegCreateKeyEx(hk, TEXT("blobs"), 0, NULL, 0, KEY_WRITE, NULL,
			     &bhk, NULL);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_DEBUG, "WINREG: Could not add blobs key: "
			   "error 0x%x (%d)",
			   (unsigned int) ret, (int) GetLastError());
		return -1;
	}

	name = wpa_strdup_tchar(blob->name);
	ret = RegSetValueEx(bhk, name, 0, REG_BINARY, blob->data,
			    blob->len);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "WINREG: Failed to set blob %s': "
			   "error 0x%x (%d)", blob->name, (unsigned int) ret,
			   (int) GetLastError());
		RegCloseKey(bhk);
		os_free(name);
		return -1;
	}
	os_free(name);

	RegCloseKey(bhk);

	return 0;
}


int wpa_config_write(const char *name, struct wpa_config *config)
{
	TCHAR buf[256];
	HKEY hk;
	LONG ret;
	int errors = 0;
	struct wpa_ssid *ssid;
	struct wpa_config_blob *blob;
	int id;

	wpa_printf(MSG_DEBUG, "Writing configuration file '%s'", name);

#ifdef UNICODE
	_snwprintf(buf, 256, WPA_KEY_PREFIX TEXT("\\configs\\%S"), name);
#else /* UNICODE */
	os_snprintf(buf, 256, WPA_KEY_PREFIX TEXT("\\configs\\%s"), name);
#endif /* UNICODE */

	ret = RegOpenKeyEx(WPA_KEY_ROOT, buf, 0, KEY_SET_VALUE | DELETE, &hk);
	if (ret != ERROR_SUCCESS) {
		wpa_printf(MSG_ERROR, "Could not open wpa_supplicant "
			   "configuration registry %s: error %d", buf,
			   (int) GetLastError());
		return -1;
	}

	if (wpa_config_write_global(config, hk)) {
		wpa_printf(MSG_ERROR, "Failed to write global configuration "
			   "data");
		errors++;
	}

	wpa_config_delete_subkeys(hk, TEXT("networks"));
	for (ssid = config->ssid, id = 0; ssid; ssid = ssid->next, id++) {
		if (ssid->key_mgmt == WPA_KEY_MGMT_WPS)
			continue; /* do not save temporary WPS networks */
		if (wpa_config_write_network(hk, ssid, id))
			errors++;
	}

	RegDeleteKey(hk, TEXT("blobs"));
	for (blob = config->blobs; blob; blob = blob->next) {
		if (wpa_config_write_blob(hk, blob))
			errors++;
	}

	RegCloseKey(hk);

	wpa_printf(MSG_DEBUG, "Configuration '%s' written %ssuccessfully",
		   name, errors ? "un" : "");
	return errors ? -1 : 0;
}
