/*
 * cyclades cyclom-y serial driver
 *	Andrew Herbert <andrew@werple.apana.org.au>, 17 August 1993
 *
 * Copyright (c) 1993 Andrew Herbert.
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
 * 3. The name Andrew Herbert may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: cy.c,v 1.5 1995/03/28 07:55:25 bde Exp $
 */

/*
 * Device minor number encoding:
 *
 *	c c x x u u u u		- bits in the minor device number
 *
 *	bits	meaning
 *	----	-------
 *	uuuu	physical serial line (i.e. unit) to use
 *			0-7 on a cyclom-8Y, 0-15 on a cyclom-16Y
 *	xx	unused
 *	cc	carrier control mode
 *			00	complete hardware carrier control of the tty.
 *				DCD must be high for the open(2) to complete.
 *			01	dialin pseudo-device (not yet implemented)
 *			10	carrier ignored until a high->low transition
 *			11	carrier completed ignored
 */

/*
 * Known deficiencies:
 *
 *	* no BREAK handling - breaks are ignored, and can't be sent either
 *	* no support for bad-char reporting, except via PARMRK
 *	* no support for dialin + dialout devices
 */

#include "cy.h"
#if NCY > 0

/* This disgusing hack because we actually have 16 units on one controller */
#if NCY < 2
#undef NCY
#define NCY (16)
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <machine/cpu.h>
#ifdef NetBSD
#include <machine/pio.h>
#include <machine/cpufunc.h>
#else
#include <machine/clock.h>
#endif

#include <i386/isa/isa_device.h>
#include <i386/isa/ic/cd1400.h>

#define RxFifoThreshold	3	/* 3 characters (out of 12) in the receive
				 * FIFO before an interrupt is generated
				 */
#define	FastRawInput	/* bypass the regular char-by-char canonical input
			 * processing whenever possible
			 */
#define	PollMode	/* use polling-based irq service routine, not the
			 * hardware svcack lines.  Must be defined for
			 * cyclom-16y boards.
			 * 
			 * XXX cyclom-8y doesn't work without this defined
			 * either (!)
			 */
#define	LogOverruns	/* log receive fifo overruns */
#undef	TxBuffer	/* buffer driver output, to be slightly more
			 * efficient
			 *
			 * XXX presently buggy
			 */
#undef	Smarts		/* enable slightly more CD1400 intelligence.  Mainly
			 * the output CR/LF processing, plus we can avoid a
			 * few checks usually done in ttyinput().
			 *
			 * XXX not yet implemented, and not particularly
			 * worthwhile either.
			 */
#define CyDebug		/* include debugging code (minimal effect on
			 * performance)
			 */

#define CY_RX_BUFS		2	/* two receive buffers per port */
#define	CY_RX_BUF_SIZE		256	/* bytes per receive buffer */
#define	CY_TX_BUF_SIZE		512	/* bytes per transmit buffer */

/* #define CD1400s_PER_CYCLOM	1 */	/* cyclom-4y */
/* #define CD1400s_PER_CYCLOM	2 */	/* cyclom-8y */
#define CD1400s_PER_CYCLOM	4	/* cyclom-16y */

/* FreeBSD's getty doesn't know option for setting RTS/CTS handshake.  Its
   getty, like a lot of other old cruft, should be replaced with something
   which used POSIX tty interfaces which at least allow enabling it.  In the
   meantime, use the force. */
#define ALWAYS_RTS_CTS	1

#if CD1400s_PER_CYCLOM < 4
#define CD1400_MEMSIZE		0x400	/* 4*256 bytes per chip: cyclom-[48]y */
#else
#define CD1400_MEMSIZE		0x100	/* 256 bytes per chip: cyclom-16y */
					/* XXX or is it 0x400 like the rest? */
#define CYCLOM_16	1		/* This is a cyclom-16Y */
#endif

#define PORTS_PER_CYCLOM	(CD1400_NO_OF_CHANNELS * CD1400s_PER_CYCLOM)
#define CYCLOM_RESET_16		0x1400			/* cyclom-16y reset */
#define CYCLOM_CLEAR_INTR	0x1800			/* intr ack address */
#define CYCLOM_CLOCK		25000000		/* baud rate clock */

#define	CY_UNITMASK		0x0f
#define	CY_CARRIERMASK		0xC0
#define CY_CARRIERSHIFT		6

#define	UNIT(x)		(minor(x) & CY_UNITMASK)
#define	CARRIER_MODE(x)	((minor(x) & CY_CARRIERMASK) >> CY_CARRIERSHIFT)

typedef u_char * volatile cy_addr;

int		cyprobe(struct isa_device *dev);
int		cyattach(struct isa_device *isdp);
void		cystart(struct tty *tp);
int		cyparam(struct tty *tp, struct termios *t);
int		cyspeed(int speed, int *prescaler_io);
static void	cy_channel_init(dev_t dev, int reset);
static void	cd1400_channel_cmd(cy_addr base, u_char cmd);

/* hsu@clinet.fi: sigh */
#ifdef __NetBSD__
#define DELAY(foo) delay(foo)
void		delay(int delay);
#endif

/* Better get rid of this until the core people agree on kernel interfaces.
   At least it will then compile on both WhichBSDs.
 */ 
#if 0
extern unsigned int	delaycount;	/* calibrated 1 ms cpu-spin delay */
#endif

struct	isa_driver cydriver = {
	cyprobe, cyattach, "cy"
};

/* low-level ping-pong buffer structure */

struct cy_buf {
	u_char		*next_char;	/* location of next char to write */
	u_int		free;		/* free chars remaining in buffer */
	struct cy_buf	*next_buf;	/* circular, you know */
	u_char		buf[CY_RX_BUF_SIZE];	/* start of the buffer */
};

/* low-level ring buffer */

#ifdef TxBuffer
struct cy_ring {
	u_char		buf[CY_TX_BUF_SIZE];
	u_char		*head;
	u_char		*tail;		/* next pos. to insert char */
	u_char		*endish;	/* physical end of buf */
	u_int		used;		/* no. of chars in queue */
};
#endif


/*
 * define a structure to keep track of each serial line
 */

