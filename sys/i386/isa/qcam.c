/*
 * FreeBSD Connectix QuickCam parallel-port camera video capture driver.
 * Copyright (c) 1996, Paul Traina.
 *
 * This driver is based in part on the Linux QuickCam driver which is
 * Copyright (c) 1996, Thomas Davis.
 *
 * QuickCam(TM) is a registered trademark of Connectix Inc.
 * Use this driver at your own risk, it is not warranted by
 * Connectix or the authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include	"qcam.h"
#if NQCAM > 0

#include	<sys/param.h>
#include	<sys/systm.h>
#include	<sys/kernel.h>
#include	<sys/conf.h>
#include	<sys/ioctl.h>
#include	<sys/uio.h>
#include	<sys/malloc.h>
#include	<sys/devconf.h>
#include	<sys/errno.h>
#ifdef DEVFS
#include	<sys/devfsext.h>
#endif /* DEVFS */

#include	<machine/cpu.h>
#include	<machine/clock.h>
#include	<machine/qcam.h>

#include	<i386/isa/qcamreg.h>
#include	<i386/isa/isa.h>
#include	<i386/isa/isa_device.h>

int	qcam_debug = 1;

static struct qcam_softc {
	u_char		*buffer;		/* frame buffer */
	u_char		*buffer_end;		/* end of frame buffer */
	u_int		flags;
	u_int		iobase;
	int		unit;			/* device */
	void		(*scanner)(struct qcam_softc *);

	int		init_req;		/* initialization required */
	int		x_size;			/* pixels */
	int		y_size;			/* pixels */
	int		x_origin;		/* ?? units */
	int		y_origin;		/* ?? units */
	int		zoom;			/* 0=none, 1=1.5x, 2=2x */
	int		bpp;			/* 4 or 6 */
	u_char		xferparms;		/* calcualted transfer params */
	u_char		contrast;
	u_char		brightness;
	u_char		whitebalance;

	struct		kern_devconf kdc;	/* kernel config database */

#ifdef	DEVFS
	void		*devfs_token;
#endif
} qcam_softc[NQCAM];

static struct kern_devconf kdc_qcam_template = {
	0, 0, 0,			/* filled in by dev_attach() */
	"qcam",				/* kdc_name */
	0, 				/* kdc_unit */
	{ 				/* kdc_md */
  	   MDDT_ISA,			/* mddc_devtype */
	   0,				/* mddc_flags */
	   "tty"			/* mddc_imask[4] */
	},
	isa_generic_externalize,	/* kdc_externalize */
	0,				/* kdc_internalize */
	0,				/* kdc_goaway */
	ISA_EXTERNALLEN,		/* kdc_datalen */
	&kdc_isa0,			/* kdc_parent */
	0,				/* kdc_parentdata */
	DC_UNCONFIGURED,		/* kdc_state */
	"",				/* kdc_description */
	DC_CLS_MISC			/* class */
};

/* flags in softc */
#define	QC_OPEN			0x01		/* device open */
#define	QC_ALIVE		0x02		/* probed and attached */
#define	QC_BIDIR_HW		0x04		/* bidir parallel port */
#define	QC_BIDIR_REQ		0x08		/* bidir xfer requested */

#define	QC_MAXFRAMEBUFSIZE	(QC_MAX_XSIZE*QC_MAX_YSIZE)

static const u_char qcam_zoommode[3][3] = {
	{ QC_XFER_WIDE,   QC_XFER_WIDE,   QC_XFER_WIDE },
	{ QC_XFER_NARROW, QC_XFER_WIDE,   QC_XFER_WIDE },
	{ QC_XFER_TIGHT,  QC_XFER_NARROW, QC_XFER_WIDE }
};

#define	UNIT(dev)		minor(dev)

static int qcam_probe(struct isa_device *id);
static int qcam_attach(struct isa_device *id);

struct isa_driver	qcamdriver =
			{qcam_probe, qcam_attach, "qcam"};

