/*
 * Copyright (C) 1995 by Pavel Antonov, Moscow, Russia.
 * Copyright (C) 1995 by Andrey A. Chernov, Moscow, Russia.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND
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
 * $FreeBSD$
 *
 */

/*
 * SDL Communications Riscom/8 (based on Cirrus Logic CL-CD180) driver
 *
 */

#include "rc.h"

/*#define RCDEBUG*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ipl.h>

#include <machine/clock.h>

#include <i386/isa/isa_device.h>

#include <i386/isa/ic/cd180.h>
#include <i386/isa/rcreg.h>

/* Prototypes */
static int     rcprobe         __P((struct isa_device *));
static int     rcattach        __P((struct isa_device *));

#define rcin(port)      RC_IN  (nec, port)
#define rcout(port,v)   RC_OUT (nec, port, v)

#define WAITFORCCR(u,c) rc_wait0(nec, (u), (c), __LINE__)
#define CCRCMD(u,c,cmd) WAITFORCCR((u), (c)); rcout(CD180_CCR, (cmd))

#define RC_IBUFSIZE     256
#define RB_I_HIGH_WATER (TTYHOG - 2 * RC_IBUFSIZE)
#define RC_OBUFSIZE     512
#define RC_IHIGHWATER   (3 * RC_IBUFSIZE / 4)
#define INPUT_FLAGS_SHIFT (2 * RC_IBUFSIZE)
#define LOTS_OF_EVENTS  64

#define RC_FAKEID       0x10

#define RC_PROBED 1
#define RC_ATTACHED 2

#define GET_UNIT(dev)   (minor(dev) & 0x3F)
#define CALLOUT(dev)    (minor(dev) & 0x80)

/* For isa routines */
struct isa_driver rcdriver = {
	INTR_TYPE_TTY,
	rcprobe,
	rcattach,
	"rc"
};
COMPAT_ISA_DRIVER(rc, rcdriver);

static	d_open_t	rcopen;
static	d_close_t	rcclose;
static	d_ioctl_t	rcioctl;

#define	CDEV_MAJOR	63
static struct cdevsw rc_cdevsw = {
	/* open */	rcopen,
	/* close */	rcclose,
	/* read */	ttyread,
	/* write */	ttywrite,
	/* ioctl */	rcioctl,
	/* poll */	ttypoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"rc",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY,
	/* bmaj */	-1
};

/* Per-board structure */
static struct rc_softc {
	u_int           rcb_probed;     /* 1 - probed, 2 - attached */
	u_int           rcb_addr;       /* Base I/O addr        */
	u_int           rcb_unit;       /* unit #               */
	u_char          rcb_dtr;        /* DTR status           */
	struct rc_chans *rcb_baserc;    /* base rc ptr          */
} rc_softc[NRC];

/* Per-channel structure */
static struct rc_chans  {
	struct rc_softc *rc_rcb;                /* back ptr             */
	u_short          rc_flags;              /* Misc. flags          */
	int              rc_chan;               /* Channel #            */
	u_char           rc_ier;                /* intr. enable reg     */
	u_char           rc_msvr;               /* modem sig. status    */
	u_char           rc_cor2;               /* options reg          */
	u_char           rc_pendcmd;            /* special cmd pending  */
	u_int            rc_dtrwait;            /* dtr timeout          */
	u_int            rc_dcdwaits;           /* how many waits DCD in open */
	u_char		 rc_hotchar;		/* end packed optimize */
	struct tty      *rc_tp;                 /* tty struct           */
	u_char          *rc_iptr;               /* Chars input buffer         */
	u_char          *rc_hiwat;              /* hi-water mark        */
	u_char          *rc_bufend;             /* end of buffer        */
	u_char          *rc_optr;               /* ptr in output buf    */
	u_char          *rc_obufend;            /* end of output buf    */
	u_char           rc_ibuf[4 * RC_IBUFSIZE];  /* input buffer         */
	u_char           rc_obuf[RC_OBUFSIZE];  /* output buffer        */
} rc_chans[NRC * CD180_NCHAN];

static int rc_scheduled_event = 0;

/* for pstat -t */
static struct tty rc_tty[NRC * CD180_NCHAN];
static const int  nrc_tty = NRC * CD180_NCHAN;

/* Flags */
#define RC_DTR_OFF      0x0001          /* DTR wait, for close/open     */
#define RC_ACTOUT       0x0002          /* Dial-out port active         */
#define RC_RTSFLOW      0x0004          /* RTS flow ctl enabled         */
#define RC_CTSFLOW      0x0008          /* CTS flow ctl enabled         */
#define RC_DORXFER      0x0010          /* RXFER event planned          */
#define RC_DOXXFER      0x0020          /* XXFER event planned          */
#define RC_MODCHG       0x0040          /* Modem status changed         */
#define RC_OSUSP        0x0080          /* Output suspended             */
#define RC_OSBUSY       0x0100          /* start() routine in progress  */
#define RC_WAS_BUFOVFL  0x0200          /* low-level buffer ovferflow   */
#define RC_WAS_SILOVFL  0x0400          /* silo buffer overflow         */
#define RC_SEND_RDY     0x0800          /* ready to send */

/* Table for translation of RCSR status bits to internal form */
static int rc_rcsrt[16] = {
	0,             TTY_OE,               TTY_FE,
	TTY_FE|TTY_OE, TTY_PE,               TTY_PE|TTY_OE,
	TTY_PE|TTY_FE, TTY_PE|TTY_FE|TTY_OE, TTY_BI,
	TTY_BI|TTY_OE, TTY_BI|TTY_FE,        TTY_BI|TTY_FE|TTY_OE,
	TTY_BI|TTY_PE, TTY_BI|TTY_PE|TTY_OE, TTY_BI|TTY_PE|TTY_FE,
	TTY_BI|TTY_PE|TTY_FE|TTY_OE
};

/* Static prototypes */
static ointhand2_t rcintr;
static void rc_hwreset          __P((int, int, unsigned int));
static int  rc_test             __P((int, int));
static void rc_discard_output   __P((struct rc_chans *));
static void rc_hardclose        __P((struct rc_chans *));
static int  rc_modctl           __P((struct rc_chans *, int, int));
static void rc_start            __P((struct tty *));
static void rc_stop              __P((struct tty *, int rw));
static int  rc_param            __P((struct tty *, struct termios *));
static swihand_t rcpoll;
static void rc_reinit           __P((struct rc_softc *));
#ifdef RCDEBUG
static void printrcflags();
#endif
static timeout_t rc_dtrwakeup;
static timeout_t rc_wakeup;
static void disc_optim		__P((struct tty	*tp, struct termios *t,	struct rc_chans	*));
static void rc_wait0            __P((int nec, int unit, int chan, int line));

/**********************************************/

