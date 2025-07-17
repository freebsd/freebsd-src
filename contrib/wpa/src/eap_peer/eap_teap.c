/*
 * EAP peer method: EAP-TEAP (RFC 7170)
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/tls.h"
#include "eap_common/eap_teap_common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "eap_config.h"
#include "eap_teap_pac.h"

#ifdef EAP_TEAP_DYNAMIC
#include "eap_teap_pac.c"
#endif /* EAP_TEAP_DYNAMIC */


static void eap_teap_deinit(struct eap_sm *sm, void *priv);


struct eap_teap_data {
	struct eap_ssl_data ssl;

	u8 teap_version; /* Negotiated version */
	u8 received_version; /* Version number received during negotiation */
	u16 tls_cs;

	const struct eap_method *phase2_method;
	void *phase2_priv;
	int phase2_success;
	int inner_method_done;
	int iresult_verified;
	int result_success_done;
	int on_tx_completion;

	struct eap_method_type phase2_type;
	struct eap_method_type *phase2_types;
	size_t num_phase2_types;
	int resuming; /* starting a resumed session */
#define EAP_TEAP_PROV_UNAUTH 1
#define EAP_TEAP_PROV_AUTH 2
	int provisioning_allowed; /* Allowed PAC provisioning modes */
	int provisioning; /* doing PAC provisioning (not the normal auth) */
	int anon_provisioning; /* doing anonymous (unauthenticated)
				* provisioning */
	int session_ticket_used;
	int test_outer_tlvs;

	u8 key_data[EAP_TEAP_KEY_LEN];
	u8 *session_id;
	size_t id_len;
	u8 emsk[EAP_EMSK_LEN];
	int success;

	struct eap_teap_pac *pac;
	struct eap_teap_pac *current_pac;
	size_t max_pac_list_len;
	int use_pac_binary_format;

	u8 simck_msk[EAP_TEAP_SIMCK_LEN];
	u8 simck_emsk[EAP_TEAP_SIMCK_LEN];
	int simck_idx;
	int cmk_emsk_available;

	struct wpabuf *pending_phase2_req;
	struct wpabuf *pending_resp;
	struct wpabuf *server_outer_tlvs;
	struct wpabuf *peer_outer_tlvs;
};


static int eap_teap_session_ticket_cb(void *ctx, const u8 *ticket, size_t len,
				      const u8 *client_random,
				      const u8 *server_random,
				      u8 *master_secret)
{
	struct eap_teap_data *data = ctx;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: SessionTicket callback");

	if (!master_secret) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: SessionTicket failed - fall back to full TLS handshake");
		data->session_ticket_used = 0;
		if (data->provisioning_allowed) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Try to provision a new PAC-Key");
			data->provisioning = 1;
			data->current_pac = NULL;
		}
		return 0;
	}

	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: SessionTicket", ticket, len);

	if (!data->current_pac) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No PAC-Key available for using SessionTicket");
		data->session_ticket_used = 0;
		return 0;
	}

	/* EAP-TEAP uses PAC-Key as the TLS master_secret */
	os_memcpy(master_secret, data->current_pac->pac_key,
		  EAP_TEAP_PAC_KEY_LEN);

	data->session_ticket_used = 1;

	return 1;
}


static void eap_teap_parse_phase1(struct eap_teap_data *data,
				  const char *phase1)
{
	const char *pos;

	pos = os_strstr(phase1, "teap_provisioning=");
	if (pos) {
		data->provisioning_allowed = atoi(pos + 18);
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Automatic PAC provisioning mode: %d",
			   data->provisioning_allowed);
	}

	pos = os_strstr(phase1, "teap_max_pac_list_len=");
	if (pos) {
		data->max_pac_list_len = atoi(pos + 22);
		if (data->max_pac_list_len == 0)
			data->max_pac_list_len = 1;
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Maximum PAC list length: %lu",
			   (unsigned long) data->max_pac_list_len);
	}

	if (os_strstr(phase1, "teap_pac_format=binary")) {
		data->use_pac_binary_format = 1;
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Using binary format for PAC list");
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (os_strstr(phase1, "teap_test_outer_tlvs=1"))
		data->test_outer_tlvs = 1;
#endif /* CONFIG_TESTING_OPTIONS */
}


static void * eap_teap_init(struct eap_sm *sm)
{
	struct eap_teap_data *data;
	struct eap_peer_config *config = eap_get_config(sm);

	if (!config)
		return NULL;

	data = os_zalloc(sizeof(*data));
	if (!data)
		return NULL;
	data->teap_version = EAP_TEAP_VERSION;
	data->max_pac_list_len = 10;

	if (config->phase1)
		eap_teap_parse_phase1(data, config->phase1);

	if ((data->provisioning_allowed & EAP_TEAP_PROV_AUTH) &&
	    !config->cert.ca_cert && !config->cert.ca_path) {
		/* Prevent PAC provisioning without mutual authentication
		 * (either by validating server certificate or by suitable
		 * inner EAP method). */
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Disable authenticated provisioning due to no ca_cert/ca_path");
		data->provisioning_allowed &= ~EAP_TEAP_PROV_AUTH;
	}

	if (eap_peer_select_phase2_methods(config, "auth=",
					   &data->phase2_types,
					   &data->num_phase2_types, 0) < 0) {
		eap_teap_deinit(sm, data);
		return NULL;
	}

	data->phase2_type.vendor = EAP_VENDOR_IETF;
	data->phase2_type.method = EAP_TYPE_NONE;

	config->teap_anon_dh = !!(data->provisioning_allowed &
				  EAP_TEAP_PROV_UNAUTH);
	if (eap_peer_tls_ssl_init(sm, &data->ssl, config, EAP_TYPE_TEAP)) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Failed to initialize SSL");
		eap_teap_deinit(sm, data);
		return NULL;
	}

	if (tls_connection_set_session_ticket_cb(sm->ssl_ctx, data->ssl.conn,
						 eap_teap_session_ticket_cb,
						 data) < 0) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Failed to set SessionTicket callback");
		eap_teap_deinit(sm, data);
		return NULL;
	}

	if (!config->pac_file) {
		wpa_printf(MSG_INFO, "EAP-TEAP: No PAC file configured");
		eap_teap_deinit(sm, data);
		return NULL;
	}

	if (data->use_pac_binary_format &&
	    eap_teap_load_pac_bin(sm, &data->pac, config->pac_file) < 0) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Failed to load PAC file");
		eap_teap_deinit(sm, data);
		return NULL;
	}

	if (!data->use_pac_binary_format &&
	    eap_teap_load_pac(sm, &data->pac, config->pac_file) < 0) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Failed to load PAC file");
		eap_teap_deinit(sm, data);
		return NULL;
	}
	eap_teap_pac_list_truncate(data->pac, data->max_pac_list_len);

	return data;
}


static void eap_teap_clear(struct eap_teap_data *data)
{
	forced_memzero(data->key_data, EAP_TEAP_KEY_LEN);
	forced_memzero(data->emsk, EAP_EMSK_LEN);
	os_free(data->session_id);
	data->session_id = NULL;
	wpabuf_free(data->pending_phase2_req);
	data->pending_phase2_req = NULL;
	wpabuf_free(data->pending_resp);
	data->pending_resp = NULL;
	wpabuf_free(data->server_outer_tlvs);
	data->server_outer_tlvs = NULL;
	wpabuf_free(data->peer_outer_tlvs);
	data->peer_outer_tlvs = NULL;
	forced_memzero(data->simck_msk, EAP_TEAP_SIMCK_LEN);
	forced_memzero(data->simck_emsk, EAP_TEAP_SIMCK_LEN);
}


static void eap_teap_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;
	struct eap_teap_pac *pac, *prev;

	if (!data)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->deinit(sm, data->phase2_priv);
	eap_teap_clear(data);
	os_free(data->phase2_types);
	eap_peer_tls_ssl_deinit(sm, &data->ssl);

	pac = data->pac;
	prev = NULL;
	while (pac) {
		prev = pac;
		pac = pac->next;
		eap_teap_free_pac(prev);
	}

	os_free(data);
}


static int eap_teap_derive_msk(struct eap_teap_data *data)
{
	/* FIX: RFC 7170 does not describe whether MSK or EMSK based S-IMCK[j]
	 * is used in this derivation */
	if (eap_teap_derive_eap_msk(data->tls_cs, data->simck_msk,
				    data->key_data) < 0 ||
	    eap_teap_derive_eap_emsk(data->tls_cs, data->simck_msk,
				     data->emsk) < 0)
		return -1;
	data->success = 1;
	return 0;
}


static int eap_teap_derive_key_auth(struct eap_sm *sm,
				    struct eap_teap_data *data)
{
	int res;

