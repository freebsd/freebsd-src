/*
 * Cronyx-Sigma adapter driver for FreeBSD.
 * Supports PPP/HDLC protocol in synchronous mode,
 * and asyncronous channels with full modem control.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 *
 * $FreeBSD: src/sys/i386/isa/cx.c,v 1.45 2000/01/29 16:17:31 peter Exp $
 *
 */
#undef DEBUG

#include "cx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/socket.h>
#include <net/if.h>

#ifdef __FreeBSD__
#   if __FreeBSD__ < 2
#      include <machine/pio.h>
#      define RB_GETC(q) getc(q)
#   endif
#endif
#ifdef __bsdi__
#   include <sys/ttystats.h>
#   include <machine/inline.h>
#   define tsleep(tp,pri,msg,x) ((tp)->t_state |= TS_WOPEN,\
		ttysleep (tp, (caddr_t)&tp->t_rawq, pri, msg, x))
#endif
#if !defined (__FreeBSD__) || __FreeBSD__ >= 2
#      define t_out t_outq
#      define RB_LEN(q) ((q).c_cc)
#      define RB_GETC(q) getc(&q)
#ifndef TSA_CARR_ON /* FreeBSD 2.x before not long after 2.0.5 */
#      define TSA_CARR_ON(tp) tp
#      define TSA_OLOWAT(q) ((caddr_t)&(q)->t_out)
#endif
#endif

#include <machine/cronyx.h>
#include <i386/isa/cxreg.h>

/* XXX imported from if_cx.c. */
void cxswitch (cx_chan_t *c, cx_soft_opt_t new);

/* XXX exported. */
void cxmint (cx_chan_t *c);
int cxrinta (cx_chan_t *c);
void cxtinta (cx_chan_t *c);
timeout_t cxtimeout;

#ifdef DEBUG
#   define print(s)     printf s
#else
#   define print(s)     {/*void*/}
#endif

#define DMABUFSZ        (6*256)         /* buffer size */
#define BYTE            *(unsigned char*)&
#define UNIT(u)         (minor(u) & 077)
#define UNIT_CTL        077

extern cx_board_t cxboard [NCX];        /* adapter state structures */
extern cx_chan_t *cxchan [NCX*NCHAN];   /* unit to channel struct pointer */
#if __FreeBSD__ >= 2
static struct tty cx_tty [NCX*NCHAN];          /* tty data */

static	d_open_t	cxopen;
static	d_close_t	cxclose;
static	d_ioctl_t	cxioctl;

#define	CDEV_MAJOR	42
/* Don't make this static, since if_cx.c uses it. */
struct cdevsw cx_cdevsw = {
	/* open */	cxopen,
	/* close */	cxclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	cxioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"cx",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
	/* bmaj */	-1
};
#else
struct tty *cx_tty [NCX*NCHAN];         /* tty data */
#endif

static void cxoproc (struct tty *tp);
static void cxstop (struct tty *tp, int flag);
static int cxparam (struct tty *tp, struct termios *t);

