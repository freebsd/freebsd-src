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
 *	$Id: autoconf.c,v 1.33.4.4 1996/06/05 19:48:59 nate Exp $
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pte.h>
#include <i386/isa/icu.h> /* For interrupts */

static void setroot(void);

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	dkn;		/* number of iostat dk numbers assigned so far */

extern int (*mountroot) __P((void));
#ifdef FFS
int ffs_mountroot __P((void));
#endif
#ifdef NFS
int nfs_mountroot __P((void));
#endif
#ifdef CD9660
int cd9660_mountroot __P((void));
#endif
#ifdef MSDOSFS
int msdosfs_mountroot __P((void));
#endif
#ifdef MFS_ROOT
int mfs_initminiroot __P((u_char *));
u_char mfs_root[MFS_ROOT*1024] = "MFS Filesystem goes here";
u_char end_mfs_root[] = "MFS Filesystem had better STOP here";
#endif

#include "eisa.h"
#if NEISA > 0
      #include <i386/eisa/eisaconf.h>
#endif

#include "pci.h"
#if NPCI > 0
      #include <pci/pcivar.h>
#endif

#include "isa.h"
#if NISA > 0
      #include <i386/isa/isa_device.h>
#endif

#ifdef CD9660
/* We need to try out all our potential CDROM drives, so we need a table. */
static struct {
	char *name;
	int major;
} try_cdrom[] = {
	{ "cd", 6 },
	{ "mcd", 7 },
	{ "scd", 16 },
	{ "matcd", 17 },
	{ 0, 0}
};

int
find_cdrom_root()
{
	int i,j,k;

	for (j = 0 ; j < 2; j++)
		for (k = 0 ; try_cdrom[k].name ; k++) {
			rootdev = makedev(try_cdrom[k].major,j*8);
			printf("trying rootdev=0x%lx (%s%d)\n",
				rootdev, try_cdrom[k].name,j);
			i = (*cd9660_mountroot)();
			if (!i) return i;
		}
	return EINVAL;
}
#endif /* CD9660 */

#include "scbus.h"
#if NSCBUS > 0
      #include <scsi/scsiconf.h>
#endif

void
configure_start()
{
#if NSCBUS > 0
	scsi_configure_start();
#endif
}

void
configure_finish()
{
#if NSCBUS > 0
	scsi_configure_finish();
#endif
}

/*
 * Determine i/o configuration for a machine.
 */
void
configure()
{

	configure_start();

	/* Allow all routines to decide for themselves if they want intrs */
	enable_intr();
	INTREN(IRQ_SLAVE);

#if NEISA > 0
	eisa_configure();
#endif

#if NPCI > 0
	pci_configure();
#endif

#if NISA > 0
	isa_configure();
#endif

	configure_finish();

#ifdef MFS_ROOT
	mfs_initminiroot(mfs_root);
#endif /* MFS_ROOT */

	if (bootverbose)
		printf("Device configuration finished.\n");

#ifdef CD9660
	if ((boothowto & RB_CDROM) && !mountroot) {
		if (bootverbose)
			printf("Considering CD-ROM root f/s.\n");
		mountroot = find_cdrom_root;
	}
#endif

#ifdef BOOTP_NFSROOT
	if (!mountroot && !nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering BOOTP NFS root f/s.\n");
		mountroot = nfs_mountroot;
	}
#endif /* BOOTP_NFSROOT */

#ifdef NFS
	if (!mountroot && nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering NFS root f/s.\n");
		mountroot = nfs_mountroot;
	}
#endif /* NFS */

#ifdef FFS
	if (!mountroot) {
		if (bootverbose)
			printf("Considering FFS root f/s.\n");
		mountroot = ffs_mountroot;
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
	if (!mountroot) {
		panic("Nobody wants to mount my root for me");
	}
	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	if (bootverbose)
		printf("Configuring root and swap devs.\n");
	setconf();
	cold = 0;
	if (bootverbose)
		printf("configure() finished.\n");
}

int
setdumpdev(dev)
	dev_t dev;
{
	int maj, psize;
	long newdumplo;

	if (dev == NODEV) {
		dumpdev = dev;
		dumplo = 0;
		return (0);
	}
	maj = major(dev);
	if (maj >= nblkdev)
		return (ENXIO);
	if (bdevsw[maj].d_psize == NULL)
		return (ENXIO);		/* XXX should sometimes be ENODEV */
	psize = bdevsw[maj].d_psize(dev);
	if (psize == -1)
		return (ENXIO);		/* XXX should sometimes be ENODEV */
	newdumplo = psize - Maxmem * NBPG / DEV_BSIZE;
	if (newdumplo < 0)
		return (ENOSPC);
	dumpdev = dev;
	dumplo = newdumplo;
	return (0);
}

u_long	bootdev = 0;		/* not a dev_t - encoding is different */

static	char devname[][2] = {
      {'w','d'},      /* 0 = wd */
      {'s','w'},      /* 1 = sw */
#define FDMAJOR 2
      {'f','d'},      /* 2 = fd */
      {'w','t'},      /* 3 = wt */
      {'s','d'},      /* 4 = sd -- new SCSI system */
};

#define	PARTITIONMASK	0x7
#define	PARTITIONSHIFT	3
#define FDUNITSHIFT     6
#define RAW_PART        2

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
static void
setroot()
{
	int  majdev, mindev, unit, part, adaptor;
	dev_t temp = 0, orootdev;
	struct swdevt *swp;

/*printf("howto %x bootdev %x ", boothowto, bootdev);*/
	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = (bootdev >> B_TYPESHIFT) & B_TYPEMASK;
	if (majdev > sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = (bootdev >> B_ADAPTORSHIFT) & B_ADAPTORMASK;
	unit = (bootdev >> B_UNITSHIFT) & B_UNITMASK;
	if (majdev == FDMAJOR) {
		part = RAW_PART;
		mindev = unit << FDUNITSHIFT;
	}
	else {
		part = (bootdev >> B_PARTITIONSHIFT) & B_PARTITIONMASK;
		mindev = (unit << PARTITIONSHIFT) + part;
	}
	orootdev = rootdev;
	rootdev = makedev(majdev, mindev);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;
	printf("changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		mindev >> (majdev == FDMAJOR ? FDUNITSHIFT : PARTITIONSHIFT),
		part + 'a');
}
