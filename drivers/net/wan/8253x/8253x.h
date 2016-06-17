/* -*- linux-c -*- */
/* $Id: 8253x.h,v 1.14 2002/02/10 22:17:25 martillo Exp $
 * sab82532.h: Register Definitions for the Siemens SAB82532 DUSCC
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 *
 * Modified Aug 1, 2000 Francois Wautier  
 * Modified for complete driver Joachim Martillo
 */

/* Modifications:
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#ifndef _SAB82532_H
#define _SAB82532_H

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/serial.h>	/* need struct async_icount for ioctl */
#include <linux/netdevice.h>

#include "8253xioc.h"
#include "ring.h"

#define SAB8253X_MAX_TEC_DELAY	200000 /* 1 character time (at 50 baud) */
#define SAB8253X_MAX_CEC_DELAY	 50000 /* 2.5 TX CLKs (at 50 baud) */

#define SERIAL_TYPE_SYNCTTY 3	/* check tty driver to make sure okay */
				/* SERIAL_TYPE_NORMAL and SERIAL_TYPE_CALLOUT */
				/* seem to be used by the tty driver */
				/* only to print out a warning not to */
				/* use callout devices - next version of */
				/* the code, for now want to be able to */
				/* maintain some of the structure of */
				/* a 2.2.* driver for those that are */
				/* running old kernels. */

#define READB(port, rreg) (*port->readbyte)(port,\
	(unsigned char *)&(port->regs->async_read.rreg))
#define WRITEB(port, rreg, val) (*port->writebyte)(port,\
	(unsigned char *)&(port->regs->async_write.rreg), val)
#ifdef DEFINE_VARIABLE
static unsigned char tmpval; 
#endif
	/* Used in the macro below -- don't create a variable called tmpval*/
#define SET_REG_BIT(port,rreg,bit)\
	tmpval=(*port->readbyte)(port,\
		(unsigned char *)&(port->regs->async_read.rreg));\
	tmpval |= bit;\
	(*port->writebyte)(port,\
		(unsigned char *)&(port->regs->async_write.rreg), tmpval);
#define CLEAR_REG_BIT(port,rreg,bit)\
	tmpval=(*port->readbyte)(port,\
		(unsigned char *)&(port->regs->async_read.rreg));\
	tmpval &= ~(bit);\
	(*port->writebyte)(port,\
		(unsigned char *)&(port->regs->async_write.rreg), tmpval);
#define MASK_REG_BIT(port,rreg,bit)\
	tmpval=(*port->readbyte)(port,\
		(unsigned char *)&(port->regs->async_read.rreg));\
	tmpval &= bit;\
	(*port->writebyte)(port,\
		(unsigned char *)&(port->regs->async_write.rreg), tmpval);
#define READ_X_WRITEB(port,rreg,op,val)\
	tmpval=(*port->readbyte)(port,\
		(unsigned char *)&(port->regs->async_read.rreg));\
	tmpval op= val;\
	(*port->writebyte)(port,\
		(unsigned char *)&(port->regs->async_write.rreg), tmpval);


struct sab82532_async_rd_regs 
{
	volatile unsigned char	rfifo[0x20];	/* Receive FIFO				*/
	volatile unsigned char	star;		/* Status Register			*/
	volatile unsigned char	rsta;		/* actually an HDLC register */
	volatile unsigned char	mode;		/* Mode Register			*/
	volatile unsigned char	timr;		/* Timer Register			*/
	volatile unsigned char	xon;		/* XON Character			*/
	volatile unsigned char	xoff;		/* XOFF Character			*/
	volatile unsigned char	tcr;		/* Termination Character Register	*/
	volatile unsigned char	dafo;		/* Data Format				*/
	volatile unsigned char	rfc;		/* RFIFO Control Register		*/
	volatile unsigned char	__pad2;
	volatile unsigned char	rbcl;		/* Receive Byte Count Low		*/
	volatile unsigned char	rbch;		/* Receive Byte Count High		*/
	volatile unsigned char	ccr0;		/* Channel Configuration Register 0	*/
	volatile unsigned char	ccr1;		/* Channel Configuration Register 1	*/
	volatile unsigned char	ccr2;		/* Channel Configuration Register 2	*/
	volatile unsigned char	ccr3;		/* Channel Configuration Register 3	*/
	volatile unsigned char	__pad3[4];
	volatile unsigned char	vstr;		/* Version Status Register		*/
	volatile unsigned char	__pad4[3];
	volatile unsigned char	gis;		/* Global Interrupt Status		*/
	volatile unsigned char	ipc;		/* Interrupt Port Configuration		*/
	volatile unsigned char	isr0;		/* Interrupt Status 0			*/
	volatile unsigned char	isr1;		/* Interrupt Status 1			*/
	volatile unsigned char	pvr;		/* Port Value Register			*/
	volatile unsigned char	pis;		/* Port Interrupt Status		*/
	volatile unsigned char	pcr;		/* Port Configuration Register		*/
	volatile unsigned char	ccr4;		/* Channel Configuration Register 4	*/
};

