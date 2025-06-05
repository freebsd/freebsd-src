/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/rtest.c */
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

#include "k5-int.h"
#include <stdio.h>
#include "kdc_util.h"
#include "extern.h"

void krb5_klog_syslog(void);

static krb5_principal
make_princ(krb5_context ctx, const char *str, const char *prog)
{
    krb5_principal ret;
    char *dat;

    if(!(ret = (krb5_principal) malloc(sizeof(krb5_principal_data)))) {
        com_err(prog, ENOMEM, "while allocating principal data");
        exit(3);
    }
    memset(ret, 0, sizeof(krb5_principal_data));

    /* We do not include the null... */
    if(!(dat = (char *) malloc(strlen(str)))) {
        com_err(prog, ENOMEM, "while allocating principal realm data");
        exit(3);
    }
    memcpy(dat, str, strlen(str));
    krb5_princ_set_realm_data(ctx, ret, dat);
    krb5_princ_set_realm_length(ctx, ret, strlen(str));

    return ret;
}

int
main(int argc, char **argv)
{
    krb5_data otrans;
    krb5_data ntrans;
    krb5_principal tgs, cl, sv;
    krb5_error_code kret;
    krb5_context ctx;

    if (argc < 4) {
        fprintf(stderr, "not enough args\n");
        exit(1);
    }


    /* Get a context */
    kret = krb5int_init_context_kdc(&ctx);
    if (kret) {
        com_err(argv[0], kret, "while getting krb5 context");
        exit(2);
    }

    ntrans.length = 0;
    ntrans.data = 0;

    otrans.length = strlen(argv[1]);
    if (otrans.length)
        otrans.data = (char *) malloc(otrans.length);
    else
        otrans.data = 0;
    memcpy(otrans.data,argv[1], otrans.length);

    tgs = make_princ(ctx, argv[2], argv[0]);
    cl  = make_princ(ctx, argv[3], argv[0]);
    sv  = make_princ(ctx, argv[4], argv[0]);

    add_to_transited(&otrans,&ntrans,tgs,cl,sv);

    printf("%s\n",ntrans.data);

    /* Free up all memory so we can profile for leaks */
    if (otrans.data)
        free(otrans.data);
    free(ntrans.data);

    krb5_free_principal(ctx, tgs);
    krb5_free_principal(ctx, cl);
    krb5_free_principal(ctx, sv);
    krb5_free_context(ctx);

    exit(0);
}

void krb5_klog_syslog(void) {}
kdc_realm_t *
find_realm_data(struct server_handle *handle,
                char *rname, krb5_ui_4 rsize)
{
    return 0;
}
