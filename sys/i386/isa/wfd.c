/*
 * Copyright (c) 1997,1998  Junichi Satoh <junichi@astec.co.jp>
 *   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Junichi Satoh ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Junichi Satoh BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *      $Id: wfd.c,v 1.1.2.1 1998/01/16 22:42:17 pst Exp $
 */

/*
 * ATAPI Floppy, LS-120 driver
 */

#include "wdc.h"
#include "wfd.h"
#include "opt_atapi.h"

#if NWFD > 0 && NWDC > 0 && defined (ATAPI)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/cdio.h>
#include <machine/ioctl_fd.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <i386/isa/atapi.h>
#include <i386/isa/fdc.h>

static	d_open_t	wfdbopen;
static	d_close_t	wfdbclose;
static	d_ioctl_t	wfdioctl;
static	d_strategy_t	wfdstrategy;

#define CDEV_MAJOR 87
#define BDEV_MAJOR 24
static struct cdevsw wfd_cdevsw;
static struct bdevsw wfd_bdevsw = 
	{ wfdbopen,	wfdbclose,	wfdstrategy,	wfdioctl,
	  nodump,	nopsize,	0,	"wfd",	&wfd_cdevsw,	-1 };

#ifndef ATAPI_STATIC
static
#endif
int  wfdattach(struct atapi*, int, struct atapi_params*, int);

#define NUNIT   (NWDC*2)                /* Max. number of devices */
#define UNIT(d) ((minor(d) >> 3) & 3)   /* Unit part of minor device number */

#define F_BOPEN         0x0001          /* The block device is opened */
#define F_MEDIA_CHANGED 0x0002          /* The media have changed since open */
#define F_DEBUG         0x0004          /* Print debug info */

/*
 * LS-120 Capabilities and Mechanical Status Page
 */
struct cappage {
    /* Mode data header */
    u_short	data_length;
    u_char	medium_type;
#define MDT_UNKNOWN     0x00
#define MDT_NO_DISC     0x70
#define MDT_DOOR_OPEN   0x71
#define MDT_FMT_ERROR   0x72

#define MDT_2DD_UN	0x10
#define MDT_2DD		0x11
#define MDT_2HD_UN	0x20
#define MDT_2HD_12_98	0x22
#define MDT_2HD_12	0x23
#define MDT_2HD_144	0x24
#define MDT_LS120	0x31

    unsigned        reserved0       :7;
    unsigned        wp              :1;     /* Write protect */
    u_char          reserved1[4];

    /* Capabilities page */
    unsigned        page_code       :6;     /* Page code - Should be 0x5 */
#define CAP_PAGE        0x05
    unsigned        reserved1_6     :1;     /* Reserved */
    unsigned        ps              :1;     /* The device is capable of savi
ng the page */
    u_char          page_length;            /* Page Length - Should be 0x1e */
    u_short         transfer_rate;          /* In kilobits per second */
    u_char          heads, sectors;         /* Number of heads, Number of sectors per track */
    u_short         sector_size;            /* Byes per sector */
    u_short         cyls;                   /* Number of cylinders */
    u_char          reserved10[10];
    u_char          motor_delay;            /* Motor off delay */
    u_char          reserved21[7];
    u_short         rpm;                    /* Rotations per minute */
    u_char          reserved30[2];
};

/* misuse a flag to identify format operation */
#define B_FORMAT B_XXX

struct wfd {
	struct atapi *ata;              /* Controller structure */
	int unit;                       /* IDE bus drive unit */
	int lun;                        /* Logical device unit */
	int flags;                      /* Device state flags */
	int refcnt;                     /* The number of raw opens */
	struct buf_queue_head buf_queue;  /* Queue of i/o requests */
	struct atapi_params *param;     /* Drive parameters table */
	struct cappage cap;             /* Capabilities page info */
	char description[80];           /* Device description */
#ifdef	DEVFS
	void	*cdevs;
	void	*bdevs;
#endif
	struct diskslices *dk_slices;	/* virtual drives */
};

