/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_attr.c - Attribute test program */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "t_test.h"

const static unsigned char encoded[] = {
    0xba, 0xfc, 0xed, 0x50, 0xe1, 0xeb, 0xa6, 0xc3,
    0xc1, 0x75, 0x20, 0xe9, 0x10, 0xce, 0xc2, 0xcb
};

const static unsigned char auth[] = {
    0xac, 0x9d, 0xc1, 0x62, 0x08, 0xc4, 0xc7, 0x8b,
    0xa1, 0x2f, 0x25, 0x0a, 0xc4, 0x1d, 0x36, 0x41
};

int
main()
{
    unsigned char outbuf[MAX_ATTRSETSIZE];
    const char *decoded = "accept";
    const char *secret = "foo";
    krb5_error_code retval;
    krb5_context ctx;
    const char *tmp;
    krb5_data in;
    size_t len;

    noerror(krb5_init_context(&ctx));

    /* Make sure User-Name is 1. */
    insist(krad_attr_name2num("User-Name") == 1);

    /* Make sure 2 is User-Password. */
    tmp = krad_attr_num2name(2);
    insist(tmp != NULL);
    insist(strcmp(tmp, "User-Password") == 0);

    /* Test decoding. */
    in = make_data((void *)encoded, sizeof(encoded));
    noerror(kr_attr_decode(ctx, secret, auth,
                           krad_attr_name2num("User-Password"),
                           &in, outbuf, &len));
    insist(len == strlen(decoded));
    insist(memcmp(outbuf, decoded, len) == 0);

    /* Test encoding. */
    in = string2data((char *)decoded);
    retval = kr_attr_encode(ctx, secret, auth,
                            krad_attr_name2num("User-Password"),
                            &in, outbuf, &len);
    insist(retval == 0);
    insist(len == sizeof(encoded));
    insist(memcmp(outbuf, encoded, len) == 0);

    /* Test constraint. */
    in.length = 100;
    insist(kr_attr_valid(krad_attr_name2num("User-Password"), &in) == 0);
    in.length = 200;
    insist(kr_attr_valid(krad_attr_name2num("User-Password"), &in) != 0);

    krb5_free_context(ctx);
    return 0;
}
