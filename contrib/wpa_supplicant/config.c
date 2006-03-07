/*
 * WPA Supplicant / Configuration parser and common functions
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
#include <string.h>

#include "common.h"
#include "wpa.h"
#include "sha1.h"
#include "wpa_supplicant.h"
#include "eapol_sm.h"
#include "eap.h"
#include "l2_packet.h"
#include "config.h"


/*
 * Structure for network configuration parsing. This data is used to implement
 * a generic parser for each network block variable. The table of configuration
 * variables is defined below in this file (ssid_fields[]).
 */
struct parse_data {
	/* Configuration variable name */
	char *name;

	/* Parser function for this variable */
	int (*parser)(const struct parse_data *data, struct wpa_ssid *ssid,
		      int line, const char *value);

	/* Writer function (i.e., to get the variable in text format from
	 * internal presentation). */
	char * (*writer)(const struct parse_data *data, struct wpa_ssid *ssid);

	/* Variable specific parameters for the parser. */
	void *param1, *param2, *param3, *param4;

	/* 0 = this variable can be included in debug output
	 * 1 = this variable contains key/private data and it must not be
	 *     included in debug output unless explicitly requested
	 */
	int key_data;
};


static char * wpa_config_parse_string(const char *value, size_t *len)
{
	if (*value == '"') {
		char *pos;
		value++;
		pos = strchr(value, '"');
		if (pos == NULL || pos[1] != '\0')
			return NULL;
		*pos = '\0';
		*len = strlen(value);
		return strdup(value);
	} else {
		u8 *str;
		size_t hlen = strlen(value);
		if (hlen % 1)
			return NULL;
		*len = hlen / 2;
		str = malloc(*len);
		if (str == NULL)
			return NULL;
		if (hexstr2bin(value, str, *len)) {
			free(str);
			return NULL;
		}
		return (char *) str;
	}
}


static int wpa_config_parse_str(const struct parse_data *data,
				struct wpa_ssid *ssid,
				int line, const char *value)
{
	size_t res_len, *dst_len;
	char **dst;

	dst = (char **) (((u8 *) ssid) + (long) data->param1);
	dst_len = (size_t *) (((u8 *) ssid) + (long) data->param2);

	free(*dst);
	*dst = wpa_config_parse_string(value, &res_len);
	if (*dst == NULL) {
		wpa_printf(MSG_ERROR, "Line %d: failed to parse %s '%s'.",
			   line, data->name, value);
		return -1;
	}
	if (data->param2)
		*dst_len = res_len;

	if (data->key_data) {
		wpa_hexdump_ascii_key(MSG_MSGDUMP, data->name,
				      (u8 *) *dst, res_len);
	} else {
		wpa_hexdump_ascii(MSG_MSGDUMP, data->name,
				  (u8 *) *dst, res_len);
	}

	if (data->param3 && res_len < (size_t) data->param3) {
		wpa_printf(MSG_ERROR, "Line %d: too short %s (len=%lu "
			   "min_len=%ld)", line, data->name,
			   (unsigned long) res_len, (long) data->param3);
		free(*dst);
		*dst = NULL;
		return -1;
	}

	if (data->param4 && res_len > (size_t) data->param4) {
		wpa_printf(MSG_ERROR, "Line %d: too long %s (len=%lu "
			   "max_len=%ld)", line, data->name,
			   (unsigned long) res_len, (long) data->param4);
		free(*dst);
		*dst = NULL;
		return -1;
	}

	return 0;
}


static int is_hex(const u8 *data, size_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (data[i] < 32 || data[i] >= 127)
			return 1;
	}
	return 0;
}


static char * wpa_config_write_string_ascii(const u8 *value, size_t len)
{
	int i;
	char *buf, *pos, *end;

	pos = buf = malloc(len + 3);
	if (buf == NULL)
		return NULL;
	end = buf + len + 3;
	pos += snprintf(pos, end - pos, "\"");
	for (i = 0; i < len; i++)
		pos += snprintf(pos, end - pos, "%c", value[i]);
	pos += snprintf(pos, end - pos, "\"");

	return buf;
}


static char * wpa_config_write_string_hex(const u8 *value, size_t len)
{
	int i;
	char *buf, *pos, *end;

	pos = buf = malloc(2 * len + 1);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 2 * len + 1);
	end = buf + 2 * len + 1;
	for (i = 0; i < len; i++)
		pos += snprintf(pos, end - pos, "%02x", value[i]);

	return buf;
}


static char * wpa_config_write_string(const u8 *value, size_t len)
{
	if (value == NULL)
		return NULL;

	if (is_hex(value, len))
		return wpa_config_write_string_hex(value, len);
	else
		return wpa_config_write_string_ascii(value, len);
}


static char * wpa_config_write_str(const struct parse_data *data,
				   struct wpa_ssid *ssid)
{
	size_t len;
	char **src;

	src = (char **) (((u8 *) ssid) + (long) data->param1);
	if (*src == NULL)
		return NULL;

