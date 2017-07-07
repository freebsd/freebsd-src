/*
 * To be compiled against the AES code from:
 * http://fp.gladman.plus.com/cryptography_technology/rijndael/index.htm
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "aes.h"

#define B 16U
unsigned char key[16];
unsigned char test_case_len[] = { B+1, 2*B-1, 2*B, 2*B+1, 3*B-1, 3*B, 4*B, };
#define NTESTS (sizeof(test_case_len))
struct {
    unsigned char ivec[16];
    unsigned char input[4*16];
    unsigned char output[4*16];
} test_case[NTESTS];
aes_ctx ctx, dctx;

static void init ()
{
    unsigned int i, j, r;

    srand(42);
    for (i = 0; i < 16; i++)
	key[i] = 0xff & rand();
    memset(test_case, 0, sizeof(test_case));
    for (i = 0; i < NTESTS; i++)
	for (j = 0; j < test_case_len[i]; j++) {
	    test_case[i].input[j] = 0xff & rand();
	}

    r = aes_enc_key (key, sizeof(key), &ctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_dec_key (key, sizeof(key), &dctx);
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
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    };
    static const unsigned char input[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    };
    static const unsigned char expected[16] = {
	0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
	0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a,
    };
    unsigned char output[16];
    unsigned char tmp[16];
    aes_ctx fipsctx;
    int r;

    printf ("FIPS test:\nkey:");
    hexdump (fipskey, 16);
    printf ("\ninput:");
    hexdump (input, 16);
    r = aes_enc_key (fipskey, sizeof(fipskey), &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_enc_blk (input, output, &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    printf ("\noutput:");
    hexdump (output, 16);
    printf ("\n");
    if (memcmp(expected, output, 16))
	fprintf(stderr, "wrong results!!!\n"), exit (1);
    r = aes_dec_key (fipskey, sizeof(fipskey), &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_dec_blk (output, tmp, &fipsctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    if (memcmp(input, tmp, 16))
	fprintf(stderr, "decryption failed!!\n"), exit(1);
    printf ("ok.\n\n");
}

static void
xor (unsigned char *out, const unsigned char *a, const unsigned char *b)
{
    unsigned int i;
    for (i = 0; i < B; i++)
	out[i] = a[i] ^ b[i];
}

static void
ecb_enc (unsigned char *out, unsigned char *in, unsigned int len)
{
    unsigned int i, r;
    for (i = 0; i < len; i += 16) {
	r = aes_enc_blk (in + i, out + i, &ctx);
	if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    }
    if (i != len) abort ();
}

static void
ecb_dec (unsigned char *out, unsigned char *in, unsigned int len)
{
    unsigned int i, r;
    for (i = 0; i < len; i += 16) {
	r = aes_dec_blk (in + i, out + i, &dctx);
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
    unsigned int i, r;
    unsigned char tmp[B];
    D(iv);
    memcpy (tmp, iv, B);
    for (i = 0; i < len; i += B) {
	D(in+i);
	xor (tmp, tmp, in + i);
	D(tmp);
	r = aes_enc_blk (tmp, out + i, &ctx);
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
    unsigned int i, r;
    unsigned char tmp[B];
    memcpy (tmp, iv, B);
    for (i = 0; i < len; i += B) {
	r = aes_dec_blk (in + i, tmp, &dctx);
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
    r = aes_enc_blk (pn1, cn, &ctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    D(cn);
    memset (pn, 0, sizeof(pn));
    memcpy (pn, in+B, len-B);
    D(pn);
    xor (pn, pn, cn);
    D(pn);
    r = aes_enc_blk (pn, cn1, &ctx);
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
    r = aes_dec_blk (cn1, pn, &dctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    memset (cn, 0, sizeof(cn));
    memcpy (cn, in+B, len-B);
    xor (pn, pn, cn);
    memcpy (cn+len-B, pn+len-B, 2*B-len);
    r = aes_dec_blk (cn, pn1, &dctx);
    if (!r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    xor (pn1, pn1, iv);
    memcpy(out, pn1, B);
    memcpy(out+B, pn, len-B);
}

static void ecb_test ()
{
    unsigned int testno;
    unsigned char tmp[4*B];

    printf ("ECB tests:\n");
    printf ("key:");
    hexdump (key, sizeof(key));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", testno, len);
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
    unsigned int testno;
    unsigned char tmp[4*B];

    printf ("CBC tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", testno, len);
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
    unsigned int testno;
    unsigned char tmp[4*B];

    printf ("CTS tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned int len = test_case_len[testno];
	printf ("\ntest %d - %d bytes\n", testno, len);
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
