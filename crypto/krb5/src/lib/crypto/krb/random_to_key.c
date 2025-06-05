/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

/*
 * Create a key given random data.  It is assumed that random_key has
 * already been initialized and random_key->contents have been allocated
 * with the correct length.
 */
#include "crypto_int.h"

krb5_error_code KRB5_CALLCONV
krb5_c_random_to_key(krb5_context context, krb5_enctype enctype,
                     krb5_data *random_data, krb5_keyblock *random_key)
{
    krb5_error_code ret;
    const struct krb5_keytypes *ktp;

    if (random_data == NULL || random_key == NULL ||
        random_key->contents == NULL)
        return EINVAL;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return KRB5_BAD_ENCTYPE;

    if (random_key->length != ktp->enc->keylength)
        return KRB5_BAD_KEYSIZE;

    ret = ktp->rand2key(random_data, random_key);
    if (ret)
        zap(random_key->contents, random_key->length);

    return ret;
}

krb5_error_code
k5_rand2key_direct(const krb5_data *randombits, krb5_keyblock *keyblock)
{
    if (randombits->length != keyblock->length)
        return KRB5_CRYPTO_INTERNAL;

    keyblock->magic = KV5M_KEYBLOCK;
    memcpy(keyblock->contents, randombits->data, randombits->length);
    return 0;
}

static inline void
eighth_byte(unsigned char *b)
{
    b[7] = (((b[0] & 1) << 1) | ((b[1] & 1) << 2) | ((b[2] & 1) << 3) |
            ((b[3] & 1) << 4) | ((b[4] & 1) << 5) | ((b[5] & 1) << 6) |
            ((b[6] & 1) << 7));
}

krb5_error_code
k5_rand2key_des3(const krb5_data *randombits, krb5_keyblock *keyblock)
{
    int i;

    if (randombits->length != 21)
        return KRB5_CRYPTO_INTERNAL;

    keyblock->magic = KV5M_KEYBLOCK;

    /* Take the seven bytes, move them around into the top 7 bits of the
     * 8 key bytes, then compute the parity bits.  Do this three times. */
    for (i = 0; i < 3; i++) {
        memcpy(&keyblock->contents[i * 8], &randombits->data[i * 7], 7);
        eighth_byte(&keyblock->contents[i * 8]);
        k5_des_fixup_key_parity(&keyblock->contents[i * 8]);
    }
    return 0;
}
