/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id: autoconf.c,v 1.23 1999/05/10 17:12:38 peter Exp $
 */

#include "opt_bootp.h"
#include "opt_ffs.h"
#include "opt_cd9660.h"
#include "opt_mfs.h"
#include "opt_nfsroot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h> /* for BASE_SLICE, MAX_SLICES */
#include <sys/reboot.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/devicestat.h>

#include <machine/cons.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
#include <machine/bootinfo.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#ifdef MFS_ROOT
#include <ufs/mfs/mfs_extern.h>
#endif

static void	configure __P((void *));
SYSINIT(configure, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL)

static void	configure_finish __P((void));
static void	configure_start __P((void));
static int      setdumpdev __P((dev_t dev));

device_t	isa_bus_device = 0;
struct cam_sim *boot_sim = 0;
extern int nfs_diskless_valid;

dev_t	rootdev = NODEV;
dev_t	dumpdev = NODEV;

static void
configure_start()
{
}

static void
configure_finish()
{
}

static int
atoi(const char *s)
{
    int n = 0;
    while (*s >= '0' && *s <= '9')
	n = n * 10 + (*s++ - '0');
    return n;
}

static const char *
bootdev_field(int which)
{
	char *p = bootinfo.booted_dev;
	char *q;
	static char field[128];

	/* Skip characters to find the right field */
	for (; which; which--) {
		while (*p != ' ' && *p != '\0')
			p++;
		if (*p)
			p++;
	}

	/* Copy out the field and return it */
	q = field;
	while (*p != ' ' && *p != '\0')
		*q++ = *p++;
	*q = '\0';

	return field;
}

static const char *
bootdev_protocol(void)
{
	return bootdev_field(0);
}

static int
bootdev_slot(void)
{
	return atoi(bootdev_field(2));
}

static int
bootdev_unit(void)
{
	return atoi(bootdev_field(5));
}

#if 0

static int
bootdev_bus(void)
{
	return atoi(bootdev_field(1));
}

static int
bootdev_channel(void)
{
	return atoi(bootdev_field(3));
}

static const char *
bootdev_remote_address(void)
{
	return bootdev_field(4);
}

static int
bootdev_boot_dev_type(void)
{
	return atoi(bootdev_field(6));
}

static const char *
bootdev_ctrl_dev_type(void)
{
	return bootdev_field(7);
}

#endif

void
alpha_register_pci_scsi(int bus, int slot, struct cam_sim *sim)
{
	if (!strcmp(bootdev_protocol(), "SCSI")) {
		int boot_slot = bootdev_slot();
		if (bus == boot_slot / 1000
		    && slot == boot_slot % 1000)
			boot_sim = sim;
	}
}

/*
 * Determine i/o configuration for a machine.
 */
static void
configure(void *dummy)
{
	configure_start();

	device_add_child(root_bus, platform.iobus, 0, 0);

	root_bus_configure();

	if((hwrpb->rpb_type != ST_DEC_3000_300) &&
	   (hwrpb->rpb_type != ST_DEC_3000_500)){
		/*
		 * Probe ISA devices after everything.
		 */
		if (isa_bus_device)
			bus_generic_attach(isa_bus_device);
	} 
	configure_finish();

	cninit_finish();

	/*
	 * Now we're ready to handle (pending) interrupts.
	 * XXX this is slightly misplaced.
	 */
	spl0();

	cold = 0;
}

void
cpu_rootconf()
{
#ifdef MFS_ROOT
	if (!mountrootfsname) {

		if (bootverbose)
			printf("Considering MFS root f/s.\n");
		if (mfs_getimage())
			mountrootfsname = "mfs";
		else if (bootverbose)
			printf("No MFS image available as root f/s.\n");
	}
#endif

#ifdef BOOTP_NFSROOT
        if (!strcmp(bootdev_protocol(), "BOOTP")
	    && !mountrootfsname && !nfs_diskless_valid) {
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
		static char rootname[] = "da0a";

		if (bootverbose)
			printf("Considering UFS root f/s.\n");
		mountrootfsname = "ufs";

		if (boot_sim) {
			struct cam_path *path;
			struct cam_periph *periph;
	    
			xpt_create_path(&path, NULL,
					cam_sim_path(boot_sim),
					bootdev_unit() / 100, 0);
			periph = cam_periph_find(path, "da");

			if (periph)
				rootdev = makedev(4, dkmakeminor(periph->unit_number,
								 COMPATIBILITY_SLICE, 0));

			xpt_free_path(path);
		}

		rootdevs[0] = rootdev;
		rootname[2] += dkunit(rootdev);
		rootdevnames[0] = rootname;
	}
#endif
}

void
cpu_dumpconf()
{
	if (setdumpdev(dumpdev) != 0)
                dumpdev = NODEV;
}

static int
setdumpdev(dev)
        dev_t dev;
{
        int maj, psize;
        long newdumplo;

        if (dev == NODEV) {
                dumpdev = dev;
                return (0);
        }
        maj = major(dev);
        if (maj >= nblkdev || bdevsw(dev) == NULL)
                return (ENXIO);         /* XXX is this right? */
        if (bdevsw(dev)->d_psize == NULL)
                return (ENXIO);         /* XXX should be ENODEV ? */
        psize = bdevsw(dev)->d_psize(dev);
        if (psize == -1)
                return (ENXIO);         /* XXX should be ENODEV ? */
        /*
         * XXX should clean up checking in dumpsys() to be more like this,
         * and nuke dodump sysctl (too many knobs), and move this to
         * kern_shutdown.c...
         */
        newdumplo = psize - Maxmem * PAGE_SIZE / DEV_BSIZE;
        if (newdumplo < 0)
                return (ENOSPC);
        dumpdev = dev;
        dumplo = newdumplo;
        return (0);
}

static int
sysctl_kern_dumpdev SYSCTL_HANDLER_ARGS
{
        int error;
        dev_t ndumpdev;

        ndumpdev = dumpdev;
        error = sysctl_handle_opaque(oidp, &ndumpdev, sizeof ndumpdev, req);
        if (error == 0 && req->newptr != NULL)
                error = setdumpdev(ndumpdev);
        return (error);
}

SYSCTL_PROC(_kern, KERN_DUMPDEV, dumpdev, CTLTYPE_OPAQUE|CTLFLAG_RW,
	    0, sizeof dumpdev, sysctl_kern_dumpdev, "T,dev_t", "");
