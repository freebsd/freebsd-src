/*
 * FreeBSD Connectix QuickCam parallel-port camera video capture driver.
 * Copyright (c) 1996, Paul Traina.
 *
 * This driver is based in part on the Linux QuickCam driver which is
 * Copyright (c) 1996, Thomas Davis.
 *
 * Additional ideas from code written by Michael Chinn and Nelson Minar.
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

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(bsdi)
#define	CSRG	/* a more specific way of saying not Linux and not SysV */
#endif

#ifdef	CSRG
#include	"qcam.h"
#endif

#if NQCAM > 0

#ifdef	CSRG
#include	<sys/param.h>
#include	<machine/cpufunc.h>
#include	<machine/clock.h>
#ifdef	KERNEL
#include	<sys/systm.h>
#include	<sys/devconf.h>
#endif	/* KERNEL */
#include	<machine/qcam.h>
#endif	/* CSRG */

#ifdef	LINUX
#include	<sys/param.h>
#include	<linux/kernel.h>
#include	<linux/sched.h>
#include	<linux/string.h>
#include	<linux/delay.h>
#include	<asm/io.h>
#include	"qcam-linux.h"
#include	"../include/qcam.h"
#endif	/* LINUX */

#include	"qcamreg.h"
#include	"qcamdefs.h"

/*
 * There should be _NO_ operating system dependant code or definitions
 * past this point.
 */

static const u_char qcam_zoommode[3][3] = {
	{ QC_XFER_WIDE,   QC_XFER_WIDE,   QC_XFER_WIDE },
	{ QC_XFER_NARROW, QC_XFER_WIDE,   QC_XFER_WIDE },
	{ QC_XFER_TIGHT,  QC_XFER_NARROW, QC_XFER_WIDE }
};

static int qcam_timeouts;

#ifdef	QCAM_GRAB_STATS

#define	STATBUFSIZE (QC_MAXFRAMEBUFSIZE*2+50)
static u_short qcam_rsbhigh[STATBUFSIZE];
static u_short qcam_rsblow[STATBUFSIZE];
static u_short *qcam_rsbhigh_p   = qcam_rsbhigh;
static u_short *qcam_rsblow_p    = qcam_rsblow;
static u_short *qcam_rsbhigh_end = &qcam_rsbhigh[STATBUFSIZE];
static u_short *qcam_rsblow_end  = &qcam_rsblow[STATBUFSIZE];

#define	STATHIGH(T) \
 	if (qcam_rsbhigh_p < qcam_rsbhigh_end) \
		*qcam_rsbhigh_p++ = ((T) - timeout); \
	if (!timeout) qcam_timeouts++;

#define	STATLOW(T) \
	if (qcam_rsblow_p < qcam_rsblow_end) \
		*qcam_rsblow_p++ = ((T) - timeout); \
	if (!timeout) qcam_timeouts++;

#else

#define	STATHIGH(T)	if (!timeout) qcam_timeouts++;
#define	STATLOW(T)	if (!timeout) qcam_timeouts++;

#endif	/* QCAM_GRAB_STATS */

#define	READ_STATUS_BYTE_HIGH(P, V, T) { \
	u_short timeout = (T); \
	do { (V) = read_status((P)); \
	} while (!(((V) & 0x08)) && --timeout); STATHIGH(T) \
}

#define	READ_STATUS_BYTE_LOW(P, V, T) { \
	u_short timeout = (T); \
	do { (V) = read_status((P)); \
	} while (((V) & 0x08) && --timeout); STATLOW(T) \
}
		
#define	READ_DATA_WORD_HIGH(P, V, T) { \
	u_int timeout = (T); \
	do { (V) = read_data_word((P)); \
	} while (!((V) & 0x01) && --timeout); STATHIGH(T) \
}

#define	READ_DATA_WORD_LOW(P, V, T) { \
	u_int timeout = (T); \
	do { (V) = read_data_word((P)); \
	} while (((V) & 0x01) && --timeout); STATLOW(T) \
}

inline static int
sendbyte (u_int port, int value, int delay)
{
	u_char s1, s2;

	write_data(port, value);
	if (delay) {
	    DELAY(delay);
	    write_data(port, value);
	}

	write_control(port, QC_CTL_HIGHNIB);
	READ_STATUS_BYTE_HIGH(port, s1, QC_TIMEOUT_CMD);

	write_control(port, QC_CTL_LOWNIB);
	READ_STATUS_BYTE_LOW(port, s2, QC_TIMEOUT_CMD);

	return (s1 & 0xf0) | (s2 >> 4);
}

