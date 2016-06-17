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
// Object              : AT91RM9200 / USART definitions
// Generated           : AT91 SW Application Group  01/17/2003 (13:41:22)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_USART_H
#define AT91RM9200_USART_H

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Usart
// *****************************************************************************
#ifndef __ASSEMBLY__

typedef struct _AT91S_USART {
	AT91_REG	 US_CR; 	// Control Register
	AT91_REG	 US_MR; 	// Mode Register
	AT91_REG	 US_IER; 	// Interrupt Enable Register
	AT91_REG	 US_IDR; 	// Interrupt Disable Register
	AT91_REG	 US_IMR; 	// Interrupt Mask Register
	AT91_REG	 US_CSR; 	// Channel Status Register
	AT91_REG	 US_RHR; 	// Receiver Holding Register
	AT91_REG	 US_THR; 	// Transmitter Holding Register
	AT91_REG	 US_BRGR; 	// Baud Rate Generator Register
	AT91_REG	 US_RTOR; 	// Receiver Time-out Register
	AT91_REG	 US_TTGR; 	// Transmitter Time-guard Register
	AT91_REG	 Reserved0[5]; 	//
	AT91_REG	 US_FIDI; 	// FI_DI_Ratio Register
	AT91_REG	 US_NER; 	// Nb Errors Register
	AT91_REG	 US_XXR; 	// XON_XOFF Register
	AT91_REG	 US_IF; 	// IRDA_FILTER Register
	AT91_REG	 Reserved1[44]; //
	AT91_REG	 US_RPR; 	// Receive Pointer Register
	AT91_REG	 US_RCR; 	// Receive Counter Register
	AT91_REG	 US_TPR; 	// Transmit Pointer Register
	AT91_REG	 US_TCR; 	// Transmit Counter Register
	AT91_REG	 US_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 US_RNCR; 	// Receive Next Counter Register
	AT91_REG	 US_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 US_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 US_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 US_PTSR; 	// PDC Transfer Status Register
} AT91S_USART, *AT91PS_USART;

#endif

