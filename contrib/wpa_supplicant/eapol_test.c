/*
 * WPA Supplicant - test code
 * Copyright (c) 2003-2006, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * IEEE 802.1X Supplicant test code (to be used in place of wpa_supplicant.c.
 * Not used in production version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#ifndef CONFIG_NATIVE_WINDOWS
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* CONFIG_NATIVE_WINDOWS */
#include <assert.h>

#include "common.h"
#include "config.h"
#include "eapol_sm.h"
#include "eloop.h"
#include "wpa.h"
#include "eap_i.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "radius.h"
#include "radius_client.h"
#include "l2_packet.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"


extern int wpa_debug_level;
extern int wpa_debug_show_keys;

struct wpa_driver_ops *wpa_supplicant_drivers[] = { };


struct eapol_test_data {
	struct wpa_supplicant *wpa_s;

	int eapol_test_num_reauths;
	int no_mppe_keys;
	int num_mppe_ok, num_mppe_mismatch;

	u8 radius_identifier;
	struct radius_msg *last_recv_radius;
	struct in_addr own_ip_addr;
	struct radius_client_data *radius;
	struct hostapd_radius_servers *radius_conf;

	u8 *last_eap_radius; /* last received EAP Response from Authentication
			      * Server */
	size_t last_eap_radius_len;

	u8 authenticator_pmk[PMK_LEN];
	size_t authenticator_pmk_len;
	int radius_access_accept_received;
	int radius_access_reject_received;
	int auth_timed_out;

	u8 *eap_identity;
	size_t eap_identity_len;
};

static struct eapol_test_data eapol_test;


static void send_eap_request_identity(void *eloop_ctx, void *timeout_ctx);


void hostapd_logger(void *ctx, u8 *addr, unsigned int module, int level,
		    char *fmt, ...)
{
	char *format;
	int maxlen;
	va_list ap;

	maxlen = strlen(fmt) + 100;
	format = malloc(maxlen);
	if (!format)
		return;

	va_start(ap, fmt);


	if (addr)
		snprintf(format, maxlen, "STA " MACSTR ": %s",
			 MAC2STR(addr), fmt);
	else
		snprintf(format, maxlen, "%s", fmt);

	vprintf(format, ap);
	printf("\n");

	free(format);

	va_end(ap);
}


const char * hostapd_ip_txt(const struct hostapd_ip_addr *addr, char *buf,
			    size_t buflen)
{
	if (buflen == 0 || addr == NULL)
		return NULL;

	if (addr->af == AF_INET) {
		snprintf(buf, buflen, "%s", inet_ntoa(addr->u.v4));
	} else {
		buf[0] = '\0';
	}
#ifdef CONFIG_IPV6
	if (addr->af == AF_INET6) {
		if (inet_ntop(AF_INET6, &addr->u.v6, buf, buflen) == NULL)
			buf[0] = '\0';
	}
#endif /* CONFIG_IPV6 */

	return buf;
}


