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
 *	$Id: biosdisk.c,v 1.3 1998/09/18 01:12:46 msmith Exp $
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

#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)
#define	MAXBDDEV		MAXDEV

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
    u_char		od_buf[BUFSIZE];	/* transfer buffer (do we want/need this?) */
};

static int	bd_getgeom(struct open_disk *od);
static int	bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest);

static int	bd_edd3probe(int unit);
static int	bd_edd1probe(int unit);
static int	bd_int13probe(int unit);

static int	bd_init(void);
static int	bd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int	bd_open(struct open_file *f, void *vdev);
static int	bd_close(struct open_file *f);

struct devsw biosdisk = {
    "disk", 
    DEVT_DISK, 
    bd_init,
    bd_strategy, 
    bd_open, 
    bd_close, 
    noioctl
};

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct 
{
    int		bd_unit;		/* BIOS unit number */
    int		bd_flags;
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

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

    /* sequence 0, 0x80 */
    for (base = 0; base <= 0x80; base += 0x80) {
	for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
	    bdinfo[nbdinfo].bd_unit = -1;
	    bdinfo[nbdinfo].bd_flags = (unit < 0x80) ? BD_FLOPPY : 0;
	    
	    if (bd_edd3probe(unit)) {
		bdinfo[nbdinfo].bd_flags |= BD_MODEEDD3;
	    } else if (bd_edd1probe(unit)) {
		bdinfo[nbdinfo].bd_flags |= BD_MODEEDD1;
	    } else if (bd_int13probe(unit)) {
		bdinfo[nbdinfo].bd_flags |= BD_MODEINT13;
	    } else {
		break;
	    }
	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   (unit < 0x80) ? ('A' + unit) : ('C' + unit - 0x80), nbdinfo);
	    bdinfo[nbdinfo].bd_unit = unit;
	    nbdinfo++;
	}
    }
    return(0);
}

/* 
 * Try to detect a device supported by an Enhanced Disk Drive 3.0-compliant BIOS
 */
static int
bd_edd3probe(int unit)
{
    return(0);		/* XXX not implemented yet */
}

/* 
 * Try to detect a device supported by an Enhanced Disk Drive 1.1-compliant BIOS
 */
