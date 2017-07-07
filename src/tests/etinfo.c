/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/etinfo.c - Test harness for KDC etype-info behavior */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

/*
 * Send an AS-REQ to the KDC for a specified principal, with an optionally
 * specified request enctype list.  Decode the output as either an AS-REP or a
 * KRB-ERROR and display the PA-ETYPE-INFO2, PA-ETYPE-INFO, and PA-PW-SALT
 * padata in the following format:
 *
 *     error/asrep etype-info2/etype-info/pw-salt enctype salt [s2kparams]
 *
 * enctype is omitted for PA-PW-SALT entries.  salt is displayed directly;
 * s2kparams is displayed in uppercase hex.
 */

#include "k5-int.h"

static krb5_context ctx;

static void
check(krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s\n", errmsg);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

static void
display_etinfo(krb5_etype_info_entry **list, const char *l1, const char *l2)
{
    krb5_etype_info_entry *info;
    char etname[256];
    unsigned int i;

    for (; *list != NULL; list++) {
        info = *list;
        check(krb5_enctype_to_name(info->etype, TRUE, etname, sizeof(etname)));
        printf("%s %s %s ", l1, l2, etname);
        if (info->length != KRB5_ETYPE_NO_SALT)
            printf("%.*s", info->length, info->salt);
        else
            printf("(default)");
        if (info->s2kparams.length > 0) {
            printf(" ");
            for (i = 0; i < info->s2kparams.length; i++)
                printf("%02X", (unsigned char)info->s2kparams.data[i]);
        }
        printf("\n");
    }
}

static void
display_padata(krb5_pa_data **pa_list, const char *label)
{
    krb5_pa_data *pa;
    krb5_data d;
    krb5_etype_info_entry **etinfo_list;

    for (; pa_list != NULL && *pa_list != NULL; pa_list++) {
        pa = *pa_list;
        d = make_data(pa->contents, pa->length);
        if (pa->pa_type == KRB5_PADATA_ETYPE_INFO2) {
            check(decode_krb5_etype_info2(&d, &etinfo_list));
            display_etinfo(etinfo_list, label, "etype_info2");
            krb5_free_etype_info(ctx, etinfo_list);
        } else if (pa->pa_type == KRB5_PADATA_ETYPE_INFO) {
            check(decode_krb5_etype_info(&d, &etinfo_list));
            display_etinfo(etinfo_list, label, "etype_info");
            krb5_free_etype_info(ctx, etinfo_list);
        } else if (pa->pa_type == KRB5_PADATA_PW_SALT) {
            printf("%s pw_salt %.*s\n", label, (int)d.length, d.data);
        } else if (pa->pa_type == KRB5_PADATA_AFS3_SALT) {
            printf("%s afs3_salt %.*s\n", label, (int)d.length, d.data);
        }
    }
}

int
main(int argc, char **argv)
{
    krb5_principal client;
    krb5_get_init_creds_opt *opt;
    krb5_init_creds_context icc;
    krb5_data reply, request, realm;
    krb5_error *error;
    krb5_kdc_rep *asrep;
    krb5_pa_data **padata;
    krb5_enctype *enctypes, def[] = { ENCTYPE_NULL };
    krb5_preauthtype pa_type = KRB5_PADATA_NONE;
    unsigned int flags;
    int master = 0;

    if (argc < 2 && argc > 4) {
        fprintf(stderr, "Usage: %s princname [enctypes] [patype]\n", argv[0]);
        exit(1);
    }
    check(krb5_init_context(&ctx));
    check(krb5_parse_name(ctx, argv[1], &client));
    if (argc >= 3) {
        check(krb5int_parse_enctype_list(ctx, "", argv[2], def, &enctypes));
        krb5_set_default_in_tkt_ktypes(ctx, enctypes);
        free(enctypes);
    }
    if (argc >= 4)
        pa_type = atoi(argv[3]);

    check(krb5_get_init_creds_opt_alloc(ctx, &opt));
    if (pa_type != KRB5_PADATA_NONE)
        krb5_get_init_creds_opt_set_preauth_list(opt, &pa_type, 1);

    check(krb5_init_creds_init(ctx, client, NULL, NULL, 0, opt, &icc));
    reply = empty_data();
    check(krb5_init_creds_step(ctx, icc, &reply, &request, &realm, &flags));
    assert(flags == KRB5_INIT_CREDS_STEP_FLAG_CONTINUE);
    check(krb5_sendto_kdc(ctx, &request, &realm, &reply, &master, 0));

    if (decode_krb5_error(&reply, &error) == 0) {
        decode_krb5_padata_sequence(&error->e_data, &padata);
        if (error->error == KDC_ERR_PREAUTH_REQUIRED) {
            display_padata(padata, "error");
        } else if (error->error == KDC_ERR_MORE_PREAUTH_DATA_REQUIRED) {
            display_padata(padata, "more");
        } else {
            fprintf(stderr, "Unexpected error %d\n", (int)error->error);
            return 1;
        }
        krb5_free_pa_data(ctx, padata);
        krb5_free_error(ctx, error);
    } else if (decode_krb5_as_rep(&reply, &asrep) == 0) {
        display_padata(asrep->padata, "asrep");
        krb5_free_kdc_rep(ctx, asrep);
    } else {
        abort();
    }

    krb5_free_data_contents(ctx, &request);
    krb5_free_data_contents(ctx, &reply);
    krb5_free_data_contents(ctx, &realm);
    krb5_get_init_creds_opt_free(ctx, opt);
    krb5_init_creds_free(ctx, icc);
    krb5_free_principal(ctx, client);
    krb5_free_context(ctx);
    return 0;
}
