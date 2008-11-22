/*-
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 * 'C' definitions for Specialix serial multiplex driver.
 *
 * Copyright (C) 1990, 1992, 1998 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm <peter@netplex.com.au>
 *
 * Derived from:	SunOS 4.x version
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Andy Rutter of
 *	Advanced Methods and Tools Ltd. based on original information
 *	from Specialix International.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 * $FreeBSD$
 */

#include <sys/callout.h>

/*
 * We name devices with %r in make_dev() with a radix of 32.
 */ 
#define	SI_MAXPORTPERCARD	32

/* Buffer parameters */
#define	SI_BUFFERSIZE	256

typedef	uint8_t 	BYTE;		/* Type cast for unsigned 8 bit */
typedef	uint16_t 	WORD;		/* Type cast for unsigned 16 bit */

/*
 * Hardware `registers', stored in the shared memory.
 * These are related to the firmware running on the Z280.
 */

struct si_reg	{
	BYTE	initstat;
	BYTE	memsize;
	WORD	int_count;
	WORD	revision;
	BYTE	rx_int_count;		/* isr_count on Jet */	
	BYTE	main_count;		/* spare on Z-280 */
	WORD	int_pending;
	WORD	int_counter;
	BYTE	int_scounter;
	BYTE	res[0x80 - 13];
};

/*
 *	Per module control structure, stored in shared memory.
 */
struct si_module {
	WORD	sm_next;		/* Next module */
	BYTE	sm_type;		/* Number of channels */
	BYTE	sm_number;		/* Module number on cable */
	BYTE	sm_dsr;			/* Private dsr copy */
	BYTE	sm_res[0x80 - 5];	/* Reserve space to 128 bytes */
};

/*
 *	The 'next' pointer & with 0x7fff + SI base addres give
 *	the address of the next module block if fitted. (else 0)
 *	Note that next points to the TX buffer so 0x60 must be
 *	subtracted to find the true base.
 */
#define TA4		0x00
#define TA8		0x08
#define TA4_ASIC	0x0A
#define TA8_ASIC	0x0B
#define MTA		0x28
#define SXDC		0x48

/*
 *	Per channel(port) control structure, stored in shared memory.
 */
struct	si_channel {
	/*
	 * Generic stuff
	 */
	WORD	next;			/* Next Channel */
	WORD	addr_uart;		/* Uart address */
	WORD	module;			/* address of module struct */
	BYTE 	type;			/* Uart type */
	BYTE	fill;
	/*
	 * Uart type specific stuff
	 */
	BYTE	x_status;		/* XON / XOFF status */
	BYTE	c_status;		/* cooking status */
	BYTE	hi_rxipos;		/* stuff into rx buff */
	BYTE	hi_rxopos;		/* stuff out of rx buffer */
	BYTE	hi_txopos;		/* Stuff into tx ptr */
	BYTE	hi_txipos;		/* ditto out */
	BYTE	hi_stat;		/* Command register */
	BYTE	dsr_bit;		/* Magic bit for DSR */
	BYTE	txon;			/* TX XON char */
	BYTE	txoff;			/* ditto XOFF */
	BYTE	rxon;			/* RX XON char */
	BYTE	rxoff;			/* ditto XOFF */
	BYTE	hi_mr1;			/* mode 1 image */
	BYTE	hi_mr2;			/* mode 2 image */
        BYTE	hi_csr;			/* clock register */
	BYTE	hi_op;			/* Op control */
	BYTE	hi_ip;			/* Input pins */
	BYTE	hi_state;		/* status */
	BYTE	hi_prtcl;		/* Protocol */
	BYTE	hi_txon;		/* host copy tx xon stuff */
	BYTE	hi_txoff;
	BYTE	hi_rxon;
	BYTE	hi_rxoff;
	BYTE	close_prev;		/* Was channel previously closed */
	BYTE	hi_break;		/* host copy break process */
	BYTE	break_state;		/* local copy ditto */
	BYTE	hi_mask;		/* Mask for CS7 etc. */
	BYTE	mask_z280;		/* Z280's copy */
	BYTE	res[0x60 - 36];
	BYTE	hi_txbuf[SI_BUFFERSIZE];
	BYTE	hi_rxbuf[SI_BUFFERSIZE];
	BYTE	res1[0xA0];
};

/*
 *	Register definitions
 */

/*
 *	Break input control register definitions
 */
#define	BR_IGN		0x01	/* Ignore any received breaks */
#define	BR_INT		0x02	/* Interrupt on received break */
#define BR_PARMRK	0x04	/* Enable parmrk parity error processing */
#define	BR_PARIGN	0x08	/* Ignore chars with parity errors */

