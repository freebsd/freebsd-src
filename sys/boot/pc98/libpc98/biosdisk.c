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
 * $FreeBSD$
 */

/*
 * BIOS disk device handling.
 * 
 * Ideas and algorithms from:
 *
 * - NetBSD libi386/biosdisk.c
 * - FreeBSD biosboot/disk.c
 *
 */

#include <stand.h>

#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/reboot.h>

#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#define BIOS_NUMDRIVES		0x475
#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)
#define	MAXBDDEV		MAXDEV

#define DT_ATAPI		0x10		/* disk type for ATAPI floppies */
#define WDMAJOR			0		/* major numbers for devices we frontend for */
#define WFDMAJOR		1
#define FDMAJOR			2
#define DAMAJOR			4

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
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
#define BD_MODEINT13		0x0000
#define BD_MODEEDD1		0x0001
#define BD_MODEEDD3		0x0002
#define BD_MODEMASK		0x0003
#define BD_FLOPPY		0x0004
#define BD_LABELOK		0x0008
#define BD_PARTTABOK		0x0010
    struct disklabel		od_disklabel;
    int				od_nslices;	/* slice count */
    struct dos_partition	od_slicetab[MAX_SLICES];
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
    int		bd_da_unit;		/* kernel unit number for da */
#endif
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

static int	bd_getgeom(struct open_disk *od);
static int	bd_read(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);
static int	bd_write(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);

static int	bd_int13probe(struct bdinfo *bd);

static void	bd_printslice(struct open_disk *od, struct dos_partition *dp,
		    char *prefix, int verbose);
static void	bd_printbsdslice(struct open_disk *od, daddr_t offset,
		    char *prefix, int verbose);

static int	bd_init(void);
static int	bd_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	bd_realstrategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
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
    bd_print,
    NULL
};

static int	bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev);
static void	bd_closedisk(struct open_disk *od);
static int	bd_bestslice(struct open_disk *od);
static void	bd_checkextended(struct open_disk *od, int slicenum);

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
 */
static int
bd_init(void) 
{
#ifdef PC98
    int		base, unit;
    int		da_drive=0, n=-0x10;

    /* sequence 0x90, 0x80, 0xa0 */
    for (base = 0x90; base <= 0xa0; base += n, n += 0x30) {
	for (unit = base; (nbdinfo < MAXBDDEV) || ((unit & 0x0f) < 4); unit++) {
	    bdinfo[nbdinfo].bd_unit = unit;
	    bdinfo[nbdinfo].bd_flags = (unit & 0xf0) == 0x90 ? BD_FLOPPY : 0;

	    if (!bd_int13probe(&bdinfo[nbdinfo])){
		if (((unit & 0xf0) == 0x90 && (unit & 0x0f) < 4) ||
		    ((unit & 0xf0) == 0xa0 && (unit & 0x0f) < 6))
		    continue;	/* Target IDs are not contiguous. */
		else
		    break;
	    }

	    if (bdinfo[nbdinfo].bd_flags & BD_FLOPPY){
		/* available 1.44MB access? */
		if (*(u_char *)PTOV(0xA15AE) & (1<<(unit & 0xf))) {
		    /* boot media 1.2MB FD? */
		    if ((*(u_char *)PTOV(0xA1584) & 0xf0) != 0x90)
		        bdinfo[nbdinfo].bd_unit = 0x30 + (unit & 0xf);
		}
	    }
	    else {
		if ((unit & 0xa0) == 0xa0)
		    bdinfo[nbdinfo].bd_da_unit = da_drive++;
	    }
	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   'A' + nbdinfo, nbdinfo);
	    nbdinfo++;
	}
    }
