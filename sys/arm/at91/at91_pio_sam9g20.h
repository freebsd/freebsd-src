/*
 * Theses defines come from an atmel file that says specifically that it
 * has no copyright.
 *
 * These defines are also usable for the AT91SAM9260 which has pin multiplexing
 * that is identical to the AT91SAM9G20.
 */

/* $FreeBSD$ */

#ifndef  ARM_AT91_AT91_PIO_SAM9G20_H
#define  ARM_AT91_AT91_PIO_SAM9G20_H

#include <arm/at91/at91_pioreg.h>


// *****************************************************************************
//               PIO DEFINITIONS FOR AT91SAM9G20
// *****************************************************************************
#define AT91C_PA0_SPI0_MISO (AT91C_PIO_PA0)  //  SPI 0 Master In Slave
#define AT91C_PA0_MCDB0     (AT91C_PIO_PA0)  //  Multimedia Card B Data 0
#define AT91C_PA1_SPI0_MOSI (AT91C_PIO_PA1)  //  SPI 0 Master Out Slave
#define AT91C_PA1_MCCDB     (AT91C_PIO_PA1)  //  Multimedia Card B Command
#define AT91C_PA10_MCDA2    (AT91C_PIO_PA10) //  Multimedia Card A Data 2
#define AT91C_PA10_ETX2_0   (AT91C_PIO_PA10) //  Ethernet MAC Transmit Data 2
#define AT91C_PA11_MCDA3    (AT91C_PIO_PA11) //  Multimedia Card A Data 3
#define AT91C_PA11_ETX3_0   (AT91C_PIO_PA11) //  Ethernet MAC Transmit Data 3
#define AT91C_PA12_ETX0     (AT91C_PIO_PA12) //  Ethernet MAC Transmit Data 0
#define AT91C_PA13_ETX1     (AT91C_PIO_PA13) //  Ethernet MAC Transmit Data 1
#define AT91C_PA14_ERX0     (AT91C_PIO_PA14) //  Ethernet MAC Receive Data 0
#define AT91C_PA15_ERX1     (AT91C_PIO_PA15) //  Ethernet MAC Receive Data 1
#define AT91C_PA16_ETXEN    (AT91C_PIO_PA16) //  Ethernet MAC Transmit Enable
#define AT91C_PA17_ERXDV    (AT91C_PIO_PA17) //  Ethernet MAC Receive Data Valid
#define AT91C_PA18_ERXER    (AT91C_PIO_PA18) //  Ethernet MAC Receive Error
#define AT91C_PA19_ETXCK    (AT91C_PIO_PA19) //  Ethernet MAC Transmit Clock/Reference Clock
#define AT91C_PA2_SPI0_SPCK (AT91C_PIO_PA2)  //  SPI 0 Serial Clock
#define AT91C_PA20_EMDC     (AT91C_PIO_PA20) //  Ethernet MAC Management Data Clock
#define AT91C_PA21_EMDIO    (AT91C_PIO_PA21) //  Ethernet MAC Management Data Input/Output
#define AT91C_PA22_ADTRG    (AT91C_PIO_PA22) //  ADC Trigger
#define AT91C_PA22_ETXER    (AT91C_PIO_PA22) //  Ethernet MAC Transmikt Coding Error
#define AT91C_PA23_TWD      (AT91C_PIO_PA23) //  TWI Two-wire Serial Data
#define AT91C_PA23_ETX2_1   (AT91C_PIO_PA23) //  Ethernet MAC Transmit Data 2
#define AT91C_PA24_TWCK     (AT91C_PIO_PA24) //  TWI Two-wire Serial Clock
#define AT91C_PA24_ETX3_1   (AT91C_PIO_PA24) //  Ethernet MAC Transmit Data 3
#define AT91C_PA25_TCLK0    (AT91C_PIO_PA25) //  Timer Counter 0 external clock input
#define AT91C_PA25_ERX2     (AT91C_PIO_PA25) //  Ethernet MAC Receive Data 2
#define AT91C_PA26_TIOA0    (AT91C_PIO_PA26) //  Timer Counter 0 Multipurpose Timer I/O Pin A
#define AT91C_PA26_ERX3     (AT91C_PIO_PA26) //  Ethernet MAC Receive Data 3
#define AT91C_PA27_TIOA1    (AT91C_PIO_PA27) //  Timer Counter 1 Multipurpose Timer I/O Pin A
#define AT91C_PA27_ERXCK    (AT91C_PIO_PA27) //  Ethernet MAC Receive Clock
#define AT91C_PA28_TIOA2    (AT91C_PIO_PA28) //  Timer Counter 2 Multipurpose Timer I/O Pin A
#define AT91C_PA28_ECRS     (AT91C_PIO_PA28) //  Ethernet MAC Carrier Sense/Carrier Sense and Data Valid
#define AT91C_PA29_SCK1     (AT91C_PIO_PA29) //  USART 1 Serial Clock
#define AT91C_PA29_ECOL     (AT91C_PIO_PA29) //  Ethernet MAC Collision Detected
#define AT91C_PA3_SPI0_NPCS0 (AT91C_PIO_PA3) //  SPI 0 Peripheral Chip Select 0
#define AT91C_PA3_MCDB3     (AT91C_PIO_PA3)  //  Multimedia Card B Data 3
#define AT91C_PA30_SCK2     (AT91C_PIO_PA30) //  USART 2 Serial Clock
#define AT91C_PA30_RXD4     (AT91C_PIO_PA30) //  USART 4 Receive Data
#define AT91C_PA31_SCK0     (AT91C_PIO_PA31) //  USART 0 Serial Clock
#define AT91C_PA31_TXD4     (AT91C_PIO_PA31) //  USART 4 Transmit Data
#define AT91C_PA4_RTS2      (AT91C_PIO_PA4)  //  USART 2 Ready To Send
#define AT91C_PA4_MCDB2     (AT91C_PIO_PA4)  //  Multimedia Card B Data 2
#define AT91C_PA5_CTS2      (AT91C_PIO_PA5)  //  USART 2 Clear To Send
#define AT91C_PA5_MCDB1     (AT91C_PIO_PA5)  //  Multimedia Card B Data 1
#define AT91C_PA6_MCDA0     (AT91C_PIO_PA6)  //  Multimedia Card A Data 0
#define AT91C_PA7_MCCDA     (AT91C_PIO_PA7)  //  Multimedia Card A Command
#define AT91C_PA8_MCCK      (AT91C_PIO_PA8)  //  Multimedia Card Clock
#define AT91C_PA9_MCDA1     (AT91C_PIO_PA9)  //  Multimedia Card A Data 1
#define AT91C_PB0_SPI1_MISO (AT91C_PIO_PB0)  //  SPI 1 Master In Slave
#define AT91C_PB0_TIOA3     (AT91C_PIO_PB0)  //  Timer Counter 3 Multipurpose Timer I/O Pin A
#define AT91C_PB1_SPI1_MOSI (AT91C_PIO_PB1)  //  SPI 1 Master Out Slave
#define AT91C_PB1_TIOB3     (AT91C_PIO_PB1)  //  Timer Counter 3 Multipurpose Timer I/O Pin B
#define AT91C_PB10_TXD3     (AT91C_PIO_PB10) //  USART 3 Transmit Data
#define AT91C_PB10_ISI_D8   (AT91C_PIO_PB10) //  Image Sensor Data 8
#define AT91C_PB11_RXD3     (AT91C_PIO_PB11) //  USART 3 Receive Data
#define AT91C_PB11_ISI_D9   (AT91C_PIO_PB11) //  Image Sensor Data 9
#define AT91C_PB12_TXD5     (AT91C_PIO_PB12) //  USART 5 Transmit Data
#define AT91C_PB12_ISI_D10  (AT91C_PIO_PB12) //  Image Sensor Data 10
#define AT91C_PB13_RXD5     (AT91C_PIO_PB13) //  USART 5 Receive Data
#define AT91C_PB13_ISI_D11  (AT91C_PIO_PB13) //  Image Sensor Data 11
#define AT91C_PB14_DRXD     (AT91C_PIO_PB14) //  DBGU Debug Receive Data
#define AT91C_PB15_DTXD     (AT91C_PIO_PB15) //  DBGU Debug Transmit Data
#define AT91C_PB16_TK0      (AT91C_PIO_PB16) //  SSC0 Transmit Clock
#define AT91C_PB16_TCLK3    (AT91C_PIO_PB16) //  Timer Counter 3 external clock input
#define AT91C_PB17_TF0      (AT91C_PIO_PB17) //  SSC0 Transmit Frame Sync
#define AT91C_PB17_TCLK4    (AT91C_PIO_PB17) //  Timer Counter 4 external clock input
#define AT91C_PB18_TD0      (AT91C_PIO_PB18) //  SSC0 Transmit data
#define AT91C_PB18_TIOB4    (AT91C_PIO_PB18) //  Timer Counter 4 Multipurpose Timer I/O Pin B
#define AT91C_PB19_RD0      (AT91C_PIO_PB19) //  SSC0 Receive Data
#define AT91C_PB19_TIOB5    (AT91C_PIO_PB19) //  Timer Counter 5 Multipurpose Timer I/O Pin B
#define AT91C_PB2_SPI1_SPCK (AT91C_PIO_PB2)  //  SPI 1 Serial Clock
#define AT91C_PB2_TIOA4     (AT91C_PIO_PB2)  //  Timer Counter 4 Multipurpose Timer I/O Pin A
#define AT91C_PB20_RK0      (AT91C_PIO_PB20) //  SSC0 Receive Clock
#define AT91C_PB20_ISI_D0   (AT91C_PIO_PB20) //  Image Sensor Data 0
#define AT91C_PB21_RF0      (AT91C_PIO_PB21) //  SSC0 Receive Frame Sync
#define AT91C_PB21_ISI_D1   (AT91C_PIO_PB21) //  Image Sensor Data 1
#define AT91C_PB22_DSR0     (AT91C_PIO_PB22) //  USART 0 Data Set ready
#define AT91C_PB22_ISI_D2   (AT91C_PIO_PB22) //  Image Sensor Data 2
#define AT91C_PB23_DCD0     (AT91C_PIO_PB23) //  USART 0 Data Carrier Detect
#define AT91C_PB23_ISI_D3   (AT91C_PIO_PB23) //  Image Sensor Data 3
#define AT91C_PB24_DTR0     (AT91C_PIO_PB24) //  USART 0 Data Terminal ready
#define AT91C_PB24_ISI_D4   (AT91C_PIO_PB24) //  Image Sensor Data 4
#define AT91C_PB25_RI0      (AT91C_PIO_PB25) //  USART 0 Ring Indicator
#define AT91C_PB25_ISI_D5   (AT91C_PIO_PB25) //  Image Sensor Data 5
#define AT91C_PB26_RTS0     (AT91C_PIO_PB26) //  USART 0 Ready To Send
#define AT91C_PB26_ISI_D6   (AT91C_PIO_PB26) //  Image Sensor Data 6
#define AT91C_PB27_CTS0     (AT91C_PIO_PB27) //  USART 0 Clear To Send
#define AT91C_PB27_ISI_D7   (AT91C_PIO_PB27) //  Image Sensor Data 7
#define AT91C_PB28_RTS1     (AT91C_PIO_PB28) //  USART 1 Ready To Send
#define AT91C_PB28_ISI_PCK  (AT91C_PIO_PB28) //  Image Sensor Data Clock
#define AT91C_PB29_CTS1     (AT91C_PIO_PB29) //  USART 1 Clear To Send
#define AT91C_PB29_ISI_VSYNC (AT91C_PIO_PB29) //  Image Sensor Vertical Synchro
#define AT91C_PB3_SPI1_NPCS0 (AT91C_PIO_PB3) //  SPI 1 Peripheral Chip Select 0
#define AT91C_PB3_TIOA5     (AT91C_PIO_PB3)  //  Timer Counter 5 Multipurpose Timer I/O Pin A
#define AT91C_PB30_PCK0_0   (AT91C_PIO_PB30) //  PMC Programmable Clock Output 0
#define AT91C_PB30_ISI_HSYNC (AT91C_PIO_PB30) //  Image Sensor Horizontal Synchro
#define AT91C_PB31_PCK1_0   (AT91C_PIO_PB31) //  PMC Programmable Clock Output 1
#define AT91C_PB31_ISI_MCK  (AT91C_PIO_PB31) //  Image Sensor Reference Clock
#define AT91C_PB4_TXD0      (AT91C_PIO_PB4) //  USART 0 Transmit Data
#define AT91C_PB5_RXD0      (AT91C_PIO_PB5) //  USART 0 Receive Data
#define AT91C_PB6_TXD1      (AT91C_PIO_PB6) //  USART 1 Transmit Data
#define AT91C_PB6_TCLK1     (AT91C_PIO_PB6) //  Timer Counter 1 external clock input
#define AT91C_PB7_RXD1      (AT91C_PIO_PB7) //  USART 1 Receive Data
#define AT91C_PB7_TCLK2     (AT91C_PIO_PB7) //  Timer Counter 2 external clock input
#define AT91C_PB8_TXD2      (AT91C_PIO_PB8) //  USART 2 Transmit Data
#define AT91C_PB9_RXD2      (AT91C_PIO_PB9) //  USART 2 Receive Data
#define AT91C_PC0_AD0       (AT91C_PIO_PC0) //  ADC Analog Input 0
#define AT91C_PC0_SCK3      (AT91C_PIO_PC0) //  USART 3 Serial Clock
#define AT91C_PC1_AD1       (AT91C_PIO_PC1) //  ADC Analog Input 1
#define AT91C_PC1_PCK0_1    (AT91C_PIO_PC1) //  PMC Programmable Clock Output 0
#define AT91C_PC10_A25_CFR NW (AT91C_PIO_PC10) //  Address Bus[25]
#define AT91C_PC10_CTS3     (AT91C_PIO_PC10) //  USART 3 Clear To Send
#define AT91C_PC11_NCS2     (AT91C_PIO_PC11) //  Chip Select Line 2
#define AT91C_PC11_SPI0_NPCS1 (AT91C_PIO_PC11) //  SPI 0 Peripheral Chip Select 1
#define AT91C_PC12_IRQ0     (AT91C_PIO_PC12) //  External Interrupt 0
#define AT91C_PC12_NCS7     (AT91C_PIO_PC12) //  Chip Select Line 7
#define AT91C_PC13_FIQ      (AT91C_PIO_PC13) //  AIC Fast Interrupt Input
#define AT91C_PC13_NCS6     (AT91C_PIO_PC13) //  Chip Select Line 6
#define AT91C_PC14_NCS3_NANDCS (AT91C_PIO_PC14) //  Chip Select Line 3
#define AT91C_PC14_IRQ2     (AT91C_PIO_PC14) //  External Interrupt 2
#define AT91C_PC15_NWAIT    (AT91C_PIO_PC15) //  External Wait Signal
#define AT91C_PC15_IRQ1     (AT91C_PIO_PC15) //  External Interrupt 1
#define AT91C_PC16_D16      (AT91C_PIO_PC16) //  Data Bus[16]
#define AT91C_PC16_SPI0_NPCS2 (AT91C_PIO_PC16) //  SPI 0 Peripheral Chip Select 2
#define AT91C_PC17_D17      (AT91C_PIO_PC17)  //  Data Bus[17]
#define AT91C_PC17_SPI0_NPCS3 (AT91C_PIO_PC17) //  SPI 0 Peripheral Chip Select 3
#define AT91C_PC18_D18      (AT91C_PIO_PC18)  //  Data Bus[18]
#define AT91C_PC18_SPI1_NPCS1_1 (AT91C_PIO_PC18) //  SPI 1 Peripheral Chip Select 1
#define AT91C_PC19_D19      (AT91C_PIO_PC19) //  Data Bus[19]
#define AT91C_PC19_SPI1_NPCS2_1 (AT91C_PIO_PC19) //  SPI 1 Peripheral Chip Select 2
#define AT91C_PC2_AD2       (AT91C_PIO_PC2)  //  ADC Analog Input 2
#define AT91C_PC2_PCK1_1    (AT91C_PIO_PC2)  //  PMC Programmable Clock Output 1
#define AT91C_PC20_D20      (AT91C_PIO_PC20) //  Data Bus[20]
#define AT91C_PC20_SPI1_NPCS3_1 (AT91C_PIO_PC20) //  SPI 1 Peripheral Chip Select 3
#define AT91C_PC21_D21      (AT91C_PIO_PC21) //  Data Bus[21]
#define AT91C_PC21_EF100    (AT91C_PIO_PC21) //  Ethernet MAC Force 100 Mbits/sec
#define AT91C_PC22_D22      (AT91C_PIO_PC22) //  Data Bus[22]
#define AT91C_PC22_TCLK5    (AT91C_PIO_PC22) //  Timer Counter 5 external clock input
#define AT91C_PC23_D23      (AT91C_PIO_PC23) //  Data Bus[23]
#define AT91C_PC24_D24      (AT91C_PIO_PC24) //  Data Bus[24]
#define AT91C_PC25_D25      (AT91C_PIO_PC25) //  Data Bus[25]
#define AT91C_PC26_D26      (AT91C_PIO_PC26) //  Data Bus[26]
#define AT91C_PC27_D27      (AT91C_PIO_PC27) //  Data Bus[27]
#define AT91C_PC28_D28      (AT91C_PIO_PC28) //  Data Bus[28]
#define AT91C_PC29_D29      (AT91C_PIO_PC29) //  Data Bus[29]
#define AT91C_PC3_AD3       (AT91C_PIO_PC3)  //  ADC Analog Input 3
#define AT91C_PC3_SPI1_NPCS3_0 (AT91C_PIO_PC3) //  SPI 1 Peripheral Chip Select 3
#define AT91C_PC30_D30      (AT91C_PIO_PC30) //  Data Bus[30]
#define AT91C_PC31_D31      (AT91C_PIO_PC31) //  Data Bus[31]
#define AT91C_PC4_A23       (AT91C_PIO_PC4)  //  Address Bus[23]
#define AT91C_PC4_SPI1_NPCS2_0 (AT91C_PIO_PC4) //  SPI 1 Peripheral Chip Select 2
#define AT91C_PC5_A24       (AT91C_PIO_PC5)  //  Address Bus[24]
#define AT91C_PC5_SPI1_NPCS1_0 (AT91C_PIO_PC5) //  SPI 1 Peripheral Chip Select 1
#define AT91C_PC6_TIOB2     (AT91C_PIO_PC6)  //  Timer Counter 2 Multipurpose Timer I/O Pin B
#define AT91C_PC6_CFCE1     (AT91C_PIO_PC6)  //  Compact Flash Enable 1
#define AT91C_PC7_TIOB1     (AT91C_PIO_PC7)  //  Timer Counter 1 Multipurpose Timer I/O Pin B
#define AT91C_PC7_CFCE2     (AT91C_PIO_PC7)  //  Compact Flash Enable 2
#define AT91C_PC8_NCS4_CFCS0 (AT91C_PIO_PC8) //  Chip Select Line 4
#define AT91C_PC8_RTS3      (AT91C_PIO_PC8)  //  USART 3 Ready To Send
#define AT91C_PC9_NCS5_CFCS1 (AT91C_PIO_PC9) //  Chip Select Line 5
#define AT91C_PC9_TIOB0     (AT91C_PIO_PC9)  //  Timer Counter 0 Multipurpose Timer I/O Pin B

#endif /* ARM_AT91_AT91_PIO_SAM9G20_H */
