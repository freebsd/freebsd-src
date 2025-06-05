/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/set_ccache.c */
/*
 * Copyright 1999, 2003 by the Massachusetts Institute of Technology.
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
 * Set ccache name used by gssapi, and optionally obtain old ccache
 * name.  Caller must not free returned name.
 */

#include <string.h>
#include "gssapiP_krb5.h"

OM_uint32
gss_krb5int_ccache_name(OM_uint32 *minor_status,
                        const gss_OID desired_mech,
                        const gss_OID desired_object,
                        const gss_buffer_t value)
{
    OM_uint32 err = 0;
    struct krb5_gss_ccache_name_req *req;
    char *old_name, *cur_name = NULL;

    err = gss_krb5int_initialize_library();
    if (err) {
        *minor_status = err;
        return GSS_S_FAILURE;
    }

    assert(value->length == sizeof(*req));

    if (value->length != sizeof(*req))
        return GSS_S_FAILURE;

    req = (struct krb5_gss_ccache_name_req *)value->value;

    /* Our job is simple if the caller doesn't want the current name. */
    if (req->out_name == NULL)
        return kg_set_ccache_name(minor_status, req->name);

    /* Fetch the current name and change it. */
    kg_get_ccache_name(&err, &cur_name);
    if (err)
        goto cleanup;
    kg_set_ccache_name(&err, req->name);
    if (err)
        goto cleanup;

    /* Store the current name in a thread-specific variable.  Free that
     * variable's previous contents. */
    old_name = k5_getspecific(K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME);
    err = k5_setspecific(K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME, cur_name);
    if (err)
        goto cleanup;
    free(old_name);

    /* Give the caller an alias to the stored value. */
    *req->out_name = cur_name;
    cur_name = NULL;
    err = 0;

cleanup:
    free(cur_name);
    *minor_status = err;
    return (*minor_status == 0) ? GSS_S_COMPLETE : GSS_S_FAILURE;
}