	/* RFC 7170, Section 5.1 */
	res = tls_connection_export_key(sm->ssl_ctx, data->ssl.conn,
					TEAP_TLS_EXPORTER_LABEL_SKS, NULL, 0,
					data->simck_msk, EAP_TEAP_SIMCK_LEN);
	if (res)
		return res;
	wpa_hexdump_key(MSG_DEBUG,
			"EAP-TEAP: session_key_seed (S-IMCK[0])",
			data->simck_msk, EAP_TEAP_SIMCK_LEN);
	os_memcpy(data->simck_emsk, data->simck_msk, EAP_TEAP_SIMCK_LEN);
	data->simck_idx = 0;
	return 0;
}


static int eap_teap_init_phase2_method(struct eap_sm *sm,
				       struct eap_teap_data *data)
{
	data->inner_method_done = 0;
	data->iresult_verified = 0;
	data->phase2_method =
		eap_peer_get_eap_method(data->phase2_type.vendor,
					data->phase2_type.method);
	if (!data->phase2_method)
		return -1;

	/* While RFC 7170 does not describe this, EAP-TEAP has been deployed
	 * with implementations that use the EAP-FAST-MSCHAPv2, instead of the
	 * EAP-MSCHAPv2, way of deriving the MSK for IMSK. Use that design here
	 * to interoperate.
	 */
	sm->eap_fast_mschapv2 = true;

	sm->init_phase2 = 1;
	data->phase2_priv = data->phase2_method->init(sm);
	sm->init_phase2 = 0;

	return data->phase2_priv == NULL ? -1 : 0;
}


static int eap_teap_select_phase2_method(struct eap_teap_data *data,
					 int vendor, enum eap_type type)
{
	size_t i;

	/* TODO: TNC with anonymous provisioning; need to require both
	 * completed inner EAP authentication (EAP-pwd or EAP-EKE) and TNC */

	if (data->anon_provisioning &&
	    !eap_teap_allowed_anon_prov_phase2_method(vendor, type)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: EAP type %u:%u not allowed during unauthenticated provisioning",
			   vendor, type);
		return -1;
	}

#ifdef EAP_TNC
	if (vendor == EAP_VENDOR_IETF && type == EAP_TYPE_TNC) {
		data->phase2_type.vendor = EAP_VENDOR_IETF;
		data->phase2_type.method = EAP_TYPE_TNC;
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Selected Phase 2 EAP vendor %d method %d for TNC",
			   data->phase2_type.vendor,
			   data->phase2_type.method);
		return 0;
	}
#endif /* EAP_TNC */

	for (i = 0; i < data->num_phase2_types; i++) {
		if (data->phase2_types[i].vendor != vendor ||
		    data->phase2_types[i].method != type)
			continue;

		data->phase2_type.vendor = data->phase2_types[i].vendor;
		data->phase2_type.method = data->phase2_types[i].method;
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Selected Phase 2 EAP vendor %d method %d",
			   data->phase2_type.vendor,
			   data->phase2_type.method);
		break;
	}

	if (vendor != data->phase2_type.vendor ||
	    type != data->phase2_type.method ||
	    (vendor == EAP_VENDOR_IETF && type == EAP_TYPE_NONE))
		return -1;

	return 0;
}


static void eap_teap_deinit_inner_eap(struct eap_sm *sm,
				      struct eap_teap_data *data)
{
	if (!data->phase2_priv || !data->phase2_method)
		return;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Phase 2 EAP sequence - deinitialize previous method");
	data->phase2_method->deinit(sm, data->phase2_priv);
	data->phase2_method = NULL;
	data->phase2_priv = NULL;
	data->phase2_type.vendor = EAP_VENDOR_IETF;
	data->phase2_type.method = EAP_TYPE_NONE;
}


static int eap_teap_phase2_request(struct eap_sm *sm,
				   struct eap_teap_data *data,
				   struct eap_method_ret *ret,
				   struct eap_hdr *hdr,
				   struct wpabuf **resp)
{
	size_t len = be_to_host16(hdr->length);
	u8 *pos;
	struct eap_method_ret iret;
	struct eap_peer_config *config = eap_get_config(sm);
	struct wpabuf msg;
	int vendor = EAP_VENDOR_IETF;
	enum eap_type method;

	if (len <= sizeof(struct eap_hdr)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: too short Phase 2 request (len=%lu)",
			   (unsigned long) len);
		return -1;
	}
	pos = (u8 *) (hdr + 1);
	method = *pos;
	if (method == EAP_TYPE_EXPANDED) {
		if (len < sizeof(struct eap_hdr) + 8) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short Phase 2 request (expanded header) (len=%lu)",
				   (unsigned long) len);
			return -1;
		}
		vendor = WPA_GET_BE24(pos + 1);
		method = WPA_GET_BE32(pos + 4);
	}
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Phase 2 Request: type=%u:%u",
		   vendor, method);
	if (vendor == EAP_VENDOR_IETF && method == EAP_TYPE_IDENTITY) {
		eap_teap_deinit_inner_eap(sm, data);
		*resp = eap_sm_buildIdentity(sm, hdr->identifier, 1);
		return 0;
	}

	if (data->phase2_priv && data->phase2_method &&
	    (vendor != data->phase2_type.vendor ||
	     method != data->phase2_type.method))
		eap_teap_deinit_inner_eap(sm, data);

	if (data->phase2_type.vendor == EAP_VENDOR_IETF &&
	    data->phase2_type.method == EAP_TYPE_NONE &&
	    eap_teap_select_phase2_method(data, vendor, method) < 0) {
		if (eap_peer_tls_phase2_nak(data->phase2_types,
					    data->num_phase2_types,
					    hdr, resp))
			return -1;
		return 0;
	}

	if ((!data->phase2_priv && eap_teap_init_phase2_method(sm, data) < 0) ||
	    !data->phase2_method) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Failed to initialize Phase 2 EAP method %u:%u",
			   vendor, method);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		return -1;
	}

	os_memset(&iret, 0, sizeof(iret));
	wpabuf_set(&msg, hdr, len);
	*resp = data->phase2_method->process(sm, data->phase2_priv, &iret,
					     &msg);
	if (iret.methodState == METHOD_DONE)
		data->inner_method_done = 1;
	if (!(*resp) ||
	    (iret.methodState == METHOD_DONE &&
	     iret.decision == DECISION_FAIL)) {
		/* Wait for protected indication of failure */
		ret->methodState = METHOD_MAY_CONT;
		ret->decision = DECISION_FAIL;
	} else if ((iret.methodState == METHOD_DONE ||
		    iret.methodState == METHOD_MAY_CONT) &&
		   (iret.decision == DECISION_UNCOND_SUCC ||
		    iret.decision == DECISION_COND_SUCC)) {
		data->phase2_success = 1;
	}

	if (!(*resp) && config &&
	    (config->pending_req_identity || config->pending_req_password ||
	     config->pending_req_otp || config->pending_req_new_password ||
	     config->pending_req_sim)) {
		wpabuf_free(data->pending_phase2_req);
		data->pending_phase2_req = wpabuf_alloc_copy(hdr, len);
	} else if (!(*resp))
		return -1;

	return 0;
}


static struct wpabuf * eap_teap_tlv_nak(int vendor_id, int tlv_type)
{
	struct wpabuf *buf;
	struct teap_tlv_nak *nak;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Add NAK TLV (Vendor-Id %u NAK-Type %u)",
		   vendor_id, tlv_type);
	buf = wpabuf_alloc(sizeof(*nak));
	if (!buf)
		return NULL;
	nak = wpabuf_put(buf, sizeof(*nak));
	nak->tlv_type = host_to_be16(TEAP_TLV_MANDATORY | TEAP_TLV_NAK);
	nak->length = host_to_be16(6);
	nak->vendor_id = host_to_be32(vendor_id);
	nak->nak_type = host_to_be16(tlv_type);
	return buf;
}


static struct wpabuf * eap_teap_tlv_pac_ack(void)
{
	struct wpabuf *buf;
	struct teap_tlv_result *res;
	struct teap_tlv_pac_ack *ack;

	buf = wpabuf_alloc(sizeof(*res) + sizeof(*ack));
	if (!buf)
		return NULL;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add PAC TLV (ack)");
	ack = wpabuf_put(buf, sizeof(*ack));
	ack->tlv_type = host_to_be16(TEAP_TLV_PAC | TEAP_TLV_MANDATORY);
	ack->length = host_to_be16(sizeof(*ack) - sizeof(struct teap_tlv_hdr));
	ack->pac_type = host_to_be16(PAC_TYPE_PAC_ACKNOWLEDGEMENT);
	ack->pac_len = host_to_be16(2);
	ack->result = host_to_be16(TEAP_STATUS_SUCCESS);

