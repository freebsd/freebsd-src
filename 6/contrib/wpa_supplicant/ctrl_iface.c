/*
 * WPA Supplicant / UNIX domain and UDP socket -based control interface
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/uio.h>
#endif /* CONFIG_NATIVE_WINDOWS */

#include "common.h"
#include "eloop.h"
#include "wpa.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "eapol_sm.h"
#include "wpa_supplicant_i.h"
#include "ctrl_iface.h"
#include "l2_packet.h"
#include "preauth.h"
#include "wpa_ctrl.h"
#include "eap.h"


#ifdef CONFIG_CTRL_IFACE_UDP
#define CTRL_IFACE_SOCK struct sockaddr_in
#else /* CONFIG_CTRL_IFACE_UDP */
#define CTRL_IFACE_SOCK struct sockaddr_un
#endif /* CONFIG_CTRL_IFACE_UDP */


/**
 * struct wpa_ctrl_dst - Internal data structure of control interface monitors
 *
 * This structure is used to store information about registered control
 * interface monitors into struct wpa_supplicant. This data is private to
 * ctrl_iface.c and should not be touched directly from other files.
 */
struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	CTRL_IFACE_SOCK addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


static int wpa_supplicant_ctrl_iface_set(struct wpa_supplicant *wpa_s,
					 char *cmd)
{
	char *value;
	int ret = 0;

	value = strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (strcasecmp(cmd, "EAPOL::heldPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   atoi(value), -1, -1, -1);
	} else if (strcasecmp(cmd, "EAPOL::authPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, atoi(value), -1, -1);
	} else if (strcasecmp(cmd, "EAPOL::startPeriod") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, -1, atoi(value), -1);
	} else if (strcasecmp(cmd, "EAPOL::maxStart") == 0) {
		eapol_sm_configure(wpa_s->eapol,
				   -1, -1, -1, atoi(value));
	} else if (strcasecmp(cmd, "dot11RSNAConfigPMKLifetime") == 0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_LIFETIME,
				     atoi(value)))
			ret = -1;
	} else if (strcasecmp(cmd, "dot11RSNAConfigPMKReauthThreshold") == 0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_REAUTH_THRESHOLD,
				     atoi(value)))
			ret = -1;
	} else if (strcasecmp(cmd, "dot11RSNAConfigSATimeout") == 0) {
		if (wpa_sm_set_param(wpa_s->wpa, RSNA_SA_TIMEOUT, atoi(value)))
			ret = -1;
	} else
		ret = -1;

	return ret;
}


static int wpa_supplicant_ctrl_iface_preauth(struct wpa_supplicant *wpa_s,
					     char *addr)
{
	u8 bssid[ETH_ALEN];

	if (hwaddr_aton(addr, bssid)) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE PREAUTH: invalid address "
			   "'%s'", addr);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE PREAUTH " MACSTR, MAC2STR(bssid));
	rsn_preauth_deinit(wpa_s->wpa);
	if (rsn_preauth_init(wpa_s->wpa, bssid, wpa_s->current_ssid))
		return -1;

	return 0;
}


static int wpa_supplicant_ctrl_iface_attach(struct wpa_supplicant *wpa_s,
					    CTRL_IFACE_SOCK *from,
					    socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = malloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	memset(dst, 0, sizeof(*dst));
	memcpy(&dst->addr, from, sizeof(CTRL_IFACE_SOCK));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = wpa_s->ctrl_dst;
	wpa_s->ctrl_dst = dst;
#ifdef CONFIG_CTRL_IFACE_UDP
	wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor attached %s:%d",
		   inet_ntoa(from->sin_addr), ntohs(from->sin_port));
#else /* CONFIG_CTRL_IFACE_UDP */
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path, fromlen);
#endif /* CONFIG_CTRL_IFACE_UDP */
	return 0;
}


static int wpa_supplicant_ctrl_iface_detach(struct wpa_supplicant *wpa_s,
					    CTRL_IFACE_SOCK *from,
					    socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = wpa_s->ctrl_dst;
	while (dst) {
#ifdef CONFIG_CTRL_IFACE_UDP
		if (from->sin_addr.s_addr == dst->addr.sin_addr.s_addr &&
		    from->sin_port == dst->addr.sin_port) {
			if (prev == NULL)
				wpa_s->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			free(dst);
			wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor detached "
				   "%s:%d", inet_ntoa(from->sin_addr),
				   ntohs(from->sin_port));
			return 0;
		}
#else /* CONFIG_CTRL_IFACE_UDP */
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			if (prev == NULL)
				wpa_s->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			free(dst);
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path, fromlen);
			return 0;
		}
