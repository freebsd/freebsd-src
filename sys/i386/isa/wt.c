
/*
 * Streamer tape driver for 386bsd and FreeBSD.
 * Supports Archive and Wangtek compatible QIC-02/QIC-36 boards.
 *
 * Copyright (C) 1993 by:
 *      Sergey Ryzhkov       <sir@kiae.su>
 *      Serge Vakulenko      <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
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
 * Version 1.3, Thu Nov 11 12:09:13 MSK 1993
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Code for MTERASE added by John Lind (john@starfire.mn.org) 95/09/02.
 * This was very easy due to the excellent structure and clear coding
 * of the original driver.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/mtio.h>
#include <sys/conf.h>
#include <sys/bus.h>


#include <i386/isa/isa_device.h>
#include <i386/isa/wtreg.h>

#ifndef COMPAT_OLDISA
#error "The wt device requires the old isa compatibility shims"
#endif

/*
 * Uncomment this to enable internal device tracing.
 */
#define TRACE(s)                /* printf s */

#define WTPRI                   (PZERO+10)      /* sleep priority */

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
#define WT_IEN                  0x08            /* enable dma */

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

enum wttype {
	UNKNOWN = 0,                    /* unknown type, driver disabled */
	ARCHIVE,                        /* Archive Viper SC499, SC402 etc */
	WANGTEK                         /* Wangtek */
};

typedef struct {
	unsigned short err;             /* code for error encountered */
	unsigned short ercnt;           /* number of error blocks */
	unsigned short urcnt;           /* number of underruns */
} wtstatus_t;

typedef struct {
	enum wttype type;               /* type of controller */
	unsigned unit;                  /* unit number */
	unsigned port;                  /* base i/o port */
	unsigned chan;                  /* dma channel number, 1..3 */
	unsigned flags;                 /* state of tape drive */
	unsigned dens;                  /* tape density */
	int bsize;                      /* tape block size */
	void *buf;                      /* internal i/o buffer */

	void *dmavaddr;                 /* virtual address of dma i/o buffer */
	unsigned dmatotal;              /* size of i/o buffer */
	unsigned dmaflags;              /* i/o direction, ISADMA_READ or ISADMA_WRITE */
	unsigned dmacount;              /* resulting length of dma i/o */

	wtstatus_t error;               /* status of controller */

	unsigned DATAPORT, CMDPORT, STATPORT, CTLPORT, SDMAPORT, RDMAPORT;
	unsigned char BUSY, NOEXCEP, RESETMASK, RESETVAL;
	unsigned char ONLINE, RESET, REQUEST, IEN;
} wtinfo_t;

static wtinfo_t wttab[NWT];                    /* tape info by unit number */

static int wtwait (wtinfo_t *t, int catch, char *msg);
static int wtcmd (wtinfo_t *t, int cmd);
static int wtstart (wtinfo_t *t, unsigned mode, void *vaddr, unsigned len);
static void wtdma (wtinfo_t *t);
static timeout_t wtimer;
static void wtclock (wtinfo_t *t);
static int wtreset (wtinfo_t *t);
static int wtsense (wtinfo_t *t, int verb, int ignor);
static int wtstatus (wtinfo_t *t);
static ointhand2_t wtintr;
static void wtrewind (wtinfo_t *t);
static int wtreadfm (wtinfo_t *t);
static int wtwritefm (wtinfo_t *t);
static int wtpoll (wtinfo_t *t, int mask, int bits);

static	d_open_t	wtopen;
static	d_close_t	wtclose;
static	d_ioctl_t	wtioctl;
static	d_strategy_t	wtstrategy;

#define CDEV_MAJOR 10

static struct cdevsw wt_cdevsw = {
	.d_open =	wtopen,
	.d_close =	wtclose,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	wtioctl,
	.d_strategy =	wtstrategy,
	.d_name =	"wt",
	.d_maj =	CDEV_MAJOR,
};


/*
 * Probe for the presence of the device.
 */
