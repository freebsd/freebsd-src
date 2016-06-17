#ifndef __ASM_SH_HITACHI_HS7729PCI_H
#define __ASM_SH_HITACHI_HS7729PCI_H

/*
 * linux/include/asm-sh/hitachi_hs7729pci.h
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Hitachi Semiconductor and Devices HS7729PCI support
 */

/* Box specific addresses.  */

#define PA_SUPERIO	0xa8000000	/* SMC37C935A super io chip */
#define PA_DIPSW0	0xa8c00000	/* Dip switch 5,6 */
#define PA_DIPSW1	0xa8c00002	/* Dip switch 7,8 */
#define PA_LED		0xa8c40000	/* LED */
#define PA_7SEG		0xa8c40002	/* 7 segment LED */
#define PA_BCR		0xa8c80000	/* FPGA */

#define PA_MRSHPC	0xb83fffe0	/* MR-SHPC-01 PCMCIA controler */
#define PA_MRSHPC_MW1	0xb8400000	/* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2	0xb8500000	/* MR-SHPC-01 attribute window base */
#define PA_MRSHPC_IO	0xb8600000	/* MR-SHPC-01 I/O window base */
#define MRSHPC_OPTION   (PA_MRSHPC + 0x06)
#define MRSHPC_CSR      (PA_MRSHPC + 0x08)
#define MRSHPC_ISR      (PA_MRSHPC + 0x0a)
#define MRSHPC_ICR      (PA_MRSHPC + 0x0c)
#define MRSHPC_CPWCR    (PA_MRSHPC + 0x0e)
#define MRSHPC_MW0CR1   (PA_MRSHPC + 0x10)
#define MRSHPC_MW1CR1   (PA_MRSHPC + 0x12)
#define MRSHPC_IOWCR1   (PA_MRSHPC + 0x14)
#define MRSHPC_MW0CR2   (PA_MRSHPC + 0x16)
#define MRSHPC_MW1CR2   (PA_MRSHPC + 0x18)
#define MRSHPC_IOWCR2   (PA_MRSHPC + 0x1a)
#define MRSHPC_CDCR     (PA_MRSHPC + 0x1c)
#define MRSHPC_PCIC_INFO (PA_MRSHPC + 0x1e)

#define BCR_ILCRA	(PA_BCR + 0)
#define BCR_ILCRB	(PA_BCR + 2)
#define BCR_ILCRC	(PA_BCR + 4)
#define BCR_ILCRD	(PA_BCR + 6)
#define BCR_ILCRE	(PA_BCR + 8)
#define BCR_ILCRF	(PA_BCR + 10)
#define BCR_ILCRG	(PA_BCR + 12)

#define SH7729PCI_PCI_HOST_BRIDGE 0xb0000000	/* SD0001 Register */
#define SH7729PCI_PCI_AREA	0xb0800000	/* PCI I/O Window */
#define SH7729PCI_PCI_MEM_START CONFIG_MEMORY_START
#define SH7729PCI_PCI_MEM_SIZE  CONFIG_MEMORY_SIZE

#define SH7729PCI_PCI_IRQ  0		/* SD0001 Interrupt Number */

#endif  /* __ASM_SH_HITACHI_HS7729PCI_H */
