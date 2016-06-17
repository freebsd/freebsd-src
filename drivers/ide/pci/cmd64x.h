#ifndef CMD64X_H
#define CMD64X_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_CMD64X_TIMINGS

#define CMD_DEBUG 0

#if CMD_DEBUG
#define cmdprintk(x...)	printk(x)
#else
#define cmdprintk(x...)
#endif

#ifndef SPLIT_BYTE
#define SPLIT_BYTE(B,H,L)	((H)=(B>>4), (L)=(B-((B>>4)<<4)))
#endif

/*
 * CMD64x specific registers definition.
 */
#define CFR		0x50
#define   CFR_INTR_CH0		0x02
#define CNTRL		0x51
#define	  CNTRL_DIS_RA0		0x40
#define   CNTRL_DIS_RA1		0x80
#define	  CNTRL_ENA_2ND		0x08

#define	CMDTIM		0x52
#define	ARTTIM0		0x53
#define	DRWTIM0		0x54
#define ARTTIM1 	0x55
#define DRWTIM1		0x56
#define ARTTIM23	0x57
#define   ARTTIM23_DIS_RA2	0x04
#define   ARTTIM23_DIS_RA3	0x08
#define   ARTTIM23_INTR_CH1	0x10
#define ARTTIM2		0x57
#define ARTTIM3		0x57
#define DRWTIM23	0x58
#define DRWTIM2		0x58
#define BRST		0x59
#define DRWTIM3		0x5b

#define BMIDECR0	0x70
#define MRDMODE		0x71
#define   MRDMODE_INTR_CH0	0x04
#define   MRDMODE_INTR_CH1	0x08
#define   MRDMODE_BLK_CH0	0x10
#define   MRDMODE_BLK_CH1	0x20
#define BMIDESR0	0x72
#define UDIDETCR0	0x73
#define DTPR0		0x74
#define BMIDECR1	0x78
#define BMIDECSR	0x79
#define BMIDESR1	0x7A
#define UDIDETCR1	0x7B
#define DTPR1		0x7C

#if defined(DISPLAY_CMD64X_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 cmd64x_proc;

static char * print_cmd64x_get_info(char *, struct pci_dev *, int);
static int cmd64x_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t cmd64x_procs[] __initdata = {
	{
		.name		= "cmd64x",
		.set		= 1,
		.get_info	= cmd64x_get_info,
		.parent		= NULL,
	},
};
#endif  /* defined(DISPLAY_CMD64X_TIMINGS) && defined(CONFIG_PROC_FS) */

static unsigned int init_chipset_cmd64x(struct pci_dev *, const char *);
static void init_hwif_cmd64x(ide_hwif_t *);
static void init_dma_cmd64x(ide_hwif_t *, unsigned long);

static ide_pci_device_t cmd64x_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_CMD_643,
		.name		= "CMD643",
		.init_chipset	= init_chipset_cmd64x,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_cmd64x,
		.init_dma	= init_dma_cmd64x,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_CMD_646,
		.name		= "CMD646",
		.init_chipset	= init_chipset_cmd64x,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_cmd64x,
		.init_dma	= init_dma_cmd64x,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x51,0x80,0x80}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_CMD,
		.device	= PCI_DEVICE_ID_CMD_648,
		.name		= "CMD648",
		.init_chipset	= init_chipset_cmd64x,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_cmd64x,
		.init_dma	= init_dma_cmd64x,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= PCI_VENDOR_ID_CMD,
		.device		= PCI_DEVICE_ID_CMD_649,
		.name		= "CMD649",
		.init_chipset	= init_chipset_cmd64x,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_cmd64x,
		.init_dma	= init_dma_cmd64x,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= ON_BOARD,
		.extra		= 0,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 2,
		.bootable	= EOL,
	}
};

#endif /* CMD64X_H */
