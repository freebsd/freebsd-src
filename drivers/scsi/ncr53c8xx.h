/******************************************************************************
**  Device driver for the PCI-SCSI NCR538XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  This driver has been ported to Linux from the FreeBSD NCR53C8XX driver
**  and is currently maintained by
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**  And has been ported to NetBSD by
**          Charles M. Hannum           <mycroft@gnu.ai.mit.edu>
**
*******************************************************************************
*/

#ifndef NCR53C8XX_H
#define NCR53C8XX_H

#include "sym53c8xx_defs.h"

/*
**	Define Scsi_Host_Template parameters
**
**	Used by hosts.c and ncr53c8xx.c with module configuration.
*/

#if (LINUX_VERSION_CODE >= 0x020400) || defined(HOSTS_C) || defined(MODULE)

#include <scsi/scsicam.h>

int ncr53c8xx_abort(Scsi_Cmnd *);
int ncr53c8xx_detect(Scsi_Host_Template *tpnt);
const char *ncr53c8xx_info(struct Scsi_Host *host);
int ncr53c8xx_queue_command(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int ncr53c8xx_reset(Scsi_Cmnd *, unsigned int);

#ifdef MODULE
int ncr53c8xx_release(struct Scsi_Host *);
#else
#define ncr53c8xx_release NULL
#endif


#if	LINUX_VERSION_CODE >= LinuxVersionCode(2,1,75)

#define NCR53C8XX {     name:           "",			\
			detect:         ncr53c8xx_detect,	\
			release:        ncr53c8xx_release,	\
			info:           ncr53c8xx_info, 	\
			queuecommand:   ncr53c8xx_queue_command,\
			abort:          ncr53c8xx_abort,	\
			reset:          ncr53c8xx_reset,	\
			bios_param:     scsicam_bios_param,	\
			can_queue:      SCSI_NCR_CAN_QUEUE,	\
			this_id:        7,			\
			sg_tablesize:   SCSI_NCR_SG_TABLESIZE,	\
			cmd_per_lun:    SCSI_NCR_CMD_PER_LUN,	\
			use_clustering: DISABLE_CLUSTERING} 

#else

#define NCR53C8XX {	NULL, NULL, NULL, NULL,				\
			NULL,			ncr53c8xx_detect,	\
			ncr53c8xx_release,	ncr53c8xx_info,	NULL,	\
			ncr53c8xx_queue_command,ncr53c8xx_abort,	\
			ncr53c8xx_reset, NULL,	scsicam_bios_param,	\
			SCSI_NCR_CAN_QUEUE,	7,			\
			SCSI_NCR_SG_TABLESIZE,	SCSI_NCR_CMD_PER_LUN,	\
			0,	0,	DISABLE_CLUSTERING} 
 
#endif /* LINUX_VERSION_CODE */

#endif /* defined(HOSTS_C) || defined(MODULE) */ 

#endif /* NCR53C8XX_H */
