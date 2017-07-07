/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/str_conv.c */
/*
 * Copyright (C) 1995-2015 by the Massachusetts Institute of Technology.
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

/* Convert between strings and Kerberos internal data. */

#include "k5-int.h"
#include "admin_internal.h"
#include "adm_proto.h"

#include <ctype.h>

static const char default_tupleseps[]   = ", \t";
static const char default_ksaltseps[]   = ":";

struct flag_table_row {
    const char *spec;           /* Input specifier string */
    krb5_flags flag;            /* Flag */
    int invert;                 /* Whether to invert the sense */
};

static const struct flag_table_row ftbl[] = {
    {"allow_postdated",         KRB5_KDB_DISALLOW_POSTDATED,    1},
    {"postdateable",            KRB5_KDB_DISALLOW_POSTDATED,    1},
    {"disallow_postdated",      KRB5_KDB_DISALLOW_POSTDATED,    0},
    {"allow_forwardable",       KRB5_KDB_DISALLOW_FORWARDABLE,  1},
    {"forwardable",             KRB5_KDB_DISALLOW_FORWARDABLE,  1},
    {"disallow_forwardable",    KRB5_KDB_DISALLOW_FORWARDABLE,  0},
    {"allow_tgs_req",           KRB5_KDB_DISALLOW_TGT_BASED,    1},
    {"tgt_based",               KRB5_KDB_DISALLOW_TGT_BASED,    1},
    {"disallow_tgt_based",      KRB5_KDB_DISALLOW_TGT_BASED,    0},
    {"allow_renewable",         KRB5_KDB_DISALLOW_RENEWABLE,    1},
    {"renewable",               KRB5_KDB_DISALLOW_RENEWABLE,    1},
    {"disallow_renewable",      KRB5_KDB_DISALLOW_RENEWABLE,    0},
    {"allow_proxiable",         KRB5_KDB_DISALLOW_PROXIABLE,    1},
    {"proxiable",               KRB5_KDB_DISALLOW_PROXIABLE,    1},
    {"disallow_proxiable",      KRB5_KDB_DISALLOW_PROXIABLE,    0},
    {"allow_dup_skey",          KRB5_KDB_DISALLOW_DUP_SKEY,     1},
    {"dup_skey",                KRB5_KDB_DISALLOW_DUP_SKEY,     1},
    {"disallow_dup_skey",       KRB5_KDB_DISALLOW_DUP_SKEY,     0},
    {"allow_tickets",           KRB5_KDB_DISALLOW_ALL_TIX,      1},
    {"allow_tix",               KRB5_KDB_DISALLOW_ALL_TIX,      1},
    {"disallow_all_tix",        KRB5_KDB_DISALLOW_ALL_TIX,      0},
    {"preauth",                 KRB5_KDB_REQUIRES_PRE_AUTH,     0},
    {"requires_pre_auth",       KRB5_KDB_REQUIRES_PRE_AUTH,     0},
    {"requires_preauth",        KRB5_KDB_REQUIRES_PRE_AUTH,     0},
    {"hwauth",                  KRB5_KDB_REQUIRES_HW_AUTH,      0},
    {"requires_hw_auth",        KRB5_KDB_REQUIRES_HW_AUTH,      0},
    {"requires_hwauth",         KRB5_KDB_REQUIRES_HW_AUTH,      0},
    {"needchange",              KRB5_KDB_REQUIRES_PWCHANGE,     0},
    {"pwchange",                KRB5_KDB_REQUIRES_PWCHANGE,     0},
    {"requires_pwchange",       KRB5_KDB_REQUIRES_PWCHANGE,     0},
    {"allow_svr",               KRB5_KDB_DISALLOW_SVR,          1},
    {"service",                 KRB5_KDB_DISALLOW_SVR,          1},
    {"disallow_svr",            KRB5_KDB_DISALLOW_SVR,          0},
    {"password_changing_service", KRB5_KDB_PWCHANGE_SERVICE,    0},
    {"pwchange_service",        KRB5_KDB_PWCHANGE_SERVICE,      0},
    {"pwservice",               KRB5_KDB_PWCHANGE_SERVICE,      0},
    {"md5",                     KRB5_KDB_SUPPORT_DESMD5,        0},
    {"support_desmd5",          KRB5_KDB_SUPPORT_DESMD5,        0},
    {"new_princ",               KRB5_KDB_NEW_PRINC,             0},
    {"ok_as_delegate",          KRB5_KDB_OK_AS_DELEGATE,        0},
    {"ok_to_auth_as_delegate",  KRB5_KDB_OK_TO_AUTH_AS_DELEGATE, 0},
    {"no_auth_data_required",   KRB5_KDB_NO_AUTH_DATA_REQUIRED, 0},
    {"lockdown_keys",           KRB5_KDB_LOCKDOWN_KEYS,         0},
};
#define NFTBL (sizeof(ftbl) / sizeof(ftbl[0]))