static int
send_command (struct qcam_softc *qs, int cmd, int value)
{
	if (sendbyte(qs->iobase, cmd, qs->exposure) != cmd)
		return 1;

	if (sendbyte(qs->iobase, value, qs->exposure) != value)
		return 1;

	return 0;			/* success */
}

static int
send_xfermode (struct qcam_softc *qs, int value)
{
	if (sendbyte(qs->iobase, QC_XFERMODE, qs->exposure) != QC_XFERMODE)
		return 1;

	if (sendbyte(qs->iobase, value, qs->exposure) != value)
		return 1;

	return 0;
}

void
qcam_reset (struct qcam_softc *qs)
{
	register u_int  iobase = qs->iobase;
	register u_char	result;

	write_control(iobase, 0x20);
	write_data   (iobase, 0x75);

	result = read_data(iobase);

	if ((result != 0x75) && !(qs->flags & QC_FORCEUNI))
	    qs->flags |= QC_BIDIR_HW;	/* bidirectional parallel port */
	else
	    qs->flags &= ~QC_BIDIR_HW;

	write_control(iobase, 0x0b);
	DELAY(250);
	write_control(iobase, QC_CTL_LOWNIB);
	DELAY(250);
}

static int
qcam_waitfor_bi (u_int port)
{
	u_char s1, s2;

	write_control(port, QC_CTL_HIGHWORD);
	READ_STATUS_BYTE_HIGH(port, s1, QC_TIMEOUT_INIT);

	write_control(port, QC_CTL_LOWWORD);
	READ_STATUS_BYTE_LOW(port, s2, QC_TIMEOUT);

	return (s1 & 0xf0) | (s2 >> 4);
}

/*
 * The pixels are read in 16 bits at a time, and we get 3 valid pixels per
 * 16-bit read.  The encoding format looks like this:
 *
 * |---- status reg -----| |----- data reg ------|
 * 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *  3  3  3  3  2  x  x  x  2  2  2  1  1  1  1  R
 *
 *  1 = left pixel	R = camera ready
 *  2 = middle pixel	x = unknown/unused?
 *  3 = right pixel
 *
 * XXX do not use this routine yet!  It does not work.
 *     Nelson believes that even though 6 pixels are read in per 2 words,
 *     only the 1 & 2 pixels from the first word are correct.  This seems
 *     bizzare, more study is needed here.
 */

#define	DECODE_WORD_BI4BPP(P, W) \
	*(P)++ = 16 - (((W) >> 12) & 0x0f); \
	*(P)++ = 16 - ((((W) >> 8) & 0x08) | (((W) >> 5) & 0x07)); \
	*(P)++ = 16 - (((W) >>  1) & 0x0f);

static void
qcam_bi_4bit (struct qcam_softc *qs)
{
	u_char *p;
	u_int   port;
	u_short word;

	port = qs->iobase;			/* for speed */

	qcam_waitfor_bi(port);

	/*
	 * Unlike the other routines, this routine has NOT be interleaved
	 * yet because we don't have the algorythm for 4bbp down tight yet,
	 * so why add to the confusion?
	 */
	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, QC_CTL_HIGHWORD);
		READ_DATA_WORD_HIGH(port, word, QC_TIMEOUT);
		DECODE_WORD_BI4BPP(p, word);

		write_control(port, QC_CTL_LOWWORD);
		READ_DATA_WORD_HIGH(port, word, QC_TIMEOUT);
		DECODE_WORD_BI4BPP(p, word);
	}
}

/*
 * The pixels are read in 16 bits at a time, 12 of those bits contain
 * pixel information, the format looks like this:
 *
 * |---- status reg -----| |----- data reg ------|
 * 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *  2  2  2  2  2  x  x  x  2  1  1  1  1  1  1  R
 *
 * 1 = left pixel		R = camera ready
 * 2 = right pixel		x = unknown/unused?
 */

#define	DECODE_WORD_BI6BPP(P, W) \
	*(P)++ = 63 -  (((W) >>  1) & 0x3f); \
	*(P)++ = 63 - ((((W) >> 10) & 0x3e) | (((W) >> 7) & 0x01));

