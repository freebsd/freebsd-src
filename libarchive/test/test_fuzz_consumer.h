/*-
 * Copyright (c) 2025 Tim Kientzle
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

/*
 * C99 equivalent of the DataConsumer class used by the C++ fuzzers.
 * Consumes bytes sequentially from a buffer, matching the fuzzer protocol.
 * Not all consumers use every function, so suppress unused warnings.
 */
#ifndef TEST_FUZZ_CONSUMER_H
#define TEST_FUZZ_CONSUMER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define FUZZ_UNUSED __attribute__((unused))
#else
#define FUZZ_UNUSED
#endif

struct fuzz_consumer {
	const uint8_t *data;
	size_t size;
	size_t pos;
	char string_buf[512];
};

FUZZ_UNUSED static void
fuzz_consumer_init(struct fuzz_consumer *c, const uint8_t *data, size_t size)
{
	c->data = data;
	c->size = size;
	c->pos = 0;
}

FUZZ_UNUSED static size_t
fuzz_consumer_remaining(const struct fuzz_consumer *c)
{
	return c->size - c->pos;
}

FUZZ_UNUSED static uint8_t
fuzz_consume_byte(struct fuzz_consumer *c)
{
	if (c->pos >= c->size)
		return 0;
	return c->data[c->pos++];
}

FUZZ_UNUSED static uint16_t
fuzz_consume_u16(struct fuzz_consumer *c)
{
	uint16_t val = 0;
	if (c->pos + 2 <= c->size) {
		val = (uint16_t)c->data[c->pos]
		    | ((uint16_t)c->data[c->pos + 1] << 8);
		c->pos += 2;
	}
	return val;
}

FUZZ_UNUSED static uint32_t
fuzz_consume_u32(struct fuzz_consumer *c)
{
	uint32_t val = 0;
	if (c->pos + 4 <= c->size) {
		val = (uint32_t)c->data[c->pos]
		    | ((uint32_t)c->data[c->pos + 1] << 8)
		    | ((uint32_t)c->data[c->pos + 2] << 16)
		    | ((uint32_t)c->data[c->pos + 3] << 24);
		c->pos += 4;
	}
	return val;
}

FUZZ_UNUSED static int64_t
fuzz_consume_i64(struct fuzz_consumer *c)
{
	int64_t val = 0;
	if (c->pos + 8 <= c->size) {
		int i;
		for (i = 0; i < 8; i++)
			val |= (int64_t)c->data[c->pos + i] << (8 * i);
		c->pos += 8;
	}
	return val;
}

FUZZ_UNUSED static const char *
fuzz_consume_string(struct fuzz_consumer *c, size_t max_len)
{
	size_t avail, len, actual_len;
	if (max_len > sizeof(c->string_buf) - 1)
		max_len = sizeof(c->string_buf) - 1;
	avail = c->size - c->pos;
	len = (avail < max_len) ? avail : max_len;
	actual_len = 0;
	while (actual_len < len && c->pos < c->size) {
		char ch = (char)c->data[c->pos++];
		if (ch == '\0')
			break;
		c->string_buf[actual_len++] = ch;
	}
	c->string_buf[actual_len] = '\0';
	return c->string_buf;
}

FUZZ_UNUSED static size_t
fuzz_consume_bytes(struct fuzz_consumer *c, void *out, size_t len)
{
	size_t avail = c->size - c->pos;
	size_t to_copy = (avail < len) ? avail : len;
	if (to_copy > 0) {
		memcpy(out, c->data + c->pos, to_copy);
		c->pos += to_copy;
	}
	return to_copy;
}

#endif /* TEST_FUZZ_CONSUMER_H */
