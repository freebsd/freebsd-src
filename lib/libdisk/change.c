/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: change.c,v 1.6 1995/05/24 08:59:36 jkh Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include "libdisk.h"

struct disk *
Set_Phys_Geom(struct disk *disk, u_long cyl, u_long hd, u_long sect)
{
	struct disk *d = Int_Open_Disk(disk->name,cyl*hd*sect);
	d->real_cyl = cyl;
	d->real_hd = hd;
	d->real_sect = sect;
	d->bios_cyl = disk->bios_cyl;
	d->bios_hd = disk->bios_hd;
	d->bios_sect = disk->bios_sect;
	d->flags = disk->flags;
	Free_Disk(disk);
	return d;
}

void
Set_Bios_Geom(struct disk *disk, u_long cyl, u_long hd, u_long sect)
{
	disk->bios_cyl = cyl;
	disk->bios_hd = hd;
	disk->bios_sect = sect;
	Bios_Limit_Chunk(disk->chunks,1024*hd*sect);
}

void
All_FreeBSD(struct disk *d)
{
	struct chunk *c;

    again:	
	for (c=d->chunks->part;c;c=c->next)
		if (c->type != unused) {
			Delete_Chunk(d,c);
			goto again;
		}
	c=d->chunks;
	Create_Chunk(d,c->offset,c->size,freebsd,0xa5,0);
}