	return buf;
}


static struct wpabuf * eap_teap_add_identity_type(struct eap_sm *sm,
						  struct wpabuf *msg)
{
	struct wpabuf *tlv;

	tlv = eap_teap_tlv_identity_type(sm->use_machine_cred ?
					 TEAP_IDENTITY_TYPE_MACHINE :
					 TEAP_IDENTITY_TYPE_USER);
	return wpabuf_concat(msg, tlv);
}


static struct wpabuf * eap_teap_process_eap_payload_tlv(
	struct eap_sm *sm, struct eap_teap_data *data,
	struct eap_method_ret *ret,
	u8 *eap_payload_tlv, size_t eap_payload_tlv_len,
	enum teap_identity_types req_id_type)
{
	struct eap_hdr *hdr;
	struct wpabuf *resp = NULL;

	if (eap_payload_tlv_len < sizeof(*hdr)) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: too short EAP Payload TLV (len=%lu)",
			   (unsigned long) eap_payload_tlv_len);
		return NULL;
	}

	hdr = (struct eap_hdr *) eap_payload_tlv;
	if (be_to_host16(hdr->length) > eap_payload_tlv_len) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: EAP packet overflow in EAP Payload TLV");
		return NULL;
	}

	if (hdr->code != EAP_CODE_REQUEST) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Unexpected code=%d in Phase 2 EAP header",
			   hdr->code);
		return NULL;
	}

	if (eap_teap_phase2_request(sm, data, ret, hdr, &resp)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Phase 2 Request processing failed");
		return NULL;
	}

	resp = eap_teap_tlv_eap_payload(resp);
	if (req_id_type)
		resp = eap_teap_add_identity_type(sm, resp);

	return resp;
}


static struct wpabuf * eap_teap_process_basic_auth_req(
	struct eap_sm *sm, struct eap_teap_data *data,
	u8 *basic_auth_req, size_t basic_auth_req_len,
	enum teap_identity_types req_id_type)
{
	const u8 *identity, *password;
	size_t identity_len, password_len, plen;
	struct wpabuf *resp;

	wpa_hexdump_ascii(MSG_DEBUG, "EAP-TEAP: Basic-Password-Auth-Req prompt",
			  basic_auth_req, basic_auth_req_len);
	/* TODO: send over control interface */

	identity = eap_get_config_identity(sm, &identity_len);
	password = eap_get_config_password(sm, &password_len);
	if (!identity || !password ||
	    identity_len > 255 || password_len > 255) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No username/password suitable for Basic-Password-Auth");
		return eap_teap_tlv_nak(0, TEAP_TLV_BASIC_PASSWORD_AUTH_REQ);
	}

	plen = 1 + identity_len + 1 + password_len;
	resp = wpabuf_alloc(sizeof(struct teap_tlv_hdr) + plen);
	if (!resp)
		return NULL;
	eap_teap_put_tlv_hdr(resp, TEAP_TLV_BASIC_PASSWORD_AUTH_RESP, plen);
	wpabuf_put_u8(resp, identity_len);
	wpabuf_put_data(resp, identity, identity_len);
	wpabuf_put_u8(resp, password_len);
	wpabuf_put_data(resp, password, password_len);
	wpa_hexdump_buf_key(MSG_DEBUG, "EAP-TEAP: Basic-Password-Auth-Resp",
			    resp);
	if (req_id_type)
		resp = eap_teap_add_identity_type(sm, resp);

	/* Assume this succeeds so that Result TLV(Success) from the server can
	 * be used to terminate TEAP. */
	data->phase2_success = 1;

	return resp;
}


static int
eap_teap_validate_crypto_binding(struct eap_teap_data *data,
				 const struct teap_tlv_crypto_binding *cb)
{
	u8 flags, subtype;

	subtype = cb->subtype & 0x0f;
	flags = cb->subtype >> 4;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Crypto-Binding TLV: Version %u Received Version %u Flags %u Sub-Type %u",
		   cb->version, cb->received_version, flags, subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Nonce",
		    cb->nonce, sizeof(cb->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: EMSK Compound MAC",
		    cb->emsk_compound_mac, sizeof(cb->emsk_compound_mac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: MSK Compound MAC",
		    cb->msk_compound_mac, sizeof(cb->msk_compound_mac));

	if (cb->version != EAP_TEAP_VERSION ||
	    cb->received_version != data->received_version ||
	    subtype != TEAP_CRYPTO_BINDING_SUBTYPE_REQUEST ||
	    flags < 1 || flags > 3) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Invalid Version/Flags/Sub-Type in Crypto-Binding TLV: Version %u Received Version %u Flags %u Sub-Type %u",
			   cb->version, cb->received_version, flags, subtype);
		return -1;
	}

	if (cb->nonce[EAP_TEAP_NONCE_LEN - 1] & 0x01) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Invalid Crypto-Binding TLV Nonce in request");
		return -1;
	}

	return 0;
}


static int eap_teap_write_crypto_binding(
	struct eap_teap_data *data,
	struct teap_tlv_crypto_binding *rbind,
	const struct teap_tlv_crypto_binding *cb,
	const u8 *cmk_msk, const u8 *cmk_emsk)
{
	u8 subtype, flags;

	rbind->tlv_type = host_to_be16(TEAP_TLV_MANDATORY |
				       TEAP_TLV_CRYPTO_BINDING);
	rbind->length = host_to_be16(sizeof(*rbind) -
				     sizeof(struct teap_tlv_hdr));
	rbind->version = EAP_TEAP_VERSION;
	rbind->received_version = data->received_version;
	/* FIX: RFC 7170 is not clear on which Flags value to use when
	 * Crypto-Binding TLV is used with Basic-Password-Auth */
	flags = cmk_emsk ? TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC :
		TEAP_CRYPTO_BINDING_MSK_CMAC;
	subtype = TEAP_CRYPTO_BINDING_SUBTYPE_RESPONSE;
	rbind->subtype = (flags << 4) | subtype;
	os_memcpy(rbind->nonce, cb->nonce, sizeof(cb->nonce));
	inc_byte_array(rbind->nonce, sizeof(rbind->nonce));
	os_memset(rbind->emsk_compound_mac, 0, EAP_TEAP_COMPOUND_MAC_LEN);
	os_memset(rbind->msk_compound_mac, 0, EAP_TEAP_COMPOUND_MAC_LEN);

	if (eap_teap_compound_mac(data->tls_cs, rbind, data->server_outer_tlvs,
				  data->peer_outer_tlvs, cmk_msk,
				  rbind->msk_compound_mac) < 0)
		return -1;
	if (cmk_emsk &&
	    eap_teap_compound_mac(data->tls_cs, rbind, data->server_outer_tlvs,
				  data->peer_outer_tlvs, cmk_emsk,
				  rbind->emsk_compound_mac) < 0)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Reply Crypto-Binding TLV: Version %u Received Version %u Flags %u SubType %u",
		   rbind->version, rbind->received_version, flags, subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Nonce",
		    rbind->nonce, sizeof(rbind->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: EMSK Compound MAC",
		    rbind->emsk_compound_mac, sizeof(rbind->emsk_compound_mac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: MSK Compound MAC",
		    rbind->msk_compound_mac, sizeof(rbind->msk_compound_mac));

	return 0;
}


static int eap_teap_get_cmk(struct eap_sm *sm, struct eap_teap_data *data,
			    u8 *cmk_msk, u8 *cmk_emsk)
{
	u8 *msk = NULL, *emsk = NULL;
	size_t msk_len = 0, emsk_len = 0;
	int res;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Determining CMK[%d] for Compound MAC calculation",
		   data->simck_idx + 1);

	if (!data->phase2_method)
		return eap_teap_derive_cmk_basic_pw_auth(data->tls_cs,
							 data->simck_msk,
							 cmk_msk);

	if (!data->phase2_method || !data->phase2_priv) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Phase 2 method not available");
		return -1;
	}

	if (data->phase2_method->isKeyAvailable &&
	    !data->phase2_method->isKeyAvailable(sm, data->phase2_priv)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Phase 2 key material not available");
		return -1;
	}

	if (data->phase2_method->isKeyAvailable &&
	    data->phase2_method->getKey) {
		msk = data->phase2_method->getKey(sm, data->phase2_priv,
						  &msk_len);
		if (!msk) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Could not fetch Phase 2 MSK");
			return -1;
		}
	}

	if (data->phase2_method->isKeyAvailable &&
	    data->phase2_method->get_emsk) {
		emsk = data->phase2_method->get_emsk(sm, data->phase2_priv,
						     &emsk_len);
	}

	res = eap_teap_derive_imck(data->tls_cs,
				   data->simck_msk, data->simck_emsk,
				   msk, msk_len, emsk, emsk_len,
				   data->simck_msk, cmk_msk,
				   data->simck_emsk, cmk_emsk);
	bin_clear_free(msk, msk_len);
	bin_clear_free(emsk, emsk_len);
	if (res == 0) {
		data->simck_idx++;
		if (emsk)
			data->cmk_emsk_available = 1;
	}
	return res;
}


