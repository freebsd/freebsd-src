/*
 * IDE CD-ROM driver for FreeBSD.
 * Supports ATAPI-compatible drives.
 *
 * Copyright (C) 1995 Cronyx Ltd.
 * Author Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Mon Oct  9 20:27:42 MSK 1995
 */

#include "wdc.h"
#include "wcd.h"
#if NWCD > 0 && NWDC > 0 && defined (ATAPI)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/devconf.h>
#include <sys/disklabel.h>
#include <sys/cdio.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/cpufunc.h>

#include <i386/isa/atapi.h>

static	d_open_t	wcdropen;
static	d_open_t	wcdbopen;
static	d_close_t	wcdrclose;
static	d_close_t	wcdbclose;
static	d_ioctl_t	wcdioctl;
static	d_strategy_t	wcdstrategy;

#define CDEV_MAJOR 69
#define BDEV_MAJOR 19
extern	struct cdevsw wcd_cdevsw;
static struct bdevsw wcd_bdevsw = 
	{ wcdbopen,	wcdbclose,	wcdstrategy,	wcdioctl,	/*19*/
	  nodump,	nopsize,	0,	"wcd",	&wcd_cdevsw,	-1 };

static struct cdevsw wcd_cdevsw = 
	{ wcdropen,	wcdrclose,	rawread,	nowrite,	/*69*/
	  wcdioctl,	nostop,		nullreset,	nodevtotty,/* atapi */
	  seltrue,	nommap,		wcdstrategy,	"wcd",
	  &wcd_bdevsw,	-1 };

#ifndef ATAPI_STATIC
static
#endif
int  wcdattach(struct atapi*, int, struct atapi_params*, int, struct kern_devconf*);

#define NUNIT   (NWDC*2)                /* Max. number of devices */
#define UNIT(d) ((minor(d) >> 3) & 3)   /* Unit part of minor device number */
#define SECSIZE 2048                    /* CD-ROM sector size in bytes */

#define F_BOPEN         0x0001          /* The block device is opened */
#define F_MEDIA_CHANGED 0x0002          /* The media have changed since open */
#define F_DEBUG         0x0004          /* Print debug info */

/*
 * Disc table of contents.
 */
#define MAXTRK 99
struct toc {
	struct ioc_toc_header hdr;
	struct cd_toc_entry tab[MAXTRK+1];      /* One extra for the leadout */
};

/*
 * Volume size info.
 */
struct volinfo {
	u_long volsize;         /* Volume size in blocks */
	u_long blksize;         /* Block size in bytes */
} info;

/*
 * Current subchannel status.
 */
struct subchan {
	u_char void0;
	u_char audio_status;
	u_short data_length;
	u_char data_format;
	u_char control;
	u_char track;
	u_char indx;
	u_long abslba;
	u_long rellba;
};

/*
 * Audio Control Parameters Page
 */
struct audiopage {
	/* Mode data header */
	u_short data_length;
	u_char  medium_type;
	u_char  reserved1[5];

	/* Audio control page */
	u_char  page_code;
#define AUDIO_PAGE      0x0e
#define AUDIO_PAGE_MASK 0x4e            /* changeable values */
	u_char  param_len;
	u_char  flags;
#define CD_PA_SOTC      0x02            /* mandatory */
#define CD_PA_IMMED     0x04            /* always 1 */
	u_char  reserved3[3];
	u_short lb_per_sec;
	struct port_control {
		u_char  channels : 4;
#define CHANNEL_0       1               /* mandatory */
#define CHANNEL_1       2               /* mandatory */
#define CHANNEL_2       4               /* optional */
#define CHANNEL_3       8               /* optional */
		u_char  volume;
	} port[4];
};

/*
 * CD-ROM Capabilities and Mechanical Status Page
 */
struct cappage {
	/* Mode data header */
	u_short data_length;
	u_char  medium_type;
#define MDT_UNKNOWN     0x00
#define MDT_DATA_120    0x01
#define MDT_AUDIO_120   0x02
#define MDT_COMB_120    0x03
#define MDT_PHOTO_120   0x04
#define MDT_DATA_80     0x05
#define MDT_AUDIO_80    0x06
#define MDT_COMB_80     0x07
#define MDT_PHOTO_80    0x08
#define MDT_NO_DISC     0x70
#define MDT_DOOR_OPEN   0x71
#define MDT_FMT_ERROR   0x72
	u_char  reserved1[5];

	/* Capabilities page */
	u_char  page_code;
#define CAP_PAGE        0x2a
	u_char  param_len;
	u_char  reserved2[2];