struct wfd *wfdtab[NUNIT];      /* Drive info by unit number */
static int wfdnlun = 0;         /* Number of configured drives */

static void wfd_start (struct wfd *t);
static void wfd_done (struct wfd *t, struct buf *bp, int resid,
	struct atapires result);
static void wfd_error (struct wfd *t, struct atapires result);
static int wfd_request_wait (struct wfd *t, u_char cmd, u_char a1, u_char a2,
	u_char a3, u_char a4, u_char a5, u_char a6, u_char a7, u_char a8,
	u_char a9, char *addr, int count);
static void wfd_describe (struct wfd *t);
static int wfd_eject (struct wfd *t, int closeit);
static void wfdstrategy1(struct buf *bp);

/*
 * Dump the array in hexadecimal format for debugging purposes.
 */
static void wfd_dump (int lun, char *label, void *data, int len)
{
	u_char *p = data;

	printf ("wfd%d: %s %x", lun, label, *p++);
	while (--len > 0)
		printf ("-%x", *p++);
	printf ("\n");
}

#ifndef ATAPI_STATIC
static
#endif
int 
wfdattach (struct atapi *ata, int unit, struct atapi_params *ap, int debug)
{
	struct wfd *t;
	struct atapires result;
	int lun, i;

#ifdef DEVFS
	int	mynor;
#endif
	if (wfdnlun >= NUNIT) {
		printf ("wfd: too many units\n");
		return (0);
	}
	if (!atapi_request_immediate) {
		printf("wfd: configuration error, ATAPI core code not present!\n");
		printf("wfd: check `options ATAPI_STATIC' in your kernel config file!\n");
		return (0);
	}
	t = malloc (sizeof (struct wfd), M_TEMP, M_NOWAIT);
	if (! t) {
		printf ("wfd: out of memory\n");
		return (0);
	}
	wfdtab[wfdnlun] = t;
	bzero (t, sizeof (struct wfd));
	t->ata = ata;
	t->unit = unit;
	lun = t->lun = wfdnlun++;
	t->param = ap;
	t->flags = F_MEDIA_CHANGED;
	t->refcnt = 0;
	if (debug) {
		t->flags |= F_DEBUG;
		/* Print params. */
		wfd_dump (t->lun, "info", ap, sizeof *ap);
	}

	/* Get drive capabilities. */
	/* Do it twice to avoid the stale media changed state. */
	for (i = 0; i < 2; i++) {
		result = atapi_request_immediate (ata, unit, ATAPI_MODE_SENSE,
			0, CAP_PAGE, 0, 0, 0, 0, 
			sizeof (t->cap) >> 8, sizeof (t->cap),
			0, 0, 0, 0, 0, 0, 0, (char*) &t->cap, sizeof (t->cap));
	}

	if (result.code == RES_ERR &&
	    (result.error & AER_SKEY) == AER_SK_UNIT_ATTENTION)
		result = atapi_request_immediate (ata, unit, ATAPI_MODE_SENSE,
			0, CAP_PAGE, 0, 0, 0, 0, sizeof (t->cap) >> 8,
			sizeof (t->cap), 0, 0, 0, 0, 0, 0, 0,
			(char*) &t->cap, sizeof (t->cap));

	/* Some drives have shorter capabilities page. */
	if (result.code == RES_UNDERRUN)
		result.code = 0;

	if (result.code == 0) {
		wfd_describe (t);
		if (t->flags & F_DEBUG)
			wfd_dump (t->lun, "cap", &t->cap, sizeof t->cap);
	} else
		return -1;

#ifdef DEVFS
	mynor = dkmakeminor(unit, WHOLE_DISK_SLICE, RAW_PART);
	t->bdevs = devfs_add_devswf(&wfd_bdevsw, mynor, 
				    DV_BLK, UID_ROOT, GID_OPERATOR, 0640,
				    "wfd%d", unit);
	t->cdevs = devfs_add_devswf(&wfd_cdevsw, mynor, 
				    DV_CHR, UID_ROOT, GID_OPERATOR, 0640,
				    "rwfd%d", unit);
#endif /* DEVFS */
	return (1);
}