static void ieee802_1x_encapsulate_radius(struct eapol_test_data *e,
					  const u8 *eap, size_t len)
{
	struct radius_msg *msg;
	char buf[128];
	const struct eap_hdr *hdr;
	const u8 *pos;

	wpa_printf(MSG_DEBUG, "Encapsulating EAP message into a RADIUS "
		   "packet");

	e->radius_identifier = radius_client_get_id(e->radius);
	msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST,
			     e->radius_identifier);
	if (msg == NULL) {
		printf("Could not create net RADIUS packet\n");
		return;
	}

	radius_msg_make_authenticator(msg, (u8 *) e, sizeof(*e));

	hdr = (const struct eap_hdr *) eap;
	pos = (const u8 *) (hdr + 1);
	if (len > sizeof(*hdr) && hdr->code == EAP_CODE_RESPONSE &&
	    pos[0] == EAP_TYPE_IDENTITY) {
		pos++;
		free(e->eap_identity);
		e->eap_identity_len = len - sizeof(*hdr) - 1;
		e->eap_identity = malloc(e->eap_identity_len);
		if (e->eap_identity) {
			memcpy(e->eap_identity, pos, e->eap_identity_len);
			wpa_hexdump(MSG_DEBUG, "Learned identity from "
				    "EAP-Response-Identity",
				    e->eap_identity, e->eap_identity_len);
		}
	}

	if (e->eap_identity &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME,
				 e->eap_identity, e->eap_identity_len)) {
		printf("Could not add User-Name\n");
		goto fail;
	}

	if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				 (u8 *) &e->own_ip_addr, 4)) {
		printf("Could not add NAS-IP-Address\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
		 MAC2STR(e->wpa_s->own_addr));
	if (!radius_msg_add_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
				 (u8 *) buf, strlen(buf))) {
		printf("Could not add Calling-Station-Id\n");
		goto fail;
	}

	/* TODO: should probably check MTU from driver config; 2304 is max for
	 * IEEE 802.11, but use 1400 to avoid problems with too large packets
	 */
	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_FRAMED_MTU, 1400)) {
		printf("Could not add Framed-MTU\n");
		goto fail;
	}

	if (!radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
				       RADIUS_NAS_PORT_TYPE_IEEE_802_11)) {
		printf("Could not add NAS-Port-Type\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), "CONNECT 11Mbps 802.11b");
	if (!radius_msg_add_attr(msg, RADIUS_ATTR_CONNECT_INFO,
				 (u8 *) buf, strlen(buf))) {
		printf("Could not add Connect-Info\n");
		goto fail;
	}

	if (eap && !radius_msg_add_eap(msg, eap, len)) {
		printf("Could not add EAP-Message\n");
		goto fail;
	}

	/* State attribute must be copied if and only if this packet is
	 * Access-Request reply to the previous Access-Challenge */
	if (e->last_recv_radius && e->last_recv_radius->hdr->code ==
	    RADIUS_CODE_ACCESS_CHALLENGE) {
		int res = radius_msg_copy_attr(msg, e->last_recv_radius,
					       RADIUS_ATTR_STATE);
		if (res < 0) {
			printf("Could not copy State attribute from previous "
			       "Access-Challenge\n");
			goto fail;
		}
		if (res > 0) {
			wpa_printf(MSG_DEBUG, "  Copied RADIUS State "
				   "Attribute");
		}
	}

	radius_client_send(e->radius, msg, RADIUS_AUTH, e->wpa_s->own_addr);
	return;

 fail:
	radius_msg_free(msg);
	free(msg);
}


static int eapol_test_eapol_send(void *ctx, int type, const u8 *buf,
				 size_t len)
{
	/* struct wpa_supplicant *wpa_s = ctx; */
	printf("WPA: eapol_test_eapol_send(type=%d len=%d)\n", type, len);
	if (type == IEEE802_1X_TYPE_EAP_PACKET) {
		wpa_hexdump(MSG_DEBUG, "TX EAP -> RADIUS", buf, len);
		ieee802_1x_encapsulate_radius(&eapol_test, buf, len);
	}
	return 0;
}


static void eapol_test_set_config_blob(void *ctx,
				       struct wpa_config_blob *blob)
{
	struct wpa_supplicant *wpa_s = ctx;
	wpa_config_set_blob(wpa_s->conf, blob);
}


static const struct wpa_config_blob *
eapol_test_get_config_blob(void *ctx, const char *name)
{
	struct wpa_supplicant *wpa_s = ctx;
	return wpa_config_get_blob(wpa_s->conf, name);
}


static void eapol_test_eapol_done_cb(void *ctx)
{
	printf("WPA: EAPOL processing complete\n");
}


static void eapol_sm_reauth(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_test_data *e = eloop_ctx;
	printf("\n\n\n\n\neapol_test: Triggering EAP reauthentication\n\n");
	e->radius_access_accept_received = 0;
	send_eap_request_identity(e->wpa_s, NULL);
}


static int eapol_test_compare_pmk(struct eapol_test_data *e)
{
	u8 pmk[PMK_LEN];
	int ret = 1;

	if (eapol_sm_get_key(e->wpa_s->eapol, pmk, PMK_LEN) == 0) {
		wpa_hexdump(MSG_DEBUG, "PMK from EAPOL", pmk, PMK_LEN);
		if (memcmp(pmk, e->authenticator_pmk, PMK_LEN) != 0)
			printf("WARNING: PMK mismatch\n");
		else if (e->radius_access_accept_received)
			ret = 0;
	} else if (e->authenticator_pmk_len == 16 &&
		   eapol_sm_get_key(e->wpa_s->eapol, pmk, 16) == 0) {
		wpa_hexdump(MSG_DEBUG, "LEAP PMK from EAPOL", pmk, 16);
		if (memcmp(pmk, e->authenticator_pmk, 16) != 0)
			printf("WARNING: PMK mismatch\n");
		else if (e->radius_access_accept_received)
			ret = 0;
	} else if (e->radius_access_accept_received && e->no_mppe_keys) {
		/* No keying material expected */
		ret = 0;
	}

	if (ret)
		e->num_mppe_mismatch++;
	else if (!e->no_mppe_keys)
		e->num_mppe_ok++;

	return ret;
}


static void eapol_sm_cb(struct eapol_sm *eapol, int success, void *ctx)
{
	struct eapol_test_data *e = ctx;
	printf("eapol_sm_cb: success=%d\n", success);
	e->eapol_test_num_reauths--;
	if (e->eapol_test_num_reauths < 0)
		eloop_terminate();
	else {
		eapol_test_compare_pmk(e);
		eloop_register_timeout(0, 100000, eapol_sm_reauth, e, NULL);
	}
}


static int test_eapol(struct eapol_test_data *e, struct wpa_supplicant *wpa_s,
		      struct wpa_ssid *ssid)
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
	ctx->cb_ctx = e;
	ctx->eapol_send_ctx = wpa_s;
	ctx->preauth = 0;
	ctx->eapol_done_cb = eapol_test_eapol_done_cb;
	ctx->eapol_send = eapol_test_eapol_send;
	ctx->set_config_blob = eapol_test_set_config_blob;
	ctx->get_config_blob = eapol_test_get_config_blob;
	ctx->opensc_engine_path = wpa_s->conf->opensc_engine_path;
	ctx->pkcs11_engine_path = wpa_s->conf->pkcs11_engine_path;
	ctx->pkcs11_module_path = wpa_s->conf->pkcs11_module_path;

	wpa_s->eapol = eapol_sm_init(ctx);
	if (wpa_s->eapol == NULL) {
		free(ctx);
		printf("Failed to initialize EAPOL state machines.\n");
		return -1;
	}

	wpa_s->current_ssid = ssid;
	memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.accept_802_1x_keys = 1;
	eapol_conf.required_keys = 0;
	eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;
	eapol_sm_notify_config(wpa_s->eapol, ssid, &eapol_conf);
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);


	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(wpa_s->eapol, TRUE);

	return 0;
}