static int eap_teap_session_id(struct eap_teap_data *data)
{
	const size_t max_id_len = 100;
	int res;

	os_free(data->session_id);
	data->session_id = os_malloc(max_id_len);
	if (!data->session_id)
		return -1;

	data->session_id[0] = EAP_TYPE_TEAP;
	res = tls_get_tls_unique(data->ssl.conn, data->session_id + 1,
				 max_id_len - 1);
	if (res < 0) {
		os_free(data->session_id);
		data->session_id = NULL;
		wpa_printf(MSG_ERROR, "EAP-TEAP: Failed to derive Session-Id");
		return -1;
	}

	data->id_len = 1 + res;
	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: Derived Session-Id",
		    data->session_id, data->id_len);
	return 0;
}


static struct wpabuf * eap_teap_process_crypto_binding(
	struct eap_sm *sm, struct eap_teap_data *data,
	struct eap_method_ret *ret,
	const struct teap_tlv_crypto_binding *cb, size_t bind_len)
{
	struct wpabuf *resp;
	u8 *pos;
	u8 cmk_msk[EAP_TEAP_CMK_LEN];
	u8 cmk_emsk[EAP_TEAP_CMK_LEN];
	const u8 *cmk_emsk_ptr = NULL;
	int res;
	size_t len;
	u8 flags;

	if (eap_teap_validate_crypto_binding(data, cb) < 0 ||
	    eap_teap_get_cmk(sm, data, cmk_msk, cmk_emsk) < 0)
		return NULL;

	/* Validate received MSK/EMSK Compound MAC */
	flags = cb->subtype >> 4;

	if (flags == TEAP_CRYPTO_BINDING_MSK_CMAC ||
	    flags == TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC) {
		u8 msk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];

		if (eap_teap_compound_mac(data->tls_cs, cb,
					  data->server_outer_tlvs,
					  data->peer_outer_tlvs, cmk_msk,
					  msk_compound_mac) < 0)
			return NULL;
		res = os_memcmp_const(msk_compound_mac, cb->msk_compound_mac,
				      EAP_TEAP_COMPOUND_MAC_LEN);
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Received MSK Compound MAC",
			    cb->msk_compound_mac, EAP_TEAP_COMPOUND_MAC_LEN);
		wpa_hexdump(MSG_MSGDUMP,
			    "EAP-TEAP: Calculated MSK Compound MAC",
			    msk_compound_mac, EAP_TEAP_COMPOUND_MAC_LEN);
		if (res != 0) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: MSK Compound MAC did not match");
			return NULL;
		}
	}

	if ((flags == TEAP_CRYPTO_BINDING_EMSK_CMAC ||
	     flags == TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC) &&
	    data->cmk_emsk_available) {
		u8 emsk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];

		if (eap_teap_compound_mac(data->tls_cs, cb,
					  data->server_outer_tlvs,
					  data->peer_outer_tlvs, cmk_emsk,
					  emsk_compound_mac) < 0)
			return NULL;
		res = os_memcmp_const(emsk_compound_mac, cb->emsk_compound_mac,
				      EAP_TEAP_COMPOUND_MAC_LEN);
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Received EMSK Compound MAC",
			    cb->emsk_compound_mac, EAP_TEAP_COMPOUND_MAC_LEN);
		wpa_hexdump(MSG_MSGDUMP,
			    "EAP-TEAP: Calculated EMSK Compound MAC",
			    emsk_compound_mac, EAP_TEAP_COMPOUND_MAC_LEN);
		if (res != 0) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: EMSK Compound MAC did not match");
			return NULL;
		}

		cmk_emsk_ptr = cmk_emsk;
	}

	if (flags == TEAP_CRYPTO_BINDING_EMSK_CMAC &&
	    !data->cmk_emsk_available) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Server included only EMSK Compound MAC, but no locally generated inner EAP EMSK to validate this");
		return NULL;
	}

	/*
	 * Compound MAC was valid, so authentication succeeded. Reply with
	 * crypto binding to allow server to complete authentication.
	 */

	len = sizeof(struct teap_tlv_crypto_binding);
	resp = wpabuf_alloc(len);
	if (!resp)
		return NULL;

	if (data->phase2_success && eap_teap_derive_msk(data) < 0) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Failed to generate MSK");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		data->phase2_success = 0;
		wpabuf_free(resp);
		return NULL;
	}

	if (data->phase2_success && eap_teap_session_id(data) < 0) {
		wpabuf_free(resp);
		return NULL;
	}

	pos = wpabuf_put(resp, sizeof(struct teap_tlv_crypto_binding));
	if (eap_teap_write_crypto_binding(
		    data, (struct teap_tlv_crypto_binding *) pos,
		    cb, cmk_msk, cmk_emsk_ptr) < 0) {
		wpabuf_free(resp);
		return NULL;
	}

	return resp;
}


static void eap_teap_parse_pac_tlv(struct eap_teap_pac *entry, int type,
				   u8 *pos, size_t len, int *pac_key_found)
{
	switch (type & 0x7fff) {
	case PAC_TYPE_PAC_KEY:
		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: PAC-Key", pos, len);
		if (len != EAP_TEAP_PAC_KEY_LEN) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Invalid PAC-Key length %lu",
				   (unsigned long) len);
			break;
		}
		*pac_key_found = 1;
		os_memcpy(entry->pac_key, pos, len);
		break;
	case PAC_TYPE_PAC_OPAQUE:
		wpa_hexdump(MSG_DEBUG, "EAP-TEAP: PAC-Opaque", pos, len);
		entry->pac_opaque = pos;
		entry->pac_opaque_len = len;
		break;
	case PAC_TYPE_PAC_INFO:
		wpa_hexdump(MSG_DEBUG, "EAP-TEAP: PAC-Info", pos, len);
		entry->pac_info = pos;
		entry->pac_info_len = len;
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Ignored unknown PAC type %d",
			   type);
		break;
	}
}


static int eap_teap_process_pac_tlv(struct eap_teap_pac *entry,
				    u8 *pac, size_t pac_len)
{
	struct pac_attr_hdr *hdr;
	u8 *pos;
	size_t left, len;
	int type, pac_key_found = 0;

	pos = pac;
	left = pac_len;

	while (left > sizeof(*hdr)) {
		hdr = (struct pac_attr_hdr *) pos;
		type = be_to_host16(hdr->type);
		len = be_to_host16(hdr->len);
		pos += sizeof(*hdr);
		left -= sizeof(*hdr);
		if (len > left) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: PAC TLV overrun (type=%d len=%lu left=%lu)",
				   type, (unsigned long) len,
				   (unsigned long) left);
			return -1;
		}

		eap_teap_parse_pac_tlv(entry, type, pos, len, &pac_key_found);

		pos += len;
		left -= len;
	}

	if (!pac_key_found || !entry->pac_opaque || !entry->pac_info) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC TLV does not include all the required fields");
		return -1;
	}

	return 0;
}


static int eap_teap_parse_pac_info(struct eap_teap_pac *entry, int type,
				   u8 *pos, size_t len)
{
	u16 pac_type;
	u32 lifetime;
	struct os_time now;

	switch (type & 0x7fff) {
	case PAC_TYPE_CRED_LIFETIME:
		if (len != 4) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TEAP: PAC-Info - Invalid CRED_LIFETIME length - ignored",
				    pos, len);
			return 0;
		}

		/*
		 * This is not currently saved separately in PAC files since
		 * the server can automatically initiate PAC update when
		 * needed. Anyway, the information is available from PAC-Info
		 * dump if it is needed for something in the future.
		 */
		lifetime = WPA_GET_BE32(pos);
		os_get_time(&now);
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC-Info - CRED_LIFETIME %d (%d days)",
			   lifetime, (lifetime - (u32) now.sec) / 86400);
		break;
	case PAC_TYPE_A_ID:
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-TEAP: PAC-Info - A-ID",
				  pos, len);
		entry->a_id = pos;
		entry->a_id_len = len;
		break;
	case PAC_TYPE_I_ID:
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-TEAP: PAC-Info - I-ID",
				  pos, len);
		entry->i_id = pos;
		entry->i_id_len = len;
		break;
	case PAC_TYPE_A_ID_INFO:
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-TEAP: PAC-Info - A-ID-Info",
				  pos, len);
		entry->a_id_info = pos;
		entry->a_id_info_len = len;
		break;
	case PAC_TYPE_PAC_TYPE:
		/* RFC 7170, Section 4.2.12.6 - PAC-Type TLV */
		if (len != 2) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Invalid PAC-Type length %lu (expected 2)",
				   (unsigned long) len);
			wpa_hexdump_ascii(MSG_DEBUG,
					  "EAP-TEAP: PAC-Info - PAC-Type",
					  pos, len);
			return -1;
		}
		pac_type = WPA_GET_BE16(pos);
		if (pac_type != PAC_TYPE_TUNNEL_PAC) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Unsupported PAC Type %d",
				   pac_type);
			return -1;
		}

		wpa_printf(MSG_DEBUG, "EAP-TEAP: PAC-Info - PAC-Type %d",
			   pac_type);
		entry->pac_type = pac_type;
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Ignored unknown PAC-Info type %d", type);
		break;
	}

	return 0;
}


