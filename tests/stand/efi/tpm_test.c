/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <atf-c.h>
#include <sha.h>
#include <sha256.h>

#include "tpm.c"

#undef malloc
#undef calloc

struct recorded_event {
	UINT32 pcr;
	bool failure;
	unsigned char digest[SHA256_DIGEST_LENGTH];
};

static struct preloaded_file file;
static char filename[] = "/boot/kernel/kernel";
static char filetype[] = "elf kernel";
static struct env_var variable = { "testconfig", "value", 0, NULL };
struct env_var *test_environment = &variable;
EFI_BOOT_SERVICES *BS;

static EFI_TCG2_PROTOCOL protocol2;
static EFI_TCG_PROTOCOL protocol1;
static unsigned char contents[256];
static unsigned char pcrs[2][SHA256_DIGEST_LENGTH];
static struct recorded_event events[64];
static size_t nevents, allocations, fail_allocation;
static size_t calls, fail_call;
static unsigned int cap_attempts[2], cap_fail_pcr;
static bool use_tpm1, full_log, persistent_failure, device_failed;

void *
test_malloc(size_t len)
{

	allocations++;
	return (fail_allocation != 0 && allocations >= fail_allocation ?
	    NULL : malloc(len));
}

void *
test_calloc(size_t n, size_t len)
{

	allocations++;
	return (fail_allocation != 0 && allocations >= fail_allocation ?
	    NULL : calloc(n, len));
}

struct preloaded_file *
file_findfile(const char *name __unused, const char *type __unused)
{

	return (&file);
}

struct file_metadata *
file_findmetadata(struct preloaded_file *fp, int type)
{
	struct file_metadata *md;

	for (md = fp->f_metadata; md != NULL; md = md->md_next) {
		if (md->md_type == type)
			return (md);
	}
	return (NULL);
}

void *
efi_translate(vm_offset_t addr)
{

	ATF_REQUIRE(addr >= file.f_addr);
	ATF_REQUIRE(addr - file.f_addr < sizeof(contents));
	return (contents + (addr - file.f_addr));
}

static void
extend(UINT32 pcr, const unsigned char *digest)
{
	SHA256_CTX ctx;
	SHA1_CTX ctx1;

	ATF_REQUIRE(pcr == 8 || pcr == 9);
	if (use_tpm1) {
		SHA1_Init(&ctx1);
		SHA1_Update(&ctx1, pcrs[pcr - 8], SHA_DIGEST_LENGTH);
		SHA1_Update(&ctx1, digest, SHA_DIGEST_LENGTH);
		SHA1_Final(pcrs[pcr - 8], &ctx1);
		return;
	}
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, pcrs[pcr - 8], SHA256_DIGEST_LENGTH);
	SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH);
	SHA256_Final(pcrs[pcr - 8], &ctx);
}

static EFI_STATUS
record_event(UINT32 pcr, EFI_PHYSICAL_ADDRESS addr, UINT64 len,
    const void *event, size_t event_len)
{
	SHA256_CTX ctx;
	SHA1_CTX ctx1;
	struct recorded_event *record;
	bool failure;

	calls++;
	ATF_REQUIRE(pcr == 8 || pcr == 9);
	failure = len == sizeof(efi_tpm_failure_event) &&
	    memcmp((void *)(uintptr_t)addr, efi_tpm_failure_event, len) == 0;
	if (failure) {
		cap_attempts[pcr - 8]++;
		ATF_REQUIRE_EQ(event_len, sizeof(efi_tpm_failure_event));
		ATF_REQUIRE_EQ(memcmp(event, efi_tpm_failure_event,
		    event_len), 0);
		if (pcr == cap_fail_pcr)
			return (EFI_DEVICE_ERROR);
	}
	if (calls == fail_call || device_failed) {
		device_failed = persistent_failure;
		return (EFI_DEVICE_ERROR);
	}
	ATF_REQUIRE(nevents < nitems(events));
	record = &events[nevents++];
	record->pcr = pcr;
	record->failure = failure;
	if (use_tpm1) {
		SHA1_Init(&ctx1);
		SHA1_Update(&ctx1, (void *)(uintptr_t)addr, len);
		SHA1_Final(record->digest, &ctx1);
	} else {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, (void *)(uintptr_t)addr, len);
		SHA256_Final(record->digest, &ctx);
	}
	extend(record->pcr, record->digest);
	return (full_log ? EFI_VOLUME_FULL : EFI_SUCCESS);
}

