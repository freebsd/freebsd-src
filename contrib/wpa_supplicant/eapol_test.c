/*
 * WPA Supplicant - test code
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>

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
static int eapol_test_num_reauths = 0;
static int no_mppe_keys = 0;
static int num_mppe_ok = 0, num_mppe_mismatch = 0;

static void send_eap_request_identity(void *eloop_ctx, void *timeout_ctx);


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


static void ieee802_1x_encapsulate_radius(struct wpa_supplicant *wpa_s,
					  u8 *eap, size_t len)
{
	struct radius_msg *msg;
	char buf[128];
	struct eap_hdr *hdr;
	u8 *pos;

	wpa_printf(MSG_DEBUG, "Encapsulating EAP message into a RADIUS "
		   "packet");

	wpa_s->radius_identifier = radius_client_get_id(wpa_s);
	msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST,
			     wpa_s->radius_identifier);
	if (msg == NULL) {
		printf("Could not create net RADIUS packet\n");
		return;
	}

	radius_msg_make_authenticator(msg, (u8 *) wpa_s, sizeof(*wpa_s));

	hdr = (struct eap_hdr *) eap;
	pos = (u8 *) (hdr + 1);
	if (len > sizeof(*hdr) && hdr->code == EAP_CODE_RESPONSE &&
	    pos[0] == EAP_TYPE_IDENTITY) {
		pos++;
		free(wpa_s->eap_identity);
		wpa_s->eap_identity_len = len - sizeof(*hdr) - 1;
		wpa_s->eap_identity = malloc(wpa_s->eap_identity_len);
		if (wpa_s->eap_identity) {
			memcpy(wpa_s->eap_identity, pos,
			       wpa_s->eap_identity_len);
			wpa_hexdump(MSG_DEBUG, "Learned identity from "
				    "EAP-Response-Identity",
				    wpa_s->eap_identity,
				    wpa_s->eap_identity_len);
		}
	}

	if (wpa_s->eap_identity &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_USER_NAME,
				 wpa_s->eap_identity,
				 wpa_s->eap_identity_len)) {
		printf("Could not add User-Name\n");
		goto fail;
	}

	if (!radius_msg_add_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				 (u8 *) &wpa_s->own_ip_addr, 4)) {
		printf("Could not add NAS-IP-Address\n");
		goto fail;
	}

	snprintf(buf, sizeof(buf), RADIUS_802_1X_ADDR_FORMAT,
		 MAC2STR(wpa_s->own_addr));
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
	if (wpa_s->last_recv_radius && wpa_s->last_recv_radius->hdr->code ==
	    RADIUS_CODE_ACCESS_CHALLENGE) {
		int res = radius_msg_copy_attr(msg, wpa_s->last_recv_radius,
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

	radius_client_send(wpa_s, msg, RADIUS_AUTH, wpa_s->own_addr);
	return;

 fail:
	radius_msg_free(msg);
	free(msg);
}


static int eapol_test_eapol_send(void *ctx, int type, u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	printf("WPA: wpa_eapol_send(type=%d len=%d)\n", type, len);
	if (type == IEEE802_1X_TYPE_EAP_PACKET) {
		wpa_hexdump(MSG_DEBUG, "TX EAP -> RADIUS", buf, len);
		ieee802_1x_encapsulate_radius(wpa_s, buf, len);
	}
	return 0;
}


static void eapol_test_eapol_done_cb(void *ctx)
{
	printf("WPA: EAPOL processing complete\n");
}


static void eapol_sm_reauth(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	printf("\n\n\n\n\neapol_test: Triggering EAP reauthentication\n\n");
	wpa_s->radius_access_accept_received = 0;
	send_eap_request_identity(eloop_ctx, timeout_ctx);
}


static int eapol_test_compare_pmk(struct wpa_supplicant *wpa_s)
{
	u8 pmk[PMK_LEN];
	int ret = 1;

	if (eapol_sm_get_key(wpa_s->eapol, pmk, PMK_LEN) == 0) {
		wpa_hexdump(MSG_DEBUG, "PMK from EAPOL", pmk, PMK_LEN);
		if (memcmp(pmk, wpa_s->authenticator_pmk, PMK_LEN) != 0)
			printf("WARNING: PMK mismatch\n");
		else if (wpa_s->radius_access_accept_received)
			ret = 0;
	} else if (wpa_s->authenticator_pmk_len == 16 &&
		   eapol_sm_get_key(wpa_s->eapol, pmk, 16) == 0) {
		wpa_hexdump(MSG_DEBUG, "LEAP PMK from EAPOL", pmk, 16);
		if (memcmp(pmk, wpa_s->authenticator_pmk, 16) != 0)
			printf("WARNING: PMK mismatch\n");
		else if (wpa_s->radius_access_accept_received)
			ret = 0;
	} else if (wpa_s->radius_access_accept_received && no_mppe_keys) {
		/* No keying material expected */
		ret = 0;
	}

	if (ret)
		num_mppe_mismatch++;
	else if (!no_mppe_keys)
		num_mppe_ok++;

	return ret;
}