static void test_eapol_clean(struct eapol_test_data *e,
			     struct wpa_supplicant *wpa_s)
{
	radius_client_deinit(e->radius);
	free(e->last_eap_radius);
	if (e->last_recv_radius) {
		radius_msg_free(e->last_recv_radius);
		free(e->last_recv_radius);
	}
	free(e->eap_identity);
	e->eap_identity = NULL;
	eapol_sm_deinit(wpa_s->eapol);
	wpa_s->eapol = NULL;
	if (e->radius_conf && e->radius_conf->auth_server) {
		free(e->radius_conf->auth_server->shared_secret);
		free(e->radius_conf->auth_server);
	}
	free(e->radius_conf);
	e->radius_conf = NULL;
	scard_deinit(wpa_s->scard);
	wpa_supplicant_ctrl_iface_deinit(wpa_s);
	wpa_config_free(wpa_s->conf);
}


static void send_eap_request_identity(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	u8 buf[100], *pos;
	struct ieee802_1x_hdr *hdr;
	struct eap_hdr *eap;

	hdr = (struct ieee802_1x_hdr *) buf;
	hdr->version = EAPOL_VERSION;
	hdr->type = IEEE802_1X_TYPE_EAP_PACKET;
	hdr->length = htons(5);

	eap = (struct eap_hdr *) (hdr + 1);
	eap->code = EAP_CODE_REQUEST;
	eap->identifier = 0;
	eap->length = htons(5);
	pos = (u8 *) (eap + 1);
	*pos = EAP_TYPE_IDENTITY;

	printf("Sending fake EAP-Request-Identity\n");
	eapol_sm_rx_eapol(wpa_s->eapol, wpa_s->bssid, buf,
			  sizeof(*hdr) + 5);
}


