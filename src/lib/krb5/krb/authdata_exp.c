/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2009 by the Massachusetts Institute of Technology.  All
 * Rights Reserved.
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
 *
 * krb5_authdata_export_authdata()
 */

#include "k5-int.h"
#include "authdata.h"
#include "auth_con.h"
#include "int-proto.h"

krb5_error_code KRB5_CALLCONV
krb5_authdata_export_authdata(krb5_context kcontext,
                              krb5_authdata_context context,
                              krb5_flags flags,
                              krb5_authdata ***pauthdata)
{
    int i;
    krb5_error_code code = 0;
    krb5_authdata **authdata = NULL;
    unsigned int len = 0;

    *pauthdata = NULL;

    for (i = 0; i < context->n_modules; i++) {
        struct _krb5_authdata_context_module *module = &context->modules[i];
        krb5_authdata **authdata2 = NULL;
        int j;

        if ((module->flags & flags) == 0)
            continue;

        if (module->ftable->export_authdata == NULL)
            continue;

        code = (*module->ftable->export_authdata)(kcontext,
                                                  context,
                                                  module->plugin_context,
                                                  *(module->request_context_pp),
                                                  flags,
                                                  &authdata2);
        if (code == ENOENT)
            code = 0;
        else if (code != 0)
            break;

        if (authdata2 == NULL)
            continue;

        for (j = 0; authdata2[j] != NULL; j++)
            ;

        authdata = realloc(authdata, (len + j + 1) * sizeof(krb5_authdata *));
        if (authdata == NULL)
            return ENOMEM;

        memcpy(&authdata[len], authdata2, j * sizeof(krb5_authdata *));
        free(authdata2);

        len += j;
    }

    if (authdata != NULL)
        authdata[len] = NULL;

    if (code != 0) {
        krb5_free_authdata(kcontext, authdata);
        return code;
    }

    *pauthdata = authdata;

    return 0;
}
