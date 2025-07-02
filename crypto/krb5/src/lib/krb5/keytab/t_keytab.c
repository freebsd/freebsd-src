/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/keytab/t_keytab.c - Tests for keytab interface */
/*
 * Copyright (C) 2007 by the Massachusetts Institute of Technology.
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
#include "autoconf.h"
#include <stdio.h>
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>


int debug=0;

extern const krb5_kt_ops krb5_ktf_writable_ops;

#define KRB5_OK 0

#define CHECK_ERR(kret,err,msg)                         \
    if (kret != err) {                                  \
        com_err(msg, kret, "");                         \
        fflush(stderr);                                 \
        exit(1);                                        \
    } else if(debug) printf("%s went ok\n", msg);

#define CHECK(kret,msg) CHECK_ERR(kret, 0, msg)

#define CHECK_STR(str,msg)                              \
    if (str == 0) {                                     \
        com_err(msg, kret, "");                         \
        exit(1);                                        \
    } else if(debug) printf("%s went ok\n", msg);

static void
test_misc(krb5_context context)
{
    /* Tests for certain error returns */
    krb5_error_code       kret;
    krb5_keytab ktid;
    char defname[BUFSIZ];
    char *name;

    fprintf(stderr, "Testing miscellaneous error conditions\n");

    kret = krb5_kt_resolve(context, "unknown_method_ep:/tmp/name", &ktid);
    CHECK_ERR(kret, KRB5_KT_UNKNOWN_TYPE, "resolve unknown type");

    /* Test length limits on krb5_kt_default_name */
    kret = krb5_kt_default_name(context, defname, sizeof(defname));
    CHECK(kret, "krb5_kt_default_name error");

    /* Now allocate space - without the null... */
    name = malloc(strlen(defname));
    if(!name) {
        fprintf(stderr, "Out of memory in testing\n");
        exit(1);
    }
    kret = krb5_kt_default_name(context, name, strlen(defname));
    free(name);
    CHECK_ERR(kret, KRB5_CONFIG_NOTENUFSPACE, "krb5_kt_default_name limited");
}

