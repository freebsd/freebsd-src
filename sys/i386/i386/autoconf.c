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
 * $FreeBSD: src/sys/i386/i386/autoconf.c,v 1.146 1999/12/26 16:21:16 bde Exp $
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the vba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */
#include "opt_bootp.h"
#include "opt_ffs.h"
#include "opt_cd9660.h"
#include "opt_nfs.h"
#include "opt_nfsroot.h"
#include "opt_bus.h"
#include "opt_rootdevname.h"

#include "isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/reboot.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/cons.h>

#include <machine/bootinfo.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#ifdef APIC_IO
#include <machine/smp.h>
#else
#include <i386/isa/icu.h>
#endif /* APIC_IO */

#if NISA > 0
#include <isa/isavar.h>

device_t isa_bus_device = 0;
#endif

static void	configure_first __P((void *));
static void	configure __P((void *));
static void	configure_final __P((void *));

#if defined(FFS) && defined(FFS_ROOT)
static void	setroot __P((void));
#endif

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
/* SI_ORDER_SECOND is hookable */
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
/* SI_ORDER_MIDDLE is hookable */
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

device_t nexus_dev;

/*
 * Determine i/o configuration for a machine.
 */
static void
configure_first(dummy)
	void *dummy;
{
}

static void
configure(dummy)
	void *dummy;
{

	/*
	 * Activate the ICU's.  Note that we are explicitly at splhigh()
	 * at present as we have no way to disable stray PCI level triggered
	 * interrupts until the devices have had a driver attached.  This
	 * is particularly a problem when the interrupts are shared.  For
	 * example, if IRQ 10 is shared between a disk and network device
	 * and the disk device generates an interrupt, if we "activate"
	 * IRQ 10 when the network driver is set up, then we will get
	 * recursive interrupt 10's as nothing will know how to turn off
	 * the disk device's interrupt.
	 *
	 * Having the ICU's active means we can probe interrupt routing to
	 * see if a device causes the corresponding pending bit to be set.
	 *
	 * This is all rather inconvenient.
	 */
#ifdef APIC_IO
	bsp_apic_configure();
	enable_intr();
#else
	enable_intr();
	INTREN(IRQ_SLAVE);
#endif /* APIC_IO */

	/* nexus0 is the top of the i386 device tree */
	device_add_child(root_bus, "nexus", 0);

	/* initialize new bus architecture */
	root_bus_configure();

#if NISA > 0
	/*
	 * Explicitly probe and attach ISA last.  The isa bus saves
	 * it's device node at attach time for us here.
	 */
	if (isa_bus_device)
		isa_probe_children(isa_bus_device);
#endif

	/*
	 * Now we're ready to handle (pending) interrupts.
	 * XXX this is slightly misplaced.
	 */
	spl0();

	/*
	 * Allow lowering of the ipl to the lowest kernel level if we
	 * panic (or call tsleep() before clearing `cold').  No level is
	 * completely safe (since a panic may occur in a critical region
	 * at splhigh()), but we want at least bio interrupts to work.
	 */
	safepri = cpl;
}

static void
configure_final(dummy)
	void *dummy;
{
	int i;

	cninit_finish();

	if (bootverbose) {

#ifdef APIC_IO
		imen_dump();
#endif /* APIC_IO */

		/*
		 * Print out the BIOS's idea of the disk geometries.
		 */
		printf("BIOS Geometries:\n");
		for (i = 0; i < N_BIOS_GEOM; i++) {
			unsigned long bios_geom;
			int max_cylinder, max_head, max_sector;

			bios_geom = bootinfo.bi_bios_geom[i];

			/*
			 * XXX the bootstrap punts a 1200K floppy geometry
			 * when the get-disk-geometry interrupt fails.  Skip
			 * drives that have this geometry.
			 */
			if (bios_geom == 0x4f010f)
				continue;

			printf(" %x:%08lx ", i, bios_geom);
			max_cylinder = bios_geom >> 16;
			max_head = (bios_geom >> 8) & 0xff;
			max_sector = bios_geom & 0xff;
			printf(
		"0..%d=%d cylinders, 0..%d=%d heads, 1..%d=%d sectors\n",
			       max_cylinder, max_cylinder + 1,
			       max_head, max_head + 1,
			       max_sector, max_sector);
		}
		printf(" %d accounted for\n", bootinfo.bi_n_bios_used);

		printf("Device configuration finished.\n");
	}
	cold = 0;
}