	u_char  audio_play : 1;         /* audio play supported */
	u_char  composite : 1;          /* composite audio/video supported */
	u_char  dport1 : 1;             /* digital audio on port 1 */
	u_char  dport2 : 1;             /* digital audio on port 2 */
	u_char  mode2_form1 : 1;        /* mode 2 form 1 (XA) read */
	u_char  mode2_form2 : 1;        /* mode 2 form 2 format */
	u_char  multisession : 1;       /* multi-session photo-CD */
	u_char  : 1;
	u_char  cd_da : 1;              /* audio-CD read supported */
	u_char  cd_da_stream : 1;       /* CD-DA streaming */
	u_char  rw : 1;                 /* combined R-W subchannels */
	u_char  rw_corr : 1;            /* R-W subchannel data corrected */
	u_char  c2 : 1;                 /* C2 error pointers supported */
	u_char  isrc : 1;               /* can return the ISRC info */
	u_char  upc : 1;                /* can return the catalog number UPC */
	u_char  : 1;
	u_char  lock : 1;               /* could be locked */
	u_char  locked : 1;             /* current lock state */
	u_char  prevent : 1;            /* prevent jumper installed */
	u_char  eject : 1;              /* can eject */
	u_char  : 1;
	u_char  mech : 3;               /* loading mechanism type */
#define MECH_CADDY      0
#define MECH_TRAY       1
#define MECH_POPUP      2
#define MECH_CHANGER    4
#define MECH_CARTRIDGE  5
	u_char  sep_vol : 1;            /* independent volume of channels */
	u_char  sep_mute : 1;           /* independent mute of channels */
	u_char  : 6;

	u_short max_speed;              /* max raw data rate in bytes/1000 */
	u_short max_vol_levels;         /* number of discrete volume levels */
	u_short buf_size;               /* internal buffer size in bytes/1024 */
	u_short cur_speed;              /* current data rate in bytes/1000  */

	/* Digital drive output format description (optional?) */
	u_char  reserved3;
	u_char  bckf : 1;               /* data valid on failing edge of BCK */
	u_char  rch : 1;                /* high LRCK indicates left channel */
	u_char  lsbf : 1;               /* set if LSB first */
	u_char  dlen: 2;
#define DLEN_32         0               /* 32 BCKs */
#define DLEN_16         1               /* 16 BCKs */
#define DLEN_24         2               /* 24 BCKs */
#define DLEN_24_I2S     3               /* 24 BCKs (I2S) */
	u_char  : 3;
	u_char  reserved4[2];
};

struct wcd {
	struct atapi *ata;              /* Controller structure */
	int unit;                       /* IDE bus drive unit */
	int lun;                        /* Logical device unit */
	int flags;                      /* Device state flags */
	int refcnt;                     /* The number of raw opens */
	struct buf_queue_head buf_queue;               /* Queue of i/o requests */
	struct atapi_params *param;     /* Drive parameters table */
	struct toc toc;                 /* Table of disc contents */
	struct volinfo info;            /* Volume size info */
	struct audiopage au;            /* Audio page info */
	struct cappage cap;             /* Capabilities page info */
	struct audiopage aumask;        /* Audio page mask */
	struct subchan subchan;         /* Subchannel info */
	struct kern_devconf cf;         /* Driver configuration info */
	char description[80];           /* Device description */
#ifdef	DEVFS
	void	*ra_devfs_token;
	void	*rc_devfs_token;
	void	*a_devfs_token;
	void	*c_devfs_token;
#endif
};

struct wcd *wcdtab[NUNIT];      /* Drive info by unit number */
static int wcdnlun = 0;         /* Number of configured drives */

static void wcd_start (struct wcd *t);
static void wcd_done (struct wcd *t, struct buf *bp, int resid,
	struct atapires result);
static void wcd_error (struct wcd *t, struct atapires result);
static int wcd_read_toc (struct wcd *t);
static int wcd_request_wait (struct wcd *t, u_char cmd, u_char a1, u_char a2,
	u_char a3, u_char a4, u_char a5, u_char a6, u_char a7, u_char a8,
	u_char a9, char *addr, int count);
static int wcd_externalize (struct kern_devconf*, struct sysctl_req *);
static int wcd_goaway (struct kern_devconf *kdc, int force);
static void wcd_describe (struct wcd *t);
static int wcd_open(dev_t dev, int rawflag);
static int wcd_setchan (struct wcd *t,
	u_char c0, u_char c1, u_char c2, u_char c3);
static int wcd_eject (struct wcd *t);

static struct kern_devconf cftemplate = {
	0, 0, 0, "wcd", 0, { MDDT_DISK, 0 },
	wcd_externalize, 0, wcd_goaway, DISK_EXTERNALLEN,
	0, 0, DC_IDLE, "ATAPI compact disc",
};

/*
 * Dump the array in hexadecimal format for debugging purposes.
 */
static void wcd_dump (int lun, char *label, void *data, int len)
{
	u_char *p = data;

	printf ("wcd%d: %s %x", lun, label, *p++);
	while (--len > 0)
		printf ("-%x", *p++);
	printf ("\n");
}

static int wcd_externalize (struct kern_devconf *kdc, struct sysctl_req *req)
{
	return disk_externalize (wcdtab[kdc->kdc_unit]->unit, req);
}

static int wcd_goaway (struct kern_devconf *kdc, int force)
{
	dev_detach (kdc);
	return 0;
}

#ifndef ATAPI_STATIC
static
#endif
int 
wcdattach (struct atapi *ata, int unit, struct atapi_params *ap, int debug,
	struct kern_devconf *parent)
{
	struct wcd *t;
	struct atapires result;
	int lun;

