/*
 * hostapd / EAP-TTLS (draft-ietf-pppext-eap-ttls-05.txt)
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
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "ms_funcs.h"
#include "md5.h"
#include "tls.h"
#include "eap_ttls.h"

#define EAP_TTLS_VERSION 0


static void eap_ttls_reset(struct eap_sm *sm, void *priv);


struct eap_ttls_data {
	struct eap_ssl_data ssl;
	enum {
		START, PHASE1, PHASE2_START, PHASE2_METHOD,
		PHASE2_MSCHAPV2_RESP, SUCCESS, FAILURE
	} state;

	int ttls_version;
	const struct eap_method *phase2_method;
	void *phase2_priv;
	int mschapv2_resp_ok;
	u8 mschapv2_auth_response[20];
	u8 mschapv2_ident;
};


static const char * eap_ttls_state_txt(int state)
{
	switch (state) {
	case START:
		return "START";
	case PHASE1:
		return "PHASE1";
	case PHASE2_START:
		return "PHASE2_START";
	case PHASE2_METHOD:
		return "PHASE2_METHOD";
	case PHASE2_MSCHAPV2_RESP:
		return "PHASE2_MSCHAPV2_RESP";
	case SUCCESS:
		return "SUCCESS";
	case FAILURE:
		return "FAILURE";
	default:
		return "Unknown?!";
	}
}


static void eap_ttls_state(struct eap_ttls_data *data, int state)
{
	wpa_printf(MSG_DEBUG, "EAP-TTLS: %s -> %s",
		   eap_ttls_state_txt(data->state),
		   eap_ttls_state_txt(state));
	data->state = state;
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


static int eap_ttls_avp_encapsulate(u8 **resp, size_t *resp_len, u32 avp_code,
				    int mandatory)
{
	u8 *avp, *pos;

	avp = malloc(sizeof(struct ttls_avp) + *resp_len + 4);
	if (avp == NULL) {
		free(*resp);
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


struct eap_ttls_avp {
	 /* Note: eap is allocated memory; caller is responsible for freeing
	  * it. All the other pointers are pointing to the packet data, i.e.,
	  * they must not be freed separately. */
	u8 *eap;
	size_t eap_len;
	u8 *user_name;
	size_t user_name_len;
	u8 *user_password;
	size_t user_password_len;
	u8 *chap_challenge;
	size_t chap_challenge_len;
	u8 *chap_password;
	size_t chap_password_len;
	u8 *mschap_challenge;
	size_t mschap_challenge_len;
	u8 *mschap_response;
	size_t mschap_response_len;
	u8 *mschap2_response;
	size_t mschap2_response_len;
};


