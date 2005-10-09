/*
 * WPA Supplicant - test code for pre-authentication
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * IEEE 802.1X Supplicant test code (to be used in place of wpa_supplicant.c.
 * Not used in production version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>

#include "common.h"
#include "config.h"
#include "eapol_sm.h"
#include "eloop.h"
#include "wpa.h"
#include "eap.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "l2_packet.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"


extern int wpa_debug_level;
extern int wpa_debug_show_keys;

void wpa_msg(struct wpa_supplicant *wpa_s, int level, char *fmt, ...)
{
	va_list ap;
	char *buf;
	const int buflen = 2048;
	int len;

	buf = malloc(buflen);
	if (buf == NULL) {
		printf("Failed to allocate message buffer for:\n");
		va_start(ap, fmt);
		vprintf(fmt, ap);
		printf("\n");
		va_end(ap);
		return;
	}
	va_start(ap, fmt);
	len = vsnprintf(buf, buflen, fmt, ap);
	va_end(ap);
	wpa_printf(level, "%s", buf);
	wpa_supplicant_ctrl_iface_send(wpa_s, level, buf, len);
	free(buf);
}


void wpa_supplicant_event(struct wpa_supplicant *wpa_s, wpa_event_type event,
			  union wpa_event_data *data)
{
}


int rsn_preauth_init(struct wpa_supplicant *wpa_s, u8 *dst)
{
	return -1;
}


void rsn_preauth_deinit(struct wpa_supplicant *wpa_s)
{
}


int pmksa_cache_list(struct wpa_supplicant *wpa_s, char *buf, size_t len)
{
	return 0;
}


int wpa_get_mib(struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	return 0;
}


void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec)
{
}


const char * wpa_ssid_txt(u8 *ssid, size_t ssid_len)
{
	return NULL;
}


int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s)
{
	return -1;
}


static int eapol_test_eapol_send(void *ctx, int type, u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	u8 *msg;
	size_t msglen;
	struct l2_ethhdr *ethhdr;
	struct ieee802_1x_hdr *hdr;
	int res;

	printf("WPA: wpa_eapol_send(type=%d len=%d)\n", type, len);

	if (wpa_s->l2_preauth == NULL)
		return -1;

	msglen = sizeof(*ethhdr) + sizeof(*hdr) + len;
	msg = malloc(msglen);
	if (msg == NULL)
		return -1;

	ethhdr = (struct l2_ethhdr *) msg;
	memcpy(ethhdr->h_dest, wpa_s->preauth_bssid, ETH_ALEN);
	memcpy(ethhdr->h_source, wpa_s->own_addr, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_RSN_PREAUTH);

	hdr = (struct ieee802_1x_hdr *) (ethhdr + 1);
	hdr->version = wpa_s->conf->eapol_version;
	hdr->type = type;
	hdr->length = htons(len);

	memcpy((u8 *) (hdr + 1), buf, len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL (preauth)", msg, msglen);
	res = l2_packet_send(wpa_s->l2_preauth, msg, msglen);
	free(msg);
	return res;
}


static void eapol_test_eapol_done_cb(void *ctx)
{
	printf("WPA: EAPOL processing complete\n");
}


static void eapol_sm_cb(struct eapol_sm *eapol, int success, void *ctx)
{
	printf("eapol_sm_cb: success=%d\n", success);
	eloop_terminate();
}


static int test_eapol(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	struct eapol_config eapol_conf;
	struct eapol_ctx *ctx;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		printf("Failed to allocate EAPOL context.\n");
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx = wpa_s;
	ctx->msg_ctx = wpa_s;
	ctx->scard_ctx = wpa_s->scard;
	ctx->cb = eapol_sm_cb;
	ctx->cb_ctx = wpa_s;
	ctx->preauth = 0;
	ctx->eapol_done_cb = eapol_test_eapol_done_cb;
	ctx->eapol_send = eapol_test_eapol_send;

	wpa_s->preauth_eapol = eapol_sm_init(ctx);
	if (wpa_s->preauth_eapol == NULL) {
		free(ctx);
		printf("Failed to initialize EAPOL state machines.\n");
		return -1;
	}

	wpa_s->current_ssid = ssid;
	memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.accept_802_1x_keys = 1;
	eapol_conf.required_keys = 0;
	eapol_conf.workaround = ssid->eap_workaround;
	eapol_sm_notify_config(wpa_s->preauth_eapol, ssid, &eapol_conf);


	eapol_sm_notify_portValid(wpa_s->preauth_eapol, FALSE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(wpa_s->preauth_eapol, TRUE);

	return 0;
}


static void test_eapol_clean(struct wpa_supplicant *wpa_s)
{
	l2_packet_deinit(wpa_s->l2_preauth);
	eapol_sm_deinit(wpa_s->preauth_eapol);
	wpa_s->preauth_eapol = NULL;
	scard_deinit(wpa_s->scard);
	wpa_supplicant_ctrl_iface_deinit(wpa_s);
	wpa_config_free(wpa_s->conf);
}


static void eapol_test_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	printf("EAPOL test timed out\n");
	wpa_s->auth_timed_out = 1;
	eloop_terminate();
}


static void wpa_supplicant_imsi_identity(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid)
{
	if (ssid->identity == NULL && wpa_s->imsi) {
		ssid->identity = malloc(1 + wpa_s->imsi_len);
		if (ssid->identity) {
			ssid->identity[0] = '1';
			memcpy(ssid->identity + 1, wpa_s->imsi,
			       wpa_s->imsi_len);
			ssid->identity_len = 1 + wpa_s->imsi_len;
			wpa_hexdump_ascii(MSG_DEBUG, "permanent identity from "
					  "IMSI", ssid->identity,
					  ssid->identity_len);
		}
	}
}


static void wpa_supplicant_scard_init(struct wpa_supplicant *wpa_s,
				      struct wpa_ssid *ssid)
{
	char buf[100];
	size_t len;

	if (ssid->pcsc == NULL)
		return;
	if (wpa_s->scard != NULL) {
		wpa_supplicant_imsi_identity(wpa_s, ssid);
		return;
	}
	wpa_printf(MSG_DEBUG, "Selected network is configured to use SIM - "
		   "initialize PCSC");
	wpa_s->scard = scard_init(SCARD_TRY_BOTH, ssid->pin);
	if (wpa_s->scard == NULL) {
		wpa_printf(MSG_WARNING, "Failed to initialize SIM "
			   "(pcsc-lite)");
		/* TODO: what to do here? */
		return;
	}
	eapol_sm_register_scard_ctx(wpa_s->preauth_eapol, wpa_s->scard);

	len = sizeof(buf);
	if (scard_get_imsi(wpa_s->scard, buf, &len)) {
		wpa_printf(MSG_WARNING, "Failed to get IMSI from SIM");
		/* TODO: what to do here? */
		return;
	}

	wpa_hexdump(MSG_DEBUG, "IMSI", (u8 *) buf, len);
	free(wpa_s->imsi);
	wpa_s->imsi = malloc(len);
	if (wpa_s->imsi) {
		memcpy(wpa_s->imsi, buf, len);
		wpa_s->imsi_len = len;
		wpa_supplicant_imsi_identity(wpa_s, ssid);
	}
}


