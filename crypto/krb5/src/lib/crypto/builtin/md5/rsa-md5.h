/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that
 * it is identified as the "RSA Data Security, Inc. MD5 Message-
 * Digest Algorithm" in all material mentioning or referencing this
 * software or this function.
 *
 * License is also granted to make and use derivative works
 * provided that such works are identified as "derived from the RSA
 * Data Security, Inc. MD5 Message-Digest Algorithm" in all
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

/*
***********************************************************************
** md5.h -- header file for implementation of MD5                    **
** RSA Data Security, Inc. MD5 Message-Digest Algorithm              **
** Created: 2/17/90 RLR                                              **
** Revised: 12/27/90 SRD,AJ,BSK,JT Reference C version               **
** Revised (for MD5): RLR 4/27/91                                    **
**   -- G modified to have y&~z instead of y&z                       **
**   -- FF, GG, HH modified to add in last register done             **
**   -- Access pattern: round 2 works mod 5, round 3 works mod 3     **
**   -- distinct additive constant for each step                     **
**   -- round 4 added, working mod 7                                 **
***********************************************************************
*/

#ifndef KRB5_RSA_MD5__
#define KRB5_RSA_MD5__

/* Data structure for MD5 (Message-Digest) computation */
typedef struct {
    krb5_ui_4 i[2];                       /* number of _bits_ handled mod 2^64 */
    krb5_ui_4 buf[4];                     /* scratch buffer */
    unsigned char in[64];                 /* input buffer */
    unsigned char digest[16];             /* actual digest after MD5Final call */
} krb5_MD5_CTX;

extern void krb5int_MD5Init(krb5_MD5_CTX *);
extern void krb5int_MD5Update(krb5_MD5_CTX *,const unsigned char *,unsigned int);
extern void krb5int_MD5Final(krb5_MD5_CTX *);

#define RSA_MD5_CKSUM_LENGTH            16
#define OLD_RSA_MD5_DES_CKSUM_LENGTH    16
#define NEW_RSA_MD5_DES_CKSUM_LENGTH    24
#define RSA_MD5_DES_CONFOUND_LENGTH     8

#endif /* KRB5_RSA_MD5__ */