static void eapol_test_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct eapol_test_data *e = eloop_ctx;
	printf("EAPOL test timed out\n");
	e->auth_timed_out = 1;
	eloop_terminate();
}


static char *eap_type_text(u8 type)
{
	switch (type) {
	case EAP_TYPE_IDENTITY: return "Identity";
	case EAP_TYPE_NOTIFICATION: return "Notification";
	case EAP_TYPE_NAK: return "Nak";
	case EAP_TYPE_TLS: return "TLS";
	case EAP_TYPE_TTLS: return "TTLS";
	case EAP_TYPE_PEAP: return "PEAP";
	case EAP_TYPE_SIM: return "SIM";
	case EAP_TYPE_GTC: return "GTC";
	case EAP_TYPE_MD5: return "MD5";
	case EAP_TYPE_OTP: return "OTP";
	default: return "Unknown";
	}
}


static void ieee802_1x_decapsulate_radius(struct eapol_test_data *e)
{
	u8 *eap;
	size_t len;
	struct eap_hdr *hdr;
	int eap_type = -1;
	char buf[64];
	struct radius_msg *msg;

	if (e->last_recv_radius == NULL)
		return;

	msg = e->last_recv_radius;

	eap = radius_msg_get_eap(msg, &len);
	if (eap == NULL) {
		/* draft-aboba-radius-rfc2869bis-20.txt, Chap. 2.6.3:
		 * RADIUS server SHOULD NOT send Access-Reject/no EAP-Message
		 * attribute */
		wpa_printf(MSG_DEBUG, "could not extract "
			       "EAP-Message from RADIUS message");
		free(e->last_eap_radius);
		e->last_eap_radius = NULL;
		e->last_eap_radius_len = 0;
		return;
	}

	if (len < sizeof(*hdr)) {
		wpa_printf(MSG_DEBUG, "too short EAP packet "
			       "received from authentication server");
		free(eap);
		return;
	}

	if (len > sizeof(*hdr))
		eap_type = eap[sizeof(*hdr)];

	hdr = (struct eap_hdr *) eap;
	switch (hdr->code) {
	case EAP_CODE_REQUEST:
		snprintf(buf, sizeof(buf), "EAP-Request-%s (%d)",
			 eap_type >= 0 ? eap_type_text(eap_type) : "??",
			 eap_type);
		break;
	case EAP_CODE_RESPONSE:
		snprintf(buf, sizeof(buf), "EAP Response-%s (%d)",
			 eap_type >= 0 ? eap_type_text(eap_type) : "??",
			 eap_type);
		break;
	case EAP_CODE_SUCCESS:
		snprintf(buf, sizeof(buf), "EAP Success");
		/* LEAP uses EAP Success within an authentication, so must not
		 * stop here with eloop_terminate(); */
		break;
	case EAP_CODE_FAILURE:
		snprintf(buf, sizeof(buf), "EAP Failure");
		eloop_terminate();
		break;
	default:
		snprintf(buf, sizeof(buf), "unknown EAP code");
		wpa_hexdump(MSG_DEBUG, "Decapsulated EAP packet", eap, len);
		break;
	}
	wpa_printf(MSG_DEBUG, "decapsulated EAP packet (code=%d "
		       "id=%d len=%d) from RADIUS server: %s",
		      hdr->code, hdr->identifier, ntohs(hdr->length), buf);

	/* sta->eapol_sm->be_auth.idFromServer = hdr->identifier; */

	free(e->last_eap_radius);
	e->last_eap_radius = eap;
	e->last_eap_radius_len = len;

	{
		struct ieee802_1x_hdr *hdr;
		hdr = malloc(sizeof(*hdr) + len);
		assert(hdr != NULL);
		hdr->version = EAPOL_VERSION;
		hdr->type = IEEE802_1X_TYPE_EAP_PACKET;
		hdr->length = htons(len);
		memcpy((u8 *) (hdr + 1), eap, len);
		eapol_sm_rx_eapol(e->wpa_s->eapol, e->wpa_s->bssid,
				  (u8 *) hdr, sizeof(*hdr) + len);
		free(hdr);
	}
}


