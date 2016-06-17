/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_IA64_SN_UART16550_H
#define _ASM_IA64_SN_UART16550_H


/*
 * Definitions for 16550  chip
 */

	/* defined as offsets from the data register */
#define REG_DAT     0   /* receive/transmit data */
#define REG_ICR     1   /* interrupt control register */
#define REG_ISR     2   /* interrupt status register */
#define REG_FCR     2   /* fifo control register */
#define REG_LCR     3   /* line control register */
#define REG_MCR     4   /* modem control register */
#define REG_LSR     5   /* line status register */
#define REG_MSR     6   /* modem status register */
#define REG_SCR     7   /* Scratch register      */
#define REG_DLL     0   /* divisor latch (lsb) */
#define REG_DLH     1   /* divisor latch (msb) */
#define REG_EFR		2	/* 16650 enhanced feature register */

/*
 * 16450/16550 Registers Structure.
 */

/* Line Control Register */
#define		LCR_WLS0	0x01	/*word length select bit 0 */	
#define		LCR_WLS1	0x02	/*word length select bit 2 */	
#define		LCR_STB	0x04		/* number of stop bits */
#define		LCR_PEN	0x08		/* parity enable */
#define		LCR_EPS	0x10		/* even parity select */
#define		LCR_SETBREAK 0x40	/* break key */
#define		LCR_DLAB	0x80	/* divisor latch access bit */
#define 	LCR_RXLEN   0x03    /* # of data bits per received/xmitted char */
#define 	LCR_STOP1   0x00
#define 	LCR_STOP2   0x04
#define 	LCR_PAREN   0x08
#define 	LCR_PAREVN  0x10
#define 	LCR_PARMARK 0x20
#define 	LCR_SNDBRK  0x40
#define 	LCR_DLAB    0x80


#define		LCR_BITS5	0x00	/* 5 bits per char */
#define		LCR_BITS6	0x01	/* 6 bits per char */
#define		LCR_BITS7	0x02	/* 7 bits per char */
#define		LCR_BITS8	0x03	/* 8 bits per char */

#define		LCR_1_STOP_BITS	0x00	/* 1 stop bit */
#define		LCR_2_STOP_BITS	0x04	/* 2 stop bits */

#define		LCR_MASK_BITS_CHAR 		0x03
#define 	LCR_MASK_STOP_BITS		0x04
#define		LCR_MASK_PARITY_BITS	0x18


/* Line Status Register */
#define		LSR_RCA	0x01		/* data ready */
#define		LSR_OVRRUN	0x02	/* overrun error */
#define		LSR_PARERR	0x04	/* parity error */
#define		LSR_FRMERR	0x08	/* framing error */
#define		LSR_BRKDET 	0x10	/* a break has arrived */
#define		LSR_XHRE	0x20	/* tx hold reg is now empty */
#define		LSR_XSRE	0x40	/* tx shift reg is now empty */
#define		LSR_RFBE	0x80	/* rx FIFO Buffer error */

/* Interrupt Status Regisger */
#define		ISR_MSTATUS	0x00
#define		ISR_TxRDY	0x02
#define		ISR_RxRDY	0x04
#define		ISR_ERROR_INTR	0x08
#define		ISR_FFTMOUT 0x0c	/* FIFO Timeout */
#define		ISR_RSTATUS 0x06	/* Receiver Line status */

/* Interrupt Enable Register */
#define		ICR_RIEN	0x01	/* Received Data Ready */
#define		ICR_TIEN	0x02	/* Tx Hold Register Empty */
#define		ICR_SIEN	0x04	/* Receiver Line Status */
#define		ICR_MIEN	0x08	/* Modem Status */

/* Modem Control Register */
#define		MCR_DTR		0x01	/* Data Terminal Ready */
#define		MCR_RTS		0x02	/* Request To Send */
#define		MCR_OUT1	0x04	/* Aux output - not used */
#define		MCR_OUT2	0x08	/* turns intr to 386 on/off */	
#define		MCR_LOOP	0x10	/* loopback for diagnostics */
#define		MCR_AFE 	0x20	/* Auto flow control enable */

