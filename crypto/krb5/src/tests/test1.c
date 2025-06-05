/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/test1.c - Regression tests for krb5 library */
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

#include "krb5.h"

unsigned char key_one[8] = { 0x10, 0x23, 0x32, 0x45, 0x54, 0x67, 0x76, 0x89 };
unsigned char key_two[8] = { 0xea, 0x89, 0x57, 0x76, 0x5b, 0xcd, 0x0d, 0x34 };

extern void dump_data();

tkt_test_1()
{
    krb5_data *data;
    krb5_ticket tk_in, *tk_out;
    krb5_keyblock sess_k, serv_k, *nsess;
    krb5_enc_tkt_part tk_in_enc;
    int code;
    krb5_address *addr_list[2];
    krb5_address addr_1;
    static krb5_octet ip_addr_1[4] = { 18, 72, 0, 122 };
    char *out;

    /*
     * fill in some values on the "in" side of the ticket
     */
    code = krb5_parse_name ("server/test/1@BOGUS.ORG", &tk_in.server);
    if (code != 0) {
        com_err("tkt_test_1", code, " parsing server principal");
        return;
    }

    serv_k.enctype = 1;         /* XXX symbolic constant */
    serv_k.length = 8;          /* XXX symbolic constant */
    serv_k.contents = key_one;

    sess_k.enctype = 1;         /* XXX symbolic constant */
    sess_k.length = 8;          /* XXX symbolic constant */
    sess_k.contents = key_two;

    tk_in.etype = 1;            /* XXX symbolic constant here */
    tk_in.skvno = 4;

    tk_in.enc_part2 = &tk_in_enc;

    tk_in_enc.flags = 0x11;
    tk_in_enc.session = &sess_k;

    tk_in_enc.times.authtime = 42;
    tk_in_enc.times.starttime = 43;
    tk_in_enc.times.endtime = 44;

    code = krb5_parse_name ("client/test/1@BOGUS.ORG", &tk_in_enc.client);
    if (code != 0) {
        com_err("tkt_test_1", code, " parsing client principal");
        return;
    }
    tk_in_enc.transited.length = 0;

    addr_1.addrtype = ADDRTYPE_INET; /* XXX should be KRB5_ADDR... */
    addr_1.length = 4;
    addr_1.contents = ip_addr_1;

    addr_list[0] = &addr_1;
    addr_list[1] = 0;


    tk_in_enc.caddrs = addr_list;
    tk_in_enc.authorization_data = 0;

    code = krb5_encrypt_tkt_part(&serv_k, &tk_in);
    if (code != 0) {
        com_err ("tkt_test_1", code, " encrypting ticket");
        return;
    }

    data = 0;

    code = krb5_encode_ticket (&tk_in,  &data);
    if (code != 0) {
        com_err ("tkt_test_1", code, " encoding ticket");
        return;
    }

    dump_data(data);

    tk_out = 0;
    code = krb5_decode_ticket (data, &tk_out);
    if (code != 0) {
        com_err ("tkt_test_1", code, "decoding ticket");
        return;
    }
    /* check the plaintext values */
    if (tk_out->etype != 1) {
        com_err ("tkt_test_1", 0, "wrong etype");
        return;
    }
    if (tk_out->skvno != 4) {
        com_err ("tkt_test_1", 0, "wrong kvno");
        return;
    }

    code = krb5_unparse_name(tk_out->server, &out);
    if (code != 0) {
        com_err ("tkt_test_1", code, "couldn't unparse server principal");
        return;
    }
    if (strcmp (out, "server/test/1@BOGUS.ORG") != 0) {
        com_err("tkt_test_1", 0, "wrong server principal");
        return;
    }
    free(out);
    out = 0;

    /* decode the ciphertext */
    code = krb5_decrypt_tkt_part (&serv_k, tk_out);
    if (code != 0) {
        com_err ("tkt_test_1", code, "while decrypting ticket");
        return;
    }

    /* check the contents */
    if (tk_out->enc_part2->flags != 0x11) {
        com_err("tkt_test_1", 0, "wrong flags");
        return;
    }

    nsess = tk_out->enc_part2->session;

    if (nsess->enctype != 1) {
        com_err("tkt_test_1", 0, "wrong session key type");
        return;
    }
    if (nsess->length != 8) {
        com_err("tkt_test_1", 0, "wrong session key length");
        return;
    }
    if (memcmp(nsess->contents, key_two, 8) != 0) {
        com_err("tkt_test_1", 0, "wrong session key contents");
        return;
    }

    code = krb5_unparse_name(tk_out->enc_part2->client, &out);
    if (code != 0) {
        com_err ("tkt_test_1", code, "couldn't unparse client principal");
        return;
    }
    if (strcmp (out, "client/test/1@BOGUS.ORG") != 0) {
        com_err("tkt_test_1", 0, "wrong client principal");
        return;
    }
    free(out);
    out = 0;
    if (tk_out->enc_part2->transited.length != 0) {
        com_err("tkt_test_1", 0, "wrong transited length");
        return;
    }
    /* XXX should check address here, too */
    /* XXX should check times here */
    /* XXX should check auth. data here */
    printf("test 1 passed\n");
}



main()
{
    krb5_init_ets();
    tkt_test_1();
}
