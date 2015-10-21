/*
 * hostapd - command line interface for hostapd daemon
 * Copyright (c) 2004-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <dirent.h>

#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/edit.h"
#include "common/version.h"


static const char *const hostapd_cli_version =
"hostapd_cli v" VERSION_STR "\n"
"Copyright (c) 2004-2015, Jouni Malinen <j@w1.fi> and contributors";


static const char *const hostapd_cli_license =
"This software may be distributed under the terms of the BSD license.\n"
"See README for more details.\n";

static const char *const hostapd_cli_full_license =
"This software may be distributed under the terms of the BSD license.\n"
"\n"
"Redistribution and use in source and binary forms, with or without\n"
"modification, are permitted provided that the following conditions are\n"
"met:\n"
"\n"
"1. Redistributions of source code must retain the above copyright\n"
"   notice, this list of conditions and the following disclaimer.\n"
"\n"
"2. Redistributions in binary form must reproduce the above copyright\n"
"   notice, this list of conditions and the following disclaimer in the\n"
"   documentation and/or other materials provided with the distribution.\n"
"\n"
"3. Neither the name(s) of the above-listed copyright holder(s) nor the\n"
"   names of its contributors may be used to endorse or promote products\n"
"   derived from this software without specific prior written permission.\n"
"\n"
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
"\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
"LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
"A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
"OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
"LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
"DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
"THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n";

static const char *const commands_help =
"Commands:\n"
"   mib                  get MIB variables (dot1x, dot11, radius)\n"
"   sta <addr>           get MIB variables for one station\n"
"   all_sta              get MIB variables for all stations\n"
"   new_sta <addr>       add a new station\n"
"   deauthenticate <addr>  deauthenticate a station\n"
"   disassociate <addr>  disassociate a station\n"
#ifdef CONFIG_IEEE80211W
"   sa_query <addr>      send SA Query to a station\n"
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WPS
"   wps_pin <uuid> <pin> [timeout] [addr]  add WPS Enrollee PIN\n"
"   wps_check_pin <PIN>  verify PIN checksum\n"
"   wps_pbc              indicate button pushed to initiate PBC\n"
"   wps_cancel           cancel the pending WPS operation\n"
#ifdef CONFIG_WPS_NFC
"   wps_nfc_tag_read <hexdump>  report read NFC tag with WPS data\n"
"   wps_nfc_config_token <WPS/NDEF>  build NFC configuration token\n"
"   wps_nfc_token <WPS/NDEF/enable/disable>  manager NFC password token\n"
#endif /* CONFIG_WPS_NFC */
"   wps_ap_pin <cmd> [params..]  enable/disable AP PIN\n"
"   wps_config <SSID> <auth> <encr> <key>  configure AP\n"
"   wps_get_status       show current WPS status\n"
#endif /* CONFIG_WPS */
"   get_config           show current configuration\n"
"   help                 show this usage help\n"
"   interface [ifname]   show interfaces/select interface\n"
"   level <debug level>  change debug level\n"
"   license              show full hostapd_cli license\n"
"   quit                 exit hostapd_cli\n";

static struct wpa_ctrl *ctrl_conn;
static int hostapd_cli_quit = 0;
static int hostapd_cli_attached = 0;

#ifndef CONFIG_CTRL_IFACE_DIR
#define CONFIG_CTRL_IFACE_DIR "/var/run/hostapd"
#endif /* CONFIG_CTRL_IFACE_DIR */
static const char *ctrl_iface_dir = CONFIG_CTRL_IFACE_DIR;
static const char *client_socket_dir = NULL;

static char *ctrl_ifname = NULL;
static const char *pid_file = NULL;
static const char *action_file = NULL;
static int ping_interval = 5;
static int interactive = 0;


static void usage(void)
{
	fprintf(stderr, "%s\n", hostapd_cli_version);
	fprintf(stderr,
		"\n"
		"usage: hostapd_cli [-p<path>] [-i<ifname>] [-hvB] "
		"[-a<path>] \\\n"
		"                   [-P<pid file>] [-G<ping interval>] [command..]\n"
		"\n"
		"Options:\n"
		"   -h           help (show this usage text)\n"
		"   -v           shown version information\n"
		"   -p<path>     path to find control sockets (default: "
		"/var/run/hostapd)\n"
		"   -s<dir_path> dir path to open client sockets (default: "
		CONFIG_CTRL_IFACE_DIR ")\n"
		"   -a<file>     run in daemon mode executing the action file "
		"based on events\n"
		"                from hostapd\n"
		"   -B           run a daemon in the background\n"
		"   -i<ifname>   Interface to listen on (default: first "
		"interface found in the\n"
		"                socket path)\n\n"
		"%s",
		commands_help);
}