#endif /* CONFIG_CTRL_IFACE_UDP */
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int wpa_supplicant_ctrl_iface_level(struct wpa_supplicant *wpa_s,
					   CTRL_IFACE_SOCK *from,
					   socklen_t fromlen,
					   char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = wpa_s->ctrl_dst;
	while (dst) {
#ifdef CONFIG_CTRL_IFACE_UDP
		if (from->sin_addr.s_addr == dst->addr.sin_addr.s_addr &&
		    from->sin_port == dst->addr.sin_port) {
			wpa_printf(MSG_DEBUG, "CTRL_IFACE changed monitor "
				   "level %s:%d", inet_ntoa(from->sin_addr),
				   ntohs(from->sin_port));
			dst->debug_level = atoi(level);
			return 0;
		}
#else /* CONFIG_CTRL_IFACE_UDP */
		if (fromlen == dst->addrlen &&
		    memcmp(from->sun_path, dst->addr.sun_path, fromlen) == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path, fromlen);
			dst->debug_level = atoi(level);
			return 0;
		}
#endif /* CONFIG_CTRL_IFACE_UDP */
		dst = dst->next;
	}

	return -1;
}


static int wpa_supplicant_ctrl_iface_ctrl_rsp(struct wpa_supplicant *wpa_s,
					      char *rsp)
{
	char *pos, *id_pos;
	int id;
	struct wpa_ssid *ssid;

	pos = strchr(rsp, '-');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id_pos = pos;
	pos = strchr(pos, ':');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id = atoi(id_pos);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: field=%s id=%d", rsp, id);
	wpa_hexdump_ascii_key(MSG_DEBUG, "CTRL_IFACE: value",
			      (u8 *) pos, strlen(pos));

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find SSID id=%d "
			   "to update", id);
		return -1;
	}

	if (strcmp(rsp, "IDENTITY") == 0) {
		free(ssid->identity);
		ssid->identity = (u8 *) strdup(pos);
		ssid->identity_len = strlen(pos);
		ssid->pending_req_identity = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (strcmp(rsp, "PASSWORD") == 0) {
		free(ssid->password);
		ssid->password = (u8 *) strdup(pos);
		ssid->password_len = strlen(pos);
		ssid->pending_req_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (strcmp(rsp, "NEW_PASSWORD") == 0) {
		free(ssid->new_password);
		ssid->new_password = (u8 *) strdup(pos);
		ssid->new_password_len = strlen(pos);
		ssid->pending_req_new_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (strcmp(rsp, "PIN") == 0) {
		free(ssid->pin);
		ssid->pin = strdup(pos);
		ssid->pending_req_pin = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else if (strcmp(rsp, "OTP") == 0) {
		free(ssid->otp);
		ssid->otp = (u8 *) strdup(pos);
		ssid->otp_len = strlen(pos);
		free(ssid->pending_req_otp);
		ssid->pending_req_otp = NULL;
		ssid->pending_req_otp_len = 0;
	} else if (strcmp(rsp, "PASSPHRASE") == 0) {
		free(ssid->private_key_passwd);
		ssid->private_key_passwd = (u8 *) strdup(pos);
		ssid->pending_req_passphrase = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
	} else {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown field '%s'", rsp);
		return -1;
	}

	return 0;
}


static int wpa_supplicant_ctrl_iface_status(struct wpa_supplicant *wpa_s,
					    const char *params,
					    char *buf, size_t buflen)
{
	char *pos, *end, tmp[30];
	int res, verbose;

	verbose = strcmp(params, "-VERBOSE") == 0;
	pos = buf;
	end = buf + buflen;
	if (wpa_s->wpa_state >= WPA_ASSOCIATED) {
		pos += snprintf(pos, end - pos, "bssid=" MACSTR "\n",
				MAC2STR(wpa_s->bssid));
		if (wpa_s->current_ssid) {
			pos += snprintf(pos, end - pos, "ssid=%s\n",
					wpa_ssid_txt(wpa_s->current_ssid->ssid,
						     wpa_s->current_ssid->
						     ssid_len));
		}

		pos += wpa_sm_get_status(wpa_s->wpa, pos, end - pos, verbose);
	}
	pos += snprintf(pos, end - pos, "wpa_state=%s\n",
			wpa_supplicant_state_txt(wpa_s->wpa_state));

	if (wpa_s->l2 &&
	    l2_packet_get_ip_addr(wpa_s->l2, tmp, sizeof(tmp)) >= 0)
		pos += snprintf(pos, end - pos, "ip_address=%s\n", tmp);

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		res = eapol_sm_get_status(wpa_s->eapol, pos, end - pos,
					  verbose);
		if (res >= 0)
			pos += res;
	}

	res = rsn_preauth_get_status(wpa_s->wpa, pos, end - pos, verbose);
	if (res >= 0)
		pos += res;

	return pos - buf;
}


static int wpa_supplicant_ctrl_iface_bssid(struct wpa_supplicant *wpa_s,
					   char *cmd)
{
	char *pos;
	int id;
	struct wpa_ssid *ssid;
	u8 bssid[ETH_ALEN];

	/* cmd: "<network id> <BSSID>" */
	pos = strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: id=%d bssid='%s'", id, pos);
	if (hwaddr_aton(pos, bssid)) {
		wpa_printf(MSG_DEBUG ,"CTRL_IFACE: invalid BSSID '%s'", pos);
		return -1;
	}

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find SSID id=%d "
			   "to update", id);
		return -1;
	}

	memcpy(ssid->bssid, bssid, ETH_ALEN);
	ssid->bssid_set =
		memcmp(bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN) != 0;
		

	return 0;
}


