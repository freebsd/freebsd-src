/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/asn.1/make-vectors.c - Generate ASN.1 test vectors using asn1c */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
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

/*
 * This program generates test vectors using asn1c, to be included in other
 * test programs which exercise the krb5 ASN.1 encoder and decoder functions.
 * It is intended to be used via "make test-vectors".  Currently, test vectors
 * are only generated for a subset of newer ASN.1 objects.
 */

#include <PrincipalName.h>
#include <KRB5PrincipalName.h>
#include <OtherInfo.h>
#include <PkinitSuppPubInfo.h>
#include <OTP-TOKENINFO.h>
#include <PA-OTP-CHALLENGE.h>
#include <PA-OTP-REQUEST.h>
#include <PA-OTP-ENC-REQUEST.h>
#include <AD-CAMMAC.h>

static unsigned char buf[8192];
static size_t buf_pos;

/* PrincipalName and KRB5PrincipalName */
static KerberosString_t comp_1 = { "hftsai", 6 };
static KerberosString_t comp_2 = { "extra", 5 };
static KerberosString_t *comps[] = { &comp_1, &comp_2 };
static PrincipalName_t princ = { 1, { comps, 2, 2 } };
static KRB5PrincipalName_t krb5princ = { { "ATHENA.MIT.EDU", 14 },
                                         { 1, { comps, 2, 2 } } };

/* OtherInfo */
static unsigned int krb5_arcs[] = { 1, 2, 840, 113554, 1, 2, 2 };
static OCTET_STRING_t krb5data_ostring = { "krb5data", 8 };
static OtherInfo_t other_info = {
    { 0 }, { 0 }, { 0 },        /* Initialized in main() */
    &krb5data_ostring, NULL
};

/* PkinitSuppPubInfo */
static PkinitSuppPubInfo_t supp_pub_info = { 1, { "krb5data", 8 },
                                             { "krb5data", 8 } };

/* Minimal OTP-TOKENINFO */
static OTP_TOKENINFO_t token_info_1 = { { "\0\0\0\0", 4, 0 } };

/* Maximal OTP-TOKENINFO */
static UTF8String_t vendor = { "Examplecorp", 11 };
static OCTET_STRING_t challenge = { "hark!", 5 };
static Int32_t otp_length = 10;
static OTPFormat_t otp_format; /* Initialized to 2 in main(). */
static OCTET_STRING_t token_id = { "yourtoken", 9 };
static AnyURI_t otp_alg = { "urn:ietf:params:xml:ns:keyprov:pskc:hotp", 40 };
static unsigned int sha256_arcs[] = { 2, 16, 840, 1, 101, 3, 4, 2, 1 };
static unsigned int sha1_arcs[] = { 1, 3, 14, 3, 2, 26 };
static AlgorithmIdentifier_t alg_sha256, alg_sha1; /* Initialized in main(). */
static AlgorithmIdentifier_t *algs[] = { &alg_sha256, &alg_sha1 };
static struct supportedHashAlg hash_algs = { algs, 2, 2 };
static Int32_t iter_count = 1000;
/* Flags are nextOTP | combine | collect-pin | must-encrypt-nonce |
 * separate-pin-required | check-digit */
static OTP_TOKENINFO_t token_info_2 = { { "\x77\0\0\0", 4, 0 }, &vendor,
                                        &challenge, &otp_length, &otp_format,
                                        &token_id, &otp_alg, &hash_algs,
                                        &iter_count };

/* Minimal PA-OTP-CHALLENGE */
static OTP_TOKENINFO_t *tinfo_1[] = { &token_info_1 };
static PA_OTP_CHALLENGE_t challenge_1 = { { "minnonce", 8 }, NULL,
                                          { { tinfo_1, 1, 1 } } };

