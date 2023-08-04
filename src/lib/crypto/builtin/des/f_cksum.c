/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/f_cksum.c */
/*
 * Copyright (C) 1990 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

/* DES implementation donated by Dennis Ferguson */

/*
 * des_cbc_cksum.c - compute an 8 byte checksum using DES in CBC mode
 */
#include "crypto_int.h"
#include "des_int.h"
#include "f_tables.h"

#ifdef K5_BUILTIN_DES

/*
 * This routine performs DES cipher-block-chaining checksum operation,
 * a.k.a.  Message Authentication Code.  It ALWAYS encrypts from input
 * to a single 64 bit output MAC checksum.
 *
 * The key schedule is passed as an arg, as well as the cleartext or
 * ciphertext. The cleartext and ciphertext should be in host order.
 *
 * NOTE-- the output is ALWAYS 8 bytes long.  If not enough space was
 * provided, your program will get trashed.
 *
 * The input is null padded, at the end (highest addr), to an integral
 * multiple of eight bytes.
 */

unsigned long
mit_des_cbc_cksum(const krb5_octet *in, krb5_octet *out,
                  unsigned long length, const mit_des_key_schedule schedule,
                  const krb5_octet *ivec)
{
    unsigned DES_INT32 left, right;
    const unsigned DES_INT32 *kp;
    const unsigned char *ip;
    unsigned char *op;
    DES_INT32 len;

    /*
     * Initialize left and right with the contents of the initial
     * vector.
     */
    ip = ivec;
    GET_HALF_BLOCK(left, ip);
    GET_HALF_BLOCK(right, ip);

    /*
     * Suitably initialized, now work the length down 8 bytes
     * at a time.
     */
    ip = in;
    len = length;
    while (len > 0) {
        /*
         * Get more input, xor it in.  If the length is
         * greater than or equal to 8 this is straight
         * forward.  Otherwise we have to fart around.
         */
        if (len >= 8) {
            unsigned DES_INT32 temp;
            GET_HALF_BLOCK(temp, ip);
            left  ^= temp;
            GET_HALF_BLOCK(temp, ip);
            right ^= temp;
            len -= 8;
        } else {
            /*
             * Oh, shoot.  We need to pad the
             * end with zeroes.  Work backwards
             * to do this.
             */
            ip += (int) len;
            switch(len) {
            case 7:
                right ^= (*(--ip) & FF_UINT32) <<  8;
            case 6:
                right ^= (*(--ip) & FF_UINT32) << 16;
            case 5:
                right ^= (*(--ip) & FF_UINT32) << 24;
            case 4:
                left  ^=  *(--ip) & FF_UINT32;
            case 3:
                left  ^= (*(--ip) & FF_UINT32) <<  8;
            case 2:
                left  ^= (*(--ip) & FF_UINT32) << 16;
            case 1:
                left  ^= (*(--ip) & FF_UINT32) << 24;
                break;
            }
            len = 0;
        }

        /*
         * Encrypt what we have
         */
        kp = (const unsigned DES_INT32 *)schedule;
        DES_DO_ENCRYPT(left, right, kp);
    }

    /*
     * Done.  Left and right have the checksum.  Put it into
     * the output.
     */
    op = out;
    PUT_HALF_BLOCK(left, op);
    PUT_HALF_BLOCK(right, op);

    /*
     * Return right.  I'll bet the MIT code returns this
     * inconsistantly (with the low order byte of the checksum
     * not always in the low order byte of the DES_INT32).  We won't.
     */
    return right & 0xFFFFFFFFUL;
}

#endif /* K5_BUILTIN_DES */
