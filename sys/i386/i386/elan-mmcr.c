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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>

#include <machine/md_var.h>

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
	printf("Elan-mmcr driver\n");
	make_dev(&elan_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "elan-mmcr");
	return;
}

 
SYSINIT(elan, SI_SUB_PSEUDO, SI_ORDER_MIDDLE+CDEV_MAJOR,elan_drvinit,NULL);