struct sab82532_async_wr_regs 
{
	unsigned char	xfifo[0x20];	/* Transmit FIFO			*/
	unsigned char	cmdr;		/* Command Register			*/
	unsigned char	__pad1;
	unsigned char	mode;
	unsigned char	timr;
	unsigned char	xon;
	unsigned char	xoff;
	unsigned char	tcr;
	unsigned char	dafo;
	unsigned char	rfc;
	unsigned char	__pad2;
	unsigned char	xbcl;		/* Transmit Byte Count Low		*/
	unsigned char	xbch;		/* Transmit Byte Count High		*/
	unsigned char	ccr0;
	unsigned char	ccr1;
	unsigned char	ccr2;
	unsigned char	ccr3;
	unsigned char	tsax;		/* Time-Slot Assignment Reg. Transmit	*/
	unsigned char	tsar;		/* Time-Slot Assignment Reg. Receive	*/
	unsigned char	xccr;		/* Transmit Channel Capacity Register	*/
	unsigned char	rccr;		/* Receive Channel Capacity Register	*/
	unsigned char	bgr;		/* Baud Rate Generator Register		*/
	unsigned char	tic;		/* Transmit Immediate Character		*/
	unsigned char	mxn;		/* Mask XON Character			*/
	unsigned char	mxf;		/* Mask XOFF Character			*/
	unsigned char	iva;		/* Interrupt Vector Address		*/
	unsigned char	ipc;
	unsigned char	imr0;		/* Interrupt Mask Register 0		*/
	unsigned char	imr1;		/* Interrupt Mask Register 1		*/
	unsigned char	pvr;
	unsigned char	pim;		/* Port Interrupt Mask			*/
	unsigned char	pcr;
	unsigned char	ccr4;
};

struct sab82532_async_rw_regs 
{	/* Read/Write registers			*/
	volatile unsigned char	__pad1[0x20];
	volatile unsigned char	__pad2;
	volatile unsigned char	__pad3;
	volatile unsigned char	mode;
	volatile unsigned char	timr;
	volatile unsigned char	xon;
	volatile unsigned char	xoff;
	volatile unsigned char	tcr;
	volatile unsigned char	dafo;
	volatile unsigned char	rfc;
	volatile unsigned char	__pad4;
	volatile unsigned char	__pad5;
	volatile unsigned char	__pad6;
	volatile unsigned char	ccr0;
	volatile unsigned char	ccr1;
	volatile unsigned char	ccr2;
	volatile unsigned char	ccr3;
	volatile unsigned char	__pad7;
	volatile unsigned char	__pad8;
	volatile unsigned char	__pad9;
	volatile unsigned char	__pad10;
	volatile unsigned char	__pad11;
	volatile unsigned char	__pad12;
	volatile unsigned char	__pad13;
	volatile unsigned char	__pad14;
	volatile unsigned char	__pad15;
	volatile unsigned char	ipc;
	volatile unsigned char	__pad16;
	volatile unsigned char	__pad17;
	volatile unsigned char	pvr;
	volatile unsigned char	__pad18;
	volatile unsigned char	pcr;
	volatile unsigned char	ccr4;
};

union sab82532_async_regs 
{
	__volatile__ struct sab82532_async_rd_regs	r;
	__volatile__ struct sab82532_async_wr_regs	w;
	__volatile__ struct sab82532_async_rw_regs	rw;
};

/* not really used yet */
struct sab82532_hdlc_rd_regs 
{
	volatile unsigned char	rfifo[0x20];	/* Receive FIFO				*/
	volatile unsigned char	star;		/* Status Register			*/
	volatile unsigned char	rsta;
	volatile unsigned char	mode;		/* Mode Register			*/
	volatile unsigned char	timr;		/* Timer Register			*/
	volatile unsigned char	xad1;		/* Tx Address High 1	 		*/
	volatile unsigned char	xad2;		/* Tx Address High 2			*/
	volatile unsigned char	__pad1[2];
	volatile unsigned char	ral1;		/* Rx Address Low 1			*/
	volatile unsigned char	rhcr;		/* Received HDLC Control		*/
	volatile unsigned char	rbcl;		/* Receive Byte Count Low		*/
	volatile unsigned char	rbch;		/* Receive Byte Count High		*/
	volatile unsigned char	ccr0;		/* Channel Configuration Register 0	*/
	volatile unsigned char	ccr1;		/* Channel Configuration Register 1	*/
	volatile unsigned char	ccr2;		/* Channel Configuration Register 2	*/
	volatile unsigned char	ccr3;		/* Channel Configuration Register 3	*/
	volatile unsigned char	__pad2[4];
	volatile unsigned char	vstr;		/* Version Status Register		*/
	volatile unsigned char	__pad3[3];
	volatile unsigned char	gis;		/* Global Interrupt Status		*/
	volatile unsigned char	ipc;		/* Interrupt Port Configuration		*/
	volatile unsigned char	isr0;		/* Interrupt Status 0			*/
	volatile unsigned char	isr1;		/* Interrupt Status 1			*/
	volatile unsigned char	pvr;		/* Port Value Register			*/
	volatile unsigned char	pis;		/* Port Interrupt Status		*/
	volatile unsigned char	pcr;		/* Port Configuration Register		*/
	volatile unsigned char	ccr4;		/* Channel Configuration Register 4	*/
};