static EFI_STATUS EFIAPI
measure2(EFI_TCG2_PROTOCOL *this __unused, UINT64 flags,
    EFI_PHYSICAL_ADDRESS addr, UINT64 len, EFI_TCG2_EVENT *event)
{

	ATF_REQUIRE_EQ(flags, 0);
	ATF_REQUIRE_EQ(event->Header.HeaderSize, sizeof(event->Header));
	ATF_REQUIRE_EQ(event->Header.HeaderVersion,
	    EFI_TCG2_EVENT_HEADER_VERSION);
	ATF_REQUIRE_EQ(event->Header.EventType, EV_IPL);
	ATF_REQUIRE(event->Size >= offsetof(EFI_TCG2_EVENT, Event));
	return (record_event(event->Header.PCRIndex, addr, len, event->Event,
	    event->Size - offsetof(EFI_TCG2_EVENT, Event)));
}

static EFI_STATUS EFIAPI
measure1(EFI_TCG_PROTOCOL *this __unused, EFI_PHYSICAL_ADDRESS addr,
    UINT64 len, TCG_ALGORITHM_ID algorithm, TCG_PCR_EVENT *event,
    UINT32 *event_number, EFI_PHYSICAL_ADDRESS *last)
{

	ATF_REQUIRE_EQ(algorithm, TPM_ALG_SHA);
	ATF_REQUIRE_EQ(event->EventType, EV_IPL);
	*event_number = calls;
	*last = 0;
	return (record_event(event->PCRIndex, addr, len, event->Event,
	    event->EventSize));
}

static void
reset(void)
{
	struct file_metadata *md, *next;

	for (md = file.f_metadata; md != NULL; md = next) {
		next = md->md_next;
		free(md);
	}
	bzero(&file, sizeof(file));
	bzero(pcrs, sizeof(pcrs));
	bzero(events, sizeof(events));
	bzero(contents, sizeof(contents));
	nevents = allocations = fail_allocation = 0;
	calls = fail_call = cap_fail_pcr = 0;
	bzero(cap_attempts, sizeof(cap_attempts));
	use_tpm1 = full_log = persistent_failure = device_failed = false;
	tpm_log_full = tpm_uncapped = false;
	file.f_name = filename;
	file.f_type = filetype;
	file.f_addr = 0x100000;
	protocol2.HashLogExtendEvent = measure2;
	protocol1.HashLogExtendEvent = measure1;
	tcg2 = &protocol2;
	tcg = &protocol1;
	tpm_protocol = EFI_TPM_2;
}

static void
add_metadata(int type, const void *data, size_t len)
{
	struct file_metadata *md;

	md = malloc(sizeof(*md) + len);
	ATF_REQUIRE(md != NULL);
	md->md_type = type;
	md->md_size = len;
	bcopy(data, md->md_data, len);
	md->md_next = file.f_metadata;
	file.f_metadata = md;
}

