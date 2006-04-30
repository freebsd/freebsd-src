/*-
 * Device driver for Specialix I/O8+ multiport serial card.
 *
 * Copyright 2003 Frank Mayhar <frank@exit.com>
 *
 * Derived from the "si" driver by Peter Wemm <peter@netplex.com.au>, using
 * lots of information from the Linux "specialix" driver by Roger Wolff
 * <R.E.Wolff@BitWizard.nl> and from the Intel CD1865 "Intelligent Eight-
 * Channel Communications Controller" datasheet.  Roger was also nice
 * enough to answer numerous questions about stuff specific to the I/O8+
 * not covered by the CD1865 datasheet.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 * $FreeBSD$
 */


/* CD1865 chip register definitions.  */

/*
 * Service Match Register interrupt acknowledgement values.
 *
 * These values are "obligatory" if you use the register based
 * interrupt acknowledgements; the wrong values can cause a lockup.
 * See section 8.11.1 of the Intel CD1865 "Intelligent Eight-Channel
 * Communications Controller" datasheet.
 */
#define CD1865_ACK_MINT	0x75	/* goes to MSMR                               */
#define CD1865_ACK_TINT	0x76	/* goes to TSMR                               */
#define CD1865_ACK_RINT	0x77	/* goes to RSMR                               */


#define CD1865_NUMCHAN	8	/* Total number of channels.                  */
#define CD1865_CHARTICK	16	/* Ticks per character.                       */
#define CD1865_TFIFOSZ	8	/* TX FIFO size.                              */
#define CD1865_RFIFOSZ	8	/* RX FIFO size.                              */

/*
 * Global registers.
 *	These registers are not associated with any particular channel;
 *	some define the general behavior of the card and others are only
 *	active during service requests.
 */
#define CD1865_GIVR	0x40	/* Global Interrupt Vector Register.          */
				/* The CD1865 datasheet calls this the        */
				/* "Global Vector Register" _and_ the         */
				/* "Global Service Vector Register," GSVR.    */
#define CD1865_GSVR	CD1865_GIVR
#define CD1865_GICR	0x41	/* Global Interrupting Channel Register.      */
				/* The CD1865 datasheet calls this the        */
				/* "Global Channel Register 1," GSCR1.        */
#define CD1865_GSCR1	CD1865_GICR
#define CD1865_GSCR2	0x42	/* Global Channel Register 2.                 */
#define CD1865_GSCR3	0x43	/* Global Channel Register 3.                 */
#define CD1865_MSMR	0x61	/* Priority Interrupt Level Register 1.       */
#define CD1865_TSMR	0x62	/* Priority Interrupt Level Register 2.       */
#define CD1865_RSMR	0x63	/* Priority Interrupt Level Register 3.       */
#define CD1865_CAR	0x64	/* Channel Access Register.                   */
#define CD1865_SRSR	0x65	/* Service Request Status Register.           */
#define CD1865_SRCR	0x66	/* Service Request Configuration Register.    */
#define CD1865_GFRCR	0x6b	/* Global Firmware Revision Code Register.    */
#define CD1865_PPRH	0x70	/* Prescaler Period Register High.            */
#define CD1865_PPRL	0x71	/* Prescaler Period Register Low.             */
#define CD1865_RDR	0x78	/* Receiver Data Register.                    */
#define CD1865_RCSR	0x7a	/* Receiver Character Status Register.        */
#define CD1865_TDR	0x7b	/* Transmit Data Register.                    */
#define CD1865_EOIR	0x7f	/* End of Interrupt Register.                 */
#define CD1865_MRAR	0x75	/* Modem Request Acknowlege Register.         */
#define CD1865_TRAR	0x76	/* Transmit Request Acknowlege Register.      */
#define CD1865_RRAR	0x77	/* Receive Request Acknowlege Register.       */

/*
 * Channel Registers
 *	These registers control or provide status for individual channels.
 *	Use the CD1865_CAR register to set up access to the channel before
 *	using these registers.
 */
#define CD1865_CCR	0x01	/* Channel Command Register.                  */
#define CD1865_IER	0x02	/* Interrupt Enable Register.                 */
				/* The CD1865 datasheet calls this the        */
				/* "Service Request Enable Register," SRER.   */
