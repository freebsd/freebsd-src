/*
 * Copyright (c) 1995 HD Associates, Inc.
 * All rights reserved.
 *
 * HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463-0276
 * dufault@hda.com
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
 *	This product includes software developed by HD Associates, Inc.
 * 4. The name of HD Associates, Inc.
 *    may not be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES ``AS IS'' AND
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
 * Written by:
 * Peter Dufault
 * dufault@hda.com
 */

#include "labpc.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <sys/systm.h>
#include <sys/devconf.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#define b_actf	b_act.tqe_next
#include <sys/dataacq.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/devconf.h>
#include <machine/clock.h>

#include <i386/isa/isa_device.h>



/* Miniumum timeout:
 */
#ifndef LABPC_MIN_TMO
#define LABPC_MIN_TMO (hz)
#endif

#ifndef LABPC_DEFAULT_HERZ
#define LABPC_DEFAULT_HERZ 500
#endif

/* Minor number:
 * UUSIDCCC
 * UU: Board unit.
 * S: SCAN bit for scan enable.
 * I: INTERVAL for interval support
 * D: 1: Digital I/O, 0: Analog I/O
 * CCC: Channel.
 *  Analog (D==0):
 *  input: channel must be 0 to 7.
 *  output: channel must be 0 to 2
 *          0: D-A 0
 *          1: D-A 1
 *          2: Alternate channel 0 then 1
 *
 *  Digital (D==1):
 *  input: Channel must be 0 to 2.
 *  output: Channel must be 0 to 2.
 */

/* Up to four boards:
 */
#define MAX_UNITS 4
#define UNIT(dev) (((minor(dev) & 0xB0) >> 6) & 0x3)

#define SCAN(dev)     ((minor(dev) & 0x20) >> 5)
#define INTERVAL(dev) ((minor(dev) & 0x10) >> 4)
#define DIGITAL(dev)  ((minor(dev) & 0x08) >> 3)

/* Eight channels:
 */

#define CHAN(dev) (minor(dev) & 0x7)

/* History: Derived from "dt2811.c" March 1995
 */

struct ctlr
{
	int err;
#define DROPPED_INPUT 0x100
	int base;
	int unit;
	unsigned long flags;
#define BUSY 0x00000001

	u_char cr_image[4];

	u_short sample_us;

	struct buf start_queue;	/* Start queue */
	struct buf *last;	/* End of start queue */
	u_char *data;
	u_char *data_end;
	long tmo;			/* Timeout in Herz */
	long min_tmo;		/* Timeout in Herz */
	int cleared_intr;

	int gains[8];

	dev_t dev;			/* Copy of device */

	void (*starter)(struct ctlr *ctlr, long count);
	void (*stop)(struct ctlr *ctlr);
	void (*intr)(struct ctlr *ctlr);

	/* Digital I/O support.  Copy of Data Control Register for 8255:
	 */
	u_char dcr_val, dcr_is;

	/* Device configuration structure:
	 */
	struct kern_devconf kdc;
#ifdef DEVFS
	void *devfs_token;
#endif
};

#ifdef LOUTB
/* loutb is a slow outb for debugging.  The overrun test may fail
 * with this for some slower processors.
 */
static inline void loutb(int port, u_char val)
{
	outb(port, val);
	DELAY(1);
}
#else
#define loutb(port, val) outb(port, val)
#endif

static struct ctlr **labpcs;	/* XXX: Should be dynamic */

/* CR_EXPR: A macro that sets the shadow register in addition to
 * sending out the data.
 */
#define CR_EXPR(LABPC, CR, EXPR) do { \
	(LABPC)->cr_image[CR - 1] EXPR ; \
	loutb(((LABPC)->base + ( (CR == 4) ? (0x0F) : (CR - 1))), ((LABPC)->cr_image[(CR - 1)])); \
} while (0)