/* Maximal PA-OTP-CHALLENGE */
static OTP_TOKENINFO_t *tinfo_2[] = { &token_info_1, &token_info_2 };
static UTF8String_t service = { "testservice", 11 };
static KerberosString_t salt = { "keysalt", 7 };
static OCTET_STRING_t s2kparams = { "1234", 4 };
static PA_OTP_CHALLENGE_t challenge_2 = { { "maxnonce", 8 }, &service,
                                          { { tinfo_2, 2, 2 } }, &salt,
                                          &s2kparams };

/* Minimal PA-OTP-REQUEST */
static UInt32_t kvno = 5;
static PA_OTP_REQUEST_t request_1 = { { "\0\0\0\0", 4, 0 }, NULL,
                                      { 0, &kvno,
                                        { "krbASN.1 test message", 21 } } };

/* Maximal PA-OTP-REQUEST */
/* Flags are nextOTP | combine */
static OCTET_STRING_t nonce = { "nonce", 5 };
static OCTET_STRING_t otp_value = { "frogs", 5 };
static UTF8String_t otp_pin = { "myfirstpin", 10 };
/* Corresponds to Unix time 771228197 */
static KerberosTime_t otp_time = { "19940610060317Z", 15 };
static OCTET_STRING_t counter = { "346", 3 };
static PA_OTP_REQUEST_t request_2 = { { "\x60\0\0\0", 4, 0 }, &nonce,
                                      { 0, &kvno,
                                        { "krbASN.1 test message", 21 } },
                                      &alg_sha256, &iter_count, &otp_value,
                                      &otp_pin, &challenge, &otp_time,
                                      &counter, &otp_format, &token_id,
                                      &otp_alg, &vendor };

/* PA-OTP-ENC-REQUEST */
static PA_OTP_ENC_REQUEST_t enc_request = { { "krb5data", 8 } };

/*
 * There is no ASN.1 name for a single authorization data element, so asn1c
 * declares it as "struct Member" in an inner scope.  This structure must be
 * laid out identically to that one.
 */
struct ad_element {
    Int32_t ad_type;
    OCTET_STRING_t ad_data;
    asn_struct_ctx_t _asn_ctx;
};

/* Authorization data elements and lists, for use in CAMMAC */
static struct ad_element ad_1 = { 1, { "ad1", 3 } };
static struct ad_element ad_2 = { 2, { "ad2", 3 } };
static struct ad_element *adlist_1[] = { &ad_1 };
static struct ad_element *adlist_2[] = { &ad_1, &ad_2 };

/* Minimal Verifier */
static Verifier_t verifier_1 = { Verifier_PR_mac,
                                 { { NULL, NULL, NULL,
                                     { 1, { "cksum1", 6 } } } } };

/* Maximal Verifier */
static Int32_t enctype = 16;
static Verifier_t verifier_2 = { Verifier_PR_mac,
                                 { { &princ, &kvno, &enctype,
                                     { 1, { "cksum2", 6 } } } } };

/* Minimal CAMMAC */
static AD_CAMMAC_t cammac_1 = { { { (void *)adlist_1, 1, 1 } },
                                NULL, NULL, NULL };

/* Maximal CAMMAC */
static Verifier_MAC_t vmac_1 = { &princ, &kvno, &enctype,
                                 { 1, { "cksumkdc", 8 } } };
static Verifier_MAC_t vmac_2 = { &princ, &kvno, &enctype,
                                 { 1, { "cksumsvc", 8 } } };
static Verifier_t *verifiers[] = { &verifier_1, &verifier_2 };
static struct other_verifiers overfs = { { verifiers, 2, 2 } };
static AD_CAMMAC_t cammac_2 = { { { (void *)adlist_2, 2, 2 } },
                                &vmac_1, &vmac_2, &overfs };

static int
consume(const void *data, size_t size, void *dummy)
{
    memcpy(buf + buf_pos, data, size);
    buf_pos += size;
    return 0;
}

/* Display a C string literal representing the contents of buf, and
 * reinitialize buf_pos for the next encoding operation. */
static void
printbuf(void)
{
    size_t i;

    for (i = 0; i < buf_pos; i++) {
        printf("%02X", buf[i]);
        if (i + 1 < buf_pos)
            printf(" ");
    }
    buf_pos = 0;
}

