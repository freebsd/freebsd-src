/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)com.c	7.5 (Berkeley) 5/16/91
 *	$Id: sio.c,v 1.62 1998/06/24 13:37:23 kato Exp $
 */

#include "opt_comconsole.h"
#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_devfs.h"
#include "opt_sio.h"
#include "sio.h"
#include "pnp.h"

#ifndef EXTRA_SIO
#if NPNP > 0
#define EXTRA_SIO 2
#else
#define EXTRA_SIO 0
#endif
#endif

#define NSIOTOT (NSIO + EXTRA_SIO)

/*
 * Serial driver, based on 386BSD-0.1 com driver.
 * Mostly rewritten to use pseudo-DMA.
 * Works for National Semiconductor NS8250-NS16550AF UARTs.
 * COM driver, based on HP dca driver.
 *
 * Changes for PC-Card integration:
 *	- Added PC-Card driver table and handlers
 */
/*===============================================================
 * 386BSD(98),FreeBSD-1.1x(98) com driver.
 * -----
 * modified for PC9801 by M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 * Chou "TEFUTEFU" Hirotomi
 *			Kyoto Univ.  the faculty of medicine
 *===============================================================
 * FreeBSD-2.0.1(98) sio driver.
 * -----
 * modified for pc98 Internal i8251 and MICRO CORE MC16550II
 *			T.Koike(hfc01340@niftyserve.or.jp)
 * implement kernel device configuration
 *			aizu@orient.center.nitech.ac.jp
 *
 * Notes.
 * -----
 *  PC98 localization based on 386BSD(98) com driver. Using its PC98 local
 *  functions.
 *  This driver is under debugging,has bugs.
 *
 * 1) config
 *  options COM_MULTIPORT  #if using MC16550II
 *  device sio0 at nec? port 0x30  tty irq 4 vector siointr #internal
 *  device sio1 at nec? port 0xd2  tty irq 5 flags 0x101 vector siointr #mc1
 *  device sio2 at nec? port 0x8d2 tty flags 0x101 vector siointr       #mc2
 *                         # ~~~~~iobase        ~~multi port flag
 *                         #                   ~  master device is sio1
 * 2) device
 *  cd /dev; MAKEDEV ttyd0 ttyd1 ..
 * 3) /etc/rc.serial
 *  57600bps is too fast for sio0(internal8251)
 *  my ex.
 *    #set default speed 9600
 *    modem()
 *       :
 *      stty </dev/ttyid$i crtscts 9600
 *       :                 #       ~~~~ default speed(can change after init.)
 *    modem 0 1 2
 * 4) COMCONSOLE
 *  not changed.
 * 5) PC9861K,PIO9032B,B98_01
 *  not tested.
 */
/*
 * modified for AIWA B98-01
 * by T.Hatanou <hatanou@yasuda.comm.waseda.ac.jp>  last update: 15 Sep.1995 
 *
 * How to configure...
 *   # options COM_MULTIPORT         # support for MICROCORE MC16550II
 *      ... comment-out this line, which will conflict with B98_01.
 *   options "B98_01"                # support for AIWA B98-01
 *   device  sio1 at nec? port 0x00d1 tty irq ? vector siointr
 *   device  sio2 at nec? port 0x00d5 tty irq ? vector siointr
 *      ... you can leave these lines `irq ?', irq will be autodetected.
 */
#ifdef PC98
#define	MC16550		0
#define COM_IF_INTERNAL	1
#if 0
#define COM_IF_PC9861K	2
#define COM_IF_PIO9032B	3
#endif
#ifdef	B98_01
#undef  COM_MULTIPORT	/* COM_MULTIPORT will conflict with B98_01 */
#define COM_IF_B98_01	4
#endif /* B98_01 */
#endif /* PC98 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif

#include <machine/clock.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <i386/isa/icu.h>
#include <i386/isa/isa_device.h>
#include <pc98/pc98/sioreg.h>
#include <i386/isa/ic/i8251.h>
#else
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/sioreg.h>
#endif
#include <i386/isa/intr_machdep.h>

#ifdef COM_ESP
#include <i386/isa/ic/esp.h>
#endif
#include <i386/isa/ic/ns16550.h>

#include "card.h"
#if NCARD > 0
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#endif

#if NPNP > 0
#include <i386/isa/pnp.h>
#endif

#ifdef SMP
#define disable_intr()	COM_DISABLE_INTR()
#define enable_intr()	COM_ENABLE_INTR()
#endif /* SMP */

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */
#define	RB_I_HIGH_WATER	(TTYHOG - 2 * RS_IBUFSIZE)
#define	RS_IBUFSIZE	256

#define	CALLOUT_MASK		0x80
#define	CONTROL_MASK		0x60
#define	CONTROL_INIT_STATE	0x20
#define	CONTROL_LOCK_STATE	0x40
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	((mynor) & ~MINOR_MAGIC_MASK)

#ifdef COM_MULTIPORT
/* checks in flags for multiport and which is multiport "master chip"
 * for a given card
 */
#define	COM_ISMULTIPORT(dev)	((dev)->id_flags & 0x01)
#define	COM_MPMASTER(dev)	(((dev)->id_flags >> 8) & 0x0ff)
#define	COM_NOTAST4(dev)	((dev)->id_flags & 0x04)
#endif /* COM_MULTIPORT */

#define	COM_CONSOLE(dev)	((dev)->id_flags & 0x10)
#define	COM_FORCECONSOLE(dev)	((dev)->id_flags & 0x20)
#define	COM_LLCONSOLE(dev)	((dev)->id_flags & 0x40)
#define	COM_LOSESOUTINTS(dev)	((dev)->id_flags & 0x08)
#define	COM_NOFIFO(dev)		((dev)->id_flags & 0x02)
#define COM_ST16650A(dev)	((dev)->id_flags & 0x20000)
#define COM_C_NOPROBE     (0x40000)
#define COM_NOPROBE(dev)  ((dev)->id_flags & COM_C_NOPROBE)
#define COM_C_IIR_TXRDYBUG    (0x80000)
#define COM_IIR_TXRDYBUG(dev) ((dev)->id_flags & COM_C_IIR_TXRDYBUG)
#define	COM_FIFOSIZE(dev)	(((dev)->id_flags & 0xff000000) >> 24)

#ifndef PC98
#define	com_scr		7	/* scratch register for 16450-16550 (R/W) */
#endif /* !PC98 */

/*
 * Input buffer watermarks.
 * The external device is asked to stop sending when the buffer exactly reaches
 * high water, or when the high level requests it.
 * The high level is notified immediately (rather than at a later clock tick)
 * when this watermark is reached.
 * The buffer size is chosen so the watermark should almost never be reached.
 * The low watermark is invisibly 0 since the buffer is always emptied all at
 * once.
 */
#define	RS_IHIGHWATER (3 * RS_IBUFSIZE / 4)

/*
 * com state bits.
 * (CS_BUSY | CS_TTGO) and (CS_BUSY | CS_TTGO | CS_ODEVREADY) must be higher
 * than the other bits so that they can be tested as a group without masking
 * off the low bits.
 *
 * The following com and tty flags correspond closely:
 *	CS_BUSY		= TS_BUSY (maintained by comstart(), siopoll() and
 *				   siostop())
 *	CS_TTGO		= ~TS_TTSTOP (maintained by comparam() and comstart())
 *	CS_CTS_OFLOW	= CCTS_OFLOW (maintained by comparam())
 *	CS_RTS_IFLOW	= CRTS_IFLOW (maintained by comparam())
 * TS_FLUSH is not used.
 * XXX I think TIOCSETA doesn't clear TS_TTSTOP when it clears IXON.
 * XXX CS_*FLOW should be CF_*FLOW in com->flags (control flags not state).
 */
#define	CS_BUSY		0x80	/* output in progress */
#define	CS_TTGO		0x40	/* output not stopped by XOFF */
#define	CS_ODEVREADY	0x20	/* external device h/w ready (CTS) */
#define	CS_CHECKMSR	1	/* check of MSR scheduled */
#define	CS_CTS_OFLOW	2	/* use CTS output flow control */
#define	CS_DTR_OFF	0x10	/* DTR held off */
#define	CS_ODONE	4	/* output completed */
#define	CS_RTS_IFLOW	8	/* use RTS input flow control */
#define	CSE_BUSYCHECK	1	/* siobusycheck() scheduled */

static	char const * const	error_desc[] = {
#define	CE_OVERRUN			0
	"silo overflow",
#define	CE_INTERRUPT_BUF_OVERFLOW	1
	"interrupt-level buffer overflow",
#define	CE_TTY_BUF_OVERFLOW		2
	"tty-level buffer overflow",
};

#define	CE_NTYPES			3
#define	CE_RECORD(com, errnum)		(++(com)->delta_error_counts[errnum])

/* types.  XXX - should be elsewhere */
typedef u_int	Port_t;		/* hardware port */
typedef u_char	bool_t;		/* boolean */

/* queue of linear buffers */
struct lbq {
	u_char	*l_head;	/* next char to process */
	u_char	*l_tail;	/* one past the last char to process */
	struct lbq *l_next;	/* next in queue */
	bool_t	l_queued;	/* nonzero if queued */
};

/* com device structure */
struct com_s {
	u_int	id_flags;	/* Copy isa device falgas */
	u_char	state;		/* miscellaneous flag bits */
	bool_t  active_out;	/* nonzero if the callout device is open */
	u_char	cfcr_image;	/* copy of value written to CFCR */
#ifdef COM_ESP
	bool_t	esp;		/* is this unit a hayes esp board? */
#endif
	u_char	extra_state;	/* more flag bits, separate for order trick */
	u_char	fifo_image;	/* copy of value written to FIFO */
	bool_t	hasfifo;	/* nonzero for 16550 UARTs */
	bool_t	st16650a;	/* Is a Startech 16650A or RTS/CTS compat */
	bool_t	loses_outints;	/* nonzero if device loses output interrupts */
	u_char	mcr_image;	/* copy of value written to MCR */
#ifdef COM_MULTIPORT
	bool_t	multiport;	/* is this unit part of a multiport device? */
#endif /* COM_MULTIPORT */
	bool_t	no_irq;		/* nonzero if irq is not attached */
	bool_t  gone;		/* hardware disappeared */
	bool_t	poll;		/* nonzero if polling is required */
	bool_t	poll_output;	/* nonzero if polling for output is required */
	int	unit;		/* unit	number */
	int	dtr_wait;	/* time to hold DTR down on close (* 1/hz) */
	u_int	tx_fifo_size;
	u_int	wopeners;	/* # processes waiting for DCD in open() */

	/*
	 * The high level of the driver never reads status registers directly
	 * because there would be too many side effects to handle conveniently.
	 * Instead, it reads copies of the registers stored here by the
	 * interrupt handler.
	 */
	u_char	last_modem_status;	/* last MSR read by intr handler */
	u_char	prev_modem_status;	/* last MSR handled by high level */

	u_char	hotchar;	/* ldisc-specific char to be handled ASAP */
	u_char	*ibuf;		/* start of input buffer */
	u_char	*ibufend;	/* end of input buffer */
	u_char	*ihighwater;	/* threshold in input buffer */
	u_char	*iptr;		/* next free spot in input buffer */

	struct lbq	obufq;	/* head of queue of output buffers */
	struct lbq	obufs[2];	/* output buffers */

#ifdef PC98
	Port_t	cmd_port;
	Port_t	sts_port;
	Port_t	in_modem_port;
	Port_t	intr_ctrl_port;
	int	intr_enable;
	int	pc98_prev_modem_status;
	int	pc98_modem_delta;
	int	modem_car_chg_timer;
	int	pc98_prev_siocmd;
	int	pc98_prev_siomod;
	int	modem_checking;
	int	pc98_if_type;
#endif /* PC98 */
	Port_t	data_port;	/* i/o ports */
#ifdef COM_ESP
	Port_t	esp_port;
#endif
	Port_t	int_id_port;
	Port_t	iobase;
	Port_t	modem_ctl_port;
	Port_t	line_status_port;
	Port_t	modem_status_port;
	Port_t	intr_ctl_port;	/* Ports of IIR register */

	struct tty	*tp;	/* cross reference */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

	bool_t	do_timestamp;
	bool_t	do_dcd_timestamp;
	struct timeval	timestamp;
	struct timeval	dcd_timestamp;

	u_long	bytes_in;	/* statistics */
	u_long	bytes_out;
	u_int	delta_error_counts[CE_NTYPES];
	u_long	error_counts[CE_NTYPES];

	/*
	 * Ping-pong input buffers.  The extra factor of 2 in the sizes is
	 * to allow for an error byte for each input byte.
	 */
#define	CE_INPUT_OFFSET		RS_IBUFSIZE
	u_char	ibuf1[2 * RS_IBUFSIZE];
	u_char	ibuf2[2 * RS_IBUFSIZE];

	/*
	 * Data area for output buffers.  Someday we should build the output
	 * buffer queue without copying data.
	 */
	u_char	obuf1[256];
	u_char	obuf2[256];
#ifdef DEVFS
	void	*devfs_token_ttyd;
	void	*devfs_token_ttyl;
	void	*devfs_token_ttyi;
	void	*devfs_token_cuaa;
	void	*devfs_token_cual;
	void	*devfs_token_cuai;
#endif
};

/*
 * XXX public functions in drivers should be declared in headers produced
 * by `config', not here.
 */

/* Interrupt handling entry point. */
void	siopoll		__P((void));

/* Device switch entry points. */
#define	sioreset	noreset
#define	siommap		nommap
#define	siostrategy	nostrategy

#ifdef COM_ESP
static	int	espattach	__P((struct isa_device *isdp, struct com_s *com,
				     Port_t esp_port));
#endif
static	int	sioattach	__P((struct isa_device *dev));
static	timeout_t siobusycheck;
static	timeout_t siodtrwakeup;
static	void	comhardclose	__P((struct com_s *com));
static	void	siointr1	__P((struct com_s *com));
static	int	commctl		__P((struct com_s *com, int bits, int how));
static	int	comparam	__P((struct tty *tp, struct termios *t));
static	int	sioprobe	__P((struct isa_device *dev));
static	void	siosettimeout	__P((void));
static	void	comstart	__P((struct tty *tp));
static	timeout_t comwakeup;
static	void	disc_optim	__P((struct tty	*tp, struct termios *t,
				     struct com_s *com));

#ifdef DSI_SOFT_MODEM
static  int 	LoadSoftModem   __P((int unit,int base_io, u_long size, u_char *ptr));
#endif /* DSI_SOFT_MODEM */

static char driver_name[] = "sio";

/* table and macro for fast conversion from a unit number to its com struct */
static	struct com_s	*p_com_addr[NSIOTOT];
#define	com_addr(unit)	(p_com_addr[unit])

struct isa_driver	siodriver = {
	sioprobe, sioattach, driver_name
};

static	d_open_t	sioopen;
static	d_close_t	sioclose;
static	d_read_t	sioread;
static	d_write_t	siowrite;
static	d_ioctl_t	sioioctl;
static	d_stop_t	siostop;
static	d_devtotty_t	siodevtotty;

#define CDEV_MAJOR 28
static struct cdevsw sio_cdevsw = {
	sioopen,	sioclose,	sioread,	siowrite,
	sioioctl,	siostop,	noreset,	siodevtotty,
	ttpoll,		nommap,		NULL,		driver_name,
	NULL,		-1,
};

static	int	comconsole = -1;
static	volatile speed_t	comdefaultrate = CONSPEED;
static	u_int	com_events;	/* input chars + weighted output completions */
static	Port_t	siocniobase;
static	int	sio_timeout;
static	int	sio_timeouts_until_log;
static	struct	callout_handle sio_timeout_handle
    = CALLOUT_HANDLE_INITIALIZER(&sio_timeout_handle);
#if 0 /* XXX */
static struct tty	*sio_tty[NSIOTOT];
#else
static struct tty	sio_tty[NSIOTOT];
#endif
static	const int	nsio_tty = NSIOTOT;

#ifdef PC98
struct	siodev	{
	short	if_type;
	short	irq;
	Port_t	cmd, sts, ctrl, mod;
	};
static	int	sysclock;
static	short	port_table[5][3] = {
			{0x30, 0xb1, 0xb9},
			{0x32, 0xb3, 0xbb},
			{0x32, 0xb3, 0xbb},
			{0x33, 0xb0, 0xb2},
			{0x35, 0xb0, 0xb2}
		};
