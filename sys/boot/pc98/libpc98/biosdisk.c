/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: biosdisk.c,v 1.3 1999/03/04 16:38:12 kato Exp $
 */

/*
 * BIOS disk device handling.
 * 
 * Ideas and algorithms from:
 *
 * - NetBSD libi386/biosdisk.c
 * - FreeBSD biosboot/disk.c
 *
 * XXX Todo: add bad144 support.
 */

#include <stand.h>

#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/reboot.h>

#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)
#define	MAXBDDEV		MAXDEV

#define DT_ATAPI		0x10		/* disk type for ATAPI floppies */
#define WDMAJOR			0		/* major numbers for devices we frontend for */
#define WFDMAJOR		1
#define FDMAJOR			2
#define DAMAJOR			4

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __FUNCTION__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

struct open_disk {
    int			od_dkunit;		/* disk unit number */
    int			od_unit;		/* BIOS unit number */
    int			od_cyl;			/* BIOS geometry */
    int			od_hds;
    int			od_sec;
    int			od_boff;		/* block offset from beginning of BIOS disk */
    int			od_flags;
#define	BD_MODEMASK	0x3
#define BD_MODEINT13	0x0
#define BD_MODEEDD1	0x1
#define BD_MODEEDD3	0x2
#define BD_FLOPPY	(1<<2)
    struct disklabel		od_disklabel;
    struct dos_partition	od_parttab[NDOSPART];	/* XXX needs to grow for extended partitions */
#define BD_LABELOK	(1<<3)
#define BD_PARTTABOK	(1<<4)
};

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bdinfo
{
    int		bd_unit;		/* BIOS unit number */
    int		bd_flags;
    int		bd_type;		/* BIOS 'drive type' (floppy only) */
#ifdef PC98
    int         bd_drive;
#endif
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

static int	bd_getgeom(struct open_disk *od);
static int	bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest);

static int	bd_int13probe(struct bdinfo *bd);

static void	bd_printslice(struct open_disk *od, int offset, char *prefix);

static int	bd_init(void);
static int	bd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int	bd_realstrategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int	bd_open(struct open_file *f, ...);
static int	bd_close(struct open_file *f);
static void	bd_print(int verbose);

struct devsw biosdisk = {
    "disk", 
    DEVT_DISK, 
    bd_init,
    bd_strategy, 
    bd_open, 
    bd_close, 
    noioctl,
    bd_print
};

static int	bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev);
static void	bd_closedisk(struct open_disk *od);
static int	bd_bestslice(struct dos_partition *dptr);

/*
 * Translate between BIOS device numbers and our private unit numbers.
 */
int
bd_bios2unit(int biosdev)
{
    int		i;
    
    DEBUG("looking for bios device 0x%x", biosdev);
    for (i = 0; i < nbdinfo; i++) {
	DEBUG("bd unit %d is BIOS device 0x%x", i, bdinfo[i].bd_unit);
	if (bdinfo[i].bd_unit == biosdev)
	    return(i);
    }
    return(-1);
}

int
bd_unit2bios(int unit)
{
    if ((unit >= 0) && (unit < nbdinfo))
	return(bdinfo[unit].bd_unit);
    return(-1);
}

/*    
 * Quiz the BIOS for disk devices, save a little info about them.
 *
 * XXX should we be consulting the BIOS equipment list, specifically
 *     the value at 0x475?
 */
