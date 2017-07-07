/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccselect_k5identity.c - k5identity ccselect module */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "k5-int.h"
#include "cc-int.h"
#include <krb5/ccselect_plugin.h>
#include <ctype.h>

#ifndef _WIN32

#include <pwd.h>

static krb5_error_code
k5identity_init(krb5_context context, krb5_ccselect_moddata *data_out,
                int *priority_out)
{
    *data_out = NULL;
    *priority_out = KRB5_CCSELECT_PRIORITY_AUTHORITATIVE;
    return 0;
}

/* Match data (folded to lowercase if fold_case is set) against pattern. */
static krb5_boolean
fnmatch_data(const char *pattern, krb5_data *data, krb5_boolean fold_case)
{
    krb5_error_code ret;
    char *str, *p;
    int res;

    str = k5memdup0(data->data, data->length, &ret);
    if (str == NULL)
        return FALSE;

    if (fold_case) {
        for (p = str; *p != '\0'; p++) {
            if (isupper((unsigned char)*p))
                *p = tolower((unsigned char)*p);
        }
    }

    res = fnmatch(pattern, str, 0);
    free(str);
    return (res == 0);
}

/* Return true if server satisfies the constraint given by name and value. */
static krb5_boolean
check_constraint(krb5_context context, const char *name, const char *value,
                 krb5_principal server)
{
    if (strcmp(name, "realm") == 0) {
        return fnmatch_data(value, &server->realm, FALSE);
    } else if (strcmp(name, "service") == 0) {
        return (server->type == KRB5_NT_SRV_HST && server->length >= 2 &&
                fnmatch_data(value, &server->data[0], FALSE));
    } else if (strcmp(name, "host") == 0) {
        return (server->type == KRB5_NT_SRV_HST && server->length >= 2 &&
                fnmatch_data(value, &server->data[1], TRUE));
    }
    /* Assume unrecognized constraints are critical. */
    return FALSE;
}

/*
 * If line begins with a valid principal and server matches the constraints
 * listed afterwards, set *princ_out to the client principal described in line
 * and return true.  Otherwise return false.  May destructively affect line.
 */
static krb5_boolean
parse_line(krb5_context context, char *line, krb5_principal server,
           krb5_principal *princ_out)
{
    const char *whitespace = " \t\r\n";
    char *princ, *princ_end, *field, *field_end, *sep;

    *princ_out = NULL;

    /* Find the bounds of the principal. */
    princ = line + strspn(line, whitespace);
    if (*princ == '#')
        return FALSE;
    princ_end = princ + strcspn(princ, whitespace);
    if (princ_end == princ)
        return FALSE;

    /* Check all constraints. */
    field = princ_end + strspn(princ_end, whitespace);
    while (*field != '\0') {
        field_end = field + strcspn(field, whitespace);
        if (*field_end != '\0')
            *field_end++ = '\0';
        sep = strchr(field, '=');
        if (sep == NULL)        /* Malformed line. */
            return FALSE;
        *sep = '\0';
        if (!check_constraint(context, field, sep + 1, server))
            return FALSE;
        field = field_end + strspn(field_end, whitespace);
    }

    *princ_end = '\0';
    return (krb5_parse_name(context, princ, princ_out) == 0);
}

/* Determine the current user's homedir.  Allow HOME to override the result for
 * non-secure profiles; otherwise, use the euid's homedir from passwd. */
static char *
get_homedir(krb5_context context)
{
    const char *homedir = NULL;
    char pwbuf[BUFSIZ];
    struct passwd pwx, *pwd;

    if (!context->profile_secure)
        homedir = getenv("HOME");

    if (homedir == NULL) {
        if (k5_getpwuid_r(geteuid(), &pwx, pwbuf, sizeof(pwbuf), &pwd) != 0)
            return NULL;
        homedir = pwd->pw_dir;
    }

    return strdup(homedir);
}

static krb5_error_code
k5identity_choose(krb5_context context, krb5_ccselect_moddata data,
                  krb5_principal server, krb5_ccache *cache_out,
                  krb5_principal *princ_out)
{
    krb5_error_code ret;
    krb5_principal princ = NULL;
    char *filename, *homedir;
    FILE *fp;
    char buf[256];

    *cache_out = NULL;
    *princ_out = NULL;

    /* Open the .k5identity file. */
    homedir = get_homedir(context);
    if (homedir == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    ret = k5_path_join(homedir, ".k5identity", &filename);
    free(homedir);
    if (ret)
        return ret;
    fp = fopen(filename, "r");
    free(filename);
    if (fp == NULL)
        return KRB5_PLUGIN_NO_HANDLE;

    /* Look for a line with constraints matched by server. */
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (parse_line(context, buf, server, &princ))
            break;
    }
    fclose(fp);
    if (princ == NULL)
        return KRB5_PLUGIN_NO_HANDLE;

    /* Look for a ccache with the appropriate client principal.  If we don't
     * find out, set *princ_out to indicate the desired client principal. */
    ret = krb5_cc_cache_match(context, princ, cache_out);
    if (ret == 0 || ret == KRB5_CC_NOTFOUND)
        *princ_out = princ;
    else
        krb5_free_principal(context, princ);
    return ret;
}

krb5_error_code
ccselect_k5identity_initvt(krb5_context context, int maj_ver, int min_ver,
                           krb5_plugin_vtable vtable)
{
    krb5_ccselect_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_ccselect_vtable)vtable;
    vt->name = "k5identity";
    vt->init = k5identity_init;
    vt->choose = k5identity_choose;
    return 0;
}

#endif /* not _WIN32 */
