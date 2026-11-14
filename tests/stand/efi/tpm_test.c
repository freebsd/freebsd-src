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

struct test_image {
	unsigned char segment[64];
	unsigned char gap[64];
	unsigned char symbols[32];
};

static struct preloaded_file file;
static char filename[] = "/boot/kernel/kernel";
static char filetype[] = "elf kernel";
static struct env_var variable = { "testconfig", "value", 0, NULL };
struct env_var *test_environment = &variable;
EFI_BOOT_SERVICES *BS;

static EFI_TCG2_PROTOCOL protocol2;
static EFI_TCG_PROTOCOL protocol1;
static struct test_image image = {
	.segment = "loadable segment",
	.symbols = "symbol and string tables",
};
static const vm_offset_t load_base = 0x100000;
static unsigned char pcrs[FREEBSD_LOADER_FILE_PCR + 1][SHA256_DIGEST_LENGTH];
static struct recorded_event events[64];
static size_t nevents, allocations, fail_allocation;
static size_t calls, fail_call;
static unsigned int cap_attempts[nitems(pcrs)], cap_fail_pcr;
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
	ATF_REQUIRE(addr - file.f_addr < sizeof(image));
	return ((unsigned char *)&image + (addr - file.f_addr));
}

static void
extend(UINT32 pcr, const unsigned char *digest)
{
	SHA256_CTX ctx;
	SHA1_CTX ctx1;

	ATF_REQUIRE(pcr == FREEBSD_LOADER_CONFIG_PCR ||
	    pcr == FREEBSD_LOADER_FILE_PCR);
	if (use_tpm1) {
		SHA1_Init(&ctx1);
		SHA1_Update(&ctx1, pcrs[pcr], SHA_DIGEST_LENGTH);
		SHA1_Update(&ctx1, digest, SHA_DIGEST_LENGTH);
		SHA1_Final(pcrs[pcr], &ctx1);
		return;
	}
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, pcrs[pcr], SHA256_DIGEST_LENGTH);
	SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH);
	SHA256_Final(pcrs[pcr], &ctx);
}

static void
hash_data(const void *data, size_t len, unsigned char *digest)
{
	SHA256_CTX ctx;
	SHA1_CTX ctx1;

	if (use_tpm1) {
		SHA1_Init(&ctx1);
		SHA1_Update(&ctx1, data, len);
		SHA1_Final(digest, &ctx1);
	} else {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, data, len);
		SHA256_Final(digest, &ctx);
	}
}

