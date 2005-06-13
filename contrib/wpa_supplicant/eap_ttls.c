/*
 * WPA Supplicant / EAP-TTLS (draft-ietf-pppext-eap-ttls-03.txt)
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

#include "common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "ms_funcs.h"
#include "md5.h"
#include "tls.h"
#include "eap_ttls.h"


static void eap_ttls_deinit(struct eap_sm *sm, void *priv);


struct eap_ttls_data {
	struct eap_ssl_data ssl;

	const struct eap_method *phase2_method;
	void *phase2_priv;
	int phase2_success;
	int phase2_start;

	enum {
		EAP_TTLS_PHASE2_EAP,
		EAP_TTLS_PHASE2_MSCHAPV2,
		EAP_TTLS_PHASE2_MSCHAP,
		EAP_TTLS_PHASE2_PAP,
		EAP_TTLS_PHASE2_CHAP
	} phase2_type;
	u8 phase2_eap_type;
	u8 *phase2_eap_types;
	size_t num_phase2_eap_types;

	u8 auth_response[20];
	int auth_response_valid;
	u8 ident;
	int resuming; /* starting a resumed session */
	int reauth; /* reauthentication */
	u8 *key_data;

	u8 *pending_phase2_req;
	size_t pending_phase2_req_len;
};


static void * eap_ttls_init(struct eap_sm *sm)
{
	struct eap_ttls_data *data;
	struct wpa_ssid *config = eap_get_config(sm);
	char *selected;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	memset(data, 0, sizeof(*data));
	selected = "EAP";
	data->phase2_type = EAP_TTLS_PHASE2_EAP;
	if (config && config->phase2) {
		if (strstr(config->phase2, "autheap=")) {
			selected = "EAP";
			data->phase2_type = EAP_TTLS_PHASE2_EAP;
		} else if (strstr(config->phase2, "auth=MSCHAPV2")) {
			selected = "MSCHAPV2";
			data->phase2_type = EAP_TTLS_PHASE2_MSCHAPV2;
		} else if (strstr(config->phase2, "auth=MSCHAP")) {
			selected = "MSCHAP";
			data->phase2_type = EAP_TTLS_PHASE2_MSCHAP;
		} else if (strstr(config->phase2, "auth=PAP")) {
			selected = "PAP";
			data->phase2_type = EAP_TTLS_PHASE2_PAP;
		} else if (strstr(config->phase2, "auth=CHAP")) {
			selected = "CHAP";
			data->phase2_type = EAP_TTLS_PHASE2_CHAP;
		}
	}
	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase2 type: %s", selected);

	if (data->phase2_type == EAP_TTLS_PHASE2_EAP) {
		if (config && config->phase2) {
			char *start, *pos, *buf;
			u8 method, *methods = NULL, *_methods;
			size_t num_methods = 0;
			start = buf = strdup(config->phase2);
			if (buf == NULL) {
				eap_ttls_deinit(sm, data);
				return NULL;
			}
			while (start && *start != '\0') {
				pos = strstr(start, "autheap=");
				if (pos == NULL)
					break;
				if (start != pos && *(pos - 1) != ' ') {
					start = pos + 8;
					continue;
				}

				start = pos + 8;
				pos = strchr(start, ' ');
				if (pos)
					*pos++ = '\0';
				method = eap_get_phase2_type(start);
				if (method == EAP_TYPE_NONE) {
					wpa_printf(MSG_ERROR, "EAP-TTLS: "
						   "Unsupported Phase2 EAP "
						   "method '%s'", start);
				} else {
					num_methods++;
					_methods = realloc(methods,
							   num_methods);
					if (_methods == NULL) {
						free(methods);
						eap_ttls_deinit(sm, data);
						return NULL;
					}
					methods = _methods;
					methods[num_methods - 1] = method;
				}

				start = pos;
			}
			free(buf);
			data->phase2_eap_types = methods;
			data->num_phase2_eap_types = num_methods;
		}
		if (data->phase2_eap_types == NULL) {
			data->phase2_eap_types = eap_get_phase2_types(
				config, &data->num_phase2_eap_types);
		}
		if (data->phase2_eap_types == NULL) {
			wpa_printf(MSG_ERROR, "EAP-TTLS: No Phase2 EAP method "
				   "available");
			eap_ttls_deinit(sm, data);
			return NULL;
		}
		wpa_hexdump(MSG_DEBUG, "EAP-TTLS: Phase2 EAP types",
			    data->phase2_eap_types,
			    data->num_phase2_eap_types);
		data->phase2_eap_type = EAP_TYPE_NONE;
	}


	if (eap_tls_ssl_init(sm, &data->ssl, config)) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Failed to initialize SSL.");
		eap_ttls_deinit(sm, data);
		return NULL;
	}

	return data;
}


static void eap_ttls_deinit(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	if (data == NULL)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->deinit(sm, data->phase2_priv);
	free(data->phase2_eap_types);
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data->key_data);
	free(data->pending_phase2_req);
	free(data);
}


static int eap_ttls_encrypt(struct eap_sm *sm, struct eap_ttls_data *data,
			    int id, u8 *plain, size_t plain_len,
			    u8 **out_data, size_t *out_len)
{
	int res;
	u8 *pos;
	struct eap_hdr *resp;

	/* TODO: add support for fragmentation, if needed. This will need to
	 * add TLS Message Length field, if the frame is fragmented. */
	resp = malloc(sizeof(struct eap_hdr) + 2 + data->ssl.tls_out_limit);
	if (resp == NULL)
		return -1;

	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;

	pos = (u8 *) (resp + 1);
	*pos++ = EAP_TYPE_TTLS;
	*pos++ = 0;

	res = tls_connection_encrypt(sm->ssl_ctx, data->ssl.conn,
				     plain, plain_len,
				     pos, data->ssl.tls_out_limit);
	if (res < 0) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Failed to encrypt Phase 2 "
			   "data");
		free(resp);
		return -1;
	}

	*out_len = sizeof(struct eap_hdr) + 2 + res;
	resp->length = host_to_be16(*out_len);
	*out_data = (u8 *) resp;
	return 0;
}


