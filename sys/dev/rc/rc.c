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
 */

/*
 * SDL Communications Riscom/8 (based on Cirrus Logic CL-CD180) driver
 *
 */

#include "rc.h"
#if NRC > 0

/*#define RCDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/devconf.h>

#include <machine/clock.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/sioreg.h>

#include <i386/isa/ic/cd180.h>
#include <i386/isa/rcreg.h>

/* Prototypes */
int     rcprobe         __P((struct isa_device *));
int     rcattach        __P((struct isa_device *));

int     rcopen          __P((dev_t, int, int, struct proc *));
int     rcclose         __P((dev_t, int, int, struct proc *));
int     rcread          __P((dev_t, struct uio *, int));
int     rcwrite         __P((dev_t, struct uio *, int));
void    rcintr          __P((int));
void    rcpoll          __P((void));
void    rcstop          __P((struct tty *, int));
int     rcioctl         __P((dev_t, int, caddr_t, int, struct proc *));

#define rcin(port)      RC_IN  (nec, port)
#define rcout(port,v)   RC_OUT (nec, port, v)

/* Counter short for timeouts */
static volatile int rcnt;

#define WAITFORCCR { for (rcnt = 100000; rcin(CD180_CCR) && rcnt; rcnt--) ; }
#define CCRCMD(cmd) WAITFORCCR; rcout(CD180_CCR, cmd)

#define RC_IBUFSIZE     512
#define RC_OBUFSIZE     1024
#define RC_IHIGHWATER   (3 * RC_IBUFSIZE / 4)
#define INPUT_FLAGS_SHIFT (2 * RC_IBUFSIZE)
#define LOTS_OF_EVENTS  64

#define RC_TXTIMEO      30    /* 30 seconds wait if intr loss */
#define RC_FAKEID       0x10

#define GET_UNIT(dev)   (minor(dev) & 0x3F)
#define CALLOUT(dev)    (minor(dev) & 0x80)

/* For isa routines */
struct isa_driver rcdriver = {
	rcprobe, rcattach, "rc"
};

/* Per-board structure */
static struct rc_softc {
	u_int           rcb_probed;     /* 1 if device probed   */
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
	long             rc_txitime;            /* time of last TX intr */
	u_int            rc_dcdwaits;           /* how many waits DCD in open */
	u_char		 rc_hotchar;		/* end packed optimize */
	struct tty      *rc_tp;                 /* tty struct           */
	u_char          *rc_iptr;               /* Chars input buffer         */
	u_char          *rc_hiwat;              /* hi-water mark        */
	u_char          *rc_bufend;             /* end of buffer        */
	u_char          *rc_optr;               /* ptr in output buf    */
	u_char           rc_ocnt;
	u_char          *rc_obufend;            /* end of output buf    */
	u_char           rc_ibuf[4 * RC_IBUFSIZE];  /* input buffer         */
	u_char           rc_obuf[RC_OBUFSIZE];  /* output buffer        */
} rc_chans[NRC * CD180_NCHAN];

static int rc_scheduled_event = 0;

/* for pstat -t */
struct tty rc_tty[NRC * CD180_NCHAN];
int        nrc_tty = NRC * CD180_NCHAN;

/* Flags */
#define RC_DTR_OFF      000001          /* DTR wait, for close/open     */
#define RC_ACTOUT       000002          /* Dial-out port active         */
#define RC_RTSFLOW      000004          /* RTS flow ctl enabled         */
#define RC_CTSFLOW      000010          /* CTS flow ctl enabled         */
#define RC_DORXFER      000020          /* RXFER event planned          */
#define RC_DOXXFER      000040          /* RXFER event planned          */
#define RC_MODCHG       000100          /* Modem status changed         */
#define RC_OSUSP        000200          /* Output suspended             */
#define RC_OSBUSY       000400          /* start() routine in progress  */
#define RC_WAS_BUFOVFL  001000          /* low-level buffer ovferflow   */
#define RC_WAS_SILOVFL  002000          /* silo buffer overflow         */
#define RC_SEND_RDY     004000          /* ready to send */

static  struct speedtab rc_speedtab[] = {
	0,	0,
	50,     RC_BRD(50),
	75,     RC_BRD(75),
	110,    RC_BRD(110),
	134,    RC_BRD(134),
	150,    RC_BRD(150),
	200,    RC_BRD(200),
	300,    RC_BRD(300),
	600,    RC_BRD(600),
	1200,   RC_BRD(1200),
	1800,   RC_BRD(1800),
	2400,   RC_BRD(2400),
	4800,   RC_BRD(4800),
	9600,   RC_BRD(9600),
	19200,  RC_BRD(19200),
	38400,  RC_BRD(38400),
	57600,  RC_BRD(57600),
	/* real max value is 76800 with 9.8304 MHz clock */
	-1,	-1
};

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
static void rc_hwreset          __P((int, unsigned int));
static int  rc_test             __P((int, int));
static void rc_discard_output   __P((struct rc_chans *));
static void rc_hardclose        __P((struct rc_chans *));
static int  rc_modctl           __P((struct rc_chans *, int, int));
static void rc_start            __P((struct tty *));
static int  rc_param            __P((struct tty *, struct termios *));
static void rc_registerdev      __P((struct isa_device *id));
static timeout_t rc_dtrwakeup;
static timeout_t rc_wakeup;
static void disc_optim		__P((struct tty	*tp, struct termios *t,	struct rc_chans	*));

