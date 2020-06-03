/** @file
  Transport Layer Security  -- TLS 1.0/1.1/1.2 Standard definitions, from RFC 2246/4346/5246

  This file contains common TLS 1.0/1.1/1.2 definitions from RFC 2246/4346/5246

  Copyright (c) 2016 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __TLS_1_H__
#define __TLS_1_H__

#pragma pack(1)

///
/// TLS Cipher Suite, refers to A.5 of rfc-2246, rfc-4346 and rfc-5246.
///
#define TLS_RSA_WITH_NULL_MD5                    {0x00, 0x01}
#define TLS_RSA_WITH_NULL_SHA                    {0x00, 0x02}
#define TLS_RSA_WITH_RC4_128_MD5                 {0x00, 0x04}
#define TLS_RSA_WITH_RC4_128_SHA                 {0x00, 0x05}
#define TLS_RSA_WITH_IDEA_CBC_SHA                {0x00, 0x07}
#define TLS_RSA_WITH_DES_CBC_SHA                 {0x00, 0x09}
#define TLS_RSA_WITH_3DES_EDE_CBC_SHA            {0x00, 0x0A}
#define TLS_DH_DSS_WITH_DES_CBC_SHA              {0x00, 0x0C}
#define TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA         {0x00, 0x0D}
#define TLS_DH_RSA_WITH_DES_CBC_SHA              {0x00, 0x0F}
#define TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA         {0x00, 0x10}
#define TLS_DHE_DSS_WITH_DES_CBC_SHA             {0x00, 0x12}
#define TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA        {0x00, 0x13}
#define TLS_DHE_RSA_WITH_DES_CBC_SHA             {0x00, 0x15}
#define TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA        {0x00, 0x16}
#define TLS_RSA_WITH_AES_128_CBC_SHA             {0x00, 0x2F}
#define TLS_DH_DSS_WITH_AES_128_CBC_SHA          {0x00, 0x30}
#define TLS_DH_RSA_WITH_AES_128_CBC_SHA          {0x00, 0x31}
#define TLS_DHE_DSS_WITH_AES_128_CBC_SHA         {0x00, 0x32}
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA         {0x00, 0x33}
#define TLS_RSA_WITH_AES_256_CBC_SHA             {0x00, 0x35}
#define TLS_DH_DSS_WITH_AES_256_CBC_SHA          {0x00, 0x36}
#define TLS_DH_RSA_WITH_AES_256_CBC_SHA          {0x00, 0x37}
#define TLS_DHE_DSS_WITH_AES_256_CBC_SHA         {0x00, 0x38}
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA         {0x00, 0x39}
#define TLS_RSA_WITH_NULL_SHA256                 {0x00, 0x3B}
#define TLS_RSA_WITH_AES_128_CBC_SHA256          {0x00, 0x3C}
#define TLS_RSA_WITH_AES_256_CBC_SHA256          {0x00, 0x3D}
#define TLS_DH_DSS_WITH_AES_128_CBC_SHA256       {0x00, 0x3E}
#define TLS_DH_RSA_WITH_AES_128_CBC_SHA256       {0x00, 0x3F}
#define TLS_DHE_DSS_WITH_AES_128_CBC_SHA256      {0x00, 0x40}
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256      {0x00, 0x67}
#define TLS_DH_DSS_WITH_AES_256_CBC_SHA256       {0x00, 0x68}
#define TLS_DH_RSA_WITH_AES_256_CBC_SHA256       {0x00, 0x69}
#define TLS_DHE_DSS_WITH_AES_256_CBC_SHA256      {0x00, 0x6A}
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256      {0x00, 0x6B}

///
/// TLS Version, refers to A.1 of rfc-2246, rfc-4346 and rfc-5246.
///
#define TLS10_PROTOCOL_VERSION_MAJOR  0x03
#define TLS10_PROTOCOL_VERSION_MINOR  0x01
#define TLS11_PROTOCOL_VERSION_MAJOR  0x03
#define TLS11_PROTOCOL_VERSION_MINOR  0x02
#define TLS12_PROTOCOL_VERSION_MAJOR  0x03
#define TLS12_PROTOCOL_VERSION_MINOR  0x03

///
/// TLS Content Type, refers to A.1 of rfc-2246, rfc-4346 and rfc-5246.
///
typedef enum {
  TlsContentTypeChangeCipherSpec = 20,
  TlsContentTypeAlert            = 21,
  TlsContentTypeHandshake        = 22,
  TlsContentTypeApplicationData  = 23,
} TLS_CONTENT_TYPE;

///
/// TLS Record Header, refers to A.1 of rfc-2246, rfc-4346 and rfc-5246.
///
typedef struct {
  UINT8                   ContentType;
  EFI_TLS_VERSION         Version;
  UINT16                  Length;
} TLS_RECORD_HEADER;

#define TLS_RECORD_HEADER_LENGTH   5

//
// The length (in bytes) of the TLSPlaintext records payload MUST NOT exceed 2^14.
// Refers to section 6.2 of RFC5246.
//
#define TLS_PLAINTEXT_RECORD_MAX_PAYLOAD_LENGTH   16384

//
// The length (in bytes) of the TLSCiphertext records payload MUST NOT exceed 2^14 + 2048.
// Refers to section 6.2 of RFC5246.
//
#define TLS_CIPHERTEXT_RECORD_MAX_PAYLOAD_LENGTH   18432

#pragma pack()

#endif

