/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_cccol.py - Test ccache collection via API */
/*
 * Copyright (C) 2013 by the Massachusetts Institute of Technology.
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

#include <krb5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static krb5_context ctx;

/* Check that code is 0.  Display an error message first if it is not. */
static void
check(krb5_error_code code)
{
    const char *errmsg;

    if (code != 0) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s\n", errmsg);
        krb5_free_error_message(ctx, errmsg);
    }
    assert(code == 0);
}

/* Construct a list of the names of each credential cache in the collection. */
static void
get_collection_names(char ***list_out, size_t *count_out)
{
    krb5_cccol_cursor cursor;
    krb5_ccache cache;
    char **list = NULL;
    size_t count = 0;
    char *name;

    check(krb5_cccol_cursor_new(ctx, &cursor));
    while (1) {
        check(krb5_cccol_cursor_next(ctx, cursor, &cache));
        if (cache == NULL)
            break;
        check(krb5_cc_get_full_name(ctx, cache, &name));
        krb5_cc_close(ctx, cache);
        list = realloc(list, (count + 1) * sizeof(*list));
        assert(list != NULL);
        list[count++] = name;
    }
    krb5_cccol_cursor_free(ctx, &cursor);
    *list_out = list;
    *count_out = count;
}

/* Return true if list contains name. */
static krb5_boolean
in_list(char **list, size_t count, const char *name)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (strcmp(list[i], name) == 0)
            return TRUE;
    }
    return FALSE;
}

/* Release the memory for a list of credential cache names. */
static void
free_list(char **list, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        krb5_free_string(ctx, list[i]);
    free(list);
}

/*
 * Check that the cache names within the current collection begin with first
 * (unless first is NULL), that the other elements match the remaining
 * arguments in some order.  others must be the number of additional cache
 * names.
 */
static void
check_collection(const char *first, size_t others, ...)
{
    va_list ap;
    char **list;
    size_t count, i;
    const char *name;

    get_collection_names(&list, &count);
    if (first != NULL) {
        assert(strcmp(first, list[0]) == 0);
        assert(count == others + 1);
    } else {
        assert(count == others);
    }
    va_start(ap, others);
    for (i = 0; i < others; i++) {
        name = va_arg(ap, const char *);
        assert(in_list(list, count, name));
    }
    va_end(ap);
    free_list(list, count);
}

/* Check that the name of cache matches expected_name. */
static void
check_name(krb5_ccache cache, const char *expected_name)
{
    char *name;

    check(krb5_cc_get_full_name(ctx, cache, &name));
    assert(strcmp(name, expected_name) == 0);
    krb5_free_string(ctx, name);
}

/* Check that when collection_name is resolved, the resulting cache's name
 * matches expected_name. */
static void
check_primary_name(const char *collection_name, const char *expected_name)
{
    krb5_ccache cache;

    check(krb5_cc_resolve(ctx, collection_name, &cache));
    check_name(cache, expected_name);
    krb5_cc_close(ctx, cache);
}

/* Check that when name is resolved, the resulting cache's principal matches
 * expected_princ, or has no principal if expected_princ is NULL. */
static void
check_princ(const char *name, krb5_principal expected_princ)
{
    krb5_ccache cache;
    krb5_principal princ;

    check(krb5_cc_resolve(ctx, name, &cache));
    if (expected_princ != NULL) {
        check(krb5_cc_get_principal(ctx, cache, &princ));
        assert(krb5_principal_compare(ctx, princ, expected_princ));
        krb5_free_principal(ctx, princ);
    } else {
        assert(krb5_cc_get_principal(ctx, cache, &princ) != 0);
    }
    krb5_cc_close(ctx, cache);
}

/* Check that krb5_cc_cache_match on princ returns a cache whose name matches
 * expected_name, or that the match fails if expected_name is NULL. */
static void
check_match(krb5_principal princ, const char *expected_name)
{
    krb5_ccache cache;

    if (expected_name != NULL) {
        check(krb5_cc_cache_match(ctx, princ, &cache));
        check_name(cache, expected_name);
        krb5_cc_close(ctx, cache);
    } else {
        assert(krb5_cc_cache_match(ctx, princ, &cache) != 0);
    }
}