#define	PC98SIO_data_port(ch)		port_table[0][ch]
#define	PC98SIO_cmd_port(ch)		port_table[1][ch]
#define	PC98SIO_sts_port(ch)		port_table[2][ch]
#define	PC98SIO_in_modem_port(ch)	port_table[3][ch]
#define	PC98SIO_intr_ctrl_port(ch)	port_table[4][ch]
#ifdef COM_IF_PIO9032B
#define   IO_COM_PIO9032B_2	0x0b8
#define   IO_COM_PIO9032B_3	0x0ba
#endif /* COM_IF_PIO9032B */
#ifdef COM_IF_B98_01
#define	  IO_COM_B98_01_2	0x0d1
#define	  IO_COM_B98_01_3	0x0d5
#endif /* COM_IF_B98_01 */
#define	COM_INT_DISABLE		{int previpri; previpri=spltty();
#define	COM_INT_ENABLE		splx(previpri);}
#define IEN_TxFLAG		IEN_Tx

#define COM_CARRIER_DETECT_EMULATE	0
#define	PC98_CHECK_MODEM_INTERVAL	(hz/10)
#define DCD_OFF_TOLERANCE		2
#define DCD_ON_RECOGNITION		2
#define	IS_8251(type)		(type != MC16550)
#define	IS_PC98IN(adr)		(adr == 0x30)

static	void	commint		__P((dev_t dev));
static	void	com_tiocm_set	__P((struct com_s *com, int msr));
static	void	com_tiocm_bis	__P((struct com_s *com, int msr));
static	void	com_tiocm_bic	__P((struct com_s *com, int msr));
static	int	com_tiocm_get	__P((struct com_s *com));
static	int	com_tiocm_get_delta	__P((struct com_s *com));
static	void	pc98_msrint_start	__P((dev_t dev));
static	void	com_cflag_and_speed_set	__P((struct com_s *com, int cflag, int speed));
static	int	pc98_ttspeedtab		__P((struct com_s *com, int speed));
static	int	pc98_get_modem_status	__P((struct com_s *com));
static	timeout_t	pc98_check_msr;
static	void	pc98_set_baud_rate	__P((struct com_s *com, int count));
static	void	pc98_i8251_reset	__P((struct com_s *com, int mode, int command));
static	void	pc98_disable_i8251_interrupt	__P((struct com_s *com, int mod));
static	void	pc98_enable_i8251_interrupt	__P((struct com_s *com, int mod));
static	int	pc98_check_i8251_interrupt	__P((struct com_s *com));
static	int	pc98_i8251_get_cmd	__P((struct com_s *com));
static	int	pc98_i8251_get_mod	__P((struct com_s *com));
static	void	pc98_i8251_set_cmd	__P((struct com_s *com, int x));
static	void	pc98_i8251_or_cmd	__P((struct com_s *com, int x));
static	void	pc98_i8251_clear_cmd	__P((struct com_s *com, int x));
static	void	pc98_i8251_clear_or_cmd	__P((struct com_s *com, int clr, int x));
static	int	pc98_check_if_type	__P((int iobase, struct siodev *iod));
static	void	pc98_check_sysclock	__P((void));
static	int	pc98_set_ioport		__P((struct com_s *com, int io_base));

#define com_int_Tx_disable(com) \
		pc98_disable_i8251_interrupt(com,IEN_Tx|IEN_TxEMP)
#define com_int_Tx_enable(com) \
		pc98_enable_i8251_interrupt(com,IEN_TxFLAG)
#define com_int_Rx_disable(com) \
		pc98_disable_i8251_interrupt(com,IEN_Rx)
#define com_int_Rx_enable(com) \
		pc98_enable_i8251_interrupt(com,IEN_Rx)
#define com_int_TxRx_disable(com) \
		pc98_disable_i8251_interrupt(com,IEN_Tx|IEN_TxEMP|IEN_Rx)
#define com_int_TxRx_enable(com) \
		pc98_enable_i8251_interrupt(com,IEN_TxFLAG|IEN_Rx)
#define com_send_break_on(com) \
		pc98_i8251_or_cmd(com,CMD8251_SBRK)
#define com_send_break_off(com) \
		pc98_i8251_clear_cmd(com,CMD8251_SBRK)

struct speedtab pc98speedtab[] = {	/* internal RS232C interface */
	0,	0,
	50,	50,
	75,	75,
	150,	150,
	200,	200,
	300,	300,
	600,	600,
	1200,	1200,
	2400,	2400,
	4800,	4800,
	9600,	9600,
	19200,	19200,
	38400,	38400,
	76800,	76800,
	20800,	20800,
	41600,	41600,
	15600,	15600,
	31200,	31200,
	62400,	62400,
	-1,	-1
};
#ifdef COM_IF_PIO9032B
struct speedtab comspeedtab_pio9032b[] = {
	300,	6,
	600,	5,
	1200,	4,
	2400,	3,
	4800,	2,
	9600,	1,
	19200,	0,
	38400,	7,
	-1,	-1
};
#endif

#ifdef COM_IF_B98_01
struct speedtab comspeedtab_b98_01[] = {
	0,	0,
	75,	15,
	150,	14,
	300,	13,
	600,	12,
	1200,	11,
	2400,	10,
	4800,	9,
	9600,	8,
	19200,	7,
	38400,	6,
	76800,	5,
	153600,	4,
	-1,	-1
};
#endif
#endif /* PC98 */

static	struct speedtab comspeedtab[] = {
	{ 0,		0 },
	{ 50,		COMBRD(50) },
	{ 75,		COMBRD(75) },
	{ 110,		COMBRD(110) },
	{ 134,		COMBRD(134) },
	{ 150,		COMBRD(150) },
	{ 200,		COMBRD(200) },
	{ 300,		COMBRD(300) },
	{ 600,		COMBRD(600) },
	{ 1200,		COMBRD(1200) },
	{ 1800,		COMBRD(1800) },
	{ 2400,		COMBRD(2400) },
	{ 4800,		COMBRD(4800) },
	{ 9600,		COMBRD(9600) },
	{ 19200,	COMBRD(19200) },
	{ 38400,	COMBRD(38400) },
	{ 57600,	COMBRD(57600) },
	{ 115200,	COMBRD(115200) },
	{ -1,		-1 }
};

#ifdef COM_ESP
/* XXX configure this properly. */
static	Port_t	likely_com_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, };
static	Port_t	likely_esp_ports[] = { 0x140, 0x180, 0x280, 0 };
#endif

/*
 * handle sysctl read/write requests for console speed
 * 
 * In addition to setting comdefaultrate for I/O through /dev/console,
 * also set the initial and lock values for the /dev/ttyXX device
 * if there is one associated with the console.  Finally, if the /dev/tty
 * device has already been open, change the speed on the open running port
 * itself.
 */

static int
sysctl_machdep_comdefaultrate SYSCTL_HANDLER_ARGS
{
	int error, s;
	speed_t newspeed;
	struct com_s *com;
	struct tty *tp;

	newspeed = comdefaultrate;

	error = sysctl_handle_opaque(oidp, &newspeed, sizeof newspeed, req);
	if (error || !req->newptr)
		return (error);

	comdefaultrate = newspeed;

	if (comconsole < 0)		/* serial console not selected? */
		return (0);

	com = com_addr(comconsole);
	if (!com)
		return (ENXIO);

	/*
	 * set the initial and lock rates for /dev/ttydXX and /dev/cuaXX
	 * (note, the lock rates really are boolean -- if non-zero, disallow
	 *  speed changes)
	 */
	com->it_in.c_ispeed  = com->it_in.c_ospeed =
	com->lt_in.c_ispeed  = com->lt_in.c_ospeed =
	com->it_out.c_ispeed = com->it_out.c_ospeed =
	com->lt_out.c_ispeed = com->lt_out.c_ospeed = comdefaultrate;

	/*
	 * if we're open, change the running rate too
	 */
	tp = com->tp;
	if (tp && (tp->t_state & TS_ISOPEN)) {
		tp->t_termios.c_ispeed =
		tp->t_termios.c_ospeed = comdefaultrate;
		s = spltty();
		error = comparam(tp, &tp->t_termios);
		splx(s);
	}
	return error;
}

SYSCTL_PROC(_machdep, OID_AUTO, conspeed, CTLTYPE_INT | CTLFLAG_RW,
	    0, 0, sysctl_machdep_comdefaultrate, "I", "");

#if NCARD > 0
/*
 *	PC-Card (PCMCIA) specific code.
 */
static int	sioinit		__P((struct pccard_devinfo *));
static void	siounload	__P((struct pccard_devinfo *));
static int	card_intr	__P((struct pccard_devinfo *));

static struct pccard_device sio_info = {
	driver_name,
	sioinit,
	siounload,
	card_intr,
	0,			/* Attributes - presently unused */
	&tty_imask		/* Interrupt mask for device */
				/* XXX - Should this also include net_imask? */
};

DATA_SET(pccarddrv_set, sio_info);

/*
 *	Initialize the device - called from Slot manager.
 */
int
sioinit(struct pccard_devinfo *devi)
{

	/* validate unit number. */
	if (devi->isahd.id_unit >= (NSIOTOT))
		return(ENODEV);
	/* Make sure it isn't already probed. */
	if (com_addr(devi->isahd.id_unit))
		return(EBUSY);

	/* It's already probed as serial by Upper */
	devi->isahd.id_flags |= COM_C_NOPROBE; 

	/*
	 * Probe the device. If a value is returned, the
	 * device was found at the location.
	 */
	if (sioprobe(&devi->isahd) == 0)
		return(ENXIO);
	if (sioattach(&devi->isahd) == 0)
		return(ENXIO);

	return(0);
}

/*
 *	siounload - unload the driver and clear the table.
 *	XXX TODO:
 *	This is usually called when the card is ejected, but
 *	can be caused by a modunload of a controller driver.
 *	The idea is to reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
static void
siounload(struct pccard_devinfo *devi)
{
	struct com_s	*com;

	if (!devi) {
		printf("NULL devi in siounload\n");
		return;
	}
	com = com_addr(devi->isahd.id_unit);
	if (!com) {
		printf("NULL com in siounload\n");
		return;
	}
	if (!com->iobase) {
		printf("sio%d already unloaded!\n",devi->isahd.id_unit);
		return;
	}
	if (com->tp && (com->tp->t_state & TS_ISOPEN)) {
		com->gone = 1;
		printf("sio%d: unload\n", devi->isahd.id_unit);
		com->tp->t_gen++;
		ttyclose(com->tp);
		ttwakeup(com->tp);
		ttwwakeup(com->tp);
	} else {
		com_addr(com->unit) = NULL;
		bzero(com, sizeof *com);
		free(com,M_TTYS);
		printf("sio%d: unload,gone\n", devi->isahd.id_unit);
	}
}

/*
 *	card_intr - Shared interrupt called from
 *	front end of PC-Card handler.
 */
static int
card_intr(struct pccard_devinfo *devi)
{
	struct com_s	*com;

	COM_LOCK();
	com = com_addr(devi->isahd.id_unit);
	if (com && !com->gone)
		siointr1(com_addr(devi->isahd.id_unit));
	COM_UNLOCK();
	return(1);
}
#endif /* NCARD > 0 */

static int
sioprobe(dev)
	struct isa_device	*dev;
{
	static bool_t	already_init;
	bool_t		failures[10];
	int		fn;
	struct isa_device	*idev;
	Port_t		iobase;
	intrmask_t	irqmap[4];
	intrmask_t	irqs;
	u_char		mcr_image;
	int		result;
	struct isa_device	*xdev;
#ifdef PC98
	int		irqout=0;
	int		ret = 0;
	int		tmp;
	struct		siodev	iod;
#endif

	if (!already_init) {
		/*
		 * Turn off MCR_IENABLE for all likely serial ports.  An unused
		 * port with its MCR_IENABLE gate open will inhibit interrupts
		 * from any used port that shares the interrupt vector.
		 * XXX the gate enable is elsewhere for some multiports.
		 */
		for (xdev = isa_devtab_tty; xdev->id_driver != NULL; xdev++)
			if (xdev->id_driver == &siodriver && xdev->id_enabled)
#ifdef PC98
				if (IS_PC98IN(xdev->id_iobase))
					outb(xdev->id_iobase + 2, 0xf2);
				else
#endif
				outb(xdev->id_iobase + com_mcr, 0);
		already_init = TRUE;
	}

	if (COM_LLCONSOLE(dev)) {
		printf("sio%d: reserved for low-level i/o\n", dev->id_unit);
		return (0);
	}

#ifdef PC98
	DELAY(10);
	/*
	 * If the port is i8251 UART (internal, B98_01)
	 */
	if(pc98_check_if_type(dev->id_iobase, &iod) == -1)
		return 0;
	if(IS_8251(iod.if_type)){
		if ( iod.irq > 0 )
			dev->id_irq = (1 << iod.irq);
		outb(iod.cmd, 0);
		DELAY(10);
		outb(iod.cmd, 0);
		DELAY(10);
		outb(iod.cmd, 0);
		DELAY(10);
		outb(iod.cmd, CMD8251_RESET);
		DELAY(1000);		/* for a while...*/
		outb(iod.cmd, 0xf2);	/* MODE (dummy) */
		DELAY(10);
		outb(iod.cmd, 0x01);	/* CMD (dummy) */
		DELAY(1000);		/* for a while...*/
		if (( inb(iod.sts) & STS8251_TxEMP ) == 0 ) {
			ret = 0;
		}
		switch (iod.if_type) {
		case COM_IF_INTERNAL:
			COM_INT_DISABLE
			tmp = ( inb( iod.ctrl ) & ~(IEN_Rx|IEN_TxEMP|IEN_Tx));
			outb( iod.ctrl, tmp|IEN_TxEMP );
			DELAY(10);
			ret = isa_irq_pending() ? 4 : 0;
			outb( iod.ctrl, tmp );
			COM_INT_ENABLE
			break;
#ifdef COM_IF_B98_01
		case COM_IF_B98_01:
			/* B98_01 doesn't activate TxEMP interrupt line
			   when being reset, so we can't check irq pending.*/
			ret = 4;
			break;
#endif
		}
		if (epson_machine_id==0x20) {	/* XXX */
			ret = 4;
		}
		return ret;
	}
#endif /* PC98 */
	/*
	 * If the device is on a multiport card and has an AST/4
	 * compatible interrupt control register, initialize this
	 * register and prepare to leave MCR_IENABLE clear in the mcr.
	 * Otherwise, prepare to set MCR_IENABLE in the mcr.
	 * Point idev to the device struct giving the correct id_irq.
	 * This is the struct for the master device if there is one.
	 */
	idev = dev;
	mcr_image = MCR_IENABLE;
#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(dev)) {
		idev = find_isadev(isa_devtab_tty, &siodriver,
				   COM_MPMASTER(dev));
		if (idev == NULL) {
			printf("sio%d: master device %d not configured\n",
			       dev->id_unit, COM_MPMASTER(dev));
			dev->id_irq = 0;
			idev = dev;
		}
#ifndef PC98
		if (!COM_NOTAST4(dev)) {
			outb(idev->id_iobase + com_scr,
			     idev->id_irq ? 0x80 : 0);
			mcr_image = 0;
		}
#endif /* !PC98 */
	}
#endif /* COM_MULTIPORT */
	if (idev->id_irq == 0)
		mcr_image = 0;

#ifdef PC98
	switch(idev->id_irq){
		case IRQ3: irqout = 4; break;
		case IRQ5: irqout = 5; break;
		case IRQ6: irqout = 6; break;
		case IRQ12: irqout = 7; break;
		default:
			printf("sio%d: irq configuration error\n",dev->id_unit);
			return (0);
	}
	outb(dev->id_iobase+0x1000, irqout);
#endif
	bzero(failures, sizeof failures);
	iobase = dev->id_iobase;

	/*
	 * We don't want to get actual interrupts, just masked ones.
	 * Interrupts from this line should already be masked in the ICU,
	 * but mask them in the processor as well in case there are some
	 * (misconfigured) shared interrupts.
	 */
	disable_intr();
