/*
 * hostapd / Configuration file
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
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
#ifndef CONFIG_NATIVE_WINDOWS
#include <grp.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "hostapd.h"
#include "driver.h"
#include "sha1.h"
#include "eap.h"
#include "radius_client.h"
#include "wpa_common.h"


#define MAX_STA_COUNT 2007


static int hostapd_config_read_vlan_file(struct hostapd_bss_config *bss,
					 const char *fname)
{
	FILE *f;
	char buf[128], *pos, *pos2;
	int line = 0, vlan_id;
	struct hostapd_vlan *vlan;

	f = fopen(fname, "r");
	if (!f) {
		printf("VLAN file '%s' not readable.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		if (buf[0] == '*') {
			vlan_id = VLAN_ID_WILDCARD;
			pos = buf + 1;
		} else {
			vlan_id = strtol(buf, &pos, 10);
			if (buf == pos || vlan_id < 1 ||
			    vlan_id > MAX_VLAN_ID) {
				printf("Invalid VLAN ID at line %d in '%s'\n",
				       line, fname);
				fclose(f);
				return -1;
			}
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;
		pos2 = pos;
		while (*pos2 != ' ' && *pos2 != '\t' && *pos2 != '\0')
			pos2++;
		*pos2 = '\0';
		if (*pos == '\0' || strlen(pos) > IFNAMSIZ) {
			printf("Invalid VLAN ifname at line %d in '%s'\n",
			       line, fname);
			fclose(f);
			return -1;
		}

		vlan = malloc(sizeof(*vlan));
		if (vlan == NULL) {
			printf("Out of memory while reading VLAN interfaces "
			       "from '%s'\n", fname);
			fclose(f);
			return -1;
		}

		memset(vlan, 0, sizeof(*vlan));
		vlan->vlan_id = vlan_id;
		strncpy(vlan->ifname, pos, sizeof(vlan->ifname));
		if (bss->vlan_tail)
			bss->vlan_tail->next = vlan;
		else
			bss->vlan = vlan;
		bss->vlan_tail = vlan;
	}

	fclose(f);

	return 0;
}


static void hostapd_config_free_vlan(struct hostapd_bss_config *bss)
{
	struct hostapd_vlan *vlan, *prev;

	vlan = bss->vlan;
	prev = NULL;
	while (vlan) {
		prev = vlan;
		vlan = vlan->next;
		free(prev);
	}

	bss->vlan = NULL;
}


/* convert floats with one decimal place to value*10 int, i.e.,
 * "1.5" will return 15 */
static int hostapd_config_read_int10(const char *value)
{
	int i, d;
	char *pos;

	i = atoi(value);
	pos = strchr(value, '.');
	d = 0;
	if (pos) {
		pos++;
		if (*pos >= '0' && *pos <= '9')
			d = *pos - '0';
	}

	return i * 10 + d;
}


static void hostapd_config_defaults_bss(struct hostapd_bss_config *bss)
{
	bss->logger_syslog_level = HOSTAPD_LEVEL_INFO;
	bss->logger_stdout_level = HOSTAPD_LEVEL_INFO;
	bss->logger_syslog = (unsigned int) -1;
	bss->logger_stdout = (unsigned int) -1;

	bss->auth_algs = HOSTAPD_AUTH_OPEN | HOSTAPD_AUTH_SHARED_KEY;

	bss->wep_rekeying_period = 300;
	/* use key0 in individual key and key1 in broadcast key */
	bss->broadcast_key_idx_min = 1;
	bss->broadcast_key_idx_max = 2;
	bss->eap_reauth_period = 3600;

	bss->wpa_group_rekey = 600;
	bss->wpa_gmk_rekey = 86400;
	bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	bss->wpa_pairwise = WPA_CIPHER_TKIP;
	bss->wpa_group = WPA_CIPHER_TKIP;

	bss->max_num_sta = MAX_STA_COUNT;

	bss->dtim_period = 2;

	bss->radius_server_auth_port = 1812;
	bss->ap_max_inactivity = AP_MAX_INACTIVITY;
	bss->eapol_version = EAPOL_VERSION;
}


static struct hostapd_config * hostapd_config_defaults(void)
{
	struct hostapd_config *conf;
	struct hostapd_bss_config *bss;
	int i;
	const int aCWmin = 15, aCWmax = 1024;
	const struct hostapd_wme_ac_params ac_bk =
		{ aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
	const struct hostapd_wme_ac_params ac_be =
		{ aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
	const struct hostapd_wme_ac_params ac_vi = /* video traffic */
		{ aCWmin >> 1, aCWmin, 2, 3000 / 32, 1 };
	const struct hostapd_wme_ac_params ac_vo = /* voice traffic */
		{ aCWmin >> 2, aCWmin >> 1, 2, 1500 / 32, 1 };

	conf = wpa_zalloc(sizeof(*conf));
	bss = wpa_zalloc(sizeof(*bss));
	if (conf == NULL || bss == NULL) {
		printf("Failed to allocate memory for configuration data.\n");
		free(conf);
		free(bss);
		return NULL;
	}

	/* set default driver based on configuration */
	conf->driver = driver_lookup("default");
	if (conf->driver == NULL) {
		printf("No default driver registered!\n");
		free(conf);
		free(bss);
		return NULL;
	}

	bss->radius = wpa_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		free(conf);
		free(bss);
		return NULL;
	}

	hostapd_config_defaults_bss(bss);

	conf->num_bss = 1;
	conf->bss = bss;

	conf->beacon_int = 100;
	conf->rts_threshold = -1; /* use driver default: 2347 */
	conf->fragm_threshold = -1; /* user driver default: 2346 */
	conf->send_probe_response = 1;
	conf->bridge_packets = INTERNAL_BRIDGE_DO_NOT_CONTROL;

	memcpy(conf->country, "US ", 3);

	for (i = 0; i < NUM_TX_QUEUES; i++)
		conf->tx_queue[i].aifs = -1; /* use hw default */

	conf->wme_ac_params[0] = ac_be;
	conf->wme_ac_params[1] = ac_bk;
	conf->wme_ac_params[2] = ac_vi;
	conf->wme_ac_params[3] = ac_vo;

	return conf;
}


static int hostapd_parse_ip_addr(const char *txt, struct hostapd_ip_addr *addr)
{
	if (inet_aton(txt, &addr->u.v4)) {
		addr->af = AF_INET;
		return 0;
	}

#ifdef CONFIG_IPV6
	if (inet_pton(AF_INET6, txt, &addr->u.v6) > 0) {
		addr->af = AF_INET6;
		return 0;
	}
#endif /* CONFIG_IPV6 */

	return -1;
}


int hostapd_mac_comp(const void *a, const void *b)
{
	return memcmp(a, b, sizeof(macaddr));
}


int hostapd_mac_comp_empty(const void *a)
{
	macaddr empty = { 0 };
	return memcmp(a, empty, sizeof(macaddr));
}


static int hostapd_config_read_maclist(const char *fname, macaddr **acl,
				       int *num)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0;
	u8 addr[ETH_ALEN];
	macaddr *newacl;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		printf("MAC list file '%s' not found.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		if (hwaddr_aton(buf, addr)) {
			printf("Invalid MAC address '%s' at line %d in '%s'\n",
			       buf, line, fname);
			fclose(f);
			return -1;
		}

