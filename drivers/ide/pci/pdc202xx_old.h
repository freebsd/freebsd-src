#ifndef PDC202XX_H
#define PDC202XX_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_PDC202XX_TIMINGS

#ifndef SPLIT_BYTE
#define SPLIT_BYTE(B,H,L)	((H)=(B>>4), (L)=(B-((B>>4)<<4)))
#endif

#define PDC202XX_DEBUG_DRIVE_INFO		0
#define PDC202XX_DECODE_REGISTER_INFO		0

const static char *pdc_quirk_drives[] = {
	"QUANTUM FIREBALLlct08 08",
	"QUANTUM FIREBALLP KA6.4",
	"QUANTUM FIREBALLP KA9.1",
	"QUANTUM FIREBALLP LM20.4",
	"QUANTUM FIREBALLP KX13.6",
	"QUANTUM FIREBALLP KX20.5",
	"QUANTUM FIREBALLP KX27.3",
	"QUANTUM FIREBALLP LM20.5",
	NULL
};

static inline u8 *pdc202xx_pio_verbose (u32 drive_pci)
{
	if ((drive_pci & 0x000ff000) == 0x000ff000) return("NOTSET");
	if ((drive_pci & 0x00000401) == 0x00000401) return("PIO 4");
	if ((drive_pci & 0x00000602) == 0x00000602) return("PIO 3");
	if ((drive_pci & 0x00000803) == 0x00000803) return("PIO 2");
	if ((drive_pci & 0x00000C05) == 0x00000C05) return("PIO 1");
	if ((drive_pci & 0x00001309) == 0x00001309) return("PIO 0");
	return("PIO ?");
}

static inline u8 *pdc202xx_dma_verbose (u32 drive_pci)
{
	if ((drive_pci & 0x00036000) == 0x00036000) return("MWDMA 2");
	if ((drive_pci & 0x00046000) == 0x00046000) return("MWDMA 1");
	if ((drive_pci & 0x00056000) == 0x00056000) return("MWDMA 0");
	if ((drive_pci & 0x00056000) == 0x00056000) return("SWDMA 2");
	if ((drive_pci & 0x00068000) == 0x00068000) return("SWDMA 1");
	if ((drive_pci & 0x000BC000) == 0x000BC000) return("SWDMA 0");
	return("PIO---");
}

static inline u8 *pdc202xx_ultra_verbose (u32 drive_pci, u16 slow_cable)
{
	if ((drive_pci & 0x000ff000) == 0x000ff000)
		return("NOTSET");
	if ((drive_pci & 0x00012000) == 0x00012000)
		return((slow_cable) ? "UDMA 2" : "UDMA 4");
	if ((drive_pci & 0x00024000) == 0x00024000)
		return((slow_cable) ? "UDMA 1" : "UDMA 3");
	if ((drive_pci & 0x00036000) == 0x00036000)
		return("UDMA 0");
	return(pdc202xx_dma_verbose(drive_pci));
}

/* A Register */
#define	SYNC_ERRDY_EN	0xC0

#define	SYNC_IN		0x80	/* control bit, different for master vs. slave drives */
#define	ERRDY_EN	0x40	/* control bit, different for master vs. slave drives */
#define	IORDY_EN	0x20	/* PIO: IOREADY */
#define	PREFETCH_EN	0x10	/* PIO: PREFETCH */

#define	PA3		0x08	/* PIO"A" timing */
#define	PA2		0x04	/* PIO"A" timing */
#define	PA1		0x02	/* PIO"A" timing */
#define	PA0		0x01	/* PIO"A" timing */

/* B Register */

#define	MB2		0x80	/* DMA"B" timing */
#define	MB1		0x40	/* DMA"B" timing */
#define	MB0		0x20	/* DMA"B" timing */

#define	PB4		0x10	/* PIO_FORCE 1:0 */

#define	PB3		0x08	/* PIO"B" timing */	/* PIO flow Control mode */
#define	PB2		0x04	/* PIO"B" timing */	/* PIO 4 */
#define	PB1		0x02	/* PIO"B" timing */	/* PIO 3 half */
#define	PB0		0x01	/* PIO"B" timing */	/* PIO 3 other half */

/* C Register */
#define	IORDYp_NO_SPEED	0x4F
#define	SPEED_DIS	0x0F

