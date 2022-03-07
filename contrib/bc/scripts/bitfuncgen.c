/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * A generator for bitwise operations test.
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#define NTESTS (100)

/**
 * Abort with an error message.
 * @param msg  The error message.
 */
void err(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	abort();
}

uint64_t rev(uint64_t a, size_t bits) {

	size_t i;
	uint64_t res = 0;

	for (i = 0; i < bits; ++i) {
		res <<= 1;
		res |= a & 1;
		a >>= 1;
	}

	return res;
}

uint64_t mod(uint64_t a, size_t bits) {

	uint64_t mod;

	if (bits < 64) mod = (uint64_t) ((1ULL << bits) - 1);
	else mod = UINT64_MAX;

	return a & mod;
}

uint64_t rol(uint64_t a, uint64_t p, size_t bits) {

	uint64_t res;

	assert(bits <= 64);

	p %= bits;

	if (!p) return a;

	res = (a << p) | (a >> (bits - p));

	return mod(res, bits);
}

uint64_t ror(uint64_t a, uint64_t p, size_t bits) {

	uint64_t res;

	assert(bits <= 64);

	p %= bits;

	if (!p) return a;

	res = (a << (bits - p)) | (a >> p);

	return mod(res, bits);
}

int main(void) {

	uint64_t a = 0, b = 0, t;
	size_t i;

	// We attempt to open this or /dev/random to get random data.
	int fd = open("/dev/urandom", O_RDONLY);

	if (fd < 0) {

		fd = open("/dev/random", O_RDONLY);

		if (fd < 0) err("cannot open a random number generator");
	}

	// Generate NTESTS tests.
	for (i = 0; i < NTESTS; ++i) {

		ssize_t nread;

		// Generate random data for the first operand.
		nread = read(fd, (char*) &a, sizeof(uint64_t));
		if (nread != sizeof(uint64_t)) err("I/O error");

		// Generate random data for the second operand.
		nread = read(fd, (char*) &b, sizeof(uint64_t));
		if (nread != sizeof(uint64_t)) err("I/O error");

		// Output the tests to stdout.
		printf("band(%lu, %lu)\n", a, b);
		printf("bor(%lu, %lu)\n", a, b);
		printf("bxor(%lu, %lu)\n", a, b);
		printf("bshl(%lu, %lu)\n", mod(a, 32), mod(b, 5));
		printf("bshr(%lu, %lu)\n", mod(a, 32), mod(b, 5));
		printf("bshl(%lu, %lu)\n", mod(b, 32), mod(a, 5));
		printf("bshr(%lu, %lu)\n", mod(b, 32), mod(a, 5));
		printf("bnot8(%lu)\nbnot8(%lu)\n", a, mod(a, 8));
		printf("bnot16(%lu)\nbnot16(%lu)\n", a, mod(a, 16));
		printf("bnot32(%lu)\nbnot32(%lu)\n", a, mod(a, 32));
		printf("bnot64(%lu)\n", a);
		printf("brev8(%lu)\nbrev8(%lu)\n", a, mod(a, 8));
		printf("brev16(%lu)\nbrev16(%lu)\n", a, mod(a, 16));
		printf("brev32(%lu)\nbrev32(%lu)\n", a, mod(a, 32));
		printf("brev64(%lu)\n", a);
		printf("brol8(%lu, %lu)\n", a, b);
		printf("brol8(%lu, %lu)\n", mod(a, 8), b);
		printf("brol8(%lu, %lu)\n", a, mod(b, 8));
		printf("brol8(%lu, %lu)\n", mod(a, 8), mod(b, 8));
		printf("brol16(%lu, %lu)\n", a, b);
		printf("brol16(%lu, %lu)\n", mod(a, 16), b);
		printf("brol16(%lu, %lu)\n", a, mod(b, 16));
		printf("brol16(%lu, %lu)\n", mod(a, 16), mod(b, 16));
		printf("brol32(%lu, %lu)\n", a, b);
		printf("brol32(%lu, %lu)\n", mod(a, 32), b);
		printf("brol32(%lu, %lu)\n", a, mod(b, 32));
		printf("brol32(%lu, %lu)\n", mod(a, 32), mod(b, 32));
		printf("brol64(%lu, %lu)\n", a, b);
		printf("bror8(%lu, %lu)\n", a, b);
		printf("bror8(%lu, %lu)\n", mod(a, 8), b);
		printf("bror8(%lu, %lu)\n", a, mod(b, 8));
		printf("bror8(%lu, %lu)\n", mod(a, 8), mod(b, 8));
		printf("bror16(%lu, %lu)\n", a, b);
		printf("bror16(%lu, %lu)\n", mod(a, 16), b);
		printf("bror16(%lu, %lu)\n", a, mod(b, 16));
		printf("bror16(%lu, %lu)\n", mod(a, 16), mod(b, 16));
		printf("bror32(%lu, %lu)\n", a, b);
		printf("bror32(%lu, %lu)\n", mod(a, 32), b);
		printf("bror32(%lu, %lu)\n", a, mod(b, 32));
		printf("bror32(%lu, %lu)\n", mod(a, 32), mod(b, 32));
		printf("bror64(%lu, %lu)\n", a, b);
		printf("bmod8(%lu)\nbmod8(%lu)\n", a, mod(a, 8));
		printf("bmod16(%lu)\nbmod16(%lu)\n", a, mod(a, 16));
		printf("bmod32(%lu)\nbmod32(%lu)\n", a, mod(a, 32));
		printf("bmod64(%lu)\n", a);

		// Output the results to stderr.
		fprintf(stderr, "%lu\n", a & b);
		fprintf(stderr, "%lu\n", a | b);
		fprintf(stderr, "%lu\n", a ^ b);
		fprintf(stderr, "%lu\n", mod(a, 32) << mod(b, 5));
		fprintf(stderr, "%lu\n", mod(a, 32) >> mod(b, 5));
		fprintf(stderr, "%lu\n", mod(b, 32) << mod(a, 5));
		fprintf(stderr, "%lu\n", mod(b, 32) >> mod(a, 5));
		t = mod(~a, 8);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		t = mod(~a, 16);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		t = mod(~a, 32);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		fprintf(stderr, "%lu\n", ~a);
		t = rev(a, 8);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		t = rev(a, 16);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		t = rev(a, 32);
		fprintf(stderr, "%lu\n%lu\n", t, t);
		t = rev(a, 64);
		fprintf(stderr, "%lu\n", t);
		fprintf(stderr, "%lu\n", rol(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", rol(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", rol(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", rol(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", rol(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", rol(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", rol(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", rol(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", rol(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", rol(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", rol(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", rol(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", rol(a, b, 64));
		fprintf(stderr, "%lu\n", ror(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", ror(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", ror(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", ror(mod(a, 8), mod(b, 8), 8));
		fprintf(stderr, "%lu\n", ror(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", ror(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", ror(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", ror(mod(a, 16), mod(b, 16), 16));
		fprintf(stderr, "%lu\n", ror(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", ror(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", ror(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", ror(mod(a, 32), mod(b, 32), 32));
		fprintf(stderr, "%lu\n", ror(a, b, 64));
		fprintf(stderr, "%lu\n%lu\n", mod(a, 8), mod(a, 8));
		fprintf(stderr, "%lu\n%lu\n", mod(a, 16), mod(a, 16));
		fprintf(stderr, "%lu\n%lu\n", mod(a, 32), mod(a, 32));
		fprintf(stderr, "%lu\n", a);
	}

	return 0;
}