static int
bd_edd1probe(int unit)
{
    return(0);		/* XXX not implemented yet */
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */

static int
bd_int13probe(int unit)
{
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    v86.eax = 0x800;
    v86.edx = unit;
    v86int();
    
    if (!(v86.efl & 0x1) &&			/* carry clear */
	((v86.edx & 0xff) > unit & 0x7f))	/* unit # OK */
	return(1);
    return(0);
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
bd_open(struct open_file *f, void *vdev)
{
    struct i386_devdesc		*dev = (struct i386_devdesc *)vdev;
    struct dos_partition	*dptr;
    struct open_disk		*od;
    struct disklabel		*lp;
    int				sector, slice, i;
    int				error;

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
    if (bd_read(od, 0, 1, od->od_buf)) {
	DEBUG("error reading MBR");
	error = EIO;
	goto out;
    }

    /* 
     * Check the slice table magic.
     */
    if ((od->od_buf[0x1fe] != 0xff) || (od->od_buf[0x1ff] != 0xaa)) {
	/* If a slice number was explicitly supplied, this is an error */
	if (dev->d_kind.biosdisk.slice > 0) {
	    DEBUG("no slice table/MBR (no magic)");
	    error = ENOENT;
	    goto out;
	}
	sector = 0;
	goto unsliced;		/* may be a floppy */
    }
    dptr = (struct dos_partition *) & od->od_buf[DOSPARTOFF];

    /* 
     * XXX No support here for 'extended' slices
     */
    if (dev->d_kind.biosdisk.slice <= 0) {
	/*
	 * Search for the first FreeBSD slice; this also works on "unsliced"
	 * disks, as they contain a "historically bogus" MBR.
	 */
	for (i = 0; i < NDOSPART; i++, dptr++)
	    if (dptr->dp_typ == DOSPTYP_386BSD) {
		sector = dptr->dp_start;
		break;
	    }
	/* Did we find something? */
	if (sector == -1) {
	    error = ENOENT;
	    goto out;
	}
    } else {
	/*
	 * Accept the supplied slice number unequivocally (we may be looking
	 * for a DOS partition) if we can handle it.
	 */
	if ((dev->d_kind.biosdisk.slice > NDOSPART) || (dev->d_kind.biosdisk.slice < 1)) {
	    error = ENOENT;
	    goto out;
	}
	dptr += (dev->d_kind.biosdisk.slice - 1);
	sector = dptr->dp_start;
    }
 unsliced:
    /* 
     * Now we have the slice, look for the partition in the disklabel if we have
     * a partition to start with.
     *
     * XXX we might want to check the label checksum.
     */
    if (dev->d_kind.biosdisk.partition < 0) {
	od->od_boff = sector;		/* no partition, must be after the slice */
	DEBUG("opening raw slice");
    } else {
	
	if (bd_read(od, sector + LABELSECTOR, 1, od->od_buf)) {
	    DEBUG("error reading disklabel");
	    error = EIO;
	    goto out;
	}
	lp = (struct disklabel *) (od->od_buf + LABELOFFSET);
	if (lp->d_magic != DISKMAGIC) {
	    DEBUG("no disklabel");
	    error = ENOENT;
	    goto out;

	} else if (dev->d_kind.biosdisk.partition >= lp->d_npartitions) {

	    /*
	     * The partition supplied is out of bounds; this is fatal.
	     */
	    DEBUG("partition '%c' exceeds partitions in table (a-'%c')",
		     'a' + dev->d_kind.biosdisk.partition, 'a' + lp->d_npartitions);
	    error = EPART;
	    goto out;

	} else {

	    /*
	     * Complain if the partition type is wrong and it shouldn't be, but
	     * regardless accept this partition.
	     */
	    if ((lp->d_partitions[dev->d_kind.biosdisk.partition].p_fstype == FS_UNUSED) &&
		  !(od->od_flags & BD_FLOPPY))	    /* Floppies often have bogus fstype */
		DEBUG("warning, partition marked as unused");

	    od->od_boff = lp->d_partitions[dev->d_kind.biosdisk.partition].p_offset;
	}
    }
    /*
     * Save our context
     */
    ((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data = od;
    DEBUG("open_disk %p, partition at 0x%x", od, od->od_boff);

 out:
    if (error)
	free(od);
    return(error);
}

static int 
bd_close(struct open_file *f)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data);

#if 0
    DEBUG("open_disk %p", od);
#endif
#if 0
    /* XXX is this required? (especially if disk already open...) */
    if (od->od_flags & BD_FLOPPY)
	delay(3000000);
#endif
    free(od);
    return(0);
}

static int 
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size, void *buf, size_t *rsize)
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

#if 0
    DEBUG("open_disk %p", od);
#endif

    if (rw != F_READ)
	return(EROFS);


    blks = size / BIOSDISK_SECSIZE;
#if 0
    DEBUG("read %d from %d+%d to %p", blks, od->od_boff, dblk, buf);
#endif

    if (rsize)
	*rsize = 0;
    if (blks && bd_read(od, dblk + od->od_boff, blks, buf)) {
	DEBUG("read error");
	return (EIO);
    }
#ifdef BD_SUPPORT_FRAGS
#if 0
    D(printf("bd_strategy: frag read %d from %d+%d+d to %p\n", 
	     fragsize, od->od_boff, dblk, blks, buf + (blks * BIOSDISK_SECSIZE)));
#endif
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

static int
bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{
    int		x, bpc, cyl, hd, sec;
    
    bpc = (od->od_sec * od->od_hds);		/* blocks per cylinder */
    v86.ctl = V86_FLAGS;
    v86.addr = 0x13;
    
    while (blks != 0) {
	x = dblk;
	cyl = x / bpc;			/* block # / blocks per cylinder */
	x %= bpc;			/* block offset into cylinder */
	hd = x / od->od_sec;		/* offset / blocks per track */
	sec = x % od->od_sec;		/* offset into track */

	/* play it safe and don't cross track boundaries */
	x = min (od->od_sec - sec, blks);

	/* correct sector number for 1-based BIOS numbering */
	sec++;
	
	/* build request  XXX support EDD requests too */
	v86.eax = 0x200 | x;
	v86.ecx = ((cyl & 0xff) << 8) | ((cyl & 0x300) >> 2) | sec;
	v86.edx = (hd << 8) | od->od_unit;
	v86.es = VTOPSEG(dest);
	v86.ebx = VTOPOFF(dest);
	v86int();
	if (v86.efl & 0x1)
	    return(-1);
	
	dest + (x * BIOSDISK_SECSIZE);
	dblk += x;
	blks -= x;
    }
    return(0);
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
	((v86.edx & 0xff) <= (od->od_unit & 0x7f)))	/* unit # bad */
	return(1);
    
    /* convert max cyl # -> # of cylinders */
    od->od_cyl = ((v86.ecx & 0xc0) << 2) + ((v86.ecx & 0xff00) >> 8) + 1;
    /* convert max head # -> # of heads */
    od->od_hds = ((v86.edx & 0xff00) >> 8) + 1;
    od->od_sec = v86.ecx & 0x3f;
    return(0);
}
