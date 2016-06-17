/*
 * include/linux/serial_reg.h
 *
 * Copyright (C) 1992, 1994 by Theodore Ts'o.
 * 
 * Redistribution of this file is permitted under the terms of the GNU 
 * Public License (GPL)
 * 
 * These are the UART port assignments, expressed as offsets from the base
 * register.  These assignments should hold for any serial port based on
 * a 8250, 16450, or 16550(A).
 */

#ifndef _LINUX_SERIAL_REG_H
#define _LINUX_SERIAL_REG_H

#define UART_RX		0	/* In:  Receive buffer (DLAB=0) */
#define UART_TX		0	/* Out: Transmit buffer (DLAB=0) */
#define UART_DLL	0	/* Out: Divisor Latch Low (DLAB=1) */
#define UART_TRG	0	/* (LCR=BF) FCTR bit 7 selects Rx or Tx
				 * In: Fifo count
				 * Out: Fifo custom trigger levels
				 * XR16C85x only */

#define UART_DLM	1	/* Out: Divisor Latch High (DLAB=1) */
#define UART_IER	1	/* Out: Interrupt Enable Register */
#define UART_FCTR	1	/* (LCR=BF) Feature Control Register
				 * XR16C85x only */

#define UART_IIR	2	/* In:  Interrupt ID Register */
#define UART_FCR	2	/* Out: FIFO Control Register */
#define UART_EFR	2	/* I/O: Extended Features Register */
				/* (DLAB=1, 16C660 only) */

#define UART_LCR	3	/* Out: Line Control Register */
#define UART_MCR	4	/* Out: Modem Control Register */
#define UART_LSR	5	/* In:  Line Status Register */
#define UART_MSR	6	/* In:  Modem Status Register */
#define UART_SCR	7	/* I/O: Scratch Register */
#define UART_EMSR	7	/* (LCR=BF) Extended Mode Select Register 
				 * FCTR bit 6 selects SCR or EMSR
				 * XR16c85x only */

/*
 * These are the definitions for the FIFO Control Register
 * (16650 only)
 */
#define UART_FCR_ENABLE_FIFO	0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR	0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT	0x04 /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT	0x08 /* For DMA applications */
#define UART_FCR_TRIGGER_MASK	0xC0 /* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1	0x00 /* Mask for trigger set at 1 */
#define UART_FCR_TRIGGER_4	0x40 /* Mask for trigger set at 4 */
#define UART_FCR_TRIGGER_8	0x80 /* Mask for trigger set at 8 */
#define UART_FCR_TRIGGER_14	0xC0 /* Mask for trigger set at 14 */
/* 16650 redefinitions */
#define UART_FCR6_R_TRIGGER_8	0x00 /* Mask for receive trigger set at 1 */
#define UART_FCR6_R_TRIGGER_16	0x40 /* Mask for receive trigger set at 4 */
#define UART_FCR6_R_TRIGGER_24  0x80 /* Mask for receive trigger set at 8 */
#define UART_FCR6_R_TRIGGER_28	0xC0 /* Mask for receive trigger set at 14 */
#define UART_FCR6_T_TRIGGER_16	0x00 /* Mask for transmit trigger set at 16 */
#define UART_FCR6_T_TRIGGER_8	0x10 /* Mask for transmit trigger set at 8 */
#define UART_FCR6_T_TRIGGER_24  0x20 /* Mask for transmit trigger set at 24 */
#define UART_FCR6_T_TRIGGER_30	0x30 /* Mask for transmit trigger set at 30 */
/* TI 16750 definitions */
#define UART_FCR7_64BYTE	0x20 /* Go into 64 byte mode */

/*
 * These are the definitions for the Line Control Register
 * 
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting 
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_DLAB	0x80	/* Divisor latch access bit */
#define UART_LCR_SBC	0x40	/* Set break control */
#define UART_LCR_SPAR	0x20	/* Stick parity (?) */
#define UART_LCR_EPAR	0x10	/* Even parity select */
#define UART_LCR_PARITY	0x08	/* Parity Enable */
#define UART_LCR_STOP	0x04	/* Stop bits: 0=1 stop bit, 1= 2 stop bits */
#define UART_LCR_WLEN5  0x00	/* Wordlength: 5 bits */
#define UART_LCR_WLEN6  0x01	/* Wordlength: 6 bits */
#define UART_LCR_WLEN7  0x02	/* Wordlength: 7 bits */
#define UART_LCR_WLEN8  0x03	/* Wordlength: 8 bits */

