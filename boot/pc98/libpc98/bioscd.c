/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2001 John H. Baldwin <jhb@FreeBSD.org>
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
 * BIOS CD device handling for CD's that have been booted off of via no
 * emulation booting as defined in the El Torito standard.
 * 
 * Ideas and algorithms from:
 *
 * - FreeBSD libi386/biosdisk.c
 *
 */

#include <stand.h>

#include <sys/param.h>
#include <machine/bootinfo.h>

#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#define BIOSCD_SECSIZE		2048
#define BUFSIZE			(1 * BIOSCD_SECSIZE)
#define	MAXBCDEV		1

/* Major numbers for devices we frontend for. */
#define ACDMAJOR		117
#define	CDMAJOR			15

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

struct specification_packet {
	u_char	sp_size;
	u_char	sp_bootmedia;
	u_char	sp_drive;
	u_char	sp_controller;
	u_int	sp_lba;
	u_short	sp_devicespec;
	u_short	sp_buffersegment;
	u_short	sp_loadsegment;
	u_short	sp_sectorcount;
	u_short	sp_cylsec;
	u_char	sp_head;
};

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bcinfo {
	int	bc_unit;		/* BIOS unit number */
	struct specification_packet bc_sp;
} bcinfo [MAXBCDEV];
static int nbcinfo = 0;

static int	bc_read(int unit, daddr_t dblk, int blks, caddr_t dest);
static int	bc_init(void);
static int	bc_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	bc_open(struct open_file *f, ...);
static int	bc_close(struct open_file *f);
static void	bc_print(int verbose);

struct devsw bioscd = {
	"cd", 
	DEVT_CD, 
	bc_init,
	bc_strategy, 
	bc_open, 
	bc_close, 
	noioctl,
	bc_print,
	NULL
};

/*
 * Translate between BIOS device numbers and our private unit numbers.
 */
int
bc_bios2unit(int biosdev)
{
	int i;
    
	DEBUG("looking for bios device 0x%x", biosdev);
	for (i = 0; i < nbcinfo; i++) {
		DEBUG("bc unit %d is BIOS device 0x%x", i, bcinfo[i].bc_unit);
		if (bcinfo[i].bc_unit == biosdev)
			return(i);
	}
	return(-1);
}

int
bc_unit2bios(int unit)
{
	if ((unit >= 0) && (unit < nbcinfo))
		return(bcinfo[unit].bc_unit);
	return(-1);
}

/*    
 * We can't quiz, we have to be told what device to use, so this functoin
 * doesn't do anything.  Instead, the loader calls bc_add() with the BIOS
 * device number to add.
 */
static int
bc_init(void) 
{

	return (0);
}

int
bc_add(int biosdev)
{

	if (nbcinfo >= MAXBCDEV)
		return (-1);
	bcinfo[nbcinfo].bc_unit = biosdev;

	/* SCSI CD-ROM only */
	if ((biosdev & 0xf0) != 0xa0)
		return (-1);
	if ((((uint32_t *)PTOV(0xA1460))[biosdev & 0x0f] & 0x1f) != 5)
		return (-1);

	printf("BIOS CD is cd%d\n", nbcinfo);
	nbcinfo++;
	return(0);
}

/*
 * Print information about disks
 */
static void
bc_print(int verbose)
{
	char line[80];
	int i;

	for (i = 0; i < nbcinfo; i++) {
		sprintf(line, "    cd%d: Device 0x%x\n", i,
		    bcinfo[i].bc_sp.sp_devicespec);
		pager_output(line);
	}
}

/*
 * Attempt to open the disk described by (dev) for use by (f).
 */
static int 
bc_open(struct open_file *f, ...)
{
	va_list ap;
	struct i386_devdesc *dev;

	va_start(ap, f);
	dev = va_arg(ap, struct i386_devdesc *);
	va_end(ap);
	if (dev->d_unit >= nbcinfo) {
		DEBUG("attempt to open nonexistent disk");
		return(ENXIO);
	}

	return(0);
}
 
static int 
bc_close(struct open_file *f)
{

	return(0);
}

