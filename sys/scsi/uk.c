/* 
 * Dummy driver for a device we can't identify.
 * by Julian Elischer (julian@tfs.com)
 *
 *      $Id: uk.c,v 1.2 93/10/11 11:53:28 julian Exp Locker: julian $
 */


#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#define NUK 16

/*
 * This driver is so simple it uses all the default services
 */
struct scsi_device uk_switch =
{
    NULL,
    NULL,
    NULL,
    NULL,
    "uk",
    0,
    0, 0
};

struct uk_data {
	u_int32 flags;
	struct scsi_link *sc_link;	/* all the inter level info */
} uk_data[NUK];

#define UK_KNOWN	0x02

static u_int32 next_uk_unit = 0;

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval 
ukattach(sc_link)
	struct scsi_link *sc_link;
{
	u_int32 unit, i, stat;
	unsigned char *tbl;

	SC_DEBUG(sc_link, SDEV_DB2, ("ukattach: "));
	/*
	 * Check we have the resources for another drive
	 */
	unit = next_uk_unit++;
	if (unit >= NUK) {
		printf("Too many unknown devices..(%d > %d) reconfigure kernel\n",
				(unit + 1), NUK);
		return (0);
	}
	/*
	 * Store information needed to contact our base driver
	 */
	uk_data[unit].sc_link = sc_link;
	sc_link->device = &uk_switch;
	sc_link->dev_unit = unit;

	printf("uk%d: unknown device\n", unit);
	uk_data[unit].flags = UK_KNOWN;

	return;

}

/*
 *    open the device.
 */
errval 
ukopen(dev)
{
	errval  errcode = 0;
	u_int32 unit, mode;
	struct scsi_link *sc_link;
	unit = minor(dev);

	/*
	 * Check the unit is legal 
	 */
	if (unit >= NUK) {
		printf("uk%d: uk %d  > %d\n", unit, unit, NUK);
		return ENXIO;
	}

	/*
	 * Make sure the device has been initialised
	 */
	if((uk_data[unit].flags & UK_KNOWN) == 0) {
		printf("uk%d: not set up\n", unit);
		return ENXIO;
	}
		
	/*
	 * Only allow one at a time
	 */
	sc_link = uk_data[unit].sc_link;
	if (sc_link->flags & SDEV_OPEN) {
		printf("uk%d: already open\n", unit);
		return ENXIO;
	}
	sc_link->flags |= SDEV_OPEN;
	SC_DEBUG(sc_link, SDEV_DB1, ("ukopen: dev=0x%x (unit %d (of %d))\n"
		,dev, unit, NUK));
	/*
	 * Catch any unit attention errors.
	 */
	return 0;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
errval 
ukclose(dev)
{
	unsigned char unit, mode;
	struct scsi_link *sc_link;

	sc_link = uk_data[unit].sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("Closing device"));
	sc_link->flags &= ~SDEV_OPEN;
	return (0);
}

/*
 * Perform special action on behalf of the user
 * Only does generic scsi ioctls.
 */
errval 
ukioctl(dev, cmd, arg, mode)
	dev_t   dev;
	u_int32 cmd;
	caddr_t arg;
{
	unsigned char unit;
	struct scsi_link *sc_link;

	/*
	 * Find the device that the user is talking about
	 */
	unit = minor(dev);
	sc_link = uk_data[unit].sc_link;
	return(scsi_do_ioctl(sc_link,cmd,arg,mode));
}

