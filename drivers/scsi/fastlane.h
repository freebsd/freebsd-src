/* fastlane.h: Defines and structures for the Fastlane SCSI driver.
 *
 * Copyright (C) 1996 Jesper Skov (jskov@cygnus.co.uk)
 */

#include "NCR53C9x.h"

#ifndef FASTLANE_H
#define FASTLANE_H

/* The controller registers can be found in the Z2 config area at these
 * offsets:
 */
#define FASTLANE_ESP_ADDR 0x1000001
#define FASTLANE_DMA_ADDR 0x1000041


/* The Fastlane DMA interface */
struct fastlane_dma_registers {
	volatile unsigned char cond_reg;	/* DMA status  (ro) [0x0000] */
#define ctrl_reg  cond_reg			/* DMA control (wo) [0x0000] */
	unsigned char dmapad1[0x3f];
	volatile unsigned char clear_strobe;    /* DMA clear   (wo) [0x0040] */
};


/* DMA status bits */
#define FASTLANE_DMA_MINT  0x80
#define FASTLANE_DMA_IACT  0x40
#define FASTLANE_DMA_CREQ  0x20

/* DMA control bits */
#define FASTLANE_DMA_FCODE 0xa0
#define FASTLANE_DMA_MASK  0xf3
#define FASTLANE_DMA_LED   0x10	/* HD led control 1 = on */
#define FASTLANE_DMA_WRITE 0x08 /* 1 = write */
#define FASTLANE_DMA_ENABLE 0x04 /* Enable DMA */
#define FASTLANE_DMA_EDI   0x02	/* Enable DMA IRQ ? */
#define FASTLANE_DMA_ESI   0x01	/* Enable SCSI IRQ */

extern int fastlane_esp_detect(struct SHT *);
extern int fastlane_esp_release(struct Scsi_Host *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			 int hostno, int inout);

#define SCSI_FASTLANE     { proc_name:		"esp-fastlane", \
			    proc_info:		esp_proc_info, \
			    name:		"Fastlane SCSI", \
			    detect:		fastlane_esp_detect, \
			    release:		fastlane_esp_release, \
			    queuecommand:	esp_queue, \
			    abort:		esp_abort, \
			    reset:		esp_reset, \
			    can_queue:          7, \
			    this_id:		7, \
			    sg_tablesize:	SG_ALL, \
			    cmd_per_lun:	1, \
			    use_clustering:	ENABLE_CLUSTERING }

#endif /* FASTLANE_H */
