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
 * $FreeBSD: src/sys/i386/boot/dosboot/disk.c,v 1.6 1999/12/08 09:32:49 phk Exp $
 */
#include <stdio.h>
#include <memory.h>

#define bcopy(a,b,c)	memcpy(b,a,c)

#include "boot.h"
#include "disklabe.h"
#include "diskslic.h"

#define	BIOS_DEV_FLOPPY	0x0
#define	BIOS_DEV_WIN	0x80

#define BPS			512
#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)

static char i_buf[BPS];
#define I_ADDR		((void *) i_buf)	/* XXX where all reads go */


static int spt, spc;

char *iodest;
struct fs *fs;
struct inode inode;
long dosdev, slice, unit, part, maj, boff, poff, bnum, cnt;

extern int biosread(int dev, int track, int head, int sector, int cnt, unsigned char far *buffer);

struct	disklabel disklabel;

static void Bread(int dosdev, long sector);

unsigned long get_diskinfo(int drive)
{
	char dr = (char) drive;
	unsigned long rt;

	_asm {
		mov ah,8		; get diskinfo
		mov dl,dr		; drive
		int 13h
		cmp ah,0
		je ok
		;
		; Failure! We assume it's a floppy!
		;
		sub ax,ax
		mov bh,ah
		mov bl,2
		mov ch,79
		mov cl,15
		mov dh,1
		mov dl,1
	ok:
		mov ah,dh
		mov al,cl
		and al,3fh
		mov word ptr rt,ax

		xor bx,bx
		mov bl,cl
		and bl,0c0h
		shl bx,2
		mov bl,ch
		mov word ptr rt+2,bx
	}
	return rt;
}

int devopen(void)
{
	struct dos_partition *dptr;
	struct disklabel *dl;
	int dosdev = (int) inode.i_dev;
	int i;
	long di, sector;
	
	di = get_diskinfo(dosdev);
	spc = (spt = (int)SPT(di)) * (int)HEADS(di);
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
		dptr = (struct dos_partition *)(((char *)I_ADDR)+DOSPARTOFF);
		sector = LABELSECTOR;
		slice = WHOLE_DISK_SLICE;
		for (i = 0; i < NDOSPART; i++, dptr++)
			if (dptr->dp_typ == DOSPTYP_386BSD) {
				slice = BASE_SLICE + i;
				sector = dptr->dp_start + LABELSECTOR;
				break;
			}
		Bread(dosdev, sector++);
		dl=((struct disklabel *)I_ADDR);
		disklabel = *dl;	/* structure copy (maybe useful later)*/
#endif	EMBEDDED_DISKLABEL
		if (dl->d_magic != DISKMAGIC) {
			printf("bad disklabel");
			return 1;
		}

		if( (maj == 4) || (maj == 0) || (maj == 1)) {
			if (dl->d_type == DTYPE_SCSI)
				maj = 4; /* use scsi as boot dev */
			else
				maj = 0; /* must be ESDI/IDE */
		}

		boff = dl->d_partitions[part].p_offset;
	}
	return 0;
}

void devread(void)
{
	long offset, sector = bnum;
	int dosdev = (int) inode.i_dev;
	for (offset = 0; offset < cnt; offset += BPS)
	{
		Bread(dosdev, sector++);
		bcopy(I_ADDR, iodest+offset, BPS);
	}
}

/* Read ahead buffer large enough for one track on a 1440K floppy.  For
 * reading from floppies, the bootstrap has to be loaded on a 64K boundary
 * to ensure that this buffer doesn't cross a 64K DMA boundary.
 */
#define RA_SECTORS	18
static char ra_buf[RA_SECTORS * BPS];
static int ra_dev;
static long ra_end;
static long ra_first;

static void Bread(int dosdev, long sector)
{
	if (dosdev != ra_dev || sector < ra_first || sector >= ra_end)
	{
		int cyl, head, sec, nsec;

		cyl = (int) (sector/(long)spc);
		head = (int) ((sector % (long) spc) / (long) spt);
		sec = (int) (sector % (long) spt);
		nsec = spt - sec;
		if (nsec > RA_SECTORS)
			nsec = RA_SECTORS;
		if (biosread(dosdev, cyl, head, sec, nsec, ra_buf) != 0)
		{
		    nsec = 1;
		    while (biosread(dosdev, cyl, head, sec, nsec, ra_buf) != 0)
				printf("Error: C:%d H:%d S:%d\n", cyl, head, sec);
		}
		ra_dev = dosdev;
		ra_first = sector;
		ra_end = sector + nsec;
	}
	bcopy(ra_buf + (sector - ra_first) * BPS, I_ADDR, BPS);
}