#define CR_CLR(LABPC, CR) CR_EXPR(LABPC, CR, &=0)
#define CR_REFRESH(LABPC, CR) CR_EXPR(LABPC, CR, &=0xff)
#define CR_SET(LABPC, CR, EXPR) CR_EXPR(LABPC, CR, = EXPR)

/* Configuration and Status Register Group.
 */
#define     CR1(LABPC) ((LABPC)->base + 0x00)	/* Page 4-5 */
	#define SCANEN    0x80
	#define GAINMASK  0x70
	#define GAIN(LABPC, SEL) do { \
		(LABPC)->cr_image[1 - 1] &= ~GAINMASK; \
		(LABPC)->cr_image[1 - 1] |= (SEL << 4); \
		loutb((LABPC)->base + (1 - 1), (LABPC)->cr_image[(1 - 1)]); \
		} while (0)

	#define TWOSCMP   0x08
	#define MAMASK    0x07
	#define MA(LABPC, SEL) do { \
		(LABPC)->cr_image[1 - 1] &= ~MAMASK; \
		(LABPC)->cr_image[1 - 1] |= SEL; \
		loutb((LABPC)->base + (1 - 1), (LABPC)->cr_image[(1 - 1)]); \
		} while (0)

#define  STATUS(LABPC) ((LABPC)->base + 0x00)	/* Page 4-7 */
	#define LABPCPLUS 0x80
	#define EXTGATA0  0x40
	#define GATA0     0x20
	#define DMATC     0x10
	#define CNTINT    0x08
	#define OVERFLOW  0x04
	#define OVERRUN   0x02
	#define DAVAIL    0x01

#define     CR2(LABPC) ((LABPC)->base + 0x01)	/* Page 4-9 */
	#define LDAC1     0x80
	#define LDAC0     0x40
	#define _2SDAC1   0x20
	#define _2SDAC0   0x10
	#define TBSEL     0x08
	#define SWTRIG    0x04
	#define HWTRIG    0x02
	#define PRETRIG   0x01
	#define SWTRIGGERRED(LABPC) ((LABPC->cr_image[1]) & SWTRIG)

#define     CR3(LABPC) ((LABPC)->base + 0x02)	/* Page 4-11 */
	#define FIFOINTEN 0x20
	#define ERRINTEN  0x10
	#define CNTINTEN  0x08
	#define TCINTEN   0x04
	#define DIOINTEN  0x02
	#define DMAEN     0x01

	#define ALLINTEN  0x3E
	#define FIFOINTENABLED(LABPC) ((LABPC->cr_image[2]) & FIFOINTEN)

#define     CR4(LABPC) ((LABPC)->base + 0x0F)	/* Page 4-13 */
	#define ECLKRCV   0x10
	#define SE_D      0x08
	#define ECKDRV    0x04
	#define EOIRCV    0x02
	#define INTSCAN   0x01

/* Analog Input Register Group
 */
#define   ADFIFO(LABPC) ((LABPC)->base + 0x0A)	/* Page 4-16 */
#define  ADCLEAR(LABPC) ((LABPC)->base + 0x08)	/* Page 4-18 */
#define  ADSTART(LABPC) ((LABPC)->base + 0x03)	/* Page 4-19 */
#define DMATCICLR(LABPC) ((LABPC)->base + 0x0A)	/* Page 4-20 */

/* Analog Output Register Group
 */
#define    DAC0L(LABPC) ((LABPC)->base + 0x04)	/* Page 4-22 */
#define    DAC0H(LABPC) ((LABPC)->base + 0x05)	/* Page 4-22 */
#define    DAC1L(LABPC) ((LABPC)->base + 0x06)	/* Page 4-22 */
#define    DAC1H(LABPC) ((LABPC)->base + 0x07)	/* Page 4-22 */

/* 8253 registers:
 */
#define A0DATA(LABPC) ((LABPC)->base + 0x14)
#define A1DATA(LABPC) ((LABPC)->base + 0x15)
#define A2DATA(LABPC) ((LABPC)->base + 0x16)
#define AMODE(LABPC) ((LABPC)->base + 0x17)