int cxopen (dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = UNIT (dev);
	cx_chan_t *c = cxchan[unit];
	unsigned short port;
	struct tty *tp;
	int error = 0;

	if (unit == UNIT_CTL) {
		print (("cx: cxopen /dev/cronyx\n"));
		return (0);
	}
	if (unit >= NCX*NCHAN || !c || c->type==T_NONE)
		return (ENXIO);
	port = c->chip->port;
	print (("cx%d.%d: cxopen unit=%d\n", c->board->num, c->num, unit));
	if (c->mode != M_ASYNC)
		return (EBUSY);
	if (! c->ttyp) {
#ifdef __FreeBSD__
#if __FreeBSD__ >= 2
		c->ttyp = &cx_tty[unit];
#else
		c->ttyp = cx_tty[unit] = ttymalloc (cx_tty[unit]);
#endif
#else
		MALLOC (cx_tty[unit], struct tty*, sizeof (struct tty), M_DEVBUF, M_WAITOK);
		bzero (cx_tty[unit], sizeof (*cx_tty[unit]));
		c->ttyp = cx_tty[unit];
#endif
		c->ttyp->t_oproc = cxoproc;
		c->ttyp->t_stop = cxstop;
		c->ttyp->t_param = cxparam;
	}
	dev->si_tty = c->ttyp;
#ifdef __bsdi__
	if (! c->ttydev) {
		MALLOC (c->ttydev, struct ttydevice_tmp*,
			sizeof (struct ttydevice_tmp), M_DEVBUF, M_WAITOK);
		bzero (c->ttydev, sizeof (*c->ttydev));
		strcpy (c->ttydev->tty_name, "cx");
		c->ttydev->tty_unit = unit;
		c->ttydev->tty_base = unit;
		c->ttydev->tty_count = 1;
		c->ttydev->tty_ttys = c->ttyp;
		tty_attach (c->ttydev);
	}
#endif
	tp = c->ttyp;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) && (tp->t_state & TS_XCLUDE) &&
	    suser(p))
		return (EBUSY);
	if (! (tp->t_state & TS_ISOPEN)) {
		ttychars (tp);
		if (tp->t_ispeed == 0) {
#ifdef __bsdi__
			tp->t_termios = deftermios;
#else
			tp->t_iflag = 0;
			tp->t_oflag = 0;
			tp->t_lflag = 0;
			tp->t_cflag = CREAD | CS8 | HUPCL;
			tp->t_ispeed = c->rxbaud;
			tp->t_ospeed = c->txbaud;
#endif
		}
		cxparam (tp, &tp->t_termios);
		ttsetwater (tp);
	}

	spltty ();
	if (! (tp->t_state & TS_ISOPEN)) {
		/*
		 * Compute optimal receiver buffer length.
		 * The best choice is rxbaud/400.
		 * Make it even, to avoid byte-wide DMA transfers.
		 * --------------------------
		 * Baud rate    Buffer length
		 * --------------------------
		 *      300     4
		 *     1200     4
		 *     9600     24
		 *    19200     48
		 *    38400     96
		 *    57600     192
		 *   115200     288
		 * --------------------------
		 */
		int rbsz = (c->rxbaud + 800 - 1) / 800 * 2;
		if (rbsz < 4)
			rbsz = 4;
		else if (rbsz > DMABUFSZ)
			rbsz = DMABUFSZ;

		/* Initialize channel, enable receiver. */
		cx_cmd (port, CCR_INITCH | CCR_ENRX);
		cx_cmd (port, CCR_INITCH | CCR_ENRX);

		/* Start receiver. */
		outw (ARBCNT(port), rbsz);
		outw (BRBCNT(port), rbsz);
		outw (ARBSTS(port), BSTS_OWN24);
		outw (BRBSTS(port), BSTS_OWN24);

		/* Enable interrupts. */
		outb (IER(port), IER_RXD | IER_RET | IER_TXD | IER_MDM);

		cx_chan_dtr (c, 1);
		cx_chan_rts (c, 1);
	}
	if (cx_chan_cd (c))
		(*linesw[tp->t_line].l_modem)(tp, 1);
	if (! (flag & O_NONBLOCK)) {
		/* Lock the channel against cxconfig while we are
		 * waiting for carrier. */
		c->sopt.lock = 1;
		while (!(tp->t_cflag & CLOCAL) && !(tp->t_state & TS_CARR_ON))
			if ((error = tsleep (TSA_CARR_ON(tp), TTIPRI | PCATCH,
			    "cxdcd", 0)))
				break;
		c->sopt.lock = 0;       /* Unlock the channel. */
	}
	print (("cx%d.%d: cxopen done csr=%b\n", c->board->num, c->num,
		inb(CSR(c->chip->port)), CSRA_BITS));
	spl0 ();
	if (error)
		return (error);
#if __FreeBSD__ >= 2
	error = (*linesw[tp->t_line].l_open) (dev, tp);
#else
	error = (*linesw[tp->t_line].l_open) (dev, tp, 0);
#endif
	return (error);
}