static struct wpa_ctrl * hostapd_cli_open_connection(const char *ifname)
{
	char *cfile;
	int flen;

	if (ifname == NULL)
		return NULL;

	flen = strlen(ctrl_iface_dir) + strlen(ifname) + 2;
	cfile = malloc(flen);
	if (cfile == NULL)
		return NULL;
	snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ifname);

	if (client_socket_dir && client_socket_dir[0] &&
	    access(client_socket_dir, F_OK) < 0) {
		perror(client_socket_dir);
		free(cfile);
		return NULL;
	}

	ctrl_conn = wpa_ctrl_open2(cfile, client_socket_dir);
	free(cfile);
	return ctrl_conn;
}


static void hostapd_cli_close_connection(void)
{
	if (ctrl_conn == NULL)
		return;

	if (hostapd_cli_attached) {
		wpa_ctrl_detach(ctrl_conn);
		hostapd_cli_attached = 0;
	}
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
}


static void hostapd_cli_msg_cb(char *msg, size_t len)
{
	printf("%s\n", msg);
}


static int _wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd, int print)
{
	char buf[4096];
	size_t len;
	int ret;

	if (ctrl_conn == NULL) {
		printf("Not connected to hostapd - command dropped.\n");
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len,
			       hostapd_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		return -1;
	}
	if (print) {
		buf[len] = '\0';
		printf("%s", buf);
	}
	return 0;
}


static inline int wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd)
{
	return _wpa_ctrl_command(ctrl, cmd, 1);
}


static int hostapd_cli_cmd_ping(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PING");
}


static int hostapd_cli_cmd_relog(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOG");
}


static int hostapd_cli_cmd_status(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc > 0 && os_strcmp(argv[0], "driver") == 0)
		return wpa_ctrl_command(ctrl, "STATUS-DRIVER");
	return wpa_ctrl_command(ctrl, "STATUS");
}


static int hostapd_cli_cmd_mib(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc > 0) {
		char buf[100];
		os_snprintf(buf, sizeof(buf), "MIB %s", argv[0]);
		return wpa_ctrl_command(ctrl, buf);
	}
	return wpa_ctrl_command(ctrl, "MIB");
}


static int hostapd_cli_exec(const char *program, const char *arg1,
			    const char *arg2)
{
	char *arg;
	size_t len;
	int res;

	len = os_strlen(arg1) + os_strlen(arg2) + 2;
	arg = os_malloc(len);
	if (arg == NULL)
		return -1;
	os_snprintf(arg, len, "%s %s", arg1, arg2);
	res = os_exec(program, arg, 1);
	os_free(arg);

	return res;
}


static void hostapd_cli_action_process(char *msg, size_t len)
{
	const char *pos;

	pos = msg;
	if (*pos == '<') {
		pos = os_strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}

	hostapd_cli_exec(action_file, ctrl_ifname, pos);
}


