/*
 * Driver for a device we can't identify.
 * by Julian Elischer (julian@tfs.com)
 *
 *      $Id: uk.c,v 1.17 1997/03/23 06:33:54 bde Exp $
 *
 * If you find that you are adding any code to this file look closely
 * at putting it in "scsi_driver.c" instead.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#include <scsi/scsiconf.h>
#include <scsi/scsi_driver.h>

static	d_open_t	ukopen;
static	d_close_t	ukclose;
static	d_ioctl_t	ukioctl;

#define CDEV_MAJOR 31
static struct cdevsw uk_cdevsw = 
	{ ukopen,	ukclose,	noread,         nowrite,      	/*31*/
	  ukioctl,	nostop,		nullreset,	nodevtotty,/* unknown */
	  seltrue,	nommap,		NULL,	"uk"	,NULL,	-1 };

SCSI_DEVICE_ENTRIES(uk)

struct scsi_device uk_switch =
{
	NULL,
	NULL,
	NULL,
	NULL,
	"uk",
	0,
	{0, 0},
	SDEV_ONCE_ONLY|SDEV_UK,	/* Only one open allowed */
	0,
	"Unknown",
	ukopen,
	0,
	T_UNKNOWN,
	0,
	0,
	0,
	0,
	0,
	0,
};


static uk_devsw_installed = 0;

static void 	uk_drvinit(void *unused)
{
	dev_t dev;

	if( ! uk_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&uk_cdevsw, NULL);
		uk_devsw_installed = 1;
    	}
}

SYSINIT(ukdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,uk_drvinit,NULL)


