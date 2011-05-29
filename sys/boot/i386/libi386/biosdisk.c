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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/diskmbr.h>
#include <sys/gpt.h>
#include <machine/bootinfo.h>

#include <stdarg.h>
#include <uuid.h>

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

#ifdef LOADER_GPT_SUPPORT
struct gpt_part {
    int		gp_index;
    uuid_t	gp_type;
    uint64_t	gp_start;
    uint64_t	gp_end;
};
#endif

struct open_disk {
    int			od_dkunit;		/* disk unit number */
    int			od_unit;		/* BIOS unit number */
    int			od_cyl;			/* BIOS geometry */
    int			od_hds;
    int			od_sec;
    daddr_t			od_boff;		/* block offset from beginning of BIOS disk */
    int			od_flags;
#define BD_MODEINT13		0x0000
#define BD_MODEEDD1		0x0001
#define BD_MODEEDD3		0x0002
#define BD_MODEMASK		0x0003
#define BD_FLOPPY		0x0004
#define BD_LABELOK		0x0008
#define BD_PARTTABOK		0x0010
#ifdef LOADER_GPT_SUPPORT
#define	BD_GPTOK		0x0020
#endif
    union {
	struct {
	    struct disklabel		mbr_disklabel;
	    int				mbr_nslices;	/* slice count */
	    struct dos_partition	mbr_slicetab[NEXTDOSPART];
	} _mbr;
#ifdef LOADER_GPT_SUPPORT
	struct {
	    int				gpt_nparts;		
	    struct gpt_part		*gpt_partitions;
	} _gpt;
#endif
    } _data;
};

#define	od_disklabel		_data._mbr.mbr_disklabel
#define	od_nslices		_data._mbr.mbr_nslices
#define	od_slicetab		_data._mbr.mbr_slicetab
#ifdef LOADER_GPT_SUPPORT
#define	od_nparts		_data._gpt.gpt_nparts
#define	od_partitions		_data._gpt.gpt_partitions
#endif

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bdinfo
{
    int		bd_unit;		/* BIOS unit number */
    int		bd_flags;
    int		bd_type;		/* BIOS 'drive type' (floppy only) */
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

static int	bd_getgeom(struct open_disk *od);
static int	bd_read(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);
static int	bd_write(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);

static int	bd_int13probe(struct bdinfo *bd);

#ifdef LOADER_GPT_SUPPORT
static void	bd_printgptpart(struct open_disk *od, struct gpt_part *gp,
		    char *prefix, int verbose);
#endif
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
static int	bd_open_mbr(struct open_disk *od, struct i386_devdesc *dev);
static int	bd_bestslice(struct open_disk *od);
static void	bd_checkextended(struct open_disk *od, int slicenum);
#ifdef LOADER_GPT_SUPPORT
static int	bd_open_gpt(struct open_disk *od, struct i386_devdesc *dev);
static struct gpt_part *bd_best_gptpart(struct open_disk *od);
#endif

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
    int		base, unit, nfd = 0;

    /* sequence 0, 0x80 */
    for (base = 0; base <= 0x80; base += 0x80) {
	for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
#ifndef VIRTUALBOX
	    /* check the BIOS equipment list for number of fixed disks */
	    if((base == 0x80) &&
	       (nfd >= *(unsigned char *)PTOV(BIOS_NUMDRIVES)))
		break;
#endif

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
    return(0);
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */
static int
bd_int13probe(struct bdinfo *bd)
{
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = bd->bd_unit;
    v86int();
    
    if (!(v86.efl & 0x1) &&				/* carry clear */
	((v86.edx & 0xff) > ((unsigned)bd->bd_unit & 0x7f))) {	/* unit # OK */
	if ((v86.ecx & 0x3f) == 0) {			/* absurd sector size */
		DEBUG("Invalid geometry for unit %d", bd->bd_unit);
		return(0);				/* skip device */
	}
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
	    if((v86.eax & 0xff00) >= 0x3000)
	        bd->bd_flags |= BD_MODEEDD3;
	}
	return(1);
    }
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
	sprintf(line, "    disk%d:   BIOS drive %c:\n", i, 
		(bdinfo[i].bd_unit < 0x80) ? ('A' + bdinfo[i].bd_unit) : ('C' + bdinfo[i].bd_unit - 0x80));
	pager_output(line);

	/* try to open the whole disk */
	dev.d_unit = i;
	dev.d_kind.biosdisk.slice = -1;
	dev.d_kind.biosdisk.partition = -1;
	
	if (!bd_opendisk(&od, &dev)) {

#ifdef LOADER_GPT_SUPPORT
	    /* Do we have a GPT table? */
	    if (od->od_flags & BD_GPTOK) {
		for (j = 0; j < od->od_nparts; j++) {
		    sprintf(line, "      disk%dp%d", i,
			od->od_partitions[j].gp_index);
		    bd_printgptpart(od, &od->od_partitions[j], line, verbose);
		}
	    } else
#endif
	    /* Do we have a partition table? */
	    if (od->od_flags & BD_PARTTABOK) {
		dptr = &od->od_slicetab[0];

		/* Check for a "dedicated" disk */
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
	    }
	    bd_closedisk(od);
	}
    }
}