static u8 * eap_ttls_avp_hdr(u8 *avphdr, u32 avp_code, u32 vendor_id,
			     int mandatory, size_t len)
{
	struct ttls_avp_vendor *avp;
	u8 flags;
	size_t hdrlen;

	avp = (struct ttls_avp_vendor *) avphdr;
	flags = mandatory ? AVP_FLAGS_MANDATORY : 0;
	if (vendor_id) {
		flags |= AVP_FLAGS_VENDOR;
		hdrlen = sizeof(*avp);
		avp->vendor_id = host_to_be32(vendor_id);
	} else {
		hdrlen = sizeof(struct ttls_avp);
	}

	avp->avp_code = host_to_be32(avp_code);
	avp->avp_length = host_to_be32((flags << 24) | (hdrlen + len));

	return avphdr + hdrlen;
}


static u8 * eap_ttls_avp_add(u8 *start, u8 *avphdr, u32 avp_code,
			     u32 vendor_id, int mandatory,
			     u8 *data, size_t len)
{
	u8 *pos;
	pos = eap_ttls_avp_hdr(avphdr, avp_code, vendor_id, mandatory, len);
	memcpy(pos, data, len);
	pos += len;
	AVP_PAD(start, pos);
	return pos;
}


static int eap_ttls_avp_encapsulate(u8 **resp, size_t *resp_len, u32 avp_code,
				    int mandatory)
{
	u8 *avp, *pos;

	avp = malloc(sizeof(struct ttls_avp) + *resp_len + 4);
	if (avp == NULL) {
		free(*resp);
		*resp = NULL;
		*resp_len = 0;
		return -1;
	}

	pos = eap_ttls_avp_hdr(avp, avp_code, 0, mandatory, *resp_len);
	memcpy(pos, *resp, *resp_len);
	pos += *resp_len;
	AVP_PAD(avp, pos);
	free(*resp);
	*resp = avp;
	*resp_len = pos - avp;
	return 0;
}


static int eap_ttls_phase2_nak(struct eap_sm *sm,
			       struct eap_ttls_data *data,
			       struct eap_hdr *hdr,
			       u8 **resp, size_t *resp_len)
{
	struct eap_hdr *resp_hdr;
	u8 *pos = (u8 *) (hdr + 1);

	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 Request: Nak type=%d", *pos);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: Allowed Phase2 EAP types",
		    data->phase2_eap_types, data->num_phase2_eap_types);
	*resp_len = sizeof(struct eap_hdr) + 1 + data->num_phase2_eap_types;
	*resp = malloc(*resp_len);
	if (*resp == NULL)
		return -1;

	resp_hdr = (struct eap_hdr *) (*resp);
	resp_hdr->code = EAP_CODE_RESPONSE;
	resp_hdr->identifier = hdr->identifier;
	resp_hdr->length = host_to_be16(*resp_len);
	pos = (u8 *) (resp_hdr + 1);
	*pos++ = EAP_TYPE_NAK;
	memcpy(pos, data->phase2_eap_types, data->num_phase2_eap_types);

	return 0;
}


static int eap_ttls_phase2_request_eap(struct eap_sm *sm,
				       struct eap_ttls_data *data,
				       struct eap_method_ret *ret,
				       struct eap_hdr *req,
				       struct eap_hdr *hdr,
				       u8 **resp, size_t *resp_len)
{
	size_t len = be_to_host16(hdr->length);
	u8 *pos;
	struct eap_method_ret iret;
	struct wpa_ssid *config = eap_get_config(sm);

	if (len <= sizeof(struct eap_hdr)) {
		wpa_printf(MSG_INFO, "EAP-TTLS: too short "
			   "Phase 2 request (len=%lu)", (unsigned long) len);
		return -1;
	}
	pos = (u8 *) (hdr + 1);
	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 EAP Request: type=%d", *pos);
	switch (*pos) {
	case EAP_TYPE_IDENTITY:
		*resp = eap_sm_buildIdentity(sm, req->identifier, resp_len, 1);
		break;
	default:
		if (data->phase2_eap_type == EAP_TYPE_NONE) {
			int i;
			for (i = 0; i < data->num_phase2_eap_types; i++) {
				if (data->phase2_eap_types[i] != *pos)
					continue;

				data->phase2_eap_type = *pos;
				wpa_printf(MSG_DEBUG, "EAP-TTLS: Selected "
					   "Phase 2 EAP method %d",
					   data->phase2_eap_type);
				break;
			}
		}
		if (*pos != data->phase2_eap_type || *pos == EAP_TYPE_NONE) {
			if (eap_ttls_phase2_nak(sm, data, hdr, resp, resp_len))
				return -1;
			break;
		}

		if (data->phase2_priv == NULL) {
			data->phase2_method = eap_sm_get_eap_methods(*pos);
			if (data->phase2_method) {
				sm->init_phase2 = 1;
				data->phase2_priv =
					data->phase2_method->init(sm);
				sm->init_phase2 = 0;
			}
		}
		if (data->phase2_priv == NULL || data->phase2_method == NULL) {
			wpa_printf(MSG_INFO, "EAP-TTLS: failed to initialize "
				   "Phase 2 EAP method %d", *pos);
			return -1;
		}
		memset(&iret, 0, sizeof(iret));
		*resp = data->phase2_method->process(sm, data->phase2_priv,
						     &iret, (u8 *) hdr, len,
						     resp_len);
		if ((iret.methodState == METHOD_DONE ||
		     iret.methodState == METHOD_MAY_CONT) &&
		    (iret.decision == DECISION_UNCOND_SUCC ||
		     iret.decision == DECISION_COND_SUCC ||
		     iret.decision == DECISION_FAIL)) {
			ret->methodState = iret.methodState;
			ret->decision = iret.decision;
		}
		break;
	}

	if (*resp == NULL &&
	    (config->pending_req_identity || config->pending_req_password ||
	     config->pending_req_otp)) {
		return 0;
	}

	if (*resp == NULL)
		return -1;

	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: AVP encapsulate EAP Response",
		    *resp, *resp_len);
	return eap_ttls_avp_encapsulate(resp, resp_len,
					RADIUS_ATTR_EAP_MESSAGE, 1);
}


