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

#define KEYSIZE		32	/* 32 bytes == 256 bits */

struct yarrowhash {		/* Big! Make static! */
	BF_KEY hashkey;		/* Data cycles through here */
	u_char ivec[8];		/* Blowfish Internal */
	u_char hash[KEYSIZE];	/* Repeatedly encrypted */
};

struct yarrowkey {		/* Big! Make static! */
	BF_KEY key;		/* Key schedule */
	u_char ivec[8];		/* Blowfish Internal */
};

void yarrow_hash_init(struct yarrowhash *, void *, size_t);
void yarrow_hash_iterate(struct yarrowhash *, void *, size_t);
void yarrow_hash_finish(struct yarrowhash *, void *);
void yarrow_encrypt_init(struct yarrowkey *, void *, size_t);
void yarrow_encrypt(struct yarrowkey *context, void *, void *, size_t);