/* EXTRA DELAY? */

	/*
	 * Initialize the speed and the word size and wait long enough to
	 * drain the maximum of 16 bytes of junk in device output queues.
	 * The speed is undefined after a master reset and must be set
	 * before relying on anything related to output.  There may be
	 * junk after a (very fast) soft reboot and (apparently) after
	 * master reset.
	 * XXX what about the UART bug avoided by waiting in comparam()?
	 * We don't want to to wait long enough to drain at 2 bps.
	 */
	if (iobase == siocniobase)
		DELAY((16 + 1) * 1000000 / (comdefaultrate / 10));
	else {
		outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
		outb(iobase + com_dlbl, COMBRD(SIO_TEST_SPEED) & 0xff);
		outb(iobase + com_dlbh, (u_int) COMBRD(SIO_TEST_SPEED) >> 8);
		outb(iobase + com_cfcr, CFCR_8BITS);
		DELAY((16 + 1) * 1000000 / (SIO_TEST_SPEED / 10));
	}

	/*
	 * Enable the interrupt gate and disable device interupts.  This
	 * should leave the device driving the interrupt line low and
	 * guarantee an edge trigger if an interrupt can be generated.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image);
	outb(iobase + com_ier, 0);
	DELAY(1000);		/* XXX */
	irqmap[0] = isa_irq_pending();

	/*
	 * Attempt to set loopback mode so that we can send a null byte
	 * without annoying any external device.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image | MCR_LOOPBACK);

	/*
	 * Attempt to generate an output interrupt.  On 8250's, setting
	 * IER_ETXRDY generates an interrupt independent of the current
	 * setting and independent of whether the THR is empty.  On 16450's,
	 * setting IER_ETXRDY generates an interrupt independent of the
	 * current setting.  On 16550A's, setting IER_ETXRDY only
	 * generates an interrupt when IER_ETXRDY is not already set.
	 */
	outb(iobase + com_ier, IER_ETXRDY);

	/*
	 * On some 16x50 incompatibles, setting IER_ETXRDY doesn't generate
	 * an interrupt.  They'd better generate one for actually doing
	 * output.  Loopback may be broken on the same incompatibles but
	 * it's unlikely to do more than allow the null byte out.
	 */
	outb(iobase + com_data, 0);
	DELAY((1 + 2) * 1000000 / (SIO_TEST_SPEED / 10));

	/*
	 * Turn off loopback mode so that the interrupt gate works again
	 * (MCR_IENABLE was hidden).  This should leave the device driving
	 * an interrupt line high.  It doesn't matter if the interrupt
	 * line oscillates while we are not looking at it, since interrupts
	 * are disabled.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image);

    /*
	 * It's a definitly Serial PCMCIA(16550A), but still be required
	 * for IIR_TXRDY implementation ( Palido 321s, DC-1S... )
	 */
	if ( COM_NOPROBE(dev) ) {
		/* Reading IIR register twice */
		for ( fn = 0; fn < 2; fn ++ ) {
			DELAY(10000);
			failures[6] = inb(iobase + com_iir);
		}
		/* Check IIR_TXRDY clear ? */
		result = IO_COMSIZE;
		if ( failures[6] & IIR_TXRDY ) {
			/* Nop, Double check with clearing IER */
			outb(iobase + com_ier, 0);
			if ( inb(iobase + com_iir) & IIR_NOPEND ) {
				/* Ok. we're familia this gang */
				dev->id_flags |= COM_C_IIR_TXRDYBUG; /* Set IIR_TXRDYBUG */
			} else {
				/* Unknow, Just omit this chip.. XXX*/
				result = 0;
			}
		} else {
			/* OK. this is well-known guys */
			dev->id_flags &= ~COM_C_IIR_TXRDYBUG; /*Clear IIR_TXRDYBUG*/
		}
		outb(iobase + com_cfcr, CFCR_8BITS);
		enable_intr();
		return (iobase == siocniobase ? IO_COMSIZE : result);
	}

	/*
	 * Check that
	 *	o the CFCR, IER and MCR in UART hold the values written to them
	 *	  (the values happen to be all distinct - this is good for
	 *	  avoiding false positive tests from bus echoes).
	 *	o an output interrupt is generated and its vector is correct.
	 *	o the interrupt goes away when the IIR in the UART is read.
	 */
/* EXTRA DELAY? */
	failures[0] = inb(iobase + com_cfcr) - CFCR_8BITS;
	failures[1] = inb(iobase + com_ier) - IER_ETXRDY;
	failures[2] = inb(iobase + com_mcr) - mcr_image;
	DELAY(10000);		/* Some internal modems need this time */
	irqmap[1] = isa_irq_pending();
	failures[4] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_TXRDY;
	DELAY(1000);		/* XXX */
	irqmap[2] = isa_irq_pending();
	failures[6] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	/*
	 * Turn off all device interrupts and check that they go off properly.
	 * Leave MCR_IENABLE alone.  For ports without a master port, it gates
	 * the OUT2 output of the UART to
	 * the ICU input.  Closing the gate would give a floating ICU input
	 * (unless there is another device driving at) and spurious interrupts.
	 * (On the system that this was first tested on, the input floats high
	 * and gives a (masked) interrupt as soon as the gate is closed.)
	 */
	outb(iobase + com_ier, 0);
	outb(iobase + com_cfcr, CFCR_8BITS);	/* dummy to avoid bus echo */
	failures[7] = inb(iobase + com_ier);
	DELAY(1000);		/* XXX */
	irqmap[3] = isa_irq_pending();
	failures[9] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	enable_intr();

	irqs = irqmap[1] & ~irqmap[0];
	if (idev->id_irq != 0 && (idev->id_irq & irqs) == 0)
		printf(
		"sio%d: configured irq %d not in bitmap of probed irqs %#x\n",
		    dev->id_unit, ffs(idev->id_irq) - 1, irqs);
	if (bootverbose)
		printf("sio%d: irq maps: %#x %#x %#x %#x\n",
		    dev->id_unit, irqmap[0], irqmap[1], irqmap[2], irqmap[3]);

	result = IO_COMSIZE;
	for (fn = 0; fn < sizeof failures; ++fn)
		if (failures[fn]) {
			outb(iobase + com_mcr, 0);
			result = 0;
			if (bootverbose) {
				printf("sio%d: probe failed test(s):",
				    dev->id_unit);
				for (fn = 0; fn < sizeof failures; ++fn)
					if (failures[fn])
						printf(" %d", fn);
				printf("\n");
			}
			break;
		}
	return (iobase == siocniobase ? IO_COMSIZE : result);
}

#ifdef COM_ESP
static int
espattach(isdp, com, esp_port)
	struct isa_device	*isdp;
	struct com_s		*com;
	Port_t			esp_port;
{
	u_char	dips;
	u_char	val;

	/*
	 * Check the ESP-specific I/O port to see if we're an ESP
	 * card.  If not, return failure immediately.
	 */
	if ((inb(esp_port) & 0xf3) == 0) {
		printf(" port 0x%x is not an ESP board?\n", esp_port);
		return (0);
	}

	/*
	 * We've got something that claims to be a Hayes ESP card.
	 * Let's hope so.
	 */

	/* Get the dip-switch configuration */
	outb(esp_port + ESP_CMD1, ESP_GETDIPS);
	dips = inb(esp_port + ESP_STATUS1);

	/*
	 * Bits 0,1 of dips say which COM port we are.
	 */
	if (com->iobase == likely_com_ports[dips & 0x03])
		printf(" : ESP");
	else {
		printf(" esp_port has com %d\n", dips & 0x03);
		return (0);
	}

	/*
	 * Check for ESP version 2.0 or later:  bits 4,5,6 = 010.
	 */
	outb(esp_port + ESP_CMD1, ESP_GETTEST);
	val = inb(esp_port + ESP_STATUS1);	/* clear reg 1 */
	val = inb(esp_port + ESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		return (0);
	}

	/*
	 * Check for ability to emulate 16550:  bit 7 == 1
	 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		return (0);
	}

	/*
	 * Okay, we seem to be a Hayes ESP card.  Whee.
	 */
	com->esp = TRUE;
	com->esp_port = esp_port;
	return (1);
}
#endif /* COM_ESP */

static int
sioattach(isdp)
	struct isa_device	*isdp;
{
	struct com_s	*com;
	dev_t		dev;
#ifdef COM_ESP
	Port_t		*espp;
#endif
	Port_t		iobase;
	int		s;
	int		unit;

	isdp->id_ri_flags |= RI_FAST;
	iobase = isdp->id_iobase;
	unit = isdp->id_unit;
	com = malloc(sizeof *com, M_TTYS, M_NOWAIT);
	if (com == NULL)
		return (0);

	/*
	 * sioprobe() has initialized the device registers as follows:
	 *	o cfcr = CFCR_8BITS.
	 *	  It is most important that CFCR_DLAB is off, so that the
	 *	  data port is not hidden when we enable interrupts.
	 *	o ier = 0.
	 *	  Interrupts are only enabled when the line is open.
	 *	o mcr = MCR_IENABLE, or 0 if the port has AST/4 compatible
	 *	  interrupt control register or the config specifies no irq.
	 *	  Keeping MCR_DTR and MCR_RTS off might stop the external
	 *	  device from sending before we are ready.
	 */
	bzero(com, sizeof *com);
	com->unit = unit;
	com->cfcr_image = CFCR_8BITS;
	com->dtr_wait = 3 * hz;
	com->loses_outints = COM_LOSESOUTINTS(isdp) != 0;
	com->no_irq = isdp->id_irq == 0;
	com->tx_fifo_size = 1;
	com->iptr = com->ibuf = com->ibuf1;
	com->ibufend = com->ibuf1 + RS_IBUFSIZE;
	com->ihighwater = com->ibuf1 + RS_IHIGHWATER;
	com->obufs[0].l_head = com->obuf1;
	com->obufs[1].l_head = com->obuf2;

	com->iobase = iobase;
#ifdef PC98
	if(pc98_set_ioport(com, iobase) == -1)
		if((iobase & 0x0f0) == 0xd0) {
			com->pc98_if_type = MC16550;
			com->data_port = iobase + com_data;
			com->int_id_port = iobase + com_iir;
			com->modem_ctl_port = iobase + com_mcr;
			com->mcr_image = inb(com->modem_ctl_port);
			com->line_status_port = iobase + com_lsr;
			com->modem_status_port = iobase + com_msr;
			com->intr_ctl_port = iobase + com_ier;
		}
#else /* not PC98 */
	com->data_port = iobase + com_data;
	com->int_id_port = iobase + com_iir;
	com->modem_ctl_port = iobase + com_mcr;
	com->mcr_image = inb(com->modem_ctl_port);
	com->line_status_port = iobase + com_lsr;
	com->modem_status_port = iobase + com_msr;
	com->intr_ctl_port = iobase + com_ier;
#endif

	/*
	 * We don't use all the flags from <sys/ttydefaults.h> since they
	 * are only relevant for logins.  It's important to have echo off
	 * initially so that the line doesn't start blathering before the
	 * echo flag can be turned off.
	 */
	com->it_in.c_iflag = 0;
	com->it_in.c_oflag = 0;
	com->it_in.c_cflag = TTYDEF_CFLAG;
	com->it_in.c_lflag = 0;
	if (unit == comconsole) {
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			DELAY(100000);
#endif
		com->it_in.c_iflag = TTYDEF_IFLAG;
		com->it_in.c_oflag = TTYDEF_OFLAG;
		com->it_in.c_cflag = TTYDEF_CFLAG | CLOCAL;
		com->it_in.c_lflag = TTYDEF_LFLAG;
		com->lt_out.c_cflag = com->lt_in.c_cflag = CLOCAL;
		com->lt_out.c_ispeed = com->lt_out.c_ospeed =
		com->lt_in.c_ispeed = com->lt_in.c_ospeed =
		com->it_in.c_ispeed = com->it_in.c_ospeed = comdefaultrate;
	} else
		com->it_in.c_ispeed = com->it_in.c_ospeed = TTYDEF_SPEED;
	termioschars(&com->it_in);
	com->it_out = com->it_in;

	/* attempt to determine UART type */
	printf("sio%d: type", unit);

#ifdef DSI_SOFT_MODEM
	if((inb(iobase+7) ^ inb(iobase+7)) & 0x80) {
	    printf(" Digicom Systems, Inc. SoftModem");
	goto determined_type;
	}
#endif /* DSI_SOFT_MODEM */

#ifndef PC98
#ifdef COM_MULTIPORT
	if (!COM_ISMULTIPORT(isdp) && !COM_IIR_TXRDYBUG(isdp))
#else
	if (!COM_IIR_TXRDYBUG(isdp))
#endif
	{
		u_char	scr;
		u_char	scr1;
		u_char	scr2;

		scr = inb(iobase + com_scr);
		outb(iobase + com_scr, 0xa5);
		scr1 = inb(iobase + com_scr);
		outb(iobase + com_scr, 0x5a);
		scr2 = inb(iobase + com_scr);
		outb(iobase + com_scr, scr);
		if (scr1 != 0xa5 || scr2 != 0x5a) {
			printf(" 8250");
			goto determined_type;
		}
	}
#endif /* !PC98 */
#ifdef PC98
	if(IS_8251(com->pc98_if_type)){
		com_int_TxRx_disable( com );
		com_cflag_and_speed_set( com, com->it_in.c_cflag,
						comdefaultrate );
		com_tiocm_bic( com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE );
		com_send_break_off( com );
		switch(com->pc98_if_type){
		case COM_IF_INTERNAL:
			printf(" 8251 (internal)");
			break;
#ifdef COM_IF_PC9861K
		case COM_IF_PC9861K:
			printf(" 8251 (PC9861K)");
			break;
#endif
#ifdef COM_IF_PIO9032B
		case COM_IF_PIO9032B:
			printf(" 8251 (PIO9032B)");
			break;
#endif
#ifdef COM_IF_B98_01
		case COM_IF_B98_01:
			printf(" 8251 (B98_01)");
			break;
#endif
		}
	} else {
#endif /* PC98 */
	outb(iobase + com_fifo, FIFO_ENABLE | FIFO_RX_HIGH);
	DELAY(100);
	com->st16650a = 0;
	switch (inb(com->int_id_port) & IIR_FIFO_MASK) {
	case FIFO_RX_LOW:
		printf(" 16450");
		break;
	case FIFO_RX_MEDL:
		printf(" 16450?");
		break;
	case FIFO_RX_MEDH:
		printf(" 16550?");
		break;
	case FIFO_RX_HIGH:
		if (COM_NOFIFO(isdp)) {
			printf(" 16550A fifo disabled");
		} else {
			com->hasfifo = TRUE;
			if (COM_ST16650A(isdp)) {
				com->st16650a = 1;
				com->tx_fifo_size = 32;
				printf(" ST16650A");
			} else {
				com->tx_fifo_size = COM_FIFOSIZE(isdp);
				printf(" 16550A");
			}
		}
#ifdef COM_ESP
		for (espp = likely_esp_ports; *espp != 0; espp++)
			if (espattach(isdp, com, *espp)) {
				com->tx_fifo_size = 1024;
				break;
			}
#endif
		if (!com->st16650a) {
			if (!com->tx_fifo_size)
				com->tx_fifo_size = 16;
			else
				printf(" lookalike with %d bytes FIFO",
				    com->tx_fifo_size);
		}

		break;
	}
	
#ifdef COM_ESP
	if (com->esp) {
		/*
		 * Set 16550 compatibility mode.
		 * We don't use the ESP_MODE_SCALE bit to increase the
		 * fifo trigger levels because we can't handle large
		 * bursts of input.
		 * XXX flow control should be set in comparam(), not here.
		 */
		outb(com->esp_port + ESP_CMD1, ESP_SETMODE);
		outb(com->esp_port + ESP_CMD2, ESP_MODE_RTS | ESP_MODE_FIFO);

		/* Set RTS/CTS flow control. */
		outb(com->esp_port + ESP_CMD1, ESP_SETFLOWTYPE);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_RTS);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_CTS);

		/* Set flow-control levels. */
		outb(com->esp_port + ESP_CMD1, ESP_SETRXFLOW);
		outb(com->esp_port + ESP_CMD2, HIBYTE(768));
		outb(com->esp_port + ESP_CMD2, LOBYTE(768));
		outb(com->esp_port + ESP_CMD2, HIBYTE(512));
		outb(com->esp_port + ESP_CMD2, LOBYTE(512));
	}
#endif /* COM_ESP */
	outb(iobase + com_fifo, 0);
determined_type: ;

