/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/des_int.h */
/*
 * Copyright 1987, 1988, 1990, 2002 by the Massachusetts Institute of
 * Technology.  All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */
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

/* Private include file for the Data Encryption Standard library. */

/* only do the whole thing once  */
#ifndef DES_INTERNAL_DEFS
#define DES_INTERNAL_DEFS

#include "k5-int.h"
/*
 * Begin "mit-des.h"
 */
#ifndef KRB5_MIT_DES__
#define KRB5_MIT_DES__

#if defined(__MACH__) && defined(__APPLE__)
#include <TargetConditionals.h>
#include <AvailabilityMacros.h>
#if TARGET_RT_MAC_CFM
#error "Use KfM 4.0 SDK headers for CFM compilation."
#endif
#if defined(DEPRECATED_IN_MAC_OS_X_VERSION_10_5) && !defined(KRB5_SUPRESS_DEPRECATED_WARNINGS)
#define KRB5INT_DES_DEPRECATED DEPRECATED_IN_MAC_OS_X_VERSION_10_5
#endif
#endif /* defined(__MACH__) && defined(__APPLE__) */

/* Macro to add deprecated attribute to DES types and functions */
/* Currently only defined on macOS 10.5 and later.              */
#ifndef KRB5INT_DES_DEPRECATED
#define KRB5INT_DES_DEPRECATED
#endif

#include <limits.h>

#if UINT_MAX >= 0xFFFFFFFFUL
#define DES_INT32 int
#define DES_UINT32 unsigned int
#else
#define DES_INT32 long
#define DES_UINT32 unsigned long
#endif

typedef unsigned char des_cblock[8]     /* crypto-block size */
KRB5INT_DES_DEPRECATED;

/*
 * Key schedule.
 *
 * This used to be
 *
 * typedef struct des_ks_struct {
 *     union { DES_INT32 pad; des_cblock _;} __;
 * } des_key_schedule[16];
 *
 * but it would cause trouble if DES_INT32 were ever more than 4
 * bytes.  The reason is that all the encryption functions cast it to
 * (DES_INT32 *), and treat it as if it were DES_INT32[32].  If
 * 2*sizeof(DES_INT32) is ever more than sizeof(des_cblock), the
 * caller-allocated des_key_schedule will be overflowed by the key
 * scheduling functions.  We can't assume that every platform will
 * have an exact 32-bit int, and nothing should be looking inside a
 * des_key_schedule anyway.
 */
typedef struct des_ks_struct {  DES_INT32 _[2]; } des_key_schedule[16]
KRB5INT_DES_DEPRECATED;

typedef des_cblock mit_des_cblock;
typedef des_key_schedule mit_des_key_schedule;

/* Triple-DES structures */
typedef mit_des_cblock          mit_des3_cblock[3];
typedef mit_des_key_schedule    mit_des3_key_schedule[3];

#define MIT_DES_ENCRYPT 1
#define MIT_DES_DECRYPT 0

typedef struct mit_des_ran_key_seed {
    krb5_encrypt_block eblock;
    krb5_data sequence;
} mit_des_random_state;

/* the first byte of the key is already in the keyblock */

#define MIT_DES_BLOCK_LENGTH            (8*sizeof(krb5_octet))
/* This used to be 8*sizeof(krb5_octet) */
#define MIT_DES_KEYSIZE                 8

#define MIT_DES_CBC_CKSUM_LENGTH        (4*sizeof(krb5_octet))

#endif /* KRB5_MIT_DES__ */
/*
 * End "mit-des.h"
 */

/* afsstring2key.c */
krb5_error_code mit_afs_string_to_key(krb5_keyblock *keyblock,
                                      const krb5_data *data,
                                      const krb5_data *salt);
char *mit_afs_crypt(const char *pw, const char *salt, char *iobuf);

/* f_cksum.c */
unsigned long mit_des_cbc_cksum(const krb5_octet *, krb5_octet *,
                                unsigned long, const mit_des_key_schedule,
                                const krb5_octet *);

/* f_cbc.c (used by test programs) */
int
mit_des_cbc_encrypt(const mit_des_cblock *in, mit_des_cblock *out,
                    unsigned long length, const mit_des_key_schedule schedule,
                    const mit_des_cblock ivec, int enc);

#define mit_des_zeroblock krb5int_c_mit_des_zeroblock
extern const mit_des_cblock mit_des_zeroblock;

/* fin_rndkey.c */
krb5_error_code mit_des_finish_random_key(const krb5_encrypt_block *,
                                          krb5_pointer *);

/* finish_key.c */
krb5_error_code mit_des_finish_key(krb5_encrypt_block *);

