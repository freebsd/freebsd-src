/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/conv_princ.c */
/*
 * Copyright 1992 by the Massachusetts Institute of Technology.
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
 * Build a principal from a V4 specification, or separate a V5
 * principal into name, instance, and realm.
 *
 * NOTE: This is highly site specific, and is only really necessary
 * for sites who need to convert from V4 to V5.  It is used by both
 * the KDC and the kdb5_convert program.  Since its use is highly
 * specialized, the necessary information is just going to be
 * hard-coded in this file.
 */

#include "k5-int.h"
#include <string.h>
#include <ctype.h>

/* The maximum sizes for V4 aname, realm, sname, and instance +1 */
/* Taken from krb.h */
#define         ANAME_SZ        40
#define         REALM_SZ        40
#define         SNAME_SZ        40
#define         INST_SZ         40

struct krb_convert {
    char                *v4_str;
    char                *v5_str;
    unsigned int        flags : 8;
    unsigned int        len : 8;
};

#define DO_REALM_CONVERSION 0x00000001

/*
 * Kadmin doesn't do realm conversion because it's currently
 * kadmin/REALM.NAME.  Zephyr doesn't because it's just zephyr/zephyr.
 *
 * "Realm conversion" is a bit of a misnomer; really, the v5 name is
 * using a FQDN or something that looks like it, where the v4 name is
 * just using the first label.  Sometimes that second principal name
 * component is a hostname, sometimes the realm name, sometimes it's
 * neither.
 *
 * This list should probably be more configurable, and more than
 * likely on a per-realm basis, so locally-defined services can be
 * added, or not.
 */
static const struct krb_convert sconv_list[] = {
    /* Realm conversion, Change service name */
#define RC(V5NAME,V4NAME) { V5NAME, V4NAME, DO_REALM_CONVERSION, sizeof(V5NAME)-1 }
    /* Realm conversion */
#define R(NAME)         { NAME, NAME, DO_REALM_CONVERSION, sizeof(NAME)-1 }
    /* No Realm conversion */
#define NR(NAME)        { NAME, NAME, 0, sizeof(NAME)-1 }

    NR("kadmin"),
    RC("rcmd", "host"),
    R("discuss"),
    R("rvdsrv"),
    R("sample"),
    R("olc"),
    R("pop"),
    R("sis"),
    R("rfs"),
    R("imap"),
    R("ftp"),
    R("ecat"),
    R("daemon"),
    R("gnats"),
    R("moira"),
    R("prms"),
    R("mandarin"),
    R("register"),
    R("changepw"),
    R("sms"),
    R("afpserver"),
    R("gdss"),
    R("news"),
    R("abs"),
    R("nfs"),
    R("tftp"),
    NR("zephyr"),
    R("http"),
    R("khttp"),
    R("pgpsigner"),
    R("irc"),
    R("mandarin-agent"),
    R("write"),
    R("palladium"),
    {0, 0, 0, 0},
#undef R
#undef RC
#undef NR
};

/*
 * char *strnchr(s, c, n)
 *   char *s;
 *   char c;
 *   unsigned int n;
 *
 * returns a pointer to the first occurrence of character c in the
 * string s, or a NULL pointer if c does not occur in in the string;
 * however, at most the first n characters will be considered.
 *
 * This falls in the "should have been in the ANSI C library"
 * category. :-)
 */
static char *
strnchr(char *s, int c, unsigned int n)
{
    if (n < 1)
        return 0;

    while (n-- && *s) {
        if (*s == c)
            return s;
        s++;
    }
    return 0;
}


/* XXX This calls for a new error code */
#define KRB5_INVALID_PRINCIPAL KRB5_LNAME_BADFORMAT

