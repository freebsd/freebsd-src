/*	$FreeBSD$ */
/*	$NetBSD: rf_configure.h,v 1.4 1999/03/02 03:18:49 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/********************************
 *
 * rf_configure.h
 *
 * header file for raidframe configuration in the kernel version only.
 * configuration is invoked via ioctl rather than at boot time
 *
 *******************************/


#ifndef _RF__RF_CONFIGURE_H_
#define _RF__RF_CONFIGURE_H_

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>

#include <sys/param.h>
#include <sys/proc.h>

#if defined(__NetBSD__)
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#endif

/* the raidframe configuration, passed down through an ioctl.
 * the driver can be reconfigured (with total loss of data) at any time,
 * but it must be shut down first.
 */
struct RF_Config_s {
	RF_RowCol_t numRow, numCol, numSpare;	/* number of rows, columns,
						 * and spare disks */
	dev_t   devs[RF_MAXROW][RF_MAXCOL];	/* device numbers for disks
						 * comprising array */
	char    devnames[RF_MAXROW][RF_MAXCOL][50];	/* device names */
	dev_t   spare_devs[RF_MAXSPARE];	/* device numbers for spare
						 * disks */
	char    spare_names[RF_MAXSPARE][50];	/* device names */
	RF_SectorNum_t sectPerSU;	/* sectors per stripe unit */
	RF_StripeNum_t SUsPerPU;/* stripe units per parity unit */
	RF_StripeNum_t SUsPerRU;/* stripe units per reconstruction unit */
	RF_ParityConfig_t parityConfig;	/* identifies the RAID architecture to
					 * be used */
	RF_DiskQueueType_t diskQueueType;	/* 'f' = fifo, 'c' = cvscan,
						 * not used in kernel */
	char    maxOutstandingDiskReqs;	/* # concurrent reqs to be sent to a
					 * disk.  not used in kernel. */
	char    debugVars[RF_MAXDBGV][50];	/* space for specifying debug
						 * variables & their values */
	unsigned int layoutSpecificSize;	/* size in bytes of
						 * layout-specific info */
	void   *layoutSpecific;	/* a pointer to a layout-specific structure to
				 * be copied in */
	int     force;                          /* if !0, ignore many fatal
						   configuration conditions */
	/* 
	   "force" is used to override cases where the component labels would 
	   indicate that configuration should not proceed without user 
	   intervention
	 */
};
#ifndef _KERNEL
int     rf_MakeConfig(char *configname, RF_Config_t * cfgPtr);
int     rf_MakeLayoutSpecificNULL(FILE * fp, RF_Config_t * cfgPtr, void *arg);
int     rf_MakeLayoutSpecificDeclustered(FILE * configfp, RF_Config_t * cfgPtr, void *arg);
void   *rf_ReadSpareTable(RF_SparetWait_t * req, char *fname);
#endif				/* !_KERNEL */

#endif				/* !_RF__RF_CONFIGURE_H_ */
