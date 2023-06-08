/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* \summary: Internet Security Association and Key Management Protocol (ISAKMP) printer */

/* specification: RFC 2407, RFC 2408, RFC 5996 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* The functions from print-esp.c used in this file are only defined when both
 * OpenSSL and evp.h are detected. Employ the same preprocessor device here.
 */
#ifndef HAVE_OPENSSL_EVP_H
#undef HAVE_LIBCRYPTO
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect-ctype.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"
#include "ip6.h"
#include "ipproto.h"

typedef nd_byte cookie_t[8];
typedef nd_byte msgid_t[4];

#define PORT_ISAKMP 500

/* 3.1 ISAKMP Header Format (IKEv1 and IKEv2)
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !                          Initiator                            !
        !                            Cookie                             !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !                          Responder                            !
        !                            Cookie                             !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !  Next Payload ! MjVer ! MnVer ! Exchange Type !     Flags     !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !                          Message ID                           !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !                            Length                             !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct isakmp {
	cookie_t i_ck;		/* Initiator Cookie */
	cookie_t r_ck;		/* Responder Cookie */
	nd_uint8_t np;		/* Next Payload Type */
	nd_uint8_t vers;
#define ISAKMP_VERS_MAJOR	0xf0
#define ISAKMP_VERS_MAJOR_SHIFT	4
#define ISAKMP_VERS_MINOR	0x0f
#define ISAKMP_VERS_MINOR_SHIFT	0
	nd_uint8_t etype;	/* Exchange Type */
	nd_uint8_t flags;	/* Flags */
	msgid_t msgid;
	nd_uint32_t len;	/* Length */
};

/* Next Payload Type */
#define ISAKMP_NPTYPE_NONE   0 /* NONE*/
#define ISAKMP_NPTYPE_SA     1 /* Security Association */
#define ISAKMP_NPTYPE_P      2 /* Proposal */
#define ISAKMP_NPTYPE_T      3 /* Transform */
#define ISAKMP_NPTYPE_KE     4 /* Key Exchange */
#define ISAKMP_NPTYPE_ID     5 /* Identification */
#define ISAKMP_NPTYPE_CERT   6 /* Certificate */
#define ISAKMP_NPTYPE_CR     7 /* Certificate Request */
#define ISAKMP_NPTYPE_HASH   8 /* Hash */
#define ISAKMP_NPTYPE_SIG    9 /* Signature */
#define ISAKMP_NPTYPE_NONCE 10 /* Nonce */
#define ISAKMP_NPTYPE_N     11 /* Notification */
#define ISAKMP_NPTYPE_D     12 /* Delete */
#define ISAKMP_NPTYPE_VID   13 /* Vendor ID */
#define ISAKMP_NPTYPE_v2E   46 /* v2 Encrypted payload */

#define IKEv1_MAJOR_VERSION  1
#define IKEv1_MINOR_VERSION  0

#define IKEv2_MAJOR_VERSION  2
#define IKEv2_MINOR_VERSION  0

/* Flags */
#define ISAKMP_FLAG_E 0x01 /* Encryption Bit */
#define ISAKMP_FLAG_C 0x02 /* Commit Bit */
#define ISAKMP_FLAG_extra 0x04

/* IKEv2 */
#define ISAKMP_FLAG_I (1 << 3)  /* (I)nitiator */
#define ISAKMP_FLAG_V (1 << 4)  /* (V)ersion   */
#define ISAKMP_FLAG_R (1 << 5)  /* (R)esponse  */


/* 3.2 Payload Generic Header
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        ! Next Payload  !   RESERVED    !         Payload Length        !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct isakmp_gen {
	nd_uint8_t  np;       /* Next Payload */
	nd_uint8_t  critical; /* bit 7 - critical, rest is RESERVED */
	nd_uint16_t len;      /* Payload Length */
};

/* 3.3 Data Attributes
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        !A!       Attribute Type        !    AF=0  Attribute Length     !
        !F!                             !    AF=1  Attribute Value      !
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        .                   AF=0  Attribute Value                       .
        .                   AF=1  Not Transmitted                       .
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct isakmp_data {
	nd_uint16_t type;     /* defined by DOI-spec, and Attribute Format */
	nd_uint16_t lorv;     /* if f equal 1, Attribute Length */
	                      /* if f equal 0, Attribute Value */
	/* if f equal 1, Attribute Value */
};

/* 3.4 Security Association Payload */
	/* MAY NOT be used, because of being defined in ipsec-doi. */
	/*
	If the current payload is the last in the message,
	then the value of the next payload field will be 0.
	This field MUST NOT contain the
	values for the Proposal or Transform payloads as they are considered
	part of the security association negotiation.  For example, this
	field would contain the value "10" (Nonce payload) in the first
	message of a Base Exchange (see Section 4.4) and the value "0" in the
	first message of an Identity Protect Exchange (see Section 4.5).
	*/
struct ikev1_pl_sa {
	struct isakmp_gen h;
	nd_uint32_t doi; /* Domain of Interpretation */
	nd_uint32_t sit; /* Situation */
};

/* 3.5 Proposal Payload */
	/*
	The value of the next payload field MUST only contain the value "2"
	or "0".  If there are additional Proposal payloads in the message,
	then this field will be 2.  If the current Proposal payload is the
	last within the security association proposal, then this field will
	be 0.
	*/
struct ikev1_pl_p {
	struct isakmp_gen h;
	nd_uint8_t p_no;      /* Proposal # */
	nd_uint8_t prot_id;   /* Protocol */
	nd_uint8_t spi_size;  /* SPI Size */
	nd_uint8_t num_t;     /* Number of Transforms */
	/* SPI */
};

/* 3.6 Transform Payload */
	/*
	The value of the next payload field MUST only contain the value "3"
	or "0".  If there are additional Transform payloads in the proposal,
	then this field will be 3.  If the current Transform payload is the
	last within the proposal, then this field will be 0.
	*/
struct ikev1_pl_t {
	struct isakmp_gen h;
	nd_uint8_t  t_no;        /* Transform # */
	nd_uint8_t  t_id;        /* Transform-Id */
	nd_byte     reserved[2]; /* RESERVED2 */
	/* SA Attributes */
};

/* 3.7 Key Exchange Payload */
struct ikev1_pl_ke {
	struct isakmp_gen h;
	/* Key Exchange Data */
};

/* 3.8 Identification Payload */
	/* MUST NOT to be used, because of being defined in ipsec-doi. */
struct ikev1_pl_id {
	struct isakmp_gen h;
	union {
		nd_uint8_t  id_type;   /* ID Type */
		nd_uint32_t doi_data;  /* DOI Specific ID Data */
	} d;
	/* Identification Data */
};

/* 3.9 Certificate Payload */
struct ikev1_pl_cert {
	struct isakmp_gen h;
	nd_uint8_t encode; /* Cert Encoding */
	nd_uint8_t cert;   /* Certificate Data */
		/*
		This field indicates the type of
		certificate or certificate-related information contained in the
		Certificate Data field.
		*/
};

/* 3.10 Certificate Request Payload */
struct ikev1_pl_cr {
	struct isakmp_gen h;
	nd_uint8_t num_cert; /* # Cert. Types */
	/*
	Certificate Types (variable length)
	  -- Contains a list of the types of certificates requested,
	  sorted in order of preference.  Each individual certificate
	  type is 1 octet.  This field is NOT requiredo
	*/
	/* # Certificate Authorities (1 octet) */
	/* Certificate Authorities (variable length) */
};

/* 3.11 Hash Payload */
	/* may not be used, because of having only data. */
struct ikev1_pl_hash {
	struct isakmp_gen h;
	/* Hash Data */
};

/* 3.12 Signature Payload */
	/* may not be used, because of having only data. */
struct ikev1_pl_sig {
	struct isakmp_gen h;
	/* Signature Data */
};

/* 3.13 Nonce Payload */
	/* may not be used, because of having only data. */
struct ikev1_pl_nonce {
	struct isakmp_gen h;
	/* Nonce Data */
};

/* 3.14 Notification Payload */
struct ikev1_pl_n {
	struct isakmp_gen h;
	nd_uint32_t doi;      /* Domain of Interpretation */
	nd_uint8_t  prot_id;  /* Protocol-ID */
	nd_uint8_t  spi_size; /* SPI Size */
	nd_uint16_t type;     /* Notify Message Type */
	/* SPI */
	/* Notification Data */
};

/* 3.14.1 Notify Message Types */
/* NOTIFY MESSAGES - ERROR TYPES */
#define ISAKMP_NTYPE_INVALID_PAYLOAD_TYPE           1
#define ISAKMP_NTYPE_DOI_NOT_SUPPORTED              2
#define ISAKMP_NTYPE_SITUATION_NOT_SUPPORTED        3
#define ISAKMP_NTYPE_INVALID_COOKIE                 4
#define ISAKMP_NTYPE_INVALID_MAJOR_VERSION          5
#define ISAKMP_NTYPE_INVALID_MINOR_VERSION          6
#define ISAKMP_NTYPE_INVALID_EXCHANGE_TYPE          7
#define ISAKMP_NTYPE_INVALID_FLAGS                  8
#define ISAKMP_NTYPE_INVALID_MESSAGE_ID             9
#define ISAKMP_NTYPE_INVALID_PROTOCOL_ID            10
#define ISAKMP_NTYPE_INVALID_SPI                    11
#define ISAKMP_NTYPE_INVALID_TRANSFORM_ID           12
#define ISAKMP_NTYPE_ATTRIBUTES_NOT_SUPPORTED       13
#define ISAKMP_NTYPE_NO_PROPOSAL_CHOSEN             14
#define ISAKMP_NTYPE_BAD_PROPOSAL_SYNTAX            15
#define ISAKMP_NTYPE_PAYLOAD_MALFORMED              16
#define ISAKMP_NTYPE_INVALID_KEY_INFORMATION        17
#define ISAKMP_NTYPE_INVALID_ID_INFORMATION         18
#define ISAKMP_NTYPE_INVALID_CERT_ENCODING          19
#define ISAKMP_NTYPE_INVALID_CERTIFICATE            20
#define ISAKMP_NTYPE_BAD_CERT_REQUEST_SYNTAX        21
#define ISAKMP_NTYPE_INVALID_CERT_AUTHORITY         22
#define ISAKMP_NTYPE_INVALID_HASH_INFORMATION       23
#define ISAKMP_NTYPE_AUTHENTICATION_FAILED          24
#define ISAKMP_NTYPE_INVALID_SIGNATURE              25
#define ISAKMP_NTYPE_ADDRESS_NOTIFICATION           26

/* 3.15 Delete Payload */
struct ikev1_pl_d {
	struct isakmp_gen h;
	nd_uint32_t doi;      /* Domain of Interpretation */
	nd_uint8_t  prot_id;  /* Protocol-Id */
	nd_uint8_t  spi_size; /* SPI Size */
	nd_uint16_t num_spi;  /* # of SPIs */
	/* SPI(es) */
};

/* IKEv2 (RFC4306) */

/* 3.3  Security Association Payload -- generic header */
/* 3.3.1.  Proposal Substructure */
struct ikev2_p {
	struct isakmp_gen h;
	nd_uint8_t p_no;      /* Proposal # */
	nd_uint8_t prot_id;   /* Protocol */
	nd_uint8_t spi_size;  /* SPI Size */
	nd_uint8_t num_t;     /* Number of Transforms */
};

/* 3.3.2.  Transform Substructure */
struct ikev2_t {
	struct isakmp_gen h;
	nd_uint8_t  t_type;    /* Transform Type (ENCR,PRF,INTEG,etc.*/
	nd_byte     res2;      /* reserved byte */
	nd_uint16_t t_id;     /* Transform ID */
};

enum ikev2_t_type {
	IV2_T_ENCR = 1,
	IV2_T_PRF  = 2,
	IV2_T_INTEG= 3,
	IV2_T_DH   = 4,
	IV2_T_ESN  = 5
};

/* 3.4.  Key Exchange Payload */
struct ikev2_ke {
	struct isakmp_gen h;
	nd_uint16_t  ke_group;
	nd_uint16_t  ke_res1;
	/* KE data */
};


