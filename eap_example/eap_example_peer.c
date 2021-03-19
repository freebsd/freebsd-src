/*
 * Example application showing how EAP peer code from wpa_supplicant can be
 * used as a library.
 * Copyright (c) 2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eap_peer/eap.h"
#include "eap_peer/eap_config.h"
#include "wpabuf.h"

void eap_example_server_rx(const u8 *data, size_t data_len);


struct eap_peer_ctx {
	bool eapSuccess;
	bool eapRestart;
	bool eapFail;
	bool eapResp;
	bool eapNoResp;
	bool eapReq;
	bool portEnabled;
	bool altAccept; /* for EAP */
	bool altReject; /* for EAP */
	bool eapTriggerStart;

	struct wpabuf *eapReqData; /* for EAP */

	unsigned int idleWhile; /* for EAP state machine */

	struct eap_peer_config eap_config;
	struct eap_sm *eap;
};


static struct eap_peer_ctx eap_ctx;


static struct eap_peer_config * peer_get_config(void *ctx)
{
	struct eap_peer_ctx *peer = ctx;
	return &peer->eap_config;
}


static bool peer_get_bool(void *ctx, enum eapol_bool_var variable)
{
	struct eap_peer_ctx *peer = ctx;
	if (peer == NULL)
		return false;
	switch (variable) {
	case EAPOL_eapSuccess:
		return peer->eapSuccess;
	case EAPOL_eapRestart:
		return peer->eapRestart;
	case EAPOL_eapFail:
		return peer->eapFail;
	case EAPOL_eapResp:
		return peer->eapResp;
	case EAPOL_eapNoResp:
		return peer->eapNoResp;
	case EAPOL_eapReq:
		return peer->eapReq;
	case EAPOL_portEnabled:
		return peer->portEnabled;
	case EAPOL_altAccept:
		return peer->altAccept;
	case EAPOL_altReject:
		return peer->altReject;
	case EAPOL_eapTriggerStart:
		return peer->eapTriggerStart;
	}
	return false;
}


static void peer_set_bool(void *ctx, enum eapol_bool_var variable, bool value)
{
	struct eap_peer_ctx *peer = ctx;
	if (peer == NULL)
		return;
	switch (variable) {
	case EAPOL_eapSuccess:
		peer->eapSuccess = value;
		break;
	case EAPOL_eapRestart:
		peer->eapRestart = value;
		break;
	case EAPOL_eapFail:
		peer->eapFail = value;
		break;
	case EAPOL_eapResp:
		peer->eapResp = value;
		break;
	case EAPOL_eapNoResp:
		peer->eapNoResp = value;
		break;
	case EAPOL_eapReq:
		peer->eapReq = value;
		break;
	case EAPOL_portEnabled:
		peer->portEnabled = value;
		break;
	case EAPOL_altAccept:
		peer->altAccept = value;
		break;
	case EAPOL_altReject:
		peer->altReject = value;
		break;
	case EAPOL_eapTriggerStart:
		peer->eapTriggerStart = value;
		break;
	}
}


static unsigned int peer_get_int(void *ctx, enum eapol_int_var variable)
{
	struct eap_peer_ctx *peer = ctx;
	if (peer == NULL)
		return 0;
	switch (variable) {
	case EAPOL_idleWhile:
		return peer->idleWhile;
	}
	return 0;
}


static void peer_set_int(void *ctx, enum eapol_int_var variable,
			 unsigned int value)
{
	struct eap_peer_ctx *peer = ctx;
	if (peer == NULL)
		return;
	switch (variable) {
	case EAPOL_idleWhile:
		peer->idleWhile = value;
		break;
	}
}


static struct wpabuf * peer_get_eapReqData(void *ctx)
{
	struct eap_peer_ctx *peer = ctx;
	if (peer == NULL || peer->eapReqData == NULL)
		return NULL;

	return peer->eapReqData;
}


static void peer_set_config_blob(void *ctx, struct wpa_config_blob *blob)
{
	printf("TODO: %s\n", __func__);
}


static const struct wpa_config_blob *
peer_get_config_blob(void *ctx, const char *name)
{
	printf("TODO: %s\n", __func__);
	return NULL;
}


static void peer_notify_pending(void *ctx)
{
	printf("TODO: %s\n", __func__);
}