#else
    int		base, unit, nfd = 0;

    /* sequence 0, 0x80 */
    for (base = 0; base <= 0x80; base += 0x80) {
	for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
	    /* check the BIOS equipment list for number of fixed disks */
	    if((base == 0x80) &&
	       (nfd >= *(unsigned char *)PTOV(BIOS_NUMDRIVES)))
	        break;

	    bdinfo[nbdinfo].bd_unit = unit;
	    bdinfo[nbdinfo].bd_flags = (unit < 0x80) ? BD_FLOPPY : 0;

	    if (!bd_int13probe(&bdinfo[nbdinfo]))
		break;

	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   (unit < 0x80) ? ('A' + unit) : ('C' + unit - 0x80), nbdinfo);
	    nbdinfo++;
	    if (base == 0x80)
	        nfd++;
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

    if (bd->bd_flags & BD_FLOPPY) {
	addr = 0xa155c;
    } else {
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
	((v86.edx & 0xff) > ((unsigned)bd->bd_unit & 0x7f))) {	/* unit # OK */
	bd->bd_flags |= BD_MODEINT13;
	bd->bd_type = v86.ebx & 0xff;

	/* Determine if we can use EDD with this device. */
	v86.eax = 0x4100;
	v86.edx = bd->bd_unit;
	v86.ebx = 0x55aa;
	v86int();
	if (!(v86.efl & 0x1) &&				/* carry clear */
	    ((v86.ebx & 0xffff) == 0xaa55) &&		/* signature */
	    (v86.ecx & 0x1)) {				/* packets mode ok */
	    bd->bd_flags |= BD_MODEEDD1;
	    if((v86.eax & 0xff00) > 0x300)
	        bd->bd_flags |= BD_MODEEDD3;
	}
	return(1);
    }
    return(0);
#endif
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
	sprintf(line, "    disk%d:   BIOS drive %c:\n", i, 'A' + i);
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
		dptr = &od->od_slicetab[0];

		/* Check for a "dedicated" disk */
#ifdef PC98
		for (j = 0; j < od->od_nslices; j++) {
		    switch(dptr[j].dp_mid) {
		    case DOSMID_386BSD:
		        sprintf(line, "      disk%ds%d", i, j + 1);
			bd_printbsdslice(od,
			    dptr[j].dp_scyl * od->od_hds * od->od_sec +
			    dptr[j].dp_shd * od->od_sec + dptr[j].dp_ssect,
			    line, verbose);
			break;
		    default:
		    }
		}
#else
		if ((dptr[3].dp_typ == DOSPTYP_386BSD) &&
		    (dptr[3].dp_start == 0) &&
		    (dptr[3].dp_size == 50000)) {
		    sprintf(line, "      disk%d", i);
		    bd_printbsdslice(od, 0, line, verbose);
		} else {
		    for (j = 0; j < od->od_nslices; j++) {
		        sprintf(line, "      disk%ds%d", i, j + 1);
			bd_printslice(od, &dptr[j], line, verbose);
                    }
                }
#endif
	    }
	    bd_closedisk(od);
	}
    }
}

#ifndef PC98
/*
 * Print information about slices on a disk.  For the size calculations we
 * assume a 512 byte sector.
 */
