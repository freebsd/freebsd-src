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
 * $FreeBSD: src/sys/pc98/pc98/sio.c,v 1.124.2.6 2000/08/05 04:46:39 nyan Exp $
 *	from: @(#)com.c	7.5 (Berkeley) 5/16/91
 *	from: i386/isa sio.c,v 1.234
 */

#include "opt_comconsole.h"
#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_sio.h"
#include "card.h"
#include "pci.h"
#include "sio.h"

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
 *  device sio0 at nec? port 0x30  tty irq 4             #internal
 *  device sio1 at nec? port 0xd2  tty irq 5 flags 0x101 #mc1
 *  device sio2 at nec? port 0x8d2 tty flags 0x101       #mc2
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
 *   device  sio1 at nec? port 0x00d1 tty irq ?
 *   device  sio2 at nec? port 0x00d5 tty irq ?
 *      ... you can leave these lines `irq ?', irq will be autodetected.
 */
/*
 * Modified by Y.Takahashi of Kogakuin University.
 */
/*
 * modified for 8251(FIFO) by Seigo TANIMURA <tanimura@FreeBSD.org>
 */

#ifdef PC98
#define COM_IF_INTERNAL		0x00
#define COM_IF_PC9861K_1	0x01
#define COM_IF_PC9861K_2	0x02
#define COM_IF_IND_SS_1		0x03
#define COM_IF_IND_SS_2		0x04
#define COM_IF_PIO9032B_1	0x05
#define COM_IF_PIO9032B_2	0x06
#define COM_IF_B98_01_1		0x07
#define COM_IF_B98_01_2		0x08
#define COM_IF_END1		COM_IF_B98_01_2
#define COM_IF_RSA98		0x10	/* same as COM_IF_NS16550 */
#define COM_IF_NS16550		0x11
#define COM_IF_SECOND_CCU	0x12	/* same as COM_IF_NS16550 */
#define COM_IF_MC16550II	0x13
#define COM_IF_MCRS98		0x14	/* same as COM_IF_MC16550II */
#define COM_IF_RSB3000		0x15
#define COM_IF_RSB384		0x16
#define COM_IF_MODEM_CARD	0x17	/* same as COM_IF_NS16550 */
#define COM_IF_RSA98III		0x18
#define COM_IF_ESP98		0x19
#define COM_IF_END2		COM_IF_ESP98
#endif /* PC98 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/timepps.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <i386/isa/ic/i8251.h>
#else
#include <isa/isareg.h>
#endif
#include <isa/isavar.h>
#if NPCI > 0
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#endif
#include <machine/lock.h>

#include <machine/clock.h>
#include <machine/ipl.h>
#ifndef SMP
#include <machine/lock.h>
#endif
#include <machine/resource.h>

#include <isa/sioreg.h>

#ifdef COM_ESP
#include <i386/isa/ic/esp.h>
#endif
#include <i386/isa/ic/ns16550.h>
#ifdef PC98
#include <i386/isa/ic/rsa.h>
#endif

#ifndef __i386__
#define disable_intr()
#define enable_intr()
#endif

#ifdef SMP
#define disable_intr()	COM_DISABLE_INTR()
#define enable_intr()	COM_ENABLE_INTR()
#endif /* SMP */

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */

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
#define	COM_ISMULTIPORT(flags)	((flags) & 0x01)
#define	COM_MPMASTER(flags)	(((flags) >> 8) & 0x0ff)
#define	COM_NOTAST4(flags)	((flags) & 0x04)
#endif /* COM_MULTIPORT */

#define	COM_CONSOLE(flags)	((flags) & 0x10)
#define	COM_FORCECONSOLE(flags)	((flags) & 0x20)
#define	COM_LLCONSOLE(flags)	((flags) & 0x40)
#define	COM_DEBUGGER(flags)	((flags) & 0x80)
#define	COM_LOSESOUTINTS(flags)	((flags) & 0x08)
#define	COM_NOFIFO(flags)		((flags) & 0x02)
#define COM_ST16650A(flags)	((flags) & 0x20000)
#define COM_C_NOPROBE		(0x40000)
#define COM_NOPROBE(flags)	((flags) & COM_C_NOPROBE)
#define COM_C_IIR_TXRDYBUG	(0x80000)
#define COM_IIR_TXRDYBUG(flags)	((flags) & COM_C_IIR_TXRDYBUG)
#define	COM_FIFOSIZE(flags)	(((flags) & 0xff000000) >> 24)

#ifdef PC98
#define	com_emr		com_msr	/* Extension mode register for RSB-2000/3000 */
#endif
#define	com_scr		7	/* scratch register for 16450-16550 (R/W) */

#define	sio_getreg(com, off) \
	(bus_space_read_1((com)->bst, (com)->bsh, (off)))
#define	sio_setreg(com, off, value) \
	(bus_space_write_1((com)->bst, (com)->bsh, (off), (value)))

/*
 * com state bits.
 * (CS_BUSY | CS_TTGO) and (CS_BUSY | CS_TTGO | CS_ODEVREADY) must be higher
 * than the other bits so that they can be tested as a group without masking
 * off the low bits.
 *
 * The following com and tty flags correspond closely:
 *	CS_BUSY		= TS_BUSY (maintained by comstart(), siopoll() and
 *				   comstop())
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
	u_int	flags;		/* Copy isa device flags */
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
	u_char	*ibufold;	/* old input buffer, to be freed */
	u_char	*ihighwater;	/* threshold in input buffer */
	u_char	*iptr;		/* next free spot in input buffer */
	int	ibufsize;	/* size of ibuf (not include error bytes) */
	int	ierroff;	/* offset of error bytes in ibuf */

	struct lbq	obufq;	/* head of queue of output buffers */
	struct lbq	obufs[2];	/* output buffers */

	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;

#ifdef PC98
	Port_t	cmd_port;
	Port_t	sts_port;
	Port_t	in_modem_port;
	Port_t	intr_ctrl_port;
	Port_t	rsabase;	/* iobase address of a I/O-DATA RSA board */
	int	intr_enable;
	int	pc98_prev_modem_status;
	int	pc98_modem_delta;
	int	modem_car_chg_timer;
	int	pc98_prev_siocmd;
	int	pc98_prev_siomod;
	int	modem_checking;
	int	pc98_if_type;

	bool_t	pc98_8251fifo;
	bool_t	pc98_8251fifo_enable;
#endif /* PC98 */
	Port_t	data_port;	/* i/o ports */
#ifdef COM_ESP
	Port_t	esp_port;
#endif
	Port_t	int_id_port;
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
	struct	pps_state pps;

	u_long	bytes_in;	/* statistics */
	u_long	bytes_out;
	u_int	delta_error_counts[CE_NTYPES];
	u_long	error_counts[CE_NTYPES];

	struct resource *irqres;
	struct resource *ioportres;
	void *cookie;

	/*
	 * Data area for output buffers.  Someday we should build the output
	 * buffer queue without copying data.
	 */
#ifdef PC98
	int	obufsize;
 	u_char	*obuf1;
 	u_char	*obuf2;
#else
	u_char	obuf1[256];
	u_char	obuf2[256];
#endif
};

#ifdef COM_ESP
static	int	espattach	__P((struct com_s *com, Port_t esp_port));
#endif
static	int	sioattach	__P((device_t dev, int rid));
static	int	sio_isa_attach	__P((device_t dev));

static	timeout_t siobusycheck;
static	timeout_t siodtrwakeup;
static	void	comhardclose	__P((struct com_s *com));
static	void	sioinput	__P((struct com_s *com));
static	void	siointr1	__P((struct com_s *com));
static	void	siointr		__P((void *arg));
static	int	commctl		__P((struct com_s *com, int bits, int how));
static	int	comparam	__P((struct tty *tp, struct termios *t));
static	swihand_t siopoll;
static	int	sioprobe	__P((device_t dev, int xrid));
static	int	sio_isa_probe	__P((device_t dev));
static	void	siosettimeout	__P((void));
static	int	siosetwater	__P((struct com_s *com, speed_t speed));
static	void	comstart	__P((struct tty *tp));
static	void	comstop		__P((struct tty *tp, int rw));
static	timeout_t comwakeup;
static	void	disc_optim	__P((struct tty	*tp, struct termios *t,
				     struct com_s *com));

#if NCARD > 0
static	int	sio_pccard_attach __P((device_t dev));
static	int	sio_pccard_detach __P((device_t dev));
static	int	sio_pccard_probe __P((device_t dev));
#endif /* NCARD > 0 */

#if NPCI > 0
static	int	sio_pci_attach __P((device_t dev));
static	void	sio_pci_kludge_unit __P((device_t dev));
static	int	sio_pci_probe __P((device_t dev));
#endif /* NPCI > 0 */

static char driver_name[] = "sio";

/* table and macro for fast conversion from a unit number to its com struct */
static	devclass_t	sio_devclass;
#define	com_addr(unit)	((struct com_s *) \
			 devclass_get_softc(sio_devclass, unit))

static device_method_t sio_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sio_isa_probe),
	DEVMETHOD(device_attach,	sio_isa_attach),

	{ 0, 0 }
};

static driver_t sio_isa_driver = {
	driver_name,
	sio_isa_methods,
	sizeof(struct com_s),
};

#if NCARD > 0
static device_method_t sio_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sio_pccard_probe),
	DEVMETHOD(device_attach,	sio_pccard_attach),
	DEVMETHOD(device_detach,	sio_pccard_detach),

	{ 0, 0 }
};

static driver_t sio_pccard_driver = {
	driver_name,
	sio_pccard_methods,
	sizeof(struct com_s),
};
#endif /* NCARD > 0 */

#if NPCI > 0
static device_method_t sio_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sio_pci_probe),
	DEVMETHOD(device_attach,	sio_pci_attach),

	{ 0, 0 }
};

static driver_t sio_pci_driver = {
	driver_name,
	sio_pci_methods,
	sizeof(struct com_s),
};
#endif /* NPCI > 0 */

static	d_open_t	sioopen;
static	d_close_t	sioclose;
static	d_read_t	sioread;
static	d_write_t	siowrite;
static	d_ioctl_t	sioioctl;

#define	CDEV_MAJOR	28
static struct cdevsw sio_cdevsw = {
	/* open */	sioopen,
	/* close */	sioclose,
	/* read */	sioread,
	/* write */	siowrite,
	/* ioctl */	sioioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	driver_name,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
	/* bmaj */	-1
};

int	comconsole = -1;
static	volatile speed_t	comdefaultrate = CONSPEED;
#ifdef __alpha__
static	volatile speed_t	gdbdefaultrate = CONSPEED;
#endif
static	u_int	com_events;	/* input chars + weighted output completions */
static	Port_t	siocniobase;
static	int	siocnunit;
static	Port_t	siogdbiobase;
static	int	siogdbunit = -1;
static	bool_t	sio_registered;
static	int	sio_timeout;
static	int	sio_timeouts_until_log;
static	struct	callout_handle sio_timeout_handle
    = CALLOUT_HANDLE_INITIALIZER(&sio_timeout_handle);
static	int	sio_numunits;

#ifdef PC98
struct	siodev	{
	short	if_type;
	short	irq;
	Port_t	cmd, sts, ctrl, mod;
};
static	int	sysclock;

#define	COM_INT_DISABLE		{int previpri; previpri=spltty();
#define	COM_INT_ENABLE		splx(previpri);}
#define IEN_TxFLAG		IEN_Tx

#define COM_CARRIER_DETECT_EMULATE	0
#define	PC98_CHECK_MODEM_INTERVAL	(hz/10)
#define DCD_OFF_TOLERANCE		2
#define DCD_ON_RECOGNITION		2
#define GET_IFTYPE(flags)		((flags >> 24) & 0x1f)
#define SET_IFTYPE(type)		(type << 24)
#define IS_8251(if_type)		(!(if_type & 0x10))
#define COM1_EXT_CLOCK			0x40000

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
static	int	pc98_check_if_type	__P((device_t dev, struct siodev *iod));
static	int	pc98_check_8251vfast	__P((void));
static	int	pc98_check_8251fifo	__P((void));
static	void	pc98_check_sysclock	__P((void));
static	void	pc98_set_ioport		__P((struct com_s *com));

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

static struct speedtab pc98speedtab[] = {	/* internal RS232C interface */
	{ 0,		0, },
	{ 50,		50, },
	{ 75,		75, },
	{ 150,		150, },
	{ 200,		200, },
	{ 300,		300, },
	{ 600,		600, },
	{ 1200,		1200, },
	{ 2400,		2400, },
	{ 4800,		4800, },
	{ 9600,		9600, },
	{ 19200,	19200, },
	{ 38400,	38400, },
	{ 51200,	51200, },
	{ 76800,	76800, },
	{ 20800,	20800, },
	{ 31200,	31200, },
	{ 41600,	41600, },
	{ 62400,	62400, },
	{ -1,		-1 }
};
static struct speedtab pc98fast_speedtab[] = {
	{ 9600,		0x80 | COMBRD(9600), },
	{ 19200,	0x80 | COMBRD(19200), },
	{ 38400,	0x80 | COMBRD(38400), },
	{ 57600,	0x80 | COMBRD(57600), },
	{ 115200,	0x80 | COMBRD(115200), },
	{ -1,		-1 }
};
static struct speedtab comspeedtab_pio9032b[] = {
	{ 300,		6, },
	{ 600,		5, },
	{ 1200,		4, },
	{ 2400,		3, },
	{ 4800,		2, },
	{ 9600,		1, },
	{ 19200,	0, },
	{ 38400,	7, },
	{ -1,		-1 }
};
static struct speedtab comspeedtab_b98_01[] = {
	{ 75,		11, },
	{ 150,		10, },
	{ 300,		9, },
	{ 600,		8, },
	{ 1200,		7, },
	{ 2400,		6, },
	{ 4800,		5, },
	{ 9600,		4, },
	{ 19200,	3, },
	{ 38400,	2, },
	{ 76800,	1, },
	{ 153600,	0, },
	{ -1,		-1 }
};
static struct speedtab comspeedtab_mc16550[] = {
	{ 300,		1536, },
	{ 600,		768, },
	{ 1200,		384, },
	{ 2400,		192, },
	{ 4800,		96, },
	{ 9600,		48, },
	{ 19200,	24, },
	{ 38400,	12, },
	{ 57600,	8, },
	{ 115200,	4, },
	{ 153600,	3, },
	{ 230400,	2, },
	{ 460800,	1, },
	{ -1,		-1 }
};
static struct speedtab comspeedtab_rsb384[] = {
	{ 300,		3840, },
	{ 600,		1920, },
	{ 1200,		960, },
	{ 2400,		480, },
	{ 4800,		240, },
	{ 9600,		120, },
	{ 19200,	60, },
	{ 38400,	30, },
	{ 57600,	20, },
	{ 115200,	10, },
	{ 128000,	9, },
	{ 144000,	8, },
	{ 192000,	6, },
	{ 230400,	5, },
	{ 288000,	4, },
	{ 384000,	3, },
	{ 576000,	2, },
	{ 1152000,	1, },
	{ -1,		-1 }
};
static  struct speedtab comspeedtab_rsa[] = {
        { 0,		0 },
	{ 50,		COMBRD_RSA(50) },
	{ 75,		COMBRD_RSA(75) },
	{ 110,		COMBRD_RSA(110) },
	{ 134,		COMBRD_RSA(134) },
	{ 150,		COMBRD_RSA(150) },
	{ 200,		COMBRD_RSA(200) },
	{ 300,		COMBRD_RSA(300) },
	{ 600,		COMBRD_RSA(600) },
	{ 1200,		COMBRD_RSA(1200) },
	{ 1800,		COMBRD_RSA(1800) },
	{ 2400,		COMBRD_RSA(2400) },
	{ 4800,		COMBRD_RSA(4800) },
	{ 9600,		COMBRD_RSA(9600) },
	{ 19200,	COMBRD_RSA(19200) },
	{ 38400,	COMBRD_RSA(38400) },
	{ 57600,	COMBRD_RSA(57600) },
	{ 115200,	COMBRD_RSA(115200) },
	{ 230400,	COMBRD_RSA(230400) },
	{ 460800,	COMBRD_RSA(460800) },
	{ 921600,	COMBRD_RSA(921600) },
	{ -1,           -1 }
};
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

#ifdef PC98
struct {
	char	*name;
	short	port_table[7];
	short	irr_mask;
	struct speedtab	*speedtab;
	short	check_irq;
} if_8251_type[] = {
	/* COM_IF_INTERNAL */
	{ " (internal)", {0x30, 0x32, 0x32, 0x33, 0x35, -1, -1},
	     -1, pc98speedtab, 1 },
	/* COM_IF_PC9861K_1 */
	{ " (PC9861K)", {0xb1, 0xb3, 0xb3, 0xb0, 0xb0, -1, -1},
	     3, NULL, 1 },
	/* COM_IF_PC9861K_2 */
	{ " (PC9861K)", {0xb9, 0xbb, 0xbb, 0xb2, 0xb2, -1, -1},
	      3, NULL, 1 },
	/* COM_IF_IND_SS_1 */
	{ " (IND-SS)", {0xb1, 0xb3, 0xb3, 0xb0, 0xb0, 0xb3, -1},
	     3, comspeedtab_mc16550, 1 },
	/* COM_IF_IND_SS_2 */
	{ " (IND-SS)", {0xb9, 0xbb, 0xbb, 0xb2, 0xb2, 0xbb, -1},
	     3, comspeedtab_mc16550, 1 },
	/* COM_IF_PIO9032B_1 */
	{ " (PIO9032B)", {0xb1, 0xb3, 0xb3, 0xb0, 0xb0, 0xb8, -1},
	      7, comspeedtab_pio9032b, 1 },
	/* COM_IF_PIO9032B_2 */
	{ " (PIO9032B)", {0xb9, 0xbb, 0xbb, 0xb2, 0xb2, 0xba, -1},
	      7, comspeedtab_pio9032b, 1 },
	/* COM_IF_B98_01_1 */
	{ " (B98-01)", {0xb1, 0xb3, 0xb3, 0xb0, 0xb0, 0xd1, 0xd3},
	      7, comspeedtab_b98_01, 0 },
	/* COM_IF_B98_01_2 */
	{ " (B98-01)", {0xb9, 0xbb, 0xbb, 0xb2, 0xb2, 0xd5, 0xd7},
	     7, comspeedtab_b98_01, 0 },
};
#define	PC98SIO_data_port(type)		(if_8251_type[type].port_table[0])
#define	PC98SIO_cmd_port(type)		(if_8251_type[type].port_table[1])
#define	PC98SIO_sts_port(type)		(if_8251_type[type].port_table[2])
#define	PC98SIO_in_modem_port(type)	(if_8251_type[type].port_table[3])
#define	PC98SIO_intr_ctrl_port(type)	(if_8251_type[type].port_table[4])
#define	PC98SIO_baud_rate_port(type)	(if_8251_type[type].port_table[5])
#define	PC98SIO_func_port(type)		(if_8251_type[type].port_table[6])

#define	I8251F_data		0x130
#define	I8251F_lsr		0x132
#define	I8251F_msr		0x134
#define	I8251F_iir		0x136
#define	I8251F_fcr		0x138
#define	I8251F_div		0x13a


static bus_addr_t port_table_0[] =
	{0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007};
static bus_addr_t port_table_1[] =
	{0x000, 0x002, 0x004, 0x006, 0x008, 0x00a, 0x00c, 0x00e};
static bus_addr_t port_table_8[] =
	{0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700};
static bus_addr_t port_table_rsa[] = {
	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007
};

struct {
	char		*name;
	short		irr_read;
	short		irr_write;
	bus_addr_t	*iat;
	bus_size_t	iatsz;
	struct speedtab	*speedtab;
} if_16550a_type[] = {
	/* COM_IF_RSA98 */
	{" (RSA-98)", -1, -1, port_table_0, IO_COMSIZE, comspeedtab},
	/* COM_IF_NS16550 */
	{"", -1, -1, port_table_0, IO_COMSIZE, comspeedtab},
	/* COM_IF_SECOND_CCU */
	{"", -1, -1, port_table_0, IO_COMSIZE, comspeedtab},
	/* COM_IF_MC16550II */
	{" (MC16550II)", -1, 0x1000, port_table_8, IO_COMSIZE,
	 comspeedtab_mc16550},
	/* COM_IF_MCRS98 */
	{" (MC-RS98)", -1, 0x1000, port_table_8, IO_COMSIZE,
	 comspeedtab_mc16550},
	/* COM_IF_RSB3000 */
	{" (RSB-3000)", 0xbf, -1, port_table_1, IO_COMSIZE,
	 comspeedtab_rsb384},
	/* COM_IF_RSB384 */
	{" (RSB-384)", 0xbf, -1, port_table_1, IO_COMSIZE, comspeedtab_rsb384},
	/* COM_IF_MODEM_CARD */
	{"", -1, -1, port_table_0, IO_COMSIZE, comspeedtab},
	/* COM_IF_RSA98III */
	{" (RSA-98III)", -1, -1, port_table_rsa, 16, comspeedtab_rsa},
	/* COM_IF_ESP98 */
	{" (ESP98)", -1, -1, port_table_1, IO_COMSIZE, comspeedtab_mc16550},
};
#endif /* PC98 */

#ifdef COM_ESP
#ifdef PC98

/* XXX configure this properly. */
static  Port_t  likely_com_ports[] = { 0, 0xb0, 0xb1, 0 };
static  Port_t  likely_esp_ports[] = { 0xc0d0, 0 };

#define	ESP98_CMD1	(ESP_CMD1 * 0x100)
#define	ESP98_CMD2	(ESP_CMD2 * 0x100)
#define	ESP98_STATUS1	(ESP_STATUS1 * 0x100)
#define	ESP98_STATUS2	(ESP_STATUS2 * 0x100)

#else /* PC98 */

/* XXX configure this properly. */
static	Port_t	likely_com_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, };
static	Port_t	likely_esp_ports[] = { 0x140, 0x180, 0x280, 0 };

#endif /* PC98 */
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
sysctl_machdep_comdefaultrate(SYSCTL_HANDLER_ARGS)
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
	if (com == NULL)
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

#define SET_FLAG(dev, bit) device_set_flags(dev, device_get_flags(dev) | (bit))
#define CLR_FLAG(dev, bit) device_set_flags(dev, device_get_flags(dev) & ~(bit))

#if NCARD > 0
static int
sio_pccard_probe(dev)
	device_t	dev;
{
	/* Do not probe IRQ - pccard doesn't turn on the interrupt line */
	/* until bus_setup_intr */
	SET_FLAG(dev, COM_C_NOPROBE);

	return (sioprobe(dev, 0));
}

static int
sio_pccard_attach(dev)
	device_t	dev;
{
	return (sioattach(dev, 0));
}

/*
 *	sio_detach - unload the driver and clear the table.
 *	XXX TODO:
 *	This is usually called when the card is ejected, but
 *	can be caused by a modunload of a controller driver.
 *	The idea is to reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
static int
sio_pccard_detach(dev)
	device_t	dev;
{
	struct com_s	*com;

	com = (struct com_s *) device_get_softc(dev);
	if (com == NULL) {
		device_printf(dev, "NULL com in siounload\n");
		return (0);
	}
	com->gone = 1;
	if (com->irqres) {
		bus_teardown_intr(dev, com->irqres, com->cookie);
		bus_release_resource(dev, SYS_RES_IRQ, 0, com->irqres);
	}
	if (com->ioportres)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, com->ioportres);
	if (com->tp && (com->tp->t_state & TS_ISOPEN)) {
		device_printf(dev, "still open, forcing close\n");
		com->tp->t_gen++;
		ttyclose(com->tp);
		ttwakeup(com->tp);
		ttwwakeup(com->tp);
	} else {
		if (com->ibuf != NULL)
			free(com->ibuf, M_DEVBUF);
	}
	device_printf(dev, "unloaded\n");
	return (0);
}
#endif /* NCARD > 0 */

#if NPCI > 0
struct pci_ids {
	u_int32_t	type;
	const char	*desc;
	int		rid;
};

static struct pci_ids pci_ids[] = {
	{ 0x100812b9, "3COM PCI FaxModem", 0x10 },
	{ 0x048011c1, "ActionTec 56k FAX PCI Modem", 0x14 },
	{ 0x00000000, NULL, 0 }
};

static int
sio_pci_attach(dev)
	device_t	dev;
{
	u_int32_t	type;
	struct pci_ids	*id;

	type = pci_get_devid(dev);
	id = pci_ids;
	while (id->type && id->type != type)
		id++;
	if (id->desc == NULL)
		return (ENXIO);
	sio_pci_kludge_unit(dev);
	return (sioattach(dev, id->rid));
}

/*
 * Don't cut and paste this to other drivers.  It is a horrible kludge
 * which will fail to work and also be unnecessary in future versions.
 */
static void
sio_pci_kludge_unit(dev)
	device_t dev;
{
	devclass_t	dc;
	int		err;
	int		start;
	int		unit;

	unit = 0;
	start = 0;
	while (resource_int_value("sio", unit, "port", &start) == 0 && 
	    start > 0)
		unit++;
	if (device_get_unit(dev) < unit) {
		dc = device_get_devclass(dev);
		while (devclass_get_device(dc, unit))
			unit++;
		device_printf(dev, "moving to sio%d\n", unit);
		err = device_set_unit(dev, unit);	/* EVIL DO NOT COPY */
		if (err)
			device_printf(dev, "error moving device %d\n", err);
	}
}

static int
sio_pci_probe(dev)
	device_t	dev;
{
	u_int32_t	type;
	struct pci_ids	*id;

	type = pci_get_devid(dev);
	id = pci_ids;
	while (id->type && id->type != type)
		id++;
	if (id->desc == NULL)
		return (ENXIO);
	device_set_desc(dev, id->desc);
	return (sioprobe(dev, id->rid));
}
#endif /* NPCI > 0 */

static struct isa_pnp_id sio_ids[] = {
	{0x0005d041, "Standard PC COM port"},	/* PNP0500 */
	{0x0105d041, "16550A-compatible COM port"},	/* PNP0501 */
	{0x0205d041, "Multiport serial device (non-intelligent 16550)"}, /* PNP0502 */
	{0x1005d041, "Generic IRDA-compatible device"},	/* PNP0510 */
	{0x1105d041, "Generic IRDA-compatible device"},	/* PNP0511 */
	/* Devices that do not have a compatid */
	{0x7602a904, NULL},	/* AEI0276 - 56K v.90 Fax Modem (LKT) */
	{0x00007905, NULL},	/* AKY0000 - 56K Plug&Play Modem */
	{0x01405407, NULL},	/* AZT4001 - AZT3000 PnP SOUND DEVICE, MODEM */
	{0x56039008, NULL},	/* BDP0356 - Best Data 56x2 */
	{0x36339008, NULL},	/* BDP3336 - Best Data Prods. 336F */
	{0x0014490a, NULL},	/* BRI1400 - Boca 33.6 PnP */
	{0x0015490a, NULL},	/* BRI1500 - Internal Fax Data */
	{0x0034490a, NULL},	/* BRI3400 - Internal ACF Modem */
	{0x00b4490a, NULL},	/* BRIB400 - Boca 56k PnP */
	{0x0030320d, NULL},	/* CIR3000 - Cirrus Logic V43 */
	{0x0100440e, NULL},	/* CRD0001 - Cardinal MVP288IV ? */
	{0x1200c31e, NULL},	/* GVC0012 - VF1128HV-R9 (win modem?) */
	{0x0303c31e, NULL},	/* GVC0303 - MaxTech 33.6 PnP D/F/V */
	{0x0505c31e, NULL},	/* GVC0505 - GVC 56k Faxmodem */
	{0x0050c31e, NULL},	/* GVC5000 - some GVC modem */
	{0x3800f91e, NULL},	/* GWY0038 - Telepath with v.90 */
	{0x9062f91e, NULL},	/* GWY6290 - Telepath with x2 Technology */
	{0x0000f435, NULL},	/* MOT0000 - Motorola ModemSURFR 33.6 Intern */
	{0x5015f435, NULL},	/* MOT1550 - Motorola ModemSURFR 56K Modem */
	{0xf015f435, NULL},	/* MOT15F0 - Motorola VoiceSURFR 56K Modem */
	{0x6045f435, NULL},	/* MOT4560 - Motorola ? */
	{0x61e7a338, NULL},	/* NECE761 - 33.6Modem */
	{0x39804f3f, NULL},	/* OZO8039 - Zoom 56k flex */
	{0x3024a341, NULL},	/* PMC2430 - Pace 56 Voice Internal Modem */
	{0x1000eb49, NULL},	/* ROK0010 - Rockwell ? */
	{0x5002734a, NULL},	/* RSS0250 - 5614Jx3(G) Internal Modem */
	{0xc100ad4d, NULL},	/* SMM00C1 - Leopard 56k PnP */
	{0x9012b04e, NULL},	/* SUP1290 - Supra ? */
	{0x1013b04e, NULL},	/* SUP1310 - SupraExpress 336i PnP */
	{0x8013b04e, NULL},	/* SUP1380 - SupraExpress 288i PnP Voice */
	{0x8113b04e, NULL},	/* SUP1381 - SupraExpress 336i PnP Voice */
	{0x5016b04e, NULL},	/* SUP1650 - Supra 336i Sp Intl */
	{0x7420b04e, NULL},	/* SUP2070 - Supra ? */
	{0x8020b04e, NULL},	/* SUP2080 - Supra ? */
	{0x8420b04e, NULL},	/* SUP2084 - SupraExpress 56i PnP */
	{0x7121b04e, NULL},	/* SUP2171 - SupraExpress 56i Sp? */
	{0x8024b04e, NULL},	/* SUP2480 - Supra ? */
	{0x01007256, NULL},	/* USR0001 - U.S. Robotics Inc., Sportster W */
	{0x02007256, NULL},	/* USR0002 - U.S. Robotics Inc. Sportster 33. */
	{0x04007256, NULL},	/* USR0004 - USR Sportster 14.4k */
	{0x06007256, NULL},	/* USR0006 - USR Sportster 33.6k */
	{0x11007256, NULL},	/* USR0011 - USR ? */
	{0x01017256, NULL},	/* USR0101 - USR ? */
	{0x30207256, NULL},	/* USR2030 - U.S.Robotics Inc. Sportster 560 */
	{0x50207256, NULL},	/* USR2050 - U.S.Robotics Inc. Sportster 33. */
	{0x70207256, NULL},	/* USR2070 - U.S.Robotics Inc. Sportster 560 */
	{0x30307256, NULL},	/* USR3030 - U.S. Robotics 56K FAX INT */
	{0x31307256, NULL},	/* USR3031 - U.S. Robotics 56K FAX INT */
	{0x50307256, NULL},	/* USR3050 - U.S. Robotics 56K FAX INT */
	{0x70307256, NULL},	/* USR3070 - U.S. Robotics 56K Voice INT */
	{0x90307256, NULL},	/* USR3090 - USR ? */
	{0x90917256, NULL},	/* USR9190 - USR 56k Voice INT */
	{0x0300695c, NULL},	/* WCI0003 - Fax/Voice/Modem/Speakphone/Asvd */
	{0x61f7896a, NULL},	/* ZTIF761 - Zoom ComStar 33.6 */
#ifdef PC98
	{0x0100e4a5, "RSA-98III"},
#endif
	{0}
};



static int
sio_isa_probe(dev)
	device_t	dev;
{
#ifdef PC98
	int	logical_id;
#endif
	/* Check isapnp ids */
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, sio_ids) == ENXIO)
		return (ENXIO);
#ifdef PC98
	logical_id = isa_get_logicalid(dev);
	if (logical_id == 0x0100e4a5)		/* RSA-98III */
		device_set_flags(dev, SET_IFTYPE(COM_IF_RSA98III));
#endif
	return (sioprobe(dev, 0));
}

static int
sioprobe(dev, xrid)
	device_t	dev;
	int		xrid;
{
#if 0
	static bool_t	already_init;
	device_t	xdev;
#endif
	struct com_s	*com;
	bool_t		failures[10];
	int		fn;
	device_t	idev;
	Port_t		iobase;
	intrmask_t	irqmap[4];
	intrmask_t	irqs;
	u_char		mcr_image;
	int		result;
	u_long		xirq;
	u_int		flags = device_get_flags(dev);
	int		rid;
	struct resource *port;
#ifdef PC98
	int		tmp;
	struct siodev	iod;
#endif

#ifdef PC98
	iod.if_type = GET_IFTYPE(flags);
	if ((iod.if_type < 0 || iod.if_type > COM_IF_END1) &&
	    (iod.if_type < 0x10 || iod.if_type > COM_IF_END2))
		return ENXIO;
#endif

	rid = xrid;
#ifdef PC98
	if (IS_8251(iod.if_type)) {
		port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					  0, ~0, 1, RF_ACTIVE);
	} else if (iod.if_type == COM_IF_RSA98III ||
		   isa_get_vendorid(dev)) {
		port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
			if_16550a_type[iod.if_type & 0x0f].iatsz, RF_ACTIVE);
	} else {
		port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
			if_16550a_type[iod.if_type & 0x0f].iat,
			if_16550a_type[iod.if_type & 0x0f].iatsz, RF_ACTIVE);
	}
#else
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, IO_COMSIZE, RF_ACTIVE);
#endif
	if (!port)
		return (ENXIO);
#ifdef PC98
	if (!IS_8251(iod.if_type)) {
		if (isa_load_resourcev(port,
			if_16550a_type[iod.if_type & 0x0f].iat,
			if_16550a_type[iod.if_type & 0x0f].iatsz) != 0) {
		    bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		    return ENXIO;
		}
	}
#endif

	com = device_get_softc(dev);
	com->bst = rman_get_bustag(port);
	com->bsh = rman_get_bushandle(port);

#if 0
	/*
	 * XXX this is broken - when we are first called, there are no
	 * previously configured IO ports.  We could hard code
	 * 0x3f8, 0x2f8, 0x3e8, 0x2e8 etc but that's probably worse.
	 * This code has been doing nothing since the conversion since
	 * "count" is zero the first time around.
	 */
	if (!already_init) {
		/*
		 * Turn off MCR_IENABLE for all likely serial ports.  An unused
		 * port with its MCR_IENABLE gate open will inhibit interrupts
		 * from any used port that shares the interrupt vector.
		 * XXX the gate enable is elsewhere for some multiports.
		 */
		device_t *devs;
		int count, i, xioport;
#ifdef PC98
		int xiftype;
#endif

		devclass_get_devices(sio_devclass, &devs, &count);
#ifdef PC98
		for (i = 0; i < count; i++) {
			xdev = devs[i];
			xioport = bus_get_resource_start(xdev, SYS_RES_IOPORT, 0);
			xiftype = GET_IFTYPE(device_get_flags(xdev));
			if (device_is_enabled(xdev) && xioport > 0) {
			    if (IS_8251(xiftype))
				outb((xioport & 0xff00) | PC98SIO_cmd_port(xiftype & 0x0f), 0xf2);
			    else
				outb(xioport + if_16550a_type[xiftype & 0x0f].iat[com_mcr], 0);
			}
		}
#else
		for (i = 0; i < count; i++) {
			xdev = devs[i];
			if (device_is_enabled(xdev) &&
			    bus_get_resource(xdev, SYS_RES_IOPORT, 0, &xioport,
					     NULL) == 0)
				outb(xioport + com_mcr, 0);
		}
#endif
		free(devs, M_TEMP);
		already_init = TRUE;
	}
#endif

	if (COM_LLCONSOLE(flags)) {
		printf("sio%d: reserved for low-level i/o\n",
		       device_get_unit(dev));
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return (ENXIO);
	}

#ifdef PC98
	DELAY(10);

	/*
	 * If the port is i8251 UART (internal, B98_01)
	 */
	if (pc98_check_if_type(dev, &iod) == -1) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return (ENXIO);
	}
	if (iod.irq > 0)
		bus_set_resource(dev, SYS_RES_IRQ, 0, iod.irq, 1);
	if (IS_8251(iod.if_type)) {
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
		    result = (ENXIO);
		}
		if (if_8251_type[iod.if_type & 0x0f].check_irq) {
		    COM_INT_DISABLE
		    tmp = ( inb( iod.ctrl ) & ~(IEN_Rx|IEN_TxEMP|IEN_Tx));
		    outb( iod.ctrl, tmp|IEN_TxEMP );
		    DELAY(10);
		    result = isa_irq_pending() ? 0 : ENXIO;
		    outb( iod.ctrl, tmp );
		    COM_INT_ENABLE
		} else {
		    /*
		     * B98_01 doesn't activate TxEMP interrupt line
		     * when being reset, so we can't check irq pending.
		     */
		    result = 0;
		}
		if (epson_machine_id==0x20) {	/* XXX */
		    result = 0;
		}
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return result;
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
	if (COM_ISMULTIPORT(flags)) {
#ifndef PC98
		Port_t xiobase;
		u_long io;
#endif

		idev = devclass_get_device(sio_devclass, COM_MPMASTER(flags));
		if (idev == NULL) {
			printf("sio%d: master device %d not configured\n",
			       device_get_unit(dev), COM_MPMASTER(flags));
			idev = dev;
		}
#ifndef PC98
		if (!COM_NOTAST4(flags)) {
			if (bus_get_resource(idev, SYS_RES_IOPORT, 0, &io,
					     NULL) == 0) {
				xiobase = io;
				if (bus_get_resource(idev, SYS_RES_IRQ, 0,
				    NULL, NULL) == 0)
					outb(xiobase + com_scr, 0x80);
				else
					outb(xiobase + com_scr, 0);
			}
			mcr_image = 0;
		}
#endif
	}
#endif /* COM_MULTIPORT */
	if (bus_get_resource(idev, SYS_RES_IRQ, 0, NULL, NULL) != 0)
		mcr_image = 0;

	bzero(failures, sizeof failures);
	iobase = rman_get_start(port);

#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III) {
		mcr_image = 0;

		outb(iobase + rsa_msr,   0x04);
		outb(iobase + rsa_frr,   0x00);
		if ((inb(iobase + rsa_srr) & 0x36) != 0x36) {
			bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
			return (ENXIO);
		}
		outb(iobase + rsa_ier,   0x00);
		outb(iobase + rsa_frr,   0x00);
		outb(iobase + rsa_tivsr, 0x00);
		outb(iobase + rsa_tcr,   0x00);
	}

	tmp = if_16550a_type[iod.if_type & 0x0f].irr_write;
	if (tmp != -1) {
	    /* MC16550II */
	    int	irqout;
	    switch (isa_get_irq(idev)) {
	    case 3: irqout = 4; break;
	    case 5: irqout = 5; break;
	    case 6: irqout = 6; break;
	    case 12: irqout = 7; break;
	    default:
		printf("sio%d: irq configuration error\n",
		       device_get_unit(dev));
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return (ENXIO);
	    }
	    outb((iobase & 0x00ff) | tmp, irqout);
	}
#endif

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
#ifdef PC98
		tmp = ttspeedtab(SIO_TEST_SPEED,
				 if_16550a_type[iod.if_type & 0x0f].speedtab);
		sio_setreg(com, com_cfcr, CFCR_DLAB | CFCR_8BITS);
		sio_setreg(com, com_dlbl, tmp & 0xff);
		sio_setreg(com, com_dlbh, (tmp >> 8) & 0xff);
		sio_setreg(com, com_cfcr, CFCR_8BITS);
#else
		sio_setreg(com, com_cfcr, CFCR_DLAB | CFCR_8BITS);
		sio_setreg(com, com_dlbl, COMBRD(SIO_TEST_SPEED) & 0xff);
		sio_setreg(com, com_dlbh, (u_int) COMBRD(SIO_TEST_SPEED) >> 8);
		sio_setreg(com, com_cfcr, CFCR_8BITS);
#endif
		DELAY((16 + 1) * 1000000 / (SIO_TEST_SPEED / 10));
	}

	/*
	 * Enable the interrupt gate and disable device interupts.  This
	 * should leave the device driving the interrupt line low and
	 * guarantee an edge trigger if an interrupt can be generated.
	 */
/* EXTRA DELAY? */
	sio_setreg(com, com_mcr, mcr_image);
	sio_setreg(com, com_ier, 0);
	DELAY(1000);		/* XXX */
	irqmap[0] = isa_irq_pending();

	/*
	 * Attempt to set loopback mode so that we can send a null byte
	 * without annoying any external device.
	 */
/* EXTRA DELAY? */
	sio_setreg(com, com_mcr, mcr_image | MCR_LOOPBACK);

	/*
	 * Attempt to generate an output interrupt.  On 8250's, setting
	 * IER_ETXRDY generates an interrupt independent of the current
	 * setting and independent of whether the THR is empty.  On 16450's,
	 * setting IER_ETXRDY generates an interrupt independent of the
	 * current setting.  On 16550A's, setting IER_ETXRDY only
	 * generates an interrupt when IER_ETXRDY is not already set.
	 */
	sio_setreg(com, com_ier, IER_ETXRDY);
#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III)
		outb(iobase + rsa_ier, 0x04);
#endif

	/*
	 * On some 16x50 incompatibles, setting IER_ETXRDY doesn't generate
	 * an interrupt.  They'd better generate one for actually doing
	 * output.  Loopback may be broken on the same incompatibles but
	 * it's unlikely to do more than allow the null byte out.
	 */
	sio_setreg(com, com_data, 0);
	DELAY((1 + 2) * 1000000 / (SIO_TEST_SPEED / 10));

	/*
	 * Turn off loopback mode so that the interrupt gate works again
	 * (MCR_IENABLE was hidden).  This should leave the device driving
	 * an interrupt line high.  It doesn't matter if the interrupt
	 * line oscillates while we are not looking at it, since interrupts
	 * are disabled.
	 */
/* EXTRA DELAY? */
	sio_setreg(com, com_mcr, mcr_image);

	/*
	 * Some pcmcia cards have the "TXRDY bug", so we check everyone
	 * for IIR_TXRDY implementation ( Palido 321s, DC-1S... )
	 */
	if (COM_NOPROBE(flags)) {
		/* Reading IIR register twice */
		for (fn = 0; fn < 2; fn ++) {
			DELAY(10000);
			failures[6] = sio_getreg(com, com_iir);
		}
		/* Check IIR_TXRDY clear ? */
		result = 0;
		if (failures[6] & IIR_TXRDY) {
			/* Nop, Double check with clearing IER */
			sio_setreg(com, com_ier, 0);
			if (sio_getreg(com, com_iir) & IIR_NOPEND) {
				/* Ok. we're familia this gang */
				SET_FLAG(dev, COM_C_IIR_TXRDYBUG);
			} else {
				/* Unknown, Just omit this chip.. XXX */
				result = ENXIO;
			}
		} else {
			/* OK. this is well-known guys */
			CLR_FLAG(dev, COM_C_IIR_TXRDYBUG);
		}
		sio_setreg(com, com_cfcr, CFCR_8BITS);
		enable_intr();
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return (iobase == siocniobase ? 0 : result);
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
	failures[0] = sio_getreg(com, com_cfcr) - CFCR_8BITS;
	failures[1] = sio_getreg(com, com_ier) - IER_ETXRDY;
	failures[2] = sio_getreg(com, com_mcr) - mcr_image;
	DELAY(10000);		/* Some internal modems need this time */
	irqmap[1] = isa_irq_pending();
	failures[4] = (sio_getreg(com, com_iir) & IIR_IMASK) - IIR_TXRDY;
#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III)
		inb(iobase + rsa_srr);
#endif
	DELAY(1000);		/* XXX */
	irqmap[2] = isa_irq_pending();
	failures[6] = (sio_getreg(com, com_iir) & IIR_IMASK) - IIR_NOPEND;
#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III)
		inb(iobase + rsa_srr);
#endif

	/*
	 * Turn off all device interrupts and check that they go off properly.
	 * Leave MCR_IENABLE alone.  For ports without a master port, it gates
	 * the OUT2 output of the UART to
	 * the ICU input.  Closing the gate would give a floating ICU input
	 * (unless there is another device driving it) and spurious interrupts.
	 * (On the system that this was first tested on, the input floats high
	 * and gives a (masked) interrupt as soon as the gate is closed.)
	 */
	sio_setreg(com, com_ier, 0);
	sio_setreg(com, com_cfcr, CFCR_8BITS);	/* dummy to avoid bus echo */
	failures[7] = sio_getreg(com, com_ier);
#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III)
		outb(iobase + rsa_ier, 0x00);
#endif
	DELAY(1000);		/* XXX */
	irqmap[3] = isa_irq_pending();
	failures[9] = (sio_getreg(com, com_iir) & IIR_IMASK) - IIR_NOPEND;
#ifdef PC98
        if (iod.if_type == COM_IF_RSA98III) {
		inb(iobase + rsa_srr);
		outb(iobase + rsa_frr, 0x00);
	}
#endif

	enable_intr();

	irqs = irqmap[1] & ~irqmap[0];
	if (bus_get_resource(idev, SYS_RES_IRQ, 0, &xirq, NULL) == 0 &&
	    ((1 << xirq) & irqs) == 0)
		printf(
		"sio%d: configured irq %ld not in bitmap of probed irqs %#x\n",
		    device_get_unit(dev), xirq, irqs);
	if (bootverbose)
		printf("sio%d: irq maps: %#x %#x %#x %#x\n",
		    device_get_unit(dev),
		    irqmap[0], irqmap[1], irqmap[2], irqmap[3]);

	result = 0;
	for (fn = 0; fn < sizeof failures; ++fn)
		if (failures[fn]) {
			sio_setreg(com, com_mcr, 0);
			result = ENXIO;
			if (bootverbose) {
				printf("sio%d: probe failed test(s):",
				    device_get_unit(dev));
				for (fn = 0; fn < sizeof failures; ++fn)
					if (failures[fn])
						printf(" %d", fn);
				printf("\n");
			}
			break;
		}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
	return (iobase == siocniobase ? 0 : result);
}

#ifdef COM_ESP
static int
espattach(com, esp_port)
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
#ifdef PC98
	outb(esp_port + ESP98_CMD1, ESP_GETDIPS);
	dips = inb(esp_port + ESP98_STATUS1);
#else
	outb(esp_port + ESP_CMD1, ESP_GETDIPS);
	dips = inb(esp_port + ESP_STATUS1);
#endif

	/*
	 * Bits 0,1 of dips say which COM port we are.
	 */
#ifdef PC98
	if ((rman_get_start(com->ioportres) & 0xff) ==
	    likely_com_ports[dips & 0x03])
#else
	if (rman_get_start(com->ioportres) == likely_com_ports[dips & 0x03])
#endif
		printf(" : ESP");
	else {
		printf(" esp_port has com %d\n", dips & 0x03);
		return (0);
	}

	/*
	 * Check for ESP version 2.0 or later:  bits 4,5,6 = 010.
	 */
#ifdef PC98
	outb(esp_port + ESP98_CMD1, ESP_GETTEST);
	val = inb(esp_port + ESP98_STATUS1);	/* clear reg 1 */
	val = inb(esp_port + ESP98_STATUS2);
#else
	outb(esp_port + ESP_CMD1, ESP_GETTEST);
	val = inb(esp_port + ESP_STATUS1);	/* clear reg 1 */
	val = inb(esp_port + ESP_STATUS2);
#endif
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
sio_isa_attach(dev)
	device_t	dev;
{
	return (sioattach(dev, 0));
}

static int
sioattach(dev, xrid)
	device_t	dev;
	int		xrid;
{
	struct com_s	*com;
#ifdef COM_ESP
	Port_t		*espp;
#endif
	Port_t		iobase;
	int		unit;
	u_int		flags;
	int		rid;
	struct resource *port;
	int		ret;
#ifdef PC98
	u_char		*obuf;
	u_long		obufsize;
	int		if_type = GET_IFTYPE(device_get_flags(dev));
#endif

	rid = xrid;
#ifdef PC98
	if (IS_8251(if_type)) {
		port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					  0, ~0, 1, RF_ACTIVE);
	} else if (if_type == COM_IF_RSA98III ||
		   isa_get_vendorid(dev)) {
		port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
			if_16550a_type[if_type & 0x0f].iatsz, RF_ACTIVE);
	} else {
		port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
			if_16550a_type[if_type & 0x0f].iat,
			if_16550a_type[if_type & 0x0f].iatsz, RF_ACTIVE);
	}
#else
	port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				  0, ~0, IO_COMSIZE, RF_ACTIVE);
#endif
	if (!port)
		return (ENXIO);
#ifdef PC98
	if (!IS_8251(if_type)) {
		if (isa_load_resourcev(port,
			if_16550a_type[if_type & 0x0f].iat,
			if_16550a_type[if_type & 0x0f].iatsz) != 0) {
		    bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		    return ENXIO;
		}
	}
#endif

	iobase = rman_get_start(port);
	unit = device_get_unit(dev);
	com = device_get_softc(dev);
	flags = device_get_flags(dev);

	if (unit >= sio_numunits)
		sio_numunits = unit + 1;

#ifdef PC98
	obufsize = 256;
	if (if_type == COM_IF_RSA98III)
		obufsize = 2048;
	if ((obuf = malloc(obufsize * 2, M_DEVBUF, M_NOWAIT)) == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return ENXIO;
	}
	bzero(obuf, obufsize * 2);
#endif

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
	com->ioportres = port;
	com->bst = rman_get_bustag(port);
	com->bsh = rman_get_bushandle(port);
	com->cfcr_image = CFCR_8BITS;
	com->dtr_wait = 3 * hz;
	com->loses_outints = COM_LOSESOUTINTS(flags) != 0;
	com->no_irq = bus_get_resource(dev, SYS_RES_IRQ, 0, NULL, NULL) != 0;
	com->tx_fifo_size = 1;
#ifdef PC98
	com->obufsize = obufsize;
	com->obuf1 = obuf;
	com->obuf2 = obuf + obufsize;
#endif
	com->obufs[0].l_head = com->obuf1;
	com->obufs[1].l_head = com->obuf2;

#ifdef PC98
	com->pc98_if_type = if_type;

	if (IS_8251(if_type)) {
	    pc98_set_ioport(com);

	    if (if_type == COM_IF_INTERNAL && pc98_check_8251fifo()) {
		com->pc98_8251fifo = 1;
		com->pc98_8251fifo_enable = 0;
	    }
	} else {
	    bus_addr_t	*iat = if_16550a_type[if_type & 0x0f].iat;

	    com->data_port = iobase + iat[com_data];
	    com->int_id_port = iobase + iat[com_iir];
	    com->modem_ctl_port = iobase + iat[com_mcr];
	    com->mcr_image = inb(com->modem_ctl_port);
	    com->line_status_port = iobase + iat[com_lsr];
	    com->modem_status_port = iobase + iat[com_msr];
	    com->intr_ctl_port = iobase + iat[com_ier];
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
		if (IS_8251(com->pc98_if_type))
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
	if (siosetwater(com, com->it_in.c_ispeed) != 0) {
		enable_intr();
		/*
		 * Leave i/o resources allocated if this is a `cn'-level
		 * console, so that other devices can't snarf them.
		 */
		if (iobase != siocniobase)
			bus_release_resource(dev, SYS_RES_IOPORT, rid, port);
		return (ENOMEM);
	}
	enable_intr();
	termioschars(&com->it_in);
	com->it_out = com->it_in;

	/* attempt to determine UART type */
	printf("sio%d: type", unit);