static int
bd_init(void) 
{
    int		base, unit;

#ifdef PC98
    int         hd_drive=0, n=-0x10;
    /* sequence 0x90, 0x80, 0xa0 */
    for (base = 0x90; base <= 0xa0; base += n, n += 0x30) {
	for (unit = base; (nbdinfo < MAXBDDEV) || ((unit & 0x0f) < 4); unit++) {
	    bdinfo[nbdinfo].bd_unit = unit;
	    bdinfo[nbdinfo].bd_flags = (unit & 0xf0) == 0x90 ? BD_FLOPPY : 0;

	    /* XXX add EDD probes */
	    if (!bd_int13probe(&bdinfo[nbdinfo])){
		if (((unit & 0xf0) == 0x90 && (unit & 0x0f) < 4) ||
		    ((unit & 0xf0) == 0xa0 && (unit & 0x0f) < 6))
		    continue;	/* Target IDs are not contiguous. */
		else
		    break;
	    }

	    if (bdinfo[nbdinfo].bd_flags & BD_FLOPPY){
	        bdinfo[nbdinfo].bd_drive = 'A' + (unit & 0xf);
		/* available 1.44MB access? */
		if (*(u_char *)PTOV(0xA15AE) & (1<<(unit & 0xf))){
		    /* boot media 1.2MB FD? */
		    if ((*(u_char *)PTOV(0xA1584) & 0xf0) != 0x90)
		        bdinfo[nbdinfo].bd_unit = 0x30 + (unit & 0xf);
		}
	    }
	    else
	        bdinfo[nbdinfo].bd_drive = 'C' + hd_drive++;
	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   bdinfo[nbdinfo].bd_drive, nbdinfo);
	    nbdinfo++;
	}
    }
#else
    /* sequence 0, 0x80 */
    for (base = 0; base <= 0x80; base += 0x80) {
	for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
	    bdinfo[nbdinfo].bd_unit = unit;
	    bdinfo[nbdinfo].bd_flags = (unit < 0x80) ? BD_FLOPPY : 0;

	    /* XXX add EDD probes */
	    if (!bd_int13probe(&bdinfo[nbdinfo]))
		break;

	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   (unit < 0x80) ? ('A' + unit) : ('C' + unit - 0x80), nbdinfo);
	    nbdinfo++;
	}
    }
#endif
    return(0);
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */

static int
bd_int13probe(struct bdinfo *bd)
{
#ifdef PC98
    int addr;
    if (bd->bd_flags & BD_FLOPPY){
	addr = 0xa155c;
    }
    else {
	if ((bd->bd_unit & 0xf0) == 0x80)
	    addr = 0xa155d;
	else
	    addr = 0xa1482;
    }
    if ( *(u_char *)PTOV(addr) & (1<<(bd->bd_unit & 0x0f))) {
	bd->bd_flags |= BD_MODEINT13;
	return(1);
    }
    return(0);
#else
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = bd->bd_unit;
    v86int();
    
    if (!(v86.efl & 0x1) &&				/* carry clear */
	((v86.edx & 0xff) > (bd->bd_unit & 0x7f))) {	/* unit # OK */
	bd->bd_flags |= BD_MODEINT13;
	bd->bd_type = v86.ebx & 0xff;
	return(1);
    }
#endif
    return(0);
}

/*
 * Print information about disks
 */
static void
bd_print(int verbose)
{
    int				i, j;
    char			line[80];
    struct i386_devdesc		dev;
    struct open_disk		*od;
    struct dos_partition	*dptr;
    
    for (i = 0; i < nbdinfo; i++) {
#ifdef PC98
	sprintf(line, "    disk%d:   BIOS drive %c:\n", i, 
		bdinfo[i].bd_drive);
#else
	sprintf(line, "    disk%d:   BIOS drive %c:\n", i, 
		(bdinfo[i].bd_unit < 0x80) ? ('A' + bdinfo[i].bd_unit) : ('C' + bdinfo[i].bd_unit - 0x80));
#endif
	pager_output(line);

	/* try to open the whole disk */
	dev.d_kind.biosdisk.unit = i;
	dev.d_kind.biosdisk.slice = -1;
	dev.d_kind.biosdisk.partition = -1;
	
	if (!bd_opendisk(&od, &dev)) {

	    /* Do we have a partition table? */
	    if (od->od_flags & BD_PARTTABOK) {
		dptr = &od->od_parttab[0];

		/* Check for a "truly dedicated" disk */
#ifdef PC98
		for (j = 0; j < NDOSPART; j++) {
		    switch(dptr[j].dp_mid) {
		    case DOSMID_386BSD:
		        sprintf(line, "      disk%ds%d", i, j + 1);
			bd_printslice(od, dptr[j].dp_scyl * od->od_hds * od->od_sec + dptr[j].dp_shd * od->od_sec + dptr[j].dp_ssect, line);
			break;
		    default:
		    }
		}
#else
		if ((dptr[3].dp_typ == DOSPTYP_386BSD) &&
		    (dptr[3].dp_start == 0) &&
		    (dptr[3].dp_size == 50000)) {
		    sprintf(line, "      disk%d", i);
		    bd_printslice(od, 0, line);
		} else {
		    for (j = 0; j < NDOSPART; j++) {
			switch(dptr[j].dp_typ) {
			case DOSPTYP_386BSD:
			    sprintf(line, "      disk%ds%d", i, j + 1);
			    bd_printslice(od, dptr[j].dp_start, line);
			    break;
			default:
			}
		    }
		    
		}
#endif
	    }
	    bd_closedisk(od);
	}
    }
}

