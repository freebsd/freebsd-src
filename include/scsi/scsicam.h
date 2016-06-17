/*
 * scsicam.h - SCSI CAM support functions, use for HDIO_GETGEO, etc.
 *
 * Copyright 1993, 1994 Drew Eckhardt
 *      Visionary Computing 
 *      (Unix and Linux consulting and custom programming)
 *      drew@Colorado.EDU
 *	+1 (303) 786-7975
 *
 * For more information, please consult the SCSI-CAM draft.
 */

#ifndef SCSICAM_H
#define SCSICAM_H
#include <linux/kdev_t.h>
extern int scsicam_bios_param (Disk *disk, kdev_t dev, int *ip);
extern int scsi_partsize(struct buffer_head *bh, unsigned long capacity,
           unsigned int  *cyls, unsigned int *hds, unsigned int *secs);
#endif /* def SCSICAM_H */