krb5_error_code KRB5_CALLCONV
krb5_524_conv_principal(krb5_context context, krb5_const_principal princ,
                        char *name, char *inst, char *realm)
{
    const struct krb_convert *p;
    const krb5_data *compo;
    char *c, *tmp_realm, *tmp_prealm;
    unsigned int tmp_realm_len;
    int retval;

    if (context->profile == 0)
        return KRB5_CONFIG_CANTOPEN;

    *name = *inst = '\0';
    switch (princ->length) {
    case 2:
        /* Check if this principal is listed in the table */
        compo = &princ->data[0];
        p = sconv_list;
        while (p->v4_str) {
            if (p->len == compo->length
                && memcmp(p->v5_str, compo->data, compo->length) == 0) {
                /*
                 * It is, so set the new name now, and chop off
                 * instance's domain name if requested.
                 */
                if (strlcpy(name, p->v4_str, ANAME_SZ) >= ANAME_SZ)
                    return KRB5_INVALID_PRINCIPAL;
                if (p->flags & DO_REALM_CONVERSION) {
                    compo = &princ->data[1];
                    c = strnchr(compo->data, '.', compo->length);
                    if (!c || (c - compo->data) >= INST_SZ - 1)
                        return KRB5_INVALID_PRINCIPAL;
                    memcpy(inst, compo->data, (size_t) (c - compo->data));
                    inst[c - compo->data] = '\0';
                }
                break;
            }
            p++;
        }
        /* If inst isn't set, the service isn't listed in the table, */
        /* so just copy it. */
        if (*inst == '\0') {
            compo = &princ->data[1];
            if (compo->length >= INST_SZ - 1)
                return KRB5_INVALID_PRINCIPAL;
            if (compo->length > 0)
                memcpy(inst, compo->data, compo->length);
            inst[compo->length] = '\0';
        }
        /* fall through */
    case 1:
        /* name may have been set above; otherwise, just copy it */
        if (*name == '\0') {
            compo = &princ->data[0];
            if (compo->length >= ANAME_SZ)
                return KRB5_INVALID_PRINCIPAL;
            if (compo->length > 0)
                memcpy(name, compo->data, compo->length);
            name[compo->length] = '\0';
        }
        break;
    default:
        return KRB5_INVALID_PRINCIPAL;
    }

    compo = &princ->realm;

    tmp_prealm = malloc(compo->length + 1);
    if (tmp_prealm == NULL)
        return ENOMEM;
    strncpy(tmp_prealm, compo->data, compo->length);
    tmp_prealm[compo->length] = '\0';

    /* Ask for v4_realm corresponding to
       krb5 principal realm from krb5.conf realms stanza */

    retval = profile_get_string(context->profile, KRB5_CONF_REALMS,
                                tmp_prealm, KRB5_CONF_V4_REALM, 0,
                                &tmp_realm);
    free(tmp_prealm);
    if (retval) {
        return retval;
    } else {
        if (tmp_realm == 0) {
            if (compo->length > REALM_SZ - 1)
                return KRB5_INVALID_PRINCIPAL;
            strncpy(realm, compo->data, compo->length);
            realm[compo->length] = '\0';
        } else {
            tmp_realm_len =  strlen(tmp_realm);
            if (tmp_realm_len > REALM_SZ - 1) {
                profile_release_string(tmp_realm);
                return KRB5_INVALID_PRINCIPAL;
            }
            strncpy(realm, tmp_realm, tmp_realm_len);
            realm[tmp_realm_len] = '\0';
            profile_release_string(tmp_realm);
        }
    }
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_425_conv_principal(krb5_context context, const char *name,
                        const char *instance, const char *realm,
                        krb5_principal *princ)
{
    const struct krb_convert *p;
    char buf[256];             /* V4 instances are limited to 40 characters */
    krb5_error_code retval;
    char *domain, *cp;
    char **full_name = 0;
    const char *names[5], *names2[2];
    void*      iterator = NULL;
    char** v4realms = NULL;
    char* realm_name = NULL;
    char* dummy_value = NULL;

    /* First, convert the realm, since the v4 realm is not necessarily the same as the v5 realm
       To do that, iterate over all the realms in the config file, looking for a matching
       v4_realm line */
    names2 [0] = KRB5_CONF_REALMS;
    names2 [1] = NULL;
    retval = profile_iterator_create (context -> profile, names2, PROFILE_ITER_LIST_SECTION | PROFILE_ITER_SECTIONS_ONLY, &iterator);
    while (retval == 0) {
        retval = profile_iterator (&iterator, &realm_name, &dummy_value);
        if ((retval == 0) && (realm_name != NULL)) {
            names [0] = KRB5_CONF_REALMS;
            names [1] = realm_name;
            names [2] = KRB5_CONF_V4_REALM;
            names [3] = NULL;

            retval = profile_get_values (context -> profile, names, &v4realms);
            if ((retval == 0) && (v4realms != NULL) && (v4realms [0] != NULL) && (strcmp (v4realms [0], realm) == 0)) {
                realm = realm_name;
                break;
            } else if (retval == PROF_NO_RELATION) {
                /* If it's not found, just keep going */
                retval = 0;
            }
        } else if ((retval == 0) && (realm_name == NULL)) {
            break;
        }
        if (v4realms != NULL) {
            profile_free_list(v4realms);
            v4realms = NULL;
        }
        if (realm_name != NULL) {
            profile_release_string (realm_name);
            realm_name = NULL;
        }
        if (dummy_value != NULL) {
            profile_release_string (dummy_value);
            dummy_value = NULL;
        }
    }

    if (instance) {
        if (instance[0] == '\0') {
            instance = 0;
            goto not_service;
        }
        p = sconv_list;
        while (1) {
            if (!p->v4_str)
                goto not_service;
            if (!strcmp(p->v4_str, name))
                break;
            p++;
        }
        name = p->v5_str;
        if ((p->flags & DO_REALM_CONVERSION) && !strchr(instance, '.')) {
            names[0] = KRB5_CONF_REALMS;
            names[1] = realm;
            names[2] = KRB5_CONF_V4_INSTANCE_CONVERT;
            names[3] = instance;
            names[4] = 0;
            retval = profile_get_values(context->profile, names, &full_name);
            if (retval == 0 && full_name && full_name[0]) {
                instance = full_name[0];
            } else {
                strncpy(buf, instance, sizeof(buf));
                buf[sizeof(buf) - 1] = '\0';
                retval = krb5_get_realm_domain(context, realm, &domain);
                if (retval)
                    goto cleanup;
                if (domain) {
                    for (cp = domain; *cp; cp++)
                        if (isupper((unsigned char) (*cp)))
                            *cp = tolower((unsigned char) *cp);
                    strncat(buf, ".", sizeof(buf) - 1 - strlen(buf));
                    strncat(buf, domain, sizeof(buf) - 1 - strlen(buf));
                    free(domain);
                }
                instance = buf;
            }
        }
    }

not_service:
    retval = krb5_build_principal(context, princ, strlen(realm), realm, name,
                                  instance, NULL);
cleanup:
    if (iterator) profile_iterator_free (&iterator);
    if (full_name) profile_free_list(full_name);
    if (v4realms) profile_free_list(v4realms);
    if (realm_name) profile_release_string (realm_name);
    if (dummy_value) profile_release_string (dummy_value);
    return retval;
}