/*
 *	Protocol register provided by host for XON/XOFF and cooking
 */
#define	SP_TANY		0x01	/* Tx XON any char */
#define	SP_TXEN		0x02	/* Tx XON/XOFF enabled */
#define	SP_CEN		0x04	/* Cooking enabled */
#define	SP_RXEN		0x08	/* Rx XON/XOFF enabled */
#define	SP_DCEN		0x20	/* DCD / DTR check */
#define	SP_PAEN		0x80	/* Parity checking enabled */

/*
 *	HOST STATUS / COMMAND REGISTER
 */
#define	IDLE_OPEN	0x00	/* Default mode, TX and RX polled
				   buffer updated etc */
#define	LOPEN		0x02	/* Local open command (no modem ctl */
#define MOPEN		0x04	/* Open and monitor modem lines (blocks
				   for DCD */
#define MPEND		0x06	/* Wating for DCD */
#define CONFIG		0x08	/* Channel config has changed */
#define CLOSE		0x0A	/* Close channel */
#define SBREAK		0x0C	/* Start break */
#define EBREAK		0x0E	/* End break */
#define IDLE_CLOSE	0x10	/* Closed channel */
#define IDLE_BREAK	0x12	/* In a break */
#define FCLOSE		0x14	/* Force a close */
#define RESUME		0x16	/* Clear a pending xoff */
#define WFLUSH		0x18	/* Flush output buffer */
#define RFLUSH		0x1A	/* Flush input buffer */

/*
 *	Host status register
 */
#define	ST_BREAK	0x01	/* Break received (clear with config) */

/*
 *	OUTPUT PORT REGISTER
 */
#define	OP_CTS	0x01	/* Enable CTS */
#define OP_DSR	0x02	/* Enable DSR */
/*
 *	INPUT PORT REGISTER
 */
#define	IP_DCD	0x04	/* DCD High */
#define IP_DTR	0x20	/* DTR High */
#define IP_RTS	0x02	/* RTS High */
#define	IP_RI	0x40	/* RI  High */

/*
 *	Mode register and uart specific stuff
 */
/*
 *	MODE REGISTER 1
 */
#define	MR1_5_BITS	0x00
#define	MR1_6_BITS	0x01
#define	MR1_7_BITS	0x02
#define	MR1_8_BITS	0x03
/*
 *	Parity
 */
#define	MR1_ODD		0x04
#define	MR1_EVEN	0x00
/*
 *	Parity mode
 */
#define	MR1_WITH	0x00
#define	MR1_FORCE	0x08
#define	MR1_NONE	0x10
#define	MR1_SPECIAL	0x18
/*
 *	Error mode
 */
#define	MR1_CHAR	0x00
#define	MR1_BLOCK	0x20
/*
 *	Request to send line automatic control
 */
#define	MR1_CTSCONT	0x80

/*
 *	MODE REGISTER 2
 */
/*
 *	Number of stop bits
 */
#define	MR2_1_STOP	0x07
#define	MR2_2_STOP	0x0F
/*
 *	Clear to send automatic testing before character sent
 */
#define	MR2_RTSCONT	0x10
/*
 *	Reset RTS automatically after sending character?
 */
#define	MR2_CTSCONT	0x20
/*
 *	Channel mode
 */
#define	MR2_NORMAL	0x00
#define	MR2_AUTO	0x40
#define	MR2_LOCAL	0x80
#define	MR2_REMOTE	0xC0

/*
 *	CLOCK SELECT REGISTER - this and the code assumes ispeed == ospeed
 */
/*
 * Clocking rates are in lower and upper nibbles.. R = upper, T = lower
 */
#define	CLK75		0x0
#define	CLK110		0x1	/* 110 on XIO!! */
#define	CLK38400	0x2	/* out of sequence */
#define	CLK150		0x3
#define	CLK300		0x4
#define	CLK600		0x5
#define	CLK1200		0x6
#define	CLK2000		0x7
#define	CLK2400		0x8
#define	CLK4800		0x9
#define	CLK7200		0xa	/* unchecked */
#define	CLK9600		0xb
#define	CLK19200	0xc
#define	CLK57600	0xd

/*
 * Per-port (channel) soft information structure, stored in the driver.
 * This is visible via ioctl()'s.
 */
