/*
 * To be compiled against the AES code from:
 * https://github.com/BrianGladman/AES
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "aes.h"

#define B 16U
uint8_t key[16] = { 0x46, 0x64, 0x31, 0x29, 0x64, 0x86, 0xED, 0x9C,
		    0xD7, 0x1F, 0xC2, 0x07, 0x25, 0x48, 0x20, 0xA2 };
size_t test_case_len[] = { B+1, 2*B-1, 2*B, 2*B+1, 3*B-1, 3*B, 4*B, };
#define NTESTS (sizeof(test_case_len) / sizeof(*test_case_len))
uint8_t test_case[NTESTS][4 * B] = {
    { 0xC4, 0xA8, 0x5A, 0xEB, 0x0B, 0x20, 0x41, 0x49,
      0x4F, 0x8B, 0xF1, 0xF8, 0xCD, 0x30, 0xF1, 0x13,
      0x94 },
    { 0x22, 0x3C, 0xF8, 0xA8, 0x29, 0x95, 0x80, 0x49,
      0x57, 0x87, 0x6E, 0x9F, 0xA7, 0x11, 0x63, 0x50,
      0x6B, 0x4E, 0x5B, 0x8C, 0x8F, 0xA4, 0xDB, 0x1B,
      0x95, 0xD3, 0xE8, 0xC5, 0xC5, 0xFB, 0x5A },
    { 0xE7, 0x37, 0x52, 0x90, 0x60, 0xE7, 0x10, 0xA9,
      0x3E, 0x97, 0x18, 0xDD, 0x3E, 0x29, 0x41, 0x8E,
      0x94, 0x8F, 0xE9, 0x20, 0x1F, 0x8D, 0xFB, 0x3A,
      0x22, 0xCF, 0x22, 0xE8, 0x94, 0x1D, 0x42, 0x7B },
    { 0x54, 0x94, 0x0B, 0xB4, 0x7C, 0x1B, 0x5E, 0xBA,
      0xB2, 0x76, 0x98, 0xF1, 0x9F, 0xD9, 0x7F, 0x33,
      0x68, 0x69, 0x54, 0x87, 0xF6, 0x4F, 0xC1, 0x19,
      0x1E, 0xE3, 0x01, 0xB2, 0x00, 0x43, 0x2E, 0x54,
      0xD7 },
    { 0x39, 0x09, 0x53, 0x55, 0x67, 0x0E, 0x07, 0xDD,
      0xA6, 0xF8, 0x7C, 0x7F, 0x78, 0xAF, 0xE7, 0xE1,
      0x03, 0x6F, 0xD7, 0x53, 0x30, 0xF0, 0x71, 0x14,
      0xF1, 0x24, 0x14, 0x34, 0x52, 0x69, 0x0C, 0x8B,
      0x72, 0x5F, 0xE0, 0xD9, 0x6D, 0xE8, 0xB6, 0x13,
      0xE0, 0x32, 0x92, 0x58, 0xE1, 0x7A, 0x39 },
    { 0xE5, 0xE9, 0x11, 0x38, 0x19, 0x01, 0xA9, 0x2D,
      0xF3, 0xCD, 0x42, 0x27, 0x1F, 0xAB, 0x33, 0xAB,
      0x1D, 0x93, 0x8B, 0xF6, 0x00, 0x73, 0xAC, 0x14,
      0x54, 0xDE, 0xA6, 0xAC, 0xBF, 0x20, 0xE6, 0xA4,
      0x09, 0xF7, 0xDC, 0x23, 0xF8, 0x86, 0x50, 0xEB,
      0x53, 0x92, 0x13, 0x73, 0x3D, 0x46, 0x1E, 0x5A },
    { 0xD9, 0xA9, 0x50, 0xDA, 0x1D, 0xFC, 0xEE, 0x71,
      0xDA, 0x94, 0x1D, 0x9A, 0xB5, 0x03, 0x3E, 0xBE,
      0xFA, 0x1B, 0xE1, 0xF3, 0xA1, 0x32, 0xDE, 0xF4,
      0xC4, 0xF1, 0x67, 0x02, 0x38, 0x85, 0x5C, 0x11,
      0x2F, 0xAD, 0xEB, 0x4C, 0xA9, 0xD9, 0xBD, 0x84,
      0x6E, 0xDA, 0x1E, 0x23, 0xDE, 0x5C, 0xE1, 0xD8,
      0x77, 0xC3, 0xCB, 0x18, 0xF5, 0xAA, 0x0D, 0xB9,
      0x9B, 0x74, 0xBB, 0xD3, 0xFA, 0x18, 0xE5, 0x29 }
};
aes_encrypt_ctx ctx;
aes_decrypt_ctx dctx;

static void init ()
{
    AES_RETURN r;

    r = aes_encrypt_key128(key, &ctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_decrypt_key128(key, &dctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
    aes_crypt_ctx fipsctx;
    int r;

    printf ("FIPS test:\nkey:");
    hexdump (fipskey, 16);
    printf ("\ninput:");
    hexdump (input, 16);
    r = aes_encrypt_key128(fipskey, &fipsctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_encrypt(input, output, &fipsctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    printf ("\noutput:");
    hexdump (output, 16);
    printf ("\n");
    if (memcmp(expected, output, 16))
	fprintf(stderr, "wrong results!!!\n"), exit (1);
    r = aes_decrypt_key128(fipskey, &fipsctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    r = aes_decrypt(output, tmp, &fipsctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
	r = aes_encrypt(in + i, out + i, &ctx);
	if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    }
    if (i != len) abort ();
}

static void
ecb_dec (unsigned char *out, unsigned char *in, unsigned int len)
{
    unsigned int i, r;
    for (i = 0; i < len; i += 16) {
	r = aes_decrypt(in + i, out + i, &dctx);
	if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
	r = aes_encrypt(tmp, out + i, &ctx);
	if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
	r = aes_decrypt(in + i, tmp, &dctx);
	if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
    r = aes_encrypt(pn1, cn, &ctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    D(cn);
    memset (pn, 0, sizeof(pn));
    memcpy (pn, in+B, len-B);
    D(pn);
    xor (pn, pn, cn);
    D(pn);
    r = aes_encrypt(pn, cn1, &ctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
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
    r = aes_decrypt(cn1, pn, &dctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    memset (cn, 0, sizeof(cn));
    memcpy (cn, in+B, len-B);
    xor (pn, pn, cn);
    memcpy (cn+len-B, pn+len-B, 2*B-len);
    r = aes_decrypt(cn, pn1, &dctx);
    if (r) fprintf(stderr, "error, line %d\n", __LINE__), exit(1);
    xor (pn1, pn1, iv);
    memcpy(out, pn1, B);
    memcpy(out+B, pn, len-B);
}

static void ecb_test ()
{
    unsigned int testno;
    uint8_t output[4 * B], tmp[4 * B];

    printf ("ECB tests:\n");
    printf ("key:");
    hexdump (key, sizeof(key));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", testno, len);
	printf ("input:");
	hexdump (test_case[testno], len);
	printf ("\n");
	ecb_enc (output, test_case[testno], len);
	printf ("output:");
	hexdump (output, len);
	printf ("\n");
	ecb_dec (tmp, output, len);
	if (memcmp (tmp, test_case[testno], len)) {
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
    uint8_t output[4 * B], tmp[4 * B];

    printf ("CBC tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned len = (test_case_len[testno] + 15) & ~15;
	printf ("\ntest %d - %d bytes\n", testno, len);
	printf ("input:");
	hexdump (test_case[testno], len);
	printf ("\n");
	cbc_enc (output, test_case[testno], ivec, len);
	printf ("output:");
	hexdump (output, len);
	printf ("\n");
	cbc_dec (tmp, output, ivec, len);
	if (memcmp (tmp, test_case[testno], len)) {
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
    uint8_t output[4 * B], tmp[4 * B];

    printf ("CTS tests:\n");
    printf ("initial vector:");
    hexdump (ivec, sizeof(ivec));
    for (testno = 0; testno < NTESTS; testno++) {
	unsigned int len = test_case_len[testno];
	printf ("\ntest %d - %d bytes\n", testno, len);
	printf ("input:");
	hexdump (test_case[testno], len);
	printf ("\n");
	cts_enc (output, test_case[testno], ivec, len);
	printf ("output:");
	hexdump (output, len);
	printf ("\n");
	cts_dec (tmp, output, ivec, len);
	if (memcmp (tmp, test_case[testno], len))
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
