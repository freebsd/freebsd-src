/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

/* Exercise the actual loader source with firmware and allocation mocks. */
#define TPM_TEST_IMPLEMENTATION
#include <stand.h>
#include <bootstrap.h>
#include <sys/linker.h>
#include <efi.h>
#include <efilib.h>
#include <Protocol/Tcg2Protocol.h>
#include <Protocol/TcgService.h>
#include <dev/tpm/tpm_eventlog.h>
#include <atf-c.h>

#include "eventlog_fixtures.h"

struct firmware_spec_id {
	struct spec_id_header header;
	struct spec_id_algorithm bank;
	uint8_t vendor_info_size;
};

struct action_event {
	struct event2_header header;
	struct sha256_digest sha256;
	uint8_t event_size[4];
};

static const struct action_event action = {
	.header.pcr_index = { 5 },
	.header.event_type = { EV_ACTION },
	.header.digest_count = { 1 },
	.sha256 = { { ALG_SHA256 }, { 0xa5 } },
};

static union {
	uint8_t bytes[TPM_EVENTLOG_MAX_SIZE + EFI_PAGE_SIZE];
	struct {
		struct firmware_spec_id spec;
		struct action_event event;
	} tcg2;
	struct legacy_record legacy[3];
} primary __aligned(EFI_PAGE_SIZE);

static union {
	uint8_t bytes[EFI_PAGE_SIZE];
	struct {
		struct final_table_header header;
		struct action_event events[2];
	} table;
} final __aligned(EFI_PAGE_SIZE);
static EFI_MEMORY_DESCRIPTOR map[2];
static EFI_PHYSICAL_ADDRESS first, last;
static EFI_STATUS log_status;
static bool absent, after_exit, truncated, omit_final;
static bool legacy, present, deactivated, sha1_only, empty_tcg2;
static unsigned int log_calls, table_calls;
static unsigned int allocations, fail_at, outstanding, messages;

void *
test_malloc(size_t size)
{
	void *p;

	ATF_REQUIRE(!after_exit);
	if (++allocations == fail_at)
		return (NULL);
	p = malloc(size);
	if (p != NULL)
		outstanding++;
	return (p);
}

void *
test_calloc(size_t count, size_t size)
{
	void *p;

	p = test_malloc(count * size);
	if (p != NULL)
		memset(p, 0, count * size);
	return (p);
}

void
test_free(void *p)
{

	if (p != NULL) {
		ATF_REQUIRE(outstanding > 0);
		outstanding--;
		free(p);
	}
}

int
test_printf(const char *fmt __unused, ...)
{

	ATF_REQUIRE(!after_exit);
	messages++;
	return (0);
}

void *
efi_get_table(EFI_GUID *guid)
{
	EFI_GUID expected = EFI_TCG2_FINAL_EVENTS_TABLE_GUID;

	ATF_REQUIRE_EQ(memcmp(guid, &expected, sizeof(expected)), 0);
	table_calls++;
	return (omit_final ? NULL : &final);
}

static EFI_STATUS EFIAPI
get_log(EFI_TCG2_PROTOCOL *protocol __unused, EFI_TCG2_EVENT_LOG_FORMAT format,
    EFI_PHYSICAL_ADDRESS *start, EFI_PHYSICAL_ADDRESS *end, BOOLEAN *shortlog)
{

	ATF_REQUIRE(!after_exit);
	ATF_REQUIRE_EQ(*start, 0);
	ATF_REQUIRE_EQ(*end, 0);
	ATF_REQUIRE_EQ(*shortlog, false);
	log_calls++;
	if (format == EFI_TCG2_EVENT_LOG_FORMAT_TCG_2) {
		ATF_REQUIRE_EQ(log_calls, 1);
		if (empty_tcg2)
			return (EFI_SUCCESS);
		if (sha1_only) {
			/* Ignore any output arguments from a failed call. */
			*start = *end = UINT64_MAX;
			*shortlog = true;
			return (EFI_UNSUPPORTED);
		}
	} else {
		ATF_REQUIRE_EQ(format, EFI_TCG2_EVENT_LOG_FORMAT_TCG_1_2);
		ATF_REQUIRE_EQ(log_calls, 2);
	}
	*start = first;
	*end = last;
	*shortlog = truncated;
	return (log_status);
}