static int 
wtprobe (struct isa_device *id)
{
	wtinfo_t *t = wttab + id->id_unit;

	t->unit = id->id_unit;
	t->chan = id->id_drq;
	t->port = id->id_iobase;
	if (t->chan<1 || t->chan>3) {
		printf ("wt%d: Bad drq=%d, should be 1..3\n", t->unit, t->chan);
		return (0);
	}

	/* Try Wangtek. */
	t->type = WANGTEK;
	t->CTLPORT = WT_CTLPORT (t->port);  t->STATPORT = WT_STATPORT (t->port);
	t->CMDPORT = WT_CMDPORT (t->port);  t->DATAPORT = WT_DATAPORT (t->port);
	t->SDMAPORT = 0;                    t->RDMAPORT = 0;
	t->BUSY = WT_BUSY;                  t->NOEXCEP = WT_NOEXCEP;
	t->RESETMASK = WT_RESETMASK;        t->RESETVAL = WT_RESETVAL;
	t->ONLINE = WT_ONLINE;              t->RESET = WT_RESET;
	t->REQUEST = WT_REQUEST;            t->IEN = WT_IEN;
	if (wtreset (t))
		return (WT_NPORT);

	/* Try Archive. */
	t->type = ARCHIVE;
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
	t->type = UNKNOWN;
	return (0);
}

/*
 * Device is found, configure it.
 */
static int
wtattach (struct isa_device *id)
{
	wtinfo_t *t = wttab + id->id_unit;
	dev_t dev;

	id->id_ointr = wtintr;
	if (t->type == ARCHIVE) {
		printf ("wt%d: type <Archive>\n", t->unit);
		outb (t->RDMAPORT, 0);          /* reset dma */
	} else
		printf ("wt%d: type <Wangtek>\n", t->unit);
	t->flags = TPSTART;                     /* tape is rewound */
	t->dens = -1;                           /* unknown density */
	isa_dmainit(t->chan, 1024);

	dev = make_dev(&wt_cdevsw, id->id_unit,
	    UID_ROOT, GID_WHEEL, 0600, "rwt%d", id->id_unit);
	dev->si_drv1 = t;
	return (1);
}

struct isa_driver wtdriver = {
	INTR_TYPE_BIO,
	wtprobe,
	wtattach,
	"wt",
};
COMPAT_ISA_DRIVER(wt, wtdriver);

/*
 * Open routine, called on every device open.
 */