#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(isdp)) {
		com->multiport = TRUE;
		printf(" (multiport");
		if (unit == COM_MPMASTER(isdp))
			printf(" master");
		printf(")");
		com->no_irq = find_isadev(isa_devtab_tty, &siodriver,
					  COM_MPMASTER(isdp))->id_irq == 0;
	 }
#endif /* COM_MULTIPORT */
#ifdef PC98
	}
#endif
	if (unit == comconsole)
		printf(", console");
	if ( COM_IIR_TXRDYBUG(isdp) )
		printf(" with a bogus IIR_TXRDY register");
	printf("\n");

	s = spltty();
	com_addr(unit) = com;
	splx(s);

	dev = makedev(CDEV_MAJOR, 0);
	cdevsw_add(&dev, &sio_cdevsw, NULL);
#ifdef DEVFS
	com->devfs_token_ttyd = devfs_add_devswf(&sio_cdevsw,
		unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyd%r", unit);
	com->devfs_token_ttyi = devfs_add_devswf(&sio_cdevsw,
		unit | CONTROL_INIT_STATE, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyid%r", unit);
	com->devfs_token_ttyl = devfs_add_devswf(&sio_cdevsw,
		unit | CONTROL_LOCK_STATE, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyld%r", unit);
	com->devfs_token_cuaa = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuaa%r", unit);
	com->devfs_token_cuai = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK | CONTROL_INIT_STATE, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuaia%r", unit);
	com->devfs_token_cual = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK | CONTROL_LOCK_STATE, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuala%r", unit);
#endif
	com->id_flags = isdp->id_flags; /* Heritate id_flags for later */
	return (1);
}

static int
sioopen(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	Port_t		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIOTOT || (com = com_addr(unit)) == NULL)
		return (ENXIO);
	if (com->gone)
		return (ENXIO);
	if (mynor & CONTROL_MASK)
		return (0);
#if 0 /* XXX */
	tp = com->tp = sio_tty[unit] = ttymalloc(sio_tty[unit]);
#else
	tp = com->tp = &sio_tty[unit];
#endif
	s = spltty();
	/*
	 * We jump to this label after all non-interrupted sleeps to pick
	 * up any changes of the device state.
	 */
open_top:
	while (com->state & CS_DTR_OFF) {
		error = tsleep(&com->dtr_wait, TTIPRI | PCATCH, "siodtr", 0);
		if (com_addr(unit) == NULL)
			return (ENXIO);
		if (error != 0 || com->gone)
			goto out;
	}
	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (mynor & CALLOUT_MASK) {
			if (!com->active_out) {
				error = EBUSY;
				goto out;
			}
		} else {
			if (com->active_out) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto out;
				}
				error =	tsleep(&com->active_out,
					       TTIPRI | PCATCH, "siobi", 0);
				if (com_addr(unit) == NULL)
					return (ENXIO);
				if (error != 0 || com->gone)
					goto out;
				goto open_top;
			}
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		tp->t_oproc = comstart;
		tp->t_param = comparam;
		tp->t_dev = dev;
		tp->t_termios = mynor & CALLOUT_MASK
				? com->it_out : com->it_in;
#ifdef PC98
		if(!IS_8251(com->pc98_if_type))
#endif
		(void)commctl(com, TIOCM_DTR | TIOCM_RTS, DMSET);
		com->poll = com->no_irq;
		com->poll_output = com->loses_outints;
		++com->wopeners;
		error = comparam(tp, &tp->t_termios);
		--com->wopeners;
		if (error != 0)
			goto out;
#ifdef PC98
		if(IS_8251(com->pc98_if_type)){
			com_tiocm_bis(com, TIOCM_DTR|TIOCM_RTS);
			pc98_msrint_start(dev);
		}
#endif
		/*
		 * XXX we should goto open_top if comparam() slept.
		 */
		ttsetwater(tp);
		iobase = com->iobase;
		if (com->hasfifo) {
			/*
			 * (Re)enable and drain fifos.
			 *
			 * Certain SMC chips cause problems if the fifos
			 * are enabled while input is ready.  Turn off the
			 * fifo if necessary to clear the input.  We test
			 * the input ready bit after enabling the fifos
			 * since we've already enabled them in comparam()
			 * and to handle races between enabling and fresh
			 * input.
			 */
			while (TRUE) {
				outb(iobase + com_fifo,
				     FIFO_RCV_RST | FIFO_XMT_RST
				     | com->fifo_image);
				/*
				 * XXX the delays are for superstitious
				 * historical reasons.  It must be less than
				 * the character time at the maximum
				 * supported speed (87 usec at 115200 bps
				 * 8N1).  Otherwise we might loop endlessly
				 * if data is streaming in.  We used to use
				 * delays of 100.  That usually worked
				 * because DELAY(100) used to usually delay
				 * for about 85 usec instead of 100.
				 */
				DELAY(50);
				if (!(inb(com->line_status_port) & LSR_RXRDY))
					break;
				outb(iobase + com_fifo, 0);
				DELAY(50);
				(void) inb(com->data_port);
			}
		}

		disable_intr();
#ifdef PC98
		if(IS_8251(com->pc98_if_type)){
			com_tiocm_bis(com, TIOCM_LE);
			com->pc98_prev_modem_status =
				pc98_get_modem_status(com);
			com_int_Rx_enable(com);
		} else {
#endif
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
		com->prev_modem_status = com->last_modem_status
		    = inb(com->modem_status_port);
		if (COM_IIR_TXRDYBUG(com)) {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ERLS
						| IER_EMSC);
		} else {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ETXRDY
						| IER_ERLS | IER_EMSC);
		}
#ifdef PC98
		}
#endif
		enable_intr();
		/*
		 * Handle initial DCD.  Callout devices get a fake initial
		 * DCD (trapdoor DCD).  If we are callout, then any sleeping
		 * callin opens get woken up and resume sleeping on "siobi"
		 * instead of "siodcd".
		 */
		/*
		 * XXX `mynor & CALLOUT_MASK' should be
		 * `tp->t_cflag & (SOFT_CARRIER | TRAPDOOR_CARRIER) where
		 * TRAPDOOR_CARRIER is the default initial state for callout
		 * devices and SOFT_CARRIER is like CLOCAL except it hides
		 * the true carrier.
		 */
#ifdef PC98
		if ((IS_8251(com->pc98_if_type) &&
			(pc98_get_modem_status(com) & TIOCM_CAR)) ||
		    (!IS_8251(com->pc98_if_type) &&
			(com->prev_modem_status & MSR_DCD)) ||
		    mynor & CALLOUT_MASK)
#else
		if (com->prev_modem_status & MSR_DCD || mynor & CALLOUT_MASK)
#endif
			(*linesw[tp->t_line].l_modem)(tp, 1);
	}
	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !(mynor & CALLOUT_MASK)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		++com->wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "siodcd", 0);
		if (com_addr(unit) == NULL)
			return (ENXIO);
		--com->wopeners;
		if (error != 0 || com->gone)
			goto out;
		goto open_top;
	}
	error =	(*linesw[tp->t_line].l_open)(dev, tp);
	disc_optim(tp, &tp->t_termios, com);
	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		com->active_out = TRUE;
	siosettimeout();
out:
	splx(s);
	if (!(tp->t_state & TS_ISOPEN) && com->wopeners == 0)
		comhardclose(com);
	return (error);
}

static int
sioclose(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	struct com_s	*com;
	int		mynor;
	int		s;
	struct tty	*tp;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (0);
	com = com_addr(MINOR_TO_UNIT(mynor));
	tp = com->tp;
	s = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
#ifdef PC98
	com->modem_checking = 0;
#endif
	disc_optim(tp, &tp->t_termios, com);
	siostop(tp, FREAD | FWRITE);
	comhardclose(com);
	ttyclose(tp);
	siosettimeout();
	splx(s);
	if (com->gone) {
		printf("sio%d: gone\n", com->unit);
		s = spltty();
		com_addr(com->unit) = 0;
		bzero(tp,sizeof *tp);
		bzero(com,sizeof *com);
		free(com,M_TTYS);
		splx(s);
	}
	return (0);
}

static void
comhardclose(com)
	struct com_s	*com;
{
	Port_t		iobase;
	int		s;
	struct tty	*tp;
	int		unit;

	unit = com->unit;
	iobase = com->iobase;
	s = spltty();
	com->poll = FALSE;
	com->poll_output = FALSE;
	com->do_timestamp = FALSE;
	com->do_dcd_timestamp = FALSE;
#ifdef PC98
	if(IS_8251(com->pc98_if_type))
		com_send_break_off(com);
	else
#endif
	outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
	{
#ifdef PC98
		int tmp;
		if(IS_8251(com->pc98_if_type))
			com_int_TxRx_disable(com);
		else
#endif
		outb(iobase + com_ier, 0);
		tp = com->tp;
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			tmp = pc98_get_modem_status(com) & TIOCM_CAR;
		else
			tmp = com->prev_modem_status & MSR_DCD;
#endif
		if (tp->t_cflag & HUPCL
		    /*
		     * XXX we will miss any carrier drop between here and the
		     * next open.  Perhaps we should watch DCD even when the
		     * port is closed; it is not sufficient to check it at
		     * the next open because it might go up and down while
		     * we're not watching.
		     */
		    || !com->active_out
#ifdef PC98
		       && !(tmp)
#else
		       && !(com->prev_modem_status & MSR_DCD)
#endif
		       && !(com->it_in.c_cflag & CLOCAL)
		    || !(tp->t_state & TS_ISOPEN)) {
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				com_tiocm_bic(com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE);
			else
#endif
			(void)commctl(com, TIOCM_DTR, DMBIC);
			if (com->dtr_wait != 0 && !(com->state & CS_DTR_OFF)) {
				timeout(siodtrwakeup, com, com->dtr_wait);
				com->state |= CS_DTR_OFF;
			}
		}
#ifdef PC98
		else {
			if(IS_8251(com->pc98_if_type))
				com_tiocm_bic(com, TIOCM_LE );
		}
#endif
	}
	if (com->hasfifo) {
		/*
		 * Disable fifos so that they are off after controlled
		 * reboots.  Some BIOSes fail to detect 16550s when the
		 * fifos are enabled.
		 */
		outb(iobase + com_fifo, 0);
	}
	com->active_out = FALSE;
	wakeup(&com->active_out);
	wakeup(TSA_CARR_ON(tp));	/* restart any wopeners */
	splx(s);
}

static int
sioread(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	int		unit;
	struct tty	*tp;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);
	unit = MINOR_TO_UNIT(mynor);
	if (com_addr(unit)->gone)
		return (ENODEV);
	tp = com_addr(unit)->tp;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