static void
elf_metadata(bool is64, bool relocatable, vm_offset_t base)
{
	Elf64_Ehdr eh64 = { .e_ident = { ELFMAG0, ELFMAG1, ELFMAG2,
	    ELFMAG3, ELFCLASS64 }, .e_entry = 0x200000, .e_shnum = 1 };
	Elf32_Ehdr eh32 = { .e_ident = { ELFMAG0, ELFMAG1, ELFMAG2,
	    ELFMAG3, ELFCLASS32 }, .e_entry = 0x200000, .e_shnum = 1 };
	Elf64_Shdr sh64 = { .sh_type = SHT_RELA, .sh_flags = SHF_ALLOC,
	    .sh_info = 2, .sh_link = 3, .sh_size = 64 };
	Elf32_Shdr sh32 = { .sh_type = SHT_REL, .sh_flags = SHF_ALLOC,
	    .sh_info = 2, .sh_link = 3, .sh_size = 64 };
	Elf64_Addr addr64;
	Elf32_Addr addr32;
	static const int types[] = { MODINFOMD_SSYM, MODINFOMD_ESYM,
	    MODINFOMD_DYNAMIC, MODINFOMD_CTORS_ADDR, MODINFOMD_CTORS_SIZE };
	size_t i;

	reset();
	file.f_addr = base;
	eh64.e_type = eh32.e_type = relocatable ? ET_REL : ET_EXEC;
	sh64.sh_addr = sh32.sh_addr = relocatable ? base : 0x200000;
	if (is64) {
		add_metadata(MODINFOMD_ELFHDR, &eh64, sizeof(eh64));
		add_metadata(MODINFOMD_SHDR, &sh64, sizeof(sh64));
	} else {
		add_metadata(MODINFOMD_ELFHDR, &eh32, sizeof(eh32));
		add_metadata(MODINFOMD_SHDR, &sh32, sizeof(sh32));
	}
	for (i = 0; i < nitems(types); i++) {
		addr64 = i < 2 ? base + 128 + i * 64 : 0x200010 + i * 16;
		addr32 = addr64;
		if (is64)
			add_metadata(types[i], &addr64, sizeof(addr64));
		else
			add_metadata(types[i], &addr32, sizeof(addr32));
	}
}

static void
metadata_digest(unsigned char *digest)
{

	nevents = 0;
	efi_tpm_measure_file_metadata(&file);
	ATF_REQUIRE_EQ(nevents, 1);
	ATF_REQUIRE_EQ(events[0].pcr, 9);
	bcopy(events[0].digest, digest, SHA256_DIGEST_LENGTH);
}

static void
check_metadata_changes(bool is64)
{
	struct file_metadata *md;
	unsigned char before[SHA256_DIGEST_LENGTH], after[SHA256_DIGEST_LENGTH];
	size_t i;

	elf_metadata(is64, false, 0x100000);
	metadata_digest(before);
	/* Every metadata byte is committed, including entry and ctor ranges. */
	for (md = file.f_metadata; md != NULL; md = md->md_next) {
		for (i = 0; i < md->md_size; i++) {
			if (md->md_type == MODINFOMD_ELFHDR && i == EI_CLASS)
				continue;
			md->md_data[i] ^= 1;
			metadata_digest(after);
			ATF_REQUIRE_MSG(memcmp(before, after,
			    sizeof(before)) != 0,
			    "metadata %#x byte %zu was not measured",
			    md->md_type, i);
			md->md_data[i] ^= 1;
		}
	}
	reset();
}

static void
check_placement(bool is64)
{
	unsigned char before[SHA256_DIGEST_LENGTH], after[SHA256_DIGEST_LENGTH];
	struct file_metadata *md, *next, *head;
	unsigned char original[sizeof(Elf64_Shdr)];

	elf_metadata(is64, true, 0x100000);
	md = file_findmetadata(&file, MODINFOMD_SHDR);
	bcopy(md->md_data, original, md->md_size);
	metadata_digest(before);
	ATF_REQUIRE_EQ(memcmp(md->md_data, original, md->md_size), 0);
	elf_metadata(is64, true, 0x300000);
	metadata_digest(after);
	ATF_REQUIRE_EQ(memcmp(before, after, sizeof(before)), 0);
	/* The loader's metadata list order must not affect the measurement. */
	head = NULL;
	for (md = file.f_metadata; md != NULL; md = next) {
		next = md->md_next;
		md->md_next = head;
		head = md;
	}
	file.f_metadata = head;
	metadata_digest(after);
	ATF_REQUIRE_EQ(memcmp(before, after, sizeof(before)), 0);
	reset();
}

ATF_TC_WITHOUT_HEAD(metadata64);
ATF_TC_BODY(metadata64, tc)
{
	check_metadata_changes(true);
}

