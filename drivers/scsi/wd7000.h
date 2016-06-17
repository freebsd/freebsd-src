#ifndef _WD7000_H

/* $Id: $
 *
 * Header file for the WD-7000 driver for Linux
 *
 * John Boyd <boyd@cis.ohio-state.edu>  Jan 1994:
 * This file has been reduced to only the definitions needed for the
 * WD7000 host structure.
 *
 * Revision by Miroslav Zagorac <zaga@fly.cc.fer.hr>  Jun 1997.
 */

#include <linux/types.h>
#include <linux/kdev_t.h>

int wd7000_set_info (char *buffer, int length, struct Scsi_Host *host);
int wd7000_proc_info (char *buffer, char **start, off_t offset, int length, int hostno, int inout);
int wd7000_detect (Scsi_Host_Template *);
int wd7000_command (Scsi_Cmnd *);
int wd7000_queuecommand (Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int wd7000_abort (Scsi_Cmnd *);
int wd7000_reset (Scsi_Cmnd *, unsigned int);
int wd7000_biosparam (Disk *, kdev_t, int *);

#ifndef NULL
#define NULL 0L
#endif

/*
 *  In this version, sg_tablesize now defaults to WD7000_SG, and will
 *  be set to SG_NONE for older boards.  This is the reverse of the
 *  previous default, and was changed so that the driver-level
 *  Scsi_Host_Template would reflect the driver's support for scatter/
 *  gather.
 *
 *  Also, it has been reported that boards at Revision 6 support scatter/
 *  gather, so the new definition of an "older" board has been changed
 *  accordingly.
 */
#define WD7000_Q    16
#define WD7000_SG   16

#define WD7000 {						\
	proc_name:		"wd7000",			\
	proc_info:		wd7000_proc_info,		\
	name:			"Western Digital WD-7000",	\
	detect:			wd7000_detect,			\
	command:		wd7000_command,			\
	queuecommand:		wd7000_queuecommand,		\
	abort:			wd7000_abort,			\
	reset:			wd7000_reset,			\
	bios_param:		wd7000_biosparam,		\
	can_queue:		WD7000_Q,			\
	this_id:		7,				\
	sg_tablesize:		WD7000_SG,			\
	cmd_per_lun:		1,				\
	unchecked_isa_dma:	1,				\
	use_clustering:		ENABLE_CLUSTERING,		\
	use_new_eh_code:	0				\
}
#endif