/* 3.5.  Identification Payloads */
enum ikev2_id_type {
	ID_IPV4_ADDR=1,
	ID_FQDN=2,
	ID_RFC822_ADDR=3,
	ID_IPV6_ADDR=5,
	ID_DER_ASN1_DN=9,
	ID_DER_ASN1_GN=10,
	ID_KEY_ID=11
};
struct ikev2_id {
	struct isakmp_gen h;
	nd_uint8_t type;        /* ID type */
	nd_byte    res1;
	nd_byte    res2[2];
	/* SPI */
	/* Notification Data */
};

/* 3.10 Notification Payload */
struct ikev2_n {
	struct isakmp_gen h;
	nd_uint8_t  prot_id;  /* Protocol-ID */
	nd_uint8_t  spi_size; /* SPI Size */
	nd_uint16_t type;     /* Notify Message Type */
};

enum ikev2_n_type {
	IV2_NOTIFY_UNSUPPORTED_CRITICAL_PAYLOAD            = 1,
	IV2_NOTIFY_INVALID_IKE_SPI                         = 4,
	IV2_NOTIFY_INVALID_MAJOR_VERSION                   = 5,
	IV2_NOTIFY_INVALID_SYNTAX                          = 7,
	IV2_NOTIFY_INVALID_MESSAGE_ID                      = 9,
	IV2_NOTIFY_INVALID_SPI                             =11,
	IV2_NOTIFY_NO_PROPOSAL_CHOSEN                      =14,
	IV2_NOTIFY_INVALID_KE_PAYLOAD                      =17,
	IV2_NOTIFY_AUTHENTICATION_FAILED                   =24,
	IV2_NOTIFY_SINGLE_PAIR_REQUIRED                    =34,
	IV2_NOTIFY_NO_ADDITIONAL_SAS                       =35,
	IV2_NOTIFY_INTERNAL_ADDRESS_FAILURE                =36,
	IV2_NOTIFY_FAILED_CP_REQUIRED                      =37,
	IV2_NOTIFY_INVALID_SELECTORS                       =39,
	IV2_NOTIFY_INITIAL_CONTACT                         =16384,
	IV2_NOTIFY_SET_WINDOW_SIZE                         =16385,
	IV2_NOTIFY_ADDITIONAL_TS_POSSIBLE                  =16386,
	IV2_NOTIFY_IPCOMP_SUPPORTED                        =16387,
	IV2_NOTIFY_NAT_DETECTION_SOURCE_IP                 =16388,
	IV2_NOTIFY_NAT_DETECTION_DESTINATION_IP            =16389,
	IV2_NOTIFY_COOKIE                                  =16390,
	IV2_NOTIFY_USE_TRANSPORT_MODE                      =16391,
	IV2_NOTIFY_HTTP_CERT_LOOKUP_SUPPORTED              =16392,
	IV2_NOTIFY_REKEY_SA                                =16393,
	IV2_NOTIFY_ESP_TFC_PADDING_NOT_SUPPORTED           =16394,
	IV2_NOTIFY_NON_FIRST_FRAGMENTS_ALSO                =16395
};

struct notify_messages {
	uint16_t type;
	char     *msg;
};

/* 3.8 Authentication Payload */
struct ikev2_auth {
	struct isakmp_gen h;
	nd_uint8_t  auth_method;  /* Protocol-ID */
	nd_byte     reserved[3];
	/* authentication data */
};

enum ikev2_auth_type {
	IV2_RSA_SIG = 1,
	IV2_SHARED  = 2,
	IV2_DSS_SIG = 3
};

/* refer to RFC 2409 */

#if 0
/* isakmp sa structure */
struct oakley_sa {
	uint8_t  proto_id;            /* OAKLEY */
	vchar_t   *spi;                /* spi */
	uint8_t  dhgrp;               /* DH; group */
	uint8_t  auth_t;              /* method of authentication */
	uint8_t  prf_t;               /* type of prf */
	uint8_t  hash_t;              /* type of hash */
	uint8_t  enc_t;               /* type of cipher */
	uint8_t  life_t;              /* type of duration of lifetime */
	uint32_t ldur;                /* life duration */
};
#endif

/* refer to RFC 2407 */

#define IPSEC_DOI 1

/* 4.2 IPSEC Situation Definition */
#define IPSECDOI_SIT_IDENTITY_ONLY           0x00000001
#define IPSECDOI_SIT_SECRECY                 0x00000002
#define IPSECDOI_SIT_INTEGRITY               0x00000004

/* 4.4.1 IPSEC Security Protocol Identifiers */
  /* 4.4.2 IPSEC ISAKMP Transform Values */
#define IPSECDOI_PROTO_ISAKMP                        1
#define   IPSECDOI_KEY_IKE                             1

/* 4.4.1 IPSEC Security Protocol Identifiers */
#define IPSECDOI_PROTO_IPSEC_AH                      2
  /* 4.4.3 IPSEC AH Transform Values */
#define   IPSECDOI_AH_MD5                              2
#define   IPSECDOI_AH_SHA                              3
#define   IPSECDOI_AH_DES                              4
#define   IPSECDOI_AH_SHA2_256                         5
#define   IPSECDOI_AH_SHA2_384                         6
#define   IPSECDOI_AH_SHA2_512                         7

/* 4.4.1 IPSEC Security Protocol Identifiers */
#define IPSECDOI_PROTO_IPSEC_ESP                     3
  /* 4.4.4 IPSEC ESP Transform Identifiers */
#define   IPSECDOI_ESP_DES_IV64                        1
#define   IPSECDOI_ESP_DES                             2
#define   IPSECDOI_ESP_3DES                            3
#define   IPSECDOI_ESP_RC5                             4
#define   IPSECDOI_ESP_IDEA                            5
#define   IPSECDOI_ESP_CAST                            6
#define   IPSECDOI_ESP_BLOWFISH                        7
#define   IPSECDOI_ESP_3IDEA                           8
#define   IPSECDOI_ESP_DES_IV32                        9
#define   IPSECDOI_ESP_RC4                            10
#define   IPSECDOI_ESP_NULL                           11
#define   IPSECDOI_ESP_RIJNDAEL				12
#define   IPSECDOI_ESP_AES				12

/* 4.4.1 IPSEC Security Protocol Identifiers */
#define IPSECDOI_PROTO_IPCOMP                        4
  /* 4.4.5 IPSEC IPCOMP Transform Identifiers */
#define   IPSECDOI_IPCOMP_OUI                          1
#define   IPSECDOI_IPCOMP_DEFLATE                      2
#define   IPSECDOI_IPCOMP_LZS                          3

/* 4.5 IPSEC Security Association Attributes */
#define IPSECDOI_ATTR_SA_LTYPE                1 /* B */
#define   IPSECDOI_ATTR_SA_LTYPE_DEFAULT        1
#define   IPSECDOI_ATTR_SA_LTYPE_SEC            1
#define   IPSECDOI_ATTR_SA_LTYPE_KB             2
#define IPSECDOI_ATTR_SA_LDUR                 2 /* V */
#define   IPSECDOI_ATTR_SA_LDUR_DEFAULT         28800 /* 8 hours */
#define IPSECDOI_ATTR_GRP_DESC                3 /* B */
#define IPSECDOI_ATTR_ENC_MODE                4 /* B */
	/* default value: host dependent */
#define   IPSECDOI_ATTR_ENC_MODE_TUNNEL         1
#define   IPSECDOI_ATTR_ENC_MODE_TRNS           2
#define IPSECDOI_ATTR_AUTH                    5 /* B */
	/* 0 means not to use authentication. */
#define   IPSECDOI_ATTR_AUTH_HMAC_MD5           1
#define   IPSECDOI_ATTR_AUTH_HMAC_SHA1          2
#define   IPSECDOI_ATTR_AUTH_DES_MAC            3
#define   IPSECDOI_ATTR_AUTH_KPDK               4 /*RFC-1826(Key/Pad/Data/Key)*/
	/*
	 * When negotiating ESP without authentication, the Auth
	 * Algorithm attribute MUST NOT be included in the proposal.
	 * When negotiating ESP without confidentiality, the Auth
	 * Algorithm attribute MUST be included in the proposal and
	 * the ESP transform ID must be ESP_NULL.
	*/
#define IPSECDOI_ATTR_KEY_LENGTH              6 /* B */
#define IPSECDOI_ATTR_KEY_ROUNDS              7 /* B */
#define IPSECDOI_ATTR_COMP_DICT_SIZE          8 /* B */
#define IPSECDOI_ATTR_COMP_PRIVALG            9 /* V */

/* 4.6.1 Security Association Payload */
struct ipsecdoi_sa {
	struct isakmp_gen h;
	nd_uint32_t doi; /* Domain of Interpretation */
	nd_uint32_t sit; /* Situation */
};

struct ipsecdoi_secrecy_h {
	nd_uint16_t len;
	nd_uint16_t reserved;
};

/* 4.6.2.1 Identification Type Values */
struct ipsecdoi_id {
	struct isakmp_gen h;
	nd_uint8_t  type;	/* ID Type */
	nd_uint8_t  proto_id;	/* Protocol ID */
	nd_uint16_t port;	/* Port */
	/* Identification Data */
};

#define IPSECDOI_ID_IPV4_ADDR                        1
#define IPSECDOI_ID_FQDN                             2
#define IPSECDOI_ID_USER_FQDN                        3
#define IPSECDOI_ID_IPV4_ADDR_SUBNET                 4
#define IPSECDOI_ID_IPV6_ADDR                        5
#define IPSECDOI_ID_IPV6_ADDR_SUBNET                 6
#define IPSECDOI_ID_IPV4_ADDR_RANGE                  7
#define IPSECDOI_ID_IPV6_ADDR_RANGE                  8
#define IPSECDOI_ID_DER_ASN1_DN                      9
#define IPSECDOI_ID_DER_ASN1_GN                      10
#define IPSECDOI_ID_KEY_ID                           11

/* 4.6.3 IPSEC DOI Notify Message Types */
/* Notify Messages - Status Types */
#define IPSECDOI_NTYPE_RESPONDER_LIFETIME                  24576
#define IPSECDOI_NTYPE_REPLAY_STATUS                       24577
#define IPSECDOI_NTYPE_INITIAL_CONTACT                     24578

#define DECLARE_PRINTER(func) static const u_char *ike##func##_print( \
		netdissect_options *ndo, u_char tpay,	              \
		const struct isakmp_gen *ext,			      \
		u_int item_len, \
		const u_char *end_pointer, \
		uint32_t phase,\
		uint32_t doi0, \
		uint32_t proto0, int depth)

DECLARE_PRINTER(v1_sa);
DECLARE_PRINTER(v1_p);
DECLARE_PRINTER(v1_t);
DECLARE_PRINTER(v1_ke);
DECLARE_PRINTER(v1_id);
DECLARE_PRINTER(v1_cert);
DECLARE_PRINTER(v1_cr);
DECLARE_PRINTER(v1_sig);
DECLARE_PRINTER(v1_hash);
DECLARE_PRINTER(v1_nonce);
DECLARE_PRINTER(v1_n);
DECLARE_PRINTER(v1_d);
DECLARE_PRINTER(v1_vid);

DECLARE_PRINTER(v2_sa);
DECLARE_PRINTER(v2_ke);
DECLARE_PRINTER(v2_ID);
DECLARE_PRINTER(v2_cert);
DECLARE_PRINTER(v2_cr);
DECLARE_PRINTER(v2_auth);
DECLARE_PRINTER(v2_nonce);
DECLARE_PRINTER(v2_n);
DECLARE_PRINTER(v2_d);
DECLARE_PRINTER(v2_vid);
DECLARE_PRINTER(v2_TS);
DECLARE_PRINTER(v2_cp);
DECLARE_PRINTER(v2_eap);

static const u_char *ikev2_e_print(netdissect_options *ndo,
				   const struct isakmp *base,
				   u_char tpay,
				   const struct isakmp_gen *ext,
				   u_int item_len,
				   const u_char *end_pointer,
				   uint32_t phase,
				   uint32_t doi0,
				   uint32_t proto0, int depth);


static const u_char *ike_sub0_print(netdissect_options *ndo,u_char, const struct isakmp_gen *,
	const u_char *,	uint32_t, uint32_t, uint32_t, int);
static const u_char *ikev1_sub_print(netdissect_options *ndo,u_char, const struct isakmp_gen *,
	const u_char *, uint32_t, uint32_t, uint32_t, int);

