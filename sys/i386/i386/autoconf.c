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

#if defined(NFS) && defined(NFS_ROOT)
static void	pxe_setup_nfsdiskless(void);
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
	pxe_setup_nfsdiskless();
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
	if (devsw(dev) == NULL)
		return;
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

#if defined(NFS) && defined(NFS_ROOT)

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>

extern struct nfs_diskless	nfs_diskless;

static int
inaddr_to_sockaddr(char *ev, struct sockaddr_in *sa)
{
	u_int32_t	a[4];
	char		*cp;

	bzero(sa, sizeof(*sa));
	sa->sin_len = sizeof(*sa);
	sa->sin_family = AF_INET;

	if ((cp = getenv(ev)) == NULL)
		return(1);
	if (sscanf(cp, "%d.%d.%d.%d", &a[0], &a[1], &a[2], &a[3]) != 4)
		return(1);
	/* XXX is this ordering correct? */
	sa->sin_addr.s_addr = (a[3] << 24) + (a[2] << 16) + (a[1] << 8) + a[0];
	return(0);
}

static int
hwaddr_to_sockaddr(char *ev, struct sockaddr_dl *sa)
{
	char		*cp;
	u_int32_t	a[6];

	bzero(sa, sizeof(*sa));
	sa->sdl_len = sizeof(*sa);
	sa->sdl_family = AF_LINK;
	sa->sdl_type = IFT_ETHER;
	sa->sdl_alen = ETHER_ADDR_LEN;
	if ((cp = getenv(ev)) == NULL)
		return(1);
	if (sscanf(cp, "%x:%x:%x:%x:%x:%x", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]) != 6)
		return(1);
	sa->sdl_data[0] = a[0];
	sa->sdl_data[1] = a[1];
	sa->sdl_data[2] = a[2];
	sa->sdl_data[3] = a[3];
	sa->sdl_data[4] = a[4];
	sa->sdl_data[5] = a[5];
	return(0);
}

static int
decode_nfshandle(char *ev, u_char *fh) 
{
	u_char	*cp;
	int	len, val;

	if (((cp = getenv(ev)) == NULL) || (strlen(cp) < 2) || (*cp != 'X'))
		return(0);
	len = 0;
	cp++;
	for (;;) {
		if (*cp == 'X')
			return(len);
		if ((sscanf(cp, "%2x", &val) != 1) || (val > 0xff))
			return(0);
		*(fh++) = val;
		len++;
		cp += 2;
		if (len > NFSX_V2FH)
		    return(0);
	}
}

/*
 * Populate the essential fields in the nfsv3_diskless structure.
 *
 * The loader is expected to export the following environment variables:
 *
 * boot.netif.ip		IP address on boot interface
 * boot.netif.netmask		netmask on boot interface
 * boot.netif.gateway		default gateway (optional)
 * boot.netif.hwaddr		hardware address of boot interface
 * boot.nfsroot.server		IP address of root filesystem server
 * boot.nfsroot.path		path of the root filesystem on server
 * boot.nfsroot.nfshandle	NFS handle for root filesystem on server
 */
static void
pxe_setup_nfsdiskless()
{
	struct nfs_diskless	*nd = &nfs_diskless;
	struct ifnet		*ifp;
	struct ifaddr		*ifa;
	struct sockaddr_dl	*sdl, ourdl;
	struct sockaddr_in	myaddr, netmask;
	char			*cp;

	/* set up interface */
	if (inaddr_to_sockaddr("boot.netif.ip", &myaddr))
		return;
	if (inaddr_to_sockaddr("boot.netif.netmask", &netmask)) {
		printf("PXE: no netmask\n");
		return;
	}
	bcopy(&myaddr, &nd->myif.ifra_addr, sizeof(myaddr));
	bcopy(&myaddr, &nd->myif.ifra_broadaddr, sizeof(myaddr));
	((struct sockaddr_in *) &nd->myif.ifra_broadaddr)->sin_addr.s_addr =
		myaddr.sin_addr.s_addr | ~ netmask.sin_addr.s_addr;
	bcopy(&netmask, &nd->myif.ifra_mask, sizeof(netmask));

	if (hwaddr_to_sockaddr("boot.netif.hwaddr", &ourdl)) {
		printf("PXE: no hardware address\n");
		return;
	}
	ifa = NULL;
	ifp = TAILQ_FIRST(&ifnet);
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			if ((ifa->ifa_addr->sa_family == AF_LINK) &&
			    (sdl = ((struct sockaddr_dl *)ifa->ifa_addr))) {
				if ((sdl->sdl_type == ourdl.sdl_type) &&
				    (sdl->sdl_alen == ourdl.sdl_alen) &&
				    !bcmp(sdl->sdl_data + sdl->sdl_nlen,
					  ourdl.sdl_data + ourdl.sdl_nlen, 
					  sdl->sdl_alen))
				    goto match_done;
			}
		}
	}
	printf("PXE: no interface\n");
	return;	/* no matching interface */
match_done:
	sprintf(nd->myif.ifra_name, "%s%d", ifp->if_name, ifp->if_unit);

	
	/* set up gateway */
	inaddr_to_sockaddr("boot.netif.gateway", &nd->mygateway);

	/* XXX set up swap? */

	/* set up root mount */
	nd->root_args.rsize = 8192;		/* XXX tunable? */
	nd->root_args.wsize = 8192;
	nd->root_args.sotype = SOCK_DGRAM;
	nd->root_args.flags = (NFSMNT_WSIZE | NFSMNT_RSIZE | NFSMNT_RESVPORT);
	if (inaddr_to_sockaddr("boot.nfsroot.server", &nd->root_saddr)) {
		printf("PXE: no server\n");
		return;
	}
	nd->root_saddr.sin_port = htons(NFS_PORT);
	if (decode_nfshandle("boot.nfsroot.nfshandle", &nd->root_fh[0]) == 0) {
		printf("PXE: no NFS handle\n");
		return;
	}
	if ((cp = getenv("boot.nfsroot.path")) != NULL)
		strncpy(nd->root_hostnam, cp, MNAMELEN - 1);

	nfs_diskless_valid = 1;
}
#endif