struct sab82532_hdlc_wr_regs 
{
	unsigned char	xfifo[0x20];	/* Transmit FIFO			*/
	unsigned char	cmdr;		/* Command Register			*/
	unsigned char	pre;		/* Preamble                             */
	unsigned char	mode;
	unsigned char	timr;
	unsigned char	xad1;		/* Tx Address High 1	 		*/
	unsigned char	xad2;		/* Tx Address High 2	 		*/
	unsigned char	rah1;		/* Rx Address High 1	 		*/
	unsigned char	rah2;		/* Rx Address High 2	 		*/
	unsigned char	ral1;		/* Rx Address Low 1			*/
	unsigned char	ral2;		/* Rx Address Low 2			*/
	unsigned char	xbcl;		/* Transmit Byte Count Low		*/
	unsigned char	xbch;		/* Transmit Byte Count High		*/
	unsigned char	ccr0;
	unsigned char	ccr1;
	unsigned char	ccr2;
	unsigned char	ccr3;
	unsigned char	tsax;		/* Time-Slot Assignment Reg. Transmit	*/
	unsigned char	tsar;		/* Time-Slot Assignment Reg. Receive	*/
	unsigned char	xccr;		/* Transmit Channel Capacity Register	*/
	unsigned char	rccr;		/* Receive Channel Capacity Register	*/
	unsigned char	bgr;		/* Baud Rate Generator Register		*/
	unsigned char	rlcr;		/* Rx Frame Length Check		*/
	unsigned char	aml;		/* Address Mask Low			*/
	unsigned char	amh;		/* Address Mask High			*/
	unsigned char	iva;		/* Interrupt Vector Address		*/
	unsigned char	ipc;
	unsigned char	imr0;		/* Interrupt Mask Register 0		*/
	unsigned char	imr1;		/* Interrupt Mask Register 1		*/
	unsigned char	pvr;
	unsigned char	pim;		/* Port Interrupt Mask			*/
	unsigned char	pcr;
	unsigned char	ccr4;
};

union sab82532_regs 
{
	__volatile__ struct sab82532_async_rd_regs	async_read;
	__volatile__ struct sab82532_async_wr_regs	async_write;
	__volatile__ struct sab82532_hdlc_rd_regs	hdlc_read;
	__volatile__ struct sab82532_hdlc_wr_regs	hdlc_write;
};

/*
 * Modem signal definition
 */

typedef struct mctlsig 
{
	unsigned char *reg;	/* chip register offset */
	unsigned char inverted;	/* interpret the results as inverted */
	unsigned char mask;	/* bit within that register */
	unsigned char val;	/* cached value */
	unsigned int irq;	/* address of correct isr register */
	unsigned char irqmask;	/*  */
	unsigned char cnst;	/* A value that should always be set for
				 * this signal register */ 
} mctlsig_t, MCTLSIG;

union sab8253x_irq_status 
{
	unsigned int stat;
	unsigned char images[4];
	struct
	{
		unsigned char isr0;
		unsigned char isr1;
		unsigned char pis;
	} sreg;
};
				/* the following are deprecated */
				/* older version of structure above */
				/* used array*/
#define      ISR0_IDX 0
#define      ISR1_IDX 1
#define       PIS_IDX 2

/*
 * Each port has a structure like this associated to it.
 * All the port are linked with the  next fields
 * All the port of one chip are linked with the  next_by_chip
 */

#define	FUNCTION_NR	0
#define	FUNCTION_AO	1
#define	FUNCTION_NA	2
#define FUNCTION_UN	3

