/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/init_os_ctx.c */
/*
 * Copyright 1994, 2007, 2008, 2009 by the Massachusetts Institute of Technology.
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
#include "os-proto.h"
#include "../krb/int-proto.h"
#include "prof_int.h"        /* XXX for profile_copy, not public yet */

#if defined(_WIN32)
#include <winsock.h>
#include <Shlobj.h>

static krb5_error_code
get_from_windows_dir(
    char **pname
)
{
    UINT size = GetWindowsDirectory(0, 0);
    *pname = malloc(size + strlen(DEFAULT_PROFILE_FILENAME) + 2);
    if (*pname) {
        GetWindowsDirectory(*pname, size);
        strcat(*pname, "\\");
        strcat(*pname, DEFAULT_PROFILE_FILENAME);
        return 0;
    } else {
        return KRB5_CONFIG_CANTOPEN;
    }
}

static krb5_error_code
get_from_module_dir(
    char **pname
)
{
    const DWORD size = 1024; /* fixed buffer */
    int found = 0;
    char *p = NULL;
    char *name = NULL;
    struct _stat s;

    *pname = 0;

    name = malloc(size);
    if (!name)
        return ENOMEM;

#ifdef _WIN64
    if (!GetModuleFileName(GetModuleHandle("krb5_64"), name, size))
#else
    if (!GetModuleFileName(GetModuleHandle("krb5_32"), name, size))
#endif
        goto cleanup;

    p = name + strlen(name);
    while ((p >= name) && (*p != '\\') && (*p != '/')) p--;
    if (p < name)
        goto cleanup;
    p++;
    strncpy(p, DEFAULT_PROFILE_FILENAME, size - (p - name));
    name[size - 1] = 0;
    found = !_stat(name, &s);

cleanup:
    if (found)
        *pname = name;
    else
        free(name);
    return 0;
}

/*
 * get_from_registry
 *
 * This will find a profile in the registry.  *pbuffer != 0 if we
 * found something.  Make sure to free(*pbuffer) when done.  It will
 * return an error code if there is an error the user should know
 * about.  We maintain the invariant: return value != 0 =>
 * *pbuffer == 0.
 */
static krb5_error_code
get_from_registry(
    char** pbuffer,
    HKEY hBaseKey
)
{
    HKEY hKey = 0;
    LONG rc = 0;
    DWORD size = 0;
    krb5_error_code retval = 0;
    const char *key_path = "Software\\MIT\\Kerberos5";
    const char *value_name = "config";

    assert(pbuffer != NULL);
    *pbuffer = NULL;

    if ((rc = RegOpenKeyEx(hBaseKey, key_path, 0, KEY_QUERY_VALUE,
                           &hKey)) != ERROR_SUCCESS) {
        /* not a real error */
        goto cleanup;
    }
    rc = RegQueryValueEx(hKey, value_name, 0, 0, 0, &size);
    if ((rc != ERROR_SUCCESS) &&  (rc != ERROR_MORE_DATA)) {
        /* not a real error */
        goto cleanup;
    }
    *pbuffer = malloc(size);
    if (!*pbuffer) {
        retval = ENOMEM;
        goto cleanup;
    }
    if ((rc = RegQueryValueEx(hKey, value_name, 0, 0, *pbuffer, &size)) !=
        ERROR_SUCCESS) {
        /*
         * Let's not call it a real error in case it disappears, but
         * we need to free so that we say we did not find anything.
         */
        free(*pbuffer);
        *pbuffer = 0;
        goto cleanup;
    }
cleanup:
    if (hKey)
        RegCloseKey(hKey);
    if (retval && *pbuffer) {
        free(*pbuffer);
        /* Let's say we did not find anything: */
        *pbuffer = 0;
    }
    return retval;
}

/*
 * get_from_known_folder
 *
 * This will find a profile in the specified known folder (e.g. CSIDL_APPDATA).
 * *pbuffer != 0 if we found something.  Make sure to free(*pbuffer) when done.
 * It will return an error code if there is an error the user should know
 * about.  We maintain the invariant: return value != 0 =>
 * *pbuffer == 0.
 */