struct cy {
	cy_addr		base_addr;	/* base address of this port's cd1400 */
	struct tty	*tty;
	u_int		dtrwait;	/* time (in ticks) to hold dtr low after close */
	u_int		recv_exception;	/* exception chars received */
	u_int		recv_normal;	/* normal chars received */
	u_int		xmit;		/* chars transmitted */
	u_int		mdm;		/* modem signal changes */
#ifdef CyDebug
	u_int		start_count;	/* no. of calls to cystart() */
	u_int		start_real;	/* no. of calls that did something */
#endif
	u_char		carrier_mode;	/* hardware carrier handling mode */
					/*
					 * 0 = always use
					 * 1 = always use (dialin port)
					 * 2 = ignore during open, then use it
					 * 3 = ignore completely
					 */
	u_char		carrier_delta;	/* true if carrier has changed state */
	u_char		fifo_overrun;	/* true if cd1400 receive fifo has... */
	u_char		rx_buf_overrun;	/* true if low-level buf overflow */
	u_char		intr_enable;	/* CD1400 SRER shadow */
	u_char		modem_sig;	/* CD1400 modem signal shadow */
	u_char		channel_control;/* CD1400 CCR control command shadow */
	u_char		cor[3];		/* CD1400 COR1-3 shadows */
#ifdef Smarts
	u_char		spec_char[4];	/* CD1400 SCHR1-4 shadows */
#endif
	struct cy_buf	*rx_buf;		/* current receive buffer */
	struct cy_buf	rx_buf_pool[CY_RX_BUFS];/* receive ping-pong buffers */
#ifdef TxBuffer
	struct cy_ring	tx_buf;		/* transmit buffer */
#endif
};

int	cydefaultrate = TTYDEF_SPEED;
cy_addr	cyclom_base;			/* base address of the card */
static	struct cy *info[NCY*PORTS_PER_CYCLOM];
#ifdef __FreeBSD__ /* XXX actually only temporarily for 2.1-Development */
struct	tty cy_tty[NCY*PORTS_PER_CYCLOM];
#else
struct	tty *cy_tty[NCY*PORTS_PER_CYCLOM];
#endif
static	volatile u_char timeout_scheduled = 0;	/* true if a timeout has been scheduled */

#ifdef CyDebug
u_int	cy_svrr_probes = 0;		/* debugging */
u_int	cy_timeouts = 0;
u_int	cy_timeout_req = 0;
#endif

/**********************************************************************/

int
cyprobe(struct isa_device *dev)
{
    int		i, j;
    u_char	version = 0;	/* firmware version */

    /* Cyclom-16Y hardware reset (Cyclom-8Ys don't care) */
    i = *(cy_addr)(dev->id_maddr + CYCLOM_RESET_16);

    DELAY(500);	/* wait for the board to get its act together (500 us) */

    for (i = 0; i < CD1400s_PER_CYCLOM; i++) {
	cy_addr	base = dev->id_maddr + i * CD1400_MEMSIZE;

	/* wait for chip to become ready for new command */
	for (j = 0; j < 100; j += 50) {
	    DELAY(50);	/* wait 50 us */

	    if (!*(base + CD1400_CCR))
	    	break;
	}

	/* clear the GFRCR register */
	*(base + CD1400_GFRCR) = 0;

	/* issue a reset command */
	*(base + CD1400_CCR) = CD1400_CMD_RESET;

	/* wait for the CD1400 to initialise itself */
	for (j = 0; j < 1000; j += 50) {
	    DELAY(50);	/* wait 50 us */

	    /* retrieve firmware version */
	    version = *(base + CD1400_GFRCR);
	    if (version)
		break;
	}

	/* anything in the 40-4f range is fine */
	if ((version & 0xf0) != 0x40) {
	    return 0;
	}
    }

    return 1;	/* found */
}


int
cyattach(struct isa_device *isdp)
{
/*    u_char	unit = UNIT(isdp->id_unit); */
    int		i, j, k;

    /* global variable used various routines */
    cyclom_base = (cy_addr)isdp->id_maddr;

    for (i = 0, k = 0; i < CD1400s_PER_CYCLOM; i++) {
	cy_addr	base = cyclom_base + i * CD1400_MEMSIZE;

	/* setup a 1ms clock tick */
	*(base + CD1400_PPR) = CD1400_CLOCK_25_1MS;

	for (j = 0; j < CD1400_NO_OF_CHANNELS; j++, k++) {
	    struct cy	*ip;

	    /*
	     * grab some space.  it'd be more polite to do this in cyopen(),
	     * but hey.
	     */
	    info[k] = ip = malloc(sizeof(struct cy), M_DEVBUF, M_WAITOK);

	    /* clear all sorts of junk */
	    bzero(ip, sizeof(struct cy));

	    ip->base_addr = base;

	    /* initialise the channel, without resetting it first */
	    cy_channel_init(k, 0);
	}
    }

    /* clear interrupts */
    *(cyclom_base + CYCLOM_CLEAR_INTR) = (u_char)0;

    return 1;
}


int
cyopen(dev_t dev, int flag, int mode, struct proc *p)
{
	u_int		unit = UNIT(dev);
	struct cy	*infop;
	cy_addr		base;
	struct tty	*tp;
	int		error = 0;
	u_char		carrier;

	if (unit >= /* NCY * ? */ PORTS_PER_CYCLOM)
		return (ENXIO);

	infop = info[unit];
	base = infop->base_addr;
#ifdef __FreeBSD__
	infop->tty = &cy_tty[unit];
#else
	if (!cy_tty[unit])
	    infop->tty = cy_tty[unit] = ttymalloc();
#endif
	tp = infop->tty;

	tp->t_oproc = cystart;
	tp->t_param = cyparam;
	tp->t_dev = dev;
	if (!(tp->t_state & TS_ISOPEN)) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cydefaultrate;
		}

		(void) spltty();
		cy_channel_init(unit, 1);	/* reset the hardware */

		/*
		 * raise dtr and generally set things up correctly.  this
		 * has the side-effect of selecting the appropriate cd1400
		 * channel, to help us with subsequent channel control stuff
		 */
		cyparam(tp, &tp->t_termios);

		/* check carrier, and set t_state's TS_CARR_ON flag accordingly */
		infop->modem_sig = *(base + CD1400_MSVR);
		carrier = infop->modem_sig & CD1400_MSVR_CD;

		if (carrier || (infop->carrier_mode >= 2))
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &=~ TS_CARR_ON;

		/*
		 * enable modem & rx interrupts - relies on cyparam()
		 * having selected the appropriate cd1400 channel
		 */
		infop->intr_enable = (1 << 7) | (1 << 4);
		*(base + CD1400_SRER) = infop->intr_enable;

		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	if (!(flag & O_NONBLOCK))
		while (!(tp->t_cflag & CLOCAL) &&
		       !(tp->t_state & TS_CARR_ON) && !error)
			error = ttysleep(tp, (caddr_t)&tp->t_rawq,
		       			TTIPRI|PCATCH, ttopen, 0);
	(void) spl0();

	if (!error)
		error = (*linesw[(u_char)tp->t_line].l_open)(dev, tp);
	return (error);
} /* end of cyopen() */