static void
bd_printslice(struct open_disk *od, struct dos_partition *dp, char *prefix,
	int verbose)
{
	char line[80];

	switch (dp->dp_typ) {
	case DOSPTYP_386BSD:
		bd_printbsdslice(od, (daddr_t)dp->dp_start, prefix, verbose);
		return;
	case DOSPTYP_LINSWP:
		if (verbose)
			sprintf(line, "%s: Linux swap %.6dMB (%d - %d)\n",
			    prefix, dp->dp_size / 2048,
			    dp->dp_start, dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: Linux swap\n", prefix);
		break;
	case DOSPTYP_LINUX:
		/*
		 * XXX
		 * read the superblock to confirm this is an ext2fs partition?
		 */
		if (verbose)
			sprintf(line, "%s: ext2fs  %.6dMB (%d - %d)\n", prefix,
			    dp->dp_size / 2048, dp->dp_start,
			    dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: ext2fs\n", prefix);
		break;
	case 0x00:				/* unused partition */
	case DOSPTYP_EXT:
		return;
	case 0x01:
		if (verbose)
			sprintf(line, "%s: FAT-12  %.6dMB (%d - %d)\n", prefix,
			    dp->dp_size / 2048, dp->dp_start,
			    dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: FAT-12\n", prefix);
		break;
	case 0x04:
	case 0x06:
	case 0x0e:
		if (verbose)
			sprintf(line, "%s: FAT-16  %.6dMB (%d - %d)\n", prefix,
			    dp->dp_size / 2048, dp->dp_start,
			    dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: FAT-16\n", prefix);
		break;
	case 0x0b:
	case 0x0c:
		if (verbose)
			sprintf(line, "%s: FAT-32  %.6dMB (%d - %d)\n", prefix,
			    dp->dp_size / 2048, dp->dp_start,
			    dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: FAT-32\n", prefix);
		break;
	default:
		if (verbose)
			sprintf(line, "%s: Unknown fs: 0x%x  %.6dMB (%d - %d)\n",
			    prefix, dp->dp_typ, dp->dp_size / 2048,
			    dp->dp_start, dp->dp_start + dp->dp_size);
		else
			sprintf(line, "%s: Unknown fs: 0x%x\n", prefix,
			    dp->dp_typ);
	}
	pager_output(line);
}
#endif

/*
 * Print out each valid partition in the disklabel of a FreeBSD slice.
 * For size calculations, we assume a 512 byte sector size.
 */
static void
bd_printbsdslice(struct open_disk *od, daddr_t offset, char *prefix,
    int verbose)
{
    char		line[80];
    char		buf[BIOSDISK_SECSIZE];
    struct disklabel	*lp;
    int			i;

    /* read disklabel */
    if (bd_read(od, offset + LABELSECTOR, 1, buf))
	return;
    lp =(struct disklabel *)(&buf[0]);
    if (lp->d_magic != DISKMAGIC) {
	sprintf(line, "%s: FFS  bad disklabel\n", prefix);
	pager_output(line);
	return;
    }
    
    /* Print partitions */
    for (i = 0; i < lp->d_npartitions; i++) {
	/*
	 * For each partition, make sure we know what type of fs it is.  If
	 * not, then skip it.  However, since floppies often have bogus
	 * fstypes, print the 'a' partition on a floppy even if it is marked
	 * unused.
	 */
	if ((lp->d_partitions[i].p_fstype == FS_BSDFFS) ||
            (lp->d_partitions[i].p_fstype == FS_SWAP) ||
            (lp->d_partitions[i].p_fstype == FS_VINUM) ||
	    ((lp->d_partitions[i].p_fstype == FS_UNUSED) && 
	     (od->od_flags & BD_FLOPPY) && (i == 0))) {

	    /* Only print out statistics in verbose mode */
	    if (verbose)
	        sprintf(line, "  %s%c: %s  %.6dMB (%d - %d)\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap" : 
		    (lp->d_partitions[i].p_fstype == FS_VINUM) ? "vinum" :
		    "FFS",
		    lp->d_partitions[i].p_size / 2048,
		    lp->d_partitions[i].p_offset,
		    lp->d_partitions[i].p_offset + lp->d_partitions[i].p_size);
	    else
	        sprintf(line, "  %s%c: %s\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap" : 
		    (lp->d_partitions[i].p_fstype == FS_VINUM) ? "vinum" :
		    "FFS");
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
    char			buf[BUFSIZE];

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
    od->od_nslices = 0;
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
    if (((u_char)buf[0x1fe] != 0x55) || ((u_char)buf[0x1ff] != 0xaa)) {
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

    /*
     * copy the partition table, then pick up any extended partitions.
     */
    bcopy(buf + DOSPARTOFF, &od->od_slicetab,
      sizeof(struct dos_partition) * NDOSPART);
#ifdef PC98
    od->od_nslices = NDOSPART;		/* extended slices start here */
#else
    od->od_nslices = 4;			/* extended slices start here */
    for (i = 0; i < NDOSPART; i++)
        bd_checkextended(od, i);
#endif
    od->od_flags |= BD_PARTTABOK;
    dptr = &od->od_slicetab[0];

    /* Is this a request for the whole disk? */
    if (dev->d_kind.biosdisk.slice == -1) {
	sector = 0;
	goto unsliced;
    }

    /*
     * if a slice number was supplied but not found, this is an error.
     */
    if (dev->d_kind.biosdisk.slice > 0) {
        slice = dev->d_kind.biosdisk.slice - 1;
        if (slice >= od->od_nslices) {
            DEBUG("slice %d not found", slice);
	    error = ENOENT;
	    goto out;
        }
    }

#ifndef PC98
    /*
     * Check for the historically bogus MBR found on true dedicated disks
     */
    if ((dptr[3].dp_typ == DOSPTYP_386BSD) &&
      (dptr[3].dp_start == 0) &&
      (dptr[3].dp_size == 50000)) {
        sector = 0;
        goto unsliced;
    }
#endif

    /* Try to auto-detect the best slice; this should always give a slice number */
    if (dev->d_kind.biosdisk.slice == 0) {
	slice = bd_bestslice(od);
        if (slice == -1) {
	    error = ENOENT;
            goto out;
        }
        dev->d_kind.biosdisk.slice = slice;
    }

    dptr = &od->od_slicetab[0];
    /*
     * Accept the supplied slice number unequivocally (we may be looking
     * at a DOS partition).
     */
    dptr += (dev->d_kind.biosdisk.slice - 1);	/* we number 1-4, offsets are 0-3 */
#ifdef PC98
    sector = dptr->dp_scyl * od->od_hds * od->od_sec +
	dptr->dp_shd * od->od_sec + dptr->dp_ssect;
    {
	int end = dptr->dp_ecyl * od->od_hds * od->od_sec +
	    dptr->dp_ehd * od->od_sec + dptr->dp_esect;
	DEBUG("slice entry %d at %d, %d sectors",
	      dev->d_kind.biosdisk.slice - 1, sector, end-sector);
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

#ifdef DISK_DEBUG
	/* Complain if the partition is unused unless this is a floppy. */
	if ((lp->d_partitions[dev->d_kind.biosdisk.partition].p_fstype == FS_UNUSED) &&
	    !(od->od_flags & BD_FLOPPY))
	    DEBUG("warning, partition marked as unused");
#endif
	
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

#ifndef PC98
static void
bd_checkextended(struct open_disk *od, int slicenum)
{
	char	buf[BIOSDISK_SECSIZE];
	struct dos_partition *dp;
	u_int base;
	int i, start, end;

	dp = &od->od_slicetab[slicenum];
	start = od->od_nslices;

	if (dp->dp_size == 0)
		goto done;
	if (dp->dp_typ != DOSPTYP_EXT)
		goto done;
	if (bd_read(od, (daddr_t)dp->dp_start, 1, buf))
		goto done;
	if (((u_char)buf[0x1fe] != 0x55) || ((u_char)buf[0x1ff] != 0xaa)) {
		DEBUG("no magic in extended table");
		goto done;
	}
	base = dp->dp_start;
	dp = (struct dos_partition *)(&buf[DOSPARTOFF]);
	for (i = 0; i < NDOSPART; i++, dp++) {
		if (dp->dp_size == 0)
			continue;
		if (od->od_nslices == MAX_SLICES)
			goto done;
		dp->dp_start += base;
		bcopy(dp, &od->od_slicetab[od->od_nslices], sizeof(*dp));
		od->od_nslices++;
	}
	end = od->od_nslices;

	/*
	 * now, recursively check the slices we just added
	 */
	for (i = start; i < end; i++)
		bd_checkextended(od, i);
done:
	return;
}
#endif

/*
 * Search for a slice with the following preferences:
 *
 * 1: Active FreeBSD slice
 * 2: Non-active FreeBSD slice
 * 3: Active Linux slice
 * 4: non-active Linux slice
 * 5: Active FAT/FAT32 slice
 * 6: non-active FAT/FAT32 slice
 */
#define PREF_RAWDISK	0
#define PREF_FBSD_ACT	1
#define PREF_FBSD	2
#define PREF_LINUX_ACT	3
#define PREF_LINUX	4
#define PREF_DOS_ACT	5
#define PREF_DOS	6
#define PREF_NONE	7

/*
 * slicelimit is in the range 0 .. NDOSPART
 */
static int
bd_bestslice(struct open_disk *od)
{
	struct dos_partition *dp;
	int pref, preflevel;
	int i, prefslice;
	
	prefslice = 0;
	preflevel = PREF_NONE;

	dp = &od->od_slicetab[0];
	for (i = 0; i < od->od_nslices; i++, dp++) {

#ifdef PC98
		switch(dp->dp_mid & 0x7f) {
		case DOSMID_386BSD & 0x7f:		/* FreeBSD */
			if ((dp->dp_mid & 0x80) &&
			    (preflevel > PREF_FBSD_ACT)) {
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
			if ((dp->dp_mid & 0x80) &&
			    (preflevel > PREF_DOS_ACT)) {
				pref = i;
				preflevel = PREF_DOS_ACT;
			} else if (preflevel > PREF_DOS) {
				pref = i;
				preflevel = PREF_DOS;
			}
			break;
		}
#else
		switch (dp->dp_typ) {
		case DOSPTYP_386BSD:		/* FreeBSD */
			pref = dp->dp_flag & 0x80 ? PREF_FBSD_ACT : PREF_FBSD;
			break;

		case DOSPTYP_LINUX:
			pref = dp->dp_flag & 0x80 ? PREF_LINUX_ACT : PREF_LINUX;
			break;
    
		case 0x01:		/* DOS/Windows */
		case 0x04:
		case 0x06:
		case 0x0b:
		case 0x0c:
		case 0x0e:
			pref = dp->dp_flag & 0x80 ? PREF_DOS_ACT : PREF_DOS;
			break;

		default:
		        pref = PREF_NONE;
		}
		if (pref < preflevel) {
			preflevel = pref;
			prefslice = i + 1;
		}
#endif
	}
	return (prefslice);
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
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
    struct bcache_devdata	bcd;
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);

    bcd.dv_strategy = bd_realstrategy;
    bcd.dv_devdata = devdata;
    return(bcache_strategy(&bcd, od->od_unit, rw, dblk+od->od_boff, size, buf, rsize));
}

static int 
bd_realstrategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
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


	switch(rw){
		case F_READ:
    blks = size / BIOSDISK_SECSIZE;
    DEBUG("read %d from %d to %p", blks, dblk, buf);

    if (rsize)
	*rsize = 0;
    if (blks && bd_read(od, dblk, blks, buf)) {
	DEBUG("read error");
	return (EIO);
    }
#ifdef BD_SUPPORT_FRAGS
    DEBUG("bd_strategy: frag read %d from %d+%d to %p", 
	     fragsize, dblk, blks, buf + (blks * BIOSDISK_SECSIZE));
    if (fragsize && bd_read(od, dblk + blks, 1, fragsize)) {
	DEBUG("frag read error");
	return(EIO);
    }
    bcopy(fragbuf, buf + (blks * BIOSDISK_SECSIZE), fragsize);
#endif
    if (rsize)
	*rsize = size;
    return (0);
		break;

		case F_WRITE :
    blks = size / BIOSDISK_SECSIZE;
    DEBUG("write %d from %d to %p", blks, dblk, buf);

    if (rsize)
	*rsize = 0;
    if (blks && bd_write(od, dblk, blks, buf)) {
	DEBUG("write error");
	return (EIO);
    }
#ifdef BD_SUPPORT_FRAGS
	if(fragsize) {
	DEBUG("Attempted to write a frag");
		return (EIO);
	}
#endif

    if (rsize)
	*rsize = size;
    return (0);
		default:
		 /* DO NOTHING */
	}

	return EROFS;
}

/* Max number of sectors to bounce-buffer if the request crosses a 64k boundary */
#define FLOPPY_BOUNCEBUF	18

static int
bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{
    u_int	x, bpc, cyl, hd, sec, result, resid, retry, maxfer;
    caddr_t	p, xp, bbuf, breg;
    
    /* Just in case some idiot actually tries to read -1 blocks... */
    if (blks < 0)
	return (-1);

    bpc = (od->od_sec * od->od_hds);		/* blocks per cylinder */
    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
#ifdef PC98
    if (
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
#ifdef PC98
	x = min(od->od_sec, (unsigned)blks);
#else
	x = min(FLOPPY_BOUNCEBUF, (unsigned)blks);
#endif
	bbuf = malloc(x * 2 * BIOSDISK_SECSIZE);
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) == ((u_int32_t)VTOP(dest + x * BIOSDISK_SECSIZE) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BIOSDISK_SECSIZE;
	}
	maxfer = x;			/* limit transfers to bounce region size */
    } else {
	breg = bbuf = NULL;
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
		v86.ctl = V86_FLAGS;
		v86.addr = 0x1b;
		v86.eax = 0x0300 | od->od_unit;
#else
		v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0;
		v86.edx = od->od_unit;
#endif
		v86int();
	    }
	    
#ifdef PC98
	    v86.ctl = V86_FLAGS;
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
	    result = (v86.efl & 0x1);
	    if (result == 0)
		break;
#else
	    if(cyl > 1023) {
	        /* use EDD if the disk supports it, otherwise, return error */
	        if(od->od_flags & BD_MODEEDD1) {
		    static unsigned short packet[8];

		    packet[0] = 0x10;
		    packet[1] = x;
		    packet[2] = VTOPOFF(xp);
		    packet[3] = VTOPSEG(xp);
		    packet[4] = dblk & 0xffff;
		    packet[5] = dblk >> 16;
		    packet[6] = 0;
		    packet[7] = 0;
		    v86.ctl = V86_FLAGS;
		    v86.addr = 0x13;
		    v86.eax = 0x4200;
		    v86.edx = od->od_unit;
		    v86.ds = VTOPSEG(packet);
		    v86.esi = VTOPOFF(packet);
		    v86int();
		    result = (v86.efl & 0x1);
		    if(result == 0)
		      break;
		} else {
		    result = 1;
		    break;
		}
	    } else {
	        /* Use normal CHS addressing */
	        v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0x200 | x;
		v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
		v86.edx = (hd << 8) | od->od_unit;
		v86.es = VTOPSEG(xp);
		v86.ebx = VTOPOFF(xp);
		v86int();
		result = (v86.efl & 0x1);
		if (result == 0)
		  break;
	    }
#endif
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
bd_write(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{
    u_int	x, bpc, cyl, hd, sec, result, resid, retry, maxfer;
    caddr_t	p, xp, bbuf, breg;
    
    /* Just in case some idiot actually tries to read -1 blocks... */
    if (blks < 0)
	return (-1);

    bpc = (od->od_sec * od->od_hds);		/* blocks per cylinder */
    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
#ifdef PC98
    if (
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

#ifdef PC98
	x = min(od->od_sec, (unsigned)blks);
#else
	x = min(FLOPPY_BOUNCEBUF, (unsigned)blks);
#endif
	bbuf = malloc(x * 2 * BIOSDISK_SECSIZE);
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) == ((u_int32_t)VTOP(dest + x * BIOSDISK_SECSIZE) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BIOSDISK_SECSIZE;
	}
	maxfer = x;			/* limit transfers to bounce region size */
    } else {
	breg = bbuf = NULL;
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


	/* Put your Data In, Put your Data out,
	   Put your Data In, and shake it all about 
	*/
	if (bbuf != NULL)
	    bcopy(p, breg, x * BIOSDISK_SECSIZE);
	p += (x * BIOSDISK_SECSIZE);
	dblk += x;
	resid -= x;

	/* Loop retrying the operation a couple of times.  The BIOS may also retry. */
	for (retry = 0; retry < 3; retry++) {
	    /* if retrying, reset the drive */
	    if (retry > 0) {
#ifdef PC98
		v86.ctl = V86_FLAGS;
		v86.addr = 0x1b;
		v86.eax = 0x0300 | od->od_unit;
#else
		v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0;
		v86.edx = od->od_unit;
#endif
		v86int();
	    }
	    
#ifdef PC98
	    v86.ctl = V86_FLAGS;
	    v86.addr = 0x1b;
	    if (od->od_flags & BD_FLOPPY) {
		v86.eax = 0xd500 | od->od_unit;
		v86.ecx = 0x0200 | (cyl & 0xff);
	    } else {
		v86.eax = 0x0500 | od->od_unit;
		v86.ecx = cyl;
	    }
	    v86.edx = (hd << 8) | sec;
	    v86.ebx = x * BIOSDISK_SECSIZE;
	    v86.es = VTOPSEG(xp);
	    v86.ebp = VTOPOFF(xp);
	    v86int();
	    result = (v86.efl & 0x1);
	    if (result == 0)
		break;
#else
	    if(cyl > 1023) {
	        /* use EDD if the disk supports it, otherwise, return error */
	        if(od->od_flags & BD_MODEEDD1) {
		    static unsigned short packet[8];

		    packet[0] = 0x10;
		    packet[1] = x;
		    packet[2] = VTOPOFF(xp);
		    packet[3] = VTOPSEG(xp);
		    packet[4] = dblk & 0xffff;
		    packet[5] = dblk >> 16;
		    packet[6] = 0;
		    packet[7] = 0;
		    v86.ctl = V86_FLAGS;
		    v86.addr = 0x13;
			/* Should we Write with verify ?? 0x4302 ? */
		    v86.eax = 0x4300;
		    v86.edx = od->od_unit;
		    v86.ds = VTOPSEG(packet);
		    v86.esi = VTOPOFF(packet);
		    v86int();
		    result = (v86.efl & 0x1);
		    if(result == 0)
		      break;
		} else {
		    result = 1;
		    break;
		}
	    } else {
	        /* Use normal CHS addressing */
	        v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0x300 | x;
		v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
		v86.edx = (hd << 8) | od->od_unit;
		v86.es = VTOPSEG(xp);
		v86.ebx = VTOPOFF(xp);
		v86int();
		result = (v86.efl & 0x1);
		if (result == 0)
		  break;
	    }
#endif
	}
	
#ifdef PC98
	DEBUG("%d sectors from %d/%d/%d to %p (0x%x) %s", x, cyl, hd,
	    od->od_flags & BD_FLOPPY ? sec - 1 : sec, p, VTOP(p),
	    result ? "failed" : "ok");
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
    } else {
	v86.ctl = V86_FLAGS;
	v86.addr = 0x1b;
	v86.eax = 0x8400 | od->od_unit;
	v86int();
      
	od->od_cyl = v86.ecx;
	od->od_hds = (v86.edx >> 8) & 0xff;
	od->od_sec = v86.edx & 0xff;
	if (v86.efl & 0x1)
	    return(1);
    }
#else
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = od->od_unit;
    v86int();

    if ((v86.efl & 0x1) ||				/* carry set */
	((v86.edx & 0xff) <= (unsigned)(od->od_unit & 0x7f)))	/* unit # bad */
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
 * Return the BIOS geometry of a given "fixed drive" in a format
 * suitable for the legacy bootinfo structure.  Since the kernel is
 * expecting raw int 0x13/0x8 values for N_BIOS_GEOM drives, we
 * prefer to get the information directly, rather than rely on being
 * able to put it together from information already maintained for
 * different purposes and for a probably different number of drives.
 *
 * For valid drives, the geometry is expected in the format (31..0)
 * "000000cc cccccccc hhhhhhhh 00ssssss"; and invalid drives are
 * indicated by returning the geometry of a "1.2M" PC-format floppy
 * disk.  And, incidentally, what is returned is not the geometry as
 * such but the highest valid cylinder, head, and sector numbers.
 */
u_int32_t
bd_getbigeom(int bunit)
{

#ifdef PC98
    int hds = 0;
    int unit = 0x80;		/* IDE HDD */
    u_int addr = 0xA155d;

    while (unit < 0xa7) {
	if (*(u_char *)PTOV(addr) & (1 << (unit & 0x0f)))
	    if (hds++ == bunit)
		break;
	if (++unit == 0x84) {
	    unit = 0xa0;	/* SCSI HDD */
	    addr = 0xA1482;
	}
    }
    if (unit == 0xa7)
	return 0x4f010f;
    v86.ctl = V86_FLAGS;
    v86.addr = 0x1b;
    v86.eax = 0x8400 | unit;
    v86int();
    if (v86.efl & 0x1)
	return 0x4f010f;
    return ((v86.ecx & 0xffff) << 16) | (v86.edx & 0xffff);
#else
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = 0x80 + bunit;
    v86int();
    if (v86.efl & 0x1)
	return 0x4f010f;
    return ((v86.ecx & 0xc0) << 18) | ((v86.ecx & 0xff00) << 8) |
	   (v86.edx & 0xff00) | (v86.ecx & 0x3f);
#endif
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
    /* default root disk unit number */
#ifdef PC98
    if ((biosdev & 0xf0) == 0xa0)
	unit = bdinfo[dev->d_kind.biosdisk.unit].bd_da_unit;
    else
	unit = biosdev & 0xf;
#else
    unit = (biosdev & 0x7f) - unitofs;
#endif

    /* XXX a better kludge to set the root disk unit number */
    if ((nip = getenv("root_disk_unit")) != NULL) {
	i = strtol(nip, &cp, 0);
	/* check for parse error */
	if ((cp != nip) && (*cp == 0))
	    unit = i;
    }

    rootdev = MAKEBOOTDEV(major,
			  (dev->d_kind.biosdisk.slice + 1) >> 4, 	/* XXX slices may be wrong here */
			  (dev->d_kind.biosdisk.slice + 1) & 0xf, 
			  unit,
			  dev->d_kind.biosdisk.partition);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}