typedef struct sab_port 
{
	int				magic;
	union sab82532_regs		*regs;
	struct sab_chip			*chip;
	struct sab_board		*board;
	struct tty_struct		*tty;
	unsigned char			*xmit_buf;
	int				 xmit_head; /* put characters on write */
	int				 xmit_tail; /* read from here -- in writeb or writefifo */
	int				 xmit_cnt;
	
	/*
	 * Various linked list pertinent to this link 
	 */
	struct sab_port * next;
	struct sab_port * next_by_chip;
	struct sab_port * next_by_board;
	struct sab_port * next_by_cim;
	
	struct net_device *dev;
	struct net_device *next_dev;
	
	struct fasync_struct * async_queue;
	struct sk_buff_head *sab8253xbuflist; /* need to keep */
				/* a list of all */
				/* skbuffs so that */
				/* we can guarantee */
				/* freeing all when */
				/* the PPC is stopped */
				/* on close*/
	struct sk_buff_head *sab8253xc_rcvbuflist; /* used for passing */
				/* buffers from interrupt */
				/* receive process*/
	
	
	DCONTROL2 dcontrol2;
	DCONTROL2 active2;
	DCONTROL2 sabnext2;

	int			DoingInterrupt;
	int			irq;
	int			flags;	       /* suggested by serial.h
						* but this driver is
						* more general */
	int			syncflags;
	int			type;	       /* SAB82532/8 version */
	unsigned int		function;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			xmit_fifo_size;
	int			recv_fifo_size;
	int			custom_divisor;
	unsigned long		baud;
	unsigned int		ebrg;
	unsigned int		cec_timeout;
	unsigned int		tec_timeout;

	int			x_char;
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	int			all_sent;
	int			is_console;
#define OPEN_NOT 0
#define OPEN_ASYNC 1
#define OPEN_SYNC 2
#define OPEN_BSC 3
#define OPEN_RAW 4
#define OPEN_SYNC_NET 5
#define OPEN_SYNC_CHAR 6
	unsigned int		open_type; /* Sync? Async?, int better for hw bps */
	unsigned char	        interrupt_mask0;
	unsigned char		interrupt_mask1;
				/* Modem signals */
	mctlsig_t		dtr;
	mctlsig_t		dsr;
	mctlsig_t		dcd;
	mctlsig_t		cts;
	mctlsig_t		rts;
	mctlsig_t		txclkdir; /* Direction of TxClk */
	unsigned long           custspeed; /* Custom speed */
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			count;
	int			blocked_open;
	long			session;
	long			pgrp;
	struct tq_struct	tqueue;
	struct tq_struct	tqueue_hangup;
	struct async_icount	icount;
	struct termios		normal_termios;
	struct termios		callout_termios;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
	wait_queue_head_t	delta_msr_wait;
	wait_queue_head_t	read_wait;
	wait_queue_head_t	write_wait;
  
  /*
   * Pointer to FIFO access routines.  These are individualized
   *  by hardware because different hardware may have different
   *  ways to get to the FIFOs.
   */
  
	void			(*readfifo)(struct sab_port *port, 
					    unsigned char *buf,
					    u32 nbytes);
	void			(*writefifo)(struct sab_port *port);
  
  /*
   * Pointer to register access routines.  These are individualized
   *  by hardware because different hardware may have different
   *  ways to get to the register.
   */
	unsigned char		(*readbyte)(struct sab_port *port, 
					    unsigned char *reg);
	void	        	(*writebyte)(struct sab_port *port, 
					     unsigned char *reg,
					     unsigned char val);
	u16			(*readword)(struct sab_port *port, 
					    u16 *reg);
	void	        	(*writeword)(struct sab_port *port, 
					     u16 *reg,
					     u16 val);
  
  /*
   * Pointer to protocol functions
   *
   */
  
	unsigned int portno;
	void (*receive_chars)(struct sab_port *port, 
			      union sab8253x_irq_status *stat);
	void (*transmit_chars)(struct sab_port *port,
			       union sab8253x_irq_status *stat);
	void (*check_status)(struct sab_port *port,
			     union sab8253x_irq_status *stat);
	unsigned int receive_test;
	unsigned int transmit_test;
	unsigned int check_status_test;
	struct channelcontrol ccontrol;
	
	unsigned int tx_full;
	unsigned int rx_empty;
	struct counters Counters;
	struct net_device_stats stats;
	
				/* collect statistics for netstat */
				/* etc.  those programs don't know */
				/* about priorities*/

	int			msgbufindex;
	char			msgbuf[RXSIZE];
	unsigned int		buffergreedy;
	unsigned int		sigmode;
} sab_port_t, SAB_PORT;

/*
 * per-parallel port structure
 */

/* SAB82538 4 8-bits parallel ports
 * To summarize the use of the parallel port:
 *                    RS-232
 * Parallel port A -- TxClkdir control	(output) ports 0 - 7
 * Parallel port B -- DTR		(output) ports 0 - 7
 * Parallel port C -- DSR		(input)  ports 0 - 7
 * Parallel port D -- driver power down	(output) drivers 0 - 3
 *
 * SAB82532 (Aurora) 1 8-bit parallel port
 * To summarize the use of the parallel port:
 *                    RS-232
 *  A       B        I/O     descr
 * P0      P4      output  TxClk ctrl
 * P1      P5      output  DTR
 * P2      P6      input   DSR
 * P3      P7      output  485 control
 *
 * Note that this new version of the driver
 * does not support the SPARC motherboard ESCC2
 *
 * SAB82532 (Sun) 1 8-bit parallel port
 * To summarize the use of the parallel port:
 *                    RS-232
 *  A       B        I/O     descr
 * P0      P3      input    DSR
 * P1      P2      output    DTR
 * P5      P6      input     ?
 * P4      P7      output    ?
 *
 */
				/* not sure how usefule */
typedef struct sabparport 
{
  /* cached values: */
	unsigned char	 pp_pcr;
	unsigned char	 pp_pim;
	unsigned char	 pp_pvr;
	
	/* register offsets: */
	unsigned int	 pp_pcrreg;
	unsigned int	 pp_pimreg;
	unsigned int	 pp_pisreg;
	unsigned int	 pp_pvrreg;
} sabparport_t, SABPARPORT;

