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
// Object              : AT91RM9200 / TWI definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_TWI_H
#define AT91RM9200_TWI_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Two-wire Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_TWI {
	AT91_REG	 TWI_CR; 	// Control Register
	AT91_REG	 TWI_MMR; 	// Master Mode Register
	AT91_REG	 TWI_SMR; 	// Slave Mode Register
	AT91_REG	 TWI_IADR; 	// Internal Address Register
	AT91_REG	 TWI_CWGR; 	// Clock Waveform Generator Register
	AT91_REG	 Reserved0[3]; 	//
	AT91_REG	 TWI_SR; 	// Status Register
	AT91_REG	 TWI_IER; 	// Interrupt Enable Register
	AT91_REG	 TWI_IDR; 	// Interrupt Disable Register
	AT91_REG	 TWI_IMR; 	// Interrupt Mask Register
	AT91_REG	 TWI_RHR; 	// Receive Holding Register
	AT91_REG	 TWI_THR; 	// Transmit Holding Register
	AT91_REG	 Reserved1[50]; 	//
	AT91_REG	 TWI_RPR; 	// Receive Pointer Register
	AT91_REG	 TWI_RCR; 	// Receive Counter Register
	AT91_REG	 TWI_TPR; 	// Transmit Pointer Register
	AT91_REG	 TWI_TCR; 	// Transmit Counter Register
	AT91_REG	 TWI_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 TWI_RNCR; 	// Receive Next Counter Register
	AT91_REG	 TWI_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 TWI_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 TWI_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 TWI_PTSR; 	// PDC Transfer Status Register
} AT91S_TWI, *AT91PS_TWI;

#endif

// -------- TWI_CR : (TWI Offset: 0x0) TWI Control Register --------
#define AT91C_TWI_START       ( 0x1 <<  0) // (TWI) Send a START Condition
#define AT91C_TWI_STOP        ( 0x1 <<  1) // (TWI) Send a STOP Condition
#define AT91C_TWI_MSEN        ( 0x1 <<  2) // (TWI) TWI Master Transfer Enabled
#define AT91C_TWI_MSDIS       ( 0x1 <<  3) // (TWI) TWI Master Transfer Disabled
#define AT91C_TWI_SVEN        ( 0x1 <<  4) // (TWI) TWI Slave Transfer Enabled
#define AT91C_TWI_SVDIS       ( 0x1 <<  5) // (TWI) TWI Slave Transfer Disabled
#define AT91C_TWI_SWRST       ( 0x1 <<  7) // (TWI) Software Reset
// -------- TWI_MMR : (TWI Offset: 0x4) TWI Master Mode Register --------
#define AT91C_TWI_IADRSZ      ( 0x3 <<  8) // (TWI) Internal Device Address Size
#define 	AT91C_TWI_IADRSZ_NO                   ( 0x0 <<  8) // (TWI) No internal device address
#define 	AT91C_TWI_IADRSZ_1_BYTE               ( 0x1 <<  8) // (TWI) One-byte internal device address
#define 	AT91C_TWI_IADRSZ_2_BYTE               ( 0x2 <<  8) // (TWI) Two-byte internal device address
#define 	AT91C_TWI_IADRSZ_3_BYTE               ( 0x3 <<  8) // (TWI) Three-byte internal device address
#define AT91C_TWI_MREAD       ( 0x1 << 12) // (TWI) Master Read Direction
#define AT91C_TWI_DADR        ( 0x7F <<  6) // (TWI) Device Address
// -------- TWI_SMR : (TWI Offset: 0x8) TWI Slave Mode Register --------
#define AT91C_TWI_SADR        ( 0x7F << 16) // (TWI) Slave Device Address
// -------- TWI_CWGR : (TWI Offset: 0x10) TWI Clock Waveform Generator Register --------
#define AT91C_TWI_CLDIV       ( 0xFF <<  0) // (TWI) Clock Low Divider
#define AT91C_TWI_CHDIV       ( 0xFF <<  8) // (TWI) Clock High Divider
#define AT91C_TWI_CKDIV       ( 0x7 << 16) // (TWI) Clock Divider
// -------- TWI_SR : (TWI Offset: 0x20) TWI Status Register --------
#define AT91C_TWI_TXCOMP      ( 0x1 <<  0) // (TWI) Transmission Completed
#define AT91C_TWI_RXRDY       ( 0x1 <<  1) // (TWI) Receive holding register ReaDY
#define AT91C_TWI_TXRDY       ( 0x1 <<  2) // (TWI) Transmit holding register ReaDY
#define AT91C_TWI_SVREAD      ( 0x1 <<  3) // (TWI) Slave Read
#define AT91C_TWI_SVACC       ( 0x1 <<  4) // (TWI) Slave Access
#define AT91C_TWI_GCACC       ( 0x1 <<  5) // (TWI) General Call Access
#define AT91C_TWI_OVRE        ( 0x1 <<  6) // (TWI) Overrun Error
#define AT91C_TWI_UNRE        ( 0x1 <<  7) // (TWI) Underrun Error
#define AT91C_TWI_NACK        ( 0x1 <<  8) // (TWI) Not Acknowledged
#define AT91C_TWI_ARBLST      ( 0x1 <<  9) // (TWI) Arbitration Lost
// -------- TWI_IER : (TWI Offset: 0x24) TWI Interrupt Enable Register --------
// -------- TWI_IDR : (TWI Offset: 0x28) TWI Interrupt Disable Register --------
// -------- TWI_IMR : (TWI Offset: 0x2c) TWI Interrupt Mask Register --------

#endif