	if (data->param2)
		len = *((size_t *) (((u8 *) ssid) + (long) data->param2));
	else
		len = strlen(*src);

	return wpa_config_write_string((const u8 *) *src, len);
}


static int wpa_config_parse_int(const struct parse_data *data,
				struct wpa_ssid *ssid,
				int line, const char *value)
{
	int *dst;

	dst = (int *) (((u8 *) ssid) + (long) data->param1);
	*dst = atoi(value);
	wpa_printf(MSG_MSGDUMP, "%s=%d (0x%x)", data->name, *dst, *dst);

	if (data->param3 && *dst < (long) data->param3) {
		wpa_printf(MSG_ERROR, "Line %d: too small %s (value=%d "
			   "min_value=%ld)", line, data->name, *dst,
			   (long) data->param3);
		*dst = (long) data->param3;
		return -1;
	}

	if (data->param4 && *dst > (long) data->param4) {
		wpa_printf(MSG_ERROR, "Line %d: too large %s (value=%d "
			   "max_value=%ld)", line, data->name, *dst,
			   (long) data->param4);
		*dst = (long) data->param4;
		return -1;
	}

	return 0;
}


static char * wpa_config_write_int(const struct parse_data *data,
				   struct wpa_ssid *ssid)
{
	int *src;
	char *value;

	src = (int *) (((u8 *) ssid) + (long) data->param1);

	value = malloc(20);
	if (value == NULL)
		return NULL;
	snprintf(value, 20, "%d", *src);
	return value;
}


static int wpa_config_parse_bssid(const struct parse_data *data,
				  struct wpa_ssid *ssid, int line,
				  const char *value)
{
	if (hwaddr_aton(value, ssid->bssid)) {
		wpa_printf(MSG_ERROR, "Line %d: Invalid BSSID '%s'.",
			   line, value);
		return -1;
	}
	ssid->bssid_set = 1;
	wpa_hexdump(MSG_MSGDUMP, "BSSID", ssid->bssid, ETH_ALEN);
	return 0;
}


static char * wpa_config_write_bssid(const struct parse_data *data,
				     struct wpa_ssid *ssid)
{
	char *value;

	if (!ssid->bssid_set)
		return NULL;

	value = malloc(20);
	if (value == NULL)
		return NULL;
	snprintf(value, 20, MACSTR, MAC2STR(ssid->bssid));
	return value;
}


static int wpa_config_parse_psk(const struct parse_data *data,
				struct wpa_ssid *ssid, int line,
				const char *value)
{
	if (*value == '"') {
		char *pos;
		size_t len;

		value++;
		pos = strrchr(value, '"');
		if (pos)
			*pos = '\0';
		len = strlen(value);
		if (len < 8 || len > 63) {
			wpa_printf(MSG_ERROR, "Line %d: Invalid passphrase "
				   "length %lu (expected: 8..63) '%s'.",
				   line, (unsigned long) len, value);
			return -1;
		}
		wpa_hexdump_ascii_key(MSG_MSGDUMP, "PSK (ASCII passphrase)",
				      (u8 *) value, len);
		ssid->passphrase = strdup(value);
		return ssid->passphrase == NULL ? -1 : 0;
	}

	if (hexstr2bin(value, ssid->psk, PMK_LEN) ||
	    value[PMK_LEN * 2] != '\0') {
		wpa_printf(MSG_ERROR, "Line %d: Invalid PSK '%s'.",
			   line, value);
		return -1;
	}
	ssid->psk_set = 1;
	wpa_hexdump_key(MSG_MSGDUMP, "PSK", ssid->psk, PMK_LEN);
	return 0;
}


static char * wpa_config_write_psk(const struct parse_data *data,
				   struct wpa_ssid *ssid)
{
	if (ssid->passphrase)
		return wpa_config_write_string_ascii(
			(const u8 *) ssid->passphrase,
			strlen(ssid->passphrase));

	if (ssid->psk_set)
		return wpa_config_write_string_hex(ssid->psk, PMK_LEN);

	return NULL;
}


static int wpa_config_parse_proto(const struct parse_data *data,
				  struct wpa_ssid *ssid, int line,
				  const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "WPA") == 0)
			val |= WPA_PROTO_WPA;
		else if (strcmp(start, "RSN") == 0 ||
			 strcmp(start, "WPA2") == 0)
			val |= WPA_PROTO_RSN;
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid proto '%s'",
				   line, start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR,
			   "Line %d: no proto values configured.", line);
		errors++;
	}

	wpa_printf(MSG_MSGDUMP, "proto: 0x%x", val);
	ssid->proto = val;
	return errors ? -1 : 0;
}


static char * wpa_config_write_proto(const struct parse_data *data,
				     struct wpa_ssid *ssid)
{
	int first = 1;
	char *buf, *pos, *end;

	pos = buf = malloc(10);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 10);
	end = buf + 10;

	if (ssid->proto & WPA_PROTO_WPA) {
		pos += snprintf(pos, end - pos, "%sWPA", first ? "" : " ");
		first = 0;
	}

	if (ssid->proto & WPA_PROTO_RSN) {
		pos += snprintf(pos, end - pos, "%sRSN", first ? "" : " ");
		first = 0;
	}

	return buf;
}