#ifdef BOOTP
extern void bootpc_init(void);
#endif
/*
 * Do legacy root filesystem discovery.
 */
void
cpu_rootconf()
{
#ifdef BOOTP
        bootpc_init();
#endif
#if defined(NFS) && defined(NFS_ROOT)
#if !defined(BOOTP_NFSROOT)
	if (nfs_diskless_valid)
#endif
		rootdevnames[0] = "nfs:";
#endif
#if defined(FFS) && defined(FFS_ROOT)
        if (!rootdevnames[0])
                setroot();
#endif
}
SYSINIT(cpu_rootconf, SI_SUB_ROOT_CONF, SI_ORDER_FIRST, cpu_rootconf, NULL)

u_long	bootdev = 0;		/* not a dev_t - encoding is different */

#if defined(FFS) && defined(FFS_ROOT)
#define FDMAJOR 	2
#define FDUNITSHIFT     6

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * set rootdevs[] and rootdevnames[] to correspond to the
 * boot device(s).
 *
 * This code survives in order to allow the system to be 
 * booted from legacy environments that do not correctly
 * populate the kernel environment. There are significant
 * restrictions on the bootability of the system in this
 * situation; it can only be mounting root from a 'da'
 * 'wd' or 'fd' device, and the root filesystem must be ufs.
 */
static void
setroot()
{
	int majdev, mindev, unit, slice, part;
	dev_t newrootdev, dev;
	char partname[2];
	char *sname;

	if ((bootdev & B_MAGICMASK) != B_DEVMAGIC) {
		printf("no B_DEVMAGIC (bootdev=%#lx)\n", bootdev);
		return;
	}
	majdev = B_TYPE(bootdev);
	dev = makebdev(majdev, 0);
	if (devsw(dev) == NULL) {
		printf("no devsw (majdev=%d bootdev=%#lx)\n", majdev, bootdev);
		return;
	}
	unit = B_UNIT(bootdev);
	slice = B_SLICE(bootdev);
	if (slice == WHOLE_DISK_SLICE)
		slice = COMPATIBILITY_SLICE;
	if (slice < 0 || slice >= MAX_SLICES) {
		printf("bad slice\n");
		return;
	}

	/*
	 * XXX kludge for inconsistent unit numbering and lack of slice
	 * support for floppies.
	 */
	if (majdev == FDMAJOR) {
		slice = COMPATIBILITY_SLICE;
		part = RAW_PART;
		mindev = unit << FDUNITSHIFT;
	} else {
		part = B_PARTITION(bootdev);
		mindev = dkmakeminor(unit, slice, part);
	}

	newrootdev = makebdev(majdev, mindev);
	sname = dsname(newrootdev, unit, slice, part, partname);
	rootdevnames[0] = malloc(strlen(sname) + 6, M_DEVBUF, M_NOWAIT);
	sprintf(rootdevnames[0], "ufs:%s%s", sname, partname);

	/*
	 * For properly dangerously dedicated disks (ones with a historical
	 * bogus partition table), the boot blocks will give slice = 4, but
	 * the kernel will only provide the compatibility slice since it
	 * knows that slice 4 is not a real slice.  Arrange to try mounting
	 * the compatibility slice as root if mounting the slice passed by
	 * the boot blocks fails.  This handles the dangerously dedicated
	 * case and perhaps others.
	 */
	if (slice == COMPATIBILITY_SLICE)
		return;
	slice = COMPATIBILITY_SLICE;
	sname = dsname(newrootdev, unit, slice, part, partname);
	rootdevnames[1] = malloc(strlen(sname) + 6, M_DEVBUF, M_NOWAIT);
	sprintf(rootdevnames[1], "ufs:%s%s", sname, partname);
}
#endif
