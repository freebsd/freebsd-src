/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_base.c */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * Base "glue" functions for the replay cache.
 */

#include "k5-int.h"
#include "rc-int.h"
#include "k5-thread.h"
#include "../os/os-proto.h"

struct typelist {
    const krb5_rc_ops *ops;
    struct typelist *next;
};
static struct typelist none = { &k5_rc_none_ops, 0 };
static struct typelist file2 = { &k5_rc_file2_ops, &none };
static struct typelist dfl = { &k5_rc_dfl_ops, &file2 };
static struct typelist *typehead = &dfl;

krb5_error_code
k5_rc_default(krb5_context context, krb5_rcache *rc_out)
{
    krb5_error_code ret;
    const char *val;
    char *profstr, *rcname;

    *rc_out = NULL;

    /* If KRB5RCACHENAME is set in the environment, resolve it. */
    val = secure_getenv("KRB5RCACHENAME");
    if (val != NULL)
        return k5_rc_resolve(context, val, rc_out);

    /* If KRB5RCACHETYPE is set in the environment, resolve it with an empty
     * residual (primarily to support KRB5RCACHETYPE=none). */
    val = secure_getenv("KRB5RCACHETYPE");
    if (val != NULL) {
        if (asprintf(&rcname, "%s:", val) < 0)
            return ENOMEM;
        ret = k5_rc_resolve(context, rcname, rc_out);
        free(rcname);
        return ret;
    }

    /* If [libdefaults] default_rcache_name is set, expand path tokens in the
     * value and resolve it. */
    if (profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                           KRB5_CONF_DEFAULT_RCACHE_NAME, NULL, NULL,
                           &profstr) == 0 && profstr != NULL) {
        ret = k5_expand_path_tokens(context, profstr, &rcname);
        profile_release_string(profstr);
        if (ret)
            return ret;
        ret = k5_rc_resolve(context, rcname, rc_out);
        free(rcname);
        return ret;
    }

    /* Resolve the default type with no residual. */
    return k5_rc_resolve(context, "dfl:", rc_out);
}


krb5_error_code
k5_rc_resolve(krb5_context context, const char *name, krb5_rcache *rc_out)
{
    krb5_error_code ret;
    struct typelist *t;
    const char *sep;
    size_t len;
    krb5_rcache rc = NULL;

    *rc_out = NULL;

    sep = strchr(name, ':');
    if (sep == NULL)
        return KRB5_RC_PARSE;
    len = sep - name;

    for (t = typehead; t != NULL; t = t->next) {
        if (strncmp(t->ops->type, name, len) == 0 && t->ops->type[len] == '\0')
            break;
    }
    if (t == NULL)
        return KRB5_RC_TYPE_NOTFOUND;

    rc = k5alloc(sizeof(*rc), &ret);
    if (rc == NULL)
        goto error;
    rc->name = strdup(name);
    if (rc->name == NULL) {
        ret = ENOMEM;
        goto error;
    }
    ret = t->ops->resolve(context, sep + 1, &rc->data);
    if (ret)
        goto error;
    rc->ops = t->ops;
    rc->magic = KV5M_RCACHE;

    *rc_out = rc;
    return 0;

error:
    if (rc != NULL) {
        free(rc->name);
        free(rc);
    }
    return ret;
}

void
k5_rc_close(krb5_context context, krb5_rcache rc)
{
    rc->ops->close(context, rc->data);
    free(rc->name);
    free(rc);
}

krb5_error_code
k5_rc_store(krb5_context context, krb5_rcache rc,
            const krb5_enc_data *authenticator)
{
    krb5_error_code ret;
    krb5_data tag;

    ret = k5_rc_tag_from_ciphertext(context, authenticator, &tag);
    if (ret)
        return ret;
    return rc->ops->store(context, rc->data, &tag);
}

const char *
k5_rc_get_name(krb5_context context, krb5_rcache rc)
{
    return rc->name;
}

krb5_error_code
k5_rc_tag_from_ciphertext(krb5_context context, const krb5_enc_data *enc,
                          krb5_data *tag_out)
{
    krb5_error_code ret;
    const krb5_data *cdata = &enc->ciphertext;
    unsigned int len;

    *tag_out = empty_data();

    ret = krb5_c_crypto_length(context, enc->enctype,
                               KRB5_CRYPTO_TYPE_CHECKSUM, &len);
    if (ret)
        return ret;
    if (cdata->length < len)
        return EINVAL;
    *tag_out = make_data(cdata->data + cdata->length - len, len);
    return 0;
}

/*
 * Stub functions for former internal replay cache functions used by OpenSSL
 * (despite the lack of prototypes) before the OpenSSL 1.1 release.
 */

krb5_error_code krb5_rc_default(krb5_context, krb5_rcache *);
krb5_error_code KRB5_CALLCONV krb5_rc_destroy(krb5_context, krb5_rcache);
krb5_error_code KRB5_CALLCONV krb5_rc_get_lifespan(krb5_context, krb5_rcache,
                                                   krb5_deltat *);
krb5_error_code KRB5_CALLCONV krb5_rc_initialize(krb5_context, krb5_rcache,
                                                 krb5_deltat);

krb5_error_code
krb5_rc_default(krb5_context context, krb5_rcache *rc)
{
    return EINVAL;
}

krb5_error_code KRB5_CALLCONV
krb5_rc_destroy(krb5_context context, krb5_rcache rc)
{
    return EINVAL;
}

krb5_error_code KRB5_CALLCONV
krb5_rc_get_lifespan(krb5_context context, krb5_rcache rc, krb5_deltat *span)
{
    return EINVAL;
}

krb5_error_code KRB5_CALLCONV
krb5_rc_initialize(krb5_context context, krb5_rcache rc, krb5_deltat span)
{
    return EINVAL;
}