static int wpa_config_parse_key_mgmt(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "WPA-PSK") == 0)
			val |= WPA_KEY_MGMT_PSK;
		else if (strcmp(start, "WPA-EAP") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X;
		else if (strcmp(start, "IEEE8021X") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_NO_WPA;
		else if (strcmp(start, "NONE") == 0)
			val |= WPA_KEY_MGMT_NONE;
		else if (strcmp(start, "WPA-NONE") == 0)
			val |= WPA_KEY_MGMT_WPA_NONE;
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid key_mgmt '%s'",
				   line, start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR,
			   "Line %d: no key_mgmt values configured.", line);
		errors++;
	}

	wpa_printf(MSG_MSGDUMP, "key_mgmt: 0x%x", val);
	ssid->key_mgmt = val;
	return errors ? -1 : 0;
}


static char * wpa_config_write_key_mgmt(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	int first = 1;
	char *buf, *pos, *end;

	pos = buf = malloc(50);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 50);
	end = buf + 50;

	if (ssid->key_mgmt & WPA_KEY_MGMT_PSK) {
		pos += snprintf(pos, end - pos, "%sWPA-PSK", first ? "" : " ");
		first = 0;
	}

	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		pos += snprintf(pos, end - pos, "%sWPA-EAP", first ? "" : " ");
		first = 0;
	}

	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		pos += snprintf(pos, end - pos, "%sIEEE8021X",
				first ? "" : " ");
		first = 0;
	}

	if (ssid->key_mgmt & WPA_KEY_MGMT_NONE) {
		pos += snprintf(pos, end - pos, "%sNONE", first ? "" : " ");
		first = 0;
	}

	if (ssid->key_mgmt & WPA_KEY_MGMT_WPA_NONE) {
		pos += snprintf(pos, end - pos, "%sWPA-NONE",
				first ? "" : " ");
		first = 0;
	}

	return buf;
}


static int wpa_config_parse_cipher(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "CCMP") == 0)
			val |= WPA_CIPHER_CCMP;
		else if (strcmp(start, "TKIP") == 0)
			val |= WPA_CIPHER_TKIP;
		else if (strcmp(start, "WEP104") == 0)
			val |= WPA_CIPHER_WEP104;
		else if (strcmp(start, "WEP40") == 0)
			val |= WPA_CIPHER_WEP40;
		else if (strcmp(start, "NONE") == 0)
			val |= WPA_CIPHER_NONE;
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid cipher '%s'.",
				   line, start);
			free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR, "Line %d: no cipher values configured.",
			   line);
		return -1;
	}
	return val;
}


static char * wpa_config_write_cipher(int cipher)
{
	int first = 1;
	char *buf, *pos, *end;

	pos = buf = malloc(50);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 50);
	end = buf + 50;

	if (cipher & WPA_CIPHER_CCMP) {
		pos += snprintf(pos, end - pos, "%sCCMP", first ? "" : " ");
		first = 0;
	}

	if (cipher & WPA_CIPHER_TKIP) {
		pos += snprintf(pos, end - pos, "%sTKIP", first ? "" : " ");
		first = 0;
	}

	if (cipher & WPA_CIPHER_WEP104) {
		pos += snprintf(pos, end - pos, "%sWEP104", first ? "" : " ");
		first = 0;
	}

	if (cipher & WPA_CIPHER_WEP40) {
		pos += snprintf(pos, end - pos, "%sWEP40", first ? "" : " ");
		first = 0;
	}

	if (cipher & WPA_CIPHER_NONE) {
		pos += snprintf(pos, end - pos, "%sNONE", first ? "" : " ");
		first = 0;
	}

	return buf;
}


static int wpa_config_parse_pairwise(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	int val;
	val = wpa_config_parse_cipher(line, value);
	if (val == -1)
		return -1;
	if (val & ~(WPA_CIPHER_CCMP | WPA_CIPHER_TKIP | WPA_CIPHER_NONE)) {
		wpa_printf(MSG_ERROR, "Line %d: not allowed pairwise cipher "
			   "(0x%x).", line, val);
		return -1;
	}

	wpa_printf(MSG_MSGDUMP, "pairwise: 0x%x", val);
	ssid->pairwise_cipher = val;
	return 0;
}


static char * wpa_config_write_pairwise(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	return wpa_config_write_cipher(ssid->pairwise_cipher);
}


static int wpa_config_parse_group(const struct parse_data *data,
				  struct wpa_ssid *ssid, int line,
				  const char *value)
{
	int val;
	val = wpa_config_parse_cipher(line, value);
	if (val == -1)
		return -1;
	if (val & ~(WPA_CIPHER_CCMP | WPA_CIPHER_TKIP | WPA_CIPHER_WEP104 |
		    WPA_CIPHER_WEP40)) {
		wpa_printf(MSG_ERROR, "Line %d: not allowed group cipher "
			   "(0x%x).", line, val);
		return -1;
	}

	wpa_printf(MSG_MSGDUMP, "group: 0x%x", val);
	ssid->group_cipher = val;
	return 0;
}