static krb5_error_code
get_from_known_folder(
    int folderId,
    char** pbuffer
)
{
    char szPath[MAX_PATH];
    const char * software_suffix = "\\MIT\\Kerberos5";
    krb5_error_code retval = 0;
    size_t size;
    struct _stat s;

    assert(pbuffer);
    *pbuffer = NULL;

    if (SUCCEEDED(SHGetFolderPath(NULL,
                                  folderId /*|CSIDL_FLAG_CREATE*/,
                                  NULL,
                                  SHGFP_TYPE_CURRENT,
                                  szPath))) {
        size = strlen(software_suffix) + strlen("\\" DEFAULT_PROFILE_FILENAME) + strlen(szPath);
        if ((size + 1) >= sizeof(szPath)) {
            goto cleanup;
        }
        strlcat(szPath, software_suffix, sizeof(szPath));
        strlcat(szPath, "\\", sizeof(szPath));
        strlcat(szPath, DEFAULT_PROFILE_FILENAME, sizeof(szPath));
    } else {
        /* Might want to deliberate a bit better why we failed.
            But for the time being this is not an error */
        goto cleanup;
    }

    if (_stat(szPath, &s)) {
        goto cleanup;
    }

    *pbuffer = malloc(size + 1);
    if (!*pbuffer) {
        retval = ENOMEM;
        goto cleanup;
    }

    strlcpy (*pbuffer, szPath, size + 1);

cleanup:
    if (retval && *pbuffer) {
        free(*pbuffer);
        /* Let's say we did not find anything: */
        *pbuffer = 0;
    }
    return retval;
}

#endif /* _WIN32 */

static void
free_filespecs(profile_filespec_t *files)
{
    char **cp;

    if (files == 0)
        return;

    for (cp = files; *cp; cp++)
        free(*cp);
    free(files);
}

/* This function is needed by KfM's KerberosPreferences API
 * because it needs to be able to specify "secure" */
static krb5_error_code
os_get_default_config_files(profile_filespec_t **pfiles, krb5_boolean secure)
{
    profile_filespec_t* files;
#if defined(_WIN32)
    krb5_error_code retval = 0;
    char *name = 0;

    if (!secure) {
        char *env = getenv("KRB5_CONFIG");
        if (env) {
            name = strdup(env);
            if (!name) return ENOMEM;
        }
    }
    if (!name && !secure) {
        /* HKCU */
        retval = get_from_registry(&name, HKEY_CURRENT_USER);
        if (retval) return retval;
    }
    if (!name) {
        /* HKLM */
        retval = get_from_registry(&name, HKEY_LOCAL_MACHINE);
        if (retval) return retval;
    }

    if (!name && !secure) {
        retval = get_from_known_folder(CSIDL_APPDATA, &name);
        if (retval) return retval;
    }

    if (!name) {
        retval = get_from_known_folder(CSIDL_COMMON_APPDATA, &name);
        if (retval) return retval;
    }

    if (!name && !secure) {
        /* module dir */
        retval = get_from_module_dir(&name);
        if (retval) return retval;
    }
    if (!name) {
        /* windows dir */
        retval = get_from_windows_dir(&name);
    }
    if (retval)
        return retval;
    if (!name)
        return KRB5_CONFIG_CANTOPEN; /* should never happen */

    files = malloc(2 * sizeof(char *));
    if (!files)
        return ENOMEM;
    files[0] = name;
    files[1] = 0;
#else /* !_WIN32 */
    char* filepath = 0;
    int n_entries, i;
    unsigned int ent_len;
    const char *s, *t;

    if (secure) {
        filepath = DEFAULT_SECURE_PROFILE_PATH;
    } else {
        filepath = getenv("KRB5_CONFIG");
        if (!filepath) filepath = DEFAULT_PROFILE_PATH;
    }

    /* count the distinct filename components */
    for(s = filepath, n_entries = 1; *s; s++) {
        if (*s == ':')
            n_entries++;
    }

    /* the array is NULL terminated */
    files = (char**) malloc((n_entries+1) * sizeof(char*));
    if (files == 0)
        return ENOMEM;

    /* measure, copy, and skip each one */
    for(s = filepath, i=0; (t = strchr(s, ':')) || (t=s+strlen(s)); s=t+1, i++) {
        ent_len = t-s;
        files[i] = (char*) malloc(ent_len + 1);
        if (files[i] == 0) {
            /* if malloc fails, free the ones that worked */
            while(--i >= 0) free(files[i]);
            free(files);
            return ENOMEM;
        }
        strncpy(files[i], s, ent_len);
        files[i][ent_len] = 0;
        if (*t == 0) {
            i++;
            break;
        }
    }
    /* cap the array */
    files[i] = 0;
#endif /* !_WIN32 */
    *pfiles = (profile_filespec_t *)files;
    return 0;
}

