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
// Object              : AT91RM9200 / EMAC definitions
// Generated           : AT91 SW Application Group  01/17/2003 (13:41:21)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_EMAC_H
#define AT91RM9200_EMAC_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Ethernet MAC
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_EMAC {
	AT91_REG	 EMAC_CTL; 	// Network Control Register
	AT91_REG	 EMAC_CFG; 	// Network Configuration Register
	AT91_REG	 EMAC_SR; 	// Network Status Register
	AT91_REG	 EMAC_TAR; 	// Transmit Address Register
	AT91_REG	 EMAC_TCR; 	// Transmit Control Register
	AT91_REG	 EMAC_TSR; 	// Transmit Status Register
	AT91_REG	 EMAC_RBQP; 	// Receive Buffer Queue Pointer
	AT91_REG	 Reserved0[1]; 	//
	AT91_REG	 EMAC_RSR; 	// Receive Status Register
	AT91_REG	 EMAC_ISR; 	// Interrupt Status Register
	AT91_REG	 EMAC_IER; 	// Interrupt Enable Register
	AT91_REG	 EMAC_IDR; 	// Interrupt Disable Register
	AT91_REG	 EMAC_IMR; 	// Interrupt Mask Register
	AT91_REG	 EMAC_MAN; 	// PHY Maintenance Register
	AT91_REG	 Reserved1[2]; 	//
	AT91_REG	 EMAC_FRA; 	// Frames Transmitted OK Register
	AT91_REG	 EMAC_SCOL; 	// Single Collision Frame Register
	AT91_REG	 EMAC_MCOL; 	// Multiple Collision Frame Register
	AT91_REG	 EMAC_OK; 	// Frames Received OK Register
	AT91_REG	 EMAC_SEQE; 	// Frame Check Sequence Error Register
	AT91_REG	 EMAC_ALE; 	// Alignment Error Register
	AT91_REG	 EMAC_DTE; 	// Deferred Transmission Frame Register
	AT91_REG	 EMAC_LCOL; 	// Late Collision Register
	AT91_REG	 EMAC_ECOL; 	// Excessive Collision Register
	AT91_REG	 EMAC_CSE; 	// Carrier Sense Error Register
	AT91_REG	 EMAC_TUE; 	// Transmit Underrun Error Register
	AT91_REG	 EMAC_CDE; 	// Code Error Register
	AT91_REG	 EMAC_ELR; 	// Excessive Length Error Register
	AT91_REG	 EMAC_RJB; 	// Receive Jabber Register
	AT91_REG	 EMAC_USF; 	// Undersize Frame Register
	AT91_REG	 EMAC_SQEE; 	// SQE Test Error Register
	AT91_REG	 EMAC_DRFC; 	// Discarded RX Frame Register
	AT91_REG	 Reserved2[3]; 	//
	AT91_REG	 EMAC_HSH; 	// Hash Address High[63:32]
	AT91_REG	 EMAC_HSL; 	// Hash Address Low[31:0]
	AT91_REG	 EMAC_SA1L; 	// Specific Address 1 Low, First 4 bytes
	AT91_REG	 EMAC_SA1H; 	// Specific Address 1 High, Last 2 bytes
	AT91_REG	 EMAC_SA2L; 	// Specific Address 2 Low, First 4 bytes
	AT91_REG	 EMAC_SA2H; 	// Specific Address 2 High, Last 2 bytes
	AT91_REG	 EMAC_SA3L; 	// Specific Address 3 Low, First 4 bytes
	AT91_REG	 EMAC_SA3H; 	// Specific Address 3 High, Last 2 bytes
	AT91_REG	 EMAC_SA4L; 	// Specific Address 4 Low, First 4 bytes
	AT91_REG	 EMAC_SA4H; 	// Specific Address 4 High, Last 2 bytesr
} AT91S_EMAC, *AT91PS_EMAC;

#endif

