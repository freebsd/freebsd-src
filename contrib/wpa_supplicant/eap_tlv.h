#ifndef EAP_TLV_H
#define EAP_TLV_H

/* EAP-TLV TLVs (draft-josefsson-ppext-eap-tls-eap-07.txt) */
#define EAP_TLV_RESULT_TLV 3 /* Acknowledged Result */
#define EAP_TLV_NAK_TLV 4
#define EAP_TLV_CRYPTO_BINDING_TLV 5
#define EAP_TLV_CONNECTION_BINDING_TLV 6
#define EAP_TLV_VENDOR_SPECIFIC_TLV 7
#define EAP_TLV_URI_TLV 8
#define EAP_TLV_EAP_PAYLOAD_TLV 9
#define EAP_TLV_INTERMEDIATE_RESULT_TLV 10
#define EAP_TLV_PAC_TLV 11 /* draft-cam-winget-eap-fast-01.txt */
#define EAP_TLV_CRYPTO_BINDING_TLV_ 12 /* draft-cam-winget-eap-fast-01.txt */

#define EAP_TLV_RESULT_SUCCESS 1
#define EAP_TLV_RESULT_FAILURE 2

#define EAP_TLV_TYPE_MANDATORY 0x8000

struct eap_tlv_hdr {
	u16 tlv_type;
	u16 length;
};

struct eap_tlv_nak_tlv {
	u16 tlv_type;
	u16 length;
	u32 vendor_id;
	u16 nak_type;
} __attribute__((packed));

struct eap_tlv_result_tlv {
	u16 tlv_type;
	u16 length;
	u16 status;
} __attribute__((packed));

struct eap_tlv_intermediate_result_tlv {
	u16 tlv_type;
	u16 length;
	u16 status;
} __attribute__((packed));

struct eap_tlv_crypto_binding__tlv {
	u16 tlv_type;
	u16 length;
	u8 reserved;
	u8 version;
	u8 received_version;
	u8 subtype;
	u8 nonce[32];
	u8 compound_mac[20];
} __attribute__((packed));

struct eap_tlv_pac_ack_tlv {
	u16 tlv_type;
	u16 length;
	u16 pac_type;
	u16 pac_len;
	u16 result;
} __attribute__((packed));

#define EAP_TLV_CRYPTO_BINDING_SUBTYPE_REQUEST 0
#define EAP_TLV_CRYPTO_BINDING_SUBTYPE_RESPONSE 1


u8 * eap_tlv_build_nak(int id, int nak_type, size_t *resp_len);
u8 * eap_tlv_build_result(int id, int status, size_t *resp_len);
int eap_tlv_process(struct eap_sm *sm, struct eap_method_ret *ret,
		    struct eap_hdr *hdr, u8 **resp, size_t *resp_len);

#endif /* EAP_TLV_H */