static int wpa_supplicant_ctrl_iface_list_networks(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	char *pos, *end;
	struct wpa_ssid *ssid;

	pos = buf;
	end = buf + buflen;
	pos += snprintf(pos, end - pos, "network id / ssid / bssid / flags\n");

	ssid = wpa_s->conf->ssid;
	while (ssid) {
		pos += snprintf(pos, end - pos, "%d\t%s",
				ssid->id,
				wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		if (ssid->bssid_set) {
			pos += snprintf(pos, end - pos, "\t" MACSTR,
					MAC2STR(ssid->bssid));
		} else {
			pos += snprintf(pos, end - pos, "\tany");
		}
		pos += snprintf(pos, end - pos, "\t%s%s",
				ssid == wpa_s->current_ssid ? "[CURRENT]" : "",
				ssid->disabled ? "[DISABLED]" : "");
		pos += snprintf(pos, end - pos, "\n");

		ssid = ssid->next;
	}

	return pos - buf;
}


static char * wpa_supplicant_cipher_txt(char *pos, char *end, int cipher)
{
	int first = 1;
	pos += snprintf(pos, end - pos, "-");
	if (cipher & WPA_CIPHER_NONE) {
		pos += snprintf(pos, end - pos, "%sNONE", first ? "" : "+");
		first = 0;
	}
	if (cipher & WPA_CIPHER_WEP40) {
		pos += snprintf(pos, end - pos, "%sWEP40", first ? "" : "+");
		first = 0;
	}
	if (cipher & WPA_CIPHER_WEP104) {
		pos += snprintf(pos, end - pos, "%sWEP104", first ? "" : "+");
		first = 0;
	}
	if (cipher & WPA_CIPHER_TKIP) {
		pos += snprintf(pos, end - pos, "%sTKIP", first ? "" : "+");
		first = 0;
	}
	if (cipher & WPA_CIPHER_CCMP) {
		pos += snprintf(pos, end - pos, "%sCCMP", first ? "" : "+");
		first = 0;
	}
	return pos;
}


static char * wpa_supplicant_ie_txt(struct wpa_supplicant *wpa_s,
				    char *pos, char *end, const char *proto,
				    const u8 *ie, size_t ie_len)
{
	struct wpa_ie_data data;
	int first;

	pos += snprintf(pos, end - pos, "[%s-", proto);

	if (wpa_parse_wpa_ie(ie, ie_len, &data) < 0) {
		pos += snprintf(pos, end - pos, "?]");
		return pos;
	}

	first = 1;
	if (data.key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		pos += snprintf(pos, end - pos, "%sEAP", first ? "" : "+");
		first = 0;
	}
	if (data.key_mgmt & WPA_KEY_MGMT_PSK) {
		pos += snprintf(pos, end - pos, "%sPSK", first ? "" : "+");
		first = 0;
	}
	if (data.key_mgmt & WPA_KEY_MGMT_WPA_NONE) {
		pos += snprintf(pos, end - pos, "%sNone", first ? "" : "+");
		first = 0;
	}

	pos = wpa_supplicant_cipher_txt(pos, end, data.pairwise_cipher);

	if (data.capabilities & WPA_CAPABILITY_PREAUTH)
		pos += snprintf(pos, end - pos, "-preauth");

	pos += snprintf(pos, end - pos, "]");

	return pos;
}


static int wpa_supplicant_ctrl_iface_scan_results(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	char *pos, *end;
	struct wpa_scan_result *res;
	int i;

	if (wpa_s->scan_results == NULL &&
	    wpa_supplicant_get_scan_results(wpa_s) < 0)
		return 0;

	pos = buf;
	end = buf + buflen;
	pos += snprintf(pos, end - pos, "bssid / frequency / signal level / "
			"flags / ssid\n");

	for (i = 0; i < wpa_s->num_scan_results; i++) {
		res = &wpa_s->scan_results[i];
		pos += snprintf(pos, end - pos, MACSTR "\t%d\t%d\t",
				MAC2STR(res->bssid), res->freq, res->level);
		if (res->wpa_ie_len) {
			pos = wpa_supplicant_ie_txt(wpa_s, pos, end, "WPA",
						    res->wpa_ie,
						    res->wpa_ie_len);
		}
		if (res->rsn_ie_len) {
			pos = wpa_supplicant_ie_txt(wpa_s, pos, end, "WPA2",
						    res->rsn_ie,
						    res->rsn_ie_len);
		}
		if (!res->wpa_ie_len && !res->rsn_ie_len &&
		    res->caps & IEEE80211_CAP_PRIVACY)
			pos += snprintf(pos, end - pos, "[WEP]");
		if (res->caps & IEEE80211_CAP_IBSS)
			pos += snprintf(pos, end - pos, "[IBSS]");

		pos += snprintf(pos, end - pos, "\t%s",
				wpa_ssid_txt(res->ssid, res->ssid_len));

		pos += snprintf(pos, end - pos, "\n");
	}

	return pos - buf;
}


static int wpa_supplicant_ctrl_iface_select_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" or "any" */
	if (strcmp(cmd, "any") == 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SELECT_NETWORK any");
		ssid = wpa_s->conf->ssid;
		while (ssid) {
			ssid->disabled = 0;
			ssid = ssid->next;
		}
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
		return 0;
	}

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: SELECT_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid != wpa_s->current_ssid && wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);

	/* Mark all other networks disabled and trigger reassociation */
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		ssid->disabled = id != ssid->id;
		ssid = ssid->next;
	}
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, 0);

	return 0;
}


