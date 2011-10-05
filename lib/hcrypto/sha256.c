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

#include "config.h"

#include "hash.h"
#include "sha.h"

#define Ch(x,y,z) (((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define ROTR(x,n)   (((x)>>(n)) | ((x) << (32 - (n))))

#define Sigma0(x)	(ROTR(x,2)  ^ ROTR(x,13) ^ ROTR(x,22))
#define Sigma1(x)	(ROTR(x,6)  ^ ROTR(x,11) ^ ROTR(x,25))
#define sigma0(x)	(ROTR(x,7)  ^ ROTR(x,18) ^ ((x)>>3))
#define sigma1(x)	(ROTR(x,17) ^ ROTR(x,19) ^ ((x)>>10))

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define E m->counter[4]
#define F m->counter[5]
#define G m->counter[6]
#define H m->counter[7]

static const uint32_t constant_256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void
SHA256_Init (SHA256_CTX *m)
{
    m->sz[0] = 0;
    m->sz[1] = 0;
    A = 0x6a09e667;
    B = 0xbb67ae85;
    C = 0x3c6ef372;
    D = 0xa54ff53a;
    E = 0x510e527f;
    F = 0x9b05688c;
    G = 0x1f83d9ab;
    H = 0x5be0cd19;
}

static void
calc (SHA256_CTX *m, uint32_t *in)
{
    uint32_t AA, BB, CC, DD, EE, FF, GG, HH;
    uint32_t data[64];
    int i;

    AA = A;
    BB = B;
    CC = C;
    DD = D;
    EE = E;
    FF = F;
    GG = G;
    HH = H;

    for (i = 0; i < 16; ++i)
	data[i] = in[i];
    for (i = 16; i < 64; ++i)
	data[i] = sigma1(data[i-2]) + data[i-7] +
	    sigma0(data[i-15]) + data[i - 16];

    for (i = 0; i < 64; i++) {
	uint32_t T1, T2;

	T1 = HH + Sigma1(EE) + Ch(EE, FF, GG) + constant_256[i] + data[i];
	T2 = Sigma0(AA) + Maj(AA,BB,CC);

	HH = GG;
	GG = FF;
	FF = EE;
	EE = DD + T1;
	DD = CC;
	CC = BB;
	BB = AA;
	AA = T1 + T2;
    }

    A += AA;
    B += BB;
    C += CC;
    D += DD;
    E += EE;
    F += FF;
    G += GG;
    H += HH;
}

/*
 * From `Performance analysis of MD5' by Joseph D. Touch <touch@isi.edu>
 */

#if !defined(WORDS_BIGENDIAN) || defined(_CRAY)
static inline uint32_t
swap_uint32_t (uint32_t t)
{
#define ROL(x,n) ((x)<<(n))|((x)>>(32-(n)))
    uint32_t temp1, temp2;

    temp1   = cshift(t, 16);
    temp2   = temp1 >> 8;
    temp1  &= 0x00ff00ff;
    temp2  &= 0x00ff00ff;
    temp1 <<= 8;
    return temp1 | temp2;
}
#endif

struct x32{
    unsigned int a:32;
    unsigned int b:32;
};

void
SHA256_Update (SHA256_CTX *m, const void *v, size_t len)
{
    const unsigned char *p = v;
    size_t old_sz = m->sz[0];
    size_t offset;

    m->sz[0] += len * 8;
    if (m->sz[0] < old_sz)
	++m->sz[1];
    offset = (old_sz / 8) % 64;
    while(len > 0){
	size_t l = min(len, 64 - offset);
	memcpy(m->save + offset, p, l);
	offset += l;
	p += l;
	len -= l;
	if(offset == 64){
#if !defined(WORDS_BIGENDIAN) || defined(_CRAY)
	    int i;
	    uint32_t current[16];
	    struct x32 *us = (struct x32*)m->save;
	    for(i = 0; i < 8; i++){
		current[2*i+0] = swap_uint32_t(us[i].a);
		current[2*i+1] = swap_uint32_t(us[i].b);
	    }
	    calc(m, current);
#else
	    calc(m, (uint32_t*)m->save);
#endif
	    offset = 0;
	}
    }
}

void
SHA256_Final (void *res, SHA256_CTX *m)
{
    unsigned char zeros[72];
    unsigned offset = (m->sz[0] / 8) % 64;
    unsigned int dstart = (120 - offset - 1) % 64 + 1;

    *zeros = 0x80;
    memset (zeros + 1, 0, sizeof(zeros) - 1);
    zeros[dstart+7] = (m->sz[0] >> 0) & 0xff;
    zeros[dstart+6] = (m->sz[0] >> 8) & 0xff;
    zeros[dstart+5] = (m->sz[0] >> 16) & 0xff;
    zeros[dstart+4] = (m->sz[0] >> 24) & 0xff;
    zeros[dstart+3] = (m->sz[1] >> 0) & 0xff;
    zeros[dstart+2] = (m->sz[1] >> 8) & 0xff;
    zeros[dstart+1] = (m->sz[1] >> 16) & 0xff;
    zeros[dstart+0] = (m->sz[1] >> 24) & 0xff;
    SHA256_Update (m, zeros, dstart + 8);
    {
	int i;
	unsigned char *r = (unsigned char*)res;

	for (i = 0; i < 8; ++i) {
	    r[4*i+3] = m->counter[i] & 0xFF;
	    r[4*i+2] = (m->counter[i] >> 8) & 0xFF;
	    r[4*i+1] = (m->counter[i] >> 16) & 0xFF;
	    r[4*i]   = (m->counter[i] >> 24) & 0xFF;
	}
    }
}
