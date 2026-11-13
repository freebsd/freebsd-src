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

static uint8_t primary[TPM_EVENTLOG_MAX_SIZE + EFI_PAGE_SIZE]
    __aligned(EFI_PAGE_SIZE);
static uint8_t final[EFI_PAGE_SIZE] __aligned(EFI_PAGE_SIZE);
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
	return (omit_final ? NULL : final);
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

/* A Spec ID with one SHA-256 bank and an event with no payload. */
static void
event2(uint8_t *p)
{

	memset(p, 0, 50);
	le32enc(p, 5);
	le32enc(p + 4, 5);
	le32enc(p + 8, 1);
	le16enc(p + 12, 11);
	memset(p + 14, 0xa5, 32);
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
	memset(primary, 0, sizeof(primary));
	le32enc(primary + 4, 3);
	le32enc(primary + 28, 33);
	memcpy(primary + 32, "Spec ID Event03", 16);
	primary[53] = primary[55] = 2;
	le32enc(primary + 56, 1);
	le16enc(primary + 60, 11);
	le16enc(primary + 62, 32);
	event2(primary + 65);
	first = (uintptr_t)primary;
	last = first + 65;
	memset(final, 0, sizeof(final));
	le64enc(final, 1);
	le64enc(final + 8, 1);
	event2(final + 16);
	map[0] = (EFI_MEMORY_DESCRIPTOR) {
		.Type = EfiACPIMemoryNVS,
		.PhysicalStart = (uintptr_t)primary,
		.NumberOfPages = sizeof(primary) / EFI_PAGE_SIZE,
	};
	map[1] = (EFI_MEMORY_DESCRIPTOR) {
		.Type = EfiACPIMemoryNVS,
		.PhysicalStart = (uintptr_t)final,
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
	ATF_CHECK_EQ(fp->f_metadata->md_size, sizeof(hdr) + size);
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

	setup();
	truncated = true;
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	/* md_copymodules must see the full reservation before exit. */
	ATF_CHECK_EQ(fp.f_metadata->md_size,
	    sizeof(struct tpm_eventlog_header) + TPM_EVENTLOG_MAX_SIZE);
	le64enc(final + 8, 2);
	event2(final + 66);
	final[66 + 14] = 0x5a;
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, 165, TPM_EVENTLOG_TCG2, TPM_EVENTLOG_TRUNCATED);
	data = fp.f_metadata->md_data + sizeof(struct tpm_eventlog_header);
	ATF_CHECK_EQ(memcmp(data, primary, 115), 0);
	ATF_CHECK_EQ(memcmp(data + 115, final + 66, 50), 0);
	discard(&fp);

	setup();
	omit_final = true;
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	after_exit = true;
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, 115, TPM_EVENTLOG_TCG2, 0);
	discard(&fp);

	setup();
	last = first;
	le64enc(final + 8, 0);
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
	check_log(&fp, 65, TPM_EVENTLOG_TCG2, 0);
	discard(&fp);
}

ATF_TC_WITHOUT_HEAD(absent_log);
ATF_TC_BODY(absent_log, tc)
{
	struct preloaded_file fp = { 0 };
	unsigned int i;

	for (i = 0; i < 3; i++) {
		setup();
		if (i == 0)
			absent = true;
		else if (i == 1)
			first = 0;
		else
			last = 0;
		ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
		ATF_CHECK(fp.f_metadata == NULL);
		ATF_CHECK_EQ(messages, 0);
		ATF_CHECK_EQ(outstanding, 0);
	}
}

ATF_TC_WITHOUT_HEAD(invalid_log);
ATF_TC_BODY(invalid_log, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	unsigned int i;

	for (i = 0; i < 5; i++) {
		setup();
		switch (i) {
		case 0:
			last = first + 1;
			break;
		case 1:
			last++;
			break;
		case 2:
			map[0].Attribute = EFI_MEMORY_RP;
			break;
		case 3:
			map[0].Type = EfiReservedMemoryType;
			break;
		case 4:
			log_status = EFI_DEVICE_ERROR;
			break;
		}
		ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
		ATF_CHECK(fp.f_metadata == NULL);
		ATF_CHECK_EQ(messages, 1);
		ATF_CHECK_EQ(outstanding, 0);
	}
	setup();
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
	le64enc(final + 8, 0); /* The observed prefix must not disappear. */
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
	unsigned int i;

	for (i = 0; i < 3; i++) {
		setup();
		le32enc(primary + 65 + 46, TPM_EVENTLOG_MAX_SIZE - 115 +
		    (i == 2));
		log = efi_tpm_log_prepare(&fp);
		if (i == 2) {
			ATF_CHECK(log == NULL);
			ATF_CHECK(fp.f_metadata == NULL);
			ATF_CHECK_EQ(outstanding, 0);
			continue;
		}
		ATF_REQUIRE(log != NULL);
		if (i == 1) {
			le64enc(final + 8, 2);
			event2(final + 66);
		}
		after_exit = true;
		efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
		if (i == 0)
			check_log(&fp, TPM_EVENTLOG_MAX_SIZE,
			    TPM_EVENTLOG_TCG2, 0);
		else
			ATF_CHECK(fp.f_metadata == NULL);
		discard(&fp);
	}
}

