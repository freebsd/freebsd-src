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

krb5_error_code KRB5_CALLCONV
krb5_cccol_last_change_time(krb5_context context,
                            krb5_timestamp *change_time)
{
    krb5_error_code ret = 0;
    krb5_cccol_cursor c = NULL;
    krb5_ccache ccache = NULL;
    krb5_timestamp last_time = 0;
    krb5_timestamp max_change_time = 0;

    *change_time = 0;

    ret = krb5_cccol_cursor_new(context, &c);

    while (!ret) {
        ret = krb5_cccol_cursor_next(context, c, &ccache);
        if (ccache) {
            ret = krb5_cc_last_change_time(context, ccache, &last_time);
            if (!ret && ts_after(last_time, max_change_time)) {
                max_change_time = last_time;
            }
            ret = 0;
        }
        else {
            break;
        }
    }
    *change_time = max_change_time;
    return ret;
}

/*
 * krb5_cccol_lock and krb5_cccol_unlock are defined in ccbase.c
 */

krb5_error_code KRB5_CALLCONV
krb5_cc_cache_match(krb5_context context, krb5_principal client,
                    krb5_ccache *cache_out)
{
    krb5_error_code ret;
    krb5_cccol_cursor cursor;
    krb5_ccache cache = NULL;
    krb5_principal princ;
    char *name;
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
    if (cache == NULL) {
        ret = krb5_unparse_name(context, client, &name);
        if (ret == 0) {
            k5_setmsg(context, KRB5_CC_NOTFOUND,
                      _("Can't find client principal %s in cache collection"),
                      name);
            krb5_free_unparsed_name(context, name);
        }
        ret = KRB5_CC_NOTFOUND;
    } else
        *cache_out = cache;
    return ret;
}

/* Store the error state for code from context into errsave, but only if code
 * indicates an error and errsave is empty. */
static void
save_first_error(krb5_context context, krb5_error_code code,
                 struct errinfo *errsave)
{
    if (code && code != KRB5_CC_END && !errsave->code)
        k5_save_ctx_error(context, code, errsave);
}

/* Return 0 if cache contains any non-config credentials.  Return KRB5_CC_END
 * if it does not, or another error if we failed to read through it. */
static krb5_error_code
has_content(krb5_context context, krb5_ccache cache)
{
    krb5_error_code ret;
    krb5_boolean found = FALSE;
    krb5_cc_cursor cache_cursor;
    krb5_creds creds;

    ret = krb5_cc_start_seq_get(context, cache, &cache_cursor);
    if (ret)
        return ret;
    while (!found) {
        ret = krb5_cc_next_cred(context, cache, &cache_cursor, &creds);
        if (ret)
            break;
        if (!krb5_is_config_principal(context, creds.server))
            found = TRUE;
        krb5_free_cred_contents(context, &creds);
    }
    krb5_cc_end_seq_get(context, cache, &cache_cursor);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_cccol_have_content(krb5_context context)
{
    krb5_error_code ret;
    krb5_cccol_cursor col_cursor;
    krb5_ccache cache;
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
        ret = has_content(context, cache);
        save_first_error(context, ret, &errsave);
        if (!ret)
            found = TRUE;
        krb5_cc_close(context, cache);
    }
    krb5_cccol_cursor_free(context, &col_cursor);
    if (found)
        return 0;

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