static void ieee802_1x_get_keys(struct eapol_test_data *e,
				struct radius_msg *msg, struct radius_msg *req,
				u8 *shared_secret, size_t shared_secret_len)
{
	struct radius_ms_mppe_keys *keys;

	keys = radius_msg_get_ms_keys(msg, req, shared_secret,
				      shared_secret_len);
	if (keys && keys->send == NULL && keys->recv == NULL) {
		free(keys);
		keys = radius_msg_get_cisco_keys(msg, req, shared_secret,
						 shared_secret_len);
	}

	if (keys) {
		if (keys->send) {
			wpa_hexdump(MSG_DEBUG, "MS-MPPE-Send-Key (sign)",
				    keys->send, keys->send_len);
		}
		if (keys->recv) {
			wpa_hexdump(MSG_DEBUG, "MS-MPPE-Recv-Key (crypt)",
				    keys->recv, keys->recv_len);
			e->authenticator_pmk_len =
				keys->recv_len > PMK_LEN ? PMK_LEN :
				keys->recv_len;
			memcpy(e->authenticator_pmk, keys->recv,
			       e->authenticator_pmk_len);
		}

		free(keys->send);
		free(keys->recv);
		free(keys);
	}
}


/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult
ieee802_1x_receive_auth(struct radius_msg *msg, struct radius_msg *req,
			u8 *shared_secret, size_t shared_secret_len,
			void *data)
{
	struct eapol_test_data *e = data;

	/* RFC 2869, Ch. 5.13: valid Message-Authenticator attribute MUST be
	 * present when packet contains an EAP-Message attribute */
	if (msg->hdr->code == RADIUS_CODE_ACCESS_REJECT &&
	    radius_msg_get_attr(msg, RADIUS_ATTR_MESSAGE_AUTHENTICATOR, NULL,
				0) < 0 &&
	    radius_msg_get_attr(msg, RADIUS_ATTR_EAP_MESSAGE, NULL, 0) < 0) {
		wpa_printf(MSG_DEBUG, "Allowing RADIUS "
			      "Access-Reject without Message-Authenticator "
			      "since it does not include EAP-Message\n");
	} else if (radius_msg_verify(msg, shared_secret, shared_secret_len,
				     req, 1)) {
		printf("Incoming RADIUS packet did not have correct "
		       "Message-Authenticator - dropped\n");
		return RADIUS_RX_UNKNOWN;
	}

	if (msg->hdr->code != RADIUS_CODE_ACCESS_ACCEPT &&
	    msg->hdr->code != RADIUS_CODE_ACCESS_REJECT &&
	    msg->hdr->code != RADIUS_CODE_ACCESS_CHALLENGE) {
		printf("Unknown RADIUS message code\n");
		return RADIUS_RX_UNKNOWN;
	}

	e->radius_identifier = -1;
	wpa_printf(MSG_DEBUG, "RADIUS packet matching with station");

	if (e->last_recv_radius) {
		radius_msg_free(e->last_recv_radius);
		free(e->last_recv_radius);
	}

	e->last_recv_radius = msg;

	switch (msg->hdr->code) {
	case RADIUS_CODE_ACCESS_ACCEPT:
		e->radius_access_accept_received = 1;
		ieee802_1x_get_keys(e, msg, req, shared_secret,
				    shared_secret_len);
		break;
	case RADIUS_CODE_ACCESS_REJECT:
		e->radius_access_reject_received = 1;
		break;
	}

	ieee802_1x_decapsulate_radius(e);

	if ((msg->hdr->code == RADIUS_CODE_ACCESS_ACCEPT &&
	     e->eapol_test_num_reauths < 0) ||
	    msg->hdr->code == RADIUS_CODE_ACCESS_REJECT) {
		eloop_terminate();
	}

	return RADIUS_RX_QUEUED;
}