static int eap_ttls_phase2_request_mschapv2(struct eap_sm *sm,
					    struct eap_ttls_data *data,
					    struct eap_method_ret *ret,
					    struct eap_hdr *req,
					    struct eap_hdr *hdr,
					    u8 **resp, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *buf, *pos, *challenge, *username, *peer_challenge;
	size_t username_len;
	int i;

	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 MSCHAPV2 Request");

	/* MSCHAPv2 does not include optional domain name in the
	 * challenge-response calculation, so remove domain prefix
	 * (if present). */
	username = config->identity;
	username_len = config->identity_len;
	pos = username;
	for (i = 0; i < username_len; i++) {
		if (username[i] == '\\') {
			username_len -= i + 1;
			username += i + 1;
			break;
		}
	}

	pos = buf = malloc(config->identity_len + 1000);
	if (buf == NULL) {
		wpa_printf(MSG_ERROR,
			   "EAP-TTLS/MSCHAPV2: Failed to allocate memory");
		return -1;
	}

	/* User-Name */
	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_USER_NAME, 0, 1,
			       config->identity, config->identity_len);

	/* MS-CHAP-Challenge */
	challenge = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				       EAP_TTLS_MSCHAPV2_CHALLENGE_LEN * 2 +
				       1);
	if (challenge == NULL) {
		free(buf);
		wpa_printf(MSG_ERROR, "EAP-TTLS/MSCHAPV2: Failed to derive "
			   "implicit challenge");
		return -1;
	}
	peer_challenge = challenge + 1 + EAP_TTLS_MSCHAPV2_CHALLENGE_LEN;

	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_MS_CHAP_CHALLENGE,
			       RADIUS_VENDOR_ID_MICROSOFT, 1,
			       challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);

	/* MS-CHAP2-Response */
	pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_MS_CHAP2_RESPONSE,
			       RADIUS_VENDOR_ID_MICROSOFT, 1,
			       EAP_TTLS_MSCHAPV2_RESPONSE_LEN);
	data->ident = challenge[EAP_TTLS_MSCHAPV2_CHALLENGE_LEN];
	*pos++ = data->ident;
	*pos++ = 0; /* Flags */
	memcpy(pos, peer_challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);
	pos += EAP_TTLS_MSCHAPV2_CHALLENGE_LEN;
	memset(pos, 0, 8); /* Reserved, must be zero */
	pos += 8;
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: MSCHAPV2: implicit auth_challenge",
		    challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: MSCHAPV2: peer_challenge",
		    peer_challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-TTLS: MSCHAPV2 username",
			  username, username_len);
	wpa_hexdump_ascii_key(MSG_DEBUG, "EAP-TTLS: MSCHAPV2 password",
			      config->password, config->password_len);
	generate_nt_response(challenge, peer_challenge,
			     username, username_len,
			     config->password, config->password_len,
			     pos);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: MSCHAPV2 response", pos, 24);
	generate_authenticator_response(config->password, config->password_len,
					peer_challenge, challenge,
					username, username_len,
					pos, data->auth_response);
	data->auth_response_valid = 1;

	pos += 24;
	free(challenge);
	AVP_PAD(buf, pos);

	*resp = buf;
	*resp_len = pos - buf;

	if (sm->workaround) {
		/* At least FreeRADIUS seems to be terminating
		 * EAP-TTLS/MSHCAPV2 without the expected MS-CHAP-v2 Success
		 * packet. */
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: EAP workaround - "
			   "allow success without tunneled response");
		ret->methodState = METHOD_MAY_CONT;
		ret->decision = DECISION_COND_SUCC;
	}

	return 0;
}


static int eap_ttls_phase2_request_mschap(struct eap_sm *sm,
					  struct eap_ttls_data *data,
					  struct eap_method_ret *ret,
					  struct eap_hdr *req,
					  struct eap_hdr *hdr,
					  u8 **resp, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *buf, *pos, *challenge;

	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 MSCHAP Request");

	pos = buf = malloc(config->identity_len + 1000);
	if (buf == NULL) {
		wpa_printf(MSG_ERROR,
			   "EAP-TTLS/MSCHAP: Failed to allocate memory");
		return -1;
	}

	/* User-Name */
	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_USER_NAME, 0, 1,
			       config->identity, config->identity_len);

	/* MS-CHAP-Challenge */
	challenge = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				       EAP_TLS_KEY_LEN);
	if (challenge == NULL) {
		free(buf);
		wpa_printf(MSG_ERROR, "EAP-TTLS/MSCHAP: Failed to derive "
			   "implicit challenge");
		return -1;
	}

	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_MS_CHAP_CHALLENGE,
			       RADIUS_VENDOR_ID_MICROSOFT, 1,
			       challenge, EAP_TTLS_MSCHAP_CHALLENGE_LEN);

	/* MS-CHAP-Response */
	pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_MS_CHAP_RESPONSE,
			       RADIUS_VENDOR_ID_MICROSOFT, 1,
			       EAP_TTLS_MSCHAP_RESPONSE_LEN);
	data->ident = challenge[EAP_TTLS_MSCHAP_CHALLENGE_LEN];
	*pos++ = data->ident;
	*pos++ = 1; /* Flags: Use NT style passwords */
	memset(pos, 0, 24); /* LM-Response */
	pos += 24;
	nt_challenge_response(challenge,
			      config->password, config->password_len,
			      pos); /* NT-Response */
	wpa_hexdump_ascii_key(MSG_DEBUG, "EAP-TTLS: MSCHAP password",
			      config->password, config->password_len);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: MSCHAP implicit challenge",
		    challenge, EAP_TTLS_MSCHAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: MSCHAP response", pos, 24);
	pos += 24;
	free(challenge);
	AVP_PAD(buf, pos);

	*resp = buf;
	*resp_len = pos - buf;

	/* EAP-TTLS/MSCHAP does not provide tunneled success notification, so
	 * assume that Phase2 succeeds. */
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_COND_SUCC;

	return 0;
}


