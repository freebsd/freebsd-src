/*	$FreeBSD$ */
/*	$NetBSD: rf_disks.h,v 1.8 2000/03/27 03:25:17 oster Exp $	*/
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

/*
 * rf_disks.h -- header file for code related to physical disks
 */

#ifndef _RF__RF_DISKS_H_
#define _RF__RF_DISKS_H_

#include <sys/types.h>

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_bsd.h>

/*
 * A physical disk can be in one of several states:
 * IF YOU ADD A STATE, CHECK TO SEE IF YOU NEED TO MODIFY RF_DEAD_DISK() BELOW.
 */
enum RF_DiskStatus_e {
	rf_ds_optimal,		/* no problems */
	rf_ds_failed,		/* reconstruction ongoing */
	rf_ds_reconstructing,	/* reconstruction complete to spare, dead disk
				 * not yet replaced */
	rf_ds_dist_spared,	/* reconstruction complete to distributed
				 * spare space, dead disk not yet replaced */
	rf_ds_spared,		/* reconstruction complete to distributed
				 * spare space, dead disk not yet replaced */
	rf_ds_spare,		/* an available spare disk */
	rf_ds_used_spare	/* a spare which has been used, and hence is
				 * not available */
};
typedef enum RF_DiskStatus_e RF_DiskStatus_t;

struct RF_RaidDisk_s {
	char    devname[56];	/* name of device file */
	RF_DiskStatus_t status;	/* whether it is up or down */
	RF_RowCol_t spareRow;	/* if in status "spared", this identifies the
				 * spare disk */
	RF_RowCol_t spareCol;	/* if in status "spared", this identifies the
				 * spare disk */
	RF_SectorCount_t numBlocks;	/* number of blocks, obtained via READ
					 * CAPACITY */
	int     blockSize;
	RF_SectorCount_t partitionSize; /* The *actual* and *full* size of 
					   the partition, from the disklabel */
	int     auto_configured;/* 1 if this component was autoconfigured.
				   0 otherwise. */
	dev_t   dev;
};
/*
 * An RF_DiskOp_t ptr is really a pointer to a UAGT_CCB, but I want
 * to isolate the cam layer from all other layers, so I typecast to/from
 * RF_DiskOp_t * (i.e. void *) at the interfaces.
 */
typedef void RF_DiskOp_t;

/* if a disk is in any of these states, it is inaccessible */
#define RF_DEAD_DISK(_dstat_) (((_dstat_) == rf_ds_spared) || \
	((_dstat_) == rf_ds_reconstructing) || ((_dstat_) == rf_ds_failed) || \
	((_dstat_) == rf_ds_dist_spared))

#ifdef _KERNEL
#include <dev/raidframe/rf_bsd.h>

int rf_ConfigureDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
		      RF_Config_t * cfgPtr);
int rf_ConfigureSpareDisks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
			   RF_Config_t * cfgPtr);
int rf_ConfigureDisk(RF_Raid_t * raidPtr, char *buf, RF_RaidDisk_t * diskPtr,
		     RF_RowCol_t row, RF_RowCol_t col);
int rf_AutoConfigureDisks(RF_Raid_t *raidPtr, RF_Config_t *cfgPtr,
			  RF_AutoConfig_t *auto_config);
int rf_CheckLabels( RF_Raid_t *, RF_Config_t *);
int rf_add_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);
int rf_remove_hot_spare(RF_Raid_t *raidPtr, RF_SingleComponent_t *sparePtr);
int rf_delete_component(RF_Raid_t *raidPtr, RF_SingleComponent_t *component);
int rf_incorporate_hot_spare(RF_Raid_t *raidPtr, 
			     RF_SingleComponent_t *component);
#endif /* _KERNEL */
#endif				/* !_RF__RF_DISKS_H_ */
