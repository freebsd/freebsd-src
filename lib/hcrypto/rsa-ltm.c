/*
 * Copyright (c) 2006 - 2007, 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <krb5-types.h>
#include <assert.h>

#include <rsa.h>

#include <roken.h>

#include "tommath.h"

static int
random_num(mp_int *num, size_t len)
{
    unsigned char *p;

    len = (len + 7) / 8;
    p = malloc(len);
    if (p == NULL)
	return 1;
    if (RAND_bytes(p, len) != 1) {
	free(p);
	return 1;
    }
    mp_read_unsigned_bin(num, p, len);
    free(p);
    return 0;
}

static void
BN2mpz(mp_int *s, const BIGNUM *bn)
{
    size_t len;
    void *p;

    len = BN_num_bytes(bn);
    p = malloc(len);
    BN_bn2bin(bn, p);
    mp_read_unsigned_bin(s, p, len);
    free(p);
}

static void
setup_blind(mp_int *n, mp_int *b, mp_int *bi)
{
    random_num(b, mp_count_bits(n));
    mp_mod(b, n, b);
    mp_invmod(b, n, bi);
}

static void
blind(mp_int *in, mp_int *b, mp_int *e, mp_int *n)
{
    mp_int t1;
    mp_init(&t1);
    /* in' = (in * b^e) mod n */
    mp_exptmod(b, e, n, &t1);
    mp_mul(&t1, in, in);
    mp_mod(in, n, in);
    mp_clear(&t1);
}

static void
unblind(mp_int *out, mp_int *bi, mp_int *n)
{
    /* out' = (out * 1/b) mod n */
    mp_mul(out, bi, out);
    mp_mod(out, n, out);
}

static int
ltm_rsa_private_calculate(mp_int * in, mp_int * p,  mp_int * q,
			  mp_int * dmp1, mp_int * dmq1, mp_int * iqmp,
			  mp_int * out)
{
    mp_int vp, vq, u;

    mp_init_multi(&vp, &vq, &u, NULL);

    /* vq = c ^ (d mod (q - 1)) mod q */
    /* vp = c ^ (d mod (p - 1)) mod p */
    mp_mod(in, p, &u);
    mp_exptmod(&u, dmp1, p, &vp);
    mp_mod(in, q, &u);
    mp_exptmod(&u, dmq1, q, &vq);

    /* C2 = 1/q mod p  (iqmp) */
    /* u = (vp - vq)C2 mod p. */
    mp_sub(&vp, &vq, &u);
    if (mp_isneg(&u))
	mp_add(&u, p, &u);
    mp_mul(&u, iqmp, &u);
    mp_mod(&u, p, &u);

    /* c ^ d mod n = vq + u q */
    mp_mul(&u, q, &u);
    mp_add(&u, &vq, out);

    mp_clear_multi(&vp, &vq, &u, NULL);

    return 0;
}

/*
 *
 */

static int
ltm_rsa_public_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    int res;
    size_t size, padlen;
    mp_int enc, dec, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    mp_init_multi(&n, &e, &enc, &dec, NULL);

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen) {
	mp_clear_multi(&n, &e, &enc, &dec);
	return -2;
    }

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    p = p0 = malloc(size - 1);
    if (p0 == NULL) {
	mp_clear_multi(&e, &n, &enc, &dec, NULL);
	return -3;
    }

    padlen = size - flen - 3;

    *p++ = 2;
    if (RAND_bytes(p, padlen) != 1) {
	mp_clear_multi(&e, &n, &enc, &dec, NULL);
	free(p0);
	return -4;
    }
    while(padlen) {
	if (*p == 0)
	    *p = 1;
	padlen--;
	p++;
    }
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size - 1);

    mp_read_unsigned_bin(&dec, p0, size - 1);
    free(p0);

    res = mp_exptmod(&dec, &e, &n, &enc);

    mp_clear_multi(&dec, &e, &n, NULL);

    if (res != 0) {
	mp_clear(&enc);
	return -4;
    }

    {
	size_t ssize;
	ssize = mp_unsigned_bin_size(&enc);
	assert(size >= ssize);
	mp_to_unsigned_bin(&enc, to);
	size = ssize;
    }
    mp_clear(&enc);

    return size;
}

