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
 *	$Id: autoconf.c,v 1.130 1999/08/06 20:29:46 phk Exp $
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
#include "opt_nfsroot.h"
#include "opt_bus.h"
#include "opt_rootdevname.h"

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
#include <sys/sysctl.h>
#include <sys/cons.h>

#include <machine/bootinfo.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */

#include <i386/isa/icu.h>

#include "pnp.h"
#if NPNP > 0
#include <i386/isa/isa_device.h>
#include <i386/isa/pnp.h>
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
device_t isa_bus_device = 0;
#endif

static void	configure_first __P((void *));
static void	configure __P((void *));
static void	configure_final __P((void *));

static void	configure_finish __P((void));
static void	configure_start __P((void));
#if defined(FFS) || defined(FFS_ROOT)
static void	setroot __P((void));
#endif
static int	setrootbyname __P((char *name));
static void	gets __P((char *));

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
/* SI_ORDER_SECOND is hookable */
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
/* SI_ORDER_MIDDLE is hookable */
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

#if defined(CD9660) || defined(CD9660_ROOT)

#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <machine/clock.h>

/*
 * XXX All this CD-ROM root stuff is fairly messy.  Ick.
 *
 * We need to try out all our potential CDROM drives, so we need a table.
 */
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

static int	find_cdrom_root __P((void));

static int
find_cdrom_root()
{
	int i, j, error;
	struct cdevsw *bd;
	dev_t orootdev;

#if CD9660_ROOTDELAY > 0
	DELAY(CD9660_ROOTDELAY * 1000000);
#endif
	orootdev = rootdev;
	for (i = 0 ; i < 2; i++)
		for (j = 0 ; try_cdrom[j].name ; j++) {
			if (try_cdrom[j].major >= NUMCDEVSW)
				continue;
			rootdev = makebdev(try_cdrom[j].major, i * 8);
			bd = bdevsw(rootdev);
			if (bd == NULL || bd->d_open == NULL)
				continue;
			if (bootverbose)
				printf("trying %s%d as rootdev (%p)\n",
				       try_cdrom[j].name, i, (void *)rootdev);
			error = (bd->d_open)(rootdev, FREAD, S_IFBLK, curproc);
			if (error == 0) {
				if (bd->d_close != NULL)
					(bd->d_close)(rootdev, FREAD, S_IFBLK,
						      curproc);
				return 0;
			}
		}

	rootdev = orootdev;
	return EINVAL;
}
#endif /* CD9660 || CD9660_ROOT */

static void
configure_start()
{
}

static void
configure_finish()
{
}

device_t nexus_dev;

/*
 * Determine i/o configuration for a machine.
 */
static void
configure_first(dummy)
	void *dummy;
{

	configure_start();		/* DDB hook? */
}

