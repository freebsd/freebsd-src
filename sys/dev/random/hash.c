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

#include <crypto/rijndael/rijndael.h>

#include <dev/random/hash.h>

/* initialise the hash by zeroing it */
void
yarrow_hash_init(struct yarrowhash *context)
{
	rijndael_cipherInit(&context->cipher, MODE_CBC, NULL);
	bzero(context->hash, KEYSIZE);
	context->partial = 0;
}

/* Do a Davies-Meyer hash using a block cipher.
 * H_0 = I
 * H_i = E_M_i(H_i-1) ^ H_i-1
 */
void
yarrow_hash_iterate(struct yarrowhash *context, void *data, size_t size)
{
	u_char temp[KEYSIZE];
	u_int i, j;
	union {
		void *pv;
		char *pc;
	} trans;

	trans.pv = data;
	for (i = 0; i < size; i++) {
		context->accum[context->partial++] = trans.pc[i];
		if (context->partial == (KEYSIZE - 1)) {
			rijndael_makeKey(&context->hashkey, DIR_ENCRYPT,
				KEYSIZE*8, context->accum);
			rijndael_blockEncrypt(&context->cipher,
				&context->hashkey, context->hash,
				KEYSIZE*8, temp);
			for (j = 0; j < KEYSIZE; j++)
				context->hash[j] ^= temp[j];
			bzero(context->accum, KEYSIZE);
			context->partial = 0;
		}
	}
}

/* Conclude by returning the hash in the supplied /buf/ which must be
 * KEYSIZE bytes long. Trailing data (less than KEYSIZE bytes) are
 * not forgotten.
 */
void
yarrow_hash_finish(struct yarrowhash *context, void *buf)
{
	u_char temp[KEYSIZE];
	int i;

	if (context->partial) {
		rijndael_makeKey(&context->hashkey, DIR_ENCRYPT,
			KEYSIZE*8, context->accum);
		rijndael_blockEncrypt(&context->cipher,
			&context->hashkey, context->hash,
			KEYSIZE*8, temp);
		for (i = 0; i < KEYSIZE; i++)
			context->hash[i] ^= temp[i];
	}
	memcpy(buf, context->hash, KEYSIZE);
	bzero(context->hash, KEYSIZE);
}

/* Initialise the encryption routine by setting up the key schedule
 * from the supplied /key/ which must be KEYSIZE bytes of binary
 * data.
 */
void
yarrow_encrypt_init(struct yarrowkey *context, void *data)
{
	rijndael_cipherInit(&context->cipher, MODE_CBC, NULL);
	rijndael_makeKey(&context->key, DIR_ENCRYPT, KEYSIZE*8, data);
}

/* Encrypt the supplied data using the key schedule preset in the context.
 * KEYSIZE bytes are encrypted from /d_in/ to /d_out/.
 */
void
yarrow_encrypt(struct yarrowkey *context, void *d_in, void *d_out)
{
	rijndael_blockEncrypt(&context->cipher, &context->key, d_in,
		KEYSIZE*8, d_out);
}
