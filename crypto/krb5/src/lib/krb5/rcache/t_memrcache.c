/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/t_memrcache.c - memory replay cache tests */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "memrcache.c"

int
main()
{
    krb5_error_code ret;
    krb5_context context;
    k5_memrcache mrc;
    int i;
    uint8_t tag[4];
    krb5_data tag_data = make_data(tag, 4);
    struct entry *e;

    ret = krb5_init_context(&context);
    assert(ret == 0);

    /* Store a thousand unique tags, then verify that they all appear as
     * replays. */
    ret = k5_memrcache_create(context, &mrc);
    assert(ret == 0);
    for (i = 0; i < 1000; i++) {
        store_32_be(i, tag);
        ret = k5_memrcache_store(context, mrc, &tag_data);
        assert(ret == 0);
    }
    for (i = 0; i < 1000; i++) {
        store_32_be(i, tag);
        ret = k5_memrcache_store(context, mrc, &tag_data);
        assert(ret == KRB5KRB_AP_ERR_REPEAT);
    }
    k5_memrcache_free(context, mrc);

    /* Store a thousand unique tags, each spaced out so that previous entries
     * appear as expired.  Verify that the expiration queue has one entry. */
    ret = k5_memrcache_create(context, &mrc);
    assert(ret == 0);
    context->clockskew = 100;
    for (i = 1; i < 1000; i++) {
        krb5_set_debugging_time(context, i * 200, 0);
        store_32_be(i, tag);
        ret = k5_memrcache_store(context, mrc, &tag_data);
        assert(ret == 0);
    }
    e = K5_TAILQ_FIRST(&mrc->expiration_queue);
    assert(e != NULL && K5_TAILQ_NEXT(e, links) == NULL);
    k5_memrcache_free(context, mrc);

    krb5_free_context(context);
    return 0;
}
