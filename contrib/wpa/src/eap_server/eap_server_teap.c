/*
 * EAP-TEAP server (RFC 7170)
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/aes_wrap.h"
#include "crypto/tls.h"
#include "crypto/random.h"
#include "eap_common/eap_teap_common.h"
#include "eap_i.h"
#include "eap_tls_common.h"


static void eap_teap_reset(struct eap_sm *sm, void *priv);


/* Private PAC-Opaque TLV types */
#define PAC_OPAQUE_TYPE_PAD 0
#define PAC_OPAQUE_TYPE_KEY 1
#define PAC_OPAQUE_TYPE_LIFETIME 2
#define PAC_OPAQUE_TYPE_IDENTITY 3

struct eap_teap_data {
	struct eap_ssl_data ssl;
	enum {
		START, PHASE1, PHASE1B, PHASE2_START, PHASE2_ID,
		PHASE2_BASIC_AUTH, PHASE2_METHOD, CRYPTO_BINDING, REQUEST_PAC,
		FAILURE_SEND_RESULT, SUCCESS_SEND_RESULT, SUCCESS, FAILURE
	} state;

	u8 teap_version;
	u8 peer_version;
	u16 tls_cs;

	const struct eap_method *phase2_method;
	void *phase2_priv;

	u8 crypto_binding_nonce[32];
	int final_result;

	u8 simck_msk[EAP_TEAP_SIMCK_LEN];
	u8 cmk_msk[EAP_TEAP_CMK_LEN];
	u8 simck_emsk[EAP_TEAP_SIMCK_LEN];
	u8 cmk_emsk[EAP_TEAP_CMK_LEN];
	int simck_idx;
	int cmk_emsk_available;

	u8 pac_opaque_encr[16];
	u8 *srv_id;
	size_t srv_id_len;
	char *srv_id_info;

	unsigned int basic_auth_not_done:1;
	unsigned int inner_eap_not_done:1;
	int anon_provisioning;
	int skipped_inner_auth;
	int send_new_pac; /* server triggered re-keying of Tunnel PAC */
	struct wpabuf *pending_phase2_resp;
	struct wpabuf *server_outer_tlvs;
	struct wpabuf *peer_outer_tlvs;
	u8 *identity; /* from PAC-Opaque or client certificate */
	size_t identity_len;
	int eap_seq;
	int tnc_started;

	int pac_key_lifetime;
	int pac_key_refresh_time;

	enum teap_error_codes error_code;
	enum teap_identity_types cur_id_type;

	bool check_crypto_binding;
};


static int eap_teap_process_phase2_start(struct eap_sm *sm,
					 struct eap_teap_data *data);
static int eap_teap_phase2_init(struct eap_sm *sm, struct eap_teap_data *data,
				int vendor, enum eap_type eap_type);


static const char * eap_teap_state_txt(int state)
{
	switch (state) {
	case START:
		return "START";
	case PHASE1:
		return "PHASE1";
	case PHASE1B:
		return "PHASE1B";
	case PHASE2_START:
		return "PHASE2_START";
	case PHASE2_ID:
		return "PHASE2_ID";
	case PHASE2_BASIC_AUTH:
		return "PHASE2_BASIC_AUTH";
	case PHASE2_METHOD:
		return "PHASE2_METHOD";
	case CRYPTO_BINDING:
		return "CRYPTO_BINDING";
	case REQUEST_PAC:
		return "REQUEST_PAC";
	case FAILURE_SEND_RESULT:
		return "FAILURE_SEND_RESULT";
	case SUCCESS_SEND_RESULT:
		return "SUCCESS_SEND_RESULT";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "Unknown?!";
	}
}


static void eap_teap_state(struct eap_teap_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-TEAP: %s -> %s",
		   eap_teap_state_txt(data->state),
		   eap_teap_state_txt(state));
	data->state = state;
}


static enum eap_type eap_teap_req_failure(struct eap_teap_data *data,
					  enum teap_error_codes error)
{
	eap_teap_state(data, FAILURE_SEND_RESULT);
	return EAP_TYPE_NONE;
}


static int eap_teap_session_ticket_cb(void *ctx, const u8 *ticket, size_t len,
				      const u8 *client_random,
				      const u8 *server_random,
				      u8 *master_secret)
{
	struct eap_teap_data *data = ctx;
	const u8 *pac_opaque;
	size_t pac_opaque_len;
	u8 *buf, *pos, *end, *pac_key = NULL;
	os_time_t lifetime = 0;
	struct os_time now;
	u8 *identity = NULL;
	size_t identity_len = 0;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: SessionTicket callback");
	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: SessionTicket (PAC-Opaque)",
		    ticket, len);

	if (len < 4 || WPA_GET_BE16(ticket) != PAC_TYPE_PAC_OPAQUE) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Ignore invalid SessionTicket");
		return 0;
	}

	pac_opaque_len = WPA_GET_BE16(ticket + 2);
	pac_opaque = ticket + 4;
	if (pac_opaque_len < 8 || pac_opaque_len % 8 ||
	    pac_opaque_len > len - 4) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Ignore invalid PAC-Opaque (len=%lu left=%lu)",
			   (unsigned long) pac_opaque_len,
			   (unsigned long) len);
		return 0;
	}
	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: Received PAC-Opaque",
		    pac_opaque, pac_opaque_len);

	buf = os_malloc(pac_opaque_len - 8);
	if (!buf) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Failed to allocate memory for decrypting PAC-Opaque");
		return 0;
	}

	if (aes_unwrap(data->pac_opaque_encr, sizeof(data->pac_opaque_encr),
		       (pac_opaque_len - 8) / 8, pac_opaque, buf) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Failed to decrypt PAC-Opaque");
		os_free(buf);
		/*
		 * This may have been caused by server changing the PAC-Opaque
		 * encryption key, so just ignore this PAC-Opaque instead of
		 * failing the authentication completely. Provisioning can now
		 * be used to provision a new PAC.
		 */
		return 0;
	}

	end = buf + pac_opaque_len - 8;
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: Decrypted PAC-Opaque",
			buf, end - buf);

	pos = buf;
	while (end - pos > 1) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		if (elen > end - pos)
			break;

		switch (id) {
		case PAC_OPAQUE_TYPE_PAD:
			goto done;
		case PAC_OPAQUE_TYPE_KEY:
			if (elen != EAP_TEAP_PAC_KEY_LEN) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Invalid PAC-Key length %d",
					   elen);
				os_free(buf);
				return -1;
			}
			pac_key = pos;
			wpa_hexdump_key(MSG_DEBUG,
					"EAP-TEAP: PAC-Key from decrypted PAC-Opaque",
					pac_key, EAP_TEAP_PAC_KEY_LEN);
			break;
		case PAC_OPAQUE_TYPE_LIFETIME:
			if (elen != 4) {
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Invalid PAC-Key lifetime length %d",
					   elen);
				os_free(buf);
				return -1;
			}
			lifetime = WPA_GET_BE32(pos);
			break;
		case PAC_OPAQUE_TYPE_IDENTITY:
			identity = pos;
			identity_len = elen;
			break;
		}

		pos += elen;
	}
done:

	if (!pac_key) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No PAC-Key included in PAC-Opaque");
		os_free(buf);
		return -1;
	}

	if (identity) {
		wpa_hexdump_ascii(MSG_DEBUG,
				  "EAP-TEAP: Identity from PAC-Opaque",
				  identity, identity_len);
		os_free(data->identity);
		data->identity = os_malloc(identity_len);
		if (data->identity) {
			os_memcpy(data->identity, identity, identity_len);
			data->identity_len = identity_len;
		}
	}

	if (os_get_time(&now) < 0 || lifetime <= 0 || now.sec > lifetime) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC-Key not valid anymore (lifetime=%ld now=%ld)",
			   lifetime, now.sec);
		data->send_new_pac = 2;
		/*
		 * Allow PAC to be used to allow a PAC update with some level
		 * of server authentication (i.e., do not fall back to full TLS
		 * handshake since we cannot be sure that the peer would be
		 * able to validate server certificate now). However, reject
		 * the authentication since the PAC was not valid anymore. Peer
		 * can connect again with the newly provisioned PAC after this.
		 */
	} else if (lifetime - now.sec < data->pac_key_refresh_time) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC-Key soft timeout; send an update if authentication succeeds");
		data->send_new_pac = 1;
	}

	/* EAP-TEAP uses PAC-Key as the TLS master_secret */
	os_memcpy(master_secret, pac_key, EAP_TEAP_PAC_KEY_LEN);

	os_free(buf);

	return 1;
}


