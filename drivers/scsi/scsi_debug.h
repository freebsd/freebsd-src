#ifndef _SCSI_DEBUG_H

#include <linux/types.h>
#include <linux/kdev_t.h>

static int scsi_debug_detect(Scsi_Host_Template *);
static int scsi_debug_release(struct Scsi_Host *);
/* static int scsi_debug_command(Scsi_Cmnd *); */
static int scsi_debug_queuecommand(Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
static int scsi_debug_ioctl(Scsi_Device *, int, void *);
static int scsi_debug_biosparam(Disk *, kdev_t, int[]);
static int scsi_debug_abort(Scsi_Cmnd *);
static int scsi_debug_bus_reset(Scsi_Cmnd *);
static int scsi_debug_device_reset(Scsi_Cmnd *);
static int scsi_debug_host_reset(Scsi_Cmnd *);
static int scsi_debug_proc_info(char *, char **, off_t, int, int, int);
static const char * scsi_debug_info(struct Scsi_Host *);

/*
 * This driver is written for the lk 2.4 series
 */
#define SCSI_DEBUG_CANQUEUE  255 	/* needs to be >= 1 */

#define SCSI_DEBUG_MAX_CMD_LEN 16

#define SCSI_DEBUG_TEMPLATE \
		   {proc_info:         scsi_debug_proc_info,	\
		    name:              "SCSI DEBUG",		\
		    info:              scsi_debug_info,		\
		    detect:            scsi_debug_detect,	\
		    release:           scsi_debug_release,	\
		    ioctl:             scsi_debug_ioctl,	\
		    queuecommand:      scsi_debug_queuecommand, \
		    eh_abort_handler:  scsi_debug_abort,	\
		    eh_bus_reset_handler: scsi_debug_bus_reset,	\
		    eh_device_reset_handler: scsi_debug_device_reset,	\
		    eh_host_reset_handler: scsi_debug_host_reset,	\
		    bios_param:        scsi_debug_biosparam,	\
		    can_queue:         SCSI_DEBUG_CANQUEUE,	\
		    this_id:           7,			\
		    sg_tablesize:      64,			\
		    cmd_per_lun:       3,			\
		    unchecked_isa_dma: 0,			\
		    use_clustering:    ENABLE_CLUSTERING,	\
		    use_new_eh_code:   1,			\
}

#endif