static int eap_teap_process_pac_info(struct eap_teap_pac *entry)
{
	struct pac_attr_hdr *hdr;
	u8 *pos;
	size_t left, len;
	int type;

	/* RFC 7170, Section 4.2.12.4 */

	/* PAC-Type defaults to Tunnel PAC (Type 1) */
	entry->pac_type = PAC_TYPE_TUNNEL_PAC;

	pos = entry->pac_info;
	left = entry->pac_info_len;
	while (left > sizeof(*hdr)) {
		hdr = (struct pac_attr_hdr *) pos;
		type = be_to_host16(hdr->type);
		len = be_to_host16(hdr->len);
		pos += sizeof(*hdr);
		left -= sizeof(*hdr);
		if (len > left) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: PAC-Info overrun (type=%d len=%lu left=%lu)",
				   type, (unsigned long) len,
				   (unsigned long) left);
			return -1;
		}

		if (eap_teap_parse_pac_info(entry, type, pos, len) < 0)
			return -1;

		pos += len;
		left -= len;
	}

	if (!entry->a_id || !entry->a_id_info) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC-Info does not include all the required fields");
		return -1;
	}

	return 0;
}


static struct wpabuf * eap_teap_process_pac(struct eap_sm *sm,
					    struct eap_teap_data *data,
					    struct eap_method_ret *ret,
					    u8 *pac, size_t pac_len)
{
	struct eap_peer_config *config = eap_get_config(sm);
	struct eap_teap_pac entry;

	os_memset(&entry, 0, sizeof(entry));
	if (eap_teap_process_pac_tlv(&entry, pac, pac_len) ||
	    eap_teap_process_pac_info(&entry))
		return NULL;

	eap_teap_add_pac(&data->pac, &data->current_pac, &entry);
	eap_teap_pac_list_truncate(data->pac, data->max_pac_list_len);
	if (data->use_pac_binary_format)
		eap_teap_save_pac_bin(sm, data->pac, config->pac_file);
	else
		eap_teap_save_pac(sm, data->pac, config->pac_file);

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Send PAC-Acknowledgement - %s initiated provisioning completed successfully",
		   data->provisioning ? "peer" : "server");
	return eap_teap_tlv_pac_ack();
}


static int eap_teap_parse_decrypted(struct wpabuf *decrypted,
				    struct eap_teap_tlv_parse *tlv,
				    struct wpabuf **resp)
{
	u16 tlv_type;
	int mandatory, res;
	size_t len;
	u8 *pos, *end;

	os_memset(tlv, 0, sizeof(*tlv));

	/* Parse TLVs from the decrypted Phase 2 data */
	pos = wpabuf_mhead(decrypted);
	end = pos + wpabuf_len(decrypted);
	while (end - pos >= 4) {
		mandatory = pos[0] & 0x80;
		tlv_type = WPA_GET_BE16(pos) & 0x3fff;
		pos += 2;
		len = WPA_GET_BE16(pos);
		pos += 2;
		if (len > (size_t) (end - pos)) {
			wpa_printf(MSG_INFO, "EAP-TEAP: TLV overflow");
			return -1;
		}
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Received Phase 2: TLV type %u (%s) length %u%s",
			   tlv_type, eap_teap_tlv_type_str(tlv_type),
			   (unsigned int) len,
			   mandatory ? " (mandatory)" : "");

		res = eap_teap_parse_tlv(tlv, tlv_type, pos, len);
		if (res == -2)
			break;
		if (res < 0) {
			if (mandatory) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: NAK unknown mandatory TLV type %u",
					   tlv_type);
				*resp = eap_teap_tlv_nak(0, tlv_type);
				break;
			}

			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Ignore unknown optional TLV type %u",
				   tlv_type);
		}

		pos += len;
	}

	return 0;
}


static struct wpabuf * eap_teap_pac_request(void)
{
	struct wpabuf *req;
	struct teap_tlv_request_action *act;
	struct teap_tlv_hdr *pac;
	struct teap_attr_pac_type *type;

	req = wpabuf_alloc(sizeof(*act) + sizeof(*pac) + sizeof(*type));
	if (!req)
		return NULL;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add Request Action TLV (Process TLV)");
	act = wpabuf_put(req, sizeof(*act));
	act->tlv_type = host_to_be16(TEAP_TLV_REQUEST_ACTION);
	act->length = host_to_be16(2);
	act->status = TEAP_STATUS_SUCCESS;
	act->action = TEAP_REQUEST_ACTION_PROCESS_TLV;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add PAC TLV (PAC-Type = Tunnel)");
	pac = wpabuf_put(req, sizeof(*pac));
	pac->tlv_type = host_to_be16(TEAP_TLV_PAC);
	pac->length = host_to_be16(sizeof(*type));

	type = wpabuf_put(req, sizeof(*type));
	type->type = host_to_be16(PAC_TYPE_PAC_TYPE);
	type->length = host_to_be16(2);
	type->pac_type = host_to_be16(PAC_TYPE_TUNNEL_PAC);

	return req;
}


static int eap_teap_process_decrypted(struct eap_sm *sm,
				      struct eap_teap_data *data,
				      struct eap_method_ret *ret,
				      u8 identifier,
				      struct wpabuf *decrypted,
				      struct wpabuf **out_data)
{
	struct wpabuf *resp = NULL, *tmp;
	struct eap_teap_tlv_parse tlv;
	int failed = 0;
	enum teap_error_codes error = 0;
	int iresult_added = 0;

	if (eap_teap_parse_decrypted(decrypted, &tlv, &resp) < 0) {
		/* Parsing failed - no response available */
		return 0;
	}

	if (resp) {
		/* Parsing rejected the message - send out an error response */
		goto send_resp;
	}

	if (tlv.result == TEAP_STATUS_FAILURE) {
		/* Server indicated failure - respond similarly per
		 * RFC 7170, 3.6.3. This authentication exchange cannot succeed
		 * and will be terminated with a cleartext EAP Failure. */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Server rejected authentication");
		resp = eap_teap_tlv_result(TEAP_STATUS_FAILURE, 0);
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
		goto send_resp;
	}

	if (tlv.iresult == TEAP_STATUS_SUCCESS && !tlv.crypto_binding) {
		/* Intermediate-Result TLV indicating success, but no
		 * Crypto-Binding TLV */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Intermediate-Result TLV indicating success, but no Crypto-Binding TLV");
		failed = 1;
		error = TEAP_ERROR_TUNNEL_COMPROMISE_ERROR;
		goto done;
	}

	if (!data->iresult_verified && !data->result_success_done &&
	    tlv.result == TEAP_STATUS_SUCCESS && !tlv.crypto_binding) {
		/* Result TLV indicating success, but no Crypto-Binding TLV */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Result TLV indicating success, but no Crypto-Binding TLV");
		failed = 1;
		error = TEAP_ERROR_TUNNEL_COMPROMISE_ERROR;
		goto done;
	}

