/*
 * Streamer tape driver for 386bsd and FreeBSD.
 * Supports Archive QIC-02 and Wangtek QIC-02/QIC-36 boards.
 *
 * Copyright (C) 1993 by:
 *      Sergey Ryzhkov       <sir@kiae.su>
 *      Serge Vakulenko      <vak@zebub.msk.su>
 *
 * Placed in the public domain with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * This driver is derived from the old 386bsd Wangtek streamer tape driver,
 * made by Robert Baron at CMU, based on Intel sources.
 * Authors thank Robert Baron, CMU and Intel and retain here
 * the original CMU copyright notice.
 *
 *	from: Version 1.1, Fri Sep 24 02:14:31 MSD 1993
 *	$Id: wt.c,v 1.3 1993/10/16 13:46:35 rgrimes Exp $
 */

/*
 * Copyright (c) 1989 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Robert Baron
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "wt.h"
#if NWT > 0

#include "sys/param.h"
#include "sys/buf.h"
#include "sys/fcntl.h"
#include "sys/malloc.h"
#include "sys/ioctl.h"
#include "sys/mtio.h"
#include "vm/vm_param.h"
#include "i386/include/pio.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/wtreg.h"

#define WTPRI                   (PZERO+10)      /* sleep priority */
#define BLKSIZE                 512             /* streamer tape block size */

/*
 * Wangtek controller ports
 */
#define WT_CTLPORT(base)        ((base)+0)      /* control, write only */
#define WT_STATPORT(base)       ((base)+0)      /* status, read only */
#define WT_CMDPORT(base)        ((base)+1)      /* command, write only */
#define WT_DATAPORT(base)       ((base)+1)      /* data, read only */
#define WT_NPORT                2               /* 2 i/o ports */

/* status port bits */
#define WT_BUSY                 0x01            /* not ready bit define */
#define WT_NOEXCEP              0x02            /* no exception bit define */
#define WT_RESETMASK            0x07            /* to check after reset */
#define WT_RESETVAL             0x05            /* state after reset */

/* control port bits */
#define WT_ONLINE               0x01            /* device selected */
#define WT_RESET                0x02            /* reset command */
#define WT_REQUEST              0x04            /* request command */
#define WT_IEN(chan)            ((chan)>2 ? 0x10 : 0x8) /* enable intr */

/*
 * Archive controller ports
 */
#define AV_DATAPORT(base)       ((base)+0)      /* data, read only */
#define AV_CMDPORT(base)        ((base)+0)      /* command, write only */
#define AV_STATPORT(base)       ((base)+1)      /* status, read only */
#define AV_CTLPORT(base)        ((base)+1)      /* control, write only */
#define AV_SDMAPORT(base)       ((base)+2)      /* start dma */
#define AV_RDMAPORT(base)       ((base)+3)      /* reset dma */
#define AV_NPORT                4               /* 4 i/o ports */

/* status port bits */
#define AV_BUSY                 0x40            /* not ready bit define */
#define AV_NOEXCEP              0x20            /* no exception bit define */
#define AV_RESETMASK            0xf8            /* to check after reset */
#define AV_RESETVAL             0x50            /* state after reset */

/* control port bits */
#define AV_RESET                0x80            /* reset command */
#define AV_REQUEST              0x40            /* request command */
#define AV_IEN                  0x20            /* enable interrupts */

#define DMA_STATUSREG           0x8
#define DMA_DONE(chan)          (1 << (chan))

typedef struct {
	unsigned short err;             /* code for error encountered */
	unsigned short ercnt;           /* number of error blocks */
	unsigned short urcnt;           /* number of underruns */
} wtstatus_t;

