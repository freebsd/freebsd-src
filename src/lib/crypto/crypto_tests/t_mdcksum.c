/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/crypto_tests/t_mdcksum.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

/* Test checksum and checksum compatability for rsa-md[4,5]-des. */

#ifndef MD
#define MD      5
#endif  /* MD */

#include "k5-int.h"
#if     MD == 4
#include "rsa-md4.h"
#endif  /* MD == 4 */
#if     MD == 5
#include "rsa-md5.h"
#endif  /* MD == 5 */
#include "des_int.h"

#define MD5_K5BETA_COMPAT
#define MD4_K5BETA_COMPAT

#if     MD == 4
#define CONFOUNDER_LENGTH       RSA_MD4_DES_CONFOUND_LENGTH
#define NEW_CHECKSUM_LENGTH     NEW_RSA_MD4_DES_CKSUM_LENGTH
#define OLD_CHECKSUM_LENGTH     OLD_RSA_MD4_DES_CKSUM_LENGTH
#define CHECKSUM_TYPE           CKSUMTYPE_RSA_MD4_DES
#ifdef  MD4_K5BETA_COMPAT
#define K5BETA_COMPAT   1
#else   /* MD4_K5BETA_COMPAT */
#undef  K5BETA_COMPAT
#endif  /* MD4_K5BETA_COMPAT */
#define CKSUM_FUNCTION          krb5_md4_crypto_sum_func
#define COMPAT_FUNCTION         krb5_md4_crypto_compat_sum_func
#define VERIFY_FUNCTION         krb5_md4_crypto_verify_func
#endif  /* MD == 4 */

#if     MD == 5
#define CONFOUNDER_LENGTH       RSA_MD5_DES_CONFOUND_LENGTH
#define NEW_CHECKSUM_LENGTH     NEW_RSA_MD5_DES_CKSUM_LENGTH
#define OLD_CHECKSUM_LENGTH     OLD_RSA_MD5_DES_CKSUM_LENGTH
#define CHECKSUM_TYPE           CKSUMTYPE_RSA_MD5_DES
#ifdef  MD5_K5BETA_COMPAT
#define K5BETA_COMPAT   1
#else   /* MD5_K5BETA_COMPAT */
#undef  K5BETA_COMPAT
#endif  /* MD5_K5BETA_COMPAT */
#define CKSUM_FUNCTION          krb5_md5_crypto_sum_func
#define COMPAT_FUNCTION         krb5_md5_crypto_compat_sum_func
#define VERIFY_FUNCTION         krb5_md5_crypto_verify_func
#endif  /* MD == 5 */

static void
print_checksum(text, number, message, checksum)
    char       *text;
    int        number;
    char       *message;
    krb5_checksum      *checksum;
{
    int i;

    printf("%s MD%d checksum(\"%s\") = ", text, number, message);
    for (i=0; i<checksum->length; i++)
        printf("%02x", checksum->contents[i]);
    printf("\n");
}

/*
 * Test the checksum verification of Old Style (tm) and correct RSA-MD[4,5]-DES
 * checksums.
 */
int
main(argc, argv)
    int argc;
    char **argv;
{
    int                   msgindex;
    krb5_context          kcontext;
    krb5_encrypt_block    encblock;
    krb5_keyblock         keyblock;
    krb5_error_code       kret;
    krb5_checksum         oldstyle_checksum;
    krb5_checksum         newstyle_checksum;
    krb5_data             pwdata;
    char                  *pwd;

    pwd = "test password";
    pwdata.length = strlen(pwd);
    pwdata.data = pwd;
    krb5_use_enctype(kcontext, &encblock, DEFAULT_KDC_ENCTYPE);
    if ((kret = mit_des_string_to_key(&encblock, &keyblock, &pwdata, NULL))) {
        printf("mit_des_string_to_key choked with %d\n", kret);
        return(kret);
    }
    if ((kret = mit_des_process_key(&encblock, &keyblock))) {
        printf("mit_des_process_key choked with %d\n", kret);
        return(kret);
    }

    oldstyle_checksum.length = OLD_CHECKSUM_LENGTH;
    if (!(oldstyle_checksum.contents = (krb5_octet *) malloc(OLD_CHECKSUM_LENGTH))) {
        printf("cannot get memory for old style checksum\n");
        return(ENOMEM);
    }
    newstyle_checksum.length = NEW_CHECKSUM_LENGTH;
    if (!(newstyle_checksum.contents = (krb5_octet *)
          malloc(NEW_CHECKSUM_LENGTH))) {
        printf("cannot get memory for new style checksum\n");
        return(ENOMEM);
    }
    for (msgindex = 1; msgindex < argc; msgindex++) {
        if ((kret = CKSUM_FUNCTION(argv[msgindex],
                                   strlen(argv[msgindex]),
                                   (krb5_pointer) keyblock.contents,
                                   keyblock.length,
                                   &newstyle_checksum))) {
            printf("krb5_calculate_checksum choked with %d\n", kret);
            break;
        }
        print_checksum("correct", MD, argv[msgindex], &newstyle_checksum);
#ifdef  K5BETA_COMPAT
        if ((kret = COMPAT_FUNCTION(argv[msgindex],
                                    strlen(argv[msgindex]),
                                    (krb5_pointer) keyblock.contents,
                                    keyblock.length,
                                    &oldstyle_checksum))) {
            printf("old style calculate_checksum choked with %d\n", kret);
            break;
        }
        print_checksum("old", MD, argv[msgindex], &oldstyle_checksum);
#endif  /* K5BETA_COMPAT */
        if ((kret = VERIFY_FUNCTION(&newstyle_checksum,
                                    argv[msgindex],
                                    strlen(argv[msgindex]),
                                    (krb5_pointer) keyblock.contents,
                                    keyblock.length))) {
            printf("verify on new checksum choked with %d\n", kret);
            break;
        }
        printf("Verify succeeded for \"%s\"\n", argv[msgindex]);
#ifdef  K5BETA_COMPAT
        if ((kret = VERIFY_FUNCTION(&oldstyle_checksum,
                                    argv[msgindex],
                                    strlen(argv[msgindex]),
                                    (krb5_pointer) keyblock.contents,
                                    keyblock.length))) {
            printf("verify on old checksum choked with %d\n", kret);
            break;
        }
        printf("Compatible checksum verify succeeded for \"%s\"\n",
               argv[msgindex]);
#endif  /* K5BETA_COMPAT */
        newstyle_checksum.contents[0]++;
        if (!(kret = VERIFY_FUNCTION(&newstyle_checksum,
                                     argv[msgindex],
                                     strlen(argv[msgindex]),
                                     (krb5_pointer) keyblock.contents,
                                     keyblock.length))) {
            printf("verify on new checksum should have choked\n");
            break;
        }
        printf("Verify of bad checksum OK for \"%s\"\n", argv[msgindex]);
#ifdef  K5BETA_COMPAT
        oldstyle_checksum.contents[0]++;
        if (!(kret = VERIFY_FUNCTION(&oldstyle_checksum,
                                     argv[msgindex],
                                     strlen(argv[msgindex]),
                                     (krb5_pointer) keyblock.contents,
                                     keyblock.length))) {
            printf("verify on old checksum should have choked\n");
            break;
        }
        printf("Compatible checksum verify of altered checksum OK for \"%s\"\n",
               argv[msgindex]);
#endif  /* K5BETA_COMPAT */
        kret = 0;
    }
    if (!kret)
        printf("%d tests passed successfully for MD%d checksum\n", argc-1, MD);
    return(kret);
}