	if (wcdnlun >= NUNIT) {
		printf ("wcd: too many units\n");
		return (0);
	}
	if (!atapi_request_immediate) {
		printf("wcd: configuration error, ATAPI core code not present!\n");
		printf("wcd: check `options ATAPI_STATIC' in your kernel config file!\n");
		return (0);
	}
	t = malloc (sizeof (struct wcd), M_TEMP, M_NOWAIT);
	if (! t) {
		printf ("wcd: out of memory\n");
		return (0);
	}
	wcdtab[wcdnlun] = t;
	bzero (t, sizeof (struct wcd));
	t->ata = ata;
	t->unit = unit;
	lun = t->lun = wcdnlun++;
	t->param = ap;
	t->flags = F_MEDIA_CHANGED;
	t->refcnt = 0;
	if (debug) {
		t->flags |= F_DEBUG;
		/* Print params. */
		wcd_dump (t->lun, "info", ap, sizeof *ap);
	}

	/* Get drive capabilities. */
	result = atapi_request_immediate (ata, unit, ATAPI_MODE_SENSE,
		0, CAP_PAGE, 0, 0, 0, 0, sizeof (t->cap) >> 8, sizeof (t->cap),
		0, 0, 0, 0, 0, 0, 0, (char*) &t->cap, sizeof (t->cap));

	/* Do it twice to avoid the stale media changed state. */
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
		wcd_describe (t);
		if (t->flags & F_DEBUG)
			wcd_dump (t->lun, "cap", &t->cap, sizeof t->cap);
	}

	/* Register driver */
	t->cf = cftemplate;
	t->cf.kdc_unit = t->lun;
	t->cf.kdc_parent = parent;
	t->cf.kdc_description = t->description;
	strcpy (t->description, cftemplate.kdc_description);
	strcat (t->description, ": ");
	strncpy (t->description + strlen(t->description),
		ap->model, sizeof(ap->model));
	dev_attach (&t->cf);

#ifdef DEVFS
#define WDC_UID 0
#define WDC_GID 13
	t->ra_devfs_token = 
		devfs_add_devswf(&wcd_cdevsw, (lun * 8), DV_CHR, WDC_UID, 
				 WDC_GID, 0600, "rwcd%da", lun);
	t->rc_devfs_token = 
		devfs_add_devswf(&wcd_cdevsw, (lun * 8) + RAW_PART, DV_CHR,
				 WDC_UID,  WDC_GID, 0600, "rwcd%dc", lun);
	t->a_devfs_token = 
		devfs_add_devswf(&wcd_bdevsw, (lun * 8), DV_BLK, WDC_UID,
				 WDC_GID, 0600, "wcd%da", lun);
	t->c_devfs_token = 
		devfs_add_devswf(&wcd_bdevsw, (lun * 8) + RAW_PART, DV_BLK,
				 WDC_UID,  WDC_GID, 0600, "wcd%dc", lun);
#endif
	return (1);
}

void wcd_describe (struct wcd *t)
{
	char *m;

	t->cap.max_speed      = ntohs (t->cap.max_speed);
	t->cap.max_vol_levels = ntohs (t->cap.max_vol_levels);
	t->cap.buf_size       = ntohs (t->cap.buf_size);
	t->cap.cur_speed      = ntohs (t->cap.cur_speed);

	printf ("wcd%d: ", t->lun);
	if (t->cap.cur_speed != t->cap.max_speed)
		printf ("%d/", t->cap.cur_speed * 1000 / 1024);
	printf ("%dKb/sec", t->cap.max_speed * 1000 / 1024);
	if (t->cap.buf_size)
		printf (", %dKb cache", t->cap.buf_size);

	if (t->cap.audio_play)
		printf (", audio play");
	if (t->cap.max_vol_levels)
		printf (", %d volume levels", t->cap.max_vol_levels);

	switch (t->cap.mech) {
	default:             m = 0;           break;
	case MECH_CADDY:     m = "caddy";     break;
	case MECH_TRAY:      m = "tray";      break;
	case MECH_POPUP:     m = "popup";     break;
	case MECH_CHANGER:   m = "changer";   break;
	case MECH_CARTRIDGE: m = "cartridge"; break;
	}
	if (m)
		printf (", %s%s", t->cap.eject ? "ejectable " : "", m);
	else if (t->cap.eject)
		printf (", eject");
	printf ("\n");

	printf ("wcd%d: ", t->lun);
	switch (t->cap.medium_type) {
	case MDT_UNKNOWN:   printf ("medium type unknown");          break;
	case MDT_DATA_120:  printf ("120mm data disc loaded");       break;
	case MDT_AUDIO_120: printf ("120mm audio disc loaded");      break;
	case MDT_COMB_120:  printf ("120mm data/audio disc loaded"); break;
	case MDT_PHOTO_120: printf ("120mm photo disc loaded");      break;
	case MDT_DATA_80:   printf ("80mm data disc loaded");        break;
	case MDT_AUDIO_80:  printf ("80mm audio disc loaded");       break;
	case MDT_COMB_80:   printf ("80mm data/audio disc loaded");  break;
	case MDT_PHOTO_80:  printf ("80mm photo disc loaded");       break;
	case MDT_NO_DISC:   printf ("no disc inside");               break;
	case MDT_DOOR_OPEN: printf ("door open");                    break;
	case MDT_FMT_ERROR: printf ("medium format error");          break;
	default:    printf ("medium type=0x%x", t->cap.medium_type); break;
	}
	if (t->cap.lock)
		printf (t->cap.locked ? ", locked" : ", unlocked");
	if (t->cap.prevent)
		printf (", lock protected");
	printf ("\n");
}