#define ESCC2	2
#define ESCC8	8

/*
 * Per-chip structure
 */


typedef struct sab_chip 
{
	unsigned int		chip_type;
	struct sab_board	*c_board;	/* Parent board */
	struct aura_cim		*c_cim;		/* if present */
	unsigned char		c_chipno;	/* chip number */
	unsigned char	 	c_revision;	/* the revision code from the VSTR */
	unsigned char         	c_nports;	/* number of ports per chip */
	void 			*c_regs;	/* base address for chip registers */
	struct sab_port		*c_portbase;
	struct sab_chip       	*next;          /* the next chip in the chip chain */
	struct sab_chip       	*next_by_board;    /* the next chip on the board */
	struct sab_chip		*next_by_cim;
	void 			(*int_disable)(struct sab_chip* chip);
} sab_chip_t, SAB_CHIP;


/* Some useful facts */
#define SAB82532_REG_SIZE               0x40
#define SAB82538_REG_SIZE               0x40

/* RFIFO Status Byte */
#define SAB82532_RSTAT_PE		0x80
#define SAB82532_RSTAT_FE		0x40
#define SAB82532_RSTAT_PARITY		0x01

/* Status Register (STAR) */
#define SAB82532_STAR_XDOV		0x80
#define SAB82532_STAR_XFW		0x40
#define SAB82532_STAR_RFNE		0x20
#define SAB82532_STAR_FCS		0x10
#define SAB82532_STAR_TEC		0x08
#define SAB82532_STAR_RLI		0x08
#define SAB82532_STAR_CEC		0x04
#define SAB82532_STAR_CTS		0x02

/* Command Register (CMDR) */
#define SAB82532_CMDR_RMC		0x80
#define SAB82532_CMDR_RRES		0x40
#define SAB82532_CMDR_RHR		0x40
#define SAB82532_CMDR_RFRD		0x20
#define SAB82532_CMDR_STI		0x10
#define SAB82532_CMDR_XF		0x08
#define SAB82532_CMDR_XTF		0x08
#define SAB82532_CMDR_XME		0x02
#define SAB82532_CMDR_XRES		0x01

				/* leaving them for reference */
				/* they are now defined in 8253xioc.h*/
#if 0
/* Mode Register (MODE) */
#define SAB82532_MODE_TM0		0x80
#define SAB82532_MODE_FRTS		0x40
#define SAB82532_MODE_FCTS		0x20
#define SAB82532_MODE_FLON		0x10
#define SAB82532_MODE_TCPU		0x10
#define SAB82532_MODE_RAC		0x08
#define SAB82532_MODE_RTS		0x04
#define SAB82532_MODE_TRS		0x02
#define SAB82532_MODE_TLP		0x01
#endif

/* Receive Status Register (READ)  */
#define SAB82532_RSTA_VFR		0x80
#define SAB82532_RSTA_RDO		0x40
#define SAB82532_RSTA_CRC		0x20
#define SAB82532_RSTA_RAB		0x10

/* Timer Register (TIMR) */
#define SAB82532_TIMR_CNT_MASK		0xe0
#define SAB82532_TIMR_VALUE_MASK	0x1f

/* Data Format (DAFO) */
#define SAB82532_DAFO_XBRK		0x40
#define SAB82532_DAFO_STOP		0x20
#define SAB82532_DAFO_PAR_SPACE		0x00
#define SAB82532_DAFO_PAR_ODD		0x08
#define SAB82532_DAFO_PAR_EVEN		0x10
#define SAB82532_DAFO_PAR_MARK		0x18
#define SAB82532_DAFO_PARE		0x04
#define SAB82532_DAFO_CHL8		0x00
#define SAB82532_DAFO_CHL7		0x01
#define SAB82532_DAFO_CHL6		0x02
#define SAB82532_DAFO_CHL5		0x03

/* RFIFO Control Register (RFC) */
#define SAB82532_RFC_DPS		0x40
#define SAB82532_RFC_DXS		0x20
#define SAB82532_RFC_RFDF		0x10
#define SAB82532_RFC_RFTH_1		0x00
#define SAB82532_RFC_RFTH_4		0x04
#define SAB82532_RFC_RFTH_16		0x08
#define SAB82532_RFC_RFTH_32		0x0c
#define SAB82532_RFC_TCDE		0x01

/* Received Byte Count High (RBCH) */
#define SAB82532_RBCH_DMA		0x80
#define SAB82532_RBCH_CAS		0x20
#define SAB82532_RBCH_OV		0x10
#define SAB82532_RBCH_HMSK		0x0F

/* Transmit Byte Count High (XBCH) */
#define SAB82532_XBCH_DMA		0x80
#define SAB82532_XBCH_CAS		0x20
#define SAB82532_XBCH_XC		0x10

				/* leaving them for reference */
				/* they are now defined in 8253xioc.h*/
