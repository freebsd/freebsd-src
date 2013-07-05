/*
 * hostapd / Configuration file parser
 * Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#ifndef CONFIG_NATIVE_WINDOWS
#include <grp.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "utils/common.h"
#include "utils/uuid.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "eap_server/eap.h"
#include "radius/radius_client.h"
#include "ap/wpa_auth.h"
#include "ap/ap_config.h"
#include "config_file.h"


extern struct wpa_driver_ops *wpa_drivers[];


#ifndef CONFIG_NO_VLAN
static int hostapd_config_read_vlan_file(struct hostapd_bss_config *bss,
					 const char *fname)
{
	FILE *f;
	char buf[128], *pos, *pos2;
	int line = 0, vlan_id;
	struct hostapd_vlan *vlan;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "VLAN file '%s' not readable.", fname);
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
				wpa_printf(MSG_ERROR, "Invalid VLAN ID at "
					   "line %d in '%s'", line, fname);
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
		if (*pos == '\0' || os_strlen(pos) > IFNAMSIZ) {
			wpa_printf(MSG_ERROR, "Invalid VLAN ifname at line %d "
				   "in '%s'", line, fname);
			fclose(f);
			return -1;
		}

		vlan = os_malloc(sizeof(*vlan));
		if (vlan == NULL) {
			wpa_printf(MSG_ERROR, "Out of memory while reading "
				   "VLAN interfaces from '%s'", fname);
			fclose(f);
			return -1;
		}

		os_memset(vlan, 0, sizeof(*vlan));
		vlan->vlan_id = vlan_id;
		os_strlcpy(vlan->ifname, pos, sizeof(vlan->ifname));
		if (bss->vlan_tail)
			bss->vlan_tail->next = vlan;
		else
			bss->vlan = vlan;
		bss->vlan_tail = vlan;
	}

	fclose(f);

	return 0;
}
#endif /* CONFIG_NO_VLAN */


static int hostapd_acl_comp(const void *a, const void *b)
{
	const struct mac_acl_entry *aa = a;
	const struct mac_acl_entry *bb = b;
	return os_memcmp(aa->addr, bb->addr, sizeof(macaddr));
}


static int hostapd_config_read_maclist(const char *fname,
				       struct mac_acl_entry **acl, int *num)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0;
	u8 addr[ETH_ALEN];
	struct mac_acl_entry *newacl;
	int vlan_id;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "MAC list file '%s' not found.", fname);
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
			wpa_printf(MSG_ERROR, "Invalid MAC address '%s' at "
				   "line %d in '%s'", buf, line, fname);
			fclose(f);
			return -1;
		}

		vlan_id = 0;
		pos = buf;
		while (*pos != '\0' && *pos != ' ' && *pos != '\t')
			pos++;
		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos != '\0')
			vlan_id = atoi(pos);

		newacl = os_realloc_array(*acl, *num + 1, sizeof(**acl));
		if (newacl == NULL) {
			wpa_printf(MSG_ERROR, "MAC list reallocation failed");
			fclose(f);
			return -1;
		}

		*acl = newacl;
		os_memcpy((*acl)[*num].addr, addr, ETH_ALEN);
		(*acl)[*num].vlan_id = vlan_id;
		(*num)++;
	}

	fclose(f);

	qsort(*acl, *num, sizeof(**acl), hostapd_acl_comp);

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

	if (os_strncmp(fname, "sqlite:", 7) == 0) {
		os_free(conf->eap_user_sqlite);
		conf->eap_user_sqlite = os_strdup(fname + 7);
		return 0;
	}

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "EAP user file '%s' not found.", fname);
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
			wpa_printf(MSG_ERROR, "Invalid EAP identity (no \" in "
				   "start) on line %d in '%s'", line, fname);
			goto failed;
		}

		user = os_zalloc(sizeof(*user));
		if (user == NULL) {
			wpa_printf(MSG_ERROR, "EAP user allocation failed");
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
				wpa_printf(MSG_ERROR, "Invalid EAP identity "
					   "(no \" in end) on line %d in '%s'",
					   line, fname);
				goto failed;
			}

			user->identity = os_malloc(pos - start);
			if (user->identity == NULL) {
				wpa_printf(MSG_ERROR, "Failed to allocate "
					   "memory for EAP identity");
				goto failed;
			}
			os_memcpy(user->identity, start, pos - start);
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
			wpa_printf(MSG_ERROR, "No EAP method on line %d in "
				   "'%s'", line, fname);
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
			char *pos3 = os_strchr(start, ',');
			if (pos3) {
				*pos3++ = '\0';
			}
			user->methods[num_methods].method =
				eap_server_get_type(
					start,
					&user->methods[num_methods].vendor);
			if (user->methods[num_methods].vendor ==
			    EAP_VENDOR_IETF &&
			    user->methods[num_methods].method == EAP_TYPE_NONE)
			{
				if (os_strcmp(start, "TTLS-PAP") == 0) {
					user->ttls_auth |= EAP_TTLS_AUTH_PAP;
					goto skip_eap;
				}
				if (os_strcmp(start, "TTLS-CHAP") == 0) {
					user->ttls_auth |= EAP_TTLS_AUTH_CHAP;
					goto skip_eap;
				}
				if (os_strcmp(start, "TTLS-MSCHAP") == 0) {
					user->ttls_auth |=
						EAP_TTLS_AUTH_MSCHAP;
					goto skip_eap;
				}
				if (os_strcmp(start, "TTLS-MSCHAPV2") == 0) {
					user->ttls_auth |=
						EAP_TTLS_AUTH_MSCHAPV2;
					goto skip_eap;
				}
				wpa_printf(MSG_ERROR, "Unsupported EAP type "
					   "'%s' on line %d in '%s'",
					   start, line, fname);
				goto failed;
			}

			num_methods++;
			if (num_methods >= EAP_MAX_METHODS)
				break;
		skip_eap:
			if (pos3 == NULL)
				break;
			start = pos3;
		}
		if (num_methods == 0 && user->ttls_auth == 0) {
			wpa_printf(MSG_ERROR, "No EAP types configured on "
				   "line %d in '%s'", line, fname);
			goto failed;
		}

		if (pos == NULL)
			goto done;

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos == '\0')
			goto done;

		if (os_strncmp(pos, "[ver=0]", 7) == 0) {
			user->force_version = 0;
			goto done;
		}

		if (os_strncmp(pos, "[ver=1]", 7) == 0) {
			user->force_version = 1;
			goto done;
		}

		if (os_strncmp(pos, "[2]", 3) == 0) {
			user->phase2 = 1;
			goto done;
		}

		if (*pos == '"') {
			pos++;
			start = pos;
			while (*pos != '"' && *pos != '\0')
				pos++;
			if (*pos == '\0') {
				wpa_printf(MSG_ERROR, "Invalid EAP password "
					   "(no \" in end) on line %d in '%s'",
					   line, fname);
				goto failed;
			}

			user->password = os_malloc(pos - start);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR, "Failed to allocate "
					   "memory for EAP password");
				goto failed;
			}
			os_memcpy(user->password, start, pos - start);
			user->password_len = pos - start;

			pos++;
		} else if (os_strncmp(pos, "hash:", 5) == 0) {
			pos += 5;
			pos2 = pos;
			while (*pos2 != '\0' && *pos2 != ' ' &&
			       *pos2 != '\t' && *pos2 != '#')
				pos2++;
			if (pos2 - pos != 32) {
				wpa_printf(MSG_ERROR, "Invalid password hash "
					   "on line %d in '%s'", line, fname);
				goto failed;
			}
			user->password = os_malloc(16);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR, "Failed to allocate "
					   "memory for EAP password hash");
				goto failed;
			}
			if (hexstr2bin(pos, user->password, 16) < 0) {
				wpa_printf(MSG_ERROR, "Invalid hash password "
					   "on line %d in '%s'", line, fname);
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
				wpa_printf(MSG_ERROR, "Invalid hex password "
					   "on line %d in '%s'", line, fname);
				goto failed;
			}
			user->password = os_malloc((pos2 - pos) / 2);
			if (user->password == NULL) {
				wpa_printf(MSG_ERROR, "Failed to allocate "
					   "memory for EAP password");
				goto failed;
			}
			if (hexstr2bin(pos, user->password,
				       (pos2 - pos) / 2) < 0) {
				wpa_printf(MSG_ERROR, "Invalid hex password "
					   "on line %d in '%s'", line, fname);
				goto failed;
			}
			user->password_len = (pos2 - pos) / 2;
			pos = pos2;
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (os_strncmp(pos, "[2]", 3) == 0) {
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
			os_free(user->password);
			os_free(user->identity);
			os_free(user);
		}
		ret = -1;
		break;
	}

	fclose(f);

	return ret;
}
#endif /* EAP_SERVER */


#ifndef CONFIG_NO_RADIUS
static int
hostapd_config_read_radius_addr(struct hostapd_radius_server **server,
				int *num_server, const char *val, int def_port,
				struct hostapd_radius_server **curr_serv)
{
	struct hostapd_radius_server *nserv;
	int ret;
	static int server_index = 1;

	nserv = os_realloc_array(*server, *num_server + 1, sizeof(*nserv));
	if (nserv == NULL)
		return -1;

	*server = nserv;
	nserv = &nserv[*num_server];
	(*num_server)++;
	(*curr_serv) = nserv;

	os_memset(nserv, 0, sizeof(*nserv));
	nserv->port = def_port;
	ret = hostapd_parse_ip_addr(val, &nserv->addr);
	nserv->index = server_index++;

	return ret;
}


static struct hostapd_radius_attr *
hostapd_parse_radius_attr(const char *value)
{
	const char *pos;
	char syntax;
	struct hostapd_radius_attr *attr;
	size_t len;

	attr = os_zalloc(sizeof(*attr));
	if (attr == NULL)
		return NULL;

	attr->type = atoi(value);

	pos = os_strchr(value, ':');
	if (pos == NULL) {
		attr->val = wpabuf_alloc(1);
		if (attr->val == NULL) {
			os_free(attr);
			return NULL;
		}
		wpabuf_put_u8(attr->val, 0);
		return attr;
	}

	pos++;
	if (pos[0] == '\0' || pos[1] != ':') {
		os_free(attr);
		return NULL;
	}
	syntax = *pos++;
	pos++;

	switch (syntax) {
	case 's':
		attr->val = wpabuf_alloc_copy(pos, os_strlen(pos));
		break;
	case 'x':
		len = os_strlen(pos);
		if (len & 1)
			break;
		len /= 2;
		attr->val = wpabuf_alloc(len);
		if (attr->val == NULL)
			break;
		if (hexstr2bin(pos, wpabuf_put(attr->val, len), len) < 0) {
			wpabuf_free(attr->val);
			os_free(attr);
			return NULL;
		}
		break;
	case 'd':
		attr->val = wpabuf_alloc(4);
		if (attr->val)
			wpabuf_put_be32(attr->val, atoi(pos));
		break;
	default:
		os_free(attr);
		return NULL;
	}

	if (attr->val == NULL) {
		os_free(attr);
		return NULL;
	}

	return attr;
}


static int hostapd_parse_das_client(struct hostapd_bss_config *bss,
				    const char *val)
{
	char *secret;

	secret = os_strchr(val, ' ');
	if (secret == NULL)
		return -1;

	secret++;

	if (hostapd_parse_ip_addr(val, &bss->radius_das_client_addr))
		return -1;

	os_free(bss->radius_das_shared_secret);
	bss->radius_das_shared_secret = (u8 *) os_strdup(secret);
	if (bss->radius_das_shared_secret == NULL)
		return -1;
	bss->radius_das_shared_secret_len = os_strlen(secret);

	return 0;
}
#endif /* CONFIG_NO_RADIUS */


static int hostapd_config_parse_key_mgmt(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = os_strdup(value);
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
		if (os_strcmp(start, "WPA-PSK") == 0)
			val |= WPA_KEY_MGMT_PSK;
		else if (os_strcmp(start, "WPA-EAP") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X;
#ifdef CONFIG_IEEE80211R
		else if (os_strcmp(start, "FT-PSK") == 0)
			val |= WPA_KEY_MGMT_FT_PSK;
		else if (os_strcmp(start, "FT-EAP") == 0)
			val |= WPA_KEY_MGMT_FT_IEEE8021X;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
		else if (os_strcmp(start, "WPA-PSK-SHA256") == 0)
			val |= WPA_KEY_MGMT_PSK_SHA256;
		else if (os_strcmp(start, "WPA-EAP-SHA256") == 0)
			val |= WPA_KEY_MGMT_IEEE8021X_SHA256;
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_SAE
		else if (os_strcmp(start, "SAE") == 0)
			val |= WPA_KEY_MGMT_SAE;
		else if (os_strcmp(start, "FT-SAE") == 0)
			val |= WPA_KEY_MGMT_FT_SAE;
#endif /* CONFIG_SAE */
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid key_mgmt '%s'",
				   line, start);
			os_free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}

	os_free(buf);
	if (val == 0) {
		wpa_printf(MSG_ERROR, "Line %d: no key_mgmt values "
			   "configured.", line);
		return -1;
	}

	return val;
}