#ifndef PC98
#ifdef COM_MULTIPORT
	if (!COM_ISMULTIPORT(flags) && !COM_IIR_TXRDYBUG(flags))
#else
	if (!COM_IIR_TXRDYBUG(flags))
#endif
	{
		u_char	scr;
		u_char	scr1;
		u_char	scr2;

		scr = sio_getreg(com, com_scr);
		sio_setreg(com, com_scr, 0xa5);
		scr1 = sio_getreg(com, com_scr);
		sio_setreg(com, com_scr, 0x5a);
		scr2 = sio_getreg(com, com_scr);
		sio_setreg(com, com_scr, scr);
		if (scr1 != 0xa5 || scr2 != 0x5a) {
			printf(" 8250");
			goto determined_type;
		}
	}
#endif /* !PC98 */
#ifdef PC98
	if (IS_8251(com->pc98_if_type)) {
	    if (com->pc98_8251fifo && !COM_NOFIFO(flags))
		com->tx_fifo_size = 16;
	    com_int_TxRx_disable( com );
	    com_cflag_and_speed_set( com, com->it_in.c_cflag, comdefaultrate );
	    com_tiocm_bic( com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE );
	    com_send_break_off( com );

	    if (com->pc98_if_type == COM_IF_INTERNAL) {
		printf(" (internal%s%s)",
		       com->pc98_8251fifo ? " fifo" : "",
		       PC98SIO_baud_rate_port(com->pc98_if_type) != -1 ?
		       " v-fast" : "");
	    } else {
		printf(" 8251%s", if_8251_type[com->pc98_if_type & 0x0f].name);
	    }
	} else {
#endif /* PC98 */
	sio_setreg(com, com_fifo, FIFO_ENABLE | FIFO_RX_HIGH);
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
		if (COM_NOFIFO(flags)) {
			printf(" 16550A fifo disabled");
		} else {
			com->hasfifo = TRUE;
#ifdef PC98
			com->tx_fifo_size = 0;	/* XXX flag conflicts. */
			printf(" 16550A");
#else
			if (COM_ST16650A(flags)) {
				com->st16650a = 1;
				com->tx_fifo_size = 32;
				printf(" ST16650A");
			} else {
				com->tx_fifo_size = COM_FIFOSIZE(flags);
				printf(" 16550A");
			}
#endif
		}
#ifdef PC98
		if (com->pc98_if_type == COM_IF_RSA98III) {
			com->tx_fifo_size = 2048;
			com->rsabase = iobase;
			outb(com->rsabase + rsa_ier, 0x00);
			outb(com->rsabase + rsa_frr, 0x00);
		}
#endif

#ifdef COM_ESP
#ifdef PC98
		if (com->pc98_if_type == COM_IF_ESP98)
#endif
		for (espp = likely_esp_ports; *espp != 0; espp++)
			if (espattach(com, *espp)) {
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
	
#ifdef PC98
	if (com->pc98_if_type == COM_IF_RSB3000) {
	    /* Set RSB-2000/3000 Extended Buffer mode. */
	    u_char lcr;
	    lcr = sio_getreg(com, com_cfcr);
	    sio_setreg(com, com_cfcr, lcr | CFCR_DLAB);
	    sio_setreg(com, com_emr, EMR_EXBUFF | EMR_EFMODE);
	    sio_setreg(com, com_cfcr, lcr);
	}
#endif

#ifdef COM_ESP
	if (com->esp) {
		/*
		 * Set 16550 compatibility mode.
		 * We don't use the ESP_MODE_SCALE bit to increase the
		 * fifo trigger levels because we can't handle large
		 * bursts of input.
		 * XXX flow control should be set in comparam(), not here.
		 */
#ifdef PC98
		outb(com->esp_port + ESP98_CMD1, ESP_SETMODE);
		outb(com->esp_port + ESP98_CMD2, ESP_MODE_RTS | ESP_MODE_FIFO);
#else
		outb(com->esp_port + ESP_CMD1, ESP_SETMODE);
		outb(com->esp_port + ESP_CMD2, ESP_MODE_RTS | ESP_MODE_FIFO);
#endif

		/* Set RTS/CTS flow control. */
#ifdef PC98
		outb(com->esp_port + ESP98_CMD1, ESP_SETFLOWTYPE);
		outb(com->esp_port + ESP98_CMD2, ESP_FLOW_RTS);
		outb(com->esp_port + ESP98_CMD2, ESP_FLOW_CTS);
#else
		outb(com->esp_port + ESP_CMD1, ESP_SETFLOWTYPE);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_RTS);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_CTS);
#endif

		/* Set flow-control levels. */
#ifdef PC98
		outb(com->esp_port + ESP98_CMD1, ESP_SETRXFLOW);
		outb(com->esp_port + ESP98_CMD2, HIBYTE(768));
		outb(com->esp_port + ESP98_CMD2, LOBYTE(768));
		outb(com->esp_port + ESP98_CMD2, HIBYTE(512));
		outb(com->esp_port + ESP98_CMD2, LOBYTE(512));
#else
		outb(com->esp_port + ESP_CMD1, ESP_SETRXFLOW);
		outb(com->esp_port + ESP_CMD2, HIBYTE(768));
		outb(com->esp_port + ESP_CMD2, LOBYTE(768));
		outb(com->esp_port + ESP_CMD2, HIBYTE(512));
		outb(com->esp_port + ESP_CMD2, LOBYTE(512));
#endif

#ifdef PC98
                /* Set UART clock prescaler. */
                outb(com->esp_port + ESP98_CMD1, ESP_SETCLOCK);
                outb(com->esp_port + ESP98_CMD2, 2);	/* 4 times */
#endif
	}