/* Given a size in 512 byte sectors, convert it to a human-readable number. */
static char *
display_size(uint64_t size)
{
    static char buf[80];
    char unit;

    size /= 2;
    unit = 'K';
    if (size >= 10485760000LL) {
	size /= 1073741824;
	unit = 'T';
    } else if (size >= 10240000) {
	size /= 1048576;
	unit = 'G';
    } else if (size >= 10000) {
	size /= 1024;
	unit = 'M';
    }
    sprintf(buf, "%.6ld%cB", (long)size, unit);
    return (buf);
}

#ifdef LOADER_GPT_SUPPORT
static uuid_t efi = GPT_ENT_TYPE_EFI;
static uuid_t freebsd_boot = GPT_ENT_TYPE_FREEBSD_BOOT;
static uuid_t freebsd_ufs = GPT_ENT_TYPE_FREEBSD_UFS;
static uuid_t freebsd_swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static uuid_t freebsd_zfs = GPT_ENT_TYPE_FREEBSD_ZFS;
static uuid_t ms_basic_data = GPT_ENT_TYPE_MS_BASIC_DATA;

static void
bd_printgptpart(struct open_disk *od, struct gpt_part *gp, char *prefix,
    int verbose)
{
    char stats[80];
    char line[96];

    if (verbose)
	sprintf(stats, " %s", display_size(gp->gp_end + 1 - gp->gp_start));
    else
	stats[0] = '\0';

    if (uuid_equal(&gp->gp_type, &efi, NULL))
	sprintf(line, "%s: EFI         %s\n", prefix, stats);
    else if (uuid_equal(&gp->gp_type, &ms_basic_data, NULL))
	sprintf(line, "%s: FAT/NTFS    %s\n", prefix, stats);
    else if (uuid_equal(&gp->gp_type, &freebsd_boot, NULL))
	sprintf(line, "%s: FreeBSD boot%s\n", prefix, stats);
    else if (uuid_equal(&gp->gp_type, &freebsd_ufs, NULL))
	sprintf(line, "%s: FreeBSD UFS %s\n", prefix, stats);
    else if (uuid_equal(&gp->gp_type, &freebsd_zfs, NULL))
	sprintf(line, "%s: FreeBSD ZFS %s\n", prefix, stats);
    else if (uuid_equal(&gp->gp_type, &freebsd_swap, NULL))
	sprintf(line, "%s: FreeBSD swap%s\n", prefix, stats);
    else
	sprintf(line, "%s: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x%s\n",
	    prefix,
	    gp->gp_type.time_low, gp->gp_type.time_mid,
	    gp->gp_type.time_hi_and_version,
	    gp->gp_type.clock_seq_hi_and_reserved, gp->gp_type.clock_seq_low,
	    gp->gp_type.node[0], gp->gp_type.node[1], gp->gp_type.node[2],
	    gp->gp_type.node[3], gp->gp_type.node[4], gp->gp_type.node[5],
	    stats);
    pager_output(line);
}
#endif

/*
 * Print information about slices on a disk.  For the size calculations we
 * assume a 512 byte sector.
 */