typedef struct {
	unsigned unit;                  /* unit number */
	unsigned port;                  /* base i/o port */
	unsigned chan;                  /* dma channel number, 1..3 */
	unsigned flags;                 /* state of tape drive */
	unsigned dens;                  /* tape density */
	void *buf;                      /* internal i/o buffer */

	void *dmavaddr;                 /* virtual address of dma i/o buffer */
	unsigned dmatotal;              /* size of i/o buffer */
	unsigned dmaflags;              /* i/o direction, B_READ or B_WRITE */
	unsigned dmacount;              /* resulting length of dma i/o */

	wtstatus_t error;               /* status of controller */

	unsigned short DATAPORT, CMDPORT, STATPORT, CTLPORT, SDMAPORT, RDMAPORT;
	unsigned char BUSY, NOEXCEP, RESETMASK, RESETVAL;
	unsigned char ONLINE, RESET, REQUEST, IEN;
} wtinfo_t;

wtinfo_t wttab[NWT];                    /* tape info by unit number */

extern int hz;                          /* number of ticks per second */

static int wtwait (wtinfo_t *t, int catch, char *msg);
static int wtcmd (wtinfo_t *t, int cmd);
static int wtstart (wtinfo_t *t, unsigned mode, void *vaddr, unsigned len);
static void wtdma (wtinfo_t *t);
static void wtimer (wtinfo_t *t);
static void wtclock (wtinfo_t *t);
static int wtreset (wtinfo_t *t);
static int wtsense (wtinfo_t *t, int ignor);
static int wtstatus (wtinfo_t *t);
static void wtrewind (wtinfo_t *t);
static int wtreadfm (wtinfo_t *t);
static int wtwritefm (wtinfo_t *t);
static int wtpoll (wtinfo_t *t);

extern void DELAY (int usec);
extern void bcopy (void *from, void *to, unsigned len);
extern void isa_dmastart (int flags, void *addr, unsigned len, unsigned chan);
extern void isa_dmadone (int flags, void *addr, unsigned len, int chan);
extern void printf (char *str, ...);
extern int splbio (void);
extern int splx (int level);
extern void timeout (void (*func) (), void *arg, int timo);
extern int tsleep (void *chan, int priority, char *msg, int timo);
extern void wakeup (void *chan);

/*
 * Probe for the presence of the device.
 */
int wtprobe (struct isa_device *id)
{
	wtinfo_t *t = wttab + id->id_unit;

	t->unit = id->id_unit;
	t->chan = id->id_drq;
	t->port = 0;                    /* Mark it as not configured. */
	if (t->chan<1 || t->chan>3) {
		printf ("wt%d: Bad drq=%d, should be 1..3\n", t->unit, t->chan);
		return (0);
	}
	t->port = id->id_iobase;

	/* Try Wangtek. */
	t->CTLPORT = WT_CTLPORT (t->port);  t->STATPORT = WT_STATPORT (t->port);
	t->CMDPORT = WT_CMDPORT (t->port);  t->DATAPORT = WT_DATAPORT (t->port);
	t->SDMAPORT = 0;                    t->RDMAPORT = 0;
	t->BUSY = WT_BUSY;                  t->NOEXCEP = WT_NOEXCEP;
	t->RESETMASK = WT_RESETMASK;        t->RESETVAL = WT_RESETVAL;
	t->ONLINE = WT_ONLINE;              t->RESET = WT_RESET;
	t->REQUEST = WT_REQUEST;            t->IEN = WT_IEN (t->chan);
	if (wtreset (t))
		return (WT_NPORT);

	/* Try Archive. */
	t->CTLPORT = AV_CTLPORT (t->port);  t->STATPORT = AV_STATPORT (t->port);
	t->CMDPORT = AV_CMDPORT (t->port);  t->DATAPORT = AV_DATAPORT (t->port);
	t->SDMAPORT = AV_SDMAPORT (t->port); t->RDMAPORT = AV_RDMAPORT (t->port);
	t->BUSY = AV_BUSY;                  t->NOEXCEP = AV_NOEXCEP;
	t->RESETMASK = AV_RESETMASK;        t->RESETVAL = AV_RESETVAL;
	t->ONLINE = 0;                      t->RESET = AV_RESET;
	t->REQUEST = AV_REQUEST;            t->IEN = AV_IEN;
	if (wtreset (t))
		return (AV_NPORT);

	/* Tape controller not found. */
	t->port = 0;
	return (0);
}

/*
 * Device is found, configure it.
 */