static const u_char *ikev2_sub_print(netdissect_options *ndo,
				     const struct isakmp *base,
				     u_char np, const struct isakmp_gen *ext,
				     const u_char *ep, uint32_t phase,
				     uint32_t doi, uint32_t proto,
				     int depth);


static char *numstr(u_int);

static void
ikev1_print(netdissect_options *ndo,
	    const u_char *bp,  u_int length,
	    const u_char *bp2, const struct isakmp *base);

#define MAXINITIATORS	20
static int ninitiator = 0;
union inaddr_u {
	nd_ipv4 in4;
	nd_ipv6 in6;
};
static struct {
	cookie_t initiator;
	u_int version;
	union inaddr_u iaddr;
	union inaddr_u raddr;
} cookiecache[MAXINITIATORS];

/* protocol id */
static const char *protoidstr[] = {
	NULL, "isakmp", "ipsec-ah", "ipsec-esp", "ipcomp",
};

/* isakmp->np */
static const char *npstr[] = {
	"none", "sa", "p", "t", "ke", "id", "cert", "cr", "hash", /* 0 - 8 */
	"sig", "nonce", "n", "d", "vid",      /* 9 - 13 */
	"pay14", "pay15", "pay16", "pay17", "pay18", /* 14- 18 */
	"pay19", "pay20", "pay21", "pay22", "pay23", /* 19- 23 */
	"pay24", "pay25", "pay26", "pay27", "pay28", /* 24- 28 */
	"pay29", "pay30", "pay31", "pay32",          /* 29- 32 */
	"v2sa",  "v2ke",  "v2IDi", "v2IDr", "v2cert",/* 33- 37 */
	"v2cr",  "v2auth","v2nonce", "v2n",   "v2d",   /* 38- 42 */
	"v2vid", "v2TSi", "v2TSr", "v2e",   "v2cp",  /* 43- 47 */
	"v2eap",                                     /* 48 */

};

/* isakmp->np */
static const u_char *(*npfunc[])(netdissect_options *ndo, u_char tpay,
				 const struct isakmp_gen *ext,
				 u_int item_len,
				 const u_char *end_pointer,
				 uint32_t phase,
				 uint32_t doi0,
				 uint32_t proto0, int depth) = {
	NULL,
	ikev1_sa_print,
	ikev1_p_print,
	ikev1_t_print,
	ikev1_ke_print,
	ikev1_id_print,
	ikev1_cert_print,
	ikev1_cr_print,
	ikev1_hash_print,
	ikev1_sig_print,
	ikev1_nonce_print,
	ikev1_n_print,
	ikev1_d_print,
	ikev1_vid_print,                  /* 13 */
	NULL, NULL, NULL, NULL, NULL,     /* 14- 18 */
	NULL, NULL, NULL, NULL, NULL,     /* 19- 23 */
	NULL, NULL, NULL, NULL, NULL,     /* 24- 28 */
	NULL, NULL, NULL, NULL,           /* 29- 32 */
	ikev2_sa_print,                 /* 33 */
	ikev2_ke_print,                 /* 34 */
	ikev2_ID_print,                 /* 35 */
	ikev2_ID_print,                 /* 36 */
	ikev2_cert_print,               /* 37 */
	ikev2_cr_print,                 /* 38 */
	ikev2_auth_print,               /* 39 */
	ikev2_nonce_print,              /* 40 */
	ikev2_n_print,                  /* 41 */
	ikev2_d_print,                  /* 42 */
	ikev2_vid_print,                /* 43 */
	ikev2_TS_print,                 /* 44 */
	ikev2_TS_print,                 /* 45 */
	NULL, /* ikev2_e_print,*/       /* 46 - special */
	ikev2_cp_print,                 /* 47 */
	ikev2_eap_print,                /* 48 */
};

/* isakmp->etype */
static const char *etypestr[] = {
/* IKEv1 exchange types */
	"none", "base", "ident", "auth", "agg", "inf", NULL, NULL,  /* 0-7 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  /*  8-15 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  /* 16-23 */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,  /* 24-31 */
	"oakley-quick", "oakley-newgroup",               /* 32-33 */
/* IKEv2 exchange types */
	"ikev2_init", "ikev2_auth", "child_sa", "inf2"   /* 34-37 */
};

#define STR_OR_ID(x, tab) \
	(((x) < sizeof(tab)/sizeof(tab[0]) && tab[(x)])	? tab[(x)] : numstr(x))
#define PROTOIDSTR(x)	STR_OR_ID(x, protoidstr)
#define NPSTR(x)	STR_OR_ID(x, npstr)
#define ETYPESTR(x)	STR_OR_ID(x, etypestr)

#define CHECKLEN(p, np)							\
		if (ep < (const u_char *)(p)) {				\
			ND_PRINT(" [|%s]", NPSTR(np));		\
			goto done;					\
		}


#define NPFUNC(x) \
	(((x) < sizeof(npfunc)/sizeof(npfunc[0]) && npfunc[(x)]) \
		? npfunc[(x)] : NULL)

static int
iszero(netdissect_options *ndo, const u_char *p, size_t l)
{
	while (l != 0) {
		if (GET_U_1(p))
			return 0;
		p++;
		l--;
	}
	return 1;
}

/* find cookie from initiator cache */
static int
cookie_find(const cookie_t *in)
{
	int i;

	for (i = 0; i < MAXINITIATORS; i++) {
		if (memcmp(in, &cookiecache[i].initiator, sizeof(*in)) == 0)
			return i;
	}

	return -1;
}

/* record initiator */
static void
cookie_record(netdissect_options *ndo, const cookie_t *in, const u_char *bp2)
{
	int i;
	const struct ip *ip;
	const struct ip6_hdr *ip6;

	i = cookie_find(in);
	if (0 <= i) {
		ninitiator = (i + 1) % MAXINITIATORS;
		return;
	}

	ip = (const struct ip *)bp2;
	switch (IP_V(ip)) {
	case 4:
		cookiecache[ninitiator].version = 4;
		UNALIGNED_MEMCPY(&cookiecache[ninitiator].iaddr.in4,
				 ip->ip_src, sizeof(nd_ipv4));
		UNALIGNED_MEMCPY(&cookiecache[ninitiator].raddr.in4,
				 ip->ip_dst, sizeof(nd_ipv4));
		break;
	case 6:
		ip6 = (const struct ip6_hdr *)bp2;
		cookiecache[ninitiator].version = 6;
		UNALIGNED_MEMCPY(&cookiecache[ninitiator].iaddr.in6,
				 ip6->ip6_src, sizeof(nd_ipv6));
		UNALIGNED_MEMCPY(&cookiecache[ninitiator].raddr.in6,
				 ip6->ip6_dst, sizeof(nd_ipv6));
		break;
	default:
		return;
	}
	UNALIGNED_MEMCPY(&cookiecache[ninitiator].initiator, in, sizeof(*in));
	ninitiator = (ninitiator + 1) % MAXINITIATORS;
}

#define cookie_isinitiator(ndo, x, y)	cookie_sidecheck(ndo, (x), (y), 1)
#define cookie_isresponder(ndo, x, y)	cookie_sidecheck(ndo, (x), (y), 0)
static int
cookie_sidecheck(netdissect_options *ndo, int i, const u_char *bp2, int initiator)
{
	const struct ip *ip;
	const struct ip6_hdr *ip6;

	ip = (const struct ip *)bp2;
	switch (IP_V(ip)) {
	case 4:
		if (cookiecache[i].version != 4)
			return 0;
		if (initiator) {
			if (UNALIGNED_MEMCMP(ip->ip_src, &cookiecache[i].iaddr.in4, sizeof(nd_ipv4)) == 0)
				return 1;
		} else {
			if (UNALIGNED_MEMCMP(ip->ip_src, &cookiecache[i].raddr.in4, sizeof(nd_ipv4)) == 0)
				return 1;
		}
		break;
	case 6:
		if (cookiecache[i].version != 6)
			return 0;
		ip6 = (const struct ip6_hdr *)bp2;
		if (initiator) {
			if (UNALIGNED_MEMCMP(ip6->ip6_src, &cookiecache[i].iaddr.in6, sizeof(nd_ipv6)) == 0)
				return 1;
		} else {
			if (UNALIGNED_MEMCMP(ip6->ip6_src, &cookiecache[i].raddr.in6, sizeof(nd_ipv6)) == 0)
				return 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void
hexprint(netdissect_options *ndo, const uint8_t *loc, size_t len)
{
	const uint8_t *p;
	size_t i;

	p = loc;
	for (i = 0; i < len; i++)
		ND_PRINT("%02x", p[i] & 0xff);
}

static int
rawprint(netdissect_options *ndo, const uint8_t *loc, size_t len)
{
	ND_TCHECK_LEN(loc, len);

	hexprint(ndo, loc, len);
	return 1;
trunc:
	return 0;
}


/*
 * returns false if we run out of data buffer
 */
static int ike_show_somedata(netdissect_options *ndo,
			     const u_char *cp, const u_char *ep)
{
	/* there is too much data, just show some of it */
	const u_char *end = ep - 20;
	size_t  elen = 20;
	size_t  len = ep - cp;
	if(len > 10) {
		len = 10;
	}

	/* really shouldn't happen because of above */
	if(end < cp + len) {
		end = cp+len;
		elen = ep - end;
	}

	ND_PRINT(" data=(");
	if(!rawprint(ndo, (const uint8_t *)(cp), len)) goto trunc;
	ND_PRINT("...");
	if(elen) {
		if(!rawprint(ndo, (const uint8_t *)(end), elen)) goto trunc;
	}
	ND_PRINT(")");
	return 1;

trunc:
	return 0;
}

struct attrmap {
	const char *type;
	u_int nvalue;
	const char *value[30];	/*XXX*/
};

static const u_char *
ikev1_attrmap_print(netdissect_options *ndo,
		    const u_char *p, const u_char *ep2,
		    const struct attrmap *map, size_t nmap)
{
	u_int totlen;
	uint32_t t, v;

	if (GET_U_1(p) & 0x80)
		totlen = 4;
	else {
		totlen = 4 + GET_BE_U_2(p + 2);
	}
	if (ep2 < p + totlen) {
		ND_PRINT("[|attr]");
		return ep2 + 1;
	}

	ND_PRINT("(");
	t = GET_BE_U_2(p) & 0x7fff;
	if (map && t < nmap && map[t].type)
		ND_PRINT("type=%s ", map[t].type);
	else
		ND_PRINT("type=#%u ", t);
	if (GET_U_1(p) & 0x80) {
		ND_PRINT("value=");
		v = GET_BE_U_2(p + 2);
		if (map && t < nmap && v < map[t].nvalue && map[t].value[v])
			ND_PRINT("%s", map[t].value[v]);
		else {
			if (!rawprint(ndo, (const uint8_t *)(p + 2), 2)) {
				ND_PRINT(")");
				goto trunc;
			}
		}
	} else {
		ND_PRINT("len=%u value=", totlen - 4);
		if (!rawprint(ndo, (const uint8_t *)(p + 4), totlen - 4)) {
			ND_PRINT(")");
			goto trunc;
		}
	}
	ND_PRINT(")");
	return p + totlen;

trunc:
	return NULL;
}

static const u_char *
ikev1_attr_print(netdissect_options *ndo, const u_char *p, const u_char *ep2)
{
	u_int totlen;
	uint32_t t;

	if (GET_U_1(p) & 0x80)
		totlen = 4;
	else {
		totlen = 4 + GET_BE_U_2(p + 2);
	}
	if (ep2 < p + totlen) {
		ND_PRINT("[|attr]");
		return ep2 + 1;
	}

	ND_PRINT("(");
	t = GET_BE_U_2(p) & 0x7fff;
	ND_PRINT("type=#%u ", t);
	if (GET_U_1(p) & 0x80) {
		ND_PRINT("value=");
		t = GET_U_1(p + 2);
		if (!rawprint(ndo, (const uint8_t *)(p + 2), 2)) {
			ND_PRINT(")");
			goto trunc;
		}
	} else {
		ND_PRINT("len=%u value=", totlen - 4);
		if (!rawprint(ndo, (const uint8_t *)(p + 4), totlen - 4)) {
			ND_PRINT(")");
			goto trunc;
		}
	}
	ND_PRINT(")");
	return p + totlen;

trunc:
	return NULL;
}

static const u_char *
ikev1_sa_print(netdissect_options *ndo, u_char tpay _U_,
	       const struct isakmp_gen *ext,
		u_int item_len _U_,
		const u_char *ep, uint32_t phase, uint32_t doi0 _U_,
		uint32_t proto0, int depth)
{
	const struct ikev1_pl_sa *p;
	uint32_t doi, sit, ident;
	const u_char *cp, *np;
	int t;

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_SA));

	p = (const struct ikev1_pl_sa *)ext;
	ND_TCHECK_SIZE(p);
	doi = GET_BE_U_4(p->doi);
	sit = GET_BE_U_4(p->sit);
	if (doi != 1) {
		ND_PRINT(" doi=%u", doi);
		ND_PRINT(" situation=%u", sit);
		return (const u_char *)(p + 1);
	}

	ND_PRINT(" doi=ipsec");
	ND_PRINT(" situation=");
	t = 0;
	if (sit & 0x01) {
		ND_PRINT("identity");
		t++;
	}
	if (sit & 0x02) {
		ND_PRINT("%ssecrecy", t ? "+" : "");
		t++;
	}
	if (sit & 0x04)
		ND_PRINT("%sintegrity", t ? "+" : "");

	np = (const u_char *)ext + sizeof(struct ikev1_pl_sa);
	if (sit != 0x01) {
		ident = GET_BE_U_4(ext + 1);
		ND_PRINT(" ident=%u", ident);
		np += sizeof(ident);
	}

	ext = (const struct isakmp_gen *)np;
	ND_TCHECK_SIZE(ext);

	cp = ikev1_sub_print(ndo, ISAKMP_NPTYPE_P, ext, ep, phase, doi, proto0,
		depth);

	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_SA));
	return NULL;
}

