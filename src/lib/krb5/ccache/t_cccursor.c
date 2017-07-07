/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_cccursor.c - Simple test harness for cccol API */
/*
 * Copyright 2011 by the Massachusetts Institute of Technology.
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
 * Displays a list of caches returned by the cccol cursor.  The first argument,
 * if given, is set to the default cache name for the context before iterating.
 * Any remaining argments are resolved as caches and kept open during the
 * iteration.  If the argument "CONTENT" is given as one of the cache names,
 * immediately exit with status 0 if the collection contains credentials and 1
 * if it does not.
 */

#include "k5-int.h"

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context ctx;
    krb5_cccol_cursor cursor;
    krb5_ccache cache, hold[64];
    int i;
    char *name;

    assert(krb5_init_context(&ctx) == 0);
    if (argc > 1)
        assert(krb5_cc_set_default_name(ctx, argv[1]) == 0);

    if (argc > 2) {
        assert(argc < 60);
        for (i = 2; i < argc; i++) {
            if (strcmp(argv[i], "CONTENT") == 0) {
                ret = krb5_cccol_have_content(ctx);
                krb5_free_context(ctx);
                return ret != 0;
            }
            assert(krb5_cc_resolve(ctx, argv[i], &hold[i - 2]) == 0);
        }
    }

    assert(krb5_cccol_cursor_new(ctx, &cursor) == 0);
    while (1) {
        assert(krb5_cccol_cursor_next(ctx, cursor, &cache) == 0);
        if (cache == NULL)
            break;
        assert(krb5_cc_get_full_name(ctx, cache, &name) == 0);
        printf("%s\n", name);
        krb5_free_string(ctx, name);
        krb5_cc_close(ctx, cache);
    }
    assert(krb5_cccol_cursor_free(ctx, &cursor) == 0);

    for (i = 2; i < argc; i++)
        krb5_cc_close(ctx, hold[i - 2]);

    krb5_free_context(ctx);
    return 0;
}