static int
wtopen (dev_t dev, int flag, int fmt, struct thread *td)
{
	int u = minor (dev) & WT_UNIT;
	wtinfo_t *t;
	int error;

	if (u >= NWT)
		return (ENXIO);

	t = dev->si_drv1;

	if (t->type == UNKNOWN)
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

		/* Check the controller status */
		if (! wtsense (t, 0, (flag & FWRITE) ? 0 : TP_WRP)) {
			/* Bad status, reset the controller */
			if (! wtreset (t))
				return (EIO);
			if (! wtsense (t, 1, (flag & FWRITE) ? 0 : TP_WRP))
				return (EIO);
		}

		/* Set up tape density. */
		if (t->dens != (minor (dev) & WT_DENSEL)) {
			int d = 0;

			switch (minor (dev) & WT_DENSEL) {
			case WT_DENSDFLT: default: break; /* default density */
			case WT_QIC11:  d = QIC_FMT11;  break; /* minor 010 */
			case WT_QIC24:  d = QIC_FMT24;  break; /* minor 020 */
			case WT_QIC120: d = QIC_FMT120; break; /* minor 030 */
			case WT_QIC150: d = QIC_FMT150; break; /* minor 040 */
			case WT_QIC300: d = QIC_FMT300; break; /* minor 050 */
			case WT_QIC600: d = QIC_FMT600; break; /* minor 060 */
			}
			if (d) {
				/* Change tape density. */
				if (! wtcmd (t, d))
					return (EIO);
				if (! wtsense (t, 1, TP_WRP | TP_ILL))
					return (EIO);

				/* Check the status of the controller. */
				if (t->error.err & TP_ILL) {
					printf ("wt%d: invalid tape density\n", t->unit);
					return (ENODEV);
				}
			}
			t->dens = minor (dev) & WT_DENSEL;
		}
		t->flags &= ~TPSTART;
	} else if (t->dens != (minor (dev) & WT_DENSEL))
		return (ENXIO);

	t->bsize = (minor (dev) & WT_BSIZE) ? 1024 : 512;
	t->buf = malloc (t->bsize, M_TEMP, M_WAITOK);
	if (! t->buf)
		return (EAGAIN);

	if (isa_dma_acquire(t->chan))
		return(EBUSY);

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
static int
wtclose (dev_t dev, int flags, int fmt, struct thread *td)
{
	int u = minor (dev) & WT_UNIT;
	wtinfo_t *t;

	t = dev->si_drv1;

	if (u >= NWT || t->type == UNKNOWN)
		return (ENXIO);

	/* If rewind is pending, do nothing */
	if (t->flags & TPREW)
		goto done;

	/* If seek forward is pending and no rewind on close, do nothing */
	if (t->flags & TPRMARK) {
		if (minor (dev) & WT_NOREWIND)
			goto done;

		/* If read file mark is going on, wait */
		wtwait (t, 0, "wtrfm");
	}

	if (t->flags & TPWANY)
		/* Tape was written.  Write file mark. */
		wtwritefm (t);

	if (! (minor (dev) & WT_NOREWIND)) {
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
	free (t->buf, M_TEMP);
	isa_dma_release(t->chan);
	return (0);
}

/*
 * Ioctl routine.  Compatible with BSD ioctls.
 * There are two possible ioctls:
 * ioctl (int fd, MTIOCGET, struct mtget *buf)  -- get status
 * ioctl (int fd, MTIOCTOP, struct mtop *buf)   -- do BSD-like op
 */
static int
wtioctl (dev_t dev, u_long cmd, caddr_t arg, int flags, struct thread *td)
{
	int u = minor (dev) & WT_UNIT;
	wtinfo_t *t;
	int error, count, op;

	t = dev->si_drv1;
	if (u >= NWT || t->type == UNKNOWN)
		return (ENXIO);

	switch (cmd) {
	default:
		return (EINVAL);
	case MTIOCIEOT:         /* ignore EOT errors */
	case MTIOCEEOT:         /* enable EOT errors */
		return (0);
	case MTIOCGET:
		((struct mtget*)arg)->mt_type =
			t->type == ARCHIVE ? MT_ISVIPER1 : 0x11;
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
		error = wtwait (t, PCATCH, "wtorew");
		if (error)
			return (error);
		wtrewind (t);
		return (0);
	case MTFSF:             /* forward space file */
		for (count=((struct mtop*)arg)->mt_count; count>0; --count) {
			error = wtwait (t, PCATCH, "wtorfm");
			if (error)
				return (error);
			error = wtreadfm (t);
			if (error)
				return (error);
		}
		return (0);
	case MTWEOF:            /* write an end-of-file record */
		if (! (t->flags & TPWRITE) || (t->flags & TPWP))
			return (EACCES);
		error = wtwait (t, PCATCH, "wtowfm");
		if (error)
			return (error);
		error = wtwritefm (t);
		if (error)
			return (error);
		return (0);
	case MTRETENS:		/* re-tension tape */
		error = wtwait (t, PCATCH, "wtretens");
		if (error)
			return (error);
		op = QIC_RETENS;
		goto erase_retens;
		
	case MTERASE:		/* erase to EOM */
		if (! (t->flags & TPWRITE) || (t->flags & TPWP))
			return (EACCES);
		error = wtwait (t, PCATCH, "wterase");
		if (error)
			return (error);
		op = QIC_ERASE;
	erase_retens:
		/* ERASE and RETENS operations work like REWIND. */
		/* Simulate the rewind operation here. */
		t->flags &= ~(TPRO | TPWO | TPVOL);
		if (! wtcmd (t, op))
			return (EIO);
		t->flags |= TPSTART | TPREW;
		t->flags |= TPWANY;
		wtclock (t);
		return (0);
	}
	return (EINVAL);
}

/*
 * Strategy routine.
 */
static void
wtstrategy (struct bio *bp)
{
	int u = minor (bp->bio_dev) & WT_UNIT;
	wtinfo_t *t;
	int s;

	t = bp->bio_dev->si_drv1;
	bp->bio_resid = bp->bio_bcount;
	if (u >= NWT || t->type == UNKNOWN) {
		bp->bio_error = ENXIO;
		goto err2xit;
	}

	/* at file marks and end of tape, we just return '0 bytes available' */
	if (t->flags & TPVOL)
		goto xit;

	if (bp->bio_bcount % t->bsize != 0) {
		bp->bio_error = EINVAL;
		goto err2xit;
	}

	if (bp->bio_cmd == BIO_READ) {
		/* Check read access and no previous write to this tape. */
		if (! (t->flags & TPREAD) || (t->flags & TPWANY))
			goto errxit;

		/* For now, we assume that all data will be copied out */
		/* If read command outstanding, just skip down */
		if (! (t->flags & TPRO)) {
			if (! wtsense (t, 1, TP_WRP))   /* clear status */
				goto errxit;
			if (! wtcmd (t, QIC_RDDATA)) {  /* sed read mode */
				wtsense (t, 1, TP_WRP);
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
			if (! wtsense (t, 1, 0))        /* clear status */
				goto errxit;
			if (! wtcmd (t, QIC_WRTDATA)) { /* set write mode */
				wtsense (t, 1, 0);
				goto errxit;
			}
			t->flags |= TPWO | TPWANY;
		}
	}

	if (! bp->bio_bcount)
		goto xit;

	t->flags &= ~TPEXCEP;
	s = splbio ();
	if (wtstart (t, bp->bio_cmd == BIO_READ ? ISADMA_READ : ISADMA_WRITE, 
	    bp->bio_data, bp->bio_bcount)) {
		wtwait (t, 0, (bp->bio_cmd == BIO_READ) ? "wtread" : "wtwrite");
		bp->bio_resid -= t->dmacount;
	}
	splx (s);

	if (t->flags & TPEXCEP) {
errxit:		bp->bio_error = EIO;
err2xit:	bp->bio_flags |= BIO_ERROR;
	}
xit:    biodone (bp);
	return;
}

/*
 * Interrupt routine.
 */
static void
wtintr (int u)
{
	wtinfo_t *t = wttab + u;
	unsigned char s;

	if (u >= NWT || t->type == UNKNOWN) {
		TRACE (("wtintr() -- device not configured\n"));
		return;
	}

	s = inb (t->STATPORT);                  /* get status */
	TRACE (("wtintr() status=0x%x -- ", s));
	if ((s & (t->BUSY | t->NOEXCEP)) == (t->BUSY | t->NOEXCEP)) {
		TRACE (("busy\n"));
		return;                         /* device is busy */
	}

	/*
	 * Check if rewind finished.
	 */
	if (t->flags & TPREW) {
		TRACE (((s & (t->BUSY | t->NOEXCEP)) == (t->BUSY | t->NOEXCEP) ?
			"rewind busy?\n" : "rewind finished\n"));
		t->flags &= ~TPREW;             /* Rewind finished. */
		wtsense (t, 1, TP_WRP);
		wakeup (t);
		return;
	}

	/*
	 * Check if writing/reading of file mark finished.
	 */
	if (t->flags & (TPRMARK | TPWMARK)) {
		TRACE (((s & (t->BUSY | t->NOEXCEP)) == (t->BUSY | t->NOEXCEP) ?
			"marker r/w busy?\n" : "marker r/w finished\n"));
		if (! (s & t->NOEXCEP))         /* operation failed */
			wtsense (t, 1, (t->flags & TPRMARK) ? TP_WRP : 0);
		t->flags &= ~(TPRMARK | TPWMARK); /* operation finished */
		wakeup (t);
		return;
	}

	/*
	 * Do we started any i/o?  If no, just return.
	 */
	if (! (t->flags & TPACTIVE)) {
		TRACE (("unexpected interrupt\n"));
		return;
	}

	/*
	 * Clean up dma.
	 */
	if ((t->dmaflags & ISADMA_READ) && (t->dmatotal - t->dmacount) < t->bsize) {
		/* If reading short block, copy the internal buffer
		 * to the user memory. */
		isa_dmadone (t->dmaflags, t->buf, t->bsize, t->chan);
		bcopy (t->buf, t->dmavaddr, t->dmatotal - t->dmacount);
	} else
		isa_dmadone (t->dmaflags, t->dmavaddr, t->bsize, t->chan);

	t->flags &= ~TPACTIVE;
	t->dmacount += t->bsize;
	t->dmavaddr = (char *)t->dmavaddr + t->bsize;

	/*
	 * On exception, check for end of file and end of volume.
	 */
	if (! (s & t->NOEXCEP)) {
		TRACE (("i/o exception\n"));
		wtsense (t, 1, (t->dmaflags & ISADMA_READ) ? TP_WRP : 0);
		if (t->error.err & (TP_EOM | TP_FIL))
			t->flags |= TPVOL;      /* end of file */
		else
			t->flags |= TPEXCEP;    /* i/o error */
		wakeup (t);
		return;
	}

	if (t->dmacount < t->dmatotal) {        /* continue i/o */
		wtdma (t);
		TRACE (("continue i/o, %d\n", t->dmacount));
		return;
	}
	if (t->dmacount > t->dmatotal)          /* short last block */
		t->dmacount = t->dmatotal;
	wakeup (t);	/* wake up user level */
	TRACE (("i/o finished, %d\n", t->dmacount));
}

/* start the rewind operation */
static void
wtrewind (wtinfo_t *t)
{
	int rwmode = (t->flags & (TPRO | TPWO));

	t->flags &= ~(TPRO | TPWO | TPVOL);
	/*
	 * Wangtek strictly follows QIC-02 standard:
	 * clearing ONLINE in read/write modes causes rewind.
	 * REWIND command is not allowed in read/write mode
	 * and gives `illegal command' error.
	 */
	if (t->type==WANGTEK && rwmode) {
		outb (t->CTLPORT, 0);
	} else if (! wtcmd (t, QIC_REWIND))
		return;
	t->flags |= TPSTART | TPREW;
	wtclock (t);
}

/* start the `read marker' operation */
static int
wtreadfm (wtinfo_t *t)
{
	t->flags &= ~(TPRO | TPWO | TPVOL);
	if (! wtcmd (t, QIC_READFM)) {
		wtsense (t, 1, TP_WRP);
		return (EIO);
	}
	t->flags |= TPRMARK | TPRANY;
	wtclock (t);
	/* Don't wait for completion here. */
	return (0);
}

/* write marker to the tape */
static int
wtwritefm (wtinfo_t *t)
{
	tsleep (wtwritefm, WTPRI, "wtwfm", hz); /* timeout: 1 second */
	t->flags &= ~(TPRO | TPWO);
	if (! wtcmd (t, QIC_WRITEFM)) {
		wtsense (t, 1, 0);
		return (EIO);
	}
	t->flags |= TPWMARK | TPWANY;
	wtclock (t);
	return (wtwait (t, 0, "wtwfm"));
}

/* while controller status & mask == bits continue waiting */
static int
wtpoll (wtinfo_t *t, int mask, int bits)
{
	int s, i;

	/* Poll status port, waiting for specified bits. */
	for (i=0; i<1000; ++i) {                        /* up to 1 msec */
		s = inb (t->STATPORT);
		if ((s & mask) != bits)
			return (s);
		DELAY (1);
	}
	for (i=0; i<100; ++i) {                         /* up to 10 msec */
		s = inb (t->STATPORT);
		if ((s & mask) != bits)
			return (s);
		DELAY (100);
	}
	for (;;) {                                      /* forever */
		s = inb (t->STATPORT);
		if ((s & mask) != bits)
			return (s);
		tsleep (wtpoll, WTPRI, "wtpoll", 1); /* timeout: 1 tick */
	}
}

/* execute QIC command */
static int
wtcmd (wtinfo_t *t, int cmd)
{
	int s, x;

	TRACE (("wtcmd() cmd=0x%x\n", cmd));
	x = splbio();
	s = wtpoll (t, t->BUSY | t->NOEXCEP, t->BUSY | t->NOEXCEP); /* ready? */
	if (! (s & t->NOEXCEP)) {                       /* error */
	        splx(x);
		return (0);
	}

	outb (t->CMDPORT, cmd);                         /* output the command */

	outb (t->CTLPORT, t->REQUEST | t->ONLINE);      /* set request */
	wtpoll (t, t->BUSY, t->BUSY);                   /* wait for ready */
	outb (t->CTLPORT, t->IEN | t->ONLINE);          /* reset request */
	wtpoll (t, t->BUSY, 0);                         /* wait for not ready */
	splx(x);
	return (1);
}

/* wait for the end of i/o, seeking marker or rewind operation */
static int
wtwait (wtinfo_t *t, int catch, char *msg)
{
	int error;

	TRACE (("wtwait() `%s'\n", msg));
	while (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK)) {
		error = tsleep (t, WTPRI | catch, msg, 0);
		if (error)
			return (error);
	}
	return (0);
}

/* initialize dma for the i/o operation */
static void
wtdma (wtinfo_t *t)
{
	t->flags |= TPACTIVE;
	wtclock (t);

	if (t->type == ARCHIVE)
		outb (t->SDMAPORT, 0);          /* set dma */

	if ((t->dmaflags & ISADMA_READ) && (t->dmatotal - t->dmacount) < t->bsize)
		/* Reading short block.  Do it through the internal buffer. */
		isa_dmastart (t->dmaflags, t->buf, t->bsize, t->chan);
	else
		isa_dmastart (t->dmaflags, t->dmavaddr, t->bsize, t->chan);
}

/* start i/o operation */
static int
wtstart (wtinfo_t *t, unsigned flags, void *vaddr, unsigned len)
{
	int s, x;

	TRACE (("wtstart()\n"));
	x = splbio();
	s = wtpoll (t, t->BUSY | t->NOEXCEP, t->BUSY | t->NOEXCEP); /* ready? */
	if (! (s & t->NOEXCEP)) {
		t->flags |= TPEXCEP;            /* error */
		splx(x);
		return (0);
	}
	t->flags &= ~TPEXCEP;                   /* clear exception flag */
	t->dmavaddr = vaddr;
	t->dmatotal = len;
	t->dmacount = 0;
	t->dmaflags = flags;
	wtdma (t);
	splx(x);
	return (1);
}

/* start timer */
static void
wtclock (wtinfo_t *t)
{
	if (! (t->flags & TPTIMER)) {
		t->flags |= TPTIMER;
		/* Some controllers seem to lose dma interrupts too often.
		 * To make the tape stream we need 1 tick timeout. */
		timeout (wtimer, (caddr_t)t, (t->flags & TPACTIVE) ? 1 : hz);
	}
}

/*
 * Simulate an interrupt periodically while i/o is going.
 * This is necessary in case interrupts get eaten due to
 * multiple devices on a single IRQ line.
 */
static void
wtimer (void *xt)
{
	wtinfo_t *t = (wtinfo_t *)xt;
	int s;

	t->flags &= ~TPTIMER;
	if (! (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK)))
		return;

	/* If i/o going, simulate interrupt. */
	s = splbio ();
	if ((inb (t->STATPORT) & (t->BUSY | t->NOEXCEP)) != (t->BUSY | t->NOEXCEP)) {
		TRACE (("wtimer() -- "));
		wtintr (t->unit);
	}
	splx (s);

	/* Restart timer if i/o pending. */
	if (t->flags & (TPACTIVE | TPREW | TPRMARK | TPWMARK))
		wtclock (t);
}

