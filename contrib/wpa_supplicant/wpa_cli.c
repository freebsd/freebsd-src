/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
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

#include "includes.h"

#ifdef CONFIG_CTRL_IFACE

#ifdef CONFIG_CTRL_IFACE_UNIX
#include <dirent.h>
#endif /* CONFIG_CTRL_IFACE_UNIX */
#ifdef CONFIG_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* CONFIG_READLINE */

#include "wpa_ctrl.h"
#include "common.h"
#include "version.h"


static const char *wpa_cli_version =
"wpa_cli v" VERSION_STR "\n"
"Copyright (c) 2004-2007, Jouni Malinen <j@w1.fi> and contributors";


static const char *wpa_cli_license =
"This program is free software. You can distribute it and/or modify it\n"
"under the terms of the GNU General Public License version 2.\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license. See README and COPYING for more details.\n";

static const char *wpa_cli_full_license =
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License version 2 as\n"
"published by the Free Software Foundation.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA\n"
"\n"
"Alternatively, this software may be distributed under the terms of the\n"
"BSD license.\n"
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

static const char *commands_help =
"commands:\n"
"  status [verbose] = get current WPA/EAPOL/EAP status\n"
"  mib = get MIB variables (dot1x, dot11)\n"
"  help = show this usage help\n"
"  interface [ifname] = show interfaces/select interface\n"
"  level <debug level> = change debug level\n"
"  license = show full wpa_cli license\n"
"  logoff = IEEE 802.1X EAPOL state machine logoff\n"
"  logon = IEEE 802.1X EAPOL state machine logon\n"
"  set = set variables (shows list of variables when run without arguments)\n"
"  pmksa = show PMKSA cache\n"
"  reassociate = force reassociation\n"
"  reconfigure = force wpa_supplicant to re-read its configuration file\n"
"  preauthenticate <BSSID> = force preauthentication\n"
"  identity <network id> <identity> = configure identity for an SSID\n"
"  password <network id> <password> = configure password for an SSID\n"
"  new_password <network id> <password> = change password for an SSID\n"
"  pin <network id> <pin> = configure pin for an SSID\n"
"  otp <network id> <password> = configure one-time-password for an SSID\n"
"  passphrase <network id> <passphrase> = configure private key passphrase\n"
"    for an SSID\n"
"  bssid <network id> <BSSID> = set preferred BSSID for an SSID\n"
"  list_networks = list configured networks\n"
"  select_network <network id> = select a network (disable others)\n"
"  enable_network <network id> = enable a network\n"
"  disable_network <network id> = disable a network\n"
"  add_network = add a network\n"
"  remove_network <network id> = remove a network\n"
"  set_network <network id> <variable> <value> = set network variables "
"(shows\n"
"    list of variables when run without arguments)\n"
"  get_network <network id> <variable> = get network variables\n"
"  save_config = save the current configuration\n"
"  disconnect = disconnect and wait for reassociate command before "
"connecting\n"
"  scan = request new BSS scan\n"
"  scan_results = get latest scan results\n"
"  get_capability <eap/pairwise/group/key_mgmt/proto/auth_alg> = "
"get capabilies\n"
"  ap_scan <value> = set ap_scan parameter\n"
"  stkstart <addr> = request STK negotiation with <addr>\n"
"  terminate = terminate wpa_supplicant\n"
"  quit = exit wpa_cli\n";

static struct wpa_ctrl *ctrl_conn;
static int wpa_cli_quit = 0;
static int wpa_cli_attached = 0;
static int wpa_cli_connected = 0;
static int wpa_cli_last_id = 0;
static const char *ctrl_iface_dir = "/var/run/wpa_supplicant";
static char *ctrl_ifname = NULL;
static const char *pid_file = NULL;
static const char *action_file = NULL;


static void usage(void)
{
	printf("wpa_cli [-p<path to ctrl sockets>] [-i<ifname>] [-hvB] "
	       "[-a<action file>] \\\n"
	       "        [-P<pid file>] [-g<global ctrl>]  [command..]\n"
	       "  -h = help (show this usage text)\n"
	       "  -v = shown version information\n"
	       "  -a = run in daemon mode executing the action file based on "
	       "events from\n"
	       "       wpa_supplicant\n"
	       "  -B = run a daemon in the background\n"
	       "  default path: /var/run/wpa_supplicant\n"
	       "  default interface: first interface found in socket path\n"
	       "%s",
	       commands_help);
}