static char * wpa_config_write_group(const struct parse_data *data,
				     struct wpa_ssid *ssid)
{
	return wpa_config_write_cipher(ssid->group_cipher);
}


static int wpa_config_parse_auth_alg(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	int val = 0, last, errors = 0;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		if (strcmp(start, "OPEN") == 0)
			val |= WPA_AUTH_ALG_OPEN;
		else if (strcmp(start, "SHARED") == 0)
			val |= WPA_AUTH_ALG_SHARED;
		else if (strcmp(start, "LEAP") == 0)
			val |= WPA_AUTH_ALG_LEAP;
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid auth_alg '%s'",
				   line, start);
			errors++;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR,
			   "Line %d: no auth_alg values configured.", line);
		errors++;
	}

	wpa_printf(MSG_MSGDUMP, "auth_alg: 0x%x", val);
	ssid->auth_alg = val;
	return errors ? -1 : 0;
}


static char * wpa_config_write_auth_alg(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	int first = 1;
	char *buf, *pos, *end;

	pos = buf = malloc(30);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 30);
	end = buf + 30;

	if (ssid->auth_alg & WPA_AUTH_ALG_OPEN) {
		pos += snprintf(pos, end - pos, "%sOPEN", first ? "" : " ");
		first = 0;
	}

	if (ssid->auth_alg & WPA_AUTH_ALG_SHARED) {
		pos += snprintf(pos, end - pos, "%sSHARED", first ? "" : " ");
		first = 0;
	}

	if (ssid->auth_alg & WPA_AUTH_ALG_LEAP) {
		pos += snprintf(pos, end - pos, "%sLEAP", first ? "" : " ");
		first = 0;
	}

	return buf;
}