static const u_char *
ikev1_p_print(netdissect_options *ndo, u_char tpay _U_,
	      const struct isakmp_gen *ext, u_int item_len _U_,
	       const u_char *ep, uint32_t phase, uint32_t doi0,
	       uint32_t proto0 _U_, int depth)
{
	const struct ikev1_pl_p *p;
	const u_char *cp;
	uint8_t spi_size;

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_P));

	p = (const struct ikev1_pl_p *)ext;
	ND_TCHECK_SIZE(p);
	ND_PRINT(" #%u protoid=%s transform=%u",
		  GET_U_1(p->p_no), PROTOIDSTR(GET_U_1(p->prot_id)),
		  GET_U_1(p->num_t));
	spi_size = GET_U_1(p->spi_size);
	if (spi_size) {
		ND_PRINT(" spi=");
		if (!rawprint(ndo, (const uint8_t *)(p + 1), spi_size))
			goto trunc;
	}

	ext = (const struct isakmp_gen *)((const u_char *)(p + 1) + spi_size);
	ND_TCHECK_SIZE(ext);

	cp = ikev1_sub_print(ndo, ISAKMP_NPTYPE_T, ext, ep, phase, doi0,
			     GET_U_1(p->prot_id), depth);

	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_P));
	return NULL;
}

static const char *ikev1_p_map[] = {
	NULL, "ike",
};

static const char *ikev2_t_type_map[]={
	NULL, "encr", "prf", "integ", "dh", "esn"
};

static const char *ah_p_map[] = {
	NULL, "(reserved)", "md5", "sha", "1des",
	"sha2-256", "sha2-384", "sha2-512",
};

static const char *prf_p_map[] = {
	NULL, "hmac-md5", "hmac-sha", "hmac-tiger",
	"aes128_xcbc"
};

static const char *integ_p_map[] = {
	NULL, "hmac-md5", "hmac-sha", "dec-mac",
	"kpdk-md5", "aes-xcbc"
};

static const char *esn_p_map[] = {
	"no-esn", "esn"
};

static const char *dh_p_map[] = {
	NULL, "modp768",
	"modp1024",    /* group 2 */
	"EC2N 2^155",  /* group 3 */
	"EC2N 2^185",  /* group 4 */
	"modp1536",    /* group 5 */
	"iana-grp06", "iana-grp07", /* reserved */
	"iana-grp08", "iana-grp09",
	"iana-grp10", "iana-grp11",
	"iana-grp12", "iana-grp13",
	"modp2048",    /* group 14 */
	"modp3072",    /* group 15 */
	"modp4096",    /* group 16 */
	"modp6144",    /* group 17 */
	"modp8192",    /* group 18 */
};

static const char *esp_p_map[] = {
	NULL, "1des-iv64", "1des", "3des", "rc5", "idea", "cast",
	"blowfish", "3idea", "1des-iv32", "rc4", "null", "aes"
};

static const char *ipcomp_p_map[] = {
	NULL, "oui", "deflate", "lzs",
};

static const struct attrmap ipsec_t_map[] = {
	{ NULL,	0, { NULL } },
	{ "lifetype", 3, { NULL, "sec", "kb", }, },
	{ "life", 0, { NULL } },
	{ "group desc", 18,	{ NULL, "modp768",
				  "modp1024",    /* group 2 */
				  "EC2N 2^155",  /* group 3 */
				  "EC2N 2^185",  /* group 4 */
				  "modp1536",    /* group 5 */
				  "iana-grp06", "iana-grp07", /* reserved */
				  "iana-grp08", "iana-grp09",
				  "iana-grp10", "iana-grp11",
				  "iana-grp12", "iana-grp13",
				  "modp2048",    /* group 14 */
				  "modp3072",    /* group 15 */
				  "modp4096",    /* group 16 */
				  "modp6144",    /* group 17 */
				  "modp8192",    /* group 18 */
		}, },
	{ "enc mode", 3, { NULL, "tunnel", "transport", }, },
	{ "auth", 5, { NULL, "hmac-md5", "hmac-sha1", "1des-mac", "keyed", }, },
	{ "keylen", 0, { NULL } },
	{ "rounds", 0, { NULL } },
	{ "dictsize", 0, { NULL } },
	{ "privalg", 0, { NULL } },
};

static const struct attrmap encr_t_map[] = {
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 0, 1 */
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 2, 3 */
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 4, 5 */
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 6, 7 */
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 8, 9 */
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 10,11*/
	{ NULL,	0, { NULL } },	{ NULL,	0, { NULL } },  /* 12,13*/
	{ "keylen", 14, { NULL }},
};

static const struct attrmap oakley_t_map[] = {
	{ NULL,	0, { NULL } },
	{ "enc", 8,	{ NULL, "1des", "idea", "blowfish", "rc5",
			  "3des", "cast", "aes", }, },
	{ "hash", 7,	{ NULL, "md5", "sha1", "tiger",
			  "sha2-256", "sha2-384", "sha2-512", }, },
	{ "auth", 6,	{ NULL, "preshared", "dss", "rsa sig", "rsa enc",
			  "rsa enc revised", }, },
	{ "group desc", 18,	{ NULL, "modp768",
				  "modp1024",    /* group 2 */
				  "EC2N 2^155",  /* group 3 */
				  "EC2N 2^185",  /* group 4 */
				  "modp1536",    /* group 5 */
				  "iana-grp06", "iana-grp07", /* reserved */
				  "iana-grp08", "iana-grp09",
				  "iana-grp10", "iana-grp11",
				  "iana-grp12", "iana-grp13",
				  "modp2048",    /* group 14 */
				  "modp3072",    /* group 15 */
				  "modp4096",    /* group 16 */
				  "modp6144",    /* group 17 */
				  "modp8192",    /* group 18 */
		}, },
	{ "group type", 4,	{ NULL, "MODP", "ECP", "EC2N", }, },
	{ "group prime", 0, { NULL } },
	{ "group gen1", 0, { NULL } },
	{ "group gen2", 0, { NULL } },
	{ "group curve A", 0, { NULL } },
	{ "group curve B", 0, { NULL } },
	{ "lifetype", 3,	{ NULL, "sec", "kb", }, },
	{ "lifeduration", 0, { NULL } },
	{ "prf", 0, { NULL } },
	{ "keylen", 0, { NULL } },
	{ "field", 0, { NULL } },
	{ "order", 0, { NULL } },
};

static const u_char *
ikev1_t_print(netdissect_options *ndo, u_char tpay _U_,
	      const struct isakmp_gen *ext, u_int item_len,
	      const u_char *ep, uint32_t phase _U_, uint32_t doi _U_,
	      uint32_t proto, int depth _U_)
{
	const struct ikev1_pl_t *p;
	const u_char *cp;
	const char *idstr;
	const struct attrmap *map;
	size_t nmap;
	const u_char *ep2;

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_T));

	p = (const struct ikev1_pl_t *)ext;
	ND_TCHECK_SIZE(p);

	switch (proto) {
	case 1:
		idstr = STR_OR_ID(GET_U_1(p->t_id), ikev1_p_map);
		map = oakley_t_map;
		nmap = sizeof(oakley_t_map)/sizeof(oakley_t_map[0]);
		break;
	case 2:
		idstr = STR_OR_ID(GET_U_1(p->t_id), ah_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	case 3:
		idstr = STR_OR_ID(GET_U_1(p->t_id), esp_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	case 4:
		idstr = STR_OR_ID(GET_U_1(p->t_id), ipcomp_p_map);
		map = ipsec_t_map;
		nmap = sizeof(ipsec_t_map)/sizeof(ipsec_t_map[0]);
		break;
	default:
		idstr = NULL;
		map = NULL;
		nmap = 0;
		break;
	}

	if (idstr)
		ND_PRINT(" #%u id=%s ", GET_U_1(p->t_no), idstr);
	else
		ND_PRINT(" #%u id=%u ", GET_U_1(p->t_no), GET_U_1(p->t_id));
	cp = (const u_char *)(p + 1);
	ep2 = (const u_char *)p + item_len;
	while (cp < ep && cp < ep2) {
		if (map && nmap)
			cp = ikev1_attrmap_print(ndo, cp, ep2, map, nmap);
		else
			cp = ikev1_attr_print(ndo, cp, ep2);
		if (cp == NULL)
			goto trunc;
	}
	if (ep < ep2)
		ND_PRINT("...");
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_T));
	return NULL;
}

static const u_char *
ikev1_ke_print(netdissect_options *ndo, u_char tpay _U_,
	       const struct isakmp_gen *ext, u_int item_len,
	       const u_char *ep _U_, uint32_t phase _U_, uint32_t doi _U_,
	       uint32_t proto _U_, int depth _U_)
{
	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_KE));

	ND_TCHECK_SIZE(ext);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" key len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && item_len > 4) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_KE));
	return NULL;
}

static const u_char *
ikev1_id_print(netdissect_options *ndo, u_char tpay _U_,
	       const struct isakmp_gen *ext, u_int item_len,
	       const u_char *ep _U_, uint32_t phase, uint32_t doi _U_,
	       uint32_t proto _U_, int depth _U_)
{
#define USE_IPSECDOI_IN_PHASE1	1
	const struct ikev1_pl_id *p;
	static const char *idtypestr[] = {
		"IPv4", "IPv4net", "IPv6", "IPv6net",
	};
	static const char *ipsecidtypestr[] = {
		NULL, "IPv4", "FQDN", "user FQDN", "IPv4net", "IPv6",
		"IPv6net", "IPv4range", "IPv6range", "ASN1 DN", "ASN1 GN",
		"keyid",
	};
	u_int len;
	const u_char *data;

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_ID));

	p = (const struct ikev1_pl_id *)ext;
	ND_TCHECK_SIZE(p);
	if (sizeof(*p) < item_len) {
		data = (const u_char *)(p + 1);
		len = item_len - sizeof(*p);
	} else {
		data = NULL;
		len = 0;
	}

#if 0 /*debug*/
	ND_PRINT(" [phase=%u doi=%u proto=%u]", phase, doi, proto);
