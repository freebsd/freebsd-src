/*
 * Ported to boot 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
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
 */

/*
 * HISTORY
 * $Log:	disk.c,v $
 * Revision 2.2  92/04/04  11:35:49  rpd
 * 	Fabricated from 3.0 bootstrap and 2.5 boot disk.c:
 * 	with support for scsi
 * 	[92/03/30            mg32]
 * 
 */

/*
 * 9/20/92
 * Peng-Toh Sim.  sim@cory.berkeley.edu
 * Added bad144 support under 386bsd for wd's
 * So, bad block remapping is done when loading the kernel.
 */

#include "boot.h"
#ifdef DO_BAD144
#include <sys/dkbad.h>
#endif DO_BAD144
#include <sys/disklabel.h>

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS		512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)

char *devs[] = {"wd", "hd", "fd", "wt", "sd", 0};

#ifdef DO_BAD144
struct dkbad dkb;
int do_bad144;
int bsize;
#endif DO_BAD144

int spt, spc;

char *iodest;
struct fs *fs;
struct inode inode;
int dosdev,  unit, part, maj, boff, poff, bnum, cnt;

/*#define EMBEDDED_DISKLABEL 1*/
extern struct disklabel disklabel;
/*struct	disklabel disklabel;*/

devopen()
{
	struct dos_partition *dptr;
	struct disklabel *dl;
	int dosdev = inode.i_dev;
	int i, sector, di;
	
	di = get_diskinfo(dosdev);
	spc = (spt = SPT(di)) * HEADS(di);
	if (dosdev == 2)
	{
		boff = 0;
		part = (spt == 15 ? 3 : 1);
	}
	else
	{
#ifdef	EMBEDDED_DISKLABEL
		dl = &disklabel;
#else	EMBEDDED_DISKLABEL
		Bread(dosdev, 0);
		dptr = (struct dos_partition *)(((char *)0)+DOSPARTOFF);
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_typ == DOSPTYP_386BSD)
				break;
		sector = dptr->dp_start + LABELSECTOR;
		Bread(dosdev, sector++);
		dl=((struct disklabel *)0);
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
		boff = dl->d_partitions[part].p_offset;
#ifdef DO_BAD144
		bsize = dl->d_partitions[part].p_size;
		do_bad144 = 0;
		if (dl->d_flags & D_BADSECT) {
		    /* this disk uses bad144 */
		    int i;
		    int dkbbnum;
		    struct dkbad *dkbptr;

		    /* find the first readable bad144 sector */
		    /* some of this code is copied from ufs/disk_subr.c */
		    /* read a bad sector table */
		    dkbbnum = dl->d_secperunit - dl->d_nsectors;
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
		      printf("Bad badsect table\n");
		    else
		      printf("Using bad144 bad sector at %d\n", dkbbnum+i);
		}
#endif DO_BAD144
	}
	return 0;
}

devread()
{
	int offset, sector = bnum;
	int dosdev = inode.i_dev;
	for (offset = 0; offset < cnt; offset += BPS)
	{
		Bread(dosdev, badsect(dosdev, sector++));
		bcopy(0, iodest+offset, BPS);
	}
}

Bread(dosdev,sector)
     int dosdev,sector;
{
	int cyl = sector/spc, head = (sector%spc)/spt, secnum = sector%spt;
	while (biosread(dosdev, cyl,head,secnum))
	{
		printf("Error: C:%d H:%d S:%d\n",cyl,head,secnum);
	}
}

badsect(dosdev, sector)
     int dosdev, sector;
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

	    cyl = sector / dl->d_secpercyl;
	    head = (sector % dl->d_secpercyl) / dl->d_nsectors;
	    sec = sector % dl->d_nsectors;
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
	    newsec = dl->d_secperunit - dl->d_nsectors - i -1;
	    return newsec;
	}
#endif DO_BAD144
      no_remap:
	return sector;
}