static void wpa_init_conf(struct eapol_test_data *e,
			  struct wpa_supplicant *wpa_s, const char *authsrv,
			  int port, const char *secret)
{
	struct hostapd_radius_server *as;
	int res;

	wpa_s->bssid[5] = 1;
	wpa_s->own_addr[5] = 2;
	e->own_ip_addr.s_addr = htonl((127 << 24) | 1);
	strncpy(wpa_s->ifname, "test", sizeof(wpa_s->ifname));

	e->radius_conf = malloc(sizeof(struct hostapd_radius_servers));
	assert(e->radius_conf != NULL);
	memset(e->radius_conf, 0, sizeof(struct hostapd_radius_servers));
	e->radius_conf->num_auth_servers = 1;
	as = malloc(sizeof(struct hostapd_radius_server));
	assert(as != NULL);
	memset(as, 0, sizeof(*as));
#ifdef CONFIG_NATIVE_WINDOWS
	{
		int a[4];
		u8 *pos;
		sscanf(authsrv, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]);
		pos = (u8 *) &as->addr.u.v4;
		*pos++ = a[0];
		*pos++ = a[1];
		*pos++ = a[2];
		*pos++ = a[3];
	}
#else /* CONFIG_NATIVE_WINDOWS */
	inet_aton(authsrv, &as->addr.u.v4);
#endif /* CONFIG_NATIVE_WINDOWS */
	as->addr.af = AF_INET;
	as->port = port;
	as->shared_secret = (u8 *) strdup(secret);
	as->shared_secret_len = strlen(secret);
	e->radius_conf->auth_server = as;
	e->radius_conf->auth_servers = as;
	e->radius_conf->msg_dumps = 1;

	e->radius = radius_client_init(wpa_s, e->radius_conf);
	assert(e->radius != NULL);

	res = radius_client_register(e->radius, RADIUS_AUTH,
				     ieee802_1x_receive_auth, e);
	assert(res == 0);
}


static int scard_test(void)
{
	struct scard_data *scard;
	size_t len;
	char imsi[20];
	unsigned char rand[16];
#ifdef PCSC_FUNCS
	unsigned char sres[4];
	unsigned char kc[8];
#endif /* PCSC_FUNCS */
#define num_triplets 5
	unsigned char rand_[num_triplets][16];
	unsigned char sres_[num_triplets][4];
	unsigned char kc_[num_triplets][8];
	int i, j, res;

#define AKA_RAND_LEN 16
#define AKA_AUTN_LEN 16
#define AKA_AUTS_LEN 14
#define RES_MAX_LEN 16
#define IK_LEN 16
#define CK_LEN 16
	unsigned char aka_rand[AKA_RAND_LEN];
	unsigned char aka_autn[AKA_AUTN_LEN];
	unsigned char aka_auts[AKA_AUTS_LEN];
	unsigned char aka_res[RES_MAX_LEN];
	size_t aka_res_len;
	unsigned char aka_ik[IK_LEN];
	unsigned char aka_ck[CK_LEN];

	scard = scard_init(SCARD_TRY_BOTH);
	if (scard == NULL)
		return -1;
	if (scard_set_pin(scard, "1234")) {
		wpa_printf(MSG_WARNING, "PIN validation failed");
		scard_deinit(scard);
		return -1;
	}

	len = sizeof(imsi);
	if (scard_get_imsi(scard, imsi, &len))
		goto failed;
	wpa_hexdump_ascii(MSG_DEBUG, "SCARD: IMSI", (u8 *) imsi, len);
	/* NOTE: Permanent Username: 1 | IMSI */

	memset(rand, 0, sizeof(rand));
	if (scard_gsm_auth(scard, rand, sres, kc))
		goto failed;

	memset(rand, 0xff, sizeof(rand));
	if (scard_gsm_auth(scard, rand, sres, kc))
		goto failed;

	for (i = 0; i < num_triplets; i++) {
		memset(rand_[i], i, sizeof(rand_[i]));
		if (scard_gsm_auth(scard, rand_[i], sres_[i], kc_[i]))
			goto failed;
	}

	for (i = 0; i < num_triplets; i++) {
		printf("1");
		for (j = 0; j < len; j++)
			printf("%c", imsi[j]);
		printf(",");
		for (j = 0; j < 16; j++)
			printf("%02X", rand_[i][j]);
		printf(",");
		for (j = 0; j < 4; j++)
			printf("%02X", sres_[i][j]);
		printf(",");
		for (j = 0; j < 8; j++)
			printf("%02X", kc_[i][j]);
		printf("\n");
	}

	wpa_printf(MSG_DEBUG, "Trying to use UMTS authentication");

	/* seq 39 (0x28) */
	memset(aka_rand, 0xaa, 16);
	memcpy(aka_autn, "\x86\x71\x31\xcb\xa2\xfc\x61\xdf"
	       "\xa3\xb3\x97\x9d\x07\x32\xa2\x12", 16);

	res = scard_umts_auth(scard, aka_rand, aka_autn, aka_res, &aka_res_len,
			      aka_ik, aka_ck, aka_auts);
	if (res == 0) {
		wpa_printf(MSG_DEBUG, "UMTS auth completed successfully");
		wpa_hexdump(MSG_DEBUG, "RES", aka_res, aka_res_len);
		wpa_hexdump(MSG_DEBUG, "IK", aka_ik, IK_LEN);
		wpa_hexdump(MSG_DEBUG, "CK", aka_ck, CK_LEN);
	} else if (res == -2) {
		wpa_printf(MSG_DEBUG, "UMTS auth resulted in synchronization "
			   "failure");
		wpa_hexdump(MSG_DEBUG, "AUTS", aka_auts, AKA_AUTS_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "UMTS auth failed");
	}

failed:
	scard_deinit(scard);

	return 0;
#undef num_triplets
}