#endif
	switch (phase) {
#ifndef USE_IPSECDOI_IN_PHASE1
	case 1:
#endif
	default:
		ND_PRINT(" idtype=%s",
			 STR_OR_ID(GET_U_1(p->d.id_type), idtypestr));
		ND_PRINT(" doi_data=%u",
			  GET_BE_U_4(p->d.doi_data) & 0xffffff);
		break;

#ifdef USE_IPSECDOI_IN_PHASE1
	case 1:
#endif
	case 2:
	    {
		const struct ipsecdoi_id *doi_p;
		const char *p_name;
		uint8_t type, proto_id;

		doi_p = (const struct ipsecdoi_id *)ext;
		ND_TCHECK_SIZE(doi_p);
		type = GET_U_1(doi_p->type);
		ND_PRINT(" idtype=%s", STR_OR_ID(type, ipsecidtypestr));
		/* A protocol ID of 0 DOES NOT mean IPPROTO_IP! */
		proto_id = GET_U_1(doi_p->proto_id);
		if (!ndo->ndo_nflag && proto_id && (p_name = netdb_protoname(proto_id)) != NULL)
			ND_PRINT(" protoid=%s", p_name);
		else
			ND_PRINT(" protoid=%u", proto_id);
		ND_PRINT(" port=%u", GET_BE_U_2(doi_p->port));
		if (!len)
			break;
		if (data == NULL)
			goto trunc;
		ND_TCHECK_LEN(data, len);
		switch (type) {
		case IPSECDOI_ID_IPV4_ADDR:
			if (len < 4)
				ND_PRINT(" len=%u [bad: < 4]", len);
			else
				ND_PRINT(" len=%u %s", len, GET_IPADDR_STRING(data));
			len = 0;
			break;
		case IPSECDOI_ID_FQDN:
		case IPSECDOI_ID_USER_FQDN:
		    {
			u_int i;
			ND_PRINT(" len=%u ", len);
			for (i = 0; i < len; i++)
				fn_print_char(ndo, GET_U_1(data + i));
			len = 0;
			break;
		    }
		case IPSECDOI_ID_IPV4_ADDR_SUBNET:
		    {
			const u_char *mask;
			if (len < 8)
				ND_PRINT(" len=%u [bad: < 8]", len);
			else {
				mask = data + sizeof(nd_ipv4);
				ND_PRINT(" len=%u %s/%u.%u.%u.%u", len,
					  GET_IPADDR_STRING(data),
					  GET_U_1(mask), GET_U_1(mask + 1),
					  GET_U_1(mask + 2),
					  GET_U_1(mask + 3));
			}
			len = 0;
			break;
		    }
		case IPSECDOI_ID_IPV6_ADDR:
			if (len < 16)
				ND_PRINT(" len=%u [bad: < 16]", len);
			else
				ND_PRINT(" len=%u %s", len, GET_IP6ADDR_STRING(data));
			len = 0;
			break;
		case IPSECDOI_ID_IPV6_ADDR_SUBNET:
		    {
			const u_char *mask;
			if (len < 32)
				ND_PRINT(" len=%u [bad: < 32]", len);
			else {
				mask = (const u_char *)(data + sizeof(nd_ipv6));
				/*XXX*/
				ND_PRINT(" len=%u %s/0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", len,
					  GET_IP6ADDR_STRING(data),
					  GET_U_1(mask), GET_U_1(mask + 1),
					  GET_U_1(mask + 2),
					  GET_U_1(mask + 3),
					  GET_U_1(mask + 4),
					  GET_U_1(mask + 5),
					  GET_U_1(mask + 6),
					  GET_U_1(mask + 7),
					  GET_U_1(mask + 8),
					  GET_U_1(mask + 9),
					  GET_U_1(mask + 10),
					  GET_U_1(mask + 11),
					  GET_U_1(mask + 12),
					  GET_U_1(mask + 13),
					  GET_U_1(mask + 14),
					  GET_U_1(mask + 15));
			}
			len = 0;
			break;
		    }
		case IPSECDOI_ID_IPV4_ADDR_RANGE:
			if (len < 8)
				ND_PRINT(" len=%u [bad: < 8]", len);
			else {
				ND_PRINT(" len=%u %s-%s", len,
					  GET_IPADDR_STRING(data),
					  GET_IPADDR_STRING(data + sizeof(nd_ipv4)));
			}
			len = 0;
			break;
		case IPSECDOI_ID_IPV6_ADDR_RANGE:
			if (len < 32)
				ND_PRINT(" len=%u [bad: < 32]", len);
			else {
				ND_PRINT(" len=%u %s-%s", len,
					  GET_IP6ADDR_STRING(data),
					  GET_IP6ADDR_STRING(data + sizeof(nd_ipv6)));
			}
			len = 0;
			break;
		case IPSECDOI_ID_DER_ASN1_DN:
		case IPSECDOI_ID_DER_ASN1_GN:
		case IPSECDOI_ID_KEY_ID:
			break;
		}
		break;
	    }
	}
	if (data && len) {
		ND_PRINT(" len=%u", len);
		if (2 < ndo->ndo_vflag) {
			ND_PRINT(" ");
			if (!rawprint(ndo, (const uint8_t *)data, len))
				goto trunc;
		}
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_ID));
	return NULL;
}

static const u_char *
ikev1_cert_print(netdissect_options *ndo, u_char tpay _U_,
		 const struct isakmp_gen *ext, u_int item_len,
		 const u_char *ep _U_, uint32_t phase _U_,
		 uint32_t doi0 _U_,
		 uint32_t proto0 _U_, int depth _U_)
{
	const struct ikev1_pl_cert *p;
	static const char *certstr[] = {
		"none",	"pkcs7", "pgp", "dns",
		"x509sign", "x509ke", "kerberos", "crl",
		"arl", "spki", "x509attr",
	};

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_CERT));

	p = (const struct ikev1_pl_cert *)ext;
	ND_TCHECK_SIZE(p);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	ND_PRINT(" type=%s", STR_OR_ID(GET_U_1(p->encode), certstr));
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_CERT));
	return NULL;
}

static const u_char *
ikev1_cr_print(netdissect_options *ndo, u_char tpay _U_,
	       const struct isakmp_gen *ext, u_int item_len,
	       const u_char *ep _U_, uint32_t phase _U_, uint32_t doi0 _U_,
	       uint32_t proto0 _U_, int depth _U_)
{
	const struct ikev1_pl_cert *p;
	static const char *certstr[] = {
		"none",	"pkcs7", "pgp", "dns",
		"x509sign", "x509ke", "kerberos", "crl",
		"arl", "spki", "x509attr",
	};

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_CR));

	p = (const struct ikev1_pl_cert *)ext;
	ND_TCHECK_SIZE(p);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	ND_PRINT(" type=%s", STR_OR_ID(GET_U_1(p->encode), certstr));
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_CR));
	return NULL;
}

static const u_char *
ikev1_hash_print(netdissect_options *ndo, u_char tpay _U_,
		 const struct isakmp_gen *ext, u_int item_len,
		 const u_char *ep _U_, uint32_t phase _U_, uint32_t doi _U_,
		 uint32_t proto _U_, int depth _U_)
{
	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_HASH));

	ND_TCHECK_SIZE(ext);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_HASH));
	return NULL;
}

static const u_char *
ikev1_sig_print(netdissect_options *ndo, u_char tpay _U_,
		const struct isakmp_gen *ext, u_int item_len,
		const u_char *ep _U_, uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_SIG));

	ND_TCHECK_SIZE(ext);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_SIG));
	return NULL;
}

static const u_char *
ikev1_nonce_print(netdissect_options *ndo, u_char tpay _U_,
		  const struct isakmp_gen *ext,
		  u_int item_len,
		  const u_char *ep,
		  uint32_t phase _U_, uint32_t doi _U_,
		  uint32_t proto _U_, int depth _U_)
{
	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_NONCE));

	ND_TCHECK_SIZE(ext);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" n len=%u", item_len - 4);
	if (item_len > 4) {
		if (ndo->ndo_vflag > 2) {
			ND_PRINT(" ");
			if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
				goto trunc;
		} else if (ndo->ndo_vflag > 1) {
			ND_PRINT(" ");
			if (!ike_show_somedata(ndo, (const u_char *)(ext + 1), ep))
				goto trunc;
		}
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_NONCE));
	return NULL;
}

static const u_char *
ikev1_n_print(netdissect_options *ndo, u_char tpay _U_,
	      const struct isakmp_gen *ext, u_int item_len,
	      const u_char *ep, uint32_t phase _U_, uint32_t doi0 _U_,
	      uint32_t proto0 _U_, int depth _U_)
{
	const struct ikev1_pl_n *p;
	const u_char *cp;
	const u_char *ep2;
	uint32_t doi;
	uint32_t proto;
	uint16_t type;
	uint8_t spi_size;
	static const char *notify_error_str[] = {
		NULL,				"INVALID-PAYLOAD-TYPE",
		"DOI-NOT-SUPPORTED",		"SITUATION-NOT-SUPPORTED",
		"INVALID-COOKIE",		"INVALID-MAJOR-VERSION",
		"INVALID-MINOR-VERSION",	"INVALID-EXCHANGE-TYPE",
		"INVALID-FLAGS",		"INVALID-MESSAGE-ID",
		"INVALID-PROTOCOL-ID",		"INVALID-SPI",
		"INVALID-TRANSFORM-ID",		"ATTRIBUTES-NOT-SUPPORTED",
		"NO-PROPOSAL-CHOSEN",		"BAD-PROPOSAL-SYNTAX",
		"PAYLOAD-MALFORMED",		"INVALID-KEY-INFORMATION",
		"INVALID-ID-INFORMATION",	"INVALID-CERT-ENCODING",
		"INVALID-CERTIFICATE",		"CERT-TYPE-UNSUPPORTED",
		"INVALID-CERT-AUTHORITY",	"INVALID-HASH-INFORMATION",
		"AUTHENTICATION-FAILED",	"INVALID-SIGNATURE",
		"ADDRESS-NOTIFICATION",		"NOTIFY-SA-LIFETIME",
		"CERTIFICATE-UNAVAILABLE",	"UNSUPPORTED-EXCHANGE-TYPE",
		"UNEQUAL-PAYLOAD-LENGTHS",
	};
	static const char *ipsec_notify_error_str[] = {
		"RESERVED",
	};
	static const char *notify_status_str[] = {
		"CONNECTED",
	};
	static const char *ipsec_notify_status_str[] = {
		"RESPONDER-LIFETIME",		"REPLAY-STATUS",
		"INITIAL-CONTACT",
	};
/* NOTE: these macro must be called with x in proper range */

/* 0 - 8191 */
#define NOTIFY_ERROR_STR(x) \
	STR_OR_ID((x), notify_error_str)

/* 8192 - 16383 */
#define IPSEC_NOTIFY_ERROR_STR(x) \
	STR_OR_ID((u_int)((x) - 8192), ipsec_notify_error_str)

/* 16384 - 24575 */
#define NOTIFY_STATUS_STR(x) \
	STR_OR_ID((u_int)((x) - 16384), notify_status_str)

/* 24576 - 32767 */
#define IPSEC_NOTIFY_STATUS_STR(x) \
	STR_OR_ID((u_int)((x) - 24576), ipsec_notify_status_str)

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_N));

	p = (const struct ikev1_pl_n *)ext;
	ND_TCHECK_SIZE(p);
	doi = GET_BE_U_4(p->doi);
	proto = GET_U_1(p->prot_id);
	if (doi != 1) {
		ND_PRINT(" doi=%u", doi);
		ND_PRINT(" proto=%u", proto);
		type = GET_BE_U_2(p->type);
		if (type < 8192)
			ND_PRINT(" type=%s", NOTIFY_ERROR_STR(type));
		else if (type < 16384)
			ND_PRINT(" type=%s", numstr(type));
		else if (type < 24576)
			ND_PRINT(" type=%s", NOTIFY_STATUS_STR(type));
		else
			ND_PRINT(" type=%s", numstr(type));
		spi_size = GET_U_1(p->spi_size);
		if (spi_size) {
			ND_PRINT(" spi=");
			if (!rawprint(ndo, (const uint8_t *)(p + 1), spi_size))
				goto trunc;
		}
		return (const u_char *)(p + 1) + spi_size;
	}

	ND_PRINT(" doi=ipsec");
	ND_PRINT(" proto=%s", PROTOIDSTR(proto));
	type = GET_BE_U_2(p->type);
	if (type < 8192)
		ND_PRINT(" type=%s", NOTIFY_ERROR_STR(type));
	else if (type < 16384)
		ND_PRINT(" type=%s", IPSEC_NOTIFY_ERROR_STR(type));
	else if (type < 24576)
		ND_PRINT(" type=%s", NOTIFY_STATUS_STR(type));
	else if (type < 32768)
		ND_PRINT(" type=%s", IPSEC_NOTIFY_STATUS_STR(type));
	else
		ND_PRINT(" type=%s", numstr(type));
	spi_size = GET_U_1(p->spi_size);
	if (spi_size) {
		ND_PRINT(" spi=");
		if (!rawprint(ndo, (const uint8_t *)(p + 1), spi_size))
			goto trunc;
	}

	cp = (const u_char *)(p + 1) + spi_size;
	ep2 = (const u_char *)p + item_len;

	if (cp < ep) {
		switch (type) {
		case IPSECDOI_NTYPE_RESPONDER_LIFETIME:
		    {
			const struct attrmap *map = oakley_t_map;
			size_t nmap = sizeof(oakley_t_map)/sizeof(oakley_t_map[0]);
			ND_PRINT(" attrs=(");
			while (cp < ep && cp < ep2) {
				cp = ikev1_attrmap_print(ndo, cp, ep2, map, nmap);
				if (cp == NULL) {
					ND_PRINT(")");
					goto trunc;
				}
			}
			ND_PRINT(")");
			break;
		    }
		case IPSECDOI_NTYPE_REPLAY_STATUS:
			ND_PRINT(" status=(");
			ND_PRINT("replay detection %sabled",
				  GET_BE_U_4(cp) ? "en" : "dis");
			ND_PRINT(")");
			break;
		default:
			/*
			 * XXX - fill in more types here; see, for example,
			 * draft-ietf-ipsec-notifymsg-04.
			 */
			if (ndo->ndo_vflag > 3) {
				ND_PRINT(" data=(");
				if (!rawprint(ndo, (const uint8_t *)(cp), ep - cp))
					goto trunc;
				ND_PRINT(")");
			} else {
				if (!ike_show_somedata(ndo, cp, ep))
					goto trunc;
			}
			break;
		}
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_N));
	return NULL;
}

