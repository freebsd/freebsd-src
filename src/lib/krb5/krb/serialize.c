/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/serialize.c - Base serialization routines */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"

/*
 * krb5_ser_pack_int32()        - Pack a 4-byte integer if space is available.
 *                                Update buffer pointer and remaining space.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_int32(krb5_int32 iarg, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= sizeof(krb5_int32)) {
        store_32_be(iarg, *bufp);
        *bufp += sizeof(krb5_int32);
        *remainp -= sizeof(krb5_int32);
        return(0);
    }
    else
        return(ENOMEM);
}

/*
 * krb5_ser_pack_int64()        - Pack an 8-byte integer if space is available.
 *                                Update buffer pointer and remaining space.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_int64(int64_t iarg, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= sizeof(int64_t)) {
        store_64_be(iarg, (unsigned char *)*bufp);
        *bufp += sizeof(int64_t);
        *remainp -= sizeof(int64_t);
        return(0);
    }
    else
        return(ENOMEM);
}

/*
 * krb5_ser_pack_bytes()        - Pack a string of bytes.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_pack_bytes(krb5_octet *ostring, size_t osize, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= osize) {
        memcpy(*bufp, ostring, osize);
        *bufp += osize;
        *remainp -= osize;
        return(0);
    }
    else
        return(ENOMEM);
}

/*
 * krb5_ser_unpack_int32()      - Unpack a 4-byte integer if it's there.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_int32(krb5_int32 *intp, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= sizeof(krb5_int32)) {
        *intp = load_32_be(*bufp);
        *bufp += sizeof(krb5_int32);
        *remainp -= sizeof(krb5_int32);
        return(0);
    }
    else
        return(ENOMEM);
}

/*
 * krb5_ser_unpack_int64()      - Unpack an 8-byte integer if it's there.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_int64(int64_t *intp, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= sizeof(int64_t)) {
        *intp = load_64_be((unsigned char *)*bufp);
        *bufp += sizeof(int64_t);
        *remainp -= sizeof(int64_t);
        return(0);
    }
    else
        return(ENOMEM);
}

/*
 * krb5_ser_unpack_bytes()      - Unpack a byte string if it's there.
 */
krb5_error_code KRB5_CALLCONV
krb5_ser_unpack_bytes(krb5_octet *istring, size_t isize, krb5_octet **bufp, size_t *remainp)
{
    if (*remainp >= isize) {
        memcpy(istring, *bufp, isize);
        *bufp += isize;
        *remainp -= isize;
        return(0);
    }
    else
        return(ENOMEM);
}
