/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_attrset.c - Attribute set test program */
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

const static unsigned char auth[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const static unsigned char encpass[] = {
    0x58, 0x8d, 0xff, 0xda, 0x37, 0xf9, 0xe4, 0xca,
    0x19, 0xae, 0x49, 0xb7, 0x16, 0x6d, 0x58, 0x27
};

int
main()
{
    unsigned char buffer[KRAD_PACKET_SIZE_MAX], encoded[MAX_ATTRSETSIZE];
    const char *username = "testUser", *password = "accept";
    const krb5_data *tmpp;
    krad_attrset *set;
    krb5_context ctx;
    size_t len = 0, encode_len;
    krb5_data tmp;

    noerror(krb5_init_context(&ctx));
    noerror(krad_attrset_new(ctx, &set));

    /* Add username. */
    tmp = string2data((char *)username);
    noerror(krad_attrset_add(set, krad_attr_name2num("User-Name"), &tmp));

    /* Add password. */
    tmp = string2data((char *)password);
    noerror(krad_attrset_add(set, krad_attr_name2num("User-Password"), &tmp));

    /* Encode attrset. */
    noerror(kr_attrset_encode(set, "foo", auth, buffer, &encode_len));
    krad_attrset_free(set);

    /* Manually encode User-Name. */
    encoded[len + 0] = krad_attr_name2num("User-Name");
    encoded[len + 1] = strlen(username) + 2;
    memcpy(encoded + len + 2, username, strlen(username));
    len += encoded[len + 1];

    /* Manually encode User-Password. */
    encoded[len + 0] = krad_attr_name2num("User-Password");
    encoded[len + 1] = sizeof(encpass) + 2;
    memcpy(encoded + len + 2, encpass, sizeof(encpass));
    len += encoded[len + 1];

    /* Compare output. */
    insist(len == encode_len);
    insist(memcmp(encoded, buffer, len) == 0);

    /* Decode output. */
    tmp = make_data(buffer, len);
    noerror(kr_attrset_decode(ctx, &tmp, "foo", auth, &set));

    /* Test getting an attribute. */
    tmp = string2data((char *)username);
    tmpp = krad_attrset_get(set, krad_attr_name2num("User-Name"), 0);
    insist(tmpp != NULL);
    insist(tmpp->length == tmp.length);
    insist(strncmp(tmpp->data, tmp.data, tmp.length) == 0);

    krad_attrset_free(set);
    krb5_free_context(ctx);
    return 0;
}