void
cyclose_wakeup(void *arg)
{
	wakeup(arg);
} /* end of cyclose_wakeup() */


int
cyclose(dev_t dev, int flag, int mode, struct proc *p)
{
	u_int		unit = UNIT(dev);
	struct cy	*infop = info[unit];
	struct tty	*tp = infop->tty;
	cy_addr		base = infop->base_addr;
	int		s;

	(*linesw[(u_char)tp->t_line].l_close)(tp, flag);

	s = spltty();
	/* select the appropriate channel on the CD1400 */
	*(base + CD1400_CAR) = (u_char)(unit & 0x03);

	/* disable this channel and lower DTR */
	infop->intr_enable = 0;
	*(base + CD1400_SRER) = (u_char)0;			/* no intrs */
	*(base + CD1400_DTR) = (u_char)CD1400_DTR_CLEAR;	/* no DTR */
	infop->modem_sig &= ~CD1400_MSVR_DTR;

	/* disable receiver (leave transmitter enabled) */
	infop->channel_control = (1 << 4) | (1 << 3) | 1;
	cd1400_channel_cmd(base, infop->channel_control);
	splx(s);

	ttyclose(tp);
#ifdef broken /* session holds a ref to the tty; can't deallocate */
	ttyfree(tp);
	infop->tty = cy_tty[unit] = (struct tty *)NULL;
#endif

	if (infop->dtrwait) {
		int error;

		timeout(cyclose_wakeup, (caddr_t)&infop->dtrwait, infop->dtrwait);
		do {
			error = tsleep((caddr_t)&infop->dtrwait,
					TTIPRI|PCATCH, "cyclose", 0);
		} while (error == ERESTART);
	}

	return 0;
} /* end of cyclose() */


int
cyread(dev_t dev, struct uio *uio, int flag)
{
	u_int		unit = UNIT(dev);
	struct tty	*tp = info[unit]->tty;

	return (*linesw[(u_char)tp->t_line].l_read)(tp, uio, flag);
} /* end of cyread() */


int
cywrite(dev_t dev, struct uio *uio, int flag)
{
	u_int		unit = UNIT(dev);
	struct tty	*tp = info[unit]->tty;

#ifdef Smarts
	/* XXX duplicate ttwrite(), but without so much output processing on
	 * CR & LF chars.  Hardly worth the effort, given that high-throughput
	 * sessions are raw anyhow.
	 */
#else
	return (*linesw[(u_char)tp->t_line].l_write)(tp, uio, flag);
#endif
} /* end of cywrite() */


#ifdef Smarts
/* standard line discipline input routine */
int
cyinput(int c, struct tty *tp)
{
	/* XXX duplicate ttyinput(), but without the IXOFF/IXON/ISTRIP/IPARMRK
	 * bits, as they are done by the CD1400.  Hardly worth the effort,
	 * given that high-throughput sessions are raw anyhow.
	 */
} /* end of cyinput() */
#endif /* Smarts */


inline static void
service_upper_rx(int unit)
{
	struct	cy *ip = info[unit];
	struct	tty *tp = ip->tty;
	struct	cy_buf *buf;
	int		i;
	u_char	*ch;

	buf = ip->rx_buf;

	/* give service_rx() a new one */
	disable_intr();		/* faster than spltty() */
	ip->rx_buf = buf->next_buf;
	enable_intr();

	if (tp->t_state & TS_ISOPEN) {
	    ch = buf->buf;
	    i = buf->next_char - buf->buf;

#ifdef FastRawInput
	    /* try to avoid calling the line discipline stuff if we can */
	    if ((tp->t_line == 0) &&
		    !(tp->t_iflag & (ICRNL | IMAXBEL | INLCR)) && 
		    !(tp->t_lflag & (ECHO | ECHONL | ICANON | IEXTEN |
			ISIG | PENDIN)) &&
		    !(tp->t_state & (TS_CNTTB | TS_LNCH))) {

		i = b_to_q(ch, i, &tp->t_rawq);
		if (i) {
			/*
			 * we have no RTS flow control support on cy-8
			 * boards, so this is really just tough luck
			 */

			log(LOG_WARNING, "cy%d: tty input queue overflow\n",
			    unit);
		}

		ttwakeup(tp);	/* notify any readers */
	    }
	    else
#endif /* FastRawInput */
	    {
		while (i--)
		    (*linesw[(u_char)tp->t_line].l_rint)((int)*ch++, tp);
	    }
	}

	/* clear the buffer we've just processed */
	buf->next_char = buf->buf;
	buf->free = CY_RX_BUF_SIZE;
} /* end of service_upper_rx() */


#ifdef TxBuffer
static void
service_upper_tx(int unit)
{
	struct	cy *ip = info[unit];
	struct	tty *tp = ip->tty;

	tp->t_state &=~ (TS_BUSY|TS_FLUSH);

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}

	if (tp->t_outq.c_cc > 0) {
		struct	cy_ring *txq = &ip->tx_buf;
		int	free_count = CY_TX_BUF_SIZE - ip->tx_buf.used;
		u_char	*cp = txq->tail;
		int	count;
		int	chars_done;

		tp->t_state |= TS_BUSY;

		/* find the largest contig. copy we can do */
		count = ((txq->endish - cp) > free_count) ?
			    free_count : txq->endish - cp;

		count = ((cp + free_count) > txq->endish) ?
			    txq->endish - cp : free_count;

		/* copy the first slab */
		chars_done = q_to_b(&tp->t_outq, cp, count);

		/* check for wrap-around time */
		cp += chars_done;
		if (cp == txq->endish)
			cp = txq->buf;		/* back to the start */

		/* copy anything else, after we've wrapped around */
		if ((chars_done == count) && (count != free_count)) {
			/* copy the second slab */
			count = q_to_b(&tp->t_outq, cp, free_count - count);
			cp += count;
			chars_done += count;
		}

		/*
		 * update queue, protecting ourselves from any rampant
		 * lower-layers
		 */
		disable_intr();
		txq->tail = cp;
		txq->used += chars_done;
		enable_intr();
	}

	if (!tp->t_outq.c_cc)
		tp->t_state &=~ TS_BUSY;
} /* end of service_upper_tx() */
#endif /* TxBuffer */


