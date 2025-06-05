/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/t_stdio.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
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

#include "scc.h"

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

int x = 0x12345;
krb5_address addr = {
    ADDRTYPE_INET,
    4,
    (krb5_octet *) &x,
};

krb5_address *addrs[] = {
    &addr,
    0,
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
        4444,
    },
    1,
    5555,
    addrs,
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
        com_err(msg, kret, "");                 \
    } else printf("%s went ok\n", msg);

int flags = 0;
void
scc_test()
{
    krb5_ccache id;
    krb5_creds creds;
    krb5_error_code kret;
    krb5_cc_cursor cursor;

    init_test_cred();

    kret = krb5_scc_resolve(context, &id, "/tmp/tkt_test");
    CHECK(kret, "resolve");
    kret = krb5_scc_initialize(context, id, test_creds.client);
    CHECK(kret, "initialize");
    kret = krb5_scc_store(id, &test_creds);
    CHECK(kret, "store");

    kret = krb5_scc_set_flags (id, flags);
    CHECK(kret, "set_flags");
    kret = krb5_scc_start_seq_get(id, &cursor);
    CHECK(kret, "start_seq_get");
    kret = 0;
    while (kret != KRB5_CC_END) {
        printf("Calling next_cred\n");
        kret = krb5_scc_next_cred(id, &cursor, &creds);
        CHECK(kret, "next_cred");
    }
    kret = krb5_scc_end_seq_get(id, &cursor);
    CHECK(kret, "end_seq_get");

    kret = krb5_scc_close(id);
    CHECK(kret, "close");


    kret = krb5_scc_resolve(&id, "/tmp/tkt_test");
    CHECK(kret, "resolve");
    kret = krb5_scc_destroy(id);
    CHECK(kret, "destroy");
}

int remove (s) char*s; { return unlink(s); }
int main () {
    initialize_krb5_error_table ();
    init_test_cred ();
    scc_test ();
    flags = !flags;
    scc_test ();
    return 0;
}