static	d_open_t	qcam_open;
static	d_close_t	qcam_close;
static	d_read_t	qcam_read;
static	d_ioctl_t	qcam_ioctl;

#define CDEV_MAJOR 73			/* XXX change this! */

static struct cdevsw qcam_cdevsw = 
	{ qcam_open,	qcam_close,	qcam_read,	nowrite,
	  qcam_ioctl,	nostop,		nullreset,	nodevtotty,
	  noselect,	nommap,		nostrategy,	"qcam",
	  NULL,		-1  };

#define	read_data(P)		inb((P))
#define	read_data_word(P)	inw((P))
#define	read_status(P)		inb((P)+1)
#define	write_data(P, V)	outb((P)+0, (V))
#define	write_status(P, V)	outb((P)+1, (V))
#define write_control(P, V)	outb((P)+2, (V))

#define	QC_TIMEOUT		10000		/* timeout on reads */

#define	READ_STATUS_BYTE_HIGH(P, V) { \
	u_int timeout = QC_TIMEOUT; \
	do { (V) = read_status((P)); \
	} while (!(((V) & 0x08)) && timeout-- > 0); \
}

#define	READ_STATUS_BYTE_LOW(P, V) { \
	u_int timeout = QC_TIMEOUT; \
	do { (V) = read_status((P)); \
	} while (((V) & 0x08) && timeout-- > 0); \
}
		
#define	READ_DATA_WORD_HIGH(P, V) { \
	u_int timeout = QC_TIMEOUT; \
	do { (V) = read_data_word((P)); \
	} while (!(((V) & 0x01)) && timeout-- > 0); \
}

#define	READ_DATA_WORD_LOW(P, V) { \
	u_int timeout = QC_TIMEOUT; \
	do { (V) = read_data_word((P)); \
	} while (((V) & 0x01) && timeout-- > 0); \
}

inline static int
sendbyte (u_int port, int value)
{
	u_char s1, s2;

	write_data(port, value);
	DELAY(100);
	write_data(port, value);
	DELAY(100);
	write_data(port, value);

	write_control(port, 0x06);
	READ_STATUS_BYTE_HIGH(port, s1);

	write_control(port, 0x0e);
	READ_STATUS_BYTE_LOW(port, s2);

	return (s1 & 0xf0) | (s2 >> 4);
}

static int
send_command (u_int port, int cmd, int value)
{
	if (sendbyte(port, cmd) != cmd)
		return 1;

	if (sendbyte(port, value) != value)
		return 1;

	return 0;			/* success */
}

static void
qcam_reset (struct qcam_softc *qs)
{
	register u_int  iobase = qs->iobase;

	write_control(iobase, 0x20);
	write_data   (iobase, 0x75);

	if (read_data(iobase) != 0x75)
	    qs->flags |= QC_BIDIR_HW;	/* bidirectional parallel port */
	else
	    qs->flags &= ~QC_BIDIR_HW;

	write_control(iobase, 0x0b);
	DELAY(250);
	write_control(iobase, 0x0e);
	DELAY(250);
}

static int
qcam_waitfor_bi (u_int port)
{
	u_char s1, s2;

	write_control(port, 0x28);
	READ_STATUS_BYTE_HIGH(port, s1);

	write_control(port, 0x2f);
	READ_STATUS_BYTE_LOW(port, s2);

	return (s1 & 0xf0) | (s2 >> 4);
}

static void
qcam_bi_4bit (struct qcam_softc *qs)
{
	static int first_time = 1;

	if (first_time) {
		first_time = 0;
		printf("qcam%d: bidirectional 4bpp transfers not implemented\n",
			qs->unit);
	}
}

