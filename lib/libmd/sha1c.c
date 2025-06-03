/*-
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 * Copyright (c) 2024 Robert Clausecker <fuz@freebsd.org>
 *
 * Adapted from Go's crypto/sha1/sha1.go.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *   * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <sha.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/endian.h>

#ifdef SHA1_ASM
extern void	sha1_block(SHA1_CTX *, const void *, size_t);
#else
static void	sha1_block(SHA1_CTX *, const void *, size_t);
#endif

#define INIT0 0x67452301
#define INIT1 0xEFCDAB89
#define INIT2 0x98BADCFE
#define INIT3 0x10325476
#define INIT4 0xC3D2E1F0

#define K0 0x5A827999
#define K1 0x6ED9EBA1
#define K2 0x8F1BBCDC
#define K3 0xCA62C1D6

void
SHA1_Init(SHA1_CTX *c)
{
	c->h0 = INIT0;
	c->h1 = INIT1;
	c->h2 = INIT2;
	c->h3 = INIT3;
	c->h4 = INIT4;
	c->Nl = 0;
	c->Nh = 0;
	c->num = 0;
}

void
SHA1_Update(SHA1_CTX *c, const void *data, size_t len)
{
	uint64_t nn;
	const char *p = data;

	nn = (uint64_t)c->Nl | (uint64_t)c->Nh << 32;
	nn += len;
	c->Nl = (uint32_t)nn;
	c->Nh = (uint32_t)(nn >> 32);

	if (c->num > 0) {
		size_t n = SHA_CBLOCK - c->num;

		if (n > len)
			n = len;

		memcpy((char *)c->data + c->num, p, n);
		c->num += n;
		if (c->num == SHA_CBLOCK) {
			sha1_block(c, (void *)c->data, SHA_CBLOCK);
			c->num = 0;
		}

		p += n;
		len -= n;
	}

	if (len >= SHA_CBLOCK) {
		size_t n = len & ~(size_t)(SHA_CBLOCK - 1);

		sha1_block(c, p, n);
		p += n;
		len -= n;
	}

	if (len > 0) {
		memcpy(c->data, p, len);
		c->num = len;
	}
}

void
SHA1_Final(unsigned char *md, SHA1_CTX *c)
{
	uint64_t len;
	size_t t;
	unsigned char tmp[SHA_CBLOCK + sizeof(uint64_t)] = {0x80, 0};

	len = (uint64_t)c->Nl | (uint64_t)c->Nh << 32;
	t = 64 + 56 - c->Nl % 64;
	if (t > 64)
		t -= 64;

	/* length in bits */
	len <<= 3;
	be64enc(tmp + t, len);
	SHA1_Update(c, tmp, t + 8);
	assert(c->num == 0);

	be32enc(md +  0, c->h0);
	be32enc(md +  4, c->h1);
	be32enc(md +  8, c->h2);
	be32enc(md + 12, c->h3);
	be32enc(md + 16, c->h4);

	explicit_bzero(c, sizeof(*c));
}

#ifndef SHA1_ASM
static void
/* invariant: len is a multiple of SHA_CBLOCK */
sha1_block(SHA1_CTX *c, const void *data, size_t len)
{
	uint32_t w[16];
	uint32_t h0 = c->h0, h1 = c->h1, h2 = c->h2, h3 = c->h3, h4 = c->h4;
	const char *p = data;

	while (len >= SHA_CBLOCK) {
		size_t i;
		uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
		uint32_t f, t, tmp;

#		pragma unroll
		for (i = 0; i < 16; i++)
			w[i] = be32dec(p + 4*i);

#		pragma unroll
		for (i = 0; i < 16; i++) {
			f = b & c | ~b & d;
			t = (a << 5 | a >> 32 - 5) + f + e + w[i & 0xf] + K0;
			e = d;
			d = c;
			c = b << 30 | b >> 32 - 30;
			b = a;
			a = t;
		}

#		pragma unroll
		for (; i < 20; i++) {
			tmp = w[i - 3 & 0xf] ^ w[i - 8 & 0xf] ^ w[i - 14 & 0xf] ^ w[i & 0xf];
			w[i & 0xf] = tmp << 1 | tmp >> 32 - 1;

			f = b & c | ~b & d;
			t = (a << 5 | a >> 32 - 5) + f + e + w[i & 0xf] + K0;
			e = d;
			d = c;
			c = b << 30 | b >> 32 - 30;
			b = a;
			a = t;
		}

#		pragma unroll
		for (; i < 40; i++) {
			tmp = w[i - 3 & 0xf] ^ w[i - 8 & 0xf] ^ w[i - 14 & 0xf] ^ w[i & 0xf];
			w[i & 0xf] = tmp << 1 | tmp >> 32 - 1;

			f = b ^ c ^ d;
			t = (a << 5 | a >> 32 - 5) + f + e + w[i & 0xf] + K1;
			e = d;
			d = c;
			c = b << 30 | b >> 32 - 30;
			b = a;
			a = t;
		}

#		pragma unroll
		for (; i < 60; i++) {
			tmp = w[i - 3 & 0xf] ^ w[i - 8 & 0xf] ^ w[i - 14 & 0xf] ^ w[i & 0xf];
			w[i & 0xf] = tmp << 1 | tmp >> 32 - 1;

			f = (b | c) & d | b & c;
			t = (a << 5 | a >> 32 - 5) + f + e + w[i & 0xf] + K2;
			e = d;
			d = c;
			c = b << 30 | b >> 32 - 30;
			b = a;
			a = t;
		}

#		pragma unroll
		for (; i < 80; i++) {
			tmp = w[i - 3 & 0xf] ^ w[i - 8 & 0xf] ^ w[i - 14 & 0xf] ^ w[i & 0xf];
			w[i & 0xf] = tmp << 1 | tmp >> 32 - 1;

			f = b ^ c ^ d;
			t = (a << 5 | a >> 32 - 5) + f + e + w[i & 0xf] + K3;
			e = d;
			d = c;
			c = b << 30 | b >> 32 - 30;
			b = a;
			a = t;
		}

		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;

		p += SHA_CBLOCK;
		len -= SHA_CBLOCK;
	}

	c->h0 = h0;
	c->h1 = h1;
	c->h2 = h2;
	c->h3 = h3;
	c->h4 = h4;
}
#endif

#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef SHA_Init
__weak_reference(_libmd_SHA_Init, SHA_Init);
#undef SHA_Update
__weak_reference(_libmd_SHA_Update, SHA_Update);
#undef SHA_Final
__weak_reference(_libmd_SHA_Final, SHA_Final);
#undef SHA1_Init
__weak_reference(_libmd_SHA1_Init, SHA1_Init);
#undef SHA1_Update
__weak_reference(_libmd_SHA1_Update, SHA1_Update);
#undef SHA1_Final
__weak_reference(_libmd_SHA1_Final, SHA1_Final);
#endif
