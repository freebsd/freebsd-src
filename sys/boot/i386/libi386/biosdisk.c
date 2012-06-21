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

#include <sys/disk.h>
#include <stand.h>
#include <machine/bootinfo.h>

#include <part.h>
#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include <edd.h>
#include "libi386.h"

#define BIOS_NUMDRIVES		0x475
#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)

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
	int		od_dkunit;	/* disk unit number */
	int		od_unit;	/* BIOS unit number */
	int		od_slice;	/* slice number of the parent table */
	daddr_t		od_boff;	/* offset from the beginning */
	daddr_t		od_size;	/* disk or partition size */
	struct ptable	*od_ptable;
};

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bdinfo
{
	int		bd_unit;	/* BIOS unit number */
	int		bd_cyl;		/* BIOS geometry */
	int		bd_hds;
	int		bd_sec;
	int		bd_flags;
#define	BD_MODEINT13	0x0000
#define	BD_MODEEDD1	0x0001
#define	BD_MODEEDD3	0x0002
#define	BD_MODEMASK	0x0003
#define	BD_FLOPPY	0x0004
	int		bd_type;	/* BIOS 'drive type' (floppy only) */
	uint16_t	bd_sectorsize;	/* Sector size */
	uint64_t	bd_sectors;	/* Disk size */
	struct ptable	*bd_ptable;	/* Partition table */
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

#define	BD(od)		(bdinfo[(od)->od_dkunit])
#define	BDSZ(od)	(bdinfo[(od)->od_dkunit].bd_sectors)
#define	BDSECSZ(od)	(bdinfo[(od)->od_dkunit].bd_sectorsize)

static int bd_read(struct open_disk *od, daddr_t dblk, int blks,
    caddr_t dest);
static int bd_write(struct open_disk *od, daddr_t dblk, int blks,
    caddr_t dest);
static int bd_int13probe(struct bdinfo *bd);

static int bd_init(void);
static void bd_cleanup(void);
static int bd_strategy(void *devdata, int flag, daddr_t dblk, size_t size,
    char *buf, size_t *rsize);
static int bd_realstrategy(void *devdata, int flag, daddr_t dblk,
    size_t size, char *buf, size_t *rsize);
static int bd_open(struct open_file *f, ...);
static int bd_close(struct open_file *f);
static int bd_ioctl(struct open_file *f, u_long cmd, void *data);
static void bd_print(int verbose);

struct bd_print_args {
	struct open_disk	*od;
	const char		*prefix;
	int			verbose;
};

struct devsw biosdisk = {
	"disk",
	DEVT_DISK,
	bd_init,
	bd_strategy,
	bd_open,
	bd_close,
	bd_ioctl,
	bd_print,
	bd_cleanup
};

static int bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev);
static void bd_closedisk(struct open_disk *od);

/*
 * Translate between BIOS device numbers and our private unit numbers.
 */
int
bd_bios2unit(int biosdev)
{
	int i;

	DEBUG("looking for bios device 0x%x", biosdev);
	for (i = 0; i < nbdinfo; i++) {
		DEBUG("bd unit %d is BIOS device 0x%x", i, bdinfo[i].bd_unit);
		if (bdinfo[i].bd_unit == biosdev)
			return (i);
	}
	return (-1);
}

int
bd_unit2bios(int unit)
{

	if ((unit >= 0) && (unit < nbdinfo))
		return (bdinfo[unit].bd_unit);
	return (-1);
}

/*
 * Quiz the BIOS for disk devices, save a little info about them.
 */
static int
bd_init(void)
{
	int base, unit, nfd = 0;

	/* sequence 0, 0x80 */
	for (base = 0; base <= 0x80; base += 0x80) {
		for (unit = base; (nbdinfo < MAXBDDEV); unit++) {
#ifndef VIRTUALBOX
			/*
			 * Check the BIOS equipment list for number
			 * of fixed disks.
			 */
			if(base == 0x80 &&
			    (nfd >= *(unsigned char *)PTOV(BIOS_NUMDRIVES)))
				break;
#endif
			bdinfo[nbdinfo].bd_unit = unit;
			bdinfo[nbdinfo].bd_flags = unit < 0x80 ? BD_FLOPPY: 0;
			if (!bd_int13probe(&bdinfo[nbdinfo]))
				break;

			/* XXX we need "disk aliases" to make this simpler */
			printf("BIOS drive %c: is disk%d\n", (unit < 0x80) ?
			    ('A' + unit): ('C' + unit - 0x80), nbdinfo);
			nbdinfo++;
			if (base == 0x80)
				nfd++;
		}
	}
	return(0);
}

