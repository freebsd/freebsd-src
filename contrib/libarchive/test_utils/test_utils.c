/*
 * Copyright (c) 2003-2012 Tim Kientzle
 * Copyright (c) 2012 Andres Mejia
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_utils.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static inline uint64_t
xorshift64(uint64_t *state)
{
	uint64_t x = *state;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	*state = x;
	return (x);
}

/*
 * Fill a buffer with reproducible pseudo-random data using a simple xorshift
 * algorithm. Originally, most tests filled buffers with a loop that calls
 * rand() once for each byte. However, this initialization can be extremely
 * slow when running on emulated platforms such as QEMU where 16M calls to
 * rand() take a long time: Before the test_write_format_7zip_large_copy test
 * took ~22 seconds, whereas using a xorshift random number generator (that can
 * be inlined) reduces it to ~17 seconds on QEMU RISC-V.
 */
static void
fill_with_pseudorandom_data_seed(uint64_t seed, void *buffer, size_t size)
{
	uint64_t *aligned_buffer;
	size_t num_values;
	size_t i;
	size_t unaligned_suffix;
	size_t unaligned_prefix = 0;
	/*
	 * To avoid unaligned stores we only fill the aligned part of the buffer
	 * with pseudo-random data and fill the unaligned prefix with 0xab and
	 * the suffix with 0xcd.
	 */
	if ((uintptr_t)buffer % sizeof(uint64_t)) {
		unaligned_prefix =
		    sizeof(uint64_t) - (uintptr_t)buffer % sizeof(uint64_t);
		aligned_buffer =
		    (uint64_t *)((char *)buffer + unaligned_prefix);
		memset(buffer, 0xab, unaligned_prefix);
	} else {
		aligned_buffer = (uint64_t *)buffer;
	}
	assert((uintptr_t)aligned_buffer % sizeof(uint64_t) == 0);
	num_values = (size - unaligned_prefix) / sizeof(uint64_t);
	unaligned_suffix =
	    size - unaligned_prefix - num_values * sizeof(uint64_t);
	for (i = 0; i < num_values; i++) {
		aligned_buffer[i] = xorshift64(&seed);
	}
	if (unaligned_suffix) {
		memset((char *)buffer + size - unaligned_suffix, 0xcd,
		    unaligned_suffix);
	}
}

void
fill_with_pseudorandom_data(void *buffer, size_t size)
{
	uint64_t seed;
	const char* seed_str;
	/*
	 * Check if a seed has been specified in the environment, otherwise fall
	 * back to using rand() as a seed.
	 */
	if ((seed_str = getenv("TEST_RANDOM_SEED")) != NULL) {
		errno = 0;
		seed = strtoull(seed_str, NULL, 10);
		if (errno != 0) {
			fprintf(stderr, "strtoull(%s) failed: %s", seed_str,
			    strerror(errno));
			seed = rand();
		}
	} else {
		seed = rand();
	}
	fill_with_pseudorandom_data_seed(seed, buffer, size);
}
