/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "k5-int.h"

/*
 * This PAC and keys are copied (with permission) from Samba torture
 * regression test suite, they where created by Andrew Bartlet.
 */

static const unsigned char saved_pac[] = {
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xd8, 0x01, 0x00, 0x00,
    0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x20, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc,
    0xc8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x30, 0xdf, 0xa6, 0xcb,
    0x4f, 0x7d, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x7f, 0xc0, 0x3c, 0x4e, 0x59, 0x62, 0x73, 0xc5, 0x01, 0xc0, 0x3c, 0x4e, 0x59,
    0x62, 0x73, 0xc5, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x16, 0x00, 0x16, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x02, 0x00, 0x65, 0x00, 0x00, 0x00,
    0xed, 0x03, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x02, 0x00,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x16, 0x00, 0x20, 0x00, 0x02, 0x00, 0x16, 0x00, 0x18, 0x00,
    0x24, 0x00, 0x02, 0x00, 0x28, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x57, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00,
    0x41, 0x00, 0x4c, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x02, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x57, 0x00, 0x32, 0x00,
    0x30, 0x00, 0x30, 0x00, 0x33, 0x00, 0x46, 0x00, 0x49, 0x00, 0x4e, 0x00, 0x41, 0x00, 0x4c, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x57, 0x00, 0x49, 0x00,
    0x4e, 0x00, 0x32, 0x00, 0x4b, 0x00, 0x33, 0x00, 0x54, 0x00, 0x48, 0x00, 0x49, 0x00, 0x4e, 0x00,
    0x4b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x15, 0x00, 0x00, 0x00, 0x11, 0x2f, 0xaf, 0xb5, 0x90, 0x04, 0x1b, 0xec, 0x50, 0x3b, 0xec, 0xdc,
    0x01, 0x00, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x66, 0x28, 0xea, 0x37, 0x80, 0xc5, 0x01, 0x16, 0x00, 0x77, 0x00, 0x32, 0x00, 0x30, 0x00,
    0x30, 0x00, 0x33, 0x00, 0x66, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x24, 0x00,
    0x76, 0xff, 0xff, 0xff, 0x37, 0xd5, 0xb0, 0xf7, 0x24, 0xf0, 0xd6, 0xd4, 0xec, 0x09, 0x86, 0x5a,
    0xa0, 0xe8, 0xc3, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x76, 0xff, 0xff, 0xff, 0xb4, 0xd8, 0xb8, 0xfe,
    0x83, 0xb3, 0x13, 0x3f, 0xfc, 0x5c, 0x41, 0xad, 0xe2, 0x64, 0x83, 0xe0, 0x00, 0x00, 0x00, 0x00
};

static unsigned int type_1_length = 472;

static const krb5_keyblock kdc_keyblock = {
    0, ENCTYPE_ARCFOUR_HMAC,
    16, (krb5_octet *)"\xB2\x86\x75\x71\x48\xAF\x7F\xD2\x52\xC5\x36\x03\xA1\x50\xB7\xE7"
};

static const krb5_keyblock member_keyblock = {
    0, ENCTYPE_ARCFOUR_HMAC,
    16, (krb5_octet *)"\xD2\x17\xFA\xEA\xE5\xE6\xB5\xF9\x5C\xCC\x94\x07\x7A\xB8\xA5\xFC"
};

static time_t authtime = 1120440609;
static const char *user = "w2003final$@WIN2K3.THINKER.LOCAL";

#if !defined(__cplusplus) && (__GNUC__ > 2)
static void err(krb5_context ctx, krb5_error_code code, const char *fmt, ...)
    __attribute__((__format__(__printf__, 3, 0)));
#endif