static int eap_ttls_avp_parse(u8 *buf, size_t len, struct eap_ttls_avp *parse)
{
	struct ttls_avp *avp;
	u8 *pos;
	int left;

	pos = buf;
	left = len;
	memset(parse, 0, sizeof(*parse));

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
			return -1;
		}
		dpos = (u8 *) (avp + 1);
		dlen = avp_length - sizeof(*avp);
		if (avp_flags & AVP_FLAGS_VENDOR) {
			if (dlen < 4) {
				wpa_printf(MSG_WARNING, "EAP-TTLS: vendor AVP "
					   "underflow");
				return -1;
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
			if (parse->eap == NULL) {
				parse->eap = malloc(dlen);
				if (parse->eap == NULL) {
					wpa_printf(MSG_WARNING, "EAP-TTLS: "
						   "failed to allocate memory "
						   "for Phase 2 EAP data");
					return -1;
				}
				memcpy(parse->eap, dpos, dlen);
				parse->eap_len = dlen;
			} else {
				u8 *neweap = realloc(parse->eap,
						     parse->eap_len + dlen);
				if (neweap == NULL) {
					wpa_printf(MSG_WARNING, "EAP-TTLS: "
						   "failed to allocate memory "
						   "for Phase 2 EAP data");
					free(parse->eap);
					parse->eap = NULL;
					return -1;
				}
				memcpy(neweap + parse->eap_len, dpos, dlen);
				parse->eap = neweap;
				parse->eap_len += dlen;
			}
		} else if (vendor_id == 0 &&
			   avp_code == RADIUS_ATTR_USER_NAME) {
			wpa_hexdump_ascii(MSG_DEBUG, "EAP-TTLS: User-Name",
					  dpos, dlen);
			parse->user_name = dpos;
			parse->user_name_len = dlen;
		} else if (vendor_id == 0 &&
			   avp_code == RADIUS_ATTR_USER_PASSWORD) {
			u8 *password = dpos;
			size_t password_len = dlen;
			while (password_len > 0 &&
			       password[password_len - 1] == '\0') {
				password_len--;
			}
			wpa_hexdump_ascii_key(MSG_DEBUG, "EAP-TTLS: "
					      "User-Password (PAP)",
					      password, password_len);
			parse->user_password = password;
			parse->user_password_len = password_len;
		} else if (vendor_id == 0 &&
			   avp_code == RADIUS_ATTR_CHAP_CHALLENGE) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TTLS: CHAP-Challenge (CHAP)",
				    dpos, dlen);
			parse->chap_challenge = dpos;
			parse->chap_challenge_len = dlen;
		} else if (vendor_id == 0 &&
			   avp_code == RADIUS_ATTR_CHAP_PASSWORD) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TTLS: CHAP-Password (CHAP)",
				    dpos, dlen);
			parse->chap_password = dpos;
			parse->chap_password_len = dlen;
		} else if (vendor_id == RADIUS_VENDOR_ID_MICROSOFT &&
			   avp_code == RADIUS_ATTR_MS_CHAP_CHALLENGE) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TTLS: MS-CHAP-Challenge",
				    dpos, dlen);
			parse->mschap_challenge = dpos;
			parse->mschap_challenge_len = dlen;
		} else if (vendor_id == RADIUS_VENDOR_ID_MICROSOFT &&
			   avp_code == RADIUS_ATTR_MS_CHAP_RESPONSE) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TTLS: MS-CHAP-Response (MSCHAP)",
				    dpos, dlen);
			parse->mschap_response = dpos;
			parse->mschap_response_len = dlen;
		} else if (vendor_id == RADIUS_VENDOR_ID_MICROSOFT &&
			   avp_code == RADIUS_ATTR_MS_CHAP2_RESPONSE) {
			wpa_hexdump(MSG_DEBUG,
				    "EAP-TTLS: MS-CHAP2-Response (MSCHAPV2)",
				    dpos, dlen);
			parse->mschap2_response = dpos;
			parse->mschap2_response_len = dlen;
		} else if (avp_flags & AVP_FLAGS_MANDATORY) {
			wpa_printf(MSG_WARNING, "EAP-TTLS: Unsupported "
				   "mandatory AVP code %d vendor_id %d - "
				   "dropped", (int) avp_code, (int) vendor_id);
			return -1;
		} else {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Ignoring unsupported "
				   "AVP code %d vendor_id %d",
				   (int) avp_code, (int) vendor_id);
		}

		pad = (4 - (avp_length & 3)) & 3;
		pos += avp_length + pad;
		left -= avp_length + pad;
	}

	return 0;
}


static void * eap_ttls_init(struct eap_sm *sm)
{
	struct eap_ttls_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL)
		return data;
	memset(data, 0, sizeof(*data));
	data->ttls_version = EAP_TTLS_VERSION;
	data->state = START;

	if (eap_tls_ssl_init(sm, &data->ssl, 0)) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Failed to initialize SSL.");
		eap_ttls_reset(sm, data);
		return NULL;
	}

	return data;
}


static void eap_ttls_reset(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	if (data == NULL)
		return;
	if (data->phase2_priv && data->phase2_method)
		data->phase2_method->reset(sm, data->phase2_priv);
	eap_tls_ssl_deinit(sm, &data->ssl);
	free(data);
}


static u8 * eap_ttls_build_start(struct eap_sm *sm, struct eap_ttls_data *data,
				 int id, size_t *reqDataLen)
{
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(*req) + 2;
	req = malloc(*reqDataLen);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-TTLS: Failed to allocate memory for"
			   " request");
		eap_ttls_state(data, FAILURE);
		return NULL;
	}

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_TTLS;
	*pos = EAP_TLS_FLAGS_START | data->ttls_version;

	eap_ttls_state(data, PHASE1);

	return (u8 *) req;
}