static void
qcam_bi_6bit (struct qcam_softc *qs)
{
	u_char *p;
	u_int   word, port;

	port = qs->iobase;			/* for speed */

	qcam_waitfor_bi(port);			/* wait for ready */

	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, 0x26);
		READ_DATA_WORD_HIGH(port, word);

		/* this can be optimized _significantly_ */
		word = (((word & 0xff00) >> 3) | (word & 0x00ff)) << 1;
		word = ((word & 0x00ff)  >> 2) | (word & 0xff00);
		*p++ = 63 - (word & 0xff);
		*p++ = 63 - ((word >> 8) & 0xff);

		write_control(port, 0x2f);
		READ_DATA_WORD_LOW(port, word);

		/* this can be optimized _significantly_ */
		word = (((word & 0xff00) >> 3) | (word & 0x00ff)) << 1;
		word = ((word & 0x00ff)  >> 2) | (word & 0xff00);

		*p++ = 63 - (word & 0xff);
		*p++ = 63 - ((word >> 8) & 0xff);
	}
}

static void
qcam_uni_4bit (struct qcam_softc *qs)
{
	u_char	*p;
	u_int	word, port;

	port = qs->iobase;

	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, 0x06);
		READ_STATUS_BYTE_HIGH(port, word);

		*p++ = 16 - ((word & 0xf0) >> 4);

		write_control(port, 0x0e);
		READ_STATUS_BYTE_LOW(port, word);

		*p++ = 16 - ((word & 0xf0) >> 4);
	}
}

static void
qcam_uni_6bit (struct qcam_softc *qs)
{
	u_char	*p;
	u_int	word1, word2, word3, port;

	port = qs->iobase;

	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, 0x06);
		READ_STATUS_BYTE_HIGH(port, word1);

		word2 = word1 & 0xf0;

		write_control(port, 0x0e);
		READ_STATUS_BYTE_LOW(port, word1);

		word2 |= (word1 & 0xf0) >> 4;
		*p++ = 63 - ((word2 >> 2) & 0x3f);

		write_control(port, 0x06);
		READ_STATUS_BYTE_HIGH(port, word1);

		word3 = word2;
		word2 = word1 & 0xf0;

		write_control(port, 0x0e);
		READ_STATUS_BYTE_LOW(port, word1);

		word2 |= (word1 & 0xf0) >> 4;
		*p++ = 63 - (((word3 & 0x03) << 4) | (word2 >> 4));

		write_control(port, 0x06);
		READ_STATUS_BYTE_HIGH(port, word1);

		word3 = word2;
		word2 = word1 & 0xf0;

		write_control(port, 0x0e);
		READ_STATUS_BYTE_LOW(port, word1);

		word2 |= (word1 & 0xf0) >> 4;
		*p++ = 63 - (((word3 & 0x0f) << 2) | (word2 >> 6));
		*p++ = 63 - (word2 & 0x3f);
	}
}

static void
qcam_xferparms (struct qcam_softc *qs)
{
	int bidir;

	qs->xferparms = 0;

	/*
	 * XXX the && qs->bpp==6 is a temporary hack because we don't
	 *     have code for doing 4bpp bidirectional transfers yet
	 *     ...soon, my son.
	 */

	bidir = (((qs->flags & (QC_BIDIR_HW|QC_BIDIR_REQ)) ==
			       (QC_BIDIR_HW|QC_BIDIR_REQ)) &&
		 (qs->bpp == 6));

	if (bidir)
		qs->xferparms |= QC_XFER_BIDIR;

	if (qs->bpp == 6) {
		qs->xferparms |= QC_XFER_6BPP;
		qs->scanner    = bidir ? qcam_bi_6bit : qcam_uni_6bit;
	} else {
		qs->scanner    = bidir ? qcam_bi_4bit : qcam_uni_4bit;
	}

	if (qs->x_size > 160 || qs->y_size > 120) {
		qs->xferparms |= qcam_zoommode[0][qs->zoom];
	} else if (qs->x_size > 80 || qs->y_size > 60) {
		qs->xferparms |= qcam_zoommode[1][qs->zoom];
	} else
		qs->xferparms |= qcam_zoommode[2][qs->zoom];

#ifdef	DEBUG
	if (qcam_debug)
		printf("qcam%d: [(%d,%d), %sdir, %dbpp, %d zoom] = 0x%x\n",
			qs->unit, qs->x_size, qs->y_size,
			bidir ? "bi" : "uni", qs->bpp, qs->zoom,
			qs->xferparms);
#endif
}

