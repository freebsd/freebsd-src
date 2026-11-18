/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#include <atf-c.h>
#include <sha256.h>

#include "tpm.c"

#undef malloc
#undef calloc

struct recorded_event {
	UINT32 pcr;
	unsigned char digest[SHA256_DIGEST_LENGTH];
};

static struct preloaded_file file;
static char filename[] = "/boot/kernel/kernel";
static char filetype[] = "elf kernel";
static struct env_var variable = { "testconfig", "value", 0, NULL };
struct env_var *test_environment = &variable;
EFI_BOOT_SERVICES *BS;

static EFI_TCG2_PROTOCOL protocol2;
static unsigned char contents[256];
static unsigned char pcrs[2][SHA256_DIGEST_LENGTH];
static struct recorded_event events[64];
static size_t nevents, allocations, fail_allocation;

void *
test_malloc(size_t len)
{

	return (++allocations == fail_allocation ? NULL : malloc(len));
}

void *
test_calloc(size_t n, size_t len)
{

	return (++allocations == fail_allocation ? NULL : calloc(n, len));
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

	ATF_REQUIRE(pcr == 8 || pcr == 9);
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, pcrs[pcr - 8], SHA256_DIGEST_LENGTH);
	SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH);
	SHA256_Final(pcrs[pcr - 8], &ctx);
}

static EFI_STATUS EFIAPI
measure2(EFI_TCG2_PROTOCOL *this __unused, UINT64 flags __unused,
    EFI_PHYSICAL_ADDRESS addr, UINT64 len, EFI_TCG2_EVENT *event)
{
	SHA256_CTX ctx;
	struct recorded_event *record;

	ATF_REQUIRE(nevents < nitems(events));
	record = &events[nevents++];
	record->pcr = event->Header.PCRIndex;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, (void *)(uintptr_t)addr, len);
	SHA256_Final(record->digest, &ctx);
	extend(record->pcr, record->digest);
	return (EFI_SUCCESS);
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
	file.f_name = filename;
	file.f_type = filetype;
	file.f_addr = 0x100000;
	protocol2.HashLogExtendEvent = measure2;
	tcg2 = &protocol2;
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

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, metadata64);
	ATF_TP_ADD_TC(tp, metadata32);
	ATF_TP_ADD_TC(tp, placement64);
	ATF_TP_ADD_TC(tp, placement32);
	return (atf_no_error());
}