static int wpa_supplicant_ctrl_iface_enable_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: ENABLE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (wpa_s->current_ssid == NULL && ssid->disabled) {
		/*
		 * Try to reassociate since there is no current configuration
		 * and a new network was made available. */
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
	ssid->disabled = 0;

	return 0;
}


static int wpa_supplicant_ctrl_iface_disable_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: DISABLE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	ssid->disabled = 1;

	return 0;
}


static int wpa_supplicant_ctrl_iface_add_network(
	struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	struct wpa_ssid *ssid;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: ADD_NETWORK");

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return -1;
	ssid->disabled = 1;
	wpa_config_set_network_defaults(ssid);

	return snprintf(buf, buflen, "%d\n", ssid->id);
}


static int wpa_supplicant_ctrl_iface_remove_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;

	/* cmd: "<network id>" */
	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: REMOVE_NETWORK id=%d", id);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL ||
	    wpa_config_remove_network(wpa_s->conf, id) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (ssid == wpa_s->current_ssid)
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);

	return 0;
}


static int wpa_supplicant_ctrl_iface_set_network(
	struct wpa_supplicant *wpa_s, char *cmd)
{
	int id;
	struct wpa_ssid *ssid;
	char *name, *value;

	/* cmd: "<network id> <variable name> <value>" */
	name = strchr(cmd, ' ');
	if (name == NULL)
		return -1;
	*name++ = '\0';

	value = strchr(name, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: SET_NETWORK id=%d name='%s' "
		   "value='%s'", id, name, value);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	if (wpa_config_set(ssid, name, value, 0) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Failed to set network "
			   "variable '%s' to '%s'", name, value);
		return -1;
	}

	if ((strcmp(name, "psk") == 0 && value[0] == '"' && ssid->ssid_len) ||
	    (strcmp(name, "ssid") == 0 && ssid->passphrase))
		wpa_config_update_psk(ssid);

	return 0;
}


static int wpa_supplicant_ctrl_iface_get_network(
	struct wpa_supplicant *wpa_s, char *cmd, char *buf, size_t buflen)
{
	int id;
	struct wpa_ssid *ssid;
	char *name, *value;

	/* cmd: "<network id> <variable name>" */
	name = strchr(cmd, ' ');
	if (name == NULL)
		return -1;
	*name++ = '\0';

	id = atoi(cmd);
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_NETWORK id=%d name='%s'",
		   id, name);

	ssid = wpa_config_get_network(wpa_s->conf, id);
	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Could not find network "
			   "id=%d", id);
		return -1;
	}

	value = wpa_config_get(ssid, name);
	if (value == NULL) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Failed to get network "
			   "variable '%s'", name);
		return -1;
	}

	snprintf(buf, buflen, "%s", value);

	free(value);

	return strlen(buf);
}


static int wpa_supplicant_ctrl_iface_save_config(struct wpa_supplicant *wpa_s)
{
	int ret;

	if (!wpa_s->conf->update_config) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Not allowed "
			   "to update configuration (update_config=0)");
		return -1;
	}

	ret = wpa_config_write(wpa_s->confname, wpa_s->conf);
	if (ret) {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Failed to "
			   "update configuration");
	} else {
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: SAVE_CONFIG - Configuration"
			   " updated");
	}

	return ret;
}


