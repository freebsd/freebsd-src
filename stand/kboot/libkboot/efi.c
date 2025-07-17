/*
 * Copyright (c) 2024 Netflix, Inc
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/linker.h>
#include "stand.h"
#include "bootstrap.h"
#include "efi.h"
#include "seg.h"
#include "util.h"

vm_paddr_t efi_systbl_phys;
struct efi_map_header *efi_map_hdr;
uint32_t efi_map_size;
vm_paddr_t efi_map_phys_src;	/* From DTB */
vm_paddr_t efi_map_phys_dst;	/* From our memory map metadata module */

void
efi_set_systbl(uint64_t tbl)
{
	efi_systbl_phys = tbl;
}
		
#if 0
/* Note: This is useless since runtime-map is a subset */
void
efi_read_from_sysfs(void)
{
	uint32_t efisz, sz, map_size;
	int entries = 0;
	struct efi_md *map;		/* Really an array */
	char *buf;
	struct stat sb;
	char fn[100];

	/*
	 * Count the number of entries we have. They are numbered from 0
	 * through entries - 1.
	 */
	do {
		printf("Looking at index %d\n", entries);
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/phys_addr", entries++);
	} while (stat(fn, &sb) == 0);

	/*
	 * We incremented entries one past the first failure, so we need to
	 * adjust the count and the test for 'nothing found' is against 1.
	 */
	if (entries == 1)
		goto err;
	entries--;

	/* XXX lots of copied code, refactor? */
	map_size = sizeof(struct efi_md) * entries;
	efisz = roundup2(sizeof(*efi_map_hdr), 16);
	sz = efisz + map_size;
	buf = malloc(efisz + map_size);
	if (buf == NULL)
		return;
	efi_map_hdr = (struct efi_map_header *)buf;
	efi_map_size = sz;
	map = (struct efi_md *)(buf + efisz);
	bzero(map, sz);
	efi_map_hdr->memory_size = map_size;
	efi_map_hdr->descriptor_size = sizeof(struct efi_md);
	efi_map_hdr->descriptor_version = EFI_MEMORY_DESCRIPTOR_VERSION;
	for (int i = 0; i < entries; i++) {
		struct efi_md *m;

		printf("Populating index %d\n", i);
		m = map + i;
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/type", i);
		if (!file2u32(fn, &m->md_type))
			goto err;
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/phys_addr", i);
		if (!file2u64(fn, &m->md_phys))
			goto err;
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/virt_addr", i);
		if (!file2u64(fn, &m->md_virt))
			goto err;
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/num_pages", i);
		if (!file2u64(fn, &m->md_pages))
			goto err;
		snprintf(fn, sizeof(fn), "/sys/firmware/efi/runtime-map/%d/attribute", i);
		if (!file2u64(fn, &m->md_attr))
			goto err;
	}
	efi_map_phys_src = 0;
	printf("UEFI MAP:\n");
	print_efi_map(efi_map_hdr);
	printf("DONE\n");
	return;
err:
	printf("Parse error in reading current memory map\n");
}
#endif

/*
 * We may have no ability to read the PA that this map is in, so pass
 * the address to FreeBSD via a rather odd flag entry as the first map
 * so early boot can copy the memory map into this space and have the
 * rest of the code cope.
 */
bool
efi_read_from_pa(uint64_t pa, uint32_t map_size, uint32_t desc_size, uint32_t vers)
{
	uint32_t efisz, sz;
	char *buf;
	int fd2, len;
	struct efi_md *map;		/* Really an array */

	/*
	 * We may have no ability to read the PA that this map is in, so pass
	 * the address to FreeBSD via a rather odd flag entry as the first map
	 * so early boot can copy the memory map into this space and have the
	 * rest of the code cope. We also have to round the size of the header
	 * to 16 byte boundary.
	 */
	efisz = roundup2(sizeof(*efi_map_hdr), 16);
	sz = efisz + map_size;
	buf = malloc(efisz + map_size);
	if (buf == NULL)
		return false;
	efi_map_hdr = (struct efi_map_header *)buf;
	efi_map_size = sz;
	map = (struct efi_md *)(buf + efisz);
	bzero(map, sz);
	efi_map_hdr->memory_size = map_size;
	efi_map_hdr->descriptor_size = desc_size;
	efi_map_hdr->descriptor_version = vers;

	/*
	 * Try to read in the actual UEFI map. This may fail, and that's OK. We just
	 * won't print the map.
	 */
	fd2 = open("host:/dev/mem", O_RDONLY);
	if (fd2 < 0)
		goto no_read;
	if (lseek(fd2, pa, SEEK_SET) < 0)
		goto no_read;
	len = read(fd2, map, sz);
	if (len != sz)
		goto no_read;
	efi_map_phys_src = 0;		/* Mark MODINFOMD_EFI_MAP as valid */
	close(fd2);
	printf("UEFI MAP:\n");
	print_efi_map(efi_map_hdr);
	return (true);

no_read:				/* Just get it the trampoline */
	efi_map_phys_src = pa;
	close(fd2);
	return (true);
}

void
foreach_efi_map_entry(struct efi_map_header *efihdr, efi_map_entry_cb cb, void *argp)
{
	struct efi_md *map, *p;
	size_t efisz;
	int ndesc, i;

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = roundup2(sizeof(struct efi_map_header), 16);
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);

	if (efihdr->descriptor_size == 0)
		return;
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	for (i = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		cb(p, argp);
	}
}

/* XXX REFACTOR WITH KERNEL */
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

void
print_efi_map(struct efi_map_header *efihdr)
{
	printf("%23s %12s %12s %8s %4s\n",
	    "Type", "Physical", "Virtual", "#Pages", "Attr");

	foreach_efi_map_entry(efihdr, print_efi_map_entry, NULL);
}

void
efi_bi_loadsmap(struct preloaded_file *kfp)
{
	/*
	 * Make a note of a systbl. This is nearly mandatory on AARCH64.
	 */
	if (efi_systbl_phys)
		file_addmetadata(kfp, MODINFOMD_FW_HANDLE, sizeof(efi_systbl_phys), &efi_systbl_phys);

	/*
	 * If we have efi_map_hdr, then it's a pointer to the PA where this
	 * memory map lives. The trampoline code will copy it over. If we don't
	 * have it, panic because /proc/iomem isn't sufficient and there's no
	 * hope.
	 */
	if (efi_map_hdr != NULL) {
		file_addmetadata(kfp, MODINFOMD_EFI_MAP, efi_map_size, efi_map_hdr);
		return;
	}

	panic("Can't get UEFI memory map, nor a pointer to it, can't proceed.\n");
}
