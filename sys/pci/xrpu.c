/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 * A very simple device driver for PCI cards based on Xilinx 6200 series
 * FPGA/RPU devices.  Current Functionality is to allow you to open and
 * mmap the entire thing into your program.
 *
 * Hardware currently supported:
 *	www.vcc.com HotWorks 1 6216 based card.
 *
 */

#ifndef DEVFS
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/devfsext.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>

static	char*	xrpu_probe  (pcici_t tag, pcidi_t type);
static	void	xrpu_attach (pcici_t tag, int unit);
static	u_long	xrpu_count;

static	vm_offset_t virbase, physbase;

static int
xrpuopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

static int
xrpuclose(dev_t dev, int flag, int mode, struct proc *p)
{ 
	return (0);
}

static int
xrpummap(dev_t dev, int offset, int nprot)
{
	if (offset >= 0x1000000) 
		return (-1);
	return (i386_btop(physbase + offset));
}

/*
 * Device driver initialization stuff
 */

#define CDEV_MAJOR 100
static struct cdevsw xrpudevsw = {
	xrpuopen,	xrpuclose,	noread,		nowrite,
	noioctl,	nullstop,	noreset,	nodevtotty,
	seltrue,	xrpummap,	nostrategy,	"xrpu",
	NULL,		-1
};

/*
 * PCI initialization stuff
 */

static struct pci_device xrpu_device = {
	"xrpu",
	xrpu_probe,
	xrpu_attach,
	&xrpu_count,
	NULL
};

DATA_SET (pcidevice_set, xrpu_device);

static char* 
xrpu_probe (pcici_t tag, pcidi_t typea)
{
	int data = pci_conf_read(tag, PCI_CLASS_REG);
	u_int id = pci_conf_read(tag, PCI_ID_REG);
	const char *vendor, *chip, *type;

	vendor = chip = type = 0;

	if (id == 0x6216133e) {
		return "VCC Hotworks-I xc6216";
	}
	return 0;
}

static void
xrpu_attach (pcici_t tag, int unit)
{
	dev_t cdev = makedev(CDEV_MAJOR, 0);

	pci_map_mem(tag, PCI_MAP_REG_START, &virbase, &physbase);

	printf("Mapped physbase %p to virbase %p\n", physbase, virbase);

	cdevsw_add(&cdev, &xrpudevsw, NULL);

	devfs_add_devswf(&xrpudevsw, 0, DV_CHR, UID_ROOT, GID_WHEEL, 0600,
		"xrpu0", 0);
}
#endif /* DEVFS */
