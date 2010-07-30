/*
 * WPA Supplicant - test code
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
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

#include "includes.h"
#include <assert.h>

#include "common.h"
#include "config.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "eap_peer/eap.h"
#include "eloop.h"
#include "wpa.h"
#include "eap_peer/eap_i.h"
#include "wpa_supplicant_i.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"


extern int wpa_debug_level;
extern int wpa_debug_show_keys;

struct wpa_driver_ops *wpa_supplicant_drivers[] = { NULL };


struct extra_radius_attr {
	u8 type;
	char syntax;
	char *data;
	struct extra_radius_attr *next;
};

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

	char *connect_info;
	u8 own_addr[ETH_ALEN];
	struct extra_radius_attr *extra_attrs;
};

static struct eapol_test_data eapol_test;


static void send_eap_request_identity(void *eloop_ctx, void *timeout_ctx);


static void hostapd_logger_cb(void *ctx, const u8 *addr, unsigned int module,
			      int level, const char *txt, size_t len)
{
	if (addr)
		wpa_printf(MSG_DEBUG, "STA " MACSTR ": %s\n",
			   MAC2STR(addr), txt);
	else
		wpa_printf(MSG_DEBUG, "%s", txt);
}


static int add_extra_attr(struct radius_msg *msg,
			  struct extra_radius_attr *attr)
{
	size_t len;
	char *pos;
	u32 val;
	char buf[128];

	switch (attr->syntax) {
	case 's':
		os_snprintf(buf, sizeof(buf), "%s", attr->data);
		len = os_strlen(buf);
		break;
	case 'n':
		buf[0] = '\0';
		len = 1;
		break;
	case 'x':
		pos = attr->data;
		if (pos[0] == '0' && pos[1] == 'x')
			pos += 2;
		len = os_strlen(pos);
		if ((len & 1) || (len / 2) > sizeof(buf)) {
			printf("Invalid extra attribute hexstring\n");
			return -1;
		}
		len /= 2;
		if (hexstr2bin(pos, (u8 *) buf, len) < 0) {
			printf("Invalid extra attribute hexstring\n");
			return -1;
		}
		break;
	case 'd':
		val = htonl(atoi(attr->data));
		os_memcpy(buf, &val, 4);
		len = 4;
		break;
	default:
		printf("Incorrect extra attribute syntax specification\n");
		return -1;
	}

	if (!radius_msg_add_attr(msg, attr->type, (u8 *) buf, len)) {
		printf("Could not add attribute %d\n", attr->type);
		return -1;
	}

	return 0;
}


static int add_extra_attrs(struct radius_msg *msg,
			   struct extra_radius_attr *attrs)
{
	struct extra_radius_attr *p;
	for (p = attrs; p; p = p->next) {
		if (add_extra_attr(msg, p) < 0)
			return -1;
	}
	return 0;
}


static struct extra_radius_attr *
find_extra_attr(struct extra_radius_attr *attrs, u8 type)
{
	struct extra_radius_attr *p;
	for (p = attrs; p; p = p->next) {
		if (p->type == type)
			return p;
	}
	return NULL;
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
		os_free(e->eap_identity);
		e->eap_identity_len = len - sizeof(*hdr) - 1;
		e->eap_identity = os_malloc(e->eap_identity_len);
		if (e->eap_identity) {
			os_memcpy(e->eap_identity, pos, e->eap_identity_len);
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

	if (!find_extra_attr(e->extra_attrs, RADIUS_ATTR_NAS_IP_ADDRESS) &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				 (u8 *) &e->own_ip_addr, 4)) {
		printf("Could not add NAS-IP-Address\n");
		goto fail;
	}

	os_snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
		    MAC2STR(e->wpa_s->own_addr));
	if (!find_extra_attr(e->extra_attrs, RADIUS_ATTR_CALLING_STATION_ID)
	    &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_CALLING_STATION_ID,
				 (u8 *) buf, os_strlen(buf))) {
		printf("Could not add Calling-Station-Id\n");
		goto fail;
	}

	/* TODO: should probably check MTU from driver config; 2304 is max for
	 * IEEE 802.11, but use 1400 to avoid problems with too large packets
	 */
	if (!find_extra_attr(e->extra_attrs, RADIUS_ATTR_FRAMED_MTU) &&
	    !radius_msg_add_attr_int32(msg, RADIUS_ATTR_FRAMED_MTU, 1400)) {
		printf("Could not add Framed-MTU\n");
		goto fail;
	}

	if (!find_extra_attr(e->extra_attrs, RADIUS_ATTR_NAS_PORT_TYPE) &&
	    !radius_msg_add_attr_int32(msg, RADIUS_ATTR_NAS_PORT_TYPE,
				       RADIUS_NAS_PORT_TYPE_IEEE_802_11)) {
		printf("Could not add NAS-Port-Type\n");
		goto fail;
	}

	os_snprintf(buf, sizeof(buf), "%s", e->connect_info);
	if (!find_extra_attr(e->extra_attrs, RADIUS_ATTR_CONNECT_INFO) &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_CONNECT_INFO,
				 (u8 *) buf, os_strlen(buf))) {
		printf("Could not add Connect-Info\n");
		goto fail;
	}

	if (add_extra_attrs(msg, e->extra_attrs) < 0)
		goto fail;

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
	os_free(msg);
}