static void
err(krb5_context ctx, krb5_error_code code, const char *fmt, ...)
{
    va_list ap;
    char *msg;
    const char *errmsg = NULL;

    va_start(ap, fmt);
    if (vasprintf(&msg, fmt, ap) < 0)
        exit(1);
    va_end(ap);
    if (ctx && code)
        errmsg = krb5_get_error_message(ctx, code);
    if (errmsg)
        fprintf(stderr, "t_pac: %s: %s\n", msg, errmsg);
    else
        fprintf(stderr, "t_pac: %s\n", msg);
    exit(1);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_pac pac;
    krb5_data data;
    krb5_principal p;

    ret = krb5_init_context(&context);
    if (ret)
        err(NULL, 0, "krb5_init_contex");

    krb5_set_default_realm(context, "WIN2K3.THINKER.LOCAL");

    ret = krb5_parse_name(context, user, &p);
    if (ret)
        err(context, ret, "krb5_parse_name");

    ret = krb5_pac_parse(context, saved_pac, sizeof(saved_pac), &pac);
    if (ret)
        err(context, ret, "krb5_pac_parse");

    ret = krb5_pac_verify(context, pac, authtime, p,
                          &member_keyblock, &kdc_keyblock);
    if (ret)
        err(context, ret, "krb5_pac_verify");

    ret = krb5_pac_sign(context, pac, authtime, p,
                        &member_keyblock, &kdc_keyblock, &data);
    if (ret)
        err(context, ret, "krb5_pac_sign");

    krb5_pac_free(context, pac);

    ret = krb5_pac_parse(context, data.data, data.length, &pac);
    krb5_free_data_contents(context, &data);
    if (ret)
        err(context, ret, "krb5_pac_parse 2");

    ret = krb5_pac_verify(context, pac, authtime, p,
                          &member_keyblock, &kdc_keyblock);
    if (ret)
        err(context, ret, "krb5_pac_verify 2");

    /* make a copy and try to reproduce it */
    {
        uint32_t *list;
        size_t len, i;
        krb5_pac pac2;

        ret = krb5_pac_init(context, &pac2);
        if (ret)
            err(context, ret, "krb5_pac_init");

        /* our two user buffer plus the three "system" buffers */
        ret = krb5_pac_get_types(context, pac, &len, &list);
        if (ret)
            err(context, ret, "krb5_pac_get_types");

        for (i = 0; i < len; i++) {
            /* skip server_cksum, privsvr_cksum, and logon_name */
            if (list[i] == 6 || list[i] == 7 || list[i] == 10)
                continue;

            ret = krb5_pac_get_buffer(context, pac, list[i], &data);
            if (ret)
                err(context, ret, "krb5_pac_get_buffer");

            if (list[i] == 1) {
                if (type_1_length != data.length)
                    err(context, 0, "type 1 have wrong length: %lu",
                        (unsigned long)data.length);
            } else
                err(context, 0, "unknown type %lu", (unsigned long)list[i]);

            ret = krb5_pac_add_buffer(context, pac2, list[i], &data);
            if (ret)
                err(context, ret, "krb5_pac_add_buffer");
            krb5_free_data_contents(context, &data);
        }
        free(list);

        ret = krb5_pac_sign(context, pac2, authtime, p,
                            &member_keyblock, &kdc_keyblock, &data);
        if (ret)
            err(context, ret, "krb5_pac_sign 4");

        krb5_pac_free(context, pac2);

        ret = krb5_pac_parse(context, data.data, data.length, &pac2);
        if (ret)
            err(context, ret, "krb5_pac_parse 4");

        ret = krb5_pac_verify(context, pac2, authtime, p,
                              &member_keyblock, &kdc_keyblock);
        if (ret)
            err(context, ret, "krb5_pac_verify 4");

        krb5_free_data_contents(context, &data);

        krb5_pac_free(context, pac2);
    }

    krb5_pac_free(context, pac);

    /*
     * Test empty free
     */

    ret = krb5_pac_init(context, &pac);
    if (ret)
        err(context, ret, "krb5_pac_init");
    krb5_pac_free(context, pac);

    /*
     * Test add remove buffer
     */

    ret = krb5_pac_init(context, &pac);
    if (ret)
        err(context, ret, "krb5_pac_init");

    {
        const krb5_data cdata = { 0, 2, "\x00\x01" } ;

        ret = krb5_pac_add_buffer(context, pac, 1, &cdata);
        if (ret)
            err(context, ret, "krb5_pac_add_buffer");
    }
    {
        ret = krb5_pac_get_buffer(context, pac, 1, &data);
        if (ret)
            err(context, ret, "krb5_pac_get_buffer");
        if (data.length != 2 || memcmp(data.data, "\x00\x01", 2) != 0)
            err(context, 0, "krb5_pac_get_buffer data not the same");
        krb5_free_data_contents(context, &data);
    }

    {
        const krb5_data cdata = { 0, 2, "\x02\x00" } ;

        ret = krb5_pac_add_buffer(context, pac, 2, &cdata);
        if (ret)
            err(context, ret, "krb5_pac_add_buffer");
    }
    {
        ret = krb5_pac_get_buffer(context, pac, 1, &data);
        if (ret)
            err(context, ret, "krb5_pac_get_buffer");
        if (data.length != 2 || memcmp(data.data, "\x00\x01", 2) != 0)
            err(context, 0, "krb5_pac_get_buffer data not the same");
        krb5_free_data_contents(context, &data);
        /* */
        ret = krb5_pac_get_buffer(context, pac, 2, &data);
        if (ret)
            err(context, ret, "krb5_pac_get_buffer");
        if (data.length != 2 || memcmp(data.data, "\x02\x00", 2) != 0)
            err(context, 0, "krb5_pac_get_buffer data not the same");
        krb5_free_data_contents(context, &data);
    }

    ret = krb5_pac_sign(context, pac, authtime, p,
                        &member_keyblock, &kdc_keyblock, &data);
    if (ret)
        err(context, ret, "krb5_pac_sign");

    krb5_pac_free(context, pac);

    ret = krb5_pac_parse(context, data.data, data.length, &pac);
    krb5_free_data_contents(context, &data);
    if (ret)
        err(context, ret, "krb5_pac_parse 3");

    ret = krb5_pac_verify(context, pac, authtime, p,
                          &member_keyblock, &kdc_keyblock);
    if (ret)
        err(context, ret, "krb5_pac_verify 3");

    {
        uint32_t *list;
        size_t len;

        /* our two user buffer plus the three "system" buffers */
        ret = krb5_pac_get_types(context, pac, &len, &list);
        if (ret)
            err(context, ret, "krb5_pac_get_types");
        if (len != 5)
            err(context, 0, "list wrong length");
        free(list);
    }

    krb5_pac_free(context, pac);

    krb5_free_principal(context, p);
    krb5_free_context(context);

    return 0;
}