/* Quick device probing */
static int
rcprobe(dvp)
	struct  isa_device      *dvp;
{
	int             irq = ffs(dvp->id_irq) - 1;
	register int    nec = dvp->id_iobase;

	if (dvp->id_unit > NRC)
		return 0;
	if (!RC_VALIDADDR(nec)) {
		printf("rc%d: illegal base address %x\n", dvp->id_unit, nec);
		return 0;
	}
	if (!RC_VALIDIRQ(irq)) {
		printf("rc%d: illegal IRQ value %d\n", dvp->id_unit, irq);
		return 0;
	}
	rcout(CD180_PPRL, 0x22); /* Random values to Prescale reg. */
	rcout(CD180_PPRH, 0x11);
	if (rcin(CD180_PPRL) != 0x22 || rcin(CD180_PPRH) != 0x11)
		return 0;
	/* Now, test the board more thoroughly, with diagnostic */
	if (rc_test(nec, dvp->id_unit))
		return 0;
	rc_softc[dvp->id_unit].rcb_probed = RC_PROBED;

	return 0xF;
}

static int
rcattach(dvp)
	struct  isa_device      *dvp;
{
	register int            chan, nec = dvp->id_iobase;
	struct rc_softc         *rcb = &rc_softc[dvp->id_unit];
	struct rc_chans         *rc  = &rc_chans[dvp->id_unit * CD180_NCHAN];
	static int              rc_started = 0;
	struct tty              *tp;

	dvp->id_ointr = rcintr;

	/* Thorooughly test the device */
	if (rcb->rcb_probed != RC_PROBED)
		return 0;
	rcb->rcb_addr   = nec;
	rcb->rcb_dtr    = 0;
	rcb->rcb_baserc = rc;
	rcb->rcb_unit	= dvp->id_unit;
	/*rcb->rcb_chipid = 0x10 + dvp->id_unit;*/
	printf("rc%d: %d chans, firmware rev. %c\n", rcb->rcb_unit,
		CD180_NCHAN, (rcin(CD180_GFRCR) & 0xF) + 'A');

	for (chan = 0; chan < CD180_NCHAN; chan++, rc++) {
		rc->rc_rcb     = rcb;
		rc->rc_chan    = chan;
		rc->rc_iptr    = rc->rc_ibuf;
		rc->rc_bufend  = &rc->rc_ibuf[RC_IBUFSIZE];
		rc->rc_hiwat   = &rc->rc_ibuf[RC_IHIGHWATER];
		rc->rc_flags   = rc->rc_ier = rc->rc_msvr = 0;
		rc->rc_cor2    = rc->rc_pendcmd = 0;
		rc->rc_optr    = rc->rc_obufend  = rc->rc_obuf;
		rc->rc_dtrwait = 3 * hz;
		rc->rc_dcdwaits= 0;
		rc->rc_hotchar = 0;
		tp = rc->rc_tp = &rc_tty[chan + (dvp->id_unit * CD180_NCHAN)];
		ttychars(tp);
		tp->t_lflag = tp->t_iflag = tp->t_oflag = 0;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	}
	rcb->rcb_probed = RC_ATTACHED;
	if (!rc_started) {
		cdevsw_add(&rc_cdevsw);
		register_swi(SWI_TTY, rcpoll);
		rc_wakeup((void *)NULL);
		rc_started = 1;
	}
	return 1;
}