static int eapol_test_eapol_send(void *ctx, int type, const u8 *buf,
				 size_t len)
{
	/* struct wpa_supplicant *wpa_s = ctx; */
	printf("WPA: eapol_test_eapol_send(type=%d len=%lu)\n",
	       type, (unsigned long) len);
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
		if (os_memcmp(pmk, e->authenticator_pmk, PMK_LEN) != 0) {
			printf("WARNING: PMK mismatch\n");
			wpa_hexdump(MSG_DEBUG, "PMK from AS",
				    e->authenticator_pmk, PMK_LEN);
		} else if (e->radius_access_accept_received)
			ret = 0;
	} else if (e->authenticator_pmk_len == 16 &&
		   eapol_sm_get_key(e->wpa_s->eapol, pmk, 16) == 0) {
		wpa_hexdump(MSG_DEBUG, "LEAP PMK from EAPOL", pmk, 16);
		if (os_memcmp(pmk, e->authenticator_pmk, 16) != 0) {
			printf("WARNING: PMK mismatch\n");
			wpa_hexdump(MSG_DEBUG, "PMK from AS",
				    e->authenticator_pmk, 16);
		} else if (e->radius_access_accept_received)
			ret = 0;
	} else if (e->radius_access_accept_received && e->no_mppe_keys) {
		/* No keying material expected */
		ret = 0;
	}

	if (ret && !e->no_mppe_keys)
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

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		printf("Failed to allocate EAPOL context.\n");
		return -1;
	}
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
#ifdef EAP_TLS_OPENSSL
	ctx->opensc_engine_path = wpa_s->conf->opensc_engine_path;
	ctx->pkcs11_engine_path = wpa_s->conf->pkcs11_engine_path;
	ctx->pkcs11_module_path = wpa_s->conf->pkcs11_module_path;
#endif /* EAP_TLS_OPENSSL */

	wpa_s->eapol = eapol_sm_init(ctx);
	if (wpa_s->eapol == NULL) {
		os_free(ctx);
		printf("Failed to initialize EAPOL state machines.\n");
		return -1;
	}

	wpa_s->current_ssid = ssid;
	os_memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.accept_802_1x_keys = 1;
	eapol_conf.required_keys = 0;
	eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;
	eapol_sm_notify_config(wpa_s->eapol, &ssid->eap, &eapol_conf);
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);


	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(wpa_s->eapol, TRUE);

	return 0;
}


static void test_eapol_clean(struct eapol_test_data *e,
			     struct wpa_supplicant *wpa_s)
{
	struct extra_radius_attr *p, *prev;

	radius_client_deinit(e->radius);
	os_free(e->last_eap_radius);
	if (e->last_recv_radius) {
		radius_msg_free(e->last_recv_radius);
		os_free(e->last_recv_radius);
	}
	os_free(e->eap_identity);
	e->eap_identity = NULL;
	eapol_sm_deinit(wpa_s->eapol);
	wpa_s->eapol = NULL;
	if (e->radius_conf && e->radius_conf->auth_server) {
		os_free(e->radius_conf->auth_server->shared_secret);
		os_free(e->radius_conf->auth_server);
	}
	os_free(e->radius_conf);
	e->radius_conf = NULL;
	scard_deinit(wpa_s->scard);
	if (wpa_s->ctrl_iface) {
		wpa_supplicant_ctrl_iface_deinit(wpa_s->ctrl_iface);
		wpa_s->ctrl_iface = NULL;
	}
	wpa_config_free(wpa_s->conf);