static int eap_teap_derive_key_auth(struct eap_sm *sm,
				    struct eap_teap_data *data)
{
	int res;

	/* RFC 7170, Section 5.1 */
	res = tls_connection_export_key(sm->cfg->ssl_ctx, data->ssl.conn,
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


static int eap_teap_update_icmk(struct eap_sm *sm, struct eap_teap_data *data)
{
	u8 *msk = NULL, *emsk = NULL;
	size_t msk_len = 0, emsk_len = 0;
	int res;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Deriving ICMK[%d] (S-IMCK and CMK)",
		   data->simck_idx + 1);

	if (sm->cfg->eap_teap_auth == 1)
		return eap_teap_derive_cmk_basic_pw_auth(data->tls_cs,
							 data->simck_msk,
							 data->cmk_msk);

	if (!data->phase2_method || !data->phase2_priv) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Phase 2 method not available");
		return -1;
	}

	if (data->phase2_method->getKey) {
		msk = data->phase2_method->getKey(sm, data->phase2_priv,
						  &msk_len);
		if (!msk) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Could not fetch Phase 2 MSK");
			return -1;
		}
	}

	if (data->phase2_method->get_emsk) {
		emsk = data->phase2_method->get_emsk(sm, data->phase2_priv,
						     &emsk_len);
	}

	res = eap_teap_derive_imck(data->tls_cs,
				   data->simck_msk, data->simck_emsk,
				   msk, msk_len, emsk, emsk_len,
				   data->simck_msk, data->cmk_msk,
				   data->simck_emsk, data->cmk_emsk);
	bin_clear_free(msk, msk_len);
	bin_clear_free(emsk, emsk_len);
	if (res == 0) {
		data->simck_idx++;
		if (emsk)
			data->cmk_emsk_available = 1;
	}
	return 0;
}


static void * eap_teap_init(struct eap_sm *sm)
{
	struct eap_teap_data *data;

	data = os_zalloc(sizeof(*data));
	if (!data)
		return NULL;
	data->teap_version = EAP_TEAP_VERSION;
	data->state = START;

	if (eap_server_tls_ssl_init(sm, &data->ssl,
				    sm->cfg->eap_teap_auth == 2 ? 2 : 0,
				    EAP_TYPE_TEAP)) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Failed to initialize SSL.");
		eap_teap_reset(sm, data);
		return NULL;
	}

	/* TODO: Add anon-DH TLS cipher suites (and if one is negotiated,
	 * enforce inner EAP with mutual authentication to be used) */

	if (tls_connection_set_session_ticket_cb(sm->cfg->ssl_ctx,
						 data->ssl.conn,
						 eap_teap_session_ticket_cb,
						 data) < 0) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Failed to set SessionTicket callback");
		eap_teap_reset(sm, data);
		return NULL;
	}

	if (!sm->cfg->pac_opaque_encr_key) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: No PAC-Opaque encryption key configured");
		eap_teap_reset(sm, data);
		return NULL;
	}
	os_memcpy(data->pac_opaque_encr, sm->cfg->pac_opaque_encr_key,
		  sizeof(data->pac_opaque_encr));

	if (!sm->cfg->eap_fast_a_id) {
		wpa_printf(MSG_INFO, "EAP-TEAP: No A-ID configured");
		eap_teap_reset(sm, data);
		return NULL;
	}
	data->srv_id = os_malloc(sm->cfg->eap_fast_a_id_len);
	if (!data->srv_id) {
		eap_teap_reset(sm, data);
		return NULL;
	}
	os_memcpy(data->srv_id, sm->cfg->eap_fast_a_id,
		  sm->cfg->eap_fast_a_id_len);
	data->srv_id_len = sm->cfg->eap_fast_a_id_len;

	if (!sm->cfg->eap_fast_a_id_info) {
		wpa_printf(MSG_INFO, "EAP-TEAP: No A-ID-Info configured");
		eap_teap_reset(sm, data);
		return NULL;
	}
	data->srv_id_info = os_strdup(sm->cfg->eap_fast_a_id_info);
	if (!data->srv_id_info) {
		eap_teap_reset(sm, data);
		return NULL;
	}

	/* PAC-Key lifetime in seconds (hard limit) */
	data->pac_key_lifetime = sm->cfg->pac_key_lifetime;

	/*
	 * PAC-Key refresh time in seconds (soft limit on remaining hard
	 * limit). The server will generate a new PAC-Key when this number of
	 * seconds (or fewer) of the lifetime remains.
	 */
	data->pac_key_refresh_time = sm->cfg->pac_key_refresh_time;

	return data;
}


static void eap_teap_reset(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	if (!data)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->reset(sm, data->phase2_priv);
	eap_server_tls_ssl_deinit(sm, &data->ssl);
	os_free(data->srv_id);
	os_free(data->srv_id_info);
	wpabuf_free(data->pending_phase2_resp);
	wpabuf_free(data->server_outer_tlvs);
	wpabuf_free(data->peer_outer_tlvs);
	os_free(data->identity);
	forced_memzero(data->simck_msk, EAP_TEAP_SIMCK_LEN);
	forced_memzero(data->simck_emsk, EAP_TEAP_SIMCK_LEN);
	forced_memzero(data->cmk_msk, EAP_TEAP_CMK_LEN);
	forced_memzero(data->cmk_emsk, EAP_TEAP_CMK_LEN);
	forced_memzero(data->pac_opaque_encr, sizeof(data->pac_opaque_encr));
	bin_clear_free(data, sizeof(*data));
}


static struct wpabuf * eap_teap_build_start(struct eap_sm *sm,
					    struct eap_teap_data *data, u8 id)
{
	struct wpabuf *req;
	size_t outer_tlv_len = sizeof(struct teap_tlv_hdr) + data->srv_id_len;
	const u8 *start, *end;

	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_TEAP,
			    1 + 4 + outer_tlv_len, EAP_CODE_REQUEST, id);
	if (!req) {
		wpa_printf(MSG_ERROR,
			   "EAP-TEAP: Failed to allocate memory for request");
		eap_teap_state(data, FAILURE);
		return NULL;
	}

	wpabuf_put_u8(req, EAP_TLS_FLAGS_START | EAP_TEAP_FLAGS_OUTER_TLV_LEN |
		      data->teap_version);
	wpabuf_put_be32(req, outer_tlv_len);

	start = wpabuf_put(req, 0);

	/* RFC 7170, Section 4.2.2: Authority-ID TLV */
	eap_teap_put_tlv(req, TEAP_TLV_AUTHORITY_ID,
			 data->srv_id, data->srv_id_len);

	end = wpabuf_put(req, 0);
	wpabuf_free(data->server_outer_tlvs);
	data->server_outer_tlvs = wpabuf_alloc_copy(start, end - start);
	if (!data->server_outer_tlvs) {
		eap_teap_state(data, FAILURE);
		return NULL;
	}

	eap_teap_state(data, PHASE1);

	return req;
}


static int eap_teap_phase1_done(struct eap_sm *sm, struct eap_teap_data *data)
{
	char cipher[64];

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Phase 1 done, starting Phase 2");

	if (!data->identity && sm->cfg->eap_teap_auth == 2) {
		const char *subject;

		subject = tls_connection_get_peer_subject(data->ssl.conn);
		if (subject) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Peer subject from Phase 1 client certificate: '%s'",
				   subject);
			data->identity = (u8 *) os_strdup(subject);
			data->identity_len = os_strlen(subject);
		}
	}

	data->tls_cs = tls_connection_get_cipher_suite(data->ssl.conn);
	wpa_printf(MSG_DEBUG, "EAP-TEAP: TLS cipher suite 0x%04x",
		   data->tls_cs);

	if (tls_get_cipher(sm->cfg->ssl_ctx, data->ssl.conn,
			   cipher, sizeof(cipher)) < 0) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Failed to get cipher information");
		eap_teap_state(data, FAILURE);
		return -1;
	}
	data->anon_provisioning = os_strstr(cipher, "ADH") != NULL;

	if (data->anon_provisioning)
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Anonymous provisioning");

	if (eap_teap_derive_key_auth(sm, data) < 0) {
		eap_teap_state(data, FAILURE);
		return -1;
	}

	eap_teap_state(data, PHASE2_START);

	return 0;
}


