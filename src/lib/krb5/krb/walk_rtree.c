/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/walk_rtree.c */
/*
 * Copyright 1990,1991,2008,2009 by the Massachusetts Institute of Technology.
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
 * krb5_walk_realm_tree()
 * krb5_free_realm_tree()
 *
 * internal function, used by krb5_get_cred_from_kdc()
 */

#include "k5-int.h"
#include "int-proto.h"

/*
 * Structure to help with finding the common suffix between client and
 * server realm during hierarchical traversal.
 */
struct hstate {
    char *str;
    size_t len;
    char *tail;
    char *dot;
};

static krb5_error_code
rtree_capath_tree(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  char **vals,
                  krb5_principal **tree);

static krb5_error_code
rtree_capath_vals(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  char ***vals);

static krb5_error_code
rtree_hier_tree(krb5_context context,
                const krb5_data *client,
                const krb5_data *server,
                krb5_principal **rettree,
                int sep);

static krb5_error_code
rtree_hier_realms(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  krb5_data **realms,
                  size_t *nrealms,
                  int sep);

static void
free_realmlist(krb5_context context,
               krb5_data *realms,
               size_t nrealms);

static krb5_error_code
rtree_hier_tweens(krb5_context context,
                  struct hstate *realm,
                  krb5_data **tweens,
                  size_t *ntweens,
                  int dotail,
                  int sep);

static void
adjtail(struct hstate *c, struct hstate *s, int sep);

static void
comtail(struct hstate *c, struct hstate *s, int sep);

krb5_error_code
krb5_walk_realm_tree( krb5_context context,
                      const krb5_data *client,
                      const krb5_data *server,
                      krb5_principal **tree,
                      int realm_sep)
{
    krb5_error_code retval = 0;
    char **capvals;

    if (client->data == NULL || server->data == NULL)
        return KRB5_NO_TKT_IN_RLM;

    if (data_eq(*client, *server))
        return KRB5_NO_TKT_IN_RLM;
    retval = rtree_capath_vals(context, client, server, &capvals);
    if (retval)
        return retval;

    if (capvals != NULL) {
        retval = rtree_capath_tree(context, client, server, capvals, tree);
        return retval;
    }

    retval = rtree_hier_tree(context, client, server, tree, realm_sep);
    return retval;
}

krb5_error_code
k5_client_realm_path(krb5_context context, const krb5_data *client,
                     const krb5_data *server, krb5_data **rpath_out)
{
    krb5_error_code retval;
    char **capvals = NULL;
    size_t i;
    krb5_data *rpath = NULL, d;

    retval = rtree_capath_vals(context, client, server, &capvals);
    if (retval)
        return retval;

    /* A capaths value of "." means no intermediates. */
    if (capvals != NULL && capvals[0] != NULL && *capvals[0] == '.') {
        profile_free_list(capvals);
        capvals = NULL;
    }

    /* Count capaths (if any) and allocate space.  Leave room for the client
     * realm, server realm, and terminator. */
    for (i = 0; capvals != NULL && capvals[i] != NULL; i++);
    rpath = calloc(i + 3, sizeof(*rpath));
    if (rpath == NULL)
        return ENOMEM;

    /* Populate rpath with the client realm, capaths, and server realm. */
    retval = krb5int_copy_data_contents(context, client, &rpath[0]);
    if (retval)
        goto cleanup;
    for (i = 0; capvals != NULL && capvals[i] != NULL; i++) {
        d = make_data(capvals[i], strcspn(capvals[i], "\t "));
        retval = krb5int_copy_data_contents(context, &d, &rpath[i + 1]);
        if (retval)
            goto cleanup;
    }
    retval = krb5int_copy_data_contents(context, server, &rpath[i + 1]);
    if (retval)
        goto cleanup;

    /* Terminate rpath and return it. */
    rpath[i + 2] = empty_data();
    *rpath_out = rpath;
    rpath = NULL;

cleanup:
    profile_free_list(capvals);
    krb5int_free_data_list(context, rpath);
    return retval;
}

