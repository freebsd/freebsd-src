// ----------------------------------------------------------------------------
//          ATMEL Microcontroller Software Support  -  ROUSSET  -
// ----------------------------------------------------------------------------
//  The software is delivered "AS IS" without warranty or condition of any
//  kind, either express, implied or statutory. This includes without
//  limitation any warranty or condition with respect to merchantability or
//  fitness for any particular purpose, or against the infringements of
//  intellectual property rights of others.
// ----------------------------------------------------------------------------
// File Name           : AT91RM9200.h
// Object              : AT91RM9200 / SPI definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_SPI_H
#define AT91RM9200_SPI_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Serial Parallel Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_SPI {
	AT91_REG	 SPI_CR; 	// Control Register
	AT91_REG	 SPI_MR; 	// Mode Register
	AT91_REG	 SPI_RDR; 	// Receive Data Register
	AT91_REG	 SPI_TDR; 	// Transmit Data Register
	AT91_REG	 SPI_SR; 	// Status Register
	AT91_REG	 SPI_IER; 	// Interrupt Enable Register
	AT91_REG	 SPI_IDR; 	// Interrupt Disable Register
	AT91_REG	 SPI_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved0[4]; 	//
	AT91_REG	 SPI_CSR0; 	// Chip Select Register 0
	AT91_REG	 SPI_CSR1; 	// Chip Select Register 1
	AT91_REG	 SPI_CSR2; 	// Chip Select Register 2
	AT91_REG	 SPI_CSR3; 	// Chip Select Register 3
	AT91_REG	 Reserved1[48]; 	//
	AT91_REG	 SPI_RPR; 	// Receive Pointer Register
	AT91_REG	 SPI_RCR; 	// Receive Counter Register
	AT91_REG	 SPI_TPR; 	// Transmit Pointer Register
	AT91_REG	 SPI_TCR; 	// Transmit Counter Register
	AT91_REG	 SPI_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 SPI_RNCR; 	// Receive Next Counter Register
	AT91_REG	 SPI_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 SPI_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 SPI_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 SPI_PTSR; 	// PDC Transfer Status Register
} AT91S_SPI, *AT91PS_SPI;

#endif

// -------- SPI_CR : (SPI Offset: 0x0) SPI Control Register --------
#define AT91C_SPI_SPIEN       ( 0x1 <<  0) // (SPI) SPI Enable
#define AT91C_SPI_SPIDIS      ( 0x1 <<  1) // (SPI) SPI Disable
#define AT91C_SPI_SWRST       ( 0x1 <<  7) // (SPI) SPI Software reset
// -------- SPI_MR : (SPI Offset: 0x4) SPI Mode Register --------
#define AT91C_SPI_MSTR        ( 0x1 <<  0) // (SPI) Master/Slave Mode
#define AT91C_SPI_PS          ( 0x1 <<  1) // (SPI) Peripheral Select
#define 	AT91C_SPI_PS_FIXED                ( 0x0 <<  1) // (SPI) Fixed Peripheral Select
#define 	AT91C_SPI_PS_VARIABLE             ( 0x1 <<  1) // (SPI) Variable Peripheral Select
#define AT91C_SPI_PCSDEC      ( 0x1 <<  2) // (SPI) Chip Select Decode
#define AT91C_SPI_DIV32       ( 0x1 <<  3) // (SPI) Clock Selection
#define AT91C_SPI_MODFDIS     ( 0x1 <<  4) // (SPI) Mode Fault Detection
#define AT91C_SPI_LLB         ( 0x1 <<  7) // (SPI) Clock Selection
#define AT91C_SPI_PCS         ( 0xF << 16) // (SPI) Peripheral Chip Select
#define AT91C_SPI_DLYBCS      ( 0xFF << 24) // (SPI) Delay Between Chip Selects
// -------- SPI_RDR : (SPI Offset: 0x8) Receive Data Register --------
#define AT91C_SPI_RD          ( 0xFFFF <<  0) // (SPI) Receive Data
#define AT91C_SPI_RPCS        ( 0xF << 16) // (SPI) Peripheral Chip Select Status
// -------- SPI_TDR : (SPI Offset: 0xc) Transmit Data Register --------
#define AT91C_SPI_TD          ( 0xFFFF <<  0) // (SPI) Transmit Data
#define AT91C_SPI_TPCS        ( 0xF << 16) // (SPI) Peripheral Chip Select Status
// -------- SPI_SR : (SPI Offset: 0x10) Status Register --------
#define AT91C_SPI_RDRF        ( 0x1 <<  0) // (SPI) Receive Data Register Full
#define AT91C_SPI_TDRE        ( 0x1 <<  1) // (SPI) Transmit Data Register Empty
#define AT91C_SPI_MODF        ( 0x1 <<  2) // (SPI) Mode Fault Error
#define AT91C_SPI_OVRES       ( 0x1 <<  3) // (SPI) Overrun Error Status
#define AT91C_SPI_SPENDRX     ( 0x1 <<  4) // (SPI) End of Receiver Transfer
#define AT91C_SPI_SPENDTX     ( 0x1 <<  5) // (SPI) End of Transmit Transfer
#define AT91C_SPI_RXBUFF      ( 0x1 <<  6) // (SPI) RXBUFF Interrupt
#define AT91C_SPI_TXBUFE      ( 0x1 <<  7) // (SPI) TXBUFE Interrupt
#define AT91C_SPI_SPIENS      ( 0x1 << 16) // (SPI) Enable Status
// -------- SPI_IER : (SPI Offset: 0x14) Interrupt Enable Register --------
// -------- SPI_IDR : (SPI Offset: 0x18) Interrupt Disable Register --------
// -------- SPI_IMR : (SPI Offset: 0x1c) Interrupt Mask Register --------
// -------- SPI_CSR : (SPI Offset: 0x30) Chip Select Register --------
#define AT91C_SPI_CPOL        ( 0x1 <<  0) // (SPI) Clock Polarity
#define AT91C_SPI_NCPHA       ( 0x1 <<  1) // (SPI) Clock Phase
#define AT91C_SPI_BITS        ( 0xF <<  4) // (SPI) Bits Per Transfer
#define 	AT91C_SPI_BITS_8                    ( 0x0 <<  4) // (SPI) 8 Bits Per transfer
#define 	AT91C_SPI_BITS_9                    ( 0x1 <<  4) // (SPI) 9 Bits Per transfer
#define 	AT91C_SPI_BITS_10                   ( 0x2 <<  4) // (SPI) 10 Bits Per transfer
#define 	AT91C_SPI_BITS_11                   ( 0x3 <<  4) // (SPI) 11 Bits Per transfer
#define 	AT91C_SPI_BITS_12                   ( 0x4 <<  4) // (SPI) 12 Bits Per transfer
#define 	AT91C_SPI_BITS_13                   ( 0x5 <<  4) // (SPI) 13 Bits Per transfer
#define 	AT91C_SPI_BITS_14                   ( 0x6 <<  4) // (SPI) 14 Bits Per transfer
#define 	AT91C_SPI_BITS_15                   ( 0x7 <<  4) // (SPI) 15 Bits Per transfer
#define 	AT91C_SPI_BITS_16                   ( 0x8 <<  4) // (SPI) 16 Bits Per transfer
#define AT91C_SPI_SCBR        ( 0xFF <<  8) // (SPI) Serial Clock Baud Rate
#define AT91C_SPI_DLYBS       ( 0xFF << 16) // (SPI) Serial Clock Baud Rate
#define AT91C_SPI_DLYBCT      ( 0xFF << 24) // (SPI) Delay Between Consecutive Transfers

#endif