static const u_char *
ikev1_d_print(netdissect_options *ndo, u_char tpay _U_,
	      const struct isakmp_gen *ext, u_int item_len _U_,
	      const u_char *ep _U_, uint32_t phase _U_, uint32_t doi0 _U_,
	      uint32_t proto0 _U_, int depth _U_)
{
	const struct ikev1_pl_d *p;
	const uint8_t *q;
	uint32_t doi;
	uint32_t proto;
	uint8_t spi_size;
	uint16_t num_spi;
	u_int i;

	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_D));

	p = (const struct ikev1_pl_d *)ext;
	ND_TCHECK_SIZE(p);
	doi = GET_BE_U_4(p->doi);
	proto = GET_U_1(p->prot_id);
	if (doi != 1) {
		ND_PRINT(" doi=%u", doi);
		ND_PRINT(" proto=%u", proto);
	} else {
		ND_PRINT(" doi=ipsec");
		ND_PRINT(" proto=%s", PROTOIDSTR(proto));
	}
	spi_size = GET_U_1(p->spi_size);
	ND_PRINT(" spilen=%u", spi_size);
	num_spi = GET_BE_U_2(p->num_spi);
	ND_PRINT(" nspi=%u", num_spi);
	ND_PRINT(" spi=");
	q = (const uint8_t *)(p + 1);
	for (i = 0; i < num_spi; i++) {
		if (i != 0)
			ND_PRINT(",");
		if (!rawprint(ndo, (const uint8_t *)q, spi_size))
			goto trunc;
		q += spi_size;
	}
	return q;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_D));
	return NULL;
}

static const u_char *
ikev1_vid_print(netdissect_options *ndo, u_char tpay _U_,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	ND_PRINT("%s:", NPSTR(ISAKMP_NPTYPE_VID));

	ND_TCHECK_SIZE(ext);
	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_VID));
	return NULL;
}

/************************************************************/
/*                                                          */
/*              IKE v2 - rfc4306 - dissector                */
/*                                                          */
/************************************************************/

static void
ikev2_pay_print(netdissect_options *ndo, const char *payname, uint8_t critical)
{
	ND_PRINT("%s%s:", payname, critical&0x80 ? "[C]" : "");
}

static const u_char *
ikev2_gen_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext, u_int item_len)
{
	const struct isakmp_gen *p = (const struct isakmp_gen *)ext;

	ND_TCHECK_SIZE(ext);
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(p->critical));

	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_t_print(netdissect_options *ndo, int tcount,
	      const struct isakmp_gen *ext, u_int item_len,
	      const u_char *ep)
{
	const struct ikev2_t *p;
	uint16_t  t_id;
	uint8_t t_type;
	const u_char *cp;
	const char *idstr;
	const struct attrmap *map;
	size_t nmap;
	const u_char *ep2;

	p = (const struct ikev2_t *)ext;
	ND_TCHECK_SIZE(p);
	ikev2_pay_print(ndo, NPSTR(ISAKMP_NPTYPE_T), GET_U_1(p->h.critical));

	t_id = GET_BE_U_2(p->t_id);

	map = NULL;
	nmap = 0;

	t_type = GET_U_1(p->t_type);
	switch (t_type) {
	case IV2_T_ENCR:
		idstr = STR_OR_ID(t_id, esp_p_map);
		map = encr_t_map;
		nmap = sizeof(encr_t_map)/sizeof(encr_t_map[0]);
		break;

	case IV2_T_PRF:
		idstr = STR_OR_ID(t_id, prf_p_map);
		break;

	case IV2_T_INTEG:
		idstr = STR_OR_ID(t_id, integ_p_map);
		break;

	case IV2_T_DH:
		idstr = STR_OR_ID(t_id, dh_p_map);
		break;

	case IV2_T_ESN:
		idstr = STR_OR_ID(t_id, esn_p_map);
		break;

	default:
		idstr = NULL;
		break;
	}

	if (idstr)
		ND_PRINT(" #%u type=%s id=%s ", tcount,
			  STR_OR_ID(t_type, ikev2_t_type_map),
			  idstr);
	else
		ND_PRINT(" #%u type=%s id=%u ", tcount,
			  STR_OR_ID(t_type, ikev2_t_type_map),
			  t_id);
	cp = (const u_char *)(p + 1);
	ep2 = (const u_char *)p + item_len;
	while (cp < ep && cp < ep2) {
		if (map && nmap) {
			cp = ikev1_attrmap_print(ndo, cp, ep2, map, nmap);
		} else
			cp = ikev1_attr_print(ndo, cp, ep2);
		if (cp == NULL)
			goto trunc;
	}
	if (ep < ep2)
		ND_PRINT("...");
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_T));
	return NULL;
}

static const u_char *
ikev2_p_print(netdissect_options *ndo, u_char tpay _U_, int pcount _U_,
	      const struct isakmp_gen *ext, u_int oprop_length,
	      const u_char *ep, int depth)
{
	const struct ikev2_p *p;
	u_int prop_length;
	uint8_t spi_size;
	const u_char *cp;
	int i;
	int tcount;
	u_char np;
	u_int item_len;

	p = (const struct ikev2_p *)ext;
	ND_TCHECK_SIZE(p);

	ikev2_pay_print(ndo, NPSTR(ISAKMP_NPTYPE_P), GET_U_1(p->h.critical));

	/*
	 * ikev2_sa_print() guarantees that this is >= 4.
	 */
	prop_length = oprop_length - 4;
	ND_PRINT(" #%u protoid=%s transform=%u len=%u",
		  GET_U_1(p->p_no),  PROTOIDSTR(GET_U_1(p->prot_id)),
		  GET_U_1(p->num_t), oprop_length);
	cp = (const u_char *)(p + 1);

	spi_size = GET_U_1(p->spi_size);
	if (spi_size) {
		if (prop_length < spi_size)
			goto toolong;
		ND_PRINT(" spi=");
		if (!rawprint(ndo, (const uint8_t *)cp, spi_size))
			goto trunc;
		cp += spi_size;
		prop_length -= spi_size;
	}

	/*
	 * Print the transforms.
	 */
	tcount = 0;
	for (np = ISAKMP_NPTYPE_T; np != 0; np = GET_U_1(ext->np)) {
		tcount++;
		ext = (const struct isakmp_gen *)cp;
		if (prop_length < sizeof(*ext))
			goto toolong;
		ND_TCHECK_SIZE(ext);

		/*
		 * Since we can't have a payload length of less than 4 bytes,
		 * we need to bail out here if the generic header is nonsensical
		 * or truncated, otherwise we could loop forever processing
		 * zero-length items or otherwise misdissect the packet.
		 */
		item_len = GET_BE_U_2(ext->len);
		if (item_len <= 4)
			goto trunc;

		if (prop_length < item_len)
			goto toolong;
		ND_TCHECK_LEN(cp, item_len);

		depth++;
		ND_PRINT("\n");
		for (i = 0; i < depth; i++)
			ND_PRINT("    ");
		ND_PRINT("(");
		if (np == ISAKMP_NPTYPE_T) {
			cp = ikev2_t_print(ndo, tcount, ext, item_len, ep);
			if (cp == NULL) {
				/* error, already reported */
				return NULL;
			}
		} else {
			ND_PRINT("%s", NPSTR(np));
			cp += item_len;
		}
		ND_PRINT(")");
		depth--;
		prop_length -= item_len;
	}
	return cp;
toolong:
	/*
	 * Skip the rest of the proposal.
	 */
	cp += prop_length;
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_P));
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_P));
	return NULL;
}