static int wpa_supplicant_ctrl_iface_get_capability(
	struct wpa_supplicant *wpa_s, const char *field, char *buf,
	size_t buflen)
{
	struct wpa_driver_capa capa;
	int res, first = 1;
	char *pos, *end;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_CAPABILITY '%s'", field);

	if (strcmp(field, "eap") == 0) {
		return eap_get_names(buf, buflen);
	}

	res = wpa_drv_get_capa(wpa_s, &capa);

	pos = buf;
	end = pos + buflen;

	if (strcmp(field, "pairwise") == 0) {
		if (res < 0)
			return snprintf(buf, buflen, "CCMP TKIP NONE");

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			pos += snprintf(pos, end - pos, "%sCCMP",
					first ? "" : " ");
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			pos += snprintf(pos, end - pos, "%sTKIP",
					first ? "" : " ");
			first = 0;
		}

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE) {
			pos += snprintf(pos, end - pos, "%sNONE",
					first ? "" : " ");
			first = 0;
		}

		return pos - buf;
	}

	if (strcmp(field, "group") == 0) {
		if (res < 0)
			return snprintf(buf, buflen, "CCMP TKIP WEP104 WEP40");

		if (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP) {
			pos += snprintf(pos, end - pos, "%sCCMP",
					first ? "" : " ");
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_TKIP) {
			pos += snprintf(pos, end - pos, "%sTKIP",
					first ? "" : " ");
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP104) {
			pos += snprintf(pos, end - pos, "%sWEP104",
					first ? "" : " ");
			first = 0;
		}

		if (capa.enc & WPA_DRIVER_CAPA_ENC_WEP40) {
			pos += snprintf(pos, end - pos, "%sWEP40",
					first ? "" : " ");
			first = 0;
		}

		return pos - buf;
	}

	if (strcmp(field, "key_mgmt") == 0) {
		if (res < 0) {
			return snprintf(buf, buflen, "WPA-PSK WPA-EAP "
					"IEEE8021X WPA-NONE NONE");
		}

		pos += snprintf(pos, end - pos, "NONE IEEE8021X");

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2))
			pos += snprintf(pos, end - pos, " WPA-EAP");

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK))
			pos += snprintf(pos, end - pos, " WPA-PSK");

		if (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA_NONE)
			pos += snprintf(pos, end - pos, " WPA-NONE");

		return pos - buf;
	}

	if (strcmp(field, "proto") == 0) {
		if (res < 0)
			return snprintf(buf, buflen, "RSN WPA");

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
			pos += snprintf(pos, end - pos, "%sRSN",
					first ? "" : " ");
			first = 0;
		}

		if (capa.key_mgmt & (WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				     WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK)) {
			pos += snprintf(pos, end - pos, "%sWPA",
					first ? "" : " ");
			first = 0;
		}

		return pos - buf;
	}

	if (strcmp(field, "auth_alg") == 0) {
		if (res < 0)
			return snprintf(buf, buflen, "OPEN SHARED LEAP");

		if (capa.auth & (WPA_DRIVER_AUTH_OPEN)) {
			pos += snprintf(pos, end - pos, "%sOPEN",
					first ? "" : " ");
			first = 0;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_SHARED)) {
			pos += snprintf(pos, end - pos, "%sSHARED",
					first ? "" : " ");
			first = 0;
		}

		if (capa.auth & (WPA_DRIVER_AUTH_LEAP)) {
			pos += snprintf(pos, end - pos, "%sLEAP",
					first ? "" : " ");
			first = 0;
		}

		return pos - buf;
	}

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown GET_CAPABILITY field '%s'",
		   field);

	return -1;
}


