/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / Configuration file
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
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <grp.h>

#include "hostapd.h"
#include "driver.h"
#include "sha1.h"
#include "eap.h"


static struct hostapd_config *hostapd_config_defaults(void)
{
	struct hostapd_config *conf;

	conf = malloc(sizeof(*conf));
	if (conf == NULL) {
		printf("Failed to allocate memory for configuration data.\n");
		return NULL;
	}
	memset(conf, 0, sizeof(*conf));

	/* set default driver based on configuration */
	conf->driver = driver_lookup("default");
	if (conf->driver == NULL) {
		printf("No default driver registered!\n");
		free(conf);
		return NULL;
	}

	conf->wep_rekeying_period = 300;
	conf->eap_reauth_period = 3600;

	conf->logger_syslog_level = HOSTAPD_LEVEL_INFO;
	conf->logger_stdout_level = HOSTAPD_LEVEL_INFO;
	conf->logger_syslog = (unsigned int) -1;
	conf->logger_stdout = (unsigned int) -1;

	conf->auth_algs = HOSTAPD_AUTH_OPEN | HOSTAPD_AUTH_SHARED_KEY;

	conf->wpa_group_rekey = 600;
	conf->wpa_gmk_rekey = 86400;
	conf->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	conf->wpa_pairwise = WPA_CIPHER_TKIP;
	conf->wpa_group = WPA_CIPHER_TKIP;

	conf->radius_server_auth_port = 1812;

	return conf;
}


static int mac_comp(const void *a, const void *b)
{
	return memcmp(a, b, sizeof(macaddr));
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

	qsort(*acl, *num, sizeof(macaddr), mac_comp);

	return 0;
}


static int hostapd_config_read_wpa_psk(const char *fname,
				       struct hostapd_config *conf)
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

		psk = malloc(sizeof(*psk));
		if (psk == NULL) {
			printf("WPA PSK allocation failed\n");
			ret = -1;
			break;
		}
		memset(psk, 0, sizeof(*psk));
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
			pbkdf2_sha1(pos, conf->ssid, conf->ssid_len,
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

		psk->next = conf->wpa_psk;
		conf->wpa_psk = psk;
	}

	fclose(f);

	return ret;
}


int hostapd_setup_wpa_psk(struct hostapd_config *conf)
{
	if (conf->wpa_passphrase != NULL) {
		if (conf->wpa_psk != NULL) {
			printf("Warning: both WPA PSK and passphrase set. "
			       "Using passphrase.\n");
			free(conf->wpa_psk);
		}
		conf->wpa_psk = malloc(sizeof(struct hostapd_wpa_psk));
		if (conf->wpa_psk == NULL) {
			printf("Unable to alloc space for PSK\n");
			return -1;
		}
		wpa_hexdump_ascii(MSG_DEBUG, "SSID",
				  (u8 *) conf->ssid, conf->ssid_len);
		wpa_hexdump_ascii(MSG_DEBUG, "PSK (ASCII passphrase)",
				  (u8 *) conf->wpa_passphrase,
				  strlen(conf->wpa_passphrase));
		memset(conf->wpa_psk, 0, sizeof(struct hostapd_wpa_psk));
		pbkdf2_sha1(conf->wpa_passphrase,
			    conf->ssid, conf->ssid_len,
			    4096, conf->wpa_psk->psk, PMK_LEN);
		wpa_hexdump(MSG_DEBUG, "PSK (from passphrase)",
			    conf->wpa_psk->psk, PMK_LEN);
		conf->wpa_psk->group = 1;

		memset(conf->wpa_passphrase, 0, strlen(conf->wpa_passphrase));
		free(conf->wpa_passphrase);
		conf->wpa_passphrase = 0;
	}

	if (conf->wpa_psk_file) {
		if (hostapd_config_read_wpa_psk(conf->wpa_psk_file, conf))
			return -1;
		free(conf->wpa_psk_file);
		conf->wpa_psk_file = NULL;
	}

	return 0;
}


#ifdef EAP_AUTHENTICATOR
static int hostapd_config_read_eap_user(const char *fname,
					struct hostapd_config *conf)
{
	FILE *f;
	char buf[512], *pos, *start;
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

		user = malloc(sizeof(*user));
		if (user == NULL) {
			printf("EAP user allocation failed\n");
			goto failed;
		}
		memset(user, 0, sizeof(*user));
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
			char *pos2 = strchr(start, ',');
			if (pos2) {
				*pos2++ = '\0';
			}
			user->methods[num_methods] = eap_get_type(start);
			if (user->methods[num_methods] == EAP_TYPE_NONE) {
				printf("Unsupported EAP type '%s' on line %d "
				       "in '%s'\n", start, line, fname);
				goto failed;
			}