/**********************************************/

/* Quick device probing */
int rcprobe(dvp)
	struct  isa_device      *dvp;
{
	int             irq = ffs(dvp->id_irq) - 1;
	register int    nec = dvp->id_iobase;

	if (dvp->id_unit > NRC)
		return 0;
	rc_softc[dvp->id_unit].rcb_probed = 0;
	if (!RC_VALIDADDR(nec)) {
		printf("rc%d: illegal base address %x\n", nec);
		return 0;
	}
	if (!RC_VALIDIRQ(irq)) {
		printf("rc%d: illegal IRQ value %d\n", irq);
		return 0;
	}
	rcout(CD180_PPRL, 0x22); /* Random values to Prescale reg. */
	rcout(CD180_PPRH, 0x11);
	if (rcin(CD180_PPRL) != 0x22 || rcin(CD180_PPRH) != 0x11)
		return 0;
	/* Now, test the board more thoroughly, with diagnostic */
	if (rc_test(nec, dvp->id_unit))
		return 0;
	rc_softc[dvp->id_unit].rcb_probed = 1;
	return 1;
}

static struct kern_devconf kdc_rc[NRC] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"rc", 0, { MDDT_ISA, 0, "tty" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,        /* state */
	"RISCom/8 multiport card",
	DC_CLS_SERIAL		/* class */
} };

static void
rc_registerdev(id)
	struct isa_device *id;
{
	int	unit;

	unit = id->id_unit;
	if (unit != 0)
		kdc_rc[unit] = kdc_rc[0];
	kdc_rc[unit].kdc_unit = unit;
	kdc_rc[unit].kdc_isa = id;
	kdc_rc[unit].kdc_state = DC_UNKNOWN;
	dev_attach(&kdc_rc[unit]);
}

/* Test device, then attach */
int rcattach(dvp)
	struct  isa_device      *dvp;
{
	register int            i, chan, nec = dvp->id_iobase;
	struct rc_softc         *rcb = &rc_softc[dvp->id_unit];
	struct rc_chans         *rc  = &rc_chans[dvp->id_unit * CD180_NCHAN];
	static int              rc_wakeup_started = 0;

	/* Thorooughly test the device */
	if (!rcb->rcb_probed)
		return 0;
	rcb->rcb_addr   = nec;
	rcb->rcb_dtr    = 0;
	rcb->rcb_baserc = rc;
	/*rcb->rcb_chipid = 0x10 + dvp->id_unit;*/
	printf("rc%d: %d chans, firmware rev. %c\n", dvp->id_unit,
		CD180_NCHAN, (rcin(CD180_GFRCR) & 0xF) + 'A');

	rc_registerdev(dvp);

	for (chan = 0; chan < CD180_NCHAN; chan++, rc++) {
		rc->rc_rcb     = rcb;
		rc->rc_chan    = chan;
		rc->rc_iptr    = rc->rc_ibuf;
		rc->rc_bufend  = &rc->rc_ibuf[RC_IBUFSIZE];
		rc->rc_hiwat   = &rc->rc_ibuf[RC_IHIGHWATER];
		rc->rc_flags   = rc->rc_ier = rc->rc_msvr = 0;
		rc->rc_cor2    = rc->rc_pendcmd = 0;
		rc->rc_optr    = rc->rc_obufend  = rc->rc_obuf;
		rc->rc_txitime = (~0UL >> 1);
		rc->rc_dtrwait = 3 * hz;
		rc->rc_ocnt    = 0;
		rc->rc_dcdwaits= 0;
		rc->rc_hotchar = 0;
	}
	if (!rc_wakeup_started) {
		rc_wakeup((void *)NULL);
		rc_wakeup_started = 0;
	}
	return 1;
}