static void
bd_cleanup(void)
{
	int i;

	for (i = 0; i < nbdinfo; i++) {
		if (bdinfo[i].bd_ptable != NULL) {
			ptable_close(bdinfo[i].bd_ptable);
			bdinfo[i].bd_ptable = NULL;
		}
	}
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */
static int
bd_int13probe(struct bdinfo *bd)
{
	struct edd_params params;

	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x800;
	v86.edx = bd->bd_unit;
	v86int();

	if (V86_CY(v86.efl) ||	/* carry set */
	    (v86.ecx & 0x3f) == 0 || /* absurd sector number */
	    (v86.edx & 0xff) <= (unsigned)(bd->bd_unit & 0x7f))	/* unit # bad */
		return (0);	/* skip device */

	/* Convert max cyl # -> # of cylinders */
	bd->bd_cyl = ((v86.ecx & 0xc0) << 2) + ((v86.ecx & 0xff00) >> 8) + 1;
	/* Convert max head # -> # of heads */
	bd->bd_hds = ((v86.edx & 0xff00) >> 8) + 1;
	bd->bd_sec = v86.ecx & 0x3f;
	bd->bd_type = v86.ebx & 0xff;
	bd->bd_flags |= BD_MODEINT13;

	/* Calculate sectors count from the geometry */
	bd->bd_sectors = bd->bd_cyl * bd->bd_hds * bd->bd_sec;
	bd->bd_sectorsize = BIOSDISK_SECSIZE;
	DEBUG("unit 0x%x geometry %d/%d/%d", bd->bd_unit, bd->bd_cyl,
	    bd->bd_hds, bd->bd_sec);

	/* Determine if we can use EDD with this device. */
	v86.eax = 0x4100;
	v86.edx = bd->bd_unit;
	v86.ebx = 0x55aa;
	v86int();
	if (V86_CY(v86.efl) ||	/* carry set */
	    (v86.ebx & 0xffff) != 0xaa55 || /* signature */
	    (v86.ecx & EDD_INTERFACE_FIXED_DISK) == 0)
		return (1);
	/* EDD supported */
	bd->bd_flags |= BD_MODEEDD1;
	if ((v86.eax & 0xff00) >= 0x3000)
		bd->bd_flags |= BD_MODEEDD3;
	/* Get disk params */
	params.len = sizeof(struct edd_params);
	v86.ctl = V86_FLAGS;
	v86.addr = 0x13;
	v86.eax = 0x4800;
	v86.edx = bd->bd_unit;
	v86.ds = VTOPSEG(&params);
	v86.esi = VTOPOFF(&params);
	v86int();
	if (!V86_CY(v86.efl)) {
		bd->bd_sectors = params.sectors;
		bd->bd_sectorsize = params.sector_size;
	}
	return (1);
}

/* Convert the size to a human-readable number. */
static char *
display_size(uint64_t size, uint16_t sectorsize)
{
	static char buf[80];
	char unit;

	size = size * sectorsize / 1024;
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

static void
printpartition(void *arg, const char *pname, const struct ptable_entry *part)
{
	struct bd_print_args *pa, bsd;
	struct i386_devdesc dev;
	char line[80];

	pa = (struct bd_print_args *)arg;
	sprintf(line, "  %s%s: %s %s\n", pa->prefix, pname,
	    parttype2str(part->type), pa->verbose == 0 ? "":
	    display_size(part->end - part->start + 1, BDSECSZ(pa->od)));
	pager_output(line);
	if (part->type == PART_FREEBSD) {
		/* Open slice with BSD label */
		dev.d_unit = pa->od->od_dkunit;
		dev.d_kind.biosdisk.slice = part->index;
		dev.d_kind.biosdisk.partition = -1;
		if (!bd_opendisk(&bsd.od, &dev)) {
			sprintf(line, "  %s%s", pa->prefix, pname);
			bsd.prefix = line;
			bsd.verbose = pa->verbose;
			ptable_iterate(bsd.od->od_ptable, &bsd,
			    printpartition);
			bd_closedisk(bsd.od);
		}
	}
}
/*
 * Print information about disks
 */
static void
bd_print(int verbose)
{
	struct bd_print_args pa;
	static char line[80];
	struct i386_devdesc dev;
	int i;

	for (i = 0; i < nbdinfo; i++) {
		sprintf(line, "    disk%d:   BIOS drive %c:\n", i,
		    (bdinfo[i].bd_unit < 0x80) ? ('A' + bdinfo[i].bd_unit):
		    ('C' + bdinfo[i].bd_unit - 0x80));
		pager_output(line);

		/* try to open the whole disk */
		dev.d_unit = i;
		dev.d_kind.biosdisk.slice = -1;
		dev.d_kind.biosdisk.partition = -1;
		if (!bd_opendisk(&pa.od, &dev)) {
			sprintf(line, "    disk%d", i);
			pa.prefix = line;
			pa.verbose = verbose;
			ptable_iterate(pa.od->od_ptable, &pa,
			    printpartition);
			bd_closedisk(pa.od);
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
	struct i386_devdesc *dev;
	struct open_disk *od;
	va_list ap;
	int error;

	va_start(ap, f);
	dev = va_arg(ap, struct i386_devdesc *);
	va_end(ap);
	if ((error = bd_opendisk(&od, dev)) != 0)
		return (error);

	/* Save our context */
	((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data = od;
	DEBUG("open_disk %p, partition at 0x%x", od, od->od_boff);
	return (0);
}

static int
diskread(void *dev, void *buf, size_t blocks, off_t offset)
{
	struct open_disk *od = dev;

	return (bd_read(dev, offset + od->od_boff, blocks, buf));
}

static int
bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev)
{
	struct open_disk *od;
	struct ptable_entry part;
	int error;

	if (dev->d_unit >= nbdinfo) {
		DEBUG("attempt to open nonexistent disk");
		return (ENXIO);
	}
	od = (struct open_disk *)malloc(sizeof(struct open_disk));
	if (!od) {
		DEBUG("no memory");
		return (ENOMEM);
	}

	/* Look up BIOS unit number, initalize open_disk structure */
	od->od_dkunit = dev->d_unit;
	od->od_unit = bdinfo[od->od_dkunit].bd_unit;
	od->od_ptable = bdinfo[od->od_dkunit].bd_ptable;
	od->od_slice = 0;
	od->od_boff = 0;
	error = 0;
	DEBUG("open '%s', unit 0x%x slice %d partition %d",
	    i386_fmtdev(dev), dev->d_unit, dev->d_kind.biosdisk.slice,
	    dev->d_kind.biosdisk.partition);

	/* Determine disk layout. */
	if (od->od_ptable == NULL) {
		od->od_ptable = ptable_open(od, BDSZ(od), BDSECSZ(od),
		    diskread);
		if (od->od_ptable == NULL) {
			DEBUG("Can't read partition table");
			error = ENXIO;
			goto out;
		}
		/* Save the result */
		bdinfo[od->od_dkunit].bd_ptable = od->od_ptable;
	}
	/*
	 * What we want to open:
	 * a whole disk:
	 *	slice = -1
	 *
	 * a MBR slice:
	 *	slice = 1 .. 4
	 *	partition = -1
	 *
	 * an EBR slice:
	 *	slice = 5 .. N
	 *	partition = -1
	 *
	 * a GPT partition:
	 *	slice = 1 .. N
	 *	partition = 255
	 *
	 * BSD partition within an MBR slice:
	 *	slice = 1 .. N
	 *	partition = 0 .. 19
	 */
	if (dev->d_kind.biosdisk.slice > 0) {
		/* Try to get information about partition */
		error = ptable_getpart(od->od_ptable, &part,
		    dev->d_kind.biosdisk.slice);
		if (error != 0) /* Partition isn't exists */
			goto out;
		/* Adjust open_disk's offset within the biosdisk */
		od->od_boff = part.start;
		if (dev->d_kind.biosdisk.partition == 255)
			goto out; /* Nothing more to do */

		/* Try to read BSD label */
		od->od_ptable = ptable_open(od, part.end - part.start + 1,
		    BDSECSZ(od), diskread);
		if (od->od_ptable == NULL) {
			DEBUG("Can't read BSD label");
			error = ENXIO;
			/* Keep parent partition table opened */
			goto out;
		}
		/* Save the slice number of the parent partition */
		od->od_slice = part.index;
		if (dev->d_kind.biosdisk.partition == -1)
			goto out; /* Nothing more to do */
		error = ptable_getpart(od->od_ptable, &part,
		    dev->d_kind.biosdisk.partition);
		if (error != 0) {
			/*
			 * Keep parent partition table opened, but
			 * close this one (BSD label).
			 */
			ptable_close(od->od_ptable);
			goto out;
		}
		/* Adjust open_disk's offset within the biosdisk */
		od->od_boff += part.start;
	} else if (dev->d_kind.biosdisk.slice == 0) {
		error = ptable_getbestpart(od->od_ptable, &part);
		if (error != 0)
			goto out;
		/* Save the slice number of best partition to dev */
		dev->d_kind.biosdisk.slice = part.index;
		od->od_boff = part.start;
	}
out:
	if (error != 0) {
		free(od);
	} else {
		*odp = od;	/* return the open disk */
	}
	return (error);
}

static int
bd_close(struct open_file *f)
{
	struct open_disk *od;

	od = (struct open_disk *)
	    (((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data);
	bd_closedisk(od);
	return (0);
}

static int
bd_ioctl(struct open_file *f, u_long cmd, void *data)
{
	struct open_disk *od;

	od = (struct open_disk *)
	    (((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data);
	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = BDSECSZ(od);
		break;
	case DIOCGMEDIASIZE:
		*(off_t *)data = BDSZ(od) * BDSECSZ(od);
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

static void
bd_closedisk(struct open_disk *od)
{

	DEBUG("close_disk %p", od);
	/* Close only nested ptables */
	if (od->od_slice != 0 && od->od_ptable != NULL)
		ptable_close(od->od_ptable);
	free(od);
}

static int
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
	struct bcache_devdata bcd;
	struct open_disk *od;

	od = (struct open_disk *)
	    (((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);
	bcd.dv_strategy = bd_realstrategy;
	bcd.dv_devdata = devdata;
	return (bcache_strategy(&bcd, od->od_unit, rw, dblk + od->od_boff,
	    size, buf, rsize));
}

static int 
bd_realstrategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);
    int			blks;
#ifdef BD_SUPPORT_FRAGS /* XXX: sector size */
    char		fragbuf[BIOSDISK_SECSIZE];
    size_t		fragsize;

    fragsize = size % BIOSDISK_SECSIZE;
#else
    if (size % BDSECSZ(od))
	panic("bd_strategy: %d bytes I/O not multiple of block size", size);
#endif

    DEBUG("open_disk %p", od);
    blks = size / BDSECSZ(od);
    if (rsize)
	*rsize = 0;

    switch(rw){
    case F_READ:
	DEBUG("read %d from %lld to %p", blks, dblk, buf);

	if (blks && bd_read(od, dblk, blks, buf)) {
	    DEBUG("read error");
	    return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS /* XXX: sector size */
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

    packet.len = sizeof(struct edd_packet);
    packet.count = blks;
    packet.off = VTOPOFF(dest);
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
    return (V86_CY(v86.efl));
}

static int
bd_chs_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, bpc, cyl, hd, sec;

    bpc = BD(od).bd_sec * BD(od).bd_hds;	/* blocks per cylinder */
    x = dblk;
    cyl = x / bpc;			/* block # / blocks per cylinder */
    x %= bpc;				/* block offset into cylinder */
    hd = x / BD(od).bd_sec;		/* offset / blocks per track */
    sec = x % BD(od).bd_sec;		/* offset into track */

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
    return (V86_CY(v86.efl));
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
	((VTOP(dest) >> 16) != (VTOP(dest + blks * BDSECSZ(od)) >> 16)))) {

	/* 
	 * There is a 64k physical boundary somewhere in the
	 * destination buffer, or the destination buffer is above
	 * first 1MB of physical memory so we have to arrange a
	 * suitable bounce buffer.  Allocate a buffer twice as large
	 * as we need to.  Use the bottom half unless there is a break
	 * there, in which case we use the top half.
	 */
	x = min(FLOPPY_BOUNCEBUF, (unsigned)blks);
	bbuf = alloca(x * 2 * BDSECSZ(od));
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) ==
	    ((u_int32_t)VTOP(bbuf + x * BDSECSZ(od)) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BDSECSZ(od);
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
	sec = dblk % BD(od).bd_sec;	/* offset into track */
	x = min(BD(od).bd_sec - sec, resid);
	if (maxfer > 0)
	    x = min(x, maxfer);		/* fit bounce buffer */

	/* where do we transfer to? */
	xp = bbuf == NULL ? p : breg;

	/*
	 * Put your Data In, Put your Data out,
	 * Put your Data In, and shake it all about 
	 */
	if (write && bbuf != NULL)
	    bcopy(p, breg, x * BDSECSZ(od));

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

	    if (BD(od).bd_flags & BD_MODEEDD1)
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
	    bcopy(breg, p, x * BDSECSZ(od));
	p += (x * BDSECSZ(od));
	dblk += x;
	resid -= x;
    }

/*    hexdump(dest, (blks * BDSECSZ(od))); */
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
    if (V86_CY(v86.efl))
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
    int				i, unit;

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
	    /* assume an IDE disk */
	    major = WDMAJOR;
    }
    /* default root disk unit number */
    unit = biosdev & 0x7f;

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