inline static void
service_upper_mdm(int unit)
{
	struct	cy *ip = info[unit];

	if (ip->carrier_delta) {
		int	carrier = ip->modem_sig & CD1400_MSVR_CD;
		struct	tty *tp = ip->tty;

		if (!(*linesw[(u_char)tp->t_line].l_modem)(tp, carrier)) {
			cy_addr	base = ip->base_addr;

			/* clear DTR */
			disable_intr();
			*(base + CD1400_CAR) = (u_char)(unit & 0x03);
			*(base + CD1400_DTR) = (u_char)CD1400_DTR_CLEAR;
			ip->modem_sig &= ~CD1400_MSVR_DTR;
			ip->carrier_delta = 0;
			enable_intr();
		}
		else {
			disable_intr();
			ip->carrier_delta = 0;
			enable_intr();
		}
	}
} /* end of service_upper_mdm() */


/* upper level character processing routine */
void
cytimeout(void *ptr)
{
	int	unit;

	timeout_scheduled = 0;

#ifdef CyDebug
	cy_timeouts++;
#endif

	/* check each port in turn */
	for (unit = 0; unit < NCY*PORTS_PER_CYCLOM; unit++) {
		struct	cy *ip = info[unit];
#ifndef TxBuffer
		struct	tty *tp = ip->tty;
#endif

		/* ignore anything that is not open */
		if (!ip->tty)
			continue;

		/*
		 * any received chars to handle? (doesn't matter if intr routine
		 * kicks in while we're testing this)
		 */
		if (ip->rx_buf->free != CY_RX_BUF_SIZE)
			service_upper_rx(unit);

#ifdef TxBuffer
		/* anything to add to the transmit buffer (low-water mark)? */
		if (ip->tx_buf.used < CY_TX_BUF_SIZE/2)
			service_upper_tx(unit);
#else
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state&TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)&tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
#endif

		/* anything modem signals altered? */
		service_upper_mdm(unit);

		/* any overruns to log? */
#ifdef LogOverruns
		if (ip->fifo_overrun) {
			/*
			 * turn off the alarm - not important enough to bother
			 * with interrupt protection.
			 */
			ip->fifo_overrun = 0;

			log(LOG_WARNING, "cy%d: receive fifo overrun\n", unit);
		}
#endif
		if (ip->rx_buf_overrun) {
			/*
			 * turn off the alarm - not important enough to bother
			 * with interrupt protection.
			 */
			ip->rx_buf_overrun = 0;

			log(LOG_WARNING, "cy%d: receive buffer full\n", unit);
		}
	}
} /* cytimeout() */


inline static void
schedule_upper_service(void)
{
#ifdef CyDebug
    cy_timeout_req++;
#endif

    if (!timeout_scheduled) {
	timeout(cytimeout, (caddr_t)0, 1);	/* call next tick */
	timeout_scheduled = 1;
    }
} /* end of schedule_upper_service() */


/* initialise a channel on the cyclom board */

static void
cy_channel_init(dev_t dev, int reset)
{
	u_int	unit = UNIT(dev);
	int	carrier_mode = CARRIER_MODE(dev);
	struct	cy *ip = info[unit];
	cy_addr	base = ip->base_addr;
	struct	tty *tp = ip->tty;
	struct	cy_buf *buf, *next_buf;
	int	i;
#ifndef PollMode
	u_char	cd1400_unit;
#endif

	/* clear the structure and refill it */
	bzero(ip, sizeof(struct cy));
	ip->base_addr = base;
	ip->tty = tp;
	ip->carrier_mode = carrier_mode;

	/* select channel of the CD1400 */
	*(base + CD1400_CAR) = (u_char)(unit & 0x03);

	if (reset)
		cd1400_channel_cmd(base, 0x80);	/* reset the channel */

	/* set LIVR to 0 - intr routines depend on this */
	*(base + CD1400_LIVR) = 0;

#ifndef PollMode
	/* set top four bits of {R,T,M}ICR to the cd1400
	 * number, cd1400_unit
	 */
	cd1400_unit = unit / CD1400_NO_OF_CHANNELS;
	*(base + CD1400_RICR) = (u_char)(cd1400_unit << 4);
	*(base + CD1400_TICR) = (u_char)(cd1400_unit << 4);
	*(base + CD1400_MICR) = (u_char)(cd1400_unit << 4);
#endif

	ip->dtrwait = hz/4;	/* quarter of a second */

	/* setup low-level buffers */
	i = CY_RX_BUFS;
	ip->rx_buf = next_buf = &ip->rx_buf_pool[0];
	while (i--) {
		buf = &ip->rx_buf_pool[i];

		buf->next_char = buf->buf;	/* first char to use */
		buf->free = CY_RX_BUF_SIZE;	/* i.e. empty */
		buf->next_buf = next_buf;	/* where to go next */
		next_buf = buf;
	}

#ifdef TxBuffer
	ip->tx_buf.endish = ip->tx_buf.buf + CY_TX_BUF_SIZE;

	/* clear the low-level tx buffer */
	ip->tx_buf.head = ip->tx_buf.tail = ip->tx_buf.buf;
	ip->tx_buf.used = 0;
#endif

	/* clear the low-level rx buffer */
	ip->rx_buf->next_char = ip->rx_buf->buf;	/* first char to use */
	ip->rx_buf->free = CY_RX_BUF_SIZE;		/* completely empty */
} /* end of cy_channel_init() */


