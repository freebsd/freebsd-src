/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)conf.c	5.8 (Berkeley) 5/12/91
 *	$Id: conf.c,v 1.113 1995/12/08 11:13:18 julian Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>


#define NUMCDEV 96
#define NUMBDEV 32

struct bdevsw	*bdevsw[NUMBDEV];
int	nblkdev = NUMBDEV;
struct cdevsw	*cdevsw[NUMCDEV];
int	nchrdev = NUMCDEV;

/*
 * The routines below are total "BULLSHIT" and will be trashed
 * When I have 'proved' the JREMOD changes above..
 */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(1, 0);

/*
 * The tunnel device's LKM wants to know where to install itself in the
 * cdevsw table.  Sigh.
 */
dev_t	tuncdev = makedev(52, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == 2 && (minor(dev) == 0 || minor(dev) == 1));
}

int
iszerodev(dev)
	dev_t dev;
{
	return (major(dev) == 2 && minor(dev) == 12);
}

/*
 * Routine to determine if a device is a disk.
 *
 * A minimal stub routine can always return 0.
 */
int
isdisk(dev, type)
	dev_t dev;
	int type;
{

	switch (major(dev)) {
	case 15:		/* VBLK: vn, VCHR: cd */
		return (1);
	case 0:			/* wd */
	case 2:			/* fd */
	case 4:			/* sd */
	case 6:			/* cd */
	case 7:			/* mcd */
	case 16:		/* scd */
	case 17:		/* matcd */
	case 18:		/* ata */
	case 19:		/* wcd */
	case 20:		/* od */
		if (type == VBLK)
			return (1);
		return (0);
	case 3:			/* wd */
	case 9:			/* fd */
	case 13:		/* sd */
	case 29:		/* mcd */
	case 43:		/* vn */
	case 45:		/* scd */
	case 46:		/* matcd */
	case 69:		/* wcd */
	case 70:		/* od */
		if (type == VCHR)
			return (1);
		/* fall through */
	default:
		return (0);
	}
	/* NOTREACHED */
}

#ifndef NEW_STUFF_JRE

/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	switch (major(dev)) {
	case 3:		blkmaj = 0;  break; /* wd */
	case 9:		blkmaj = 2;  break; /* fd */
	case 10:	blkmaj = 3;  break; /* wt */
	case 13:	blkmaj = 4;  break; /* sd */
	case 14:	blkmaj = 5;  break; /* st */
	case 15:	blkmaj = 6;  break; /* cd */
	case 29:	blkmaj = 7;  break; /* mcd */
	case 43:	blkmaj = 15; break; /* vn */
	case 45:	blkmaj = 16; break; /* scd */
	case 46:	blkmaj = 17; break; /* matcd */
	case 69:	blkmaj = 19; break; /* wcd */
	case 70:	blkmaj = 20; break; /* od */
	default:
		return (NODEV);
	}
	return (makedev(blkmaj, minor(dev)));
}

int
getmajorbyname(name)
	const char *name;
{

	if (strcmp(name, "sc") == 0)
		return (12);
	if (strcmp(name, "vt") == 0)
		return (12);
	return (NULL);
}


static struct cdevsw **
getcdevbyname(char *name)
{
	int maj;

	maj = getmajorbyname(name);
	return (maj < 0 ? NULL : &cdevsw[maj]);
}

#else	/* NEW_STUFF_JRE *//*===============================================*/


/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t
chrtoblk(dev_t dev)
{
	int blkmaj;
	struct bdevsw *bd;

        bd = cdevsw[major(dev)]->d_bdev;
	if ( bd ) 
	  return(makedev(bd->d_maj,minor(dev)));
	else
	  return(NODEV);
}

/* Only checks cdevs */
int
getmajorbyname(const char *name)
{
	struct cdevsw *cd;
	int maj;
	char *dname;

	for( maj = 0; maj <nchrdev ; maj++) {
		if ( dname = cdevsw[maj]->d_name) {
			if ( strcmp(name, dname) == 0 ) {
				return maj;
			}
		}
	}
	return -1; /* XXX */ /* Was 0 */
}


/* utterly pointless with devfs */
static struct cdevsw **
getcdevbyname(const char *name)
{
	struct cdevsw *cd;
	int maj;
	char *dname;

	for( maj = 0; maj <nchrdev ; maj++) {
		if ( dname = cdevsw[maj]->d_name) {
			if ( strcmp(name, dname) == 0 ) {
				return &cdevsw[maj];
			}
		}
	}
	return NULL;
}
#endif /* NEW_STUFF_JRE */

/* Zap these as soon as we find out who calls them  , and "why?"*/
int
register_cdev(name, cdp)
	const char *name;
	const struct cdevsw *cdp;
{
	struct cdevsw **dst_cdp;

	dst_cdp = getcdevbyname(name);
	if (dst_cdp == NULL)
		return (ENXIO);
	if ((*dst_cdp != NULL)
	   && ((*dst_cdp)->d_open != nxopen)
	   && ((*dst_cdp)->d_open != NULL))
		return (EBUSY);
	*dst_cdp = cdp;
	return (0);
}

static struct cdevsw nxcdevsw = {
	nxopen,		nxclose,	nxread,		nxwrite,
	nxioctl,	nxstop,		nxreset,	nxdevtotty,
	nxselect,	nxmmap,		NULL,
};

int
unregister_cdev(name, cdp)
	const char *name;
	const struct cdevsw *cdp;
{
	struct cdevsw **dst_cdp;

	dst_cdp = getcdevbyname(name);
	if (dst_cdp == NULL)
		return (ENXIO);
	if ((*dst_cdp)->d_open != cdp->d_open)
		return (EBUSY);
	*dst_cdp = &nxcdevsw;
	return (0);
}
