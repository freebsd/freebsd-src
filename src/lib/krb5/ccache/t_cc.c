/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_cc.c */
/*
 * Copyright 2000 by the Massachusetts Institute of Technology.
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
#include "cc-int.h"
#include <stdio.h>
#include <stdlib.h>
#include "autoconf.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "com_err.h"

#define KRB5_OK 0

krb5_creds test_creds, test_creds2;

int debug=0;

static void
init_structs(void)
{
    static int add=0x12345;

    static krb5_address addr;

    static krb5_address *addrs[] = {
        &addr,
        0,
    };

    addr.magic = KV5M_ADDRESS;
    addr.addrtype = ADDRTYPE_INET;
    addr.length = 4;
    addr.contents = (krb5_octet *) &add;

    test_creds.magic = KV5M_CREDS;
    test_creds.client = NULL;
    test_creds.server = NULL;

    test_creds.keyblock.magic = KV5M_KEYBLOCK;
    test_creds.keyblock.contents = 0;
    test_creds.keyblock.enctype = 1;
    test_creds.keyblock.length = 1;
    test_creds.keyblock.contents = (unsigned char *) "1";
    test_creds.times.authtime = 1111;
    test_creds.times.starttime = 2222;
    test_creds.times.endtime = 3333;
    test_creds.times.renew_till = 4444;
    test_creds.is_skey = 1;
    test_creds.ticket_flags = 5555;
    test_creds.addresses = addrs;

#define SET_TICKET(ent, str) {ent.magic = KV5M_DATA; ent.length = sizeof(str); ent.data = str;}
    SET_TICKET(test_creds.ticket, "This is ticket 1");
    SET_TICKET(test_creds.second_ticket, "This is ticket 2");
    test_creds.authdata = NULL;
}

static krb5_error_code
init_test_cred(krb5_context context)
{
    krb5_error_code kret;
    unsigned int i;
    krb5_authdata *a;
#define REALM "REALM"
    kret = krb5_build_principal(context, &test_creds.client, sizeof(REALM), REALM,
                                "client-comp1", "client-comp2", NULL);
    if(kret)
        return kret;

    kret = krb5_build_principal(context, &test_creds.server, sizeof(REALM), REALM,
                                "server-comp1", "server-comp2", NULL);
    if(kret) {
        krb5_free_principal(context, test_creds.client);
        test_creds.client = 0;
        goto cleanup;
    }

    test_creds.authdata = malloc (3 * sizeof(krb5_authdata *));
    if (!test_creds.authdata) {
        kret = ENOMEM;
        goto cleanup;
    }

    for (i = 0 ; i <= 2 ; i++) {
        test_creds.authdata[i] = 0;
    }
    a = (krb5_authdata *) malloc(sizeof(krb5_authdata));
    if(!a) {
        kret = ENOMEM;
        goto cleanup;
    }
    a->magic = KV5M_AUTHDATA;
    a->ad_type = KRB5_AUTHDATA_IF_RELEVANT;
    a->contents = (krb5_octet * ) malloc(1);
    if(!a->contents) {
        free(a);
        kret = ENOMEM;
        goto cleanup;
    }
    a->contents[0]=5;
    a->length = 1;
    test_creds.authdata[0] = a;

    a = (krb5_authdata *) malloc(sizeof(krb5_authdata));
    if(!a) {
        kret = ENOMEM;
        goto cleanup;
    }
    a->magic = KV5M_AUTHDATA;
    a->ad_type = KRB5_AUTHDATA_KDC_ISSUED;
    a->contents = (krb5_octet * ) malloc(2);
    if(!a->contents) {
        free(a);
        kret = ENOMEM;
        goto cleanup;
    }
    a->contents[0]=4;
    a->contents[1]=6;
    a->length = 2;
    test_creds.authdata[1] = a;

    memcpy(&test_creds2, &test_creds, sizeof(test_creds));
    kret = krb5_build_principal(context, &test_creds2.server, sizeof(REALM),
                                REALM, "server-comp1", "server-comp3", NULL);

cleanup:
    if(kret) {
        if (test_creds.client) {
            krb5_free_principal(context, test_creds.client);
            test_creds.client = 0;
        }
        if (test_creds.server) {
            krb5_free_principal(context, test_creds.server);
            test_creds.server = 0;

        }
        if (test_creds.authdata) {
            krb5_free_authdata(context, test_creds.authdata);
            test_creds.authdata = 0;
        }
    }

    return kret;
}

static void
free_test_cred(krb5_context context)
{
    krb5_free_principal(context, test_creds.client);

    krb5_free_principal(context, test_creds.server);
    krb5_free_principal(context, test_creds2.server);

    if(test_creds.authdata) {
        krb5_free_authdata(context, test_creds.authdata);
        test_creds.authdata = 0;
    }
}

#define CHECK(kret,msg)                                 \
    if (kret != KRB5_OK) {                              \
        com_err(msg, kret, "");                         \
        fflush(stderr);                                 \
        exit(1);                                        \
    } else if(debug) printf("%s went ok\n", msg);

#define CHECK_STR(str,msg)                              \
    if (str == 0) {                                     \
        com_err(msg, kret, "");                         \
        exit(1);                                        \
    } else if(debug) printf("%s went ok\n", msg);

#define CHECK_BOOL(expr,errstr,msg)                     \
    if (expr) {                                         \
        fprintf(stderr, "%s %s\n", msg, errstr);        \
        exit(1);                                        \
    } else if(debug) printf("%s went ok\n", msg);

#define CHECK_FAIL(experr, kret, msg)           \
    if (experr != kret) { CHECK(kret, msg);}

static void
check_num_entries(krb5_context context, krb5_ccache cache, int expected,
                  unsigned linenum)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_creds creds;
    int count = 0;

    ret = krb5_cc_start_seq_get(context, cache, &cursor);
    if (ret != 0) {
        com_err("", ret, "(on line %d) - krb5_cc_start_seq_get", linenum);
        fflush(stderr);
        exit(1);
    }

    while (1) {
        ret = krb5_cc_next_cred(context, cache, &cursor, &creds);
        if (ret)
            break;

        count++;
        krb5_free_cred_contents(context, &creds);
    }
    krb5_cc_end_seq_get(context, cache, &cursor);
    if (ret != KRB5_CC_END) {
        CHECK(ret, "counting entries in ccache");
    }

    if (count != expected) {
        com_err("", KRB5_FCC_INTERNAL,
                "(on line %d) - count didn't match (expected %d, got %d)",
                linenum, expected, count);
        fflush(stderr);
        exit(1);
    }
}

static void
cc_test(krb5_context context, const char *name, krb5_flags flags)
{
    krb5_ccache id, id2;
    krb5_creds creds;
    krb5_error_code kret;
    krb5_cc_cursor cursor;
    krb5_principal tmp;
    krb5_flags matchflags = KRB5_TC_MATCH_IS_SKEY;

    const char *c_name;
    char newcache[300];
    char *save_type;

    kret = init_test_cred(context);
    CHECK(kret, "init_creds");

    kret = krb5_cc_resolve(context, name, &id);
    CHECK(kret, "resolve");
    kret = krb5_cc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize");

    c_name = krb5_cc_get_name(context, id);
    CHECK_STR(c_name, "get_name");

    c_name = krb5_cc_get_type(context, id);
    CHECK_STR(c_name, "get_type");
    save_type=strdup(c_name);
    CHECK_STR(save_type, "copying type");

    kret = krb5_cc_store_cred(context, id, &test_creds);
    CHECK(kret, "store");

    kret = krb5_cc_get_principal(context, id, &tmp);
    CHECK(kret, "get_principal");

    CHECK_BOOL(krb5_realm_compare(context, tmp, test_creds.client) != TRUE,
               "realms do not match", "realm_compare");


    CHECK_BOOL(krb5_principal_compare(context, tmp, test_creds.client) != TRUE,
               "principals do not match", "principal_compare");

    krb5_free_principal(context, tmp);

    kret = krb5_cc_set_flags (context, id, flags);
    CHECK(kret, "set_flags");

    kret = krb5_cc_start_seq_get(context, id, &cursor);
    CHECK(kret, "start_seq_get");
    kret = 0;
    while (kret != KRB5_CC_END) {
        if(debug) printf("Calling next_cred\n");
        kret = krb5_cc_next_cred(context, id, &cursor, &creds);
        if(kret == KRB5_CC_END) {
            if(debug) printf("next_cred: ok at end\n");
        }
        else {
            CHECK(kret, "next_cred");
            krb5_free_cred_contents(context, &creds);
        }

    }
    kret = krb5_cc_end_seq_get(context, id, &cursor);
    CHECK(kret, "end_seq_get");

    kret = krb5_cc_close(context, id);
    CHECK(kret, "close");


    /* ------------------------------------------------- */
    kret = krb5_cc_resolve(context, name, &id);
    CHECK(kret, "resolve2");

    {
        /* Copy the cache test*/
        snprintf(newcache, sizeof(newcache), "%s.new", name);
        kret = krb5_cc_resolve(context, newcache, &id2);
        CHECK(kret, "resolve of new cache");

        /* This should fail as the new creds are not initialized */
        kret = krb5_cc_copy_creds(context, id, id2);
        CHECK_FAIL(KRB5_FCC_NOFILE, kret, "copy_creds");

        kret = krb5_cc_initialize(context, id2, test_creds.client);
        CHECK(kret, "initialize of id2");

        kret = krb5_cc_copy_creds(context, id, id2);
        CHECK(kret, "copy_creds");

        kret = krb5_cc_destroy(context, id2);
        CHECK(kret, "destroy new cache");
    }

    /* Destroy the first cache */
    kret = krb5_cc_destroy(context, id);
    CHECK(kret, "destroy");

    /* ----------------------------------------------------- */
    /* Tests the generate new code */
    kret = krb5_cc_new_unique(context, save_type,
                              NULL, &id2);
    CHECK(kret, "new_unique");

    kret = krb5_cc_initialize(context, id2, test_creds.client);
    CHECK(kret, "initialize");

    kret = krb5_cc_store_cred(context, id2, &test_creds);
    CHECK(kret, "store");

    kret = krb5_cc_destroy(context, id2);
    CHECK(kret, "destroy id2");

    /* ----------------------------------------------------- */
    /* Test credential removal */
    kret = krb5_cc_resolve(context, name, &id);
    CHECK(kret, "resolving for remove");

    kret = krb5_cc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize for remove");
    check_num_entries(context, id, 0, __LINE__);

    kret = krb5_cc_store_cred(context, id, &test_creds);
    CHECK(kret, "store for remove (first pass)");
    check_num_entries(context, id, 1, __LINE__); /* 1 */

    kret = krb5_cc_remove_cred(context, id, matchflags, &test_creds);
    CHECK(kret, "removing credential (first pass)");
    check_num_entries(context, id, 0, __LINE__); /* empty */

    kret = krb5_cc_store_cred(context, id, &test_creds);
    CHECK(kret, "first store for remove (second pass)");
    check_num_entries(context, id, 1, __LINE__); /* 1 */

    kret = krb5_cc_store_cred(context, id, &test_creds2);
    CHECK(kret, "second store for remove (second pass)");
    check_num_entries(context, id, 2, __LINE__); /* 1, 2 */

    kret = krb5_cc_remove_cred(context, id, matchflags, &test_creds2);
    CHECK(kret, "first remove (second pass)");
    check_num_entries(context, id, 1, __LINE__); /* 1 */

    kret = krb5_cc_store_cred(context, id, &test_creds2);
    CHECK(kret, "third store for remove (second pass)");
    check_num_entries(context, id, 2, __LINE__); /* 1, 2 */

    kret = krb5_cc_remove_cred(context, id, matchflags, &test_creds);
    CHECK(kret, "second remove (second pass)");
    check_num_entries(context, id, 1, __LINE__); /* 2 */

    kret = krb5_cc_remove_cred(context, id, matchflags, &test_creds2);
    CHECK(kret, "third remove (second pass)");
    check_num_entries(context, id, 0, __LINE__); /* empty */

    kret = krb5_cc_destroy(context, id);
    CHECK(kret, "destruction for remove");

    /* Test removal with iteration. */
    kret = krb5_cc_resolve(context, name, &id);
    CHECK(kret, "resolving for remove-iter");

    kret = krb5_cc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize for remove-iter");

    kret = krb5_cc_store_cred(context, id, &test_creds);
    CHECK(kret, "first store for remove-iter");

    kret = krb5_cc_store_cred(context, id, &test_creds2);
    CHECK(kret, "second store for remove-iter");

    kret = krb5_cc_start_seq_get(context, id, &cursor);
    CHECK(kret, "start_seq_get for remove-iter");

    kret = krb5_cc_remove_cred(context, id, matchflags, &test_creds);
    CHECK(kret, "remove for remove-iter");

    while (1) {
        /* The removed credential may or may not be present in the cache -
         * either behavior is technically correct. */
        kret = krb5_cc_next_cred(context, id, &cursor, &creds);
        if (kret == KRB5_CC_END)
            break;
        CHECK(kret, "next_cred for remove-iter: %s");

        CHECK(creds.times.endtime == 0, "no-lifetime cred");

        krb5_free_cred_contents(context, &creds);
    }

    kret = krb5_cc_end_seq_get(context, id, &cursor);
    CHECK(kret, "end_seq_get for remove-iter");

    kret = krb5_cc_destroy(context, id);
    CHECK(kret, "destruction for remove-iter");

    free(save_type);
    free_test_cred(context);
}