static int eap_ttls_phase2_request_pap(struct eap_sm *sm,
				       struct eap_ttls_data *data,
				       struct eap_method_ret *ret,
				       struct eap_hdr *req,
				       struct eap_hdr *hdr,
				       u8 **resp, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *buf, *pos;
	size_t pad;

	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 PAP Request");

	pos = buf = malloc(config->identity_len + config->password_len + 100);
	if (buf == NULL) {
		wpa_printf(MSG_ERROR,
			   "EAP-TTLS/PAP: Failed to allocate memory");
		return -1;
	}

	/* User-Name */
	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_USER_NAME, 0, 1,
			       config->identity, config->identity_len);

	/* User-Password; in RADIUS, this is encrypted, but EAP-TTLS encrypts
	 * the data, so no separate encryption is used in the AVP itself.
	 * However, the password is padded to obfuscate its length. */
	pad = (16 - (config->password_len & 15)) & 15;
	pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_USER_PASSWORD, 0, 1,
			       config->password_len + pad);
	memcpy(pos, config->password, config->password_len);
	pos += config->password_len;
	memset(pos, 0, pad);
	pos += pad;
	AVP_PAD(buf, pos);

	*resp = buf;
	*resp_len = pos - buf;

	/* EAP-TTLS/PAP does not provide tunneled success notification, so
	 * assume that Phase2 succeeds. */
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_COND_SUCC;

	return 0;
}


static int eap_ttls_phase2_request_chap(struct eap_sm *sm,
					struct eap_ttls_data *data,
					struct eap_method_ret *ret,
					struct eap_hdr *req,
					struct eap_hdr *hdr,
					u8 **resp, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	u8 *buf, *pos, *challenge;
	MD5_CTX context;

	wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase 2 CHAP Request");

	pos = buf = malloc(config->identity_len + 1000);
	if (buf == NULL) {
		wpa_printf(MSG_ERROR,
			   "EAP-TTLS/CHAP: Failed to allocate memory");
		return -1;
	}

	/* User-Name */
	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_USER_NAME, 0, 1,
			       config->identity, config->identity_len);

	/* CHAP-Challenge */
	challenge = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				       EAP_TLS_KEY_LEN);
	if (challenge == NULL) {
		free(buf);
		wpa_printf(MSG_ERROR, "EAP-TTLS/CHAP: Failed to derive "
			   "implicit challenge");
		return -1;
	}

	pos = eap_ttls_avp_add(buf, pos, RADIUS_ATTR_CHAP_CHALLENGE, 0, 1,
			       challenge, EAP_TTLS_CHAP_CHALLENGE_LEN);

	/* CHAP-Password */
	pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_CHAP_PASSWORD, 0, 1,
			       1 + EAP_TTLS_CHAP_PASSWORD_LEN);
	data->ident = challenge[EAP_TTLS_CHAP_CHALLENGE_LEN];
	*pos++ = data->ident;

	/* MD5(Ident + Password + Challenge) */
	MD5Init(&context);
	MD5Update(&context, &data->ident, 1);
	MD5Update(&context, config->password, config->password_len);
	MD5Update(&context, challenge, EAP_TTLS_CHAP_CHALLENGE_LEN);
	MD5Final(pos, &context);

	wpa_hexdump_ascii(MSG_DEBUG, "EAP-TTLS: CHAP username",
			  config->identity, config->identity_len);
	wpa_hexdump_ascii_key(MSG_DEBUG, "EAP-TTLS: CHAP password",
			      config->password, config->password_len);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: CHAP implicit challenge",
		    challenge, EAP_TTLS_CHAP_CHALLENGE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: CHAP password",
		    pos, EAP_TTLS_CHAP_PASSWORD_LEN);
	pos += EAP_TTLS_CHAP_PASSWORD_LEN;
	free(challenge);
	AVP_PAD(buf, pos);

	*resp = buf;
	*resp_len = pos - buf;

	/* EAP-TTLS/CHAP does not provide tunneled success notification, so
	 * assume that Phase2 succeeds. */
	ret->methodState = METHOD_DONE;
	ret->decision = DECISION_COND_SUCC;

	return 0;
}


