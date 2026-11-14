/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef _EVENTLOG_FIXTURES_H_
#define _EVENTLOG_FIXTURES_H_

#include <sys/endian.h>
#include <stddef.h>
#include <stdint.h>

/* Byte arrays keep the wire layout independent of host alignment and order. */
enum {
	ALG_SHA1 = 0x0004,
	ALG_SHA256 = 0x000b,
};

struct event1_header {
	uint8_t pcr_index[4];
	uint8_t event_type[4];
	uint8_t digest[20];
	uint8_t event_size[4];
};

struct legacy_record {
	struct event1_header header;
	uint8_t data[3];
};

static const struct legacy_record legacy_event = {
	.header.pcr_index = { 8 },
	.header.event_type = { 13 },	/* EV_IPL */
	.header.digest = { 0xa5 },
	.header.event_size = { sizeof(legacy_event.data) },
	.data = { 'a', 'b', 'c' },
};

struct spec_id_header {
	struct event1_header event;
	uint8_t signature[16];
	uint8_t platform_class[4];
	uint8_t minor, major, errata, uintn_size;
	uint8_t algorithm_count[4];
};

struct spec_id_algorithm {
	uint8_t id[2];
	uint8_t size[2];
};

static inline struct spec_id_header
spec_id_header(size_t record_size, uint32_t bank_count)
{
	struct spec_id_header hdr = {
		.event.event_type = { 3 },	/* EV_NO_ACTION */
		.signature = "Spec ID Event03",
		.major = 2,
		.uintn_size = 2,		/* 64-bit UINTN */
	};

	le32enc(hdr.event.event_size, record_size - sizeof(hdr.event));
	le32enc(hdr.algorithm_count, bank_count);
	return (hdr);
}

struct event2_header {
	uint8_t pcr_index[4];
	uint8_t event_type[4];
	uint8_t digest_count[4];
};

struct sha256_digest {
	uint8_t id[2];
	uint8_t bytes[32];
};

struct final_table_header {
	uint8_t version[8];
	uint8_t event_count[8];
};

#endif