static const char *outflags[] = {
    "DISALLOW_POSTDATED",       /* 0x00000001 */
    "DISALLOW_FORWARDABLE",     /* 0x00000002 */
    "DISALLOW_TGT_BASED",       /* 0x00000004 */
    "DISALLOW_RENEWABLE",       /* 0x00000008 */
    "DISALLOW_PROXIABLE",       /* 0x00000010 */
    "DISALLOW_DUP_SKEY",        /* 0x00000020 */
    "DISALLOW_ALL_TIX",         /* 0x00000040 */
    "REQUIRES_PRE_AUTH",        /* 0x00000080 */
    "REQUIRES_HW_AUTH",         /* 0x00000100 */
    "REQUIRES_PWCHANGE",        /* 0x00000200 */
    NULL,                       /* 0x00000400 */
    NULL,                       /* 0x00000800 */
    "DISALLOW_SVR",             /* 0x00001000 */
    "PWCHANGE_SERVICE",         /* 0x00002000 */
    "SUPPORT_DESMD5",           /* 0x00004000 */
    "NEW_PRINC",                /* 0x00008000 */
    NULL,                       /* 0x00010000 */
    NULL,                       /* 0x00020000 */
    NULL,                       /* 0x00040000 */
    NULL,                       /* 0x00080000 */
    "OK_AS_DELEGATE",           /* 0x00100000 */
    "OK_TO_AUTH_AS_DELEGATE",   /* 0x00200000 */
    "NO_AUTH_DATA_REQUIRED",    /* 0x00400000 */
    "LOCKDOWN_KEYS",            /* 0x00800000 */
};
#define NOUTFLAGS (sizeof(outflags) / sizeof(outflags[0]))

/*
 * Given s, which is a normalized flagspec with the prefix stripped off, and
 * req_neg indicating whether the flagspec is negated, update the toset and
 * toclear masks.
 */
static krb5_error_code
raw_flagspec_to_mask(const char *s, int req_neg, krb5_flags *toset,
                     krb5_flags *toclear)
{
    int found = 0, invert = 0;
    size_t i;
    krb5_flags flag;
    unsigned long ul;

    for (i = 0; !found && i < NFTBL; i++) {
        if (strcmp(s, ftbl[i].spec) != 0)
            continue;
        /* Found a match */
        found = 1;
        invert = ftbl[i].invert;
        flag = ftbl[i].flag;
    }
    /* Accept hexadecimal numbers. */
    if (!found && strncmp(s, "0x", 2) == 0) {
        /* Assume that krb5_flags are 32 bits long. */
        ul = strtoul(s, NULL, 16) & 0xffffffff;
        flag = (krb5_flags)ul;
        found = 1;
    }
    if (!found)
        return EINVAL;
    if (req_neg)
        invert = !invert;
    if (invert)
        *toclear &= ~flag;
    else
        *toset |= flag;
    return 0;
}

/*
 * Update the toset and toclear flag masks according to flag specifier string
 * spec, which is of the form {+|-}flagname.  toset and toclear can point to
 * the same flag word.
 */
krb5_error_code
krb5_flagspec_to_mask(const char *spec, krb5_flags *toset, krb5_flags *toclear)
{
    int req_neg = 0;
    char *copy, *cp, *s;
    krb5_error_code retval;

    s = copy = strdup(spec);
    if (s == NULL)
        return ENOMEM;

    if (*s == '-') {
        req_neg = 1;
        s++;
    } else if (*s == '+')
        s++;

    for (cp = s; *cp != '\0'; cp++) {
        /* Transform hyphens to underscores.*/
        if (*cp == '-')
            *cp = '_';
        /* Downcase. */
        if (isupper((unsigned char)*cp))
            *cp = tolower((unsigned char)*cp);
    }
    retval = raw_flagspec_to_mask(s, req_neg, toset, toclear);
    free(copy);
    return retval;
}

/*
 * Copy the flag name of flagnum to outstr.  On error, outstr points to a null
 * pointer.
 */
krb5_error_code
krb5_flagnum_to_string(int flagnum, char **outstr)
{
    const char *s = NULL;

    *outstr = NULL;
    if ((unsigned int)flagnum < NOUTFLAGS)
        s = outflags[flagnum];
    if (s == NULL) {
        /* Assume that krb5_flags are 32 bits long. */
        if (asprintf(outstr, "0x%08lx", 1UL << flagnum) == -1)
            *outstr = NULL;
    } else {
        *outstr = strdup(s);
    }
    if (*outstr == NULL)
        return ENOMEM;
    return 0;
}

/*
 * Create a null-terminated array of string representations of flags.  Store a
 * null pointer into outarray if there would be no strings.
 */
