/* AM53/79C974 (PCscsi) driver release 0.5

 * The architecture and much of the code of this device
 * driver was originally developed by Drew Eckhardt for
 * the NCR5380. The following copyrights apply:
 *  For the architecture and all parts similar to the NCR5380:
 *    Copyright 1993, Drew Eckhardt
 *      Visionary Computing 
 *      (Unix and Linux consulting and custom programming)
 *      drew@colorado.edu
 *      +1 (303) 666-5836
 *
 *  The AM53C974_nobios_detect code was originally developed by
 *   Robin Cutshaw (robin@xfree86.org) and is used here in a 
 *   modified form.
 *
 *  For the other parts:
 *    Copyright 1994, D. Frieauff
 *    EMail: fri@rsx42sun0.dofn.de
 *    Phone: x49-7545-8-2256 , x49-7541-42305
 */

/*
 * $Log: AM53C974.h,v $
 */

#ifndef AM53C974_H
#define AM53C974_H

#include <scsi/scsicam.h>

struct AM53C974_hostdata {
	volatile unsigned in_reset:1;	/* flag, says bus reset pending */
	volatile unsigned aborted:1;	/* flag, says aborted */
	volatile unsigned selecting:1;	/* selection started, but not yet finished */
	volatile unsigned disconnecting: 1;	/* disconnection started, but not yet finished */
	volatile unsigned dma_busy:1;	/* dma busy when service request for info transfer received */
	volatile unsigned char msgout[10];	/* message to output in MSGOUT_PHASE */
	volatile unsigned char last_message[10];	/* last message OUT */
	volatile Scsi_Cmnd *issue_queue;	/* waiting to be issued */
	volatile Scsi_Cmnd *disconnected_queue;		/* waiting for reconnect */
	volatile Scsi_Cmnd *sel_cmd;	/* command for selection */
	volatile Scsi_Cmnd *connected;	/* currently connected command */
	volatile unsigned char busy[8];		/* index = target, bit = lun */
	unsigned char sync_per[8];	/* synchronous transfer period (in effect) */
	unsigned char sync_off[8];	/* synchronous offset (in effect) */
	unsigned char sync_neg[8];	/* sync. negotiation performed (in effect) */
	unsigned char sync_en[8];	/* sync. negotiation performed (in effect) */
	unsigned char max_rate[8];	/* max. transfer rate (setup) */
	unsigned char max_offset[8];	/* max. sync. offset (setup), only valid if corresponding sync_en is nonzero */
};

#define AM53C974 {				  \
    proc_name:      "am53c974",    	          \
    name:           "AM53C974",        		  \
    detect:         AM53C974_pci_detect,	  \
    release:        AM53C974_release,		  \
    info:           AM53C974_info,     		  \
    command:        AM53C974_command,  		  \
    queuecommand:   AM53C974_queue_command,	  \
    abort:          AM53C974_abort,    		  \
    reset:          AM53C974_reset,    		  \
    bios_param:     scsicam_bios_param,		  \
    can_queue:      12,                 	  \
    this_id:        -1,                           \
    sg_tablesize:   SG_ALL,            		  \
    cmd_per_lun:    1,                 		  \
    use_clustering: DISABLE_CLUSTERING 		  \
    }

static int AM53C974_pci_detect(Scsi_Host_Template * tpnt);
static int AM53C974_release(struct Scsi_Host *shp);
static const char *AM53C974_info(struct Scsi_Host *);
static int AM53C974_command(Scsi_Cmnd * SCpnt);
static int AM53C974_queue_command(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *));
static int AM53C974_abort(Scsi_Cmnd * cmd);
static int AM53C974_reset(Scsi_Cmnd * cmd, unsigned int);

#endif				/* AM53C974_H */
