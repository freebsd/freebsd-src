/*
 *        eata.h - used by the low-level driver for EATA/DMA SCSI host adapters.
 */
#ifndef _EATA_H
#define _EATA_H

#include <scsi/scsicam.h>

int eata2x_detect(Scsi_Host_Template *);
int eata2x_release(struct Scsi_Host *);
int eata2x_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int eata2x_abort(Scsi_Cmnd *);
int eata2x_reset(Scsi_Cmnd *);
int eata2x_biosparam(Disk *, kdev_t, int *);

#define EATA_VERSION "6.70.00"

#define EATA {                                                               \
                name:              "EATA/DMA 2.0x rev. " EATA_VERSION " ",   \
                detect:                  eata2x_detect,                      \
                release:                 eata2x_release,                     \
                queuecommand:            eata2x_queuecommand,                \
                abort:                   NULL,                               \
                reset:                   NULL,                               \
                eh_abort_handler:        eata2x_abort,                       \
                eh_device_reset_handler: NULL,                               \
                eh_bus_reset_handler:    NULL,                               \
                eh_host_reset_handler:   eata2x_reset,                       \
                bios_param:              eata2x_biosparam,                   \
                this_id:                 7,                                  \
                unchecked_isa_dma:       1,                                  \
                use_clustering:          ENABLE_CLUSTERING,                  \
                use_new_eh_code:         1    /* Enable new error code */    \
             }

#endif