static void
bd_printslice(struct open_disk *od, int offset, char *prefix)
{
    char		line[80];
    u_char		buf[BIOSDISK_SECSIZE];
    struct disklabel	*lp;
    int			i;

    /* read disklabel */
    if (bd_read(od, offset + LABELSECTOR, 1, buf))
	return;
    lp =(struct disklabel *)(&buf[0]);
    if (lp->d_magic != DISKMAGIC) {
	sprintf(line, "bad disklabel\n");
	pager_output(line);
	return;
    }
    
    /* Print partitions */
    for (i = 0; i < lp->d_npartitions; i++) {
	if ((lp->d_partitions[i].p_fstype == FS_BSDFFS) || (lp->d_partitions[i].p_fstype == FS_SWAP) ||
	    ((lp->d_partitions[i].p_fstype == FS_UNUSED) && 
	     (od->od_flags & BD_FLOPPY) && (i == 0))) {	/* Floppies often have bogus fstype, print 'a' */
	    sprintf(line, "  %s%c: %s  %.6dMB (%d - %d)\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap" : "FFS",
		    lp->d_partitions[i].p_size / 2048,	/* 512-byte sector assumption */
		    lp->d_partitions[i].p_offset, lp->d_partitions[i].p_offset + lp->d_partitions[i].p_size);
	    pager_output(line);
	}
    }
}


/*
 * Attempt to open the disk described by (dev) for use by (f).
 *
 * Note that the philosophy here is "give them exactly what
 * they ask for".  This is necessary because being too "smart"
 * about what the user might want leads to complications.
 * (eg. given no slice or partition value, with a disk that is
 *  sliced - are they after the first BSD slice, or the DOS
 *  slice before it?)
 */