static void
qcam_init (struct qcam_softc *qs)
{
	int x_size = (qs->bpp == 4) ? qs->x_size / 2 : qs->x_size / 4;

	qcam_xferparms(qs);

	send_command(qs->iobase, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs->iobase, QC_BRIGHTNESS,   1);
	send_command(qs->iobase, QC_BRIGHTNESS,   1);
	send_command(qs->iobase, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs->iobase, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs->iobase, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs->iobase, QC_XSIZE,	  x_size);
	send_command(qs->iobase, QC_YSIZE,	  qs->y_size);
	send_command(qs->iobase, QC_YORG,	  qs->y_origin);
	send_command(qs->iobase, QC_XORG,	  qs->x_origin);
	send_command(qs->iobase, QC_CONTRAST,	  qs->contrast);
	send_command(qs->iobase, QC_WHITEBALANCE, qs->whitebalance);

	if (qs->buffer)
	    qs->buffer_end = qs->buffer +
			     min((qs->x_size*qs->y_size), QC_MAXFRAMEBUFSIZE);

	qs->init_req = 0;
}

static void
qcam_scan (struct qcam_softc *qs)
{
	register u_int iobase = qs->iobase;

	if (qs->init_req)
		qcam_init(qs);

	send_command(iobase, QC_BRIGHTNESS, qs->brightness);
	send_command(iobase, QC_XFERMODE,   qs->xferparms);

	if (qs->scanner)
		(*qs->scanner)(qs);

	write_control(iobase, 0x0f);
	write_control(iobase, 0x0b);
}

static void
qcam_registerdev (struct isa_device *id, const char *descr)
{
	struct kern_devconf *kdc = &qcam_softc[id->id_unit].kdc;

	*kdc = kdc_qcam_template;		/* byte-copy template */

	kdc->kdc_unit = id->id_unit;
	kdc->kdc_parentdata = id;
	kdc->kdc_description = descr;

	dev_attach(kdc);
}

static int
qcam_probe (struct isa_device *devp)
{
	u_char reg, last;
	int i, transitions = 0;

	switch (devp->id_iobase) {	/* don't probe weird ports */
	case IO_LPT1:
	case IO_LPT2:
	case IO_LPT3:
		break;
	default:
		printf("qcam%d: ignoring non-standard port 0x%x\n",
		       devp->id_unit, devp->id_iobase);
		return 0;
	}

	/* write 0's to control and data ports */
	write_control(devp->id_iobase, 0x20);
	write_control(devp->id_iobase, 0x0b);
	write_control(devp->id_iobase, 0x0e);

	/*
	 * Attempt a non-destructive probe for the QuickCam.
	 * Current models appear to toggle the upper 4 bits of
	 * the status register at approximately 5-10 Hz.
	 *
	 * Be aware that this isn't the way that Connectix detects the
	 * camera (they send a reset and try to handshake),  but this
	 * way is safe.
	 */
	last = reg = read_status(devp->id_iobase);

	for (i = 0; i < QC_PROBELIMIT; i++) {

	    reg = read_status(devp->id_iobase) & 0xf0;

	    if (reg != last)	/* if we got a toggle, count it */
		transitions++;

	    last = reg;
	    DELAY(100000);	/* 100ms */
	}

	if (transitions <= QC_PROBECNTLOW || transitions >= QC_PROBECNTHI) {
	    if (bootverbose)
		printf("qcam%d: not found, probed %d, got %d transitions\n",
		       devp->id_unit, i, transitions);
	    return 0;
	}

	qcam_registerdev(devp, "QuickCam video input");
	return 1;		/* found */
}