static void eapol_sm_cb(struct eapol_sm *eapol, int success, void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	printf("eapol_sm_cb: success=%d\n", success);
	eapol_test_num_reauths--;
	if (eapol_test_num_reauths < 0)
		eloop_terminate();
	else {
		eapol_test_compare_pmk(wpa_s);
		eloop_register_timeout(0, 100000, eapol_sm_reauth,
				       wpa_s, NULL);
	}
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


static void test_eapol_clean(struct wpa_supplicant *wpa_s)
{
	radius_client_deinit(wpa_s);
	free(wpa_s->last_eap_radius);
	if (wpa_s->last_recv_radius) {
		radius_msg_free(wpa_s->last_recv_radius);
		free(wpa_s->last_recv_radius);
	}
	free(wpa_s->eap_identity);
	wpa_s->eap_identity = NULL;
	eapol_sm_deinit(wpa_s->eapol);
	wpa_s->eapol = NULL;
	if (wpa_s->auth_server) {
		free(wpa_s->auth_server->shared_secret);
		free(wpa_s->auth_server);
	}
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
	struct wpa_supplicant *wpa_s = eloop_ctx;
	printf("EAPOL test timed out\n");
	wpa_s->auth_timed_out = 1;
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


static void ieee802_1x_decapsulate_radius(struct wpa_supplicant *wpa_s)
{
	u8 *eap;
	size_t len;
	struct eap_hdr *hdr;
	int eap_type = -1;
	char buf[64];
	struct radius_msg *msg;

	if (wpa_s->last_recv_radius == NULL)
		return;

	msg = wpa_s->last_recv_radius;

	eap = radius_msg_get_eap(msg, &len);
	if (eap == NULL) {
		/* draft-aboba-radius-rfc2869bis-20.txt, Chap. 2.6.3:
		 * RADIUS server SHOULD NOT send Access-Reject/no EAP-Message
		 * attribute */
		wpa_printf(MSG_DEBUG, "could not extract "
			       "EAP-Message from RADIUS message");
		free(wpa_s->last_eap_radius);
		wpa_s->last_eap_radius = NULL;
		wpa_s->last_eap_radius_len = 0;
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

	if (wpa_s->last_eap_radius)
		free(wpa_s->last_eap_radius);
	wpa_s->last_eap_radius = eap;
	wpa_s->last_eap_radius_len = len;

	{
		struct ieee802_1x_hdr *hdr;
		hdr = malloc(sizeof(*hdr) + len);
		assert(hdr != NULL);
		hdr->version = EAPOL_VERSION;
		hdr->type = IEEE802_1X_TYPE_EAP_PACKET;
		hdr->length = htons(len);
		memcpy((u8 *) (hdr + 1), eap, len);
		eapol_sm_rx_eapol(wpa_s->eapol, wpa_s->bssid,
				  (u8 *) hdr, sizeof(*hdr) + len);
		free(hdr);
	}
}


static void ieee802_1x_get_keys(struct wpa_supplicant *wpa_s,
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
			wpa_s->authenticator_pmk_len =
				keys->recv_len > PMK_LEN ? PMK_LEN :
				keys->recv_len;
			memcpy(wpa_s->authenticator_pmk, keys->recv,
			       wpa_s->authenticator_pmk_len);
		}

		free(keys->send);
		free(keys->recv);
		free(keys);
	}
}


/* Process the RADIUS frames from Authentication Server */
static RadiusRxResult
ieee802_1x_receive_auth(struct wpa_supplicant *wpa_s,
			struct radius_msg *msg, struct radius_msg *req,
			u8 *shared_secret, size_t shared_secret_len,
			void *data)
{
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
				     req)) {
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

	wpa_s->radius_identifier = -1;
	wpa_printf(MSG_DEBUG, "RADIUS packet matching with station");

	if (wpa_s->last_recv_radius) {
		radius_msg_free(wpa_s->last_recv_radius);
		free(wpa_s->last_recv_radius);
	}

	wpa_s->last_recv_radius = msg;

	switch (msg->hdr->code) {
	case RADIUS_CODE_ACCESS_ACCEPT:
		wpa_s->radius_access_accept_received = 1;
		ieee802_1x_get_keys(wpa_s, msg, req, shared_secret,
				    shared_secret_len);
		break;
	case RADIUS_CODE_ACCESS_REJECT:
		wpa_s->radius_access_reject_received = 1;
		break;
	}

	ieee802_1x_decapsulate_radius(wpa_s);

	if ((msg->hdr->code == RADIUS_CODE_ACCESS_ACCEPT &&
	     eapol_test_num_reauths < 0) ||
	    msg->hdr->code == RADIUS_CODE_ACCESS_REJECT) {
		eloop_terminate();
	}

	return RADIUS_RX_QUEUED;
}