static int
ltm_rsa_public_decrypt(int flen, const unsigned char* from,
		       unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p;
    int res;
    size_t size;
    mp_int s, us, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    if (flen > RSA_size(rsa))
	return -2;

    mp_init_multi(&e, &n, &s, &us, NULL);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

#if 0
    /* Check that the exponent is larger then 3 */
    if (mp_int_compare_value(&e, 3) <= 0) {
	mp_clear_multi(&e, &n, &s, &us, NULL);
	return -3;
    }
#endif

    mp_read_unsigned_bin(&s, rk_UNCONST(from), flen);

    if (mp_cmp(&s, &n) >= 0) {
	mp_clear_multi(&e, &n, &s, &us, NULL);
	return -4;
    }

    res = mp_exptmod(&s, &e, &n, &us);

    mp_clear_multi(&e, &n, &s, NULL);

    if (res != 0) {
	mp_clear(&us);
	return -5;
    }
    p = to;


    size = mp_unsigned_bin_size(&us);
    assert(size <= RSA_size(rsa));
    mp_to_unsigned_bin(&us, p);

    mp_clear(&us);

    /* head zero was skipped by mp_to_unsigned_bin */
    if (*p == 0)
	return -6;
    if (*p != 1)
	return -7;
    size--; p++;
    while (size && *p == 0xff) {
	size--; p++;
    }
    if (size == 0 || *p != 0)
	return -8;
    size--; p++;

    memmove(to, p, size);

    return size;
}

static int
ltm_rsa_private_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    int res;
    int size;
    mp_int in, out, n, e;
    mp_int bi, b;
    int blinding = (rsa->flags & RSA_FLAG_NO_BLINDING) == 0;
    int do_unblind = 0;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    mp_init_multi(&e, &n, &in, &out, &b, &bi, NULL);

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen)
	return -2;

    p0 = p = malloc(size);
    *p++ = 0;
    *p++ = 1;
    memset(p, 0xff, size - flen - 3);
    p += size - flen - 3;
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    mp_read_unsigned_bin(&in, p0, size);
    free(p0);

    if(mp_isneg(&in) || mp_cmp(&in, &n) >= 0) {
	size = -3;
	goto out;
    }

    if (blinding) {
	setup_blind(&n, &b, &bi);
	blind(&in, &b, &e, &n);
	do_unblind = 1;
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	mp_int p, q, dmp1, dmq1, iqmp;

	mp_init_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = ltm_rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	mp_clear_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	if (res != 0) {
	    size = -4;
	    goto out;
	}
    } else {
	mp_int d;

	BN2mpz(&d, rsa->d);
	res = mp_exptmod(&in, &d, &n, &out);
	mp_clear(&d);
	if (res != 0) {
	    size = -5;
	    goto out;
	}
    }

    if (do_unblind)
	unblind(&out, &bi, &n);

    if (size > 0) {
	size_t ssize;
	ssize = mp_unsigned_bin_size(&out);
	assert(size >= ssize);
	mp_to_unsigned_bin(&out, to);
	size = ssize;
    }

 out:
    mp_clear_multi(&e, &n, &in, &out, &b, &bi, NULL);

    return size;
}

static int
ltm_rsa_private_decrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *ptr;
    int res, size;
    mp_int in, out, n, e, b, bi;
    int blinding = (rsa->flags & RSA_FLAG_NO_BLINDING) == 0;
    int do_unblind = 0;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);
    if (flen > size)
	return -2;

    mp_init_multi(&in, &n, &e, &out, &b, &bi, NULL);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    mp_read_unsigned_bin(&in, rk_UNCONST(from), flen);

    if(mp_isneg(&in) || mp_cmp(&in, &n) >= 0) {
	size = -2;
	goto out;
    }

    if (blinding) {
	setup_blind(&n, &b, &bi);
	blind(&in, &b, &e, &n);
	do_unblind = 1;
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	mp_int p, q, dmp1, dmq1, iqmp;

	mp_init_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = ltm_rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	mp_clear_multi(&p, &q, &dmp1, &dmq1, &iqmp, NULL);

	if (res != 0) {
	    size = -3;
	    goto out;
	}

    } else {
	mp_int d;

	if(mp_isneg(&in) || mp_cmp(&in, &n) >= 0)
	    return -4;

	BN2mpz(&d, rsa->d);
	res = mp_exptmod(&in, &d, &n, &out);
	mp_clear(&d);
	if (res != 0) {
	    size = -5;
	    goto out;
	}
    }

    if (do_unblind)
	unblind(&out, &bi, &n);

    ptr = to;
    {
	size_t ssize;
	ssize = mp_unsigned_bin_size(&out);
	assert(size >= ssize);
	mp_to_unsigned_bin(&out, ptr);
	size = ssize;
    }

    /* head zero was skipped by mp_int_to_unsigned */
    if (*ptr != 2) {
	size = -6;
	goto out;
    }
    size--; ptr++;
    while (size && *ptr != 0) {
	size--; ptr++;
    }
    if (size == 0)
	return -7;
    size--; ptr++;

    memmove(to, ptr, size);

 out:
    mp_clear_multi(&e, &n, &in, &out, &b, &bi, NULL);

    return size;
}