static void
qcam_bi_6bit (struct qcam_softc *qs)
{
	u_char *p;
	u_short hi, low;
	u_int   port;

	port = qs->iobase;			/* for speed */

	qcam_waitfor_bi(port);

	/*
	 * This was interleaved before, but I cut it back to the simple
	 * mode so that it's easier for people to play with it.  A quick
	 * unrolling of the loop coupled with interleaved decoding and I/O
	 * should get us a slight CPU bonus later.
	 */
	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, QC_CTL_HIGHWORD);
		READ_DATA_WORD_HIGH(port, hi, QC_TIMEOUT);
		DECODE_WORD_BI6BPP(p, hi);

		write_control(port, QC_CTL_LOWWORD);
		READ_DATA_WORD_LOW(port, low, QC_TIMEOUT);
		DECODE_WORD_BI6BPP(p, low);
	}
}

/*
 * We're doing something tricky here that makes this routine a little
 * more complex than you would expect.  We're interleaving the high
 * and low nibble reads with the math required for nibble munging.
 * This should allow us to use the "free" time while we're waiting for
 * the next nibble to come ready to do any data conversion operations.
 */
#define	DECODE_WORD_UNI4BPP(P, W) \
	*(P)++ = 16 - ((W) >> 4);

static void
qcam_uni_4bit (struct qcam_softc *qs)
{
	u_char	*p, *end, hi, low;
	u_int	port;

	port = qs->iobase;
	p    = qs->buffer;
	end  = qs->buffer_end - 1;

	/* request and wait for first nibble */

	write_control(port, QC_CTL_HIGHNIB);
	READ_STATUS_BYTE_HIGH(port, hi, QC_TIMEOUT_INIT);

	/* request second nibble, munge first nibble while waiting, read 2nd */

	write_control(port, QC_CTL_LOWNIB);
	DECODE_WORD_UNI4BPP(p, hi);
	READ_STATUS_BYTE_LOW(port, low, QC_TIMEOUT);

	while (p < end) {
		write_control(port, QC_CTL_HIGHNIB);
		DECODE_WORD_UNI4BPP(p, low);
		READ_STATUS_BYTE_HIGH(port, hi, QC_TIMEOUT);

		write_control(port, QC_CTL_LOWNIB);
		DECODE_WORD_UNI4BPP(p, hi);
		READ_STATUS_BYTE_LOW(port, low, QC_TIMEOUT);
	}
	DECODE_WORD_UNI4BPP(p, low);
}

/*
 * If you treat each pair of nibble operations as pulling in a byte, you
 * end up with a byte stream that looks like this:
 *
 *    msb	      lsb
 *    	2 2 1 1 1 1 1 1
 *	2 2 2 2 3 3 3 3
 *	3 3 4 4 4 4 4 4
 */

static void
qcam_uni_6bit (struct qcam_softc *qs)
{
	u_char	*p;
	u_int   port;
	u_char	word1, word2, word3, hi, low;

	port = qs->iobase;

	/*
	 * This routine has been partially interleaved... we can do a better
	 * job, but for right now, I've deliberately kept it less efficient
	 * so we can play with decoding without hurting peoples brains.
	 */
	for (p = qs->buffer; p < qs->buffer_end; ) {
		write_control(port, QC_CTL_HIGHNIB);
		READ_STATUS_BYTE_HIGH(port, hi, QC_TIMEOUT_INIT);
		write_control(port, QC_CTL_LOWNIB);
		READ_STATUS_BYTE_LOW(port, low, QC_TIMEOUT);
		write_control(port, QC_CTL_HIGHNIB);
		word1 = (hi & 0xf0) | (low >>4);
		READ_STATUS_BYTE_HIGH(port, hi, QC_TIMEOUT);
		write_control(port, QC_CTL_LOWNIB);
		*p++ = 63 - (word1 >> 2);
		READ_STATUS_BYTE_LOW(port, low, QC_TIMEOUT);
		write_control(port, QC_CTL_HIGHNIB);
		word2 = (hi & 0xf0) | (low >> 4);
		READ_STATUS_BYTE_HIGH(port, hi, QC_TIMEOUT);
		write_control(port, QC_CTL_LOWNIB);
		*p++ = 63 - (((word1 & 0x03) << 4) | (word2 >> 4));
		READ_STATUS_BYTE_LOW(port, low, QC_TIMEOUT);
		word3 = (hi & 0xf0) | (low >> 4);
		*p++ = 63 - (((word2 & 0x0f) << 2) | (word3 >> 6));
		*p++ = 63 - (word3 & 0x3f);
	}

	/* XXX this is something xfqcam does, doesn't make sense to me,
	   but we don't see timeoutes here... ? */
	write_control(port, QC_CTL_LOWNIB);
	READ_STATUS_BYTE_LOW(port, word1, QC_TIMEOUT);
	write_control(port, QC_CTL_HIGHNIB);
	READ_STATUS_BYTE_LOW(port, word1, QC_TIMEOUT);
}

