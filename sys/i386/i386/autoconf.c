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
 *	$Id: autoconf.c,v 1.23 1995/04/14 15:13:23 dufault Exp $
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

#include <machine/md_var.h>
#include <machine/pte.h>

static void swapconf(void);
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

#include "isa.h"
#if NISA > 0
      #include <i386/isa/isa_device.h>
#endif

#include "pci.h"
#if NPCI > 0
      #include <pci/pcivar.h>
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
			printf("trying rootdev=%x (%s%d)\n",
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

#if NISA > 0
	isa_configure();
#endif

#if NPCI > 0
	pci_configure();
#endif

	configure_finish();

#ifdef CD9660
	if ((boothowto & RB_CDROM) && !mountroot)
		mountroot = find_cdrom_root;
#endif

#ifdef NFS
	if (!mountroot && nfs_diskless_valid)
		mountroot = nfs_mountroot;
#endif /* NFS */

#ifdef FFS
	if (!mountroot) {
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
		setconf();
	}
#endif
	if (!mountroot) {
		panic("Nobody wants to mount my root for me");
	}
	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	swapconf();
	cold = 0;
}

/*
 * Configure swap space and related parameters.
 */
static void
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev > 0; swp++)
	{
		unsigned d = major(swp->sw_dev);

		if (d > nblkdev) break;
		if (bdevsw[d].d_psize) {
			nblks = (*bdevsw[d].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			else
				swp->sw_nblks = 0;
		}
		swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
	}
	if (dumplo == 0 && bdevsw[major(dumpdev)].d_psize)
		dumplo = (*bdevsw[major(dumpdev)].d_psize)(dumpdev) -
			Maxmem*NBPG/512;
	if (dumplo < 0)
		dumplo = 0;
}

#define	DOSWAP			/* change swdevt and dumpdev */
u_long	bootdev = 0;		/* should be dev_t, but not until 32 bits */

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
#ifdef DOSWAP
	mindev &= ~PARTITIONMASK;
	for (swp = swdevt; swp->sw_dev; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == (minor(swp->sw_dev) & ~PARTITIONMASK)) {

			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == 0)
		return;
	/*
	 * If dumpdev was the same as the old primary swap
	 * device, move it to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
#endif
}