/*
 * Checks if a credential type is registered with the library
 */
static int
check_registered(krb5_context context, const char *prefix)
{
    char name[300];
    krb5_error_code kret;
    krb5_ccache id;

    snprintf(name, sizeof(name), "%s/tmp/cctest.%ld", prefix, (long) getpid());

    kret = krb5_cc_resolve(context, name, &id);
    if(kret != KRB5_OK) {
        if(kret == KRB5_CC_UNKNOWN_TYPE)
            return 0;
        com_err("Checking on credential type", kret, "%s", prefix);
        fflush(stderr);
        return 0;
    }

    kret = krb5_cc_close(context, id);
    if(kret != KRB5_OK) {
        com_err("Checking on credential type - closing", kret, "%s", prefix);
        fflush(stderr);
    }

    return 1;
}


static void
do_test(krb5_context context, const char *prefix)
{
    char name[300];

    snprintf(name, sizeof(name), "%s/tmp/cctest.%ld", prefix, (long) getpid());
    printf("Starting test on %s\n", name);
    cc_test (context, name, 0);
    cc_test (context, name, !0);
    printf("Test on %s passed\n", name);
}

static void
test_misc(krb5_context context)
{
    /* Tests for certain error returns */
    krb5_error_code       kret;
    krb5_ccache id;
    const krb5_cc_ops *ops_save;

    fprintf(stderr, "Testing miscellaneous error conditions\n");

    kret = krb5_cc_resolve(context, "unknown_method_ep:/tmp/name", &id);
    if (kret != KRB5_CC_UNKNOWN_TYPE) {
        CHECK(kret, "resolve unknown type");
    }

    /* Test for not specifying a cache type with no defaults */
    ops_save = krb5_cc_dfl_ops;
    krb5_cc_dfl_ops = 0;

    kret = krb5_cc_resolve(context, "/tmp/e", &id);
    if (kret != KRB5_CC_BADNAME) {
        CHECK(kret, "resolve no builtin type");
    }

    krb5_cc_dfl_ops = ops_save;

}