static EFI_STATUS EFIAPI
status_check(EFI_TCG_PROTOCOL *protocol __unused,
    TCG_EFI_BOOT_SERVICE_CAPABILITY *cap, UINT32 *features,
    EFI_PHYSICAL_ADDRESS *start, EFI_PHYSICAL_ADDRESS *end)
{

	ATF_REQUIRE(!after_exit);
	ATF_REQUIRE_EQ(cap->Size, sizeof(*cap));
	ATF_REQUIRE_EQ(*features, 0);
	cap->TPMPresentFlag = present;
	cap->TPMDeactivatedFlag = deactivated;
	*start = first;
	*end = last;
	return (log_status);
}

static EFI_TCG2_PROTOCOL tcg2 = { .GetEventLog = get_log };
static EFI_TCG_PROTOCOL tcg = { .StatusCheck = status_check };

static EFI_STATUS EFIAPI
locate(EFI_GUID *guid, void *registration __unused, void **protocol)
{
	EFI_GUID expected2 = EFI_TCG2_PROTOCOL_GUID;
	EFI_GUID expected1 = EFI_TCG_PROTOCOL_GUID;

	ATF_REQUIRE(!after_exit);
	if (memcmp(guid, &expected2, sizeof(expected2)) == 0) {
		log_calls = 0;
		*protocol = &tcg2;
		return (absent || legacy ? EFI_NOT_FOUND : EFI_SUCCESS);
	}
	ATF_REQUIRE_EQ(memcmp(guid, &expected1, sizeof(expected1)), 0);
	*protocol = &tcg;
	return (absent ? EFI_NOT_FOUND : EFI_SUCCESS);
}

static EFI_STATUS EFIAPI
get_map(UINTN *size, EFI_MEMORY_DESCRIPTOR *buf, UINTN *key,
    UINTN *descsize, UINT32 *version)
{

	ATF_REQUIRE(!after_exit);
	*key = 1;
	*descsize = sizeof(map[0]);
	*version = EFI_MEMORY_DESCRIPTOR_VERSION;
	if (*size < sizeof(map)) {
		*size = sizeof(map);
		return (EFI_BUFFER_TOO_SMALL);
	}
	memcpy(buf, map, sizeof(map));
	*size = sizeof(map);
	return (EFI_SUCCESS);
}

static EFI_BOOT_SERVICES boot_services = {
	.LocateProtocol = locate,
	.GetMemoryMap = get_map,
};
EFI_BOOT_SERVICES *BS = &boot_services;

/* Firmware appends one distinguishable event after the snapshot. */
static void
append_final_event(void)
{

	final.table.events[1] = action;
	final.table.events[1].sha256.bytes[0] = 0x5a;
	le64enc(final.table.header.event_count, 2);
}

static void
setup(void)
{

	ATF_REQUIRE_EQ(outstanding, 0);
	absent = after_exit = truncated = omit_final = false;
	allocations = fail_at = messages = 0;
	log_status = EFI_SUCCESS;
	legacy = deactivated = sha1_only = empty_tcg2 = false;
	present = true;
	log_calls = table_calls = 0;
	memset(&primary, 0, sizeof(primary));
	primary.tcg2.spec.header = spec_id_header(sizeof(primary.tcg2.spec), 1);
	le16enc(primary.tcg2.spec.bank.id, ALG_SHA256);
	le16enc(primary.tcg2.spec.bank.size, sizeof(action.sha256.bytes));
	primary.tcg2.event = action;
	first = (uintptr_t)&primary;
	last = (uintptr_t)&primary.tcg2.event;

	memset(&final, 0, sizeof(final));
	le64enc(final.table.header.version, EFI_TCG2_FINAL_EVENTS_TABLE_VERSION);
	le64enc(final.table.header.event_count, 1);
	final.table.events[0] = action;
	map[0] = (EFI_MEMORY_DESCRIPTOR) {
		.Type = EfiACPIMemoryNVS,
		.PhysicalStart = (uintptr_t)&primary,
		.NumberOfPages = sizeof(primary) / EFI_PAGE_SIZE,
	};
	map[1] = (EFI_MEMORY_DESCRIPTOR) {
		.Type = EfiACPIMemoryNVS,
		.PhysicalStart = (uintptr_t)&final,
		.NumberOfPages = sizeof(final) / EFI_PAGE_SIZE,
	};
}

static void
discard(struct preloaded_file *fp)
{

	if (fp->f_metadata != NULL) {
		test_free(fp->f_metadata);
		fp->f_metadata = NULL;
	}
	ATF_CHECK_EQ(outstanding, 0);
}