// -------- EMAC_CTL : (EMAC Offset: 0x0)  --------
#define AT91C_EMAC_LB         ( 0x1 <<  0) // (EMAC) Loopback. Optional. When set, loopback signal is at high level.
#define AT91C_EMAC_LBL        ( 0x1 <<  1) // (EMAC) Loopback local.
#define AT91C_EMAC_RE         ( 0x1 <<  2) // (EMAC) Receive enable.
#define AT91C_EMAC_TE         ( 0x1 <<  3) // (EMAC) Transmit enable.
#define AT91C_EMAC_MPE        ( 0x1 <<  4) // (EMAC) Management port enable.
#define AT91C_EMAC_CSR        ( 0x1 <<  5) // (EMAC) Clear statistics registers.
#define AT91C_EMAC_ISR        ( 0x1 <<  6) // (EMAC) Increment statistics registers.
#define AT91C_EMAC_WES        ( 0x1 <<  7) // (EMAC) Write enable for statistics registers.
#define AT91C_EMAC_BP         ( 0x1 <<  8) // (EMAC) Back pressure.
// -------- EMAC_CFG : (EMAC Offset: 0x4) Network Configuration Register --------
#define AT91C_EMAC_SPD        ( 0x1 <<  0) // (EMAC) Speed.
#define AT91C_EMAC_FD         ( 0x1 <<  1) // (EMAC) Full duplex.
#define AT91C_EMAC_BR         ( 0x1 <<  2) // (EMAC) Bit rate.
#define AT91C_EMAC_CAF        ( 0x1 <<  4) // (EMAC) Copy all frames.
#define AT91C_EMAC_NBC        ( 0x1 <<  5) // (EMAC) No broadcast.
#define AT91C_EMAC_MTI        ( 0x1 <<  6) // (EMAC) Multicast hash enable
#define AT91C_EMAC_UNI        ( 0x1 <<  7) // (EMAC) Unicast hash enable.
#define AT91C_EMAC_BIG        ( 0x1 <<  8) // (EMAC) Receive 1522 bytes.
#define AT91C_EMAC_EAE        ( 0x1 <<  9) // (EMAC) External address match enable.
#define AT91C_EMAC_CLK        ( 0x3 << 10) // (EMAC)
#define 	AT91C_EMAC_CLK_HCLK_8               ( 0x0 << 10) // (EMAC) HCLK divided by 8
#define 	AT91C_EMAC_CLK_HCLK_16              ( 0x1 << 10) // (EMAC) HCLK divided by 16
#define 	AT91C_EMAC_CLK_HCLK_32              ( 0x2 << 10) // (EMAC) HCLK divided by 32
#define 	AT91C_EMAC_CLK_HCLK_64              ( 0x3 << 10) // (EMAC) HCLK divided by 64
#define AT91C_EMAC_RTY        ( 0x1 << 12) // (EMAC)
#define AT91C_EMAC_RMII       ( 0x1 << 13) // (EMAC)
// -------- EMAC_SR : (EMAC Offset: 0x8) Network Status Register --------
#define AT91C_EMAC_MDIO       ( 0x1 <<  1) // (EMAC)
#define AT91C_EMAC_IDLE       ( 0x1 <<  2) // (EMAC)
// -------- EMAC_TCR : (EMAC Offset: 0x10) Transmit Control Register --------
#define AT91C_EMAC_LEN        ( 0x7FF <<  0) // (EMAC)
#define AT91C_EMAC_NCRC       ( 0x1 << 15) // (EMAC)
// -------- EMAC_TSR : (EMAC Offset: 0x14) Transmit Control Register --------
#define AT91C_EMAC_OVR        ( 0x1 <<  0) // (EMAC)
#define AT91C_EMAC_COL        ( 0x1 <<  1) // (EMAC)
#define AT91C_EMAC_RLE        ( 0x1 <<  2) // (EMAC)
#define AT91C_EMAC_TXIDLE     ( 0x1 <<  3) // (EMAC)
#define AT91C_EMAC_BNQ        ( 0x1 <<  4) // (EMAC)
#define AT91C_EMAC_COMP       ( 0x1 <<  5) // (EMAC)
#define AT91C_EMAC_UND        ( 0x1 <<  6) // (EMAC)
// -------- EMAC_RSR : (EMAC Offset: 0x20) Receive Status Register --------
#define AT91C_EMAC_BNA        ( 0x1 <<  0) // (EMAC)
#define AT91C_EMAC_REC        ( 0x1 <<  1) // (EMAC)
// -------- EMAC_ISR : (EMAC Offset: 0x24) Interrupt Status Register --------
#define AT91C_EMAC_DONE       ( 0x1 <<  0) // (EMAC)
#define AT91C_EMAC_RCOM       ( 0x1 <<  1) // (EMAC)
#define AT91C_EMAC_RBNA       ( 0x1 <<  2) // (EMAC)
#define AT91C_EMAC_TOVR       ( 0x1 <<  3) // (EMAC)
#define AT91C_EMAC_TUND       ( 0x1 <<  4) // (EMAC)
#define AT91C_EMAC_RTRY       ( 0x1 <<  5) // (EMAC)
#define AT91C_EMAC_TBRE       ( 0x1 <<  6) // (EMAC)
#define AT91C_EMAC_TCOM       ( 0x1 <<  7) // (EMAC)
#define AT91C_EMAC_TIDLE      ( 0x1 <<  8) // (EMAC)
#define AT91C_EMAC_LINK       ( 0x1 <<  9) // (EMAC)
#define AT91C_EMAC_ROVR       ( 0x1 << 10) // (EMAC)
#define AT91C_EMAC_HRESP      ( 0x1 << 11) // (EMAC)
// -------- EMAC_IER : (EMAC Offset: 0x28) Interrupt Enable Register --------
// -------- EMAC_IDR : (EMAC Offset: 0x2c) Interrupt Disable Register --------
// -------- EMAC_IMR : (EMAC Offset: 0x30) Interrupt Mask Register --------
// -------- EMAC_MAN : (EMAC Offset: 0x34) PHY Maintenance Register --------
#define AT91C_EMAC_DATA       ( 0xFFFF <<  0) // (EMAC)
#define AT91C_EMAC_CODE       ( 0x3 << 16) // (EMAC)
#define         AT91C_EMAC_CODE_802_3 ( 0x2 << 16) // (EMAC) Write Operation
#define AT91C_EMAC_REGA       ( 0x1F << 18) // (EMAC)
#define AT91C_EMAC_PHYA       ( 0x1F << 23) // (EMAC)
#define AT91C_EMAC_RW         ( 0x3 << 28) // (EMAC)
#define         AT91C_EMAC_RW_R       ( 0x2 << 28) // (EMAC) Read Operation
#define         AT91C_EMAC_RW_W       ( 0x1 << 28) // (EMAC) Write Operation
#define AT91C_EMAC_HIGH       ( 0x1 << 30) // (EMAC)
#define AT91C_EMAC_LOW        ( 0x1 << 31) // (EMAC)

#endif