static void
configure(dummy)
	void *dummy;
{

	/* Allow all routines to decide for themselves if they want intrs */
	/*
	 * XXX Since this cannot be achieved on all architectures, we should
	 * XXX go back to disabling all interrupts until configuration is
	 * XXX completed and switch any devices that rely on the current
	 * XXX behavior to no longer rely on interrupts or to register an
	 * XXX interrupt_driven_config_hook for the task.
	 */
	/*
	 * XXX The above is wrong, because we're implicitly at splhigh(),
	 * XXX and should stay there, so enabling interrupts in the CPU
	 * XXX and the ICU at most gives pending interrupts which just get
	 * XXX in the way.
	 */
#ifdef APIC_IO
	bsp_apic_configure();
	enable_intr();
#else
	enable_intr();
	INTREN(IRQ_SLAVE);
#endif /* APIC_IO */

#if NPNP > 0
	pnp_configure();
#endif

	/* nexus0 is the top of the i386 device tree */
	device_add_child(root_bus, "nexus", 0, 0);

	/* initialize new bus architecture */
	root_bus_configure();

#if NISA > 0
	if (isa_bus_device)
		bus_generic_attach(isa_bus_device);
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

	configure_finish();			/* DDB hook? */

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


void
cpu_rootconf()
{
	/*
	 * XXX NetBSD has a much cleaner approach to finding root.
	 * XXX We should adopt their code.
	 */
#if defined(CD9660) || defined(CD9660_ROOT)
	if ((boothowto & RB_CDROM)) {
		if (bootverbose)
			printf("Considering CD-ROM root f/s.\n");
		/* NB: find_cdrom_root() sets rootdev if successful. */
		if (find_cdrom_root() == 0)
			mountrootfsname = "cd9660";
		else if (bootverbose)
			printf("No CD-ROM available as root f/s.\n");
	}
#endif

#ifdef BOOTP_NFSROOT
	if (!mountrootfsname && !nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering BOOTP NFS root f/s.\n");
		mountrootfsname = "nfs";
	}
#endif /* BOOTP_NFSROOT */
#if defined(NFS) || defined(NFS_ROOT)
	if (!mountrootfsname && nfs_diskless_valid) {
		if (bootverbose)
			printf("Considering NFS root f/s.\n");
		mountrootfsname = "nfs";
	}
#endif /* NFS */

#if defined(FFS) || defined(FFS_ROOT)
	if (!mountrootfsname) {
		mountrootfsname = "ufs";
		if (bootverbose)
			printf("Considering FFS root f/s.\n");
		if (boothowto & RB_ASKNAME)
			setconf();
		else
			setroot();
	}
#endif

	if (!mountrootfsname) {
		panic("Nobody wants to mount my root for me");
	}
}

u_long	bootdev = 0;		/* not a dev_t - encoding is different */

#define FDMAJOR 2
#define FDUNITSHIFT     6

#if defined(FFS) || defined(FFS_ROOT)
/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * set rootdevs[] and rootdevnames[] to correspond to the
 * boot device(s).
 */
static void
setroot()
{
	int majdev, mindev, unit, slice, part;
	dev_t newrootdev, dev;
	char partname[2];
	char *sname;

	if (boothowto & RB_DFLTROOT) {
#ifdef ROOTDEVNAME
		setrootbyname(ROOTDEVNAME);
#else
		setconf();
#endif
		return;
	}
	if ((bootdev & B_MAGICMASK) != B_DEVMAGIC) {
		printf("No B_DEVMAGIC\n");
		setconf();
		return;
	}
	majdev = B_TYPE(bootdev);
	dev = makebdev(majdev, 0);
	if (bdevsw(dev) == NULL) {
		printf("No bdevsw (majdev=%d bootdev=%p)\n", majdev,
		    (void *)bootdev);
		setconf();
		return;
	}
	unit = B_UNIT(bootdev);
	slice = B_SLICE(bootdev);
	if (slice == WHOLE_DISK_SLICE)
		slice = COMPATIBILITY_SLICE;
	if (slice < 0 || slice >= MAX_SLICES) {
		printf("bad slice\n");
		setconf();
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
	rootdevs[0] = newrootdev;
	sname = dsname(bdevsw(newrootdev)->d_name, unit, slice, part, partname);
	rootdevnames[0] = malloc(strlen(sname) + 2, M_DEVBUF, M_NOWAIT);
	sprintf(rootdevnames[0], "%s%s", sname, partname);

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
	rootdevs[1] = dkmodslice(newrootdev, slice);
	sname = dsname(bdevsw(newrootdev)->d_name, unit, slice, part, partname);
	rootdevnames[1] = malloc(strlen(sname) + 2, M_DEVBUF, M_NOWAIT);
	sprintf(rootdevnames[1], "%s%s", sname, partname);
}
#endif



static int
setrootbyname(char *name)
{
	char *cp;
	int bd, unit, slice, part;
	dev_t dev;

	printf("setrootbyname(\"%s\")\n", name);
	slice = 0;
	part = 0;
	cp = name;
	while (cp != '\0' && (*cp < '0' || *cp > '9'))
		cp++;
	if (cp == name) {
		printf("missing device name\n");
		return(1);
	}
	if (*cp == '\0') {
		printf("missing unit number\n");
		return(1);
	}
	unit = *cp - '0';
	*cp++ = '\0';
	for (bd = 0; bd < NUMCDEVSW; bd++) {
		dev = makebdev(bd, 0);
		if (bdevsw(dev) != NULL &&
		    strcmp(bdevsw(dev)->d_name, name) == 0)
			goto gotit;
	}
	return (2);
gotit:
	while (*cp >= '0' && *cp <= '9')
		unit += 10 * unit + *cp++ - '0';
	if (*cp == 's' && cp[1] >= '0' && cp[1] <= '9') {
		slice = cp[1] - '0' + 1;
		cp += 2;
	}
	if (*cp >= 'a' && *cp <= 'h') {
		part = *cp - 'a';
		cp++;
	}
	if (*cp != '\0') {
		printf("junk after name\n");
		return (1);
	}
	rootdev = makebdev(bd, dkmakeminor(unit, slice, part));
	printf("driver=%s, unit=%d, slice=%d, part=%d -> rootdev=%p\n",
		name, unit, slice, part, (void *)rootdev);
	return 0;
}

void
setconf()
{
	char name[128];
	int i;
	dev_t dev;

	for(;;) {
		printf("root device? ");
		gets(name);
		i = setrootbyname(name);
		if (!i)
			return;
	
		printf("use one of:\n");
		for (i = 0; i < NUMCDEVSW; i++) {
			dev = makebdev(i, 0);
			if (bdevsw(dev) != NULL)
			    printf(" \"%s\"", bdevsw(dev)->d_name);
		}
		printf("\nfollowed by a unit number...\n");
	}
}

static void
gets(cp)
	char *cp;
{
	register char *lp;
	register int c;

	lp = cp;
	for (;;) {
		printf("%c", c = cngetc() & 0177);
		switch (c) {
		case -1:
		case '\n':
		case '\r':
			*lp++ = '\0';
			return;
		case '\b':
		case '\177':
			if (lp > cp) {
				printf(" \b");
				lp--;
			}
			continue;
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;
		case '@':
		case 'u' & 037:
			lp = cp;
			printf("%c", '\n');
			continue;
		default:
			*lp++ = c;
		}
	}
}
