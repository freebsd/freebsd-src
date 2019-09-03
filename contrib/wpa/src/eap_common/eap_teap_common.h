/*
 * EAP-TEAP definitions (RFC 7170)
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EAP_TEAP_H
#define EAP_TEAP_H

#define EAP_TEAP_VERSION 1
#define EAP_TEAP_KEY_LEN 64
#define EAP_TEAP_IMCK_LEN 60
#define EAP_TEAP_SIMCK_LEN 40
#define EAP_TEAP_CMK_LEN 20
#define EAP_TEAP_COMPOUND_MAC_LEN 20
#define EAP_TEAP_NONCE_LEN 32

#define TEAP_TLS_EXPORTER_LABEL_SKS "EXPORTER: teap session key seed"

#define TLS_EXT_PAC_OPAQUE 35

/*
 * RFC 7170: Section 4.2.12.1 - Formats for PAC Attributes
 * Note: bit 0x8000 (Mandatory) and bit 0x4000 (Reserved) are also defined
 * in the general TLV format (Section 4.2.1).
 */
#define PAC_TYPE_PAC_KEY 1
#define PAC_TYPE_PAC_OPAQUE 2
#define PAC_TYPE_CRED_LIFETIME 3
#define PAC_TYPE_A_ID 4
#define PAC_TYPE_I_ID 5
/* 6 - Reserved */
#define PAC_TYPE_A_ID_INFO 7
#define PAC_TYPE_PAC_ACKNOWLEDGEMENT 8
#define PAC_TYPE_PAC_INFO 9
#define PAC_TYPE_PAC_TYPE 10

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

struct pac_attr_hdr {
	be16 type;
	be16 len;
} STRUCT_PACKED;

struct teap_tlv_hdr {
	be16 tlv_type;
	be16 length;
} STRUCT_PACKED;

/* Result TLV and Intermediate-Result TLV */
struct teap_tlv_result {
	be16 tlv_type;
	be16 length;
	be16 status;
	/* for Intermediate-Result TLV, followed by optional TLVs */
} STRUCT_PACKED;

struct teap_tlv_nak {
	be16 tlv_type;
	be16 length;
	be32 vendor_id;
	be16 nak_type;
	/* followed by optional TLVs */
} STRUCT_PACKED;

struct teap_tlv_crypto_binding {
	be16 tlv_type; /* TLV Type[14b] and M/R flags */
	be16 length;
	u8 reserved;
	u8 version;
	u8 received_version;
	u8 subtype; /* Flags[4b] and Sub-Type[4b] */
	u8 nonce[EAP_TEAP_NONCE_LEN];
	u8 emsk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];
	u8 msk_compound_mac[EAP_TEAP_COMPOUND_MAC_LEN];
} STRUCT_PACKED;

struct teap_tlv_request_action {
	be16 tlv_type;
	be16 length;
	u8 status;
	u8 action;
	/* followed by optional TLVs */
} STRUCT_PACKED;

enum teap_request_action {
	TEAP_REQUEST_ACTION_PROCESS_TLV = 1,
	TEAP_REQUEST_ACTION_NEGOTIATE_EAP = 2,
};

/* PAC TLV with PAC-Acknowledgement TLV attribute */
struct teap_tlv_pac_ack {
	be16 tlv_type;
	be16 length;
	be16 pac_type;
	be16 pac_len;
	be16 result;
} STRUCT_PACKED;

struct teap_attr_pac_type {
	be16 type; /* PAC_TYPE_PAC_TYPE */
	be16 length; /* 2 */
	be16 pac_type;
} STRUCT_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

#define TEAP_CRYPTO_BINDING_SUBTYPE_REQUEST 0
#define TEAP_CRYPTO_BINDING_SUBTYPE_RESPONSE 1

#define TEAP_CRYPTO_BINDING_EMSK_CMAC 1
#define TEAP_CRYPTO_BINDING_MSK_CMAC 2
#define TEAP_CRYPTO_BINDING_EMSK_AND_MSK_CMAC 3


#define EAP_TEAP_PAC_KEY_LEN 48

/* RFC 7170: 4.2.12.6 PAC-Type TLV */
#define PAC_TYPE_TUNNEL_PAC 1