static u8 * eap_ttls_build_req(struct eap_sm *sm, struct eap_ttls_data *data,
			       int id, size_t *reqDataLen)
{
	int res;
	u8 *req;

	res = eap_tls_buildReq_helper(sm, &data->ssl, EAP_TYPE_TTLS,
				      data->ttls_version, id, &req,
				      reqDataLen);

	if (tls_connection_established(sm->ssl_ctx, data->ssl.conn)) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase1 done, starting "
			   "Phase2");
		eap_ttls_state(data, PHASE2_START);
	}

	if (res == 1)
		return eap_tls_build_ack(reqDataLen, id, EAP_TYPE_TTLS,
					 data->ttls_version);
	return req;
}


static u8 * eap_ttls_encrypt(struct eap_sm *sm, struct eap_ttls_data *data,
			     int id, u8 *plain, size_t plain_len,
			     size_t *out_len)
{
	int res;
	u8 *pos;
	struct eap_hdr *req;

	/* TODO: add support for fragmentation, if needed. This will need to
	 * add TLS Message Length field, if the frame is fragmented. */
	req = malloc(sizeof(struct eap_hdr) + 2 + data->ssl.tls_out_limit);
	if (req == NULL)
		return NULL;

	req->code = EAP_CODE_REQUEST;
	req->identifier = id;

	pos = (u8 *) (req + 1);
	*pos++ = EAP_TYPE_TTLS;
	*pos++ = data->ttls_version;

	res = tls_connection_encrypt(sm->ssl_ctx, data->ssl.conn,
				     plain, plain_len,
				     pos, data->ssl.tls_out_limit);
	if (res < 0) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Failed to encrypt Phase 2 "
			   "data");
		free(req);
		return NULL;
	}

	*out_len = sizeof(struct eap_hdr) + 2 + res;
	req->length = host_to_be16(*out_len);
	return (u8 *) req;
}


static u8 * eap_ttls_build_phase2_eap_req(struct eap_sm *sm,
					  struct eap_ttls_data *data,
					  int id, size_t *reqDataLen)
{
	u8 *req, *encr_req;
	size_t req_len;


	req = data->phase2_method->buildReq(sm, data->phase2_priv, id,
					    &req_len);
	if (req == NULL)
		return NULL;

	wpa_hexdump_key(MSG_DEBUG, "EAP-TTLS/EAP: Encapsulate Phase 2 data",
			req, req_len);

	if (eap_ttls_avp_encapsulate(&req, &req_len, RADIUS_ATTR_EAP_MESSAGE,
				     1) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: Failed to encapsulate "
			   "packet");
		return NULL;
	}

	wpa_hexdump_key(MSG_DEBUG, "EAP-TTLS/EAP: Encrypt encapsulated Phase "
			"2 data", req, req_len);

	encr_req = eap_ttls_encrypt(sm, data, id, req, req_len, reqDataLen);
	free(req);

	return encr_req;
}


static u8 * eap_ttls_build_phase2_mschapv2(struct eap_sm *sm,
					   struct eap_ttls_data *data,
					   int id, size_t *reqDataLen)
{
	u8 *req, *encr_req, *pos, *end;
	size_t req_len;
	int i;

	pos = req = malloc(100);
	if (req == NULL)
		return NULL;
	end = req + 200;

	if (data->mschapv2_resp_ok) {
		pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_MS_CHAP2_SUCCESS,
				       RADIUS_VENDOR_ID_MICROSOFT, 1, 43);
		*pos++ = data->mschapv2_ident;
		pos += snprintf((char *) pos, end - pos, "S=");
		for (i = 0; i < sizeof(data->mschapv2_auth_response); i++) {
			pos += snprintf((char *) pos, end - pos, "%02X",
					data->mschapv2_auth_response[i]);
		}
	} else {
		pos = eap_ttls_avp_hdr(pos, RADIUS_ATTR_MS_CHAP_ERROR,
				       RADIUS_VENDOR_ID_MICROSOFT, 1, 6);
		memcpy(pos, "Failed", 6);
		pos += 6;
		AVP_PAD(req, pos);
	}

	req_len = pos - req;
	wpa_hexdump_key(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Encrypting Phase 2 "
			"data", req, req_len);

	encr_req = eap_ttls_encrypt(sm, data, id, req, req_len, reqDataLen);
	free(req);

	return encr_req;
}