#define CD1865_SRER	CD1865_IER
#define CD1865_COR1	0x03	/* Channel Option Register 1.                 */
#define CD1865_COR2	0x04	/* Channel Option Register 2.                 */
#define CD1865_COR3	0x05	/* Channel Option Register 3.                 */
#define CD1865_CCSR	0x06	/* Channel Control Status Register.           */
#define CD1865_RDCR	0x07	/* Receive Data Count Register.               */
#define CD1865_SCHR1	0x09	/* Special Character Register 1.              */
#define CD1865_SCHR2	0x0a	/* Special Character Register 2.              */
#define CD1865_SCHR3	0x0b	/* Special Character Register 3.              */
#define CD1865_SCHR4	0x0c	/* Special Character Register 4.              */
#define CD1865_MCOR1	0x10	/* Modem Change Option 1 Register.            */
#define CD1865_MCOR2	0x11	/* Modem Change Option 2 Register.            */
#define CD1865_MCR	0x12	/* Modem Change Register.                     */
#define CD1865_RTPR	0x18	/* Receive Timeout Period Register.           */
#define CD1865_MSVR	0x28	/* Modem Signal Value Register.               */
#define CD1865_MSVRTS	0x29	/* Modem Signal Value Register.               */
#define CD1865_MSVDTR	0x2a	/* Modem Signal Value Register.               */
#define CD1865_RBPRH	0x31	/* Receive Baud Rate Period Register High.    */
#define CD1865_RBPRL	0x32	/* Receive Baud Rate Period Register Low.     */
#define CD1865_TBPRH	0x39	/* Transmit Baud Rate Period Register High.   */
#define CD1865_TBPRL	0x3a	/* Transmit Baud Rate Period Register Low.    */


/*
 * Global Interrupt Vector Register, read/write (0x40).
 */
#define CD1865_GIVR_ITMASK	0x07 /* Interrupt type mask.                  */
#define CD1865_GIVR_IT_MODEM	0x01 /* Modem Signal Change Interrupt.        */
#define CD1865_GIVR_IT_TX	0x02 /* Transmit Data Interrupt.              */
#define CD1865_GIVR_IT_RCV	0x03 /* Receive Good Data Interrupt.          */
#define CD1865_GIVR_IT_REXC	0x07 /* Receive Exception Interrupt.          */


/*
 * Global Interrupt Channel Register read/write (0x41)
 */
#define CD1865_GICR_CHAN_MASK	0x1c /* Channel Number Mask.                  */
#define CD1865_GICR_CHAN_SHIFT	2    /* Channel Number shift.                 */


/*
 * Channel Access Register, read/write (0x64).
 */
#define CD1865_CAR_CHAN_MASK	0x07 /* Channel Number Mask.                  */
#define CD1865_CAR_A7		0x08 /* A7 Address Extension (unused).        */


/*
 * Receive Character Status Register, readonly (0x7a).
 */
#define CD1865_RCSR_TOUT	0x80 /* Rx Timeout.                           */
#define CD1865_RCSR_SCDET	0x70 /* Special Character Detected Mask.      */
#define CD1865_RCSR_NO_SC	0x00 /* No Special Characters Detected.       */
#define CD1865_RCSR_SC_1	0x10 /* Special Char 1 (or 1 & 3) Detected.   */
#define CD1865_RCSR_SC_2	0x20 /* Special Char 2 (or 2 & 4) Detected.   */
#define CD1865_RCSR_SC_3	0x30 /* Special Char 3 Detected.              */
#define CD1865_RCSR_SC_4	0x40 /* Special Char 4 Detected.              */
#define CD1865_RCSR_BREAK	0x08 /* Break detected.                       */
#define CD1865_RCSR_PE		0x04 /* Parity Error.                         */
#define CD1865_RCSR_FE		0x02 /* Frame Error.                          */
#define CD1865_RCSR_OE		0x01 /* Overrun Error.                        */


/*
 * Channel Command Register, read/write (0x01)
 * 	Commands in groups can be OR-ed together.
 */
#define CD1865_CCR_HARDRESET	0x81 /* Reset the CD1865 (like a powercycle). */

#define CD1865_CCR_SOFTRESET	0x80 /* Soft Channel Reset (one channel).     */

