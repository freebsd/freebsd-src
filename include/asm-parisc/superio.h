#ifndef _PARISC_SUPERIO_H
#define _PARISC_SUPERIO_H

/* Offsets to configuration and base address registers */
#define IC_PIC1    0x20		/* PCI I/O address of master 8259 */
#define IC_PIC2    0xA0		/* PCI I/O address of slave */
#define SIO_CR     0x5A		/* Configuration Register */
#define SIO_ACPIBAR 0x88		/* ACPI BAR */
#define SIO_FDCBAR 0x90		/* Floppy Disk Controller BAR */
#define SIO_SP1BAR 0x94		/* Serial 1 BAR */
#define SIO_SP2BAR 0x98		/* Serial 2 BAR */
#define SIO_PPBAR  0x9C		/* Parallel BAR */

/* Interrupt triggers and routing */
#define TRIGGER_1  0x67		/* Edge/level trigger register 1 */
#define TRIGGER_2  0x68		/* Edge/level trigger register 2 */
#define IR_SER     0x69		/* Serial 1 [0:3] and Serial 2 [4:7] */
#define IR_PFD     0x6a		/* Parallel [0:3] and Floppy [4:7] */
#define IR_IDE     0x6b		/* IDE1 [0:3] and IDE2 [4:7] */
#define IR_USB     0x6d         /* USB [4:7] */
#define IR_LOW     0x69		/* Lowest interrupt routing reg */
#define IR_HIGH    0x71		/* Highest interrupt routing reg */

/* 8259 operational control words */
#define OCW2_EOI   0x20		/* Non-specific EOI */
#define OCW2_SEOI  0x60		/* Specific EOI */
#define OCW3_IIR   0x0A		/* Read request register */
#define OCW3_ISR   0x0B		/* Read service register */
#define OCW3_POLL  0x0C		/* Poll the PIC for an interrupt vector */

/* Interrupt lines. Only PIC1 is used */
#define USB_IRQ    1		/* USB */
#define SP1_IRQ    3		/* Serial port 1 */
#define SP2_IRQ    4		/* Serial port 2 */
#define PAR_IRQ    5		/* Parallel port */
#define FDC_IRQ    6		/* Floppy controller */
#define IDE_IRQ    7		/* IDE (pri+sec) */

/* ACPI registers */
#define USB_REG_CR	0x1f	/* USB Regulator Control Register */

#define SUPERIO_NIRQS   8

struct superio_device {
	u16 fdc_base;
	u16 sp1_base;
	u16 sp2_base;
	u16 pp_base;
	u16 acpi_base;
	int iosapic_irq;
	int iosapic_irq_enabled;
	struct irq_region *irq_region;
	struct pci_dev *lio_pdev;       /* pci device for legacy IO fn */
};

/*
 * Does NS make a 87415 based plug in PCI card? If so, because of this
 * macro we currently don't support it being plugged into a machine
 * that contains a SuperIO chip AND has CONFIG_SUPERIO enabled.
 *
 * This could be fixed by checking to see if function 1 exists, and
 * if it is SuperIO Legacy IO; but really now, is this combination
 * going to EVER happen?
 */

#define SUPERIO_IDE_FN 0 /* Function number of IDE controller */
#define SUPERIO_LIO_FN 1 /* Function number of Legacy IO controller */
#define SUPERIO_USB_FN 2 /* Function number of USB controller */

#define is_superio_device(x) \
	(((x)->vendor == PCI_VENDOR_ID_NS) && \
	(  ((x)->device == PCI_DEVICE_ID_NS_87415) \
	|| ((x)->device == PCI_DEVICE_ID_NS_87560_LIO) \
	|| ((x)->device == PCI_DEVICE_ID_NS_87560_USB) ) )

extern void superio_inform_irq(int irq);
extern void superio_serial_init(void);		/* called by rs_init() */
extern int superio_fixup_irq(struct pci_dev *pcidev); /* called by iosapic */
extern int superio_get_ide_irq(void);

#endif /* _PARISC_SUPERIO_H */