/*
 * Regression tests for #8202.  Because memory ccaches share objects between
 * different handles to the same cache and between iterators and caches,
 * historically there have been some bugs when those objects are released.
 */
static void
test_memory_concurrent(krb5_context context)
{
    krb5_error_code kret;
    krb5_ccache id1, id2;
    krb5_cc_cursor cursor;
    krb5_creds creds;

    /* Create two handles to the same memory ccache and destroy them. */
    kret = krb5_cc_resolve(context, "MEMORY:x", &id1);
    CHECK(kret, "resolve 1");
    kret = krb5_cc_resolve(context, "MEMORY:x", &id2);
    CHECK(kret, "resolve 2");
    kret = krb5_cc_destroy(context, id1);
    CHECK(kret, "destroy 1");
    kret = krb5_cc_destroy(context, id2);
    CHECK(kret, "destroy 2");

    kret = init_test_cred(context);
    CHECK(kret, "init_creds");

    /* Reinitialize the cache after creating an iterator for it, and verify
     * that the iterator ends gracefully. */
    kret = krb5_cc_resolve(context, "MEMORY:x", &id1);
    CHECK(kret, "resolve");
    kret = krb5_cc_initialize(context, id1, test_creds.client);
    CHECK(kret, "initialize");
    kret = krb5_cc_store_cred(context, id1, &test_creds);
    CHECK(kret, "store");
    kret = krb5_cc_start_seq_get(context, id1, &cursor);
    CHECK(kret, "start_seq_get");
    kret = krb5_cc_initialize(context, id1, test_creds.client);
    CHECK(kret, "initialize again");
    kret = krb5_cc_next_cred(context, id1, &cursor, &creds);
    CHECK_BOOL(kret != KRB5_CC_END, "iterator should end", "next_cred");
    kret = krb5_cc_end_seq_get(context, id1, &cursor);
    CHECK(kret, "end_seq_get");
    kret = krb5_cc_destroy(context, id1);
    CHECK(kret, "destroy");

    free_test_cred(context);
}