#define CD1865_CCR_CORCHG1	0x42 /* Channel Option Register 1 Changed.    */
#define CD1865_CCR_CORCHG2	0x44 /* Channel Option Register 2 Changed.    */
#define CD1865_CCR_CORCHG3	0x48 /* Channel Option Register 3 Changed.    */

#define CD1865_CCR_SSCH1	0x21 /* Send Special Character 1.             */

#define CD1865_CCR_SSCH2	0x22 /* Send Special Character 2.             */

#define CD1865_CCR_SSCH3	0x23 /* Send Special Character 3.             */

#define CD1865_CCR_SSCH4	0x24 /* Send Special Character 4.             */

#define CD1865_CCR_TXEN		0x18 /* Enable Transmitter.                   */
#define CD1865_CCR_RXEN		0x12 /* Enable Receiver.                      */

#define CD1865_CCR_TXDIS	0x14 /* Disable Transmitter.                  */
#define CD1865_CCR_RXDIS	0x11 /* Disable Receiver.                     */


/*
 * Interrupt Enable Register, read/write (0x02).
 * 	(aka Service Request Enable Register)
 */
#define CD1865_IER_DSR		0x80 /* Enable DSR change interrupt.          */
#define CD1865_IER_CD		0x40 /* Enable CD change interrupt.           */
#define CD1865_IER_CTS		0x20 /* Enable CTS change interrupt.          */
#define CD1865_IER_RXD		0x10 /* Enable Receive Data interrupt.        */
#define CD1865_IER_RXSC		0x08 /* Enable Receive Special Character int. */
#define CD1865_IER_TXRDY	0x04 /* Enable Transmit ready interrupt.      */
#define CD1865_IER_TXEMPTY	0x02 /* Enable Transmit empty interrupt.      */
#define CD1865_IER_NNDT		0x01 /* Enable "No New Data Timeout" int.     */


/*
 * Channel Option Register 1, read/write (0x03).
 */
#define CD1865_COR1_ODDP	0x80 /* Odd Parity.                           */
#define CD1865_COR1_PARMODE	0x60 /* Parity enable mask.                   */
#define CD1865_COR1_NOPAR	0x00 /* No Parity.                            */
#define CD1865_COR1_FORCEPAR	0x20 /* Force Parity.                         */
#define CD1865_COR1_NORMPAR	0x40 /* Normal Parity.                        */
#define CD1865_COR1_IGNORE	0x10 /* Ignore Parity on RX.                  */
#define CD1865_COR1_STOPBITS	0x0c /* Number of Stop Bits.                  */
#define CD1865_COR1_1SB		0x00 /* 1 Stop Bit.                           */
#define CD1865_COR1_15SB	0x04 /* 1.5 Stop Bits.                        */
#define CD1865_COR1_2SB		0x08 /* 2 Stop Bits.                          */
#define CD1865_COR1_CHARLEN	0x03 /* Character Length.                     */
#define CD1865_COR1_5BITS	0x00 /* 5 bits.                               */
#define CD1865_COR1_6BITS	0x01 /* 6 bits.                               */
#define CD1865_COR1_7BITS	0x02 /* 7 bits.                               */
#define CD1865_COR1_8BITS	0x03 /* 8 bits.                               */


/*
 * Channel Option Register 2, read/write (0x04).
 */
#define CD1865_COR2_IXM		0x80 /* Implied XON mode.                     */
#define CD1865_COR2_TXIBE	0x40 /* Enable In-Band (XON/XOFF) Flow Control*/
#define CD1865_COR2_ETC		0x20 /* Embedded Tx Commands Enable.          */
#define CD1865_COR2_LLM		0x10 /* Local Loopback Mode.                  */
#define CD1865_COR2_RLM		0x08 /* Remote Loopback Mode.                 */
#define CD1865_COR2_RTSAO	0x04 /* RTS Automatic Output Enable.          */
#define CD1865_COR2_CTSAE	0x02 /* CTS Automatic Enable.                 */
#define CD1865_COR2_DSRAE	0x01 /* DSR Automatic Enable.                 */


/*
 * Channel Option Register 3, read/write (0x05).
 */
