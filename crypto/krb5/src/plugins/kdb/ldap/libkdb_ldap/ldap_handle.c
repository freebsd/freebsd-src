/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_handle.c */
/*
 * Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ldap_main.h"

/*
 * Return ldap server handle from the pool. If the pool is exhausted return NULL.
 * Do not lock the mutex, caller should lock it
 */

static krb5_ldap_server_handle *
krb5_get_ldap_handle(krb5_ldap_context *ldap_context)
{
    krb5_ldap_server_handle    *ldap_server_handle=NULL;
    krb5_ldap_server_info      *ldap_server_info=NULL;
    int                        cnt=0;

    while (ldap_context->server_info_list[cnt] != NULL) {
        ldap_server_info = ldap_context->server_info_list[cnt];
        if (ldap_server_info->server_status != OFF) {
            if (ldap_server_info->ldap_server_handles != NULL) {
                ldap_server_handle = ldap_server_info->ldap_server_handles;
                ldap_server_info->ldap_server_handles = ldap_server_handle->next;
                break;
            }
        }
        ++cnt;
    }
    return ldap_server_handle;
}

/*
 * This is called in case krb5_get_ldap_handle returns NULL.
 * Try getting a single connection (handle) and return the same by
 * calling krb5_get_ldap_handle function.
 * Do not lock the mutex here. The caller should lock it
 */

static krb5_ldap_server_handle *
krb5_retry_get_ldap_handle(krb5_ldap_context *ldap_context,
                           krb5_error_code *st)
{
    krb5_ldap_server_handle    *ldap_server_handle=NULL;

    if ((*st=krb5_ldap_db_single_init(ldap_context)) != 0)
        return NULL;

    ldap_server_handle = krb5_get_ldap_handle(ldap_context);
    return ldap_server_handle;
}

/*
 * Put back the ldap server handle to the front of the list of handles of the
 * ldap server info structure.
 * Do not lock the mutex here. The caller should lock it.
 */

static krb5_error_code
krb5_put_ldap_handle(krb5_ldap_server_handle *ldap_server_handle)
{

    if (ldap_server_handle == NULL)
        return 0;

    ldap_server_handle->next = ldap_server_handle->server_info->ldap_server_handles;
    ldap_server_handle->server_info->ldap_server_handles = ldap_server_handle;
    return 0;
}

/*
 * Free up all the ldap server handles of the server info.
 * This function is called when the ldap server returns LDAP_SERVER_DOWN.
 */

static krb5_error_code
krb5_ldap_cleanup_handles(krb5_ldap_server_info *ldap_server_info)
{
    krb5_ldap_server_handle    *ldap_server_handle = NULL;

    while (ldap_server_info->ldap_server_handles != NULL) {
        ldap_server_handle = ldap_server_info->ldap_server_handles;
        ldap_server_info->ldap_server_handles = ldap_server_handle->next;
        /* ldap_unbind_s(ldap_server_handle); */
        free (ldap_server_handle);
        ldap_server_handle = NULL;
    }
    return 0;
}

/*
 * wrapper function called from outside to get a handle.
 */

krb5_error_code
krb5_ldap_request_handle_from_pool(krb5_ldap_context *ldap_context,
                                   krb5_ldap_server_handle **
                                   ldap_server_handle)
{
    krb5_error_code            st=0;

    *ldap_server_handle = NULL;

    HNDL_LOCK(ldap_context);
    if (((*ldap_server_handle)=krb5_get_ldap_handle(ldap_context)) == NULL)
        (*ldap_server_handle)=krb5_retry_get_ldap_handle(ldap_context, &st);
    HNDL_UNLOCK(ldap_context);
    return st;
}

/*
 * wrapper function wrapper called to get the next ldap server handle, when the current
 * ldap server handle returns LDAP_SERVER_DOWN.
 */

krb5_error_code
krb5_ldap_request_next_handle_from_pool(krb5_ldap_context *ldap_context,
                                        krb5_ldap_server_handle **
                                        ldap_server_handle)
{
    krb5_error_code            st=0;

    HNDL_LOCK(ldap_context);
    (*ldap_server_handle)->server_info->server_status = OFF;
    time(&(*ldap_server_handle)->server_info->downtime);
    krb5_put_ldap_handle(*ldap_server_handle);
    krb5_ldap_cleanup_handles((*ldap_server_handle)->server_info);

    if (((*ldap_server_handle)=krb5_get_ldap_handle(ldap_context)) == NULL)
        (*ldap_server_handle)=krb5_retry_get_ldap_handle(ldap_context, &st);
    HNDL_UNLOCK(ldap_context);
    return st;
}

/*
 * wrapper function to call krb5_put_ldap_handle.
 */

void
krb5_ldap_put_handle_to_pool(krb5_ldap_context *ldap_context,
                             krb5_ldap_server_handle *ldap_server_handle)
{
    if (ldap_server_handle != NULL) {
        HNDL_LOCK(ldap_context);
        krb5_put_ldap_handle(ldap_server_handle);
        HNDL_UNLOCK(ldap_context);
    }
    return;
}