static int scard_get_triplets(int argc, char *argv[])
{
	struct scard_data *scard;
	size_t len;
	char imsi[20];
	unsigned char rand[16];
	unsigned char sres[4];
	unsigned char kc[8];
	int num_triplets;
	int i, j;

	if (argc < 2 || ((num_triplets = atoi(argv[1])) <= 0)) {
		printf("invalid parameters for sim command\n");
		return -1;
	}

	if (argc <= 2 || strcmp(argv[2], "debug") != 0) {
		/* disable debug output */
		wpa_debug_level = 99;
	}

	scard = scard_init(SCARD_GSM_SIM_ONLY);
	if (scard == NULL) {
		printf("Failed to open smartcard connection\n");
		return -1;
	}
	if (scard_set_pin(scard, argv[0])) {
		wpa_printf(MSG_WARNING, "PIN validation failed");
		scard_deinit(scard);
		return -1;
	}

	len = sizeof(imsi);
	if (scard_get_imsi(scard, imsi, &len)) {
		scard_deinit(scard);
		return -1;
	}

	for (i = 0; i < num_triplets; i++) {
		memset(rand, i, sizeof(rand));
		if (scard_gsm_auth(scard, rand, sres, kc))
			break;

		/* IMSI:Kc:SRES:RAND */
		for (j = 0; j < len; j++)
			printf("%c", imsi[j]);
		printf(":");
		for (j = 0; j < 8; j++)
			printf("%02X", kc[j]);
		printf(":");
		for (j = 0; j < 4; j++)
			printf("%02X", sres[j]);
		printf(":");
		for (j = 0; j < 16; j++)
			printf("%02X", rand[j]);
		printf("\n");
	}

	scard_deinit(scard);

	return 0;
}


static void eapol_test_terminate(int sig, void *eloop_ctx,
				 void *signal_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_msg(wpa_s, MSG_INFO, "Signal %d received - terminating", sig);
	eloop_terminate();
}