int cxclose (dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = UNIT (dev);
	cx_chan_t *c = cxchan[unit];
	struct tty *tp;
	int s;

	if (unit == UNIT_CTL)
		return (0);
	tp = c->ttyp;
	(*linesw[tp->t_line].l_close) (tp, flag);

	/* Disable receiver.
	 * Transmitter continues sending the queued data. */
	s = spltty ();
	outb (CAR(c->chip->port), c->num & 3);
	outb (IER(c->chip->port), IER_TXD | IER_MDM);
	cx_cmd (c->chip->port, CCR_DISRX);

	/* Clear DTR and RTS. */
	if ((tp->t_cflag & HUPCL) || ! (tp->t_state & TS_ISOPEN)) {
		cx_chan_dtr (c, 0);
		cx_chan_rts (c, 0);
	}

	/* Stop sending break. */
	if (c->brk == BRK_SEND) {
		c->brk = BRK_STOP;
		if (! (tp->t_state & TS_BUSY))
			cxoproc (tp);
	}
	splx (s);
	ttyclose (tp);
	return (0);
}

int cxioctl (dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = UNIT (dev);
	cx_chan_t *c, *m;
	cx_stat_t *st;
	struct tty *tp;
	int error, s;
	unsigned char msv;
	struct ifnet *master;

	if (unit == UNIT_CTL) {
		/* Process an ioctl request on /dev/cronyx */
		cx_options_t *o = (cx_options_t*) data;

		if (o->board >= NCX || o->channel >= NCHAN)
			return (EINVAL);
		c = &cxboard[o->board].chan[o->channel];
		if (c->type == T_NONE)
			return (ENXIO);
		switch (cmd) {
		default:
			return (EINVAL);

		case CXIOCSETMODE:
			print (("cx%d.%d: CXIOCSETMODE\n", o->board, o->channel));
			if (c->type == T_NONE)
				return (EINVAL);
			if (c->type == T_ASYNC && o->mode != M_ASYNC)
				return (EINVAL);
			if (o->mode == M_ASYNC)
				switch (c->type) {
				case T_SYNC_RS232:
				case T_SYNC_V35:
				case T_SYNC_RS449:
					return (EINVAL);
				}
			/* Somebody is waiting for carrier? */
			if (c->sopt.lock)
				return (EBUSY);
			/* /dev/ttyXX is already opened by someone? */
			if (c->mode == M_ASYNC && c->ttyp &&
			    (c->ttyp->t_state & TS_ISOPEN))
				return (EBUSY);
			/* Network interface is up? */
			if (c->mode != M_ASYNC && (c->ifp->if_flags & IFF_UP))
				return (EBUSY);

			/* Find the master interface. */
			master = *o->master ? ifunit (o->master) : c->ifp;
			if (! master)
				return (EINVAL);
			m = cxchan[master->if_unit];

			/* Leave the previous master queue. */
			if (c->master != c->ifp) {
				cx_chan_t *p = cxchan[c->master->if_unit];

				for (; p; p=p->slaveq)
					if (p->slaveq == c)
						p->slaveq = c->slaveq;
			}

			/* Set up new master. */
			c->master = master;
			c->slaveq = 0;

			/* Join the new master queue. */
			if (c->master != c->ifp) {
				c->slaveq = m->slaveq;
				m->slaveq = c;
			}

			c->mode   = o->mode;
			c->rxbaud = o->rxbaud;
			c->txbaud = o->txbaud;
			c->opt    = o->opt;
			c->aopt   = o->aopt;
			c->hopt   = o->hopt;
			c->bopt   = o->bopt;
			c->xopt   = o->xopt;
			switch (c->num) {
			case 0: c->board->if0type = o->iftype; break;
			case 8: c->board->if8type = o->iftype; break;
			}
			s = spltty ();
			cxswitch (c, o->sopt);
			cx_setup_chan (c);
			outb (IER(c->chip->port), 0);
			splx (s);
			break;

		case CXIOCGETSTAT:
			st = (cx_stat_t*) data;
			st->rintr  = c->stat->rintr;
			st->tintr  = c->stat->tintr;
			st->mintr  = c->stat->mintr;
			st->ibytes = c->stat->ibytes;
			st->ipkts  = c->stat->ipkts;
			st->ierrs  = c->stat->ierrs;
			st->obytes = c->stat->obytes;
			st->opkts  = c->stat->opkts;
			st->oerrs  = c->stat->oerrs;
			break;

		case CXIOCGETMODE:
			print (("cx%d.%d: CXIOCGETMODE\n", o->board, o->channel));
			o->type   = c->type;
			o->mode   = c->mode;
			o->rxbaud = c->rxbaud;
			o->txbaud = c->txbaud;
			o->opt    = c->opt;
			o->aopt   = c->aopt;
			o->hopt   = c->hopt;
			o->bopt   = c->bopt;
			o->xopt   = c->xopt;
			o->sopt   = c->sopt;
			switch (c->num) {
			case 0: o->iftype = c->board->if0type; break;
			case 8: o->iftype = c->board->if8type; break;
			}
			if (c->master != c->ifp)
				snprintf (o->master, sizeof(o->master),
				    "%s%d", c->master->if_name,
					c->master->if_unit);
			else
				*o->master = 0;
			break;
		}
		return (0);
	}

	c = cxchan[unit];
	tp = c->ttyp;
	if (! tp)
		return (EINVAL);
#if __FreeBSD__ >= 2
	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, data, flag, p);