static void
qcam_default (struct qcam_softc *qs) {
	qs->contrast     = QC_DEF_CONTRAST;
	qs->brightness   = QC_DEF_BRIGHTNESS;
	qs->whitebalance = QC_DEF_WHITEBALANCE;
	qs->x_size	 = QC_DEF_XSIZE;
	qs->y_size	 = QC_DEF_YSIZE;
	qs->x_origin	 = QC_DEF_XORG;
	qs->y_origin	 = QC_DEF_YORG;
	qs->bpp		 = QC_DEF_BPP;
	qs->zoom	 = QC_DEF_ZOOM;
}

static int
qcam_attach (struct isa_device *devp)
{
	struct	qcam_softc *qs = &qcam_softc[devp->id_unit];

	qs->iobase	 = devp->id_iobase;
	qs->unit	 = devp->id_unit;
	qs->kdc.kdc_state = DC_IDLE;
	qs->flags |= QC_ALIVE;

	qcam_default(qs);
	qcam_xferparms(qs);

#ifdef DEVFS
{
	char name[32];
	sprintf(name,"qcam%d", qs->unit);
	qs->devfs_token = devfs_add_devsw("/", name, &qcam_cdevsw, qs->unit,
					  DV_CHR, 0, 0, 0600);
}
#endif
	return 1;
}

static int
qcam_open (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct qcam_softc *qs = (struct qcam_softc *)&qcam_softc[UNIT(dev)];

	if (!(qs->flags & QC_ALIVE))
		return ENXIO;

	if (qs->flags & QC_OPEN)
		return EBUSY;

	qs->buffer_end = qs->buffer = malloc(QC_MAXFRAMEBUFSIZE, M_DEVBUF,
					     M_WAITOK);
	if (!qs->buffer)
		return ENOMEM;

	qcam_reset(qs);
	qcam_default(qs);
	qcam_init(qs);

	qs->flags |= QC_OPEN;
	qs->kdc.kdc_state = DC_BUSY;
	return 0;
}

static int
qcam_close (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct qcam_softc *qs = (struct qcam_softc *)&qcam_softc[UNIT(dev)];

	if (qs->buffer) {
	    free(qs->buffer, M_DEVBUF);
	    qs->buffer = NULL;
	    qs->buffer_end = NULL;
	}

	qs->flags &= ~QC_OPEN;
	qs->kdc.kdc_state = DC_IDLE;
	return 0;
}

static int
qcam_read (dev_t dev, struct uio *uio, int ioflag)
{
	struct qcam_softc *qs = (struct qcam_softc *)&qcam_softc[UNIT(dev)];
	int bytes, bufsize;
	int error;

	/* if we've seeked back to 0, that's our signal to scan */
	if (uio->uio_offset == 0)
		qcam_scan(qs);

	bufsize = qs->buffer_end - qs->buffer;
	if (uio->uio_offset > bufsize)
		return EIO;

	bytes = min(uio->uio_resid, (bufsize - uio->uio_offset));
	error = uiomove(qs->buffer + uio->uio_offset, bytes, uio);
	if (error)
		return error;

	return 0;		/* success */
}

