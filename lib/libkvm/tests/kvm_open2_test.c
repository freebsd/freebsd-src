/*-
 * Copyright (c) 2017 Enji Cooper <ngie@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "../../../sys/powerpc/include/minidump.h"
#include "kvm_test_common.h"

#define	PPC64_DMAP_BASE		0xc000000000000000ULL
#define	PPC64_KERN_BASE		0xc008000000000000ULL
#define	RADIX_ROOT_SIZE		(1UL << 16)
#define	RADIX_ROOT_ENTRIES	(RADIX_ROOT_SIZE / sizeof(uint64_t))
#define	RADIX_VALID		0x8000000000000000ULL
#define	RADIX_LEAF		0x4000000000000000ULL
#define	RADIX_READ		0x4
#define	RADIX_WRITE		0x2

static int
unresolved_symbol(const char *name __unused, kvaddr_t *value __unused)
{

	return (-1);
}

static void
create_powerpc64le_kernel(const char *path)
{
	Elf64_Ehdr ehdr;
	ssize_t n;
	int fd;

	memset(&ehdr, 0, sizeof(ehdr));
	memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_type = htole16(ET_DYN);
	ehdr.e_machine = htole16(EM_PPC64);
	ehdr.e_version = htole32(EV_CURRENT);
	ehdr.e_ehsize = htole16(sizeof(ehdr));

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE(fd != -1);
	n = write(fd, &ehdr, sizeof(ehdr));
	ATF_REQUIRE_EQ(n, (ssize_t)sizeof(ehdr));
	ATF_REQUIRE(close(fd) == 0);
}

static void
create_invalid_powerpc64_minidump(const char *path)
{
	char buf[PAGE_SIZE];
	ssize_t n;
	int fd;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, "minidump FreeBSD/powerpc64",
	    sizeof("minidump FreeBSD/powerpc64"));

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE(fd != -1);
	n = write(fd, buf, sizeof(buf));
	ATF_REQUIRE_EQ(n, (ssize_t)sizeof(buf));
	ATF_REQUIRE(close(fd) == 0);
}

static void
write_all(int fd, const void *buf, size_t len)
{
	const char *p;
	ssize_t n;

	p = buf;
	while (len != 0) {
		n = write(fd, p, len);
		ATF_REQUIRE(n > 0);
		p += n;
		len -= n;
	}
}

static void
create_powerpc64le_radix_minidump(const char *path)
{
	struct minidumphdr hdr;
	uint64_t entry, *dump_avail, *root;
	uint8_t *page;
	size_t root_index;
	int fd, i;

	dump_avail = calloc(1, PAGE_SIZE);
	root = calloc(1, RADIX_ROOT_SIZE);
	page = calloc(1, PAGE_SIZE);
	ATF_REQUIRE(dump_avail != NULL);
	ATF_REQUIRE(root != NULL);
	ATF_REQUIRE(page != NULL);

	memset(&hdr, 0, sizeof(hdr));
	strlcpy(hdr.magic, MINIDUMP_MAGIC, sizeof(hdr.magic));
	strlcpy(hdr.mmu_name, "mmu_radix", sizeof(hdr.mmu_name));
	hdr.version = htole32(MINIDUMP_VERSION);
	hdr.bitmapsize = htole32(PAGE_SIZE);
	hdr.pmapsize = htole32(RADIX_ROOT_SIZE);
	hdr.kernbase = htole64(PPC64_KERN_BASE);
	hdr.kernend = htole64(PPC64_KERN_BASE + PAGE_SIZE);
	hdr.dmapbase = htole64(PPC64_DMAP_BASE);
	hdr.dmapend = htole64(PPC64_DMAP_BASE + 0x10000);
	hdr.startkernel = htole64(PPC64_DMAP_BASE + 0x4000);
	hdr.endkernel = htole64(PPC64_DMAP_BASE + 0x5000);
	hdr.dumpavailsize = htole32(PAGE_SIZE);

	dump_avail[0] = htole64(0);
	dump_avail[1] = htole64(0x10000);

	root_index = (PPC64_KERN_BASE >> 39) & (RADIX_ROOT_ENTRIES - 1);
	root[root_index] = htobe64(RADIX_VALID | 0x1000 | 9);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE(fd != -1);
	memset(page, 0, PAGE_SIZE);
	memcpy(page, &hdr, sizeof(hdr));
	write_all(fd, page, PAGE_SIZE);
	write_all(fd, dump_avail, PAGE_SIZE);
	memset(page, 0, PAGE_SIZE);
	for (i = 1; i <= 4; i++)
		page[i / NBBY] |= 1U << (i % NBBY);
	write_all(fd, page, PAGE_SIZE);
	write_all(fd, root, RADIX_ROOT_SIZE);

	/* L2, L3, and PTE pages, followed by the mapped data page. */
	for (i = 1; i <= 3; i++) {
		memset(page, 0, PAGE_SIZE);
		if (i != 3)
			entry = htobe64(RADIX_VALID | ((i + 1) * 0x1000) |
			    9);
		else
			entry = htobe64(RADIX_VALID | RADIX_LEAF |
			    RADIX_READ | RADIX_WRITE | 0x4000);
		memcpy(page, &entry, sizeof(entry));
		write_all(fd, page, PAGE_SIZE);
	}
	memset(page, 0, PAGE_SIZE);
	memcpy(page, "radix minidump", sizeof("radix minidump"));
	write_all(fd, page, PAGE_SIZE);
	ATF_REQUIRE(close(fd) == 0);

	free(page);
	free(root);
	free(dump_avail);
}

