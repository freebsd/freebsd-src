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


/*
 * Per-channel soft information structure, stored in the driver.  It's called
 * "sx_port" just because the si driver calls it "si_port."
 *
 * This information is mostly visible via ioctl().
 */
struct sx_port {
	int		sp_chan;	/* Channel number, for convenience.   */
	struct tty	*sp_tty;
	int		sp_state;
	int		sp_active_out;	/* callout is open */
	int		sp_delta_overflows;
	u_int		sp_wopeners;	/* Processes waiting for DCD.         */
	struct termios	sp_iin;		/* Initial state.                     */
	struct termios	sp_iout;
	struct termios	sp_lin;		/* Lock state.                        */
	struct termios	sp_lout;
#ifdef	SX_DEBUG
	int		sp_debug;	/* debug mask */
#endif
};

/*
 * Various important values.
 */
#define SX_NUMCHANS	8	/* Eight channels on an I/O8+.                */
#define SX_PCI_IO_SPACE	8	/* How much address space to use.             */
#define SX_CCR_TIMEOUT	10000	/* Channel Command Register timeout, 10ms.    */
#define SX_GSVR_TIMEOUT	1000	/* GSVR reset timeout, 1ms.                   */
#define SX_CD1865_ID	0x10	/* ID of the I/O8+ CD1865 chip.               */
#define SX_EI		0x80	/* "Enable interrupt" flag for I/O8+ commands.*/

#define SX_DATA_REG	0	/* Data register.                             */
#define SX_ADDR_REG	1	/* Address register.                          */

/*
 * The I/O8+ has a 25MHz oscillator on board, but the CD1865 runs at half
 * that.
 */
#define SX_CD1865_CLOCK	12500000	/* CD1865 clock on I/O8+.             */
#define SX_CD1865_TICK 4000		/* Timer tick rate, via prescaler.    */
#define SX_CD1865_PRESCALE (SX_CD1865_CLOCK/SX_CD1865_TICK) /* Prescale value.*/

#include <sys/callout.h>

/*
 * Device numbering for the sx device.
 *
 * The minor number is broken up into four fields as follows:
 *	Field			Bits	Mask
 *      ---------------------	----	----
 *	Channel (port) number	0-2	0x07
 *	"DTR pin is DTR" flag	3	0x08
 *	Unused (zero)		4	0x10
 *	Card number		5-6	0x60
 *	Callout device flag	7	0x80
 *
 * The next 8 bits in the word is the major number, followed by the
 * "initial state device" flag and then the "lock state device" flag.
 */
#define	SX_CHAN_MASK		0x07
#define SX_DTRPIN_MASK		0x08
#define	SX_CARD_MASK		0x60
#define	SX_TTY_MASK		0x7f
#define SX_CALLOUT_MASK		0x80
#define	SX_INIT_STATE_MASK	0x10000
#define	SX_LOCK_STATE_MASK	0x20000
#define	SX_STATE_MASK		0x30000
#define	SX_SPECIAL_MASK		0x30000

#define SX_CARDSHIFT		5
#define	SX_MINOR2CHAN(m)	(m & SX_CHAN_MASK)
#define	SX_MINOR2CARD(m)	((m & SX_CARD_MASK) >> SX_CARDSHIFT)
#define	SX_MINOR2TTY(m)		(m & SX_TTY_MASK)

#define	DEV_IS_CALLOUT(m)	(m & SX_CALLOUT_MASK)
#define	DEV_IS_STATE(m)		(m & SX_STATE_MASK)
#define	DEV_IS_SPECIAL(m)	(m & SX_SPECIAL_MASK)
#define DEV_DTRPIN(m)		(m & SX_DTRPIN_MASK)

#define	MINOR2SC(m)	((struct sx_softc *)devclass_get_softc(sx_devclass,\
							      SX_MINOR2CARD(m)))
#define	MINOR2PP(m)	(MINOR2SC((m))->sc_ports + SX_MINOR2CHAN((m)))
#define	MINOR2TP(m)	(MINOR2PP((m))->sp_tty)
#define	TP2PP(tp)	(MINOR2PP(SX_MINOR2TTY(minor((tp)->t_dev))))
#define TP2SC(tp)	(MINOR2SC(minor((tp)->t_dev)))
#define PP2SC(pp)	(MINOR2SC(minor((pp)->sp_tty->t_dev)))

/* Buffer parameters */
#define	SX_BUFFERSIZE	CD1865_RFIFOSZ	/* Just the size of the receive FIFO. */
#define SX_I_HIGH_WATER	(TTYHOG - 2 * SX_BUFFERSIZE)

/*
 * Precomputed bitrate clock divisors.  Formula is
 *
 *                Clock rate (Hz)        12500000
 *	divisor = ---------------  or  ------------
 *                 16 * bit rate       16 * bitrate
 *
 * All values are rounded to the nearest integer.
 */