	if (tlv.iresult != TEAP_STATUS_SUCCESS &&
	    tlv.iresult != TEAP_STATUS_FAILURE &&
	    data->inner_method_done) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Inner EAP method exchange completed, but no Intermediate-Result TLV included");
		failed = 1;
		error = TEAP_ERROR_TUNNEL_COMPROMISE_ERROR;
		goto done;
	}

	if (tlv.crypto_binding) {
		if (tlv.iresult != TEAP_STATUS_SUCCESS &&
		    tlv.result != TEAP_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Unexpected Crypto-Binding TLV without Result TLV or Intermediate-Result TLV indicating success");
			failed = 1;
			error = TEAP_ERROR_UNEXPECTED_TLVS_EXCHANGED;
			goto done;
		}

		tmp = eap_teap_process_crypto_binding(sm, data, ret,
						      tlv.crypto_binding,
						      tlv.crypto_binding_len);
		if (!tmp) {
			failed = 1;
			error = TEAP_ERROR_TUNNEL_COMPROMISE_ERROR;
		} else {
			resp = wpabuf_concat(resp, tmp);
			if (tlv.result == TEAP_STATUS_SUCCESS && !failed)
				data->result_success_done = 1;
			if (tlv.iresult == TEAP_STATUS_SUCCESS && !failed) {
				data->inner_method_done = 0;
				data->iresult_verified = 1;
			}
		}
	}

	if (tlv.identity_type == TEAP_IDENTITY_TYPE_MACHINE) {
		struct eap_peer_config *config = eap_get_config(sm);

		sm->use_machine_cred = config && config->machine_identity &&
			config->machine_identity_len;
	} else if (tlv.identity_type) {
		sm->use_machine_cred = 0;
	}
	if (tlv.identity_type) {
		struct eap_peer_config *config = eap_get_config(sm);

		os_free(data->phase2_types);
		data->phase2_types = NULL;
		data->num_phase2_types = 0;
		if (config &&
		    eap_peer_select_phase2_methods(config, "auth=",
						   &data->phase2_types,
						   &data->num_phase2_types,
						   sm->use_machine_cred) < 0) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Failed to update Phase 2 EAP types");
			failed = 1;
			goto done;
		}
	}

	if (tlv.basic_auth_req) {
		tmp = eap_teap_process_basic_auth_req(sm, data,
						      tlv.basic_auth_req,
						      tlv.basic_auth_req_len,
						      tlv.identity_type);
		if (!tmp)
			failed = 1;
		resp = wpabuf_concat(resp, tmp);
	} else if (tlv.eap_payload_tlv) {
		tmp = eap_teap_process_eap_payload_tlv(sm, data, ret,
						       tlv.eap_payload_tlv,
						       tlv.eap_payload_tlv_len,
						       tlv.identity_type);
		if (!tmp)
			failed = 1;
		resp = wpabuf_concat(resp, tmp);

		if (tlv.iresult == TEAP_STATUS_SUCCESS ||
		    tlv.iresult == TEAP_STATUS_FAILURE) {
			tmp = eap_teap_tlv_result(failed ?
						  TEAP_STATUS_FAILURE :
						  TEAP_STATUS_SUCCESS, 1);
			resp = wpabuf_concat(resp, tmp);
			if (tlv.iresult == TEAP_STATUS_FAILURE)
				failed = 1;
			iresult_added = 1;
		}
	}

	if (data->result_success_done && data->session_ticket_used &&
	    eap_teap_derive_msk(data) == 0) {
		/* Assume the server might accept authentication without going
		 * through inner authentication. */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC used - server may decide to skip inner authentication");
		ret->methodState = METHOD_MAY_CONT;
		ret->decision = DECISION_COND_SUCC;
	} else if (data->result_success_done &&
		   tls_connection_get_own_cert_used(data->ssl.conn) &&
		   eap_teap_derive_msk(data) == 0) {
		/* Assume the server might accept authentication without going
		 * through inner authentication. */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Client certificate used - server may decide to skip inner authentication");
		ret->methodState = METHOD_MAY_CONT;
		ret->decision = DECISION_COND_SUCC;
	}

	if (tlv.pac) {
		if (tlv.result == TEAP_STATUS_SUCCESS) {
			tmp = eap_teap_process_pac(sm, data, ret,
						   tlv.pac, tlv.pac_len);
			resp = wpabuf_concat(resp, tmp);
		} else {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: PAC TLV without Result TLV acknowledging success");
			failed = 1;
			error = TEAP_ERROR_UNEXPECTED_TLVS_EXCHANGED;
		}
	}

	if (!data->current_pac && data->provisioning && !failed && !tlv.pac &&
	    tlv.crypto_binding &&
	    (!data->anon_provisioning ||
	     (data->phase2_success && data->phase2_method &&
	      data->phase2_method->vendor == 0 &&
	      eap_teap_allowed_anon_prov_cipher_suite(data->tls_cs) &&
	      eap_teap_allowed_anon_prov_phase2_method(
		      data->phase2_method->vendor,
		      data->phase2_method->method))) &&
	    (tlv.iresult == TEAP_STATUS_SUCCESS ||
	     tlv.result == TEAP_STATUS_SUCCESS)) {
		/*
		 * Need to request Tunnel PAC when using authenticated
		 * provisioning.
		 */
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Request Tunnel PAC");
		tmp = eap_teap_pac_request();
		resp = wpabuf_concat(resp, tmp);
	}

done:
	if (failed) {
		tmp = eap_teap_tlv_result(TEAP_STATUS_FAILURE, 0);
		resp = wpabuf_concat(tmp, resp);

		if (error != 0) {
			tmp = eap_teap_tlv_error(error);
			resp = wpabuf_concat(tmp, resp);
		}

		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
	} else if (tlv.result == TEAP_STATUS_SUCCESS) {
		tmp = eap_teap_tlv_result(TEAP_STATUS_SUCCESS, 0);
		resp = wpabuf_concat(tmp, resp);
	}
	if ((tlv.iresult == TEAP_STATUS_SUCCESS ||
	     tlv.iresult == TEAP_STATUS_FAILURE) && !iresult_added) {
		tmp = eap_teap_tlv_result((!failed && data->phase2_success) ?
					  TEAP_STATUS_SUCCESS :
					  TEAP_STATUS_FAILURE, 1);
		resp = wpabuf_concat(tmp, resp);
	}

	if (resp && tlv.result == TEAP_STATUS_SUCCESS && !failed &&
	    (tlv.crypto_binding || data->iresult_verified) &&
	    data->phase2_success) {
		/* Successfully completed Phase 2 */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Authentication completed successfully");
		ret->methodState = METHOD_MAY_CONT;
		data->on_tx_completion = data->provisioning ?
			METHOD_MAY_CONT : METHOD_DONE;
		ret->decision = DECISION_UNCOND_SUCC;
	}

	if (!resp) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No recognized TLVs - send empty response packet");
		resp = wpabuf_alloc(1);
	}

send_resp:
	if (!resp)
		return 0;

	wpa_hexdump_buf(MSG_DEBUG, "EAP-TEAP: Encrypting Phase 2 data", resp);
	if (eap_peer_tls_encrypt(sm, &data->ssl, EAP_TYPE_TEAP,
				 data->teap_version, identifier,
				 resp, out_data)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Failed to encrypt a Phase 2 frame");
	}
	wpabuf_free(resp);

	return 0;
}


static int eap_teap_decrypt(struct eap_sm *sm, struct eap_teap_data *data,
			    struct eap_method_ret *ret, u8 identifier,
			    const struct wpabuf *in_data,
			    struct wpabuf **out_data)
{
	struct wpabuf *in_decrypted;
	int res;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Received %lu bytes encrypted data for Phase 2",
		   (unsigned long) wpabuf_len(in_data));

	if (data->pending_phase2_req) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Pending Phase 2 request - skip decryption and use old data");
		/* Clear TLS reassembly state. */
		eap_peer_tls_reset_input(&data->ssl);

		in_decrypted = data->pending_phase2_req;
		data->pending_phase2_req = NULL;
		goto continue_req;
	}

	if (wpabuf_len(in_data) == 0) {
		/* Received TLS ACK - requesting more fragments */
		res = eap_peer_tls_encrypt(sm, &data->ssl, EAP_TYPE_TEAP,
					   data->teap_version,
					   identifier, NULL, out_data);
		if (res == 0 && !data->ssl.tls_out &&
		    data->on_tx_completion) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Mark authentication completed at full TX of fragments");
			ret->methodState = data->on_tx_completion;
			data->on_tx_completion = 0;
			ret->decision = DECISION_UNCOND_SUCC;
		}
		return res;
	}

	res = eap_peer_tls_decrypt(sm, &data->ssl, in_data, &in_decrypted);
	if (res)
		return res;

continue_req:
	wpa_hexdump_buf(MSG_MSGDUMP, "EAP-TEAP: Decrypted Phase 2 TLV(s)",
			in_decrypted);

	if (wpabuf_len(in_decrypted) < 4) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Too short Phase 2 TLV frame (len=%lu)",
			   (unsigned long) wpabuf_len(in_decrypted));
		wpabuf_free(in_decrypted);
		return -1;
	}

	res = eap_teap_process_decrypted(sm, data, ret, identifier,
					 in_decrypted, out_data);

	wpabuf_free(in_decrypted);

	return res;
}


static void eap_teap_select_pac(struct eap_teap_data *data,
				const u8 *a_id, size_t a_id_len)
{
	if (!a_id)
		return;
	data->current_pac = eap_teap_get_pac(data->pac, a_id, a_id_len,
					     PAC_TYPE_TUNNEL_PAC);
	if (data->current_pac) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC found for this A-ID (PAC-Type %d)",
			   data->current_pac->pac_type);
		wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-TEAP: A-ID-Info",
				  data->current_pac->a_id_info,
				  data->current_pac->a_id_info_len);
	}
}