static const u_char *
ikev2_sa_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext1,
		u_int osa_length, const u_char *ep,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth)
{
	const struct isakmp_gen *ext;
	u_int sa_length;
	const u_char *cp;
	int i;
	int pcount;
	u_char np;
	u_int item_len;

	ND_TCHECK_SIZE(ext1);
	ikev2_pay_print(ndo, "sa", GET_U_1(ext1->critical));

	/*
	 * ikev2_sub0_print() guarantees that this is >= 4.
	 */
	osa_length= GET_BE_U_2(ext1->len);
	sa_length = osa_length - 4;
	ND_PRINT(" len=%u", sa_length);

	/*
	 * Print the payloads.
	 */
	cp = (const u_char *)(ext1 + 1);
	pcount = 0;
	for (np = ISAKMP_NPTYPE_P; np != 0; np = GET_U_1(ext->np)) {
		pcount++;
		ext = (const struct isakmp_gen *)cp;
		if (sa_length < sizeof(*ext))
			goto toolong;
		ND_TCHECK_SIZE(ext);

		/*
		 * Since we can't have a payload length of less than 4 bytes,
		 * we need to bail out here if the generic header is nonsensical
		 * or truncated, otherwise we could loop forever processing
		 * zero-length items or otherwise misdissect the packet.
		 */
		item_len = GET_BE_U_2(ext->len);
		if (item_len <= 4)
			goto trunc;

		if (sa_length < item_len)
			goto toolong;
		ND_TCHECK_LEN(cp, item_len);

		depth++;
		ND_PRINT("\n");
		for (i = 0; i < depth; i++)
			ND_PRINT("    ");
		ND_PRINT("(");
		if (np == ISAKMP_NPTYPE_P) {
			cp = ikev2_p_print(ndo, np, pcount, ext, item_len,
					   ep, depth);
			if (cp == NULL) {
				/* error, already reported */
				return NULL;
			}
		} else {
			ND_PRINT("%s", NPSTR(np));
			cp += item_len;
		}
		ND_PRINT(")");
		depth--;
		sa_length -= item_len;
	}
	return cp;
toolong:
	/*
	 * Skip the rest of the SA.
	 */
	cp += sa_length;
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_ke_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	const struct ikev2_ke *k;

	k = (const struct ikev2_ke *)ext;
	ND_TCHECK_SIZE(k);
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(k->h.critical));

	if (item_len < 8) {
		ND_PRINT(" len=%u < 8", item_len);
		return (const u_char *)ext + item_len;
	}
	ND_PRINT(" len=%u group=%s", item_len - 8,
		  STR_OR_ID(GET_BE_U_2(k->ke_group), dh_p_map));

	if (2 < ndo->ndo_vflag && 8 < item_len) {
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(k + 1), item_len - 8))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_ID_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	const struct ikev2_id *idp;
	u_int idtype_len, i;
	unsigned int dumpascii, dumphex;
	const unsigned char *typedata;

	idp = (const struct ikev2_id *)ext;
	ND_TCHECK_SIZE(idp);
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(idp->h.critical));

	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (2 < ndo->ndo_vflag && 4 < item_len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}

	idtype_len =item_len - sizeof(struct ikev2_id);
	dumpascii = 0;
	dumphex   = 0;
	typedata  = (const unsigned char *)(ext)+sizeof(struct ikev2_id);

	switch(GET_U_1(idp->type)) {
	case ID_IPV4_ADDR:
		ND_PRINT(" ipv4:");
		dumphex=1;
		break;
	case ID_FQDN:
		ND_PRINT(" fqdn:");
		dumpascii=1;
		break;
	case ID_RFC822_ADDR:
		ND_PRINT(" rfc822:");
		dumpascii=1;
		break;
	case ID_IPV6_ADDR:
		ND_PRINT(" ipv6:");
		dumphex=1;
		break;
	case ID_DER_ASN1_DN:
		ND_PRINT(" dn:");
		dumphex=1;
		break;
	case ID_DER_ASN1_GN:
		ND_PRINT(" gn:");
		dumphex=1;
		break;
	case ID_KEY_ID:
		ND_PRINT(" keyid:");
		dumphex=1;
		break;
	}

	if(dumpascii) {
		ND_TCHECK_LEN(typedata, idtype_len);
		for(i=0; i<idtype_len; i++) {
			if(ND_ASCII_ISPRINT(GET_U_1(typedata + i))) {
				ND_PRINT("%c", GET_U_1(typedata + i));
			} else {
				ND_PRINT(".");
			}
		}
	}
	if(dumphex) {
		if (!rawprint(ndo, (const uint8_t *)typedata, idtype_len))
			goto trunc;
	}

	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_cert_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ikev2_cr_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ikev2_auth_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	const struct ikev2_auth *p;
	const char *v2_auth[]={ "invalid", "rsasig",
				"shared-secret", "dsssig" };
	const u_char *authdata = (const u_char *)ext + sizeof(struct ikev2_auth);

	ND_TCHECK_LEN(ext, sizeof(struct ikev2_auth));
	p = (const struct ikev2_auth *)ext;
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(p->h.critical));

	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u method=%s", item_len-4,
		  STR_OR_ID(GET_U_1(p->auth_method), v2_auth));
	if (item_len > 4) {
		if (ndo->ndo_vflag > 1) {
			ND_PRINT(" authdata=(");
			if (!rawprint(ndo, (const uint8_t *)authdata, item_len - sizeof(struct ikev2_auth)))
				goto trunc;
			ND_PRINT(") ");
		} else if (ndo->ndo_vflag) {
			if (!ike_show_somedata(ndo, authdata, ep))
				goto trunc;
		}
	}

	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_nonce_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	ND_TCHECK_SIZE(ext);
	ikev2_pay_print(ndo, "nonce", GET_U_1(ext->critical));

	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u", item_len - 4);
	if (1 < ndo->ndo_vflag && 4 < item_len) {
		ND_PRINT(" nonce=(");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
		ND_PRINT(") ");
	} else if(ndo->ndo_vflag && 4 < item_len) {
		if(!ike_show_somedata(ndo, (const u_char *)(ext+1), ep)) goto trunc;
	}

	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

/* notify payloads */
static const u_char *
ikev2_n_print(netdissect_options *ndo, u_char tpay _U_,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	const struct ikev2_n *p;
	uint16_t type;
	uint8_t spi_size;
	const u_char *cp;
	u_char showspi, showsomedata;
	const char *notify_name;

	p = (const struct ikev2_n *)ext;
	ND_TCHECK_SIZE(p);
	ikev2_pay_print(ndo, NPSTR(ISAKMP_NPTYPE_N), GET_U_1(p->h.critical));

	showspi = 1;
	showsomedata=0;
	notify_name=NULL;

	ND_PRINT(" prot_id=%s", PROTOIDSTR(GET_U_1(p->prot_id)));

	type = GET_BE_U_2(p->type);

	/* notify space is annoying sparse */
	switch(type) {
	case IV2_NOTIFY_UNSUPPORTED_CRITICAL_PAYLOAD:
		notify_name = "unsupported_critical_payload";
		showspi = 0;
		break;

	case IV2_NOTIFY_INVALID_IKE_SPI:
		notify_name = "invalid_ike_spi";
		showspi = 1;
		break;

	case IV2_NOTIFY_INVALID_MAJOR_VERSION:
		notify_name = "invalid_major_version";
		showspi = 0;
		break;

	case IV2_NOTIFY_INVALID_SYNTAX:
		notify_name = "invalid_syntax";
		showspi = 1;
		break;

	case IV2_NOTIFY_INVALID_MESSAGE_ID:
		notify_name = "invalid_message_id";
		showspi = 1;
		break;

	case IV2_NOTIFY_INVALID_SPI:
		notify_name = "invalid_spi";
		showspi = 1;
		break;

	case IV2_NOTIFY_NO_PROPOSAL_CHOSEN:
		notify_name = "no_protocol_chosen";
		showspi = 1;
		break;

	case IV2_NOTIFY_INVALID_KE_PAYLOAD:
		notify_name = "invalid_ke_payload";
		showspi = 1;
		break;

	case IV2_NOTIFY_AUTHENTICATION_FAILED:
		notify_name = "authentication_failed";
		showspi = 1;
		break;

	case IV2_NOTIFY_SINGLE_PAIR_REQUIRED:
		notify_name = "single_pair_required";
		showspi = 1;
		break;

	case IV2_NOTIFY_NO_ADDITIONAL_SAS:
		notify_name = "no_additional_sas";
		showspi = 0;
		break;

	case IV2_NOTIFY_INTERNAL_ADDRESS_FAILURE:
		notify_name = "internal_address_failure";
		showspi = 0;
		break;

	case IV2_NOTIFY_FAILED_CP_REQUIRED:
		notify_name = "failed:cp_required";
		showspi = 0;
		break;

	case IV2_NOTIFY_INVALID_SELECTORS:
		notify_name = "invalid_selectors";
		showspi = 0;
		break;

	case IV2_NOTIFY_INITIAL_CONTACT:
		notify_name = "initial_contact";
		showspi = 0;
		break;

	case IV2_NOTIFY_SET_WINDOW_SIZE:
		notify_name = "set_window_size";
		showspi = 0;
		break;

	case IV2_NOTIFY_ADDITIONAL_TS_POSSIBLE:
		notify_name = "additional_ts_possible";
		showspi = 0;
		break;

	case IV2_NOTIFY_IPCOMP_SUPPORTED:
		notify_name = "ipcomp_supported";
		showspi = 0;
		break;

	case IV2_NOTIFY_NAT_DETECTION_SOURCE_IP:
		notify_name = "nat_detection_source_ip";
		showspi = 1;
		break;

	case IV2_NOTIFY_NAT_DETECTION_DESTINATION_IP:
		notify_name = "nat_detection_destination_ip";
		showspi = 1;
		break;

	case IV2_NOTIFY_COOKIE:
		notify_name = "cookie";
		showspi = 1;
		showsomedata= 1;
		break;

	case IV2_NOTIFY_USE_TRANSPORT_MODE:
		notify_name = "use_transport_mode";
		showspi = 0;
		break;

	case IV2_NOTIFY_HTTP_CERT_LOOKUP_SUPPORTED:
		notify_name = "http_cert_lookup_supported";
		showspi = 0;
		break;

	case IV2_NOTIFY_REKEY_SA:
		notify_name = "rekey_sa";
		showspi = 1;
		break;

	case IV2_NOTIFY_ESP_TFC_PADDING_NOT_SUPPORTED:
		notify_name = "tfc_padding_not_supported";
		showspi = 0;
		break;

	case IV2_NOTIFY_NON_FIRST_FRAGMENTS_ALSO:
		notify_name = "non_first_fragment_also";
		showspi = 0;
		break;

	default:
		if (type < 8192) {
			notify_name="error";
		} else if(type < 16384) {
			notify_name="private-error";
		} else if(type < 40960) {
			notify_name="status";
		} else {
			notify_name="private-status";
		}
	}

	if(notify_name) {
		ND_PRINT(" type=%u(%s)", type, notify_name);
	}


	spi_size = GET_U_1(p->spi_size);
	if (showspi && spi_size) {
		ND_PRINT(" spi=");
		if (!rawprint(ndo, (const uint8_t *)(p + 1), spi_size))
			goto trunc;
	}

	cp = (const u_char *)(p + 1) + spi_size;

	if (cp < ep) {
		if (ndo->ndo_vflag > 3 || (showsomedata && ep-cp < 30)) {
			ND_PRINT(" data=(");
			if (!rawprint(ndo, (const uint8_t *)(cp), ep - cp))
				goto trunc;

			ND_PRINT(")");
		} else if (showsomedata) {
			if (!ike_show_somedata(ndo, cp, ep))
				goto trunc;
		}
	}

	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(ISAKMP_NPTYPE_N));
	return NULL;
}

static const u_char *
ikev2_d_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ikev2_vid_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	const u_char *vid;
	u_int i, len;

	ND_TCHECK_SIZE(ext);
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(ext->critical));

	/*
	 * Our caller has ensured that the length is >= 4.
	 */
	ND_PRINT(" len=%u vid=", item_len - 4);

	vid = (const u_char *)(ext+1);
	len = item_len - 4;
	ND_TCHECK_LEN(vid, len);
	for(i=0; i<len; i++) {
		if(ND_ASCII_ISPRINT(GET_U_1(vid + i)))
			ND_PRINT("%c", GET_U_1(vid + i));
		else ND_PRINT(".");
	}
	if (2 < ndo->ndo_vflag && 4 < len) {
		/* Print the entire payload in hex */
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), item_len - 4))
			goto trunc;
	}
	return (const u_char *)ext + item_len;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_TS_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ikev2_e_print(netdissect_options *ndo,
#ifndef HAVE_LIBCRYPTO
	      _U_
#endif
	      const struct isakmp *base,
	      u_char tpay,
	      const struct isakmp_gen *ext,
	      u_int item_len, const u_char *ep _U_,
#ifndef HAVE_LIBCRYPTO
	      _U_
#endif
	      uint32_t phase,
#ifndef HAVE_LIBCRYPTO
	      _U_
#endif
	      uint32_t doi,
#ifndef HAVE_LIBCRYPTO
	      _U_
#endif
	      uint32_t proto,
#ifndef HAVE_LIBCRYPTO
	      _U_
#endif
	      int depth)
{
	const u_char *dat;
	u_int dlen;
#ifdef HAVE_LIBCRYPTO
	uint8_t np;
#endif

	ND_TCHECK_SIZE(ext);
	ikev2_pay_print(ndo, NPSTR(tpay), GET_U_1(ext->critical));

	dlen = item_len-4;

	ND_PRINT(" len=%u", dlen);
	if (2 < ndo->ndo_vflag && 4 < dlen) {
		ND_PRINT(" ");
		if (!rawprint(ndo, (const uint8_t *)(ext + 1), dlen))
			goto trunc;
	}

	dat = (const u_char *)(ext+1);
	ND_TCHECK_LEN(dat, dlen);

#ifdef HAVE_LIBCRYPTO
	np = GET_U_1(ext->np);

	/* try to decrypt it! */
	if(esp_decrypt_buffer_by_ikev2_print(ndo,
					     GET_U_1(base->flags) & ISAKMP_FLAG_I,
					     base->i_ck, base->r_ck,
					     dat, dat+dlen)) {

		ext = (const struct isakmp_gen *)ndo->ndo_packetp;

		/* got it decrypted, print stuff inside. */
		ikev2_sub_print(ndo, base, np, ext,
				ndo->ndo_snapend, phase, doi, proto, depth+1);

		/*
		 * esp_decrypt_buffer_by_ikev2_print pushed information
		 * on the buffer stack; we're done with the buffer, so
		 * pop it (which frees the buffer)
		 */
		nd_pop_packet_info(ndo);
	}
#endif


	/* always return NULL, because E must be at end, and NP refers
	 * to what was inside.
	 */
	return NULL;
trunc:
	ND_PRINT(" [|%s]", NPSTR(tpay));
	return NULL;
}