static struct wpa_ctrl * wpa_cli_open_connection(const char *ifname)
{
#if defined(CONFIG_CTRL_IFACE_UDP) || defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
	ctrl_conn = wpa_ctrl_open(ifname);
	return ctrl_conn;
#else /* CONFIG_CTRL_IFACE_UDP || CONFIG_CTRL_IFACE_NAMED_PIPE */
	char *cfile;
	int flen;

	if (ifname == NULL)
		return NULL;

	flen = os_strlen(ctrl_iface_dir) + os_strlen(ifname) + 2;
	cfile = os_malloc(flen);
	if (cfile == NULL)
		return NULL;
	os_snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ifname);

	ctrl_conn = wpa_ctrl_open(cfile);
	os_free(cfile);
	return ctrl_conn;
#endif /* CONFIG_CTRL_IFACE_UDP || CONFIG_CTRL_IFACE_NAMED_PIPE */
}


static void wpa_cli_close_connection(void)
{
	if (ctrl_conn == NULL)
		return;

	if (wpa_cli_attached) {
		wpa_ctrl_detach(ctrl_conn);
		wpa_cli_attached = 0;
	}
	wpa_ctrl_close(ctrl_conn);
	ctrl_conn = NULL;
}


static void wpa_cli_msg_cb(char *msg, size_t len)
{
	printf("%s\n", msg);
}


static int _wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd, int print)
{
	char buf[2048];
	size_t len;
	int ret;

	if (ctrl_conn == NULL) {
		printf("Not connected to wpa_supplicant - command dropped.\n");
		return -1;
	}
	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, cmd, os_strlen(cmd), buf, &len,
			       wpa_cli_msg_cb);
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


static int wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd)
{
	return _wpa_ctrl_command(ctrl, cmd, 1);
}


static int wpa_cli_cmd_status(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	int verbose = argc > 0 && os_strcmp(argv[0], "verbose") == 0;
	return wpa_ctrl_command(ctrl, verbose ? "STATUS-VERBOSE" : "STATUS");
}


static int wpa_cli_cmd_ping(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PING");
}


static int wpa_cli_cmd_mib(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "MIB");
}


static int wpa_cli_cmd_pmksa(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PMKSA");
}


static int wpa_cli_cmd_help(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	printf("%s", commands_help);
	return 0;
}


static int wpa_cli_cmd_license(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	printf("%s\n\n%s\n", wpa_cli_version, wpa_cli_full_license);
	return 0;
}


static int wpa_cli_cmd_quit(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	wpa_cli_quit = 1;
	return 0;
}


static void wpa_cli_show_variables(void)
{
	printf("set variables:\n"
	       "  EAPOL::heldPeriod (EAPOL state machine held period, "
	       "in seconds)\n"
	       "  EAPOL::authPeriod (EAPOL state machine authentication "
	       "period, in seconds)\n"
	       "  EAPOL::startPeriod (EAPOL state machine start period, in "
	       "seconds)\n"
	       "  EAPOL::maxStart (EAPOL state machine maximum start "
	       "attempts)\n");
	printf("  dot11RSNAConfigPMKLifetime (WPA/WPA2 PMK lifetime in "
	       "seconds)\n"
	       "  dot11RSNAConfigPMKReauthThreshold (WPA/WPA2 reauthentication"
	       " threshold\n\tpercentage)\n"
	       "  dot11RSNAConfigSATimeout (WPA/WPA2 timeout for completing "
	       "security\n\tassociation in seconds)\n");
}


static int wpa_cli_cmd_set(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc == 0) {
		wpa_cli_show_variables();
		return 0;
	}

	if (argc != 2) {
		printf("Invalid SET command: needs two arguments (variable "
		       "name and value)\n");
		return 0;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET %s %s", argv[0], argv[1]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long SET command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_logoff(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "LOGOFF");
}


static int wpa_cli_cmd_logon(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "LOGON");
}


static int wpa_cli_cmd_reassociate(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "REASSOCIATE");
}


