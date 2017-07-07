/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/gen_seqnum.c */
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
 * Routine to automatically generate a starting sequence number.
 * We do this by getting a random key and encrypting something with it,
 * then taking the output and slicing it up.
 */

#include "k5-int.h"

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static inline krb5_data
key2data (krb5_keyblock k)
{
    krb5_data d;
    d.magic = KV5M_DATA;
    d.length = k.length;
    d.data = (char *) k.contents;
    return d;
}

krb5_error_code
krb5_generate_seq_number(krb5_context context, const krb5_keyblock *key, krb5_ui_4 *seqno)
{
    krb5_data seed;
    krb5_error_code retval;

    seed = key2data(*key);
    if ((retval = krb5_c_random_add_entropy(context, KRB5_C_RANDSOURCE_TRUSTEDPARTY, &seed)))
        return(retval);

    seed.length = sizeof(*seqno);
    seed.data = (char *) seqno;
    retval = krb5_c_random_make_octets(context, &seed);
    if (retval)
        return retval;
    /*
     * Work around implementation incompatibilities by not generating
     * initial sequence numbers greater than 2^30.  Previous MIT
     * implementations use signed sequence numbers, so initial
     * sequence numbers 2^31 to 2^32-1 inclusive will be rejected.
     * Letting the maximum initial sequence number be 2^30-1 allows
     * for about 2^30 messages to be sent before wrapping into
     * "negative" numbers.
     */
    *seqno &= 0x3fffffff;
    if (*seqno == 0)
        *seqno = 1;
    return 0;
}
