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
// Object              : AT91RM9200 / UDP Device definitions
// Generated           : AT91 SW Application Group  12/03/2002 (10:48:02)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_UDP_H
#define AT91RM9200_UDP_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR USB Device Interface
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_UDP {
	AT91_REG	 UDP_NUM; 	// Frame Number Register
	AT91_REG	 UDP_GLBSTATE; 	// Global State Register
	AT91_REG	 UDP_FADDR; 	// Function Address Register
	AT91_REG	 Reserved0[1]; 	//
	AT91_REG	 UDP_IER; 	// Interrupt Enable Register
	AT91_REG	 UDP_IDR; 	// Interrupt Disable Register
	AT91_REG	 UDP_IMR; 	// Interrupt Mask Register
	AT91_REG	 UDP_ISR; 	// Interrupt Status Register
	AT91_REG	 UDP_ICR; 	// Interrupt Clear Register
	AT91_REG	 Reserved1[1]; 	//
	AT91_REG	 UDP_RSTEP; 	// Reset Endpoint Register
	AT91_REG	 Reserved2[1]; 	//
	AT91_REG	 UDP_CSR[8]; 	// Endpoint Control and Status Register
	AT91_REG	 UDP_FDR[8]; 	// Endpoint FIFO Data Register
} AT91S_UDP, *AT91PS_UDP;

#endif

