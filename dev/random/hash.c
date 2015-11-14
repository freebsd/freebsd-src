/*-
 * Copyright (c) 2000-2015 Mark R V Murray
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#else /* !_KERNEL */
#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "unit_test.h"
#endif /* _KERNEL */

#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha2.h>

#include <dev/random/hash.h>

/* This code presumes that RANDOM_KEYSIZE is twice as large as RANDOM_BLOCKSIZE */
CTASSERT(RANDOM_KEYSIZE == 2*RANDOM_BLOCKSIZE);

/* Initialise the hash */
void
randomdev_hash_init(struct randomdev_hash *context)
{

	SHA256_Init(&context->sha);
}

/* Iterate the hash */
void
randomdev_hash_iterate(struct randomdev_hash *context, const void *data, size_t size)
{

	SHA256_Update(&context->sha, data, size);
}

/* Conclude by returning the hash in the supplied <*buf> which must be
 * RANDOM_KEYSIZE bytes long.
 */
void
randomdev_hash_finish(struct randomdev_hash *context, void *buf)
{

	SHA256_Final(buf, &context->sha);
}

/* Initialise the encryption routine by setting up the key schedule
 * from the supplied <*data> which must be RANDOM_KEYSIZE bytes of binary
 * data.
 */
void
randomdev_encrypt_init(struct randomdev_key *context, const void *data)
{

	rijndael_cipherInit(&context->cipher, MODE_ECB, NULL);
	rijndael_makeKey(&context->key, DIR_ENCRYPT, RANDOM_KEYSIZE*8, data);
}

/* Encrypt the supplied data using the key schedule preset in the context.
 * <length> bytes are encrypted from <*d_in> to <*d_out>. <length> must be
 * a multiple of RANDOM_BLOCKSIZE.
 */
void
randomdev_encrypt(struct randomdev_key *context, const void *d_in, void *d_out, u_int length)
{

	rijndael_blockEncrypt(&context->cipher, &context->key, d_in, length*8, d_out);
}