static void
bd_printslice(struct open_disk *od, struct dos_partition *dp, char *prefix,
	int verbose)
{
	char stats[80];
	char line[80];

	if (verbose)
		sprintf(stats, " %s (%d - %d)", display_size(dp->dp_size),
		    dp->dp_start, dp->dp_start + dp->dp_size);
	else
		stats[0] = '\0';

	switch (dp->dp_typ) {
	case DOSPTYP_386BSD:
		bd_printbsdslice(od, (daddr_t)dp->dp_start, prefix, verbose);
		return;
	case DOSPTYP_LINSWP:
		sprintf(line, "%s: Linux swap%s\n", prefix, stats);
		break;
	case DOSPTYP_LINUX:
		/*
		 * XXX
		 * read the superblock to confirm this is an ext2fs partition?
		 */
		sprintf(line, "%s: ext2fs%s\n", prefix, stats);
		break;
	case 0x00:				/* unused partition */
	case DOSPTYP_EXT:
		return;
	case 0x01:
		sprintf(line, "%s: FAT-12%s\n", prefix, stats);
		break;
	case 0x04:
	case 0x06:
	case 0x0e:
		sprintf(line, "%s: FAT-16%s\n", prefix, stats);
		break;
	case 0x07:
		sprintf(line, "%s: NTFS/HPFS%s\n", prefix, stats);
		break;
	case 0x0b:
	case 0x0c:
		sprintf(line, "%s: FAT-32%s\n", prefix, stats);
		break;
	default:
		sprintf(line, "%s: Unknown fs: 0x%x %s\n", prefix, dp->dp_typ,
		    stats);
	}
	pager_output(line);
}

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
	        sprintf(line, "  %s%c: %s %s (%d - %d)\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap " : 
		    (lp->d_partitions[i].p_fstype == FS_VINUM) ? "vinum" :
		    "FFS  ",
		    display_size(lp->d_partitions[i].p_size),
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
    struct open_disk		*od;
    int				error;

    if (dev->d_unit >= nbdinfo) {
	DEBUG("attempt to open nonexistent disk");
	return(ENXIO);
    }
    
    od = (struct open_disk *)malloc(sizeof(struct open_disk));
    if (!od) {
	DEBUG("no memory");
	return (ENOMEM);
    }

    /* Look up BIOS unit number, intialise open_disk structure */
    od->od_dkunit = dev->d_unit;
    od->od_unit = bdinfo[od->od_dkunit].bd_unit;
    od->od_flags = bdinfo[od->od_dkunit].bd_flags;
    od->od_boff = 0;
    error = 0;
    DEBUG("open '%s', unit 0x%x slice %d partition %d",
	     i386_fmtdev(dev), dev->d_unit, 
	     dev->d_kind.biosdisk.slice, dev->d_kind.biosdisk.partition);

    /* Get geometry for this open (removable device may have changed) */
    if (bd_getgeom(od)) {
	DEBUG("can't get geometry");
	error = ENXIO;
	goto out;
    }

    /* Determine disk layout. */
#ifdef LOADER_GPT_SUPPORT
    error = bd_open_gpt(od, dev);
    if (error)
#endif
	error = bd_open_mbr(od, dev);
    
 out:
    if (error) {
	free(od);
    } else {
	*odp = od;	/* return the open disk */
    }
    return(error);
}

