/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"

krb5_error_code KRB5_CALLCONV
krb5_cc_copy_creds(krb5_context context, krb5_ccache incc, krb5_ccache outcc)
{
    krb5_error_code code;
    krb5_cc_cursor cur = 0;
    krb5_creds creds;

    if ((code = krb5_cc_start_seq_get(context, incc, &cur)))
        goto cleanup;

    while (!(code = krb5_cc_next_cred(context, incc, &cur, &creds))) {
        code = krb5_cc_store_cred(context, outcc, &creds);
        krb5_free_cred_contents(context, &creds);
        if (code)
            goto cleanup;
    }

    if (code != KRB5_CC_END)
        goto cleanup;

    code = krb5_cc_end_seq_get(context, incc, &cur);
    cur = 0;
    if (code)
        goto cleanup;

    code = 0;

cleanup:
    /* If set then we are in an error pathway */
    if (cur)
        krb5_cc_end_seq_get(context, incc, &cur);

    return(code);
}
