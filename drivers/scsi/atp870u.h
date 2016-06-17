#ifndef _ATP870U_H

/* $Id: atp870u.h,v 1.0 1997/05/07 15:09:00 root Exp root $

 * Header file for the ACARD 870U/W driver for Linux
 *
 * $Log: atp870u.h,v $
 * Revision 1.0  1997/05/07  15:09:00  root
 * Initial revision
 *
 */

#include <linux/types.h>
#include <linux/kdev_t.h>

/* I/O Port */

#define MAX_CDB 12
#define MAX_SENSE 14

int atp870u_detect(Scsi_Host_Template *);
int atp870u_command(Scsi_Cmnd *);
int atp870u_queuecommand(Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
int atp870u_abort(Scsi_Cmnd *);
int atp870u_reset(Scsi_Cmnd *, unsigned int);
int atp870u_biosparam(Disk *, kdev_t, int *);
int atp870u_release(struct Scsi_Host *);
void send_s870(unsigned char);

#define qcnt		32
#define ATP870U_SCATTER 128
#define ATP870U_CMDLUN 1

#ifndef NULL
#define NULL 0
#endif

extern const char *atp870u_info(struct Scsi_Host *);

extern int atp870u_proc_info(char *, char **, off_t, int, int, int);

#define ATP870U {						\
	next: NULL,						\
	module: NULL,						\
	proc_info: atp870u_proc_info,				\
	name: NULL,						\
	detect: atp870u_detect, 				\
	release: atp870u_release,				\
	info: atp870u_info,					\
	command: atp870u_command,				\
	queuecommand: atp870u_queuecommand,			\
	eh_strategy_handler: NULL,				\
	eh_abort_handler: NULL, 				\
	eh_device_reset_handler: NULL,				\
	eh_bus_reset_handler: NULL,				\
	eh_host_reset_handler: NULL,				\
	abort: atp870u_abort,					\
	reset: atp870u_reset,					\
	slave_attach: NULL,					\
	bios_param: atp870u_biosparam,				\
	can_queue: qcnt,	 /* max simultaneous cmds      */\
	this_id: 7,	       /* scsi id of host adapter    */\
	sg_tablesize: ATP870U_SCATTER,	/* max scatter-gather cmds    */\
	cmd_per_lun: ATP870U_CMDLUN,	/* cmds per lun (linked cmds) */\
	present: 0,		/* number of 7xxx's present   */\
	unchecked_isa_dma: 0,	/* no memory DMA restrictions */\
	use_clustering: ENABLE_CLUSTERING,			\
	use_new_eh_code: 0					\
}

#endif