static int
siowrite(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit = MINOR_TO_UNIT(mynor);
	if (com_addr(unit)->gone)
		return (ENODEV);
	tp = com_addr(unit)->tp;
	/*
	 * (XXX) We disallow virtual consoles if the physical console is
	 * a serial port.  This is in case there is a display attached that
	 * is not the console.  In that situation we don't need/want the X
	 * server taking over the console.
	 */
	if (constty != NULL && unit == comconsole)
		constty = NULL;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

static void
siobusycheck(chan)
	void	*chan;
{
	struct com_s	*com;
	int		s;

	com = (struct com_s *)chan;

	/*
	 * Clear TS_BUSY if low-level output is complete.
	 * spl locking is sufficient because siointr1() does not set CS_BUSY.
	 * If siointr1() clears CS_BUSY after we look at it, then we'll get
	 * called again.  Reading the line status port outside of siointr1()
	 * is safe because CS_BUSY is clear so there are no output interrupts
	 * to lose.
	 */
	s = spltty();
	if (com->state & CS_BUSY)
		com->extra_state &= ~CSE_BUSYCHECK;	/* False alarm. */
#ifdef	PC98
	else if (IS_8251(com->pc98_if_type) &&
		 (inb(com->sts_port) & (STS8251_TxRDY | STS8251_TxEMP))
		 == (STS8251_TxRDY | STS8251_TxEMP) ||
		 (inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
		 == (LSR_TSRE | LSR_TXRDY)) {
#else
	else if ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	    == (LSR_TSRE | LSR_TXRDY)) {
#endif
		com->tp->t_state &= ~TS_BUSY;
		ttwwakeup(com->tp);
		com->extra_state &= ~CSE_BUSYCHECK;
	} else
		timeout(siobusycheck, com, hz / 100);
	splx(s);
}

static void
siodtrwakeup(chan)
	void	*chan;
{
	struct com_s	*com;

	com = (struct com_s *)chan;
	com->state &= ~CS_DTR_OFF;
	wakeup(&com->dtr_wait);
}

void
siointr(unit)
	int	unit;
{
#ifndef COM_MULTIPORT
	COM_LOCK();
	siointr1(com_addr(unit));
	COM_UNLOCK();
#else /* COM_MULTIPORT */
	struct com_s    *com;
	bool_t		possibly_more_intrs;

	/*
	 * Loop until there is no activity on any port.  This is necessary
	 * to get an interrupt edge more than to avoid another interrupt.
	 * If the IRQ signal is just an OR of the IRQ signals from several
	 * devices, then the edge from one may be lost because another is
	 * on.
	 */
	COM_LOCK();
	do {
		possibly_more_intrs = FALSE;
		for (unit = 0; unit < NSIOTOT; ++unit) {
			com = com_addr(unit);
			/*
			 * XXX COM_LOCK();
			 * would it work here, or be counter-productive?
			 */
#ifdef PC98
			if (com != NULL 
			    && !com->gone
			    && IS_8251(com->pc98_if_type)){
				siointr1(com);
			} else
#endif /* PC98 */
			if (com != NULL 
			    && !com->gone
			    && (inb(com->int_id_port) & IIR_IMASK)
			       != IIR_NOPEND) {
				siointr1(com);
				possibly_more_intrs = TRUE;
			}
			/* XXX COM_UNLOCK(); */
		}
	} while (possibly_more_intrs);
	COM_UNLOCK();
#endif /* COM_MULTIPORT */
}

static void
siointr1(com)
	struct com_s	*com;
{
	u_char	line_status;
	u_char	modem_status;
	u_char	*ioptr;
	u_char	recv_data;
	u_char	int_ident;
	u_char	int_ctl;
	u_char	int_ctl_new;

#ifdef PC98
	u_char	tmp=0;
recv_data=0;
#endif /* PC98 */

	int_ctl = inb(com->intr_ctl_port);
	int_ctl_new = int_ctl;

	while (!com->gone) {
#ifdef PC98
status_read:;
		if (IS_8251(com->pc98_if_type)) {
			tmp = inb(com->sts_port);
more_intr:
			line_status = 0;
			if (tmp & STS8251_TxRDY) line_status |= LSR_TXRDY;
			if (tmp & STS8251_RxRDY) line_status |= LSR_RXRDY;
			if (tmp & STS8251_TxEMP) line_status |= LSR_TSRE;
			if (tmp & STS8251_PE)    line_status |= LSR_PE;
			if (tmp & STS8251_OE)    line_status |= LSR_OE;
			if (tmp & STS8251_FE)    line_status |= LSR_FE;
			if (tmp & STS8251_BD_SD) line_status |= LSR_BI;
		} else
#endif /* PC98 */
		line_status = inb(com->line_status_port);

		/* input event? (check first to help avoid overruns) */
		while (line_status & LSR_RCV_MASK) {
			/* break/unnattached error bits or real input? */
#ifdef PC98
			if(IS_8251(com->pc98_if_type)){
				recv_data = inb(com->data_port);
				if(tmp & 0x78){
					pc98_i8251_or_cmd(com,CMD8251_ER);
					recv_data = 0;
				}
			} else {
#endif /* PC98 */
			if (!(line_status & LSR_RXRDY))
				recv_data = 0;
			else
				recv_data = inb(com->data_port);
#ifdef PC98
			}
#endif
			if (line_status & (LSR_BI | LSR_FE | LSR_PE)) {
				/*
				 * Don't store BI if IGNBRK or FE/PE if IGNPAR.
				 * Otherwise, push the work to a higher level
				 * (to handle PARMRK) if we're bypassing.
				 * Otherwise, convert BI/FE and PE+INPCK to 0.
				 *
				 * This makes bypassing work right in the
				 * usual "raw" case (IGNBRK set, and IGNPAR
				 * and INPCK clear).
				 *
				 * Note: BI together with FE/PE means just BI.
				 */
				if (line_status & LSR_BI) {
#if defined(DDB) && defined(BREAK_TO_DEBUGGER)
					if (com->unit == comconsole) {
						breakpoint();
						goto cont;
					}
#endif
					if (com->tp == NULL
					    || com->tp->t_iflag & IGNBRK)
						goto cont;
				} else {
					if (com->tp == NULL
					    || com->tp->t_iflag & IGNPAR)
						goto cont;
				}
				if (com->tp->t_state & TS_CAN_BYPASS_L_RINT
				    && (line_status & (LSR_BI | LSR_FE)
					|| com->tp->t_iflag & INPCK))
					recv_data = 0;
			}
			++com->bytes_in;
			if (com->hotchar != 0 && recv_data == com->hotchar)
				setsofttty();
			ioptr = com->iptr;
			if (ioptr >= com->ibufend)
				CE_RECORD(com, CE_INTERRUPT_BUF_OVERFLOW);
			else {
				if (com->do_timestamp)
					microtime(&com->timestamp);
				++com_events;
				schedsofttty();
#if 0 /* for testing input latency vs efficiency */
if (com->iptr - com->ibuf == 8)
	setsofttty();
#endif
				ioptr[0] = recv_data;
				ioptr[CE_INPUT_OFFSET] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#ifdef PC98
					if(IS_8251(com->pc98_if_type))
						com_tiocm_bic(com, TIOCM_RTS);
					else
#endif
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
				if (line_status & LSR_OE)
					CE_RECORD(com, CE_OVERRUN);
			}
cont:
			/*
			 * "& 0x7F" is to avoid the gcc-1.40 generating a slow
			 * jump from the top of the loop to here
			 */
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				goto status_read;
			else
#endif
			line_status = inb(com->line_status_port) & 0x7F;
		}

		/* modem status change? (always check before doing output) */
#ifdef PC98
		if(!IS_8251(com->pc98_if_type)){
#endif
		modem_status = inb(com->modem_status_port);
		if (modem_status != com->last_modem_status) {
			if (com->do_dcd_timestamp
			    && !(com->last_modem_status & MSR_DCD)
			    && modem_status & MSR_DCD)
				microtime(&com->dcd_timestamp);

			/*
			 * Schedule high level to handle DCD changes.  Note
			 * that we don't use the delta bits anywhere.  Some
			 * UARTs mess them up, and it's easy to remember the
			 * previous bits and calculate the delta.
			 */
			com->last_modem_status = modem_status;
			if (!(com->state & CS_CHECKMSR)) {
				com_events += LOTS_OF_EVENTS;
				com->state |= CS_CHECKMSR;
				setsofttty();
			}

			/* handle CTS change immediately for crisp flow ctl */
			if (com->state & CS_CTS_OFLOW) {
				if (modem_status & MSR_CTS)
					com->state |= CS_ODEVREADY;
				else
					com->state &= ~CS_ODEVREADY;
			}
		}
#ifdef PC98
		}
#endif

		/* output queued and everything ready? */
		if (line_status & LSR_TXRDY
		    && com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
			ioptr = com->obufq.l_head;
			if (com->tx_fifo_size > 1) {
				u_int	ocount;

				ocount = com->obufq.l_tail - ioptr;
				if (ocount > com->tx_fifo_size)
					ocount = com->tx_fifo_size;
				com->bytes_out += ocount;
				do
					outb(com->data_port, *ioptr++);
				while (--ocount != 0);
			} else {
				outb(com->data_port, *ioptr++);
				++com->bytes_out;
			}
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				if ( !(pc98_check_i8251_interrupt(com) & IEN_TxFLAG) )
					com_int_Tx_enable(com);
#endif
			com->obufq.l_head = ioptr;
			if (COM_IIR_TXRDYBUG(com)) {
				int_ctl_new = int_ctl | IER_ETXRDY;
			}
			if (ioptr >= com->obufq.l_tail) {
				struct lbq	*qp;

				qp = com->obufq.l_next;
				qp->l_queued = FALSE;
				qp = qp->l_next;
				if (qp != NULL) {
					com->obufq.l_head = qp->l_head;
					com->obufq.l_tail = qp->l_tail;
					com->obufq.l_next = qp;
				} else {
					/* output just completed */
					if ( COM_IIR_TXRDYBUG(com) ) {
						int_ctl_new = int_ctl & ~IER_ETXRDY;
					}
					com->state &= ~CS_BUSY;
#if defined(PC98)
					if(IS_8251(com->pc98_if_type))
						if ( pc98_check_i8251_interrupt(com) & IEN_TxFLAG )
							com_int_Tx_disable(com);
#endif
				}
				if (!(com->state & CS_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->state |= CS_ODONE;
					setsofttty();	/* handle at high level ASAP */
				}
			}
			if ( COM_IIR_TXRDYBUG(com) && (int_ctl != int_ctl_new)) {
				outb(com->intr_ctl_port, int_ctl_new);
			}
		}
#ifdef PC98
		else if (line_status & LSR_TXRDY) {
			if(IS_8251(com->pc98_if_type))
				if ( pc98_check_i8251_interrupt(com) & IEN_TxFLAG )
					com_int_Tx_disable(com);
		}
		if(IS_8251(com->pc98_if_type))
			if ((tmp = inb(com->sts_port)) & STS8251_RxRDY)
				goto more_intr;
#endif

		/* finished? */
#ifndef COM_MULTIPORT
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			return;
#endif
		if ((inb(com->int_id_port) & IIR_IMASK) == IIR_NOPEND)
#endif /* COM_MULTIPORT */
			return;
	}
}

static int
sioioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	Port_t		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	int		oldcmd;
	struct termios	term;
#endif

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
	if (com->gone)
		return (ENODEV);
	iobase = com->iobase;
	if (mynor & CONTROL_MASK) {
		struct termios	*ct;

		switch (mynor & CONTROL_MASK) {
		case CONTROL_INIT_STATE:
			ct = mynor & CALLOUT_MASK ? &com->it_out : &com->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct = mynor & CALLOUT_MASK ? &com->lt_out : &com->lt_in;
			break;
		default:
			return (ENODEV);	/* /dev/nodev */
		}
		switch (cmd) {
		case TIOCSETA:
			error = suser(p->p_ucred, &p->p_acflag);
			if (error != 0)
				return (error);
			*ct = *(struct termios *)data;
			return (0);
		case TIOCGETA:
			*(struct termios *)data = *ct;
			return (0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return (0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return (0);
#ifdef DSI_SOFT_MODEM
		/*
		 * Download micro-code to Digicom modem.
		 */
		case TIOCDSIMICROCODE:
			{
			u_long l;
			u_char *p,*pi;

			pi = (u_char*)(*(caddr_t*)data);
			error = copyin(pi,&l,sizeof l);
			if(error)
				{return error;};
			pi += sizeof l;

			p = malloc(l,M_TEMP,M_NOWAIT);
			if(!p)
				{return ENOBUFS;}
			error = copyin(pi,p,l);
			if(error)
				{free(p,M_TEMP); return error;};
			if(error = LoadSoftModem(
			    MINOR_TO_UNIT(mynor),iobase,l,p))
				{free(p,M_TEMP); return error;}
			free(p,M_TEMP);
			return(0);
			}
#endif /* DSI_SOFT_MODEM */
		default:
			return (ENOTTY);
		}
	}
	tp = com->tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t)&term;
#endif
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int	cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt = mynor & CALLOUT_MASK
				     ? &com->lt_out : &com->lt_in;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
			      | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
			      | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
			      | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
			      | (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
		return (error);
	s = spltty();
	error = ttioctl(tp, cmd, data, flag);
	disc_optim(tp, &tp->t_termios, com);
	if (error != ENOIOCTL) {
		splx(s);
		return (error);
	}
#ifdef PC98
	if(IS_8251(com->pc98_if_type)){
	    switch (cmd) {
	    case TIOCSBRK:
		com_send_break_on( com );
		break;
	    case TIOCCBRK:
		com_send_break_off( com );
		break;
	    case TIOCSDTR:
		com_tiocm_bis(com, TIOCM_DTR | TIOCM_RTS );
		break;
	    case TIOCCDTR:
		com_tiocm_bic(com, TIOCM_DTR);
		break;
	/*
	 * XXX should disallow changing MCR_RTS if CS_RTS_IFLOW is set.  The
	 * changes get undone on the next call to comparam().
	 */
	    case TIOCMSET:
		com_tiocm_set( com, *(int *)data );
		break;
	    case TIOCMBIS:
		com_tiocm_bis( com, *(int *)data );
		break;
	    case TIOCMBIC:
		com_tiocm_bic( com, *(int *)data );
		break;
	    case TIOCMGET:
		*(int *)data = com_tiocm_get(com);
		break;
	    case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0) {
			splx(s);
			return (error);
		}
		com->dtr_wait = *(int *)data * hz / 100;
		break;
	    case TIOCMGDTRWAIT:
		*(int *)data = com->dtr_wait * 100 / hz;
		break;
	    case TIOCTIMESTAMP:
		com->do_timestamp = TRUE;
		*(struct timeval *)data = com->timestamp;
		break;
	    case TIOCDCDTIMESTAMP:
		com->do_dcd_timestamp = TRUE;
		*(struct timeval *)data = com->dcd_timestamp;
		break;
	    default:
		splx(s);
		return (ENOTTY);
	    }
	} else {
#endif
	switch (cmd) {
	case TIOCSBRK:
		outb(iobase + com_cfcr, com->cfcr_image |= CFCR_SBREAK);
		break;
	case TIOCCBRK:
		outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
		break;
	case TIOCSDTR:
		(void)commctl(com, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		(void)commctl(com, TIOCM_DTR, DMBIC);
		break;
	/*
	 * XXX should disallow changing MCR_RTS if CS_RTS_IFLOW is set.  The
	 * changes get undone on the next call to comparam().
	 */
	case TIOCMSET:
		(void)commctl(com, *(int *)data, DMSET);
		break;
	case TIOCMBIS:
		(void)commctl(com, *(int *)data, DMBIS);
		break;
	case TIOCMBIC:
		(void)commctl(com, *(int *)data, DMBIC);
		break;
	case TIOCMGET:
		*(int *)data = commctl(com, 0, DMGET);
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0) {
			splx(s);
			return (error);
		}
		com->dtr_wait = *(int *)data * hz / 100;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = com->dtr_wait * 100 / hz;
		break;
	case TIOCTIMESTAMP:
		com->do_timestamp = TRUE;
		*(struct timeval *)data = com->timestamp;
		break;
	case TIOCDCDTIMESTAMP:
		com->do_dcd_timestamp = TRUE;
		*(struct timeval *)data = com->dcd_timestamp;
		break;
	default:
		splx(s);
		return (ENOTTY);
	}
#ifdef PC98
	}
#endif
	splx(s);
	return (0);
}

void
siopoll()
{
	int		unit;

	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < NSIOTOT; ++unit) {
		u_char		*buf;
		struct com_s	*com;
		u_char		*ibuf;
		int		incc;
		struct tty	*tp;
#ifdef PC98
		int		tmp;
#endif

		com = com_addr(unit);
		if (com == NULL)
			continue;
		tp = com->tp;
		if (tp == NULL || com->gone) {
			/*
			 * Discard any events related to never-opened or
			 * going-away devices.
			 */
			disable_intr();
			incc = com->iptr - com->ibuf;
			com->iptr = com->ibuf;
			if (com->state & CS_CHECKMSR) {
				incc += LOTS_OF_EVENTS;
				com->state &= ~CS_CHECKMSR;
			}
			com_events -= incc;
			enable_intr();
			continue;
		}

		/* switch the role of the low-level input buffers */
		if (com->iptr == (ibuf = com->ibuf)) {
			buf = NULL;     /* not used, but compiler can't tell */
			incc = 0;
		} else {
			buf = ibuf;
			disable_intr();
			incc = com->iptr - buf;
			com_events -= incc;
			if (ibuf == com->ibuf1)
				ibuf = com->ibuf2;
			else
				ibuf = com->ibuf1;
			com->ibufend = ibuf + RS_IBUFSIZE;
			com->ihighwater = ibuf + RS_IHIGHWATER;
			com->iptr = ibuf;

			/*
			 * There is now room for another low-level buffer full
			 * of input, so enable RTS if it is now disabled and
			 * there is room in the high-level buffer.
			 */
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				tmp = com_tiocm_get(com) & TIOCM_RTS;
			else
				tmp = com->mcr_image & MCR_RTS;
#endif
			if ((com->state & CS_RTS_IFLOW)
#ifdef PC98
			    && !(tmp)
#else
			    && !(com->mcr_image & MCR_RTS)
#endif
			    && !(tp->t_state & TS_TBLOCK))
#ifdef PC98
				if(IS_8251(com->pc98_if_type))
					com_tiocm_bis(com, TIOCM_RTS);
				else
#endif
				outb(com->modem_ctl_port,
				     com->mcr_image |= MCR_RTS);
			enable_intr();
			com->ibuf = ibuf;
		}

		if (com->state & CS_CHECKMSR) {
			u_char	delta_modem_status;

#ifdef PC98
			if(!IS_8251(com->pc98_if_type)){
#endif
			disable_intr();
			delta_modem_status = com->last_modem_status
					     ^ com->prev_modem_status;
			com->prev_modem_status = com->last_modem_status;
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_CHECKMSR;
			enable_intr();
			if (delta_modem_status & MSR_DCD)
				(*linesw[tp->t_line].l_modem)
					(tp, com->prev_modem_status & MSR_DCD);
#ifdef PC98
			}
#endif
		}
		if (com->state & CS_ODONE) {
			disable_intr();
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_ODONE;
			enable_intr();
			if (!(com->state & CS_BUSY)
			    && !(com->extra_state & CSE_BUSYCHECK)) {
				timeout(siobusycheck, com, hz / 100);
				com->extra_state |= CSE_BUSYCHECK;
			}
			(*linesw[tp->t_line].l_start)(tp);
		}
		if (incc <= 0 || !(tp->t_state & TS_ISOPEN) ||
		    !(tp->t_cflag & CREAD))
			continue;
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode.  It usually takes about 450
		 * instructions (that's without canonical processing or echo!).
		 * slinput is reasonably fast (usually 40 instructions plus
		 * call overhead).
		 */
		if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
			if (tp->t_rawq.c_cc + incc >= RB_I_HIGH_WATER
			    && (com->state & CS_RTS_IFLOW
				|| tp->t_iflag & IXOFF)
			    && !(tp->t_state & TS_TBLOCK))
				ttyblock(tp);
			tk_nin += incc;
			tk_rawcc += incc;
			tp->t_rawcc += incc;
			com->delta_error_counts[CE_TTY_BUF_OVERFLOW]
				+= b_to_q((char *)buf, incc, &tp->t_rawq);
			ttwakeup(tp);
			if (tp->t_state & TS_TTSTOP
			    && (tp->t_iflag & IXANY
				|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
				tp->t_state &= ~TS_TTSTOP;
				tp->t_lflag &= ~FLUSHO;
				comstart(tp);
			}
		} else {
			do {
				u_char	line_status;
				int	recv_data;

				line_status = (u_char) buf[CE_INPUT_OFFSET];
				recv_data = (u_char) *buf++;
				if (line_status
				    & (LSR_BI | LSR_FE | LSR_OE | LSR_PE)) {
					if (line_status & LSR_BI)
						recv_data |= TTY_BI;
					if (line_status & LSR_FE)
						recv_data |= TTY_FE;
					if (line_status & LSR_OE)
						recv_data |= TTY_OE;
					if (line_status & LSR_PE)
						recv_data |= TTY_PE;
				}
				(*linesw[tp->t_line].l_rint)(recv_data, tp);
			} while (--incc > 0);
		}
		if (com_events == 0)
			break;
	}
	if (com_events >= LOTS_OF_EVENTS)
		goto repeat;
}

static int
comparam(tp, t)
	struct tty	*tp;
	struct termios	*t;
{
	u_int		cfcr;
	int		cflag;
	struct com_s	*com;
	int		divisor;
	u_char		dlbh;
	u_char		dlbl;
	int		error;
	Port_t		iobase;
	int		s;
	int		unit;
	int		txtimeout;
#ifdef PC98
	Port_t		tmp_port;
	int		tmp_flg;
#endif

#ifdef PC98
	cfcr = 0;
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
	if(IS_8251(com->pc98_if_type)) {
		divisor = pc98_ttspeedtab(com, t->c_ospeed);
	} else
#endif
	/* do historical conversions */
	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	/* check requested parameters */
	divisor = ttspeedtab(t->c_ospeed, comspeedtab);
	if (divisor < 0 || divisor > 0 && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/* parameters are OK, convert them to the com struct and the device */
#ifndef PC98
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
#endif
	s = spltty();
#ifdef PC98
	if(IS_8251(com->pc98_if_type)){
		if(divisor == 0)
			com_tiocm_bic( com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE );
		else
			com_tiocm_bis( com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE );
	} else {
#endif
	if (divisor == 0)
		(void)commctl(com, TIOCM_DTR, DMBIC);	/* hang up line */
	else
		(void)commctl(com, TIOCM_DTR, DMBIS);
#ifdef PC98
	}
#endif
	cflag = t->c_cflag;
#ifdef PC98
	if(!IS_8251(com->pc98_if_type)){
#endif
	switch (cflag & CSIZE) {
	case CS5:
		cfcr = CFCR_5BITS;
		break;
	case CS6:
		cfcr = CFCR_6BITS;
		break;
	case CS7:
		cfcr = CFCR_7BITS;
		break;
	default:
		cfcr = CFCR_8BITS;
		break;
	}
	if (cflag & PARENB) {
		cfcr |= CFCR_PENAB;
		if (!(cflag & PARODD))
			cfcr |= CFCR_PEVEN;
	}
	if (cflag & CSTOPB)
		cfcr |= CFCR_STOPB;

	if (com->hasfifo && divisor != 0) {
		/*
		 * Use a fifo trigger level low enough so that the input
		 * latency from the fifo is less than about 16 msec and
		 * the total latency is less than about 30 msec.  These
		 * latencies are reasonable for humans.  Serial comms
		 * protocols shouldn't expect anything better since modem
		 * latencies are larger.
		 */
		com->fifo_image = t->c_ospeed <= 4800
				  ? FIFO_ENABLE : FIFO_ENABLE | FIFO_RX_HIGH;
#ifdef COM_ESP
		/*
		 * The Hayes ESP card needs the fifo DMA mode bit set
		 * in compatibility mode.  If not, it will interrupt
		 * for each character received.
		 */
		if (com->esp)
			com->fifo_image |= FIFO_DMA_MODE;
#endif
		outb(iobase + com_fifo, com->fifo_image);
	}

	/*
	 * Some UARTs lock up if the divisor latch registers are selected
	 * while the UART is doing output (they refuse to transmit anything
	 * more until given a hard reset).  Fix this by stopping filling
	 * the device buffers and waiting for them to drain.  Reading the
	 * line status port outside of siointr1() might lose some receiver
	 * error bits, but that is acceptable here.
	 */
#ifdef PC98
	}
#endif
	disable_intr();
retry:
	com->state &= ~CS_TTGO;
	txtimeout = tp->t_timeout;
	enable_intr();
#ifdef PC98
	if(IS_8251(com->pc98_if_type)){
		tmp_port = com->sts_port;
		tmp_flg = (STS8251_TxRDY|STS8251_TxEMP);
	} else {
		tmp_port = com->line_status_port;
		tmp_flg = (LSR_TSRE|LSR_TXRDY);
	}
	while ((inb(tmp_port) & tmp_flg) != tmp_flg) {
#else
	while ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	       != (LSR_TSRE | LSR_TXRDY)) {
#endif
		tp->t_state |= TS_SO_OCOMPLETE;
		error = ttysleep(tp, TSA_OCOMPLETE(tp), TTIPRI | PCATCH,
				 "siotx", hz / 100);
		if (   txtimeout != 0
		    && (!error || error	== EAGAIN)
		    && (txtimeout -= hz	/ 100) <= 0
		   )
			error = EIO;
		if (com->gone)
			error = ENODEV;
		if (error != 0 && error != EAGAIN) {
			if (!(tp->t_state & TS_TTSTOP)) {
				disable_intr();
				com->state |= CS_TTGO;
				enable_intr();
			}
			splx(s);
			return (error);
		}
	}

	disable_intr();		/* very important while com_data is hidden */

	/*
	 * XXX - clearing CS_TTGO is not sufficient to stop further output,
	 * because siopoll() calls comstart() which usually sets it again
	 * because TS_TTSTOP is clear.  Setting TS_TTSTOP would not be
	 * sufficient, for similar reasons.
	 */
#ifdef PC98
	if ((inb(tmp_port) & tmp_flg) != tmp_flg)
#else
	if ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	    != (LSR_TSRE | LSR_TXRDY))
#endif
		goto retry;

#ifdef PC98
	if(!IS_8251(com->pc98_if_type)){
#endif
	if (divisor != 0) {
		outb(iobase + com_cfcr, cfcr | CFCR_DLAB);
		/*
		 * Only set the divisor registers if they would change,
		 * since on some 16550 incompatibles (UMC8669F), setting
		 * them while input is arriving them loses sync until
		 * data stops arriving.
		 */
		dlbl = divisor & 0xFF;
		if (inb(iobase + com_dlbl) != dlbl)
			outb(iobase + com_dlbl, dlbl);
		dlbh = (u_int) divisor >> 8;
		if (inb(iobase + com_dlbh) != dlbh)
			outb(iobase + com_dlbh, dlbh);
	}


	outb(iobase + com_cfcr, com->cfcr_image = cfcr);

#ifdef PC98
	} else
		com_cflag_and_speed_set(com, cflag, t->c_ospeed);
#endif
	if (!(tp->t_state & TS_TTSTOP))
		com->state |= CS_TTGO;

	if (cflag & CRTS_IFLOW) {
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) | 0x40);
		}
		com->state |= CS_RTS_IFLOW;
		/*
		 * If CS_RTS_IFLOW just changed from off to on, the change
		 * needs to be propagated to MCR_RTS.  This isn't urgent,
		 * so do it later by calling comstart() instead of repeating
		 * a lot of code from comstart() here.
		 */
	} else if (com->state & CS_RTS_IFLOW) {
		com->state &= ~CS_RTS_IFLOW;
		/*
		 * CS_RTS_IFLOW just changed from on to off.  Force MCR_RTS
		 * on here, since comstart() won't do it later.
		 */
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			com_tiocm_bis(com, TIOCM_RTS);
		else
#endif
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) & ~0x40);
		}
	}


	/*
	 * Set up state to handle output flow control.
	 * XXX - worth handling MDMBUF (DCD) flow control at the lowest level?
	 * Now has 10+ msec latency, while CTS flow has 50- usec latency.
	 */
	com->state |= CS_ODEVREADY;
	com->state &= ~CS_CTS_OFLOW;
	if (cflag & CCTS_OFLOW) {
		com->state |= CS_CTS_OFLOW;
#ifdef PC98
		if(IS_8251(com->pc98_if_type)){
			if (!(pc98_get_modem_status(com) & TIOCM_CTS))
				com->state &= ~CS_ODEVREADY;
		} else {
#endif
		if (!(com->last_modem_status & MSR_CTS))
			com->state &= ~CS_ODEVREADY;
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) | 0x80);
		}