static int 
wcd_open (dev_t dev, int rawflag)
{
	int lun = UNIT(dev);
	struct wcd *t;

	/* Check that the device number is legal
	 * and the ATAPI driver is loaded. */
	if (lun >= wcdnlun || ! atapi_request_immediate)
		return (ENXIO);
	t = wcdtab[lun];

	/* On the first open, read the table of contents. */
	if (! (t->flags & F_BOPEN) && ! t->refcnt) {
		/* Read table of contents. */
		if (wcd_read_toc (t) < 0)
			return (EIO);

		/* Lock the media. */
		wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
	}
	if (rawflag)
		++t->refcnt;
	else
		t->flags |= F_BOPEN;
	return (0);
}

int wcdbopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	return wcd_open (dev, 0);
}

int wcdropen (dev_t dev, int flags, int fmt, struct proc *p)
{
	return wcd_open (dev, 1);
}

/*
 * Close the device.  Only called if we are the LAST
 * occurence of an open device.
 */
int wcdbclose (dev_t dev, int flags, int fmt, struct proc *p)
{
	int lun = UNIT(dev);
	struct wcd *t = wcdtab[lun];

	/* If we were the last open of the entire device, release it. */
	if (! t->refcnt)
		wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	t->flags &= ~F_BOPEN;
	return (0);
}