#define	CLK75		0x28b1		/* 10416.666667                       */
#define	CLK110		0x1bbe		/*  7102.272727                       */
#define	CLK150		0x1458		/*  5208.333333                       */
#define	CLK300		0x0a2c		/*  2604.166667                       */
#define	CLK600		0x0516		/*  1302.083333                       */
#define	CLK1200		0x028b		/*   651.0416667                      */
#define	CLK2000		0x0187		/*   390.625                          */
#define	CLK2400		0x0146		/*   325.5208333                      */
#define	CLK4800		0x00a3		/*   162.7604167                      */
#define	CLK7200		0x006d		/*   108.5069444                      */
#define	CLK9600		0x0051		/*    81.38020833                     */
#define	CLK19200	0x0029		/*    40.69010417                     */
#define	CLK38400	0x0014		/*    20.34505208                     */
#define	CLK57600	0x000e		/*    13.56336806                     */
#define	CLK115200	0x0007		/*     6.781684028                    */


/* sp_state */
#define	SX_SS_CLOSED	0x00000		/* Port is closed.                    */
#define	SX_SS_OPEN	0x00001		/* Port is open and active.           */
#define SX_SS_XMIT	0x00002		/* We're transmitting data.           */
#define SX_SS_INTR	0x00004		/* We're processing an interrupt.     */
#define SX_SS_CLOSING	0x00008		/* in the middle of an sxclose()      */
#define	SX_SS_WAITWRITE	0x00010
#define	SX_SS_BLOCKWRITE 0x00020
#define	SX_SS_DTR_OFF	0x00040		/* DTR held off                       */
#define SX_SS_IFLOW	0x00080		/* Input (RTS) flow control on.       */
#define SX_SS_OFLOW	0x00100		/* Output (CTS) flow control on.      */
#define SX_SS_IRCV	0x00200		/* In a receive interrupt.            */
#define SX_SS_IMODEM	0x00400		/* In a modem-signal interrupt.       */
#define SX_SS_IRCVEXC	0x00800		/* In a receive-exception interrupt.  */
#define SX_SS_IXMIT	0x01000		/* In a transmit interrupt.           */
#define SX_SS_OSTOP	0x02000		/* Stopped by output flow control.    */
#define SX_SS_ISTOP	0x04000		/* Stopped by input flow control.     */
#define SX_SS_DTRPIN	0x08000		/* DTR/RTS pin is DTR.                */
#define SX_SS_DOBRK	0x10000		/* Change break status.               */
#define SX_SS_BREAK	0x20000		/* Doing break.                       */

#define SX_DTRPIN(pp)	((pp)->sp_state & SX_SS_DTRPIN) /* DTR/RTS pin is DTR.*/
#define SX_XMITTING(pp)	((pp)->sp_state & SX_SS_XMIT) /* We're transmitting.  */
#define SX_INTR(pp)	((pp)->sp_state & SX_SS_INTR) /* In an interrupt.     */
#define SX_IXMIT(pp)	((pp)->sp_state & SX_SS_IXMIT) /* Transmit interrupt. */
#define SX_IFLOW(pp)	((pp)->sp_state & SX_SS_IFLOW) /* Input flow control. */
#define SX_OFLOW(pp)	((pp)->sp_state & SX_SS_OFLOW) /* Output flow control.*/
#define SX_IRCV(pp)	((pp)->sp_state & SX_SS_IRCV) /* Receive interrupt.   */
#define SX_IMODEM(pp)	((pp)->sp_state & SX_SS_IMODEM) /* Modem state change.*/
#define SX_IRCVEXC(pp)	((pp)->sp_state & SX_SS_IRCVEXC) /* Rcv exception.    */
#define SX_OSTOP(pp)	((pp)->sp_state & SX_SS_OSTOP) /* Output stopped.     */
#define SX_ISTOP(pp)	((pp)->sp_state & SX_SS_ISTOP) /* Input stopped.      */
#define SX_DOBRK(pp)	((pp)->sp_state & SX_SS_DOBRK) /* Change break status.*/
#define SX_BREAK(pp)	((pp)->sp_state & SX_SS_BREAK) /* Doing break.        */

#define	DBG_ENTRY		0x00000001
#define	DBG_DRAIN		0x00000002
#define	DBG_OPEN		0x00000004
#define	DBG_CLOSE		0x00000008
/*				0x00000010*/
#define	DBG_WRITE		0x00000020
#define	DBG_PARAM		0x00000040
#define	DBG_INTR		0x00000080
#define	DBG_IOCTL		0x00000100
/*				0x00000200 */
/*				0x00000400*/
#define	DBG_OPTIM		0x00000800
#define	DBG_START		0x00001000
#define	DBG_EXIT		0x00002000
#define	DBG_FAIL		0x00004000
#define	DBG_STOP		0x00008000
#define	DBG_AUTOBOOT		0x00010000
#define	DBG_MODEM		0x00020000
#define DBG_MODEM_STATE		0x00040000
#define DBG_RECEIVE		0x00080000
#define	DBG_POLL		0x00100000
#define DBG_TRANSMIT		0x00200000
#define DBG_RECEIVE_EXC		0x00400000
#define DBG_PRINTF		0x80000000
#define	DBG_ALL			0xffffffff