krb5_error_code
krb5_flags_to_strings(krb5_int32 flags, char ***outarray)
{
    char **a = NULL, **a_new = NULL, **ap;
    size_t amax = 0, i;
    krb5_error_code retval;

    *outarray = NULL;

    /* Assume that krb5_flags are 32 bits long. */
    for (i = 0; i < 32; i++) {
        if (!(flags & (1UL << i)))
            continue;

        a_new = realloc(a, (amax + 2) * sizeof(*a));
        if (a_new == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        a = a_new;
        retval = krb5_flagnum_to_string(i, &a[amax++]);
        a[amax] = NULL;
        if (retval)
            goto cleanup;
    }
    *outarray = a;
    return 0;
cleanup:
    for (ap = a; ap != NULL && *ap != NULL; ap++) {
        free(*ap);
    }
    free(a);
    return retval;
}

/*
 * krb5_keysalt_is_present()    - Determine if a key/salt pair is present
 *                                in a list of key/salt tuples.
 *
 *      Salttype may be negative to indicate a search for only a enctype.
 */
krb5_boolean
krb5_keysalt_is_present(ksaltlist, nksalts, enctype, salttype)
    krb5_key_salt_tuple *ksaltlist;
    krb5_int32          nksalts;
    krb5_enctype        enctype;
    krb5_int32          salttype;
{
    krb5_boolean        foundit;
    int                 i;

    foundit = 0;
    if (ksaltlist) {
        for (i=0; i<nksalts; i++) {
            if ((ksaltlist[i].ks_enctype == enctype) &&
                ((ksaltlist[i].ks_salttype == salttype) ||
                 (salttype < 0))) {
                foundit = 1;
                break;
            }
        }
    }
    return(foundit);
}

/* NOTE: This is a destructive parser (writes NULs). */
static krb5_error_code
string_to_keysalt(char *s, const char *ksaltseps,
                  krb5_enctype *etype, krb5_int32 *stype)
{
    char *sp;
    const char *ksseps = (ksaltseps != NULL) ? ksaltseps : default_ksaltseps;
    krb5_error_code ret = 0;

    sp = strpbrk(s, ksseps);
    if (sp != NULL) {
        *sp++ = '\0';
    }
    ret = krb5_string_to_enctype(s, etype);
    if (ret)
        return ret;

    /* Default to normal salt if omitted. */
    *stype = KRB5_KDB_SALTTYPE_NORMAL;
    if (sp == NULL)
        return 0;
    return krb5_string_to_salttype(sp, stype);
}

/*
 * krb5_string_to_keysalts()    - Convert a string representation to a list
 *                                of key/salt tuples.
 */
krb5_error_code
krb5_string_to_keysalts(const char *string, const char *tupleseps,
                        const char *ksaltseps, krb5_boolean dups,
                        krb5_key_salt_tuple **ksaltp, krb5_int32 *nksaltp)
{
    char *copy, *p, *ksp;
    char *tlasts = NULL;
    const char *tseps = (tupleseps != NULL) ? tupleseps : default_tupleseps;
    krb5_int32 nksalts = 0;
    krb5_int32 stype;
    krb5_enctype etype;
    krb5_error_code ret = 0;
    krb5_key_salt_tuple *ksalts = NULL, *ksalts_new = NULL;

    *ksaltp = NULL;
    *nksaltp = 0;
    p = copy = strdup(string);
    if (p == NULL)
        return ENOMEM;
    while ((ksp = strtok_r(p, tseps, &tlasts)) != NULL) {
        /* Pass a null pointer to subsequent calls to strtok_r(). */
        p = NULL;
        ret = string_to_keysalt(ksp, ksaltseps, &etype, &stype);
        if (ret)
            goto cleanup;

        /* Ignore duplicate keysalts if caller asks. */
        if (!dups && krb5_keysalt_is_present(ksalts, nksalts, etype, stype))
            continue;

        ksalts_new = realloc(ksalts, (nksalts + 1) * sizeof(*ksalts));
        if (ksalts_new == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        ksalts = ksalts_new;
        ksalts[nksalts].ks_enctype = etype;
        ksalts[nksalts].ks_salttype = stype;
        nksalts++;
    }
    *ksaltp = ksalts;
    *nksaltp = nksalts;
cleanup:
    if (ret)
        free(ksalts);
    free(copy);
    return ret;
}

/*
 * krb5_keysalt_iterate()       - Do something for each unique key/salt
 *                                combination.
 *
 * If ignoresalt set, then salttype is ignored.
 */
krb5_error_code
krb5_keysalt_iterate(ksaltlist, nksalt, ignoresalt, iterator, arg)
    krb5_key_salt_tuple *ksaltlist;
    krb5_int32          nksalt;
    krb5_boolean        ignoresalt;
    krb5_error_code     (*iterator) (krb5_key_salt_tuple *, krb5_pointer);
    krb5_pointer        arg;
{
    int                 i;
    krb5_error_code     kret;
    krb5_key_salt_tuple scratch;

    kret = 0;
    for (i=0; i<nksalt; i++) {
        scratch.ks_enctype = ksaltlist[i].ks_enctype;
        scratch.ks_salttype = (ignoresalt) ? -1 : ksaltlist[i].ks_salttype;
        if (!krb5_keysalt_is_present(ksaltlist,
                                     i,
                                     scratch.ks_enctype,
                                     scratch.ks_salttype)) {
            kret = (*iterator)(&scratch, arg);
            if (kret)
                break;
        }
    }
    return(kret);
}