#define	DMARQp		0x80
#define	IORDYp		0x40
#define	DMAR_EN		0x20
#define	DMAW_EN		0x10

#define	MC3		0x08	/* DMA"C" timing */
#define	MC2		0x04	/* DMA"C" timing */
#define	MC1		0x02	/* DMA"C" timing */
#define	MC0		0x01	/* DMA"C" timing */

#if PDC202XX_DECODE_REGISTER_INFO

#define REG_A		0x01
#define REG_B		0x02
#define REG_C		0x04
#define REG_D		0x08

static void decode_registers (u8 registers, u8 value)
{
	u8	bit = 0, bit1 = 0, bit2 = 0;

	switch(registers) {
		case REG_A:
			bit2 = 0;
			printk("A Register ");
			if (value & 0x80) printk("SYNC_IN ");
			if (value & 0x40) printk("ERRDY_EN ");
			if (value & 0x20) printk("IORDY_EN ");
			if (value & 0x10) printk("PREFETCH_EN ");
			if (value & 0x08) { printk("PA3 ");bit2 |= 0x08; }
			if (value & 0x04) { printk("PA2 ");bit2 |= 0x04; }
			if (value & 0x02) { printk("PA1 ");bit2 |= 0x02; }
			if (value & 0x01) { printk("PA0 ");bit2 |= 0x01; }
			printk("PIO(A) = %d ", bit2);
			break;
		case REG_B:
			bit1 = 0;bit2 = 0;
			printk("B Register ");
			if (value & 0x80) { printk("MB2 ");bit1 |= 0x80; }
			if (value & 0x40) { printk("MB1 ");bit1 |= 0x40; }
			if (value & 0x20) { printk("MB0 ");bit1 |= 0x20; }
			printk("DMA(B) = %d ", bit1 >> 5);
			if (value & 0x10) printk("PIO_FORCED/PB4 ");
			if (value & 0x08) { printk("PB3 ");bit2 |= 0x08; }
			if (value & 0x04) { printk("PB2 ");bit2 |= 0x04; }
			if (value & 0x02) { printk("PB1 ");bit2 |= 0x02; }
			if (value & 0x01) { printk("PB0 ");bit2 |= 0x01; }
			printk("PIO(B) = %d ", bit2);
			break;
		case REG_C:
			bit2 = 0;
			printk("C Register ");
			if (value & 0x80) printk("DMARQp ");
			if (value & 0x40) printk("IORDYp ");
			if (value & 0x20) printk("DMAR_EN ");
			if (value & 0x10) printk("DMAW_EN ");

			if (value & 0x08) { printk("MC3 ");bit2 |= 0x08; }
			if (value & 0x04) { printk("MC2 ");bit2 |= 0x04; }
			if (value & 0x02) { printk("MC1 ");bit2 |= 0x02; }
			if (value & 0x01) { printk("MC0 ");bit2 |= 0x01; }
			printk("DMA(C) = %d ", bit2);
			break;
		case REG_D:
			printk("D Register ");
			break;
		default:
			return;
	}
	printk("\n        %s ", (registers & REG_D) ? "DP" :
				(registers & REG_C) ? "CP" :
				(registers & REG_B) ? "BP" :
				(registers & REG_A) ? "AP" : "ERROR");
	for (bit=128;bit>0;bit/=2)
		printk("%s", (value & bit) ? "1" : "0");
	printk("\n");
}

#endif /* PDC202XX_DECODE_REGISTER_INFO */

#define set_2regs(a, b)					\
	do {						\
		hwif->OUTB((a + adj), indexreg);	\
		hwif->OUTB(b, datareg);			\
	} while(0)

#define set_ultra(a, b, c)				\
	do {						\
		set_2regs(0x10,(a));			\
		set_2regs(0x11,(b));			\
		set_2regs(0x12,(c));			\
	} while(0)

#define set_ata2(a, b)					\
	do {						\
		set_2regs(0x0e,(a));			\
		set_2regs(0x0f,(b));			\
	} while(0)

#define set_pio(a, b, c)				\
	do { 						\
		set_2regs(0x0c,(a));			\
		set_2regs(0x0d,(b));			\
		set_2regs(0x13,(c));			\
	} while(0)

#define DISPLAY_PDC202XX_TIMINGS

#if defined(DISPLAY_PDC202XX_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 pdc202xx_proc;

