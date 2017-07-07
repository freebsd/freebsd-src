/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/chk_trans.c */
/*
 * Copyright 2001, 2007 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include <stdarg.h>

#if defined (TEST) || defined (TEST2)
# undef DEBUG
# define DEBUG
#endif

#ifdef DEBUG
#define verbose krb5int_chk_trans_verbose
static int verbose = 0;
# define Tprintf(ARGS) if (verbose) printf ARGS
#else
# define Tprintf(ARGS) (void)(0)
#endif

#define MAXLEN 512

static krb5_error_code
process_intermediates (krb5_error_code (*fn)(krb5_data *, void *), void *data,
                       const krb5_data *n1, const krb5_data *n2) {
    unsigned int len1, len2, i;
    char *p1, *p2;

    Tprintf (("process_intermediates(%.*s,%.*s)\n",
              (int) n1->length, n1->data, (int) n2->length, n2->data));

    len1 = n1->length;
    len2 = n2->length;

    Tprintf (("(walking intermediates now)\n"));
    /* Simplify...  */
    if (len1 > len2) {
        const krb5_data *p;
        int tmp = len1;
        len1 = len2;
        len2 = tmp;
        p = n1;
        n1 = n2;
        n2 = p;
    }
    /* Okay, now len1 is always shorter or equal.  */
    if (len1 == len2) {
        if (memcmp (n1->data, n2->data, len1)) {
            Tprintf (("equal length but different strings in path: '%.*s' '%.*s'\n",
                      (int) n1->length, n1->data, (int) n2->length, n2->data));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        Tprintf (("(end intermediates)\n"));
        return 0;
    }
    /* Now len1 is always shorter.  */
    if (len1 == 0)
        /* Shouldn't be possible.  Internal error?  */
        return KRB5KRB_AP_ERR_ILL_CR_TKT;
    p1 = n1->data;
    p2 = n2->data;
    if (p1[0] == '/') {
        /* X.500 style names, with common prefix.  */
        if (p2[0] != '/') {
            Tprintf (("mixed name formats in path: x500='%.*s' domain='%.*s'\n",
                      (int) len1, p1, (int) len2, p2));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        if (memcmp (p1, p2, len1)) {
            Tprintf (("x500 names with different prefixes '%.*s' '%.*s'\n",
                      (int) len1, p1, (int) len2, p2));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        for (i = len1 + 1; i < len2; i++)
            if (p2[i] == '/') {
                krb5_data d;
                krb5_error_code r;

                d.data = p2;
                d.length = i;
                r = (*fn) (&d, data);
                if (r)
                    return r;
            }
    } else {
        /* Domain style names, with common suffix.  */
        if (p2[0] == '/') {
            Tprintf (("mixed name formats in path: domain='%.*s' x500='%.*s'\n",
                      (int) len1, p1, (int) len2, p2));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        if (memcmp (p1, p2 + (len2 - len1), len1)) {
            Tprintf (("domain names with different suffixes '%.*s' '%.*s'\n",
                      (int) len1, p1, (int) len2, p2));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        for (i = len2 - len1 - 1; i > 0; i--) {
            Tprintf (("looking at '%.*s'\n", (int) (len2 - i), p2+i));
            if (p2[i-1] == '.') {
                krb5_data d;
                krb5_error_code r;

                d.data = p2+i;
                d.length = len2 - i;
                r = (*fn) (&d, data);
                if (r)
                    return r;
            }
        }
    }
    Tprintf (("(end intermediates)\n"));
    return 0;
}

static krb5_error_code
maybe_join (krb5_data *last, krb5_data *buf, unsigned int bufsiz)
{
    if (buf->length == 0)
        return 0;
    if (buf->data[0] == '/') {
        if (last->length + buf->length > bufsiz) {
            Tprintf (("too big: last=%d cur=%d max=%d\n", last->length, buf->length, bufsiz));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        memmove (buf->data+last->length, buf->data, buf->length);
        memcpy (buf->data, last->data, last->length);
        buf->length += last->length;
    } else if (buf->data[buf->length-1] == '.') {
        /* We can ignore the case where the previous component was
           empty; the strcat will be a no-op.  It should probably
           be an error case, but let's be flexible.  */
        if (last->length+buf->length > bufsiz) {
            Tprintf (("too big\n"));
            return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
        memcpy (buf->data + buf->length, last->data, last->length);
        buf->length += last->length;
    }
    /* Otherwise, do nothing.  */
    return 0;
}

/* The input strings cannot contain any \0 bytes, according to the
   spec, but our API is such that they may not be \0 terminated
   either.  Thus we keep on treating them as krb5_data objects instead
   of C strings.  */
static krb5_error_code
foreach_realm (krb5_error_code (*fn)(krb5_data *comp,void *data), void *data,
               const krb5_data *crealm, const krb5_data *srealm,
               const krb5_data *transit)
{
    char buf[MAXLEN], last[MAXLEN];
    char *p, *bufp;
    int next_lit, intermediates, l;
    krb5_data this_component;
    krb5_error_code r;
    krb5_data last_component;

    /* Invariants:
       - last_component points to last[]
       - this_component points to buf[]
       - last_component has length of last
       - this_component has length of buf when calling out
       Keep these consistent, and we should be okay.  */

    next_lit = 0;
    intermediates = 0;
    memset (buf, 0, sizeof (buf));

    this_component.data = buf;
    last_component.data = last;
    last_component.length = 0;

#define print_data(fmt,d) Tprintf((fmt,(int)(d)->length,(d)->data))
    print_data ("client realm: %.*s\n", crealm);
    print_data ("server realm: %.*s\n", srealm);
    print_data ("transit enc.: %.*s\n", transit);

    if (transit->length == 0) {
        Tprintf (("no other realms transited\n"));
        return 0;
    }

    bufp = buf;
    for (p = transit->data, l = transit->length; l; p++, l--) {
        if (next_lit) {
            *bufp++ = *p;
            if (bufp == buf+sizeof(buf))
                return KRB5KRB_AP_ERR_ILL_CR_TKT;
            next_lit = 0;
        } else if (*p == '\\') {
            next_lit = 1;
        } else if (*p == ',') {
            if (bufp != buf) {
                this_component.length = bufp - buf;
                r = maybe_join (&last_component, &this_component, sizeof(buf));
                if (r)
                    return r;
                r = (*fn) (&this_component, data);
                if (r)
                    return r;
                if (intermediates) {
                    if (p == transit->data)
                        r = process_intermediates (fn, data,
                                                   &this_component, crealm);
                    else {
                        r = process_intermediates (fn, data, &this_component,
                                                   &last_component);
                    }
                    if (r)
                        return r;
                }
                intermediates = 0;
                memcpy (last, buf, sizeof (buf));
                last_component.length = this_component.length;
                memset (buf, 0, sizeof (buf));
                bufp = buf;
            } else {
                intermediates = 1;
                if (p == transit->data) {
                    if (crealm->length >= MAXLEN)
                        return KRB5KRB_AP_ERR_ILL_CR_TKT;
                    if (crealm->length > 0)
                        memcpy (last, crealm->data, crealm->length);
                    last[crealm->length] = '\0';
                    last_component.length = crealm->length;
                }
            }
        } else if (*p == ' ' && bufp == buf) {
            /* This next component stands alone, even if it has a
               trailing dot or leading slash.  */
            memset (last, 0, sizeof (last));
            last_component.length = 0;
        } else {
            /* Not a special character; literal.  */
            *bufp++ = *p;
            if (bufp == buf+sizeof(buf))
                return KRB5KRB_AP_ERR_ILL_CR_TKT;
        }
    }
    /* At end.  Must be normal state.  */
    if (next_lit)
        Tprintf (("ending in next-char-literal state\n"));
    /* Process trailing element or comma.  */
    if (bufp == buf) {
        /* Trailing comma.  */
        r = process_intermediates (fn, data, &last_component, srealm);
    } else {
        /* Trailing component.  */
        this_component.length = bufp - buf;
        r = maybe_join (&last_component, &this_component, sizeof(buf));
        if (r)
            return r;
        r = (*fn) (&this_component, data);
        if (r)
            return r;
        if (intermediates)
            r = process_intermediates (fn, data, &this_component,
                                       &last_component);
    }
    if (r != 0)
        return r;
    return 0;
}

struct check_data {
    krb5_context ctx;
    krb5_principal *tgs;
};

static krb5_error_code
check_realm_in_list (krb5_data *realm, void *data)
{
    struct check_data *cdata = data;
    int i;

    Tprintf ((".. checking '%.*s'\n", (int) realm->length, realm->data));
    for (i = 0; cdata->tgs[i]; i++) {
        if (data_eq (cdata->tgs[i]->realm, *realm))
            return 0;
    }
    Tprintf (("BAD!\n"));
    return KRB5KRB_AP_ERR_ILL_CR_TKT;
}

krb5_error_code
krb5_check_transited_list (krb5_context ctx, const krb5_data *trans_in,
                           const krb5_data *crealm, const krb5_data *srealm)
{
    krb5_data trans;
    struct check_data cdata;
    krb5_error_code r;
    const krb5_data *anonymous;

    trans.length = trans_in->length;
    trans.data = (char *) trans_in->data;
    if (trans.length && (trans.data[trans.length-1] == '\0'))
        trans.length--;

    Tprintf (("krb5_check_transited_list(trans=\"%.*s\", crealm=\"%.*s\", srealm=\"%.*s\")\n",
              (int) trans.length, trans.data,
              (int) crealm->length, crealm->data,
              (int) srealm->length, srealm->data));
    if (trans.length == 0)
        return 0;
    anonymous = krb5_anonymous_realm();
    if (crealm->length == anonymous->length &&
        (memcmp(crealm->data, anonymous->data, anonymous->length) == 0))
        return 0; /* Nothing to check for anonymous */

    r = krb5_walk_realm_tree (ctx, crealm, srealm, &cdata.tgs,
                              KRB5_REALM_BRANCH_CHAR);
    if (r) {
        Tprintf (("error %ld\n", (long) r));
        return r;
    }
#ifdef DEBUG /* avoid compiler warning about 'd' unused */
    {
        int i;
        Tprintf (("tgs list = {\n"));
        for (i = 0; cdata.tgs[i]; i++) {
            char *name;
            r = krb5_unparse_name (ctx, cdata.tgs[i], &name);
            Tprintf (("\t'%s'\n", name));
            free (name);
        }
        Tprintf (("}\n"));
    }
#endif
    cdata.ctx = ctx;
    r = foreach_realm (check_realm_in_list, &cdata, crealm, srealm, &trans);
    krb5_free_realm_tree (ctx, cdata.tgs);
    return r;
}

#ifdef TEST

static krb5_error_code
print_a_realm (krb5_data *realm, void *data)
{
    printf ("%.*s\n", (int) realm->length, realm->data);
    return 0;
}

int main (int argc, char *argv[]) {
    const char *me;
    krb5_data crealm, srealm, transit;
    krb5_error_code r;
    int expand_only = 0;

    me = strrchr (argv[0], '/');
    me = me ? me+1 : argv[0];

    while (argc > 3 && argv[1][0] == '-') {
        if (!strcmp ("-v", argv[1]))
            verbose++, argc--, argv++;
        else if (!strcmp ("-x", argv[1]))
            expand_only++, argc--, argv++;
        else
            goto usage;
    }

    if (argc != 4) {
    usage:
        printf ("usage: %s [-v] [-x] clientRealm serverRealm transitEncoding\n",
                me);
        return 1;
    }

    crealm.data = argv[1];
    crealm.length = strlen(argv[1]);
    srealm.data = argv[2];
    srealm.length = strlen(argv[2]);
    transit.data = argv[3];
    transit.length = strlen(argv[3]);

    if (expand_only) {

        printf ("client realm: %s\n", argv[1]);
        printf ("server realm: %s\n", argv[2]);
        printf ("transit enc.: %s\n", argv[3]);

        if (argv[3][0] == 0) {
            printf ("no other realms transited\n");
            return 0;
        }

        r = foreach_realm (print_a_realm, NULL, &crealm, &srealm, &transit);
        if (r)
            printf ("--> returned error %ld\n", (long) r);
        return r != 0;

    } else {

        /* Actually check the values against the supplied krb5.conf file.  */
        krb5_context ctx;
        r = krb5_init_context (&ctx);
        if (r) {
            com_err (me, r, "initializing krb5 context");
            return 1;
        }
        r = krb5_check_transited_list (ctx, &transit, &crealm, &srealm);
        if (r == KRB5KRB_AP_ERR_ILL_CR_TKT) {
            printf ("NO\n");
        } else if (r == 0) {
            printf ("YES\n");
        } else {
            printf ("kablooey!\n");
            com_err (me, r, "checking transited-realm list");
            return 1;
        }
        return 0;
    }
}

#endif /* TEST */