static u8 * eap_ttls_buildReq(struct eap_sm *sm, void *priv, int id,
			      size_t *reqDataLen)
{
	struct eap_ttls_data *data = priv;

	switch (data->state) {
	case START:
		return eap_ttls_build_start(sm, data, id, reqDataLen);
	case PHASE1:
		return eap_ttls_build_req(sm, data, id, reqDataLen);
	case PHASE2_METHOD:
		return eap_ttls_build_phase2_eap_req(sm, data, id, reqDataLen);
	case PHASE2_MSCHAPV2_RESP:
		return eap_ttls_build_phase2_mschapv2(sm, data, id,
						      reqDataLen);
	default:
		wpa_printf(MSG_DEBUG, "EAP-TTLS: %s - unexpected state %d",
			   __func__, data->state);
		return NULL;
	}
}


static Boolean eap_ttls_check(struct eap_sm *sm, void *priv,
			      u8 *respData, size_t respDataLen)
{
	struct eap_hdr *resp;
	u8 *pos;
	size_t len;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	if (respDataLen < sizeof(*resp) + 2 || *pos != EAP_TYPE_TTLS ||
	    (len = ntohs(resp->length)) > respDataLen) {
		wpa_printf(MSG_INFO, "EAP-TTLS: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_ttls_process_phase2_pap(struct eap_sm *sm,
					struct eap_ttls_data *data,
					const u8 *user_password,
					size_t user_password_len)
{
	/* TODO: add support for verifying that the user entry accepts
	 * EAP-TTLS/PAP. */
	if (!sm->user || !sm->user->password) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/PAP: No user password "
			   "configured");
		eap_ttls_state(data, FAILURE);
		return;
	}

	if (sm->user->password_len != user_password_len ||
	    memcmp(sm->user->password, user_password, user_password_len) != 0)
	{
		wpa_printf(MSG_DEBUG, "EAP-TTLS/PAP: Invalid user password");
		eap_ttls_state(data, FAILURE);
		return;
	}

	wpa_printf(MSG_DEBUG, "EAP-TTLS/PAP: Correct user password");
	eap_ttls_state(data, SUCCESS);
}


static void eap_ttls_process_phase2_chap(struct eap_sm *sm,
					 struct eap_ttls_data *data,
					 const u8 *challenge,
					 size_t challenge_len,
					 const u8 *password,
					 size_t password_len)
{
	MD5_CTX context;
	u8 *chal, hash[MD5_MAC_LEN];

	if (challenge == NULL || password == NULL ||
	    challenge_len != EAP_TTLS_CHAP_CHALLENGE_LEN ||
	    password_len != 1 + EAP_TTLS_CHAP_PASSWORD_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: Invalid CHAP attributes "
			   "(challenge len %lu password len %lu)",
			   (unsigned long) challenge_len,
			   (unsigned long) password_len);
		eap_ttls_state(data, FAILURE);
		return;
	}

	/* TODO: add support for verifying that the user entry accepts
	 * EAP-TTLS/CHAP. */
	if (!sm->user || !sm->user->password) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: No user password "
			   "configured");
		eap_ttls_state(data, FAILURE);
		return;
	}

	chal = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				  EAP_TTLS_CHAP_CHALLENGE_LEN + 1);
	if (chal == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: Failed to generate "
			   "challenge from TLS data");
		eap_ttls_state(data, FAILURE);
		return;
	}

	if (memcmp(challenge, chal, EAP_TTLS_CHAP_CHALLENGE_LEN) != 0 ||
	    password[0] != chal[EAP_TTLS_CHAP_CHALLENGE_LEN]) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: Challenge mismatch");
		free(chal);
		eap_ttls_state(data, FAILURE);
		return;
	}
	free(chal);

	/* MD5(Ident + Password + Challenge) */
	MD5Init(&context);
	MD5Update(&context, password, 1);
	MD5Update(&context, sm->user->password, sm->user->password_len);
	MD5Update(&context, challenge, challenge_len);
	MD5Final(hash, &context);

	if (memcmp(hash, password + 1, EAP_TTLS_CHAP_PASSWORD_LEN) == 0) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: Correct user password");
		eap_ttls_state(data, SUCCESS);
	} else {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/CHAP: Invalid user password");
		eap_ttls_state(data, FAILURE);
	}
}


