/* 
 * Dummy driver for a device we can't identify.
 * by Julian Elischer (julian@tfs.com)
 *
 *      $Id: uk.c,v 1.4 1994/08/13 03:50:31 wollman Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

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
#define UK_KNOWN	0x02
	struct scsi_link *sc_link;	/* all the inter level info */
};

struct uk_driver {
	u_int32		size;
	struct uk_data	**uk_data;
} uk_driver;

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
	struct uk_data *uk, **ukrealloc;
	unsigned char *tbl;

	SC_DEBUG(sc_link, SDEV_DB2, ("ukattach: "));

	/*
	 * allocate the resources for another drive
	 * if we have already allocate a uk_data pointer we must
	 * copy the old pointers into a new region that is
	 * larger and release the old region, aka realloc
	 */
	/* XXX
	 * This if will always be true for now, but future code may
	 * preallocate more units to reduce overhead.  This would be
	 * done by changing the malloc to be (next_uk_unit * x) and
	 * the uk_driver.size++ to be +x
	 */
	unit = next_uk_unit++;
	if (unit >= uk_driver.size) {
		ukrealloc =
			malloc(sizeof(uk_driver.uk_data) * next_uk_unit,
			M_DEVBUF, M_NOWAIT);
		if (!ukrealloc) {
			printf("uk%ld: malloc failed for ukrealloc\n", unit);
			return (0);
		}
		/* Make sure we have something to copy before we copy it */
		bzero(ukrealloc, sizeof(uk_driver.uk_data) * next_uk_unit);
		if (uk_driver.size) {
			bcopy(uk_driver.uk_data, ukrealloc,
				sizeof(uk_driver.uk_data) * uk_driver.size);
			free(uk_driver.uk_data, M_DEVBUF);
		}
		uk_driver.uk_data = ukrealloc;
		uk_driver.uk_data[unit] = NULL;
		uk_driver.size++;
	}
	if (uk_driver.uk_data[unit]) {
		printf("uk%ld: Already has storage!\n", unit);
		return (0);
	}
	/*
	 * alloate the per drive data area
	 */
	uk = uk_driver.uk_data[unit] =
		malloc(sizeof(struct uk_data), M_DEVBUF, M_NOWAIT);
	if (!uk) {
		printf("uk%ld: malloc failed for uk_data\n", unit);
		return (0);
	}
	bzero(uk, sizeof(struct uk_data));

	/*
	 * Store information needed to contact our base driver
	 */
	uk->sc_link = sc_link;
	sc_link->device = &uk_switch;
	sc_link->dev_unit = unit;

	printf("uk%d: unknown device\n", unit);
	uk->flags = UK_KNOWN;

	return 1;		/* XXX ??? */
}

/*
 *    open the device.
 */
errval 
ukopen(dev)
	dev_t dev;
{
	errval  errcode = 0;
	u_int32 unit, mode;
	struct uk_data *uk;
	struct scsi_link *sc_link;
	unit = minor(dev);

	/*
	 * Check the unit is legal 
	 */
	if (unit >= uk_driver.size) {
		printf("uk%d: uk %d  > %d\n", unit, unit, uk_driver.size);
		return ENXIO;
	}
	uk = uk_driver.uk_data[unit];

	/*
	 * Make sure the device has been initialised
	 */
	if((!uk) || (!(uk->flags & UK_KNOWN))) {
		printf("uk%d: not set up\n", unit);
		return ENXIO;
	}
		
	/*
	 * Only allow one at a time
	 */
	sc_link = uk->sc_link;
	if (sc_link->flags & SDEV_OPEN) {
		printf("uk%d: already open\n", unit);
		return ENXIO;
	}
	sc_link->flags |= SDEV_OPEN;
	SC_DEBUG(sc_link, SDEV_DB1, ("ukopen: dev=0x%x (unit %d (of %d))\n",
		dev, unit, uk_driver.size));
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
	dev_t dev;
{
	u_int32 unit = minor(dev);
	unsigned mode; /* XXX !!! XXX FIXME!!! 0??? */
	struct uk_data *uk;
	struct scsi_link *sc_link;

	uk = uk_driver.uk_data[unit];
	sc_link = uk->sc_link;

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
	int mode;
{
	unsigned char unit;
	struct uk_data *uk;
	struct scsi_link *sc_link;

	/*
	 * Find the device that the user is talking about
	 */
	unit = minor(dev);
	uk = uk_driver.uk_data[unit];
	sc_link = uk->sc_link;
	return(scsi_do_ioctl(sc_link,cmd,arg,mode));
}