#endif /* COM_ESP */
	sio_setreg(com, com_fifo, 0);
#ifdef PC98
	printf("%s", if_16550a_type[com->pc98_if_type & 0x0f].name);
#else
determined_type: ;
#endif

#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(flags)) {
		device_t masterdev;

		com->multiport = TRUE;
		printf(" (multiport");
		if (unit == COM_MPMASTER(flags))
			printf(" master");
		printf(")");
		masterdev = devclass_get_device(sio_devclass,
		    COM_MPMASTER(flags));
		com->no_irq = (masterdev == NULL || bus_get_resource(masterdev,
		    SYS_RES_IRQ, 0, NULL, NULL) != 0);
	 }
#endif /* COM_MULTIPORT */
#ifdef PC98
	}
#endif
	if (unit == comconsole)
		printf(", console");
	if (COM_IIR_TXRDYBUG(flags))
		printf(" with a bogus IIR_TXRDY register");
	printf("\n");

	if (!sio_registered) {
		register_swi(SWI_TTY, siopoll);
		sio_registered = TRUE;
	}
	make_dev(&sio_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0600, "ttyd%r", unit);
	make_dev(&sio_cdevsw, unit | CONTROL_INIT_STATE,
	    UID_ROOT, GID_WHEEL, 0600, "ttyid%r", unit);
	make_dev(&sio_cdevsw, unit | CONTROL_LOCK_STATE,
	    UID_ROOT, GID_WHEEL, 0600, "ttyld%r", unit);
	make_dev(&sio_cdevsw, unit | CALLOUT_MASK,
	    UID_UUCP, GID_DIALER, 0660, "cuaa%r", unit);
	make_dev(&sio_cdevsw, unit | CALLOUT_MASK | CONTROL_INIT_STATE,
	    UID_UUCP, GID_DIALER, 0660, "cuaia%r", unit);
	make_dev(&sio_cdevsw, unit | CALLOUT_MASK | CONTROL_LOCK_STATE,
	    UID_UUCP, GID_DIALER, 0660, "cuala%r", unit);
	com->flags = flags;
	com->pps.ppscap = PPS_CAPTUREASSERT | PPS_CAPTURECLEAR;
	pps_init(&com->pps);

	rid = 0;
	com->irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0ul, ~0ul, 1,
	    RF_ACTIVE);
	if (com->irqres) {
		ret = BUS_SETUP_INTR(device_get_parent(dev), dev, com->irqres,
				     INTR_TYPE_TTY | INTR_TYPE_FAST,
				     siointr, com, &com->cookie);
		if (ret) {
			ret = BUS_SETUP_INTR(device_get_parent(dev), dev,
					     com->irqres, INTR_TYPE_TTY,
					     siointr, com, &com->cookie);
			if (ret == 0)
				device_printf(dev, "unable to activate interrupt in fast mode - using normal mode");
		}
		if (ret)
			device_printf(dev, "could not activate interrupt\n");
	}

	return (0);
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
	int		mynor;
	int		s;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	com = com_addr(unit);
	if (com == NULL)
		return (ENXIO);
	if (com->gone)
		return (ENXIO);
	if (mynor & CONTROL_MASK)
		return (0);
	tp = dev->si_tty = com->tp = ttymalloc(com->tp);
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
		if (tp->t_state & TS_XCLUDE &&
		    suser(p)) {
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
		tp->t_stop = comstop;
		tp->t_dev = dev;
		tp->t_termios = mynor & CALLOUT_MASK
				? com->it_out : com->it_in;
#ifdef PC98
		if (!IS_8251(com->pc98_if_type))
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
		if (IS_8251(com->pc98_if_type)) {
			com_tiocm_bis(com, TIOCM_DTR|TIOCM_RTS);
			pc98_msrint_start(dev);
			if (com->pc98_8251fifo) {
			    com->pc98_8251fifo_enable = 1;
			    outb(I8251F_fcr, CTRL8251F_ENABLE |
				 CTRL8251F_XMT_RST | CTRL8251F_RCV_RST);
			}
		}
#endif
		/*
		 * XXX we should goto open_top if comparam() slept.
		 */
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
				sio_setreg(com, com_fifo,
					   FIFO_RCV_RST | FIFO_XMT_RST
					   | com->fifo_image);
#ifdef PC98
				if (com->pc98_if_type == COM_IF_RSA98III)
					outb(com->rsabase + rsa_frr , 0x00);
#endif
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
#ifdef PC98
				if (com->pc98_if_type == COM_IF_RSA98III ?
				    !(inb(com->rsabase + rsa_srr) & 0x08) :
				    !(inb(com->line_status_port) & LSR_RXRDY))
					break;
#else
				if (!(inb(com->line_status_port) & LSR_RXRDY))
					break;
#endif
				sio_setreg(com, com_fifo, 0);
				DELAY(50);
				(void) inb(com->data_port);
			}
		}

		disable_intr();
