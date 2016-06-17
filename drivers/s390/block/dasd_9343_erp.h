/* 
 * File...........: linux/drivers/s390/block/dasd_9343_erp.h
 * Author(s)......: Horst Hummel <Horst Hummel@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 2000
 *
 * History of changes (starts July 2000)
 */

#ifndef DASD_9343_ERP_H
#define DASD_9343_ERP_H

dasd_era_t dasd_9343_erp_examine (ccw_req_t *, devstat_t *);

ccw_req_t *dasd_9343_erp_action (ccw_req_t *);

#endif				/* DASD_9343_ERP_H */