void wfd_describe (struct wfd *t)
{
	int no_print = 0;

	t->cap.cyls = ntohs (t->cap.cyls);
	t->cap.sector_size = ntohs (t->cap.sector_size);

	printf ("wfd%d: ", t->lun);
	switch (t->cap.medium_type) {
	case MDT_UNKNOWN:
		printf ("medium type unknown (no disk)");
		no_print = 1;
		break;
	case MDT_2DD_UN:
		printf ("2DD(capacity unknown) floppy disk loaded");
		no_print = 1;
		break;
	case MDT_2DD:
		printf ("720KB floppy disk loaded");
		break;
	case MDT_2HD_UN:
		printf ("2HD(capacity unknown) floppy disk loaded");
		no_print = 1;
		break;
	case MDT_2HD_12_98:
		printf ("1.25MB(PC-9801 format) floppy disk loaded");
		break;
	case MDT_2HD_12:
		printf ("1.2MB floppy disk loaded");
		break;
	case MDT_2HD_144:
		printf ("1.44MB floppy disk loaded");
		break;
	case MDT_LS120:
		printf ("120MB floppy disk loaded");
		break;
	case MDT_NO_DISC:
		printf ("no disc inside");
		no_print = 1;
		break;
	case MDT_DOOR_OPEN:
		printf ("door open");
		no_print = 1;
		break;
	case MDT_FMT_ERROR:
		printf ("medium format error");
		no_print = 1;
		break;
	default:
		printf ("medium type=0x%x", t->cap.medium_type);
		break;
	}
	if (t->cap.wp)
		printf(", write protected");
	printf ("\n");

	if (!no_print) {
		printf ("wfd%d: ", t->lun);
		printf ("%lu cyls", t->cap.cyls);
		printf (", %lu heads, %lu S/T", t->cap.heads, t->cap.sectors);
		printf (", %lu B/S", t->cap.sector_size);
		printf ("\n");
	}
}

int wfdbopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	int lun = UNIT(dev);
	struct wfd *t;
	struct atapires result;
	int errcode = 0;
	struct disklabel label;

	/* Check that the device number is legal
	 * and the ATAPI driver is loaded. */
	if (lun >= wfdnlun || ! atapi_request_immediate) {
		printf("ENXIO lun=%d, wfdnlun=%d, im=%d\n", lun, wfdnlun, atapi_request_immediate);
		return (ENXIO);
	}
	t = wfdtab[lun];

	t->flags &= ~F_MEDIA_CHANGED;
	/* Lock the media. */
	wfd_request_wait (t, ATAPI_PREVENT_ALLOW,
			  0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

	/* Sense the media type */
	result = atapi_request_wait (t->ata, t->unit, ATAPI_MODE_SENSE,
				     0, CAP_PAGE, 0, 0, 0, 0, 
				     sizeof (t->cap) >> 8, sizeof (t->cap),
				     0, 0, 0, 0, 0, 0, 0, 
				     (char*) &t->cap, sizeof (t->cap));
	if (result.code)
		printf ("wfd%d: Sense the media type is failed.\n", t->lun);
	else {
		t->cap.cyls = ntohs (t->cap.cyls);
		t->cap.sector_size = ntohs (t->cap.sector_size);
	}

	/* Build label for whole disk. */
	bzero(&label, sizeof label);
	label.d_secsize = t->cap.sector_size;
	label.d_nsectors = t->cap.sectors;
	label.d_ntracks = t->cap.heads;
	label.d_ncylinders = t->cap.cyls;
	label.d_secpercyl = t->cap.heads * t->cap.sectors;
	label.d_rpm = 720;
	label.d_secperunit = label.d_secpercyl * t->cap.cyls;

	/* Initialize slice tables. */
	errcode = dsopen("wfd", dev, fmt, &t->dk_slices, &label, wfdstrategy1,
			 (ds_setgeom_t *)NULL, &wfd_bdevsw, &wfd_cdevsw);
	if (errcode != 0)
		return errcode;

	t->flags |= F_BOPEN;
	return (0);
}