#ifdef PC98
		if (IS_8251(com->pc98_if_type)) {
		    com_tiocm_bis(com, TIOCM_LE);
		    com->pc98_prev_modem_status = pc98_get_modem_status(com);
		    com_int_Rx_enable(com);
		} else {
#endif
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
		com->prev_modem_status = com->last_modem_status
		    = inb(com->modem_status_port);
		if (COM_IIR_TXRDYBUG(com->flags)) {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ERLS
						| IER_EMSC);
		} else {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ETXRDY
						| IER_ERLS | IER_EMSC);
		}
#ifdef PC98
		if (com->pc98_if_type == COM_IF_RSA98III) {
			outb(com->rsabase + rsa_ier, 0x1d);
			outb(com->intr_ctl_port, IER_ERLS | IER_EMSC);
		}
#endif
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
			(*linesw[tp->t_line].l_modem)(tp, 1);
#else
		if (com->prev_modem_status & MSR_DCD || mynor & CALLOUT_MASK)
			(*linesw[tp->t_line].l_modem)(tp, 1);
#endif
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
	if (com == NULL)
		return (ENODEV);
	tp = com->tp;
	s = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
#ifdef PC98
	com->modem_checking = 0;
#endif
	disc_optim(tp, &tp->t_termios, com);
	comstop(tp, FREAD | FWRITE);
	comhardclose(com);
	ttyclose(tp);
	siosettimeout();
	splx(s);
	if (com->gone) {
		printf("sio%d: gone\n", com->unit);
		s = spltty();
		if (com->ibuf != NULL)
			free(com->ibuf, M_DEVBUF);
		bzero(tp, sizeof *tp);
		splx(s);
	}
	return (0);
}