static int wpa_config_parse_eap(const struct parse_data *data,
				struct wpa_ssid *ssid, int line,
				const char *value)
{
	int last, errors = 0;
	char *start, *end, *buf;
	u8 *methods = NULL, *tmp;
	size_t num_methods = 0;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (*start != '\0') {
		while (*start == ' ' || *start == '\t')
			start++;
		if (*start == '\0')
			break;
		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\0')
			end++;
		last = *end == '\0';
		*end = '\0';
		tmp = methods;
		methods = realloc(methods, num_methods + 1);
		if (methods == NULL) {
			free(tmp);
			return -1;
		}
		methods[num_methods] = eap_get_type(start);
		if (methods[num_methods] == EAP_TYPE_NONE) {
			wpa_printf(MSG_ERROR, "Line %d: unknown EAP method "
				   "'%s'", line, start);
			wpa_printf(MSG_ERROR, "You may need to add support for"
				   " this EAP method during wpa_supplicant\n"
				   "build time configuration.\n"
				   "See README for more information.");
			errors++;
		} else if (methods[num_methods] == EAP_TYPE_LEAP)
			ssid->leap++;
		else
			ssid->non_leap++;
		num_methods++;
		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	tmp = methods;
	methods = realloc(methods, num_methods + 1);
	if (methods == NULL) {
		free(tmp);
		return -1;
	}
	methods[num_methods] = EAP_TYPE_NONE;
	num_methods++;

	wpa_hexdump(MSG_MSGDUMP, "eap methods", methods, num_methods);
	ssid->eap_methods = methods;
	return errors ? -1 : 0;
}


static char * wpa_config_write_eap(const struct parse_data *data,
				   struct wpa_ssid *ssid)
{
	int first = 1;
	char *buf, *pos, *end;
	const u8 *eap_methods = ssid->eap_methods;
	const char *name;

	if (eap_methods == NULL)
		return NULL;

	pos = buf = malloc(100);
	if (buf == NULL)
		return NULL;
	memset(buf, 0, 100);
	end = buf + 100;

	while (*eap_methods != EAP_TYPE_NONE) {
		name = eap_get_name(*eap_methods);
		if (name)
			pos += snprintf(pos, end - pos, "%s%s",
					first ? "" : " ", name);
		first = 0;
		eap_methods++;
	}

	return buf;
}


static int wpa_config_parse_wep_key(u8 *key, size_t *len, int line,
				    const char *value, int idx)
{
	char *buf, title[20];

	buf = wpa_config_parse_string(value, len);
	if (buf == NULL) {
		wpa_printf(MSG_ERROR, "Line %d: Invalid WEP key %d '%s'.",
			   line, idx, value);
		return -1;
	}
	if (*len > MAX_WEP_KEY_LEN) {
		wpa_printf(MSG_ERROR, "Line %d: Too long WEP key %d '%s'.",
			   line, idx, value);
		free(buf);
		return -1;
	}
	memcpy(key, buf, *len);
	free(buf);
	snprintf(title, sizeof(title), "wep_key%d", idx);
	wpa_hexdump_key(MSG_MSGDUMP, title, key, *len);
	return 0;
}


static int wpa_config_parse_wep_key0(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	return wpa_config_parse_wep_key(ssid->wep_key[0],
					&ssid->wep_key_len[0], line,
					value, 0);
}


static int wpa_config_parse_wep_key1(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	return wpa_config_parse_wep_key(ssid->wep_key[1],
					&ssid->wep_key_len[1], line,
					value, 1);
}


static int wpa_config_parse_wep_key2(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	return wpa_config_parse_wep_key(ssid->wep_key[2],
					&ssid->wep_key_len[2], line,
					value, 2);
}


static int wpa_config_parse_wep_key3(const struct parse_data *data,
				     struct wpa_ssid *ssid, int line,
				     const char *value)
{
	return wpa_config_parse_wep_key(ssid->wep_key[3],
					&ssid->wep_key_len[3], line,
					value, 3);
}


static char * wpa_config_write_wep_key(struct wpa_ssid *ssid, int idx)
{
	if (ssid->wep_key_len[idx] == 0)
		return NULL;
	return wpa_config_write_string(ssid->wep_key[idx],
				       ssid->wep_key_len[idx]);
}


static char * wpa_config_write_wep_key0(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	return wpa_config_write_wep_key(ssid, 0);
}


static char * wpa_config_write_wep_key1(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	return wpa_config_write_wep_key(ssid, 1);
}


static char * wpa_config_write_wep_key2(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	return wpa_config_write_wep_key(ssid, 2);
}


static char * wpa_config_write_wep_key3(const struct parse_data *data,
					struct wpa_ssid *ssid)
{
	return wpa_config_write_wep_key(ssid, 3);
}


/* Helper macros for network block parser */

/* OFFSET: Get offset of a variable within the wpa_ssid structure */
#define OFFSET(v) ((void *) &((struct wpa_ssid *) 0)->v)

/* STR: Define a string variable for an ASCII string; f = field name */
#define STR(f) .name = #f, .parser = wpa_config_parse_str, \
	.writer = wpa_config_write_str, .param1 = OFFSET(f)

/* STR_LEN: Define a string variable with a separate variable for storing the
 * data length. Unlike STR(), this can be used to store arbitrary binary data
 * (i.e., even nul termination character). */
#define STR_LEN(f) STR(f), .param2 = OFFSET(f ## _len)

/* STR_RANGE: Like STR_LEN(), but with minimum and maximum allowed length
 * explicitly specified. */
#define STR_RANGE(f, min, max) STR_LEN(f), .param3 = (void *) (min), \
	.param4 = (void *) (max)


/* INT: Define an integer variable */
#define INT(f) .name = #f, .parser = wpa_config_parse_int, \
	.writer = wpa_config_write_int, \
	.param1 = OFFSET(f), .param2 = (void *) 0

/* INT: Define an integer variable with allowed value range */
#define INT_RANGE(f, min, max) INT(f), .param3 = (void *) (min), \
	.param4 = (void *) (max)

/* FUNC: Define a configuration variable that uses a custom function for
 * parsing and writing the value. */
#define FUNC(f) .name = #f, .parser = wpa_config_parse_ ## f, \
		.writer = wpa_config_write_ ## f

/*
 * Table of network configuration variables. This table is used to parse each
 * network configuration variable, e.g., each line in wpa_supplicant.conf file
 * that is insider a network block.
 *
 * This table is generated using the helper macros defined above and with
 * generous help from the C pre-processor. The field name is stored as a string
 * into .name and for STR and INT types, the offset of the target buffer within
 * struct wpa_ssid is stored in .param1. .param2 (if not NULL) is similar
 * offset to the field containing the length of the configuration variable.
 * .param3 and .param4 can be used to mark the allowed range (length for STR
 * and value for INT).
 *
 * For each configuration line in wpa_supplicant.conf, the parser goes through
 * this table and select the entry that matches with the field name. The parser
 * function (.parser) is then called to parse the actual value of the field.
 *
 * This kind of mechanism makes it easy to add new configuration parameters,
 * since only one line needs to be added into this table and in struct wpa_ssid
 * definitions if the new variable is either a string or integer. More complex
 * types will need to use their own parser and writer functions.
 */
static const struct parse_data ssid_fields[] = {
	{ STR_RANGE(ssid, 0, MAX_SSID_LEN) },
	{ INT_RANGE(scan_ssid, 0, 1) },
	{ FUNC(bssid) },
	{ FUNC(psk), .key_data = 1 },
	{ FUNC(proto) },
	{ FUNC(key_mgmt) },
	{ FUNC(pairwise) },
	{ FUNC(group) },
	{ FUNC(auth_alg) },
	{ FUNC(eap) },
	{ STR_LEN(identity) },
	{ STR_LEN(anonymous_identity) },
	{ STR_RANGE(eappsk, EAP_PSK_LEN, EAP_PSK_LEN), .key_data = 1 },
	{ STR_LEN(nai) },
	{ STR_LEN(password), .key_data = 1 },
	{ STR(ca_cert) },
	{ STR(ca_path) },
	{ STR(client_cert) },
	{ STR(private_key) },
	{ STR(private_key_passwd), .key_data = 1 },
	{ STR(dh_file) },
	{ STR(subject_match) },
	{ STR(altsubject_match) },
	{ STR(ca_cert2) },
	{ STR(ca_path2) },
	{ STR(client_cert2) },
	{ STR(private_key2) },
	{ STR(private_key2_passwd), .key_data = 1 },
	{ STR(dh_file2) },
	{ STR(subject_match2) },
	{ STR(altsubject_match2) },
	{ STR(phase1) },
	{ STR(phase2) },
	{ STR(pcsc) },
	{ STR(pin), .key_data = 1 },
	{ STR(engine_id) },
	{ STR(key_id) },
	{ INT(engine) },
	{ INT(eapol_flags) },
	{ FUNC(wep_key0), .key_data = 1 },
	{ FUNC(wep_key1), .key_data = 1 },
	{ FUNC(wep_key2), .key_data = 1 },
	{ FUNC(wep_key3), .key_data = 1 },
	{ INT(wep_tx_keyidx) },
	{ INT(priority) },
	{ INT(eap_workaround) },
	{ STR(pac_file) },
	{ INT_RANGE(mode, 0, 1) },
	{ INT_RANGE(proactive_key_caching, 0, 1) },
	{ INT_RANGE(disabled, 0, 1) },
};

#undef OFFSET
#undef STR
#undef STR_LEN
#undef STR_RANGE
#undef INT
#undef INT_RANGE
#undef FUNC
#define NUM_SSID_FIELDS (sizeof(ssid_fields) / sizeof(ssid_fields[0]))


/**
 * wpa_config_add_prio_network - Add a network to priority lists
 * @config: Configuration data from wpa_config_read()
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to add a network block to the priority list of
 * networks. This must be called for each network when reading in the full
 * configuration. In addition, this can be used indirectly when updating
 * priorities by calling wpa_config_update_prio_list().
 */
int wpa_config_add_prio_network(struct wpa_config *config,
				struct wpa_ssid *ssid)
{
	int prio;
	struct wpa_ssid *prev, **nlist;

	for (prio = 0; prio < config->num_prio; prio++) {
		prev = config->pssid[prio];
		if (prev->priority == ssid->priority) {
			while (prev->pnext)
				prev = prev->pnext;
			prev->pnext = ssid;
			return 0;
		}
	}

	/* First network for this priority - add new priority list */
	nlist = realloc(config->pssid,
			(config->num_prio + 1) * sizeof(struct wpa_ssid *));
	if (nlist == NULL)
		return -1;

	for (prio = 0; prio < config->num_prio; prio++) {
		if (nlist[prio]->priority < ssid->priority)
			break;
	}

	memmove(&nlist[prio + 1], &nlist[prio],
		(config->num_prio - prio) * sizeof(struct wpa_ssid *));

	nlist[prio] = ssid;
	config->num_prio++;
	config->pssid = nlist;

	return 0;
}


/**
 * wpa_config_update_prio_list - Update network priority list
 * @config: Configuration data from wpa_config_read()
 * Returns: 0 on success, -1 on failure
 *
 * This function is called to update the priority list of networks in the
 * configuration when a network is being added or removed. This is also called
 * if a priority for a network is changed.
 */
static int wpa_config_update_prio_list(struct wpa_config *config)
{
	struct wpa_ssid *ssid;
	int ret = 0;

	free(config->pssid);
	config->pssid = NULL;
	config->num_prio = 0;

	ssid = config->ssid;
	while (ssid) {
		ssid->pnext = NULL;
		if (wpa_config_add_prio_network(config, ssid) < 0)
			ret = -1;
		ssid = ssid->next;
	}

	return ret;
}


/**
 * wpa_config_free_ssid - Free network/ssid configuration data
 * @ssid: Configuration data for the network
 *
 * This function frees all resources allocated for the netowkr configuration
 * data.
 */
void wpa_config_free_ssid(struct wpa_ssid *ssid)
{
	free(ssid->ssid);
	free(ssid->passphrase);
	free(ssid->eap_methods);
	free(ssid->identity);
	free(ssid->anonymous_identity);
	free(ssid->eappsk);
	free(ssid->nai);
	free(ssid->password);
	free(ssid->ca_cert);
	free(ssid->ca_path);
	free(ssid->client_cert);
	free(ssid->private_key);
	free(ssid->private_key_passwd);
	free(ssid->dh_file);
	free(ssid->subject_match);
	free(ssid->altsubject_match);
	free(ssid->ca_cert2);
	free(ssid->ca_path2);
	free(ssid->client_cert2);
	free(ssid->private_key2);
	free(ssid->private_key2_passwd);
	free(ssid->dh_file2);
	free(ssid->subject_match2);
	free(ssid->altsubject_match2);
	free(ssid->phase1);
	free(ssid->phase2);
	free(ssid->pcsc);
	free(ssid->pin);
	free(ssid->engine_id);
	free(ssid->key_id);
	free(ssid->otp);
	free(ssid->pending_req_otp);
	free(ssid->pac_file);
	free(ssid->new_password);
	free(ssid);
}


/**
 * wpa_config_free - Free configuration data
 * @config: Configuration data from wpa_config_read()
 *
 * This function frees all resources allocated for the configuration data by
 * wpa_config_read().
 */
void wpa_config_free(struct wpa_config *config)
{
	struct wpa_config_blob *blob, *prevblob;
	struct wpa_ssid *ssid, *prev = NULL;
	ssid = config->ssid;
	while (ssid) {
		prev = ssid;
		ssid = ssid->next;
		wpa_config_free_ssid(prev);
	}

	blob = config->blobs;
	prevblob = NULL;
	while (blob) {
		prevblob = blob;
		blob = blob->next;
		wpa_config_free_blob(prevblob);
	}

	free(config->ctrl_interface);
	free(config->opensc_engine_path);
	free(config->pkcs11_engine_path);
	free(config->pkcs11_module_path);
	free(config->driver_param);
	free(config->pssid);
	free(config);
}


/**
 * wpa_config_allowed_eap_method - Check whether EAP method is allowed
 * @ssid: Pointer to a configuration data
 * @method: EAP type
 * Returns: 1 = allowed EAP method, 0 = not allowed
 */
int wpa_config_allowed_eap_method(struct wpa_ssid *ssid, int method)
{
	u8 *pos;

	if (ssid == NULL || ssid->eap_methods == NULL)
		return 1;

	pos = ssid->eap_methods;
	while (*pos != EAP_TYPE_NONE) {
		if (*pos == method)
			return 1;
		pos++;
	}
	return 0;
}


/**
 * wpa_config_get_network - Get configured network based on id
 * @config: Configuration data from wpa_config_read()
 * @id: Unique network id to search for
 * Returns: Network configuration or %NULL if not found
 */
struct wpa_ssid * wpa_config_get_network(struct wpa_config *config, int id)
{
	struct wpa_ssid *ssid;

	ssid = config->ssid;
	while (ssid) {
		if (id == ssid->id)
			break;
		ssid = ssid->next;
	}

	return ssid;
}


/**
 * wpa_config_add_network - Add a new network with empty configuration
 * @config: Configuration data from wpa_config_read()
 * Returns: The new network configuration or %NULL if operation failed
 */
struct wpa_ssid * wpa_config_add_network(struct wpa_config *config)
{
	int id;
	struct wpa_ssid *ssid, *last = NULL;

	id = -1;
	ssid = config->ssid;
	while (ssid) {
		if (ssid->id > id)
			id = ssid->id;
		last = ssid;
		ssid = ssid->next;
	}
	id++;

	ssid = malloc(sizeof(*ssid));
	if (ssid == NULL)
		return NULL;
	memset(ssid, 0, sizeof(*ssid));
	ssid->id = id;
	if (last)
		last->next = ssid;
	else
		config->ssid = ssid;

	wpa_config_update_prio_list(config);

	return ssid;
}


/**
 * wpa_config_remove_network - Remove a configured network based on id
 * @config: Configuration data from wpa_config_read()
 * @id: Unique network id to search for
 * Returns: 0 on success, or -1 if the network was not found
 */
int wpa_config_remove_network(struct wpa_config *config, int id)
{
	struct wpa_ssid *ssid, *prev = NULL;

	ssid = config->ssid;
	while (ssid) {
		if (id == ssid->id)
			break;
		prev = ssid;
		ssid = ssid->next;
	}

	if (ssid == NULL)
		return -1;

	if (prev)
		prev->next = ssid->next;
	else
		config->ssid = ssid->next;

	wpa_config_update_prio_list(config);
	wpa_config_free_ssid(ssid);
	return 0;
}


/**
 * wpa_config_set_network_defaults - Set network default values
 * @ssid: Pointer to a network configuration data
 */
void wpa_config_set_network_defaults(struct wpa_ssid *ssid)
{
	ssid->proto = DEFAULT_PROTO;
	ssid->pairwise_cipher = DEFAULT_PAIRWISE;
	ssid->group_cipher = DEFAULT_GROUP;
	ssid->key_mgmt = DEFAULT_KEY_MGMT;
	ssid->eapol_flags = DEFAULT_EAPOL_FLAGS;
	ssid->eap_workaround = DEFAULT_EAP_WORKAROUND;
}


/**
 * wpa_config_set - Set a variable in network configuration
 * @ssid: Pointer to a network configuration data
 * @var: Variable name, e.g., "ssid"
 * @value: Variable value
 * @line: Line number in configuration file or 0 if not used
 * Returns: 0 on success, -1 on failure
 *
 * This function can be used to set network configuration variables based on
 * both the configuration file and management interface input. The value
 * parameter must be in the same format as the text-based configuration file is
 * using. For example, strings are using double quotation marks.
 */
int wpa_config_set(struct wpa_ssid *ssid, const char *var, const char *value,
		   int line)
{
	int i, ret = 0;

	if (ssid == NULL || var == NULL || value == NULL)
		return -1;

	for (i = 0; i < NUM_SSID_FIELDS; i++) {
		const struct parse_data *field = &ssid_fields[i];
		if (strcmp(var, field->name) != 0)
			continue;

		if (field->parser(field, ssid, line, value)) {
			if (line) {
				wpa_printf(MSG_ERROR, "Line %d: failed to "
					   "parse %s '%s'.", line, var, value);
			}
			ret = -1;
		}
		break;
	}
	if (i == NUM_SSID_FIELDS) {
		if (line) {
			wpa_printf(MSG_ERROR, "Line %d: unknown network field "
				   "'%s'.", line, var);
		}
		ret = -1;
	}

	return ret;
}


/**
 * wpa_config_get - Get a variable in network configuration
 * @ssid: Pointer to a network configuration data
 * @var: Variable name, e.g., "ssid"
 * Returns: Value of the variable or %NULL on failure
 *
 * This function can be used to get network configuration variables. The
 * returned value is a copy of the configuration variable in text format, i.e,.
 * the same format that the text-based configuration file and wpa_config_set()
 * are using for the value. The caller is responsible for freeing the returned
 * value.
 */
char * wpa_config_get(struct wpa_ssid *ssid, const char *var)
{
	int i;

	if (ssid == NULL || var == NULL)
		return NULL;

	for (i = 0; i < NUM_SSID_FIELDS; i++) {
		const struct parse_data *field = &ssid_fields[i];
		if (strcmp(var, field->name) == 0)
			return field->writer(field, ssid);
	}

	return NULL;
}


/**
 * wpa_config_update_psk - Update WPA PSK based on passphrase and SSID
 * @ssid: Pointer to a network configuration data
 *
 * This function must be called to update WPA PSK when either SSID or the
 * passphrase has changed for the network configuration.
 */
void wpa_config_update_psk(struct wpa_ssid *ssid)
{
	pbkdf2_sha1(ssid->passphrase,
		    (char *) ssid->ssid, ssid->ssid_len, 4096,
		    ssid->psk, PMK_LEN);
	wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
			ssid->psk, PMK_LEN);
	ssid->psk_set = 1;
}


