/*-
 *  ----------------------------------------------------------------------------
 *          ATMEL Microcontroller Software Support  -  ROUSSET  -
 *  ----------------------------------------------------------------------------
 *  Copyright (c) 2009, Atmel Corporation
 * 
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 * 
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the disclaimer below.
 * 
 *  Atmel's name may not be used to endorse or promote products derived from
 *  this software without specific prior written permission. 
 *  
 *  DISCLAIMER:  THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 *  DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 *  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  ----------------------------------------------------------------------------
 *
 * From AT91LIB version 1.9 boards/at91sam9g45-ek/at91sam9g45/AT91SAM9G45.h
 */

/* $FreeBSD$ */

#ifndef  ARM_AT91_AT91_PIO_SAM9G45_H
#define  ARM_AT91_AT91_PIO_SAM9G45_H

#include <arm/at91/at91_pioreg.h>

// *****************************************************************************
//               PIO DEFINITIONS FOR AT91SAM9G45
// *****************************************************************************
#define AT91C_PA0_MCI0_CK  (AT91C_PIO_PA0) //  
#define AT91C_PA0_TCLK3    (AT91C_PIO_PA0) //  
#define AT91C_PA1_MCI0_CDA (AT91C_PIO_PA1) //  
#define AT91C_PA1_TIOA3    (AT91C_PIO_PA1) //  
#define AT91C_PA10_ETX0     (AT91C_PIO_PA10) //  Ethernet MAC Transmit Data 0
#define AT91C_PA11_ETX1     (AT91C_PIO_PA11) //  Ethernet MAC Transmit Data 1
#define AT91C_PA12_ERX0     (AT91C_PIO_PA12) //  Ethernet MAC Receive Data 0
#define AT91C_PA13_ERX1     (AT91C_PIO_PA13) //  Ethernet MAC Receive Data 1
#define AT91C_PA14_ETXEN    (AT91C_PIO_PA14) //  Ethernet MAC Transmit Enable
#define AT91C_PA15_ERXDV    (AT91C_PIO_PA15) //  Ethernet MAC Receive Data Valid
#define AT91C_PA16_ERXER    (AT91C_PIO_PA16) //  Ethernet MAC Receive Error
#define AT91C_PA17_ETXCK_EREFCK (AT91C_PIO_PA17) //  Ethernet MAC Transmit Clock/Reference Clock
#define AT91C_PA18_EMDC     (AT91C_PIO_PA18) //  Ethernet MAC Management Data Clock
#define AT91C_PA19_EMDIO    (AT91C_PIO_PA19) //  Ethernet MAC Management Data Input/Output
#define AT91C_PA2_MCI0_DA0 (AT91C_PIO_PA2) //  
#define AT91C_PA2_TIOB3    (AT91C_PIO_PA2) //  
#define AT91C_PA20_TWD0     (AT91C_PIO_PA20) //  TWI Two-wire Serial Data
#define AT91C_PA21_TWCK0    (AT91C_PIO_PA21) //  TWI Two-wire Serial Clock
#define AT91C_PA22_MCI1_CDA (AT91C_PIO_PA22) //  
#define AT91C_PA22_SCK3     (AT91C_PIO_PA22) //  
#define AT91C_PA23_MCI1_DA0 (AT91C_PIO_PA23) //  
#define AT91C_PA23_RTS3     (AT91C_PIO_PA23) //  
#define AT91C_PA24_MCI1_DA1 (AT91C_PIO_PA24) //  
#define AT91C_PA24_CTS3     (AT91C_PIO_PA24) //  
#define AT91C_PA25_MCI1_DA2 (AT91C_PIO_PA25) //  
#define AT91C_PA25_PWM3     (AT91C_PIO_PA25) //  
#define AT91C_PA26_MCI1_DA3 (AT91C_PIO_PA26) //  
#define AT91C_PA26_TIOB2    (AT91C_PIO_PA26) //  
#define AT91C_PA27_MCI1_DA4 (AT91C_PIO_PA27) //  
#define AT91C_PA27_ETXER    (AT91C_PIO_PA27) //  Ethernet MAC Transmikt Coding Error
#define AT91C_PA28_MCI1_DA5 (AT91C_PIO_PA28) //  
#define AT91C_PA28_ERXCK    (AT91C_PIO_PA28) //  Ethernet MAC Receive Clock
#define AT91C_PA29_MCI1_DA6 (AT91C_PIO_PA29) //  
#define AT91C_PA29_ECRS     (AT91C_PIO_PA29) //  Ethernet MAC Carrier Sense/Carrier Sense and Data Valid
#define AT91C_PA3_MCI0_DA1 (AT91C_PIO_PA3) //  
#define AT91C_PA3_TCLK4    (AT91C_PIO_PA3) //  
#define AT91C_PA30_MCI1_DA7 (AT91C_PIO_PA30) //  
#define AT91C_PA30_ECOL     (AT91C_PIO_PA30) //  Ethernet MAC Collision Detected
#define AT91C_PA31_MCI1_CK  (AT91C_PIO_PA31) //  
#define AT91C_PA31_PCK0     (AT91C_PIO_PA31) //  
#define AT91C_PA4_MCI0_DA2 (AT91C_PIO_PA4) //  
#define AT91C_PA4_TIOA4    (AT91C_PIO_PA4) //  
#define AT91C_PA5_MCI0_DA3 (AT91C_PIO_PA5) //  
#define AT91C_PA5_TIOB4    (AT91C_PIO_PA5) //  
#define AT91C_PA6_MCI0_DA4 (AT91C_PIO_PA6) //  
#define AT91C_PA6_ETX2     (AT91C_PIO_PA6) //  Ethernet MAC Transmit Data 2
#define AT91C_PA7_MCI0_DA5 (AT91C_PIO_PA7) //  
#define AT91C_PA7_ETX3     (AT91C_PIO_PA7) //  Ethernet MAC Transmit Data 3
#define AT91C_PA8_MCI0_DA6 (AT91C_PIO_PA8) //  
#define AT91C_PA8_ERX2     (AT91C_PIO_PA8) //  Ethernet MAC Receive Data 2
#define AT91C_PA9_MCI0_DA7 (AT91C_PIO_PA9) //  
#define AT91C_PA9_ERX3     (AT91C_PIO_PA9) //  Ethernet MAC Receive Data 3
#define AT91C_PB0_SPI0_MISO (AT91C_PIO_PB0) //  SPI 0 Master In Slave
#define AT91C_PB1_SPI0_MOSI (AT91C_PIO_PB1) //  SPI 0 Master Out Slave
#define AT91C_PB10_TWD1     (AT91C_PIO_PB10) //  
#define AT91C_PB10_ISI_D10  (AT91C_PIO_PB10) //  
#define AT91C_PB11_TWCK1    (AT91C_PIO_PB11) //  
#define AT91C_PB11_ISI_D11  (AT91C_PIO_PB11) //  
#define AT91C_PB12_DRXD     (AT91C_PIO_PB12) //  
#define AT91C_PB13_DTXD     (AT91C_PIO_PB13) //  
#define AT91C_PB14_SPI1_MISO (AT91C_PIO_PB14) //  
#define AT91C_PB15_SPI1_MOSI (AT91C_PIO_PB15) //  
#define AT91C_PB15_CTS0     (AT91C_PIO_PB15) //  
#define AT91C_PB16_SPI1_SPCK (AT91C_PIO_PB16) //  
#define AT91C_PB16_SCK0     (AT91C_PIO_PB16) //  
#define AT91C_PB17_SPI1_NPCS0 (AT91C_PIO_PB17) //  
#define AT91C_PB17_RTS0     (AT91C_PIO_PB17) //  
#define AT91C_PB18_RXD0     (AT91C_PIO_PB18) //  
#define AT91C_PB18_SPI0_NPCS1 (AT91C_PIO_PB18) //  
#define AT91C_PB19_TXD0     (AT91C_PIO_PB19) //  
#define AT91C_PB19_SPI0_NPCS2 (AT91C_PIO_PB19) //  
#define AT91C_PB2_SPI0_SPCK (AT91C_PIO_PB2) //  SPI 0 Serial Clock
#define AT91C_PB20_ISI_D0   (AT91C_PIO_PB20) //  
#define AT91C_PB21_ISI_D1   (AT91C_PIO_PB21) //  
#define AT91C_PB22_ISI_D2   (AT91C_PIO_PB22) //  
#define AT91C_PB23_ISI_D3   (AT91C_PIO_PB23) //  
#define AT91C_PB24_ISI_D4   (AT91C_PIO_PB24) //  
#define AT91C_PB25_ISI_D5   (AT91C_PIO_PB25) //  
#define AT91C_PB26_ISI_D6   (AT91C_PIO_PB26) //  
#define AT91C_PB27_ISI_D7   (AT91C_PIO_PB27) //  
#define AT91C_PB28_ISI_PCK  (AT91C_PIO_PB28) //  
#define AT91C_PB29_ISI_VSYNC (AT91C_PIO_PB29) //  
#define AT91C_PB3_SPI0_NPCS0 (AT91C_PIO_PB3) //  SPI 0 Peripheral Chip Select 0
#define AT91C_PB30_ISI_HSYNC (AT91C_PIO_PB30) //  
#define AT91C_PB31_         (AT91C_PIO_PB31) //  
#define AT91C_PB31_PCK1     (AT91C_PIO_PB31) //  
#define AT91C_PB4_TXD1     (AT91C_PIO_PB4) //  USART 1 Transmit Data
#define AT91C_PB5_RXD1     (AT91C_PIO_PB5) //  USART 1 Receive Data
#define AT91C_PB6_TXD2     (AT91C_PIO_PB6) //  USART 2 Transmit Data
#define AT91C_PB7_RXD2     (AT91C_PIO_PB7) //  USART 2 Receive Data
#define AT91C_PB8_TXD3     (AT91C_PIO_PB8) //  USART 3 Transmit Data
#define AT91C_PB8_ISI_D8   (AT91C_PIO_PB8) //  
#define AT91C_PB9_RXD3     (AT91C_PIO_PB9) //  USART 3 Receive Data
#define AT91C_PB9_ISI_D9   (AT91C_PIO_PB9) //  
#define AT91C_PC0_DQM2     (AT91C_PIO_PC0) //  DQM2
#define AT91C_PC1_DQM3     (AT91C_PIO_PC1) //  DQM3
#define AT91C_PC10_NCS4_CFCS0 (AT91C_PIO_PC10) //  
#define AT91C_PC10_TCLK2    (AT91C_PIO_PC10) //  
#define AT91C_PC11_NCS5_CFCS1 (AT91C_PIO_PC11) //  
#define AT91C_PC11_CTS2     (AT91C_PIO_PC11) //  
#define AT91C_PC12_A25_CFRNW (AT91C_PIO_PC12) //  
#define AT91C_PC13_NCS2     (AT91C_PIO_PC13) //  
#define AT91C_PC14_NCS3_NANDCS (AT91C_PIO_PC14) //  
#define AT91C_PC15_NWAIT    (AT91C_PIO_PC15) //  
#define AT91C_PC16_D16      (AT91C_PIO_PC16) //  
#define AT91C_PC17_D17      (AT91C_PIO_PC17) //  
#define AT91C_PC18_D18      (AT91C_PIO_PC18) //  
#define AT91C_PC19_D19      (AT91C_PIO_PC19) //  
#define AT91C_PC2_A19      (AT91C_PIO_PC2) //  
#define AT91C_PC20_D20      (AT91C_PIO_PC20) //  
#define AT91C_PC21_D21      (AT91C_PIO_PC21) //  
#define AT91C_PC22_D22      (AT91C_PIO_PC22) //  
#define AT91C_PC23_D23      (AT91C_PIO_PC23) //  
#define AT91C_PC24_D24      (AT91C_PIO_PC24) //  
#define AT91C_PC25_D25      (AT91C_PIO_PC25) //  
#define AT91C_PC26_D26      (AT91C_PIO_PC26) //  
#define AT91C_PC27_D27      (AT91C_PIO_PC27) //  
#define AT91C_PC28_D28      (AT91C_PIO_PC28) //  
#define AT91C_PC29_D29      (AT91C_PIO_PC29) //  
#define AT91C_PC3_A20      (AT91C_PIO_PC3) //  
#define AT91C_PC30_D30      (AT91C_PIO_PC30) //  
#define AT91C_PC31_D31      (AT91C_PIO_PC31) //  
#define AT91C_PC4_A21_NANDALE (AT91C_PIO_PC4) //  
#define AT91C_PC5_A22_NANDCLE (AT91C_PIO_PC5) //  
#define AT91C_PC6_A23      (AT91C_PIO_PC6) //  
#define AT91C_PC7_A24      (AT91C_PIO_PC7) //  
#define AT91C_PC8_CFCE1    (AT91C_PIO_PC8) //  
#define AT91C_PC9_CFCE2    (AT91C_PIO_PC9) //  
#define AT91C_PC9_RTS2     (AT91C_PIO_PC9) //  
#define AT91C_PD0_TK0      (AT91C_PIO_PD0) //  
#define AT91C_PD0_PWM3     (AT91C_PIO_PD0) //  
#define AT91C_PD1_TF0      (AT91C_PIO_PD1) //  
#define AT91C_PD10_TD1      (AT91C_PIO_PD10) //  
#define AT91C_PD11_RD1      (AT91C_PIO_PD11) //  
#define AT91C_PD12_TK1      (AT91C_PIO_PD12) //  
#define AT91C_PD12_PCK0     (AT91C_PIO_PD12) //  
#define AT91C_PD13_RK1      (AT91C_PIO_PD13) //  
#define AT91C_PD14_TF1      (AT91C_PIO_PD14) //  
#define AT91C_PD15_RF1      (AT91C_PIO_PD15) //  
#define AT91C_PD16_RTS1     (AT91C_PIO_PD16) //  
#define AT91C_PD17_CTS1     (AT91C_PIO_PD17) //  
#define AT91C_PD18_SPI1_NPCS2 (AT91C_PIO_PD18) //  
#define AT91C_PD18_IRQ      (AT91C_PIO_PD18) //  
#define AT91C_PD19_SPI1_NPCS3 (AT91C_PIO_PD19) //  
#define AT91C_PD19_FIQ      (AT91C_PIO_PD19) //  
#define AT91C_PD2_TD0      (AT91C_PIO_PD2) //  
#define AT91C_PD20_TIOA0    (AT91C_PIO_PD20) //  
#define AT91C_PD21_TIOA1    (AT91C_PIO_PD21) //  
#define AT91C_PD22_TIOA2    (AT91C_PIO_PD22) //  
#define AT91C_PD23_TCLK0    (AT91C_PIO_PD23) //  
#define AT91C_PD24_SPI0_NPCS1 (AT91C_PIO_PD24) //  
#define AT91C_PD24_PWM0     (AT91C_PIO_PD24) //  
#define AT91C_PD25_SPI0_NPCS2 (AT91C_PIO_PD25) //  
#define AT91C_PD25_PWM1     (AT91C_PIO_PD25) //  
#define AT91C_PD26_PCK0     (AT91C_PIO_PD26) //  
#define AT91C_PD26_PWM2     (AT91C_PIO_PD26) //  
#define AT91C_PD27_PCK1     (AT91C_PIO_PD27) //  
#define AT91C_PD27_SPI0_NPCS3 (AT91C_PIO_PD27) //  
#define AT91C_PD28_TSADTRG  (AT91C_PIO_PD28) //  
#define AT91C_PD28_SPI1_NPCS1 (AT91C_PIO_PD28) //  
#define AT91C_PD29_TCLK1    (AT91C_PIO_PD29) //  
#define AT91C_PD29_SCK1     (AT91C_PIO_PD29) //  
#define AT91C_PD3_RD0      (AT91C_PIO_PD3) //  
#define AT91C_PD30_TIOB0    (AT91C_PIO_PD30) //  
#define AT91C_PD30_SCK2     (AT91C_PIO_PD30) //  
#define AT91C_PD31_TIOB1    (AT91C_PIO_PD31) //  
#define AT91C_PD31_PWM1     (AT91C_PIO_PD31) //  
#define AT91C_PD4_RK0      (AT91C_PIO_PD4) //  
#define AT91C_PD5_RF0      (AT91C_PIO_PD5) //  
#define AT91C_PD6_AC97RX   (AT91C_PIO_PD6) //  
#define AT91C_PD7_AC97TX   (AT91C_PIO_PD7) //  
#define AT91C_PD7_TIOA5    (AT91C_PIO_PD7) //  
#define AT91C_PD8_AC97FS   (AT91C_PIO_PD8) //  
#define AT91C_PD8_TIOB5    (AT91C_PIO_PD8) //  
#define AT91C_PD9_AC97CK   (AT91C_PIO_PD9) //  
#define AT91C_PD9_TCLK5    (AT91C_PIO_PD9) //  
#define AT91C_PE0_LCDPWR   (AT91C_PIO_PE0) //  
#define AT91C_PE0_PCK0     (AT91C_PIO_PE0) //  
#define AT91C_PE1_LCDMOD   (AT91C_PIO_PE1) //  
#define AT91C_PE10_LCDD3    (AT91C_PIO_PE10) //  
#define AT91C_PE10_LCDD5    (AT91C_PIO_PE10) //  
#define AT91C_PE11_LCDD4    (AT91C_PIO_PE11) //  
#define AT91C_PE11_LCDD6    (AT91C_PIO_PE11) //  
#define AT91C_PE12_LCDD5    (AT91C_PIO_PE12) //  
#define AT91C_PE12_LCDD7    (AT91C_PIO_PE12) //  
#define AT91C_PE13_LCDD6    (AT91C_PIO_PE13) //  
#define AT91C_PE13_LCDD10   (AT91C_PIO_PE13) //  
#define AT91C_PE14_LCDD7    (AT91C_PIO_PE14) //  
#define AT91C_PE14_LCDD11   (AT91C_PIO_PE14) //  
#define AT91C_PE15_LCDD8    (AT91C_PIO_PE15) //  
#define AT91C_PE15_LCDD12   (AT91C_PIO_PE15) //  
#define AT91C_PE16_LCDD9    (AT91C_PIO_PE16) //  
#define AT91C_PE16_LCDD13   (AT91C_PIO_PE16) //  
#define AT91C_PE17_LCDD10   (AT91C_PIO_PE17) //  
#define AT91C_PE17_LCDD14   (AT91C_PIO_PE17) //  
#define AT91C_PE18_LCDD11   (AT91C_PIO_PE18) //  
#define AT91C_PE18_LCDD15   (AT91C_PIO_PE18) //  
#define AT91C_PE19_LCDD12   (AT91C_PIO_PE19) //  
#define AT91C_PE19_LCDD18   (AT91C_PIO_PE19) //  
#define AT91C_PE2_LCDCC    (AT91C_PIO_PE2) //  
#define AT91C_PE20_LCDD13   (AT91C_PIO_PE20) //  
#define AT91C_PE20_LCDD19   (AT91C_PIO_PE20) //  
#define AT91C_PE21_LCDD14   (AT91C_PIO_PE21) //  
#define AT91C_PE21_LCDD20   (AT91C_PIO_PE21) //  
#define AT91C_PE22_LCDD15   (AT91C_PIO_PE22) //  
#define AT91C_PE22_LCDD21   (AT91C_PIO_PE22) //  
#define AT91C_PE23_LCDD16   (AT91C_PIO_PE23) //  
#define AT91C_PE23_LCDD22   (AT91C_PIO_PE23) //  
#define AT91C_PE24_LCDD17   (AT91C_PIO_PE24) //  
#define AT91C_PE24_LCDD23   (AT91C_PIO_PE24) //  
#define AT91C_PE25_LCDD18   (AT91C_PIO_PE25) //  
#define AT91C_PE26_LCDD19   (AT91C_PIO_PE26) //  
#define AT91C_PE27_LCDD20   (AT91C_PIO_PE27) //  
#define AT91C_PE28_LCDD21   (AT91C_PIO_PE28) //  
#define AT91C_PE29_LCDD22   (AT91C_PIO_PE29) //  
#define AT91C_PE3_LCDVSYNC (AT91C_PIO_PE3) //  
#define AT91C_PE30_LCDD23   (AT91C_PIO_PE30) //  
#define AT91C_PE31_PWM2     (AT91C_PIO_PE31) //  
#define AT91C_PE31_PCK1     (AT91C_PIO_PE31) //  
#define AT91C_PE4_LCDHSYNC (AT91C_PIO_PE4) //  
#define AT91C_PE5_LCDDOTCK (AT91C_PIO_PE5) //  
#define AT91C_PE6_LCDDEN   (AT91C_PIO_PE6) //  
#define AT91C_PE7_LCDD0    (AT91C_PIO_PE7) //  
#define AT91C_PE7_LCDD2    (AT91C_PIO_PE7) //  
#define AT91C_PE8_LCDD1    (AT91C_PIO_PE8) //  
#define AT91C_PE8_LCDD3    (AT91C_PIO_PE8) //  
#define AT91C_PE9_LCDD2    (AT91C_PIO_PE9) //  
#define AT91C_PE9_LCDD4    (AT91C_PIO_PE9) //  

#endif /* ARM_AT91_AT91_PIO_SAM9G45_H */