static void
check_log(struct preloaded_file *fp, size_t size, uint32_t format,
    uint32_t flags)
{
	struct tpm_eventlog_header hdr;

	ATF_REQUIRE(fp->f_metadata != NULL);
	ATF_CHECK_EQ(fp->f_metadata->md_type, MODINFOMD_TPM_EVENTLOG);
	ATF_REQUIRE_EQ(fp->f_metadata->md_size, sizeof(hdr) + size);
	memcpy(&hdr, fp->f_metadata->md_data, sizeof(hdr));
	ATF_CHECK_EQ(hdr.version, TPM_EVENTLOG_VERSION);
	ATF_CHECK_EQ(hdr.length, size);
	ATF_CHECK_EQ(hdr.format, format);
	ATF_CHECK_EQ(hdr.flags, flags);
	ATF_CHECK(tpm_eventlog_valid((void *)(fp->f_metadata->md_data +
	    sizeof(hdr)), size, format));
}

ATF_TC_WITHOUT_HEAD(handoff);
ATF_TC_BODY(handoff, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	const char *data;
	uint8_t saved[sizeof(primary.tcg2)];

	setup();
	memcpy(saved, primary.bytes, sizeof(saved));
	truncated = true;
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	/* md_copymodules must see the full reservation before exit. */
	ATF_REQUIRE(fp.f_metadata != NULL);
	ATF_CHECK_EQ(fp.f_metadata->md_size,
	    sizeof(struct tpm_eventlog_header) + TPM_EVENTLOG_MAX_SIZE);
	append_final_event();
	after_exit = true;
	/* Firmware memory may change after the snapshot. */
	memset(primary.bytes, 0xa5, sizeof(saved));
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, sizeof(saved) + sizeof(action), TPM_EVENTLOG_TCG2,
	    TPM_EVENTLOG_TRUNCATED);
	data = fp.f_metadata->md_data + sizeof(struct tpm_eventlog_header);
	ATF_CHECK_EQ(memcmp(data, saved, sizeof(saved)), 0);
	ATF_CHECK_EQ(memcmp(data + sizeof(saved), &final.table.events[1],
	    sizeof(action)), 0);
	discard(&fp);

	setup();
	omit_final = true;
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, sizeof(primary.tcg2), TPM_EVENTLOG_TCG2, 0);
	discard(&fp);

	setup();
	last = first;
	le64enc(final.table.header.event_count, 0);
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, sizeof(primary.tcg2.spec), TPM_EVENTLOG_TCG2, 0);
	discard(&fp);
}

static void
check_no_log(struct preloaded_file *fp, unsigned int expected_messages)
{

	ATF_CHECK(efi_tpm_log_prepare(fp) == NULL);
	ATF_CHECK(fp->f_metadata == NULL);
	ATF_CHECK_EQ(messages, expected_messages);
	ATF_CHECK_EQ(outstanding, 0);
}

ATF_TC_WITHOUT_HEAD(absent_log);
ATF_TC_BODY(absent_log, tc)
{
	struct preloaded_file fp = { 0 };

	setup();
	absent = true;
	check_no_log(&fp, 0);

	setup();
	first = 0;
	check_no_log(&fp, 0);

	setup();
	last = 0;
	check_no_log(&fp, 0);
}

ATF_TC_WITHOUT_HEAD(invalid_log);
ATF_TC_BODY(invalid_log, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;

	setup();
	last = first + 1;	/* Inside the Spec ID record. */
	check_no_log(&fp, 1);

	setup();
	last++;			/* Inside the last event. */
	check_no_log(&fp, 1);

	setup();
	map[0].Attribute = EFI_MEMORY_RP;
	check_no_log(&fp, 1);

	setup();
	map[0].Type = EfiReservedMemoryType;
	check_no_log(&fp, 1);

	setup();
	log_status = EFI_DEVICE_ERROR;
	check_no_log(&fp, 1);

	setup();
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	/* Firmware lost the final-table prefix observed during prepare. */
	le64enc(final.table.header.event_count, 0);
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(outstanding, 0);
}

ATF_TC_WITHOUT_HEAD(size_limit);
ATF_TC_BODY(size_limit, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	const size_t max_payload = TPM_EVENTLOG_MAX_SIZE - sizeof(primary.tcg2);

	/* The primary log alone exactly fills the reservation. */
	setup();
	le32enc(primary.tcg2.event.event_size, max_payload);
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, TPM_EVENTLOG_MAX_SIZE, TPM_EVENTLOG_TCG2, 0);
	discard(&fp);

	/* Adding a final event takes that same log over the limit. */
	setup();
	le32enc(primary.tcg2.event.event_size, max_payload);
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	append_final_event();
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	ATF_CHECK(fp.f_metadata == NULL);
	discard(&fp);

	/* A primary log one byte over the limit fails during prepare. */
	setup();
	le32enc(primary.tcg2.event.event_size, max_payload + 1);
	ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(outstanding, 0);
}

