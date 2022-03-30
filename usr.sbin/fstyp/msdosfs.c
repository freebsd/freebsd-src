/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2006 Tobias Reifenberger
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fstyp.h"
#include "msdosfs.h"

#define LABEL_NO_NAME		"NO NAME    "

/*
 * XXX the signature 0x55 0xAA as the last two bytes of 512 is not required
 * by specifications, but was historically required by fstyp.  This check
 * should be removed, with a more comprehensive BPB validation instead.
 */
static bool
check_signature(uint8_t sector0[512])
{
	/* Check for the FAT boot sector signature. */
	if (sector0[510] == 0x55 && sector0[511] == 0xaa)
		return (true);
	/* Special case for Raspberry Pi Pico bootloader. */
	if (sector0[510] == 0 && sector0[511] == 0 &&
	    sector0[0] == 0xeb && sector0[1] == 0x3c && sector0[2] == 0x90)
		return (true);
	return (false);
}

int
fstyp_msdosfs(FILE *fp, char *label, size_t size)
{
	FAT_BSBPB *pfat_bsbpb;
	FAT32_BSBPB *pfat32_bsbpb;
	FAT_DES *pfat_entry;
	uint8_t *sector0, *sector;
	size_t copysize;

	sector0 = NULL;
	sector = NULL;

	/* Load 1st sector with boot sector and boot parameter block. */
	sector0 = (uint8_t *)read_buf(fp, 0, 512);
	if (sector0 == NULL)
		return (1);

	if (!check_signature(sector0)) {
		goto error;
	}

	/*
	 * Test if this is really a FAT volume and determine the FAT type.
	 */

	pfat_bsbpb = (FAT_BSBPB *)sector0;
	pfat32_bsbpb = (FAT32_BSBPB *)sector0;

	if (UINT16BYTES(pfat_bsbpb->BPB_FATSz16) != 0) {
		/*
		 * If the BPB_FATSz16 field is not zero and the string "FAT" is
		 * at the right place, this should be a FAT12 or FAT16 volume.
		 */
		if (strncmp(pfat_bsbpb->BS_FilSysType, "FAT", 3) != 0) {
			goto error;
		}

		/* A volume with no name should have "NO NAME    " as label. */
		if (strncmp(pfat_bsbpb->BS_VolLab, LABEL_NO_NAME,
		    sizeof(pfat_bsbpb->BS_VolLab)) == 0) {
			goto endofchecks;
		}
		copysize = MIN(size - 1, sizeof(pfat_bsbpb->BS_VolLab));
		memcpy(label, pfat_bsbpb->BS_VolLab, copysize);
		label[copysize] = '\0';
	} else if (UINT32BYTES(pfat32_bsbpb->BPB_FATSz32) != 0) {
		uint32_t fat_FirstDataSector, fat_BytesPerSector, offset;

		/*
		 * If the BPB_FATSz32 field is not zero and the string "FAT" is
		 * at the right place, this should be a FAT32 volume.
		 */
		if (strncmp(pfat32_bsbpb->BS_FilSysType, "FAT", 3) != 0) {
			goto error;
		}

		/*
		 * If the volume label is not "NO NAME    " we're done.
		 */
		if (strncmp(pfat32_bsbpb->BS_VolLab, LABEL_NO_NAME,
		    sizeof(pfat32_bsbpb->BS_VolLab)) != 0) {
			copysize = MIN(size - 1,
			    sizeof(pfat32_bsbpb->BS_VolLab));
			memcpy(label, pfat32_bsbpb->BS_VolLab, copysize);
			label[copysize] = '\0';
			goto endofchecks;
		}

		/*
		 * If the volume label "NO NAME    " is in the boot sector, the
		 * label of FAT32 volumes may be stored as a special entry in
		 * the root directory.
		 */
		fat_FirstDataSector =
		    UINT16BYTES(pfat32_bsbpb->BPB_RsvdSecCnt) +
		    (pfat32_bsbpb->BPB_NumFATs *
		     UINT32BYTES(pfat32_bsbpb->BPB_FATSz32));
		fat_BytesPerSector = UINT16BYTES(pfat32_bsbpb->BPB_BytsPerSec);

		//    fat_FirstDataSector, fat_BytesPerSector);

		for (offset = fat_BytesPerSector * fat_FirstDataSector;;
		    offset += fat_BytesPerSector) {
			sector = (uint8_t *)read_buf(fp, offset, fat_BytesPerSector);
			if (sector == NULL)
				goto error;

			pfat_entry = (FAT_DES *)sector;
			do {
				/* No more entries available. */
				if (pfat_entry->DIR_Name[0] == 0) {
					goto endofchecks;
				}

				/* Skip empty or long name entries. */
				if (pfat_entry->DIR_Name[0] == 0xe5 ||
				    (pfat_entry->DIR_Attr &
				     FAT_DES_ATTR_LONG_NAME) ==
				    FAT_DES_ATTR_LONG_NAME) {
					continue;
				}

				/*
				 * The name of the entry is the volume label if
				 * ATTR_VOLUME_ID is set.
				 */
				if (pfat_entry->DIR_Attr &
				    FAT_DES_ATTR_VOLUME_ID) {
					copysize = MIN(size - 1,
					    sizeof(pfat_entry->DIR_Name));
					memcpy(label, pfat_entry->DIR_Name,
					    copysize);
					label[copysize] = '\0';
					goto endofchecks;
				}
			} while((uint8_t *)(++pfat_entry) <
			    (uint8_t *)(sector + fat_BytesPerSector));
			free(sector);
		}
	} else {
		goto error;
	}

endofchecks:
	rtrim(label, size);

	free(sector0);
	free(sector);

	return (0);

error:
	free(sector0);
	free(sector);

	return (1);
}
