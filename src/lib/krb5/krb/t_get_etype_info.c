/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_get_etype_info.c - test harness for krb5_get_etype_info() */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"
#include "k5-hex.h"
#include <krb5.h>

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_principal princ;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_enctype *etypes = NULL, *newptr, etype;
    krb5_data salt, s2kparams;
    const char *armor_ccache = NULL, *msg;
    char buf[128], *hex;
    int c, netypes = 0;

    while ((c = getopt(argc, argv, "e:T:")) != -1) {
        switch (c) {
        case 'e':
            newptr = realloc(etypes, (netypes + 1) * sizeof(*etypes));
            assert(newptr != NULL);
            etypes = newptr;
            ret = krb5_string_to_enctype(optarg, &etypes[netypes]);
            assert(!ret);
            netypes++;
            break;
        case 'T':
            armor_ccache = optarg;
            break;
        }
    }
    assert(argc == optind + 1);

    ret = krb5_init_context(&context);
    assert(!ret);
    ret = krb5_parse_name(context, argv[optind], &princ);
    assert(!ret);
    if (netypes > 0 || armor_ccache != NULL) {
        ret = krb5_get_init_creds_opt_alloc(context, &opt);
        assert(!ret);
        if (netypes > 0)
            krb5_get_init_creds_opt_set_etype_list(opt, etypes, netypes);
        if (armor_ccache != NULL) {
            ret = krb5_get_init_creds_opt_set_fast_ccache_name(context, opt,
                                                               armor_ccache);
            assert(!ret);
        }
    }
    ret = krb5_get_etype_info(context, princ, opt, &etype, &salt, &s2kparams);
    if (ret) {
        msg = krb5_get_error_message(context, ret);
        fprintf(stderr, "%s\n", msg);
        krb5_free_error_message(context, msg);
        exit(1);
    } else if (etype == ENCTYPE_NULL) {
        printf("no etype-info\n");
    } else {
        ret = krb5_enctype_to_name(etype, TRUE, buf, sizeof(buf));
        assert(!ret);
        printf("etype: %s\n", buf);
        printf("salt: %.*s\n", (int)salt.length, salt.data);
        if (s2kparams.length > 0) {
            ret = k5_hex_encode(s2kparams.data, s2kparams.length, TRUE, &hex);
            assert(!ret);
            printf("s2kparams: %s\n", hex);
            free(hex);
        }
    }

    krb5_free_data_contents(context, &salt);
    krb5_free_data_contents(context, &s2kparams);
    krb5_free_principal(context, princ);
    krb5_get_init_creds_opt_free(context, opt);
    krb5_free_context(context);
    free(etypes);
    return 0;
}