int wcdrclose (dev_t dev, int flags, int fmt, struct proc *p)
{
	int lun = UNIT(dev);
	struct wcd *t = wcdtab[lun];

	/* If we were the last open of the entire device, release it. */
	if (! (t->flags & F_BOPEN) && t->refcnt == 1)
		wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	--t->refcnt;
	return (0);
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand. The transfer is described by a buf and will include only one
 * physical transfer.
 */
void wcdstrategy (struct buf *bp)
{
	int lun = UNIT(bp->b_dev);
	struct wcd *t = wcdtab[lun];
	int x;

	/* Can't ever write to a CD. */
	if (! (bp->b_flags & B_READ)) {
		bp->b_error = EROFS;
		bp->b_flags |= B_ERROR;
		biodone (bp);
		return;
	}

	/* If it's a null transfer, return immediatly. */
	if (bp->b_bcount == 0) {
		bp->b_resid = 0;
		biodone (bp);
		return;
	}

	/* Process transfer request. */
	bp->b_pblkno = bp->b_blkno;
	bp->b_resid = bp->b_bcount;
	x = splbio();

	/* Place it in the queue of disk activities for this disk. */
	tqdisksort (&t->buf_queue, bp);

	/* Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion. */
	wcd_start (t);
	splx(x);
}

/*
 * Look to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates an ATAPI command to perform the
 * transfer in the buf.
 * The bufs are queued by the strategy routine (wcdstrategy).
 * Must be called at the correct (splbio) level.
 */
static void wcd_start (struct wcd *t)
{
	struct buf *bp = TAILQ_FIRST(&t->buf_queue);
	u_long blkno, nblk;

	/* See if there is a buf to do and we are not already doing one. */
	if (! bp)
		return;

	/* Unqueue the request. */
	TAILQ_REMOVE(&t->buf_queue, bp, b_act);

	/* Should reject all queued entries if media have changed. */
	if (t->flags & F_MEDIA_CHANGED) {
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		biodone (bp);
		return;
	}

	/* We have a buf, now we should make a command
	 * First, translate the block to absolute and put it in terms of the
	 * logical blocksize of the device.
	 * What if something asks for 512 bytes not on a 2k boundary? */
	blkno = bp->b_blkno / (SECSIZE / 512);
	nblk = (bp->b_bcount + (SECSIZE - 1)) / SECSIZE;

	atapi_request_callback (t->ata, t->unit, ATAPI_READ_BIG, 0,
		blkno>>24, blkno>>16, blkno>>8, blkno, 0, nblk>>8, nblk, 0, 0,
		0, 0, 0, 0, 0, (u_char*) bp->b_un.b_addr, bp->b_bcount,
		wcd_done, t, bp);
	t->cf.kdc_state = DC_BUSY;
}

static void wcd_done (struct wcd *t, struct buf *bp, int resid,
	struct atapires result)
{
	if (result.code) {
		wcd_error (t, result);
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
	} else
		bp->b_resid = resid;
	biodone (bp);
	t->cf.kdc_state = DC_IDLE;
	wcd_start (t);
}

static void wcd_error (struct wcd *t, struct atapires result)
{
	if (result.code != RES_ERR)
		return;
	switch (result.error & AER_SKEY) {
	case AER_SK_NOT_READY:
		if (result.error & ~AER_SKEY) {
			/* Audio disc. */
			printf ("wcd%d: cannot read audio disc\n", t->lun);
			return;
		}
		/* Tray open. */
		if (! (t->flags & F_MEDIA_CHANGED))
			printf ("wcd%d: tray open\n", t->lun);
		t->flags |= F_MEDIA_CHANGED;
		return;

	case AER_SK_UNIT_ATTENTION:
		/* Media changed. */
		if (! (t->flags & F_MEDIA_CHANGED))
			printf ("wcd%d: media changed\n", t->lun);
		t->flags |= F_MEDIA_CHANGED;
		return;

	case AER_SK_ILLEGAL_REQUEST:
		/* Unknown command or invalid command arguments. */
		if (t->flags & F_DEBUG)
			printf ("wcd%d: invalid command\n", t->lun);
		return;
	}
	printf ("wcd%d: i/o error, status=%b, error=%b\n", t->lun,
		result.status, ARS_BITS, result.error, AER_BITS);
}

static int wcd_request_wait (struct wcd *t, u_char cmd, u_char a1, u_char a2,
	u_char a3, u_char a4, u_char a5, u_char a6, u_char a7, u_char a8,
	u_char a9, char *addr, int count)
{
	struct atapires result;

	t->cf.kdc_state = DC_BUSY;
	result = atapi_request_wait (t->ata, t->unit, cmd,
		a1, a2, a3, a4, a5, a6, a7, a8, a9, 0, 0, 0, 0, 0, 0,
		addr, count);
	t->cf.kdc_state = DC_IDLE;
	if (result.code) {
		wcd_error (t, result);
		return (EIO);
	}
	return (0);
}

static inline void lba2msf (int lba, u_char *m, u_char *s, u_char *f)
{
	lba += 150;             /* offset of first logical frame */
	lba &= 0xffffff;        /* negative lbas use only 24 bits */
	*m = lba / (60 * 75);
	lba %= (60 * 75);
	*s = lba / 75;
	*f = lba % 75;
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
int wcdioctl (dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	int lun = UNIT(dev);
	struct wcd *t = wcdtab[lun];
	int error = 0;

	if (t->flags & F_MEDIA_CHANGED)
		switch (cmd) {
		case CDIOCSETDEBUG:
		case CDIOCCLRDEBUG:
		case CDIOCRESET:
			/* These ops are media change transparent. */
			break;
		default:
			/* Read table of contents. */
			wcd_read_toc (t);

			/* Lock the media. */
			wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
				0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);
			break;
		}
	switch (cmd) {
	default:
		return (ENOTTY);

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

	case CDIOCRESUME:
		return wcd_request_wait (t, ATAPI_PAUSE,
			0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);

	case CDIOCPAUSE:
		return wcd_request_wait (t, ATAPI_PAUSE,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCSTART:
		return wcd_request_wait (t, ATAPI_START_STOP,
			1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCSTOP:
		return wcd_request_wait (t, ATAPI_START_STOP,
			1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCALLOW:
		return wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCPREVENT:
		return wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCRESET:
		if (p->p_cred->pc_ucred->cr_uid)
			return (EPERM);
		return wcd_request_wait (t, ATAPI_TEST_UNIT_READY,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	case CDIOCEJECT:
		/* Don't allow eject if the device is opened
		 * by somebody (not us) in block mode. */
		if ((t->flags & F_BOPEN) && t->refcnt)
			return (EBUSY);
		return wcd_eject (t);

	case CDIOREADTOCHEADER:
		if (! t->toc.hdr.ending_track)
			return (EIO);
		bcopy (&t->toc.hdr, addr, sizeof t->toc.hdr);
		break;

	case CDIOREADTOCENTRYS: {
		struct ioc_read_toc_entry *te =
			(struct ioc_read_toc_entry*) addr;
		struct toc *toc = &t->toc;
		struct toc buf;
		u_long len;
		u_char starting_track = te->starting_track;

		if (! t->toc.hdr.ending_track)
			return (EIO);

		if (   te->data_len < sizeof(toc->tab[0])
		    || (te->data_len % sizeof(toc->tab[0])) != 0
		    || te->address_format != CD_MSF_FORMAT
		    && te->address_format != CD_LBA_FORMAT
		   )
			return EINVAL;

		if (starting_track == 0)
			starting_track = toc->hdr.starting_track;
		else if (starting_track == 170) /* Handle leadout request */
			starting_track = toc->hdr.ending_track + 1;
		else if (starting_track < toc->hdr.starting_track ||
			 starting_track > toc->hdr.ending_track + 1)
                        return (EINVAL);

		len = ((toc->hdr.ending_track + 1 - starting_track) + 1) *
			sizeof(toc->tab[0]);
		if (te->data_len < len)
			len = te->data_len;
		if (len > sizeof(toc->tab))
			return EINVAL;

		/* Convert to MSF format, if needed. */
		if (te->address_format == CD_MSF_FORMAT) {
			struct cd_toc_entry *e;

			buf = t->toc;
			toc = &buf;
			e = toc->tab + (toc->hdr.ending_track + 1 -
					toc->hdr.starting_track) + 1;
			while (--e >= toc->tab)
				lba2msf (ntohl(e->addr.lba), &e->addr.msf.minute,
				    &e->addr.msf.second, &e->addr.msf.frame);
		}
		return copyout (toc->tab + starting_track -
				toc->hdr.starting_track, te->data, len);
	}
	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args =
			(struct ioc_read_subchannel*) addr;
		struct cd_sub_channel_info data;
		u_long len = args->data_len;
		int abslba, rellba;

		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header))
			return (EINVAL);

		if (wcd_request_wait (t, ATAPI_READ_SUBCHANNEL, 0, 0x40, 1, 0,
		    0, 0, sizeof (t->subchan) >> 8, sizeof (t->subchan),
		    0, (char*)&t->subchan, sizeof (t->subchan)) != 0)
			return (EIO);
		if (t->flags & F_DEBUG)
			wcd_dump (t->lun, "subchan", &t->subchan, sizeof t->subchan);

		abslba = t->subchan.abslba;
		rellba = t->subchan.rellba;
		if (args->address_format == CD_MSF_FORMAT) {
			lba2msf (ntohl(abslba),
				&data.what.position.absaddr.msf.minute,
				&data.what.position.absaddr.msf.second,
				&data.what.position.absaddr.msf.frame);
			lba2msf (ntohl(rellba),
				&data.what.position.reladdr.msf.minute,
				&data.what.position.reladdr.msf.second,
				&data.what.position.reladdr.msf.frame);
		} else {
			data.what.position.absaddr.lba = abslba;
			data.what.position.reladdr.lba = rellba;
		}
		data.header.audio_status = t->subchan.audio_status;
		data.what.position.control = t->subchan.control & 0xf;
		data.what.position.addr_type = t->subchan.control >> 4;
		data.what.position.track_number = t->subchan.track;
		data.what.position.index_number = t->subchan.indx;

		return copyout (&data, args->data, len);
	}
	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args = (struct ioc_play_msf*) addr;

		return wcd_request_wait (t, ATAPI_PLAY_MSF, 0, 0,
			args->start_m, args->start_s, args->start_f,
			args->end_m, args->end_s, args->end_f, 0, 0, 0);
	}
	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args = (struct ioc_play_blocks*) addr;

		return wcd_request_wait (t, ATAPI_PLAY_BIG, 0,
			args->blk >> 24 & 0xff, args->blk >> 16 & 0xff,
			args->blk >> 8 & 0xff, args->blk & 0xff,
			args->len >> 24 & 0xff, args->len >> 16 & 0xff,
			args->len >> 8 & 0xff, args->len & 0xff, 0, 0);
	}
	case CDIOCPLAYTRACKS: {
		struct ioc_play_track *args = (struct ioc_play_track*) addr;
		u_long start, len;
		int t1, t2;

		if (! t->toc.hdr.ending_track)
			return (EIO);

		/* Ignore index fields,
		 * play from start_track to end_track inclusive. */
		if (args->end_track < t->toc.hdr.ending_track+1)
			++args->end_track;
		if (args->end_track > t->toc.hdr.ending_track+1)
			args->end_track = t->toc.hdr.ending_track+1;
		t1 = args->start_track - t->toc.hdr.starting_track;
		t2 = args->end_track - t->toc.hdr.starting_track;
		if (t1 < 0 || t2 < 0)
			return (EINVAL);
		start = ntohl(t->toc.tab[t1].addr.lba);
		len = ntohl(t->toc.tab[t2].addr.lba) - start;

		return wcd_request_wait (t, ATAPI_PLAY_BIG, 0,
			start >> 24 & 0xff, start >> 16 & 0xff,
			start >> 8 & 0xff, start & 0xff,
			len >> 24 & 0xff, len >> 16 & 0xff,
			len >> 8 & 0xff, len & 0xff, 0, 0);
	}
	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol*) addr;

		error = wcd_request_wait (t, ATAPI_MODE_SENSE, 0, AUDIO_PAGE,
			0, 0, 0, 0, sizeof (t->au) >> 8, sizeof (t->au), 0,
			(char*) &t->au, sizeof (t->au));
		if (error)
			return (error);
		if (t->flags & F_DEBUG)
			wcd_dump (t->lun, "au", &t->au, sizeof t->au);
		if (t->au.page_code != AUDIO_PAGE)
			return (EIO);
		arg->vol[0] = t->au.port[0].volume;
		arg->vol[1] = t->au.port[1].volume;
		arg->vol[2] = t->au.port[2].volume;
		arg->vol[3] = t->au.port[3].volume;
		break;
	}
	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol*) addr;

		error = wcd_request_wait (t, ATAPI_MODE_SENSE, 0, AUDIO_PAGE,
			0, 0, 0, 0, sizeof (t->au) >> 8, sizeof (t->au), 0,
			(char*) &t->au, sizeof (t->au));
		if (error)
			return (error);
		if (t->flags & F_DEBUG)
			wcd_dump (t->lun, "au", &t->au, sizeof t->au);
		if (t->au.page_code != AUDIO_PAGE)
			return (EIO);

		error = wcd_request_wait (t, ATAPI_MODE_SENSE, 0,
			AUDIO_PAGE_MASK, 0, 0, 0, 0, sizeof (t->aumask) >> 8,
			sizeof (t->aumask), 0, (char*) &t->aumask,
			sizeof (t->aumask));
		if (error)
			return (error);
		if (t->flags & F_DEBUG)
			wcd_dump (t->lun, "mask", &t->aumask, sizeof t->aumask);

		/* Sony-55E requires the data length field to be zeroed. */
		t->au.data_length = 0;

		t->au.port[0].channels = CHANNEL_0;
		t->au.port[1].channels = CHANNEL_1;
		t->au.port[0].volume = arg->vol[0] & t->aumask.port[0].volume;
		t->au.port[1].volume = arg->vol[1] & t->aumask.port[1].volume;
		t->au.port[2].volume = arg->vol[2] & t->aumask.port[2].volume;
		t->au.port[3].volume = arg->vol[3] & t->aumask.port[3].volume;
		return wcd_request_wait (t, ATAPI_MODE_SELECT_BIG, 0x10,
			0, 0, 0, 0, 0, sizeof (t->au) >> 8, sizeof (t->au),
			0, (char*) &t->au, - sizeof (t->au));
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch*) addr;

		return wcd_setchan (t, arg->patch[0], arg->patch[1],
			arg->patch[2], arg->patch[3]);
	}
	case CDIOCSETMONO:
		return wcd_setchan (t, CHANNEL_0 | CHANNEL_1,
			CHANNEL_0 | CHANNEL_1, 0, 0);

	case CDIOCSETSTERIO:
		return wcd_setchan (t, CHANNEL_0, CHANNEL_1, 0, 0);

	case CDIOCSETMUTE:
		return wcd_setchan (t, 0, 0, 0, 0);

	case CDIOCSETLEFT:
		return wcd_setchan (t, CHANNEL_0, CHANNEL_0, 0, 0);

	case CDIOCSETRIGHT:
		return wcd_setchan (t, CHANNEL_1, CHANNEL_1, 0, 0);
	}
	return (error);
}

