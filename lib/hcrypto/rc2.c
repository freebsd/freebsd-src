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

#include <config.h>

#include "rc2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Implemented from Peter Gutmann's "Specification for Ron Rivests Cipher No.2"
 * rfc2268 and "On the Design and Security of RC2" was also useful.
 */

static unsigned int Sbox[256] = {
    0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed,
    0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d,
    0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76, 0x53, 0x8e,
    0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2,
    0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13,
    0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32,
    0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b,
    0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82,
    0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c,
    0x73, 0x56, 0xc0, 0x14, 0xa7, 0x8c, 0xf1, 0xdc,
    0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1,
    0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26,
    0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57,
    0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03,
    0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7,
    0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7,
    0x08, 0xe8, 0xea, 0xde, 0x80, 0x52, 0xee, 0xf7,
    0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a,
    0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74,
    0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec,
    0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc,
    0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
    0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a,
    0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55, 0x97, 0x31,
    0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae,
    0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9,
    0xd3, 0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c,
    0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9,
    0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff, 0xb0,
    0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e,
    0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77,
    0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad
};

void
RC2_set_key(RC2_KEY *key, int len, const unsigned char *data, int bits)
{
    unsigned char k[128];
    int j, T8, TM;

    if (len <= 0)
	abort();
    if (len > 128)
	len = 128;
    if (bits <= 0 || bits > 1024)
	bits = 1024;

    for (j = 0; j < len; j++)
	k[j] = data[j];
    for (; j < 128; j++)
	k[j] = Sbox[(k[j - len] + k[j - 1]) & 0xff];

    T8 = (bits + 7) / 8;
    j = (8*T8 - bits);
    TM = 0xff >> j;

    k[128 - T8] = Sbox[k[128 - T8] & TM];

    for (j = 127 - T8; j >= 0; j--)
	k[j] = Sbox[k[j + 1] ^ k[j + T8]];

    for (j = 0; j < 64; j++)
	key->data[j] = k[(j * 2) + 0] | (k[(j * 2) + 1] << 8);
    memset(k, 0, sizeof(k));
}

#define ROT16L(w,n)  ((w<<n)|(w>>(16-n)))
#define ROT16R(w,n)  ((w>>n)|(w<<(16-n)))

void
RC2_encryptc(unsigned char *in, unsigned char *out, const RC2_KEY *key)
{
    int i, j;
    int w0, w1, w2, w3;
    int t0, t1, t2, t3;

    w0 = in[0] | (in[1] << 8);
    w1 = in[2] | (in[3] << 8);
    w2 = in[4] | (in[5] << 8);
    w3 = in[6] | (in[7] << 8);

    for (i = 0; i < 16; i++) {
	j = i * 4;
	t0 = (w0 + (w1 & ~w3) + (w2 & w3) + key->data[j + 0]) & 0xffff;
	w0 = ROT16L(t0, 1);
	t1 = (w1 + (w2 & ~w0) + (w3 & w0) + key->data[j + 1]) & 0xffff;
	w1 = ROT16L(t1, 2);
	t2 = (w2 + (w3 & ~w1) + (w0 & w1) + key->data[j + 2]) & 0xffff;
	w2 = ROT16L(t2, 3);
	t3 = (w3 + (w0 & ~w2) + (w1 & w2) + key->data[j + 3]) & 0xffff;
	w3 = ROT16L(t3, 5);
	if(i == 4 || i == 10) {
	    w0 += key->data[w3 & 63];
	    w1 += key->data[w0 & 63];
	    w2 += key->data[w1 & 63];
	    w3 += key->data[w2 & 63];
	}
    }

    out[0] = w0 & 0xff;
    out[1] = (w0 >> 8) & 0xff;
    out[2] = w1 & 0xff;
    out[3] = (w1 >> 8) & 0xff;
    out[4] = w2 & 0xff;
    out[5] = (w2 >> 8) & 0xff;
    out[6] = w3 & 0xff;
    out[7] = (w3 >> 8) & 0xff;
}

void
RC2_decryptc(unsigned char *in, unsigned char *out, const RC2_KEY *key)
{
    int i, j;
    int w0, w1, w2, w3;
    int t0, t1, t2, t3;

    w0 = in[0] | (in[1] << 8);
    w1 = in[2] | (in[3] << 8);
    w2 = in[4] | (in[5] << 8);
    w3 = in[6] | (in[7] << 8);

    for (i = 15; i >= 0; i--) {
	j = i * 4;

	if(i == 4 || i == 10) {
	    w3 = (w3 - key->data[w2 & 63]) & 0xffff;
	    w2 = (w2 - key->data[w1 & 63]) & 0xffff;
	    w1 = (w1 - key->data[w0 & 63]) & 0xffff;
	    w0 = (w0 - key->data[w3 & 63]) & 0xffff;
	}

	t3 = ROT16R(w3, 5);
	w3 = (t3 - (w0 & ~w2) - (w1 & w2) - key->data[j + 3]) & 0xffff;
	t2 = ROT16R(w2, 3);
	w2 = (t2 - (w3 & ~w1) - (w0 & w1) - key->data[j + 2]) & 0xffff;
	t1 = ROT16R(w1, 2);
	w1 = (t1 - (w2 & ~w0) - (w3 & w0) - key->data[j + 1]) & 0xffff;
	t0 = ROT16R(w0, 1);
	w0 = (t0 - (w1 & ~w3) - (w2 & w3) - key->data[j + 0]) & 0xffff;

    }
    out[0] = w0 & 0xff;
    out[1] = (w0 >> 8) & 0xff;
    out[2] = w1 & 0xff;
    out[3] = (w1 >> 8) & 0xff;
    out[4] = w2 & 0xff;
    out[5] = (w2 >> 8) & 0xff;
    out[6] = w3 & 0xff;
    out[7] = (w3 >> 8) & 0xff;
}

void
RC2_cbc_encrypt(const unsigned char *in, unsigned char *out, long size,
		RC2_KEY *key, unsigned char *iv, int forward_encrypt)
{
    unsigned char tmp[RC2_BLOCK_SIZE];
    int i;

    if (forward_encrypt) {
	while (size >= RC2_BLOCK_SIZE) {
	    for (i = 0; i < RC2_BLOCK_SIZE; i++)
		tmp[i] = in[i] ^ iv[i];
	    RC2_encryptc(tmp, out, key);
	    memcpy(iv, out, RC2_BLOCK_SIZE);
	    size -= RC2_BLOCK_SIZE;
	    in += RC2_BLOCK_SIZE;
	    out += RC2_BLOCK_SIZE;
	}
	if (size) {
	    for (i = 0; i < size; i++)
		tmp[i] = in[i] ^ iv[i];
	    for (i = size; i < RC2_BLOCK_SIZE; i++)
		tmp[i] = iv[i];
	    RC2_encryptc(tmp, out, key);
	    memcpy(iv, out, RC2_BLOCK_SIZE);
	}
    } else {
	while (size >= RC2_BLOCK_SIZE) {
	    memcpy(tmp, in, RC2_BLOCK_SIZE);
	    RC2_decryptc(tmp, out, key);
	    for (i = 0; i < RC2_BLOCK_SIZE; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, RC2_BLOCK_SIZE);
	    size -= RC2_BLOCK_SIZE;
	    in += RC2_BLOCK_SIZE;
	    out += RC2_BLOCK_SIZE;
	}
	if (size) {
	    memcpy(tmp, in, RC2_BLOCK_SIZE);
	    RC2_decryptc(tmp, out, key);
	    for (i = 0; i < size; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, RC2_BLOCK_SIZE);
	}
    }
}
