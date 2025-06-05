/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_marshal.c - test program for cred marshalling */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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

#include "cc-int.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>

/*
 * Versions 1 and 2 of the ccache format use native byte order representations
 * of integers.  The test data below is from a little-endian platform.  Skip
 * those tests on big-endian platforms by starting at version 3.
 */
#ifdef K5_BE
#define FIRST_VERSION 3
#else
#define FIRST_VERSION 1
#endif

/* Each test contains the expected binary representation of a credential cache
 * divided into the header, the default principal, and two credentials. */
const struct test {
    size_t headerlen;
    const unsigned char header[256];
    size_t princlen;
    const unsigned char princ[256];
    size_t cred1len;
    const unsigned char cred1[256];
    size_t cred2len;
    const unsigned char cred2[256];
} tests[4] = {
    {
        /* Version 1 header */
        2,
        "\x05\x01",
        /* Version 1 principal */
        33,
        "\x02\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54\x45\x53\x54\x2E"
        "\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63\x6C\x69\x65\x6E"
        "\x74",
        /* Version 1 cred 1 */
        165,
        "\x02\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54\x45\x53\x54\x2E"
        "\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63\x6C\x69\x65\x6E"
        "\x74\x03\x00\x00\x00\x0B\x00\x00\x00\x45\x58\x41\x4D\x50\x4C\x45"
        "\x2E\x43\x4F\x4D\x04\x00\x00\x00\x74\x65\x73\x74\x04\x00\x00\x00"
        "\x68\x6F\x73\x74\x11\x00\x10\x00\x00\x00\x00\x01\x02\x03\x04\x05"
        "\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x0B\x00\x00\x00\xDE\x00"
        "\x00\x00\x05\x0D\x00\x00\x00\xCA\x9A\x3B\x00\x00\x00\x80\x40\x01"
        "\x00\x00\x00\x02\x00\x04\x00\x00\x00\x0A\x00\x00\x01\x02\x00\x00"
        "\x00\x00\x02\x0A\x00\x00\x00\x73\x69\x67\x6E\x74\x69\x63\x6B\x65"
        "\x74\x9C\xFF\x00\x00\x00\x00\x06\x00\x00\x00\x74\x69\x63\x6B\x65"
        "\x74\x00\x00\x00\x00",
        /* Version 1 cred 2 */
        113,
        "\x02\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54\x45\x53\x54\x2E"
        "\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63\x6C\x69\x65\x6E"
        "\x74\x01\x00\x00\x00\x00\x00\x00\x00\x17\x00\x10\x00\x00\x00\x0F"
        "\x0E\x0D\x0C\x0B\x0A\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x00\x00\x00"
        "\x74\x69\x63\x6B\x65\x74\x07\x00\x00\x00\x32\x74\x69\x63\x6B\x65"
        "\x74"
    },
    {
        /* Version 2 header */
        2,
        "\x05\x02",
        /* Version 2 principal */
        37,
        "\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74",
        /* Version 2 cred 1 */
        173,
        "\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x01\x00\x00\x00\x02\x00\x00\x00\x0B\x00\x00"
        "\x00\x45\x58\x41\x4D\x50\x4C\x45\x2E\x43\x4F\x4D\x04\x00\x00\x00"
        "\x74\x65\x73\x74\x04\x00\x00\x00\x68\x6F\x73\x74\x11\x00\x10\x00"
        "\x00\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D"
        "\x0E\x0F\x0B\x00\x00\x00\xDE\x00\x00\x00\x05\x0D\x00\x00\x00\xCA"
        "\x9A\x3B\x00\x00\x00\x80\x40\x01\x00\x00\x00\x02\x00\x04\x00\x00"
        "\x00\x0A\x00\x00\x01\x02\x00\x00\x00\x00\x02\x0A\x00\x00\x00\x73"
        "\x69\x67\x6E\x74\x69\x63\x6B\x65\x74\x9C\xFF\x00\x00\x00\x00\x06"
        "\x00\x00\x00\x74\x69\x63\x6B\x65\x74\x00\x00\x00\x00",
        /* Version 2 cred 2 */
        121,
        "\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x00\x00\x00\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x0A\x00\x00\x00\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x17\x00\x10\x00\x00\x00\x0F\x0E\x0D\x0C\x0B\x0A\x09\x08\x07"
        "\x06\x05\x04\x03\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x06\x00\x00\x00\x74\x69\x63\x6B\x65\x74\x07\x00"
        "\x00\x00\x32\x74\x69\x63\x6B\x65\x74"
    },
    {
        /* Version 3 header */
        2,
        "\x05\x03",
        /* Version 3 principal */
        37,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74",
        /* Version 3 cred 1 */
        175,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00"
        "\x0B\x45\x58\x41\x4D\x50\x4C\x45\x2E\x43\x4F\x4D\x00\x00\x00\x04"
        "\x74\x65\x73\x74\x00\x00\x00\x04\x68\x6F\x73\x74\x00\x11\x00\x11"
        "\x00\x00\x00\x10\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B"
        "\x0C\x0D\x0E\x0F\x00\x00\x00\x0B\x00\x00\x00\xDE\x00\x00\x0D\x05"
        "\x3B\x9A\xCA\x00\x00\x40\x80\x00\x00\x00\x00\x00\x01\x00\x02\x00"
        "\x00\x00\x04\x0A\x00\x00\x01\x00\x00\x00\x02\x02\x00\x00\x00\x00"
        "\x0A\x73\x69\x67\x6E\x74\x69\x63\x6B\x65\x74\xFF\x9C\x00\x00\x00"
        "\x00\x00\x00\x00\x06\x74\x69\x63\x6B\x65\x74\x00\x00\x00\x00",
        /* Version 3 cred 2 */
        123,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x17\x00\x17\x00\x00\x00\x10\x0F\x0E\x0D\x0C\x0B\x0A\x09"
        "\x08\x07\x06\x05\x04\x03\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x74\x69\x63\x6B\x65\x74"
        "\x00\x00\x00\x07\x32\x74\x69\x63\x6B\x65\x74"
    },
    {
        /* Version 4 header */
        16,
        "\x05\x04\x00\x0C\x00\x01\x00\x08\x00\x00\x01\x2C\x00\x00\xD4\x31",
        /* Version 4 principal */
        37,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74",
        /* Version 4 cred 1 */
        173,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x00\x00\x00\x01\x00\x00\x00\x02\x00\x00\x00"
        "\x0B\x45\x58\x41\x4D\x50\x4C\x45\x2E\x43\x4F\x4D\x00\x00\x00\x04"
        "\x74\x65\x73\x74\x00\x00\x00\x04\x68\x6F\x73\x74\x00\x11\x00\x00"
        "\x00\x10\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D"
        "\x0E\x0F\x00\x00\x00\x0B\x00\x00\x00\xDE\x00\x00\x0D\x05\x3B\x9A"
        "\xCA\x00\x00\x40\x80\x00\x00\x00\x00\x00\x01\x00\x02\x00\x00\x00"
        "\x04\x0A\x00\x00\x01\x00\x00\x00\x02\x02\x00\x00\x00\x00\x0A\x73"
        "\x69\x67\x6E\x74\x69\x63\x6B\x65\x74\xFF\x9C\x00\x00\x00\x00\x00"
        "\x00\x00\x06\x74\x69\x63\x6B\x65\x74\x00\x00\x00\x00",
        /* Version 4 cred 2 */
        121,
        "\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x0B\x4B\x52\x42\x54"
        "\x45\x53\x54\x2E\x43\x4F\x4D\x00\x00\x00\x0A\x74\x65\x73\x74\x63"
        "\x6C\x69\x65\x6E\x74\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x17\x00\x00\x00\x10\x0F\x0E\x0D\x0C\x0B\x0A\x09\x08\x07"
        "\x06\x05\x04\x03\x02\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x06\x74\x69\x63\x6B\x65\x74\x00\x00"
        "\x00\x07\x32\x74\x69\x63\x6B\x65\x74"
    }
};