static int hostapd_config_parse_cipher(int line, const char *value)
{
	int val = 0, last;
	char *start, *end, *buf;

	buf = os_strdup(value);
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
		if (os_strcmp(start, "CCMP") == 0)
			val |= WPA_CIPHER_CCMP;
		else if (os_strcmp(start, "GCMP") == 0)
			val |= WPA_CIPHER_GCMP;
		else if (os_strcmp(start, "TKIP") == 0)
			val |= WPA_CIPHER_TKIP;
		else if (os_strcmp(start, "WEP104") == 0)
			val |= WPA_CIPHER_WEP104;
		else if (os_strcmp(start, "WEP40") == 0)
			val |= WPA_CIPHER_WEP40;
		else if (os_strcmp(start, "NONE") == 0)
			val |= WPA_CIPHER_NONE;
		else {
			wpa_printf(MSG_ERROR, "Line %d: invalid cipher '%s'.",
				   line, start);
			os_free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}
	os_free(buf);

	if (val == 0) {
		wpa_printf(MSG_ERROR, "Line %d: no cipher values configured.",
			   line);
		return -1;
	}
	return val;
}


static int hostapd_config_read_wep(struct hostapd_wep_keys *wep, int keyidx,
				   char *val)
{
	size_t len = os_strlen(val);

	if (keyidx < 0 || keyidx > 3 || wep->key[keyidx] != NULL)
		return -1;

	if (val[0] == '"') {
		if (len < 2 || val[len - 1] != '"')
			return -1;
		len -= 2;
		wep->key[keyidx] = os_malloc(len);
		if (wep->key[keyidx] == NULL)
			return -1;
		os_memcpy(wep->key[keyidx], val + 1, len);
		wep->len[keyidx] = len;
	} else {
		if (len & 1)
			return -1;
		len /= 2;
		wep->key[keyidx] = os_malloc(len);
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

	os_free(*rate_list);
	*rate_list = NULL;

	pos = val;
	count = 0;
	while (*pos != '\0') {
		if (*pos == ' ')
			count++;
		pos++;
	}

	list = os_malloc(sizeof(int) * (count + 2));
	if (list == NULL)
		return -1;
	pos = val;
	count = 0;
	while (*pos != '\0') {
		end = os_strchr(pos, ' ');
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

	bss = os_realloc_array(conf->bss, conf->num_bss + 1,
			       sizeof(struct hostapd_bss_config));
	if (bss == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for "
			   "multi-BSS entry");
		return -1;
	}
	conf->bss = bss;

	bss = &(conf->bss[conf->num_bss]);
	os_memset(bss, 0, sizeof(*bss));
	bss->radius = os_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for "
			   "multi-BSS RADIUS data");
		return -1;
	}

	conf->num_bss++;
	conf->last_bss = bss;

	hostapd_config_defaults_bss(bss);
	os_strlcpy(bss->iface, ifname, sizeof(bss->iface));
	os_memcpy(bss->ssid.vlan, bss->iface, IFNAMSIZ + 1);

	return 0;
}


/* convert floats with one decimal place to value*10 int, i.e.,
 * "1.5" will return 15 */
static int hostapd_config_read_int10(const char *value)
{
	int i, d;
	char *pos;

	i = atoi(value);
	pos = os_strchr(value, '.');
	d = 0;
	if (pos) {
		pos++;
		if (*pos >= '0' && *pos <= '9')
			d = *pos - '0';
	}

	return i * 10 + d;
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
	IEEE80211_TX_QUEUE_DATA3 = 3 /* used for EDCA AC_BK data */
};

static int hostapd_config_tx_queue(struct hostapd_config *conf, char *name,
				   char *val)
{
	int num;
	char *pos;
	struct hostapd_tx_queue_params *queue;

	/* skip 'tx_queue_' prefix */
	pos = name + 9;
	if (os_strncmp(pos, "data", 4) == 0 &&
	    pos[4] >= '0' && pos[4] <= '9' && pos[5] == '_') {
		num = pos[4] - '0';
		pos += 6;
	} else if (os_strncmp(pos, "after_beacon_", 13) == 0 ||
		   os_strncmp(pos, "beacon_", 7) == 0) {
		wpa_printf(MSG_INFO, "DEPRECATED: '%s' not used", name);
		return 0;
	} else {
		wpa_printf(MSG_ERROR, "Unknown tx_queue name '%s'", pos);
		return -1;
	}

	if (num >= NUM_TX_QUEUES) {
		/* for backwards compatibility, do not trigger failure */
		wpa_printf(MSG_INFO, "DEPRECATED: '%s' not used", name);
		return 0;
	}

	queue = &conf->tx_queue[num];

	if (os_strcmp(pos, "aifs") == 0) {
		queue->aifs = atoi(val);
		if (queue->aifs < 0 || queue->aifs > 255) {
			wpa_printf(MSG_ERROR, "Invalid AIFS value %d",
				   queue->aifs);
			return -1;
		}
	} else if (os_strcmp(pos, "cwmin") == 0) {
		queue->cwmin = atoi(val);
		if (!valid_cw(queue->cwmin)) {
			wpa_printf(MSG_ERROR, "Invalid cwMin value %d",
				   queue->cwmin);
			return -1;
		}
	} else if (os_strcmp(pos, "cwmax") == 0) {
		queue->cwmax = atoi(val);
		if (!valid_cw(queue->cwmax)) {
			wpa_printf(MSG_ERROR, "Invalid cwMax value %d",
				   queue->cwmax);
			return -1;
		}
	} else if (os_strcmp(pos, "burst") == 0) {
		queue->burst = hostapd_config_read_int10(val);
	} else {
		wpa_printf(MSG_ERROR, "Unknown tx_queue field '%s'", pos);
		return -1;
	}

	return 0;
}


#ifdef CONFIG_IEEE80211R
static int add_r0kh(struct hostapd_bss_config *bss, char *value)
{
	struct ft_remote_r0kh *r0kh;
	char *pos, *next;

	r0kh = os_zalloc(sizeof(*r0kh));
	if (r0kh == NULL)
		return -1;

	/* 02:01:02:03:04:05 a.example.com 000102030405060708090a0b0c0d0e0f */
	pos = value;
	next = os_strchr(pos, ' ');
	if (next)
		*next++ = '\0';
	if (next == NULL || hwaddr_aton(pos, r0kh->addr)) {
		wpa_printf(MSG_ERROR, "Invalid R0KH MAC address: '%s'", pos);
		os_free(r0kh);
		return -1;
	}

	pos = next;
	next = os_strchr(pos, ' ');
	if (next)
		*next++ = '\0';
	if (next == NULL || next - pos > FT_R0KH_ID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "Invalid R0KH-ID: '%s'", pos);
		os_free(r0kh);
		return -1;
	}
	r0kh->id_len = next - pos - 1;
	os_memcpy(r0kh->id, pos, r0kh->id_len);

	pos = next;
	if (hexstr2bin(pos, r0kh->key, sizeof(r0kh->key))) {
		wpa_printf(MSG_ERROR, "Invalid R0KH key: '%s'", pos);
		os_free(r0kh);
		return -1;
	}

	r0kh->next = bss->r0kh_list;
	bss->r0kh_list = r0kh;

	return 0;
}


static int add_r1kh(struct hostapd_bss_config *bss, char *value)
{
	struct ft_remote_r1kh *r1kh;
	char *pos, *next;

	r1kh = os_zalloc(sizeof(*r1kh));
	if (r1kh == NULL)
		return -1;

	/* 02:01:02:03:04:05 02:01:02:03:04:05
	 * 000102030405060708090a0b0c0d0e0f */
	pos = value;
	next = os_strchr(pos, ' ');
	if (next)
		*next++ = '\0';
	if (next == NULL || hwaddr_aton(pos, r1kh->addr)) {
		wpa_printf(MSG_ERROR, "Invalid R1KH MAC address: '%s'", pos);
		os_free(r1kh);
		return -1;
	}

	pos = next;
	next = os_strchr(pos, ' ');
	if (next)
		*next++ = '\0';
	if (next == NULL || hwaddr_aton(pos, r1kh->id)) {
		wpa_printf(MSG_ERROR, "Invalid R1KH-ID: '%s'", pos);
		os_free(r1kh);
		return -1;
	}

	pos = next;
	if (hexstr2bin(pos, r1kh->key, sizeof(r1kh->key))) {
		wpa_printf(MSG_ERROR, "Invalid R1KH key: '%s'", pos);
		os_free(r1kh);
		return -1;
	}

	r1kh->next = bss->r1kh_list;
	bss->r1kh_list = r1kh;

	return 0;
}
#endif /* CONFIG_IEEE80211R */


#ifdef CONFIG_IEEE80211N
static int hostapd_config_ht_capab(struct hostapd_config *conf,
				   const char *capab)
{
	if (os_strstr(capab, "[LDPC]"))
		conf->ht_capab |= HT_CAP_INFO_LDPC_CODING_CAP;
	if (os_strstr(capab, "[HT40-]")) {
		conf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		conf->secondary_channel = -1;
	}
	if (os_strstr(capab, "[HT40+]")) {
		conf->ht_capab |= HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		conf->secondary_channel = 1;
	}
	if (os_strstr(capab, "[SMPS-STATIC]")) {
		conf->ht_capab &= ~HT_CAP_INFO_SMPS_MASK;
		conf->ht_capab |= HT_CAP_INFO_SMPS_STATIC;
	}
	if (os_strstr(capab, "[SMPS-DYNAMIC]")) {
		conf->ht_capab &= ~HT_CAP_INFO_SMPS_MASK;
		conf->ht_capab |= HT_CAP_INFO_SMPS_DYNAMIC;
	}
	if (os_strstr(capab, "[GF]"))
		conf->ht_capab |= HT_CAP_INFO_GREEN_FIELD;
	if (os_strstr(capab, "[SHORT-GI-20]"))
		conf->ht_capab |= HT_CAP_INFO_SHORT_GI20MHZ;
	if (os_strstr(capab, "[SHORT-GI-40]"))
		conf->ht_capab |= HT_CAP_INFO_SHORT_GI40MHZ;
	if (os_strstr(capab, "[TX-STBC]"))
		conf->ht_capab |= HT_CAP_INFO_TX_STBC;
	if (os_strstr(capab, "[RX-STBC1]")) {
		conf->ht_capab &= ~HT_CAP_INFO_RX_STBC_MASK;
		conf->ht_capab |= HT_CAP_INFO_RX_STBC_1;
	}
	if (os_strstr(capab, "[RX-STBC12]")) {
		conf->ht_capab &= ~HT_CAP_INFO_RX_STBC_MASK;
		conf->ht_capab |= HT_CAP_INFO_RX_STBC_12;
	}
	if (os_strstr(capab, "[RX-STBC123]")) {
		conf->ht_capab &= ~HT_CAP_INFO_RX_STBC_MASK;
		conf->ht_capab |= HT_CAP_INFO_RX_STBC_123;
	}
	if (os_strstr(capab, "[DELAYED-BA]"))
		conf->ht_capab |= HT_CAP_INFO_DELAYED_BA;
	if (os_strstr(capab, "[MAX-AMSDU-7935]"))
		conf->ht_capab |= HT_CAP_INFO_MAX_AMSDU_SIZE;
	if (os_strstr(capab, "[DSSS_CCK-40]"))
		conf->ht_capab |= HT_CAP_INFO_DSSS_CCK40MHZ;
	if (os_strstr(capab, "[PSMP]"))
		conf->ht_capab |= HT_CAP_INFO_PSMP_SUPP;
	if (os_strstr(capab, "[LSIG-TXOP-PROT]"))
		conf->ht_capab |= HT_CAP_INFO_LSIG_TXOP_PROTECT_SUPPORT;

	return 0;
}
#endif /* CONFIG_IEEE80211N */