/* RC interrupt handling */
void    rcintr(unit)
	int             unit;
{
	register struct rc_softc        *rcb = &rc_softc[unit];
	register struct rc_chans        *rc;
	register u_char			val;
	register u_int                  nec, bsr, iack, ucnt;
	int                             good_data, resid;

	nec = rcb->rcb_addr;

possibly_more_intrs:
	bsr = ~(rcin(RC_BSR));

#ifdef RCDEBUG
	printf("rcintr: %d (%02x) %s %s %s %s\n", unit, bsr,
		(bsr & RC_BSR_TOUT)?"TOUT":"",
		(bsr & RC_BSR_RXINT)?"RXINT":"",
		(bsr & RC_BSR_TXINT)?"TXINT":"",
		(bsr & RC_BSR_MOINT)?"MOINT":"");
#endif
	if (bsr & RC_BSR_RXINT) {
		iack = rcin(RC_PILR_RX);
#ifdef RCDEBUG
		printf("rxint iack = %02x\n", iack);
#endif
		rc = rcb->rcb_baserc + (rcin(CD180_GICR) >> GICR_LSH);
		ucnt  = rcin(CD180_RDCR);
		resid = 0;
		good_data = (iack == (GIVR_IT_RGDI | RC_FAKEID));
#ifdef RCDEBUG
		printrcflags(rc, "rxint");
#endif
		/* Do RTS flow control stuff */
		if (  (rc->rc_flags & RC_RTSFLOW)
		    || !rc->rc_tp
		    || !(rc->rc_tp->t_state & TS_ISOPEN)) {
			if (  (!rc->rc_tp
			    || !(rc->rc_tp->t_state & TS_ISOPEN)
			    || (rc->rc_tp->t_state & TS_TBLOCK))
			    && (rc->rc_msvr & MSVR_RTS))
				rcout(CD180_MSVR,
					rc->rc_msvr &= ~MSVR_RTS);
			else if (!(rc->rc_msvr & MSVR_RTS))
				rcout(CD180_MSVR,
					rc->rc_msvr |= MSVR_RTS);
		}

		if (rc->rc_tp && (rc->rc_tp->t_state & TS_ISOPEN)) {
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
			/* check foor good data */
			if (good_data) {
				while (ucnt-- > 0) {
					val = rcin(CD180_RDR);
					rc->rc_iptr[0] = val;
					rc->rc_iptr[INPUT_FLAGS_SHIFT] = 0;
					rc->rc_iptr++;
					rc_scheduled_event++;
					if (rc->rc_hotchar != 0	&& val == rc->rc_hotchar)
						setsofttty();
				}
			} else {
				/* Store also status data */
				while (ucnt-- > 0) {
					iack = rcin(CD180_RCSR);
					if (iack & RCSR_TOUT) {
						(void) rcin(CD180_RDR);
						break;
					}
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
					if (   !(iack & (RCSR_PE|RCSR_FE|RCSR_BREAK))
					    || (!(iack & (RCSR_PE|RCSR_FE))
					    ||  !(rc->rc_tp->t_iflag & IGNPAR))
					    && (!(iack & RCSR_BREAK)
					    ||  !(rc->rc_tp->t_iflag & IGNBRK))) {
						if (   (iack & (RCSR_PE|RCSR_FE))
						    && (rc->rc_tp->t_state & TS_CAN_BYPASS_L_RINT)
						    && ((iack & RCSR_FE)
						    ||  (iack & RCSR_PE)
						    &&  (rc->rc_tp->t_iflag & INPCK)))
							val = 0;
						else if	(rc->rc_hotchar	!= 0 &&	val == rc->rc_hotchar)
							setsofttty();
						rc->rc_iptr[0] = val;
						rc->rc_iptr[INPUT_FLAGS_SHIFT] = iack;
						rc->rc_iptr++;
						rc_scheduled_event++;
					}
				}
			}
			rc->rc_flags |= RC_DORXFER;
		} else
			resid = ucnt;
		/* Clear FIFO if necessary */
		while (resid-- > 0) {
			if (!good_data)
				iack = rcin(CD180_RCSR);
			else
				iack = 0;
			(void) rcin(CD180_RDR);
			if (iack & RCSR_TOUT)
				break;
		}
		rcout(CD180_EOIR, 0);
		goto possibly_more_intrs;
	}
	if (bsr & RC_BSR_MOINT) {
		iack = rcin(CD180_MCR);
		rc = rcb->rcb_baserc + (rcin(CD180_GICR) >> GICR_LSH);
#ifdef RCDEBUG
		printrcflags(rc, "moint");
#endif
		rc->rc_msvr = rcin(CD180_MSVR);
		if (rc->rc_flags & RC_CTSFLOW) {
			if (rc->rc_msvr & MSVR_CTS)
				rc->rc_flags |= RC_SEND_RDY;
			else
				rc->rc_flags &= ~RC_SEND_RDY;
		}
		if (iack & MCR_CDCHG) {
			rc->rc_flags |= RC_MODCHG;
			rc_scheduled_event += LOTS_OF_EVENTS;
			setsofttty();
		}
		rcout(CD180_EOIR, 0);
		goto    possibly_more_intrs;
	}
	if (bsr & RC_BSR_TXINT) {
		rc = rcb->rcb_baserc + (rcin(CD180_GICR) >> GICR_LSH);
		rc->rc_txitime = time.tv_sec;
#ifdef RCDEBUG
		printrcflags(rc, "txint");
#endif
		if (    (rc->rc_flags & RC_OSUSP)
		    || !(rc->rc_flags & RC_SEND_RDY))
			goto skip;
		ucnt = rc->rc_obufend - rc->rc_optr;
		if (ucnt > CD180_NFIFO)
			ucnt = CD180_NFIFO;
		/* Handle breaks and other stuff */
		if (rc->rc_pendcmd) {
			rcout(CD180_COR2, rc->rc_cor2 |= COR2_ETC);
			rcout(CD180_TDR,  CD180_C_ESC);
			rcout(CD180_TDR,  rc->rc_pendcmd);
			rcout(CD180_COR2, rc->rc_cor2 &= ~COR2_ETC);
			rc->rc_pendcmd = 0;
			rcout(CD180_EOIR, 0);
			goto possibly_more_intrs;
		}
		while (ucnt-- > 0)
			rcout(CD180_TDR, *rc->rc_optr++);

		/* output completed? */
		if (rc->rc_optr >= rc->rc_obufend) {
			rcout(CD180_IER, rc->rc_ier &=
				~(IER_TXRDY|IER_TXMPTY));
#ifdef RCDEBUG
			printf("tx intr disabled\n");
#endif
			rc->rc_flags |= RC_DOXXFER;
			rc_scheduled_event += LOTS_OF_EVENTS;
			setsofttty();
		}
	skip:
		rcout(CD180_EOIR, 0);
		goto    possibly_more_intrs;
	}
	rcout(RC_BSR,     0);   /* -/-      */
#ifdef RCDEBUG
	if (rc_scheduled_event)
		printf("event scheduled unit %d\n", unit);
#endif
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
	if (rc->rc_flags & RC_RTSFLOW) {
		if ((tp->t_state & TS_TBLOCK) &&
		    (rc->rc_msvr & MSVR_RTS)) {
			rcout(CD180_CAR, rc->rc_chan);
			rcout(CD180_MSVR,
				rc->rc_msvr &= ~MSVR_RTS);
		} else if (!(rc->rc_msvr & MSVR_RTS)) {
			rcout(CD180_CAR, rc->rc_chan);
			rcout(CD180_MSVR,
				rc->rc_msvr |= MSVR_RTS);
		}
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;
#ifdef RCDEBUG
	printrcflags(rc, "rcstart");
#endif
	/* Checking for stale tx intrs */
	if ((rc->rc_ier & IER_TXRDY) &&
	    (rc->rc_txitime - time.tv_sec) > RC_TXTIMEO) {
		rc->rc_txitime = time.tv_sec;
		printf("rc%d: chan %d: lost TX intr, reinit\n",
			rc->rc_rcb->rcb_unit, rc->rc_chan);
		/* try to re-initialize channel */
		rcout(CD180_CAR, rc->rc_chan);
		CCRCMD(CCR_RESETCHAN);
		(void) rc_param(rc->rc_tp, &rc->rc_tp->t_termios);
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
#ifdef RCDEBUG
	printf("rcstart: q = %d olen = %d\n",
		tp->t_outq.c_cc, rc->rc_obufend - rc->rc_optr);
#endif
	/* maybe we need to check for lost intrs here */
	if (rc->rc_optr < rc->rc_obufend)
		goto    out;    /* output still in progress ... */

	if (tp->t_outq.c_cc > 0) {
		u_int   ocnt;

		tp->t_state |= TS_BUSY;
		ocnt = q_to_b(&tp->t_outq, rc->rc_obuf, sizeof rc->rc_obuf);
		disable_intr();
		rc->rc_ocnt = ocnt;
		rc->rc_optr = rc->rc_obuf;
		rc->rc_obufend = rc->rc_optr + rc->rc_ocnt;
		enable_intr();
		if ((rc->rc_ier & IER_TXRDY) == 0) {
#ifdef RCDEBUG
			printf("rcstart: enable txint\n");
#endif
			rcout(CD180_CAR, rc->rc_chan);
			rcout(CD180_IER, rc->rc_ier |= IER_TXRDY);
		}
	} else {
		rc->rc_ocnt = 0;
		tp->t_flags &= ~TS_BUSY;
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
	register int            s;
	register struct tty    *tp;
	register int            chan, icnt, c, nec, unit;

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
				rc->rc_flags &= ~RC_WAS_BUFOVFL;
				rc_scheduled_event--;
				printf("rc%d/%d: interrupt-level buffer overflow\n",
					unit, chan);
			}
			if (rc->rc_flags & RC_WAS_SILOVFL) {
				rc->rc_flags &= ~RC_WAS_SILOVFL;
				rc_scheduled_event--;
				printf("rc%d/%d: silo overflow\n",
					unit, chan);
			}
			if (rc->rc_flags & RC_MODCHG) {
				rc->rc_flags &= ~RC_MODCHG;
				rc_scheduled_event -= LOTS_OF_EVENTS;
				if (tp)
					(*linesw[tp->t_line].l_modem)(tp, !!(rc->rc_msvr & MSVR_CD));
			}
			if (rc->rc_flags & RC_DORXFER) {
				rc->rc_flags &= ~RC_DORXFER;

				disable_intr();
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
					if ((rc->rc_flags & RC_RTSFLOW)
					    && !(rc->rc_msvr & MSVR_RTS)
					    && tp != NULL
					    && (tp->t_state & TS_ISOPEN)
					    && !(tp->t_state & TS_TBLOCK)) {
						rcout(CD180_CAR, chan);
						rcout(CD180_MSVR,
							rc->rc_msvr |= MSVR_RTS);
					}
					rc_scheduled_event -= icnt;
				}
				enable_intr();

				if (icnt <= 0 || !tp || !(tp->t_state & TS_ISOPEN))
					goto done1;

				if (   linesw[tp->t_line].l_rint == ttyinput
				    && ((rc->rc_flags & RC_RTSFLOW) || (tp->t_iflag & IXOFF))
				    && !(tp->t_state & TS_TBLOCK)
				    && (tp->t_rawq.c_cc + icnt) > RC_IHIGHWATER) {
					int queue_full = 0;

					if ((tp->t_iflag & IXOFF) &&
					    tp->t_cc[VSTOP] != _POSIX_VDISABLE &&
					    (queue_full = putc(tp->t_cc[VSTOP], &tp->t_outq)) == 0 ||
					    (rc->rc_flags & RC_RTSFLOW)) {
						tp->t_state |= TS_TBLOCK;
						ttstart(tp);
						if (queue_full) /* try again */
							tp->t_state &= ~TS_TBLOCK;
					}
				}
				if (   (tp->t_state & TS_CAN_BYPASS_L_RINT)
				    && !(tp->t_state & TS_LOCAL)) {
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
						ttstart(tp);
					}
				} else {
					for (; tptr < eptr; tptr++)
						(*linesw[tp->t_line].l_rint)
						    (tptr[0] |
						    rc_rcsrt[tptr[INPUT_FLAGS_SHIFT] & 0xF], tp);
				}
done1:
			}
			if (rc->rc_flags & RC_DOXXFER) {
				rc_discard_output(rc);
				(*linesw[tp->t_line].l_start)(tp);
			}
		}
		if (rc_scheduled_event == 0)
			break;
	}
	if (rc_scheduled_event >= LOTS_OF_EVENTS)
		goto repeat;
}

