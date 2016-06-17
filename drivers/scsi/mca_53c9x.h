/* mca_53c94.h: Defines and structures for the SCSI adapter found on NCR 35xx
 *  (and maybe some other) Microchannel machines.
 *
 * Code taken mostly from Cyberstorm SCSI drivers
 *   Copyright (C) 1996 Jesper Skov (jskov@cygnus.co.uk)
 *
 * Hacked to work with the NCR MCA stuff by Tymm Twillman (tymm@computer.org)
 *   1998
 */

#include "NCR53C9x.h"

#ifndef MCA_53C9X_H
#define MCA_53C9X_H

/*
 * From ibmmca.c (IBM scsi controller card driver) -- used for turning PS2 disk
 *  activity LED on and off
 */

#define PS2_SYS_CTR	0x92

extern int mca_esp_detect(struct SHT *);
extern int mca_esp_release(struct Scsi_Host *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			 int hostno, int inout);


#define MCA_53C9X         { proc_name:		"esp", \
			    name:		"NCR 53c9x SCSI", \
			    detect:		mca_esp_detect, \
			    release:		mca_esp_release, \
			    queuecommand:	esp_queue, \
			    abort:		esp_abort, \
			    reset:		esp_reset, \
			    can_queue:          7, \
			    sg_tablesize:	SG_ALL, \
			    cmd_per_lun:	1, \
                            unchecked_isa_dma:  1, \
			    use_clustering:	DISABLE_CLUSTERING }

/* Ports the ncr's 53c94 can be put at; indexed by pos register value */

#define MCA_53C9X_IO_PORTS {                             \
                         0x0000, 0x0240, 0x0340, 0x0400, \
	                 0x0420, 0x3240, 0x8240, 0xA240, \
	                }
			
/*
 * Supposedly there were some cards put together with the 'c9x and 86c01.  If
 *   they have different ID's from the ones on the 3500 series machines, 
 *   you can add them here and hopefully things will work out.
 */
			
#define MCA_53C9X_IDS {          \
                         0x7F4C, \
			 0x0000, \
                        }

#endif /* MCA_53C9X_H */

