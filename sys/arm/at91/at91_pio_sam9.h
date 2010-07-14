/*
 * Theses defines come from an atmel file that says specifically that it
 * has no copyright.
 */

/* $FreeBSD$ */

// *****************************************************************************
//               PIO DEFINITIONS FOR AT91SAM9261
// *****************************************************************************
#define AT91C_PIO_PA0        ((unsigned int) 1 <<  0) // Pin Controlled by PA0
#define AT91C_PA0_MISO0    ((unsigned int) AT91C_PIO_PA0) //  SPI0 Master In Slave
#define AT91C_PA0_MCDA0    ((unsigned int) AT91C_PIO_PA0) //  Multimedia Card A Data 0
#define AT91C_PIO_PA1        ((unsigned int) 1 <<  1) // Pin Controlled by PA1
#define AT91C_PA1_MOSI0    ((unsigned int) AT91C_PIO_PA1) //  SPI0 Master Out Slave
#define AT91C_PA1_MCCDA    ((unsigned int) AT91C_PIO_PA1) //  Multimedia Card A Command
#define AT91C_PIO_PA10       ((unsigned int) 1 << 10) // Pin Controlled by PA10
#define AT91C_PA10_DTXD     ((unsigned int) AT91C_PIO_PA10) //  DBGU Debug Transmit Data
#define AT91C_PA10_PCK3     ((unsigned int) AT91C_PIO_PA10) //  PMC Programmable clock Output 3
#define AT91C_PIO_PA11       ((unsigned int) 1 << 11) // Pin Controlled by PA11
#define AT91C_PA11_TSYNC    ((unsigned int) AT91C_PIO_PA11) //  Trace Synchronization Signal
#define AT91C_PA11_SCK1     ((unsigned int) AT91C_PIO_PA11) //  USART1 Serial Clock
#define AT91C_PIO_PA12       ((unsigned int) 1 << 12) // Pin Controlled by PA12
#define AT91C_PA12_TCLK     ((unsigned int) AT91C_PIO_PA12) //  Trace Clock
#define AT91C_PA12_RTS1     ((unsigned int) AT91C_PIO_PA12) //  USART1 Ready To Send
#define AT91C_PIO_PA13       ((unsigned int) 1 << 13) // Pin Controlled by PA13
#define AT91C_PA13_TPS0     ((unsigned int) AT91C_PIO_PA13) //  Trace ARM Pipeline Status 0
#define AT91C_PA13_CTS1     ((unsigned int) AT91C_PIO_PA13) //  USART1 Clear To Send
#define AT91C_PIO_PA14       ((unsigned int) 1 << 14) // Pin Controlled by PA14
#define AT91C_PA14_TPS1     ((unsigned int) AT91C_PIO_PA14) //  Trace ARM Pipeline Status 1
#define AT91C_PA14_SCK2     ((unsigned int) AT91C_PIO_PA14) //  USART2 Serial Clock
#define AT91C_PIO_PA15       ((unsigned int) 1 << 15) // Pin Controlled by PA15
#define AT91C_PA15_TPS2     ((unsigned int) AT91C_PIO_PA15) //  Trace ARM Pipeline Status 2
#define AT91C_PA15_RTS2     ((unsigned int) AT91C_PIO_PA15) //  USART2 Ready To Send
#define AT91C_PIO_PA16       ((unsigned int) 1 << 16) // Pin Controlled by PA16
#define AT91C_PA16_TPK0     ((unsigned int) AT91C_PIO_PA16) //  Trace Packet Port 0
#define AT91C_PA16_CTS2     ((unsigned int) AT91C_PIO_PA16) //  USART2 Clear To Send
#define AT91C_PIO_PA17       ((unsigned int) 1 << 17) // Pin Controlled by PA17
#define AT91C_PA17_TPK1     ((unsigned int) AT91C_PIO_PA17) //  Trace Packet Port 1
#define AT91C_PA17_TF1      ((unsigned int) AT91C_PIO_PA17) //  SSC1 Transmit Frame Sync
#define AT91C_PIO_PA18       ((unsigned int) 1 << 18) // Pin Controlled by PA18
#define AT91C_PA18_TPK2     ((unsigned int) AT91C_PIO_PA18) //  Trace Packet Port 2
#define AT91C_PA18_TK1      ((unsigned int) AT91C_PIO_PA18) //  SSC1 Transmit Clock
#define AT91C_PIO_PA19       ((unsigned int) 1 << 19) // Pin Controlled by PA19
#define AT91C_PA19_TPK3     ((unsigned int) AT91C_PIO_PA19) //  Trace Packet Port 3
#define AT91C_PA19_TD1      ((unsigned int) AT91C_PIO_PA19) //  SSC1 Transmit Data
#define AT91C_PIO_PA2        ((unsigned int) 1 <<  2) // Pin Controlled by PA2
#define AT91C_PA2_SPCK0    ((unsigned int) AT91C_PIO_PA2) //  SPI0 Serial Clock
#define AT91C_PA2_MCCK     ((unsigned int) AT91C_PIO_PA2) //  Multimedia Card Clock
#define AT91C_PIO_PA20       ((unsigned int) 1 << 20) // Pin Controlled by PA20
#define AT91C_PA20_TPK4     ((unsigned int) AT91C_PIO_PA20) //  Trace Packet Port 4
#define AT91C_PA20_RD1      ((unsigned int) AT91C_PIO_PA20) //  SSC1 Receive Data
#define AT91C_PIO_PA21       ((unsigned int) 1 << 21) // Pin Controlled by PA21
#define AT91C_PA21_TPK5     ((unsigned int) AT91C_PIO_PA21) //  Trace Packet Port 5
#define AT91C_PA21_RK1      ((unsigned int) AT91C_PIO_PA21) //  SSC1 Receive Clock
#define AT91C_PIO_PA22       ((unsigned int) 1 << 22) // Pin Controlled by PA22
#define AT91C_PA22_TPK6     ((unsigned int) AT91C_PIO_PA22) //  Trace Packet Port 6
#define AT91C_PA22_RF1      ((unsigned int) AT91C_PIO_PA22) //  SSC1 Receive Frame Sync
#define AT91C_PIO_PA23       ((unsigned int) 1 << 23) // Pin Controlled by PA23
#define AT91C_PA23_TPK7     ((unsigned int) AT91C_PIO_PA23) //  Trace Packet Port 7
#define AT91C_PA23_RTS0     ((unsigned int) AT91C_PIO_PA23) //  USART0 Ready To Send
#define AT91C_PIO_PA24       ((unsigned int) 1 << 24) // Pin Controlled by PA24
#define AT91C_PA24_TPK8     ((unsigned int) AT91C_PIO_PA24) //  Trace Packet Port 8
#define AT91C_PA24_NPCS11   ((unsigned int) AT91C_PIO_PA24) //  SPI1 Peripheral Chip Select 1
#define AT91C_PIO_PA25       ((unsigned int) 1 << 25) // Pin Controlled by PA25
#define AT91C_PA25_TPK9     ((unsigned int) AT91C_PIO_PA25) //  Trace Packet Port 9
#define AT91C_PA25_NPCS12   ((unsigned int) AT91C_PIO_PA25) //  SPI1 Peripheral Chip Select 2
#define AT91C_PIO_PA26       ((unsigned int) 1 << 26) // Pin Controlled by PA26
#define AT91C_PA26_TPK10    ((unsigned int) AT91C_PIO_PA26) //  Trace Packet Port 10
#define AT91C_PA26_NPCS13   ((unsigned int) AT91C_PIO_PA26) //  SPI1 Peripheral Chip Select 3
#define AT91C_PIO_PA27       ((unsigned int) 1 << 27) // Pin Controlled by PA27
#define AT91C_PA27_TPK11    ((unsigned int) AT91C_PIO_PA27) //  Trace Packet Port 11
#define AT91C_PA27_NPCS01   ((unsigned int) AT91C_PIO_PA27) //  SPI0 Peripheral Chip Select 1
#define AT91C_PIO_PA28       ((unsigned int) 1 << 28) // Pin Controlled by PA28
#define AT91C_PA28_TPK12    ((unsigned int) AT91C_PIO_PA28) //  Trace Packet Port 12
#define AT91C_PA28_NPCS02   ((unsigned int) AT91C_PIO_PA28) //  SPI0 Peripheral Chip Select 2
#define AT91C_PIO_PA29       ((unsigned int) 1 << 29) // Pin Controlled by PA29
#define AT91C_PA29_TPK13    ((unsigned int) AT91C_PIO_PA29) //  Trace Packet Port 13
#define AT91C_PA29_NPCS03   ((unsigned int) AT91C_PIO_PA29) //  SPI0 Peripheral Chip Select 3
#define AT91C_PIO_PA3        ((unsigned int) 1 <<  3) // Pin Controlled by PA3
#define AT91C_PA3_NPCS00   ((unsigned int) AT91C_PIO_PA3) //  SPI0 Peripheral Chip Select 0
#define AT91C_PIO_PA30       ((unsigned int) 1 << 30) // Pin Controlled by PA30
#define AT91C_PA30_TPK14    ((unsigned int) AT91C_PIO_PA30) //  Trace Packet Port 14
#define AT91C_PA30_A23      ((unsigned int) AT91C_PIO_PA30) //  Address Bus bit 23
#define AT91C_PIO_PA31       ((unsigned int) 1 << 31) // Pin Controlled by PA31
#define AT91C_PA31_TPK15    ((unsigned int) AT91C_PIO_PA31) //  Trace Packet Port 15
#define AT91C_PA31_A24      ((unsigned int) AT91C_PIO_PA31) //  Address Bus bit 24
#define AT91C_PIO_PA4        ((unsigned int) 1 <<  4) // Pin Controlled by PA4
#define AT91C_PA4_NPCS01   ((unsigned int) AT91C_PIO_PA4) //  SPI0 Peripheral Chip Select 1
#define AT91C_PA4_MCDA1    ((unsigned int) AT91C_PIO_PA4) //  Multimedia Card A Data 1
#define AT91C_PIO_PA5        ((unsigned int) 1 <<  5) // Pin Controlled by PA5
#define AT91C_PA5_NPCS02   ((unsigned int) AT91C_PIO_PA5) //  SPI0 Peripheral Chip Select 2
#define AT91C_PA5_MCDA2    ((unsigned int) AT91C_PIO_PA5) //  Multimedia Card A Data 2
#define AT91C_PIO_PA6        ((unsigned int) 1 <<  6) // Pin Controlled by PA6
#define AT91C_PA6_NPCS03   ((unsigned int) AT91C_PIO_PA6) //  SPI0 Peripheral Chip Select 3
#define AT91C_PA6_MCDA3    ((unsigned int) AT91C_PIO_PA6) //  Multimedia Card A Data 3
#define AT91C_PIO_PA7        ((unsigned int) 1 <<  7) // Pin Controlled by PA7
#define AT91C_PA7_TWD      ((unsigned int) AT91C_PIO_PA7) //  TWI Two-wire Serial Data
#define AT91C_PA7_PCK0     ((unsigned int) AT91C_PIO_PA7) //  PMC Programmable clock Output 0
#define AT91C_PIO_PA8        ((unsigned int) 1 <<  8) // Pin Controlled by PA8
#define AT91C_PA8_TWCK     ((unsigned int) AT91C_PIO_PA8) //  TWI Two-wire Serial Clock
#define AT91C_PA8_PCK1     ((unsigned int) AT91C_PIO_PA8) //  PMC Programmable clock Output 1
#define AT91C_PIO_PA9        ((unsigned int) 1 <<  9) // Pin Controlled by PA9
#define AT91C_PA9_DRXD     ((unsigned int) AT91C_PIO_PA9) //  DBGU Debug Receive Data
#define AT91C_PA9_PCK2     ((unsigned int) AT91C_PIO_PA9) //  PMC Programmable clock Output 2
#define AT91C_PIO_PB0        ((unsigned int) 1 <<  0) // Pin Controlled by PB0
#define AT91C_PB0_LCDVSYNC ((unsigned int) AT91C_PIO_PB0) //  LCD Vertical Synchronization
#define AT91C_PIO_PB1        ((unsigned int) 1 <<  1) // Pin Controlled by PB1
#define AT91C_PB1_LCDHSYNC ((unsigned int) AT91C_PIO_PB1) //  LCD Horizontal Synchronization
#define AT91C_PIO_PB10       ((unsigned int) 1 << 10) // Pin Controlled by PB10
#define AT91C_PB10_LCDD5    ((unsigned int) AT91C_PIO_PB10) //  LCD Data Bus Bit 5
#define AT91C_PB10_LCDD10   ((unsigned int) AT91C_PIO_PB10) //  LCD Data Bus Bit 10
#define AT91C_PIO_PB11       ((unsigned int) 1 << 11) // Pin Controlled by PB11
#define AT91C_PB11_LCDD6    ((unsigned int) AT91C_PIO_PB11) //  LCD Data Bus Bit 6
#define AT91C_PB11_LCDD11   ((unsigned int) AT91C_PIO_PB11) //  LCD Data Bus Bit 11
#define AT91C_PIO_PB12       ((unsigned int) 1 << 12) // Pin Controlled by PB12
#define AT91C_PB12_LCDD7    ((unsigned int) AT91C_PIO_PB12) //  LCD Data Bus Bit 7
#define AT91C_PB12_LCDD12   ((unsigned int) AT91C_PIO_PB12) //  LCD Data Bus Bit 12
#define AT91C_PIO_PB13       ((unsigned int) 1 << 13) // Pin Controlled by PB13
#define AT91C_PB13_LCDD8    ((unsigned int) AT91C_PIO_PB13) //  LCD Data Bus Bit 8
#define AT91C_PB13_LCDD13   ((unsigned int) AT91C_PIO_PB13) //  LCD Data Bus Bit 13
#define AT91C_PIO_PB14       ((unsigned int) 1 << 14) // Pin Controlled by PB14
#define AT91C_PB14_LCDD9    ((unsigned int) AT91C_PIO_PB14) //  LCD Data Bus Bit 9
#define AT91C_PB14_LCDD14   ((unsigned int) AT91C_PIO_PB14) //  LCD Data Bus Bit 14
#define AT91C_PIO_PB15       ((unsigned int) 1 << 15) // Pin Controlled by PB15
#define AT91C_PB15_LCDD10   ((unsigned int) AT91C_PIO_PB15) //  LCD Data Bus Bit 10
#define AT91C_PB15_LCDD15   ((unsigned int) AT91C_PIO_PB15) //  LCD Data Bus Bit 15
#define AT91C_PIO_PB16       ((unsigned int) 1 << 16) // Pin Controlled by PB16
#define AT91C_PB16_LCDD11   ((unsigned int) AT91C_PIO_PB16) //  LCD Data Bus Bit 11
#define AT91C_PB16_LCDD19   ((unsigned int) AT91C_PIO_PB16) //  LCD Data Bus Bit 19
#define AT91C_PIO_PB17       ((unsigned int) 1 << 17) // Pin Controlled by PB17
#define AT91C_PB17_LCDD12   ((unsigned int) AT91C_PIO_PB17) //  LCD Data Bus Bit 12
#define AT91C_PB17_LCDD20   ((unsigned int) AT91C_PIO_PB17) //  LCD Data Bus Bit 20
#define AT91C_PIO_PB18       ((unsigned int) 1 << 18) // Pin Controlled by PB18
#define AT91C_PB18_LCDD13   ((unsigned int) AT91C_PIO_PB18) //  LCD Data Bus Bit 13
#define AT91C_PB18_LCDD21   ((unsigned int) AT91C_PIO_PB18) //  LCD Data Bus Bit 21
#define AT91C_PIO_PB19       ((unsigned int) 1 << 19) // Pin Controlled by PB19
#define AT91C_PB19_LCDD14   ((unsigned int) AT91C_PIO_PB19) //  LCD Data Bus Bit 14
#define AT91C_PB19_LCDD22   ((unsigned int) AT91C_PIO_PB19) //  LCD Data Bus Bit 22
#define AT91C_PIO_PB2        ((unsigned int) 1 <<  2) // Pin Controlled by PB2
#define AT91C_PB2_LCDDOTCK ((unsigned int) AT91C_PIO_PB2) //  LCD Dot Clock
#define AT91C_PB2_PCK0     ((unsigned int) AT91C_PIO_PB2) //  PMC Programmable clock Output 0
#define AT91C_PIO_PB20       ((unsigned int) 1 << 20) // Pin Controlled by PB20
#define AT91C_PB20_LCDD15   ((unsigned int) AT91C_PIO_PB20) //  LCD Data Bus Bit 15
#define AT91C_PB20_LCDD23   ((unsigned int) AT91C_PIO_PB20) //  LCD Data Bus Bit 23
#define AT91C_PIO_PB21       ((unsigned int) 1 << 21) // Pin Controlled by PB21
#define AT91C_PB21_TF0      ((unsigned int) AT91C_PIO_PB21) //  SSC0 Transmit Frame Sync
#define AT91C_PB21_LCDD16   ((unsigned int) AT91C_PIO_PB21) //  LCD Data Bus Bit 16
#define AT91C_PIO_PB22       ((unsigned int) 1 << 22) // Pin Controlled by PB22
#define AT91C_PB22_TK0      ((unsigned int) AT91C_PIO_PB22) //  SSC0 Transmit Clock
#define AT91C_PB22_LCDD17   ((unsigned int) AT91C_PIO_PB22) //  LCD Data Bus Bit 17
#define AT91C_PIO_PB23       ((unsigned int) 1 << 23) // Pin Controlled by PB23
#define AT91C_PB23_TD0      ((unsigned int) AT91C_PIO_PB23) //  SSC0 Transmit Data
#define AT91C_PB23_LCDD18   ((unsigned int) AT91C_PIO_PB23) //  LCD Data Bus Bit 18
#define AT91C_PIO_PB24       ((unsigned int) 1 << 24) // Pin Controlled by PB24
#define AT91C_PB24_RD0      ((unsigned int) AT91C_PIO_PB24) //  SSC0 Receive Data
#define AT91C_PB24_LCDD19   ((unsigned int) AT91C_PIO_PB24) //  LCD Data Bus Bit 19
#define AT91C_PIO_PB25       ((unsigned int) 1 << 25) // Pin Controlled by PB25
#define AT91C_PB25_RK0      ((unsigned int) AT91C_PIO_PB25) //  SSC0 Receive Clock
#define AT91C_PB25_LCDD20   ((unsigned int) AT91C_PIO_PB25) //  LCD Data Bus Bit 20
#define AT91C_PIO_PB26       ((unsigned int) 1 << 26) // Pin Controlled by PB26
#define AT91C_PB26_RF0      ((unsigned int) AT91C_PIO_PB26) //  SSC0 Receive Frame Sync
#define AT91C_PB26_LCDD21   ((unsigned int) AT91C_PIO_PB26) //  LCD Data Bus Bit 21
#define AT91C_PIO_PB27       ((unsigned int) 1 << 27) // Pin Controlled by PB27
#define AT91C_PB27_NPCS11   ((unsigned int) AT91C_PIO_PB27) //  SPI1 Peripheral Chip Select 1
#define AT91C_PB27_LCDD22   ((unsigned int) AT91C_PIO_PB27) //  LCD Data Bus Bit 22
#define AT91C_PIO_PB28       ((unsigned int) 1 << 28) // Pin Controlled by PB28
#define AT91C_PB28_NPCS10   ((unsigned int) AT91C_PIO_PB28) //  SPI1 Peripheral Chip Select 0
#define AT91C_PB28_LCDD23   ((unsigned int) AT91C_PIO_PB28) //  LCD Data Bus Bit 23
#define AT91C_PIO_PB29       ((unsigned int) 1 << 29) // Pin Controlled by PB29
#define AT91C_PB29_SPCK1    ((unsigned int) AT91C_PIO_PB29) //  SPI1 Serial Clock
#define AT91C_PB29_IRQ2     ((unsigned int) AT91C_PIO_PB29) //  Interrupt input 2
#define AT91C_PIO_PB3        ((unsigned int) 1 <<  3) // Pin Controlled by PB3
#define AT91C_PB3_LCDDEN   ((unsigned int) AT91C_PIO_PB3) //  LCD Data Enable
#define AT91C_PIO_PB30       ((unsigned int) 1 << 30) // Pin Controlled by PB30
#define AT91C_PB30_MISO1    ((unsigned int) AT91C_PIO_PB30) //  SPI1 Master In Slave
#define AT91C_PB30_IRQ1     ((unsigned int) AT91C_PIO_PB30) //  Interrupt input 1
#define AT91C_PIO_PB31       ((unsigned int) 1 << 31) // Pin Controlled by PB31
#define AT91C_PB31_MOSI1    ((unsigned int) AT91C_PIO_PB31) //  SPI1 Master Out Slave
#define AT91C_PB31_PCK2     ((unsigned int) AT91C_PIO_PB31) //  PMC Programmable clock Output 2
#define AT91C_PIO_PB4        ((unsigned int) 1 <<  4) // Pin Controlled by PB4
#define AT91C_PB4_LCDCC    ((unsigned int) AT91C_PIO_PB4) //  LCD Contrast Control
#define AT91C_PB4_LCDD2    ((unsigned int) AT91C_PIO_PB4) //  LCD Data Bus Bit 2
#define AT91C_PIO_PB5        ((unsigned int) 1 <<  5) // Pin Controlled by PB5
#define AT91C_PB5_LCDD0    ((unsigned int) AT91C_PIO_PB5) //  LCD Data Bus Bit 0
#define AT91C_PB5_LCDD3    ((unsigned int) AT91C_PIO_PB5) //  LCD Data Bus Bit 3
#define AT91C_PIO_PB6        ((unsigned int) 1 <<  6) // Pin Controlled by PB6
#define AT91C_PB6_LCDD1    ((unsigned int) AT91C_PIO_PB6) //  LCD Data Bus Bit 1
#define AT91C_PB6_LCDD4    ((unsigned int) AT91C_PIO_PB6) //  LCD Data Bus Bit 4
#define AT91C_PIO_PB7        ((unsigned int) 1 <<  7) // Pin Controlled by PB7
#define AT91C_PB7_LCDD2    ((unsigned int) AT91C_PIO_PB7) //  LCD Data Bus Bit 2
#define AT91C_PB7_LCDD5    ((unsigned int) AT91C_PIO_PB7) //  LCD Data Bus Bit 5
#define AT91C_PIO_PB8        ((unsigned int) 1 <<  8) // Pin Controlled by PB8
#define AT91C_PB8_LCDD3    ((unsigned int) AT91C_PIO_PB8) //  LCD Data Bus Bit 3
#define AT91C_PB8_LCDD6    ((unsigned int) AT91C_PIO_PB8) //  LCD Data Bus Bit 6
#define AT91C_PIO_PB9        ((unsigned int) 1 <<  9) // Pin Controlled by PB9
#define AT91C_PB9_LCDD4    ((unsigned int) AT91C_PIO_PB9) //  LCD Data Bus Bit 4
#define AT91C_PB9_LCDD7    ((unsigned int) AT91C_PIO_PB9) //  LCD Data Bus Bit 7
#define AT91C_PIO_PC0        ((unsigned int) 1 <<  0) // Pin Controlled by PC0
#define AT91C_PC0_SMOE     ((unsigned int) AT91C_PIO_PC0) //  SmartMedia Output Enable
#define AT91C_PC0_NCS6     ((unsigned int) AT91C_PIO_PC0) //  Chip Select 6
#define AT91C_PIO_PC1        ((unsigned int) 1 <<  1) // Pin Controlled by PC1
#define AT91C_PC1_SMWE     ((unsigned int) AT91C_PIO_PC1) //  SmartMedia Write Enable
#define AT91C_PC1_NCS7     ((unsigned int) AT91C_PIO_PC1) //  Chip Select 7
#define AT91C_PIO_PC10       ((unsigned int) 1 << 10) // Pin Controlled by PC10
#define AT91C_PC10_RTS0     ((unsigned int) AT91C_PIO_PC10) //  USART0 Ready To Send
#define AT91C_PC10_SCK0     ((unsigned int) AT91C_PIO_PC10) //  USART0 Serial Clock
#define AT91C_PIO_PC11       ((unsigned int) 1 << 11) // Pin Controlled by PC11
#define AT91C_PC11_CTS0     ((unsigned int) AT91C_PIO_PC11) //  USART0 Clear To Send
#define AT91C_PC11_FIQ      ((unsigned int) AT91C_PIO_PC11) //  AIC Fast Interrupt Input
#define AT91C_PIO_PC12       ((unsigned int) 1 << 12) // Pin Controlled by PC12
#define AT91C_PC12_TXD1     ((unsigned int) AT91C_PIO_PC12) //  USART1 Transmit Data
#define AT91C_PC12_NCS6     ((unsigned int) AT91C_PIO_PC12) //  Chip Select 6
#define AT91C_PIO_PC13       ((unsigned int) 1 << 13) // Pin Controlled by PC13
#define AT91C_PC13_RXD1     ((unsigned int) AT91C_PIO_PC13) //  USART1 Receive Data
#define AT91C_PC13_NCS7     ((unsigned int) AT91C_PIO_PC13) //  Chip Select 7
#define AT91C_PIO_PC14       ((unsigned int) 1 << 14) // Pin Controlled by PC14
#define AT91C_PC14_TXD2     ((unsigned int) AT91C_PIO_PC14) //  USART2 Transmit Data
#define AT91C_PC14_NPCS12   ((unsigned int) AT91C_PIO_PC14) //  SPI1 Peripheral Chip Select 2
#define AT91C_PIO_PC15       ((unsigned int) 1 << 15) // Pin Controlled by PC15
#define AT91C_PC15_RXD2     ((unsigned int) AT91C_PIO_PC15) //  USART2 Receive Data
#define AT91C_PC15_NPCS13   ((unsigned int) AT91C_PIO_PC15) //  SPI1 Peripheral Chip Select 3
#define AT91C_PIO_PC16       ((unsigned int) 1 << 16) // Pin Controlled by PC16
#define AT91C_PC16_D16      ((unsigned int) AT91C_PIO_PC16) //  Data Bus [16]
#define AT91C_PC16_TCLK0    ((unsigned int) AT91C_PIO_PC16) //  Timer Counter 0 external clock input
#define AT91C_PIO_PC17       ((unsigned int) 1 << 17) // Pin Controlled by PC17
#define AT91C_PC17_D17      ((unsigned int) AT91C_PIO_PC17) //  Data Bus [17]
#define AT91C_PC17_TCLK1    ((unsigned int) AT91C_PIO_PC17) //  Timer Counter 1 external clock input
#define AT91C_PIO_PC18       ((unsigned int) 1 << 18) // Pin Controlled by PC18
#define AT91C_PC18_D18      ((unsigned int) AT91C_PIO_PC18) //  Data Bus [18]
#define AT91C_PC18_TCLK2    ((unsigned int) AT91C_PIO_PC18) //  Timer Counter 2 external clock input
#define AT91C_PIO_PC19       ((unsigned int) 1 << 19) // Pin Controlled by PC19
#define AT91C_PC19_D19      ((unsigned int) AT91C_PIO_PC19) //  Data Bus [19]
#define AT91C_PC19_TIOA0    ((unsigned int) AT91C_PIO_PC19) //  Timer Counter 0 Multipurpose Timer I/O Pin A
#define AT91C_PIO_PC2        ((unsigned int) 1 <<  2) // Pin Controlled by PC2
#define AT91C_PC2_NWAIT    ((unsigned int) AT91C_PIO_PC2) //  NWAIT
#define AT91C_PC2_IRQ0     ((unsigned int) AT91C_PIO_PC2) //  Interrupt input 0
#define AT91C_PIO_PC20       ((unsigned int) 1 << 20) // Pin Controlled by PC20
#define AT91C_PC20_D20      ((unsigned int) AT91C_PIO_PC20) //  Data Bus [20]
#define AT91C_PC20_TIOB0    ((unsigned int) AT91C_PIO_PC20) //  Timer Counter 0 Multipurpose Timer I/O Pin B
#define AT91C_PIO_PC21       ((unsigned int) 1 << 21) // Pin Controlled by PC21
#define AT91C_PC21_D21      ((unsigned int) AT91C_PIO_PC21) //  Data Bus [21]
#define AT91C_PC21_TIOA1    ((unsigned int) AT91C_PIO_PC21) //  Timer Counter 1 Multipurpose Timer I/O Pin A
#define AT91C_PIO_PC22       ((unsigned int) 1 << 22) // Pin Controlled by PC22
#define AT91C_PC22_D22      ((unsigned int) AT91C_PIO_PC22) //  Data Bus [22]
#define AT91C_PC22_TIOB1    ((unsigned int) AT91C_PIO_PC22) //  Timer Counter 1 Multipurpose Timer I/O Pin B
#define AT91C_PIO_PC23       ((unsigned int) 1 << 23) // Pin Controlled by PC23
#define AT91C_PC23_D23      ((unsigned int) AT91C_PIO_PC23) //  Data Bus [23]
#define AT91C_PC23_TIOA2    ((unsigned int) AT91C_PIO_PC23) //  Timer Counter 2 Multipurpose Timer I/O Pin A
#define AT91C_PIO_PC24       ((unsigned int) 1 << 24) // Pin Controlled by PC24
#define AT91C_PC24_D24      ((unsigned int) AT91C_PIO_PC24) //  Data Bus [24]
#define AT91C_PC24_TIOB2    ((unsigned int) AT91C_PIO_PC24) //  Timer Counter 2 Multipurpose Timer I/O Pin B
#define AT91C_PIO_PC25       ((unsigned int) 1 << 25) // Pin Controlled by PC25
#define AT91C_PC25_D25      ((unsigned int) AT91C_PIO_PC25) //  Data Bus [25]
#define AT91C_PC25_TF2      ((unsigned int) AT91C_PIO_PC25) //  SSC2 Transmit Frame Sync
#define AT91C_PIO_PC26       ((unsigned int) 1 << 26) // Pin Controlled by PC26
#define AT91C_PC26_D26      ((unsigned int) AT91C_PIO_PC26) //  Data Bus [26]
#define AT91C_PC26_TK2      ((unsigned int) AT91C_PIO_PC26) //  SSC2 Transmit Clock
#define AT91C_PIO_PC27       ((unsigned int) 1 << 27) // Pin Controlled by PC27
#define AT91C_PC27_D27      ((unsigned int) AT91C_PIO_PC27) //  Data Bus [27]
#define AT91C_PC27_TD2      ((unsigned int) AT91C_PIO_PC27) //  SSC2 Transmit Data
#define AT91C_PIO_PC28       ((unsigned int) 1 << 28) // Pin Controlled by PC28
#define AT91C_PC28_D28      ((unsigned int) AT91C_PIO_PC28) //  Data Bus [28]
#define AT91C_PC28_RD2      ((unsigned int) AT91C_PIO_PC28) //  SSC2 Receive Data
#define AT91C_PIO_PC29       ((unsigned int) 1 << 29) // Pin Controlled by PC29
#define AT91C_PC29_D29      ((unsigned int) AT91C_PIO_PC29) //  Data Bus [29]
#define AT91C_PC29_RK2      ((unsigned int) AT91C_PIO_PC29) //  SSC2 Receive Clock
#define AT91C_PIO_PC3        ((unsigned int) 1 <<  3) // Pin Controlled by PC3
#define AT91C_PC3_A25_CFRNW ((unsigned int) AT91C_PIO_PC3) //  Address Bus[25] / Compact Flash Read Not Write
#define AT91C_PIO_PC30       ((unsigned int) 1 << 30) // Pin Controlled by PC30
#define AT91C_PC30_D30      ((unsigned int) AT91C_PIO_PC30) //  Data Bus [30]
#define AT91C_PC30_RF2      ((unsigned int) AT91C_PIO_PC30) //  SSC2 Receive Frame Sync
#define AT91C_PIO_PC31       ((unsigned int) 1 << 31) // Pin Controlled by PC31
#define AT91C_PC31_D31      ((unsigned int) AT91C_PIO_PC31) //  Data Bus [31]
#define AT91C_PC31_PCK1     ((unsigned int) AT91C_PIO_PC31) //  PMC Programmable clock Output 1
#define AT91C_PIO_PC4        ((unsigned int) 1 <<  4) // Pin Controlled by PC4
#define AT91C_PC4_NCS4_CFCS0 ((unsigned int) AT91C_PIO_PC4) //  Chip Select 4 / CompactFlash Chip Select 0
#define AT91C_PIO_PC5        ((unsigned int) 1 <<  5) // Pin Controlled by PC5
#define AT91C_PC5_NCS5_CFCS1 ((unsigned int) AT91C_PIO_PC5) //  Chip Select 5 / CompactFlash Chip Select 1
#define AT91C_PIO_PC6        ((unsigned int) 1 <<  6) // Pin Controlled by PC6
#define AT91C_PC6_CFCE1    ((unsigned int) AT91C_PIO_PC6) //  CompactFlash Chip Enable 1
#define AT91C_PIO_PC7        ((unsigned int) 1 <<  7) // Pin Controlled by PC7
#define AT91C_PC7_CFCE2    ((unsigned int) AT91C_PIO_PC7) //  CompactFlash Chip Enable 2
#define AT91C_PIO_PC8        ((unsigned int) 1 <<  8) // Pin Controlled by PC8
#define AT91C_PC8_TXD0     ((unsigned int) AT91C_PIO_PC8) //  USART0 Transmit Data
#define AT91C_PC8_PCK2     ((unsigned int) AT91C_PIO_PC8) //  PMC Programmable clock Output 2
#define AT91C_PIO_PC9        ((unsigned int) 1 <<  9) // Pin Controlled by PC9
#define AT91C_PC9_RXD0     ((unsigned int) AT91C_PIO_PC9) //  USART0 Receive Data
#define AT91C_PC9_PCK3     ((unsigned int) AT91C_PIO_PC9) //  PMC Programmable clock Output 3