static BIGNUM *
mpz2BN(mp_int *s)
{
    size_t size;
    BIGNUM *bn;
    void *p;

    size = mp_unsigned_bin_size(s);
    p = malloc(size);
    if (p == NULL && size != 0)
	return NULL;

    mp_to_unsigned_bin(s, p);

    bn = BN_bin2bn(p, size, NULL);
    free(p);
    return bn;
}

#define CHECK(f, v) if ((f) != (v)) { goto out; }

static int
ltm_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
    mp_int el, p, q, n, d, dmp1, dmq1, iqmp, t1, t2, t3;
    int counter, ret, bitsp;

    if (bits < 789)
	return -1;

    bitsp = (bits + 1) / 2;

    ret = -1;

    mp_init_multi(&el, &p, &q, &n, &d,
		  &dmp1, &dmq1, &iqmp,
		  &t1, &t2, &t3, NULL);

    BN2mpz(&el, e);

    /* generate p and q so that p != q and bits(pq) ~ bits */
    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&p, bitsp), 0);
	CHECK(mp_find_prime(&p), MP_YES);

	mp_sub_d(&p, 1, &t1);
	mp_gcd(&t1, &el, &t2);
    } while(mp_cmp_d(&t2, 1) != 0);

    BN_GENCB_call(cb, 3, 0);

    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&q, bits - bitsp), 0);
	CHECK(mp_find_prime(&q), MP_YES);

	if (mp_cmp(&p, &q) == 0) /* don't let p and q be the same */
	    continue;

	mp_sub_d(&q, 1, &t1);
	mp_gcd(&t1, &el, &t2);
    } while(mp_cmp_d(&t2, 1) != 0);

    /* make p > q */
    if (mp_cmp(&p, &q) < 0) {
	mp_int c;
	c = p;
	p = q;
	q = c;
    }

    BN_GENCB_call(cb, 3, 1);

    /* calculate n,  		n = p * q */
    mp_mul(&p, &q, &n);

    /* calculate d, 		d = 1/e mod (p - 1)(q - 1) */
    mp_sub_d(&p, 1, &t1);
    mp_sub_d(&q, 1, &t2);
    mp_mul(&t1, &t2, &t3);
    mp_invmod(&el, &t3, &d);

    /* calculate dmp1		dmp1 = d mod (p-1) */
    mp_mod(&d, &t1, &dmp1);
    /* calculate dmq1		dmq1 = d mod (q-1) */
    mp_mod(&d, &t2, &dmq1);
    /* calculate iqmp 		iqmp = 1/q mod p */
    mp_invmod(&q, &p, &iqmp);

    /* fill in RSA key */

    rsa->e = mpz2BN(&el);
    rsa->p = mpz2BN(&p);
    rsa->q = mpz2BN(&q);
    rsa->n = mpz2BN(&n);
    rsa->d = mpz2BN(&d);
    rsa->dmp1 = mpz2BN(&dmp1);
    rsa->dmq1 = mpz2BN(&dmq1);
    rsa->iqmp = mpz2BN(&iqmp);

    ret = 1;

out:
    mp_clear_multi(&el, &p, &q, &n, &d,
		   &dmp1, &dmq1, &iqmp,
		   &t1, &t2, &t3, NULL);

    return ret;
}

static int
ltm_rsa_init(RSA *rsa)
{
    return 1;
}

static int
ltm_rsa_finish(RSA *rsa)
{
    return 1;
}

const RSA_METHOD hc_rsa_ltm_method = {
    "hcrypto ltm RSA",
    ltm_rsa_public_encrypt,
    ltm_rsa_public_decrypt,
    ltm_rsa_private_encrypt,
    ltm_rsa_private_decrypt,
    NULL,
    NULL,
    ltm_rsa_init,
    ltm_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    ltm_rsa_generate_key
};

const RSA_METHOD *
RSA_ltm_method(void)
{
    return &hc_rsa_ltm_method;
}