#define TICR(LABPC) ((LABPC)->base + 0x0c)

#define B0DATA(LABPC) ((LABPC)->base + 0x18)
#define B1DATA(LABPC) ((LABPC)->base + 0x19)
#define B2DATA(LABPC) ((LABPC)->base + 0x1A)
#define BMODE(LABPC) ((LABPC)->base + 0x1B)

/* 8255 registers:
 */

#define PORTX(LABPC, X) ((LABPC)->base + 0x10 + X)

#define PORTA(LABPC) PORTX(LABPC, 0)
#define PORTB(LABPC) PORTX(LABPC, 1)
#define PORTC(LABPC) PORTX(LABPC, 2)

#define DCR(LABPC) ((LABPC)->base + 0x13)

static int labpcattach(struct isa_device *dev);
static int labpcprobe(struct isa_device *dev);
struct isa_driver labpcdriver =
	{ labpcprobe, labpcattach, "labpc", 0  };

static	d_open_t	labpcopen;
static	d_close_t	labpcclose;
static	d_ioctl_t	labpcioctl;
static	d_strategy_t	labpcstrategy;

#define CDEV_MAJOR 66
static struct cdevsw labpc_cdevsw = 
	{ labpcopen,	labpcclose,	rawread,	rawwrite,	/*66*/
	  labpcioctl,	nostop,		nullreset,	nodevtotty,/* labpc */
	  seltrue,	nommap,		labpcstrategy, "labpc",	NULL,	-1 };

static void start(struct ctlr *ctlr);

static void
bp_done(struct buf *bp, int err)
{
	bp->b_error = err;

	if (err || bp->b_resid)
	{
		bp->b_flags |= B_ERROR;
	}

	biodone(bp);
}

static void tmo_stop(void *p);

static void
done_and_start_next(struct ctlr *ctlr, struct buf *bp, int err)
{
	bp->b_resid = ctlr->data_end - ctlr->data;

	ctlr->data = 0;

	ctlr->start_queue.b_actf = bp->b_actf;
	bp_done(bp, err);

	untimeout(tmo_stop, ctlr);

	start(ctlr);
}

static inline void
ad_clear(struct ctlr *ctlr)
{
	int i;
	loutb(ADCLEAR(ctlr), 0);
	for (i = 0; i < 10000 && (inb(STATUS(ctlr)) & GATA0); i++)
		;
	(void)inb(ADFIFO(ctlr));
	(void)inb(ADFIFO(ctlr));
}

/* reset: Reset the board following the sequence on page 5-1
 */
static inline void
reset(struct ctlr *ctlr)
{
	int s = splhigh();

	CR_CLR(ctlr, 3);	/* Turn off interrupts first */
	splx(s);

	CR_CLR(ctlr, 1);
	CR_CLR(ctlr, 2);
	CR_CLR(ctlr, 4);

	loutb(AMODE(ctlr), 0x34);
	loutb(A0DATA(ctlr),0x0A);
	loutb(A0DATA(ctlr),0x00);

	loutb(DMATCICLR(ctlr), 0x00);
	loutb(TICR(ctlr), 0x00);

	ad_clear(ctlr);

	loutb(DAC0L(ctlr), 0);
	loutb(DAC0H(ctlr), 0);
	loutb(DAC1L(ctlr), 0);
	loutb(DAC1H(ctlr), 0);

	ad_clear(ctlr);
}

static int
labpc_goaway(struct kern_devconf *kdc, int force)
{
	if(force) {
		dev_detach(kdc);
		return 0;
	} else {
		return EBUSY;   /* XXX fix */
	}
}

static struct kern_devconf kdc_template = {
      0, 0, 0,                /* filled in by dev_attach */
      "labpc", 0, { MDDT_ISA, 0, "tty" },
      isa_generic_externalize, 0, labpc_goaway, ISA_EXTERNALLEN,
      &kdc_isa0,              /* parent */
      0,                      /* parentdata */
      DC_UNKNOWN,
      "?"                     /* Description (filled in later ) */
};