#ifdef CONFIG_IEEE80211AC
static int hostapd_config_vht_capab(struct hostapd_config *conf,
				    const char *capab)
{
	if (os_strstr(capab, "[MAX-MPDU-7991]"))
		conf->vht_capab |= VHT_CAP_MAX_MPDU_LENGTH_7991;
	if (os_strstr(capab, "[MAX-MPDU-11454]"))
		conf->vht_capab |= VHT_CAP_MAX_MPDU_LENGTH_11454;
	if (os_strstr(capab, "[VHT160]"))
		conf->vht_capab |= VHT_CAP_SUPP_CHAN_WIDTH_160MHZ;
	if (os_strstr(capab, "[VHT160-80PLUS80]"))
		conf->vht_capab |= VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
	if (os_strstr(capab, "[VHT160-80PLUS80]"))
		conf->vht_capab |= VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ;
	if (os_strstr(capab, "[RXLDPC]"))
		conf->vht_capab |= VHT_CAP_RXLDPC;
	if (os_strstr(capab, "[SHORT-GI-80]"))
		conf->vht_capab |= VHT_CAP_SHORT_GI_80;
	if (os_strstr(capab, "[SHORT-GI-160]"))
		conf->vht_capab |= VHT_CAP_SHORT_GI_160;
	if (os_strstr(capab, "[TX-STBC-2BY1]"))
		conf->vht_capab |= VHT_CAP_TXSTBC;
	if (os_strstr(capab, "[RX-STBC-1]"))
		conf->vht_capab |= VHT_CAP_RXSTBC_1;
	if (os_strstr(capab, "[RX-STBC-12]"))
		conf->vht_capab |= VHT_CAP_RXSTBC_2;
	if (os_strstr(capab, "[RX-STBC-123]"))
		conf->vht_capab |= VHT_CAP_RXSTBC_3;
	if (os_strstr(capab, "[RX-STBC-1234]"))
		conf->vht_capab |= VHT_CAP_RXSTBC_4;
	if (os_strstr(capab, "[SU-BEAMFORMER]"))
		conf->vht_capab |= VHT_CAP_MU_BEAMFORMER_CAPABLE;
	if (os_strstr(capab, "[SU-BEAMFORMEE]"))
		conf->vht_capab |= VHT_CAP_MU_BEAMFORMEE_CAPABLE;
	if (os_strstr(capab, "[BF-ANTENNA-2]") &&
	    (conf->vht_capab & VHT_CAP_MU_BEAMFORMER_CAPABLE))
		conf->vht_capab |= VHT_CAP_BEAMFORMER_ANTENNAS_MAX;
	if (os_strstr(capab, "[SOUNDING-DIMENSION-2]") &&
	    (conf->vht_capab & VHT_CAP_MU_BEAMFORMER_CAPABLE))
		conf->vht_capab |= VHT_CAP_SOUNDING_DIMENTION_MAX;
	if (os_strstr(capab, "[MU-BEAMFORMER]"))
		conf->vht_capab |= VHT_CAP_MU_BEAMFORMER_CAPABLE;
	if (os_strstr(capab, "[MU-BEAMFORMEE]"))
		conf->vht_capab |= VHT_CAP_MU_BEAMFORMEE_CAPABLE;
	if (os_strstr(capab, "[VHT-TXOP-PS]"))
		conf->vht_capab |= VHT_CAP_VHT_TXOP_PS;
	if (os_strstr(capab, "[HTC-VHT]"))
		conf->vht_capab |= VHT_CAP_HTC_VHT;
	if (os_strstr(capab, "[MAX-A-MPDU-LEN-EXP0]"))
		conf->vht_capab |= VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT;
	if (os_strstr(capab, "[VHT-LINK-ADAPT2]") &&
	    (conf->vht_capab & VHT_CAP_HTC_VHT))
		conf->vht_capab |= VHT_CAP_VHT_LINK_ADAPTATION_VHT_UNSOL_MFB;
	if (os_strstr(capab, "[VHT-LINK-ADAPT3]") &&
	    (conf->vht_capab & VHT_CAP_HTC_VHT))
		conf->vht_capab |= VHT_CAP_VHT_LINK_ADAPTATION_VHT_MRQ_MFB;
	if (os_strstr(capab, "[RX-ANTENNA-PATTERN]"))
		conf->vht_capab |= VHT_CAP_RX_ANTENNA_PATTERN;
	if (os_strstr(capab, "[TX-ANTENNA-PATTERN]"))
		conf->vht_capab |= VHT_CAP_TX_ANTENNA_PATTERN;
	return 0;
}
#endif /* CONFIG_IEEE80211AC */


static int hostapd_config_check_bss(struct hostapd_bss_config *bss,
				    struct hostapd_config *conf)
{
	if (bss->ieee802_1x && !bss->eap_server &&
	    !bss->radius->auth_servers) {
		wpa_printf(MSG_ERROR, "Invalid IEEE 802.1X configuration (no "
			   "EAP authenticator configured).");
		return -1;
	}

	if (bss->wpa && bss->wpa_psk_radius != PSK_RADIUS_IGNORED &&
	    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
		wpa_printf(MSG_ERROR, "WPA-PSK using RADIUS enabled, but no "
			   "RADIUS checking (macaddr_acl=2) enabled.");
		return -1;
	}

	if (bss->wpa && (bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL &&
	    (bss->wpa_psk_radius != PSK_RADIUS_REQUIRED ||
	     bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH)) {
		wpa_printf(MSG_ERROR, "WPA-PSK enabled, but PSK or passphrase "
			   "is not configured.");
		return -1;
	}

	if (hostapd_mac_comp_empty(bss->bssid) != 0) {
		size_t i;

		for (i = 0; i < conf->num_bss; i++) {
			if ((&conf->bss[i] != bss) &&
			    (hostapd_mac_comp(conf->bss[i].bssid,
					      bss->bssid) == 0)) {
				wpa_printf(MSG_ERROR, "Duplicate BSSID " MACSTR
					   " on interface '%s' and '%s'.",
					   MAC2STR(bss->bssid),
					   conf->bss[i].iface, bss->iface);
				return -1;
			}
		}
	}

#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(bss->wpa_key_mgmt) &&
	    (bss->nas_identifier == NULL ||
	     os_strlen(bss->nas_identifier) < 1 ||
	     os_strlen(bss->nas_identifier) > FT_R0KH_ID_MAX_LEN)) {
		wpa_printf(MSG_ERROR, "FT (IEEE 802.11r) requires "
			   "nas_identifier to be configured as a 1..48 octet "
			   "string");
		return -1;
	}
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_IEEE80211N
	if (conf->ieee80211n && conf->hw_mode == HOSTAPD_MODE_IEEE80211B) {
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) in 11b mode is not "
			   "allowed, disabling HT capabilites");
	}

	if (conf->ieee80211n &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WEP is not "
			   "allowed, disabling HT capabilities");
	}

	if (conf->ieee80211n && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP))) {
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WPA/WPA2 "
			   "requires CCMP/GCMP to be enabled, disabling HT "
			   "capabilities");
	}
#endif /* CONFIG_IEEE80211N */

#ifdef CONFIG_WPS2
	if (bss->wps_state && bss->ignore_broadcast_ssid) {
		wpa_printf(MSG_INFO, "WPS: ignore_broadcast_ssid "
			   "configuration forced WPS to be disabled");
		bss->wps_state = 0;
	}

	if (bss->wps_state && bss->ssid.wep.keys_set && bss->wpa == 0) {
		wpa_printf(MSG_INFO, "WPS: WEP configuration forced WPS to be "
			   "disabled");
		bss->wps_state = 0;
	}

	if (bss->wps_state && bss->wpa &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & WPA_CIPHER_CCMP))) {
		wpa_printf(MSG_INFO, "WPS: WPA/TKIP configuration without "
			   "WPA2/CCMP forced WPS to be disabled");
		bss->wps_state = 0;
	}
#endif /* CONFIG_WPS2 */

#ifdef CONFIG_HS20
	if (bss->hs20 &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP)))) {
		wpa_printf(MSG_ERROR, "HS 2.0: WPA2-Enterprise/CCMP "
			   "configuration is required for Hotspot 2.0 "
			   "functionality");
		return -1;
	}
#endif /* CONFIG_HS20 */

	return 0;
}


static int hostapd_config_check(struct hostapd_config *conf)
{
	size_t i;

	if (conf->ieee80211d && (!conf->country[0] || !conf->country[1])) {
		wpa_printf(MSG_ERROR, "Cannot enable IEEE 802.11d without "
			   "setting the country_code");
		return -1;
	}

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_config_check_bss(&conf->bss[i], conf))
			return -1;
	}

	return 0;
}


#ifdef CONFIG_INTERWORKING
static int parse_roaming_consortium(struct hostapd_bss_config *bss, char *pos,
				    int line)
{
	size_t len = os_strlen(pos);
	u8 oi[MAX_ROAMING_CONSORTIUM_LEN];

	struct hostapd_roaming_consortium *rc;

	if ((len & 1) || len < 2 * 3 || len / 2 > MAX_ROAMING_CONSORTIUM_LEN ||
	    hexstr2bin(pos, oi, len / 2)) {
		wpa_printf(MSG_ERROR, "Line %d: invalid roaming_consortium "
			   "'%s'", line, pos);
		return -1;
	}
	len /= 2;

	rc = os_realloc_array(bss->roaming_consortium,
			      bss->roaming_consortium_count + 1,
			      sizeof(struct hostapd_roaming_consortium));
	if (rc == NULL)
		return -1;

	os_memcpy(rc[bss->roaming_consortium_count].oi, oi, len);
	rc[bss->roaming_consortium_count].len = len;

	bss->roaming_consortium = rc;
	bss->roaming_consortium_count++;

	return 0;
}


static int parse_lang_string(struct hostapd_lang_string **array,
			     unsigned int *count, char *pos)
{
	char *sep;
	size_t clen, nlen;
	struct hostapd_lang_string *ls;

	sep = os_strchr(pos, ':');
	if (sep == NULL)
		return -1;
	*sep++ = '\0';

	clen = os_strlen(pos);
	if (clen < 2)
		return -1;
	nlen = os_strlen(sep);
	if (nlen > 252)
		return -1;

	ls = os_realloc_array(*array, *count + 1,
			      sizeof(struct hostapd_lang_string));
	if (ls == NULL)
		return -1;

	*array = ls;
	ls = &(*array)[*count];
	(*count)++;

	os_memset(ls->lang, 0, sizeof(ls->lang));
	os_memcpy(ls->lang, pos, clen);
	ls->name_len = nlen;
	os_memcpy(ls->name, sep, nlen);

	return 0;
}


static int parse_venue_name(struct hostapd_bss_config *bss, char *pos,
			    int line)
{
	if (parse_lang_string(&bss->venue_name, &bss->venue_name_count, pos)) {
		wpa_printf(MSG_ERROR, "Line %d: Invalid venue_name '%s'",
			   line, pos);
		return -1;
	}
	return 0;
}


static int parse_3gpp_cell_net(struct hostapd_bss_config *bss, char *buf,
			       int line)
{
	size_t count;
	char *pos;
	u8 *info = NULL, *ipos;

	/* format: <MCC1,MNC1>[;<MCC2,MNC2>][;...] */

	count = 1;
	for (pos = buf; *pos; pos++) {
		if ((*pos < '0' && *pos > '9') && *pos != ';' && *pos != ',')
			goto fail;
		if (*pos == ';')
			count++;
	}
	if (1 + count * 3 > 0x7f)
		goto fail;

	info = os_zalloc(2 + 3 + count * 3);
	if (info == NULL)
		return -1;

	ipos = info;
	*ipos++ = 0; /* GUD - Version 1 */
	*ipos++ = 3 + count * 3; /* User Data Header Length (UDHL) */
	*ipos++ = 0; /* PLMN List IEI */
	/* ext(b8) | Length of PLMN List value contents(b7..1) */
	*ipos++ = 1 + count * 3;
	*ipos++ = count; /* Number of PLMNs */

	pos = buf;
	while (pos && *pos) {
		char *mcc, *mnc;
		size_t mnc_len;

		mcc = pos;
		mnc = os_strchr(pos, ',');
		if (mnc == NULL)
			goto fail;
		*mnc++ = '\0';
		pos = os_strchr(mnc, ';');
		if (pos)
			*pos++ = '\0';

		mnc_len = os_strlen(mnc);
		if (os_strlen(mcc) != 3 || (mnc_len != 2 && mnc_len != 3))
			goto fail;

		/* BC coded MCC,MNC */
		/* MCC digit 2 | MCC digit 1 */
		*ipos++ = ((mcc[1] - '0') << 4) | (mcc[0] - '0');
		/* MNC digit 3 | MCC digit 3 */
		*ipos++ = (((mnc_len == 2) ? 0xf0 : ((mnc[2] - '0') << 4))) |
			(mcc[2] - '0');
		/* MNC digit 2 | MNC digit 1 */
		*ipos++ = ((mnc[1] - '0') << 4) | (mnc[0] - '0');
	}

	os_free(bss->anqp_3gpp_cell_net);
	bss->anqp_3gpp_cell_net = info;
	bss->anqp_3gpp_cell_net_len = 2 + 3 + 3 * count;
	wpa_hexdump(MSG_MSGDUMP, "3GPP Cellular Network information",
		    bss->anqp_3gpp_cell_net, bss->anqp_3gpp_cell_net_len);

	return 0;

fail:
	wpa_printf(MSG_ERROR, "Line %d: Invalid anqp_3gpp_cell_net: %s",
		   line, buf);
	os_free(info);
	return -1;
}