int
main(int argc, char **argv)
{
    krb5_ccache ccinitial, ccu1, ccu2;
    krb5_principal princ1, princ2, princ3;
    const char *collection_name, *typename;
    char *initial_primary_name, *unique1_name, *unique2_name;

    /*
     * Get the collection name from the command line.  This is a ccache name
     * with collection semantics, like DIR:/path/to/directory.  This test
     * program assumes that the collection is empty to start with.
     */
    assert(argc == 2);
    collection_name = argv[1];

    /*
     * Set the default ccache for the context to be the collection name, so the
     * library can find the collection.
     */
    check(krb5_init_context(&ctx));
    check(krb5_cc_set_default_name(ctx, collection_name));

    /*
     * Resolve the collection name.  Since the collection is empty, this should
     * generate a subsidiary name of an uninitialized cache.  Getting the name
     * of the resulting cache should give us the subsidiary name, not the
     * collection name.  This resulting subsidiary name should be consistent if
     * we resolve the collection name again, and the collection should still be
     * empty since we haven't initialized the cache.
     */
    check(krb5_cc_resolve(ctx, collection_name, &ccinitial));
    check(krb5_cc_get_full_name(ctx, ccinitial, &initial_primary_name));
    assert(strcmp(initial_primary_name, collection_name) != 0);
    check_primary_name(collection_name, initial_primary_name);
    check_collection(NULL, 0);
    check_princ(collection_name, NULL);
    check_princ(initial_primary_name, NULL);

    /*
     * Before initializing the primary ccache, generate and initialize two
     * unique caches of the collection's type.  Check that the cache names
     * resolve to the generated caches and appear in the collection.  (They
     * might appear before being initialized; that's not currently considered
     * important).  The primary cache for the collection should remain as the
     * uninitialized cache from the previous step.
     */
    typename = krb5_cc_get_type(ctx, ccinitial);
    check(krb5_cc_new_unique(ctx, typename, NULL, &ccu1));
    check(krb5_cc_get_full_name(ctx, ccu1, &unique1_name));
    check(krb5_parse_name(ctx, "princ1@X", &princ1));
    check(krb5_cc_initialize(ctx, ccu1, princ1));
    check_princ(unique1_name, princ1);
    check_match(princ1, unique1_name);
    check_collection(NULL, 1, unique1_name);
    check(krb5_cc_new_unique(ctx, typename, NULL, &ccu2));
    check(krb5_cc_get_full_name(ctx, ccu2, &unique2_name));
    check(krb5_parse_name(ctx, "princ2@X", &princ2));
    check(krb5_cc_initialize(ctx, ccu2, princ2));
    check_princ(unique2_name, princ2);
    check_match(princ1, unique1_name);
    check_match(princ2, unique2_name);
    check_collection(NULL, 2, unique1_name, unique2_name);
    assert(strcmp(unique1_name, initial_primary_name) != 0);
    assert(strcmp(unique1_name, collection_name) != 0);
    assert(strcmp(unique2_name, initial_primary_name) != 0);
    assert(strcmp(unique2_name, collection_name) != 0);
    assert(strcmp(unique2_name, unique1_name) != 0);
    check_primary_name(collection_name, initial_primary_name);

    /*
     * Initialize the initial primary cache.  Make sure it didn't change names,
     * that the previously retrieved name and the collection name both resolve
     * to the initialized cache, and that it now appears first in the
     * collection.
     */
    check(krb5_parse_name(ctx, "princ3@X", &princ3));
    check(krb5_cc_initialize(ctx, ccinitial, princ3));
    check_name(ccinitial, initial_primary_name);
    check_princ(initial_primary_name, princ3);
    check_princ(collection_name, princ3);
    check_match(princ3, initial_primary_name);
    check_collection(initial_primary_name, 2, unique1_name, unique2_name);

    /*
     * Switch the primary cache to each cache we have open.  One each switch,
     * check the primary name, check that the collection resolves to the
     * expected cache, and check that the new primary name appears first in the
     * collection.
     */
    check(krb5_cc_switch(ctx, ccu1));
    check_primary_name(collection_name, unique1_name);
    check_princ(collection_name, princ1);
    check_collection(unique1_name, 2, initial_primary_name, unique2_name);
    check(krb5_cc_switch(ctx, ccu2));
    check_primary_name(collection_name, unique2_name);
    check_princ(collection_name, princ2);
    check_collection(unique2_name, 2, initial_primary_name, unique1_name);
    check(krb5_cc_switch(ctx, ccinitial));
    check_primary_name(collection_name, initial_primary_name);
    check_princ(collection_name, princ3);
    check_collection(initial_primary_name, 2, unique1_name, unique2_name);

    /*
     * Temporarily set the context default ccache to a subsidiary name, and
     * check that iterating over the collection yields that subsidiary cache
     * and no others.
     */
    check(krb5_cc_set_default_name(ctx, unique1_name));
    check_collection(unique1_name, 0);
    check(krb5_cc_set_default_name(ctx, collection_name));

    /*
     * Destroy the primary cache.  Make sure this causes both the initial
     * primary name and the collection name to resolve to an uninitialized
     * cache.  Make sure the primary name doesn't change and doesn't appear in
     * the collection any more.
     */
    check(krb5_cc_destroy(ctx, ccinitial));
    check_princ(initial_primary_name, NULL);
    check_princ(collection_name, NULL);
    check_primary_name(collection_name, initial_primary_name);
    check_match(princ1, unique1_name);
    check_match(princ2, unique2_name);
    check_match(princ3, NULL);
    check_collection(NULL, 2, unique1_name, unique2_name);

    /*
     * Switch to the first unique cache after destroying the primary cache.
     * Check that the collection name resolves to this cache and that the new
     * primary name appears first in the collection.
     */
    check(krb5_cc_switch(ctx, ccu1));
    check_primary_name(collection_name, unique1_name);
    check_princ(collection_name, princ1);
    check_collection(unique1_name, 1, unique2_name);

    /*
     * Destroy the second unique cache (which is not the current primary),
     * check that it is on longer initialized, and check that it no longer
     * appears in the collection.  Check that destroying the non-primary cache
     * doesn't affect the primary name.
     */
    check(krb5_cc_destroy(ctx, ccu2));
    check_princ(unique2_name, NULL);
    check_match(princ2, NULL);
    check_collection(unique1_name, 0);
    check_primary_name(collection_name, unique1_name);
    check_match(princ1, unique1_name);
    check_princ(collection_name, princ1);

    /*
     * Destroy the first unique cache.  Check that the collection is empty and
     * still has the same primary name.
     */
    check(krb5_cc_destroy(ctx, ccu1));
    check_princ(unique1_name, NULL);
    check_princ(collection_name, NULL);
    check_primary_name(collection_name, unique1_name);
    check_match(princ1, NULL);
    check_collection(NULL, 0);

    krb5_free_string(ctx, initial_primary_name);
    krb5_free_string(ctx, unique1_name);
    krb5_free_string(ctx, unique2_name);
    krb5_free_principal(ctx, princ1);
    krb5_free_principal(ctx, princ2);
    krb5_free_principal(ctx, princ3);
    krb5_free_context(ctx);
    return 0;
}
