/** @file
  Definitions of DSP0274 Security Protocol & Data Model Specification (SPDM)
  version 1.2.0 in Distributed Management Task Force (DMTF).

Copyright (c) 2019 - 2024, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SPDM_H__
#define __SPDM_H__

#pragma pack(1)

#define SPDM_MAX_SLOT_COUNT        8
#define SPDM_MAX_OPAQUE_DATA_SIZE  1024
#define SPDM_NONCE_SIZE            32
#define SPDM_RANDOM_DATA_SIZE      32
///
/// SPDM response code (1.0)
///
#define SPDM_DIGESTS                  0x01
#define SPDM_CERTIFICATE              0x02
#define SPDM_CHALLENGE_AUTH           0x03
#define SPDM_VERSION                  0x04
#define SPDM_MEASUREMENTS             0x60
#define SPDM_CAPABILITIES             0x61
#define SPDM_ALGORITHMS               0x63
#define SPDM_VENDOR_DEFINED_RESPONSE  0x7E
#define SPDM_ERROR                    0x7F
///
/// SPDM response code (1.1)
///
#define SPDM_KEY_EXCHANGE_RSP           0x64
#define SPDM_FINISH_RSP                 0x65
#define SPDM_PSK_EXCHANGE_RSP           0x66
#define SPDM_PSK_FINISH_RSP             0x67
#define SPDM_HEARTBEAT_ACK              0x68
#define SPDM_KEY_UPDATE_ACK             0x69
#define SPDM_ENCAPSULATED_REQUEST       0x6A
#define SPDM_ENCAPSULATED_RESPONSE_ACK  0x6B
#define SPDM_END_SESSION_ACK            0x6C
///
/// SPDM response code (1.2)
///
#define SPDM_CSR                  0x6D
#define SPDM_SET_CERTIFICATE_RSP  0x6E
#define SPDM_CHUNK_SEND_ACK       0x05
#define SPDM_CHUNK_RESPONSE       0x06
///
/// SPDM request code (1.0)
///
#define SPDM_GET_DIGESTS             0x81
#define SPDM_GET_CERTIFICATE         0x82
#define SPDM_CHALLENGE               0x83
#define SPDM_GET_VERSION             0x84
#define SPDM_GET_MEASUREMENTS        0xE0
#define SPDM_GET_CAPABILITIES        0xE1
#define SPDM_NEGOTIATE_ALGORITHMS    0xE3
#define SPDM_VENDOR_DEFINED_REQUEST  0xFE
#define SPDM_RESPOND_IF_READY        0xFF
///
/// SPDM request code (1.1)
///
#define SPDM_KEY_EXCHANGE                   0xE4
#define SPDM_FINISH                         0xE5
#define SPDM_PSK_EXCHANGE                   0xE6
#define SPDM_PSK_FINISH                     0xE7
#define SPDM_HEARTBEAT                      0xE8
#define SPDM_KEY_UPDATE                     0xE9
#define SPDM_GET_ENCAPSULATED_REQUEST       0xEA
#define SPDM_DELIVER_ENCAPSULATED_RESPONSE  0xEB
#define SPDM_END_SESSION                    0xEC
///
/// SPDM request code (1.2)
///
#define SPDM_GET_CSR          0xED
#define SPDM_SET_CERTIFICATE  0xEE
#define SPDM_CHUNK_SEND       0x85
#define SPDM_CHUNK_GET        0x86

///
/// SPDM message header
///
typedef struct {
  UINT8    SPDMVersion;
  UINT8    RequestResponseCode;
  UINT8    Param1;
  UINT8    Param2;
} SPDM_MESSAGE_HEADER;

#define SPDM_MESSAGE_VERSION_10  0x10
#define SPDM_MESSAGE_VERSION_11  0x11
#define SPDM_MESSAGE_VERSION_12  0x12
#define SPDM_MESSAGE_VERSION     SPDM_MESSAGE_VERSION_10

///
/// SPDM GET_VERSION request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_GET_VERSION_REQUEST;

///
/// SPDM GET_VERSION response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  UINT8                  Reserved;
  UINT8                  VersionNumberEntryCount;
  // SPDM_VERSION_NUMBER  VersionNumberEntry[VersionNumberEntryCount];
} SPDM_VERSION_RESPONSE;

///
/// SPDM VERSION structure
///
/// bit[15:12] major_version
/// bit[11:8]  minor_version
/// bit[7:4]   update_version_number
/// bit[3:0]   alpha
typedef UINT16 SPDM_VERSION_NUMBER;
#define SPDM_VERSION_NUMBER_SHIFT_BIT  8

#define SPDM_VERSION_1_2_SIGNING_PREFIX_CONTEXT  "dmtf-spdm-v1.2.*"
#define SPDM_VERSION_1_2_SIGNING_PREFIX_CONTEXT_SIZE \
    (sizeof(SPDM_VERSION_1_2_SIGNING_PREFIX_CONTEXT) - 1)
#define SPDM_VERSION_1_2_SIGNING_CONTEXT_SIZE  100
///
/// SPDM GET_CAPABILITIES request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  // Below field is added in 1.1.
  UINT8                  Reserved;
  UINT8                  CTExponent;
  UINT16                 Reserved2;
  UINT32                 Flags;
  // Below field is added in 1.2.
  UINT32                 DataTransferSize;
  UINT32                 MaxSpdmMsgSize;
} SPDM_GET_CAPABILITIES_REQUEST;

///
/// SPDM GET_CAPABILITIES response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  UINT8                  Reserved;
  UINT8                  CTExponent;
  UINT16                 Reserved2;
  UINT32                 Flags;
  // Below field is added in 1.2.
  UINT32                 DataTransferSize;
  UINT32                 MaxSpdmMsgSize;
} SPDM_CAPABILITIES_RESPONSE;

#define SPDM_MIN_DATA_TRANSFER_SIZE_VERSION_12  42

///
/// SPDM GET_CAPABILITIES request Flags (1.1)
///
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP                    BIT1
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHAL_CAP                    BIT2
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCRYPT_CAP                 BIT6
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP                     BIT7
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP                BIT8
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_EX_CAP                  BIT9
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PSK_CAP                     (BIT10 | BIT11)
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PSK_CAP_REQUESTER           BIT10
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCAP_CAP                   BIT12
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HBEAT_CAP                   BIT13
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_UPD_CAP                 BIT14
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP  BIT15
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PUB_KEY_ID_CAP              BIT16
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_11_MASK                     (\
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CERT_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHAL_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCRYPT_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MAC_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_EX_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PSK_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_ENCAP_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HBEAT_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_KEY_UPD_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_PUB_KEY_ID_CAP)

///
/// SPDM GET_CAPABILITIES request Flags (1.2)
///
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHUNK_CAP  BIT17
#define SPDM_GET_CAPABILITIES_REQUEST_FLAGS_12_MASK    (\
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_11_MASK | \
        SPDM_GET_CAPABILITIES_REQUEST_FLAGS_CHUNK_CAP)
///
/// SPDM GET_CAPABILITIES response Flags (1.0)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CACHE_CAP        BIT0
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP         BIT1
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHAL_CAP         BIT2
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP         (BIT3 | BIT4)
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_NO_SIG  BIT3
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_SIG     BIT4
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP   BIT5
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_10_MASK          (\
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CACHE_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHAL_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP)
///
/// SPDM GET_CAPABILITIES response Flags (1.1)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCRYPT_CAP                     BIT6
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MAC_CAP                         BIT7
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MUT_AUTH_CAP                    BIT8
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP                      BIT9
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP                         (BIT10 | BIT11)
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP_RESPONDER               BIT10
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP_RESPONDER_WITH_CONTEXT  BIT11
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCAP_CAP                       BIT12
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HBEAT_CAP                       BIT13
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_UPD_CAP                     BIT14
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP      BIT15
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PUB_KEY_ID_CAP                  BIT16
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_11_MASK                         (\
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_10_MASK | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCRYPT_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MAC_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MUT_AUTH_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_EX_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PSK_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ENCAP_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HBEAT_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_KEY_UPD_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_HANDSHAKE_IN_THE_CLEAR_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_PUB_KEY_ID_CAP)
///
/// SPDM GET_CAPABILITIES response Flags (1.2)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHUNK_CAP       BIT17
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ALIAS_CERT_CAP  BIT18

///
/// SPDM GET_CAPABILITIES response Flags (1.2.1)
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_SET_CERT_CAP            BIT19
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CSR_CAP                 BIT20
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_INSTALL_RESET_CAP  BIT21
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_12_MASK                 (\
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_11_MASK | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHUNK_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_ALIAS_CERT_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_SET_CERT_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CSR_CAP | \
        SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_INSTALL_RESET_CAP)
///
/// SPDM NEGOTIATE_ALGORITHMS request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == Number of Algorithms Structure Tables
  // Param2 == RSVD
  UINT16                 Length;
  UINT8                  MeasurementSpecification;

  // OtherParamsSupport is added in 1.2.
  // BIT[0:3]=opaque_data_format support
  // BIT[4:7]=Reserved
  UINT8                  OtherParamsSupport;
  UINT32                 BaseAsymAlgo;
  UINT32                 BaseHashAlgo;
  UINT8                  Reserved2[12];
  UINT8                  ExtAsymCount;
  UINT8                  ExtHashCount;
  UINT16                 Reserved3;
  // SPDM_EXTENDED_ALGORITHM                 ExtAsym[ExtAsymCount];
  // SPDM_EXTENDED_ALGORITHM                 ExtHash[ExtHashCount];
  // Below field is added in 1.1.
  // SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE  AlgStruct[Param1];
} SPDM_NEGOTIATE_ALGORITHMS_REQUEST;

#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_LENGTH_VERSION_10         BIT6
#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_LENGTH_VERSION_11         BIT7
#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_LENGTH_VERSION_12         BIT7
#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_EXT_ALG_COUNT_VERSION_10  BIT3
#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_EXT_ALG_COUNT_VERSION_11  (BIT4 | BIT2)
#define SPDM_NEGOTIATE_ALGORITHMS_REQUEST_MAX_EXT_ALG_COUNT_VERSION_12  (BIT4 | BIT2)

typedef struct {
  UINT8    AlgType;
  UINT8    AlgCount;             // BIT[0:3]=ExtAlgCount, BIT[4:7]=FixedAlgByteCount
  // UINT8                AlgSupported[FixedAlgByteCount];
  // UINT32               AlgExternal[ExtAlgCount];
} SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE;

typedef struct {
  UINT8    ExtAlgCount       : 4;
  UINT8    FixedAlgByteCount : 4;
} SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_COUNT;

#define SPDM_NEGOTIATE_ALGORITHMS_MAX_NUM_STRUCT_TABLE_ALG  4

#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_DHE                2
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_AEAD               3
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_REQ_BASE_ASYM_ALG  4
#define SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE_ALG_TYPE_KEY_SCHEDULE       5

typedef struct {
  UINT8     AlgType;
  UINT8     AlgCount;
  UINT16    AlgSupported;
} SPDM_NEGOTIATE_ALGORITHMS_COMMON_STRUCT_TABLE;

///
/// SPDM NEGOTIATE_ALGORITHMS request BaseAsymAlgo/REQ_BASE_ASYM_ALG
///
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048          BIT0
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048          BIT1
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072          BIT2
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072          BIT3
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256  BIT4
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096          BIT5
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096          BIT6
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384  BIT7
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521  BIT8

///
/// SPDM NEGOTIATE_ALGORITHMS request base_asym_algo/REQ_BASE_ASYM_ALG (1.2)
///
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_SM2_ECC_SM2_P256  BIT9
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED25519             BIT10
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_EDDSA_ED448               BIT11

///
/// SPDM NEGOTIATE_ALGORITHMS request BaseHashAlgo
///
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256   BIT0
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384   BIT1
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512   BIT2
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256  BIT3
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384  BIT4
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512  BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS request base_hash_algo (1.2)
///
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SM3_256  BIT6

///
/// SPDM NEGOTIATE_ALGORITHMS request DHE
///
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_2048   BIT0
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_3072   BIT1
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_FFDHE_4096   BIT2
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_256_R1  BIT3
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_384_R1  BIT4
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SECP_521_R1  BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS request DHE (1.2)
///
#define SPDM_ALGORITHMS_DHE_NAMED_GROUP_SM2_P256  BIT6

///
/// SPDM NEGOTIATE_ALGORITHMS request AEAD
///
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_128_GCM        BIT0
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AES_256_GCM        BIT1
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_CHACHA20_POLY1305  BIT2

///
/// SPDM NEGOTIATE_ALGORITHMS request AEAD (1.2)
///
#define SPDM_ALGORITHMS_AEAD_CIPHER_SUITE_AEAD_SM4_GCM  BIT3
///
/// SPDM NEGOTIATE_ALGORITHMS request KEY_SCHEDULE
///
#define SPDM_ALGORITHMS_KEY_SCHEDULE_HMAC_HASH  BIT0

///
/// SPDM NEGOTIATE_ALGORITHMS response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == Number of Algorithms Structure Tables
  // Param2 == RSVD
  UINT16                 Length;
  UINT8                  MeasurementSpecificationSel;

  // OtherParamsSelection is added in 1.2.
  // BIT[0:3]=opaque_data_format select,
  // BIT[4:7]=Reserved
  UINT8                  OtherParamsSelection;
  UINT32                 MeasurementHashAlgo;
  UINT32                 BaseAsymSel;
  UINT32                 BaseHashSel;
  UINT8                  Reserved2[12];
  UINT8                  ExtAsymSelCount;
  UINT8                  ExtHashSelCount;
  UINT16                 Reserved3;
  // SPDM_EXTENDED_ALGORITHM                 ExtAsymSel[ExtAsymSelCount];
  // SPDM_EXTENDED_ALGORITHM                 ExtHashSel[ExtHashSelCount];
  // Below field is added in 1.1.
  // SPDM_NEGOTIATE_ALGORITHMS_STRUCT_TABLE  AlgStruct[Param1];
} SPDM_ALGORITHMS_RESPONSE;

///
/// SPDM NEGOTIATE_ALGORITHMS response MeasurementHashAlgo
///
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_RAW_BIT_STREAM_ONLY  BIT0
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256      BIT1
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384      BIT2
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_512      BIT3
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_256     BIT4
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_384     BIT5
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_512     BIT6

///
/// SPDM NEGOTIATE_ALGORITHMS response measurement_hash_algo (1.2)
///
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SM3_256  BIT7

///
/// SPDM Opaque Data Format (1.2)
///
#define SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_NONE  0x0
#define SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_0     0x1
#define SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_1     0x2
#define SPDM_ALGORITHMS_OPAQUE_DATA_FORMAT_MASK  0xF

///
/// SPDM Opaque Data Format 1 (1.2)
///
typedef struct {
  UINT8    TotalElements;
  UINT8    Reserved[3];
  // opaque_element_table_t  opaque_list[];
} SPDM_GENERAL_OPAQUE_DATA_TABLE_HEADER;

///
/// SPDM extended algorithm
///
typedef struct {
  UINT8     RegistryID;
  UINT8     Reserved;
  UINT16    AlgorithmID;
} SPDM_EXTENDED_ALGORITHM;

///
/// SPDM RegistryID
///
#define SPDM_REGISTRY_ID_DMTF     0
#define SPDM_REGISTRY_ID_TCG      1
#define SPDM_REGISTRY_ID_USB      2
#define SPDM_REGISTRY_ID_PCISIG   3
#define SPDM_REGISTRY_ID_IANA     4
#define SPDM_REGISTRY_ID_HDBASET  5
#define SPDM_REGISTRY_ID_MIPI     6
#define SPDM_REGISTRY_ID_CXL      7
#define SPDM_REGISTRY_ID_JEDEC    8

///
/// SPDM GET_DIGESTS request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_GET_DIGESTS_REQUEST;

///
/// SPDM GET_DIGESTS response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == SlotMask
  // UINT8                Digest[DigestSize][SlotCount];
} SPDM_DIGESTS_RESPONSE;

///
/// SPDM GET_CERTIFICATE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == SlotNum
  // Param2 == RSVD
  UINT16                 Offset;
  UINT16                 Length;
} SPDM_GET_CERTIFICATE_REQUEST;

#define SPDM_GET_CERTIFICATE_REQUEST_SLOT_ID_MASK  0xF
///
/// SPDM GET_CERTIFICATE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == SlotNum
  // Param2 == RSVD
  UINT16                 PortionLength;
  UINT16                 RemainderLength;
  // UINT8                CertChain[PortionLength];
} SPDM_CERTIFICATE_RESPONSE;

#define SPDM_CERTIFICATE_RESPONSE_SLOT_ID_MASK  0xF

typedef struct {
  //
  // Total length of the certificate chain, in bytes,
  // including all fields in this table.
  //
  UINT16    Length;
  UINT16    Reserved;
  //
  // Digest of the Root Certificate.
  // Note that Root Certificate is ASN.1 DER-encoded for this digest.
  // The hash size is determined by the SPDM device.
  //
  // UINT8    RootHash[HashSize];
  //
  // One or more ASN.1 DER-encoded X509v3 certificates where the first certificate is signed by the Root
  // Certificate or is the Root Certificate itself and each subsequent certificate is signed by the preceding
  // certificate. The last certificate is the Leaf Certificate.
  //
  // UINT8    Certificates[Length - 4 - HashSize];
} SPDM_CERT_CHAIN;

///
/// Maximum size, in bytes, of a certificate chain.
///
#define SPDM_MAX_CERTIFICATE_CHAIN_SIZE  65535
///
/// SPDM CHALLENGE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == SlotNum
  // Param2 == HashType
  UINT8                  Nonce[32];
} SPDM_CHALLENGE_REQUEST;

///
/// SPDM CHALLENGE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == ResponseAttribute
  // Param2 == SlotMask
  // UINT8                CertChainHash[DigestSize];
  // UINT8                Nonce[32];
  // UINT8                MeasurementSummaryHash[DigestSize];
  // UINT16               OpaqueLength;
  // UINT8                OpaqueData[OpaqueLength];
  // UINT8                Signature[KeySize];
} SPDM_CHALLENGE_AUTH_RESPONSE;

///
/// SPDM generic request measurement summary HashType
///
#define SPDM_REQUEST_NO_MEASUREMENT_SUMMARY_HASH     0
#define SPDM_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH  1
#define SPDM_REQUEST_ALL_MEASUREMENTS_HASH           0xFF

///
/// SPDM CHALLENGE request measurement summary HashType
///
#define SPDM_CHALLENGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH  SPDM_REQUEST_NO_MEASUREMENT_SUMMARY_HASH
#define SPDM_CHALLENGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH \
    SPDM_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH
#define SPDM_CHALLENGE_REQUEST_ALL_MEASUREMENTS_HASH  SPDM_REQUEST_ALL_MEASUREMENTS_HASH

#define SPDM_CHALLENGE_AUTH_RESPONSE_ATTRIBUTE_SLOT_ID_MASK  0xF

typedef struct {
  UINT8    SlotNum         : 4;
  UINT8    Reserved        : 3;
  UINT8    BasicMutAuthReq : 1;
} SPDM_CHALLENGE_AUTH_RESPONSE_ATTRIBUTE;

///
/// Deprecated in SPDM 1.2
///
#define SPDM_CHALLENGE_AUTH_RESPONSE_ATTRIBUTE_BASIC_MUT_AUTH_REQ  BIT7

#define SPDM_CHALLENGE_AUTH_SIGN_CONTEXT           "responder-challenge_auth signing"
#define SPDM_CHALLENGE_AUTH_SIGN_CONTEXT_SIZE      (sizeof(SPDM_CHALLENGE_AUTH_SIGN_CONTEXT) - 1)
#define SPDM_MUT_CHALLENGE_AUTH_SIGN_CONTEXT       "requester-challenge_auth signing"
#define SPDM_MUT_CHALLENGE_AUTH_SIGN_CONTEXT_SIZE  (sizeof(SPDM_MUT_CHALLENGE_AUTH_SIGN_CONTEXT) - 1)

///
/// SPDM GET_MEASUREMENTS request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == Attributes
  // Param2 == MeasurementOperation
  UINT8                  Nonce[32];
  // Below field is added in 1.1.
  UINT8                  SlotIDParam; // BIT[0:3]=SlotNum, BIT[4:7]=Reserved
} SPDM_GET_MEASUREMENTS_REQUEST;

typedef struct {
  UINT8    SlotNum  : 4;
  UINT8    Reserved : 4;
} SPDM_GET_MEASUREMENTS_REQUEST_SLOT_ID_PARAMETER;

#define SPDM_GET_MEASUREMENTS_REQUEST_SLOT_ID_MASK  0xF

///
/// SPDM GET_MEASUREMENTS request Attributes
///
#define SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_GENERATE_SIGNATURE         BIT0
#define SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_RAW_BIT_STREAM_REQUESTED   BIT1
#define SPDM_GET_MEASUREMENTS_REQUEST_ATTRIBUTES_NEW_MEASUREMENT_REQUESTED  BIT2

///
/// SPDM GET_MEASUREMENTS request MeasurementOperation
///
#define SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_TOTAL_NUMBER_OF_MEASUREMENTS  0

///
/// SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_INDEX
///
#define SPDM_GET_MEASUREMENTS_REQUEST_MEASUREMENT_OPERATION_ALL_MEASUREMENTS  0xFF

///
/// SPDM MEASUREMENTS block common header
///
typedef struct {
  UINT8     Index;
  UINT8     MeasurementSpecification;
  UINT16    MeasurementSize;
  // UINT8                Measurement[MeasurementSize];
} SPDM_MEASUREMENT_BLOCK_COMMON_HEADER;

#define SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF  BIT0

///
/// SPDM MEASUREMENTS block DMTF header
///
typedef struct {
  UINT8     DMTFSpecMeasurementValueType;
  UINT16    DMTFSpecMeasurementValueSize;
  // UINT8                DMTFSpecMeasurementValue[DMTFSpecMeasurementValueSize];
} SPDM_MEASUREMENT_BLOCK_DMTF_HEADER;

typedef struct {
  SPDM_MEASUREMENT_BLOCK_COMMON_HEADER    MeasurementBlockCommonHeader;
  SPDM_MEASUREMENT_BLOCK_DMTF_HEADER      MeasurementBlockDmtfHeader;
  // UINT8                                 HashValue[HashSize];
} SPDM_MEASUREMENT_BLOCK_DMTF;

typedef struct {
  UINT8    Content      : 7;
  UINT8    Presentation : 1;
} SPDM_MEASUREMENTS_BLOCK_MEASUREMENT_TYPE;

///
/// SPDM MEASUREMENTS block MeasurementValueType
///
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_IMMUTABLE_ROM           0
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE        1
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION  2
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_FIRMWARE_CONFIGURATION  3
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MEASUREMENT_MANIFEST    4
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_DEVICE_MODE             5
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_VERSION                 6
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_SECURE_VERSION_NUMBER   7
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MASK                    0x7
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM          BIT7

///
/// SPDM MEASUREMENTS block index
///
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_INDEX_MEASUREMENT_MANIFEST  0xFD
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_INDEX_DEVICE_MODE           0xFE

///
/// SPDM MEASUREMENTS device mode
///
typedef struct {
  UINT32    OperationalModeCapabilities;
  UINT32    OperationalModeState;
  UINT32    DeviceModeCapabilities;
  UINT32    DeviceModeState;
} SPDM_MEASUREMENT_DEVICE_MODE;

#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_MANUFACTURING_MODE   BIT0
#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_VALIDATION_MODE      BIT1
#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_NORMAL_MODE          BIT2
#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_RECOVERY_MODE        BIT3
#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_RMA_MODE             BIT4
#define SPDM_MEASUREMENT_DEVICE_OPERATION_MODE_DECOMMISSIONED_MODE  BIT5

#define SPDM_MEASUREMENT_DEVICE_MODE_NON_INVASIVE_DEBUG_MODE_IS_ACTIVE              BIT0
#define SPDM_MEASUREMENT_DEVICE_MODE_INVASIVE_DEBUG_MODE_IS_ACTIVE                  BIT1
#define SPDM_MEASUREMENT_DEVICE_MODE_NON_INVASIVE_DEBUG_MODE_HAS_BEEN_ACTIVE        BIT2
#define SPDM_MEASUREMENT_DEVICE_MODE_INVASIVE_DEBUG_MODE_HAS_BEEN_ACTIVE            BIT3
#define SPDM_MEASUREMENT_DEVICE_MODE_INVASIVE_DEBUG_MODE_HAS_BEEN_ACTIVE_AFTER_MFG  BIT4

///
/// SPDM MEASUREMENTS SVN
///
typedef UINT64 SPDM_MEASUREMENTS_SECURE_VERSION_NUMBER;

///
/// SPDM GET_MEASUREMENTS response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == TotalNumberOfMeasurement/RSVD
  // Param2 == SlotNum
  UINT8                  NumberOfBlocks;
  UINT8                  MeasurementRecordLength[3];
  // UINT8                MeasurementRecord[MeasurementRecordLength];
  // UINT8                Nonce[32];
  // UINT16               OpaqueLength;
  // UINT8                OpaqueData[OpaqueLength];
  // UINT8                Signature[KeySize];
} SPDM_MEASUREMENTS_RESPONSE;

#define SPDM_MEASUREMENTS_RESPONSE_SLOT_ID_MASK  0xF

///
/// SPDM MEASUREMENTS content changed
///
#define SPDM_MEASUREMENTS_RESPONSE_CONTENT_CHANGE_MASK          0x30
#define SPDM_MEASUREMENTS_RESPONSE_CONTENT_CHANGE_NO_DETECTION  0x00
#define SPDM_MEASUREMENTS_RESPONSE_CONTENT_CHANGE_DETECTED      0x10
#define SPDM_MEASUREMENTS_RESPONSE_CONTENT_NO_CHANGE_DETECTED   0x20

#define SPDM_MEASUREMENTS_SIGN_CONTEXT       "responder-measurements signing"
#define SPDM_MEASUREMENTS_SIGN_CONTEXT_SIZE  (sizeof(SPDM_MEASUREMENTS_SIGN_CONTEXT) - 1)

#define SPDM_MEL_SPECIFICATION_DMTF  BIT0

///
/// SPDM ERROR response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == Error Code
  // Param2 == Error Data
  // UINT8                ExtendedErrorData[];
} SPDM_ERROR_RESPONSE;

#define SPDM_EXTENDED_ERROR_DATA_MAX_SIZE  32

///
/// SPDM error code
///
#define SPDM_ERROR_CODE_INVALID_REQUEST      0x01
#define SPDM_ERROR_CODE_BUSY                 0x03
#define SPDM_ERROR_CODE_UNEXPECTED_REQUEST   0x04
#define SPDM_ERROR_CODE_UNSPECIFIED          0x05
#define SPDM_ERROR_CODE_UNSUPPORTED_REQUEST  0x07
#define SPDM_ERROR_CODE_VERSION_MISMATCH     0x41
#define SPDM_ERROR_CODE_RESPONSE_NOT_READY   0x42
#define SPDM_ERROR_CODE_REQUEST_RESYNCH      0x43
#define SPDM_ERROR_CODE_VENDOR_DEFINED       0xFF
///
/// SPDM error code (1.1)
///
#define SPDM_ERROR_CODE_DECRYPT_ERROR           0x06
#define SPDM_ERROR_CODE_REQUEST_IN_FLIGHT       0x08
#define SPDM_ERROR_CODE_INVALID_RESPONSE_CODE   0x09
#define SPDM_ERROR_CODE_SESSION_LIMIT_EXCEEDED  0x0A

///
/// SPDM error code (1.2)
///
#define SPDM_ERROR_CODE_SESSION_REQUIRED    0x0B
#define SPDM_ERROR_CODE_RESET_REQUIRED      0x0C
#define SPDM_ERROR_CODE_RESPONSE_TOO_LARGE  0x0D
#define SPDM_ERROR_CODE_REQUEST_TOO_LARGE   0x0E
#define SPDM_ERROR_CODE_LARGE_RESPONSE      0x0F
#define SPDM_ERROR_CODE_MESSAGE_LOST        0x10
///
/// SPDM ResponseNotReady extended data
///
typedef struct {
  UINT8    RDTExponent;
  UINT8    RequestCode;
  UINT8    Token;
  UINT8    Rdtm;
} SPDM_ERROR_DATA_RESPONSE_NOT_READY;

typedef struct {
  SPDM_MESSAGE_HEADER                   Header;
  // Param1 == Error Code
  // Param2 == Error Data
  SPDM_ERROR_DATA_RESPONSE_NOT_READY    ExtendErrorData;
} SPDM_ERROR_RESPONSE_DATA_RESPONSE_NOT_READY;

///
/// SPDM LargeResponse extended data
///
typedef struct {
  UINT8    Handle;
} SPDM_ERROR_DATA_LARGE_RESPONSE;

typedef struct {
  SPDM_MESSAGE_HEADER               Header;

  // param1 == Error Code
  // param2 == Error data
  //
  SPDM_ERROR_DATA_LARGE_RESPONSE    ExtendErrorData;
} SPDM_ERROR_RESPONSE_LARGE_RESPONSE;

///
/// SPDM RESPONSE_IF_READY request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RequestCode
  // Param2 == Token
} SPDM_RESPONSE_IF_READY_REQUEST;

///
/// Maximum size of a vendor defined message data length
/// limited by the length field size which is 2 bytes
///
#define SPDM_MAX_VENDOR_DEFINED_DATA_LEN  65535

///
/// Maximum size of a vendor defined vendor id length
/// limited by the length field size which is 1 byte
///
#define SPDM_MAX_VENDOR_ID_LENGTH  255

///
/// SPDM VENDOR_DEFINED request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  UINT16                 StandardID;
  UINT8                  Len;
  // UINT8                VendorID[Len];
  // UINT16               PayloadLength;
  // UINT8                VendorDefinedPayload[PayloadLength];
} SPDM_VENDOR_DEFINED_REQUEST_MSG;

///
/// SPDM VENDOR_DEFINED response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  UINT16                 StandardID;
  UINT8                  Len;
  // UINT8                VendorID[Len];
  // UINT16               PayloadLength;
  // UINT8                VendorDefinedPayload[PayloadLength];
} SPDM_VENDOR_DEFINED_RESPONSE_MSG;

//
// Below command is defined in SPDM 1.1
//

///
/// SPDM KEY_EXCHANGE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == HashType
  // Param2 == SlotNum
  UINT16                 ReqSessionID;
  UINT16                 Reserved;
  UINT8                  RandomData[32];
  // UINT8                ExchangeData[D];
  // UINT16               OpaqueLength;
  // UINT8                OpaqueData[OpaqueLength];
} SPDM_KEY_EXCHANGE_REQUEST;

///
/// SPDM KEY_EXCHANGE request session_policy
///
#define SPDM_KEY_EXCHANGE_REQUEST_SESSION_POLICY_TERMINATION_POLICY_RUNTIME_UPDATE  BIT0

///
/// SPDM KEY_EXCHANGE request measurement summary HashType
///
#define SPDM_KEY_EXCHANGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH \
    SPDM_REQUEST_NO_MEASUREMENT_SUMMARY_HASH
#define SPDM_KEY_EXCHANGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH \
    SPDM_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH
#define SPDM_KEY_EXCHANGE_REQUEST_ALL_MEASUREMENTS_HASH  SPDM_REQUEST_ALL_MEASUREMENTS_HASH

///
/// SPDM KEY_EXCHANGE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == HeartbeatPeriod
  // Param2 == RSVD
  UINT16                 RspSessionID;
  UINT8                  MutAuthRequested;
  UINT8                  ReqSlotIDParam;
  UINT8                  RandomData[32];
  // UINT8                ExchangeData[D];
  // UINT8                MeasurementSummaryHash[DigestSize];
  // UINT16               OpaqueLength;
  // UINT8                OpaqueData[OpaqueLength];
  // UINT8                Signature[S];
  // UINT8                ResponderVerifyData[H];
} SPDM_KEY_EXCHANGE_RESPONSE;

///
/// SPDM KEY_EXCHANGE response MutAuthRequested
///
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED                     BIT0
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_ENCAP_REQUEST  BIT1
#define SPDM_KEY_EXCHANGE_RESPONSE_MUT_AUTH_REQUESTED_WITH_GET_DIGESTS    BIT2

#define SPDM_KEY_EXCHANGE_RESPONSE_SIGN_CONTEXT  "responder-key_exchange_rsp signing"
#define SPDM_KEY_EXCHANGE_RESPONSE_SIGN_CONTEXT_SIZE \
    (sizeof(SPDM_KEY_EXCHANGE_RESPONSE_SIGN_CONTEXT) - 1)

#define SPDM_VERSION_1_2_KEY_EXCHANGE_REQUESTER_CONTEXT  "Requester-KEP-dmtf-spdm-v1.2"
#define SPDM_VERSION_1_2_KEY_EXCHANGE_REQUESTER_CONTEXT_SIZE \
    (sizeof(SPDM_VERSION_1_2_KEY_EXCHANGE_REQUESTER_CONTEXT) - 1)

#define SPDM_VERSION_1_2_KEY_EXCHANGE_RESPONDER_CONTEXT  "Responder-KEP-dmtf-spdm-v1.2"
#define SPDM_VERSION_1_2_KEY_EXCHANGE_RESPONDER_CONTEXT_SIZE \
    (sizeof(SPDM_VERSION_1_2_KEY_EXCHANGE_RESPONDER_CONTEXT) - 1)

///
/// SPDM FINISH request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == SignatureIncluded
  // Param2 == ReqSlotNum
  // UINT8                Signature[S];
  // UINT8                RequesterVerifyData[H];
} SPDM_FINISH_REQUEST;

///
/// SPDM FINISH request SignatureIncluded
///
#define SPDM_FINISH_REQUEST_ATTRIBUTES_SIGNATURE_INCLUDED  BIT0

///
/// SPDM FINISH response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  // UINT8                ResponderVerifyData[H];
} SPDM_FINISH_RESPONSE;

#define SPDM_FINISH_SIGN_CONTEXT       "requester-finish signing"
#define SPDM_FINISH_SIGN_CONTEXT_SIZE  (sizeof(SPDM_FINISH_SIGN_CONTEXT) - 1)

///
/// SPDM PSK_EXCHANGE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == HashType
  // Param2 == RSVD/session_policy (1.2)
  UINT16                 ReqSessionID;
  UINT16                 PSKHintLength;
  UINT16                 RequesterContextLength;
  UINT16                 OpaqueLength;
  // UINT8                PSKHint[PSKHintLength];
  // UINT8                RequesterContext[RequesterContextLength];
  // UINT8                OpaqueData[OpaqueLength];
} SPDM_PSK_EXCHANGE_REQUEST;

///
/// SPDM PSK_EXCHANGE request measurement summary HashType
///
#define SPDM_PSK_EXCHANGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH \
    SPDM_REQUEST_NO_MEASUREMENT_SUMMARY_HASH
#define SPDM_PSK_EXCHANGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH \
    SPDM_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH
#define SPDM_PSK_EXCHANGE_REQUEST_ALL_MEASUREMENTS_HASH  SPDM_REQUEST_ALL_MEASUREMENTS_HASH

///
/// SPDM PSK_EXCHANGE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == HeartbeatPeriod
  // Param2 == RSVD
  UINT16                 RspSessionID;
  UINT16                 Reserved;
  UINT16                 ResponderContextLength;
  UINT16                 OpaqueLength;
  // UINT8                MeasurementSummaryHash[DigestSize];
  // UINT8                ResponderContext[ResponderContextLength];
  // UINT8                OpaqueData[OpaqueLength];
  // UINT8                ResponderVerifyData[H];
} SPDM_PSK_EXCHANGE_RESPONSE;

///
/// SPDM PSK_FINISH request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
  // UINT8                RequesterVerifyData[H];
} SPDM_PSK_FINISH_REQUEST;

///
/// SPDM PSK_FINISH response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_PSK_FINISH_RESPONSE;

///
/// SPDM HEARTBEAT request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_HEARTBEAT_REQUEST;

///
/// SPDM HEARTBEAT response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_HEARTBEAT_RESPONSE;

///
/// SPDM KEY_UPDATE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == KeyOperation
  // Param2 == Tag
} SPDM_KEY_UPDATE_REQUEST;

///
/// SPDM KEY_UPDATE Operations Table
///
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_UPDATE_KEY       1
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_UPDATE_ALL_KEYS  2
#define SPDM_KEY_UPDATE_OPERATIONS_TABLE_VERIFY_NEW_KEY   3

///
/// SPDM KEY_UPDATE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == KeyOperation
  // Param2 == Tag
} SPDM_KEY_UPDATE_RESPONSE;

///
/// SPDM GET_ENCAPSULATED_REQUEST request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_GET_ENCAPSULATED_REQUEST_REQUEST;

///
/// SPDM ENCAPSULATED_REQUEST response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RequestID
  // Param2 == RSVD
  // UINT8                EncapsulatedRequest[];
} SPDM_ENCAPSULATED_REQUEST_RESPONSE;

///
/// SPDM DELIVER_ENCAPSULATED_RESPONSE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RequestID
  // Param2 == RSVD
  // UINT8                EncapsulatedResponse[];
} SPDM_DELIVER_ENCAPSULATED_RESPONSE_REQUEST;

///
/// SPDM ENCAPSULATED_RESPONSE_ACK response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RequestID
  // Param2 == PayloadType
  // below 4 bytes are added in 1.2.
  UINT8                  AckRequestId;
  UINT8                  Reserved[3];
  // UINT8                EncapsulatedRequest[];
} SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE;

///
/// SPDM ENCAPSULATED_RESPONSE_ACK_RESPONSE Payload Type
///
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_ABSENT           0
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_PRESENT          1
#define SPDM_ENCAPSULATED_RESPONSE_ACK_RESPONSE_PAYLOAD_TYPE_REQ_SLOT_NUMBER  2

///
/// SPDM END_SESSION request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == EndSessionRequestAttributes
  // Param2 == RSVD
} SPDM_END_SESSION_REQUEST;

///
/// SPDM END_SESSION request Attributes
///
#define SPDM_END_SESSION_REQUEST_ATTRIBUTES_PRESERVE_NEGOTIATED_STATE_CLEAR  BIT0

///
/// SPDM END_SESSION response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  // Param1 == RSVD
  // Param2 == RSVD
} SPDM_END_SESSION_RESPONSE;

//
// Below command is defined in SPDM 1.2
//

///
/// SPDM SET_CERTIFICATE request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 == BIT[0:3]=slot_id, BIT[4:7]=RSVD
  // param2 == RSVD
  // param1 and param2 are updated in 1.3
  // param1 == Request attributes, BIT[0:3]=slot_id, BIT[4:6]=SetCertModel, BIT[7]=Erase
  // param2 == KeyPairID
  // void * CertChain
} SPDM_SET_CERTIFICATE_REQUEST;

#define SPDM_SET_CERTIFICATE_REQUEST_SLOT_ID_MASK  0xF

///
/// SPDM SET_CERTIFICATE request Attributes
///
#define SPDM_SET_CERTIFICATE_REQUEST_ATTRIBUTES_CERT_MODEL_MASK    0x70
#define SPDM_SET_CERTIFICATE_REQUEST_ATTRIBUTES_CERT_MODEL_OFFSET  4
#define SPDM_SET_CERTIFICATE_REQUEST_ATTRIBUTES_ERASE              0x80

///
/// SPDM SET_CERTIFICATE_RSP response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 == BIT[0:3]=slot_id, BIT[4:7]=RSVD
  // param2 == RSVD
} SPDM_SET_CERTIFICATE_RESPONSE;

#define SPDM_SET_CERTIFICATE_RESPONSE_SLOT_ID_MASK  0xF

///
/// SPDM GET_CSR request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;
  UINT16                 RequesterInfoLength;
  UINT16                 OpaqueDataLength;

  // UINT8 RequesterInfo[RequesterInfoLength];
  // UINT8 OpaqueData[OpaqueDataLength];
} SPDM_GET_CSR_REQUEST;

///
/// SPDM GET_CSR request Attributes
///
#define SPDM_GET_CSR_REQUEST_ATTRIBUTES_CERT_MODEL_MASK          0x07
#define SPDM_GET_CSR_REQUEST_ATTRIBUTES_CSR_TRACKING_TAG_MASK    0x38
#define SPDM_GET_CSR_REQUEST_ATTRIBUTES_CSR_TRACKING_TAG_OFFSET  3
#define SPDM_GET_CSR_REQUEST_ATTRIBUTES_OVERWRITE                0x80
#define SPDM_GET_CSR_REQUEST_ATTRIBUTES_MAX_CSR_CERT_MODEL       4

///
/// Maximum size, in bytes, of a CSR.
///
#define SPDM_MAX_CSR_SIZE  65535

///
/// SPDM CSR response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 == RSVD
  // param2 == RSVD
  UINT16                 CsrLength;
  UINT16                 Reserved;
} SPDM_CSR_RESPONSE;

///
/// SPDM CHUNK_SEND request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 - Request Attributes
  // param2 - Handle
  UINT16                 ChunkSeqNo;
  UINT16                 Reserved;
  UINT32                 ChunkSize;

  // UINT32 LargeMessageSize;
  // UINT8  SpdmChunk[ChunkSize];
} SPDM_CHUNK_SEND_REQUEST;

#define SPDM_CHUNK_SEND_REQUEST_ATTRIBUTE_LAST_CHUNK  (1 << 0)

///
/// SPDM CHUNK_SEND_ACK response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 - Response Attributes
  // param2 - Handle
  UINT16                 ChunkSeqNo;
  // UINT8 response_to_large_request[variable]
} SPDM_CHUNK_SEND_ACK_RESPONSE;

#define SPDM_CHUNK_SEND_ACK_RESPONSE_ATTRIBUTE_EARLY_ERROR_DETECTED  (1 << 0)

///
/// SPDM CHUNK_GET request
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 - Reserved
  // param2 - Handle
  UINT16                 ChunkSeqNo;
} SPDM_CHUNK_GET_REQUEST;

///
/// SPDM CHUNK_RESPONSE response
///
typedef struct {
  SPDM_MESSAGE_HEADER    Header;

  // param1 - Response Attributes
  // param2 - Handle
  UINT16                 ChunkSeqNo;
  UINT16                 Reserved;
  UINT32                 ChunkSize;

  // UINT32 LargeMessageSize;
  // UINT8  SpdmChunk[ChunkSize];
} SPDM_CHUNK_RESPONSE_RESPONSE;

#define SPDM_CHUNK_GET_RESPONSE_ATTRIBUTE_LAST_CHUNK  (1 << 0)
#pragma pack()

#define SPDM_VERSION_1_1_BIN_CONCAT_LABEL  "spdm1.1 "
#define SPDM_VERSION_1_2_BIN_CONCAT_LABEL  "spdm1.2 "
#define SPDM_BIN_STR_0_LABEL               "derived"
#define SPDM_BIN_STR_1_LABEL               "req hs data"
#define SPDM_BIN_STR_2_LABEL               "rsp hs data"
#define SPDM_BIN_STR_3_LABEL               "req app data"
#define SPDM_BIN_STR_4_LABEL               "rsp app data"
#define SPDM_BIN_STR_5_LABEL               "key"
#define SPDM_BIN_STR_6_LABEL               "iv"
#define SPDM_BIN_STR_7_LABEL               "finished"
#define SPDM_BIN_STR_8_LABEL               "exp master"
#define SPDM_BIN_STR_9_LABEL               "traffic upd"

///
/// The maximum amount of time in microseconds the Responder has to provide a response
/// to requests that do not require cryptographic processing.
///
#define SPDM_ST1_VALUE_US  100000

///
/// id-DMTF 1.3.6.1.4.1.412.
/// These OID are defiend in ANNEX C (informative) OID reference section from the DMTF SPDM spec.
/// https://www.dmtf.org/sites/default/files/standards/documents/DSP0274_1.2.2.pdf
///
#define SPDM_OID_DMTF \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C }
// id-DMTF-spdm, { id-DMTF 274 }, 1.3.6.1.4.1.412.274
#define SPDM_OID_DMTF_SPDM \
    {0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12 }
// id-DMTF-device-info, { id-DMTF-spdm 1 }, 1.3.6.1.4.1.412.274.1
#define SPDM_OID_DMTF_DEVICE_INFO \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x01 }
// id-DMTF-hardware-identity, { id-DMTF-spdm 2 }, 1.3.6.1.4.1.412.274.2
#define SPDM_OID_DMTF_HARDWARE_IDENTITY \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x02 }
// id-DMTF-eku-responder-auth, { id-DMTF-spdm 3 }, 1.3.6.1.4.1.412.274.3
#define SPDM_OID_DMTF_EKU_RESPONDER_AUTH \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x03 }
// id-DMTF-eku-requester-auth, { id-DMTF-spdm 4 }, 1.3.6.1.4.1.412.274.4
#define SPDM_OID_DMTF_EKU_REQUESTER_AUTH \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x04 }
// id-DMTF-mutable-certificate, { id-DMTF-spdm 5 }, 1.3.6.1.4.1.412.274.5
#define SPDM_OID_DMTF_MUTABLE_CERTIFICATE \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x05 }
// id-DMTF-SPDM-extension, { id-DMTF-spdm 6 }, 1.3.6.1.4.1.412.274.6
#define SPDM_OID_DMTF_SPDM_EXTENSION \
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x83, 0x1C, 0x82, 0x12, 0x06 }
#endif
