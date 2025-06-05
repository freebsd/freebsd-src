/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/ktbase.c - Registration functions for keytab */
/*
 * Copyright 1990,2008 by the Massachusetts Institute of Technology.
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
 * Copyright 2007 by Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "k5-int.h"
#include "k5-thread.h"
#include "kt-int.h"

#ifndef LEAN_CLIENT

extern const krb5_kt_ops krb5_ktf_ops;
extern const krb5_kt_ops krb5_ktf_writable_ops;
extern const krb5_kt_ops krb5_mkt_ops;

struct krb5_kt_typelist {
    const krb5_kt_ops *ops;
    const struct krb5_kt_typelist *next;
};
const static struct krb5_kt_typelist krb5_kt_typelist_memory = {
    &krb5_mkt_ops,
    NULL
};
const static struct krb5_kt_typelist krb5_kt_typelist_wrfile  = {
    &krb5_ktf_writable_ops,
    &krb5_kt_typelist_memory
};
const static struct krb5_kt_typelist krb5_kt_typelist_file  = {
    &krb5_ktf_ops,
    &krb5_kt_typelist_wrfile
};

static const struct krb5_kt_typelist *kt_typehead = &krb5_kt_typelist_file;
/* Lock for protecting the type list.  */
static k5_mutex_t kt_typehead_lock = K5_MUTEX_PARTIAL_INITIALIZER;

int krb5int_kt_initialize(void)
{
    int err;

    err = k5_mutex_finish_init(&kt_typehead_lock);
    if (err)
        goto done;
    err = krb5int_mkt_initialize();
    if (err)
        goto done;

done:
    return(err);
}

void
krb5int_kt_finalize(void)
{
    const struct krb5_kt_typelist *t, *t_next;

    k5_mutex_destroy(&kt_typehead_lock);
    for (t = kt_typehead; t != &krb5_kt_typelist_file; t = t_next) {
        t_next = t->next;
        free((struct krb5_kt_typelist *)t);
    }

    krb5int_mkt_finalize();
}


/*
 * Register a new key table type
 * don't replace if it already exists; return an error instead.
 */

krb5_error_code KRB5_CALLCONV
krb5_kt_register(krb5_context context, const krb5_kt_ops *ops)
{
    const struct krb5_kt_typelist *t;
    struct krb5_kt_typelist *newt;

    k5_mutex_lock(&kt_typehead_lock);
    for (t = kt_typehead; t && strcmp(t->ops->prefix,ops->prefix);t = t->next)
        ;
    if (t) {
        k5_mutex_unlock(&kt_typehead_lock);
        return KRB5_KT_TYPE_EXISTS;
    }
    if (!(newt = (struct krb5_kt_typelist *) malloc(sizeof(*t)))) {
        k5_mutex_unlock(&kt_typehead_lock);
        return ENOMEM;
    }
    newt->next = kt_typehead;
    newt->ops = ops;
    kt_typehead = newt;
    k5_mutex_unlock(&kt_typehead_lock);
    return 0;
}

/*
 * Resolve a key table name into a keytab object.
 *
 * The name is currently constrained to be of the form "type:residual";
 *
 * The "type" portion corresponds to one of the registered key table
 * types, while the "residual" portion is specific to the
 * particular keytab type.
 */

#include <ctype.h>
krb5_error_code KRB5_CALLCONV
krb5_kt_resolve (krb5_context context, const char *name, krb5_keytab *ktid)
{
    const struct krb5_kt_typelist *tlist;
    char *pfx = NULL;
    unsigned int pfxlen;
    const char *cp, *resid;
    krb5_error_code err = 0;
    krb5_keytab id;

    *ktid = NULL;

    cp = strchr (name, ':');
    if (!cp)
        return (*krb5_kt_dfl_ops.resolve)(context, name, ktid);

    pfxlen = cp - name;

    if ( pfxlen == 1 && isalpha((unsigned char) name[0]) ) {
        /* We found a drive letter not a prefix - use FILE */
        pfx = strdup("FILE");
        if (!pfx)
            return ENOMEM;

        resid = name;
    } else if (name[0] == '/') {
        pfx = strdup("FILE");
        if (!pfx)
            return ENOMEM;
        resid = name;
    } else {
        resid = name + pfxlen + 1;
        pfx = k5memdup0(name, pfxlen, &err);
        if (pfx == NULL)
            return err;
    }

    *ktid = (krb5_keytab) 0;

    k5_mutex_lock(&kt_typehead_lock);
    tlist = kt_typehead;
    /* Don't need to hold the lock, since entries are never modified
       or removed once they're in the list.  Just need to protect
       access to the list head variable itself.  */
    k5_mutex_unlock(&kt_typehead_lock);
    for (; tlist; tlist = tlist->next) {
        if (strcmp (tlist->ops->prefix, pfx) == 0) {
            err = (*tlist->ops->resolve)(context, resid, &id);
            if (!err)
                *ktid = id;
            goto cleanup;
        }
    }
    err = KRB5_KT_UNKNOWN_TYPE;

cleanup:
    free(pfx);
    return err;
}

krb5_error_code KRB5_CALLCONV
krb5_kt_dup(krb5_context context, krb5_keytab in, krb5_keytab *out)
{
    krb5_error_code err;
    char name[BUFSIZ];

    err = in->ops->get_name(context, in, name, sizeof(name));
    return err ? err : krb5_kt_resolve(context, name, out);
}

#endif /* LEAN_CLIENT */