static void eap_ttls_process_phase2_mschap(struct eap_sm *sm,
					   struct eap_ttls_data *data,
					   u8 *challenge, size_t challenge_len,
					   u8 *response, size_t response_len)
{
	u8 *chal, nt_response[24];

	if (challenge == NULL || response == NULL ||
	    challenge_len != EAP_TTLS_MSCHAP_CHALLENGE_LEN ||
	    response_len != EAP_TTLS_MSCHAP_RESPONSE_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: Invalid MS-CHAP "
			   "attributes (challenge len %lu response len %lu)",
			   (unsigned long) challenge_len,
			   (unsigned long) response_len);
		eap_ttls_state(data, FAILURE);
		return;
	}

	/* TODO: add support for verifying that the user entry accepts
	 * EAP-TTLS/MSCHAP. */
	if (!sm->user || !sm->user->password) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: No user password "
			   "configured");
		eap_ttls_state(data, FAILURE);
		return;
	}

	chal = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				  EAP_TTLS_MSCHAP_CHALLENGE_LEN + 1);
	if (chal == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: Failed to generate "
			   "challenge from TLS data");
		eap_ttls_state(data, FAILURE);
		return;
	}

	if (memcmp(challenge, chal, EAP_TTLS_MSCHAP_CHALLENGE_LEN) != 0 ||
	    response[0] != chal[EAP_TTLS_MSCHAP_CHALLENGE_LEN]) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: Challenge mismatch");
		free(chal);
		eap_ttls_state(data, FAILURE);
		return;
	}
	free(chal);

	nt_challenge_response(challenge, sm->user->password,
			      sm->user->password_len, nt_response);

	if (memcmp(nt_response, response + 2 + 24, 24) == 0) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: Correct response");
		eap_ttls_state(data, SUCCESS);
	} else {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAP: Invalid NT-Response");
		wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAP: Received",
			    response + 2 + 24, 24);
		wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAP: Expected",
			    nt_response, 24);
		eap_ttls_state(data, FAILURE);
	}
}


static void eap_ttls_process_phase2_mschapv2(struct eap_sm *sm,
					     struct eap_ttls_data *data,
					     u8 *challenge,
					     size_t challenge_len,
					     u8 *response, size_t response_len)
{
	u8 *chal, *username, nt_response[24], *pos, *rx_resp, *peer_challenge,
		*auth_challenge;
	size_t username_len;
	int i;

	if (challenge == NULL || response == NULL ||
	    challenge_len != EAP_TTLS_MSCHAPV2_CHALLENGE_LEN ||
	    response_len != EAP_TTLS_MSCHAPV2_RESPONSE_LEN) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Invalid MS-CHAP2 "
			   "attributes (challenge len %lu response len %lu)",
			   (unsigned long) challenge_len,
			   (unsigned long) response_len);
		eap_ttls_state(data, FAILURE);
		return;
	}

	/* TODO: add support for verifying that the user entry accepts
	 * EAP-TTLS/MSCHAPV2. */
	if (!sm->user || !sm->user->password) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: No user password "
			   "configured");
		eap_ttls_state(data, FAILURE);
		return;
	}

	/* MSCHAPv2 does not include optional domain name in the
	 * challenge-response calculation, so remove domain prefix
	 * (if present). */
	username = sm->identity;
	username_len = sm->identity_len;
	pos = username;
	for (i = 0; i < username_len; i++) {
		if (username[i] == '\\') {
			username_len -= i + 1;
			username += i + 1;
			break;
		}
	}

	chal = eap_tls_derive_key(sm, &data->ssl, "ttls challenge",
				  EAP_TTLS_MSCHAPV2_CHALLENGE_LEN + 1);
	if (chal == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Failed to generate "
			   "challenge from TLS data");
		eap_ttls_state(data, FAILURE);
		return;
	}

	if (memcmp(challenge, chal, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN) != 0 ||
	    response[0] != chal[EAP_TTLS_MSCHAPV2_CHALLENGE_LEN]) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Challenge mismatch");
		free(chal);
		eap_ttls_state(data, FAILURE);
		return;
	}
	free(chal);

	auth_challenge = challenge;
	peer_challenge = response + 2;

	wpa_hexdump_ascii(MSG_MSGDUMP, "EAP-TTLS/MSCHAPV2: User",
			  username, username_len);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAPV2: auth_challenge",
		    auth_challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);
	wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAPV2: peer_challenge",
		    peer_challenge, EAP_TTLS_MSCHAPV2_CHALLENGE_LEN);

	generate_nt_response(auth_challenge, peer_challenge,
			     username, username_len,
			     sm->user->password, sm->user->password_len,
			     nt_response);

	rx_resp = response + 2 + EAP_TTLS_MSCHAPV2_CHALLENGE_LEN + 8;
	if (memcmp(nt_response, rx_resp, 24) == 0) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Correct "
			   "NT-Response");
		data->mschapv2_resp_ok = 1;

		generate_authenticator_response(sm->user->password,
						sm->user->password_len,
						peer_challenge,
						auth_challenge,
						username, username_len,
						nt_response,
						data->mschapv2_auth_response);

	} else {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Invalid "
			   "NT-Response");
		wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAPV2: Received",
			    rx_resp, 24);
		wpa_hexdump(MSG_MSGDUMP, "EAP-TTLS/MSCHAPV2: Expected",
			    nt_response, 24);
		data->mschapv2_resp_ok = 0;
	}
	eap_ttls_state(data, PHASE2_MSCHAPV2_RESP);
	data->mschapv2_ident = response[0];
}


