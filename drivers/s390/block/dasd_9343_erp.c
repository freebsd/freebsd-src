/* 
 * File...........: linux/drivers/s390/block/dasd_9345_erp.h
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 2000
 */

#include <asm/ccwcache.h>
#include "dasd_int.h"

#ifdef PRINTK_HEADER
#undef PRINTK_HEADER
#define PRINTK_HEADER "dasd_erp(9343)"
#endif				/* PRINTK_HEADER */

dasd_era_t dasd_9343_erp_examine (ccw_req_t * cqr, devstat_t * stat)
{
	if (stat->cstat == 0x00 &&
	    stat->dstat == (DEV_STAT_CHN_END | DEV_STAT_DEV_END))
		    return dasd_era_none;
	return dasd_era_recover;
}