/*
 * These are the definitions for the Line Status Register
 */
#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */

/*
 * These are the definitions for the Interrupt Identification Register
 */
#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */

#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */

/*
 * These are the definitions for the Interrupt Enable Register
 */
#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */
/*
 * Sleep mode for ST16650 and TI16750.
 * Note that for 16650, EFR-bit 4 must be selected as well.
 */
#define UART_IERX_SLEEP  0x10	/* Enable sleep mode */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD	0x80	/* Data Carrier Detect */
#define UART_MSR_RI	0x40	/* Ring Indicator */
#define UART_MSR_DSR	0x20	/* Data Set Ready */
#define UART_MSR_CTS	0x10	/* Clear to Send */
#define UART_MSR_DDCD	0x08	/* Delta DCD */
#define UART_MSR_TERI	0x04	/* Trailing edge ring indicator */
#define UART_MSR_DDSR	0x02	/* Delta DSR */
#define UART_MSR_DCTS	0x01	/* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F	/* Any of the delta bits! */

/*
 * These are the definitions for the Extended Features Register
 * (StarTech 16C660 only, when DLAB=1)
 */
#define UART_EFR_CTS	0x80	/* CTS flow control */
#define UART_EFR_RTS	0x40	/* RTS flow control */
#define UART_EFR_SCD	0x20	/* Special character detect */
#define UART_EFR_ECB	0x10	/* Enhanced control bit */
/*
 * the low four bits control software flow control
 */

/*
 * These register definitions are for the 16C950
 */
#define UART_ASR	0x01	/* Additional Status Register */
#define UART_RFL	0x03	/* Receiver FIFO level */
#define UART_TFL 	0x04	/* Transmitter FIFO level */
#define UART_ICR	0x05	/* Index Control Register */

/* The 16950 ICR registers */
#define UART_ACR	0x00	/* Additional Control Register */
#define UART_CPR	0x01	/* Clock Prescalar Register */
#define UART_TCR	0x02	/* Times Clock Register */
#define UART_CKS	0x03	/* Clock Select Register */
#define UART_TTL	0x04	/* Transmitter Interrupt Trigger Level */
#define UART_RTL	0x05	/* Receiver Interrupt Trigger Level */
#define UART_FCL	0x06	/* Flow Control Level Lower */
#define UART_FCH	0x07	/* Flow Control Level Higher */
#define UART_ID1	0x08	/* ID #1 */
#define UART_ID2	0x09	/* ID #2 */
#define UART_ID3	0x0A	/* ID #3 */
#define UART_REV	0x0B	/* Revision */
#define UART_CSR	0x0C	/* Channel Software Reset */
#define UART_NMR	0x0D	/* Nine-bit Mode Register */
#define UART_CTR	0xFF

/*
 * The 16C950 Additional Control Reigster
 */
#define UART_ACR_RXDIS	0x01	/* Receiver disable */
#define UART_ACR_TXDIS	0x02	/* Receiver disable */
#define UART_ACR_DSRFC	0x04	/* DSR Flow Control */
#define UART_ACR_TLENB	0x20	/* 950 trigger levels enable */
#define UART_ACR_ICRRD	0x40	/* ICR Read enable */
#define UART_ACR_ASREN	0x80	/* Additional status enable */

/*
 * These are the definitions for the Feature Control Register
 * (XR16C85x only, when LCR=bf; doubles with the Interrupt Enable
 * Register, UART register #1)
 */
#define UART_FCTR_RTS_NODELAY	0x00  /* RTS flow control delay */
#define UART_FCTR_RTS_4DELAY	0x01
#define UART_FCTR_RTS_6DELAY	0x02
#define UART_FCTR_RTS_8DELAY	0x03
#define UART_FCTR_IRDA	0x04  /* IrDa data encode select */
#define UART_FCTR_TX_INT	0x08  /* Tx interrupt type select */
#define UART_FCTR_TRGA	0x00  /* Tx/Rx 550 trigger table select */
#define UART_FCTR_TRGB	0x10  /* Tx/Rx 650 trigger table select */
#define UART_FCTR_TRGC	0x20  /* Tx/Rx 654 trigger table select */
#define UART_FCTR_TRGD	0x30  /* Tx/Rx 850 programmable trigger select */
#define UART_FCTR_SCR_SWAP	0x40  /* Scratch pad register swap */
#define UART_FCTR_RX	0x00  /* Programmable trigger mode select */
#define UART_FCTR_TX	0x80  /* Programmable trigger mode select */

