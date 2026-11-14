/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <dev/tpm/tpm_eventlog.h>
#include <atf-c.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "eventlog_fixtures.h"

struct spec_id_record {
	struct spec_id_header header;
	struct spec_id_algorithm banks[2];
	uint8_t vendor_info_size;
};

static struct spec_id_record
specid(void)
{
	struct spec_id_record spec = {
		.banks = {
			{ { ALG_SHA1 }, { 20 } },
			{ { ALG_SHA256 }, { 32 } },
		},
	};

	spec.header = spec_id_header(sizeof(spec), nitems(spec.banks));
	return (spec);
}

struct sha1_digest {
	uint8_t id[2];
	uint8_t bytes[20];
};

struct ipl_payload {
	uint8_t size[4];
	uint8_t data[3];
};

struct ipl_event {
	struct event2_header header;
	struct sha1_digest sha1;
	struct sha256_digest sha256;
	struct ipl_payload payload;
};

static const struct ipl_event ipl = {
	.header.pcr_index = { 8 },
	.header.event_type = { 13 },	/* EV_IPL */
	.header.digest_count = { 2 },
	.sha1 = { { ALG_SHA1 }, { 0xa5 } },
	.sha256 = { { ALG_SHA256 }, { 0x5a } },
	.payload.size = { sizeof(ipl.payload.data) },
	.payload.data = { 'a', 'b', 'c' },
};

ATF_TC_WITHOUT_HEAD(records);
ATF_TC_BODY(records, tc)
{
	struct tpm_eventlog_algorithms algs;
	struct spec_id_record spec = specid();
	size_t log_size = sizeof(spec) + sizeof(ipl);
	uint8_t buf[1 + sizeof(spec) + sizeof(ipl) + 1]
	    __aligned(sizeof(uint64_t)) = { 0 };
	uint8_t *log = buf + 1;	/* Deliberately unaligned. */
	uint8_t *event = log + sizeof(spec);
	uint8_t *digests = event + sizeof(ipl.header);

	memcpy(log, &spec, sizeof(spec));
	memcpy(event, &ipl, sizeof(ipl));
	ATF_REQUIRE_EQ(tpm_eventlog_specid(log, sizeof(spec), &algs),
	    sizeof(spec));
	ATF_CHECK_EQ(tpm_eventlog_event2(event, sizeof(ipl), &algs),
	    sizeof(ipl));
	ATF_CHECK(tpm_eventlog_valid(log, sizeof(spec), TPM_EVENTLOG_TCG2));
	ATF_CHECK(tpm_eventlog_valid(log, log_size, TPM_EVENTLOG_TCG2));
	ATF_CHECK(!tpm_eventlog_valid(log, log_size, UINT32_MAX));
	ATF_CHECK(!tpm_eventlog_valid(log, log_size + 1, TPM_EVENTLOG_TCG2));

	/* Algorithms may appear in a different order in each event. */
	memcpy(digests, &ipl.sha256, sizeof(ipl.sha256));
	memcpy(digests + sizeof(ipl.sha256), &ipl.sha1, sizeof(ipl.sha1));
	ATF_CHECK(tpm_eventlog_valid(log, log_size, TPM_EVENTLOG_TCG2));
}

ATF_TC_WITHOUT_HEAD(malformed);
ATF_TC_BODY(malformed, tc)
{
	struct tpm_eventlog_algorithms algs;
	struct spec_id_record spec = specid();
	struct ipl_event event = ipl;
	const void *spec_bytes = &spec, *event_bytes = &event;
	/* Otherwise complete, including the payload after two SHA-1 digests. */
	struct {
		struct event2_header header;
		struct sha1_digest digests[2];
		struct ipl_payload payload;
	} duplicate = { ipl.header, { ipl.sha1, ipl.sha1 }, ipl.payload };

	ATF_REQUIRE_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs),
	    sizeof(spec));
	le32enc(event.header.digest_count, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_event2(event_bytes, sizeof(event), &algs), 0);
	ATF_CHECK_EQ(tpm_eventlog_event2((const void *)&duplicate,
	    sizeof(duplicate), &algs), 0);
	event = ipl;
	le16enc(event.sha1.id, UINT16_MAX);
	ATF_CHECK_EQ(tpm_eventlog_event2(event_bytes, sizeof(event), &algs), 0);
	event = ipl;
	le32enc(event.payload.size, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_event2(event_bytes, sizeof(event), &algs), 0);

	le32enc(spec.header.algorithm_count, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs), 0);
	spec = specid();
	le16enc(spec.banks[1].id, ALG_SHA1);
	ATF_CHECK_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs), 0);
	spec = specid();
	le16enc(spec.banks[0].size, 0);
	ATF_CHECK_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs), 0);
	spec = specid();
	spec.vendor_info_size = 1;
	ATF_CHECK_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs), 0);
	spec = specid();
	spec.header.event.digest[0] = 1;
	ATF_CHECK_EQ(tpm_eventlog_specid(spec_bytes, sizeof(spec), &algs), 0);
}