/* RC interrupt handling */
static void
rcintr(unit)
	int             unit;
{
	register struct rc_softc        *rcb = &rc_softc[unit];
	register struct rc_chans        *rc;
	register int                    nec, resid;
	register u_char                 val, iack, bsr, ucnt, *optr;
	int                             good_data, t_state;

	if (rcb->rcb_probed != RC_ATTACHED) {
		printf("rc%d: bogus interrupt\n", unit);
		return;
	}
	nec = rcb->rcb_addr;

	bsr = ~(rcin(RC_BSR));

	if (!(bsr & (RC_BSR_TOUT|RC_BSR_RXINT|RC_BSR_TXINT|RC_BSR_MOINT))) {
		printf("rc%d: extra interrupt\n", unit);
		rcout(CD180_EOIR, 0);
		return;
	}

	while (bsr & (RC_BSR_TOUT|RC_BSR_RXINT|RC_BSR_TXINT|RC_BSR_MOINT)) {
#ifdef RCDEBUG_DETAILED
		printf("rc%d: intr (%02x) %s%s%s%s\n", unit, bsr,
			(bsr & RC_BSR_TOUT)?"TOUT ":"",
			(bsr & RC_BSR_RXINT)?"RXINT ":"",
			(bsr & RC_BSR_TXINT)?"TXINT ":"",
			(bsr & RC_BSR_MOINT)?"MOINT":"");
#endif
		if (bsr & RC_BSR_TOUT) {
			printf("rc%d: hardware failure, reset board\n", unit);
			rcout(RC_CTOUT, 0);
			rc_reinit(rcb);
			return;
		}
		if (bsr & RC_BSR_RXINT) {
			iack = rcin(RC_PILR_RX);
			good_data = (iack == (GIVR_IT_RGDI | RC_FAKEID));
			if (!good_data && iack != (GIVR_IT_REI | RC_FAKEID)) {
				printf("rc%d: fake rxint: %02x\n", unit, iack);
				goto more_intrs;
			}
			rc = rcb->rcb_baserc + ((rcin(CD180_GICR) & GICR_CHAN) >> GICR_LSH);
			t_state = rc->rc_tp->t_state;
			/* Do RTS flow control stuff */
			if (  (rc->rc_flags & RC_RTSFLOW)
			    || !(t_state & TS_ISOPEN)
			   ) {
				if (  (   !(t_state & TS_ISOPEN)
				       || (t_state & TS_TBLOCK)
				      )
				    && (rc->rc_msvr & MSVR_RTS)
				   )
					rcout(CD180_MSVR,
						rc->rc_msvr &= ~MSVR_RTS);
				else if (!(rc->rc_msvr & MSVR_RTS))
					rcout(CD180_MSVR,
						rc->rc_msvr |= MSVR_RTS);
			}
			ucnt  = rcin(CD180_RDCR) & 0xF;
			resid = 0;

			if (t_state & TS_ISOPEN) {
				/* check for input buffer overflow */
				if ((rc->rc_iptr + ucnt) >= rc->rc_bufend) {
					resid  = ucnt;
					ucnt   = rc->rc_bufend - rc->rc_iptr;
					resid -= ucnt;
					if (!(rc->rc_flags & RC_WAS_BUFOVFL)) {
						rc->rc_flags |= RC_WAS_BUFOVFL;
						rc_scheduled_event++;
					}
				}
				optr = rc->rc_iptr;
				/* check foor good data */
				if (good_data) {
					while (ucnt-- > 0) {
						val = rcin(CD180_RDR);
						optr[0] = val;
						optr[INPUT_FLAGS_SHIFT] = 0;
						optr++;
						rc_scheduled_event++;
						if (val != 0 && val == rc->rc_hotchar)
							setsofttty();
					}
				} else {
					/* Store also status data */
					while (ucnt-- > 0) {
						iack = rcin(CD180_RCSR);
						if (iack & RCSR_Timeout)
							break;
						if (   (iack & RCSR_OE)
						    && !(rc->rc_flags & RC_WAS_SILOVFL)) {
							rc->rc_flags |= RC_WAS_SILOVFL;
							rc_scheduled_event++;
						}
						val = rcin(CD180_RDR);
						/*
						  Don't store PE if IGNPAR and BREAK if IGNBRK,
						  this hack allows "raw" tty optimization
						  works even if IGN* is set.
						*/
						if (   !(iack & (RCSR_PE|RCSR_FE|RCSR_Break))
						    || ((!(iack & (RCSR_PE|RCSR_FE))
						    ||  !(rc->rc_tp->t_iflag & IGNPAR))
						    && (!(iack & RCSR_Break)
						    ||  !(rc->rc_tp->t_iflag & IGNBRK)))) {
							if (   (iack & (RCSR_PE|RCSR_FE))
							    && (t_state & TS_CAN_BYPASS_L_RINT)
							    && ((iack & RCSR_FE)
							    ||  ((iack & RCSR_PE)
							    &&  (rc->rc_tp->t_iflag & INPCK))))
								val = 0;
							else if (val != 0 && val == rc->rc_hotchar)
								setsofttty();
							optr[0] = val;
							optr[INPUT_FLAGS_SHIFT] = iack;
							optr++;
							rc_scheduled_event++;
						}
					}
				}
				rc->rc_iptr = optr;
				rc->rc_flags |= RC_DORXFER;
			} else
				resid = ucnt;
			/* Clear FIFO if necessary */
			while (resid-- > 0) {
				if (!good_data)
					iack = rcin(CD180_RCSR);
				else
					iack = 0;
				if (iack & RCSR_Timeout)
					break;
				(void) rcin(CD180_RDR);
			}
			goto more_intrs;
		}
		if (bsr & RC_BSR_MOINT) {
			iack = rcin(RC_PILR_MODEM);
			if (iack != (GIVR_IT_MSCI | RC_FAKEID)) {
				printf("rc%d: fake moint: %02x\n", unit, iack);
				goto more_intrs;
			}
			rc = rcb->rcb_baserc + ((rcin(CD180_GICR) & GICR_CHAN) >> GICR_LSH);
			iack = rcin(CD180_MCR);
			rc->rc_msvr = rcin(CD180_MSVR);
			rcout(CD180_MCR, 0);
#ifdef RCDEBUG
			printrcflags(rc, "moint");
#endif
			if (rc->rc_flags & RC_CTSFLOW) {
				if (rc->rc_msvr & MSVR_CTS)
					rc->rc_flags |= RC_SEND_RDY;
				else
					rc->rc_flags &= ~RC_SEND_RDY;
			} else
				rc->rc_flags |= RC_SEND_RDY;
			if ((iack & MCR_CDchg) && !(rc->rc_flags & RC_MODCHG)) {
				rc_scheduled_event += LOTS_OF_EVENTS;
				rc->rc_flags |= RC_MODCHG;
				setsofttty();
			}
			goto more_intrs;
		}
		if (bsr & RC_BSR_TXINT) {
			iack = rcin(RC_PILR_TX);
			if (iack != (GIVR_IT_TDI | RC_FAKEID)) {
				printf("rc%d: fake txint: %02x\n", unit, iack);
				goto more_intrs;
			}
			rc = rcb->rcb_baserc + ((rcin(CD180_GICR) & GICR_CHAN) >> GICR_LSH);
			if (    (rc->rc_flags & RC_OSUSP)
			    || !(rc->rc_flags & RC_SEND_RDY)
			   )
				goto more_intrs;
			/* Handle breaks and other stuff */
			if (rc->rc_pendcmd) {
				rcout(CD180_COR2, rc->rc_cor2 |= COR2_ETC);
				rcout(CD180_TDR,  CD180_C_ESC);
				rcout(CD180_TDR,  rc->rc_pendcmd);
				rcout(CD180_COR2, rc->rc_cor2 &= ~COR2_ETC);
				rc->rc_pendcmd = 0;
				goto more_intrs;
			}
			optr = rc->rc_optr;
			resid = rc->rc_obufend - optr;
			if (resid > CD180_NFIFO)
				resid = CD180_NFIFO;
			while (resid-- > 0)
				rcout(CD180_TDR, *optr++);
			rc->rc_optr = optr;

			/* output completed? */
			if (optr >= rc->rc_obufend) {
				rcout(CD180_IER, rc->rc_ier &= ~IER_TxRdy);
#ifdef RCDEBUG
				printf("rc%d/%d: output completed\n", unit, rc->rc_chan);
#endif
				if (!(rc->rc_flags & RC_DOXXFER)) {
					rc_scheduled_event += LOTS_OF_EVENTS;
					rc->rc_flags |= RC_DOXXFER;
					setsofttty();
				}
			}
		}
	more_intrs:
		rcout(CD180_EOIR, 0);   /* end of interrupt */
		rcout(RC_CTOUT, 0);
		bsr = ~(rcin(RC_BSR));
	}
}