#else
	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, data, flag);
#endif
	if (error != ENOIOCTL)
		return (error);
	error = ttioctl (tp, cmd, data, flag);
	if (error != ENOIOCTL)
		return (error);

	s = spltty ();
	switch (cmd) {
	default:
		splx (s);
		return (ENOTTY);
	case TIOCSBRK:          /* Start sending line break */
		c->brk = BRK_SEND;
		if (! (tp->t_state & TS_BUSY))
			cxoproc (tp);
		break;
	case TIOCCBRK:          /* Stop sending line break */
		c->brk = BRK_STOP;
		if (! (tp->t_state & TS_BUSY))
			cxoproc (tp);
		break;
	case TIOCSDTR:          /* Set DTR */
		cx_chan_dtr (c, 1);
		break;
	case TIOCCDTR:          /* Clear DTR */
		cx_chan_dtr (c, 0);
		break;
	case TIOCMSET:          /* Set DTR/RTS */
		cx_chan_dtr (c, (*(int*)data & TIOCM_DTR) ? 1 : 0);
		cx_chan_rts (c, (*(int*)data & TIOCM_RTS) ? 1 : 0);
		break;
	case TIOCMBIS:          /* Add DTR/RTS */
		if (*(int*)data & TIOCM_DTR) cx_chan_dtr (c, 1);
		if (*(int*)data & TIOCM_RTS) cx_chan_rts (c, 1);
		break;
	case TIOCMBIC:          /* Clear DTR/RTS */
		if (*(int*)data & TIOCM_DTR) cx_chan_dtr (c, 0);
		if (*(int*)data & TIOCM_RTS) cx_chan_rts (c, 0);
		break;
	case TIOCMGET:          /* Get modem status */
		msv = inb (MSVR(c->chip->port));
		*(int*)data = TIOCM_LE; /* always enabled while open */
		if (msv & MSV_DSR) *(int*)data |= TIOCM_DSR;
		if (msv & MSV_CTS) *(int*)data |= TIOCM_CTS;
		if (msv & MSV_CD)  *(int*)data |= TIOCM_CD;
		if (c->dtr)        *(int*)data |= TIOCM_DTR;
		if (c->rts)        *(int*)data |= TIOCM_RTS;
		break;
	}
	splx (s);
	return (0);
}

/*
 * Fill transmitter buffer with data.
 */
