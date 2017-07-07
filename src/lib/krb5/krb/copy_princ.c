/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/copy_princ.c */
/*
 * Copyright 1990, 2009 by the Massachusetts Institute of Technology.
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

/*
 * Copy a principal structure, with fresh allocation.
 */
krb5_error_code KRB5_CALLCONV
krb5_copy_principal(krb5_context context, krb5_const_principal inprinc, krb5_principal *outprinc)
{
    register krb5_principal tempprinc;
    krb5_int32 i;

    tempprinc = (krb5_principal)malloc(sizeof(krb5_principal_data));

    if (tempprinc == 0)
        return ENOMEM;

    *tempprinc = *inprinc;

    tempprinc->data = malloc(inprinc->length * sizeof(krb5_data));

    if (tempprinc->data == 0) {
        free(tempprinc);
        return ENOMEM;
    }

    for (i = 0; i < inprinc->length; i++) {
        if (krb5int_copy_data_contents(context, &inprinc->data[i],
                                       &tempprinc->data[i]) != 0) {
            while (--i >= 0)
                free(tempprinc->data[i].data);
            free (tempprinc->data);
            free (tempprinc);
            return ENOMEM;
        }
    }

    if (krb5int_copy_data_contents_add0(context, &inprinc->realm,
                                        &tempprinc->realm) != 0) {
        for (i = 0; i < inprinc->length; i++)
            free(tempprinc->data[i].data);
        free(tempprinc->data);
        free(tempprinc);
        return ENOMEM;
    }

    *outprinc = tempprinc;
    return 0;
}
