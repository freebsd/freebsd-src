/*-
 * Copyright (c) 2000 Mark R V Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/libkern.h>
#include <sys/random.h>
#include <sys/types.h>

#include <crypto/blowfish/blowfish.h>

#include <dev/random/hash.h>

/* initialise the hash by copying in some supplied data */
void
yarrow_hash_init(struct yarrowhash *context, void *data, size_t size)
{
	size_t count;

	count = size > KEYSIZE ? KEYSIZE : size;
	memset(context->hash, 0xff, KEYSIZE);
	memcpy(context->hash, data, count);
}

/* Do a Davies-Meyer hash using a block cipher.
 * H_0 = I
 * H_i = E_M_i(H_i-1) ^ H_i-1
 */
void
yarrow_hash_iterate(struct yarrowhash *context, void *data, size_t size)
{
	u_char keybuffer[KEYSIZE], temp[KEYSIZE];
	size_t count;
	int iteration, last, i;

	iteration = 0;
	last = 0;
	for (;;) {
		if (size <= KEYSIZE)
			last = 1;
		count = size > KEYSIZE ? KEYSIZE : size;
		memcpy(keybuffer, &((u_char *)data)[iteration], count);
		memset(&keybuffer[KEYSIZE - count], 0xff, count);
		BF_set_key(&context->hashkey, count,
			&((u_char *)data)[iteration]);
		BF_cbc_encrypt(context->hash, temp, KEYSIZE, &context->hashkey,
			context->ivec, BF_ENCRYPT);
		for (i = 0; i < KEYSIZE; i++)
			context->hash[i] ^= temp[i];
		if (last)
			break;
		iteration += KEYSIZE;
		size -= KEYSIZE;
	}
}

/* Conclude by returning a pointer to the data */
void
yarrow_hash_finish(struct yarrowhash *context, void *buf)
{
	memcpy(buf, context->hash, sizeof(context->hash));
}

/* Initialise the encryption routine by setting up the key schedule */
void
yarrow_encrypt_init(struct yarrowkey *context, void *data, size_t size)
{
	size_t count;

	count = size > KEYSIZE ? KEYSIZE : size;
	BF_set_key(&context->key, size, data);
}

/* Encrypt the supplied data using the key schedule preset in the context */
void
yarrow_encrypt(struct yarrowkey *context, void *d_in, void *d_out, size_t size)
{
	size_t count;
	int iteration, last;

	last = 0;
	for (iteration = 0;; iteration += KEYSIZE) {
		if (size <= KEYSIZE)
			last = 1;
		count = size > KEYSIZE ? KEYSIZE : size;
		BF_cbc_encrypt(&((u_char *)d_in)[iteration],
			&((u_char *)d_out)[iteration], count, &context->key,
			context->ivec, BF_ENCRYPT);
		if (last)
			break;
		size -= KEYSIZE;
	}
}