static void
cxout (cx_chan_t *c, char b)
{
	unsigned char *buf, *p, sym;
	unsigned short port = c->chip->port, len = 0, cnt_port, sts_port;
	struct tty *tp = c->ttyp;

	if (! tp)
		return;

	/* Choose the buffer. */
	if (b == 'A') {
		buf      = c->atbuf;
		cnt_port = ATBCNT(port);
		sts_port = ATBSTS(port);
	} else {
		buf      = c->btbuf;
		cnt_port = BTBCNT(port);
		sts_port = BTBSTS(port);
	}

	/* Is it busy? */
	if (inb (sts_port) & BSTS_OWN24) {
		tp->t_state |= TS_BUSY;
		return;
	}

	switch (c->brk) {
	case BRK_SEND:
		*buf++ = 0;     /* extended transmit command */
		*buf++ = 0x81;  /* send break */
		*buf++ = 0;     /* extended transmit command */
		*buf++ = 0x82;  /* insert delay */
		*buf++ = 250;   /* 1/4 of second */
		*buf++ = 0;     /* extended transmit command */
		*buf++ = 0x82;  /* insert delay */
		*buf++ = 250;   /* + 1/4 of second */
		len = 8;
		c->brk = BRK_IDLE;
		break;
	case BRK_STOP:
		*buf++ = 0;     /* extended transmit command */
		*buf++ = 0x83;  /* stop break */
		len = 2;
		c->brk = BRK_IDLE;
		break;
	case BRK_IDLE:
		p = buf;
		if (tp->t_iflag & IXOFF)
			while (RB_LEN (tp->t_out) && p<buf+DMABUFSZ-1) {
				sym = RB_GETC (tp->t_out);
				/* Send XON/XOFF out of band. */
				if (sym == tp->t_cc[VSTOP]) {
					outb (STCR(port), STC_SNDSPC|STC_SSPC_2);
					continue;
				}
				if (sym == tp->t_cc[VSTART]) {
					outb (STCR(port), STC_SNDSPC|STC_SSPC_1);
					continue;
				}
				/* Duplicate NULLs in ETC mode. */
				if (! sym)
					*p++ = 0;
				*p++ = sym;
			}
		else
			while (RB_LEN (tp->t_out) && p<buf+DMABUFSZ-1) {
				sym = RB_GETC (tp->t_out);
				/* Duplicate NULLs in ETC mode. */
				if (! sym)
					*p++ = 0;
				*p++ = sym;
			}
		len = p - buf;
		break;
	}

	/* Start transmitter. */
	if (len) {
		outw (cnt_port, len);
		outb (sts_port, BSTS_INTR | BSTS_OWN24);
		c->stat->obytes += len;
		tp->t_state |= TS_BUSY;
		print (("cx%d.%d: out %d bytes to %c\n",
			c->board->num, c->num, len, b));
	}
}

void cxoproc (struct tty *tp)
{
	int unit = UNIT (tp->t_dev);
	cx_chan_t *c = cxchan[unit];
	unsigned short port = c->chip->port;
	int s = spltty ();

	/* Set current channel number */
	outb (CAR(port), c->num & 3);

	if (! (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))) {
		/* Start transmitter. */
		if (! (inb (CSR(port)) & CSRA_TXEN))
			cx_cmd (port, CCR_ENTX);

		/* Determine the buffer order. */
		if (inb (DMABSTS(port)) & DMABSTS_NTBUF) {
			cxout (c, 'B');
			cxout (c, 'A');
		} else {
			cxout (c, 'A');
			cxout (c, 'B');
		}
	}
#ifndef TS_ASLEEP /* FreeBSD some time after 2.0.5 */
	ttwwakeup(tp);
#else
	if (RB_LEN (tp->t_out) <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(TSA_OLOWAT(tp));
		}
		selwakeup(&tp->t_wsel);
	}
#endif
	splx (s);
}