/**
 * wpa_config_get_blob - Get a named configuration blob
 * @config: Configuration data from wpa_config_read()
 * @name: Name of the blob
 * Returns: Pointer to blob data or %NULL if not found
 */
const struct wpa_config_blob * wpa_config_get_blob(struct wpa_config *config,
						   const char *name)
{
	struct wpa_config_blob *blob = config->blobs;

	while (blob) {
		if (strcmp(blob->name, name) == 0)
			return blob;
		blob = blob->next;
	}
	return NULL;
}


/**
 * wpa_config_set_blob - Set or add a named configuration blob
 * @config: Configuration data from wpa_config_read()
 * @blob: New value for the blob
 *
 * Adds a new configuration blob or replaces the current value of an existing
 * blob.
 */
void wpa_config_set_blob(struct wpa_config *config,
			 struct wpa_config_blob *blob)
{
	wpa_config_remove_blob(config, blob->name);
	blob->next = config->blobs;
	config->blobs = blob;
}


/**
 * wpa_config_free_blob - Free blob data
 * @blob: Pointer to blob to be freed
 */
void wpa_config_free_blob(struct wpa_config_blob *blob)
{
	if (blob) {
		free(blob->name);
		free(blob->data);
		free(blob);
	}
}


/**
 * wpa_config_remove_blob - Remove a named configuration blob
 * @config: Configuration data from wpa_config_read()
 * @name: Name of the blob to remove
 * Returns: 0 if blob was removed or -1 if blob was not found
 */
