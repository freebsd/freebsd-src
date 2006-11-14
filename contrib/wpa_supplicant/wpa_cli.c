/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
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
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef CONFIG_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* CONFIG_READLINE */

#include "wpa_ctrl.h"
#ifdef CONFIG_NATIVE_WINDOWS
#include "common.h"
#endif /* CONFIG_NATIVE_WINDOWS */
#include "version.h"


static const char *wpa_cli_version =
"wpa_cli v" VERSION_STR "\n"
"Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi> and contributors";


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
"Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
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
"  terminate = terminate wpa_supplicant\n"
"  quit = exit wpa_cli\n";

static struct wpa_ctrl *ctrl_conn;
static int wpa_cli_quit = 0;
static int wpa_cli_attached = 0;
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
#ifdef CONFIG_CTRL_IFACE_UDP
	ctrl_conn = wpa_ctrl_open("");
	return ctrl_conn;
#else /* CONFIG_CTRL_IFACE_UDP */
	char *cfile;
	int flen;

	if (ifname == NULL)
		return NULL;

	flen = strlen(ctrl_iface_dir) + strlen(ifname) + 2;
	cfile = malloc(flen);
	if (cfile == NULL)
		return NULL;
	snprintf(cfile, flen, "%s/%s", ctrl_iface_dir, ifname);

	ctrl_conn = wpa_ctrl_open(cfile);
	free(cfile);
	return ctrl_conn;