int wtattach (struct isa_device *id)
{
	wtinfo_t *t = wttab + id->id_unit;

	if (t->RDMAPORT) {
		printf ("wt%d: type <Archive>\n", t->unit);
		outb (t->RDMAPORT, 0);          /* reset dma */
	} else
		printf ("wt%d: type <Wangtek>\n", t->unit);
	t->flags = TPSTART;                     /* tape is rewound */
	t->dens = -1;                           /* unknown density */
	t->buf = malloc (BLKSIZE, M_TEMP, M_NOWAIT);
	return (1);
}

struct isa_driver wtdriver = { wtprobe, wtattach, "wt", };

int wtdump (int dev)
{
	/* Not implemented */
	return (EINVAL);
}

int wtsize (int dev)
{
	/* Not implemented */
	return (-1);
}

/*
 * Open routine, called on every device open.
 */
int wtopen (int dev, int flag)
{
	int u = minor (dev) & T_UNIT;
	wtinfo_t *t = wttab + u;
	int error;

	if (u >= NWT || !t->port)
		return (ENXIO);

	/* Check that device is not in use */
	if (t->flags & TPINUSE)
		return (EBUSY);

	/* If the tape is in rewound state, check the status and set density. */
	if (t->flags & TPSTART) {
		/* If rewind is going on, wait */
		error = wtwait (t, PCATCH, "wtrew");
		if (error)
			return (error);

		if (! wtsense (t, (flag & FWRITE) ? 0 : TP_WRP)) {
			/* Bad status. Reset the controller. */
			if (! wtreset (t))
				return (ENXIO);
			if (! wtsense (t, (flag & FWRITE) ? 0 : TP_WRP))
				return (ENXIO);
		}

		/* Set up tape density. */
		if (t->dens != (minor (dev) & T_DENSEL)) {
			int d;

			switch (minor (dev) & T_DENSEL) {
			default:
			case T_800BPI:  d = QIC_FMT150; break;  /* minor 000 */
			case T_1600BPI: d = QIC_FMT120; break;  /* minor 010 */
			case T_6250BPI: d = QIC_FMT24;  break;  /* minor 020 */
			case T_BADBPI:  d = QIC_FMT11;  break;  /* minor 030 */
			}
			if (! wtcmd (t, d))
				return (ENXIO);

			/* Check the status of the controller. */
			if (! wtsense (t, (flag & FWRITE) ? 0 : TP_WRP))
				return (ENXIO);

			t->dens = minor (dev) & T_DENSEL;
		}
		t->flags &= ~TPSTART;
	} else if (t->dens != (minor (dev) & T_DENSEL))
		return (ENXIO);

	t->flags = TPINUSE;
	if (flag & FREAD)
		t->flags |= TPREAD;
	if (flag & FWRITE)
		t->flags |= TPWRITE;
	return (0);
}

/*
 * Close routine, called on last device close.
 */
int wtclose (int dev)
{
	int u = minor (dev) & T_UNIT;
	wtinfo_t *t = wttab + u;

	if (u >= NWT || !t->port)
		return (ENXIO);

	/* If rewind is pending, do nothing */
	if (t->flags & TPREW)
		goto done;

	/* If seek forward is pending and no rewind on close, do nothing */
	if ((t->flags & TPRMARK) && (minor (dev) & T_NOREWIND))
		goto done;

	/* If file mark read is going on, wait */
	wtwait (t, 0, "wtrfm");

	if (t->flags & TPWANY)
		/* Tape was written.  Write file mark. */
		wtwritefm (t);

	if (! (minor (dev) & T_NOREWIND)) {
		/* Rewind tape to beginning of tape. */
		/* Don't wait until rewind, though. */
		wtrewind (t);
		goto done;
	}
	if ((t->flags & TPRANY) && ! (t->flags & (TPVOL | TPWANY)))
		/* Space forward to after next file mark if no writing done. */
		/* Don't wait for completion. */
		wtreadfm (t);
done:
	t->flags &= TPREW | TPRMARK | TPSTART | TPTIMER;
	return (0);
}