ATF_TC_WITHOUT_HEAD(metadata32);
ATF_TC_BODY(metadata32, tc)
{
	check_metadata_changes(false);
}

ATF_TC_WITHOUT_HEAD(placement64);
ATF_TC_BODY(placement64, tc)
{
	check_placement(true);
}

ATF_TC_WITHOUT_HEAD(placement32);
ATF_TC_BODY(placement32, tc)
{
	check_placement(false);
}

static struct recorded_event golden_events[nitems(events)];
static unsigned char golden_pcrs[sizeof(pcrs)];
static size_t golden_count;

static void
boot_input(bool tpm1)
{

	elf_metadata(true, false, 0x100000);
	use_tpm1 = tpm1;
	tpm_protocol = tpm1 ? EFI_TPM_1 : EFI_TPM_2;
}

static void
register_contents(void)
{

	efi_tpm_measure_file(&file, file.f_addr, 64);
	efi_tpm_measure_file(&file, file.f_addr + 128, 32);
}

static void
golden_boot(bool tpm1)
{

	boot_input(tpm1);
	register_contents();
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
	ATF_REQUIRE_EQ(nevents, 5);
	ATF_REQUIRE_EQ(events[0].pcr, 8);
	ATF_REQUIRE_EQ(events[1].pcr, 8);
	ATF_REQUIRE_EQ(events[2].pcr, 9);
	ATF_REQUIRE_EQ(events[3].pcr, 9);
	ATF_REQUIRE_EQ(events[4].pcr, 9);
	golden_count = nevents;
	bcopy(events, golden_events, sizeof(events));
	bcopy(pcrs, golden_pcrs, sizeof(pcrs));
}

static void
check_capped(void)
{
	size_t prefix, i;

	ATF_REQUIRE(!tpm_uncapped);
	ATF_REQUIRE_EQ(tpm_protocol, EFI_TPM_NONE);
	ATF_REQUIRE_EQ(cap_attempts[0], 1);
	ATF_REQUIRE_EQ(cap_attempts[1], 1);
	ATF_REQUIRE(nevents >= 2);
	prefix = nevents - 2;
	ATF_REQUIRE_EQ(events[prefix].pcr, 8);
	ATF_REQUIRE_EQ(events[prefix + 1].pcr, 9);
	ATF_REQUIRE(events[prefix].failure);
	ATF_REQUIRE(events[prefix + 1].failure);
	for (i = 0; i < prefix; i++) {
		ATF_REQUIRE_EQ(events[i].pcr, golden_events[i].pcr);
		ATF_REQUIRE_EQ(memcmp(events[i].digest, golden_events[i].digest,
		    sizeof(events[i].digest)), 0);
	}
	/* Replaying the missing good suffix must not repair either PCR. */
	for (i = prefix; i < golden_count; i++)
		extend(golden_events[i].pcr, golden_events[i].digest);
	ATF_REQUIRE(memcmp(pcrs[0], golden_pcrs, sizeof(pcrs[0])) != 0);
	ATF_REQUIRE(memcmp(pcrs[1], golden_pcrs + sizeof(pcrs[0]),
	    sizeof(pcrs[1])) != 0);
}

static void
check_allocations(bool tpm1)
{
	size_t count, i, capped_calls;

	golden_boot(tpm1);
	count = allocations;
	for (i = 1; i <= count; i++) {
		boot_input(tpm1);
		/* All allocations keep failing, including in recovery. */
		fail_allocation = i;
		register_contents();
		ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
		check_capped();
		capped_calls = calls;
		ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
		ATF_REQUIRE_EQ(calls, capped_calls);
	}
	reset();
}

static void
check_firmware_errors(bool tpm1)
{
	size_t i;

	golden_boot(tpm1);
	for (i = 1; i <= golden_count; i++) {
		boot_input(tpm1);
		register_contents();
		fail_call = i;
		ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
		check_capped();
	}
	reset();
}