#endif /* CONFIG_CTRL_IFACE_UDP */
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
	ret = wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, &len,
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
	int verbose = argc > 0 && strcmp(argv[0], "verbose") == 0;
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

	if (argc == 0) {
		wpa_cli_show_variables();
		return 0;
	}

	if (argc != 2) {
		printf("Invalid SET command: needs two arguments (variable "
		       "name and value)\n");
		return 0;
	}

	if (snprintf(cmd, sizeof(cmd), "SET %s %s", argv[0], argv[1]) >=
	    sizeof(cmd) - 1) {
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

	if (argc != 1) {
		printf("Invalid PREAUTH command: needs one argument "
		       "(BSSID)\n");
		return 0;
	}

	if (snprintf(cmd, sizeof(cmd), "PREAUTH %s", argv[0]) >=
	    sizeof(cmd) - 1) {
		printf("Too long PREAUTH command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_level(struct wpa_ctrl *ctrl, int argc, char *argv[])
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


static int wpa_cli_cmd_identity(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid IDENTITY command: needs two arguments "
		       "(network id and identity)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "IDENTITY-%s:%s",
		       argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_password(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid PASSWORD command: needs two arguments "
		       "(network id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "PASSWORD-%s:%s",
		       argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_new_password(struct wpa_ctrl *ctrl, int argc,
				    char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid NEW_PASSWORD command: needs two arguments "
		       "(network id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "NEW_PASSWORD-%s:%s",
		       argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_pin(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid PIN command: needs two arguments "
		       "(network id and pin)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "PIN-%s:%s",
			argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_otp(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid OTP command: needs two arguments (network "
		       "id and password)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "OTP-%s:%s",
		       argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_passphrase(struct wpa_ctrl *ctrl, int argc,
				  char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid PASSPHRASE command: needs two arguments "
		       "(network id and passphrase)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, WPA_CTRL_RSP "PASSPHRASE-%s:%s",
		       argv[0], argv[1]);
	for (i = 2; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_bssid(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	char cmd[256], *pos, *end;
	int i;

	if (argc < 2) {
		printf("Invalid BSSID command: needs two arguments (network "
		       "id and BSSID)\n");
		return 0;
	}

	end = cmd + sizeof(cmd);
	pos = cmd;
	pos += snprintf(pos, end - pos, "BSSID");
	for (i = 0; i < argc; i++)
		pos += snprintf(pos, end - pos, " %s", argv[i]);

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

	snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %s", argv[0]);

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

	snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %s", argv[0]);

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

	snprintf(cmd, sizeof(cmd), "DISABLE_NETWORK %s", argv[0]);

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

	snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %s", argv[0]);

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

	if (argc == 0) {
		wpa_cli_show_network_variables();
		return 0;
	}

	if (argc != 3) {
		printf("Invalid SET_NETWORK command: needs three arguments\n"
		       "(network id, variable name, and value)\n");
		return 0;
	}

	if (snprintf(cmd, sizeof(cmd), "SET_NETWORK %s %s %s",
		     argv[0], argv[1], argv[2]) >= sizeof(cmd) - 1) {
		printf("Too long SET_NETWORK command.\n");
		return 0;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int wpa_cli_cmd_get_network(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];

	if (argc == 0) {
		wpa_cli_show_network_variables();
		return 0;
	}

	if (argc != 2) {
		printf("Invalid GET_NETWORK command: needs two arguments\n"
		       "(network id and variable name)\n");
		return 0;
	}

	if (snprintf(cmd, sizeof(cmd), "GET_NETWORK %s %s",
		     argv[0], argv[1]) >= sizeof(cmd) - 1) {
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

	if (argc != 1) {
		printf("Invalid GET_CAPABILITY command: needs one argument\n");
		return 0;
	}

	snprintf(cmd, sizeof(cmd), "GET_CAPABILITY %s", argv[0]);

	return wpa_ctrl_command(ctrl, cmd);
}


static void wpa_cli_list_interfaces(struct wpa_ctrl *ctrl)
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


static int wpa_cli_cmd_interface(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc < 1) {
		wpa_cli_list_interfaces(ctrl);
		return 0;
	}

	wpa_cli_close_connection();
	free(ctrl_ifname);
	ctrl_ifname = strdup(argv[0]);

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
			"driver_param\n");
		return 0;
	}

	/*
	 * INTERFACE_ADD <ifname>TAB<confname>TAB<driver>TAB<ctrl_interface>TAB
	 * <driver_param>
	 */
	snprintf(cmd, sizeof(cmd), "INTERFACE_ADD %s\t%s\t%s\t%s\t%s", argv[0],
		 argc > 1 ? argv[1] : "", argc > 2 ? argv[2] : "",
		 argc > 3 ? argv[3] : "", argc > 4 ? argv[4] : "");
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

	snprintf(cmd, sizeof(cmd), "INTERFACE_REMOVE %s", argv[0]);
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
	{ NULL, NULL }
};


static void wpa_request(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	struct wpa_cli_cmd *cmd, *match = NULL;
	int count;

	count = 0;
	cmd = wpa_cli_commands;
	while (cmd->cmd) {
		if (strncasecmp(cmd->cmd, argv[0], strlen(argv[0])) == 0) {
			match = cmd;
			if (strcasecmp(cmd->cmd, argv[0]) == 0) {
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


static int str_match(const char *a, const char *b)
{
	return strncmp(a, b, strlen(b)) == 0;
}


static int wpa_cli_exec(const char *program, const char *arg1,
			const char *arg2)
{
	char *cmd;
	size_t len;

	len = strlen(program) + strlen(arg1) + strlen(arg2) + 3;
	cmd = malloc(len);
	if (cmd == NULL)
		return -1;
	snprintf(cmd, len, "%s %s %s", program, arg1, arg2);
	system(cmd);
	free(cmd);

	return 0;
}


static void wpa_cli_action_process(const char *msg)
{
	const char *pos;

	pos = msg;
	if (*pos == '<') {
		/* skip priority */
		pos = strchr(pos, '>');
		if (pos)
			pos++;
		else
			pos = msg;
	}

	if (str_match(pos, WPA_EVENT_CONNECTED)) {
		wpa_cli_exec(action_file, ctrl_ifname, "CONNECTED");
	} else if (str_match(pos, WPA_EVENT_DISCONNECTED)) {
		wpa_cli_exec(action_file, ctrl_ifname, "DISCONNECTED");
	} else if (str_match(pos, WPA_EVENT_TERMINATING)) {
		printf("wpa_supplicant is terminating - stop monitoring\n");
		wpa_cli_quit = 1;
	}
}


static void wpa_cli_action_cb(char *msg, size_t len)
{
	wpa_cli_action_process(msg);
}


static void wpa_cli_recv_pending(struct wpa_ctrl *ctrl, int in_read,
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
}


#ifdef CONFIG_READLINE
static char * wpa_cli_cmd_gen(const char *text, int state)
{
	static int i, len;
	const char *cmd;

	if (state == 0) {
		i = 0;
		len = strlen(text);
	}

	while ((cmd = wpa_cli_commands[i].cmd)) {
		i++;
		if (strncasecmp(cmd, text, len) == 0)
			return strdup(cmd);
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
		int hfile_len = strlen(home) + 1 + strlen(fname) + 1;
		hfile = malloc(hfile_len);
		if (hfile) {
			snprintf(hfile, hfile_len, "%s/%s", home, fname);
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
			if (h == NULL || strcmp(cmd, h->line) != 0)
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
				char *pos2 = strrchr(pos, '"');
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
			free(cmd);
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
			if (strncasecmp(p, "pa", 2) == 0 ||
			    strncasecmp(p, "o", 1) == 0 ||
			    strncasecmp(p, "n", 1)) {
				h = remove_history(where_history());
				if (h) {
					free(h->line);
					free(h->data);
					free(h);
				}
				h = current_history();
			} else {
				h = next_history();
			}
		}
		write_history(hfile);
		free(hfile);
	}
#endif /* CONFIG_READLINE */
}


static void wpa_cli_action(struct wpa_ctrl *ctrl)
{
	fd_set rfds;
	int fd, res;
	struct timeval tv;
	char buf[16];
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
			    len < 4 || memcmp(buf, "PONG", 4) != 0) {
				printf("wpa_supplicant did not reply to PING "
				       "command - exiting\n");
				break;
			}
		}
	}
}


static void wpa_cli_cleanup(void)
{
	wpa_cli_close_connection();
	if (pid_file)
		unlink(pid_file);

#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */
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
	if (!ctrl_conn) {
		ctrl_conn = wpa_cli_open_connection(ctrl_ifname);
		if (ctrl_conn) {
			printf("Connection to wpa_supplicant "
			       "re-established\n");
			if (wpa_ctrl_attach(ctrl_conn) == 0) {
				wpa_cli_attached = 1;
			} else {
				printf("Warning: Failed to attach to "
				       "wpa_supplicant.\n");
			}
		}
	}
	if (ctrl_conn)
		wpa_cli_recv_pending(ctrl_conn, 1, 0);
	alarm(1);
}
#endif /* CONFIG_NATIVE_WINDOWS */


int main(int argc, char *argv[])
{
	int interactive;
	int warning_displayed = 0;
	int c;
	int daemonize = 0;
	FILE *f;
	const char *global = NULL;

#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		printf("Could not find a usable WinSock.dll\n");
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

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
			ctrl_ifname = strdup(optarg);
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
		ctrl_conn = wpa_ctrl_open(global);
		if (ctrl_conn == NULL) {
			perror("Failed to connect to wpa_supplicant - "
			       "wpa_ctrl_open");
			return -1;
		}
	}

	for (; !global;) {
		if (ctrl_ifname == NULL) {
			struct dirent *dent;
			DIR *dir = opendir(ctrl_iface_dir);
			if (dir) {
				while ((dent = readdir(dir))) {
					if (strcmp(dent->d_name, ".") == 0 ||
					    strcmp(dent->d_name, "..") == 0)
						continue;
					printf("Selected interface '%s'\n",
					       dent->d_name);
					ctrl_ifname = strdup(dent->d_name);
					break;
				}
				closedir(dir);
			}
		}
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
		sleep(1);
		continue;
	}

	signal(SIGINT, wpa_cli_terminate);
	signal(SIGTERM, wpa_cli_terminate);
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

	if (daemonize && daemon(0, 0)) {
		perror("daemon");
		return -1;
	}

	if (pid_file) {
		f = fopen(pid_file, "w");
		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}

	if (interactive)
		wpa_cli_interactive();
	else if (action_file)
		wpa_cli_action(ctrl_conn);
	else
		wpa_request(ctrl_conn, argc - optind, &argv[optind]);

	free(ctrl_ifname);
	wpa_cli_cleanup();

	return 0;
}
