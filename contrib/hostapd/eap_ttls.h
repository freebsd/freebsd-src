#ifndef EAP_TTLS_H
#define EAP_TTLS_H

struct ttls_avp {
	u32 avp_code;
	u32 avp_length; /* 8-bit flags, 24-bit length;
			 * length includes AVP header */
	/* optional 32-bit Vendor-ID */
	/* Data */
};

struct ttls_avp_vendor {
	u32 avp_code;
	u32 avp_length; /* 8-bit flags, 24-bit length;
			 * length includes AVP header */
	u32 vendor_id;
	/* Data */
};

#define AVP_FLAGS_VENDOR 0x80
#define AVP_FLAGS_MANDATORY 0x40

#define AVP_PAD(start, pos) \
do { \
	int pad; \
	pad = (4 - (((pos) - (start)) & 3)) & 3; \
	memset((pos), 0, pad); \
	pos += pad; \
} while(0)


/* RFC 2865 */
#define RADIUS_ATTR_USER_NAME 1
#define RADIUS_ATTR_USER_PASSWORD 2
#define RADIUS_ATTR_CHAP_PASSWORD 3
#define RADIUS_ATTR_REPLY_MESSAGE 18
#define RADIUS_ATTR_CHAP_CHALLENGE 60
#define RADIUS_ATTR_EAP_MESSAGE 79

/* RFC 2548 */
#define RADIUS_VENDOR_ID_MICROSOFT 311
#define RADIUS_ATTR_MS_CHAP_RESPONSE 1
#define RADIUS_ATTR_MS_CHAP_ERROR 2
#define RADIUS_ATTR_MS_CHAP_NT_ENC_PW 6
#define RADIUS_ATTR_MS_CHAP_CHALLENGE 11
#define RADIUS_ATTR_MS_CHAP2_RESPONSE 25
#define RADIUS_ATTR_MS_CHAP2_SUCCESS 26
#define RADIUS_ATTR_MS_CHAP2_CPW 27

#define EAP_TTLS_MSCHAPV2_CHALLENGE_LEN 16
#define EAP_TTLS_MSCHAPV2_RESPONSE_LEN 50
#define EAP_TTLS_MSCHAP_CHALLENGE_LEN 8
#define EAP_TTLS_MSCHAP_RESPONSE_LEN 50
#define EAP_TTLS_CHAP_CHALLENGE_LEN 16
#define EAP_TTLS_CHAP_PASSWORD_LEN 16

#endif /* EAP_TTLS_H */