/* Feed characters to output buffer */
static void rc_start(tp)
register struct tty *tp;
{
	register struct rc_chans       *rc = &rc_chans[GET_UNIT(tp->t_dev)];
	register int                    nec = rc->rc_rcb->rcb_addr, s;

	if (rc->rc_flags & RC_OSBUSY)
		return;
	s = spltty();
	rc->rc_flags |= RC_OSBUSY;
	disable_intr();
	if (tp->t_state & TS_TTSTOP)
		rc->rc_flags |= RC_OSUSP;
	else
		rc->rc_flags &= ~RC_OSUSP;
	/* Do RTS flow control stuff */
	if (   (rc->rc_flags & RC_RTSFLOW)
	    && (tp->t_state & TS_TBLOCK)
	    && (rc->rc_msvr & MSVR_RTS)
	   ) {
		rcout(CD180_CAR, rc->rc_chan);
		rcout(CD180_MSVR, rc->rc_msvr &= ~MSVR_RTS);
	} else if (!(rc->rc_msvr & MSVR_RTS)) {
		rcout(CD180_CAR, rc->rc_chan);
		rcout(CD180_MSVR, rc->rc_msvr |= MSVR_RTS);
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;
#ifdef RCDEBUG
	printrcflags(rc, "rcstart");
#endif
	ttwwakeup(tp);
#ifdef RCDEBUG
	printf("rcstart: outq = %d obuf = %d\n",
		tp->t_outq.c_cc, rc->rc_obufend - rc->rc_optr);
#endif
	if (tp->t_state & TS_BUSY)
		goto    out;    /* output still in progress ... */

	if (tp->t_outq.c_cc > 0) {
		u_int   ocnt;

		tp->t_state |= TS_BUSY;
		ocnt = q_to_b(&tp->t_outq, rc->rc_obuf, sizeof rc->rc_obuf);
		disable_intr();
		rc->rc_optr = rc->rc_obuf;
		rc->rc_obufend = rc->rc_optr + ocnt;
		enable_intr();
		if (!(rc->rc_ier & IER_TxRdy)) {
#ifdef RCDEBUG
			printf("rc%d/%d: rcstart enable txint\n", rc->rc_rcb->rcb_unit, rc->rc_chan);
#endif
			rcout(CD180_CAR, rc->rc_chan);
			rcout(CD180_IER, rc->rc_ier |= IER_TxRdy);
		}
	}
out:
	rc->rc_flags &= ~RC_OSBUSY;
	(void) splx(s);
}

/* Handle delayed events. */
void rcpoll()
{
	register struct rc_chans *rc;
	register struct rc_softc *rcb;
	register u_char        *tptr, *eptr;
	register struct tty    *tp;
	register int            chan, icnt, nec, unit;

	if (rc_scheduled_event == 0)
		return;
repeat:
	for (unit = 0; unit < NRC; unit++) {
		rcb = &rc_softc[unit];
		rc = rcb->rcb_baserc;
		nec = rc->rc_rcb->rcb_addr;
		for (chan = 0; chan < CD180_NCHAN; rc++, chan++) {
			tp = rc->rc_tp;
#ifdef RCDEBUG
			if (rc->rc_flags & (RC_DORXFER|RC_DOXXFER|RC_MODCHG|
			    RC_WAS_BUFOVFL|RC_WAS_SILOVFL))
				printrcflags(rc, "rcevent");
#endif
			if (rc->rc_flags & RC_WAS_BUFOVFL) {
				disable_intr();
				rc->rc_flags &= ~RC_WAS_BUFOVFL;
				rc_scheduled_event--;
				enable_intr();
				printf("rc%d/%d: interrupt-level buffer overflow\n",
					unit, chan);
			}
			if (rc->rc_flags & RC_WAS_SILOVFL) {
				disable_intr();
				rc->rc_flags &= ~RC_WAS_SILOVFL;
				rc_scheduled_event--;
				enable_intr();
				printf("rc%d/%d: silo overflow\n",
					unit, chan);
			}
			if (rc->rc_flags & RC_MODCHG) {
				disable_intr();
				rc->rc_flags &= ~RC_MODCHG;
				rc_scheduled_event -= LOTS_OF_EVENTS;
				enable_intr();
				(*linesw[tp->t_line].l_modem)(tp, !!(rc->rc_msvr & MSVR_CD));
			}
			if (rc->rc_flags & RC_DORXFER) {
				disable_intr();
				rc->rc_flags &= ~RC_DORXFER;
				eptr = rc->rc_iptr;
				if (rc->rc_bufend == &rc->rc_ibuf[2 * RC_IBUFSIZE])
					tptr = &rc->rc_ibuf[RC_IBUFSIZE];
				else
					tptr = rc->rc_ibuf;
				icnt = eptr - tptr;
				if (icnt > 0) {
					if (rc->rc_bufend == &rc->rc_ibuf[2 * RC_IBUFSIZE]) {
						rc->rc_iptr   = rc->rc_ibuf;
						rc->rc_bufend = &rc->rc_ibuf[RC_IBUFSIZE];
						rc->rc_hiwat  = &rc->rc_ibuf[RC_IHIGHWATER];
					} else {
						rc->rc_iptr   = &rc->rc_ibuf[RC_IBUFSIZE];
						rc->rc_bufend = &rc->rc_ibuf[2 * RC_IBUFSIZE];
						rc->rc_hiwat  =
							&rc->rc_ibuf[RC_IBUFSIZE + RC_IHIGHWATER];
					}
					if (   (rc->rc_flags & RC_RTSFLOW)
					    && (tp->t_state & TS_ISOPEN)
					    && !(tp->t_state & TS_TBLOCK)
					    && !(rc->rc_msvr & MSVR_RTS)
					    ) {
						rcout(CD180_CAR, chan);
						rcout(CD180_MSVR,
							rc->rc_msvr |= MSVR_RTS);
					}
					rc_scheduled_event -= icnt;
				}
				enable_intr();

				if (icnt <= 0 || !(tp->t_state & TS_ISOPEN))
					goto done1;

				if (   (tp->t_state & TS_CAN_BYPASS_L_RINT)
				    && !(tp->t_state & TS_LOCAL)) {
					if ((tp->t_rawq.c_cc + icnt) >= RB_I_HIGH_WATER
					    && ((rc->rc_flags & RC_RTSFLOW) || (tp->t_iflag & IXOFF))
					    && !(tp->t_state & TS_TBLOCK))
						ttyblock(tp);
					tk_nin += icnt;
					tk_rawcc += icnt;
					tp->t_rawcc += icnt;
					if (b_to_q(tptr, icnt, &tp->t_rawq))
						printf("rc%d/%d: tty-level buffer overflow\n",
							unit, chan);
					ttwakeup(tp);
					if ((tp->t_state & TS_TTSTOP) && ((tp->t_iflag & IXANY)
					    || (tp->t_cc[VSTART] == tp->t_cc[VSTOP]))) {
						tp->t_state &= ~TS_TTSTOP;
						tp->t_lflag &= ~FLUSHO;
						rc_start(tp);
					}
				} else {
					for (; tptr < eptr; tptr++)
						(*linesw[tp->t_line].l_rint)
						    (tptr[0] |
						    rc_rcsrt[tptr[INPUT_FLAGS_SHIFT] & 0xF], tp);
				}
done1: ;
			}
			if (rc->rc_flags & RC_DOXXFER) {
				disable_intr();
				rc_scheduled_event -= LOTS_OF_EVENTS;
				rc->rc_flags &= ~RC_DOXXFER;
				rc->rc_tp->t_state &= ~TS_BUSY;
				enable_intr();
				(*linesw[tp->t_line].l_start)(tp);
			}
		}
		if (rc_scheduled_event == 0)
			break;
	}
	if (rc_scheduled_event >= LOTS_OF_EVENTS)
		goto repeat;
}

static	void
rc_stop(tp, rw)
	register struct tty     *tp;
	int                     rw;
{
	register struct rc_chans        *rc = &rc_chans[GET_UNIT(tp->t_dev)];
	u_char *tptr, *eptr;

#ifdef RCDEBUG
	printf("rc%d/%d: rc_stop %s%s\n", rc->rc_rcb->rcb_unit, rc->rc_chan,
		(rw & FWRITE)?"FWRITE ":"", (rw & FREAD)?"FREAD":"");
#endif
	if (rw & FWRITE)
		rc_discard_output(rc);
	disable_intr();
	if (rw & FREAD) {
		rc->rc_flags &= ~RC_DORXFER;
		eptr = rc->rc_iptr;
		if (rc->rc_bufend == &rc->rc_ibuf[2 * RC_IBUFSIZE]) {
			tptr = &rc->rc_ibuf[RC_IBUFSIZE];
			rc->rc_iptr = &rc->rc_ibuf[RC_IBUFSIZE];
		} else {
			tptr = rc->rc_ibuf;
			rc->rc_iptr = rc->rc_ibuf;
		}
		rc_scheduled_event -= eptr - tptr;
	}
	if (tp->t_state & TS_TTSTOP)
		rc->rc_flags |= RC_OSUSP;
	else
		rc->rc_flags &= ~RC_OSUSP;
	enable_intr();
}

static	int
rcopen(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	register struct rc_chans *rc;
	register struct tty      *tp;
	int             unit, nec, s, error = 0;

	unit = GET_UNIT(dev);
	if (unit >= NRC * CD180_NCHAN)
		return ENXIO;
	if (rc_softc[unit / CD180_NCHAN].rcb_probed != RC_ATTACHED)
		return ENXIO;
	rc  = &rc_chans[unit];
	tp  = rc->rc_tp;
	dev->si_tty = tp;
	nec = rc->rc_rcb->rcb_addr;
#ifdef RCDEBUG
	printf("rc%d/%d: rcopen: dev %x\n", rc->rc_rcb->rcb_unit, unit, dev);
#endif
	s = spltty();

again:
	while (rc->rc_flags & RC_DTR_OFF) {
		error = tsleep(&(rc->rc_dtrwait), TTIPRI | PCATCH, "rcdtr", 0);
		if (error != 0)
			goto out;
	}
	if (tp->t_state & TS_ISOPEN) {
		if (CALLOUT(dev)) {
			if (!(rc->rc_flags & RC_ACTOUT)) {
				error = EBUSY;
				goto out;
			}
		} else {
			if (rc->rc_flags & RC_ACTOUT) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto out;
				}
				error = tsleep(&rc->rc_rcb,
				     TTIPRI|PCATCH, "rcbi", 0);
				if (error)
					goto out;
				goto again;
			}
		}
		if (tp->t_state & TS_XCLUDE &&
		    suser(p)) {
			error = EBUSY;
			goto out;
		}
	} else {
		tp->t_oproc   = rc_start;
		tp->t_param   = rc_param;
		tp->t_stop    = rc_stop;
		tp->t_dev     = dev;

		if (CALLOUT(dev))
			tp->t_cflag |= CLOCAL;
		else
			tp->t_cflag &= ~CLOCAL;

		error = rc_param(tp, &tp->t_termios);
		if (error)
			goto out;
		(void) rc_modctl(rc, TIOCM_RTS|TIOCM_DTR, DMSET);

		if ((rc->rc_msvr & MSVR_CD) || CALLOUT(dev))
			(*linesw[tp->t_line].l_modem)(tp, 1);
	}
	if (!(tp->t_state & TS_CARR_ON) && !CALLOUT(dev)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		rc->rc_dcdwaits++;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "rcdcd", 0);
		rc->rc_dcdwaits--;
		if (error != 0)
			goto out;
		goto again;
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	disc_optim(tp, &tp->t_termios, rc);
	if ((tp->t_state & TS_ISOPEN) && CALLOUT(dev))
		rc->rc_flags |= RC_ACTOUT;
