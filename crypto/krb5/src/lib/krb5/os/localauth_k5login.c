/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/localauth_k5login.c - k5login localauth module */
/*
 * Copyright (C) 1990,1993,2007,2013 by the Massachusetts Institute
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

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/localauth_plugin.h>

#if !defined(_WIN32)            /* Not yet for Windows */
#include <pwd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <hfs/hfs_mount.h>
#define FILE_OWNER_OK(UID)  ((UID) == 0 || (UID) == UNKNOWNUID)
#else
#define FILE_OWNER_OK(UID)  ((UID) == 0)
#endif

/*
 * Find the k5login filename for luser, either in the user's homedir or in a
 * configured directory under the username.
 */
static krb5_error_code
get_k5login_filename(krb5_context context, const char *lname,
                     const char *homedir, char **filename_out)
{
    krb5_error_code ret;
    char *dir, *filename;

    *filename_out = NULL;
    ret = profile_get_string(context->profile, KRB5_CONF_LIBDEFAULTS,
                             KRB5_CONF_K5LOGIN_DIRECTORY, NULL, NULL, &dir);
    if (ret != 0)
        return ret;

    if (dir == NULL) {
        /* Look in the user's homedir. */
        if (asprintf(&filename, "%s/.k5login", homedir) < 0)
            return ENOMEM;
    } else {
        /* Look in the configured directory. */
        if (asprintf(&filename, "%s/%s", dir, lname) < 0)
            ret = ENOMEM;
        profile_release_string(dir);
        if (ret)
            return ret;
    }
    *filename_out = filename;
    return 0;
}

/* Determine whether aname is authorized to log in as lname according to the
 * user's k5login file. */
static krb5_error_code
userok_k5login(krb5_context context, krb5_localauth_moddata data,
               krb5_const_principal aname, const char *lname)
{
    krb5_error_code ret;
    int authoritative = TRUE, gobble;
    char *filename = NULL, *princname = NULL;
    char *newline, linebuf[BUFSIZ], pwbuf[BUFSIZ];
    struct stat sbuf;
    struct passwd pwx, *pwd;
    FILE *fp = NULL;

    ret = profile_get_boolean(context->profile, KRB5_CONF_LIBDEFAULTS,
                              KRB5_CONF_K5LOGIN_AUTHORITATIVE, NULL, TRUE,
                              &authoritative);
    if (ret)
        goto cleanup;

    /* Get the local user's .k5login filename. */
    ret = k5_getpwnam_r(lname, &pwx, pwbuf, sizeof(pwbuf), &pwd);
    if (ret) {
        ret = EPERM;
        goto cleanup;
    }
    ret = get_k5login_filename(context, lname, pwd->pw_dir, &filename);
    if (ret)
        goto cleanup;

    if (access(filename, F_OK) != 0) {
        ret = KRB5_PLUGIN_NO_HANDLE;
        goto cleanup;
    }

    ret = krb5_unparse_name(context, aname, &princname);
    if (ret)
        goto cleanup;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        ret = errno;
        goto cleanup;
    }
    set_cloexec_file(fp);

    /* For security reasons, the .k5login file must be owned either by
     * the user or by root. */
    if (fstat(fileno(fp), &sbuf)) {
        ret = errno;
        goto cleanup;
    }
    if (sbuf.st_uid != pwd->pw_uid && !FILE_OWNER_OK(sbuf.st_uid)) {
        ret = EPERM;
        goto cleanup;
    }

    /* Check each line. */
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
        newline = strrchr(linebuf, '\n');
        if (newline != NULL)
            *newline = '\0';
        if (strcmp(linebuf, princname) == 0) {
            ret = 0;
            goto cleanup;
        }
        /* Clean up the rest of the line if necessary. */
        if (newline == NULL)
            while ((gobble = getc(fp)) != EOF && gobble != '\n');
    }

    /* We didn't find it. */
    ret = EPERM;

cleanup:
    free(princname);
    free(filename);
    if (fp != NULL)
        fclose(fp);
    /* If k5login files are non-authoritative, never reject. */
    return (!authoritative && ret) ? KRB5_PLUGIN_NO_HANDLE : ret;
}

#else /* _WIN32 */

static krb5_error_code
userok_k5login(krb5_context context, krb5_localauth_moddata data,
               krb5_const_principal aname, const char *lname)
{
    return KRB5_PLUGIN_NO_HANDLE;
}

#endif

krb5_error_code
localauth_k5login_initvt(krb5_context context, int maj_ver, int min_ver,
                         krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;

    vt->name = "k5login";
    vt->userok = userok_k5login;
    return 0;
}