static int eap_ttls_phase2_eap_init(struct eap_sm *sm,
				    struct eap_ttls_data *data, u8 eap_type)
{
	if (data->phase2_priv && data->phase2_method) {
		data->phase2_method->reset(sm, data->phase2_priv);
		data->phase2_method = NULL;
		data->phase2_priv = NULL;
	}
	data->phase2_method = eap_sm_get_eap_methods(eap_type);
	if (!data->phase2_method)
		return -1;

	sm->init_phase2 = 1;
	data->phase2_priv = data->phase2_method->init(sm);
	sm->init_phase2 = 0;
	return 0;
}


static void eap_ttls_process_phase2_eap_response(struct eap_sm *sm,
						 struct eap_ttls_data *data,
						 u8 *in_data, size_t in_len)
{
	u8 next_type = EAP_TYPE_NONE;
	struct eap_hdr *hdr;
	u8 *pos;
	size_t left;

	if (data->phase2_priv == NULL) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: %s - Phase2 not "
			   "initialized?!", __func__);
		return;
	}

	hdr = (struct eap_hdr *) in_data;
	pos = (u8 *) (hdr + 1);
	left = in_len - sizeof(*hdr);

	if (in_len > sizeof(*hdr) && *pos == EAP_TYPE_NAK) {
		wpa_hexdump(MSG_DEBUG, "EAP-TTLS/EAP: Phase2 type Nak'ed; "
			    "allowed types", pos + 1, left - 1);
		eap_sm_process_nak(sm, pos + 1, left - 1);
		if (sm->user && sm->user_eap_method_index < EAP_MAX_METHODS &&
		    sm->user->methods[sm->user_eap_method_index] !=
		    EAP_TYPE_NONE) {
			next_type =
				sm->user->methods[sm->user_eap_method_index++];
			wpa_printf(MSG_DEBUG, "EAP-TTLS: try EAP type %d",
				   next_type);
			eap_ttls_phase2_eap_init(sm, data, next_type);
		} else {
			eap_ttls_state(data, FAILURE);
		}
		return;
	}

	if (data->phase2_method->check(sm, data->phase2_priv, in_data,
				       in_len)) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: Phase2 check() asked to "
			   "ignore the packet");
		return;
	}

	data->phase2_method->process(sm, data->phase2_priv, in_data, in_len);

	if (!data->phase2_method->isDone(sm, data->phase2_priv))
		return;

	if (!data->phase2_method->isSuccess(sm, data->phase2_priv)) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: Phase2 method failed");
		eap_ttls_state(data, FAILURE);
		return;
	}

	switch (data->state) {
	case PHASE2_START:
		if (eap_user_get(sm, sm->identity, sm->identity_len, 1) != 0) {
			wpa_hexdump_ascii(MSG_DEBUG, "EAP_TTLS: Phase2 "
					  "Identity not found in the user "
					  "database",
					  sm->identity, sm->identity_len);
			eap_ttls_state(data, FAILURE);
			break;
		}

		eap_ttls_state(data, PHASE2_METHOD);
		next_type = sm->user->methods[0];
		sm->user_eap_method_index = 1;
		wpa_printf(MSG_DEBUG, "EAP-TTLS: try EAP type %d", next_type);
		break;
	case PHASE2_METHOD:
		eap_ttls_state(data, SUCCESS);
		break;
	case FAILURE:
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TTLS: %s - unexpected state %d",
			   __func__, data->state);
		break;
	}

	eap_ttls_phase2_eap_init(sm, data, next_type);
}