ATF_TC_WITHOUT_HEAD(short_records);
ATF_TC_BODY(short_records, tc)
{
	struct tpm_eventlog_algorithms algs;
	struct spec_id_record spec = specid();
	uint8_t *map, *end;
	size_t i, pagesize;

	pagesize = getpagesize();
	map = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0);
	ATF_REQUIRE(map != MAP_FAILED);
	end = map + pagesize;
	ATF_REQUIRE_EQ(mprotect(end, pagesize, PROT_NONE), 0);
	for (i = 0; i < sizeof(spec); i++) {
		memcpy(end - i, &spec, i);
		ATF_CHECK_EQ(tpm_eventlog_specid(end - i, i, &algs), 0);
	}
	ATF_REQUIRE_EQ(tpm_eventlog_specid((const void *)&spec, sizeof(spec),
	    &algs), sizeof(spec));
	for (i = 0; i < sizeof(ipl); i++) {
		memcpy(end - i, &ipl, i);
		ATF_CHECK_EQ(tpm_eventlog_event2(end - i, i, &algs), 0);
	}
	for (i = 0; i < sizeof(legacy_event); i++) {
		memcpy(end - i, &legacy_event, i);
		ATF_CHECK_EQ(tpm_eventlog_event1(end - i, i), 0);
	}
	ATF_REQUIRE_EQ(munmap(map, pagesize * 2), 0);
}

ATF_TC_WITHOUT_HEAD(final_events);
ATF_TC_BODY(final_events, tc)
{
	struct tpm_eventlog_algorithms algs;
	struct spec_id_record spec = specid();
	struct final_table {
		struct final_table_header header;
		struct ipl_event events[2];
	} table = {
		.header.version = { 1 },
		.header.event_count = { 2 },
		.events = { ipl, ipl },
	};
	const void *bytes = &table;
	size_t start, end, i;

	ATF_REQUIRE_EQ(tpm_eventlog_specid((const void *)&spec, sizeof(spec),
	    &algs), sizeof(spec));
	ATF_REQUIRE(tpm_eventlog_final(bytes, sizeof(table), &algs, 1,
	    &start, &end));
	ATF_CHECK_EQ(start, offsetof(struct final_table, events[1]));
	ATF_CHECK_EQ(end, sizeof(table));
	ATF_REQUIRE(tpm_eventlog_final(bytes, sizeof(table), &algs, 2,
	    &start, &end));
	ATF_CHECK_EQ(start, end);
	ATF_REQUIRE(tpm_eventlog_final(bytes, sizeof(table), &algs, 0,
	    &start, &end));
	ATF_CHECK_EQ(start, sizeof(table.header));
	ATF_CHECK(!tpm_eventlog_final(bytes, sizeof(table), &algs, 3,
	    &start, &end));
	for (i = 0; i < sizeof(table); i++)
		ATF_CHECK(!tpm_eventlog_final(bytes, i, &algs, 0,
		    &start, &end));
	le64enc(table.header.event_count, UINT64_MAX);
	ATF_CHECK(!tpm_eventlog_final(bytes, sizeof(table), &algs, 0,
	    &start, &end));
	le64enc(table.header.event_count, 0);
	ATF_REQUIRE(tpm_eventlog_final(bytes, sizeof(table.header), &algs, 0,
	    &start, &end));
	ATF_CHECK_EQ(start, end);
}

ATF_TC_WITHOUT_HEAD(legacy_records);
ATF_TC_BODY(legacy_records, tc)
{
	uint8_t log[2 * sizeof(legacy_event)];
	struct event1_header header = legacy_event.header;

	memcpy(log, &legacy_event, sizeof(legacy_event));
	memcpy(log + sizeof(legacy_event), &legacy_event, sizeof(legacy_event));
	ATF_CHECK_EQ(tpm_eventlog_event1(log, sizeof(legacy_event)),
	    sizeof(legacy_event));
	ATF_CHECK(tpm_eventlog_valid(log, sizeof(legacy_event),
	    TPM_EVENTLOG_TCG12));
	ATF_CHECK(tpm_eventlog_valid(log, sizeof(log), TPM_EVENTLOG_TCG12));
	ATF_CHECK(!tpm_eventlog_valid(log, 0, TPM_EVENTLOG_TCG12));
	ATF_CHECK(!tpm_eventlog_valid(log, sizeof(log) - 1, TPM_EVENTLOG_TCG12));
	ATF_CHECK(!tpm_eventlog_valid(log, sizeof(log), TPM_EVENTLOG_TCG2));

	le32enc(header.event_size, UINT32_MAX);
	memcpy(log, &header, sizeof(header));
	ATF_CHECK_EQ(tpm_eventlog_event1(log, sizeof(legacy_event)), 0);
	le32enc(header.event_size, 0);
	ATF_CHECK_EQ(tpm_eventlog_event1((const void *)&header, sizeof(header)),
	    sizeof(header));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, records);
	ATF_TP_ADD_TC(tp, malformed);
	ATF_TP_ADD_TC(tp, short_records);
	ATF_TP_ADD_TC(tp, final_events);
	ATF_TP_ADD_TC(tp, legacy_records);
	return (atf_no_error());
}