/* init_rkey.c */
krb5_error_code mit_des_init_random_key(const krb5_encrypt_block *,
                                        const krb5_keyblock *,
                                        krb5_pointer *);

/* key_parity.c */
void mit_des_fixup_key_parity(mit_des_cblock);
int mit_des_check_key_parity(mit_des_cblock);

/* key_sched.c */
int mit_des_key_sched(mit_des_cblock, mit_des_key_schedule);

/* process_ky.c */
krb5_error_code mit_des_process_key(krb5_encrypt_block *,
                                    const krb5_keyblock *);

/* random_key.c */
krb5_error_code mit_des_random_key(const krb5_encrypt_block *,
                                   krb5_pointer, krb5_keyblock **);

/* string2key.c */
krb5_error_code mit_des_string_to_key(const krb5_encrypt_block *,
                                      krb5_keyblock *, const krb5_data *,
                                      const krb5_data *);
krb5_error_code mit_des_string_to_key_int(krb5_keyblock *, const krb5_data *,
                                          const krb5_data *);

/* weak_key.c */
int mit_des_is_weak_key(mit_des_cblock);

/* cmb_keys.c */
krb5_error_code mit_des_combine_subkeys(const krb5_keyblock *,
                                        const krb5_keyblock *,
                                        krb5_keyblock **);

/* f_pcbc.c */
int mit_des_pcbc_encrypt();

/* f_sched.c */
int mit_des_make_key_sched(mit_des_cblock, mit_des_key_schedule);


/* misc.c */
extern void swap_bits(char *);
extern unsigned long long_swap_bits(unsigned long);
extern unsigned long swap_six_bits_to_ansi(unsigned long);
extern unsigned long swap_four_bits_to_ansi(unsigned long);
extern unsigned long swap_bit_pos_1(unsigned long);
extern unsigned long swap_bit_pos_0(unsigned long);
extern unsigned long swap_bit_pos_0_to_ansi(unsigned long);
extern unsigned long rev_swap_bit_pos_0(unsigned long);
extern unsigned long swap_byte_bits(unsigned long);
extern unsigned long swap_long_bytes_bit_number(unsigned long);
#ifdef FILE
/* XXX depends on FILE being a #define! */
extern void test_set(FILE *, const char *, int, const char *, int);
#endif

void
krb5int_des3_cbc_encrypt(krb5_crypto_iov *data, unsigned long num_data,
                         const mit_des_key_schedule ks1,
                         const mit_des_key_schedule ks2,
                         const mit_des_key_schedule ks3,
                         mit_des_cblock ivec);

void
krb5int_des3_cbc_decrypt(krb5_crypto_iov *data, unsigned long num_data,
                         const mit_des_key_schedule ks1,
                         const mit_des_key_schedule ks2,
                         const mit_des_key_schedule ks3,
                         mit_des_cblock ivec);

void
krb5int_des_cbc_encrypt(krb5_crypto_iov *data, unsigned long num_data,
                        const mit_des_key_schedule schedule,
                        mit_des_cblock ivec);

void
krb5int_des_cbc_decrypt(krb5_crypto_iov *data, unsigned long num_data,
                        const mit_des_key_schedule schedule,
                        mit_des_cblock ivec);

void
krb5int_des_cbc_mac(const krb5_crypto_iov *data, unsigned long num_data,
                    const mit_des_key_schedule schedule, mit_des_cblock ivec,
                    mit_des_cblock out);

/* d3_procky.c */
krb5_error_code mit_des3_process_key(krb5_encrypt_block *eblock,
                                     const krb5_keyblock *keyblock);

/* d3_kysched.c */
int mit_des3_key_sched(mit_des3_cblock key, mit_des3_key_schedule schedule);

/* d3_str2ky.c */
krb5_error_code mit_des3_string_to_key(const krb5_encrypt_block *eblock,
                                       krb5_keyblock *keyblock,
                                       const krb5_data *data,
                                       const krb5_data *salt);

/* u_nfold.c */
krb5_error_code mit_des_n_fold(const krb5_octet *input, const size_t in_len,
                               krb5_octet *output, const size_t out_len);

/* u_rn_key.c */
int mit_des_is_weak_keyblock(krb5_keyblock *keyblock);

void mit_des_fixup_keyblock_parity(krb5_keyblock *keyblock);

krb5_error_code mit_des_set_random_generator_seed(const krb5_data *seed,
                                                  krb5_pointer random_state);

krb5_error_code mit_des_set_random_sequence_number(const krb5_data *sequence,
                                                   krb5_pointer random_state);
#endif  /*DES_INTERNAL_DEFS*/