/* Modem Status Register */
#define		MSR_DCTS	0x01	/* Delta Clear To Send */
#define		MSR_DDSR	0x02	/* Delta Data Set Ready */
#define		MSR_DRI		0x04	/* Trail Edge Ring Indicator */
#define		MSR_DDCD	0x08	/* Delta Data Carrier Detect */
#define		MSR_CTS		0x10	/* Clear To Send */
#define		MSR_DSR		0x20	/* Data Set Ready */
#define		MSR_RI		0x40	/* Ring Indicator */
#define		MSR_DCD		0x80	/* Data Carrier Detect */

#define 	DELTAS(x) 	((x)&(MSR_DCTS|MSR_DDSR|MSR_DRI|MSR_DDCD))
#define 	STATES(x) 	((x)(MSR_CTS|MSR_DSR|MSR_RI|MSR_DCD))


#define		FCR_FIFOEN	0x01	/* enable receive/transmit fifo */
#define		FCR_RxFIFO	0x02	/* enable receive fifo */
#define		FCR_TxFIFO	0x04	/* enable transmit fifo */
#define 	FCR_MODE1	0x08	/* change to mode 1 */
#define		RxLVL0		0x00	/* Rx fifo level at 1	*/
#define		RxLVL1		0x40	/* Rx fifo level at 4 */
#define		RxLVL2		0x80	/* Rx fifo level at 8 */
#define		RxLVL3		0xc0	/* Rx fifo level at 14 */

#define 	FIFOEN		(FCR_FIFOEN | FCR_RxFIFO | FCR_TxFIFO | RxLVL3 | FCR_MODE1) 

#define		FCT_TxMASK	0x30	/* mask for Tx trigger */
#define		FCT_RxMASK	0xc0	/* mask for Rx trigger */

/* enhanced festures register */
#define		EFR_SFLOW	0x0f	/* various S/w Flow Controls */
#define 	EFR_EIC		0x10	/* Enhanced Interrupt Control bit */
#define 	EFR_SCD		0x20	/* Special Character Detect */
#define 	EFR_RTS		0x40	/* RTS flow control */
#define 	EFR_CTS		0x80	/* CTS flow control */

/* Rx Tx software flow controls in 16650 enhanced mode */
#define		SFLOW_Tx0	0x00	/* no Xmit flow control */
#define		SFLOW_Tx1	0x08	/* Transmit Xon1, Xoff1 */
#define		SFLOW_Tx2	0x04	/* Transmit Xon2, Xoff2 */
#define		SFLOW_Tx3	0x0c	/* Transmit Xon1,Xon2, Xoff1,Xoff2 */
#define		SFLOW_Rx0	0x00	/* no Rcv flow control */
#define		SFLOW_Rx1	0x02	/* Receiver compares Xon1, Xoff1 */
#define		SFLOW_Rx2	0x01	/* Receiver compares Xon2, Xoff2 */

#define	ASSERT_DTR(x)		(x |= MCR_DTR)
#define	ASSERT_RTS(x)		(x |= MCR_RTS)
#define DU_RTS_ASSERTED(x)  (((x) & MCR_RTS) != 0)
#define DU_RTS_ASSERT(x)    ((x) |= MCR_RTS)
#define DU_RTS_DEASSERT(x)  ((x) &= ~MCR_RTS)

#define SER_DIVISOR(x, clk)		(((clk) + (x) * 8) / ((x) * 16))
#define DIVISOR_TO_BAUD(div, clk)	((clk) / 16 / (div))


/*
 * ioctl(fd, I_STR, arg)
 * use the SIOC_RS422 and SIOC_EXTCLK combination to support MIDI
 */
#define SIOC        ('z' << 8)  /* z for z85130 */
#define SIOC_EXTCLK (SIOC | 1)  /* select/de-select external clock */
#define SIOC_RS422  (SIOC | 2)  /* select/de-select RS422 protocol */
#define SIOC_ITIMER (SIOC | 3)  /* upstream timer adjustment */
#define SIOC_LOOPBACK   (SIOC | 4)  /* diagnostic loopback test mode */


