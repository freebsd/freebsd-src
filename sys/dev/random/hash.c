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
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>

/* initialise the hash */
void
yarrow_hash_init(struct yarrowhash *context)
{
	SHA256_Init(&context->sha);
}

/* iterate the hash */
void
yarrow_hash_iterate(struct yarrowhash *context, void *data, size_t size)
{
	SHA256_Update(&context->sha, data, size);
}

/* Conclude by returning the hash in the supplied /buf/ which must be
 * KEYSIZE bytes long.
 */
void
yarrow_hash_finish(struct yarrowhash *context, void *buf)
{
	SHA256_Final(buf, &context->sha);
}

/* Initialise the encryption routine by setting up the key schedule
 * from the supplied /data/ which must be KEYSIZE bytes of binary
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
