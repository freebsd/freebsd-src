/** @example tkt_creds.c
 *
 *  Usage example for krb5_tkt_creds function family
 */
#include "krb5.h"

krb5_error_code
func(krb5_context context, krb5_flags options,
     krb5_ccache ccache, krb5_creds *in_creds,
     krb5_creds **out_creds)
{
    krb5_error_code code = KRB5_OK;
    krb5_creds *ncreds = NULL;
    krb5_tkt_creds_context ctx = NULL;

    *out_creds = NULL;

    /* Allocate a container. */
    ncreds = k5alloc(sizeof(*ncreds), &code);
    if (ncreds == NULL)
        goto cleanup;

    /* Make and execute a krb5_tkt_creds context to get the credential. */
    code = krb5_tkt_creds_init(context, ccache, in_creds, options, &ctx);
    if (code != KRB5_OK)
        goto cleanup;
    code = krb5_tkt_creds_get(context, ctx);
    if (code != KRB5_OK)
        goto cleanup;
    code = krb5_tkt_creds_get_creds(context, ctx, ncreds);
    if (code != KRB5_OK)
        goto cleanup;

    *out_creds = ncreds;
    ncreds = NULL;

cleanup:
    krb5_free_creds(context, ncreds);
    krb5_tkt_creds_free(context, ctx);
    return code;
}

/* Allocate zeroed memory; set *code to 0 on success or ENOMEM on failure. */
static inline void *
k5alloc(size_t len, krb5_error_code *code)
{
    void *ptr;

    /* Allocate at least one byte since zero-byte allocs may return NULL. */
    ptr = calloc((len > 0) ? len : 1, 1);
    *code = (ptr == NULL) ? ENOMEM : 0;
    return ptr;
}


