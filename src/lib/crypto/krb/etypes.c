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

/* these will be linear searched.  if they ever get big, a binary
   search or hash table would be better, which means these would need
   to be sorted.  An array would be more efficient, but that assumes
   that the keytypes are all near each other.  I'd rather not make
   that assumption. */

const struct krb5_keytypes krb5int_enctypes_list[] = {
    { ENCTYPE_DES_CBC_CRC,
      "des-cbc-crc", { 0 }, "DES cbc mode with CRC-32",
      &krb5int_enc_des, &krb5int_hash_crc32,
      16,
      krb5int_old_crypto_length, krb5int_old_encrypt, krb5int_old_decrypt,
      krb5int_des_string_to_key, k5_rand2key_des,
      krb5int_des_prf,
      CKSUMTYPE_RSA_MD5_DES,
      ETYPE_WEAK, 56 },
    { ENCTYPE_DES_CBC_MD4,
      "des-cbc-md4", { 0 }, "DES cbc mode with RSA-MD4",
      &krb5int_enc_des, &krb5int_hash_md4,
      16,
      krb5int_old_crypto_length, krb5int_old_encrypt, krb5int_old_decrypt,
      krb5int_des_string_to_key, k5_rand2key_des,
      krb5int_des_prf,
      CKSUMTYPE_RSA_MD4_DES,
      ETYPE_WEAK, 56 },
    { ENCTYPE_DES_CBC_MD5,
      "des-cbc-md5", { "des" }, "DES cbc mode with RSA-MD5",
      &krb5int_enc_des, &krb5int_hash_md5,
      16,
      krb5int_old_crypto_length, krb5int_old_encrypt, krb5int_old_decrypt,
      krb5int_des_string_to_key, k5_rand2key_des,
      krb5int_des_prf,
      CKSUMTYPE_RSA_MD5_DES,
      ETYPE_WEAK, 56 },
    { ENCTYPE_DES_CBC_RAW,
      "des-cbc-raw", { 0 }, "DES cbc mode raw",
      &krb5int_enc_des, NULL,
      16,
      krb5int_raw_crypto_length, krb5int_raw_encrypt, krb5int_raw_decrypt,
      krb5int_des_string_to_key, k5_rand2key_des,
      krb5int_des_prf,
      0,
      ETYPE_WEAK, 56 },
    { ENCTYPE_DES3_CBC_RAW,
      "des3-cbc-raw", { 0 }, "Triple DES cbc mode raw",
      &krb5int_enc_des3, NULL,
      16,
      krb5int_raw_crypto_length, krb5int_raw_encrypt, krb5int_raw_decrypt,
      krb5int_dk_string_to_key, k5_rand2key_des3,
      NULL, /*PRF*/
      0,
      ETYPE_WEAK, 112 },

    { ENCTYPE_DES3_CBC_SHA1,
      "des3-cbc-sha1", { "des3-hmac-sha1", "des3-cbc-sha1-kd" },
      "Triple DES cbc mode with HMAC/sha1",
      &krb5int_enc_des3, &krb5int_hash_sha1,
      16,
      krb5int_dk_crypto_length, krb5int_dk_encrypt, krb5int_dk_decrypt,
      krb5int_dk_string_to_key, k5_rand2key_des3,
      krb5int_dk_prf,
      CKSUMTYPE_HMAC_SHA1_DES3,
      0 /*flags*/, 112 },

    { ENCTYPE_DES_HMAC_SHA1,
      "des-hmac-sha1", { 0 }, "DES with HMAC/sha1",
      &krb5int_enc_des, &krb5int_hash_sha1,
      8,
      krb5int_dk_crypto_length, krb5int_dk_encrypt, krb5int_dk_decrypt,
      krb5int_dk_string_to_key, k5_rand2key_des,
      NULL, /*PRF*/
      0,
      ETYPE_WEAK, 56 },

    /* rc4-hmac uses a 128-bit key, but due to weaknesses in the RC4 cipher, we
     * consider its strength degraded and assign it an SSF value of 64. */
    { ENCTYPE_ARCFOUR_HMAC,
      "arcfour-hmac", { "rc4-hmac", "arcfour-hmac-md5" },
      "ArcFour with HMAC/md5",
      &krb5int_enc_arcfour,
      &krb5int_hash_md5,
      20,
      krb5int_arcfour_crypto_length, krb5int_arcfour_encrypt,
      krb5int_arcfour_decrypt, krb5int_arcfour_string_to_key,
      k5_rand2key_direct, krb5int_arcfour_prf,
      CKSUMTYPE_HMAC_MD5_ARCFOUR,
      0 /*flags*/, 64 },
    { ENCTYPE_ARCFOUR_HMAC_EXP,
      "arcfour-hmac-exp", { "rc4-hmac-exp", "arcfour-hmac-md5-exp" },
      "Exportable ArcFour with HMAC/md5",
      &krb5int_enc_arcfour,
      &krb5int_hash_md5,
      20,
      krb5int_arcfour_crypto_length, krb5int_arcfour_encrypt,
      krb5int_arcfour_decrypt, krb5int_arcfour_string_to_key,
      k5_rand2key_direct, krb5int_arcfour_prf,
      CKSUMTYPE_HMAC_MD5_ARCFOUR,
      ETYPE_WEAK, 40
    },

    { ENCTYPE_AES128_CTS_HMAC_SHA1_96,
      "aes128-cts-hmac-sha1-96", { "aes128-cts", "aes128-sha1" },
      "AES-128 CTS mode with 96-bit SHA-1 HMAC",
      &krb5int_enc_aes128, &krb5int_hash_sha1,
      16,
      krb5int_aes_crypto_length, krb5int_dk_encrypt, krb5int_dk_decrypt,
      krb5int_aes_string_to_key, k5_rand2key_direct,
      krb5int_dk_prf,
      CKSUMTYPE_HMAC_SHA1_96_AES128,
      0 /*flags*/, 128 },
    { ENCTYPE_AES256_CTS_HMAC_SHA1_96,
      "aes256-cts-hmac-sha1-96", { "aes256-cts", "aes256-sha1" },
      "AES-256 CTS mode with 96-bit SHA-1 HMAC",
      &krb5int_enc_aes256, &krb5int_hash_sha1,
      16,
      krb5int_aes_crypto_length, krb5int_dk_encrypt, krb5int_dk_decrypt,
      krb5int_aes_string_to_key, k5_rand2key_direct,
      krb5int_dk_prf,
      CKSUMTYPE_HMAC_SHA1_96_AES256,
      0 /*flags*/, 256 },

    { ENCTYPE_CAMELLIA128_CTS_CMAC,
      "camellia128-cts-cmac", { "camellia128-cts" },
      "Camellia-128 CTS mode with CMAC",
      &krb5int_enc_camellia128, NULL,
      16,
      krb5int_camellia_crypto_length,
      krb5int_dk_cmac_encrypt, krb5int_dk_cmac_decrypt,
      krb5int_camellia_string_to_key, k5_rand2key_direct,
      krb5int_dk_cmac_prf,
      CKSUMTYPE_CMAC_CAMELLIA128,
      0 /*flags*/, 128 },
    { ENCTYPE_CAMELLIA256_CTS_CMAC,
      "camellia256-cts-cmac", { "camellia256-cts" },
      "Camellia-256 CTS mode with CMAC",
      &krb5int_enc_camellia256, NULL,
      16,
      krb5int_camellia_crypto_length,
      krb5int_dk_cmac_encrypt, krb5int_dk_cmac_decrypt,
      krb5int_camellia_string_to_key, k5_rand2key_direct,
      krb5int_dk_cmac_prf,
      CKSUMTYPE_CMAC_CAMELLIA256,
      0 /*flags */, 256 },

    { ENCTYPE_AES128_CTS_HMAC_SHA256_128,
      "aes128-cts-hmac-sha256-128", { "aes128-sha2" },
      "AES-128 CTS mode with 128-bit SHA-256 HMAC",
      &krb5int_enc_aes128, &krb5int_hash_sha256,
      32,
      krb5int_aes2_crypto_length, krb5int_etm_encrypt, krb5int_etm_decrypt,
      krb5int_aes2_string_to_key, k5_rand2key_direct,
      krb5int_aes2_prf,
      CKSUMTYPE_HMAC_SHA256_128_AES128,
      0 /*flags*/, 128 },
    { ENCTYPE_AES256_CTS_HMAC_SHA384_192,
      "aes256-cts-hmac-sha384-192", { "aes256-sha2" },
      "AES-256 CTS mode with 192-bit SHA-384 HMAC",
      &krb5int_enc_aes256, &krb5int_hash_sha384,
      48,
      krb5int_aes2_crypto_length, krb5int_etm_encrypt, krb5int_etm_decrypt,
      krb5int_aes2_string_to_key, k5_rand2key_direct,
      krb5int_aes2_prf,
      CKSUMTYPE_HMAC_SHA384_192_AES256,
      0 /*flags*/, 256 },
};

const int krb5int_enctypes_length =
    sizeof(krb5int_enctypes_list) / sizeof(struct krb5_keytypes);
