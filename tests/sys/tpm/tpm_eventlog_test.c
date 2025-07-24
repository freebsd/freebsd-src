/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <sys/mman.h>
#include <dev/tpm/tpm_eventlog.h>
#include <atf-c.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* Two active banks: SHA-1 and SHA-256, followed by a three-byte EV_IPL. */
static size_t
specid(uint8_t *p)
{

	memset(p, 0, 69);
	le32enc(p + 4, 3);
	le32enc(p + 28, 37);
	memcpy(p + 32, "Spec ID Event03", 16);
	p[53] = 2;
	p[55] = 2;
	le32enc(p + 56, 2);
	le16enc(p + 60, 4);
	le16enc(p + 62, 20);
	le16enc(p + 64, 11);
	le16enc(p + 66, 32);
	return (69);
}

static size_t
event2(uint8_t *p)
{

	memset(p, 0, 75);
	le32enc(p, 8);
	le32enc(p + 4, 13);
	le32enc(p + 8, 2);
	le16enc(p + 12, 4);
	memset(p + 14, 0xa5, 20);
	le16enc(p + 34, 11);
	memset(p + 36, 0x5a, 32);
	le32enc(p + 68, 3);
	memcpy(p + 72, "abc", 3);
	return (75);
}

ATF_TC_WITHOUT_HEAD(records);
ATF_TC_BODY(records, tc)
{
	struct tpm_eventlog_algorithms algs;
	uint8_t buf[256], *p = buf + 1;
	size_t n, m;

	n = specid(p);
	m = event2(p + n);
	ATF_REQUIRE_EQ(tpm_eventlog_specid(p, n, &algs), n);
	ATF_CHECK_EQ(tpm_eventlog_event2(p + n, m, &algs), m);
	ATF_CHECK(tpm_eventlog_valid(p, n, TPM_EVENTLOG_TCG2));
	ATF_CHECK(tpm_eventlog_valid(p, n + m, TPM_EVENTLOG_TCG2));
	ATF_CHECK(!tpm_eventlog_valid(p, n + m, 99));
	ATF_CHECK(!tpm_eventlog_valid(p, n + m + 1, TPM_EVENTLOG_TCG2));

	/* Algorithms may appear in a different order in each event. */
	le16enc(p + n + 12, 11);
	memset(p + n + 14, 0x5a, 32);
	le16enc(p + n + 46, 4);
	memset(p + n + 48, 0xa5, 20);
	ATF_CHECK(tpm_eventlog_valid(p, n + m, TPM_EVENTLOG_TCG2));
}

ATF_TC_WITHOUT_HEAD(malformed);
ATF_TC_BODY(malformed, tc)
{
	struct tpm_eventlog_algorithms algs;
	uint8_t buf[256], saved[256];
	size_t n, m;

	n = specid(buf);
	m = event2(buf + n);
	memcpy(saved, buf, n + m);
	ATF_REQUIRE_EQ(tpm_eventlog_specid(buf, n, &algs), n);

	le32enc(buf + n + 8, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_event2(buf + n, m, &algs), 0);
	memcpy(buf, saved, n + m);
	le16enc(buf + n + 34, 4); /* Duplicate algorithm. */
	ATF_CHECK_EQ(tpm_eventlog_event2(buf + n, m, &algs), 0);
	memcpy(buf, saved, n + m);
	le16enc(buf + n + 12, 0xffff);
	ATF_CHECK_EQ(tpm_eventlog_event2(buf + n, m, &algs), 0);
	memcpy(buf, saved, n + m);
	le32enc(buf + n + 68, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_event2(buf + n, m, &algs), 0);

	memcpy(buf, saved, n + m);
	le32enc(buf + 56, UINT32_MAX);
	ATF_CHECK_EQ(tpm_eventlog_specid(buf, n, &algs), 0);
	memcpy(buf, saved, n + m);
	le16enc(buf + 64, 4);
	ATF_CHECK_EQ(tpm_eventlog_specid(buf, n, &algs), 0);
	memcpy(buf, saved, n + m);
	le16enc(buf + 62, 0);
	ATF_CHECK_EQ(tpm_eventlog_specid(buf, n, &algs), 0);
	memcpy(buf, saved, n + m);
	buf[68] = 1; /* Vendor bytes run beyond the first event. */
	ATF_CHECK_EQ(tpm_eventlog_specid(buf, n, &algs), 0);
	memcpy(buf, saved, n + m);
	buf[8] = 1; /* Spec ID's SHA-1 digest must be zero. */
	ATF_CHECK_EQ(tpm_eventlog_specid(buf, n, &algs), 0);
}

ATF_TC_WITHOUT_HEAD(short_records);
ATF_TC_BODY(short_records, tc)
{
	struct tpm_eventlog_algorithms algs;
	uint8_t buf[256], *map, *end;
	size_t n, m, i, pagesize;

	n = specid(buf);
	m = event2(buf + n);
	ATF_REQUIRE_EQ(tpm_eventlog_specid(buf, n, &algs), n);
	pagesize = getpagesize();
	map = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0);
	ATF_REQUIRE(map != MAP_FAILED);
	end = map + pagesize;
	ATF_REQUIRE_EQ(mprotect(end, pagesize, PROT_NONE), 0);
	for (i = 0; i < n; i++) {
		memcpy(end - i, buf, i);
		ATF_CHECK_EQ(tpm_eventlog_specid(end - i, i, &algs), 0);
	}
	ATF_REQUIRE_EQ(tpm_eventlog_specid(buf, n, &algs), n);
	for (i = 0; i < m; i++) {
		memcpy(end - i, buf + n, i);
		ATF_CHECK_EQ(tpm_eventlog_event2(end - i, i, &algs), 0);
	}
	ATF_REQUIRE_EQ(munmap(map, pagesize * 2), 0);
}

ATF_TC_WITHOUT_HEAD(final_events);
ATF_TC_BODY(final_events, tc)
{
	struct tpm_eventlog_algorithms algs;
	uint8_t header[69], table[256];
	size_t start, end, i, len;

	ATF_REQUIRE_EQ(tpm_eventlog_specid(header, specid(header), &algs),
	    sizeof(header));
	memset(table, 0, sizeof(table));
	le64enc(table, 1);
	le64enc(table + 8, 2);
	len = 16 + event2(table + 16);
	len += event2(table + len);
	ATF_REQUIRE(tpm_eventlog_final(table, len, &algs, 1, &start, &end));
	ATF_CHECK_EQ(start, 91);
	ATF_CHECK_EQ(end, 166);
	ATF_REQUIRE(tpm_eventlog_final(table, len, &algs, 2, &start, &end));
	ATF_CHECK_EQ(start, end);
	ATF_REQUIRE(tpm_eventlog_final(table, len, &algs, 0, &start, &end));
	ATF_CHECK_EQ(start, 16);
	ATF_CHECK(!tpm_eventlog_final(table, len, &algs, 3, &start, &end));
	for (i = 0; i < len; i++)
		ATF_CHECK(!tpm_eventlog_final(table, i, &algs, 0,
		    &start, &end));
	le64enc(table + 8, UINT64_MAX);
	ATF_CHECK(!tpm_eventlog_final(table, len, &algs, 0, &start, &end));
	le64enc(table + 8, 0);
	ATF_CHECK(tpm_eventlog_final(table, 16, &algs, 0, &start, &end));
	ATF_CHECK_EQ(start, end);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, records);
	ATF_TP_ADD_TC(tp, malformed);
	ATF_TP_ADD_TC(tp, short_records);
	ATF_TP_ADD_TC(tp, final_events);
	return (atf_no_error());
}
