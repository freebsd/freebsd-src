/*-
 * Copyright (c) 2022 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/efi.h>
#include <machine/metadata.h>
#include <sys/linker.h>
#include <fdt_platform.h>
#include <libfdt.h>

#include "kboot.h"
#include "bootstrap.h"

/*
 * Info from dtb about the EFI system
 */
vm_paddr_t efi_systbl_phys;
struct efi_map_header *efi_map_hdr;
uint32_t efi_map_size;
vm_paddr_t efi_map_phys_src;	/* From DTB */
vm_paddr_t efi_map_phys_dst;	/* From our memory map metadata module */

typedef void (*efi_map_entry_cb)(struct efi_md *, void *argp);

static void
foreach_efi_map_entry(struct efi_map_header *efihdr, efi_map_entry_cb cb, void *argp)
{
	struct efi_md *map, *p;
	size_t efisz;
	int ndesc, i;

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);

	if (efihdr->descriptor_size == 0)
		return;
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	for (i = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		cb(p, argp);
	}
}

static void
print_efi_map_entry(struct efi_md *p, void *argp __unused)
{
	const char *type;
	static const char *types[] = {
		"Reserved",
		"LoaderCode",
		"LoaderData",
		"BootServicesCode",
		"BootServicesData",
		"RuntimeServicesCode",
		"RuntimeServicesData",
		"ConventionalMemory",
		"UnusableMemory",
		"ACPIReclaimMemory",
		"ACPIMemoryNVS",
		"MemoryMappedIO",
		"MemoryMappedIOPortSpace",
		"PalCode",
		"PersistentMemory"
	};

	if (p->md_type < nitems(types))
		type = types[p->md_type];
	else
		type = "<INVALID>";
	printf("%23s %012lx %012lx %08lx ", type, p->md_phys,
	    p->md_virt, p->md_pages);
	if (p->md_attr & EFI_MD_ATTR_UC)
		printf("UC ");
	if (p->md_attr & EFI_MD_ATTR_WC)
		printf("WC ");
	if (p->md_attr & EFI_MD_ATTR_WT)
		printf("WT ");
	if (p->md_attr & EFI_MD_ATTR_WB)
		printf("WB ");
	if (p->md_attr & EFI_MD_ATTR_UCE)
		printf("UCE ");
	if (p->md_attr & EFI_MD_ATTR_WP)
		printf("WP ");
	if (p->md_attr & EFI_MD_ATTR_RP)
		printf("RP ");
	if (p->md_attr & EFI_MD_ATTR_XP)
		printf("XP ");
	if (p->md_attr & EFI_MD_ATTR_NV)
		printf("NV ");
	if (p->md_attr & EFI_MD_ATTR_MORE_RELIABLE)
		printf("MORE_RELIABLE ");
	if (p->md_attr & EFI_MD_ATTR_RO)
		printf("RO ");
	if (p->md_attr & EFI_MD_ATTR_RT)
		printf("RUNTIME");
	printf("\n");
}

static bool
do_memory_from_fdt(int fd)
{
	struct stat sb;
	char *buf = NULL;
	int len, offset, fd2 = -1;
	uint32_t sz, ver, esz, efisz;
	uint64_t mmap_pa;
	const uint32_t *u32p;
	const uint64_t *u64p;
	struct efi_map_header *efihdr;
	struct efi_md *map;

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
	efi_systbl_phys = fdt64_to_cpu(*u64p);
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

	/*
	 * We may have no ability to read the PA that this map is in, so pass
	 * the address to FreeBSD via a rather odd flag entry as the first map
	 * so early boot can copy the memory map into this space and have the
	 * rest of the code cope.
	 */
	efisz = (sizeof(*efihdr) + 0xf) & ~0xf;
	buf = malloc(sz + efisz);
	if (buf == NULL)
		return false;
	efihdr = (struct efi_map_header *)buf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);
	bzero(map, sz);
	efihdr->memory_size = sz;
	efihdr->descriptor_size = esz;
	efihdr->descriptor_version = ver;

	/*
	 * Save EFI table. Either this will be an empty table filled in by the trampoline,
	 * or we'll read it below. Either way, set these two variables so we share the best
	 * UEFI memory map with the kernel.
	 */
	efi_map_hdr = efihdr;
	efi_map_size = sz + efisz;

	/*
	 * Try to read in the actual UEFI map.
	 */
	fd2 = open("host:/dev/mem", O_RDONLY);
	if (fd2 < 0) {
		printf("Will read UEFI mem map in tramp: no /dev/mem, need CONFIG_DEVMEM=y\n");
		goto no_read;
	}
	if (lseek(fd2, mmap_pa, SEEK_SET) < 0) {
		printf("Will read UEFI mem map in tramp: lseek failed\n");
		goto no_read;
	}
	len = read(fd2, map, sz);
	if (len != sz) {
		if (len < 0 && errno == EPERM)
			printf("Will read UEFI mem map in tramp: kernel needs CONFIG_STRICT_DEVMEM=n\n");
		else
			printf("Will read UEFI mem map in tramp: lean = %d errno = %d\n", len, errno);
		goto no_read;
	}
	printf("Read UEFI mem map from physmem\n");
	efi_map_phys_src = 0; /* Mark MODINFOMD_EFI_MAP as valid */
	close(fd2);
	printf("UEFI MAP:\n");
	printf("%23s %12s %12s %8s %4s\n",
	    "Type", "Physical", "Virtual", "#Pages", "Attr");
	foreach_efi_map_entry(efihdr, print_efi_map_entry, NULL);
	return true;	/* OK, we really have the memory map */

no_read:
	efi_map_phys_src = mmap_pa;
	close(fd2);
	return true;	/* We can get it the trampoline */

errout:
	free(buf);
	return false;
}

bool
enumerate_memory_arch(void)
{
	int fd = -1;
	bool rv = false;

	fd = open("host:/sys/firmware/fdt", O_RDONLY);
	if (fd != -1) {
		rv = do_memory_from_fdt(fd);
		close(fd);
		/*
		 * So, we have physaddr to the memory table. However, we can't
		 * open /dev/mem on some platforms to get the actual table. So
		 * we have to fall through to get it from /proc/iomem.
		 */
	}
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

	s = first_avail(KERN_ALIGN, HOLE_SIZE, SYSTEM_RAM);
	if (s != 0)
		return (s);
	s = 0x40000000 | 0x4200000;	/* should never get here */
	printf("Falling back to crazy address %#lx\n", s);
	return (s);
}

void
bi_loadsmap(struct preloaded_file *kfp)
{

	/*
	 * Make a note of a systbl. This is nearly mandatory on AARCH64.
	 */
	if (efi_systbl_phys)
		file_addmetadata(kfp, MODINFOMD_FW_HANDLE, sizeof(efi_systbl_phys), &efi_systbl_phys);

	/*
	 * If we have efi_map_hdr, then it's a pointer to the PA where this
	 * memory map lives. The trampoline code will copy it over. If we don't
	 * have it, we use whatever we found in /proc/iomap.
	 */
	if (efi_map_hdr != NULL) {
		file_addmetadata(kfp, MODINFOMD_EFI_MAP, efi_map_size, efi_map_hdr);
		return;
	}
	panic("Can't get UEFI memory map, nor a pointer to it, can't proceed.\n");
}
