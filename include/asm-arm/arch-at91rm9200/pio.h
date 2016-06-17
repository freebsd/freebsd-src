/*
 * linux/include/asm-arm/arch-at91rm9200/pio.h
 *
 *  Copyright (c) 2003 SAN People
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __ASM_ARCH_PIO_H
#define __ASM_ARCH_PIO_H

#include <asm/arch/hardware.h>

static inline void AT91_CfgPIO_USART0(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA17_TXD0 | AT91C_PA18_RXD0
		| AT91C_PA20_CTS0 | AT91C_PA21_RTS0;
}

static inline void AT91_CfgPIO_USART1(void) {
	AT91_SYS->PIOB_PDR = AT91C_PB18_RI1 | AT91C_PB19_DTR1
		| AT91C_PB20_TXD1 | AT91C_PB21_RXD1 | AT91C_PB23_DCD1
		| AT91C_PB24_CTS1 | AT91C_PB25_DSR1 | AT91C_PB26_RTS1;
}

static inline void AT91_CfgPIO_USART2(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA22_RXD2 | AT91C_PA23_TXD2;
}

static inline void AT91_CfgPIO_USART3(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA5_TXD3 | AT91C_PA6_RXD3;
	AT91_SYS->PIOA_BSR = AT91C_PA5_TXD3 | AT91C_PA6_RXD3;
}

static inline void AT91_CfgPIO_DBGU(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA31_DTXD | AT91C_PA30_DRXD;
}

/*
 * Configure Ethernet for RMII mode.
 */
static inline void AT91_CfgPIO_EMAC_RMII(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA16_EMDIO | AT91C_PA15_EMDC | AT91C_PA14_ERXER | AT91C_PA13_ERX1
		| AT91C_PA12_ERX0 | AT91C_PA11_ECRS_ECRSDV | AT91C_PA10_ETX1
		| AT91C_PA9_ETX0 | AT91C_PA8_ETXEN | AT91C_PA7_ETXCK_EREFCK;
	AT91_SYS->PIOB_PDR = AT91C_PB25_EF100 | AT91C_PB19_ERXCK;
	AT91_SYS->PIOB_BSR = AT91C_PB25_EF100 | AT91C_PB19_ERXCK;
}

/*
 * Configure Ethernet for MII mode.
 */
static inline void AT91_CfgPIO_EMAC_MII(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA16_EMDIO | AT91C_PA15_EMDC | AT91C_PA14_ERXER | AT91C_PA13_ERX1
		| AT91C_PA12_ERX0 | AT91C_PA11_ECRS_ECRSDV | AT91C_PA10_ETX1
		| AT91C_PA9_ETX0 | AT91C_PA8_ETXEN | AT91C_PA7_ETXCK_EREFCK;
	AT91_SYS->PIOB_PDR = AT91C_PB25_EF100 | AT91C_PB19_ERXCK | AT91C_PB18_ECOL | AT91C_PB17_ERXDV
		| AT91C_PB16_ERX3 | AT91C_PB15_ERX2 | AT91C_PB14_ETXER | AT91C_PB13_ETX3
		| AT91C_PB12_ETX2;
	AT91_SYS->PIOB_BSR = AT91C_PB25_EF100 | AT91C_PB19_ERXCK | AT91C_PB18_ECOL | AT91C_PB17_ERXDV
		| AT91C_PB16_ERX3 | AT91C_PB15_ERX2 | AT91C_PB14_ETXER | AT91C_PB13_ETX3
		| AT91C_PB12_ETX2;
}

/*
 * Enable the Two-Wire interface.
 */
static inline void AT91_CfgPIO_TWI(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA25_TWD | AT91C_PA26_TWCK;
	AT91_SYS->PIOA_ASR = AT91C_PA25_TWD | AT91C_PA26_TWCK;
}

/*
 * Enable the Serial Peripheral Interface.
 */
static inline void AT91_CfgPIO_SPI(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA0_MISO | AT91C_PA1_MOSI | AT91C_PA2_SPCK;
}

static inline void AT91_CfgPIO_SPI_CS0(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA3_NPCS0;
}

static inline void AT91_CfgPIO_SPI_CS1(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA4_NPCS1;
}

static inline void AT91_CfgPIO_SPI_CS2(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA5_NPCS2;
}

static inline void AT91_CfgPIO_SPI_CS3(void) {
	AT91_SYS->PIOA_PDR = AT91C_PA6_NPCS3;
}

/*
 * Select the DataFlash card.
 */
static inline void AT91_CfgPIO_DataFlashCard(void) {
	AT91_SYS->PIOB_PER = AT91C_PIO_PB7;
	AT91_SYS->PIOB_OER = AT91C_PIO_PB7;
	AT91_SYS->PIOB_CODR = AT91C_PIO_PB7;
}

/*
 * Enable NAND Flash (SmartMedia) interface.
 */
static inline void AT91_CfgPIO_SmartMedia(void) {
	/* enable PC0=SMCE, PC1=SMOE, PC3=SMWE, A21=CLE, A22=ALE */
	AT91_SYS->PIOC_ASR = AT91C_PC0_BFCK | AT91C_PC1_BFRDY_SMOE | AT91C_PC3_BFBAA_SMWE;
	AT91_SYS->PIOC_PDR = AT91C_PC0_BFCK | AT91C_PC1_BFRDY_SMOE | AT91C_PC3_BFBAA_SMWE;

	/* Configure PC2 as input (signal READY of the SmartMedia) */
	AT91_SYS->PIOC_PER = AT91C_PC2_BFAVD;	/* enable direct output enable */
	AT91_SYS->PIOC_ODR = AT91C_PC2_BFAVD;	/* disable output */

	/* Configure PB1 as input (signal Card Detect of the SmartMedia) */
	AT91_SYS->PIOB_PER = AT91C_PIO_PB1;	/* enable direct output enable */
	AT91_SYS->PIOB_ODR = AT91C_PIO_PB1;	/* disable output */
}

static inline int AT91_PIO_SmartMedia_RDY(void) {
	return (AT91_SYS->PIOC_PDSR & AT91C_PIO_PC2) ? 1 : 0;
}

static inline int AT91_PIO_SmartMedia_CardDetect(void) {
	return (AT91_SYS->PIOB_PDSR & AT91C_PIO_PB1) ? 1 : 0;
}

#endif
