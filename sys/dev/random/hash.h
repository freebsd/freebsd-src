/*-
 * Copyright (c) 2000-2013 Mark R V Murray
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

#ifndef SYS_DEV_RANDOM_HASH_H_INCLUDED
#define SYS_DEV_RANDOM_HASH_H_INCLUDED

#define	KEYSIZE		32	/* (in bytes) == 256 bits */
#define	BLOCKSIZE	16	/* (in bytes) == 128 bits */

struct randomdev_hash {		/* Big! Make static! */
	SHA256_CTX	sha;
};

struct randomdev_key {		/* Big! Make static! */
	keyInstance key;	/* Key schedule */
	cipherInstance cipher;	/* Rijndael internal */
};

void randomdev_hash_init(struct randomdev_hash *);
void randomdev_hash_iterate(struct randomdev_hash *, void *, size_t);
void randomdev_hash_finish(struct randomdev_hash *, void *);
void randomdev_encrypt_init(struct randomdev_key *, void *);
void randomdev_encrypt(struct randomdev_key *context, void *, void *, unsigned);

#endif