int
main()
{
    /* Initialize values which can't use static initializers. */
    asn_long2INTEGER(&otp_format, 2);  /* Alphanumeric */
    OBJECT_IDENTIFIER_set_arcs(&alg_sha256.algorithm, sha256_arcs,
                               sizeof(*sha256_arcs),
                               sizeof(sha256_arcs) / sizeof(*sha256_arcs));
    OBJECT_IDENTIFIER_set_arcs(&alg_sha1.algorithm, sha1_arcs,
                               sizeof(*sha1_arcs),
                               sizeof(sha1_arcs) / sizeof(*sha1_arcs));
    OBJECT_IDENTIFIER_set_arcs(&other_info.algorithmID.algorithm, krb5_arcs,
                               sizeof(*krb5_arcs),
                               sizeof(krb5_arcs) / sizeof(*krb5_arcs));

    printf("PrincipalName:\n");
    der_encode(&asn_DEF_PrincipalName, &princ, consume, NULL);
    printbuf();

    /* Print this encoding and also use it to initialize two fields of
     * other_info. */
    printf("\nKRB5PrincipalName:\n");
    der_encode(&asn_DEF_KRB5PrincipalName, &krb5princ, consume, NULL);
    OCTET_STRING_fromBuf(&other_info.partyUInfo, buf, buf_pos);
    OCTET_STRING_fromBuf(&other_info.partyVInfo, buf, buf_pos);
    printbuf();

    printf("\nOtherInfo:\n");
    der_encode(&asn_DEF_OtherInfo, &other_info, consume, NULL);
    printbuf();
    free(other_info.partyUInfo.buf);
    free(other_info.partyVInfo.buf);

    printf("\nPkinitSuppPubInfo:\n");
    der_encode(&asn_DEF_PkinitSuppPubInfo, &supp_pub_info, consume, NULL);
    printbuf();

    printf("\nMinimal OTP-TOKEN-INFO:\n");
    der_encode(&asn_DEF_OTP_TOKENINFO, &token_info_1, consume, NULL);
    printbuf();

    printf("\nMaximal OTP-TOKEN-INFO:\n");
    der_encode(&asn_DEF_OTP_TOKENINFO, &token_info_2, consume, NULL);
    printbuf();

    printf("\nMinimal PA-OTP-CHALLENGE:\n");
    der_encode(&asn_DEF_PA_OTP_CHALLENGE, &challenge_1, consume, NULL);
    printbuf();

    printf("\nMaximal PA-OTP-CHALLENGE:\n");
    der_encode(&asn_DEF_PA_OTP_CHALLENGE, &challenge_2, consume, NULL);
    printbuf();

    printf("\nMinimal PA-OTP-REQUEST:\n");
    der_encode(&asn_DEF_PA_OTP_REQUEST, &request_1, consume, NULL);
    printbuf();

    printf("\nMaximal PA-OTP-REQUEST:\n");
    der_encode(&asn_DEF_PA_OTP_REQUEST, &request_2, consume, NULL);
    printbuf();

    printf("\nPA-OTP-ENC-REQUEST:\n");
    der_encode(&asn_DEF_PA_OTP_ENC_REQUEST, &enc_request, consume, NULL);
    printbuf();

    printf("\nMinimal Verifier:\n");
    der_encode(&asn_DEF_Verifier, &verifier_1, consume, NULL);
    printbuf();

    printf("\nMaximal Verifier:\n");
    der_encode(&asn_DEF_Verifier, &verifier_2, consume, NULL);
    printbuf();

    printf("\nMinimal AD-CAMMAC:\n");
    der_encode(&asn_DEF_AD_CAMMAC, &cammac_1, consume, NULL);
    printbuf();

    printf("\nMaximal AD-CAMMAC:\n");
    der_encode(&asn_DEF_AD_CAMMAC, &cammac_2, consume, NULL);
    printbuf();

    printf("\n");
    return 0;
}
