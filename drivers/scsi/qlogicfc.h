/*
 * QLogic ISP2x00 SCSI-FCP
 * 
 * Written by Erik H. Moe, ehm@cris.com
 * Copyright 1995, Erik H. Moe
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/* Renamed and updated to 1.3.x by Michael Griffith <grif@cs.ucr.edu> */

/* This is a version of the isp1020 driver which was modified by
 * Chris Loveland <cwl@iol.unh.edu> to support the isp2x00
 */


/*
 * $Date: 1995/09/22 02:32:56 $
 * $Revision: 0.5 $
 *
 * $Log: isp1020.h,v $
 * Revision 0.5  1995/09/22  02:32:56  root
 * do auto request sense
 *
 * Revision 0.4  1995/08/07  04:48:28  root
 * supply firmware with driver.
 * numerous bug fixes/general cleanup of code.
 *
 * Revision 0.3  1995/07/16  16:17:16  root
 * added reset/abort code.
 *
 * Revision 0.2  1995/06/29  03:19:43  root
 * fixed biosparam.
 * added queue protocol.
 *
 * Revision 0.1  1995/06/25  01:56:13  root
 * Initial release.
 *
 */

#ifndef _QLOGICFC_H
#define _QLOGICFC_H

/*
 * With the qlogic interface, every queue slot can hold a SCSI
 * command with up to 2 scatter/gather entries.  If we need more
 * than 2 entries, continuation entries can be used that hold
 * another 5 entries each.  Unlike for other drivers, this means
 * that the maximum number of scatter/gather entries we can
 * support at any given time is a function of the number of queue
 * slots available.  That is, host->can_queue and host->sg_tablesize
 * are dynamic and _not_ independent.  This all works fine because
 * requests are queued serially and the scatter/gather limit is
 * determined for each queue request anew.
 */

#define DATASEGS_PER_COMMAND 2
#define DATASEGS_PER_CONT 5

#define QLOGICFC_REQ_QUEUE_LEN	127	/* must be power of two - 1 */
#define QLOGICFC_MAX_SG(ql)	(DATASEGS_PER_COMMAND + (((ql) > 0) ? DATASEGS_PER_CONT*((ql) - 1) : 0))
#define QLOGICFC_CMD_PER_LUN    8

int isp2x00_detect(Scsi_Host_Template *);
int isp2x00_release(struct Scsi_Host *);
const char * isp2x00_info(struct Scsi_Host *);
int isp2x00_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int isp2x00_abort(Scsi_Cmnd *);
int isp2x00_reset(Scsi_Cmnd *, unsigned int);
int isp2x00_biosparam(Disk *, kdev_t, int[]);

#ifndef NULL
#define NULL (0)
#endif

#define QLOGICFC {							   \
        detect:                 isp2x00_detect,                            \
        release:                isp2x00_release,                           \
        info:                   isp2x00_info,                              \
        queuecommand:           isp2x00_queuecommand,                      \
        eh_abort_handler:       isp2x00_abort,                             \
        reset:                  isp2x00_reset,                             \
        bios_param:             isp2x00_biosparam,                         \
        can_queue:              QLOGICFC_REQ_QUEUE_LEN,                    \
        this_id:                -1,                                        \
        sg_tablesize:           QLOGICFC_MAX_SG(QLOGICFC_REQ_QUEUE_LEN),   \
	cmd_per_lun:		QLOGICFC_CMD_PER_LUN, 			   \
        present:                0,                                         \
        unchecked_isa_dma:      0,                                         \
        use_clustering:         ENABLE_CLUSTERING,			   \
	highmem_io:		1					   \
}

#endif /* _QLOGICFC_H */



