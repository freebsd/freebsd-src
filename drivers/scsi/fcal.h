/* fcal.h: Generic Fibre Channel Arbitrated Loop SCSI host adapter driver definitions.
 *
 * Copyright (C) 1998,1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

#ifndef _FCAL_H
#define _FCAL_H

#include "../fc4/fcp_impl.h"

struct fcal {
	/* fc must be first */
	fc_channel		*fc;
	unsigned char		map[128];
	fc_wwn			nport_wwn[128];
	fc_wwn			node_wwn[128];
};

/* Arbitrary constant. Cannot be too large, as fc4 layer has limitations
   for a particular channel */
#define FCAL_CAN_QUEUE		512

int fcal_detect(Scsi_Host_Template *);
int fcal_release(struct Scsi_Host *);
int fcal_proc_info (char *, char **, off_t, int, int, int);

#define FCAL {							\
	name:			"Fibre Channel Arbitrated Loop",\
	detect:			fcal_detect,			\
	release:		fcal_release,			\
	proc_info:		fcal_proc_info,			\
	queuecommand:		fcp_scsi_queuecommand,		\
	can_queue:		FCAL_CAN_QUEUE,			\
	this_id:		-1,				\
	sg_tablesize:		1,				\
	cmd_per_lun:		1,				\
	use_clustering:		ENABLE_CLUSTERING,		\
	use_new_eh_code:	FCP_SCSI_USE_NEW_EH_CODE,	\
	abort:			fcp_old_abort,			\
	eh_abort_handler:	fcp_scsi_abort,			\
	eh_device_reset_handler:fcp_scsi_dev_reset,		\
	eh_bus_reset_handler:	fcp_scsi_bus_reset,		\
	eh_host_reset_handler:	fcp_scsi_host_reset,		\
}	

#endif /* !(_FCAL_H) */
