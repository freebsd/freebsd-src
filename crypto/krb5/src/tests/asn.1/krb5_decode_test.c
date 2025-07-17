/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/krb5_decode_test.c */
/*
 * Copyright (C) 1994 by the Massachusetts Institute of Technology.
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
#include "k5-spake.h"
#include "ktest.h"
#include "com_err.h"
#include "utility.h"
#include "ktest_equal.h"

#include "debug.h"
#include <string.h>

krb5_context test_context;
int error_count = 0;

void krb5_ktest_free_enc_data(krb5_context context, krb5_enc_data *val);

#ifndef DISABLE_PKINIT
static int equal_principal(krb5_principal *ref, krb5_principal var);
static void ktest_free_auth_pack(krb5_context context, krb5_auth_pack *val);
static void ktest_free_kdc_dh_key_info(krb5_context context,
                                       krb5_kdc_dh_key_info *val);
static void ktest_free_pa_pk_as_req(krb5_context context,
                                    krb5_pa_pk_as_req *val);
static void ktest_free_pa_pk_as_rep(krb5_context context,
                                    krb5_pa_pk_as_rep *val);
static void ktest_free_reply_key_pack(krb5_context context,
                                      krb5_reply_key_pack *val);
#endif
static void ktest_free_kkdcp_message(krb5_context context,
                                     krb5_kkdcp_message *val);

int main(argc, argv)
    int argc;
    char **argv;
{
    krb5_data code;
    krb5_error_code retval;

    retval = krb5_init_context(&test_context);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
        exit(1);
    }
    init_access(argv[0]);


#define setup(type,constructor)                                         \
    type ref, *var;                                                     \
    constructor(&ref);                                                  \
    do {} while (0)

#define decode_run(typestring,description,encoding,decoder,comparator,cleanup) do { \
    retval = krb5_data_hex_parse(&code,encoding);                       \
    if (retval) {                                                       \
        com_err("krb5_decode_test", retval, "while parsing %s", typestring); \
        exit(1);                                                        \
    }                                                                   \
    retval = decoder(&code,&var);                                       \
    if (retval) {                                                       \
        com_err("krb5_decode_test", retval, "while decoding %s", typestring); \
        error_count++;                                                  \
    }                                                                   \
    test(comparator(&ref,var),typestring);                              \
    printf("%s\n",description);                                         \
    krb5_free_data_contents(test_context, &code);                       \
    cleanup(test_context, var);                                         \
} while (0)

#define decode_fail(err,typestring,description,encoding,decoder) do {   \
    retval = krb5_data_hex_parse(&code,encoding);                       \
    if (retval) {                                                       \
        com_err("krb5_decode_test", retval, "while parsing %s", typestring); \
        exit(1);                                                        \
    }                                                                   \
    retval = decoder(&code,&var);                                       \
    if (retval != (err)) {                                              \
        com_err("krb5_decode_test", retval, "while decoding %s", typestring); \
        error_count++;                                                  \
    }                                                                   \
    test(1,typestring);                                                 \
    printf("%s\n",description);                                         \
    krb5_free_data_contents(test_context, &code);                       \
} while (0)

    /****************************************************************/
    /* decode_krb5_authenticator */
    {
        setup(krb5_authenticator,ktest_make_sample_authenticator);

        decode_run("authenticator","","62 81 A1 30 81 9E A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34 A4 05 02 03 01 E2 40 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A7 03 02 01 11 A8 24 30 22 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72",decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0xffffff80;
        decode_run("authenticator","(80 -> seq-number 0xffffff80)",
                   "62 81 A1 30 81 9E"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 03 02 01 80"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0xffffffff;
        decode_run("authenticator","(FF -> seq-number 0xffffffff)",
                   "62 81 A1 30 81 9E"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 03 02 01 FF"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0xff;
        decode_run("authenticator","(00FF -> seq-number 0xff)",
                   "62 81 A2 30 81 9F"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 04 02 02 00 FF"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0xffffffff;
        decode_run("authenticator","(00FFFFFFFF -> seq-number 0xffffffff)",
                   "62 81 A5 30 81 A2"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 07 02 05 00 FF FF FF FF"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0x7fffffff;
        decode_run("authenticator","(7FFFFFFF -> seq-number 0x7fffffff)",
                   "62 81 A4 30 81 A1"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 06 02 04 7F FF FF FF"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ref.seq_number = 0xffffffff;
        decode_run("authenticator","(FFFFFFFF -> seq-number 0xffffffff)",
                   "62 81 A4 30 81 A1"
                   "   A0 03 02 01 05"
                   "   A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55"
                   "   A2 1A 30 18"
                   "      A0 03 02 01 01"
                   "      A1 11 30 0F"
                   "         1B 06 68 66 74 73 61 69"
                   "         1B 05 65 78 74 72 61"
                   "   A3 0F 30 0D"
                   "      A0 03 02 01 01"
                   "      A1 06 04 04 31 32 33 34"
                   "   A4 05 02 03 01 E2 40"
                   "   A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A"
                   "   A6 13 30 11"
                   "      A0 03 02 01 01"
                   "      A1 0A 04 08 31 32 33 34 35 36 37 38"
                   "   A7 06 02 04 FF FF FF FF"
                   "   A8 24 30 22"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   "      30 0F"
                   "         A0 03 02 01 01"
                   "         A1 08 04 06 66 6F 6F 62 61 72"
                   ,decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ktest_destroy_checksum(&(ref.checksum));
        ktest_destroy_keyblock(&(ref.subkey));
        ref.seq_number = 0;
        ktest_empty_authorization_data(ref.authorization_data);
        decode_run("authenticator","(optionals empty)","62 4F 30 4D A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 05 02 03 01 E2 40 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ktest_destroy_authorization_data(&(ref.authorization_data));

        decode_run("authenticator","(optionals NULL)","62 4F 30 4D A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 05 02 03 01 E2 40 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_authenticator,ktest_equal_authenticator,krb5_free_authenticator);

        ktest_empty_authenticator(&ref);
    }

    /****************************************************************/
    /* decode_krb5_ticket */
    {
        setup(krb5_ticket,ktest_make_sample_ticket);
        decode_run("ticket","","61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_ticket,ktest_equal_ticket,krb5_free_ticket);
        decode_run("ticket","(+ trailing [4] INTEGER","61 61 30 5F A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A4 03 02 01 01",decode_krb5_ticket,ktest_equal_ticket,krb5_free_ticket);

/*
  "61 80 30 80 "
  "  A0 03 02 01 05 "
  "  A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 "
  "  A2 80 30 80 "
  "    A0 03 02 01 01 "
  "    A1 80 30 80 "
  "      1B 06 68 66 74 73 61 69 "
  "      1B 05 65 78 74 72 61 "
  "    00 00 00 00 "
  "  00 00 00 00 "
  "  A3 80 30 80 "
  "    A0 03 02 01 00 "
  "    A1 03 02 01 05 "
  "    A2 17 04 15 6B 72 62 41 53 4E 2E 31 "
  "      20 74 65 73 74 20 6D 65 73 73 61 67 65 "
  "  00 00 00 00"
  "00 00 00 00"
*/
        decode_fail(ASN1_INDEF,"ticket","(indefinite lengths)", "61 80 30 80 A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 80 30 80 A0 03 02 01 01 A1 80 30 80 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 00 00 00 00 00 00 00 00 A3 80 30 80 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 00 00 00 00 00 00 00 00" ,decode_krb5_ticket);
/*
  "61 80 30 80 "
  "  A0 03 02 01 05 "
  "  A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 "
  "  A2 80 30 80 "
  "    A0 03 02 01 01 "
  "    A1 80 30 80 "
  "      1B 06 68 66 74 73 61 69 "
  "      1B 05 65 78 74 72 61 "
  "    00 00 00 00 "
  "  00 00 00 00 "
  "  A3 80 30 80 "
  "    A0 03 02 01 00 "
  "    A1 03 02 01 05 "
  "    A2 17 04 15 6B 72 62 41 53 4E 2E 31 "
  "      20 74 65 73 74 20 6D 65 73 73 61 67 65 "
  "  00 00 00 00"
  "  A4 03 02 01 01 "
  "00 00 00 00"
*/
        decode_fail(ASN1_INDEF,"ticket","(indefinite lengths + trailing [4] INTEGER)", "61 80 30 80 A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 80 30 80 A0 03 02 01 01 A1 80 30 80 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 00 00 00 00 00 00 00 00 A3 80 30 80 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 00 00 00 00 A4 03 02 01 01 00 00 00 00",decode_krb5_ticket);

        ktest_empty_ticket(&ref);

    }

    /****************************************************************/
    /* decode_krb5_encryption_key */
    {
        setup(krb5_keyblock,ktest_make_sample_keyblock);

        decode_run("encryption_key","","30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);

        decode_run("encryption_key","(+ trailing [2] INTEGER)","30 16 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 03 02 01 01",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        decode_run("encryption_key","(+ trailing [2] SEQUENCE {[0] INTEGER})","30 1A A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 07 30 05 A0 03 02 01 01",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        decode_fail(ASN1_INDEF,"encryption_key","(indefinite lengths)","30 80 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 00 00",decode_krb5_encryption_key);
        decode_fail(ASN1_INDEF,"encryption_key","(indefinite lengths + trailing [2] INTEGER)","30 80 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 03 02 01 01 00 00",decode_krb5_encryption_key);
        decode_fail(ASN1_INDEF,"encryption_key","(indefinite lengths + trailing [2] SEQUENCE {[0] INTEGER})","30 80 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 80 30 80 A0 03 02 01 01 00 00 00 00 00 00",decode_krb5_encryption_key);
        decode_fail(ASN1_INDEF,"encryption_key","(indefinite lengths + trailing SEQUENCE {[0] INTEGER})","30 80 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 30 80 A0 03 02 01 01 00 00 00 00",decode_krb5_encryption_key);
        ref.enctype = -1;
        decode_run("encryption_key","(enctype = -1)","30 11 A0 03 02 01 FF A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        ref.enctype = -255;
        decode_run("encryption_key","(enctype = -255)","30 12 A0 04 02 02 FF 01 A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        ref.enctype = 255;
        decode_run("encryption_key","(enctype = 255)","30 12 A0 04 02 02 00 FF A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        ref.enctype = -2147483648U;
        decode_run("encryption_key","(enctype = -2147483648)","30 14 A0 06 02 04 80 00 00 00 A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);
        ref.enctype = 2147483647;
        decode_run("encryption_key","(enctype = 2147483647)","30 14 A0 06 02 04 7F FF FF FF A1 0A 04 08 31 32 33 34 35 36 37 38",decode_krb5_encryption_key,ktest_equal_encryption_key,krb5_free_keyblock);

        ktest_empty_keyblock(&ref);
    }

    /****************************************************************/
    /* decode_krb5_enc_tkt_part */
    {
        setup(krb5_enc_tkt_part,ktest_make_sample_enc_tkt_part);
        decode_run("enc_tkt_part","","63 82 01 14 30 82 01 10 A0 07 03 05 00 FE DC BA 98 A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A8 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A9 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 AA 24 30 22 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part,krb5_free_enc_tkt_part);

        ref.times.starttime = 0;
        ref.times.renew_till = 0;
        ktest_destroy_address(&(ref.caddrs[1]));
        ktest_destroy_address(&(ref.caddrs[0]));
        ktest_destroy_authdata(&(ref.authorization_data[1]));
        ktest_destroy_authdata(&(ref.authorization_data[0]));
        /* ISODE version fails on the empty caddrs field */
        ktest_destroy_addresses(&(ref.caddrs));
        ktest_destroy_authorization_data(&(ref.authorization_data));

        decode_run("enc_tkt_part","(optionals NULL)","63 81 A5 30 81 A2 A0 07 03 05 00 FE DC BA 98 A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part, krb5_free_enc_tkt_part);

        decode_run("enc_tkt_part","(optionals NULL + bitstring enlarged to 38 bits)","63 81 A6 30 81 A3 A0 08 03 06 02 FE DC BA 98 DC A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part,krb5_free_enc_tkt_part);

        decode_run("enc_tkt_part","(optionals NULL + bitstring enlarged to 40 bits)","63 81 A6 30 81 A3 A0 08 03 06 00 FE DC BA 98 DE A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part,krb5_free_enc_tkt_part);

        decode_run("enc_tkt_part","(optionals NULL + bitstring reduced to 29 bits)","63 81 A5 30 81 A2 A0 07 03 05 03 FE DC BA 98 A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part,krb5_free_enc_tkt_part);

        ref.flags &= 0xFFFFFF00;

        decode_run("enc_tkt_part","(optionals NULL + bitstring reduced to 24 bits)","63 81 A4 30 81 A1 A0 06 03 04 00 FE DC BA A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 2E 30 2C A0 03 02 01 01 A1 25 04 23 45 44 55 2C 4D 49 54 2E 2C 41 54 48 45 4E 41 2E 2C 57 41 53 48 49 4E 47 54 4F 4E 2E 45 44 55 2C 43 53 2E A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_enc_tkt_part,ktest_equal_enc_tkt_part,krb5_free_enc_tkt_part);

        ktest_empty_enc_tkt_part(&ref);
    }

    /****************************************************************/
    /* decode_krb5_enc_kdc_rep_part */
    {
        setup(krb5_enc_kdc_rep_part,ktest_make_sample_enc_kdc_rep_part);

        decode_run("enc_kdc_rep_part","","7A 82 01 0E 30 82 01 0A A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 36 30 34 30 18 A0 03 02 01 FB A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A 30 18 A0 03 02 01 FB A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A2 03 02 01 2A A3 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A4 07 03 05 00 FE DC BA 98 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A8 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A9 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 AA 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 AB 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23",decode_krb5_enc_kdc_rep_part,ktest_equal_enc_kdc_rep_part,krb5_free_enc_kdc_rep_part);

        ref.key_exp = 0;
        /* ref.times.starttime = 0;*/
        ref.times.starttime = ref.times.authtime;
        ref.times.renew_till = 0;
        ref.flags &= ~TKT_FLG_RENEWABLE;
        ktest_destroy_addresses(&(ref.caddrs));

        decode_run("enc_kdc_rep_part","(optionals NULL)","7A 81 B2 30 81 AF A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 36 30 34 30 18 A0 03 02 01 FB A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A 30 18 A0 03 02 01 FB A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A2 03 02 01 2A A4 07 03 05 00 FE 5C BA 98 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A9 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 AA 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61",decode_krb5_enc_kdc_rep_part,ktest_equal_enc_kdc_rep_part,krb5_free_enc_kdc_rep_part);

        ktest_empty_enc_kdc_rep_part(&ref);
    }

    /****************************************************************/
    /* decode_krb5_as_rep */
    {
        setup(krb5_kdc_rep,ktest_make_sample_kdc_rep);
        ref.msg_type = KRB5_AS_REP;

        decode_run("as_rep","","6B 81 EA 30 81 E7 A0 03 02 01 05 A1 03 02 01 0B A2 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A4 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 5E 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A6 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_as_rep,ktest_equal_as_rep,krb5_free_kdc_rep);

/*
  6B 80 30 80
  A0 03 02 01 05
  A1 03 02 01 0B
  A2 80 30 80
  30 80
  A1 03 02 01 0D
  A2 09 04 07 70 61 2D 64 61 74 61
  00 00
  30 80
  A1 03 02 01 0D
  A2 09 04 07 70 61 2D 64 61 74 61
  00 00
  00 00 00 00
  A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55
  A4 80 30 80
  A0 03 02 01 01
  A1 80 30 80
  1B 06 68 66 74 73 61 69
  1B 05 65 78 74 72 61
  00 00 00 00
  00 00 00 00
  A5 80 61 80 30 80
  A0 03 02 01 05
  A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55
  A2 80 30 80
  A0 03 02 01 01
  A1 80 30 80
  1B 06 68 66 74 73 61 69
  1B 05 65 78 74 72 61
  00 00 00 00
  00 00 00 00
  A3 80 30 80
  A0 03 02 01 00
  A1 03 02 01 05
  A2 17 04 15 6B 72 62 41 53 4E 2E 31
  20 74 65 73 74 20 6D 65
  73 73 61 67 65
  00 00 00 00
  00 00 00 00 00 00
  A6 80 30 80
  A0 03 02 01 00
  A1 03 02 01 05
  A2 17 04 15 6B 72 62 41 53 4E 2E 31
  20 74 65 73 74 20 6D 65
  73 73 61 67 65
  00 00 00 00
  00 00 00 00
*/
        decode_fail(ASN1_INDEF,"as_rep","(indefinite lengths)","6B 80 30 80 A0 03 02 01 05 A1 03 02 01 0B A2 80 30 80 30 80 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 00 00 30 80 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 00 00 00 00 00 00 A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A4 80 30 80 A0 03 02 01 01 A1 80 30 80 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 00 00 00 00 00 00 00 00 A5 80 61 80 30 80 A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 80 30 80 A0 03 02 01 01 A1 80 30 80 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 00 00 00 00 00 00 00 00 A3 80 30 80 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 00 00 00 00 00 00 00 00 00 00 A6 80 30 80 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 00 00 00 00 00 00 00 00",decode_krb5_as_rep);
        ktest_destroy_pa_data_array(&(ref.padata));
        decode_run("as_rep","(optionals NULL)","6B 81 C2 30 81 BF A0 03 02 01 05 A1 03 02 01 0B A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A4 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 5E 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A6 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_as_rep,ktest_equal_as_rep,krb5_free_kdc_rep);

        ktest_empty_kdc_rep(&ref);
    }

    /****************************************************************/
    /* decode_krb5_tgs_rep */
    {
        setup(krb5_kdc_rep,ktest_make_sample_kdc_rep);
        ref.msg_type = KRB5_TGS_REP;

        decode_run("tgs_rep","","6D 81 EA 30 81 E7 A0 03 02 01 05 A1 03 02 01 0D A2 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A4 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 5E 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A6 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_tgs_rep,ktest_equal_tgs_rep,krb5_free_kdc_rep);

        ktest_destroy_pa_data_array(&(ref.padata));
        decode_run("tgs_rep","(optionals NULL)","6D 81 C2 30 81 BF A0 03 02 01 05 A1 03 02 01 0D A3 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A4 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 5E 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A6 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_tgs_rep,ktest_equal_tgs_rep,krb5_free_kdc_rep);

        ktest_empty_kdc_rep(&ref);
    }

    /****************************************************************/
    /* decode_krb5_ap_req */
    {
        setup(krb5_ap_req,ktest_make_sample_ap_req);
        decode_run("ap_req","","6E 81 9D 30 81 9A A0 03 02 01 05 A1 03 02 01 0E A2 07 03 05 00 FE DC BA 98 A3 5E 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A4 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_ap_req,ktest_equal_ap_req,krb5_free_ap_req);
        ktest_empty_ap_req(&ref);

    }

    /****************************************************************/
    /* decode_krb5_ap_rep */
    {
        setup(krb5_ap_rep,ktest_make_sample_ap_rep);
        decode_run("ap_rep","","6F 33 30 31 A0 03 02 01 05 A1 03 02 01 0F A2 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_ap_rep,ktest_equal_ap_rep,krb5_free_ap_rep);
        ktest_empty_ap_rep(&ref);
    }

    /****************************************************************/
    /* decode_krb5_ap_rep_enc_part */
    {
        setup(krb5_ap_rep_enc_part,ktest_make_sample_ap_rep_enc_part);

        decode_run("ap_rep_enc_part","","7B 36 30 34 A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A1 05 02 03 01 E2 40 A2 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A3 03 02 01 11",decode_krb5_ap_rep_enc_part,ktest_equal_ap_rep_enc_part,krb5_free_ap_rep_enc_part);

        ktest_destroy_keyblock(&(ref.subkey));
        ref.seq_number = 0;
        decode_run("ap_rep_enc_part","(optionals NULL)","7B 1C 30 1A A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A1 05 02 03 01 E2 40",decode_krb5_ap_rep_enc_part,ktest_equal_ap_rep_enc_part,krb5_free_ap_rep_enc_part);

        retval = krb5_data_hex_parse(&code, "7B 06 30 04 A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A1 05 02 03 01 E2 40");
        if (retval) {
            com_err("krb5_decode_test", retval, "while parsing");
            exit(1);
        }
        retval = decode_krb5_ap_rep_enc_part(&code, &var);
        if (retval != ASN1_OVERRUN) {
            printf("ERROR: ");
            error_count++;
        } else {
            printf("OK: ");
        }
        printf("ap_rep_enc_part(optionals NULL + expect ASN1_OVERRUN for inconsistent length of timestamp)\n");
        krb5_free_data_contents(test_context, &code);
        krb5_free_ap_rep_enc_part(test_context, var);

        ktest_empty_ap_rep_enc_part(&ref);
    }

    /****************************************************************/
    /* decode_krb5_as_req */
    {
        setup(krb5_kdc_req,ktest_make_sample_kdc_req);
        ref.msg_type = KRB5_AS_REQ;

        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("as_req","","6A 82 01 E4 30 82 01 E0 A1 03 02 01 05 A2 03 02 01 0A A3 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A4 82 01 AA 30 82 01 A6 A0 07 03 05 00 FE DC BA 90 A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 A9 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 AA 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_as_req,ktest_equal_as_req,krb5_free_kdc_req);

        ktest_destroy_pa_data_array(&(ref.padata));
        ktest_destroy_principal(&(ref.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(ref.server));
#endif
        ref.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        ref.from = 0;
        ref.rtime = 0;
        ktest_destroy_addresses(&(ref.addresses));
        ktest_destroy_enc_data(&(ref.authorization_data));
        decode_run("as_req","(optionals NULL except second_ticket)","6A 82 01 14 30 82 01 10 A1 03 02 01 05 A2 03 02 01 0A A4 82 01 02 30 81 FF A0 07 03 05 00 FE DC BA 98 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_as_req,ktest_equal_as_req,krb5_free_kdc_req);
        ktest_destroy_sequence_of_ticket(&(ref.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(ref.server));
#endif
        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("as_req","(optionals NULL except server)","6A 69 30 67 A1 03 02 01 05 A2 03 02 01 0A A4 5B 30 59 A0 07 03 05 00 FE DC BA 90 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01",decode_krb5_as_req,ktest_equal_as_req,krb5_free_kdc_req);

        ktest_empty_kdc_req(&ref);

    }


    /****************************************************************/
    /* decode_krb5_tgs_req */
    {
        setup(krb5_kdc_req,ktest_make_sample_kdc_req);
        ref.msg_type = KRB5_TGS_REQ;

        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("tgs_req","","6C 82 01 E4 30 82 01 E0 A1 03 02 01 05 A2 03 02 01 0C A3 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A4 82 01 AA 30 82 01 A6 A0 07 03 05 00 FE DC BA 90 A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 A9 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 AA 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_tgs_req,ktest_equal_tgs_req,krb5_free_kdc_req);

        ktest_destroy_pa_data_array(&(ref.padata));
        ktest_destroy_principal(&(ref.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(ref.server));
#endif
        ref.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        ref.from = 0;
        ref.rtime = 0;
        ktest_destroy_addresses(&(ref.addresses));
        ktest_destroy_enc_data(&(ref.authorization_data));
        decode_run("tgs_req","(optionals NULL except second_ticket)","6C 82 01 14 30 82 01 10 A1 03 02 01 05 A2 03 02 01 0C A4 82 01 02 30 81 FF A0 07 03 05 00 FE DC BA 98 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_tgs_req,ktest_equal_tgs_req,krb5_free_kdc_req);

        ktest_destroy_sequence_of_ticket(&(ref.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(ref.server));
#endif
        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("tgs_req","(optionals NULL except server)","6C 69 30 67 A1 03 02 01 05 A2 03 02 01 0C A4 5B 30 59 A0 07 03 05 00 FE DC BA 90 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01",decode_krb5_tgs_req,ktest_equal_tgs_req,krb5_free_kdc_req);

        ktest_empty_kdc_req(&ref);
    }

    /****************************************************************/
    /* decode_krb5_kdc_req_body */
    {
        krb5_kdc_req ref, *var;
        memset(&ref, 0, sizeof(krb5_kdc_req));
        ktest_make_sample_kdc_req_body(&ref);
        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("kdc_req_body","","30 82 01 A6 A0 07 03 05 00 FE DC BA 90 A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 A9 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 AA 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_kdc_req_body,ktest_equal_kdc_req_body,krb5_free_kdc_req);

        ktest_destroy_principal(&(ref.client));
#ifndef ISODE_SUCKS
        ktest_destroy_principal(&(ref.server));
#endif
        ref.kdc_options |= KDC_OPT_ENC_TKT_IN_SKEY;
        ref.from = 0;
        ref.rtime = 0;
        ktest_destroy_addresses(&(ref.addresses));
        ktest_destroy_enc_data(&(ref.authorization_data));
        decode_run("kdc_req_body","(optionals NULL except second_ticket)","30 81 FF A0 07 03 05 00 FE DC BA 98 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_kdc_req_body,ktest_equal_kdc_req_body,krb5_free_kdc_req);

        ktest_destroy_sequence_of_ticket(&(ref.second_ticket));
#ifndef ISODE_SUCKS
        ktest_make_sample_principal(&(ref.server));
#endif
        ref.kdc_options &= ~KDC_OPT_ENC_TKT_IN_SKEY;
        decode_run("kdc_req_body","(optionals NULL except server)","30 59 A0 07 03 05 00 FE DC BA 90 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01",decode_krb5_kdc_req_body,ktest_equal_kdc_req_body,krb5_free_kdc_req);
        ref.nktypes = 0;
        free(ref.ktype);
        ref.ktype = NULL;
        decode_run("kdc_req_body","(optionals NULL except server; zero-length etypes)","30 53 A0 07 03 05 00 FE DC BA 90 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 02 30 00",decode_krb5_kdc_req_body,ktest_equal_kdc_req_body,krb5_free_kdc_req);

        ktest_empty_kdc_req(&ref);
    }


    /****************************************************************/
    /* decode_krb5_safe */
    {
        setup(krb5_safe,ktest_make_sample_safe);
        decode_run("safe","","74 6E 30 6C A0 03 02 01 05 A1 03 02 01 14 A2 4F 30 4D A0 0A 04 08 6B 72 62 35 64 61 74 61 A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A2 05 02 03 01 E2 40 A3 03 02 01 11 A4 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A5 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A3 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",decode_krb5_safe,ktest_equal_safe,krb5_free_safe);

        ref.timestamp = 0;
        ref.usec = 0;
        ref.seq_number = 0;
        ktest_destroy_address(&(ref.r_address));
        decode_run("safe","(optionals NULL)","74 3E 30 3C A0 03 02 01 05 A1 03 02 01 14 A2 1F 30 1D A0 0A 04 08 6B 72 62 35 64 61 74 61 A4 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A3 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",decode_krb5_safe,ktest_equal_safe,krb5_free_safe);

        ktest_empty_safe(&ref);
    }

    /****************************************************************/
    /* decode_krb5_priv */
    {
        setup(krb5_priv,ktest_make_sample_priv);
        decode_run("priv","","75 33 30 31 A0 03 02 01 05 A1 03 02 01 15 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_priv,ktest_equal_priv,krb5_free_priv);
        ktest_empty_priv(&ref);
    }

    /****************************************************************/
    /* decode_krb5_enc_priv_part */
    {
        setup(krb5_priv_enc_part,ktest_make_sample_priv_enc_part);
        decode_run("enc_priv_part","","7C 4F 30 4D A0 0A 04 08 6B 72 62 35 64 61 74 61 A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A2 05 02 03 01 E2 40 A3 03 02 01 11 A4 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A5 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23",decode_krb5_enc_priv_part,ktest_equal_enc_priv_part,krb5_free_priv_enc_part);

        ref.timestamp = 0;
        ref.usec = 0;
        ref.seq_number = 0;
        ktest_destroy_address(&(ref.r_address));
        decode_run("enc_priv_part","(optionals NULL)","7C 1F 30 1D A0 0A 04 08 6B 72 62 35 64 61 74 61 A4 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23",decode_krb5_enc_priv_part,ktest_equal_enc_priv_part,krb5_free_priv_enc_part);
        ktest_empty_priv_enc_part(&ref);
    }

    /****************************************************************/
    /* decode_krb5_cred */
    {
        setup(krb5_cred,ktest_make_sample_cred);
        decode_run("cred","","76 81 F6 30 81 F3 A0 03 02 01 05 A1 03 02 01 16 A2 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_cred,ktest_equal_cred,krb5_free_cred);
        ktest_empty_cred(&ref);
    }

    /****************************************************************/
    /* decode_krb5_enc_cred_part */
    {
        setup(krb5_cred_enc_part,ktest_make_sample_cred_enc_part);
        decode_run("enc_cred_part","","7D 82 02 23 30 82 02 1F A0 82 01 DA 30 82 01 D6 30 81 E8 A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 07 03 05 00 FE DC BA 98 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A8 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A9 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 AA 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 81 E8 A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 07 03 05 00 FE DC BA 98 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A8 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A9 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 AA 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A1 03 02 01 2A A2 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A3 05 02 03 01 E2 40 A4 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 A5 0F 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23",decode_krb5_enc_cred_part,ktest_equal_enc_cred_part,krb5_free_cred_enc_part);
        /* free_cred_enc_part does not free the pointer */
        free(var);
        ktest_destroy_principal(&(ref.ticket_info[0]->client));
        ktest_destroy_principal(&(ref.ticket_info[0]->server));
        ref.ticket_info[0]->flags = 0;
        ref.ticket_info[0]->times.authtime = 0;
        ref.ticket_info[0]->times.starttime = 0;
        ref.ticket_info[0]->times.endtime = 0;
        ref.ticket_info[0]->times.renew_till = 0;
        ktest_destroy_addresses(&(ref.ticket_info[0]->caddrs));
        ref.nonce = 0;
        ref.timestamp = 0;
        ref.usec = 0;
        ktest_destroy_address(&(ref.s_address));
        ktest_destroy_address(&(ref.r_address));
        decode_run("enc_cred_part","(optionals NULL)","7D 82 01 0E 30 82 01 0A A0 82 01 06 30 82 01 02 30 15 A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 30 81 E8 A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 07 03 05 00 FE DC BA 98 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A8 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A9 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 AA 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23",decode_krb5_enc_cred_part,ktest_equal_enc_cred_part,krb5_free_cred_enc_part);
        /* free_cred_enc_part does not free the pointer */
        free(var);

        ktest_empty_cred_enc_part(&ref);
    }

    /****************************************************************/
    /* decode_krb5_error */
    {
        setup(krb5_error,ktest_make_sample_error);
        decode_run("error","","7E 81 BA 30 81 B7 A0 03 02 01 05 A1 03 02 01 1E A2 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A3 05 02 03 01 E2 40 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 05 02 03 01 E2 40 A6 03 02 01 3C A7 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A8 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A9 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 AA 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 AB 0A 1B 08 6B 72 62 35 64 61 74 61 AC 0A 04 08 6B 72 62 35 64 61 74 61",decode_krb5_error,ktest_equal_error,krb5_free_error);

        ref.ctime = 0;
        ktest_destroy_principal(&(ref.client));
        ktest_empty_data(&(ref.text));
        ktest_empty_data(&(ref.e_data));
        decode_run("error","(optionals NULL)","7E 60 30 5E A0 03 02 01 05 A1 03 02 01 1E A3 05 02 03 01 E2 40 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 05 02 03 01 E2 40 A6 03 02 01 3C A9 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 AA 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61",decode_krb5_error,ktest_equal_error,krb5_free_error);

        ktest_empty_error(&ref);
    }

    /****************************************************************/
    /* decode_krb5_authdata and krb5int_get_authdata_containee_types */
    {
        krb5_authdata **ref, **var, tmp;
        unsigned int count;
        krb5_authdatatype *types = NULL;
        ktest_make_sample_authorization_data(&ref);
        retval = krb5_data_hex_parse(&code,"30 22 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72");
        if (retval) {
            com_err("parsing authorization_data",retval,"");
            exit(1);
        }
        retval = decode_krb5_authdata(&code,&var);
        if (retval) com_err("decoding authorization_data",retval,"");
        test(ktest_equal_authorization_data(ref,var),"authorization_data\n");
        tmp.length = code.length;
        tmp.contents = (krb5_octet *)code.data;
        retval = krb5int_get_authdata_containee_types(test_context, &tmp,
                                                      &count, &types);
        if (retval) com_err("reading authdata types",retval,"");
        test(count == 2 && types[0] == 1 && types[1] == 1,
             "authorization_data(types only)\n");
        free(types);
        krb5_free_data_contents(test_context, &code);
        krb5_free_authdata(test_context, var);
        ktest_destroy_authorization_data(&ref);
    }

    /****************************************************************/
    /* decode_krb5_padata_sequence and decode_krb5_typed_data */
    {
        krb5_pa_data **ref, **var;
        ktest_make_sample_pa_data_array(&ref);
        retval = krb5_data_hex_parse(&code,"30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61");
        if (retval) {
            com_err("parsing padata_sequence",retval,"");
            exit(1);
        }
        retval = decode_krb5_padata_sequence(&code,&var);
        if (retval) com_err("decoding padata_sequence",retval,"");
        test(ktest_equal_sequence_of_pa_data(ref,var),"pa_data\n");
        krb5_free_pa_data(test_context, var);
        krb5_free_data_contents(test_context, &code);
        retval = krb5_data_hex_parse(&code,"30 24 30 10 A0 03 02 01 0D A1 09 04 07 70 61 2D 64 61 74 61 30 10 A0 03 02 01 0D A1 09 04 07 70 61 2D 64 61 74 61");
        if (retval) {
            com_err("parsing padata_sequence",retval,"");
            exit(1);
        }
        retval = decode_krb5_typed_data(&code,&var);
        if (retval) com_err("decoding typed_data",retval,"");
        test(ktest_equal_sequence_of_pa_data(ref,var),"typed_data\n");
        krb5_free_pa_data(test_context, var);
        krb5_free_data_contents(test_context, &code);
        ktest_destroy_pa_data_array(&ref);
    }

    /****************************************************************/
    /* decode_krb5_padata_sequence (empty) */
    {
        krb5_pa_data **ref, **var;
        ktest_make_sample_empty_pa_data_array(&ref);
        retval = krb5_data_hex_parse(&code,"30 00");
        if (retval) {
            com_err("parsing padata_sequence (empty)",retval,"");
            exit(1);
        }
        retval = decode_krb5_padata_sequence(&code,&var);
        if (retval) com_err("decoding padata_sequence (empty)",retval,"");
        test(ktest_equal_sequence_of_pa_data(ref,var),"pa_data (empty)\n");
        krb5_free_pa_data(test_context, var);
        krb5_free_data_contents(test_context, &code);
        ktest_destroy_pa_data_array(&ref);
    }

    /****************************************************************/
    /* decode_etype_info */
    {
        krb5_etype_info ref, var;

        ktest_make_sample_etype_info(&ref);
        retval = krb5_data_hex_parse(&code,"30 33 30 14 A0 03 02 01 00 A1 0D 04 0B 4D 6F 72 74 6F 6E 27 73 20 23 30 30 05 A0 03 02 01 01 30 14 A0 03 02 01 02 A1 0D 04 0B 4D 6F 72 74 6F 6E 27 73 20 23 32");
        if (retval) {
            com_err("krb5_decode_test", retval, "while parsing etype_info");
            exit(1);
        }
        retval = decode_krb5_etype_info(&code,&var);
        if (retval) {
            com_err("krb5_decode_test", retval, "while decoding etype_info");
        }
        test(ktest_equal_etype_info(ref,var),"etype_info\n");

        ktest_destroy_etype_info(var);
        ktest_destroy_etype_info_entry(ref[2]);      ref[2] = 0;
        ktest_destroy_etype_info_entry(ref[1]);      ref[1] = 0;
        krb5_free_data_contents(test_context, &code);

        retval = krb5_data_hex_parse(&code,"30 16 30 14 A0 03 02 01 00 A1 0D 04 0B 4D 6F 72 74 6F 6E 27 73 20 23 30");
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while parsing etype_info (only one)");
            exit(1);
        }
        retval = decode_krb5_etype_info(&code,&var);
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while decoding etype_info (only one)");
        }
        test(ktest_equal_etype_info(ref,var),"etype_info (only one)\n");

        ktest_destroy_etype_info(var);
        ktest_destroy_etype_info_entry(ref[0]);      ref[0] = 0;
        krb5_free_data_contents(test_context, &code);

        retval = krb5_data_hex_parse(&code,"30 00");
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while parsing etype_info (no info)");
            exit(1);
        }
        retval = decode_krb5_etype_info(&code,&var);
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while decoding etype_info (no info)");
        }
        test(ktest_equal_etype_info(ref,var),"etype_info (no info)\n");

        krb5_free_data_contents(test_context, &code);
        ktest_destroy_etype_info(var);
        ktest_destroy_etype_info(ref);
    }

    /****************************************************************/
    /* decode_etype_info2 */
    {
        krb5_etype_info ref, var;

        ktest_make_sample_etype_info2(&ref);
        retval = krb5_data_hex_parse(&code,"30 51 30 1E A0 03 02 01 00 A1 0D 1B 0B 4D 6F 72 74 6F 6E 27 73 20 23 30 A2 08 04 06 73 32 6B 3A 20 30 30 0F A0 03 02 01 01 A2 08 04 06 73 32 6B 3A 20 31 30 1E A0 03 02 01 02 A1 0D 1B 0B 4D 6F 72 74 6F 6E 27 73 20 23 32 A2 08 04 06 73 32 6B 3A 20 32");
        if (retval) {
            com_err("krb5_decode_test", retval, "while parsing etype_info2");
            exit(1);
        }
        retval = decode_krb5_etype_info2(&code,&var);
        if (retval) {
            com_err("krb5_decode_test", retval, "while decoding etype_info2");
        }
        test(ktest_equal_etype_info(ref,var),"etype_info2\n");

        ktest_destroy_etype_info(var);
        ktest_destroy_etype_info_entry(ref[2]);      ref[2] = 0;
        ktest_destroy_etype_info_entry(ref[1]);      ref[1] = 0;
        krb5_free_data_contents(test_context, &code);

        retval = krb5_data_hex_parse(&code,"30 20 30 1E A0 03 02 01 00 A1 0D 1B 0B 4D 6F 72 74 6F 6E 27 73 20 23 30 A2 08 04 06 73 32 6B 3A 20 30");
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while parsing etype_info2 (only one)");
            exit(1);
        }
        retval = decode_krb5_etype_info2(&code,&var);
        if (retval) {
            com_err("krb5_decode_test", retval,
                    "while decoding etype_info2 (only one)");
        }
        test(ktest_equal_etype_info(ref,var),"etype_info2 (only one)\n");

        krb5_free_data_contents(test_context, &code);
        ktest_destroy_etype_info(var);
        ktest_destroy_etype_info(ref);
    }

    /****************************************************************/
    /* decode_pa_enc_ts */
    {
        setup(krb5_pa_enc_ts,ktest_make_sample_pa_enc_ts);
        decode_run("pa_enc_ts","","30 1A A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A1 05 02 03 01 E2 40",decode_krb5_pa_enc_ts,ktest_equal_krb5_pa_enc_ts,krb5_free_pa_enc_ts);
        ref.pausec = 0;
        decode_run("pa_enc_ts (no usec)","","30 13 A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",decode_krb5_pa_enc_ts,ktest_equal_krb5_pa_enc_ts,krb5_free_pa_enc_ts);
    }

    /****************************************************************/
    /* decode_enc_data */
    {
        setup(krb5_enc_data,ktest_make_sample_enc_data);
        decode_run("enc_data","","30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_enc_data,ktest_equal_enc_data,krb5_ktest_free_enc_data);
        ref.kvno = 0xFF000000;
        decode_run("enc_data","(MSB-set kvno)","30 26 A0 03 02 01 00 A1 06 02 04 FF 00 00 00 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_enc_data,ktest_equal_enc_data,krb5_ktest_free_enc_data);
        ref.kvno = 0xFFFFFFFF;
        decode_run("enc_data","(kvno=-1)","30 23 A0 03 02 01 00 A1 03 02 01 FF A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_enc_data,ktest_equal_enc_data,krb5_ktest_free_enc_data);
        ktest_destroy_enc_data(&ref);
    }

    /****************************************************************/
    /* decode_sam_challenge_2 */
    {
        setup(krb5_sam_challenge_2,ktest_make_sample_sam_challenge_2);
        decode_run("sam_challenge_2","","30 22 A0 0D 30 0B 04 09 63 68 61 6C 6C 65 6E 67 65 A1 11 30 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",decode_krb5_sam_challenge_2,ktest_equal_sam_challenge_2,krb5_free_sam_challenge_2);
        ktest_empty_sam_challenge_2(&ref);

    }

    /****************************************************************/
    /* decode_sam_challenge_2_body */
    {
        setup(krb5_sam_challenge_2_body,ktest_make_sample_sam_challenge_2_body);
        decode_run("sam_challenge_2_body","","30 64 A0 03 02 01 2A A1 07 03 05 00 80 00 00 00 A2 0B 04 09 74 79 70 65 20 6E 61 6D 65 A4 11 04 0F 63 68 61 6C 6C 65 6E 67 65 20 6C 61 62 65 6C A5 10 04 0E 63 68 61 6C 6C 65 6E 67 65 20 69 70 73 65 A6 16 04 14 72 65 73 70 6F 6E 73 65 5F 70 72 6F 6D 70 74 20 69 70 73 65 A8 05 02 03 54 32 10 A9 03 02 01 14",decode_krb5_sam_challenge_2_body,ktest_equal_sam_challenge_2_body,krb5_free_sam_challenge_2_body);
        ktest_empty_sam_challenge_2_body(&ref);

    }

    /****************************************************************/
    /* decode_pa_for_user */
    {
        setup(krb5_pa_for_user,ktest_make_sample_pa_for_user);
        decode_run("pa_for_user","","30 4B A0 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34 A3 0A 1B 08 6B 72 62 35 64 61 74 61",decode_krb5_pa_for_user,ktest_equal_pa_for_user,krb5_free_pa_for_user);
        ktest_empty_pa_for_user(&ref);
    }

    /****************************************************************/
    /* decode_pa_s4u_x509_user */
    {
        setup(krb5_pa_s4u_x509_user,ktest_make_sample_pa_s4u_x509_user);
        decode_run("pa_s4u_x509_user","","30 68 A0 55 30 53 A0 06 02 04 00 CA 14 9A A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 12 04 10 70 61 5F 73 34 75 5F 78 35 30 39 5F 75 73 65 72 A4 07 03 05 00 80 00 00 00 A1 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",decode_krb5_pa_s4u_x509_user,ktest_equal_pa_s4u_x509_user,krb5_free_pa_s4u_x509_user);
        ktest_empty_pa_s4u_x509_user(&ref);
    }

    /****************************************************************/
    /* decode_pa_pac_req */
    {
        /* This type has no encoder and is very simple.  Test two
         * hand-generated encodings. */
        krb5_pa_pac_req *req1 = NULL, *req2 = NULL;
        code = make_data("\x30\x05\xA0\x03\x01\x01\x00", 7);
        retval = decode_krb5_pa_pac_req(&code, &req1);
        if (retval) {
            com_err(argv[0], retval, "while decoding PA-PAC-REQ");
            exit(1);
        }
        code = make_data("\x30\x05\xA0\x03\x01\x01\xFF", 7);
        retval = decode_krb5_pa_pac_req(&code, &req2);
        if (retval) {
            com_err(argv[0], retval, "while decoding PA-PAC-REQ");
            exit(1);
        }
        if (req1->include_pac != 0 || req2->include_pac != 1) {
            printf("ERROR: ");
            error_count++;
        } else {
            printf("OK: ");
        }
        printf("pa_pac_rec\n");
        free(req1);
        free(req2);
    }

    /****************************************************************/
    /* decode_ad_kdcissued */
    {
        setup(krb5_ad_kdcissued,ktest_make_sample_ad_kdcissued);
        decode_run("ad_kdcissued","","30 65 A0 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 24 30 22 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72 30 0F A0 03 02 01 01 A1 08 04 06 66 6F 6F 62 61 72",decode_krb5_ad_kdcissued,ktest_equal_ad_kdcissued,krb5_free_ad_kdcissued);
        ktest_empty_ad_kdcissued(&ref);
    }

    /****************************************************************/
    /* decode_iakerb_header */
    {
        setup(krb5_iakerb_header,ktest_make_sample_iakerb_header);
        decode_run("iakerb_header","","30 18 A1 0A 04 08 6B 72 62 35 64 61 74 61 A2 0A 04 08 6B 72 62 35 64 61 74 61",decode_krb5_iakerb_header,ktest_equal_iakerb_header,krb5_free_iakerb_header);
        ktest_empty_iakerb_header(&ref);
    }

    /****************************************************************/
    /* decode_iakerb_finished */
    {
        setup(krb5_iakerb_finished,ktest_make_sample_iakerb_finished);
        decode_run("iakerb_finished","","30 11 A1 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",decode_krb5_iakerb_finished,ktest_equal_iakerb_finished,krb5_free_iakerb_finished);
        ktest_empty_iakerb_finished(&ref);
    }

    /****************************************************************/
    /* decode_fast_response */
    {
        setup(krb5_fast_response,ktest_make_sample_fast_response);
        decode_run("fast_response","","30 81 9F A0 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A1 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A2 5B 30 59 A0 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A1 05 02 03 01 E2 40 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34 A3 03 02 01 2A",decode_krb5_fast_response,ktest_equal_fast_response,krb5_free_fast_response);
        ktest_empty_fast_response(&ref);
    }

    /****************************************************************/
    /* decode_pa_fx_fast_reply */
    {
        setup(krb5_enc_data,ktest_make_sample_enc_data);
        decode_run("pa_fx_fast_reply","","A0 29 30 27 A0 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_pa_fx_fast_reply,ktest_equal_enc_data,krb5_free_enc_data);
        ktest_destroy_enc_data(&ref);
    }

    /****************************************************************/
    /* decode_krb5_otp_tokeninfo */
    {
        setup(krb5_otp_tokeninfo,ktest_make_minimal_otp_tokeninfo);
        decode_run("otp_tokeninfo","(optionals NULL)","30 07 80 05 00 00 00 00 00",decode_krb5_otp_tokeninfo,ktest_equal_otp_tokeninfo,k5_free_otp_tokeninfo);
        ktest_empty_otp_tokeninfo(&ref);
    }
    {
        setup(krb5_otp_tokeninfo,ktest_make_maximal_otp_tokeninfo);
        decode_run("otp_tokeninfo","","30 72 80 05 00 77 00 00 00 81 0B 45 78 61 6D 70 6C 65 63 6F 72 70 82 05 68 61 72 6B 21 83 01 0A 84 01 02 85 09 79 6F 75 72 74 6F 6B 65 6E 86 28 75 72 6E 3A 69 65 74 66 3A 70 61 72 61 6D 73 3A 78 6D 6C 3A 6E 73 3A 6B 65 79 70 72 6F 76 3A 70 73 6B 63 3A 68 6F 74 70 A7 16 30 0B 06 09 60 86 48 01 65 03 04 02 01 30 07 06 05 2B 0E 03 02 1A 88 02 03 E8",decode_krb5_otp_tokeninfo,ktest_equal_otp_tokeninfo,k5_free_otp_tokeninfo);
        ktest_empty_otp_tokeninfo(&ref);
    }

    /****************************************************************/
    /* decode_krb5_pa_otp_challenge */
    {
        setup(krb5_pa_otp_challenge,ktest_make_minimal_pa_otp_challenge);
        decode_run("pa_otp_challenge","(optionals NULL)","30 15 80 08 6D 69 6E 6E 6F 6E 63 65 A2 09 30 07 80 05 00 00 00 00 00",decode_krb5_pa_otp_challenge,ktest_equal_pa_otp_challenge,k5_free_pa_otp_challenge);
        ktest_empty_pa_otp_challenge(&ref);
    }
    {
        setup(krb5_pa_otp_challenge,ktest_make_maximal_pa_otp_challenge);
        decode_run("pa_otp_challenge","","30 81 A5 80 08 6D 61 78 6E 6F 6E 63 65 81 0B 74 65 73 74 73 65 72 76 69 63 65 A2 7D 30 07 80 05 00 00 00 00 00 30 72 80 05 00 77 00 00 00 81 0B 45 78 61 6D 70 6C 65 63 6F 72 70 82 05 68 61 72 6B 21 83 01 0A 84 01 02 85 09 79 6F 75 72 74 6F 6B 65 6E 86 28 75 72 6E 3A 69 65 74 66 3A 70 61 72 61 6D 73 3A 78 6D 6C 3A 6E 73 3A 6B 65 79 70 72 6F 76 3A 70 73 6B 63 3A 68 6F 74 70 A7 16 30 0B 06 09 60 86 48 01 65 03 04 02 01 30 07 06 05 2B 0E 03 02 1A 88 02 03 E8 83 07 6B 65 79 73 61 6C 74 84 04 31 32 33 34",decode_krb5_pa_otp_challenge,ktest_equal_pa_otp_challenge,k5_free_pa_otp_challenge);
        ktest_empty_pa_otp_challenge(&ref);
    }

    /****************************************************************/
    /* decode_krb5_pa_otp_req */
    {
        setup(krb5_pa_otp_req,ktest_make_minimal_pa_otp_req);
        decode_run("pa_otp_req","(optionals NULL)","30 2C 80 05 00 00 00 00 00 A2 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_pa_otp_req,ktest_equal_pa_otp_req,k5_free_pa_otp_req);
        ktest_empty_pa_otp_req(&ref);
    }
    {
        setup(krb5_pa_otp_req,ktest_make_maximal_pa_otp_req);
        decode_run("pa_otp_req","","30 81 B9 80 05 00 60 00 00 00 81 05 6E 6F 6E 63 65 A2 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A3 0B 06 09 60 86 48 01 65 03 04 02 01 84 02 03 E8 85 05 66 72 6F 67 73 86 0A 6D 79 66 69 72 73 74 70 69 6E 87 05 68 61 72 6B 21 88 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A 89 03 33 34 36 8A 01 02 8B 09 79 6F 75 72 74 6F 6B 65 6E 8C 28 75 72 6E 3A 69 65 74 66 3A 70 61 72 61 6D 73 3A 78 6D 6C 3A 6E 73 3A 6B 65 79 70 72 6F 76 3A 70 73 6B 63 3A 68 6F 74 70 8D 0B 45 78 61 6D 70 6C 65 63 6F 72 70",decode_krb5_pa_otp_req,ktest_equal_pa_otp_req,k5_free_pa_otp_req);
        ktest_empty_pa_otp_req(&ref);
    }

    /****************************************************************/
    /* decode_krb5_pa_otp_enc_req */
    {
        setup(krb5_data,ktest_make_sample_data);
        decode_run("pa_otp_enc_req","","30 0A 80 08 6B 72 62 35 64 61 74 61",decode_krb5_pa_otp_enc_req,ktest_equal_data,krb5_free_data);
        ktest_empty_data(&ref);
    }

    /****************************************************************/
    /* decode_krb5_kkdcp_message */
    {
        setup(krb5_kkdcp_message,ktest_make_sample_kkdcp_message);
        decode_run("kkdcp_message","","30 82 01 FC A0 82 01 EC 04 82 01 E8 6A 82 01 E4 30 82 01 E0 A1 03 02 01 05 A2 03 02 01 0A A3 26 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 A4 82 01 AA 30 82 01 A6 A0 07 03 05 00 FE DC BA 98 A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A2 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A3 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A4 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A5 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A6 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A7 03 02 01 2A A8 08 30 06 02 01 00 02 01 01 A9 20 30 1E 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 30 0D A0 03 02 01 02 A1 06 04 04 12 D0 00 23 AA 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 AB 81 BF 30 81 BC 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 61 5C 30 5A A0 03 02 01 05 A1 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A2 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65 A1 0A 1B 08 6B 72 62 35 64 61 74 61",decode_krb5_kkdcp_message,ktest_equal_kkdcp_message,ktest_free_kkdcp_message);
        ktest_empty_kkdcp_message(&ref);
    }

    /****************************************************************/
    /* decode_krb5_cammac */
    {
        setup(krb5_cammac,ktest_make_minimal_cammac);
        decode_run("cammac","(optionals NULL)","30 12 A0 10 30 0E 30 0C A0 03 02 01 01 A1 05 04 03 61 64 31",decode_krb5_cammac,ktest_equal_cammac,k5_free_cammac);
        ktest_empty_cammac(&ref);
    }
    {
        setup(krb5_cammac,ktest_make_maximal_cammac);
        decode_run("cammac","","30 81 F2 A0 1E 30 1C 30 0C A0 03 02 01 01 A1 05 04 03 61 64 31 30 0C A0 03 02 01 02 A1 05 04 03 61 64 32 A1 3D 30 3B A0 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A1 03 02 01 05 A2 03 02 01 10 A3 13 30 11 A0 03 02 01 01 A1 0A 04 08 63 6B 73 75 6D 6B 64 63 A2 3D 30 3B A0 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A1 03 02 01 05 A2 03 02 01 10 A3 13 30 11 A0 03 02 01 01 A1 0A 04 08 63 6B 73 75 6D 73 76 63 A3 52 30 50 30 13 A3 11 30 0F A0 03 02 01 01 A1 08 04 06 63 6B 73 75 6D 31 30 39 A0 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61 A1 03 02 01 05 A2 03 02 01 10 A3 11 30 0F A0 03 02 01 01 A1 08 04 06 63 6B 73 75 6D 32",decode_krb5_cammac,ktest_equal_cammac,k5_free_cammac);
        ktest_empty_cammac(&ref);
    }

    /****************************************************************/
    /* decode_krb5_secure_cookie */
    {
        setup(krb5_secure_cookie,ktest_make_sample_secure_cookie);
        decode_run("secure_cookie","","30 2C 02 04 2D F8 02 25 30 24 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61 30 10 A1 03 02 01 0D A2 09 04 07 70 61 2D 64 61 74 61",decode_krb5_secure_cookie,ktest_equal_secure_cookie,k5_free_secure_cookie);
        ktest_empty_secure_cookie(&ref);
    }

    /****************************************************************/
    /* decode_krb5_spake_factor */
    {
        setup(krb5_spake_factor,ktest_make_minimal_spake_factor);
        decode_run("spake_factor","(optionals NULL)","30 05 A0 03 02 01 01",decode_krb5_spake_factor,ktest_equal_spake_factor,k5_free_spake_factor);
        ktest_empty_spake_factor(&ref);
    }
    {
        setup(krb5_spake_factor,ktest_make_maximal_spake_factor);
        decode_run("spake_factor","","30 0E A0 03 02 01 02 A1 07 04 05 66 64 61 74 61",decode_krb5_spake_factor,ktest_equal_spake_factor,k5_free_spake_factor);
        ktest_empty_spake_factor(&ref);
    }

    /****************************************************************/
    /* decode_krb5_pa_spake */
    {
        setup(krb5_pa_spake,ktest_make_support_pa_spake);
        decode_run("pa_spake","(support)","A0 0C 30 0A A0 08 30 06 02 01 01 02 01 02",decode_krb5_pa_spake,ktest_equal_pa_spake,k5_free_pa_spake);
        ktest_empty_pa_spake(&ref);
    }
    {
        setup(krb5_pa_spake,ktest_make_challenge_pa_spake);
        decode_run("pa_spake","(challenge)","A1 2D 30 2B A0 03 02 01 01 A1 09 04 07 54 20 76 61 6C 75 65 A2 19 30 17 30 05 A0 03 02 01 01 30 0E A0 03 02 01 02 A1 07 04 05 66 64 61 74 61",decode_krb5_pa_spake,ktest_equal_pa_spake,k5_free_pa_spake);
        ktest_empty_pa_spake(&ref);
    }
    {
        setup(krb5_pa_spake,ktest_make_response_pa_spake);
        decode_run("pa_spake","(response)","A2 34 30 32 A0 09 04 07 53 20 76 61 6C 75 65 A1 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_pa_spake,ktest_equal_pa_spake,k5_free_pa_spake);
        ktest_empty_pa_spake(&ref);
    }
    {
        setup(krb5_pa_spake,ktest_make_encdata_pa_spake);
        decode_run("pa_spake","(encdata)","A3 25 30 23 A0 03 02 01 00 A1 03 02 01 05 A2 17 04 15 6B 72 62 41 53 4E 2E 31 20 74 65 73 74 20 6D 65 73 73 61 67 65",decode_krb5_pa_spake,ktest_equal_pa_spake,k5_free_pa_spake);
        ktest_empty_pa_spake(&ref);
    }

#ifndef DISABLE_PKINIT

    /****************************************************************/
    /* decode_krb5_pa_pk_as_req */
    {
        setup(krb5_pa_pk_as_req,ktest_make_sample_pa_pk_as_req);
        decode_run("krb5_pa_pk_as_req","","30 38 80 08 6B 72 62 35 64 61 74 61 A1 22 30 20 30 1E 80 08 6B 72 62 35 64 61 74 61 81 08 6B 72 62 35 64 61 74 61 82 08 6B 72 62 35 64 61 74 61 82 08 6B 72 62 35 64 61 74 61",
                   acc.decode_krb5_pa_pk_as_req,
                   ktest_equal_pa_pk_as_req,ktest_free_pa_pk_as_req);
        ktest_empty_pa_pk_as_req(&ref);
    }

    /****************************************************************/
    /* decode_krb5_pa_pk_as_rep */
    {
        setup(krb5_pa_pk_as_rep,ktest_make_sample_pa_pk_as_rep_dhInfo);
        decode_run("krb5_pa_pk_as_rep","(dhInfo)","A0 28 30 26 80 08 6B 72 62 35 64 61 74 61 A1 0A 04 08 6B 72 62 35 64 61 74 61 A2 0E 30 0C A0 0A 06 08 6B 72 62 35 64 61 74 61",
                   acc.decode_krb5_pa_pk_as_rep,
                   ktest_equal_pa_pk_as_rep,ktest_free_pa_pk_as_rep);
        ktest_empty_pa_pk_as_rep(&ref);
    }
    {
        setup(krb5_pa_pk_as_rep,ktest_make_sample_pa_pk_as_rep_encKeyPack);
        decode_run("krb5_pa_pk_as_rep","(encKeyPack)","81 08 6B 72 62 35 64 61 74 61",
                   acc.decode_krb5_pa_pk_as_rep,
                   ktest_equal_pa_pk_as_rep,ktest_free_pa_pk_as_rep);
        ktest_empty_pa_pk_as_rep(&ref);
    }

    /****************************************************************/
    /* decode_krb5_auth_pack */
    {
        setup(krb5_auth_pack,ktest_make_sample_auth_pack);
        decode_run("krb5_auth_pack","","30 81 85 A0 35 30 33 A0 05 02 03 01 E2 40 A1 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A A2 03 02 01 2A A3 06 04 04 31 32 33 34 A4 0A 04 08 6B 72 62 35 64 61 74 61 A1 08 04 06 70 76 61 6C 75 65 A2 24 30 22 30 13 06 09 2A 86 48 86 F7 12 01 02 02 04 06 70 61 72 61 6D 73 30 0B 06 09 2A 86 48 86 F7 12 01 02 02 A3 0A 04 08 6B 72 62 35 64 61 74 61 A4 10 30 0E 30 0C A0 0A 06 08 6B 72 62 35 64 61 74 61",
                   acc.decode_krb5_auth_pack,
                   ktest_equal_auth_pack,ktest_free_auth_pack);
        ktest_empty_auth_pack(&ref);
    }

    /****************************************************************/
    /* decode_krb5_kdc_dh_key_info */
    {
        setup(krb5_kdc_dh_key_info,ktest_make_sample_kdc_dh_key_info);
        decode_run("krb5_kdc_dh_key_info","","30 25 A0 0B 03 09 00 6B 72 62 35 64 61 74 61 A1 03 02 01 2A A2 11 18 0F 31 39 39 34 30 36 31 30 30 36 30 33 31 37 5A",
                   acc.decode_krb5_kdc_dh_key_info,
                   ktest_equal_kdc_dh_key_info,ktest_free_kdc_dh_key_info);
        ktest_empty_kdc_dh_key_info(&ref);
    }

    /****************************************************************/
    /* decode_krb5_reply_key_pack */
    {
        setup(krb5_reply_key_pack,ktest_make_sample_reply_key_pack);
        decode_run("krb5_reply_key_pack","","30 26 A0 13 30 11 A0 03 02 01 01 A1 0A 04 08 31 32 33 34 35 36 37 38 A1 0F 30 0D A0 03 02 01 01 A1 06 04 04 31 32 33 34",
                   acc.decode_krb5_reply_key_pack,
                   ktest_equal_reply_key_pack,ktest_free_reply_key_pack);
        ktest_empty_reply_key_pack(&ref);
    }

    /****************************************************************/
    /* decode_krb5_principal_name */
    /* We have no encoder for this type (KerberosName from RFC 4556); the
     * encoding is hand-generated. */
    {
        krb5_principal ref, var;

        ktest_make_sample_principal(&ref);
        decode_run("krb5_principal_name","","30 2E A0 10 1B 0E 41 54 48 45 4E 41 2E 4D 49 54 2E 45 44 55 A1 1A 30 18 A0 03 02 01 01 A1 11 30 0F 1B 06 68 66 74 73 61 69 1B 05 65 78 74 72 61",
                   acc.decode_krb5_principal_name,equal_principal,krb5_free_principal);
    }

#endif /* not DISABLE_PKINIT */

#ifdef ENABLE_LDAP
    /* ldap sequence_of_keys */
    {
        setup(ldap_seqof_key_data,ktest_make_sample_ldap_seqof_key_data);
        decode_run("ldap_seqof_key_data","","30 81 87 A0 03 02 01 01 A1 03 02 01 01 A2 03 02 01 2A A3 03 02 01 0E A4 71 30 6F 30 23 A0 10 30 0E A0 03 02 01 00 A1 07 04 05 73 61 6C 74 30 A1 0F 30 0D A0 03 02 01 02 A1 06 04 04 6B 65 79 30 30 23 A0 10 30 0E A0 03 02 01 01 A1 07 04 05 73 61 6C 74 31 A1 0F 30 0D A0 03 02 01 02 A1 06 04 04 6B 65 79 31 30 23 A0 10 30 0E A0 03 02 01 02 A1 07 04 05 73 61 6C 74 32 A1 0F 30 0D A0 03 02 01 02 A1 06 04 04 6B 65 79 32",acc.asn1_ldap_decode_sequence_of_keys,ktest_equal_ldap_sequence_of_keys,ktest_empty_ldap_seqof_key_data);
        ktest_empty_ldap_seqof_key_data(test_context, &ref);
    }

#endif

    krb5_free_context(test_context);
    exit(error_count);
    return(error_count);
}


void krb5_ktest_free_enc_data(krb5_context context, krb5_enc_data *val)
{
    if (val) {
        krb5_free_data_contents(context, &(val->ciphertext));
        free(val);
    }
}

#ifndef DISABLE_PKINIT

/* Glue function to make ktest_equal_principal_data look like what decode_run
 * expects. */
static int
equal_principal(krb5_principal *ref, krb5_principal var)
{
    return ktest_equal_principal_data(*ref, var);
}

static void
ktest_free_auth_pack(krb5_context context, krb5_auth_pack *val)
{
    if (val)
        ktest_empty_auth_pack(val);
    free(val);
}

static void
ktest_free_kdc_dh_key_info(krb5_context context, krb5_kdc_dh_key_info *val)
{
    if (val)
        ktest_empty_kdc_dh_key_info(val);
    free(val);
}

static void
ktest_free_pa_pk_as_req(krb5_context context, krb5_pa_pk_as_req *val)
{
    if (val)
        ktest_empty_pa_pk_as_req(val);
    free(val);
}

static void
ktest_free_pa_pk_as_rep(krb5_context context, krb5_pa_pk_as_rep *val)
{
    if (val)
        ktest_empty_pa_pk_as_rep(val);
    free(val);
}

static void
ktest_free_reply_key_pack(krb5_context context, krb5_reply_key_pack *val)
{
    if (val)
        ktest_empty_reply_key_pack(val);
    free(val);
}

#endif /* not DISABLE_PKINIT */

static void
ktest_free_kkdcp_message(krb5_context context,
                         krb5_kkdcp_message *val)
{
    if (val)
        ktest_empty_kkdcp_message(val);
    free(val);
}