	p = e->extra_attrs;
	while (p) {
		prev = p;
		p = p->next;
		os_free(prev);
	}
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
	case EAP_TYPE_FAST: return "FAST";
	case EAP_TYPE_SAKE: return "SAKE";
	case EAP_TYPE_PSK: return "PSK";
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
		os_free(e->last_eap_radius);
		e->last_eap_radius = NULL;
		e->last_eap_radius_len = 0;
		return;
	}

	if (len < sizeof(*hdr)) {
		wpa_printf(MSG_DEBUG, "too short EAP packet "
			       "received from authentication server");
		os_free(eap);
		return;
	}

	if (len > sizeof(*hdr))
		eap_type = eap[sizeof(*hdr)];

	hdr = (struct eap_hdr *) eap;
	switch (hdr->code) {
	case EAP_CODE_REQUEST:
		os_snprintf(buf, sizeof(buf), "EAP-Request-%s (%d)",
			    eap_type >= 0 ? eap_type_text(eap_type) : "??",
			    eap_type);
		break;
	case EAP_CODE_RESPONSE:
		os_snprintf(buf, sizeof(buf), "EAP Response-%s (%d)",
			    eap_type >= 0 ? eap_type_text(eap_type) : "??",
			    eap_type);
		break;
	case EAP_CODE_SUCCESS:
		os_strlcpy(buf, "EAP Success", sizeof(buf));
		/* LEAP uses EAP Success within an authentication, so must not
		 * stop here with eloop_terminate(); */
		break;
	case EAP_CODE_FAILURE:
		os_strlcpy(buf, "EAP Failure", sizeof(buf));
		eloop_terminate();
		break;
	default:
		os_strlcpy(buf, "unknown EAP code", sizeof(buf));
		wpa_hexdump(MSG_DEBUG, "Decapsulated EAP packet", eap, len);
		break;
	}
	wpa_printf(MSG_DEBUG, "decapsulated EAP packet (code=%d "
		       "id=%d len=%d) from RADIUS server: %s",
		      hdr->code, hdr->identifier, ntohs(hdr->length), buf);

	/* sta->eapol_sm->be_auth.idFromServer = hdr->identifier; */

	os_free(e->last_eap_radius);
	e->last_eap_radius = eap;
	e->last_eap_radius_len = len;

	{
		struct ieee802_1x_hdr *dot1x;
		dot1x = os_malloc(sizeof(*dot1x) + len);
		assert(dot1x != NULL);
		dot1x->version = EAPOL_VERSION;
		dot1x->type = IEEE802_1X_TYPE_EAP_PACKET;
		dot1x->length = htons(len);
		os_memcpy((u8 *) (dot1x + 1), eap, len);
		eapol_sm_rx_eapol(e->wpa_s->eapol, e->wpa_s->bssid,
				  (u8 *) dot1x, sizeof(*dot1x) + len);
		os_free(dot1x);
	}
}


static void ieee802_1x_get_keys(struct eapol_test_data *e,
				struct radius_msg *msg, struct radius_msg *req,
				const u8 *shared_secret,
				size_t shared_secret_len)
{
	struct radius_ms_mppe_keys *keys;

	keys = radius_msg_get_ms_keys(msg, req, shared_secret,
				      shared_secret_len);
	if (keys && keys->send == NULL && keys->recv == NULL) {
		os_free(keys);
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
			os_memcpy(e->authenticator_pmk, keys->recv,
				  e->authenticator_pmk_len);
			if (e->authenticator_pmk_len == 16 && keys->send &&
			    keys->send_len == 16) {
				/* MS-CHAP-v2 derives 16 octet keys */
				wpa_printf(MSG_DEBUG, "Use MS-MPPE-Send-Key "
					   "to extend PMK to 32 octets");
				os_memcpy(e->authenticator_pmk +
					  e->authenticator_pmk_len,
					  keys->send, keys->send_len);
				e->authenticator_pmk_len += keys->send_len;
			}
		}

		os_free(keys->send);
		os_free(keys->recv);
		os_free(keys);
	}
}