static void
check_cap_errors(bool tpm1)
{
	unsigned int pcr;
	size_t capped_calls;

	for (pcr = 8; pcr <= 10; pcr++) {
		boot_input(tpm1);
		register_contents();
		fail_call = 1;
		cap_fail_pcr = pcr;
		/* Model firmware disabling the TPM after a device error. */
		persistent_failure = pcr == 10;
		ATF_REQUIRE_EQ(efi_tpm_measure_boot(), EIO);
		ATF_REQUIRE(tpm_uncapped);
		ATF_REQUIRE_EQ(cap_attempts[0], 1);
		ATF_REQUIRE_EQ(cap_attempts[1], 1);
		capped_calls = calls;
		ATF_REQUIRE_EQ(efi_tpm_measure_boot(), EIO);
		ATF_REQUIRE_EQ(calls, capped_calls);
	}
	/* A failed cap during loading must also prevent the later boot. */
	boot_input(tpm1);
	fail_allocation = 1;
	cap_fail_pcr = 8;
	register_contents();
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), EIO);
	reset();
}

ATF_TC_WITHOUT_HEAD(allocations_tpm2);
ATF_TC_BODY(allocations_tpm2, tc)
{
	check_allocations(false);
}

ATF_TC_WITHOUT_HEAD(allocations_tpm1);
ATF_TC_BODY(allocations_tpm1, tc)
{
	check_allocations(true);
}

ATF_TC_WITHOUT_HEAD(firmware_errors_tpm2);
ATF_TC_BODY(firmware_errors_tpm2, tc)
{
	check_firmware_errors(false);
}

ATF_TC_WITHOUT_HEAD(firmware_errors_tpm1);
ATF_TC_BODY(firmware_errors_tpm1, tc)
{
	check_firmware_errors(true);
}

ATF_TC_WITHOUT_HEAD(cap_errors_tpm2);
ATF_TC_BODY(cap_errors_tpm2, tc)
{
	check_cap_errors(false);
}

ATF_TC_WITHOUT_HEAD(cap_errors_tpm1);
ATF_TC_BODY(cap_errors_tpm1, tc)
{
	check_cap_errors(true);
}

ATF_TC_WITHOUT_HEAD(volume_full);
ATF_TC_BODY(volume_full, tc)
{

	golden_boot(false);
	boot_input(false);
	register_contents();
	full_log = true;
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
	ATF_REQUIRE_EQ(nevents, golden_count);
	ATF_REQUIRE_EQ(memcmp(pcrs, golden_pcrs, sizeof(pcrs)), 0);
	ATF_REQUIRE_EQ(cap_attempts[0] + cap_attempts[1], 0);
	ATF_REQUIRE_EQ(tpm_protocol, EFI_TPM_2);
	/* Full logs also cannot prevent allocation-free error marking. */
	boot_input(false);
	full_log = true;
	fail_allocation = 1;
	register_contents();
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
	check_capped();
	reset();
}

ATF_TC_WITHOUT_HEAD(no_tpm);
ATF_TC_BODY(no_tpm, tc)
{

	reset();
	tpm_protocol = EFI_TPM_NONE;
	register_contents();
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
	ATF_REQUIRE_EQ(allocations, 0);
	ATF_REQUIRE_EQ(calls, 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, metadata64);
	ATF_TP_ADD_TC(tp, metadata32);
	ATF_TP_ADD_TC(tp, placement64);
	ATF_TP_ADD_TC(tp, placement32);
	ATF_TP_ADD_TC(tp, allocations_tpm2);
	ATF_TP_ADD_TC(tp, allocations_tpm1);
	ATF_TP_ADD_TC(tp, firmware_errors_tpm2);
	ATF_TP_ADD_TC(tp, firmware_errors_tpm1);
	ATF_TP_ADD_TC(tp, cap_errors_tpm2);
	ATF_TP_ADD_TC(tp, cap_errors_tpm1);
	ATF_TP_ADD_TC(tp, volume_full);
	ATF_TP_ADD_TC(tp, no_tpm);
	return (atf_no_error());
}
