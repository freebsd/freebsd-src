/*
 * hostapd - command line interface for hostapd daemon
 * Copyright (c) 2004-2016, Jouni Malinen <j@w1.fi>
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
#include "common/cli.h"

#ifndef CONFIG_NO_CTRL_IFACE

static const char *const hostapd_cli_version =
"hostapd_cli v" VERSION_STR "\n"
"Copyright (c) 2004-2016, Jouni Malinen <j@w1.fi> and contributors";

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
static int event_handler_registered = 0;

static DEFINE_DL_LIST(stations); /* struct cli_txt_entry */

static void print_help(FILE *stream, const char *cmd);
static char ** list_cmd_list(void);
static void hostapd_cli_receive(int sock, void *eloop_ctx, void *sock_ctx);


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
		"                socket path)\n\n");
	print_help(stderr, NULL);
}


static void register_event_handler(struct wpa_ctrl *ctrl)
{
	if (!ctrl_conn)
		return;
	if (interactive) {
		event_handler_registered =
			!eloop_register_read_sock(wpa_ctrl_get_fd(ctrl),
						  hostapd_cli_receive,
						  NULL, NULL);
	}
}


static void unregister_event_handler(struct wpa_ctrl *ctrl)
{
	if (!ctrl_conn)
		return;
	if (interactive && event_handler_registered) {
		eloop_unregister_read_sock(wpa_ctrl_get_fd(ctrl));
		event_handler_registered = 0;
	}
}


static struct wpa_ctrl * hostapd_cli_open_connection(const char *ifname)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	char *cfile;
	int flen;
#endif /* !CONFIG_CTRL_IFACE_UDP */

	if (ifname == NULL)
		return NULL;

#ifdef CONFIG_CTRL_IFACE_UDP
	ctrl_conn = wpa_ctrl_open(ifname);
	return ctrl_conn;
#else /* CONFIG_CTRL_IFACE_UDP */
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
#endif /* CONFIG_CTRL_IFACE_UDP */
}


static void hostapd_cli_close_connection(void)
{
	if (ctrl_conn == NULL)
		return;

	unregister_event_handler(ctrl_conn);
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


static int hostapd_cli_cmd(struct wpa_ctrl *ctrl, const char *cmd,
			   int min_args, int argc, char *argv[])
{
	char buf[4096];

	if (argc < min_args) {
		printf("Invalid %s command - at least %d argument%s required.\n",
		       cmd, min_args, min_args > 1 ? "s are" : " is");
		return -1;
	}
	if (write_cmd(buf, sizeof(buf), cmd, argc, argv) < 0)
		return -1;
	return wpa_ctrl_command(ctrl, buf);
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


static char ** hostapd_complete_deauthenticate(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = cli_txt_list_array(&stations);
		break;
	}

	return res;
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


static char ** hostapd_complete_disassociate(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = cli_txt_list_array(&stations);
		break;
	}

	return res;
}


#ifdef CONFIG_TAXONOMY
static int hostapd_cli_cmd_signature(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	char buf[64];

	if (argc != 1) {
		printf("Invalid 'signature' command - exactly one argument, STA address, is required.\n");
		return -1;
	}
	os_snprintf(buf, sizeof(buf), "SIGNATURE %s", argv[0]);
	return wpa_ctrl_command(ctrl, buf);
}
#endif /* CONFIG_TAXONOMY */


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
	print_help(stdout, argc > 0 ? argv[0] : NULL);
	return 0;
}


static char ** hostapd_cli_complete_help(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = list_cmd_list();
		break;
	}

	return res;
}