/* reset the controller */
static int
wtreset (wtinfo_t *t)
{
	/* Perform QIC-02 and QIC-36 compatible reset sequence. */
	/* Thanks to Mikael Hybsch <micke@dynas.se>. */
	int s, i;

	outb (t->CTLPORT, t->RESET | t->ONLINE); /* send reset */
	DELAY (30);
	outb (t->CTLPORT, t->ONLINE);           /* turn off reset */
	DELAY (30);

	/* Read the controller status. */
	s = inb (t->STATPORT);
	if (s == 0xff)                          /* no port at this address? */
		return (0);

	/* Wait 3 sec for reset to complete. Needed for QIC-36 boards? */
	for (i=0; i<3000; ++i) {
		if (! (s & t->BUSY) || ! (s & t->NOEXCEP))
			break;
		DELAY (1000);
		s = inb (t->STATPORT);
	}
	return ((s & t->RESETMASK) == t->RESETVAL);
}

/* get controller status information */
/* return 0 if user i/o request should receive an i/o error code */
static int
wtsense (wtinfo_t *t, int verb, int ignor)
{
	char *msg = 0;
	int err;

	TRACE (("wtsense() ignor=0x%x\n", ignor));
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
	if (! verb)
		return (0);

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
static int
wtstatus (wtinfo_t *t)
{
	char *p;
	int x;

	x = splbio();
	wtpoll (t, t->BUSY | t->NOEXCEP, t->BUSY | t->NOEXCEP); /* ready? */
	outb (t->CMDPORT, QIC_RDSTAT);  /* send `read status' command */

	outb (t->CTLPORT, t->REQUEST | t->ONLINE);      /* set request */
	wtpoll (t, t->BUSY, t->BUSY);                   /* wait for ready */
	outb (t->CTLPORT, t->ONLINE);                   /* reset request */
	wtpoll (t, t->BUSY, 0);                         /* wait for not ready */

	p = (char*) &t->error;
	while (p < (char*)&t->error + 6) {
		int s = wtpoll (t, t->BUSY | t->NOEXCEP, t->BUSY | t->NOEXCEP);
		if (! (s & t->NOEXCEP)) {               /* error */
		        splx(x);
			return (0);
		}

		*p++ = inb (t->DATAPORT);               /* read status byte */

		outb (t->CTLPORT, t->REQUEST | t->ONLINE); /* set request */
		wtpoll (t, t->BUSY, 0);                 /* wait for not ready */
		DELAY(20);
		outb (t->CTLPORT, t->ONLINE);           /* unset request */
	}
	splx(x);
	return (1);
}