out:
	(void) splx(s);

	if(rc->rc_dcdwaits == 0 && !(tp->t_state & TS_ISOPEN))
		rc_hardclose(rc);

	return error;
}

static	int
rcclose(dev, flag, mode, p)
	dev_t           dev;
	int             flag, mode;
	struct proc    *p;
{
	register struct rc_chans *rc;
	register struct tty      *tp;
	int  s, unit = GET_UNIT(dev);

	if (unit >= NRC * CD180_NCHAN)
		return ENXIO;
	rc  = &rc_chans[unit];
	tp  = rc->rc_tp;
#ifdef RCDEBUG
	printf("rc%d/%d: rcclose dev %x\n", rc->rc_rcb->rcb_unit, unit, dev);
#endif
	s = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	disc_optim(tp, &tp->t_termios, rc);
	rc_stop(tp, FREAD | FWRITE);
	rc_hardclose(rc);
	ttyclose(tp);
	splx(s);
	return 0;
}

static void rc_hardclose(rc)
register struct rc_chans *rc;
{
	register int s, nec = rc->rc_rcb->rcb_addr;
	register struct tty *tp = rc->rc_tp;

	s = spltty();
	rcout(CD180_CAR, rc->rc_chan);

	/* Disable rx/tx intrs */
	rcout(CD180_IER, rc->rc_ier = 0);
	if (   (tp->t_cflag & HUPCL)
	    || (!(rc->rc_flags & RC_ACTOUT)
	       && !(rc->rc_msvr & MSVR_CD)
	       && !(tp->t_cflag & CLOCAL))
	    || !(tp->t_state & TS_ISOPEN)
	   ) {
		CCRCMD(rc->rc_rcb->rcb_unit, rc->rc_chan, CCR_ResetChan);
		WAITFORCCR(rc->rc_rcb->rcb_unit, rc->rc_chan);
		(void) rc_modctl(rc, TIOCM_RTS, DMSET);
		if (rc->rc_dtrwait) {
			timeout(rc_dtrwakeup, rc, rc->rc_dtrwait);
			rc->rc_flags |= RC_DTR_OFF;
		}
	}
	rc->rc_flags &= ~RC_ACTOUT;
	wakeup((caddr_t) &rc->rc_rcb);  /* wake bi */
	wakeup(TSA_CARR_ON(tp));
	(void) splx(s);
}

/* Reset the bastard */
static void rc_hwreset(unit, nec, chipid)
	register int    unit, nec;
	unsigned int    chipid;
{
	CCRCMD(unit, -1, CCR_HWRESET);            /* Hardware reset */
	DELAY(20000);
	WAITFORCCR(unit, -1);

	rcout(RC_CTOUT, 0);             /* Clear timeout  */
	rcout(CD180_GIVR,  chipid);
	rcout(CD180_GICR,  0);

	/* Set Prescaler Registers (1 msec) */
	rcout(CD180_PPRL, ((RC_OSCFREQ + 999) / 1000) & 0xFF);
	rcout(CD180_PPRH, ((RC_OSCFREQ + 999) / 1000) >> 8);

	/* Initialize Priority Interrupt Level Registers */
	rcout(CD180_PILR1, RC_PILR_MODEM);
	rcout(CD180_PILR2, RC_PILR_TX);
	rcout(CD180_PILR3, RC_PILR_RX);

	/* Reset DTR */
	rcout(RC_DTREG, ~0);
}