static int
cxparam (struct tty *tp, struct termios *t)
{
	int unit = UNIT (tp->t_dev);
	cx_chan_t *c = cxchan[unit];
	unsigned short port = c->chip->port;
	int clock, period, s;
	cx_cor1_async_t cor1;

	if (t->c_ospeed == 0) {
		/* Clear DTR and RTS. */
		s = spltty ();
		cx_chan_dtr (c, 0);
		cx_chan_rts (c, 0);
		splx (s);
		print (("cx%d.%d: cxparam (hangup)\n", c->board->num, c->num));
		return (0);
	}
	print (("cx%d.%d: cxparam\n", c->board->num, c->num));

	/* Check requested parameters. */
	if (t->c_ospeed < 300 || t->c_ospeed > 256*1024)
                return(EINVAL);
	if (t->c_ispeed && (t->c_ispeed < 300 || t->c_ispeed > 256*1024))
                return(EINVAL);

#ifdef __bsdi__
	/* CLOCAL flag set -- wakeup everybody who waits for CD. */
	/* FreeBSD does this themselves. */
	if (! (tp->t_cflag & CLOCAL) && (t->c_cflag & CLOCAL))
		wakeup ((caddr_t) &tp->t_rawq);
#endif
	/* And copy them to tty and channel structures. */
	c->rxbaud = tp->t_ispeed = t->c_ispeed;
	c->txbaud = tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	/* Set character length and parity mode. */
	BYTE cor1 = 0;
	switch (t->c_cflag & CSIZE) {
	default:
	case CS8: cor1.charlen = 7; break;
	case CS7: cor1.charlen = 6; break;
	case CS6: cor1.charlen = 5; break;
	case CS5: cor1.charlen = 4; break;
	}
	if (t->c_cflag & PARENB) {
		cor1.parmode = PARM_NORMAL;
		cor1.ignpar = 0;
		cor1.parity = (t->c_cflag & PARODD) ? PAR_ODD : PAR_EVEN;
	} else {
		cor1.parmode = PARM_NOPAR;
		cor1.ignpar = 1;
	}

	/* Enable/disable hardware CTS. */
	c->aopt.cor2.ctsae = (t->c_cflag & CRTSCTS) ? 1 : 0;
	/* Handle DSR as CTS. */
	c->aopt.cor2.dsrae = (t->c_cflag & CRTSCTS) ? 1 : 0;
	/* Enable extended transmit command mode.
	 * Unfortunately, there is no other method for sending break. */
	c->aopt.cor2.etc = 1;
	/* Enable/disable hardware XON/XOFF. */
	c->aopt.cor2.ixon = (t->c_iflag & IXON) ? 1 : 0;
	c->aopt.cor2.ixany = (t->c_iflag & IXANY) ? 1 : 0;

	/* Set the number of stop bits. */
	if (t->c_cflag & CSTOPB)
		c->aopt.cor3.stopb = STOPB_2;
	else
		c->aopt.cor3.stopb = STOPB_1;
	/* Disable/enable passing XON/XOFF chars to the host. */
	c->aopt.cor3.scde = (t->c_iflag & IXON) ? 1 : 0;
	c->aopt.cor3.flowct = (t->c_iflag & IXON) ? FLOWCC_NOTPASS : FLOWCC_PASS;

	c->aopt.schr1 = t->c_cc[VSTART];        /* XON */
	c->aopt.schr2 = t->c_cc[VSTOP];         /* XOFF */

	/* Set current channel number. */
	s = spltty ();
	outb (CAR(port), c->num & 3);

	/* Set up receiver clock values. */
	cx_clock (c->chip->oscfreq, c->rxbaud, &clock, &period);
	c->opt.rcor.clk = clock;
	outb (RCOR(port), BYTE c->opt.rcor);
	outb (RBPR(port), period);

	/* Set up transmitter clock values. */
	cx_clock (c->chip->oscfreq, c->txbaud, &clock, &period);
	c->opt.tcor.clk = clock;
	c->opt.tcor.ext1x = 0;
	outb (TCOR(port), BYTE c->opt.tcor);
	outb (TBPR(port), period);

	outb (COR2(port), BYTE c->aopt.cor2);
	outb (COR3(port), BYTE c->aopt.cor3);
	outb (SCHR1(port), c->aopt.schr1);
	outb (SCHR2(port), c->aopt.schr2);

	if (BYTE c->aopt.cor1 != BYTE cor1) {
		BYTE c->aopt.cor1 = BYTE cor1;
		outb (COR1(port), BYTE c->aopt.cor1);
		/* Any change to COR1 require reinitialization. */
		/* Unfortunately, it may cause transmitter glitches... */
		cx_cmd (port, CCR_INITCH);
	}
	splx (s);
	return (0);
}

/*
 * Stop output on a line
 */
