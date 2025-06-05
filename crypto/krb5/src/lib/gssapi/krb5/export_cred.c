/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/export_cred.c - krb5 export_cred implementation */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "k5-json.h"
#include "gssapiP_krb5.h"

/* Return a JSON null or array value representing princ. */
static krb5_error_code
json_principal(krb5_context context, krb5_principal princ,
               k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_string str = NULL;
    char *princname;

    *val_out = NULL;
    if (princ == NULL)
        return k5_json_null_create_val(val_out);
    ret = krb5_unparse_name(context, princ, &princname);
    if (ret)
        return ret;
    ret = k5_json_string_create(princname, &str);
    krb5_free_unparsed_name(context, princname);
    *val_out = str;
    return ret;
}

/* Return a json null or array value representing etypes. */
static krb5_error_code
json_etypes(krb5_enctype *etypes, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_number num;
    k5_json_array array;

    *val_out = NULL;
    if (etypes == NULL)
        return k5_json_null_create_val(val_out);
    ret = k5_json_array_create(&array);
    if (ret)
        return ret;
    for (; *etypes != 0; etypes++) {
        ret = k5_json_number_create(*etypes, &num);
        if (ret)
            goto err;
        ret = k5_json_array_add(array, num);
        k5_json_release(num);
        if (ret)
            goto err;
    }
    *val_out = array;
    return 0;
err:
    k5_json_release(array);
    return ret;
}

/* Return a JSON null or array value representing name. */
static krb5_error_code
json_kgname(krb5_context context, krb5_gss_name_t name, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array = NULL;
    k5_json_value princ;

    *val_out = NULL;
    if (name == NULL)
        return k5_json_null_create_val(val_out);
    ret = json_principal(context, name->princ, &princ);
    if (ret)
        return ret;
    ret = k5_json_array_fmt(&array, "vss", princ, name->service, name->host);
    k5_json_release(princ);
    *val_out = array;
    return ret;
}

/* Return a JSON null or string value representing keytab. */
static krb5_error_code
json_keytab(krb5_context context, krb5_keytab keytab, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_string str;
    char name[1024];

    *val_out = NULL;
    if (keytab == NULL)
        return k5_json_null_create_val(val_out);
    ret = krb5_kt_get_name(context, keytab, name, sizeof(name));
    if (ret)
        return ret;
    ret = k5_json_string_create(name, &str);
    *val_out = str;
    return ret;
}

/* Return a JSON null or string value representing rcache. */
static krb5_error_code
json_rcache(krb5_context context, krb5_rcache rcache, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_string str = NULL;

    if (rcache == NULL)
        return k5_json_null_create_val(val_out);
    ret = k5_json_string_create(k5_rc_get_name(context, rcache), &str);
    *val_out = str;
    return ret;
}

/* Return a JSON array value representing keyblock. */
static krb5_error_code
json_keyblock(krb5_keyblock *kb, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;

    *val_out = NULL;
    ret = k5_json_array_fmt(&array, "iB", kb->enctype, (void *)kb->contents,
                            (size_t)kb->length);
    if (ret)
        return ret;
    *val_out = array;
    return 0;
}

/* Return a JSON array value representing addr. */
static krb5_error_code
json_address(krb5_address *addr, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;

    *val_out = NULL;
    ret = k5_json_array_fmt(&array, "iB", addr->addrtype,
                            (void *)addr->contents, (size_t)addr->length);
    if (ret)
        return ret;
    *val_out = array;
    return 0;
}

/* Return a JSON null or array value representing addrs. */
static krb5_error_code
json_addresses(krb5_address **addrs, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;
    k5_json_value val;

    *val_out = NULL;
    if (addrs == NULL)
        return k5_json_null_create_val(val_out);
    ret = k5_json_array_create(&array);
    if (ret)
        return ret;
    for (; *addrs != NULL; addrs++) {
        ret = json_address(*addrs, &val);
        if (ret)
            goto err;
        ret = k5_json_array_add(array, val);
        k5_json_release(val);
        if (ret)
            goto err;
    }
    *val_out = array;
    return 0;
err:
    k5_json_release(array);
    return ret;
}