static int wpa_cli_cmd_preauthenticate(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid PREAUTH command: needs one argument "
		       "(BSSID)\n");
		return 0;
	}

	res = os_snprintf(cmd, sizeof(cmd), "PREAUTH %s", argv[0]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long PREAUTH command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_ap_scan(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid AP_SCAN command: needs one argument (ap_scan "
		       "value)\n");
		return 0;
	}
	res = os_snprintf(cmd, sizeof(cmd), "AP_SCAN %s", argv[0]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long AP_SCAN command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_stkstart(struct wpa_ctrl *ctrl, int argc,
				char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid STKSTART command: needs one argument "
		       "(Peer STA MAC address)\n");
		return 0;
	}

	res = os_snprintf(cmd, sizeof(cmd), "STKSTART %s", argv[0]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long STKSTART command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_level(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid LEVEL command: needs one argument (debug "
		       "level)\n");
		return 0;
	}
	res = os_snprintf(cmd, sizeof(cmd), "LEVEL %s", argv[0]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long LEVEL command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_identity(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid IDENTITY command: needs two arguments "
		       "(network id and identity)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "IDENTITY-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long IDENTITY command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long IDENTITY command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_password(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid PASSWORD command: needs two arguments "
		       "(network id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "PASSWORD-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long PASSWORD command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long PASSWORD command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_new_password(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid NEW_PASSWORD command: needs two arguments "
		       "(network id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "NEW_PASSWORD-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long NEW_PASSWORD command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long NEW_PASSWORD command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_pin(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid PIN command: needs two arguments "
		       "(network id and pin)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "PIN-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long PIN command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long PIN command.\n");
			return 0;
		}
		pos += ret;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_otp(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid OTP command: needs two arguments (network "
		       "id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "OTP-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long OTP command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long OTP command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_passphrase(struct wpa_ctrl *ctrl, int argc,
				  char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid PASSPHRASE command: needs two arguments "
		       "(network id and passphrase)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, WPA_CTRL_RSP "PASSPHRASE-%s:%s",
			  argv[0], argv[1]);
	if (ret < 0 || ret >= end - pos) {
		printf("Too long PASSPHRASE command.\n");
		return 0;
	}
	pos += ret;
	for (i = 2; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long PASSPHRASE command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_bssid(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i, ret;

	if (argc < 2) {
		printf("Invalid BSSID command: needs two arguments (network "
		       "id and BSSID)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	ret = os_snprintf(pos, end - pos, "BSSID");
	if (ret < 0 || ret >= end - pos) {
		printf("Too long BSSID command.\n");
		return 0;
	}
	pos += ret;
	for (i = 0; i < argc; i++) {
		ret = os_snprintf(pos, end - pos, " %s", argv[i]);
		if (ret < 0 || ret >= end - pos) {
			printf("Too long BSSID command.\n");
			return 0;
		}
		pos += ret;
	}

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_list_networks(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	return wpa_ctrl_command(ctrl, "LIST_NETWORKS");
}


static int wpa_cli_cmd_select_network(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char cmd[32];

	if (argc < 1) {
		printf("Invalid SELECT_NETWORK command: needs one argument "
		       "(network id)\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %s", argv[0]);
	cmd[sizeof(cmd) - 1] = '\0';

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_enable_network(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char cmd[32];

	if (argc < 1) {
		printf("Invalid ENABLE_NETWORK command: needs one argument "
		       "(network id)\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %s", argv[0]);
	cmd[sizeof(cmd) - 1] = '\0';

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_disable_network(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	char cmd[32];

	if (argc < 1) {
		printf("Invalid DISABLE_NETWORK command: needs one argument "
		       "(network id)\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "DISABLE_NETWORK %s", argv[0]);
	cmd[sizeof(cmd) - 1] = '\0';

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_add_network(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "ADD_NETWORK");
}


static int wpa_cli_cmd_remove_network(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char cmd[32];

	if (argc < 1) {
		printf("Invalid REMOVE_NETWORK command: needs one argument "
		       "(network id)\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %s", argv[0]);
	cmd[sizeof(cmd) - 1] = '\0';

	return wpa_ctrl_command(ctrl, cmd);
}


static void wpa_cli_show_network_variables(void)
{
	printf("set_network variables:\n"
	       "  ssid (network name, SSID)\n"
	       "  psk (WPA passphrase or pre-shared key)\n"
	       "  key_mgmt (key management protocol)\n"
	       "  identity (EAP identity)\n"
	       "  password (EAP password)\n"
	       "  ...\n"
	       "\n"
	       "Note: Values are entered in the same format as the "
	       "configuration file is using,\n"
	       "i.e., strings values need to be inside double quotation "
	       "marks.\n"
	       "For example: set_network 1 ssid \"network name\"\n"
	       "\n"
	       "Please see wpa_supplicant.conf documentation for full list "
	       "of\navailable variables.\n");
}


static int wpa_cli_cmd_set_network(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];
	int res;

	if (argc == 0) {
		wpa_cli_show_network_variables();
		return 0;
	}

	if (argc != 3) {
		printf("Invalid SET_NETWORK command: needs three arguments\n"
		       "(network id, variable name, and value)\n");
		return 0;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_NETWORK %s %s %s",
			  argv[0], argv[1], argv[2]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long SET_NETWORK command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_get_network(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];
	int res;

	if (argc == 0) {
		wpa_cli_show_network_variables();
		return 0;
	}

	if (argc != 2) {
		printf("Invalid GET_NETWORK command: needs two arguments\n"
		       "(network id and variable name)\n");
		return 0;
	}

	res = os_snprintf(cmd, sizeof(cmd), "GET_NETWORK %s %s",
			  argv[0], argv[1]);
	if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
		printf("Too long GET_NETWORK command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_disconnect(struct wpa_ctrl *ctrl, int argc,
				  char *argv[])
{
	return wpa_ctrl_command(ctrl, "DISCONNECT");
}


static int wpa_cli_cmd_save_config(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "SAVE_CONFIG");
}


static int wpa_cli_cmd_scan(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "SCAN");
}


static int wpa_cli_cmd_scan_results(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	return wpa_ctrl_command(ctrl, "SCAN_RESULTS");
}


static int wpa_cli_cmd_get_capability(struct wpa_ctrl *ctrl, int argc,
				      char *argv[])
{
	char cmd[64];

	if (argc < 1 || argc > 2) {
		printf("Invalid GET_CAPABILITY command: need either one or "
		       "two arguments\n");
		return 0;
	}

	if ((argc == 2) && os_strcmp(argv[1], "strict") != 0) {
		printf("Invalid GET_CAPABILITY command: second argument, "
		       "if any, must be 'strict'\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "GET_CAPABILITY %s%s", argv[0],
		    (argc == 2) ? " strict" : "");
	cmd[sizeof(cmd) - 1] = '\0';

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_list_interfaces(struct wpa_ctrl *ctrl)
{
	printf("Available interfaces:\n");
	return wpa_ctrl_command(ctrl, "INTERFACES");
}


static int wpa_cli_cmd_interface(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		wpa_cli_list_interfaces(ctrl);
		return 0;
	}

	wpa_cli_close_connection();
	os_free(ctrl_ifname);
	ctrl_ifname = os_strdup(argv[0]);

	if (wpa_cli_open_connection(ctrl_ifname)) {
		printf("Connected to interface '%s.\n", ctrl_ifname);
		if (wpa_ctrl_attach(ctrl_conn) == 0) {
			wpa_cli_attached = 1;
		} else {
			printf("Warning: Failed to attach to "
			       "wpa_supplicant.\n");
		}
	} else {
		printf("Could not connect to interface '%s' - re-trying\n",
			ctrl_ifname);
	}
	return 0;
}


static int wpa_cli_cmd_reconfigure(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	return wpa_ctrl_command(ctrl, "RECONFIGURE");
}


static int wpa_cli_cmd_terminate(struct wpa_ctrl *ctrl, int argc,
				 char *argv[])
{
	return wpa_ctrl_command(ctrl, "TERMINATE");
}


static int wpa_cli_cmd_interface_add(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	char cmd[256];

	if (argc < 1) {
		printf("Invalid INTERFACE_ADD command: needs at least one "
		       "argument (interface name)\n"
			"All arguments: ifname confname driver ctrl_interface "
			"driver_param bridge_name\n");
		return 0;
	}

	/*
	 * INTERFACE_ADD <ifname>TAB<confname>TAB<driver>TAB<ctrl_interface>TAB
	 * <driver_param>TAB<bridge_name>
	 */
	os_snprintf(cmd, sizeof(cmd), "INTERFACE_ADD %s\t%s\t%s\t%s\t%s\t%s",
		    argv[0],
		    argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "",
		    argc > 3 ? argv[3] : "", argc > 4 ? argv[4] : "",
		    argc > 5 ? argv[5] : "");
	cmd[sizeof(cmd) - 1] = '\0';
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_interface_remove(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char cmd[128];

	if (argc != 1) {
		printf("Invalid INTERFACE_REMOVE command: needs one argument "
		       "(interface name)\n");
		return 0;
	}

	os_snprintf(cmd, sizeof(cmd), "INTERFACE_REMOVE %s", argv[0]);
	cmd[sizeof(cmd) - 1] = '\0';
	return wpa_ctrl_command(ctrl, cmd);
}


struct wpa_cli_cmd {
	const char *cmd;
	int (*handler)(struct wpa_ctrl *ctrl, int argc, char *argv[]);
};

static struct wpa_cli_cmd wpa_cli_commands[] = {
	{ "status", wpa_cli_cmd_status },
	{ "ping", wpa_cli_cmd_ping },
	{ "mib", wpa_cli_cmd_mib },
	{ "help", wpa_cli_cmd_help },
	{ "interface", wpa_cli_cmd_interface },
	{ "level", wpa_cli_cmd_level },
	{ "license", wpa_cli_cmd_license },
	{ "quit", wpa_cli_cmd_quit },
	{ "set", wpa_cli_cmd_set },
	{ "logon", wpa_cli_cmd_logon },
	{ "logoff", wpa_cli_cmd_logoff },
	{ "pmksa", wpa_cli_cmd_pmksa },
	{ "reassociate", wpa_cli_cmd_reassociate },
	{ "preauthenticate", wpa_cli_cmd_preauthenticate },
	{ "identity", wpa_cli_cmd_identity },
	{ "password", wpa_cli_cmd_password },
	{ "new_password", wpa_cli_cmd_new_password },
	{ "pin", wpa_cli_cmd_pin },
	{ "otp", wpa_cli_cmd_otp },
	{ "passphrase", wpa_cli_cmd_passphrase },
	{ "bssid", wpa_cli_cmd_bssid },
	{ "list_networks", wpa_cli_cmd_list_networks },
	{ "select_network", wpa_cli_cmd_select_network },
	{ "enable_network", wpa_cli_cmd_enable_network },
	{ "disable_network", wpa_cli_cmd_disable_network },
	{ "add_network", wpa_cli_cmd_add_network },
	{ "remove_network", wpa_cli_cmd_remove_network },
	{ "set_network", wpa_cli_cmd_set_network },
	{ "get_network", wpa_cli_cmd_get_network },
	{ "save_config", wpa_cli_cmd_save_config },
	{ "disconnect", wpa_cli_cmd_disconnect },
	{ "scan", wpa_cli_cmd_scan },
	{ "scan_results", wpa_cli_cmd_scan_results },
	{ "get_capability", wpa_cli_cmd_get_capability },
	{ "reconfigure", wpa_cli_cmd_reconfigure },
	{ "terminate", wpa_cli_cmd_terminate },
	{ "interface_add", wpa_cli_cmd_interface_add },
	{ "interface_remove", wpa_cli_cmd_interface_remove },
	{ "ap_scan", wpa_cli_cmd_ap_scan },
	{ "stkstart", wpa_cli_cmd_stkstart },
	{ NULL, NULL }
};


static void wpa_request(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	struct wpa_cli_cmd *cmd, *match = NULL;
	int count;

	count = 0;
	cmd = wpa_cli_commands;
	while (cmd->cmd) {
		if (os_strncasecmp(cmd->cmd, argv[0], os_strlen(argv[0])) == 0)
		{
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
		cmd = wpa_cli_commands;
		while (cmd->cmd) {
			if (os_strncasecmp(cmd->cmd, argv[0],
					   os_strlen(argv[0])) == 0) {
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


static int str_match(const char *a, const char *b)
{
	return os_strncmp(a, b, os_strlen(b)) == 0;
}


static int wpa_cli_exec(const char *program, const char *arg1,
			const char *arg2)
{
	char *cmd;
	size_t len;

	len = os_strlen(program) + os_strlen(arg1) + os_strlen(arg2) + 3;
	cmd = os_malloc(len);
	if (cmd == NULL)
		return -1;
	os_snprintf(cmd, len, "%s %s %s", program, arg1, arg2);
	cmd[len - 1] = '\0';
#ifndef _WIN32_WCE
	system(cmd);
#endif /* _WIN32_WCE */
	os_free(cmd);

	return 0;
}


static void wpa_cli_action_process(const char *msg)
{
	const char *pos;
	char *copy = NULL, *id, *pos2;

	pos = msg;
	if (*pos == '<') {
		/* skip priority */
		pos = os_strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}

	if (str_match(pos, WPA_EVENT_CONNECTED)) {
		int new_id = -1;
		os_unsetenv("WPA_ID");
		os_unsetenv("WPA_ID_STR");
		os_unsetenv("WPA_CTRL_DIR");

		pos = os_strstr(pos, "[id=");
		if (pos)
			copy = os_strdup(pos + 4);

		if (copy) {
			pos2 = id = copy;
			while (*pos2 && *pos2 != ' ')
				pos2++;
			*pos2++ = '\0';
			new_id = atoi(id);
			os_setenv("WPA_ID", id, 1);
			while (*pos2 && *pos2 != '=')
				pos2++;
			if (*pos2 == '=')
				pos2++;
			id = pos2;
			while (*pos2 && *pos2 != ']')
				pos2++;
			*pos2 = '\0';
			os_setenv("WPA_ID_STR", id, 1);
			os_free(copy);
		}

		os_setenv("WPA_CTRL_DIR", ctrl_iface_dir, 1);

		if (!wpa_cli_connected || new_id != wpa_cli_last_id) {
			wpa_cli_connected = 1;
			wpa_cli_last_id = new_id;
			wpa_cli_exec(action_file, ctrl_ifname, "CONNECTED");
		}
	} else if (str_match(pos, WPA_EVENT_DISCONNECTED)) {
		if (wpa_cli_connected) {
			wpa_cli_connected = 0;
			wpa_cli_exec(action_file, ctrl_ifname, "DISCONNECTED");
		}
	} else if (str_match(pos, WPA_EVENT_TERMINATING)) {
		printf("wpa_supplicant is terminating - stop monitoring\n");
		wpa_cli_quit = 1;
	}
}


#ifndef CONFIG_ANSI_C_EXTRA
static void wpa_cli_action_cb(char *msg, size_t len)
{
	wpa_cli_action_process(msg);
}
#endif /* CONFIG_ANSI_C_EXTRA */


static void wpa_cli_reconnect(void)
{
	wpa_cli_close_connection();
	ctrl_conn = wpa_cli_open_connection(ctrl_ifname);
	if (ctrl_conn) {
		printf("Connection to wpa_supplicant re-established\n");
		if (wpa_ctrl_attach(ctrl_conn) == 0) {
			wpa_cli_attached = 1;
		} else {
			printf("Warning: Failed to attach to "
			       "wpa_supplicant.\n");
		}
	}
}


static void wpa_cli_recv_pending(struct wpa_ctrl *ctrl, int in_read,
				 int action_monitor)
{
	int first = 1;
	if (ctrl_conn == NULL) {
		wpa_cli_reconnect();
		return;
	}
	while (wpa_ctrl_pending(ctrl) > 0) {
		char buf[256];
		size_t len = sizeof(buf) - 1;
		if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
			buf[len] = '\0';
			if (action_monitor)
				wpa_cli_action_process(buf);
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

	if (wpa_ctrl_pending(ctrl) < 0) {
		printf("Connection to wpa_supplicant lost - trying to "
		       "reconnect\n");
		wpa_cli_reconnect();
	}
}


#ifdef CONFIG_READLINE
static char * wpa_cli_cmd_gen(const char *text, int state)
{
	static int i, len;
	const char *cmd;

	if (state == 0) {
		i = 0;
		len = os_strlen(text);
	}

	while ((cmd = wpa_cli_commands[i].cmd)) {
		i++;
		if (os_strncasecmp(cmd, text, len) == 0)
			return os_strdup(cmd);
	}

	return NULL;
}


static char * wpa_cli_dummy_gen(const char *text, int state)
{
	return NULL;
}


static char ** wpa_cli_completion(const char *text, int start, int end)
{
	return rl_completion_matches(text, start == 0 ?
				     wpa_cli_cmd_gen : wpa_cli_dummy_gen);
}
#endif /* CONFIG_READLINE */


static void wpa_cli_interactive(void)
{
#define max_args 10
	char cmdbuf[256], *cmd, *argv[max_args], *pos;
	int argc;
#ifdef CONFIG_READLINE
	char *home, *hfile = NULL;
#endif /* CONFIG_READLINE */

	printf("\nInteractive mode\n\n");

#ifdef CONFIG_READLINE
	rl_attempted_completion_function = wpa_cli_completion;
	home = getenv("HOME");
	if (home) {
		const char *fname = ".wpa_cli_history";
		int hfile_len = os_strlen(home) + 1 + os_strlen(fname) + 1;
		hfile = os_malloc(hfile_len);
		if (hfile) {
			os_snprintf(hfile, hfile_len, "%s/%s", home, fname);
			hfile[hfile_len - 1] = '\0';
			read_history(hfile);
			stifle_history(100);
		}
	}
#endif /* CONFIG_READLINE */

	do {
		wpa_cli_recv_pending(ctrl_conn, 0, 0);
#ifndef CONFIG_NATIVE_WINDOWS
		alarm(1);
#endif /* CONFIG_NATIVE_WINDOWS */
#ifdef CONFIG_READLINE
		cmd = readline("> ");
		if (cmd && *cmd) {
			HIST_ENTRY *h;
			while (next_history())
				;
			h = previous_history();
			if (h == NULL || os_strcmp(cmd, h->line) != 0)
				add_history(cmd);
			next_history();
		}
#else /* CONFIG_READLINE */
		printf("> ");
		cmd = fgets(cmdbuf, sizeof(cmdbuf), stdin);
#endif /* CONFIG_READLINE */
#ifndef CONFIG_NATIVE_WINDOWS
		alarm(0);
#endif /* CONFIG_NATIVE_WINDOWS */
		if (cmd == NULL)
			break;
		wpa_cli_recv_pending(ctrl_conn, 0, 0);
		pos = cmd;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		argc = 0;
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
		if (argc)
			wpa_request(ctrl_conn, argc, argv);

		if (cmd != cmdbuf)
			os_free(cmd);
	} while (!wpa_cli_quit);

#ifdef CONFIG_READLINE
	if (hfile) {
		/* Save command history, excluding lines that may contain
		 * passwords. */
		HIST_ENTRY *h;
		history_set_pos(0);
		h = next_history();
		while (h) {
			char *p = h->line;
			while (*p == ' ' || *p == '\t')
				p++;
			if (os_strncasecmp(p, "pa", 2) == 0 ||
			    os_strncasecmp(p, "o", 1) == 0 ||
			    os_strncasecmp(p, "n", 1)) {
				h = remove_history(where_history());
				if (h) {
					os_free(h->line);
					os_free(h->data);
					os_free(h);
				}
				h = current_history();
			} else {
				h = next_history();
			}
		}
		write_history(hfile);
		os_free(hfile);
	}
#endif /* CONFIG_READLINE */
}


static void wpa_cli_action(struct wpa_ctrl *ctrl)
{
#ifdef CONFIG_ANSI_C_EXTRA
	/* TODO: ANSI C version(?) */
	printf("Action processing not supported in ANSI C build.\n");
#else /* CONFIG_ANSI_C_EXTRA */
	fd_set rfds;
	int fd, res;
	struct timeval tv;
	char buf[256]; /* note: large enough to fit in unsolicited messages */
	size_t len;

	fd = wpa_ctrl_get_fd(ctrl);

	while (!wpa_cli_quit) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		res = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (res < 0 && errno != EINTR) {
			perror("select");
			break;
		}

		if (FD_ISSET(fd, &rfds))
			wpa_cli_recv_pending(ctrl, 0, 1);
		else {
			/* verify that connection is still working */
			len = sizeof(buf) - 1;
			if (wpa_ctrl_request(ctrl, "PING", 4, buf, &len,
					     wpa_cli_action_cb) < 0 ||
			    len < 4 || os_memcmp(buf, "PONG", 4) != 0) {
				printf("wpa_supplicant did not reply to PING "
				       "command - exiting\n");
				break;
			}
		}
	}
#endif /* CONFIG_ANSI_C_EXTRA */
}


static void wpa_cli_cleanup(void)
{
	wpa_cli_close_connection();
	if (pid_file)
		os_daemonize_terminate(pid_file);

	os_program_deinit();
}

static void wpa_cli_terminate(int sig)
{
	wpa_cli_cleanup();
	exit(0);
}


#ifndef CONFIG_NATIVE_WINDOWS
static void wpa_cli_alarm(int sig)
{
	if (ctrl_conn && _wpa_ctrl_command(ctrl_conn, "PING", 0)) {
		printf("Connection to wpa_supplicant lost - trying to "
		       "reconnect\n");
		wpa_cli_close_connection();
	}
	if (!ctrl_conn)
		wpa_cli_reconnect();
	if (ctrl_conn)
		wpa_cli_recv_pending(ctrl_conn, 1, 0);
	alarm(1);
}
#endif /* CONFIG_NATIVE_WINDOWS */


static char * wpa_cli_get_default_ifname(void)
{
	char *ifname = NULL;

#ifdef CONFIG_CTRL_IFACE_UNIX
	struct dirent *dent;
	DIR *dir = opendir(ctrl_iface_dir);
	if (!dir)
		return NULL;
	while ((dent = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
		/*
		 * Skip the file if it is not a socket. Also accept
		 * DT_UNKNOWN (0) in case the C library or underlying
		 * file system does not support d_type.
		 */
		if (dent->d_type != DT_SOCK && dent->d_type != DT_UNKNOWN)
			continue;
#endif /* _DIRENT_HAVE_D_TYPE */
		if (os_strcmp(dent->d_name, ".") == 0 ||
		    os_strcmp(dent->d_name, "..") == 0)
			continue;
		printf("Selected interface '%s'\n", dent->d_name);
		ifname = os_strdup(dent->d_name);
		break;
	}
	closedir(dir);
#endif /* CONFIG_CTRL_IFACE_UNIX */

#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
	char buf[2048], *pos;
	size_t len;
	struct wpa_ctrl *ctrl;
	int ret;

	ctrl = wpa_ctrl_open(NULL);
	if (ctrl == NULL)
		return NULL;

	len = sizeof(buf) - 1;
	ret = wpa_ctrl_request(ctrl, "INTERFACES", 10, buf, &len, NULL);
	if (ret >= 0) {
		buf[len] = '\0';
		pos = os_strchr(buf, '\n');
		if (pos)
			*pos = '\0';
		ifname = os_strdup(buf);
	}
	wpa_ctrl_close(ctrl);
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */

	return ifname;
}


int main(int argc, char *argv[])
{
	int interactive;
	int warning_displayed = 0;
	int c;
	int daemonize = 0;
	const char *global = NULL;

	if (os_program_init())
		return -1;

	for (;;) {
		c = getopt(argc, argv, "a:Bg:hi:p:P:v");
		if (c < 0)
			break;
		switch (c) {
		case 'a':
			action_file = optarg;
			break;
		case 'B':
			daemonize = 1;
			break;
		case 'g':
			global = optarg;
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			printf("%s\n", wpa_cli_version);
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
		default:
			usage();
			return -1;
		}
	}

	interactive = (argc == optind) && (action_file == NULL);

	if (interactive)
		printf("%s\n\n%s\n\n", wpa_cli_version, wpa_cli_license);

	if (global) {
#ifdef CONFIG_CTRL_IFACE_NAMED_PIPE
		ctrl_conn = wpa_ctrl_open(NULL);
#else /* CONFIG_CTRL_IFACE_NAMED_PIPE */
		ctrl_conn = wpa_ctrl_open(global);
#endif /* CONFIG_CTRL_IFACE_NAMED_PIPE */
		if (ctrl_conn == NULL) {
			perror("Failed to connect to wpa_supplicant - "
			       "wpa_ctrl_open");
			return -1;
		}
	}

	for (; !global;) {
		if (ctrl_ifname == NULL)
			ctrl_ifname = wpa_cli_get_default_ifname();
		ctrl_conn = wpa_cli_open_connection(ctrl_ifname);
		if (ctrl_conn) {
			if (warning_displayed)
				printf("Connection established.\n");
			break;
		}

		if (!interactive) {
			perror("Failed to connect to wpa_supplicant - "
			       "wpa_ctrl_open");
			return -1;
		}

		if (!warning_displayed) {
			printf("Could not connect to wpa_supplicant - "
			       "re-trying\n");
			warning_displayed = 1;
		}
		os_sleep(1, 0);
		continue;
	}

#ifndef _WIN32_WCE
	signal(SIGINT, wpa_cli_terminate);
	signal(SIGTERM, wpa_cli_terminate);
#endif /* _WIN32_WCE */
#ifndef CONFIG_NATIVE_WINDOWS
	signal(SIGALRM, wpa_cli_alarm);
#endif /* CONFIG_NATIVE_WINDOWS */

	if (interactive || action_file) {
		if (wpa_ctrl_attach(ctrl_conn) == 0) {
			wpa_cli_attached = 1;
		} else {
			printf("Warning: Failed to attach to "
			       "wpa_supplicant.\n");
			if (!interactive)
				return -1;
		}
	}

	if (daemonize && os_daemonize(pid_file))
		return -1;

	if (interactive)
		wpa_cli_interactive();
	else if (action_file)
		wpa_cli_action(ctrl_conn);
	else
		wpa_request(ctrl_conn, argc - optind, &argv[optind]);

	os_free(ctrl_ifname);
	wpa_cli_cleanup();

	return 0;
}

#else /* CONFIG_CTRL_IFACE */
int main(int argc, char *argv[])
{
	printf("CONFIG_CTRL_IFACE not defined - wpa_cli disabled\n");
	return -1;
}
#endif /* CONFIG_CTRL_IFACE */