static int
bd_open_mbr(struct open_disk *od, struct i386_devdesc *dev)
{
    struct dos_partition	*dptr;
    struct disklabel		*lp;
    int				sector, slice, i;
    int				error;
    char			buf[BUFSIZE];

    /*
     * Following calculations attempt to determine the correct value
     * for d->od_boff by looking for the slice and partition specified,
     * or searching for reasonable defaults.
     */

    /*
     * Find the slice in the DOS slice table.
     */
    od->od_nslices = 0;
    if (bd_read(od, 0, 1, buf)) {
	DEBUG("error reading MBR");
	return (EIO);
    }

    /* 
     * Check the slice table magic.
     */
    if (((u_char)buf[0x1fe] != 0x55) || ((u_char)buf[0x1ff] != 0xaa)) {
	/* If a slice number was explicitly supplied, this is an error */
	if (dev->d_kind.biosdisk.slice > 0) {
	    DEBUG("no slice table/MBR (no magic)");
	    return (ENOENT);
	}
	sector = 0;
	goto unsliced;		/* may be a floppy */
    }

    /*
     * copy the partition table, then pick up any extended partitions.
     */
    bcopy(buf + DOSPARTOFF, &od->od_slicetab,
      sizeof(struct dos_partition) * NDOSPART);
    od->od_nslices = 4;			/* extended slices start here */
    for (i = 0; i < NDOSPART; i++)
        bd_checkextended(od, i);
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
	    return (ENOENT);
        }
    }

    /*
     * Check for the historically bogus MBR found on true dedicated disks
     */
    if ((dptr[3].dp_typ == DOSPTYP_386BSD) &&
      (dptr[3].dp_start == 0) &&
      (dptr[3].dp_size == 50000)) {
        sector = 0;
        goto unsliced;
    }

    /* Try to auto-detect the best slice; this should always give a slice number */
    if (dev->d_kind.biosdisk.slice == 0) {
	slice = bd_bestslice(od);
        if (slice == -1) {
	    return (ENOENT);
        }
        dev->d_kind.biosdisk.slice = slice;
    }

    dptr = &od->od_slicetab[0];
    /*
     * Accept the supplied slice number unequivocally (we may be looking
     * at a DOS partition).
     */
    dptr += (dev->d_kind.biosdisk.slice - 1);	/* we number 1-4, offsets are 0-3 */
    sector = dptr->dp_start;
    DEBUG("slice entry %d at %d, %d sectors", dev->d_kind.biosdisk.slice - 1, sector, dptr->dp_size);

    /*
     * If we are looking at a BSD slice, and the partition is < 0, assume the 'a' partition
     */
    if ((dptr->dp_typ == DOSPTYP_386BSD) && (dev->d_kind.biosdisk.partition < 0))
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
	    return (EIO);
	}
	DEBUG("copy %d bytes of label from %p to %p", sizeof(struct disklabel), buf + LABELOFFSET, &od->od_disklabel);
	bcopy(buf + LABELOFFSET, &od->od_disklabel, sizeof(struct disklabel));
	lp = &od->od_disklabel;
	od->od_flags |= BD_LABELOK;

	if (lp->d_magic != DISKMAGIC) {
	    DEBUG("no disklabel");
	    return (ENOENT);
	}
	if (dev->d_kind.biosdisk.partition >= lp->d_npartitions) {
	    DEBUG("partition '%c' exceeds partitions in table (a-'%c')",
		  'a' + dev->d_kind.biosdisk.partition, 'a' + lp->d_npartitions);
	    return (EPART);
	}

#ifdef DISK_DEBUG
	/* Complain if the partition is unused unless this is a floppy. */
	if ((lp->d_partitions[dev->d_kind.biosdisk.partition].p_fstype == FS_UNUSED) &&
	    !(od->od_flags & BD_FLOPPY))
	    DEBUG("warning, partition marked as unused");
#endif
	
	od->od_boff = 
		lp->d_partitions[dev->d_kind.biosdisk.partition].p_offset -
		lp->d_partitions[RAW_PART].p_offset +
		sector;
    }
    return (0);
}

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
		if (od->od_nslices == NEXTDOSPART)
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
	}
	return (prefslice);
}