/* ANL - Modified to allow Configurable Authentication Paths.
 * This modification removes the restriction on the choice of realm
 * names, i.e. they nolonger have to be hierarchical. This
 * is allowed by RFC 1510: "If a hierarchical organization is not used
 * it may be necessary to consult some database in order to construct
 * an authentication path between realms."  The database is contained
 * in the [capaths] section of the krb5.conf file.
 * Client to server paths are defined. There are n**2 possible
 * entries, but only those entries which are needed by the client
 * or server need be present in its krb5.conf file. (n entries or 2*n
 * entries if the same krb5.conf is used for clients and servers)
 *
 * for example: ESnet will be running a KDC which will share
 * inter-realm keys with its many organizations which include among
 * other ANL, NERSC and PNL. Each of these organizations wants to
 * use its DNS name in the realm, ANL.GOV. In addition ANL wants
 * to authenticatite to HAL.COM via a K5.MOON and K5.JUPITER
 * A [capaths] section of the krb5.conf file for the ANL.GOV clients
 * and servers would look like:
 *
 * [capaths]
 * ANL.GOV = {
 *              NERSC.GOV = ES.NET
 *              PNL.GOV = ES.NET
 *              ES.NET = .
 *              HAL.COM = K5.MOON
 *              HAL.COM = K5.JUPITER
 * }
 * NERSC.GOV = {
 *              ANL.GOV = ES.NET
 * }
 * PNL.GOV = {
 *              ANL.GOV = ES.NET
 * }
 * ES.NET = {
 *              ANL.GOV = .
 * }
 * HAL.COM = {
 *              ANL.GOV = K5.JUPITER
 *              ANL.GOV = K5.MOON
 * }
 *
 * In the above a "." is used to mean directly connected since the
 * the profile routines cannot handle a null entry.
 *
 * If no client-to-server path is found, the default hierarchical path
 * is still generated.
 *
 * This version of the Configurable Authentication Path modification
 * differs from the previous versions prior to K5 beta 5 in that
 * the profile routines are used, and the explicit path from client's
 * realm to server's realm must be given. The modifications will work
 * together.
 * DEE - 5/23/95
 */

/*
 * Build a tree given a set of profile values retrieved by
 * walk_rtree_capath_vals().
 */
static krb5_error_code
rtree_capath_tree(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  char **vals,
                  krb5_principal **rettree)
{
    krb5_error_code retval = 0;
    unsigned int nvals, nlinks, nprincs, i;
    krb5_data srcrealm, dstrealm;
    krb5_principal *tree, *pprinc;

    *rettree = NULL;
    tree = pprinc = NULL;
    for (nvals = 0; vals[nvals] != NULL; nvals++)
        ;
    if (vals[0] != NULL && *vals[0] == '.') {
        nlinks = 0;
    } else {
        nlinks = nvals;
    }
    nprincs = nlinks + 2;
    tree = calloc(nprincs + 1, sizeof(krb5_principal));
    if (tree == NULL) {
        retval = ENOMEM;
        goto error;
    }
    for (i = 0; i < nprincs + 1; i++)
        tree[i] = NULL;
    /* Invariant: PPRINC points one past end of list. */
    pprinc = &tree[0];
    /* Local TGS name */
    retval = krb5int_tgtname(context, client, client, pprinc++);
    if (retval) goto error;
    srcrealm = *client;
    for (i = 0; i < nlinks; i++) {
        dstrealm.data = vals[i];
        dstrealm.length = strcspn(vals[i], "\t ");
        retval = krb5int_tgtname(context, &dstrealm, &srcrealm, pprinc++);
        if (retval) goto error;
        srcrealm = dstrealm;
    }
    retval = krb5int_tgtname(context, server, &srcrealm, pprinc++);
    if (retval) goto error;
    *rettree = tree;

error:
    profile_free_list(vals);
    if (retval) {
        while (pprinc != NULL && pprinc > &tree[0]) {
            /* krb5_free_principal() correctly handles null input */
            krb5_free_principal(context, *--pprinc);
            *pprinc = NULL;
        }
        free(tree);
    }
    return retval;
}

/*
 * Get realm list from "capaths" section of the profile.  Deliberately
 * returns success but leaves VALS null if profile_get_values() fails
 * by not finding anything.
 */