struct si_port {
	volatile struct si_channel *sp_ccb;
	struct tty	*sp_tty;
	int		sp_pend;	/* pending command */
	int		sp_last_hi_ip;	/* cached DCD */
	int		sp_state;
	int		sp_delta_overflows;
	struct callout_handle lstart_ch;/* For canceling our timeout */
#ifdef	SI_DEBUG
	int		sp_debug;	/* debug mask */
	char		sp_name[5];
#endif
};

/* sp_state */
/*			0x0001	--					*/
/*			0x0002	--					*/
/*			0x0004	--					*/
/*			0x0008	--					*/
/*			0x0010	--					*/
/*			0x0020	--					*/
/*			0x0040	-- 	 				*/
/*			0x0080	-- 	 				*/
#define SS_LSTART	0x0100	/* lstart timeout pending		*/
#define SS_INLSTART	0x0200	/* running an lstart induced t_oproc	*/
/*			0x0400	--					*/
/*			0x0800	--					*/

/*
 *	Command post flags
 */
#define	SI_NOWAIT	0x00	/* Don't wait for command */
#define SI_WAIT		0x01	/* Wait for complete */

/*
 *	SI ioctls
 */
/*
 * struct for use by Specialix ioctls - used by siconfig(8)
 */
typedef struct {
	unsigned char
		sid_port:5,			/* 0 - 31 ports per card */
		sid_card:2,			/* 0 - 3 cards */
		sid_control:1;			/* controlling device (all cards) */
} sidev_t;
struct si_tcsi {
	sidev_t	tc_dev;
	union {
		int	x_int;
		int	x_dbglvl;
	}	tc_action;
#define	tc_card		tc_dev.sid_card
#define	tc_port		tc_dev.sid_port
#define	tc_int		tc_action.x_int
#define	tc_dbglvl	tc_action.x_dbglvl
};

struct si_pstat {
	sidev_t	tc_dev;
	union {
		struct si_port    x_siport;
		struct si_channel x_ccb;
		struct tty        x_tty;
	} tc_action;
#define tc_siport	tc_action.x_siport
#define tc_ccb		tc_action.x_ccb
#define tc_tty		tc_action.x_tty
};

#define	IOCTL_MIN	96
#define	TCSIDEBUG	_IOW('S', 96, struct si_tcsi)	/* Toggle debug */
#define	TCSIRXIT	_IOW('S', 97, struct si_tcsi)	/* RX int throttle */
#define	TCSIIT		_IOW('S', 98, struct si_tcsi)	/* TX int throttle */
			/* 99 defunct */
			/* 100 defunct */
			/* 101 defunct */
			/* 102 defunct */
			/* 103 defunct */
			/* 104 defunct */
#define	TCSISTATE	_IOWR('S', 105, struct si_tcsi)	/* get current state of RTS
						   DCD and DTR pins */
			/* 106 defunct */
#define	TCSIPORTS	_IOR('S', 107, int)	/* Number of ports found */
#define	TCSISDBG_LEVEL	_IOW('S', 108, struct si_tcsi)	/* equivalent of TCSIDEBUG which sets a
					 * particular debug level (DBG_??? bit
					 * mask), default is 0xffff */
#define	TCSIGDBG_LEVEL	_IOWR('S', 109, struct si_tcsi)
#define	TCSIGRXIT	_IOWR('S', 110, struct si_tcsi)
#define	TCSIGIT		_IOWR('S', 111, struct si_tcsi)
			/* 112 defunct */
			/* 113 defunct */
			/* 114 defunct */
			/* 115 defunct */
			/* 116 defunct */
			/* 117 defunct */

#define	TCSISDBG_ALL	_IOW('S', 118, int)		/* set global debug level */
#define	TCSIGDBG_ALL	_IOR('S', 119, int)		/* get global debug level */

			/* 120 defunct */
			/* 121 defunct */
			/* 122 defunct */
			/* 123 defunct */
#define	TCSIMODULES	_IOR('S', 124, int)	/* Number of modules found */

/* Various stats and monitoring hooks per tty device */
#define	TCSI_PORT	_IOWR('S', 125, struct si_pstat) /* get si_port */
#define	TCSI_CCB	_IOWR('S', 126, struct si_pstat) /* get si_ccb */
#define	TCSI_TTY	_IOWR('S', 127, struct si_pstat) /* get tty struct */

#define	IOCTL_MAX	127

#define	IS_SI_IOCTL(cmd)	((u_int)((cmd)&0xff00) == ('S'<<8) && \
		(u_int)((cmd)&0xff) >= IOCTL_MIN && \
		(u_int)((cmd)&0xff) <= IOCTL_MAX)

#define	CONTROLDEV	"/dev/si_control"