static inline void
labpc_registerdev(struct isa_device *id)
{
	struct kern_devconf *kdc = &labpcs[id->id_unit]->kdc;
	kdc->kdc_unit = id->id_unit;
	kdc->kdc_parentdata = id;
	dev_attach(kdc);
}

/* overrun: slam the start convert register and OVERRUN should get set:
 */
static u_char
overrun(struct ctlr *ctlr)
{
	int i;

	u_char status = inb(STATUS(ctlr));
	for (i = 0; ((status & OVERRUN) == 0) && i < 100; i++)
	{
		loutb(ADSTART(ctlr), 1);
		status = inb(STATUS(ctlr));
	}

	return status;
}

static int
labpcinit(void)
{
	if (NLABPC > MAX_UNITS)
		return 0;

	labpcs = malloc(NLABPC * sizeof(struct ctlr *), M_DEVBUF, M_NOWAIT);
	if (labpcs)
	{
		bzero(labpcs, NLABPC * sizeof(struct cltr *));
		return 1;
	}
	return 0;
}

static int
labpcprobe(struct isa_device *dev)
{
	static unit;
	struct ctlr scratch, *ctlr;
	u_char status;

	if (!labpcs)
	{
		if (labpcinit() == 0)
		{
			printf("labpcprobe: init failed\n");
			return 0;
		}
	}

	if (unit > NLABPC)
	{
		printf("Too many LAB-PCs.  Reconfigure O/S.\n");
		return 0;
	}
	ctlr = &scratch;	/* Need somebody with the right base for the macros */
	ctlr->base = dev->id_iobase;

	/* XXX: There really isn't a perfect way to probe this board.
	 *      Here is my best attempt:
	 */
	reset(ctlr);

	/* After reset none of these bits should be set:
	 */
	status = inb(STATUS(ctlr));
	if (status & (GATA0 | OVERFLOW | DAVAIL | OVERRUN))
		return 0;

	/* Now try to overrun the board FIFO and get the overrun bit set:
	 */
	status = overrun(ctlr);

	if ((status & OVERRUN) == 0)	/* No overrun bit set? */
		return 0;

	/* Assume we have a board.
	 */
	reset(ctlr);

	if ( (labpcs[unit] = malloc(sizeof(struct ctlr), M_DEVBUF, M_NOWAIT)) )
	{
		struct ctlr *l = labpcs[unit];

		bzero(l, sizeof(struct ctlr));
		l->base = ctlr->base;
		dev->id_unit = l->unit = unit;

		l->kdc = kdc_template;
		l->kdc.kdc_state = DC_IDLE;

		if ((status & LABPCPLUS) == 0)
			l->kdc.kdc_description = "National Instrument's LabPC+";
		else
			l->kdc.kdc_description = "National Instrument's LabPC";

		unit++;
		return 0x20;
	}
	else
	{
		printf("labpc%d: Can't malloc.\n", unit);
		return 0;
	}
}

/* attach: Set things in a normal state.
 */
static int
labpcattach(struct isa_device *dev)
{
	struct ctlr *ctlr = labpcs[dev->id_unit];

	ctlr->sample_us = (1000000.0 / (double)LABPC_DEFAULT_HERZ) + .50;
	reset(ctlr);
    labpc_registerdev(dev);

	ctlr->min_tmo = LABPC_MIN_TMO;

	ctlr->dcr_val = 0x80;
	ctlr->dcr_is = 0x80;
	loutb(DCR(ctlr), ctlr->dcr_val);

#ifdef DEVFS
	ctlr->devfs_token = 
		devfs_add_devswf(&labpc_cdevsw, 0, DV_CHR, 
                                 /* what  UID GID PERM */
				 0, 0, 0600, 
				 "labpc%d", dev->id_unit);
#endif
	return 1;
}

/* Null handlers:
 */