/*
 * Ioctl routine.  Compatible with BSD ioctls.
 * Direct QIC-02 commands ERASE and RETENSION added.
 * There are three possible ioctls:
 * ioctl (int fd, MTIOCGET, struct mtget *buf)  -- get status
 * ioctl (int fd, MTIOCTOP, struct mtop *buf)   -- do BSD-like op
 * ioctl (int fd, WTQICMD, int qicop)           -- do QIC op
 */
int wtioctl (int dev, int cmd, void *arg, int mode)
{
	int u = minor (dev) & T_UNIT;
	wtinfo_t *t = wttab + u;
	int error, count, op;

	if (u >= NWT || !t->port)
		return (ENXIO);

	switch (cmd) {
	default:
		return (EINVAL);
	case WTQICMD:                   /* direct QIC command */
		op = (int) *(void**)arg;
		switch (op) {
		default:
			return (EINVAL);
		case QIC_ERASE:         /* erase the whole tape */
			if (! (t->flags & TPWRITE) || (t->flags & TPWP))
				return (EACCES);
			if (error = wtwait (t, PCATCH, "wterase"))
				return (error);
			break;
		case QIC_RETENS:        /* retension the tape */
			if (error = wtwait (t, PCATCH, "wtretens"))
				return (error);
			break;
		}
		/* Both ERASE and RETENS operations work like REWIND. */
		/* Simulate the rewind operation here. */
		t->flags &= ~(TPRO | TPWO | TPVOL);
		if (! wtcmd (t, op))
			return (EIO);
		t->flags |= TPSTART | TPREW;
		if (op == QIC_ERASE)
			t->flags |= TPWANY;
		wtclock (t);
		return (0);
	case MTIOCIEOT:         /* ignore EOT errors */
	case MTIOCEEOT:         /* enable EOT errors */
		return (0);
	case MTIOCGET:
		((struct mtget*)arg)->mt_type = t->RDMAPORT ? MT_ISVIPER1 : 0x11;
		((struct mtget*)arg)->mt_dsreg = t->flags;      /* status */
		((struct mtget*)arg)->mt_erreg = t->error.err;  /* errors */
		((struct mtget*)arg)->mt_resid = 0;
		((struct mtget*)arg)->mt_fileno = 0;            /* file */
		((struct mtget*)arg)->mt_blkno = 0;             /* block */
		return (0);
	case MTIOCTOP:
		break;
	}
	switch ((short) ((struct mtop*)arg)->mt_op) {
	default:
	case MTFSR:             /* forward space record */
	case MTBSR:             /* backward space record */
	case MTBSF:             /* backward space file */
		break;
	case MTNOP:             /* no operation, sets status only */
	case MTCACHE:           /* enable controller cache */
	case MTNOCACHE:         /* disable controller cache */
		return (0);
	case MTREW:             /* rewind */
	case MTOFFL:            /* rewind and put the drive offline */
		if (t->flags & TPREW)   /* rewind is running */
			return (0);
		if (error = wtwait (t, PCATCH, "wtorew"))
			return (error);
		wtrewind (t);
		return (0);
	case MTFSF:             /* forward space file */
		for (count=((struct mtop*)arg)->mt_count; count>0; --count) {
			if (error = wtwait (t, PCATCH, "wtorfm"))
				return (error);
			if (error = wtreadfm (t))
				return (error);
		}
		return (0);
	case MTWEOF:            /* write an end-of-file record */
		if (! (t->flags & TPWRITE) || (t->flags & TPWP))
			return (EACCES);
		if (error = wtwait (t, PCATCH, "wtowfm"))
			return (error);
		if (error = wtwritefm (t))
			return (error);
		return (0);
	}
	return (EINVAL);
}

/*
 * Strategy routine.
 */