static krb5_error_code
rtree_capath_vals(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  char ***vals)
{
    krb5_error_code retval = 0;
    /* null-terminated realm names */
    char *clientz = NULL, *serverz = NULL;
    const char *key[4];

    *vals = NULL;

    clientz = k5memdup0(client->data, client->length, &retval);
    if (clientz == NULL)
        goto error;

    serverz = k5memdup0(server->data, server->length, &retval);
    if (serverz == NULL)
        goto error;

    key[0] = "capaths";
    key[1] = clientz;
    key[2] = serverz;
    key[3] = NULL;
    retval = profile_get_values(context->profile, key, vals);
    switch (retval) {
    case PROF_NO_SECTION:
    case PROF_NO_RELATION:
        /*
         * Not found; don't return an error.
         */
        retval = 0;
        break;
    default:
        break;
    }
error:
    free(clientz);
    free(serverz);
    return retval;
}

/*
 * Build tree by hierarchical traversal.
 */
static krb5_error_code
rtree_hier_tree(krb5_context context,
                const krb5_data *client,
                const krb5_data *server,
                krb5_principal **rettree,
                int sep)
{
    krb5_error_code retval;
    krb5_data *realms;
    const krb5_data *dstrealm, *srcrealm;
    krb5_principal *tree, *pprinc;
    size_t nrealms, nprincs, i;

    *rettree = NULL;
    retval = rtree_hier_realms(context, client, server,
                               &realms, &nrealms, sep);
    if (retval)
        return retval;
    nprincs = nrealms;
    pprinc = tree = calloc(nprincs + 1, sizeof(krb5_principal));
    if (tree == NULL) {
        retval = ENOMEM;
        goto error;
    }
    for (i = 0; i < nrealms; i++)
        tree[i] = NULL;
    srcrealm = client;
    for (i = 0; i < nrealms; i++) {
        dstrealm = &realms[i];
        retval = krb5int_tgtname(context, dstrealm, srcrealm, pprinc++);
        if (retval) goto error;
        srcrealm = dstrealm;
    }
    *rettree = tree;
    free_realmlist(context, realms, nrealms);
    return 0;
error:
    while (pprinc != NULL && pprinc > tree) {
        krb5_free_principal(context, *--pprinc);
        *pprinc = NULL;
    }
    free_realmlist(context, realms, nrealms);
    free(tree);
    return retval;
}

/*
 * Construct list of realms between client and server.
 */
static krb5_error_code
rtree_hier_realms(krb5_context context,
                  const krb5_data *client,
                  const krb5_data *server,
                  krb5_data **realms,
                  size_t *nrealms,
                  int sep)
{
    krb5_error_code retval;
    struct hstate c, s;
    krb5_data *ctweens = NULL, *stweens = NULL, *twp, *r, *rp;
    size_t nctween, nstween;

    *realms = NULL;
    *nrealms = 0;

    r = rp = NULL;
    c.str = client->data;
    c.len = client->length;
    c.dot = c.tail = NULL;
    s.str = server->data;
    s.len = server->length;
    s.dot = s.tail = NULL;

    comtail(&c, &s, sep);
    adjtail(&c, &s, sep);

    retval = rtree_hier_tweens(context, &c, &ctweens, &nctween, 1, sep);
    if (retval) goto error;
    retval = rtree_hier_tweens(context, &s, &stweens, &nstween, 0, sep);
    if (retval) goto error;

    rp = r = calloc(nctween + nstween, sizeof(krb5_data));
    if (r == NULL) {
        retval = ENOMEM;
        goto error;
    }
    /* Copy client realm "tweens" forward. */
    for (twp = ctweens; twp < &ctweens[nctween]; twp++) {
        retval = krb5int_copy_data_contents(context, twp, rp);
        if (retval) goto error;
        rp++;
    }
    /* Copy server realm "tweens" backward. */
    for (twp = &stweens[nstween]; twp-- > stweens;) {
        retval = krb5int_copy_data_contents(context, twp, rp);
        if (retval) goto error;
        rp++;
    }
error:
    free(ctweens);
    free(stweens);
    if (retval) {
        free_realmlist(context, r, rp - r);
        return retval;
    }
    *realms = r;
    *nrealms = rp - r;
    return 0;
}

static void
free_realmlist(krb5_context context,
               krb5_data *realms,
               size_t nrealms)
{
    size_t i;

    for (i = 0; i < nrealms; i++)
        krb5_free_data_contents(context, &realms[i]);
    free(realms);
}

