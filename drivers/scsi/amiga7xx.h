#ifndef AMIGA7XX_H

#include <linux/types.h>

int amiga7xx_detect(Scsi_Host_Template *);
const char *NCR53c7x0_info(void);
int NCR53c7xx_queue_command(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int NCR53c7xx_abort(Scsi_Cmnd *);
int NCR53c7x0_release (struct Scsi_Host *);
int NCR53c7xx_reset(Scsi_Cmnd *, unsigned int);
void NCR53c7x0_intr(int irq, void *dev_id, struct pt_regs * regs);

#ifndef NULL
#define NULL 0
#endif

#ifndef CMD_PER_LUN
#define CMD_PER_LUN 3
#endif

#ifndef CAN_QUEUE
#define CAN_QUEUE 24
#endif

#include <scsi/scsicam.h>

#define AMIGA7XX_SCSI {name:                "Amiga NCR53c710 SCSI", \
		       detect:              amiga7xx_detect,    \
		       queuecommand:        NCR53c7xx_queue_command, \
		       abort:               NCR53c7xx_abort,   \
		       reset:               NCR53c7xx_reset,   \
		       bios_param:          scsicam_bios_param,   \
		       can_queue:           24,       \
		       this_id:             7,               \
		       sg_tablesize:        63,          \
		       cmd_per_lun:	    3,     \
		       use_clustering:      DISABLE_CLUSTERING }

#endif /* AMIGA7XX_H */