/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult
ieee802_1x_receive_auth(struct radius_msg *msg, struct radius_msg *req,
			const u8 *shared_secret, size_t shared_secret_len,
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
		os_free(e->last_recv_radius);
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
			  int port, const char *secret,
			  const char *cli_addr)
{
	struct hostapd_radius_server *as;
	int res;

	wpa_s->bssid[5] = 1;
	os_memcpy(wpa_s->own_addr, e->own_addr, ETH_ALEN);
	e->own_ip_addr.s_addr = htonl((127 << 24) | 1);
	os_strlcpy(wpa_s->ifname, "test", sizeof(wpa_s->ifname));

	e->radius_conf = os_zalloc(sizeof(struct hostapd_radius_servers));
	assert(e->radius_conf != NULL);
	e->radius_conf->num_auth_servers = 1;
	as = os_zalloc(sizeof(struct hostapd_radius_server));
	assert(as != NULL);
#if defined(CONFIG_NATIVE_WINDOWS) || defined(CONFIG_ANSI_C_EXTRA)
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
#else /* CONFIG_NATIVE_WINDOWS or CONFIG_ANSI_C_EXTRA */
	inet_aton(authsrv, &as->addr.u.v4);
#endif /* CONFIG_NATIVE_WINDOWS or CONFIG_ANSI_C_EXTRA */
	as->addr.af = AF_INET;
	as->port = port;
	as->shared_secret = (u8 *) os_strdup(secret);
	as->shared_secret_len = os_strlen(secret);
	e->radius_conf->auth_server = as;
	e->radius_conf->auth_servers = as;
	e->radius_conf->msg_dumps = 1;
	if (cli_addr) {
		if (hostapd_parse_ip_addr(cli_addr,
					  &e->radius_conf->client_addr) == 0)
			e->radius_conf->force_client_addr = 1;
		else {
			wpa_printf(MSG_ERROR, "Invalid IP address '%s'",
				   cli_addr);
			assert(0);
		}
	}

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
	unsigned char _rand[16];
#ifdef PCSC_FUNCS
	unsigned char sres[4];
	unsigned char kc[8];
#endif /* PCSC_FUNCS */
#define num_triplets 5
	unsigned char rand_[num_triplets][16];
	unsigned char sres_[num_triplets][4];
	unsigned char kc_[num_triplets][8];
	int i, res;
	size_t j;

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

	os_memset(_rand, 0, sizeof(_rand));
	if (scard_gsm_auth(scard, _rand, sres, kc))
		goto failed;

	os_memset(_rand, 0xff, sizeof(_rand));
	if (scard_gsm_auth(scard, _rand, sres, kc))
		goto failed;

	for (i = 0; i < num_triplets; i++) {
		os_memset(rand_[i], i, sizeof(rand_[i]));
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
	os_memset(aka_rand, 0xaa, 16);
	os_memcpy(aka_autn, "\x86\x71\x31\xcb\xa2\xfc\x61\xdf"
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
	unsigned char _rand[16];
	unsigned char sres[4];
	unsigned char kc[8];
	int num_triplets;
	int i;
	size_t j;

	if (argc < 2 || ((num_triplets = atoi(argv[1])) <= 0)) {
		printf("invalid parameters for sim command\n");
		return -1;
	}

	if (argc <= 2 || os_strcmp(argv[2], "debug") != 0) {
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
		os_memset(_rand, i, sizeof(_rand));
		if (scard_gsm_auth(scard, _rand, sres, kc))
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
			printf("%02X", _rand[j]);
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
	       "[-s<AS secret>]\\\n"
	       "           [-r<count>] [-t<timeout>] [-C<Connect-Info>] \\\n"
	       "           [-M<client MAC address>] \\\n"
	       "           [-N<attr spec>] \\\n"
	       "           [-A<client IP>]\n"
	       "eapol_test scard\n"
	       "eapol_test sim <PIN> <num triplets> [debug]\n"
	       "\n");
	printf("options:\n"
	       "  -c<conf> = configuration file\n"
	       "  -a<AS IP> = IP address of the authentication server, "
	       "default 127.0.0.1\n"
	       "  -p<AS port> = UDP port of the authentication server, "
	       "default 1812\n"
	       "  -s<AS secret> = shared secret with the authentication "
	       "server, default 'radius'\n"
	       "  -A<client IP> = IP address of the client, default: select "
	       "automatically\n"
	       "  -r<count> = number of re-authentications\n"
	       "  -W = wait for a control interface monitor before starting\n"
	       "  -S = save configuration after authentication\n"
	       "  -n = no MPPE keys expected\n"
	       "  -t<timeout> = sets timeout in seconds (default: 30 s)\n"
	       "  -C<Connect-Info> = RADIUS Connect-Info (default: "
	       "CONNECT 11Mbps 802.11b)\n"
	       "  -M<client MAC address> = Set own MAC address "
	       "(Calling-Station-Id,\n"
	       "                           default: 02:00:00:00:00:01)\n"
	       "  -N<attr spec> = send arbitrary attribute specified by:\n"
	       "                  attr_id:syntax:value or attr_id\n"
	       "                  attr_id - number id of the attribute\n"
	       "                  syntax - one of: s, d, x\n"
	       "                     s = string\n"
	       "                     d = integer\n"
	       "                     x = octet string\n"
	       "                  value - attribute value.\n"
	       "       When only attr_id is specified, NULL will be used as "
	       "value.\n"
	       "       Multiple attributes can be specified by using the "
	       "option several times.\n");
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant wpa_s;
	int c, ret = 1, wait_for_monitor = 0, save_config = 0;
	char *as_addr = "127.0.0.1";
	int as_port = 1812;
	char *as_secret = "radius";
	char *cli_addr = NULL;
	char *conf = NULL;
	int timeout = 30;
	char *pos;
	struct extra_radius_attr *p = NULL, *p1;

	if (os_program_init())
		return -1;

	hostapd_logger_register_cb(hostapd_logger_cb);

	os_memset(&eapol_test, 0, sizeof(eapol_test));
	eapol_test.connect_info = "CONNECT 11Mbps 802.11b";
	os_memcpy(eapol_test.own_addr, "\x02\x00\x00\x00\x00\x01", ETH_ALEN);

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	for (;;) {
		c = getopt(argc, argv, "a:A:c:C:M:nN:p:r:s:St:W");
		if (c < 0)
			break;
		switch (c) {
		case 'a':
			as_addr = optarg;
			break;
		case 'A':
			cli_addr = optarg;
			break;
		case 'c':
			conf = optarg;
			break;
		case 'C':
			eapol_test.connect_info = optarg;
			break;
		case 'M':
			if (hwaddr_aton(optarg, eapol_test.own_addr)) {
				usage();
				return -1;
			}
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
		case 't':
			timeout = atoi(optarg);
			break;
		case 'W':
			wait_for_monitor++;
			break;
		case 'N':
			p1 = os_zalloc(sizeof(p1));
			if (p1 == NULL)
				break;
			if (!p)
				eapol_test.extra_attrs = p1;
			else
				p->next = p1;
			p = p1;

			p->type = atoi(optarg);
			pos = os_strchr(optarg, ':');
			if (pos == NULL) {
				p->syntax = 'n';
				p->data = NULL;
				break;
			}

			pos++;
			if (pos[0] == '\0' || pos[1] != ':') {
				printf("Incorrect format of attribute "
				       "specification\n");
				break;
			}

			p->syntax = pos[0];
			p->data = pos + 2;
			break;
		default:
			usage();
			return -1;
		}
	}

	if (argc > optind && os_strcmp(argv[optind], "scard") == 0) {
		return scard_test();
	}

	if (argc > optind && os_strcmp(argv[optind], "sim") == 0) {
		return scard_get_triplets(argc - optind - 1,
					  &argv[optind + 1]);
	}

	if (conf == NULL) {
		usage();
		printf("Configuration file is required.\n");
		return -1;
	}

	if (eap_peer_register_methods()) {
		wpa_printf(MSG_ERROR, "Failed to register EAP methods");
		return -1;
	}

	if (eloop_init(&wpa_s)) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return -1;
	}

	os_memset(&wpa_s, 0, sizeof(wpa_s));
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

	wpa_init_conf(&eapol_test, &wpa_s, as_addr, as_port, as_secret,
		      cli_addr);
	wpa_s.ctrl_iface = wpa_supplicant_ctrl_iface_init(&wpa_s);
	if (wpa_s.ctrl_iface == NULL) {
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
		wpa_supplicant_ctrl_iface_wait(wpa_s.ctrl_iface);

	eloop_register_timeout(timeout, 0, eapol_test_timeout, &eapol_test,
			       NULL);
	eloop_register_timeout(0, 0, send_eap_request_identity, &wpa_s, NULL);
	eloop_register_signal_terminate(eapol_test_terminate, NULL);
	eloop_register_signal_reconfig(eapol_test_terminate, NULL);
	eloop_run();

	eloop_cancel_timeout(eapol_test_timeout, &eapol_test, NULL);
	eloop_cancel_timeout(eapol_sm_reauth, &eapol_test, NULL);

	if (eapol_test_compare_pmk(&eapol_test) == 0 ||
	    eapol_test.no_mppe_keys)
		ret = 0;
	if (eapol_test.auth_timed_out)
		ret = -2;
	if (eapol_test.radius_access_reject_received)
		ret = -3;

	if (save_config)
		wpa_config_write(conf, wpa_s.conf);

	test_eapol_clean(&eapol_test, &wpa_s);

	eap_peer_unregister_methods();

	eloop_destroy();

	printf("MPPE keys OK: %d  mismatch: %d\n",
	       eapol_test.num_mppe_ok, eapol_test.num_mppe_mismatch);
	if (eapol_test.num_mppe_mismatch)
		ret = -4;
	if (ret)
		printf("FAILURE\n");
	else
		printf("SUCCESS\n");

	os_program_deinit();

	return ret;
}
