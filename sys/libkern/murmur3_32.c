/*-
 * Copyright (c) 2014 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/hash.h>
#include <sys/endian.h>
#include <sys/stdint.h>
#include <sys/types.h>

#define rol32(i32, n) ((i32) << (n) | (i32) >> (32 - (n)))

/*
 * $FreeBSD$
 * Simple implementation of the Murmur3-32 hash function optimized for
 * aligned sequences of 32-bit words.  If len is not a multiple of 4, it
 * will be rounded down, droping trailer bytes.
 */
uint32_t
murmur3_aligned_32(const void *data, size_t len, uint32_t seed)
{
	const uint32_t *data32;
	uint32_t hash, k;
	size_t res;

	/* initialize */
	len -= len % sizeof(*data32);
	res = len;
	data32 = data;
	hash = seed;

	/* iterate */
	for (res = 0; res < len; res += sizeof(*data32), data32++) {
		k = le32toh(*data32);
		k *= 0xcc9e2d51;
		k = rol32(k, 15);
		k *= 0x1b873593;
		hash ^= k;
		hash = rol32(hash, 13);
		hash *= 5;
		hash += 0xe6546b64;
	}

	/* finalize */
	hash ^= (uint32_t)len;
	hash ^= hash >> 16;
	hash *= 0x85ebca6b;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35;
	hash ^= hash >> 16;
	return (hash);
}

