/*
 * linux/include/asm-arm/arch-sa1100/huw_webpanel.h
 *
 * based of assabet.h
 *
 * This file contains the hardware specific definitions for HUW_Webpanel
 *
 * 2000/11/13 Roman Jordan <jor@hoeft-wessel.de>
 */

#ifndef __ASM_ARCH_HARDWARE_H
#error "include <asm/hardware.h> instead"
#endif

/* System Configuration Register flags */

#define SCR_SDRAM_LOW	(1<<2)	/* SDRAM size (low bit) */
#define SCR_SDRAM_HIGH	(1<<3)	/* SDRAM size (high bit) */
#define SCR_FLASH_LOW	(1<<4)	/* Flash size (low bit) */
#define SCR_FLASH_HIGH	(1<<5)	/* Flash size (high bit) */
#define SCR_GFX		(1<<8)	/* Graphics Accelerator (0 = present) */
#define SCR_SA1111	(1<<9)	/* Neponset (0 = present) */

#define SCR_INIT	-1


/* Board Control Register */

#define BCR_BASE  0xf1000000
#define BCR (*(volatile unsigned int *)(BCR_BASE))

#define BCR_PSIO_DTR1      (1<<29)
#define BCR_TFT_NPWR       (1<<28)
#define BCR_PSIO_DTR3      (1<<27)
#define BCR_TFT_ENA        (1<<26)
#define BCR_CCFL_POW       (1<<25)
#define BCR_PSIO_RTS1      (1<<24)
#define BCR_PWM_BACKLIGHT  (1<<23)


#ifndef __ASSEMBLY__
extern unsigned long SCR_value;
extern unsigned long BCR_value;
#define BCR_set( x )	BCR = (BCR_value |= (x))
#define BCR_clear( x )	BCR = (BCR_value &= ~(x))
#endif



