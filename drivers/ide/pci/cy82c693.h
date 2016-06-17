#ifndef CY82C693_H
#define CY82C693_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

/* the current version */
#define CY82_VERSION	"CY82C693U driver v0.34 99-13-12 Andreas S. Krebs (akrebs@altavista.net)"

/*
 *	The following are used to debug the driver.
 */
#define	CY82C693_DEBUG_LOGS	0
#define	CY82C693_DEBUG_INFO	0

/* define CY82C693_SETDMA_CLOCK to set DMA Controller Clock Speed to ATCLK */
#undef CY82C693_SETDMA_CLOCK

/*
 *	NOTE: the value for busmaster timeout is tricky and I got it by
 *	 trial and error!  By using a to low value will cause DMA timeouts
 *	 and drop IDE performance, and by using a to high value will cause
 *	 audio playback to scatter.
 *	 If you know a better value or how to calc it, please let me know.
 */

/* twice the value written in cy82c693ub datasheet */
#define BUSMASTER_TIMEOUT	0x50
/*
 * the value above was tested on my machine and it seems to work okay
 */

/* here are the offset definitions for the registers */
#define CY82_IDE_CMDREG		0x04
#define CY82_IDE_ADDRSETUP	0x48
#define CY82_IDE_MASTER_IOR	0x4C	
#define CY82_IDE_MASTER_IOW	0x4D	
#define CY82_IDE_SLAVE_IOR	0x4E	
#define CY82_IDE_SLAVE_IOW	0x4F
#define CY82_IDE_MASTER_8BIT	0x50	
#define CY82_IDE_SLAVE_8BIT	0x51	

#define CY82_INDEX_PORT		0x22
#define CY82_DATA_PORT		0x23

#define CY82_INDEX_CTRLREG1	0x01
#define CY82_INDEX_CHANNEL0	0x30
#define CY82_INDEX_CHANNEL1	0x31
#define CY82_INDEX_TIMEOUT	0x32

/* the max PIO mode - from datasheet */
#define CY82C693_MAX_PIO	4

/* the min and max PCI bus speed in MHz - from datasheet */
#define CY82C963_MIN_BUS_SPEED	25
#define CY82C963_MAX_BUS_SPEED	33

/* the struct for the PIO mode timings */
typedef struct pio_clocks_s {
        u8	address_time;	/* Address setup (clocks) */
	u8	time_16r;	/* clocks for 16bit IOR (0xF0=Active/data, 0x0F=Recovery) */
	u8	time_16w;	/* clocks for 16bit IOW (0xF0=Active/data, 0x0F=Recovery) */
	u8	time_8;		/* clocks for 8bit (0xF0=Active/data, 0x0F=Recovery) */
} pio_clocks_t;

static unsigned int init_chipset_cy82c693(struct pci_dev *, const char *);
static void init_hwif_cy82c693(ide_hwif_t *);
static void init_iops_cy82c693(ide_hwif_t *);

static ide_pci_device_t cy82c693_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_CONTAQ,
		.device		= PCI_DEVICE_ID_CONTAQ_82C693,
		.name		= "CY82C693",
		.init_chipset	= init_chipset_cy82c693,
		.init_iops	= init_iops_cy82c693,
		.init_hwif	= init_hwif_cy82c693,
		.init_dma	= NULL,
		.channels	= 1,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* CY82C693_H */
