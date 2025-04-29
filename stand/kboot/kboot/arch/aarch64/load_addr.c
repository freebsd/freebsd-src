/*-
 * Copyright (c) 2022 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <machine/metadata.h>
#include <sys/linker.h>
#include <fdt_platform.h>
#include <libfdt.h>

#include "kboot.h"
#include "efi.h"

static bool
do_memory_from_fdt(int fd)
{
	struct stat sb;
	char *buf = NULL;
	int len, offset;
	uint32_t sz, ver, esz;
	uint64_t mmap_pa;
	const uint32_t *u32p;
	const uint64_t *u64p;

	if (fstat(fd, &sb) < 0)
		return false;
	buf = malloc(sb.st_size);
	if (buf == NULL)
		return false;
	len = read(fd, buf, sb.st_size);
	/* NB: we're reading this from sysfs, so mismatch OK */
	if (len <= 0)
		goto errout;

	/*
	 * Look for /chosen to find these values:
	 * linux,uefi-system-table	PA of the UEFI System Table.
	 * linux,uefi-mmap-start	PA of the UEFI memory map
	 * linux,uefi-mmap-size		Size of mmap
	 * linux,uefi-mmap-desc-size	Size of each entry of mmap
	 * linux,uefi-mmap-desc-ver	Format version, should be 1
	 */
	offset = fdt_path_offset(buf, "/chosen");
	if (offset <= 0)
		goto errout;
	u64p = fdt_getprop(buf, offset, "linux,uefi-system-table", &len);
	if (u64p == NULL)
		goto errout;
	efi_set_systbl(fdt64_to_cpu(*u64p));
	u32p = fdt_getprop(buf, offset, "linux,uefi-mmap-desc-ver", &len);
	if (u32p == NULL)
		goto errout;
	ver = fdt32_to_cpu(*u32p);
	u32p = fdt_getprop(buf, offset, "linux,uefi-mmap-desc-size", &len);
	if (u32p == NULL)
		goto errout;
	esz = fdt32_to_cpu(*u32p);
	u32p = fdt_getprop(buf, offset, "linux,uefi-mmap-size", &len);
	if (u32p == NULL)
		goto errout;
	sz = fdt32_to_cpu(*u32p);
	u64p = fdt_getprop(buf, offset, "linux,uefi-mmap-start", &len);
	if (u64p == NULL)
		goto errout;
	mmap_pa = fdt64_to_cpu(*u64p);
	free(buf);

	printf("UEFI MMAP: Ver %d Ent Size %d Tot Size %d PA %#lx\n",
	    ver, esz, sz, mmap_pa);

	efi_read_from_pa(mmap_pa, sz, esz, ver);
	return true;

errout:
	free(buf);
	return false;
}

bool
enumerate_memory_arch(void)
{
	int fd = -1;
	bool rv = false;

	/*
	 * FDT publishes the parameters for the memory table in a series of
	 * nodes in the DTB.  One of them is the physical address for the memory
	 * table. Try to open the fdt nblob to find this information if we can
	 * and try to grab things from memory. If we return rv == TRUE then
	 * we found it. The global efi_map_phys_src is set != 0 when we know
	 * the PA but can't read it.
	 */
	fd = open("host:/sys/firmware/fdt", O_RDONLY);
	if (fd != -1) {
		rv = do_memory_from_fdt(fd);
		close(fd);
	}

	/*
	 * One would think that one could use the raw EFI map to find memory
	 * that's free to boot the kernel with. However, Linux reserves some
	 * areas that it needs to properly. I'm not sure if the printf should be
	 * a panic, but for now, so I can debug (maybe at the loader prompt),
	 * I'm printing and carrying on.
	 */
	if (!rv) {
		printf("Could not obtain UEFI memory tables, expect failure\n");
	}

	populate_avail_from_iomem();
	print_avail();

	return true;
}

uint64_t
kboot_get_phys_load_segment(void)
{
#define HOLE_SIZE	(64ul << 20)
#define KERN_ALIGN	(2ul << 20)
	static uint64_t	s = 0;

	if (s != 0)
		return (s);

	print_avail();
	s = first_avail(KERN_ALIGN, HOLE_SIZE, SYSTEM_RAM);
	printf("KBOOT GET PHYS Using %#llx\n", (long long)s);
	if (s != 0)
		return (s);
	s = 0x40000000 | 0x4200000;	/* should never get here */
	/* XXX PANIC? XXX */
	printf("Falling back to the crazy address %#lx which works in qemu\n", s);
	return (s);
}

void
bi_loadsmap(struct preloaded_file *kfp)
{
	efi_bi_loadsmap(kfp);
}