static void
verify_princ(krb5_principal p)
{
    assert(p->length == 1);
    assert(data_eq_string(p->realm, "KRBTEST.COM"));
    assert(data_eq_string(p->data[0], "testclient"));
}

static void
verify_cred1(const krb5_creds *c)
{
    uint32_t ipaddr = ntohl(0x0A000001);

    verify_princ(c->client);
    assert(c->server->length == 2);
    assert(data_eq_string(c->server->realm, "EXAMPLE.COM"));
    assert(data_eq_string(c->server->data[0], "test"));
    assert(data_eq_string(c->server->data[1], "host"));
    assert(c->keyblock.enctype == ENCTYPE_AES128_CTS_HMAC_SHA1_96);
    assert(c->keyblock.length == 16);
    assert(memcmp(c->keyblock.contents,
                  "\x0\x1\x2\x3\x4\x5\x6\x7\x8\x9\xA\xB\xC\xD\xE\xF",
                  16) == 0);
    assert(c->times.authtime == 11);
    assert(c->times.starttime == 222);
    assert(c->times.endtime == 3333);
    assert(c->times.renew_till == 1000000000);
    assert(c->is_skey == FALSE);
    assert(c->ticket_flags == (TKT_FLG_FORWARDABLE | TKT_FLG_RENEWABLE));
    assert(c->addresses != NULL && c->addresses[0] != NULL);
    assert(c->addresses[0]->addrtype == ADDRTYPE_INET);
    assert(c->addresses[0]->length == 4);
    assert(memcmp(c->addresses[0]->contents, &ipaddr, 4) == 0);
    assert(c->addresses[1] == NULL);
    assert(c->authdata != NULL && c->authdata[0] != NULL);
    assert(c->authdata[0]->ad_type == KRB5_AUTHDATA_SIGNTICKET);
    assert(c->authdata[0]->length == 10);
    assert(memcmp(c->authdata[0]->contents, "signticket", 10) == 0);
    assert(c->authdata[1] != NULL);
    assert(c->authdata[1]->ad_type == -100);
    assert(c->authdata[1]->length == 0);
    assert(c->authdata[2] == NULL);
    assert(data_eq_string(c->ticket, "ticket"));
    assert(c->second_ticket.length == 0);
}