		newacl = (macaddr *) realloc(*acl, (*num + 1) * ETH_ALEN);
		if (newacl == NULL) {
			printf("MAC list reallocation failed\n");
			fclose(f);
			return -1;
		}

		*acl = newacl;
		memcpy((*acl)[*num], addr, ETH_ALEN);
		(*num)++;
	}

	fclose(f);

	qsort(*acl, *num, sizeof(macaddr), hostapd_mac_comp);

	return 0;
}


static int hostapd_config_read_wpa_psk(const char *fname,
				       struct hostapd_ssid *ssid)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0, ret = 0, len, ok;
	u8 addr[ETH_ALEN];
	struct hostapd_wpa_psk *psk;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		printf("WPA PSK file '%s' not found.\n", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		if (hwaddr_aton(buf, addr)) {
			printf("Invalid MAC address '%s' on line %d in '%s'\n",
			       buf, line, fname);
			ret = -1;
			break;
		}

		psk = wpa_zalloc(sizeof(*psk));
		if (psk == NULL) {
			printf("WPA PSK allocation failed\n");
			ret = -1;
			break;
		}
		if (memcmp(addr, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) == 0)
			psk->group = 1;
		else
			memcpy(psk->addr, addr, ETH_ALEN);

		pos = buf + 17;
		if (pos == '\0') {
			printf("No PSK on line %d in '%s'\n", line, fname);
			free(psk);
			ret = -1;
			break;
		}
		pos++;

		ok = 0;
		len = strlen(pos);
		if (len == 64 && hexstr2bin(pos, psk->psk, PMK_LEN) == 0)
			ok = 1;
		else if (len >= 8 && len < 64) {
			pbkdf2_sha1(pos, ssid->ssid, ssid->ssid_len,
				    4096, psk->psk, PMK_LEN);
			ok = 1;
		}
		if (!ok) {
			printf("Invalid PSK '%s' on line %d in '%s'\n",
			       pos, line, fname);
			free(psk);
			ret = -1;
			break;
		}

		psk->next = ssid->wpa_psk;
		ssid->wpa_psk = psk;
	}

	fclose(f);

	return ret;
}


int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf)
{
	struct hostapd_ssid *ssid = &conf->ssid;

	if (ssid->wpa_passphrase != NULL) {
		if (ssid->wpa_psk != NULL) {
			printf("Warning: both WPA PSK and passphrase set. "
			       "Using passphrase.\n");
			free(ssid->wpa_psk);
		}
		ssid->wpa_psk = wpa_zalloc(sizeof(struct hostapd_wpa_psk));
		if (ssid->wpa_psk == NULL) {
			printf("Unable to alloc space for PSK\n");
			return -1;
		}
		wpa_hexdump_ascii(MSG_DEBUG, "SSID",
				  (u8 *) ssid->ssid, ssid->ssid_len);
		wpa_hexdump_ascii(MSG_DEBUG, "PSK (ASCII passphrase)",
				  (u8 *) ssid->wpa_passphrase,
				  strlen(ssid->wpa_passphrase));
		pbkdf2_sha1(ssid->wpa_passphrase,
			    ssid->ssid, ssid->ssid_len,
			    4096, ssid->wpa_psk->psk, PMK_LEN);
		wpa_hexdump(MSG_DEBUG, "PSK (from passphrase)",
			    ssid->wpa_psk->psk, PMK_LEN);
		ssid->wpa_psk->group = 1;

		memset(ssid->wpa_passphrase, 0,
		       strlen(ssid->wpa_passphrase));
		free(ssid->wpa_passphrase);
		ssid->wpa_passphrase = NULL;
	}

	if (ssid->wpa_psk_file) {
		if (hostapd_config_read_wpa_psk(ssid->wpa_psk_file,
						&conf->ssid))
			return -1;
		free(ssid->wpa_psk_file);
		ssid->wpa_psk_file = NULL;
	}

	return 0;
}


