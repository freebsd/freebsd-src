/*
 * include/asm-arm/arch-sa1100/flexanet.h
 *
 * Created 2001/05/04 by Jordi Colomer <jco@ict.es>
 *
 * This file contains the hardware specific definitions for FlexaNet
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

/* Board Control Register (virtual address) */
#define FHH_BCR_PHYS  0x10000000
#define FHH_BCR_VIRT  0xf0000000
#define FHH_BCR (*(volatile unsigned int *)(FHH_BCR_VIRT))

/* Power-up value */
#define FHH_BCR_POWERUP	0x00000000

/* Mandatory bits */
#define FHH_BCR_LED_GREEN  (1<<0)  /* General-purpose green LED (1 = on) */
#define FHH_BCR_SPARE_1    (1<<1)  /* Not defined */
#define FHH_BCR_CF1_RST    (1<<2)  /* Compact Flash Slot #1 Reset (1 = reset) */
#define FHH_BCR_CF2_RST    (1<<3)  /* Compact Flash Slot #2 Reset (1 = reset) */
#define FHH_BCR_GUI_NRST   (1<<4)  /* GUI board reset (0 = reset) */
#define FHH_BCR_RTS1       (1<<5)  /* RS232 RTS for UART-1 */
#define FHH_BCR_RTS3       (1<<6)  /* RS232 RTS for UART-3 */
#define FHH_BCR_XCDBG0     (1<<7)  /* Not defined. Wired to XPLA3 for debug */

/* BCR extension, only required by L3-bus in some audio codecs */
#define FHH_BCR_L3MOD      (1<<8)  /* L3-bus MODE signal */
#define FHH_BCR_L3DAT      (1<<9)  /* L3-bus DATA signal */
#define FHH_BCR_L3CLK      (1<<10) /* L3-bus CLK signal */
#define FHH_BCR_SPARE_11   (1<<11) /* Not defined */
#define FHH_BCR_SPARE_12   (1<<12) /* Not defined */
#define FHH_BCR_SPARE_13   (1<<13) /* Not defined */
#define FHH_BCR_SPARE_14   (1<<14) /* Not defined */
#define FHH_BCR_SPARE_15   (1<<15) /* Not defined */

 /* Board Status Register (virtual address) */
#define FHH_BSR_BASE  FHH_BCR_VIRT
#define FHH_BSR (*(volatile unsigned int *)(FHH_BSR_BASE))

#define FHH_BSR_CTS1       (1<<0)  /* RS232 CTS for UART-1 */
#define FHH_BSR_CTS3       (1<<1)  /* RS232 CTS for UART-3 */
#define FHH_BSR_DSR1       (1<<2)  /* RS232 DSR for UART-1 */
#define FHH_BSR_DSR3       (1<<3)  /* RS232 DSR for UART-3 */
#define FHH_BSR_ID0        (1<<4)  /* Board identification */
#define FHH_BSR_ID1        (1<<5)
#define FHH_BSR_CFG0       (1<<6)  /* Board configuration options */
#define FHH_BSR_CFG1       (1<<7)

#ifndef __ASSEMBLY__
extern unsigned long flexanet_BCR;	/* Image of the BCR */
#define FLEXANET_BCR_set( x )    FHH_BCR = (flexanet_BCR |= (x))
#define FLEXANET_BCR_clear( x )  FHH_BCR = (flexanet_BCR &= ~(x))
#endif

/* GPIOs for which the generic definition doesn't say much */
#define GPIO_CF1_NCD       GPIO_GPIO (14)  /* Card Detect from CF slot #1 */
#define GPIO_CF2_NCD       GPIO_GPIO (15)  /* Card Detect from CF slot #2 */
#define GPIO_CF1_IRQ       GPIO_GPIO (16)  /* IRQ from CF slot #1 */
#define GPIO_CF2_IRQ       GPIO_GPIO (17)  /* IRQ from CF slot #2 */
#define GPIO_APP_IRQ       GPIO_GPIO (18)  /* Extra IRQ from application bus */
#define GPIO_RADIO_REF     GPIO_GPIO (20)  /* Ref. clock for UART3 (Radio) */
#define GPIO_CF1_BVD1      GPIO_GPIO (21)  /* BVD1 from CF slot #1 */
#define GPIO_CF2_BVD1      GPIO_GPIO (22)  /* BVD1 from CF slot #2 */
#define GPIO_GUI_IRQ       GPIO_GPIO (23)  /* IRQ from GUI board (i.e., UCB1300) */
#define GPIO_ETH_IRQ       GPIO_GPIO (24)  /* IRQ from Ethernet controller */
#define GPIO_INTIP_IRQ     GPIO_GPIO (25)  /* Measurement IRQ (INTIP) */
#define GPIO_LED_RED       GPIO_GPIO (26)  /* General-purpose red LED */

/* IRQ sources from GPIOs */
#define IRQ_GPIO_CF1_CD    IRQ_GPIO14
#define IRQ_GPIO_CF2_CD    IRQ_GPIO15
#define IRQ_GPIO_CF1_IRQ   IRQ_GPIO16
#define IRQ_GPIO_CF2_IRQ   IRQ_GPIO17
#define IRQ_GPIO_APP       IRQ_GPIO18
#define IRQ_GPIO_CF1_BVD1  IRQ_GPIO21
#define IRQ_GPIO_CF2_BVD1  IRQ_GPIO22
#define IRQ_GPIO_GUI       IRQ_GPIO23
#define IRQ_GPIO_ETH       IRQ_GPIO24
#define IRQ_GPIO_INTIP     IRQ_GPIO25


/* On-Board Ethernet */
#define _FHH_ETH_IOBASE		0x18000000	/* I/O base (physical addr) */
#define _FHH_ETH_MMBASE		0x18800000	/* Attribute-memory base */
#define FHH_ETH_SIZE		0x01000000	/* total size */
#define FHH_ETH_VIRT		0xF1000000	/* Ethernet virtual address */

#define FHH_ETH_p2v( x )	((x) - _FHH_ETH_IOBASE + FHH_ETH_VIRT)
#define FHH_ETH_v2p( x )	((x) - FHH_ETH_VIRT + _FHH_ETH_IOBASE)

#define FHH_ETH_IOBASE		FHH_ETH_p2v(_FHH_ETH_IOBASE) /* Virtual base addr */
#define FHH_ETH_MMBASE		FHH_ETH_p2v(_FHH_ETH_MMBASE)


