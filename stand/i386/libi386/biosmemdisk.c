/*-
 * Copyright (c) 2020 Richard Russo <russor@ruka.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Source of information: https://repo.or.cz/syslinux.git
 *
 * Implements the MEMDISK protocol from syslinux, found in doc/memdisk.txt
 * (search MEMDISK info structure). Since we validate the pointer to the mBFT, a
 * minimum version of 3.85 is needed. Note: All this could be done in the
 * kernel, since we don't have hooks to use this inside the boot loader. The
 * details of these structures can be found in memdisk/memdisk.inc (search
 * for mBFT).
 *
 * The kernel could just grab the mBFT table, but instead relies on us finding
 * it and setting the right env variables.
 */
#include <stand.h>
#include <machine/stdarg.h>
#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#include "platform/acfreebsd.h"
#include "acconfig.h"
#define ACPI_SYSTEM_XFACE
#include "actypes.h"
#include "actbl.h"

struct memdisk_info {
	uint32_t mdi_13h_hook_ptr;	/* not included in mdi_length! */
	uint16_t mdi_length;
	uint8_t  mdi_minor;
	uint8_t  mdi_major;
	uint32_t mdi_disk_ptr;
	uint32_t mdi_disk_sectors;
	uint32_t mdi_far_ptr_cmdline;
	uint32_t mdi_old_int13h;
	uint32_t mdi_old_int15h;
	uint16_t mdi_dos_mem_before;
	uint8_t  mdi_boot_loader_id;
	uint8_t  mdi_sector_size;	/* Code below assumes this is last */
} __attribute__((packed));

struct safe_13h_hook {
	char sh_jmp[3];
	char sh_id[8];
	char sh_vendor[8];
	uint16_t sh_next_offset;
	uint16_t sh_next_segment;
	uint32_t sh_flags;
	uint32_t sh_mbft;
} __attribute__((packed));

/*
 * Maximum length of INT 13 entries we'll chase. Real disks are on this list,
 * potentially, so we may have to look through them to find the memdisk.
 */
#define MEMDISK_MAX	32

/*
 * Scan for MEMDISK virtual block devices
 */
void
biosmemdisk_detect(void)
{
	char line[80], scratch[80];
	int hook = 0, count = 0, sector_size;
	uint16_t segment, offset;
	struct safe_13h_hook *probe;
	ACPI_TABLE_HEADER *mbft;
	uint8_t *cp, sum;
	struct memdisk_info *mdi;

	/*
	 * Walk through the int13 handler linked list, looking for possible
	 * MEMDISKs.
	 *
	 * The max is arbitrary to ensure termination.
	 */
	offset = *(uint16_t *)PTOV(0x13 * 4);
	segment = *(uint16_t *)PTOV(0x13 * 4 + 2);
	while (hook < MEMDISK_MAX && !(segment == 0 && offset == 0)) {
		/*
		 * Walk the linked list, making sure each node has the right
		 * signature and only looking at MEMDISK nodes.
		 */
		probe = (struct safe_13h_hook *)PTOV(segment * 16 + offset);
		if (memcmp(probe->sh_id, "$INT13SF", sizeof(probe->sh_id)) != 0) {
			printf("Found int 13h unsafe hook at %p (%x:%x)\n",
			    probe, segment, offset);
			break;
		}
		if (memcmp(probe->sh_vendor, "MEMDISK ", sizeof(probe->sh_vendor)) != 0)
			goto end_of_loop;

		/*
		 * If it is a memdisk, make sure the mBFT signature is correct
		 * and its checksum is right.
		 */
		mbft = (ACPI_TABLE_HEADER *)PTOV(probe->sh_mbft);
		if (memcmp(mbft->Signature, "mBFT", sizeof(mbft->Signature)) != 0)
			goto end_of_loop;
		sum = 0;
		cp = (uint8_t *)mbft;
		for (int idx = 0; idx < mbft->Length; ++idx)
			sum += *(cp + idx);
		if (sum != 0)
			goto end_of_loop;

		/*
		 * The memdisk info follows the ACPI_TABLE_HEADER in the mBFT
		 * section. If the sector size is present and non-zero use it
		 * otherwise assume 512.
		 */
		mdi = (struct memdisk_info *)PTOV(probe->sh_mbft + sizeof(*mbft));
		sector_size = 512;
		if (mdi->mdi_length + sizeof(mdi->mdi_13h_hook_ptr) >= sizeof(*mdi) &&
		    mdi->mdi_sector_size != 0)
			sector_size = 1 << mdi->mdi_sector_size;

		printf("memdisk %d.%d disk at %#x (%d sectors = %d bytes)\n",
		    mdi->mdi_major, mdi->mdi_minor, mdi->mdi_disk_ptr,
		    mdi->mdi_disk_sectors, mdi->mdi_disk_sectors * sector_size);

		snprintf(line, sizeof(line), "hint.md.%d.physaddr", count);
		snprintf(scratch, sizeof(scratch), "0x%08x", mdi->mdi_disk_ptr);
		setenv(line, scratch, 1);
		snprintf(line, sizeof(line), "hint.md.%d.len", count);
		snprintf(scratch, sizeof(scratch), "%d", mdi->mdi_disk_sectors * sector_size);
		setenv(line, scratch, 1);
		count++;
end_of_loop:
		hook++;
		offset = probe->sh_next_offset;
		segment = probe->sh_next_segment;
	}
}