static void null_intr (struct ctlr *ctlr)             { }
static void null_start(struct ctlr *ctlr, long count) { }
static void null_stop (struct ctlr *ctlr)             { }

static inline void
trigger(struct ctlr *ctlr)
{
	CR_EXPR(ctlr, 2, |= SWTRIG);
}

static void
ad_start(struct ctlr *ctlr, long count)
{
	if (!SWTRIGGERRED(ctlr)) {
		int chan = CHAN(ctlr->dev);
		CR_EXPR(ctlr, 1, &= ~SCANEN);
		CR_EXPR(ctlr, 2, &= ~TBSEL);

		MA(ctlr, chan);
		GAIN(ctlr, ctlr->gains[chan]);

		if (SCAN(ctlr->dev))
			CR_EXPR(ctlr, 1, |= SCANEN);

		loutb(AMODE(ctlr), 0x34);
		loutb(A0DATA(ctlr), (u_char)((ctlr->sample_us & 0xff)));
		loutb(A0DATA(ctlr), (u_char)((ctlr->sample_us >> 8)&0xff));
		loutb(AMODE(ctlr), 0x70);

		ad_clear(ctlr);
		trigger(ctlr);
	}

	ctlr->tmo = ((count + 16) * (long)ctlr->sample_us * hz) / 1000000 +
		ctlr->min_tmo;
}

static void
ad_interval_start(struct ctlr *ctlr, long count)
{
	int chan = CHAN(ctlr->dev);
	int n_frames = count / (chan + 1);

	if (!SWTRIGGERRED(ctlr)) {
		CR_EXPR(ctlr, 1, &= ~SCANEN);
		CR_EXPR(ctlr, 2, &= ~TBSEL);

		MA(ctlr, chan);
		GAIN(ctlr, ctlr->gains[chan]);

		/* XXX: Is it really possible that you clear INTSCAN as
		 * the documentation says?  That seems pretty unlikely.
		 */
		CR_EXPR(ctlr, 4, &= ~INTSCAN);	/* XXX: Is this possible? */

		/* Program the sample interval counter to run as fast as
		 * possible.
		 */
		loutb(AMODE(ctlr), 0x34);
		loutb(A0DATA(ctlr), (u_char)(0x02));
		loutb(A0DATA(ctlr), (u_char)(0x00));
		loutb(AMODE(ctlr), 0x70);

		/* Program the interval scanning counter to run at the sample
		 * frequency.
		 */
		loutb(BMODE(ctlr), 0x74);
		loutb(B1DATA(ctlr), (u_char)((ctlr->sample_us & 0xff)));
		loutb(B1DATA(ctlr), (u_char)((ctlr->sample_us >> 8)&0xff));
		CR_EXPR(ctlr, 1, |= SCANEN);

		ad_clear(ctlr);
		trigger(ctlr);
	}

	/* Each frame time takes two microseconds per channel times
	 * the number of channels being sampled plus the sample period.
	 */
	ctlr->tmo = ((n_frames + 16) *
	((long)ctlr->sample_us + (chan + 1 ) * 2 ) * hz) / 1000000 +
		ctlr->min_tmo;
}

static void
all_stop(struct ctlr *ctlr)
{
	reset(ctlr);
}

static void
tmo_stop(void *p)
{
	struct ctlr *ctlr = (struct ctlr *)p;
	struct buf *bp;

	int s = spltty();

	if (ctlr == 0)
	{
		printf("labpc?: Null ctlr struct?\n");
		splx(s);
		return;
	}

	printf("labpc%d: timeout", ctlr->unit);

	(*ctlr->stop)(ctlr);

	bp = ctlr->start_queue.b_actf;

	if (bp == 0) {
		printf(", Null bp.\n");
		splx(s);
		return;
	}

	printf("\n");

	done_and_start_next(ctlr, bp, ETIMEDOUT);

	splx(s);
}

