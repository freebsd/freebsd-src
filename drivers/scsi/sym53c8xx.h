/******************************************************************************
**  High Performance device driver for the Symbios 53C896 controller.
**
**  Copyright (C) 1998-2001  Gerard Roudier <groudier@free.fr>
**
**  This driver also supports all the Symbios 53C8XX controller family, 
**  except 53C810 revisions < 16, 53C825 revisions < 16 and all 
**  revisions of 53C815 controllers.
**
**  This driver is based on the Linux port of the FreeBSD ncr driver.
** 
**  Copyright (C) 1994  Wolfgang Stanglmeier
**  
**-----------------------------------------------------------------------------
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
**  The Linux port of the FreeBSD ncr driver has been achieved in 
**  november 1995 by:
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
**-----------------------------------------------------------------------------
**
**  Major contributions:
**  --------------------
**
**  NVRAM detection and reading.
**    Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
**
*******************************************************************************
*/

#ifndef SYM53C8XX_H
#define SYM53C8XX_H

#include "sym53c8xx_defs.h"

/*
**	Define Scsi_Host_Template parameters
**
**	Used by hosts.c and sym53c8xx.c with module configuration.
*/

#if (LINUX_VERSION_CODE >= 0x020400) || defined(HOSTS_C) || defined(MODULE)

#include <scsi/scsicam.h>

int sym53c8xx_abort(Scsi_Cmnd *);
int sym53c8xx_detect(Scsi_Host_Template *tpnt);
const char *sym53c8xx_info(struct Scsi_Host *host);
int sym53c8xx_queue_command(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int sym53c8xx_reset(Scsi_Cmnd *, unsigned int);

#ifdef MODULE
int sym53c8xx_release(struct Scsi_Host *);
#else
#define sym53c8xx_release NULL
#endif


#if	LINUX_VERSION_CODE >= LinuxVersionCode(2,1,75)

#define SYM53C8XX {     name:           "",			\
			detect:         sym53c8xx_detect,	\
			release:        sym53c8xx_release,	\
			info:           sym53c8xx_info, 	\
			queuecommand:   sym53c8xx_queue_command,\
			abort:          sym53c8xx_abort,	\
			reset:          sym53c8xx_reset,	\
			bios_param:     scsicam_bios_param,	\
			can_queue:      SCSI_NCR_CAN_QUEUE,	\
			this_id:        7,			\
			sg_tablesize:   SCSI_NCR_SG_TABLESIZE,	\
			cmd_per_lun:    SCSI_NCR_CMD_PER_LUN,	\
			max_sectors:    MAX_SEGMENTS*8,		\
			use_clustering: DISABLE_CLUSTERING,	\
			highmem_io:	1}

#else

#define SYM53C8XX {	NULL, NULL, NULL, NULL,				\
			NULL,			sym53c8xx_detect,	\
			sym53c8xx_release,	sym53c8xx_info,	NULL,	\
			sym53c8xx_queue_command,sym53c8xx_abort,	\
			sym53c8xx_reset, NULL,	scsicam_bios_param,	\
			SCSI_NCR_CAN_QUEUE,	7,			\
			SCSI_NCR_SG_TABLESIZE,	SCSI_NCR_CMD_PER_LUN,	\
			0,	0,	DISABLE_CLUSTERING} 
 
#endif /* LINUX_VERSION_CODE */

#endif /* defined(HOSTS_C) || defined(MODULE) */ 

#endif /* SYM53C8XX_H */