/* Check that order is preserved during iteration.  Not all cache types have
 * this property. */
static void
test_order(krb5_context context, const char *name)
{
    krb5_error_code kret;
    krb5_ccache id;
    krb5_cc_cursor cursor;
    krb5_creds creds;

    kret = init_test_cred(context);
    CHECK(kret, "init_creds");

    kret = krb5_cc_resolve(context, name, &id);
    CHECK(kret, "resolve");
    kret = krb5_cc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize");
    kret = krb5_cc_store_cred(context, id, &test_creds);
    CHECK(kret, "store 1");
    kret = krb5_cc_store_cred(context, id, &test_creds2);
    CHECK(kret, "store 2");

    kret = krb5_cc_start_seq_get(context, id, &cursor);
    CHECK(kret, "start_seq_get");
    kret = krb5_cc_next_cred(context, id, &cursor, &creds);
    CHECK(kret, "next_cred 1");
    CHECK_BOOL(krb5_principal_compare(context, creds.server,
                                      test_creds.server) != TRUE,
               "first cred does not match", "principal_compare");
    krb5_free_cred_contents(context, &creds);

    kret = krb5_cc_next_cred(context, id, &cursor, &creds);
    CHECK(kret, "next_cred 2");
    CHECK_BOOL(krb5_principal_compare(context, creds.server,
                                      test_creds2.server) != TRUE,
               "second cred does not match", "principal_compare");
    krb5_free_cred_contents(context, &creds);

    krb5_cc_end_seq_get(context, id, &cursor);
    krb5_cc_close(context, id);
    free_test_cred(context);
}