#if 0
/* Channel Configuration Register 0 (CCR0) */
#define SAB82532_CCR0_PU		0x80
#define SAB82532_CCR0_MCE		0x40
#define SAB82532_CCR0_SC_NRZ		0x00
#define SAB82532_CCR0_SC_NRZI		0x08
#define SAB82532_CCR0_SC_FM0		0x10
#define SAB82532_CCR0_SC_FM1		0x14
#define SAB82532_CCR0_SC_MANCH		0x18
#define SAB82532_CCR0_SM_HDLC		0x00
#define SAB82532_CCR0_SM_SDLC_LOOP	0x01
#define SAB82532_CCR0_SM_BISYNC		0x02
#define SAB82532_CCR0_SM_ASYNC		0x03

/* Channel Configuration Register 1 (CCR1) */
#define SAB82532_CCR1_SFLG		0x80
#define SAB82532_CCR1_ODS		0x10
#define SAB82532_CCR1_BCR		0x08
#define SAB82532_CCR1_IFF		0x08
#define SAB82532_CCR1_ITF		0x00
#define SAB82532_CCR1_CM_MASK		0x07

/* Channel Configuration Register 2 (CCR2) */
#define SAB82532_CCR2_SOC1		0x80
#define SAB82532_CCR2_SOC0		0x40
#define SAB82532_CCR2_BR9		0x80
#define SAB82532_CCR2_BR8		0x40
#define SAB82532_CCR2_BDF		0x20
#define SAB82532_CCR2_SSEL		0x10
#define SAB82532_CCR2_XCS0		0x20
#define SAB82532_CCR2_RCS0		0x10
#define SAB82532_CCR2_TOE		0x08
#define SAB82532_CCR2_RWX		0x04
#define SAB82532_CCR2_C32		0x02
#define SAB82532_CCR2_DIV		0x01

/* Channel Configuration Register 3 (CCR3) */
#define SAB82532_CCR3_PSD		0x01
#define SAB82532_CCR3_RCRC		0x04
#endif

/* Time Slot Assignment Register Transmit (TSAX) */
#define SAB82532_TSAX_TSNX_MASK		0xfc
#define SAB82532_TSAX_XCS2		0x02	/* see also CCR2 */
#define SAB82532_TSAX_XCS1		0x01

/* Time Slot Assignment Register Receive (TSAR) */
#define SAB82532_TSAR_TSNR_MASK		0xfc
#define SAB82532_TSAR_RCS2		0x02	/* see also CCR2 */
#define SAB82532_TSAR_RCS1		0x01

/* Version Status Register (VSTR) */
#define SAB85232_REG_VSTR               0x34
#define SAB82532_VSTR_CD		0x80
#define SAB82532_VSTR_DPLA		0x40
#define SAB82532_VSTR_VN_MASK		0x0f
#define SAB82532_VSTR_VN_1		0x00
#define SAB82532_VSTR_VN_2		0x01
#define SAB82532_VSTR_VN_3_2		0x02

/* Global Interrupt Status Register (GIS) */
#define SAB82532_GIS_PI			0x80
#define SAB82532_GIS_ISA1		0x08
#define SAB82532_GIS_ISA0		0x04
#define SAB82532_GIS_ISB1		0x02
#define SAB82532_GIS_ISB0		0x01
#define SAB82532_GIS_MASK		0x8f 
#define SAB82538_GIS_PIA		0x80
#define SAB82538_GIS_PIB		0x40
#define SAB82538_GIS_PIC		0x20
#define SAB82538_GIS_PID		0x10
#define SAB82538_GIS_CII		0x08
#define SAB82538_GIS_CHNL_MASK          0x07
#define SAB82538_GIS_MASK		0x28  /* Port C and CII ! */ 

/* Interrupt Vector Address (IVA) */
#define SAB82532_REG_IVA                0x38
#define SAB82532_IVA_MASK		0xf1
#define SAB82538_IVA_ROT                0x02

/* Interrupt Port Configuration (IPC) */
#define SAB82532_REG_IPC                0x39
#define SAB82532_IPC_VIS		0x80
#define SAB82532_IPC_SLA1		0x10
#define SAB82532_IPC_SLA0		0x08
#define SAB82532_IPC_CASM		0x04
#define SAB82532_IPC_IC_OPEN_DRAIN	0x00
#define SAB82532_IPC_IC_ACT_LOW		0x01
#define SAB82532_IPC_IC_ACT_HIGH	0x03

/* Interrupt Status Register 0 (ISR0) */
#define SAB82532_ISR0_TCD		0x80
#define SAB82532_ISR0_RME		0x80
#define SAB82532_ISR0_TIME		0x40
#define SAB82532_ISR0_RFS		0x40
#define SAB82532_ISR0_PERR		0x20
#define SAB82532_ISR0_FERR		0x10
#define SAB82532_ISR0_PLLA		0x08
#define SAB82532_ISR0_CDSC		0x04
#define SAB82532_ISR0_RFO		0x02
#define SAB82532_ISR0_RPF		0x01