/*
 * Read the entire TOC for the disc into our internal buffer.
 */
static int wcd_read_toc (struct wcd *t)
{
	int ntracks, len;
	struct atapires result;

	bzero (&t->toc, sizeof (t->toc));
	bzero (&t->info, sizeof (t->info));

	/* Check for the media.
	 * Do it twice to avoid the stale media changed state. */
	result = atapi_request_wait (t->ata, t->unit, ATAPI_TEST_UNIT_READY,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	if (result.code == RES_ERR &&
	    (result.error & AER_SKEY) == AER_SK_UNIT_ATTENTION) {
		t->flags |= F_MEDIA_CHANGED;
		result = atapi_request_wait (t->ata, t->unit,
			ATAPI_TEST_UNIT_READY, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	if (result.code) {
		wcd_error (t, result);
		return (EIO);
	}
	t->flags &= ~F_MEDIA_CHANGED;

	/* First read just the header, so we know how long the TOC is. */
	len = sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry);
	if (wcd_request_wait (t, ATAPI_READ_TOC, 0, 0, 0, 0, 0, 0,
	    len >> 8, len & 0xff, 0, (char*)&t->toc, len) != 0) {
err:            bzero (&t->toc, sizeof (t->toc));
		return (0);
	}

	ntracks = t->toc.hdr.ending_track - t->toc.hdr.starting_track + 1;
	if (ntracks <= 0 || ntracks > MAXTRK)
		goto err;

	/* Now read the whole schmeer. */
	len = sizeof(struct ioc_toc_header) +
		ntracks * sizeof(struct cd_toc_entry);
	if (wcd_request_wait (t, ATAPI_READ_TOC, 0, 0, 0, 0, 0, 0,
	    len >> 8, len & 0xff, 0, (char*)&t->toc, len) & 0xff)
		goto err;

	NTOHS(t->toc.hdr.len);

	/* Read disc capacity. */
	if (wcd_request_wait (t, ATAPI_READ_CAPACITY, 0, 0, 0, 0, 0, 0,
	    0, sizeof(t->info), 0, (char*)&t->info, sizeof(t->info)) != 0)
		bzero (&t->info, sizeof (t->info));

	/* make fake leadout entry */
	t->toc.tab[ntracks].control = t->toc.tab[ntracks-1].control;
	t->toc.tab[ntracks].addr_type = t->toc.tab[ntracks-1].addr_type;
	t->toc.tab[ntracks].track = 170; /* magic */
	t->toc.tab[ntracks].addr.lba = t->info.volsize;

	NTOHL(t->info.volsize);
	NTOHL(t->info.blksize);

	/* Print the disc description string on every disc change.
	 * It would help to track the history of disc changes. */
	if (t->info.volsize && t->toc.hdr.ending_track &&
	    (t->flags & F_MEDIA_CHANGED) && (t->flags & F_DEBUG)) {
		printf ("wcd%d: ", t->lun);
		if (t->toc.tab[0].control & 4)
			printf ("%ldMB ", t->info.volsize / 512);
		else
			printf ("%ld:%ld audio ", t->info.volsize/75/60,
				t->info.volsize/75%60);
		printf ("(%ld sectors), %d tracks\n", t->info.volsize,
			t->toc.hdr.ending_track - t->toc.hdr.starting_track + 1);
	}
	return (0);
}

/*
 * Set up the audio channel masks.
 */
static int wcd_setchan (struct wcd *t,
	u_char c0, u_char c1, u_char c2, u_char c3)
{
	int error;

	error = wcd_request_wait (t, ATAPI_MODE_SENSE, 0, AUDIO_PAGE,
		0, 0, 0, 0, sizeof (t->au) >> 8, sizeof (t->au), 0,
		(char*) &t->au, sizeof (t->au));
	if (error)
		return (error);
	if (t->flags & F_DEBUG)
		wcd_dump (t->lun, "au", &t->au, sizeof t->au);
	if (t->au.page_code != AUDIO_PAGE)
		return (EIO);

	/* Sony-55E requires the data length field to be zeroed. */
	t->au.data_length = 0;

	t->au.port[0].channels = c0;
	t->au.port[1].channels = c1;
	t->au.port[2].channels = c2;
	t->au.port[3].channels = c3;
	return wcd_request_wait (t, ATAPI_MODE_SELECT_BIG, 0x10,
		0, 0, 0, 0, 0, sizeof (t->au) >> 8, sizeof (t->au),
		0, (char*) &t->au, - sizeof (t->au));
}

static int wcd_eject (struct wcd *t)
{
	struct atapires result;

	/* Try to stop the disc. */
	t->cf.kdc_state = DC_BUSY;
	result = atapi_request_wait (t->ata, t->unit, ATAPI_START_STOP,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	t->cf.kdc_state = DC_IDLE;

	if (result.code == RES_ERR &&
	    ((result.error & AER_SKEY) == AER_SK_NOT_READY ||
	    (result.error & AER_SKEY) == AER_SK_UNIT_ATTENTION)) {
		/*
		 * The disc was unloaded.
		 * Load it (close tray).
		 * Read the table of contents.
		 */
		int err = wcd_request_wait (t, ATAPI_START_STOP,
			0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0);
		if (err)
			return (err);

		/* Read table of contents. */
		wcd_read_toc (t);

		/* Lock the media. */
		wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0);

		return (0);
	}

	if (result.code) {
		wcd_error (t, result);
		return (EIO);
	}

	/* Give it some time to stop spinning. */
	tsleep ((caddr_t)&lbolt, PRIBIO, "wcdej1", 0);
	tsleep ((caddr_t)&lbolt, PRIBIO, "wcdej2", 0);

	/* Unlock. */
	wcd_request_wait (t, ATAPI_PREVENT_ALLOW,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	/* Eject. */
	t->flags |= F_MEDIA_CHANGED;
	return wcd_request_wait (t, ATAPI_START_STOP,
		0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0);
}

#ifdef WCD_MODULE
/*
 * Loadable ATAPI CD-ROM driver stubs.
 */
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Construct lkm_dev structures (see lkm.h).
 * Our bdevsw/cdevsw slot numbers are 19/69.
 */


MOD_DEV(wcd, LM_DT_BLOCK, BDEV_MAJOR, &wcd_bdevsw);
MOD_DEV(rwcd, LM_DT_CHAR, CDEV_MAJOR, &wcd_cdevsw);

/*
 * Function called when loading the driver.
 */
int wcd_load (struct lkm_table *lkmtp, int cmd)
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
				    wcdattach (ata, u, ata->params[u],
				    ata->debug, ata->parent) >= 0)
				{
					/* Drive found. */
					ata->attached[u] = 1;
					++n;
				}
	if (! n)
		/* No IDE CD-ROMs found. */
		return ENXIO;
	return 0;
}

/*
 * Function called when unloading the driver.
 */
int wcd_unload (struct lkm_table *lkmtp, int cmd)
{
	struct wcd **t;

	for (t=wcdtab; t<wcdtab+wcdnlun; ++t)
		if (((*t)->flags & F_BOPEN) || (*t)->refcnt)
			/* The device is opened, cannot unload the driver. */
			return EBUSY;
	for (t=wcdtab; t<wcdtab+wcdnlun; ++t) {
		(*t)->ata->attached[(*t)->unit] = 0;
		free (*t, M_TEMP);
	}
	wcdnlun = 0;
	bzero (wcdtab, sizeof(wcdtab));
	return 0;
}

/*
 * Dispatcher function for the module (load/unload/stat).
 */
int wcd_mod (struct lkm_table *lkmtp, int cmd, int ver)
{
	int err = 0;

	if (ver != LKM_VERSION)
		return EINVAL;

	if (cmd == LKM_E_LOAD)
		err = wcd_load (lkmtp, cmd);
	else if (cmd == LKM_E_UNLOAD)
		err = wcd_unload (lkmtp, cmd);
	if (err)
		return err;

	/* Register the cdevsw entry. */
	lkmtp->private.lkm_dev = &rwcd_module;
	err = lkmdispatch (lkmtp, cmd);
	if (err)
		return err;

	/* Register the bdevsw entry. */
	lkmtp->private.lkm_dev = &wcd_module;
	return lkmdispatch (lkmtp, cmd);
}
#endif /* WCD_MODULE */

static wcd_devsw_installed = 0;

static void 	wcd_drvinit(void *unused)
{
	dev_t dev;

	if( ! wcd_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&wcd_cdevsw, NULL);
		dev = makedev(BDEV_MAJOR, 0);
		bdevsw_add(&dev,&wcd_bdevsw, NULL);
		wcd_devsw_installed = 1;
    	}
}

SYSINIT(wcddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,wcd_drvinit,NULL)


#endif /* NWCD && NWDC && ATAPI */