static int 
bd_open(struct open_file *f, ...)
{
    va_list			ap;
    struct i386_devdesc		*dev;
    struct open_disk		*od;
    int				error;

    va_start(ap, f);
    dev = va_arg(ap, struct i386_devdesc *);
    va_end(ap);
    if ((error = bd_opendisk(&od, dev)))
	return(error);
    
    /*
     * Save our context
     */
    ((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data = od;
    DEBUG("open_disk %p, partition at 0x%x", od, od->od_boff);
    return(0);
}

static int
bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev)
{
    struct dos_partition	*dptr;
    struct disklabel		*lp;
    struct open_disk		*od;
    int				sector, slice, i;
    int				error;
    u_char			buf[BUFSIZE];
    daddr_t			pref_slice[4];

    if (dev->d_kind.biosdisk.unit >= nbdinfo) {
	DEBUG("attempt to open nonexistent disk");
	return(ENXIO);
    }
    
    od = (struct open_disk *)malloc(sizeof(struct open_disk));
    if (!od) {
	DEBUG("no memory");
	return (ENOMEM);
    }

    /* Look up BIOS unit number, intialise open_disk structure */
    od->od_dkunit = dev->d_kind.biosdisk.unit;
    od->od_unit = bdinfo[od->od_dkunit].bd_unit;
    od->od_flags = bdinfo[od->od_dkunit].bd_flags;
    od->od_boff = 0;
    error = 0;
    DEBUG("open '%s', unit 0x%x slice %d partition %c",
	     i386_fmtdev(dev), dev->d_kind.biosdisk.unit, 
	     dev->d_kind.biosdisk.slice, dev->d_kind.biosdisk.partition + 'a');

    /* Get geometry for this open (removable device may have changed) */
    if (bd_getgeom(od)) {
	DEBUG("can't get geometry");
	error = ENXIO;
	goto out;
    }

    /*
     * Following calculations attempt to determine the correct value
     * for d->od_boff by looking for the slice and partition specified,
     * or searching for reasonable defaults.
     */

    /*
     * Find the slice in the DOS slice table.
     */
#ifdef PC98
    if (od->od_flags & BD_FLOPPY) {
	sector = 0;
	goto unsliced;
    }
#endif
    if (bd_read(od, 0, 1, buf)) {
	DEBUG("error reading MBR");
	error = EIO;
	goto out;
    }

    /* 
     * Check the slice table magic.
     */
    if ((buf[0x1fe] != 0x55) || (buf[0x1ff] != 0xaa)) {
	/* If a slice number was explicitly supplied, this is an error */
	if (dev->d_kind.biosdisk.slice > 0) {
	    DEBUG("no slice table/MBR (no magic)");
	    error = ENOENT;
	    goto out;
	}
	sector = 0;
	goto unsliced;		/* may be a floppy */
    }
#ifdef PC98
    if (bd_read(od, 1, 1, buf)) {
	DEBUG("error reading MBR");
	error = EIO;
	goto out;
    }
#endif
    bcopy(buf + DOSPARTOFF, &od->od_parttab, sizeof(struct dos_partition) * NDOSPART);
    dptr = &od->od_parttab[0];
    od->od_flags |= BD_PARTTABOK;

    /* Is this a request for the whole disk? */
    if (dev->d_kind.biosdisk.slice == -1) {
	sector = 0;
	goto unsliced;
    }

    /* Try to auto-detect the best slice; this should always give a slice number */
    if (dev->d_kind.biosdisk.slice == 0)
	dev->d_kind.biosdisk.slice = bd_bestslice(dptr);

    switch (dev->d_kind.biosdisk.slice) {
    case -1:
	error = ENOENT;
	goto out;
    case 0:
	sector = 0;
	goto unsliced;
    default:
	break;
    }

    /*
     * Accept the supplied slice number unequivocally (we may be looking
     * at a DOS partition).
     */
    dptr += (dev->d_kind.biosdisk.slice - 1);	/* we number 1-4, offsets are 0-3 */
#ifdef PC98
    sector = dptr->dp_scyl * od->od_hds * od->od_sec + dptr->dp_shd * od->od_sec + dptr->dp_ssect;
    {
	int end = dptr->dp_ecyl * od->od_hds * od->od_sec + dptr->dp_ehd * od->od_sec + dptr->dp_esect;
	DEBUG("slice entry %d at %d, %d sectors", dev->d_kind.biosdisk.slice - 1, sector, end-sector);
    }
#else
    sector = dptr->dp_start;
    DEBUG("slice entry %d at %d, %d sectors", dev->d_kind.biosdisk.slice - 1, sector, dptr->dp_size);
#endif

    /*
     * If we are looking at a BSD slice, and the partition is < 0, assume the 'a' partition
     */
#ifdef PC98
    if ((dptr->dp_mid == DOSMID_386BSD) && (dev->d_kind.biosdisk.partition < 0))
#else
    if ((dptr->dp_typ == DOSPTYP_386BSD) && (dev->d_kind.biosdisk.partition < 0))
#endif
	dev->d_kind.biosdisk.partition = 0;

 unsliced:
    /* 
     * Now we have the slice offset, look for the partition in the disklabel if we have
     * a partition to start with.
     *
     * XXX we might want to check the label checksum.
     */
    if (dev->d_kind.biosdisk.partition < 0) {
	od->od_boff = sector;		/* no partition, must be after the slice */
	DEBUG("opening raw slice");
    } else {
	if (bd_read(od, sector + LABELSECTOR, 1, buf)) {
	    DEBUG("error reading disklabel");
	    error = EIO;
	    goto out;
	}
	DEBUG("copy %d bytes of label from %p to %p", sizeof(struct disklabel), buf + LABELOFFSET, &od->od_disklabel);
	bcopy(buf + LABELOFFSET, &od->od_disklabel, sizeof(struct disklabel));
	lp = &od->od_disklabel;
	od->od_flags |= BD_LABELOK;

	if (lp->d_magic != DISKMAGIC) {
	    DEBUG("no disklabel");
	    error = ENOENT;
	    goto out;
	}
	if (dev->d_kind.biosdisk.partition >= lp->d_npartitions) {
	    DEBUG("partition '%c' exceeds partitions in table (a-'%c')",
		  'a' + dev->d_kind.biosdisk.partition, 'a' + lp->d_npartitions);
	    error = EPART;
	    goto out;

	}

	/* Complain if the partition type is wrong */
	if ((lp->d_partitions[dev->d_kind.biosdisk.partition].p_fstype == FS_UNUSED) &&
	    !(od->od_flags & BD_FLOPPY))	    /* Floppies often have bogus fstype */
	    DEBUG("warning, partition marked as unused");
	
	od->od_boff = lp->d_partitions[dev->d_kind.biosdisk.partition].p_offset;
    }
    
 out:
    if (error) {
	free(od);
    } else {
	*odp = od;	/* return the open disk */
    }
    return(error);
}


/*
 * Search for a slice with the following preferences:
 *
 * 1: Active FreeBSD slice
 * 2: Non-active FreeBSD slice
 * 3: Active FAT/FAT32 slice
 * 4: non-active FAT/FAT32 slice
 */
#define PREF_FBSD_ACT	0
#define PREF_FBSD	1
#define PREF_DOS_ACT	2
#define PREF_DOS	3
#define PREF_NONE	4

static int
bd_bestslice(struct dos_partition *dptr)
{
    int		i;
    int		preflevel, pref;


#ifndef PC98	
    /*
     * Check for the historically bogus MBR found on true dedicated disks
     */
    if ((dptr[3].dp_typ == DOSPTYP_386BSD) &&
	(dptr[3].dp_start == 0) &&
	(dptr[3].dp_size == 50000)) 
	return(0);
#endif

    preflevel = PREF_NONE;
    pref = -1;
    
    /* 
     * XXX No support here for 'extended' slices
     */
    for (i = 0; i < NDOSPART; i++) {
#ifdef PC98
	switch(dptr[i].dp_mid & 0x7f) {
	case DOSMID_386BSD & 0x7f:		/* FreeBSD */
	    if ((dptr[i].dp_mid & 0x80) && (preflevel > PREF_FBSD_ACT)) {
		pref = i;
		preflevel = PREF_FBSD_ACT;
	    } else if (preflevel > PREF_FBSD) {
		pref = i;
		preflevel = PREF_FBSD;
	    }
	    break;
	    
	    case 0x11:				/* DOS/Windows */
	    case 0x20:
	    case 0x21:
	    case 0x22:
	    case 0x23:
	    case 0x63:
	    if ((dptr[i].dp_mid & 0x80) && (preflevel > PREF_DOS_ACT)) {
		pref = i;
		preflevel = PREF_DOS_ACT;
	    } else if (preflevel > PREF_DOS) {
		pref = i;
		preflevel = PREF_DOS;
	    }
	    break;
	}
#else
	switch(dptr[i].dp_typ) {
	case DOSPTYP_386BSD:			/* FreeBSD */
	    if ((dptr[i].dp_flag & 0x80) && (preflevel > PREF_FBSD_ACT)) {
		pref = i;
		preflevel = PREF_FBSD_ACT;
	    } else if (preflevel > PREF_FBSD) {
		pref = i;
		preflevel = PREF_FBSD;
	    }
	    break;
	    
	    case 0x04:				/* DOS/Windows */
	    case 0x06:
	    case 0x0b:
	    case 0x0c:
	    case 0x0e:
	    case 0x63:
	    if ((dptr[i].dp_flag & 0x80) && (preflevel > PREF_DOS_ACT)) {
		pref = i;
		preflevel = PREF_DOS_ACT;
	    } else if (preflevel > PREF_DOS) {
		pref = i;
		preflevel = PREF_DOS;
	    }
	    break;
	}
#endif
    }
    return(pref + 1);	/* slices numbered 1-4 */
}
 

static int 
bd_close(struct open_file *f)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data);

    bd_closedisk(od);
    return(0);
}

