/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 * The AMD Elan sc520 is a system-on-chip gadget which is used in embedded
 * kind of things, see www.soekris.com for instance, and it has a few quirks
 * we need to deal with.
 * Unfortunately we cannot identify the gadget by CPUID output because it
 * depends on strapping options and only the stepping field may be useful
 * and those are undocumented from AMDs side.
 *
 * So instead we recognize the on-chip host-PCI bridge and call back from
 * sys/i386/pci/pci_bus.c to here if we find it.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/timetc.h>
#include <sys/proc.h>

#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

uint16_t *elan_mmcr;


static unsigned
elan_get_timecount(struct timecounter *tc)
{
	return (elan_mmcr[0xc84 / 2]);
}

static struct timecounter elan_timecounter = {
	elan_get_timecount,
	0,
	0xffff,
	33333333 / 4,
	"ELAN"
};

void
init_AMD_Elan_sc520(void)
{
	u_int new;
	int i;

	if (bootverbose)
		printf("Doing h0h0magic for AMD Elan sc520\n");
	elan_mmcr = pmap_mapdev(0xfffef000, 0x1000);

	/*-
	 * The i8254 is driven with a nonstandard frequency which is
	 * derived thusly:
	 *   f = 32768 * 45 * 25 / 31 = 1189161.29...
	 * We use the sysctl to get the timecounter etc into whack.
	 */
	
	new = 1189161;
	i = kernel_sysctlbyname(&thread0, "machdep.i8254_freq", 
	    NULL, 0, 
	    &new, sizeof new, 
	    NULL);
	if (bootverbose)
		printf("sysctl machdep.i8254_freq=%d returns %d\n", new, i);

	/* Start GP timer #2 and use it as timecounter, hz permitting */
	elan_mmcr[0xc82 / 2] = 0xc001;
	tc_init(&elan_timecounter);
}


/*
 * Device driver initialization stuff
 */

static d_open_t	elan_open;
static d_close_t elan_close;
static d_ioctl_t elan_ioctl;
static d_mmap_t elan_mmap;

#define CDEV_MAJOR 100			/* Share with xrpu */
static struct cdevsw elan_cdevsw = {
	/* open */	elan_open,
	/* close */	elan_close,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	elan_ioctl,
	/* poll */	nopoll,
	/* mmap */	elan_mmap,
	/* strategy */	nostrategy,
	/* name */	"elan",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static int
elan_open(dev_t dev, int flag, int mode, struct  thread *td)
{
	return (0);
}

static int
elan_close(dev_t dev, int flag, int mode, struct  thread *td)
{ 
	return (0);
}

static int
elan_mmap(dev_t dev, vm_offset_t offset, int nprot)
{
	if (offset >= 0x1000) 
		return (-1);
	return (i386_btop(0xfffef000));
}

static int
elan_ioctl(dev_t dev, u_long cmd, caddr_t arg, int flag, struct  thread *tdr)
{
	return(ENOENT);
}

static void
elan_drvinit(void)
{

	if (elan_mmcr == NULL)
		return;
	printf("Elan-mmcr driver: MMCR at %p\n", elan_mmcr);
	make_dev(&elan_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "elan-mmcr");
	return;
}

SYSINIT(elan, SI_SUB_PSEUDO, SI_ORDER_MIDDLE+CDEV_MAJOR,elan_drvinit,NULL);