static int eap_ttls_phase2_request(struct eap_sm *sm,
				   struct eap_ttls_data *data,
				   struct eap_method_ret *ret,
				   struct eap_hdr *req,
				   struct eap_hdr *hdr,
				   u8 **resp, size_t *resp_len)
{
	struct wpa_ssid *config = eap_get_config(sm);
	int res = 0;

	if (data->phase2_type == EAP_TTLS_PHASE2_MSCHAPV2 ||
	    data->phase2_type == EAP_TTLS_PHASE2_MSCHAP ||
	    data->phase2_type == EAP_TTLS_PHASE2_PAP ||
	    data->phase2_type == EAP_TTLS_PHASE2_CHAP) {
		if (config == NULL || config->identity == NULL) {
			wpa_printf(MSG_INFO,
				   "EAP-TTLS: Identity not configured");
			eap_sm_request_identity(sm, config);
			if (config->password == NULL)
				eap_sm_request_password(sm, config);
			return 0;
		}

		if (config->password == NULL) {
			wpa_printf(MSG_INFO,
				   "EAP-TTLS: Password not configured");
			eap_sm_request_password(sm, config);
			return 0;
		}
	}

	switch (data->phase2_type) {
	case EAP_TTLS_PHASE2_EAP:
		res = eap_ttls_phase2_request_eap(sm, data, ret, req, hdr,
						  resp, resp_len);
		break;
	case EAP_TTLS_PHASE2_MSCHAPV2:
		res = eap_ttls_phase2_request_mschapv2(sm, data, ret, req, hdr,
						       resp, resp_len);
		break;
	case EAP_TTLS_PHASE2_MSCHAP:
		res = eap_ttls_phase2_request_mschap(sm, data, ret, req, hdr,
						     resp, resp_len);
		break;
	case EAP_TTLS_PHASE2_PAP:
		res = eap_ttls_phase2_request_pap(sm, data, ret, req, hdr,
						  resp, resp_len);
		break;
	case EAP_TTLS_PHASE2_CHAP:
		res = eap_ttls_phase2_request_chap(sm, data, ret, req, hdr,
						   resp, resp_len);
		break;
	default:
		wpa_printf(MSG_ERROR, "EAP-TTLS: Phase 2 - Unknown");
		res = -1;
		break;
	}

	if (res < 0) {
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
	}

	return res;
}


static int eap_ttls_decrypt(struct eap_sm *sm, struct eap_ttls_data *data,
			    struct eap_method_ret *ret, struct eap_hdr *req,
			    u8 *in_data, size_t in_len,
			    u8 **out_data, size_t *out_len)
{
	u8 *in_decrypted = NULL, *pos;
	int buf_len, len_decrypted = 0, len, left, retval = 0, res;
	struct eap_hdr *hdr = NULL;
	u8 *resp = NULL, *mschapv2 = NULL, *eapdata = NULL;
	size_t resp_len, eap_len = 0;
	struct ttls_avp *avp;
	u8 recv_response[20];
	int mschapv2_error = 0;
	struct wpa_ssid *config = eap_get_config(sm);

	wpa_printf(MSG_DEBUG, "EAP-TTLS: received %lu bytes encrypted data for"
		   " Phase 2", (unsigned long) in_len);

	if (data->pending_phase2_req) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Pending Phase 2 request - "
			   "skip decryption and use old data");
		/* Clear TLS reassembly state. */
		free(data->ssl.tls_in);
		data->ssl.tls_in = NULL;
		data->ssl.tls_in_len = 0;
		data->ssl.tls_in_left = 0;
		data->ssl.tls_in_total = 0;

		in_decrypted = data->pending_phase2_req;
		data->pending_phase2_req = NULL;
		len_decrypted = data->pending_phase2_req_len;
		if (data->pending_phase2_req_len == 0) {
			free(in_decrypted);
			in_decrypted = NULL;
			goto fake_req_identity;
		}
		goto continue_req;
	}

	if (in_len == 0 && data->phase2_start) {
		data->phase2_start = 0;
		/* EAP-TTLS does not use Phase2 on fast re-auth; this must be
		 * done only if TLS part was indeed resuming a previous
		 * session. Most Authentication Servers terminate EAP-TTLS
		 * before reaching this point, but some do not. Make
		 * wpa_supplicant stop phase 2 here, if needed. */
		if (data->reauth &&
		    tls_connection_resumed(sm->ssl_ctx, data->ssl.conn)) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Session resumption - "
				   "skip phase 2");
			*out_data = eap_tls_build_ack(&data->ssl, out_len,
						      req->identifier,
						      EAP_TYPE_TTLS, 0);
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_UNCOND_SUCC;
			data->phase2_success = 1;
			return 0;
		}
	fake_req_identity:
		wpa_printf(MSG_DEBUG, "EAP-TTLS: empty data in beginning of "
			   "Phase 2 - use fake EAP-Request Identity");
		buf_len = sizeof(*hdr) + 1;
		in_decrypted = malloc(buf_len);
		if (in_decrypted == NULL) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: failed to allocate "
				   "memory for fake EAP-Identity Request");
			retval = -1;
			goto done;
		}
		hdr = (struct eap_hdr *) in_decrypted;
		hdr->code = EAP_CODE_REQUEST;
		hdr->identifier = 0;
		hdr->length = host_to_be16(sizeof(*hdr) + 1);
		in_decrypted[sizeof(*hdr)] = EAP_TYPE_IDENTITY;
		goto process_eap;
	}

	res = eap_tls_data_reassemble(sm, &data->ssl, &in_data, &in_len);
	if (res < 0 || res == 1) {
		retval = res;
		goto done;
	}

	buf_len = in_len;
	if (data->ssl.tls_in_total > buf_len)
		buf_len = data->ssl.tls_in_total;
	in_decrypted = malloc(buf_len);
	if (in_decrypted == NULL) {
		free(data->ssl.tls_in);
		data->ssl.tls_in = NULL;
		data->ssl.tls_in_len = 0;
		wpa_printf(MSG_WARNING, "EAP-TTLS: failed to allocate memory "
			   "for decryption");
		retval = -1;
		goto done;
	}

	len_decrypted = tls_connection_decrypt(sm->ssl_ctx, data->ssl.conn,
					       in_data, in_len,
					       in_decrypted, buf_len);
	free(data->ssl.tls_in);
	data->ssl.tls_in = NULL;
	data->ssl.tls_in_len = 0;
	if (len_decrypted < 0) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Failed to decrypt Phase 2 "
			   "data");
		retval = -1;
		goto done;
	}