static void ad_intr(struct ctlr *ctlr)
{
	u_char status;

	if (ctlr->cr_image[2] == 0)
	{
		if (ctlr->cleared_intr)
		{
			ctlr->cleared_intr = 0;
			return;
		}

		printf("ad_intr (should not happen) interrupt with interrupts off\n");
		printf("status %x, cr3 %x\n", inb(STATUS(ctlr)), ctlr->cr_image[2]);
		return;
	}

	while ( (status = (inb(STATUS(ctlr)) & (DAVAIL|OVERRUN|OVERFLOW)) ) )
	{
		if ((status & (OVERRUN|OVERFLOW)))
		{
			struct buf *bp = ctlr->start_queue.b_actf;

			printf("ad_intr: error: bp %0p, data %0p, status %x",
			bp, ctlr->data, status);

			if (status & OVERRUN)
				printf(" Conversion overrun (multiple A-D trigger)");

			if (status & OVERFLOW)
				printf(" FIFO overflow");

			printf("\n");

			if (bp)
			{
				done_and_start_next(ctlr, bp, EIO);
				return;
			}
			else
			{
				printf("ad_intr: (should not happen) error between records\n");
				ctlr->err = status;	/* Set overrun condition */
				return;
			}
		}
		else	/* FIFO interrupt */
		{
			struct buf *bp = ctlr->start_queue.b_actf;

			if (ctlr->data)
			{
				*ctlr->data++ = inb(ADFIFO(ctlr));
				if (ctlr->data == ctlr->data_end)	/* Normal completion */
				{
					done_and_start_next(ctlr, bp, 0);
					return;
				}
			}
			else	/* Interrupt with no where to put the data.  */
			{
				printf("ad_intr: (should not happen) dropped input.\n");
				(void)inb(ADFIFO(ctlr));

				printf("bp %0p, status %x, cr3 %x\n", bp, status,
				ctlr->cr_image[2]);

				ctlr->err = DROPPED_INPUT;
				return;
			}
		}
	}
}

void labpcintr(int unit)
{
	struct ctlr *ctlr = labpcs[unit];
	(*ctlr->intr)(ctlr);
}

/* lockout_multiple_opens: Return whether or not we can open again, or
 * if the new mode is inconsistent with an already opened mode.
 * We only permit multiple opens for digital I/O now.
 */

static int
lockout_multiple_open(dev_t current, dev_t next)
{
	return ! (DIGITAL(current) && DIGITAL(next));
}

static	int
labpcopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_short unit = UNIT(dev);

	struct ctlr *ctlr;

	if (unit >= MAX_UNITS)
		return ENXIO;

	ctlr = labpcs[unit];

	if (ctlr == 0)
		return ENXIO;

	/* Don't allow another open if we have to change modes.
	 */

	if ( (ctlr->flags & BUSY) == 0)
	{
		ctlr->flags |= BUSY;
		ctlr->kdc.kdc_state = DC_BUSY;

		reset(ctlr);

		ctlr->err = 0;
		ctlr->dev = dev;

		ctlr->intr = null_intr;
		ctlr->starter = null_start;
		ctlr->stop = null_stop;
	}
	else if (lockout_multiple_open(ctlr->dev, dev))
		return EBUSY;

	return 0;
}

static	int
labpcclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct ctlr *ctlr = labpcs[UNIT(dev)];

	(*ctlr->stop)(ctlr);

	ctlr->kdc.kdc_state = DC_IDLE;
	ctlr->flags &= ~BUSY;

	return 0;
}

/* Start: Start a frame going in or out.
 */
