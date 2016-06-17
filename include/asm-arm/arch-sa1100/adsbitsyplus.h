/*
 * linux/include/asm-arm/arch-sa1100/adsbitsy.h
 *
 * Created Feb 7, 2003 by Robert Whaley <rwhaley@applieddata.net>
 *
 * This file comes from adsbitsy.h of Woojung Huh <whuh@applieddata.net>
 *
 * This file contains the hardware specific definitions for the
 * ADS Bitsy Board
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

#define ADS_CPLD_BASE		(0x10000000)
#define ADS_p2v( x )		((x) - ADS_CPLD_BASE + 0xf1000000)
#define ADS_v2p( x )		((x) - 0xf1000000 + ADS_CPLD_BASE)


#define _ADS_CPLD_IO1                0x10000000
#define _ADS_CPLD_IO2                0x10000004
#define _ADS_CPLD_IODR1              0x10000008
#define _ADS_CPLD_IODR2              0x1000000C
#define _ADS_CPLD_PCON               0x10000010
#define _ADS_CPLD_SUPPC              0x10000014

/* IO bits */
#define ADS_IO1_MASK 0xFF
#define ADS_IO2_MASK 0x03

#define ADS_IO1_BIT0 0x01
#define ADS_IO1_BIT1 0x02
#define ADS_IO1_BIT2 0x04
#define ADS_IO1_BIT3 0x08
#define ADS_IO1_BIT4 0x10
#define ADS_IO1_BIT5 0x20
#define ADS_IO1_BIT6 0x40
#define ADS_IO1_BIT7 0x80
#define ADS_IO2_BIT8 0x01
#define ADS_IO2_BIT9 0x02

#define ADS_IO2_CPLD_REV_MASK 0xF0
#define ADS_IO2_CPLD_REV_5_MAGIC 0x30

#define ADS_PCON_PANEL_ON   0x01
#define ADS_PCON_AUDIO_ON   0x02
#define ADS_PCON_AUDIOPA_ON 0x04
#define ADS_PCON_COM1_3_ON  0x08
#define ADS_PCON_CONN_B_PE1 0x10
#define ADS_PCON_CONN_B_PE2 0x20

#define ADS_SUPPC_VEE_ON     0x01
#define ADS_SUPPC_TS_SPI_SEL 0x02
#define ADS_SUPPC_CB_SPI_SEL 0x04
#define ADS_SUPPC_AVR_WKP    0x08

/* Virtual register addresses */

#ifndef __ASSEMBLY__
#define ADS_CPLD_IO1                (*((volatile u_char *) ADS_p2v(_ADS_CPLD_IO1)))
#define ADS_CPLD_IO2                (*((volatile u_char *) ADS_p2v(_ADS_CPLD_IO2)))
#define ADS_CPLD_IODR1              (*((volatile u_char *) ADS_p2v(_ADS_CPLD_IODR1)))
#define ADS_CPLD_IODR2              (*((volatile u_char *) ADS_p2v(_ADS_CPLD_IODR2)))
#define ADS_CPLD_PCON               (*((volatile u_char *) ADS_p2v(_ADS_CPLD_PCON)))
#define ADS_CPLD_SUPPC              (*((volatile u_char *) ADS_p2v(_ADS_CPLD_SUPPC)))

#endif

#define ADSBITSYPLUS_SA1111_BASE	(0x18000000)
