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
 *	$Id: disk.c,v 1.19 1996/09/11 19:23:10 phk Exp $
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
#ifdef DO_BAD144
#include <sys/dkbad.h>
#endif DO_BAD144
#include <sys/disklabel.h>
#include <sys/diskslice.h>

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS		512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)

#ifdef DO_BAD144
struct dkbad dkb;
int do_bad144;
#endif DO_BAD144
int bsize;

int spt, spc;

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


int
devopen(void)
{
	struct dos_partition *dptr;
	struct disklabel *dl;
	char *p;
	int i, sector = 0, di, dosdev_copy;

	dosdev_copy = dosdev;
	di = get_diskinfo(dosdev_copy);
	spt = SPT(di);

	/* Hack for 2.88MB floppy drives. */
	if (!(dosdev_copy & 0x80) && spt == 36)
		spt = 18;

	spc = spt * HEADS(di);

#ifndef RAWBOOT
	{
#ifdef	EMBEDDED_DISKLABEL
		dl = &disklabel;
#else	EMBEDDED_DISKLABEL
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
#endif	EMBEDDED_DISKLABEL
		if (dl->d_magic != DISKMAGIC) {
			printf("bad disklabel");
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

		/* This is a good idea for all disks */
		bsize = dl->d_partitions[part].p_size;
#ifdef DO_BAD144
		do_bad144 = 0;
		if (dl->d_flags & D_BADSECT) {
		    /* this disk uses bad144 */
		    int i;
		    int dkbbnum;
		    struct dkbad *dkbptr;

		    /* find the first readable bad sector table */
		    /* some of this code is copied from ufs/ufs_disksubr.c */
		    /* including the bugs :-( */
		    /* read a bad sector table */

#define BAD144_PART	2	/* XXX scattered magic numbers */
#define BSD_PART	0	/* XXX should be 2 but bad144.c uses 0 */
		    if (dl->d_partitions[BSD_PART].p_offset != 0)
			    dkbbnum = dl->d_partitions[BAD144_PART].p_offset
				      + dl->d_partitions[BAD144_PART].p_size;
		    else
			    dkbbnum = dl->d_secperunit;
		    dkbbnum -= dl->d_nsectors;

		    if (dl->d_secsize > DEV_BSIZE)
		      dkbbnum *= dl->d_secsize / DEV_BSIZE;
		    else
		      dkbbnum /= DEV_BSIZE / dl->d_secsize;
		    i = 0;
		    do_bad144 = 0;
		    do {
			/* XXX: what if the "DOS sector" < 512 bytes ??? */
			p = Bread(dosdev_copy, dkbbnum + i);
			dkbptr = (struct dkbad *) p;
/* XXX why is this not in <sys/dkbad.h> ??? */
#define DKBAD_MAGIC 0x4321
			if (dkbptr->bt_mbz == 0 &&
			        dkbptr->bt_flag == DKBAD_MAGIC) {
			    dkb = *dkbptr;	/* structure copy */
			    do_bad144 = 1;
			    break;
			}
			i += 2;
		    } while (i < 10 && i < dl->d_nsectors);
		    if (!do_bad144)
		      printf("Bad bad sector table\n");
		    else
		      printf("Using bad sector table at %d\n", dkbbnum+i);
		}
#endif /* DO_BAD144 */
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
		p = Bread(dosdev_copy, badsect(dosdev_copy, sector++));
		bcopy(p, iodest+offset, BPS);
	}
}


char *
Bread(int dosdev, int sector)
{
	if (dosdev != ra_dev || sector < ra_first || sector >= ra_end)
	{
		int cyl, head, sec, nsec;

		cyl = sector/spc;
		if (cyl > 1023) {
			printf("Error: C:%d > 1023 (BIOS limit)\n", cyl);
			for(;;);        /* loop forever */
		}
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
			printf("Error: C:%d H:%d S:%d\n", cyl, head, sec);
			twiddle();
		    }
		}
		ra_dev = dosdev;
		ra_first = sector;
		ra_end = sector + nsec;
	}
	return (ra_buf + (sector - ra_first) * BPS);
}

int
badsect(int dosdev, int sector)
{
#if defined(DO_BAD144) && !defined(RAWBOOT)
	int i;
	if (do_bad144) {
		u_short cyl;
		u_short head;
		u_short sec;
		int newsec;
		struct disklabel *dl = &disklabel;

		/* XXX */
		/* from wd.c */
		/* bt_cyl = cylinder number in sorted order */
		/* bt_trksec is actually (head << 8) + sec */

		/* only remap sectors in the partition */
		if (sector < boff || sector >= boff + bsize) {
			goto no_remap;
		}

		cyl = (sector-boff) / dl->d_secpercyl;
		head = ((sector-boff) % dl->d_secpercyl) / dl->d_nsectors;
		sec = (sector-boff) % dl->d_nsectors;
		sec = (head<<8) + sec;

		/* now, look in the table for a possible bad sector */
		for (i=0; i<126; i++) {
			if (dkb.bt_bad[i].bt_cyl == cyl) {
				/* found same cylinder */
				if (dkb.bt_bad[i].bt_trksec == sec) {
					/* FOUND! */
					break;
				}
			} else if (dkb.bt_bad[i].bt_cyl > cyl) {
				i = 126;
				break;
			}
		}
		if (i == 126) {
			/* didn't find bad sector */
			goto no_remap;
		}
		/* otherwise find replacement sector */
		if (dl->d_partitions[BSD_PART].p_offset != 0)
			newsec = dl->d_partitions[BAD144_PART].p_offset
				+ dl->d_partitions[BAD144_PART].p_size;
		else
			newsec = dl->d_secperunit;
		newsec -= dl->d_nsectors + i + 1;
		return newsec;
	}
  no_remap:
#endif 
	return sector;
}