/* service a receive interrupt */
inline static void
service_rx(int cd, caddr_t base)
{
	struct cy	*infop;
	unsigned	count;
	int		ch;
	u_char		serv_type, channel;
#ifdef PollMode
	u_char		save_rir, save_car;
#endif

	/* setup */
#ifdef PollMode
	save_rir = *(base + CD1400_RIR);
	channel = cd * CD1400_NO_OF_CHANNELS + (save_rir & 0x3);
	save_car = *(base + CD1400_CAR);
	*(base + CD1400_CAR) = save_rir;	/* enter modem service */
	serv_type = *(base + CD1400_RIVR);
#else
	serv_type = *(base + CD1400_SVCACKR);	/* ack receive service */
	channel = ((u_char)*(base + CD1400_RICR)) >> 2;	/* get cyclom channel # */

#ifdef CyDebug
	if (channel >= PORTS_PER_CYCLOM) {
	    printf("cy: service_rx - channel %02x\n", channel);
	    panic("cy: service_rx - bad channel");
	}
#endif
#endif

	infop = info[channel];

	/* read those chars */
	if (serv_type & CD1400_RIVR_EXCEPTION) {
		/* read the exception status */
		u_char status = *(base + CD1400_RDSR);

		/* XXX is it a break?  Do something if it is! */

		/* XXX is IGNPAR not set?  Store a null in the buffer. */

#ifdef LogOverruns
		if (status & CD1400_RDSR_OVERRUN) {
#if 0
			ch |= TTY_PE;		/* for SLIP */
#endif
			infop->fifo_overrun++;
		}
#endif
		infop->recv_exception++;
	}
	else {
		struct	cy_buf *buf = infop->rx_buf;

		count = (u_char)*(base + CD1400_RDCR);	/* how many to read? */
		infop->recv_normal += count;
		if (buf->free < count) {
			infop->rx_buf_overrun += count;

			/* read & discard everything */
			while (count--)
				ch = (u_char)*(base + CD1400_RDSR);
		}
		else {
			/* slurp it into our low-level buffer */
			buf->free -= count;
			while (count--) {
				ch = (u_char)*(base + CD1400_RDSR);	/* read the char */
				*(buf->next_char++) = ch;
			}
		}
	}

#ifdef PollMode
	*(base + CD1400_RIR) = (u_char)(save_rir & 0x3f);	/* terminate service context */
#else
	*(base + CD1400_EOSRR) = (u_char)0;	/* terminate service context */
#endif
} /* end of service_rx */


/* service a transmit interrupt */
inline static void
service_tx(int cd, caddr_t base)
{
	struct cy	*ip;
#ifdef TxBuffer
	struct cy_ring	*txq;
#else
	struct tty	*tp;
#endif
	u_char		channel;
#ifdef PollMode
	u_char		save_tir, save_car;
#else
	u_char		vector;
#endif

	/* setup */
#ifdef PollMode
	save_tir = *(base + CD1400_TIR);
	channel = cd * CD1400_NO_OF_CHANNELS + (save_tir & 0x3);
	save_car = *(base + CD1400_CAR);
	*(base + CD1400_CAR) = save_tir;	/* enter tx service */
#else
	vector = *(base + CD1400_SVCACKT);	/* ack transmit service */
	channel = ((u_char)*(base + CD1400_TICR)) >> 2;	/* get cyclom channel # */

#ifdef CyDebug
	if (channel >= PORTS_PER_CYCLOM) {
	    printf("cy: service_tx - channel %02x\n", channel);
	    panic("cy: service_tx - bad channel");
	}
#endif
#endif

	ip = info[channel];
#ifdef TxBuffer
	txq = &ip->tx_buf;

	if (txq->used > 0) {
		cy_addr	base = ip->base_addr;
		int	count = min(CD1400_FIFOSIZE, txq->used);
		int	chars_done = count;
		u_char	*cp = txq->head;
		u_char	*buf_end = txq->endish;

		/* ip->state |= CY_BUSY; */
		while (count--) {
			*(base + CD1400_TDR) = *cp++;
			if (cp >= buf_end)
				cp = txq->buf;
		};
		txq->head = cp;
		txq->used -= chars_done; /* important that this is atomic */
		ip->xmit += chars_done;
	}

	/*
	 * disable tx intrs if no more chars to send.  we re-enable
	 * them in cystart()
	 */
	if (!txq->used) {
		ip->intr_enable &=~ (1 << 2);
		*(base + CD1400_SRER) = ip->intr_enable;
		/* ip->state &= ~CY_BUSY; */
	}
#else
	tp = ip->tty;

	if (!(tp->t_state & TS_TTSTOP) && (tp->t_outq.c_cc > 0)) {
		cy_addr	base = ip->base_addr;
		int	count = min(CD1400_FIFOSIZE, tp->t_outq.c_cc);

		ip->xmit += count;
		tp->t_state |= TS_BUSY;
		while (count--)
			*(base + CD1400_TDR) = getc(&tp->t_outq);
	}

	/*
	 * disable tx intrs if no more chars to send.  we re-enable them
	 * in cystart()
	 */
	if (!tp->t_outq.c_cc) {
		ip->intr_enable &=~ (1 << 2);
		*(base + CD1400_SRER) = ip->intr_enable;
		tp->t_state &= ~TS_BUSY;
	}
#endif

#ifdef PollMode
	*(base + CD1400_TIR) = (u_char)(save_tir & 0x3f);	/* terminate service context */
#else
	*(base + CD1400_EOSRR) = (u_char)0;	/* terminate service context */
#endif
} /* end of service_tx */


/* service a modem status interrupt */
inline static void
service_mdm(int cd, caddr_t base)
{
	struct cy	*infop;
	u_char		channel, deltas;
#ifdef PollMode
	u_char		save_mir, save_car;
#else
	u_char		vector;
#endif

	/* setup */
#ifdef PollMode
	save_mir = *(base + CD1400_MIR);
	channel = cd * CD1400_NO_OF_CHANNELS + (save_mir & 0x3);
	save_car = *(base + CD1400_CAR);
	*(base + CD1400_CAR) = save_mir;	/* enter modem service */
#else
	vector = *(base + CD1400_SVCACKM);	/* ack modem service */
	channel = ((u_char)*(base + CD1400_MICR)) >> 2;	/* get cyclom channel # */

#ifdef CyDebug
	if (channel >= PORTS_PER_CYCLOM) {
	    printf("cy: service_mdm - channel %02x\n", channel);
	    panic("cy: service_mdm - bad channel");
	}
#endif
#endif

	infop = info[channel];

	/* read the siggies and see what's changed */
	infop->modem_sig = (u_char)*(base + CD1400_MSVR);
	deltas = (u_char)*(base + CD1400_MISR);

	if ((infop->carrier_mode <= 2) && (deltas & CD1400_MISR_CDd))
		/* something for the upper layer to deal with */
		infop->carrier_delta = 1;

	infop->mdm++;

	/* terminate service context */
#ifdef PollMode
	*(base + CD1400_MIR) = (u_char)(save_mir & 0x3f);
#else
	*(base + CD1400_EOSRR) = (u_char)0;
#endif
} /* end of service_mdm */