#ifdef PC98
		}
#endif
	} else {
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) & ~0x80);
		}
	}


	outb(iobase + com_cfcr, com->cfcr_image);


	/* XXX shouldn't call functions while intrs are disabled. */
	disc_optim(tp, t, com);
	/*
	 * Recover from fiddling with CS_TTGO.  We used to call siointr1()
	 * unconditionally, but that defeated the careful discarding of
	 * stale input in sioopen().
	 */
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);

	enable_intr();
	splx(s);
	comstart(tp);
	return (0);
}

static void
comstart(tp)
	struct tty	*tp;
{
	struct com_s	*com;
	int		s;
	int		unit;
#ifdef PC98
	int		tmp;
#endif

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	s = spltty();
	disable_intr();
	if (tp->t_state & TS_TTSTOP)
		com->state &= ~CS_TTGO;
	else
		com->state |= CS_TTGO;
	if (tp->t_state & TS_TBLOCK) {
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			tmp = com_tiocm_get(com) & TIOCM_RTS;
		else
			tmp = com->mcr_image & MCR_RTS;
		if (tmp && (com->state & CS_RTS_IFLOW))
#else
		if (com->mcr_image & MCR_RTS && com->state & CS_RTS_IFLOW)
#endif
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				com_tiocm_bic(com, TIOCM_RTS);
			else
#endif
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
	} else {
#ifdef PC98
		if(IS_8251(com->pc98_if_type))
			tmp = com_tiocm_get(com) & TIOCM_RTS;
		else
			tmp = com->mcr_image & MCR_RTS;
		if (!(tmp) && com->iptr < com->ihighwater
			&& com->state & CS_RTS_IFLOW)
#else
		if (!(com->mcr_image & MCR_RTS) && com->iptr < com->ihighwater
		    && com->state & CS_RTS_IFLOW)
#endif
#ifdef PC98
			if(IS_8251(com->pc98_if_type))
				com_tiocm_bis(com, TIOCM_RTS);
			else
#endif
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
#ifdef PC98
/*		if(IS_8251(com->pc98_if_type))
			com_int_Tx_enable(com); */
#endif
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc != 0) {
		struct lbq	*qp;
		struct lbq	*next;

		if (!com->obufs[0].l_queued) {
			com->obufs[0].l_tail
			    = com->obuf1 + q_to_b(&tp->t_outq, com->obuf1,
						  sizeof com->obuf1);
			com->obufs[0].l_next = NULL;
			com->obufs[0].l_queued = TRUE;
			disable_intr();
			if (com->state & CS_BUSY) {
				qp = com->obufq.l_next;
				while ((next = qp->l_next) != NULL)
					qp = next;
				qp->l_next = &com->obufs[0];
			} else {
				com->obufq.l_head = com->obufs[0].l_head;
				com->obufq.l_tail = com->obufs[0].l_tail;
				com->obufq.l_next = &com->obufs[0];
				com->state |= CS_BUSY;
			}
			enable_intr();
		}
		if (tp->t_outq.c_cc != 0 && !com->obufs[1].l_queued) {
			com->obufs[1].l_tail
			    = com->obuf2 + q_to_b(&tp->t_outq, com->obuf2,
						  sizeof com->obuf2);
			com->obufs[1].l_next = NULL;
			com->obufs[1].l_queued = TRUE;
			disable_intr();
			if (com->state & CS_BUSY) {
				qp = com->obufq.l_next;
				while ((next = qp->l_next) != NULL)
					qp = next;
				qp->l_next = &com->obufs[1];
			} else {
				com->obufq.l_head = com->obufs[1].l_head;
				com->obufq.l_tail = com->obufs[1].l_tail;
				com->obufq.l_next = &com->obufs[1];
				com->state |= CS_BUSY;
			}
			enable_intr();
		}
		tp->t_state |= TS_BUSY;
	}
	disable_intr();
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);	/* fake interrupt to start output */
	enable_intr();
#ifdef PC98
/*		if(IS_8251(com->pc98_if_type))
			com_int_Tx_enable(com); */
#endif
	ttwwakeup(tp);
	splx(s);
}

static void
siostop(tp, rw)
	struct tty	*tp;
	int		rw;
{
	struct com_s	*com;

	com = com_addr(DEV_TO_UNIT(tp->t_dev));
	if (com->gone)
		return;
	disable_intr();
	if (rw & FWRITE) {
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			/* XXX does this flush everything? */
			outb(com->iobase + com_fifo,
			     FIFO_XMT_RST | com->fifo_image);
		com->obufs[0].l_queued = FALSE;
		com->obufs[1].l_queued = FALSE;
		if (com->state & CS_ODONE)
			com_events -= LOTS_OF_EVENTS;
		com->state &= ~(CS_ODONE | CS_BUSY);
		com->tp->t_state &= ~TS_BUSY;
	}
	if (rw & FREAD) {
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			/* XXX does this flush everything? */
			outb(com->iobase + com_fifo,
			     FIFO_RCV_RST | com->fifo_image);
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	enable_intr();
	comstart(tp);
}

static struct tty *
siodevtotty(dev)
	dev_t	dev;
{
	int	mynor;
	int	unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (NULL);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIOTOT)
		return (NULL);
	return (&sio_tty[unit]);
}

static int
commctl(com, bits, how)
	struct com_s	*com;
	int		bits;
	int		how;
{
	int	mcr;
	int	msr;

	if (how == DMGET) {
		bits = TIOCM_LE;	/* XXX - always enabled while open */
		mcr = com->mcr_image;
		if (mcr & MCR_DTR)
			bits |= TIOCM_DTR;
		if (mcr & MCR_RTS)
			bits |= TIOCM_RTS;
		msr = com->prev_modem_status;
		if (msr & MSR_CTS)
			bits |= TIOCM_CTS;
		if (msr & MSR_DCD)
			bits |= TIOCM_CD;
		if (msr & MSR_DSR)
			bits |= TIOCM_DSR;
		/*
		 * XXX - MSR_RI is naturally volatile, and we make MSR_TERI
		 * more volatile by reading the modem status a lot.  Perhaps
		 * we should latch both bits until the status is read here.
		 */
		if (msr & (MSR_RI | MSR_TERI))
			bits |= TIOCM_RI;
		return (bits);
	}
	mcr = 0;
	if (bits & TIOCM_DTR)
		mcr |= MCR_DTR;
	if (bits & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (com->gone)
		return(0);
	disable_intr();
	switch (how) {
	case DMSET:
		outb(com->modem_ctl_port,
		     com->mcr_image = mcr | (com->mcr_image & MCR_IENABLE));
		break;
	case DMBIS:
		outb(com->modem_ctl_port, com->mcr_image |= mcr);
		break;
	case DMBIC:
		outb(com->modem_ctl_port, com->mcr_image &= ~mcr);
		break;
	}
	enable_intr();
	return (0);
}

static void
siosettimeout()
{
	struct com_s	*com;
	bool_t		someopen;
	int		unit;

	/*
	 * Set our timeout period to 1 second if no polled devices are open.
	 * Otherwise set it to max(1/200, 1/hz).
	 * Enable timeouts iff some device is open.
	 */
	untimeout(comwakeup, (void *)NULL, sio_timeout_handle);
	sio_timeout = hz;
	someopen = FALSE;
	for (unit = 0; unit < NSIOTOT; ++unit) {
		com = com_addr(unit);
		if (com != NULL && com->tp != NULL
		    && com->tp->t_state & TS_ISOPEN && !com->gone) {
			someopen = TRUE;
			if (com->poll || com->poll_output) {
				sio_timeout = hz > 200 ? hz / 200 : 1;
				break;
			}
		}
	}
	if (someopen) {
		sio_timeouts_until_log = hz / sio_timeout;
		sio_timeout_handle = timeout(comwakeup, (void *)NULL,
					     sio_timeout);
	} else {
		/* Flush error messages, if any. */
		sio_timeouts_until_log = 1;
		comwakeup((void *)NULL);
		untimeout(comwakeup, (void *)NULL, sio_timeout_handle);
	}
}

static void
comwakeup(chan)
	void	*chan;
{
	struct com_s	*com;
	int		unit;

	sio_timeout_handle = timeout(comwakeup, (void *)NULL, sio_timeout);

	/*
	 * Recover from lost output interrupts.
	 * Poll any lines that don't use interrupts.
	 */
	for (unit = 0; unit < NSIOTOT; ++unit) {
		com = com_addr(unit);
		if (com != NULL && !com->gone
		    && (com->state >= (CS_BUSY | CS_TTGO) || com->poll)) {
			disable_intr();
			siointr1(com);
			enable_intr();
		}
	}

	/*
	 * Check for and log errors, but not too often.
	 */
	if (--sio_timeouts_until_log > 0)
		return;
	sio_timeouts_until_log = hz / sio_timeout;
	for (unit = 0; unit < NSIOTOT; ++unit) {
		int	errnum;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		if (com->gone)
			continue;
		for (errnum = 0; errnum < CE_NTYPES; ++errnum) {
			u_int	delta;
			u_long	total;

			disable_intr();
			delta = com->delta_error_counts[errnum];
			com->delta_error_counts[errnum] = 0;
			enable_intr();
			if (delta == 0)
				continue;
			total = com->error_counts[errnum] += delta;
			log(LOG_ERR, "sio%d: %u more %s%s (total %lu)\n",
			    unit, delta, error_desc[errnum],
			    delta == 1 ? "" : "s", total);
		}
	}
}

#ifdef PC98
/* commint is called when modem control line changes */
static void
commint(dev_t dev)
{
	register struct tty *tp;
	int	stat,delta;
	struct com_s *com;
	int	mynor,unit;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	com = com_addr(unit);
	tp = com->tp;

	stat = com_tiocm_get(com);
	delta = com_tiocm_get_delta(com);

	if (com->state & CS_CTS_OFLOW) {
		if (stat & TIOCM_CTS)
			com->state |= CS_ODEVREADY;
		else
			com->state &= ~CS_ODEVREADY;
	}
	if ((delta & TIOCM_CAR) && (mynor & CALLOUT_MASK) == 0) {
	    if (stat & TIOCM_CAR )
		(void)(*linesw[tp->t_line].l_modem)(tp, 1);
	    else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
		/* negate DTR, RTS */
		com_tiocm_bic(com, (tp->t_cflag & HUPCL) ?
				TIOCM_DTR|TIOCM_RTS|TIOCM_LE : TIOCM_LE );
		/* disable IENABLE */
		com_int_TxRx_disable( com );
	    }
	}
}
#endif