static int pdc202xx_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t pdc202xx_procs[] __initdata = {
	{
		.name		= "pdc202xx",
		.set		= 1,
		.get_info	= pdc202xx_get_info,
		.parent		= NULL,
	},
};
#endif /* DISPLAY_PDC202XX_TIMINGS && CONFIG_PROC_FS */


static void init_setup_pdc202ata4(struct pci_dev *dev, ide_pci_device_t *d);
static void init_setup_pdc20265(struct pci_dev *, ide_pci_device_t *);
static void init_setup_pdc202xx(struct pci_dev *, ide_pci_device_t *);
static unsigned int init_chipset_pdc202xx(struct pci_dev *, const char *);
static void init_hwif_pdc202xx(ide_hwif_t *);
static void init_dma_pdc202xx(ide_hwif_t *, unsigned long);

static ide_pci_device_t pdc202xx_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_PROMISE,
		.device		= PCI_DEVICE_ID_PROMISE_20246,
		.name		= "PDC20246",
		.init_setup	= init_setup_pdc202ata4,
		.init_chipset	= init_chipset_pdc202xx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_pdc202xx,
		.init_dma	= init_dma_pdc202xx,
		.channels	= 2,
		.autodma	= AUTODMA,
#ifdef CONFIG_PDC202XX_FORCE
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
#else /* !CONFIG_PDC202XX_FORCE */
		.enablebits	= {{0x50,0x02,0x02}, {0x50,0x04,0x04}},
#endif
		.bootable	= OFF_BOARD,
		.extra		= 16,
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_PROMISE,
		.device		= PCI_DEVICE_ID_PROMISE_20262,
		.name		= "PDC20262",
		.init_setup	= init_setup_pdc202ata4,
		.init_chipset	= init_chipset_pdc202xx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_pdc202xx,
		.init_dma	= init_dma_pdc202xx,
		.channels	= 2,
		.autodma	= AUTODMA,
#ifdef CONFIG_PDC202XX_FORCE
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
#else /* !CONFIG_PDC202XX_FORCE */
		.enablebits	= {{0x50,0x02,0x02}, {0x50,0x04,0x04}},
#endif
		.bootable	= OFF_BOARD,
		.extra		= 48,
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_PROMISE,
		.device		= PCI_DEVICE_ID_PROMISE_20263,
		.name		= "PDC20263",
		.init_setup	= init_setup_pdc202ata4,
		.init_chipset	= init_chipset_pdc202xx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_pdc202xx,
		.init_dma	= init_dma_pdc202xx,
		.channels	= 2,
		.autodma	= AUTODMA,
#ifdef CONFIG_PDC202XX_FORCE
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
#else /* !CONFIG_PDC202XX_FORCE */
		.enablebits	= {{0x50,0x02,0x02}, {0x50,0x04,0x04}},
#endif
		.bootable	= OFF_BOARD,
		.extra		= 48,
	},{	/* 3 */
		.vendor		= PCI_VENDOR_ID_PROMISE,
		.device		= PCI_DEVICE_ID_PROMISE_20265,
		.name		= "PDC20265",
		.init_setup	= init_setup_pdc20265,
		.init_chipset	= init_chipset_pdc202xx,
		.init_hwif	= init_hwif_pdc202xx,
		.init_dma	= init_dma_pdc202xx,
		.channels	= 2,
		.autodma	= AUTODMA,
#ifdef CONFIG_PDC202XX_FORCE
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
#else /* !CONFIG_PDC202XX_FORCE */
		.enablebits	= {{0x50,0x02,0x02}, {0x50,0x04,0x04}},
#endif
		.bootable	= OFF_BOARD,
		.extra		= 48,
	},{	/* 4 */
		.vendor		= PCI_VENDOR_ID_PROMISE,
		.device		= PCI_DEVICE_ID_PROMISE_20267,
		.name		= "PDC20267",
		.init_setup	= init_setup_pdc202xx,
		.init_chipset	= init_chipset_pdc202xx,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_pdc202xx,
		.init_dma	= init_dma_pdc202xx,
		.channels	= 2,
		.autodma	= AUTODMA,
#ifdef CONFIG_PDC202XX_FORCE
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
#else /* !CONFIG_PDC202XX_FORCE */
		.enablebits	= {{0x50,0x02,0x02}, {0x50,0x04,0x04}},
#endif
		.bootable	= OFF_BOARD,
		.extra		= 48,
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* PDC202XX_H */