#define CD1865_COR3_XONCH	0x80 /* XON is a pair of characters (1 & 3).  */
#define CD1865_COR3_XOFFCH	0x40 /* XOFF is a pair of characters (2 & 4). */
#define CD1865_COR3_FCT		0x20 /* Flow-Control Transparency Mode.       */
#define CD1865_COR3_SCDE	0x10 /* Special Character Detection Enable.   */
#define CD1865_COR3_RXTH	0x0f /* RX FIFO Threshold value (1-8).        */


/*
 * Channel Control Status Register, readonly (0x06)
 */
#define CD1865_CCSR_RXEN	0x80 /* Receiver Enabled.                     */
#define CD1865_CCSR_RXFLOFF	0x40 /* Receive Flow Off (XOFF was sent).     */
#define CD1865_CCSR_RXFLON	0x20 /* Receive Flow On (XON was sent).       */
#define CD1865_CCSR_TXEN	0x08 /* Transmitter Enabled.                  */
#define CD1865_CCSR_TXFLOFF	0x04 /* Transmit Flow Off (got XOFF).         */
#define CD1865_CCSR_TXFLON	0x02 /* Transmit Flow On (got XON).           */


/*
 * Modem Change Option Register 1, read/write (0x10).
 */
#define CD1865_MCOR1_DSRZD	0x80 /* Detect 0->1 transition of DSR.        */
#define CD1865_MCOR1_CDZD	0x40 /* Detect 0->1 transition of CD.         */
#define CD1865_MCOR1_CTSZD	0x20 /* Detect 0->1 transition of CTS.        */
#define CD1865_MCOR1_DTRTH	0x0f /* Auto DTR flow control Threshold (1-8).*/
#define CD1865_MCOR1_NODTRFC	0x0  /* Automatic DTR flow control disabled.  */


/*
 * Modem Change Option Register 2, read/write (0x11).
 */
#define CD1865_MCOR2_DSROD	0x80 /* Detect 1->0 transition of DSR.        */
#define CD1865_MCOR2_CDOD	0x40 /* Detect 1->0 transition of CD.         */
#define CD1865_MCOR2_CTSOD	0x20 /* Detect 1->0 transition of CTS.        */

/*
 * Modem Change Register, read/write (0x12).
 */
#define CD1865_MCR_DSRCHG	0x80 /* DSR Changed.                          */
#define CD1865_MCR_CDCHG	0x40 /* CD Changed.                           */
#define CD1865_MCR_CTSCHG	0x20 /* CTS Changed.                          */


/*
 * Modem Signal Value Register, read/write (0x28)
 *
 * Note:
 *	These are inverted with respect to the actual signals!  If the
 *	signal is present, the bit is zero, else the bit is one.
 */
#define CD1865_MSVR_DSR		0x80 /* Current state of DSR input.           */
#define CD1865_MSVR_CD		0x40 /* Current state of CD input.            */
#define CD1865_MSVR_CTS		0x20 /* Current state of CTS input.           */
#define CD1865_MSVR_DTR		0x02 /* Current state of DTR output.          */
#define CD1865_MSVR_RTS		0x01 /* Current state of RTS output.          */
#define CD1865_MSVR_OFF		0xe3 /* All signals off.                      */
#define CD1865_MSVR_ON		0x00 /* All signals on.                       */

/*
 * Escape characters.  These are sent in-band when embedded commands are
 * enabled with CD1865_COR2_ETC.
 */
#define CD1865_C_ESC		0x00 /* Escape character.                     */
#define CD1865_C_SBRK		0x81 /* Start sending BREAK.                  */
#define CD1865_C_DELAY		0x82 /* Delay output.                         */
#define CD1865_C_EBRK		0x83 /* Stop sending BREAK.                   */

#define CD1865_SRSR_RREQint	0x10 /* Receive request interrupt.            */
#define CD1865_SRSR_TREQint	0x04 /* Transmit request interrupt.           */
#define CD1865_SRSR_MREQint	0x01 /* Modem signal change request interrupt.*/
#define CD1865_SRSR_REQint	0x15 /* All of the above.                     */

#define CD1865_SRCR_PKGTYPE	0x80
#define CD1865_SRCR_REGACKEN	0x40
#define CD1865_SRCR_DAISYEN	0x20
#define CD1865_SRCR_GLOBPRI	0x10
#define CD1865_SRCR_UNFAIR	0x08
#define CD1865_SRCR_AUTOPRI	0x02
#define CD1865_SRCR_PRISEL	0x01