static void
disc_optim(tp, t, com)
	struct tty	*tp;
	struct termios	*t;
	struct com_s	*com;
{
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK)
		|| (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
	    && linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	com->hotchar = linesw[tp->t_line].l_hotchar;
}

/*
 * Following are all routines needed for SIO to act as console
 */
#include <machine/cons.h>

struct siocnstate {
	u_char	dlbl;
	u_char	dlbh;
	u_char	ier;
	u_char	cfcr;
	u_char	mcr;
};

static speed_t siocngetspeed __P((Port_t, struct speedtab *));
static void siocnclose	__P((struct siocnstate *sp));
static void siocnopen	__P((struct siocnstate *sp));
static void siocntxwait	__P((void));

static void
siocntxwait()
{
	int	timo;

	/*
	 * Wait for any pending transmission to finish.  Required to avoid
	 * the UART lockup bug when the speed is changed, and for normal
	 * transmits.
	 */
	timo = 100000;
	while ((inb(siocniobase + com_lsr) & (LSR_TSRE | LSR_TXRDY))
	       != (LSR_TSRE | LSR_TXRDY) && --timo != 0)
		;
}

/*
 * Read the serial port specified and try to figure out what speed
 * it's currently running at.  We're assuming the serial port has
 * been initialized and is basicly idle.  This routine is only intended
 * to be run at system startup.
 *
 * If the value read from the serial port doesn't make sense, return 0.
 */

static speed_t
siocngetspeed(iobase, table)
	Port_t iobase;
	struct speedtab *table;
{
	int	code;
	u_char	dlbh;
	u_char	dlbl;
	u_char  cfcr;

	cfcr = inb(iobase + com_cfcr);
	outb(iobase + com_cfcr, CFCR_DLAB | cfcr);

	dlbl = inb(iobase + com_dlbl);
	dlbh = inb(iobase + com_dlbh);

	outb(iobase + com_cfcr, cfcr);

	code = dlbh << 8 | dlbl;

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);

	return 0;	/* didn't match anything sane */
}

static void
siocnopen(sp)
	struct siocnstate	*sp;
{
	int	divisor;
	u_char	dlbh;
	u_char	dlbl;
	Port_t	iobase;

	/*
	 * Save all the device control registers except the fifo register
	 * and set our default ones (cs8 -parenb speed=comdefaultrate).
	 * We can't save the fifo register since it is read-only.
	 */
	iobase = siocniobase;
	sp->ier = inb(iobase + com_ier);
	outb(iobase + com_ier, 0);	/* spltty() doesn't stop siointr() */
	siocntxwait();
	sp->cfcr = inb(iobase + com_cfcr);
	outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
	sp->dlbl = inb(iobase + com_dlbl);
	sp->dlbh = inb(iobase + com_dlbh);
	/*
	 * Only set the divisor registers if they would change, since on
	 * some 16550 incompatibles (Startech), setting them clears the
	 * data input register.  This also reduces the effects of the
	 * UMC8669F bug.
	 */
	divisor = ttspeedtab(comdefaultrate, comspeedtab);
	dlbl = divisor & 0xFF;
	if (sp->dlbl != dlbl)
		outb(iobase + com_dlbl, dlbl);
	dlbh = (u_int) divisor >> 8;
	if (sp->dlbh != dlbh)
		outb(iobase + com_dlbh, dlbh);
	outb(iobase + com_cfcr, CFCR_8BITS);
	sp->mcr = inb(iobase + com_mcr);
	/*
	 * We don't want interrupts, but must be careful not to "disable"
	 * them by clearing the MCR_IENABLE bit, since that might cause
	 * an interrupt by floating the IRQ line.
	 */
	outb(iobase + com_mcr, (sp->mcr & MCR_IENABLE) | MCR_DTR | MCR_RTS);
}

static void
siocnclose(sp)
	struct siocnstate	*sp;
{
	Port_t	iobase;

	/*
	 * Restore the device control registers.
	 */
	siocntxwait();
	iobase = siocniobase;
	outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
	if (sp->dlbl != inb(iobase + com_dlbl))
		outb(iobase + com_dlbl, sp->dlbl);
	if (sp->dlbh != inb(iobase + com_dlbh))
		outb(iobase + com_dlbh, sp->dlbh);
	outb(iobase + com_cfcr, sp->cfcr);
	/*
	 * XXX damp oscillations of MCR_DTR and MCR_RTS by not restoring them.
	 */
	outb(iobase + com_mcr, sp->mcr | MCR_DTR | MCR_RTS);
	outb(iobase + com_ier, sp->ier);
}

void
siocnprobe(cp)
	struct consdev	*cp;
{
	speed_t			boot_speed;
	u_char			cfcr;
	struct isa_device	*dvp;
	int			s;
	struct siocnstate	sp;

	/*
	 * Find our first enabled console, if any.  If it is a high-level
	 * console device, then initialize it and return successfully.
	 * If it is a low-level console device, then initialize it and
	 * return unsuccessfully.  It must be initialized in both cases
	 * for early use by console drivers and debuggers.  Initializing
	 * the hardware is not necessary in all cases, since the i/o
	 * routines initialize it on the fly, but it is necessary if
	 * input might arrive while the hardware is switched back to an
	 * uninitialized state.  We can't handle multiple console devices
	 * yet because our low-level routines don't take a device arg.
	 * We trust the user to set the console flags properly so that we
	 * don't need to probe.
	 */
	cp->cn_pri = CN_DEAD;
	for (dvp = isa_devtab_tty; dvp->id_driver != NULL; dvp++)
		if (dvp->id_driver == &siodriver && dvp->id_enabled
		    && COM_CONSOLE(dvp)) {
			siocniobase = dvp->id_iobase;
			s = spltty();
			if (boothowto & RB_SERIAL) {
				boot_speed = siocngetspeed(siocniobase,
							   comspeedtab);
				if (boot_speed)
					comdefaultrate = boot_speed;
			}

			/*
			 * Initialize the divisor latch.  We can't rely on
			 * siocnopen() to do this the first time, since it 
			 * avoids writing to the latch if the latch appears
			 * to have the correct value.  Also, if we didn't
			 * just read the speed from the hardware, then we
			 * need to set the speed in hardware so that
			 * switching it later is null.
			 */
			cfcr = inb(siocniobase + com_cfcr);
			outb(siocniobase + com_cfcr, CFCR_DLAB | cfcr);
			outb(siocniobase + com_dlbl,
			     COMBRD(comdefaultrate) & 0xff);
			outb(siocniobase + com_dlbh,
			     (u_int) COMBRD(comdefaultrate) >> 8);
			outb(siocniobase + com_cfcr, cfcr);

			siocnopen(&sp);
			splx(s);
			if (!COM_LLCONSOLE(dvp)) {
				cp->cn_dev = makedev(CDEV_MAJOR, dvp->id_unit);
				cp->cn_pri = COM_FORCECONSOLE(dvp)
					     || boothowto & RB_SERIAL
					     ? CN_REMOTE : CN_NORMAL;
			}
			break;
		}
}

void
siocninit(cp)
	struct consdev	*cp;
{
	comconsole = DEV_TO_UNIT(cp->cn_dev);
}

int
siocncheckc(dev)
	dev_t	dev;
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siocniobase;
	s = spltty();
	siocnopen(&sp);
	if (inb(iobase + com_lsr) & LSR_RXRDY)
		c = inb(iobase + com_data);
	else
		c = -1;
	siocnclose(&sp);
	splx(s);
	return (c);
}


int
siocngetc(dev)
	dev_t	dev;
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siocniobase;
	s = spltty();
	siocnopen(&sp);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp);
	splx(s);
	return (c);
}

void
siocnputc(dev, c)
	dev_t	dev;
	int	c;
{
	int	s;
	struct siocnstate	sp;

	s = spltty();
	siocnopen(&sp);
	siocntxwait();
	outb(siocniobase + com_data, c);
	siocnclose(&sp);
	splx(s);
}

#ifdef DSI_SOFT_MODEM
/*
 * The magic code to download microcode to a "Connection 14.4+Fax"
 * modem from Digicom Systems Inc.  Very magic.
 */

#define DSI_ERROR(str) { ptr = str; goto error; }
static int
LoadSoftModem(int unit, int base_io, u_long size, u_char *ptr)
{
    int int_c,int_k;
    int data_0188, data_0187;

    /*
     * First see if it is a DSI SoftModem
     */
    if(!((inb(base_io+7) ^ inb(base_io+7)) & 0x80))
	return ENODEV;

    data_0188 = inb(base_io+4);
    data_0187 = inb(base_io+3);
    outb(base_io+3,0x80);
    outb(base_io+4,0x0C);
    outb(base_io+0,0x31);
    outb(base_io+1,0x8C);
    outb(base_io+7,0x10);
    outb(base_io+7,0x19);

    if(0x18 != (inb(base_io+7) & 0x1A))
	DSI_ERROR("dsp bus not granted");

    if(0x01 != (inb(base_io+7) & 0x01)) {
	outb(base_io+7,0x18);
	outb(base_io+7,0x19);
	if(0x01 != (inb(base_io+7) & 0x01))
	    DSI_ERROR("program mem not granted");
    }

    int_c = 0;

    while(1) {
	if(int_c >= 7 || size <= 0x1800)
	    break;

	for(int_k = 0 ; int_k < 0x800; int_k++) {
	    outb(base_io+0,*ptr++);
	    outb(base_io+1,*ptr++);
	    outb(base_io+2,*ptr++);
	}

	size -= 0x1800;
	int_c++;
    }

    if(size > 0x1800) {
 	outb(base_io+7,0x18);
 	outb(base_io+7,0x19);
	if(0x00 != (inb(base_io+7) & 0x01))
	    DSI_ERROR("program data not granted");

	for(int_k = 0 ; int_k < 0x800; int_k++) {
	    outb(base_io+1,*ptr++);
	    outb(base_io+2,0);
	    outb(base_io+1,*ptr++);
	    outb(base_io+2,*ptr++);
	}

	size -= 0x1800;

	while(size > 0x1800) {
	    for(int_k = 0 ; int_k < 0xC00; int_k++) {
		outb(base_io+1,*ptr++);
		outb(base_io+2,*ptr++);
	    }
	    size -= 0x1800;
	}

	if(size < 0x1800) {
	    for(int_k=0;int_k<size/2;int_k++) {
		outb(base_io+1,*ptr++);
		outb(base_io+2,*ptr++);
	    }
	}

    } else if (size > 0) {
	if(int_c == 7) {
	    outb(base_io+7,0x18);
	    outb(base_io+7,0x19);
	    if(0x00 != (inb(base_io+7) & 0x01))
		DSI_ERROR("program data not granted");
	    for(int_k = 0 ; int_k < size/3; int_k++) {
		outb(base_io+1,*ptr++);
		outb(base_io+2,0);
		outb(base_io+1,*ptr++);
		outb(base_io+2,*ptr++);
	    }
	} else {
	    for(int_k = 0 ; int_k < size/3; int_k++) {
		outb(base_io+0,*ptr++);
		outb(base_io+1,*ptr++);
		outb(base_io+2,*ptr++);
	    }
	}
    }
    outb(base_io+7,0x11);
    outb(base_io+7,3);

    outb(base_io+4,data_0188 & 0xfb);

    outb(base_io+3,data_0187);

    return 0;
error:
    printf("sio%d: DSI SoftModem microcode load failed: <%s>\n",unit,ptr);
    outb(base_io+7,0x00); \
    outb(base_io+3,data_0187); \
    outb(base_io+4,data_0188);  \
    return EIO;
}
#endif /* DSI_SOFT_MODEM */

/*
 * support PnP cards if we are using 'em
 */

#if NPNP > 0

static struct siopnp_ids {
	u_long vend_id;
	char *id_str;
} siopnp_ids[] = {
	{ 0x5015f435, "MOT1550"},
	{ 0x8113b04e, "Supra1381"},
	{ 0x9012b04e, "Supra1290"},
	{ 0x11007256, "USR0011"},
	{ 0x30207256, "USR2030"},
	{ 0 }
};

static char *siopnp_probe(u_long csn, u_long vend_id);
static void siopnp_attach(u_long csn, u_long vend_id, char *name,
	struct isa_device *dev);
static u_long nsiopnp = NSIO;

static struct pnp_device siopnp = {
	"siopnp",
	siopnp_probe,
	siopnp_attach,
	&nsiopnp,
	&tty_imask
};
DATA_SET (pnpdevice_set, siopnp);

static char *
siopnp_probe(u_long csn, u_long vend_id)
{
	struct siopnp_ids *ids;
	char *s = NULL;

	for(ids = siopnp_ids; ids->vend_id != 0; ids++) {
		if (vend_id == ids->vend_id) {
			s = ids->id_str;
			break;
		}
	}

	if (s) {
		struct pnp_cinfo d;
		read_pnp_parms(&d, 0);
		if (d.enable == 0 || d.flags & 1) {
			printf("CSN %d is disabled.\n", csn);
			return (NULL);
		}

	}

	return (s);
}

static void
siopnp_attach(u_long csn, u_long vend_id, char *name, struct isa_device *dev)
{
	struct pnp_cinfo d;
	struct isa_device *dvp;

	if (dev->id_unit >= NSIOTOT)
		return;

	if (read_pnp_parms(&d, 0) == 0) {
		printf("failed to read pnp parms\n");
		return;
	}

	write_pnp_parms(&d, 0);

	enable_pnp_card();

	dev->id_iobase = d.port[0];
	dev->id_irq = (1 << d.irq[0]);
	dev->id_intr = siointr;
	dev->id_ri_flags = RI_FAST;
	dev->id_drq = -1;

	if (dev->id_driver == NULL) {
		dev->id_driver = &siodriver;
		dvp = find_isadev(isa_devtab_tty, &siodriver, 0);
		if (dvp != NULL)
			dev->id_id = dvp->id_id;
	}

	if ((dev->id_alive = sioprobe(dev)) != 0)
		sioattach(dev);
	else
		printf("sio%d: probe failed\n", dev->id_unit);
}
#endif
#ifdef PC98
/*
 *  pc98 local function
 */

static void
com_tiocm_set(struct com_s *com, int msr)
{
	int	s;
	int	tmp = 0;
	int	mask = CMD8251_TxEN|CMD8251_RxEN|CMD8251_DTR|CMD8251_RTS;

	s=spltty();
	com->pc98_prev_modem_status = ( msr & (TIOCM_LE|TIOCM_DTR|TIOCM_RTS) )
	   | ( com->pc98_prev_modem_status & ~(TIOCM_LE|TIOCM_DTR|TIOCM_RTS) );
	tmp |= (CMD8251_TxEN|CMD8251_RxEN);
	if ( msr & TIOCM_DTR ) tmp |= CMD8251_DTR;
	if ( msr & TIOCM_RTS ) tmp |= CMD8251_RTS;
	pc98_i8251_clear_or_cmd( com, mask, tmp );
	splx(s);
}

static void
com_tiocm_bis(struct com_s *com, int msr)
{
	int	s;
	int	tmp = 0;

	s=spltty();
	com->pc98_prev_modem_status |= ( msr & (TIOCM_LE|TIOCM_DTR|TIOCM_RTS) );
	tmp |= CMD8251_TxEN|CMD8251_RxEN;
	if ( msr & TIOCM_DTR ) tmp |= CMD8251_DTR;
	if ( msr & TIOCM_RTS ) tmp |= CMD8251_RTS;

	pc98_i8251_or_cmd( com, tmp );
	splx(s);
}

