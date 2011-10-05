/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#define A m->counter[0]
#define B m->counter[1]
#define C m->counter[2]
#define D m->counter[3]
#define E m->counter[4]
#define X data

void
SHA1_Init (struct sha *m)
{
  m->sz[0] = 0;
  m->sz[1] = 0;
  A = 0x67452301;
  B = 0xefcdab89;
  C = 0x98badcfe;
  D = 0x10325476;
  E = 0xc3d2e1f0;
}


#define F0(x,y,z) CRAYFIX((x & y) | (~x & z))
#define F1(x,y,z) (x ^ y ^ z)
#define F2(x,y,z) ((x & y) | (x & z) | (y & z))
#define F3(x,y,z) F1(x,y,z)

#define K0 0x5a827999
#define K1 0x6ed9eba1
#define K2 0x8f1bbcdc
#define K3 0xca62c1d6

#define DO(t,f,k) \
do { \
  uint32_t temp; \
 \
  temp = cshift(AA, 5) + f(BB,CC,DD) + EE + data[t] + k; \
  EE = DD; \
  DD = CC; \
  CC = cshift(BB, 30); \
  BB = AA; \
  AA = temp; \
} while(0)

static inline void
calc (struct sha *m, uint32_t *in)
{
  uint32_t AA, BB, CC, DD, EE;
  uint32_t data[80];
  int i;

  AA = A;
  BB = B;
  CC = C;
  DD = D;
  EE = E;

  for (i = 0; i < 16; ++i)
    data[i] = in[i];
  for (i = 16; i < 80; ++i)
    data[i] = cshift(data[i-3] ^ data[i-8] ^ data[i-14] ^ data[i-16], 1);

  /* t=[0,19] */

  DO(0,F0,K0);
  DO(1,F0,K0);
  DO(2,F0,K0);
  DO(3,F0,K0);
  DO(4,F0,K0);
  DO(5,F0,K0);
  DO(6,F0,K0);
  DO(7,F0,K0);
  DO(8,F0,K0);
  DO(9,F0,K0);
  DO(10,F0,K0);
  DO(11,F0,K0);
  DO(12,F0,K0);
  DO(13,F0,K0);
  DO(14,F0,K0);
  DO(15,F0,K0);
  DO(16,F0,K0);
  DO(17,F0,K0);
  DO(18,F0,K0);
  DO(19,F0,K0);

  /* t=[20,39] */

  DO(20,F1,K1);
  DO(21,F1,K1);
  DO(22,F1,K1);
  DO(23,F1,K1);
  DO(24,F1,K1);
  DO(25,F1,K1);
  DO(26,F1,K1);
  DO(27,F1,K1);
  DO(28,F1,K1);
  DO(29,F1,K1);
  DO(30,F1,K1);
  DO(31,F1,K1);
  DO(32,F1,K1);
  DO(33,F1,K1);
  DO(34,F1,K1);
  DO(35,F1,K1);
  DO(36,F1,K1);
  DO(37,F1,K1);
  DO(38,F1,K1);
  DO(39,F1,K1);

  /* t=[40,59] */

  DO(40,F2,K2);
  DO(41,F2,K2);
  DO(42,F2,K2);
  DO(43,F2,K2);
  DO(44,F2,K2);
  DO(45,F2,K2);
  DO(46,F2,K2);
  DO(47,F2,K2);
  DO(48,F2,K2);
  DO(49,F2,K2);
  DO(50,F2,K2);
  DO(51,F2,K2);
  DO(52,F2,K2);
  DO(53,F2,K2);
  DO(54,F2,K2);
  DO(55,F2,K2);
  DO(56,F2,K2);
  DO(57,F2,K2);
  DO(58,F2,K2);
  DO(59,F2,K2);

  /* t=[60,79] */

  DO(60,F3,K3);
  DO(61,F3,K3);
  DO(62,F3,K3);
  DO(63,F3,K3);
  DO(64,F3,K3);
  DO(65,F3,K3);
  DO(66,F3,K3);
  DO(67,F3,K3);
  DO(68,F3,K3);
  DO(69,F3,K3);
  DO(70,F3,K3);
  DO(71,F3,K3);
  DO(72,F3,K3);
  DO(73,F3,K3);
  DO(74,F3,K3);
  DO(75,F3,K3);
  DO(76,F3,K3);
  DO(77,F3,K3);
  DO(78,F3,K3);
  DO(79,F3,K3);

  A += AA;
  B += BB;
  C += CC;
  D += DD;
  E += EE;
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
SHA1_Update (struct sha *m, const void *v, size_t len)
{
  const unsigned char *p = v;
  size_t old_sz = m->sz[0];
  size_t offset;

  m->sz[0] += len * 8;
  if (m->sz[0] < old_sz)
      ++m->sz[1];
  offset = (old_sz / 8)  % 64;
  while(len > 0){
    size_t l = min(len, 64 - offset);
    memcpy(m->save + offset, p, l);
    offset += l;
    p += l;
    len -= l;
    if(offset == 64){
#if !defined(WORDS_BIGENDIAN) || defined(_CRAY)
      int i;
      uint32_t SHA1current[16];
      struct x32 *us = (struct x32*)m->save;
      for(i = 0; i < 8; i++){
	SHA1current[2*i+0] = swap_uint32_t(us[i].a);
	SHA1current[2*i+1] = swap_uint32_t(us[i].b);
      }
      calc(m, SHA1current);
#else
      calc(m, (uint32_t*)m->save);
#endif
      offset = 0;
    }
  }
}

void
SHA1_Final (void *res, struct sha *m)
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
  SHA1_Update (m, zeros, dstart + 8);
  {
      int i;
      unsigned char *r = (unsigned char*)res;

      for (i = 0; i < 5; ++i) {
	  r[4*i+3] = m->counter[i] & 0xFF;
	  r[4*i+2] = (m->counter[i] >> 8) & 0xFF;
	  r[4*i+1] = (m->counter[i] >> 16) & 0xFF;
	  r[4*i]   = (m->counter[i] >> 24) & 0xFF;
      }
  }
#if 0
  {
    int i;
    uint32_t *r = (uint32_t *)res;

    for (i = 0; i < 5; ++i)
      r[i] = swap_uint32_t (m->counter[i]);
  }
#endif
}