extern const krb5_cc_ops krb5_mcc_ops;
extern const krb5_cc_ops krb5_fcc_ops;

int
main(void)
{
    krb5_context context;
    krb5_error_code     kret;

    if ((kret = krb5_init_context(&context))) {
        printf("Couldn't initialize krb5 library: %s\n",
               error_message(kret));
        exit(1);
    }

    kret = krb5_cc_register(context, &krb5_mcc_ops,0);
    if(kret && kret != KRB5_CC_TYPE_EXISTS) {
        CHECK(kret, "register_mem");
    }

    kret = krb5_cc_register(context, &krb5_fcc_ops,0);
    if(kret && kret != KRB5_CC_TYPE_EXISTS) {
        CHECK(kret, "register_mem");
    }

    /* Registering a second time tests for error return */
    kret = krb5_cc_register(context, &krb5_fcc_ops,0);
    if(kret != KRB5_CC_TYPE_EXISTS) {
        CHECK(kret, "register_mem");
    }

    /* Registering with override should work */
    kret = krb5_cc_register(context, &krb5_fcc_ops,1);
    CHECK(kret, "register_mem override");

    init_structs();

    test_misc(context);
    do_test(context, "");

    if (check_registered(context, "KEYRING:process:"))
        do_test(context, "KEYRING:process:");
    else
        printf("Skipping KEYRING: test - unregistered type\n");

    do_test(context, "MEMORY:");
    do_test(context, "FILE:");

    test_memory_concurrent(context);

    test_order(context, "MEMORY:order");

    krb5_free_context(context);
    return 0;
}