/* Return a JSON array value representing ad. */
static krb5_error_code
json_authdata_element(krb5_authdata *ad, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;

    *val_out = NULL;
    ret = k5_json_array_fmt(&array, "iB", ad->ad_type, (void *)ad->contents,
                            (size_t)ad->length);
    if (ret)
        return ret;
    *val_out = array;
    return 0;
}

/* Return a JSON null or array value representing authdata. */
static krb5_error_code
json_authdata(krb5_authdata **authdata, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;
    k5_json_value val;

    *val_out = NULL;
    if (authdata == NULL)
        return k5_json_null_create_val(val_out);
    ret = k5_json_array_create(&array);
    if (ret)
        return ret;
    for (; *authdata != NULL; authdata++) {
        ret = json_authdata_element(*authdata, &val);
        if (ret)
            goto err;
        ret = k5_json_array_add(array, val);
        k5_json_release(val);
        if (ret)
            goto err;
    }
    *val_out = array;
    return 0;
err:
    k5_json_release(array);
    return ret;
}

/* Return a JSON array value representing creds. */
static krb5_error_code
json_creds(krb5_context context, krb5_creds *creds, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;
    k5_json_value client = NULL, server = NULL, keyblock = NULL, addrs = NULL;
    k5_json_value authdata = NULL;

    *val_out = NULL;
    ret = json_principal(context, creds->client, &client);
    if (ret)
        goto cleanup;
    ret = json_principal(context, creds->server, &server);
    if (ret)
        goto cleanup;
    ret = json_keyblock(&creds->keyblock, &keyblock);
    if (ret)
        goto cleanup;
    ret = json_addresses(creds->addresses, &addrs);
    if (ret)
        goto cleanup;
    ret = json_authdata(creds->authdata, &authdata);
    if (ret)
        goto cleanup;

    ret = k5_json_array_fmt(&array, "vvviiiibivBBv", client, server, keyblock,
                            creds->times.authtime, creds->times.starttime,
                            creds->times.endtime, creds->times.renew_till,
                            creds->is_skey, creds->ticket_flags, addrs,
                            (void *)creds->ticket.data,
                            (size_t)creds->ticket.length,
                            (void *)creds->second_ticket.data,
                            (size_t)creds->second_ticket.length, authdata);
    if (ret)
        goto cleanup;
    *val_out = array;

cleanup:
    k5_json_release(client);
    k5_json_release(server);
    k5_json_release(keyblock);
    k5_json_release(addrs);
    k5_json_release(authdata);
    return ret;
}

/* Return a JSON array value representing the contents of ccache. */
static krb5_error_code
json_ccache_contents(krb5_context context, krb5_ccache ccache,
                     k5_json_value *val_out)
{
    krb5_error_code ret;
    krb5_principal princ;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    k5_json_array array;
    k5_json_value val;

    *val_out = NULL;
    ret = k5_json_array_create(&array);
    if (ret)
        return ret;

    /* Put the principal in the first array entry. */
    ret = krb5_cc_get_principal(context, ccache, &princ);
    if (ret)
        goto err;
    ret = json_principal(context, princ, &val);
    krb5_free_principal(context, princ);
    if (ret)
        goto err;
    ret = k5_json_array_add(array, val);
    k5_json_release(val);
    if (ret)
        goto err;

    /* Put credentials in the remaining array entries. */
    ret = krb5_cc_start_seq_get(context, ccache, &cursor);
    if (ret)
        goto err;
    while ((ret = krb5_cc_next_cred(context, ccache, &cursor, &creds)) == 0) {
        ret = json_creds(context, &creds, &val);
        krb5_free_cred_contents(context, &creds);
        if (ret)
            break;
        ret = k5_json_array_add(array, val);
        k5_json_release(val);
        if (ret)
            break;
    }
    krb5_cc_end_seq_get(context, ccache, &cursor);
    if (ret != KRB5_CC_END)
        goto err;
    *val_out = array;
    return 0;

err:
    k5_json_release(array);
    return ret;
}

