/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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
#include <string.h>
#include <limits.h>

#include <krb5-types.h>
#include <roken.h>
#include <rfc2459_asn1.h> /* XXX */
#include <der.h>

#include <bn.h>
#include <rand.h>
#include <hex.h>

BIGNUM *
BN_new(void)
{
    heim_integer *hi;
    hi = calloc(1, sizeof(*hi));
    return (BIGNUM *)hi;
}

void
BN_free(BIGNUM *bn)
{
    BN_clear(bn);
    free(bn);
}

void
BN_clear(BIGNUM *bn)
{
    heim_integer *hi = (heim_integer *)bn;
    if (hi->data) {
	memset(hi->data, 0, hi->length);
	free(hi->data);
    }
    memset(hi, 0, sizeof(*hi));
}

void
BN_clear_free(BIGNUM *bn)
{
    BN_free(bn);
}

BIGNUM *
BN_dup(const BIGNUM *bn)
{
    BIGNUM *b = BN_new();
    if (der_copy_heim_integer((const heim_integer *)bn, (heim_integer *)b)) {
	BN_free(b);
	return NULL;
    }
    return b;
}

/*
 * If the caller really want to know the number of bits used, subtract
 * one from the length, multiply by 8, and then lookup in the table
 * how many bits the hightest byte uses.
 */
int
BN_num_bits(const BIGNUM *bn)
{
    static unsigned char num2bits[256] = {
	0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,  5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    };
    const heim_integer *i = (const void *)bn;
    if (i->length == 0)
	return 0;
    return (i->length - 1) * 8 + num2bits[((unsigned char *)i->data)[0]];
}

int
BN_num_bytes(const BIGNUM *bn)
{
    return ((const heim_integer *)bn)->length;
}

/*
 * Ignore negative flag.
 */

BIGNUM *
BN_bin2bn(const void *s, int len, BIGNUM *bn)
{
    heim_integer *hi = (void *)bn;

    if (len < 0)
	return NULL;

    if (hi == NULL) {
	hi = (heim_integer *)BN_new();
	if (hi == NULL)
	    return NULL;
    }
    if (hi->data)
	BN_clear((BIGNUM *)hi);
    hi->negative = 0;
    hi->data = malloc(len);
    if (hi->data == NULL && len != 0) {
	if (bn == NULL)
	    BN_free((BIGNUM *)hi);
	return NULL;
    }
    hi->length = len;
    memcpy(hi->data, s, len);
    return (BIGNUM *)hi;
}

int
BN_bn2bin(const BIGNUM *bn, void *to)
{
    const heim_integer *hi = (const void *)bn;
    memcpy(to, hi->data, hi->length);
    return hi->length;
}

int
BN_hex2bn(BIGNUM **bnp, const char *in)
{
    int negative;
    ssize_t ret;
    size_t len;
    void *data;

    len = strlen(in);
    data = malloc(len);
    if (data == NULL)
	return 0;

    if (*in == '-') {
	negative = 1;
	in++;
    } else
	negative = 0;

    ret = hex_decode(in, data, len);
    if (ret < 0) {
	free(data);
	return 0;
    }

    *bnp = BN_bin2bn(data, ret, NULL);
    free(data);
    if (*bnp == NULL)
	return 0;
    BN_set_negative(*bnp, negative);
    return 1;
}

char *
BN_bn2hex(const BIGNUM *bn)
{
    ssize_t ret;
    size_t len;
    void *data;
    char *str;

    len = BN_num_bytes(bn);
    data = malloc(len);
    if (data == NULL)
	return 0;

    len = BN_bn2bin(bn, data);

    ret = hex_encode(data, len, &str);
    free(data);
    if (ret < 0)
	return 0;

    return str;
}

int
BN_cmp(const BIGNUM *bn1, const BIGNUM *bn2)
{
    return der_heim_integer_cmp((const heim_integer *)bn1,
				(const heim_integer *)bn2);
}

void
BN_set_negative(BIGNUM *bn, int flag)
{
    ((heim_integer *)bn)->negative = (flag ? 1 : 0);
}

int
BN_is_negative(const BIGNUM *bn)
{
    return ((const heim_integer *)bn)->negative ? 1 : 0;
}

