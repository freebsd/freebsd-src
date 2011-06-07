/* $FreeBSD$ */

#ifndef  ARM_AT91_AT91_PIO_RM9200_H
#define  ARM_AT91_AT91_PIO_RM9200_H

#include <arm/at91/at91_pioreg.h>
/*
 * These defines come from an atmel file that says specifically that it
 * has no copyright.
 */

//*****************************************************************************
//               PIO DEFINITIONS FOR AT91RM9200
//*****************************************************************************
#define	AT91C_PA0_MISO     (AT91C_PIO_PA0) //  SPI Master In Slave
#define	AT91C_PA0_PCK3     (AT91C_PIO_PA0) //  PMC Programmable Clock Output 3
#define	AT91C_PA1_MOSI     (AT91C_PIO_PA1) //  SPI Master Out Slave
#define	AT91C_PA1_PCK0     (AT91C_PIO_PA1) //  PMC Programmable Clock Output 0
#define	AT91C_PA10_ETX1     (AT91C_PIO_PA10) //  Ethernet MAC Transmit Data 1
#define	AT91C_PA10_MCDB1    (AT91C_PIO_PA10) //  Multimedia Card B Data 1
#define	AT91C_PA11_ECRS_ECRSDV (AT91C_PIO_PA11) //  Ethernet MAC Carrier Sense/Carrier Sense and Data Valid
#define	AT91C_PA11_MCDB2    (AT91C_PIO_PA11) //  Multimedia Card B Data 2
#define	AT91C_PA12_ERX0     (AT91C_PIO_PA12) //  Ethernet MAC Receive Data 0
#define	AT91C_PA12_MCDB3    (AT91C_PIO_PA12) //  Multimedia Card B Data 3
#define	AT91C_PA13_ERX1     (AT91C_PIO_PA13) //  Ethernet MAC Receive Data 1
#define	AT91C_PA13_TCLK0    (AT91C_PIO_PA13) //  Timer Counter 0 external clock input
#define	AT91C_PA14_ERXER    (AT91C_PIO_PA14) //  Ethernet MAC Receive Error
#define	AT91C_PA14_TCLK1    (AT91C_PIO_PA14) //  Timer Counter 1 external clock input
#define	AT91C_PA15_EMDC     (AT91C_PIO_PA15) //  Ethernet MAC Management Data Clock
#define	AT91C_PA15_TCLK2    (AT91C_PIO_PA15) //  Timer Counter 2 external clock input
#define	AT91C_PA16_EMDIO    (AT91C_PIO_PA16) //  Ethernet MAC Management Data Input/Output
#define	AT91C_PA16_IRQ6     (AT91C_PIO_PA16) //  AIC Interrupt input 6
#define	AT91C_PA17_TXD0     (AT91C_PIO_PA17) //  USART 0 Transmit Data
#define	AT91C_PA17_TIOA0    (AT91C_PIO_PA17) //  Timer Counter 0 Multipurpose Timer I/O Pin A
#define	AT91C_PA18_RXD0     (AT91C_PIO_PA18) //  USART 0 Receive Data
#define	AT91C_PA18_TIOB0    (AT91C_PIO_PA18) //  Timer Counter 0 Multipurpose Timer I/O Pin B
#define	AT91C_PA19_SCK0     (AT91C_PIO_PA19) //  USART 0 Serial Clock
#define	AT91C_PA19_TIOA1    (AT91C_PIO_PA19) //  Timer Counter 1 Multipurpose Timer I/O Pin A
#define	AT91C_PA2_SPCK     (AT91C_PIO_PA2) //  SPI Serial Clock
#define	AT91C_PA2_IRQ4     (AT91C_PIO_PA2) //  AIC Interrupt Input 4
#define	AT91C_PA20_CTS0     (AT91C_PIO_PA20) //  USART 0 Clear To Send
#define	AT91C_PA20_TIOB1    (AT91C_PIO_PA20) //  Timer Counter 1 Multipurpose Timer I/O Pin B
#define	AT91C_PA21_RTS0     (AT91C_PIO_PA21) //  Usart 0 Ready To Send
#define	AT91C_PA21_TIOA2    (AT91C_PIO_PA21) //  Timer Counter 2 Multipurpose Timer I/O Pin A
#define	AT91C_PA22_RXD2     (AT91C_PIO_PA22) //  USART 2 Receive Data
#define	AT91C_PA22_TIOB2    (AT91C_PIO_PA22) //  Timer Counter 2 Multipurpose Timer I/O Pin B
#define	AT91C_PA23_TXD2     (AT91C_PIO_PA23) //  USART 2 Transmit Data
#define	AT91C_PA23_IRQ3     (AT91C_PIO_PA23) //  Interrupt input 3
#define	AT91C_PA24_SCK2     (AT91C_PIO_PA24) //  USART2 Serial Clock
#define	AT91C_PA24_PCK1     (AT91C_PIO_PA24) //  PMC Programmable Clock Output 1
#define	AT91C_PA25_TWD      (AT91C_PIO_PA25) //  TWI Two-wire Serial Data
#define	AT91C_PA25_IRQ2     (AT91C_PIO_PA25) //  Interrupt input 2
#define	AT91C_PA26_TWCK     (AT91C_PIO_PA26) //  TWI Two-wire Serial Clock
#define	AT91C_PA26_IRQ1     (AT91C_PIO_PA26) //  Interrupt input 1
#define	AT91C_PA27_MCCK     (AT91C_PIO_PA27) //  Multimedia Card Clock
#define	AT91C_PA27_TCLK3    (AT91C_PIO_PA27) //  Timer Counter 3 External Clock Input
#define	AT91C_PA28_MCCDA    (AT91C_PIO_PA28) //  Multimedia Card A Command
#define	AT91C_PA28_TCLK4    (AT91C_PIO_PA28) //  Timer Counter 4 external Clock Input
#define	AT91C_PA29_MCDA0    (AT91C_PIO_PA29) //  Multimedia Card A Data 0
#define	AT91C_PA29_TCLK5    (AT91C_PIO_PA29) //  Timer Counter 5 external clock input
#define	AT91C_PA3_NPCS0    (AT91C_PIO_PA3) //  SPI Peripheral Chip Select 0
#define	AT91C_PA3_IRQ5     (AT91C_PIO_PA3) //  AIC Interrupt Input 5
#define	AT91C_PA30_DRXD     (AT91C_PIO_PA30) //  DBGU Debug Receive Data
#define	AT91C_PA30_CTS2     (AT91C_PIO_PA30) //  Usart 2 Clear To Send
#define	AT91C_PA31_DTXD     (AT91C_PIO_PA31) //  DBGU Debug Transmit Data
#define	AT91C_PA31_RTS2     (AT91C_PIO_PA31) //  USART 2 Ready To Send
#define	AT91C_PA4_NPCS1    (AT91C_PIO_PA4) //  SPI Peripheral Chip Select 1
#define	AT91C_PA4_PCK1     (AT91C_PIO_PA4) //  PMC Programmable Clock Output 1
#define	AT91C_PA5_NPCS2    (AT91C_PIO_PA5) //  SPI Peripheral Chip Select 2
#define	AT91C_PA5_TXD3     (AT91C_PIO_PA5) //  USART 3 Transmit Data
#define	AT91C_PA6_NPCS3    (AT91C_PIO_PA6) //  SPI Peripheral Chip Select 3
#define	AT91C_PA6_RXD3     (AT91C_PIO_PA6) //  USART 3 Receive Data
#define	AT91C_PA7_ETXCK_EREFCK (AT91C_PIO_PA7) //  Ethernet MAC Transmit Clock/Reference Clock
#define	AT91C_PA7_PCK2     (AT91C_PIO_PA7) //  PMC Programmable Clock 2
#define	AT91C_PA8_ETXEN    (AT91C_PIO_PA8) //  Ethernet MAC Transmit Enable
#define	AT91C_PA8_MCCDB    (AT91C_PIO_PA8) //  Multimedia Card B Command
#define	AT91C_PA9_ETX0     (AT91C_PIO_PA9) //  Ethernet MAC Transmit Data 0
#define	AT91C_PA9_MCDB0    (AT91C_PIO_PA9) //  Multimedia Card B Data 0
#define	AT91C_PB0_TF0      (AT91C_PIO_PB0) //  SSC Transmit Frame Sync 0
#define	AT91C_PB0_TIOB3    (AT91C_PIO_PB0) //  Timer Counter 3 Multipurpose Timer I/O Pin B
#define	AT91C_PB1_TK0      (AT91C_PIO_PB1) //  SSC Transmit Clock 0
#define	AT91C_PB1_CTS3     (AT91C_PIO_PB1) //  USART 3 Clear To Send
#define	AT91C_PB10_RK1      (AT91C_PIO_PB10) //  SSC Receive Clock 1
#define	AT91C_PB10_TIOA5    (AT91C_PIO_PB10) //  Timer Counter 5 Multipurpose Timer I/O Pin A
#define	AT91C_PB11_RF1      (AT91C_PIO_PB11) //  SSC Receive Frame Sync 1
#define	AT91C_PB11_TIOB5    (AT91C_PIO_PB11) //  Timer Counter 5 Multipurpose Timer I/O Pin B
#define	AT91C_PB12_TF2      (AT91C_PIO_PB12) //  SSC Transmit Frame Sync 2
#define	AT91C_PB12_ETX2     (AT91C_PIO_PB12) //  Ethernet MAC Transmit Data 2
#define	AT91C_PB13_TK2      (AT91C_PIO_PB13) //  SSC Transmit Clock 2
#define	AT91C_PB13_ETX3     (AT91C_PIO_PB13) //  Ethernet MAC Transmit Data 3
#define	AT91C_PB14_TD2      (AT91C_PIO_PB14) //  SSC Transmit Data 2
#define	AT91C_PB14_ETXER    (AT91C_PIO_PB14) //  Ethernet MAC Transmikt Coding Error
#define	AT91C_PB15_RD2      (AT91C_PIO_PB15) //  SSC Receive Data 2
#define	AT91C_PB15_ERX2     (AT91C_PIO_PB15) //  Ethernet MAC Receive Data 2
#define	AT91C_PB16_RK2      (AT91C_PIO_PB16) //  SSC Receive Clock 2
#define	AT91C_PB16_ERX3     (AT91C_PIO_PB16) //  Ethernet MAC Receive Data 3
#define	AT91C_PB17_RF2      (AT91C_PIO_PB17) //  SSC Receive Frame Sync 2
#define	AT91C_PB17_ERXDV    (AT91C_PIO_PB17) //  Ethernet MAC Receive Data Valid
#define	AT91C_PB18_RI1      (AT91C_PIO_PB18) //  USART 1 Ring Indicator
#define	AT91C_PB18_ECOL     (AT91C_PIO_PB18) //  Ethernet MAC Collision Detected
#define	AT91C_PB19_DTR1     (AT91C_PIO_PB19) //  USART 1 Data Terminal ready
#define	AT91C_PB19_ERXCK    (AT91C_PIO_PB19) //  Ethernet MAC Receive Clock
#define	AT91C_PB2_TD0      (AT91C_PIO_PB2) //  SSC Transmit data
#define	AT91C_PB2_SCK3     (AT91C_PIO_PB2) //  USART 3 Serial Clock
#define	AT91C_PB20_TXD1     (AT91C_PIO_PB20) //  USART 1 Transmit Data
#define	AT91C_PB21_RXD1     (AT91C_PIO_PB21) //  USART 1 Receive Data
#define	AT91C_PB22_SCK1     (AT91C_PIO_PB22) //  USART1 Serial Clock
#define	AT91C_PB23_DCD1     (AT91C_PIO_PB23) //  USART 1 Data Carrier Detect
#define	AT91C_PB24_CTS1     (AT91C_PIO_PB24) //  USART 1 Clear To Send
#define	AT91C_PB25_DSR1     (AT91C_PIO_PB25) //  USART 1 Data Set ready
#define	AT91C_PB25_EF100    (AT91C_PIO_PB25) //  Ethernet MAC Force 100 Mbits/sec
#define	AT91C_PB26_RTS1     (AT91C_PIO_PB26) //  Usart 0 Ready To Send
#define	AT91C_PB27_PCK0     (AT91C_PIO_PB27) //  PMC Programmable Clock Output 0
#define	AT91C_PB28_FIQ      (AT91C_PIO_PB28) //  AIC Fast Interrupt Input
#define	AT91C_PB29_IRQ0     (AT91C_PIO_PB29) //  Interrupt input 0
#define	AT91C_PB3_RD0      (AT91C_PIO_PB3) //  SSC Receive Data
#define	AT91C_PB3_MCDA1    (AT91C_PIO_PB3) //  Multimedia Card A Data 1
#define	AT91C_PB4_RK0      (AT91C_PIO_PB4) //  SSC Receive Clock
#define	AT91C_PB4_MCDA2    (AT91C_PIO_PB4) //  Multimedia Card A Data 2
#define	AT91C_PB5_RF0      (AT91C_PIO_PB5) //  SSC Receive Frame Sync 0
#define	AT91C_PB5_MCDA3    (AT91C_PIO_PB5) //  Multimedia Card A Data 3
#define	AT91C_PB6_TF1      (AT91C_PIO_PB6) //  SSC Transmit Frame Sync 1
#define	AT91C_PB6_TIOA3    (AT91C_PIO_PB6) //  Timer Counter 4 Multipurpose Timer I/O Pin A
#define	AT91C_PB7_TK1      (AT91C_PIO_PB7) //  SSC Transmit Clock 1
#define	AT91C_PB7_TIOB3    (AT91C_PIO_PB7) //  Timer Counter 3 Multipurpose Timer I/O Pin B
#define	AT91C_PB8_TD1      (AT91C_PIO_PB8) //  SSC Transmit Data 1
#define	AT91C_PB8_TIOA4    (AT91C_PIO_PB8) //  Timer Counter 4 Multipurpose Timer I/O Pin A
#define	AT91C_PB9_RD1      (AT91C_PIO_PB9) //  SSC Receive Data 1
#define	AT91C_PB9_TIOB4    (AT91C_PIO_PB9) //  Timer Counter 4 Multipurpose Timer I/O Pin B
#define	AT91C_PC0_BFCK     (AT91C_PIO_PC0) //  Burst Flash Clock
#define	AT91C_PC1_BFRDY_SMOE (AT91C_PIO_PC1) //  Burst Flash Ready
#define	AT91C_PC10_NCS4_CFCS (AT91C_PIO_PC10) //  Compact Flash Chip Select
#define	AT91C_PC11_NCS5_CFCE1 (AT91C_PIO_PC11) //  Chip Select 5 / Compact Flash Chip Enable 1
#define	AT91C_PC12_NCS6_CFCE2 (AT91C_PIO_PC12) //  Chip Select 6 / Compact Flash Chip Enable 2
#define	AT91C_PC13_NCS7     (AT91C_PIO_PC13) //  Chip Select 7
#define	AT91C_PC16_D16      (AT91C_PIO_PC16) //  Data Bus [16]
#define	AT91C_PC17_D17      (AT91C_PIO_PC17) //  Data Bus [17]
#define	AT91C_PC18_D18      (AT91C_PIO_PC18) //  Data Bus [18]
#define	AT91C_PC19_D19      (AT91C_PIO_PC19) //  Data Bus [19]
#define	AT91C_PC2_BFAVD    (AT91C_PIO_PC2)u //  Burst Flash Address Valid
#define	AT91C_PC20_D20      (AT91C_PIO_PC20) //  Data Bus [20]
#define	AT91C_PC21_D21      (AT91C_PIO_PC21) //  Data Bus [21]
#define	AT91C_PC22_D22      (AT91C_PIO_PC22) //  Data Bus [22]
#define	AT91C_PC23_D23      (AT91C_PIO_PC23) //  Data Bus [23]
#define	AT91C_PC24_D24      (AT91C_PIO_PC24) //  Data Bus [24]
#define	AT91C_PC25_D25      (AT91C_PIO_PC25) //  Data Bus [25]
#define	AT91C_PC26_D26      (AT91C_PIO_PC26) //  Data Bus [26]
#define	AT91C_PC27_D27      (AT91C_PIO_PC27) //  Data Bus [27]
#define	AT91C_PC28_D28      (AT91C_PIO_PC28) //  Data Bus [28]
#define	AT91C_PC29_D29      (AT91C_PIO_PC29) //  Data Bus [29]
#define	AT91C_PC3_BFBAA_SMWE (AT91C_PIO_PC3) //  Burst Flash Address Advance / SmartMedia Write Enable
#define	AT91C_PC30_D30      (AT91C_PIO_PC30) //  Data Bus [30]
#define	AT91C_PC31_D31      (AT91C_PIO_PC31) //  Data Bus [31]
#define	AT91C_PC4_BFOE     (AT91C_PIO_PC4) //  Burst Flash Output Enable
#define	AT91C_PC5_BFWE     (AT91C_PIO_PC5) //  Burst Flash Write Enable
#define	AT91C_PC6_NWAIT    (AT91C_PIO_PC6) //  NWAIT
#define	AT91C_PC7_A23      (AT91C_PIO_PC7) //  Address Bus[23]
#define	AT91C_PC8_A24      (AT91C_PIO_PC8) //  Address Bus[24]
#define	AT91C_PC9_A25_CFRNW (AT91C_PIO_PC9) //  Address Bus[25] /  Compact Flash Read Not Write
#define	AT91C_PD0_ETX0     (AT91C_PIO_PD0) //  Ethernet MAC Transmit Data 0
#define	AT91C_PD1_ETX1     (AT91C_PIO_PD1) //  Ethernet MAC Transmit Data 1
#define	AT91C_PD10_PCK3     (AT91C_PIO_PD10) //  PMC Programmable Clock Output 3
#define	AT91C_PD10_TPS1     (AT91C_PIO_PD10) //  ETM ARM9 pipeline status 1
#define	AT91C_PD11_         (AT91C_PIO_PD11) //  
#define	AT91C_PD11_TPS2     (AT91C_PIO_PD11) //  ETM ARM9 pipeline status 2
#define	AT91C_PD12_         (AT91C_PIO_PD12) //  
#define	AT91C_PD12_TPK0     (AT91C_PIO_PD12) //  ETM Trace Packet 0
#define	AT91C_PD13_         (AT91C_PIO_PD13) //  
#define	AT91C_PD13_TPK1     (AT91C_PIO_PD13) //  ETM Trace Packet 1
#define	AT91C_PD14_         (AT91C_PIO_PD14) //  
#define	AT91C_PD14_TPK2     (AT91C_PIO_PD14) //  ETM Trace Packet 2
#define	AT91C_PD15_TD0      (AT91C_PIO_PD15) //  SSC Transmit data
#define	AT91C_PD15_TPK3     (AT91C_PIO_PD15) //  ETM Trace Packet 3
#define	AT91C_PD16_TD1      (AT91C_PIO_PD16) //  SSC Transmit Data 1
#define	AT91C_PD16_TPK4     (AT91C_PIO_PD16) //  ETM Trace Packet 4
#define	AT91C_PD17_TD2      (AT91C_PIO_PD17) //  SSC Transmit Data 2
#define	AT91C_PD17_TPK5     (AT91C_PIO_PD17) //  ETM Trace Packet 5
#define	AT91C_PD18_NPCS1    (AT91C_PIO_PD18) //  SPI Peripheral Chip Select 1
#define	AT91C_PD18_TPK6     (AT91C_PIO_PD18) //  ETM Trace Packet 6
#define	AT91C_PD19_NPCS2    (AT91C_PIO_PD19) //  SPI Peripheral Chip Select 2
#define	AT91C_PD19_TPK7     (AT91C_PIO_PD19) //  ETM Trace Packet 7
#define	AT91C_PD2_ETX2     (AT91C_PIO_PD2) //  Ethernet MAC Transmit Data 2
#define	AT91C_PD20_NPCS3    (AT91C_PIO_PD20) //  SPI Peripheral Chip Select 3
#define	AT91C_PD20_TPK8     (AT91C_PIO_PD20) //  ETM Trace Packet 8
#define	AT91C_PD21_RTS0     (AT91C_PIO_PD21) //  Usart 0 Ready To Send
#define	AT91C_PD21_TPK9     (AT91C_PIO_PD21) //  ETM Trace Packet 9
#define	AT91C_PD22_RTS1     (AT91C_PIO_PD22) //  Usart 0 Ready To Send
#define	AT91C_PD22_TPK10    (AT91C_PIO_PD22) //  ETM Trace Packet 10
#define	AT91C_PD23_RTS2     (AT91C_PIO_PD23) //  USART 2 Ready To Send
#define	AT91C_PD23_TPK11    (AT91C_PIO_PD23) //  ETM Trace Packet 11
#define	AT91C_PD24_RTS3     (AT91C_PIO_PD24) //  USART 3 Ready To Send
#define	AT91C_PD24_TPK12    (AT91C_PIO_PD24) //  ETM Trace Packet 12
#define	AT91C_PD25_DTR1     (AT91C_PIO_PD25) //  USART 1 Data Terminal ready
#define	AT91C_PD25_TPK13    (AT91C_PIO_PD25) //  ETM Trace Packet 13
#define	AT91C_PD26_TPK14    (AT91C_PIO_PD26) //  ETM Trace Packet 14
#define	AT91C_PD27_TPK15    (AT91C_PIO_PD27) //  ETM Trace Packet 15
#define	AT91C_PD3_ETX3     (AT91C_PIO_PD3) //  Ethernet MAC Transmit Data 3
#define	AT91C_PD4_ETXEN    (AT91C_PIO_PD4) //  Ethernet MAC Transmit Enable
#define	AT91C_PD5_ETXER    (AT91C_PIO_PD5) //  Ethernet MAC Transmikt Coding Error
#define	AT91C_PD6_DTXD     (AT91C_PIO_PD6) //  DBGU Debug Transmit Data
#define	AT91C_PD7_PCK0     (AT91C_PIO_PD7) //  PMC Programmable Clock Output 0
#define	AT91C_PD7_TSYNC    (AT91C_PIO_PD7) //  ETM Synchronization signal
#define	AT91C_PD8_PCK1     (AT91C_PIO_PD8) //  PMC Programmable Clock Output 1
#define	AT91C_PD8_TCLK     (AT91C_PIO_PD8) //  ETM Trace Clock signal
#define	AT91C_PD9_PCK2     (AT91C_PIO_PD9) //  PMC Programmable Clock 2
#define	AT91C_PD9_TPS0     (AT91C_PIO_PD9) //  ETM ARM9 pipeline status 0

#endif /* ARM_AT91_AT91_PIO_RM9200_H */