int
cyintr(int unit)
{
    int		cd;
    u_char	status;

    /* check each CD1400 in turn */
    for (cd = 0; cd < CD1400s_PER_CYCLOM; cd++) {
	cy_addr	base = cyclom_base + cd*CD1400_MEMSIZE;

	/* poll to see if it has any work */
	while (status = (u_char)*(base + CD1400_SVRR)) {
#ifdef CyDebug
	    cy_svrr_probes++;
#endif
	    /* service requests as appropriate, giving priority to RX */
	    if (status & CD1400_SVRR_RX)
		    service_rx(cd, base);
	    if (status & CD1400_SVRR_TX)
		    service_tx(cd, base);
	    if (status & CD1400_SVRR_MDM)
		    service_mdm(cd, base);
	}
    }

    /* request upper level service to deal with whatever happened */
    schedule_upper_service();

    /* re-enable interrupts on the cyclom */
    *(cyclom_base + CYCLOM_CLEAR_INTR) = (u_char)0;

    return 1;
}


int
cyioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	int		unit = UNIT(dev);
	struct cy	*infop = info[unit];
	struct tty	*tp = infop->tty;
	int		error;

	error = (*linesw[(u_char)tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag
#ifdef NetBSD
			, p
#endif
			);
	if (error >= 0)
		return (error);

	switch (cmd) {
#ifdef notyet	/* sigh - more junk to do XXX */
	case TIOCSBRK:
		break;
	case TIOCCBRK:
		break;
	case TIOCSDTR:
		break;
	case TIOCCDTR:
		break;

	case TIOCMSET:
		break;
	case TIOCMBIS:
		break;
	case TIOCMBIC:
		break;
#endif /* notyet */

	case TIOCMGET: {
		int	bits = 0;
		u_char	status = infop->modem_sig;

		if (status & CD1400_MSVR_DTR) bits |= TIOCM_DTR | TIOCM_RTS;
		if (status & CD1400_MSVR_CD) bits |= TIOCM_CD;
		if (status & CD1400_MSVR_CTS) bits |= TIOCM_CTS;
		if (status & CD1400_MSVR_DSR) bits |= TIOCM_DSR;
#ifdef CYCLOM_16
		if (status & CD1400_MSVR_RI) bits |= TIOCM_RI;
#endif
		if (infop->channel_control & 0x02) bits |= TIOCM_LE;
		*(int *)data = bits;
		break;
	}

#ifdef TIOCMSBIDIR
	case TIOCMSBIDIR:
		return (ENOTTY);
#endif /* TIOCMSBIDIR */

#ifdef TIOCMGBIDIR
	case TIOCMGBIDIR:
		return (ENOTTY);
#endif /* TIOCMGBIDIR */

#ifdef TIOCMSDTRWAIT
	case TIOCMSDTRWAIT:
		/* must be root to set dtr delay */
			if (p->p_ucred->cr_uid != 0)
				return(EPERM);

			infop->dtrwait = *(u_int *)data;
			break;
#endif /* TIOCMSDTRWAIT */

#ifdef TIOCMGDTRWAIT
	case TIOCMGDTRWAIT:
		*(u_int *)data = infop->dtrwait;
		break;
#endif /* TIOCMGDTRWAIT */

	default:
		return (ENOTTY);
	}

	return 0;
} /* end of cyioctl() */


