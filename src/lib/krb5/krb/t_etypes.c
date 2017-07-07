/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_etypes.c - test program for krb5int_parse_enctype_list */
/*
 * Copyright 2009  by the Massachusetts Institute of Technology.
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
#include <stdio.h>
#include "com_err.h"

static struct {
    const char *str;
    krb5_enctype defaults[64];
    krb5_enctype expected_noweak[64];
    krb5_enctype expected[64];
    krb5_error_code expected_err_noweak;
    krb5_error_code expected_err_weak;
} tests[] = {
    /* Empty string, unused default list */
    { "",
      { ENCTYPE_DES_CBC_CRC, 0 },
      { 0 },
      { 0 },
      0, 0
    },
    /* Single weak enctype */
    { "des-cbc-md4",
      { 0 },
      { 0 },
      { ENCTYPE_DES_CBC_MD4, 0 },
      0, 0
    },
    /* Single non-weak enctype */
    { "aes128-cts-hmac-sha1-96",
      { 0 },
      { ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },
      { ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },
      0, 0
    },
    /* Two enctypes, one an alias, one weak */
    { "rc4-hmac des-cbc-md5",
      { 0 },
      { ENCTYPE_ARCFOUR_HMAC, 0 },
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES_CBC_MD5, 0 },
      0, 0
    },
    /* Three enctypes, all weak, case variation, funky separators */
    { "  deS-HMac-shA1 , arCFour-hmaC-mD5-exp\tdeS3-Cbc-RAw\n",
      { 0 },
      { 0 },
      { ENCTYPE_DES_HMAC_SHA1, ENCTYPE_ARCFOUR_HMAC_EXP,
        ENCTYPE_DES3_CBC_RAW, 0 },
      0, 0
    },
    /* Default set with enctypes added (one weak in each pair) */
    { "DEFAULT des-cbc-raw +des3-hmac-sha1",
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_ARCFOUR_HMAC_EXP, 0 },
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES3_CBC_SHA1, 0 },
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_ARCFOUR_HMAC_EXP,
        ENCTYPE_DES_CBC_RAW, ENCTYPE_DES3_CBC_SHA1, 0 },
      0, 0
    },
    /* Default set with enctypes removed */
    { "default -aes128-cts -des-hmac-sha1",
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_DES_CBC_MD5, ENCTYPE_DES_HMAC_SHA1, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_DES_CBC_MD5, 0 },
      0, 0
    },
    /* Family followed by enctype */
    { "aes des3-cbc-sha1-kd",
      { 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_AES256_CTS_HMAC_SHA384_192, ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        ENCTYPE_DES3_CBC_SHA1, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_AES256_CTS_HMAC_SHA384_192, ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        ENCTYPE_DES3_CBC_SHA1, 0 },
      0, 0
    },
    /* Family with enctype removed */
    { "camellia -camellia256-cts-cmac",
      { 0 },
      { ENCTYPE_CAMELLIA128_CTS_CMAC, 0 },
      { ENCTYPE_CAMELLIA128_CTS_CMAC, 0 }
    },
    /* Enctype followed by two families */
    { "+rc4-hmAC des3 +des",
      { 0 },
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES3_CBC_SHA1, 0 },
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES3_CBC_SHA1, ENCTYPE_DES_CBC_CRC,
        ENCTYPE_DES_CBC_MD5, ENCTYPE_DES_CBC_MD4 },
      0, 0
    },
    /* Default set with family added and enctype removed */
    { "DEFAULT +aes -arcfour-hmac-md5",
      { ENCTYPE_ARCFOUR_HMAC, ENCTYPE_DES3_CBC_SHA1, ENCTYPE_DES_CBC_CRC, 0 },
      { ENCTYPE_DES3_CBC_SHA1, ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        ENCTYPE_AES128_CTS_HMAC_SHA1_96, ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        ENCTYPE_AES128_CTS_HMAC_SHA256_128, 0 },
      { ENCTYPE_DES3_CBC_SHA1, ENCTYPE_DES_CBC_CRC,
        ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_AES256_CTS_HMAC_SHA384_192, ENCTYPE_AES128_CTS_HMAC_SHA256_128,
        0 },
      0, 0
    },
    /* Default set with families removed and enctypes added (one redundant) */
    { "DEFAULT -des -des3 rc4-hmac rc4-hmac-exp",
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_DES3_CBC_SHA1, ENCTYPE_ARCFOUR_HMAC,
        ENCTYPE_DES_CBC_CRC, ENCTYPE_DES_CBC_MD5, ENCTYPE_DES_CBC_MD4, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_ARCFOUR_HMAC, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_ARCFOUR_HMAC, ENCTYPE_ARCFOUR_HMAC_EXP, 0 },
      0, 0
    },
    /* Default set with family moved to front */
    { "des3 +DEFAULT",
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES128_CTS_HMAC_SHA1_96,
        ENCTYPE_DES3_CBC_SHA1, 0 },
      { ENCTYPE_DES3_CBC_SHA1, ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },
      { ENCTYPE_DES3_CBC_SHA1, ENCTYPE_AES256_CTS_HMAC_SHA1_96,
        ENCTYPE_AES128_CTS_HMAC_SHA1_96, 0 },
      0, 0
    },
    /* Two families with default set removed (exotic case), enctype added */
    { "aes +rc4 -DEFaulT des3-hmac-sha1",
      { ENCTYPE_AES128_CTS_HMAC_SHA1_96, ENCTYPE_DES3_CBC_SHA1,
        ENCTYPE_ARCFOUR_HMAC, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        ENCTYPE_AES128_CTS_HMAC_SHA256_128, ENCTYPE_DES3_CBC_SHA1, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_AES256_CTS_HMAC_SHA384_192,
        ENCTYPE_AES128_CTS_HMAC_SHA256_128, ENCTYPE_DES3_CBC_SHA1, 0 },
      0, 0
    },
    /* Test krb5_set_default_in_tkt_ktypes */
    { NULL,
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_DES_CBC_CRC, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, 0 },
      { ENCTYPE_AES256_CTS_HMAC_SHA1_96, ENCTYPE_DES_CBC_CRC, 0 },
      0, 0
    },
    /* Should get KRB5_CONFIG_ETYPE_NOSUPP if app-provided list has no strong
     * enctypes and allow_weak_crypto=false. */
    { NULL,
      { ENCTYPE_DES_CBC_CRC, 0 },
      { 0 },
      { ENCTYPE_DES_CBC_CRC, 0 },
      KRB5_CONFIG_ETYPE_NOSUPP, 0
    },
    /* Should get EINVAL if app provides an empty list. */
    { NULL,
      { 0 },
      { 0 },
      { 0 },
      EINVAL, EINVAL
    }
};