static void wpa_supplicant_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	char buf[256];
	int res;
	CTRL_IFACE_SOCK from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 2048;
	int reply_len;
	int new_attached = 0, ctrl_rsp = 0;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	if (strncmp(buf, WPA_CTRL_RSP, strlen(WPA_CTRL_RSP)) == 0) {
		wpa_hexdump_ascii_key(MSG_DEBUG, "RX ctrl_iface",
				      (u8 *) buf, res);
	} else {
		wpa_hexdump_ascii(MSG_DEBUG, "RX ctrl_iface", (u8 *) buf, res);
	}

	reply = malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (strcmp(buf, "PING") == 0) {
		memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (strcmp(buf, "MIB") == 0) {
		reply_len = wpa_sm_get_mib(wpa_s->wpa, reply, reply_size);
		if (reply_len >= 0) {
			res = eapol_sm_get_mib(wpa_s->eapol, reply + reply_len,
					       reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
	} else if (strncmp(buf, "STATUS", 6) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_status(
			wpa_s, buf + 6, reply, reply_size);
	} else if (strcmp(buf, "PMKSA") == 0) {
		reply_len = pmksa_cache_list(wpa_s->wpa, reply, reply_size);
	} else if (strncmp(buf, "SET ", 4) == 0) {
		if (wpa_supplicant_ctrl_iface_set(wpa_s, buf + 4))
			reply_len = -1;
	} else if (strcmp(buf, "LOGON") == 0) {
		eapol_sm_notify_logoff(wpa_s->eapol, FALSE);
	} else if (strcmp(buf, "LOGOFF") == 0) {
		eapol_sm_notify_logoff(wpa_s->eapol, TRUE);
	} else if (strcmp(buf, "REASSOCIATE") == 0) {
		wpa_s->disconnected = 0;
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (strncmp(buf, "PREAUTH ", 8) == 0) {
		if (wpa_supplicant_ctrl_iface_preauth(wpa_s, buf + 8))
			reply_len = -1;
	} else if (strcmp(buf, "ATTACH") == 0) {
		if (wpa_supplicant_ctrl_iface_attach(wpa_s, &from, fromlen))
			reply_len = -1;
		else
			new_attached = 1;
	} else if (strcmp(buf, "DETACH") == 0) {
		if (wpa_supplicant_ctrl_iface_detach(wpa_s, &from, fromlen))
			reply_len = -1;
	} else if (strncmp(buf, "LEVEL ", 6) == 0) {
		if (wpa_supplicant_ctrl_iface_level(wpa_s, &from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (strncmp(buf, WPA_CTRL_RSP, strlen(WPA_CTRL_RSP)) == 0) {
		if (wpa_supplicant_ctrl_iface_ctrl_rsp(
			    wpa_s, buf + strlen(WPA_CTRL_RSP)))
			reply_len = -1;
		else
			ctrl_rsp = 1;
	} else if (strcmp(buf, "RECONFIGURE") == 0) {
		if (wpa_supplicant_reload_configuration(wpa_s))
			reply_len = -1;
	} else if (strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else if (strncmp(buf, "BSSID ", 6) == 0) {
		if (wpa_supplicant_ctrl_iface_bssid(wpa_s, buf + 6))
			reply_len = -1;
	} else if (strcmp(buf, "LIST_NETWORKS") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_list_networks(
			wpa_s, reply, reply_size);
	} else if (strcmp(buf, "DISCONNECT") == 0) {
		wpa_s->disconnected = 1;
		wpa_supplicant_disassociate(wpa_s, REASON_DEAUTH_LEAVING);
	} else if (strcmp(buf, "SCAN") == 0) {
		wpa_s->scan_req = 2;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (strcmp(buf, "SCAN_RESULTS") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_scan_results(
			wpa_s, reply, reply_size);
	} else if (strncmp(buf, "SELECT_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_select_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (strncmp(buf, "ENABLE_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_enable_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (strncmp(buf, "DISABLE_NETWORK ", 16) == 0) {
		if (wpa_supplicant_ctrl_iface_disable_network(wpa_s, buf + 16))
			reply_len = -1;
	} else if (strcmp(buf, "ADD_NETWORK") == 0) {
		reply_len = wpa_supplicant_ctrl_iface_add_network(
			wpa_s, reply, reply_size);
	} else if (strncmp(buf, "REMOVE_NETWORK ", 15) == 0) {
		if (wpa_supplicant_ctrl_iface_remove_network(wpa_s, buf + 15))
			reply_len = -1;
	} else if (strncmp(buf, "SET_NETWORK ", 12) == 0) {
		if (wpa_supplicant_ctrl_iface_set_network(wpa_s, buf + 12))
			reply_len = -1;
	} else if (strncmp(buf, "GET_NETWORK ", 12) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_get_network(
			wpa_s, buf + 12, reply, reply_size);
	} else if (strcmp(buf, "SAVE_CONFIG") == 0) {
		if (wpa_supplicant_ctrl_iface_save_config(wpa_s))
			reply_len = -1;
	} else if (strncmp(buf, "GET_CAPABILITY ", 15) == 0) {
		reply_len = wpa_supplicant_ctrl_iface_get_capability(
			wpa_s, buf + 15, reply, reply_size);
	} else {
		memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}
	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	free(reply);

	if (new_attached)
		eapol_sm_notify_ctrl_attached(wpa_s->eapol);
	if (ctrl_rsp)
		eapol_sm_notify_ctrl_response(wpa_s->eapol);
}


#ifndef CONFIG_CTRL_IFACE_UDP
static char * wpa_supplicant_ctrl_iface_path(struct wpa_supplicant *wpa_s)
{
	char *buf;
	size_t len;

	if (wpa_s->conf->ctrl_interface == NULL)
		return NULL;

	len = strlen(wpa_s->conf->ctrl_interface) + strlen(wpa_s->ifname) + 2;
	buf = malloc(len);
	if (buf == NULL)
		return NULL;

	snprintf(buf, len, "%s/%s",
		 wpa_s->conf->ctrl_interface, wpa_s->ifname);
#ifdef __CYGWIN__
	{
		/* Windows/WinPcap uses interface names that are not suitable
		 * as a file name - convert invalid chars to underscores */
		char *pos = buf;
		while (*pos) {
			if (*pos == '\\')
				*pos = '_';
			pos++;
		}
	}
#endif /* __CYGWIN__ */
	return buf;
}
#endif /* CONFIG_CTRL_IFACE_UDP */


/**
 * wpa_supplicant_ctrl_iface_init - Initialize control interface
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * Initialize the control interface and start receiving commands from external
 * programs.
 */
int wpa_supplicant_ctrl_iface_init(struct wpa_supplicant *wpa_s)
{
	CTRL_IFACE_SOCK addr;
	int s = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
	char *fname = NULL;
#endif /* CONFIG_CTRL_IFACE_UDP */

	wpa_s->ctrl_sock = -1;

	if (wpa_s->conf->ctrl_interface == NULL)
		return 0;

#ifdef CONFIG_CTRL_IFACE_UDP
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_INET)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((127 << 24) | 1);
	addr.sin_port = htons(WPA_CTRL_IFACE_PORT);
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(AF_INET)");
		goto fail;
	}
#else /* CONFIG_CTRL_IFACE_UDP */
	if (mkdir(wpa_s->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (wpa_s->conf->ctrl_interface_gid_set &&
	    chown(wpa_s->conf->ctrl_interface, 0,
		  wpa_s->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

	if (strlen(wpa_s->conf->ctrl_interface) + 1 + strlen(wpa_s->ifname) >=
	    sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	fname = wpa_supplicant_ctrl_iface_path(wpa_s);
	if (fname == NULL)
		goto fail;
	strncpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(PF_UNIX)");
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   fname);
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (wpa_s->conf->ctrl_interface_gid_set &&
	    chown(fname, 0, wpa_s->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	free(fname);
#endif /* CONFIG_CTRL_IFACE_UDP */

	wpa_s->ctrl_sock = s;
	eloop_register_read_sock(s, wpa_supplicant_ctrl_iface_receive, wpa_s,
				 NULL);

	return 0;

fail:
	if (s >= 0)
		close(s);
#ifndef CONFIG_CTRL_IFACE_UDP
	if (fname) {
		unlink(fname);
		free(fname);
	}
#endif /* CONFIG_CTRL_IFACE_UDP */
	return -1;
}


/**
 * wpa_supplicant_ctrl_iface_deinit - Deinitialize control interface
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * Deinitialize the control interface that was initialized with
 * wpa_supplicant_ctrl_iface_init().
 */
void wpa_supplicant_ctrl_iface_deinit(struct wpa_supplicant *wpa_s)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (wpa_s->ctrl_sock > -1) {
#ifndef CONFIG_CTRL_IFACE_UDP
		char *fname;
#endif /* CONFIG_CTRL_IFACE_UDP */
		eloop_unregister_read_sock(wpa_s->ctrl_sock);
		if (wpa_s->ctrl_dst) {
			/*
			 * Wait a second before closing the control socket if
			 * there are any attached monitors in order to allow
			 * them to receive any pending messages.
			 */
			wpa_printf(MSG_DEBUG, "CTRL_IFACE wait for attached "
				   "monitors to receive messages");
			sleep(1);
		}
		close(wpa_s->ctrl_sock);
		wpa_s->ctrl_sock = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
		fname = wpa_supplicant_ctrl_iface_path(wpa_s);
		if (fname)
			unlink(fname);
		free(fname);

		if (rmdir(wpa_s->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				perror("rmdir[ctrl_interface]");
			}
		}
#endif /* CONFIG_CTRL_IFACE_UDP */
	}

	dst = wpa_s->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		free(prev);
	}
}


/**
 * wpa_supplicant_ctrl_iface_send - Send a control interface packet to monitors
 * @wpa_s: Pointer to wpa_supplicant data
 * @level: Priority level of the message
 * @buf: Message data
 * @len: Message length
 *
 * Send a packet to all monitor programs attached to the control interface.
 */
void wpa_supplicant_ctrl_iface_send(struct wpa_supplicant *wpa_s, int level,
				    char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	char levelstr[10];
	int idx;
#ifdef CONFIG_CTRL_IFACE_UDP
	char *sbuf;
	int llen;

	dst = wpa_s->ctrl_dst;
	if (wpa_s->ctrl_sock < 0 || dst == NULL)
		return;

	snprintf(levelstr, sizeof(levelstr), "<%d>", level);

	llen = strlen(levelstr);
	sbuf = malloc(llen + len);
	if (sbuf == NULL)
		return;

	memcpy(sbuf, levelstr, llen);
	memcpy(sbuf + llen, buf, len);

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_printf(MSG_DEBUG, "CTRL_IFACE monitor send %s:%d",
				   inet_ntoa(dst->addr.sin_addr),
				   ntohs(dst->addr.sin_port));
			if (sendto(wpa_s->ctrl_sock, sbuf, llen + len, 0,
				   (struct sockaddr *) &dst->addr,
				   sizeof(dst->addr)) < 0) {
				perror("sendto(CTRL_IFACE monitor)");
				dst->errors++;
				if (dst->errors > 10) {
					wpa_supplicant_ctrl_iface_detach(
						wpa_s, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
	free(sbuf);
#else /* CONFIG_CTRL_IFACE_UDP */
	struct msghdr msg;
	struct iovec io[2];

	dst = wpa_s->ctrl_dst;
	if (wpa_s->ctrl_sock < 0 || dst == NULL)
		return;

	snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	io[0].iov_base = levelstr;
	io[0].iov_len = strlen(levelstr);
	io[1].iov_base = buf;
	io[1].iov_len = len;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen);
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(wpa_s->ctrl_sock, &msg, 0) < 0) {
				perror("sendmsg(CTRL_IFACE monitor)");
				dst->errors++;
				if (dst->errors > 10) {
					wpa_supplicant_ctrl_iface_detach(
						wpa_s, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
#endif /* CONFIG_CTRL_IFACE_UDP */
}


/**
 * wpa_supplicant_ctrl_iface_wait - Wait for ctrl_iface monitor
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * Wait until the first message from an external program using the control
 * interface is received. This function can be used to delay normal startup
 * processing to allow control interface programs to attach with
 * %wpa_supplicant before normal operations are started.
 */
void wpa_supplicant_ctrl_iface_wait(struct wpa_supplicant *wpa_s)
{
	fd_set rfds;

	if (wpa_s->ctrl_sock < 0)
		return;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE - %s - wait for monitor",
		   wpa_s->ifname);

	FD_ZERO(&rfds);
	FD_SET(wpa_s->ctrl_sock, &rfds);
	select(wpa_s->ctrl_sock + 1, &rfds, NULL, NULL, NULL);
}


static int wpa_supplicant_global_iface_add(struct wpa_global *global,
					   char *cmd)
{
	struct wpa_interface iface;
	char *pos;

	/*
	 * <ifname>TAB<confname>TAB<driver>TAB<ctrl_interface>TAB<driver_param>
	 */
	wpa_printf(MSG_DEBUG, "CTRL_IFACE GLOBAL INTERFACE_ADD '%s'", cmd);

	memset(&iface, 0, sizeof(iface));

	do {
		iface.ifname = pos = cmd;
		pos = strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.ifname[0] == '\0')
			return -1;
		if (pos == NULL)
			break;

		iface.confname = pos;
		pos = strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.confname[0] == '\0')
			iface.confname = NULL;
		if (pos == NULL)
			break;

		iface.driver = pos;
		pos = strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.driver[0] == '\0')
			iface.driver = NULL;
		if (pos == NULL)
			break;

		iface.ctrl_interface = pos;
		pos = strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.ctrl_interface[0] == '\0')
			iface.ctrl_interface = NULL;
		if (pos == NULL)
			break;

		iface.driver_param = pos;
		pos = strchr(pos, '\t');
		if (pos)
			*pos++ = '\0';
		if (iface.driver_param[0] == '\0')
			iface.driver_param = NULL;
		if (pos == NULL)
			break;
	} while (0);

	if (wpa_supplicant_get_iface(global, iface.ifname))
		return -1;

	return wpa_supplicant_add_iface(global, &iface) ? 0 : -1;
}


static int wpa_supplicant_global_iface_remove(struct wpa_global *global,
					      char *cmd)
{
	struct wpa_supplicant *wpa_s;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GLOBAL INTERFACE_REMOVE '%s'", cmd);

	wpa_s = wpa_supplicant_get_iface(global, cmd);
	if (wpa_s == NULL)
		return -1;
	return wpa_supplicant_remove_iface(global, wpa_s);
}


static void wpa_supplicant_global_ctrl_iface_receive(int sock, void *eloop_ctx,
						     void *sock_ctx)
{
	struct wpa_global *global = eloop_ctx;
	char buf[256];
	int res;
	CTRL_IFACE_SOCK from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 2048;
	int reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	wpa_hexdump_ascii(MSG_DEBUG, "RX global ctrl_iface", (u8 *) buf, res);

	reply = malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (strcmp(buf, "PING") == 0) {
		memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (strncmp(buf, "INTERFACE_ADD ", 14) == 0) {
		if (wpa_supplicant_global_iface_add(global, buf + 14))
			reply_len = -1;
	} else if (strncmp(buf, "INTERFACE_REMOVE ", 17) == 0) {
		if (wpa_supplicant_global_iface_remove(global, buf + 17))
			reply_len = -1;
	} else {
		memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}
	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	free(reply);
}


/**
 * wpa_supplicant_global_ctrl_iface_init - Initialize global control interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * Returns: 0 on success, -1 on failure
 *
 * Initialize the global control interface and start receiving commands from
 * external programs.
 */
int wpa_supplicant_global_ctrl_iface_init(struct wpa_global *global)
{
	CTRL_IFACE_SOCK addr;
	int s = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
	char *fname = NULL;
#endif /* CONFIG_CTRL_IFACE_UDP */

	global->ctrl_sock = -1;

	if (global->params.ctrl_interface == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "Global control interface '%s'",
		   global->params.ctrl_interface);

#ifdef CONFIG_CTRL_IFACE_UDP
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_INET)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((127 << 24) | 1);
	addr.sin_port = htons(WPA_GLOBAL_CTRL_IFACE_PORT);
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(AF_INET)");
		goto fail;
	}
#else /* CONFIG_CTRL_IFACE_UDP */
	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, global->params.ctrl_interface,
		sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind(PF_UNIX)");
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(global->params.ctrl_interface) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   global->params.ctrl_interface);
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'",
				   global->params.ctrl_interface);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore",
				   global->params.ctrl_interface);
			goto fail;
		}
	}
#endif /* CONFIG_CTRL_IFACE_UDP */

	global->ctrl_sock = s;
	eloop_register_read_sock(s, wpa_supplicant_global_ctrl_iface_receive,
				 global, NULL);

	return 0;

fail:
	if (s >= 0)
		close(s);
#ifndef CONFIG_CTRL_IFACE_UDP
	if (fname) {
		unlink(fname);
		free(fname);
	}
#endif /* CONFIG_CTRL_IFACE_UDP */
	return -1;
}


/**
 * wpa_supplicant_global_ctrl_iface_deinit - Deinitialize global ctrl interface
 * @global: Pointer to global data from wpa_supplicant_init()
 *
 * Deinitialize the global control interface that was initialized with
 * wpa_supplicant_global_ctrl_iface_init().
 */
void wpa_supplicant_global_ctrl_iface_deinit(struct wpa_global *global)
{
	if (global->ctrl_sock < 0)
		return;

	eloop_unregister_read_sock(global->ctrl_sock);
	close(global->ctrl_sock);
	global->ctrl_sock = -1;

#ifndef CONFIG_CTRL_IFACE_UDP
	if (global->params.ctrl_interface)
		unlink(global->params.ctrl_interface);
#endif /* CONFIG_CTRL_IFACE_UDP */
}