static void eap_ttls_process_phase2_eap(struct eap_sm *sm,
					struct eap_ttls_data *data,
					const u8 *eap, size_t eap_len)
{
	struct eap_hdr *hdr;
	size_t len;

	if (data->state == PHASE2_START) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: initializing Phase 2");
		if (eap_ttls_phase2_eap_init(sm, data, EAP_TYPE_IDENTITY) < 0)
		{
			wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: failed to "
				   "initialize EAP-Identity");
			return;
		}
	}

	if (eap_len < sizeof(*hdr)) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: too short Phase 2 EAP "
			   "packet (len=%lu)", (unsigned long) eap_len);
		return;
	}

	hdr = (struct eap_hdr *) eap;
	len = be_to_host16(hdr->length);
	wpa_printf(MSG_DEBUG, "EAP-TTLS/EAP: received Phase 2 EAP: code=%d "
		   "identifier=%d length=%lu", hdr->code, hdr->identifier,
		   (unsigned long) len);
	if (len > eap_len) {
		wpa_printf(MSG_INFO, "EAP-TTLS/EAP: Length mismatch in Phase 2"
			   " EAP frame (hdr len=%lu, data len in AVP=%lu)",
			   (unsigned long) len, (unsigned long) eap_len);
		return;
	}

	switch (hdr->code) {
	case EAP_CODE_RESPONSE:
		eap_ttls_process_phase2_eap_response(sm, data, (u8 *) hdr,
						     len);
		break;
	default:
		wpa_printf(MSG_INFO, "EAP-TTLS/EAP: Unexpected code=%d in "
			   "Phase 2 EAP header", hdr->code);
		break;
	}
}


static void eap_ttls_process_phase2(struct eap_sm *sm,
				    struct eap_ttls_data *data,
				    struct eap_hdr *resp,
				    u8 *in_data, size_t in_len)
{
	u8 *in_decrypted;
	int buf_len, len_decrypted, res;
	struct eap_ttls_avp parse;

	wpa_printf(MSG_DEBUG, "EAP-TTLS: received %lu bytes encrypted data for"
		   " Phase 2", (unsigned long) in_len);

	res = eap_tls_data_reassemble(sm, &data->ssl, &in_data, &in_len);
	if (res < 0 || res == 1)
		return;

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
		return;
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
		free(in_decrypted);
		eap_ttls_state(data, FAILURE);
		return;
	}

	wpa_hexdump_key(MSG_DEBUG, "EAP-TTLS: Decrypted Phase 2 EAP",
			in_decrypted, len_decrypted);

	if (eap_ttls_avp_parse(in_decrypted, len_decrypted, &parse) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Failed to parse AVPs");
		free(in_decrypted);
		eap_ttls_state(data, FAILURE);
		return;
	}

	if (parse.user_name) {
		free(sm->identity);
		sm->identity = malloc(parse.user_name_len);
		if (sm->identity) {
			memcpy(sm->identity, parse.user_name,
			       parse.user_name_len);
			sm->identity_len = parse.user_name_len;
		}
		if (eap_user_get(sm, parse.user_name, parse.user_name_len, 1)
		    != 0) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS: Phase2 Identity not "
				   "found in the user database");
			eap_ttls_state(data, FAILURE);
			goto done;
		}
	}

	if (parse.eap) {
		eap_ttls_process_phase2_eap(sm, data, parse.eap,
					    parse.eap_len);
	} else if (parse.user_password) {
		eap_ttls_process_phase2_pap(sm, data, parse.user_password,
					    parse.user_password_len);
	} else if (parse.chap_password) {
		eap_ttls_process_phase2_chap(sm, data,
					     parse.chap_challenge,
					     parse.chap_challenge_len,
					     parse.chap_password,
					     parse.chap_password_len);
	} else if (parse.mschap_response) {
		eap_ttls_process_phase2_mschap(sm, data,
					       parse.mschap_challenge,
					       parse.mschap_challenge_len,
					       parse.mschap_response,
					       parse.mschap_response_len);
	} else if (parse.mschap2_response) {
		eap_ttls_process_phase2_mschapv2(sm, data,
						 parse.mschap_challenge,
						 parse.mschap_challenge_len,
						 parse.mschap2_response,
						 parse.mschap2_response_len);
	}