static void
verify_cred2(const krb5_creds *c)
{
    verify_princ(c->client);
    assert(c->server->length == 0);
    assert(c->server->realm.length == 0);
    assert(c->keyblock.enctype == ENCTYPE_ARCFOUR_HMAC);
    assert(c->keyblock.length == 16);
    assert(memcmp(c->keyblock.contents,
                  "\xF\xE\xD\xC\xB\xA\x9\x8\x7\x6\x5\x4\x3\x2\x1\x0",
                  16) == 0);
    assert(c->times.authtime == 0);
    assert(c->times.starttime == 0);
    assert(c->times.endtime == 0);
    assert(c->times.renew_till == 0);
    assert(c->is_skey == TRUE);
    assert(c->ticket_flags == 0);
    assert(c->addresses == NULL || c->addresses[0] == NULL);
    assert(c->authdata == NULL || c->authdata[0] == NULL);
    assert(data_eq_string(c->ticket, "ticket"));
    assert(data_eq_string(c->second_ticket, "2ticket"));
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_ccache cache;
    krb5_principal princ;
    krb5_creds cred1, cred2, *alloc_cred;
    krb5_cc_cursor cursor;
    const char *filename;
    char *ccname, filebuf[256];
    int version, fd;
    const struct test *t;
    struct k5buf buf;
    krb5_data ser_data, *alloc_data;

    if (argc != 2)
        abort();
    filename = argv[1];
    if (asprintf(&ccname, "FILE:%s", filename) == -1)
        abort();

    if (krb5_init_context(&context) != 0)
        abort();

    /* Test public functions for unmarshalling and marshalling. */
    ser_data = make_data((char *)tests[3].cred1, tests[3].cred1len);
    if (krb5_unmarshal_credentials(context, &ser_data, &alloc_cred) != 0)
        abort();
    verify_cred1(alloc_cred);
    if (krb5_marshal_credentials(context, alloc_cred, &alloc_data) != 0)
        abort();
    assert(alloc_data->length == tests[3].cred1len);
    assert(memcmp(tests[3].cred1, alloc_data->data, alloc_data->length) == 0);
    krb5_free_data(context, alloc_data);
    krb5_free_creds(context, alloc_cred);

    for (version = FIRST_VERSION; version <= 4; version++) {
        t = &tests[version - 1];

        /* Test principal unmarshalling and marshalling. */
        if (k5_unmarshal_princ(t->princ, t->princlen, version, &princ) != 0)
            abort();
        verify_princ(princ);
        k5_buf_init_dynamic(&buf);
        k5_marshal_princ(&buf, version, princ);
        assert(buf.len == t->princlen);
        assert(memcmp(t->princ, buf.data, buf.len) == 0);
        k5_buf_free(&buf);

        /* Test cred1 unmarshalling and marshalling. */
        if (k5_unmarshal_cred(t->cred1, t->cred1len, version, &cred1) != 0)
            abort();
        verify_cred1(&cred1);
        k5_buf_init_dynamic(&buf);
        k5_marshal_cred(&buf, version, &cred1);
        assert(buf.len == t->cred1len);
        assert(memcmp(t->cred1, buf.data, buf.len) == 0);
        k5_buf_free(&buf);

        /* Test cred2 unmarshalling and marshalling. */
        if (k5_unmarshal_cred(t->cred2, t->cred2len, version, &cred2) != 0)
            abort();
        verify_cred2(&cred2);
        k5_buf_init_dynamic(&buf);
        k5_marshal_cred(&buf, version, &cred2);
        assert(buf.len == t->cred2len);
        assert(memcmp(t->cred2, buf.data, buf.len) == 0);
        k5_buf_free(&buf);

        /* Write a ccache containing the principal and creds.  Use the same
         * time offset as the version 4 test data used. */
        context->fcc_default_format = 0x0500 + version;
        context->os_context.time_offset = 300;
        context->os_context.usec_offset = 54321;
        context->os_context.os_flags = KRB5_OS_TOFFSET_VALID;
        if (krb5_cc_resolve(context, ccname, &cache) != 0)
            abort();
        if (krb5_cc_initialize(context, cache, princ) != 0)
            abort();
        if (krb5_cc_store_cred(context, cache, &cred1) != 0)
            abort();
        if (krb5_cc_store_cred(context, cache, &cred2) != 0)
            abort();
        if (krb5_cc_close(context, cache) != 0)
            abort();

        /* Verify the cache representation against the test data. */
        fd = open(filename, O_RDONLY);
        if (fd == -1)
            abort();
        if (read(fd, filebuf, t->headerlen) != (ssize_t)t->headerlen)
            abort();
        assert(memcmp(filebuf, t->header, t->headerlen) == 0);
        if (read(fd, filebuf, t->princlen) != (ssize_t)t->princlen)
            abort();
        assert(memcmp(filebuf, t->princ, t->princlen) == 0);
        if (read(fd, filebuf, t->cred1len) != (ssize_t)t->cred1len)
            abort();
        assert(memcmp(filebuf, t->cred1, t->cred1len) == 0);
        if (read(fd, filebuf, t->cred2len) != (ssize_t)t->cred2len)
            abort();
        assert(memcmp(filebuf, t->cred2, t->cred2len) == 0);
        close(fd);

        krb5_free_principal(context, princ);
        krb5_free_cred_contents(context, &cred1);
        krb5_free_cred_contents(context, &cred2);

        /* Write a cache containing the test data. */
        fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, 0700);
        if (fd == -1)
            abort();
        if (write(fd, t->header, t->headerlen) != (ssize_t)t->headerlen)
            abort();
        if (write(fd, t->princ, t->princlen) != (ssize_t)t->princlen)
            abort();
        if (write(fd, t->cred1, t->cred1len) != (ssize_t)t->cred1len)
            abort();
        if (write(fd, t->cred2, t->cred2len) != (ssize_t)t->cred2len)
            abort();
        close(fd);

        /* Read the cache and verify that it matches. */
        if (krb5_cc_resolve(context, ccname, &cache) != 0)
            abort();
        if (krb5_cc_get_principal(context, cache, &princ) != 0)
            abort();
        /* Not every version stores the time offset, but at least it shouldn't
         * have changed from when we set it before. */
        assert(context->os_context.time_offset == 300);
        assert(context->os_context.usec_offset == 54321);
        verify_princ(princ);
        if (krb5_cc_start_seq_get(context, cache, &cursor) != 0)
            abort();
        if (krb5_cc_next_cred(context, cache, &cursor, &cred1) != 0)
            abort();
        verify_cred1(&cred1);
        krb5_free_cred_contents(context, &cred1);
        if (krb5_cc_next_cred(context, cache, &cursor, &cred2) != 0)
            abort();
        verify_cred2(&cred2);
        krb5_free_cred_contents(context, &cred2);
        if (krb5_cc_next_cred(context, cache, &cursor, &cred2) != KRB5_CC_END)
            abort();
        if (krb5_cc_end_seq_get(context, cache, &cursor) != 0)
            abort();
        if (krb5_cc_close(context, cache) != 0)
            abort();
        krb5_free_principal(context, princ);
    }

    (void)unlink(filename);
    free(ccname);
    krb5_free_context(context);
    return 0;
}