static void
start(struct ctlr *ctlr)
{
	struct buf *bp;

	if ((bp = ctlr->start_queue.b_actf) == 0)
	{
		/* We must turn off FIFO interrupts when there is no
		 * place to put the data.  We have to get back to
		 * reading before the FIFO overflows.
		 */
		CR_EXPR(ctlr, 3, &= ~(FIFOINTEN|ERRINTEN));
		ctlr->cleared_intr = 1;
		ctlr->start_queue.b_active = 0;
		return;
	}

	ctlr->data = (u_char *)bp->b_un.b_addr;
	ctlr->data_end = ctlr->data + bp->b_bcount;

	if (ctlr->err)
	{
		printf("labpc start: (should not happen) error between records.\n");
		done_and_start_next(ctlr, bp, EIO);
		return;
	}

	if (ctlr->data == 0)
	{
		printf("labpc start: (should not happen) NULL data pointer.\n");
		done_and_start_next(ctlr, bp, EIO);
		return;
	}


	(*ctlr->starter)(ctlr, bp->b_bcount);

	if (!FIFOINTENABLED(ctlr))	/* We can store the data again */
	{
		CR_EXPR(ctlr, 3, |= (FIFOINTEN|ERRINTEN));

		/* Don't wait for the interrupts to fill things up.
		 */
		(*ctlr->intr)(ctlr);
	}

	timeout(tmo_stop, ctlr, ctlr->tmo);
}

static void
ad_strategy(struct buf *bp, struct ctlr *ctlr)
{
	int s;

	s = spltty();
	bp->b_actf = NULL;

	if (ctlr->start_queue.b_active)
	{
		ctlr->last->b_actf = bp;
		ctlr->last = bp;
	}
	else
	{
		ctlr->start_queue.b_active = 1;
		ctlr->start_queue.b_actf = bp;
		ctlr->last = bp;
		start(ctlr);
	}
	splx(s);
}

/* da_strategy: Send data to the D-A.  The CHAN field should be
 * 0: D-A port 0
 * 1: D-A port 1
 * 2: Alternate port 0 then port 1
 *
 * XXX:
 *
 * 1. There is no state for CHAN field 2:
 * the first sample in each buffer goes to channel 0.
 *
 * 2. No interrupt support yet.
 */
static void
da_strategy(struct buf *bp, struct ctlr *ctlr)
{
	int len;
	u_char *data;
	int port;
	int i;

	switch(CHAN(bp->b_dev))
	{
		case 0:
			port = DAC0L(ctlr);
			break;

		case 1:
			port = DAC1L(ctlr);
			break;

		case 2:	/* Device 2 handles both ports interleaved. */
			if (bp->b_bcount <= 2)
			{
				port = DAC0L(ctlr);
				break;
			}

			len = bp->b_bcount / 2;
			data = (u_char *)bp->b_un.b_addr;

			for (i = 0; i < len; i++)
			{
				loutb(DAC0H(ctlr), *data++);
				loutb(DAC0L(ctlr), *data++);
				loutb(DAC1H(ctlr), *data++);
				loutb(DAC1L(ctlr), *data++);
			}

			bp->b_resid = bp->b_bcount & 3;
			bp_done(bp, 0);
			return;

		default:
			bp_done(bp, ENXIO);
			return;
	}

	/* Port 0 or 1 falls through to here.
	 */
	if (bp->b_bcount & 1)	/* Odd transfers are illegal */
		bp_done(bp, EIO);

	len = bp->b_bcount;
	data = (u_char *)bp->b_un.b_addr;

	for (i = 0; i < len; i++)
	{
		loutb(port + 1, *data++);
		loutb(port, *data++);
	}

	bp->b_resid = 0;

	bp_done(bp, 0);
}

/* Input masks for MODE 0 of the ports treating PC as a single
 * 8 bit port.  Set these bits to set the port to input.
 */
                            /* A     B    lowc  highc combined */
static u_char set_input[] = { 0x10, 0x02, 0x01,  0x08,  0x09 };

static void flush_dcr(struct ctlr *ctlr)
{
	if (ctlr->dcr_is != ctlr->dcr_val)
	{
		loutb(DCR(ctlr), ctlr->dcr_val);
		ctlr->dcr_is = ctlr->dcr_val;
	}
}

/* do: Digital output
 */
