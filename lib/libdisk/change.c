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
#include "libdisk.h"

void
Set_Bios_Geom(struct disk *disk, u_long cyl, u_long hd, u_long sect)
{
	disk->bios_cyl = cyl;
	disk->bios_hd = hd;
	disk->bios_sect = sect;
}

void
Sanitize_Bios_Geom(struct disk *disk)
{
	int sane;

	sane = 1;

#ifdef PC98
	if (disk->bios_cyl >= 65536)
#else
	if (disk->bios_cyl > 1024)
#endif
		sane = 0;
	if (disk->bios_hd > 16)
		sane = 0;
#ifdef PC98
	if (disk->bios_sect >= 256)
#else
	if (disk->bios_sect > 63)
#endif
		sane = 0;
	if (disk->bios_cyl*disk->bios_hd*disk->bios_sect != disk->chunks->size)
		sane = 0;
	if (sane)
		return;

	/* First try something that IDE can handle */
	disk->bios_sect = 63;
	disk->bios_hd = 16;
	disk->bios_cyl = disk->chunks->size / (disk->bios_sect*disk->bios_hd);

#ifdef PC98
	if (disk->bios_cyl < 65536)
#else
	if (disk->bios_cyl < 1024)
#endif
		return;

	/* Hmm, try harder... */
#ifdef PC98
	/* Assume standard SCSI parameter */
	disk->bios_sect = 128;
	disk->bios_hd = 8;
#else
	disk->bios_hd = 255;
#endif
	disk->bios_cyl = disk->chunks->size / (disk->bios_sect*disk->bios_hd);

	return;
}

void
All_FreeBSD(struct disk *d, int force_all)
{
	struct chunk *c;

    again:
	for (c = d->chunks->part; c; c = c->next)
		if (c->type != unused) {
			Delete_Chunk(d, c);
			goto again;
		}
	c = d->chunks;
	if (force_all) {
		Sanitize_Bios_Geom(d);
#ifdef PC98
		Create_Chunk(d, c->offset, c->size, freebsd, 0x494,
		    CHUNK_FORCE_ALL, "FreeBSD");
#else
		Create_Chunk(d, c->offset, c->size, freebsd, 0xa5,
		    CHUNK_FORCE_ALL);
#endif
	} else {
#ifdef PC98
		Create_Chunk(d, c->offset, c->size, freebsd, 0x494, 0,
		    "FreeBSD");
#else
		Create_Chunk(d, c->offset, c->size, freebsd, 0xa5, 0);
#endif
	}
}