ATF_TC_WITHOUT_HEAD(cleanup);
ATF_TC_BODY(cleanup, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	unsigned int i, prepare_allocations;

	setup();
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	prepare_allocations = allocations;
	efi_tpm_log_free(log); /* Failed boot or staging reservation. */
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(outstanding, 0);

	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	absent = true; /* A repeated boot must remove the old snapshot. */
	ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(outstanding, 0);

	for (i = 1; i <= prepare_allocations; i++) {
		setup();
		fail_at = i;
		ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
		ATF_CHECK(fp.f_metadata == NULL);
		ATF_CHECK_EQ(outstanding, 0);
	}
}

/* Two legacy SHA-1 events with three-byte payloads. */
static void
setup_legacy(void)
{

	setup();
	legacy = true;
	memset(primary.bytes, 0, sizeof(primary));
	primary.legacy[0] = primary.legacy[1] = legacy_event;
	last = (uintptr_t)&primary.legacy[1];
	/* An unrelated final table must not be parsed for a legacy log. */
	le64enc(final.table.header.event_count, UINT64_MAX);
}

ATF_TC_WITHOUT_HEAD(legacy_log);
ATF_TC_BODY(legacy_log, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	uint8_t saved[2 * sizeof(legacy_event)];
	unsigned int i;
	size_t size;

	for (i = 1; i <= 2; i++) {
		setup_legacy();
		size = sizeof(legacy_event) * i;
		memcpy(saved, primary.bytes, size);
		last = (uintptr_t)&primary.legacy[i - 1];
		log = efi_tpm_log_prepare(&fp);
		ATF_REQUIRE(log != NULL);
		/* The legacy snapshot deliberately excludes later events. */
		primary.legacy[i] = legacy_event;
		after_exit = true;
		memset(primary.bytes, 0xa5, size);
		efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
		check_log(&fp, size, TPM_EVENTLOG_TCG12, 0);
		ATF_CHECK_EQ(memcmp(fp.f_metadata->md_data +
		    sizeof(struct tpm_eventlog_header), saved, size), 0);
		ATF_CHECK_EQ(table_calls, 0);
		discard(&fp);
	}
}

ATF_TC_WITHOUT_HEAD(legacy_absent);
ATF_TC_BODY(legacy_absent, tc)
{
	struct preloaded_file fp = { 0 };

	setup_legacy();
	present = false;
	check_no_log(&fp, 0);

	setup_legacy();
	deactivated = true;
	check_no_log(&fp, 0);

	setup_legacy();
	last = 0;
	memset(primary.bytes, 0, sizeof(primary));
	check_no_log(&fp, 0);

	setup_legacy();
	first = 0;
	check_no_log(&fp, 0);
}

static void
check_sha1_fallback(struct preloaded_file *fp)
{
	struct efi_tpm_eventlog *log;

	log = efi_tpm_log_prepare(fp);
	ATF_REQUIRE(log != NULL);
	ATF_CHECK_EQ(log_calls, 2);
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(fp, 2 * sizeof(legacy_event), TPM_EVENTLOG_TCG12,
	    truncated ? TPM_EVENTLOG_TRUNCATED : 0);
	ATF_CHECK_EQ(table_calls, 0);
	discard(fp);
}

ATF_TC_WITHOUT_HEAD(tcg2_sha1);
ATF_TC_BODY(tcg2_sha1, tc)
{
	struct preloaded_file fp = { 0 };

	setup_legacy();
	legacy = false;
	sha1_only = true;
	check_sha1_fallback(&fp);

	setup_legacy();
	legacy = false;
	empty_tcg2 = true;
	truncated = true;
	check_sha1_fallback(&fp);

	setup();
	empty_tcg2 = true;
	log_status = EFI_UNSUPPORTED;
	/* An absent TPM stays silent even if SHA-1 format is unsupported. */
	check_no_log(&fp, 0);

	setup_legacy();
	legacy = false;
	sha1_only = true;
	last = 0;
	check_no_log(&fp, 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, handoff);
	ATF_TP_ADD_TC(tp, absent_log);
	ATF_TP_ADD_TC(tp, invalid_log);
	ATF_TP_ADD_TC(tp, size_limit);
	ATF_TP_ADD_TC(tp, cleanup);
	ATF_TP_ADD_TC(tp, legacy_log);
	ATF_TP_ADD_TC(tp, legacy_absent);
	ATF_TP_ADD_TC(tp, tcg2_sha1);
	return (atf_no_error());
}
