/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:35:49  rpd
 * $FreeBSD$
 */

/*
 * Ported to PC-9801 by Yoshio Kimura
 */

/*
 * 93/10/08  bde
 *	If there is no 386BSD partition, initialize the label sector with
 *	LABELSECTOR instead of with garbage.
 *
 * 93/08/22  bde
 *	Fixed reading of bad sector table.  It is at the end of the 'c'
 *	partition, which is not always at the end of the disk.
 */

#include "boot.h"
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/diskpc98.h>

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS		512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	(((di)>>8)&0xff)


static int spt, spc;

struct fs *fs;
struct inode inode;
int dosdev, unit, slice, part, maj, boff;

/*#define EMBEDDED_DISKLABEL 1*/

/* Read ahead buffer large enough for one track on a 1440K floppy.  For
 * reading from floppies, the bootstrap has to be loaded on a 64K boundary
 * to ensure that this buffer doesn't cross a 64K DMA boundary.
 */
#define RA_SECTORS	18
static char ra_buf[RA_SECTORS * BPS];
static int ra_dev;
static int ra_end;
static int ra_first;

static char *Bread(int dosdev, int sector);

int
devopen(void)
{
	struct dos_partition *dptr;
	struct disklabel *dl;
	char *p;
	int i, sector = 0, di, dosdev_copy;

	dosdev_copy = dosdev;
	di = get_diskinfo(dosdev_copy);
	spc = (spt = SPT(di)) * HEADS(di);

#ifndef RAWBOOT
	if ((dosdev_copy & 0xf0) == 0x90)
	{
		boff = 0;
		part = (spt == 15 ? 0 : 1);
	}
	else
	{
#ifdef	EMBEDDED_DISKLABEL
		dl = &disklabel;
#else	/* EMBEDDED_DISKLABEL */
#ifdef PC98
		p = Bread(dosdev_copy, 1);
		dptr = (struct dos_partition *)p;
		slice = WHOLE_DISK_SLICE;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_mid == DOSPTYP_386BSD) {
				slice = BASE_SLICE + i;
				sector = dptr->dp_scyl * spc;
				break;
			}
		p = Bread(dosdev, sector + LABELSECTOR);
		dl=((struct disklabel *)p);
		disklabel = *dl;	/* structure copy (maybe useful later)*/
#else
		p = Bread(dosdev_copy, 0);
		dptr = (struct dos_partition *)(p+DOSPARTOFF);
		slice = WHOLE_DISK_SLICE;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_typ == DOSPTYP_386BSD) {
				slice = BASE_SLICE + i;
				sector = dptr->dp_start;
				break;
			}
		p = Bread(dosdev_copy, sector + LABELSECTOR);
		dl=((struct disklabel *)p);
		disklabel = *dl;	/* structure copy (maybe useful later)*/
#endif /* PC98 */
#endif /* EMBEDDED_DISKLABEL */
		if (dl->d_magic != DISKMAGIC) {
			printf("bad disklabel\n");
			return 1;
		}
		if( (maj == 4) || (maj == 0) || (maj == 1))
		{
			if (dl->d_type == DTYPE_SCSI)
			{
				maj = 4; /* use scsi as boot dev */
			}
			else
			{
				maj = 0; /* must be ESDI/IDE */
			}
		}
		/* This little trick is for OnTrack DiskManager disks */
		boff = dl->d_partitions[part].p_offset -
			dl->d_partitions[2].p_offset + sector;

#ifndef PC98
		bsize = dl->d_partitions[part].p_size;
		if (bsize == 0) {
			printf("empty partition\n");
			return 1;
		}

#endif

	}
#endif /* RAWBOOT */
	return 0;
}


/*
 * Be aware that cnt is rounded up to N*BPS
 */
void
devread(char *iodest, int sector, int cnt)
{
	int offset;
	char *p;
	int dosdev_copy;

	for (offset = 0; offset < cnt; offset += BPS)
	{
		dosdev_copy = dosdev;
		p = Bread(dosdev_copy, sector++);
		bcopy(p, iodest+offset, BPS);
	}
}


static char *
Bread(int dosdev, int sector)
{
	if (dosdev != ra_dev || sector < ra_first || sector >= ra_end)
	{
		int cyl, head, sec, nsec;

		cyl = sector/spc;
#ifndef PC98
		if (cyl > 1023) {
			printf("Error: C:%d > 1023 (BIOS limit)\n", cyl);
			for(;;);        /* loop forever */
		}
#endif
		head = (sector % spc) / spt;
		sec = sector % spt;
		nsec = spt - sec;
		if (nsec > RA_SECTORS)
			nsec = RA_SECTORS;
		twiddle();
		if (biosread(dosdev, cyl, head, sec, nsec, ra_buf) != 0)
		{
		    nsec = 1;
		    twiddle();
		    while (biosread(dosdev, cyl, head, sec, nsec, ra_buf) != 0) {
			printf("Error: D:0x%x C:%d H:%d S:%d\n",
			       dosdev, cyl, head, sec);
			twiddle();
		    }
		}
		ra_dev = dosdev;
		ra_first = sector;
		ra_end = sector + nsec;
	}
	return (ra_buf + (sector - ra_first) * BPS);
}