static struct wpabuf * eap_teap_build_phase2_req(struct eap_sm *sm,
						 struct eap_teap_data *data,
						 u8 id)
{
	struct wpabuf *req, *id_tlv = NULL;

	if (sm->cfg->eap_teap_auth == 1 ||
	    (data->phase2_priv && data->phase2_method &&
	     data->phase2_method->vendor == EAP_VENDOR_IETF &&
	     data->phase2_method->method == EAP_TYPE_IDENTITY)) {
		switch (sm->cfg->eap_teap_id) {
		case EAP_TEAP_ID_ALLOW_ANY:
			break;
		case EAP_TEAP_ID_REQUIRE_USER:
		case EAP_TEAP_ID_REQUEST_USER_ACCEPT_MACHINE:
			data->cur_id_type = TEAP_IDENTITY_TYPE_USER;
			id_tlv = eap_teap_tlv_identity_type(data->cur_id_type);
			break;
		case EAP_TEAP_ID_REQUIRE_MACHINE:
		case EAP_TEAP_ID_REQUEST_MACHINE_ACCEPT_USER:
			data->cur_id_type = TEAP_IDENTITY_TYPE_MACHINE;
			id_tlv = eap_teap_tlv_identity_type(data->cur_id_type);
			break;
		case EAP_TEAP_ID_REQUIRE_USER_AND_MACHINE:
			if (data->cur_id_type == TEAP_IDENTITY_TYPE_USER)
				data->cur_id_type = TEAP_IDENTITY_TYPE_MACHINE;
			else
				data->cur_id_type = TEAP_IDENTITY_TYPE_USER;
			id_tlv = eap_teap_tlv_identity_type(data->cur_id_type);
			break;
		}
	}

	if (sm->cfg->eap_teap_auth == 1) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Initiate Basic-Password-Auth");
		data->basic_auth_not_done = 1;
		req = wpabuf_alloc(sizeof(struct teap_tlv_hdr));
		if (!req) {
			wpabuf_free(id_tlv);
			return NULL;
		}
		eap_teap_put_tlv_hdr(req, TEAP_TLV_BASIC_PASSWORD_AUTH_REQ, 0);
		return wpabuf_concat(req, id_tlv);
	}

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Initiate inner EAP method");
	data->inner_eap_not_done = 1;
	if (!data->phase2_priv) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Phase 2 method not initialized");
		wpabuf_free(id_tlv);
		return NULL;
	}

	req = data->phase2_method->buildReq(sm, data->phase2_priv, id);
	if (!req) {
		wpabuf_free(id_tlv);
		return NULL;
	}

	wpa_hexdump_buf_key(MSG_MSGDUMP, "EAP-TEAP: Phase 2 EAP-Request", req);

	return wpabuf_concat(eap_teap_tlv_eap_payload(req), id_tlv);
}


static struct wpabuf * eap_teap_build_crypto_binding(
	struct eap_sm *sm, struct eap_teap_data *data)
{
	struct wpabuf *buf;
	struct teap_tlv_result *result;
	struct teap_tlv_crypto_binding *cb;
	u8 subtype, flags;

	buf = wpabuf_alloc(2 * sizeof(*result) + sizeof(*cb));
	if (!buf)
		return NULL;

	if (data->send_new_pac || data->anon_provisioning ||
	    data->basic_auth_not_done || data->inner_eap_not_done ||
	    data->phase2_method || sm->cfg->eap_teap_separate_result)
		data->final_result = 0;
	else
		data->final_result = 1;

	if (!data->final_result || data->eap_seq > 0 ||
	    sm->cfg->eap_teap_auth == 1) {
		/* Intermediate-Result */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Add Intermediate-Result TLV (status=SUCCESS)");
		result = wpabuf_put(buf, sizeof(*result));
		result->tlv_type = host_to_be16(TEAP_TLV_MANDATORY |
						TEAP_TLV_INTERMEDIATE_RESULT);
		result->length = host_to_be16(2);
		result->status = host_to_be16(TEAP_STATUS_SUCCESS);
	}

	if (data->final_result) {
		/* Result TLV */
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Add Result TLV (status=SUCCESS)");
		result = wpabuf_put(buf, sizeof(*result));
		result->tlv_type = host_to_be16(TEAP_TLV_MANDATORY |
						TEAP_TLV_RESULT);
		result->length = host_to_be16(2);
		result->status = host_to_be16(TEAP_STATUS_SUCCESS);
	}

