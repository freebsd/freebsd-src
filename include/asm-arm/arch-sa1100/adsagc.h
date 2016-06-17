/*
 * linux/include/asm-arm/arch-sa1100/adsagc.h
 *
 * Created Feb 7, 2003 by Robert Whaley <rwhaley@applieddata.net>
 *
 * This file comes from graphicsmaster.h of Woojung Huh <whuh@applieddata.net>
 *
 * This file contains the hardware specific definitions for the
 * ADS Advanced Graphics Client
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

#define ADS_CPLD_BASE		(0x10000000)
#define ADS_p2v( x )		((x) - ADS_CPLD_BASE + 0xf0000000)
#define ADS_v2p( x )		((x) - 0xf0000000 + ADS_CPLD_BASE)


#define _ADS_SW_SWITCHES	0x10060000	/* Software Switches */

#define _ADS_CR1                0x10060004      /* audio and serial */
#define _ADS_CR2                0x10060008      /* clocks */
#define _ADS_CR3                0x1006000c      /* AVR, LCD, LEDs */

#define ADS_CR1_AMP             0x01
#define ADS_CR1_CODEC           0x02
#define ADS_CR1_BTL             0x04
#define ADS_CR1_AUDIO_RST       0x08
#define ADS_CR1_COM3_ENA        0x10
#define ADS_CR1_IRDA_ENA        0x20
#define ADS_CR1_SPI_SEL         0x40
#define ADS_CR1_ARM_RST         0x80

#define ADS_CR2_CLK_SEL0        0x01
#define ADS_CR2_CLK_SEL1        0x02
#define ADS_CR2_PLL_OFF         0x04
#define ADS_CR2_CLK_PWR         0x08

#define ADS_CR3_PNLON           0x01
#define ADS_CR3_VEECTL          0x02
#define ADS_CR3_BLON            0x04
#define ADS_CR3_WAKEUP          0x08


/* Extra IRQ Controller */
#define _ADS_INT_ST1		0x10080000	/* IRQ Status #1 */
#define _ADS_INT_EN1		0x10080008	/* IRQ Enable #1 */
#define _ADS_DCR			0x10080018	/* Discrete Control Reg */

/* Discrete Controller (AVR:Atmel AT90LS8535) */
#define _ADS_AVR_REG		0x10080018

/* On-Board Ethernet */
#define _ADS_ETHERNET		0x40000000	/* Ethernet */

/* LEDs */
#define ADS_LED0	0x10		/* on-board Green */
#define ADS_LED1	0x20		/* on-board Yellow */
#define ADS_LED2	0x40		/* on-board Red */

/* DCR */
#define DCR_AVR_RESET		0x01
#define DCR_SA1111_RESET	0x02
#define	DCR_BACKLITE_ON		0x04

/* Virtual register addresses */

#ifndef __ASSEMBLY__
#define ADS_INT_ST1	(*((volatile u_char *) ADS_p2v(_ADS_INT_ST1)))
#define ADS_INT_EN1	(*((volatile u_char *) ADS_p2v(_ADS_INT_EN1)))

#define ADS_CR1  	(*((volatile u_char *) ADS_p2v(_ADS_CR1)))
#define ADS_CR2  	(*((volatile u_char *) ADS_p2v(_ADS_CR2)))
#define ADS_CR3  	(*((volatile u_char *) ADS_p2v(_ADS_CR3)))

#define ADS_ETHERNET	((int) ADS_p2v(_ADS_ETHERNET))
#define ADS_AVR_REG	(*((volatile u_char *) ADS_p2v(_ADS_AVR_REG)))
#define ADS_DCR		(*((volatile u_char *) ADS_p2v(_ADS_DCR)))
#endif

#define ADS_SA1111_BASE		(0x18000000)
