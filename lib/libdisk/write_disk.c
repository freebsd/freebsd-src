/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <paths.h>
#include "libdisk.h"

void
Fill_Disklabel(struct disklabel *dl, const struct disk *new,
    const struct chunk *c1)
{
	struct chunk *c2;
	int j;

	memset(dl, 0, sizeof *dl);

	for (c2 = c1->part; c2; c2 = c2->next) {
		if (c2->type == unused)
			continue;
		if (!strcmp(c2->name, "X"))
			continue;
		j = c2->name[strlen(c2->name) - 1] - 'a';
		if (j < 0 || j >= MAXPARTITIONS || j == RAW_PART)
			continue;
		dl->d_partitions[j].p_size = c2->size;
		dl->d_partitions[j].p_offset = c2->offset;
		dl->d_partitions[j].p_fstype = c2->subtype;
	}

	dl->d_bbsize = BBSIZE;
	/*
	 * Add in defaults for superblock size, interleave, and rpms
	 */
	dl->d_sbsize = 0;

	strcpy(dl->d_typename, c1->name);

	dl->d_secsize = 512;
	dl->d_secperunit = new->chunks->size;
	dl->d_ncylinders = new->bios_cyl;
	dl->d_ntracks = new->bios_hd;
	dl->d_nsectors = new->bios_sect;
	dl->d_secpercyl = dl->d_ntracks * dl->d_nsectors;

	dl->d_npartitions = MAXPARTITIONS;

	dl->d_type = new->name[0] == 's' || new->name[0] == 'd' ||
	    new->name[0] == 'o' ? DTYPE_SCSI : DTYPE_ESDI;
	dl->d_partitions[RAW_PART].p_size = c1->size;
	dl->d_partitions[RAW_PART].p_offset = c1->offset;
	dl->d_rpm = 3600;
	dl->d_interleave = 1;

	dl->d_magic = DISKMAGIC;
	dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);
}
