/*-
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
 *	from: @(#)autoconf.c	7.1 (Berkeley) 5/9/91
 *	$Id: autoconf.c,v 1.82 1997/11/21 18:27:08 bde Exp $
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include "opt_cd9660.h"
#include "opt_ffs.h"
#include "opt_nfs.h"
#include "opt_mfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/devfsext.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <machine/bootinfo.h>
#include <machine/md_var.h>

u_long	bootdev = 0;	/* from bootblocks: not dev_t - encoding is different */
struct vnode *root_device_vnode = NULL;
char root_device_name[64];


/*#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <machine/clock.h> */

struct major_hack {
	int major;
	char *name;
} hack_major[] = {
{0,	"wd"},		/* ST506 disk controller (with IDE extensions) */
{2,	"fd"},		/* floppy disk */
{3,	"wt"},		/* QIC-02/36 tape */
{4,	"sd"},		/* SCSI "disk" type */
{5,	"st"},		/* SCSI "tape" type */
{6,	"cd"},		/* SCSI "cdrom" type */
{7,	"mcd"},		/* Mitsumi CDROM interface */
{15,	"vn"},		/* vnode disk device */
{16,	"scd"},		/* Sony CDROM interface */
{17,	"matcd"},	/* Matsushita/Panasonic/Creative(SB) CDROM interface */
{18,	"ata"},		/* "device independent" ATA/IDE driver */
{19,	"wcdb"},	/* ATAPI CDROM client of "ata" */
{20,	"od"},		/* SCSI "magneto-optical" disk */
{21,	"ccd"},		/* concatenated disk */
{22,	"gd"},		/* Geometry disk. */
{23,	"worm"},	/* SCSI "worm type" */
{0,	NULL}
};

static struct {
	char *name;
	int major;
} try_cdrom[] = {
	{ "cd", 6 },
	{ "mcd", 7 },
	{ "scd", 16 },
	{ "matcd", 17 },
	{ "wcd", 19 },
	{ 0, 0}
};

#ifdef CD9660
/*
 * XXX All this CD-ROM root stuff is fairly messy.  Ick.
 * We need to look for a cdrom that we can open.
 * Of course we don't KNOW which, so just try them all in 
 * an arbitrary order.
 * If we find one, we open it and put it's vnode into the right place.
 * (and the name)
 */
static int
find_cdrom_root(void)
{
	int i, j, error;
	struct bdevsw *bd;
	char buf[32];
	struct vnode *vn;

#if CD9660_ROOTDELAY > 0
	DELAY(CD9660_ROOTDELAY * 1000000);
#endif
	for (i = 0 ; i < 2; i++) {
		for (j = 0 ; try_cdrom[j].name ; j++) {
			if (try_cdrom[j].major >= nblkdev)
				continue;
			/*if (bootverbose)*/
				printf("trying %s%d as rootdev\n",
				       try_cdrom[j].name, i);
			sprintf(buf, "\%s%d", try_cdrom[j].name, i);
			vn = devfs_open_device(buf, DV_BLK);
			if (vn) {
				root_device_vnode = vn;
				strcpy(root_device_name, buf);
				rootdev = makedev(try_cdrom[j].major, i * 8);
				return 0;
			}
		}
	}
	return EINVAL;
}
#endif /* CD9660 */


/*
 * Open a standard 386BSD compatible disk, according to the 
 * old rules (sd0a is the a part of the first BSD slice on sd0.)
 */
static int
open_root_by_major(dev_t dev)
{
	/*
	 * given a dev_t (from god-only knows where)
	 * turn it into a sane device name.
	 */
	int	 maj = major(dev);
	int 	 min = minor(dev);
	int	 unit = (min >> 3) & 0x0f;
	int	 part = (min & 0x07);
	int 	 i = 0;
	struct	major_hack *mp;
	struct vnode *vn;
	char buf[32];

	mp = hack_major;
	while (mp->name) {
		if ( mp->major == maj)
			break;
		mp++;
	}
	if (mp->name == NULL) {
		return (EINVAL);
	}

	/* try see if we have old style partitions */
	sprintf(buf,"/%s%d%c", mp->name, unit, 'a'+ part);
	printf ("WOULD SELECT %s ", buf);
	if ((vn = devfs_open_device(buf, DV_BLK))) {
		printf("And it exists\n");
	} else {
		printf("but it doesn't exist\n");
		for ( i = 1; i < 5; i++ ) {
			sprintf(buf,"/%s%ds%d%c", mp->name, unit, i, 'a'+ part);
			if ((vn = devfs_open_device(buf, DV_BLK))) {
				printf("%s exists, I'll use that\n", buf);
				break;
			} else {
				printf ("%s didn't work\n", buf);
			}
		}
		if (i == 5) {
			return (EINVAL);
		}
	}
	if (vn) {
		root_device_vnode = vn;
		strcpy(root_device_name, buf);
		rootdev = devfs_vntodev(vn);
		return (0);
	}
	return (EINVAL);
}

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
static void
setroot(void)
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t newrootdev;
	struct vnode *vn;
	char buf[32];