void cxstop (struct tty *tp, int flag)
{
	cx_chan_t *c = cxchan[UNIT(tp->t_dev)];
	unsigned short port = c->chip->port;
	int s = spltty ();

	if (tp->t_state & TS_BUSY) {
		print (("cx%d.%d: cxstop\n", c->board->num, c->num));

		/* Set current channel number */
		outb (CAR(port), c->num & 3);

		/* Stop transmitter */
		cx_cmd (port, CCR_DISTX);
	}
	splx (s);
}

/*
 * Handle receive interrupts, including receive errors and
 * receive timeout interrupt.
 */
int cxrinta (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned short len = 0, risr = inw (RISR(port)), reoir = 0;
	struct tty *tp = c->ttyp;

	/* Compute optimal receiver buffer length. */
	int rbsz = (c->rxbaud + 800 - 1) / 800 * 2;
	if (rbsz < 4)
		rbsz = 4;
	else if (rbsz > DMABUFSZ)
		rbsz = DMABUFSZ;

	if (risr & RISA_TIMEOUT) {
		unsigned long rcbadr = (unsigned short) inw (RCBADRL(port)) |
			(long) inw (RCBADRU(port)) << 16;
		unsigned char *buf = 0;
		unsigned short cnt_port = 0, sts_port = 0;
		if (rcbadr >= c->brphys && rcbadr < c->brphys+DMABUFSZ) {
			buf = c->brbuf;
			len = rcbadr - c->brphys;
			cnt_port = BRBCNT(port);
			sts_port = BRBSTS(port);
		} else if (rcbadr >= c->arphys && rcbadr < c->arphys+DMABUFSZ) {
			buf = c->arbuf;
			len = rcbadr - c->arphys;
			cnt_port = ARBCNT(port);
			sts_port = ARBSTS(port);
		} else
			printf ("cx%d.%d: timeout: invalid buffer address\n",
				c->board->num, c->num);

		if (len) {
			print (("cx%d.%d: async receive timeout (%d bytes), risr=%b, arbsts=%b, brbsts=%b\n",
				c->board->num, c->num, len, risr, RISA_BITS,
				inb (ARBSTS(port)), BSTS_BITS, inb (BRBSTS(port)), BSTS_BITS));
			c->stat->ibytes += len;
			if (tp && (tp->t_state & TS_ISOPEN)) {
				int i;
				int (*rint)(int, struct tty *) =
					linesw[tp->t_line].l_rint;

				for (i=0; i<len; ++i)
					(*rint) (buf[i], tp);
			}

			/* Restart receiver. */
			outw (cnt_port, rbsz);
			outb (sts_port, BSTS_OWN24);
		}
		return (REOI_TERMBUFF);
	}

	print (("cx%d.%d: async receive interrupt, risr=%b, arbsts=%b, brbsts=%b\n",
		c->board->num, c->num, risr, RISA_BITS,
		inb (ARBSTS(port)), BSTS_BITS, inb (BRBSTS(port)), BSTS_BITS));

	if (risr & RIS_BUSERR) {
		printf ("cx%d.%d: receive bus error\n", c->board->num, c->num);
		++c->stat->ierrs;
	}
	if (risr & (RIS_OVERRUN | RISA_PARERR | RISA_FRERR | RISA_BREAK)) {
		int err = 0;

		if (risr & RISA_PARERR)
			err |= TTY_PE;
		if (risr & RISA_FRERR)
			err |= TTY_FE;
#ifdef TTY_OE
		if (risr & RIS_OVERRUN)
			err |= TTY_OE;
#endif
#ifdef TTY_BI
		if (risr & RISA_BREAK)
			err |= TTY_BI;
#endif
		print (("cx%d.%d: receive error %x\n", c->board->num, c->num, err));
		if (tp && (tp->t_state & TS_ISOPEN))
			(*linesw[tp->t_line].l_rint) (err, tp);
		++c->stat->ierrs;
	}

	/* Discard exception characters. */
	if ((risr & RISA_SCMASK) && tp && (tp->t_iflag & IXON))
		reoir |= REOI_DISCEXC;

	/* Handle received data. */
	if ((risr & RIS_EOBUF) && tp && (tp->t_state & TS_ISOPEN)) {
		int (*rint)(int, struct tty *) = linesw[tp->t_line].l_rint;
		unsigned char *buf;
		int i;

		len = (risr & RIS_BB) ? inw(BRBCNT(port)) : inw(ARBCNT(port));

		print (("cx%d.%d: async: %d bytes received\n",
			c->board->num, c->num, len));
		c->stat->ibytes += len;

		buf = (risr & RIS_BB) ? c->brbuf : c->arbuf;
		for (i=0; i<len; ++i)
			(*rint) (buf[i], tp);
	}

	/* Restart receiver. */
	if (! (inb (ARBSTS(port)) & BSTS_OWN24)) {
		outw (ARBCNT(port), rbsz);
		outb (ARBSTS(port), BSTS_OWN24);
	}
	if (! (inb (BRBSTS(port)) & BSTS_OWN24)) {
		outw (BRBCNT(port), rbsz);
		outb (BRBSTS(port), BSTS_OWN24);
	}
	return (reoir);
}