static int parse_nai_realm(struct hostapd_bss_config *bss, char *buf, int line)
{
	struct hostapd_nai_realm_data *realm;
	size_t i, j, len;
	int *offsets;
	char *pos, *end, *rpos;

	offsets = os_calloc(bss->nai_realm_count * MAX_NAI_REALMS,
			    sizeof(int));
	if (offsets == NULL)
		return -1;

	for (i = 0; i < bss->nai_realm_count; i++) {
		realm = &bss->nai_realm_data[i];
		for (j = 0; j < MAX_NAI_REALMS; j++) {
			offsets[i * MAX_NAI_REALMS + j] =
				realm->realm[j] ?
				realm->realm[j] - realm->realm_buf : -1;
		}
	}

	realm = os_realloc_array(bss->nai_realm_data, bss->nai_realm_count + 1,
				 sizeof(struct hostapd_nai_realm_data));
	if (realm == NULL) {
		os_free(offsets);
		return -1;
	}
	bss->nai_realm_data = realm;

	/* patch the pointers after realloc */
	for (i = 0; i < bss->nai_realm_count; i++) {
		realm = &bss->nai_realm_data[i];
		for (j = 0; j < MAX_NAI_REALMS; j++) {
			int offs = offsets[i * MAX_NAI_REALMS + j];
			if (offs >= 0)
				realm->realm[j] = realm->realm_buf + offs;
			else
				realm->realm[j] = NULL;
		}
	}
	os_free(offsets);

	realm = &bss->nai_realm_data[bss->nai_realm_count];
	os_memset(realm, 0, sizeof(*realm));

	pos = buf;
	realm->encoding = atoi(pos);
	pos = os_strchr(pos, ',');
	if (pos == NULL)
		goto fail;
	pos++;

	end = os_strchr(pos, ',');
	if (end) {
		len = end - pos;
		*end = '\0';
	} else {
		len = os_strlen(pos);
	}

	if (len > MAX_NAI_REALMLEN) {
		wpa_printf(MSG_ERROR, "Too long a realm string (%d > max %d "
			   "characters)", (int) len, MAX_NAI_REALMLEN);
		goto fail;
	}
	os_memcpy(realm->realm_buf, pos, len);

	if (end)
		pos = end + 1;
	else
		pos = NULL;

	while (pos && *pos) {
		struct hostapd_nai_realm_eap *eap;

		if (realm->eap_method_count >= MAX_NAI_EAP_METHODS) {
			wpa_printf(MSG_ERROR, "Too many EAP methods");
			goto fail;
		}

		eap = &realm->eap_method[realm->eap_method_count];
		realm->eap_method_count++;

		end = os_strchr(pos, ',');
		if (end == NULL)
			end = pos + os_strlen(pos);

		eap->eap_method = atoi(pos);
		for (;;) {
			pos = os_strchr(pos, '[');
			if (pos == NULL || pos > end)
				break;
			pos++;
			if (eap->num_auths >= MAX_NAI_AUTH_TYPES) {
				wpa_printf(MSG_ERROR, "Too many auth params");
				goto fail;
			}
			eap->auth_id[eap->num_auths] = atoi(pos);
			pos = os_strchr(pos, ':');
			if (pos == NULL || pos > end)
				goto fail;
			pos++;
			eap->auth_val[eap->num_auths] = atoi(pos);
			pos = os_strchr(pos, ']');
			if (pos == NULL || pos > end)
				goto fail;
			pos++;
			eap->num_auths++;
		}

		if (*end != ',')
			break;

		pos = end + 1;
	}

	/* Split realm list into null terminated realms */
	rpos = realm->realm_buf;
	i = 0;
	while (*rpos) {
		if (i >= MAX_NAI_REALMS) {
			wpa_printf(MSG_ERROR, "Too many realms");
			goto fail;
		}
		realm->realm[i++] = rpos;
		rpos = os_strchr(rpos, ';');
		if (rpos == NULL)
			break;
		*rpos++ = '\0';
	}

	bss->nai_realm_count++;

	return 0;

fail:
	wpa_printf(MSG_ERROR, "Line %d: invalid nai_realm '%s'", line, buf);
	return -1;
}

#endif /* CONFIG_INTERWORKING */


#ifdef CONFIG_HS20
static int hs20_parse_conn_capab(struct hostapd_bss_config *bss, char *buf,
				 int line)
{
	u8 *conn_cap;
	char *pos;

	if (bss->hs20_connection_capability_len >= 0xfff0)
		return -1;

	conn_cap = os_realloc(bss->hs20_connection_capability,
			      bss->hs20_connection_capability_len + 4);
	if (conn_cap == NULL)
		return -1;

	bss->hs20_connection_capability = conn_cap;
	conn_cap += bss->hs20_connection_capability_len;
	pos = buf;
	conn_cap[0] = atoi(pos);
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		return -1;
	pos++;
	WPA_PUT_LE16(conn_cap + 1, atoi(pos));
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		return -1;
	pos++;
	conn_cap[3] = atoi(pos);
	bss->hs20_connection_capability_len += 4;

	return 0;
}


static int hs20_parse_wan_metrics(struct hostapd_bss_config *bss, char *buf,
				  int line)
{
	u8 *wan_metrics;
	char *pos;

	/* <WAN Info>:<DL Speed>:<UL Speed>:<DL Load>:<UL Load>:<LMD> */

	wan_metrics = os_zalloc(13);
	if (wan_metrics == NULL)
		return -1;

	pos = buf;
	/* WAN Info */
	if (hexstr2bin(pos, wan_metrics, 1) < 0)
		goto fail;
	pos += 2;
	if (*pos != ':')
		goto fail;
	pos++;

	/* Downlink Speed */
	WPA_PUT_LE32(wan_metrics + 1, atoi(pos));
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		goto fail;
	pos++;

	/* Uplink Speed */
	WPA_PUT_LE32(wan_metrics + 5, atoi(pos));
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		goto fail;
	pos++;

	/* Downlink Load */
	wan_metrics[9] = atoi(pos);
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		goto fail;
	pos++;

	/* Uplink Load */
	wan_metrics[10] = atoi(pos);
	pos = os_strchr(pos, ':');
	if (pos == NULL)
		goto fail;
	pos++;

	/* LMD */
	WPA_PUT_LE16(wan_metrics + 11, atoi(pos));

	os_free(bss->hs20_wan_metrics);
	bss->hs20_wan_metrics = wan_metrics;

	return 0;

fail:
	wpa_printf(MSG_ERROR, "Line %d: Invalid hs20_wan_metrics '%s'",
		   line, pos);
	os_free(wan_metrics);
	return -1;
}


static int hs20_parse_oper_friendly_name(struct hostapd_bss_config *bss,
					 char *pos, int line)
{
	if (parse_lang_string(&bss->hs20_oper_friendly_name,
			      &bss->hs20_oper_friendly_name_count, pos)) {
		wpa_printf(MSG_ERROR, "Line %d: Invalid "
			   "hs20_oper_friendly_name '%s'", line, pos);
		return -1;
	}
	return 0;
}
#endif /* CONFIG_HS20 */


#ifdef CONFIG_WPS_NFC
static struct wpabuf * hostapd_parse_bin(const char *buf)
{
	size_t len;
	struct wpabuf *ret;

	len = os_strlen(buf);
	if (len & 0x01)
		return NULL;
	len /= 2;

	ret = wpabuf_alloc(len);
	if (ret == NULL)
		return NULL;

	if (hexstr2bin(buf, wpabuf_put(ret, len), len)) {
		wpabuf_free(ret);
		return NULL;
	}

	return ret;
}
#endif /* CONFIG_WPS_NFC */


