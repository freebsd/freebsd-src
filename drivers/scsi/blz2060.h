/* blz2060.h: Defines and structures for the Blizzard 2060 SCSI driver.
 *
 * Copyright (C) 1996 Jesper Skov (jskov@cygnus.co.uk)
 *
 * This file is based on cyber_esp.h (hence the occasional reference to
 * CyberStorm).
 */

#include "NCR53C9x.h"

#ifndef BLZ2060_H
#define BLZ2060_H

/* The controller registers can be found in the Z2 config area at these
 * offsets:
 */
#define BLZ2060_ESP_ADDR 0x1ff00
#define BLZ2060_DMA_ADDR 0x1ffe0


/* The Blizzard 2060 DMA interface
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Only two things can be programmed in the Blizzard DMA:
 *  1) The data direction is controlled by the status of bit 31 (1 = write)
 *  2) The source/dest address (word aligned, shifted one right) in bits 30-0
 *
 * Figure out interrupt status by reading the ESP status byte.
 */
struct blz2060_dma_registers {
	volatile unsigned char dma_led_ctrl;	/* DMA led control   [0x000] */
	unsigned char dmapad1[0x0f];
	volatile unsigned char dma_addr0; 	/* DMA address (MSB) [0x010] */
	unsigned char dmapad2[0x03];
	volatile unsigned char dma_addr1; 	/* DMA address       [0x014] */
	unsigned char dmapad3[0x03];
	volatile unsigned char dma_addr2; 	/* DMA address       [0x018] */
	unsigned char dmapad4[0x03];
	volatile unsigned char dma_addr3; 	/* DMA address (LSB) [0x01c] */
};

#define BLZ2060_DMA_WRITE 0x80000000

/* DMA control bits */
#define BLZ2060_DMA_LED    0x02		/* HD led control 1 = off */

extern int blz2060_esp_detect(struct SHT *);
extern int blz2060_esp_release(struct Scsi_Host *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			 int hostno, int inout);

#define SCSI_BLZ2060      { proc_name:		"esp-blz2060", \
			    proc_info:		esp_proc_info, \
			    name:		"Blizzard2060 SCSI", \
			    detect:		blz2060_esp_detect, \
			    release:		blz2060_esp_release, \
			    queuecommand:	esp_queue, \
			    abort:		esp_abort, \
			    reset:		esp_reset, \
			    can_queue:          7, \
			    this_id:		7, \
			    sg_tablesize:	SG_ALL, \
			    cmd_per_lun:	1, \
			    use_clustering:	ENABLE_CLUSTERING }

#endif /* BLZ2060_H */