#ifdef EAP_SERVER
static int hostapd_config_read_eap_user(const char *fname,
					struct hostapd_bss_config *conf)
{
	FILE *f;
	char buf[512], *pos, *start, *pos2;
	int line = 0, ret = 0, num_methods;
	struct hostapd_eap_user *user, *tail = NULL;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		printf("EAP user file '%s' not found.\n", fname);
		return -1;
	}

	/* Lines: "user" METHOD,METHOD2 "password" (password optional) */
	while (fgets(buf, sizeof(buf), f)) {
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		user = NULL;

		if (buf[0] != '"' && buf[0] != '*') {
			printf("Invalid EAP identity (no \" in start) on "
			       "line %d in '%s'\n", line, fname);
			goto failed;
		}

		user = wpa_zalloc(sizeof(*user));
		if (user == NULL) {
			printf("EAP user allocation failed\n");
			goto failed;
		}
		user->force_version = -1;

		if (buf[0] == '*') {
			pos = buf;
		} else {
			pos = buf + 1;
			start = pos;
			while (*pos != '"' && *pos != '\0')
				pos++;
			if (*pos == '\0') {
				printf("Invalid EAP identity (no \" in end) on"
				       " line %d in '%s'\n", line, fname);
				goto failed;
			}

			user->identity = malloc(pos - start);
			if (user->identity == NULL) {
				printf("Failed to allocate memory for EAP "
				       "identity\n");
				goto failed;
			}
			memcpy(user->identity, start, pos - start);
			user->identity_len = pos - start;

			if (pos[0] == '"' && pos[1] == '*') {
				user->wildcard_prefix = 1;
				pos++;
			}
		}
		pos++;
		while (*pos == ' ' || *pos == '\t')
			pos++;

		if (*pos == '\0') {
			printf("No EAP method on line %d in '%s'\n",
			       line, fname);
			goto failed;
		}

		start = pos;
		while (*pos != ' ' && *pos != '\t' && *pos != '\0')
			pos++;
		if (*pos == '\0') {
			pos = NULL;
		} else {
			*pos = '\0';
			pos++;
		}
		num_methods = 0;
		while (*start) {
			char *pos3 = strchr(start, ',');
			if (pos3) {
				*pos3++ = '\0';
			}
			user->methods[num_methods].method =
				eap_get_type(start, &user->methods[num_methods]
					     .vendor);
			if (user->methods[num_methods].vendor ==
			    EAP_VENDOR_IETF &&
			    user->methods[num_methods].method == EAP_TYPE_NONE)
			{
				printf("Unsupported EAP type '%s' on line %d "
				       "in '%s'\n", start, line, fname);
				goto failed;
			}

			num_methods++;
			if (num_methods >= EAP_USER_MAX_METHODS)
				break;
			if (pos3 == NULL)
				break;
			start = pos3;
		}
		if (num_methods == 0) {
			printf("No EAP types configured on line %d in '%s'\n",
			       line, fname);
			goto failed;
		}

		if (pos == NULL)
			goto done;

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos == '\0')
			goto done;

		if (strncmp(pos, "[ver=0]", 7) == 0) {
			user->force_version = 0;
			goto done;
		}

		if (strncmp(pos, "[ver=1]", 7) == 0) {
			user->force_version = 1;
			goto done;
		}

		if (strncmp(pos, "[2]", 3) == 0) {
			user->phase2 = 1;
			goto done;
		}

		if (*pos == '"') {
			pos++;
			start = pos;
			while (*pos != '"' && *pos != '\0')
				pos++;
			if (*pos == '\0') {
				printf("Invalid EAP password (no \" in end) "
				       "on line %d in '%s'\n", line, fname);
				goto failed;
			}

			user->password = malloc(pos - start);
			if (user->password == NULL) {
				printf("Failed to allocate memory for EAP "
				       "password\n");
				goto failed;
			}
			memcpy(user->password, start, pos - start);
			user->password_len = pos - start;

			pos++;
		} else if (strncmp(pos, "hash:", 5) == 0) {
			pos += 5;
			pos2 = pos;
			while (*pos2 != '\0' && *pos2 != ' ' &&
			       *pos2 != '\t' && *pos2 != '#')
				pos2++;
			if (pos2 - pos != 32) {
				printf("Invalid password hash on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password = malloc(16);
			if (user->password == NULL) {
				printf("Failed to allocate memory for EAP "
				       "password hash\n");
				goto failed;
			}
			if (hexstr2bin(pos, user->password, 16) < 0) {
				printf("Invalid hash password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password_len = 16;
			user->password_hash = 1;
			pos = pos2;
		} else {
			pos2 = pos;
			while (*pos2 != '\0' && *pos2 != ' ' &&
			       *pos2 != '\t' && *pos2 != '#')
				pos2++;
			if ((pos2 - pos) & 1) {
				printf("Invalid hex password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password = malloc((pos2 - pos) / 2);
			if (user->password == NULL) {
				printf("Failed to allocate memory for EAP "
				       "password\n");
				goto failed;
			}
			if (hexstr2bin(pos, user->password,
				       (pos2 - pos) / 2) < 0) {
				printf("Invalid hex password on line %d in "
				       "'%s'\n", line, fname);
				goto failed;
			}
			user->password_len = (pos2 - pos) / 2;
			pos = pos2;
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (strncmp(pos, "[2]", 3) == 0) {
			user->phase2 = 1;
		}

	done:
		if (tail == NULL) {
			tail = conf->eap_user = user;
		} else {
			tail->next = user;
			tail = user;
		}
		continue;

	failed:
		if (user) {
			free(user->password);
			free(user->identity);
			free(user);
		}
		ret = -1;
		break;
	}

	fclose(f);

	return ret;
}
#endif /* EAP_SERVER */


static int
hostapd_config_read_radius_addr(struct hostapd_radius_server **server,
				int *num_server, const char *val, int def_port,
				struct hostapd_radius_server **curr_serv)
{
	struct hostapd_radius_server *nserv;
	int ret;
	static int server_index = 1;

	nserv = realloc(*server, (*num_server + 1) * sizeof(*nserv));
	if (nserv == NULL)
		return -1;

	*server = nserv;
	nserv = &nserv[*num_server];
	(*num_server)++;
	(*curr_serv) = nserv;

	memset(nserv, 0, sizeof(*nserv));
	nserv->port = def_port;
	ret = hostapd_parse_ip_addr(val, &nserv->addr);
	nserv->index = server_index++;

	return ret;
}


static int hostapd_config_parse_key_mgmt(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (start != '\0') {
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
		else {
			printf("Line %d: invalid key_mgmt '%s'\n",
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
		printf("Line %d: no key_mgmt values configured.\n", line);
		return -1;
	}

	return val;
}


static int hostapd_config_parse_cipher(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = strdup(value);
	if (buf == NULL)
		return -1;
	start = buf;

	while (start != '\0') {
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
			printf("Line %d: invalid cipher '%s'.", line, start);
			free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}
	free(buf);

	if (val == 0) {
		printf("Line %d: no cipher values configured.", line);
		return -1;
	}
	return val;
}


static int hostapd_config_check_bss(struct hostapd_bss_config *bss,
				    struct hostapd_config *conf)
{
	if (bss->ieee802_1x && !bss->eap_server &&
	    !bss->radius->auth_servers) {
		printf("Invalid IEEE 802.1X configuration (no EAP "
		       "authenticator configured).\n");
		return -1;
	}

	if (bss->wpa && (bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL) {
		printf("WPA-PSK enabled, but PSK or passphrase is not "
		       "configured.\n");
		return -1;
	}

	if (hostapd_mac_comp_empty(bss->bssid) != 0) {
		size_t i;

		for (i = 0; i < conf->num_bss; i++) {
			if ((&conf->bss[i] != bss) &&
			    (hostapd_mac_comp(conf->bss[i].bssid,
					      bss->bssid) == 0)) {
				printf("Duplicate BSSID " MACSTR
				       " on interface '%s' and '%s'.\n",
				       MAC2STR(bss->bssid),
				       conf->bss[i].iface, bss->iface);
				return -1;
			}
		}
	}

	return 0;
}


static int hostapd_config_check(struct hostapd_config *conf)
{
	size_t i;

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_config_check_bss(&conf->bss[i], conf))
			return -1;
	}

	return 0;
}


static int hostapd_config_read_wep(struct hostapd_wep_keys *wep, int keyidx,
				   char *val)
{
	size_t len = strlen(val);

	if (keyidx < 0 || keyidx > 3 || wep->key[keyidx] != NULL)
		return -1;

	if (val[0] == '"') {
		if (len < 2 || val[len - 1] != '"')
			return -1;
		len -= 2;
		wep->key[keyidx] = malloc(len);
		if (wep->key[keyidx] == NULL)
			return -1;
		memcpy(wep->key[keyidx], val + 1, len);
		wep->len[keyidx] = len;
	} else {
		if (len & 1)
			return -1;
		len /= 2;
		wep->key[keyidx] = malloc(len);
		if (wep->key[keyidx] == NULL)
			return -1;
		wep->len[keyidx] = len;
		if (hexstr2bin(val, wep->key[keyidx], len) < 0)
			return -1;
	}

	wep->keys_set++;

	return 0;
}


static int hostapd_parse_rates(int **rate_list, char *val)
{
	int *list;
	int count;
	char *pos, *end;

	free(*rate_list);
	*rate_list = NULL;

	pos = val;
	count = 0;
	while (*pos != '\0') {
		if (*pos == ' ')
			count++;
		pos++;
	}

	list = malloc(sizeof(int) * (count + 2));
	if (list == NULL)
		return -1;
	pos = val;
	count = 0;
	while (*pos != '\0') {
		end = strchr(pos, ' ');
		if (end)
			*end = '\0';

		list[count++] = atoi(pos);
		if (!end)
			break;
		pos = end + 1;
	}
	list[count] = -1;

	*rate_list = list;
	return 0;
}


static int hostapd_config_bss(struct hostapd_config *conf, const char *ifname)
{
	struct hostapd_bss_config *bss;

	if (*ifname == '\0')
		return -1;

	bss = realloc(conf->bss, (conf->num_bss + 1) *
		      sizeof(struct hostapd_bss_config));
	if (bss == NULL) {
		printf("Failed to allocate memory for multi-BSS entry\n");
		return -1;
	}
	conf->bss = bss;

	bss = &(conf->bss[conf->num_bss]);
	memset(bss, 0, sizeof(*bss));
	bss->radius = wpa_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		printf("Failed to allocate memory for multi-BSS RADIUS "
		       "data\n");
		return -1;
	}

	conf->num_bss++;
	conf->last_bss = bss;

	hostapd_config_defaults_bss(bss);
	snprintf(bss->iface, sizeof(bss->iface), "%s", ifname);
	memcpy(bss->ssid.vlan, bss->iface, IFNAMSIZ + 1);

	return 0;
}


static int valid_cw(int cw)
{
	return (cw == 1 || cw == 3 || cw == 7 || cw == 15 || cw == 31 ||
		cw == 63 || cw == 127 || cw == 255 || cw == 511 || cw == 1023);
}


enum {
	IEEE80211_TX_QUEUE_DATA0 = 0, /* used for EDCA AC_VO data */
	IEEE80211_TX_QUEUE_DATA1 = 1, /* used for EDCA AC_VI data */
	IEEE80211_TX_QUEUE_DATA2 = 2, /* used for EDCA AC_BE data */
	IEEE80211_TX_QUEUE_DATA3 = 3, /* used for EDCA AC_BK data */
	IEEE80211_TX_QUEUE_DATA4 = 4,
	IEEE80211_TX_QUEUE_AFTER_BEACON = 6,
	IEEE80211_TX_QUEUE_BEACON = 7
};

static int hostapd_config_tx_queue(struct hostapd_config *conf, char *name,
				   char *val)
{
	int num;
	char *pos;
	struct hostapd_tx_queue_params *queue;

	/* skip 'tx_queue_' prefix */
	pos = name + 9;
	if (strncmp(pos, "data", 4) == 0 &&
	    pos[4] >= '0' && pos[4] <= '9' && pos[5] == '_') {
		num = pos[4] - '0';
		pos += 6;
	} else if (strncmp(pos, "after_beacon_", 13) == 0) {
		num = IEEE80211_TX_QUEUE_AFTER_BEACON;
		pos += 13;
	} else if (strncmp(pos, "beacon_", 7) == 0) {
		num = IEEE80211_TX_QUEUE_BEACON;
		pos += 7;
	} else {
		printf("Unknown tx_queue name '%s'\n", pos);
		return -1;
	}

	queue = &conf->tx_queue[num];

	if (strcmp(pos, "aifs") == 0) {
		queue->aifs = atoi(val);
		if (queue->aifs < 0 || queue->aifs > 255) {
			printf("Invalid AIFS value %d\n", queue->aifs);
			return -1;
		}
	} else if (strcmp(pos, "cwmin") == 0) {
		queue->cwmin = atoi(val);
		if (!valid_cw(queue->cwmin)) {
			printf("Invalid cwMin value %d\n", queue->cwmin);
			return -1;
		}
	} else if (strcmp(pos, "cwmax") == 0) {
		queue->cwmax = atoi(val);
		if (!valid_cw(queue->cwmax)) {
			printf("Invalid cwMax value %d\n", queue->cwmax);
			return -1;
		}
	} else if (strcmp(pos, "burst") == 0) {
		queue->burst = hostapd_config_read_int10(val);
	} else {
		printf("Unknown tx_queue field '%s'\n", pos);
		return -1;
	}

	queue->configured = 1;

	return 0;
}


static int hostapd_config_wme_ac(struct hostapd_config *conf, char *name,
				   char *val)
{
	int num, v;
	char *pos;
	struct hostapd_wme_ac_params *ac;

	/* skip 'wme_ac_' prefix */
	pos = name + 7;
	if (strncmp(pos, "be_", 3) == 0) {
		num = 0;
		pos += 3;
	} else if (strncmp(pos, "bk_", 3) == 0) {
		num = 1;
		pos += 3;
	} else if (strncmp(pos, "vi_", 3) == 0) {
		num = 2;
		pos += 3;
	} else if (strncmp(pos, "vo_", 3) == 0) {
		num = 3;
		pos += 3;
	} else {
		printf("Unknown wme name '%s'\n", pos);
		return -1;
	}

	ac = &conf->wme_ac_params[num];

	if (strcmp(pos, "aifs") == 0) {
		v = atoi(val);
		if (v < 1 || v > 255) {
			printf("Invalid AIFS value %d\n", v);
			return -1;
		}
		ac->aifs = v;
	} else if (strcmp(pos, "cwmin") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			printf("Invalid cwMin value %d\n", v);
			return -1;
		}
		ac->cwmin = v;
	} else if (strcmp(pos, "cwmax") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			printf("Invalid cwMax value %d\n", v);
			return -1;
		}
		ac->cwmax = v;
	} else if (strcmp(pos, "txop_limit") == 0) {
		v = atoi(val);
		if (v < 0 || v > 0xffff) {
			printf("Invalid txop value %d\n", v);
			return -1;
		}
		ac->txopLimit = v;
	} else if (strcmp(pos, "acm") == 0) {
		v = atoi(val);
		if (v < 0 || v > 1) {
			printf("Invalid acm value %d\n", v);
			return -1;
		}
		ac->admission_control_mandatory = v;
	} else {
		printf("Unknown wme_ac_ field '%s'\n", pos);
		return -1;
	}

	return 0;
}


struct hostapd_config * hostapd_config_read(const char *fname)
{
	struct hostapd_config *conf;
	struct hostapd_bss_config *bss;
	FILE *f;
	char buf[256], *pos;
	int line = 0;
	int errors = 0;
	size_t i;

	f = fopen(fname, "r");
	if (f == NULL) {
		printf("Could not open configuration file '%s' for reading.\n",
		       fname);
		return NULL;
	}

	conf = hostapd_config_defaults();
	if (conf == NULL) {
		fclose(f);
		return NULL;
	}
	bss = conf->last_bss = conf->bss;

	while (fgets(buf, sizeof(buf), f)) {
		bss = conf->last_bss;
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		pos = strchr(buf, '=');
		if (pos == NULL) {
			printf("Line %d: invalid line '%s'\n", line, buf);
			errors++;
			continue;
		}
		*pos = '\0';
		pos++;

		if (strcmp(buf, "interface") == 0) {
			snprintf(conf->bss[0].iface,
				 sizeof(conf->bss[0].iface), "%s", pos);
		} else if (strcmp(buf, "bridge") == 0) {
			snprintf(bss->bridge, sizeof(bss->bridge), "%s", pos);
		} else if (strcmp(buf, "driver") == 0) {
			conf->driver = driver_lookup(pos);
			if (conf->driver == NULL) {
				printf("Line %d: invalid/unknown driver "
				       "'%s'\n", line, pos);
				errors++;
			}
		} else if (strcmp(buf, "debug") == 0) {
			bss->debug = atoi(pos);
		} else if (strcmp(buf, "logger_syslog_level") == 0) {
			bss->logger_syslog_level = atoi(pos);
		} else if (strcmp(buf, "logger_stdout_level") == 0) {
			bss->logger_stdout_level = atoi(pos);
		} else if (strcmp(buf, "logger_syslog") == 0) {
			bss->logger_syslog = atoi(pos);
		} else if (strcmp(buf, "logger_stdout") == 0) {
			bss->logger_stdout = atoi(pos);
		} else if (strcmp(buf, "dump_file") == 0) {
			bss->dump_log_name = strdup(pos);
		} else if (strcmp(buf, "ssid") == 0) {
			bss->ssid.ssid_len = strlen(pos);
			if (bss->ssid.ssid_len > HOSTAPD_MAX_SSID_LEN ||
			    bss->ssid.ssid_len < 1) {
				printf("Line %d: invalid SSID '%s'\n", line,
				       pos);
				errors++;
			} else {
				memcpy(bss->ssid.ssid, pos,
				       bss->ssid.ssid_len);
				bss->ssid.ssid[bss->ssid.ssid_len] = '\0';
				bss->ssid.ssid_set = 1;
			}
		} else if (strcmp(buf, "macaddr_acl") == 0) {
			bss->macaddr_acl = atoi(pos);
			if (bss->macaddr_acl != ACCEPT_UNLESS_DENIED &&
			    bss->macaddr_acl != DENY_UNLESS_ACCEPTED &&
			    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
				printf("Line %d: unknown macaddr_acl %d\n",
				       line, bss->macaddr_acl);
			}
		} else if (strcmp(buf, "accept_mac_file") == 0) {
			if (hostapd_config_read_maclist(pos, &bss->accept_mac,
							&bss->num_accept_mac))
			{
				printf("Line %d: Failed to read "
				       "accept_mac_file '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (strcmp(buf, "deny_mac_file") == 0) {
			if (hostapd_config_read_maclist(pos, &bss->deny_mac,
							&bss->num_deny_mac))
			{
				printf("Line %d: Failed to read "
				       "deny_mac_file '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (strcmp(buf, "ap_max_inactivity") == 0) {
			bss->ap_max_inactivity = atoi(pos);
		} else if (strcmp(buf, "country_code") == 0) {
			memcpy(conf->country, pos, 2);
			/* FIX: make this configurable */
			conf->country[2] = ' ';
		} else if (strcmp(buf, "ieee80211d") == 0) {
			conf->ieee80211d = atoi(pos);
		} else if (strcmp(buf, "ieee80211h") == 0) {
			conf->ieee80211h = atoi(pos);
		} else if (strcmp(buf, "assoc_ap_addr") == 0) {
			if (hwaddr_aton(pos, bss->assoc_ap_addr)) {
				printf("Line %d: invalid MAC address '%s'\n",
				       line, pos);
				errors++;
			}
			bss->assoc_ap = 1;
		} else if (strcmp(buf, "ieee8021x") == 0) {
			bss->ieee802_1x = atoi(pos);
		} else if (strcmp(buf, "eapol_version") == 0) {
			bss->eapol_version = atoi(pos);
			if (bss->eapol_version < 1 ||
			    bss->eapol_version > 2) {
				printf("Line %d: invalid EAPOL "
				       "version (%d): '%s'.\n",
				       line, bss->eapol_version, pos);
				errors++;
			} else
				wpa_printf(MSG_DEBUG, "eapol_version=%d",
					   bss->eapol_version);
#ifdef EAP_SERVER
		} else if (strcmp(buf, "eap_authenticator") == 0) {
			bss->eap_server = atoi(pos);
			printf("Line %d: obsolete eap_authenticator used; "
			       "this has been renamed to eap_server\n", line);
		} else if (strcmp(buf, "eap_server") == 0) {
			bss->eap_server = atoi(pos);
		} else if (strcmp(buf, "eap_user_file") == 0) {
			if (hostapd_config_read_eap_user(pos, bss))
				errors++;
		} else if (strcmp(buf, "ca_cert") == 0) {
			free(bss->ca_cert);
			bss->ca_cert = strdup(pos);
		} else if (strcmp(buf, "server_cert") == 0) {
			free(bss->server_cert);
			bss->server_cert = strdup(pos);
		} else if (strcmp(buf, "private_key") == 0) {
			free(bss->private_key);
			bss->private_key = strdup(pos);
		} else if (strcmp(buf, "private_key_passwd") == 0) {
			free(bss->private_key_passwd);
			bss->private_key_passwd = strdup(pos);
		} else if (strcmp(buf, "check_crl") == 0) {
			bss->check_crl = atoi(pos);
#ifdef EAP_SIM
		} else if (strcmp(buf, "eap_sim_db") == 0) {
			free(bss->eap_sim_db);
			bss->eap_sim_db = strdup(pos);
#endif /* EAP_SIM */
#endif /* EAP_SERVER */
		} else if (strcmp(buf, "eap_message") == 0) {
			char *term;
			bss->eap_req_id_text = strdup(pos);
			if (bss->eap_req_id_text == NULL) {
				printf("Line %d: Failed to allocate memory "
				       "for eap_req_id_text\n", line);
				errors++;
				continue;
			}
			bss->eap_req_id_text_len =
				strlen(bss->eap_req_id_text);
			term = strstr(bss->eap_req_id_text, "\\0");
			if (term) {
				*term++ = '\0';
				memmove(term, term + 1,
					bss->eap_req_id_text_len -
					(term - bss->eap_req_id_text) - 1);
				bss->eap_req_id_text_len--;
			}
		} else if (strcmp(buf, "wep_key_len_broadcast") == 0) {
			bss->default_wep_key_len = atoi(pos);
			if (bss->default_wep_key_len > 13) {
				printf("Line %d: invalid WEP key len %lu "
				       "(= %lu bits)\n", line,
				       (unsigned long)
				       bss->default_wep_key_len,
				       (unsigned long)
				       bss->default_wep_key_len * 8);
				errors++;
			}
		} else if (strcmp(buf, "wep_key_len_unicast") == 0) {
			bss->individual_wep_key_len = atoi(pos);
			if (bss->individual_wep_key_len < 0 ||
			    bss->individual_wep_key_len > 13) {
				printf("Line %d: invalid WEP key len %d "
				       "(= %d bits)\n", line,
				       bss->individual_wep_key_len,
				       bss->individual_wep_key_len * 8);
				errors++;
			}
		} else if (strcmp(buf, "wep_rekey_period") == 0) {
			bss->wep_rekeying_period = atoi(pos);
			if (bss->wep_rekeying_period < 0) {
				printf("Line %d: invalid period %d\n",
				       line, bss->wep_rekeying_period);
				errors++;
			}
		} else if (strcmp(buf, "eap_reauth_period") == 0) {
			bss->eap_reauth_period = atoi(pos);
			if (bss->eap_reauth_period < 0) {
				printf("Line %d: invalid period %d\n",
				       line, bss->eap_reauth_period);
				errors++;
			}
		} else if (strcmp(buf, "eapol_key_index_workaround") == 0) {
			bss->eapol_key_index_workaround = atoi(pos);
#ifdef CONFIG_IAPP
		} else if (strcmp(buf, "iapp_interface") == 0) {
			bss->ieee802_11f = 1;
			snprintf(bss->iapp_iface, sizeof(bss->iapp_iface),
				 "%s", pos);
#endif /* CONFIG_IAPP */
		} else if (strcmp(buf, "own_ip_addr") == 0) {
			if (hostapd_parse_ip_addr(pos, &bss->own_ip_addr)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (strcmp(buf, "nas_identifier") == 0) {
			bss->nas_identifier = strdup(pos);
		} else if (strcmp(buf, "auth_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &bss->radius->auth_servers,
				    &bss->radius->num_auth_servers, pos, 1812,
				    &bss->radius->auth_server)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (bss->radius->auth_server &&
			   strcmp(buf, "auth_server_port") == 0) {
			bss->radius->auth_server->port = atoi(pos);
		} else if (bss->radius->auth_server &&
			   strcmp(buf, "auth_server_shared_secret") == 0) {
			int len = strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				printf("Line %d: empty shared secret is not "
				       "allowed.\n", line);
				errors++;
			}
			bss->radius->auth_server->shared_secret =
				(u8 *) strdup(pos);
			bss->radius->auth_server->shared_secret_len = len;
		} else if (strcmp(buf, "acct_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &bss->radius->acct_servers,
				    &bss->radius->num_acct_servers, pos, 1813,
				    &bss->radius->acct_server)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (bss->radius->acct_server &&
			   strcmp(buf, "acct_server_port") == 0) {
			bss->radius->acct_server->port = atoi(pos);
		} else if (bss->radius->acct_server &&
			   strcmp(buf, "acct_server_shared_secret") == 0) {
			int len = strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				printf("Line %d: empty shared secret is not "
				       "allowed.\n", line);
				errors++;
			}
			bss->radius->acct_server->shared_secret =
				(u8 *) strdup(pos);
			bss->radius->acct_server->shared_secret_len = len;
		} else if (strcmp(buf, "radius_retry_primary_interval") == 0) {
			bss->radius->retry_primary_interval = atoi(pos);
		} else if (strcmp(buf, "radius_acct_interim_interval") == 0) {
			bss->radius->acct_interim_interval = atoi(pos);
		} else if (strcmp(buf, "auth_algs") == 0) {
			bss->auth_algs = atoi(pos);
			if (bss->auth_algs == 0) {
				printf("Line %d: no authentication algorithms "
				       "allowed\n",
				       line);
				errors++;
			}
		} else if (strcmp(buf, "max_num_sta") == 0) {
			bss->max_num_sta = atoi(pos);
			if (bss->max_num_sta < 0 ||
			    bss->max_num_sta > MAX_STA_COUNT) {
				printf("Line %d: Invalid max_num_sta=%d; "
				       "allowed range 0..%d\n", line,
				       bss->max_num_sta, MAX_STA_COUNT);
				errors++;
			}
		} else if (strcmp(buf, "wpa") == 0) {
			bss->wpa = atoi(pos);
		} else if (strcmp(buf, "wpa_group_rekey") == 0) {
			bss->wpa_group_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_strict_rekey") == 0) {
			bss->wpa_strict_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_gmk_rekey") == 0) {
			bss->wpa_gmk_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_passphrase") == 0) {
			int len = strlen(pos);
			if (len < 8 || len > 63) {
				printf("Line %d: invalid WPA passphrase length"
				       " %d (expected 8..63)\n", line, len);
				errors++;
			} else {
				free(bss->ssid.wpa_passphrase);
				bss->ssid.wpa_passphrase = strdup(pos);
			}
		} else if (strcmp(buf, "wpa_psk") == 0) {
			free(bss->ssid.wpa_psk);
			bss->ssid.wpa_psk =
				wpa_zalloc(sizeof(struct hostapd_wpa_psk));
			if (bss->ssid.wpa_psk == NULL)
				errors++;
			else if (hexstr2bin(pos, bss->ssid.wpa_psk->psk,
					    PMK_LEN) ||
				 pos[PMK_LEN * 2] != '\0') {
				printf("Line %d: Invalid PSK '%s'.\n", line,
				       pos);
				errors++;
			} else {
				bss->ssid.wpa_psk->group = 1;
			}
		} else if (strcmp(buf, "wpa_psk_file") == 0) {
			free(bss->ssid.wpa_psk_file);
			bss->ssid.wpa_psk_file = strdup(pos);
			if (!bss->ssid.wpa_psk_file) {
				printf("Line %d: allocation failed\n", line);
				errors++;
			}
		} else if (strcmp(buf, "wpa_key_mgmt") == 0) {
			bss->wpa_key_mgmt =
				hostapd_config_parse_key_mgmt(line, pos);
			if (bss->wpa_key_mgmt == -1)
				errors++;
		} else if (strcmp(buf, "wpa_pairwise") == 0) {
			bss->wpa_pairwise =
				hostapd_config_parse_cipher(line, pos);
			if (bss->wpa_pairwise == -1 ||
			    bss->wpa_pairwise == 0)
				errors++;
			else if (bss->wpa_pairwise &
				 (WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
				  WPA_CIPHER_WEP104)) {
				printf("Line %d: unsupported pairwise "
				       "cipher suite '%s'\n",
				       bss->wpa_pairwise, pos);
				errors++;
			} else {
				if (bss->wpa_pairwise & WPA_CIPHER_TKIP)
					bss->wpa_group = WPA_CIPHER_TKIP;
				else
					bss->wpa_group = WPA_CIPHER_CCMP;
			}
#ifdef CONFIG_RSN_PREAUTH
		} else if (strcmp(buf, "rsn_preauth") == 0) {
			bss->rsn_preauth = atoi(pos);
		} else if (strcmp(buf, "rsn_preauth_interfaces") == 0) {
			bss->rsn_preauth_interfaces = strdup(pos);
#endif /* CONFIG_RSN_PREAUTH */
#ifdef CONFIG_PEERKEY
		} else if (strcmp(buf, "peerkey") == 0) {
			bss->peerkey = atoi(pos);
#endif /* CONFIG_PEERKEY */
		} else if (strcmp(buf, "ctrl_interface") == 0) {
			free(bss->ctrl_interface);
			bss->ctrl_interface = strdup(pos);
		} else if (strcmp(buf, "ctrl_interface_group") == 0) {
#ifndef CONFIG_NATIVE_WINDOWS
			struct group *grp;
			char *endp;
			const char *group = pos;

			grp = getgrnam(group);
			if (grp) {
				bss->ctrl_interface_gid = grp->gr_gid;
				bss->ctrl_interface_gid_set = 1;
				wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
					   " (from group name '%s')",
					   bss->ctrl_interface_gid, group);
				continue;
			}

			/* Group name not found - try to parse this as gid */
			bss->ctrl_interface_gid = strtol(group, &endp, 10);
			if (*group == '\0' || *endp != '\0') {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid group "
					   "'%s'", line, group);
				errors++;
				continue;
			}
			bss->ctrl_interface_gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   bss->ctrl_interface_gid);
#endif /* CONFIG_NATIVE_WINDOWS */
#ifdef RADIUS_SERVER
		} else if (strcmp(buf, "radius_server_clients") == 0) {
			free(bss->radius_server_clients);
			bss->radius_server_clients = strdup(pos);
		} else if (strcmp(buf, "radius_server_auth_port") == 0) {
			bss->radius_server_auth_port = atoi(pos);
		} else if (strcmp(buf, "radius_server_ipv6") == 0) {
			bss->radius_server_ipv6 = atoi(pos);
#endif /* RADIUS_SERVER */
		} else if (strcmp(buf, "test_socket") == 0) {
			free(bss->test_socket);
			bss->test_socket = strdup(pos);
		} else if (strcmp(buf, "use_pae_group_addr") == 0) {
			bss->use_pae_group_addr = atoi(pos);
		} else if (strcmp(buf, "hw_mode") == 0) {
			if (strcmp(pos, "a") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211A;
			else if (strcmp(pos, "b") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211B;
			else if (strcmp(pos, "g") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211G;
			else {
				printf("Line %d: unknown hw_mode '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (strcmp(buf, "channel") == 0) {
			conf->channel = atoi(pos);
		} else if (strcmp(buf, "beacon_int") == 0) {
			int val = atoi(pos);
			/* MIB defines range as 1..65535, but very small values
			 * cause problems with the current implementation.
			 * Since it is unlikely that this small numbers are
			 * useful in real life scenarios, do not allow beacon
			 * period to be set below 15 TU. */
			if (val < 15 || val > 65535) {
				printf("Line %d: invalid beacon_int %d "
				       "(expected 15..65535)\n",
				       line, val);
				errors++;
			} else
				conf->beacon_int = val;
		} else if (strcmp(buf, "dtim_period") == 0) {
			bss->dtim_period = atoi(pos);
			if (bss->dtim_period < 1 || bss->dtim_period > 255) {
				printf("Line %d: invalid dtim_period %d\n",
				       line, bss->dtim_period);
				errors++;
			}
		} else if (strcmp(buf, "rts_threshold") == 0) {
			conf->rts_threshold = atoi(pos);
			if (conf->rts_threshold < 0 ||
			    conf->rts_threshold > 2347) {
				printf("Line %d: invalid rts_threshold %d\n",
				       line, conf->rts_threshold);
				errors++;
			}
		} else if (strcmp(buf, "fragm_threshold") == 0) {
			conf->fragm_threshold = atoi(pos);
			if (conf->fragm_threshold < 256 ||
			    conf->fragm_threshold > 2346) {
				printf("Line %d: invalid fragm_threshold %d\n",
				       line, conf->fragm_threshold);
				errors++;
			}
		} else if (strcmp(buf, "send_probe_response") == 0) {
			int val = atoi(pos);
			if (val != 0 && val != 1) {
				printf("Line %d: invalid send_probe_response "
				       "%d (expected 0 or 1)\n", line, val);
			} else
				conf->send_probe_response = val;
		} else if (strcmp(buf, "supported_rates") == 0) {
			if (hostapd_parse_rates(&conf->supported_rates, pos)) {
				printf("Line %d: invalid rate list\n", line);
				errors++;
			}
		} else if (strcmp(buf, "basic_rates") == 0) {
			if (hostapd_parse_rates(&conf->basic_rates, pos)) {
				printf("Line %d: invalid rate list\n", line);
				errors++;
			}
		} else if (strcmp(buf, "ignore_broadcast_ssid") == 0) {
			bss->ignore_broadcast_ssid = atoi(pos);
		} else if (strcmp(buf, "bridge_packets") == 0) {
			conf->bridge_packets = atoi(pos);
		} else if (strcmp(buf, "wep_default_key") == 0) {
			bss->ssid.wep.idx = atoi(pos);
			if (bss->ssid.wep.idx > 3) {
				printf("Invalid wep_default_key index %d\n",
				       bss->ssid.wep.idx);
				errors++;
			}
		} else if (strcmp(buf, "wep_key0") == 0 ||
			   strcmp(buf, "wep_key1") == 0 ||
			   strcmp(buf, "wep_key2") == 0 ||
			   strcmp(buf, "wep_key3") == 0) {
			if (hostapd_config_read_wep(&bss->ssid.wep,
						    buf[7] - '0', pos)) {
				printf("Line %d: invalid WEP key '%s'\n",
				       line, buf);
				errors++;
			}
		} else if (strcmp(buf, "dynamic_vlan") == 0) {
			bss->ssid.dynamic_vlan = atoi(pos);
		} else if (strcmp(buf, "vlan_file") == 0) {
			if (hostapd_config_read_vlan_file(bss, pos)) {
				printf("Line %d: failed to read VLAN file "
				       "'%s'\n", line, pos);
				errors++;
			}
#ifdef CONFIG_FULL_DYNAMIC_VLAN
		} else if (strcmp(buf, "vlan_tagged_interface") == 0) {
			bss->ssid.vlan_tagged_interface = strdup(pos);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
		} else if (strcmp(buf, "passive_scan_interval") == 0) {
			conf->passive_scan_interval = atoi(pos);
		} else if (strcmp(buf, "passive_scan_listen") == 0) {
			conf->passive_scan_listen = atoi(pos);
		} else if (strcmp(buf, "passive_scan_mode") == 0) {
			conf->passive_scan_mode = atoi(pos);
		} else if (strcmp(buf, "ap_table_max_size") == 0) {
			conf->ap_table_max_size = atoi(pos);
		} else if (strcmp(buf, "ap_table_expiration_time") == 0) {
			conf->ap_table_expiration_time = atoi(pos);
		} else if (strncmp(buf, "tx_queue_", 9) == 0) {
			if (hostapd_config_tx_queue(conf, buf, pos)) {
				printf("Line %d: invalid TX queue item\n",
				       line);
				errors++;
			}
		} else if (strcmp(buf, "wme_enabled") == 0) {
			bss->wme_enabled = atoi(pos);
		} else if (strncmp(buf, "wme_ac_", 7) == 0) {
			if (hostapd_config_wme_ac(conf, buf, pos)) {
				printf("Line %d: invalid wme ac item\n",
				       line);
				errors++;
			}
		} else if (strcmp(buf, "bss") == 0) {
			if (hostapd_config_bss(conf, pos)) {
				printf("Line %d: invalid bss item\n", line);
				errors++;
			}
		} else if (strcmp(buf, "bssid") == 0) {
			if (bss == conf->bss) {
				printf("Line %d: bssid item not allowed "
				       "for the default interface\n", line);
				errors++;
			} else if (hwaddr_aton(pos, bss->bssid)) {
				printf("Line %d: invalid bssid item\n", line);
				errors++;
			}
#ifdef CONFIG_IEEE80211W
		} else if (strcmp(buf, "ieee80211w") == 0) {
			bss->ieee80211w = atoi(pos);
#endif /* CONFIG_IEEE80211W */
		} else {
			printf("Line %d: unknown configuration item '%s'\n",
			       line, buf);
			errors++;
		}
	}

	fclose(f);

	if (bss->individual_wep_key_len == 0) {
		/* individual keys are not use; can use key idx0 for broadcast
		 * keys */
		bss->broadcast_key_idx_min = 0;
	}

	for (i = 0; i < conf->num_bss; i++) {
		bss = &conf->bss[i];

		bss->radius->auth_server = bss->radius->auth_servers;
		bss->radius->acct_server = bss->radius->acct_servers;

		if (bss->wpa && bss->ieee802_1x) {
			bss->ssid.security_policy = SECURITY_WPA;
		} else if (bss->wpa) {
			bss->ssid.security_policy = SECURITY_WPA_PSK;
		} else if (bss->ieee802_1x) {
			bss->ssid.security_policy = SECURITY_IEEE_802_1X;
			bss->ssid.wep.default_len = bss->default_wep_key_len;
		} else if (bss->ssid.wep.keys_set)
			bss->ssid.security_policy = SECURITY_STATIC_WEP;
		else
			bss->ssid.security_policy = SECURITY_PLAINTEXT;
	}

	if (hostapd_config_check(conf))
		errors++;

	if (errors) {
		printf("%d errors found in configuration file '%s'\n",
		       errors, fname);
		hostapd_config_free(conf);
		conf = NULL;
	}

	return conf;
}


int hostapd_wep_key_cmp(struct hostapd_wep_keys *a, struct hostapd_wep_keys *b)
{
	int i;

	if (a->idx != b->idx || a->default_len != b->default_len)
		return 1;
	for (i = 0; i < NUM_WEP_KEYS; i++)
		if (a->len[i] != b->len[i] ||
		    memcmp(a->key[i], b->key[i], a->len[i]) != 0)
			return 1;
	return 0;
}


static void hostapd_config_free_radius(struct hostapd_radius_server *servers,
				       int num_servers)
{
	int i;

	for (i = 0; i < num_servers; i++) {
		free(servers[i].shared_secret);
	}
	free(servers);
}


static void hostapd_config_free_eap_user(struct hostapd_eap_user *user)
{
	free(user->identity);
	free(user->password);
	free(user);
}


static void hostapd_config_free_wep(struct hostapd_wep_keys *keys)
{
	int i;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		free(keys->key[i]);
		keys->key[i] = NULL;
	}
}


static void hostapd_config_free_bss(struct hostapd_bss_config *conf)
{
	struct hostapd_wpa_psk *psk, *prev;
	struct hostapd_eap_user *user, *prev_user;

	if (conf == NULL)
		return;

	psk = conf->ssid.wpa_psk;
	while (psk) {
		prev = psk;
		psk = psk->next;
		free(prev);
	}

	free(conf->ssid.wpa_passphrase);
	free(conf->ssid.wpa_psk_file);
#ifdef CONFIG_FULL_DYNAMIC_VLAN
	free(conf->ssid.vlan_tagged_interface);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

	user = conf->eap_user;
	while (user) {
		prev_user = user;
		user = user->next;
		hostapd_config_free_eap_user(prev_user);
	}

	free(conf->dump_log_name);
	free(conf->eap_req_id_text);
	free(conf->accept_mac);
	free(conf->deny_mac);
	free(conf->nas_identifier);
	hostapd_config_free_radius(conf->radius->auth_servers,
				   conf->radius->num_auth_servers);
	hostapd_config_free_radius(conf->radius->acct_servers,
				   conf->radius->num_acct_servers);
	free(conf->rsn_preauth_interfaces);
	free(conf->ctrl_interface);
	free(conf->ca_cert);
	free(conf->server_cert);
	free(conf->private_key);
	free(conf->private_key_passwd);
	free(conf->eap_sim_db);
	free(conf->radius_server_clients);
	free(conf->test_socket);
	free(conf->radius);
	hostapd_config_free_vlan(conf);
	if (conf->ssid.dyn_vlan_keys) {
		struct hostapd_ssid *ssid = &conf->ssid;
		size_t i;
		for (i = 0; i <= ssid->max_dyn_vlan_keys; i++) {
			if (ssid->dyn_vlan_keys[i] == NULL)
				continue;
			hostapd_config_free_wep(ssid->dyn_vlan_keys[i]);
			free(ssid->dyn_vlan_keys[i]);
		}
		free(ssid->dyn_vlan_keys);
		ssid->dyn_vlan_keys = NULL;
	}
}


void hostapd_config_free(struct hostapd_config *conf)
{
	size_t i;

	if (conf == NULL)
		return;

	for (i = 0; i < conf->num_bss; i++)
		hostapd_config_free_bss(&conf->bss[i]);
	free(conf->bss);

	free(conf);
}


/* Perform a binary search for given MAC address from a pre-sorted list.
 * Returns 1 if address is in the list or 0 if not. */
int hostapd_maclist_found(macaddr *list, int num_entries, const u8 *addr)
{
	int start, end, middle, res;

	start = 0;
	end = num_entries - 1;

	while (start <= end) {
		middle = (start + end) / 2;
		res = memcmp(list[middle], addr, ETH_ALEN);
		if (res == 0)
			return 1;
		if (res < 0)
			start = middle + 1;
		else
			end = middle - 1;
	}

	return 0;
}


int hostapd_rate_found(int *list, int rate)
{
	int i;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] >= 0; i++)
		if (list[i] == rate)
			return 1;

	return 0;
}


const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
	struct hostapd_vlan *v = vlan;
	while (v) {
		if (v->vlan_id == vlan_id || v->vlan_id == VLAN_ID_WILDCARD)
			return v->ifname;
		v = v->next;
	}
	return NULL;
}


const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *prev_psk)
{
	struct hostapd_wpa_psk *psk;
	int next_ok = prev_psk == NULL;

	for (psk = conf->ssid.wpa_psk; psk != NULL; psk = psk->next) {
		if (next_ok &&
		    (psk->group || memcmp(psk->addr, addr, ETH_ALEN) == 0))
			return psk->psk;

		if (psk->psk == prev_psk)
			next_ok = 1;
	}

	return NULL;
}


const struct hostapd_eap_user *
hostapd_get_eap_user(const struct hostapd_bss_config *conf, const u8 *identity,
		     size_t identity_len, int phase2)
{
	struct hostapd_eap_user *user = conf->eap_user;

	while (user) {
		if (!phase2 && user->identity == NULL) {
			/* Wildcard match */
			break;
		}

		if (!phase2 && user->wildcard_prefix &&
		    identity_len >= user->identity_len &&
		    memcmp(user->identity, identity, user->identity_len) == 0)
		{
			/* Wildcard prefix match */
			break;
		}

		if (user->phase2 == !!phase2 &&
		    user->identity_len == identity_len &&
		    memcmp(user->identity, identity, identity_len) == 0)
			break;
		user = user->next;
	}

	return user;
}