#ifdef LOADER_GPT_SUPPORT
static int
bd_open_gpt(struct open_disk *od, struct i386_devdesc *dev)
{
    struct dos_partition *dp;
    struct gpt_hdr *hdr;
    struct gpt_ent *ent;
    struct gpt_part *gp;
    int	entries_per_sec, error, i, part;
    daddr_t lba, elba;
    char gpt[BIOSDISK_SECSIZE], tbl[BIOSDISK_SECSIZE];

    /*
     * Following calculations attempt to determine the correct value
     * for d->od_boff by looking for the slice and partition specified,
     * or searching for reasonable defaults.
     */
    error = 0;

    /* First, read the MBR and see if we have a PMBR. */
    if (bd_read(od, 0, 1, tbl)) {
	DEBUG("error reading MBR");
	return (EIO);
    }

    /* Check the slice table magic. */
    if (((u_char)tbl[0x1fe] != 0x55) || ((u_char)tbl[0x1ff] != 0xaa))
	return (ENXIO);

    /* Check for GPT slice. */
    part = 0;
    dp = (struct dos_partition *)(tbl + DOSPARTOFF);
    for (i = 0; i < NDOSPART; i++) {
	if (dp[i].dp_typ == 0xee)
	    part++;
	else if ((part != 1) && (dp[i].dp_typ != 0x00))
	    return (EINVAL);
    }
    if (part != 1)
	return (EINVAL);

    /* Read primary GPT table header. */
    if (bd_read(od, 1, 1, gpt)) {
	DEBUG("error reading GPT header");
	return (EIO);
    }
    hdr = (struct gpt_hdr *)gpt;
    if (bcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0 ||
	hdr->hdr_lba_self != 1 || hdr->hdr_revision < 0x00010000 ||
	hdr->hdr_entsz < sizeof(*ent) ||
	BIOSDISK_SECSIZE % hdr->hdr_entsz != 0) {
	DEBUG("Invalid GPT header\n");
	return (EINVAL);
    }

    /* Now walk the partition table to count the number of valid partitions. */
    part = 0;
    entries_per_sec = BIOSDISK_SECSIZE / hdr->hdr_entsz;
    elba = hdr->hdr_lba_table + hdr->hdr_entries / entries_per_sec;
    for (lba = hdr->hdr_lba_table; lba < elba; lba++) {
	if (bd_read(od, lba, 1, tbl)) {
	    DEBUG("error reading GPT table");
	    return (EIO);
	}
	for (i = 0; i < entries_per_sec; i++) {
	    ent = (struct gpt_ent *)(tbl + i * hdr->hdr_entsz);
	    if (uuid_is_nil(&ent->ent_type, NULL) || ent->ent_lba_start == 0 ||
		ent->ent_lba_end < ent->ent_lba_start)
		continue;
	    part++;
	}
    }

    /* Save the important information about all the valid partitions. */
    od->od_nparts = part;
    if (part != 0) {
	od->od_partitions = malloc(part * sizeof(struct gpt_part));
	part = 0;	
	for (lba = hdr->hdr_lba_table; lba < elba; lba++) {
	    if (bd_read(od, lba, 1, tbl)) {
		DEBUG("error reading GPT table");
		error = EIO;
		goto out;
	    }
	    for (i = 0; i < entries_per_sec; i++) {
		ent = (struct gpt_ent *)(tbl + i * hdr->hdr_entsz);
		if (uuid_is_nil(&ent->ent_type, NULL) ||
		    ent->ent_lba_start == 0 ||
		    ent->ent_lba_end < ent->ent_lba_start)
		    continue;
		od->od_partitions[part].gp_index = (lba - hdr->hdr_lba_table) *
		    entries_per_sec + i + 1;
		od->od_partitions[part].gp_type = ent->ent_type;
		od->od_partitions[part].gp_start = ent->ent_lba_start;
		od->od_partitions[part].gp_end = ent->ent_lba_end;
		part++;
	    }
	}
    }
    od->od_flags |= BD_GPTOK;

    /* Is this a request for the whole disk? */
    if (dev->d_kind.biosdisk.slice < 0) {
	od->od_boff = 0;
	return (0);
    }

    /*
     * If a partition number was supplied, then the user is trying to use
     * an MBR address rather than a GPT address, so fail.
     */
    if (dev->d_kind.biosdisk.partition != 0xff) {
	error = ENOENT;
	goto out;
    }

    /* If a slice number was supplied but not found, this is an error. */
    gp = NULL;
    if (dev->d_kind.biosdisk.slice > 0) {
	for (i = 0; i < od->od_nparts; i++) {
	    if (od->od_partitions[i].gp_index == dev->d_kind.biosdisk.slice) {
		gp = &od->od_partitions[i];
		break;
	    }
	}
	if (gp == NULL) {
            DEBUG("partition %d not found", dev->d_kind.biosdisk.slice);
	    error = ENOENT;
	    goto out;
        }
    }

    /* Try to auto-detect the best partition. */
    if (dev->d_kind.biosdisk.slice == 0) {
	gp = bd_best_gptpart(od);
	if (gp == NULL) {
	    error = ENOENT;
	    goto out;
	}
	dev->d_kind.biosdisk.slice = gp->gp_index;
    }
    od->od_boff = gp->gp_start;

out:
    if (error) {
	if (od->od_nparts > 0)
	    free(od->od_partitions);
	od->od_flags &= ~BD_GPTOK;
    }
    return (error);
}