/* Set channel parameters */
static int rc_param(tp, ts)
	register struct  tty    *tp;
	struct termios          *ts;
{
	register struct rc_chans *rc = &rc_chans[GET_UNIT(tp->t_dev)];
	register int    nec = rc->rc_rcb->rcb_addr;
	int      idivs, odivs, s, val, cflag, iflag, lflag, inpflow;

	if (   ts->c_ospeed < 0 || ts->c_ospeed > 76800
	    || ts->c_ispeed < 0 || ts->c_ispeed > 76800
	   )
		return (EINVAL);
	if (ts->c_ispeed == 0)
		ts->c_ispeed = ts->c_ospeed;
	odivs = RC_BRD(ts->c_ospeed);
	idivs = RC_BRD(ts->c_ispeed);

	s = spltty();

	/* Select channel */
	rcout(CD180_CAR, rc->rc_chan);

	/* If speed == 0, hangup line */
	if (ts->c_ospeed == 0) {
		CCRCMD(rc->rc_rcb->rcb_unit, rc->rc_chan, CCR_ResetChan);
		WAITFORCCR(rc->rc_rcb->rcb_unit, rc->rc_chan);
		(void) rc_modctl(rc, TIOCM_DTR, DMBIC);
	}

	tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	cflag = ts->c_cflag;
	iflag = ts->c_iflag;
	lflag = ts->c_lflag;

	if (idivs > 0) {
		rcout(CD180_RBPRL, idivs & 0xFF);
		rcout(CD180_RBPRH, idivs >> 8);
	}
	if (odivs > 0) {
		rcout(CD180_TBPRL, odivs & 0xFF);
		rcout(CD180_TBPRH, odivs >> 8);
	}

	/* set timeout value */
	if (ts->c_ispeed > 0) {
		int itm = ts->c_ispeed > 2400 ? 5 : 10000 / ts->c_ispeed + 1;

		if (   !(lflag & ICANON)
		    && ts->c_cc[VMIN] != 0 && ts->c_cc[VTIME] != 0
		    && ts->c_cc[VTIME] * 10 > itm)
			itm = ts->c_cc[VTIME] * 10;

		rcout(CD180_RTPR, itm <= 255 ? itm : 255);
	}

	switch (cflag & CSIZE) {
		case CS5:       val = COR1_5BITS;      break;
		case CS6:       val = COR1_6BITS;      break;
		case CS7:       val = COR1_7BITS;      break;
		default:
		case CS8:       val = COR1_8BITS;      break;
	}
	if (cflag & PARENB) {
		val |= COR1_NORMPAR;
		if (cflag & PARODD)
			val |= COR1_ODDP;
		if (!(cflag & INPCK))
			val |= COR1_Ignore;
	} else
		val |= COR1_Ignore;
	if (cflag & CSTOPB)
		val |= COR1_2SB;
	rcout(CD180_COR1, val);

	/* Set FIFO threshold */
	val = ts->c_ospeed <= 4800 ? 1 : CD180_NFIFO / 2;
	inpflow = 0;
	if (   (iflag & IXOFF)
	    && (   ts->c_cc[VSTOP] != _POSIX_VDISABLE
		&& (   ts->c_cc[VSTART] != _POSIX_VDISABLE
		    || (iflag & IXANY)
		   )
	       )
	   ) {
		inpflow = 1;
		val |= COR3_SCDE|COR3_FCT;
	}
	rcout(CD180_COR3, val);

	/* Initialize on-chip automatic flow control */
	val = 0;
	rc->rc_flags &= ~(RC_CTSFLOW|RC_SEND_RDY);
	if (cflag & CCTS_OFLOW) {
		rc->rc_flags |= RC_CTSFLOW;
		val |= COR2_CtsAE;
	} else
		rc->rc_flags |= RC_SEND_RDY;
	if (tp->t_state & TS_TTSTOP)
		rc->rc_flags |= RC_OSUSP;
	else
		rc->rc_flags &= ~RC_OSUSP;
	if (cflag & CRTS_IFLOW)
		rc->rc_flags |= RC_RTSFLOW;
	else
		rc->rc_flags &= ~RC_RTSFLOW;

	if (inpflow) {
		if (ts->c_cc[VSTART] != _POSIX_VDISABLE)
			rcout(CD180_SCHR1, ts->c_cc[VSTART]);
		rcout(CD180_SCHR2, ts->c_cc[VSTOP]);
		val |= COR2_TxIBE;
		if (iflag & IXANY)
			val |= COR2_IXM;
	}

	rcout(CD180_COR2, rc->rc_cor2 = val);

	CCRCMD(rc->rc_rcb->rcb_unit, rc->rc_chan,
		CCR_CORCHG1 | CCR_CORCHG2 | CCR_CORCHG3);

	disc_optim(tp, ts, rc);

	/* modem ctl */
	val = cflag & CLOCAL ? 0 : MCOR1_CDzd;
	if (cflag & CCTS_OFLOW)
		val |= MCOR1_CTSzd;
	rcout(CD180_MCOR1, val);

	val = cflag & CLOCAL ? 0 : MCOR2_CDod;
	if (cflag & CCTS_OFLOW)
		val |= MCOR2_CTSod;
	rcout(CD180_MCOR2, val);

	/* enable i/o and interrupts */
	CCRCMD(rc->rc_rcb->rcb_unit, rc->rc_chan,
		CCR_XMTREN | ((cflag & CREAD) ? CCR_RCVREN : CCR_RCVRDIS));
	WAITFORCCR(rc->rc_rcb->rcb_unit, rc->rc_chan);

	rc->rc_ier = cflag & CLOCAL ? 0 : IER_CD;
	if (cflag & CCTS_OFLOW)
		rc->rc_ier |= IER_CTS;
	if (cflag & CREAD)
		rc->rc_ier |= IER_RxData;
	if (tp->t_state & TS_BUSY)
		rc->rc_ier |= IER_TxRdy;
	if (ts->c_ospeed != 0)
		rc_modctl(rc, TIOCM_DTR, DMBIS);
	if ((cflag & CCTS_OFLOW) && (rc->rc_msvr & MSVR_CTS))
		rc->rc_flags |= RC_SEND_RDY;
	rcout(CD180_IER, rc->rc_ier);
	(void) splx(s);
	return 0;
}

/* Re-initialize board after bogus interrupts */
static void rc_reinit(rcb)
struct rc_softc         *rcb;
{
	register struct rc_chans       *rc, *rce;
	register int                    nec;