void rcstop(tp, rw)
	register struct tty     *tp;
	int                     rw;
{
	register struct rc_chans        *rc = &rc_chans[GET_UNIT(tp->t_dev)];
	u_char *tptr, *eptr;

#ifdef RCDEBUG
	printf("rcstop %d/%d: %s%s\n", rc->rc_rcb->rcb_unit, rc->rc_chan,
		(rw & FWRITE)?"FWRITE ":"", (rw & FREAD)?"FREAD":"");
#endif
	if (rw & FWRITE)
		rc_discard_output(rc);
	disable_intr();
	if (rw & FREAD) {
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

int rcopen(dev, flag, mode, p)
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
	rc  = &rc_chans[unit];
	tp  = rc->rc_tp = &rc_tty[unit];
	nec = rc->rc_rcb->rcb_addr;
#ifdef RCDEBUG
	printf("rcopen: dev %02x\n", dev);
#endif
	s = spltty();

again:
	while (rc->rc_flags & RC_DTR_OFF) {
		error = tsleep(&rc->rc_dtrwait, TTIPRI | PCATCH, "rcdtr", 0);
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
				if (error = tsleep(&rc->rc_rcb,
				     TTIPRI|PCATCH, "rcbi", 0))
					goto out;
				goto again;
			}
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			error = EBUSY;
			goto out;
		}
	} else {
		tp->t_oproc   = rc_start;
		tp->t_param   = rc_param;
		tp->t_dev     = dev;

		if (tp->t_ispeed == 0) {
			ttychars(tp);
			tp->t_lflag = tp->t_iflag = tp->t_oflag = 0;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_ispeed = tp->t_ospeed = 9600;
		}
		if (CALLOUT(dev))
			tp->t_cflag |= CLOCAL;
		else
			tp->t_cflag &= ~CLOCAL;

		(void) rc_modctl(rc, TIOCM_DTR|TIOCM_RTS, DMSET);

		error = rc_param(tp, &tp->t_termios);
		if (error)
			goto out;

		ttsetwater(tp);

		disable_intr();
		rcout(CD180_CAR, rc->rc_chan);
		rc->rc_msvr = rcin(CD180_MSVR);
		rcout(CD180_IER, rc->rc_ier |= IER_CD | IER_TXRDY | IER_RXD);
		enable_intr();

		if ((rc->rc_msvr & MSVR_CD) || CALLOUT(dev))
			(*linesw[tp->t_line].l_modem)(tp, 1);
	}
	if (!(tp->t_state & TS_CARR_ON) && !CALLOUT(dev)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		rc->rc_dcdwaits++;
		error = tsleep(&tp->t_rawq, TTIPRI | PCATCH, "rcdcd", 0);
		rc->rc_dcdwaits--;
		if (error != 0)
			goto out;
		goto again;
	}
	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if ((tp->t_state & TS_ISOPEN) && CALLOUT(dev))
		rc->rc_flags |= RC_ACTOUT;