static void rsn_preauth_receive(void *ctx, unsigned char *src_addr,
				unsigned char *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "RX pre-auth from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX pre-auth", buf, len);

	if (wpa_s->preauth_eapol == NULL ||
	    memcmp(wpa_s->preauth_bssid, "\x00\x00\x00\x00\x00\x00",
		   ETH_ALEN) == 0 ||
	    memcmp(wpa_s->preauth_bssid, src_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_WARNING, "RSN pre-auth frame received from "
			   "unexpected source " MACSTR " - dropped",
			   MAC2STR(src_addr));
		return;
	}

	eapol_sm_rx_eapol(wpa_s->preauth_eapol, src_addr, buf, len);
}


static void wpa_init_conf(struct wpa_supplicant *wpa_s, const char *target,
			  const char *ifname)
{
	strncpy(wpa_s->ifname, ifname, sizeof(wpa_s->ifname));

	if (hwaddr_aton(target, wpa_s->preauth_bssid)) {
		printf("Failed to parse target address '%s'.\n", target);
		exit(-1);
	}

	wpa_s->l2_preauth = l2_packet_init(wpa_s->ifname, NULL,
					   ETH_P_RSN_PREAUTH,
					   rsn_preauth_receive, wpa_s);
	if (wpa_s->l2_preauth == NULL) {
		wpa_printf(MSG_WARNING, "RSN: Failed to initialize L2 packet "
			   "processing for pre-authentication");
		exit(-1);
	}

	if (l2_packet_get_own_addr(wpa_s->l2_preauth, wpa_s->own_addr)) {
		wpa_printf(MSG_WARNING, "Failed to get own L2 address\n");
		exit(-1);
	}
}


static void eapol_test_terminate(int sig, void *eloop_ctx,
				 void *signal_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_msg(wpa_s, MSG_INFO, "Signal %d received - terminating", sig);
	eloop_terminate();
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant wpa_s;
	int ret = 1;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (argc != 4) {
		printf("usage: eapol_test <conf> <target MAC address> "
		       "<ifname>\n");
		return -1;
	}

	eloop_init(&wpa_s);

	memset(&wpa_s, 0, sizeof(wpa_s));
	wpa_s.conf = wpa_config_read(argv[1]);
	if (wpa_s.conf == NULL) {
		printf("Failed to parse configuration file '%s'.\n", argv[1]);
		return -1;
	}
	if (wpa_s.conf->ssid == NULL) {
		printf("No networks defined.\n");
		return -1;
	}

	wpa_init_conf(&wpa_s, argv[2], argv[3]);
	if (wpa_supplicant_ctrl_iface_init(&wpa_s)) {
		printf("Failed to initialize control interface '%s'.\n"
		       "You may have another eapol_test process already "
		       "running or the file was\n"
		       "left by an unclean termination of eapol_test in "
		       "which case you will need\n"
		       "to manually remove this file before starting "
		       "eapol_test again.\n",
		       wpa_s.conf->ctrl_interface);
		return -1;
	}
	wpa_supplicant_scard_init(&wpa_s, wpa_s.conf->ssid);

	if (test_eapol(&wpa_s, wpa_s.conf->ssid))
		return -1;

	eloop_register_timeout(30, 0, eapol_test_timeout, &wpa_s, NULL);
	eloop_register_signal(SIGINT, eapol_test_terminate, NULL);
	eloop_register_signal(SIGTERM, eapol_test_terminate, NULL);
	eloop_register_signal(SIGHUP, eapol_test_terminate, NULL);
	eloop_run();

	if (wpa_s.auth_timed_out)
		ret = -2;
	else
		ret = 0;

	test_eapol_clean(&wpa_s);

	eloop_destroy();

	return ret;
}