/*
 * These are the definitions for the Enhanced Mode Select Register
 * (XR16C85x only, when LCR=bf and FCTR bit 6=1; doubles with the
 * Scratch register, UART register #7)
 */
#define UART_EMSR_FIFO_COUNT	0x01  /* Rx/Tx select */
#define UART_EMSR_ALT_COUNT	0x02  /* Alternating count select */

/*
 * These are the definitions for the Programmable Trigger
 * Register (XR16C85x only, when LCR=bf; doubles with the UART RX/TX
 * register, UART register #0)
 */
#define UART_TRG_1	0x01
#define UART_TRG_4	0x04
#define UART_TRG_8	0x08
#define UART_TRG_16	0x10
#define UART_TRG_32	0x20
#define UART_TRG_64	0x40
#define UART_TRG_96	0x60
#define UART_TRG_120	0x78
#define UART_TRG_128	0x80

/*
 * These definitions are for the RSA-DV II/S card, from
 *
 * Kiyokazu SUTO <suto@ks-and-ks.ne.jp>
 */

#define UART_RSA_BASE (-8)

#define UART_RSA_MSR ((UART_RSA_BASE) + 0) /* I/O: Mode Select Register */

#define UART_RSA_MSR_SWAP (1 << 0) /* Swap low/high 8 bytes in I/O port addr */
#define UART_RSA_MSR_FIFO (1 << 2) /* Enable the external FIFO */
#define UART_RSA_MSR_FLOW (1 << 3) /* Enable the auto RTS/CTS flow control */
#define UART_RSA_MSR_ITYP (1 << 4) /* Level (1) / Edge triger (0) */

#define UART_RSA_IER ((UART_RSA_BASE) + 1) /* I/O: Interrupt Enable Register */

#define UART_RSA_IER_Rx_FIFO_H (1 << 0) /* Enable Rx FIFO half full int. */
#define UART_RSA_IER_Tx_FIFO_H (1 << 1) /* Enable Tx FIFO half full int. */
#define UART_RSA_IER_Tx_FIFO_E (1 << 2) /* Enable Tx FIFO empty int. */
#define UART_RSA_IER_Rx_TOUT (1 << 3) /* Enable char receive timeout int */
#define UART_RSA_IER_TIMER (1 << 4) /* Enable timer interrupt */

#define UART_RSA_SRR ((UART_RSA_BASE) + 2) /* IN: Status Read Register */

#define UART_RSA_SRR_Tx_FIFO_NEMP (1 << 0) /* Tx FIFO is not empty (1) */
#define UART_RSA_SRR_Tx_FIFO_NHFL (1 << 1) /* Tx FIFO is not half full (1) */
#define UART_RSA_SRR_Tx_FIFO_NFUL (1 << 2) /* Tx FIFO is not full (1) */
#define UART_RSA_SRR_Rx_FIFO_NEMP (1 << 3) /* Rx FIFO is not empty (1) */
#define UART_RSA_SRR_Rx_FIFO_NHFL (1 << 4) /* Rx FIFO is not half full (1) */
#define UART_RSA_SRR_Rx_FIFO_NFUL (1 << 5) /* Rx FIFO is not full (1) */
#define UART_RSA_SRR_Rx_TOUT (1 << 6) /* Character reception timeout occurred (1) */
#define UART_RSA_SRR_TIMER (1 << 7) /* Timer interrupt occurred */

#define UART_RSA_FRR ((UART_RSA_BASE) + 2) /* OUT: FIFO Reset Register */

#define UART_RSA_TIVSR ((UART_RSA_BASE) + 3) /* I/O: Timer Interval Value Set Register */

#define UART_RSA_TCR ((UART_RSA_BASE) + 4) /* OUT: Timer Control Register */

#define UART_RSA_TCR_SWITCH (1 << 0) /* Timer on */

/*
 * The RSA DSV/II board has two fixed clock frequencies.  One is the
 * standard rate, and the other is 8 times faster.
 */
#define SERIAL_RSA_BAUD_BASE (921600)
#define SERIAL_RSA_BAUD_BASE_LO (SERIAL_RSA_BAUD_BASE / 8)

#endif /* _LINUX_SERIAL_REG_H */