static void
digital_out_strategy(struct buf *bp, struct ctlr *ctlr)
{
	int len;
	u_char *data;
	int port;
	int i;
	int chan = CHAN(bp->b_dev);

	ctlr->dcr_val &= ~set_input[chan];	/* Digital out: Clear bit */
	flush_dcr(ctlr);

	port = PORTX(ctlr, chan);

	len = bp->b_bcount;
	data = (u_char *)bp->b_un.b_addr;

	for (i = 0; i < len; i++)
	{
		loutb(port, *data++);
	}

	bp->b_resid = 0;

	bp_done(bp, 0);
}

/* digital_in_strategy: Digital input
 */
static void
digital_in_strategy(struct buf *bp, struct ctlr *ctlr)
{
	int len;
	u_char *data;
	int port;
	int i;
	int chan = CHAN(bp->b_dev);

	ctlr->dcr_val |= set_input[chan];	/* Digital in: Set bit */
	flush_dcr(ctlr);
	port = PORTX(ctlr, chan);

	len = bp->b_bcount;
	data = (u_char *)bp->b_un.b_addr;

	for (i = 0; i < len; i++)
	{
		*data++ = inb(port);
	}

	bp->b_resid = 0;

	bp_done(bp, 0);
}


static	void
labpcstrategy(struct buf *bp)
{
	struct ctlr *ctlr = labpcs[UNIT(bp->b_dev)];

	if (DIGITAL(bp->b_dev)) {
		if (bp->b_flags & B_READ) {
			ctlr->starter = null_start;
			ctlr->stop = all_stop;
			ctlr->intr = null_intr;
			digital_in_strategy(bp, ctlr);
		}
		else
		{
			ctlr->starter = null_start;
			ctlr->stop = all_stop;
			ctlr->intr = null_intr;
			digital_out_strategy(bp, ctlr);
		}
	}
	else {
		if (bp->b_flags & B_READ) {

			ctlr->starter = INTERVAL(ctlr->dev) ? ad_interval_start : ad_start;
			ctlr->stop = all_stop;
			ctlr->intr = ad_intr;
			ad_strategy(bp, ctlr);
		}
		else
		{
			ctlr->starter = null_start;
			ctlr->stop = all_stop;
			ctlr->intr = null_intr;
			da_strategy(bp, ctlr);
		}
	}
}

static	int
labpcioctl(dev_t dev, int cmd, caddr_t arg, int mode, struct proc *p)
{
	struct ctlr *ctlr = labpcs[UNIT(dev)];

	switch(cmd)
	{
		case AD_MICRO_PERIOD_SET:
		{
			/* XXX I'm only supporting what I have to, which is
			 * no slow periods.  You can't get any slower than 15 Hz
			 * with the current setup.  To go slower you'll need to
			 * support TCINTEN in CR3.
			 */

			long sample_us = *(long *)arg;

			if (sample_us > 65535)
				return EIO;

			ctlr->sample_us = sample_us;
			return 0;
		}

		case AD_MICRO_PERIOD_GET:
			*(long *)arg = ctlr->sample_us;
			return 0;

		case AD_NGAINS_GET:
			*(int *)arg = 8;
			return 0;

		case AD_NCHANS_GET:
			*(int *)arg = 8;
			return 0;

		case AD_SUPPORTED_GAINS:
		{
			static double gains[] = {1., 1.25, 2., 5., 10., 20., 50., 100.};
			copyout(gains, *(caddr_t *)arg, sizeof(gains));

			return 0;
		}

		case AD_GAINS_SET:
		{
			copyin(*(caddr_t *)arg, ctlr->gains, sizeof(ctlr->gains));
			return 0;
		}

		case AD_GAINS_GET:
		{
			copyout(ctlr->gains, *(caddr_t *)arg, sizeof(ctlr->gains));
			return 0;
		}

		default:
			return ENOTTY;
	}
}


static labpc_devsw_installed = 0;

static void 	labpc_drvinit(void *unused)
{
	dev_t dev;

	if( ! labpc_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&labpc_cdevsw,NULL);
		labpc_devsw_installed = 1;
    	}
}

SYSINIT(labpcdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,labpc_drvinit,NULL)