static void
bd_closedisk(struct open_disk *od)
{
    DEBUG("open_disk %p", od);
#if 0
    /* XXX is this required? (especially if disk already open...) */
    if (od->od_flags & BD_FLOPPY)
	delay(3000000);
#endif
    free(od);
}

static int 
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
    struct bcache_devdata	bcd;
    
    bcd.dv_strategy = bd_realstrategy;
    bcd.dv_devdata = devdata;
    return(bcache_strategy(&bcd, rw, dblk, size, buf, rsize));
}

static int 
bd_realstrategy(void *devdata, int rw, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);
    int			blks;
#ifdef BD_SUPPORT_FRAGS
    char		fragbuf[BIOSDISK_SECSIZE];
    size_t		fragsize;

    fragsize = size % BIOSDISK_SECSIZE;
#else
    if (size % BIOSDISK_SECSIZE)
	panic("bd_strategy: %d bytes I/O not multiple of block size", size);
#endif

    DEBUG("open_disk %p", od);

    if (rw != F_READ)
	return(EROFS);


    blks = size / BIOSDISK_SECSIZE;
    DEBUG("read %d from %d+%d to %p", blks, od->od_boff, dblk, buf);

    if (rsize)
	*rsize = 0;
    if (blks && bd_read(od, dblk + od->od_boff, blks, buf)) {
	DEBUG("read error");
	return (EIO);
    }