static int eap_teap_use_pac_opaque(struct eap_sm *sm,
				   struct eap_teap_data *data,
				   struct eap_teap_pac *pac)
{
	u8 *tlv;
	size_t tlv_len, olen;
	struct teap_tlv_hdr *ehdr;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add PAC-Opaque TLS extension");
	olen = pac->pac_opaque_len;
	tlv_len = sizeof(*ehdr) + olen;
	tlv = os_malloc(tlv_len);
	if (tlv) {
		ehdr = (struct teap_tlv_hdr *) tlv;
		ehdr->tlv_type = host_to_be16(PAC_TYPE_PAC_OPAQUE);
		ehdr->length = host_to_be16(olen);
		os_memcpy(ehdr + 1, pac->pac_opaque, olen);
	}
	if (!tlv ||
	    tls_connection_client_hello_ext(sm->ssl_ctx, data->ssl.conn,
					    TLS_EXT_PAC_OPAQUE,
					    tlv, tlv_len) < 0) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Failed to add PAC-Opaque TLS extension");
		os_free(tlv);
		return -1;
	}
	os_free(tlv);

	return 0;
}


static int eap_teap_clear_pac_opaque_ext(struct eap_sm *sm,
					 struct eap_teap_data *data)
{
	if (tls_connection_client_hello_ext(sm->ssl_ctx, data->ssl.conn,
					    TLS_EXT_PAC_OPAQUE, NULL, 0) < 0) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Failed to remove PAC-Opaque TLS extension");
		return -1;
	}
	return 0;
}


static int eap_teap_process_start(struct eap_sm *sm,
				  struct eap_teap_data *data, u8 flags,
				  const u8 *pos, size_t left)
{
	const u8 *a_id = NULL;
	size_t a_id_len = 0;

	/* TODO: Support (mostly theoretical) case of TEAP/Start request being
	 * fragmented */

	/* EAP-TEAP version negotiation (RFC 7170, Section 3.2) */
	data->received_version = flags & EAP_TLS_VERSION_MASK;
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Start (server ver=%u, own ver=%u)",
		   data->received_version, data->teap_version);
	if (data->received_version < 1) {
		/* Version 1 was the first defined version, so reject 0 */
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Server used unknown TEAP version %u",
			   data->received_version);
		return -1;
	}
	if (data->received_version < data->teap_version)
		data->teap_version = data->received_version;
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Using TEAP version %d",
		   data->teap_version);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Start message payload", pos, left);

	/* Parse Authority-ID TLV from Outer TLVs, if present */
	if (flags & EAP_TEAP_FLAGS_OUTER_TLV_LEN) {
		const u8 *outer_pos, *outer_end;
		u32 outer_tlv_len;

		if (left < 4) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Not enough room for the Outer TLV Length field");
			return -1;
		}

		outer_tlv_len = WPA_GET_BE32(pos);
		pos += 4;
		left -= 4;

		if (outer_tlv_len > left) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Truncated Outer TLVs field (Outer TLV Length: %u; remaining buffer: %u)",
				   outer_tlv_len, (unsigned int) left);
			return -1;
		}

		outer_pos = pos + left - outer_tlv_len;
		outer_end = outer_pos + outer_tlv_len;
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Start message Outer TLVs",
			    outer_pos, outer_tlv_len);
		wpabuf_free(data->server_outer_tlvs);
		data->server_outer_tlvs = wpabuf_alloc_copy(outer_pos,
							    outer_tlv_len);
		if (!data->server_outer_tlvs)
			return -1;
		left -= outer_tlv_len;
		if (left > 0) {
			wpa_hexdump(MSG_INFO,
				    "EAP-TEAP: Unexpected TLS Data in Start message",
				    pos, left);
			return -1;
		}

		while (outer_pos < outer_end) {
			u16 tlv_type, tlv_len;

			if (outer_end - outer_pos < 4) {
				wpa_printf(MSG_INFO,
					   "EAP-TEAP: Truncated Outer TLV header");
				return -1;
			}
			tlv_type = WPA_GET_BE16(outer_pos);
			outer_pos += 2;
			tlv_len = WPA_GET_BE16(outer_pos);
			outer_pos += 2;
			/* Outer TLVs are required to be optional, so no need to
			 * check the M flag */
			tlv_type &= TEAP_TLV_TYPE_MASK;
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Outer TLV: Type=%u Length=%u",
				   tlv_type, tlv_len);
			if (outer_end - outer_pos < tlv_len) {
				wpa_printf(MSG_INFO,
					   "EAP-TEAP: Truncated Outer TLV (Type %u)",
					   tlv_type);
				return -1;
			}
			if (tlv_type == TEAP_TLV_AUTHORITY_ID) {
				wpa_hexdump(MSG_DEBUG, "EAP-TEAP: Authority-ID",
					    outer_pos, tlv_len);
				if (a_id) {
					wpa_printf(MSG_INFO,
						   "EAP-TEAP: Multiple Authority-ID TLVs in TEAP/Start");
					return -1;
				}
				a_id = outer_pos;
				a_id_len = tlv_len;
			} else {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Ignore unknown Outer TLV (Type %u)",
					   tlv_type);
			}
			outer_pos += tlv_len;
		}
	} else if (left > 0) {
		wpa_hexdump(MSG_INFO,
			    "EAP-TEAP: Unexpected TLS Data in Start message",
			    pos, left);
		return -1;
	}

	eap_teap_select_pac(data, a_id, a_id_len);

	if (data->resuming && data->current_pac) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Trying to resume session - do not add PAC-Opaque to TLS ClientHello");
		if (eap_teap_clear_pac_opaque_ext(sm, data) < 0)
			return -1;
	} else if (data->current_pac) {
		/*
		 * PAC found for the A-ID and we are not resuming an old
		 * session, so add PAC-Opaque extension to ClientHello.
		 */
		if (eap_teap_use_pac_opaque(sm, data, data->current_pac) < 0)
			return -1;
	} else if (data->provisioning_allowed) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No PAC found - starting provisioning");
		if (eap_teap_clear_pac_opaque_ext(sm, data) < 0)
			return -1;
		data->provisioning = 1;
	}

	return 0;
}


#ifdef CONFIG_TESTING_OPTIONS
static struct wpabuf * eap_teap_add_stub_outer_tlvs(struct eap_teap_data *data,
						    struct wpabuf *resp)
{
	struct wpabuf *resp2;
	u16 len;
	const u8 *pos;
	u8 flags;

	wpabuf_free(data->peer_outer_tlvs);
	data->peer_outer_tlvs = wpabuf_alloc(4 + 4);
	if (!data->peer_outer_tlvs) {
		wpabuf_free(resp);
		return NULL;
	}

	/* Outer TLVs (stub Vendor-Specific TLV for testing) */
	wpabuf_put_be16(data->peer_outer_tlvs, TEAP_TLV_VENDOR_SPECIFIC);
	wpabuf_put_be16(data->peer_outer_tlvs, 4);
	wpabuf_put_be32(data->peer_outer_tlvs, EAP_VENDOR_HOSTAP);
	wpa_hexdump_buf(MSG_DEBUG, "EAP-TEAP: TESTING - Add stub Outer TLVs",
			data->peer_outer_tlvs);

	wpa_hexdump_buf(MSG_DEBUG,
			"EAP-TEAP: TEAP/Start response before modification",
			resp);
	resp2 = wpabuf_alloc(wpabuf_len(resp) + 4 +
			     wpabuf_len(data->peer_outer_tlvs));
	if (!resp2) {
		wpabuf_free(resp);
		return NULL;
	}

	pos = wpabuf_head(resp);
	wpabuf_put_u8(resp2, *pos++); /* Code */
	wpabuf_put_u8(resp2, *pos++); /* Identifier */
	len = WPA_GET_BE16(pos);
	pos += 2;
	wpabuf_put_be16(resp2, len + 4 + wpabuf_len(data->peer_outer_tlvs));
	wpabuf_put_u8(resp2, *pos++); /* Type */
	/* Flags | Ver (with Outer TLV length included flag set to 1) */
	flags = *pos++;
	if (flags & (EAP_TEAP_FLAGS_OUTER_TLV_LEN |
		     EAP_TLS_FLAGS_LENGTH_INCLUDED)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Cannot add Outer TLVs for testing");
		wpabuf_free(resp);
		wpabuf_free(resp2);
		return NULL;
	}
	flags |= EAP_TEAP_FLAGS_OUTER_TLV_LEN;
	wpabuf_put_u8(resp2, flags);
	/* Outer TLV Length */
	wpabuf_put_be32(resp2, wpabuf_len(data->peer_outer_tlvs));
	/* TLS Data */
	wpabuf_put_data(resp2, pos, wpabuf_len(resp) - 6);
	wpabuf_put_buf(resp2, data->peer_outer_tlvs); /* Outer TLVs */