/* Return a JSON null, string, or array value representing ccache. */
static krb5_error_code
json_ccache(krb5_context context, krb5_ccache ccache, k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_string str;
    char *name;

    *val_out = NULL;
    if (ccache == NULL)
        return k5_json_null_create_val(val_out);
    if (strcmp(krb5_cc_get_type(context, ccache), "MEMORY") == 0) {
        return json_ccache_contents(context, ccache, val_out);
    } else {
        ret = krb5_cc_get_full_name(context, ccache, &name);
        if (ret)
            return ret;
        ret = k5_json_string_create(name, &str);
        free(name);
        *val_out = str;
        return ret;
    }
}

/* Return a JSON array value representing cred. */
static krb5_error_code
json_kgcred(krb5_context context, krb5_gss_cred_id_t cred,
            k5_json_value *val_out)
{
    krb5_error_code ret;
    k5_json_array array;
    k5_json_value name = NULL, imp = NULL, keytab = NULL, rcache = NULL;
    k5_json_value ccache = NULL, ckeytab = NULL, etypes = NULL;

    *val_out = NULL;
    ret = json_kgname(context, cred->name, &name);
    if (ret)
        goto cleanup;
    ret = json_principal(context, cred->impersonator, &imp);
    if (ret)
        goto cleanup;
    ret = json_keytab(context, cred->keytab, &keytab);
    if (ret)
        goto cleanup;
    ret = json_rcache(context, cred->rcache, &rcache);
    if (ret)
        goto cleanup;
    ret = json_ccache(context, cred->ccache, &ccache);
    if (ret)
        goto cleanup;
    ret = json_keytab(context, cred->client_keytab, &ckeytab);
    if (ret)
        goto cleanup;
    ret = json_etypes(cred->req_enctypes, &etypes);
    if (ret)
        goto cleanup;

    ret = k5_json_array_fmt(&array, "ivvbbvvvvbLLvs", cred->usage, name, imp,
                            cred->default_identity, cred->iakerb_mech, keytab,
                            rcache, ccache, ckeytab, cred->have_tgt,
                            (long long)ts2tt(cred->expire),
                            (long long)ts2tt(cred->refresh_time), etypes,
                            cred->password);
    if (ret)
        goto cleanup;
    *val_out = array;

cleanup:
    k5_json_release(name);
    k5_json_release(imp);
    k5_json_release(keytab);
    k5_json_release(rcache);
    k5_json_release(ccache);
    k5_json_release(ckeytab);
    k5_json_release(etypes);
    return ret;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_export_cred(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
                     gss_buffer_t token)
{
    OM_uint32 status = GSS_S_COMPLETE;
    krb5_context context;
    krb5_error_code ret;
    krb5_gss_cred_id_t cred;
    k5_json_array array = NULL;
    k5_json_value jcred = NULL;
    char *str = NULL;
    krb5_data d;

    ret = krb5_gss_init_context(&context);
    if (ret) {
        *minor_status = ret;
        return GSS_S_FAILURE;
    }

    /* Validate and lock cred_handle. */
    status = krb5_gss_validate_cred_1(minor_status, cred_handle, context);
    if (status != GSS_S_COMPLETE)
        return status;
    cred = (krb5_gss_cred_id_t)cred_handle;

    if (json_kgcred(context, cred, &jcred))
        goto oom;
    if (k5_json_array_fmt(&array, "sv", CRED_EXPORT_MAGIC, jcred))
        goto oom;
    if (k5_json_encode(array, &str))
        goto oom;
    d = string2data(str);
    if (data_to_gss(&d, token))
        goto oom;
    str = NULL;

cleanup:
    free(str);
    k5_mutex_unlock(&cred->lock);
    k5_json_release(array);
    k5_json_release(jcred);
    krb5_free_context(context);
    return status;

oom:
    *minor_status = ENOMEM;
    status = GSS_S_FAILURE;
    goto cleanup;
}