static int eap_peer_register_methods(void)
{
	int ret = 0;

#ifdef EAP_MD5
	if (ret == 0)
		ret = eap_peer_md5_register();
#endif /* EAP_MD5 */

#ifdef EAP_TLS
	if (ret == 0)
		ret = eap_peer_tls_register();
#endif /* EAP_TLS */

#ifdef EAP_MSCHAPv2
	if (ret == 0)
		ret = eap_peer_mschapv2_register();
#endif /* EAP_MSCHAPv2 */

#ifdef EAP_PEAP
	if (ret == 0)
		ret = eap_peer_peap_register();
#endif /* EAP_PEAP */

#ifdef EAP_TTLS
	if (ret == 0)
		ret = eap_peer_ttls_register();
#endif /* EAP_TTLS */

#ifdef EAP_GTC
	if (ret == 0)
		ret = eap_peer_gtc_register();
#endif /* EAP_GTC */

#ifdef EAP_OTP
	if (ret == 0)
		ret = eap_peer_otp_register();
#endif /* EAP_OTP */

#ifdef EAP_SIM
	if (ret == 0)
		ret = eap_peer_sim_register();
#endif /* EAP_SIM */

#ifdef EAP_LEAP
	if (ret == 0)
		ret = eap_peer_leap_register();
#endif /* EAP_LEAP */

#ifdef EAP_PSK
	if (ret == 0)
		ret = eap_peer_psk_register();
#endif /* EAP_PSK */

#ifdef EAP_AKA
	if (ret == 0)
		ret = eap_peer_aka_register();
#endif /* EAP_AKA */

#ifdef EAP_AKA_PRIME
	if (ret == 0)
		ret = eap_peer_aka_prime_register();
#endif /* EAP_AKA_PRIME */

#ifdef EAP_FAST
	if (ret == 0)
		ret = eap_peer_fast_register();
#endif /* EAP_FAST */

#ifdef EAP_PAX
	if (ret == 0)
		ret = eap_peer_pax_register();
#endif /* EAP_PAX */

#ifdef EAP_SAKE
	if (ret == 0)
		ret = eap_peer_sake_register();
#endif /* EAP_SAKE */

#ifdef EAP_GPSK
	if (ret == 0)
		ret = eap_peer_gpsk_register();
#endif /* EAP_GPSK */

#ifdef EAP_WSC
	if (ret == 0)
		ret = eap_peer_wsc_register();
#endif /* EAP_WSC */

#ifdef EAP_IKEV2
	if (ret == 0)
		ret = eap_peer_ikev2_register();
#endif /* EAP_IKEV2 */

#ifdef EAP_VENDOR_TEST
	if (ret == 0)
		ret = eap_peer_vendor_test_register();
#endif /* EAP_VENDOR_TEST */

#ifdef EAP_TNC
	if (ret == 0)
		ret = eap_peer_tnc_register();
#endif /* EAP_TNC */

	return ret;
}


static struct eapol_callbacks eap_cb;
static struct eap_config eap_conf;

int eap_example_peer_init(void)
{
	if (eap_peer_register_methods() < 0)
		return -1;

	os_memset(&eap_ctx, 0, sizeof(eap_ctx));

	eap_ctx.eap_config.identity = (u8 *) os_strdup("user");
	eap_ctx.eap_config.identity_len = 4;
	eap_ctx.eap_config.password = (u8 *) os_strdup("password");
	eap_ctx.eap_config.password_len = 8;
	eap_ctx.eap_config.cert.ca_cert = os_strdup("ca.pem");
	eap_ctx.eap_config.fragment_size = 1398;

	os_memset(&eap_cb, 0, sizeof(eap_cb));
	eap_cb.get_config = peer_get_config;
	eap_cb.get_bool = peer_get_bool;
	eap_cb.set_bool = peer_set_bool;
	eap_cb.get_int = peer_get_int;
	eap_cb.set_int = peer_set_int;
	eap_cb.get_eapReqData = peer_get_eapReqData;
	eap_cb.set_config_blob = peer_set_config_blob;
	eap_cb.get_config_blob = peer_get_config_blob;
	eap_cb.notify_pending = peer_notify_pending;

	os_memset(&eap_conf, 0, sizeof(eap_conf));
	eap_ctx.eap = eap_peer_sm_init(&eap_ctx, &eap_cb, &eap_ctx, &eap_conf);
	if (eap_ctx.eap == NULL)
		return -1;

	/* Enable "port" to allow authentication */
	eap_ctx.portEnabled = true;

	return 0;
}


void eap_example_peer_deinit(void)
{
	eap_peer_sm_deinit(eap_ctx.eap);
	eap_peer_unregister_methods();
	wpabuf_free(eap_ctx.eapReqData);
	os_free(eap_ctx.eap_config.identity);
	os_free(eap_ctx.eap_config.password);
	os_free(eap_ctx.eap_config.cert.ca_cert);
}


int eap_example_peer_step(void)
{
	int res;
	res = eap_peer_sm_step(eap_ctx.eap);

	if (eap_ctx.eapResp) {
		struct wpabuf *resp;
		printf("==> Response\n");
		eap_ctx.eapResp = false;
		resp = eap_get_eapRespData(eap_ctx.eap);
		if (resp) {
			/* Send EAP response to the server */
			eap_example_server_rx(wpabuf_head(resp),
					      wpabuf_len(resp));
			wpabuf_free(resp);
		}
	}

	if (eap_ctx.eapSuccess) {
		res = 0;
		if (eap_key_available(eap_ctx.eap)) {
			const u8 *key;
			size_t key_len;
			key = eap_get_eapKeyData(eap_ctx.eap, &key_len);
			wpa_hexdump(MSG_DEBUG, "EAP keying material",
				    key, key_len);
		}
	}

	return res;
}


void eap_example_peer_rx(const u8 *data, size_t data_len)
{
	/* Make received EAP message available to the EAP library */
	eap_ctx.eapReq = true;
	wpabuf_free(eap_ctx.eapReqData);
	eap_ctx.eapReqData = wpabuf_alloc_copy(data, data_len);
}
