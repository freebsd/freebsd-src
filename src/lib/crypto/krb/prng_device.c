/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/prng_device.c - OS device-based PRNG implementation */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
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
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 *  or implied warranty.
 */

/*
 * This file implements a PRNG module which relies on the system's /dev/urandom
 * device.  An OS packager can select this module given sufficient confidence
 * in the operating system's native PRNG quality.
 */

#include "crypto_int.h"

#define DEVICE "/dev/urandom"

static int fd = -1;

int
k5_prng_init(void)
{
    /* Try to open the random device read-write; if that fails, read-only is
     * okay. */
    fd = open(DEVICE, O_RDWR, 0);
    if (fd == -1)
        fd = open(DEVICE, O_RDONLY, 0);
    if (fd == -1)
        return errno;
    return 0;
}

void
k5_prng_cleanup(void)
{
    close(fd);
    fd = -1;
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_add_entropy(krb5_context context, unsigned int randsource,
                          const krb5_data *indata)
{
    krb5_error_code ret;

    ret = krb5int_crypto_init();
    if (ret)
        return ret;

    /* Some random devices let user-space processes contribute entropy.  Don't
     * worry if this fails. */
    (void)write(fd, indata->data, indata->length);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_make_octets(krb5_context context, krb5_data *outdata)
{
    char *buf = outdata->data;
    size_t len = outdata->length;
    ssize_t count;

    while (len > 0) {
        count = read(fd, buf, len);
        if (count == 0)         /* Not expected from a random device. */
            return KRB5_CRYPTO_INTERNAL;
        if (count == -1)
            return errno;
        buf += count;
        len -= count;
    }
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_c_random_os_entropy(krb5_context context, int strong, int *success)
{
    return 0;
}