static int hostapd_config_fill(struct hostapd_config *conf,
			       struct hostapd_bss_config *bss,
			       char *buf, char *pos, int line)
{
	int errors = 0;

	{
		if (os_strcmp(buf, "interface") == 0) {
			os_strlcpy(conf->bss[0].iface, pos,
				   sizeof(conf->bss[0].iface));
		} else if (os_strcmp(buf, "bridge") == 0) {
			os_strlcpy(bss->bridge, pos, sizeof(bss->bridge));
		} else if (os_strcmp(buf, "wds_bridge") == 0) {
			os_strlcpy(bss->wds_bridge, pos,
				   sizeof(bss->wds_bridge));
		} else if (os_strcmp(buf, "driver") == 0) {
			int j;
			/* clear to get error below if setting is invalid */
			conf->driver = NULL;
			for (j = 0; wpa_drivers[j]; j++) {
				if (os_strcmp(pos, wpa_drivers[j]->name) == 0)
				{
					conf->driver = wpa_drivers[j];
					break;
				}
			}
			if (conf->driver == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: invalid/"
					   "unknown driver '%s'", line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "debug") == 0) {
			wpa_printf(MSG_DEBUG, "Line %d: DEPRECATED: 'debug' "
				   "configuration variable is not used "
				   "anymore", line);
		} else if (os_strcmp(buf, "logger_syslog_level") == 0) {
			bss->logger_syslog_level = atoi(pos);
		} else if (os_strcmp(buf, "logger_stdout_level") == 0) {
			bss->logger_stdout_level = atoi(pos);
		} else if (os_strcmp(buf, "logger_syslog") == 0) {
			bss->logger_syslog = atoi(pos);
		} else if (os_strcmp(buf, "logger_stdout") == 0) {
			bss->logger_stdout = atoi(pos);
		} else if (os_strcmp(buf, "dump_file") == 0) {
			bss->dump_log_name = os_strdup(pos);
		} else if (os_strcmp(buf, "ssid") == 0) {
			bss->ssid.ssid_len = os_strlen(pos);
			if (bss->ssid.ssid_len > HOSTAPD_MAX_SSID_LEN ||
			    bss->ssid.ssid_len < 1) {
				wpa_printf(MSG_ERROR, "Line %d: invalid SSID "
					   "'%s'", line, pos);
				errors++;
			} else {
				os_memcpy(bss->ssid.ssid, pos,
					  bss->ssid.ssid_len);
				bss->ssid.ssid_set = 1;
			}
		} else if (os_strcmp(buf, "ssid2") == 0) {
			size_t slen;
			char *str = wpa_config_parse_string(pos, &slen);
			if (str == NULL || slen < 1 ||
				   slen > HOSTAPD_MAX_SSID_LEN) {
				wpa_printf(MSG_ERROR, "Line %d: invalid SSID "
					   "'%s'", line, pos);
				errors++;
			} else {
				os_memcpy(bss->ssid.ssid, str, slen);
				bss->ssid.ssid_len = slen;
				bss->ssid.ssid_set = 1;
			}
			os_free(str);
		} else if (os_strcmp(buf, "utf8_ssid") == 0) {
			bss->ssid.utf8_ssid = atoi(pos) > 0;
		} else if (os_strcmp(buf, "macaddr_acl") == 0) {
			bss->macaddr_acl = atoi(pos);
			if (bss->macaddr_acl != ACCEPT_UNLESS_DENIED &&
			    bss->macaddr_acl != DENY_UNLESS_ACCEPTED &&
			    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
				wpa_printf(MSG_ERROR, "Line %d: unknown "
					   "macaddr_acl %d",
					   line, bss->macaddr_acl);
			}
		} else if (os_strcmp(buf, "accept_mac_file") == 0) {
			if (hostapd_config_read_maclist(pos, &bss->accept_mac,
							&bss->num_accept_mac))
			{
				wpa_printf(MSG_ERROR, "Line %d: Failed to "
					   "read accept_mac_file '%s'",
					   line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "deny_mac_file") == 0) {
			if (hostapd_config_read_maclist(pos, &bss->deny_mac,
							&bss->num_deny_mac)) {
				wpa_printf(MSG_ERROR, "Line %d: Failed to "
					   "read deny_mac_file '%s'",
					   line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "wds_sta") == 0) {
			bss->wds_sta = atoi(pos);
		} else if (os_strcmp(buf, "ap_isolate") == 0) {
			bss->isolate = atoi(pos);
		} else if (os_strcmp(buf, "ap_max_inactivity") == 0) {
			bss->ap_max_inactivity = atoi(pos);
		} else if (os_strcmp(buf, "skip_inactivity_poll") == 0) {
			bss->skip_inactivity_poll = atoi(pos);
		} else if (os_strcmp(buf, "country_code") == 0) {
			os_memcpy(conf->country, pos, 2);
			/* FIX: make this configurable */
			conf->country[2] = ' ';
		} else if (os_strcmp(buf, "ieee80211d") == 0) {
			conf->ieee80211d = atoi(pos);
		} else if (os_strcmp(buf, "ieee8021x") == 0) {
			bss->ieee802_1x = atoi(pos);
		} else if (os_strcmp(buf, "eapol_version") == 0) {
			bss->eapol_version = atoi(pos);
			if (bss->eapol_version < 1 ||
			    bss->eapol_version > 2) {
				wpa_printf(MSG_ERROR, "Line %d: invalid EAPOL "
					   "version (%d): '%s'.",
					   line, bss->eapol_version, pos);
				errors++;
			} else
				wpa_printf(MSG_DEBUG, "eapol_version=%d",
					   bss->eapol_version);
#ifdef EAP_SERVER
		} else if (os_strcmp(buf, "eap_authenticator") == 0) {
			bss->eap_server = atoi(pos);
			wpa_printf(MSG_ERROR, "Line %d: obsolete "
				   "eap_authenticator used; this has been "
				   "renamed to eap_server", line);
		} else if (os_strcmp(buf, "eap_server") == 0) {
			bss->eap_server = atoi(pos);
		} else if (os_strcmp(buf, "eap_user_file") == 0) {
			if (hostapd_config_read_eap_user(pos, bss))
				errors++;
		} else if (os_strcmp(buf, "ca_cert") == 0) {
			os_free(bss->ca_cert);
			bss->ca_cert = os_strdup(pos);
		} else if (os_strcmp(buf, "server_cert") == 0) {
			os_free(bss->server_cert);
			bss->server_cert = os_strdup(pos);
		} else if (os_strcmp(buf, "private_key") == 0) {
			os_free(bss->private_key);
			bss->private_key = os_strdup(pos);
		} else if (os_strcmp(buf, "private_key_passwd") == 0) {
			os_free(bss->private_key_passwd);
			bss->private_key_passwd = os_strdup(pos);
		} else if (os_strcmp(buf, "check_crl") == 0) {
			bss->check_crl = atoi(pos);
		} else if (os_strcmp(buf, "dh_file") == 0) {
			os_free(bss->dh_file);
			bss->dh_file = os_strdup(pos);
		} else if (os_strcmp(buf, "fragment_size") == 0) {
			bss->fragment_size = atoi(pos);
#ifdef EAP_SERVER_FAST
		} else if (os_strcmp(buf, "pac_opaque_encr_key") == 0) {
			os_free(bss->pac_opaque_encr_key);
			bss->pac_opaque_encr_key = os_malloc(16);
			if (bss->pac_opaque_encr_key == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: No memory for "
					   "pac_opaque_encr_key", line);
				errors++;
			} else if (hexstr2bin(pos, bss->pac_opaque_encr_key,
					      16)) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "pac_opaque_encr_key", line);
				errors++;
			}
		} else if (os_strcmp(buf, "eap_fast_a_id") == 0) {
			size_t idlen = os_strlen(pos);
			if (idlen & 1) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "eap_fast_a_id", line);
				errors++;
			} else {
				os_free(bss->eap_fast_a_id);
				bss->eap_fast_a_id = os_malloc(idlen / 2);
				if (bss->eap_fast_a_id == NULL ||
				    hexstr2bin(pos, bss->eap_fast_a_id,
					       idlen / 2)) {
					wpa_printf(MSG_ERROR, "Line %d: "
						   "Failed to parse "
						   "eap_fast_a_id", line);
					errors++;
				} else
					bss->eap_fast_a_id_len = idlen / 2;
			}
		} else if (os_strcmp(buf, "eap_fast_a_id_info") == 0) {
			os_free(bss->eap_fast_a_id_info);
			bss->eap_fast_a_id_info = os_strdup(pos);
		} else if (os_strcmp(buf, "eap_fast_prov") == 0) {
			bss->eap_fast_prov = atoi(pos);
		} else if (os_strcmp(buf, "pac_key_lifetime") == 0) {
			bss->pac_key_lifetime = atoi(pos);
		} else if (os_strcmp(buf, "pac_key_refresh_time") == 0) {
			bss->pac_key_refresh_time = atoi(pos);
#endif /* EAP_SERVER_FAST */
#ifdef EAP_SERVER_SIM
		} else if (os_strcmp(buf, "eap_sim_db") == 0) {
			os_free(bss->eap_sim_db);
			bss->eap_sim_db = os_strdup(pos);
		} else if (os_strcmp(buf, "eap_sim_aka_result_ind") == 0) {
			bss->eap_sim_aka_result_ind = atoi(pos);
#endif /* EAP_SERVER_SIM */
#ifdef EAP_SERVER_TNC
		} else if (os_strcmp(buf, "tnc") == 0) {
			bss->tnc = atoi(pos);
#endif /* EAP_SERVER_TNC */
#ifdef EAP_SERVER_PWD
		} else if (os_strcmp(buf, "pwd_group") == 0) {
			bss->pwd_group = atoi(pos);
#endif /* EAP_SERVER_PWD */
#endif /* EAP_SERVER */
		} else if (os_strcmp(buf, "eap_message") == 0) {
			char *term;
			bss->eap_req_id_text = os_strdup(pos);
			if (bss->eap_req_id_text == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: Failed to "
					   "allocate memory for "
					   "eap_req_id_text", line);
				errors++;
				return errors;
			}
			bss->eap_req_id_text_len =
				os_strlen(bss->eap_req_id_text);
			term = os_strstr(bss->eap_req_id_text, "\\0");
			if (term) {
				*term++ = '\0';
				os_memmove(term, term + 1,
					   bss->eap_req_id_text_len -
					   (term - bss->eap_req_id_text) - 1);
				bss->eap_req_id_text_len--;
			}
		} else if (os_strcmp(buf, "wep_key_len_broadcast") == 0) {
			bss->default_wep_key_len = atoi(pos);
			if (bss->default_wep_key_len > 13) {
				wpa_printf(MSG_ERROR, "Line %d: invalid WEP "
					   "key len %lu (= %lu bits)", line,
					   (unsigned long)
					   bss->default_wep_key_len,
					   (unsigned long)
					   bss->default_wep_key_len * 8);
				errors++;
			}
		} else if (os_strcmp(buf, "wep_key_len_unicast") == 0) {
			bss->individual_wep_key_len = atoi(pos);
			if (bss->individual_wep_key_len < 0 ||
			    bss->individual_wep_key_len > 13) {
				wpa_printf(MSG_ERROR, "Line %d: invalid WEP "
					   "key len %d (= %d bits)", line,
					   bss->individual_wep_key_len,
					   bss->individual_wep_key_len * 8);
				errors++;
			}
		} else if (os_strcmp(buf, "wep_rekey_period") == 0) {
			bss->wep_rekeying_period = atoi(pos);
			if (bss->wep_rekeying_period < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "period %d",
					   line, bss->wep_rekeying_period);
				errors++;
			}
		} else if (os_strcmp(buf, "eap_reauth_period") == 0) {
			bss->eap_reauth_period = atoi(pos);
			if (bss->eap_reauth_period < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "period %d",
					   line, bss->eap_reauth_period);
				errors++;
			}
		} else if (os_strcmp(buf, "eapol_key_index_workaround") == 0) {
			bss->eapol_key_index_workaround = atoi(pos);
#ifdef CONFIG_IAPP
		} else if (os_strcmp(buf, "iapp_interface") == 0) {
			bss->ieee802_11f = 1;
			os_strlcpy(bss->iapp_iface, pos,
				   sizeof(bss->iapp_iface));
#endif /* CONFIG_IAPP */
		} else if (os_strcmp(buf, "own_ip_addr") == 0) {
			if (hostapd_parse_ip_addr(pos, &bss->own_ip_addr)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid IP "
					   "address '%s'", line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "nas_identifier") == 0) {
			bss->nas_identifier = os_strdup(pos);
#ifndef CONFIG_NO_RADIUS
		} else if (os_strcmp(buf, "auth_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &bss->radius->auth_servers,
				    &bss->radius->num_auth_servers, pos, 1812,
				    &bss->radius->auth_server)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid IP "
					   "address '%s'", line, pos);
				errors++;
			}
		} else if (bss->radius->auth_server &&
			   os_strcmp(buf, "auth_server_port") == 0) {
			bss->radius->auth_server->port = atoi(pos);
		} else if (bss->radius->auth_server &&
			   os_strcmp(buf, "auth_server_shared_secret") == 0) {
			int len = os_strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				wpa_printf(MSG_ERROR, "Line %d: empty shared "
					   "secret is not allowed.", line);
				errors++;
			}
			bss->radius->auth_server->shared_secret =
				(u8 *) os_strdup(pos);
			bss->radius->auth_server->shared_secret_len = len;
		} else if (os_strcmp(buf, "acct_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &bss->radius->acct_servers,
				    &bss->radius->num_acct_servers, pos, 1813,
				    &bss->radius->acct_server)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid IP "
					   "address '%s'", line, pos);
				errors++;
			}
		} else if (bss->radius->acct_server &&
			   os_strcmp(buf, "acct_server_port") == 0) {
			bss->radius->acct_server->port = atoi(pos);
		} else if (bss->radius->acct_server &&
			   os_strcmp(buf, "acct_server_shared_secret") == 0) {
			int len = os_strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				wpa_printf(MSG_ERROR, "Line %d: empty shared "
					   "secret is not allowed.", line);
				errors++;
			}
			bss->radius->acct_server->shared_secret =
				(u8 *) os_strdup(pos);
			bss->radius->acct_server->shared_secret_len = len;
		} else if (os_strcmp(buf, "radius_retry_primary_interval") ==
			   0) {
			bss->radius->retry_primary_interval = atoi(pos);
		} else if (os_strcmp(buf, "radius_acct_interim_interval") == 0)
		{
			bss->acct_interim_interval = atoi(pos);
		} else if (os_strcmp(buf, "radius_request_cui") == 0) {
			bss->radius_request_cui = atoi(pos);
		} else if (os_strcmp(buf, "radius_auth_req_attr") == 0) {
			struct hostapd_radius_attr *attr, *a;
			attr = hostapd_parse_radius_attr(pos);
			if (attr == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "radius_auth_req_attr", line);
				errors++;
			} else if (bss->radius_auth_req_attr == NULL) {
				bss->radius_auth_req_attr = attr;
			} else {
				a = bss->radius_auth_req_attr;
				while (a->next)
					a = a->next;
				a->next = attr;
			}
		} else if (os_strcmp(buf, "radius_acct_req_attr") == 0) {
			struct hostapd_radius_attr *attr, *a;
			attr = hostapd_parse_radius_attr(pos);
			if (attr == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "radius_acct_req_attr", line);
				errors++;
			} else if (bss->radius_acct_req_attr == NULL) {
				bss->radius_acct_req_attr = attr;
			} else {
				a = bss->radius_acct_req_attr;
				while (a->next)
					a = a->next;
				a->next = attr;
			}
		} else if (os_strcmp(buf, "radius_das_port") == 0) {
			bss->radius_das_port = atoi(pos);
		} else if (os_strcmp(buf, "radius_das_client") == 0) {
			if (hostapd_parse_das_client(bss, pos) < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "DAS client", line);
				errors++;
			}
		} else if (os_strcmp(buf, "radius_das_time_window") == 0) {
			bss->radius_das_time_window = atoi(pos);
		} else if (os_strcmp(buf, "radius_das_require_event_timestamp")
			   == 0) {
			bss->radius_das_require_event_timestamp = atoi(pos);
#endif /* CONFIG_NO_RADIUS */
		} else if (os_strcmp(buf, "auth_algs") == 0) {
			bss->auth_algs = atoi(pos);
			if (bss->auth_algs == 0) {
				wpa_printf(MSG_ERROR, "Line %d: no "
					   "authentication algorithms allowed",
					   line);
				errors++;
			}
		} else if (os_strcmp(buf, "max_num_sta") == 0) {
			bss->max_num_sta = atoi(pos);
			if (bss->max_num_sta < 0 ||
			    bss->max_num_sta > MAX_STA_COUNT) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "max_num_sta=%d; allowed range "
					   "0..%d", line, bss->max_num_sta,
					   MAX_STA_COUNT);
				errors++;
			}
		} else if (os_strcmp(buf, "wpa") == 0) {
			bss->wpa = atoi(pos);
		} else if (os_strcmp(buf, "wpa_group_rekey") == 0) {
			bss->wpa_group_rekey = atoi(pos);
		} else if (os_strcmp(buf, "wpa_strict_rekey") == 0) {
			bss->wpa_strict_rekey = atoi(pos);
		} else if (os_strcmp(buf, "wpa_gmk_rekey") == 0) {
			bss->wpa_gmk_rekey = atoi(pos);
		} else if (os_strcmp(buf, "wpa_ptk_rekey") == 0) {
			bss->wpa_ptk_rekey = atoi(pos);
		} else if (os_strcmp(buf, "wpa_passphrase") == 0) {
			int len = os_strlen(pos);
			if (len < 8 || len > 63) {
				wpa_printf(MSG_ERROR, "Line %d: invalid WPA "
					   "passphrase length %d (expected "
					   "8..63)", line, len);
				errors++;
			} else {
				os_free(bss->ssid.wpa_passphrase);
				bss->ssid.wpa_passphrase = os_strdup(pos);
				os_free(bss->ssid.wpa_psk);
				bss->ssid.wpa_psk = NULL;
			}
		} else if (os_strcmp(buf, "wpa_psk") == 0) {
			os_free(bss->ssid.wpa_psk);
			bss->ssid.wpa_psk =
				os_zalloc(sizeof(struct hostapd_wpa_psk));
			if (bss->ssid.wpa_psk == NULL)
				errors++;
			else if (hexstr2bin(pos, bss->ssid.wpa_psk->psk,
					    PMK_LEN) ||
				 pos[PMK_LEN * 2] != '\0') {
				wpa_printf(MSG_ERROR, "Line %d: Invalid PSK "
					   "'%s'.", line, pos);
				errors++;
			} else {
				bss->ssid.wpa_psk->group = 1;
				os_free(bss->ssid.wpa_passphrase);
				bss->ssid.wpa_passphrase = NULL;
			}
		} else if (os_strcmp(buf, "wpa_psk_file") == 0) {
			os_free(bss->ssid.wpa_psk_file);
			bss->ssid.wpa_psk_file = os_strdup(pos);
			if (!bss->ssid.wpa_psk_file) {
				wpa_printf(MSG_ERROR, "Line %d: allocation "
					   "failed", line);
				errors++;
			}
		} else if (os_strcmp(buf, "wpa_key_mgmt") == 0) {
			bss->wpa_key_mgmt =
				hostapd_config_parse_key_mgmt(line, pos);
			if (bss->wpa_key_mgmt == -1)
				errors++;
		} else if (os_strcmp(buf, "wpa_psk_radius") == 0) {
			bss->wpa_psk_radius = atoi(pos);
			if (bss->wpa_psk_radius != PSK_RADIUS_IGNORED &&
			    bss->wpa_psk_radius != PSK_RADIUS_ACCEPTED &&
			    bss->wpa_psk_radius != PSK_RADIUS_REQUIRED) {
				wpa_printf(MSG_ERROR, "Line %d: unknown "
					   "wpa_psk_radius %d",
					   line, bss->wpa_psk_radius);
				errors++;
			}
		} else if (os_strcmp(buf, "wpa_pairwise") == 0) {
			bss->wpa_pairwise =
				hostapd_config_parse_cipher(line, pos);
			if (bss->wpa_pairwise == -1 ||
			    bss->wpa_pairwise == 0)
				errors++;
			else if (bss->wpa_pairwise &
				 (WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
				  WPA_CIPHER_WEP104)) {
				wpa_printf(MSG_ERROR, "Line %d: unsupported "
					   "pairwise cipher suite '%s'",
					   bss->wpa_pairwise, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "rsn_pairwise") == 0) {
			bss->rsn_pairwise =
				hostapd_config_parse_cipher(line, pos);
			if (bss->rsn_pairwise == -1 ||
			    bss->rsn_pairwise == 0)
				errors++;
			else if (bss->rsn_pairwise &
				 (WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
				  WPA_CIPHER_WEP104)) {
				wpa_printf(MSG_ERROR, "Line %d: unsupported "
					   "pairwise cipher suite '%s'",
					   bss->rsn_pairwise, pos);
				errors++;
			}
#ifdef CONFIG_RSN_PREAUTH
		} else if (os_strcmp(buf, "rsn_preauth") == 0) {
			bss->rsn_preauth = atoi(pos);
		} else if (os_strcmp(buf, "rsn_preauth_interfaces") == 0) {
			bss->rsn_preauth_interfaces = os_strdup(pos);
#endif /* CONFIG_RSN_PREAUTH */
#ifdef CONFIG_PEERKEY
		} else if (os_strcmp(buf, "peerkey") == 0) {
			bss->peerkey = atoi(pos);
#endif /* CONFIG_PEERKEY */
#ifdef CONFIG_IEEE80211R
		} else if (os_strcmp(buf, "mobility_domain") == 0) {
			if (os_strlen(pos) != 2 * MOBILITY_DOMAIN_ID_LEN ||
			    hexstr2bin(pos, bss->mobility_domain,
				       MOBILITY_DOMAIN_ID_LEN) != 0) {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid "
					   "mobility_domain '%s'", line, pos);
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "r1_key_holder") == 0) {
			if (os_strlen(pos) != 2 * FT_R1KH_ID_LEN ||
			    hexstr2bin(pos, bss->r1_key_holder,
				       FT_R1KH_ID_LEN) != 0) {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid "
					   "r1_key_holder '%s'", line, pos);
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "r0_key_lifetime") == 0) {
			bss->r0_key_lifetime = atoi(pos);
		} else if (os_strcmp(buf, "reassociation_deadline") == 0) {
			bss->reassociation_deadline = atoi(pos);
		} else if (os_strcmp(buf, "r0kh") == 0) {
			if (add_r0kh(bss, pos) < 0) {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid "
					   "r0kh '%s'", line, pos);
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "r1kh") == 0) {
			if (add_r1kh(bss, pos) < 0) {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid "
					   "r1kh '%s'", line, pos);
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "pmk_r1_push") == 0) {
			bss->pmk_r1_push = atoi(pos);
		} else if (os_strcmp(buf, "ft_over_ds") == 0) {
			bss->ft_over_ds = atoi(pos);
#endif /* CONFIG_IEEE80211R */
#ifndef CONFIG_NO_CTRL_IFACE
		} else if (os_strcmp(buf, "ctrl_interface") == 0) {
			os_free(bss->ctrl_interface);
			bss->ctrl_interface = os_strdup(pos);
		} else if (os_strcmp(buf, "ctrl_interface_group") == 0) {
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
				return errors;
			}

			/* Group name not found - try to parse this as gid */
			bss->ctrl_interface_gid = strtol(group, &endp, 10);
			if (*group == '\0' || *endp != '\0') {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid group "
					   "'%s'", line, group);
				errors++;
				return errors;
			}
			bss->ctrl_interface_gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   bss->ctrl_interface_gid);