static struct gpt_part *
bd_best_gptpart(struct open_disk *od)
{
    struct gpt_part *gp, *prefpart;
    int i, pref, preflevel;
	
    prefpart = NULL;
    preflevel = PREF_NONE;

    gp = od->od_partitions;
    for (i = 0; i < od->od_nparts; i++, gp++) {
	/* Windows. XXX: Also Linux. */
	if (uuid_equal(&gp->gp_type, &ms_basic_data, NULL))
	    pref = PREF_DOS;
	/* FreeBSD */
	else if (uuid_equal(&gp->gp_type, &freebsd_ufs, NULL) ||
	    uuid_equal(&gp->gp_type, &freebsd_zfs, NULL))
	    pref = PREF_FBSD;
	else
	    pref = PREF_NONE;
	if (pref < preflevel) {
	    preflevel = pref;
	    prefpart = gp;
	}
    }
    return (prefpart);
}
#endif

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
#ifdef LOADER_GPT_SUPPORT
    if (od->od_flags & BD_GPTOK && od->od_nparts > 0)
	free(od->od_partitions);
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
    blks = size / BIOSDISK_SECSIZE;
    if (rsize)
	*rsize = 0;

    switch(rw){
    case F_READ:
	DEBUG("read %d from %lld to %p", blks, dblk, buf);

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
	break;
    case F_WRITE :
	DEBUG("write %d from %d to %p", blks, dblk, buf);

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
	break;
    default:
	/* DO NOTHING */
	return (EROFS);
    }

    if (rsize)
	*rsize = size;
    return (0);
}

/* Max number of sectors to bounce-buffer if the request crosses a 64k boundary */
#define FLOPPY_BOUNCEBUF	18

static int
bd_edd_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    static struct edd_packet packet;

    packet.len = 0x10;
    packet.count = blks;
    packet.offset = VTOPOFF(dest);
    packet.seg = VTOPSEG(dest);
    packet.lba = dblk;
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    if (write)
	/* Should we Write with verify ?? 0x4302 ? */
	v86.eax = 0x4300;
    else
	v86.eax = 0x4200;
    v86.edx = od->od_unit;
    v86.ds = VTOPSEG(&packet);
    v86.esi = VTOPOFF(&packet);
    v86int();
    return (v86.efl & 0x1);
}

static int
bd_chs_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, bpc, cyl, hd, sec;

    bpc = (od->od_sec * od->od_hds);	/* blocks per cylinder */
    x = dblk;
    cyl = x / bpc;			/* block # / blocks per cylinder */
    x %= bpc;				/* block offset into cylinder */
    hd = x / od->od_sec;		/* offset / blocks per track */
    sec = x % od->od_sec;		/* offset into track */

    /* correct sector number for 1-based BIOS numbering */
    sec++;

    if (cyl > 1023)
	/* CHS doesn't support cylinders > 1023. */
	return (1);

    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    if (write)
	v86.eax = 0x300 | blks;
    else
	v86.eax = 0x200 | blks;
    v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
    v86.edx = (hd << 8) | od->od_unit;
    v86.es = VTOPSEG(dest);
    v86.ebx = VTOPOFF(dest);
    v86int();
    return (v86.efl & 0x1);
}

