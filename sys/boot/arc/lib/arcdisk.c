/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Doug Rabson <dfr@freebsd.org>
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
 * $FreeBSD: src/sys/boot/arc/lib/arcdisk.c,v 1.3 1999/12/08 09:32:40 phk Exp $
 */

/*
 * ARC disk device handling.
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

#include <machine/stdarg.h>

#include "bootstrap.h"
#include "libarc.h"
#include "arctypes.h"
#include "arcfuncs.h"

#define ARCDISK_SECSIZE	512

#define BUFSIZE		(1 * ARCDISK_SECSIZE)
#define	MAXBDDEV	MAXDEV

#ifdef DISK_DEBUG
# define D(x)	x
#else
# define D(x)
#endif

static int	bd_init(void);
static int	bd_strategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int	bd_realstrategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int	bd_open(struct open_file *f, void *vdev);
static int	bd_close(struct open_file *f);
static void	bd_print(int verbose);

struct open_disk {
    int			od_fd;
    int			od_unit;		/* our unit number */
    int			od_boff;		/* block offset from beginning of ARC disk */
    int			od_flags;
#define BD_FLOPPY	(1<<2)
    u_char		od_buf[BUFSIZE];	/* transfer buffer (do we want/need this?) */
};

struct devsw arcdisk = {
    "disk", 
    DEVT_DISK, 
    bd_init,
    bd_strategy, 
    bd_open, 
    bd_close, 
    noioctl,
    bd_print
};

/*
 * List of ARC devices, translation from disk unit number to
 * ARC unit number.
 */
static struct 
{
    char	bd_name[64];
    int		bd_unit;		/* ARC unit number */
    int		bd_namelen;
    int		bd_flags;
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

/*    
 * Quiz ARC for disk devices, save a little info about them.
 */
static int
bd_init(void) 
{
    nbdinfo++;

    return (0);
}

/*
 * Print information about disks
 */
static void
bd_print(int verbose)
{
    int		i;
    char	line[80];
    
    for (i = 0; i < nbdinfo; i++) {
	sprintf(line, "    disk%d:   ARC drive %s", i, bdinfo[i].bd_name);
	pager_output(line);
	/* XXX more detail? */
	pager_output("\n");
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
bd_open(struct open_file *f, void *vdev)
{
    struct arc_devdesc		*dev = vdev;
    struct dos_partition	*dptr;
    struct open_disk		*od;
    struct disklabel		*lp;
    int				sector, slice, i;
    int				error;
    int				unit;
    u_int32_t			fd;

    unit = dev->d_kind.arcdisk.unit;
    if (unit >= nbdinfo) {
	D(printf("attempt to open nonexistent disk\n"));
	return(ENXIO);
    }
    
    if (Open("scsi(0)disk(0)rdisk(0)partition(0)",
	     OpenReadOnly, &fd) != ESUCCESS)
      if (Open("scsi(0)disk(1)rdisk(0)partition(0)",
	       OpenReadOnly, &fd) != ESUCCESS)
	if (Open("multi(0)disk(0)fdisk(0)partition(0)",
		 OpenReadOnly, &fd) != ESUCCESS)
	return(ENXIO);

    od = (struct open_disk *) malloc(sizeof(struct open_disk));
    if (!od) {
	D(printf("arcdiskopen: no memory\n"));
	return (ENOMEM);
    }

    /* Look up ARC unit number, intialise open_disk structure */
    od->od_fd = fd;
    od->od_unit = dev->d_kind.arcdisk.unit;
    od->od_flags = bdinfo[od->od_unit].bd_flags;
    od->od_boff = 0;
    error = 0;

#if 0
    /* Get geometry for this open (removable device may have changed) */
    if (set_geometry(&od->od_ll)) {
	D(printf("bd_open: can't get geometry\n"));
	error = ENXIO;
	goto out;
    }
#endif

    /*
     * Following calculations attempt to determine the correct value
     * for d->od_boff by looking for the slice and partition specified,
     * or searching for reasonable defaults.
     */

#if 0
    /*
     * Find the slice in the DOS slice table.
     */
    if (readsects(&od->od_ll, 0, 1, od->od_buf, 0)) {
	D(printf("bd_open: error reading MBR\n"));
	error = EIO;
	goto out;
    }

    /* 
     * Check the slice table magic.
     */
    if ((od->od_buf[0x1fe] != 0xff) || (od->od_buf[0x1ff] != 0xaa)) {
	/* If a slice number was explicitly supplied, this is an error */
	if (dev->d_kind.arcdisk.slice > 0) {
	    D(printf("bd_open: no slice table/MBR (no magic)\n"));
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
    if (dev->d_kind.arcdisk.slice <= 0) {
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
	if ((dev->d_kind.arcdisk.slice > NDOSPART) || (dev->d_kind.arcdisk.slice < 1)) {
	    error = ENOENT;
	    goto out;
	}
	dptr += (dev->d_kind.arcdisk.slice - 1);
	sector = dptr->dp_start;
    }
 unsliced:

#else
    sector = 0;
#endif
    /* 
     * Now we have the slice, look for the partition in the disklabel if we have
     * a partition to start with.
     */
    if (dev->d_kind.arcdisk.partition < 0) {
	od->od_boff = sector;		/* no partition, must be after the slice */
    } else {
	if (bd_strategy(od, F_READ, sector + LABELSECTOR, 512, od->od_buf, 0)) {
	    D(printf("bd_open: error reading disklabel\n"));
	    error = EIO;
	    goto out;
	}
	lp = (struct disklabel *) (od->od_buf + LABELOFFSET);
	if (lp->d_magic != DISKMAGIC) {
	    D(printf("bd_open: no disklabel\n"));
	    error = ENOENT;
	    goto out;

	} else if (dev->d_kind.arcdisk.partition >= lp->d_npartitions) {

	    /*
	     * The partition supplied is out of bounds; this is fatal.
	     */
	    D(printf("partition '%c' exceeds partitions in table (a-'%c')\n",
		     'a' + dev->d_kind.arcdisk.partition, 'a' + lp->d_npartitions));
	    error = EPART;
	    goto out;

	} else {

	    /*
	     * Complain if the partition type is wrong and it shouldn't be, but
	     * regardless accept this partition.
	     */
	    D(if ((lp->d_partitions[dev->d_kind.arcdisk.partition].p_fstype == FS_UNUSED) &&
		  !(od->od_flags & BD_FLOPPY))	    /* Floppies often have bogus fstype */
	      printf("bd_open: warning, partition marked as unused\n"););

	    od->od_boff = lp->d_partitions[dev->d_kind.arcdisk.partition].p_offset;
	}
    }
    /*
     * Save our context
     */
    f->f_devdata = od;

 out:
    if (error)
	free(od);
    return(error);
}

static int 
bd_close(struct open_file *f)
{
    struct open_disk	*od = f->f_devdata;

    Close(od->od_fd);

    free(od);
    f->f_devdata = NULL;
    return(0);
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
bd_realstrategy(void *devdata, int flag, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
    struct open_disk	*od = (struct open_disk *)devdata;
    fpos_t		seek;
    u_int32_t		count;

    if (size % ARCDISK_SECSIZE)
	panic("bd_strategy: I/O not block multiple");

    if (flag != F_READ)
	return(EROFS);

    if (rsize)
	*rsize = 0;

    seek = 512 * (dblk + od->od_boff);
    Seek(od->od_fd, &seek, SeekAbsolute);
    if (Read(od->od_fd, buf, size, &count) != ESUCCESS) {
	D(printf("read error\n"));
	return (EIO);
    }

    if (rsize)
	*rsize = count;
    return (0);
}