continue_req:
	data->phase2_start = 0;

	wpa_hexdump(MSG_DEBUG, "EAP-TTLS: Decrypted Phase 2 AVPs",
		    in_decrypted, len_decrypted);
	if (len_decrypted < sizeof(struct ttls_avp)) {
		wpa_printf(MSG_WARNING, "EAP-TTLS: Too short Phase 2 AVP frame"
			   " len=%d expected %lu or more - dropped",
			   len_decrypted,
			   (unsigned long) sizeof(struct ttls_avp));
		retval = -1;
		goto done;
	}

	/* Parse AVPs */
	pos = in_decrypted;
	left = len_decrypted;
	mschapv2 = NULL;

	while (left > 0) {
		u32 avp_code, avp_length, vendor_id = 0;
		u8 avp_flags, *dpos;
		size_t pad, dlen;
		avp = (struct ttls_avp *) pos;
		avp_code = be_to_host32(avp->avp_code);
		avp_length = be_to_host32(avp->avp_length);
		avp_flags = (avp_length >> 24) & 0xff;
		avp_length &= 0xffffff;
		wpa_printf(MSG_DEBUG, "EAP-TTLS: AVP: code=%d flags=0x%02x "
			   "length=%d", (int) avp_code, avp_flags,
			   (int) avp_length);
		if (avp_length > left) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: AVP overflow "
				   "(len=%d, left=%d) - dropped",
				   (int) avp_length, left);
			retval = -1;
			goto done;
		}
		dpos = (u8 *) (avp + 1);
		dlen = avp_length - sizeof(*avp);
		if (avp_flags & AVP_FLAGS_VENDOR) {
			if (dlen < 4) {
				wpa_printf(MSG_WARNING, "EAP-TTLS: vendor AVP "
					   "underflow");
				retval = -1;
				goto done;
			}
			vendor_id = be_to_host32(* (u32 *) dpos);
			wpa_printf(MSG_DEBUG, "EAP-TTLS: AVP vendor_id %d",
				   (int) vendor_id);
			dpos += 4;
			dlen -= 4;
		}

		wpa_hexdump(MSG_DEBUG, "EAP-TTLS: AVP data", dpos, dlen);

		if (vendor_id == 0 && avp_code == RADIUS_ATTR_EAP_MESSAGE) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: AVP - EAP Message");
			if (eapdata == NULL) {
				eapdata = malloc(dlen);
				if (eapdata == NULL) {
					retval = -1;
					wpa_printf(MSG_WARNING, "EAP-TTLS: "
						   "failed to allocate memory "
						   "for Phase 2 EAP data");
					goto done;
				}
				memcpy(eapdata, dpos, dlen);
				eap_len = dlen;
			} else {
				u8 *neweap = realloc(eapdata, eap_len + dlen);
				if (neweap == NULL) {
					retval = -1;
					wpa_printf(MSG_WARNING, "EAP-TTLS: "
						   "failed to allocate memory "
						   "for Phase 2 EAP data");
					goto done;
				}
				memcpy(neweap + eap_len, dpos, dlen);
				eapdata = neweap;
				eap_len += dlen;
			}
		} else if (vendor_id == 0 &&
			   avp_code == RADIUS_ATTR_REPLY_MESSAGE) {
			/* This is an optional message that can be displayed to
			 * the user. */
			wpa_hexdump_ascii(MSG_DEBUG,
					  "EAP-TTLS: AVP - Reply-Message",
					  dpos, dlen);
		} else if (vendor_id == RADIUS_VENDOR_ID_MICROSOFT &&
			   avp_code == RADIUS_ATTR_MS_CHAP2_SUCCESS) {
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-TTLS: "
					  "MS-CHAP2-Success", dpos, dlen);
			if (dlen != 43) {
				wpa_printf(MSG_WARNING, "EAP-TTLS: Unexpected "
					   "MS-CHAP2-Success length "
					   "(len=%lu, expected 43)",
					   (unsigned long) dlen);
				retval = -1;
				break;
			}
			mschapv2 = dpos;
		} else if (vendor_id == RADIUS_VENDOR_ID_MICROSOFT &&
			   avp_code == RADIUS_ATTR_MS_CHAP_ERROR) {
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-TTLS: "
					  "MS-CHAP-Error", dpos, dlen);
			mschapv2_error = 1;
		} else if (avp_flags & AVP_FLAGS_MANDATORY) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: Unsupported "
				   "mandatory AVP code %d vendor_id %d - "
				   "dropped", (int) avp_code, (int) vendor_id);
			retval = -1;
			goto done;
		} else {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Ignoring unsupported "
				   "AVP code %d vendor_id %d",
				   (int) avp_code, (int) vendor_id);
		}

		pad = (4 - (avp_length & 3)) & 3;
		pos += avp_length + pad;
		left -= avp_length + pad;
	}

	switch (data->phase2_type) {
	case EAP_TTLS_PHASE2_EAP:
		if (eapdata == NULL) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: No EAP Message in "
				   "the packet - dropped");
			retval = -1;
			goto done;
		}

		wpa_hexdump(MSG_DEBUG, "EAP-TTLS: Phase 2 EAP",
			    eapdata, eap_len);
		hdr = (struct eap_hdr *) eapdata;

		if (eap_len < sizeof(*hdr)) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: Too short Phase 2 "
				   "EAP frame (len=%lu, expected %lu or more) "
				   "- dropped", (unsigned long) eap_len,
				   (unsigned long) sizeof(*hdr));
			retval = -1;
			goto done;
		}
		len = be_to_host16(hdr->length);
		if (len > eap_len) {
			wpa_printf(MSG_INFO, "EAP-TTLS: Length mismatch in "
				   "Phase 2 EAP frame (EAP hdr len=%d, EAP "
				   "data len in AVP=%lu)", len,
				   (unsigned long) eap_len);
			retval = -1;
			goto done;
		}
		wpa_printf(MSG_DEBUG, "EAP-TTLS: received Phase 2: code=%d "
			   "identifier=%d length=%d",
			   hdr->code, hdr->identifier, len);
	process_eap:
		switch (hdr->code) {
		case EAP_CODE_REQUEST:
			if (eap_ttls_phase2_request(sm, data, ret, req, hdr,
						    &resp, &resp_len)) {
				wpa_printf(MSG_INFO, "EAP-TTLS: Phase2 "
					   "Request processing failed");
				retval = -1;
				goto done;
			}
			break;
		default:
			wpa_printf(MSG_INFO, "EAP-TTLS: Unexpected code=%d in "
				   "Phase 2 EAP header", hdr->code);
			retval = -1;
			break;
		}
		break;
	case EAP_TTLS_PHASE2_MSCHAPV2:
		if (mschapv2_error) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Received "
				   "MS-CHAP-Error - failed");
			ret->methodState = METHOD_DONE;
			ret->decision = DECISION_FAIL;
			*out_data = eap_tls_build_ack(&data->ssl, out_len,
						      req->identifier,
						      EAP_TYPE_TTLS, 0);
			break;
		}

		if (mschapv2 == NULL) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: no MS-CHAP2-Success"
				   " AVP received for Phase2 MSCHAPV2");
			retval = -1;
			break;
		}
		if (mschapv2[0] != data->ident) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: Ident mismatch "
				   "for Phase 2 MSCHAPV2 (received Ident "
				   "0x%02x, expected 0x%02x)",
				   mschapv2[0], data->ident);
			retval = -1;
			break;
		}
		if (!data->auth_response_valid ||
		    mschapv2[1] != 'S' || mschapv2[2] != '=' ||
		    hexstr2bin((char *) (mschapv2 + 3), recv_response, 20) ||
		    memcmp(data->auth_response, recv_response, 20) != 0) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: Invalid "
				   "authenticator response in Phase 2 "
				   "MSCHAPV2 success request");
			retval = -1;
			break;
		}

		wpa_printf(MSG_INFO, "EAP-TTLS: Phase 2 MSCHAPV2 "
			   "authentication succeeded");
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_UNCOND_SUCC;
		data->phase2_success = 1;

		/* Reply with empty data; authentication server will reply
		 * with EAP-Success after this. */
		retval = 1;
		goto done;
	case EAP_TTLS_PHASE2_MSCHAP:
	case EAP_TTLS_PHASE2_PAP:
	case EAP_TTLS_PHASE2_CHAP:
		/* EAP-TTLS/{MSCHAP,PAP,CHAP} should not send any TLS tunneled
		 * requests to the supplicant */
		wpa_printf(MSG_INFO, "EAP-TTLS: Phase 2 received unexpected "
			   "tunneled data");
		retval = -1;
		break;
	}

	if (resp) {
		wpa_hexdump_key(MSG_DEBUG, "EAP-TTLS: Encrypting Phase 2 data",
				resp, resp_len);

		if (eap_ttls_encrypt(sm, data, req->identifier,
				     resp, resp_len, out_data, out_len)) {
			wpa_printf(MSG_INFO, "EAP-TTLS: Failed to encrypt "
				   "a Phase 2 frame");
		}
		free(resp);
	} else if (config->pending_req_identity ||
		   config->pending_req_password ||
		   config->pending_req_otp) {
		free(data->pending_phase2_req);
		data->pending_phase2_req = malloc(len_decrypted);
		if (data->pending_phase2_req) {
			memcpy(data->pending_phase2_req, in_decrypted,
			       len_decrypted);
			data->pending_phase2_req_len = len_decrypted;
		}
	}

