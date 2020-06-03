/** @file
  Definitions of Security Protocol & Data Model Specification (SPDM)
  version 1.0.0 in Distributed Management Task Force (DMTF).

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef __SPDM_H__
#define __SPDM_H__

#pragma pack(1)

///
/// SPDM response code
///
#define SPDM_DIGESTS               0x01
#define SPDM_CERTIFICATE           0x02
#define SPDM_CHALLENGE_AUTH        0x03
#define SPDM_VERSION               0x04
#define SPDM_MEASUREMENTS          0x60
#define SPDM_CAPABILITIES          0x61
#define SPDM_SET_CERT_RESPONSE     0x62
#define SPDM_ALGORITHMS            0x63
#define SPDM_ERROR                 0x7F
///
/// SPDM request code
///
#define SPDM_GET_DIGESTS           0x81
#define SPDM_GET_CERTIFICATE       0x82
#define SPDM_CHALLENGE             0x83
#define SPDM_GET_VERSION           0x84
#define SPDM_GET_MEASUREMENTS      0xE0
#define SPDM_GET_CAPABILITIES      0xE1
#define SPDM_NEGOTIATE_ALGORITHMS  0xE3
#define SPDM_RESPOND_IF_READY      0xFF

///
/// SPDM message header
///
typedef struct {
  UINT8   SPDMVersion;
  UINT8   RequestResponseCode;
  UINT8   Param1;
  UINT8   Param2;
} SPDM_MESSAGE_HEADER;

#define SPDM_MESSAGE_VERSION  0x10

///
/// SPDM GET_VERSION request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
} SPDM_GET_VERSION_REQUEST;

///
/// SPDM GET_VERSION response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                Reserved;
  UINT8                VersionNumberEntryCount;
//SPDM_VERSION_NUMBER  VersionNumberEntry[VersionNumberEntryCount];
} SPDM_VERSION_RESPONSE;

///
/// SPDM VERSION structure
///
typedef struct {
  UINT16               Alpha:4;
  UINT16               UpdateVersionNumber:4;
  UINT16               MinorVersion:4;
  UINT16               MajorVersion:4;
} SPDM_VERSION_NUMBER;

///
/// SPDM GET_CAPABILITIES request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
} SPDM_GET_CAPABILITIES_REQUEST;

///
/// SPDM GET_CAPABILITIES response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                Reserved;
  UINT8                CTExponent;
  UINT16               Reserved2;
  UINT32               Flags;
} SPDM_CAPABILITIES_RESPONSE;

///
/// SPDM GET_CAPABILITIES response Flags
///
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CACHE_CAP       BIT0
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CERT_CAP        BIT1
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHAL_CAP        BIT2
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP        (BIT3 | BIT4)
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_NO_SIG   BIT3
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_CAP_SIG      BIT4
#define SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_MEAS_FRESH_CAP  BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT16               Length;
  UINT8                MeasurementSpecification;
  UINT8                Reserved;
  UINT32               BaseAsymAlgo;
  UINT32               BaseHashAlgo;
  UINT8                Reserved2[12];
  UINT8                ExtAsymCount;
  UINT8                ExtHashCount;
  UINT16               Reserved3;
//UINT32               ExtAsym[ExtAsymCount];
//UINT32               ExtHash[ExtHashCount];
} SPDM_NEGOTIATE_ALGORITHMS_REQUEST;

///
/// SPDM NEGOTIATE_ALGORITHMS request BaseAsymAlgo
///
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_2048           BIT0
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_2048           BIT1
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_3072           BIT2
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_3072           BIT3
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P256   BIT4
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSASSA_4096           BIT5
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_RSAPSS_4096           BIT6
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P384   BIT7
#define SPDM_ALGORITHMS_BASE_ASYM_ALGO_TPM_ALG_ECDSA_ECC_NIST_P521   BIT8

///
/// SPDM NEGOTIATE_ALGORITHMS request BaseHashAlgo
///
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_256               BIT0
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_384               BIT1
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA_512               BIT2
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_256              BIT3
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_384              BIT4
#define SPDM_ALGORITHMS_BASE_HASH_ALGO_TPM_ALG_SHA3_512              BIT5

///
/// SPDM NEGOTIATE_ALGORITHMS response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT16               Length;
  UINT8                MeasurementSpecificationSel;
  UINT8                Reserved;
  UINT32               MeasurementHashAlgo;
  UINT32               BaseAsymSel;
  UINT32               BaseHashSel;
  UINT8                Reserved2[12];
  UINT8                ExtAsymSelCount;
  UINT8                ExtHashSelCount;
  UINT16               Reserved3;
//UINT32               ExtAsymSel[ExtAsymSelCount];
//UINT32               ExtHashSel[ExtHashSelCount];
} SPDM_ALGORITHMS_RESPONSE;

///
/// SPDM NEGOTIATE_ALGORITHMS response MeasurementHashAlgo
///
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_RAW_BIT_STREAM_ONLY BIT0
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_256     BIT1
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_384     BIT2
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA_512     BIT3
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_256    BIT4
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_384    BIT5
#define SPDM_ALGORITHMS_MEASUREMENT_HASH_ALGO_TPM_ALG_SHA3_512    BIT6

///
/// SPDM GET_DIGESTS request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
} SPDM_GET_DIGESTS_REQUEST;

///
/// SPDM GET_DIGESTS response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
//UINT8                Digest[DigestSize];
} SPDM_DIGESTS_RESPONSE;

///
/// SPDM GET_DIGESTS request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT16               Offset;
  UINT16               Length;
} SPDM_GET_CERTIFICATE_REQUEST;

///
/// SPDM GET_DIGESTS response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT16               PortionLength;
  UINT16               RemainderLength;
//UINT8                CertChain[CertChainSize];
} SPDM_CERTIFICATE_RESPONSE;

///
/// SPDM CHALLENGE request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                Nonce[32];
} SPDM_CHALLENGE_REQUEST;

///
/// SPDM CHALLENGE response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
//UINT8                CertChainHash[DigestSize];
//UINT8                Nonce[32];
//UINT8                MeasurementSummaryHash[DigestSize];
//UINT16               OpaqueLength;
//UINT8                OpaqueData[OpaqueLength];
//UINT8                Signature[KeySize];
} SPDM_CHALLENGE_AUTH_RESPONSE;

///
/// SPDM GET_MEASUREMENTS request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                Nonce[32];
} SPDM_GET_MEASUREMENTS_REQUEST;

///
/// SPDM MEASUREMENTS block common header
///
typedef struct {
  UINT8                Index;
  UINT8                MeasurementSpecification;
  UINT16               MeasurementSize;
//UINT8                Measurement[MeasurementSize];
} SPDM_MEASUREMENT_BLOCK_COMMON_HEADER;

#define SPDM_MEASUREMENT_BLOCK_HEADER_SPECIFICATION_DMTF BIT0

///
/// SPDM MEASUREMENTS block DMTF header
///
typedef struct {
  UINT8                DMTFSpecMeasurementValueType;
  UINT16               DMTFSpecMeasurementValueSize;
//UINT8                DMTFSpecMeasurementValue[DMTFSpecMeasurementValueSize];
} SPDM_MEASUREMENT_BLOCK_DMTF_HEADER;

///
/// SPDM MEASUREMENTS block MeasurementValueType
///
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_IMMUTABLE_ROM           0
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MUTABLE_FIRMWARE        1
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_HARDWARE_CONFIGURATION  2
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_FIRMWARE_CONFIGURATION  3
#define SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_RAW_BIT_STREAM          BIT7

///
/// SPDM GET_MEASUREMENTS response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                NumberOfBlocks;
  UINT8                MeasurementRecordLength[3];
//UINT8                MeasurementRecord[MeasurementRecordLength];
//UINT8                Nonce[32];
//UINT16               OpaqueLength;
//UINT8                OpaqueData[OpaqueLength];
//UINT8                Signature[KeySize];
} SPDM_MEASUREMENTS_RESPONSE;

///
/// SPDM ERROR response
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  // Param1 == Error Code
  // Param2 == Error Data
//UINT8                ExtendedErrorData[];
} SPDM_ERROR_RESPONSE;

///
/// SPDM error code
///
#define SPDM_ERROR_CODE_INVALID_REQUEST         0x01
#define SPDM_ERROR_CODE_BUSY                    0x03
#define SPDM_ERROR_CODE_UNEXPECTED_REQUEST      0x04
#define SPDM_ERROR_CODE_UNSPECIFIED             0x05
#define SPDM_ERROR_CODE_UNSUPPORTED_REQUEST     0x07
#define SPDM_ERROR_CODE_MAJOR_VERSION_MISMATCH  0x41
#define SPDM_ERROR_CODE_RESPONSE_NOT_READY      0x42
#define SPDM_ERROR_CODE_REQUEST_RESYNCH         0x43

///
/// SPDM RESPONSE_IF_READY request
///
typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  // Param1 == RequestCode
  // Param2 == Token
} SPDM_RESPONSE_IF_READY_REQUEST;

#pragma pack()

#endif