	nec = rcb->rcb_addr;
	rc_hwreset(rcb->rcb_unit, nec, RC_FAKEID);
	rc  = &rc_chans[rcb->rcb_unit * CD180_NCHAN];
	rce = rc + CD180_NCHAN;
	for (; rc < rce; rc++)
		(void) rc_param(rc->rc_tp, &rc->rc_tp->t_termios);
}

static	int
rcioctl(dev, cmd, data, flag, p)
dev_t           dev;
u_long          cmd;
int		flag;
caddr_t         data;
struct proc     *p;
{
	register struct rc_chans       *rc = &rc_chans[GET_UNIT(dev)];
	register int                    s, error;
	struct tty                     *tp = rc->rc_tp;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	disc_optim(tp, &tp->t_termios, rc);
	if (error != ENOIOCTL)
		return (error);
	s = spltty();

	switch (cmd) {
	    case TIOCSBRK:
		rc->rc_pendcmd = CD180_C_SBRK;
		break;

	    case TIOCCBRK:
		rc->rc_pendcmd = CD180_C_EBRK;
		break;

	    case TIOCSDTR:
		(void) rc_modctl(rc, TIOCM_DTR, DMBIS);
		break;

	    case TIOCCDTR:
		(void) rc_modctl(rc, TIOCM_DTR, DMBIC);
		break;

	    case TIOCMGET:
		*(int *) data = rc_modctl(rc, 0, DMGET);
		break;

	    case TIOCMSET:
		(void) rc_modctl(rc, *(int *) data, DMSET);
		break;

	    case TIOCMBIC:
		(void) rc_modctl(rc, *(int *) data, DMBIC);
		break;

	    case TIOCMBIS:
		(void) rc_modctl(rc, *(int *) data, DMBIS);
		break;

	    case TIOCMSDTRWAIT:
		error = suser(p);
		if (error != 0) {
			splx(s);
			return (error);
		}
		rc->rc_dtrwait = *(int *)data * hz / 100;
		break;

	    case TIOCMGDTRWAIT:
		*(int *)data = rc->rc_dtrwait * 100 / hz;
		break;

	    default:
		(void) splx(s);
		return ENOTTY;
	}
	(void) splx(s);
	return 0;
}


/* Modem control routines */

static int rc_modctl(rc, bits, cmd)
register struct rc_chans       *rc;
int                             bits, cmd;
{
	register int    nec = rc->rc_rcb->rcb_addr;
	u_char         *dtr = &rc->rc_rcb->rcb_dtr, msvr;

	rcout(CD180_CAR, rc->rc_chan);

	switch (cmd) {
	    case DMSET:
		rcout(RC_DTREG, (bits & TIOCM_DTR) ?
				~(*dtr |= 1 << rc->rc_chan) :
				~(*dtr &= ~(1 << rc->rc_chan)));
		msvr = rcin(CD180_MSVR);
		if (bits & TIOCM_RTS)
			msvr |= MSVR_RTS;
		else
			msvr &= ~MSVR_RTS;
		if (bits & TIOCM_DTR)
			msvr |= MSVR_DTR;
		else
			msvr &= ~MSVR_DTR;
		rcout(CD180_MSVR, msvr);
		break;

	    case DMBIS:
		if (bits & TIOCM_DTR)
			rcout(RC_DTREG, ~(*dtr |= 1 << rc->rc_chan));
		msvr = rcin(CD180_MSVR);
		if (bits & TIOCM_RTS)
			msvr |= MSVR_RTS;
		if (bits & TIOCM_DTR)
			msvr |= MSVR_DTR;
		rcout(CD180_MSVR, msvr);
		break;

	    case DMGET:
		bits = TIOCM_LE;
		msvr = rc->rc_msvr = rcin(CD180_MSVR);

		if (msvr & MSVR_RTS)
			bits |= TIOCM_RTS;
		if (msvr & MSVR_CTS)
			bits |= TIOCM_CTS;
		if (msvr & MSVR_DSR)
			bits |= TIOCM_DSR;
		if (msvr & MSVR_DTR)
			bits |= TIOCM_DTR;
		if (msvr & MSVR_CD)
			bits |= TIOCM_CD;
		if (~rcin(RC_RIREG) & (1 << rc->rc_chan))
			bits |= TIOCM_RI;
		return bits;

	    case DMBIC:
		if (bits & TIOCM_DTR)
			rcout(RC_DTREG, ~(*dtr &= ~(1 << rc->rc_chan)));
		msvr = rcin(CD180_MSVR);
		if (bits & TIOCM_RTS)
			msvr &= ~MSVR_RTS;
		if (bits & TIOCM_DTR)
			msvr &= ~MSVR_DTR;
		rcout(CD180_MSVR, msvr);
		break;
	}
	rc->rc_msvr = rcin(CD180_MSVR);
	return 0;
}

