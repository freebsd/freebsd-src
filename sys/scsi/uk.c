/* 
 * Driver for a device we can't identify.
 * by Julian Elischer (julian@tfs.com)
 *
 *      $Id: uk.c,v 1.6 1995/01/08 13:38:38 dufault Exp $
 */

#include <sys/param.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

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
	ukattach,
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

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval 
ukattach(struct scsi_link *sc_link)
{
	printf("unknown device\n");

	return 0;
}