static int hostapd_cli_cmd_sta(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'sta' command - at least one argument, STA "
		       "address, is required.\n");
		return -1;
	}
	if (argc > 1)
		snprintf(buf, sizeof(buf), "STA %s %s", argv[0], argv[1]);
	else
		snprintf(buf, sizeof(buf), "STA %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_new_sta(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char buf[64];
	if (argc != 1) {
		printf("Invalid 'new_sta' command - exactly one argument, STA "
		       "address, is required.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), "NEW_STA %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_deauthenticate(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'deauthenticate' command - exactly one "
		       "argument, STA address, is required.\n");
		return -1;
	}
	if (argc > 1)
		os_snprintf(buf, sizeof(buf), "DEAUTHENTICATE %s %s",
			    argv[0], argv[1]);
	else
		os_snprintf(buf, sizeof(buf), "DEAUTHENTICATE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_disassociate(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'disassociate' command - exactly one "
		       "argument, STA address, is required.\n");
		return -1;
	}
	if (argc > 1)
		os_snprintf(buf, sizeof(buf), "DISASSOCIATE %s %s",
			    argv[0], argv[1]);
	else
		os_snprintf(buf, sizeof(buf), "DISASSOCIATE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


#ifdef CONFIG_IEEE80211W
static int hostapd_cli_cmd_sa_query(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char buf[64];
	if (argc != 1) {
		printf("Invalid 'sa_query' command - exactly one argument, "
		       "STA address, is required.\n");
		return -1;
	}
	snprintf(buf, sizeof(buf), "SA_QUERY %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}
#endif /* CONFIG_IEEE80211W */


#ifdef CONFIG_WPS
static int hostapd_cli_cmd_wps_pin(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char buf[256];
	if (argc < 2) {
		printf("Invalid 'wps_pin' command - at least two arguments, "
		       "UUID and PIN, are required.\n");
		return -1;
	}
	if (argc > 3)
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s %s %s",
			 argv[0], argv[1], argv[2], argv[3]);
	else if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s %s",
			 argv[0], argv[1], argv[2]);
	else
		snprintf(buf, sizeof(buf), "WPS_PIN %s %s", argv[0], argv[1]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_wps_check_pin(struct wpa_ctrl *ctrl, int argc,
					 char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1 && argc != 2) {
		printf("Invalid WPS_CHECK_PIN command: needs one argument:\n"
		       "- PIN to be verified\n");
		return -1;
	}

	if (argc == 2)
		res = os_snprintf(cmd, sizeof(cmd), "WPS_CHECK_PIN %s %s",
				  argv[0], argv[1]);
	else
		res = os_snprintf(cmd, sizeof(cmd), "WPS_CHECK_PIN %s",
				  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_CHECK_PIN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_wps_pbc(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_PBC");
}


static int hostapd_cli_cmd_wps_cancel(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_CANCEL");
}


#ifdef CONFIG_WPS_NFC
static int hostapd_cli_cmd_wps_nfc_tag_read(struct wpa_ctrl *ctrl, int argc,
					    char *argv[])
{
	int ret;
	char *buf;
	size_t buflen;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_tag_read' command - one argument "
		       "is required.\n");
		return -1;
	}

	buflen = 18 + os_strlen(argv[0]);
	buf = os_malloc(buflen);
	if (buf == NULL)
		return -1;
	os_snprintf(buf, buflen, "WPS_NFC_TAG_READ %s", argv[0]);

	ret = wpa_ctrl_command(ctrl, buf);
	os_free(buf);

	return ret;
}


static int hostapd_cli_cmd_wps_nfc_config_token(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_config_token' command - one argument "
		       "is required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "WPS_NFC_CONFIG_TOKEN %s",
			  argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_NFC_CONFIG_TOKEN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_wps_nfc_token(struct wpa_ctrl *ctrl,
					 int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 1) {
		printf("Invalid 'wps_nfc_token' command - one argument is "
		       "required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "WPS_NFC_TOKEN %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long WPS_NFC_TOKEN command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_nfc_get_handover_sel(struct wpa_ctrl *ctrl,
						int argc, char *argv[])
{
	char cmd[64];
	int res;

	if (argc != 2) {
		printf("Invalid 'nfc_get_handover_sel' command - two arguments "
		       "are required.\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "NFC_GET_HANDOVER_SEL %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long NFC_GET_HANDOVER_SEL command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}

#endif /* CONFIG_WPS_NFC */


static int hostapd_cli_cmd_wps_ap_pin(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[64];
	if (argc < 1) {
		printf("Invalid 'wps_ap_pin' command - at least one argument "
		       "is required.\n");
		return -1;
	}
	if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s %s %s",
			 argv[0], argv[1], argv[2]);
	else if (argc > 1)
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s %s",
			 argv[0], argv[1]);
	else
		snprintf(buf, sizeof(buf), "WPS_AP_PIN %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_wps_get_status(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	return wpa_ctrl_command(ctrl, "WPS_GET_STATUS");
}


static int hostapd_cli_cmd_wps_config(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[256];
	char ssid_hex[2 * SSID_MAX_LEN + 1];
	char key_hex[2 * 64 + 1];
	int i;

	if (argc < 1) {
		printf("Invalid 'wps_config' command - at least two arguments "
		       "are required.\n");
		return -1;
	}

	ssid_hex[0] = '\0';
	for (i = 0; i < SSID_MAX_LEN; i++) {
		if (argv[0][i] == '\0')
			break;
		os_snprintf(&ssid_hex[i * 2], 3, "%02x", argv[0][i]);
	}

	key_hex[0] = '\0';
	if (argc > 3) {
		for (i = 0; i < 64; i++) {
			if (argv[3][i] == '\0')
				break;
			os_snprintf(&key_hex[i * 2], 3, "%02x",
				    argv[3][i]);
		}
	}

	if (argc > 3)
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s %s %s",
			 ssid_hex, argv[1], argv[2], key_hex);
	else if (argc > 2)
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s %s",
			 ssid_hex, argv[1], argv[2]);
	else
		snprintf(buf, sizeof(buf), "WPS_CONFIG %s %s",
			 ssid_hex, argv[1]);
	return wpa_ctrl_command(ctrl, buf);
}
#endif /* CONFIG_WPS */


static int hostapd_cli_cmd_disassoc_imminent(struct wpa_ctrl *ctrl, int argc,
					     char *argv[])
{
	char buf[300];
	int res;

	if (argc < 2) {
		printf("Invalid 'disassoc_imminent' command - two arguments "
		       "(STA addr and Disassociation Timer) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "DISASSOC_IMMINENT %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_ess_disassoc(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char buf[300];
	int res;

	if (argc < 3) {
		printf("Invalid 'ess_disassoc' command - three arguments (STA "
		       "addr, disassoc timer, and URL) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "ESS_DISASSOC %s %s %s",
			  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_bss_tm_req(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char buf[2000], *tmp;
	int res, i, total;

	if (argc < 1) {
		printf("Invalid 'bss_tm_req' command - at least one argument (STA addr) is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "BSS_TM_REQ %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;

	total = res;
	for (i = 1; i < argc; i++) {
		tmp = &buf[total];
		res = os_snprintf(tmp, sizeof(buf) - total, " %s", argv[i]);
		if (os_snprintf_error(sizeof(buf) - total, res))
			return -1;
		total += res;
	}
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_get_config(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "GET_CONFIG");
}


static int wpa_ctrl_command_sta(struct wpa_ctrl *ctrl, char *cmd,
				char *addr, size_t addr_len)
{
	char buf[4096], *pos;
	size_t len;
	int ret;

	if (ctrl_conn == NULL) {
		printf("Not connected to hostapd - command dropped.\n");
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len,
			       hostapd_cli_msg_cb);
	if (ret == -2) {
		printf("'%s' command timed out.\n", cmd);
		return -2;
	} else if (ret < 0) {
		printf("'%s' command failed.\n", cmd);
		return -1;
	}

	buf[len] = '\0';
	if (memcmp(buf, "FAIL", 4) == 0)
		return -1;
	printf("%s", buf);

	pos = buf;
	while (*pos != '\0' && *pos != '\n')
		pos++;
	*pos = '\0';
	os_strlcpy(addr, buf, addr_len);
	return 0;
}


static int hostapd_cli_cmd_all_sta(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char addr[32], cmd[64];

	if (wpa_ctrl_command_sta(ctrl, "STA-FIRST", addr, sizeof(addr)))
		return 0;
	do {
		snprintf(cmd, sizeof(cmd), "STA-NEXT %s", addr);
	} while (wpa_ctrl_command_sta(ctrl, cmd, addr, sizeof(addr)) == 0);

	return -1;
}


static int hostapd_cli_cmd_help(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	printf("%s", commands_help);
	return 0;
}


static int hostapd_cli_cmd_license(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	printf("%s\n\n%s\n", hostapd_cli_version, hostapd_cli_full_license);
	return 0;
}


static int hostapd_cli_cmd_set_qos_map_set(struct wpa_ctrl *ctrl,
					   int argc, char *argv[])
{
	char buf[200];
	int res;

	if (argc != 1) {
		printf("Invalid 'set_qos_map_set' command - "
		       "one argument (comma delimited QoS map set) "
		       "is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SET_QOS_MAP_SET %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_send_qos_map_conf(struct wpa_ctrl *ctrl,
					     int argc, char *argv[])
{
	char buf[50];
	int res;

	if (argc != 1) {
		printf("Invalid 'send_qos_map_conf' command - "
		       "one argument (STA addr) is needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "SEND_QOS_MAP_CONF %s", argv[0]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_hs20_wnm_notif(struct wpa_ctrl *ctrl, int argc,
					  char *argv[])
{
	char buf[300];
	int res;

	if (argc < 2) {
		printf("Invalid 'hs20_wnm_notif' command - two arguments (STA "
		       "addr and URL) are needed\n");
		return -1;
	}

	res = os_snprintf(buf, sizeof(buf), "HS20_WNM_NOTIF %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_hs20_deauth_req(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	char buf[300];
	int res;

	if (argc < 3) {
		printf("Invalid 'hs20_deauth_req' command - at least three arguments (STA addr, Code, Re-auth Delay) are needed\n");
		return -1;
	}

	if (argc > 3)
		res = os_snprintf(buf, sizeof(buf),
				  "HS20_DEAUTH_REQ %s %s %s %s",
				  argv[0], argv[1], argv[2], argv[3]);
	else
		res = os_snprintf(buf, sizeof(buf),
				  "HS20_DEAUTH_REQ %s %s %s",
				  argv[0], argv[1], argv[2]);
	if (os_snprintf_error(sizeof(buf), res))
		return -1;
	return wpa_ctrl_command(ctrl, buf);
}


static int hostapd_cli_cmd_quit(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	hostapd_cli_quit = 1;
	if (interactive)
		eloop_terminate();
	return 0;
}


static int hostapd_cli_cmd_level(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	if (argc != 1) {
		printf("Invalid LEVEL command: needs one argument (debug "
		       "level)\n");
		return 0;
	}
	snprintf(cmd, sizeof(cmd), "LEVEL %s", argv[0]);
	return wpa_ctrl_command(ctrl, cmd);
}


static void hostapd_cli_list_interfaces(struct wpa_ctrl *ctrl)
{
	struct dirent *dent;
	DIR *dir;

	dir = opendir(ctrl_iface_dir);
	if (dir == NULL) {
		printf("Control interface directory '%s' could not be "
		       "openned.\n", ctrl_iface_dir);
		return;
	}

	printf("Available interfaces:\n");
	while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		printf("%s\n", dent->d_name);
	}
	closedir(dir);
}


static int hostapd_cli_cmd_interface(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	if (argc < 1) {
		hostapd_cli_list_interfaces(ctrl);
		return 0;
	}

	hostapd_cli_close_connection();
	os_free(ctrl_ifname);
	ctrl_ifname = os_strdup(argv[0]);
	if (ctrl_ifname == NULL)
		return -1;

	if (hostapd_cli_open_connection(ctrl_ifname)) {
		printf("Connected to interface '%s.\n", ctrl_ifname);
		if (wpa_ctrl_attach(ctrl_conn) == 0) {
			hostapd_cli_attached = 1;
		} else {
			printf("Warning: Failed to attach to "
			       "hostapd.\n");
		}
	} else {
		printf("Could not connect to interface '%s' - re-trying\n",
			ctrl_ifname);
	}
	return 0;
}


static int hostapd_cli_cmd_set(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 2) {
		printf("Invalid SET command: needs two arguments (variable "
		       "name and value)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET %s %s", argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_get(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid GET command: needs one argument (variable "
		       "name)\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "GET %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long GET command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


#ifdef CONFIG_FST
static int hostapd_cli_cmd_fst(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;
	int i;
	int total;

	if (argc <= 0) {
		printf("FST command: parameters are required.\n");
		return -1;
	}

	total = os_snprintf(cmd, sizeof(cmd), "FST-MANAGER");

	for (i = 0; i < argc; i++) {
		res = os_snprintf(cmd + total, sizeof(cmd) - total, " %s",
				  argv[i]);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Too long fst command.\n");
			return -1;
		}
		total += res;
	}
	return wpa_ctrl_command(ctrl, cmd);
}
#endif /* CONFIG_FST */


static int hostapd_cli_cmd_chan_switch(struct wpa_ctrl *ctrl,
				       int argc, char *argv[])
{
	char cmd[256];
	int res;
	int i;
	char *tmp;
	int total;

	if (argc < 2) {
		printf("Invalid chan_switch command: needs at least two "
		       "arguments (count and freq)\n"
		       "usage: <cs_count> <freq> [sec_channel_offset=] "
		       "[center_freq1=] [center_freq2=] [bandwidth=] "
		       "[blocktx] [ht|vht]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "CHAN_SWITCH %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long CHAN_SWITCH command.\n");
		return -1;
	}

	total = res;
	for (i = 2; i < argc; i++) {
		tmp = cmd + total;
		res = os_snprintf(tmp, sizeof(cmd) - total, " %s", argv[i]);
		if (os_snprintf_error(sizeof(cmd) - total, res)) {
			printf("Too long CHAN_SWITCH command.\n");
			return -1;
		}
		total += res;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_enable(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "ENABLE");
}


static int hostapd_cli_cmd_reload(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "RELOAD");
}


static int hostapd_cli_cmd_disable(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	return wpa_ctrl_command(ctrl, "DISABLE");
}


static int hostapd_cli_cmd_vendor(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc < 2 || argc > 3) {
		printf("Invalid vendor command\n"
		       "usage: <vendor id> <command id> [<hex formatted command argument>]\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "VENDOR %s %s %s", argv[0], argv[1],
			  argc == 3 ? argv[2] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long VENDOR command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_erp_flush(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return wpa_ctrl_command(ctrl, "ERP_FLUSH");
}


static int hostapd_cli_cmd_log_level(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	char cmd[256];
	int res;

	res = os_snprintf(cmd, sizeof(cmd), "LOG_LEVEL%s%s%s%s",
			  argc >= 1 ? " " : "",
			  argc >= 1 ? argv[0] : "",
			  argc == 2 ? " " : "",
			  argc == 2 ? argv[1] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long option\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


struct hostapd_cli_cmd {
	const char *cmd;
	int (*handler)(struct wpa_ctrl *ctrl, int argc, char *argv[]);
};

static const struct hostapd_cli_cmd hostapd_cli_commands[] = {
	{ "ping", hostapd_cli_cmd_ping },
	{ "mib", hostapd_cli_cmd_mib },
	{ "relog", hostapd_cli_cmd_relog },
	{ "status", hostapd_cli_cmd_status },
	{ "sta", hostapd_cli_cmd_sta },
	{ "all_sta", hostapd_cli_cmd_all_sta },
	{ "new_sta", hostapd_cli_cmd_new_sta },
	{ "deauthenticate", hostapd_cli_cmd_deauthenticate },
	{ "disassociate", hostapd_cli_cmd_disassociate },
#ifdef CONFIG_IEEE80211W
	{ "sa_query", hostapd_cli_cmd_sa_query },
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WPS
	{ "wps_pin", hostapd_cli_cmd_wps_pin },
	{ "wps_check_pin", hostapd_cli_cmd_wps_check_pin },
	{ "wps_pbc", hostapd_cli_cmd_wps_pbc },
	{ "wps_cancel", hostapd_cli_cmd_wps_cancel },
#ifdef CONFIG_WPS_NFC
	{ "wps_nfc_tag_read", hostapd_cli_cmd_wps_nfc_tag_read },
	{ "wps_nfc_config_token", hostapd_cli_cmd_wps_nfc_config_token },
	{ "wps_nfc_token", hostapd_cli_cmd_wps_nfc_token },
	{ "nfc_get_handover_sel", hostapd_cli_cmd_nfc_get_handover_sel },
#endif /* CONFIG_WPS_NFC */
	{ "wps_ap_pin", hostapd_cli_cmd_wps_ap_pin },
	{ "wps_config", hostapd_cli_cmd_wps_config },
	{ "wps_get_status", hostapd_cli_cmd_wps_get_status },
#endif /* CONFIG_WPS */
	{ "disassoc_imminent", hostapd_cli_cmd_disassoc_imminent },
	{ "ess_disassoc", hostapd_cli_cmd_ess_disassoc },
	{ "bss_tm_req", hostapd_cli_cmd_bss_tm_req },
	{ "get_config", hostapd_cli_cmd_get_config },
	{ "help", hostapd_cli_cmd_help },
	{ "interface", hostapd_cli_cmd_interface },
#ifdef CONFIG_FST
	{ "fst", hostapd_cli_cmd_fst },
#endif /* CONFIG_FST */
	{ "level", hostapd_cli_cmd_level },
	{ "license", hostapd_cli_cmd_license },
	{ "quit", hostapd_cli_cmd_quit },
	{ "set", hostapd_cli_cmd_set },
	{ "get", hostapd_cli_cmd_get },
	{ "set_qos_map_set", hostapd_cli_cmd_set_qos_map_set },
	{ "send_qos_map_conf", hostapd_cli_cmd_send_qos_map_conf },
	{ "chan_switch", hostapd_cli_cmd_chan_switch },
	{ "hs20_wnm_notif", hostapd_cli_cmd_hs20_wnm_notif },
	{ "hs20_deauth_req", hostapd_cli_cmd_hs20_deauth_req },
	{ "vendor", hostapd_cli_cmd_vendor },
	{ "enable", hostapd_cli_cmd_enable },
	{ "reload", hostapd_cli_cmd_reload },
	{ "disable", hostapd_cli_cmd_disable },
	{ "erp_flush", hostapd_cli_cmd_erp_flush },
	{ "log_level", hostapd_cli_cmd_log_level },
	{ NULL, NULL }
};


static void wpa_request(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	const struct hostapd_cli_cmd *cmd, *match = NULL;
	int count;

	count = 0;
	cmd = hostapd_cli_commands;
	while (cmd->cmd) {
		if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
			match = cmd;
			if (os_strcasecmp(cmd->cmd, argv[0]) == 0) {
				/* we have an exact match */
				count = 1;
				break;
			}
			count++;
		}
		cmd++;
	}

	if (count > 1) {
		printf("Ambiguous command '%s'; possible commands:", argv[0]);
		cmd = hostapd_cli_commands;
		while (cmd->cmd) {
			if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) ==
			    0) {
				printf(" %s", cmd->cmd);
			}
			cmd++;
		}
		printf("\n");
	} else if (count == 0) {
		printf("Unknown command '%s'\n", argv[0]);
	} else {
		match->handler(ctrl, argc - 1, &argv[1]);
	}
}


static void hostapd_cli_recv_pending(struct wpa_ctrl *ctrl, int in_read,
				     int action_monitor)
{
	int first = 1;
	if (ctrl_conn == NULL)
		return;
	while (wpa_ctrl_pending(ctrl)) {
		char buf[256];
		size_t len = sizeof(buf) - 1;
		if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
			buf[len] = '\0';
			if (action_monitor)
				hostapd_cli_action_process(buf, len);
			else {
				if (in_read && first)
					printf("\n");
				first = 0;
				printf("%s\n", buf);
			}
		} else {
			printf("Could not read pending message.\n");
			break;
		}
	}
}


#define max_args 10

static int tokenize_cmd(char *cmd, char *argv[])
{
	char *pos;
	int argc = 0;

	pos = cmd;
	for (;;) {
		while (*pos == ' ')
			pos++;
		if (*pos == '\0')
			break;
		argv[argc] = pos;
		argc++;
		if (argc == max_args)
			break;
		if (*pos == '"') {
			char *pos2 = os_strrchr(pos, '"');
			if (pos2)
				pos = pos2 + 1;
		}
		while (*pos != '\0' && *pos != ' ')
			pos++;
		if (*pos == ' ')
			*pos++ = '\0';
	}

	return argc;
}


static void hostapd_cli_ping(void *eloop_ctx, void *timeout_ctx)
{
	if (ctrl_conn && _wpa_ctrl_command(ctrl_conn, "PING", 0)) {
		printf("Connection to hostapd lost - trying to reconnect\n");
		hostapd_cli_close_connection();
	}
	if (!ctrl_conn) {
		ctrl_conn = hostapd_cli_open_connection(ctrl_ifname);
		if (ctrl_conn) {
			printf("Connection to hostapd re-established\n");
			if (wpa_ctrl_attach(ctrl_conn) == 0) {
				hostapd_cli_attached = 1;
			} else {
				printf("Warning: Failed to attach to "
				       "hostapd.\n");
			}
		}
	}
	if (ctrl_conn)
		hostapd_cli_recv_pending(ctrl_conn, 1, 0);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_ping, NULL, NULL);
}


static void hostapd_cli_eloop_terminate(int sig, void *signal_ctx)
{
	eloop_terminate();
}


static void hostapd_cli_edit_cmd_cb(void *ctx, char *cmd)
{
	char *argv[max_args];
	int argc;
	argc = tokenize_cmd(cmd, argv);
	if (argc)
		wpa_request(ctrl_conn, argc, argv);
}


static void hostapd_cli_edit_eof_cb(void *ctx)
{
	eloop_terminate();
}


static void hostapd_cli_interactive(void)
{
	printf("\nInteractive mode\n\n");

	eloop_register_signal_terminate(hostapd_cli_eloop_terminate, NULL);
	edit_init(hostapd_cli_edit_cmd_cb, hostapd_cli_edit_eof_cb,
		  NULL, NULL, NULL, NULL);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_ping, NULL, NULL);

	eloop_run();

	edit_deinit(NULL, NULL);
	eloop_cancel_timeout(hostapd_cli_ping, NULL, NULL);
}


static void hostapd_cli_cleanup(void)
{
	hostapd_cli_close_connection();
	if (pid_file)
		os_daemonize_terminate(pid_file);

	os_program_deinit();
}


static void hostapd_cli_action(struct wpa_ctrl *ctrl)
{
	fd_set rfds;
	int fd, res;
	struct timeval tv;
	char buf[256];
	size_t len;

	fd = wpa_ctrl_get_fd(ctrl);

	while (!hostapd_cli_quit) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = ping_interval;
		tv.tv_usec = 0;
		res = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (res < 0 && errno != EINTR) {
			perror("select");
			break;
		}

		if (FD_ISSET(fd, &rfds))
			hostapd_cli_recv_pending(ctrl, 0, 1);
		else {
			len = sizeof(buf) - 1;
			if (wpa_ctrl_request(ctrl, "PING", 4, buf, &len,
					     hostapd_cli_action_process) < 0 ||
			    len < 4 || os_memcmp(buf, "PONG", 4) != 0) {
				printf("hostapd did not reply to PING "
				       "command - exiting\n");
				break;
			}
		}
	}
}


int main(int argc, char *argv[])
{
	int warning_displayed = 0;
	int c;
	int daemonize = 0;

	if (os_program_init())
		return -1;

	for (;;) {
		c = getopt(argc, argv, "a:BhG:i:p:P:s:v");
		if (c < 0)
			break;
		switch (c) {
		case 'a':
			action_file = optarg;
			break;
		case 'B':
			daemonize = 1;
			break;
		case 'G':
			ping_interval = atoi(optarg);
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			printf("%s\n", hostapd_cli_version);
			return 0;
		case 'i':
			os_free(ctrl_ifname);
			ctrl_ifname = os_strdup(optarg);
			break;
		case 'p':
			ctrl_iface_dir = optarg;
			break;
		case 'P':
			pid_file = optarg;
			break;
		case 's':
			client_socket_dir = optarg;
			break;
		default:
			usage();
			return -1;
		}
	}

	interactive = (argc == optind) && (action_file == NULL);

	if (interactive) {
		printf("%s\n\n%s\n\n", hostapd_cli_version,
		       hostapd_cli_license);
	}

	if (eloop_init())
		return -1;

	for (;;) {
		if (ctrl_ifname == NULL) {
			struct dirent *dent;
			DIR *dir = opendir(ctrl_iface_dir);
			if (dir) {
				while ((dent = readdir(dir))) {
					if (os_strcmp(dent->d_name, ".") == 0
					    ||
					    os_strcmp(dent->d_name, "..") == 0)
						continue;
					printf("Selected interface '%s'\n",
					       dent->d_name);
					ctrl_ifname = os_strdup(dent->d_name);
					break;
				}
				closedir(dir);
			}
		}
		ctrl_conn = hostapd_cli_open_connection(ctrl_ifname);
		if (ctrl_conn) {
			if (warning_displayed)
				printf("Connection established.\n");
			break;
		}

		if (!interactive) {
			perror("Failed to connect to hostapd - "
			       "wpa_ctrl_open");
			return -1;
		}

		if (!warning_displayed) {
			printf("Could not connect to hostapd - re-trying\n");
			warning_displayed = 1;
		}
		os_sleep(1, 0);
		continue;
	}

	if (interactive || action_file) {
		if (wpa_ctrl_attach(ctrl_conn) == 0) {
			hostapd_cli_attached = 1;
		} else {
			printf("Warning: Failed to attach to hostapd.\n");
			if (action_file)
				return -1;
		}
	}

	if (daemonize && os_daemonize(pid_file))
		return -1;

	if (interactive)
		hostapd_cli_interactive();
	else if (action_file)
		hostapd_cli_action(ctrl_conn);
	else
		wpa_request(ctrl_conn, argc - optind, &argv[optind]);

	os_free(ctrl_ifname);
	eloop_destroy();
	hostapd_cli_cleanup();
	return 0;
}