/*
 * Build a list of realms between a given realm and the common
 * suffix.  The original realm is included, but the "tail" is only
 * included if DOTAIL is true.
 *
 * Warning: This function intentionally aliases memory.  Caller must
 * make copies as needed and not call krb5_free_data_contents, etc.
 */
static krb5_error_code
rtree_hier_tweens(krb5_context context,
                  struct hstate *realm,
                  krb5_data **tweens,
                  size_t *ntweens,
                  int dotail,
                  int sep)
{
    char *p, *r, *rtail, *lp;
    size_t rlen, n;
    krb5_data *tws, *ntws;

    r = realm->str;
    rlen = realm->len;
    rtail = realm->tail;
    *tweens = ntws = tws = NULL;
    *ntweens = n = 0;

    for (lp = p = r; p < &r[rlen]; p++) {
        if (*p != sep && &p[1] != &r[rlen])
            continue;
        if (lp == rtail && !dotail)
            break;
        ntws = realloc(tws, (n + 1) * sizeof(krb5_data));
        if (ntws == NULL) {
            free(tws);
            return ENOMEM;
        }
        tws = ntws;
        tws[n].data = lp;
        tws[n].length = &r[rlen] - lp;
        n++;
        if (lp == rtail)
            break;
        lp = &p[1];
    }
    *tweens = tws;
    *ntweens = n;
    return 0;
}

/*
 * Adjust suffixes that each starts at the beginning of a component,
 * to avoid the problem where "BC.EXAMPLE.COM" is erroneously reported
 * as a parent of "ABC.EXAMPLE.COM".
 */
static void
adjtail(struct hstate *c, struct hstate *s, int sep)
{
    int cfull, sfull;
    char *cp, *sp;

    cp = c->tail;
    sp = s->tail;
    if (cp == NULL || sp == NULL)
        return;
    /*
     * Is it a full component?  Yes, if it's the beginning of the
     * string or there's a separator to the left.
     *
     * The index of -1 is valid because it only gets evaluated if the
     * pointer is not at the beginning of the string.
     */
    cfull = (cp == c->str || cp[-1] == sep);
    sfull = (sp == s->str || sp[-1] == sep);
    /*
     * If they're both full components, we're done.
     */
    if (cfull && sfull) {
        return;
    } else if (c->dot != NULL && s->dot != NULL) {
        cp = c->dot + 1;
        sp = s->dot + 1;
        /*
         * Out of bounds? Can only happen if there are trailing dots.
         */
        if (cp >= &c->str[c->len] || sp >= &s->str[s->len]) {
            cp = sp = NULL;
        }
    } else {
        cp = sp = NULL;
    }
    c->tail = cp;
    s->tail = sp;
}

/*
 * Find common suffix of C and S.
 *
 * C->TAIL and S->TAIL will point to the respective suffixes.  C->DOT
 * and S->DOT will point to the nearest instances of SEP to the right
 * of the start of each suffix.  Caller must initialize TAIL and DOT
 * pointers to null.
 */
static void
comtail(struct hstate *c, struct hstate *s, int sep)
{
    char *cp, *sp, *cdot, *sdot;

    if (c->len == 0 || s->len == 0)
        return;

    cdot = sdot = NULL;
    /*
     * ANSI/ISO C allows a pointer one past the end but not one
     * before the beginning of an array.
     */
    cp = &c->str[c->len];
    sp = &s->str[s->len];
    /*
     * Set CP and SP to point to the common suffix of each string.
     * When we run into separators (dots, unless someone has a X.500
     * style realm), keep pointers to the latest pair.
     */
    while (cp > c->str && sp > s->str) {
        if (*--cp != *--sp) {
            /*
             * Didn't match, so most recent match is one byte to the
             * right (or not at all).
             */
            cp++;
            sp++;
            break;
        }
        /*
         * Keep track of matching dots.
         */
        if (*cp == sep) {
            cdot = cp;
            sdot = sp;
        }
    }
    /* No match found at all. */
    if (cp == &c->str[c->len])
        return;
    c->tail = cp;
    s->tail = sp;
    c->dot = cdot;
    s->dot = sdot;
}

void
krb5_free_realm_tree(krb5_context context, krb5_principal *realms)
{
    krb5_principal *nrealms = realms;
    if (realms == NULL)
        return;
    while (*nrealms) {
        krb5_free_principal(context, *nrealms);
        nrealms++;
    }
    free(realms);
}
