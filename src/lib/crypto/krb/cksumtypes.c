/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "crypto_int.h"

const struct krb5_cksumtypes krb5int_cksumtypes_list[] = {
    { CKSUMTYPE_CRC32,
      "crc32", { 0 }, "CRC-32",
      NULL, &krb5int_hash_crc32,
      krb5int_unkeyed_checksum, NULL,
      4, 4, CKSUM_UNKEYED | CKSUM_NOT_COLL_PROOF },

    { CKSUMTYPE_RSA_MD4,
      "md4", { 0 }, "RSA-MD4",
      NULL, &krb5int_hash_md4,
      krb5int_unkeyed_checksum, NULL,
      16, 16, CKSUM_UNKEYED },

    { CKSUMTYPE_RSA_MD4_DES,
      "md4-des", { 0 }, "RSA-MD4 with DES cbc mode",
      &krb5int_enc_des, &krb5int_hash_md4,
      krb5int_confounder_checksum, krb5int_confounder_verify,
      24, 24, 0 },

    { CKSUMTYPE_DESCBC,
      "des-cbc", { 0 }, "DES cbc mode",
      &krb5int_enc_des, NULL,
      krb5int_cbc_checksum, NULL,
      8, 8, 0 },

    { CKSUMTYPE_RSA_MD5,
      "md5", { 0 }, "RSA-MD5",
      NULL, &krb5int_hash_md5,
      krb5int_unkeyed_checksum, NULL,
      16, 16, CKSUM_UNKEYED },

    { CKSUMTYPE_RSA_MD5_DES,
      "md5-des", { 0 }, "RSA-MD5 with DES cbc mode",
      &krb5int_enc_des, &krb5int_hash_md5,
      krb5int_confounder_checksum, krb5int_confounder_verify,
      24, 24, 0 },

    { CKSUMTYPE_NIST_SHA,
      "sha", { 0 }, "NIST-SHA",
      NULL, &krb5int_hash_sha1,
      krb5int_unkeyed_checksum, NULL,
      20, 20, CKSUM_UNKEYED },

    { CKSUMTYPE_HMAC_SHA1_DES3,
      "hmac-sha1-des3", { "hmac-sha1-des3-kd" }, "HMAC-SHA1 DES3 key",
      &krb5int_enc_des3, &krb5int_hash_sha1,
      krb5int_dk_checksum, NULL,
      20, 20, 0 },

    { CKSUMTYPE_HMAC_MD5_ARCFOUR,
      "hmac-md5-rc4", { "hmac-md5-enc", "hmac-md5-earcfour" },
      "Microsoft HMAC MD5",
      NULL, &krb5int_hash_md5,
      krb5int_hmacmd5_checksum, NULL,
      16, 16, 0 },

    { CKSUMTYPE_HMAC_SHA1_96_AES128,
      "hmac-sha1-96-aes128", { 0 }, "HMAC-SHA1 AES128 key",
      &krb5int_enc_aes128, &krb5int_hash_sha1,
      krb5int_dk_checksum, NULL,
      20, 12, 0 },

    { CKSUMTYPE_HMAC_SHA1_96_AES256,
      "hmac-sha1-96-aes256", { 0 }, "HMAC-SHA1 AES256 key",
      &krb5int_enc_aes256, &krb5int_hash_sha1,
      krb5int_dk_checksum, NULL,
      20, 12, 0 },

    { CKSUMTYPE_MD5_HMAC_ARCFOUR,
      "md5-hmac-rc4", { 0 }, "Microsoft MD5 HMAC",
      &krb5int_enc_arcfour, &krb5int_hash_md5,
      krb5int_hmacmd5_checksum, NULL,
      16, 16, 0 },

    { CKSUMTYPE_CMAC_CAMELLIA128,
      "cmac-camellia128", { 0 }, "CMAC Camellia128 key",
      &krb5int_enc_camellia128, NULL,
      krb5int_dk_cmac_checksum, NULL,
      16, 16, 0 },

    { CKSUMTYPE_CMAC_CAMELLIA256,
      "cmac-camellia256", { 0 }, "CMAC Camellia256 key",
      &krb5int_enc_camellia256, NULL,
      krb5int_dk_cmac_checksum, NULL,
      16, 16, 0 },

    { CKSUMTYPE_HMAC_SHA256_128_AES128,
      "hmac-sha256-128-aes128", { 0 }, "HMAC-SHA256 AES128 key",
      &krb5int_enc_aes128, &krb5int_hash_sha256,
      krb5int_etm_checksum, NULL,
      32, 16, 0 },

    { CKSUMTYPE_HMAC_SHA384_192_AES256,
      "hmac-sha384-192-aes256", { 0 }, "HMAC-SHA384 AES256 key",
      &krb5int_enc_aes256, &krb5int_hash_sha384,
      krb5int_etm_checksum, NULL,
      48, 24, 0 },
};

const size_t krb5int_cksumtypes_length =
    sizeof(krb5int_cksumtypes_list) / sizeof(struct krb5_cksumtypes);