done:
	free(in_decrypted);
	free(parse.eap);
 }


static void eap_ttls_process(struct eap_sm *sm, void *priv,
			     u8 *respData, size_t respDataLen)
{
	struct eap_ttls_data *data = priv;
	struct eap_hdr *resp;
	u8 *pos, flags;
	int left;
	unsigned int tls_msg_len;
	int peer_version;

	resp = (struct eap_hdr *) respData;
	pos = (u8 *) (resp + 1);
	pos++;
	flags = *pos++;
	left = htons(resp->length) - sizeof(struct eap_hdr) - 2;
	wpa_printf(MSG_DEBUG, "EAP-TTLS: Received packet(len=%lu) - "
		   "Flags 0x%02x", (unsigned long) respDataLen, flags);
	peer_version = flags & EAP_PEAP_VERSION_MASK;
	if (peer_version < data->ttls_version) {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: peer ver=%d, own ver=%d; "
			   "use version %d",
			   peer_version, data->ttls_version, peer_version);
		data->ttls_version = peer_version;
			   
	}
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (left < 4) {
			wpa_printf(MSG_INFO, "EAP-TTLS: Short frame with TLS "
				   "length");
			eap_ttls_state(data, FAILURE);
			return;
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

	switch (data->state) {
	case PHASE1:
		if (eap_tls_process_helper(sm, &data->ssl, pos, left) < 0) {
			wpa_printf(MSG_INFO, "EAP-TTLS: TLS processing "
				   "failed");
			eap_ttls_state(data, FAILURE);
		}
		break;
	case PHASE2_START:
	case PHASE2_METHOD:
		eap_ttls_process_phase2(sm, data, resp, pos, left);
		break;
	case PHASE2_MSCHAPV2_RESP:
		if (data->mschapv2_resp_ok && left == 0) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Peer "
				   "acknowledged response");
			eap_ttls_state(data, SUCCESS);
		} else if (!data->mschapv2_resp_ok) {
			wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Peer "
				   "acknowledged error");
			eap_ttls_state(data, FAILURE);
		} else {
			wpa_printf(MSG_DEBUG, "EAP-TTLS/MSCHAPV2: Unexpected "
				   "frame from peer (payload len %d, expected "
				   "empty frame)", left);
			eap_ttls_state(data, FAILURE);
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Unexpected state %d in %s",
			   data->state, __func__);
		break;
	}
}


static Boolean eap_ttls_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	return data->state == SUCCESS || data->state == FAILURE;
}


static u8 * eap_ttls_getKey(struct eap_sm *sm, void *priv, size_t *len)
{
	struct eap_ttls_data *data = priv;
	u8 *eapKeyData;

	if (data->state != SUCCESS)
		return NULL;

	eapKeyData = eap_tls_derive_key(sm, &data->ssl,
					"ttls keying material",
					EAP_TLS_KEY_LEN);
	if (eapKeyData) {
		*len = EAP_TLS_KEY_LEN;
		wpa_hexdump(MSG_DEBUG, "EAP-TTLS: Derived key",
			    eapKeyData, EAP_TLS_KEY_LEN);
	} else {
		wpa_printf(MSG_DEBUG, "EAP-TTLS: Failed to derive key");
	}

	return eapKeyData;
}


static Boolean eap_ttls_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_ttls_data *data = priv;
	return data->state == SUCCESS;
}


const struct eap_method eap_method_ttls =
{
	.method = EAP_TYPE_TTLS,
	.name = "TTLS",
	.init = eap_ttls_init,
	.reset = eap_ttls_reset,
	.buildReq = eap_ttls_buildReq,
	.check = eap_ttls_check,
	.process = eap_ttls_process,
	.isDone = eap_ttls_isDone,
	.getKey = eap_ttls_getKey,
	.isSuccess = eap_ttls_isSuccess,
};
