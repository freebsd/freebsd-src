/* pluto.h: SparcSTORAGE Array SCSI host adapter driver definitions.
 *
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#ifndef _PLUTO_H
#define _PLUTO_H

#include "../fc4/fcp_impl.h"

struct pluto {
	/* This must be first */
	fc_channel	*fc;
	char		rev_str[5];
	char		fw_rev_str[5];
	char		serial_str[13];
};

struct pluto_inquiry {
	u8	dtype;
	u8	removable:1, qualifier:7;
	u8	iso:2, ecma:3, ansi:3;
	u8	aenc:1, trmiop:1, :2, rdf:4;
	u8	len;
	u8	xxx1;
	u8	xxx2;
	u8	reladdr:1, wbus32:1, wbus16:1, sync:1, linked:1, :1, cmdque:1, softreset:1;
	u8	vendor_id[8];
	u8	product_id[16];
	u8	revision[4];
	u8	fw_revision[4];
	u8	serial[12];
	u8	xxx3[2];
	u8	channels;
	u8	targets;
};

/* This is the max number of outstanding SCSI commands per pluto */
#define PLUTO_CAN_QUEUE		254

int pluto_detect(Scsi_Host_Template *);
int pluto_release(struct Scsi_Host *);
const char * pluto_info(struct Scsi_Host *);

#define PLUTO {							\
	name:			"Sparc Storage Array 100/200",	\
	detect:			pluto_detect,			\
	release:		pluto_release,			\
	info:			pluto_info,			\
	queuecommand:		fcp_scsi_queuecommand,		\
	can_queue:		PLUTO_CAN_QUEUE,		\
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

#endif /* !(_PLUTO_H) */