static const u_char *
ikev2_cp_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ikev2_eap_print(netdissect_options *ndo, u_char tpay,
		const struct isakmp_gen *ext,
		u_int item_len, const u_char *ep _U_,
		uint32_t phase _U_, uint32_t doi _U_,
		uint32_t proto _U_, int depth _U_)
{
	return ikev2_gen_print(ndo, tpay, ext, item_len);
}

static const u_char *
ike_sub0_print(netdissect_options *ndo,
		 u_char np, const struct isakmp_gen *ext, const u_char *ep,

	       uint32_t phase, uint32_t doi, uint32_t proto, int depth)
{
	const u_char *cp;
	u_int item_len;

	cp = (const u_char *)ext;
	ND_TCHECK_SIZE(ext);

	/*
	 * Since we can't have a payload length of less than 4 bytes,
	 * we need to bail out here if the generic header is nonsensical
	 * or truncated, otherwise we could loop forever processing
	 * zero-length items or otherwise misdissect the packet.
	 */
	item_len = GET_BE_U_2(ext->len);
	if (item_len <= 4)
		return NULL;

	if (NPFUNC(np)) {
		/*
		 * XXX - what if item_len is too short, or too long,
		 * for this payload type?
		 */
		cp = (*npfunc[np])(ndo, np, ext, item_len, ep, phase, doi, proto, depth);
	} else {
		ND_PRINT("%s", NPSTR(np));
		cp += item_len;
	}

	return cp;
trunc:
	nd_print_trunc(ndo);
	return NULL;
}

static const u_char *
ikev1_sub_print(netdissect_options *ndo,
		u_char np, const struct isakmp_gen *ext, const u_char *ep,
		uint32_t phase, uint32_t doi, uint32_t proto, int depth)
{
	const u_char *cp;
	int i;
	u_int item_len;

	cp = (const u_char *)ext;

	while (np) {
		ND_TCHECK_SIZE(ext);

		item_len = GET_BE_U_2(ext->len);
		ND_TCHECK_LEN(ext, item_len);

		depth++;
		ND_PRINT("\n");
		for (i = 0; i < depth; i++)
			ND_PRINT("    ");
		ND_PRINT("(");
		cp = ike_sub0_print(ndo, np, ext, ep, phase, doi, proto, depth);
		ND_PRINT(")");
		depth--;

		if (cp == NULL) {
			/* Zero-length subitem */
			return NULL;
		}

		np = GET_U_1(ext->np);
		ext = (const struct isakmp_gen *)cp;
	}
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(np));
	return NULL;
}

static char *
numstr(u_int x)
{
	static char buf[20];
	snprintf(buf, sizeof(buf), "#%u", x);
	return buf;
}

static void
ikev1_print(netdissect_options *ndo,
	    const u_char *bp,  u_int length,
	    const u_char *bp2, const struct isakmp *base)
{
	const struct isakmp *p;
	const u_char *ep;
	u_int flags;
	u_char np;
	int i;
	u_int phase;

	p = (const struct isakmp *)bp;
	ep = ndo->ndo_snapend;

	phase = (GET_BE_U_4(base->msgid) == 0) ? 1 : 2;
	if (phase == 1)
		ND_PRINT(" phase %u", phase);
	else
		ND_PRINT(" phase %u/others", phase);

	i = cookie_find(&base->i_ck);
	if (i < 0) {
		if (iszero(ndo, base->r_ck, sizeof(base->r_ck))) {
			/* the first packet */
			ND_PRINT(" I");
			if (bp2)
				cookie_record(ndo, &base->i_ck, bp2);
		} else
			ND_PRINT(" ?");
	} else {
		if (bp2 && cookie_isinitiator(ndo, i, bp2))
			ND_PRINT(" I");
		else if (bp2 && cookie_isresponder(ndo, i, bp2))
			ND_PRINT(" R");
		else
			ND_PRINT(" ?");
	}

	ND_PRINT(" %s", ETYPESTR(GET_U_1(base->etype)));
	flags = GET_U_1(base->flags);
	if (flags) {
		ND_PRINT("[%s%s]", flags & ISAKMP_FLAG_E ? "E" : "",
			  flags & ISAKMP_FLAG_C ? "C" : "");
	}

	if (ndo->ndo_vflag) {
		const struct isakmp_gen *ext;

		ND_PRINT(":");

		np = GET_U_1(base->np);

		/* regardless of phase... */
		if (flags & ISAKMP_FLAG_E) {
			/*
			 * encrypted, nothing we can do right now.
			 * we hope to decrypt the packet in the future...
			 */
			ND_PRINT(" [encrypted %s]", NPSTR(np));
			goto done;
		}

		CHECKLEN(p + 1, np);
		ext = (const struct isakmp_gen *)(p + 1);
		ikev1_sub_print(ndo, np, ext, ep, phase, 0, 0, 0);
	}

done:
	if (ndo->ndo_vflag) {
		if (GET_BE_U_4(base->len) != length) {
			ND_PRINT(" (len mismatch: isakmp %u/ip %u)",
				  GET_BE_U_4(base->len), length);
		}
	}
}

static const u_char *
ikev2_sub0_print(netdissect_options *ndo, const struct isakmp *base,
		 u_char np,
		 const struct isakmp_gen *ext, const u_char *ep,
		 uint32_t phase, uint32_t doi, uint32_t proto, int depth)
{
	const u_char *cp;
	u_int item_len;

	cp = (const u_char *)ext;
	ND_TCHECK_SIZE(ext);

	/*
	 * Since we can't have a payload length of less than 4 bytes,
	 * we need to bail out here if the generic header is nonsensical
	 * or truncated, otherwise we could loop forever processing
	 * zero-length items or otherwise misdissect the packet.
	 */
	item_len = GET_BE_U_2(ext->len);
	if (item_len <= 4)
		return NULL;

	if (np == ISAKMP_NPTYPE_v2E) {
		cp = ikev2_e_print(ndo, base, np, ext, item_len,
				   ep, phase, doi, proto, depth);
	} else if (NPFUNC(np)) {
		/*
		 * XXX - what if item_len is too short, or too long,
		 * for this payload type?
		 */
		cp = (*npfunc[np])(ndo, np, ext, item_len,
				   ep, phase, doi, proto, depth);
	} else {
		ND_PRINT("%s", NPSTR(np));
		cp += item_len;
	}

	return cp;
trunc:
	nd_print_trunc(ndo);
	return NULL;
}

static const u_char *
ikev2_sub_print(netdissect_options *ndo,
		const struct isakmp *base,
		u_char np, const struct isakmp_gen *ext, const u_char *ep,
		uint32_t phase, uint32_t doi, uint32_t proto, int depth)
{
	const u_char *cp;
	int i;

	cp = (const u_char *)ext;
	while (np) {
		ND_TCHECK_SIZE(ext);

		ND_TCHECK_LEN(ext, GET_BE_U_2(ext->len));

		depth++;
		ND_PRINT("\n");
		for (i = 0; i < depth; i++)
			ND_PRINT("    ");
		ND_PRINT("(");
		cp = ikev2_sub0_print(ndo, base, np,
				      ext, ep, phase, doi, proto, depth);
		ND_PRINT(")");
		depth--;

		if (cp == NULL) {
			/* Zero-length subitem */
			return NULL;
		}

		np = GET_U_1(ext->np);
		ext = (const struct isakmp_gen *)cp;
	}
	return cp;
trunc:
	ND_PRINT(" [|%s]", NPSTR(np));
	return NULL;
}

static void
ikev2_print(netdissect_options *ndo,
	    const u_char *bp,  u_int length,
	    const u_char *bp2 _U_, const struct isakmp *base)
{
	const struct isakmp *p;
	const u_char *ep;
	uint8_t flags;
	u_char np;
	u_int phase;

	p = (const struct isakmp *)bp;
	ep = ndo->ndo_snapend;

	phase = (GET_BE_U_4(base->msgid) == 0) ? 1 : 2;
	if (phase == 1)
		ND_PRINT(" parent_sa");
	else
		ND_PRINT(" child_sa ");

	ND_PRINT(" %s", ETYPESTR(GET_U_1(base->etype)));
	flags = GET_U_1(base->flags);
	if (flags) {
		ND_PRINT("[%s%s%s]",
			  flags & ISAKMP_FLAG_I ? "I" : "",
			  flags & ISAKMP_FLAG_V ? "V" : "",
			  flags & ISAKMP_FLAG_R ? "R" : "");
	}

	if (ndo->ndo_vflag) {
		const struct isakmp_gen *ext;

		ND_PRINT(":");

		np = GET_U_1(base->np);

		/* regardless of phase... */
		if (flags & ISAKMP_FLAG_E) {
			/*
			 * encrypted, nothing we can do right now.
			 * we hope to decrypt the packet in the future...
			 */
			ND_PRINT(" [encrypted %s]", NPSTR(np));
			goto done;
		}

		CHECKLEN(p + 1, np)
		ext = (const struct isakmp_gen *)(p + 1);
		ikev2_sub_print(ndo, base, np, ext, ep, phase, 0, 0, 0);
	}

done:
	if (ndo->ndo_vflag) {
		if (GET_BE_U_4(base->len) != length) {
			ND_PRINT(" (len mismatch: isakmp %u/ip %u)",
				  GET_BE_U_4(base->len), length);
		}
	}
}

void
isakmp_print(netdissect_options *ndo,
	     const u_char *bp, u_int length,
	     const u_char *bp2)
{
	const struct isakmp *p;
	const u_char *ep;
	u_int major, minor;

	ndo->ndo_protocol = "isakmp";
#ifdef HAVE_LIBCRYPTO
	/* initialize SAs */
	if (ndo->ndo_sa_list_head == NULL) {
		if (ndo->ndo_espsecret)
			esp_decodesecret_print(ndo);
	}
#endif

	p = (const struct isakmp *)bp;
	ep = ndo->ndo_snapend;

	if ((const struct isakmp *)ep < p + 1) {
		nd_print_trunc(ndo);
		return;
	}

	ND_PRINT("isakmp");
	major = (GET_U_1(p->vers) & ISAKMP_VERS_MAJOR)
		>> ISAKMP_VERS_MAJOR_SHIFT;
	minor = (GET_U_1(p->vers) & ISAKMP_VERS_MINOR)
		>> ISAKMP_VERS_MINOR_SHIFT;

	if (ndo->ndo_vflag) {
		ND_PRINT(" %u.%u", major, minor);
	}

	if (ndo->ndo_vflag) {
		ND_PRINT(" msgid ");
		hexprint(ndo, p->msgid, sizeof(p->msgid));
	}

	if (1 < ndo->ndo_vflag) {
		ND_PRINT(" cookie ");
		hexprint(ndo, p->i_ck, sizeof(p->i_ck));
		ND_PRINT("->");
		hexprint(ndo, p->r_ck, sizeof(p->r_ck));
	}
	ND_PRINT(":");

	switch(major) {
	case IKEv1_MAJOR_VERSION:
		ikev1_print(ndo, bp, length, bp2, p);
		break;

	case IKEv2_MAJOR_VERSION:
		ikev2_print(ndo, bp, length, bp2, p);
		break;
	}
}

void
isakmp_rfc3948_print(netdissect_options *ndo,
		     const u_char *bp, u_int length,
		     const u_char *bp2, int ver, int fragmented, u_int ttl_hl)
{
	ndo->ndo_protocol = "isakmp_rfc3948";
	if(length == 1 && GET_U_1(bp)==0xff) {
		ND_PRINT("isakmp-nat-keep-alive");
		return;
	}

	if(length < 4) {
		goto trunc;
	}

	/*
	 * see if this is an IKE packet
	 */
	if (GET_BE_U_4(bp) == 0) {
		ND_PRINT("NONESP-encap: ");
		isakmp_print(ndo, bp+4, length-4, bp2);
		return;
	}

	/* must be an ESP packet */
	{
		ND_PRINT("UDP-encap: ");

		esp_print(ndo, bp, length, bp2, ver, fragmented, ttl_hl);

		/*
		 * Either this has decrypted the payload and
		 * printed it, in which case there's nothing more
		 * to do, or it hasn't, in which case there's
		 * nothing more to do.
		 */
		return;
	}

trunc:
	nd_print_trunc(ndo);
}