done:
	free(in_decrypted);
	free(eapdata);

	if (retval < 0) {
		ret->methodState = METHOD_DONE;
		ret->decision = DECISION_FAIL;
	}

	return retval;
}


static u8 * eap_ttls_process(struct eap_sm *sm, void *priv,
			     struct eap_method_ret *ret,
			     u8 *reqData, size_t reqDataLen,
			     size_t *respDataLen)
{
	struct eap_hdr *req;
	int left, res;
	unsigned int tls_msg_len;
	u8 flags, *pos, *resp, id;
	struct eap_ttls_data *data = priv;

	if (tls_get_errors(sm->ssl_ctx)) {
		wpa_printf(MSG_INFO, "EAP-TTLS: TLS errors detected");
		ret->ignore = TRUE;
		return NULL;
	}

	req = (struct eap_hdr *) reqData;
	pos = (u8 *) (req + 1);
	if (reqDataLen < sizeof(*req) + 2 || *pos != EAP_TYPE_TTLS ||
	    (left = be_to_host16(req->length)) > reqDataLen) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Invalid frame");
		ret->ignore = TRUE;
		return NULL;
	}
	left -= sizeof(struct eap_hdr);
	id = req->identifier;
	pos++;
	flags = *pos++;
	left -= 2;
	wpa_printf(MSG_DEBUG, "EAP-TTLS: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) reqDataLen, flags);
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-TTLS: Short frame with TLS "
				   "length");
			ret->ignore = TRUE;
			return NULL;
		}
		tls_msg_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) |
			pos[3];
		wpa_printf(MSG_DEBUG, "EAP-TTLS: TLS Message Length: %d",
			   tls_msg_len);
		if (data->ssl.tls_in_left == 0) {
			data->ssl.tls_in_total = tls_msg_len;
			data->ssl.tls_in_left = tls_msg_len;
			free(data->ssl.tls_in);
			data->ssl.tls_in = NULL;
			data->ssl.tls_in_len = 0;
		}
		pos += 4;
		left -= 4;
	}

	ret->ignore = FALSE;
	ret->methodState = METHOD_CONT;
	ret->decision = DECISION_FAIL;
	ret->allowNotifications = TRUE;

	if (flags & EAP_TLS_FLAGS_START) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Start");
		/* draft-ietf-pppext-eap-ttls-03.txt, Ch. 8.1:
		 * EAP-TTLS Start packet may, in a future specification, be
		 * allowed to contain data. Client based on this draft version
		 * must ignore such data but must not reject the Start packet.
		 */
		left = 0;
	}

	resp = NULL;
	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
	    !data->resuming) {
		res = eap_ttls_decrypt(sm, data, ret, req, pos, left,
				       &resp, respDataLen);
	} else {
		res = eap_tls_process_helper(sm, &data->ssl, EAP_TYPE_TTLS, 0,
					     id, pos, left,
					     &resp, respDataLen);

		if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
			wpa_printf(MSG_DEBUG,
				   "EAP-TTLS: TLS done, proceed to Phase 2");
			if (data->resuming) {
				wpa_printf(MSG_DEBUG, "EAP-TTLS: fast reauth -"
					   " may skip Phase 2");
				ret->decision = DECISION_COND_SUCC;
				ret->methodState = METHOD_MAY_CONT;
			}
			data->phase2_start = 1;
			free(data->key_data);
			data->key_data =
				eap_tls_derive_key(sm, &data->ssl,
						   "ttls keying material",
						   EAP_TLS_KEY_LEN);
			if (data->key_data) {
				wpa_hexdump_key(MSG_DEBUG,
						"EAP-TTLS: Derived key",
						data->key_data,
						EAP_TLS_KEY_LEN);
			} else {
				wpa_printf(MSG_DEBUG, "EAP-TTLS: Failed to "
					   "derive key");
			}

			if (*respDataLen == 0) {
				if (eap_ttls_decrypt(sm, data, ret, req, NULL,
						     0, &resp, respDataLen)) {
					wpa_printf(MSG_WARNING, "EAP-TTLS: "
						   "failed to process early "
						   "start for Phase 2");
				}
				res = 0;
			}
			data->resuming = 0;
		}
	}

	if (ret->methodState == METHOD_DONE) {
		ret->allowNotifications = FALSE;
		if (ret->decision == DECISION_UNCOND_SUCC ||
		    ret->decision == DECISION_COND_SUCC) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Authentication "
				   "completed successfully");
			data->phase2_success = 1;
		}
	} else if (sm->workaround && ret->methodState == METHOD_MAY_CONT &&
		   (ret->decision == DECISION_UNCOND_SUCC ||
		    ret->decision == DECISION_COND_SUCC)) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Authentication "
				   "completed successfully (EAP workaround)");
			data->phase2_success = 1;
	}

	if (res == 1) {
		return eap_tls_build_ack(&data->ssl, respDataLen, id,
					 EAP_TYPE_TTLS, 0);
	}
	return resp;
}