/* RFC 7170, 4.2.1: General TLV Format */
enum teap_tlv_types {
	TEAP_TLV_AUTHORITY_ID = 1,
	TEAP_TLV_IDENTITY_TYPE = 2,
	TEAP_TLV_RESULT = 3,
	TEAP_TLV_NAK = 4,
	TEAP_TLV_ERROR = 5,
	TEAP_TLV_CHANNEL_BINDING = 6,
	TEAP_TLV_VENDOR_SPECIFIC = 7,
	TEAP_TLV_REQUEST_ACTION = 8,
	TEAP_TLV_EAP_PAYLOAD = 9,
	TEAP_TLV_INTERMEDIATE_RESULT = 10,
	TEAP_TLV_PAC = 11,
	TEAP_TLV_CRYPTO_BINDING = 12,
	TEAP_TLV_BASIC_PASSWORD_AUTH_REQ = 13,
	TEAP_TLV_BASIC_PASSWORD_AUTH_RESP = 14,
	TEAP_TLV_PKCS7 = 15,
	TEAP_TLV_PKCS10 = 16,
	TEAP_TLV_TRUSTED_SERVER_ROOT = 17,
};

enum teap_tlv_result_status {
	TEAP_STATUS_SUCCESS = 1,
	TEAP_STATUS_FAILURE = 2
};

#define TEAP_TLV_MANDATORY 0x8000
#define TEAP_TLV_TYPE_MASK 0x3fff

/* RFC 7170, 4.2.6: Error TLV */
enum teap_error_codes {
	TEAP_ERROR_INNER_METHOD = 1001,
	TEAP_ERROR_UNSPEC_AUTH_INFRA_PROBLEM = 1002,
	TEAP_ERROR_UNSPEC_AUTHENTICATION_FAILURE = 1003,
	TEAP_ERROR_UNSPEC_AUTHORIZATION_FAILURE = 1004,
	TEAP_ERROR_USER_ACCOUNT_CRED_UNAVAILABLE = 1005,
	TEAP_ERROR_USER_ACCOUNT_EXPIRED = 1006,
	TEAP_ERROR_USER_ACCOUNT_LOCKED_TRY_AGAIN_LATER = 1007,
	TEAP_ERROR_USER_ACCOUNT_LOCKED_ADMIN_REQ = 1008,
	TEAP_ERROR_TUNNEL_COMPROMISE_ERROR = 2001,
	TEAP_ERROR_UNEXPECTED_TLVS_EXCHANGED = 2002,
};

struct wpabuf;
struct tls_connection;

struct eap_teap_tlv_parse {
	u8 *eap_payload_tlv;
	size_t eap_payload_tlv_len;
	struct teap_tlv_crypto_binding *crypto_binding;
	size_t crypto_binding_len;
	int iresult;
	int result;
	u8 *nak;
	size_t nak_len;
	u8 request_action;
	u8 request_action_status;
	u8 *pac;
	size_t pac_len;
	u8 *basic_auth_req;
	size_t basic_auth_req_len;
	u8 *basic_auth_resp;
	size_t basic_auth_resp_len;
};

void eap_teap_put_tlv_hdr(struct wpabuf *buf, u16 type, u16 len);
void eap_teap_put_tlv(struct wpabuf *buf, u16 type, const void *data, u16 len);
void eap_teap_put_tlv_buf(struct wpabuf *buf, u16 type,
			  const struct wpabuf *data);
struct wpabuf * eap_teap_tlv_eap_payload(struct wpabuf *buf);
int eap_teap_derive_eap_msk(const u8 *simck, u8 *msk);
int eap_teap_derive_eap_emsk(const u8 *simck, u8 *emsk);
int eap_teap_derive_cmk_basic_pw_auth(const u8 *s_imck_msk, u8 *cmk);
int eap_teap_derive_imck(const u8 *prev_s_imck_msk, const u8 *prev_s_imck_emsk,
			 const u8 *msk, size_t msk_len,
			 const u8 *emsk, size_t emsk_len,
			 u8 *s_imck_msk, u8 *cmk_msk,
			 u8 *s_imck_emsk, u8 *cmk_emsk);
int eap_teap_compound_mac(u16 tls_cs, const struct teap_tlv_crypto_binding *cb,
			  const struct wpabuf *server_outer_tlvs,
			  const struct wpabuf *peer_outer_tlvs,
			  const u8 *cmk, u8 *compound_mac);
int eap_teap_parse_tlv(struct eap_teap_tlv_parse *tlv,
		       int tlv_type, u8 *pos, size_t len);
const char * eap_teap_tlv_type_str(enum teap_tlv_types type);
struct wpabuf * eap_teap_tlv_result(int status, int intermediate);
struct wpabuf * eap_teap_tlv_error(enum teap_error_codes error);
int eap_teap_allowed_anon_prov_phase2_method(u8 type);
int eap_teap_allowed_anon_prov_cipher_suite(u16 cs);

#endif /* EAP_TEAP_H */
