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
 *	$Id: disk.c,v 1.1.1.1 1996/06/14 10:04:37 asami Exp $
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
#ifdef DO_BAD144
#include <sys/dkbad.h>
#endif DO_BAD144
#include <sys/disklabel.h>
#include <sys/diskslice.h>

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS		512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	(((di)>>8)&0xff)

#ifdef DO_BAD144
struct dkbad dkb;
int do_bad144;
#endif DO_BAD144
int bsize;

int spt, spc;

char *iodest;
struct fs *fs;
struct inode inode;
int dosdev, unit, slice, part, maj, boff, poff, bnum, cnt;

/*#define EMBEDDED_DISKLABEL 1*/

#define I_ADDR		((void *) 0)	/* XXX where all reads go */

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
	int dosdev = inode.i_dev;
	int i, sector = 0, di;
#if 0   /* Save space, already have hard error for cyl > 1023 in Bread */
	u_long bend;
#endif

	di = get_diskinfo(dosdev);
	spc = (spt = SPT(di)) * HEADS(di);
	if ((dosdev & 0xf0) == 0x90)
	{
		boff = 0;
		part = (spt == 15 ? 0 : 1);
	}
	else
	{
#ifdef	EMBEDDED_DISKLABEL
		dl = &disklabel;
#else	EMBEDDED_DISKLABEL
#ifdef PC98
		Bread(dosdev, 1);
		dptr = (struct dos_partition *)0;
		slice = WHOLE_DISK_SLICE;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_mid == DOSPTYP_386BSD) {
				slice = BASE_SLICE + i;
				sector = dptr->dp_scyl * spc;
				break;
			}
		Bread(dosdev, sector + LABELSECTOR);
		dl=((struct disklabel *)0);
		disklabel = *dl;	/* structure copy (maybe useful later)*/
#else
		Bread(dosdev, 0);
		dptr = (struct dos_partition *)(((char *)0)+DOSPARTOFF);
		slice = WHOLE_DISK_SLICE;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_typ == DOSPTYP_386BSD) {
				slice = BASE_SLICE + i;
				sector = dptr->dp_start;
				break;
			}
		Bread(dosdev, sector + LABELSECTOR);
		dl=((struct disklabel *)0);
		disklabel = *dl;	/* structure copy (maybe useful later)*/
#endif /* PC98 */
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

#ifndef PC98
		/* This is a good idea for all disks */
		bsize = dl->d_partitions[part].p_size;
#if 0   /* Save space, already have hard error for cyl > 1023 in Bread */
		bend = boff + bsize - 1 ;
		if (bend / spc >= 1024) {
			printf("boot partition end >= cyl 1024, BIOS can't load kernel stored beyond this limit\n");
#endif
#endif

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
			Bread(dosdev, dkbbnum + i);
			dkbptr = (struct dkbad *) 0;
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
#endif DO_BAD144
	}
	return 0;
}

void
devread(void)
{
	int offset, sector = bnum;
	int dosdev = inode.i_dev;
	for (offset = 0; offset < cnt; offset += BPS)
	{
		Bread(dosdev, badsect(dosdev, sector++));
		bcopy(0, iodest+offset, BPS);
	}
}

void
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
			printf("Error: C:%d H:%d S:%d\n", cyl, head, sec);
			twiddle();
		    }
		}
		ra_dev = dosdev;
		ra_first = sector;
		ra_end = sector + nsec;
	}
	bcopy(ra_buf + (sector - ra_first) * BPS, I_ADDR, BPS);
}

int
badsect(int dosdev, int sector)
{
	int i;
#ifdef DO_BAD144
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
#endif DO_BAD144
  no_remap:
	return sector;
}