static krb5_error_code
add_kdc_config_file(profile_filespec_t **pfiles)
{
    char *file = NULL;
    size_t count = 0;
    profile_filespec_t *newfiles;

    file = getenv(KDC_PROFILE_ENV);
    if (file == NULL)
        file = DEFAULT_KDC_PROFILE;

    for (count = 0; (*pfiles)[count]; count++)
        ;
    count += 2;
    newfiles = malloc(count * sizeof(*newfiles));
    if (newfiles == NULL)
        return ENOMEM;
    memcpy(newfiles + 1, *pfiles, (count-1) * sizeof(*newfiles));
    newfiles[0] = strdup(file);
    if (newfiles[0] == NULL) {
        int e = ENOMEM;
        free(newfiles);
        return e;
    }
    free(*pfiles);
    *pfiles = newfiles;
    return 0;
}


/* Set the profile paths in the context.  If secure is set to TRUE
   then do not include user paths (from environment variables, etc).
   If kdc is TRUE, include kdc.conf from whereever we expect to find
   it.  */
static krb5_error_code
os_init_paths(krb5_context ctx, krb5_boolean kdc)
{
    krb5_error_code    retval = 0;
    profile_filespec_t *files = 0;
    krb5_boolean secure = ctx->profile_secure;

    retval = os_get_default_config_files(&files, secure);

    if (retval == 0 && kdc)
        retval = add_kdc_config_file(&files);

    if (!retval) {
        retval = profile_init_flags((const_profile_filespec_t *) files,
                                    PROFILE_INIT_ALLOW_MODULE, &ctx->profile);

        /* If none of the filenames can be opened, use an empty profile. */
        if (retval == ENOENT)
            retval = profile_init(NULL, &ctx->profile);
    }

    if (files)
        free_filespecs(files);

    if (retval)
        ctx->profile = 0;

    if (retval == ENOENT)
        return KRB5_CONFIG_CANTOPEN;

    if ((retval == PROF_SECTION_NOTOP) ||
        (retval == PROF_SECTION_SYNTAX) ||
        (retval == PROF_RELATION_SYNTAX) ||
        (retval == PROF_EXTRA_CBRACE) ||
        (retval == PROF_MISSING_OBRACE))
        return KRB5_CONFIG_BADFORMAT;

    return retval;
}

krb5_error_code
k5_os_init_context(krb5_context ctx, profile_t profile, krb5_flags flags)
{
    krb5_os_context os_ctx;
    krb5_error_code    retval = 0;
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
#endif /* _WIN32 */

    os_ctx = &ctx->os_context;
    os_ctx->magic = KV5M_OS_CONTEXT;
    os_ctx->time_offset = 0;
    os_ctx->usec_offset = 0;
    os_ctx->os_flags = 0;
    os_ctx->default_ccname = 0;

    PLUGIN_DIR_INIT(&ctx->libkrb5_plugins);
    ctx->preauth_context = NULL;

    /* Use the profile we were handed, or create one from config files. */
    if (profile)
        retval = profile_copy(profile, &ctx->profile);
    else
        retval = os_init_paths(ctx, (flags & KRB5_INIT_CONTEXT_KDC) != 0);
    if (retval)
        return retval;

#ifdef _WIN32
    /* We initialize winsock to version 1.1 but
     * we do not care if we succeed or fail.
     */
    wVersionRequested = 0x0101;
    WSAStartup (wVersionRequested, &wsaData);
#endif /* _WIN32 */

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_profile (krb5_context ctx, profile_t *profile)
{
    return profile_copy (ctx->profile, profile);
}

krb5_error_code
krb5_set_config_files(krb5_context ctx, const char **filenames)
{
    krb5_error_code retval = 0;
    profile_t    profile;

    retval = profile_init_flags(filenames, PROFILE_INIT_ALLOW_MODULE,
                                &profile);
    if (retval)
        return retval;

    if (ctx->profile)
        profile_release(ctx->profile);
    ctx->profile = profile;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_default_config_files(char ***pfilenames)
{
    if (!pfilenames)
        return EINVAL;
    return os_get_default_config_files(pfilenames, FALSE);
}

void KRB5_CALLCONV
krb5_free_config_files(char **filenames)
{
    free_filespecs(filenames);
}

void
k5_os_free_context(krb5_context ctx)
{
    krb5_os_context os_ctx;

    os_ctx = &ctx->os_context;

    if (os_ctx->default_ccname) {
        free(os_ctx->default_ccname);
        os_ctx->default_ccname = 0;
    }

    os_ctx->magic = 0;

    if (ctx->profile) {
        profile_release(ctx->profile);
        ctx->profile = 0;
    }

    if (ctx->preauth_context) {
        k5_free_preauth_context(ctx);
        ctx->preauth_context = NULL;
    }
    krb5int_close_plugin_dirs (&ctx->libkrb5_plugins);

#ifdef _WIN32
    WSACleanup();
#endif /* _WIN32 */
}