#ifdef BD_SUPPORT_FRAGS
    DEBUG("bd_strategy: frag read %d from %d+%d+d to %p", 
	     fragsize, od->od_boff, dblk, blks, buf + (blks * BIOSDISK_SECSIZE));
    if (fragsize && bd_read(od, dblk + od->od_boff + blks, 1, fragsize)) {
	DEBUG("frag read error");
	return(EIO);
    }
    bcopy(fragbuf, buf + (blks * BIOSDISK_SECSIZE), fragsize);
#endif
    if (rsize)
	*rsize = size;
    return (0);
}

/* Max number of sectors to bounce-buffer if the request crosses a 64k boundary */
#define FLOPPY_BOUNCEBUF	18

static int
bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{
    int		x, bpc, cyl, hd, sec, result, resid, cnt, retry, maxfer;
    caddr_t	p, xp, bbuf, breg;
    
    bpc = (od->od_sec * od->od_hds);		/* blocks per cylinder */
    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
#ifdef PC98
    if (((od->od_unit & 0xf0) == 0x90 || (od->od_unit & 0xf0) == 0x30) && 
#else
    if ((od->od_unit < 0x80) && 
#endif
	((VTOP(dest) >> 16) != (VTOP(dest + blks * BIOSDISK_SECSIZE) >> 16))) {

	/* 
	 * There is a 64k physical boundary somewhere in the destination buffer, so we have
	 * to arrange a suitable bounce buffer.  Allocate a buffer twice as large as we
	 * need to.  Use the bottom half unless there is a break there, in which case we
	 * use the top half.
	 */
	x = min(FLOPPY_BOUNCEBUF, blks);
	bbuf = malloc(x * 2 * BIOSDISK_SECSIZE);
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) == ((u_int32_t)VTOP(dest + x * BIOSDISK_SECSIZE) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BIOSDISK_SECSIZE;
	}
	maxfer = x;			/* limit transfers to bounce region size */
    } else {
	bbuf = NULL;
	maxfer = 0;
    }
    
    while (resid > 0) {
	x = dblk;
	cyl = x / bpc;			/* block # / blocks per cylinder */
	x %= bpc;			/* block offset into cylinder */
	hd = x / od->od_sec;		/* offset / blocks per track */
	sec = x % od->od_sec;		/* offset into track */

	/* play it safe and don't cross track boundaries (XXX this is probably unnecessary) */
	x = min(od->od_sec - sec, resid);
	if (maxfer > 0)
	    x = min(x, maxfer);		/* fit bounce buffer */

	/* where do we transfer to? */
	xp = bbuf == NULL ? p : breg;

	/* correct sector number for 1-based BIOS numbering */
#ifdef PC98
	if ((od->od_unit & 0xf0) == 0x30 || (od->od_unit & 0xf0) == 0x90)
	    sec++;
#else
	sec++;
#endif

	/* Loop retrying the operation a couple of times.  The BIOS may also retry. */
	for (retry = 0; retry < 3; retry++) {
	    /* if retrying, reset the drive */
	    if (retry > 0) {
#ifdef PC98
#else
		v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0;
		v86.edx = od->od_unit;
		v86int();
#endif
	    }
	    
	    /* build request  XXX support EDD requests too */
#ifdef PC98
	    v86.ctl = 0;
	    v86.addr = 0x1b;
	    if (od->od_flags & BD_FLOPPY) {
	        v86.eax = 0xd600 | od->od_unit;
		v86.ecx = 0x0200 | (cyl & 0xff);
	    }
	    else {
	        v86.eax = 0x0600 | od->od_unit;
		v86.ecx = cyl;
	    }
	    v86.edx = (hd << 8) | sec;
	    v86.ebx = x * BIOSDISK_SECSIZE;
	    v86.es = VTOPSEG(xp);
	    v86.ebp = VTOPOFF(xp);
	    v86int();
#else
	    v86.ctl = V86_FLAGS;
	    v86.addr = 0x13;
	    v86.eax = 0x200 | x;
	    v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
	    v86.edx = (hd << 8) | od->od_unit;
	    v86.es = VTOPSEG(xp);
	    v86.ebx = VTOPOFF(xp);
	    v86int();
#endif
	    result = (v86.efl & 0x1);
	    if (result == 0)
		break;
	}
	
#ifdef PC98
 	DEBUG("%d sectors from %d/%d/%d to %p (0x%x) %s", x, cyl, hd, od->od_flags & BD_FLOPPY ? sec - 1 : sec, p, VTOP(p), result ? "failed" : "ok");
	/* BUG here, cannot use v86 in printf because putchar uses it too */
	DEBUG("ax = 0x%04x cx = 0x%04x dx = 0x%04x status 0x%x", 
	      od->od_flags & BD_FLOPPY ? 0xd600 | od->od_unit : 0x0600 | od->od_unit,
	      od->od_flags & BD_FLOPPY ? 0x0200 | cyl : cyl, (hd << 8) | sec,
	      (v86.eax >> 8) & 0xff);
#else
 	DEBUG("%d sectors from %d/%d/%d to %p (0x%x) %s", x, cyl, hd, sec - 1, p, VTOP(p), result ? "failed" : "ok");
	/* BUG here, cannot use v86 in printf because putchar uses it too */
	DEBUG("ax = 0x%04x cx = 0x%04x dx = 0x%04x status 0x%x", 
	      0x200 | x, ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec, (hd << 8) | od->od_unit, (v86.eax >> 8) & 0xff);
#endif
	if (result) {
	    if (bbuf != NULL)
		free(bbuf);
	    return(-1);
	}
	if (bbuf != NULL)
	    bcopy(breg, p, x * BIOSDISK_SECSIZE);
	p += (x * BIOSDISK_SECSIZE);
	dblk += x;
	resid -= x;
    }
	
/*    hexdump(dest, (blks * BIOSDISK_SECSIZE)); */
    if (bbuf != NULL)
	free(bbuf);
    return(0);
}

