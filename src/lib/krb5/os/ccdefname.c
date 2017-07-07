/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/ccdefname.c - Return default credential cache name */
/*
 * Copyright 1990, 2007 by the Massachusetts Institute of Technology.
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

#define NEED_WINDOWS
#include "k5-int.h"
#include "../ccache/cc-int.h"
#include "os-proto.h"
#include <stdio.h>

#if defined(_WIN32)
static int get_from_registry_indirect(char *name_buf, int name_size)
{
    /* If the RegKRB5CCNAME variable is set, it will point to
     * the registry key that has the name of the cache to use.
     * The Gradient PC-DCE sets the registry key
     * [HKEY_CURRENT_USER\Software\Gradient\DCE\Default\KRB5CCNAME]
     * to point at the cache file name (including the FILE: prefix).
     * By indirecting with the RegKRB5CCNAME entry in kerberos.ini,
     * we can accomodate other versions that might set a registry
     * variable.
     */
    char newkey[256];

    LONG name_buf_size;
    HKEY hkey;
    int found = 0;
    char *cp;

    newkey[0] = 0;
    GetPrivateProfileString(INI_FILES, "RegKRB5CCNAME", "",
                            newkey, sizeof(newkey), KERBEROS_INI);
    if (!newkey[0])
        return 0;

    newkey[sizeof(newkey)-1] = 0;
    cp = strrchr(newkey,'\\');
    if (cp) {
        *cp = '\0'; /* split the string */
        cp++;
    } else
        cp = "";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, newkey, 0,
                     KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
        return 0;

    name_buf_size = name_size;
    if (RegQueryValueEx(hkey, cp, 0, 0,
                        name_buf, &name_buf_size) != ERROR_SUCCESS)
    {
        RegCloseKey(hkey);
        return 0;
    }

    RegCloseKey(hkey);
    return 1;
}

static const char *key_path = "Software\\MIT\\Kerberos5";
static const char *value_name = "ccname";
static int
set_to_registry(
    HKEY hBaseKey,
    const char *name_buf
)
{
    HRESULT result;
    HKEY hKey;

    if ((result = RegCreateKeyEx(hBaseKey, key_path, 0, NULL,
                                 REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                                 &hKey, NULL)) != ERROR_SUCCESS) {
        return 0;
    }
    if (RegSetValueEx(hKey, value_name, 0, REG_SZ, name_buf,
                      strlen(name_buf)+1) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return 0;
    }
    RegCloseKey(hKey);
    return 1;
}


/*
 * get_from_registry
 *
 * This will find the ccname in the registry.  Returns 0 on error, non-zero
 * on success.
 */

static int
get_from_registry(
    HKEY hBaseKey,
    char *name_buf,
    int name_size
)
{
    HKEY hKey;
    DWORD name_buf_size = (DWORD)name_size;

    if (RegOpenKeyEx(hBaseKey, key_path, 0, KEY_QUERY_VALUE,
                     &hKey) != ERROR_SUCCESS)
        return 0;
    if (RegQueryValueEx(hKey, value_name, 0, 0,
                        name_buf, &name_buf_size) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return 0;
    }
    RegCloseKey(hKey);
    return 1;
}

#define APPEND_KRB5CC "\\krb5cc"

static int
try_dir(
    char* dir,
    char* buffer,
    int buf_len
)
{
    struct _stat s;
    if (!dir)
        return 0;
    if (_stat(dir, &s))
        return 0;
    if (!(s.st_mode & _S_IFDIR))
        return 0;
    if (buffer != dir) {
        strncpy(buffer, dir, buf_len);
        buffer[buf_len-1]='\0';
    }
    strncat(buffer, APPEND_KRB5CC, buf_len-strlen(buffer));
    buffer[buf_len-1] = '\0';
    return 1;
}