	/* Crypto-Binding TLV */
	cb = wpabuf_put(buf, sizeof(*cb));
	cb->tlv_type = host_to_be16(TEAP_TLV_MANDATORY |
				    TEAP_TLV_CRYPTO_BINDING);
	cb->length = host_to_be16(sizeof(*cb) - sizeof(struct teap_tlv_hdr));
	cb->version = EAP_TEAP_VERSION;
	cb->received_version = data->peer_version;
	/* FIX: RFC 7170 is not clear on which Flags value to use when
	 * Crypto-Binding TLV is used with Basic-Password-Auth */
	flags = data->cmk_emsk_available ?
		TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC :
		TEAP_CRYPTO_BINDING_MSK_CMAC;
	subtype = TEAP_CRYPTO_BINDING_SUBTYPE_REQUEST;
	cb->subtype = (flags << 4) | subtype;
	if (random_get_bytes(cb->nonce, sizeof(cb->nonce)) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	/*
	 * RFC 7170, Section 4.2.13:
	 * The nonce in a request MUST have its least significant bit set to 0.
	 */
	cb->nonce[sizeof(cb->nonce) - 1] &= ~0x01;

	os_memcpy(data->crypto_binding_nonce, cb->nonce, sizeof(cb->nonce));

	if (eap_teap_compound_mac(data->tls_cs, cb, data->server_outer_tlvs,
				  data->peer_outer_tlvs, data->cmk_msk,
				  cb->msk_compound_mac) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	if (data->cmk_emsk_available &&
	    eap_teap_compound_mac(data->tls_cs, cb, data->server_outer_tlvs,
				  data->peer_outer_tlvs, data->cmk_emsk,
				  cb->emsk_compound_mac) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Add Crypto-Binding TLV: Version %u Received Version %u Flags %u Sub-Type %u",
		   cb->version, cb->received_version, flags, subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Nonce",
		    cb->nonce, sizeof(cb->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: EMSK Compound MAC",
		    cb->emsk_compound_mac, sizeof(cb->emsk_compound_mac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: MSK Compound MAC",
		    cb->msk_compound_mac, sizeof(cb->msk_compound_mac));

	data->check_crypto_binding = true;

	return buf;
}


static struct wpabuf * eap_teap_build_pac(struct eap_sm *sm,
					  struct eap_teap_data *data)
{
	u8 pac_key[EAP_TEAP_PAC_KEY_LEN];
	u8 *pac_buf, *pac_opaque;
	struct wpabuf *buf;
	u8 *pos;
	size_t buf_len, srv_id_info_len, pac_len;
	struct teap_tlv_hdr *pac_tlv;
	struct pac_attr_hdr *pac_info;
	struct teap_tlv_result *result;
	struct os_time now;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Build a new PAC");

	if (random_get_bytes(pac_key, EAP_TEAP_PAC_KEY_LEN) < 0 ||
	    os_get_time(&now) < 0)
		return NULL;
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: Generated PAC-Key",
			pac_key, EAP_TEAP_PAC_KEY_LEN);

	pac_len = (2 + EAP_TEAP_PAC_KEY_LEN) + (2 + 4) +
		(2 + sm->identity_len) + 8;
	pac_buf = os_malloc(pac_len);
	if (!pac_buf)
		return NULL;

	srv_id_info_len = os_strlen(data->srv_id_info);

	pos = pac_buf;
	*pos++ = PAC_OPAQUE_TYPE_KEY;
	*pos++ = EAP_TEAP_PAC_KEY_LEN;
	os_memcpy(pos, pac_key, EAP_TEAP_PAC_KEY_LEN);
	pos += EAP_TEAP_PAC_KEY_LEN;

	wpa_printf(MSG_DEBUG, "EAP-TEAP: PAC-Key lifetime: %u seconds",
		   data->pac_key_lifetime);
	*pos++ = PAC_OPAQUE_TYPE_LIFETIME;
	*pos++ = 4;
	WPA_PUT_BE32(pos, now.sec + data->pac_key_lifetime);
	pos += 4;

	if (sm->identity) {
		wpa_hexdump_ascii(MSG_DEBUG, "EAP-TEAP: PAC-Opaque Identity",
				  sm->identity, sm->identity_len);
		*pos++ = PAC_OPAQUE_TYPE_IDENTITY;
		*pos++ = sm->identity_len;
		os_memcpy(pos, sm->identity, sm->identity_len);
		pos += sm->identity_len;
	}

	pac_len = pos - pac_buf;
	while (pac_len % 8) {
		*pos++ = PAC_OPAQUE_TYPE_PAD;
		pac_len++;
	}

	pac_opaque = os_malloc(pac_len + 8);
	if (!pac_opaque) {
		os_free(pac_buf);
		return NULL;
	}
	if (aes_wrap(data->pac_opaque_encr, sizeof(data->pac_opaque_encr),
		     pac_len / 8, pac_buf, pac_opaque) < 0) {
		os_free(pac_buf);
		os_free(pac_opaque);
		return NULL;
	}
	os_free(pac_buf);

	pac_len += 8;
	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: PAC-Opaque", pac_opaque, pac_len);

	buf_len = sizeof(*pac_tlv) +
		sizeof(struct pac_attr_hdr) + EAP_TEAP_PAC_KEY_LEN +
		sizeof(struct pac_attr_hdr) + pac_len +
		data->srv_id_len + srv_id_info_len + 100 + sizeof(*result);
	buf = wpabuf_alloc(buf_len);
	if (!buf) {
		os_free(pac_opaque);
		return NULL;
	}

	/* Result TLV */
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add Result TLV (status=SUCCESS)");
	result = wpabuf_put(buf, sizeof(*result));
	WPA_PUT_BE16((u8 *) &result->tlv_type,
		     TEAP_TLV_MANDATORY | TEAP_TLV_RESULT);
	WPA_PUT_BE16((u8 *) &result->length, 2);
	WPA_PUT_BE16((u8 *) &result->status, TEAP_STATUS_SUCCESS);

	/* PAC TLV */
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add PAC TLV");
	pac_tlv = wpabuf_put(buf, sizeof(*pac_tlv));
	pac_tlv->tlv_type = host_to_be16(TEAP_TLV_MANDATORY | TEAP_TLV_PAC);

	/* PAC-Key */
	eap_teap_put_tlv(buf, PAC_TYPE_PAC_KEY, pac_key, EAP_TEAP_PAC_KEY_LEN);

	/* PAC-Opaque */
	eap_teap_put_tlv(buf, PAC_TYPE_PAC_OPAQUE, pac_opaque, pac_len);
	os_free(pac_opaque);

	/* PAC-Info */
	pac_info = wpabuf_put(buf, sizeof(*pac_info));
	pac_info->type = host_to_be16(PAC_TYPE_PAC_INFO);

	/* PAC-Lifetime (inside PAC-Info) */
	eap_teap_put_tlv_hdr(buf, PAC_TYPE_CRED_LIFETIME, 4);
	wpabuf_put_be32(buf, now.sec + data->pac_key_lifetime);

	/* A-ID (inside PAC-Info) */
	eap_teap_put_tlv(buf, PAC_TYPE_A_ID, data->srv_id, data->srv_id_len);

	/* Note: headers may be misaligned after A-ID */

	if (sm->identity) {
		eap_teap_put_tlv(buf, PAC_TYPE_I_ID, sm->identity,
				 sm->identity_len);
	}

	/* A-ID-Info (inside PAC-Info) */
	eap_teap_put_tlv(buf, PAC_TYPE_A_ID_INFO, data->srv_id_info,
			 srv_id_info_len);

	/* PAC-Type (inside PAC-Info) */
	eap_teap_put_tlv_hdr(buf, PAC_TYPE_PAC_TYPE, 2);
	wpabuf_put_be16(buf, PAC_TYPE_TUNNEL_PAC);

	/* Update PAC-Info and PAC TLV Length fields */
	pos = wpabuf_put(buf, 0);
	pac_info->len = host_to_be16(pos - (u8 *) (pac_info + 1));
	pac_tlv->length = host_to_be16(pos - (u8 *) (pac_tlv + 1));

	return buf;
}


static int eap_teap_encrypt_phase2(struct eap_sm *sm,
				   struct eap_teap_data *data,
				   struct wpabuf *plain, int piggyback)
{
	struct wpabuf *encr;

	wpa_hexdump_buf_key(MSG_DEBUG, "EAP-TEAP: Encrypting Phase 2 TLVs",
			    plain);
	encr = eap_server_tls_encrypt(sm, &data->ssl, plain);
	wpabuf_free(plain);

	if (!encr)
		return -1;

	if (data->ssl.tls_out && piggyback) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Piggyback Phase 2 data (len=%d) with last Phase 1 Message (len=%d used=%d)",
			   (int) wpabuf_len(encr),
			   (int) wpabuf_len(data->ssl.tls_out),
			   (int) data->ssl.tls_out_pos);
		if (wpabuf_resize(&data->ssl.tls_out, wpabuf_len(encr)) < 0) {
			wpa_printf(MSG_WARNING,
				   "EAP-TEAP: Failed to resize output buffer");
			wpabuf_free(encr);
			return -1;
		}
		wpabuf_put_buf(data->ssl.tls_out, encr);
		wpabuf_free(encr);
	} else {
		wpabuf_free(data->ssl.tls_out);
		data->ssl.tls_out_pos = 0;
		data->ssl.tls_out = encr;
	}

	return 0;
}


static struct wpabuf * eap_teap_buildReq(struct eap_sm *sm, void *priv, u8 id)
{
	struct eap_teap_data *data = priv;
	struct wpabuf *req = NULL;
	int piggyback = 0;
	bool move_to_method = true;

	if (data->ssl.state == FRAG_ACK) {
		return eap_server_tls_build_ack(id, EAP_TYPE_TEAP,
						data->teap_version);
	}

	if (data->ssl.state == WAIT_FRAG_ACK) {
		return eap_server_tls_build_msg(&data->ssl, EAP_TYPE_TEAP,
						data->teap_version, id);
	}

	switch (data->state) {
	case START:
		return eap_teap_build_start(sm, data, id);
	case PHASE1B:
		if (tls_connection_established(sm->cfg->ssl_ctx,
					       data->ssl.conn)) {
			if (eap_teap_phase1_done(sm, data) < 0)
				return NULL;
			if (data->state == PHASE2_START) {
				int res;

				/*
				 * Try to generate Phase 2 data to piggyback
				 * with the end of Phase 1 to avoid extra
				 * roundtrip.
				 */
				wpa_printf(MSG_DEBUG,
					   "EAP-TEAP: Try to start Phase 2");
				res = eap_teap_process_phase2_start(sm, data);
				if (res == 1) {
					req = eap_teap_build_crypto_binding(
						sm, data);
					piggyback = 1;
					break;
				}

				if (res)
					break;
				req = eap_teap_build_phase2_req(sm, data, id);
				piggyback = 1;
			}
		}
		break;
	case PHASE2_ID:
	case PHASE2_BASIC_AUTH:
	case PHASE2_METHOD:
		req = eap_teap_build_phase2_req(sm, data, id);
		break;
	case CRYPTO_BINDING:
		req = eap_teap_build_crypto_binding(sm, data);
		if (req && sm->cfg->eap_teap_auth == 0 &&
		    data->inner_eap_not_done &&
		    !data->phase2_method &&
		    sm->cfg->eap_teap_method_sequence == 0) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Continue with inner EAP authentication for second credential (optimized)");
			eap_teap_state(data, PHASE2_ID);
			if (eap_teap_phase2_init(sm, data, EAP_VENDOR_IETF,
						 EAP_TYPE_IDENTITY) < 0) {
				eap_teap_state(data, FAILURE);
				wpabuf_free(req);
				return NULL;
			}
			move_to_method = false;
		}
		if (data->phase2_method) {
			/*
			 * Include the start of the next EAP method in the
			 * sequence in the same message with Crypto-Binding to
			 * save a round-trip.
			 */
			struct wpabuf *eap;

			eap = eap_teap_build_phase2_req(sm, data, id);
			req = wpabuf_concat(req, eap);
			if (move_to_method)
				eap_teap_state(data, PHASE2_METHOD);
		}
		break;
	case REQUEST_PAC:
		req = eap_teap_build_pac(sm, data);
		break;
	case FAILURE_SEND_RESULT:
		req = eap_teap_tlv_result(TEAP_STATUS_FAILURE, 0);
		if (data->error_code)
			req = wpabuf_concat(
				req, eap_teap_tlv_error(data->error_code));
		break;
	case SUCCESS_SEND_RESULT:
		req = eap_teap_tlv_result(TEAP_STATUS_SUCCESS, 0);
		data->final_result = 1;
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TEAP: %s - unexpected state %d",
			   __func__, data->state);
		return NULL;
	}

	if (req && eap_teap_encrypt_phase2(sm, data, req, piggyback) < 0)
		return NULL;

	return eap_server_tls_build_msg(&data->ssl, EAP_TYPE_TEAP,
					data->teap_version, id);
}