/* Interrupt Status Register 1 (ISR1) */
#define SAB82532_ISR1_BRK		0x80
#define SAB82532_ISR1_BRKT		0x40
#define SAB82532_ISR1_RDO		0x40
#define SAB82532_ISR1_ALLS		0x20
#define SAB82532_ISR1_XOFF		0x10
#define SAB82532_ISR1_XDU		0x10
#define SAB82532_ISR1_TIN		0x08
#define SAB82532_ISR1_CSC		0x04
#define SAB82532_ISR1_XON		0x02
#define SAB82532_ISR1_XPR		0x01

/* Interrupt Mask Register 0 (IMR0) */
#define SAB82532_IMR0_TCD		0x80
#define SAB82532_IMR0_RME		0x80
#define SAB82532_IMR0_TIME		0x40
#define SAB82532_IMR0_RFS		0x40
#define SAB82532_IMR0_PERR		0x20
#define SAB82532_IMR0_RSC		0x20
#define SAB82532_IMR0_FERR		0x10
#define SAB82532_IMR0_PCE		0x10
#define SAB82532_IMR0_PLLA		0x08
#define SAB82532_IMR0_CDSC		0x04
#define SAB82532_IMR0_RFO		0x02
#define SAB82532_IMR0_RPF		0x01

/* Interrupt Mask Register 1 (IMR1) */
#define SAB82532_IMR1_BRK		0x80
#define SAB82532_IMR1_EOP		0x80
#define SAB82532_IMR1_BRKT		0x40
#define SAB82532_IMR1_RDO		0x40
#define SAB82532_IMR1_ALLS		0x20
#define SAB82532_IMR1_XOFF		0x10
#define SAB82532_IMR1_EXE		0x10
#define SAB82532_IMR1_TIN		0x08
#define SAB82532_IMR1_CSC		0x04
#define SAB82532_IMR1_XON		0x02
#define SAB82532_IMR1_XMR		0x02
#define SAB82532_IMR1_XPR		0x01

/* Port Value Register (PVR) */
#define SAB82532_REG_PVR                 0x3c
#define SAB82538_REG_PVR_A               0x3c
#define SAB82538_REG_PVR_B               0xbc
#define SAB82538_REG_PVR_C               0x13c
#define SAB82538_REG_PVR_D               0x1bc

/* Port Value Register (PIM) */
#define SAB82532_REG_PIM                 0x3d
#define SAB82538_REG_PIM_A               0x3d
#define SAB82538_REG_PIM_B               0xbd
#define SAB82538_REG_PIM_C               0x13d
#define SAB82538_REG_PIM_D               0x1bd
/* Port Value Register (PIS) */
#define SAB82532_REG_PIS                 0x3d
#define SAB82538_REG_PIS_A               0x3d
#define SAB82538_REG_PIS_B               0xbd
#define SAB82538_REG_PIS_C               0x13d
#define SAB82538_REG_PIS_D               0x1bd

/* Port Value Register (PCR) */
#define SAB82532_REG_PCR                 0x3e
#define SAB82538_REG_PCR_A               0x3e
#define SAB82538_REG_PCR_B               0xbe
#define SAB82538_REG_PCR_C               0x13e
#define SAB82538_REG_PCR_D               0x1be

				/* leaving them for reference */
				/* they are now defined in 8253xioc.h*/

#if 0
/* Channel Configuration Register 4 (CCR4) */
#define SAB82532_CCR4_MCK4		0x80/* needs to be set when board clock */
					    /* over 10 Mhz (?)*/
#define SAB82532_CCR4_EBRG		0x40
#define SAB82532_CCR4_TST1		0x20
#define SAB82532_CCR4_ICD		0x10
#endif


/* Port Interrupt Status Register (PIS) */
#define SAB82532_PIS_SYNC_B		0x08
#define SAB82532_PIS_DTR_B		0x04
#define SAB82532_PIS_DTR_A		0x02
#define SAB82532_PIS_SYNC_A		0x01


/* More things useful */
#define SAB_MAGIC 5977

/* When computing the baudrate, we "encode" it by multiplying
 * the actual baudrate by 2. This way we can use 134.5
 */
#define ENCODEBR(x)  ((x)<<1)

/*
 * Raise a modem signal y on port x, tmpval must exist! */
#define RAISE(xx,y) \
{ \
	  unsigned char __tmpval__; \
	  __tmpval__= (xx)->readbyte((xx),(xx)->y.reg);\
	  if((xx)->y.inverted)\
	    __tmpval__ &= ~((xx)->y.mask);\
	  else\
	    __tmpval__ |= (xx)->y.mask;\
	  __tmpval__ |= (xx)->y.cnst;\
	  (xx)->y.val=1;\
	  (xx)->writebyte((xx),(xx)->y.reg,__tmpval__);\
}
/*
 * Lower a modem signal y on port x, __tmpval__ must exist! */