out:
	(void) splx(s);

	if(rc->rc_dcdwaits == 0 && !(tp->t_state & TS_ISOPEN))
		rc_hardclose(rc);

	return error;
}

int rcclose(dev, flag, mode, p)
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
	s = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	rcstop(tp, FREAD | FWRITE);
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

	/* Disable all intrs */
	rcout(CD180_IER, rc->rc_ier = 0);
	if (   tp->t_cflag & HUPCL
	    || !(rc->rc_flags & RC_ACTOUT)
	       && !(rc->rc_msvr & MSVR_CD)
	       && !(tp->t_cflag & CLOCAL)
	    || !(tp->t_state & TS_ISOPEN)) {
		(void) rc_modctl(rc, TIOCM_RTS, DMSET);
		if (rc->rc_dtrwait) {
			timeout(rc_dtrwakeup, rc, rc->rc_dtrwait);
			rc->rc_flags |= RC_DTR_OFF;
		}
	}
	rc->rc_flags &= ~RC_ACTOUT;
	wakeup((caddr_t) &rc->rc_rcb);  /* wake bi */
	wakeup((caddr_t) &tp->t_rawq);  /* wake dcd */
	(void) splx(s);
}

/* Read from line */
int rcread(dev, uio, flag)
	dev_t           dev;
	struct uio      *uio;
	int             flag;
{
	struct tty *tp = rc_chans[GET_UNIT(dev)].rc_tp;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

/* Write to line */
int rcwrite(dev, uio, flag)
	dev_t           dev;
	struct uio      *uio;
	int             flag;
{
	struct tty *tp = rc_chans[GET_UNIT(dev)].rc_tp;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/* Reset the bastard */
static void rc_hwreset(nec, chipid)
	register int    nec;
	unsigned int    chipid;
{
	CCRCMD(CCR_HWRESET);            /* Hardware reset */
	DELAY(20000);
	rcout(RC_BSR_TOUT, 0);          /* Clear timeout  */
	rcout(CD180_GIVR,  chipid);
	rcout(CD180_GICR,  0);

	/* Set Prescaler Registers (1 msec) */
	rcout(CD180_PPRL, (RC_OSCFREQ / 1000) & 0xFF);
	rcout(CD180_PPRH, (RC_OSCFREQ / 1000) >> 8);

	/* Initialize Priority Interrupt Level Registers */
	rcout(CD180_PILR1, RC_PILR_MODEM);
	rcout(CD180_PILR2, RC_PILR_TX);
	rcout(CD180_PILR3, RC_PILR_RX);

	/* Reset DTR */
	rcout(RC_DTR, ~0);
}

/* Set channel parameters */
static int rc_param(tp, ts)
	register struct  tty    *tp;
	struct termios          *ts;
{
	register struct rc_chans        *rc = &rc_chans[GET_UNIT(tp->t_dev)];
	register int    nec = rc->rc_rcb->rcb_addr;
	int      idivs, odivs, s, val, cflag, iflag, lflag;

	odivs = ttspeedtab(ts->c_ospeed, rc_speedtab);
	if (ts->c_ispeed == 0)
		ts->c_ispeed = ts->c_ospeed;
	idivs = ttspeedtab(ts->c_ispeed, rc_speedtab);
	if (idivs < 0 || odivs < 0)
		return (EINVAL);

	s = spltty();

	/* If speed == 0, hangup line */
	if (ts->c_ospeed == 0)
		rc_modctl(rc, TIOCM_DTR, DMBIC);
	else
		rc_modctl(rc, TIOCM_RTS|TIOCM_DTR, DMBIS);

	tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	cflag = ts->c_cflag;
	iflag = ts->c_iflag;
	lflag = ts->c_lflag;

	/* Select channel */
	rcout(CD180_CAR, rc->rc_chan);

	if (idivs > 0) {
		rcout(CD180_RBPRL, idivs & 0xFF);
		rcout(CD180_RBPRH, idivs >> 8);
	}
	if (odivs > 0) {
		rcout(CD180_TBPRL, odivs & 0xFF);
		rcout(CD180_TBPRH, odivs >> 8);
	}

	/* set timeout value */
	rcout(CD180_RTPR,  0);

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
	} else
		val |= COR1_IGNORE;
	if (cflag & CSTOPB)
		val |= COR1_2SB;
	rcout(CD180_COR1, val);

	/* Set FIFO threshold */
	rcout(CD180_COR3, ts->c_ospeed <= 4800 ? 1 :  CD180_NFIFO / 2);
	CCRCMD(CCR_CORCHG1 | CCR_CORCHG3);

	/* Initialize on-chip automatic flow control */
	val = 0;

	if (cflag & CCTS_OFLOW) {
		rc->rc_flags |= RC_CTSFLOW;
		val |= COR2_CTSAE;
		rc->rc_msvr = rcin(CD180_MSVR);
		if (rc->rc_msvr & MSVR_CTS)
			rc->rc_flags |= RC_SEND_RDY;
		else
			rc->rc_flags &= ~RC_SEND_RDY;
	}
	else
		rc->rc_flags |= RC_SEND_RDY;

	if (cflag & CRTS_IFLOW)
		rc->rc_flags |= RC_RTSFLOW;

	if (iflag & (IXON|IXOFF)) {
		/* Initailize xon/xoff characters */
		rcout(CD180_SCHR1, ts->c_cc[CSTART]);
		rcout(CD180_SCHR2, ts->c_cc[CSTOP]);
		if (iflag & IXON) {
			val |= COR2_TXIBE;
			if (iflag & IXANY)
				val |= COR2_IXM;
		}
	}

	rcout(CD180_COR2, val);
	CCRCMD(CCR_CORCHG2);

	disc_optim(tp, ts, rc);

	/* modem ctl */
	rcout(CD180_MCOR1, MCOR1_CDZD);
	rcout(CD180_MCOR2, MCOR2_CDOD);

	/* enable i/o and interrupts */
	CCRCMD(CCR_TXEN|CCR_RXEN);
	rcout(CD180_IER, rc->rc_ier |= IER_CD | IER_RXD);

	(void) splx(s);
	return 0;
}

int rcioctl(dev, cmd, data, flag, p)
dev_t           dev;
int             cmd, flag;
caddr_t         data;
struct proc     *p;
{
	register struct rc_chans       *rc = &rc_chans[GET_UNIT(dev)];
	register int                    s, error;
	struct tty                     *tp = rc->rc_tp;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
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
		(void) rc_modctl(rc, TIOCM_RTS|TIOCM_DTR, DMBIS);
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
		error = suser(p->p_ucred, &p->p_acflag);
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
	u_char         *dtr = &rc->rc_rcb->rcb_dtr;
	unsigned int    msvr;

	rcout(CD180_CAR, rc->rc_chan);

	switch (cmd) {
	    case DMSET:
		rcout(CD180_MSVR, 0);
		*dtr &= ~(1 << rc->rc_chan);
		/* falltrough */

	    case DMBIS:
		if (bits & TIOCM_RTS)
			rcout(CD180_MSVR, MSVR_RTS);
		if (bits & TIOCM_DTR)
			rcout(RC_DTR, ~(*dtr |= (1 << rc->rc_chan)));
		break;

	    case DMGET:
		msvr = rcin(CD180_MSVR);
		bits = TIOCM_LE;

		if (msvr & MSVR_RTS)
			bits |= TIOCM_RTS;
		if (msvr & MSVR_CTS)
			bits |= TIOCM_CTS;
		if (msvr & MSVR_DSR)
			bits |= TIOCM_DSR;
		if (msvr & MSVR_DTR)
			bits |= TIOCM_DTR;
		return bits;

	    case DMBIC:
		if (bits & TIOCM_DTR)
			rcout(RC_DTR, ~(*dtr &= ~(1 << rc->rc_chan)));
		if (bits & TIOCM_RTS)
			rcout(CD180_MSVR, 0);
		break;
	}
	return 0;
}

/* Test the board. */
int rc_test(nec, unit)
	register int    nec;
	int             unit;
{
	int     chan = 0, nopt = 0;
	int     i = 0, rcnt, old_level;
	unsigned int    iack, chipid;
	unsigned short  divs;
	static  u_char  ctest[] = "\377\125\252\045\244\0\377";
#define CTLEN   8
#define ERR(s)  { \
		printf("rc%d: ", unit); printf s ; printf("\n"); \
		(void) splx(old_level); return 1; }
#define TWAITFORCCR \
		for (rcnt = 100000; rcin(CD180_CCR) && rcnt; rcnt--) ; \
		if (!rcnt) ERR(("Timeout waiting for zero CCR"))

	struct rtest {
		u_char  txbuf[CD180_NFIFO];     /* TX buffer  */
		u_char  rxbuf[CD180_NFIFO];     /* RX buffer  */
		int     rxptr;                  /* RX pointer */
		int     txptr;                  /* TX pointer */
	} tchans[CD180_NCHAN];

	old_level = splhigh();

	chipid = RC_FAKEID;

	/* First, reset board to inital state */
	rc_hwreset(nec, chipid);

	/* Initialize channels */
	for (chan = 0; chan < CD180_NCHAN; chan++) {

		divs = RC_BRD(19200);

		TWAITFORCCR;

		/* Select and reset channel */
		rcout(CD180_CAR, chan);
		rcout(CD180_CCR, CCR_RESETCHAN);
		TWAITFORCCR;

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
		TWAITFORCCR;
		rcout(CD180_CCR,  CCR_CORCHG1 | CCR_CORCHG2 | CCR_CORCHG3);
		TWAITFORCCR;
		rcout(CD180_CCR,  CCR_RXEN | CCR_TXEN);
		rcout(CD180_MSVR, MSVR_RTS);

		/* Fill TXBUF with test data */
		for (i = 0; i < CD180_NFIFO; i++) {
			tchans[chan].txbuf[i] = ctest[i];
			tchans[chan].rxbuf[i] = 0;
		}
		tchans[chan].txptr = tchans[chan].rxptr = 0;

		/* Now, start transmit */
		rcout(CD180_IER, IER_TXMPTY | IER_RXD);
	}
	/* Pseudo-interrupt poll stuff */
	for (rcnt = 10000; rcnt-- > 0; rcnt--) {
		i = ~(rcin(RC_BSR)) & 0xF;
		if (i & RC_BSR_TOUT)
			ERR(("BSR timeout bit set\n"))
		if (i & RC_BSR_TXINT) {
			iack = rcin(RC_PILR_TX);
			if (iack != (GIVR_IT_TDI | chipid))
				ERR(("Bad TX intr ack (%02x != %02x)\n",
					iack, GIVR_IT_TDI | chipid));
			chan = (rcin(CD180_GICR) >> 2) & 07;
			/* If no more data to transmit, disable TX intr */
			if (tchans[chan].txptr >= CD180_NFIFO) {
				iack = rcin(CD180_IER);
				rcout(CD180_IER, iack & ~IER_TXMPTY);
			} else {
				for (iack = tchans[chan].txptr;
				    iack < CD180_NFIFO; iack++)
					rcout(CD180_TDR,
					    tchans[chan].txbuf[iack]);
				tchans[chan].txptr = iack;
				rcout(CD180_EOIR, 0);
			}

		}
		if (i & RC_BSR_RXINT) {
			unsigned int ucnt;

			iack = rcin(RC_PILR_RX);
			if (iack != (GIVR_IT_RGDI | chipid) &&
			    iack != (GIVR_IT_REI  | chipid))
				ERR(("Bad RX intr ack (%02x != %02x)\n",
					iack, GIVR_IT_RGDI | chipid))
			chan = (rcin(CD180_GICR) >> 2) & 07;
			ucnt = rcin(CD180_RDCR) & 0xF;
			while (ucnt-- > 0) {
				iack = rcin(CD180_RCSR);
				if (iack & RCSR_TOUT)
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
		rcout(RC_BSR, 0);
		for (iack = chan = 0; chan < CD180_NCHAN; chan++)
			if (tchans[chan].rxptr >= CD180_NFIFO)
				iack++;
		if (iack == CD180_NCHAN)
			break;
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

int printrcflags(rc, comment)
struct rc_chans  *rc;
char             *comment;
{
	u_short f = rc->rc_flags;

	printf("rc %d/%d %s flags: %s%s%s%s%s%s%s%s%s\n",
		rc->rc_rcb->rcb_unit, rc->rc_chan, comment,
		(f & RC_DTR_OFF)?"DTR_OFF " :"",
		(f & RC_ACTOUT) ?"ACTOUT ":"",
		(f & RC_RTSFLOW)?"RTSFL " :"",
		(f & RC_CTSFLOW)?"CTSFL " :"",
		(f & RC_DORXFER)?"DORXF " :"",
		(f & RC_DOXXFER)?"DOXXF " :"",
		(f & RC_MODCHG) ?"MODC "  :"",
		(f & RC_OSUSP)  ?"OSUSP " :"");
	return 0;
}
#endif /* RCDEBUG */

struct tty *
rcdevtotty(dev)
	dev_t	dev;
{
	int	unit;

	unit = GET_UNIT(dev);
	if (unit >= NRC * CD180_NCHAN)
		return NULL;
	return (&rc_tty[unit]);
}

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
	enable_intr();
	rc->rc_tp->t_state &= ~TS_BUSY;
}

static void
rc_wakeup(chan)
	void	*chan;
{
	int		unit;

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

	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP
			   | IXOFF | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK) ||
		(t->c_iflag & (IGNPAR|IGNBRK)) == (IGNPAR|IGNBRK))
	    && !(t->c_lflag & (ECHO | ECHONL | ICANON | IEXTEN | ISIG
			   | PENDIN))
	    && linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	if (tp->t_line == SLIPDISC)
		rc->rc_hotchar = 0xc0;
	else if (tp->t_line == PPPDISC)
		rc->rc_hotchar = 0x7e;
	else
		rc->rc_hotchar = 0;
}
#endif /* NRC */
