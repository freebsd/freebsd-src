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
 * $FreeBSD$
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
#include "opt_isa.h"
#include "opt_nfs.h"
#include "opt_nfsroot.h"
#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/cons.h>

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsdiskless.h>

#include <machine/bootinfo.h>
#include <machine/md_var.h>
#ifdef APIC_IO
#include <machine/smp.h>
#else
#include <i386/isa/icu.h>
#endif /* APIC_IO */

#ifdef DEV_ISA
#include <isa/isavar.h>

device_t isa_bus_device = 0;
#endif

static void	configure_first(void *);
static void	configure(void *);
static void	configure_final(void *);

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
/* SI_ORDER_SECOND is hookable */
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
/* SI_ORDER_MIDDLE is hookable */
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

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

#ifdef DEV_ISA
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
}

static void
configure_final(dummy)
	void *dummy;
{

	cninit_finish(); 

	if (bootverbose) {

#ifdef APIC_IO
		imen_dump();
#endif /* APIC_IO */

#ifdef PC98
		{
		int i;
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
		}
#endif

		printf("Device configuration finished.\n");
	}
	cold = 0;
}

/*
 * Do legacy root filesystem discovery.
 */
void
cpu_rootconf()
{
#ifdef BOOTP
        bootpc_init();
#endif
#if defined(NFSCLIENT) && defined(NFS_ROOT)
#if !defined(BOOTP_NFSROOT)
	nfs_setup_diskless();
	if (nfs_diskless_valid)
#endif
		rootdevnames[0] = "nfs:";
#endif
}
SYSINIT(cpu_rootconf, SI_SUB_ROOT_CONF, SI_ORDER_FIRST, cpu_rootconf, NULL)