static bool eap_teap_check(struct eap_sm *sm, void *priv,
			   struct wpabuf *respData)
{
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_TEAP, respData, &len);
	if (!pos || len < 1) {
		wpa_printf(MSG_INFO, "EAP-TEAP: Invalid frame");
		return true;
	}

	return false;
}


static int eap_teap_phase2_init(struct eap_sm *sm, struct eap_teap_data *data,
				int vendor, enum eap_type eap_type)
{
	if (data->phase2_priv && data->phase2_method) {
		data->phase2_method->reset(sm, data->phase2_priv);
		data->phase2_method = NULL;
		data->phase2_priv = NULL;
	}
	data->phase2_method = eap_server_get_eap_method(vendor, eap_type);
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

	return data->phase2_priv ? 0 : -1;
}


static int eap_teap_valid_id_type(struct eap_sm *sm, struct eap_teap_data *data,
				  enum teap_identity_types id_type)
{
	if (sm->cfg->eap_teap_id == EAP_TEAP_ID_REQUIRE_USER &&
	    id_type != TEAP_IDENTITY_TYPE_USER)
		return 0;
	if (sm->cfg->eap_teap_id == EAP_TEAP_ID_REQUIRE_MACHINE &&
	    id_type != TEAP_IDENTITY_TYPE_MACHINE)
		return 0;
	if (sm->cfg->eap_teap_id == EAP_TEAP_ID_REQUIRE_USER_AND_MACHINE &&
	    id_type != data->cur_id_type)
		return 0;
	if (sm->cfg->eap_teap_id != EAP_TEAP_ID_ALLOW_ANY &&
	    id_type != TEAP_IDENTITY_TYPE_USER &&
	    id_type != TEAP_IDENTITY_TYPE_MACHINE)
		return 0;
	return 1;
}


static void eap_teap_process_phase2_response(struct eap_sm *sm,
					     struct eap_teap_data *data,
					     u8 *in_data, size_t in_len,
					     enum teap_identity_types id_type)
{
	int next_vendor = EAP_VENDOR_IETF;
	enum eap_type next_type = EAP_TYPE_NONE;
	struct eap_hdr *hdr;
	u8 *pos;
	size_t left;
	struct wpabuf buf;
	const struct eap_method *m = data->phase2_method;
	void *priv = data->phase2_priv;

	if (!priv) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: %s - Phase 2 not initialized?!",
			   __func__);
		return;
	}

	hdr = (struct eap_hdr *) in_data;
	pos = (u8 *) (hdr + 1);

	if (in_len > sizeof(*hdr) && *pos == EAP_TYPE_NAK) {
		left = in_len - sizeof(*hdr);
		wpa_hexdump(MSG_DEBUG,
			    "EAP-TEAP: Phase 2 type Nak'ed; allowed types",
			    pos + 1, left - 1);
#ifdef EAP_SERVER_TNC
		if (m && m->vendor == EAP_VENDOR_IETF &&
		    m->method == EAP_TYPE_TNC) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Peer Nak'ed required TNC negotiation");
			next_vendor = EAP_VENDOR_IETF;
			next_type = eap_teap_req_failure(data, 0);
			eap_teap_phase2_init(sm, data, next_vendor, next_type);
			return;
		}
#endif /* EAP_SERVER_TNC */
		eap_sm_process_nak(sm, pos + 1, left - 1);
		if (sm->user && sm->user_eap_method_index < EAP_MAX_METHODS &&
		    sm->user->methods[sm->user_eap_method_index].method !=
		    EAP_TYPE_NONE) {
			next_vendor = sm->user->methods[
				sm->user_eap_method_index].vendor;
			next_type = sm->user->methods[
				sm->user_eap_method_index++].method;
			wpa_printf(MSG_DEBUG, "EAP-TEAP: try EAP type %u:%u",
				   next_vendor, next_type);
		} else {
			next_vendor = EAP_VENDOR_IETF;
			next_type = eap_teap_req_failure(data, 0);
		}
		eap_teap_phase2_init(sm, data, next_vendor, next_type);
		return;
	}

	wpabuf_set(&buf, in_data, in_len);

	if (m->check(sm, priv, &buf)) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Phase 2 check() asked to ignore the packet");
		eap_teap_req_failure(data, TEAP_ERROR_INNER_METHOD);
		return;
	}

	m->process(sm, priv, &buf);

	if (!m->isDone(sm, priv))
		return;

	if (!m->isSuccess(sm, priv)) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Phase 2 method failed");
		next_vendor = EAP_VENDOR_IETF;
		next_type = eap_teap_req_failure(data, TEAP_ERROR_INNER_METHOD);
		eap_teap_phase2_init(sm, data, next_vendor, next_type);
		return;
	}

	switch (data->state) {
	case PHASE2_ID:
		if (!eap_teap_valid_id_type(sm, data, id_type)) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Provided Identity-Type %u not allowed",
				   id_type);
			eap_teap_req_failure(data, TEAP_ERROR_INNER_METHOD);
			break;
		}
		if (eap_user_get(sm, sm->identity, sm->identity_len, 1) != 0) {
			wpa_hexdump_ascii(MSG_DEBUG,
					  "EAP-TEAP: Phase 2 Identity not found in the user database",
					  sm->identity, sm->identity_len);
			next_vendor = EAP_VENDOR_IETF;
			next_type = eap_teap_req_failure(
				data, TEAP_ERROR_INNER_METHOD);
			break;
		}

		eap_teap_state(data, PHASE2_METHOD);
		if (data->anon_provisioning) {
			/* TODO: Allow any inner EAP method that provides
			 * mutual authentication and EMSK derivation (i.e.,
			 * EAP-pwd or EAP-EKE). */
			next_vendor = EAP_VENDOR_IETF;
			next_type = EAP_TYPE_PWD;
			sm->user_eap_method_index = 0;
		} else {
			next_vendor = sm->user->methods[0].vendor;
			next_type = sm->user->methods[0].method;
			sm->user_eap_method_index = 1;
		}
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Try EAP type %u:%u",
			   next_vendor, next_type);
		break;
	case PHASE2_METHOD:
	case CRYPTO_BINDING:
		eap_teap_update_icmk(sm, data);
		if (data->state == PHASE2_METHOD &&
		    (sm->cfg->eap_teap_id !=
		     EAP_TEAP_ID_REQUIRE_USER_AND_MACHINE ||
		     data->cur_id_type == TEAP_IDENTITY_TYPE_MACHINE))
			data->inner_eap_not_done = 0;
		eap_teap_state(data, CRYPTO_BINDING);
		data->eap_seq++;
		next_vendor = EAP_VENDOR_IETF;
		next_type = EAP_TYPE_NONE;
#ifdef EAP_SERVER_TNC
		if (sm->cfg->tnc && !data->tnc_started) {
			wpa_printf(MSG_DEBUG, "EAP-TEAP: Initialize TNC");
			next_vendor = EAP_VENDOR_IETF;
			next_type = EAP_TYPE_TNC;
			data->tnc_started = 1;
		}
#endif /* EAP_SERVER_TNC */
		break;
	case FAILURE:
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TEAP: %s - unexpected state %d",
			   __func__, data->state);
		break;
	}

	eap_teap_phase2_init(sm, data, next_vendor, next_type);
}


static void eap_teap_process_phase2_eap(struct eap_sm *sm,
					struct eap_teap_data *data,
					u8 *in_data, size_t in_len,
					enum teap_identity_types id_type)
{
	struct eap_hdr *hdr;
	size_t len;

	hdr = (struct eap_hdr *) in_data;
	if (in_len < (int) sizeof(*hdr)) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Too short Phase 2 EAP frame (len=%lu)",
			   (unsigned long) in_len);
		eap_teap_req_failure(data, TEAP_ERROR_INNER_METHOD);
		return;
	}
	len = be_to_host16(hdr->length);
	if (len > in_len) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Length mismatch in Phase 2 EAP frame (len=%lu hdr->length=%lu)",
			   (unsigned long) in_len, (unsigned long) len);
		eap_teap_req_failure(data, TEAP_ERROR_INNER_METHOD);
		return;
	}
	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Received Phase 2: code=%d identifier=%d length=%lu",
		   hdr->code, hdr->identifier,
		   (unsigned long) len);
	switch (hdr->code) {
	case EAP_CODE_RESPONSE:
		eap_teap_process_phase2_response(sm, data, (u8 *) hdr, len,
						 id_type);
		break;
	default:
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Unexpected code=%d in Phase 2 EAP header",
			   hdr->code);
		break;
	}
}