			num_methods++;
			if (num_methods >= EAP_USER_MAX_METHODS)
				break;
			if (pos2 == NULL)
				break;
			start = pos2;
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

		if (*pos != '"') {
			printf("Invalid EAP password (no \" in start) on "
			       "line %d in '%s'\n", line, fname);
			goto failed;
		}
		pos++;
		start = pos;
		while (*pos != '"' && *pos != '\0')
			pos++;
		if (*pos == '\0') {
			printf("Invalid EAP password (no \" in end) on "
			       "line %d in '%s'\n", line, fname);
			goto failed;
		}

		user->password = malloc(pos - start);
		if (user->password == NULL) {
			printf("Failed to allocate memory for EAP password\n");
			goto failed;
		}
		memcpy(user->password, start, pos - start);
		user->password_len = pos - start;

		pos++;
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
			free(user->identity);
			free(user);
		}
		ret = -1;
		break;
	}

	fclose(f);

	return ret;
}
#endif /* EAP_AUTHENTICATOR */


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
	ret = !inet_aton(val, &nserv->addr);
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
			printf("Line %d: invalid key_mgmt '%s'", line, start);
			free(buf);
			return -1;
		}

		if (last)
			break;
		start = end + 1;
	}

	free(buf);
	if (val == 0) {
		printf("Line %d: no key_mgmt values configured.", line);
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


static int hostapd_config_check(struct hostapd_config *conf)
{
	if (conf->ieee802_1x && !conf->eap_authenticator &&
	    !conf->auth_servers) {
		printf("Invalid IEEE 802.1X configuration (no EAP "
		       "authenticator configured).\n");
		return -1;
	}

	if (conf->wpa && (conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    conf->wpa_psk == NULL && conf->wpa_passphrase == NULL) {
		printf("WPA-PSK enabled, but PSK or passphrase is not "
		       "configured.\n");
		return -1;
	}

	return 0;
}


struct hostapd_config * hostapd_config_read(const char *fname)
{
	struct hostapd_config *conf;
	FILE *f;
	char buf[256], *pos;
	int line = 0;
	int errors = 0;
	char *accept_mac_file = NULL, *deny_mac_file = NULL;
#ifdef EAP_AUTHENTICATOR
	char *eap_user_file = NULL;
#endif /* EAP_AUTHENTICATOR */

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

		pos = strchr(buf, '=');
		if (pos == NULL) {
			printf("Line %d: invalid line '%s'\n", line, buf);
			errors++;
			continue;
		}
		*pos = '\0';
		pos++;

		if (strcmp(buf, "interface") == 0) {
			snprintf(conf->iface, sizeof(conf->iface), "%s", pos);
		} else if (strcmp(buf, "bridge") == 0) {
			snprintf(conf->bridge, sizeof(conf->bridge), "%s",
				 pos);
		} else if (strcmp(buf, "driver") == 0) {
			conf->driver = driver_lookup(pos);
			if (conf->driver == NULL) {
				printf("Line %d: invalid/unknown driver "
				       "'%s'\n", line, pos);
				errors++;
			}
		} else if (strcmp(buf, "debug") == 0) {
			conf->debug = atoi(pos);
		} else if (strcmp(buf, "logger_syslog_level") == 0) {
			conf->logger_syslog_level = atoi(pos);
		} else if (strcmp(buf, "logger_stdout_level") == 0) {
			conf->logger_stdout_level = atoi(pos);
		} else if (strcmp(buf, "logger_syslog") == 0) {
			conf->logger_syslog = atoi(pos);
		} else if (strcmp(buf, "logger_stdout") == 0) {
			conf->logger_stdout = atoi(pos);
		} else if (strcmp(buf, "dump_file") == 0) {
			conf->dump_log_name = strdup(pos);
		} else if (strcmp(buf, "ssid") == 0) {
			conf->ssid_len = strlen(pos);
			if (conf->ssid_len >= HOSTAPD_SSID_LEN ||
			    conf->ssid_len < 1) {
				printf("Line %d: invalid SSID '%s'\n", line,
				       pos);
				errors++;
			}
			memcpy(conf->ssid, pos, conf->ssid_len);
			conf->ssid[conf->ssid_len] = '\0';
			conf->ssid_set = 1;
		} else if (strcmp(buf, "macaddr_acl") == 0) {
			conf->macaddr_acl = atoi(pos);
			if (conf->macaddr_acl != ACCEPT_UNLESS_DENIED &&
			    conf->macaddr_acl != DENY_UNLESS_ACCEPTED &&
			    conf->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
				printf("Line %d: unknown macaddr_acl %d\n",
				       line, conf->macaddr_acl);
			}
		} else if (strcmp(buf, "accept_mac_file") == 0) {
			accept_mac_file = strdup(pos);
			if (!accept_mac_file) {
				printf("Line %d: allocation failed\n", line);
				errors++;
			}
		} else if (strcmp(buf, "deny_mac_file") == 0) {
			deny_mac_file = strdup(pos);
			if (!deny_mac_file) {
				printf("Line %d: allocation failed\n", line);
				errors++;
			}
		} else if (strcmp(buf, "assoc_ap_addr") == 0) {
			if (hwaddr_aton(pos, conf->assoc_ap_addr)) {
				printf("Line %d: invalid MAC address '%s'\n",
				       line, pos);
				errors++;
			}
			conf->assoc_ap = 1;
		} else if (strcmp(buf, "ieee8021x") == 0) {
			conf->ieee802_1x = atoi(pos);
#ifdef EAP_AUTHENTICATOR
		} else if (strcmp(buf, "eap_authenticator") == 0) {
			conf->eap_authenticator = atoi(pos);
		} else if (strcmp(buf, "eap_user_file") == 0) {
			free(eap_user_file);
			eap_user_file = strdup(pos);
			if (!eap_user_file) {
				printf("Line %d: allocation failed\n", line);
				errors++;
			}
		} else if (strcmp(buf, "ca_cert") == 0) {
			free(conf->ca_cert);
			conf->ca_cert = strdup(pos);
		} else if (strcmp(buf, "server_cert") == 0) {
			free(conf->server_cert);
			conf->server_cert = strdup(pos);
		} else if (strcmp(buf, "private_key") == 0) {
			free(conf->private_key);
			conf->private_key = strdup(pos);
		} else if (strcmp(buf, "private_key_passwd") == 0) {
			free(conf->private_key_passwd);
			conf->private_key_passwd = strdup(pos);
#ifdef EAP_SIM
		} else if (strcmp(buf, "eap_sim_db") == 0) {
			free(conf->eap_sim_db);
			conf->eap_sim_db = strdup(pos);
#endif /* EAP_SIM */
#endif /* EAP_AUTHENTICATOR */
		} else if (strcmp(buf, "eap_message") == 0) {
			conf->eap_req_id_text = strdup(pos);
		} else if (strcmp(buf, "wep_key_len_broadcast") == 0) {
			conf->default_wep_key_len = atoi(pos);
			if (conf->default_wep_key_len > 13) {
				printf("Line %d: invalid WEP key len %lu "
				       "(= %lu bits)\n", line,
				       (unsigned long)
				       conf->default_wep_key_len,
				       (unsigned long)
				       conf->default_wep_key_len * 8);
				errors++;
			}
		} else if (strcmp(buf, "wep_key_len_unicast") == 0) {
			conf->individual_wep_key_len = atoi(pos);
			if (conf->individual_wep_key_len < 0 ||
			    conf->individual_wep_key_len > 13) {
				printf("Line %d: invalid WEP key len %d "
				       "(= %d bits)\n", line,
				       conf->individual_wep_key_len,
				       conf->individual_wep_key_len * 8);
				errors++;
			}
		} else if (strcmp(buf, "wep_rekey_period") == 0) {
			conf->wep_rekeying_period = atoi(pos);
			if (conf->wep_rekeying_period < 0) {
				printf("Line %d: invalid period %d\n",
				       line, conf->wep_rekeying_period);
				errors++;
			}
		} else if (strcmp(buf, "eap_reauth_period") == 0) {
			conf->eap_reauth_period = atoi(pos);
			if (conf->eap_reauth_period < 0) {
				printf("Line %d: invalid period %d\n",
				       line, conf->eap_reauth_period);
				errors++;
			}
		} else if (strcmp(buf, "eapol_key_index_workaround") == 0) {
			conf->eapol_key_index_workaround = atoi(pos);
#ifdef CONFIG_IAPP
		} else if (strcmp(buf, "iapp_interface") == 0) {
			conf->ieee802_11f = 1;
			snprintf(conf->iapp_iface, sizeof(conf->iapp_iface),
				 "%s", pos);
#endif /* CONFIG_IAPP */
		} else if (strcmp(buf, "own_ip_addr") == 0) {
			if (!inet_aton(pos, &conf->own_ip_addr)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (strcmp(buf, "nas_identifier") == 0) {
			conf->nas_identifier = strdup(pos);
		} else if (strcmp(buf, "auth_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &conf->auth_servers,
				    &conf->num_auth_servers, pos, 1812,
				    &conf->auth_server)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (conf->auth_server &&
			   strcmp(buf, "auth_server_port") == 0) {
			conf->auth_server->port = atoi(pos);
		} else if (conf->auth_server &&
			   strcmp(buf, "auth_server_shared_secret") == 0) {
			int len = strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				printf("Line %d: empty shared secret is not "
				       "allowed.\n", line);
				errors++;
			}
			conf->auth_server->shared_secret = (u8 *) strdup(pos);
			conf->auth_server->shared_secret_len = len;
		} else if (strcmp(buf, "acct_server_addr") == 0) {
			if (hostapd_config_read_radius_addr(
				    &conf->acct_servers,
				    &conf->num_acct_servers, pos, 1813,
				    &conf->acct_server)) {
				printf("Line %d: invalid IP address '%s'\n",
				       line, pos);
				errors++;
			}
		} else if (conf->acct_server &&
			   strcmp(buf, "acct_server_port") == 0) {
			conf->acct_server->port = atoi(pos);
		} else if (conf->acct_server &&
			   strcmp(buf, "acct_server_shared_secret") == 0) {
			int len = strlen(pos);
			if (len == 0) {
				/* RFC 2865, Ch. 3 */
				printf("Line %d: empty shared secret is not "
				       "allowed.\n", line);
				errors++;
			}
			conf->acct_server->shared_secret = (u8 *) strdup(pos);
			conf->acct_server->shared_secret_len = len;
		} else if (strcmp(buf, "radius_retry_primary_interval") == 0) {
			conf->radius_retry_primary_interval = atoi(pos);
		} else if (strcmp(buf, "radius_acct_interim_interval") == 0) {
			conf->radius_acct_interim_interval = atoi(pos);
		} else if (strcmp(buf, "auth_algs") == 0) {
			conf->auth_algs = atoi(pos);
			if (conf->auth_algs == 0) {
				printf("Line %d: no authentication algorithms "
				       "allowed\n",
				       line);
				errors++;
			}
		} else if (strcmp(buf, "wpa") == 0) {
			conf->wpa = atoi(pos);
		} else if (strcmp(buf, "wpa_group_rekey") == 0) {
			conf->wpa_group_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_strict_rekey") == 0) {
			conf->wpa_strict_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_gmk_rekey") == 0) {
			conf->wpa_gmk_rekey = atoi(pos);
		} else if (strcmp(buf, "wpa_passphrase") == 0) {
			int len = strlen(pos);
			if (len < 8 || len > 63) {
				printf("Line %d: invalid WPA passphrase length"
				       " %d (expected 8..63)\n", line, len);
				errors++;
			} else {
				free(conf->wpa_passphrase);
				conf->wpa_passphrase = strdup(pos);
			}
		} else if (strcmp(buf, "wpa_psk") == 0) {
			free(conf->wpa_psk);
			conf->wpa_psk = malloc(sizeof(struct hostapd_wpa_psk));
			if (conf->wpa_psk) {
				memset(conf->wpa_psk, 0,
				       sizeof(struct hostapd_wpa_psk));
			}
			if (conf->wpa_psk == NULL)
				errors++;
			else if (hexstr2bin(pos, conf->wpa_psk->psk, PMK_LEN)
				 || pos[PMK_LEN * 2] != '\0') {
				printf("Line %d: Invalid PSK '%s'.\n", line,
				       pos);
				errors++;
			} else {
				conf->wpa_psk->group = 1;
			}
		} else if (strcmp(buf, "wpa_psk_file") == 0) {
			free(conf->wpa_psk_file);
			conf->wpa_psk_file = strdup(pos);
			if (!conf->wpa_psk_file) {
				printf("Line %d: allocation failed\n", line);
				errors++;
			}
		} else if (strcmp(buf, "wpa_key_mgmt") == 0) {
			conf->wpa_key_mgmt =
				hostapd_config_parse_key_mgmt(line, pos);
			if (conf->wpa_key_mgmt == -1)
				errors++;
		} else if (strcmp(buf, "wpa_pairwise") == 0) {
			conf->wpa_pairwise =
				hostapd_config_parse_cipher(line, pos);
			if (conf->wpa_pairwise == -1 ||
			    conf->wpa_pairwise == 0)
				errors++;
			else if (conf->wpa_pairwise &
				 (WPA_CIPHER_NONE | WPA_CIPHER_WEP40 |
				  WPA_CIPHER_WEP104)) {
				printf("Line %d: unsupported pairwise "
				       "cipher suite '%s'\n",
				       conf->wpa_pairwise, pos);
				errors++;
			} else {
				if (conf->wpa_pairwise & WPA_CIPHER_TKIP)
					conf->wpa_group = WPA_CIPHER_TKIP;
				else
					conf->wpa_group = WPA_CIPHER_CCMP;
			}
#ifdef CONFIG_RSN_PREAUTH
		} else if (strcmp(buf, "rsn_preauth") == 0) {
			conf->rsn_preauth = atoi(pos);
		} else if (strcmp(buf, "rsn_preauth_interfaces") == 0) {
			conf->rsn_preauth_interfaces = strdup(pos);
#endif /* CONFIG_RSN_PREAUTH */
		} else if (strcmp(buf, "ctrl_interface") == 0) {
			free(conf->ctrl_interface);
			conf->ctrl_interface = strdup(pos);
		} else if (strcmp(buf, "ctrl_interface_group") == 0) {
			struct group *grp;
			char *endp;
			const char *group = pos;

			grp = getgrnam(group);
			if (grp) {
				conf->ctrl_interface_gid = grp->gr_gid;
				wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
					   " (from group name '%s')",
					   conf->ctrl_interface_gid, group);
				continue;
			}

			/* Group name not found - try to parse this as gid */
			conf->ctrl_interface_gid = strtol(group, &endp, 10);
			if (*group == '\0' || *endp != '\0') {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid group "
					   "'%s'", line, group);
				errors++;
				continue;
			}
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   conf->ctrl_interface_gid);
#ifdef RADIUS_SERVER
		} else if (strcmp(buf, "radius_server_clients") == 0) {
			free(conf->radius_server_clients);
			conf->radius_server_clients = strdup(pos);
		} else if (strcmp(buf, "radius_server_auth_port") == 0) {
			conf->radius_server_auth_port = atoi(pos);
#endif /* RADIUS_SERVER */
		} else {
			printf("Line %d: unknown configuration item '%s'\n",
			       line, buf);
			errors++;
		}
	}

	fclose(f);

	if (hostapd_config_read_maclist(accept_mac_file, &conf->accept_mac,
					&conf->num_accept_mac))
		errors++;
	free(accept_mac_file);
	if (hostapd_config_read_maclist(deny_mac_file, &conf->deny_mac,
					&conf->num_deny_mac))
		errors++;
	free(deny_mac_file);

#ifdef EAP_AUTHENTICATOR
	if (hostapd_config_read_eap_user(eap_user_file, conf))
		errors++;
	free(eap_user_file);
#endif /* EAP_AUTHENTICATOR */

	conf->auth_server = conf->auth_servers;
	conf->acct_server = conf->acct_servers;

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


void hostapd_config_free(struct hostapd_config *conf)
{
	struct hostapd_wpa_psk *psk, *prev;
	struct hostapd_eap_user *user, *prev_user;

	if (conf == NULL)
		return;

	psk = conf->wpa_psk;
	while (psk) {
		prev = psk;
		psk = psk->next;
		free(prev);
	}

	free(conf->wpa_passphrase);
	free(conf->wpa_psk_file);

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
	hostapd_config_free_radius(conf->auth_servers, conf->num_auth_servers);
	hostapd_config_free_radius(conf->acct_servers, conf->num_acct_servers);
	free(conf->rsn_preauth_interfaces);
	free(conf->ctrl_interface);
	free(conf->ca_cert);
	free(conf->server_cert);
	free(conf->private_key);
	free(conf->private_key_passwd);
	free(conf->eap_sim_db);
	free(conf->radius_server_clients);
	free(conf);
}


/* Perform a binary search for given MAC address from a pre-sorted list.
 * Returns 1 if address is in the list or 0 if not. */
int hostapd_maclist_found(macaddr *list, int num_entries, u8 *addr)
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


const u8 * hostapd_get_psk(const struct hostapd_config *conf, const u8 *addr,
			   const u8 *prev_psk)
{
	struct hostapd_wpa_psk *psk;
	int next_ok = prev_psk == NULL;

	for (psk = conf->wpa_psk; psk != NULL; psk = psk->next) {
		if (next_ok &&
		    (psk->group || memcmp(psk->addr, addr, ETH_ALEN) == 0))
			return psk->psk;

		if (psk->psk == prev_psk)
			next_ok = 1;
	}

	return NULL;
}


const struct hostapd_eap_user *
hostapd_get_eap_user(const struct hostapd_config *conf, const u8 *identity,
		     size_t identity_len, int phase2)
{
	struct hostapd_eap_user *user = conf->eap_user;

	while (user) {
		if (!phase2 && user->identity == NULL) {
			/* Wildcard match */
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
