#ifndef _I2O_SCSI_H
#define _I2O_SCSI_H

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif

#define LinuxVersionCode(v, p, s) (((v)<<16)+((p)<<8)+(s))

#include <linux/types.h>
#include <linux/kdev_t.h>

#define I2O_SCSI_ID 15
#define I2O_SCSI_CAN_QUEUE 4
#define I2O_SCSI_CMD_PER_LUN 6

static int i2o_scsi_detect(Scsi_Host_Template *);
static const char *i2o_scsi_info(struct Scsi_Host *);
static int i2o_scsi_command(Scsi_Cmnd *);
static int i2o_scsi_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
static int i2o_scsi_abort(Scsi_Cmnd *);
static int i2o_scsi_reset(Scsi_Cmnd *, unsigned int);
static int i2o_scsi_bios_param(Disk *, kdev_t, int *);
extern void i2o_scsi_setup(char *str, int *ints);
static int i2o_scsi_release(struct Scsi_Host *host);

#define I2OSCSI {                                          \
		  next: NULL,				    \
                  proc_name:         "i2o_scsi",   \
                  name:              "I2O SCSI Layer", 	    \
                  detect:            i2o_scsi_detect,       \
                  release:	     i2o_scsi_release,	    \
                  info:              i2o_scsi_info,         \
                  command:           i2o_scsi_command,      \
                  queuecommand:      i2o_scsi_queuecommand, \
                  abort:             i2o_scsi_abort,        \
                  reset:             i2o_scsi_reset,        \
                  bios_param:        i2o_scsi_bios_param,   \
                  can_queue:         I2O_SCSI_CAN_QUEUE,    \
                  this_id:           I2O_SCSI_ID,           \
                  sg_tablesize:      8,                     \
                  cmd_per_lun:       I2O_SCSI_CMD_PER_LUN,  \
                  unchecked_isa_dma: 0,                     \
                  use_clustering:    ENABLE_CLUSTERING     \
                  }

#endif