#endif /* CONFIG_NATIVE_WINDOWS */
#endif /* CONFIG_NO_CTRL_IFACE */
#ifdef RADIUS_SERVER
		} else if (os_strcmp(buf, "radius_server_clients") == 0) {
			os_free(bss->radius_server_clients);
			bss->radius_server_clients = os_strdup(pos);
		} else if (os_strcmp(buf, "radius_server_auth_port") == 0) {
			bss->radius_server_auth_port = atoi(pos);
		} else if (os_strcmp(buf, "radius_server_ipv6") == 0) {
			bss->radius_server_ipv6 = atoi(pos);
#endif /* RADIUS_SERVER */
		} else if (os_strcmp(buf, "test_socket") == 0) {
			os_free(bss->test_socket);
			bss->test_socket = os_strdup(pos);
		} else if (os_strcmp(buf, "use_pae_group_addr") == 0) {
			bss->use_pae_group_addr = atoi(pos);
		} else if (os_strcmp(buf, "hw_mode") == 0) {
			if (os_strcmp(pos, "a") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211A;
			else if (os_strcmp(pos, "b") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211B;
			else if (os_strcmp(pos, "g") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211G;
			else if (os_strcmp(pos, "ad") == 0)
				conf->hw_mode = HOSTAPD_MODE_IEEE80211AD;
			else {
				wpa_printf(MSG_ERROR, "Line %d: unknown "
					   "hw_mode '%s'", line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "wps_rf_bands") == 0) {
			if (os_strcmp(pos, "a") == 0)
				bss->wps_rf_bands = WPS_RF_50GHZ;
			else if (os_strcmp(pos, "g") == 0 ||
				 os_strcmp(pos, "b") == 0)
				bss->wps_rf_bands = WPS_RF_24GHZ;
			else if (os_strcmp(pos, "ag") == 0 ||
				 os_strcmp(pos, "ga") == 0)
				bss->wps_rf_bands =
					WPS_RF_24GHZ | WPS_RF_50GHZ;
			else {
				wpa_printf(MSG_ERROR, "Line %d: unknown "
					   "wps_rf_band '%s'", line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "channel") == 0) {
			conf->channel = atoi(pos);
		} else if (os_strcmp(buf, "beacon_int") == 0) {
			int val = atoi(pos);
			/* MIB defines range as 1..65535, but very small values
			 * cause problems with the current implementation.
			 * Since it is unlikely that this small numbers are
			 * useful in real life scenarios, do not allow beacon
			 * period to be set below 15 TU. */
			if (val < 15 || val > 65535) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "beacon_int %d (expected "
					   "15..65535)", line, val);
				errors++;
			} else
				conf->beacon_int = val;
		} else if (os_strcmp(buf, "dtim_period") == 0) {
			bss->dtim_period = atoi(pos);
			if (bss->dtim_period < 1 || bss->dtim_period > 255) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "dtim_period %d",
					   line, bss->dtim_period);
				errors++;
			}
		} else if (os_strcmp(buf, "rts_threshold") == 0) {
			conf->rts_threshold = atoi(pos);
			if (conf->rts_threshold < 0 ||
			    conf->rts_threshold > 2347) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "rts_threshold %d",
					   line, conf->rts_threshold);
				errors++;
			}
		} else if (os_strcmp(buf, "fragm_threshold") == 0) {
			conf->fragm_threshold = atoi(pos);
			if (conf->fragm_threshold < 256 ||
			    conf->fragm_threshold > 2346) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "fragm_threshold %d",
					   line, conf->fragm_threshold);
				errors++;
			}
		} else if (os_strcmp(buf, "send_probe_response") == 0) {
			int val = atoi(pos);
			if (val != 0 && val != 1) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "send_probe_response %d (expected "
					   "0 or 1)", line, val);
			} else
				conf->send_probe_response = val;
		} else if (os_strcmp(buf, "supported_rates") == 0) {
			if (hostapd_parse_rates(&conf->supported_rates, pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid rate "
					   "list", line);
				errors++;
			}
		} else if (os_strcmp(buf, "basic_rates") == 0) {
			if (hostapd_parse_rates(&conf->basic_rates, pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid rate "
					   "list", line);
				errors++;
			}
		} else if (os_strcmp(buf, "preamble") == 0) {
			if (atoi(pos))
				conf->preamble = SHORT_PREAMBLE;
			else
				conf->preamble = LONG_PREAMBLE;
		} else if (os_strcmp(buf, "ignore_broadcast_ssid") == 0) {
			bss->ignore_broadcast_ssid = atoi(pos);
		} else if (os_strcmp(buf, "wep_default_key") == 0) {
			bss->ssid.wep.idx = atoi(pos);
			if (bss->ssid.wep.idx > 3) {
				wpa_printf(MSG_ERROR, "Invalid "
					   "wep_default_key index %d",
					   bss->ssid.wep.idx);
				errors++;
			}
		} else if (os_strcmp(buf, "wep_key0") == 0 ||
			   os_strcmp(buf, "wep_key1") == 0 ||
			   os_strcmp(buf, "wep_key2") == 0 ||
			   os_strcmp(buf, "wep_key3") == 0) {
			if (hostapd_config_read_wep(&bss->ssid.wep,
						    buf[7] - '0', pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid WEP "
					   "key '%s'", line, buf);
				errors++;
			}
#ifndef CONFIG_NO_VLAN
		} else if (os_strcmp(buf, "dynamic_vlan") == 0) {
			bss->ssid.dynamic_vlan = atoi(pos);
		} else if (os_strcmp(buf, "vlan_file") == 0) {
			if (hostapd_config_read_vlan_file(bss, pos)) {
				wpa_printf(MSG_ERROR, "Line %d: failed to "
					   "read VLAN file '%s'", line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "vlan_naming") == 0) {
			bss->ssid.vlan_naming = atoi(pos);
			if (bss->ssid.vlan_naming >= DYNAMIC_VLAN_NAMING_END ||
			    bss->ssid.vlan_naming < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "naming scheme %d", line,
                                           bss->ssid.vlan_naming);
				errors++;
                        }
#ifdef CONFIG_FULL_DYNAMIC_VLAN
		} else if (os_strcmp(buf, "vlan_tagged_interface") == 0) {
			bss->ssid.vlan_tagged_interface = os_strdup(pos);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
#endif /* CONFIG_NO_VLAN */
		} else if (os_strcmp(buf, "ap_table_max_size") == 0) {
			conf->ap_table_max_size = atoi(pos);
		} else if (os_strcmp(buf, "ap_table_expiration_time") == 0) {
			conf->ap_table_expiration_time = atoi(pos);
		} else if (os_strncmp(buf, "tx_queue_", 9) == 0) {
			if (hostapd_config_tx_queue(conf, buf, pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid TX "
					   "queue item", line);
				errors++;
			}
		} else if (os_strcmp(buf, "wme_enabled") == 0 ||
			   os_strcmp(buf, "wmm_enabled") == 0) {
			bss->wmm_enabled = atoi(pos);
		} else if (os_strcmp(buf, "uapsd_advertisement_enabled") == 0) {
			bss->wmm_uapsd = atoi(pos);
		} else if (os_strncmp(buf, "wme_ac_", 7) == 0 ||
			   os_strncmp(buf, "wmm_ac_", 7) == 0) {
			if (hostapd_config_wmm_ac(conf->wmm_ac_params, buf,
						  pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid WMM "
					   "ac item", line);
				errors++;
			}
		} else if (os_strcmp(buf, "bss") == 0) {
			if (hostapd_config_bss(conf, pos)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid bss "
					   "item", line);
				errors++;
			}
		} else if (os_strcmp(buf, "bssid") == 0) {
			if (hwaddr_aton(pos, bss->bssid)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid bssid "
					   "item", line);
				errors++;
			}