static void wpa_supplicant_imsi_identity(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid)
{
	int aka = 0;
	u8 *pos = ssid->eap_methods;

	while (pos && *pos != EAP_TYPE_NONE) {
		if (*pos == EAP_TYPE_AKA) {
			aka = 1;
			break;
		}
		pos++;
	}

	if (ssid->identity == NULL && wpa_s->imsi) {
		ssid->identity = malloc(1 + wpa_s->imsi_len);
		if (ssid->identity) {
			ssid->identity[0] = aka ? '0' : '1';
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
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);

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


static void wpa_init_conf(struct wpa_supplicant *wpa_s, const char *authsrv,
			  int port, const char *secret)
{
	struct hostapd_radius_server *as;
	int res;

	wpa_s->bssid[5] = 1;
	wpa_s->own_addr[5] = 2;
	wpa_s->own_ip_addr.s_addr = htonl((127 << 24) | 1);
	strncpy(wpa_s->ifname, "test", sizeof(wpa_s->ifname));

	wpa_s->num_auth_servers = 1;
	as = malloc(sizeof(struct hostapd_radius_server));
	assert(as != NULL);
	inet_aton(authsrv, &as->addr);
	as->port = port;
	as->shared_secret = (u8 *) strdup(secret);
	as->shared_secret_len = strlen(secret);
	wpa_s->auth_server = wpa_s->auth_servers = as;


	res = radius_client_init(wpa_s);
	assert(res == 0);

	res = radius_client_register(wpa_s, RADIUS_AUTH,
				     ieee802_1x_receive_auth, NULL);
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

	scard = scard_init(SCARD_TRY_BOTH, "1234");
	if (scard == NULL)
		return -1;

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

	scard = scard_init(SCARD_GSM_SIM_ONLY, argv[0]);
	if (scard == NULL) {
		printf("Failed to open smartcard connection\n");
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
	       "eapol_test [-n] -c<conf> [-a<AS IP>] [-p<AS port>] "
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
	       "  -n = no MPPE keys expected\n");
}


int main(int argc, char *argv[])
{
	struct wpa_supplicant wpa_s;
	int c, ret = 1;
	char *as_addr = "127.0.0.1";
	int as_port = 1812;
	char *as_secret = "radius";
	char *conf = NULL;

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	for (;;) {
		c = getopt(argc, argv, "a:c:np:r:s:");
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
			no_mppe_keys++;
			break;
		case 'p':
			as_port = atoi(optarg);
			break;
		case 'r':
			eapol_test_num_reauths = atoi(optarg);
			break;
		case 's':
			as_secret = optarg;
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
	wpa_s.conf = wpa_config_read(conf);
	if (wpa_s.conf == NULL) {
		printf("Failed to parse configuration file '%s'.\n", conf);
		return -1;
	}
	if (wpa_s.conf->ssid == NULL) {
		printf("No networks defined.\n");
		return -1;
	}

	wpa_init_conf(&wpa_s, as_addr, as_port, as_secret);
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
	eloop_register_timeout(0, 0, send_eap_request_identity, &wpa_s, NULL);
	eloop_register_signal(SIGINT, eapol_test_terminate, NULL);
	eloop_register_signal(SIGTERM, eapol_test_terminate, NULL);
	eloop_register_signal(SIGHUP, eapol_test_terminate, NULL);
	eloop_run();

	if (eapol_test_compare_pmk(&wpa_s) == 0)
		ret = 0;
	if (wpa_s.auth_timed_out)
		ret = -2;
	if (wpa_s.radius_access_reject_received)
		ret = -3;

	test_eapol_clean(&wpa_s);

	eloop_destroy();

	printf("MPPE keys OK: %d  mismatch: %d\n",
	       num_mppe_ok, num_mppe_mismatch);
	if (num_mppe_mismatch)
		ret = -4;
	if (ret)
		printf("FAILURE\n");
	else
		printf("SUCCESS\n");

	return ret;
}