static void
kt_test(krb5_context context, const char *name)
{
    krb5_error_code kret;
    krb5_keytab kt;
    const char *type;
    char buf[BUFSIZ];
    char *p;
    krb5_keytab_entry kent, kent2;
    krb5_principal princ;
    krb5_kt_cursor cursor, cursor2;
    int cnt;
    krb5_enctype e1 = ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        e2 = ENCTYPE_AES256_CTS_HMAC_SHA384_192;

    kret = krb5_kt_resolve(context, name, &kt);
    CHECK(kret, "resolve");

    type = krb5_kt_get_type(context, kt);
    CHECK_STR(type, "getting kt type");
    printf("  Type is: %s\n", type);

    kret = krb5_kt_get_name(context, kt, buf, sizeof(buf));
    CHECK(kret, "get_name");
    printf("  Name is: %s\n", buf);

    /* Check that length checks fail */
    /* The buffer is allocated too small - to allow for valgrind test of
       overflows
    */
    p = malloc(strlen(buf));
    kret = krb5_kt_get_name(context, kt, p, 1);
    CHECK_ERR(kret, KRB5_KT_NAME_TOOLONG, "get_name - size 1");


    kret = krb5_kt_get_name(context, kt, p, strlen(buf));
    CHECK_ERR(kret, KRB5_KT_NAME_TOOLONG, "get_name");
    free(p);

    /* Try to lookup unknown principal - when keytab does not exist*/
    kret = krb5_parse_name(context, "test/test2@TEST.MIT.EDU", &princ);
    CHECK(kret, "parsing principal");

    /* This will return ENOENT for FILE because the file doesn't exist,
     * so accept that or KRB5_KT_NOTFOUND. */
    kret = krb5_kt_get_entry(context, kt, princ, 0, 0, &kent);
    if (kret != ENOENT) {
        CHECK_ERR(kret, KRB5_KT_NOTFOUND, "Getting non-existent entry");
    }

    kret = krb5_kt_have_content(context, kt);
    CHECK_ERR(kret, KRB5_KT_NOTFOUND, "Checking for keytab content (empty)");


    /* ===================   Add entries to keytab ================= */
    /*
     * Add the following for this principal
     * enctype e1, kvno 1, key = "1"
     * enctype e2, kvno 1, key = "1"
     * enctype e1, kvno 2, key = "2"
     */
    memset(&kent, 0, sizeof(kent));
    kent.magic = KV5M_KEYTAB_ENTRY;
    kent.principal = princ;
    kent.timestamp = 327689;
    kent.vno = 1;
    kent.key.magic = KV5M_KEYBLOCK;
    kent.key.enctype = e1;
    kent.key.length = 1;
    kent.key.contents = (krb5_octet *) "1";


    kret = krb5_kt_add_entry(context, kt, &kent);
    CHECK(kret, "Adding initial entry");

    kent.key.enctype = e2;
    kret = krb5_kt_add_entry(context, kt, &kent);
    CHECK(kret, "Adding second entry");

    kent.key.enctype = e1;
    kent.vno = 2;
    kent.key.contents = (krb5_octet *) "2";
    kret = krb5_kt_add_entry(context, kt, &kent);
    CHECK(kret, "Adding third entry");

    /* Free memory */
    krb5_free_principal(context, princ);

    /* ==============   Test iterating over contents of keytab ========= */

    kret = krb5_kt_have_content(context, kt);
    CHECK(kret, "Checking for keytab content (full)");

    kret = krb5_kt_start_seq_get(context, kt, &cursor);
    CHECK(kret, "Start sequence get");


    memset(&kent, 0, sizeof(kent));
    cnt = 0;
    while((kret = krb5_kt_next_entry(context, kt, &kent, &cursor)) == 0) {
        if(((kent.vno != 1) && (kent.vno != 2)) ||
           ((kent.key.enctype != e1) && (kent.key.enctype != e2)) ||
           (kent.key.length != 1) ||
           (kent.key.contents[0] != kent.vno +'0')) {
            fprintf(stderr, "Error in read contents\n");
            exit(1);
        }

        if((kent.magic != KV5M_KEYTAB_ENTRY) ||
           (kent.key.magic != KV5M_KEYBLOCK)) {
            fprintf(stderr, "Magic number in sequence not proper\n");
            exit(1);
        }

        cnt++;
        krb5_free_keytab_entry_contents(context, &kent);
    }
    CHECK_ERR(kret, KRB5_KT_END, "getting next entry");

    if(cnt != 3) {
        fprintf(stderr, "Mismatch in number of entries in keytab");
    }

    kret = krb5_kt_end_seq_get(context, kt, &cursor);
    CHECK(kret, "End sequence get");


    /* ==========================   get_entry tests ============== */

    /* Try to lookup unknown principal  - now that keytab exists*/
    kret = krb5_parse_name(context, "test3/test2@TEST.MIT.EDU", &princ);
    CHECK(kret, "parsing principal");


    kret = krb5_kt_get_entry(context, kt, princ, 0, 0, &kent);
    CHECK_ERR(kret, KRB5_KT_NOTFOUND, "Getting nonexistent entry");

    krb5_free_principal(context, princ);

    /* Try to lookup known principal */
    kret = krb5_parse_name(context, "test/test2@TEST.MIT.EDU", &princ);
    CHECK(kret, "parsing principal");

    kret = krb5_kt_get_entry(context, kt, princ, 0, 0, &kent);
    CHECK(kret, "looking up principal");

    /* Ensure a valid answer  - we did not specify an enctype or kvno */
    if (!krb5_principal_compare(context, princ, kent.principal) ||
        ((kent.vno != 1) && (kent.vno != 2)) ||
        ((kent.key.enctype != e1) && (kent.key.enctype != e2)) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Retrieved principal does not check\n");
        exit(1);
    }

    krb5_free_keytab_entry_contents(context, &kent);

    /* Try to lookup a specific enctype - but unspecified kvno - should give
     * max kvno
     */
    kret = krb5_kt_get_entry(context, kt, princ, 0, e1, &kent);
    CHECK(kret, "looking up principal");

    /* Ensure a valid answer  - we did specified an enctype */
    if (!krb5_principal_compare(context, princ, kent.principal) ||
        (kent.vno != 2) || (kent.key.enctype != e1) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Retrieved principal does not check\n");

        exit(1);

    }

    krb5_free_keytab_entry_contents(context, &kent);

    /* Try to lookup unspecified enctype, but a specified kvno */

    kret = krb5_kt_get_entry(context, kt, princ, 2, 0, &kent);
    CHECK(kret, "looking up principal");

    /* Ensure a valid answer  - we did not specify a kvno */
    if (!krb5_principal_compare(context, princ, kent.principal) ||
        (kent.vno != 2) || (kent.key.enctype != e1) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Retrieved principal does not check\n");

        exit(1);

    }

    krb5_free_keytab_entry_contents(context, &kent);



    /* Try to lookup specified enctype and kvno */

    kret = krb5_kt_get_entry(context, kt, princ, 1, e1, &kent);
    CHECK(kret, "looking up principal");

    if (!krb5_principal_compare(context, princ, kent.principal) ||
        (kent.vno != 1) || (kent.key.enctype != e1) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Retrieved principal does not check\n");

        exit(1);

    }

    krb5_free_keytab_entry_contents(context, &kent);


    /* Try lookup with active iterators.  */
    kret = krb5_kt_start_seq_get(context, kt, &cursor);
    CHECK(kret, "Start sequence get(2)");
    kret = krb5_kt_start_seq_get(context, kt, &cursor2);
    CHECK(kret, "Start sequence get(3)");
    kret = krb5_kt_next_entry(context, kt, &kent, &cursor);
    CHECK(kret, "getting next entry(2)");
    krb5_free_keytab_entry_contents(context, &kent);
    kret = krb5_kt_next_entry(context, kt, &kent, &cursor);
    CHECK(kret, "getting next entry(3)");
    kret = krb5_kt_next_entry(context, kt, &kent2, &cursor2);
    CHECK(kret, "getting next entry(4)");
    krb5_free_keytab_entry_contents(context, &kent2);
    kret = krb5_kt_get_entry(context, kt, kent.principal, 0, 0, &kent2);
    CHECK(kret, "looking up principal(2)");
    krb5_free_keytab_entry_contents(context, &kent2);
    kret = krb5_kt_next_entry(context, kt, &kent2, &cursor2);
    CHECK(kret, "getting next entry(5)");
    if (!krb5_principal_compare(context, kent.principal, kent2.principal)) {
        fprintf(stderr, "iterators not in sync\n");
        exit(1);
    }
    krb5_free_keytab_entry_contents(context, &kent);
    krb5_free_keytab_entry_contents(context, &kent2);
    kret = krb5_kt_next_entry(context, kt, &kent, &cursor);
    CHECK(kret, "getting next entry(6)");
    kret = krb5_kt_next_entry(context, kt, &kent2, &cursor2);
    CHECK(kret, "getting next entry(7)");
    krb5_free_keytab_entry_contents(context, &kent);
    krb5_free_keytab_entry_contents(context, &kent2);
    kret = krb5_kt_end_seq_get(context, kt, &cursor);
    CHECK(kret, "ending sequence get(1)");
    kret = krb5_kt_end_seq_get(context, kt, &cursor2);
    CHECK(kret, "ending sequence get(2)");

    /* Try to lookup specified enctype and kvno  - that does not exist*/

    kret = krb5_kt_get_entry(context, kt, princ, 3, e1, &kent);
    CHECK_ERR(kret, KRB5_KT_KVNONOTFOUND,
              "looking up specific principal, kvno, enctype");

    krb5_free_principal(context, princ);


    /* =========================   krb5_kt_remove_entry =========== */
    /* Lookup the keytab entry w/ 2 kvno - and delete version 2 -
       ensure gone */
    kret = krb5_parse_name(context, "test/test2@TEST.MIT.EDU", &princ);
    CHECK(kret, "parsing principal");

    kret = krb5_kt_get_entry(context, kt, princ, 0, e1, &kent);
    CHECK(kret, "looking up principal");

    /* Ensure a valid answer  - we are looking for max(kvno) and enc=e1 */
    if (!krb5_principal_compare(context, princ, kent.principal) ||
        (kent.vno != 2) || (kent.key.enctype != e1) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Retrieved principal does not check\n");

        exit(1);

    }

    /* Delete it */
    kret = krb5_kt_remove_entry(context, kt, &kent);
    CHECK(kret, "Removing entry");

    krb5_free_keytab_entry_contents(context, &kent);
    /* And ensure gone */

    kret = krb5_kt_get_entry(context, kt, princ, 0, e1, &kent);
    CHECK(kret, "looking up principal");

    /* Ensure a valid answer - kvno should now be 1 - we deleted 2 */
    if (!krb5_principal_compare(context, princ, kent.principal) ||
        (kent.vno != 1) || (kent.key.enctype != e1) ||
        (kent.key.length != 1) ||
        (kent.key.contents[0] != kent.vno +'0')) {
        fprintf(stderr, "Delete principal check failed\n");

        exit(1);

    }
    krb5_free_keytab_entry_contents(context, &kent);

    krb5_free_principal(context, princ);

    /* =======================  Finally close =======================  */

    kret = krb5_kt_close(context, kt);
    CHECK(kret, "close");

}

static void
do_test(krb5_context context, const char *prefix, krb5_boolean delete)
{
    char *name, *filename;

    if (asprintf(&filename, "/tmp/kttest.%ld", (long) getpid()) < 0) {
        perror("asprintf");
        exit(1);
    }
    if (asprintf(&name, "%s%s", prefix, filename) < 0) {
        perror("asprintf");
        exit(1);
    }
    printf("Starting test on %s\n", name);
    kt_test(context, name);
    printf("Test on %s passed\n", name);
    if(delete)
        unlink(filename);
    free(filename);
    free(name);

}

int
main(void)
{
    krb5_context context;
    krb5_error_code kret;


    if ((kret = krb5_init_context(&context))) {
        printf("Couldn't initialize krb5 library: %s\n",
               error_message(kret));
        exit(1);
    }

    /* All keytab types are registered by default -- test for
       redundant error */
    kret = krb5_kt_register(context, &krb5_ktf_writable_ops);
    CHECK_ERR(kret, KRB5_KT_TYPE_EXISTS, "register ktf_writable");

    test_misc(context);
    do_test(context, "WRFILE:", FALSE);
    do_test(context, "MEMORY:", TRUE);

    krb5_free_context(context);
    return 0;

}
