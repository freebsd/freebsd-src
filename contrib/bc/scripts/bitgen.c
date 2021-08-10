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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define NTESTS (100)

/**
 * Abort with an error message.
 * @param msg  The error message.
 */
void err(const char *msg) {
	fprintf(stderr, "%s\n", msg);
	abort();
}

int main(void) {

	uint64_t a = 0, b = 0;
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
		printf("blshift(%llu, %lu)\n", a & ((1ULL << 32) - 1), b & 31);
		printf("brshift(%llu, %lu)\n", a & ((1ULL << 32) - 1), b & 31);
		printf("blshift(%llu, %lu)\n", b & ((1ULL << 32) - 1), a & 31);
		printf("brshift(%llu, %lu)\n", b & ((1ULL << 32) - 1), a & 31);

		// Output the results to stderr.
		fprintf(stderr, "%lu\n", a & b);
		fprintf(stderr, "%lu\n", a | b);
		fprintf(stderr, "%lu\n", a ^ b);
		fprintf(stderr, "%llu\n", (a & ((1ULL << 32) - 1)) << (b & 31));
		fprintf(stderr, "%llu\n", (a & ((1ULL << 32) - 1)) >> (b & 31));
		fprintf(stderr, "%llu\n", (b & ((1ULL << 32) - 1)) << (a & 31));
		fprintf(stderr, "%llu\n", (b & ((1ULL << 32) - 1)) >> (a & 31));
	}

	return 0;
}
