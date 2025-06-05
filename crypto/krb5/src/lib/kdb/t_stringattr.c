/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kdb/t_strings.c - Test program for KDB string attribute functions */
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
#include <kdb.h>

/*
 * This program exercises the KDB entry APIs for string attributes.  It relies
 * on some implementation knowledge:
 *
 * - The APIs will work on a DB entry initialized to all zeros (because it has
 *   an empty tl_data list).
 *
 * - Attribute order in krb5_dbe_get_strings matches attribute insertion order.
 */

int
main()
{
    krb5_db_entry *ent;
    krb5_context context;
    krb5_string_attr *strings;
    char *val;
    int count;

    assert(krb5int_init_context_kdc(&context) == 0);

    /* Start with an empty entry. */
    ent = calloc(1, sizeof(*ent));
    if (ent == NULL) {
        fprintf(stderr, "Can't allocate memory for entry.\n");
        return 1;
    }

    /* Check that the entry has no strings to start. */
    assert(krb5_dbe_get_strings(context, ent, &strings, &count) == 0);
    assert(strings == NULL && count == 0);
    krb5_dbe_free_strings(context, strings, count);

    /* Check that we get a null value querying a specific attribute. */
    assert(krb5_dbe_get_string(context, ent, "foo", &val) == 0);
    assert(val == NULL);

    /* Set some attributes one at a time, including a deletion. */
    assert(krb5_dbe_set_string(context, ent, "eggs", "dozen") == 0);
    assert(krb5_dbe_set_string(context, ent, "price", "right") == 0);
    assert(krb5_dbe_set_string(context, ent, "eggs", NULL) == 0);
    assert(krb5_dbe_set_string(context, ent, "time", "flies") == 0);

    /* Query each attribute. */
    assert(krb5_dbe_get_string(context, ent, "price", &val) == 0);
    assert(strcmp(val, "right") == 0);
    krb5_dbe_free_string(context, val);
    assert(krb5_dbe_get_string(context, ent, "time", &val) == 0);
    assert(strcmp(val, "flies") == 0);
    krb5_dbe_free_string(context, val);
    assert(krb5_dbe_get_string(context, ent, "eggs", &val) == 0);
    assert(val == NULL);

    /* Query the list of attributes and verify it. */
    assert(krb5_dbe_get_strings(context, ent, &strings, &count) == 0);
    assert(count == 2);
    assert(strcmp(strings[0].key, "price") == 0);
    assert(strcmp(strings[0].value, "right") == 0);
    assert(strcmp(strings[1].key, "time") == 0);
    assert(strcmp(strings[1].value, "flies") == 0);
    krb5_dbe_free_strings(context, strings, count);

    krb5_db_free_principal(context, ent);
    krb5_free_context(context);
    return 0;
}
