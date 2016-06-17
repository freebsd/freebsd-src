/* oktagon_esp.h: Defines and structures for the CyberStorm SCSI Mk II driver.
 *
 * Copyright (C) 1996 Jesper Skov (jskov@cs.auc.dk)
 */

#include "NCR53C9x.h"

#ifndef OKTAGON_ESP_H
#define OKTAGON_ESP_H

/* The controller registers can be found in the Z2 config area at these
 * offsets:
 */
#define OKTAGON_ESP_ADDR 0x03000
#define OKTAGON_DMA_ADDR 0x01000


/* The CyberStorm II DMA interface */
struct oktagon_dma_registers {
	volatile unsigned char cond_reg;        /* DMA cond    (ro)  [0x000] */
#define ctrl_reg  cond_reg			/* DMA control (wo)  [0x000] */
	unsigned char dmapad4[0x3f];
	volatile unsigned char dma_addr0;	/* DMA address (MSB) [0x040] */
	unsigned char dmapad1[3];
	volatile unsigned char dma_addr1;	/* DMA address       [0x044] */
	unsigned char dmapad2[3];
	volatile unsigned char dma_addr2;	/* DMA address       [0x048] */
	unsigned char dmapad3[3];
	volatile unsigned char dma_addr3;	/* DMA address (LSB) [0x04c] */
};

extern int oktagon_esp_detect(struct SHT *);
extern int oktagon_esp_release(struct Scsi_Host *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			int hostno, int inout);

#define SCSI_OKTAGON_ESP {                       \
   proc_name:           "esp-oktagon",           \
   proc_info:           &esp_proc_info,          \
   name:                "BSC Oktagon SCSI",      \
   detect:              oktagon_esp_detect,      \
   release:             oktagon_esp_release,     \
   queuecommand:        esp_queue,               \
   abort:               esp_abort,               \
   reset:               esp_reset,               \
   can_queue:           7,                       \
   this_id:             7,                       \
   sg_tablesize:        SG_ALL,                  \
   cmd_per_lun:         1,                       \
   use_clustering:      ENABLE_CLUSTERING }

#endif /* OKTAGON_ESP_H */