static void eap_teap_process_basic_auth_resp(struct eap_sm *sm,
					     struct eap_teap_data *data,
					     u8 *in_data, size_t in_len,
					     enum teap_identity_types id_type)
{
	u8 *pos, *end, *username, *password, *new_id;
	u8 userlen, passlen;

	if (!eap_teap_valid_id_type(sm, data, id_type)) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Provided Identity-Type %u not allowed",
			   id_type);
		eap_teap_req_failure(data, 0);
		return;
	}

	pos = in_data;
	end = pos + in_len;

	if (end - pos < 1) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No room for Basic-Password-Auth-Resp Userlen field");
		eap_teap_req_failure(data, 0);
		return;
	}
	userlen = *pos++;
	if (end - pos < userlen) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Truncated Basic-Password-Auth-Resp Username field");
		eap_teap_req_failure(data, 0);
		return;
	}
	username = pos;
	pos += userlen;
	wpa_hexdump_ascii(MSG_DEBUG,
			  "EAP-TEAP: Basic-Password-Auth-Resp Username",
			  username, userlen);

	if (end - pos < 1) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No room for Basic-Password-Auth-Resp Passlen field");
		eap_teap_req_failure(data, 0);
		return;
	}
	passlen = *pos++;
	if (end - pos < passlen) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Truncated Basic-Password-Auth-Resp Password field");
		eap_teap_req_failure(data, 0);
		return;
	}
	password = pos;
	pos += passlen;
	wpa_hexdump_ascii_key(MSG_DEBUG,
			      "EAP-TEAP: Basic-Password-Auth-Resp Password",
			      password, passlen);

	if (end > pos) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Unexpected %d extra octet(s) at the end of Basic-Password-Auth-Resp TLV",
			   (int) (end - pos));
		eap_teap_req_failure(data, 0);
		return;
	}

	if (eap_user_get(sm, username, userlen, 1) != 0) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Username not found in the user database");
		eap_teap_req_failure(data, 0);
		return;
	}

	if (!sm->user || !sm->user->password || sm->user->password_hash) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: No plaintext user password configured");
		eap_teap_req_failure(data, 0);
		return;
	}

	if (sm->user->password_len != passlen ||
	    os_memcmp_const(sm->user->password, password, passlen) != 0) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Invalid password");
		eap_teap_req_failure(data, 0);
		return;
	}

	wpa_printf(MSG_DEBUG, "EAP-TEAP: Correct password");
	new_id = os_memdup(username, userlen);
	if (new_id) {
		os_free(sm->identity);
		sm->identity = new_id;
		sm->identity_len = userlen;
	}
	if (sm->cfg->eap_teap_id != EAP_TEAP_ID_REQUIRE_USER_AND_MACHINE ||
	    data->cur_id_type == TEAP_IDENTITY_TYPE_MACHINE)
		data->basic_auth_not_done = 0;
	eap_teap_state(data, CRYPTO_BINDING);
	eap_teap_update_icmk(sm, data);
}


static int eap_teap_parse_tlvs(struct wpabuf *data,
			       struct eap_teap_tlv_parse *tlv)
{
	u16 tlv_type;
	int mandatory, res;
	size_t len;
	u8 *pos, *end;

	os_memset(tlv, 0, sizeof(*tlv));

	pos = wpabuf_mhead(data);
	end = pos + wpabuf_len(data);
	while (end - pos > 4) {
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
				/* TODO: generate NAK TLV */
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


static int eap_teap_validate_crypto_binding(
	struct eap_teap_data *data, const struct teap_tlv_crypto_binding *cb,
	size_t bind_len)
{
	u8 flags, subtype;

	subtype = cb->subtype & 0x0f;
	flags = cb->subtype >> 4;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Reply Crypto-Binding TLV: Version %u Received Version %u Flags %u Sub-Type %u",
		   cb->version, cb->received_version, flags, subtype);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Nonce",
		    cb->nonce, sizeof(cb->nonce));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: EMSK Compound MAC",
		    cb->emsk_compound_mac, sizeof(cb->emsk_compound_mac));
	wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: MSK Compound MAC",
		    cb->msk_compound_mac, sizeof(cb->msk_compound_mac));

	if (cb->version != EAP_TEAP_VERSION ||
	    cb->received_version != data->peer_version) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Unexpected version in Crypto-Binding: Version %u Received Version %u",
			   cb->version, cb->received_version);
		return -1;
	}

	if (flags < 1 || flags > 3) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Unexpected Flags in Crypto-Binding: %u",
			   flags);
		return -1;
	}

	if (subtype != TEAP_CRYPTO_BINDING_SUBTYPE_RESPONSE) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Unexpected Sub-Type in Crypto-Binding: %u",
			   subtype);
		return -1;
	}

	if (os_memcmp_const(data->crypto_binding_nonce, cb->nonce,
			    EAP_TEAP_NONCE_LEN - 1) != 0 ||
	    (data->crypto_binding_nonce[EAP_TEAP_NONCE_LEN - 1] | 1) !=
	    cb->nonce[EAP_TEAP_NONCE_LEN - 1]) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Invalid Nonce in Crypto-Binding");
		return -1;
	}

	if (flags == TEAP_CRYPTO_BINDING_MSK_CMAC ||
	    flags == TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC) {
		u8 msk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];

		if (eap_teap_compound_mac(data->tls_cs, cb,
					  data->server_outer_tlvs,
					  data->peer_outer_tlvs, data->cmk_msk,
					  msk_compound_mac) < 0)
			return -1;
		if (os_memcmp_const(msk_compound_mac, cb->msk_compound_mac,
				    EAP_TEAP_COMPOUND_MAC_LEN) != 0) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TEAP: Calculated MSK Compound MAC",
				    msk_compound_mac,
				    EAP_TEAP_COMPOUND_MAC_LEN);
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: MSK Compound MAC did not match");
			return -1;
		}
	}

	if ((flags == TEAP_CRYPTO_BINDING_EMSK_CMAC ||
	     flags == TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC) &&
	    data->cmk_emsk_available) {
		u8 emsk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];

		if (eap_teap_compound_mac(data->tls_cs, cb,
					  data->server_outer_tlvs,
					  data->peer_outer_tlvs, data->cmk_emsk,
					  emsk_compound_mac) < 0)
			return -1;
		if (os_memcmp_const(emsk_compound_mac, cb->emsk_compound_mac,
				    EAP_TEAP_COMPOUND_MAC_LEN) != 0) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TEAP: Calculated EMSK Compound MAC",
				    emsk_compound_mac,
				    EAP_TEAP_COMPOUND_MAC_LEN);
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: EMSK Compound MAC did not match");
			return -1;
		}
	}

	if (flags == TEAP_CRYPTO_BINDING_EMSK_CMAC &&
	    !data->cmk_emsk_available) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Peer included only EMSK Compound MAC, but no locally generated inner EAP EMSK to validate this");
		return -1;
	}

	return 0;
}


static int eap_teap_pac_type(u8 *pac, size_t len, u16 type)
{
	struct teap_attr_pac_type *tlv;

	if (!pac || len != sizeof(*tlv))
		return 0;

	tlv = (struct teap_attr_pac_type *) pac;

	return be_to_host16(tlv->type) == PAC_TYPE_PAC_TYPE &&
		be_to_host16(tlv->length) == 2 &&
		be_to_host16(tlv->pac_type) == type;
}


static void eap_teap_process_phase2_tlvs(struct eap_sm *sm,
					 struct eap_teap_data *data,
					 struct wpabuf *in_data)
{
	struct eap_teap_tlv_parse tlv;
	bool check_crypto_binding = data->state == CRYPTO_BINDING ||
		data->check_crypto_binding;