#define LOWER(xx,y) \
{\
	  unsigned char __tmpval__; \
	  __tmpval__= (xx)->readbyte((xx),(xx)->y.reg);\
	  if((xx)->y.inverted)\
	    __tmpval__ |= (xx)->y.mask;\
	  else\
	    __tmpval__ &= ~((xx)->y.mask);\
	  __tmpval__ |= (xx)->y.cnst;\
	  (xx)->y.val=0;\
	  (xx)->writebyte((xx),(xx)->y.reg,__tmpval__);\
}

#define ISON(xx,y) \
          ((xx)->y.inverted != (((xx)->readbyte((xx),(xx)->y.reg)&(xx)->y.mask) ==(xx)->y.mask) )
/*
 * Now let's define all those functions we need else where.
 *
 * This should probably be reorganized
 */
extern void sab8253x_setup_ttydriver(void);
extern int finish_sab8253x_setup_ttydriver(void);
extern void sab8253x_setup_ttyport(struct sab_port *port);
extern void sab8253x_cleanup_ttydriver(void);
extern void sab8253x_start_txS(struct sab_port *port);

static int inline sab8253x_serial_paranoia_check(struct sab_port *port,
						 kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null sab8253x for (%s) in %s\n";
	
	if (!port) 
	{
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (port->magic != SAB_MAGIC) 
	{
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

static void inline sab8253x_cec_wait(struct sab_port *port)
{
	int timeout = port->cec_timeout;

#if 1				/* seems to work for 82532s */
	while ((READB(port, star) & SAB82532_STAR_CEC) && --timeout)
	{
		udelay(1);
	}
#else
	if (READB(port,star) & SAB82532_STAR_CEC)
	{
		udelay(1);
	}
#endif
}

extern void sab8253x_transmit_charsS(struct sab_port *port, union sab8253x_irq_status *stat);
extern void sab8253x_flush_charsS(struct tty_struct *tty);
extern void sab8253x_set_termiosS(struct tty_struct *tty, struct termios *old_termios);
extern void sab8253x_stopS(struct tty_struct *tty);
extern void sab8253x_startS(struct tty_struct *tty);
extern void sab8253x_send_xcharS(struct tty_struct *tty, char ch);
extern void sab8253x_transmit_charsN(struct sab_port *port,
				     union sab8253x_irq_status *stat);
extern SAB_PORT  *AuraPortRoot;

#define MAX_SAB8253X_RCV_QUEUE_LEN 50

struct ebrg_struct 
{
	int	baud;
	int	n;
	int	m;
};

extern task_queue tq_8253x_serial;

extern int sab8253x_openS(struct tty_struct *tty, struct file * filp);
extern void sab8253x_closeS(struct tty_struct *tty, struct file * filp);
extern int sab8253x_writeS(struct tty_struct * tty, int from_user,
			   const unsigned char *buf, int count);
extern void sab8253x_throttleS(struct tty_struct * tty);
extern void sab8253x_unthrottleS(struct tty_struct * tty);
extern void sab8253x_hangupS(struct tty_struct *tty);
extern void sab8253x_breakS(struct tty_struct *tty, int break_state);
extern void Sab8253xCleanUpTransceiveN(SAB_PORT* priv);

				/* used for running routines in the */
				/* soft int part of the driver */
				/* at one time the flip buffer routines */
				/* seem to have been too time consuming */
				/* to invoke in the hardware interrupt */
				/* routing -- I am not so sure there is */
				/* a problem with modern processor and */
				/* memory speeds, but maybe there is a */
				/* requirement that certain tty routines */
				/* not be executed with ints turned off.*/

static void inline sab8253x_sched_event(struct sab_port *port, int event)
{
	port->event |= 1 << event;
	queue_task(&port->tqueue, &tq_8253x_serial);
	mark_bh(AURORA_BH);
}

extern unsigned int 
sab8253x_baud(sab_port_t *port, unsigned long encbaud,
	      unsigned char *bgr, unsigned char *ccr2,
	      unsigned char *ccr4, unsigned long *truebaudp);
extern void Sab8253xFreeAllReceiveListSKBUFFS(SAB_PORT* priv);
extern int Sab8253xSetUpLists(SAB_PORT *priv);
extern int Sab8253xCountTransmitDescriptors(SAB_PORT *port);
extern int Sab8253xCountTransmit(SAB_PORT *port);
extern void sab8253x_init_lineS(struct sab_port *port);
extern void sab8253x_init_lineS(struct sab_port *port);
extern int getccr2configS(struct sab_port *port);
extern void sab8253x_change_speedN(struct sab_port *port);
extern void sab8253x_shutdownN(struct sab_port *port);
extern int sab8253x_startupN(struct sab_port *port);
extern int sab8253x_block_til_ready(struct tty_struct *tty, struct file * filp,
				    struct sab_port *port);
extern void sab8253x_wait_until_sent(struct tty_struct *tty, int timeout);
extern void sab8253x_flush_buffer(struct tty_struct *tty);
extern void aura_sp502_program(SAB_PORT *port, unsigned int index);
#endif 
