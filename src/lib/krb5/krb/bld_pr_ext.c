/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/bld_pr_ext.c */
/*
 * Copyright 1991, 2008, 2009 by the Massachusetts Institute of Technology.
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

/*
 *
 * Build a principal from a list of lengths and strings
 */

#include "k5-int.h"

#include <stdarg.h>

krb5_error_code KRB5_CALLCONV_C
krb5_build_principal_ext(krb5_context context,  krb5_principal * princ,
                         unsigned int rlen, const char * realm, ...)
{
    va_list ap;
    int i, count = 0;
    krb5_data *princ_data;
    krb5_principal princ_ret;
    krb5_data tmpdata;

    va_start(ap, realm);
    /* count up */
    while (va_arg(ap, int) != 0) {
        (void)va_arg(ap, char *);               /* pass one up */
        count++;
    }
    va_end(ap);

    /* we do a 2-pass to avoid the need to guess on allocation needs
       cf. bld_princ.c */
    /* get space for array */
    princ_data = (krb5_data *) malloc(sizeof(krb5_data) * count);
    if (!princ_data)
        return ENOMEM;
    princ_ret = (krb5_principal) malloc(sizeof(krb5_principal_data));
    if (!princ_ret) {
        free(princ_data);
        return ENOMEM;
    }
    princ_ret->data = princ_data;
    princ_ret->length = count;
    tmpdata.length = rlen;
    tmpdata.data = (char *) realm;
    if (krb5int_copy_data_contents_add0(context, &tmpdata, &princ_ret->realm) != 0) {
        free(princ_data);
        free(princ_ret);
        return ENOMEM;
    }

    /* process rest of components */
    va_start(ap, realm);
    for (i = 0; i < count; i++) {
        tmpdata.length = va_arg(ap, unsigned int);
        tmpdata.data = va_arg(ap, char *);
        if (krb5int_copy_data_contents_add0(context, &tmpdata,
                                            &princ_data[i]) != 0)
            goto free_out;
    }
    va_end(ap);
    *princ = princ_ret;
    princ_ret->type = KRB5_NT_UNKNOWN;
    return 0;

free_out:
    while (--i >= 0)
        free(princ_data[i].data);
    free(princ_data);
    free(princ_ret->realm.data);
    free(princ_ret);
    va_end(ap);
    return ENOMEM;
}
