/*
 * Copyright (c) 2009
 * NTT (Nippon Telegraph and Telephone Corporation) . All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "camellia.h"

#define B 16U
unsigned char key[16];
unsigned char test_case_len[] = { B+1, 2*B-1, 2*B, 2*B+1, 3*B-1, 3*B, 4*B, };
#define NTESTS (sizeof(test_case_len))
struct {
    unsigned char ivec[16];
    unsigned char input[4*16];
    unsigned char output[4*16];
} test_case[NTESTS];
camellia_ctx ctx, dctx;

static void init ()
{
    size_t i, j;
    cam_rval r;

    srand(42);
    for (i = 0; i < 16; i++)
	key[i] = 0xff & rand();
    memset(test_case, 0, sizeof(test_case));
    for (i = 0; i < NTESTS; i++)
	for (j = 0; j < test_case_len[i]; j++) {
	    test_case[i].input[j] = 0xff & rand();
	}

    r = camellia_enc_key (key, sizeof(key), &ctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = camellia_dec_key (key, sizeof(key), &dctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
}

static void hexdump(const unsigned char *ptr, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
	printf ("%s%02X", (i % 16 == 0) ? "\n    " : " ", ptr[i]);
}

static void fips_test ()
{
    static const unsigned char fipskey[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    };
    static const unsigned char input[16] = {
	0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    };
    static const unsigned char expected[16] = {
	0x67,0x67,0x31,0x38,0x54,0x96,0x69,0x73,
	0x08,0x57,0x06,0x56,0x48,0xea,0xbe,0x43
    };
    unsigned char output[16];
    unsigned char tmp[16];
    camellia_ctx fipsctx;
    int r;

    printf ("FIPS test:\nkey:");
    hexdump (fipskey, 16);
    printf ("\ninput:");
    hexdump (input, 16);
    r = camellia_enc_key (fipskey, sizeof(fipskey), &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = camellia_enc_blk (input, output, &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    printf ("\noutput:");
    hexdump (output, 16);
    printf ("\n");
    if (memcmp(expected, output, 16))
	fprintf(stderr, "wrong results!!!\n"), exit (1);
    r = camellia_dec_key (fipskey, sizeof(fipskey), &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = camellia_dec_blk (output, tmp, &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    if (memcmp(input, tmp, 16))
	fprintf(stderr, "decryption failed!!\n"), exit(1);
    printf ("ok.\n\n");
}

static void
xor (unsigned char *out, const unsigned char *a, const unsigned char *b)
{
    size_t i;
    for (i = 0; i < B; i++)
	out[i] = a[i] ^ b[i];
}

static void
ecb_enc (unsigned char *out, unsigned char *in, unsigned int len)
{
    size_t i;
    cam_rval r;
    for (i = 0; i < len; i += 16) {
	r = camellia_enc_blk (in + i, out + i, &ctx);
	if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    }
    if (i != len) abort ();
}

static void
ecb_dec (unsigned char *out, unsigned char *in, unsigned int len)
{
    size_t i;
    cam_rval r;
    for (i = 0; i < len; i += 16) {
	r = camellia_dec_blk (in + i, out + i, &dctx);
	if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    }
    if (i != len) abort ();
}

#define D(X) (printf("%s %d: %s=",__FUNCTION__,__LINE__, #X),hexdump(X,B),printf("\n"))

#undef D
#define D(X)

static void
cbc_enc (unsigned char *out, unsigned char *in, unsigned char *iv,
	 unsigned int len)
{
    size_t i;
    cam_rval r;
    unsigned char tmp[B];
    D(iv);
    memcpy (tmp, iv, B);
    for (i = 0; i < len; i += B) {
	D(in+i);
	xor (tmp, tmp, in + i);
	D(tmp);
	r = camellia_enc_blk (tmp, out + i, &ctx);
	if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
	memcpy (tmp, out + i, B);
	D(out+i);
    }
    if (i != len) abort ();
}

static void
cbc_dec (unsigned char *out, unsigned char *in, unsigned char *iv,
	 unsigned int len)
{
    size_t i;
    cam_rval r;
    unsigned char tmp[B];
    memcpy (tmp, iv, B);
    for (i = 0; i < len; i += B) {
	r = camellia_dec_blk (in + i, tmp, &dctx);
	if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
	xor (tmp, tmp, iv);
	iv = in + i;
	memcpy (out + i, tmp, B);
    }
    if (i != len) abort ();
}

static void
cts_enc (unsigned char *out, unsigned char *in, unsigned char *iv,
	 unsigned int len)
{
    int r;
    unsigned int len2;
    unsigned char pn1[B], pn[B], cn[B], cn1[B];

    if (len < B + 1) abort ();
    len2 = (len - B - 1) & ~(B-1);
    cbc_enc (out, in, iv, len2);
    out += len2;
    in += len2;
    len -= len2;
    if (len2)
	iv = out - B;
    if (len <= B || len > 2 * B)
	abort ();
    printf ("(did CBC mode for %d)\n", len2);

    D(in);
    xor (pn1, in, iv);
    D(pn1);
    r = camellia_enc_blk (pn1, cn, &ctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    D(cn);
    memset (pn, 0, sizeof(pn));
    memcpy (pn, in+B, len-B);
    D(pn);
    xor (pn, pn, cn);
    D(pn);
    r = camellia_enc_blk (pn, cn1, &ctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    D(cn1);
    memcpy(out, cn1, B);
    memcpy(out+B, cn, len-B);
}

static void
cts_dec (unsigned char *out, unsigned char *in, unsigned char *iv,
	 unsigned int len)
{
    int r;
    unsigned int len2;
    unsigned char pn1[B], pn[B], cn[B], cn1[B];

    if (len < B + 1) abort ();
    len2 = (len - B - 1) & ~(B-1);
    cbc_dec (out, in, iv, len2);
    out += len2;
    in += len2;
    len -= len2;
    if (len2)
	iv = in - B;
    if (len <= B || len > 2 * B)
	abort ();

    memcpy (cn1, in, B);
    r = camellia_dec_blk (cn1, pn, &dctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    memset (cn, 0, sizeof(cn));
    memcpy (cn, in+B, len-B);
    xor (pn, pn, cn);
    memcpy (cn+len-B, pn+len-B, 2*B-len);
    r = camellia_dec_blk (cn, pn1, &dctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    xor (pn1, pn1, iv);
    memcpy(out, pn1, B);
    memcpy(out+B, pn, len-B);
}

static void ecb_test ()
{
    size_t testno;
    unsigned char tmp[4*B];

    printf ("ECB tests:\n");
    printf ("key:");
    hexdump (key, sizeof(key));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", (int)testno, len);
	printf ("input:");
	hexdump (test_case[testno].input, len);
	printf ("\n");
	ecb_enc (test_case[testno].output, test_case[testno].input, len);
	printf ("output:");
	hexdump (test_case[testno].output, len);
	printf ("\n");
	ecb_dec (tmp, test_case[testno].output, len);
	if (memcmp (tmp, test_case[testno].input, len)) {
	    printf ("ecb decrypt failed!!");
	    hexdump (tmp, len);
	    printf ("\n");
	    exit (1);
	}
    }
    printf ("\n");
}

unsigned char ivec[16] = { 0 };

static void cbc_test ()
{
    size_t testno;
    unsigned char tmp[4*B];

    printf ("CBC tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", (int)testno, len);
	printf ("input:");
	hexdump (test_case[testno].input, len);
	printf ("\n");
	cbc_enc (test_case[testno].output, test_case[testno].input, ivec, len);
	printf ("output:");
	hexdump (test_case[testno].output, len);
	printf ("\n");
	cbc_dec (tmp, test_case[testno].output, ivec, len);
	if (memcmp (tmp, test_case[testno].input, len)) {
	    printf("cbc decrypt failed!!");
	    hexdump (tmp, len);
	    printf ("\n");
	    exit(1);
	}
    }
    printf ("\n");
}

static void cts_test ()
{
    size_t testno;
    unsigned char tmp[4*B];

    printf ("CTS tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned int len = test_case_len[testno];
	printf ("\ntest %d - %d bytes\n", (int)testno, len);
	printf ("input:");
	hexdump (test_case[testno].input, len);
	printf ("\n");
	cts_enc (test_case[testno].output, test_case[testno].input, ivec, len);
	printf ("output:");
	hexdump (test_case[testno].output, len);
	printf ("\n");
	cts_dec (tmp, test_case[testno].output, ivec, len);
	if (memcmp (tmp, test_case[testno].input, len))
	    fprintf (stderr, "cts decrypt failed!!\n"), exit(1);
    }
    printf ("\n");
}

int main ()
{
    init ();
    fips_test ();

    ecb_test();
    cbc_test();
    cts_test();

    return 0;
}