static void
qcam_xferparms (struct qcam_softc *qs)
{
	int bidir;

	qs->xferparms = 0;

	bidir = (qs->flags & QC_BIDIR_HW);
	if (bidir)
		qs->xferparms |= QC_XFER_BIDIR;

	if (qcam_debug)
		printf("qcam%d: %dbpp %sdirectional scan mode selected\n",
			qs->unit, qs->bpp, bidir ? "bi" : "uni");

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
}

static void
qcam_init (struct qcam_softc *qs)
{
	int x_size = (qs->bpp == 4) ? qs->x_size / 2 : qs->x_size / 4;

	qcam_xferparms(qs);

	send_command(qs, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs, QC_BRIGHTNESS,   1);
	send_command(qs, QC_BRIGHTNESS,   1);
	send_command(qs, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs, QC_BRIGHTNESS,   qs->brightness);
	send_command(qs, QC_YSIZE,	  qs->y_size);
	send_command(qs, QC_XSIZE,	  x_size);
	send_command(qs, QC_YORG,	  qs->y_origin);
	send_command(qs, QC_XORG,	  qs->x_origin);
	send_command(qs, QC_CONTRAST,	  qs->contrast);
	send_command(qs, QC_WHITEBALANCE, qs->whitebalance);

	if (qs->buffer)
	    qs->buffer_end = qs->buffer +
			     min((qs->x_size*qs->y_size), QC_MAXFRAMEBUFSIZE);

	qs->init_req = 0;
}

int
qcam_scan (struct qcam_softc *qs)
{
	int timeouts;

#ifdef	QCAM_GRAB_STATS
	bzero(qcam_rsbhigh, sizeof(qcam_rsbhigh));
	bzero(qcam_rsblow,  sizeof(qcam_rsblow));
	qcam_rsbhigh_p = qcam_rsbhigh;
	qcam_rsblow_p  = qcam_rsblow;
#endif

	timeouts = qcam_timeouts;

	if (qs->init_req)
		qcam_init(qs);

	if (send_xfermode(qs, qs->xferparms))
		return 1;

	if (qcam_debug && (timeouts != qcam_timeouts))
		printf("qcam%d: %d timeouts during init\n", qs->unit,
			qcam_timeouts - timeouts);

	timeouts = qcam_timeouts;

	if (qs->scanner)
		(*qs->scanner)(qs);
	else
		return 1;

	if (qcam_debug && (timeouts != qcam_timeouts))
		printf("qcam%d: %d timeouts during scan\n", qs->unit,
			qcam_timeouts - timeouts);

	write_control(qs->iobase, 0x0f);

	return 0;			/* success */
}

void
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
	qs->exposure	 = QC_DEF_EXPOSURE;
}

#ifndef	QCAM_INVASIVE_SCAN
/*
 * Attempt a non-destructive probe for the QuickCam.
 * Current models appear to toggle the upper 4 bits of
 * the status register at approximately 5-10 Hz.
 *
 * Be aware that this isn't the way that Connectix detects the
 * camera (they send a reset and try to handshake),  but this
 * way is safe.
 */
int
qcam_detect (u_int port) {
	int i, transitions = 0;
	u_char reg, last;

	write_control(port, 0x20);
	write_control(port, 0x0b);
	write_control(port, 0x0e);

	last = reg = read_status(port);

	for (i = 0; i < QC_PROBELIMIT; i++) {
	    reg = read_status(port) & 0xf0;

	    if (reg != last)	/* if we got a toggle, count it */
		transitions++;

	    last = reg;
	    DELAY(100000);	/* 100ms */
	}

	return transitions >= QC_PROBECNTLOW &&
	       transitions <= QC_PROBECNTHI;
}
#else
/*
 * This form of probing for the camera can cause garbage to show
 * up on your printers if they're plugged in instead.  However,
 * some folks have a problem with the nondestructive scan when
 * using EPP/ECP parallel ports.
 *
 * Try to send down a brightness command, if we succeed, we've
 * got a camera on the remote side.
 */
int
qcam_detect (u_int port) {
	write_control(port, 0x20);
	write_data(port, 0x75);
	read_data(port);
	write_control(port, 0x0b);
	DELAY(250);
	write_control(port, 0x0e);
	DELAY(250);

	if (sendbyte(port, QC_BRIGHTNESS, QC_DEF_EXPOSURE) != QC_BRIGHTNESS)
		return 0;	/* failure */
	return (sendbyte(port, 1, QC_DEF_EXPOSURE) == 1);
}
#endif

#endif /* NQCAM */