int
cyparam(struct tty *tp, struct termios *t)
{
	u_char		unit = UNIT(tp->t_dev);
	struct cy	*infop = info[unit];
	cy_addr		base = infop->base_addr;
	int		cflag = t->c_cflag;
	int		iflag = t->c_iflag;
	int		ispeed, ospeed;
	int		itimeout;
	int		iprescaler, oprescaler;
	int		s;
	u_char		cor_change = 0;
	u_char		opt;

	if (!t->c_ispeed)
	    t->c_ispeed = t->c_ospeed;

	s = spltty();

	/* select the appropriate channel on the CD1400 */
	*(base + CD1400_CAR) = unit & 0x03;

	/* handle DTR drop on speed == 0 trick */
	if (t->c_ospeed == 0) {
	    *(base + CD1400_DTR) = CD1400_DTR_CLEAR;
	    infop->modem_sig &= ~CD1400_MSVR_DTR;
	}
	else {
	    *(base + CD1400_DTR) = CD1400_DTR_SET;
	    infop->modem_sig |= CD1400_MSVR_DTR;
	}

	/* set baud rates if they've changed from last time */

	if ((ospeed = cyspeed(t->c_ospeed, &oprescaler)) < 0)
	    return EINVAL;
	*(base + CD1400_TBPR) = (u_char)ospeed;
	*(base + CD1400_TCOR) = (u_char)oprescaler;

	if ((ispeed = cyspeed(t->c_ispeed, &iprescaler)) < 0)
	    return EINVAL;
	*(base + CD1400_RBPR) = (u_char)ispeed;
	*(base + CD1400_RCOR) = (u_char)iprescaler;

	/*
	 * set receive time-out period
	 *	generate a rx interrupt if no new chars are received in
	 *	this many ticks
	 * don't bother comparing old & new VMIN, VTIME and ispeed - it
	 * can't be much worse just to calculate and set it each time!
	 * certainly less hassle. :-)
	 */

	/*
	 * calculate minimum timeout period:
	 *     5 ms or the time it takes to receive 1 char, rounded up to the
	 *     next ms, whichever is greater
	 */
	if (t->c_ispeed > 0) {
	    itimeout = (t->c_ispeed > 2200) ? 5 : (10000/t->c_ispeed + 1);

	    /* if we're using VTIME as an inter-char timeout, and it is set to
	     * be longer than the minimum calculated above, go for it
	     */
	    if (t->c_cc[VMIN] && t->c_cc[VTIME] && t->c_cc[VTIME]*10 > itimeout)
		itimeout = t->c_cc[VTIME]*10;

	    /* store it, taking care not to overflow the byte-sized register */
	    *(base + CD1400_RTPR) = (u_char)((itimeout <= 255) ? itimeout : 255);
	}


	/*
	 * channel control
	 *	receiver enable
	 *	transmitter enable (always set)
	 */
	opt = (1 << 4) | (1 << 3) | ((cflag & CREAD) ? (1 << 1) : 1);
	if (opt != infop->channel_control) {
	    infop->channel_control = opt;
	    cd1400_channel_cmd(base, opt);
	}

#ifdef Smarts
	/* set special chars */
	if (t->c_cc[VSTOP] != _POSIX_VDISABLE &&
		(t->c_cc[VSTOP] != infop->spec_char[0])) {
	    *(base + CD1400_SCHR1) = infop->spec_char[0] = t->c_cc[VSTOP];
	}
	if (t->c_cc[VSTART] != _POSIX_VDISABLE &&
		(t->c_cc[VSTART] != infop->spec_char[1])) {
	    *(base + CD1400_SCHR2) = infop->spec_char[0] = t->c_cc[VSTART];
	}
	if (t->c_cc[VINTR] != _POSIX_VDISABLE &&
		(t->c_cc[VINTR] != infop->spec_char[2])) {
	    *(base + CD1400_SCHR3) = infop->spec_char[0] = t->c_cc[VINTR];
	}
	if (t->c_cc[VSUSP] != _POSIX_VDISABLE &&
		(t->c_cc[VSUSP] != infop->spec_char[3])) {
	    *(base + CD1400_SCHR4) = infop->spec_char[0] = t->c_cc[VSUSP];
	}
#endif

	/*
	 * set channel option register 1 -
	 *	parity mode
	 *	stop bits
	 *	char length
	 */
	opt = 0;
	/* parity */
	if (cflag & PARENB) {
	    if (cflag & PARODD)
		opt |= 1 << 7;
	    opt |= 2 << 5;		/* normal parity mode */
	}
	if (!(iflag & INPCK))
	    opt |= 1 << 4;		/* ignore parity */
	/* stop bits */
	if (cflag & CSTOPB)
	    opt |= 2 << 2;
	/* char length */
	opt |= (cflag & CSIZE) >> 8;	/* nasty, but fast */
	if (opt != infop->cor[0]) {
	    cor_change |= 1 << 1;
	    *(base + CD1400_COR1) = opt;
	}

	/*
	 * set channel option register 2 -
	 *	flow control
	 */
	opt = 0;
#ifdef Smarts
	if (iflag & IXANY)
	    opt |= 1 << 7;		/* auto output restart on any char after XOFF */
	if (iflag & IXOFF)
	    opt |= 1 << 6;		/* auto XOFF output flow-control */
#endif
#ifndef ALWAYS_RTS_CTS	
	if (cflag & CCTS_OFLOW)
#endif
	    opt |= 1 << 1;		/* auto CTS flow-control */
	
	if (opt != infop->cor[1]) {
	    cor_change |= 1 << 2;
	    *(base + CD1400_COR2) = opt;
	}

	/*
	 * set channel option register 3 -
	 *	receiver FIFO interrupt threshold
	 *	flow control
	 */
	opt = RxFifoThreshold;	/* rx fifo threshold */
#ifdef Smarts
	if (t->c_lflag & ICANON)
	    opt |= 1 << 6;		/* detect INTR & SUSP chars */
	if (iflag & IXOFF)
	    opt |= (1 << 5) | (1 << 4);	/* transparent in-band flow control */
#endif
	if (opt != infop->cor[2]) {
	    cor_change |= 1 << 3;
	    *(base + CD1400_COR3) = opt;
	}


	/* notify the CD1400 if COR1-3 have changed */
	if (cor_change) {
	    cor_change |= 1 << 6;	/* COR change flag */
	    cd1400_channel_cmd(base, cor_change);
	}

	/*
	 * set channel option register 4 -
	 *	CR/NL processing
	 *	break processing
	 *	received exception processing
	 */
	opt = 0;
	if (iflag & IGNCR)
	    opt |= 1 << 7;
#ifdef Smarts
	/*
	 * we need a new ttyinput() for this, as we don't want to
	 * have ICRNL && INLCR being done in both layers, or to have
	 * synchronisation problems
	 */
	if (iflag & ICRNL)
	    opt |= 1 << 6;
	if (iflag & INLCR)
	    opt |= 1 << 5;
#endif
	if (iflag & IGNBRK)
	    opt |= 1 << 4;
	if (!(iflag & BRKINT))
	    opt |= 1 << 3;
	if (iflag & IGNPAR)
#ifdef LogOverruns
	    opt |= 0;		/* broken chars cause receive exceptions */
#else
	    opt |= 2;		/* discard broken chars */
#endif
	else {
	    if (iflag & PARMRK)
		opt |= 4;	/* precede broken chars with 0xff 0x0 */
	    else
#ifdef LogOverruns
		opt |= 0;	/* broken chars cause receive exceptions */
#else
		opt |= 3;	/* convert framing/parity errs to nulls */
#endif
	}
	*(base + CD1400_COR4) = opt;

	/*
	 * set channel option register 5 -
	 */
	opt = 0;
	if (iflag & ISTRIP)
	    opt |= 1 << 7;
	if (t->c_iflag & IEXTEN) {
	    opt |= 1 << 6;	/* enable LNEXT (e.g. ctrl-v quoting) handling */
	}
#ifdef Smarts
	if (t->c_oflag & ONLCR)
	    opt |= 1 << 1;
	if (t->c_oflag & OCRNL)
	    opt |= 1;
#endif
	*(base + CD1400_COR5) = opt;

	/*
	 * set modem change option register 1
	 *	generate modem interrupts on which 1 -> 0 input transitions
	 *	also controls auto-DTR output flow-control, which we don't use
	 */
	opt = (cflag & CLOCAL) ? 0 : 1 << 4;	/* CD */
	*(base + CD1400_MCOR1) = opt;

	/*
	 * set modem change option register 2
	 *	generate modem interrupts on specific 0 -> 1 input transitions
	 */
	opt = (cflag & CLOCAL) ? 0 : 1 << 4;	/* CD */
	*(base + CD1400_MCOR2) = opt;

	splx(s);

	return 0;
} /* end of cyparam */


void
cystart(struct tty *tp)
{
	u_char		unit = UNIT(tp->t_dev);
	struct cy	*infop = info[unit];
	cy_addr		base = infop->base_addr;
	int		s;

#ifdef CyDebug
	infop->start_count++;
#endif

	/* check the flow-control situation */
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))
		return;

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}

#ifdef TxBuffer
	service_upper_tx(unit);		/* feed the monster */