static void
com_tiocm_bic(struct com_s *com, int msr)
{
	int	s;
	int	tmp = msr;

	s=spltty();
	com->pc98_prev_modem_status &= ~( msr & (TIOCM_LE|TIOCM_DTR|TIOCM_RTS) );
	if ( msr & TIOCM_DTR ) tmp |= CMD8251_DTR;
	if ( msr & TIOCM_RTS ) tmp |= CMD8251_RTS;

	pc98_i8251_clear_cmd( com, tmp );
	splx(s);
}

static int
com_tiocm_get(struct com_s *com)
{
	return( com->pc98_prev_modem_status );
}

static int
com_tiocm_get_delta(struct com_s *com)
{
	int	tmp;

	tmp = com->pc98_modem_delta;
	com->pc98_modem_delta = 0;
	return( tmp );
}

/* convert to TIOCM_?? ( ioctl.h ) */
static int
pc98_get_modem_status(struct com_s *com)
{
	int	stat, stat2;
	register int	msr;

	stat  = inb(com->sts_port);
	stat2 = inb(com->in_modem_port);
	msr = com->pc98_prev_modem_status
			& ~(TIOCM_CAR|TIOCM_RI|TIOCM_DSR|TIOCM_CTS);
	if ( !(stat2 & CICSCD_CD) ) msr |= TIOCM_CAR;
	if ( !(stat2 & CICSCD_CI) ) msr |= TIOCM_RI;
	if (   stat & STS8251_DSR ) msr |= TIOCM_DSR;
	if ( !(stat2 & CICSCD_CS) ) msr |= TIOCM_CTS;
#if COM_CARRIER_DETECT_EMULATE
	if ( msr & (TIOCM_DSR|TIOCM_CTS) ) {
		msr |= TIOCM_CAR;
	}
#endif
	return(msr);
}

static void
pc98_check_msr(void* chan)
{
	int	msr, delta;
	int	s;
	register struct tty *tp;
	struct	com_s *com;
	int	mynor;
	int	unit;
	dev_t	dev;

	dev=(dev_t)chan;
	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	com = com_addr(unit);
	tp = com->tp;

	s = spltty();
	msr = pc98_get_modem_status(com);
	/* make change flag */
	delta = msr ^ com->pc98_prev_modem_status;
	if ( delta & TIOCM_CAR ) {
	    if ( com->modem_car_chg_timer ) {
		if ( -- com->modem_car_chg_timer )
		    msr ^= TIOCM_CAR;
	    } else {
		if ( com->modem_car_chg_timer = ( msr & TIOCM_CAR ) ?
			     DCD_ON_RECOGNITION : DCD_OFF_TOLERANCE )
		    msr ^= TIOCM_CAR;
	    }
	} else
	    com->modem_car_chg_timer = 0;
	delta = ( msr ^ com->pc98_prev_modem_status ) &
			(TIOCM_CAR|TIOCM_RI|TIOCM_DSR|TIOCM_CTS);
	com->pc98_prev_modem_status = msr;
	delta = ( com->pc98_modem_delta |= delta );
	splx(s);
	if ( com->modem_checking || (tp->t_state & (TS_ISOPEN)) ) {
		if ( delta ) {
			commint(dev);
		}
		timeout(pc98_check_msr, (caddr_t)dev,
					PC98_CHECK_MODEM_INTERVAL);
	} else {
		com->modem_checking = 0;
	}
}

static void
pc98_msrint_start(dev_t dev)
{
	struct	com_s *com;
	int	mynor;
	int	unit;
	int	s = spltty();

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	com = com_addr(unit);
	/* modem control line check routine envoke interval is 1/10 sec */
	if ( com->modem_checking == 0 ) {
		com->pc98_prev_modem_status = pc98_get_modem_status(com);
		com->pc98_modem_delta = 0;
		timeout(pc98_check_msr, (caddr_t)dev,
					PC98_CHECK_MODEM_INTERVAL);
		com->modem_checking = 1;
	}
	splx(s);
}

static void
pc98_disable_i8251_interrupt(struct com_s *com, int mod)
{
	/* disable interrupt */
	register int	tmp;

	mod |= ~(IEN_Tx|IEN_TxEMP|IEN_Rx);
	COM_INT_DISABLE
	tmp = inb( com->intr_ctrl_port ) & ~(IEN_Tx|IEN_TxEMP|IEN_Rx);
	outb( com->intr_ctrl_port, (com->intr_enable&=~mod) | tmp );
	COM_INT_ENABLE
}

static void
pc98_enable_i8251_interrupt(struct com_s *com, int mod)
{
	register int	tmp;

	COM_INT_DISABLE
	tmp = inb( com->intr_ctrl_port ) & ~(IEN_Tx|IEN_TxEMP|IEN_Rx);
	outb( com->intr_ctrl_port, (com->intr_enable|=mod) | tmp );
	COM_INT_ENABLE
}

static int
pc98_check_i8251_interrupt(struct com_s *com)
{
	return ( com->intr_enable & 0x07 );
}

static void
pc98_i8251_clear_cmd(struct com_s *com, int x)
{
	int	tmp;

	COM_INT_DISABLE
	tmp = com->pc98_prev_siocmd & ~(x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	COM_INT_ENABLE
}

static void
pc98_i8251_or_cmd(struct com_s *com, int x)
{
	int	tmp;

	COM_INT_DISABLE
	tmp = com->pc98_prev_siocmd | (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	COM_INT_ENABLE
}

static void
pc98_i8251_set_cmd(struct com_s *com, int x)
{
	int	tmp;

	COM_INT_DISABLE
	tmp = (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	COM_INT_ENABLE
}

static void
pc98_i8251_clear_or_cmd(struct com_s *com, int clr, int x)
{
	int	tmp;
	COM_INT_DISABLE
	tmp = com->pc98_prev_siocmd & ~(clr);
	tmp |= (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	COM_INT_ENABLE
}

static int
pc98_i8251_get_cmd(struct com_s *com)
{
	return com->pc98_prev_siocmd;
}

static int
pc98_i8251_get_mod(struct com_s *com)
{
	return com->pc98_prev_siomod;
}

static void
pc98_i8251_reset(struct com_s *com, int mode, int command)
{
	outb(com->cmd_port, 0);	/* dummy */
	DELAY(2);
	outb(com->cmd_port, 0);	/* dummy */
	DELAY(2);
	outb(com->cmd_port, 0);	/* dummy */
	DELAY(2);
	outb(com->cmd_port, CMD8251_RESET);	/* internal reset */
	DELAY(2);
	outb(com->cmd_port, mode );	/* mode register */
	com->pc98_prev_siomod = mode;
	DELAY(2);
	pc98_i8251_set_cmd( com, (command|CMD8251_ER) );
}

static void
pc98_check_sysclock(void)
{
	/* get system clock from port */
	if ( pc98_machine_type & M_8M ) {
	/* 8 MHz system & H98 */
		sysclock = 8;
	} else {
	/* 5 MHz system */
		sysclock = 5;
	}
}

static void
com_cflag_and_speed_set( struct com_s *com, int cflag, int speed)
{
	int	cfcr=0, count;
	int	previnterrupt;

	count = pc98_ttspeedtab( com, speed );
	if ( count < 0 ) return;

	previnterrupt = pc98_check_i8251_interrupt(com);
	pc98_disable_i8251_interrupt( com, IEN_Tx|IEN_TxEMP|IEN_Rx );

	switch ( cflag&CSIZE ) {
	  case CS5:
		cfcr = MOD8251_5BITS; break;
	  case CS6:
		cfcr = MOD8251_6BITS; break;
	  case CS7:
		cfcr = MOD8251_7BITS; break;
	  case CS8:
		cfcr = MOD8251_8BITS; break;
	}
	if ( cflag&PARENB ) {
	    if ( cflag&PARODD )
		cfcr |= MOD8251_PODD;
	    else
		cfcr |= MOD8251_PEVEN;
	} else
		cfcr |= MOD8251_PDISAB;

	if ( cflag&CSTOPB )
		cfcr |= MOD8251_STOP2;
	else
		cfcr |= MOD8251_STOP1;

	if ( count & 0x10000 )
		cfcr |= MOD8251_CLKX1;
	else
		cfcr |= MOD8251_CLKX16;

	if (epson_machine_id != 0x20) {	/* XXX */
	{
		int	tmp;
		while (!((tmp = inb(com->sts_port)) & STS8251_TxEMP))
			;
	}
	}
	/* set baud rate from ospeed */
	pc98_set_baud_rate( com, count );

	if ( cfcr != pc98_i8251_get_mod(com) )
		pc98_i8251_reset(com, cfcr, pc98_i8251_get_cmd(com) );

	pc98_enable_i8251_interrupt( com, previnterrupt );
}

static int
pc98_ttspeedtab(struct com_s *com, int speed)
{
	int	effect_sp, count=-1, mod;

	switch ( com->pc98_if_type ) {
	    case COM_IF_INTERNAL:
		/* for *1CLK asynchronous! mode		, TEFUTEFU */
		effect_sp = ttspeedtab( speed, pc98speedtab );
		if ( effect_sp < 0 )
			effect_sp = ttspeedtab( (speed-1), pc98speedtab );
		if ( effect_sp <= 0 )
			return effect_sp;
		mod = (sysclock == 5 ? 2457600 : 1996800);
		if ( effect_sp == speed )
			mod /= 16;
		count = mod / effect_sp;
		if ( count > 65535 )
			return(-1);
		if ( effect_sp >= 2400 )
			if ( !(sysclock != 5 && 
				(effect_sp == 19200 || effect_sp == 38400)) )
				if ( ( mod % effect_sp ) != 0 )
					return(-1);
		if ( effect_sp != speed )
			count |= 0x10000;
		break;
#ifdef COM_IF_PC9861K
	    case COM_IF_PC9861K:
		effect_sp = speed;
		count = 1;
		break;
#endif
#ifdef COM_IF_PIO9032B
	    case COM_IF_PIO9032B:
		if ( speed == 0 ) return 0;
		count = ttspeedtab( speed, comspeedtab_pio9032b );
		if ( count < 0 ) return count;
		effect_sp = speed;
		break;
#endif
#ifdef COM_IF_B98_01
	    case COM_IF_B98_01:
		effect_sp=speed;
		count = ttspeedtab( speed, comspeedtab_b98_01 );
		if ( count <= 3 )
			return -1;         /* invalid speed/count */
		if ( count <= 5 )
			count |= 0x10000;  /* x1 mode for 76800 and 153600 */
		else
			count -= 4;        /* x16 mode for slower */
		break;
#endif
	}
	return count;
}

static void
pc98_set_baud_rate( struct com_s *com, int count)
{
	int	s;

	switch ( com->pc98_if_type ) {
	    case COM_IF_INTERNAL:
		if ( count < 0 ) {
			printf( "[ Illegal count : %d ]", count );
			return;
		} else if ( count == 0) 
			return;
		/* set i8253 */
		s = splclock();
		outb( 0x77, 0xb6 );
		outb( 0x5f, 0);
		outb( 0x75, count & 0xff );
		outb( 0x5f, 0);
		outb( 0x75, (count >> 8) & 0xff );
		splx(s);
		break;
#if 0
#ifdef COM_IF_PC9861K
	    case COM_IF_PC9861K:
		break;
		/* ext. RS232C board: speed is determined by DIP switch */
#endif
#endif /* 0 */
#ifdef COM_IF_PIO9032B
	    case COM_IF_PIO9032B:
		outb( com_addr[unit], count & 0x07 );
		break;
#endif
#ifdef COM_IF_B98_01
	    case COM_IF_B98_01:
		outb( com->iobase,     count & 0x0f );
#ifdef B98_01_OLD
		/* some old board should be controlled in different way,
		   but this hasn't been tested yet.*/
		outb( com->iobase+2, ( count & 0x10000 ) ? 0xf0 : 0xf2 );
#endif
		break;
#endif
	}
}
static int
pc98_check_if_type( int iobase, struct siodev *iod)
{
	int	irr = 0, tmp = 0;
	int	ret = 0;
	static  short	irq_tab[2][8] = {
		{  3,  5,  6,  9, 10, 12, 13, -1},
		{  3, 10, 12, 13,  5,  6,  9, -1}
	};
	iod->irq = 0;
	switch ( iobase & 0xff ) {
		case IO_COM1:
			iod->if_type = COM_IF_INTERNAL;
			ret = 0; iod->irq = 4; break;
#ifdef COM_IF_PC9861K
		case IO_COM2:
			iod->if_type = COM_IF_PC9861K;
			ret = 1; irr = 0; tmp = 3; break;
		case IO_COM3:
			iod->if_type = COM_IF_PC9861K;
			ret = 2; irr = 1; tmp = 3; break;
#endif
#ifdef COM_IF_PIO9032B
	    case IO_COM_PIO9032B_2:
			iod->if_type = COM_IF_PIO9032B;
			ret = 1; irr = 0; tmp = 7; break;
	    case IO_COM_PIO9032B_3:
			iod->if_type = COM_IF_PIO9032B;
			ret = 2; irr = 1; tmp = 7; break;
#endif
#ifdef COM_IF_B98_01
	    case IO_COM_B98_01_2:
			iod->if_type = COM_IF_B98_01;
			ret = 1; irr = 0; tmp = 7;
			outb(iobase + 2, 0xf2);
			outb(iobase,     4);
			break;
	    case IO_COM_B98_01_3:
			iod->if_type = COM_IF_B98_01;
			ret = 2; irr = 1; tmp = 7;
			outb(iobase + 2, 0xf2);
			outb(iobase    , 4);
			break;
#endif
	    default:
			if((iobase & 0x0f0) == 0xd0){
				iod->if_type = MC16550;
				return 0;
			}
			return -1;
	}

	iod->cmd  = ( iobase & 0xff00 )|PC98SIO_cmd_port(ret);
	iod->sts  = ( iobase & 0xff00 )|PC98SIO_sts_port(ret);
	iod->mod  = ( iobase & 0xff00 )|PC98SIO_in_modem_port(ret);
	iod->ctrl = ( iobase & 0xff00 )|PC98SIO_intr_ctrl_port(ret);

	if ( iod->irq == 0 ) {
		tmp &= inb( iod->mod );
		iod->irq = irq_tab[irr][tmp];
		if ( iod->irq == -1 ) return -1;
	}
	return 0;
}
static int
pc98_set_ioport( struct com_s *com, int io_base )
{
	int	a, io, type;

	switch ( io_base & 0xff ) {
	    case IO_COM1: a = 0; io = 0; type = COM_IF_INTERNAL;
					 pc98_check_sysclock(); break;
#ifdef COM_IF_PC9861K
	    case IO_COM2: a = 1; io = 0; type = COM_IF_PC9861K; break;
	    case IO_COM3: a = 2; io = 0; type = COM_IF_PC9861K; break;
#endif /* COM_IF_PC9861K */
#ifdef COM_IF_PIO9032B
			/* PIO9032B : I/O address is changeable */
	    case IO_COM_PIO9032B_2:
			a = 1; io = io_base & 0xff00;
			type = COM_IF_PIO9032B; break;
	    case IO_COM_PIO9032B_3:
			a = 2; io = io_base & 0xff00;
			type = COM_IF_PIO9032B; break;
#endif /* COM_IF_PIO9032B */
#ifdef COM_IF_B98_01
	    case IO_COM_B98_01_2:
			a = 1; io = 0; type = COM_IF_B98_01; break;
	    case IO_COM_B98_01_3:
			a = 2; io = 0; type = COM_IF_B98_01; break;
#endif /* COM_IF_B98_01*/
	    default:	/* i/o address not match */
		return -1;
	}

	com->pc98_if_type	= type;
	com->data_port		= io | PC98SIO_data_port(a);
	com->cmd_port		= io | PC98SIO_cmd_port(a);
	com->sts_port		= io | PC98SIO_sts_port(a);
	com->in_modem_port	= io | PC98SIO_in_modem_port(a);
	com->intr_ctrl_port	= io | PC98SIO_intr_ctrl_port(a);
	return 0;
}
#endif /* PC98 defined */