static void usage(void)
{
	printf("usage:\n"
	       "eapol_test [-nWS] -c<conf> [-a<AS IP>] [-p<AS port>] "
	       "[-s<AS secret>] [-r<count>]\n"
	       "eapol_test scard\n"
	       "eapol_test sim <PIN> <num triplets> [debug]\n"
	       "\n"
	       "options:\n"
	       "  -c<conf> = configuration file\n"
	       "  -a<AS IP> = IP address of the authentication server, "
	       "default 127.0.0.1\n"
	       "  -p<AS port> = UDP port of the authentication server, "
	       "default 1812\n"
	       "  -s<AS secret> = shared secret with the authentication "
	       "server, default 'radius'\n"
	       "  -r<count> = number of re-authentications\n"
	       "  -W = wait for a control interface monitor before starting\n"
	       "  -S = save configuration after authentiation\n"
	       "  -n = no MPPE keys expected\n");
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant wpa_s;
	int c, ret = 1, wait_for_monitor = 0, save_config = 0;
	char *as_addr = "127.0.0.1";
	int as_port = 1812;
	char *as_secret = "radius";
	char *conf = NULL;

#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		printf("Could not find a usable WinSock.dll\n");
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	memset(&eapol_test, 0, sizeof(eapol_test));

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	for (;;) {
		c = getopt(argc, argv, "a:c:np:r:s:SW");
		if (c < 0)
			break;
		switch (c) {
		case 'a':
			as_addr = optarg;
			break;
		case 'c':
			conf = optarg;
			break;
		case 'n':
			eapol_test.no_mppe_keys++;
			break;
		case 'p':
			as_port = atoi(optarg);
			break;
		case 'r':
			eapol_test.eapol_test_num_reauths = atoi(optarg);
			break;
		case 's':
			as_secret = optarg;
			break;
		case 'S':
			save_config++;
			break;
		case 'W':
			wait_for_monitor++;
			break;
		default:
			usage();
			return -1;
		}
	}

	if (argc > optind && strcmp(argv[optind], "scard") == 0) {
		return scard_test();
	}

	if (argc > optind && strcmp(argv[optind], "sim") == 0) {
		return scard_get_triplets(argc - optind - 1,
					  &argv[optind + 1]);
	}

	if (conf == NULL) {
		usage();
		printf("Configuration file is required.\n");
		return -1;
	}

	eloop_init(&wpa_s);

	memset(&wpa_s, 0, sizeof(wpa_s));
	eapol_test.wpa_s = &wpa_s;
	wpa_s.conf = wpa_config_read(conf);
	if (wpa_s.conf == NULL) {
		printf("Failed to parse configuration file '%s'.\n", conf);
		return -1;
	}
	if (wpa_s.conf->ssid == NULL) {
		printf("No networks defined.\n");
		return -1;
	}

	wpa_init_conf(&eapol_test, &wpa_s, as_addr, as_port, as_secret);
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
	if (wpa_supplicant_scard_init(&wpa_s, wpa_s.conf->ssid))
		return -1;

	if (test_eapol(&eapol_test, &wpa_s, wpa_s.conf->ssid))
		return -1;

	if (wait_for_monitor)
		wpa_supplicant_ctrl_iface_wait(&wpa_s);

	eloop_register_timeout(30, 0, eapol_test_timeout, &eapol_test, NULL);
	eloop_register_timeout(0, 0, send_eap_request_identity, &wpa_s, NULL);
	eloop_register_signal(SIGINT, eapol_test_terminate, NULL);
	eloop_register_signal(SIGTERM, eapol_test_terminate, NULL);
#ifndef CONFIG_NATIVE_WINDOWS
	eloop_register_signal(SIGHUP, eapol_test_terminate, NULL);
#endif /* CONFIG_NATIVE_WINDOWS */
	eloop_run();

	if (eapol_test_compare_pmk(&eapol_test) == 0)
		ret = 0;
	if (eapol_test.auth_timed_out)
		ret = -2;
	if (eapol_test.radius_access_reject_received)
		ret = -3;

	if (save_config)
		wpa_config_write(conf, wpa_s.conf);

	test_eapol_clean(&eapol_test, &wpa_s);

	eloop_destroy();

	printf("MPPE keys OK: %d  mismatch: %d\n",
	       eapol_test.num_mppe_ok, eapol_test.num_mppe_mismatch);
	if (eapol_test.num_mppe_mismatch)
		ret = -4;
	if (ret)
		printf("FAILURE\n");
	else
		printf("SUCCESS\n");

#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */

	return ret;
}
