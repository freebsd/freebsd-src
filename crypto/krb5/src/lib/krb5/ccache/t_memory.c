/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_memory.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

#include "mcc.h"

krb5_data client1 = {
#define DATA "client1-comp1"
    sizeof(DATA),
    DATA,
#undef DATA
};

krb5_data client2 = {
#define DATA "client1-comp2"
    sizeof(DATA),
    DATA,
#undef DATA
};

krb5_data server1 = {
#define DATA "server1-comp1"
    sizeof(DATA),
    DATA,
#undef DATA
};

krb5_data server2 = {
#define DATA "server1-comp2"
    sizeof(DATA),
    DATA,
#undef DATA
};

krb5_creds test_creds = {
    NULL,
    NULL,
    {
        1,
        1,
        (unsigned char *) "1"
    },
    {
        1111,
        2222,
        3333,
        4444
    },
    1,
    5555,
    {
#define TICKET "This is ticket 1"
        sizeof(TICKET),
        TICKET,
#undef TICKET
    },
    {
#define TICKET "This is ticket 2"
        sizeof(TICKET),
        TICKET,
#undef TICKET
    },
};

void
init_test_cred()
{
    test_creds.client = (krb5_principal) malloc(sizeof(krb5_data *)*3);
    test_creds.client[0] = &client1;
    test_creds.client[1] = &client2;
    test_creds.client[2] = NULL;

    test_creds.server = (krb5_principal) malloc(sizeof(krb5_data *)*3);
    test_creds.server[0] = &server1;
    test_creds.server[1] = &server2;
    test_creds.server[2] = NULL;
}

#define CHECK(kret,msg)                         \
    if (kret != KRB5_OK) {                      \
        printf("%s returned %d\n", msg, kret);  \
    };

void
mcc_test()
{
    krb5_ccache id;
    krb5_creds creds;
    krb5_error_code kret;
    krb5_cc_cursor cursor;

    init_test_cred();

    kret = krb5_mcc_resolve(context, &id, "/tmp/tkt_test");
    CHECK(kret, "resolve");
    kret = krb5_mcc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize");
    kret = krb5_mcc_store(context, id, &test_creds);
    CHECK(kret, "store");

    kret = krb5_mcc_start_seq_get(context, id, &cursor);
    CHECK(kret, "start_seq_get");
    kret = 0;
    while (kret != KRB5_CC_END) {
        printf("Calling next_cred\n");
        kret = krb5_mcc_next_cred(context, id, &cursor, &creds);
        CHECK(kret, "next_cred");
    }
    kret = krb5_mcc_end_seq_get(context, id, &cursor);
    CHECK(kret, "end_seq_get");

    kret = krb5_mcc_destroy(context, id);
    CHECK(kret, "destroy");
    kret = krb5_mcc_close(context, id);
    CHECK(kret, "close");
}
