/* lib/crypto/crypto_tests/camellia-test.c */
/*
 * Copyright (c) 2009
 * NTT (Nippon Telegraph and Telephone Corporation) . All rights reserved.
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
 * 
 *
 * Subset of NIST tests for AES as applied to Camellia; specifically, the
 * variable-key and variable-text tests for 128- and 256-bit keys.
 */

#include <stdio.h>
#include "crypto_int.h"

static char key[32];
static char plain[16], cipher[16], zero[16];

static krb5_keyblock enc_key;
static krb5_data ivec;
static void init()
{
    enc_key.contents = (unsigned char *)key;
    enc_key.length = 16;
    ivec.data = zero;
    ivec.length = 16;
}
static void enc()
{
    krb5_key k;
    krb5_crypto_iov iov;
    krb5_data cdata = make_data(cipher, 16);

    iov.flags = KRB5_CRYPTO_TYPE_DATA;
    iov.data = make_data(plain, 16);
    krb5_k_create_key(NULL, &enc_key, &k);
    /* cbc-mac is the same as block encryption for a single block. */
    krb5int_camellia_cbc_mac(k, &iov, 1, &ivec, &cdata);
    krb5_k_free_key(NULL, k);
}

static void hexdump(const char *label, const char *cp, int len)
{
    printf("%s=", label);
    while (len--) printf("%02X", 0xff & *cp++);
    printf("\n");
}

static void set_bit(char *ptr, int bitnum)
{
    int bytenum;
    bytenum = bitnum / 8;
    bitnum %= 8;
    /* First bit is the high bit! */
    ptr[bytenum] = 1 << (7 - bitnum);
}

/* Variable-Key tests */
static void vk_test_1(int len)
{
    int i;

    enc_key.enctype = ENCTYPE_CAMELLIA128_CTS_CMAC;
    enc_key.length = len;
    printf("\nKEYSIZE=%d\n\n", len * 8);
    memset(plain, 0, sizeof(plain));
    hexdump("PT", plain, 16);
    for (i = 0; i < len * 8; i++) {
	memset(key, 0, len);
	set_bit(key, i);
	printf("\nI=%d\n", i+1);
	hexdump("KEY", key, len);
	enc();
	hexdump("CT", cipher, 16);
    }
    printf("\n==========\n");
}
static void vk_test()
{
    vk_test_1(16);
    vk_test_1(32);
}

/* Variable-Text tests */
static void vt_test_1(int len, krb5_enctype etype)
{
    int i;

    enc_key.enctype = etype;
    enc_key.length = len;
    printf("\nKEYSIZE=%d\n\n", len * 8);
    memset(key, 0, len);
    hexdump("KEY", key, len);
    for (i = 0; i < 16 * 8; i++) {
	memset(plain, 0, sizeof(plain));
	set_bit(plain, i);
	printf("\nI=%d\n", i+1);
	hexdump("PT", plain, 16);
	enc();
	hexdump("CT", cipher, 16);
    }
    printf("\n==========\n");
}
static void vt_test()
{
    vt_test_1(16, ENCTYPE_CAMELLIA128_CTS_CMAC);
    vt_test_1(32, ENCTYPE_CAMELLIA256_CTS_CMAC);
}

int main (int argc, char *argv[])
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "-k"))) {
	fprintf(stderr,
		"usage:\t%s -k\tfor variable-key tests\n"
		"   or:\t%s   \tfor variable-plaintext tests\n",
		argv[0], argv[0]);
	return 1;
    }
    init();
    if (argc == 2)
	vk_test();
    else
	vt_test();
    return 0;
}
