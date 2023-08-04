/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/cccursor.c */
/*
 * Copyright 2006, 2007 by the Massachusetts Institute of Technology.
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
 * cursor for sequential traversal of ccaches
 */

#include "cc-int.h"
#include "../krb/int-proto.h"
#include "../os/os-proto.h"

#include <assert.h>

struct _krb5_cccol_cursor {
    krb5_cc_typecursor typecursor;
    const krb5_cc_ops *ops;
    krb5_cc_ptcursor ptcursor;
};
/* typedef of krb5_cccol_cursor is in krb5.h */

krb5_error_code KRB5_CALLCONV
krb5_cccol_cursor_new(krb5_context context,
                      krb5_cccol_cursor *cursor)
{
    krb5_error_code ret = 0;
    krb5_cccol_cursor n = NULL;

    *cursor = NULL;
    n = malloc(sizeof(*n));
    if (n == NULL)
        return ENOMEM;

    n->typecursor = NULL;
    n->ptcursor = NULL;
    n->ops = NULL;

    ret = krb5int_cc_typecursor_new(context, &n->typecursor);
    if (ret)
        goto errout;

    do {
        /* Find first backend with ptcursor functionality. */
        ret = krb5int_cc_typecursor_next(context, n->typecursor, &n->ops);
        if (ret || n->ops == NULL)
            goto errout;
    } while (n->ops->ptcursor_new == NULL);

    ret = n->ops->ptcursor_new(context, &n->ptcursor);
    if (ret)
        goto errout;

errout:
    if (ret) {
        krb5_cccol_cursor_free(context, &n);
    }
    *cursor = n;
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_cccol_cursor_next(krb5_context context,
                       krb5_cccol_cursor cursor,
                       krb5_ccache *ccache_out)
{
    krb5_error_code ret = 0;
    krb5_ccache ccache;

    *ccache_out = NULL;

    /* Are we out of backends? */
    if (cursor->ops == NULL)
        return 0;

    while (1) {
        ret = cursor->ops->ptcursor_next(context, cursor->ptcursor, &ccache);
        if (ret)
            return ret;
        if (ccache != NULL) {
            *ccache_out = ccache;
            return 0;
        }

        ret = cursor->ops->ptcursor_free(context, &cursor->ptcursor);
        if (ret)
            return ret;

        do {
            /* Find next type with ptcursor functionality. */
            ret = krb5int_cc_typecursor_next(context, cursor->typecursor,
                                             &cursor->ops);
            if (ret)
                return ret;
            if (cursor->ops == NULL)
                return 0;
        } while (cursor->ops->ptcursor_new == NULL);

        ret = cursor->ops->ptcursor_new(context, &cursor->ptcursor);
        if (ret)
            return ret;
    }
}

krb5_error_code KRB5_CALLCONV
krb5_cccol_cursor_free(krb5_context context,
                       krb5_cccol_cursor *cursor)
{
    krb5_cccol_cursor c = *cursor;

    if (c == NULL)
        return 0;

    if (c->ptcursor != NULL)
        c->ops->ptcursor_free(context, &c->ptcursor);
    if (c->typecursor != NULL)
        krb5int_cc_typecursor_free(context, &c->typecursor);
    free(c);

    *cursor = NULL;
    return 0;
}

static krb5_error_code
match_caches(krb5_context context, krb5_const_principal client,
             krb5_ccache *cache_out)
{
    krb5_error_code ret;
    krb5_cccol_cursor cursor;
    krb5_ccache cache = NULL;
    krb5_principal princ;
    krb5_boolean eq;

    *cache_out = NULL;

    ret = krb5_cccol_cursor_new(context, &cursor);
    if (ret)
        return ret;

    while ((ret = krb5_cccol_cursor_next(context, cursor, &cache)) == 0 &&
           cache != NULL) {
        ret = krb5_cc_get_principal(context, cache, &princ);
        if (ret == 0) {
            eq = krb5_principal_compare(context, princ, client);
            krb5_free_principal(context, princ);
            if (eq)
                break;
        }
        krb5_cc_close(context, cache);
    }
    krb5_cccol_cursor_free(context, &cursor);

    if (ret)
        return ret;
    if (cache == NULL)
        return KRB5_CC_NOTFOUND;

    *cache_out = cache;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_cc_cache_match(krb5_context context, krb5_principal client,
                    krb5_ccache *cache_out)
{
    krb5_error_code ret;
    struct canonprinc iter = { client, .subst_defrealm = TRUE };
    krb5_const_principal canonprinc = NULL;
    krb5_ccache cache = NULL;
    char *name;

    *cache_out = NULL;

    while ((ret = k5_canonprinc(context, &iter, &canonprinc)) == 0 &&
           canonprinc != NULL) {
        ret = match_caches(context, canonprinc, &cache);
        if (ret != KRB5_CC_NOTFOUND)
            break;
    }
    free_canonprinc(&iter);

    if (ret == 0 && canonprinc == NULL) {
        ret = KRB5_CC_NOTFOUND;
        if (krb5_unparse_name(context, client, &name) == 0) {
            k5_setmsg(context, ret,
                      _("Can't find client principal %s in cache collection"),
                      name);
            krb5_free_unparsed_name(context, name);
        }
    }

    TRACE_CC_CACHE_MATCH(context, client, ret);
    if (ret)
        return ret;

    *cache_out = cache;
    return 0;
}

/* Store the error state for code from context into errsave, but only if code
 * indicates an error and errsave is empty. */
static void
save_first_error(krb5_context context, krb5_error_code code,
                 struct errinfo *errsave)
{
    if (code && code != KRB5_FCC_NOFILE && !errsave->code)
        k5_save_ctx_error(context, code, errsave);
}

krb5_error_code KRB5_CALLCONV
krb5_cccol_have_content(krb5_context context)
{
    krb5_error_code ret;
    krb5_cccol_cursor col_cursor;
    krb5_ccache cache;
    krb5_principal princ;
    krb5_boolean found = FALSE;
    struct errinfo errsave = EMPTY_ERRINFO;
    const char *defname;

    ret = krb5_cccol_cursor_new(context, &col_cursor);
    save_first_error(context, ret, &errsave);
    if (ret)
        goto no_entries;

    while (!found) {
        ret = krb5_cccol_cursor_next(context, col_cursor, &cache);
        save_first_error(context, ret, &errsave);
        if (ret || cache == NULL)
            break;
        ret = krb5_cc_get_principal(context, cache, &princ);
        save_first_error(context, ret, &errsave);
        if (!ret)
            found = TRUE;
        krb5_free_principal(context, princ);
        krb5_cc_close(context, cache);
    }
    krb5_cccol_cursor_free(context, &col_cursor);
    if (found) {
        k5_clear_error(&errsave);
        return 0;
    }

no_entries:
    if (errsave.code) {
        /* Report the first error we encountered. */
        ret = k5_restore_ctx_error(context, &errsave);
        k5_wrapmsg(context, ret, KRB5_CC_NOTFOUND,
                   _("No Kerberos credentials available"));
    } else {
        /* Report the default cache name. */
        defname = krb5_cc_default_name(context);
        if (defname != NULL) {
            k5_setmsg(context, KRB5_CC_NOTFOUND,
                      _("No Kerberos credentials available "
                        "(default cache: %s)"), defname);
        }
    }
    return KRB5_CC_NOTFOUND;
}