	wpabuf_free(resp);
	wpa_hexdump_buf(MSG_DEBUG,
			"EAP-TEAP: TEAP/Start response after modification",
			resp2);
	return resp2;
}
#endif /* CONFIG_TESTING_OPTIONS */


static struct wpabuf * eap_teap_process(struct eap_sm *sm, void *priv,
					struct eap_method_ret *ret,
					const struct wpabuf *reqData)
{
	const struct eap_hdr *req;
	size_t left;
	int res;
	u8 flags, id;
	struct wpabuf *resp;
	const u8 *pos;
	struct eap_teap_data *data = priv;
	struct wpabuf msg;

	pos = eap_peer_tls_process_init(sm, &data->ssl, EAP_TYPE_TEAP, ret,
					reqData, &left, &flags);
	if (!pos)
		return NULL;

	req = wpabuf_head(reqData);
	id = req->identifier;

	if (flags & EAP_TLS_FLAGS_START) {
		if (eap_teap_process_start(sm, data, flags, pos, left) < 0)
			return NULL;

		/* Outer TLVs are not used in further packet processing and
		 * there cannot be TLS Data in this TEAP/Start message, so
		 * enforce that by ignoring whatever data might remain in the
		 * buffer. */
		left = 0;
	} else if (flags & EAP_TEAP_FLAGS_OUTER_TLV_LEN) {
		/* TODO: RFC 7170, Section 4.3.1 indicates that the unexpected
		 * Outer TLVs MUST be ignored instead of ignoring the full
		 * message. */
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Outer TLVs present in non-Start message -> ignore message");
		return NULL;
	}

	wpabuf_set(&msg, pos, left);

	resp = NULL;
	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
	    !data->resuming) {
		/* Process tunneled (encrypted) phase 2 data. */
		res = eap_teap_decrypt(sm, data, ret, id, &msg, &resp);
		if (res < 0) {
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			/*
			 * Ack possible Alert that may have caused failure in
			 * decryption.
			 */
			res = 1;
		}
	} else {
		if (sm->waiting_ext_cert_check && data->pending_resp) {
			struct eap_peer_config *config = eap_get_config(sm);

			if (config->pending_ext_cert_check ==
			    EXT_CERT_CHECK_GOOD) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: External certificate check succeeded - continue handshake");
				resp = data->pending_resp;
				data->pending_resp = NULL;
				sm->waiting_ext_cert_check = 0;
				return resp;
			}

			if (config->pending_ext_cert_check ==
			    EXT_CERT_CHECK_BAD) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: External certificate check failed - force authentication failure");
				ret->methodState = METHOD_DONE;
				ret->decision = DECISION_FAIL;
				sm->waiting_ext_cert_check = 0;
				return NULL;
			}

			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Continuing to wait external server certificate validation");
			return NULL;
		}

		/* Continue processing TLS handshake (phase 1). */
		res = eap_peer_tls_process_helper(sm, &data->ssl,
						  EAP_TYPE_TEAP,
						  data->teap_version, id, &msg,
						  &resp);
		if (res < 0) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: TLS processing failed");
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			return resp;
		}

		if (sm->waiting_ext_cert_check) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Waiting external server certificate validation");
			wpabuf_free(data->pending_resp);
			data->pending_resp = resp;
			return NULL;
		}

		if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
			char cipher[80];

			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: TLS done, proceed to Phase 2");
			data->tls_cs =
				tls_connection_get_cipher_suite(data->ssl.conn);
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: TLS cipher suite 0x%04x",
				   data->tls_cs);

			if (data->provisioning &&
			    (!(data->provisioning_allowed &
			       EAP_TEAP_PROV_AUTH) ||
			     tls_get_cipher(sm->ssl_ctx, data->ssl.conn,
					    cipher, sizeof(cipher)) < 0 ||
			     os_strstr(cipher, "ADH-") ||
			     os_strstr(cipher, "anon"))) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Using anonymous (unauthenticated) provisioning");
				data->anon_provisioning = 1;
			} else {
				data->anon_provisioning = 0;
			}
			data->resuming = 0;
			if (eap_teap_derive_key_auth(sm, data) < 0) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Could not derive keys");
				ret->methodState = METHOD_DONE;
				ret->decision = DECISION_FAIL;
				wpabuf_free(resp);
				return NULL;
			}
		}

		if (res == 2) {
			/*
			 * Application data included in the handshake message.
			 */
			wpabuf_free(data->pending_phase2_req);
			data->pending_phase2_req = resp;
			resp = NULL;
			res = eap_teap_decrypt(sm, data, ret, id, &msg, &resp);
		}
	}

	if (res == 1) {
		wpabuf_free(resp);
		return eap_peer_tls_build_ack(id, EAP_TYPE_TEAP,
					      data->teap_version);
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (data->test_outer_tlvs && res == 0 && resp &&
	    (flags & EAP_TLS_FLAGS_START) && wpabuf_len(resp) >= 6)
		resp = eap_teap_add_stub_outer_tlvs(data, resp);
#endif /* CONFIG_TESTING_OPTIONS */

	return resp;
}


#if 0 /* TODO */
static bool eap_teap_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	return tls_connection_established(sm->ssl_ctx, data->ssl.conn);
}


static void eap_teap_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	if (data->phase2_priv && data->phase2_method &&
	    data->phase2_method->deinit_for_reauth)
		data->phase2_method->deinit_for_reauth(sm, data->phase2_priv);
	eap_teap_clear(data);
}


static void * eap_teap_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	if (eap_peer_tls_reauth_init(sm, &data->ssl)) {
		eap_teap_deinit(sm, data);
		return NULL;
	}
	if (data->phase2_priv && data->phase2_method &&
	    data->phase2_method->init_for_reauth)
		data->phase2_method->init_for_reauth(sm, data->phase2_priv);
	data->phase2_success = 0;
	data->inner_method_done = 0;
	data->result_success_done = 0;
	data->iresult_verified = 0;
	data->done_on_tx_completion = 0;
	data->resuming = 1;
	data->provisioning = 0;
	data->anon_provisioning = 0;
	data->simck_idx = 0;
	return priv;
}
#endif


static int eap_teap_get_status(struct eap_sm *sm, void *priv, char *buf,
			       size_t buflen, int verbose)
{
	struct eap_teap_data *data = priv;
	int len, ret;

	len = eap_peer_tls_status(sm, &data->ssl, buf, buflen, verbose);
	if (data->phase2_method) {
		ret = os_snprintf(buf + len, buflen - len,
				  "EAP-TEAP Phase 2 method=%s\n",
				  data->phase2_method->name);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}
	return len;
}


static bool eap_teap_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	return data->success;
}


static u8 * eap_teap_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	u8 *key;

	if (!data->success)
		return NULL;

	key = os_memdup(data->key_data, EAP_TEAP_KEY_LEN);
	if (!key)
		return NULL;

	*len = EAP_TEAP_KEY_LEN;

	return key;
}


static u8 * eap_teap_get_session_id(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	u8 *id;

	if (!data->success || !data->session_id)
		return NULL;

	id = os_memdup(data->session_id, data->id_len);
	if (!id)
		return NULL;

	*len = data->id_len;

	return id;
}


static u8 * eap_teap_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	u8 *key;

	if (!data->success)
		return NULL;

	key = os_memdup(data->emsk, EAP_EMSK_LEN);
	if (!key)
		return NULL;

	*len = EAP_EMSK_LEN;

	return key;
}


int eap_peer_teap_register(void)
{
	struct eap_method *eap;

	eap = eap_peer_method_alloc(EAP_PEER_METHOD_INTERFACE_VERSION,
				    EAP_VENDOR_IETF, EAP_TYPE_TEAP, "TEAP");
	if (!eap)
		return -1;

	eap->init = eap_teap_init;
	eap->deinit = eap_teap_deinit;
	eap->process = eap_teap_process;
	eap->isKeyAvailable = eap_teap_isKeyAvailable;
	eap->getKey = eap_teap_getKey;
	eap->getSessionId = eap_teap_get_session_id;
	eap->get_status = eap_teap_get_status;
#if 0 /* TODO */
	eap->has_reauth_data = eap_teap_has_reauth_data;
	eap->deinit_for_reauth = eap_teap_deinit_for_reauth;
	eap->init_for_reauth = eap_teap_init_for_reauth;
#endif
	eap->get_emsk = eap_teap_get_emsk;

	return eap_peer_method_register(eap);
}
