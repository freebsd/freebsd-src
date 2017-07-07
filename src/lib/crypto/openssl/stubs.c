/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/openssl/stubs.c - OpenSSL stub functions */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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

/*
 * This file defines symbols which must be exported by libk5crypto because they
 * are in the export list (for the sake of test programs), but which are not
 * used when OpenSSL is the back end.
 */

#include "k5-int.h"

/*
 * These functions are used by the Fortuna PRNG and test program.  They are
 * defined to OpenSSL equivalents when the OpenSSL back end headers are
 * used.
 */
void krb5int_aes_enc_blk(void);
void krb5int_aes_enc_key(void);
void k5_sha256_final(void);
void k5_sha256_init(void);
void k5_sha256_update(void);

void krb5int_aes_enc_blk(void)
{
    abort();
}

void krb5int_aes_enc_key(void)
{
    abort();
}

void k5_sha256_final(void)
{
    abort();
}

void k5_sha256_init(void)
{
    abort();
}

void k5_sha256_update(void)
{
    abort();
}
