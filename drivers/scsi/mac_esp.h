
/*
mac_esp.h

copyright 1997 David Weis, weisd3458@uni.edu
*/


#include "NCR53C9x.h"

#ifndef MAC_ESP_H
#define MAC_ESP_H

/* #define DEBUG_MAC_ESP */

extern int mac_esp_detect(struct SHT *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);


#define SCSI_MAC_ESP      { proc_name:		"esp", \
			    name:		"Mac 53C9x SCSI", \
			    detect:		mac_esp_detect, \
			    release:		NULL, \
			    info:		esp_info, \
			    /* command:		esp_command, */ \
			    queuecommand:	esp_queue, \
			    abort:		esp_abort, \
			    reset:		esp_reset, \
			    can_queue:          7, \
			    this_id:		7, \
			    sg_tablesize:	SG_ALL, \
			    cmd_per_lun:	1, \
			    use_clustering:	DISABLE_CLUSTERING, \
			    use_new_eh_code:	0 }

#endif /* MAC_ESP_H */