int wpa_config_remove_blob(struct wpa_config *config, const char *name)
{
	struct wpa_config_blob *pos = config->blobs, *prev = NULL;

	while (pos) {
		if (strcmp(pos->name, name) == 0) {
			if (prev)
				prev->next = pos->next;
			else
				config->blobs = pos->next;
			wpa_config_free_blob(pos);
			return 0;
		}
		prev = pos;
		pos = pos->next;
	}

	return -1;
}


/**
 * wpa_config_alloc_empty - Allocate an empty configuration
 * @ctrl_interface: Control interface parameters, e.g., path to UNIX domain
 * socket
 * @driver_param: Driver parameters
 * Returns: Pointer to allocated configuration data or %NULL on failure
 */
struct wpa_config * wpa_config_alloc_empty(const char *ctrl_interface,
					   const char *driver_param)
{
	struct wpa_config *config;

	config = malloc(sizeof(*config));
	if (config == NULL)
		return NULL;
	memset(config, 0, sizeof(*config));
	config->eapol_version = DEFAULT_EAPOL_VERSION;
	config->ap_scan = DEFAULT_AP_SCAN;
	config->fast_reauth = DEFAULT_FAST_REAUTH;

	if (ctrl_interface)
		config->ctrl_interface = strdup(ctrl_interface);
	if (driver_param)
		config->driver_param = strdup(driver_param);

	return config;
}