ATF_TC_WITHOUT_HEAD(kvm_open2_negative_test_nonexistent_corefile);
ATF_TC_BODY(kvm_open2_negative_test_nonexistent_corefile, tc)
{

	errbuf_clear();
	ATF_CHECK(kvm_open2(NULL, "/nonexistent", O_RDONLY, NULL, NULL) == NULL);
	ATF_CHECK(!errbuf_has_error(errbuf));
	errbuf_clear();
	ATF_CHECK(kvm_open2(NULL, "/nonexistent", O_RDONLY,
	    errbuf, NULL) == NULL);
	ATF_CHECK(errbuf_has_error(errbuf));
}

ATF_TC_WITHOUT_HEAD(kvm_open2_negative_test_nonexistent_execfile);
ATF_TC_BODY(kvm_open2_negative_test_nonexistent_execfile, tc)
{

	errbuf_clear();
	ATF_CHECK(kvm_open2("/nonexistent", _PATH_DEVZERO, O_RDONLY,
	    NULL, NULL) == NULL);
	ATF_CHECK(strlen(errbuf) == 0);
	errbuf_clear();
	ATF_CHECK(kvm_open2("/nonexistent", _PATH_DEVZERO, O_RDONLY,
	    errbuf, NULL) == NULL);
	ATF_CHECK(errbuf_has_error(errbuf));
}

ATF_TC(kvm_open2_negative_test_invalid_corefile);
ATF_TC_HEAD(kvm_open2_negative_test_invalid_corefile, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(kvm_open2_negative_test_invalid_corefile, tc)
{
	kvm_t *kd;

	errbuf_clear();
	atf_utils_create_file("some-file", "this is a text file");
	kd = kvm_open2(NULL, "some-file", O_RDONLY, errbuf, NULL);
	ATF_CHECK(errbuf_has_error(errbuf));
	ATF_REQUIRE_MSG(kd == NULL, "kvm_open2 succeeded");
}

ATF_TC(kvm_open2_negative_test_invalid_execfile);
ATF_TC_HEAD(kvm_open2_negative_test_invalid_execfile, tc)
{

	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(kvm_open2_negative_test_invalid_execfile, tc)
{
	kvm_t *kd;

	errbuf_clear();
	atf_utils_create_file("some-file", "this is a text file");
	kd = kvm_open2("some-file", "/bin/sh", O_RDONLY, errbuf, NULL);
	ATF_CHECK(errbuf_has_error(errbuf));
	ATF_REQUIRE_MSG(kd == NULL, "kvm_open2 succeeded unexpectedly");
}

ATF_TC_WITHOUT_HEAD(kvm_open2_powerpc64le_minidump_probe);
ATF_TC_BODY(kvm_open2_powerpc64le_minidump_probe, tc)
{
	kvm_t *kd;

	create_powerpc64le_kernel("kernel");
	create_invalid_powerpc64_minidump("vmcore");
	errbuf_clear();
	kd = kvm_open2("kernel", "vmcore", O_RDONLY, errbuf,
	    unresolved_symbol);
	ATF_REQUIRE_MSG(kd == NULL, "kvm_open2 succeeded unexpectedly");
	ATF_CHECK_MATCH("wrong minidump version", errbuf);
}

ATF_TC_WITHOUT_HEAD(kvm_open2_powerpc64le_radix_minidump);
ATF_TC_BODY(kvm_open2_powerpc64le_radix_minidump, tc)
{
	char buf[sizeof("radix minidump")];
	kvm_t *kd;

	create_powerpc64le_kernel("kernel-radix");
	create_powerpc64le_radix_minidump("vmcore-radix");
	errbuf_clear();
	kd = kvm_open2("kernel-radix", "vmcore-radix", O_RDONLY, errbuf,
	    unresolved_symbol);
	ATF_REQUIRE_MSG(kd != NULL, "kvm_open2 failed: %s", errbuf);
	ATF_REQUIRE_EQ(kvm_read2(kd, PPC64_KERN_BASE, buf, sizeof(buf)),
	    (ssize_t)sizeof(buf));
	ATF_CHECK_STREQ(buf, "radix minidump");
	memset(buf, 0, sizeof(buf));
	ATF_REQUIRE_EQ(kvm_read2(kd, PPC64_DMAP_BASE + 0x4000, buf,
	    sizeof(buf)), (ssize_t)sizeof(buf));
	ATF_CHECK_STREQ(buf, "radix minidump");
	ATF_REQUIRE(kvm_close(kd) == 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, kvm_open2_negative_test_invalid_corefile);
	ATF_TP_ADD_TC(tp, kvm_open2_negative_test_invalid_execfile);
	ATF_TP_ADD_TC(tp, kvm_open2_negative_test_nonexistent_corefile);
	ATF_TP_ADD_TC(tp, kvm_open2_negative_test_nonexistent_execfile);
	ATF_TP_ADD_TC(tp, kvm_open2_powerpc64le_minidump_probe);
	ATF_TP_ADD_TC(tp, kvm_open2_powerpc64le_radix_minidump);

	return (atf_no_error());
}
