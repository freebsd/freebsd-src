/*
 * Copyright (c) 2001, Dr Brian Gladman <brg@gladman.uk.net>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explcit or implied warranties
 * in respect of any properties, including, but not limited to, correctness
 * and fitness for purpose.
 */

/*
 * Issue Date: 21/01/2002
 *
 * This file contains the definitions required to use AES (Rijndael) in C.
 */

#ifndef _AES_H
#define _AES_H

#include <stdint.h>

/*  BLOCK_SIZE is in BYTES: 16, 24, 32 or undefined for aes.c and 16, 20,
    24, 28, 32 or undefined for aespp.c.  When left undefined a slower
    version that provides variable block length is compiled.
*/

#define BLOCK_SIZE  16

/* key schedule length (in 32-bit words)    */

#if !defined(BLOCK_SIZE)
#define KS_LENGTH   128
#else
#define KS_LENGTH   4 * BLOCK_SIZE
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

typedef uint16_t    aes_fret;   /* type for function return value       */
#define aes_bad     0           /* bad function return value            */
#define aes_good    1           /* good function return value           */
#ifndef AES_DLL                 /* implement normal or DLL functions    */
#define aes_rval    aes_fret
#else
#define aes_rval    aes_fret __declspec(dllexport) _stdcall
#endif

typedef struct                      /* the AES context for encryption   */
{   uint32_t    k_sch[KS_LENGTH];   /* the encryption key schedule      */
    uint32_t    n_rnd;              /* the number of cipher rounds      */
    uint32_t    n_blk;              /* the number of bytes in the state */
} aes_ctx;

/* for Kerberos 5 tree -- hide names!  */
#define aes_blk_len	krb5int_aes_blk_len
#define aes_enc_key	krb5int_aes_enc_key
#define aes_enc_blk	krb5int_aes_enc_blk
#define aes_dec_key	krb5int_aes_dec_key
#define aes_dec_blk	krb5int_aes_dec_blk
#define fl_tab		krb5int_fl_tab
#define ft_tab		krb5int_ft_tab
#define il_tab		krb5int_il_tab
#define im_tab		krb5int_im_tab
#define it_tab		krb5int_it_tab
#define rcon_tab	krb5int_rcon_tab

aes_rval aes_blk_len(unsigned int blen, aes_ctx cx[1]);

aes_rval aes_enc_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1]);
aes_rval aes_enc_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

aes_rval aes_dec_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1]);
aes_rval aes_dec_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

#if defined(__cplusplus)
}
#endif

#endif