static int hostapd_cli_cmd_license(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	printf("%s\n\n%s\n", hostapd_cli_version, cli_full_license);
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


static void hostapd_cli_get_interfaces(struct wpa_ctrl *ctrl,
				       struct dl_list *interfaces)
{
	struct dirent *dent;
	DIR *dir;

	if (!ctrl || !interfaces)
		return;
	dir = opendir(ctrl_iface_dir);
	if (dir == NULL)
		return;

	while ((dent = readdir(dir))) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		cli_txt_list_add(interfaces, dent->d_name);
	}
	closedir(dir);
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
			register_event_handler(ctrl_conn);
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


static char ** hostapd_complete_interface(const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	DEFINE_DL_LIST(interfaces);

	switch (arg) {
	case 1:
		hostapd_cli_get_interfaces(ctrl_conn, &interfaces);
		res = cli_txt_list_array(&interfaces);
		cli_txt_list_flush(&interfaces);
		break;
	}

	return res;
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


static int hostapd_cli_cmd_raw(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	if (argc == 0)
		return -1;
	return hostapd_cli_cmd(ctrl, argv[0], 0, argc - 1, &argv[1]);
}


static int hostapd_cli_cmd_pmksa(struct wpa_ctrl *ctrl, int argc, char *argv[])
{
	return wpa_ctrl_command(ctrl, "PMKSA");
}


static int hostapd_cli_cmd_pmksa_flush(struct wpa_ctrl *ctrl, int argc,
				       char *argv[])
{
	return wpa_ctrl_command(ctrl, "PMKSA_FLUSH");
}


static int hostapd_cli_cmd_set_neighbor(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	char cmd[2048];
	int res;

	if (argc < 3 || argc > 5) {
		printf("Invalid set_neighbor command: needs 3-5 arguments\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "SET_NEIGHBOR %s %s %s %s %s",
			  argv[0], argv[1], argv[2], argc >= 4 ? argv[3] : "",
			  argc == 5 ? argv[4] : "");
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long SET_NEIGHBOR command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_remove_neighbor(struct wpa_ctrl *ctrl, int argc,
					   char *argv[])
{
	char cmd[400];
	int res;

	if (argc != 2) {
		printf("Invalid remove_neighbor command: needs 2 arguments\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "REMOVE_NEIGHBOR %s %s",
			  argv[0], argv[1]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long REMOVE_NEIGHBOR command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_req_lci(struct wpa_ctrl *ctrl, int argc,
				   char *argv[])
{
	char cmd[256];
	int res;

	if (argc != 1) {
		printf("Invalid req_lci command - requires destination address\n");
		return -1;
	}

	res = os_snprintf(cmd, sizeof(cmd), "REQ_LCI %s", argv[0]);
	if (os_snprintf_error(sizeof(cmd), res)) {
		printf("Too long REQ_LCI command.\n");
		return -1;
	}
	return wpa_ctrl_command(ctrl, cmd);
}


static int hostapd_cli_cmd_req_range(struct wpa_ctrl *ctrl, int argc,
				     char *argv[])
{
	if (argc < 4) {
		printf("Invalid req_range command: needs at least 4 arguments - dest address, randomization interval, min AP count, and 1 to 16 AP addresses\n");
		return -1;
	}

	return hostapd_cli_cmd(ctrl, "REQ_RANGE", 4, argc, argv);
}


static int hostapd_cli_cmd_driver_flags(struct wpa_ctrl *ctrl, int argc,
					char *argv[])
{
	return wpa_ctrl_command(ctrl, "DRIVER_FLAGS");
}


struct hostapd_cli_cmd {
	const char *cmd;
	int (*handler)(struct wpa_ctrl *ctrl, int argc, char *argv[]);
	char ** (*completion)(const char *str, int pos);
	const char *usage;
};

static const struct hostapd_cli_cmd hostapd_cli_commands[] = {
	{ "ping", hostapd_cli_cmd_ping, NULL,
	  "= pings hostapd" },
	{ "mib", hostapd_cli_cmd_mib, NULL,
	  "= get MIB variables (dot1x, dot11, radius)" },
	{ "relog", hostapd_cli_cmd_relog, NULL, NULL },
	{ "status", hostapd_cli_cmd_status, NULL, NULL },
	{ "sta", hostapd_cli_cmd_sta, NULL,
	  "<addr> = get MIB variables for one station" },
	{ "all_sta", hostapd_cli_cmd_all_sta, NULL,
	   "= get MIB variables for all stations" },
	{ "new_sta", hostapd_cli_cmd_new_sta, NULL,
	  "<addr> = add a new station" },
	{ "deauthenticate", hostapd_cli_cmd_deauthenticate,
	  hostapd_complete_deauthenticate,
	  "<addr> = deauthenticate a station" },
	{ "disassociate", hostapd_cli_cmd_disassociate,
	  hostapd_complete_disassociate,
	  "<addr> = disassociate a station" },
#ifdef CONFIG_TAXONOMY
	{ "signature", hostapd_cli_cmd_signature, NULL,
	  "<addr> = get taxonomy signature for a station" },
#endif /* CONFIG_TAXONOMY */
#ifdef CONFIG_IEEE80211W
	{ "sa_query", hostapd_cli_cmd_sa_query, NULL,
	  "<addr> = send SA Query to a station" },
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WPS
	{ "wps_pin", hostapd_cli_cmd_wps_pin, NULL,
	  "<uuid> <pin> [timeout] [addr] = add WPS Enrollee PIN" },
	{ "wps_check_pin", hostapd_cli_cmd_wps_check_pin, NULL,
	  "<PIN> = verify PIN checksum" },
	{ "wps_pbc", hostapd_cli_cmd_wps_pbc, NULL,
	  "= indicate button pushed to initiate PBC" },
	{ "wps_cancel", hostapd_cli_cmd_wps_cancel, NULL,
	  "= cancel the pending WPS operation" },
#ifdef CONFIG_WPS_NFC
	{ "wps_nfc_tag_read", hostapd_cli_cmd_wps_nfc_tag_read, NULL,
	  "<hexdump> = report read NFC tag with WPS data" },
	{ "wps_nfc_config_token", hostapd_cli_cmd_wps_nfc_config_token, NULL,
	  "<WPS/NDEF> = build NFC configuration token" },
	{ "wps_nfc_token", hostapd_cli_cmd_wps_nfc_token, NULL,
	  "<WPS/NDEF/enable/disable> = manager NFC password token" },
	{ "nfc_get_handover_sel", hostapd_cli_cmd_nfc_get_handover_sel, NULL,
	  NULL },
#endif /* CONFIG_WPS_NFC */
	{ "wps_ap_pin", hostapd_cli_cmd_wps_ap_pin, NULL,
	  "<cmd> [params..] = enable/disable AP PIN" },
	{ "wps_config", hostapd_cli_cmd_wps_config, NULL,
	  "<SSID> <auth> <encr> <key> = configure AP" },
	{ "wps_get_status", hostapd_cli_cmd_wps_get_status, NULL,
	  "= show current WPS status" },
#endif /* CONFIG_WPS */
	{ "disassoc_imminent", hostapd_cli_cmd_disassoc_imminent, NULL, NULL },
	{ "ess_disassoc", hostapd_cli_cmd_ess_disassoc, NULL, NULL },
	{ "bss_tm_req", hostapd_cli_cmd_bss_tm_req, NULL, NULL },
	{ "get_config", hostapd_cli_cmd_get_config, NULL,
	  "= show current configuration" },
	{ "help", hostapd_cli_cmd_help, hostapd_cli_complete_help,
	  "= show this usage help" },
	{ "interface", hostapd_cli_cmd_interface, hostapd_complete_interface,
	  "[ifname] = show interfaces/select interface" },
#ifdef CONFIG_FST
	{ "fst", hostapd_cli_cmd_fst, NULL, NULL },
#endif /* CONFIG_FST */
	{ "raw", hostapd_cli_cmd_raw, NULL, NULL },
	{ "level", hostapd_cli_cmd_level, NULL,
	  "<debug level> = change debug level" },
	{ "license", hostapd_cli_cmd_license, NULL,
	  "= show full hostapd_cli license" },
	{ "quit", hostapd_cli_cmd_quit, NULL,
	  "= exit hostapd_cli" },
	{ "set", hostapd_cli_cmd_set, NULL, NULL },
	{ "get", hostapd_cli_cmd_get, NULL, NULL },
	{ "set_qos_map_set", hostapd_cli_cmd_set_qos_map_set, NULL, NULL },
	{ "send_qos_map_conf", hostapd_cli_cmd_send_qos_map_conf, NULL, NULL },
	{ "chan_switch", hostapd_cli_cmd_chan_switch, NULL, NULL },
	{ "hs20_wnm_notif", hostapd_cli_cmd_hs20_wnm_notif, NULL, NULL },
	{ "hs20_deauth_req", hostapd_cli_cmd_hs20_deauth_req, NULL, NULL },
	{ "vendor", hostapd_cli_cmd_vendor, NULL, NULL },
	{ "enable", hostapd_cli_cmd_enable, NULL, NULL },
	{ "reload", hostapd_cli_cmd_reload, NULL, NULL },
	{ "disable", hostapd_cli_cmd_disable, NULL, NULL },
	{ "erp_flush", hostapd_cli_cmd_erp_flush, NULL, NULL },
	{ "log_level", hostapd_cli_cmd_log_level, NULL, NULL },
	{ "pmksa", hostapd_cli_cmd_pmksa, NULL, NULL },
	{ "pmksa_flush", hostapd_cli_cmd_pmksa_flush, NULL, NULL },
	{ "set_neighbor", hostapd_cli_cmd_set_neighbor, NULL, NULL },
	{ "remove_neighbor", hostapd_cli_cmd_remove_neighbor, NULL, NULL },
	{ "req_lci", hostapd_cli_cmd_req_lci, NULL, NULL },
	{ "req_range", hostapd_cli_cmd_req_range, NULL, NULL },
	{ "driver_flags", hostapd_cli_cmd_driver_flags, NULL, NULL },
	{ NULL, NULL, NULL, NULL }
};


/*
 * Prints command usage, lines are padded with the specified string.
 */
static void print_cmd_help(FILE *stream, const struct hostapd_cli_cmd *cmd,
			   const char *pad)
{
	char c;
	size_t n;

	if (cmd->usage == NULL)
		return;
	fprintf(stream, "%s%s ", pad, cmd->cmd);
	for (n = 0; (c = cmd->usage[n]); n++) {
		fprintf(stream, "%c", c);
		if (c == '\n')
			fprintf(stream, "%s", pad);
	}
	fprintf(stream, "\n");
}


static void print_help(FILE *stream, const char *cmd)
{
	int n;

	fprintf(stream, "commands:\n");
	for (n = 0; hostapd_cli_commands[n].cmd; n++) {
		if (cmd == NULL || str_starts(hostapd_cli_commands[n].cmd, cmd))
			print_cmd_help(stream, &hostapd_cli_commands[n], "  ");
	}
}


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


static void cli_event(const char *str)
{
	const char *start, *s;

	start = os_strchr(str, '>');
	if (start == NULL)
		return;

	start++;

	if (str_starts(start, AP_STA_CONNECTED)) {
		s = os_strchr(start, ' ');
		if (s == NULL)
			return;
		cli_txt_list_add(&stations, s + 1);
		return;
	}

	if (str_starts(start, AP_STA_DISCONNECTED)) {
		s = os_strchr(start, ' ');
		if (s == NULL)
			return;
		cli_txt_list_del_addr(&stations, s + 1);
		return;
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
				cli_event(buf);
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


static void hostapd_cli_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	hostapd_cli_recv_pending(ctrl_conn, 0, 0);
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
				register_event_handler(ctrl_conn);
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


static char ** list_cmd_list(void)
{
	char **res;
	int i, count;

	count = ARRAY_SIZE(hostapd_cli_commands);
	res = os_calloc(count + 1, sizeof(char *));
	if (res == NULL)
		return NULL;

	for (i = 0; hostapd_cli_commands[i].cmd; i++) {
		res[i] = os_strdup(hostapd_cli_commands[i].cmd);
		if (res[i] == NULL)
			break;
	}

	return res;
}


static char ** hostapd_cli_cmd_completion(const char *cmd, const char *str,
				      int pos)
{
	int i;

	for (i = 0; hostapd_cli_commands[i].cmd; i++) {
		if (os_strcasecmp(hostapd_cli_commands[i].cmd, cmd) != 0)
			continue;
		if (hostapd_cli_commands[i].completion)
			return hostapd_cli_commands[i].completion(str, pos);
		if (!hostapd_cli_commands[i].usage)
			return NULL;
		edit_clear_line();
		printf("\r%s\n", hostapd_cli_commands[i].usage);
		edit_redraw();
		break;
	}

	return NULL;
}


static char ** hostapd_cli_edit_completion_cb(void *ctx, const char *str,
					      int pos)
{
	char **res;
	const char *end;
	char *cmd;

	end = os_strchr(str, ' ');
	if (end == NULL || str + pos < end)
		return list_cmd_list();

	cmd = os_malloc(pos + 1);
	if (cmd == NULL)
		return NULL;
	os_memcpy(cmd, str, pos);
	cmd[end - str] = '\0';
	res = hostapd_cli_cmd_completion(cmd, str, pos);
	os_free(cmd);
	return res;
}


static void hostapd_cli_interactive(void)
{
	printf("\nInteractive mode\n\n");

	eloop_register_signal_terminate(hostapd_cli_eloop_terminate, NULL);
	edit_init(hostapd_cli_edit_cmd_cb, hostapd_cli_edit_eof_cb,
		  hostapd_cli_edit_completion_cb, NULL, NULL, NULL);
	eloop_register_timeout(ping_interval, 0, hostapd_cli_ping, NULL, NULL);

	eloop_run();

	cli_txt_list_flush(&stations);
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
		printf("%s\n\n%s\n\n", hostapd_cli_version, cli_license);
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
			register_event_handler(ctrl_conn);
		} else {
			printf("Warning: Failed to attach to hostapd.\n");
			if (action_file)
				return -1;
		}
	}

	if (daemonize && os_daemonize(pid_file) && eloop_sock_requeue())
		return -1;

	if (interactive)
		hostapd_cli_interactive();
	else if (action_file)
		hostapd_cli_action(ctrl_conn);
	else
		wpa_request(ctrl_conn, argc - optind, &argv[optind]);

	unregister_event_handler(ctrl_conn);
	os_free(ctrl_ifname);
	eloop_destroy();
	hostapd_cli_cleanup();
	return 0;
}

#else /* CONFIG_NO_CTRL_IFACE */

int main(int argc, char *argv[])
{
	return -1;
}

#endif /* CONFIG_NO_CTRL_IFACE */
