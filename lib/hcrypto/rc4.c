/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

/* implemented from description in draft-kaukonen-cipher-arcfour-03.txt */

#include "config.h"

#include <rc4.h>

#define SWAP(k,x,y)				\
{ unsigned int _t; 				\
  _t = k->state[x]; 				\
  k->state[x] = k->state[y]; 			\
  k->state[y] = _t;				\
}

void
RC4_set_key(RC4_KEY *key, const int len, const unsigned char *data)
{
    int i, j;

    for (i = 0; i < 256; i++)
	key->state[i] = i;
    for (i = 0, j = 0; i < 256; i++) {
	j = (j + key->state[i] + data[i % len]) % 256;
	SWAP(key, i, j);
    }
    key->x = key->y = 0;
}

void
RC4(RC4_KEY *key, const int len, const unsigned char *in, unsigned char *out)
{
    int i, t;
    unsigned x, y;

    x = key->x;
    y = key->y;
    for (i = 0; i < len; i++) {
	x = (x + 1) % 256;
	y = (y + key->state[x]) % 256;
	SWAP(key, x, y);
	t = (key->state[x] + key->state[y]) % 256;
	*out++ = key->state[t] ^ *in++;
    }
    key->x = x;
    key->y = y;
}