	if (eap_teap_parse_tlvs(in_data, &tlv) < 0) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Failed to parse received Phase 2 TLVs");
		return;
	}

	if (tlv.result == TEAP_STATUS_FAILURE) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Result TLV indicated failure");
		eap_teap_state(data, FAILURE);
		return;
	}

	if (tlv.nak) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Peer NAK'ed Vendor-Id %u NAK-Type %u",
			   WPA_GET_BE32(tlv.nak), WPA_GET_BE16(tlv.nak + 4));
		eap_teap_state(data, FAILURE_SEND_RESULT);
		return;
	}

	if (data->state == REQUEST_PAC) {
		u16 type, len, res;

		if (!tlv.pac || tlv.pac_len < 6) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: No PAC Acknowledgement received");
			eap_teap_state(data, FAILURE);
			return;
		}

		type = WPA_GET_BE16(tlv.pac);
		len = WPA_GET_BE16(tlv.pac + 2);
		res = WPA_GET_BE16(tlv.pac + 4);

		if (type != PAC_TYPE_PAC_ACKNOWLEDGEMENT || len != 2 ||
		    res != TEAP_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: PAC TLV did not contain acknowledgement");
			eap_teap_state(data, FAILURE);
			return;
		}

		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: PAC-Acknowledgement received - PAC provisioning succeeded");
		eap_teap_state(data, SUCCESS);
		return;
	}

	if (check_crypto_binding) {
		if (!tlv.crypto_binding) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: No Crypto-Binding TLV received");
			eap_teap_state(data, FAILURE);
			return;
		}

		if (data->final_result &&
		    tlv.result != TEAP_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Crypto-Binding TLV without Success Result");
			eap_teap_state(data, FAILURE);
			return;
		}

		if (sm->cfg->eap_teap_auth != 1 &&
		    !data->skipped_inner_auth &&
		    tlv.iresult != TEAP_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Crypto-Binding TLV without intermediate Success Result");
			eap_teap_state(data, FAILURE);
			return;
		}

		if (eap_teap_validate_crypto_binding(data, tlv.crypto_binding,
						     tlv.crypto_binding_len)) {
			eap_teap_state(data, FAILURE);
			return;
		}

		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Valid Crypto-Binding TLV received");
		data->check_crypto_binding = false;
		if (data->final_result) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Authentication completed successfully");
		}

		if (data->anon_provisioning &&
		    sm->cfg->eap_fast_prov != ANON_PROV &&
		    sm->cfg->eap_fast_prov != BOTH_PROV) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Client is trying to use unauthenticated provisioning which is disabled");
			eap_teap_state(data, FAILURE);
			return;
		}

		if (sm->cfg->eap_fast_prov != AUTH_PROV &&
		    sm->cfg->eap_fast_prov != BOTH_PROV &&
		    tlv.request_action == TEAP_REQUEST_ACTION_PROCESS_TLV &&
		    eap_teap_pac_type(tlv.pac, tlv.pac_len,
				      PAC_TYPE_TUNNEL_PAC)) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Client is trying to use authenticated provisioning which is disabled");
			eap_teap_state(data, FAILURE);
			return;
		}

		if (data->anon_provisioning ||
		    (tlv.request_action == TEAP_REQUEST_ACTION_PROCESS_TLV &&
		     eap_teap_pac_type(tlv.pac, tlv.pac_len,
				       PAC_TYPE_TUNNEL_PAC))) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Requested a new Tunnel PAC");
			eap_teap_state(data, REQUEST_PAC);
		} else if (data->send_new_pac) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Server triggered re-keying of Tunnel PAC");
			eap_teap_state(data, REQUEST_PAC);
		} else if (data->final_result) {
			eap_teap_state(data, SUCCESS);
		} else if (sm->cfg->eap_teap_separate_result) {
			eap_teap_state(data, SUCCESS_SEND_RESULT);
		}
	}

	if (tlv.basic_auth_resp) {
		if (sm->cfg->eap_teap_auth != 1) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Unexpected Basic-Password-Auth-Resp when trying to use inner EAP");
			eap_teap_state(data, FAILURE);
			return;
		}
		eap_teap_process_basic_auth_resp(sm, data, tlv.basic_auth_resp,
						 tlv.basic_auth_resp_len,
						 tlv.identity_type);
	}

	if (tlv.eap_payload_tlv) {
		if (sm->cfg->eap_teap_auth == 1) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Unexpected EAP Payload TLV when trying to use Basic-Password-Auth");
			eap_teap_state(data, FAILURE);
			return;
		}
		eap_teap_process_phase2_eap(sm, data, tlv.eap_payload_tlv,
					    tlv.eap_payload_tlv_len,
					    tlv.identity_type);
	}

	if (data->state == SUCCESS_SEND_RESULT &&
	    tlv.result == TEAP_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Peer agreed with final success - authentication completed");
		eap_teap_state(data, SUCCESS);
	} else if (check_crypto_binding && data->state == CRYPTO_BINDING &&
		   sm->cfg->eap_teap_auth == 1 && data->basic_auth_not_done) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Continue with basic password authentication for second credential");
		eap_teap_state(data, PHASE2_BASIC_AUTH);
	} else if (check_crypto_binding && data->state == CRYPTO_BINDING &&
		   sm->cfg->eap_teap_auth == 0 && data->inner_eap_not_done &&
		   sm->cfg->eap_teap_method_sequence == 1) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Continue with inner EAP authentication for second credential");
		eap_teap_state(data, PHASE2_ID);
		if (eap_teap_phase2_init(sm, data, EAP_VENDOR_IETF,
					 EAP_TYPE_IDENTITY) < 0)
			eap_teap_state(data, FAILURE);
	}
}


static void eap_teap_process_phase2(struct eap_sm *sm,
				    struct eap_teap_data *data,
				    struct wpabuf *in_buf)
{
	struct wpabuf *in_decrypted;

	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Received %lu bytes encrypted data for Phase 2",
		   (unsigned long) wpabuf_len(in_buf));

	if (data->pending_phase2_resp) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Pending Phase 2 response - skip decryption and use old data");
		eap_teap_process_phase2_tlvs(sm, data,
					     data->pending_phase2_resp);
		wpabuf_free(data->pending_phase2_resp);
		data->pending_phase2_resp = NULL;
		return;
	}

	in_decrypted = tls_connection_decrypt(sm->cfg->ssl_ctx, data->ssl.conn,
					      in_buf);
	if (!in_decrypted) {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Failed to decrypt Phase 2 data");
		eap_teap_state(data, FAILURE);
		return;
	}

	wpa_hexdump_buf_key(MSG_DEBUG, "EAP-TEAP: Decrypted Phase 2 TLVs",
			    in_decrypted);

	eap_teap_process_phase2_tlvs(sm, data, in_decrypted);

	if (sm->method_pending == METHOD_PENDING_WAIT) {
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Phase 2 method is in pending wait state - save decrypted response");
		wpabuf_free(data->pending_phase2_resp);
		data->pending_phase2_resp = in_decrypted;
		return;
	}

	wpabuf_free(in_decrypted);
}


static int eap_teap_process_version(struct eap_sm *sm, void *priv,
				    int peer_version)
{
	struct eap_teap_data *data = priv;

	if (peer_version < 1) {
		/* Version 1 was the first defined version, so reject 0 */
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Peer used unknown TEAP version %u",
			   peer_version);
		return -1;
	}

	if (peer_version < data->teap_version) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: peer ver=%u, own ver=%u; "
			   "use version %u",
			   peer_version, data->teap_version, peer_version);
		data->teap_version = peer_version;
	}

	data->peer_version = peer_version;

	return 0;
}


static int eap_teap_process_phase1(struct eap_sm *sm,
				   struct eap_teap_data *data)
{
	if (eap_server_tls_phase1(sm, &data->ssl) < 0) {
		wpa_printf(MSG_INFO, "EAP-TEAP: TLS processing failed");
		eap_teap_state(data, FAILURE);
		return -1;
	}

	if (!tls_connection_established(sm->cfg->ssl_ctx, data->ssl.conn) ||
	    wpabuf_len(data->ssl.tls_out) > 0)
		return 1;

	/*
	 * Phase 1 was completed with the received message (e.g., when using
	 * abbreviated handshake), so Phase 2 can be started immediately
	 * without having to send through an empty message to the peer.
	 */

	return eap_teap_phase1_done(sm, data);
}


static int eap_teap_process_phase2_start(struct eap_sm *sm,
					 struct eap_teap_data *data)
{
	int next_vendor;
	enum eap_type next_type;

	if (data->identity) {
		/* Used PAC and identity is from PAC-Opaque */
		os_free(sm->identity);
		sm->identity = data->identity;
		data->identity = NULL;
		sm->identity_len = data->identity_len;
		data->identity_len = 0;
		if (eap_user_get(sm, sm->identity, sm->identity_len, 1) != 0) {
			wpa_hexdump_ascii(MSG_DEBUG,
					  "EAP-TEAP: Phase 2 Identity not found in the user database",
					  sm->identity, sm->identity_len);
			next_vendor = EAP_VENDOR_IETF;
			next_type = EAP_TYPE_NONE;
			eap_teap_state(data, PHASE2_METHOD);
		} else if (sm->cfg->eap_teap_pac_no_inner ||
			sm->cfg->eap_teap_auth == 2) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Used PAC or client certificate and identity already known - skip inner auth");
			data->skipped_inner_auth = 1;
			/* FIX: Need to derive CMK here. However, how is that
			 * supposed to be done? RFC 7170 does not tell that for
			 * the no-inner-auth case. */
			eap_teap_derive_cmk_basic_pw_auth(data->tls_cs,
							  data->simck_msk,
							  data->cmk_msk);
			eap_teap_state(data, CRYPTO_BINDING);
			return 1;
		} else if (sm->cfg->eap_teap_auth == 1) {
			eap_teap_state(data, PHASE2_BASIC_AUTH);
			return 1;
		} else {
			wpa_printf(MSG_DEBUG,
				   "EAP-TEAP: Identity already known - skip Phase 2 Identity Request");
			next_vendor = sm->user->methods[0].vendor;
			next_type = sm->user->methods[0].method;
			sm->user_eap_method_index = 1;
			eap_teap_state(data, PHASE2_METHOD);
		}

	} else if (sm->cfg->eap_teap_auth == 1) {
		eap_teap_state(data, PHASE2_BASIC_AUTH);
		return 0;
	} else {
		eap_teap_state(data, PHASE2_ID);
		next_vendor = EAP_VENDOR_IETF;
		next_type = EAP_TYPE_IDENTITY;
	}

	return eap_teap_phase2_init(sm, data, next_vendor, next_type);
}