#endif

	s = spltty();

	if (!(infop->intr_enable & (1 << 2))) {
	    /* select the channel */
	    *(base + CD1400_CAR) = unit & (u_char)3;

	    /* (re)enable interrupts to set things in motion */
	    infop->intr_enable |= (1 << 2);
	    *(base + CD1400_SRER) = infop->intr_enable;

	    infop->start_real++;
	}

	splx(s);
} /* end of cystart() */


int
cystop(struct tty *tp, int flag)
{
	u_char		unit = UNIT(tp->t_dev);
	struct cy	*ip = info[unit];
	cy_addr		base = ip->base_addr;
	int		s;

	s = spltty();

	/* select the channel */
	*(base + CD1400_CAR) = unit & 3;

	/* halt output by disabling transmit interrupts */
	ip->intr_enable &=~ (1 << 2);
	*(base + CD1400_SRER) = ip->intr_enable;

	splx(s);

	return 0;
}

struct tty *
cydevtotty(dev_t dev)
{
	u_char unit = UNIT(dev);

	if (unit >= /* NCY * ? */ PORTS_PER_CYCLOM)
		return NULL;

	return info[unit]->tty;
}

int
cyspeed(int speed, int *prescaler_io)
{
    int		actual;
    int		error;
    int		divider;
    int		prescaler;
    int		prescaler_unit;

    if (speed == 0)
	return 0;

    if (speed < 0 || speed > 150000)
	return -1;

    /* determine which prescaler to use */
    for (prescaler_unit = 4, prescaler = 2048; prescaler_unit;
	    prescaler_unit--, prescaler >>= 2) {
	if (CYCLOM_CLOCK/prescaler/speed > 63)
	    break;
    }

    divider = (CYCLOM_CLOCK/prescaler*2/speed + 1)/2;	/* round off */
    if (divider > 255)
	divider = 255;
    actual = CYCLOM_CLOCK/prescaler/divider;
    error = ((actual-speed)*2000/speed +1)/2;	/* percentage */

    /* 3.0% max error tolerance */
    if (error < -30 || error > 30)
	return -1;

#if 0
    printf("prescaler = %d (%d)\n", prescaler, prescaler_unit);
    printf("divider = %d (%x)\n", divider, divider);
    printf("actual = %d\n", actual);
    printf("error = %d\n", error);
#endif

    *prescaler_io = prescaler_unit;
    return divider;
} /* end of cyspeed() */


static void
cd1400_channel_cmd(cy_addr base, u_char cmd)
{
 	/* XXX hsu@clinet.fi: This is always more dependent on ISA bus speed, 
	   as the card is probed every round?  Replaced delaycount with 8k.
	   Either delaycount has to be implemented in FreeBSD or more sensible
	   way of doing these should be implemented.  DELAY isn't enough here.
	   */
	unsigned maxwait = 5 * 8 * 1024;	/* approx. 5 ms */

	/* wait for processing of previous command to complete */
	while (*(base + CD1400_CCR) && maxwait--)
	  	;
	
	if (!maxwait)
		log(LOG_ERR, "cy: channel command timeout (%d loops) - arrgh\n",
		    5 * 8 * 1024);

	*(base + CD1400_CCR) = cmd;
} /* end of cd1400_channel_cmd() */


#ifdef CyDebug
/* useful in ddb */
void
cyclear(void)
{
    /* clear the timeout request */
    disable_intr();
    timeout_scheduled = 0;
    enable_intr();
}

void
cyclearintr(void)
{
    /* clear interrupts */
    *(cyclom_base + CYCLOM_CLEAR_INTR) = (u_char)0;
}

int
cyparam_dummy(struct tty *tp, struct termios *t)
{
    return 0;
}

void
cyset(int unit, int active)
{
    if (unit < 0 || unit >= /* NCY *? */ PORTS_PER_CYCLOM) {
	printf("bad unit number %d\n", unit);
	return;
    }
#ifdef __FreeBSD__
    cy_tty[unit].t_param = active ? cyparam : cyparam_dummy;
#else
    cy_tty[unit]->t_param = active ? cyparam : cyparam_dummy;
#endif
}


/* useful in ddb */
void
cystatus(int unit)
{
	struct cy	*infop = info[unit];
	struct tty	*tp = infop->tty;
	cy_addr		base = infop->base_addr;

	printf("info for channel %d\n", unit);
	printf("------------------\n");

	printf("cd1400 base address:\t0x%x\n", (int)infop->base_addr);

	/* select the port */
	*(base + CD1400_CAR) = (u_char)unit;

	printf("saved channel_control:\t%02x\n", infop->channel_control);
	printf("saved cor1:\t\t%02x\n", infop->cor[0]);
	printf("service request enable reg:\t%02x (%02x cached)\n",
		(u_char)*(base + CD1400_SRER), infop->intr_enable);
	printf("service request register:\t%02x\n",
		(u_char)*(base + CD1400_SVRR));
	printf("\n");
	printf("modem status:\t\t\t%02x (%02x cached)\n",
		(u_char)*(base + CD1400_MSVR), infop->modem_sig);
	printf("rx/tx/mdm interrupt registers:\t%02x %02x %02x\n",
		(u_char)*(base + CD1400_RIR), (u_char)*(base + CD1400_TIR),
		(u_char)*(base + CD1400_MIR));
	printf("\n");
	if (tp) {
		printf("tty state:\t\t\t%04x\n", tp->t_state);
		printf("upper layer queue lengths:\t%d raw, %d canon, %d output\n",
		    tp->t_rawq.c_cc, tp->t_canq.c_cc, tp->t_outq.c_cc);
	}
	else
		printf("tty state:\t\t\tclosed\n");
	printf("\n");

	printf("calls to cystart():\t\t%d (%d useful)\n",
		infop->start_count, infop->start_real);
	printf("\n");
	printf("total cyclom service probes:\t%d\n", cy_svrr_probes);
	printf("calls to upper layer:\t\t%d\n", cy_timeouts);
	printf("rx buffer chars free:\t\t%d\n", infop->rx_buf->free);
#ifdef TxBuffer
	printf("tx buffer chars used:\t\t%d\n", infop->tx_buf.used);
#endif
	printf("received chars:\t\t\t%d good, %d exception\n",
		infop->recv_normal, infop->recv_exception);
	printf("transmitted chars:\t\t%d\n", infop->xmit);
	printf("modem signal deltas:\t\t%d\n", infop->mdm);
	printf("\n");
} /* end of cystatus() */
#endif
#endif /* NCY > 0 */
