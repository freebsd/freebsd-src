/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/hostream_registry.c - registry hostrealm module */
/*
 * Copyright (C) 2015 by the Massachusetts Institute
 * of Technology.  All rights reserved.
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

/*
 * This file implements the built-in registry module for the hostrealm
 * interface, which uses Windows registry configuration to determine the
 * local default realm.
 */

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/hostrealm_plugin.h>

#ifdef _WIN32
/*
 * Look up a default_realm entry starting from the given base key.
 * The output *buf_out will be non-NULL if an entry was found; the
 * caller is responsible for freeing *pbuffer.
 *
 * On success, return 0 and set *str_out to the default realm to be
 * used.  Return KRB5_PLUGIN_NO_HANDLE if the registry key does not
 * exist, or another code if it cannot be read.
 */
static krb5_error_code
get_from_registry(HKEY hBaseKey, char **str_out)
{
    DWORD bsize = 0;
    LONG rc;
    krb5_error_code ret;
    char *str = NULL;
    const char *path = "Software\\MIT\\Kerberos5";
    const char *value = "default_realm";

    *str_out = NULL;

    /* Call with zero size to determine the amount of storage needed. */
    rc = RegGetValue(hBaseKey, path, value, RRF_RT_REG_SZ, NULL, str,
                     &bsize);
    if (rc == ERROR_FILE_NOT_FOUND)
        return KRB5_PLUGIN_NO_HANDLE;
    if (FAILED(rc) || bsize <= 0)
        return EIO;
    str = malloc(bsize);
    if (str == NULL)
        return ENOMEM;
    rc = RegGetValue(hBaseKey, path, value, RRF_RT_REG_SZ, NULL, str,
                     &bsize);
    if (FAILED(rc)) {
        ret = EIO;
        goto cleanup;
    }

    ret = 0;
    *str_out = str;
    str = NULL;
cleanup:
    free(str);
    return ret;
}

/* Look up the default_realm variable in the
 * {HKLM,HKCU}\Software\MIT\Kerberos5\default_realm registry values. */
static krb5_error_code
registry_default_realm(krb5_context context, krb5_hostrealm_moddata data,
                       char ***realms_out)
{
    krb5_error_code ret;
    char *prof_realm;

    *realms_out = NULL;
    ret = get_from_registry(HKEY_LOCAL_MACHINE, &prof_realm);
    if (ret == KRB5_PLUGIN_NO_HANDLE)
        ret = get_from_registry(HKEY_CURRENT_USER, &prof_realm);
    if (ret)
        return ret;
    ret = k5_make_realmlist(prof_realm, realms_out);
    free(prof_realm);
    return ret;
}
#else /* _WIN32 */
static krb5_error_code
registry_default_realm(krb5_context context, krb5_hostrealm_moddata data,
                       char ***realms_out)
{
        return KRB5_PLUGIN_NO_HANDLE;
}
#endif /* _WIN32 */

static void
registry_free_realmlist(krb5_context context, krb5_hostrealm_moddata data,
                       char **list)
{
    krb5_free_host_realm(context, list);
}

krb5_error_code
hostrealm_registry_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_hostrealm_vtable vt = (krb5_hostrealm_vtable)vtable;

    vt->name = "registry";
    vt->default_realm = registry_default_realm;
    vt->free_list = registry_free_realmlist;
    return 0;
}