void wtstrategy (struct buf *bp)
{
	int u = minor (bp->b_dev) & T_UNIT;
	wtinfo_t *t = wttab + u;
	int s;

	bp->b_resid = bp->b_bcount;
	if (u >= NWT || !t->port)
		goto errxit;

	/* at file marks and end of tape, we just return '0 bytes available' */
	if (t->flags & TPVOL)
		goto xit;

	if (bp->b_flags & B_READ) {
		/* Check read access and no previous write to this tape. */
		if (! (t->flags & TPREAD) || (t->flags & TPWANY))
			goto errxit;

		/* For now, we assume that all data will be copied out */
		/* If read command outstanding, just skip down */
		if (! (t->flags & TPRO)) {
			if (! wtsense (t, TP_WRP))      /* clear status */
				goto errxit;
			if (! wtcmd (t, QIC_RDDATA)) {  /* sed read mode */
				wtsense (t, TP_WRP);
				goto errxit;
			}
			t->flags |= TPRO | TPRANY;
		}
	} else {
		/* Check write access and write protection. */
		/* No previous read from this tape allowed. */
		if (! (t->flags & TPWRITE) || (t->flags & (TPWP | TPRANY)))
			goto errxit;

		/* If write command outstanding, just skip down */
		if (! (t->flags & TPWO)) {
			if (! wtsense (t, 0))           /* clear status */
				goto errxit;
			if (! wtcmd (t, QIC_WRTDATA)) { /* set write mode */
				wtsense (t, 0);
				goto errxit;
			}
			t->flags |= TPWO | TPWANY;
		}
	}

	if (! bp->b_bcount)
		goto xit;

	t->flags &= ~TPEXCEP;
	s = splbio ();
	if (wtstart (t, bp->b_flags, bp->b_un.b_addr, bp->b_bcount)) {
		wtwait (t, 0, (bp->b_flags & B_READ) ? "wtread" : "wtwrite");
		bp->b_resid -= t->dmacount;
	}
	splx (s);

	if (t->flags & TPEXCEP) {
errxit:         bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
xit:    biodone (bp);
	return;
}

/*
 * Interrupt routine.
 */
void wtintr (int u)
{
	wtinfo_t *t = wttab + u;
	unsigned char s;

	if (u >= NWT || !t->port)
		return;

	s = inb (t->STATPORT);                  /* get status */
	if ((s & (t->BUSY | t->NOEXCEP)) == (t->BUSY | t->NOEXCEP))
		return;                         /* device is busy */
	outb (t->CTLPORT, t->ONLINE);           /* stop controller */

	/*
	 * Check if rewind finished.
	 */
	if (t->flags & TPREW) {
		t->flags &= ~TPREW;             /* Rewind finished. */
		wtsense (t, TP_WRP);
		wakeup (t);
		return;
	}

	/*
	 * Check if writing/reading of file mark finished.
	 */
	if (t->flags & (TPRMARK | TPWMARK)) {
		if (! (s & t->NOEXCEP))         /* Operation failed. */
			wtsense (t, (t->flags & TPRMARK) ? TP_WRP : 0);
		t->flags &= ~(TPRMARK | TPWMARK); /* Operation finished. */
		wakeup (t);
		return;
	}

	/*
	 * Do we started any i/o?  If no, just return.
	 */
	if (! (t->flags & TPACTIVE))
		return;
	t->flags &= ~TPACTIVE;

	if (inb (DMA_STATUSREG) & DMA_DONE (t->chan))   /* if dma finished */
		t->dmacount += BLKSIZE;                 /* increment counter */

	/*
	 * Clean up dma.
	 */
	if ((t->dmaflags & B_READ) && (t->dmatotal - t->dmacount) < BLKSIZE) {
		/* If the address crosses 64-k boundary, or reading short block,
		 * copy the internal buffer to the user memory. */
		isa_dmadone (t->dmaflags, t->buf, BLKSIZE, t->chan);
		bcopy (t->buf, t->dmavaddr, t->dmatotal - t->dmacount);
	} else
		isa_dmadone (t->dmaflags, t->dmavaddr, BLKSIZE, t->chan);

	/*
	 * On exception, check for end of file and end of volume.
	 */
	if (! (s & t->NOEXCEP)) {
		wtsense (t, (t->dmaflags & B_READ) ? TP_WRP : 0);
		if (t->error.err & (TP_EOM | TP_FIL))
			t->flags |= TPVOL;      /* end of file */
		else
			t->flags |= TPEXCEP;    /* i/o error */
		wakeup (t);
		return;
	}

	if (t->dmacount < t->dmatotal) {        /* continue i/o */
		t->dmavaddr += BLKSIZE;
		wtdma (t);
		return;
	}
	if (t->dmacount > t->dmatotal)          /* short last block */
		t->dmacount = t->dmatotal;
	wakeup (t);                             /* wake up user level */
}

/* start the rewind operation */
static void wtrewind (wtinfo_t *t)
{
	t->flags &= ~(TPRO | TPWO | TPVOL);
	if (! wtcmd (t, QIC_REWIND))
		return;
	t->flags |= TPSTART | TPREW;
	wtclock (t);
}

/* start the `read marker' operation */
static int wtreadfm (wtinfo_t *t)
{
	t->flags &= ~(TPRO | TPWO | TPVOL);
	if (! wtcmd (t, QIC_READFM)) {
		wtsense (t, TP_WRP);
		return (EIO);
	}
	t->flags |= TPRMARK | TPRANY;
	wtclock (t);
	/* Don't wait for completion here. */
	return (0);
}

/* write marker to the tape */
static int wtwritefm (wtinfo_t *t)
{
	tsleep (wtwritefm, WTPRI, "wtwfm", hz);         /* timeout: 1 second */
	t->flags &= ~(TPRO | TPWO);
	if (! wtcmd (t, QIC_WRITEFM)) {
		wtsense (t, 0);
		return (EIO);
	}
	t->flags |= TPWMARK | TPWANY;
	wtclock (t);
	return (wtwait (t, 0, "wtwfm"));
}

/* wait for controller ready or exception */
static int wtpoll (wtinfo_t *t)
{
	int s, NOTREADY = t->BUSY | t->NOEXCEP;

	/* Poll status port, waiting for ready or exception. */
	do s = inb (t->STATPORT);
	while ((s & NOTREADY) == NOTREADY);
	return (s);
}

/* execute QIC command */
static int wtcmd (wtinfo_t *t, int cmd)
{
	if (! (wtpoll (t) & t->NOEXCEP))                /* wait for ready */
		return (0);                             /* error */
	
	outb (t->CMDPORT, cmd);                         /* output the command */

	outb (t->CTLPORT, t->REQUEST | t->ONLINE);      /* set request */
	while (inb (t->STATPORT) & t->BUSY)             /* wait for ready */
		continue;
	outb (t->CTLPORT, t->IEN | t->ONLINE);          /* reset request */
	while (! (inb (t->STATPORT) & t->BUSY))         /* wait for not ready */
		continue;

	return (1);
}

/* wait for the end of i/o, seeking marker or rewind operation */
static int wtwait (wtinfo_t *t, int catch, char *msg)
{
	int error;

	while (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		if (error = tsleep (t, WTPRI | catch, msg, 0))
			return (error);
	return (0);
}

/* initialize dma for the i/o operation */
static void wtdma (wtinfo_t *t)
{
	t->flags |= TPACTIVE;
	wtclock (t);

	if (t->SDMAPORT)
		outb (t->SDMAPORT, 0);          /* set dma */

	if ((t->dmaflags & B_READ) && (t->dmatotal - t->dmacount) < BLKSIZE)
		/* Reading short block.  Do it through the internal buffer. */
		isa_dmastart (t->dmaflags, t->buf, BLKSIZE, t->chan);
	else
		isa_dmastart (t->dmaflags, t->dmavaddr, BLKSIZE, t->chan);

	outb (t->CTLPORT, t->IEN | t->ONLINE);
}

/* start i/o operation */
static int wtstart (wtinfo_t *t, unsigned flags, void *vaddr, unsigned len)
{
	if (! (wtpoll (t) & t->NOEXCEP)) {      /* wait for ready or error */
		t->flags |= TPEXCEP;            /* error */
		return (0);
	}
	t->flags &= ~TPEXCEP;                   /* clear exception flag */
	t->dmavaddr = vaddr;
	t->dmatotal = len;
	t->dmacount = 0;
	t->dmaflags = flags;
	wtdma (t);
	return (1);
}

/* start timer */
static void wtclock (wtinfo_t *t)
{
	if (! (t->flags & TPTIMER)) {
		t->flags |= TPTIMER;
		timeout (wtimer, t, hz);
	}
}

/*
 * Simulate an interrupt periodically while i/o is going.
 * This is necessary in case interrupts get eaten due to
 * multiple devices on a single IRQ line.
 */
static void wtimer (wtinfo_t *t)
{
	int s;

	t->flags &= ~TPTIMER;
	if (! (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK)))
		return;

	/* If i/o going, simulate interrupt. */
	s = splbio ();
	wtintr (t->unit);
	splx (s);

	/* Restart timer if i/o pending. */
	if (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		wtclock (t);
}

/* reset the controller */
static int wtreset (wtinfo_t *t)
{
	outb (t->CTLPORT, t->RESET);            /* send reset */
	DELAY (25);
	outb (t->CTLPORT, 0);                   /* turn off reset */
	if ((inb (t->STATPORT) & t->RESETMASK) != t->RESETVAL)
		return (0);
	return (1);
}

/* get controller status information */
/* return 0 if user i/o request should receive an i/o error code */
static int wtsense (wtinfo_t *t, int ignor)
{
	char *msg = 0;
	int err;

	t->flags &= ~(TPRO | TPWO);
	if (! wtstatus (t))
		return (0);
	if (! (t->error.err & TP_ST0))
		t->error.err &= ~TP_ST0MASK;
	if (! (t->error.err & TP_ST1))
		t->error.err &= ~TP_ST1MASK;
	t->error.err &= ~ignor;         /* ignore certain errors */
	err = t->error.err & (TP_FIL | TP_BNL | TP_UDA | TP_EOM | TP_WRP |
		TP_USL | TP_CNI | TP_MBD | TP_NDT | TP_ILL);
	if (! err)
		return (1);

	/* lifted from tdriver.c from Wangtek */
	if      (err & TP_USL)  msg = "Drive not online";
	else if (err & TP_CNI)  msg = "No cartridge";
	else if ((err & TP_WRP) && !(t->flags & TPWP)) {
		msg = "Tape is write protected";
		t->flags |= TPWP;
	}
	else if (err & TP_FIL)  msg = 0 /*"Filemark detected"*/;
	else if (err & TP_EOM)  msg = 0 /*"End of tape"*/;
	else if (err & TP_BNL)  msg = "Block not located";
	else if (err & TP_UDA)  msg = "Unrecoverable data error";
	else if (err & TP_NDT)  msg = "No data detected";
	else if (err & TP_ILL)  msg = "Illegal command";
	if (msg)
		printf ("wt%d: %s\n", t->unit, msg);
	return (0);
}

/* get controller status information */
static int wtstatus (wtinfo_t *t)
{
	char *p;

	wtpoll (t);                     /* wait for ready or exception */
	outb (t->CMDPORT, QIC_RDSTAT);  /* send `read status' command */

	outb (t->CTLPORT, t->REQUEST | t->ONLINE);      /* set request */
	while (inb (t->STATPORT) & t->BUSY)             /* wait for ready */
		continue;
	outb (t->CTLPORT, t->ONLINE);                   /* reset request */
	while (! (inb (t->STATPORT) & t->BUSY))         /* wait for not ready */
		continue;

	p = (char*) &t->error;
	while (p < (char*)&t->error + 6) {
		if (! (wtpoll (t) & t->NOEXCEP))        /* wait for ready */
			return (0);                     /* error */

		*p++ = inb (t->DATAPORT);               /* read status byte */

		outb (t->CTLPORT, t->REQUEST);          /* set request */
		while (! (inb (t->STATPORT) & t->BUSY)) /* wait for not ready */
			continue;
		/* DELAY (50); */                       /* wait 50 usec */
		outb (t->CTLPORT, 0);                   /* unset request */
	}
	return (1);
}
#endif /* NWT */
