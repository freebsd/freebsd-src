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

	if (disk->bios_cyl >= 65536)
		sane = 0;
#ifdef PC98
	if (disk->bios_hd >= 256)
		sane = 0;
	if (disk->bios_sect >= 256)
		sane = 0;
#else
	if (disk->bios_hd > 256)
		sane = 0;
	if (disk->bios_sect > 63)
		sane = 0;
#endif
	if (disk->bios_cyl * disk->bios_hd * disk->bios_sect !=
	    disk->chunks->size)
		sane = 0;
	if (sane)
		return;

	/* First try something that IDE can handle */
	disk->bios_sect = 63;
	disk->bios_hd = 16;
	disk->bios_cyl = disk->chunks->size /
		(disk->bios_sect * disk->bios_hd);

#ifdef PC98
	if (disk->bios_cyl < 65536)
#else
	if (disk->bios_cyl < 1024)
#endif
		return;

	/* Hmm, try harder... */
	/* Assume standard SCSI parameter */
#ifdef PC98
	disk->bios_sect = 128;
	disk->bios_hd = 8;
#else
	disk->bios_hd = 255;
#endif
	disk->bios_cyl = disk->chunks->size /
		(disk->bios_sect * disk->bios_hd);

#ifdef PC98
	if (disk->bios_cyl < 65536)
		return;

	/* Assume UIDE-133/98-A Challenger BIOS 0.9821C parameter */
	disk->bios_sect = 255;
	disk->bios_hd = 16;
	disk->bios_cyl = disk->chunks->size /
		(disk->bios_sect * disk->bios_hd);

	if (disk->bios_cyl < 65536)
		return;

	/* BIG-na-Drive? */
	disk->bios_hd = 255;
	disk->bios_cyl = disk->chunks->size /
		(disk->bios_sect * disk->bios_hd);
#endif
}

void
All_FreeBSD(struct disk *d, int force_all)
{
	struct chunk *c;
	int type;

#ifdef PC98
	type = 0xc494;
#else
	type = 0xa5;
#endif

again:
	for (c = d->chunks->part; c; c = c->next)
		if (c->type != unused) {
			Delete_Chunk(d, c);
			goto again;
		}
	c = d->chunks;
	if (force_all) {
		Sanitize_Bios_Geom(d);
		Create_Chunk(d, c->offset, c->size, freebsd, type,
		    CHUNK_FORCE_ALL, "FreeBSD");
	} else {
		Create_Chunk(d, c->offset, c->size, freebsd, type, 0,
		    "FreeBSD");
	}
}