static Boolean eap_ttls_has_reauth_data(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	return tls_connection_established(sm->ssl_ctx, data->ssl.conn) &&
		data->phase2_success;
}


static void eap_ttls_deinit_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	free(data->pending_phase2_req);
	data->pending_phase2_req = NULL;
}


static void * eap_ttls_init_for_reauth(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	free(data->key_data);
	data->key_data = NULL;
	if (eap_tls_reauth_init(sm, &data->ssl)) {
		free(data);
		return NULL;
	}
	data->phase2_start = 0;
	data->phase2_success = 0;
	data->resuming = 1;
	data->reauth = 1;
	return priv;
}


static int eap_ttls_get_status(struct eap_sm *sm, void *priv, char *buf,
			       size_t buflen, int verbose)
{
	struct eap_ttls_data *data = priv;
	int len;

	len = eap_tls_status(sm, &data->ssl, buf, buflen, verbose);
	switch (data->phase2_type) {
	case EAP_TTLS_PHASE2_EAP:
		len += snprintf(buf + len, buflen - len,
				"EAP-TTLS Phase2 method=EAP-%s\n",
				data->phase2_method ? data->phase2_method->name
				: "?");
		break;
	case EAP_TTLS_PHASE2_MSCHAPV2:
		len += snprintf(buf + len, buflen - len,
				"EAP-TTLS Phase2 method=MSCHAPV2\n");
		break;
	case EAP_TTLS_PHASE2_MSCHAP:
		len += snprintf(buf + len, buflen - len,
				"EAP-TTLS Phase2 method=MSCHAP\n");
		break;
	case EAP_TTLS_PHASE2_PAP:
		len += snprintf(buf + len, buflen - len,
				"EAP-TTLS Phase2 method=PAP\n");
		break;
	case EAP_TTLS_PHASE2_CHAP:
		len += snprintf(buf + len, buflen - len,
				"EAP-TTLS Phase2 method=CHAP\n");
		break;
	}

	return len;
}


static Boolean eap_ttls_isKeyAvailable(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	return data->key_data != NULL && data->phase2_success;
}


static u8 * eap_ttls_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_ttls_data *data = priv;
	u8 *key;

	if (data->key_data == NULL || !data->phase2_success)
		return NULL;

	key = malloc(EAP_TLS_KEY_LEN);
	if (key == NULL)
		return NULL;

	*len = EAP_TLS_KEY_LEN;
	memcpy(key, data->key_data, EAP_TLS_KEY_LEN);

	return key;
}


const struct eap_method eap_method_ttls =
{
	.method = EAP_TYPE_TTLS,
	.name = "TTLS",
	.init = eap_ttls_init,
	.deinit = eap_ttls_deinit,
	.process = eap_ttls_process,
	.isKeyAvailable = eap_ttls_isKeyAvailable,
	.getKey = eap_ttls_getKey,
	.get_status = eap_ttls_get_status,
	.has_reauth_data = eap_ttls_has_reauth_data,
	.deinit_for_reauth = eap_ttls_deinit_for_reauth,
	.init_for_reauth = eap_ttls_init_for_reauth,
};