/* Test the board. */
int rc_test(nec, unit)
	register int    nec;
	int             unit;
{
	int     chan = 0;
	int     i = 0, rcnt, old_level;
	unsigned int    iack, chipid;
	unsigned short  divs;
	static  u_char  ctest[] = "\377\125\252\045\244\0\377";
#define CTLEN   8
#define ERR(s)  { \
		printf("rc%d: ", unit); printf s ; printf("\n"); \
		(void) splx(old_level); return 1; }

	struct rtest {
		u_char  txbuf[CD180_NFIFO];     /* TX buffer  */
		u_char  rxbuf[CD180_NFIFO];     /* RX buffer  */
		int     rxptr;                  /* RX pointer */
		int     txptr;                  /* TX pointer */
	} tchans[CD180_NCHAN];

	old_level = spltty();

	chipid = RC_FAKEID;

	/* First, reset board to inital state */
	rc_hwreset(unit, nec, chipid);

	divs = RC_BRD(19200);

	/* Initialize channels */
	for (chan = 0; chan < CD180_NCHAN; chan++) {

		/* Select and reset channel */
		rcout(CD180_CAR, chan);
		CCRCMD(unit, chan, CCR_ResetChan);
		WAITFORCCR(unit, chan);

		/* Set speed */
		rcout(CD180_RBPRL, divs & 0xFF);
		rcout(CD180_RBPRH, divs >> 8);
		rcout(CD180_TBPRL, divs & 0xFF);
		rcout(CD180_TBPRH, divs >> 8);

		/* set timeout value */
		rcout(CD180_RTPR,  0);

		/* Establish local loopback */
		rcout(CD180_COR1, COR1_NOPAR | COR1_8BITS | COR1_1SB);
		rcout(CD180_COR2, COR2_LLM);
		rcout(CD180_COR3, CD180_NFIFO);
		CCRCMD(unit, chan, CCR_CORCHG1 | CCR_CORCHG2 | CCR_CORCHG3);
		CCRCMD(unit, chan, CCR_RCVREN | CCR_XMTREN);
		WAITFORCCR(unit, chan);
		rcout(CD180_MSVR, MSVR_RTS);

		/* Fill TXBUF with test data */
		for (i = 0; i < CD180_NFIFO; i++) {
			tchans[chan].txbuf[i] = ctest[i];
			tchans[chan].rxbuf[i] = 0;
		}
		tchans[chan].txptr = tchans[chan].rxptr = 0;

		/* Now, start transmit */
		rcout(CD180_IER, IER_TxMpty|IER_RxData);
	}
	/* Pseudo-interrupt poll stuff */
	for (rcnt = 10000; rcnt-- > 0; rcnt--) {
		i = ~(rcin(RC_BSR));
		if (i & RC_BSR_TOUT)
			ERR(("BSR timeout bit set\n"))
		else if (i & RC_BSR_TXINT) {
			iack = rcin(RC_PILR_TX);
			if (iack != (GIVR_IT_TDI | chipid))
				ERR(("Bad TX intr ack (%02x != %02x)\n",
					iack, GIVR_IT_TDI | chipid));
			chan = (rcin(CD180_GICR) & GICR_CHAN) >> GICR_LSH;
			/* If no more data to transmit, disable TX intr */
			if (tchans[chan].txptr >= CD180_NFIFO) {
				iack = rcin(CD180_IER);
				rcout(CD180_IER, iack & ~IER_TxMpty);
			} else {
				for (iack = tchans[chan].txptr;
				    iack < CD180_NFIFO; iack++)
					rcout(CD180_TDR,
					    tchans[chan].txbuf[iack]);
				tchans[chan].txptr = iack;
			}
			rcout(CD180_EOIR, 0);
		} else if (i & RC_BSR_RXINT) {
			u_char ucnt;

			iack = rcin(RC_PILR_RX);
			if (iack != (GIVR_IT_RGDI | chipid) &&
			    iack != (GIVR_IT_REI  | chipid))
				ERR(("Bad RX intr ack (%02x != %02x)\n",
					iack, GIVR_IT_RGDI | chipid))
			chan = (rcin(CD180_GICR) & GICR_CHAN) >> GICR_LSH;
			ucnt = rcin(CD180_RDCR) & 0xF;
			while (ucnt-- > 0) {
				iack = rcin(CD180_RCSR);
				if (iack & RCSR_Timeout)
					break;
				if (iack & 0xF)
					ERR(("Bad char chan %d (RCSR = %02X)\n",
					    chan, iack))
				if (tchans[chan].rxptr > CD180_NFIFO)
					ERR(("Got extra chars chan %d\n",
					    chan))
				tchans[chan].rxbuf[tchans[chan].rxptr++] =
					rcin(CD180_RDR);
			}
			rcout(CD180_EOIR, 0);
		}
		rcout(RC_CTOUT, 0);
		for (iack = chan = 0; chan < CD180_NCHAN; chan++)
			if (tchans[chan].rxptr >= CD180_NFIFO)
				iack++;
		if (iack == CD180_NCHAN)
			break;
	}
	for (chan = 0; chan < CD180_NCHAN; chan++) {
		/* Select and reset channel */
		rcout(CD180_CAR, chan);
		CCRCMD(unit, chan, CCR_ResetChan);
	}

	if (!rcnt)
		ERR(("looses characters during local loopback\n"))
	/* Now, check data */
	for (chan = 0; chan < CD180_NCHAN; chan++)
		for (i = 0; i < CD180_NFIFO; i++)
			if (ctest[i] != tchans[chan].rxbuf[i])
				ERR(("data mismatch chan %d ptr %d (%d != %d)\n",
				    chan, i, ctest[i], tchans[chan].rxbuf[i]))
	(void) splx(old_level);
	return 0;
}

#ifdef RCDEBUG
static void printrcflags(rc, comment)
struct rc_chans  *rc;
char             *comment;
{
	u_short f = rc->rc_flags;
	register int    nec = rc->rc_rcb->rcb_addr;

	printf("rc%d/%d: %s flags: %s%s%s%s%s%s%s%s%s%s%s%s\n",
		rc->rc_rcb->rcb_unit, rc->rc_chan, comment,
		(f & RC_DTR_OFF)?"DTR_OFF " :"",
		(f & RC_ACTOUT) ?"ACTOUT " :"",
		(f & RC_RTSFLOW)?"RTSFLOW " :"",
		(f & RC_CTSFLOW)?"CTSFLOW " :"",
		(f & RC_DORXFER)?"DORXFER " :"",
		(f & RC_DOXXFER)?"DOXXFER " :"",
		(f & RC_MODCHG) ?"MODCHG "  :"",
		(f & RC_OSUSP)  ?"OSUSP " :"",
		(f & RC_OSBUSY) ?"OSBUSY " :"",
		(f & RC_WAS_BUFOVFL) ?"BUFOVFL " :"",
		(f & RC_WAS_SILOVFL) ?"SILOVFL " :"",
		(f & RC_SEND_RDY) ?"SEND_RDY":"");

	rcout(CD180_CAR, rc->rc_chan);

	printf("rc%d/%d: msvr %02x ier %02x ccsr %02x\n",
		rc->rc_rcb->rcb_unit, rc->rc_chan,
		rcin(CD180_MSVR),
		rcin(CD180_IER),
		rcin(CD180_CCSR));
}
#endif /* RCDEBUG */

static void
rc_dtrwakeup(chan)
	void	*chan;
{
	struct rc_chans  *rc;

	rc = (struct rc_chans *)chan;
	rc->rc_flags &= ~RC_DTR_OFF;
	wakeup(&rc->rc_dtrwait);
}

static void
rc_discard_output(rc)
	struct rc_chans  *rc;
{
	disable_intr();
	if (rc->rc_flags & RC_DOXXFER) {
		rc_scheduled_event -= LOTS_OF_EVENTS;
		rc->rc_flags &= ~RC_DOXXFER;
	}
	rc->rc_optr = rc->rc_obufend;
	rc->rc_tp->t_state &= ~TS_BUSY;
	enable_intr();
	ttwwakeup(rc->rc_tp);
}

static void
rc_wakeup(chan)
	void	*chan;
{
	timeout(rc_wakeup, (caddr_t)NULL, 1);

	if (rc_scheduled_event != 0) {
		int	s;

		s = splsofttty();
		rcpoll();
		splx(s);
	}
}

static void
disc_optim(tp, t, rc)
	struct tty	*tp;
	struct termios	*t;
	struct rc_chans	*rc;
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
	rc->rc_hotchar = linesw[tp->t_line].l_hotchar;
}

static void
rc_wait0(nec, unit, chan, line)
	int     nec, unit, chan, line;
{
	int rcnt;

	for (rcnt = 50; rcnt && rcin(CD180_CCR); rcnt--)
		DELAY(30);
	if (rcnt == 0)
		printf("rc%d/%d: channel command timeout, rc.c line: %d\n",
		      unit, chan, line);
}