static int
bd_getgeom(struct open_disk *od)
{

#ifdef PC98
    if (od->od_flags & BD_FLOPPY) {
        od->od_cyl = 79;
	od->od_hds = 2;
	od->od_sec = (od->od_unit & 0xf0) == 0x30 ? 18 : 15;
    }
    else {
        v86.ctl = 0;
	v86.addr = 0x1b;
	v86.eax = 0x8400 | od->od_unit;
	v86int();
      
	od->od_cyl = v86.ecx;
	od->od_hds = (v86.edx >> 8) & 0xff;
	od->od_sec = v86.edx & 0xff;
    }
#else
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = od->od_unit;
    v86int();

    if ((v86.efl & 0x1) ||				/* carry set */
	((v86.edx & 0xff) <= (od->od_unit & 0x7f)))	/* unit # bad */
	return(1);
    
    /* convert max cyl # -> # of cylinders */
    od->od_cyl = ((v86.ecx & 0xc0) << 2) + ((v86.ecx & 0xff00) >> 8) + 1;
    /* convert max head # -> # of heads */
    od->od_hds = ((v86.edx & 0xff00) >> 8) + 1;
    od->od_sec = v86.ecx & 0x3f;
#endif

    DEBUG("unit 0x%x geometry %d/%d/%d", od->od_unit, od->od_cyl, od->od_hds, od->od_sec);
    return(0);
}

