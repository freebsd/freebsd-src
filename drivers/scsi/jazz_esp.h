/* jazz_esp.h: Defines and structures for the JAZZ SCSI driver.
 *
 * Copyright (C) 1997 Thomas Bogendoerfer (tsbogend@alpha.franken.de)
 */

#ifndef JAZZ_ESP_H
#define JAZZ_ESP_H

#define EREGS_PAD(n)

#include "NCR53C9x.h"


extern int jazz_esp_detect(struct SHT *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			 int hostno, int inout);

#define SCSI_JAZZ_ESP {                                         \
		proc_name:      "esp",				\
		proc_info:      &esp_proc_info,			\
		name:           "ESP 100/100a/200",		\
		detect:         jazz_esp_detect,		\
		info:           esp_info,			\
		command:        esp_command,			\
		queuecommand:   esp_queue,			\
		abort:          esp_abort,			\
		reset:          esp_reset,			\
		can_queue:      7,				\
		this_id:        7,				\
		sg_tablesize:   SG_ALL,				\
		cmd_per_lun:    1,				\
		use_clustering: DISABLE_CLUSTERING, }

#endif /* JAZZ_ESP_H */