static void
show_enctypes(krb5_context ctx, krb5_enctype *list)
{
    unsigned int i;

    for (i = 0; list[i]; i++) {
        fprintf(stderr, "%d", (int) list[i]);
        if (list[i + 1])
            fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");
}

static void
compare(krb5_context ctx, krb5_enctype *result, krb5_enctype *expected,
        const char *profstr, krb5_boolean weak)
{
    unsigned int i;

    if (!result)
        return;
    for (i = 0; result[i]; i++) {
        if (result[i] != expected[i])
            break;
    }
    if (!result[i] && !expected[i]) /* Success! */
        return;
    if (profstr != NULL)
        fprintf(stderr, "Unexpected result while parsing: %s\n", profstr);
    fprintf(stderr, "Expected: ");
    show_enctypes(ctx, expected);
    fprintf(stderr, "Result: ");
    show_enctypes(ctx, result);
    fprintf(stderr, "allow_weak_crypto was %s\n", weak ? "true" : "false");
    exit(1);
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_error_code ret, expected_err;
    krb5_enctype *list;
    krb5_boolean weak;
    unsigned int i;
    char *copy;

    ret = krb5_init_context(&ctx);
    if (ret) {
        com_err("krb5_init_context", ret, "");
        return 2;
    }
    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        for (weak = FALSE; weak <= TRUE; weak++) {
            ctx->allow_weak_crypto = weak;
            if (weak)
                expected_err = tests[i].expected_err_weak;
            else
                expected_err = tests[i].expected_err_noweak;

            if (tests[i].str != NULL) {
                copy = strdup(tests[i].str);
                ret = krb5int_parse_enctype_list(ctx, "", copy,
                                                 tests[i].defaults, &list);
                if (ret != expected_err) {
                    com_err("krb5int_parse_enctype_list", ret, "");
                    return 2;
                }
            } else {
                /* No string; test the filtering on the set_default_etype_var
                 * instead. */
                copy = NULL;
                list = NULL;
                ret = krb5_set_default_in_tkt_ktypes(ctx, tests[i].defaults);
                if (ret != expected_err) {
                    com_err("krb5_set_default_in_tkt_ktypes", ret, "");
                    return 2;
                }
            }
            if (!expected_err) {
                compare(ctx, tests[i].str ? list : ctx->in_tkt_etypes,
                        (weak) ? tests[i].expected : tests[i].expected_noweak,
                        tests[i].str, weak);
            }
            free(copy);
            free(list);
            if (!tests[i].str)
                krb5_set_default_in_tkt_ktypes(ctx, NULL);
        }
    }

    krb5_free_context(ctx);

    return 0;
}