/*
 * Return a suitable dev_t value for (dev).
 *
 * In the case where it looks like (dev) is a SCSI disk, we allow the number of
 * IDE disks to be specified in $num_ide_disks.  There should be a Better Way.
 */
int
bd_getdev(struct i386_devdesc *dev)
{
    struct open_disk		*od;
    int				biosdev;
    int 			major;
    int				rootdev;
    char			*nip, *cp;
    int				unitofs = 0, i, unit;

    biosdev = bd_unit2bios(dev->d_kind.biosdisk.unit);
    DEBUG("unit %d BIOS device %d", dev->d_kind.biosdisk.unit, biosdev);
    if (biosdev == -1)				/* not a BIOS device */
	return(-1);
    if (bd_opendisk(&od, dev) != 0)		/* oops, not a viable device */
	return(-1);

#ifdef PC98
    if ((biosdev & 0xf0) == 0x90 || (biosdev & 0xf0) == 0x30) {
#else
    if (biosdev < 0x80) {
#endif
	/* floppy (or emulated floppy) or ATAPI device */
	if (bdinfo[dev->d_kind.biosdisk.unit].bd_type == DT_ATAPI) {
	    /* is an ATAPI disk */
	    major = WFDMAJOR;
	} else {
	    /* is a floppy disk */
	    major = FDMAJOR;
	}
    } else {
	/* harddisk */
	if ((od->od_flags & BD_LABELOK) && (od->od_disklabel.d_type == DTYPE_SCSI)) {
	    /* label OK, disk labelled as SCSI */
	    major = DAMAJOR;
	    /* check for unit number correction hint, now deprecated */
	    if ((nip = getenv("num_ide_disks")) != NULL) {
		i = strtol(nip, &cp, 0);
		/* check for parse error */
		if ((cp != nip) && (*cp == 0))
		    unitofs = i;
	    }
	} else {
	    /* assume an IDE disk */
	    major = WDMAJOR;
	}
    }
    /* XXX a better kludge to set the root disk unit number */
    if ((nip = getenv("root_disk_unit")) != NULL) {
	i = strtol(nip, &cp, 0);
	/* check for parse error */
	if ((cp != nip) && (*cp == 0))
	    unit = i;
    } else {
#ifdef PC98
        unit = biosdev & 0xf;					/* allow for #wd compenstation in da case */
#else
	unit = (biosdev & 0x7f) - unitofs;					/* allow for #wd compenstation in da case */
#endif
    }

    rootdev = MAKEBOOTDEV(major,
			  (dev->d_kind.biosdisk.slice + 1) >> 4, 	/* XXX slices may be wrong here */
			  (dev->d_kind.biosdisk.slice + 1) & 0xf, 
			  unit,
			  dev->d_kind.biosdisk.partition);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}

/*
 * Fix (dev) so that it refers to the 'real' disk/slice/partition that it implies.
 */
int
bd_fixupdev(struct i386_devdesc *dev)
{
    struct open_disk *od;
    
    /*
     * Open the disk.  This will fix up the slice and partition fields.
     */
    if (bd_opendisk(&od, dev) != 0)
	return(ENOENT);
    
    bd_closedisk(od);
}
