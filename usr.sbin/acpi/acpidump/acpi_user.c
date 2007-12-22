/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <kenv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "acpidump.h"

static char	hint_acpi_0_rsdp[] = "hint.acpi.0.rsdp";
static char	machdep_acpi_root[] = "machdep.acpi_root";
static int      acpi_mem_fd = -1;

struct acpi_user_mapping {
	LIST_ENTRY(acpi_user_mapping) link;
	vm_offset_t     pa;
	caddr_t         va;
	size_t          size;
};

LIST_HEAD(acpi_user_mapping_list, acpi_user_mapping) maplist;

static void
acpi_user_init(void)
{

	if (acpi_mem_fd == -1) {
		acpi_mem_fd = open("/dev/mem", O_RDONLY);
		if (acpi_mem_fd == -1)
			err(1, "opening /dev/mem");
		LIST_INIT(&maplist);
	}
}

static struct acpi_user_mapping *
acpi_user_find_mapping(vm_offset_t pa, size_t size)
{
	struct	acpi_user_mapping *map;

	/* First search for an existing mapping */
	for (map = LIST_FIRST(&maplist); map; map = LIST_NEXT(map, link)) {
		if (map->pa <= pa && map->size >= pa + size - map->pa)
			return (map);
	}

	/* Then create a new one */
	size = round_page(pa + size) - trunc_page(pa);
	pa = trunc_page(pa);
	map = malloc(sizeof(struct acpi_user_mapping));
	if (!map)
		errx(1, "out of memory");
	map->pa = pa;
	map->va = mmap(0, size, PROT_READ, MAP_SHARED, acpi_mem_fd, pa);
	map->size = size;
	if ((intptr_t) map->va == -1)
		err(1, "can't map address");
	LIST_INSERT_HEAD(&maplist, map, link);

	return (map);
}

static struct ACPIrsdp *
acpi_get_rsdp(u_long addr)
{
	struct ACPIrsdp rsdp;
	size_t len;

	/* Read in the table signature and check it. */
	pread(acpi_mem_fd, &rsdp, 8, addr);
	if (memcmp(rsdp.signature, "RSD PTR ", 8))
		return (NULL);

	/* Read the entire table. */
	pread(acpi_mem_fd, &rsdp, sizeof(rsdp), addr);

	/* Run the checksum only over the version 1 header. */
	if (acpi_checksum(&rsdp, 20))
		return (NULL);

	/* If the revision is 0, assume a version 1 length. */
	if (rsdp.revision == 0)
		len = 20;
	else
		len = rsdp.length;

	/* XXX Should handle ACPI 2.0 RSDP extended checksum here. */

	return (acpi_map_physical(addr, len));
}

static struct ACPIrsdp *
acpi_scan_rsd_ptr(void)
{
#if defined(__amd64__) || defined(__i386__)
	struct ACPIrsdp *rsdp;
	u_long		addr, end;

	/*
	 * On ia32, scan physical memory for the RSD PTR if above failed.
	 * According to section 5.2.2 of the ACPI spec, we only consider
	 * two regions for the base address:
	 * 1. EBDA (1 KB area addressed by the 16 bit pointer at 0x40E
	 * 2. High memory (0xE0000 - 0xFFFFF)
	 */
	addr = RSDP_EBDA_PTR;
	pread(acpi_mem_fd, &addr, sizeof(uint16_t), addr);
	addr <<= 4;
	end = addr + RSDP_EBDA_SIZE;
	for (; addr < end; addr += 16)
		if ((rsdp = acpi_get_rsdp(addr)) != NULL)
			return (rsdp);
	addr = RSDP_HI_START;
	end = addr + RSDP_HI_SIZE;
	for (; addr < end; addr += 16)
		if ((rsdp = acpi_get_rsdp(addr)) != NULL)
			return (rsdp);
#endif /* __amd64__ || __i386__ */
	return (NULL);
}

/*
 * Public interfaces
 */
struct ACPIrsdp *
acpi_find_rsd_ptr(void)
{
	struct ACPIrsdp *rsdp;
	char		buf[20];
	u_long		addr;
	size_t		len;

	acpi_user_init();

	addr = 0;

	/* Attempt to use kenv or sysctl to find RSD PTR record. */
	if (kenv(KENV_GET, hint_acpi_0_rsdp, buf, 20) == 0)
		addr = strtoul(buf, NULL, 0);
	if (addr == 0) {
		len = sizeof(addr);
		if (sysctlbyname(machdep_acpi_root, &addr, &len, NULL, 0) != 0)
			addr = 0;
	}
	if (addr != 0 && (rsdp = acpi_get_rsdp(addr)) != NULL)
		return (rsdp);

	return (acpi_scan_rsd_ptr());
}

void *
acpi_map_physical(vm_offset_t pa, size_t size)
{
	struct	acpi_user_mapping *map;

	map = acpi_user_find_mapping(pa, size);
	return (map->va + (pa - map->pa));
}

struct ACPIsdt *
dsdt_load_file(char *infile)
{
	struct ACPIsdt	*sdt;
	uint8_t		*dp;
	struct stat	 sb;

	if ((acpi_mem_fd = open(infile, O_RDONLY)) == -1)
		errx(1, "opening %s", infile);

	LIST_INIT(&maplist);

	if (fstat(acpi_mem_fd, &sb) == -1)
		errx(1, "fstat %s", infile);

	dp = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, acpi_mem_fd, 0);
	if (dp == NULL)
		errx(1, "mmap %s", infile);

	sdt = (struct ACPIsdt *)dp;
	if (strncmp(dp, "DSDT", 4) != 0 || acpi_checksum(sdt, sdt->len) != 0)
		return (NULL);

	return (sdt);
}