/*
 * Handle transmit interrupt.
 */
void cxtinta (cx_chan_t *c)
{
	struct tty *tp = c->ttyp;
	unsigned short port = c->chip->port;
	unsigned char tisr = inb (TISR(port));

	print (("cx%d.%d: async transmit interrupt, tisr=%b, atbsts=%b, btbsts=%b\n",
		c->board->num, c->num, tisr, TIS_BITS,
		inb (ATBSTS(port)), BSTS_BITS, inb (BTBSTS(port)), BSTS_BITS));

	if (tisr & TIS_BUSERR) {
		printf ("cx%d.%d: transmit bus error\n",
			c->board->num, c->num);
		++c->stat->oerrs;
	} else if (tisr & TIS_UNDERRUN) {
		printf ("cx%d.%d: transmit underrun error\n",
			c->board->num, c->num);
		++c->stat->oerrs;
	}
	if (tp) {
		tp->t_state &= ~(TS_BUSY | TS_FLUSH);
		if (tp->t_line)
			(*linesw[tp->t_line].l_start) (tp);
		else
			cxoproc (tp);
	}
}

/*
 * Handle modem interrupt.
 */
void cxmint (cx_chan_t *c)
{
	unsigned short port = c->chip->port;
	unsigned char misr = inb (MISR(port));
	unsigned char msvr = inb (MSVR(port));
	struct tty *tp = c->ttyp;

	if (c->mode != M_ASYNC) {
		printf ("cx%d.%d: unexpected modem interrupt, misr=%b, msvr=%b\n",
			c->board->num, c->num, misr, MIS_BITS, msvr, MSV_BITS);
		return;
	}
	print (("cx%d.%d: modem interrupt, misr=%b, msvr=%b\n",
		c->board->num, c->num, misr, MIS_BITS, msvr, MSV_BITS));

	/* Ignore DSR events. */
	/* Ignore RTC/CTS events, handled by hardware. */
	/* Handle carrier detect/loss. */
	if (tp && (misr & MIS_CCD))
		(*linesw[tp->t_line].l_modem) (tp, (msvr & MSV_CD) != 0);
}

/*
 * Recover after lost transmit interrupts.
 */
void cxtimeout (void *a)
{
	cx_board_t *b;
	cx_chan_t *c;
	struct tty *tp;
	int s;

	for (b=cxboard; b<cxboard+NCX; ++b)
		for (c=b->chan; c<b->chan+NCHAN; ++c) {
			tp = c->ttyp;
			if (c->type==T_NONE || c->mode!=M_ASYNC || !tp)
				continue;
			s = spltty ();
			if (tp->t_state & TS_BUSY) {
				tp->t_state &= ~TS_BUSY;
				if (tp->t_line)
					(*linesw[tp->t_line].l_start) (tp);
				else
					cxoproc (tp);
			}
			splx (s);
		}
	timeout (cxtimeout, 0, hz*5);
}


#if defined(__FreeBSD__) && (__FreeBSD__ > 1 )
static void 	cx_drvinit(void *unused)
{

	cdevsw_add(&cx_cdevsw);
}

SYSINIT(cxdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cx_drvinit,NULL)


#endif