#ifdef CONFIG_IEEE80211W
		} else if (os_strcmp(buf, "ieee80211w") == 0) {
			bss->ieee80211w = atoi(pos);
		} else if (os_strcmp(buf, "assoc_sa_query_max_timeout") == 0) {
			bss->assoc_sa_query_max_timeout = atoi(pos);
			if (bss->assoc_sa_query_max_timeout == 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "assoc_sa_query_max_timeout", line);
				errors++;
			}
		} else if (os_strcmp(buf, "assoc_sa_query_retry_timeout") == 0)
		{
			bss->assoc_sa_query_retry_timeout = atoi(pos);
			if (bss->assoc_sa_query_retry_timeout == 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "assoc_sa_query_retry_timeout",
					   line);
				errors++;
			}
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_IEEE80211N
		} else if (os_strcmp(buf, "ieee80211n") == 0) {
			conf->ieee80211n = atoi(pos);
		} else if (os_strcmp(buf, "ht_capab") == 0) {
			if (hostapd_config_ht_capab(conf, pos) < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "ht_capab", line);
				errors++;
			}
		} else if (os_strcmp(buf, "require_ht") == 0) {
			conf->require_ht = atoi(pos);
#endif /* CONFIG_IEEE80211N */
#ifdef CONFIG_IEEE80211AC
		} else if (os_strcmp(buf, "ieee80211ac") == 0) {
			conf->ieee80211ac = atoi(pos);
		} else if (os_strcmp(buf, "vht_capab") == 0) {
			if (hostapd_config_vht_capab(conf, pos) < 0) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "vht_capab", line);
				errors++;
			}
		} else if (os_strcmp(buf, "require_vht") == 0) {
			conf->require_vht = atoi(pos);
		} else if (os_strcmp(buf, "vht_oper_chwidth") == 0) {
			conf->vht_oper_chwidth = atoi(pos);
		} else if (os_strcmp(buf, "vht_oper_centr_freq_seg0_idx") == 0)
		{
			conf->vht_oper_centr_freq_seg0_idx = atoi(pos);
		} else if (os_strcmp(buf, "vht_oper_centr_freq_seg1_idx") == 0)
		{
			conf->vht_oper_centr_freq_seg1_idx = atoi(pos);
#endif /* CONFIG_IEEE80211AC */
		} else if (os_strcmp(buf, "max_listen_interval") == 0) {
			bss->max_listen_interval = atoi(pos);
		} else if (os_strcmp(buf, "disable_pmksa_caching") == 0) {
			bss->disable_pmksa_caching = atoi(pos);
		} else if (os_strcmp(buf, "okc") == 0) {
			bss->okc = atoi(pos);
#ifdef CONFIG_WPS
		} else if (os_strcmp(buf, "wps_state") == 0) {
			bss->wps_state = atoi(pos);
			if (bss->wps_state < 0 || bss->wps_state > 2) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "wps_state", line);
				errors++;
			}
		} else if (os_strcmp(buf, "ap_setup_locked") == 0) {
			bss->ap_setup_locked = atoi(pos);
		} else if (os_strcmp(buf, "uuid") == 0) {
			if (uuid_str2bin(pos, bss->uuid)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid UUID",
					   line);
				errors++;
			}
		} else if (os_strcmp(buf, "wps_pin_requests") == 0) {
			os_free(bss->wps_pin_requests);
			bss->wps_pin_requests = os_strdup(pos);
		} else if (os_strcmp(buf, "device_name") == 0) {
			if (os_strlen(pos) > 32) {
				wpa_printf(MSG_ERROR, "Line %d: Too long "
					   "device_name", line);
				errors++;
			}
			os_free(bss->device_name);
			bss->device_name = os_strdup(pos);
		} else if (os_strcmp(buf, "manufacturer") == 0) {
			if (os_strlen(pos) > 64) {
				wpa_printf(MSG_ERROR, "Line %d: Too long "
					   "manufacturer", line);
				errors++;
			}
			os_free(bss->manufacturer);
			bss->manufacturer = os_strdup(pos);
		} else if (os_strcmp(buf, "model_name") == 0) {
			if (os_strlen(pos) > 32) {
				wpa_printf(MSG_ERROR, "Line %d: Too long "
					   "model_name", line);
				errors++;
			}
			os_free(bss->model_name);
			bss->model_name = os_strdup(pos);
		} else if (os_strcmp(buf, "model_number") == 0) {
			if (os_strlen(pos) > 32) {
				wpa_printf(MSG_ERROR, "Line %d: Too long "
					   "model_number", line);
				errors++;
			}
			os_free(bss->model_number);
			bss->model_number = os_strdup(pos);
		} else if (os_strcmp(buf, "serial_number") == 0) {
			if (os_strlen(pos) > 32) {
				wpa_printf(MSG_ERROR, "Line %d: Too long "
					   "serial_number", line);
				errors++;
			}
			os_free(bss->serial_number);
			bss->serial_number = os_strdup(pos);
		} else if (os_strcmp(buf, "device_type") == 0) {
			if (wps_dev_type_str2bin(pos, bss->device_type))
				errors++;
		} else if (os_strcmp(buf, "config_methods") == 0) {
			os_free(bss->config_methods);
			bss->config_methods = os_strdup(pos);
		} else if (os_strcmp(buf, "os_version") == 0) {
			if (hexstr2bin(pos, bss->os_version, 4)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "os_version", line);
				errors++;
			}
		} else if (os_strcmp(buf, "ap_pin") == 0) {
			os_free(bss->ap_pin);
			bss->ap_pin = os_strdup(pos);
		} else if (os_strcmp(buf, "skip_cred_build") == 0) {
			bss->skip_cred_build = atoi(pos);
		} else if (os_strcmp(buf, "extra_cred") == 0) {
			os_free(bss->extra_cred);
			bss->extra_cred =
				(u8 *) os_readfile(pos, &bss->extra_cred_len);
			if (bss->extra_cred == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: could not "
					   "read Credentials from '%s'",
					   line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "wps_cred_processing") == 0) {
			bss->wps_cred_processing = atoi(pos);
		} else if (os_strcmp(buf, "ap_settings") == 0) {
			os_free(bss->ap_settings);
			bss->ap_settings =
				(u8 *) os_readfile(pos, &bss->ap_settings_len);
			if (bss->ap_settings == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: could not "
					   "read AP Settings from '%s'",
					   line, pos);
				errors++;
			}
		} else if (os_strcmp(buf, "upnp_iface") == 0) {
			bss->upnp_iface = os_strdup(pos);
		} else if (os_strcmp(buf, "friendly_name") == 0) {
			os_free(bss->friendly_name);
			bss->friendly_name = os_strdup(pos);
		} else if (os_strcmp(buf, "manufacturer_url") == 0) {
			os_free(bss->manufacturer_url);
			bss->manufacturer_url = os_strdup(pos);
		} else if (os_strcmp(buf, "model_description") == 0) {
			os_free(bss->model_description);
			bss->model_description = os_strdup(pos);
		} else if (os_strcmp(buf, "model_url") == 0) {
			os_free(bss->model_url);
			bss->model_url = os_strdup(pos);
		} else if (os_strcmp(buf, "upc") == 0) {
			os_free(bss->upc);
			bss->upc = os_strdup(pos);
		} else if (os_strcmp(buf, "pbc_in_m1") == 0) {
			bss->pbc_in_m1 = atoi(pos);
#ifdef CONFIG_WPS_NFC
		} else if (os_strcmp(buf, "wps_nfc_dev_pw_id") == 0) {
			bss->wps_nfc_dev_pw_id = atoi(pos);
			if (bss->wps_nfc_dev_pw_id < 0x10 ||
			    bss->wps_nfc_dev_pw_id > 0xffff) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "wps_nfc_dev_pw_id value", line);
				errors++;
			}
		} else if (os_strcmp(buf, "wps_nfc_dh_pubkey") == 0) {
			wpabuf_free(bss->wps_nfc_dh_pubkey);
			bss->wps_nfc_dh_pubkey = hostapd_parse_bin(pos);
		} else if (os_strcmp(buf, "wps_nfc_dh_privkey") == 0) {
			wpabuf_free(bss->wps_nfc_dh_privkey);
			bss->wps_nfc_dh_privkey = hostapd_parse_bin(pos);
		} else if (os_strcmp(buf, "wps_nfc_dev_pw") == 0) {
			wpabuf_free(bss->wps_nfc_dev_pw);
			bss->wps_nfc_dev_pw = hostapd_parse_bin(pos);
#endif /* CONFIG_WPS_NFC */
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P_MANAGER
		} else if (os_strcmp(buf, "manage_p2p") == 0) {
			int manage = atoi(pos);
			if (manage)
				bss->p2p |= P2P_MANAGE;
			else
				bss->p2p &= ~P2P_MANAGE;
		} else if (os_strcmp(buf, "allow_cross_connection") == 0) {
			if (atoi(pos))
				bss->p2p |= P2P_ALLOW_CROSS_CONNECTION;
			else
				bss->p2p &= ~P2P_ALLOW_CROSS_CONNECTION;
#endif /* CONFIG_P2P_MANAGER */
		} else if (os_strcmp(buf, "disassoc_low_ack") == 0) {
			bss->disassoc_low_ack = atoi(pos);
		} else if (os_strcmp(buf, "tdls_prohibit") == 0) {
			int val = atoi(pos);
			if (val)
				bss->tdls |= TDLS_PROHIBIT;
			else
				bss->tdls &= ~TDLS_PROHIBIT;
		} else if (os_strcmp(buf, "tdls_prohibit_chan_switch") == 0) {
			int val = atoi(pos);
			if (val)
				bss->tdls |= TDLS_PROHIBIT_CHAN_SWITCH;
			else
				bss->tdls &= ~TDLS_PROHIBIT_CHAN_SWITCH;
#ifdef CONFIG_RSN_TESTING
		} else if (os_strcmp(buf, "rsn_testing") == 0) {
			extern int rsn_testing;
			rsn_testing = atoi(pos);
#endif /* CONFIG_RSN_TESTING */
		} else if (os_strcmp(buf, "time_advertisement") == 0) {
			bss->time_advertisement = atoi(pos);
		} else if (os_strcmp(buf, "time_zone") == 0) {
			size_t tz_len = os_strlen(pos);
			if (tz_len < 4 || tz_len > 255) {
				wpa_printf(MSG_DEBUG, "Line %d: invalid "
					   "time_zone", line);
				errors++;
				return errors;
			}
			os_free(bss->time_zone);
			bss->time_zone = os_strdup(pos);
			if (bss->time_zone == NULL)
				errors++;
#ifdef CONFIG_WNM
		} else if (os_strcmp(buf, "wnm_sleep_mode") == 0) {
			bss->wnm_sleep_mode = atoi(pos);
		} else if (os_strcmp(buf, "bss_transition") == 0) {
			bss->bss_transition = atoi(pos);
#endif /* CONFIG_WNM */
#ifdef CONFIG_INTERWORKING
		} else if (os_strcmp(buf, "interworking") == 0) {
			bss->interworking = atoi(pos);
		} else if (os_strcmp(buf, "access_network_type") == 0) {
			bss->access_network_type = atoi(pos);
			if (bss->access_network_type < 0 ||
			    bss->access_network_type > 15) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "access_network_type", line);
				errors++;
			}
		} else if (os_strcmp(buf, "internet") == 0) {
			bss->internet = atoi(pos);
		} else if (os_strcmp(buf, "asra") == 0) {
			bss->asra = atoi(pos);
		} else if (os_strcmp(buf, "esr") == 0) {
			bss->esr = atoi(pos);
		} else if (os_strcmp(buf, "uesa") == 0) {
			bss->uesa = atoi(pos);
		} else if (os_strcmp(buf, "venue_group") == 0) {
			bss->venue_group = atoi(pos);
			bss->venue_info_set = 1;
		} else if (os_strcmp(buf, "venue_type") == 0) {
			bss->venue_type = atoi(pos);
			bss->venue_info_set = 1;
		} else if (os_strcmp(buf, "hessid") == 0) {
			if (hwaddr_aton(pos, bss->hessid)) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "hessid", line);
				errors++;
			}
		} else if (os_strcmp(buf, "roaming_consortium") == 0) {
			if (parse_roaming_consortium(bss, pos, line) < 0)
				errors++;
		} else if (os_strcmp(buf, "venue_name") == 0) {
			if (parse_venue_name(bss, pos, line) < 0)
				errors++;
		} else if (os_strcmp(buf, "network_auth_type") == 0) {
			u8 auth_type;
			u16 redirect_url_len;
			if (hexstr2bin(pos, &auth_type, 1)) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "network_auth_type '%s'",
					   line, pos);
				errors++;
				return errors;
			}
			if (auth_type == 0 || auth_type == 2)
				redirect_url_len = os_strlen(pos + 2);
			else
				redirect_url_len = 0;
			os_free(bss->network_auth_type);
			bss->network_auth_type =
				os_malloc(redirect_url_len + 3 + 1);
			if (bss->network_auth_type == NULL) {
				errors++;
				return errors;
			}
			*bss->network_auth_type = auth_type;
			WPA_PUT_LE16(bss->network_auth_type + 1,
				     redirect_url_len);
			if (redirect_url_len)
				os_memcpy(bss->network_auth_type + 3,
					  pos + 2, redirect_url_len);
			bss->network_auth_type_len = 3 + redirect_url_len;
		} else if (os_strcmp(buf, "ipaddr_type_availability") == 0) {
			if (hexstr2bin(pos, &bss->ipaddr_type_availability, 1))
			{
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "ipaddr_type_availability '%s'",
					   line, pos);
				bss->ipaddr_type_configured = 0;
				errors++;
				return errors;
			}
			bss->ipaddr_type_configured = 1;
		} else if (os_strcmp(buf, "domain_name") == 0) {
			int j, num_domains, domain_len, domain_list_len = 0;
			char *tok_start, *tok_prev;
			u8 *domain_list, *domain_ptr;

			domain_list_len = os_strlen(pos) + 1;
			domain_list = os_malloc(domain_list_len);
			if (domain_list == NULL) {
				errors++;
				return errors;
			}

			domain_ptr = domain_list;
			tok_prev = pos;
			num_domains = 1;
			while ((tok_prev = os_strchr(tok_prev, ','))) {
				num_domains++;
				tok_prev++;
			}
			tok_prev = pos;
			for (j = 0; j < num_domains; j++) {
				tok_start = os_strchr(tok_prev, ',');
				if (tok_start) {
					domain_len = tok_start - tok_prev;
					*domain_ptr = domain_len;
					os_memcpy(domain_ptr + 1, tok_prev,
						  domain_len);
					domain_ptr += domain_len + 1;
					tok_prev = ++tok_start;
				} else {
					domain_len = os_strlen(tok_prev);
					*domain_ptr = domain_len;
					os_memcpy(domain_ptr + 1, tok_prev,
						  domain_len);
					domain_ptr += domain_len + 1;
				}
			}

			os_free(bss->domain_name);
			bss->domain_name = domain_list;
			bss->domain_name_len = domain_list_len;
		} else if (os_strcmp(buf, "anqp_3gpp_cell_net") == 0) {
			if (parse_3gpp_cell_net(bss, pos, line) < 0)
				errors++;
		} else if (os_strcmp(buf, "nai_realm") == 0) {
			if (parse_nai_realm(bss, pos, line) < 0)
				errors++;
		} else if (os_strcmp(buf, "gas_frag_limit") == 0) {
			bss->gas_frag_limit = atoi(pos);
		} else if (os_strcmp(buf, "gas_comeback_delay") == 0) {
			bss->gas_comeback_delay = atoi(pos);
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_RADIUS_TEST
		} else if (os_strcmp(buf, "dump_msk_file") == 0) {
			os_free(bss->dump_msk_file);
			bss->dump_msk_file = os_strdup(pos);
#endif /* CONFIG_RADIUS_TEST */
#ifdef CONFIG_HS20
		} else if (os_strcmp(buf, "hs20") == 0) {
			bss->hs20 = atoi(pos);
		} else if (os_strcmp(buf, "disable_dgaf") == 0) {
			bss->disable_dgaf = atoi(pos);
		} else if (os_strcmp(buf, "hs20_oper_friendly_name") == 0) {
			if (hs20_parse_oper_friendly_name(bss, pos, line) < 0)
				errors++;
		} else if (os_strcmp(buf, "hs20_wan_metrics") == 0) {
			if (hs20_parse_wan_metrics(bss, pos, line) < 0) {
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "hs20_conn_capab") == 0) {
			if (hs20_parse_conn_capab(bss, pos, line) < 0) {
				errors++;
				return errors;
			}
		} else if (os_strcmp(buf, "hs20_operating_class") == 0) {
			u8 *oper_class;
			size_t oper_class_len;
			oper_class_len = os_strlen(pos);
			if (oper_class_len < 2 || (oper_class_len & 0x01)) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "hs20_operating_class '%s'",
					   line, pos);
				errors++;
				return errors;
			}
			oper_class_len /= 2;
			oper_class = os_malloc(oper_class_len);
			if (oper_class == NULL) {
				errors++;
				return errors;
			}
			if (hexstr2bin(pos, oper_class, oper_class_len)) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "hs20_operating_class '%s'",
					   line, pos);
				os_free(oper_class);
				errors++;
				return errors;
			}
			os_free(bss->hs20_operating_class);
			bss->hs20_operating_class = oper_class;
			bss->hs20_operating_class_len = oper_class_len;
