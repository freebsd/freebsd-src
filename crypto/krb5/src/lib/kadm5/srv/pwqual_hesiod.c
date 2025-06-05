/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/srv/pwqual_hesiod.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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

/*
 * Password quality module to check passwords against GECOS fields of Hesiod
 * passwd information, if the tree is compiled with Hesiod support.
 */

#include "k5-int.h"
#include <krb5/pwqual_plugin.h>
#include "server_internal.h"
#include <ctype.h>

#ifdef HESIOD
#include <pwd.h>

static char *
reverse(char *str, char *newstr, size_t newstr_size)
{
    char *p, *q;
    size_t i;

    i = strlen(str);
    if (i >= newstr_size)
        i = newstr_size - 1;
    p = str + i - 1;
    q = newstr;
    q[i] = '\0';
    for (; i > 0; i--)
        *q++ = *p--;

    return newstr;
}

static int
str_check_gecos(char *gecos, const char *pwstr)
{
    char *cp, *ncp, *tcp, revbuf[80];

    for (cp = gecos; *cp; ) {
        /* Skip past punctuation */
        for (; *cp; cp++)
            if (isalnum(*cp))
                break;

        /* Skip to the end of the word */
        for (ncp = cp; *ncp; ncp++) {
            if (!isalnum(*ncp) && *ncp != '\'')
                break;
        }

        /* Delimit end of word */
        if (*ncp)
            *ncp++ = '\0';

        /* Check word to see if it's the password */
        if (*cp) {
            if (!strcasecmp(pwstr, cp))
                return 1;
            tcp = reverse(cp, revbuf, sizeof(revbuf));
            if (!strcasecmp(pwstr, tcp))
                return 1;
            cp = ncp;
        } else
            break;
    }
    return 0;
}
#endif /* HESIOD */

static krb5_error_code
hesiod_check(krb5_context context, krb5_pwqual_moddata data,
             const char *password, const char *policy_name,
             krb5_principal princ, const char **languages)
{
#ifdef HESIOD
    extern struct passwd *hes_getpwnam();
    struct passwd *ent;
    int i, n;
    const char *cp;

    /* Don't check for principals with no password policy. */
    if (policy_name == NULL)
        return 0;

    n = krb5_princ_size(handle->context, princ);
    for (i = 0; i < n; i++) {
        ent = hes_getpwnam(cp);
        if (ent && ent->pw_gecos && str_check_gecos(ent->pw_gecos, password)) {
            k5_setmsg(context, KADM5_PASS_Q_DICT,
                      _("Password may not match user information."));
            return KADM5_PASS_Q_DICT;
        }
    }
#endif /* HESIOD */
    return 0;
}

krb5_error_code
pwqual_hesiod_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable)
{
    krb5_pwqual_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (krb5_pwqual_vtable)vtable;
    vt->name = "hesiod";
    vt->check = hesiod_check;
    return 0;
}