static const unsigned char is_set[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

int
BN_is_bit_set(const BIGNUM *bn, int bit)
{
    heim_integer *hi = (heim_integer *)bn;
    unsigned char *p = hi->data;

    if ((bit / 8) > hi->length || hi->length == 0)
	return 0;

    return p[hi->length - 1 - (bit / 8)] & is_set[bit % 8];
}

int
BN_set_bit(BIGNUM *bn, int bit)
{
    heim_integer *hi = (heim_integer *)bn;
    unsigned char *p;

    if ((bit / 8) > hi->length || hi->length == 0) {
	size_t len = (bit + 7) / 8;
	void *d = realloc(hi->data, len);
	if (d == NULL)
	    return 0;
	hi->data = d;
	p = hi->data;
	memset(&p[hi->length], 0, len);
	hi->length = len;
    } else
	p = hi->data;

    p[hi->length - 1 - (bit / 8)] |= is_set[bit % 8];
    return 1;
}

int
BN_clear_bit(BIGNUM *bn, int bit)
{
    heim_integer *hi = (heim_integer *)bn;
    unsigned char *p = hi->data;

    if ((bit / 8) > hi->length || hi->length == 0)
	return 0;

    p[hi->length - 1 - (bit / 8)] &= (unsigned char)(~(is_set[bit % 8]));

    return 1;
}

int
BN_set_word(BIGNUM *bn, unsigned long num)
{
    unsigned char p[sizeof(num)];
    unsigned long num2;
    int i, len;

    for (num2 = num, i = 0; num2 > 0; i++)
	num2 = num2 >> 8;

    len = i;
    for (; i > 0; i--) {
	p[i - 1] = (num & 0xff);
	num = num >> 8;
    }

    bn = BN_bin2bn(p, len, bn);
    return bn != NULL;
}

unsigned long
BN_get_word(const BIGNUM *bn)
{
    heim_integer *hi = (heim_integer *)bn;
    unsigned long num = 0;
    int i;

    if (hi->negative || hi->length > sizeof(num))
	return ULONG_MAX;

    for (i = 0; i < hi->length; i++)
	num = ((unsigned char *)hi->data)[i] | (num << 8);
    return num;
}

int
BN_rand(BIGNUM *bn, int bits, int top, int bottom)
{
    size_t len = (bits + 7) / 8;
    heim_integer *i = (heim_integer *)bn;

    BN_clear(bn);

    i->negative = 0;
    i->data = malloc(len);
    if (i->data == NULL && len != 0)
	return 0;
    i->length = len;

    if (RAND_bytes(i->data, i->length) != 1) {
	free(i->data);
	i->data = NULL;
	return 0;
    }

    {
	size_t j = len * 8;
	while(j > bits) {
	    BN_clear_bit(bn, j - 1);
	    j--;
	}
    }

    if (top == -1) {
	;
    } else if (top == 0 && bits > 0) {
	BN_set_bit(bn, bits - 1);
    } else if (top == 1 && bits > 1) {
	BN_set_bit(bn, bits - 1);
	BN_set_bit(bn, bits - 2);
    } else {
	BN_clear(bn);
	return 0;
    }

    if (bottom && bits > 0)
	BN_set_bit(bn, 0);

    return 1;
}

/*
 *
 */

int
BN_uadd(BIGNUM *res, const BIGNUM *a, const BIGNUM *b)
{
    const heim_integer *ai = (const heim_integer *)a;
    const heim_integer *bi = (const heim_integer *)b;
    const unsigned char *ap, *bp;
    unsigned char *cp;
    heim_integer ci;
    int carry = 0;
    ssize_t len;

    if (ai->negative && bi->negative)
	return 0;
    if (ai->length < bi->length) {
	const heim_integer *si = bi;
	bi = ai; ai = si;
    }

    ci.negative = 0;
    ci.length = ai->length + 1;
    ci.data = malloc(ci.length);
    if (ci.data == NULL)
	return 0;

    ap = &((const unsigned char *)ai->data)[ai->length - 1];
    bp = &((const unsigned char *)bi->data)[bi->length - 1];
    cp = &((unsigned char *)ci.data)[ci.length - 1];

    for (len = bi->length; len > 0; len--) {
	carry = *ap + *bp + carry;
	*cp = carry & 0xff;
	carry = (carry & ~0xff) ? 1 : 0;
	ap--; bp--; cp--;
    }
    for (len = ai->length - bi->length; len > 0; len--) {
	carry = *ap + carry;
	*cp = carry & 0xff;
	carry = (carry & ~0xff) ? 1 : 0;
	ap--; cp--;
    }
    if (!carry)
	memmove(cp, cp + 1, --ci.length);
    else
	*cp = carry;

    BN_clear(res);
    *((heim_integer *)res) = ci;

    return 1;
}


/*
 * Callback when doing slow generation of numbers, like primes.
 */

void
BN_GENCB_set(BN_GENCB *gencb, int (*cb_2)(int, int, BN_GENCB *), void *ctx)
{
    gencb->ver = 2;
    gencb->cb.cb_2 = cb_2;
    gencb->arg = ctx;
}

int
BN_GENCB_call(BN_GENCB *cb, int a, int b)
{
    if (cb == NULL || cb->cb.cb_2 == NULL)
	return 1;
    return cb->cb.cb_2(a, b, cb);
}

/*
 *
 */

struct BN_CTX {
    struct {
	BIGNUM **val;
	size_t used;
	size_t len;
    } bn;
    struct {
	size_t *val;
	size_t used;
	size_t len;
    } stack;
};

BN_CTX *
BN_CTX_new(void)
{
    struct BN_CTX *c;
    c = calloc(1, sizeof(*c));
    return c;
}

void
BN_CTX_free(BN_CTX *c)
{
    size_t i;
    for (i = 0; i < c->bn.len; i++)
	BN_free(c->bn.val[i]);
    free(c->bn.val);
    free(c->stack.val);
}

BIGNUM *
BN_CTX_get(BN_CTX *c)
{
    if (c->bn.used == c->bn.len) {
	void *ptr;
	size_t i;
	c->bn.len += 16;
	ptr = realloc(c->bn.val, c->bn.len * sizeof(c->bn.val[0]));
	if (ptr == NULL)
	    return NULL;
	c->bn.val = ptr;
	for (i = c->bn.used; i < c->bn.len; i++) {
	    c->bn.val[i] = BN_new();
	    if (c->bn.val[i] == NULL) {
		c->bn.len = i;
		return NULL;
	    }
	}
    }
    return c->bn.val[c->bn.used++];
}

void
BN_CTX_start(BN_CTX *c)
{
    if (c->stack.used == c->stack.len) {
	void *ptr;
	c->stack.len += 16;
	ptr = realloc(c->stack.val, c->stack.len * sizeof(c->stack.val[0]));
	if (ptr == NULL)
	    abort();
	c->stack.val = ptr;
    }
    c->stack.val[c->stack.used++] = c->bn.used;
}

void
BN_CTX_end(BN_CTX *c)
{
    const size_t prev = c->stack.val[c->stack.used - 1];
    size_t i;

    if (c->stack.used == 0)
	abort();

    for (i = prev; i < c->bn.used; i++)
	BN_clear(c->bn.val[i]);

    c->stack.used--;
    c->bn.used = prev;
}