static int 
bc_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf,
    size_t *rsize)
{
	struct i386_devdesc *dev;
	int unit;
	int blks;
#ifdef BD_SUPPORT_FRAGS
	char fragbuf[BIOSCD_SECSIZE];
	size_t fragsize;

	fragsize = size % BIOSCD_SECSIZE;
#else
	if (size % BIOSCD_SECSIZE)
		return (EINVAL);
#endif

	if (rw != F_READ)
		return(EROFS);
	dev = (struct i386_devdesc *)devdata;
	unit = dev->d_unit;
	blks = size / BIOSCD_SECSIZE;
	if (dblk % (BIOSCD_SECSIZE / DEV_BSIZE) != 0)
		return (EINVAL);
	dblk /= (BIOSCD_SECSIZE / DEV_BSIZE);
	DEBUG("read %d from %lld to %p", blks, dblk, buf);

	if (rsize)
		*rsize = 0;
	if (blks && bc_read(unit, dblk, blks, buf)) {
		DEBUG("read error");
		return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS
	DEBUG("frag read %d from %lld+%d to %p", 
	    fragsize, dblk, blks, buf + (blks * BIOSCD_SECSIZE));
	if (fragsize && bc_read(unit, dblk + blks, 1, fragbuf)) {
		DEBUG("frag read error");
		return(EIO);
	}
	bcopy(fragbuf, buf + (blks * BIOSCD_SECSIZE), fragsize);
#endif	
	if (rsize)
		*rsize = size;
	return (0);
}

/* Max number of sectors to bounce-buffer at a time. */
#define	CD_BOUNCEBUF	8

static int
bc_read(int unit, daddr_t dblk, int blks, caddr_t dest)
{
	u_int maxfer, resid, result, retry, x;
	caddr_t bbuf, p, xp;
	int biosdev;
#ifdef DISK_DEBUG
	int error;
#endif
    
	/* Just in case some idiot actually tries to read -1 blocks... */
	if (blks < 0)
		return (-1);

	/* If nothing to do, just return succcess. */
	if (blks == 0)
		return (0);

	/* Decide whether we have to bounce */
	if (VTOP(dest) >> 20 != 0) {
		/* 
		 * The destination buffer is above first 1MB of
		 * physical memory so we have to arrange a suitable
		 * bounce buffer.
		 */
		x = min(CD_BOUNCEBUF, (unsigned)blks);
		bbuf = alloca(x * BIOSCD_SECSIZE);
		maxfer = x;
	} else {
		bbuf = NULL;
		maxfer = 0;
	}
	
	biosdev = bc_unit2bios(unit);
	resid = blks;
	p = dest;

	while (resid > 0) {
		if (bbuf)
			xp = bbuf;
		else
			xp = p;
		x = resid;
		if (maxfer > 0)
			x = min(x, maxfer);

		/*
		 * Loop retrying the operation a couple of times.  The BIOS
		 * may also retry.
		 */
		for (retry = 0; retry < 3; retry++) {
			/* If retrying, reset the drive */
			if (retry > 0) {
				v86.ctl = V86_FLAGS;
				v86.addr = 0x1b;
				v86.eax = 0x0300 | biosdev;
				v86int();
			}

			v86.ctl = V86_FLAGS;
			v86.addr = 0x1b;
			v86.eax = 0x0600 | (biosdev & 0x7f);
			v86.ebx = x * BIOSCD_SECSIZE;
			v86.ecx = dblk & 0xffff;
			v86.edx = (dblk >> 16) & 0xffff;
			v86.ebp = VTOPOFF(xp);
			v86.es = VTOPSEG(xp);
			v86int();
			result = V86_CY(v86.efl);
			if (result == 0)
				break;
		}
	
#ifdef DISK_DEBUG
		error = (v86.eax >> 8) & 0xff;
#endif
		DEBUG("%d sectors from %lld to %p (0x%x) %s", x, dblk, p,
		    VTOP(p), result ? "failed" : "ok");
		DEBUG("unit %d  status 0x%x", unit, error);
		if (bbuf != NULL)
			bcopy(bbuf, p, x * BIOSCD_SECSIZE);
		p += (x * BIOSCD_SECSIZE);
		dblk += x;
		resid -= x;
	}
	
/*	hexdump(dest, (blks * BIOSCD_SECSIZE)); */
	return(0);
}

/*
 * Return a suitable dev_t value for (dev).
 */
int
bc_getdev(struct i386_devdesc *dev)
{
    int biosdev, unit, device;
    int major;
    int rootdev;

    unit = dev->d_unit;
    biosdev = bc_unit2bios(unit);
    DEBUG("unit %d BIOS device %d", unit, biosdev);
    if (biosdev == -1)				/* not a BIOS device */
	return(-1);

    device = biosdev & 0xf0;
    if (device == 0x80)
	major = ACDMAJOR;
    else if (device == 0xa0)
	major = CDMAJOR;
    else
	return (-1);

    unit = 0;	/* XXX */

    /* XXX: Assume partition 'a'. */
    rootdev = MAKEBOOTDEV(major, 0, unit, 0);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}
