/* lib/crypto/builtin/camellia/camellia.h - Camellia version 1.2.0 */
/*
 * Copyright (c) 2006,2007,2009
 * NTT (Nippon Telegraph and Telephone Corporation) . All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer as
 *   the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NTT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NTT BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HEADER_CAMELLIA_H
#define HEADER_CAMELLIA_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CAMELLIA_BLOCK_SIZE 16
#define CAMELLIA_TABLE_BYTE_LEN 272
#define CAMELLIA_TABLE_WORD_LEN (CAMELLIA_TABLE_BYTE_LEN / 4)

#ifndef BLOCK_SIZE
#define BLOCK_SIZE CAMELLIA_BLOCK_SIZE
#endif

typedef unsigned int KEY_TABLE_TYPE[CAMELLIA_TABLE_WORD_LEN];

/* u32 must be 32bit word */
typedef uint32_t u32;
typedef uint8_t u8;

/* For the Kerberos 5 tree, hide the Camellia symbol names. */
#define camellia_setup128      k5_camellia_setup128
#define camellia_setup192      k5_camellia_setup192
#define camellia_setup256      k5_camellia_setup256
#define camellia_encrypt128    k5_camellia_encrypt128
#define camellia_decrypt128    k5_camellia_decrypt128
#define camellia_encrypt256    k5_camellia_encrypt256
#define camellia_decrypt256    k5_camellia_decrypt256
#define Camellia_Ekeygen       k5_Camellia_Ekeygen
#define Camellia_EncryptBlock  k5_Camellia_EncryptBlock
#define Camellia_DecryptBlock  k5_Camellia_DecryptBlock

void camellia_setup128(const unsigned char *key, u32 *subkey);
void camellia_setup192(const unsigned char *key, u32 *subkey);
void camellia_setup256(const unsigned char *key, u32 *subkey);
void camellia_encrypt128(const u32 *subkey, u32 *io);
void camellia_decrypt128(const u32 *subkey, u32 *io);
void camellia_encrypt256(const u32 *subkey, u32 *io);
void camellia_decrypt256(const u32 *subkey, u32 *io);

void Camellia_Ekeygen(const int keyBitLength,
		      const unsigned char *rawKey, 
		      KEY_TABLE_TYPE keyTable);

void Camellia_EncryptBlock(const int keyBitLength,
			   const unsigned char *plaintext, 
			   const KEY_TABLE_TYPE keyTable, 
			   unsigned char *cipherText);

void Camellia_DecryptBlock(const int keyBitLength, 
			   const unsigned char *cipherText, 
			   const KEY_TABLE_TYPE keyTable, 
			   unsigned char *plaintext);


typedef uint16_t    cam_fret;   /* type for function return value       */
#define camellia_good 1
#define camellia_bad 1
#ifndef CAMELLIA_DLL                 /* implement normal or DLL functions    */
#define cam_rval    cam_fret
#else
#define cam_rval    cam_fret __declspec(dllexport) _stdcall
#endif

typedef struct                      /* the Camellia context for encryption */
{
    uint32_t k_sch[CAMELLIA_TABLE_WORD_LEN]; /* the encryption key schedule */
    int keybitlen;			/* bitlength of key */
} camellia_ctx;


/* for Kerberos 5 tree -- hide names!  */
#define camellia_blk_len	krb5int_camellia_blk_len
#define camellia_enc_key	krb5int_camellia_enc_key
#define camellia_enc_blk	krb5int_camellia_enc_blk
#define camellia_dec_key	krb5int_camellia_dec_key
#define camellia_dec_blk	krb5int_camellia_dec_blk

cam_rval camellia_blk_len(unsigned int blen, camellia_ctx cx[1]);
cam_rval camellia_enc_key(const unsigned char in_key[], unsigned int klen,
			  camellia_ctx cx[1]);
cam_rval camellia_enc_blk(const unsigned char in_blk[],
			  unsigned char out_blk[],
			  const camellia_ctx cx[1]);
cam_rval camellia_dec_key(const unsigned char in_key[], unsigned int klen,
			  camellia_ctx cx[1]);
cam_rval camellia_dec_blk(const unsigned char in_blk[],
			  unsigned char out_blk[],
			  const camellia_ctx cx[1]);

#ifdef  __cplusplus
}
#endif

#endif /* HEADER_CAMELLIA_H */