static void
comhardclose(com)
	struct com_s	*com;
{
	int		s;
	struct tty	*tp;
	int		unit;

	unit = com->unit;
	s = spltty();
	com->poll = FALSE;
	com->poll_output = FALSE;
	com->do_timestamp = FALSE;
	com->do_dcd_timestamp = FALSE;
	com->pps.ppsparam.mode = 0;
#ifdef PC98
	if (IS_8251(com->pc98_if_type))
		com_send_break_off(com);
	else
#endif
	sio_setreg(com, com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
	{
#ifdef PC98
		int tmp;
		if (IS_8251(com->pc98_if_type))
			com_int_TxRx_disable(com);
		else
			sio_setreg(com, com_ier, 0);
		if (com->pc98_if_type == COM_IF_RSA98III)
			outb(com->rsabase + rsa_ier, 0x00);
#else
		sio_setreg(com, com_ier, 0);
#endif
		tp = com->tp;
#ifdef PC98
		if (IS_8251(com->pc98_if_type))
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
		    || (!com->active_out
#ifdef PC98
			&& !(tmp)
#else
		        && !(com->prev_modem_status & MSR_DCD)
#endif
		        && !(com->it_in.c_cflag & CLOCAL))
		    || !(tp->t_state & TS_ISOPEN)) {
#ifdef PC98
			if (IS_8251(com->pc98_if_type))
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
			if (IS_8251(com->pc98_if_type))
				com_tiocm_bic(com, TIOCM_LE);
		}
#endif
	}
#ifdef PC98
	if (com->pc98_8251fifo)	{
	    if (com->pc98_8251fifo_enable)
		outb(I8251F_fcr, CTRL8251F_XMT_RST | CTRL8251F_RCV_RST);
	    com->pc98_8251fifo_enable = 0;
	}
#endif
	if (com->hasfifo) {
		/*
		 * Disable fifos so that they are off after controlled
		 * reboots.  Some BIOSes fail to detect 16550s when the
		 * fifos are enabled.
		 */
		sio_setreg(com, com_fifo, 0);
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
	struct com_s	*com;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);
	com = com_addr(MINOR_TO_UNIT(mynor));
	if (com == NULL || com->gone)
		return (ENODEV);
	return ((*linesw[com->tp->t_line].l_read)(com->tp, uio, flag));
}