ATF_TC_WITHOUT_HEAD(cleanup);
ATF_TC_BODY(cleanup, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	unsigned int i;

	/* Fail the map, context and reserved metadata allocations in turn. */
	for (i = 1; i <= 3; i++) {
		setup();
		fail_at = i;
		ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
		ATF_CHECK(fp.f_metadata == NULL);
		ATF_CHECK_EQ(outstanding, 0);
	}
	setup();
	log = efi_tpm_log_prepare(&fp);
	ATF_REQUIRE(log != NULL);
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
}

/* Two legacy SHA-1 events with three-byte payloads. */
static void
setup_legacy(void)
{

	setup();
	legacy = true;
	memset(primary, 0, sizeof(primary));
	le32enc(primary, 5);
	le32enc(primary + 4, 5);
	le32enc(primary + 28, 3);
	memcpy(primary + 32, "abc", 3);
	memcpy(primary + 35, primary, 35);
	last = first + 35;
	/* An unrelated final table must not be parsed for a legacy log. */
	le64enc(final + 8, UINT64_MAX);
}

ATF_TC_WITHOUT_HEAD(legacy_log);
ATF_TC_BODY(legacy_log, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	unsigned int i;
	size_t size;

	for (i = 1; i <= 2; i++) {
		setup_legacy();
		size = 35 * i;
		last = first + size - 35;
		log = efi_tpm_log_prepare(&fp);
		ATF_REQUIRE(log != NULL);
		/* The legacy snapshot deliberately excludes later events. */
		memcpy(primary + size, primary, 35);
		after_exit = true;
		efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
		check_log(&fp, size, TPM_EVENTLOG_TCG12, 0);
		ATF_CHECK_EQ(memcmp(fp.f_metadata->md_data +
		    sizeof(struct tpm_eventlog_header), primary, size), 0);
		ATF_CHECK_EQ(table_calls, 0);
		discard(&fp);
	}
}

ATF_TC_WITHOUT_HEAD(legacy_absent);
ATF_TC_BODY(legacy_absent, tc)
{
	struct preloaded_file fp = { 0 };
	unsigned int i;

	for (i = 0; i < 4; i++) {
		setup_legacy();
		switch (i) {
		case 0:
			present = false;
			break;
		case 1:
			deactivated = true;
			break;
		case 2:
			last = 0;
			memset(primary, 0, sizeof(primary));
			break;
		case 3:
			first = 0;
			break;
		}
		ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
		ATF_CHECK(fp.f_metadata == NULL);
		ATF_CHECK_EQ(messages, 0);
		ATF_CHECK_EQ(outstanding, 0);
	}
}

ATF_TC_WITHOUT_HEAD(tcg2_sha1);
ATF_TC_BODY(tcg2_sha1, tc)
{
	struct preloaded_file fp = { 0 };
	struct efi_tpm_eventlog *log;
	unsigned int i;

	for (i = 0; i < 2; i++) {
		setup_legacy();
		legacy = false;
		sha1_only = i == 0;
		empty_tcg2 = i == 1;
		truncated = i == 1;
		log = efi_tpm_log_prepare(&fp);
		ATF_REQUIRE(log != NULL);
		ATF_CHECK_EQ(log_calls, 2);
		after_exit = true;
		efi_tpm_log_finish(log, map, sizeof(map), sizeof(map[0]));
		check_log(&fp, 70, TPM_EVENTLOG_TCG12,
		    truncated ? TPM_EVENTLOG_TRUNCATED : 0);
		ATF_CHECK_EQ(table_calls, 0);
		discard(&fp);
	}
	setup();
	empty_tcg2 = true;
	log_status = EFI_UNSUPPORTED;
	/* An absent TPM stays silent even if SHA-1 format is unsupported. */
	ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(messages, 0);
	ATF_CHECK_EQ(outstanding, 0);

	setup_legacy();
	legacy = false;
	sha1_only = true;
	last = 0;
	ATF_CHECK(efi_tpm_log_prepare(&fp) == NULL);
	ATF_CHECK(fp.f_metadata == NULL);
	ATF_CHECK_EQ(messages, 0);
	ATF_CHECK_EQ(outstanding, 0);
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
