/*
 * Driver for a device we can't identify.
 * by Julian Elischer (julian@tfs.com)
 *
 *      $Id: uk.c,v 1.10 1995/11/29 10:49:07 julian Exp $
 *
 * If you find that you are adding any code to this file look closely
 * at putting it in "scsi_driver.c" instead.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#ifdef JREMOD
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 31
#endif /*JREMOD*/


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
	SDEV_ONCE_ONLY,	/* Only one open allowed */
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

#ifdef JREMOD
struct cdevsw uk_cdevsw = 
	{ ukopen,	ukclose,	noread,         nowrite,      	/*31*/
	  ukioctl,	nostop,		nullreset,	nodevtotty,/* unknown */
	  seltrue,	nommap,		NULL };			   /* scsi */

static uk_devsw_installed = 0;

static void 	uk_drvinit(void *unused)
{
	dev_t dev;

	if( ! uk_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&uk_cdevsw,NULL);
		uk_devsw_installed = 1;
#ifdef DEVFS
		{
			int x;
/* default for a simple device with no probe routine (usually delete this) */
			x=devfs_add_devsw(
/*	path	name	devsw		minor	type   uid gid perm*/
	"/",	"uk",	major(dev),	0,	DV_CHR,	0,  0, 0600);
		}
#endif
    	}
}

SYSINIT(ukdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,uk_drvinit,NULL)

#endif /* JREMOD */