#endif /* CONFIG_HS20 */
		} else if (os_strcmp(buf, "vendor_elements") == 0) {
			struct wpabuf *elems;
			size_t len = os_strlen(pos);
			if (len & 0x01) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "vendor_elements '%s'", line, pos);
				return 1;
			}
			len /= 2;
			if (len == 0) {
				wpabuf_free(bss->vendor_elements);
				bss->vendor_elements = NULL;
				return 0;
			}

			elems = wpabuf_alloc(len);
			if (elems == NULL)
				return 1;

			if (hexstr2bin(pos, wpabuf_put(elems, len), len)) {
				wpabuf_free(elems);
				wpa_printf(MSG_ERROR, "Line %d: Invalid "
					   "vendor_elements '%s'", line, pos);
				return 1;
			}

			wpabuf_free(bss->vendor_elements);
			bss->vendor_elements = elems;
		} else {
			wpa_printf(MSG_ERROR, "Line %d: unknown configuration "
				   "item '%s'", line, buf);
			errors++;
		}
	}

	return errors;
}


static void hostapd_set_security_params(struct hostapd_bss_config *bss)
{
	int pairwise;

	if (bss->individual_wep_key_len == 0) {
		/* individual keys are not use; can use key idx0 for
		 * broadcast keys */
		bss->broadcast_key_idx_min = 0;
	}

	/* Select group cipher based on the enabled pairwise cipher
	 * suites */
	pairwise = 0;
	if (bss->wpa & 1)
		pairwise |= bss->wpa_pairwise;
	if (bss->wpa & 2) {
		if (bss->rsn_pairwise == 0)
			bss->rsn_pairwise = bss->wpa_pairwise;
		pairwise |= bss->rsn_pairwise;
	}
	if (pairwise & WPA_CIPHER_TKIP)
		bss->wpa_group = WPA_CIPHER_TKIP;
	else if ((pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP)) ==
		 WPA_CIPHER_GCMP)
		bss->wpa_group = WPA_CIPHER_GCMP;
	else
		bss->wpa_group = WPA_CIPHER_CCMP;

	bss->radius->auth_server = bss->radius->auth_servers;
	bss->radius->acct_server = bss->radius->acct_servers;

	if (bss->wpa && bss->ieee802_1x) {
		bss->ssid.security_policy = SECURITY_WPA;
	} else if (bss->wpa) {
		bss->ssid.security_policy = SECURITY_WPA_PSK;
	} else if (bss->ieee802_1x) {
		int cipher = WPA_CIPHER_NONE;
		bss->ssid.security_policy = SECURITY_IEEE_802_1X;
		bss->ssid.wep.default_len = bss->default_wep_key_len;
		if (bss->default_wep_key_len)
			cipher = bss->default_wep_key_len >= 13 ?
				WPA_CIPHER_WEP104 : WPA_CIPHER_WEP40;
		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
	} else if (bss->ssid.wep.keys_set) {
		int cipher = WPA_CIPHER_WEP40;
		if (bss->ssid.wep.len[0] >= 13)
			cipher = WPA_CIPHER_WEP104;
		bss->ssid.security_policy = SECURITY_STATIC_WEP;
		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
	} else {
		bss->ssid.security_policy = SECURITY_PLAINTEXT;
		bss->wpa_group = WPA_CIPHER_NONE;
		bss->wpa_pairwise = WPA_CIPHER_NONE;
		bss->rsn_pairwise = WPA_CIPHER_NONE;
	}
}


/**
 * hostapd_config_read - Read and parse a configuration file
 * @fname: Configuration file name (including path, if needed)
 * Returns: Allocated configuration data structure
 */
struct hostapd_config * hostapd_config_read(const char *fname)
{
	struct hostapd_config *conf;
	struct hostapd_bss_config *bss;
	FILE *f;
	char buf[512], *pos;
	int line = 0;
	int errors = 0;
	size_t i;

	f = fopen(fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_ERROR, "Could not open configuration file '%s' "
			   "for reading.", fname);
		return NULL;
	}

	conf = hostapd_config_defaults();
	if (conf == NULL) {
		fclose(f);
		return NULL;
	}

	/* set default driver based on configuration */
	conf->driver = wpa_drivers[0];
	if (conf->driver == NULL) {
		wpa_printf(MSG_ERROR, "No driver wrappers registered!");
		hostapd_config_free(conf);
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

		pos = os_strchr(buf, '=');
		if (pos == NULL) {
			wpa_printf(MSG_ERROR, "Line %d: invalid line '%s'",
				   line, buf);
			errors++;
			continue;
		}
		*pos = '\0';
		pos++;
		errors += hostapd_config_fill(conf, bss, buf, pos, line);
	}

	fclose(f);

	for (i = 0; i < conf->num_bss; i++)
		hostapd_set_security_params(&conf->bss[i]);

	if (hostapd_config_check(conf))
		errors++;

#ifndef WPA_IGNORE_CONFIG_ERRORS
	if (errors) {
		wpa_printf(MSG_ERROR, "%d errors found in configuration file "
			   "'%s'", errors, fname);
		hostapd_config_free(conf);
		conf = NULL;
	}
#endif /* WPA_IGNORE_CONFIG_ERRORS */

	return conf;
}


int hostapd_set_iface(struct hostapd_config *conf,
		      struct hostapd_bss_config *bss, char *field, char *value)
{
	int errors;
	size_t i;

	errors = hostapd_config_fill(conf, bss, field, value, 0);
	if (errors) {
		wpa_printf(MSG_INFO, "Failed to set configuration field '%s' "
			   "to value '%s'", field, value);
		return -1;
	}

	for (i = 0; i < conf->num_bss; i++)
		hostapd_set_security_params(&conf->bss[i]);

	if (hostapd_config_check(conf)) {
		wpa_printf(MSG_ERROR, "Configuration check failed");
		return -1;
	}

	return 0;
}