// -------- UDP_FRM_NUM : (UDP Offset: 0x0) USB Frame Number Register --------
#define AT91C_UDP_FRM_NUM     ( 0x7FF <<  0) // (UDP) Frame Number as Defined in the Packet Field Formats
#define AT91C_UDP_FRM_ERR     ( 0x1 << 16) // (UDP) Frame Error
#define AT91C_UDP_FRM_OK      ( 0x1 << 17) // (UDP) Frame OK
// -------- UDP_GLB_STATE : (UDP Offset: 0x4) USB Global State Register --------
#define AT91C_UDP_FADDEN      ( 0x1 <<  0) // (UDP) Function Address Enable
#define AT91C_UDP_CONFG       ( 0x1 <<  1) // (UDP) Configured
#define AT91C_UDP_RMWUPE      ( 0x1 <<  2) // (UDP) Remote Wake Up Enable
#define AT91C_UDP_RSMINPR     ( 0x1 <<  3) // (UDP) A Resume Has Been Sent to the Host
// -------- UDP_FADDR : (UDP Offset: 0x8) USB Function Address Register --------
#define AT91C_UDP_FADD        ( 0xFF <<  0) // (UDP) Function Address Value
#define AT91C_UDP_FEN         ( 0x1 <<  8) // (UDP) Function Enable
// -------- UDP_IER : (UDP Offset: 0x10) USB Interrupt Enable Register --------
#define AT91C_UDP_EPINT0      ( 0x1 <<  0) // (UDP) Endpoint 0 Interrupt
#define AT91C_UDP_EPINT1      ( 0x1 <<  1) // (UDP) Endpoint 0 Interrupt
#define AT91C_UDP_EPINT2      ( 0x1 <<  2) // (UDP) Endpoint 2 Interrupt
#define AT91C_UDP_EPINT3      ( 0x1 <<  3) // (UDP) Endpoint 3 Interrupt
#define AT91C_UDP_EPINT4      ( 0x1 <<  4) // (UDP) Endpoint 4 Interrupt
#define AT91C_UDP_EPINT5      ( 0x1 <<  5) // (UDP) Endpoint 5 Interrupt
#define AT91C_UDP_EPINT6      ( 0x1 <<  6) // (UDP) Endpoint 6 Interrupt
#define AT91C_UDP_EPINT7      ( 0x1 <<  7) // (UDP) Endpoint 7 Interrupt
#define AT91C_UDP_RXSUSP      ( 0x1 <<  8) // (UDP) USB Suspend Interrupt
#define AT91C_UDP_RXRSM       ( 0x1 <<  9) // (UDP) USB Resume Interrupt
#define AT91C_UDP_EXTRSM      ( 0x1 << 10) // (UDP) USB External Resume Interrupt
#define AT91C_UDP_SOFINT      ( 0x1 << 11) // (UDP) USB Start Of frame Interrupt
#define AT91C_UDP_WAKEUP      ( 0x1 << 13) // (UDP) USB Resume Interrupt
// -------- UDP_IDR : (UDP Offset: 0x14) USB Interrupt Disable Register --------
// -------- UDP_IMR : (UDP Offset: 0x18) USB Interrupt Mask Register --------
// -------- UDP_ISR : (UDP Offset: 0x1c) USB Interrupt Status Register --------
#define AT91C_UDP_ENDBUSRES   ( 0x1 << 12) // (UDP) USB End Of Bus Reset Interrupt
// -------- UDP_ICR : (UDP Offset: 0x20) USB Interrupt Clear Register --------
// -------- UDP_RST_EP : (UDP Offset: 0x28) USB Reset Endpoint Register --------
#define AT91C_UDP_EP0         ( 0x1 <<  0) // (UDP) Reset Endpoint 0
#define AT91C_UDP_EP1         ( 0x1 <<  1) // (UDP) Reset Endpoint 1
#define AT91C_UDP_EP2         ( 0x1 <<  2) // (UDP) Reset Endpoint 2
#define AT91C_UDP_EP3         ( 0x1 <<  3) // (UDP) Reset Endpoint 3
#define AT91C_UDP_EP4         ( 0x1 <<  4) // (UDP) Reset Endpoint 4
#define AT91C_UDP_EP5         ( 0x1 <<  5) // (UDP) Reset Endpoint 5
#define AT91C_UDP_EP6         ( 0x1 <<  6) // (UDP) Reset Endpoint 6
#define AT91C_UDP_EP7         ( 0x1 <<  7) // (UDP) Reset Endpoint 7
// -------- UDP_CSR : (UDP Offset: 0x30) USB Endpoint Control and Status Register --------
#define AT91C_UDP_TXCOMP      ( 0x1 <<  0) // (UDP) Generates an IN packet with data previously written in the DPR
#define AT91C_UDP_RX_DATA_BK0 ( 0x1 <<  1) // (UDP) Receive Data Bank 0
#define AT91C_UDP_RXSETUP     ( 0x1 <<  2) // (UDP) Sends STALL to the Host (Control endpoints)
#define AT91C_UDP_ISOERROR    ( 0x1 <<  3) // (UDP) Isochronous error (Isochronous endpoints)
#define AT91C_UDP_TXPKTRDY    ( 0x1 <<  4) // (UDP) Transmit Packet Ready
#define AT91C_UDP_FORCESTALL  ( 0x1 <<  5) // (UDP) Force Stall (used by Control, Bulk and Isochronous endpoints).
#define AT91C_UDP_RX_DATA_BK1 ( 0x1 <<  6) // (UDP) Receive Data Bank 1 (only used by endpoints with ping-pong attributes).
#define AT91C_UDP_DIR         ( 0x1 <<  7) // (UDP) Transfer Direction
#define AT91C_UDP_EPTYPE      ( 0x7 <<  8) // (UDP) Endpoint type
#define 	AT91C_UDP_EPTYPE_CTRL                 ( 0x0 <<  8) // (UDP) Control
#define 	AT91C_UDP_EPTYPE_ISO_OUT              ( 0x1 <<  8) // (UDP) Isochronous OUT
#define 	AT91C_UDP_EPTYPE_BULK_OUT             ( 0x2 <<  8) // (UDP) Bulk OUT
#define 	AT91C_UDP_EPTYPE_INT_OUT              ( 0x3 <<  8) // (UDP) Interrupt OUT
#define 	AT91C_UDP_EPTYPE_ISO_IN               ( 0x5 <<  8) // (UDP) Isochronous IN
#define 	AT91C_UDP_EPTYPE_BULK_IN              ( 0x6 <<  8) // (UDP) Bulk IN
#define 	AT91C_UDP_EPTYPE_INT_IN               ( 0x7 <<  8) // (UDP) Interrupt IN
#define AT91C_UDP_DTGLE       ( 0x1 << 11) // (UDP) Data Toggle
#define AT91C_UDP_EPEDS       ( 0x1 << 15) // (UDP) Endpoint Enable Disable
#define AT91C_UDP_RXBYTECNT   ( 0x7FF << 16) // (UDP) Number Of Bytes Available in the FIFO

#endif