static int
qcam_ioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct qcam_softc *qs = (struct qcam_softc *)&qcam_softc[UNIT(dev)];
	struct qcam      *info= (struct qcam *)data;

	if (!data)
		return(EINVAL);

	switch (cmd) {

	case QC_GET:
		info->qc_version	= QC_IOCTL_VERSION;
		info->qc_xsize		= qs->x_size;
		info->qc_ysize		= qs->y_size;
		info->qc_xorigin	= qs->x_origin;
		info->qc_yorigin	= qs->y_origin;
		info->qc_bpp		= qs->bpp;
		info->qc_zoom		= qs->zoom;
		info->qc_brightness	= qs->brightness;
		info->qc_whitebalance	= qs->whitebalance;
		info->qc_contrast	= qs->contrast;

		if (qs->flags & QC_BIDIR_REQ)
			info->qc_mode	= QC_MODE_BIDIR;
		else
			info->qc_mode	= QC_MODE_UNIDIR;
		break;

	case QC_SET:
		/*
		 * sanity check parameters passed in by user
		 * we're extra paranoid right now because the API
		 * is in flux
		 */
		if (info->qc_xsize        > QC_MAX_XSIZE	||
		    info->qc_ysize        > QC_MAX_YSIZE	||
		    info->qc_xorigin      > QC_MAX_XSIZE	||
		    info->qc_yorigin      > QC_MAX_YSIZE	||
		    (info->qc_bpp != 4 && info->qc_bpp != 6)	||
		    info->qc_zoom         > QC_ZOOM_200		||
		    info->qc_brightness   > UCHAR_MAX		||
		    info->qc_whitebalance > UCHAR_MAX		||
		    info->qc_contrast     > UCHAR_MAX)
			return EINVAL;

		if (info->qc_mode & ~QC_MODE_BIDIR)
			return EINVAL;

		/* version check */
		if (info->qc_version != QC_IOCTL_VERSION)
			return EINVAL;

		qs->x_size	  = info->qc_xsize;
		qs->y_size	  = info->qc_ysize;
		qs->x_origin	  = info->qc_xorigin;
		qs->y_origin	  = info->qc_yorigin;
		qs->bpp		  = info->qc_bpp;
		qs->zoom	  = info->qc_zoom;
		qs->brightness	  = info->qc_brightness;
		qs->whitebalance  = info->qc_whitebalance;
		qs->contrast	  = info->qc_contrast;

		if (info->qc_mode & QC_MODE_BIDIR)
			qs->flags |= QC_BIDIR_REQ;
		else
			qs->flags &= ~QC_BIDIR_REQ;

		/* request initialization before next scan pass */
		qs->init_req = 1;
		break;

	default:
		return ENOTTY;
	}

	return 0;
}

/*
 *-------------------------------------------------------------------
 */

#ifdef	ACTUALLY_LKM_NOT_KERNEL
/*
 * Loadable QuickCam driver stubs
 * XXX This isn't quite working yet, but the template work is done.
 * XXX do not attempt to use this driver as a LKM (yet)
 */
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

/*
 * Construct lkm_dev structures (see lkm.h)
 */

MOD_DEV(qcam, LM_DT_CHAR, CDEV_MAJOR, &qcam_cdevsw);

static int
qcam_load (struct lkm_table *lkmtp, int cmd)
{
	/* we need to call attach here with sane parameters */
	return 0;		/* nothing need be done */
}

static int
qcam_unload (struct lkm_table *lkmtp, int cmd)
{
	int i;

	for (i = 0; i < NQCAM; i++)
		if (qcam_softc[i].flags & QC_OPEN)
			return EBUSY;

	return 0;
}

int
qcam_mod (struct lkm_table *lkmtp, int cmd, int ver)
{
	int err = 0;

	if (ver != LKM_VERSION)
		return EINVAL;

	switch (cmd) {
	case LKM_E_LOAD:
		err = qcam_load(lkmtp, cmd);
		break;
	case LKM_E_UNLOAD:
		err = qcam_unload(lkmtp, cmd);
		break;
	}

	if (err)
		return err;

	/* register the cdevsw entry */
	lkmtp->private.lkm_dev = &qcam_module;
	return lkmdispatch(lkmtp, cmd);
}
#endif	/* LKM */

/*
 *-------------------------------------------------------------------
 */

/*
 * Initialize the dynamic cdevsw hooks.
 */
static void
qcam_drvinit (void *unused)
{
	static int qcam_devsw_installed = 0;
	dev_t dev;

	if (!qcam_devsw_installed) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&qcam_cdevsw, NULL);
		qcam_devsw_installed++;
    	}
}

SYSINIT(qcamdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,qcam_drvinit,NULL)

#endif /* NQCAM */
