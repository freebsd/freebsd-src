/*
 * wlantest controller
 * Copyright (c) 2010-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/un.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/edit.h"
#include "wlantest_ctrl.h"


static int get_cmd_arg_num(const char *str, int pos)
{
	int arg = 0, i;

	for (i = 0; i <= pos; i++) {
		if (str[i] != ' ') {
			arg++;
			while (i <= pos && str[i] != ' ')
				i++;
		}
	}

	if (arg > 0)
		arg--;
	return arg;
}


static int get_prev_arg_pos(const char *str, int pos)
{
	while (pos > 0 && str[pos - 1] != ' ')
		pos--;
	while (pos > 0 && str[pos - 1] == ' ')
		pos--;
	while (pos > 0 && str[pos - 1] != ' ')
		pos--;
	return pos;
}


static u8 * attr_get(u8 *buf, size_t buflen, enum wlantest_ctrl_attr attr,
		     size_t *len)
{
	u8 *pos = buf;

	while (pos + 8 <= buf + buflen) {
		enum wlantest_ctrl_attr a;
		size_t alen;
		a = WPA_GET_BE32(pos);
		pos += 4;
		alen = WPA_GET_BE32(pos);
		pos += 4;
		if (pos + alen > buf + buflen) {
			printf("Invalid control message attribute\n");
			return NULL;
		}
		if (a == attr) {
			*len = alen;
			return pos;
		}
		pos += alen;
	}

	return NULL;
}


static u8 * attr_hdr_add(u8 *pos, u8 *end, enum wlantest_ctrl_attr attr,
			 size_t len)
{
	if (pos == NULL || end - pos < 8 + len)
		return NULL;
	WPA_PUT_BE32(pos, attr);
	pos += 4;
	WPA_PUT_BE32(pos, len);
	pos += 4;
	return pos;
}


static u8 * attr_add_str(u8 *pos, u8 *end, enum wlantest_ctrl_attr attr,
			 const char *str)
{
	size_t len = os_strlen(str);

	if (pos == NULL || end - pos < 8 + len)
		return NULL;
	WPA_PUT_BE32(pos, attr);
	pos += 4;
	WPA_PUT_BE32(pos, len);
	pos += 4;
	os_memcpy(pos, str, len);
	pos += len;
	return pos;
}


static u8 * attr_add_be32(u8 *pos, u8 *end, enum wlantest_ctrl_attr attr,
			  u32 val)
{
	if (pos == NULL || end - pos < 12)
		return NULL;
	WPA_PUT_BE32(pos, attr);
	pos += 4;
	WPA_PUT_BE32(pos, 4);
	pos += 4;
	WPA_PUT_BE32(pos, val);
	pos += 4;
	return pos;
}


static int cmd_send_and_recv(int s, const u8 *cmd, size_t cmd_len,
			     u8 *resp, size_t max_resp_len)
{
	int res;
	enum wlantest_ctrl_cmd cmd_resp;

	if (send(s, cmd, cmd_len, 0) < 0)
		return -1;
	res = recv(s, resp, max_resp_len, 0);
	if (res < 4)
		return -1;

	cmd_resp = WPA_GET_BE32(resp);
	if (cmd_resp == WLANTEST_CTRL_SUCCESS)
		return res;

	if (cmd_resp == WLANTEST_CTRL_UNKNOWN_CMD)
		printf("Unknown command\n");
	else if (cmd_resp == WLANTEST_CTRL_INVALID_CMD)
		printf("Invalid command\n");

	return -1;
}


static int cmd_simple(int s, enum wlantest_ctrl_cmd cmd)
{
	u8 buf[4];
	int res;
	WPA_PUT_BE32(buf, cmd);
	res = cmd_send_and_recv(s, buf, sizeof(buf), buf, sizeof(buf));
	return res < 0 ? -1 : 0;
}


static char ** get_bssid_list(int s)
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[4];
	u8 *bssid;
	size_t len;
	int rlen, i;
	char **res;

	WPA_PUT_BE32(buf, WLANTEST_CTRL_LIST_BSS);
	rlen = cmd_send_and_recv(s, buf, sizeof(buf), resp, sizeof(resp));
	if (rlen < 0)
		return NULL;

	bssid = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_BSSID, &len);
	if (bssid == NULL)
		return NULL;

	res = os_calloc(len / ETH_ALEN + 1, sizeof(char *));
	if (res == NULL)
		return NULL;
	for (i = 0; i < len / ETH_ALEN; i++) {
		res[i] = os_zalloc(18);
		if (res[i] == NULL)
			break;
		os_snprintf(res[i], 18, MACSTR, MAC2STR(bssid + ETH_ALEN * i));
	}

	return res;
}


static char ** get_sta_list(int s, const u8 *bssid, int add_bcast)
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos, *end;
	u8 *addr;
	size_t len;
	int rlen, i;
	char **res;

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_LIST_STA);
	pos += 4;
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	os_memcpy(pos, bssid, ETH_ALEN);
	pos += ETH_ALEN;
	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return NULL;

	addr = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_STA_ADDR, &len);
	if (addr == NULL)
		return NULL;

	res = os_calloc(len / ETH_ALEN + 1 + add_bcast, sizeof(char *));
	if (res == NULL)
		return NULL;
	for (i = 0; i < len / ETH_ALEN; i++) {
		res[i] = os_zalloc(18);
		if (res[i] == NULL)
			break;
		os_snprintf(res[i], 18, MACSTR, MAC2STR(addr + ETH_ALEN * i));
	}
	if (add_bcast)
		res[i] = os_strdup("ff:ff:ff:ff:ff:ff");

	return res;
}


static int cmd_ping(int s, int argc, char *argv[])
{
	int res = cmd_simple(s, WLANTEST_CTRL_PING);
	if (res == 0)
		printf("PONG\n");
	return res == 0;
}


static int cmd_terminate(int s, int argc, char *argv[])
{
	return cmd_simple(s, WLANTEST_CTRL_TERMINATE);
}


static int cmd_list_bss(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[4];
	u8 *bssid;
	size_t len;
	int rlen, i;

	WPA_PUT_BE32(buf, WLANTEST_CTRL_LIST_BSS);
	rlen = cmd_send_and_recv(s, buf, sizeof(buf), resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	bssid = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_BSSID, &len);
	if (bssid == NULL)
		return -1;

	for (i = 0; i < len / ETH_ALEN; i++)
		printf(MACSTR " ", MAC2STR(bssid + ETH_ALEN * i));
	printf("\n");

	return 0;
}


static int cmd_list_sta(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos;
	u8 *addr;
	size_t len;
	int rlen, i;

	if (argc < 1) {
		printf("list_sta needs one argument: BSSID\n");
		return -1;
	}

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_LIST_STA);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_BSSID);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	addr = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_STA_ADDR, &len);
	if (addr == NULL)
		return -1;

	for (i = 0; i < len / ETH_ALEN; i++)
		printf(MACSTR " ", MAC2STR(addr + ETH_ALEN * i));
	printf("\n");

	return 0;
}


static char ** complete_list_sta(int s, const char *str, int pos)
{
	if (get_cmd_arg_num(str, pos) == 1)
		return get_bssid_list(s);
	return NULL;
}


static int cmd_flush(int s, int argc, char *argv[])
{
	return cmd_simple(s, WLANTEST_CTRL_FLUSH);
}


static int cmd_clear_sta_counters(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos;
	int rlen;

	if (argc < 2) {
		printf("clear_sta_counters needs two arguments: BSSID and "
		       "STA address\n");
		return -1;
	}

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_CLEAR_STA_COUNTERS);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_BSSID);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	WPA_PUT_BE32(pos, WLANTEST_ATTR_STA_ADDR);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid STA address '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	printf("OK\n");
	return 0;
}


static char ** complete_clear_sta_counters(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		res = get_bssid_list(s);
		break;
	case 2:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


static int cmd_clear_bss_counters(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos;
	int rlen;

	if (argc < 1) {
		printf("clear_bss_counters needs one argument: BSSID\n");
		return -1;
	}

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_CLEAR_BSS_COUNTERS);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_BSSID);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	printf("OK\n");
	return 0;
}


static char ** complete_clear_bss_counters(int s, const char *str, int pos)
{
	if (get_cmd_arg_num(str, pos) == 1)
		return get_bssid_list(s);
	return NULL;
}


static int cmd_clear_tdls_counters(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos;
	int rlen;

	if (argc < 3) {
		printf("clear_tdls_counters needs three arguments: BSSID, "
		       "STA1 address, STA2 address\n");
		return -1;
	}

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_CLEAR_TDLS_COUNTERS);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_BSSID);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	WPA_PUT_BE32(pos, WLANTEST_ATTR_STA_ADDR);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid STA1 address '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	WPA_PUT_BE32(pos, WLANTEST_ATTR_STA2_ADDR);
	pos += 4;
	WPA_PUT_BE32(pos, ETH_ALEN);
	pos += 4;
	if (hwaddr_aton(argv[2], pos) < 0) {
		printf("Invalid STA2 address '%s'\n", argv[2]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	printf("OK\n");
	return 0;
}


static char ** complete_clear_tdls_counters(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		res = get_bssid_list(s);
		break;
	case 2:
	case 3:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


struct sta_counters {
	const char *name;
	enum wlantest_sta_counter num;
};

static const struct sta_counters sta_counters[] = {
	{ "auth_tx", WLANTEST_STA_COUNTER_AUTH_TX },
	{ "auth_rx", WLANTEST_STA_COUNTER_AUTH_RX },
	{ "assocreq_tx", WLANTEST_STA_COUNTER_ASSOCREQ_TX },
	{ "reassocreq_tx", WLANTEST_STA_COUNTER_REASSOCREQ_TX },
	{ "ptk_learned", WLANTEST_STA_COUNTER_PTK_LEARNED },
	{ "valid_deauth_tx", WLANTEST_STA_COUNTER_VALID_DEAUTH_TX },
	{ "valid_deauth_rx", WLANTEST_STA_COUNTER_VALID_DEAUTH_RX },
	{ "invalid_deauth_tx", WLANTEST_STA_COUNTER_INVALID_DEAUTH_TX },
	{ "invalid_deauth_rx", WLANTEST_STA_COUNTER_INVALID_DEAUTH_RX },
	{ "valid_disassoc_tx", WLANTEST_STA_COUNTER_VALID_DISASSOC_TX },
	{ "valid_disassoc_rx", WLANTEST_STA_COUNTER_VALID_DISASSOC_RX },
	{ "invalid_disassoc_tx", WLANTEST_STA_COUNTER_INVALID_DISASSOC_TX },
	{ "invalid_disassoc_rx", WLANTEST_STA_COUNTER_INVALID_DISASSOC_RX },
	{ "valid_saqueryreq_tx", WLANTEST_STA_COUNTER_VALID_SAQUERYREQ_TX },
	{ "valid_saqueryreq_rx", WLANTEST_STA_COUNTER_VALID_SAQUERYREQ_RX },
	{ "invalid_saqueryreq_tx",
	  WLANTEST_STA_COUNTER_INVALID_SAQUERYREQ_TX },
	{ "invalid_saqueryreq_rx",
	  WLANTEST_STA_COUNTER_INVALID_SAQUERYREQ_RX },
	{ "valid_saqueryresp_tx", WLANTEST_STA_COUNTER_VALID_SAQUERYRESP_TX },
	{ "valid_saqueryresp_rx", WLANTEST_STA_COUNTER_VALID_SAQUERYRESP_RX },
	{ "invalid_saqueryresp_tx",
	  WLANTEST_STA_COUNTER_INVALID_SAQUERYRESP_TX },
	{ "invalid_saqueryresp_rx",
	  WLANTEST_STA_COUNTER_INVALID_SAQUERYRESP_RX },
	{ "ping_ok", WLANTEST_STA_COUNTER_PING_OK },
	{ "assocresp_comeback", WLANTEST_STA_COUNTER_ASSOCRESP_COMEBACK },
	{ "reassocresp_comeback", WLANTEST_STA_COUNTER_REASSOCRESP_COMEBACK },
	{ "ping_ok_first_assoc", WLANTEST_STA_COUNTER_PING_OK_FIRST_ASSOC },
	{ "valid_deauth_rx_ack", WLANTEST_STA_COUNTER_VALID_DEAUTH_RX_ACK },
	{ "valid_disassoc_rx_ack",
	  WLANTEST_STA_COUNTER_VALID_DISASSOC_RX_ACK },
	{ "invalid_deauth_rx_ack",
	  WLANTEST_STA_COUNTER_INVALID_DEAUTH_RX_ACK },
	{ "invalid_disassoc_rx_ack",
	  WLANTEST_STA_COUNTER_INVALID_DISASSOC_RX_ACK },
	{ "deauth_rx_asleep", WLANTEST_STA_COUNTER_DEAUTH_RX_ASLEEP },
	{ "deauth_rx_awake", WLANTEST_STA_COUNTER_DEAUTH_RX_AWAKE },
	{ "disassoc_rx_asleep", WLANTEST_STA_COUNTER_DISASSOC_RX_ASLEEP },
	{ "disassoc_rx_awake", WLANTEST_STA_COUNTER_DISASSOC_RX_AWAKE },
	{ "prot_data_tx", WLANTEST_STA_COUNTER_PROT_DATA_TX },
	{ "deauth_rx_rc6", WLANTEST_STA_COUNTER_DEAUTH_RX_RC6 },
	{ "deauth_rx_rc7", WLANTEST_STA_COUNTER_DEAUTH_RX_RC7 },
	{ "disassoc_rx_rc6", WLANTEST_STA_COUNTER_DISASSOC_RX_RC6 },
	{ "disassoc_rx_rc7", WLANTEST_STA_COUNTER_DISASSOC_RX_RC7 },
	{ NULL, 0 }
};

static int cmd_get_sta_counter(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	size_t len;

	if (argc != 3) {
		printf("get_sta_counter needs at three arguments: "
		       "counter name, BSSID, and STA address\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_GET_STA_COUNTER);
	pos += 4;

	for (i = 0; sta_counters[i].name; i++) {
		if (os_strcasecmp(sta_counters[i].name, argv[0]) == 0)
			break;
	}
	if (sta_counters[i].name == NULL) {
		printf("Unknown STA counter '%s'\n", argv[0]);
		printf("Counters:");
		for (i = 0; sta_counters[i].name; i++)
			printf(" %s", sta_counters[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_STA_COUNTER,
			    sta_counters[i].num);
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[2], pos) < 0) {
		printf("Invalid STA address '%s'\n", argv[2]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_COUNTER, &len);
	if (pos == NULL || len != 4)
		return -1;
	printf("%u\n", WPA_GET_BE32(pos));
	return 0;
}


static char ** complete_get_sta_counter(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		/* counter list */
		count = ARRAY_SIZE(sta_counters);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			return NULL;
		for (i = 0; sta_counters[i].name; i++) {
			res[i] = os_strdup(sta_counters[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = get_bssid_list(s);
		break;
	case 3:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


struct bss_counters {
	const char *name;
	enum wlantest_bss_counter num;
};

static const struct bss_counters bss_counters[] = {
	{ "valid_bip_mmie", WLANTEST_BSS_COUNTER_VALID_BIP_MMIE },
	{ "invalid_bip_mmie", WLANTEST_BSS_COUNTER_INVALID_BIP_MMIE },
	{ "missing_bip_mmie", WLANTEST_BSS_COUNTER_MISSING_BIP_MMIE },
	{ "bip_deauth", WLANTEST_BSS_COUNTER_BIP_DEAUTH },
	{ "bip_disassoc", WLANTEST_BSS_COUNTER_BIP_DISASSOC },
	{ "probe_response", WLANTEST_BSS_COUNTER_PROBE_RESPONSE },
	{ NULL, 0 }
};

static int cmd_get_bss_counter(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	size_t len;

	if (argc != 2) {
		printf("get_bss_counter needs at two arguments: "
		       "counter name and BSSID\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_GET_BSS_COUNTER);
	pos += 4;

	for (i = 0; bss_counters[i].name; i++) {
		if (os_strcasecmp(bss_counters[i].name, argv[0]) == 0)
			break;
	}
	if (bss_counters[i].name == NULL) {
		printf("Unknown BSS counter '%s'\n", argv[0]);
		printf("Counters:");
		for (i = 0; bss_counters[i].name; i++)
			printf(" %s", bss_counters[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_BSS_COUNTER,
			    bss_counters[i].num);
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_COUNTER, &len);
	if (pos == NULL || len != 4)
		return -1;
	printf("%u\n", WPA_GET_BE32(pos));
	return 0;
}


static char ** complete_get_bss_counter(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;

	switch (arg) {
	case 1:
		/* counter list */
		count = ARRAY_SIZE(bss_counters);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			return NULL;
		for (i = 0; bss_counters[i].name; i++) {
			res[i] = os_strdup(bss_counters[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = get_bssid_list(s);
		break;
	}

	return res;
}


static int cmd_relog(int s, int argc, char *argv[])
{
	return cmd_simple(s, WLANTEST_CTRL_RELOG);
}


struct tdls_counters {
	const char *name;
	enum wlantest_tdls_counter num;
};

static const struct tdls_counters tdls_counters[] = {
	{ "valid_direct_link", WLANTEST_TDLS_COUNTER_VALID_DIRECT_LINK },
	{ "invalid_direct_link", WLANTEST_TDLS_COUNTER_INVALID_DIRECT_LINK },
	{ "valid_ap_path", WLANTEST_TDLS_COUNTER_VALID_AP_PATH },
	{ "invalid_ap_path", WLANTEST_TDLS_COUNTER_INVALID_AP_PATH },
	{ "setup_req", WLANTEST_TDLS_COUNTER_SETUP_REQ },
	{ "setup_resp_ok", WLANTEST_TDLS_COUNTER_SETUP_RESP_OK },
	{ "setup_resp_fail", WLANTEST_TDLS_COUNTER_SETUP_RESP_FAIL },
	{ "setup_conf_ok", WLANTEST_TDLS_COUNTER_SETUP_CONF_OK },
	{ "setup_conf_fail", WLANTEST_TDLS_COUNTER_SETUP_CONF_FAIL },
	{ "teardown", WLANTEST_TDLS_COUNTER_TEARDOWN },
	{ NULL, 0 }
};

static int cmd_get_tdls_counter(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	size_t len;

	if (argc != 4) {
		printf("get_tdls_counter needs four arguments: "
		       "counter name, BSSID, STA1 address, STA2 address\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_GET_TDLS_COUNTER);
	pos += 4;

	for (i = 0; tdls_counters[i].name; i++) {
		if (os_strcasecmp(tdls_counters[i].name, argv[0]) == 0)
			break;
	}
	if (tdls_counters[i].name == NULL) {
		printf("Unknown TDLS counter '%s'\n", argv[0]);
		printf("Counters:");
		for (i = 0; tdls_counters[i].name; i++)
			printf(" %s", tdls_counters[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_TDLS_COUNTER,
			    tdls_counters[i].num);
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[2], pos) < 0) {
		printf("Invalid STA1 address '%s'\n", argv[2]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA2_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[3], pos) < 0) {
		printf("Invalid STA2 address '%s'\n", argv[3]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_COUNTER, &len);
	if (pos == NULL || len != 4)
		return -1;
	printf("%u\n", WPA_GET_BE32(pos));
	return 0;
}


static char ** complete_get_tdls_counter(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		/* counter list */
		count = ARRAY_SIZE(tdls_counters);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			return NULL;
		for (i = 0; tdls_counters[i].name; i++) {
			res[i] = os_strdup(tdls_counters[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = get_bssid_list(s);
		break;
	case 3:
	case 4:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


struct inject_frames {
	const char *name;
	enum wlantest_inject_frame frame;
};

static const struct inject_frames inject_frames[] = {
	{ "auth", WLANTEST_FRAME_AUTH },
	{ "assocreq", WLANTEST_FRAME_ASSOCREQ },
	{ "reassocreq", WLANTEST_FRAME_REASSOCREQ },
	{ "deauth", WLANTEST_FRAME_DEAUTH },
	{ "disassoc", WLANTEST_FRAME_DISASSOC },
	{ "saqueryreq", WLANTEST_FRAME_SAQUERYREQ },
	{ NULL, 0 }
};

static int cmd_inject(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	enum wlantest_inject_protection prot;

	/* <frame> <prot> <sender> <BSSID> <STA/ff:ff:ff:ff:ff:ff> */

	if (argc < 5) {
		printf("inject needs five arguments: frame, protection, "
		       "sender, BSSID, STA/ff:ff:ff:ff:ff:ff\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_INJECT);
	pos += 4;

	for (i = 0; inject_frames[i].name; i++) {
		if (os_strcasecmp(inject_frames[i].name, argv[0]) == 0)
			break;
	}
	if (inject_frames[i].name == NULL) {
		printf("Unknown inject frame '%s'\n", argv[0]);
		printf("Frames:");
		for (i = 0; inject_frames[i].name; i++)
			printf(" %s", inject_frames[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_INJECT_FRAME,
			    inject_frames[i].frame);

	if (os_strcasecmp(argv[1], "normal") == 0)
		prot = WLANTEST_INJECT_NORMAL;
	else if (os_strcasecmp(argv[1], "protected") == 0)
		prot = WLANTEST_INJECT_PROTECTED;
	else if (os_strcasecmp(argv[1], "unprotected") == 0)
		prot = WLANTEST_INJECT_UNPROTECTED;
	else if (os_strcasecmp(argv[1], "incorrect") == 0)
		prot = WLANTEST_INJECT_INCORRECT_KEY;
	else {
		printf("Unknown protection type '%s'\n", argv[1]);
		printf("Protection types: normal protected unprotected "
		       "incorrect\n");
		return -1;
	}
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_INJECT_PROTECTION, prot);

	if (os_strcasecmp(argv[2], "ap") == 0) {
		pos = attr_add_be32(pos, end, WLANTEST_ATTR_INJECT_SENDER_AP,
				    1);
	} else if (os_strcasecmp(argv[2], "sta") == 0) {
		pos = attr_add_be32(pos, end, WLANTEST_ATTR_INJECT_SENDER_AP,
				    0);
	} else {
		printf("Unknown sender '%s'\n", argv[2]);
		printf("Sender types: ap sta\n");
		return -1;
	}

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[3], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[3]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[4], pos) < 0) {
		printf("Invalid STA '%s'\n", argv[4]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	printf("OK\n");
	return 0;
}


static char ** complete_inject(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		/* frame list */
		count = ARRAY_SIZE(inject_frames);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			break;
		for (i = 0; inject_frames[i].name; i++) {
			res[i] = os_strdup(inject_frames[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = os_calloc(5, sizeof(char *));
		if (res == NULL)
			break;
		res[0] = os_strdup("normal");
		if (res[0] == NULL)
			break;
		res[1] = os_strdup("protected");
		if (res[1] == NULL)
			break;
		res[2] = os_strdup("unprotected");
		if (res[2] == NULL)
			break;
		res[3] = os_strdup("incorrect");
		if (res[3] == NULL)
			break;
		break;
	case 3:
		res = os_calloc(3, sizeof(char *));
		if (res == NULL)
			break;
		res[0] = os_strdup("ap");
		if (res[0] == NULL)
			break;
		res[1] = os_strdup("sta");
		if (res[1] == NULL)
			break;
		break;
	case 4:
		res = get_bssid_list(s);
		break;
	case 5:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 1);
		break;
	}

	return res;
}


static u8 * add_hex(u8 *pos, u8 *end, const char *str)
{
	const char *s;
	int val;

	s = str;
	while (*s) {
		while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' ||
		       *s == ':')
			s++;
		if (*s == '\0')
			break;
		if (*s == '#') {
			while (*s != '\0' && *s != '\r' && *s != '\n')
				s++;
			continue;
		}

		val = hex2byte(s);
		if (val < 0) {
			printf("Invalid hex encoding '%s'\n", s);
			return NULL;
		}
		if (pos == end) {
			printf("Too long frame\n");
			return NULL;
		}
		*pos++ = val;
		s += 2;
	}

	return pos;
}


static int cmd_send(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[WLANTEST_CTRL_MAX_CMD_LEN], *end, *pos, *len_pos;
	int rlen;
	enum wlantest_inject_protection prot;
	int arg;

	/* <prot> <raw frame as hex dump> */

	if (argc < 2) {
		printf("send needs two arguments: protected/unprotected, "
		       "raw frame as hex dump\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SEND);
	pos += 4;

	if (os_strcasecmp(argv[0], "normal") == 0)
		prot = WLANTEST_INJECT_NORMAL;
	else if (os_strcasecmp(argv[0], "protected") == 0)
		prot = WLANTEST_INJECT_PROTECTED;
	else if (os_strcasecmp(argv[0], "unprotected") == 0)
		prot = WLANTEST_INJECT_UNPROTECTED;
	else if (os_strcasecmp(argv[0], "incorrect") == 0)
		prot = WLANTEST_INJECT_INCORRECT_KEY;
	else {
		printf("Unknown protection type '%s'\n", argv[1]);
		printf("Protection types: normal protected unprotected "
		       "incorrect\n");
		return -1;
	}
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_INJECT_PROTECTION, prot);

	WPA_PUT_BE32(pos, WLANTEST_ATTR_FRAME);
	pos += 4;
	len_pos = pos;
	pos += 4;

	for (arg = 1; pos && arg < argc; arg++)
		pos = add_hex(pos, end, argv[arg]);
	if (pos == NULL)
		return -1;

	WPA_PUT_BE32(len_pos, pos - len_pos - 4);

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	printf("OK\n");
	return 0;
}


static char ** complete_send(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;

	switch (arg) {
	case 1:
		res = os_calloc(5, sizeof(char *));
		if (res == NULL)
			break;
		res[0] = os_strdup("normal");
		if (res[0] == NULL)
			break;
		res[1] = os_strdup("protected");
		if (res[1] == NULL)
			break;
		res[2] = os_strdup("unprotected");
		if (res[2] == NULL)
			break;
		res[3] = os_strdup("incorrect");
		if (res[3] == NULL)
			break;
		break;
	}

	return res;
}


static int cmd_version(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[4];
	char *version;
	size_t len;
	int rlen, i;

	WPA_PUT_BE32(buf, WLANTEST_CTRL_VERSION);
	rlen = cmd_send_and_recv(s, buf, sizeof(buf), resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	version = (char *) attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_VERSION,
				    &len);
	if (version == NULL)
		return -1;

	for (i = 0; i < len; i++)
		putchar(version[i]);
	printf("\n");

	return 0;
}


static int cmd_add_passphrase(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos, *end;
	size_t len;
	int rlen;

	if (argc < 1) {
		printf("add_passphrase needs one argument: passphrase\n");
		return -1;
	}

	len = os_strlen(argv[0]);
	if (len < 8 || len > 63) {
		printf("Invalid passphrase '%s'\n", argv[0]);
		return -1;
	}
	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_ADD_PASSPHRASE);
	pos += 4;
	pos = attr_add_str(pos, end, WLANTEST_ATTR_PASSPHRASE,
			   argv[0]);
	if (argc > 1) {
		pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
		if (hwaddr_aton(argv[1], pos) < 0) {
			printf("Invalid BSSID '%s'\n", argv[3]);
			return -1;
		}
		pos += ETH_ALEN;
	}

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	return 0;
}


static int cmd_add_wepkey(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *pos, *end;
	int rlen;

	if (argc < 1) {
		printf("add_wepkey needs one argument: WEP key\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_ADD_PASSPHRASE);
	pos += 4;
	pos = attr_add_str(pos, end, WLANTEST_ATTR_WEPKEY, argv[0]);

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;
	return 0;
}


struct sta_infos {
	const char *name;
	enum wlantest_sta_info num;
};

static const struct sta_infos sta_infos[] = {
	{ "proto", WLANTEST_STA_INFO_PROTO },
	{ "pairwise", WLANTEST_STA_INFO_PAIRWISE },
	{ "key_mgmt", WLANTEST_STA_INFO_KEY_MGMT },
	{ "rsn_capab", WLANTEST_STA_INFO_RSN_CAPAB },
	{ "state", WLANTEST_STA_INFO_STATE },
	{ "gtk", WLANTEST_STA_INFO_GTK },
	{ NULL, 0 }
};

static int cmd_info_sta(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	size_t len;
	char info[100];

	if (argc != 3) {
		printf("sta_info needs at three arguments: "
		       "counter name, BSSID, and STA address\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_INFO_STA);
	pos += 4;

	for (i = 0; sta_infos[i].name; i++) {
		if (os_strcasecmp(sta_infos[i].name, argv[0]) == 0)
			break;
	}
	if (sta_infos[i].name == NULL) {
		printf("Unknown STA info '%s'\n", argv[0]);
		printf("Info fields:");
		for (i = 0; sta_infos[i].name; i++)
			printf(" %s", sta_infos[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_STA_INFO,
			    sta_infos[i].num);
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[2], pos) < 0) {
		printf("Invalid STA address '%s'\n", argv[2]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_INFO, &len);
	if (pos == NULL)
		return -1;
	if (len >= sizeof(info))
		len = sizeof(info) - 1;
	os_memcpy(info, pos, len);
	info[len] = '\0';
	printf("%s\n", info);
	return 0;
}


static char ** complete_info_sta(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		/* counter list */
		count = ARRAY_SIZE(sta_infos);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			return NULL;
		for (i = 0; sta_infos[i].name; i++) {
			res[i] = os_strdup(sta_infos[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = get_bssid_list(s);
		break;
	case 3:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


struct bss_infos {
	const char *name;
	enum wlantest_bss_info num;
};

static const struct bss_infos bss_infos[] = {
	{ "proto", WLANTEST_BSS_INFO_PROTO },
	{ "pairwise", WLANTEST_BSS_INFO_PAIRWISE },
	{ "group", WLANTEST_BSS_INFO_GROUP },
	{ "group_mgmt", WLANTEST_BSS_INFO_GROUP_MGMT },
	{ "key_mgmt", WLANTEST_BSS_INFO_KEY_MGMT },
	{ "rsn_capab", WLANTEST_BSS_INFO_RSN_CAPAB },
	{ NULL, 0 }
};

static int cmd_info_bss(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen, i;
	size_t len;
	char info[100];

	if (argc != 2) {
		printf("bss_info needs at two arguments: "
		       "field name and BSSID\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_INFO_BSS);
	pos += 4;

	for (i = 0; bss_infos[i].name; i++) {
		if (os_strcasecmp(bss_infos[i].name, argv[0]) == 0)
			break;
	}
	if (bss_infos[i].name == NULL) {
		printf("Unknown BSS info '%s'\n", argv[0]);
		printf("Info fields:");
		for (i = 0; bss_infos[i].name; i++)
			printf(" %s", bss_infos[i].name);
		printf("\n");
		return -1;
	}

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_BSS_INFO,
			    bss_infos[i].num);
	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_INFO, &len);
	if (pos == NULL)
		return -1;
	if (len >= sizeof(info))
		len = sizeof(info) - 1;
	os_memcpy(info, pos, len);
	info[len] = '\0';
	printf("%s\n", info);
	return 0;
}


static char ** complete_info_bss(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	int i, count;

	switch (arg) {
	case 1:
		/* counter list */
		count = ARRAY_SIZE(bss_infos);
		res = os_calloc(count, sizeof(char *));
		if (res == NULL)
			return NULL;
		for (i = 0; bss_infos[i].name; i++) {
			res[i] = os_strdup(bss_infos[i].name);
			if (res[i] == NULL)
				break;
		}
		break;
	case 2:
		res = get_bssid_list(s);
		break;
	}

	return res;
}


static int cmd_get_tx_tid(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen;
	size_t len;

	if (argc != 3) {
		printf("get_tx_tid needs three arguments: "
		       "BSSID, STA address, and TID\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_GET_TX_TID);
	pos += 4;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid STA address '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_TID, atoi(argv[2]));

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_COUNTER, &len);
	if (pos == NULL || len != 4)
		return -1;
	printf("%u\n", WPA_GET_BE32(pos));
	return 0;
}


static int cmd_get_rx_tid(int s, int argc, char *argv[])
{
	u8 resp[WLANTEST_CTRL_MAX_RESP_LEN];
	u8 buf[100], *end, *pos;
	int rlen;
	size_t len;

	if (argc != 3) {
		printf("get_tx_tid needs three arguments: "
		       "BSSID, STA address, and TID\n");
		return -1;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_GET_RX_TID);
	pos += 4;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_BSSID, ETH_ALEN);
	if (hwaddr_aton(argv[0], pos) < 0) {
		printf("Invalid BSSID '%s'\n", argv[0]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_hdr_add(pos, end, WLANTEST_ATTR_STA_ADDR, ETH_ALEN);
	if (hwaddr_aton(argv[1], pos) < 0) {
		printf("Invalid STA address '%s'\n", argv[1]);
		return -1;
	}
	pos += ETH_ALEN;

	pos = attr_add_be32(pos, end, WLANTEST_ATTR_TID, atoi(argv[2]));

	rlen = cmd_send_and_recv(s, buf, pos - buf, resp, sizeof(resp));
	if (rlen < 0)
		return -1;

	pos = attr_get(resp + 4, rlen - 4, WLANTEST_ATTR_COUNTER, &len);
	if (pos == NULL || len != 4)
		return -1;
	printf("%u\n", WPA_GET_BE32(pos));
	return 0;
}


static char ** complete_get_tid(int s, const char *str, int pos)
{
	int arg = get_cmd_arg_num(str, pos);
	char **res = NULL;
	u8 addr[ETH_ALEN];

	switch (arg) {
	case 1:
		res = get_bssid_list(s);
		break;
	case 2:
		if (hwaddr_aton(&str[get_prev_arg_pos(str, pos)], addr) < 0)
			break;
		res = get_sta_list(s, addr, 0);
		break;
	}

	return res;
}


struct wlantest_cli_cmd {
	const char *cmd;
	int (*handler)(int s, int argc, char *argv[]);
	const char *usage;
	char ** (*complete)(int s, const char *str, int pos);
};

static const struct wlantest_cli_cmd wlantest_cli_commands[] = {
	{ "ping", cmd_ping, "= test connection to wlantest", NULL },
	{ "terminate", cmd_terminate, "= terminate wlantest", NULL },
	{ "list_bss", cmd_list_bss, "= get BSS list", NULL },
	{ "list_sta", cmd_list_sta, "<BSSID> = get STA list",
	  complete_list_sta },
	{ "flush", cmd_flush, "= drop all collected BSS data", NULL },
	{ "clear_sta_counters", cmd_clear_sta_counters,
	  "<BSSID> <STA> = clear STA counters", complete_clear_sta_counters },
	{ "clear_bss_counters", cmd_clear_bss_counters,
	  "<BSSID> = clear BSS counters", complete_clear_bss_counters },
	{ "get_sta_counter", cmd_get_sta_counter,
	  "<counter> <BSSID> <STA> = get STA counter value",
	  complete_get_sta_counter },
	{ "get_bss_counter", cmd_get_bss_counter,
	  "<counter> <BSSID> = get BSS counter value",
	  complete_get_bss_counter },
	{ "inject", cmd_inject,
	  "<frame> <prot> <sender> <BSSID> <STA/ff:ff:ff:ff:ff:ff>",
	  complete_inject },
	{ "send", cmd_send,
	  "<prot> <raw frame as hex dump>",
	  complete_send },
	{ "version", cmd_version, "= get wlantest version", NULL },
	{ "add_passphrase", cmd_add_passphrase,
	  "<passphrase> = add a known passphrase", NULL },
	{ "add_wepkey", cmd_add_wepkey,
	  "<WEP key> = add a known WEP key", NULL },
	{ "info_sta", cmd_info_sta,
	  "<field> <BSSID> <STA> = get STA information",
	  complete_info_sta },
	{ "info_bss", cmd_info_bss,
	  "<field> <BSSID> = get BSS information",
	  complete_info_bss },
	{ "clear_tdls_counters", cmd_clear_tdls_counters,
	  "<BSSID> <STA1> <STA2> = clear TDLS counters",
	  complete_clear_tdls_counters },
	{ "get_tdls_counter", cmd_get_tdls_counter,
	  "<counter> <BSSID> <STA1> <STA2> = get TDLS counter value",
	  complete_get_tdls_counter },
	{ "get_bss_counter", cmd_get_bss_counter,
	  "<counter> <BSSID> = get BSS counter value",
	  complete_get_bss_counter },
	{ "relog", cmd_relog, "= re-open log-file (allow rolling logs)", NULL },
	{ "get_tx_tid", cmd_get_tx_tid,
	  "<BSSID> <STA> <TID> = get STA TX TID counter value",
	  complete_get_tid },
	{ "get_rx_tid", cmd_get_rx_tid,
	  "<BSSID> <STA> <TID> = get STA RX TID counter value",
	  complete_get_tid },
	{ NULL, NULL, NULL, NULL }
};


static int ctrl_command(int s, int argc, char *argv[])
{
	const struct wlantest_cli_cmd *cmd, *match = NULL;
	int count = 0;
	int ret = 0;

	for (cmd = wlantest_cli_commands; cmd->cmd; cmd++) {
		if (os_strncasecmp(cmd->cmd, argv[0], os_strlen(argv[0])) == 0)
		{
			match = cmd;
			if (os_strcasecmp(cmd->cmd, argv[0]) == 0) {
				/* exact match */
				count = 1;
				break;
			}
			count++;
		}
	}

	if (count > 1) {
		printf("Ambiguous command '%s'; possible commands:", argv[0]);
		for (cmd = wlantest_cli_commands; cmd->cmd; cmd++) {
			if (os_strncasecmp(cmd->cmd, argv[0],
					   os_strlen(argv[0])) == 0) {
				printf(" %s", cmd->cmd);
			}
		}
		printf("\n");
		ret = 1;
	} else if (count == 0) {
		printf("Unknown command '%s'\n", argv[0]);
		ret = 1;
	} else {
		ret = match->handler(s, argc - 1, &argv[1]);
	}

	return ret;
}


struct wlantest_cli {
	int s;
};


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


static void wlantest_cli_edit_cmd_cb(void *ctx, char *cmd)
{
	struct wlantest_cli *cli = ctx;
	char *argv[max_args];
	int argc;
	argc = tokenize_cmd(cmd, argv);
	if (argc) {
		int ret = ctrl_command(cli->s, argc, argv);
		if (ret < 0)
			printf("FAIL\n");
	}
}


static void wlantest_cli_eloop_terminate(int sig, void *signal_ctx)
{
	eloop_terminate();
}


static void wlantest_cli_edit_eof_cb(void *ctx)
{
	eloop_terminate();
}


static char ** wlantest_cli_cmd_list(void)
{
	char **res;
	int i;

	res = os_calloc(ARRAY_SIZE(wlantest_cli_commands), sizeof(char *));
	if (res == NULL)
		return NULL;

	for (i = 0; wlantest_cli_commands[i].cmd; i++) {
		res[i] = os_strdup(wlantest_cli_commands[i].cmd);
		if (res[i] == NULL)
			break;
	}

	return res;
}


static char ** wlantest_cli_cmd_completion(struct wlantest_cli *cli,
					   const char *cmd, const char *str,
					   int pos)
{
	int i;

	for (i = 0; wlantest_cli_commands[i].cmd; i++) {
		const struct wlantest_cli_cmd *c = &wlantest_cli_commands[i];
		if (os_strcasecmp(c->cmd, cmd) == 0) {
			edit_clear_line();
			printf("\r%s\n", c->usage);
			edit_redraw();
			if (c->complete)
				return c->complete(cli->s, str, pos);
			break;
		}
	}

	return NULL;
}


static char ** wlantest_cli_edit_completion_cb(void *ctx, const char *str,
					       int pos)
{
	struct wlantest_cli *cli = ctx;
	char **res;
	const char *end;
	char *cmd;

	end = os_strchr(str, ' ');
	if (end == NULL || str + pos < end)
		return wlantest_cli_cmd_list();

	cmd = os_malloc(pos + 1);
	if (cmd == NULL)
		return NULL;
	os_memcpy(cmd, str, pos);
	cmd[end - str] = '\0';
	res = wlantest_cli_cmd_completion(cli, cmd, str, pos);
	os_free(cmd);
	return res;
}


static void wlantest_cli_interactive(int s)
{
	struct wlantest_cli cli;
	char *home, *hfile = NULL;

	if (eloop_init())
		return;

	home = getenv("HOME");
	if (home) {
		const char *fname = ".wlantest_cli_history";
		int hfile_len = os_strlen(home) + 1 + os_strlen(fname) + 1;
		hfile = os_malloc(hfile_len);
		if (hfile)
			os_snprintf(hfile, hfile_len, "%s/%s", home, fname);
	}

	cli.s = s;
	eloop_register_signal_terminate(wlantest_cli_eloop_terminate, &cli);
	edit_init(wlantest_cli_edit_cmd_cb, wlantest_cli_edit_eof_cb,
		  wlantest_cli_edit_completion_cb, &cli, hfile, NULL);

	eloop_run();

	edit_deinit(hfile, NULL);
	os_free(hfile);
	eloop_destroy();
}


int main(int argc, char *argv[])
{
	int s;
	struct sockaddr_un addr;
	int ret = 0;

	if (os_program_init())
		return -1;

	s = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path + 1, WLANTEST_SOCK_NAME,
		   sizeof(addr.sun_path) - 1);
	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("connect");
		close(s);
		return -1;
	}

	if (argc > 1) {
		ret = ctrl_command(s, argc - 1, &argv[1]);
		if (ret < 0)
			printf("FAIL\n");
	} else {
		wlantest_cli_interactive(s);
	}

	close(s);

	os_program_deinit();

	return ret;
}