/*printf("howto %x bootdev %x ", boothowto, bootdev);*/
	/*
	 * If bootdev is screwy, just open the default device
	 * that was compiled in.
	 */
	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC) {
		open_root_by_major(rootdev);
		return;
	}
	majdev	= (bootdev >> B_TYPESHIFT)	& B_TYPEMASK;
	unit	= (bootdev >> B_UNITSHIFT)	& B_UNITMASK;
	part	= (bootdev >> B_PARTITIONSHIFT)	& B_PARTITIONMASK;

	/* 
	 * We have two choices.. do it by hand
	 * or use the major/minor we were given
	 */
	switch (majdev) {
	case	2: /*fd*/
		sprintf(buf,"/fd%d", unit);
		if ((vn = devfs_open_device(buf, DV_BLK))) {
			root_device_vnode = vn;
			strcpy(root_device_name, buf);
			rootdev = devfs_vntodev(vn);
		}
		break;
	case	0: /*wd*/
	case	4: /*sd*/
		newrootdev = makedev(majdev,(unit * 8)+part);
		open_root_by_major(newrootdev);
		break;
	default: /* try something different.. you never know.. */
		open_root_by_major(rootdev);
		return;
	}
	if ( root_device_vnode == NULL) {
		panic("can't mount root");
	}
}

void
cpu_rootconf()
{
	/*
	 * XXX NetBSD has a much cleaner approach to finding root.
	 * XXX We should adopt their code.
	 */
#ifdef CD9660
	if ((boothowto & RB_CDROM)) {
		if (bootverbose)
			printf("Considering CD-ROM root f/s.\n");
		/* NB: find_cdrom_root() sets rootdev if successful. */
		if (find_cdrom_root() == 0)
			mountrootfsname = "cd9660";
		else if (bootverbose)
			printf("No CD-ROM available as root f/s.\n");
	}
#endif /* CD9660 */

#ifdef MFS_ROOT
	if (!mountrootfsname) {
		if (bootverbose)
			printf("Considering MFS root f/s.\n");
		mountrootfsname = "mfs";
		/*
		 * Ignore the -a flag if this kernel isn't compiled
		 * with a generic root/swap configuration: if we skip
		 * setroot() and we aren't a generic kernel, chaos
		 * will ensue because setconf() will be a no-op.
		 * (rootdev is always initialized to NODEV in a
		 * generic configuration, so we test for that.)
		 */
		if ((boothowto & RB_ASKNAME) == 0 || rootdev != NODEV)
			setroot();
	}
#endif

#ifdef BOOTP_NFSROOT
	if (!mountrootfsname && !nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering BOOTP NFS root f/s.\n");
		mountrootfsname = "nfs";
	}
#endif /* BOOTP_NFSROOT */
#ifdef NFS
	if (!mountrootfsname && nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering NFS root f/s.\n");
		mountrootfsname = "nfs";
	}
#endif /* NFS */

#ifdef FFS
	if (!mountrootfsname) {
		mountrootfsname = "ufs";
		if (bootverbose)
			printf("Considering FFS root f/s.\n");
		/*
		 * Ignore the -a flag if this kernel isn't compiled
		 * with a generic root/swap configuration: if we skip
		 * setroot() and we aren't a generic kernel, chaos
		 * will ensue because setconf() will be a no-op.
		 * (rootdev is always initialized to NODEV in a
		 * generic configuration, so we test for that.)
		 */
		if ((boothowto & RB_ASKNAME) == 0 || rootdev != NODEV)
			setroot();
	}
#endif

#ifdef LFS
	if (!mountrootfsname) {
		if (bootverbose)
			printf("Considering LFS root f/s.\n");
		mountrootfsname = "lfs";
		/*
		 * Ignore the -a flag if this kernel isn't compiled
		 * with a generic root/swap configuration: if we skip
		 * setroot() and we aren't a generic kernel, chaos
		 * will ensue because setconf() will be a no-op.
		 * (rootdev is always initialized to NODEV in a
		 * generic configuration, so we test for that.)
		 */
		if ((boothowto & RB_ASKNAME) == 0 || rootdev != NODEV)
			setroot();
	}
#endif

	if (!mountrootfsname) {
		panic("Nobody wants to mount my root for me");
	}

	setconf();
}