static int
bd_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, sec, result, resid, retry, maxfer;
    caddr_t	p, xp, bbuf, breg;
    
    /* Just in case some idiot actually tries to read/write -1 blocks... */
    if (blks < 0)
	return (-1);

    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
    if (VTOP(dest) >> 20 != 0 || ((od->od_unit < 0x80) && 
	((VTOP(dest) >> 16) != (VTOP(dest + blks * BIOSDISK_SECSIZE) >> 16)))) {

	/* 
	 * There is a 64k physical boundary somewhere in the
	 * destination buffer, or the destination buffer is above
	 * first 1MB of physical memory so we have to arrange a
	 * suitable bounce buffer.  Allocate a buffer twice as large
	 * as we need to.  Use the bottom half unless there is a break
	 * there, in which case we use the top half.
	 */
	x = min(FLOPPY_BOUNCEBUF, (unsigned)blks);
	bbuf = alloca(x * 2 * BIOSDISK_SECSIZE);
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) ==
	    ((u_int32_t)VTOP(bbuf + x * BIOSDISK_SECSIZE) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BIOSDISK_SECSIZE;
	}
	maxfer = x;		/* limit transfers to bounce region size */
    } else {
	breg = bbuf = NULL;
	maxfer = 0;
    }
    
    while (resid > 0) {
	/*
	 * Play it safe and don't cross track boundaries.
	 * (XXX this is probably unnecessary)
	 */
	sec = dblk % od->od_sec;	/* offset into track */
	x = min(od->od_sec - sec, resid);
	if (maxfer > 0)
	    x = min(x, maxfer);		/* fit bounce buffer */

	/* where do we transfer to? */
	xp = bbuf == NULL ? p : breg;

	/*
	 * Put your Data In, Put your Data out,
	 * Put your Data In, and shake it all about 
	 */
	if (write && bbuf != NULL)
	    bcopy(p, breg, x * BIOSDISK_SECSIZE);

	/*
	 * Loop retrying the operation a couple of times.  The BIOS
	 * may also retry.
	 */
	for (retry = 0; retry < 3; retry++) {
	    /* if retrying, reset the drive */
	    if (retry > 0) {
		v86.ctl = V86_FLAGS;
		v86.addr = 0x13;
		v86.eax = 0;
		v86.edx = od->od_unit;
		v86int();
	    }

	    if (od->od_flags & BD_MODEEDD1)
		result = bd_edd_io(od, dblk, x, xp, write);
	    else
		result = bd_chs_io(od, dblk, x, xp, write);
	    if (result == 0)
		break;
	}

	if (write)
	    DEBUG("Write %d sector(s) from %p (0x%x) to %lld %s", x,
		p, VTOP(p), dblk, result ? "failed" : "ok");
	else
	    DEBUG("Read %d sector(s) from %lld to %p (0x%x) %s", x,
		dblk, p, VTOP(p), result ? "failed" : "ok");
	if (result) {
	    return(-1);
	}
	if (!write && bbuf != NULL)
	    bcopy(breg, p, x * BIOSDISK_SECSIZE);
	p += (x * BIOSDISK_SECSIZE);
	dblk += x;
	resid -= x;
    }

/*    hexdump(dest, (blks * BIOSDISK_SECSIZE)); */
    return(0);
}

static int
bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{

    return (bd_io(od, dblk, blks, dest, 0));
}

static int
bd_write(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{

    return (bd_io(od, dblk, blks, dest, 1));
}

static int
bd_getgeom(struct open_disk *od)
{

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

    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = 0x80 + bunit;
    v86int();
    if (v86.efl & 0x1)
	return 0x4f010f;
    return ((v86.ecx & 0xc0) << 18) | ((v86.ecx & 0xff00) << 8) |
	   (v86.edx & 0xff00) | (v86.ecx & 0x3f);
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

    biosdev = bd_unit2bios(dev->d_unit);
    DEBUG("unit %d BIOS device %d", dev->d_unit, biosdev);
    if (biosdev == -1)				/* not a BIOS device */
	return(-1);
    if (bd_opendisk(&od, dev) != 0)		/* oops, not a viable device */
	return(-1);

    if (biosdev < 0x80) {
	/* floppy (or emulated floppy) or ATAPI device */
	if (bdinfo[dev->d_unit].bd_type == DT_ATAPI) {
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
    unit = (biosdev & 0x7f) - unitofs;

    /* XXX a better kludge to set the root disk unit number */
    if ((nip = getenv("root_disk_unit")) != NULL) {
	i = strtol(nip, &cp, 0);
	/* check for parse error */
	if ((cp != nip) && (*cp == 0))
	    unit = i;
    }

    rootdev = MAKEBOOTDEV(major, dev->d_kind.biosdisk.slice + 1, unit,
	dev->d_kind.biosdisk.partition);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}
