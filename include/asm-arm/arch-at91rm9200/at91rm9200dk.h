/*
 * linux/include/asm-arm/arch-at91rm9200/at91rm9200dk.h
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

#ifndef __ASM_ARCH_HARDWARE_AT91RM9200DK_H
#define __ASM_ARCH_HARDWARE_AT91RM9200DK_H


/* AT91RM92000 clocks */
#define AT91C_MAIN_CLOCK	179712000	/* from 18.432 MHz crystal (18432000 / 4 * 39) */
#define AT91C_MASTER_CLOCK	59904000	/* peripheral clock (AT91C_MASTER_CLOCK / 3) */
#define AT91C_SLOW_CLOCK	32768		/* slow clock */


/* FLASH */
#define AT91_FLASH_BASE		0x10000000	// NCS0: Flash physical base address

/* SDRAM */
#define AT91_SDRAM_BASE		0x20000000	// NCS1: SDRAM physical base address

/* SmartMedia */
#define AT91_SMARTMEDIA_BASE	0x40000000	// NCS3: Smartmedia physical base address

/* Multi-Master Memory controller */
#define AT91_UHP_BASE		0x00300000	// USB Host controller


/* Peripheral interrupt configuration */
#define AT91_SMR_FIQ	(AT91C_AIC_PRIOR_HIGHEST | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (FIQ)
#define AT91_SMR_SYS	(AT91C_AIC_PRIOR_HIGHEST | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// System Peripheral
#define AT91_SMR_PIOA	(AT91C_AIC_PRIOR_LOWEST	 | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Parallel IO Controller A
#define AT91_SMR_PIOB	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Parallel IO Controller B
#define AT91_SMR_PIOC	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Parallel IO Controller C
#define AT91_SMR_PIOD	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Parallel IO Controller D
#define AT91_SMR_US0	(AT91C_AIC_PRIOR_6       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USART 0
#define AT91_SMR_US1	(AT91C_AIC_PRIOR_6       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USART 1
#define AT91_SMR_US2	(AT91C_AIC_PRIOR_6       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USART 2
#define AT91_SMR_US3	(AT91C_AIC_PRIOR_6       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USART 3
#define AT91_SMR_MCI	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Multimedia Card Interface
#define AT91_SMR_UDP	(AT91C_AIC_PRIOR_4       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USB Device Port
#define AT91_SMR_TWI	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Two-Wire Interface
#define AT91_SMR_SPI	(AT91C_AIC_PRIOR_6       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Serial Peripheral Interface
#define AT91_SMR_SSC0	(AT91C_AIC_PRIOR_5       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Serial Synchronous Controller 0
#define AT91_SMR_SSC1	(AT91C_AIC_PRIOR_5       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Serial Synchronous Controller 1
#define AT91_SMR_SSC2	(AT91C_AIC_PRIOR_5       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Serial Synchronous Controller 2
#define AT91_SMR_TC0	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 0
#define AT91_SMR_TC1	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 1
#define AT91_SMR_TC2	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 2
#define AT91_SMR_TC3	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 3
#define AT91_SMR_TC4	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 4
#define AT91_SMR_TC5	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Timer Counter 5
#define AT91_SMR_UHP	(AT91C_AIC_PRIOR_3       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// USB Host port
#define AT91_SMR_EMAC	(AT91C_AIC_PRIOR_3       | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Ethernet MAC
#define AT91_SMR_IRQ0	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ0)
#define AT91_SMR_IRQ1	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ1)
#define AT91_SMR_IRQ2	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ2)
#define AT91_SMR_IRQ3	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ3)
#define AT91_SMR_IRQ4	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ4)
#define AT91_SMR_IRQ5	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ5)
#define AT91_SMR_IRQ6	(AT91C_AIC_PRIOR_LOWEST  | AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE)	// Advanced Interrupt Controller (IRQ6)


/*
 * Serial port configuration.
 *    0 .. 3 = USART0 .. USART3
 *    4      = DBGU
 */
#define AT91C_UART_MAP		{ 4, 1, -1, -1, -1 }	/* ttyS0, ..., ttyS4 */
#define AT91C_CONSOLE		0			/* ttyS0 */

#endif
