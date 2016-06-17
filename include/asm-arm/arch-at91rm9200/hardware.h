/*
 * linux/include/asm-arm/arch-at91rm9200/hardware.h
 *
 *  Copyright (c) 2003 SAN People
 *  Copyright (c) 2003 ATMEL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/sizes.h>

#include <asm/arch/AT91RM9200.h>
#include <asm/arch/AT91RM9200_SYS.h>

#ifndef __ASSEMBLY__
 /*
  * The following variable is defined in arch/arm/mach-at91rm9200/core.c
  * It is a pointer to the AT91RM9200 system peripherals.
  */
extern AT91PS_SYS AT91_SYS;
#endif


/*
 * Remap the peripherals from address 0xFFFA0000 .. 0xFFFFFFFF
 * to 0xFEFA0000 .. 0xFF000000.  (384Kb)
 */
#define AT91C_IO_PHYS_BASE	0xFFFA0000
#define AT91C_IO_SIZE		(0xFFFFFFFF - AT91C_IO_PHYS_BASE + 1)
#define AT91C_IO_VIRT_BASE	(0xFF000000 - AT91C_IO_SIZE)

 /* Convert a physical IO address to virtual IO address */
#define AT91_IO_P2V(x)	((x) - AT91C_IO_PHYS_BASE + AT91C_IO_VIRT_BASE)

/*
 * Virtual to Physical Address mapping for IO devices.
 */
#define AT91C_VA_BASE_SYS	AT91_IO_P2V(AT91C_BASE_SYS)
#define AT91C_VA_BASE_SPI	AT91_IO_P2V(AT91C_BASE_SPI)
#define AT91C_VA_BASE_SSC2	AT91_IO_P2V(AT91C_BASE_SSC2)
#define AT91C_VA_BASE_SSC1	AT91_IO_P2V(AT91C_BASE_SSC1)
#define AT91C_VA_BASE_SSC0	AT91_IO_P2V(AT91C_BASE_SSC0)
#define AT91C_VA_BASE_US3	AT91_IO_P2V(AT91C_BASE_US3)
#define AT91C_VA_BASE_US2	AT91_IO_P2V(AT91C_BASE_US2)
#define AT91C_VA_BASE_US1	AT91_IO_P2V(AT91C_BASE_US1)
#define AT91C_VA_BASE_US0	AT91_IO_P2V(AT91C_BASE_US0)
#define AT91C_VA_BASE_EMAC	AT91_IO_P2V(AT91C_BASE_EMAC)
#define AT91C_VA_BASE_TWI	AT91_IO_P2V(AT91C_BASE_TWI)
#define AT91C_VA_BASE_MCI	AT91_IO_P2V(AT91C_BASE_MCI)
#define AT91C_VA_BASE_UDP	AT91_IO_P2V(AT91C_BASE_UDP)
#define AT91C_VA_BASE_TCB1	AT91_IO_P2V(AT91C_BASE_TCB1)
#define AT91C_VA_BASE_TCB0	AT91_IO_P2V(AT91C_BASE_TCB0)


#define AT91C_BASE_SRAM		0x00200000	/* Internal SRAM base address */

#define AT91C_NR_UART		5		/* 4 USART3's and one DBGU port */

 /* Definition of interrupt priority levels */
#define AT91C_AIC_PRIOR_0 AT91C_AIC_PRIOR_LOWEST
#define AT91C_AIC_PRIOR_1 ((unsigned int) 0x1)
#define AT91C_AIC_PRIOR_2 ((unsigned int) 0x2)
#define AT91C_AIC_PRIOR_3 ((unsigned int) 0x3)
#define AT91C_AIC_PRIOR_4 ((unsigned int) 0x4)
#define AT91C_AIC_PRIOR_5 ((unsigned int) 0x5)
#define AT91C_AIC_PRIOR_6 ((unsigned int) 0x6)
#define AT91C_AIC_PRIOR_7 AT91C_AIC_PRIOR_HIGEST


/*
 * Implementation specific hardware definitions.
 */

#ifdef CONFIG_ARCH_AT91RM9200DK
#include <asm/arch/at91rm9200dk.h>
#endif


#endif