static void eap_teap_process_msg(struct eap_sm *sm, void *priv,
				 const struct wpabuf *respData)
{
	struct eap_teap_data *data = priv;

	switch (data->state) {
	case PHASE1:
	case PHASE1B:
		if (eap_teap_process_phase1(sm, data))
			break;

		/* fall through */
	case PHASE2_START:
		eap_teap_process_phase2_start(sm, data);
		break;
	case PHASE2_ID:
	case PHASE2_BASIC_AUTH:
	case PHASE2_METHOD:
	case CRYPTO_BINDING:
	case REQUEST_PAC:
	case SUCCESS_SEND_RESULT:
		eap_teap_process_phase2(sm, data, data->ssl.tls_in);
		break;
	case FAILURE_SEND_RESULT:
		/* Protected failure result indication completed. Ignore the
		 * received message (which is supposed to include Result TLV
		 * indicating failure) and terminate exchange with cleartext
		 * EAP-Failure. */
		eap_teap_state(data, FAILURE);
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Unexpected state %d in %s",
			   data->state, __func__);
		break;
	}
}


static void eap_teap_process(struct eap_sm *sm, void *priv,
			     struct wpabuf *respData)
{
	struct eap_teap_data *data = priv;
	const u8 *pos;
	size_t len;
	struct wpabuf *resp = respData;
	u8 flags;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_TEAP, respData, &len);
	if (!pos || len < 1)
		return;

	flags = *pos++;
	len--;

	if (flags & EAP_TEAP_FLAGS_OUTER_TLV_LEN) {
		/* Extract Outer TLVs from the message before common TLS
		 * processing */
		u32 message_len = 0, outer_tlv_len;
		const u8 *hdr;

		if (data->state != PHASE1) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Unexpected Outer TLVs in a message that is not the first message from the peer");
			return;
		}

		if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
			if (len < 4) {
				wpa_printf(MSG_INFO,
					   "EAP-TEAP: Too short message to include Message Length field");
				return;
			}

			message_len = WPA_GET_BE32(pos);
			pos += 4;
			len -= 4;
			if (message_len < 4) {
				wpa_printf(MSG_INFO,
					   "EAP-TEAP: Message Length field has too msall value to include Outer TLV Length field");
				return;
			}
		}

		if (len < 4) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short message to include Outer TLVs Length field");
			return;
		}

		outer_tlv_len = WPA_GET_BE32(pos);
		pos += 4;
		len -= 4;

		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Message Length %u Outer TLV Length %u",
			  message_len, outer_tlv_len);
		if (len < outer_tlv_len) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short message to include Outer TLVs field");
			return;
		}

		if (message_len &&
		    (message_len < outer_tlv_len ||
		     message_len < 4 + outer_tlv_len)) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Message Length field has too small value to include Outer TLVs");
			return;
		}

		if (wpabuf_len(respData) < 4 + outer_tlv_len ||
		    len < outer_tlv_len)
			return;
		resp = wpabuf_alloc(wpabuf_len(respData) - 4 - outer_tlv_len);
		if (!resp)
			return;
		hdr = wpabuf_head(respData);
		wpabuf_put_u8(resp, *hdr++); /* Code */
		wpabuf_put_u8(resp, *hdr++); /* Identifier */
		wpabuf_put_be16(resp, WPA_GET_BE16(hdr) - 4 - outer_tlv_len);
		hdr += 2;
		wpabuf_put_u8(resp, *hdr++); /* Type */
		/* Flags | Ver */
		wpabuf_put_u8(resp, flags & ~EAP_TEAP_FLAGS_OUTER_TLV_LEN);

		if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED)
			wpabuf_put_be32(resp, message_len - 4 - outer_tlv_len);

		wpabuf_put_data(resp, pos, len - outer_tlv_len);
		pos += len - outer_tlv_len;
		wpabuf_free(data->peer_outer_tlvs);
		data->peer_outer_tlvs = wpabuf_alloc_copy(pos, outer_tlv_len);
		if (!data->peer_outer_tlvs)
			return;
		wpa_hexdump_buf(MSG_DEBUG, "EAP-TEAP: Outer TLVs",
				data->peer_outer_tlvs);

		wpa_hexdump_buf(MSG_DEBUG,
				"EAP-TEAP: TLS Data message after Outer TLV removal",
				resp);
		pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_TEAP, resp,
				       &len);
		if (!pos || len < 1) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Invalid frame after Outer TLV removal");
			return;
		}
	}

	if (data->state == PHASE1)
		eap_teap_state(data, PHASE1B);

	if (eap_server_tls_process(sm, &data->ssl, resp, data,
				   EAP_TYPE_TEAP, eap_teap_process_version,
				   eap_teap_process_msg) < 0)
		eap_teap_state(data, FAILURE);

	if (resp != respData)
		wpabuf_free(resp);
}


static bool eap_teap_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_teap_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	u8 *eapKeyData;

	if (data->state != SUCCESS)
		return NULL;

	eapKeyData = os_malloc(EAP_TEAP_KEY_LEN);
	if (!eapKeyData)
		return NULL;

	/* FIX: RFC 7170 does not describe whether MSK or EMSK based S-IMCK[j]
	 * is used in this derivation */
	if (eap_teap_derive_eap_msk(data->tls_cs, data->simck_msk,
				    eapKeyData) < 0) {
		os_free(eapKeyData);
		return NULL;
	}
	*len = EAP_TEAP_KEY_LEN;

	return eapKeyData;
}


static u8 * eap_teap_get_emsk(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	u8 *eapKeyData;

	if (data->state != SUCCESS)
		return NULL;

	eapKeyData = os_malloc(EAP_EMSK_LEN);
	if (!eapKeyData)
		return NULL;

	/* FIX: RFC 7170 does not describe whether MSK or EMSK based S-IMCK[j]
	 * is used in this derivation */
	if (eap_teap_derive_eap_emsk(data->tls_cs, data->simck_msk,
				     eapKeyData) < 0) {
		os_free(eapKeyData);
		return NULL;
	}
	*len = EAP_EMSK_LEN;

	return eapKeyData;
}


static bool eap_teap_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_teap_data *data = priv;

	return data->state == SUCCESS;
}


static u8 * eap_teap_get_session_id(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_teap_data *data = priv;
	const size_t max_id_len = 100;
	int res;
	u8 *id;

	if (data->state != SUCCESS)
		return NULL;

	id = os_malloc(max_id_len);
	if (!id)
		return NULL;

	id[0] = EAP_TYPE_TEAP;
	res = tls_get_tls_unique(data->ssl.conn, id + 1, max_id_len - 1);
	if (res < 0) {
		os_free(id);
		wpa_printf(MSG_ERROR, "EAP-TEAP: Failed to derive Session-Id");
		return NULL;
	}

	*len = 1 + res;
	wpa_hexdump(MSG_DEBUG, "EAP-TEAP: Derived Session-Id", id, *len);
	return id;
}


int eap_server_teap_register(void)
{
	struct eap_method *eap;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_TEAP, "TEAP");
	if (!eap)
		return -1;

	eap->init = eap_teap_init;
	eap->reset = eap_teap_reset;
	eap->buildReq = eap_teap_buildReq;
	eap->check = eap_teap_check;
	eap->process = eap_teap_process;
	eap->isDone = eap_teap_isDone;
	eap->getKey = eap_teap_getKey;
	eap->get_emsk = eap_teap_get_emsk;
	eap->isSuccess = eap_teap_isSuccess;
	eap->getSessionId = eap_teap_get_session_id;

	return eap_server_method_register(eap);
}