// -------- US_CR : (USART Offset: 0x0) Debug Unit Control Register --------
#define AT91C_US_RSTRX        ( 0x1 <<  2) // (USART) Reset Receiver
#define AT91C_US_RSTTX        ( 0x1 <<  3) // (USART) Reset Transmitter
#define AT91C_US_RXEN	      ( 0x1 <<  4) // (USART) Receiver Enable
#define AT91C_US_RXDIS        ( 0x1 <<  5) // (USART) Receiver Disable
#define AT91C_US_TXEN	      ( 0x1 <<  6) // (USART) Transmitter Enable
#define AT91C_US_TXDIS        ( 0x1 <<  7) // (USART) Transmitter Disable
#define AT91C_US_RSTSTA       ( 0x1 <<  8) // (USART) Reset Status Bits
#define AT91C_US_STTBRK       ( 0x1 <<  9) // (USART) Start Break
#define AT91C_US_STPBRK       ( 0x1 << 10) // (USART) Stop Break
#define AT91C_US_STTTO        ( 0x1 << 11) // (USART) Start Time-out
#define AT91C_US_SENDA        ( 0x1 << 12) // (USART) Send Address
#define AT91C_US_RSTIT        ( 0x1 << 13) // (USART) Reset Iterations
#define AT91C_US_RSTNACK      ( 0x1 << 14) // (USART) Reset Non Acknowledge
#define AT91C_US_RETTO        ( 0x1 << 15) // (USART) Rearm Time-out
#define AT91C_US_DTREN        ( 0x1 << 16) // (USART) Data Terminal ready Enable
#define AT91C_US_DTRDIS       ( 0x1 << 17) // (USART) Data Terminal ready Disable
#define AT91C_US_RTSEN        ( 0x1 << 18) // (USART) Request to Send enable
#define AT91C_US_RTSDIS       ( 0x1 << 19) // (USART) Request to Send Disable
// -------- US_MR : (USART Offset: 0x4) Debug Unit Mode Register --------
#define AT91C_US_USMODE       ( 0xF <<  0) // (USART) Usart mode
#define 	AT91C_US_USMODE_NORMAL               ( 0x0) // (USART) Normal
#define 	AT91C_US_USMODE_RS485                ( 0x1) // (USART) RS485
#define 	AT91C_US_USMODE_HWHSH                ( 0x2) // (USART) Hardware Handshaking
#define 	AT91C_US_USMODE_MODEM                ( 0x3) // (USART) Modem
#define 	AT91C_US_USMODE_ISO7816_0            ( 0x4) // (USART) ISO7816 protocol: T = 0
#define 	AT91C_US_USMODE_ISO7816_1            ( 0x6) // (USART) ISO7816 protocol: T = 1
#define 	AT91C_US_USMODE_IRDA                 ( 0x8) // (USART) IrDA
#define 	AT91C_US_USMODE_SWHSH                ( 0xC) // (USART) Software Handshaking
#define AT91C_US_CLKS         ( 0x3 <<  4) // (USART) Clock Selection (Baud Rate generator Input Clock
#define 	AT91C_US_CLKS_CLOCK                ( 0x0 <<  4) // (USART) Clock
#define 	AT91C_US_CLKS_FDIV1                ( 0x1 <<  4) // (USART) fdiv1
#define 	AT91C_US_CLKS_SLOW                 ( 0x2 <<  4) // (USART) slow_clock (ARM)
#define 	AT91C_US_CLKS_EXT                  ( 0x3 <<  4) // (USART) External (SCK)
#define AT91C_US_CHRL         ( 0x3 <<  6) // (USART) Clock Selection (Baud Rate generator Input Clock
#define 	AT91C_US_CHRL_5_BITS               ( 0x0 <<  6) // (USART) Character Length: 5 bits
#define 	AT91C_US_CHRL_6_BITS               ( 0x1 <<  6) // (USART) Character Length: 6 bits
#define 	AT91C_US_CHRL_7_BITS               ( 0x2 <<  6) // (USART) Character Length: 7 bits
#define 	AT91C_US_CHRL_8_BITS               ( 0x3 <<  6) // (USART) Character Length: 8 bits
#define AT91C_US_SYNC         ( 0x1 <<  8) // (USART) Synchronous Mode Select
#define AT91C_US_PAR          ( 0x7 <<  9) // (USART) Parity type
#define		AT91C_US_PAR_EVEN                 ( 0x0 <<  9) // (USART) Even Parity
#define		AT91C_US_PAR_ODD                  ( 0x1 <<  9) // (USART) Odd Parity
#define 	AT91C_US_PAR_SPACE                ( 0x2 <<  9) // (USART) Parity forced to 0 (Space)
#define		AT91C_US_PAR_MARK                 ( 0x3 <<  9) // (USART) Parity forced to 1 (Mark)
#define		AT91C_US_PAR_NONE                 ( 0x4 <<  9) // (USART) No Parity
#define		AT91C_US_PAR_MULTI_DROP           ( 0x6 <<  9) // (USART) Multi-drop mode
#define AT91C_US_NBSTOP       ( 0x3 << 12) // (USART) Number of Stop bits
#define 	AT91C_US_NBSTOP_1_BIT                ( 0x0 << 12) // (USART) 1 stop bit
#define 	AT91C_US_NBSTOP_15_BIT               ( 0x1 << 12) // (USART) Asynchronous (SYNC=0) 2 stop bits Synchronous (SYNC=1) 2 stop bits
#define 	AT91C_US_NBSTOP_2_BIT                ( 0x2 << 12) // (USART) 2 stop bits
#define AT91C_US_CHMODE       ( 0x3 << 14) // (USART) Channel Mode
#define 	AT91C_US_CHMODE_NORMAL               ( 0x0 << 14) // (USART) Normal Mode: The USART channel operates as an RX/TX USART.
#define 	AT91C_US_CHMODE_AUTO                 ( 0x1 << 14) // (USART) Automatic Echo: Receiver Data Input is connected to the TXD pin.
#define 	AT91C_US_CHMODE_LOCAL                ( 0x2 << 14) // (USART) Local Loopback: Transmitter Output Signal is connected to Receiver Input Signal.
#define 	AT91C_US_CHMODE_REMOTE               ( 0x3 << 14) // (USART) Remote Loopback: RXD pin is internally connected to TXD pin.
#define AT91C_US_MSBF         ( 0x1 << 16) // (USART) Bit Order
#define AT91C_US_MODE9        ( 0x1 << 17) // (USART) 9-bit Character length
#define AT91C_US_CKLO         ( 0x1 << 18) // (USART) Clock Output Select
#define AT91C_US_OVER         ( 0x1 << 19) // (USART) Over Sampling Mode
#define AT91C_US_INACK        ( 0x1 << 20) // (USART) Inhibit Non Acknowledge
#define AT91C_US_DSNACK       ( 0x1 << 21) // (USART) Disable Successive NACK
#define AT91C_US_MAX_ITER     ( 0x1 << 24) // (USART) Number of Repetitions
#define AT91C_US_FILTER       ( 0x1 << 28) // (USART) Receive Line Filter
// -------- US_IER : (USART Offset: 0x8) Debug Unit Interrupt Enable Register --------
#define AT91C_US_RXRDY        ( 0x1 <<  0) // (USART) RXRDY Interrupt
#define AT91C_US_TXRDY        ( 0x1 <<  1) // (USART) TXRDY Interrupt
#define AT91C_US_RXBRK        ( 0x1 <<  2) // (USART) Break Received/End of Break
#define AT91C_US_ENDRX        ( 0x1 <<  3) // (USART) End of Receive Transfer Interrupt
#define AT91C_US_ENDTX        ( 0x1 <<  4) // (USART) End of Transmit Interrupt
#define AT91C_US_OVRE         ( 0x1 <<  5) // (USART) Overrun Interrupt
#define AT91C_US_FRAME        ( 0x1 <<  6) // (USART) Framing Error Interrupt
#define AT91C_US_PARE         ( 0x1 <<  7) // (USART) Parity Error Interrupt
#define AT91C_US_TIMEOUT      ( 0x1 <<  8) // (USART) Receiver Time-out
#define AT91C_US_TXEMPTY      ( 0x1 <<  9) // (USART) TXEMPTY Interrupt
#define AT91C_US_ITERATION    ( 0x1 << 10) // (USART) Max number of Repetitions Reached
#define AT91C_US_TXBUFE       ( 0x1 << 11) // (USART) TXBUFE Interrupt
#define AT91C_US_RXBUFF       ( 0x1 << 12) // (USART) RXBUFF Interrupt
#define AT91C_US_NACK         ( 0x1 << 13) // (USART) Non Acknowledge
#define AT91C_US_RIIC         ( 0x1 << 16) // (USART) Ring INdicator Input Change Flag
#define AT91C_US_DSRIC        ( 0x1 << 17) // (USART) Data Set Ready Input Change Flag
#define AT91C_US_DCDIC        ( 0x1 << 18) // (USART) Data Carrier Flag
#define AT91C_US_CTSIC        ( 0x1 << 19) // (USART) Clear To Send Input Change Flag
#define AT91C_US_COMM_TX      ( 0x1 << 30) // (USART) COMM_TX Interrupt
#define AT91C_US_COMM_RX      ( 0x1 << 31) // (USART) COMM_RX Interrupt
// -------- US_IDR : (USART Offset: 0xc) Debug Unit Interrupt Disable Register --------
// -------- US_IMR : (USART Offset: 0x10) Debug Unit Interrupt Mask Register --------
// -------- US_CSR : (USART Offset: 0x14) Debug Unit Channel Status Register --------
#define AT91C_US_RI           ( 0x1 << 20) // (USART) Image of RI Input
#define AT91C_US_DSR          ( 0x1 << 21) // (USART) Image of DSR Input
#define AT91C_US_DCD          ( 0x1 << 22) // (USART) Image of DCD Input
#define AT91C_US_CTS          ( 0x1 << 23) // (USART) Image of CTS Input

#endif