static krb5_error_code
get_from_os_buffer(char *name_buf, unsigned int name_size)
{
    char *prefix = krb5_cc_dfl_ops->prefix;
    unsigned int size;
    char *p;
    DWORD gle;

    SetLastError(0);
    GetEnvironmentVariable(KRB5_ENV_CCNAME, name_buf, name_size);
    gle = GetLastError();
    if (gle == 0)
        return 0;
    else if (gle != ERROR_ENVVAR_NOT_FOUND)
        return ENOMEM;

    if (get_from_registry(HKEY_CURRENT_USER,
                          name_buf, name_size) != 0)
        return 0;

    if (get_from_registry(HKEY_LOCAL_MACHINE,
                          name_buf, name_size) != 0)
        return 0;

    if (get_from_registry_indirect(name_buf, name_size) != 0)
        return 0;

    strncpy(name_buf, prefix, name_size - 1);
    name_buf[name_size - 1] = 0;
    size = name_size - strlen(prefix);
    if (size > 0)
        strcat(name_buf, ":");
    size--;
    p = name_buf + name_size - size;
    if (!strcmp(prefix, "API")) {
        strncpy(p, "krb5cc", size);
    } else if (!strcmp(prefix, "FILE") || !strcmp(prefix, "STDIO")) {
        if (!try_dir(getenv("TEMP"), p, size) &&
            !try_dir(getenv("TMP"), p, size))
        {
            unsigned int len = GetWindowsDirectory(p, size);
            name_buf[name_size - 1] = 0;
            if (len < size - sizeof(APPEND_KRB5CC))
                strcat(p, APPEND_KRB5CC);
        }
    } else {
        strncpy(p, "default_cache_name", size);
    }
    name_buf[name_size - 1] = 0;
    return 0;
}

static void
get_from_os(krb5_context context)
{
    krb5_error_code err;
    char buf[1024];

    if (get_from_os_buffer(buf, sizeof(buf)) == 0)
        context->os_context.default_ccname = strdup(buf);
}

#else /* not _WIN32 */

static void
get_from_os(krb5_context context)
{
    (void)k5_expand_path_tokens(context, DEFCCNAME,
                                &context->os_context.default_ccname);
}

#endif /* not _WIN32 */

#if defined(_WIN32)
static void
set_for_os(const char *name)
{
    set_to_registry(HKEY_CURRENT_USER, name);
}
#else
static void
set_for_os(const char *name)
{
    /* No implementation at present. */
}
#endif

/*
 * Set the default ccache name for all processes for the current user
 * (and the current context)
 */
krb5_error_code KRB5_CALLCONV
krb5int_cc_user_set_default_name(krb5_context context, const char *name)
{
    krb5_error_code err;

    err = krb5_cc_set_default_name(context, name);
    if (err)
        return err;
    set_for_os(name);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_cc_set_default_name(krb5_context context, const char *name)
{
    krb5_os_context os_ctx;
    char *new_ccname = NULL;

    if (!context || context->magic != KV5M_CONTEXT)
        return KV5M_CONTEXT;

    if (name != NULL) {
        new_ccname = strdup(name);
        if (new_ccname == NULL)
            return ENOMEM;
    }

    /* Free the old ccname and store the new one. */
    os_ctx = &context->os_context;
    free(os_ctx->default_ccname);
    os_ctx->default_ccname = new_ccname;
    return 0;
}


const char * KRB5_CALLCONV
krb5_cc_default_name(krb5_context context)
{
    krb5_os_context os_ctx;
    char *profstr, *envstr;

    if (!context || context->magic != KV5M_CONTEXT)
        return NULL;

    os_ctx = &context->os_context;
    if (os_ctx->default_ccname != NULL)
        return os_ctx->default_ccname;

    /* Try the environment variable first. */
    envstr = getenv(KRB5_ENV_CCNAME);
    if (envstr != NULL) {
        os_ctx->default_ccname = strdup(envstr);
        return os_ctx->default_ccname;
    }

    if (profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                           KRB5_CONF_DEFAULT_CCACHE_NAME, NULL, NULL,
                           &profstr) == 0 && profstr != NULL) {
        (void)k5_expand_path_tokens(context, profstr, &os_ctx->default_ccname);
        profile_release_string(profstr);
        return os_ctx->default_ccname;
    }

    /* Fall back on the default ccache name for the OS. */
    get_from_os(context);
    return os_ctx->default_ccname;
}