/*
 * Close the device.  Only called if we are the LAST
 * occurence of an open device.
 */
int wfdbclose (dev_t dev, int flags, int fmt, struct proc *p)
{
	int lun = UNIT(dev);
	struct wfd *t = wfdtab[lun];

	dsclose(dev, fmt, t->dk_slices);
	if(!dsisopen(t->dk_slices)) {
		/* If we were the last open of the entire device, release it. */
		if (! t->refcnt)
			wfd_request_wait (t, ATAPI_PREVENT_ALLOW,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		t->flags &= ~F_BOPEN;
	}
	return (0);
}

static void
wfdstrategy1(struct buf *bp)
{
	/*
	 * XXX - do something to make wdstrategy() but not this block while
	 * we're doing dsinit() and dsioctl().
	 */
	wfdstrategy(bp);
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand. The transfer is described by a buf and will include only one
 * physical transfer.
 */
void wfdstrategy (struct buf *bp)
{
	int lun = UNIT(bp->b_dev);
	struct wfd *t = wfdtab[lun];
	int x;

	/* If it's a null transfer, return immediatly. */
	if (bp->b_bcount == 0) {
		bp->b_resid = 0;
		biodone (bp);
		return;
	}

	/*
	 * Do bounds checking, adjust transfer, and set b_pblkno.
         */
	if (dscheck(bp, t->dk_slices) <= 0) {
		biodone(bp);
		return;
	}

	x = splbio();

	/* Place it in the queue of disk activities for this disk. */
	tqdisksort (&t->buf_queue, bp);

	/* Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion. */
	wfd_start (t);
	splx(x);
}

/*
 * Look to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates an ATAPI command to perform the
 * transfer in the buf.
 * The bufs are queued by the strategy routine (wfdstrategy).
 * Must be called at the correct (splbio) level.
 */
static void wfd_start (struct wfd *t)
{
	struct buf *bp = TAILQ_FIRST(&t->buf_queue);
	u_long blkno, nblk;
	u_char op_code;
	long count;

	/* See if there is a buf to do and we are not already doing one. */
	if (! bp)
		return;

	/* Unqueue the request. */
	TAILQ_REMOVE(&t->buf_queue, bp, b_act);

	/* We have a buf, now we should make a command
	 * First, translate the block to absolute and put it in terms of the
	 * logical blocksize of the device. */
	blkno = bp->b_pblkno / (t->cap.sector_size / 512);
	nblk = (bp->b_bcount + (t->cap.sector_size - 1)) / t->cap.sector_size;

	if(bp->b_flags & B_READ) {
		op_code = ATAPI_READ_BIG;
		count = bp->b_bcount;
	} else {
		op_code = ATAPI_WRITE_BIG;
		count = -bp->b_bcount;
	}

	atapi_request_callback (t->ata, t->unit, op_code, 0,
		blkno>>24, blkno>>16, blkno>>8, blkno, 0, nblk>>8, nblk, 0, 0,
		0, 0, 0, 0, 0, (u_char*) bp->b_un.b_addr, count,
		(void*)wfd_done, t, bp);
}

static void wfd_done (struct wfd *t, struct buf *bp, int resid,
	struct atapires result)
{
	if (result.code) {
		wfd_error (t, result);
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
	} else
		bp->b_resid = resid;
	biodone (bp);
	wfd_start (t);
}

static void wfd_error (struct wfd *t, struct atapires result)
{
	if (result.code != RES_ERR)
		return;
	switch (result.error & AER_SKEY) {
	case AER_SK_NOT_READY:
		if (result.error & ~AER_SKEY) {
			/* Not Ready */
			printf ("wfd%d: not ready\n", t->lun);
			return;
		}
		/* Tray open. */
		if (! (t->flags & F_MEDIA_CHANGED))
			printf ("wfd%d: tray open\n", t->lun);
		t->flags |= F_MEDIA_CHANGED;
		return;

	case AER_SK_UNIT_ATTENTION:
		/* Media changed. */
		if (! (t->flags & F_MEDIA_CHANGED))
			printf ("wfd%d: media changed\n", t->lun);
		t->flags |= F_MEDIA_CHANGED;
		return;

	case AER_SK_ILLEGAL_REQUEST:
		/* Unknown command or invalid command arguments. */
		if (t->flags & F_DEBUG)
			printf ("wfd%d: invalid command\n", t->lun);
		return;
	}
	printf ("wfd%d: i/o error, status=%b, error=%b\n", t->lun,
		result.status, ARS_BITS, result.error, AER_BITS);
}

static int wfd_request_wait (struct wfd *t, u_char cmd, u_char a1, u_char a2,
	u_char a3, u_char a4, u_char a5, u_char a6, u_char a7, u_char a8,
	u_char a9, char *addr, int count)
{
	struct atapires result;

	result = atapi_request_wait (t->ata, t->unit, cmd,
		a1, a2, a3, a4, a5, a6, a7, a8, a9, 0, 0, 0, 0, 0, 0,
		addr, count);
	if (result.code) {
		wfd_error (t, result);
		return (EIO);
	}
	return (0);
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
int wfdioctl (dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	int lun = UNIT(dev);
	struct wfd *t = wfdtab[lun];
	int error = 0;

	struct disklabel *dl;
	char buffer[DEV_BSIZE];

	error = dsioctl("wfd", dev, cmd, addr, flag, &t->dk_slices,
			wfdstrategy1, (ds_setgeom_t *)NULL);
	if (error != -1)
		return (error);

	if (t->flags & F_MEDIA_CHANGED)
		switch (cmd) {
		case CDIOCSETDEBUG:
		case CDIOCCLRDEBUG:
		case CDIOCRESET:
			/* These ops are media change transparent. */
			break;
		default:
			/* Lock the media. */
			wfd_request_wait (t, ATAPI_PREVENT_ALLOW,
				0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
			break;
		}
	switch (cmd) {
	case CDIOCSETDEBUG:
		if (p->p_cred->pc_ucred->cr_uid)
			return (EPERM);
		t->flags |= F_DEBUG;
		atapi_debug (t->ata, 1);
		return 0;
	case CDIOCCLRDEBUG:
		if (p->p_cred->pc_ucred->cr_uid)
			return (EPERM);
		t->flags &= ~F_DEBUG;
		atapi_debug (t->ata, 0);
		return 0;
	case CDIOCRESET:
		if (p->p_cred->pc_ucred->cr_uid)
			return (EPERM);
		return wfd_request_wait (t, ATAPI_TEST_UNIT_READY,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	case CDIOCEJECT:
		/* Don't allow eject if the device is opened
		 * by somebody (not us) in block mode. */
		if ((t->flags & F_BOPEN) && t->refcnt)
			return (EBUSY);
		return wfd_eject (t, 0);
	case CDIOCCLOSE:
		if ((t->flags & F_BOPEN) && t->refcnt)
			return (0);
		return wfd_eject (t, 1);
	default:
		return (ENOTTY);
	}
	return (error);
}

static int wfd_eject (struct wfd *t, int closeit)
{
	struct atapires result;

	/* Try to stop the disc. */
	result = atapi_request_wait (t->ata, t->unit, ATAPI_START_STOP,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	if (result.code == RES_ERR &&
	    ((result.error & AER_SKEY) == AER_SK_NOT_READY ||
	    (result.error & AER_SKEY) == AER_SK_UNIT_ATTENTION)) {
		int err;

		if (!closeit)
			return (0);
		/*
		 * The disc was unloaded.
		 * Load it (close tray).
		 * Read the table of contents.
		 */
		err = wfd_request_wait (t, ATAPI_START_STOP,
			0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0);
		if (err)
			return (err);

		/* Lock the media. */
		wfd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

		return (0);
	}

	if (result.code) {
		wfd_error (t, result);
		return (EIO);
	}

	if (closeit)
		return (0);

	/* Give it some time to stop spinning. */
	tsleep ((caddr_t)&lbolt, PRIBIO, "wfdej1", 0);
	tsleep ((caddr_t)&lbolt, PRIBIO, "wfdej2", 0);

	/* Unlock. */
	wfd_request_wait (t, ATAPI_PREVENT_ALLOW,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	/* Eject. */
	t->flags |= F_MEDIA_CHANGED;
	return wfd_request_wait (t, ATAPI_START_STOP,
		0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0);
}

#ifdef WFD_MODULE
/*
 * Loadable ATAPI Floppy driver stubs.
 */
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Construct lkm_dev structures (see lkm.h).
 * Our bdevsw/cdevsw slot numbers are 19/69.
 */


MOD_DEV(wfd, LM_DT_BLOCK, BDEV_MAJOR, &wfd_bdevsw);
MOD_DEV(rwfd, LM_DT_CHAR, CDEV_MAJOR, &wfd_cdevsw);

/*
 * Function called when loading the driver.
 */
int wfd_load (struct lkm_table *lkmtp, int cmd)
{
	struct atapi *ata;
	int n, u;

	if (! atapi_start)
		/* No ATAPI driver available. */
		return EPROTONOSUPPORT;
	n = 0;
	for (ata=atapi_tab; ata<atapi_tab+2; ++ata)
		if (ata->port)
			for (u=0; u<2; ++u)
				/* Probing controller ata->ctrlr, unit u. */
				if (ata->params[u] && ! ata->attached[u] &&
				    wfdattach (ata, u, ata->params[u],
				    ata->debug) >= 0)
				{
					/* Drive found. */
					ata->attached[u] = 1;
					++n;
				}
	if (! n)
		/* No IDE Floppies found. */
		return ENXIO;
	return 0;
}

/*
 * Function called when unloading the driver.
 */
int wfd_unload (struct lkm_table *lkmtp, int cmd)
{
	struct wfd **t;

	for (t=wfdtab; t<wfdtab+wfdnlun; ++t)
		if (((*t)->flags & F_BOPEN) || (*t)->refcnt)
			/* The device is opened, cannot unload the driver. */
			return EBUSY;
	for (t=wfdtab; t<wfdtab+wfdnlun; ++t) {
		(*t)->ata->attached[(*t)->unit] = 0;
		free (*t, M_TEMP);
	}
	wfdnlun = 0;
	bzero (wfdtab, sizeof(wfdtab));
	return 0;
}

/*
 * Dispatcher function for the module (load/unload/stat).
 */
int wfd_mod (struct lkm_table *lkmtp, int cmd, int ver)
{
	int err = 0;

	if (ver != LKM_VERSION)
		return EINVAL;

	if (cmd == LKM_E_LOAD)
		err = wfd_load (lkmtp, cmd);
	else if (cmd == LKM_E_UNLOAD)
		err = wfd_unload (lkmtp, cmd);
	if (err)
		return err;

	/* XXX Poking around in the LKM internals like this is bad.
	 */
	/* Register the cdevsw entry. */
	lkmtp->private.lkm_dev = & MOD_PRIVATE(rwfd);
	err = lkmdispatch (lkmtp, cmd);
	if (err)
		return err;

	/* Register the bdevsw entry. */
	lkmtp->private.lkm_dev = & MOD_PRIVATE(wfd);
	return lkmdispatch (lkmtp, cmd);
}
#endif /* WFD_MODULE */

static wfd_devsw_installed = 0;

static void 	wfd_drvinit(void *unused)
{
	if( ! wfd_devsw_installed ) {
		bdevsw_add_generic(BDEV_MAJOR, CDEV_MAJOR, &wfd_bdevsw);
		wfd_devsw_installed = 1;
    	}
}

SYSINIT(wfddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,wfd_drvinit,NULL)


#endif /* NWFD && NWDC && ATAPI */