static int
siowrite(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct com_s	*com;
	int		unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit = MINOR_TO_UNIT(mynor);
	com = com_addr(unit);
	if (com == NULL || com->gone)
		return (ENODEV);
	/*
	 * (XXX) We disallow virtual consoles if the physical console is
	 * a serial port.  This is in case there is a display attached that
	 * is not the console.  In that situation we don't need/want the X
	 * server taking over the console.
	 */
	if (constty != NULL && unit == comconsole)
		constty = NULL;
	return ((*linesw[com->tp->t_line].l_write)(com->tp, uio, flag));
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
	else if ((IS_8251(com->pc98_if_type) &&
		  ((com->pc98_8251fifo_enable &&
		    (inb(I8251F_lsr) & (STS8251F_TxRDY | STS8251F_TxEMP))
		    == (STS8251F_TxRDY | STS8251F_TxEMP)) ||
		   (!com->pc98_8251fifo_enable &&
		    (inb(com->sts_port) & (STS8251_TxRDY | STS8251_TxEMP))
		    == (STS8251_TxRDY | STS8251_TxEMP)))) ||
		 ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
		  == (LSR_TSRE | LSR_TXRDY))) {
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

static void
sioinput(com)
	struct com_s	*com;
{
	u_char		*buf;
	int		incc;
	u_char		line_status;
	int		recv_data;
	struct tty	*tp;

	buf = com->ibuf;
	tp = com->tp;
	if (!(tp->t_state & TS_ISOPEN) || !(tp->t_cflag & CREAD)) {
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
		return;
	}
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode.  It usually takes about 450
		 * instructions (that's without canonical processing or echo!).
		 * slinput is reasonably fast (usually 40 instructions plus
		 * call overhead).
		 */
		do {
			enable_intr();
			incc = com->iptr - buf;
			if (tp->t_rawq.c_cc + incc > tp->t_ihiwat
			    && (com->state & CS_RTS_IFLOW
				|| tp->t_iflag & IXOFF)
			    && !(tp->t_state & TS_TBLOCK))
				ttyblock(tp);
			com->delta_error_counts[CE_TTY_BUF_OVERFLOW]
				+= b_to_q((char *)buf, incc, &tp->t_rawq);
			buf += incc;
			tk_nin += incc;
			tk_rawcc += incc;
			tp->t_rawcc += incc;
			ttwakeup(tp);
			if (tp->t_state & TS_TTSTOP
			    && (tp->t_iflag & IXANY
				|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
				tp->t_state &= ~TS_TTSTOP;
				tp->t_lflag &= ~FLUSHO;
				comstart(tp);
			}
			disable_intr();
		} while (buf < com->iptr);
	} else {
		do {
			enable_intr();
			line_status = buf[com->ierroff];
			recv_data = *buf++;
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
			disable_intr();
		} while (buf < com->iptr);
	}
	com_events -= (com->iptr - com->ibuf);
	com->iptr = com->ibuf;

	/*
	 * There is now room for another low-level buffer full of input,
	 * so enable RTS if it is now disabled and there is room in the
	 * high-level buffer.
	 */
#ifdef PC98
	if (IS_8251(com->pc98_if_type)) {
		if ((com->state & CS_RTS_IFLOW) &&
		    !(com_tiocm_get(com) & TIOCM_RTS) &&
		    !(tp->t_state & TS_TBLOCK))
			com_tiocm_bis(com, TIOCM_RTS);
	} else {
		if ((com->state & CS_RTS_IFLOW) &&
		    !(com->mcr_image & MCR_RTS) &&
		    !(tp->t_state & TS_TBLOCK))
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
	}
#else
	if ((com->state & CS_RTS_IFLOW) && !(com->mcr_image & MCR_RTS) &&
	    !(tp->t_state & TS_TBLOCK))
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#endif
}

void
siointr(arg)
	void		*arg;
{
#ifndef COM_MULTIPORT
	COM_LOCK();
	siointr1((struct com_s *) arg);
	COM_UNLOCK();
#else /* COM_MULTIPORT */
	bool_t		possibly_more_intrs;
	int		unit;
	struct com_s	*com;
#ifdef PC98
	u_char		rsa_buf_status;
#endif

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
		for (unit = 0; unit < sio_numunits; ++unit) {
			com = com_addr(unit);
			/*
			 * XXX COM_LOCK();
			 * would it work here, or be counter-productive?
			 */
#ifdef PC98
			if (com != NULL 
			    && !com->gone
			    && IS_8251(com->pc98_if_type)) {
				siointr1(com);
			} else if (com != NULL 
			    && !com->gone
			    && com->pc98_if_type == COM_IF_RSA98III) {
				rsa_buf_status =
				    inb(com->rsabase + rsa_srr) & 0xc9;
				if ((rsa_buf_status & 0xc8)
				    || !(rsa_buf_status & 0x01)) {
				    siointr1(com);
				    if (rsa_buf_status !=
					(inb(com->rsabase + rsa_srr) & 0xc9))
					possibly_more_intrs = TRUE;
				}
			} else
#endif
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
	u_char	int_ctl;
	u_char	int_ctl_new;
	struct	timecounter *tc;
	u_int	count;

#ifdef PC98
	u_char	tmp = 0;
	u_char	rsa_buf_status = 0;
	int	rsa_tx_fifo_size = 0;
#endif /* PC98 */

	int_ctl = inb(com->intr_ctl_port);
	int_ctl_new = int_ctl;

	while (!com->gone) {
#ifdef PC98
status_read:;
		if (IS_8251(com->pc98_if_type)) {
			if (com->pc98_8251fifo_enable)
				tmp = inb(I8251F_lsr);
			else
				tmp = inb(com->sts_port);
more_intr:
			line_status = 0;
			if (com->pc98_8251fifo_enable) {
			    if (tmp & STS8251F_TxRDY) line_status |= LSR_TXRDY;
			    if (tmp & STS8251F_RxRDY) line_status |= LSR_RXRDY;
			    if (tmp & STS8251F_TxEMP) line_status |= LSR_TSRE;
			    if (tmp & STS8251F_PE)    line_status |= LSR_PE;
			    if (tmp & STS8251F_OE)    line_status |= LSR_OE;
			    if (tmp & STS8251F_BD_SD) line_status |= LSR_BI;
			} else {
			    if (tmp & STS8251_TxRDY)  line_status |= LSR_TXRDY;
			    if (tmp & STS8251_RxRDY)  line_status |= LSR_RXRDY;
			    if (tmp & STS8251_TxEMP)  line_status |= LSR_TSRE;
			    if (tmp & STS8251_PE)     line_status |= LSR_PE;
			    if (tmp & STS8251_OE)     line_status |= LSR_OE;
			    if (tmp & STS8251_FE)     line_status |= LSR_FE;
			    if (tmp & STS8251_BD_SD)  line_status |= LSR_BI;
			}
		} else {
#endif /* PC98 */
		if (com->pps.ppsparam.mode & PPS_CAPTUREBOTH) {
			modem_status = inb(com->modem_status_port);
		        if ((modem_status ^ com->last_modem_status) & MSR_DCD) {
				tc = timecounter;
				count = tc->tc_get_timecount(tc);
				pps_event(&com->pps, tc, count, 
				    (modem_status & MSR_DCD) ? 
				    PPS_CAPTUREASSERT : PPS_CAPTURECLEAR);
			}
		}
		line_status = inb(com->line_status_port);
#ifdef PC98
		}
		if (com->pc98_if_type == COM_IF_RSA98III)
			rsa_buf_status = inb(com->rsabase + rsa_srr);
#endif /* PC98 */

		/* input event? (check first to help avoid overruns) */
#ifndef PC98
		while (line_status & LSR_RCV_MASK) {
#else
		while ((line_status & LSR_RCV_MASK)
		       || (com->pc98_if_type == COM_IF_RSA98III
			   && (rsa_buf_status & 0x08))) {
#endif /* PC98 */
			/* break/unnattached error bits or real input? */
#ifdef PC98
			if (IS_8251(com->pc98_if_type)) {
				if (com->pc98_8251fifo_enable) {
				    recv_data = inb(I8251F_data);
				    if (tmp & (STS8251F_PE | STS8251F_OE |
					       STS8251F_BD_SD)) {
					pc98_i8251_or_cmd(com, CMD8251_ER);
					recv_data = 0;
				    }
				} else {
				    recv_data = inb(com->data_port);
				    if (tmp & (STS8251_PE | STS8251_OE |
					       STS8251_FE | STS8251_BD_SD)) {
					pc98_i8251_or_cmd(com, CMD8251_ER);
					recv_data = 0;
				    }
				}
			} else if (com->pc98_if_type == COM_IF_RSA98III) {
				if (!(rsa_buf_status & 0x08))
					recv_data = 0;
				else
					recv_data = inb(com->data_port);
			} else
#endif
			if (!(line_status & LSR_RXRDY))
				recv_data = 0;
			else
				recv_data = inb(com->data_port);
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
				ioptr[com->ierroff] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#ifdef PC98
					IS_8251(com->pc98_if_type) ?
						com_tiocm_bic(com, TIOCM_RTS) :
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
			if (IS_8251(com->pc98_if_type))
				goto status_read;
			else
#endif
			line_status = inb(com->line_status_port) & 0x7F;
#ifdef PC98
			if (com->pc98_if_type == COM_IF_RSA98III)
				rsa_buf_status = inb(com->rsabase + rsa_srr);
#endif /* PC98 */
		}

		/* modem status change? (always check before doing output) */
#ifdef PC98
		if (!IS_8251(com->pc98_if_type)) {
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
#ifndef PC98
		if (line_status & LSR_TXRDY
		    && com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
#else
		if (((com->pc98_if_type == COM_IF_RSA98III)
		     ? (rsa_buf_status & 0x02)
		     : (line_status & LSR_TXRDY))
		    && com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
#endif
#ifdef PC98
			Port_t	tmp_data_port;
			
			if (IS_8251(com->pc98_if_type) &&
			    com->pc98_8251fifo_enable)
				tmp_data_port = I8251F_data;
			else
				tmp_data_port = com->data_port;
#endif

			ioptr = com->obufq.l_head;
			if (com->tx_fifo_size > 1) {
				u_int	ocount;

				ocount = com->obufq.l_tail - ioptr;
#ifdef PC98
				if (com->pc98_if_type == COM_IF_RSA98III) {
				  rsa_buf_status = inb(com->rsabase + rsa_srr);
				  rsa_tx_fifo_size = 1024;
				  if (!(rsa_buf_status & 0x01))
				      rsa_tx_fifo_size = 2048;
				  if (ocount > rsa_tx_fifo_size)
				      ocount = rsa_tx_fifo_size;
				} else
#endif
				if (ocount > com->tx_fifo_size)
					ocount = com->tx_fifo_size;
				com->bytes_out += ocount;
				do
#ifdef PC98
					outb(tmp_data_port, *ioptr++);
#else
					outb(com->data_port, *ioptr++);
#endif
				while (--ocount != 0);
			} else {
#ifdef PC98
				outb(tmp_data_port, *ioptr++);
#else
				outb(com->data_port, *ioptr++);
#endif
				++com->bytes_out;
			}
#ifdef PC98
			if (IS_8251(com->pc98_if_type))
			    if (!(pc98_check_i8251_interrupt(com) & IEN_TxFLAG))
				com_int_Tx_enable(com);
#endif
			com->obufq.l_head = ioptr;
			if (COM_IIR_TXRDYBUG(com->flags)) {
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
					if (COM_IIR_TXRDYBUG(com->flags)) {
						int_ctl_new = int_ctl & ~IER_ETXRDY;
					}
					com->state &= ~CS_BUSY;
#if defined(PC98)
					if (IS_8251(com->pc98_if_type) &&
					    pc98_check_i8251_interrupt(com) & IEN_TxFLAG)
						com_int_Tx_disable(com);
#endif
				}
				if (!(com->state & CS_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->state |= CS_ODONE;
					setsofttty();	/* handle at high level ASAP */
				}
			}
			if (COM_IIR_TXRDYBUG(com->flags) && (int_ctl != int_ctl_new)) {
#ifdef PC98
				if (com->pc98_if_type == COM_IF_RSA98III) {
				    int_ctl_new &= ~(IER_ETXRDY | IER_ERXRDY);
				    outb(com->intr_ctl_port, int_ctl_new);
				    outb(com->rsabase + rsa_ier, 0x1d);
				} else
#endif
				outb(com->intr_ctl_port, int_ctl_new);
			}
		}
#ifdef PC98
		else if (line_status & LSR_TXRDY) {
		    if (IS_8251(com->pc98_if_type))
			if (pc98_check_i8251_interrupt(com) & IEN_TxFLAG)
			    com_int_Tx_disable(com);
		}
		if (IS_8251(com->pc98_if_type)) {
		    if (com->pc98_8251fifo_enable) {
			if ((tmp = inb(I8251F_lsr)) & STS8251F_RxRDY)
			    goto more_intr;
		    } else {
			if ((tmp = inb(com->sts_port)) & STS8251_RxRDY)
			    goto more_intr;
		    }
		}
#endif

		/* finished? */
#ifndef COM_MULTIPORT
#ifdef PC98
		if (IS_8251(com->pc98_if_type))
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
	int		mynor;
	int		s;
	struct tty	*tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	u_long		oldcmd;
	struct termios	term;
#endif

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
	if (com == NULL || com->gone)
		return (ENODEV);
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
			error = suser(p);
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
	if (IS_8251(com->pc98_if_type)) {
	    switch (cmd) {
	    case TIOCSBRK:
		com_send_break_on(com);
		break;
	    case TIOCCBRK:
		com_send_break_off(com);
		break;
	    case TIOCSDTR:
		com_tiocm_bis(com, TIOCM_DTR | TIOCM_RTS);
		break;
	    case TIOCCDTR:
		com_tiocm_bic(com, TIOCM_DTR);
		break;
	/*
	 * XXX should disallow changing MCR_RTS if CS_RTS_IFLOW is set.  The
	 * changes get undone on the next call to comparam().
	 */
	    case TIOCMSET:
		com_tiocm_set(com, *(int *)data);
		break;
	    case TIOCMBIS:
		com_tiocm_bis(com, *(int *)data);
		break;
	    case TIOCMBIC:
		com_tiocm_bic(com, *(int *)data);
		break;
	    case TIOCMGET:
		*(int *)data = com_tiocm_get(com);
		break;
	    case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p);
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
		error = pps_ioctl(cmd, data, &com->pps);
		if (error == ENODEV)
			error = ENOTTY;
		return (error);
	    }
	} else {
#endif
	switch (cmd) {
	case TIOCSBRK:
		sio_setreg(com, com_cfcr, com->cfcr_image |= CFCR_SBREAK);
		break;
	case TIOCCBRK:
		sio_setreg(com, com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
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
		error = suser(p);
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
		error = pps_ioctl(cmd, data, &com->pps);
		if (error == ENODEV)
			error = ENOTTY;
		return (error);
	}
#ifdef PC98
	}
#endif
	splx(s);
	return (0);
}

static void
siopoll()
{
	int		unit;

	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < sio_numunits; ++unit) {
		struct com_s	*com;
		int		incc;
		struct tty	*tp;

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
		if (com->iptr != com->ibuf) {
			disable_intr();
			sioinput(com);
			enable_intr();
		}
		if (com->state & CS_CHECKMSR) {
			u_char	delta_modem_status;

#ifdef PC98
			if (!IS_8251(com->pc98_if_type)) {
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
	int		s;
	int		unit;
#ifdef PC98
	u_char		param = 0;
#endif

#ifdef PC98
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);

	cfcr = 0;
	if (IS_8251(com->pc98_if_type)) {
	    divisor = pc98_ttspeedtab(com, t->c_ospeed);
	} else {
	    /* do historical conversions */
	    if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	    /* check requested parameters */
	    divisor = ttspeedtab(t->c_ospeed,
			if_16550a_type[com->pc98_if_type & 0x0f].speedtab);
	}
#else
	/* do historical conversions */
	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	/* check requested parameters */
	divisor = ttspeedtab(t->c_ospeed, comspeedtab);
#endif
	if (divisor < 0 || (divisor > 0 && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

#ifndef PC98
	/* parameters are OK, convert them to the com struct and the device */
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	if (com == NULL)
		return (ENODEV);
#endif
	s = spltty();
#ifdef PC98
	if (IS_8251(com->pc98_if_type)) {
		if (divisor == 0)
			com_tiocm_bic(com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE);
		else
			com_tiocm_bis(com, TIOCM_DTR|TIOCM_RTS|TIOCM_LE);
	} else
#endif
	if (divisor == 0)
		(void)commctl(com, TIOCM_DTR, DMBIC);	/* hang up line */
	else
		(void)commctl(com, TIOCM_DTR, DMBIS);
	cflag = t->c_cflag;
#ifdef PC98
	if (!IS_8251(com->pc98_if_type)) {
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
		sio_setreg(com, com_fifo, com->fifo_image);
	}
#ifdef PC98
	}
#endif

	/*
	 * This returns with interrupts disabled so that we can complete
	 * the speed change atomically.  Keeping interrupts disabled is
	 * especially important while com_data is hidden.
	 */
	(void) siosetwater(com, t->c_ispeed);

#ifdef PC98
	if (IS_8251(com->pc98_if_type))
		com_cflag_and_speed_set(com, cflag, t->c_ospeed);
	else {
#endif
	if (divisor != 0) {
		sio_setreg(com, com_cfcr, cfcr | CFCR_DLAB);
		/*
		 * Only set the divisor registers if they would change,
		 * since on some 16550 incompatibles (UMC8669F), setting
		 * them while input is arriving them loses sync until
		 * data stops arriving.
		 */
		dlbl = divisor & 0xFF;
		if (sio_getreg(com, com_dlbl) != dlbl)
			sio_setreg(com, com_dlbl, dlbl);
		dlbh = (u_int) divisor >> 8;
		if (sio_getreg(com, com_dlbh) != dlbh)
			sio_setreg(com, com_dlbh, dlbh);
	}

	sio_setreg(com, com_cfcr, com->cfcr_image = cfcr);
#ifdef PC98
	}
#endif

	if (!(tp->t_state & TS_TTSTOP))
		com->state |= CS_TTGO;

	if (cflag & CRTS_IFLOW) {
#ifndef PC98
		if (com->st16650a) {
			sio_setreg(com, com_cfcr, 0xbf);
			sio_setreg(com, com_fifo,
				   sio_getreg(com, com_fifo) | 0x40);
		}
#endif
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
		if (IS_8251(com->pc98_if_type))
			com_tiocm_bis(com, TIOCM_RTS);
		else
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#else
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
		if (com->st16650a) {
			sio_setreg(com, com_cfcr, 0xbf);
			sio_setreg(com, com_fifo,
				   sio_getreg(com, com_fifo) & ~0x40);
		}
#endif
	}


	/*
	 * Set up state to handle output flow control.
	 * XXX - worth handling MDMBUF (DCD) flow control at the lowest level?
	 * Now has 10+ msec latency, while CTS flow has 50- usec latency.
	 */
	com->state |= CS_ODEVREADY;
	com->state &= ~CS_CTS_OFLOW;
#ifdef PC98
	if (com->pc98_if_type == COM_IF_RSA98III) {
		param = inb(com->rsabase + rsa_msr);
		outb(com->rsabase + rsa_msr, param & 0x14);
	}
#endif
	if (cflag & CCTS_OFLOW) {
		com->state |= CS_CTS_OFLOW;
#ifdef PC98
		if (IS_8251(com->pc98_if_type)) {
			if (!(pc98_get_modem_status(com) & TIOCM_CTS))
				com->state &= ~CS_ODEVREADY;
		} else {
			if (com->pc98_if_type == COM_IF_RSA98III) {
				/* Set automatic flow control mode */
				outb(com->rsabase + rsa_msr, param | 0x08);
			} else
#endif
		if (!(com->last_modem_status & MSR_CTS))
			com->state &= ~CS_ODEVREADY;
#ifdef PC98
		}
#else
		if (com->st16650a) {
			sio_setreg(com, com_cfcr, 0xbf);
			sio_setreg(com, com_fifo,
				   sio_getreg(com, com_fifo) | 0x80);
		}
	} else {
		if (com->st16650a) {
			sio_setreg(com, com_cfcr, 0xbf);
			sio_setreg(com, com_fifo,
				   sio_getreg(com, com_fifo) & ~0x80);
		}
#endif
	}

#ifdef PC98
	if (!IS_8251(com->pc98_if_type))
#endif
	sio_setreg(com, com_cfcr, com->cfcr_image);

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
	if (com->ibufold != NULL) {
		free(com->ibufold, M_DEVBUF);
		com->ibufold = NULL;
	}
	return (0);
}

static int
siosetwater(com, speed)
	struct com_s	*com;
	speed_t		speed;
{
	int		cp4ticks;
	u_char		*ibuf;
	int		ibufsize;
	struct tty	*tp;

	/*
	 * Make the buffer size large enough to handle a softtty interrupt
	 * latency of about 2 ticks without loss of throughput or data
	 * (about 3 ticks if input flow control is not used or not honoured,
	 * but a bit less for CS5-CS7 modes).
	 */
	cp4ticks = speed / 10 / hz * 4;
	for (ibufsize = 128; ibufsize < cp4ticks;)
		ibufsize <<= 1;
#ifdef PC98
	if (com->pc98_if_type == COM_IF_RSA98III)
		ibufsize = 2048;
#endif
	if (ibufsize == com->ibufsize) {
		disable_intr();
		return (0);
	}

	/*
	 * Allocate input buffer.  The extra factor of 2 in the size is
	 * to allow for an error byte for each input byte.
	 */
	ibuf = malloc(2 * ibufsize, M_DEVBUF, M_NOWAIT);
	if (ibuf == NULL) {
		disable_intr();
		return (ENOMEM);
	}

	/* Initialize non-critical variables. */
	com->ibufold = com->ibuf;
	com->ibufsize = ibufsize;
	tp = com->tp;
	if (tp != NULL) {
		tp->t_ififosize = 2 * ibufsize;
		tp->t_ispeedwat = (speed_t)-1;
		tp->t_ospeedwat = (speed_t)-1;
	}

	/*
	 * Read current input buffer, if any.  Continue with interrupts
	 * disabled.
	 */
	disable_intr();
	if (com->iptr != com->ibuf)
		sioinput(com);

	/*-
	 * Initialize critical variables, including input buffer watermarks.
	 * The external device is asked to stop sending when the buffer
	 * exactly reaches high water, or when the high level requests it.
	 * The high level is notified immediately (rather than at a later
	 * clock tick) when this watermark is reached.
	 * The buffer size is chosen so the watermark should almost never
	 * be reached.
	 * The low watermark is invisibly 0 since the buffer is always
	 * emptied all at once.
	 */
	com->iptr = com->ibuf = ibuf;
	com->ibufend = ibuf + ibufsize;
	com->ierroff = ibufsize;
	com->ihighwater = ibuf + 3 * ibufsize / 4;
	return (0);
}

static void
comstart(tp)
	struct tty	*tp;
{
	struct com_s	*com;
	int		s;
	int		unit;

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	if (com == NULL)
		return;
	s = spltty();
	disable_intr();
	if (tp->t_state & TS_TTSTOP)
		com->state &= ~CS_TTGO;
	else
		com->state |= CS_TTGO;
	if (tp->t_state & TS_TBLOCK) {
#ifdef PC98
		if (IS_8251(com->pc98_if_type)) {
		    if ((com_tiocm_get(com) & TIOCM_RTS) &&
			(com->state & CS_RTS_IFLOW))
			com_tiocm_bic(com, TIOCM_RTS);
		} else {
		    if ((com->mcr_image & MCR_RTS) &&
			(com->state & CS_RTS_IFLOW))
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
		}
#else
		if (com->mcr_image & MCR_RTS && com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
#endif
	} else {
#ifdef PC98
		if (IS_8251(com->pc98_if_type)) {
		    if (!(com_tiocm_get(com) & TIOCM_RTS) &&
			com->iptr < com->ihighwater &&
			com->state & CS_RTS_IFLOW)
			com_tiocm_bis(com, TIOCM_RTS);
		} else {
		    if (!(com->mcr_image & MCR_RTS) &&
			com->iptr < com->ihighwater &&
			com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
		}
#else
		if (!(com->mcr_image & MCR_RTS) && com->iptr < com->ihighwater
		    && com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#endif
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc != 0) {
		struct lbq	*qp;
		struct lbq	*next;

		if (!com->obufs[0].l_queued) {
			com->obufs[0].l_tail
			    = com->obuf1 + q_to_b(&tp->t_outq, com->obuf1,
#ifdef PC98
						  com->obufsize);
#else
						  sizeof com->obuf1);
#endif
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
#ifdef PC98
						  com->obufsize);
#else
						  sizeof com->obuf2);
#endif
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
	ttwwakeup(tp);
	splx(s);
}

static void
comstop(tp, rw)
	struct tty	*tp;
	int		rw;
{
	struct com_s	*com;
#ifdef PC98
	int		rsa98_tmp  = 0;
#endif

	com = com_addr(DEV_TO_UNIT(tp->t_dev));
	if (com == NULL || com->gone)
		return;
	disable_intr();
	if (rw & FWRITE) {
#ifdef PC98
		if (!IS_8251(com->pc98_if_type)) {
#endif
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			sio_setreg(com, com_fifo,
				   FIFO_XMT_RST | com->fifo_image);
#ifdef PC98
		if (com->pc98_if_type == COM_IF_RSA98III)
		    for (rsa98_tmp = 0; rsa98_tmp < 2048; rsa98_tmp++)
			sio_setreg(com, com_fifo,
				   FIFO_XMT_RST | com->fifo_image);
		}
#endif
		com->obufs[0].l_queued = FALSE;
		com->obufs[1].l_queued = FALSE;
		if (com->state & CS_ODONE)
			com_events -= LOTS_OF_EVENTS;
		com->state &= ~(CS_ODONE | CS_BUSY);
		com->tp->t_state &= ~TS_BUSY;
	}
	if (rw & FREAD) {
#ifdef PC98
		if (!IS_8251(com->pc98_if_type)) {
		    if (com->pc98_if_type == COM_IF_RSA98III)
			for (rsa98_tmp = 0; rsa98_tmp < 2048; rsa98_tmp++)
			    sio_getreg(com, com_data);
#endif
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			sio_setreg(com, com_fifo,
				   FIFO_RCV_RST | com->fifo_image);
#ifdef PC98
		}
#endif
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	enable_intr();
	comstart(tp);
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
	for (unit = 0; unit < sio_numunits; ++unit) {
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
	for (unit = 0; unit < sio_numunits; ++unit) {
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
	for (unit = 0; unit < sio_numunits; ++unit) {
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
#include <sys/cons.h>

struct siocnstate {
	u_char	dlbl;
	u_char	dlbh;
	u_char	ier;
	u_char	cfcr;
	u_char	mcr;
};

static speed_t siocngetspeed __P((Port_t, struct speedtab *));
static void siocnclose	__P((struct siocnstate *sp, Port_t iobase));
static void siocnopen	__P((struct siocnstate *sp, Port_t iobase, int speed));
static void siocntxwait	__P((Port_t iobase));

static cn_probe_t siocnprobe;
static cn_init_t siocninit;
static cn_checkc_t siocncheckc;
static cn_getc_t siocngetc;
static cn_putc_t siocnputc;

#ifdef __i386__
CONS_DRIVER(sio, siocnprobe, siocninit, NULL, siocngetc, siocncheckc,
	    siocnputc, NULL);
#endif

/* To get the GDB related variables */
#if DDB > 0
#include <ddb/ddb.h>
#endif

static void
siocntxwait(iobase)
	Port_t	iobase;
{
	int	timo;

	/*
	 * Wait for any pending transmission to finish.  Required to avoid
	 * the UART lockup bug when the speed is changed, and for normal
	 * transmits.
	 */
	timo = 100000;
	while ((inb(iobase + com_lsr) & (LSR_TSRE | LSR_TXRDY))
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

	for (; table->sp_speed != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);

	return (0);	/* didn't match anything sane */
}

static void
siocnopen(sp, iobase, speed)
	struct siocnstate	*sp;
	Port_t			iobase;
	int			speed;
{
	int	divisor;
	u_char	dlbh;
	u_char	dlbl;

	/*
	 * Save all the device control registers except the fifo register
	 * and set our default ones (cs8 -parenb speed=comdefaultrate).
	 * We can't save the fifo register since it is read-only.
	 */
	sp->ier = inb(iobase + com_ier);
	outb(iobase + com_ier, 0);	/* spltty() doesn't stop siointr() */
	siocntxwait(iobase);
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
	divisor = ttspeedtab(speed, comspeedtab);
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
siocnclose(sp, iobase)
	struct siocnstate	*sp;
	Port_t			iobase;
{
	/*
	 * Restore the device control registers.
	 */
	siocntxwait(iobase);
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

static void
siocnprobe(cp)
	struct consdev	*cp;
{
	speed_t			boot_speed;
	u_char			cfcr;
	int			s, unit;
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

	for (unit = 0; unit < 16; unit++) { /* XXX need to know how many */
		int flags;
		int disabled;
		if (resource_int_value("sio", unit, "disabled", &disabled) == 0) {
			if (disabled)
				continue;
		}
		if (resource_int_value("sio", unit, "flags", &flags))
			continue;
		if (COM_CONSOLE(flags) || COM_DEBUGGER(flags)) {
			int port;
			Port_t iobase;

			if (resource_int_value("sio", unit, "port", &port))
				continue;
			iobase = port;
			s = spltty();
			if (boothowto & RB_SERIAL) {
				boot_speed = siocngetspeed(iobase, comspeedtab);
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
			cfcr = inb(iobase + com_cfcr);
			outb(iobase + com_cfcr, CFCR_DLAB | cfcr);
			outb(iobase + com_dlbl,
			     COMBRD(comdefaultrate) & 0xff);
			outb(iobase + com_dlbh,
			     (u_int) COMBRD(comdefaultrate) >> 8);
			outb(iobase + com_cfcr, cfcr);

			siocnopen(&sp, iobase, comdefaultrate);

			splx(s);
			if (COM_CONSOLE(flags) && !COM_LLCONSOLE(flags)) {
				cp->cn_dev = makedev(CDEV_MAJOR, unit);
				cp->cn_pri = COM_FORCECONSOLE(flags)
					     || boothowto & RB_SERIAL
					     ? CN_REMOTE : CN_NORMAL;
				siocniobase = iobase;
				siocnunit = unit;
			}
			if (COM_DEBUGGER(flags)) {
				printf("sio%d: gdb debugging port\n", unit);
				siogdbiobase = iobase;
				siogdbunit = unit;
#if DDB > 0
				gdbdev = makedev(CDEV_MAJOR, unit);
				gdb_getc = siocngetc;
				gdb_putc = siocnputc;
#endif
			}
		}
	}
#ifdef	__i386__
#if DDB > 0
	/*
	 * XXX Ugly Compatability.
	 * If no gdb port has been specified, set it to be the console
	 * as some configuration files don't specify the gdb port.
	 */
	if (gdbdev == NODEV && (boothowto & RB_GDB)) {
		printf("Warning: no GDB port specified. Defaulting to sio%d.\n",
			siocnunit);
		printf("Set flag 0x80 on desired GDB port in your\n");
		printf("configuration file (currently sio only).\n");
		siogdbiobase = siocniobase;
		siogdbunit = siocnunit;
		gdbdev = makedev(CDEV_MAJOR, siocnunit);
		gdb_getc = siocngetc;
		gdb_putc = siocnputc;
	}
#endif
#endif
}

#ifdef __alpha__

CONS_DRIVER(sio, NULL, NULL, NULL, siocngetc, siocncheckc, siocnputc, NULL);

int
siocnattach(port, speed)
	int port;
	int speed;
{
	int			s;
	u_char			cfcr;
	struct siocnstate	sp;

	siocniobase = port;
	comdefaultrate = speed;
	sio_consdev.cn_pri = CN_NORMAL;
	sio_consdev.cn_dev = makedev(CDEV_MAJOR, 0);

	s = spltty();

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

	siocnopen(&sp, siocniobase, comdefaultrate);
	splx(s);

	cn_tab = &sio_consdev;
	return (0);
}

int
siogdbattach(port, speed)
	int port;
	int speed;
{
	int			s;
	u_char			cfcr;
	struct siocnstate	sp;

	siogdbiobase = port;
	gdbdefaultrate = speed;

	s = spltty();

	/*
	 * Initialize the divisor latch.  We can't rely on
	 * siocnopen() to do this the first time, since it 
	 * avoids writing to the latch if the latch appears
	 * to have the correct value.  Also, if we didn't
	 * just read the speed from the hardware, then we
	 * need to set the speed in hardware so that
	 * switching it later is null.
	 */
	cfcr = inb(siogdbiobase + com_cfcr);
	outb(siogdbiobase + com_cfcr, CFCR_DLAB | cfcr);
	outb(siogdbiobase + com_dlbl,
	     COMBRD(gdbdefaultrate) & 0xff);
	outb(siogdbiobase + com_dlbh,
	     (u_int) COMBRD(gdbdefaultrate) >> 8);
	outb(siogdbiobase + com_cfcr, cfcr);

	siocnopen(&sp, siogdbiobase, gdbdefaultrate);
	splx(s);

	return (0);
}

#endif

static void
siocninit(cp)
	struct consdev	*cp;
{
	comconsole = DEV_TO_UNIT(cp->cn_dev);
}

static int
siocncheckc(dev)
	dev_t	dev;
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	if (minor(dev) == siogdbunit)
		iobase = siogdbiobase;
	else
		iobase = siocniobase;
	s = spltty();
	siocnopen(&sp, iobase, comdefaultrate);
	if (inb(iobase + com_lsr) & LSR_RXRDY)
		c = inb(iobase + com_data);
	else
		c = -1;
	siocnclose(&sp, iobase);
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

	if (minor(dev) == siogdbunit)
		iobase = siogdbiobase;
	else
		iobase = siocniobase;
	s = spltty();
	siocnopen(&sp, iobase, comdefaultrate);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp, iobase);
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
	Port_t	iobase;

	if (minor(dev) == siogdbunit)
		iobase = siogdbiobase;
	else
		iobase = siocniobase;
	s = spltty();
	siocnopen(&sp, iobase, comdefaultrate);
	siocntxwait(iobase);
	outb(iobase + com_data, c);
	siocnclose(&sp, iobase);
	splx(s);
}

#ifdef __alpha__
int
siogdbgetc()
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siogdbiobase;
	s = spltty();
	siocnopen(&sp, iobase, gdbdefaultrate);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp, iobase);
	splx(s);
	return (c);
}

void
siogdbputc(c)
	int	c;
{
	int	s;
	struct siocnstate	sp;

	s = spltty();
	siocnopen(&sp, siogdbiobase, gdbdefaultrate);
	siocntxwait(siogdbiobase);
	outb(siogdbiobase + com_data, c);
	siocnclose(&sp, siogdbiobase);
	splx(s);
}
#endif

DRIVER_MODULE(sio, isa, sio_isa_driver, sio_devclass, 0, 0);
#if NCARD > 0
DRIVER_MODULE(sio, pccard, sio_pccard_driver, sio_devclass, 0, 0);
#endif
#if NPCI > 0
DRIVER_MODULE(sio, pci, sio_pci_driver, sio_devclass, 0, 0);
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
	register int	msr;

	msr = com->pc98_prev_modem_status
			& ~(TIOCM_CAR|TIOCM_RI|TIOCM_DSR|TIOCM_CTS);
	if (com->pc98_8251fifo_enable) {
		int	stat2;

		stat2 = inb(I8251F_msr);
		if ( stat2 & CICSCDF_CD ) msr |= TIOCM_CAR;
		if ( stat2 & CICSCDF_CI ) msr |= TIOCM_RI;
		if ( stat2 & CICSCDF_DR ) msr |= TIOCM_DSR;
		if ( stat2 & CICSCDF_CS ) msr |= TIOCM_CTS;
#if COM_CARRIER_DETECT_EMULATE
		if ( msr & (TIOCM_DSR|TIOCM_CTS) ) {
			msr |= TIOCM_CAR;
		}
#endif
	} else {
		int	stat, stat2;
		
		stat  = inb(com->sts_port);
		stat2 = inb(com->in_modem_port);
		if ( !(stat2 & CICSCD_CD) ) msr |= TIOCM_CAR;
		if ( !(stat2 & CICSCD_CI) ) msr |= TIOCM_RI;
		if (   stat & STS8251_DSR ) msr |= TIOCM_DSR;
		if ( !(stat2 & CICSCD_CS) ) msr |= TIOCM_CTS;
#if COM_CARRIER_DETECT_EMULATE
		if ( msr & (TIOCM_DSR|TIOCM_CTS) ) {
			msr |= TIOCM_CAR;
		}
#endif
	}
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
		if ((com->modem_car_chg_timer = (msr & TIOCM_CAR) ?
		     DCD_ON_RECOGNITION : DCD_OFF_TOLERANCE) != 0)
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
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, 0);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, CTRL8251F_ENABLE);
	COM_INT_ENABLE
}

static void
pc98_i8251_or_cmd(struct com_s *com, int x)
{
	int	tmp;

	COM_INT_DISABLE
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, 0);
	tmp = com->pc98_prev_siocmd | (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, CTRL8251F_ENABLE);
	COM_INT_ENABLE
}

static void
pc98_i8251_set_cmd(struct com_s *com, int x)
{
	int	tmp;

	COM_INT_DISABLE
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, 0);
	tmp = (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, CTRL8251F_ENABLE);
	COM_INT_ENABLE
}

static void
pc98_i8251_clear_or_cmd(struct com_s *com, int clr, int x)
{
	int	tmp;
	COM_INT_DISABLE
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, 0);
	tmp = com->pc98_prev_siocmd & ~(clr);
	tmp |= (x);
	outb(com->cmd_port, tmp);
	com->pc98_prev_siocmd = tmp & ~(CMD8251_ER|CMD8251_RESET|CMD8251_EH);
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, CTRL8251F_ENABLE);
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
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, 0);
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
	DELAY(10);
	if (com->pc98_8251fifo_enable)
	    outb(I8251F_fcr, CTRL8251F_ENABLE |
		 CTRL8251F_XMT_RST | CTRL8251F_RCV_RST);
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
		int	tmp;
		while (!((tmp = inb(com->sts_port)) & STS8251_TxEMP))
			;
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
	int	if_type, effect_sp, count = -1, mod;

	if_type = com->pc98_if_type & 0x0f;

	switch (com->pc98_if_type) {
	case COM_IF_INTERNAL:
	    if (PC98SIO_baud_rate_port(if_type) != -1) {
		count = ttspeedtab(speed, if_8251_type[if_type].speedtab);
		if (count > 0) {
		    count |= COM1_EXT_CLOCK;
		    break;
		}
	    }

	    /* for *1CLK asynchronous! mode, TEFUTEFU */
	    mod = (sysclock == 5) ? 2457600 : 1996800;
	    effect_sp = ttspeedtab( speed, pc98speedtab );
	    if ( effect_sp < 0 )	/* XXX */
		effect_sp = ttspeedtab( (speed - 1), pc98speedtab );
	    if ( effect_sp <= 0 )
		return effect_sp;
	    if ( effect_sp == speed )
		mod /= 16;
	    if ( mod % effect_sp )
		return(-1);
	    count = mod / effect_sp;
	    if ( count > 65535 )
		return(-1);
	    if ( effect_sp != speed )
		count |= 0x10000;
	    break;
	case COM_IF_PC9861K_1:
	case COM_IF_PC9861K_2:
	    count = 1;
	    break;
	case COM_IF_IND_SS_1:
	case COM_IF_IND_SS_2:
	case COM_IF_PIO9032B_1:
	case COM_IF_PIO9032B_2:
	    if ( speed == 0 ) return 0;
	    count = ttspeedtab( speed, if_8251_type[if_type].speedtab );
	    break;
	case COM_IF_B98_01_1:
	case COM_IF_B98_01_2:
	    if ( speed == 0 ) return 0;
	    count = ttspeedtab( speed, if_8251_type[if_type].speedtab );
#ifdef B98_01_OLD
	    if (count == 0 || count == 1) {
		count += 4;
		count |= 0x20000;  /* x1 mode for 76800 and 153600 */
	    }
#endif
	    break;
	}

	return count;
}

static void
pc98_set_baud_rate( struct com_s *com, int count )
{
	int	if_type, io, s;

	if_type = com->pc98_if_type & 0x0f;
	io = rman_get_start(com->ioportres) & 0xff00;

	switch (com->pc98_if_type) {
	case COM_IF_INTERNAL:
	    if (PC98SIO_baud_rate_port(if_type) != -1) {
		if (count & COM1_EXT_CLOCK) {
		    outb((Port_t)PC98SIO_baud_rate_port(if_type), count & 0xff);
		    break;
		} else {
		    outb((Port_t)PC98SIO_baud_rate_port(if_type), 0x09);
		}
	    }

	    if ( count < 0 ) {
		printf( "[ Illegal count : %d ]", count );
		return;
	    } else if ( count == 0 )
		return;
	    /* set i8253 */
	    s = splclock();
	    if (count != 3)
		outb( 0x77, 0xb6 );
	    else
		outb( 0x77, 0xb4 );
	    outb( 0x5f, 0);
	    outb( 0x75, count & 0xff );
	    outb( 0x5f, 0);
	    outb( 0x75, (count >> 8) & 0xff );
	    splx(s);
	    break;
	case COM_IF_IND_SS_1:
	case COM_IF_IND_SS_2:
	    outb(io | PC98SIO_intr_ctrl_port(if_type), 0);
	    outb(io | PC98SIO_baud_rate_port(if_type), 0);
	    outb(io | PC98SIO_baud_rate_port(if_type), 0xc0);
	    outb(io | PC98SIO_baud_rate_port(if_type), (count >> 8) | 0x80);
	    outb(io | PC98SIO_baud_rate_port(if_type), count & 0xff);
	    break;
	case COM_IF_PIO9032B_1:
	case COM_IF_PIO9032B_2:
	    outb(io | PC98SIO_baud_rate_port(if_type), count);
	    break;
	case COM_IF_B98_01_1:
	case COM_IF_B98_01_2:
	    outb(io | PC98SIO_baud_rate_port(if_type), count & 0x0f);
#ifdef B98_01_OLD
	    /*
	     * Some old B98_01 board should be controlled
	     * in different way, but this hasn't been tested yet.
	     */
	    outb(io | PC98SIO_func_port(if_type),
		 (count & 0x20000) ? 0xf0 : 0xf2);
#endif
	    break;
	}
}
static int
pc98_check_if_type(device_t dev, struct siodev *iod)
{
	int	irr, io, if_type, tmp;
	static  short	irq_tab[2][8] = {
		{  3,  5,  6,  9, 10, 12, 13, -1},
		{  3, 10, 12, 13,  5,  6,  9, -1}
	};

	if_type = iod->if_type & 0x0f;
	iod->irq = 0;
	io = isa_get_port(dev) & 0xff00;

	if (IS_8251(iod->if_type)) {
	    if (PC98SIO_func_port(if_type) != -1) {
		outb(io | PC98SIO_func_port(if_type), 0xf2);
		tmp = ttspeedtab(9600, if_8251_type[if_type].speedtab);
		if (tmp != -1 && PC98SIO_baud_rate_port(if_type) != -1)
		    outb(io | PC98SIO_baud_rate_port(if_type), tmp);
	    }

	    iod->cmd  = io | PC98SIO_cmd_port(if_type);
	    iod->sts  = io | PC98SIO_sts_port(if_type);
	    iod->mod  = io | PC98SIO_in_modem_port(if_type);
	    iod->ctrl = io | PC98SIO_intr_ctrl_port(if_type);

	    if (iod->if_type == COM_IF_INTERNAL) {
		iod->irq = 4;

		if (pc98_check_8251vfast()) {
			PC98SIO_baud_rate_port(if_type) = I8251F_div;
			if_8251_type[if_type].speedtab = pc98fast_speedtab;
		}
	    } else {
		tmp = inb( iod->mod ) & if_8251_type[if_type].irr_mask;
		if ((isa_get_port(dev) & 0xff) == IO_COM2)
		    iod->irq = irq_tab[0][tmp];
		else
		    iod->irq = irq_tab[1][tmp];
	    }
	} else {
	    irr = if_16550a_type[if_type].irr_read;
#ifdef COM_MULTIPORT
	    if (!COM_ISMULTIPORT(device_get_flags(dev)) ||
		    device_get_unit(dev) == COM_MPMASTER(device_get_flags(dev)))
#endif
	    if (irr != -1) {
		tmp = inb(io | irr);
		if (isa_get_port(dev) & 0x01)	/* XXX depend on RSB-384 */
		    iod->irq = irq_tab[1][tmp >> 3];
		else
		    iod->irq = irq_tab[0][tmp & 0x07];
	    }
	}
	if ( iod->irq == -1 ) return -1;

	return 0;
}
static void
pc98_set_ioport(struct com_s *com)
{
	int	if_type = com->pc98_if_type & 0x0f;
	Port_t	io = rman_get_start(com->ioportres) & 0xff00;

	pc98_check_sysclock();
	com->data_port		= io | PC98SIO_data_port(if_type);
	com->cmd_port		= io | PC98SIO_cmd_port(if_type);
	com->sts_port		= io | PC98SIO_sts_port(if_type);
	com->in_modem_port	= io | PC98SIO_in_modem_port(if_type);
	com->intr_ctrl_port	= io | PC98SIO_intr_ctrl_port(if_type);
}
static int
pc98_check_8251vfast(void)
{
    int	i;

    outb(I8251F_div, 0x8c);
    DELAY(10);
    for (i = 0; i < 100; i++) {
	if ((inb(I8251F_div) & 0x80) != 0) {
	    i = 0;
	    break;
	}
	DELAY(1);
    }
    outb(I8251F_div, 0);
    DELAY(10);
    for (; i < 100; i++) {
	if ((inb(I8251F_div) & 0x80) == 0)
	    return 1;
	DELAY(1);
    }

    return 0;
}
static int
pc98_check_8251fifo(void)
{
    u_char	tmp1, tmp2;

    tmp1 = inb(I8251F_iir);
    DELAY(10);
    tmp2 = inb(I8251F_iir);
    if (((tmp1 ^ tmp2) & 0x40) != 0 && ((tmp1 | tmp2) & 0x20) == 0)
	return 1;

    return 0;
}
#endif /* PC98 defined */
