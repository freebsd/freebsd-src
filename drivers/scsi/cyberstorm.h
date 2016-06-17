/* cyberstorm.h: Defines and structures for the CyberStorm SCSI driver.
 *
 * Copyright (C) 1996 Jesper Skov (jskov@cygnus.co.uk)
 */

#include "NCR53C9x.h"

#ifndef CYBER_ESP_H
#define CYBER_ESP_H

/* The controller registers can be found in the Z2 config area at these
 * offsets:
 */
#define CYBER_ESP_ADDR 0xf400
#define CYBER_DMA_ADDR 0xf800


/* The CyberStorm DMA interface */
struct cyber_dma_registers {
	volatile unsigned char dma_addr0;	/* DMA address (MSB) [0x000] */
	unsigned char dmapad1[1];
	volatile unsigned char dma_addr1;	/* DMA address       [0x002] */
	unsigned char dmapad2[1];
	volatile unsigned char dma_addr2;	/* DMA address       [0x004] */
	unsigned char dmapad3[1];
	volatile unsigned char dma_addr3;	/* DMA address (LSB) [0x006] */
	unsigned char dmapad4[0x3fb];
	volatile unsigned char cond_reg;        /* DMA cond    (ro)  [0x402] */
#define ctrl_reg  cond_reg			/* DMA control (wo)  [0x402] */
};

/* DMA control bits */
#define CYBER_DMA_LED    0x80	/* HD led control 1 = on */
#define CYBER_DMA_WRITE  0x40	/* DMA direction. 1 = write */
#define CYBER_DMA_Z3     0x20	/* 16 (Z2) or 32 (CHIP/Z3) bit DMA transfer */

/* DMA status bits */
#define CYBER_DMA_HNDL_INTR 0x80	/* DMA IRQ pending? */

/* The bits below appears to be Phase5 Debug bits only; they were not
 * described by Phase5 so using them may seem a bit stupid...
 */
#define CYBER_HOST_ID 0x02	/* If set, host ID should be 7, otherwise
				 * it should be 6.
				 */
#define CYBER_SLOW_CABLE 0x08	/* If *not* set, assume SLOW_CABLE */

extern int cyber_esp_detect(struct SHT *);
extern int cyber_esp_release(struct Scsi_Host *);
extern const char *esp_info(struct Scsi_Host *);
extern int esp_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
extern int esp_command(Scsi_Cmnd *);
extern int esp_abort(Scsi_Cmnd *);
extern int esp_reset(Scsi_Cmnd *, unsigned int);
extern int esp_proc_info(char *buffer, char **start, off_t offset, int length,
			 int hostno, int inout);


#define SCSI_CYBERSTORM   { proc_name:		"esp-cyberstorm", \
			    proc_info:		esp_proc_info, \
			    name:		"CyberStorm SCSI", \
			    detect:		cyber_esp_detect, \
			    release:		cyber_esp_release, \
			    queuecommand:	esp_queue, \
			    abort:		esp_abort, \
			    reset:		esp_reset, \
			    can_queue:          7, \
			    this_id:		7, \
			    sg_tablesize:	SG_ALL, \
			    cmd_per_lun:	1, \
			    use_clustering:	ENABLE_CLUSTERING }

#endif /* CYBER_ESP_H */
