/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/md4/rsa-md4.h */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
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
 * Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that
 * it is identified as the "RSA Data Security, Inc. MD4 Message
 * Digest Algorithm" in all material mentioning or referencing this
 * software or this function.
 *
 * License is also granted to make and use derivative works
 * provided that such works are identified as "derived from the RSA
 * Data Security, Inc. MD4 Message Digest Algorithm" in all
 * material mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning
 * either the merchantability of this software or the suitability
 * of this software for any particular purpose.  It is provided "as
 * is" without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/* RSA MD4 header file, with Kerberos/STDC additions. */

#ifndef __KRB5_RSA_MD4_H__
#define __KRB5_RSA_MD4_H__

#ifdef unicos61
#include <sys/types.h>
#endif /* unicos61 */

/* 16 u_char's in the digest */
#define RSA_MD4_CKSUM_LENGTH    16
/* des blocksize is 8, so this works nicely... */
#define OLD_RSA_MD4_DES_CKSUM_LENGTH    16
#define NEW_RSA_MD4_DES_CKSUM_LENGTH    24
#define RSA_MD4_DES_CONFOUND_LENGTH     8

/*
**********************************************************************
** md4.h -- Header file for implementation of MD4                   **
** RSA Data Security, Inc. MD4 Message Digest Algorithm             **
** Created: 2/17/90 RLR                                             **
** Revised: 12/27/90 SRD,AJ,BSK,JT Reference C version              **
**********************************************************************
*/

/* Data structure for MD4 (Message Digest) computation */
typedef struct {
    krb5_ui_4 i[2];                       /* number of _bits_ handled mod 2^64 */
    krb5_ui_4 buf[4];                     /* scratch buffer */
    unsigned char in[64];                 /* input buffer */
    unsigned char digest[16];             /* actual digest after MD4Final call */
} krb5_MD4_CTX;

extern void krb5int_MD4Init(krb5_MD4_CTX *);
extern void krb5int_MD4Update(krb5_MD4_CTX *, const unsigned char *, unsigned int);
extern void krb5int_MD4Final(krb5_MD4_CTX *);

/*
**********************************************************************
** End of md4.h                                                     **
******************************* (cut) ********************************
*/
#endif /* __KRB5_RSA_MD4_H__ */