/* channel control register */
#define	DMA_INT_MASK		0xe0	/* ring intr mask */
#define DMA_INT_TH25		0x20	/* 25% threshold */
#define DMA_INT_TH50		0x40	/* 50% threshold */
#define DMA_INT_TH75		0x60	/* 75% threshold */
#define DMA_INT_EMPTY		0x80	/* ring buffer empty */
#define DMA_INT_NEMPTY		0xa0	/* ring buffer not empty */
#define DMA_INT_FULL		0xc0	/* ring buffer full */
#define DMA_INT_NFULL		0xe0	/* ring buffer not full */

#define DMA_CHANNEL_RESET	0x400	/* reset dma channel */
#define DMA_ENABLE			0x200	/* enable DMA */

/* peripheral controller intr status bits applicable to serial ports */
#define ISA_SERIAL0_MASK 		0x03f00000	/* mask for port #1 intrs */
#define ISA_SERIAL0_DIR			0x00100000	/* device intr request */
#define ISA_SERIAL0_Tx_THIR		0x00200000	/* Transmit DMA threshold */
#define ISA_SERIAL0_Tx_PREQ		0x00400000	/* Transmit DMA pair req */
#define ISA_SERIAL0_Tx_MEMERR	0x00800000	/* Transmit DMA memory err */
#define ISA_SERIAL0_Rx_THIR		0x01000000	/* Receive DMA threshold  */
#define ISA_SERIAL0_Rx_OVERRUN	0x02000000	/* Receive DMA over-run  */

#define ISA_SERIAL1_MASK 		0xfc000000	/* mask for port #1 intrs */
#define ISA_SERIAL1_DIR			0x04000000	/* device intr request */
#define ISA_SERIAL1_Tx_THIR		0x08000000	/* Transmit DMA threshold */
#define ISA_SERIAL1_Tx_PREQ		0x10000000	/* Transmit DMA pair req */
#define ISA_SERIAL1_Tx_MEMERR	0x20000000	/* Transmit DMA memory err */
#define ISA_SERIAL1_Rx_THIR		0x40000000	/* Receive DMA threshold  */
#define ISA_SERIAL1_Rx_OVERRUN	0x80000000	/* Receive DMA over-run  */

#define MAX_RING_BLOCKS		128			/* 4096/32 */
#define MAX_RING_SIZE		4096

/* DMA Input Control Byte */
#define	DMA_IC_OVRRUN	0x01	/* overrun error */
#define	DMA_IC_PARERR	0x02	/* parity error */
#define	DMA_IC_FRMERR	0x04	/* framing error */
#define	DMA_IC_BRKDET 	0x08	/* a break has arrived */
#define DMA_IC_VALID	0x80	/* pair is valid */

/* DMA Output Control Byte */
#define DMA_OC_TxINTR	0x20	/* set Tx intr after processing byte */
#define DMA_OC_INVALID	0x00	/* invalid pair */
#define DMA_OC_WTHR		0x40	/* Write byte to THR */
#define DMA_OC_WMCR		0x80	/* Write byte to MCR */
#define DMA_OC_DELAY	0xc0	/* time delay before next xmit */

/* ring id's */
#define RID_SERIAL0_TX	0x4		/* serial port 0, transmit ring buffer */
#define RID_SERIAL0_RX	0x5		/* serial port 0, receive ring buffer */
#define RID_SERIAL1_TX	0x6		/* serial port 1, transmit ring buffer */
#define RID_SERIAL1_RX	0x7		/* serial port 1, receive ring buffer */

#define CLOCK_XIN			22
#define PRESCALER_DIVISOR	3
#define CLOCK_ACE			7333333

/*
 * increment the ring offset. One way to do this would be to add b'100000.
 * this would let the offset value roll over automatically when it reaches
 * its maximum value (127). However when we use the offset, we must use
 * the appropriate bits only by masking with 0xfe0.
 * The other option is to shift the offset right by 5 bits and look at its
 * value. Then increment if required and shift back
 * note: 127 * 2^5 = 4064
 */
#define INC_RING_POINTER(x) \
	( ((x & 0xffe0) < 4064) ? (x += 32) : 0 )

#endif /* _ASM_IA64_SN_UART16550_H */