static EFI_STATUS
record_event(UINT32 pcr, EFI_PHYSICAL_ADDRESS addr, UINT64 len,
    const void *event, size_t event_len)
{
	struct recorded_event *record;
	bool failure;

	calls++;
	ATF_REQUIRE(pcr == FREEBSD_LOADER_CONFIG_PCR ||
	    pcr == FREEBSD_LOADER_FILE_PCR);
	failure = len == sizeof(efi_tpm_failure_event) &&
	    memcmp((void *)(uintptr_t)addr, efi_tpm_failure_event, len) == 0;
	if (failure) {
		cap_attempts[pcr]++;
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
	hash_data((const void *)(uintptr_t)addr, len, record->digest);
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
	nevents = allocations = fail_allocation = 0;
	calls = fail_call = cap_fail_pcr = 0;
	bzero(cap_attempts, sizeof(cap_attempts));
	use_tpm1 = full_log = persistent_failure = device_failed = false;
	tpm_log_full = tpm_uncapped = false;
	file.f_name = filename;
	file.f_type = filetype;
	file.f_addr = load_base;
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
	const Elf32_Addr link_addr = 0x200000;
	const vm_offset_t ssym = base + offsetof(struct test_image, symbols);
	Elf64_Ehdr eh64 = { .e_ident = { ELFMAG0, ELFMAG1, ELFMAG2,
	    ELFMAG3, ELFCLASS64 }, .e_entry = link_addr, .e_shnum = 1 };
	Elf32_Ehdr eh32 = { .e_ident = { ELFMAG0, ELFMAG1, ELFMAG2,
	    ELFMAG3, ELFCLASS32 }, .e_entry = link_addr, .e_shnum = 1 };
	Elf64_Shdr sh64 = { .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC,
	    .sh_size = sizeof(image.segment) };
	Elf32_Shdr sh32 = { .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC,
	    .sh_size = sizeof(image.segment) };
	const struct {
		int type;
		Elf64_Addr value;
	} metadata[] = {
		{ MODINFOMD_SSYM, ssym },
		{ MODINFOMD_ESYM, ssym + sizeof(image.symbols) },
		{ MODINFOMD_DYNAMIC, link_addr + 0x10 },
		{ MODINFOMD_CTORS_ADDR, link_addr + 0x20 },
		{ MODINFOMD_CTORS_SIZE, is64 ? sizeof(Elf64_Addr) :
		    sizeof(Elf32_Addr) },	/* One constructor. */
	};
	Elf32_Addr value32;
	size_t i;

	reset();
	file.f_addr = base;
	eh64.e_type = eh32.e_type = relocatable ? ET_REL : ET_EXEC;
	sh64.sh_addr = sh32.sh_addr = relocatable ? base : link_addr;
	if (is64) {
		add_metadata(MODINFOMD_ELFHDR, &eh64, sizeof(eh64));
		add_metadata(MODINFOMD_SHDR, &sh64, sizeof(sh64));
	} else {
		add_metadata(MODINFOMD_ELFHDR, &eh32, sizeof(eh32));
		add_metadata(MODINFOMD_SHDR, &sh32, sizeof(sh32));
	}
	for (i = 0; i < nitems(metadata); i++) {
		if (is64) {
			add_metadata(metadata[i].type, &metadata[i].value,
			    sizeof(metadata[i].value));
		} else {
			value32 = metadata[i].value;
			add_metadata(metadata[i].type, &value32,
			    sizeof(value32));
		}
	}
}

static void
metadata_digest(unsigned char *digest)
{

	nevents = 0;
	efi_tpm_measure_file_metadata(&file);
	ATF_REQUIRE_EQ(nevents, 1);
	ATF_REQUIRE_EQ(events[0].pcr, FREEBSD_LOADER_FILE_PCR);
	bcopy(events[0].digest, digest, SHA256_DIGEST_LENGTH);
}

static void
check_metadata_changes(bool is64)
{
	struct file_metadata *md;
	unsigned char before[SHA256_DIGEST_LENGTH], after[SHA256_DIGEST_LENGTH];
	size_t i;

	elf_metadata(is64, false, load_base);
	metadata_digest(before);
	/* EI_CLASS selects the layout. Mutate each of the other bytes. */
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

	elf_metadata(is64, true, load_base);
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
static unsigned char golden_pcrs[nitems(pcrs)][SHA256_DIGEST_LENGTH];
static size_t golden_count;

static void
boot_input(bool tpm1)
{

	elf_metadata(true, false, load_base);
	use_tpm1 = tpm1;
	tpm_protocol = tpm1 ? EFI_TPM_1 : EFI_TPM_2;
}

static void
register_contents(void)
{

	efi_tpm_measure_file(&file, file.f_addr, sizeof(image.segment));
	efi_tpm_measure_file(&file, file.f_addr + offsetof(struct test_image,
	    symbols), sizeof(image.symbols));
}

static void
golden_boot(bool tpm1)
{
	static const char environment[] =
	    "FreeBSD loader environment v1\0testconfig=value\0";
	static const char identity[] =
	    "FreeBSD loader file identity v1\0elf kernel\0/boot/kernel/kernel\0";
	const struct {
		const char *name;
		UINT32 pcr;
		const void *data;
		size_t size;
	} expected[] = {
		{ "environment", FREEBSD_LOADER_CONFIG_PCR,
		    environment, sizeof(environment) },
		{ "file identity", FREEBSD_LOADER_CONFIG_PCR,
		    identity, sizeof(identity) },
		{ "ELF metadata", FREEBSD_LOADER_FILE_PCR, NULL, 0 },
		{ "load segment", FREEBSD_LOADER_FILE_PCR,
		    image.segment, sizeof(image.segment) },
		{ "symbol table", FREEBSD_LOADER_FILE_PCR,
		    image.symbols, sizeof(image.symbols) },
	};
	unsigned char elf_digest[SHA256_DIGEST_LENGTH];
	unsigned char digest[SHA256_DIGEST_LENGTH];
	size_t i, digest_len;

	/* Get the metadata digest independently of its position in the boot. */
	boot_input(tpm1);
	metadata_digest(elf_digest);

	boot_input(tpm1);
	register_contents();
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), 0);
	ATF_REQUIRE_EQ(nevents, nitems(expected));
	digest_len = tpm1 ? SHA_DIGEST_LENGTH : SHA256_DIGEST_LENGTH;
	for (i = 0; i < nevents; i++) {
		ATF_REQUIRE_MSG(events[i].pcr == expected[i].pcr,
		    "%s measured into PCR %u", expected[i].name, events[i].pcr);
		if (expected[i].data != NULL)
			hash_data(expected[i].data, expected[i].size, digest);
		else
			memcpy(digest, elf_digest, digest_len);
		ATF_REQUIRE_MSG(memcmp(events[i].digest, digest,
		    digest_len) == 0, "unexpected %s digest at event %zu",
		    expected[i].name, i);
	}
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
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_CONFIG_PCR], 1);
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_FILE_PCR], 1);
	ATF_REQUIRE(nevents >= 2);
	prefix = nevents - 2;
	ATF_REQUIRE(prefix <= golden_count);
	ATF_REQUIRE_EQ(events[prefix].pcr, FREEBSD_LOADER_CONFIG_PCR);
	ATF_REQUIRE_EQ(events[prefix + 1].pcr, FREEBSD_LOADER_FILE_PCR);
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
	ATF_REQUIRE(memcmp(pcrs[FREEBSD_LOADER_CONFIG_PCR],
	    golden_pcrs[FREEBSD_LOADER_CONFIG_PCR], sizeof(pcrs[0])) != 0);
	ATF_REQUIRE(memcmp(pcrs[FREEBSD_LOADER_FILE_PCR],
	    golden_pcrs[FREEBSD_LOADER_FILE_PCR], sizeof(pcrs[0])) != 0);
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
check_boot_refused(void)
{
	size_t capped_calls;

	register_contents();
	fail_call = 1;
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), EIO);
	ATF_REQUIRE(tpm_uncapped);
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_CONFIG_PCR], 1);
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_FILE_PCR], 1);
	capped_calls = calls;
	ATF_REQUIRE_EQ(efi_tpm_measure_boot(), EIO);
	ATF_REQUIRE_EQ(calls, capped_calls);
}

static void
check_cap_errors(bool tpm1)
{

	boot_input(tpm1);
	cap_fail_pcr = FREEBSD_LOADER_CONFIG_PCR;
	check_boot_refused();

	boot_input(tpm1);
	cap_fail_pcr = FREEBSD_LOADER_FILE_PCR;
	check_boot_refused();

	boot_input(tpm1);
	persistent_failure = true;
	check_boot_refused();

	/* A failed cap during loading must also prevent the later boot. */
	boot_input(tpm1);
	fail_allocation = 1;
	cap_fail_pcr = FREEBSD_LOADER_CONFIG_PCR;
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
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_CONFIG_PCR], 0);
	ATF_REQUIRE_EQ(cap_attempts[FREEBSD_LOADER_FILE_PCR], 0);
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
